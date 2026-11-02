/* Differential test driver for f_torqueCtrl.
 *
 * The module is the torque reference chain of the drive:
 *
 *   TorqueCalc()          = UpdateTorqueAim() then AccDecTorqueCalc()
 *   UpdateTorqueAim()     picks the torque source, scales it, writes torqueAim
 *   AccDecTorqueCalc()    ramps upperTorque towards torqueAim through the
 *                         LINE_CHANGE_STRUCT filter
 *   TorqueLimitCalc()     watches the DC-bus current and the speed error and
 *                         reports "torque limit has been active long enough"
 *
 * Nothing in it touches a peripheral register. Its entire hardware boundary is
 * one function, LineChangeCalc, which the shared mock implements verbatim and
 * logs (see mock_torque.c / mock_torque.h).
 *
 * What is compared after every call:
 *
 *   return value            TorqueLimitCalc's 0/1 result
 *   upperTorque             the module's ramp output
 *   torqueAim               the module's scaled target
 *   torqurLine.*            all five fields of the filter state, individually
 *   torqueLog               the LineChangeCalc call sequence, in order
 *
 * torqurLine.calc is deliberately NOT compared: it is a function pointer and the
 * two sides are handed two different global structs, so its *value* is the same
 * but comparing it adds nothing over comparing the four data fields.
 *
 * ---- the hidden static state of TorqueLimitCalc ----
 *
 * TorqueLimitCalc owns two function-local statics, torqueLimitCurTicker and
 * torqueLimitSpdTicker. A driver cannot set them, so they are not "inputs" in
 * the usual sense -- they are state the module carries between calls. The only
 * way to compare them is to compare the *sequence* of return values: both sides
 * start from zeroed statics and receive an identical call sequence, so any
 * divergence in how the state is advanced shows up as a divergence in the
 * outputs. Phase 3 is therefore built as sequences rather than isolated calls,
 * and it deliberately runs long enough (400+ calls) to cross the
 * TORQUE_LIMIT_TIME / DO_CALC_PERIOD threshold that the reset/saturate branches
 * exist to implement.
 *
 * ---- the default: branch of UpdateTorqueAim ----
 *
 * UpdateTorqueAim's switch leaves `tmp` uninitialised when torqueSrc is outside
 * 0..7, and then does `torqueAim = tmp;`. That is pre-existing undefined
 * behaviour: the value stored depends on whatever the compiler happened to leave
 * in the register. The host cannot reproduce the target's value, and comparing
 * two host builds of it would be comparing two arbitrary numbers. So this driver
 * keeps torqueSrc inside 0..7 everywhere -- the reachable set, since the field is
 * a 3-bit-deep selector in the function-code table. The gap is listed in the
 * case README rather than quietly dropped.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Same include order as src/function/f_torqueCtrl.c. */
#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_io.h"
#include "f_main.h"
#include "f_p2p.h"

#include "mock_torque.h"

#define FAIL_LIMIT   20

/* The real value is TORQUE_LIMIT_TIME (800, defined in f_torqueCtrl.c itself)
 * divided by DO_CALC_PERIOD. TORQUE_LIMIT_TIME is file-local to the module, so it
 * cannot be included from here; it is repeated with the value checked against the
 * module's own behaviour by phase 3's threshold test -- if the module's constant
 * ever changed, phase 3's first assertion would fail loudly instead of silently
 * testing the wrong threshold. */
#define LIMIT_TICKS  (800 / DO_CALC_PERIOD)

/* ---- per-side exports ---------------------------------------------------- */

extern void   ref_TorqueCalc(void);
extern void   new_TorqueCalc(void);
extern void   ref_UpdateTorqueAim(void);
extern void   new_UpdateTorqueAim(void);
extern void   ref_AccDecTorqueCalc(void);
extern void   new_AccDecTorqueCalc(void);
extern Uint16 ref_TorqueLimitCalc(void);
extern Uint16 new_TorqueLimitCalc(void);

extern int16  ref_upperTorque, new_upperTorque;
extern int16  ref_torqueAim, new_torqueAim;
extern LINE_CHANGE_STRUCT ref_torqurLine, new_torqurLine;

/* ---- side plumbing ------------------------------------------------------- */

