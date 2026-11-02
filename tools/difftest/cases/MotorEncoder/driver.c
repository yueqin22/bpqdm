/* Differential test driver for MotorEncoder.
 *
 * Sixteen entry points. They fall into four groups, and the groups need
 * different treatment:
 *
 *   (a) register setup      InitRtInterface, InitSetQEP, ReInitForPG.
 *       Their whole effect is on peripheral registers and on gPGData, and
 *       there is no return value. The QEP registers are per-side (the module
 *       reaches them through its own `EQepRegs` pointer), the GPIO and CPU
 *       timer registers are shared -- see "what is compared" below.
 *
 *   (b) direction judges    JudgeABDir, JudgeUVWDir, JudgeRTDir.
 *       Read two positions, update a counter, return DIR_*. Return value AND
 *       the counter are compared, and the sweep walks the +-1820 window and
 *       the +-2 / +-32767 clamps one step at a time.
 *
 *   (c) speed maths         GetUvwPhase, VCGetFeedBackSpeed, GetSpeedMMethod,
 *       GetSpeedMTMethod, SpeedSmoothDeal, GetMDetaPos, GetMTTimeNum,
 *       RotorTransSamplePos, RotorTransCalVel.
 *       These are where the 32- and 64-bit intermediate products live, so they
 *       are the reason the shim pins `int` to 16 bits.
 *
 *   (d) the bit-bang        GetRotorTransPos.
 *
 * ---- what is compared -----------------------------------------------------
 *
 *   per-side    gPGData / gUVWPG / gPGDir / gFVCSpeed / gRotorTrans /
 *               gSpeedFilter / gSpeedLine / gUVWAngleTable -- DEFINED by this
 *               module, so each side owns a renamed copy. Whole-struct memcmp.
 *               The two QEP blocks are per-side as well: the module's own
 *               `EQepRegs` pointer is pointed at a different block for each
 *               side, so one side's QCLR / QEPSTS writes cannot leak into the
 *               other side's input image.
 *   shared      AdcResult, GpioCtrlRegs, GpioDataRegs, SysCtrlRegs, the CPU
 *               timer, the CPU-control counters, the BurrFilter call log, and
 *               the thirteen globals owned by other modules. These are single
 *               objects that both sides write, so the input image is
 *               reinstalled before each side and snapshotted after each side.
 *   return      the three Judge*Dir values.
 *
 * Every input is filled with a deterministic PRNG stream before each case, so
 * "the rewrite stopped writing this field" is visible -- a field both sides
 * leave alone compares equal, and an all-zero image cannot tell "cleared it"
 * from "never touched it".
 *
 * ---- no bare `unsigned` in this file --------------------------------------
 *
 * The shim does `#define int short`, and that only rewrites the *token* `int`.
 * A bare `unsigned` has no `int` token in it, so it stays 32 bits on the host
 * while the target would make it 16. Every counter and temporary here is
 * written `unsigned long`, and every 16-bit quantity is `Uint` or `unsigned
 * int`. A silent width difference in the driver would show up as a false
 * mismatch, or worse, as a missing one.
 *
 * ---- host traps ----------------------------------------------------------
 *
 * GetSpeedMMethod divides by `PulseNum * FullFreq01`, GetSpeedMTMethod by
 * `16 * MTTime` and later by `m_Long>>1`, RotorTransCalVel by gPWMPrdApply,
 * IntNum<<2, FullFreq01 and FcSetApply. On the target these wrap or are
 * guaranteed non-zero by the callers; on the host a zero divisor raises
 * STATUS_INTEGER_DIVIDE_BY_ZERO and kills the process. So the bench
 * *classifies* those cases instead of excluding them, in call_side() /
 * note_hosttrap(): both sides trapping is counted and skipped, one side
 * trapping while the other does not is a MISMATCH, because on the host "does
 * it trap" is itself observable behaviour. Input coverage is not reduced --
 * the swept inputs are not filtered, and the number of skipped cases is
 * printed in the summary, so a rewrite that moved the trap cannot hide.
 *
 * ---- what is NOT covered -------------------------------------------------
 *
 *   1. The serial input pin -- CLOSED by the host patch, see mock_encoder.c
 *      group 3b. GetRotorTransPos shifts in twelve bits and on the target
 *      every one of them comes from the same object:
 *          #define ROTOR_TRANS_SO   GpioDataRegs.GPADAT.bit.GPIO21
 *      a fixed memory bit. From outside the call mData could therefore only
 *      ever be 0x0000 or 0xFFF0, the twelve shift amounts were mutually
 *      indistinguishable, and `<< 13` -> `<< 12` produced byte-identical
 *      results. The host build now redirects the macro to a driver-fed 12-bit
 *      sequence (host_patch.json module 1 rule 2, storage in
 *      mock_encoder.c), so the twelve positions are separable and the
 *      negative-control list may claim them. Two honest limits remain:
 *        * A mutation that merely *permutes* two shift amounts is still
 *          invisible, and that is not a hole: `mData = mData | mBit` is a
 *          commutative OR, so every permutation of the twelve shifts is
 *          genuinely equivalent. Such a mutation has to be proved equivalent
 *          and deleted from negative_control.py, never "covered".
 *        * Only the F_RTPOS phases load a pattern. The array is
 *          zero-initialised and nothing else writes it, so an entry point
 *          that started reading the pin would see twelve zeros on both sides
 *          -- a valid but degenerate comparison, not a silent skip.
 *
 *   2. gCpuTime.tmpTime. GetTime() reads `CpuTimer1.RegsAddr->TIM.all`, a
 *      plain memory cell in the mock, so the value read at the top of
 *      GetRotorTransPos and the value read at the bottom are the same and
 *      `tmpBase - GetTime()` is always 0. The *assignment* is still tested --
 *      the input image seeds gCpuTime.tmpTime with a non-zero value, so a
 *      rewrite that drops the line keeps the seed and differs. The arithmetic
 *      is not.
 *
 *   3. Timer direction. On the chip TIM is a DOWN counter and the project
 *      always writes the difference as "earlier read minus later read"
 *      (MotorEncoder.c:692, f_main.c:314/592). Here there is only one read of
 *      the register per call, so the direction is not observable.
 *      RotorTransSamplePos wraps its difference in labs(), so it is
 *      direction-insensitive anyway.
 *
 *   4. `long` on the host is 32 bits, matching C28x (measured -- see the
 *      comment in shim/DSP2803x_Device.h). `int` is pinned to 16 bits. A
 *      pointer is 8 bytes on the host and 4 on the target; no pointer
 *      arithmetic is compared.
 *
 *   5. The peripheral REGISTER STRUCT layouts are the host's. They mix 16- and
 *      32-bit members, so MSVC inserts alignment padding the C28x
 *      word-addressed layout does not have. Ref and new share the host layout,
 *      which is what the comparison needs; the target offsets are a separate
 *      question (target_layout.py).
 */

#include <stdio.h>
#include <string.h>
#include <excpt.h>      /* __try/__except + GetExceptionCode() */

/* **NOT** `<stdlib.h>`. It declares `int abs(int)` (32-bit), and this project's
 * own `SubPrgInclude.h` declares `extern int abs(int)` -- which the shim's
 * `#define int short` turns into `short abs(short)`. Two declarations of the
 * same name with different base types is C2371, and it is a host-only
 * artefact: on C28x `int` IS 16 bits, so the project's prototype is exactly
 * what the compiler would use. The mock has the same constraint and does not
 * include it either. `atol` is declared by hand below instead. */
extern long atol(const char *);

#include "MotorEncoder.h"
#include "MotorPmsmMain.h"
#include "MotorPmsmParEst.h"
#include "MotorVCInclude.h"
#include "SubPrgInclude.h"

#define FAIL_LIMIT 20

/* ---- ref side ------------------------------------------------------------ */

extern void  ref_InitRtInterface(void);
extern void  ref_InitSetQEP(void);
extern void  ref_ReInitForPG(void);
extern Uint  ref_JudgeABDir(void);
extern Uint  ref_JudgeUVWDir(void);
extern Uint  ref_JudgeRTDir(void);
extern void  ref_GetUvwPhase(void);
extern void  ref_VCGetFeedBackSpeed(void);
extern void  ref_GetSpeedMMethod(void);
extern void  ref_GetSpeedMTMethod(void);
extern void  ref_SpeedSmoothDeal(ROTOR_SPEED_SMOOTH *, int);
extern void  ref_GetMDetaPos(void);
extern void  ref_GetMTTimeNum(void);
extern void  ref_GetRotorTransPos(void);
extern void  ref_RotorTransSamplePos(void);
extern void  ref_RotorTransCalVel(void);

extern struct EQEP_REGS      *ref_EQepRegs;
extern PG_DATA_STRUCT         ref_gPGData;
extern IPM_UVW_PG_STRUCT      ref_gUVWPG;
extern IPM_PG_DIR_STRUCT      ref_gPGDir;
extern FVC_SPEED_STRUCT       ref_gFVCSpeed;
extern ROTOR_TRANS_STRUCT     ref_gRotorTrans;
extern BURR_FILTER_STRUCT     ref_gSpeedFilter;
extern CUR_LINE_STRUCT_DEF    ref_gSpeedLine;
extern Uint const             ref_gUVWAngleTable[6];

/* ---- new side ------------------------------------------------------------ */

extern void  new_InitRtInterface(void);
extern void  new_InitSetQEP(void);
extern void  new_ReInitForPG(void);
extern Uint  new_JudgeABDir(void);
extern Uint  new_JudgeUVWDir(void);
extern Uint  new_JudgeRTDir(void);
extern void  new_GetUvwPhase(void);
extern void  new_VCGetFeedBackSpeed(void);
extern void  new_GetSpeedMMethod(void);
extern void  new_GetSpeedMTMethod(void);
extern void  new_SpeedSmoothDeal(ROTOR_SPEED_SMOOTH *, int);
extern void  new_GetMDetaPos(void);
extern void  new_GetMTTimeNum(void);
extern void  new_GetRotorTransPos(void);
extern void  new_RotorTransSamplePos(void);
extern void  new_RotorTransCalVel(void);

