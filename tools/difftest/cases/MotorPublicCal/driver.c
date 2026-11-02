/* Differential test driver for MotorPublicCal.
 *
 * Eight entry points, and they are not variations on one theme:
 *
 *   RunCaseShortGnd()     a 4-state sequencer. State 1 rewrites the three EPwm
 *                         timebases and forces the outputs low (AQCSFRC /
 *                         DBCTL); state 2 enables the drive; state 3 watches for
 *                         a fault and walks CMPA down in steps; state 4 restores
 *                         the dead-band and clears the trip.
 *   RunCaseLowPower()     a 3-state sequencer on the DC-bus filter, with a
 *                         100-count dwell before it acts. The threshold depends
 *                         on gInvInfo.InvVoltageType (220 V class gets +160,
 *                         otherwise +200).
 *   CBCLimitCurPrepare()  three independent TZFLG.CBC -> TZCLR.CBC latches.
 *   MaxUVWCurrent()       a pure function: max of |U>>12|, |V>>12|, |W>>12|.
 *   HardWareErrorDeal()   averages AIO2 over ten reads and branches to
 *                         HardWareOverUDCDeal() when the average is low, else
 *                         latches an over-current.
 *   HardWareOverUDCDeal() disable + latch over-voltage.
 *   RunCaseDcBrake()      DC-injection current ramp, then a PID on the line
 *                         current.
 *   BrakeResControl()     a chopper duty-cycle state machine on gBrake.Flag.
 *
 * ---- the thing that is easy to get wrong here: most "calls" are MACROS -----
 *
 * EnableDrive / DisableDrive / ConnectRelay / DisConnectRelay / TurnOnBrake /
 * TurnOffBrake are all `#define`s that write a single GpioDataRegs bit, and
 * GetTime() is a `#define` that reads CpuTimer1.RegsAddr->TIM.all. There is no
 * function call to observe, so the ONLY way to see "the rewrite dropped
 * DisableDrive()" is to make GpioDataRegs and CpuTimer1 real. Both are, and
 * both are compared whole.
 *
 * ---- what is compared -----------------------------------------------------
 *
 *   per-side    gDCBrake / gBrake / gShortGnd / gLowPower -- DEFINED by this
 *               module, so each side owns a renamed copy. Whole-struct memcmp.
 *   shared      EPwm1Regs, EPwm2Regs, EPwm3Regs, GpioDataRegs, the CPU timer,
 *               the PID call log, the CPU-control counters, and the fifteen
 *               globals owned by other modules.
 *   return      MaxUVWCurrent()'s value.
 *
 * Every input is filled with a deterministic PRNG stream before each case, so
 * "the rewrite stopped writing this field" is visible -- a field both sides
 * leave alone compares equal, and an all-zero image cannot tell "cleared it"
 * from "never touched it".
 *
 * ---- what is NOT covered --------------------------------------------------
 *
 *   - `int m_BrakeCur` in RunCaseDcBrake is READ WITHOUT BEING WRITTEN when
 *     neither Command.bit.StartDC nor Command.bit.StopDC is set. That is a
 *     pre-existing defect in the original, and the rewrite preserves it. It is
 *     not exercised here: a value that the source does not define cannot be
 *     differentially tested -- the two sides read whatever happened to be in
 *     their own stack frames, and a mismatch would say nothing about the code.
 *     The systematic sweep always sets StartDC or StopDC.
 *   - `abs()` is the project's `extern int abs(int)`, so `abs(gIUVWQ24.U>>12)`
 *     narrows a `long` to an `int`. On C28x `int` is 16 bits, on the host 32,
 *     so the truncation point differs. Both sides are host code, so the
 *     comparison stays valid; the absolute values are not transferable.
 *   - the EPWM_REGS / GPIO_DATA_REGS / CPUTIMER_REGS field OFFSETS are the
 *     host's, not the target's: the structs mix 16- and 32-bit members, so
 *     MSVC inserts alignment padding the C28x word-addressed layout does not
 *     have. Ref and new share the host layout, which is what the comparison
 *     needs; the target offsets are a separate question (target_layout.py).
 *   - the PID_STRUCT fields the module writes are compared, but PID() itself is
 *     a stub -- see mock_pub_cal.c. What the real PID would compute is not
 *     tested here.
 *   - EALLOW/EDIS as instructions -- only that they occurred.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MotorPublicCalInclude.h"

#define FAIL_LIMIT 20

/* ---- ref side ------------------------------------------------------------ */

extern void  ref_RunCaseShortGnd(void);
extern void  ref_RunCaseLowPower(void);
extern void  ref_CBCLimitCurPrepare(void);
extern Uint  ref_MaxUVWCurrent(void);
extern void  ref_HardWareErrorDeal(void);
extern void  ref_HardWareOverUDCDeal(void);
extern void  ref_RunCaseDcBrake(void);
extern void  ref_BrakeResControl(void);
extern DC_BRAKE_STRUCT      ref_gDCBrake;
extern BRAKE_CONTROL_STRUCT ref_gBrake;
extern SHORT_GND_STRUCT     ref_gShortGnd;
extern JUDGE_POWER_LOW      ref_gLowPower;

/* ---- new side ------------------------------------------------------------ */

