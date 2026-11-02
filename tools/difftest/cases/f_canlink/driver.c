/* Differential test driver for f_canlink.
 *
 * The module is the CAN_LINK / PLC-card CAN driver: it owns the eCAN mailbox
 * set-up and the CAN_LINK frame protocol (read/write register, read info,
 * remote-frame answer) plus the point-to-point master/slave path that shares the
 * same mailboxes.
 *
 * Public entry points, in the order the system uses them:
 *
 *   CanlinkFun()            the 2 ms task. Decides the baud/id from the function
 *                           code table (PLC card / point-to-point / plain
 *                           CAN_LINK), (re)initialises the controller when that
 *                           changes, then either transmits as the point-to-point
 *                           master or drains the receive mailboxes and dispatches
 *                           each frame. On a receive timeout it bumps the
 *                           timeout counter and, for the PLC card, raises
 *                           ERROR_COMM / COMM_ERROR_PLC.
 *   InitCanlink()           one-shot: InitdspECan, then the transmit and receive
 *                           mailbox set-up. Returns the controller status.
 *   InitCanlinkTran()       configures the transmit mailboxes.
 *   InitCanlinkRec()        configures the receive mailboxes (and the last one
 *                           with the overwrite bit).
 *   CanlinkDataTran()       tries each transmit mailbox in turn; returns SUCC,
 *                           or BUSY/TIMEOUT after `timeOut` consecutive rounds.
 *   CanlinkDataRec()        drains the receive mailboxes into CanlinkRecBuf and
 *                           returns EMPTY / SUCC / OVER.
 *   CanLlinkDataDeal()      switches on the frame type: remote -> CanLinkRemoteDeal,
 *                           control -> CanLinkConDeal, data -> PlcDataFramDeal,
 *                           anything else bumps the error counter.
 *   CanLinkRemoteDeal()     answers a remote frame from the product identity.
 *   CanLinkConDeal()        the command frame: reads/writes a register through
 *                           CommRead/CommWrite, or configures the PLC tables,
 *                           then answers the requester.
 *   CanControlWriter()      CommWrite wrapper that sets the EEPROM-save flag.
 *   CanControlRead()        CommRead wrapper that returns the first read word.
 *   CanHostTx()             point-to-point master transmit.
 *
 * Nothing here touches a peripheral register. The whole hardware boundary is the
 * ten functions supplied by the shared mock -- see mock_canlink.c.
 *
 * ---- what is compared ----
 *
 * After every call the driver compares, field by field:
 *
 *   * the three module-owned globals: CanRxTxCon, CanlinkRecBuf (whole struct)
 *     and CanLinkChara[2];
 *   * the eight globals the mock owns and the module reads/writes: the whole
 *     funcCode union, p2pData, sciFlag, commRcvData, commReadData[], curTime,
 *     errorOther and errorInfo;
 *   * the caller's scratch CANLINK_DATA_BUF -- the buffer CanLinkConDeal,
 *     CanLinkRemoteDeal, CanLlinkDataDeal and CanlinkDataTran write into;
 *   * the out-parameter of CanControlRead;
 *   * the mock call trace: count, order-sensitive hash, and the first CANT_N
 *     entries (which, four args);
 *   * the return value.
 *
 * The two Comm* trace entries carry four words rather than two, because the
 * latch globals the callee reads are the only outward effect of
 * CanControlWriter's `eeprom` argument. See mock_canlink.c.
 *
 * ---- state the module carries, and how the driver reaches it ----
 *
 * The eight shared globals are per-side only in the sense that the driver
 * installs them before each side runs and captures them after: they are one
 * object, written by whichever side is running. That is deliberate -- a rewrite
 * that stops updating one of them shows up immediately as a captured difference.
 *
 * The function-local statics are not reachable *directly*: cl2000 mangles
 * file-local statics, so they are local symbols. They are not a hole in the
 * test. Both sides run the *same* call sequence from the same start state, so
 * their statics evolve identically as long as the two implementations are
 * equivalent. What the driver must never do is give the two sides different call
 * sequences -- run_case() is written so that it cannot. The price is that the
 * driver cannot *reset* those statics, so the reachability of the "parameters
 * changed -> re-init" arm of CanlinkFun and of the transmit-timeout arm of
 * CanlinkDataTran depends on what ran before. phase_fun() and phase_seq() are
 * ordered with that in mind and say so where it matters. sBaud / sSourID start
 * at 0, which is neither CAN_BAUD_1M nor any of the three configured ids, so the
 * very first CanlinkFun() always takes the re-init arm.
 *
 * Two of those statics *are* read back out, deliberately:
 *
 *   * CanlinkDataTran's `count` -- probe_tran_residue() issues an all-fail call
 *     with a small timeOut after every phase that can leave a residue, so the
 *     counter's value becomes an ordinary compared return value. Without it the
 *     three mutants that only perturb `count` pass the whole suite.
 *   * CanlinkFun's `timeOutErr` / `initFlag` -- the receive-timeout arm needs
 *     more than four ticks and the re-init arm needs the baud/id to change, so
 *     both are reached from phase_seq()'s consecutive calls rather than from a
 *     single call.
 *
 * ---- pre-existing hazards that are NOT tested ----
 *
 * Listed rather than quietly avoided. None is a defect the rewrite may "fix":
 * changing any of them would be a behaviour change, not a rewrite.
 *
 *   a. InitCanlinkRec() runs `for (i=0; i<REC_MBOX_NUM-1; i++) InitRecMbox(...)`
 *      and then `InitRecMbox((REC_BOX_N-i), ...)` using the loop's exit value of
 *      i. That is intended (the last mailbox is configured with the overwrite
 *      bit), and the driver exercises both the loop and the tail call.
 *   b. CanlinkDataRec() tests `stat` after the loop, i.e. the status of the
 *      *last* mailbox only. So a mailbox 0..2 that reported REC_OVER does not
 *      make the function return OVER; only mailbox 3 can. Preserved as-is; the
 *      driver sweeps mockRecOverOn over all four positions so the asymmetry is
 *      pinned down rather than assumed.
 *   c. CanLlinkDataDeal()'s switch has no `break` after `case CAN_DATA_FRAME:`,
 *      but the following statement is the loop's `break`, and the arm itself
 *      ends with `break`, so it is not a fall-through. Preserved.
 *   d. CanLinkConDeal() reads `err` in the final `if (err)` after the switch. The
 *      `CAN_LINK_R_CFG`, `CAN_LINK_R_INFO` and `CAN_LINK_R_WAR` arms assign
 *      nothing to `err`, so those three commands branch on an indeterminate
 *      value. That is a real defect, and it is *not testable*: the two sides are
 *      different functions, so their stack frames are not guaranteed to hold the
 *      same leftover. It was measured, not assumed -- with those three command
 *      codes swept, the fixed phases happened to agree (the call is made straight
 *      from invoke(), so both frames inherit the same leftovers) while the nested
 *      calls in the random phase disagreed on the first one. The driver therefore
 *      sweeps `code` over the values the protocol actually uses (read/write reg,
 *      write eeprom, the two table-config commands, and one invalid code for the
 *      `default:` arm) and leaves the three empty arms alone. A rewrite must
 *      preserve this shape, but no host-side test can pin it down.
 *
 *      CanLinkConDeal is reachable from three places, so the exclusion has to be
 *      applied to all three, not just the direct call:
 *
 *        * ENTRY_CON           -- called straight from invoke();
 *        * ENTRY_DATADEAL      -- via CanLlinkDataDeal, when the frame is a
 *                                 control frame;
 *        * ENTRY_FUN / _SEQ    -- via CanlinkFun -> CanlinkDataRec ->
 *                                 CanLlinkDataDeal, for any received control
 *                                 frame.
 *
 *      The third is why the random phase builds the four receive frames' `code`
 *      fields from V_CODE_ASSIGNED rather than from the raw random word. Without
 *      that, roughly one run in ten of the full 20000-case suite would compare a
 *      value that neither implementation defines, and the mismatch would look
 *      like a rewrite defect.
 *   e. CanLinkRemoteDeal() and CanLinkConDeal() test `aq == CANlink_ACK` (0) and
 *      return early. A frame that arrives with aq clear is therefore dropped.
 *      Preserved and exercised.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Same include order as src/function/f_canlink.c. */
