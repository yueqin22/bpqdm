/* Differential-test driver for MotorInvProtect.
 *
 * The frozen baseline (cases/MotorInvProtect/ref/MotorInvProtect_ref.c) and the
 * delivered source (src/motor/MotorInvProtect.c) are compiled into two symbol
 * namespaces, ref_* and new_*. Both are handed identical inputs and every
 * observable is compared after every call.
 *
 * ---------------------------------------------------------------------------
 * Why this one is per-function with a shared-state round trip
 * ---------------------------------------------------------------------------
 * The module exports one integration entry (InvDeviceControl, which just calls
 * the other twelve in a fixed order) plus twelve independent steps. Each step
 * reads only global state and writes only global state -- there is no hidden
 * local state machine -- so each one can be driven directly with a hand-built
 * input image. InvDeviceControl is still exercised separately, because "did the
 * order survive" is not something the per-step phases can see.
 *
 * The ref and new sides share one physical copy of every input the module does
 * NOT own (peripherals and other modules' globals). So do_step() snapshots that
 * shared image, runs ref, snapshots again, *restores the input image*, runs
 * new, and compares -- otherwise the ref side's writes would leak into the new
 * side and mask a difference.
 *
 * ---------------------------------------------------------------------------
 * Turning constants into inputs
 * ---------------------------------------------------------------------------
 * Several branches are gated on a field the module only *reads* here even
 * though in the running system another module writes it. Those are inputs, and
 * the driver pokes them directly so every branch is reachable:
 *
 *     gCBCProtect.Flag.bit.CBC_U/V/W   -> the CBC accumulation branches. The
 *         module gets them from CBCLimitCurPrepare(), which is a callee we
 *         cannot model per-side (see mock_invprotect.c group 3); promoting the
 *         flags to inputs is strictly more coverage, not less.
 *     gLoadLose.ErrCnt / ChkLevel / ChkTime -> the load-loss threshold
 *     gInLose.UpCnt / UpCntRes / CntRes -> the input-phase-loss counters
 *     gOverLoad.Cnt / InvTotal / MotorTotal -> the overload integrator state
 *     gFanCtrl.EnableCnt / RunCnt -> the fan hysteresis
 *
 * ---------------------------------------------------------------------------
 * Division-by-zero is a real input-space hazard, not a style point
 * ---------------------------------------------------------------------------
 * Three divisions have a register as the divisor:
 *     OverLoadProtect   / gComPar.MotorOvLoad        / gInvInfo.InvCurrForP
 *     CBCLimitCurProtect / gInvInfo.InvCurrent
 *     LoadLoseDetect    / gMotorInfo.Current
 * and OutputPhaseLoseDetect divides by gPhaseLose.Cnt after a guard that admits
 * Cnt == 0 (when Time >= 60e6). A zero there does not produce a "mismatch", it
 * kills the process, which would silently destroy the whole ledger. So the
 * seed helpers keep every divisor non-zero, and seed_phase_lose() additionally
 * guarantees Total/Cnt >= 1 so the m_Max/m_Min split can never divide by zero.
 *
 * ---------------------------------------------------------------------------
 * dirty_* discipline
 * ---------------------------------------------------------------------------
 * The comparison only sees what the NEW side leaves behind. A mutation that
 * makes new *stop writing* a field is invisible if the pre-existing value
 * already equals what correct code writes. So every private and shared word is
 * pre-filled with 0xA5 before each scenario instead of being left at zero.
 */

#include <stdio.h>
#include <string.h>

/* <stdlib.h> is deliberately NOT included. The case shim folds `int` to the
 * target's 16 bits, and stdlib's 32-bit `int abs(int)` would then collide with
 * the project's folded 16-bit prototype in SubPrgInclude.h (SubPrg's driver hit
 * the same thing and declares the one function it needs itself). */
extern long atol(const char *);

#include "MotorInvProtectInclude.h"

/* ---- renamed entry points ------------------------------------------------ */

extern void ref_InvDeviceControl(void);
extern void ref_SoftWareErrorDeal(void);
extern void ref_TemperatureCheck(void);
extern void ref_OutputPhaseLoseDetect(void);
extern void ref_OutputLoseAdd(void);
extern void ref_OutputLoseReset(void);
extern void ref_InputPhaseLoseDetect(void);
extern void ref_ControlFan(void);
extern void ref_OverLoadProtect(void);
extern void ref_CBCLimitCurProtect(void);
extern void ref_SetCBCEnable(void);
extern void ref_SetCBCDisable(void);
extern void ref_LoadLoseDetect(void);
extern void ref_BufferResProtect(void);

extern void new_InvDeviceControl(void);
extern void new_SoftWareErrorDeal(void);
extern void new_TemperatureCheck(void);
extern void new_OutputPhaseLoseDetect(void);
extern void new_OutputLoseAdd(void);
extern void new_OutputLoseReset(void);
extern void new_InputPhaseLoseDetect(void);
extern void new_ControlFan(void);
extern void new_OverLoadProtect(void);
extern void new_CBCLimitCurProtect(void);
extern void new_SetCBCEnable(void);
extern void new_SetCBCDisable(void);
extern void new_LoadLoseDetect(void);
extern void new_BufferResProtect(void);

/* ---- private per-side state (owned by the module) ------------------------ */

extern OVER_LOAD_PROTECT                     ref_gOverLoad;
extern PHASE_LOSE_STRUCT                     ref_gPhaseLose;
extern INPUT_LOSE_STRUCT                     ref_gInLose;
extern LOAD_LOSE_STRUCT                      ref_gLoadLose;
extern FAN_CTRL_STRUCT                       ref_gFanCtrl;
extern Ulong                                 ref_gBuffResCnt;
extern CBC_PROTECT_STRUCT                    ref_gCBCProtect;
extern struct FAULT_CODE_INFOR_STRUCT_DEF    ref_gError;

extern OVER_LOAD_PROTECT                     new_gOverLoad;
extern PHASE_LOSE_STRUCT                     new_gPhaseLose;
extern INPUT_LOSE_STRUCT                     new_gInLose;
extern LOAD_LOSE_STRUCT                      new_gLoadLose;
extern FAN_CTRL_STRUCT                       new_gFanCtrl;
extern Ulong                                 new_gBuffResCnt;
extern CBC_PROTECT_STRUCT                    new_gCBCProtect;
extern struct FAULT_CODE_INFOR_STRUCT_DEF    new_gError;

/* ---- mock-owned ---------------------------------------------------------- */

extern volatile struct ADC_RESULT_REGS AdcResult;
extern volatile struct GPIO_DATA_REGS  GpioDataRegs;
extern volatile struct EPWM_REGS       EPwm1Regs;
extern volatile struct EPWM_REGS       EPwm2Regs;
extern volatile struct EPWM_REGS       EPwm3Regs;
extern volatile Uint16                 gHostCpuCtl[7];
extern Uint16                          gCbcPrepCount;

/* ---- the module's own const tables (read-only; needed by the equivalence
 * pins in main) ------------------------------------------------------------ */

extern const Uint ref_gInvOverLoadTable[9];
extern const Uint ref_gInvOverLoadTableForP[9];
extern const Uint ref_gMotorOverLoadTable[7];
extern const Uint ref_gTempTableP44X[23];
extern const Uint ref_gTempTableP8XX[23];
extern const Uint ref_gTempTableBSMXX[23];
extern const Uint ref_gTempTableSEMIKON[23];
extern const Uint ref_gTempTableWAIZHI[23];

/* ---- counters ------------------------------------------------------------ */

static long g_cases;
static long g_mism;
static long g_mismShown;

/* input-reach counters, asserted at the end (see main) */
static long g_covTemp1, g_covTemp2, g_covTemp3, g_covTemp4;
static long g_covOvScale, g_covOvGp0, g_covOvGp1;
static long g_covOutEnter, g_covInRes, g_covCbcTrip, g_covFanOn, g_covLowPower;
static long g_covTempEdge, g_covOutEdge, g_covOvEdge;
static long g_covCbcEdge, g_covLlEdge;
static long g_covTempTable;

/* ------------------------------------------------------------------------- */
/* shared state (owned by other modules, read and written by the module)     */
/* ------------------------------------------------------------------------- */

struct SHARED {
    BASE_COMMAND_STRUCT      mainCmd;
    RUN_STATUS_STRUCT        mainStatus;
    INV_STRUCT               invInfo;
    LINE_CURRENT_STRUCT      lineCur;
    MOTOR_STRUCT             motorInfo;
    TEMPLETURE_STRUCT        temperature;
    PWM_OUT_STRUCT           pwm;
    ANGLE_STRUCT             phase;
    UVW_STRUCT               iuvw12;
    UVW_STRUCT_Q24           iuvw24;
    SUB_COMMAND_UNION        subCommand;
    UDC_STRUCT               udc;
    COM_PAR_INFO_STRUCT      comPar;
    struct ADC_RESULT_REGS   adc;
    struct GPIO_DATA_REGS    gpio;
    struct EPWM_REGS         epwm1;
    struct EPWM_REGS         epwm2;
    struct EPWM_REGS         epwm3;
    Uint16                   cpu[7];
};

static struct SHARED sRef;
static struct SHARED sNew;
static struct SHARED sIn;

static void shared_save(struct SHARED *s)
{
    memset(s, 0, sizeof(*s));
    s->mainCmd     = gMainCmd;
    s->mainStatus  = gMainStatus;
    s->invInfo     = gInvInfo;
    s->lineCur     = gLineCur;
    s->motorInfo   = gMotorInfo;
    s->temperature = gTemperature;
    s->pwm         = gPWM;
    s->phase       = gPhase;
    s->iuvw12      = gIUVWQ12;
    s->iuvw24      = gIUVWQ24;
    s->subCommand  = gSubCommand;
    s->udc         = gUDC;
    s->comPar      = gComPar;
    memcpy(&s->adc,   (const void *)&AdcResult,   sizeof(s->adc));
    memcpy(&s->gpio,  (const void *)&GpioDataRegs, sizeof(s->gpio));
    memcpy(&s->epwm1, (const void *)&EPwm1Regs,   sizeof(s->epwm1));
    memcpy(&s->epwm2, (const void *)&EPwm2Regs,   sizeof(s->epwm2));
    memcpy(&s->epwm3, (const void *)&EPwm3Regs,   sizeof(s->epwm3));
    memcpy(s->cpu, (const void *)gHostCpuCtl, sizeof(s->cpu));
}