extern struct EQEP_REGS      *new_EQepRegs;
extern PG_DATA_STRUCT         new_gPGData;
extern IPM_UVW_PG_STRUCT      new_gUVWPG;
extern IPM_PG_DIR_STRUCT      new_gPGDir;
extern FVC_SPEED_STRUCT       new_gFVCSpeed;
extern ROTOR_TRANS_STRUCT     new_gRotorTrans;
extern BURR_FILTER_STRUCT     new_gSpeedFilter;
extern CUR_LINE_STRUCT_DEF    new_gSpeedLine;
extern Uint const             new_gUVWAngleTable[6];

/* ---- instrumentation provided by the mock -------------------------------- */

extern struct EQEP_REGS      gHostEqepRef;
extern struct EQEP_REGS      gHostEqepNew;
extern volatile Uint16       gHostCpuCtl[7];
extern volatile struct CPUTIMER_REGS gHostCpuTimer1Regs;
extern struct CPUTIMER_VARS  CpuTimer1;
extern Uint16                gBurrCount;
extern Uint32                gHostSoBits[];
extern Uint16                gHostSoIdx;
extern BURR_FILTER_STRUCT    gBurrLog;

/* ---- per-side snapshot --------------------------------------------------- */

struct SIDE
{
    PG_DATA_STRUCT      pgData;
    IPM_UVW_PG_STRUCT   uvwpg;
    IPM_PG_DIR_STRUCT   pgdir;
    FVC_SPEED_STRUCT    fvc;
    ROTOR_TRANS_STRUCT  rotor;
    BURR_FILTER_STRUCT  speedFilter;
    CUR_LINE_STRUCT_DEF speedLine;
    Uint                angleTable[6];
    struct EQEP_REGS    qep;
};

static struct SIDE S[2];

/* ---- input image --------------------------------------------------------- */

struct SIDE_IN
{
    PG_DATA_STRUCT      pgData;
    IPM_UVW_PG_STRUCT   uvwpg;
    IPM_PG_DIR_STRUCT   pgdir;
    FVC_SPEED_STRUCT    fvc;
    ROTOR_TRANS_STRUCT  rotor;
    BURR_FILTER_STRUCT  speedFilter;
    CUR_LINE_STRUCT_DEF speedLine;
    struct EQEP_REGS    qep;
    int                 smoothSpeed;
};

static struct SIDE_IN inSide;

static RUN_STATUS_STRUCT                  inSt;
static MOTOR_STRUCT                       inMotor;
static MOTOR_EXTERN_STRUCT                inMotorExt;
static BASE_COMMAND_STRUCT                inCmd;
static MAIN_COMMAND_EXTEND_UNION          inExt;
static BASE_PAR_STRUCT                    inBasePar;
static CPU_TIME_STRUCT                    inCpuTime;
static struct FAULT_CODE_INFOR_STRUCT_DEF inErr;
static IPM_POSITION_STRUCT                inIpmPos;
static IPM_ZERO_POS_STRUCT                inIpmZero;
static PWM_OUT_STRUCT                     inPwm;
static PMSM_EST_PARAM_DATA                inPmParEst;
static VC_INFO_STRUCT                     inVcPar;

static struct ADC_RESULT_REGS inAdc;
static struct GPIO_CTRL_REGS  inGpioCtrl;
static struct GPIO_DATA_REGS  inGpio;
static struct SYS_CTRL_REGS   inSysCtrl;
static struct CPUTIMER_REGS   inTimer;

/* ---- deterministic input fill -------------------------------------------- */

static unsigned long g_rng = 0x20261013UL;

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
    rnd_fill(&inSide,      sizeof inSide);
    rnd_fill(&inSt,        sizeof inSt);
    rnd_fill(&inMotor,     sizeof inMotor);
    rnd_fill(&inMotorExt,  sizeof inMotorExt);
    rnd_fill(&inCmd,       sizeof inCmd);
    rnd_fill(&inExt,       sizeof inExt);
    rnd_fill(&inBasePar,   sizeof inBasePar);
    rnd_fill(&inCpuTime,   sizeof inCpuTime);
    rnd_fill(&inErr,       sizeof inErr);
    rnd_fill(&inIpmPos,    sizeof inIpmPos);
    rnd_fill(&inIpmZero,   sizeof inIpmZero);
    rnd_fill(&inPwm,       sizeof inPwm);
    rnd_fill(&inPmParEst,  sizeof inPmParEst);
    rnd_fill(&inVcPar,     sizeof inVcPar);

    rnd_fill(&inAdc,       sizeof inAdc);
    rnd_fill(&inGpioCtrl,  sizeof inGpioCtrl);
    rnd_fill(&inGpio,      sizeof inGpio);
    rnd_fill(&inSysCtrl,   sizeof inSysCtrl);
    rnd_fill(&inTimer,     sizeof inTimer);

    /* 几个"位域当门"的位在伪随机字节下 0/1 各半, 但高位字节可能落到别的字段上。
     * 这里显式再钉一遍, 保证随机相位里每个门都真被打开过。 */
    inSide.qep.QFLG.bit.UTO      = (Uint16)(rnd() & 1);
    inSide.qep.QFLG.bit.WTO      = (Uint16)(rnd() & 1);
    inSide.qep.QEPSTS.bit.UPEVNT = (Uint16)(rnd() & 1);
    inSide.qep.QEPSTS.bit.COEF   = (Uint16)(rnd() & 1);
    inCmd.Command.bit.Start       = (Uint)(rnd() & 1);
    inCmd.Command.bit.ControlMode = (Uint)(rnd() & 3);
    inSt.RunStep = (Uint)(rnd() % 8);
}

/* ---- serial input pattern ------------------------------------------------ */

/* The host build turns ROTOR_TRANS_SO into gHostSoBits[gHostSoIdx++], so a
 * 12-bit resolver frame can be fed to GetRotorTransPos one bit per read. The
 * pattern is written MSB first: bit11 of `pat` is the bit the module reads
 * with `<< 15`, bit0 is the one read with `<< 4`.
 *
 * Only 0/1 is ever stored, because the pin is a one-bit field on the chip;
 * anything else would exercise a state the target cannot reach. Entries
 * 12.. are left at their static zero initialiser, so a rewrite that reads the
 * pin more than twelve times cannot run off the end.
 *
 * set_so_pattern() only fills the array. Resetting gHostSoIdx is
 * install_side()'s job, so that both sides always start at the first bit --
 * setting it here as well would let a phase that forgot to call this function
 * leave a half-consumed sequence behind. */
static Uint g_soPat;

static void set_so_pattern(Uint pat)
{
    int i;

    g_soPat = (Uint)(pat & 0x0FFF);
    for (i = 0; i < 12; i++)
    {
        gHostSoBits[i] = (Uint32)((g_soPat >> (11 - i)) & 1);
    }
}

/* ---- install / snapshot -------------------------------------------------- */

static void install_side(int side)
{
    gHostSoIdx = 0;

    if (side == 0)
    {
        ref_EQepRegs = &gHostEqepRef;
        memcpy((void *)&gHostEqepRef, &inSide.qep, sizeof inSide.qep);
        ref_gPGData      = inSide.pgData;
        ref_gUVWPG       = inSide.uvwpg;
        ref_gPGDir       = inSide.pgdir;
        ref_gFVCSpeed    = inSide.fvc;
        ref_gRotorTrans  = inSide.rotor;
        ref_gSpeedFilter = inSide.speedFilter;
        ref_gSpeedLine   = inSide.speedLine;
    }
    else
    {
        new_EQepRegs = &gHostEqepNew;
        memcpy((void *)&gHostEqepNew, &inSide.qep, sizeof inSide.qep);
        new_gPGData      = inSide.pgData;
        new_gUVWPG       = inSide.uvwpg;
        new_gPGDir       = inSide.pgdir;
        new_gFVCSpeed    = inSide.fvc;
        new_gRotorTrans  = inSide.rotor;
        new_gSpeedFilter = inSide.speedFilter;
        new_gSpeedLine   = inSide.speedLine;
    }
}

static void snapshot(int side)
{
    struct SIDE *s = &S[side];

    if (side == 0)
    {
        s->pgData      = ref_gPGData;
        s->uvwpg       = ref_gUVWPG;
        s->pgdir       = ref_gPGDir;
        s->fvc         = ref_gFVCSpeed;
        s->rotor       = ref_gRotorTrans;
        s->speedFilter = ref_gSpeedFilter;
        s->speedLine   = ref_gSpeedLine;
        memcpy(s->angleTable, (const void *)ref_gUVWAngleTable,
               sizeof s->angleTable);
        memcpy(&s->qep, (const void *)&gHostEqepRef, sizeof s->qep);
    }
    else
    {
        s->pgData      = new_gPGData;
        s->uvwpg       = new_gUVWPG;
        s->pgdir       = new_gPGDir;
        s->fvc         = new_gFVCSpeed;
        s->rotor       = new_gRotorTrans;
        s->speedFilter = new_gSpeedFilter;
        s->speedLine   = new_gSpeedLine;
        memcpy(s->angleTable, (const void *)new_gUVWAngleTable,
               sizeof s->angleTable);
        memcpy(&s->qep, (const void *)&gHostEqepNew, sizeof s->qep);
    }
}

/* ---- shared snapshot ----------------------------------------------------- */

