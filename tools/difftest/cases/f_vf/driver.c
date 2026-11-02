/* Differential test driver for f_vf.
 *
 * The module is the separate-voltage (V/F) reference chain of the drive. One
 * entry point, vfSeparateDeal(), and it does four things in order:
 *
 *   1. gate      -- only the ALL_SEPARATE (10) and HALF_SEPARATE (11) curves are
 *                  handled, and only when motorCtrlMode is VF (2). Anything else
 *                  zeroes both outputs and returns.
 *   2. source    -- pick vfSeprateVolAim from one of nine sources (function code,
 *                  AI1..AI3, pulse, multi-set, PLC, PID, communication) and scale
 *                  it to a voltage.
 *   3. shape     -- take the absolute value, apply the HALF_SEPARATE frequency
 *                  weighting, and clamp to the motor's rated voltage.
 *   4. ramp      -- push the aim through the LINE_CHANGE_STRUCT filter and read
 *                  the ramped value back out.
 *
 * Nothing in it touches a peripheral register. Its entire hardware boundary is
 * one function, LineChangeCalc, which the shared mock implements verbatim and
 * logs (see mock_vf.c / mock_vf.h).
 *
 * What is compared after every call:
 *
 *   vfSeprateVolAim          the module's scaled voltage reference
 *   vfSeparateVol            the module's ramped output (read by f_interface.c)
 *   vfSeprateVolAccLine.*    all five fields of the filter state, individually
 *   vfLog                    the LineChangeCalc call sequence, in order
 *   vfTrace                  the source-stub call trace (which source was asked
 *                            for, with which argument, in which order)
 *
 * vfSeprateVolAccLine.calc is deliberately NOT compared: it is a function
 * pointer and the two sides are handed two different global structs, so its
 * *value* is the same but comparing it adds nothing over comparing the four
 * data fields.
 *
 * ---- state the module carries between calls ----
 *
 * Unlike f_torqueCtrl's TorqueLimitCalc, whose state lives in function-local
 * statics that a driver cannot reach, this module's state is all file-scope:
 *
 *   vfSeprateVolAim          -- survives a call that takes the `default:` path
 *   vfSeparateVol            -- survives every path, and is fed back into the
 *                               filter as curValue
 *   vfSeprateVolAccLine.remainder -- the filter's accumulator remainder
 *
 * The first two are per-side globals the driver owns; the third is a field of a
 * per-side struct. So all three are installed explicitly before each call
 * (g_inAim / g_inSepVol / g_inRem) and are therefore *inputs* as well as
 * outputs. The other four filter fields are overwritten by the module before it
 * calls calc on every path that reaches calc, so the driver zeroes them; a
 * rewrite that stopped writing one of them would then read 0 on its side and
 * diverge from the reference, which is exactly what we want to see.
 *
 * ---- the `default:` branch of the source switch ----
 *
 * In f_torqueCtrl the analogous `default:` read an uninitialised local, which is
 * genuine undefined behaviour and had to be excluded from the tested state
 * space. Here it is different, and better: `default: break;` leaves
 * vfSeprateVolAim holding its **previous value**, a file-scope global. That is
 * perfectly well-defined, it is reachable (vfVoltageSrc is a Uint16 straight out
 * of the function-code table and nothing constrains it to 0..8), and it is
 * therefore tested -- see phase_default_carry(), which deliberately drives an
 * unmapped source code with a non-zero incoming aim and requires the value to
 * pass through unchanged.
 *
 * ---- pre-existing gaps that are NOT tested ----
 *
 * These are listed rather than quietly dropped; none of them is reachable from
 * a valid function-code table, and "fixing" them would be a behaviour change,
 * not a rewrite:
 *
 *   a. MULTI_SET / PLC / PID divide by maxFrq. maxFrq == 0 would be a division
 *      by zero. maxFrq is the drive's maximum frequency and is never 0.
 *   b. HALF_SEPARATE divides by motorFc.motorPara.elem.ratingFrq. Same argument.
 *   c. HALF_SEPARATE computes ABS_INT32(frq). frq == INT32_MIN would overflow.
 *      frq is a frequency in 0.01 Hz units with a bounded range.
 *   d. `(int32)X * ratingVoltage` is signed int32 arithmetic. The driver bounds
 *      X to +/-30000 and ratingVoltage to 0..65535, which keeps the product
 *      inside int32 for every case below (30000 * 65535 < 2^31).
 *   e. `outVoltageDisp` is declared extern on the module's line 28 and never
 *      used. It generates no symbol reference (confirmed against the object's
 *      undefined list) and there is nothing to test.
 *
 * The module also never reads funcCode.code.motorParaM1.elem.ratingVoltage and
 * motorFc.motorPara.elem.ratingVoltage interchangeably -- the first scales the
 * source, the second clamps the result and becomes the filter's maxValue. The
 * driver keeps them at *different* values in every phase so that a rewrite which
 * swaps them is caught rather than accidentally agreeing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Same include order as src/function/f_vf.c. */