static void shared_load(const struct SHARED *s)
{
    gMainCmd     = s->mainCmd;
    gMainStatus  = s->mainStatus;
    gInvInfo     = s->invInfo;
    gLineCur     = s->lineCur;
    gMotorInfo   = s->motorInfo;
    gTemperature = s->temperature;
    gPWM         = s->pwm;
    gPhase       = s->phase;
    gIUVWQ12     = s->iuvw12;
    gIUVWQ24     = s->iuvw24;
    gSubCommand  = s->subCommand;
    gUDC         = s->udc;
    gComPar      = s->comPar;
    memcpy((void *)&AdcResult,   &s->adc,   sizeof(s->adc));
    memcpy((void *)&GpioDataRegs, &s->gpio,  sizeof(s->gpio));
    memcpy((void *)&EPwm1Regs,   &s->epwm1, sizeof(s->epwm1));
    memcpy((void *)&EPwm2Regs,   &s->epwm2, sizeof(s->epwm2));
    memcpy((void *)&EPwm3Regs,   &s->epwm3, sizeof(s->epwm3));
    memcpy((void *)gHostCpuCtl, s->cpu, sizeof(s->cpu));
}

/* ------------------------------------------------------------------------- */
/* private state                                                            */
/* ------------------------------------------------------------------------- */

struct PRIV {
    OVER_LOAD_PROTECT                  overLoad;
    PHASE_LOSE_STRUCT                  phaseLose;
    INPUT_LOSE_STRUCT                  inLose;
    LOAD_LOSE_STRUCT                   loadLose;
    FAN_CTRL_STRUCT                    fanCtrl;
    Ulong                              buffResCnt;
    CBC_PROTECT_STRUCT                 cbc;
    struct FAULT_CODE_INFOR_STRUCT_DEF error;
};

static struct PRIV pRef;
static struct PRIV pNew;

static void priv_snap(struct PRIV *p, int isref)
{
    memset(p, 0, sizeof(*p));
    if (isref)
    {
        p->overLoad   = ref_gOverLoad;
        p->phaseLose  = ref_gPhaseLose;
        p->inLose     = ref_gInLose;
        p->loadLose   = ref_gLoadLose;
        p->fanCtrl    = ref_gFanCtrl;
        p->buffResCnt = ref_gBuffResCnt;
        p->cbc        = ref_gCBCProtect;
        p->error      = ref_gError;
    }
    else
    {
        p->overLoad   = new_gOverLoad;
        p->phaseLose  = new_gPhaseLose;
        p->inLose     = new_gInLose;
        p->loadLose   = new_gLoadLose;
        p->fanCtrl    = new_gFanCtrl;
        p->buffResCnt = new_gBuffResCnt;
        p->cbc        = new_gCBCProtect;
        p->error      = new_gError;
    }
}

/* Copy the whole private image ref -> new. Called once per scenario, *after*
 * all the input pokes, so both sides are handed byte-identical state. */
static void sync_priv(void)
{
    new_gOverLoad   = ref_gOverLoad;
    new_gPhaseLose  = ref_gPhaseLose;
    new_gInLose     = ref_gInLose;
    new_gLoadLose   = ref_gLoadLose;
    new_gFanCtrl    = ref_gFanCtrl;
    new_gBuffResCnt = ref_gBuffResCnt;
    new_gCBCProtect = ref_gCBCProtect;
    new_gError      = ref_gError;
}

/* ------------------------------------------------------------------------- */
/* entry-point dispatch                                                     */
/* ------------------------------------------------------------------------- */

enum {
    F_INVDEV = 0, F_SWERR, F_TEMP, F_OUTPLD, F_OUTADD, F_OUTRST, F_INPLD,
    F_FAN, F_OVERLOAD, F_CBC, F_CBCEN, F_CBCDIS, F_LOADLOSE, F_BUFFRES,
    F_COUNT
};

static void call_ref(int fn)
{
    switch (fn)
    {
        case F_INVDEV:   ref_InvDeviceControl();      break;
        case F_SWERR:    ref_SoftWareErrorDeal();     break;
        case F_TEMP:     ref_TemperatureCheck();      break;
        case F_OUTPLD:   ref_OutputPhaseLoseDetect(); break;
        case F_OUTADD:   ref_OutputLoseAdd();         break;
        case F_OUTRST:   ref_OutputLoseReset();       break;
        case F_INPLD:    ref_InputPhaseLoseDetect();  break;
        case F_FAN:      ref_ControlFan();            break;
        case F_OVERLOAD: ref_OverLoadProtect();       break;
        case F_CBC:      ref_CBCLimitCurProtect();    break;
        case F_CBCEN:    ref_SetCBCEnable();          break;
        case F_CBCDIS:   ref_SetCBCDisable();         break;
        case F_LOADLOSE: ref_LoadLoseDetect();        break;
        default:         ref_BufferResProtect();      break;
    }
}

static void call_new(int fn)
{
    switch (fn)
    {
        case F_INVDEV:   new_InvDeviceControl();      break;
        case F_SWERR:    new_SoftWareErrorDeal();     break;
        case F_TEMP:     new_TemperatureCheck();      break;
        case F_OUTPLD:   new_OutputPhaseLoseDetect(); break;
        case F_OUTADD:   new_OutputLoseAdd();         break;
        case F_OUTRST:   new_OutputLoseReset();       break;
        case F_INPLD:    new_InputPhaseLoseDetect();  break;
        case F_FAN:      new_ControlFan();            break;
        case F_OVERLOAD: new_OverLoadProtect();       break;
        case F_CBC:      new_CBCLimitCurProtect();    break;
        case F_CBCEN:    new_SetCBCEnable();          break;
        case F_CBCDIS:   new_SetCBCDisable();         break;
        case F_LOADLOSE: new_LoadLoseDetect();        break;
        default:         new_BufferResProtect();      break;
    }
}

/* ------------------------------------------------------------------------- */
/* comparison                                                               */
/* ------------------------------------------------------------------------- */

static void dump_first_diff(const char *tag, const char *what,
                            const void *a, const void *b, size_t n)
{
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    size_t i;

    for (i = 0; i < n; i++)
    {
        if (pa[i] != pb[i])
        {
            printf("        %s: %s byte %u  ref=0x%02X new=0x%02X\n",
                   tag, what, (unsigned)i, pa[i], pb[i]);
            return;
        }
    }
}

#define CMPM(a, b, what)                                                   \
    do {                                                                   \
        if (memcmp(&(a), &(b), sizeof(a)) != 0)                            \
        {                                                                  \
            g_mism++;                                                      \
            if (g_mismShown < 40)                                          \
            {                                                              \
                g_mismShown++;                                             \
                printf("MISMATCH %s: %s\n", tag, what);                    \
                dump_first_diff(tag, what, &(a), &(b), sizeof(a));         \
            }                                                              \
        }                                                                  \
    } while (0)

static long g_cbcRef, g_cbcNew;

static void compare(const char *tag)
{
    CMPM(sRef.mainCmd,     sNew.mainCmd,     "gMainCmd");
    CMPM(sRef.mainStatus,  sNew.mainStatus,  "gMainStatus");
    CMPM(sRef.invInfo,     sNew.invInfo,     "gInvInfo");
    CMPM(sRef.lineCur,     sNew.lineCur,     "gLineCur");
    CMPM(sRef.motorInfo,   sNew.motorInfo,   "gMotorInfo");
    CMPM(sRef.temperature, sNew.temperature, "gTemperature");
    CMPM(sRef.pwm,         sNew.pwm,         "gPWM");
    CMPM(sRef.phase,       sNew.phase,       "gPhase");
    CMPM(sRef.iuvw12,      sNew.iuvw12,      "gIUVWQ12");
    CMPM(sRef.iuvw24,      sNew.iuvw24,      "gIUVWQ24");
    CMPM(sRef.subCommand,  sNew.subCommand,  "gSubCommand");
    CMPM(sRef.udc,         sNew.udc,         "gUDC");
    CMPM(sRef.comPar,      sNew.comPar,      "gComPar");
    CMPM(sRef.adc,         sNew.adc,         "AdcResult");
    CMPM(sRef.gpio,        sNew.gpio,        "GpioDataRegs");
    CMPM(sRef.epwm1,       sNew.epwm1,       "EPwm1Regs");
    CMPM(sRef.epwm2,       sNew.epwm2,       "EPwm2Regs");
    CMPM(sRef.epwm3,       sNew.epwm3,       "EPwm3Regs");
    CMPM(sRef.cpu,         sNew.cpu,         "cpuCtl");

    CMPM(pRef.overLoad,   pNew.overLoad,   "gOverLoad");
    CMPM(pRef.phaseLose,  pNew.phaseLose,  "gPhaseLose");
    CMPM(pRef.inLose,     pNew.inLose,     "gInLose");
    CMPM(pRef.loadLose,   pNew.loadLose,   "gLoadLose");
    CMPM(pRef.fanCtrl,    pNew.fanCtrl,    "gFanCtrl");
    CMPM(pRef.buffResCnt, pNew.buffResCnt, "gBuffResCnt");
    CMPM(pRef.cbc,        pNew.cbc,        "gCBCProtect");
    CMPM(pRef.error,      pNew.error,      "gError");

    CMPM(g_cbcRef, g_cbcNew, "CBCLimitCurPrepare 调用次数");
}

/* ------------------------------------------------------------------------- */
/* one step                                                                 */
/* ------------------------------------------------------------------------- */

static void do_step(int fn, const char *tag)
{
    g_cases++;

    shared_save(&sIn);

    gCbcPrepCount = 0;
    call_ref(fn);
    shared_save(&sRef);
    priv_snap(&pRef, 1);
    g_cbcRef = (long)gCbcPrepCount;

    shared_load(&sIn);
    gCbcPrepCount = 0;
    call_new(fn);
    shared_save(&sNew);
    priv_snap(&pNew, 0);
    g_cbcNew = (long)gCbcPrepCount;

    compare(tag);
}

/* ------------------------------------------------------------------------- */
/* input helpers                                                            */
/* ------------------------------------------------------------------------- */

static unsigned long g_rng;

static unsigned long rnd(void)
{
    unsigned long x;

    g_rng = g_rng * 1664525UL + 1013904223UL;
    x = g_rng;
    x ^= x >> 16;
    x *= 0x7FEB352DUL;
    x ^= x >> 15;
    x *= 0x846CA68BUL;
    x ^= x >> 16;
    return x & 0x7FFFFFFFUL;
}

/* `long`, not `int`: the shim folds `int` to the target's 16 bits, and several
 * call sites need ranges far wider than that (PhaseLose.Time reaches 120000000,
 * BuffResCnt 400000). A 32-bit helper keeps the input spread the phases intend. */
static long rnd_int(long lo, long hi)
{
    if (hi <= lo)
    {
        return lo;
    }
    return lo + (long)(rnd() % (unsigned long)(hi - lo + 1));
}

/* Pre-fill the private image on BOTH sides with 0xA5, then lay down a set of
 * non-trivial values. sync_priv() is *not* called here: the phases poke the ref
 * side first and end_scenario() copies it across. */
