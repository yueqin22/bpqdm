/* Shared stand-ins for the f_canlink differential test.
 *
 * f_canlink.c reaches outside itself in two ways:
 *
 *   * eight data objects it reads and writes (funcCode, p2pData, sciFlag,
 *     commRcvData, commReadData, curTime, errorOther, errorInfo).  All eight are
 *     declared extern in project headers and defined in other .c files.  Those
 *     files cannot be linked here -- f_comm.c is the SCIA register programmer,
 *     f_p2p.c is itself a rewritten module -- and none of the eight is needed:
 *     they are plain data, so a definition here is what the target starts with
 *     (the target's .bss is zeroed by the boot code).  funcCode is the one with
 *     a wrinkle: f_funcCode.c also defines funcCodeGradeSum[]/funcCodeGradeAll[],
 *     whose static initialisers are differences of two address constants, which
 *     MSVC rejects (C2099).  f_canlink reads only scalar fields of
 *     funcCode.code, so owning the union here and letting the driver supply the
 *     fields as input is both simpler and sufficient.
 *
 *   * ten functions it calls: five CAN-driver primitives (InitdspECan,
 *     InitTranMbox, InitRecMbox, eCanDataTran, eCanDataRec) and five project
 *     functions (CommWrite, CommRead, PlcDataFramDeal, InvTranTabCfg,
 *     InvRecTabCfg).  None has an effect the driver could see except through
 *     its return value or through the caller's buffer, so all ten are stubs that
 *     log their arguments and return a driver-set value.
 *
 * Every stub is called with the same arguments by both sides, so the trace is a
 * faithful, order-sensitive record of what the module asked the hardware to do.
 * See mock_canlink.h for the trace shape and why a hash is needed.
 *
 * ---- what the stubs deliberately do NOT model ----
 *
 * The real InitdspECan waits for the CAN module to leave initialisation and can
 * return CAN_INIT_TIME / CAN_INIT_TIMEOUT / CAN_INIT_BAUD_ERR.  Here the answer
 * is whatever the driver sets, which is the point: the module branches on it, so
 * every arm is reachable without a CAN peripheral.
 *
 * eCanDataRec fills a mailbox's data into the caller's buffer.  The stub writes
 * one word -- the frame's message id -- so that the four receive mailboxes can
 * carry four frames with four different `framf` fields, which is what
 * CanLlinkDataDeal switches on.  The rest of the buffer is the driver's
 * installed template; the module never reads it in CanlinkDataRec, and the
 * downstream CanLlinkDataDeal only reads msgid.framf and the mdl fields, which
 * the driver controls.
 */

#include "f_comm.h"
#include "f_dspcan.h"
#include "f_canlink.h"
#include "f_plc.h"
#include "f_p2p.h"
#include "f_funcCode.h"
#include "f_error.h"

#include "mock_canlink.h"

/* ---------------------------------------------------------------------------
 * The eight data objects
 * ------------------------------------------------------------------------ */

COMM_RCV_DATA   commRcvData;
Uint16          commReadData[RTU_READ_DATA_NUM_MAX];
union SCI_FLAG  sciFlag;
FUNCCODE_ALL    funcCode;
P2P_DATA_STRUCT p2pData;
struct CUR_TIME curTime;
Uint16          errorOther;
Uint16          errorInfo;

/* ---------------------------------------------------------------------------
 * Trace and knobs
 * ------------------------------------------------------------------------ */

struct CANT_ENT cantTrace[CANT_N];
Uint32          cantTraceN;
Uint32          cantTraceHash;

Uint16 mockInitECanRc;
Uint16 mockTranSuccOn;
Uint16 mockRecOverOn;
Uint16 mockRecSuccFrom;
Uint16 mockCommWriteRc;
Uint16 mockCommReadRc;
Uint16 mockInvTranRc;
Uint16 mockInvRecRc;
Uint32 mockRecWord[4];

/* Per-kind call counters, so "succeed on the Nth eCanDataTran call" means the
 * Nth call of *that* function, not the Nth stub call overall. */
static Uint32 s_tranN;
static Uint32 s_recN;

#define FNV_OFFSET 2166136261UL
#define FNV_PRIME  16777619UL

static void log_call(Uint16 which, Uint32 a0, Uint32 a1, Uint32 a2, Uint32 a3)
{
    if (cantTraceN < CANT_N)
    {
        struct CANT_ENT *e = &cantTrace[cantTraceN];
        e->which = which;
        e->a0 = a0;
        e->a1 = a1;
        e->a2 = a2;
        e->a3 = a3;
    }

    cantTraceHash ^= (Uint32)which;  cantTraceHash *= FNV_PRIME;
    cantTraceHash ^= a0;             cantTraceHash *= FNV_PRIME;
    cantTraceHash ^= a1;             cantTraceHash *= FNV_PRIME;
    cantTraceHash ^= a2;             cantTraceHash *= FNV_PRIME;
    cantTraceHash ^= a3;             cantTraceHash *= FNV_PRIME;

    cantTraceN++;
}