extern void  new_RunCaseShortGnd(void);
extern void  new_RunCaseLowPower(void);
extern void  new_CBCLimitCurPrepare(void);
extern Uint  new_MaxUVWCurrent(void);
extern void  new_HardWareErrorDeal(void);
extern void  new_HardWareOverUDCDeal(void);
extern void  new_RunCaseDcBrake(void);
extern void  new_BrakeResControl(void);
extern DC_BRAKE_STRUCT      new_gDCBrake;
extern BRAKE_CONTROL_STRUCT new_gBrake;
extern SHORT_GND_STRUCT     new_gShortGnd;
extern JUDGE_POWER_LOW      new_gLowPower;

/* ---- instrumentation provided by the mock -------------------------------- */

#define PID_LOG_LEN 32
extern PID_STRUCT gPidLog[PID_LOG_LEN];
extern Uint16     gPidCount;
extern Uint16     gPidOutWord;
extern volatile Uint16 gHostCpuCtl[7];
extern volatile struct CPUTIMER_REGS gHostCpuTimer1Regs;
extern struct CPUTIMER_VARS          CpuTimer1;

/* ---- per-side snapshot --------------------------------------------------- */

struct SIDE
{
    DC_BRAKE_STRUCT      dcb;
    BRAKE_CONTROL_STRUCT brk;
    SHORT_GND_STRUCT     sg;
    JUDGE_POWER_LOW      lp;
};

static struct SIDE S[2];

/* ---- input image --------------------------------------------------------- */

static RUN_STATUS_STRUCT                  inSt;
static UDC_STRUCT                         inUdc;
static ADC_STRUCT                         inAdc;
static struct FAULT_CODE_INFOR_STRUCT_DEF inErr;
static INV_STRUCT                         inInv;
static CUR_EXCURSION_STRUCT               inExc;
static Ulong                              inBuffResCnt;
static ANGLE_STRUCT                       inPhase;
static CBC_PROTECT_STRUCT                 inCbc;
static UVW_STRUCT_Q24                     inIuvw;
static LINE_CURRENT_STRUCT                inLineCur;
static BASE_COMMAND_STRUCT                inCmd;
static COM_PAR_INFO_STRUCT                inCompar;
static OUT_VOLT_STRUCT                    inOutVolt;
static MOTOR_EXTERN_STRUCT                inMotorExtPer;
static VF_INFO_STRUCT                     inVfPar;

static DC_BRAKE_STRUCT      inDcb;
static BRAKE_CONTROL_STRUCT inBrk;
static SHORT_GND_STRUCT     inSg;
static JUDGE_POWER_LOW      inLp;

static struct EPWM_REGS       inE1, inE2, inE3;
static struct GPIO_DATA_REGS  inGpio;
static struct CPUTIMER_REGS   inTimer;

/* ---- deterministic input fill -------------------------------------------- */

static unsigned long g_rng = 0x20261009UL;

static unsigned long rnd(void)
{
    g_rng = g_rng * 1103515245UL + 12345UL;
    return (g_rng >> 8) & 0xFFFFFFUL;
}

static void rnd_fill(void *p, size_t n)
{
    unsigned char *b = (unsigned char *)p;
    size_t i;

    for (i = 0; i < n; i++)
    {
        g_rng = g_rng * 1103515245UL + 12345UL;
        b[i] = (unsigned char)(g_rng >> 16);
    }
}

/* 每个用例开头把所有输入打散成伪随机。理由同其它用例: ref-vs-new 只看得到 new
 * 侧的变化, 两侧都保留原值时它们当然相等 —— "改写不再写这个字段"只有在该位置
 * 原本就有一个**不等于正确值**的数时才可见。 */
static void case_begin(void)
{
    rnd_fill(&inSt,          sizeof inSt);
    rnd_fill(&inUdc,         sizeof inUdc);
    rnd_fill(&inAdc,         sizeof inAdc);
    rnd_fill(&inErr,         sizeof inErr);
    rnd_fill(&inInv,         sizeof inInv);
    rnd_fill(&inExc,         sizeof inExc);
    rnd_fill(&inBuffResCnt,  sizeof inBuffResCnt);
    rnd_fill(&inPhase,       sizeof inPhase);
    rnd_fill(&inCbc,         sizeof inCbc);
    rnd_fill(&inIuvw,        sizeof inIuvw);
    rnd_fill(&inLineCur,     sizeof inLineCur);
    rnd_fill(&inCmd,         sizeof inCmd);
    rnd_fill(&inCompar,      sizeof inCompar);
    rnd_fill(&inOutVolt,     sizeof inOutVolt);
    rnd_fill(&inMotorExtPer, sizeof inMotorExtPer);
    rnd_fill(&inVfPar,       sizeof inVfPar);

    rnd_fill(&inDcb, sizeof inDcb);
    rnd_fill(&inBrk, sizeof inBrk);
    rnd_fill(&inSg,  sizeof inSg);
    rnd_fill(&inLp,  sizeof inLp);

    rnd_fill(&inE1,    sizeof inE1);
    rnd_fill(&inE2,    sizeof inE2);
    rnd_fill(&inE3,    sizeof inE3);
    rnd_fill(&inGpio,  sizeof inGpio);
    rnd_fill(&inTimer, sizeof inTimer);
}

/* ---- install / snapshot -------------------------------------------------- */

static void install_side(int side)
{
    if (side == 0)
    {
        memcpy(&ref_gDCBrake,  &inDcb, sizeof inDcb);
        memcpy(&ref_gBrake,    &inBrk, sizeof inBrk);
        memcpy(&ref_gShortGnd, &inSg,  sizeof inSg);
        memcpy(&ref_gLowPower, &inLp,  sizeof inLp);
    }
    else
    {
        memcpy(&new_gDCBrake,  &inDcb, sizeof inDcb);
        memcpy(&new_gBrake,    &inBrk, sizeof inBrk);
        memcpy(&new_gShortGnd, &inSg,  sizeof inSg);
        memcpy(&new_gLowPower, &inLp,  sizeof inLp);
    }
}

