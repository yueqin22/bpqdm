/* Differential test driver for f_profibus.
 *
 * The module is the PROFIBUS-DP half of the serial protocol layer: seven entry
 * points, all of which are exported (`nm2000` shows them as T), plus nine data
 * objects it owns. It touches no peripheral register. Everything it can do is
 * therefore visible in either the shared objects it writes or its own exports --
 * except for the two calls it makes into f_menu.c, which is why this case has a
 * call trace (see mock_profibus.h).
 *
 * What is compared after every call:
 *
 *   return value            ProfibusStartDeal and ProfibusCommErrCheck
 *   commRcvData             the whole receive descriptor, every field
 *   commSendData            the whole send descriptor, every field
 *   sendFrame[FRAME_N]      the transmit buffer
 *   rcvFrame[FRAME_N]       the receive buffer
 *   commReadData[12]        the register block the read path copies out
 *   commRunCmd              the run command the frame's control word becomes
 *   dpStatus.all            the status word setDPStatusInfo assembles
 *   dpFrqAim                the target frequency latched out of the frame
 *   dpControlCmd/Bak        the module's own command latches (dead, see below)
 *   rcvRigthFlag, DPRcvTest the frame-header state machine
 *   saveEepromFlag/Index    the deferred-EEPROM latch pair
 *   funcCode                the whole union -- compared to prove it stays
 *                           read-only, since the two stubs never write it
 *   funcCodeOneWriteIndex[] the buffer SaveOneFuncCode() appends to
 *   funcCodeOneWriteNum     its write cursor
 *   protTrace / N / hash    the calls into f_menu.c, in order
 *
 * Every input is pre-filled with non-trivial data before each side runs, so a
 * field the rewrite stops writing shows up as a difference instead of staying
 * accidentally equal. commRcvData in particular has 14 fields and
 * ProfibusRcvDataDeal assigns at most 6 of them.
 *
 * ---- bounds the driver enforces rather than assumes ----
 *
 * The module indexes four different arrays from values that come from outside
 * itself, and three of those four are reachable from a frame the DP master
 * sends. The driver keeps every one of them inside the firmware's own contract
 * and refuses to run (exit 2) if a phase ever feeds something outside it:
 *
 *  (a) commProtocol indexes DPDataNum[4]. It is *not* the protocol selector --
 *      f_comm.c:1100 derives it as a decimal digit of funcCode.code.commProtocol
 *      (`commProtocol = digit[commType]`, with commType = PROFIBUS), so its
 *      natural range is 0..9. The menu only ever offers the four PPO selectors
 *      (PPO1/PPO2/PPO3/PPO5 = 0..3) and DPDataNum has exactly those four
 *      entries, so 0..3 is swept. A digit of 4..9 would read past the table and
 *      nothing in the firmware bounds it; that is recorded in
 *      cases/f_profibus/README.md section 7(b) rather than swept here.
 *
 *  (b) commRcvData.rcvNumMax is the frame length, and ProfibusRcvDataDeal reads
 *      rcvFrame[rcvNumMax - 1] and [rcvNumMax - 2]. The firmware's own reachable
 *      values are 5 (a 连通测试 frame) and DPDataNum[p] + 4 = 8/16/24/36. Note
 *      that rcvNumMax is a Uint16: 0 or 1 would make rcvNumMax - 2 wrap to
 *      65534 and read far past the buffer, so the sweep starts at 2.
 *
 *  (c) The same value drives ProfibusSendDataDeal's PZD loop through
 *      `pzdNum = (commSendData.sendNumMax - 16) >> 1`. Below 16 that subtraction
 *      wraps and the loop runs ~32k times, walking readDataStartIndex off the end
 *      of sendFrame. The firmware never reaches that state -- a normal frame
 *      always sets rcvNumMax = DPDataNum[commProtocol] + 4 first, and the
 *      early-returning 连通测试 path leaves rcvNumMax = 5 only while DPRcvTest is
 *      set -- so the driver only ever pairs a sub-16 length with DPRcvTest = 1,
 *      and only ever pairs DPRcvTest = 0 with 16/24/36.
 *
 *  (d) funcCode.group.fe[i] / fe[i + 10] are divided by 100 and used as
 *      funcCodeGradeSum[group] + grade. Nothing in the firmware bounds them; the
 *      values come from the user's own PZD mapping. The driver only installs
 *      values that satisfy group < FUNCCODE_GROUP_NUM and
 *      funcCodeGradeSum[group] + grade < FNUM_ALL, and it builds that set at run
 *      time from the real table rather than hard-coding it.
 *
 * These are the firmware's contracts, not the test's conveniences. Where the
 * original firmware would actually read out of bounds, that is recorded in
 * cases/f_profibus/README.md section 7 rather than papered over here.
 *
 * ---- the two exported functions nothing calls ----
 *
 * getDPControlCmd and setDPStatusInfo are exported (T) but declared in no
 * header and called from nowhere in the project. getDPControlCmd's entire body
 * is inside `#if 0`, so it is an empty function; setDPStatusInfo is live and is
 * reached only through ProfibusSendDataDeal. Both are driven directly here: the
 * former so that "the dead body stays dead" is measured, the latter so that the
 * status-word logic is covered without going through a whole frame.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Same include order as src/function/f_profibus.c (which includes only f_comm.h). */
#include "f_comm.h"
#include "f_error.h"

#include "grade_tables.inc"
#include "mock_profibus.h"

#define READ_N       RTU_READ_DATA_NUM_MAX
#define FE_N         32                 /* FENUM */
#define ONE_WRITE_N  200                /* FUNCCODE_ONE_WRITE_NUMBER_MAX, f_eeprom.c:49 */
#define FAIL_LIMIT   20

/* ---- renamed entry points ------------------------------------------------- */

extern void   ref_ProfibusRcvDataDeal(void);
extern Uint16 ref_ProfibusStartDeal(Uint16 tmp);
extern void   ref_UpdateProfibusCommFormat(Uint16 baudRate);
extern void   ref_ProfibusSendDataDeal(Uint16 err);
extern Uint16 ref_ProfibusCommErrCheck(void);
extern void   ref_getDPControlCmd(void);
extern void   ref_setDPStatusInfo(void);

extern void   new_ProfibusRcvDataDeal(void);
extern Uint16 new_ProfibusStartDeal(Uint16 tmp);
extern void   new_UpdateProfibusCommFormat(Uint16 baudRate);
extern void   new_ProfibusSendDataDeal(Uint16 err);
extern Uint16 new_ProfibusCommErrCheck(void);
extern void   new_getDPControlCmd(void);
extern void   new_setDPStatusInfo(void);

/* The module's own data. dpControlCmd / dpControlCmdBak / dpStatus are unions
 * declared inside f_profibus.c and in no header; each is exactly one Uint16
 * wide (a Uint16 and a bitfield struct of 16 bits), so the driver refers to them
 * as Uint16. That is the only reason these three are not spelled with their real
 * type -- the type is not visible outside the module. */
extern Uint16 ref_DPRcvTest;
extern Uint16 ref_dpFrqAim;
extern Uint16 ref_dpStatus;
extern Uint16 ref_dpControlCmd;
extern Uint16 ref_dpControlCmdBak;
extern Uint16 ref_rcvRigthFlag;
extern Uint16 ref_saveEepromFlag;
extern Uint16 ref_saveEepromIndex;
extern const Uint16 ref_DPDataNum[4];

extern Uint16 new_DPRcvTest;
extern Uint16 new_dpFrqAim;
extern Uint16 new_dpStatus;
extern Uint16 new_dpControlCmd;
extern Uint16 new_dpControlCmdBak;
extern Uint16 new_rcvRigthFlag;
extern Uint16 new_saveEepromFlag;
extern Uint16 new_saveEepromIndex;
extern const Uint16 new_DPDataNum[4];

/* ---- side plumbing ------------------------------------------------------- */

