/* Differential-test driver for MotorMain.
 *
 * The frozen baseline (cases/MotorMain/ref/MotorMain_ref.c) and the delivered
 * source (src/motor/MotorMain.c) are compiled into two symbol namespaces,
 * ref_* and new_*.  MotorMain.c defines NO globals of its own, so there is no
 * per-side private state: the driver keeps ONE input image of the 56 globals
 * the module reads/writes, hands it to the ref side, restores it, hands the
 * same image to the new side, and diffs the two results.
 *
 * ---------------------------------------------------------------------------
 * What is compared
 * ---------------------------------------------------------------------------
 *   - every one of the module object file's 56 undefined DATA globals, whole
 *     struct, byte for byte (that is where all of the module's output lives);
 *   - the 73-entry callee call ledger gMockCall[] -- a rewrite that drops,
 *     duplicates or reorders a call changes these counts even when the callee
 *     body is not modelled;
 *   - gAsmCount, i.e. whether the `asm(" NOP")` in the PWM-disabled branch was
 *     reached;
 *   - gHostCpuCtl[7], the DINT/EINT sequence inside SendDataPrepare.
 *
 * ---------------------------------------------------------------------------
 * Turning constants into inputs
 * ---------------------------------------------------------------------------
 * Several branches are gated on a field another module owns in the running
 * system.  Those are inputs, and the phases poke them directly so every branch
 * is reachable: RunStep, Command.bit.*, ExtendCmd.bit.*, gVCPar.SvcMode,
 * gGetParVarable.* (TUNE_STEP_NOW), gIPMInitPos.Flag/Step, gPGData.PGMode/
 * PGType, gMotorInfo.MotorType, the GpioDataRegs.AIODAT.AIO2 bit behind
 * GetOverUdcFlag(), and so on.
 *
 * ---------------------------------------------------------------------------
 * Division-by-zero is a real input-space hazard, not a style point
 * ---------------------------------------------------------------------------
 * Four divisions take a register as the divisor:
 *     Main2msMotorB     / gBasePar.FullFreq01        (twice: 50 Hz and 20 Hz pu)
 *     SendDataPrepare   / gAI.aiCounter              (three times)
 *     SendDataPrepare   / gInvInfo.BaseUdc
 *     CalRatioFromVot   / gInvInfo.InvVolt  and  / gUDC.uDC
 * A zero there does not produce a "mismatch", it kills the process, which would
 * silently destroy the whole ledger.  dirty_all() therefore pins all five
 * non-zero on every scenario, and the random phase re-pins them after its own
 * randomisation.
 *
 * ---------------------------------------------------------------------------
 * 0xA5 discipline
 * ---------------------------------------------------------------------------
 * The comparison only sees what the NEW side leaves behind.  A mutation that
 * makes new *stop writing* a field is invisible if the pre-existing value
 * already equals what correct code writes.  So every one of the 56 globals is
 * pre-filled with 0xA5 before each scenario, then the fields the module reads
 * for control are laid down explicitly.
 */

#include <stdio.h>
#include <string.h>

/* <stdlib.h> is deliberately NOT included: the case shim folds `int` to the
 * target's 16 bits, and stdlib's 32-bit `int abs(int)` would then collide with
 * the project's folded 16-bit prototype in SubPrgInclude.h.  Only atol is
 * needed, and it is declared by hand. */
extern long atol(const char *);

#include "MotorInclude.h"
#include "mock_calls.h"

/* ---- renamed entry points (15 = the ref side's whole T-symbol set) -------- */

extern void ref_InitForMotorApp(void);
extern void ref_Main0msMotor(void);
extern void ref_Main05msMotor(void);
extern void ref_Main2msMotorA(void);
extern void ref_Main2msMotorB(void);
extern void ref_Main2msMotorC(void);
extern void ref_Main2msMotorD(void);
extern void ref_SendDataPrepare(void);
extern void ref_RunCaseRun2Ms(void);
extern void ref_PrepareParForRun(void);
extern void ref_TurnToStopStatus(void);
extern void ref_RunCaseStop(void);
extern void ref_CalRatioFromVot(void);
extern void ref_ADCOverInterrupt(void);
extern void ref_PWMZeroInterrupt(void);

extern void new_InitForMotorApp(void);
extern void new_Main0msMotor(void);
extern void new_Main05msMotor(void);
extern void new_Main2msMotorA(void);
extern void new_Main2msMotorB(void);
extern void new_Main2msMotorC(void);
extern void new_Main2msMotorD(void);
extern void new_SendDataPrepare(void);
extern void new_RunCaseRun2Ms(void);
extern void new_PrepareParForRun(void);
extern void new_TurnToStopStatus(void);
extern void new_RunCaseStop(void);
extern void new_CalRatioFromVot(void);
extern void new_ADCOverInterrupt(void);
extern void new_PWMZeroInterrupt(void);

/* ---- mock-owned ----------------------------------------------------------- */

extern unsigned short                   gMockCall[MC_COUNT];
extern unsigned short                   gAsmCount;
extern volatile Uint16                  gHostCpuCtl[7];

/* ---- the input image: one entry per undefined DATA symbol ---------------- */
/* Order and types are identical to mock_motormain.c's definitions.  X(name,
 * type) must stay a macro so the struct, the save, the load and the compare
 * are all generated from this single list. */

#define IMG_LIST(X)                                                        \
    X(gInvInfo,                          INV_STRUCT)                       \
    X(gCtrMotorType,                     CONTROL_MOTOR_TYPE_ENUM)          \
    X(gMotorInfo,                        MOTOR_STRUCT)                     \
    X(gMainStatus,                       RUN_STATUS_STRUCT)                \
    X(gMainCmd,                          BASE_COMMAND_STRUCT)              \
    X(gExtendCmd,                        MAIN_COMMAND_EXTEND_UNION)        \
    X(gBasePar,                          BASE_PAR_STRUCT)                  \
    X(gComPar,                           COM_PAR_INFO_STRUCT)              \
    X(gCpuTime,                          CPU_TIME_STRUCT)                  \
    X(gTestDataReceive,                  MOTOR_DEBUG_DATA_RECEIVE_STRUCT)  \
    X(gADC,                              ADC_STRUCT)                       \
    X(gUDC,                              UDC_STRUCT)                       \
    X(gCurSamp,                          IUVW_SAMPLING_STRUCT)             \
    X(gIUVWQ12,                          UVW_STRUCT)                       \
    X(gIUVWQ24,                          UVW_STRUCT_Q24)                   \
    X(gIAlphBeta,                        ALPHABETA_STRUCT)                 \
    X(gIMTQ12,                           MT_STRUCT)                        \
    X(gIMTQ24,                           MT_STRUCT_Q24)                    \
    X(gIMTQ12_obs,                       MT_STRUCT_Q24)                    \
    X(gIMTQ24_obs,                       MT_STRUCT_Q24)                    \
    X(gIMTSet,                           MT_STRUCT_Q24)                    \
    X(gIAmpTheta,                        AMPTHETA_STRUCT)                  \
    X(gLineCur,                          LINE_CURRENT_STRUCT)              \
    X(gOutVolt,                          OUT_VOLT_STRUCT)                  \
    X(gFcCal,                            FC_CAL_STRUCT)                    \
    X(gPWM,                              PWM_OUT_STRUCT)                   \
    X(gDeadBand,                         DEAD_BAND_STRUCT)                 \
    X(gRatio,                            Uint)                             \
    X(gCBCProtect,                       CBC_PROTECT_STRUCT)               \
    X(gError,                            struct FAULT_CODE_INFOR_STRUCT_DEF)\
    X(gDCBrake,                          DC_BRAKE_STRUCT)                  \
    X(gAI,                               AI_STRUCT)                        \
    X(gUVCoff,                           UV_BIAS_COFF_STRUCT)              \
    X(gGetParVarable,                    MOTOR_PARA_EST)                   \
    X(gVFPar,                            VF_INFO_STRUCT)                   \
    X(gWsComp,                           VF_WS_COMP_STRUCT)                \
    X(gVarAvr,                           VAR_AVR_STRUCT)                   \
    X(gHVfOscDamp,                       HVF_OSC_DAMP_STRUCT)              \
    X(gVCPar,                            VC_INFO_STRUCT)                   \
    X(gImAcrQ24,                         PID32_STRUCT)                     \
    X(gItAcrQ24,                         PID32_STRUCT)                     \
    X(gFluxR,                            FLUX_STRUCT)                      \
    X(gFeisu,                            FEISU_STRUCT)                     \
    X(gFeisuNew,                         FEISU_STRUCT_NEW)                 \
    X(gIPMPos,                           IPM_POSITION_STRUCT)              \
    X(gFluxWeak,                         PM_FLUX_WEAK)                     \
    X(gIPMPosCheck,                      IPM_POS_CHECK_STRUCT)             \
    X(gIPMZero,                          IPM_ZERO_POS_STRUCT)              \
    X(gIPMInitPos,                       IPM_INITPOS_PULSE_STR)            \
    X(gPmParEst,                         PMSM_EST_PARAM_DATA)              \
    X(gEstBemf,                          PMSM_EST_BEMF)                    \
    X(gUVWPG,                            IPM_UVW_PG_STRUCT)                \
    X(gPGData,                           PG_DATA_STRUCT)                   \
    X(gSpeedFilter,                      BURR_FILTER_STRUCT)               \
    X(gRotorTrans,                       ROTOR_TRANS_STRUCT)               \
    X(gFVCSpeed,                         FVC_SPEED_STRUCT)                 \
    X(GpioDataRegs,                      struct GPIO_DATA_REGS)

