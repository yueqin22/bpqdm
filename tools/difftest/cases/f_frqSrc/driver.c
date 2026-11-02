/* Differential test driver for f_frqSrc.
 *
 * The module is the frequency-reference chain of the drive: it decides what
 * frequency the drive should run at, from whichever source the function-code
 * table selects, and it carries several pieces of state between calls.
 *
 * Public entry points, in the order the rest of the system uses them:
 *
 *   FrqSrcDeal()        the periodic task. Calls UpdateLimitFrq, UpdateRunMode,
 *                       FrqSrcOperate and JumpFrqDeal, in that order, then
 *                       clamps and publishes frqAimTmp.
 *   UpdateFrqAim()      turn frqAimTmp into frqAim, resolve direction, apply the
 *                       anti-reverse-run rule and the reverse-runDir flip.
 *   FrqSrcOperate()     choose between the bound run command and GetFrq().
 *   GetFrq()            X/Y source selection, composition and clamping.
 *   UpdateRunMode()     speed vs torque mode, plus the TorqueCalc hook.
 *   FrqPlcSetDeal()     the multi-speed (PLC) step machine.
 *   FrqAimUpDownDeal()  the panel up/down key integrator.
 *   JumpFrqDeal()       the jump-frequency (skip) bands.
 *   getUpDownLimit(), ResetUpDownFrq(), UpdateLimitFrq(), UpdateMultiSetFrq(),
 *   FrqXyCalc(), GetFrqX(), GetFrqY(), GetFrqComp(), JumpFrqEndCalc().
 *
 * Nothing here touches a peripheral register. The module's whole hardware
 * boundary is three functions it calls, which the shared mock implements:
 * FrqPidSetDeal (f_frqSrc_pid.c), GetNumberDigit1 (f_menu.c) and TorqueCalc
 * (f_torqueCtrl.c). See mock_frqSrc.c.
 *
 * ---- what is compared ----
 *
 * After every call the driver compares, field by field:
 *
 *   * all 46 scalar module globals, via the FIELDS() list below;
 *   * the four aggregate globals (frqXy, frqFlag, jumpFrq1, jumpFrq2) by
 *     memcmp -- they have no != operator;
 *   * the shared globals the module *writes* (frqAimDisp, frqXDisp, frqYDisp,
 *     frqAimPLCDisp, bFrqDigitalDone4WaitDelay, accDecTimeSrcPlc), snapshotted
 *     per side because both sides write the same object;
 *   * the mock call trace (which stub, which argument, in which order).
 *
 * ---- state the module carries, and how the driver reaches it ----
 *
 * File-scope state (46 scalars + 4 aggregates) is per-side and therefore both
 * an input and an output: the driver installs it before the call and reads it
 * back after. That is deliberate -- a rewrite that stops updating one of them
 * shows up immediately.
 *
 * accDecFrqPrcFlag lives in f_runSrc.c, not here, but this module both reads
 * and writes it, so it is installed per side as well.
 *
 * Function-local statics (frqAimTmpBak in FrqSrcDeal, bFrqDigitalOld and
 * frqDeltaDiRemainder in FrqAimUpDownDeal, four tickers in FrqPlcSetDeal,
 * stepOld in UpdateMultiSetFrq, runModeOld in UpdateRunMode) are NOT reachable
 * from the driver. They are not a hole in the test: both sides run the *same*
 * call sequence from the same start state (zero, except bPlcEndOneLoopEnable=1),
 * so their statics evolve identically as long as the two implementations are
 * equivalent. What the driver must never do is give the two sides different
 * call sequences -- run_case() below is written so that it cannot.
 *
 * ---- pre-existing gaps that are NOT tested ----
 *
 * Listed rather than quietly dropped. None is reachable from a valid
 * function-code table, and "fixing" one would be a behaviour change, not a
 * rewrite:
 *
 *   a. `FrqAimUpDownDeal` has `int32 frqDeltaDi;` assigned only inside
 *      `if ((up || down) && ...)` but read unconditionally as `delta`. On the
 *      path where bFrqDigital is 1 and neither `up` nor `down` is set and
 *      accDecFrqPrcFlag is not ACC_DEC_FRQ_WAIT, `delta` is indeterminate.
 *      The driver keeps `delta` deterministic by only entering the phase with
 *      up/down/accDecFrqPrcFlag combinations that set it, and by seeding the
 *      static remainder to 0 first. See phase_updown().
 *   b. Division by maxFrq in the multi-set / comm scaling. maxFrq is the drive
 *      maximum and is never 0; the driver keeps it >= 1.
 *   c. `(int32)X * maxFrq` is signed 32-bit arithmetic. The driver bounds the
 *      factors so every product stays inside int32.
 *   d. `funcCode.code.frqPoint` indexes decNumber[5]; the driver keeps it 0..4.
 *   e. `plcStep` indexes plcAttribute[PLC_STEP_MAX]; the driver keeps it in range.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Same include order as src/function/f_frqSrc.c. */
#include "f_frqSrc.h"
#include "f_main.h"
#include "f_runSrc.h"
#include "f_io.h"
#include "f_menu.h"
#include "f_ui.h"
#include "f_p2p.h"
#include "f_comm.h"

#include "mock_frqSrc.h"
#include "frqsrc_sides.h"

/* ---------------------------------------------------------------------------
 * The two sides
 * ------------------------------------------------------------------------ */

#define PS(n)   (side ? &new_##n : &ref_##n)

/* Scalar module globals.  (type, name) -- order is the comparison order. */
#define FIELDS(X)                 \
    X(int32,  upDownFrq)          \
    X(Uint16, upDownFrqInit)      \
    X(int32,  upDownFrqTmp)       \
    X(int32,  uPDownFrqMax)       \
    X(int32,  uPDownFrqMin)       \
    X(int32,  uPDownFrqLower1)    \
    X(int32,  uPDownFrqLower2)    \
    X(Uint16, plcStep)            \
    X(Uint32, plcTime)            \
    X(Uint16, bPlcEndOneLoop)     \
    X(Uint16, plcStepRemOld)      \
    X(Uint32, plcTimeRemOld)      \
    X(int32,  frq)                \
    X(int32,  frqTmp)             \
    X(int32,  frqFrac)            \
    X(int32,  frqTmpFrac)         \
    X(int32,  frqDroop)           \
    X(int32,  frqCurAimFrac)      \
    X(int32,  frqAim)             \
    X(int32,  frqAimTmp)          \
    X(int32,  frqAimTmp0)         \
    X(int32,  frqCurAim)          \
    X(Uint16, timeBench)          \
    X(int32,  frqDigitalTmp)      \
    X(int32,  frqDigitalPlc)      \
    X(Uint16, presetFrqOld)       \
    X(Uint16, upperFrq)           \
    X(Uint16, lowerFrq)           \
    X(Uint16, maxFrq)             \
    X(Uint16, benchFrq)           \
    X(int32,  frqAiPu)            \
    X(Uint16, frqPuQ15)           \
    X(Uint16, frqCalcSrc)         \
    X(Uint16, frqCalcSrcOld)      \
    X(Uint16, plcStepOld)         \
    X(Uint16, bStopPlc)           \
    X(Uint16, bFrqDigital)        \
    X(Uint16, frqKeyUpDownDelta)  \
    X(Uint32, reverseTicker)      \
    X(Uint16, bAntiReverseRun)    \
    X(Uint16, runMode)            \
    X(int32,  frqAimOld4Dir)      \
    X(Uint16, runCmdBind)         \
    X(Uint16, frqPlcType)         \
    X(int32,  frqMax)             \
    X(int32,  frqMin)

struct ST
{
#define DECL(t, n) t n;
    FIELDS(DECL)
#undef DECL
    struct FRQ_XY  frqXy;
    union FRQ_FLAG frqFlag;
    JUMP_FRQ       jumpFrq1;
    JUMP_FRQ       jumpFrq2;
};

static void capture(int side, struct ST *s)
{
#define CAP(t, n) s->n = *PS(n);
    FIELDS(CAP)
#undef CAP
    s->frqXy    = *PS(frqXy);
    s->frqFlag  = *PS(frqFlag);
    s->jumpFrq1 = *PS(jumpFrq1);
    s->jumpFrq2 = *PS(jumpFrq2);
}

static void install(int side, const struct ST *s)
{
#define INS(t, n) *PS(n) = s->n;
    FIELDS(INS)
#undef INS
    *PS(frqXy)    = s->frqXy;
    *PS(frqFlag)  = s->frqFlag;
    *PS(jumpFrq1) = s->jumpFrq1;
    *PS(jumpFrq2) = s->jumpFrq2;
}

/* ---------------------------------------------------------------------------
 * Shared globals the module writes
 * ------------------------------------------------------------------------ */

struct SH
{
    Uint16 frqAimDisp;
    Uint16 frqXDisp;
    Uint16 frqYDisp;
    Uint16 frqAimPLCDisp;
    Uint16 bFrqDigitalDone4WaitDelay;
    Uint16 accDecTimeSrcPlc;