#include "f_main.h"
#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_io.h"

#include "mock_vf.h"

#define FAIL_LIMIT   20

/* ---- per-side exports ---------------------------------------------------- */

extern void   ref_vfSeparateDeal(void);
extern void   new_vfSeparateDeal(void);

extern Uint16 ref_vfSeprateVolAim, new_vfSeprateVolAim;
extern Uint16 ref_vfSeparateVol,   new_vfSeparateVol;

extern LINE_CHANGE_STRUCT ref_vfSeprateVolAccLine, new_vfSeprateVolAccLine;

/* ---- side plumbing ------------------------------------------------------- */

static Uint16 *g_aim[2];
static Uint16 *g_vol[2];
static LINE_CHANGE_STRUCT *g_line[2];
static void   (*g_deal[2])(void);

static void init_ptrs(void)
{
    g_aim[0] = &ref_vfSeprateVolAim;   g_aim[1] = &new_vfSeprateVolAim;
    g_vol[0] = &ref_vfSeparateVol;     g_vol[1] = &new_vfSeparateVol;
    g_line[0] = &ref_vfSeprateVolAccLine;
    g_line[1] = &new_vfSeprateVolAccLine;

    g_deal[0] = ref_vfSeparateDeal;    g_deal[1] = new_vfSeparateDeal;
}

struct SIDE
{
    Uint16 aim;
    Uint16 sepVol;
    int32  lineAim, lineCur, lineMax, lineRem;
    Uint32 lineTicker;
    Uint16 logN;
    struct VFCALL log[VFLOG_N];
    Uint16 traceN;
    struct VFTRACE trace[VFTRACE_N];
};

static struct SIDE S[2];

/* ---- input state -------------------------------------------------------- */

static FUNCCODE_ALL     g_inFc;
static struct MOTOR_FC  g_inMotorFc;
static struct AI_DEAL   g_inAi[AI_NUMBER];
static int16            g_inPulse;
static struct DI_FUNC   g_inDiFunc;
static Uint16           g_inMaxFrq;
static int32            g_inFrq;
static union RUN_FLAG   g_inRunFlag;
static int32            g_inMultiSet, g_inPlc, g_inPid;

static Uint16 g_inAim;      /* previous vfSeprateVolAim */
static Uint16 g_inSepVol;   /* previous vfSeparateVol   */
static int32  g_inRem;      /* previous filter remainder */

static Uint32 g_seed = 0x2f6e2b1u;

static Uint16 rnd(void)
{
    g_seed = g_seed * 1103515245UL + 12345UL;
    return (Uint16)(g_seed >> 16);
}

static int16 rnd16(void)
{
    return (int16)rnd();
}

/* Zero everything, then let the caller fill in the fields it cares about.
 * The defaults are chosen so that a phase which only wants to vary one thing
 * still runs down the *productive* path (curve handled, mode VF, source FC,
 * drive running) instead of silently taking the guard's early return. */
static void reset_input(void)
{
    memset(&g_inFc, 0, sizeof g_inFc);
    memset(&g_inMotorFc, 0, sizeof g_inMotorFc);
    memset(g_inAi, 0, sizeof g_inAi);
    memset(&g_inDiFunc, 0, sizeof g_inDiFunc);
    memset(&g_inRunFlag, 0, sizeof g_inRunFlag);

    g_inPulse = 0;

    /* The two rated voltages are deliberately different, see the header note. */
    g_inFc.code.vfCurve                       = FUNCCODE_vfCurve_ALL_SEPARATE;
    g_inFc.code.vfVoltageSrc                  = FUNCCODE_vfVoltageSrc_FC;
    g_inFc.code.vfVoltageDigtalSet            = 0;
    g_inFc.code.vfVoltageAccTime              = 0;
    g_inFc.code.frqComm                       = 0;
    g_inFc.code.motorParaM1.elem.ratingVoltage = 380;
    g_inMotorFc.motorPara.elem.ratingVoltage  = 400;
    g_inMotorFc.motorPara.elem.ratingFrq      = 5000;
    g_inMotorFc.motorCtrlMode                 = FUNCCODE_motorCtrlMode_VF;

    g_inMaxFrq  = 5000;         /* never 0: the MULTI_SET/PLC/PID divisors */
    g_inFrq     = 0;
    g_inRunFlag.bit.run = 1;

    g_inMultiSet = 0;
    g_inPlc      = 0;
    g_inPid      = 0;

    g_inAim    = 0;
    g_inSepVol = 0;
    g_inRem    = 0;
}