static int16  *g_upper[2];
static int16  *g_aim[2];
static LINE_CHANGE_STRUCT *g_line[2];

static void   (*g_torqueCalc[2])(void);
static void   (*g_updateAim[2])(void);
static void   (*g_accDec[2])(void);
static Uint16 (*g_limitCalc[2])(void);

static void init_ptrs(void)
{
    g_upper[0] = &ref_upperTorque;      g_upper[1] = &new_upperTorque;
    g_aim[0]   = &ref_torqueAim;        g_aim[1]   = &new_torqueAim;
    g_line[0]  = &ref_torqurLine;       g_line[1]  = &new_torqurLine;

    g_torqueCalc[0] = ref_TorqueCalc;   g_torqueCalc[1] = new_TorqueCalc;
    g_updateAim[0]  = ref_UpdateTorqueAim; g_updateAim[1] = new_UpdateTorqueAim;
    g_accDec[0]     = ref_AccDecTorqueCalc; g_accDec[1]   = new_AccDecTorqueCalc;
    g_limitCalc[0]  = ref_TorqueLimitCalc;  g_limitCalc[1] = new_TorqueLimitCalc;
}

struct SIDE
{
    Uint16 ret;
    int16  upperTorque;
    int16  torqueAim;
    int32  lineAim, lineCur, lineMax, lineRem;
    Uint32 lineTicker;
    Uint16 logN;
    struct TORQUECALL log[TORQUELOG_N];
};

static struct SIDE S[2];

/* ---- input state -------------------------------------------------------- */

static FUNCCODE_ALL      g_inFc;
static struct MOTOR_FC   g_inMotorFc;
static struct AI_DEAL    g_inAi[AI_NUMBER];
static int16             g_inPulse;
static Uint16            g_inRunMode;
static union RUN_FLAG    g_inRunFlag;
static Uint16            g_inItDisp;
static P2P_DATA_STRUCT   g_inP2p;
static Uint16            g_inCanRxTxCon;
static int32             g_inFrq, g_inFrqRun;
static int16             g_inUpperTorque, g_inTorqueAim;
static int32             g_inLineRem;

static void install(int side)
{
    memcpy(&funcCode,  &g_inFc,      sizeof g_inFc);
    memcpy(&motorFc,   &g_inMotorFc, sizeof g_inMotorFc);
    memcpy(aiDeal,     g_inAi,       sizeof g_inAi);
    pulseInSet  = g_inPulse;
    runMode     = g_inRunMode;
    runFlag     = g_inRunFlag;
    itDisp      = g_inItDisp;
    memcpy(&p2pData, &g_inP2p, sizeof g_inP2p);
    CanRxTxCon  = g_inCanRxTxCon;
    frq         = g_inFrq;
    frqRun      = g_inFrqRun;

    *g_upper[side] = g_inUpperTorque;
    *g_aim[side]   = g_inTorqueAim;

    /* Only `remainder` survives AccDecTorqueCalc's own field writes; the other
     * four are overwritten before the filter runs. It is still installed for all
     * of them so that a call which does NOT reach the writes (early return) is
     * compared against the same starting point on both sides. */
    g_line[side]->aimValue  = 0;
    g_line[side]->curValue  = 0;
    g_line[side]->maxValue  = 0;
    g_line[side]->tickerAll = 0;
    g_line[side]->remainder = g_inLineRem;

    torqueLogN = 0;
}

