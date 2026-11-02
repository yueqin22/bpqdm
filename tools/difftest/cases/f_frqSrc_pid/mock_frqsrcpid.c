/* Shared mock for the f_frqSrc_pid differential test.
 *
 * One instance, read by both the ref and the new side, so both see exactly the
 * same inputs.  The module's own thirteen symbols are duplicated by ref_tu.c /
 * new_tu.c -- those are what gets compared.
 *
 * include order matches src/function/f_frqSrc_pid.c, plus f_error.h for
 * errorOther.
 *
 * ---- why real types, not substitutes ----
 *
 * This module's arithmetic is built on the widths of what it reads:
 *
 *   * (int32)(int16)funcCode.code.pidSet * 2097 >> 6 -- the sign extension of a
 *     16-bit field, then a signed shift;
 *   * ((Uint32)funcCode.code.pidKp << 16) / 1000 -- a 32-bit unsigned product
 *     that is *meant* to be near the wrap point;
 *   * PID_FUNC holds `int64 deltaIRem`, and PidFuncCalc accumulates into it.
 *
 * A substitute type would move a truncation or a wrap and the test would be
 * verifying a different program.  The list below is not guessed: it is every
 * undefined symbol the delivered object needs, each resolved to its real
 * declaration and cross-checked by letting the linker name the ones left out.
 *
 * ---- why three of the four functions are faithful copies ----
 *
 * LineCalc, LpfCalc and LineChangeCalc do not return a value -- they transform
 * the struct the caller hands them, and the module's next step depends on the
 * result.  Stubbing them with a constant would throw away the most interesting
 * half of the module: whether it sets up the *inputs* to those calls correctly
 * (.x1/.x2, .t/.in, .aimValue/.tickerAll/.curValue).  So they are copied
 * verbatim from their live branch -- f_io.c:2081 and f_io.c:2123 for the first
 * two, f_runSrc_accDecFrq.c:586 for the third -- and both sides call the same
 * copy, which makes any disagreement in the arguments visible in the resulting
 * state.
 *
 * UpdateMultiSetFrq is different: it belongs to f_frqSrc.c and returns a plain
 * int32, so it is stubbed with a driver-set value plus a trace entry.  That is
 * the same treatment f_frqSrc's own case gives to FrqPidSetDeal.
 *
 * None of the four is part of this module, so a bug in any of them is not this
 * case's business -- which is exactly why they are mocked rather than linked.
 */
#include "f_frqSrc.h"
#include "f_main.h"
#include "f_runSrc.h"
#include "f_io.h"
#include "f_menu.h"
#include "f_ui.h"
#include "f_error.h"

#include "mock_frqsrcpid.h"

/* ---------------------------------------------------------------------------
 * Everything the module reads from outside itself
 * ------------------------------------------------------------------------ */

FUNCCODE_ALL funcCode;      /* f_funcCode.h  the whole PID + frqYSrc parameter block */
union RUN_FLAG runFlag;     /* f_runSrc.h    bit.pid / .run / .jog */
union RUN_CMD  runCmd;      /* f_runSrc.h    bit.common */
struct DI_FUNC diFunc;      /* f_io.h        f1.bit.pidPause / .multiSet,
                                              f2.bit.pidDirRev / .pidChg / .pidPauseI */
struct AI_DEAL aiDeal[AI_NUMBER];   /* f_io.h  .set of AI1..AI3, AI_NUMBER = 3 */
int16  pulseInSet;          /* f_io.h        pulse-input setpoint */
int32  frqAiPu;             /* f_frqSrc.h    AI-derived per-unit frequency */
Uint16 upperFrq;            /* f_frqSrc.h    */
Uint16 lowerFrq;            /* f_frqSrc.h    */
Uint16 maxFrq;              /* f_frqSrc.h    */
Uint16 plcStep;             /* f_frqSrc.h    multi-speed step, this module only reads it */
Uint16 plcStepOld;          /* f_frqSrc.h    */
Uint16 frqCalcSrc;          /* f_frqSrc.h    frequency-source selector, read only here */
Uint16 frqCalcSrcOld;       /* f_frqSrc.h    */
union FRQ_FLAG frqFlag;     /* f_frqSrc.h    bit.frqSetLimit */
Uint16 errorOther;          /* f_error.h     the module *writes* ERROR_FDB_LOSE here */