    /* Observed, but deliberately NOT restored by sh_write().
     *
     * The module writes runFlag.bit.plc / .pid / .torque / .dir / .dirFinal /
     * .curDir and accDecFrqPrcFlag, and neither is a module global, so neither
     * appears in FIELDS above.  Leaving them unobserved is what let the first
     * negative-control sweep report five mutations as MISSED (M02, M03, M04,
     * M35, M60 all write runFlag; M10 writes accDecFrqPrcFlag).
     *
     * Their pre-call value is established by apply_inputs() (which memsets
     * runFlag and then sets the five input bits run/jog/common/dir/curDir) and
     * by the dedicated accDecFrqPrcFlag input install.  That is a deterministic,
     * symmetric starting state for both sides, so there is nothing for sh_write
     * to restore -- and restoring it would fight the input install.  Hence the
     * asymmetry: sh_read() fills these two, sh_write() ignores them. */
    Uint16 runFlag;
    Uint16 accDecFrqPrcFlag;
};

static void sh_read(struct SH *s)
{
    s->frqAimDisp                 = frqAimDisp;
    s->frqXDisp                   = frqXDisp;
    s->frqYDisp                   = frqYDisp;
    s->frqAimPLCDisp              = frqAimPLCDisp;
    s->bFrqDigitalDone4WaitDelay  = bFrqDigitalDone4WaitDelay;
    s->accDecTimeSrcPlc           = accDecTimeSrcPlc;
    s->runFlag                    = runFlag.all;
    s->accDecFrqPrcFlag           = accDecFrqPrcFlag;
}

static void sh_write(const struct SH *s)
{
    frqAimDisp                = s->frqAimDisp;
    frqXDisp                  = s->frqXDisp;
    frqYDisp                  = s->frqYDisp;
    frqAimPLCDisp             = s->frqAimPLCDisp;
    bFrqDigitalDone4WaitDelay = s->bFrqDigitalDone4WaitDelay;
    accDecTimeSrcPlc          = s->accDecTimeSrcPlc;
}

/* ---------------------------------------------------------------------------
 * Inputs
 * ------------------------------------------------------------------------ */

struct IN
{
    /* carried state installed before each call */
    struct ST st;
    struct SH sh0;
    Uint16    accDecFrqPrcFlag;

    /* shared inputs -- function code */
    Uint16 frqCalcSrc, frqXSrc, frqYSrc, frqYOffsetFc, frqYRange, frqYRangeBase;
    Uint16 upperFrqSrc, upperFrq, upperFrqOffset, lowerFrq, maxFrq, frqPoint;
    Uint16 frqComm, presetFrq, frqRunCmdBind, diUpDownSlope, updnBenchmark;
    Uint16 antiReverseRun, runDir, zeroSpeedDeadTime, errorRunFrqSrc;
    Uint16 torqueCtrl, torqueCtrlFwdMaxFrq, torqueCtrlRevMaxFrq;
    Uint16 jumpFrq1, jumpFrq2, jumpFrqRange, jumpFrqMode;
    Uint16 commProtocolSec, p2pRevDataSel, plcEnable, plcFrqSet;
    Uint16 plcRunMode, plcTimeUnit, plcPowerOffRemMode;
    Uint16 plcFrq0Src, plcFrq[PLC_STEP_MAX], plcRunTime[PLC_STEP_MAX];
    Uint16 plcAccDecTime[PLC_STEP_MAX];

    /* shared inputs -- other modules */
    Uint16 motorCtrlMode;
    int16  aiSet[AI_NUMBER];
    int16  pulseInSet;
    /* 32-bit, NOT 16: `union DI_FUNC1` / `union DI_FUNC2` have a `Uint32 all`,
     * and DI_FUNC1_BITS runs to bit 27 (clearUpDownFrq is bit 19, resetPLC 23).
     * These were Uint16, so `(Uint16)((i & 1) << 19)` truncated to 0 and
     * diFunc.f1.bit.clearUpDownFrq could never be set -- which is why mutation
     * M57 (upDownFrq = 0 -> 1 in that branch) was MISSED. */
    Uint32 diF1, diF2, diSelF2;
    Uint16 runBitRun, runBitJog, runBitCommon, runBitDir, runBitCurDir;
    Uint16 cmdDir, cmdJog, cmdPause, cmdCommon;
    Uint16 runDirPanelOld;
    Uint16 runStatus, startRunStatus, swingStatus, menu0DispStatus;
    Uint16 keyFunc, runSrc, errorLevel;
    int32  frqRun;
    Uint16 dpFrqAim, CanRxTxCon;
    int16  p2pProcessValue;
    int16  torqueAim;

    /* mock driving values */
    int32  mockPidFrq;
    Uint16 mockDigit[5];
    int16  mockTorqueAim;
};

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

static void apply_inputs(const struct IN *in)
{
    int i;

    memset(&funcCode, 0, sizeof(funcCode));
    funcCode.code.frqCalcSrc        = in->frqCalcSrc;
    funcCode.code.frqXSrc           = in->frqXSrc;
    funcCode.code.frqYSrc           = in->frqYSrc;
    funcCode.code.frqYOffsetFc      = in->frqYOffsetFc;
    funcCode.code.frqYRange         = in->frqYRange;
    funcCode.code.frqYRangeBase     = in->frqYRangeBase;
    funcCode.code.upperFrqSrc       = in->upperFrqSrc;
    funcCode.code.upperFrq          = in->upperFrq;
    funcCode.code.upperFrqOffset    = in->upperFrqOffset;
    funcCode.code.lowerFrq          = in->lowerFrq;
    funcCode.code.maxFrq            = in->maxFrq;
    funcCode.code.frqPoint          = in->frqPoint;
    funcCode.code.frqComm           = in->frqComm;
    funcCode.code.presetFrq         = in->presetFrq;
    funcCode.code.frqRunCmdBind     = in->frqRunCmdBind;
    funcCode.code.diUpDownSlope     = in->diUpDownSlope;
    funcCode.code.updnBenchmark     = in->updnBenchmark;
    funcCode.code.antiReverseRun    = in->antiReverseRun;
    funcCode.code.runDir            = in->runDir;
    funcCode.code.zeroSpeedDeadTime = in->zeroSpeedDeadTime;
    funcCode.code.errorRunFrqSrc    = in->errorRunFrqSrc;
    funcCode.code.torqueCtrl        = in->torqueCtrl;
    funcCode.code.torqueCtrlFwdMaxFrq = in->torqueCtrlFwdMaxFrq;
    funcCode.code.torqueCtrlRevMaxFrq = in->torqueCtrlRevMaxFrq;
    funcCode.code.jumpFrq1          = in->jumpFrq1;
    funcCode.code.jumpFrq2          = in->jumpFrq2;
    funcCode.code.jumpFrqRange      = in->jumpFrqRange;
    funcCode.code.jumpFrqMode       = in->jumpFrqMode;
    funcCode.code.commProtocolSec   = in->commProtocolSec;
    funcCode.code.p2pRevDataSel     = in->p2pRevDataSel;
    funcCode.code.plcEnable         = in->plcEnable;
    funcCode.code.plcFrqSet         = in->plcFrqSet;
    funcCode.code.plcRunMode        = in->plcRunMode;
    funcCode.code.plcTimeUnit       = in->plcTimeUnit;
    funcCode.code.plcPowerOffRemMode = in->plcPowerOffRemMode;
    funcCode.code.plcFrq0Src        = in->plcFrq0Src;
    for (i = 0; i < PLC_STEP_MAX; i++)
    {
        funcCode.code.plcFrq[i]                   = in->plcFrq[i];
        funcCode.code.plcAttribute[i].runTime     = in->plcRunTime[i];
        funcCode.code.plcAttribute[i].accDecTimeSet = in->plcAccDecTime[i];
    }

    motorFc.motorCtrlMode = in->motorCtrlMode;

    for (i = 0; i < AI_NUMBER; i++)
    {
        aiDeal[i].set = in->aiSet[i];
    }
    pulseInSet = in->pulseInSet;

    memset(&diFunc, 0, sizeof(diFunc));
    memset(&diSelectFunc, 0, sizeof(diSelectFunc));
    diFunc.f1.all       = in->diF1;
    diFunc.f2.all       = in->diF2;
    diSelectFunc.f2.all = in->diSelF2;

    memset(&runFlag, 0, sizeof(runFlag));
    memset(&runCmd, 0, sizeof(runCmd));
    runFlag.bit.run        = in->runBitRun;
    runFlag.bit.jog        = in->runBitJog;
    runFlag.bit.common     = in->runBitCommon;
    runFlag.bit.dir        = in->runBitDir;
    runFlag.bit.curDir     = in->runBitCurDir;
    runCmd.bit.dir         = in->cmdDir;
    runCmd.bit.jog         = in->cmdJog;
    runCmd.bit.pause       = in->cmdPause;
    runCmd.bit.common      = in->cmdCommon;

    runDirPanelOld = in->runDirPanelOld;
    runStatus      = (enum RUN_STATUS) in->runStatus;
    startRunStatus = (enum START_RUN_STATUS) in->startRunStatus;
    swingStatus    = (enum SWING_STATUS) in->swingStatus;
    menu0DispStatus = (enum MENU0_DISP_STATUS) in->menu0DispStatus;
    keyFunc        = in->keyFunc;
    runSrc         = in->runSrc;
    errorAttribute.all = in->errorLevel;

    frqRun  = in->frqRun;
    dpFrqAim = in->dpFrqAim;
    CanRxTxCon = in->CanRxTxCon;
    p2pData.processValue = in->p2pProcessValue;
    torqueAim = in->torqueAim;

    mockPidFrq   = in->mockPidFrq;
    mockTorqueAim = in->mockTorqueAim;
    for (i = 0; i < 5; i++)
    {
        mockDigit[i] = in->mockDigit[i];
    }
}