static void snapshot(int side)
{
    struct SIDE *s = &S[side];
    LINE_CHANGE_STRUCT *p = g_line[side];

    s->upperTorque = *g_upper[side];
    s->torqueAim   = *g_aim[side];
    s->lineAim     = p->aimValue;
    s->lineCur     = p->curValue;
    s->lineMax     = p->maxValue;
    s->lineTicker  = p->tickerAll;
    s->lineRem     = p->remainder;

    if (torqueLogN > TORQUELOG_N)
    {
        printf("FATAL: torqueLog overflow (%u > %d)\n", torqueLogN, TORQUELOG_N);
        exit(2);
    }
    s->logN = torqueLogN;
    memcpy(s->log, torqueLog, torqueLogN * sizeof(struct TORQUECALL));
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
    printf("  in: runMode=%u src=%u pu=%u run=%u itDisp=%u frq=%ld frqRun=%ld\n",
           g_inRunMode,
           (Uint16)(g_inRunMode == RUN_MODE_TORQUE_CTRL
                        ? g_inFc.code.driveUpperTorqueSrc
                        : g_inMotorFc.vcPara.spdCtrlDriveTorqueLimitSrc),
           (Uint16)(g_inRunMode == RUN_MODE_TORQUE_CTRL
                        ? g_inFc.code.driveUpperTorque
                        : g_inMotorFc.vcPara.spdCtrlDriveTorqueLimit),
           (Uint16)g_inRunFlag.bit.run, g_inItDisp,
           (long)g_inFrq, (long)g_inFrqRun);
    printf("      up=%d aim=%d rem=%ld\n",
           g_inUpperTorque, g_inTorqueAim, (long)g_inLineRem);
}

/* kind: 0 = TorqueCalc, 1 = UpdateTorqueAim, 2 = AccDecTorqueCalc,
 *       3 = TorqueLimitCalc. */
static void run_case(const char *fn, int kind)
{
    int side;

    g_cases++;

    for (side = 0; side < 2; side++)
    {
        install(side);

        switch (kind)
        {
        case 0:
            g_torqueCalc[side]();
            S[side].ret = 0;
            break;
        case 1:
            g_updateAim[side]();
            S[side].ret = 0;
            break;
        case 2:
            g_accDec[side]();
            S[side].ret = 0;
            break;
        default:
            S[side].ret = g_limitCalc[side]();
            break;
        }

        snapshot(side);
    }

    if (S[0].ret != S[1].ret)          { report(fn, "return value");   return; }
    if (S[0].upperTorque != S[1].upperTorque)
                                       { report(fn, "upperTorque");    return; }
    if (S[0].torqueAim != S[1].torqueAim)
                                       { report(fn, "torqueAim");      return; }
    if (S[0].lineAim != S[1].lineAim)  { report(fn, "torqurLine.aimValue");  return; }
    if (S[0].lineCur != S[1].lineCur)  { report(fn, "torqurLine.curValue");  return; }
    if (S[0].lineMax != S[1].lineMax)  { report(fn, "torqurLine.maxValue");  return; }
    if (S[0].lineTicker != S[1].lineTicker)
                                       { report(fn, "torqurLine.tickerAll"); return; }
    if (S[0].lineRem != S[1].lineRem)  { report(fn, "torqurLine.remainder"); return; }
    if (S[0].logN != S[1].logN)        { report(fn, "LineChangeCalc call count"); return; }
    if (S[0].logN && memcmp(S[0].log, S[1].log,
                            S[0].logN * sizeof(struct TORQUECALL)) != 0)
                                       { report(fn, "LineChangeCalc call log"); return; }
}

/* ---- input construction -------------------------------------------------- */

static Uint32 g_seed = 0x2468ACE0UL;

static Uint16 rnd(void)
{
    g_seed = g_seed * 1103515245UL + 12345UL;
    return (Uint16)(g_seed >> 16);
}

static int16 rnd16(void)
{
    return (int16)rnd();
}

/* Zero everything, then let the caller fill in the fields it cares about. */
static void reset_input(void)
{
    memset(&g_inFc, 0, sizeof g_inFc);
    memset(&g_inMotorFc, 0, sizeof g_inMotorFc);
    memset(g_inAi, 0, sizeof g_inAi);
    g_inPulse = 0;
    g_inRunMode = RUN_MODE_TORQUE_CTRL;
    g_inRunFlag.all = 0;
    g_inItDisp = 0;
    memset(&g_inP2p, 0, sizeof g_inP2p);
    g_inCanRxTxCon = 0;
    g_inFrq = 0;
    g_inFrqRun = 0;
    g_inUpperTorque = 0;
    g_inTorqueAim = 0;
    g_inLineRem = 0;
}

/* Route the (src, pu) pair to the field the module actually reads for this
 * runMode, so every phase can just name a source and a value. */