static void snapshot(int side)
{
    struct SIDE *s = &S[side];

    if (side == 0)
    {
        memcpy(&s->dcb, &ref_gDCBrake,  sizeof s->dcb);
        memcpy(&s->brk, &ref_gBrake,    sizeof s->brk);
        memcpy(&s->sg,  &ref_gShortGnd, sizeof s->sg);
        memcpy(&s->lp,  &ref_gLowPower, sizeof s->lp);
    }
    else
    {
        memcpy(&s->dcb, &new_gDCBrake,  sizeof s->dcb);
        memcpy(&s->brk, &new_gBrake,    sizeof s->brk);
        memcpy(&s->sg,  &new_gShortGnd, sizeof s->sg);
        memcpy(&s->lp,  &new_gLowPower, sizeof s->lp);
    }
}

/* ---- shared snapshot ----------------------------------------------------- */

struct SHARED
{
    struct EPWM_REGS          e1, e2, e3;
    struct GPIO_DATA_REGS     gpio;
    struct CPUTIMER_REGS      timer;
    PID_STRUCT                pidLog[PID_LOG_LEN];
    Uint16                    pidCount;
    Uint16                    pidOutWord;
    Uint16                    cpu[7];
    RUN_STATUS_STRUCT         st;
    UDC_STRUCT                udc;
    ADC_STRUCT                adc;
    struct FAULT_CODE_INFOR_STRUCT_DEF err;
    INV_STRUCT                inv;
    CUR_EXCURSION_STRUCT      exc;
    Ulong                     buffResCnt;
    ANGLE_STRUCT              phase;
    CBC_PROTECT_STRUCT        cbc;
    UVW_STRUCT_Q24            iuvw;
    LINE_CURRENT_STRUCT       lineCur;
    BASE_COMMAND_STRUCT       cmd;
    COM_PAR_INFO_STRUCT       compar;
    OUT_VOLT_STRUCT           outVolt;
    MOTOR_EXTERN_STRUCT       motorExtPer;
    VF_INFO_STRUCT            vfpar;
};

static struct SHARED SH[2];
static Uint g_ret[2];

static void install_shared(void)
{
    int i;

    memcpy((void *)&EPwm1Regs, &inE1, sizeof inE1);
    memcpy((void *)&EPwm2Regs, &inE2, sizeof inE2);
    memcpy((void *)&EPwm3Regs, &inE3, sizeof inE3);
    memcpy((void *)&GpioDataRegs, &inGpio, sizeof inGpio);
    memcpy((void *)&gHostCpuTimer1Regs, &inTimer, sizeof inTimer);

    CpuTimer1.RegsAddr = &gHostCpuTimer1Regs;

    memset(gPidLog, 0, sizeof gPidLog);
    gPidCount = 0;
    gPidOutWord = 0;

    for (i = 0; i < 7; i++)
    {
        gHostCpuCtl[i] = 0;
    }

    memcpy(&gMainStatus,   &inSt,          sizeof inSt);
    memcpy(&gUDC,          &inUdc,         sizeof inUdc);
    memcpy(&gADC,          &inAdc,         sizeof inAdc);
    memcpy(&gError,        &inErr,         sizeof inErr);
    memcpy(&gInvInfo,      &inInv,         sizeof inInv);
    memcpy(&gExcursionInfo, &inExc,        sizeof inExc);
    gBuffResCnt = inBuffResCnt;
    memcpy(&gPhase,        &inPhase,       sizeof inPhase);
    memcpy(&gCBCProtect,   &inCbc,         sizeof inCbc);
    memcpy(&gIUVWQ24,      &inIuvw,        sizeof inIuvw);
    memcpy(&gLineCur,      &inLineCur,     sizeof inLineCur);
    memcpy(&gMainCmd,      &inCmd,         sizeof inCmd);
    memcpy(&gComPar,       &inCompar,      sizeof inCompar);
    memcpy(&gOutVolt,      &inOutVolt,     sizeof inOutVolt);
    memcpy(&gMotorExtPer,  &inMotorExtPer, sizeof inMotorExtPer);
    memcpy(&gVFPar,        &inVfPar,       sizeof inVfPar);
}