static void install(int side)
{
    memcpy(&funcCode, &g_inFc, sizeof g_inFc);
    memcpy(&motorFc, &g_inMotorFc, sizeof g_inMotorFc);
    memcpy(aiDeal, g_inAi, sizeof g_inAi);
    memcpy(&diFunc, &g_inDiFunc, sizeof g_inDiFunc);
    memcpy(&runFlag, &g_inRunFlag, sizeof g_inRunFlag);

    pulseInSet = g_inPulse;
    maxFrq     = g_inMaxFrq;
    frq        = g_inFrq;

    mockMultiSetFrq = g_inMultiSet;
    mockPlcFrq      = g_inPlc;
    mockPidFrq      = g_inPid;

    *g_aim[side] = g_inAim;
    *g_vol[side] = g_inSepVol;

    /* Only `remainder` survives the module's own field writes; the other four are
     * overwritten before the filter runs. It is still installed for all of them
     * so that a call which does NOT reach the writes (guard / run-flag early
     * return) is compared against the same starting point on both sides. */
    g_line[side]->aimValue  = 0;
    g_line[side]->curValue  = 0;
    g_line[side]->maxValue  = 0;
    g_line[side]->tickerAll = 0;
    g_line[side]->remainder = g_inRem;

    vfLogN   = 0;
    vfTraceN = 0;
}

static void snapshot(int side)
{
    struct SIDE *s = &S[side];
    LINE_CHANGE_STRUCT *p = g_line[side];

    s->aim        = *g_aim[side];
    s->sepVol     = *g_vol[side];
    s->lineAim    = p->aimValue;
    s->lineCur    = p->curValue;
    s->lineMax    = p->maxValue;
    s->lineTicker = p->tickerAll;
    s->lineRem    = p->remainder;

    if (vfLogN > VFLOG_N)
    {
        printf("FATAL: vfLog overflow (%u > %d)\n", vfLogN, VFLOG_N);
        exit(2);
    }
    if (vfTraceN > VFTRACE_N)
    {
        printf("FATAL: vfTrace overflow (%u > %d)\n", vfTraceN, VFTRACE_N);
        exit(2);
    }

    s->logN = vfLogN;
    memcpy(s->log, vfLog, vfLogN * sizeof(struct VFCALL));

    s->traceN = vfTraceN;
    memcpy(s->trace, vfTrace, vfTraceN * sizeof(struct VFTRACE));
}

/* ---- case runner -------------------------------------------------------- */

static long g_cases = 0;
static int  g_fails = 0;

static void report(void)
{
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH vfSeparateDeal\n");
    printf("  in: curve=%u mode=%u src=%u dig=%u acc=%u comm=%u run=%u\n",
           g_inFc.code.vfCurve, g_inMotorFc.motorCtrlMode,
           g_inFc.code.vfVoltageSrc, g_inFc.code.vfVoltageDigtalSet,
           g_inFc.code.vfVoltageAccTime, g_inFc.code.frqComm,
           (Uint16)g_inRunFlag.bit.run);
    printf("      fcRV=%u mfRV=%u mfRF=%u maxFrq=%u frq=%ld pulse=%d\n",
           g_inFc.code.motorParaM1.elem.ratingVoltage,
           g_inMotorFc.motorPara.elem.ratingVoltage,
           g_inMotorFc.motorPara.elem.ratingFrq,
           g_inMaxFrq, (long)g_inFrq, (int)g_inPulse);
    printf("      ai=%d,%d,%d ms=%ld plc=%ld pid=%ld step=%u\n",
           (int)g_inAi[0].set, (int)g_inAi[1].set, (int)g_inAi[2].set,
           (long)g_inMultiSet, (long)g_inPlc, (long)g_inPid,
           (unsigned)g_inDiFunc.f1.bit.multiSet);
    printf("      prev: aim=%u sepVol=%u rem=%ld\n",
           g_inAim, g_inSepVol, (long)g_inRem);
    printf("  ref: aim=%u sepVol=%u line=(%ld,%ld,%ld,%lu,%ld) log=%u tr=%u\n",
           S[0].aim, S[0].sepVol, (long)S[0].lineAim, (long)S[0].lineCur,
           (long)S[0].lineMax, (unsigned long)S[0].lineTicker,
           (long)S[0].lineRem, S[0].logN, S[0].traceN);
    printf("  new: aim=%u sepVol=%u line=(%ld,%ld,%ld,%lu,%ld) log=%u tr=%u\n",
           S[1].aim, S[1].sepVol, (long)S[1].lineAim, (long)S[1].lineCur,
           (long)S[1].lineMax, (unsigned long)S[1].lineTicker,
           (long)S[1].lineRem, S[1].logN, S[1].traceN);
}