static void dirty_private(unsigned long seed)
{
    unsigned long s = seed * 2654435761UL + 1UL;

    memset(&ref_gOverLoad, 0xA5, sizeof(ref_gOverLoad));
    memset(&ref_gPhaseLose, 0xA5, sizeof(ref_gPhaseLose));
    memset(&ref_gInLose, 0xA5, sizeof(ref_gInLose));
    memset(&ref_gLoadLose, 0xA5, sizeof(ref_gLoadLose));
    memset(&ref_gFanCtrl, 0xA5, sizeof(ref_gFanCtrl));
    memset(&ref_gCBCProtect, 0xA5, sizeof(ref_gCBCProtect));
    memset(&ref_gError, 0xA5, sizeof(ref_gError));

    ref_gOverLoad.Cnt             = (int)(s % 7UL);
    ref_gOverLoad.FilterRealFreq  = (long)((s >> 3) % 4001UL);
    ref_gOverLoad.FilterInvCur    = (int)((s >> 5) % 6001UL);
    ref_gOverLoad.FilterMotorCur  = (int)((s >> 7) % 6001UL);
    ref_gOverLoad.InvTotal.all    = (s & 1UL) ? 0UL : (Ulong)((s >> 9) % 40000UL);
    ref_gOverLoad.MotorTotal.all  = (s & 2UL) ? 0UL : (Ulong)((s >> 11) % 40000UL);

    ref_gPhaseLose.Cnt       = (Uint)((s % 49999UL) + 1UL);
    ref_gPhaseLose.Time      = (Ulong)((s >> 13) % 120000000UL);
    ref_gPhaseLose.TotalU    = (Ulong)ref_gPhaseLose.Cnt * (Ulong)((s >> 15) % 4001UL + 1UL);
    ref_gPhaseLose.TotalV    = (Ulong)ref_gPhaseLose.Cnt * (Ulong)((s >> 17) % 4001UL + 1UL);
    ref_gPhaseLose.TotalW    = (Ulong)ref_gPhaseLose.Cnt * (Ulong)((s >> 19) % 4001UL + 1UL);
    ref_gPhaseLose.errMaxCur = (Uint)((s >> 21) % 65536UL);
    ref_gPhaseLose.errMinCur = (Uint)((s >> 23) % 65536UL);

    ref_gInLose.Cnt       = (Uint)((s % 1000UL));
    ref_gInLose.UpCnt     = (Uint)((s >> 2) % 1000UL);
    ref_gInLose.ErrCnt    = (Uint)((s >> 4) % 5UL);
    ref_gInLose.CntRes    = (Uint)((s >> 6) % 400UL);
    ref_gInLose.UpCntRes  = (Uint)((s >> 8) % 400UL);
    ref_gInLose.ForeInvType = (Uint)((s >> 10) % 31UL);

    ref_gLoadLose.ErrCnt   = (Uint)((s >> 12) % 300UL);
    ref_gLoadLose.ChkLevel = (Uint)((s >> 14) % 200UL);
    ref_gLoadLose.ChkTime  = (Uint)((s >> 16) % 100UL);

    ref_gFanCtrl.EnableCnt = (Uint)((s >> 18) % 1200UL);
    ref_gFanCtrl.RunCnt    = (Uint)((s >> 20) % 6000UL);

    ref_gBuffResCnt = (s & 4UL) ? 0UL : (Ulong)((s >> 22) % 300000UL);

    ref_gCBCProtect.Flag.all  = (Uint16)((s >> 24) & 0x0007UL);
    ref_gCBCProtect.TotalU    = (int)((s >> 1) % 1201UL) - 100;
    ref_gCBCProtect.TotalV    = (int)((s >> 3) % 1201UL) - 100;
    ref_gCBCProtect.TotalW    = (int)((s >> 5) % 1201UL) - 100;
    ref_gCBCProtect.Total     = (int)((s >> 7) % 1201UL) - 100;
    ref_gCBCProtect.EnableFlag = (int)((s >> 9) % 2UL);
    ref_gCBCProtect.CntU      = (int)((s >> 11) % 3500UL);
    ref_gCBCProtect.CntV      = (int)((s >> 13) % 3500UL);
    ref_gCBCProtect.CntW      = (int)((s >> 15) % 3500UL);
    ref_gCBCProtect.maxCBCTime = (Uint)((s >> 17) % 60000UL);
    ref_gCBCProtect.minCBCTime = (Uint)((s >> 19) % 60000UL);

    ref_gError.ErrorCode.all = (s & 8UL) ? 0UL : (Ulong)((s >> 26) & 0xFFFFUL);
    ref_gError.LastErrorCode = (s & 16UL) ? ref_gError.ErrorCode.all
                                          : (Ulong)((s >> 28) & 0xFFFFUL);
    ref_gError.ErrorInfo[0].all = (Uint)((s >> 2) & 0xFFFFUL);
    ref_gError.ErrorInfo[1].all = (Uint)((s >> 4) & 0xFFFFUL);
    ref_gError.ErrorInfo[2].all = (Uint)((s >> 6) & 0xFFFFUL);
    ref_gError.ErrorInfo[3].all = (Uint)((s >> 8) & 0xFFFFUL);
    ref_gError.ErrorInfo[4].all = (Uint)((s >> 10) & 0xFFFFUL);
}

/* Pre-fill every shared word. All divisors the module divides by are forced
 * non-zero here (see the header note). */
static void dirty_shared(unsigned long seed)
{
    unsigned long s = seed * 2246822519UL + 7UL;

    memset(&gMainCmd, 0xA5, sizeof(gMainCmd));
    memset(&gMainStatus, 0xA5, sizeof(gMainStatus));
    memset(&gInvInfo, 0xA5, sizeof(gInvInfo));
    memset(&gLineCur, 0xA5, sizeof(gLineCur));
    memset(&gMotorInfo, 0xA5, sizeof(gMotorInfo));
    memset(&gTemperature, 0xA5, sizeof(gTemperature));
    memset(&gPWM, 0xA5, sizeof(gPWM));
    memset(&gPhase, 0xA5, sizeof(gPhase));
    memset(&gIUVWQ12, 0xA5, sizeof(gIUVWQ12));
    memset(&gIUVWQ24, 0xA5, sizeof(gIUVWQ24));
    memset(&gSubCommand, 0xA5, sizeof(gSubCommand));
    memset(&gUDC, 0xA5, sizeof(gUDC));
    memset(&gComPar, 0xA5, sizeof(gComPar));
    memset((void *)&AdcResult, 0xA5, sizeof(AdcResult));
    memset((void *)&GpioDataRegs, 0xA5, sizeof(GpioDataRegs));
    memset((void *)&EPwm1Regs, 0xA5, sizeof(EPwm1Regs));
    memset((void *)&EPwm2Regs, 0xA5, sizeof(EPwm2Regs));
    memset((void *)&EPwm3Regs, 0xA5, sizeof(EPwm3Regs));
    memset((void *)gHostCpuCtl, 0, sizeof(gHostCpuCtl));

    gMainCmd.FreqReal     = (int)((s >> 3) % 4001UL);
    gMainCmd.FreqSyn      = (int)((s >> 5) % 8001UL) - 4000;
    gMainCmd.Command.all  = (Uint)((s >> 7) & 0xFFFFUL);
    gMainStatus.RunStep   = (Uint)((s >> 9) % 8UL);
    gMainStatus.SubStep   = (Uint)((s >> 11) % 8UL);
    gMainStatus.ErrFlag.all = (Uint)((s >> 13) & 0xFFFFUL);
    gMainStatus.StatusWord.all = (Uint)((s >> 15) & 0xFFFFUL);
    gInvInfo.InvVoltageType = (Uint)((s >> 17) % 4UL);
    gInvInfo.InvTypeApply   = (Uint)((s >> 19) % 31UL);
    gInvInfo.TempType       = (Uint)((s >> 21) % 5UL);
    gInvInfo.InvUpUDC       = (Uint)((s >> 23) % 2000UL) + 1U;
    gInvInfo.InvLowUDC      = (Uint)((s >> 25) % 1000UL) + 1U;
    gInvInfo.InvCurrent     = (Uint)((s >> 27) % 3000UL) + 1U;
    gInvInfo.InvCurrForP    = (Uint)((s >> 1) % 3000UL) + 1U;
    gInvInfo.GPType         = (Uint)((s >> 2) % 3UL);
    gLineCur.CurBaseInv     = (int)((s >> 4) % 6001UL);
    gLineCur.CurPer         = (int)((s >> 6) % 6001UL);
    gLineCur.ErrorShow      = (int)((s >> 8) % 6001UL);
    gMotorInfo.Current      = (Uint)((s >> 10) % 3000UL) + 1U;
    gMotorInfo.CurrentGet   = (Uint)((s >> 12) % 6000UL);
    gMotorInfo.FreqPer      = (Uint)((s >> 14) % 8001UL);
    gMotorInfo.MotorType    = (Uint)((s >> 16) % 4UL);
    gMotorInfo.CurBaseCoff  = (Uint)((s >> 18) % 400UL) + 1U;
    gTemperature.TempAD     = (Uint)((s >> 20) & 0xFFFFUL);
    gTemperature.TempBak    = (Uint)((s >> 22) & 0xFFFFUL);
    gTemperature.Temp       = (Uint)((s >> 24) % 121UL);
    gTemperature.ErrCnt     = (Uint)((s >> 26) % 8UL);
    gPWM.gPWMPrdApply       = (Uint)((s >> 28) % 2000UL) + 1U;
    gPhase.IMPhase          = (long)((s >> 5) % 100000UL);
    gIUVWQ12.U              = (int)((s & 0x1FFFUL) - 4000);
    gIUVWQ12.V              = (int)(((s >> 6) & 0x1FFFUL) - 4000);
    gIUVWQ12.W              = (int)(((s >> 12) & 0x1FFFUL) - 4000);
    gIUVWQ24.U              = (long)((s % 8001UL)) - 4000;
    gIUVWQ24.V              = (long)(((s >> 8) % 8001UL)) - 4000;
    gIUVWQ24.W              = (long)(((s >> 16) % 8001UL)) - 4000;
    gSubCommand.all         = (Uint)((s >> 18) & 0xFFFFUL);
    gUDC.uDC                = (Uint)((s >> 20) % 3000UL);
    gUDC.uDCFilter          = (Uint)((s >> 22) % 3000UL) + 1U;
    gUDC.uDCBigFilter       = (Uint)((s >> 24) % 3000UL) + 1U;
    gComPar.MotorOvLoad     = (Uint)((s >> 26) % 1000UL) + 1U;
    gComPar.PerMotorOvLoad  = (Uint)((s >> 28) % 100UL) + 1U;
    AdcResult.ADCRESULT13   = (Uint16)((s >> 2) & 0xFFFFUL);
}

/* Guarantee the conditions OutputPhaseLoseDetect divides under: Cnt >= 1, and
 * each Total is an exact multiple of Cnt with the average >= 1. */
static void seed_phase_lose(unsigned long seed, Ulong uAvg, Ulong vAvg, Ulong wAvg,
                            Ulong time)
{
    ref_gPhaseLose.Cnt    = (Uint)((seed % 40000UL) + 1UL);
    ref_gPhaseLose.Time   = time;
    ref_gPhaseLose.TotalU = (Ulong)ref_gPhaseLose.Cnt * uAvg;
    ref_gPhaseLose.TotalV = (Ulong)ref_gPhaseLose.Cnt * vAvg;
    ref_gPhaseLose.TotalW = (Ulong)ref_gPhaseLose.Cnt * wAvg;
}