static void snap_shared(int side)
{
    struct SHARED *s = &SH[side];
    int i;

    memcpy(&s->e1,   (const void *)&EPwm1Regs, sizeof s->e1);
    memcpy(&s->e2,   (const void *)&EPwm2Regs, sizeof s->e2);
    memcpy(&s->e3,   (const void *)&EPwm3Regs, sizeof s->e3);
    memcpy(&s->gpio, (const void *)&GpioDataRegs, sizeof s->gpio);
    memcpy(&s->timer, (const void *)&gHostCpuTimer1Regs, sizeof s->timer);

    memcpy(s->pidLog, gPidLog, sizeof s->pidLog);
    s->pidCount = gPidCount;
    s->pidOutWord = gPidOutWord;

    for (i = 0; i < 7; i++)
    {
        s->cpu[i] = gHostCpuCtl[i];
    }

    memcpy(&s->st,          &gMainStatus,   sizeof s->st);
    memcpy(&s->udc,         &gUDC,          sizeof s->udc);
    memcpy(&s->adc,         &gADC,          sizeof s->adc);
    memcpy(&s->err,         &gError,        sizeof s->err);
    memcpy(&s->inv,         &gInvInfo,      sizeof s->inv);
    memcpy(&s->exc,         &gExcursionInfo, sizeof s->exc);
    s->buffResCnt = gBuffResCnt;
    memcpy(&s->phase,       &gPhase,        sizeof s->phase);
    memcpy(&s->cbc,         &gCBCProtect,   sizeof s->cbc);
    memcpy(&s->iuvw,        &gIUVWQ24,      sizeof s->iuvw);
    memcpy(&s->lineCur,     &gLineCur,      sizeof s->lineCur);
    memcpy(&s->cmd,         &gMainCmd,      sizeof s->cmd);
    memcpy(&s->compar,      &gComPar,       sizeof s->compar);
    memcpy(&s->outVolt,     &gOutVolt,      sizeof s->outVolt);
    memcpy(&s->motorExtPer, &gMotorExtPer,  sizeof s->motorExtPer);
    memcpy(&s->vfpar,       &gVFPar,        sizeof s->vfpar);
}

/* ---- case runner --------------------------------------------------------- */

static long g_cases = 0;
static int  g_fails = 0;
static const char *g_cur = "";

static void report(const char *what)
{
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s: %s\n", g_cur, what);
    printf("  SubStep=%u RunStep=%u StartDC=%u StopDC=%u InvTypeApply=%u\n",
           (unsigned)inSt.SubStep, (unsigned)inSt.RunStep,
           (unsigned)inCmd.Command.bit.StartDC,
           (unsigned)inCmd.Command.bit.StopDC,
           (unsigned)inInv.InvTypeApply);
    printf("  ErrCode=0x%08lX BrakeCoff=%u uDC=%u uDCFilter=%u\n",
           (unsigned long)inErr.ErrorCode.all, (unsigned)inCompar.BrakeCoff,
           (unsigned)inUdc.uDC, (unsigned)inUdc.uDCFilter);
    printf("  sg.Comper=%u BaseUDC=%u ocFlag=%u ShortCur=%d\n",
           (unsigned)inSg.Comper, (unsigned)inSg.BaseUDC,
           (unsigned)inSg.ocFlag, inSg.ShortCur);
    printf("  brk.Flag=0x%04X Cnt=%u OnCnt=%u OffCnt=%u VoltageLim=%u\n",
           (unsigned)inBrk.Flag, (unsigned)inBrk.Cnt,
           (unsigned)inBrk.OnCnt, (unsigned)inBrk.OffCnt,
           (unsigned)inBrk.VoltageLim);
    printf("  dcb.Time=%d lp.WaiteTime=%u lp.UDCOld=%u ExcEnable=%d\n",
           inDcb.Time, (unsigned)inLp.WaiteTime, (unsigned)inLp.UDCOld,
           inExc.EnableCount);
    printf("  ret %u/%u  pidCount %u/%u\n",
           (unsigned)g_ret[0], (unsigned)g_ret[1],
           (unsigned)SH[0].pidCount, (unsigned)SH[1].pidCount);
}

static void cmp(const char *what, const void *a, const void *b, size_t n)
{
    if (memcmp(a, b, n) != 0)
    {
        report(what);
    }
}

static void compare(void)
{
    cmp("gDCBrake",  &S[0].dcb, &S[1].dcb, sizeof S[0].dcb);
    cmp("gBrake",    &S[0].brk, &S[1].brk, sizeof S[0].brk);
    cmp("gShortGnd", &S[0].sg,  &S[1].sg,  sizeof S[0].sg);
    cmp("gLowPower", &S[0].lp,  &S[1].lp,  sizeof S[0].lp);

    cmp("EPwm1Regs",   &SH[0].e1,   &SH[1].e1,   sizeof SH[0].e1);
    cmp("EPwm2Regs",   &SH[0].e2,   &SH[1].e2,   sizeof SH[0].e2);
    cmp("EPwm3Regs",   &SH[0].e3,   &SH[1].e3,   sizeof SH[0].e3);
    cmp("GpioDataRegs", &SH[0].gpio, &SH[1].gpio, sizeof SH[0].gpio);
    cmp("CpuTimer1Regs", &SH[0].timer, &SH[1].timer, sizeof SH[0].timer);

    cmp("pidLog",     SH[0].pidLog, SH[1].pidLog, sizeof SH[0].pidLog);
    cmp("pidCount",   &SH[0].pidCount, &SH[1].pidCount, sizeof SH[0].pidCount);
    cmp("pidOutWord", &SH[0].pidOutWord, &SH[1].pidOutWord,
        sizeof SH[0].pidOutWord);
    cmp("cpuCtl",     SH[0].cpu, SH[1].cpu, sizeof SH[0].cpu);

    cmp("gMainStatus",   &SH[0].st,   &SH[1].st,   sizeof SH[0].st);
    cmp("gUDC",          &SH[0].udc,  &SH[1].udc,  sizeof SH[0].udc);
    cmp("gADC",          &SH[0].adc,  &SH[1].adc,  sizeof SH[0].adc);
    cmp("gError",        &SH[0].err,  &SH[1].err,  sizeof SH[0].err);
    cmp("gInvInfo",      &SH[0].inv,  &SH[1].inv,  sizeof SH[0].inv);
    cmp("gExcursionInfo", &SH[0].exc, &SH[1].exc,  sizeof SH[0].exc);
    cmp("gBuffResCnt",   &SH[0].buffResCnt, &SH[1].buffResCnt,
        sizeof SH[0].buffResCnt);
    cmp("gPhase",        &SH[0].phase, &SH[1].phase, sizeof SH[0].phase);
    cmp("gCBCProtect",   &SH[0].cbc,  &SH[1].cbc,  sizeof SH[0].cbc);
    cmp("gIUVWQ24",      &SH[0].iuvw, &SH[1].iuvw, sizeof SH[0].iuvw);
    cmp("gLineCur",      &SH[0].lineCur, &SH[1].lineCur, sizeof SH[0].lineCur);
    cmp("gMainCmd",      &SH[0].cmd,  &SH[1].cmd,  sizeof SH[0].cmd);
    cmp("gComPar",       &SH[0].compar, &SH[1].compar, sizeof SH[0].compar);
    cmp("gOutVolt",      &SH[0].outVolt, &SH[1].outVolt, sizeof SH[0].outVolt);
    cmp("gMotorExtPer",  &SH[0].motorExtPer, &SH[1].motorExtPer,
        sizeof SH[0].motorExtPer);
    cmp("gVFPar",        &SH[0].vfpar, &SH[1].vfpar, sizeof SH[0].vfpar);

    if (g_ret[0] != g_ret[1])
    {
        report("MaxUVWCurrent return");
    }
}