static void set_torque_source(Uint16 src, Uint16 pu)
{
    if (g_inRunMode == RUN_MODE_TORQUE_CTRL)
    {
        g_inFc.code.driveUpperTorqueSrc  = src;
        g_inFc.code.driveUpperTorque     = pu;
    }
    else
    {
        g_inMotorFc.vcPara.spdCtrlDriveTorqueLimitSrc = src;
        g_inMotorFc.vcPara.spdCtrlDriveTorqueLimit    = pu;
    }
}

/* Fill every sub-branch input with a distinct, non-trivial value, so that a
 * rewrite which reads the wrong field produces a different result rather than
 * accidentally the same one. */
static void fill_distinct(void)
{
    g_inAi[0].set = 0x1111;
    g_inAi[1].set = (int16)0x8222;      /* negative, exercises the sign path */
    g_inAi[2].set = 0x7333;
    g_inPulse     = 0x2444;
    g_inFc.code.plcEnable    = 1;
    g_inFc.code.plcTorqueSet = 0x3555;
    g_inCanRxTxCon           = P2P_COMM_SLAVE;
    g_inFc.code.p2pRevDataSel = P2P_REV_TORQUE_SET;
    g_inP2p.processValue     = 0x4666;
    g_inFc.code.frqComm      = 0x5777;
}

/* ---- phase 1: UpdateTorqueAim, the source switch ------------------------- */

/* Sweeps the switch exhaustively over its reachable selector range, for both
 * runMode values, with two different torquePu grids. The pu grid matters: the
 * AI and PULSE cases are `(value * pu) >> 15`, so pu == 0 and pu == 1 pin the
 * result in ways that hide a wrong shift; a mid-range pu is what makes the shift
 * amount observable. */
static void phase_update_aim(void)
{
    static const Uint16 SRC[] = {
        FUNCCODE_upperTorqueSrc_FC,
        FUNCCODE_upperTorqueSrc_AI1,
        FUNCCODE_upperTorqueSrc_AI2,
        FUNCCODE_upperTorqueSrc_AI3,
        FUNCCODE_upperTorqueSrc_PULSE,
        FUNCCODE_upperTorqueSrc_COMM,
        FUNCCODE_upperTorqueSrc_MIN_AI1_AI2,
        FUNCCODE_upperTorqueSrc_MAX_AI1_AI2,
    };
    static const Uint16 PU[] = { 0, 1, 2, 2000, 32767, 65535 };
    Uint16 mode;
    Uint16 s;
    Uint16 p;

    for (mode = 0; mode < 2; mode++)
    {
        g_inRunMode = mode ? RUN_MODE_TORQUE_CTRL : RUN_MODE_SPEED_CTRL;

        for (s = 0; s < (Uint16)(sizeof SRC / sizeof SRC[0]); s++)
        {
            for (p = 0; p < (Uint16)(sizeof PU / sizeof PU[0]); p++)
            {
                reset_input();
                g_inRunMode = mode ? RUN_MODE_TORQUE_CTRL : RUN_MODE_SPEED_CTRL;
                set_torque_source(SRC[s], PU[p]);
                fill_distinct();
                run_case("UpdateTorqueAim", 1);
            }
        }
    }
}

/* The COMM case is a three-way chain (PLC -> P2P -> frqComm fallback). Its three
 * selectors are independent, so the exhaustive 2x2x2 sweep above cannot reach
 * every combination -- this one does, at two pu values. */
static void phase_comm_chain(void)
{
    static const Uint16 PU[] = { 2000, 40000 };
    Uint16 plc;
    Uint16 can;
    Uint16 rev;
    Uint16 p;

    for (p = 0; p < (Uint16)(sizeof PU / sizeof PU[0]); p++)
    {
        for (plc = 0; plc < 2; plc++)
        {
            for (can = 0; can < 2; can++)
            {
                for (rev = 0; rev < 2; rev++)
                {
                    reset_input();
                    g_inRunMode = RUN_MODE_TORQUE_CTRL;
                    set_torque_source(FUNCCODE_upperTorqueSrc_COMM, PU[p]);
                    fill_distinct();

                    g_inFc.code.plcEnable = plc;
                    g_inCanRxTxCon = can ? P2P_COMM_SLAVE : 0;
                    g_inFc.code.p2pRevDataSel =
                        rev ? P2P_REV_TORQUE_SET : P2P_REV_FRQ_SET;

                    run_case("UpdateTorqueAim", 1);
                }
            }
        }
    }
}