static void run_case(void)
{
    int side;

    g_cases++;

    for (side = 0; side < 2; side++)
    {
        install(side);
        g_deal[side]();
        snapshot(side);
    }

    if (S[0].aim != S[1].aim || S[0].sepVol != S[1].sepVol)
    {
        report();
        return;
    }
    if (S[0].lineAim != S[1].lineAim || S[0].lineCur != S[1].lineCur ||
        S[0].lineMax != S[1].lineMax || S[0].lineTicker != S[1].lineTicker ||
        S[0].lineRem != S[1].lineRem)
    {
        report();
        return;
    }
    if (S[0].logN != S[1].logN || S[0].traceN != S[1].traceN)
    {
        report();
        return;
    }
    if (S[0].logN && memcmp(S[0].log, S[1].log, S[0].logN * sizeof(struct VFCALL)))
    {
        report();
        return;
    }
    if (S[0].traceN && memcmp(S[0].trace, S[1].trace, S[0].traceN * sizeof(struct VFTRACE)))
    {
        report();
        return;
    }
}

/* =========================================================================
 * phases
 * ======================================================================= */

/* ---- phase 1: the gate ---------------------------------------------------
 *
 * Both outputs must be zeroed, and -- just as important -- the filter must not
 * run: vfSeprateVolAccLine must be left untouched and LineChangeCalc must not be
 * called. A rewrite that hoisted the filter above the gate would keep the
 * outputs right in the simple cases and still be caught here, via the log.
 */
static void phase_gate(void)
{
    static const Uint16 curves[] =
        { 0, 1, 2, 3, 9, 10, 11, 12, 65535 };
    static const Uint16 modes[] = { 0, 1, 2, 3, 65535 };
    Uint16 i, j, k;

    for (i = 0; i < sizeof curves / sizeof curves[0]; i++)
    {
        for (j = 0; j < sizeof modes / sizeof modes[0]; j++)
        {
            for (k = 0; k < 4; k++)
            {
                reset_input();
                g_inFc.code.vfCurve      = curves[i];
                g_inMotorFc.motorCtrlMode = modes[j];

                /* Non-zero incoming state, so "left untouched" is observable. */
                g_inAim    = 0x1234;
                g_inSepVol = 0x5678;
                g_inRem    = 77;

                g_inFc.code.vfVoltageSrc = FUNCCODE_vfVoltageSrc_AI1;
                g_inAi[0].set = 1234;
                g_inRunFlag.bit.run = (Uint16)(k & 1);

                run_case();
            }
        }
    }
}

/* ---- phase 2: every source, plus the unmapped codes ---------------------- */

static void phase_src_sweep(void)
{
    Uint16 src;

    for (src = 0; src <= 24; src++)
    {
        reset_input();
        g_inFc.code.vfVoltageSrc = src;

        g_inFc.code.vfVoltageDigtalSet = 3333;
        g_inAi[0].set = 1000;
        g_inAi[1].set = 2000;
        g_inAi[2].set = 3000;
        g_inPulse = 4000;
        g_inMultiSet = 1111;
        g_inPlc = 2222;
        g_inPid = 3333;
        g_inFc.code.frqComm = 4444;
        g_inDiFunc.f1.bit.multiSet = 7;

        g_inAim    = 900;
        g_inSepVol = 100;
        g_inRem    = 5;

        run_case();
    }

    /* The argument handed to UpdateMultiSetFrq is diFunc.f1.bit.multiSet, a 4-bit
     * field at bit offset 12. Sweep f1.all with the *upper* bits set so that
     * `f1.bit.multiSet` and `f1.all` are distinguishable -- otherwise a rewrite
     * that read the whole union instead of the field would agree by accident,
     * and the stub trace would not notice. */
    {
        static const Uint32 dfall[] =
            { 0x00007000UL, 0x80007000UL, 0x0000F000UL, 0xFFFFFFFFUL, 0x00001000UL };
        Uint16 d;

        for (d = 0; d < sizeof dfall / sizeof dfall[0]; d++)
        {
            reset_input();
            g_inFc.code.vfVoltageSrc = FUNCCODE_vfVoltageSrc_MULTI_SET;
            g_inDiFunc.f1.all = dfall[d];
            g_inMultiSet = 1234;
            g_inAim = 100;
            run_case();
        }
    }
}