struct IMAGE {
#define IMG_MEM(name, type) type name;
    IMG_LIST(IMG_MEM)
#undef IMG_MEM
};

static struct IMAGE imgIn;
static struct IMAGE imgRef;
static struct IMAGE imgNew;

static void img_save(struct IMAGE *img)
{
#define IMG_SAVE(name, type) img->name = name;
    IMG_LIST(IMG_SAVE)
#undef IMG_SAVE
}

static void img_load(const struct IMAGE *img)
{
#define IMG_LOAD(name, type) name = img->name;
    IMG_LIST(IMG_LOAD)
#undef IMG_LOAD
}

/* ---- call ledger snapshots ----------------------------------------------- */

static unsigned short refCall[MC_COUNT];
static unsigned short newCall[MC_COUNT];
static unsigned short refCpu[7];
static unsigned short newCpu[7];
static unsigned short refAsm;
static unsigned short newAsm;

/* ---- counters ------------------------------------------------------------ */

static long g_cases;
static long g_mism;
static long g_mismShown;

/* input-reach counters, asserted at the end (see main) */
static long g_covFn[16];
static long g_covSvcMode[2];
static long g_covTuneStep[8];
static long g_cov2msB_SpeedCheck;
static long g_cov2msB_m05Clamp;
static long g_covRun2msVF;
static long g_covRun2msSVC;
static long g_covRun2msSyncFvc;
static long g_covSendLow;
static long g_covSendGo;
static long g_covStopRunEnable;
static long g_covStopShortGnd;
static long g_covStopGetPar;
static long g_covStopSpeedCheck;
static long g_covStopIpmInit;
static long g_covStopRun;
static long g_covAdcResolver;
static long g_covAdcOverUdc;
static long g_covAdcDead280;
static long g_covAdcDead380;
static long g_covAdcSynInit;
static long g_covAdcUvwPhase;
static long g_covAdcLmIo;
static long g_covPwmDisable;
static long g_covPwmOut;
static long g_covRatioClamp;
static long g_covCpuCtl;

/* edge-probe reach counters (see phase_edges) */
static long g_covEdgeClamp;
static long g_covEdge380;
static long g_covEdgePhase;
static long g_covEdgeDefault;

/* ------------------------------------------------------------------------- */
/* entry-point dispatch                                                      */
/* ------------------------------------------------------------------------- */

enum {
    F_INIT = 0, F_MAIN0MS, F_MAIN05MS, F_MAIN2MSA, F_MAIN2MSB, F_MAIN2MSC,
    F_MAIN2MSD, F_SENDDATA, F_RUN2MS, F_PREPPAR, F_TURNTSTOP, F_RUNSTOP,
    F_CALRATIO, F_ADCOVER, F_PWMZERO, F_COUNT
};

static void call_ref(int fn)
{
    g_covFn[fn]++;
    switch (fn)
    {
        case F_INIT:      ref_InitForMotorApp();  break;
        case F_MAIN0MS:   ref_Main0msMotor();     break;
        case F_MAIN05MS:  ref_Main05msMotor();    break;
        case F_MAIN2MSA:  ref_Main2msMotorA();    break;
        case F_MAIN2MSB:  ref_Main2msMotorB();    break;
        case F_MAIN2MSC:  ref_Main2msMotorC();    break;
        case F_MAIN2MSD:  ref_Main2msMotorD();    break;
        case F_SENDDATA:  ref_SendDataPrepare();  break;
        case F_RUN2MS:    ref_RunCaseRun2Ms();    break;
        case F_PREPPAR:   ref_PrepareParForRun(); break;
        case F_TURNTSTOP: ref_TurnToStopStatus(); break;
        case F_RUNSTOP:   ref_RunCaseStop();      break;
        case F_CALRATIO:  ref_CalRatioFromVot();  break;
        case F_ADCOVER:   ref_ADCOverInterrupt(); break;
        default:          ref_PWMZeroInterrupt(); break;
    }
}

static void call_new(int fn)
{
    switch (fn)
    {
        case F_INIT:      new_InitForMotorApp();  break;
        case F_MAIN0MS:   new_Main0msMotor();     break;
        case F_MAIN05MS:  new_Main05msMotor();    break;
        case F_MAIN2MSA:  new_Main2msMotorA();    break;
        case F_MAIN2MSB:  new_Main2msMotorB();    break;
        case F_MAIN2MSC:  new_Main2msMotorC();    break;
        case F_MAIN2MSD:  new_Main2msMotorD();    break;
        case F_SENDDATA:  new_SendDataPrepare();  break;
        case F_RUN2MS:    new_RunCaseRun2Ms();    break;
        case F_PREPPAR:   new_PrepareParForRun(); break;
        case F_TURNTSTOP: new_TurnToStopStatus(); break;
        case F_RUNSTOP:   new_RunCaseStop();      break;
        case F_CALRATIO:  new_CalRatioFromVot();  break;
        case F_ADCOVER:   new_ADCOverInterrupt(); break;
        default:          new_PWMZeroInterrupt(); break;
    }
}

/* ------------------------------------------------------------------------- */
/* comparison                                                                */
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

static void compare(const char *tag)
{
#define IMG_CMP(name, type) CMPM(imgRef.name, imgNew.name, #name);
    IMG_LIST(IMG_CMP)
#undef IMG_CMP

    CMPM(refCall, newCall, "gMockCall");
    CMPM(refCpu,  newCpu,  "gHostCpuCtl");
    CMPM(refAsm,  newAsm,  "gAsmCount");
}

/* ------------------------------------------------------------------------- */
/* one step                                                                  */
/* ------------------------------------------------------------------------- */

static void zero_counters(void)
{
    memset(gMockCall, 0, sizeof(gMockCall));
    gAsmCount = 0;
    memset((void *)gHostCpuCtl, 0, sizeof(Uint16) * 7U);
}

static void do_step(int fn, const char *tag)
{
    g_cases++;

    img_save(&imgIn);

    zero_counters();
    call_ref(fn);
    img_save(&imgRef);
    memcpy(refCall, gMockCall, sizeof(refCall));
    memcpy(refCpu, (const void *)gHostCpuCtl, sizeof(refCpu));
    refAsm = gAsmCount;
    if (refCpu[0] != 0U || refCpu[1] != 0U)
    {
        g_covCpuCtl++;
    }

    img_load(&imgIn);
    zero_counters();
    call_new(fn);
    img_save(&imgNew);
    memcpy(newCall, gMockCall, sizeof(newCall));
    memcpy(newCpu, (const void *)gHostCpuCtl, sizeof(newCpu));
    newAsm = gAsmCount;

    compare(tag);
}

/* ------------------------------------------------------------------------- */
/* deterministic-random helpers                                              */
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

/* Separate stream for seeding inputs: it makes a scenario's inputs depend only
 * on that scenario's seed, not on how many rnd() calls the phases made. */
static unsigned long g_ds;

static unsigned long drnd(void)
{
    unsigned long x;

    g_ds = g_ds * 1664525UL + 1013904223UL;
    x = g_ds;
    x ^= x >> 16;
    x *= 0x7FEB352DUL;
    x ^= x >> 15;
    return x;
}

static long rnd_int(long lo, long hi)
{
    if (hi <= lo)
    {
        return lo;
    }
    return lo + (long)(rnd() % (unsigned long)(hi - lo + 1));
}

static long drnd_int(long lo, long hi)
{
    if (hi <= lo)
    {
        return lo;
    }
    return lo + (long)(drnd() % (unsigned long)(hi - lo + 1));
}

/* Pin every divisor the module divides by to a non-zero, non-trivial value. */
static void pin_divisors(void)
{
    gBasePar.FullFreq01 = (Ulong)(drnd_int(1000, 60000));
    if (gBasePar.FullFreq01 == 0UL)
    {
        gBasePar.FullFreq01 = 3000UL;
    }
    gAI.aiCounter = (int)(drnd_int(1, 1000));
    gInvInfo.BaseUdc = (Uint)(drnd_int(100, 3000));
    gInvInfo.InvVolt  = (Uint)(drnd_int(100, 3000));
    gUDC.uDC          = (Uint)(drnd_int(1, 3000));
}