static void (*g_rcvDeal[2])(void);
static Uint16 (*g_startDeal[2])(Uint16);
static void (*g_updateFormat[2])(Uint16);
static void (*g_sendDeal[2])(Uint16);
static Uint16 (*g_errCheck[2])(void);
static void (*g_getCmd[2])(void);
static void (*g_setStatus[2])(void);

static void init_ptrs(void)
{
    g_rcvDeal[0]      = ref_ProfibusRcvDataDeal;      g_rcvDeal[1]      = new_ProfibusRcvDataDeal;
    g_startDeal[0]    = ref_ProfibusStartDeal;        g_startDeal[1]    = new_ProfibusStartDeal;
    g_updateFormat[0] = ref_UpdateProfibusCommFormat; g_updateFormat[1] = new_UpdateProfibusCommFormat;
    g_sendDeal[0]     = ref_ProfibusSendDataDeal;     g_sendDeal[1]     = new_ProfibusSendDataDeal;
    g_errCheck[0]     = ref_ProfibusCommErrCheck;     g_errCheck[1]     = new_ProfibusCommErrCheck;
    g_getCmd[0]       = ref_getDPControlCmd;          g_getCmd[1]       = new_getDPControlCmd;
    g_setStatus[0]    = ref_setDPStatusInfo;          g_setStatus[1]    = new_setDPStatusInfo;
}

struct SIDE
{
    Uint16 ret;
    COMM_RCV_DATA  rcv;
    COMM_SEND_DATA snd;
    Uint16 send[FRAME_N];
    Uint16 rcvF[FRAME_N];
    Uint16 read[READ_N];
    Uint16 runCmd;
    Uint16 dpStatusAll;
    Uint16 dpFrqAim;
    Uint16 dpCmdAll;
    Uint16 dpCmdBakAll;
    Uint16 rcvRigthFlag;
    Uint16 dpRcvTest;
    Uint16 saveEepromFlag;
    Uint16 saveEepromIndex;
    Uint16 oneWrite[ONE_WRITE_N];
    Uint16 oneWriteNum;
    FUNCCODE_ALL fc;
    Uint32 ticker;
    Uint16 sciAll;
    Uint16 protocol;
    Uint32 traceN;
    Uint32 traceHash;
    struct PROT_ENT trace[PROT_N];
};

static struct SIDE S[2];

/* ---- input state -------------------------------------------------------- */

static COMM_RCV_DATA g_inRcv;
static COMM_SEND_DATA g_inSnd;
static Uint16 g_inSend[FRAME_N];
static Uint16 g_inRcvF[FRAME_N];
static Uint16 g_inRead[READ_N];
static Uint16 g_inRunCmd;
static Uint32 g_inTicker;
static Uint16 g_inSciAll;
static Uint16 g_inProtocol;
static Uint16 g_inDpCmd;
static Uint16 g_inDpCmdBak;
static Uint16 g_inRcvRigthFlag;
static Uint16 g_inDpRcvTest;
static Uint16 g_inSaveEepromFlag;
static Uint16 g_inSaveEepromIndex;
static FUNCCODE_ALL g_inFc;

/* The remaining globals the module reads. They are inputs like any other: the
 * status word depends on runFlag/frq/frqAim/errorCode, the PZD2 reply depends on
 * motorFc.motorCtrlMode, and the latched target frequency depends on maxFrq. A
 * rewrite that stopped consulting any of them has to show up as a difference, so
 * each is installed per side rather than left as whatever the last phase set. */
static Uint16 g_inMaxFrq;
static int32  g_inFrq;
static int32  g_inFrqAim;
static Uint16 g_inFrqFdb;
static Uint16 g_inFrqRunDisp;
static Uint16 g_inErrorCode;
static Uint16 g_inRunFlagAll;
static Uint16 g_inMotorCtrlMode;

/* The three PROFIBUS header bytes are #defined inside src/function/f_profibus.c,
 * not in any header, so the driver has to restate them. */
#define DP_START_HIGH   0xAA
#define DP_START_LOW    0x55
#define DP_CHECK        0x5A

static void install(void)
{
    /* Shared objects: one instance, read and written by whichever side is
     * currently running, so they are re-installed before each side. */
    memcpy(&commRcvData, &g_inRcv, sizeof g_inRcv);
    memcpy(&commSendData, &g_inSnd, sizeof g_inSnd);
    memcpy(sendFrame, g_inSend, sizeof g_inSend);
    memcpy(rcvFrame, g_inRcvF, sizeof g_inRcvF);
    memcpy(commReadData, g_inRead, sizeof g_inRead);
    commTicker  = g_inTicker;
    sciFlag.all = g_inSciAll;
    commProtocol = g_inProtocol;
    commRunCmd  = g_inRunCmd;
    memcpy(&funcCode, &g_inFc, sizeof g_inFc);

    maxFrq = g_inMaxFrq;
    frq = g_inFrq;
    frqAim = g_inFrqAim;
    frqFdb = g_inFrqFdb;
    frqRunDisp = g_inFrqRunDisp;
    errorCode = g_inErrorCode;
    runFlag.all = g_inRunFlagAll;
    motorFc.motorCtrlMode = g_inMotorCtrlMode;

    /* The module's own objects: duplicated per side, so both copies get the
     * same inputs. */
    ref_rcvRigthFlag      = new_rcvRigthFlag      = g_inRcvRigthFlag;
    ref_DPRcvTest         = new_DPRcvTest         = g_inDpRcvTest;
    ref_saveEepromFlag    = new_saveEepromFlag    = g_inSaveEepromFlag;
    ref_saveEepromIndex   = new_saveEepromIndex   = g_inSaveEepromIndex;
    ref_dpControlCmd      = new_dpControlCmd      = g_inDpCmd;
    ref_dpControlCmdBak   = new_dpControlCmdBak   = g_inDpCmdBak;
    ref_dpFrqAim          = new_dpFrqAim          = 0;
    ref_dpStatus          = new_dpStatus          = 0;

    /* SaveOneFuncCode() appends at funcCodeOneWriteNum, so the cursor is part
     * of the input and the buffer is pre-filled: "one entry appended" has to be
     * distinguishable from "nothing appended". */
    {
        Uint16 i;
        for (i = 0; i < ONE_WRITE_N; i++)
        {
            funcCodeOneWriteIndex[i] = (Uint16)(0x4000 + i);
        }
    }
    funcCodeOneWriteNum = 0;

    mockReset();
}

static void snapshot(int side)
{
    struct SIDE *s = &S[side];

    memcpy(&s->rcv, &commRcvData, sizeof s->rcv);
    memcpy(&s->snd, &commSendData, sizeof s->snd);
    memcpy(s->send, sendFrame, sizeof s->send);
    memcpy(s->rcvF, rcvFrame, sizeof s->rcvF);
    memcpy(s->read, commReadData, sizeof s->read);
    s->runCmd   = commRunCmd;
    s->ticker   = commTicker;
    s->sciAll   = sciFlag.all;
    s->protocol = commProtocol;
    memcpy(&s->fc, &funcCode, sizeof s->fc);
    memcpy(s->oneWrite, funcCodeOneWriteIndex, sizeof s->oneWrite);
    s->oneWriteNum = funcCodeOneWriteNum;

    s->traceN    = protTraceN;
    s->traceHash = protTraceHash;
    memcpy(s->trace, protTrace, sizeof s->trace);

    if (side == 0)
    {
        s->dpStatusAll     = ref_dpStatus;
        s->dpFrqAim        = ref_dpFrqAim;
        s->dpCmdAll        = ref_dpControlCmd;
        s->dpCmdBakAll     = ref_dpControlCmdBak;
        s->rcvRigthFlag    = ref_rcvRigthFlag;
        s->dpRcvTest       = ref_DPRcvTest;
        s->saveEepromFlag  = ref_saveEepromFlag;
        s->saveEepromIndex = ref_saveEepromIndex;
    }
    else
    {
        s->dpStatusAll     = new_dpStatus;
        s->dpFrqAim        = new_dpFrqAim;
        s->dpCmdAll        = new_dpControlCmd;
        s->dpCmdBakAll     = new_dpControlCmdBak;
        s->rcvRigthFlag    = new_rcvRigthFlag;
        s->dpRcvTest       = new_DPRcvTest;
        s->saveEepromFlag  = new_saveEepromFlag;
        s->saveEepromIndex = new_saveEepromIndex;
    }
}