/* ---- phase 3: the AI index ----------------------------------------------
 *
 * The module computes aiDeal[vfVoltageSrc - FUNCCODE_vfVoltageSrc_AI1].set. An
 * off-by-one in that subtraction is invisible unless the three AI entries hold
 * *different* values -- hence the distinct 1000/2000/3000 above and the wider
 * sweep here.
 */
static void phase_ai_index(void)
{
    static const int16 sets[][AI_NUMBER] =
    {
        { 1000, 2000, 3000 },
        { -1000, 0, 1000 },
        { 32767, -32768, 1 },
        { 0, 0, 0 },
        { -32768, -32768, -32768 },
    };
    Uint16 i, src, rv;

    for (i = 0; i < sizeof sets / sizeof sets[0]; i++)
    {
        for (src = FUNCCODE_vfVoltageSrc_AI1; src <= FUNCCODE_vfVoltageSrc_AI3; src++)
        {
            for (rv = 0; rv < 3; rv++)
            {
                static const Uint16 rvs[] = { 380, 65535, 1 };

                reset_input();
                g_inFc.code.vfVoltageSrc = src;
                g_inAi[0].set = sets[i][0];
                g_inAi[1].set = sets[i][1];
                g_inAi[2].set = sets[i][2];
                g_inFc.code.motorParaM1.elem.ratingVoltage = rvs[rv];
                g_inAim = 12345;
                run_case();
            }
        }
    }
}

/* ---- phase 4: the default: branch carries the previous aim ---------------
 *
 * Reachable, well-defined, and the one thing f_torqueCtrl could not test. The
 * incoming aim is non-zero and the source code is unmapped, so the only way both
 * sides agree on the *output* is if both leave the global alone.
 */
static void phase_default_carry(void)
{
    static const Uint16 dead[] = { 9, 10, 11, 12, 15, 100, 1000, 32767, 65535 };
    static const Uint16 aims[] = { 0, 1, 12345, 65535 };
    Uint16 i, j, k;

    for (i = 0; i < sizeof dead / sizeof dead[0]; i++)
    {
        for (j = 0; j < sizeof aims / sizeof aims[0]; j++)
        {
            for (k = 0; k < 2; k++)
            {
                reset_input();
                g_inFc.code.vfVoltageSrc = dead[i];
                g_inFc.code.vfCurve =
                    k ? FUNCCODE_vfCurve_HALF_SEPARATE : FUNCCODE_vfCurve_ALL_SEPARATE;
                g_inAim    = aims[j];
                g_inSepVol = 200;
                g_inRem    = 3;
                run_case();
            }
        }
    }
}

/* ---- phase 5: the scaling arithmetic ------------------------------------
 *
 * The three scalings are not interchangeable and each has a truncation point
 * worth pinning:
 *
 *   AI / PULSE   (int32)v * ratingVoltage >> 15      arithmetic shift, so
 *                                                    negative v rounds *down*,
 *                                                    unlike a division
 *   MULTI/PLC/PID (int32)v * ratingVoltage / maxFrq  a real division
 *   COMM         (int32)(int16)frqComm * rv / 10000  sign-extended first
 */