void mockReset(void)
{
    Uint32 i;

    cantTraceN    = 0;
    cantTraceHash = FNV_OFFSET;
    s_tranN       = 0;
    s_recN        = 0;

    for (i = 0; i < CANT_N; i++)
    {
        cantTrace[i].which = 0;
        cantTrace[i].a0 = cantTrace[i].a1 = 0;
        cantTrace[i].a2 = cantTrace[i].a3 = 0;
    }
}

/* ---------------------------------------------------------------------------
 * The ten stubs
 * ------------------------------------------------------------------------ */

Uint16 InitdspECan(Uint16 baud)
{
    log_call(CANT_INITECAN, baud, 0, 0, 0);
    return mockInitECanRc;
}

void InitTranMbox(Uint16 mbox, Uint32 msgid, Uint32 *dataPi)
{
    log_call(CANT_INITTRAN, mbox, msgid,
             dataPi ? dataPi[0] : 0, dataPi ? dataPi[1] : 0);
}

void InitRecMbox(Uint16 mbox, Uint32 msgid, Uint32 lam)
{
    log_call(CANT_INITREC, mbox, msgid, lam, 0);
}

Uint16 eCanDataTran(Uint16 mbox, Uint16 len, Uint32 msgid, Uint32 *dataPi)
{
    Uint16 rc;

    s_tranN++;

    if (mockTranSuccOn != 0 && s_tranN == mockTranSuccOn)
        rc = CAN_MBOX_TRAN_SUCC;
    else
        rc = CAN_MBOX_BUSY;

    log_call(CANT_ECTRAN, mbox, len, msgid, dataPi ? dataPi[0] : 0);
    return rc;
}

Uint16 eCanDataRec(Uint16 mbox, Uint32 *dataPi)
{
    Uint16 rc;
    Uint32 wrote = 0;

    s_recN++;

    if (mockRecOverOn != 0 && s_recN == mockRecOverOn)
        rc = CAN_MBOX_REC_OVER;
    else if (mockRecSuccFrom != 0 && s_recN >= mockRecSuccFrom)
        rc = CAN_MBOX_REC_SUCC;
    else
        rc = CAN_MBOX_EMPTY;

    if (rc != CAN_MBOX_EMPTY && dataPi)
    {
        wrote = mockRecWord[(s_recN - 1) & 3];
        *dataPi = wrote;
    }

    log_call(CANT_ECREC, mbox, rc, wrote, 0);
    return rc;
}

/* CommWrite and CommRead log two extra words: the values of the latch globals
 * the *callee* reads.
 *
 * CanControlWriter's `eeprom` argument does not survive the call -- the flag it
 * sets is overwritten with NO_EEPROM on the very next line -- so the argument's
 * only outward effect is what CommWrite sees when it is entered. And that is
 * not a technicality: in the real f_comm.c, CommWrite -> CommRwFuncCode reads
 * commRcvData.commCmdSaveEeprom to decide whether to persist the function code
 * being written (f_comm.c:960, `SaveOneFuncCode`), and the write path reads
 * sciFlag (f_comm.c:375). So "what the callee saw" is a genuine side effect of
 * the module under test, not an artefact of the harness.
 *
 * Without these two words, CanControlWriter(addr, data, 0) and
 * CanControlWriter(addr, data, 1) are indistinguishable from outside -- which
 * is exactly the eeprom/not-eeprom distinction the function exists to make. */
Uint16 CommWrite(Uint16 addr, Uint16 data)
{
    log_call(CANT_COMMWRITE, addr, data,
             commRcvData.commCmdSaveEeprom, sciFlag.all);
    return mockCommWriteRc;
}

Uint16 CommRead(Uint16 addr, Uint16 data)
{
    log_call(CANT_COMMREAD, addr, data,
             commRcvData.commCmdSaveEeprom, sciFlag.all);
    return mockCommReadRc;
}

void PlcDataFramDeal(struct CANLINK_DATA_BUF *dataPi)
{
    log_call(CANT_PLCDATA, dataPi->msgid.all, dataPi->mdl.all, 0, 0);
}

Uint16 InvTranTabCfg(struct CANLINK_DATA_BUF *dataPi)
{
    log_call(CANT_INVTTRAN, dataPi->msgid.all, dataPi->mdl.all, 0, 0);
    return mockInvTranRc;
}

Uint16 InvRecTabCfg(struct CANLINK_DATA_BUF *dataPi)
{
    log_call(CANT_INVREC, dataPi->msgid.all, dataPi->mdl.all, 0, 0);
    return mockInvRecRc;
}