struct SHARED
{
    struct ADC_RESULT_REGS            adc;
    struct GPIO_CTRL_REGS             gpioCtrl;
    struct GPIO_DATA_REGS             gpio;
    struct SYS_CTRL_REGS              sysCtrl;
    struct CPUTIMER_REGS              timer;
    Uint16                            cpu[7];
    Uint16                            burrCount;
    BURR_FILTER_STRUCT                burrLog;
    RUN_STATUS_STRUCT                 st;
    MOTOR_STRUCT                      motor;
    MOTOR_EXTERN_STRUCT               motorExt;
    BASE_COMMAND_STRUCT               cmd;
    MAIN_COMMAND_EXTEND_UNION         ext;
    BASE_PAR_STRUCT                   basePar;
    CPU_TIME_STRUCT                   cpuTime;
    struct FAULT_CODE_INFOR_STRUCT_DEF err;
    IPM_POSITION_STRUCT               ipmPos;
    IPM_ZERO_POS_STRUCT               ipmZero;
    PWM_OUT_STRUCT                    pwm;
    PMSM_EST_PARAM_DATA               pmParEst;
    VC_INFO_STRUCT                    vcPar;
};

static struct SHARED SH[2];
static Uint g_ret[2];

static void install_shared(void)
{
    unsigned long i;

    memcpy((void *)&AdcResult,     &inAdc,      sizeof inAdc);
    memcpy((void *)&GpioCtrlRegs,  &inGpioCtrl, sizeof inGpioCtrl);
    memcpy((void *)&GpioDataRegs,  &inGpio,     sizeof inGpio);
    memcpy((void *)&SysCtrlRegs,   &inSysCtrl,  sizeof inSysCtrl);
    memcpy((void *)&gHostCpuTimer1Regs, &inTimer, sizeof inTimer);

    CpuTimer1.RegsAddr = &gHostCpuTimer1Regs;

    for (i = 0; i < 7; i++)
    {
        gHostCpuCtl[i] = 0;
    }

    gBurrCount = 0;
    memset(&gBurrLog, 0, sizeof gBurrLog);

    gMainStatus   = inSt;
    gMotorInfo    = inMotor;
    gMotorExtInfo = inMotorExt;
    gMainCmd      = inCmd;
    gExtendCmd    = inExt;
    gBasePar      = inBasePar;
    gCpuTime      = inCpuTime;
    gError        = inErr;
    gIPMPos       = inIpmPos;
    gIPMZero      = inIpmZero;
    gPWM          = inPwm;
    gPmParEst     = inPmParEst;
    gVCPar        = inVcPar;
}

static void snap_shared(int side)
{
    struct SHARED *s = &SH[side];
    unsigned long i;

    memcpy(&s->adc,      (const void *)&AdcResult,     sizeof s->adc);
    memcpy(&s->gpioCtrl, (const void *)&GpioCtrlRegs,  sizeof s->gpioCtrl);
    memcpy(&s->gpio,     (const void *)&GpioDataRegs,  sizeof s->gpio);
    memcpy(&s->sysCtrl,  (const void *)&SysCtrlRegs,   sizeof s->sysCtrl);
    memcpy(&s->timer,    (const void *)&gHostCpuTimer1Regs, sizeof s->timer);

    for (i = 0; i < 7; i++)
    {
        s->cpu[i] = gHostCpuCtl[i];
    }

    s->burrCount = gBurrCount;
    s->burrLog   = gBurrLog;

    s->st       = gMainStatus;
    s->motor    = gMotorInfo;
    s->motorExt = gMotorExtInfo;
    s->cmd      = gMainCmd;
    s->ext      = gExtendCmd;
    s->basePar  = gBasePar;
    s->cpuTime  = gCpuTime;
    s->err      = gError;
    s->ipmPos   = gIPMPos;
    s->ipmZero  = gIPMZero;
    s->pwm      = gPWM;
    s->pmParEst = gPmParEst;
    s->vcPar    = gVCPar;
}

/* ---- case runner --------------------------------------------------------- */

enum {
    F_INITRT = 0, F_INITQEP, F_REINITPG,
    F_ABDIR, F_UVWDIR, F_RTDIR,
    F_UVWPHASE, F_VCFB, F_SPEEDM, F_SPEEDMT, F_SMOOTH,
    F_MDETA, F_MTTIME, F_RTPOS, F_RTSAMPLE, F_RTCALVEL
};
#define FN_COUNT 16

static const char *g_fn_name[FN_COUNT] = {
    "InitRtInterface", "InitSetQEP", "ReInitForPG",
    "JudgeABDir", "JudgeUVWDir", "JudgeRTDir",
    "GetUvwPhase", "VCGetFeedBackSpeed", "GetSpeedMMethod",
    "GetSpeedMTMethod", "SpeedSmoothDeal",
    "GetMDetaPos", "GetMTTimeNum", "GetRotorTransPos",
    "RotorTransSamplePos", "RotorTransCalVel"
};

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
    printf("  PGType=%u PGMode=%u RunStep=%u MotorType=%u ControlMode=%u\n",
           (unsigned long)inSide.pgData.PGType, (unsigned long)inSide.pgData.PGMode,
           (unsigned long)inSt.RunStep, (unsigned long)inMotor.MotorType,
           (unsigned long)inCmd.Command.bit.ControlMode);
    printf("  ABDirCnt=%d UVWDirCnt=%d RtDirCnt=%d FeedPos=%u\n",
           (int)inSide.pgdir.ABDirCnt, (int)inSide.pgdir.UVWDirCnt,
           (int)inSide.pgdir.RtDirCnt, (unsigned long)inIpmZero.FeedPos);
    printf("  UVWAngle=%u RtPos=%u MDetaPos=%d MTPulseNum=%u MTTime=%lu\n",
           (unsigned long)inSide.uvwpg.UVWAngle,
           (unsigned long)inSide.rotor.RTPos, (int)inSide.fvc.MDetaPos,
           (unsigned long)inSide.fvc.MTPulseNum,
           (unsigned long)inSide.fvc.MTTime);
    printf("  SOpat=%03X SpeedDir=%u PGDir=%u Poles=%u FullFreq01=%lu "
           "PulseNum=%u\n",
           (unsigned long)g_soPat,
           (unsigned long)inSide.pgData.SpeedDir,
           (unsigned long)inSide.pgData.PGDir, (unsigned long)inMotorExt.Poles,
           (unsigned long)inBasePar.FullFreq01,
           (unsigned long)inSide.pgData.PulseNum);
    printf("  QPOSLAT=%lu MLastPos=%ld QCPRD=%u MTLimitTime=%u\n",
           (unsigned long)inSide.qep.QPOSLAT, (long)inSide.fvc.MLastPos,
           (unsigned long)inSide.qep.QCPRD,
           (unsigned long)inSide.fvc.MTLimitTime);
    printf("  adc 9/10/11=%u/%u/%u ret %u/%u\n",
           (unsigned long)inAdc.ADCRESULT9,
           (unsigned long)inAdc.ADCRESULT10,
           (unsigned long)inAdc.ADCRESULT11,
           (unsigned long)g_ret[0], (unsigned long)g_ret[1]);
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
    cmp("gPGData",      &S[0].pgData,      &S[1].pgData,      sizeof S[0].pgData);
    cmp("gUVWPG",       &S[0].uvwpg,       &S[1].uvwpg,       sizeof S[0].uvwpg);
    cmp("gPGDir",       &S[0].pgdir,       &S[1].pgdir,       sizeof S[0].pgdir);
    cmp("gFVCSpeed",    &S[0].fvc,         &S[1].fvc,         sizeof S[0].fvc);
    cmp("gRotorTrans",  &S[0].rotor,       &S[1].rotor,       sizeof S[0].rotor);
    cmp("gSpeedFilter", &S[0].speedFilter, &S[1].speedFilter,
        sizeof S[0].speedFilter);
    cmp("gSpeedLine",   &S[0].speedLine,   &S[1].speedLine,
        sizeof S[0].speedLine);
    cmp("gUVWAngleTable", S[0].angleTable, S[1].angleTable,
        sizeof S[0].angleTable);
    cmp("QepBlock",     &S[0].qep,         &S[1].qep,         sizeof S[0].qep);

    cmp("AdcResult",     &SH[0].adc,      &SH[1].adc,      sizeof SH[0].adc);
    cmp("GpioCtrlRegs",  &SH[0].gpioCtrl, &SH[1].gpioCtrl,
        sizeof SH[0].gpioCtrl);
    cmp("GpioDataRegs",  &SH[0].gpio,     &SH[1].gpio,     sizeof SH[0].gpio);
    cmp("SysCtrlRegs",   &SH[0].sysCtrl,  &SH[1].sysCtrl,  sizeof SH[0].sysCtrl);
    cmp("CpuTimer1Regs", &SH[0].timer,    &SH[1].timer,    sizeof SH[0].timer);
    cmp("cpuCtl",        SH[0].cpu,       SH[1].cpu,       sizeof SH[0].cpu);
    cmp("burrCount",     &SH[0].burrCount, &SH[1].burrCount,
        sizeof SH[0].burrCount);
    cmp("burrLog",       &SH[0].burrLog,  &SH[1].burrLog,  sizeof SH[0].burrLog);

    cmp("gMainStatus",   &SH[0].st,       &SH[1].st,       sizeof SH[0].st);
    cmp("gMotorInfo",    &SH[0].motor,    &SH[1].motor,    sizeof SH[0].motor);
    cmp("gMotorExtInfo", &SH[0].motorExt, &SH[1].motorExt,
        sizeof SH[0].motorExt);
    cmp("gMainCmd",      &SH[0].cmd,      &SH[1].cmd,      sizeof SH[0].cmd);
    cmp("gExtendCmd",    &SH[0].ext,      &SH[1].ext,      sizeof SH[0].ext);
    cmp("gBasePar",      &SH[0].basePar,  &SH[1].basePar,  sizeof SH[0].basePar);
    cmp("gCpuTime",      &SH[0].cpuTime,  &SH[1].cpuTime,  sizeof SH[0].cpuTime);
    cmp("gError",        &SH[0].err,      &SH[1].err,      sizeof SH[0].err);
    cmp("gIPMPos",       &SH[0].ipmPos,   &SH[1].ipmPos,   sizeof SH[0].ipmPos);
    cmp("gIPMZero",      &SH[0].ipmZero,  &SH[1].ipmZero,  sizeof SH[0].ipmZero);
    cmp("gPWM",          &SH[0].pwm,      &SH[1].pwm,      sizeof SH[0].pwm);
    cmp("gPmParEst",     &SH[0].pmParEst, &SH[1].pmParEst, sizeof SH[0].pmParEst);
    cmp("gVCPar",        &SH[0].vcPar,    &SH[1].vcPar,    sizeof SH[0].vcPar);

    if (g_ret[0] != g_ret[1])
    {
        report("return value");
    }
}