/* ------------------------------------------------------------------------- */
/* scenario setup                                                            */
/* ------------------------------------------------------------------------- */

static void dirty_all(unsigned long seed)
{
    long i;

    /* 1. Every compared word starts at 0xA5 ... */
    memset(&gInvInfo, 0xA5, sizeof(gInvInfo));
    memset(&gCtrMotorType, 0xA5, sizeof(gCtrMotorType));
    memset(&gMotorInfo, 0xA5, sizeof(gMotorInfo));
    memset(&gMainStatus, 0xA5, sizeof(gMainStatus));
    memset(&gMainCmd, 0xA5, sizeof(gMainCmd));
    memset(&gExtendCmd, 0xA5, sizeof(gExtendCmd));
    memset(&gBasePar, 0xA5, sizeof(gBasePar));
    memset(&gComPar, 0xA5, sizeof(gComPar));
    memset(&gCpuTime, 0xA5, sizeof(gCpuTime));
    memset(&gTestDataReceive, 0xA5, sizeof(gTestDataReceive));
    memset(&gADC, 0xA5, sizeof(gADC));
    memset(&gUDC, 0xA5, sizeof(gUDC));
    memset(&gCurSamp, 0xA5, sizeof(gCurSamp));
    memset(&gIUVWQ12, 0xA5, sizeof(gIUVWQ12));
    memset(&gIUVWQ24, 0xA5, sizeof(gIUVWQ24));
    memset(&gIAlphBeta, 0xA5, sizeof(gIAlphBeta));
    memset(&gIMTQ12, 0xA5, sizeof(gIMTQ12));
    memset(&gIMTQ24, 0xA5, sizeof(gIMTQ24));
    memset(&gIMTQ12_obs, 0xA5, sizeof(gIMTQ12_obs));
    memset(&gIMTQ24_obs, 0xA5, sizeof(gIMTQ24_obs));
    memset(&gIMTSet, 0xA5, sizeof(gIMTSet));
    memset(&gIAmpTheta, 0xA5, sizeof(gIAmpTheta));
    memset(&gLineCur, 0xA5, sizeof(gLineCur));
    memset(&gOutVolt, 0xA5, sizeof(gOutVolt));
    memset(&gFcCal, 0xA5, sizeof(gFcCal));
    memset(&gPWM, 0xA5, sizeof(gPWM));
    memset(&gDeadBand, 0xA5, sizeof(gDeadBand));
    memset(&gCBCProtect, 0xA5, sizeof(gCBCProtect));
    memset(&gError, 0xA5, sizeof(gError));
    memset(&gDCBrake, 0xA5, sizeof(gDCBrake));
    memset(&gAI, 0xA5, sizeof(gAI));
    memset(&gUVCoff, 0xA5, sizeof(gUVCoff));
    memset(&gGetParVarable, 0xA5, sizeof(gGetParVarable));
    memset(&gVFPar, 0xA5, sizeof(gVFPar));
    memset(&gWsComp, 0xA5, sizeof(gWsComp));
    memset(&gVarAvr, 0xA5, sizeof(gVarAvr));
    memset(&gHVfOscDamp, 0xA5, sizeof(gHVfOscDamp));
    memset(&gVCPar, 0xA5, sizeof(gVCPar));
    memset(&gImAcrQ24, 0xA5, sizeof(gImAcrQ24));
    memset(&gItAcrQ24, 0xA5, sizeof(gItAcrQ24));
    memset(&gFluxR, 0xA5, sizeof(gFluxR));
    memset(&gFeisu, 0xA5, sizeof(gFeisu));
    memset(&gFeisuNew, 0xA5, sizeof(gFeisuNew));
    memset(&gIPMPos, 0xA5, sizeof(gIPMPos));
    memset(&gFluxWeak, 0xA5, sizeof(gFluxWeak));
    memset(&gIPMPosCheck, 0xA5, sizeof(gIPMPosCheck));
    memset(&gIPMZero, 0xA5, sizeof(gIPMZero));
    memset(&gIPMInitPos, 0xA5, sizeof(gIPMInitPos));
    memset(&gPmParEst, 0xA5, sizeof(gPmParEst));
    memset(&gEstBemf, 0xA5, sizeof(gEstBemf));
    memset(&gUVWPG, 0xA5, sizeof(gUVWPG));
    memset(&gPGData, 0xA5, sizeof(gPGData));
    memset(&gSpeedFilter, 0xA5, sizeof(gSpeedFilter));
    memset(&gRotorTrans, 0xA5, sizeof(gRotorTrans));
    memset(&gFVCSpeed, 0xA5, sizeof(gFVCSpeed));
    memset((void *)&GpioDataRegs, 0xA5, sizeof(GpioDataRegs));

    /* The module only ever *sets* the drive / sample / resolver pins, and the
     * 0xA5 prefill already leaves every one of those bits at 1 -- which would
     * hide both "dropped the write" and "wrote the other enable register"
     * (DisableDrive -> GPASET vs EnableDrive -> GPACLEAR).  Clear exactly the
     * six pins the module can touch so those writes are visible. */
    GpioDataRegs.GPASET.bit.GPIO7 = 0;
    GpioDataRegs.GPACLEAR.bit.GPIO7 = 0;
    GpioDataRegs.AIOCLEAR.bit.AIO10 = 0;
    GpioDataRegs.AIOSET.bit.AIO10 = 0;
    GpioDataRegs.GPADAT.bit.GPIO23 = 0;
    GpioDataRegs.AIODAT.bit.AIO12 = 0;

    /* 2. ... then a scenario-specific stream lays down real values. */
    g_ds = seed * 2654435761UL + 0x9E3779B9UL;

    /* ---- divisors first (see the header note) ---- */
    pin_divisors();

    /* ---- control-flow inputs ---- */
    gMainStatus.RunStep = (Uint)drnd_int(0, 7);
    gMainStatus.SubStep = (Uint)drnd_int(0, 7);
    gMainStatus.StatusWord.all = (Uint)(drnd() & 0x01FFUL);
    gMainStatus.PrgStatus.all  = (Uint)(drnd() & 0x0007UL);
    gMainStatus.ErrFlag.all    = (Uint16)(drnd() & 0x0003UL);
    gMainStatus.ParaCalTimes   = (Uint)drnd_int(0, 1000);

    gMainCmd.Command.all = (Uint)(drnd() & 0x01FFUL);
    gMainCmd.FreqSet      = (int)drnd_int(-32768, 32767);
    gMainCmd.FreqSyn      = (int)drnd_int(-32768, 32767);
    gMainCmd.FreqFeed     = (int)drnd_int(-32768, 32767);
    gMainCmd.FreqToFunc   = (int)drnd_int(-32768, 32767);
    gMainCmd.FreqSetApply = (int)drnd_int(-32768, 32767);
    gMainCmd.FreqSetBak   = (int)drnd_int(-32768, 32767);
    gMainCmd.FreqReal     = (long)drnd_int(-100000, 100000);
    gMainCmd.FreqRealFilt = (long)drnd_int(-100000, 100000);

    gExtendCmd.all = (Uint)(drnd() & 0xFFFFUL);

    gError.ErrorCode.all  = (Uint32)drnd();
    gError.LastErrorCode  = (Uint32)drnd();

    gADC.ZeroCnt  = (int)drnd_int(0, 100);
    gADC.DelaySet = (Uint)drnd_int(0, 1000);
    gADC.DelayApply = (Uint)drnd_int(0, 1000);
    gADC.Comp     = (int)drnd_int(-1000, 1000);

    gFcCal.FcBak = (Uint)drnd_int(1, 200);
    gBasePar.FcSetApply = (Uint)drnd_int(1, 200);

    gUVCoff.UDivV = (Uint)drnd_int(1, 4096);

    gMotorInfo.MotorType     = (Uint)drnd_int(0, 3);
    gMotorInfo.LastMotorType = (Uint)drnd_int(0, 3);
    gMotorInfo.Votage        = (Uint)drnd_int(1, 4000);
    gMotorInfo.Current       = (Uint)drnd_int(1, 3000);
    gMotorInfo.CurrentGet    = (Uint)drnd_int(0, 6000);
    gMotorInfo.FreqPer       = (Uint)drnd_int(0, 8000);

    gPGData.PGMode = (Uint)drnd_int(0, 2);
    gPGData.PGType = (Uint)drnd_int(0, 6);

    gIPMZero.zFilterCnt = (Uint)drnd_int(0, 100);
    gIPMInitPos.Flag    = (Uint)drnd_int(0, 1);
    gIPMInitPos.Step    = (Uint)drnd_int(0, 3);
    gIPMInitPos.InitPWMTs = (Uint)drnd_int(0, 1000);

    gVCPar.SvcMode = (Uint)drnd_int(0, 1);
    gTestDataReceive.TestData1 = (int)drnd_int(0, 1);

    gFeisu.SpeedCheck = (int)drnd_int(-32768, 32767);
    gFeisu.VoltCheck  = (Uint)drnd_int(0, 65535);
    gFeisu.SpeedLast  = (int)drnd_int(-32768, 32767);

    gComPar.SpdSearchMethod = (Uint)drnd_int(0, 4);
    gComPar.MotorOvLoad     = (Uint)drnd_int(1, 1000);
    gComPar.PerMotorOvLoad  = (Uint)drnd_int(1, 100);

    gUVWPG.UvwEstStep = (Uint)drnd_int(0, 3);
    gUVWPG.UVWAngle   = (Uint)drnd_int(0, 65535);
    gUVWPG.TotalErr   = (llong)0;

    gRotorTrans.RTPos   = (Uint)drnd_int(0, 65535);
    gRotorTrans.PosComp = (int)drnd_int(-32768, 32767);
    gRotorTrans.Flag    = (Uint)drnd_int(0, 1);

    gIPMPos.RotorPos = (Uint)drnd_int(0, 65535);

    gPmParEst.IdKp = (Uint)drnd_int(0, 3000);
    gPmParEst.IdKi = (Uint)drnd_int(0, 3000);
    gPmParEst.IqKp = (Uint)drnd_int(0, 3000);
    gPmParEst.IqKi = (Uint)drnd_int(0, 3000);

    gEstBemf.TuneFreqSet = (int)drnd_int(-32768, 32767);
    gEstBemf.IdSet       = (int)drnd_int(-32768, 32767);
    gEstBemf.IdSetFilt   = (int)drnd_int(-32768, 32767);

    gHVfOscDamp.VoltPhase = (int)drnd_int(-32768, 32767);
    gHVfOscDamp.VoltAmp   = (int)drnd_int(-32768, 32767);

    gOutVolt.VoltApply   = (int)drnd_int(-32768, 32767);
    gOutVolt.Volt        = (int)drnd_int(-32768, 32767);
    gOutVolt.VoltDisplay = (int)drnd_int(-32768, 32767);
    gOutVolt.VoltPhaseApply = (int)drnd_int(-32768, 32767);

    gVFPar.FreqApply = (int)drnd_int(-32768, 32767);
    gVFPar.tpLst     = (int)drnd_int(-32768, 32767);
    gWsComp.CompFreq = (long)drnd_int(-100000, 100000);

    gIMTQ12.M = (int)drnd_int(-32768, 32767);
    gIMTQ12.T = (int)drnd_int(-32768, 32767);
    gIMTSet.T = (long)drnd_int(-100000, 100000);
    gIMTSet.M = (long)drnd_int(-100000, 100000);

    gAI.gAI1 = (Uint)drnd_int(0, 65535);
    gAI.gAI2 = (Uint)drnd_int(0, 65535);
    gAI.gAI3 = (Uint)drnd_int(0, 65535);
    gAI.ai1Total = (Ulong)drnd();
    gAI.ai2Total = (Ulong)drnd();
    gAI.ai3Total = (Ulong)drnd();

    gRatio = (Uint)drnd_int(0, 5000);

    gLineCur.CurBaseInv  = (Uint)drnd_int(0, 6000);
    gLineCur.CurPer      = (Uint)drnd_int(0, 6000);
    gLineCur.CurPerFilter = (Ulong)drnd();
    gLineCur.CurPerShow  = (Uint)drnd_int(0, 6000);
    gLineCur.CurTorque   = (int)drnd_int(-32768, 32767);

    gIAmpTheta.Theta       = (int)drnd_int(-32768, 32767);
    gIAmpTheta.ThetaFilter = (int)drnd_int(-32768, 32767);

    gSpeedFilter.Max    = (long)drnd_int(-100000, 100000);
    gSpeedFilter.Min    = (long)drnd_int(-100000, 100000);
    gSpeedFilter.Output = (long)drnd_int(-100000, 100000);

    gCpuTime.Det05msClk = (Uint)drnd_int(0, 65535);
    gCpuTime.tmp0Ms     = (Uint)drnd_int(0, 65535);

    gDCBrake.Time = (int)drnd_int(0, 1000);

    gCurSamp.U = (long)drnd_int(-100000, 100000);
    gCurSamp.V = (long)drnd_int(-100000, 100000);
    gCurSamp.W = (long)drnd_int(-100000, 100000);
    gCurSamp.UErr = (long)drnd_int(-100000, 100000);
    gCurSamp.VErr = (long)drnd_int(-100000, 100000);

    gIUVWQ12.U = (int)drnd_int(-32768, 32767);
    gIUVWQ12.V = (int)drnd_int(-32768, 32767);
    gIUVWQ12.W = (int)drnd_int(-32768, 32767);
    gIUVWQ24.U = (long)drnd_int(-100000, 100000);
    gIUVWQ24.V = (long)drnd_int(-100000, 100000);
    gIUVWQ24.W = (long)drnd_int(-100000, 100000);

    gIAlphBeta.Alph = (long)drnd_int(-100000, 100000);
    gIAlphBeta.Beta = (long)drnd_int(-100000, 100000);
    gFluxR.Theta    = (Uint)drnd_int(0, 65535);

    gIMTQ24.M = (long)drnd_int(-100000, 100000);
    gIMTQ24.T = (long)drnd_int(-100000, 100000);

    gVarAvr.UDCFilter = (int)drnd_int(-32768, 32767);

    gCBCProtect.EnableFlag = (int)drnd_int(0, 1);

    gPWM.gPWMPrd        = (Uint)drnd_int(1, 10000);
    gPWM.gPWMPrdApply   = (Uint)drnd_int(1, 10000);
    gPWM.PWMModle       = (Uint)drnd_int(0, 1);
    gPWM.gZeroLengthPhase = (ZERO_LENGTH_PHASE_SELECT_ENUM)drnd_int(0, 3);

    gDeadBand.CompU = (int)drnd_int(-32768, 32767);
    gDeadBand.CompV = (int)drnd_int(-32768, 32767);
    gDeadBand.CompW = (int)drnd_int(-32768, 32767);

    gFluxWeak.AdjustId = (int)drnd_int(-32768, 32767);

    gFeisuNew.t_DetaTime = (Uint)drnd_int(0, 65535);
    gFeisuNew.stop_time  = (Uint)drnd_int(0, 65535);
    gFeisuNew.inh_mag    = (Uint)drnd_int(0, 65535);
    gFeisuNew.ang_amu    = (long)drnd_int(-100000, 100000);
    gFeisuNew.jicicg     = (int)drnd_int(-32768, 32767);
    gFeisuNew.jicics     = (int)drnd_int(-32768, 32767);
    gFeisuNew.gWs_out    = (int)drnd_int(-32768, 32767);

    /* ---- TUNE_STEP_NOW: ParEstContent[ParEstMstep], index pinned to 0..4 ---- */
    gGetParVarable.ParEstMstep = (Uint16)drnd_int(0, 4);
    for (i = 0; i < 5; i++)
    {
        gGetParVarable.ParEstContent[i] = (PAR_EST_MAIN_STEP)drnd_int(0, 7);
    }
    gGetParVarable.StatusWord = (TUNE_TO_FUNCTION_WORLD_ENUM)drnd_int(0, 3);
    gGetParVarable.TuneType   = (TUNE_FLOW_ENUM)drnd_int(0, 3);

    /* ---- GpioDataRegs: default GetOverUdcFlag() false ---- */
    GpioDataRegs.AIODAT.bit.AIO2 = 1;
}