static void begin_scenario(unsigned long seed)
{
    dirty_private(seed);
    dirty_shared(seed);
}

static void end_scenario(int fn, const char *tag)
{
    sync_priv();
    do_step(fn, tag);
}

/* ------------------------------------------------------------------------- */
/* phases                                                                   */
/* ------------------------------------------------------------------------- */

/* ---- SoftWareErrorDeal --------------------------------------------------- */

static void phase_sw_err(void)
{
    static const Uint runsteps[] = {
        STATUS_LOW_POWER, STATUS_STOP, STATUS_RUN, STATUS_SPEED_CHECK, 0U, 0xFFFFU
    };
    static const Ulong codes[] = {
        0UL, ERROR_SHORT_EARTH, ERROR_OVER_CURRENT, ERROR_LOW_UDC,
        ERROR_SHORT_EARTH | ERROR_OVER_CURRENT, 0xFFFFUL
    };
    size_t i, j, k, l;
    unsigned long seed = 1UL;

    for (i = 0; i < sizeof(runsteps) / sizeof(runsteps[0]); i++)
    {
        for (j = 0; j < sizeof(codes) / sizeof(codes[0]); j++)
        {
            for (k = 0; k < 2U; k++)        /* ErrorOK */
            {
                for (l = 0; l < 3U; l++)    /* over-current shutdown window */
                {
                    Uint base = (l == 0) ? 10200U : ((l == 1) ? 10240U : 10300U);
                    Uint udc;

                    begin_scenario(seed++);
                    gMainStatus.RunStep = runsteps[i];
                    ref_gError.ErrorCode.all = codes[j];
                    ref_gError.LastErrorCode = (j & 1U) ? codes[j] : 0UL;
                    gSubCommand.bit.ErrorOK = (Uint)k;
                    gMainStatus.ErrFlag.bit.OvCurFlag = (Uint)((k + l) & 1U);
                    gLineCur.CurBaseInv = (int)base;
                    AdcResult.ADCRESULT13 = 0U;     /* GetOverUdcFlag() == 1 */
                    for (udc = 0; udc < 3U; udc++)
                    {
                        gUDC.uDCFilter    = 900U;
                        gUDC.uDCBigFilter = (udc == 0) ? 500U
                                          : ((udc == 1) ? 700U : 1200U);
                        gInvInfo.InvUpUDC  = 700U;
                        gInvInfo.InvLowUDC = 700U;
                        if (runsteps[i] == STATUS_LOW_POWER) g_covLowPower++;
                        end_scenario(F_SWERR, "swerr");
                        /* the over-UDC / low-UDC boundary must be re-armed */
                        begin_scenario(seed++);
                        gMainStatus.RunStep = runsteps[i];
                        ref_gError.ErrorCode.all = codes[j];
                        ref_gError.LastErrorCode = (j & 1U) ? codes[j] : 0UL;
                        gSubCommand.bit.ErrorOK = (Uint)k;
                        gMainStatus.ErrFlag.bit.OvCurFlag = (Uint)((k + l) & 1U);
                        gLineCur.CurBaseInv = (int)base;
                        AdcResult.ADCRESULT13 = 0xFFFFU;  /* GetOverUdcFlag() == 0 */
                        gUDC.uDCFilter    = 900U;
                        gUDC.uDCBigFilter = (udc == 0) ? 500U
                                          : ((udc == 1) ? 700U : 1200U);
                        gInvInfo.InvUpUDC  = 700U;
                        gInvInfo.InvLowUDC = 700U;
                        if (runsteps[i] == STATUS_LOW_POWER) g_covLowPower++;
                        end_scenario(F_SWERR, "swerr-ovudc");
                    }
                }
            }
        }
    }
}

/* ---- TemperatureCheck ---------------------------------------------------- */

static void phase_temp(void)
{
    static const Uint volts[]  = { 0U, INV_VOLTAGE_690V, INV_VOLTAGE_1140V, 99U };
    static const Uint types[]  = { 0U, 9U, 10U, 11U, 18U, 19U, 26U, 27U, 30U, 40U };
    static const Uint temps[]  = { 0U, 1U, 2U, 3U, 9U };
    size_t i, j, k, t;
    unsigned long seed = 100000UL;

    /* Probe every table through one of the selection branches, and sweep
     * TempAD across a table's whole span -- exact hits, gaps between entries,
     * and both saturating ends. TempAD is NOT the temperature code: the module
     * computes m_TempAD = TempAD >> 6, so a table entry is only hit with
     * entry << 6. The list below is therefore all at the code scale (m_TempAD
     * == 9 for 624), and the entry-scale hits are the adex[] pass further
     * down. 624..208 is gTempTableP44X. */
    for (i = 0; i < sizeof(volts) / sizeof(volts[0]); i++)
    {
        for (j = 0; j < sizeof(types) / sizeof(types[0]); j++)
        {
            for (k = 0; k < sizeof(temps) / sizeof(temps[0]); k++)
            {
                for (t = 0; t < 16U; t++)
                {
                    Uint ad;

                    switch (t)
                    {
                        case 0:  ad = 0U;        break;  /* above table[0] */
                        case 1:  ad = 624U;      break;  /* code 9 */
                        case 2:  ad = 623U;      break;
                        case 3:  ad = 208U;      break;  /* code 3 */
                        case 4:  ad = 207U;      break;
                        case 5:  ad = 500U;      break;
                        case 6:  ad = 499U;      break;
                        case 7:  ad = 419U;      break;  /* exact mid entry */
                        case 8:  ad = 418U;      break;
                        case 9:  ad = 0xFFFFU;   break;
                        case 10: ad = 0xFFFFU - 1U; break;
                        case 11: ad = 1000U;     break;
                        case 12: ad = 32768U;    break;
                        case 13: ad = (Uint)(624U << 6); break; /* code == P44X[0] */
                        case 14: ad = (Uint)(208U << 6); break; /* code == P44X[22] */
                        default: ad = (Uint)((seed * 37UL) & 0xFFFFUL); break;
                    }

                    begin_scenario(seed++);
                    gInvInfo.InvVoltageType = volts[i];
                    gInvInfo.InvTypeApply   = types[j];
                    gInvInfo.TempType       = temps[k];
                    gTemperature.TempAD     = ad;
                    gTemperature.TempBak    = (Uint)(ad & 0xFFF0U);
                    gTemperature.Temp       = (Uint)((ad >> 6) % 121U);
                    gTemperature.ErrCnt     = 4U;

                    {
                        Uint mType = types[j];
                        if (volts[i] == INV_VOLTAGE_690V)
                        {
                            mType = (Uint)(types[j] + 27U);
                        }
                        if (mType <= 10U)       g_covTemp1++;
                        else if (mType <= 18U)  g_covTemp2++;
                        else if (mType <= 26U)  g_covTemp3++;
                        else                    g_covTemp4++;
                    }
                    end_scenario(F_TEMP, "temp");
                }
            }
        }
    }

    /* ErrCnt sweep against the 5 threshold, and the >= 8 temperature delta. */
    for (i = 0; i < 6U; i++)
    {
        for (j = 0; j < 4U; j++)
        {
            begin_scenario(seed++);
            gInvInfo.InvVoltageType = 0U;
            gInvInfo.InvTypeApply   = (Uint)j;   /* m_ErrTemp 95 */
            gInvInfo.TempType       = 0U;
            gTemperature.TempAD     = 624U;
            gTemperature.TempBak    = (Uint)(i == 0 ? 500U : 300U);
            gTemperature.Temp       = (i == 0) ? 90U : 96U;
            gTemperature.ErrCnt     = (Uint)i;
            end_scenario(F_TEMP, "temp-errcnt");
        }
    }

    /* The two remaining constants are level tests, not range tests: the 95/94
     * error level only decides when the code decodes to exactly 94, and the
     * ">= 8" refresh only decides when the delta is exactly 8. Both edges are
     * constructed from the table arithmetic instead of guessed:
     *
     *   P44X   bracket (20,21): code = 1472 + ((244 - ad) << 6) / 18
     *                          ad = 231  ->  1518  ->  Temp 94
     *   WAIZHI bracket (21,22): code = 1536 + 8 * (70 - ad)
     *                          ad = 70/69/68  ->  1536/1544/1552 (steps of 8)
     *
     * so a TempBak of 1510 with ad 231 (or 1528 with ad 70) makes the delta
     * exactly 8, and any TempBak that keeps the update from firing leaves Temp
     * sitting on 94. The same pass parks TempAD on the two table ends *at the
     * code scale* (entry << 6), which is where the two saturating comparisons
     * would have to decide. */
    {
        static const Uint adex[] = { (Uint)(231U << 6), (Uint)(231U << 6),
                                     (Uint)(70U  << 6), (Uint)(624U << 6),
                                     (Uint)(208U << 6) };
        static const Uint bakx[] = { 1510U, 1509U, 1528U, 0U, 0U };
        static const Uint tmpx[] = { 7U, 94U, 7U, 0U, 0U };
        static const Uint devx[] = { 5U, 26U };   /* P44X / WAIZHI */
        size_t a, b;

        for (a = 0; a < 5U; a++)
        {
            for (b = 0; b < 2U; b++)
            {
                begin_scenario(seed++);
                gInvInfo.InvVoltageType = 0U;
                gInvInfo.InvTypeApply   = devx[b];
                gInvInfo.TempType       = 1U;
                gTemperature.TempAD     = adex[a];
                gTemperature.TempBak    = bakx[a];
                gTemperature.Temp       = tmpx[a];
                gTemperature.ErrCnt     = 0U;
                g_covTempEdge++;
                end_scenario(F_TEMP, "temp-edge");
            }
        }

        /* one-code-unit walk over the whole P44X span, both directions: every
         * temperature code in [12,100] is produced, and every refresh crossing
         * is visited from above and below. */
        for (a = 0; a < 2U; a++)
        {
            Uint step;

            for (step = 0; step < 417U; step++)
            {
                Uint mt = (a == 0U) ? (Uint)(624U - step) : (Uint)(208U + step);

                begin_scenario(seed++);
                gInvInfo.InvVoltageType = 0U;
                gInvInfo.InvTypeApply   = 5U;
                gInvInfo.TempType       = 1U;
                gTemperature.TempAD     = (Uint)(mt << 6);
                gTemperature.TempBak    = (Uint)(mt & 0xFF00U);
                gTemperature.Temp       = (Uint)((mt >> 4) % 121U);
                gTemperature.ErrCnt     = (Uint)(mt % 5U);
                end_scenario(F_TEMP, "temp-walk");
            }
        }
    }

    /* ---- the table choice, at the code scale ------------------------------
     *
     * The five temperature tables only disagree *between* their spans, and the
     * chosen code only becomes observable at all through the
     * `mType - TempBak >= 8` refresh. Two input-shape mistakes keep that
     * invisible, and the loop above makes both of them:
     *
     *  1. TempAD must be a CODE-scale number shifted up -- the module reads
     *     m_TempAD = TempAD >> 6, so the code 400 is TempAD 25600. The t-loop
     *     deliberately pokes raw ADC readings as well (624 is code 9), and every
     *     table answers those with the same saturating constant, so the choice
     *     of table does not matter there.
     *  2. TempBak must be a code-scale number too. `(ad & 0xFFF0)` is a 16-bit
     *     ADC reading; with `code - TempBak < 0` the refresh does not fire, the
     *     code stays local, and a variant that picked a different table writes
     *     byte-identical globals. TempBak = 0 always refreshes (every code is
     *     >= 192), which is what makes the table choice visible.
     *
     * The 15 m_TempAD values span 60..640 -- inside P44X (208..624), WAIZHI
     * (62..655), SEMIKON (224..558), P8XX (76..475) and BSMXX (86..486) -- so
     * every pair of tables the selection can swap has a probe where the two
     * disagree. */
    {
        static const Uint adX[] = { 60U,  90U,  130U, 170U, 210U, 240U, 270U, 300U,
                                    340U, 380U, 420U, 460U, 520U, 580U, 640U };
        size_t x;

        for (i = 0; i < sizeof(volts) / sizeof(volts[0]); i++)
        {
            for (j = 0; j < sizeof(types) / sizeof(types[0]); j++)
            {
                for (k = 0; k < sizeof(temps) / sizeof(temps[0]); k++)
                {
                    for (x = 0; x < sizeof(adX) / sizeof(adX[0]); x++)
                    {
                        begin_scenario(seed++);
                        gInvInfo.InvVoltageType = volts[i];
                        gInvInfo.InvTypeApply   = types[j];
                        gInvInfo.TempType       = temps[k];
                        gTemperature.TempAD     = (Uint)(adX[x] << 6);
                        gTemperature.TempBak    = 0U;
                        gTemperature.Temp       = 0U;
                        gTemperature.ErrCnt     = 0U;
                        g_covTempTable++;
                        end_scenario(F_TEMP, "temp-table");
                    }
                }
            }
        }
    }
}