/* ---------------------------------------------------------------------------
 * Running one case
 * ------------------------------------------------------------------------ */

#define ENTRY_DEAL      0
#define ENTRY_UPD_AIM   1
#define ENTRY_OPERATE   2
#define ENTRY_GETFRQ    3
#define ENTRY_GETFRQX   4
#define ENTRY_GETFRQY   5
#define ENTRY_GETFRQC   6
#define ENTRY_UPDMODE   7
#define ENTRY_UPDLIMIT  8
#define ENTRY_RESET     9
#define ENTRY_GETUPDOWN 10
#define ENTRY_PLC       11
#define ENTRY_MULTISET  12
#define ENTRY_JUMPDEAL  13
#define ENTRY_JUMPEND   14
#define ENTRY_XYCALC    15
#define ENTRY_UPDOWN    16

static int32 invoke(int side, int entry, int a, int b, JUMP_FRQ *p)
{
    switch (entry)
    {
        case ENTRY_DEAL:     if (side) new_FrqSrcDeal();     else ref_FrqSrcDeal();     return 0;
        case ENTRY_UPD_AIM:  if (side) new_UpdateFrqAim();   else ref_UpdateFrqAim();   return 0;
        case ENTRY_OPERATE:  return side ? new_FrqSrcOperate() : ref_FrqSrcOperate();
        case ENTRY_GETFRQ:   return side ? new_GetFrq()        : ref_GetFrq();
        case ENTRY_GETFRQX:  return side ? new_GetFrqX()       : ref_GetFrqX();
        case ENTRY_GETFRQY:  return side ? new_GetFrqY()       : ref_GetFrqY();
        case ENTRY_GETFRQC:  return side ? new_GetFrqComp()    : ref_GetFrqComp();
        case ENTRY_UPDMODE:  if (side) new_UpdateRunMode();  else ref_UpdateRunMode();  return 0;
        case ENTRY_UPDLIMIT: if (side) new_UpdateLimitFrq(); else ref_UpdateLimitFrq(); return 0;
        case ENTRY_RESET:    if (side) new_ResetUpDownFrq(); else ref_ResetUpDownFrq(); return 0;
        case ENTRY_GETUPDOWN: if (side) new_getUpDownLimit(a, b); else ref_getUpDownLimit(a, b); return 0;
        case ENTRY_PLC:      return side ? new_FrqPlcSetDeal() : ref_FrqPlcSetDeal();
        case ENTRY_MULTISET: return side ? new_UpdateMultiSetFrq(a) : ref_UpdateMultiSetFrq(a);
        case ENTRY_JUMPDEAL: if (side) new_JumpFrqDeal();    else ref_JumpFrqDeal();    return 0;
        case ENTRY_JUMPEND:  if (side) new_JumpFrqEndCalc(p); else ref_JumpFrqEndCalc(p); return 0;
        case ENTRY_XYCALC:   return side ? new_FrqXyCalc(a) : ref_FrqXyCalc(a);
        case ENTRY_UPDOWN:   return side ? new_FrqAimUpDownDeal() : ref_FrqAimUpDownDeal();
        default:             return 0;
    }
}

static void run_case(const struct IN *in, int entry, int a, int b)
{
    struct ST st[2];
    struct SH sh[2];
    struct FRQTRACE tr[2][FRQTRACE_N];
    Uint16 trn[2];
    int32  ret[2];
    JUMP_FRQ jf[2];
    int side, i;

    g_cases++;

    if (g_trace_on)
    {
        fprintf(stderr, "case %ld entry=%d a=%d b=%d\n", g_cases, entry, a, b);
    }

    for (side = 0; side < 2; side++)
    {
        apply_inputs(in);
        install(side, &in->st);
        sh_write(&in->sh0);
        accDecFrqPrcFlag = in->accDecFrqPrcFlag;
        frqTraceN = 0;

        if (entry == ENTRY_JUMPEND)
        {
            jf[side] = in->st.jumpFrq1;
            ret[side] = invoke(side, entry, a, b, &jf[side]);
        }
        else
        {
            ret[side] = invoke(side, entry, a, b, NULL);
        }

        capture(side, &st[side]);
        sh_read(&sh[side]);
        memcpy(tr[side], frqTrace, sizeof(tr[side]));
        trn[side] = frqTraceN;
    }

    /* ---- compare ---- */
    {
        int bad = 0;

#define CMPF(t, n)                                                        \
        if (st[0].n != st[1].n) {                                         \
            char buf[128];                                                \
            sprintf(buf, "ref=%ld new=%ld", (long)st[0].n, (long)st[1].n); \
            report(#n, buf); bad = 1;                                     \
        }
        FIELDS(CMPF)
#undef CMPF

        if (memcmp(&st[0].frqXy, &st[1].frqXy, sizeof(st[0].frqXy)))
        { report("frqXy", "aggregate differs"); bad = 1; }
        if (memcmp(&st[0].frqFlag, &st[1].frqFlag, sizeof(st[0].frqFlag)))
        { report("frqFlag", "aggregate differs"); bad = 1; }
        if (memcmp(&st[0].jumpFrq1, &st[1].jumpFrq1, sizeof(st[0].jumpFrq1)))
        { report("jumpFrq1", "aggregate differs"); bad = 1; }
        if (memcmp(&st[0].jumpFrq2, &st[1].jumpFrq2, sizeof(st[0].jumpFrq2)))
        { report("jumpFrq2", "aggregate differs"); bad = 1; }

        if (memcmp(&sh[0], &sh[1], sizeof(sh[0])))
        {
            char buf[224];
            sprintf(buf, "frqAimDisp %u/%u frqXDisp %u/%u frqYDisp %u/%u "
                         "frqAimPLCDisp %u/%u bDone %u/%u accDecTimeSrcPlc %u/%u "
                         "runFlag %04x/%04x accDecFrqPrcFlag %u/%u",
                    sh[0].frqAimDisp, sh[1].frqAimDisp,
                    sh[0].frqXDisp, sh[1].frqXDisp,
                    sh[0].frqYDisp, sh[1].frqYDisp,
                    sh[0].frqAimPLCDisp, sh[1].frqAimPLCDisp,
                    sh[0].bFrqDigitalDone4WaitDelay, sh[1].bFrqDigitalDone4WaitDelay,
                    sh[0].accDecTimeSrcPlc, sh[1].accDecTimeSrcPlc,
                    sh[0].runFlag, sh[1].runFlag,
                    sh[0].accDecFrqPrcFlag, sh[1].accDecFrqPrcFlag);
            report("shared-written", buf);
            bad = 1;
        }

        if (ret[0] != ret[1])
        {
            char buf[96];
            sprintf(buf, "ref=%ld new=%ld", (long)ret[0], (long)ret[1]);
            report("return value", buf);
            bad = 1;
        }

        if (trn[0] != trn[1] ||
            memcmp(tr[0], tr[1],
                   sizeof(struct FRQTRACE) * (trn[0] < trn[1] ? trn[0] : trn[1])))
        {
            char buf[192];
            unsigned k = (trn[0] < trn[1]) ? trn[0] : trn[1];
            unsigned d = 0;
            while (d < k && tr[0][d].which == tr[1][d].which &&
                   tr[0][d].arg == tr[1][d].arg)
            {
                d++;
            }
            sprintf(buf, "n ref=%u new=%u first-diff idx=%u "
                         "ref=(%u,%u) new=(%u,%u)",
                    trn[0], trn[1], d,
                    d < FRQTRACE_N ? tr[0][d].which : 999u,
                    d < FRQTRACE_N ? tr[0][d].arg   : 999u,
                    d < FRQTRACE_N ? tr[1][d].which : 999u,
                    d < FRQTRACE_N ? tr[1][d].arg   : 999u);
            report("stub call trace", buf);
            bad = 1;
        }

        if (entry == ENTRY_JUMPEND &&
            memcmp(&jf[0], &jf[1], sizeof(jf[0])))
        {
            report("JumpFrqEndCalc out-param", "differs");
            bad = 1;
        }

        (void)bad;
    }
}

/* ---------------------------------------------------------------------------
 * Input defaults: every field zeroed, then the handful that must not be zero.
 * ------------------------------------------------------------------------ */

static struct IN base;

static void init_base(void)
{
    int i;

    memset(&base, 0, sizeof(base));

    base.maxFrq      = 5000;      /* 50.00 Hz */
    base.upperFrq    = 5000;
    base.lowerFrq    = 0;
    base.frqPoint    = 2;
    /* The module's OWN maxFrq/upperFrq globals, which are carried state rather
     * than function-code inputs.  They must be non-zero: UpdateFrqAim computes
     * frqAimPLCDisp = frqAimTmp * 10000 / maxFrq, and calling it directly
     * (without the UpdateLimitFrq that normally refreshes them) would divide by
     * zero.  That is pre-existing behaviour, documented as gap (b) in the
     * header; the driver keeps maxFrq >= 1 rather than "fixing" it. */
    base.st.maxFrq   = 5000;
    base.st.upperFrq = 5000;
    base.st.frqMax   = 5000;
    base.st.frqMin   = -5000;
    base.frqXSrc     = FUNCCODE_frqXySrc_FC;
    base.frqYSrc     = FUNCCODE_frqXySrc_FC;
    base.frqCalcSrc  = FUNCCODE_frqCalcSrc_X;
    base.motorCtrlMode = FUNCCODE_motorCtrlMode_VF;
    base.runSrc      = 0;
    base.menu0DispStatus = MENU0_DISP_STATUS_RUN_STOP;
    base.swingStatus = SWING_NONE;
    base.errorLevel  = ERROR_LEVEL_NO_ERROR;
    base.runStatus   = RUN_STATUS_NORMAL;
    base.startRunStatus = START_RUN_STATUS_INIT;
    base.runBitRun   = 1;
    base.runBitCommon = 1;
    base.runBitDir   = FORWARD_DIR;
    base.runBitCurDir = FORWARD_DIR;
    base.cmdDir      = FORWARD_DIR;
    base.cmdCommon   = 1;
    base.diUpDownSlope = 100;
    base.zeroSpeedDeadTime = 10;
    base.mockDigit[0] = 1;
    base.mockDigit[1] = 2;
    base.mockDigit[2] = 3;
    base.mockDigit[3] = 4;
    base.mockDigit[4] = 5;
    for (i = 0; i < PLC_STEP_MAX; i++)
    {
        base.plcRunTime[i]    = (Uint16)(10 + i);
        base.plcAccDecTime[i] = (Uint16)(100 + i);
        base.plcFrq[i]        = (Uint16)(100 * i);
    }
    base.plcRunMode = FUNCCODE_plcRunMode_REPEAT;
    base.plcTimeUnit = FUNCCODE_plcTimeUnit_S;
}

/* ---------------------------------------------------------------------------
 * Phases
 * ------------------------------------------------------------------------ */

/* FrqSrcDeal over the source-selection space. */
static void phase_deal(void)
{
    static const Uint16 calc[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                                  15, 16, 17, 18, 19, 20, 21, 22, 30, 40, 50, 99};
    static const Uint16 xy[]   = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 20};
    size_t i, j, k;

    g_phase = "deal";

    for (i = 0; i < sizeof(calc) / sizeof(calc[0]); i++)
    for (j = 0; j < sizeof(xy) / sizeof(xy[0]); j++)
    for (k = 0; k < sizeof(xy) / sizeof(xy[0]); k++)
    {
        struct IN in = base;

        in.frqCalcSrc = calc[i];
        in.frqXSrc    = xy[j];
        in.frqYSrc    = xy[k];
        in.presetFrq  = 300;
        in.frqYOffsetFc = 50;
        in.frqYRange  = 100;
        in.aiSet[0]   = 12345;
        in.aiSet[1]   = -23456;
        in.aiSet[2]   = 30000;
        in.pulseInSet = 15000;
        in.frqComm    = 2500;
        in.dpFrqAim   = 3333;
        in.p2pProcessValue = 4000;
        in.frqRun     = 777;
        in.torqueAim  = 100;
        in.mockPidFrq = 1234;
        in.mockTorqueAim = 50;
        run_case(&in, ENTRY_DEAL, 0, 0);
    }
}