static void begin_scenario(unsigned long seed)
{
    dirty_all(seed);
}

static void end_scenario(int fn, const char *tag)
{
    do_step(fn, tag);
}

/* ------------------------------------------------------------------------- */
/* phases                                                                    */
/* ------------------------------------------------------------------------- */

/* ---- InitForMotorApp ----------------------------------------------------- */

static void phase_init(void)
{
    static const Uint mts[4] = { MOTOR_TYPE_IM, MOTOR_TYPE_PM,
                                 MOTOR_TYPE_VARFREQ, MOTOR_NONE };
    long i;

    for (i = 0; i < 32; i++)
    {
        begin_scenario(10UL + (unsigned long)i);
        gMotorInfo.MotorType = mts[i % 4];
        gMainCmd.Command.bit.ControlMode = (Uint)(i % 4);
        gCtrMotorType = (CONTROL_MOTOR_TYPE_ENUM)(i % 13);
        end_scenario(F_INIT, "InitForMotorApp");
    }
}

/* ---- Main0msMotor -------------------------------------------------------- */

static void phase_main0ms(void)
{
    static const Uint modes[4] = { 0U, 1U, 2U, 0xFFFFU };
    static const Uint types[7] = { PG_TYPE_ABZ, PG_TYPE_UVW, PG_TYPE_RESOLVER,
                                   PG_TYPE_SC, PG_TYPE_SPECIAL_UVW,
                                   PG_TYPE_NULL, PG_TYPE_RT };
    long i, j;
    unsigned long seed = 100UL;

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 7; j++)
        {
            begin_scenario(seed++);
            gPGData.PGMode = modes[i];
            gPGData.PGType = types[j];
            end_scenario(F_MAIN0MS, "Main0msMotor");
        }
    }
}