/* ---- case runner -------------------------------------------------------- */

static long g_cases = 0;
static int  g_fails = 0;

static void report(const char *fn, const char *what)
{
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s: %s\n", fn, what);
    printf("  in: protocol=%u rcvNumMax=%u dpRcvTest=%u sciFlag=%04X err/tmp? ticker=%lu\n",
           g_inProtocol, g_inRcv.rcvNumMax, g_inDpRcvTest, g_inSciAll,
           (unsigned long)g_inTicker);
    printf("      rcvF[0..9]=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
           g_inRcvF[0], g_inRcvF[1], g_inRcvF[2], g_inRcvF[3], g_inRcvF[4],
           g_inRcvF[5], g_inRcvF[6], g_inRcvF[7], g_inRcvF[8], g_inRcvF[9]);
    printf("      saveIdx=%u saveFlag=%u upDownRc=%u\n",
           g_inSaveEepromIndex, g_inSaveEepromFlag, mockUpDownRc);
}

/* kind: 0 RcvDataDeal, 1 StartDeal(arg), 2 UpdateCommFormat(arg),
 *       3 SendDataDeal(arg), 4 CommErrCheck, 5 getDPControlCmd,
 *       6 setDPStatusInfo,
 *       7 chain: normal frame  (0xAA, 0x55) -> RcvDataDeal -> SendDataDeal,
 *       8 chain: 连通测试 frame (0x5A, addr, 0x55) -> RcvDataDeal -> SendDataDeal. */
static void run_case(const char *fn, int kind, Uint16 arg)
{
    int side;

    g_cases++;

    for (side = 0; side < 2; side++)
    {
        install();

        switch (kind)
        {
        case 0:
            g_rcvDeal[side]();
            S[side].ret = 0;
            break;
        case 1:
            S[side].ret = g_startDeal[side](arg);
            break;
        case 2:
            g_updateFormat[side](arg);
            S[side].ret = 0;
            break;
        case 3:
            g_sendDeal[side](arg);
            S[side].ret = 0;
            break;
        case 4:
            S[side].ret = g_errCheck[side]();
            break;
        case 5:
            g_getCmd[side]();
            S[side].ret = 0;
            break;
        case 6:
            g_setStatus[side]();
            S[side].ret = 0;
            break;
        case 7:
            g_startDeal[side](DP_START_HIGH);
            g_startDeal[side](DP_START_LOW);
            g_rcvDeal[side]();
            g_sendDeal[side](arg);
            S[side].ret = 0;
            break;
        default:
            g_startDeal[side](DP_CHECK);
            g_startDeal[side](0x0005);
            g_startDeal[side](DP_START_LOW);
            g_rcvDeal[side]();
            g_sendDeal[side](arg);
            S[side].ret = 0;
            break;
        }

        snapshot(side);
    }

#define CMP(field, label)                                                   \
    if (S[0].field != S[1].field) { report(fn, label); return; }

    CMP(ret, "return value")
    if (memcmp(&S[0].rcv, &S[1].rcv, sizeof S[0].rcv) != 0) { report(fn, "commRcvData"); return; }
    if (memcmp(&S[0].snd, &S[1].snd, sizeof S[0].snd) != 0) { report(fn, "commSendData"); return; }
    if (memcmp(S[0].send, S[1].send, sizeof S[0].send) != 0) { report(fn, "sendFrame"); return; }
    if (memcmp(S[0].rcvF, S[1].rcvF, sizeof S[0].rcvF) != 0) { report(fn, "rcvFrame"); return; }
    if (memcmp(S[0].read, S[1].read, sizeof S[0].read) != 0) { report(fn, "commReadData"); return; }
    CMP(runCmd, "commRunCmd")
    CMP(ticker, "commTicker")
    CMP(sciAll, "sciFlag")
    CMP(protocol, "commProtocol")
    CMP(dpStatusAll, "dpStatus")
    CMP(dpFrqAim, "dpFrqAim")
    CMP(dpCmdAll, "dpControlCmd")
    CMP(dpCmdBakAll, "dpControlCmdBak")
    CMP(rcvRigthFlag, "rcvRigthFlag")
    CMP(dpRcvTest, "DPRcvTest")
    CMP(saveEepromFlag, "saveEepromFlag")
    CMP(saveEepromIndex, "saveEepromIndex")
    CMP(oneWriteNum, "funcCodeOneWriteNum")
    if (memcmp(S[0].oneWrite, S[1].oneWrite, sizeof S[0].oneWrite) != 0)
    {
        report(fn, "funcCodeOneWriteIndex");
        return;
    }
    if (memcmp(&S[0].fc, &S[1].fc, sizeof S[0].fc) != 0)
    {
        report(fn, "funcCode (should be read-only)");
        return;
    }
    CMP(traceN, "call count into f_menu.c")
    CMP(traceHash, "call trace hash")
    if (memcmp(S[0].trace, S[1].trace, sizeof S[0].trace) != 0)
    {
        report(fn, "call trace entries");
        return;
    }
#undef CMP
}

/* ---- input construction -------------------------------------------------- */

static Uint32 g_seed = 0x13579BDFUL;
static long   g_nrand = 20000;

static Uint16 rnd(void)
{
    g_seed = g_seed * 1103515245UL + 12345UL;
    return (Uint16)(g_seed >> 16);
}

static Uint32 rnd32(void)
{
    return ((Uint32)rnd() << 16) | rnd();
}

static void reset_input(void)
{
    memset(&g_inRcv, 0, sizeof g_inRcv);
    memset(&g_inSnd, 0, sizeof g_inSnd);
    memset(g_inSend, 0, sizeof g_inSend);
    memset(g_inRcvF, 0, sizeof g_inRcvF);
    memset(g_inRead, 0, sizeof g_inRead);
    memset(&g_inFc, 0, sizeof g_inFc);
    g_inRunCmd = 0;
    g_inTicker = 0;
    g_inSciAll = 0;
    g_inProtocol = 0;
    g_inDpCmd = 0;
    g_inDpCmdBak = 0;
    g_inRcvRigthFlag = 0;
    g_inDpRcvTest = 0;
    g_inSaveEepromFlag = 0;
    g_inSaveEepromIndex = 0;
    g_inMaxFrq = 0;
    g_inFrq = 0;
    g_inFrqAim = 0;
    g_inFrqFdb = 0;
    g_inFrqRunDisp = 0;
    g_inErrorCode = 0;
    g_inRunFlagAll = 0;
    g_inMotorCtrlMode = 0;
    mockUpDownRc = 0;
}

/* Pre-fill everything the module is supposed to leave alone, so "left alone" is
 * observable rather than accidental. */