/* ---------------------------------------------------------------------------
 * Driver-set stub inputs, trace, guard counter
 * ------------------------------------------------------------------------ */

int32  mockMultiSetFrq;

struct PIDTRACE pidTrace[PIDTRACE_N];
Uint32 pidTraceN;
Uint32 pidTraceHash = 2166136261UL;      /* FNV-1a offset basis */
Uint32 pidGuardN;

static void trace(Uint16 which, Uint32 arg)
{
    if (pidTraceN < PIDTRACE_N)
    {
        pidTrace[pidTraceN].which = which;
        pidTrace[pidTraceN].arg   = arg;
    }
    pidTraceN++;

    pidTraceHash = (pidTraceHash ^ (Uint32)which) * 16777619UL;
    pidTraceHash = (pidTraceHash ^ arg)           * 16777619UL;
}

/* ---------------------------------------------------------------------------
 * The four external functions
 * ------------------------------------------------------------------------ */

/* f_io.c:2081, live branch (#elif 1 after the `#if 0`).  The `tmp` computation
 * is deliberately unconditional in the original, so the division happens even
 * when mode 1 or 2 would not need it -- kept as-is, with the zero-denominator
 * guard described in mock_frqsrcpid.h. */
void LineCalc(LINE_STRUCT *p)
{
    int32 tmp = 0;

    trace(PIDTRACE_LINECALC, (Uint32)p->x);

    if (p->x2 == p->x1)
    {
        pidGuardN++;
    }
    else
    {
        tmp = ((((int32)(p->x - p->x1) << 15) / (p->x2 - p->x1)) * (p->y2 - p->y1)
                    + ((int32)p->y1 << 15) + (1L << 14)) >> 15;
    }

    if ((p->mode == 2) && (p->x < p->x1))
        p->y = 0;
    else if (p->x <= p->x1)
        p->y = p->y1;
    else if (p->x >= p->x2)
        p->y = p->y2;
    else
        p->y = tmp;

    if (p->mode == 1)
        p->y = tmp;
}

/* f_io.c:2123, live branch.  The original already guards t == 0. */
void LpfCalc(LowPassFilter *p)
{
    int32 tmp;

    trace(PIDTRACE_LPFCALC, (Uint32)p->t);

    p->outOld = p->out;
    if (!p->t)
    {
        p->out = p->in;
    }
    else
    {
        tmp = ((int32)p->in - p->outOld + p->remainder) * AI_CALC_PERIOD;

        p->out = tmp / (p->t * 1) + p->outOld;
        p->remainder = tmp % (p->t * 1);
    }
}

/* f_runSrc_accDecFrq.c:586.  The original already guards tickerAll == 0. */
void LineChangeCalc(LINE_CHANGE_STRUCT *p)
{
    int32 delta;

    trace(PIDTRACE_LINECHANGE, p->tickerAll);

    if (p->curValue != p->aimValue)
    {
        if (!p->tickerAll)
        {
            p->curValue = p->aimValue;
        }
        else
        {
            delta = ((int32)p->maxValue + p->remainder) / p->tickerAll;
            p->remainder = ((int32)p->maxValue + p->remainder) % p->tickerAll;

            if (p->aimValue > p->curValue)
            {
                p->curValue += delta;
                if (p->curValue > p->aimValue)
                    p->curValue = p->aimValue;
            }
            else
            {
                p->curValue -= delta;
                if (p->curValue < p->aimValue)
                    p->curValue = p->aimValue;
            }
        }
    }
}

/* f_frqSrc.c:665.  Belongs to another module -- stubbed, not copied. */
int32 UpdateMultiSetFrq(Uint16 step)
{
    trace(PIDTRACE_MULTISET, (Uint32)step);
    return mockMultiSetFrq;
}
