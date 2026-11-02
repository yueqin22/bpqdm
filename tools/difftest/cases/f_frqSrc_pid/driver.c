/* Differential test driver for f_frqSrc_pid.
 *
 * The module is the PID frequency-reference generator.  Once per control
 * period it decides the PID output frequency from whichever setpoint and
 * feedback source the function-code table selects, and it carries a fair
 * amount of state between calls: the whole PID_FUNC block, two low-pass
 * filters, a setpoint ramp, three tickers, a status enum and a flag.
 *
 * Public entry points, in the order the system uses them:
 *
 *   FrqPidSetDeal()         the periodic task. Calls UpdatePidFuncRefFdb and
 *                           UpdatePidFuncPara, runs the pidCalcFlag state
 *                           machine, computes pidError, calls pidFunc.calc
 *                           (i.e. PidFuncCalc) when the gates allow, filters the
 *                           output, applies the reset conditions, and returns
 *                           the scaled frequency.
 *   PidFuncCalc()           the PID step itself; also reachable as
 *                           pidFunc.calc.
 *   UpdatePidFuncPara()     recomputes Kp/Ki/Kd, the Q formats, the output
 *                           limits and the error dead-band from funcCode.
 *   UpdatePidFuncRefFdb()   resolves the setpoint and feedback sources, ramps
 *                           the setpoint, runs the feedback low-pass filter and
 *                           the feedback-loss detector.
 *
 * Nothing here touches a peripheral register.  The module's whole hardware
 * boundary is four functions it calls, all supplied by the shared mock:
 * LineCalc, LpfCalc, LineChangeCalc and UpdateMultiSetFrq.  See
 * mock_frqsrcpid.c.
 *
 * ---- what is compared ----
 *
 * After every call the driver compares, field by field:
 *
 *   * every field of PID_FUNC except `calc` -- see PIDFIELDS below for why that
 *     one is excluded;
 *   * the whole of pidFuncRef, pidParaLine and pidLpf[0..1] by memcmp (their
 *     `calc` pointers are the shared mock, identical on both sides);
 *   * the five scalar module globals (pidInitTicker, pidCalcFlag,
 *     pidFdbLoseTicker, pidError, bPidFdbLose);
 *   * the two shared globals the module *writes*: runFlag (bit.pid is forced to
 *     1) and errorOther (set to ERROR_FDB_LOSE on a detected loss);
 *   * the mock call trace -- count, order-sensitive hash, and the first
 *     PIDTRACE_N (which, arg) pairs;
 *   * the LineCalc zero-denominator guard counter;
 *   * the return value.
 *
 * ---- state the module carries, and how the driver reaches it ----
 *
 * The nine data globals are per-side and therefore both input and output: the
 * driver installs them before the call and reads them back after.  That is
 * deliberate -- a rewrite that stops updating one of them shows up immediately.
 *
 * The two function-local statics (pidCalcFlagOld in FrqPidSetDeal, pidChgOld in
 * PidFuncCalc) are NOT reachable: cl2000 mangles file-local statics, so they are
 * local symbols.  They are not a hole in the test.  Both sides run the *same*
 * call sequence from the same start state, so their statics evolve identically
 * as long as the two implementations are equivalent.  What the driver must never
 * do is give the two sides different call sequences -- run_case() is written so
 * that it cannot.  The price is that the driver cannot *reset* those statics, so
 * the reachability of the pidChg-transition block and of the CALC->INIT_HOLD
 * transition depends on what ran before.  phase_calc() and phase_seq() are
 * ordered with that in mind and say so where it matters.
 *
 * ---- pre-existing hazards that are NOT tested ----
 *
 * Listed rather than quietly avoided.  None is a defect the rewrite may "fix":
 * changing any of them would be a behaviour change, not a rewrite.
 *
 *   a. `Ki = (p->delta << (p->Qi - tmp1 - tmp2)) / ((pidError + p->error) >> 1);`
 *      in the DI branch.  `<<` binds looser than `-`, so the shift count is
 *      `p->Qi - tmp1 - tmp2`, not `p->Qi`; and the divisor is unguarded.  On
 *      C28x a divide by zero yields a defined value and no trap, on the host it
 *      is SIGFPE.  The driver keeps the divisor away from zero -- see safe_err()
 *      and the note on the setdeal phase -- so the expression itself is
 *      exercised (including the negative shift counts, which are deterministic
 *      at /Od) without killing the process.
 *   b. The `tmp2` shift in the same branch tests `p->Qi >= p->Qd` and then
 *      shifts by `p->Qd - p->Qi`, i.e. by a non-positive count in that arm.  The
 *      condition looks inverted.  It is preserved as-is and exercised.
 *   c. `p->fdb` is assigned the low-pass output without the `> 0x7fff` clamp
 *      that was applied to its input, so a negative feedback is unclamped.  The
 *      driver keeps the filter's `t` >= 5 or 0, which bounds the output to the
 *      input range and keeps every later product inside int32.
 *   d. Division by maxFrq, by pidTi, by pidTi2 and by (x2 - x1) in LineCalc.
 *      The driver keeps maxFrq >= 1, the two Ti's >= 1 and the two interpolation
 *      abscissae distinct.
 *   e. `p->outMin = (outMin << 15) / maxFrq` is not clamped, so |outMin| can
 *      reach ~2^31/maxFrq.  That is exactly the range where `pidOut * maxFrq`
 *      still fits int32, which is why the driver lets upperFrq/lowerFrq run up to
 *      30000 and no further.
 *   f. UpdatePidFuncRefFdb declares `int32 aimValue;` and `int32 fdb;`, assigns
 *      them in every case of their switches except `default:`, and then reads
 *      them unconditionally.  An out-of-range pidSetSrc or pidFdbSrc therefore
 *      copies an indeterminate value into pidFuncRef.aimValue / p->fdb.  That is
 *      a real defect, and it is *not testable*: the two sides are different
 *      functions, so their stack frames are not guaranteed to hold the same
 *      leftover.  It was measured, not assumed -- with the two source fields
 *      swept out of range, the fixed phases happened to agree (the call is made
 *      straight from invoke(), so both frames inherit the same leftovers) while
 *      the nested calls in the random phase disagreed on the first out-of-range
 *      case, e.g. `pidFunc.fdb ref=7953 new=7949`.  The driver therefore sweeps
 *      pidSetSrc over 0..6 and pidFdbSrc over 0..8, the ranges the function-code
 *      table can actually produce, and leaves the `default:` arms alone.
 *      A rewrite must preserve this shape, but no host-side test can pin it down.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Same include order as src/function/f_frqSrc_pid.c. */
#include "f_frqSrc.h"
#include "f_main.h"
#include "f_runSrc.h"
#include "f_io.h"
#include "f_menu.h"
#include "f_ui.h"
#include "f_error.h"

#include "mock_frqsrcpid.h"
#include "frqsrcpid_sides.h"

/* ---------------------------------------------------------------------------
 * The two sides
 * ------------------------------------------------------------------------ */

#define PS(n)    (side ? &new_##n : &ref_##n)
#define PARR(n)  (side ? new_##n : ref_##n)

/* PID_FUNC fields, minus `calc`.
 *
 * `calc` is the one field that is *supposed* to differ between the sides: ref's
 * initialiser installs ref_PidFuncCalc and new's installs new_PidFuncCalc, which
 * is the whole point of the exercise.  Copying the driver's template over it
 * would point both sides at the same function and turn the test into comparing
 * a function with itself, so it is excluded from both install and compare. */