static void prefill_all(void)
{
    Uint16 i;

    for (i = 0; i < FRAME_N; i++)
    {
        g_inSend[i] = (Uint16)(0x1100 + i);
        /* 0x2A00, not 0x2200. rcvFrame[3] is DPhighFuncAddr, and the EEPROM
         * branch derives its group as (DPhighFuncAddr >> 8) & 0x0F. With a high
         * byte of 0x22 bit 3 of that mask is never set, so `& 0x0F` and
         * `& 0x07` are indistinguishable and mutant P063 escapes. 0x2A03 gives
         * 0x2A & 0x0F == 0x0A against 0x2A & 0x07 == 0x02 -- different group,
         * different funcCodeGradeSum entry, different eepromIndex. Sub-phase (a)
         * sweeps the whole sciFlag space with this frame, so the write arm is
         * reached tens of thousands of times. */
        g_inRcvF[i] = (Uint16)(0x2A00 + i);
    }
    for (i = 0; i < READ_N; i++)
    {
        /* 0x33A5, not 0x3300. The read branch splits commReadData[0] into
         * `>> 8` and `& 0x00ff`, and only index 0 is ever read. A low byte of
         * 0x00 makes `& 0x00ff` and `& 0x00f0` agree, so mutant P061 escapes;
         * 0x33A5 has a non-zero low nibble and the two disagree. The remaining
         * words stay distinct so a wrong index is still visible. */
        g_inRead[i] = (Uint16)(0x33A5 + i);
    }

    /* The whole funcCode union, so that every index the module derives lands on
     * a distinct word and a wrong index is visible in sendFrame. */
    for (i = 0; i < FNUM_ALL; i++)
    {
        g_inFc.all[i] = (Uint16)(0x1000 + i);
    }

    g_inRcv.slaveAddr = 0xAAAA;
    g_inRcv.commAddr = 0xBBBB;
    g_inRcv.commData = 0xCCCC;
    g_inRcv.rcvNum = 0xDDDD;
    g_inRcv.rcvFlag = 0xEEEE;
    g_inRcv.crcRcv = 0xF0F0;
    g_inRcv.commCmd = 0x1234;
    g_inRcv.crcSize = 0x5678;
    g_inRcv.rcvNumMax = 24;
    g_inRcv.frameSpaceTime = 0x9ABC;
    g_inRcv.delay = 0xDEF0;
    g_inRcv.rcvCrcErrCounter = 0x1357;
    g_inRcv.rcvDataJuageFlag = 0x2468;
    g_inRcv.commCmdSaveEeprom = 0x369C;

    g_inSnd.sendNumMax = 0x1111;
    g_inSnd.sendNum = 0x2222;
    g_inSnd.commData = 0x3333;
    g_inSnd.crcRcv = 0x4444;
    g_inSnd.commCmd = 0x5555;
    g_inSnd.crcSize = 0x6666;

    g_inRunCmd = 0x7777;
    g_inDpCmd = 0x0F0F;
    g_inDpCmdBak = 0xF0F0;
    g_inRcvRigthFlag = 0x0A0A;
    g_inSaveEepromIndex = 0x0505;
    g_inSaveEepromFlag = 0x0303;

    g_inMaxFrq = 0x0BB8;
    g_inFrq = 0x00012345;
    g_inFrqAim = 0x0006789A;
    g_inFrqFdb = 0x0CCC;
    g_inFrqRunDisp = 0x0DDD;
    g_inErrorCode = 0x0007;
    g_inRunFlagAll = 0x0003;
    g_inMotorCtrlMode = 0x0000;      /* != FUNCCODE_motorCtrlMode_FVC */
}

/* ---- phase 0: funcCodeGradeSum[] == GetCodeIndex() ----------------------- */

/* grade_tables.inc holds the 83 member designators of funcCodeGradeSum[] exactly
 * once, as GRADE_SUM_LIST. mock_profibus.c builds the table from a sizeof-based
 * constant formulation; here the same list is expanded through the module's own
 * GetCodeIndex to produce run-time values, and the two are compared element by
 * element.
 *
 * It matters because the substitution is load-bearing: GetGradeIndex() reads
 * funcCodeGradeSum[] directly, and f_profibus computes both the write index
 * handed to f_menu.c and the read index into funcCode.all[] from it. If the
 * rebuilt table disagreed with GetCodeIndex, the module would be reading wrong
 * indices and every later phase would be comparing two sides that are both
 * wrong in the same way -- which the differential comparison cannot see. */
static void phase_grade_sum(void)
{
    Uint16 idx[FUNCCODE_GROUP_NUM];
    Uint16 i = 0;
    Uint16 bad = 0;

    /* Non-static on purpose: a static array would need an address-difference
     * initialiser, which MSVC rejects. */
#define GRADE_SUM_DRIVER_ENTRY(m)   idx[i++] = GetCodeIndex(funcCode.m);
    GRADE_SUM_LIST(GRADE_SUM_DRIVER_ENTRY)
#undef GRADE_SUM_DRIVER_ENTRY

    if (i != FUNCCODE_GROUP_NUM)
    {
        printf("FATAL: GRADE_SUM_LIST 展开了 %u 项, 期望 %u\n", i, FUNCCODE_GROUP_NUM);
        exit(2);
    }

    for (i = 0; i < FUNCCODE_GROUP_NUM; i++)
    {
        if (funcCodeGradeSum[i] != idx[i])
        {
            if (bad < 8)
            {
                printf("FATAL: funcCodeGradeSum[%u]=%u, GetCodeIndex 得 %u\n",
                       i, funcCodeGradeSum[i], idx[i]);
            }
            bad++;
        }
    }
    if (bad)
    {
        printf("FATAL: 等级表与 GetCodeIndex 有 %u 处不一致 —— "
               "mock 里的表不可信, 后续比对没有意义\n", bad);
        exit(2);
    }
    if (idx[FUNCCODE_GROUP_NUM - 1] >= FNUM_ALL)
    {
        printf("FATAL: 末项索引 %u 越出 funcCode (%u 字)\n",
               idx[FUNCCODE_GROUP_NUM - 1], FNUM_ALL);
        exit(2);
    }

    printf("  grade table: %u/%u 与 GetCodeIndex 一致, 末项索引 %u / %u\n",
           FUNCCODE_GROUP_NUM, FUNCCODE_GROUP_NUM,
           idx[FUNCCODE_GROUP_NUM - 1], FNUM_ALL);
}

/* ---- the funcCode.group.fe[] value set ----------------------------------- */

/* The module turns fe[i] into (fe[i] / 100, fe[i] % 100) and then into
 * funcCodeGradeSum[group] + grade. Nothing bounds fe: it is the user's PZD
 * mapping, copied in from EEPROM. So the driver builds the set of fe values that
 * stay inside both arrays, from the real table, and only ever installs those.
 *
 * The set deliberately includes each group's *last* valid grade as well as its
 * first, so that the boundary between two groups is exercised: an off-by-one in
 * a rewrite that recomputed the index would show up exactly there. */
#define FE_SET_N 1400

static Uint16 g_feSet[FE_SET_N];
static Uint16 g_feSetN;

static int fe_ok(Uint16 fe)
{
    Uint16 group = (Uint16)(fe / 100);
    Uint16 grade = (Uint16)(fe % 100);

    if (group >= FUNCCODE_GROUP_NUM)
    {
        return 0;
    }
    if ((Uint32)funcCodeGradeSum[group] + (Uint32)grade >= (Uint32)FNUM_ALL)
    {
        return 0;
    }
    return 1;
}

static void build_fe_set(void)
{
    static const Uint16 GRADE[] = { 0, 1, 2, 3, 4, 5, 9, 10, 20, 50, 99 };
    Uint16 g, k;

    for (g = 0; g < FUNCCODE_GROUP_NUM; g++)
    {
        Uint16 last;

        for (k = 0; k < (Uint16)(sizeof GRADE / sizeof GRADE[0]); k++)
        {
            Uint16 fe = (Uint16)((Uint32)g * 100 + GRADE[k]);
            if (g_feSetN < FE_SET_N && fe_ok(fe))
            {
                g_feSet[g_feSetN++] = fe;
            }
        }

        /* the group's last valid grade, when it is expressible as grade < 100 */
        if (g + 1 < FUNCCODE_GROUP_NUM)
        {
            last = (Uint16)(funcCodeGradeSum[g + 1] - funcCodeGradeSum[g]);
        }
        else
        {
            last = (Uint16)(FNUM_ALL - funcCodeGradeSum[g]);
        }
        if (last > 0 && last <= 100)
        {
            Uint16 fe = (Uint16)((Uint32)g * 100 + (last - 1));
            if (g_feSetN < FE_SET_N && fe_ok(fe))
            {
                g_feSet[g_feSetN++] = fe;
            }
        }
    }

    if (g_feSetN == 0)
    {
        printf("FATAL: 没有可用的 fe 值 —— 等级表或 FNUM_ALL 不对\n");
        exit(2);
    }
}

static Uint16 pick_fe(void)
{
    return g_feSet[rnd() % g_feSetN];
}