/* UpdateFrqAim over direction / jog / anti-reverse / runDir. */
static void phase_update_frq_aim(void)
{
    static const Uint16 dirs[] = {FORWARD_DIR, REVERSE_DIR};
    static const Uint16 jog[]  = {RUN_CMD_NO_JOG, RUN_CMD_FWD_JOG, RUN_CMD_REV_JOG};
    static const int32  aims[] = {0, 1, -1, 3000, -3000, 6000, -6000};
    size_t i, j, k, m;

    g_phase = "updateFrqAim";

    for (i = 0; i < 2; i++)
    for (j = 0; j < 3; j++)
    for (k = 0; k < 7; k++)
    for (m = 0; m < 2; m++)
    {
        struct IN in = base;

        in.cmdDir     = dirs[i];
        in.cmdJog     = jog[j];
        in.runBitJog  = (jog[j] != RUN_CMD_NO_JOG);
        in.st.frqAimTmp = aims[k];
        in.st.frq       = aims[(k + 3) % 7];
        in.antiReverseRun = 0;
        in.runDir     = (m ? FUNCCODE_runDir_REVERSE : FUNCCODE_runDir_NO_REVERSE);
        in.upperFrq   = 5000;
        in.mockTorqueAim = 0;
        run_case(&in, ENTRY_UPD_AIM, 0, 0);
    }

    /* anti-reverse-run, both signs, with and without a live frqAimOld4Dir */
    for (i = 0; i < 2; i++)
    for (k = 0; k < 7; k++)
    {
        struct IN in = base;

        in.antiReverseRun = 1;
        in.cmdDir     = dirs[i];
        in.st.frqAimTmp = aims[k];
        in.st.frq       = -aims[k];
        in.st.frqAimOld4Dir = (i ? -100 : 100);
        in.st.reverseTicker = (Uint32)(k * 7);
        in.zeroSpeedDeadTime = (Uint16)(1 + k);
        in.runBitRun  = 1;
        run_case(&in, ENTRY_UPD_AIM, 0, 0);
    }

    /* direction-change ticker saturation */
    for (k = 0; k < 40; k++)
    {
        struct IN in = base;

        in.runBitRun = 1;
        in.st.frqAimOld4Dir = 100;
        in.st.frqAimTmp = -100;
        in.st.frq = 0;
        in.st.reverseTicker = (Uint32)k;
        in.zeroSpeedDeadTime = 5;
        run_case(&in, ENTRY_UPD_AIM, 0, 0);
    }
}