/* ---- host trap classification -------------------------------------------- */

#define HOST_EXC_INT_DIVIDE_BY_ZERO 0xC0000094UL

#define HOSTTRAP_PRINT  8

static int  g_trapped[2];
static long g_hosttraps;
static long g_skipped;
static int  g_fnSel;

/* One side's call, with the trap caught. Only integer-divide-by-zero is
 * caught; anything else is passed on with EXCEPTION_CONTINUE_SEARCH and still
 * kills the bench, because that would be a real defect in the module rather
 * than a host/target representability gap. */
static void call_side(int side)
{
    g_trapped[side] = 0;

    __try
    {
        switch (g_fnSel)
        {
        case F_INITRT:
            if (side == 0) ref_InitRtInterface();
            else           new_InitRtInterface();
            break;
        case F_INITQEP:
            if (side == 0) ref_InitSetQEP();
            else           new_InitSetQEP();
            break;
        case F_REINITPG:
            if (side == 0) ref_ReInitForPG();
            else           new_ReInitForPG();
            break;
        case F_ABDIR:
            if (side == 0) g_ret[0] = ref_JudgeABDir();
            else           g_ret[1] = new_JudgeABDir();
            break;
        case F_UVWDIR:
            if (side == 0) g_ret[0] = ref_JudgeUVWDir();
            else           g_ret[1] = new_JudgeUVWDir();
            break;
        case F_RTDIR:
            if (side == 0) g_ret[0] = ref_JudgeRTDir();
            else           g_ret[1] = new_JudgeRTDir();
            break;
        case F_UVWPHASE:
            if (side == 0) ref_GetUvwPhase();
            else           new_GetUvwPhase();
            break;
        case F_VCFB:
            if (side == 0) ref_VCGetFeedBackSpeed();
            else           new_VCGetFeedBackSpeed();
            break;
        case F_SPEEDM:
            if (side == 0) ref_GetSpeedMMethod();
            else           new_GetSpeedMMethod();
            break;
        case F_SPEEDMT:
            if (side == 0) ref_GetSpeedMTMethod();
            else           new_GetSpeedMTMethod();
            break;
        case F_SMOOTH:
            if (side == 0)
                ref_SpeedSmoothDeal(&ref_gFVCSpeed.MSpeedSmooth,
                                    inSide.smoothSpeed);
            else
                new_SpeedSmoothDeal(&new_gFVCSpeed.MSpeedSmooth,
                                    inSide.smoothSpeed);
            break;
        case F_MDETA:
            if (side == 0) ref_GetMDetaPos();
            else           new_GetMDetaPos();
            break;
        case F_MTTIME:
            if (side == 0) ref_GetMTTimeNum();
            else           new_GetMTTimeNum();
            break;
        case F_RTPOS:
            if (side == 0) ref_GetRotorTransPos();
            else           new_GetRotorTransPos();
            break;
        case F_RTSAMPLE:
            if (side == 0) ref_RotorTransSamplePos();
            else           new_RotorTransSamplePos();
            break;
        default:
            if (side == 0) ref_RotorTransCalVel();
            else           new_RotorTransCalVel();
            break;
        }
    }
    __except (GetExceptionCode() == HOST_EXC_INT_DIVIDE_BY_ZERO
                  ? EXCEPTION_EXECUTE_HANDLER
                  : EXCEPTION_CONTINUE_SEARCH)
    {
        g_trapped[side] = 1;
    }
}

static void note_hosttrap(void)
{
    g_hosttraps++;
    if (g_hosttraps <= HOSTTRAP_PRINT)
    {
        printf("HOSTTRAP %s (host divide-by-zero, comparison skipped): "
               "PGType=%u MTTime=%lu PulseNum=%u FullFreq01=%lu "
               "gPWMPrdApply=%u FcSetApply=%u FullFreq=%u IntNum=%u\n",
               g_fn_name[g_fnSel], (unsigned long)inSide.pgData.PGType,
               (unsigned long)inSide.fvc.MTTime,
               (unsigned long)inSide.pgData.PulseNum,
               (unsigned long)inBasePar.FullFreq01,
               (unsigned long)inPwm.gPWMPrdApply,
               (unsigned long)inBasePar.FcSetApply,
               (unsigned long)inBasePar.FullFreq,
               (unsigned long)inSide.rotor.IntNum);
    }
}

/* ---- the runner ---------------------------------------------------------- */

static void run_case(int which)
{
    g_fnSel = which;
    g_cur = g_fn_name[which];
    g_ret[0] = 0x5A5A;
    g_ret[1] = 0x5A5A;

    install_shared();
    install_side(0);
    call_side(0);
    snapshot(0);
    snap_shared(0);

    install_shared();
    install_side(1);
    call_side(1);
    snapshot(1);
    snap_shared(1);

    g_cases++;

    if (g_trapped[0] != g_trapped[1])
    {
        /* 只有一侧掉 -> 差异。宿主上"会不会掉"本身就是可观测行为。 */
        report("host trap on one side only");
        note_hosttrap();
        return;
    }

    if (g_trapped[0])
    {
        /* 两侧都掉 -> 这一格**仍然要比**。陷阱之前已经写下的全局量是确定的:
         * 每个 call_side 之前都重新 install 过输入, 每个之后都 snapshot 过。
         * 所以照常 compare(), 只是额外记一笔 skipped。这样做的理由是补一个洞:
         * 如果改写把**陷阱点的位置**挪了 (例如除数从 `A*B` 变成 `A*B*C`),
         * 两侧照样都掉, 只按"都掉了就跳过"处理就永远看不见 —— 而"陷阱前
         * 写到哪一步"恰恰会暴露它。 */
        g_skipped++;
        note_hosttrap();
    }

    compare();
}

/* ---- small helpers ------------------------------------------------------- */

/* (unsigned short) arithmetic on purpose: the module's own expressions mix
 * `Uint` and `int`, which under 16-bit `int` promote to `unsigned int`, so the
 * sweep has to build the operands the same way. */
static void set_pos16(int *bak, Uint *feed, int base, int delta)
{
    *bak  = base;
    *feed = (Uint)((unsigned int)((unsigned short)base) +
                   (unsigned int)((unsigned short)delta));
}

static Uint const near_ab[] = {
    0, 1, 2, 3, 1819, 1820, 1821, 1822, 32766, 32767, 32768, 65535
};
#define NEAR_AB_N ((unsigned long)(sizeof near_ab / sizeof near_ab[0]))

static int const cnt_sw[] = {
    -32768, -32767, -3, -2, -1, 0, 1, 2, 3, 32766, 32767
};
#define CNT_SW_N ((unsigned long)(sizeof cnt_sw / sizeof cnt_sw[0]))

/* ---- (a) register setup -------------------------------------------------- */

static void phase_init_rt(void)
{
    unsigned long k;

    for (k = 0; k < 16; k++)
    {
        case_begin();
        run_case(F_INITRT);
    }
}

static void phase_init_qep(void)
{
    unsigned long k;

    for (k = 0; k < 64; k++)
    {
        case_begin();
        inSide.qep.QEPCTL.all = (Uint16)rnd();
        inSide.qep.QDECCTL.all = (Uint16)rnd();
        inSide.qep.QCAPCTL.all = (Uint16)rnd();
        inSide.qep.QPOSCTL.all = (Uint16)rnd();
        inSide.qep.QCLR.all = (Uint16)rnd();
        inSide.qep.QPOSCNT = (int32)rnd();
        run_case(F_INITQEP);
    }
}

static void phase_reinit_pg(void)
{
    static Uint const pgtype[] = {
        0, 1, 2, 3, 4, 5, 6, 100, 101, 0xFFFF
    };
    static Uint const qidx[] = { 0, 1, 2, 3 };
    static Uint const pgmode[] = { 0, 1, 0xFFFF };
    unsigned long a, b, c;

    for (a = 0; a < sizeof pgtype / sizeof pgtype[0]; a++)
    {
        for (b = 0; b < sizeof qidx / sizeof qidx[0]; b++)
        {
            for (c = 0; c < sizeof pgmode / sizeof pgmode[0]; c++)
            {
                case_begin();
                inSide.pgData.PGType = pgtype[a];
                inExt.bit.QepIndex = (Uint)qidx[b];
                inSide.pgData.PGMode = pgmode[c];
                run_case(F_REINITPG);
            }
        }
    }
}

/* ---- (b) direction judges ------------------------------------------------ */

static void phase_judge_ab(void)
{
    static int const base[] = { 0, 1, 1000, 32767, 32768, 65535 };
    static int const delta[] = {
        0, 1, 1819, 1820, 1821, 1822, -1, -1819, -1820, -1821, -1822,
        32767, -32768
    };
    unsigned long a, b, c;

    for (a = 0; a < sizeof base / sizeof base[0]; a++)
    {
        for (b = 0; b < sizeof delta / sizeof delta[0]; b++)
        {
            for (c = 0; c < CNT_SW_N; c++)
            {
                case_begin();
                set_pos16(&inSide.pgdir.ABAngleBak, &inIpmZero.FeedPos,
                          base[a], delta[b]);
                inSide.pgdir.ABDirCnt = cnt_sw[c];
                run_case(F_ABDIR);
            }
        }
    }
}