/* Fill all 32 fe entries. The module reads fe[i] and fe[i + 10] for
 * i < pzdNum <= 13, i.e. indices 0..22, so all 32 are filled and the upper ones
 * double as a guard against a rewrite that reads further. */
static void fill_fe(Uint16 (*pick)(void))
{
    Uint16 i;

    for (i = 0; i < FE_N; i++)
    {
        g_inFc.group.fe[i] = pick();
    }
}

/* ---- bounds checks ------------------------------------------------------- */

static Uint16 g_ppo3;

/* `pzdLoop` selects whether the frame-length rule for ProfibusSendDataDeal's PZD
 * loop applies. Only that function computes pzdNum; ProfibusRcvDataDeal reads
 * the same field but only backwards from it, so a short frame is perfectly legal
 * there -- and the 连通测试 path really does run with rcvNumMax = 5. */
static void precheck(const char *fn, int pzdLoop)
{
    /* The bound is taken from the module's own table, not written as 3, so that
     * "the sweep is inside the array" stays true if DPDataNum ever changes. */
    Uint16 protoN = (Uint16)(sizeof(ref_DPDataNum) / sizeof(ref_DPDataNum[0]));

    if (protoN != (Uint16)(sizeof(new_DPDataNum) / sizeof(new_DPDataNum[0])))
    {
        printf("FATAL %s: ref/new 的 DPDataNum 长度不同\n", fn);
        exit(2);
    }
    if (g_inProtocol >= protoN)
    {
        printf("FATAL %s: commProtocol=%u 越出 DPDataNum[%u]\n", fn, g_inProtocol, protoN);
        exit(2);
    }
    if (g_inRcv.rcvNumMax < 2 || g_inRcv.rcvNumMax > 36)
    {
        printf("FATAL %s: rcvNumMax=%u 不在固件可达集 [2,36] 内\n",
               fn, g_inRcv.rcvNumMax);
        exit(2);
    }
    /* Below 16 the subtraction in `pzdNum = (sendNumMax - 16) >> 1` wraps and
     * the PZD loop walks off sendFrame. Two routes legitimately reach the
     * function with a short frame and never get that far: PPO3 forces
     * pzdNum = 0 outright, and the 连通测试 path (DPRcvTest = 1) returns from
     * the top of the function before the loop is ever reached. Anything else is
     * a harness bug, not a firmware one. */
    if (pzdLoop && !g_ppo3 && !g_inDpRcvTest && g_inRcv.rcvNumMax < 16)
    {
        printf("FATAL %s: protocol=%u (非 PPO3) 但 rcvNumMax=%u < 16 —— "
               "pzdNum 会回绕\n", fn, g_inProtocol, g_inRcv.rcvNumMax);
        exit(2);
    }
    if (pzdLoop && !g_ppo3)
    {
        Uint32 pzd = ((Uint32)g_inRcv.rcvNumMax - 16) >> 1;
        if (14 + 2 * pzd > FRAME_N)
        {
            printf("FATAL %s: rcvNumMax=%u -> pzdNum=%lu, PZD 循环会写出 sendFrame[%lu]\n",
                   fn, g_inRcv.rcvNumMax, (unsigned long)pzd,
                   (unsigned long)(14 + 2 * pzd - 1));
            exit(2);
        }
    }
}

/* ---- phase A: ProfibusRcvDataDeal --------------------------------------- */

/* Five inputs decide the outcome: commProtocol (via the PPO3 test), DPRcvTest,
 * the function code in rcvFrame[2]>>4, the address/data pair, and rcvNumMax
 * (which the tail reads backwards from). commRcvData has 14 fields and at most 6
 * are assigned, so prefill_all() is what makes "left alone" observable. */
static void phase_rcvdeal(void)
{
    static const Uint16 CMD[] = { 0x00, 0x10, 0x20, 0xE0, 0xF0, 0x30, 0x90, 0xD0 };
    Uint16 p, t, c, k;

    for (p = 0; p < 4; p++)
    {
        for (t = 0; t < 2; t++)
        {
            for (c = 0; c < (Uint16)(sizeof CMD / sizeof CMD[0]); c++)
            {
                /* rcvNumMax: every value the firmware can reach, both sides of
                 * the 2-word CRC tail. */
                static const Uint16 NLEN[] = { 2, 3, 4, 5, 8, 16, 24, 35, 36 };
                Uint16 n;

                for (n = 0; n < (Uint16)(sizeof NLEN / sizeof NLEN[0]); n++)
                {
                    reset_input();
                    prefill_all();
                    g_inProtocol = p;
                    g_inDpRcvTest = t;
                    g_inRcvF[2] = CMD[c];
                    g_inRcv.rcvNumMax = NLEN[n];
                    g_ppo3 = (Uint16)(p == 2);
                    precheck("ProfibusRcvDataDeal", 0);
                    run_case("ProfibusRcvDataDeal", 0, 0);
                }
            }
        }
    }

    /* random frames, with the address/data pair swept as a unit */
    for (k = 0; k < (Uint16)(g_nrand / 10); k++)
    {
        reset_input();
        prefill_all();
        g_inProtocol = (Uint16)(rnd() % 4);
        g_inDpRcvTest = (Uint16)(rnd() & 1);
        g_inRcvF[2] = rnd();
        g_inRcvF[3] = rnd();
        g_inRcvF[4] = rnd();
        g_inRcvF[8] = rnd();
        g_inRcvF[9] = rnd();
        g_inRcv.rcvNumMax = (Uint16)(2 + rnd() % 35);
        g_ppo3 = (Uint16)(g_inProtocol == 2);
        precheck("ProfibusRcvDataDeal", 0);
        run_case("ProfibusRcvDataDeal", 0, 0);
    }
}

/* ---- phase B: ProfibusStartDeal ----------------------------------------- */

/* Six branches hang off one gate. The gate is commRcvData.rcvDataJuageFlag; when
 * it is set the function returns 1 without touching anything, so that flag is
 * swept as a first-class input rather than left at zero. tmp is swept over the
 * three magic header bytes and off them, and rcvRigthFlag over its whole
 * meaningful range plus one value outside it. */
static void phase_startdeal(void)
{
    static const Uint16 TMP[] = { 0x005A, 0x00AA, 0x0055, 0x0000, 0xFFFF, 0x0100 };
    static const Uint16 FLAG[] = { 0, 1, 2, 3, 4, 0xFFFF };
    Uint16 t, f, j, p, d, k;

    for (t = 0; t < (Uint16)(sizeof TMP / sizeof TMP[0]); t++)
    {
        for (f = 0; f < (Uint16)(sizeof FLAG / sizeof FLAG[0]); f++)
        {
            for (j = 0; j < 2; j++)
            {
                for (p = 0; p < 4; p++)
                {
                    for (d = 0; d < 2; d++)
                    {
                        reset_input();
                        prefill_all();
                        g_inProtocol = p;
                        g_inDpRcvTest = d;
                        g_inRcvRigthFlag = FLAG[f];
                        g_inRcv.rcvDataJuageFlag = j;
                        g_inRcv.rcvNumMax = 24;
                        g_ppo3 = (Uint16)(p == 2);
                        precheck("ProfibusStartDeal", 0);
                        run_case("ProfibusStartDeal", 1, TMP[t]);
                    }
                }
            }
        }
    }

    for (k = 0; k < (Uint16)(g_nrand / 10); k++)
    {
        reset_input();
        prefill_all();
        g_inProtocol = (Uint16)(rnd() % 4);
        g_inDpRcvTest = (Uint16)(rnd() & 1);
        g_inRcvRigthFlag = rnd();
        g_inRcv.rcvDataJuageFlag = (Uint16)(rnd() & 1);
        g_inRcv.rcvFlag = rnd();
        g_inRcv.rcvNum = rnd();
        g_inRcv.rcvNumMax = 24;
        g_ppo3 = (Uint16)(g_inProtocol == 2);
        precheck("ProfibusStartDeal", 0);
        run_case("ProfibusStartDeal", 1, rnd());
    }
}