/* ---- phase 2: AccDecTorqueCalc ------------------------------------------- */

/* The ramp has three early exits (wrong mode, not running, already at target) and
 * two time constants chosen by the sign pair (upperTorque, torqueAim). Each is
 * enumerated here; the pu/time grids are chosen so the division and the modulo
 * inside the filter both produce non-trivial values. */
static void phase_accdec(void)
{
    static const int16 UP[]  = { 0, 1, -1, 2000, -2000, 32767, -32768 };
    static const int16 AIM[] = { 0, 1, -1, 1999, -1999, 32767, -32768 };
    static const Uint16 T[]  = { 0, 1, 2, 5, 100, 1000, 65535 };
    static const int32 REM[] = { 0, 1, -1, 999, -999, 2000 };
    Uint16 u;
    Uint16 a;
    Uint16 t;
    Uint16 r;

    /* (a) wrong mode: upperTorque = torqueAim, unconditional. */
    for (u = 0; u < (Uint16)(sizeof UP / sizeof UP[0]); u++)
    {
        reset_input();
        g_inRunMode = RUN_MODE_SPEED_CTRL;
        g_inUpperTorque = UP[u];
        g_inTorqueAim   = 12345;
        run_case("AccDecTorqueCalc", 2);
    }

    /* (b) torque mode, not running: upperTorque = 0. */
    for (u = 0; u < (Uint16)(sizeof UP / sizeof UP[0]); u++)
    {
        reset_input();
        g_inRunMode = RUN_MODE_TORQUE_CTRL;
        g_inRunFlag.bit.run = 0;
        g_inUpperTorque = UP[u];
        g_inTorqueAim   = 12345;
        run_case("AccDecTorqueCalc", 2);
    }

    /* (c) torque mode, running: the full sign-pair x time-constant x remainder
     * grid. This is where the acc/dec branch selection and the tickerAll
     * computation are decided. */
    for (u = 0; u < (Uint16)(sizeof UP / sizeof UP[0]); u++)
    {
        for (a = 0; a < (Uint16)(sizeof AIM / sizeof AIM[0]); a++)
        {
            for (t = 0; t < (Uint16)(sizeof T / sizeof T[0]); t++)
            {
                for (r = 0; r < (Uint16)(sizeof REM / sizeof REM[0]); r++)
                {
                    reset_input();
                    g_inRunMode = RUN_MODE_TORQUE_CTRL;
                    g_inRunFlag.bit.run = 1;
                    g_inUpperTorque = UP[u];
                    g_inTorqueAim   = AIM[a];
                    g_inFc.code.torqueCtrlAccTime = T[t];
                    g_inFc.code.torqueCtrlDecTime = T[(t + 3) % 7];
                    g_inLineRem = REM[r];
                    run_case("AccDecTorqueCalc", 2);
                }
            }
        }
    }
}

/* ---- phase 3: TorqueLimitCalc -------------------------------------------- */

/* See the file header: the two tickers are unreachable statics, so this phase is
 * built from sequences, and the threshold is crossed on purpose rather than left
 * to luck.
 *
 * `itDisp`'s threshold is (int32)(ABS_INT32(upperTorque) * 85) / 100. It is
 * recomputed here with the module's own expression so the sweep lands exactly on
 * limit-1 / limit / limit+1; a mutation to the 85 or the 100 moves the boundary
 * and flips one of those three calls. */

/* Both conditions false -> both tickers reset to 0, whatever they were. This is
 * the only way to put the statics into a known state from the driver. */
static void limit_reset(void)
{
    reset_input();
    g_inRunMode = RUN_MODE_TORQUE_CTRL;
    g_inItDisp = 0;
    g_inUpperTorque = 0;
    g_inFrq = 0;
    g_inFrqRun = 0;
    run_case("TorqueLimitCalc", 3);
}