#include "f_funcCode.h"
#include "f_dspcan.h"
#include "f_canlink.h"
#include "f_comm.h"
#include "f_plc.h"
#include "f_p2p.h"
#include "f_error.h"

#include "mock_canlink.h"
#include "canlink_sides.h"

/* ---------------------------------------------------------------------------
 * The two sides
 * ------------------------------------------------------------------------ */

#define PS(n)   (side ? &new_##n : &ref_##n)
#define PARR(n) (side ? new_##n : ref_##n)

/* The module-owned globals are per-side (renamed); the eight shared globals are
 * one object written by whichever side is running. */
struct ST
{
    Uint16                  canRxTxCon;
    struct CANLINK_REC_BUF  recBuf;
    Uint32                  chara[2];
    FUNCCODE_ALL            fc;
    P2P_DATA_STRUCT         p2p;
    union SCI_FLAG          sci;
    COMM_RCV_DATA           rcv;
    Uint16                  rd[RTU_READ_DATA_NUM_MAX];
    struct CUR_TIME         ct;
    Uint16                  errorOther;
    Uint16                  errorInfo;
    struct CANLINK_DATA_BUF scratch;
    Uint16                  readOut;
};

/* The caller's buffer for the entries that take one, and the out-parameter of
 * CanControlRead.  Both are driver-side, so they are captured separately. */
static struct CANLINK_DATA_BUF g_scratch;
static Uint16                  g_readOut;

static void capture(int side, struct ST *s)
{
    s->canRxTxCon = *PS(CanRxTxCon);
    s->recBuf     = *PS(CanlinkRecBuf);
    s->chara[0]   = PARR(CanLinkChara)[0];
    s->chara[1]   = PARR(CanLinkChara)[1];
    s->fc         = funcCode;
    s->p2p        = p2pData;
    s->sci        = sciFlag;
    s->rcv        = commRcvData;
    memcpy(s->rd, commReadData, sizeof(s->rd));
    s->ct         = curTime;
    s->errorOther = errorOther;
    s->errorInfo  = errorInfo;
    s->scratch    = g_scratch;
    s->readOut    = g_readOut;
}

static void install(int side, const struct ST *s)
{
    *PS(CanRxTxCon)     = s->canRxTxCon;
    *PS(CanlinkRecBuf)  = s->recBuf;
    PARR(CanLinkChara)[0] = s->chara[0];
    PARR(CanLinkChara)[1] = s->chara[1];
    funcCode    = s->fc;
    p2pData     = s->p2p;
    sciFlag     = s->sci;
    commRcvData = s->rcv;
    memcpy(commReadData, s->rd, sizeof(commReadData));
    curTime     = s->ct;
    errorOther  = s->errorOther;
    errorInfo   = s->errorInfo;
    g_scratch   = s->scratch;
    g_readOut   = s->readOut;
}

/* ---------------------------------------------------------------------------
 * Inputs
 * ------------------------------------------------------------------------ */

struct IN
{
    struct ST st;

    /* mock return-value knobs */
    Uint16 mockInitECanRc;
    Uint16 mockTranSuccOn;
    Uint16 mockRecOverOn;
    Uint16 mockRecSuccFrom;
    Uint16 mockCommWriteRc;
    Uint16 mockCommReadRc;
    Uint16 mockInvTranRc;
    Uint16 mockInvRecRc;
    Uint32 mockRecWord[4];

    /* entry arguments */
    Uint16 arg0;
    Uint16 arg1;
    Uint16 arg2;
};

static void apply_inputs(const struct IN *in)
{
    mockInitECanRc  = in->mockInitECanRc;
    mockTranSuccOn  = in->mockTranSuccOn;
    mockRecOverOn   = in->mockRecOverOn;
    mockRecSuccFrom = in->mockRecSuccFrom;
    mockCommWriteRc = in->mockCommWriteRc;
    mockCommReadRc  = in->mockCommReadRc;
    mockInvTranRc   = in->mockInvTranRc;
    mockInvRecRc    = in->mockInvRecRc;
    memcpy(mockRecWord, in->mockRecWord, sizeof(mockRecWord));
}

/* Build a CAN_LINK message id through the real bitfields, so the driver can
 * never disagree with the module about the layout. */
static Uint32 mkid(Uint16 src, Uint16 dst, Uint16 code, Uint16 aq, Uint16 framf)
{
    union CANLINK_MsgID m;

    m.all = 0;
    m.bit.srcSta  = src;
    m.bit.destSta = dst;
    m.bit.code    = code;
    m.bit.aq      = aq;
    m.bit.framf   = framf;
    return m.all;
}

/* ---------------------------------------------------------------------------
 * Invocation and comparison
 * ------------------------------------------------------------------------ */