/* ---- phase C: UpdateProfibusCommFormat, getDPControlCmd ------------------ */

/* baudRate is a parameter the function never reads. Sweeping it is what makes
 * "a rewrite starts using it" visible. */
static void phase_updateformat(void)
{
    static const Uint16 RATE[] = { 0, 1, 2, 9600, 19200, 38400, 0x7FFF, 0x8000, 0xFFFF };
    Uint16 p, d, r, k;

    for (p = 0; p < 4; p++)
    {
        for (d = 0; d < 2; d++)
        {
            for (r = 0; r < (Uint16)(sizeof RATE / sizeof RATE[0]); r++)
            {
                reset_input();
                prefill_all();
                g_inProtocol = p;
                g_inDpRcvTest = d;
                g_inRcv.rcvNumMax = 24;
                g_ppo3 = (Uint16)(p == 2);
                precheck("UpdateProfibusCommFormat", 0);
                run_case("UpdateProfibusCommFormat", 2, RATE[r]);
            }
        }
    }

    for (k = 0; k < (Uint16)(g_nrand / 20); k++)
    {
        reset_input();
        prefill_all();
        g_inProtocol = (Uint16)(rnd() % 4);
        g_inDpRcvTest = (Uint16)(rnd() & 1);
        g_inRcv.rcvNumMax = 24;
        g_ppo3 = (Uint16)(g_inProtocol == 2);
        precheck("UpdateProfibusCommFormat", 0);
        run_case("UpdateProfibusCommFormat", 2, rnd());
    }

    /* getDPControlCmd: the whole body is inside `#if 0`, so it must be an empty
     * function. Driving it with a non-trivial command latch is what proves the
     * dead body stayed dead -- and that a rewrite did not resurrect it. */
    for (k = 0; k < 8; k++)
    {
        reset_input();
        prefill_all();
        g_inProtocol = 1;
        g_inDpCmd = (Uint16)(k == 0 ? 0x0000 : (k == 1 ? 0x0001 : (k == 2 ? 0x0002 : rnd())));
        g_inDpCmdBak = (Uint16)(k & 1 ? 0x0001 : 0x0000);
        g_inRunCmd = 0x7777;
        g_inRcv.rcvNumMax = 24;
        g_ppo3 = 0;
        precheck("getDPControlCmd", 0);
        run_case("getDPControlCmd", 5, 0);
    }
}

/* ---- phase D: setDPStatusInfo ------------------------------------------- */

/* Four inputs: runFlag.run, runFlag.dir, frq == frqAim, errorCode. frq and
 * frqAim are int32, so the equality is a full 32-bit comparison -- the sweep
 * includes values that differ only in the high word, and negative values. */
static void phase_setstatus(void)
{
    static const int32 FRQ[] = { 0, 1, -1, 0x7FFF, 0x8000, 0xFFFF, 0x10000,
                                 -0x10000, 0x7FFFFFFF, (int32)0x80000000 };
    Uint16 a, b, e;
    Uint16 i, j;
    /* Uint32 on purpose. `for (k = 0; k <= 0xFFFF; k += 257)` with a 16-bit k is
     * an infinite loop: 0xFFFF is the largest value the type can hold, so the
     * guard is a tautology and the counter just wraps and keeps going. It does
     * not fail loudly -- it burns CPU forever with no output. Every sweep over a
     * full 16-bit space in this file therefore counts in Uint32. */
    Uint32 k;

    for (i = 0; i < (Uint16)(sizeof FRQ / sizeof FRQ[0]); i++)
    {
        for (j = 0; j < (Uint16)(sizeof FRQ / sizeof FRQ[0]); j++)
        {
            for (a = 0; a < 2; a++)
            {
                for (b = 0; b < 2; b++)
                {
                    for (e = 0; e < 3; e++)
                    {
                        static const Uint16 ERR[] = { 0, 1, 0xFFFF };
                        /* Built through the real bitfield so the test does not
                         * depend on where run/dir sit inside the union. */
                        union RUN_FLAG rf;

                        reset_input();
                        prefill_all();
                        rf.all = 0;
                        rf.bit.run = a;
                        rf.bit.dir = b;
                        g_inRunFlagAll = rf.all;
                        g_inFrq = FRQ[i];
                        g_inFrqAim = FRQ[j];
                        g_inErrorCode = ERR[e];
                        g_inRcv.rcvNumMax = 24;
                        g_ppo3 = 0;
                        precheck("setDPStatusInfo", 0);
                        run_case("setDPStatusInfo", 6, 0);
                    }
                }
            }
        }
    }

    /* the whole 16-bit runFlag space with frq/frqAim equal, and again unequal */
    for (k = 0; k <= 0xFFFF; k += 257)
    {
        reset_input();
        prefill_all();
        g_inRunFlagAll = k;
        g_inFrq = 1234;
        g_inFrqAim = (k & 1) ? 1234 : 1235;
        g_inErrorCode = (Uint16)(k & 2);
        g_inRcv.rcvNumMax = 24;
        g_ppo3 = 0;
        precheck("setDPStatusInfo", 0);
        run_case("setDPStatusInfo", 6, 0);
    }

    for (k = 0; k < (Uint16)(g_nrand / 20); k++)
    {
        reset_input();
        prefill_all();
        g_inRunFlagAll = rnd();
        g_inFrq = (int32)rnd32();
        g_inFrqAim = (int32)rnd32();
        g_inErrorCode = rnd();
        g_inRcv.rcvNumMax = 24;
        g_ppo3 = 0;
        precheck("setDPStatusInfo", 0);
        run_case("setDPStatusInfo", 6, 0);
    }
}

/* ---- phase E: ProfibusSendDataDeal -------------------------------------- */

/* The big one: a four-way branch on (DPRcvTest / DPFuncCmd / err / sciFlag.read
 * / sciFlag.write), a nested EEPROM branch, a PZD loop whose trip count comes
 * from the frame length, and two calls out to f_menu.c.
 *
 * The four reachable frame lengths for a non-PPO3 protocol are 16/24/36
 * (PPO1/2/5), giving pzdNum = 0/4/10. PPO3 forces pzdNum = 0 by a different
 * route. All of them are swept, and precheck() refuses anything else. */