/* One increment can never reach the threshold, so a boundary probe that starts
 * from a freshly reset counter is invisible: the call takes the increment branch
 * or the reset branch, and in both cases the counter is far below LIMIT_TICKS and
 * ret stays 0. The negative control found exactly that -- the first version of
 * this phase reset the counters and then probed the boundary, and the two
 * mutations that change the itDisp comparison were MISSED.
 *
 * The fix is to preload: drive both counters to LIMIT_TICKS-1 with the increment
 * branch, so the *next* call's decision on either condition is what decides
 * whether the counter reaches the threshold -- and therefore what ret is.
 *
 * The preload itself is a plain sequence of ordinary cases, so it is compared
 * like everything else; it is not a privileged setup path. */
static void limit_preload(int16 upperTorque, int32 frq, int32 frqRun, Uint16 itDisp)
{
    Uint16 i;

    for (i = 0; i < (Uint16)(LIMIT_TICKS - 1); i++)
    {
        reset_input();
        g_inRunMode = RUN_MODE_TORQUE_CTRL;
        g_inItDisp = itDisp;
        g_inUpperTorque = upperTorque;
        g_inFrq = frq;
        g_inFrqRun = frqRun;
        run_case("TorqueLimitCalc", 3);
    }
}

/* One probe call, with the counters left wherever the previous sequence put them. */
static void limit_probe(Uint16 itDisp, int16 upperTorque, int32 frq, int32 frqRun)
{
    reset_input();
    g_inRunMode = RUN_MODE_TORQUE_CTRL;
    g_inItDisp = itDisp;
    g_inUpperTorque = upperTorque;
    g_inFrq = frq;
    g_inFrqRun = frqRun;
    run_case("TorqueLimitCalc", 3);
}

static void phase_limit(void)
{
    static const int16 UP[] = { 0, 1, -1, 1000, -1000, 2000, 32767, -32768 };
    Uint16 u;
    Uint16 i;

    /* (a) the threshold crossing and the saturating increment.
     *
     * Both conditions true for LIMIT_TICKS+10 consecutive calls: the first
     * LIMIT_TICKS-1 return 0 and every later call returns 1. This is the only
     * place the threshold itself is crossed, so it is what pins LIMIT_TICKS to
     * the module's real TORQUE_LIMIT_TIME / DO_CALC_PERIOD. */
    limit_reset();
    for (i = 0; i < (Uint16)(LIMIT_TICKS + 10); i++)
    {
        limit_probe(65535, 0, 10000, 0);
    }

    /* (b) cur saturated, then spd driven up from a reset.
     *
     * Both counters advance together in (a), so cur is the binding constraint
     * there and the *value* spd is reset to is unobservable. Here cur is pinned
     * above the threshold first, which leaves spd as the only thing that can move
     * ret -- so the call on which spd crosses the threshold, and hence the reset
     * value it started from, becomes observable. */
    limit_reset();
    for (i = 0; i < (Uint16)(LIMIT_TICKS + 5); i++)
    {
        limit_probe(65535, 0, 0, 0);        /* cur -> 401, spd held at its reset */
    }
    for (i = 0; i < (Uint16)(LIMIT_TICKS + 5); i++)
    {
        limit_probe(65535, 0, 10000, 0);    /* spd -> 400+ from the reset value */
    }

    /* (c) the mirror image: spd saturated, cur driven up from a reset. */
    limit_reset();
    for (i = 0; i < (Uint16)(LIMIT_TICKS + 5); i++)
    {
        limit_probe(0, 0, 10000, 0);        /* spd -> 401, cur held at its reset */
    }
    for (i = 0; i < (Uint16)(LIMIT_TICKS + 5); i++)
    {
        limit_probe(65535, 0, 10000, 0);    /* cur -> 400+ from the reset value */
    }

    /* (d) the itDisp boundary, preloaded so the probe call decides ret.
     *
     * limit is recomputed with the module's own expression, so the sweep lands
     * exactly on limit-2..limit+2; a mutation to the 85 or the 100 moves the
     * boundary and flips one of those five calls. */
    for (u = 0; u < (Uint16)(sizeof UP / sizeof UP[0]); u++)
    {
        int32 limit = ((int32)(ABS_INT32(UP[u]) * 85)) / 100;
        int16 d;

        for (d = -2; d <= 2; d++)
        {
            limit_reset();
            limit_preload(UP[u], 10000, 0, 65535);
            limit_probe((Uint16)(int32)(limit + d), UP[u], 10000, 0);
        }
    }

    /* (e) the speed-error boundary around +/-200, preloaded the same way. The
     * module compares ABS_INT32(frq - frqRun) > 200, so 200 must not count and
     * 201 must. */
    {
        static const int32 D[] = { 199, 200, 201, -199, -200, -201 };
        Uint16 k;

        for (k = 0; k < (Uint16)(sizeof D / sizeof D[0]); k++)
        {
            limit_reset();
            limit_preload(0, 10000, 0, 65535);
            limit_probe(65535, 0,
                        D[k] >= 0 ? D[k] : 0,
                        D[k] >= 0 ? 0 : -D[k]);
        }
    }

    /* (f) a long randomised run. Every call both reads and advances the hidden
     * statics, so a swapped counter, a wrong reset or a drifting threshold shows
     * up as a divergence in the output sequence. 4000 calls is comfortably more
     * than the 400 needed to reach the threshold from any reset. */
    for (i = 0; i < 4000; i++)
    {
        reset_input();
        g_inRunMode = RUN_MODE_TORQUE_CTRL;
        g_inItDisp = (Uint16)(rnd() % 5000);
        g_inUpperTorque = rnd16();
        g_inFrq = (int32)(int16)rnd();
        g_inFrqRun = (int32)(int16)rnd();
        run_case("TorqueLimitCalc", 3);
    }
}