static void phase_scale(void)
{
    static const Uint16 rvs[] = { 0, 1, 220, 380, 400, 480, 690, 65535 };
    Uint16 i, j;

    /* AI and PULSE: the >> 15 path, negatives included. */
    for (i = 0; i < sizeof rvs / sizeof rvs[0]; i++)
    {
        for (j = 0; j < 8; j++)
        {
            static const int16 vals[] =
                { 0, 1, -1, 32767, -32768, 16384, -16384, 100 };
            Uint16 src;

            for (src = FUNCCODE_vfVoltageSrc_AI1; src <= FUNCCODE_vfVoltageSrc_AI3; src++)
            {
                reset_input();
                g_inFc.code.vfVoltageSrc = src;
                g_inAi[src - FUNCCODE_vfVoltageSrc_AI1].set = vals[j];
                g_inFc.code.motorParaM1.elem.ratingVoltage = rvs[i];
                g_inAim = 555;
                run_case();
            }

            reset_input();
            g_inFc.code.vfVoltageSrc = FUNCCODE_vfVoltageSrc_PULSE;
            g_inPulse = vals[j];
            g_inFc.code.motorParaM1.elem.ratingVoltage = rvs[i];
            g_inAim = 555;
            run_case();
        }
    }

    /* MULTI_SET / PLC / PID: the / maxFrq path. X is bounded to +/-30000 so the
     * product stays inside int32 (see note (d) in the header). */
    {
        static const int32 xs[] =
            { 0, 1, -1, 5000, -5000, 30000, -30000, 12345, -12345 };
        static const Uint16 maxFrqs[] = { 1, 2, 50, 5000, 30000, 65535 };
        Uint16 a, b;

        for (a = 0; a < sizeof xs / sizeof xs[0]; a++)
        {
            for (b = 0; b < sizeof maxFrqs / sizeof maxFrqs[0]; b++)
            {
                for (i = 0; i < sizeof rvs / sizeof rvs[0]; i++)
                {
                    Uint16 which;

                    for (which = 0; which < 3; which++)
                    {
                        reset_input();
                        g_inFc.code.vfVoltageSrc = (Uint16)(FUNCCODE_vfVoltageSrc_MULTI_SET + which);
                        g_inMaxFrq = maxFrqs[b];
                        g_inFc.code.motorParaM1.elem.ratingVoltage = rvs[i];
                        g_inMultiSet = xs[a];
                        g_inPlc      = xs[a];
                        g_inPid      = xs[a];
                        g_inDiFunc.f1.bit.multiSet = (Uint16)(a & 0xF);
                        g_inAim = 777;
                        run_case();
                    }
                }
            }
        }
    }

    /* COMM: the sign-extension path. frqComm above 32767 must behave like its
     * int16 reinterpretation, not like the unsigned value. */
    {
        static const Uint16 comms[] =
            { 0, 1, 32767, 32768, 40000, 65535, 10000, 50000 };

        for (i = 0; i < sizeof comms / sizeof comms[0]; i++)
        {
            for (j = 0; j < sizeof rvs / sizeof rvs[0]; j++)
            {
                reset_input();
                g_inFc.code.vfVoltageSrc = FUNCCODE_vfVoltageSrc_COMM;
                g_inFc.code.frqComm = comms[i];
                g_inFc.code.motorParaM1.elem.ratingVoltage = rvs[j];
                g_inAim = 888;
                run_case();
            }
        }
    }
}

/* ---- phase 6: HALF_SEPARATE --------------------------------------------
 *
 * vfSeprateVolAim = ((Uint32)2 * ABS_INT32(frq) * vfSeprateVolAim) / ratingFrq
 *
 * Two things to pin: the sign of frq must be discarded (ABS), and the division
 * is by motorFc's ratingFrq, not by funcCode's or by maxFrq.
 */
static void phase_half_separate(void)
{
    static const int32 frqs[] =
        { 0, 1, -1, 50, -50, 5000, -5000, 1000000, -1000000, 2147483647, -2147483647 };
    static const Uint16 rfqs[] = { 1, 50, 5000, 65535 };
    Uint16 i, j, k;

    for (i = 0; i < sizeof frqs / sizeof frqs[0]; i++)
    {
        for (j = 0; j < sizeof rfqs / sizeof rfqs[0]; j++)
        {
            for (k = 0; k < 2; k++)
            {
                reset_input();
                g_inFc.code.vfCurve = FUNCCODE_vfCurve_HALF_SEPARATE;
                g_inMotorFc.motorPara.elem.ratingFrq = rfqs[j];
                g_inFrq = frqs[i];

                if (k)
                {
                    /* AI source so the aim itself can be large/negative. */
                    g_inFc.code.vfVoltageSrc = FUNCCODE_vfVoltageSrc_AI2;
                    g_inAi[1].set = -20000;
                }
                else
                {
                    g_inFc.code.vfVoltageSrc = FUNCCODE_vfVoltageSrc_FC;
                    g_inFc.code.vfVoltageDigtalSet = 60000;
                }

                g_inAim = 4321;
                run_case();
            }
        }
    }
}

/* ---- phase 7: the clamp -------------------------------------------------
 *
 * The clamp compares against motorFc's rated voltage and writes the same value
 * into the filter's maxValue. funcCode's rated voltage is kept lower than
 * motorFc's throughout, so the two can never be confused for one another.
 */