static void phase_senddeal(void)
{
    static const Uint16 ERR[] = { 0, 1, 2, 0x7F, 0x80, 0xFF, 0xFFFF };
    static const Uint16 LEN[] = { 5, 16, 24, 36 };
    Uint32 s;
    Uint16 e, p, d, k;
    long base = g_cases;

#define SUBP(name)  do { printf("    %-26s: %ld\n", (name), g_cases - base); \
                         base = g_cases; } while (0)

    /* (a) the whole sciFlag space, err = 0, PPO2. 65536 cases, every branch
     * combination of the three bits the function reads and the thirteen it
     * ignores. */
    for (s = 0; s <= 0xFFFFUL; s++)
    {
        reset_input();
        prefill_all();
        g_inSciAll = (Uint16)s;
        g_inProtocol = 1;
        g_inDpRcvTest = 0;
        g_inRcv.rcvNumMax = 24;
        g_inRcvF[2] = 0x20;                 /* DPFuncCmd = 2, write */
        g_ppo3 = 0;
        fill_fe(pick_fe);
        precheck("ProfibusSendDataDeal", 1);
        run_case("ProfibusSendDataDeal", 3, 0);
    }
    SUBP("(a) sciFlag sweep");

    /* (b) err swept against a sampled sciFlag: the err branch must win. */
    for (e = 0; e < (Uint16)(sizeof ERR / sizeof ERR[0]); e++)
    {
        for (s = 0; s <= 0xFFFFUL; s += 257)
        {
            reset_input();
            prefill_all();
            g_inSciAll = (Uint16)s;
            g_inProtocol = 1;
            g_inRcv.rcvNumMax = 24;
            g_inRcvF[2] = 0x20;
            g_ppo3 = 0;
            fill_fe(pick_fe);
            precheck("ProfibusSendDataDeal", 1);
            run_case("ProfibusSendDataDeal", 3, ERR[e]);
        }
    }
    SUBP("(b) err x sciFlag");

    /* (c) protocol x DPRcvTest x frame length x the function-code command, with
     * the flags pinned to each of the three interesting arms in turn.
     *
     * The frame length is not a free variable: UpdateProfibusCommFormat()
     * writes 5 when DPRcvTest is set (the 连通测试 path, which returns before
     * the PZD loop) and DPDataNum[commProtocol] + 4 = 16/24/8/36 otherwise. The
     * 8-word PPO3 frame never enters the PZD loop either -- it forces
     * pzdNum = 0 by a different route. So each (protocol, DPRcvTest) pair has
     * exactly one reachable length, and that is the one swept here. */
    for (p = 0; p < 4; p++)
    {
        for (d = 0; d < 2; d++)
        {
            Uint16 len = (Uint16)(d ? 5 : (Uint16)(ref_DPDataNum[p] + 4));
            static const Uint16 CMD[] = { 0x00, 0x10, 0x20, 0xE0, 0x70 };
            static const Uint16 FLAGS[] = { 0x0000, 0x0001, 0x0002, 0x0004, 0x0007 };
            Uint16 c, f;

            for (c = 0; c < (Uint16)(sizeof CMD / sizeof CMD[0]); c++)
            {
                for (f = 0; f < (Uint16)(sizeof FLAGS / sizeof FLAGS[0]); f++)
                {
                    reset_input();
                    prefill_all();
                    g_inProtocol = p;
                    g_inDpRcvTest = d;
                    g_inRcv.rcvNumMax = len;
                    g_inRcvF[2] = CMD[c];
                    g_inSciAll = FLAGS[f];
                    g_inSaveEepromFlag = 1;
                    g_inSaveEepromIndex = 0;
                    g_ppo3 = (Uint16)(p == 2);
                    fill_fe(pick_fe);
                    precheck("ProfibusSendDataDeal", 1);
                    run_case("ProfibusSendDataDeal", 3, 0);
                }
            }
        }
    }
    SUBP("(c) proto x len x cmd");

    /* (d) the EEPROM branch: saveEepromIndex vs the computed index, across the
     * A0/B0 group offsets and the plain path. This is the only place the module
     * calls SaveOneFuncCode(). */
    for (p = 0; p < 4; p++)
    {
        static const Uint16 ADDR[] = { 0x0000, 0xA001, 0xB002, 0xA0FF, 0xB0FF, 0xC003 };
        static const Uint16 SAVE[] = { 0, 1, 2, 17, 33, 50, 0xFFFF };
        Uint16 a, v;

        for (a = 0; a < (Uint16)(sizeof ADDR / sizeof ADDR[0]); a++)
        {
            for (v = 0; v < (Uint16)(sizeof SAVE / sizeof SAVE[0]); v++)
            {
                reset_input();
                prefill_all();
                g_inProtocol = p;
                g_inRcv.rcvNumMax = 24;
                g_inRcv.commAddr = ADDR[a];
                g_inRcvF[3] = (Uint16)(ADDR[a] >> 8);
                g_inRcvF[4] = (Uint16)(ADDR[a] & 0xFF);
                g_inRcvF[2] = 0x20;             /* write */
                g_inSciAll = 0x0002;            /* sciFlag.write */
                g_inSaveEepromFlag = 1;
                g_inSaveEepromIndex = SAVE[v];
                g_ppo3 = (Uint16)(p == 2);
                fill_fe(pick_fe);
                precheck("ProfibusSendDataDeal", 1);
                run_case("ProfibusSendDataDeal", 3, 0);
            }
        }
    }
    SUBP("(d) eeprom branch");

    /* (e) the PZD loop's data flow: ModifyFunccodeUpDown's answer decides
     * whether ModifyFunccodeEnter runs, and its side effect on *data decides
     * what value is forwarded. Both are swept, and the PZD slot values come from
     * the bounded fe set. */
    for (e = 0; e < 2; e++)
    {
        for (p = 0; p < 4; p++)
        {
            Uint16 rep;

            for (rep = 0; rep < 40; rep++)
            {
                reset_input();
                prefill_all();
                g_inProtocol = p;
                /* The reachable length again: 16/24/36 give pzdNum = 0/4/10, and
                 * PPO3's 8-word frame forces pzdNum = 0. */
                g_inRcv.rcvNumMax = (Uint16)(ref_DPDataNum[p] + 4);
                g_inRcvF[2] = 0x20;
                g_inSciAll = 0x0000;            /* neither read nor write: loop still runs */
                mockUpDownRc = (Uint16)(e ? COMM_ERR_NONE : 1);
                g_ppo3 = (Uint16)(p == 2);
                fill_fe(pick_fe);
                precheck("ProfibusSendDataDeal", 1);
                run_case("ProfibusSendDataDeal", 3, 0);
            }
        }
    }
    SUBP("(e) pzd data flow");

    /* (f) the latched target frequency and the PZD2 reply. maxFrq gates whether
     * dpFrqAim is updated at all (`if (frq <= maxFrq)`), and
     * motorFc.motorCtrlMode picks which feedback word is echoed back. Both are
     * swept around their boundaries, with the frame's frequency word placed at
     * the offset the non-PPO3 layout puts it in. */
    {
        static const Uint16 MAXF[] = { 0, 1, 0x7FFF, 0x8000, 0xFFFF };
        static const Uint16 FRMFRQ[] = { 0, 1, 0x7FFF, 0x8000, 0xFFFF };
        Uint16 m, q;

        for (m = 0; m < (Uint16)(sizeof MAXF / sizeof MAXF[0]); m++)
        {
            for (q = 0; q < (Uint16)(sizeof FRMFRQ / sizeof FRMFRQ[0]); q++)
            {
                Uint16 c;

                for (c = 0; c < 2; c++)
                {
                    reset_input();
                    prefill_all();
                    g_inProtocol = 1;
                    g_inRcv.rcvNumMax = 24;
                    g_inRcvF[2] = 0x00;
                    g_inSciAll = 0x0000;
                    g_inMaxFrq = MAXF[m];
                    g_inRcvF[12] = (Uint16)(FRMFRQ[q] >> 8);
                    g_inRcvF[13] = (Uint16)(FRMFRQ[q] & 0xFF);
                    g_inMotorCtrlMode = (Uint16)(c ? FUNCCODE_motorCtrlMode_FVC : 0);
                    g_inFrqFdb = 0x1234;
                    g_inFrqRunDisp = 0x5678;
                    g_ppo3 = 0;
                    fill_fe(pick_fe);
                    precheck("ProfibusSendDataDeal", 1);
                    run_case("ProfibusSendDataDeal", 3, 0);
                }
            }
        }
    }
    SUBP("(f) maxFrq / PZD2");

    /* (g) random over everything, all bounds respected. */
    for (k = 0; k < (Uint16)(g_nrand / 2); k++)
    {
        Uint16 i;

        reset_input();
        prefill_all();
        g_inProtocol = (Uint16)(rnd() % 4);
        g_inDpRcvTest = (Uint16)(rnd() & 1);
        g_inRcv.rcvNumMax = (Uint16)(g_inProtocol == 2 ? LEN[rnd() % 4] : LEN[1 + rnd() % 3]);
        g_inRcv.commAddr = rnd();
        g_inSciAll = rnd();
        g_inSaveEepromFlag = (Uint16)(rnd() & 1);
        g_inSaveEepromIndex = rnd();
        g_inFc.code.commSlaveAddress = rnd();
        g_inMaxFrq = rnd();
        g_inMotorCtrlMode = (Uint16)(rnd() & 1 ? FUNCCODE_motorCtrlMode_FVC : rnd());
        g_inFrqFdb = rnd();
        g_inFrqRunDisp = rnd();
        g_inRunFlagAll = rnd();
        g_inErrorCode = rnd();
        g_inFrq = (int32)rnd32();
        g_inFrqAim = (int32)rnd32();
        mockUpDownRc = (Uint16)(rnd() & 1 ? COMM_ERR_NONE : rnd());
        for (i = 0; i < 10; i++)
        {
            g_inRcvF[i] = rnd();
        }
        g_inRcvF[2] = (Uint16)((rnd() & 0x0F) << 4);
        g_ppo3 = (Uint16)(g_inProtocol == 2);
        fill_fe(pick_fe);
        precheck("ProfibusSendDataDeal", 1);
        run_case("ProfibusSendDataDeal", 3, rnd());
    }
    SUBP("(g) random");