/* ---- phase 4: TorqueCalc (the composite entry point) --------------------- */

/* TorqueCalc is UpdateTorqueAim followed by AccDecTorqueCalc, so its value is
 * mostly in checking that the *pairing* survives a rewrite -- e.g. that the
 * rewrite did not reorder them or drop one. Randomised over both modes. */
static void phase_composite(void)
{
    Uint16 i;

    for (i = 0; i < 20000; i++)
    {
        reset_input();
        g_inRunMode = (Uint16)(rnd() % 2);
        set_torque_source((Uint16)(rnd() % 8), rnd());
        fill_distinct();
        g_inRunFlag.bit.run = (Uint16)(rnd() % 2);
        g_inUpperTorque = rnd16();
        g_inTorqueAim   = rnd16();
        g_inFc.code.torqueCtrlAccTime = rnd();
        g_inFc.code.torqueCtrlDecTime = rnd();
        g_inLineRem = (int32)rnd16();
        run_case("TorqueCalc", 0);
    }
}

/* ---- phase 5: randomised source coverage for UpdateTorqueAim -------------- */

static void phase_random_aim(void)
{
    Uint16 i;

    for (i = 0; i < 30000; i++)
    {
        reset_input();
        g_inRunMode = (Uint16)(rnd() % 2);
        set_torque_source((Uint16)(rnd() % 8), rnd());
        g_inAi[0].set = rnd16();
        g_inAi[1].set = rnd16();
        g_inAi[2].set = rnd16();
        g_inPulse = rnd16();
        g_inFc.code.plcEnable = (Uint16)(rnd() % 2);
        g_inFc.code.plcTorqueSet = rnd();
        g_inCanRxTxCon = (Uint16)(rnd() % 4);
        g_inFc.code.p2pRevDataSel = (Uint16)(rnd() % 2);
        g_inP2p.processValue = rnd();
        g_inFc.code.frqComm = rnd();
        run_case("UpdateTorqueAim", 1);
    }
}

/* ---- main ---------------------------------------------------------------- */

int main(void)
{
    long base;

    init_ptrs();

    base = g_cases;
    phase_update_aim();
    printf("  UpdateTorqueAim (源选择扫描) : %ld\n", g_cases - base);

    base = g_cases;
    phase_comm_chain();
    printf("  UpdateTorqueAim (COMM 三级链) : %ld\n", g_cases - base);

    base = g_cases;
    phase_random_aim();
    printf("  UpdateTorqueAim (随机)       : %ld\n", g_cases - base);

    base = g_cases;
    phase_accdec();
    printf("  AccDecTorqueCalc             : %ld\n", g_cases - base);

    base = g_cases;
    phase_limit();
    printf("  TorqueLimitCalc              : %ld\n", g_cases - base);

    base = g_cases;
    phase_composite();
    printf("  TorqueCalc                   : %ld\n", g_cases - base);

    printf("f_torqueCtrl difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    return g_fails ? 1 : 0;
}