#define PIDFIELDS(X)            \
    X(int32,  ref)              \
    X(int32,  fdb)              \
    X(int32,  Kp)               \
    X(int32,  Ki)               \
    X(int32,  Kd)               \
    X(int32,  Kp2)              \
    X(int32,  Ki2)              \
    X(int32,  Kd2)              \
    X(int16,  Qp)               \
    X(int16,  Qi)               \
    X(int16,  Qd)               \
    X(int32,  deltaMax)         \
    X(int32,  deltaMin)         \
    X(int32,  outMax)           \
    X(int32,  outMin)           \
    X(int32,  pidDLimit)        \
    X(int32,  errorDead)        \
    X(int32,  errorSmall)       \
    X(int32,  errorBig)         \
    X(int32,  error)            \
    X(int32,  error1)           \
    X(int32,  error2)           \
    X(int32,  delta)            \
    X(int32,  deltaPRem)        \
    X(int64,  deltaIRem)        \
    X(int32,  deltaDRem)        \
    X(int32,  deltaRemainder)   \
    X(int32,  out)              \
    X(Uint16, sampleTime)       \
    X(Uint16, sampleTcnt)

/* Per-side carried state.  Everything the module owns, in one struct. */
struct ST
{
    PID_FUNC           pidFunc;
    LINE_CHANGE_STRUCT pidFuncRef;
    LINE_STRUCT        pidParaLine;
    LowPassFilter      pidLpf[2];

    Uint32 pidInitTicker;
    int    pidCalcFlag;
    Uint32 pidFdbLoseTicker;
    int32  pidError;
    Uint16 bPidFdbLose;
};

static void capture(int side, struct ST *s)
{
    const PID_FUNC *pf = PS(pidFunc);

#define PIDCAP(t, n) s->pidFunc.n = pf->n;
    PIDFIELDS(PIDCAP)
#undef PIDCAP

    s->pidFuncRef  = *PS(pidFuncRef);
    s->pidParaLine = *PS(pidParaLine);
    s->pidLpf[0]   = PARR(pidLpf)[0];
    s->pidLpf[1]   = PARR(pidLpf)[1];

    s->pidInitTicker    = *PS(pidInitTicker);
    s->pidCalcFlag      = *PS(pidCalcFlag);
    s->pidFdbLoseTicker = *PS(pidFdbLoseTicker);
    s->pidError         = *PS(pidError);
    s->bPidFdbLose      = *PS(bPidFdbLose);
}

static void install(int side, const struct ST *s)
{
    PID_FUNC           *pf = PS(pidFunc);
    LINE_CHANGE_STRUCT *pr = PS(pidFuncRef);
    LINE_STRUCT        *pl = PS(pidParaLine);
    LowPassFilter      *lp = PARR(pidLpf);

    /* Save/restore each side's own `calc` pointer around the bulk copy, so the
     * driver can carry one template without ever touching the one field that
     * must stay side-specific. */
    void (*c)(void *) = pf->calc;

#define PIDINS(t, n) pf->n = s->pidFunc.n;
    PIDFIELDS(PIDINS)
#undef PIDINS

    pf->calc = c;

    c = pr->calc; *pr = s->pidFuncRef;  pr->calc = c;
    c = pl->calc; *pl = s->pidParaLine; pl->calc = c;
    c = lp[0].calc; lp[0] = s->pidLpf[0]; lp[0].calc = c;
    c = lp[1].calc; lp[1] = s->pidLpf[1]; lp[1].calc = c;

    *PS(pidInitTicker)    = s->pidInitTicker;
    *PS(pidCalcFlag)      = s->pidCalcFlag;
    *PS(pidFdbLoseTicker) = s->pidFdbLoseTicker;
    *PS(pidError)         = s->pidError;
    *PS(bPidFdbLose)      = s->bPidFdbLose;
}

/* ---------------------------------------------------------------------------
 * Shared globals the module writes
 * ------------------------------------------------------------------------ */

struct SH
{
    /* runFlag.bit.pid is forced to 1 on entry, and runFlag.bit.run is read by
     * the feedback-loss detector; errorOther is written on a detected loss.
     * Both are set to a deterministic per-case value by apply_inputs() before
     * each side runs, so there is nothing to restore -- only to observe. */
    Uint16 runFlag;
    Uint16 errorOther;
};

static void sh_read(struct SH *s)
{
    s->runFlag    = runFlag.all;
    s->errorOther = errorOther;
}

/* ---------------------------------------------------------------------------
 * Inputs
 * ------------------------------------------------------------------------ */

struct IN
{
    struct ST st;      /* per-side carried state, installed before the call */
    struct SH sh0;     /* unused today; kept so the shape matches the other cases */

    /* function code */
    Uint16 pidSetSrc, pidSet, pidFdbSrc, pidDir;
    Uint16 pidKp, pidTi, pidTd, reverseCutOffFrq, pidErrMin, pidDLimit;
    Uint16 pidSetChangeTime, pidFdbLpfTime, pidOutLpfTime;
    Uint16 pidKp2, pidTi2, pidTd2, pidParaChgCondition;
    Uint16 pidParaChgDelta1, pidParaChgDelta2;
    Uint16 pidInit, pidInitTime, pidOutDeltaMax, pidOutDeltaMin, pidIAttribute;
    Uint16 pidFdbLoseDetect, pidFdbLoseDetectTime, pidCalcMode;
    Uint16 frqYSrc, frqComm;

    /* digital inputs */
    Uint16 diPidPause, diPidDirRev, diPidChg, diPidPauseI, diMultiSet;

    /* run state */
    Uint16 runBitPid, runBitRun, runBitJog, cmdCommon;

    /* analog and frequency */
    int16  ai0, ai1, ai2;
    int16  pulseInSet;
    int32  frqAiPu;
    Uint16 upperFrq, lowerFrq, maxFrq;

    /* source-change and multi-speed state */
    Uint16 plcStep, plcStepOld, frqCalcSrc, frqCalcSrcOld;

    /* limits and misc */
    Uint16 frqSetLimit;
    Uint16 errorOther;
    int32  mockMultiSetFrq;
};