static long g_cases;
static int  g_fails;
static int  g_trace_on;
static const char *g_phase = "";

static void report(const char *field, const char *detail)
{
    if (g_fails < 40)
    {
        printf("MISMATCH %s: %s %s\n", g_phase, field, detail);
    }
    g_fails++;
}

#define ENTRY_INITTRAN  0   /* InitCanlinkTran(addr, chara) */
#define ENTRY_TRAN      1   /* CanlinkDataTran(&scratch, len, timeOut) -> Uint16 */
#define ENTRY_INITREC   2   /* InitCanlinkRec(addr) */
#define ENTRY_REC       3   /* CanlinkDataRec(&CanlinkRecBuf) -> Uint16 */
#define ENTRY_INIT      4   /* InitCanlink(addr, baud, chara) -> Uint16 */
#define ENTRY_FUN       5   /* CanlinkFun() */
#define ENTRY_DATADEAL  6   /* CanLlinkDataDeal(&scratch) */
#define ENTRY_REMOTE    7   /* CanLinkRemoteDeal(&scratch) */
#define ENTRY_WRITER    8   /* CanControlWriter(addr, data, eeprom) -> Uint16 */
#define ENTRY_READER    9   /* CanControlRead(addr, &out) -> Uint16 */
#define ENTRY_CON      10   /* CanLinkConDeal(&scratch) */
#define ENTRY_HOST     11   /* CanHostTx(addr, data) */
#define ENTRY_FUN_SEQ  12   /* `a` consecutive CanlinkFun() calls */

static int32 invoke(int side, int entry, int a, const struct IN *in)
{
    Uint32 *chara = side ? new_CanLinkChara : ref_CanLinkChara;
    struct CANLINK_REC_BUF *rb = side ? &new_CanlinkRecBuf : &ref_CanlinkRecBuf;
    int k;
    int32 r = 0;

    g_readOut = 0;

    switch (entry)
    {
        case ENTRY_INITTRAN:
            if (side) new_InitCanlinkTran(in->arg0, chara);
            else      ref_InitCanlinkTran(in->arg0, chara);
            break;

        case ENTRY_TRAN:
            r = side ? (int32)new_CanlinkDataTran((Uint32 *)&g_scratch, in->arg0, in->arg1)
                     : (int32)ref_CanlinkDataTran((Uint32 *)&g_scratch, in->arg0, in->arg1);
            break;

        case ENTRY_INITREC:
            if (side) new_InitCanlinkRec(in->arg0);
            else      ref_InitCanlinkRec(in->arg0);
            break;

        case ENTRY_REC:
            r = side ? (int32)new_CanlinkDataRec(rb)
                     : (int32)ref_CanlinkDataRec(rb);
            break;

        case ENTRY_INIT:
            r = side ? (int32)new_InitCanlink(in->arg0, in->arg1, chara)
                     : (int32)ref_InitCanlink(in->arg0, in->arg1, chara);
            break;

        case ENTRY_FUN:
            if (side) new_CanlinkFun(); else ref_CanlinkFun();
            break;

        case ENTRY_FUN_SEQ:
            for (k = 0; k < a; k++)
            {
                if (side) new_CanlinkFun(); else ref_CanlinkFun();
            }
            break;

        case ENTRY_DATADEAL:
            if (side) new_CanLlinkDataDeal(&g_scratch);
            else      ref_CanLlinkDataDeal(&g_scratch);
            break;

        case ENTRY_REMOTE:
            if (side) new_CanLinkRemoteDeal(&g_scratch);
            else      ref_CanLinkRemoteDeal(&g_scratch);
            break;

        case ENTRY_WRITER:
            r = side ? (int32)new_CanControlWriter(in->arg0, in->arg1, in->arg2)
                     : (int32)ref_CanControlWriter(in->arg0, in->arg1, in->arg2);
            break;

        case ENTRY_READER:
            r = side ? (int32)new_CanControlRead(in->arg0, &g_readOut)
                     : (int32)ref_CanControlRead(in->arg0, &g_readOut);
            break;

        case ENTRY_CON:
            if (side) new_CanLinkConDeal(&g_scratch);
            else      ref_CanLinkConDeal(&g_scratch);
            break;

        case ENTRY_HOST:
            if (side) new_CanHostTx(in->arg0, in->arg1);
            else      ref_CanHostTx(in->arg0, in->arg1);
            break;

        default:
            break;
    }
    return r;
}