/* FrqAimUpDownDeal: the panel up/down integrator. */
static void phase_updown(void)
{
    static const Uint16 flags[] = {0, 1, 3};
    static const Uint16 keys[]  = {KEY_UP, KEY_DOWN, 0};
    static const Uint16 accs[]  = {ACC_DEC_FRQ_NONE, ACC_DEC_FRQ_WAIT, ACC_DEC_FRQ_DONE};
    size_t i, j, k, m;

    g_phase = "updown";

    for (i = 0; i < 3; i++)            /* diFunc.f1.bit.up / .down */
    for (j = 0; j < 3; j++)
    for (k = 0; k < 3; k++)
    for (m = 0; m < 3; m++)
    {
        struct IN in = base;

        /* f_io.h struct DI_FUNC1_BITS: noFunc0 fwd1 rev2 tripleLine3 fwdJog4
         * revJog5 up6 down7 ... accDecTimeSrc16..17 frqSrcSwitch18
         * clearUpDownFrq19.  The first version of this phase used bits 1 and 2
         * (fwd/rev) for up/down, so FrqAimUpDownDeal's up/down inputs were never
         * actually asserted. */
        in.diF1 = 0;
        if (flags[i] & 1) in.diF1 |= (Uint32)(1u << 6);   /* up   */
        if (flags[i] & 2) in.diF1 |= (Uint32)(1u << 7);   /* down */
        in.keyFunc = keys[j];
        in.accDecFrqPrcFlag = accs[k];
        in.menu0DispStatus = m == 0 ? MENU0_DISP_STATUS_RUN_STOP : MENU0_DISP_STATUS_UP_DOWN;
        in.errorLevel = ERROR_LEVEL_NO_ERROR;
        in.diSelF2 = (m == 2) ? (1u << 0) : 0;
        in.st.upDownFrq = 100;
        in.st.upDownFrqInit = 7;
        in.st.uPDownFrqMax = 4000;
        in.st.uPDownFrqMin = -4000;
        in.st.lowerFrq = 100;
        in.st.frq = 200;
        in.st.frqAim = 300;
        in.st.frqKeyUpDownDelta = 25;
        in.presetFrq = 50;
        in.frqCalcSrc = FUNCCODE_frqCalcSrc_COMPOSE;
        in.frqXSrc = FUNCCODE_frqXySrc_FC;
        in.frqYSrc = FUNCCODE_frqXySrc_FC;
        in.st.frqFlag.bit.comp = (Uint16)(m & 1);
        run_case(&in, ENTRY_UPDOWN, 0, 0);
    }

    /* the clearUpDownFrq branch and the updnBenchmark branch.
     *
     * clearUpDownFrq is bit 19 of diFunc.f1, not bit 1..3 -- the first version
     * wrote `i << 1`, so the `upDownFrq = 0` inside that branch was never
     * executed and mutation M57 could not be seen. */
    for (i = 0; i < 4; i++)
    {
        struct IN in = base;

        in.diF1 = ((Uint32)(i & 1) << 19);          /* clearUpDownFrq */
        if (i & 2) in.diF1 |= (Uint32)(1u << 6);    /* up */
        in.accDecFrqPrcFlag = ACC_DEC_FRQ_NONE;
        in.updnBenchmark = (Uint16)(i & 1);
        in.menu0DispStatus = MENU0_DISP_STATUS_RUN_STOP;
        in.errorLevel = ERROR_LEVEL_NO_ERROR;
        in.diSelF2 = 0;
        in.runBitCommon = 1;
        in.st.upDownFrq = 500;
        in.st.uPDownFrqMax = 4000;
        in.st.uPDownFrqMin = -4000;
        in.st.frqFlag.bit.comp = (Uint16)(i >> 1);
        in.st.frqFlag.bit.upDownoperationStatus = UP_DN_OPERATION_ON;
        run_case(&in, ENTRY_UPDOWN, 0, 0);
    }

    /* The uPDownFrqLower1 / uPDownFrqLower2 selection.
     *
     * Two things must hold for the choice to be observable, and the phases above
     * supplied neither:
     *   (1) the branch needs |frqAim| < lowerFrq && |frq| <= lowerFrq.  The first
     *       loop used lowerFrq = 100 with frqAim = 300, which fails the first
     *       test, so the branch was never entered;
     *   (2) Lower1 and Lower2 must differ, otherwise picking the "wrong" one is
     *       a no-op.  Both stayed 0 because base is memset and nothing set them.
     * That is exactly why mutations M45/M46 were MISSED.
     *
     * bFrqDigitalOld is a function-local static, so the enclosing
     * `if (bFrqDigitalOld != bFrqDigital)` block only runs on a *transition*.
     * Each iteration therefore runs a priming case (bFrqDigital = 0) before the
     * target case (bFrqDigital = 1), so the transition is guaranteed instead of
     * depending on what the previous case happened to leave behind. */
    for (i = 0; i < 2; i++)            /* runFlag.bit.curDir */
    for (j = 0; j < 2; j++)            /* frqFlag.bit.comp */
    {
        int p;

        for (p = 0; p < 2; p++)        /* p = 0 primes, p = 1 is the target */
        {
            struct IN in = base;

            in.accDecFrqPrcFlag = ACC_DEC_FRQ_NONE;
            in.menu0DispStatus = MENU0_DISP_STATUS_RUN_STOP;
            in.swingStatus = SWING_NONE;
            in.errorLevel = ERROR_LEVEL_NO_ERROR;
            in.updnBenchmark = 0;
            in.diSelF2 = 0;
            in.runBitCommon = 1;
            in.diF1 = p ? (Uint32)(1u << 6) : 0;    /* up: target only */
            in.runBitCurDir = (Uint16)i;
            in.runDirPanelOld = 0;
            in.diUpDownSlope = 100;                 /* delta stays 0 */
            in.st.upDownFrq = 50;
            in.st.upDownFrqInit = 0;
            in.st.uPDownFrqMax = 4000;
            in.st.uPDownFrqMin = -4000;
            in.st.uPDownFrqLower1 = 111;
            in.st.uPDownFrqLower2 = 222;
            in.st.lowerFrq = 1000;
            in.st.frq = 200;
            in.st.frqAim = 100;
            in.st.frqKeyUpDownDelta = 25;
            in.st.frqFlag.bit.comp = (Uint16)j;
            in.st.frqFlag.bit.upDownoperationStatus = UP_DN_OPERATION_ON;
            in.presetFrq = 0;
            run_case(&in, ENTRY_UPDOWN, 0, 0);
        }
    }

    /* The composition-direction branch (reference lines 392~407).
     *
     * With frqFlag.bit.comp set and one of the two signed sums negative, the
     * increment is |frqAim| - |frq|; otherwise it is |frq| - |frqAim|.
     * Mutation M47 swaps exactly those two.  Reaching the anchor needs the 378
     * test to FAIL (so control falls into the else arm) while the 392 test
     * SUCCEEDS -- and the loops above never arranged that: they left frqXy at
     * zero, so both signed sums reduced to +presetFrq >= 0 and the arm at 403
     * was the only one ever taken.
     *
     * The two disjuncts get a case each: i = 0 drives x+z negative, i = 1 drives
     * y+z+presetFrq negative.  diUpDownSlope = 0 keeps the independent DI term
     * at zero, so the composed term is the only thing moving upDownFrq. */
    for (i = 0; i < 2; i++)            /* which disjunct of the 392 test holds */
    {
        int p;

        for (p = 0; p < 2; p++)        /* prime, then target */
        {
            struct IN in = base;

            in.accDecFrqPrcFlag = ACC_DEC_FRQ_NONE;
            in.menu0DispStatus = MENU0_DISP_STATUS_RUN_STOP;
            in.swingStatus = SWING_NONE;
            in.errorLevel = ERROR_LEVEL_NO_ERROR;
            in.updnBenchmark = 0;
            in.diSelF2 = 0;
            in.runBitCommon = 1;
            in.runBitJog = 0;
            in.diF1 = p ? (Uint32)(1u << 6) : 0;    /* up: target only */
            in.diUpDownSlope = 0;                   /* keep the DI term at 0 */
            in.presetFrq = 50;
            in.frqXSrc = FUNCCODE_frqXySrc_FC;
            in.frqYSrc = FUNCCODE_frqXySrc_FC;
            in.st.upDownFrq = 100;
            in.st.upDownFrqInit = 0;
            in.st.uPDownFrqMax = 4000;
            in.st.uPDownFrqMin = -4000;
            in.st.uPDownFrqLower1 = 111;
            in.st.uPDownFrqLower2 = 222;
            in.st.lowerFrq = 100;                   /* 378 fails: |500| >= 100 */
            in.st.frq = 200;
            in.st.frqAim = 500;
            in.st.frqKeyUpDownDelta = 0;
            in.st.frqXy.x = (int32)(i ? -300 : 100);
            in.st.frqXy.y = (int32)(i ? 100 : -300);
            in.st.frqXy.z = -200;
            in.st.frqFlag.bit.comp = 1;
            in.st.frqFlag.bit.upDownoperationStatus = UP_DN_OPERATION_ON;
            run_case(&in, ENTRY_UPDOWN, 0, 0);
        }
    }
}

/* FrqPlcSetDeal: the multi-speed step machine, including the carry paths. */
static void phase_plc(void)
{
    static const Uint16 modes[] = {FUNCCODE_plcRunMode_REPEAT,
                                   FUNCCODE_plcRunMode_ONCE_RUN,
                                   FUNCCODE_plcRunMode_ONCE_STOP};
    static const Uint16 rstat[] = {RUN_STATUS_NORMAL, RUN_STATUS_START, RUN_STATUS_STOP};
    static const Uint16 sstat[] = {START_RUN_STATUS_INIT,
                                   START_RUN_STATUS_HOLD_START_FRQ};
    size_t i, j, k, s;

    g_phase = "plc";

    for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
    for (k = 0; k < 2; k++)
    for (s = 0; s <= PLC_STEP_MAX; s++)
    {
        struct IN in = base;

        in.plcRunMode = modes[i];
        in.runStatus  = rstat[j];
        in.startRunStatus = sstat[k];
        in.runBitRun  = (j == 2) ? 0 : 1;
        in.runBitCommon = (Uint16)(s & 1);
        in.cmdPause   = (Uint16)((s >> 1) & 1);
        in.plcPowerOffRemMode = (Uint16)((s * 7) % 100);
        in.st.plcStep = (Uint16)s;
        in.st.plcStepOld = (Uint16)(s > 0 ? s - 1 : 0);
        in.st.plcTime = (Uint32)(s * 3);
        in.st.plcStepRemOld = (Uint16)(s % PLC_STEP_MAX);
        in.st.plcTimeRemOld = (Uint32)(s * 2);
        in.st.frqCalcSrcOld = 0;
        in.frqCalcSrc = FUNCCODE_frqCalcSrc_X;
        in.plcFrq0Src = FUNCCODE_plcFrq0Src_FC;
        run_case(&in, ENTRY_PLC, 0, 0);
    }

    /* time-based step advance: drive plcTime past plcTimeMax */
    for (i = 0; i < 3; i++)
    for (s = 0; s < PLC_STEP_MAX; s++)
    {
        struct IN in = base;

        in.plcRunMode = modes[i];
        in.runStatus = RUN_STATUS_NORMAL;
        in.runBitRun = 1;
        in.plcTimeUnit = (Uint16)(s & 1) ? FUNCCODE_plcTimeUnit_H : FUNCCODE_plcTimeUnit_S;
        in.st.plcStep = (Uint16)s;
        in.st.plcTime = (Uint32)(s == 0 ? 100000 : 0);
        in.plcRunTime[s] = (Uint16)(1 + (s % 3));
        run_case(&in, ENTRY_PLC, 0, 0);
    }

    /* End-of-loop handling: the 589~597 block is only reached when ++plcStep
     * wraps past PLC_STEP_MAX, so it needs an entry plcStep of PLC_STEP_MAX-1
     * together with a plcTime already past plcTimeMax.  The loops above never
     * produced that pair -- s == 0 had the big plcTime but a low step, and
     * s == PLC_STEP_MAX-1 had the right step but plcTime == 0 -- so
     * bPlcEndOneLoopEnable never got a chance to disagree.  That is why mutation
     * M68 (`if (!plcStep)` -> `if (plcStep)` at line 551) was MISSED.
     *
     * Repeating the identical case is what makes it observable.  Both sides take
     * the 592 block on the first pass and leave bPlcEndOneLoopEnable at 0; on the
     * second pass the reference leaves it at 0, while the mutant re-arms it at
     * 551 -- so only the mutant writes bPlcEndOneLoop = 1. */
    for (i = 0; i < 3; i++)            /* repeat so the static settles */
    {
        struct IN in = base;

        in.plcRunMode = FUNCCODE_plcRunMode_REPEAT;
        in.runStatus = RUN_STATUS_NORMAL;
        in.runBitRun = 1;
        in.runBitCommon = 1;
        in.runBitJog = 0;
        in.plcTimeUnit = FUNCCODE_plcTimeUnit_S;
        in.st.plcStep = (Uint16)(PLC_STEP_MAX - 1);
        in.st.plcStepOld = 0;
        in.st.plcTime = 100000;
        in.st.bPlcEndOneLoop = 0;
        in.plcRunTime[PLC_STEP_MAX - 1] = 1;
        run_case(&in, ENTRY_PLC, 0, 0);
    }

    /* UpdateMultiSetFrq across every source code */
    for (i = 0; i <= 8; i++)
    for (s = 0; s < 3; s++)
    {
        struct IN in = base;

        in.plcFrq0Src = (Uint16)i;
        in.frqCalcSrc = FUNCCODE_frqCalcSrc_X;
        in.aiSet[0] = 20000;
        in.pulseInSet = 10000;
        in.mockPidFrq = 4321;
        in.mockDigit[0] = 9;
        in.runSrc = 0;
        in.st.frqDigitalPlc = 111;
        in.st.frqDigitalTmp = 222;
        in.st.presetFrqOld = 1;
        run_case(&in, ENTRY_MULTISET, (int)s, 0);
    }
}