static void phase_judge_uvw(void)
{
    static int const base[] = { 0, 1, 1000, 32767, 32768, 65535 };
    static int const delta[] = {
        0, 1, 1819, 1820, 1821, 1822, -1, -1819, -1820, -1821, -1822
    };
    static int const freq[] = { -1, 0, 1, -32768, 32767 };
    unsigned long a, b, c, d;

    for (a = 0; a < sizeof base / sizeof base[0]; a++)
    {
        for (b = 0; b < sizeof delta / sizeof delta[0]; b++)
        {
            for (c = 0; c < CNT_SW_N; c++)
            {
                for (d = 0; d < sizeof freq / sizeof freq[0]; d++)
                {
                    case_begin();
                    set_pos16(&inSide.pgdir.UVWAngleBak, &inSide.uvwpg.UVWAngle,
                              base[a], delta[b]);
                    inSide.pgdir.UVWDirCnt = cnt_sw[c];
                    inCmd.FreqSet = freq[d];
                    run_case(F_UVWDIR);
                }
            }
        }
    }
}

static void phase_judge_rt(void)
{
    static int const base[] = { 0, 1, 1000, 32767, 32768, 65535 };
    static int const delta[] = {
        0, 1, 1819, 1820, 1821, 1822, -1, -1819, -1820, -1821, -1822
    };
    unsigned long a, b, c;

    for (a = 0; a < sizeof base / sizeof base[0]; a++)
    {
        for (b = 0; b < sizeof delta / sizeof delta[0]; b++)
        {
            for (c = 0; c < CNT_SW_N; c++)
            {
                case_begin();
                set_pos16(&inSide.pgdir.RtPhaseBak, &inSide.rotor.RTPos,
                          base[a], delta[b]);
                inSide.pgdir.RtDirCnt = cnt_sw[c];
                run_case(F_RTDIR);
            }
        }
    }
}

/* ---- (c) speed maths ----------------------------------------------------- */

/* GetUvwPhase: three guards, then three independent threshold comparisons at
 * 0xA50 (in the <<4 domain), then the UvwDir / RunStep swap of V and W, then
 * UVWData -> angle or error. */