static void apply_inputs(const struct IN *in)
{
    memset(&funcCode, 0, sizeof(funcCode));
    funcCode.code.pidSetSrc            = in->pidSetSrc;
    funcCode.code.pidSet               = in->pidSet;
    funcCode.code.pidFdbSrc            = in->pidFdbSrc;
    funcCode.code.pidDir               = in->pidDir;
    funcCode.code.pidKp                = in->pidKp;
    funcCode.code.pidTi                = in->pidTi;
    funcCode.code.pidTd                = in->pidTd;
    funcCode.code.reverseCutOffFrq     = in->reverseCutOffFrq;
    funcCode.code.pidErrMin            = in->pidErrMin;
    funcCode.code.pidDLimit            = in->pidDLimit;
    funcCode.code.pidSetChangeTime     = in->pidSetChangeTime;
    funcCode.code.pidFdbLpfTime        = in->pidFdbLpfTime;
    funcCode.code.pidOutLpfTime        = in->pidOutLpfTime;
    funcCode.code.pidKp2               = in->pidKp2;
    funcCode.code.pidTi2               = in->pidTi2;
    funcCode.code.pidTd2               = in->pidTd2;
    funcCode.code.pidParaChgCondition  = in->pidParaChgCondition;
    funcCode.code.pidParaChgDelta1     = in->pidParaChgDelta1;
    funcCode.code.pidParaChgDelta2     = in->pidParaChgDelta2;
    funcCode.code.pidInit              = in->pidInit;
    funcCode.code.pidInitTime          = in->pidInitTime;
    funcCode.code.pidOutDeltaMax       = in->pidOutDeltaMax;
    funcCode.code.pidOutDeltaMin       = in->pidOutDeltaMin;
    funcCode.code.pidIAttribute        = in->pidIAttribute;
    funcCode.code.pidFdbLoseDetect     = in->pidFdbLoseDetect;
    funcCode.code.pidFdbLoseDetectTime = in->pidFdbLoseDetectTime;
    funcCode.code.pidCalcMode          = in->pidCalcMode;
    funcCode.code.frqYSrc              = in->frqYSrc;
    funcCode.code.frqComm              = in->frqComm;

    runFlag.all       = 0;
    runFlag.bit.pid   = in->runBitPid;
    runFlag.bit.run   = in->runBitRun;
    runFlag.bit.jog   = in->runBitJog;

    runCmd.all        = 0;
    runCmd.bit.common = in->cmdCommon;

    diFunc.f1.all           = 0;
    diFunc.f2.all           = 0;
    diFunc.f1.bit.pidPause  = in->diPidPause;
    diFunc.f1.bit.multiSet  = in->diMultiSet;
    diFunc.f2.bit.pidDirRev = in->diPidDirRev;
    diFunc.f2.bit.pidChg    = in->diPidChg;
    diFunc.f2.bit.pidPauseI = in->diPidPauseI;

    aiDeal[0].set = in->ai0;
    aiDeal[1].set = in->ai1;
    aiDeal[2].set = in->ai2;

    pulseInSet = in->pulseInSet;
    frqAiPu    = in->frqAiPu;
    upperFrq   = in->upperFrq;
    lowerFrq   = in->lowerFrq;
    maxFrq     = in->maxFrq;

    plcStep       = in->plcStep;
    plcStepOld    = in->plcStepOld;
    frqCalcSrc    = in->frqCalcSrc;
    frqCalcSrcOld = in->frqCalcSrcOld;

    frqFlag.all = 0;
    frqFlag.bit.frqSetLimit = in->frqSetLimit;

    errorOther      = in->errorOther;
    mockMultiSetFrq = in->mockMultiSetFrq;
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

#define ENTRY_SETDEAL   0   /* FrqPidSetDeal()            -> int32 */
#define ENTRY_CALC      1   /* PidFuncCalc(&pidFunc) */
#define ENTRY_UPD_PARA  2   /* UpdatePidFuncPara(&pidFunc) */
#define ENTRY_UPD_REF   3   /* UpdatePidFuncRefFdb(&pidFunc) */
#define ENTRY_SEQ       4   /* a consecutive FrqPidSetDeal() calls */

static int32 invoke(int side, int entry, int a)
{
    PID_FUNC *p = PS(pidFunc);
    int k;
    int32 r = 0;

    switch (entry)
    {
        case ENTRY_SETDEAL:
            r = side ? new_FrqPidSetDeal() : ref_FrqPidSetDeal();
            break;

        case ENTRY_CALC:
            if (side) new_PidFuncCalc(p); else ref_PidFuncCalc(p);
            break;

        case ENTRY_UPD_PARA:
            if (side) new_UpdatePidFuncPara(p); else ref_UpdatePidFuncPara(p);
            break;

        case ENTRY_UPD_REF:
            if (side) new_UpdatePidFuncRefFdb(p); else ref_UpdatePidFuncRefFdb(p);
            break;

        case ENTRY_SEQ:
            for (k = 0; k < a; k++)
            {
                r = side ? new_FrqPidSetDeal() : ref_FrqPidSetDeal();
            }
            break;

        default:
            break;
    }
    return r;
}

static void run_case(const struct IN *in, int entry, int a)
{
    struct ST st[2];
    struct SH sh[2];
    struct PIDTRACE tr[2][PIDTRACE_N];
    Uint32 trn[2], trh[2], grd[2];
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

        memset(pidTrace, 0, sizeof(pidTrace));
        pidTraceN    = 0;
        pidTraceHash = 2166136261UL;
        pidGuardN    = 0;

        ret[side] = invoke(side, entry, a);

        capture(side, &st[side]);
        sh_read(&sh[side]);
        memcpy(tr[side], pidTrace, sizeof(tr[side]));
        trn[side] = pidTraceN;
        trh[side] = pidTraceHash;
        grd[side] = pidGuardN;
    }

    /* ---- compare ---- */
    {
#define CMPF(t, n)                                                          \
        if (st[0].n != st[1].n) {                                           \
            char buf[128];                                                  \
            sprintf(buf, "ref=%ld new=%ld", (long)st[0].n, (long)st[1].n);  \
            report(#n, buf);                                                \
        }
        CMPF(Uint32, pidInitTicker)
        CMPF(int,    pidCalcFlag)
        CMPF(Uint32, pidFdbLoseTicker)
        CMPF(int32,  pidError)
        CMPF(Uint16, bPidFdbLose)
#undef CMPF

#define PIDCMP(t, n)                                                        \
        if (st[0].pidFunc.n != st[1].pidFunc.n) {                           \
            char buf[160];                                                  \
            sprintf(buf, "ref=%lld new=%lld",                               \
                    (long long)st[0].pidFunc.n, (long long)st[1].pidFunc.n);\
            report("pidFunc." #n, buf);                                     \
        }
        PIDFIELDS(PIDCMP)
#undef PIDCMP

        if (memcmp(&st[0].pidFuncRef, &st[1].pidFuncRef,
                   sizeof(st[0].pidFuncRef)))
        { report("pidFuncRef", "aggregate differs"); }

        if (memcmp(&st[0].pidParaLine, &st[1].pidParaLine,
                   sizeof(st[0].pidParaLine)))
        { report("pidParaLine", "aggregate differs"); }

        if (memcmp(st[0].pidLpf, st[1].pidLpf, sizeof(st[0].pidLpf)))
        { report("pidLpf", "aggregate differs"); }

        if (sh[0].runFlag != sh[1].runFlag)
        {
            char buf[96];
            sprintf(buf, "ref=%04x new=%04x", sh[0].runFlag, sh[1].runFlag);
            report("runFlag", buf);
        }

        if (sh[0].errorOther != sh[1].errorOther)
        {
            char buf[96];
            sprintf(buf, "ref=%u new=%u", sh[0].errorOther, sh[1].errorOther);
            report("errorOther", buf);
        }

        if (ret[0] != ret[1])
        {
            char buf[96];
            sprintf(buf, "ref=%ld new=%ld", (long)ret[0], (long)ret[1]);
            report("return value", buf);
        }

        if (grd[0] != grd[1])
        {
            char buf[96];
            sprintf(buf, "ref=%lu new=%lu", (unsigned long)grd[0],
                    (unsigned long)grd[1]);
            report("LineCalc guard hits", buf);
        }

        if (trn[0] != trn[1] || trh[0] != trh[1] ||
            memcmp(tr[0], tr[1], sizeof(tr[0])))
        {
            char buf[192];
            unsigned d = 0;

            while (d < PIDTRACE_N &&
                   tr[0][d].which == tr[1][d].which &&
                   tr[0][d].arg   == tr[1][d].arg)
            {
                d++;
            }
            sprintf(buf, "n ref=%lu new=%lu hash %08lx/%08lx first-diff %u "
                         "ref=(%u,%lu) new=(%u,%lu)",
                    (unsigned long)trn[0], (unsigned long)trn[1],
                    (unsigned long)trh[0], (unsigned long)trh[1], d,
                    tr[0][d].which, (unsigned long)tr[0][d].arg,
                    tr[1][d].which, (unsigned long)tr[1][d].arg);
            report("stub call trace", buf);
        }
    }
}

/* ---------------------------------------------------------------------------
 * Divisors the module does not guard
 * ------------------------------------------------------------------------ */

/* PidFuncCalc's DI branch divides by ((pidError + p->error) >> 1) with no guard
 * (hazard (a) in the header).  The driver nudges p->error so the sum is either
 * >= 65536 or negative -- in both cases every arithmetic right shift from 1 to
 * 16 bits of that sum is non-zero, so neither the baseline nor a mutation that
 * changes the shift amount can turn the divisor into zero.  Guarding only the
 * `>> 1` case would leave `>> 2` and `>> 3` mutations "caught" by a SIGFPE
 * rather than by a field diff, which is luck, not evidence.
 *
 * Applied to the shared input template, so both sides still see identical
 * inputs.  Only usable where the driver controls pidError as well: in the
 * FrqPidSetDeal path the module computes pidError from the setpoint ramp and
 * the feedback filter, so those phases keep pidParaChgCondition away from DI
 * instead -- see phase_setdeal. */
static int32 safe_err(int32 pidError, int32 err)
{
    int32 s = pidError + err;

    if ((s >> 16) == 0)          /* 0 <= s <= 65535 */
    {
        err += 65536 - s;        /* -> s == 65536 */
    }
    return err;
}

/* ---------------------------------------------------------------------------
 * Input defaults
 * ------------------------------------------------------------------------ */

static struct IN base;

static void init_base(void)
{
    memset(&base, 0, sizeof(base));

    base.maxFrq   = 5000;
    base.upperFrq = 5000;
    base.lowerFrq = 0;
    base.frqAiPu  = 0;

    base.pidSetSrc  = FUNCCODE_pidSetSrc_FC;
    base.pidFdbSrc  = FUNCCODE_pidFdbSrc_AI1;
    base.pidKp      = 1000;
    base.pidTi      = 100;
    base.pidTd      = 0;
    base.pidKp2     = 500;
    base.pidTi2     = 100;
    base.pidTd2     = 0;

    base.pidErrMin          = 10;
    base.pidSetChangeTime   = 10;
    base.pidFdbLpfTime      = 10;
    base.pidOutLpfTime      = 10;
    base.pidInit            = 0;
    base.pidInitTime        = 10;
    base.pidFdbLoseDetectTime = 10;

    base.pidParaChgDelta1 = 100;
    base.pidParaChgDelta2 = 2000;

    base.mockMultiSetFrq = 1000;

    /* Carried state: the module's own initialisers leave everything zero, so the
     * template must too -- otherwise the first case would already be installing
     * a state the two sides were not both built with.  Qp/Qi/Qd are the
     * exception: they are read as shift counts before UpdatePidFuncPara has ever
     * run, and a negative count is not something the module can be tested on, so
     * they are seeded with the values UpdatePidFuncPara's own algorithm starts
     * from (24/26/12). */
    base.st.pidFunc.Qp = 24;
    base.st.pidFunc.Qi = 26;
    base.st.pidFunc.Qd = 12;
}

/* ---------------------------------------------------------------------------
 * Sweep tables
 * ------------------------------------------------------------------------ */

static const Uint16 V_MAXFRQ[] = {1, 1000, 5000, 10000};
static const Uint16 V_UPPER[]  = {0, 100, 1000, 3000, 5000, 10000, 30000};
static const Uint16 V_LOWER[]  = {0, 100, 1000, 3000, 10000, 30000};
static const Uint16 V_KP[]     = {0, 1, 100, 1000, 2000, 5000, 10000, 30000};
static const Uint16 V_TI[]     = {1, 10, 100, 1000, 10000};
static const Uint16 V_TD[]     = {0, 1, 100, 1000, 10000};
static const Uint16 V_KP2[]    = {0, 1, 100, 1000, 5000};
static const Uint16 V_TI2[]    = {1, 10, 1000, 10000};
static const Uint16 V_TD2[]    = {0, 100, 10000};
static const Uint16 V_ERRMIN[] = {0, 10, 100, 1000};
static const Uint16 V_DLIM[]   = {0, 1, 100, 1000, 10000};
static const Uint16 V_DMAX[]   = {0, 1, 100, 1000, 10000};
static const Uint16 V_DMIN[]   = {0, 1, 100, 1000, 10000};
static const Uint16 V_SETCHG[] = {0, 1, 10, 100};
static const Uint16 V_FDBLPF[] = {0, 1, 10, 100};
static const Uint16 V_OUTLPF[] = {0, 1, 10, 100};
static const Uint16 V_INIT[]   = {0, 500, 1000};
static const Uint16 V_INITT[]  = {0, 1, 10, 100};
static const Uint16 V_DELTA1[] = {0, 100, 1000, 5000};
static const Uint16 V_DELTA2[] = {200, 2000, 9000, 20000};
static const Uint16 V_IATTR[]  = {0, 1, 5, 9, 10, 11, 19, 20, 21, 29, 30, 99};
static const Uint16 V_LOSED[]  = {0, 1, 10, 100, 500};
static const Uint16 V_LOSET[]  = {0, 1, 10, 100};
static const Uint16 V_SETS[]   = {0, 1000, 30000, 65535};
static const Uint16 V_COMM[]   = {0, 1000, 30000, 65535};
static const Uint16 V_REVCUT[] = {0, 100, 1000, 5000};
static const int16  V_AI[]     = {-30000, -1000, 0, 1000, 30000};
static const int16  V_PULSE[]  = {-1000, 0, 1000, 30000};
static const int32  V_FRQPU[]  = {-3000, 0, 500, 2000, 8000};
static const Uint16 V_YSRC[]   = {0, 1, 8, 9, 13};
static const Uint16 V_Q[]      = {0, 4, 8, 12, 16, 20, 24, 26};
/* pidSetSrc / pidFdbSrc cover exactly the values the function-code table can
 * produce.  Going one past the end reads an indeterminate local -- hazard (f). */
static const Uint16 V_SETS_SRC[] = {0, 1, 2, 3, 4, 5, 6};
static const Uint16 V_FDB_SRC[]  = {0, 1, 2, 3, 4, 5, 6, 7, 8};

#define NELEM(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* ---------------------------------------------------------------------------
 * Phase 1 -- UpdatePidFuncPara, called directly
 * ------------------------------------------------------------------------ */

static void phase_para(void)
{
    int i, j, k;

    g_phase = "updPara";

    for (i = 0; i < NELEM(V_KP); i++)
    for (j = 0; j < NELEM(V_TI); j++)
    for (k = 0; k < NELEM(V_KP2); k++)
    {
        struct IN in = base;

        in.pidKp  = V_KP[i];
        in.pidTi  = V_TI[j];
        in.pidTd  = V_TD[(i + j) % NELEM(V_TD)];
        in.pidKp2 = V_KP2[k];
        in.pidTi2 = V_TI2[(i + k) % NELEM(V_TI2)];
        in.pidTd2 = V_TD2[(j + k) % NELEM(V_TD2)];

        in.upperFrq = V_UPPER[(i + k) % NELEM(V_UPPER)];
        in.lowerFrq = V_LOWER[(j + k) % NELEM(V_LOWER)];
        in.maxFrq   = V_MAXFRQ[k % NELEM(V_MAXFRQ)];
        in.frqAiPu  = V_FRQPU[(i + j) % NELEM(V_FRQPU)];

        /* frqYSrc == PID is the second arm of the outMax and outMin overrides. */
        in.frqYSrc = (k & 1) ? FUNCCODE_frqXySrc_PID : 0;

        in.reverseCutOffFrq = V_REVCUT[(i + k) % NELEM(V_REVCUT)];
        in.pidErrMin        = V_ERRMIN[(j + k) % NELEM(V_ERRMIN)];
        in.pidFdbSrc        = (Uint16)((i + j) % (FUNCCODE_pidFdbSrc_AI1_SUB_AI2 + 3));
        in.pidDLimit        = V_DLIM[(i + k) % NELEM(V_DLIM)];
        in.pidOutDeltaMax   = V_DMAX[(j + k) % NELEM(V_DMAX)];
        in.pidOutDeltaMin   = V_DMIN[(i + j) % NELEM(V_DMIN)];
        in.pidParaChgDelta1 = V_DELTA1[i % NELEM(V_DELTA1)];
        in.pidParaChgDelta2 = V_DELTA2[j % NELEM(V_DELTA2)];

        /* Exercise the "the Q format shrank, so drop the accumulator" arms. */
        in.st.pidFunc.Qp      = V_Q[(i + 1) % NELEM(V_Q)];
        in.st.pidFunc.Qi      = V_Q[(j + 3) % NELEM(V_Q)];
        in.st.pidFunc.deltaPRem = (i & 1) ? 12345 : -12345;
        in.st.pidFunc.deltaIRem = (j & 1) ? 987654321 : -987654321;

        run_case(&in, ENTRY_UPD_PARA, 0);
    }
}

/* ---------------------------------------------------------------------------
 * Phase 2 -- UpdatePidFuncRefFdb, called directly
 * ------------------------------------------------------------------------ */

static void phase_ref(void)
{
    int i, j, k;

    g_phase = "updRef";

    /* Every setpoint source against every feedback source. */
    for (i = 0; i < NELEM(V_SETS_SRC); i++)
    for (j = 0; j < NELEM(V_FDB_SRC); j++)
    for (k = 0; k < 4; k++)
    {
        struct IN in = base;

        in.pidSetSrc = V_SETS_SRC[i];
        in.pidFdbSrc = V_FDB_SRC[j];
        in.pidSet    = V_SETS[k % NELEM(V_SETS)];
        in.frqComm   = V_COMM[(k + 1) % NELEM(V_COMM)];
        in.maxFrq    = V_MAXFRQ[k % NELEM(V_MAXFRQ)];
        in.pidSetChangeTime = V_SETCHG[k % NELEM(V_SETCHG)];
        in.pidFdbLpfTime    = V_FDBLPF[(i + j) % NELEM(V_FDBLPF)];

        in.ai0 = V_AI[k % NELEM(V_AI)];
        in.ai1 = V_AI[(k + 2) % NELEM(V_AI)];
        in.ai2 = V_AI[(k + 4) % NELEM(V_AI)];
        in.pulseInSet  = V_PULSE[(i + j) % NELEM(V_PULSE)];
        in.diMultiSet  = (Uint16)((i * 3 + j) & 0xF);
        in.mockMultiSetFrq = (k & 1) ? 30000 : -30000;

        /* The setpoint ramp and the feedback filter are both stateful. */
        in.st.pidFunc.ref             = (int32)(k * 3000) - 4000;
        in.st.pidFuncRef.remainder    = (k & 1) ? 1000 : -1000;
        in.st.pidLpf[0].remainder     = (k & 1) ? 7 : -7;
        in.st.pidLpf[0].out           = (int32)(k * 500) - 1000;
        in.st.pidLpf[0].outOld        = (int32)(k * 100) - 200;

        /* The feedback-loss detector only runs when pidCalcFlag is CALC_TRUE. */
        in.st.pidCalcFlag      = (k & 2) ? 3 : 1;
        in.st.pidFdbLoseTicker = (Uint32)(k * 13);
        in.pidFdbLoseDetect    = V_LOSED[(i + j + k) % NELEM(V_LOSED)];
        in.pidFdbLoseDetectTime = V_LOSET[(i + j) % NELEM(V_LOSET)];
        in.runBitRun           = (Uint16)(k & 1);

        run_case(&in, ENTRY_UPD_REF, 0);
    }

    /* Sweep the loss ticker across the threshold so both arms of the compare
     * are taken, and the errorOther write is actually reached. */
    for (i = 0; i < 4; i++)
    {
        Uint32 t;

        for (t = 0; t < 220; t += 3)
        {
            struct IN in = base;

            in.pidFdbSrc = FUNCCODE_pidFdbSrc_AI1;
            in.ai0       = 0;                 /* fdb == 0, always "lost" */
            in.pidFdbLoseDetect     = 1;      /* threshold 32767/1000 = 32 */
            in.pidFdbLoseDetectTime = (Uint16)(1 + i);   /* 5..20 ticks */
            in.runBitRun  = 1;
            in.st.pidCalcFlag      = 3;
            in.st.pidFdbLoseTicker = t;
            in.st.pidFunc.fdb      = (int32)(t % 100);
            in.st.pidLpf[0].out    = (int32)(t % 50);

            run_case(&in, ENTRY_UPD_REF, 0);
        }
    }
}

/* ---------------------------------------------------------------------------
 * Phase 3 -- PidFuncCalc, called directly
 * ------------------------------------------------------------------------ */

static void phase_calc(void)
{
    int i, j, k, m;

    g_phase = "calc";

    /* ---- branch 3: pidParaChgCondition neither DI nor PID_ERROR ---- */
    for (i = 0; i < NELEM(V_Q); i++)
    for (j = 0; j < 6; j++)
    {
        struct IN in = base;

        in.pidParaChgCondition = FUNCCODE_pidParaChgCondition_NO;
        in.frqSetLimit = (Uint16)(j & 1);
        in.pidIAttribute = V_IATTR[(i + j) % NELEM(V_IATTR)];
        in.diPidPauseI   = (Uint16)((j >> 1) & 1);

        in.st.pidError = 3000;
        in.st.pidFunc.error = -1000;
        in.st.pidFunc.Qp = V_Q[i];
        in.st.pidFunc.Qi = V_Q[(i + 2) % NELEM(V_Q)];
        in.st.pidFunc.Qd = V_Q[(i + 4) % NELEM(V_Q)];
        in.st.pidFunc.Kp = (int32)(j * 700) - 2000;
        in.st.pidFunc.Ki = (int32)(j * 300) - 900;
        in.st.pidFunc.Kd = (int32)(j * 90) - 250;
        in.st.pidFunc.pidDLimit    = (int32)(j * 40);
        in.st.pidFunc.deltaMax     = (j & 1) ? 5000 : -5000;
        in.st.pidFunc.deltaMin     = (j & 1) ? -5000 : 5000;
        in.st.pidFunc.outMax       = (int32)(j * 4000) - 10000;
        in.st.pidFunc.outMin       = -((int32)(j * 4000) - 10000);
        in.st.pidFunc.out          = (int32)((j - 3) * 3000);
        in.st.pidFunc.error1       = (int32)(j * 200) - 600;
        in.st.pidFunc.error2       = (int32)(j * 150) - 450;
        in.st.pidFunc.delta        = (int32)(j * 100) - 300;
        in.st.pidFunc.deltaPRem    = (int32)(j * 1000) - 3000;
        in.st.pidFunc.deltaIRem    = (int64)(j * 100000) - 300000;
        in.st.pidFunc.deltaDRem    = (int32)(j * 700) - 2100;
        in.st.pidFunc.deltaRemainder = (int32)(j * 30) - 90;

        run_case(&in, ENTRY_CALC, 0);
    }

    /* ---- branch 2: pidParaChgCondition == PID_ERROR ---- */
    for (i = 0; i < NELEM(V_Q); i++)
    for (j = 0; j < 6; j++)
    {
        struct IN in = base;

        in.pidParaChgCondition = FUNCCODE_pidParaChgCondition_PID_ERROR;
        in.frqSetLimit  = (Uint16)(j & 1);
        in.pidIAttribute = V_IATTR[(i * 3 + j) % NELEM(V_IATTR)];
        in.diPidPauseI   = (Uint16)((j >> 1) & 1);

        in.st.pidError = (int32)(j * 4000) - 10000;
        in.st.pidFunc.error  = -1000;
        in.st.pidFunc.errorSmall = 0;
        in.st.pidFunc.errorBig   = 5000;
        in.st.pidFunc.Kp = 1000;  in.st.pidFunc.Kp2 = 4000;
        in.st.pidFunc.Ki = 100;   in.st.pidFunc.Ki2 = 900;
        in.st.pidFunc.Kd = 10;    in.st.pidFunc.Kd2 = 80;
        in.st.pidFunc.Qp = V_Q[i];
        in.st.pidFunc.Qi = V_Q[(i + 2) % NELEM(V_Q)];
        in.st.pidFunc.Qd = V_Q[(i + 4) % NELEM(V_Q)];
        in.st.pidFunc.out = (int32)((j - 3) * 3000);
        in.st.pidFunc.outMax = 0x7fff;
        in.st.pidFunc.outMin = -0x7fff;
        in.st.pidFunc.deltaMax = 10000;
        in.st.pidFunc.deltaMin = -10000;
        in.st.pidFunc.pidDLimit = 1000;

        run_case(&in, ENTRY_CALC, 0);
    }

    /* ---- branch 1: pidParaChgCondition == DI ----
     *
     * The inner block only runs when pidChg differs from pidChgOld, and pidChgOld
     * is a static the driver cannot reset.  A priming call with pidChg = 1 (from a
     * start of 0) guarantees the block is entered at least once; alternating
     * thereafter keeps entering it on every call.  See hazard (a) for why the
     * divisor has to be nudged away from zero. */
    {
        struct IN in = base;

        in.pidParaChgCondition = FUNCCODE_pidParaChgCondition_DI;
        in.diPidChg = 1;
        in.st.pidError = 1000;
        in.st.pidFunc.error = safe_err(1000, -1000);
        in.st.pidFunc.delta = 100;
        in.st.pidFunc.Qp = 24; in.st.pidFunc.Qi = 26; in.st.pidFunc.Qd = 12;
        in.st.pidFunc.Kp = 1000; in.st.pidFunc.Kp2 = 2000;
        in.st.pidFunc.Ki = 100;  in.st.pidFunc.Ki2 = 200;
        in.st.pidFunc.Kd = 10;   in.st.pidFunc.Kd2 = 20;
        in.st.pidFunc.error1 = 900; in.st.pidFunc.error2 = 800;

        run_case(&in, ENTRY_CALC, 0);
    }

    for (i = 0; i < NELEM(V_Q); i++)
    for (j = 0; j < 6; j++)
    for (m = 0; m < 2; m++)
    {
        struct IN in = base;
        int32 pe = (int32)(j * 4000) - 10000;

        in.pidParaChgCondition = FUNCCODE_pidParaChgCondition_DI;
        in.diPidChg    = (Uint16)m;
        in.frqSetLimit = (Uint16)(j & 1);

        in.st.pidError = pe;
        in.st.pidFunc.error = safe_err(pe, (j & 1) ? -3000 : 3000);
        in.st.pidFunc.delta = (int32)(j * 500) - 1500;
        in.st.pidFunc.Qp = V_Q[i];
        in.st.pidFunc.Qi = V_Q[(i + 2) % NELEM(V_Q)];
        in.st.pidFunc.Qd = V_Q[(i + 4) % NELEM(V_Q)];
        in.st.pidFunc.Kp = 1000; in.st.pidFunc.Kp2 = 3000;
        in.st.pidFunc.Ki = 100;  in.st.pidFunc.Ki2 = 500;
        in.st.pidFunc.Kd = 10;   in.st.pidFunc.Kd2 = 90;
        in.st.pidFunc.error1 = (int32)(j * 100) - 300;
        in.st.pidFunc.error2 = (int32)(j * 60) - 180;
        in.st.pidFunc.out = (int32)((j - 3) * 4000);
        in.st.pidFunc.outMax = 12000;
        in.st.pidFunc.outMin = -12000;
        in.st.pidFunc.deltaMax = 4000;
        in.st.pidFunc.deltaMin = -4000;
        in.st.pidFunc.pidDLimit = 500;
        in.st.pidFunc.deltaPRem = (int32)(j * 900) - 2700;
        in.st.pidFunc.deltaIRem = (int64)(j * 70000) - 210000;
        in.st.pidFunc.deltaDRem = (int32)(j * 300) - 900;
        in.st.pidFunc.deltaRemainder = (int32)(j * 20) - 60;

        run_case(&in, ENTRY_CALC, 0);
    }

    /* ---- the I-term freeze predicate, exhaustively over its four terms ---- */
    for (i = 0; i < NELEM(V_IATTR); i++)
    for (j = 0; j < 8; j++)
    {
        struct IN in = base;

        in.pidParaChgCondition = FUNCCODE_pidParaChgCondition_NO;
        in.pidIAttribute = V_IATTR[i];
        in.diPidPauseI   = (Uint16)((j >> 2) & 1);
        in.frqSetLimit   = (Uint16)((j >> 1) & 1);

        in.st.pidError = (j & 1) ? 5000 : -5000;
        in.st.pidFunc.error = -1000;
        in.st.pidFunc.out    = (j & 1) ? 20000 : -20000;
        in.st.pidFunc.outMax = (j & 1) ? 10000 : 40000;
        in.st.pidFunc.outMin = (j & 1) ? -40000 : -10000;
        in.st.pidFunc.Kp = 1200; in.st.pidFunc.Ki = 300; in.st.pidFunc.Kd = 25;
        in.st.pidFunc.deltaMax = 8000; in.st.pidFunc.deltaMin = -8000;
        in.st.pidFunc.pidDLimit = 400;
        in.st.pidFunc.deltaIRem = 123456;

        run_case(&in, ENTRY_CALC, 0);
    }
}

/* ---------------------------------------------------------------------------
 * Phase 4 -- FrqPidSetDeal, one call at a time
 *
 * pidParaChgCondition is never DI here.  The DI inner block divides by
 * ((pidError + p->error) >> 1), and in this phase pidError is computed by the
 * module from the setpoint ramp and the feedback filter rather than set by the
 * driver, so the driver cannot apply safe_err().  The DI branch is covered
 * exhaustively in phase_calc instead, where the driver does control both terms.
 * ---------------------------------------------------------------- */

static void phase_setdeal(void)
{
    int i, j, k;

    g_phase = "setdeal";

    for (i = 0; i < 2; i++)                 /* pidCalcMode */
    for (j = 0; j < 2; j++)                 /* runCmd.common */
    for (k = 0; k < 16; k++)                /* the four gates */
    {
        struct IN in = base;

        in.pidCalcMode = (Uint16)i;
        in.cmdCommon   = (Uint16)j;
        in.diPidPause  = (Uint16)((k >> 0) & 1);
        in.runBitJog   = (Uint16)((k >> 1) & 1);
        in.pidDir      = (Uint16)((k >> 2) & 1);
        in.diPidDirRev = (Uint16)((k >> 3) & 1);

        in.pidInitTime = V_INITT[k % NELEM(V_INITT)];
        in.pidInit     = V_INIT[(k / 4) % NELEM(V_INIT)];
        in.pidOutLpfTime = V_OUTLPF[k % NELEM(V_OUTLPF)];
        in.pidFdbLpfTime = V_FDBLPF[(k / 2) % NELEM(V_FDBLPF)];

        in.pidSetSrc = FUNCCODE_pidSetSrc_FC;
        in.pidFdbSrc = FUNCCODE_pidFdbSrc_AI1;
        in.pidSet    = V_SETS[k % NELEM(V_SETS)];
        in.ai0       = V_AI[(k + 1) % NELEM(V_AI)];

        in.st.pidCalcFlag = (Uint16)(k % 4);
        in.st.pidFunc.error = 200000;
        in.st.pidFunc.out   = (int32)(k * 1000) - 8000;
        in.st.pidFunc.ref   = 3000;
        in.st.pidFunc.fdb   = 1000;

        run_case(&in, ENTRY_SETDEAL, 0);
    }

    /* The three reset conditions, each isolated.
     *
     * The first two arms of `(!pidCalcFlag) || (frqCalcSrcOld != frqCalcSrc) ||
     * ((plcStepOld) && (!plcStep))` are reached by choosing pidCalcFlag and the
     * two source words; the third needs plcStepOld non-zero with plcStep zero. */
    for (i = 0; i < 4; i++)                 /* installed pidCalcFlag */
    for (j = 0; j < 4; j++)                 /* source pair */
    for (k = 0; k < 4; k++)                 /* plc pair */
    {
        static const Uint16 srcA[] = {0, 7, 7, 3};
        static const Uint16 srcB[] = {0, 7, 3, 7};
        static const Uint16 plcA[] = {0, 1, 0, 1};
        static const Uint16 plcB[] = {0, 0, 1, 1};
        struct IN in = base;

        in.pidCalcMode = FUNCCODE_pidCalcMode_YES;
        in.pidSetSrc   = FUNCCODE_pidSetSrc_FC;
        in.pidFdbSrc   = FUNCCODE_pidFdbSrc_AI1;

        in.frqCalcSrc    = srcA[j];
        in.frqCalcSrcOld = srcB[j];
        in.plcStep       = plcA[k];
        in.plcStepOld    = plcB[k];

        in.st.pidCalcFlag    = i;
        in.st.pidFunc.error  = 200000;
        in.st.pidFunc.out    = 4321;
        in.st.pidLpf[1].out  = 999;
        in.st.pidLpf[1].remainder = 77;

        run_case(&in, ENTRY_SETDEAL, 0);
    }

    /* Every setpoint source and every feedback source through the real entry
     * point, with the output filter carrying state. */
    for (i = 0; i < NELEM(V_SETS_SRC); i++)
    for (j = 0; j < NELEM(V_FDB_SRC); j++)
    {
        struct IN in = base;

        in.pidCalcMode = FUNCCODE_pidCalcMode_YES;
        in.pidSetSrc   = V_SETS_SRC[i];
        in.pidFdbSrc   = V_FDB_SRC[j];
        in.pidSet      = 20000;
        in.frqComm     = 30000;
        in.ai0 = 1000; in.ai1 = -2000; in.ai2 = 3000;
        in.pulseInSet  = 5000;
        in.diMultiSet  = 3;
        in.mockMultiSetFrq = 20000;
        in.maxFrq = 5000;

        in.st.pidCalcFlag = 3;
        in.st.pidFunc.error = 200000;
        in.st.pidFunc.ref   = 1000;
        in.st.pidFunc.fdb   = 0;
        in.st.pidFunc.out   = 5000;
        in.st.pidLpf[1].out = 4000;
        in.st.pidLpf[1].remainder = 3;

        run_case(&in, ENTRY_SETDEAL, 0);
    }

    /* The output low-pass filter and the setpoint ramp carry `remainder`, and
     * the output clamp is a two-sided arm. */
    for (i = 0; i < NELEM(V_OUTLPF); i++)
    for (j = 0; j < 6; j++)
    {
        struct IN in = base;

        in.pidCalcMode   = FUNCCODE_pidCalcMode_YES;
        in.pidOutLpfTime = V_OUTLPF[i];
        in.upperFrq = V_UPPER[j % NELEM(V_UPPER)];
        in.lowerFrq = V_LOWER[j % NELEM(V_LOWER)];
        in.maxFrq   = V_MAXFRQ[j % NELEM(V_MAXFRQ)];

        in.st.pidCalcFlag = 3;
        in.st.pidFunc.error = 200000;
        in.st.pidFunc.out = (int32)(j * 6000) - 15000;
        in.st.pidFunc.ref = 2000;
        in.st.pidFunc.fdb = 500;
        in.st.pidLpf[1].out       = (int32)(j * 2000) - 5000;
        in.st.pidLpf[1].remainder = (i & 1) ? 11 : -11;

        run_case(&in, ENTRY_SETDEAL, 0);
    }
}

/* ---------------------------------------------------------------------------
 * Phase 5 -- stateful sequences
 *
 * This is where the pidCalcFlag machine actually advances: NO_CALC -> CALC ->
 * PID_INIT_HOLD -> CALC_TRUE, and back down through the reset arms.  Neither
 * pidCalcFlagOld nor pidChgOld can be reset by the driver, so both sides simply
 * run the same sequence and are expected to stay in lockstep.  pidParaChgCondition
 * is never DI here, for the reason given above phase_setdeal.
 * ---------------------------------------------------------------- */

static void phase_seq(void)
{
    static const Uint16 initT[]  = {0, 1, 3, 10, 100};
    static const Uint16 modes[]  = {0, 1};
    static const Uint16 srcSet[] = {0, 1, 2};
    int i, j, k, n;

    g_phase = "sequence";

    for (i = 0; i < NELEM(initT); i++)
    for (j = 0; j < NELEM(modes); j++)
    for (k = 0; k < NELEM(srcSet); k++)
    for (n = 2; n <= 6; n++)
    {
        struct IN in = base;

        in.pidInitTime = initT[i];
        in.pidCalcMode = modes[j];
        in.pidSetSrc   = srcSet[k];
        in.pidFdbSrc   = FUNCCODE_pidFdbSrc_AI1;
        in.pidSet      = 15000;
        in.ai0         = (int16)(200 * n);
        in.pidOutLpfTime = 10;
        in.pidFdbLpfTime = 10;

        in.st.pidCalcFlag = 0;          /* start from NO_CALC */
        in.st.pidFunc.out = 0;

        run_case(&in, ENTRY_SEQ, n);
    }

    /* pidCalcMode off with runCmd.common pulsing: the flag must fall back to
     * NO_CALC and the reset arm must fire, wiping the accumulators. */
    for (i = 0; i < 6; i++)
    {
        struct IN in = base;

        in.pidCalcMode = FUNCCODE_pidCalcMode_NO;
        in.cmdCommon   = (Uint16)(i & 1);
        in.pidSetSrc   = FUNCCODE_pidSetSrc_FC;
        in.pidFdbSrc   = FUNCCODE_pidFdbSrc_AI1;
        in.pidSet      = 25000;
        in.ai0         = 3000;
        in.st.pidCalcFlag = 3;
        in.st.pidFunc.out = 7777;
        in.st.pidFunc.deltaPRem = 111;
        in.st.pidFunc.deltaIRem = 222222;
        in.st.pidFunc.deltaDRem = 333;
        in.st.pidFunc.deltaRemainder = 44;
        in.st.pidLpf[1].out = 555;
        in.st.pidLpf[1].remainder = 66;

        run_case(&in, ENTRY_SEQ, 4);
    }

    /* The feedback-loss detector needs several consecutive CALC_TRUE calls to
     * walk its ticker up to the threshold and then trip. */
    for (i = 0; i < 3; i++)
    {
        struct IN in = base;

        in.pidCalcMode = FUNCCODE_pidCalcMode_YES;
        in.pidSetSrc   = FUNCCODE_pidSetSrc_FC;
        in.pidFdbSrc   = FUNCCODE_pidFdbSrc_AI1;
        in.pidSet      = 20000;
        in.ai0         = 0;                       /* fdb stays below the limit */
        in.runBitRun   = 1;
        in.pidFdbLoseDetect     = (Uint16)(1 + i);
        in.pidFdbLoseDetectTime = (Uint16)(1 + 2 * i);
        in.pidFdbLpfTime = 10;
        in.pidInitTime   = 0;                     /* straight to CALC_TRUE */
        in.st.pidCalcFlag = 0;
        in.st.pidFunc.out = 0;

        run_case(&in, ENTRY_SEQ, 12);
    }
}

/* ---------------------------------------------------------------------------
 * Phase 6 -- pseudo-random combinations
 * ------------------------------------------------------------------------ */

static unsigned long rnd_state = 20261004UL;

static unsigned long rnd(void)
{
    rnd_state = rnd_state * 1103515245UL + 12345UL;
    return (rnd_state >> 16) & 0x7fffUL;
}

#define PICK(a) ((a)[rnd() % (unsigned long)NELEM(a)])

static void phase_random(long n)
{
    long c;

    g_phase = "random";

    for (c = 0; c < n; c++)
    {
        struct IN in = base;
        int32 pe;

        in.pidSetSrc  = PICK(V_SETS_SRC);
        in.pidFdbSrc  = PICK(V_FDB_SRC);
        in.pidSet     = PICK(V_SETS);
        in.frqComm    = PICK(V_COMM);
        in.pidDir     = (Uint16)(rnd() & 1);
        in.pidKp      = PICK(V_KP);
        in.pidTi      = PICK(V_TI);
        in.pidTd      = PICK(V_TD);
        in.pidKp2     = PICK(V_KP2);
        in.pidTi2     = PICK(V_TI2);
        in.pidTd2     = PICK(V_TD2);
        in.pidErrMin  = PICK(V_ERRMIN);
        in.pidDLimit  = PICK(V_DLIM);
        in.pidSetChangeTime = PICK(V_SETCHG);
        in.pidFdbLpfTime    = PICK(V_FDBLPF);
        in.pidOutLpfTime    = PICK(V_OUTLPF);
        in.pidInit     = PICK(V_INIT);
        in.pidInitTime = PICK(V_INITT);
        in.pidOutDeltaMax = PICK(V_DMAX);
        in.pidOutDeltaMin = PICK(V_DMIN);
        in.pidIAttribute  = PICK(V_IATTR);
        in.pidFdbLoseDetect     = PICK(V_LOSED);
        in.pidFdbLoseDetectTime = PICK(V_LOSET);
        in.pidCalcMode = (Uint16)(rnd() & 1);
        in.frqYSrc     = PICK(V_YSRC);
        in.reverseCutOffFrq = PICK(V_REVCUT);

        in.pidParaChgCondition = (Uint16)(rnd() % 3);
        in.pidParaChgDelta1 = PICK(V_DELTA1);
        in.pidParaChgDelta2 = PICK(V_DELTA2);

        in.diPidPause  = (Uint16)(rnd() & 1);
        in.diPidDirRev = (Uint16)(rnd() & 1);
        in.diPidChg    = (Uint16)(rnd() & 1);
        in.diPidPauseI = (Uint16)(rnd() & 1);
        in.diMultiSet  = (Uint16)(rnd() & 0xF);

        in.runBitPid = (Uint16)(rnd() & 1);
        in.runBitRun = (Uint16)(rnd() & 1);
        in.runBitJog = (Uint16)(rnd() & 1);
        in.cmdCommon = (Uint16)(rnd() & 1);

        in.ai0 = PICK(V_AI);
        in.ai1 = PICK(V_AI);
        in.ai2 = PICK(V_AI);
        in.pulseInSet = PICK(V_PULSE);
        in.frqAiPu    = PICK(V_FRQPU);
        in.upperFrq   = PICK(V_UPPER);
        in.lowerFrq   = PICK(V_LOWER);
        in.maxFrq     = PICK(V_MAXFRQ);

        in.plcStep    = (Uint16)(rnd() % 6);
        in.plcStepOld = (Uint16)(rnd() % 6);
        in.frqCalcSrc    = (Uint16)(rnd() % 8);
        in.frqCalcSrcOld = (Uint16)(rnd() % 8);
        in.frqSetLimit   = (Uint16)(rnd() & 1);
        in.errorOther    = (Uint16)((rnd() & 1) ? ERROR_FDB_LOSE : 0);
        in.mockMultiSetFrq = (int32)(rnd() % 60001) - 30000;

        in.st.pidCalcFlag      = (int)(rnd() % 4);
        in.st.pidInitTicker    = (Uint32)rnd();
        in.st.pidFdbLoseTicker = (Uint32)rnd();
        in.st.bPidFdbLose      = (Uint16)(rnd() & 1);
        in.st.pidError         = (int32)(rnd() % 40001) - 20000;

        in.st.pidFunc.Qp = PICK(V_Q);
        in.st.pidFunc.Qi = PICK(V_Q);
        in.st.pidFunc.Qd = PICK(V_Q);
        in.st.pidFunc.Kp = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.Ki = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.Kd = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.Kp2 = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.Ki2 = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.Kd2 = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.deltaMax = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.deltaMin = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.outMax   = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.outMin   = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.pidDLimit = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.errorDead = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.errorSmall = (int32)(rnd() % 20001) - 10000;
        /* Must differ from errorSmall: LineCalc divides by (x2 - x1) and the
         * driver's guard would otherwise be the only thing keeping the run
         * alive.  A gap of at least 1 is enough, but keep it well clear. */
        in.st.pidFunc.errorBig   = in.st.pidFunc.errorSmall + 1234;
        in.st.pidFunc.error  = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.error1 = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.error2 = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.delta  = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.deltaPRem = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.deltaIRem = (int64)(rnd() % 4000001) - 2000000;
        in.st.pidFunc.deltaDRem = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.deltaRemainder = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.out    = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.ref    = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.fdb    = (int32)(rnd() % 40001) - 20000;
        in.st.pidFunc.sampleTime = (Uint16)rnd();
        in.st.pidFunc.sampleTcnt = (Uint16)rnd();

        in.st.pidFuncRef.maxValue  = (int32)(rnd() % 40001) - 20000;
        in.st.pidFuncRef.aimValue  = (int32)(rnd() % 40001) - 20000;
        in.st.pidFuncRef.curValue  = (int32)(rnd() % 40001) - 20000;
        in.st.pidFuncRef.tickerAll = (Uint32)(rnd() % 200);
        in.st.pidFuncRef.remainder = (int32)(rnd() % 40001) - 20000;

        in.st.pidParaLine.mode = 0;
        in.st.pidParaLine.x1 = (int32)(rnd() % 20001) - 10000;
        in.st.pidParaLine.x2 = in.st.pidParaLine.x1 + 1 + (int32)(rnd() % 20000);
        in.st.pidParaLine.y1 = (int32)(rnd() % 20001) - 10000;
        in.st.pidParaLine.y2 = (int32)(rnd() % 20001) - 10000;
        in.st.pidParaLine.x  = (int32)(rnd() % 20001) - 10000;
        in.st.pidParaLine.y  = (int32)(rnd() % 20001) - 10000;

        in.st.pidLpf[0].t         = (int32)(rnd() % 200);
        in.st.pidLpf[0].in        = (int32)(rnd() % 40001) - 20000;
        in.st.pidLpf[0].out       = (int32)(rnd() % 40001) - 20000;
        in.st.pidLpf[0].outOld    = (int32)(rnd() % 40001) - 20000;
        in.st.pidLpf[0].remainder = (int32)(rnd() % 201) - 100;
        in.st.pidLpf[1] = in.st.pidLpf[0];

        /* The DI branch is the one place the divisor is unguarded.  It is only
         * safe to exercise when the driver supplies pidError directly, i.e. on a
         * direct PidFuncCalc call.  The three entries that go through
         * FrqPidSetDeal let the module compute pidError itself, so they stay on
         * the other two parameter-change modes; the DI branch is covered by
         * phase_calc, which owns both terms of the sum. */
        pe = in.st.pidError;

        switch (rnd() % 5)
        {
            case 0:
                in.pidParaChgCondition = FUNCCODE_pidParaChgCondition_NO;
                run_case(&in, ENTRY_SETDEAL, 0);
                break;

            case 1:
                in.st.pidFunc.error = safe_err(pe, in.st.pidFunc.error);
                run_case(&in, ENTRY_CALC, 0);
                break;

            case 2:
                in.pidParaChgCondition = FUNCCODE_pidParaChgCondition_NO;
                run_case(&in, ENTRY_UPD_PARA, 0);
                break;

            case 3:
                in.pidParaChgCondition = FUNCCODE_pidParaChgCondition_NO;
                run_case(&in, ENTRY_UPD_REF, 0);
                break;

            default:
                in.pidParaChgCondition = FUNCCODE_pidParaChgCondition_NO;
                run_case(&in, ENTRY_SEQ, 1 + (int)(rnd() % 6));
                break;
        }
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
    g_trace_on = (getenv("PIDTRACE") != NULL);

    init_base();

    base_cases = g_cases;
    phase_para();
    printf("  UpdatePidFuncPara            : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_ref();
    printf("  UpdatePidFuncRefFdb          : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_calc();
    printf("  PidFuncCalc                  : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_setdeal();
    printf("  FrqPidSetDeal                : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_seq();
    printf("  stateful sequences           : %ld\n", g_cases - base_cases);

    base_cases = g_cases;
    phase_random(nrand);
    printf("  random                       : %ld\n", g_cases - base_cases);

    printf("f_frqSrc_pid difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    return g_fails ? 1 : 0;
}