/* ---- output phase loss --------------------------------------------------- */

static void phase_out_phase(void)
{
    static const Uint runsteps[] = { STATUS_RUN, STATUS_SPEED_CHECK, STATUS_STOP, 0U };
    static const int  freqs[]    = { 0, 79, 80, 81, 4000 };
    size_t i, j, k, m;
    Ulong tvals[] = { 0UL, 59999999UL, 60000000UL, 60000001UL };
    unsigned long seed = 200000UL;

    for (i = 0; i < 4U; i++)
    {
        for (j = 0; j < 5U; j++)
        {
            for (k = 0; k < 4U; k++)
            {
                for (m = 0; m < 6U; m++)
                {
                    Ulong uAvg, vAvg, wAvg;
                    Uint cnt;

                    /* six average triples covering: balanced, one small (ratio
                     * > 10), max <= 500, exact 10x, 11x, and a zero-average
                     * case kept safe by Cnt substitution below. */
                    switch (m)
                    {
                        case 0: uAvg = 1000; vAvg = 1000; wAvg = 1000; break;
                        case 1: uAvg = 1000; vAvg = 1000; wAvg = 10;   break; /* 100x */
                        case 2: uAvg = 499;  vAvg = 499;  wAvg = 250;  break; /* max<=500 */
                        case 3: uAvg = 1000; vAvg = 100;  wAvg = 700;  break; /* 10x */
                        case 4: uAvg = 1100; vAvg = 95;   wAvg = 900;  break; /* >10x */
                        default:uAvg = 1;    vAvg = 1;    wAvg = 1;    break; /* tiny, ratio 1 */
                    }

                    begin_scenario(seed++);
                    gSubCommand.bit.OutputLost = 1U;
                    gMainCmd.FreqReal = freqs[j];
                    gMainCmd.Command.bit.StartDC = 0U;
                    gMainCmd.Command.bit.StopDC  = 0U;
                    gMainStatus.RunStep = runsteps[i];
                    cnt = (Uint)((seed % 40000UL) + 1UL);
                    if (cnt < 50000U) cnt = 50000U;      /* pass the Cnt guard too */
                    ref_gPhaseLose.Cnt    = cnt;
                    ref_gPhaseLose.Time   = tvals[k];
                    ref_gPhaseLose.TotalU = (Ulong)cnt * uAvg;
                    ref_gPhaseLose.TotalV = (Ulong)cnt * vAvg;
                    ref_gPhaseLose.TotalW = (Ulong)cnt * wAvg;
                    if (runsteps[i] == STATUS_RUN || runsteps[i] == STATUS_SPEED_CHECK)
                    {
                        if (tvals[k] >= (Ulong)DSP_CLOCK * 1000000L || cnt >= 50000U)
                        {
                            if (gMainCmd.FreqReal >= 80 &&
                                gSubCommand.bit.OutputLost == 1U)
                            {
                                g_covOutEnter++;
                            }
                        }
                    }
                    end_scenario(F_OUTPLD, "outphase");

                    /* the early-return half of the guard */
                    begin_scenario(seed++);
                    gSubCommand.bit.OutputLost = 0U;
                    gMainCmd.FreqReal = freqs[j];
                    gMainStatus.RunStep = runsteps[i];
                    ref_gPhaseLose.Cnt    = cnt;
                    ref_gPhaseLose.Time   = tvals[k];
                    ref_gPhaseLose.TotalU = (Ulong)cnt * uAvg;
                    ref_gPhaseLose.TotalV = (Ulong)cnt * vAvg;
                    ref_gPhaseLose.TotalW = (Ulong)cnt * wAvg;
                    end_scenario(F_OUTPLD, "outphase-off");
                }
            }
        }
    }

    /* Exact edge of the magnitude test. (m_Max > 500) && (m_Max / m_Min > 10)
     * is a level test on the *smaller* side too: 500 passes without the flag,
     * 501 trips it. Cnt is above 50000 and Time above 60e6 so the guard is open
     * and the averages come out exact; m_Min stays >= 1 (the divisor). */
    {
        static const Ulong mx[] = { 500UL, 501UL, 502UL, 600UL };
        static const Ulong mn[] = { 1UL, 5UL, 50UL };
        size_t a, b;

        for (a = 0; a < 4U; a++)
        {
            for (b = 0; b < 3U; b++)
            {
                begin_scenario(seed++);
                gSubCommand.bit.OutputLost = 1U;
                gMainCmd.FreqReal = 4000;
                gMainCmd.Command.bit.StartDC = 0U;
                gMainCmd.Command.bit.StopDC  = 0U;
                gMainStatus.RunStep = STATUS_RUN;
                seed_phase_lose(seed, mx[a], mn[b], mn[b], 60000000UL);
                g_covOutEdge++;
                end_scenario(F_OUTPLD, "outphase-edge");
            }
        }
    }

    /* OutputLoseAdd: the three abs() arguments, including INT16_MIN/2 spill and
     * the PWM period accumulation. */
    {
        static const int iv[] = { 0, 1, -1, 32767, -32768, 0x0FFF, -4096 };
        size_t a, b, c, d;

        for (a = 0; a < 7U; a++)
        {
            for (b = 0; b < 7U; b++)
            {
                for (c = 0; c < 7U; c++)
                {
                    for (d = 0; d < 4U; d++)
                    {
                        begin_scenario(seed++);
                        gIUVWQ24.U = (long)iv[a] << 12;
                        gIUVWQ24.V = (long)iv[b] << 12;
                        gIUVWQ24.W = (long)iv[c] << 12;
                        gPWM.gPWMPrdApply = (d == 0) ? 0U : (Uint)(1U + d * 1000U);
                        end_scenario(F_OUTADD, "outadd");
                    }
                }
            }
        }
    }

    /* OutputLoseReset: pre-fill non-zero, then demand everything is cleared. */
    {
        size_t a;
        for (a = 0; a < 8U; a++)
        {
            begin_scenario(seed++);
            end_scenario(F_OUTRST, "outrst");
        }
    }
}

/* ---- input phase loss ---------------------------------------------------- */

static void phase_in_phase(void)
{
    static const Uint runsteps[] = { STATUS_RUN, STATUS_SPEED_CHECK, STATUS_STOP, 0U };
    static const Uint adcs[]     = { 0U, 992U, 993U, 1000U, 0xFFFFU };
    size_t i, j, k, m, n;
    unsigned long seed = 300000UL;

    for (i = 0; i < 4U; i++)
    {
        for (j = 0; j < 5U; j++)
        {
            for (k = 0; k < 4U; k++)
            {
                for (m = 0; m < 4U; m++)
                {
                    for (n = 0; n < 5U; n++)
                    {
                        begin_scenario(seed++);
                        gSubCommand.bit.InputLost = (Uint)(k & 1U);
                        gMainStatus.RunStep = runsteps[i];
                        gInvInfo.InvTypeApply = (Uint)((j * 8U) & 0x1FU);
                        ref_gInLose.ForeInvType = (Uint)(((j * 8U) & 0x1FU) + ((k & 2U) ? 1U : 0U));
                        AdcResult.ADCRESULT13 = adcs[j];
                        ref_gInLose.Cnt      = (m == 0) ? 499U : ((m == 1) ? 500U : ((m == 2) ? 501U : 0U));
                        ref_gInLose.UpCnt    = (n == 0) ? 0U : ((n == 1) ? 5U : ((n == 2) ? 6U : ((n == 3) ? 484U : 485U)));
                        ref_gInLose.UpCntRes = (Uint)((seed % 3UL) * 249UL);
                        ref_gInLose.CntRes   = (Uint)((seed % 3UL) * 249UL);
                        ref_gInLose.ErrCnt   = (Uint)(seed % 3UL);
                        if (adcs[j] < 993U) g_covInRes++;
                        end_scenario(F_INPLD, "inphase");

                        /* drive the 250-count resistance window to its edge */
                        begin_scenario(seed++);
                        gSubCommand.bit.InputLost = 1U;
                        gMainStatus.RunStep = runsteps[i];
                        gInvInfo.InvTypeApply = 30U;
                        ref_gInLose.ForeInvType = 1U;
                        AdcResult.ADCRESULT13 = adcs[j];
                        ref_gInLose.Cnt      = 100U;
                        ref_gInLose.UpCnt    = 100U;
                        ref_gInLose.UpCntRes = (Uint)((n == 0) ? 0U : ((n == 1) ? 248U : 249U));
                        ref_gInLose.CntRes   = (Uint)((n == 0) ? 0U : ((n == 1) ? 249U : 250U));
                        ref_gInLose.ErrCnt   = 1U;
                        end_scenario(F_INPLD, "inphase-res");
                    }
                }
            }
        }
    }
}

/* ---- fan ----------------------------------------------------------------- */