#undef SUBP
}

/* ---- phase F: ProfibusCommErrCheck -------------------------------------- */

/* Two independent trips: the commOverTime timeout (a 32-bit multiply) and the
 * CRC error counter against a fixed threshold of 10. The timeout threshold is
 * commOverTime * 200, so the interesting ticker values are the threshold and its
 * neighbours, for every commOverTime the Uint16 can hold. */
static void phase_errcheck(void)
{
    Uint32 t;
    Uint16 k, c;

    for (t = 0; t <= 0xFFFFUL; t += 7)
    {
        Uint32 edge = (Uint32)t * 2 * TIME_UNIT_sciCommOverTime;
        Uint16 j;

        for (j = 0; j < 3; j++)
        {
            reset_input();
            prefill_all();
            g_inFc.code.commOverTime = (Uint16)t;
            g_inTicker = (j == 0) ? (edge - (edge ? 1 : 0)) : (edge + (j - 1));
            g_inRcv.rcvCrcErrCounter = 0;
            g_inRcv.rcvNumMax = 24;
            g_ppo3 = 0;
            precheck("ProfibusCommErrCheck", 0);
            run_case("ProfibusCommErrCheck", 4, 0);
        }
    }

    /* the CRC counter around its threshold, with commOverTime held at 0 so the
     * first trip cannot fire */
    for (c = 0; c <= 20; c++)
    {
        reset_input();
        prefill_all();
        g_inFc.code.commOverTime = 0;
        g_inTicker = rnd32();
        g_inRcv.rcvCrcErrCounter = c;
        g_inRcv.rcvNumMax = 24;
        g_ppo3 = 0;
        precheck("ProfibusCommErrCheck", 0);
        run_case("ProfibusCommErrCheck", 4, 0);
    }

    for (k = 0; k < (Uint16)(g_nrand / 10); k++)
    {
        reset_input();
        prefill_all();
        g_inFc.code.commOverTime = rnd();
        g_inTicker = rnd32();
        g_inRcv.rcvCrcErrCounter = rnd();
        g_inRcv.rcvNumMax = 24;
        g_ppo3 = 0;
        precheck("ProfibusCommErrCheck", 0);
        run_case("ProfibusCommErrCheck", 4, 0);
    }
}

/* ---- phase G: chained frames -------------------------------------------- */

/* A single frame split across the three entry points, in the order f_comm.c
 * calls them. This is where the state the module carries between calls matters:
 * ProfibusStartDeal sets rcvNumMax and DPRcvTest, ProfibusRcvDataDeal consumes
 * them and fills commRcvData, ProfibusSendDataDeal builds the reply from both.
 *
 * The frame length is not installed by the driver here -- it is whatever
 * ProfibusStartDeal computed, which is the point: this is the only phase where
 * the module's own value flows from one call into the next. */
static void phase_chain(void)
{
    static const Uint16 CMD[] = { 0x00, 0x10, 0x20, 0xE0 };
    Uint16 p, d, c, k;

    for (p = 0; p < 4; p++)
    {
        for (d = 0; d < 2; d++)
        {
            for (c = 0; c < (Uint16)(sizeof CMD / sizeof CMD[0]); c++)
            {
                Uint16 i;

                for (i = 0; i < 6; i++)
                {
                    reset_input();
                    prefill_all();
                    g_inProtocol = p;
                    g_inDpRcvTest = d;
                    g_inRcv.rcvNumMax = (Uint16)((p == 2) ? 24 : 16);   /* start length */
                    g_inRcv.rcvDataJuageFlag = 0;
                    g_inRcvRigthFlag = 0;
                    g_inRcvF[2] = CMD[c];
                    g_inRcvF[3] = (Uint16)(0xA0 + i);
                    g_inRcvF[4] = (Uint16)(i * 3);
                    g_inFc.code.commSlaveAddress = (Uint16)(i + 1);
                    g_inSciAll = (Uint16)(i & 7);
                    g_inSaveEepromFlag = (Uint16)(i & 1);
                    g_ppo3 = (Uint16)(p == 2);
                    fill_fe(pick_fe);
                    precheck("chain(normal)", 0);
                    run_case("chain(normal)", 7, 0);
                }
            }
        }
    }

    /* the 连通测试 frame: three header words, then the short reply */
    for (p = 0; p < 4; p++)
    {
        for (d = 0; d < 2; d++)
        {
            Uint16 i;

            for (i = 0; i < 8; i++)
            {
                reset_input();
                prefill_all();
                g_inProtocol = p;
                g_inDpRcvTest = d;
                g_inRcv.rcvNumMax = 24;
                g_inRcv.rcvDataJuageFlag = 0;
                g_inRcvRigthFlag = 0;
                g_inRcvF[2] = (Uint16)(i * 0x11);
                g_inFc.code.commSlaveAddress = (Uint16)(i + 1);
                g_ppo3 = (Uint16)(p == 2);
                fill_fe(pick_fe);
                precheck("chain(test)", 0);
                run_case("chain(test)", 8, 0);
            }
        }
    }

    for (k = 0; k < (Uint16)(g_nrand / 20); k++)
    {
        Uint16 i;

        reset_input();
        prefill_all();
        g_inProtocol = (Uint16)(rnd() % 4);
        g_inDpRcvTest = (Uint16)(rnd() & 1);
        g_inRcv.rcvNumMax = (Uint16)((g_inProtocol == 2) ? 24 : 16);
        g_inRcv.rcvDataJuageFlag = 0;
        g_inRcvRigthFlag = (Uint16)(rnd() % 5);
        g_inSciAll = rnd();
        for (i = 0; i < 10; i++)
        {
            g_inRcvF[i] = rnd();
        }
        g_inFc.code.commSlaveAddress = rnd();
        g_ppo3 = (Uint16)(g_inProtocol == 2);
        fill_fe(pick_fe);
        precheck("chain(random)", 0);
        run_case("chain(random)", (rnd() & 1) ? 7 : 8, rnd());
    }
}

/* ---- main ---------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long base;

    if (argc > 1)
    {
        g_nrand = atol(argv[1]);
        if (g_nrand < 100)
        {
            g_nrand = 100;
        }
    }

    /* Unbuffered: this driver runs six-figure case counts, and a redirected
     * stdout is block-buffered, so without this a run that aborts half way
     * leaves an empty log and no way to tell where it stopped. */
    setvbuf(stdout, NULL, _IONBF, 0);

    init_ptrs();

    phase_grade_sum();
    build_fe_set();
    printf("  fe set       : %u 个可用值 (group < %u, index < %u)\n",
           g_feSetN, FUNCCODE_GROUP_NUM, FNUM_ALL);

    base = g_cases;
    phase_rcvdeal();
    printf("  ProfibusRcvDataDeal          : %ld\n", g_cases - base);

    base = g_cases;
    phase_startdeal();
    printf("  ProfibusStartDeal            : %ld\n", g_cases - base);

    base = g_cases;
    phase_updateformat();
    printf("  UpdateProfibusCommFormat     : %ld\n", g_cases - base);

    base = g_cases;
    phase_setstatus();
    printf("  setDPStatusInfo              : %ld\n", g_cases - base);

    base = g_cases;
    phase_senddeal();
    printf("  ProfibusSendDataDeal         : %ld\n", g_cases - base);

    base = g_cases;
    phase_errcheck();
    printf("  ProfibusCommErrCheck         : %ld\n", g_cases - base);

    base = g_cases;
    phase_chain();
    printf("  chained frames               : %ld\n", g_cases - base);

    printf("f_profibus difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    return g_fails ? 1 : 0;
}