static void run_case(const struct IN *in, int entry, int a)
{
    struct ST st[2];
    struct CANT_ENT tr[2][CANT_N];
    Uint32 trn[2], trh[2];
    int32  ret[2];
    int side;

    g_cases++;

    if (g_trace_on)
    {
        fprintf(stderr, "case %ld entry=%d a=%d\n", g_cases, entry, a);
    }

    for (side = 0; side < 2; side++)
    {
        apply_inputs(in);
        install(side, &in->st);
        mockReset();

        ret[side] = invoke(side, entry, a, in);

        capture(side, &st[side]);
        memcpy(tr[side], cantTrace, sizeof(tr[side]));
        trn[side] = cantTraceN;
        trh[side] = cantTraceHash;
    }

    /* ---- compare ---- */
    {
#define CMPF(t, n)                                                          \
        if (st[0].n != st[1].n) {                                           \
            char buf[128];                                                  \
            sprintf(buf, "ref=%ld new=%ld", (long)st[0].n, (long)st[1].n);  \
            report(#n, buf);                                                \
        }
        CMPF(Uint16, canRxTxCon)
        CMPF(Uint16, errorOther)
        CMPF(Uint16, errorInfo)
        CMPF(Uint16, readOut)
#undef CMPF

#define CMPM(n)                                                             \
        if (memcmp(&st[0].n, &st[1].n, sizeof(st[0].n)))                    \
        { report(#n, "aggregate differs"); }
        CMPM(recBuf)
        CMPM(chara)
        CMPM(fc)
        CMPM(p2p)
        CMPM(sci)
        CMPM(rcv)
        CMPM(rd)
        CMPM(ct)
        CMPM(scratch)
#undef CMPM

        if (ret[0] != ret[1])
        {
            char buf[96];
            sprintf(buf, "ref=%ld new=%ld", (long)ret[0], (long)ret[1]);
            report("return value", buf);
        }

        if (trn[0] != trn[1] || trh[0] != trh[1] ||
            memcmp(tr[0], tr[1], sizeof(tr[0])))
        {
            char buf[224];
            unsigned d = 0;

            while (d < CANT_N &&
                   tr[0][d].which == tr[1][d].which &&
                   tr[0][d].a0 == tr[1][d].a0 &&
                   tr[0][d].a1 == tr[1][d].a1 &&
                   tr[0][d].a2 == tr[1][d].a2 &&
                   tr[0][d].a3 == tr[1][d].a3)
            {
                d++;
            }
            sprintf(buf, "n ref=%lu new=%lu hash %08lx/%08lx first-diff %u "
                         "ref=(%u,%lu,%lu,%lu,%lu) new=(%u,%lu,%lu,%lu,%lu)",
                    (unsigned long)trn[0], (unsigned long)trn[1],
                    (unsigned long)trh[0], (unsigned long)trh[1], d,
                    tr[0][d].which, (unsigned long)tr[0][d].a0,
                    (unsigned long)tr[0][d].a1, (unsigned long)tr[0][d].a2,
                    (unsigned long)tr[0][d].a3,
                    tr[1][d].which, (unsigned long)tr[1][d].a0,
                    (unsigned long)tr[1][d].a1, (unsigned long)tr[1][d].a2,
                    (unsigned long)tr[1][d].a3);
            report("stub call trace", buf);
        }
    }
}

/* ---------------------------------------------------------------------------
 * Input defaults
 * ------------------------------------------------------------------------ */

static struct IN base;

static void init_base(void)
{
    int i;

    memset(&base, 0, sizeof(base));

    /* --- shared globals --- */

    /* funcCode: the CAN_LINK defaults are "plain CAN_LINK", i.e. neither the PLC
     * card nor point-to-point. commBaudRate/1000 is the baud index; 6000 -> 6 =
     * CAN_BAUD_1M. commSlaveAddress & CANLINK_ID_MASK is the station id. */
    base.st.fc.code.plcEnable       = 0;
    base.st.fc.code.p2pEnable       = 0;
    base.st.fc.code.p2pTypeSel      = 0;
    base.st.fc.code.commProtocolSec = 0;
    base.st.fc.code.commSlaveAddress = 3;
    base.st.fc.code.commBaudRate    = 6000;
    base.st.fc.code.u3[10]          = 0;
    base.st.fc.code.u3[11]          = 0;

    base.st.p2p.p2pEnable     = 0;
    base.st.p2p.P2PSendData   = 0x1234;
    base.st.p2p.p2pSendPeriod = 10;
    base.st.p2p.p2pSendTcnt   = 0;
    base.st.p2p.p2pCommErrTcnt = 0;

    base.st.sci.all = 0;
    base.st.rcv.commCmdSaveEeprom = 0;
    for (i = 0; i < RTU_READ_DATA_NUM_MAX; i++)
        base.st.rd[i] = (Uint16)(0x1000 + i);

    base.st.ct.powerOnTimeSec = 0;
    base.st.ct.powerOnTimeM   = 0;

    base.st.errorOther = 0;
    base.st.errorInfo  = 0;

    /* --- module-owned globals --- */

    base.st.canRxTxCon = 0;
    base.st.chara[0]   = 0x00000000;
    base.st.chara[1]   = 0x00002774;

    /* CanlinkRecBuf is an input: CanlinkDataRec rewrites bufFull, so the
     * template must give it a non-zero value and four distinct frames, or a
     * rewrite that stops writing bufFull would agree by accident. */
    base.st.recBuf.bufFull = 0x0F;
    for (i = 0; i < REC_MBOX_NUM; i++)
    {
        base.st.recBuf.buf[i].msgid.all = mkid(0, 3, CAN_LINK_W_REG, 1,
                                               (Uint16)(CAN_REMOTE_FRAME + i));
        base.st.recBuf.buf[i].mdl.all = (Uint32)(0x2000 + i);
        base.st.recBuf.buf[i].mdh.all = (Uint32)(0x3000 + i);
        base.st.recBuf.buf[i].len     = (Uint32)(4 + i);
    }

    /* The caller's scratch frame: a control frame addressed to us. */
    base.st.scratch.msgid.all = mkid(1, 3, CAN_LINK_R_REG, 1, CAN_CONTROL_FRAME);
    base.st.scratch.mdl.all   = (Uint32)0x01020304;
    base.st.scratch.mdh.all   = (Uint32)0x05060708;
    base.st.scratch.len       = 4;

    base.st.readOut = 0;

    /* --- mock knobs --- */

    base.mockInitECanRc  = CAN_INIT_SUCC;
    base.mockTranSuccOn  = 1;               /* first transmit mailbox succeeds */
    base.mockRecOverOn   = 0;               /* no mailbox reports overflow    */
    base.mockRecSuccFrom = 1;               /* every mailbox has data         */
    base.mockCommWriteRc = 0;
    base.mockCommReadRc  = 0;
    base.mockInvTranRc   = 0;
    base.mockInvRecRc    = 0;

    for (i = 0; i < 4; i++)
    {
        base.mockRecWord[i] = mkid((Uint16)(1 + i), 3, 0, 1,
                                   (Uint16)(CAN_REMOTE_FRAME + i));
    }
}

/* ---------------------------------------------------------------------------
 * Sweep tables
 * ------------------------------------------------------------------------ */

static const Uint16 V_ADDR[]   = {0, 1, 2, 3, 0x3F, 0x40, 0xFF, 0xFFFF};
static const Uint16 V_BAUD[]   = {0, 1, 2, 3, 4, 5, 6, 7, 0xFFFF};
static const Uint16 V_INITRC[] = {CAN_INIT_TIME, CAN_INIT_SUCC,
                                  CAN_INIT_TIMEOUT, CAN_INIT_BAUD_ERR, 0xFFFF};
static const Uint16 V_LEN[]    = {0, 1, 4, 8, 16, 0xFFFF};
static const Uint16 V_TIMEOUT[] = {0, 1, 2, 4, 1000, 0xFFFF};
static const Uint16 V_SUCCON[] = {0, 1, 2, 3, 4, 5};
static const Uint16 V_OVERON[] = {0, 1, 2, 3, 4, 5};
static const Uint16 V_SUCCFROM[] = {0, 1, 2, 3, 4, 5};
static const Uint16 V_CODE[]   = {CAN_LINK_DEL, CAN_LINK_INC, CAN_LINK_R_CFG,
                                  CAN_LINK_R_REG, CAN_LINK_W_REG, CAN_LINK_R_INFO,
                                  CAN_LINK_R_WAR, CAN_LINK_W_EEP, CAN_TRAN_TAB_CFG,
                                  CAN_REC_TAB_CFG, CAN_FUN_U3_CFG, 0xFF};
static const Uint16 V_AQ[]     = {0, 1};
static const Uint16 V_FRAMF[]  = {CAN_REMOTE_FRAME, CAN_CONTROL_FRAME,
                                  CAN_CONFIG_FRAME, CAN_DATA_FRAME,
                                  0x0, 0x1, 0xE, 0xF};
/* The command codes that leave CanLinkConDeal's `err` assigned -- hazard (d).
 * Anything the driver feeds into a control frame must come from this table, or
 * the case would branch on an indeterminate value. */
static const Uint16 V_CODE_ASSIGNED[] = {CAN_LINK_DEL, CAN_LINK_INC,
                                         CAN_LINK_R_REG, CAN_LINK_W_REG,
                                         CAN_LINK_W_EEP, CAN_TRAN_TAB_CFG,
                                         CAN_REC_TAB_CFG, CAN_FUN_U3_CFG, 0xFF};
static const Uint16 V_BOOL[]   = {0, 1};
static const Uint16 V_WORD[]   = {0, 1, 0x7FFF, 0x8000, 0xFFFE, 0xFFFF};
static const Uint16 V_ERRSAT[] = {0, 1, 65533, 65534, 65535};
static const Uint16 V_PWRON[]  = {0, 10, 11, 0xFFFF};
static const Uint16 V_BAUDSEL[] = {0, 1000, 3000, 6000, 65000};
/* Small timeouts for probe_tran_residue(). With every mailbox failing, a call
 * returns TIMEOUT when count+1 >= timeOut and BUSY otherwise, so 2 / 3 / 4 read
 * out a residue of 0 / 1 / 2. */
static const Uint16 V_RESIDUE[] = {2, 3, 4};

#define NELEM(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* ---------------------------------------------------------------------------
 * Retry-counter probe
 * ------------------------------------------------------------------------ */

/* Read out the retry counter that CanlinkDataTran left behind.
 *
 * `count` is a function-local static. cl2000 makes it a local symbol, so it is
 * invisible to the driver and to target_check -- it is not in the observation
 * set, and it cannot be reset. It is still module state: the two sides run the
 * same call sequence, so it has to evolve identically, and the only way to see
 * it from outside is a later call whose threshold the residue straddles.
 *
 * That matters because **every call site in the project passes timeOut = 1000**
 * -- f_canlink.c twice (CanLlinkDataDeal, CanHostTx), f_plc.c once. With a
 * threshold that high, "count was cleared after a success" and "count was
 * cleared after a timeout" are indistinguishable in the real program: the
 * difference only shows up after ~999 further failing rounds. A rewrite that
 * got the clearing wrong, or a call site whose 1000 became 1, would therefore
 * sail through a suite that only ever calls CanlinkDataTran the way the project
 * does. (It was measured, not assumed: before this probe existed, three such
 * mutants -- success-path clearing, timeout-path clearing, and CanLlinkDataDeal's
 * own timeOut -- were the only ones in the whole 138-mutant set that the driver
 * did not catch.)
 *
 * So after every phase that can leave a residue, the driver asks the question
 * directly: an all-fail call with a small timeOut, whose return value carries
 * the counter out. A timeOut of 2 splits residue 0 (BUSY) from residue 1
 * (TIMEOUT); 3 and 4 do the same one and two units further up.
 *
 * The probe is a normal case, so it is compared like any other, and it also
 * re-normalises the counter for whatever runs next.
 */
static void probe_tran_residue(void)
{
    struct IN in;
    int i;

    for (i = 0; i < NELEM(V_RESIDUE); i++)
    {
        in = base;
        in.arg0 = 4;
        in.arg1 = V_RESIDUE[i];
        in.mockTranSuccOn = 0;      /* every mailbox fails -> walk the counter */
        run_case(&in, ENTRY_TRAN, 0);
    }
}

/* ---------------------------------------------------------------------------
 * Phases
 * ------------------------------------------------------------------------ */

static void phase_inittran(void)
{
    struct IN in;
    int i;

    for (i = 0; i < NELEM(V_ADDR); i++)
    {
        in = base;
        in.arg0 = V_ADDR[i];
        run_case(&in, ENTRY_INITTRAN, 0);
    }
}

static void phase_tran(void)
{
    struct IN in;
    int i, j, k;

    for (i = 0; i < NELEM(V_LEN); i++)
    for (j = 0; j < NELEM(V_TIMEOUT); j++)
    {
        in = base;
        in.arg0 = V_LEN[i];
        in.arg1 = V_TIMEOUT[j];
        run_case(&in, ENTRY_TRAN, 0);
    }

    /* The transmit status decides which arm of the mailbox loop returns, so
     * sweep where the first success lands, and the "never succeeds" case. */
    for (k = 0; k < NELEM(V_SUCCON); k++)
    {
        in = base;
        in.arg0 = 4;
        in.arg1 = 1;
        in.mockTranSuccOn = V_SUCCON[k];
        run_case(&in, ENTRY_TRAN, 0);
    }

    /* Both the success arm and the timeout arm clear `count`; the sweep above
     * walks both, so the residue here is exactly what those two `count = 0;`
     * statements left. */
    probe_tran_residue();
}

static void phase_initrec(void)
{
    struct IN in;
    int i;

    for (i = 0; i < NELEM(V_ADDR); i++)
    {
        in = base;
        in.arg0 = V_ADDR[i];
        run_case(&in, ENTRY_INITREC, 0);
    }
}

static void phase_rec(void)
{
    struct IN in;
    int i, j, k;

    for (i = 0; i < NELEM(V_SUCCFROM); i++)
    for (j = 0; j < NELEM(V_OVERON); j++)
    {
        in = base;
        in.mockRecSuccFrom = V_SUCCFROM[i];
        in.mockRecOverOn   = V_OVERON[j];
        run_case(&in, ENTRY_REC, 0);
    }

    /* Four frames with four different frame types, and a pre-set bufFull that
     * the function must overwrite. */
    for (k = 0; k < NELEM(V_FRAMF); k++)
    {
        in = base;
        in.mockRecWord[0] = mkid(1, 3, 0, 1, V_FRAMF[k]);
        in.mockRecWord[1] = mkid(2, 3, 0, 1, V_FRAMF[(k + 1) % NELEM(V_FRAMF)]);
        in.mockRecWord[2] = mkid(3, 3, 0, 1, V_FRAMF[(k + 2) % NELEM(V_FRAMF)]);
        in.mockRecWord[3] = mkid(4, 3, 0, 1, V_FRAMF[(k + 3) % NELEM(V_FRAMF)]);
        in.st.recBuf.bufFull = (Uint16)(0x0F ^ (k & 0x0F));
        run_case(&in, ENTRY_REC, 0);
    }
}

static void phase_init(void)
{
    struct IN in;
    int i, j, k;

    for (i = 0; i < NELEM(V_ADDR); i++)
    for (j = 0; j < NELEM(V_BAUD); j++)
    {
        in = base;
        in.arg0 = V_ADDR[i];
        in.arg1 = V_BAUD[j];
        run_case(&in, ENTRY_INIT, 0);
    }

    for (k = 0; k < NELEM(V_INITRC); k++)
    {
        in = base;
        in.arg0 = 3;
        in.arg1 = CAN_BAUD_1M;
        in.mockInitECanRc = V_INITRC[k];
        run_case(&in, ENTRY_INIT, 0);
    }
}

static void phase_writer(void)
{
    struct IN in;
    int i, j, k;

    for (i = 0; i < NELEM(V_WORD); i++)
    for (j = 0; j < NELEM(V_WORD); j++)
    for (k = 0; k < NELEM(V_BOOL); k++)
    {
        in = base;
        in.arg0 = V_WORD[i];
        in.arg1 = V_WORD[j];
        in.arg2 = V_BOOL[k];
        in.mockCommWriteRc = V_WORD[(i + j) % NELEM(V_WORD)];
        run_case(&in, ENTRY_WRITER, 0);
    }
}

static void phase_reader(void)
{
    struct IN in;
    int i, j;

    for (i = 0; i < NELEM(V_WORD); i++)
    for (j = 0; j < NELEM(V_WORD); j++)
    {
        in = base;
        in.arg0 = V_WORD[i];
        in.mockCommReadRc = V_WORD[j];
        in.st.rd[0] = V_WORD[(i + j) % NELEM(V_WORD)];
        run_case(&in, ENTRY_READER, 0);
    }
}

static void phase_con(void)
{
    struct IN in;
    int i, j, k;

    /* Sweep the command code. The three arms that leave `err` unassigned are
     * excluded -- hazard (d). */
    for (i = 0; i < NELEM(V_CODE); i++)
    {
        if (V_CODE[i] == CAN_LINK_R_CFG || V_CODE[i] == CAN_LINK_R_INFO ||
            V_CODE[i] == CAN_LINK_R_WAR)
            continue;

        /* The answer frame goes out through CanlinkDataTran(..., len, 1000).
         * base sets mockTranSuccOn = 1, i.e. the transmit always succeeds, which
         * clears the retry counter and hides anything the tail call's timeOut
         * argument does. Sweep the transmit outcome too, so the all-fail arm is
         * reached and probe_tran_residue() has something to read. */
        for (k = 0; k < NELEM(V_SUCCON); k++)
        {
            in = base;
            in.st.scratch.msgid.bit.code = V_CODE[i];
            in.st.scratch.msgid.bit.aq   = 1;
            in.mockTranSuccOn = V_SUCCON[k];
            run_case(&in, ENTRY_CON, 0);
        }
    }

    /* The aq == ACK early return. */
    for (j = 0; j < NELEM(V_AQ); j++)
    {
        in = base;
        in.st.scratch.msgid.bit.code = CAN_LINK_R_REG;
        in.st.scratch.msgid.bit.aq   = V_AQ[j];
        run_case(&in, ENTRY_CON, 0);
    }

    /* The error path and the password path. */
    for (k = 0; k < NELEM(V_WORD); k++)
    {
        in = base;
        in.st.scratch.msgid.bit.code = CAN_LINK_R_REG;
        in.st.scratch.msgid.bit.aq   = 1;
        in.mockCommReadRc = V_WORD[k];       /* nonzero -> err -> 0x8001/err */
        run_case(&in, ENTRY_CON, 0);

        in = base;
        in.st.scratch.msgid.bit.code = CAN_LINK_W_REG;
        in.st.scratch.msgid.bit.aq   = 1;
        in.st.sci.bit.pwdPass = 1;
        in.mockCommWriteRc = V_WORD[k];
        run_case(&in, ENTRY_CON, 0);
    }

    /* The PLC table-config commands. */
    for (k = 0; k < NELEM(V_WORD); k++)
    {
        in = base;
        in.st.scratch.msgid.bit.code = CAN_TRAN_TAB_CFG;
        in.st.scratch.msgid.bit.aq   = 1;
        in.mockInvTranRc = V_WORD[k];
        run_case(&in, ENTRY_CON, 0);

        in = base;
        in.st.scratch.msgid.bit.code = CAN_REC_TAB_CFG;
        in.st.scratch.msgid.bit.aq   = 1;
        in.mockInvRecRc = V_WORD[k];
        run_case(&in, ENTRY_CON, 0);
    }

    /* The answer frame goes out through CanlinkDataTran(..., len, 1000). */
    probe_tran_residue();
}

static void phase_remote(void)
{
    struct IN in;
    int i, j;

    for (i = 0; i < NELEM(V_AQ); i++)
    for (j = 0; j < NELEM(V_SUCCON); j++)
    {
        in = base;
        in.st.scratch.msgid.bit.framf = CAN_REMOTE_FRAME;
        in.st.scratch.msgid.bit.aq    = V_AQ[i];
        in.mockTranSuccOn = V_SUCCON[j];
        run_case(&in, ENTRY_REMOTE, 0);
    }

    /* The answer frame goes out through CanlinkDataTran(..., 8, 1000). */
    probe_tran_residue();
}

static void phase_data(void)
{
    struct IN in;
    int i, j, k;

    for (i = 0; i < NELEM(V_FRAMF); i++)
    for (j = 0; j < NELEM(V_AQ); j++)
    for (k = 0; k < NELEM(V_SUCCON); k++)
    {
        in = base;
        in.st.scratch.msgid.bit.framf = V_FRAMF[i];
        in.st.scratch.msgid.bit.aq    = V_AQ[j];
        in.st.scratch.msgid.bit.code  = CAN_LINK_W_REG;
        /* The control arm ends in CanLinkConDeal, whose answer frame goes out
         * through CanlinkDataTran(..., len, 1000); the remote arm goes out
         * through CanlinkDataTran(..., 8, 1000). Sweep the transmit outcome so
         * both arms see a failing transmit as well as a succeeding one. */
        in.mockTranSuccOn = V_SUCCON[k];
        run_case(&in, ENTRY_DATADEAL, 0);
    }

    /* The default arm saturates the frame-error counter. */
    for (i = 0; i < NELEM(V_ERRSAT); i++)
    {
        in = base;
        in.st.scratch.msgid.bit.framf = 0x0;
        in.st.fc.code.u3[11] = V_ERRSAT[i];
        run_case(&in, ENTRY_DATADEAL, 0);
    }

    /* The data arm ends in CanlinkDataTran(..., len, 1000) with the result
     * discarded, so the only trace it leaves is in the retry counter. */
    probe_tran_residue();
}

static void phase_host(void)
{
    struct IN in;
    int i, j;

    for (i = 0; i < NELEM(V_WORD); i++)
    for (j = 0; j < NELEM(V_SUCCON); j++)
    {
        in = base;
        in.arg0 = V_WORD[i];
        in.arg1 = V_WORD[(i + 1) % NELEM(V_WORD)];
        in.mockTranSuccOn = V_SUCCON[j];
        in.st.p2p.p2pCommErrTcnt = V_WORD[(i + 2) % NELEM(V_WORD)];
        run_case(&in, ENTRY_HOST, 0);
    }

    /* CanHostTx goes out through CanlinkDataTran(..., 4, 1000). */
    probe_tran_residue();
}

static void phase_fun(void)
{
    struct IN in;
    int i, j, k;

    /* Re-init selection: the four configurations the task can be in, crossed
     * with the timeout counters and the power-on gate. */
    for (i = 0; i < NELEM(V_BOOL); i++)          /* plcEnable   */
    for (j = 0; j < NELEM(V_BOOL); j++)          /* p2pEnable   */
    for (k = 0; k < NELEM(V_BOOL); k++)          /* p2pTypeSel  */
    {
        in = base;
        in.st.fc.code.plcEnable  = V_BOOL[i];
        in.st.fc.code.p2pEnable  = V_BOOL[j];
        in.st.fc.code.p2pTypeSel = V_BOOL[k];
        in.st.ct.powerOnTimeSec  = 11;
        run_case(&in, ENTRY_FUN, 0);
    }

    /* The baud/id selection inputs. */
    for (i = 0; i < NELEM(V_BAUDSEL); i++)
    {
        in = base;
        in.st.fc.code.commBaudRate    = V_BAUDSEL[i];
        in.st.fc.code.commSlaveAddress = V_ADDR[i % NELEM(V_ADDR)];
        run_case(&in, ENTRY_FUN, 0);
    }

    /* The receive path: four frames of four types, or an empty mailbox that
     * walks the timeout counter. */
    for (i = 0; i < NELEM(V_FRAMF); i++)
    {
        in = base;
        in.mockRecWord[0] = mkid(1, 3, 0, 1, V_FRAMF[i]);
        in.mockRecWord[1] = mkid(2, 3, 0, 1, V_FRAMF[(i + 1) % NELEM(V_FRAMF)]);
        in.mockRecWord[2] = mkid(3, 3, 0, 1, V_FRAMF[(i + 2) % NELEM(V_FRAMF)]);
        in.mockRecWord[3] = mkid(4, 3, 0, 1, V_FRAMF[(i + 3) % NELEM(V_FRAMF)]);
        run_case(&in, ENTRY_FUN, 0);
    }

    /* The timeout / error path, including the u3[10] saturation and the PLC
     * power-on gate. */
    for (i = 0; i < NELEM(V_ERRSAT); i++)
    for (j = 0; j < NELEM(V_PWRON); j++)
    {
        in = base;
        in.st.fc.code.plcEnable = 1;
        in.st.fc.code.u3[10]    = V_ERRSAT[i];
        in.st.ct.powerOnTimeSec = V_PWRON[j];
        in.mockRecSuccFrom = 0;              /* every mailbox empty */
        in.mockRecOverOn   = 0;
        run_case(&in, ENTRY_FUN, 0);
    }

    /* The point-to-point master path. */
    for (i = 0; i < NELEM(V_SUCCON); i++)
    {
        in = base;
        in.st.fc.code.p2pEnable = 1;
        in.st.p2p.p2pEnable     = 1;
        in.st.p2p.p2pSendTcnt   = 10;        /* >= p2pSendPeriod -> transmit */
        in.st.p2p.p2pSendPeriod = 10;
        in.mockTranSuccOn = V_SUCCON[i];
        run_case(&in, ENTRY_FUN, 0);
    }

    /* CanlinkFun reaches CanlinkDataTran two ways: the point-to-point master
     * transmit above, and CanLlinkDataDeal for any received data frame. */
    probe_tran_residue();
}

static void phase_seq(void)
{
    struct IN in;
    static const int runs[] = {2, 3, 4, 6, 10};
    int i;

    /* Consecutive calls: the timeout counter needs >4 ticks to fire, the
     * re-init arm depends on sBaud/sSourID, and the point-to-point master only
     * transmits every p2pSendPeriod ticks.  All three are reachable only from a
     * sequence, not from a single call. */
    for (i = 0; i < NELEM(runs); i++)
    {
        in = base;
        in.st.fc.code.plcEnable = 1;
        in.st.ct.powerOnTimeSec = 11;
        in.mockRecSuccFrom = 0;              /* stay in the timeout branch */
        run_case(&in, ENTRY_FUN_SEQ, runs[i]);
    }

    for (i = 0; i < NELEM(runs); i++)
    {
        in = base;
        in.st.fc.code.p2pEnable = 1;
        in.st.p2p.p2pEnable     = 1;
        in.st.p2p.p2pSendPeriod = 2;
        in.st.p2p.p2pSendTcnt   = 0;
        run_case(&in, ENTRY_FUN_SEQ, runs[i]);
    }

    for (i = 0; i < NELEM(runs); i++)
    {
        in = base;
        in.mockRecSuccFrom = 1;
        in.mockRecWord[0] = mkid(1, 3, CAN_LINK_R_REG, 1, CAN_CONTROL_FRAME);
        in.mockRecWord[1] = mkid(2, 3, CAN_LINK_W_REG, 1, CAN_CONTROL_FRAME);
        in.mockRecWord[2] = mkid(3, 3, 0, 1, CAN_DATA_FRAME);
        in.mockRecWord[3] = mkid(4, 3, 0, 1, CAN_REMOTE_FRAME);
        run_case(&in, ENTRY_FUN_SEQ, runs[i]);
    }

    probe_tran_residue();
}

/* ---------------------------------------------------------------------------
 * Random
 * ------------------------------------------------------------------------ */

static unsigned long rnd_state = 0x12345678UL;

static unsigned long rnd(void)
{
    rnd_state = rnd_state * 1103515245UL + 12345UL;
    return (rnd_state >> 16) & 0x7FFFUL;
}

#define PICK(a) ((a)[rnd() % (unsigned long)NELEM(a)])

static void phase_random(long n)
{
    struct IN in;
    long i;

    for (i = 0; i < n; i++)
    {
        int entry;
        int a = 0;

        in = base;

        in.st.fc.code.plcEnable        = (Uint16)PICK(V_BOOL);
        in.st.fc.code.p2pEnable        = (Uint16)PICK(V_BOOL);
        in.st.fc.code.p2pTypeSel       = (Uint16)PICK(V_BOOL);
        in.st.fc.code.commSlaveAddress = (Uint16)PICK(V_ADDR);
        in.st.fc.code.commBaudRate     = (Uint16)PICK(V_BAUDSEL);
        in.st.fc.code.u3[10]           = (Uint16)PICK(V_ERRSAT);
        in.st.fc.code.u3[11]           = (Uint16)PICK(V_ERRSAT);

        in.st.p2p.p2pEnable      = (Uint16)PICK(V_BOOL);
        in.st.p2p.p2pSendPeriod  = (Uint16)PICK(V_TIMEOUT);
        in.st.p2p.p2pSendTcnt    = (Uint16)PICK(V_TIMEOUT);
        in.st.p2p.p2pCommErrTcnt = (Uint16)PICK(V_WORD);
        in.st.p2p.P2PSendData    = (Uint16)PICK(V_WORD);

        in.st.sci.bit.pwdPass = (Uint16)PICK(V_BOOL);
        in.st.rcv.commCmdSaveEeprom = (Uint16)PICK(V_BOOL);
        in.st.rd[0] = (Uint16)PICK(V_WORD);

        in.st.ct.powerOnTimeSec = (Uint16)PICK(V_PWRON);
        in.st.ct.powerOnTimeM   = (Uint16)PICK(V_BOOL);

        in.st.canRxTxCon = (Uint16)PICK(V_ERRSAT);

        in.mockInitECanRc  = (Uint16)PICK(V_INITRC);
        in.mockTranSuccOn  = (Uint16)PICK(V_SUCCON);
        in.mockRecOverOn   = (Uint16)PICK(V_OVERON);
        in.mockRecSuccFrom = (Uint16)PICK(V_SUCCFROM);
        in.mockCommWriteRc = (Uint16)PICK(V_WORD);
        in.mockCommReadRc  = (Uint16)PICK(V_WORD);
        in.mockInvTranRc   = (Uint16)PICK(V_WORD);
        in.mockInvRecRc    = (Uint16)PICK(V_WORD);

        /* The receive frames can be dispatched straight into CanLinkConDeal by
         * CanlinkFun -> CanlinkDataRec -> CanLlinkDataDeal, so their `code`
         * fields must also come from the assigned table -- hazard (d). */
        in.mockRecWord[0] = mkid((Uint16)rnd(), (Uint16)rnd(),
                                 (Uint16)PICK(V_CODE_ASSIGNED), 1,
                                 (Uint16)PICK(V_FRAMF));
        in.mockRecWord[1] = mkid((Uint16)rnd(), (Uint16)rnd(),
                                 (Uint16)PICK(V_CODE_ASSIGNED), 1,
                                 (Uint16)PICK(V_FRAMF));
        in.mockRecWord[2] = mkid((Uint16)rnd(), (Uint16)rnd(),
                                 (Uint16)PICK(V_CODE_ASSIGNED), 1,
                                 (Uint16)PICK(V_FRAMF));
        in.mockRecWord[3] = mkid((Uint16)rnd(), (Uint16)rnd(),
                                 (Uint16)PICK(V_CODE_ASSIGNED), 1,
                                 (Uint16)PICK(V_FRAMF));

        in.st.scratch.msgid.all = mkid((Uint16)rnd(), (Uint16)rnd(),
                                       (Uint16)PICK(V_CODE), (Uint16)PICK(V_AQ),
                                       (Uint16)PICK(V_FRAMF));
        in.st.scratch.mdl.all = ((Uint32)rnd() << 16) | (Uint32)rnd();
        in.st.scratch.mdh.all = ((Uint32)rnd() << 16) | (Uint32)rnd();

        in.arg0 = (Uint16)PICK(V_WORD);
        in.arg1 = (Uint16)PICK(V_WORD);
        in.arg2 = (Uint16)PICK(V_BOOL);

        entry = (int)(rnd() % 13);
        if (entry == ENTRY_FUN_SEQ)
            a = 1 + (int)(rnd() % 8);

        /* The command arms that read an unassigned `err` are excluded -- hazard
         * (d).  CanLinkConDeal is reachable two ways: directly (ENTRY_CON), and
         * through CanLlinkDataDeal when the caller's frame is a control frame. */
        if (entry == ENTRY_CON ||
            (entry == ENTRY_DATADEAL &&
             in.st.scratch.msgid.bit.framf == CAN_CONTROL_FRAME))
        {
            Uint16 c = in.st.scratch.msgid.bit.code;
            if (c == CAN_LINK_R_CFG || c == CAN_LINK_R_INFO || c == CAN_LINK_R_WAR)
                continue;
        }

        run_case(&in, entry, a);
    }

    /* The last random case can leave the counter anywhere. */
    probe_tran_residue();
}

int main(int argc, char **argv)
{
    long nrand = (argc > 1) ? atol(argv[1]) : 20000;
    long base_cases;

    /* Unbuffered: this driver is long-running, and a crash part-way through
     * must not swallow the progress it has already made. */
    setvbuf(stdout, NULL, _IONBF, 0);
    g_trace_on = (getenv("CANTLTRACE") != NULL);

    init_base();

    base_cases = g_cases;
    phase_inittran();
    printf("  InitCanlinkTran              : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_tran();
    printf("  CanlinkDataTran              : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_initrec();
    printf("  InitCanlinkRec               : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_rec();
    printf("  CanlinkDataRec               : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_init();
    printf("  InitCanlink                  : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_writer();
    printf("  CanControlWriter             : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_reader();
    printf("  CanControlRead               : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_con();
    printf("  CanLinkConDeal               : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_remote();
    printf("  CanLinkRemoteDeal            : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_data();
    printf("  CanLlinkDataDeal             : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_host();
    printf("  CanHostTx                    : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_fun();
    printf("  CanlinkFun                   : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_seq();
    printf("  stateful sequences           : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_random(nrand);
    printf("  random                       : %ld\n", g_cases - base_cases);

    printf("f_canlink difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    return g_fails ? 1 : 0;
}