typedef void (*void_fn)(void);

static void_fn g_refFn[8];
static void_fn g_newFn[8];
static const char *g_fnName[8] = {
    "RunCaseShortGnd", "RunCaseLowPower", "CBCLimitCurPrepare",
    "MaxUVWCurrent", "HardWareErrorDeal", "HardWareOverUDCDeal",
    "RunCaseDcBrake", "BrakeResControl"
};

static void init_ptrs(void)
{
    g_refFn[0] = ref_RunCaseShortGnd;     g_newFn[0] = new_RunCaseShortGnd;
    g_refFn[1] = ref_RunCaseLowPower;     g_newFn[1] = new_RunCaseLowPower;
    g_refFn[2] = ref_CBCLimitCurPrepare;  g_newFn[2] = new_CBCLimitCurPrepare;
    g_refFn[3] = (void_fn)ref_MaxUVWCurrent;
    g_newFn[3] = (void_fn)new_MaxUVWCurrent;
    g_refFn[4] = ref_HardWareErrorDeal;   g_newFn[4] = new_HardWareErrorDeal;
    g_refFn[5] = ref_HardWareOverUDCDeal; g_newFn[5] = new_HardWareOverUDCDeal;
    g_refFn[6] = ref_RunCaseDcBrake;      g_newFn[6] = new_RunCaseDcBrake;
    g_refFn[7] = ref_BrakeResControl;     g_newFn[7] = new_BrakeResControl;
}

static void run_case(int which)
{
    g_cur = g_fnName[which];

    install_shared();
    install_side(0);
    if (which == 3)
    {
        g_ret[0] = ref_MaxUVWCurrent();
    }
    else
    {
        g_refFn[which]();
        g_ret[0] = 0;
    }
    snapshot(0);
    snap_shared(0);

    install_shared();
    install_side(1);
    if (which == 3)
    {
        g_ret[1] = new_MaxUVWCurrent();
    }
    else
    {
        g_newFn[which]();
        g_ret[1] = 0;
    }
    snapshot(1);
    snap_shared(1);

    g_cases++;
    compare();
}

/* ---- phases -------------------------------------------------------------- */

/* RunCaseShortGnd: four states plus the default. The state-3 fault test is a
 * disjunction of three terms, and state 3's second half is a two-way branch on
 * the comparator, so both edges of each are swept. */
static void phase_short_gnd(void)
{
    static const int sub[] = { 0, 1, 2, 3, 4, 5, 7 };
    static const int oc[] = { 0, 1, 2 };
    static const int sc[] = { 0, 960, 961, -960, -961, 30000, -30000 };
    static const int dUDC[] = { 0, 650, 651, 65535 };
    static const int cmpr[] = { SHORT_GND_CMPR_INC, SHORT_GND_CMPR_INC + 1, 0 };
    static const unsigned long dly[] = { 0UL, 1UL, 100UL, 0xFFFFUL };
    unsigned a, b, c, d, e, k;

    for (a = 0; a < sizeof sub / sizeof sub[0]; a++)
    {
        for (b = 0; b < sizeof oc / sizeof oc[0]; b++)
        {
            for (c = 0; c < sizeof sc / sizeof sc[0]; c++)
            {
                for (d = 0; d < sizeof dUDC / sizeof dUDC[0]; d++)
                {
                    for (e = 0; e < sizeof cmpr / sizeof cmpr[0]; e++)
                    {
                        for (k = 0; k < 2; k++)
                        {
                            case_begin();
                            inSt.SubStep = (Uint)sub[a];
                            inSg.ocFlag = (Uint)oc[b];
                            inSg.ShortCur = sc[c];
                            inSg.BaseUDC = 3000;
                            inUdc.uDC = (Uint)(3000 + dUDC[d]);
                            inSg.Comper = (Uint)cmpr[e];
                            inAdc.DelayApply = (Uint)dly[(a + k) & 3];
                            inSt.ErrFlag.bit.OvCurFlag = (Uint)k;
                            inGpio.GPASET.bit.GPIO7 = (Uint)(k ^ 1);
                            inGpio.GPACLEAR.bit.GPIO7 = (Uint)k;
                            inE1.TBPRD = (Uint16)rnd();
                            inE1.CMPB = (Uint16)rnd();
                            inE1.DBCTL.all = (Uint32)rnd();
                            inE1.AQCSFRC.all = (Uint32)rnd();
                            run_case(0);
                        }
                    }
                }
            }
        }
    }
}