/* ---- Main05msMotor ------------------------------------------------------- */

static void phase_main05ms(void)
{
    static const Uint ctrs[6] = { ASYNC_SVC, ASYNC_FVC, SYNC_SVC,
                                  DC_CONTROL, RUN_SYNC_TUNE, 0xFFFFU };
    long i;
    unsigned long seed = 200UL;

    for (i = 0; i < 6; i++)
    {
        begin_scenario(seed++);
        gCtrMotorType = (CONTROL_MOTOR_TYPE_ENUM)ctrs[i];
        end_scenario(F_MAIN05MS, "Main05msMotor");
    }
}

/* ---- Main2msMotorA ------------------------------------------------------- */

static void phase_main2msA(void)
{
    static const Uint steps[6] = { STATUS_LOW_POWER, STATUS_GET_PAR, STATUS_STOP,
                                   STATUS_RUN, STATUS_SPEED_CHECK, 0xFFFFU };
    long i, j;
    unsigned long seed = 300UL;

    for (i = 0; i < 6; i++)
    {
        for (j = 0; j < 2; j++)
        {
            begin_scenario(seed++);
            gMainStatus.RunStep = steps[i];
            gMainCmd.Command.bit.Start = (Uint)j;
            gMotorInfo.MotorType = (Uint)(((i + j) & 1) ? MOTOR_TYPE_PM : MOTOR_TYPE_IM);
            end_scenario(F_MAIN2MSA, "Main2msMotorA");
        }
    }
}

/* ---- Main2msMotorB: the branch-heavy one --------------------------------- */
/* Full cross product of the two switches (CtrMotorType 8 x RunStep 8), the
 * MotorType x SvcMode selection, the StartDC/StopDC override, and the five
 * TUNE_STEP_NOW values for RUN_SYNC_TUNE. */

static void phase_main2msB(void)
{
    static const CONTROL_MOTOR_TYPE_ENUM ctrs[8] = {
        ASYNC_SVC, ASYNC_FVC, ASYNC_VF, SYNC_VF,
        SYNC_SVC, SYNC_FVC, DC_CONTROL, RUN_SYNC_TUNE };
    static const Uint steps[8] = { STATUS_LOW_POWER, STATUS_GET_PAR, STATUS_STOP,
        STATUS_SPEED_CHECK, STATUS_RUN, STATUS_SHORT_GND,
        STATUS_IPM_INIT_POS, 0xFFFFU };
    static const Uint mts[2] = { MOTOR_TYPE_IM, MOTOR_TYPE_PM };
    long a, b, c, d, e, t;
    unsigned long seed = 1000UL;

    for (a = 0; a < 8; a++)
    {
        for (b = 0; b < 8; b++)
        {
            for (c = 0; c < 2; c++)
            {
                for (d = 0; d < 2; d++)
                {
                    for (e = 0; e < 3; e++)
                    {
                        long tmax = (ctrs[a] == RUN_SYNC_TUNE) ? 5 : 1;
                        for (t = 0; t < tmax; t++)
                        {
                            begin_scenario(seed++);
                            gCtrMotorType = ctrs[a];
                            gMainStatus.RunStep = steps[b];
                            gMotorInfo.MotorType = mts[c];
                            gVCPar.SvcMode = (Uint)d;
                            gMainCmd.Command.bit.StartDC = (Uint)(e == 1);
                            gMainCmd.Command.bit.StopDC  = (Uint)(e == 2);
                            gMainCmd.Command.bit.TorqueCtl = (Uint)((b + d) & 1);
                            gTestDataReceive.TestData1 = (int)((b + 1) & 1);
                            gGetParVarable.ParEstMstep = (Uint16)t;
                            gMainCmd.Command.bit.ControlMode = (Uint)(a & 3);
                            /* abs(FreqSet)+3 vs m05HzPu: straddle the boundary */
                            gMainCmd.FreqSet = (int)((drnd() & 1UL) ? 0 : 3000);
                            gVFPar.tpLst = (int)((drnd() & 1UL) ? 5 : 4000);
                            gMainCmd.Command.bit.SpeedFlag = (Uint)(drnd() % 3UL);

                            if (steps[b] == STATUS_SPEED_CHECK) g_cov2msB_SpeedCheck++;
                            if (steps[b] == STATUS_RUN) g_cov2msB_m05Clamp++;
                            g_covSvcMode[d & 1]++;
                            if (t < 8) g_covTuneStep[t]++;

                            end_scenario(F_MAIN2MSB, "Main2msMotorB");
                        }
                    }
                }
            }
        }
    }
}

/* ---- Main2msMotorC ------------------------------------------------------- */

static void phase_main2msC(void)
{
    static const Uint ctrs[7] = { ASYNC_SVC, ASYNC_FVC, SYNC_SVC, SYNC_FVC,
                                  RUN_SYNC_TUNE, DC_CONTROL, 0xFFFFU };
    long i;
    unsigned long seed = 3000UL;

    for (i = 0; i < 7; i++)
    {
        begin_scenario(seed++);
        gCtrMotorType = (CONTROL_MOTOR_TYPE_ENUM)ctrs[i];
        end_scenario(F_MAIN2MSC, "Main2msMotorC");
    }
}

/* ---- Main2msMotorD, SendDataPrepare -------------------------------------- */

static void phase_main2msD(void)
{
    static const Uint steps[8] = { STATUS_LOW_POWER, STATUS_STOP, STATUS_RUN,
        STATUS_GET_PAR, STATUS_SPEED_CHECK, STATUS_SHORT_GND,
        STATUS_IPM_INIT_POS, 0xFFFFU };
    long i;
    unsigned long seed = 4000UL;

    for (i = 0; i < 8; i++)
    {
        begin_scenario(seed++);
        gMainStatus.RunStep = steps[i];
        gMotorInfo.MotorType = (Uint)(i & 1) ? MOTOR_TYPE_PM : MOTOR_TYPE_IM;
        end_scenario(F_MAIN2MSD, "Main2msMotorD");
    }
}

static void phase_senddata(void)
{
    static const Uint steps[8] = { STATUS_LOW_POWER, STATUS_STOP, STATUS_RUN,
        STATUS_GET_PAR, STATUS_SPEED_CHECK, STATUS_SHORT_GND,
        STATUS_IPM_INIT_POS, 0xFFFFU };
    static const Uint cbi[7] = { 0U, 79U, 80U, 81U, 82U, 4095U, 6000U };
    static const Uint outoff[2] = { 0U, 1U };
    static const Uint mts[2] = { MOTOR_TYPE_IM, MOTOR_TYPE_PM };
    long a, b, c, d;
    unsigned long seed = 5000UL;

    for (a = 0; a < 8; a++)
    {
        for (b = 0; b < 7; b++)
        {
            for (c = 0; c < 2; c++)
            {
                for (d = 0; d < 2; d++)
                {
                    begin_scenario(seed++);
                    gMainStatus.RunStep = steps[a];
                    gLineCur.CurBaseInv = cbi[b];
                    gMainStatus.StatusWord.bit.OutOff = outoff[c];
                    gMotorInfo.MotorType = mts[d];
                    gAI.aiCounter = (int)drnd_int(1, 1000);
                    gIMTQ12.T = (int)((drnd() & 1UL) ? -3000 : 3000);
                    gLineCur.CurPerFilter = (Ulong)drnd();
                    gLineCur.CurTorque = (int)drnd_int(-5000, 5000);
                    gRatio = (Uint)((drnd() & 1UL) ? 5000 : 100);
                    gUDC.uDC = (Uint)drnd_int(1, 3000);
                    gInvInfo.BaseUdc = (Uint)drnd_int(1, 3000);

                    if (steps[a] == STATUS_LOW_POWER || steps[a] == STATUS_STOP ||
                        cbi[b] < 81U || outoff[c] == 1U)
                    {
                        g_covSendLow++;
                    }
                    else
                    {
                        g_covSendGo++;
                    }
                    end_scenario(F_SENDDATA, "SendDataPrepare");
                }
            }
        }
    }
}