static void phase_clamp(void)
{
    static const Uint16 aimSrc[] = { 0, 100, 399, 400, 401, 1000, 32767, 65535 };
    static const Uint16 motorRv[] = { 0, 1, 100, 400, 1000, 65535 };
    Uint16 i, j;

    for (i = 0; i < sizeof aimSrc / sizeof aimSrc[0]; i++)
    {
        for (j = 0; j < sizeof motorRv / sizeof motorRv[0]; j++)
        {
            reset_input();
            g_inFc.code.vfVoltageSrc = FUNCCODE_vfVoltageSrc_FC;
            g_inFc.code.vfVoltageDigtalSet = aimSrc[i];
            g_inMotorFc.motorPara.elem.ratingVoltage = motorRv[j];
            g_inFc.code.motorParaM1.elem.ratingVoltage = 100;   /* != motorRv */
            g_inAim = 321;
            run_case();
        }
    }
}

/* ---- phase 8: the run flag ---------------------------------------------
 *
 * runFlag.bit.run == 0 zeroes vfSeparateVol but *keeps* the freshly computed
 * vfSeprateVolAim, and skips the filter entirely. All three facts are observed.
 */
static void phase_runflag(void)
{
    static const Uint16 accs[] = { 0, 1, 50, 1000, 65535 };
    Uint16 i, j;

    for (i = 0; i < 2; i++)
    {
        for (j = 0; j < sizeof accs / sizeof accs[0]; j++)
        {
            reset_input();
            g_inRunFlag.bit.run = i;
            g_inFc.code.vfVoltageSrc = FUNCCODE_vfVoltageSrc_FC;
            g_inFc.code.vfVoltageDigtalSet = 350;
            g_inFc.code.vfVoltageAccTime = accs[j];
            g_inAim    = 654;
            g_inSepVol = 123;
            g_inRem    = 9;
            run_case();
        }
    }
}

/* ---- phase 9: the ramp --------------------------------------------------
 *
 * vfVoltageAccTime == 0 makes tickerAll zero, which is the filter's "snap to
 * aim" branch; anything else ramps. Both sides must agree on which branch and on
 * the resulting curValue, for a range of incoming curValue/aim/remainder.
 */
static void phase_ramp(void)
{
    Uint16 i;

    for (i = 0; i < 4000; i++)
    {
        reset_input();
        g_inFc.code.vfVoltageSrc = FUNCCODE_vfVoltageSrc_FC;
        g_inFc.code.vfVoltageDigtalSet = rnd();
        g_inFc.code.vfVoltageAccTime = (Uint16)(rnd() % 4);   /* mostly 0..3 */
        g_inMotorFc.motorPara.elem.ratingVoltage = (Uint16)(rnd() % 500);
        g_inFc.code.motorParaM1.elem.ratingVoltage = (Uint16)(rnd() % 500);

        g_inAim    = rnd();
        g_inSepVol = rnd();
        g_inRem    = (int32)rnd16();

        run_case();
    }
}

/* ---- phase 10: randomised full sweep ----------------------------------- */

static void phase_random(void)
{
    Uint16 i;

    for (i = 0; i < 30000; i++)
    {
        reset_input();

        g_inFc.code.vfCurve =
            (Uint16)(rnd() % 14);           /* covers 0..13, incl. 10 and 11 */
        g_inMotorFc.motorCtrlMode = (Uint16)(rnd() % 4);
        g_inFc.code.vfVoltageSrc  = (Uint16)(rnd() % 12);
        g_inFc.code.vfVoltageDigtalSet = rnd();
        g_inFc.code.vfVoltageAccTime   = (Uint16)(rnd() % 200);
        g_inFc.code.frqComm            = rnd();

        g_inAi[0].set = rnd16();
        g_inAi[1].set = rnd16();
        g_inAi[2].set = rnd16();
        g_inPulse     = rnd16();

        g_inDiFunc.f1.bit.multiSet = (Uint16)(rnd() % 16);
        g_inMultiSet = (int32)rnd16() / 2;
        g_inPlc      = (int32)rnd16() / 2;
        g_inPid      = (int32)rnd16() / 2;

        g_inMaxFrq = (Uint16)(1 + rnd() % 30000);
        g_inFrq    = (int32)(int16)rnd();

        g_inFc.code.motorParaM1.elem.ratingVoltage = (Uint16)(rnd() % 700);
        g_inMotorFc.motorPara.elem.ratingVoltage   = (Uint16)(rnd() % 700);
        g_inMotorFc.motorPara.elem.ratingFrq       = (Uint16)(1 + rnd() % 5000);

        g_inRunFlag.bit.run = (Uint16)(rnd() % 2);

        g_inAim    = rnd();
        g_inSepVol = rnd();
        g_inRem    = (int32)rnd16();

        run_case();
    }
}