/* UpdateLimitFrq / GetFrq / GetFrqX / GetFrqY / GetFrqComp / FrqXyCalc. */
static void phase_limits(void)
{
    static const Uint16 usrc[] = {FUNCCODE_upperFrqSrc_FC, FUNCCODE_upperFrqSrc_AI1,
                                  FUNCCODE_upperFrqSrc_AI2, FUNCCODE_upperFrqSrc_AI3,
                                  FUNCCODE_upperFrqSrc_PULSE, FUNCCODE_upperFrqSrc_COMM};
    static const Uint16 pt[] = {0, 1, 2, 3, 4};
    size_t i, j, k;

    g_phase = "limits";

    for (i = 0; i < 6; i++)
    for (j = 0; j < 5; j++)
    for (k = 0; k < 4; k++)
    {
        struct IN in = base;

        in.upperFrqSrc = usrc[i];
        in.frqPoint = pt[j];
        in.upperFrq = (Uint16)(1000 * k);
        in.upperFrqOffset = (Uint16)(100 * k);
        in.lowerFrq = (Uint16)(200 * k);
        in.maxFrq = 5000;
        in.frqComm = (Uint16)(2500 - 1000 * k);
        in.aiSet[0] = (int16)(20000 - 5000 * k);
        in.aiSet[1] = (int16)(-15000 + 3000 * k);
        in.aiSet[2] = (int16)(30000);
        in.pulseInSet = (int16)(5000 * k);
        run_case(&in, ENTRY_UPDLIMIT, 0, 0);
        run_case(&in, ENTRY_GETFRQ, 0, 0);
        run_case(&in, ENTRY_GETFRQX, 0, 0);
        run_case(&in, ENTRY_GETFRQY, 0, 0);
        run_case(&in, ENTRY_GETFRQC, 0, 0);
    }

    /* FrqXyCalc across every source code, with the multi-set/plc/pid stubs */
    for (i = 0; i <= 12; i++)
    {
        struct IN in = base;

        in.frqCalcSrc = FUNCCODE_frqCalcSrc_COMPOSE;
        in.frqXSrc = FUNCCODE_frqXySrc_FC;
        in.frqYSrc = FUNCCODE_frqXySrc_FC;
        in.maxFrq = 5000;
        in.aiSet[0] = 10000;
        in.pulseInSet = 8000;
        in.mockPidFrq = 900;
        in.st.frqAiPu = 5000;
        in.st.frqFlag.bit.comp = 1;
        in.diF1 = (Uint32)(i & 1);
        run_case(&in, ENTRY_XYCALC, (int)i, 0);
    }

    /* getUpDownLimit: comp x type x frqYRangeBase */
    for (i = 0; i < 2; i++)
    for (j = 0; j < 2; j++)
    for (k = 0; k < 2; k++)
    {
        struct IN in = base;

        in.frqYRangeBase = (Uint16)k;
        in.frqYRange = 100;
        in.st.frqXy.x = 1000;
        in.st.frqXy.y = -2000;
        in.st.frqXy.z = 500;
        in.st.frqMax = 5000;
        in.st.frqMin = -5000;
        in.st.lowerFrq = 100;
        in.maxFrq = 5000;
        in.presetFrq = 50;
        run_case(&in, ENTRY_GETUPDOWN, (int)i, (int)j);
    }

    /* GetFrq's tail up/down clear (reference lines 1262~1275).
     *
     * The guard is frqFlag.bit.upDown, then !(xsrc < AI1 || ysrc < AI1).
     * upDown is set to 1 in exactly one place -- FrqAimUpDownDeal line 455 -- and
     * GetFrq zeroes it at line 1112 on entry, so the only way it can be 1 at line
     * 1262 is for GetFrqX to have reached FrqAimUpDownDeal *within this call*:
     *
     *     GetFrq -> GetFrqX -> FrqXyCalc(frqXSrc = PLC)
     *            -> FrqPlcSetDeal -> UpdateMultiSetFrq(plcStep == 0)
     *            -> FrqAimUpDownDeal
     *
     * That is a long chain to hit by chance, which made mutation M118
     * (`upDownFrq = 0` -> `1` at line 1272) a stream-dependent catch rather than
     * a covered one -- it passed before the random stream shifted and failed
     * after.  Pin the chain explicitly. */
    {
        struct IN in = base;

        in.frqCalcSrc  = FUNCCODE_frqCalcSrc_X;          /* x only; comp stays 0 */
        in.frqXSrc     = FUNCCODE_frqXySrc_PLC;          /* >= AI1, and the chain */
        in.frqYSrc     = FUNCCODE_frqXySrc_AI2;          /* >= AI1 */
        in.plcFrq0Src  = FUNCCODE_plcFrq0Src_PRESET_FRQ; /* -> FrqAimUpDownDeal */
        in.plcRunMode  = FUNCCODE_plcRunMode_REPEAT;
        in.runStatus   = RUN_STATUS_NORMAL;
        in.runBitRun   = 1;
        in.upperFrq    = 5000;
        in.maxFrq      = 5000;
        in.st.upperFrq = 5000;
        in.st.maxFrq   = 5000;
        in.st.plcStep  = 0;                 /* so UpdateMultiSetFrq takes the switch */
        in.st.plcTime  = 0;                 /* and FrqPlcSetDeal leaves plcStep at 0 */
        in.st.upDownFrq = 123;
        in.st.upDownFrqTmp = 456;
        in.st.frqFlag.bit.upDown = 1;       /* cleared at line 1112; set only by the chain */
        run_case(&in, ENTRY_GETFRQ, 0, 0);
    }

    run_case(&base, ENTRY_RESET, 0, 0);
}

/* JumpFrqDeal / JumpFrqEndCalc. */
static void phase_jump(void)
{
    static const Uint16 ranges[] = {0, 1, 10, 100, 1000, 5000};
    static const int32  aims[]   = {0, 50, -50, 500, -500, 2500, -2500, 5000};
    size_t i, j, k, m;

    g_phase = "jump";

    for (i = 0; i < 6; i++)
    for (j = 0; j < 8; j++)
    for (k = 0; k < 8; k++)
    for (m = 0; m < 2; m++)
    {
        struct IN in = base;

        in.jumpFrq1 = (Uint16)(500 + 10 * (Uint16)k);
        in.jumpFrq2 = (Uint16)(520 + 10 * (Uint16)k);
        in.jumpFrqRange = ranges[i];
        in.jumpFrqMode = (Uint16)m;
        in.upperFrq = 5000;
        in.maxFrq = 5000;
        in.st.frqAimTmp = aims[j];
        in.st.frqTmp = aims[(j + 1) % 8];
        in.st.frq = aims[(j + 2) % 8];
        in.st.frqAim = aims[(j + 3) % 8];
        in.runBitRun = 1;
        in.cmdCommon = 1;
        run_case(&in, ENTRY_JUMPDEAL, 0, 0);
    }

    /* JumpFrqEndCalc directly, including the low-clamp */
    for (i = 0; i < 6; i++)
    for (j = 0; j < 8; j++)
    {
        struct IN in = base;

        in.st.jumpFrq1.frq = (Uint16)aims[j];
        in.st.jumpFrq1.range = ranges[i];
        run_case(&in, ENTRY_JUMPEND, 0, 0);
    }
}