/* ---- RunCaseRun2Ms ------------------------------------------------------- */

static void phase_run2ms(void)
{
    static const CONTROL_MOTOR_TYPE_ENUM ctrs[8] = {
        ASYNC_SVC, ASYNC_FVC, ASYNC_VF, SYNC_VF,
        SYNC_SVC, SYNC_FVC, DC_CONTROL, RUN_SYNC_TUNE };
    static const Uint starts[2] = { 0U, 1U };
    static const Uint flags[3] = { 0U, 1U, 2U };   /* speed_CON/ACC/DEC */
    long a, b, c;
    unsigned long seed = 6000UL;

    for (a = 0; a < 8; a++)
    {
        for (b = 0; b < 2; b++)
        {
            for (c = 0; c < 3; c++)
            {
                begin_scenario(seed++);
                gCtrMotorType = ctrs[a];
                gMainCmd.Command.bit.Start = starts[b];
                gMainCmd.Command.bit.SpeedFlag = flags[c];
                gMainCmd.FreqSet = (int)((drnd() & 1UL) ? -2000 : 2000);
                gVFPar.tpLst = (int)((drnd() & 1UL) ? -1000 : 1000);
                gMainCmd.FreqSyn = (int)drnd_int(-3000, 3000);

                if (ctrs[a] == ASYNC_VF || ctrs[a] == SYNC_VF) g_covRun2msVF++;
                if (ctrs[a] == ASYNC_SVC || ctrs[a] == ASYNC_FVC) g_covRun2msSVC++;
                if (ctrs[a] == SYNC_FVC) g_covRun2msSyncFvc++;
                end_scenario(F_RUN2MS, "RunCaseRun2Ms");
            }
        }
    }
}

/* ---- PrepareParForRun / TurnToStopStatus --------------------------------- */

static void phase_preppar(void)
{
    long i;
    unsigned long seed = 7000UL;

    for (i = 0; i < 8; i++)
    {
        begin_scenario(seed++);
        gMainCmd.Command.bit.ControlMode = (Uint)(i & 3);
        end_scenario(F_PREPPAR, "PrepareParForRun");
    }
}

static void phase_turntostop(void)
{
    long i;
    unsigned long seed = 7100UL;

    for (i = 0; i < 4; i++)
    {
        begin_scenario(seed++);
        gMainStatus.RunStep = (Uint)i;
        end_scenario(F_TURNTSTOP, "TurnToStopStatus");
    }
}

/* ---- RunCaseStop --------------------------------------------------------- */