/* ---- phase 11: stateful sequence ---------------------------------------
 *
 * The three carried quantities are fed back from the reference side, so the two
 * implementations walk the *same* trajectory and any divergence in how they
 * advance their own state shows up immediately instead of being masked by the
 * driver re-seeding the inputs.
 */
static void phase_sequence(void)
{
    static const Uint16 srcs[] =
    {
        FUNCCODE_vfVoltageSrc_FC,
        FUNCCODE_vfVoltageSrc_AI1,
        FUNCCODE_vfVoltageSrc_PULSE,
        FUNCCODE_vfVoltageSrc_MULTI_SET,
        FUNCCODE_vfVoltageSrc_PLC,
        FUNCCODE_vfVoltageSrc_PID,
        FUNCCODE_vfVoltageSrc_COMM,
        9,      /* unmapped: exercises the carry through default: */
    };
    Uint16 i;

    reset_input();
    g_inAim = 0;
    g_inSepVol = 0;
    g_inRem = 0;

    for (i = 0; i < 20000; i++)
    {
        /* Vary the environment but keep the carried state. */
        g_inFc.code.vfCurve =
            (i % 7 == 0) ? FUNCCODE_vfCurve_HALF_SEPARATE
                         : FUNCCODE_vfCurve_ALL_SEPARATE;
        g_inMotorFc.motorCtrlMode = FUNCCODE_motorCtrlMode_VF;
        g_inFc.code.vfVoltageSrc  = srcs[i % (sizeof srcs / sizeof srcs[0])];
        g_inFc.code.vfVoltageDigtalSet = (Uint16)(rnd() % 600);
        g_inFc.code.vfVoltageAccTime   = (Uint16)(rnd() % 60);
        g_inFc.code.frqComm            = rnd();
        g_inAi[0].set = rnd16();
        g_inAi[1].set = rnd16();
        g_inAi[2].set = rnd16();
        g_inPulse = rnd16();
        g_inDiFunc.f1.bit.multiSet = (Uint16)(rnd() % 16);
        g_inMultiSet = (int32)rnd16();
        g_inPlc      = (int32)rnd16();
        g_inPid      = (int32)rnd16();
        g_inMaxFrq = (Uint16)(1 + rnd() % 5000);
        g_inFrq    = (int32)(int16)rnd();
        g_inFc.code.motorParaM1.elem.ratingVoltage = (Uint16)(rnd() % 700);
        g_inMotorFc.motorPara.elem.ratingVoltage   = (Uint16)(rnd() % 700);
        g_inMotorFc.motorPara.elem.ratingFrq       = (Uint16)(1 + rnd() % 5000);
        g_inRunFlag.bit.run = (Uint16)((rnd() % 8) ? 1 : 0);

        run_case();

        if (g_fails)
        {
            return;
        }

        /* Walk the trajectory from the reference side. */
        g_inAim    = S[0].aim;
        g_inSepVol = S[0].sepVol;
        g_inRem    = S[0].lineRem;
    }
}

/* ---- main ---------------------------------------------------------------- */

int main(void)
{
    long base;

    init_ptrs();

    base = g_cases;
    phase_gate();
    printf("  gate (curve x mode)          : %ld\n", g_cases - base);

    base = g_cases;
    phase_src_sweep();
    printf("  source sweep 0..24           : %ld\n", g_cases - base);

    base = g_cases;
    phase_ai_index();
    printf("  AI index                     : %ld\n", g_cases - base);

    base = g_cases;
    phase_default_carry();
    printf("  default: state carry         : %ld\n", g_cases - base);

    base = g_cases;
    phase_scale();
    printf("  scaling arithmetic           : %ld\n", g_cases - base);

    base = g_cases;
    phase_half_separate();
    printf("  HALF_SEPARATE                : %ld\n", g_cases - base);

    base = g_cases;
    phase_clamp();
    printf("  clamp                        : %ld\n", g_cases - base);

    base = g_cases;
    phase_runflag();
    printf("  run flag                     : %ld\n", g_cases - base);

    base = g_cases;
    phase_ramp();
    printf("  ramp                         : %ld\n", g_cases - base);

    base = g_cases;
    phase_random();
    printf("  random                       : %ld\n", g_cases - base);

    base = g_cases;
    phase_sequence();
    printf("  stateful sequence            : %ld\n", g_cases - base);

    printf("f_vf difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    return g_fails ? 1 : 0;
}