/* RunCaseLowPower: the threshold is InvLowUDC + (220V ? 160 : 200), so the
 * sweep walks the filter across both sides of it. */
static void phase_low_power(void)
{
    static const int sub[] = { 0, 1, 2, 3, 4, 5 };
    static const int vt[] = { INV_VOLTAGE_220V, 1, 2, 255 };
    static const int lowUDC[] = { 0, 1000, 65535 };
    static const int dFilt[] = { -1, 0, 1 };
    static const int dOld[] = { -1, 0, 1, 1000 };
    static const int wait[] = { 0, 99, 100, 101 };
    static const int en[] = { 0, 199, 200, 201 };
    static const unsigned long errc[] = { 0UL, ERROR_LOW_UDC, 0xFFFFFFFFUL };
    unsigned a, b, c, d, e, f, g, h, k;
    int limit;

    for (a = 0; a < sizeof sub / sizeof sub[0]; a++)
    {
        for (b = 0; b < sizeof vt / sizeof vt[0]; b++)
        {
            for (c = 0; c < sizeof lowUDC / sizeof lowUDC[0]; c++)
            {
                for (d = 0; d < sizeof dFilt / sizeof dFilt[0]; d++)
                {
                    for (e = 0; e < sizeof dOld / sizeof dOld[0]; e++)
                    {
                        for (f = 0; f < sizeof wait / sizeof wait[0]; f++)
                        {
                            for (g = 0; g < sizeof en / sizeof en[0]; g++)
                            {
                                for (h = 0; h < sizeof errc / sizeof errc[0]; h++)
                                {
                                    for (k = 0; k < 2; k++)
                                    {
                                        case_begin();
                                        inInv.InvVoltageType = (Uint)vt[b];
                                        inInv.InvLowUDC = (Uint)lowUDC[c];
                                        limit = lowUDC[c] +
                                                (vt[b] == INV_VOLTAGE_220V
                                                 ? 160 : 200);
                                        inUdc.uDCFilter =
                                            (Uint)(limit + dFilt[d]);
                                        inLp.UDCOld = (Uint)(limit + dOld[e]);
                                        inLp.WaiteTime = (Uint)wait[f];
                                        inExc.EnableCount = en[g];
                                        inErr.ErrorCode.all = errc[h];
                                        inSt.ErrFlag.bit.OvCurFlag = (Uint)k;
                                        inSt.SubStep = (Uint)sub[a];
                                        run_case(1);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/* CBCLimitCurPrepare: three independent latches, so all eight combinations of
 * the three TZFLG.CBC bits. */
static void phase_cbc(void)
{
    unsigned m, k;

    for (m = 0; m < 8; m++)
    {
        for (k = 0; k < 2; k++)
        {
            case_begin();
            inE1.TZFLG.bit.CBC = (Uint16)(m & 1);
            inE2.TZFLG.bit.CBC = (Uint16)((m >> 1) & 1);
            inE3.TZFLG.bit.CBC = (Uint16)((m >> 2) & 1);
            inE1.TZCLR.all = (Uint32)rnd();
            inE2.TZCLR.all = (Uint32)rnd();
            inE3.TZCLR.all = (Uint32)rnd();
            run_case(2);
        }
    }
}

/* MaxUVWCurrent: a pure function, so the sweep is a cross of interesting
 * values around the >>12 truncation and the abs() sign change. */
static void phase_max_uvw(void)
{
    static const long v[] = {
        0L, 1L, 0x0FFFL, 0x1000L, 0x1001L, -0x1000L, -0x1001L,
        0x7FFFL, -0x8000L, 0x7FFFFFFFL, -0x7FFFFFFFL, 0x80000000L
    };
    unsigned a, b, c;

    for (a = 0; a < sizeof v / sizeof v[0]; a++)
    {
        for (b = 0; b < sizeof v / sizeof v[0]; b++)
        {
            for (c = 0; c < sizeof v / sizeof v[0]; c++)
            {
                case_begin();
                inIuvw.U = v[a];
                inIuvw.V = v[b];
                inIuvw.W = v[c];
                run_case(3);
            }
        }
    }
}

/* HardWareErrorDeal: ten reads of GpioDataRegs.AIODAT.bit.AIO2, so the sum is
 * 0 or 10 and the < 5 branch is the whole question. */
static void phase_hw_error(void)
{
    static const int aio[] = { 0, 1 };
    static const unsigned long errc[] = { 0UL, ERROR_OVER_CURRENT,
                                          0xFFFFFFFFUL };
    static const int runStep[] = { 0, STATUS_SHORT_GND, STATUS_STOP, 7 };
    static const long cur[] = { 0L, 0x1000L, -0x1000L, 0x7FFFFFFFL };
    unsigned a, b, c, d, k;

    for (a = 0; a < sizeof aio / sizeof aio[0]; a++)
    {
        for (b = 0; b < sizeof errc / sizeof errc[0]; b++)
        {
            for (c = 0; c < sizeof runStep / sizeof runStep[0]; c++)
            {
                for (d = 0; d < sizeof cur / sizeof cur[0]; d++)
                {
                    for (k = 0; k < 2; k++)
                    {
                        case_begin();
                        inGpio.AIODAT.bit.AIO2 = (Uint)aio[a];
                        inErr.ErrorCode.all = errc[b];
                        inSt.RunStep = (Uint)runStep[c];
                        inIuvw.U = cur[d];
                        inIuvw.V = -cur[d];
                        inIuvw.W = cur[(d + 1) % 4];
                        inSt.ErrFlag.bit.OvCurFlag = (Uint)k;
                        run_case(4);
                    }
                }
            }
        }
    }

    /* HardWareOverUDCDeal on its own: both the short-ground and the ordinary
     * branch, and both states of the trip bit. */
    for (c = 0; c < 2; c++)
    {
        for (k = 0; k < 2; k++)
        {
            case_begin();
            inSt.RunStep = (Uint)(c ? STATUS_SHORT_GND : STATUS_STOP);
            inSt.ErrFlag.bit.OvCurFlag = (Uint)k;
            run_case(5);
        }
    }
}

/* RunCaseDcBrake: StartDC/StopDC are always set (see the file header -- the
 * "neither" case reads an uninitialised local). gDCBrake.Time is swept across
 * the `< 2` boundary and InvTypeApply across the `16 <` boundary. */
static void phase_dc_brake(void)
{
    static const int startDC[] = { 0, 1 };
    static const int stopDC[] = { 0, 1 };
    static const int time[] = { -5, -1, 0, 1, 2, 9 };
    static const int invType[] = { 0, 16, 17, 100 };
    static const unsigned long cur[] = { 0UL, 1UL, 100UL, 65535UL };
    static const int curPer[] = { 0, 100, 65535 };
    static const int r1[] = { 0, 1, 1000, 65535 };
    unsigned a, b, c, d, e, f, g, k;

    for (a = 0; a < 2; a++)
    {
        for (b = 0; b < 2; b++)
        {
            if (!startDC[a] && !stopDC[b])
            {
                continue;
            }
            for (c = 0; c < sizeof time / sizeof time[0]; c++)
            {
                for (d = 0; d < sizeof invType / sizeof invType[0]; d++)
                {
                    for (e = 0; e < sizeof cur / sizeof cur[0]; e++)
                    {
                        for (f = 0; f < sizeof curPer / sizeof curPer[0]; f++)
                        {
                            for (g = 0; g < sizeof r1 / sizeof r1[0]; g++)
                            {
                                for (k = 0; k < 2; k++)
                                {
                                    case_begin();
                                    inCmd.Command.bit.StartDC = (Uint)startDC[a];
                                    inCmd.Command.bit.StopDC = (Uint)stopDC[b];
                                    inCompar.StartDCBrakeCur = (Uint)cur[e];
                                    inCompar.StopDCBrakeCur = (Uint)cur[e];
                                    inDcb.Time = time[c];
                                    inInv.InvTypeApply = (Uint)invType[d];
                                    inLineCur.CurPer = (Uint)curPer[f];
                                    inMotorExtPer.R1 = (int)r1[g];
                                    inOutVolt.Volt = (Uint)(k ? 0xFFFF : 0);
                                    run_case(6);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/* BrakeResControl: the four-way BrakeCoff split (100 / 0 / <=50 / else), the
 * OnCnt == 0 early exit, and the gBrake.Flag state machine. */
static void phase_brake_res(void)
{
    static const unsigned coff[] = { 0, 1, 2, 50, 51, 99, 100, 101, 255 };
    /* Offsets around *both* thresholds BrakeResControl tests against the
     * computed limit: `uDC < VoltageLim + 90` and `uDC > VoltageLim + 110`.
     * Landing exactly on the boundary is what makes `+90 -> +91` and
     * `+110 -> +111` observable; a grid that only samples 89/90/91 leaves the
     * 110 boundary untested. */
    static const int dUDC[] = {
        -200, -111, -110, -109, -91, -90, -89, -1, 0,
        1, 89, 90, 91, 109, 110, 111, 200
    };
    static const unsigned flag[] = { 0, 1, 2, 3, 0xFFFF };
    static const unsigned cnt[] = { 0, 1, 2, 65534, 65535 };
    static const unsigned onCnt[] = { 0, 1, 2, 65535 };
    static const unsigned ovP[] = { 0, 1, 100, 65535 };
    static const unsigned ovC[] = { 0, 1, 100, 65535 };
    unsigned a, b, c, d, e, f, g, k;
    unsigned lim;

    for (a = 0; a < sizeof coff / sizeof coff[0]; a++)
    {
        for (b = 0; b < sizeof dUDC / sizeof dUDC[0]; b++)
        {
            for (c = 0; c < sizeof flag / sizeof flag[0]; c++)
            {
                for (d = 0; d < sizeof cnt / sizeof cnt[0]; d++)
                {
                    for (e = 0; e < sizeof onCnt / sizeof onCnt[0]; e++)
                    {
                        for (f = 0; f < sizeof ovP / sizeof ovP[0]; f += 3)
                        {
                            for (g = 0; g < sizeof ovC / sizeof ovC[0]; g += 3)
                            {
                                for (k = 0; k < 2; k++)
                                {
                                    case_begin();
                                    inCompar.BrakeCoff = coff[a];
                                    inVfPar.ovPoint = ovP[f];
                                    inVfPar.ovPointCoff = ovC[g];
                                    lim = ovP[f] * ovC[g];
                                    inUdc.uDC = (Uint)(lim + (unsigned)dUDC[b]);
                                    inBrk.Flag = flag[c];
                                    inBrk.Cnt = cnt[d];
                                    inBrk.OnCnt = (Uint)(k ? onCnt[e] : 0);
                                    inBrk.OffCnt = (Uint)rnd();
                                    inGpio.GPASET.bit.GPIO8 = 0;
                                    inGpio.GPACLEAR.bit.GPIO8 = 0;
                                    run_case(7);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/* ---- random sweep -------------------------------------------------------- */

static void phase_random(long n)
{
    long k;

    for (k = 0; k < n; k++)
    {
        case_begin();

        switch (rnd() % 8)
        {
            case 0:
                inSt.SubStep = (Uint)(rnd() % 8);
                inSg.ocFlag = (Uint)(rnd() % 3);
                inSg.ShortCur = (int)(rnd() % 2000) - 1000;
                inSg.BaseUDC = (Uint)rnd();
                inUdc.uDC = (Uint)rnd();
                inSg.Comper = (Uint)rnd();
                inSt.ErrFlag.bit.OvCurFlag = (Uint)(rnd() & 1);
                run_case(0);
                break;

            case 1:
                inSt.SubStep = (Uint)(rnd() % 6);
                inInv.InvVoltageType = (Uint)(rnd() % 4);
                inInv.InvLowUDC = (Uint)rnd();
                inUdc.uDCFilter = (Uint)rnd();
                inLp.UDCOld = (Uint)rnd();
                inLp.WaiteTime = (Uint)(rnd() % 120);
                inExc.EnableCount = (int)(rnd() % 220);
                inErr.ErrorCode.all = (unsigned long)rnd();
                run_case(1);
                break;

            case 2:
                inE1.TZFLG.bit.CBC = (Uint16)(rnd() & 1);
                inE2.TZFLG.bit.CBC = (Uint16)(rnd() & 1);
                inE3.TZFLG.bit.CBC = (Uint16)(rnd() & 1);
                run_case(2);
                break;

            case 3:
                inIuvw.U = (long)rnd() * 0x00010001L;
                inIuvw.V = (long)rnd() * 0x00010001L;
                inIuvw.W = (long)rnd() * 0x00010001L;
                run_case(3);
                break;

            case 4:
                inGpio.AIODAT.bit.AIO2 = (Uint)(rnd() & 1);
                inErr.ErrorCode.all = (unsigned long)rnd();
                inSt.RunStep = (Uint)(rnd() % 8);
                inIuvw.U = (long)rnd() * 0x00010001L;
                inIuvw.V = (long)rnd() * 0x00010001L;
                inIuvw.W = (long)rnd() * 0x00010001L;
                run_case(4);
                break;

            case 5:
                inSt.RunStep = (Uint)(rnd() % 8);
                inErr.ErrorCode.all = (unsigned long)rnd();
                run_case(5);
                break;

            case 6:
                inCmd.Command.bit.StartDC = (Uint)(rnd() & 1);
                inCmd.Command.bit.StopDC = (Uint)(rnd() & 1);
                if (!inCmd.Command.bit.StartDC && !inCmd.Command.bit.StopDC)
                {
                    inCmd.Command.bit.StartDC = 1;
                }
                inCompar.StartDCBrakeCur = (Uint)rnd();
                inCompar.StopDCBrakeCur = (Uint)rnd();
                inDcb.Time = (int)(rnd() % 20) - 3;
                inInv.InvTypeApply = (Uint)(rnd() % 200);
                inLineCur.CurPer = (Uint)rnd();
                inMotorExtPer.R1 = (int)rnd();
                run_case(6);
                break;

            default:
                inCompar.BrakeCoff = (Uint)(rnd() % 130);
                inVfPar.ovPoint = (Uint)rnd();
                inVfPar.ovPointCoff = (Uint)rnd();
                inUdc.uDC = (Uint)rnd();
                inBrk.Flag = (Uint)rnd();
                inBrk.Cnt = (Uint)rnd();
                inBrk.OnCnt = (Uint)rnd();
                inBrk.OffCnt = (Uint)rnd();
                run_case(7);
                break;
        }
    }
}

/* ---- main ---------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 20000;
    long base;

    init_ptrs();

    base = g_cases;
    phase_short_gnd();
    printf("  RunCaseShortGnd              : %ld\n", g_cases - base);

    base = g_cases;
    phase_low_power();
    printf("  RunCaseLowPower              : %ld\n", g_cases - base);

    base = g_cases;
    phase_cbc();
    printf("  CBCLimitCurPrepare           : %ld\n", g_cases - base);

    base = g_cases;
    phase_max_uvw();
    printf("  MaxUVWCurrent                : %ld\n", g_cases - base);

    base = g_cases;
    phase_hw_error();
    printf("  HardWareErrorDeal/OverUDC    : %ld\n", g_cases - base);

    base = g_cases;
    phase_dc_brake();
    printf("  RunCaseDcBrake               : %ld\n", g_cases - base);

    base = g_cases;
    phase_brake_res();
    printf("  BrakeResControl              : %ld\n", g_cases - base);

    base = g_cases;
    phase_random(n);
    printf("  random mixed                 : %ld\n", g_cases - base);

    printf("MotorPublicCal difftest: cases=%ld mismatches=%d\n",
           g_cases, g_fails);
    return g_fails ? 1 : 0;
}