static void phase_runstop(void)
{
    static const Uint mts[2] = { MOTOR_TYPE_IM, MOTOR_TYPE_PM };
    static const Uint pgts[3] = { PG_TYPE_ABZ, PG_TYPE_RESOLVER, PG_TYPE_NULL };
    long a, b, c, d, e, f, g, h, k;
    unsigned long seed = 8000UL;

    for (a = 0; a < 2; a++)                 /* RunEnable */
    {
        for (b = 0; b < 2; b++)             /* ShortGnd */
        {
            for (c = 0; c < 2; c++)         /* ShortGndOver */
            {
                for (d = 0; d < 2; d++)     /* MotorType */
                {
                    for (e = 0; e < 2; e++) /* Start */
                    {
                        for (f = 0; f < 2; f++)     /* TuneType */
                        {
                            for (g = 0; g < 2; g++) /* SpeedSearch */
                            {
                                for (h = 0; h < 2; h++)     /* IPMInitPos.Flag */
                                {
                                    for (k = 0; k < 3; k++) /* PGType */
                                    {
                                        begin_scenario(seed++);
                                        gMainStatus.StatusWord.bit.RunEnable = (Uint)a;
                                        gExtendCmd.bit.ShortGnd = (Uint)b;
                                        gMainStatus.StatusWord.bit.ShortGndOver = (Uint)c;
                                        gMotorInfo.MotorType = mts[d];
                                        gMainCmd.Command.bit.Start = (Uint)e;
                                        gGetParVarable.TuneType =
                                            (TUNE_FLOW_ENUM)(f ? TUNE_PM_COMP_NO_LOAD : TUNE_NULL);
                                        gExtendCmd.bit.SpeedSearch = (Uint)g;
                                        gIPMInitPos.Flag = (Uint)h;
                                        gPGData.PGType = pgts[k];

                                        if (a == 1) g_covStopRunEnable++;
                                        if (a == 1 && b == 1 && c == 0) g_covStopShortGnd++;
                                        if (a == 1 && b != 1 && e == 1 && f == 1) g_covStopGetPar++;
                                        if (a == 1 && b != 1 && e == 1 && f == 0 && g == 1) g_covStopSpeedCheck++;
                                        if (a == 1 && b != 1 && e == 1 && f == 0 && g == 0 &&
                                            h == 0 && d == 1 && pgts[k] == PG_TYPE_ABZ) g_covStopIpmInit++;
                                        if (a == 1 && b != 1 && e == 1 && f == 0 && g == 0 &&
                                            !(h == 0 && d == 1 && pgts[k] == PG_TYPE_ABZ)) g_covStopRun++;

                                        end_scenario(F_RUNSTOP, "RunCaseStop");
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

/* ---- CalRatioFromVot ----------------------------------------------------- */

static void phase_calratio(void)
{
    static const Uint outoff[2] = { 0U, 1U };
    static const int va[3] = { 0, 287, 32767 };
    static const Uint vot[3] = { 1U, 1000U, 4000U };
    static const Uint iv[2] = { 1U, 3000U };
    static const Uint bu[2] = { 1U, 3000U };
    static const Uint udc[2] = { 1U, 3000U };
    long a, b, c, d, e, f;
    unsigned long seed = 9000UL;

    for (a = 0; a < 2; a++)
    {
        for (b = 0; b < 3; b++)
        {
            for (c = 0; c < 3; c++)
            {
                for (d = 0; d < 2; d++)
                {
                    for (e = 0; e < 2; e++)
                    {
                        for (f = 0; f < 2; f++)
                        {
                            begin_scenario(seed++);
                            gMainStatus.StatusWord.bit.OutOff = outoff[a];
                            gOutVolt.VoltApply = va[b];
                            gMotorInfo.Votage = vot[c];
                            gInvInfo.InvVolt = iv[d];
                            gInvInfo.BaseUdc = bu[e];
                            gUDC.uDC = udc[f];
                            g_covRatioClamp++;
                            end_scenario(F_CALRATIO, "CalRatioFromVot");
                        }
                    }
                }
            }
        }
    }
}

/* ---- ADCOverInterrupt ---------------------------------------------------- */

static void phase_adcover(void)
{
    static const CONTROL_MOTOR_TYPE_ENUM ctrs[8] = {
        ASYNC_SVC, ASYNC_FVC, ASYNC_VF, SYNC_VF,
        SYNC_SVC, SYNC_FVC, DC_CONTROL, RUN_SYNC_TUNE };
    long a, b, i;
    unsigned long seed = 10000UL;

    /* CtrMotorType x SvcMode, resolver PG, over-UDC flag set */
    for (a = 0; a < 8; a++)
    {
        for (b = 0; b < 2; b++)
        {
            begin_scenario(seed++);
            gCtrMotorType = ctrs[a];
            gVCPar.SvcMode = (Uint)b;
            gPGData.PGType = PG_TYPE_RESOLVER;
            gPGData.PGMode = 1U;
            GpioDataRegs.AIODAT.bit.AIO2 = 0U;
            g_covAdcResolver++;
            g_covAdcOverUdc++;
            g_covSvcMode[b & 1]++;
            end_scenario(F_ADCOVER, "ADCOverInterrupt");
        }
    }

    /* RUN_SYNC_TUNE switch arm: pin TUNE_STEP_NOW across the steps the branch
     * tests.  The condition at ref 710-712 matches {NO_LOAD, BEMF, WITH_LOAD}
     * and Q16 swaps WITH_LOAD for POLSE_POS.  The CtrMotorType x SvcMode loop
     * above reaches the arm but leaves the step to the scenario prefill, so
     * WITH_LOAD / POLSE_POS (the two values that separate the versions) were
     * only ever hit by chance -- Q16 was caught by the random phase alone.
     * Driving all four tested steps makes each version's behaviour a
     * deterministic field diff. */
    {
        static const PAR_EST_MAIN_STEP tuneSteps[4] = {
            PM_EST_NO_LOAD, PM_EST_BEMF, PM_EST_WITH_LOAD, PM_EST_POLSE_POS };
        long s;

        for (s = 0; s < 4; s++)
        {
            begin_scenario(seed++);
            gCtrMotorType = RUN_SYNC_TUNE;
            gGetParVarable.ParEstMstep = (Uint16)0;
            gGetParVarable.ParEstContent[0] = tuneSteps[s];
            end_scenario(F_ADCOVER, "ADCOverInterrupt");
        }
    }

    /* DeadCompMode 280 / 380 / other */
    for (a = 0; a < 3; a++)
    {
        begin_scenario(seed++);
        gExtendCmd.bit.DeadCompMode = (Uint)a;
        if (a == 1) g_covAdcDead280++;
        if (a == 2) g_covAdcDead380++;
        end_scenario(F_ADCOVER, "ADCOverInterrupt");
    }

    /* SynInitPosDetect: RunStep in {GET_PAR, IPM_INIT_POS}, PM, Step != 0 */
    {
        static const Uint rs[2] = { STATUS_GET_PAR, STATUS_IPM_INIT_POS };
        static const Uint istep[3] = { 0U, 1U, 2U };
        long j;

        for (i = 0; i < 2; i++)
        {
            for (j = 0; j < 3; j++)
            {
                begin_scenario(seed++);
                gMainStatus.RunStep = rs[i];
                gMotorInfo.MotorType = MOTOR_TYPE_PM;
                gIPMInitPos.Step = istep[j];
                g_covAdcSynInit++;
                end_scenario(F_ADCOVER, "ADCOverInterrupt");
            }
        }
    }

    /* UVW-phase branch: GET_PAR, TUNE == PM_EST_WITH_LOAD, UvwEstStep sweep */
    for (i = 0; i < 4; i++)
    {
        begin_scenario(seed++);
        gMainStatus.RunStep = STATUS_GET_PAR;
        gMotorInfo.MotorType = MOTOR_TYPE_IM;
        gGetParVarable.ParEstMstep = (Uint16)0;
        gGetParVarable.ParEstContent[0] = PM_EST_WITH_LOAD;
        gUVWPG.UvwEstStep = (Uint)i;
        gUVWPG.UVWAngle = (Uint)drnd_int(0, 65535);
        gIPMPos.RotorPos = (Uint)drnd_int(0, 65535);
        g_covAdcUvwPhase++;
        end_scenario(F_ADCOVER, "ADCOverInterrupt");
    }

    /* LmIo branch: GET_PAR, MotorType != PM, TUNE != WITH_LOAD */
    for (i = 0; i < 2; i++)
    {
        begin_scenario(seed++);
        gMainStatus.RunStep = STATUS_GET_PAR;
        gMotorInfo.MotorType = (Uint)(i ? MOTOR_TYPE_IM : MOTOR_TYPE_VARFREQ);
        gGetParVarable.ParEstMstep = (Uint16)0;
        gGetParVarable.ParEstContent[0] = (PAR_EST_MAIN_STEP)0;
        gIPMInitPos.Step = 0U;
        g_covAdcLmIo++;
        end_scenario(F_ADCOVER, "ADCOverInterrupt");
    }

    /* tail: PGMode 0 (GetMDetaPos/GetMTTimeNum) vs resolver (RT_SAMPLE_END) */
    for (i = 0; i < 3; i++)
    {
        begin_scenario(seed++);
        gPGData.PGMode = (Uint)(i == 0 ? 0 : 1);
        gPGData.PGType = (i == 2) ? PG_TYPE_RESOLVER : PG_TYPE_ABZ;
        end_scenario(F_ADCOVER, "ADCOverInterrupt");
    }
}

/* ---- PWMZeroInterrupt ---------------------------------------------------- */

static void phase_pwmzero(void)
{
    static const Uint pd[2] = { 0U, 1U };
    static const Uint pm[2] = { 0U, 1U };
    static const Uint pt[3] = { PG_TYPE_ABZ, PG_TYPE_RESOLVER, PG_TYPE_NULL };
    long a, b, c;
    unsigned long seed = 11000UL;

    for (a = 0; a < 2; a++)
    {
        for (b = 0; b < 2; b++)
        {
            for (c = 0; c < 3; c++)
            {
                begin_scenario(seed++);
                gMainStatus.PrgStatus.bit.PWMDisable = pd[a];
                gPGData.PGMode = pm[b];
                gPGData.PGType = pt[c];
                if (pd[a] == 1U) g_covPwmDisable++;
                else             g_covPwmOut++;
                end_scenario(F_PWMZERO, "PWMZeroInterrupt");
            }
        }
    }
}

/* ---- edge probes --------------------------------------------------------- */
/* The enumerated phases sweep the branches, but a handful of decisions turn on
 * an exact boundary: a strict inequality whose two sides differ by one, a clamp
 * whose threshold is only reachable on a specific input, or a ternary whose
 * false arm only shows when its condition is exactly zero.  Those cases are
 * spelled out here so a `<` -> `<=`, a shifted constant or a swapped arm is
 * visible deterministically instead of only through the random net. */

static void phase_edges(void)
{
    static const int fsets[5] = { 1634, 1635, 1000, -3000, 0 };
    static const Uint tc[3] = { 0U, 1U, 0U };   /* TorqueCtl    */
    static const int  td[3] = { 0, 0, 1 };      /* TestData1    */
    static const int  slast[3] = { 0, 1, -1 };  /* SpeedLast    */
    long i, j;

    /* (a) ASYNC_SVC / SvcMode 0 clamp: with FullFreq01 = 1000 the m05HzPu
     *     threshold is exactly 1638, so 1634 clamps and 1635 does not. */
    for (i = 0; i < 5; i++)
    {
        begin_scenario(20000UL + (unsigned long)i);
        gCtrMotorType = ASYNC_SVC;
        gMainStatus.RunStep = STATUS_RUN;
        gVCPar.SvcMode = 0U;
        gBasePar.FullFreq01 = 1000UL;
        gMainCmd.FreqSet = fsets[i];
        gMainCmd.FreqSetApply = 1234;
        g_covEdgeClamp++;
        end_scenario(F_MAIN2MSB, "edge");
    }

    /* (b) SvcMode 1 (the _380 arm): the (0,0) row reaches the identical clamp on
     *     the same boundary; the (1,0) and (0,1) rows reach the empty arm through
     *     each side of the `||` short-circuit. */
    for (i = 0; i < 5; i++)
    {
        for (j = 0; j < 3; j++)
        {
            begin_scenario(21000UL + (unsigned long)(i * 3 + j));
            gCtrMotorType = ASYNC_SVC;
            gMainStatus.RunStep = STATUS_RUN;
            gVCPar.SvcMode = 1U;
            gBasePar.FullFreq01 = 1000UL;
            gMainCmd.FreqSet = fsets[i];
            gMainCmd.FreqSetApply = 1234;
            gMainCmd.Command.bit.TorqueCtl = tc[j];
            gTestDataReceive.TestData1 = td[j];
            g_covEdge380++;
            end_scenario(F_MAIN2MSB, "edge");
        }
    }

    /* (c) SYNC_VF speed-check arm: the sign test on SpeedLast selects
     *     VoltPhaseApply 16384 vs -16384; 0 sits on the false side. */
    for (i = 0; i < 3; i++)
    {
        begin_scenario(22000UL + (unsigned long)i);
        gCtrMotorType = SYNC_VF;
        gMainStatus.RunStep = STATUS_SPEED_CHECK;
        gFeisu.SpeedLast = slast[i];
        g_covEdgePhase++;
        end_scenario(F_MAIN2MSB, "edge");
    }

    /* (d) an unmapped CtrMotorType lands in the switch default arms. */
    for (i = 0; i < 2; i++)
    {
        begin_scenario(23000UL + (unsigned long)i);
        gCtrMotorType = (CONTROL_MOTOR_TYPE_ENUM)0xFFFFU;
        gMainStatus.RunStep = (i == 0) ? STATUS_RUN : STATUS_SPEED_CHECK;
        g_covEdgeDefault++;
        end_scenario(F_MAIN2MSB, "edge");
    }

    /* (e) FreqSyn = 0 drives the SpeedLast ternary in RunCaseRun2Ms onto its
     *     false arm, which the enumerated FreqSet/FreqSyn draws never pin. */
    begin_scenario(23100UL);
    gCtrMotorType = ASYNC_SVC;
    gMainStatus.RunStep = STATUS_RUN;
    gMainCmd.Command.bit.Start = 1U;
    gMainCmd.FreqSyn = 0;
    gFeisu.SpeedLast = 1234;
    g_covEdgeDefault++;
    end_scenario(F_MAIN2MSB, "edge");
}

/* ---- random -------------------------------------------------------------- */
/* The broad safety net: a mix of entry points and raw enum/flag values that
 * the hand-built phases do not enumerate.  Every divisor is re-pinned, so no
 * draw can turn a division into a crash. */

static void phase_random(long count)
{
    long c;

    for (c = 0; c < count; c++)
    {
        long fn = (long)(rnd() % (unsigned long)F_COUNT);
        unsigned long seed = 1000000UL + (unsigned long)c;

        begin_scenario(seed);
        pin_divisors();

        GpioDataRegs.AIODAT.bit.AIO2 = (Uint16)(rnd() & 1UL);
        gCtrMotorType = (CONTROL_MOTOR_TYPE_ENUM)(rnd() % 22UL);
        gMainStatus.RunStep = (Uint)(rnd() % 9UL);
        gExtendCmd.all = (Uint)(rnd() & 0xFFFFUL);
        gMainCmd.Command.all = (Uint)(rnd() & 0x01FFUL);
        gMainCmd.Command.bit.ControlMode = (Uint)(rnd() % 4UL);
        gGetParVarable.ParEstMstep = (Uint16)(rnd() % 5UL);
        gMotorInfo.MotorType = (Uint)(rnd() % 4UL);
        gVCPar.SvcMode = (Uint)(rnd() & 1UL);
        gIPMInitPos.Flag = (Uint)(rnd() & 1UL);
        gIPMInitPos.Step = (Uint)(rnd() % 4UL);
        gPGData.PGMode = (Uint)(rnd() % 3UL);
        gPGData.PGType = (Uint)(rnd() % 7UL);

        /* belt and braces: the pins must survive the re-randomisation above */
        if (gBasePar.FullFreq01 == 0UL) gBasePar.FullFreq01 = 3000UL;
        if (gAI.aiCounter == 0) gAI.aiCounter = 1;
        if (gInvInfo.BaseUdc == 0U) gInvInfo.BaseUdc = 1000U;
        if (gInvInfo.InvVolt == 0U) gInvInfo.InvVolt = 1000U;
        if (gUDC.uDC == 0U) gUDC.uDC = 1000U;

        end_scenario((int)fn, "random");
    }
}

/* ------------------------------------------------------------------------- */
/* main                                                                     */
/* ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long random = (argc > 1) ? atol(argv[1]) : 20000L;
    long i;

    g_rng = 0x12345678UL;

    /* Width pins: the case shim folds `int` to the target's 16 bits so that
     * `Uint`/`int` storage matches the C28x.  If that fold is ever lost,
     * host-only 32-bit storage silently hides mutations, so fail loudly. */
    if (sizeof(Uint) != 2U || sizeof(int) != 2U || sizeof(long) != 4U ||
        sizeof(Uint16) != 2U || sizeof(Uint32) != 4U || sizeof(Ulong) != 4U ||
        sizeof(llong) != 8U)
    {
        printf("FATAL: host type widths differ from the target "
               "(Uint=%u int=%u long=%u llong=%u)\n",
               (unsigned)sizeof(Uint), (unsigned)sizeof(int),
               (unsigned)sizeof(long), (unsigned)sizeof(llong));
        return 2;
    }

    /* Layout pins: the driver pokes DisableDrive/EnableDrive (GPASET/GPACLEAR
     * GPIO7), GetOverUdcFlag (AIODAT AIO2) and the resolver path (GPADAT
     * GPIO23, AIODAT AIO12) directly.  A header change that moved those bits
     * would silently re-target the pokes, so assert where they are. */
    {
        volatile struct GPIO_DATA_REGS probe;

        memset((void *)&probe, 0, sizeof(probe));
        probe.GPASET.bit.GPIO7 = 1;
        if ((probe.GPASET.all & 0x00000080UL) == 0UL)
        {
            printf("FATAL: GPASET.GPIO7 is not bit 7 (all=0x%08lX)\n",
                   (unsigned long)probe.GPASET.all);
            return 2;
        }
        memset((void *)&probe, 0, sizeof(probe));
        probe.AIODAT.bit.AIO2 = 1;
        if ((probe.AIODAT.all & 0x00000004UL) == 0UL)
        {
            printf("FATAL: AIODAT.AIO2 is not bit 2 (all=0x%08lX)\n",
                   (unsigned long)probe.AIODAT.all);
            return 2;
        }
        memset((void *)&probe, 0, sizeof(probe));
        probe.GPADAT.bit.GPIO23 = 1;
        if ((probe.GPADAT.all & 0x00800000UL) == 0UL)
        {
            printf("FATAL: GPADAT.GPIO23 is not bit 23 (all=0x%08lX)\n",
                   (unsigned long)probe.GPADAT.all);
            return 2;
        }
        memset((void *)&probe, 0, sizeof(probe));
        probe.AIODAT.bit.AIO12 = 1;
        if ((probe.AIODAT.all & 0x00001000UL) == 0UL)
        {
            printf("FATAL: AIODAT.AIO12 is not bit 12 (all=0x%08lX)\n",
                   (unsigned long)probe.AIODAT.all);
            return 2;
        }
    }

    phase_init();
    phase_main0ms();
    phase_main05ms();
    phase_main2msA();
    phase_main2msB();
    phase_main2msC();
    phase_main2msD();
    phase_senddata();
    phase_run2ms();
    phase_preppar();
    phase_turntostop();
    phase_runstop();
    phase_calratio();
    phase_adcover();
    phase_pwmzero();
    phase_edges();
    phase_random(random);

    printf("MotorMain difftest: cases=%ld mismatches=%ld\n", g_cases, g_mism);
    printf("input reach: fn[]=%ld/%ld/%ld/%ld/%ld/%ld/%ld/%ld/%ld/%ld/%ld/%ld/%ld/%ld/%ld "
           "svc0=%ld svc1=%ld speedcheck=%ld sendLow=%ld sendGo=%ld "
           "stopEn=%ld shortGnd=%ld getPar=%ld spdChk=%ld ipmInit=%ld run=%ld "
           "adcRes=%ld adcOv=%ld dead280=%ld dead380=%ld synInit=%ld uvw=%ld lmio=%ld "
           "pwmDis=%ld pwmOut=%ld ratio=%ld cpuctl=%ld\n",
           g_covFn[0], g_covFn[1], g_covFn[2], g_covFn[3], g_covFn[4],
           g_covFn[5], g_covFn[6], g_covFn[7], g_covFn[8], g_covFn[9],
           g_covFn[10], g_covFn[11], g_covFn[12], g_covFn[13], g_covFn[14],
           g_covSvcMode[0], g_covSvcMode[1], g_cov2msB_SpeedCheck,
           g_covSendLow, g_covSendGo,
           g_covStopRunEnable, g_covStopShortGnd, g_covStopGetPar,
           g_covStopSpeedCheck, g_covStopIpmInit, g_covStopRun,
           g_covAdcResolver, g_covAdcOverUdc, g_covAdcDead280, g_covAdcDead380,
           g_covAdcSynInit, g_covAdcUvwPhase, g_covAdcLmIo,
           g_covPwmDisable, g_covPwmOut, g_covRatioClamp, g_covCpuCtl);
    printf("edge reach: clamp=%ld edge380=%ld phase=%ld default=%ld\n",
           g_covEdgeClamp, g_covEdge380, g_covEdgePhase, g_covEdgeDefault);

    /* Every entry point must have been driven. */
    for (i = 0; i < F_COUNT; i++)
    {
        if (g_covFn[i] == 0)
        {
            printf("FATAL: entry point %ld was never driven\n", i);
            return 2;
        }
    }
    /* Every branch the hand-built phases exist to reach must have been reached. */
    if (g_covSvcMode[0] == 0 || g_covSvcMode[1] == 0 ||
        g_cov2msB_SpeedCheck == 0 ||
        g_covRun2msVF == 0 || g_covRun2msSVC == 0 || g_covRun2msSyncFvc == 0 ||
        g_covSendLow == 0 || g_covSendGo == 0 ||
        g_covStopRunEnable == 0 || g_covStopShortGnd == 0 ||
        g_covStopGetPar == 0 || g_covStopSpeedCheck == 0 ||
        g_covStopIpmInit == 0 || g_covStopRun == 0 ||
        g_covAdcResolver == 0 || g_covAdcOverUdc == 0 || g_covAdcDead280 == 0 ||
        g_covAdcDead380 == 0 || g_covAdcSynInit == 0 ||
        g_covAdcUvwPhase == 0 || g_covAdcLmIo == 0 ||
        g_covPwmDisable == 0 || g_covPwmOut == 0)
    {
        printf("FATAL: a phase branch was never reached as an input\n");
        return 2;
    }
    if (g_covCpuCtl == 0)
    {
        printf("FATAL: the DINT/EINT pair in SendDataPrepare never ran\n");
        return 2;
    }
    if (g_covEdgeClamp == 0 || g_covEdge380 == 0 ||
        g_covEdgePhase == 0 || g_covEdgeDefault == 0)
    {
        printf("FATAL: an edge probe was never reached\n");
        return 2;
    }

    return g_mism == 0 ? 0 : 1;
}