/* UpdateRunMode: speed/torque selection and the TorqueCalc hook. */
static void phase_runmode(void)
{
    static const Uint16 tcs[] = {0, 1};
    static const Uint16 modes[] = {FUNCCODE_motorCtrlMode_VF,
                                   FUNCCODE_motorCtrlMode_FVC};
    static const int16 aims[] = {0, 50, -50};
    size_t i, j, k;

    g_phase = "runMode";

    for (i = 0; i < 2; i++)
    for (j = 0; j < 2; j++)
    for (k = 0; k < 3; k++)
    {
        struct IN in = base;

        in.torqueCtrl = tcs[i];
        in.motorCtrlMode = modes[j];
        in.mockTorqueAim = aims[k];
        in.diF2 = (Uint32)((j << 1) | (i << 2));
        in.diF1 = (Uint32)(i << 3);
        in.st.runMode = (Uint16)(j ? RUN_MODE_SPEED_CTRL : RUN_MODE_TORQUE_CTRL);
        in.frqRun = 321;
        in.torqueCtrlFwdMaxFrq = 4000;
        in.torqueCtrlRevMaxFrq = 3000;
        run_case(&in, ENTRY_UPDMODE, 0, 0);
    }
}

/* A broad random sweep over everything at once. */
static unsigned long rnd_state = 12345UL;

static unsigned long rnd(void)
{
    rnd_state = rnd_state * 1103515245UL + 12345UL;
    return (rnd_state >> 16) & 0x7FFFUL;
}

static void phase_random(long n)
{
    long c;

    g_phase = "random";

    for (c = 0; c < n; c++)
    {
        struct IN in = base;
        int i;

        in.frqCalcSrc = (Uint16)(rnd() % 22);
        in.frqXSrc    = (Uint16)(rnd() % 13);
        in.frqYSrc    = (Uint16)(rnd() % 13);
        in.frqYOffsetFc = (int16)(rnd() % 401) - 200;
        in.frqYRange  = (Uint16)(rnd() % 201);
        in.frqYRangeBase = (Uint16)(rnd() % 2);
        in.upperFrqSrc = (Uint16)(rnd() % 7);
        in.upperFrq   = (Uint16)(rnd() % 6001);
        in.upperFrqOffset = (int16)(rnd() % 1001) - 500;
        in.lowerFrq   = (Uint16)(rnd() % 6001);
        in.maxFrq     = (Uint16)(1 + rnd() % 6000);
        in.frqPoint   = (Uint16)(rnd() % 5);
        in.frqComm    = (int16)(rnd() % 20001) - 10000;
        in.presetFrq  = (int16)(rnd() % 12001) - 6000;
        in.frqRunCmdBind = (Uint16)(rnd() % 60000);
        in.diUpDownSlope = (Uint16)(rnd() % 20001);
        in.updnBenchmark = (Uint16)(rnd() % 2);
        in.antiReverseRun = (Uint16)(rnd() % 2);
        in.runDir     = (Uint16)(rnd() % 2);
        in.zeroSpeedDeadTime = (Uint16)(rnd() % 21);
        in.errorRunFrqSrc = (Uint16)(rnd() % 2);
        in.torqueCtrl = (Uint16)(rnd() % 2);
        in.torqueCtrlFwdMaxFrq = (Uint16)(rnd() % 6001);
        in.torqueCtrlRevMaxFrq = (Uint16)(rnd() % 6001);
        in.jumpFrq1   = (Uint16)(rnd() % 6001);
        in.jumpFrq2   = (Uint16)(rnd() % 6001);
        in.jumpFrqRange = (Uint16)(rnd() % 6001);
        in.jumpFrqMode = (Uint16)(rnd() % 2);
        in.commProtocolSec = (Uint16)(rnd() % 3);
        in.p2pRevDataSel = (Uint16)(rnd() % 2);
        in.plcEnable  = (Uint16)(rnd() % 2);
        in.plcFrqSet  = (int16)(rnd() % 20001) - 10000;
        in.plcRunMode = (Uint16)(rnd() % 3);
        in.plcTimeUnit = (Uint16)(rnd() % 2);
        in.plcPowerOffRemMode = (Uint16)(rnd() % 100);
        in.plcFrq0Src = (Uint16)(rnd() % 9);
        in.motorCtrlMode = (Uint16)(rnd() % 3);
        in.aiSet[0] = (int16)(rnd() % 65536) - 32768;
        in.aiSet[1] = (int16)(rnd() % 65536) - 32768;
        in.aiSet[2] = (int16)(rnd() % 65536) - 32768;
        in.pulseInSet = (int16)(rnd() % 65536) - 32768;
        /* Full 32 bits, not 15: the DI unions are Uint32 and the interesting
         * bits (clearUpDownFrq 19, resetPLC 23, ...) live above bit 15. */
        in.diF1 = ((Uint32)rnd() << 16) | (Uint32)rnd();
        in.diF2 = ((Uint32)rnd() << 16) | (Uint32)rnd();
        in.diSelF2 = ((Uint32)rnd() << 16) | (Uint32)rnd();
        in.runBitRun = (Uint16)(rnd() % 2);
        in.runBitJog = (Uint16)(rnd() % 2);
        in.runBitCommon = (Uint16)(rnd() % 2);
        in.runBitDir = (Uint16)(rnd() % 2);
        in.runBitCurDir = (Uint16)(rnd() % 2);
        in.cmdDir = (Uint16)(rnd() % 2);
        in.cmdJog = (Uint16)(rnd() % 3);
        in.cmdPause = (Uint16)(rnd() % 2);
        in.cmdCommon = (Uint16)(rnd() % 2);
        in.runDirPanelOld = (Uint16)(rnd() % 2);
        in.runStatus = (Uint16)(rnd() % 3);
        in.startRunStatus = (Uint16)(rnd() % 2);
        in.swingStatus = (Uint16)(rnd() % 3);
        in.menu0DispStatus = (Uint16)(rnd() % 3);
        in.keyFunc = (Uint16)(rnd() % 4);
        in.runSrc = (Uint16)(rnd() % 6);
        in.errorLevel = (Uint16)(rnd() % 4);
        in.frqRun = (int32)(rnd() % 12001) - 6000;
        in.dpFrqAim = (int16)(rnd() % 20001) - 10000;
        in.CanRxTxCon = (Uint16)(rnd() % 3);
        in.p2pProcessValue = (int16)(rnd() % 20001) - 10000;
        in.torqueAim = (int16)(rnd() % 20001) - 10000;
        in.mockPidFrq = (int32)(rnd() % 12001) - 6000;
        in.mockTorqueAim = (int16)(rnd() % 12001) - 6000;
        for (i = 0; i < 5; i++) in.mockDigit[i] = (Uint16)(rnd() % 10);
        for (i = 0; i < PLC_STEP_MAX; i++)
        {
            in.plcRunTime[i] = (Uint16)(rnd() % 50);
            in.plcAccDecTime[i] = (Uint16)(rnd() % 5000);
            in.plcFrq[i] = (int16)(rnd() % 20001) - 10000;
        }
        in.accDecFrqPrcFlag = (Uint16)(rnd() % 3);

        /* carried per-side state.
         *
         * This must cover *every* member of struct ST, not a hand-picked subset.
         * The first version skipped uPDownFrqLower1/2, frqCurAimFrac,
         * frqCalcSrcOld, frqMax/frqMin and about twenty others, so those stayed
         * at whatever base's memset left (0).  A mutation whose only effect is on
         * such a field, or whose branch is only entered when such a field is
         * non-zero, was then invisible -- that is the single largest source of
         * the first sweep's MISSED results. */
        in.st.frq = (int32)(rnd() % 12001) - 6000;
        in.st.frqTmp = (int32)(rnd() % 12001) - 6000;
        in.st.frqFrac = (int32)(rnd() % 12001) - 6000;
        in.st.frqTmpFrac = (int32)(rnd() % 12001) - 6000;
        in.st.frqAim = (int32)(rnd() % 12001) - 6000;
        in.st.frqAimTmp = (int32)(rnd() % 12001) - 6000;
        in.st.frqAimTmp0 = (int32)(rnd() % 12001) - 6000;
        in.st.frqCurAim = (int32)(rnd() % 12001) - 6000;
        in.st.frqCurAimFrac = (int32)(rnd() % 12001) - 6000;
        in.st.frqDroop = (int32)(rnd() % 12001) - 6000;
        in.st.upDownFrq = (int32)(rnd() % 12001) - 6000;
        in.st.upDownFrqInit = (int16)(rnd() % 12001) - 6000;
        in.st.upDownFrqTmp = (int32)(rnd() % 12001) - 6000;
        in.st.uPDownFrqMax = (int32)(rnd() % 12001) - 6000;
        in.st.uPDownFrqMin = (int32)(rnd() % 12001) - 6000;
        in.st.uPDownFrqLower1 = (int32)(rnd() % 12001) - 6000;
        in.st.uPDownFrqLower2 = (int32)(rnd() % 12001) - 6000;
        in.st.upperFrq = (Uint16)(rnd() % 6001);
        in.st.lowerFrq = (Uint16)(rnd() % 6001);
        in.st.maxFrq = (Uint16)(1 + rnd() % 6000);   /* never 0: see gap (b) */
        in.st.benchFrq = (Uint16)(rnd() % 6001);
        in.st.presetFrqOld = (Uint16)(rnd() % 6001);
        in.st.timeBench = (Uint16)(rnd() % 6001);
        in.st.frqAiPu = (int32)(rnd() % 12001) - 6000;
        in.st.frqPuQ15 = (Uint16)(rnd() % 6001);
        in.st.frqMax = (int32)(rnd() % 12001) - 6000;
        in.st.frqMin = (int32)(rnd() % 12001) - 6000;
        in.st.frqDigitalTmp = (int32)(rnd() % 12001) - 6000;
        in.st.frqDigitalPlc = (int32)(rnd() % 12001) - 6000;
        in.st.frqKeyUpDownDelta = (Uint16)(rnd() % 20001);
        in.st.frqCalcSrc = (Uint16)(rnd() % 22);
        in.st.frqCalcSrcOld = (Uint16)(rnd() % 22);
        in.st.frqPlcType = (Uint16)(rnd() % 2);
        in.st.runMode = (Uint16)(rnd() % 3);
        in.st.runCmdBind = (Uint16)(rnd() % 60000);
        in.st.bStopPlc = (Uint16)(rnd() % 2);
        in.st.bPlcEndOneLoop = (Uint16)(rnd() % 2);
        in.st.bFrqDigital = (Uint16)(rnd() % 2);
        in.st.bAntiReverseRun = (Uint16)(rnd() % 2);
        in.st.frqAimOld4Dir = (int32)(rnd() % 12001) - 6000;
        in.st.reverseTicker = (Uint32)(rnd() % 100);
        in.st.plcStep = (Uint16)(rnd() % PLC_STEP_MAX);
        in.st.plcTime = (Uint32)(rnd() % 1000);
        in.st.plcStepOld = (Uint16)(rnd() % PLC_STEP_MAX);
        in.st.plcStepRemOld = (Uint16)(rnd() % PLC_STEP_MAX);
        in.st.plcTimeRemOld = (Uint32)(rnd() % 1000);
        in.st.frqFlag.all = (Uint16)rnd();
        in.st.frqXy.x = (int32)(rnd() % 12001) - 6000;
        in.st.frqXy.y = (int32)(rnd() % 12001) - 6000;
        in.st.frqXy.z = (int32)(rnd() % 12001) - 6000;
        in.st.jumpFrq1.frq = (Uint16)(rnd() % 6001);
        in.st.jumpFrq1.range = (Uint16)(rnd() % 6001);
        in.st.jumpFrq2.frq = (Uint16)(rnd() % 6001);
        in.st.jumpFrq2.range = (Uint16)(rnd() % 6001);
        in.sh0.frqAimDisp = (Uint16)rnd();
        in.sh0.frqXDisp = (Uint16)rnd();
        in.sh0.frqYDisp = (Uint16)rnd();
        in.sh0.frqAimPLCDisp = (Uint16)rnd();
        in.sh0.bFrqDigitalDone4WaitDelay = (Uint16)(rnd() % 2);
        in.sh0.accDecTimeSrcPlc = (Uint16)rnd();

        run_case(&in, (int)(rnd() % 17), (int)(rnd() % 20), (int)(rnd() % 2));
    }
}