static void phase_fan(void)
{
    static const Uint runsteps[] = { STATUS_LOW_POWER, STATUS_RUN, STATUS_STOP };
    static const Uint enable[]   = { 0U, 499U, 500U, 501U };
    static const Uint temps[]    = { 0U, 39U, 40U, 41U, 42U, 43U, 120U };
    /* 4999/5000/5001 straddle the `RunCnt >= 5000` clamp. 0/4997/4998 are the
     * other side of it: there the counter is *not* clamped, so the stored value
     * itself is the observable and a mutation that changes the increment
     * (`RunCnt++` -> `RunCnt += 2`) cannot hide behind the clamp. */
    static const Uint runcnt[]   = { 0U, 1U, 4997U, 4998U, 4999U, 5000U, 5001U };
    size_t i, j, k, l, m;
    unsigned long seed = 400000UL;

    for (i = 0; i < 3U; i++)
    {
        for (j = 0; j < 4U; j++)
        {
            for (k = 0; k < 7U; k++)
            {
                for (l = 0; l < sizeof(runcnt) / sizeof(runcnt[0]); l++)
                {
                    for (m = 0; m < 3U; m++)
                    {
                        begin_scenario(seed++);
                        gMainStatus.RunStep = runsteps[i];
                        ref_gFanCtrl.EnableCnt = enable[j];
                        ref_gFanCtrl.RunCnt    = runcnt[l];
                        gTemperature.Temp      = temps[k];
                        gMainCmd.Command.bit.Start = (Uint)(m == 1U);
                        gSubCommand.bit.FanNoStop  = (Uint)(m == 2U);
                        if (runsteps[i] != STATUS_LOW_POWER &&
                            temps[k] > 42U)
                        {
                            g_covFanOn++;
                        }
                        end_scenario(F_FAN, "fan");
                    }
                }
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 * Driver-side replica of the module's Filter16. It exists only to solve for the
 * pre-call filter state that lands the post-call value on an exact edge; a wrong
 * copy can cost coverage, never manufacture a mismatch, because the compared
 * value is still computed by the module itself.
 * ------------------------------------------------------------------------- */
static Uint host_filter16(Uint x, Uint total)
{
    return (Uint)(((((Ulong)total << 16) + ((Ulong)x << 12)) -
                   ((Ulong)total << 12)) >> 16);
}

static Uint solve_filter16(Uint x, Uint target)
{
    unsigned t;

    for (t = 0; t < 65536U; t++)
    {
        if (host_filter16(x, (Uint)t) == target)
        {
            return (Uint)t;
        }
    }
    return 0xFFFFU;
}

/* ---- overload ------------------------------------------------------------ */

static void phase_overload(void)
{
    static const Uint types[] = { 0U, 20U, 27U, 28U, 29U };
    static const Uint gp[]    = { 0U, 1U, 2U };
    static const Uint cnts[]  = { 0U, 3U, 4U, 5U, 6U };
    static const Uint runs[]  = { STATUS_RUN, STATUS_STOP, STATUS_LOW_POWER };
    static const Uint msw[]   = { 0U, 9U, 10U, 35999U, 36000U, 36001U };
    size_t i, j, k, l, m, n;
    unsigned long seed = 500000UL;

    for (i = 0; i < 5U; i++)
    {
        for (j = 0; j < 3U; j++)
        {
            for (k = 0; k < 5U; k++)
            {
                for (l = 0; l < 3U; l++)
                {
                    for (m = 0; m < 6U; m++)
                    {
                        /* sweep the inversion current across the table span */
                        for (n = 0; n < 9U; n++)
                        {
                            static const Uint cur[] = {
                                0U, 1000U, 1050U, 1150U, 1500U, 1870U, 2200U,
                                36000U, 60000U
                            };

                            begin_scenario(seed++);
                            gInvInfo.InvTypeApply = types[i];
                            gInvInfo.GPType       = gp[j];
                            gInvInfo.InvCurrent   = 2000U;
                            gInvInfo.InvCurrForP  = 1000U;
                            ref_gOverLoad.Cnt     = (int)cnts[k];
                            gMainStatus.RunStep   = runs[l];
                            gLineCur.CurBaseInv   = (int)cur[n];
                            gLineCur.CurPer       = (int)cur[n];
                            gMainCmd.FreqReal     = 700;
                            ref_gOverLoad.FilterRealFreq = 700;
                            ref_gOverLoad.InvTotal.all   = (Ulong)msw[m] << 16;
                            ref_gOverLoad.MotorTotal.all = (Ulong)msw[m] << 16;
                            gSubCommand.bit.MotorOvLoad  = (Uint)(n & 1U);
                            gComPar.MotorOvLoad   = 100U;
                            gComPar.PerMotorOvLoad = 50U;
                            gMotorInfo.CurBaseCoff = 200U;
                            gMainStatus.StatusWord.bit.PerOvLoadInv   = 1U;
                            gMainStatus.StatusWord.bit.PerOvLoadMotor = 1U;

                            if (types[i] >= 28U) g_covOvScale++;
                            if (gp[j] == 0U)     g_covOvGp0++;
                            if (gp[j] == 1U)     g_covOvGp1++;
                            end_scenario(F_OVERLOAD, "overload");
                        }
                    }
                }
            }
        }
    }

    /* Exact edges of the derived thresholds. Filter16(x, total) with x == 0 is
     * (total * 15) >> 4, so the pre-call state is solved for the post value that
     * decodes to the wanted m_Cur:
     *
     *   FilterInvCur     6144 -> m_Cur 1500   (inside the interpolation span;
     *                                          this is the input that separates
     *                                          "FilterRealFreq < 500" from
     *                                          "<= 500", because the rescale
     *                                          shifts m_Cur and the table index)
     *   FilterInvCur     7660 -> m_Cur 1870   (m_TorCurUpper itself: the one
     *                                          input where "m_Cur >= upper" and
     *                                          "m_Cur > upper" differ -- the
     *                                          mutation is equivalent there,
     *                                          see the equivalence pins in main)
     *   FilterMotorCur   9176 -> m_Cur 2240   (m_LDeta 2240*200 == 448000, the
     *                                          exact ">= cap" level)
     *   FilterMotorCur  14058 -> m_Cur 3432   (m_LDeta 17160000, so far above
     *                                          the cap that a 16-bit m_Cur
     *                                          would wrap; exercises the
     *                                          truncation path of the cap
     *                                          branch)
     *
     * FilterRealFreq is parked on exactly 500 (FreqReal 8000 into an empty
     * state). The motor probes pre-load MotorTotal so that adding the 196608
     * step lands MSW one unit above PerMotorOvLoad * 360 -- the only level that
     * separates the two factors. InvTypeApply is kept under 28 so CurBaseInv
     * passes through unscaled, and Cnt 4 makes the ++ reach 5 (not < 5). */
    {
        static const Uint pfv[]  = { 6144U, 7660U, 0U, 0U };
        static const Uint pfmv[] = { 0U, 0U, 9176U, 14058U };
        static const Uint pcof[] = { 0U, 0U, 200U, 5000U };
        static const Uint pmol[] = { 0U, 0U, 1U, 1U };
        static const Uint pmsw[] = { 3U, 3U, 18001U, 18001U };
        size_t a;

        if (((((Ulong)6144UL * 1000UL) >> 12) != 1500UL) ||
            ((((Ulong)7660UL * 1000UL) >> 12) != 1870UL) ||
            ((((Ulong)9176UL * 1000UL) >> 12) != 2240UL) ||
            ((((Ulong)14058UL * 1000UL) >> 12) != 3432UL))
        {
            printf("FATAL: overload edge constants do not decode as documented\n");
            return;
        }

        for (a = 0; a < 4U; a++)
        {
            begin_scenario(seed++);
            gInvInfo.InvTypeApply = 5U;
            gInvInfo.GPType       = 1U;
            gInvInfo.InvCurrent   = 2000U;
            gInvInfo.InvCurrForP  = 1000U;
            ref_gOverLoad.Cnt     = 4;
            gMainStatus.RunStep   = STATUS_RUN;
            gLineCur.CurBaseInv   = 0;
            gLineCur.CurPer       = 0;
            gMainCmd.FreqReal     = 8000;   /* FilterRealFreq -> 500 */
            ref_gOverLoad.FilterRealFreq = 0;
            ref_gOverLoad.FilterInvCur   = (int)solve_filter16(0U, pfv[a]);
            ref_gOverLoad.FilterMotorCur = (int)solve_filter16(0U, pfmv[a]);
            ref_gOverLoad.InvTotal.all   = 0UL;
            ref_gOverLoad.MotorTotal.all = ((Ulong)pmsw[a] << 16) - 196608UL;
            gSubCommand.bit.MotorOvLoad  = pmol[a];
            gComPar.MotorOvLoad   = 100U;
            gComPar.PerMotorOvLoad = 50U;
            gMotorInfo.CurBaseCoff = pcof[a];
            gMainStatus.StatusWord.bit.PerOvLoadInv   = 1U;
            gMainStatus.StatusWord.bit.PerOvLoadMotor = 1U;
            g_covOvEdge++;
            end_scenario(F_OVERLOAD, "overload-edge");
        }
    }
}

/* ---- CBC ----------------------------------------------------------------- */

static void phase_cbc(void)
{
    static const Uint runs[]  = { STATUS_RUN, STATUS_STOP };
    static const int  cnt[]   = { 0, 2499, 2500, 2501, 2999, 3000, 3001 };
    static const int  flag[]  = { 0, 1, 2, 3, 4, 5, 6, 7 };
    static const Uint en[]    = { 0U, 1U };
    static const int  cur[]   = { 0, 1, -1, 9267, 9268, 32767, -32768 };
    size_t i, j, k, l, m, n;
    unsigned long seed = 600000UL;

    for (i = 0; i < 2U; i++)
    {
        for (j = 0; j < 7U; j++)
        {
            for (k = 0; k < 8U; k++)
            {
                for (l = 0; l < 2U; l++)
                {
                    for (m = 0; m < 7U; m++)
                    {
                        for (n = 0; n < 3U; n++)
                        {
                            static const int freqs[] = { 0, 20, 21 };

                            begin_scenario(seed++);
                            gMainStatus.RunStep = runs[i];
                            ref_gCBCProtect.CntU = cnt[j];
                            ref_gCBCProtect.CntV = cnt[(j + 1U) % 7U];
                            ref_gCBCProtect.CntW = cnt[(j + 2U) % 7U];
                            ref_gCBCProtect.Flag.all = (Uint16)flag[k];
                            ref_gCBCProtect.EnableFlag = (int)en[l];
                            ref_gCBCProtect.TotalU = (int)cur[m];
                            ref_gCBCProtect.TotalV = (int)cur[(m + 1U) % 7U];
                            ref_gCBCProtect.TotalW = (int)cur[(m + 2U) % 7U];
                            gSubCommand.bit.CBCEnable = en[l];
                            gMainCmd.FreqReal = freqs[n];
                            gMotorInfo.Current = 2000U;
                            gInvInfo.InvCurrent = 2000U;  /* m_Coff 1024 */
                            gIUVWQ12.U = cur[m];
                            gIUVWQ12.V = cur[(m + 3U) % 7U];
                            gIUVWQ12.W = cur[(m + 5U) % 7U];
                            if (cnt[j] > 2500 || cnt[(j + 1U) % 7U] > 2500 ||
                                cnt[(j + 2U) % 7U] > 2500)
                            {
                                g_covCbcTrip++;
                            }
                            end_scenario(F_CBC, "cbc");
                        }
                    }
                }
            }
        }
    }

    for (i = 0; i < 4U; i++)
    {
        begin_scenario(seed++);
        end_scenario((i & 1U) ? F_CBCEN : F_CBCDIS, "cbconoff");
    }

    /* Exact edges of the 9267 level test. m_CurU/V/W is the raw code only when
     * m_Coff == (Current << 10) / InvCurrent == 1024, so Current and InvCurrent
     * are equal here; every phase then takes each edge value in turn, which is
     * what separates the "> 9267" test (U == 9268) from the ">> 10" shift
     * (V == 9267 doubles to 18534 on the wrong side of the same test). FreqReal
     * is kept at 0 or 20 so the "> 20" counter clear stays out of the way. */
    {
        static const int edge[] = { 4633, 4634, 4635, 9266, 9267, 9268, 9269 };
        static const int cnt2[] = { 0, 2499, 2500, 2501 };
        size_t a, b, c;

        for (a = 0; a < 7U; a++)
        {
            for (b = 0; b < 4U; b++)
            {
                for (c = 0; c < 2U; c++)
                {
                    begin_scenario(seed++);
                    gMainStatus.RunStep = STATUS_RUN;
                    ref_gCBCProtect.CntU = cnt2[b];
                    ref_gCBCProtect.CntV = cnt2[(b + 1U) % 4U];
                    ref_gCBCProtect.CntW = cnt2[(b + 2U) % 4U];
                    ref_gCBCProtect.Flag.all = (Uint16)(c + 1U);
                    ref_gCBCProtect.EnableFlag = 1;
                    ref_gCBCProtect.TotalU = 0;
                    ref_gCBCProtect.TotalV = 0;
                    ref_gCBCProtect.TotalW = 0;
                    gSubCommand.bit.CBCEnable = 1U;
                    gMainCmd.FreqReal = (c == 0U) ? 0 : 20;
                    gMotorInfo.Current = 2000U;
                    gInvInfo.InvCurrent = 2000U;
                    gIUVWQ12.U = edge[a];
                    gIUVWQ12.V = edge[(a + 1U) % 7U];
                    gIUVWQ12.W = edge[(a + 2U) % 7U];
                    g_covCbcEdge++;
                    end_scenario(F_CBC, "cbc-edge");
                }
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 * Driver-side copies of LoadLoseDetect's two derived thresholds. They are used
 * only to park abs(gIUVWQ12.U) / abs(gMainCmd.FreqSyn) on an exact comparison
 * edge; the comparison itself is still the module's own.
 * ------------------------------------------------------------------------- */
static Uint host_load_limit(Uint chk, Uint cur, Uint get, Uint mul)
{
    Uint v = (Uint)((((Ulong)chk * (Ulong)get) / (Ulong)cur) << 2);

    v = (Uint)(((long)v * (long)mul) >> 10);
    return (v < 20U) ? 20U : v;
}

static Uint host_limit_freq(Uint per, Uint mul)
{
    return (Uint)((((Ulong)per * (Ulong)mul) >> 15));
}

/* ---- load loss ----------------------------------------------------------- */

static void phase_loadlose(void)
{
    static const Uint runs[]  = { STATUS_RUN, STATUS_STOP, 0U };
    static const Uint mtype[] = { 0U, MOTOR_TYPE_PM };
    static const Uint chkt[]  = { 0U, 1U, 2U, 100U, 1000U };
    static const Uint errc[]  = { 0U, 1U, 24U, 25U, 49U, 50U, 51U, 200U };
    static const int  iuv[]   = { 0, 1, -1, 1000, -1000, 32767, -32768 };
    size_t i, j, k, l, m, n;
    unsigned long seed = 700000UL;

    for (i = 0; i < 3U; i++)
    {
        for (j = 0; j < 2U; j++)
        {
            for (k = 0; k < 5U; k++)
            {
                for (l = 0; l < 8U; l++)
                {
                    for (m = 0; m < 7U; m++)
                    {
                        for (n = 0; n < 3U; n++)
                        {
                            static const int fsyn[] = { 0, 1000, 4000 };

                            begin_scenario(seed++);
                            gMainStatus.RunStep = runs[i];
                            gMotorInfo.MotorType = mtype[j];
                            gMotorInfo.Current   = 2000U;
                            gMotorInfo.CurrentGet = 1000U;
                            gMotorInfo.FreqPer    = 4000U;
                            ref_gLoadLose.ChkLevel = 10U;
                            ref_gLoadLose.ChkTime  = chkt[k];
                            ref_gLoadLose.ErrCnt   = errc[l];
                            gSubCommand.bit.LoadLose = (Uint)(n & 1U);
                            gMainCmd.FreqSyn = fsyn[n];
                            gMainCmd.Command.bit.StartDC = 0U;
                            gMainCmd.Command.bit.StopDC  = 0U;
                            gIUVWQ12.U = iuv[m];
                            gIUVWQ12.V = iuv[(m + 1U) % 7U];
                            gIUVWQ12.W = iuv[(m + 2U) % 7U];
                            gMainStatus.StatusWord.bit.OutOff = 1U;
                            end_scenario(F_LOADLOSE, "loadlose");
                        }
                    }
                }
            }
        }
    }

    /* Exact edges of the five constants in this function. Each shape is a
     * (ChkLevel, Current, CurrentGet, FreqPer) tuple; both scale factors and
     * both clamp outcomes are recomputed here and the current / the synchronous
     * frequency are parked on the resulting edge:
     *
     *   b == 0: abs(U) on the threshold          -> separates the ">=" guard
     *   b == 1: abs(U) on the 1446 threshold     -> separates 1447 / 1446, and
     *                                               the "< 20 ? 20" clamp
     *   b == 2: abs(FreqSyn) on the threshold    -> separates "<" from "<="
     *   b == 3: abs(FreqSyn) on the 1637 one     -> separates 1638 / 1637
     *
     * The four shapes give an interior threshold, a 1447/1446 split, the clamp
     * firing, and a second split. FreqPer 32768 is the smallest value that
     * makes the 1638 and 1637 products land on different integers. */
    {
        static const Uint shape[][4] = {
            { 10U,  2000U, 1000U, 32768U },
            { 100U, 1U,    1U,    32768U },
            { 1U,   1000U, 1000U, 32768U },
            { 200U, 1000U, 1000U, 32768U }
        };
        size_t a, b;

        for (a = 0; a < 4U; a++)
        {
            Uint lmt  = host_load_limit(shape[a][0], shape[a][1], shape[a][2], 1447U);
            Uint lmt0 = host_load_limit(shape[a][0], shape[a][1], shape[a][2], 1446U);
            Uint lfq  = host_limit_freq(shape[a][3], 1638U);
            Uint lfq0 = host_limit_freq(shape[a][3], 1637U);

            for (b = 0; b < 4U; b++)
            {
                begin_scenario(seed++);
                gMainStatus.RunStep = STATUS_RUN;
                gMotorInfo.MotorType  = 0U;
                gMotorInfo.Current    = shape[a][1];
                gMotorInfo.CurrentGet = shape[a][2];
                gMotorInfo.FreqPer    = shape[a][3];
                ref_gLoadLose.ChkLevel = shape[a][0];
                ref_gLoadLose.ChkTime  = 100U;
                ref_gLoadLose.ErrCnt   = 0U;
                gSubCommand.bit.LoadLose = 1U;
                gMainCmd.Command.bit.StartDC = 0U;
                gMainCmd.Command.bit.StopDC  = 0U;
                gMainStatus.StatusWord.bit.OutOff = 1U;

                switch (b)
                {
                    case 0:
                        gMainCmd.FreqSyn = (int)(lfq + 5U);
                        gIUVWQ12.U = (int)lmt;
                        gIUVWQ12.V = 0;
                        gIUVWQ12.W = 0;
                        break;
                    case 1:
                        gMainCmd.FreqSyn = (int)(lfq + 5U);
                        gIUVWQ12.U = (int)lmt0;
                        gIUVWQ12.V = 0;
                        gIUVWQ12.W = 0;
                        break;
                    case 2:
                        gMainCmd.FreqSyn = (int)lfq;
                        gIUVWQ12.U = 0;
                        gIUVWQ12.V = 0;
                        gIUVWQ12.W = 0;
                        break;
                    default:
                        gMainCmd.FreqSyn = (int)lfq0;
                        gIUVWQ12.U = 0;
                        gIUVWQ12.V = 0;
                        gIUVWQ12.W = 0;
                        break;
                }
                g_covLlEdge++;
                end_scenario(F_LOADLOSE, "loadlose-edge");
            }
        }
    }
}

/* ---- buffer resistor ----------------------------------------------------- */

static void phase_buffres(void)
{
    static const Ulong vals[] = {
        0UL, 1UL, 149999UL, 150000UL, 150001UL, 199999UL, 200000UL, 200001UL,
        300000UL, 4294967295UL
    };
    size_t i, j;
    unsigned long seed = 800000UL;

    for (i = 0; i < sizeof(vals) / sizeof(vals[0]); i++)
    {
        for (j = 0; j < 4U; j++)
        {
            begin_scenario(seed++);
            ref_gBuffResCnt = vals[i];
            ref_gError.ErrorCode.all = (Ulong)j;
            end_scenario(F_BUFFRES, "buffres");
        }
    }
}

/* ---- integration: the real call order ------------------------------------ */

static void phase_integration(int steps)
{
    int s;
    unsigned long seed = 900000UL;

    for (s = 0; s < steps; s++)
    {
        begin_scenario(seed++);

        /* A running-ish state so InvDeviceControl's steps do real work rather
         * than short-circuiting on the first guard. */
        gMainStatus.RunStep = (s % 4 == 0) ? STATUS_LOW_POWER
                            : ((s % 3 == 0) ? STATUS_STOP : STATUS_RUN);
        gSubCommand.all = (Uint)(rnd() & 0xFFFFUL);
        gInvInfo.InvTypeApply = (Uint)(rnd() % 31UL);
        gInvInfo.InvVoltageType = (Uint)(rnd() % 4UL);
        gInvInfo.InvCurrent = 2000U;
        gInvInfo.InvCurrForP = 1000U;
        gMotorInfo.Current = 2000U;
        gComPar.MotorOvLoad = 100U;
        gLineCur.CurBaseInv = rnd_int(0, 6000);
        gLineCur.CurPer = rnd_int(0, 6000);
        gMainCmd.FreqReal = rnd_int(0, 4000);
        ref_gPhaseLose.Cnt = (Uint)(rnd() % 40000UL + 1UL);
        ref_gPhaseLose.Time = rnd_int(0, 120000000);
        ref_gPhaseLose.TotalU = (Ulong)ref_gPhaseLose.Cnt * (Ulong)(rnd() % 2000UL + 1UL);
        ref_gPhaseLose.TotalV = (Ulong)ref_gPhaseLose.Cnt * (Ulong)(rnd() % 2000UL + 1UL);
        ref_gPhaseLose.TotalW = (Ulong)ref_gPhaseLose.Cnt * (Ulong)(rnd() % 2000UL + 1UL);
        ref_gOverLoad.Cnt = rnd_int(0, 6);
        ref_gCBCProtect.Flag.all = (Uint16)(rnd() & 0x0007UL);

        end_scenario(F_INVDEV, "invdev");
    }
}

/* ---- random -------------------------------------------------------------- */

static void phase_random(int count)
{
    int c;

    for (c = 0; c < count; c++)
    {
        int fn = (int)(rnd() % (unsigned long)F_COUNT);
        unsigned long seed = 1000000UL + (unsigned long)c;

        begin_scenario(seed);

        /* Re-seed the fields the per-function guards depend on, keeping every
         * divisor non-zero and the phase-loss averages >= 1. */
        gComPar.MotorOvLoad = (Uint)(rnd_int(1, 1000));
        gComPar.PerMotorOvLoad = (Uint)(rnd_int(1, 100));
        gInvInfo.InvCurrent = (Uint)(rnd_int(1, 3000));
        gInvInfo.InvCurrForP = (Uint)(rnd_int(1, 3000));
        gMotorInfo.Current = (Uint)(rnd_int(1, 3000));
        gMotorInfo.CurrentGet = (Uint)rnd_int(0, 6000);
        seed_phase_lose(seed,
                        (Ulong)rnd_int(1, 4000), (Ulong)rnd_int(1, 4000),
                        (Ulong)rnd_int(1, 4000),
                        (Ulong)rnd_int(0, 120000000));
        ref_gOverLoad.Cnt = rnd_int(0, 6);
        ref_gCBCProtect.Flag.all = (Uint16)(rnd() & 0x0007UL);
        ref_gCBCProtect.EnableFlag = rnd_int(0, 1);
        gSubCommand.bit.CBCEnable = (Uint)rnd_int(0, 1);
        gSubCommand.bit.OutputLost = (Uint)rnd_int(0, 1);
        gSubCommand.bit.InputLost = (Uint)rnd_int(0, 1);
        gSubCommand.bit.LoadLose = (Uint)rnd_int(0, 1);
        gSubCommand.bit.ErrorOK = (Uint)rnd_int(0, 1);
        gMainStatus.RunStep = (Uint)rnd_int(0, 6);
        gMainCmd.FreqReal = rnd_int(0, 4000);
        gMainCmd.FreqSyn = rnd_int(-4000, 4000);
        gTemperature.Temp = (Uint)rnd_int(0, 120);
        ref_gFanCtrl.EnableCnt = (Uint)rnd_int(0, 1200);
        ref_gFanCtrl.RunCnt = (Uint)rnd_int(0, 6000);
        ref_gLoadLose.ErrCnt = (Uint)rnd_int(0, 200);
        ref_gLoadLose.ChkTime = (Uint)rnd_int(0, 1000);
        ref_gInLose.Cnt = (Uint)rnd_int(0, 600);
        ref_gInLose.UpCnt = (Uint)rnd_int(0, 600);
        ref_gInLose.UpCntRes = (Uint)rnd_int(0, 400);
        ref_gInLose.CntRes = (Uint)rnd_int(0, 400);
        ref_gBuffResCnt = (Ulong)rnd_int(0, 400000);

        end_scenario(fn, "random");
    }
}

/* ------------------------------------------------------------------------- */
/* main                                                                     */
/* ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long random = (argc > 1) ? atol(argv[1]) : 20000L;

    g_rng = 0x123456789ABCDEFUL;

    /* Width pins: the case shim folds `int` to the target's 16 bits so that
     * `Uint`/`int` storage matches the C28x. If that fold is ever lost, host-only
     * 32-bit storage silently hides mutations (the L28 class), so fail loudly. */
    if (sizeof(Uint) != 2U || sizeof(int) != 2U || sizeof(long) != 4U ||
        sizeof(Uint16) != 2U || sizeof(Uint32) != 4U || sizeof(Ulong) != 4U)
    {
        printf("FATAL: 宿主类型宽度与目标端不一致 (Uint=%u int=%u long=%u)\n",
               (unsigned)sizeof(Uint), (unsigned)sizeof(int), (unsigned)sizeof(long));
        return 2;
    }

    /* Equivalence pins for the six mutations deliberately NOT in the negative
     * control inventory. Each one is invisible because two quantities coincide;
     * the mutations have to come back if any pin stops holding (see the case
     * README, "equivalence removals"). */
    {
        static const Uint *const temp_tables[5] = {
            ref_gTempTableP44X,  ref_gTempTableP8XX, ref_gTempTableBSMXX,
            ref_gTempTableSEMIKON, ref_gTempTableWAIZHI
        };
        size_t t;
        size_t i;

        /* T12 / T14 / T16: the temperature search is inside the strictly
         * decreasing tables, because then AD == table[0] bisects down to index
         * 0 and lands on mType 192, AD == table[22] bisects to the (21, 22)
         * bracket and lands on mType 1600, and the bisection provably breaks
         * within 6 bodies -- so neither the two boundary tests nor the loop
         * bound can be told apart. */
        for (t = 0; t < 5U; t++)
        {
            for (i = 0; i + 1U < 23U; i++)
            {
                if (temp_tables[t][i] <= temp_tables[t][i + 1U])
                {
                    printf("FATAL: 温度表 %u 在 %u 处不再严格递减, "
                           "T12/T14/T16 不再等价\n",
                           (unsigned)t, (unsigned)i);
                    return 2;
                }
            }
        }

        /* L19: at m_Cur == m_TorCurUpper the mutant interpolates on the last
         * slot, where the weight (upper - bottom - index*step) is exactly 0
         * because (upper - bottom) is a whole number of steps; and both tables
         * end on the value the ">=" path returns. */
        if (((1870 - 1150) % 90) != 0 || ((1370 - 1050) % 40) != 0 ||
            ref_gInvOverLoadTable[8] != 20U ||
            ref_gInvOverLoadTableForP[8] != 10U)
        {
            printf("FATAL: 过载表末位或 (upper-bottom) 不再整除 step, "
                   "L19 不再等价\n");
            return 2;
        }

        /* L31: the cap path may read either of the last two motor-table slots. */
        if (ref_gMotorOverLoadTable[6] != ref_gMotorOverLoadTable[5])
        {
            printf("FATAL: 电机过载表末两位不再相等, L31 不再等价\n");
            return 2;
        }

        /* L28: "m_LDeta >= 448000" and "m_LDeta > 448000" differ only at
         * m_LDeta == 448000, and there the else path computes 448000 >> 8 =
         * 1750, exactly the constant the "if" path assigns. 448000 is
         * 1750 * 256, the cap constant itself. */
        if ((448000UL >> 8) != 1750U || (1750UL * 256UL) != 448000UL)
        {
            printf("FATAL: 448000>>8 != 1750, L28 不再等价\n");
            return 2;
        }
    }

    /* Layout assertions: the CBC flag bits are poked directly as *inputs*, so a
     * header change that moved them would silently re-target the poke. */
    {
        CBC_PROTECT_STRUCT probe;

        memset(&probe, 0, sizeof(probe));
        probe.Flag.bit.CBC_U = 1;
        if (probe.Flag.all != 0x0001U)
        {
            printf("FATAL: CBC Flag.CBC_U is not bit 0 (got 0x%04X)\n",
                   (unsigned)probe.Flag.all);
            return 2;
        }
        memset(&probe, 0, sizeof(probe));
        probe.Flag.bit.CBC_V = 1;
        if (probe.Flag.all != 0x0002U)
        {
            printf("FATAL: CBC Flag.CBC_V is not bit 1 (got 0x%04X)\n",
                   (unsigned)probe.Flag.all);
            return 2;
        }
        memset(&probe, 0, sizeof(probe));
        probe.Flag.bit.CBC_W = 1;
        if (probe.Flag.all != 0x0004U)
        {
            printf("FATAL: CBC Flag.CBC_W is not bit 2 (got 0x%04X)\n",
                   (unsigned)probe.Flag.all);
            return 2;
        }
    }

    phase_sw_err();
    phase_temp();
    phase_out_phase();
    phase_in_phase();
    phase_fan();
    phase_overload();
    phase_cbc();
    phase_loadlose();
    phase_buffres();
    phase_integration(4000);
    phase_random(random);

    printf("MotorInvProtect difftest: cases=%ld mismatches=%ld\n", g_cases, g_mism);
    printf("input reach: temp[%ld/%ld/%ld/%ld] ovScale=%ld ovGp0=%ld ovGp1=%ld "
           "outEnter=%ld inRes=%ld cbcTrip=%ld fanOn=%ld lowPower=%ld "
           "tempEdge=%ld outEdge=%ld ovEdge=%ld cbcEdge=%ld llEdge=%ld "
           "tempTable=%ld\n",
           g_covTemp1, g_covTemp2, g_covTemp3, g_covTemp4,
           g_covOvScale, g_covOvGp0, g_covOvGp1,
           g_covOutEnter, g_covInRes, g_covCbcTrip, g_covFanOn, g_covLowPower,
           g_covTempEdge, g_covOutEdge, g_covOvEdge, g_covCbcEdge, g_covLlEdge,
           g_covTempTable);

    if (g_covTemp1 == 0 || g_covTemp2 == 0 || g_covTemp3 == 0 || g_covTemp4 == 0)
    {
        printf("FATAL: TemperatureCheck 的四个表选择分支没有全部成为输入\n");
        return 2;
    }
    if (g_covOvScale == 0 || g_covOvGp0 == 0 || g_covOvGp1 == 0)
    {
        printf("FATAL: OverLoadProtect 的 InvTypeApply>=28 / GPType 分支没全成为输入\n");
        return 2;
    }
    if (g_covOutEnter == 0 || g_covInRes == 0 || g_covCbcTrip == 0 ||
        g_covFanOn == 0 || g_covLowPower == 0)
    {
        printf("FATAL: 有分支在差分测试里从未被输入触达\n");
        return 2;
    }
    if (g_covTempEdge == 0 || g_covOutEdge == 0 || g_covOvEdge == 0 ||
        g_covCbcEdge == 0 || g_covLlEdge == 0)
    {
        printf("FATAL: 边界探针没有全部跑起来\n");
        return 2;
    }
    if (g_covTempTable == 0)
    {
        printf("FATAL: 温度表选择的码值尺度探针没有跑起来\n");
        return 2;
    }

    return g_mism == 0 ? 0 : 1;
}