static void phase_uvw_phase(void)
{
    static Uint const mtype[] = { MOTOR_TYPE_PM, 0, 100 };
    static Uint const pgtype[] = { PG_TYPE_UVW, PG_TYPE_ABZ };
    static Uint const cmode[] = { IDC_FVC_CTL, 0, 2 };
    static Uint const adc[] = { 0x0A4F, 0x0A50, 0x0A51 };
    static Uint const uvwdir[] = { 0, 1 };
    static Uint const runstep[] = { STATUS_GET_PAR, 0, 1 };
    static Uint const pmdir[] = { 0, 1 };
    static Uint const start[] = { 0, 1 };
    unsigned long a, b, c, d, e, f, g, h, i;

    for (a = 0; a < sizeof mtype / sizeof mtype[0]; a++)
    {
        for (b = 0; b < sizeof pgtype / sizeof pgtype[0]; b++)
        {
            for (c = 0; c < sizeof cmode / sizeof cmode[0]; c++)
            {
                for (d = 0; d < sizeof adc / sizeof adc[0]; d++)
                {
                    for (e = 0; e < sizeof adc / sizeof adc[0]; e++)
                    {
                        for (f = 0; f < sizeof adc / sizeof adc[0]; f++)
                        {
                            for (g = 0; g < sizeof uvwdir / sizeof uvwdir[0];
                                 g++)
                            {
                                for (h = 0; h < sizeof runstep /
                                                sizeof runstep[0]; h++)
                                {
                                    for (i = 0; i < sizeof start /
                                                    sizeof start[0]; i++)
                                    {
                                        case_begin();
                                        inMotor.MotorType = mtype[a];
                                        inSide.pgData.PGType = pgtype[b];
                                        inCmd.Command.bit.ControlMode = cmode[c];
                                        inAdc.ADCRESULT9  = adc[d];
                                        inAdc.ADCRESULT10 = adc[e];
                                        inAdc.ADCRESULT11 = adc[f];
                                        inSide.uvwpg.UvwDir = uvwdir[g];
                                        inSt.RunStep = runstep[h];
                                        inPmParEst.UvwDir = pmdir[(d + e) & 1];
                                        inCmd.Command.bit.Start = start[i];
                                        run_case(F_UVWPHASE);
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

/* VCGetFeedBackSpeed. The interesting part is the tail -- every branch funnels
 * into
 *     tempLong = (long)SpeedTemp * TransRatio;
 *     SpeedEncoder = (llong)tempLong * 16777 >> 24;
 * -- so the sweep drives SpeedTemp directly through the branches that do NOT
 * overwrite it (PG_TYPE_SC and the default arm) and through the resolver arm,
 * which sets it from gRotorTrans.FreqFeed. */
static void phase_vc_fb(void)
{
    static Uint const pgtype[] = {
        PG_TYPE_ABZ, PG_TYPE_UVW, PG_TYPE_RESOLVER, PG_TYPE_SC,
        PG_TYPE_SPECIAL_UVW, 5, PG_TYPE_NULL, 0xFFFF
    };
    static int const spd[] = {
        0, 1, -1, 100, -100, 32767, -32768, 20000, -20000, 12345
    };
    static Uint const ratio[] = { 0, 1, 2, 100, 0x8000, 0xFFFF };
    unsigned long a, b, c, k;

    for (a = 0; a < sizeof pgtype / sizeof pgtype[0]; a++)
    {
        for (b = 0; b < sizeof spd / sizeof spd[0]; b++)
        {
            for (c = 0; c < sizeof ratio / sizeof ratio[0]; c++)
            {
                for (k = 0; k < 2; k++)
                {
                    case_begin();
                    inSide.pgData.PGType = pgtype[a];
                    inSide.fvc.SpeedTemp = (int)spd[b];
                    inSide.rotor.FreqFeed  = (int)spd[b];
                    inSide.fvc.TransRatio  = ratio[c];
                    inSide.qep.QFLG.bit.WTO = (Uint16)k;

                    /* 走 ABZ 支路时要让两个子函数能算完: 除数非零。 */
                    inSide.pgData.PulseNum   = 1024;
                    inBasePar.FullFreq01     = 10000;
                    inSide.fvc.MTPulseNum    = 4;
                    inSide.fvc.MTTime        = 1000;
                    inSide.fvc.MTLimitTime   = 0;
                    inSide.fvc.Flag          = 0;
                    inSide.qep.QEPSTS.bit.COEF   = 0;
                    inSide.qep.QEPSTS.bit.UPEVNT = 1;
                    inSide.qep.QCPRD = 500;
                    inVcPar.VCSpeedFilter = 1;
                    run_case(F_VCFB);
                }
            }
        }
    }
}

static void phase_speed_m(void)
{
    static int const deta[] = { 0, 1, -1, 100, -100, 32767, -32768 };
    static int const detabak[] = { -32768, -1, 0, 1, 32767 };
    static Uint const poles[] = { 0, 1, 2, 4, 100, 0xFFFF };
    static Uint const pulse[] = { 0, 1, 2, 1024, 0xFFFF };
    static Ulong const ff01[] = { 0, 1, 2, 1000, 10000, 0xFFFFFFFFUL };
    static int const mspeed[] = {
        0, 1, -1, 8, -8, 100, 32766, 32767, -32768
    };
    static int const lasterr[] = { 0, 1, 999, 1000, 1001, 16383, 16384, 32767 };
    unsigned long a, b, c, d, e, f, g, h;

    for (a = 0; a < sizeof deta / sizeof deta[0]; a++)
    {
        for (b = 0; b < sizeof detabak / sizeof detabak[0]; b++)
        {
            for (c = 0; c < sizeof poles / sizeof poles[0]; c++)
            {
                for (d = 0; d < sizeof pulse / sizeof pulse[0]; d++)
                {
                    for (e = 0; e < sizeof ff01 / sizeof ff01[0]; e++)
                    {
                        case_begin();
                        inSide.fvc.MDetaPos = (int)deta[a];
                        inSide.fvc.MDetaPosBak = (int)detabak[b];
                        inMotorExt.Poles = poles[c];
                        inSide.pgData.PulseNum = pulse[d];
                        inBasePar.FullFreq01 = ff01[e];
                        run_case(F_SPEEDM);
                    }
                }
            }
        }
    }

    /* 第二步: 除数固定为非零, 专门扫滤波与 ++/--/<8 收口那几行。 */
    for (f = 0; f < sizeof mspeed / sizeof mspeed[0]; f++)
    {
        for (g = 0; g < sizeof lasterr / sizeof lasterr[0]; g++)
        {
            for (h = 0; h < 4; h++)
            {
                case_begin();
                inSide.pgData.PulseNum = 1024;
                inBasePar.FullFreq01 = 10000;
                inMotorExt.Poles = 4;
                inSide.fvc.MDetaPos = 1000;
                inSide.fvc.MDetaPosBak = (h & 1) ? -1 : 1;
                inSide.fvc.MSpeed = (int)mspeed[f];
                inSide.fvc.MSpeedSmooth.LastSpeed = (int)mspeed[(f + h) % 9];
                inSide.fvc.MSpeedSmooth.SpeedMaxErr = (int)lasterr[g];
                run_case(F_SPEEDM);
            }
        }
    }
}

static void phase_speed_mt(void)
{
    static Uint const pulse[] = { 0, 4, 8, 0xFFFF };
    static Ulong const mtime[] = { 0, 1, 2, 1000, 65535, 65536 };
    static int const flag[] = { 0, 1, 2 };
    static int const detabak[] = { -32768, -1, 0, 1, 32767 };
    static Uint const vcf[] = { 0, 1, 2, 10, 100 };
    static int const mtspeed[] = {
        0, 1, -1, 8, -8, 100, 32766, 32767, -32768
    };
    static Uint const limit[] = { 0, 1, 499, 500, 501, 0xFFFF };
    static Uint const qcprd[] = { 0, 1, 499, 500, 501, 0xFFFF };
    unsigned long a, b, c, d, e, k;

    for (a = 0; a < sizeof pulse / sizeof pulse[0]; a++)
    {
        for (b = 0; b < sizeof mtime / sizeof mtime[0]; b++)
        {
            for (c = 0; c < sizeof flag / sizeof flag[0]; c++)
            {
                for (d = 0; d < sizeof detabak / sizeof detabak[0]; d++)
                {
                    for (e = 0; e < sizeof vcf / sizeof vcf[0]; e++)
                    {
                        case_begin();
                        inSide.fvc.MTPulseNum = pulse[a];
                        inSide.fvc.MTTime = mtime[b];
                        inSide.fvc.Flag = (int)flag[c];
                        inSide.fvc.MDetaPosBak = (int)detabak[d];
                        inVcPar.VCSpeedFilter = vcf[e];
                        inSide.qep.QEPSTS.bit.COEF = (Uint16)(b & 1);
                        inSide.pgData.PulseNum = 1024;
                        inBasePar.FullFreq01 = 10000;
                        inMotorExt.Poles = 4;
                        run_case(F_SPEEDMT);
                    }
                }
            }
        }
    }

    /* 第二步: 扫 QCPRD / MTLimitTime / UPEVNT 与 ++/--/<8 收口。 */
    for (k = 0; k < 6; k++)
    {
        for (d = 0; d < sizeof vcf / sizeof vcf[0]; d++)
        {
            for (e = 0; e < sizeof mtspeed / sizeof mtspeed[0]; e++)
            {
                unsigned long m;

                for (m = 0; m < 2; m++)
                {
                    case_begin();
                    inSide.fvc.MTPulseNum = 4;
                    inSide.fvc.MTTime = 1000;
                    inSide.fvc.Flag = 0;
                    inSide.fvc.MTLimitTime = limit[k];
                    inSide.qep.QCPRD = qcprd[k];
                    inSide.qep.QEPSTS.bit.UPEVNT = (Uint16)m;
                    inSide.qep.QEPSTS.bit.COEF = 0;
                    inVcPar.VCSpeedFilter = vcf[d];
                    inSide.fvc.MTSpeed = (int)mtspeed[e];
                    inSide.fvc.MDetaPosBak = (m & 1) ? -1 : 1;
                    inSide.pgData.PulseNum = 1024;
                    inBasePar.FullFreq01 = 10000;
                    inMotorExt.Poles = 4;
                    run_case(F_SPEEDMT);
                }
            }
        }
    }
}

static void phase_smooth(void)
{
    static int const last[] = {
        0, 1, -1, 1000, 32766, 32767, -32768
    };
    static int const maxerr[] = {
        0, 1, 2, 999, 1000, 1001, 16382, 16383, 16384, 32767,
        /* 1998 / 1999 是 `1000 > SpeedMaxErr` 与 `999 > SpeedMaxErr` 的**唯一**
         * 分歧点: 只有 `>>1` 之后正好落到 999 时两条判据才不同 (1999>>1=999,
         * 1998>>1=999)。原来的列表里没有它们, 所以 ME69 一直是 MISSED。 */
        1998, 1999
    };
    static int const ins[] = { 0, 1, -1, 1000, 32767, -32768 };
    unsigned long a, b, c;

    for (a = 0; a < sizeof last / sizeof last[0]; a++)
    {
        for (b = 0; b < sizeof maxerr / sizeof maxerr[0]; b++)
        {
            for (c = 0; c < sizeof ins / sizeof ins[0]; c++)
            {
                case_begin();
                inSide.fvc.MSpeedSmooth.LastSpeed = (int)last[a];
                inSide.fvc.MSpeedSmooth.SpeedMaxErr = (int)maxerr[b];
                inSide.smoothSpeed = (int)ins[c];
                run_case(F_SMOOTH);
            }
        }
    }
}

/* ---- boundary states for four negative-control holes ---------------------
 *
 * 这四条变异在"随机 + 常规扫值"下抓不到, 不是因为它们等价, 而是因为被测
 * 代码里的比较恰好落在测试台从没构造出来的那个点上。逐条把那个点造出来。
 *
 * (a) ME54 (GetSpeedMMethod) 与 ME66 (GetSpeedMTMethod):
 *         if(abs(v - 目标) < 8)  v = 目标;
 *     `< 8` 与 `< 7` 只在距离**正好是 7** 时分歧。两个函数都先做
 *         v = Filter4(目标, v_prev);          // (3*v_prev + 目标) >> 2
 *     再把 v 朝目标挪一步。取 v_prev = 32757, 目标 = 32767 得
 *     (3*32757 + 32767)>>2 = 32759, 挪一步到 32760 —— 距离正好 7。
 *     v_prev = 32758 得 32761 —— 距离 6, 也分歧。目标要取到饱和值 32767,
 *     靠 driver 既有的 Poles=4 / PulseNum=1024 / FullFreq01=10000 /
 *     MDetaPos=1000 就够了 (m_Llong/m_Long = 640000, __IQsat 到 32767)。
 *
 * (b) ME60 (GetSpeedMTMethod): `if((m_Llong>>15) >= m_Long)` 改成 `>` 只在
 *     **恰好相等**时分歧。下面的三元组让 (m_Llong>>15) == m_Long 精确成立。
 *
 * (c) ME59 (GetSpeedMTMethod): `while((m_Long>>16) != 0)` 改成 `!= 1` 会让
 *     循环**少移一位**, 两个分支的商因此差 1。下面的三元组让归一化正好停在
 *     `!= 1` 变体的边界上。这里 VCSpeedFilter 必须 > 1 —— 走 Filter4 那支
 *     (`<= 1`) 时 `>>2` 会把 1 的差吞掉, 走一阶滤波那支才留得住。
 *
 * (b)(c) 的六组参数是用 build/_me_mt_search.py 对归一化循环建了个模型穷举
 * 出来的, 不是猜的; 该脚本把每条变异在参数网格上的"原式值 / 变异值"都算出来,
 * 只留下两者不相等且**不挂死**的组合 (ME59 在 `m_Long` 归到 0 时会死循环 ——
 * 那也是一种差异, 但会以 HUNG 而非 CAUGHT 记账, 所以不用)。
 */
static void phase_speed_edge(void)
{
    static int const snapPrev[] = { 32757, 32758 };
    static Uint const edgePoles[]  = { 4, 2, 1 };
    static Uint const edgeDeta[]   = { 4, 8, 16 };
    static Uint const ratioPoles[] = { 1, 1, 1 };
    static Uint const ratioDeta[]  = { 1, 4, 8 };
    static Ulong const ratioTime[] = { 1000, 10000, 500 };
    static Uint const ratioPulse[] = { 8, 3, 100 };
    unsigned long a, b;

    /* (a) GetSpeedMMethod: 距离 7 / 6 */
    for (a = 0; a < sizeof snapPrev / sizeof snapPrev[0]; a++)
    {
        case_begin();
        inSide.pgData.PulseNum              = 1024;
        inBasePar.FullFreq01                = 10000;
        inMotorExt.Poles                    = 4;
        inSide.fvc.MDetaPos                 = 1000;
        inSide.fvc.MDetaPosBak              = 1;
        inSide.fvc.MSpeed                   = (int)snapPrev[a];
        inSide.fvc.MSpeedSmooth.LastSpeed   = 32767;
        inSide.fvc.MSpeedSmooth.SpeedMaxErr = 32767;
        run_case(F_SPEEDM);
    }

    /* (a) GetSpeedMTMethod: 距离 7 / 6. 注意这里**不能**照抄 GetSpeedMMethod 的
     * PulseNum=1024/FullFreq01=10000 —— 两个函数算 m_Long 的式子不同,
     * MT 法下那组常数归一化后 (m_Llong>>15)=23437 < m_Long=40000, 走的是除法
     * 支路, m_Speed 只有 19200, 到不了饱和的 32767。要 PulseNum=600 才饱和
     * (见下面 (b) 的三元组, 同一组常数)。 */
    for (a = 0; a < sizeof snapPrev / sizeof snapPrev[0]; a++)
    {
        case_begin();
        inSide.fvc.MTPulseNum       = 4;
        inSide.fvc.MTTime           = 1000;
        inSide.fvc.Flag             = 0;
        inSide.fvc.MDetaPosBak      = 1;
        inSide.fvc.MTSpeed          = (int)snapPrev[a];
        inVcPar.VCSpeedFilter       = 1;
        inSide.qep.QEPSTS.bit.COEF  = 0;
        inSide.pgData.PulseNum      = 600;
        inBasePar.FullFreq01        = 10000;
        inMotorExt.Poles            = 4;
        run_case(F_SPEEDMT);
    }

    /* (b) ME60: (m_Llong>>15) == m_Long 精确相等 */
    for (a = 0; a < 3; a++)
    {
        case_begin();
        inMotorExt.Poles            = edgePoles[a];
        inSide.fvc.MTPulseNum       = edgeDeta[a];
        inSide.fvc.MTTime           = 1000;
        inSide.fvc.Flag             = 0;
        inSide.fvc.MDetaPosBak      = 1;
        inSide.fvc.MTSpeed          = 0;
        inVcPar.VCSpeedFilter       = 2;
        inSide.qep.QEPSTS.bit.COEF  = 0;
        inSide.pgData.PulseNum      = 600;
        inBasePar.FullFreq01        = 10000;
        run_case(F_SPEEDMT);
    }

    /* (c) ME59: 归一化停在 `!=1` 变体的边界, 商差 1 */
    for (a = 0; a < 3; a++)
    {
        for (b = 2; b <= 10; b += 8)
        {
            case_begin();
            inMotorExt.Poles            = ratioPoles[a];
            inSide.fvc.MTPulseNum       = ratioDeta[a];
            inSide.fvc.MTTime           = ratioTime[a];
            inSide.fvc.Flag             = 0;
            inSide.fvc.MDetaPosBak      = 1;
            inSide.fvc.MTSpeed          = 0;
            inVcPar.VCSpeedFilter       = (Uint)b;
            inSide.qep.QEPSTS.bit.COEF  = 0;
            inSide.pgData.PulseNum      = ratioPulse[a];
            inBasePar.FullFreq01        = 65535;
            run_case(F_SPEEDMT);
        }
    }
}

static void phase_mdeta(void)
{
    static Uint32 const lat[] = {
        0UL, 1UL, 2UL, 0x7FFFFFFFUL, 0x80000000UL, 0xFFFFFFFFUL
    };
    static long const bak[] = {
        0L, 1L, -1L, 0x7FFFFFFFL, (-0x7FFFFFFFL - 1L), 100000L
    };
    static int const detabak[] = { -32768, -1, 0, 1, 32767 };
    unsigned long a, b, c, k;

    for (a = 0; a < sizeof lat / sizeof lat[0]; a++)
    {
        for (b = 0; b < sizeof bak / sizeof bak[0]; b++)
        {
            for (c = 0; c < sizeof detabak / sizeof detabak[0]; c++)
            {
                for (k = 0; k < 2; k++)
                {
                    case_begin();
                    inSide.qep.QPOSLAT = lat[a];
                    inSide.fvc.MLastPos = bak[b];
                    inSide.fvc.MDetaPosBak = (int)detabak[c];
                    inSide.qep.QFLG.bit.UTO = (Uint16)k;
                    run_case(F_MDETA);
                }
            }
        }
    }
}

static void phase_mttime(void)
{
    static Uint const qcprd[] = { 0, 1, 100, 499, 500, 501, 0xFFFF };
    static Uint const limit[] = { 0, 1, 500, 501, 0xFFFF };
    static Uint const pulse[] = { 0, 4, 8, 0xFFFF };
    static Ulong const mtime[] = { 0, 1, 65535 };
    unsigned long a, b, c, d, k;

    for (a = 0; a < sizeof qcprd / sizeof qcprd[0]; a++)
    {
        for (b = 0; b < sizeof limit / sizeof limit[0]; b++)
        {
            for (c = 0; c < sizeof pulse / sizeof pulse[0]; c++)
            {
                for (d = 0; d < sizeof mtime / sizeof mtime[0]; d++)
                {
                    for (k = 0; k < 2; k++)
                    {
                        case_begin();
                        inSide.qep.QCPRD = qcprd[a];
                        inSide.fvc.MTLimitTime = limit[b];
                        inSide.fvc.MTPulseNum = pulse[c];
                        inSide.fvc.MTTime = mtime[d];
                        inSide.qep.QEPSTS.bit.UPEVNT = (Uint16)k;
                        run_case(F_MTTIME);
                    }
                }
            }
        }
    }
}

static void phase_rt_sample(void)
{
    static Uint32 const tim[] = { 0UL, 1UL, 26999UL, 27000UL, 27001UL,
                                  0xFFFFFFFFUL };
    static Ulong const bak[] = { 0UL, 1UL, 26999UL, 27000UL, 27001UL,
                                 0xFFFFFFFFUL };
    unsigned long a, b, k;

    for (a = 0; a < sizeof tim / sizeof tim[0]; a++)
    {
        for (b = 0; b < sizeof bak / sizeof bak[0]; b++)
        {
            for (k = 0; k < 2; k++)
            {
                case_begin();
                inTimer.TIM.all = tim[a];
                inSide.rotor.TimeBak = bak[b];
                inSide.rotor.DetaTime = (Ulong)rnd();
                inSide.rotor.Flag = (Uint)k;
                run_case(F_RTSAMPLE);
            }
        }
    }

    /* 差值正好压在 C_TIME_045MS = 60*450 = 27000 上。 */
    for (a = 0; a < 4; a++)
    {
        static int const dl[] = { 0, 1, 26999, 27000, 27001, -1, -26999,
                                  -27000, -27001 };
        unsigned long m;

        for (m = 0; m < sizeof dl / sizeof dl[0]; m++)
        {
            case_begin();
            inTimer.TIM.all = (Uint32)tim[a];
            inSide.rotor.TimeBak =
                (Ulong)((long)tim[a] + (long)dl[m]);
            run_case(F_RTSAMPLE);
        }
    }
}

static void phase_rt_calvel(void)
{
    static Uint const intcnt[] = { 0, 1, 2, 100, 0xFFFF };
    static Uint const prd[] = { 0, 1, 100, 1000, 0xFFFF };
    static Ulong const ff01[] = { 0, 1, 1000, 10000, 0xFFFFFFFFUL };
    static int const delta[] = {
        0, 1, -1, 1000, -1000, 32767, -32768, 0x7FFF, -0x8000
    };
    static Uint const fcset[] = { 0, 1, 100, 10000, 0xFFFF };
    static Uint const fullfreq[] = { 0, 1, 1000, 0x8000, 0xFFFF };
    static Uint const vcf[] = { 0, 1, 2, 10, 100 };
    static int const freqfeed[] = {
        0, 1, -1, 100, -100, 32767, -32768
    };
    unsigned long a, b, c, d, e, f, g, h;

    for (a = 0; a < sizeof intcnt / sizeof intcnt[0]; a++)
    {
        for (b = 0; b < sizeof prd / sizeof prd[0]; b++)
        {
            for (c = 0; c < sizeof ff01 / sizeof ff01[0]; c++)
            {
                for (d = 0; d < sizeof delta / sizeof delta[0]; d++)
                {
                    case_begin();
                    inSide.rotor.IntCnt = intcnt[a];
                    inPwm.gPWMPrdApply = prd[b];
                    inBasePar.FullFreq01 = ff01[c];
                    inSide.rotor.PosBak = 1000;
                    inSide.rotor.RTPos =
                        (Uint)((unsigned int)((unsigned short)1000) +
                               (unsigned int)((unsigned short)delta[d]));
                    inBasePar.FcSetApply = 10000;
                    inBasePar.FullFreq = 1000;
                    inVcPar.VCSpeedFilter = 1;
                    run_case(F_RTCALVEL);
                }
            }
        }
    }

    /* 第二步: 固定合法除数, 扫尾部的滤波 / PosComp 与 BurrFilter 的输入。 */
    for (e = 0; e < sizeof fcset / sizeof fcset[0]; e++)
    {
        for (f = 0; f < sizeof fullfreq / sizeof fullfreq[0]; f++)
        {
            for (g = 0; g < sizeof vcf / sizeof vcf[0]; g++)
            {
                for (h = 0; h < sizeof freqfeed / sizeof freqfeed[0]; h++)
                {
                    case_begin();
                    inSide.rotor.IntCnt = 2;
                    inPwm.gPWMPrdApply = 1000;
                    inBasePar.FullFreq01 = 10000;
                    inSide.rotor.PosBak = 1000;
                    inSide.rotor.RTPos = (Uint)(1000 + 100);
                    inBasePar.FcSetApply = fcset[e];
                    inBasePar.FullFreq = fullfreq[f];
                    inVcPar.VCSpeedFilter = vcf[g];
                    inSide.rotor.FreqFeed = (int)freqfeed[h];
                    inSide.rotor.RealTimeSpeed = (int)freqfeed[(h + 3) % 7];
                    run_case(F_RTCALVEL);
                }
            }
        }
    }
}

/* ---- (d) the bit-bang ---------------------------------------------------- */

static void phase_rt_pos(void)
{
    /* The serial frame. This replaces what used to be a 2-valued sweep of
     * GpioDataRegs.GPADAT.bit.GPIO21, which the host patch made dead.
     *
     * 0xFFF is the load-bearing entry: with every position set, mData has all
     * twelve bits and no two reads share a shift, so changing ANY one shift
     * amount drops exactly one bit and is caught. The others are not
     * redundant -- 0x555/0xAAA separate odd from even positions so a mutation
     * that duplicates a neighbouring shift is also visible, and 0x800/0x001
     * pin the two ends of the chain (first read and last read) on their own.
     * 0x000 keeps the all-zero case, which is the only one the old bench
     * could reach, so the new bench strictly contains the old. */
    static Uint const sopat[] = {
        0x000, 0xFFF, 0x555, 0xAAA, 0x800, 0x001
    };
    static Uint const dir[] = { 0, 1 };
    static Uint const runstep[] = { STATUS_GET_PAR, 0, 1 };
    static Uint const simu[] = { 0, 1, 0xFFFF };
    static Uint const zcnt[] = { 0, 1, 8 };
    static Uint const ratio[] = { 0, 1, 256, 0xFFFF };
    unsigned long a, b, c, d, e, f, g, h, i, k;

    for (a = 0; a < sizeof sopat / sizeof sopat[0]; a++)
    {
        for (b = 0; b < 2; b++)
        {
            for (c = 0; c < 2; c++)
            {
                for (d = 0; d < sizeof runstep / sizeof runstep[0]; d++)
                {
                    for (e = 0; e < sizeof simu / sizeof simu[0]; e++)
                    {
                        for (f = 0; f < sizeof simu / sizeof simu[0]; f++)
                        {
                            for (g = 0; g < sizeof zcnt / sizeof zcnt[0]; g++)
                            {
                                for (h = 0; h < sizeof ratio / sizeof ratio[0];
                                     h++)
                                {
                                    for (i = 0; i < 2; i++)
                                    {
                                        for (k = 0; k < 2; k++)
                                        {
                                            case_begin();
                                            set_so_pattern(sopat[a]);
                                            inSide.pgData.SpeedDir = dir[b];
                                            inSide.pgData.PGDir = dir[c];
                                            inSt.RunStep = runstep[d];
                                            inSide.rotor.SimuZBack = simu[e];
                                            inSide.rotor.SimuZBack2 = simu[f];
                                            inIpmZero.zFilterCnt = zcnt[g];
                                            inSide.rotor.PolesRatio = ratio[h];
                                            inIpmZero.Flag = (Uint)i;
                                            inIpmPos.PosInZInfo = (Uint)(k ? 0x1234 : 0);
                                            inIpmPos.RotorPos = (Uint)(k ? 0x4321 : 0);
                                            run_case(F_RTPOS);
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
}

/* ---- random sweep -------------------------------------------------------- */

static void phase_random(unsigned long n)
{
    unsigned long k;

    for (k = 0; k < n; k++)
    {
        case_begin();

        switch (rnd() % FN_COUNT)
        {
        case F_INITRT:
            run_case(F_INITRT);
            break;

        case F_INITQEP:
            run_case(F_INITQEP);
            break;

        case F_REINITPG:
            inSide.pgData.PGType = (Uint)(rnd() % 110);
            inExt.bit.QepIndex = (Uint)(rnd() & 3);
            run_case(F_REINITPG);
            break;

        case F_ABDIR:
            inIpmZero.FeedPos = (Uint)rnd();
            inSide.pgdir.ABAngleBak = (int)rnd();
            inSide.pgdir.ABDirCnt = (int)(rnd() % 40000) - 20000;
            run_case(F_ABDIR);
            break;

        case F_UVWDIR:
            inSide.uvwpg.UVWAngle = (Uint)rnd();
            inSide.pgdir.UVWAngleBak = (int)rnd();
            inSide.pgdir.UVWDirCnt = (int)(rnd() % 40000) - 20000;
            inCmd.FreqSet = (int)rnd();
            run_case(F_UVWDIR);
            break;

        case F_RTDIR:
            inSide.rotor.RTPos = (Uint)rnd();
            inSide.pgdir.RtPhaseBak = (int)rnd();
            inSide.pgdir.RtDirCnt = (int)(rnd() % 40000) - 20000;
            run_case(F_RTDIR);
            break;

        case F_UVWPHASE:
            inMotor.MotorType = (Uint)(rnd() % 4);
            inSide.pgData.PGType = (Uint)(rnd() % 8);
            inAdc.ADCRESULT9  = (Uint16)rnd();
            inAdc.ADCRESULT10 = (Uint16)rnd();
            inAdc.ADCRESULT11 = (Uint16)rnd();
            inSide.uvwpg.UvwDir = (Uint)(rnd() & 1);
            inPmParEst.UvwDir = (Uint)(rnd() & 1);
            run_case(F_UVWPHASE);
            break;

        case F_VCFB:
            inSide.pgData.PGType = (Uint)(rnd() % 110);
            inSide.fvc.SpeedTemp = (int)rnd();
            inSide.rotor.FreqFeed = (int)rnd();
            inSide.fvc.TransRatio = (Uint)rnd();
            inSide.pgData.PulseNum = (Uint)(rnd() % 4096);
            inBasePar.FullFreq01 = (Ulong)rnd();
            inSide.fvc.MTPulseNum = (Uint)(rnd() % 64);
            inSide.fvc.MTTime = (Ulong)rnd();
            inSide.fvc.MTLimitTime = (Uint)rnd();
            inSide.fvc.Flag = (int)(rnd() % 3);
            inSide.qep.QCPRD = (Uint16)rnd();
            inVcPar.VCSpeedFilter = (Uint)(rnd() % 8);
            run_case(F_VCFB);
            break;

        case F_SPEEDM:
            inSide.fvc.MDetaPos = (int)rnd();
            inSide.fvc.MDetaPosBak = (int)rnd();
            inMotorExt.Poles = (Uint)(rnd() % 16);
            inSide.pgData.PulseNum = (Uint)(rnd() % 4096);
            inBasePar.FullFreq01 = (Ulong)rnd();
            inSide.fvc.MSpeed = (int)rnd();
            inSide.fvc.MSpeedSmooth.LastSpeed = (int)rnd();
            inSide.fvc.MSpeedSmooth.SpeedMaxErr = (int)(rnd() % 40000);
            run_case(F_SPEEDM);
            break;

        case F_SPEEDMT:
            inSide.fvc.MTPulseNum = (Uint)(rnd() % 256);
            inSide.fvc.MTTime = (Ulong)rnd();
            inSide.fvc.Flag = (int)(rnd() % 3);
            inSide.fvc.MTLimitTime = (Uint)rnd();
            inSide.fvc.MDetaPosBak = (int)rnd();
            inSide.fvc.MTSpeed = (int)rnd();
            inSide.pgData.PulseNum = (Uint)(rnd() % 4096);
            inBasePar.FullFreq01 = (Ulong)rnd();
            inMotorExt.Poles = (Uint)(rnd() % 16);
            inVcPar.VCSpeedFilter = (Uint)(rnd() % 8);
            inSide.qep.QCPRD = (Uint16)rnd();
            run_case(F_SPEEDMT);
            break;

        case F_SMOOTH:
            inSide.fvc.MSpeedSmooth.LastSpeed = (int)rnd();
            inSide.fvc.MSpeedSmooth.SpeedMaxErr = (int)(rnd() % 40000);
            inSide.smoothSpeed = (int)rnd();
            run_case(F_SMOOTH);
            break;

        case F_MDETA:
            inSide.qep.QPOSLAT = (Uint32)rnd();
            inSide.fvc.MLastPos = (long)rnd();
            inSide.fvc.MDetaPosBak = (int)rnd();
            run_case(F_MDETA);
            break;

        case F_MTTIME:
            inSide.qep.QCPRD = (Uint16)rnd();
            inSide.fvc.MTLimitTime = (Uint)rnd();
            inSide.fvc.MTPulseNum = (Uint)rnd();
            inSide.fvc.MTTime = (Ulong)rnd();
            run_case(F_MTTIME);
            break;

        case F_RTPOS:
            set_so_pattern((Uint)(rnd() & 0x0FFF));
            inSide.pgData.SpeedDir = (Uint)(rnd() & 1);
            inSide.pgData.PGDir = (Uint)(rnd() & 1);
            inSt.RunStep = (Uint)(rnd() % 4);
            inSide.rotor.SimuZBack = (Uint)rnd();
            inSide.rotor.SimuZBack2 = (Uint)rnd();
            inIpmZero.zFilterCnt = (Uint)(rnd() % 12);
            inSide.rotor.PolesRatio = (Uint)rnd();
            inIpmZero.Flag = (Uint)(rnd() & 1);
            inIpmPos.PosInZInfo = (Uint)rnd();
            inIpmPos.RotorPos = (Uint)rnd();
            run_case(F_RTPOS);
            break;

        case F_RTSAMPLE:
            inTimer.TIM.all = (Uint32)rnd();
            inSide.rotor.TimeBak = (Ulong)rnd();
            inSide.rotor.DetaTime = (Ulong)rnd();
            inSide.rotor.Flag = (Uint)rnd();
            run_case(F_RTSAMPLE);
            break;

        default:
            inSide.rotor.IntCnt = (Uint)rnd();
            inPwm.gPWMPrdApply = (Uint)(rnd() % 4096);
            inBasePar.FullFreq01 = (Ulong)rnd();
            inBasePar.FcSetApply = (Uint)(rnd() % 4096);
            inBasePar.FullFreq = (Uint)rnd();
            inVcPar.VCSpeedFilter = (Uint)(rnd() % 8);
            inSide.rotor.FreqFeed = (int)rnd();
            inSide.rotor.RealTimeSpeed = (int)rnd();
            inSide.rotor.PosBak = (Uint)rnd();
            inSide.rotor.RTPos = (Uint)rnd();
            run_case(F_RTCALVEL);
            break;
        }
    }
}

/* ---- main ---------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 20000;
    long base;

    base = g_cases;
    phase_init_rt();
    printf("  InitRtInterface              : %ld\n", g_cases - base);

    base = g_cases;
    phase_init_qep();
    printf("  InitSetQEP                   : %ld\n", g_cases - base);

    base = g_cases;
    phase_reinit_pg();
    printf("  ReInitForPG                  : %ld\n", g_cases - base);

    base = g_cases;
    phase_judge_ab();
    printf("  JudgeABDir                   : %ld\n", g_cases - base);

    base = g_cases;
    phase_judge_uvw();
    printf("  JudgeUVWDir                  : %ld\n", g_cases - base);

    base = g_cases;
    phase_judge_rt();
    printf("  JudgeRTDir                   : %ld\n", g_cases - base);

    base = g_cases;
    phase_uvw_phase();
    printf("  GetUvwPhase                  : %ld\n", g_cases - base);

    base = g_cases;
    phase_vc_fb();
    printf("  VCGetFeedBackSpeed           : %ld\n", g_cases - base);

    base = g_cases;
    phase_speed_m();
    printf("  GetSpeedMMethod              : %ld\n", g_cases - base);

    base = g_cases;
    phase_speed_mt();
    printf("  GetSpeedMTMethod             : %ld\n", g_cases - base);

    base = g_cases;
    phase_smooth();
    printf("  SpeedSmoothDeal              : %ld\n", g_cases - base);

    base = g_cases;
    phase_speed_edge();
    printf("  speed edge states            : %ld\n", g_cases - base);

    base = g_cases;
    phase_mdeta();
    printf("  GetMDetaPos                  : %ld\n", g_cases - base);

    base = g_cases;
    phase_mttime();
    printf("  GetMTTimeNum                 : %ld\n", g_cases - base);

    base = g_cases;
    phase_rt_pos();
    printf("  GetRotorTransPos             : %ld\n", g_cases - base);

    base = g_cases;
    phase_rt_sample();
    printf("  RotorTransSamplePos          : %ld\n", g_cases - base);

    base = g_cases;
    phase_rt_calvel();
    printf("  RotorTransCalVel             : %ld\n", g_cases - base);

    base = g_cases;
    phase_random(n);
    printf("  random mixed                 : %ld\n", g_cases - base);

    printf("MotorEncoder difftest: cases=%ld mismatches=%d hosttraps=%ld "
           "skipped=%ld\n", g_cases, g_fails, g_hosttraps, g_skipped);
    return g_fails ? 1 : 0;
}