/* Stateful sequences: the same inputs, repeated, so carried state matters. */
static void phase_sequence(long n)
{
    long c;

    g_phase = "sequence";

    for (c = 0; c < n; c++)
    {
        struct IN in = base;

        in.frqCalcSrc = (Uint16)(1 + (c % 10));
        in.frqXSrc = (Uint16)(c % 13);
        in.frqYSrc = (Uint16)((c * 3) % 13);
        in.presetFrq = 300;
        in.maxFrq = 5000;
        in.plcRunMode = FUNCCODE_plcRunMode_REPEAT;
        in.runStatus = RUN_STATUS_NORMAL;
        in.runBitRun = 1;
        in.st.plcStep = (Uint16)(c % PLC_STEP_MAX);
        in.st.plcTime = (Uint32)(c * 17);
        in.accDecFrqPrcFlag = (Uint16)(c % 3);
        in.diF1 = (Uint32)(c & 3);
        in.mockPidFrq = 1000 + (int32)(c % 7);
        run_case(&in, ENTRY_DEAL, 0, 0);
    }
}

/* FrqPlcSetDeal with the run branch NOT taken -- run before anything else.
 *
 * plcFlag is a function-local static of FrqPlcSetDeal.  It is set to 1 the first
 * time the run branch is entered and is never cleared.  The power-off-reminder
 * guard is
 *
 *     ((FUNCCODE_plcPowerOffRemMode_REM != (plcPowerOffRemMode%10)) || plcFlag)
 *     && (FUNCCODE_plcStopRemMode_REM != (plcPowerOffRemMode/10)) && (!pause)
 *
 * so once plcFlag is 1 the first disjunct is unconditionally true and mutating
 * the comparison (mutation M66) cannot change anything.  The only way to see
 * plcFlag == 0 is to run a run=0 sweep before any run=1 case, because the driver
 * has no handle on the module's statics.  Keeping runStatus != NORMAL and not
 * (START && HOLD_START_FRQ) means the run branch is never taken here, so plcFlag
 * stays 0 for every case in this phase.  That limitation is inherent to a
 * host-side harness against non-resettable statics and is recorded in README.md. */
static void phase_plc_cold(void)
{
    static const Uint16 rems[] = {0, 2, 11, 21, 99};
    size_t i, j;

    g_phase = "plcCold";

    for (i = 0; i < 5; i++)
    for (j = 0; j < 3; j++)
    {
        struct IN in = base;

        in.runStatus      = RUN_STATUS_STOP;
        in.startRunStatus = START_RUN_STATUS_INIT;
        in.runBitRun      = 0;                    /* -> the !runFlag.bit.run arm */
        in.runBitCommon   = (Uint16)(j & 1);
        in.cmdPause       = (Uint16)(j >> 1);
        in.plcPowerOffRemMode = rems[i];
        in.plcRunMode     = FUNCCODE_plcRunMode_REPEAT;
        in.st.plcStep     = (Uint16)j;
        in.st.plcStepOld  = 0;
        in.st.plcStepRemOld  = (Uint16)(j + 7);   /* != 0, so the two arms differ */
        in.st.plcTime     = (Uint32)(j * 5);
        in.st.plcTimeRemOld  = (Uint32)(j + 9);
        run_case(&in, ENTRY_PLC, 0, 0);
    }
}

/* The zero-speed dead-time ticker threshold in UpdateFrqAim.
 *
 * The threshold is funcCode.code.zeroSpeedDeadTime *
 * (TIME_UNIT_ZERO_SPEED_DEAD / RUN_CTRL_PERIOD), i.e. zeroSpeedDeadTime * 50.
 * Changing that `/` to `*` (mutation M37) raises it to zeroSpeedDeadTime * 200,
 * which is only observable when reverseTicker lands between the two thresholds.
 * The pre-existing phases swept reverseTicker only up to 42 -- below even the
 * smaller threshold -- so the branch was never taken at all, in either version.
 * Sweep the ticker across both. */
static void phase_deadtick(void)
{
    static const Uint16 zsdt[] = {1, 2, 3, 5};
    size_t i;
    Uint32 t;

    g_phase = "deadTick";

    for (i = 0; i < 4; i++)
    for (t = 0; t <= 1000; t += 7)
    {
        struct IN in = base;

        in.runBitRun      = 1;
        in.runBitDir      = FORWARD_DIR;
        in.runBitCurDir   = REVERSE_DIR;          /* so the assignment shows */
        in.runDir         = FUNCCODE_runDir_NO_REVERSE;
        in.antiReverseRun = 0;
        in.zeroSpeedDeadTime = zsdt[i];
        in.upperFrq       = 5000;
        in.st.maxFrq      = 5000;
        in.st.upperFrq    = 5000;
        in.st.frqAimTmp   = -100;                 /* frqAim = -100 */
        in.st.frq         = 0;                    /* frq4Dir == 0 */
        in.st.frqAimOld4Dir = 100;                /* 100 * -100 < 0 */
        in.st.reverseTicker = t;
        run_case(&in, ENTRY_UPD_AIM, 0, 0);
    }
}

int main(int argc, char **argv)
{
    long nrand = (argc > 1) ? atol(argv[1]) : 20000;
    long base_cases;

    /* Unbuffered: this driver is long-running, and a crash part-way through
     * must not swallow the progress it has already made (redirected stdout is
     * block-buffered, so a crash loses everything not yet flushed). */
    setvbuf(stdout, NULL, _IONBF, 0);
    g_trace_on = (getenv("FRQTRACE") != NULL);

    init_base();

    /* Must come first: it relies on FrqPlcSetDeal's plcFlag static still being 0. */
    base_cases = g_cases;
    phase_plc_cold();
    printf("  FrqPlcSetDeal (plcFlag 0)    : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_deal();
    printf("  deal source sweep            : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_update_frq_aim();
    printf("  UpdateFrqAim                 : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_deadtick();
    printf("  zero-speed dead ticker       : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_updown();
    printf("  FrqAimUpDownDeal             : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_plc();
    printf("  FrqPlcSetDeal / MultiSet     : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_limits();
    printf("  limits / GetFrq / XyCalc     : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_jump();
    printf("  JumpFrqDeal / JumpFrqEndCalc : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_runmode();
    printf("  UpdateRunMode                : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_random(nrand);
    printf("  random                       : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_sequence(nrand);
    printf("  stateful sequence            : %ld\n", g_cases - base_cases);

    printf("f_frqSrc difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    return g_fails ? 1 : 0;
}
