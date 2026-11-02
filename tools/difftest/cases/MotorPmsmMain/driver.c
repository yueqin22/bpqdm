/* Differential-test driver for MotorPmsmMain.
 *
 * The frozen baseline (cases/MotorPmsmMain/ref/MotorPmsmMain_ref.c) and the
 * delivered source (src/motor/MotorPmsmMain.c) are compiled into two symbol
 * namespaces, ref_* and new_*.  The module owns five data objects of its own
 * (gIPMPos / gFluxWeak / gPMInitPos / gIPMPosCheck / gPmDecoup -- `B` symbols
 * in build/Debug/MotorPmsmMain.obj), which are per-side private memory and
 * renamed; the 21 globals it reads/writes but does not define are ONE shared
 * instance supplied by mock_pmsmmain.c.
 *
 * ---------------------------------------------------------------------------
 * What is compared, per call
 * ---------------------------------------------------------------------------
 *   - the five per-side objects, whole struct, byte for byte;
 *   - the 21 shared globals, whole struct, byte for byte (including the ones
 *     the module is only supposed to READ -- an unexpected write is a bug too);
 *   - EQepRegs / PieCtrlRegs / GpioDataRegs, whole register images;
 *   - gHostCpuCtl[7], the DINT/EINT and EALLOW/EDIS pairs;
 *   - gMockCall[] / gMockArg[]: how many times each callee was called and with
 *     what argument.
 *
 * The last one matters more here than usual.  Four of the ten entry points
 * express their answer ONLY by calling another module:
 *     PG_Zero_isr       -> SetIPMPos(gIPMPos.RotorZero)
 *     IPMCheckInitPos   -> SetIPMPos(uvwPos) / SetIPMPos_ABZRef(uvwPos)
 *     RunCaseIpmInitPos -> SynInitPosDetSetPwm(6)
 * A stub that ignored its argument would make "the module computed the wrong
 * position" completely invisible.
 *
 * ---------------------------------------------------------------------------
 * Division is the input-space hazard, not a style point
 * ---------------------------------------------------------------------------
 * Six expressions take a field as a divisor, and a zero there does not produce
 * a "mismatch" -- it kills the process (MSVC #DE) and takes the whole ledger
 * with it:
 *     SynCalRealPos    QepTotal % (PulseNum<<2)   and  / (int)PulseNum
 *     SynCalLdAndLq    / (m_Cos1 - m_Cos2)        -- depends on the argument
 *     IPMCalAcrPIDCoff / gMotorInfo.Votage        (twice)
 *     PrepPmsmCsrPrar  / (32767 - gMotorInfo.FreqPer)   in the else arm
 *     PmFluxWeakDeal   / gMotorExtPer.LD  and  / gFluxWeak.AbsFrqLpf
 * pin_inputs() fixes every one of them to a non-zero value, and
 * SynCalLdAndLq's divisor is not pinnable at all (it is a function of the
 * argument), so phase_ldlq() computes the same denominator the module will
 * compute and SKIPS the argument when it is zero, counting the skips.  On the
 * target a division by zero does not trap, so the skipped arguments are a
 * genuine host-only coverage gap, recorded in the case README.
 *
 * ---------------------------------------------------------------------------
 * 0xA5 discipline
 * ---------------------------------------------------------------------------
 * Every per-side field the module only WRITES starts at 0xA5, so a rewrite that
 * stops writing it is visible instead of being masked by a pre-existing equal
 * value.  Every field the module READS is set to a meaningful input value by
 * the scenario.  The distinction is made per field, not per struct -- gIPMPos
 * alone has both kinds side by side.
 */

#include <stdio.h>
#include <string.h>

/* <stdlib.h> is deliberately NOT included: the case shim folds `int` to the
 * target's 16 bits and stdlib's 32-bit prototypes would then collide with the
 * project's folded ones.  Only atol is needed, declared by hand. */
extern long atol(const char *);

#include "MotorInclude.h"
#include "mock_calls.h"

/* ---- renamed entry points ------------------------------------------------ */

extern void ref_PG_Zero_isr(void);
extern void ref_IPMCheckInitPos(void);
extern void ref_PmChkInitPosRest(void);
extern void ref_RunCaseIpmInitPos(void);
extern void ref_SynCalRealPos(void);
extern void ref_SynCalLdAndLq(Uint m_Pos);
extern void ref_IPMCalAcrPIDCoff(void);
extern void ref_PrepPmsmCsrPrar(void);
extern void ref_PmFluxWeakDeal(void);
extern void ref_PmDecoupleDeal(void);

extern void new_PG_Zero_isr(void);
extern void new_IPMCheckInitPos(void);
extern void new_PmChkInitPosRest(void);
extern void new_RunCaseIpmInitPos(void);
extern void new_SynCalRealPos(void);
extern void new_SynCalLdAndLq(Uint m_Pos);
extern void new_IPMCalAcrPIDCoff(void);
extern void new_PrepPmsmCsrPrar(void);
extern void new_PmFluxWeakDeal(void);
extern void new_PmDecoupleDeal(void);

/* ---- renamed per-side state --------------------------------------------- */

extern IPM_POSITION_STRUCT   ref_gIPMPos;
extern PM_FLUX_WEAK          ref_gFluxWeak;
extern PM_INIT_POSITION      ref_gPMInitPos;
extern IPM_POS_CHECK_STRUCT  ref_gIPMPosCheck;
extern PM_DECOUPLE           ref_gPmDecoup;

extern IPM_POSITION_STRUCT   new_gIPMPos;
extern PM_FLUX_WEAK          new_gFluxWeak;
extern PM_INIT_POSITION      new_gPMInitPos;
extern IPM_POS_CHECK_STRUCT  new_gIPMPosCheck;
extern PM_DECOUPLE           new_gPmDecoup;

/* ---- mock-owned --------------------------------------------------------- */

extern struct EQEP_REGS               *EQepRegs;
extern volatile struct PIE_CTRL_REGS   PieCtrlRegs;
extern volatile struct GPIO_DATA_REGS  GpioDataRegs;
extern volatile Uint16                 gHostCpuCtl[7];
extern unsigned short                  gMockCall[MC_COUNT];
extern unsigned long                   gMockArg[MC_COUNT];

/* ---- the images --------------------------------------------------------- */
/* X(name, type) must stay a macro so the struct, the load, the capture and the
 * compare are all generated from one list. */

#define ST_LIST(X)                                                         \
    X(gIPMPos,      IPM_POSITION_STRUCT)                                   \
    X(gFluxWeak,    PM_FLUX_WEAK)                                          \
    X(gPMInitPos,   PM_INIT_POSITION)                                      \
    X(gIPMPosCheck, IPM_POS_CHECK_STRUCT)                                  \
    X(gPmDecoup,    PM_DECOUPLE)

#define SH_LIST(X)                                                         \
    X(gMainCmd,       BASE_COMMAND_STRUCT)                                 \
    X(gMainStatus,    RUN_STATUS_STRUCT)                                   \
    X(gBasePar,       BASE_PAR_STRUCT)                                     \
    X(gVCPar,         VC_INFO_STRUCT)                                      \
    X(gMotorInfo,     MOTOR_STRUCT)                                        \
    X(gMotorExtInfo,  MOTOR_EXTERN_STRUCT)                                 \
    X(gMotorExtPer,   MOTOR_EXTERN_STRUCT)                                 \
    X(gMotorExtReg,   MOTOR_EXTERN_STRUCT)                                 \
    X(gOutVolt,       OUT_VOLT_STRUCT)                                     \
    X(gIMTQ24,        MT_STRUCT_Q24)                                       \
    X(gIMTSet,        MT_STRUCT_Q24)                                       \
    X(gImAcrQ24,      PID32_STRUCT)                                        \
    X(gItAcrQ24,      PID32_STRUCT)                                        \
    X(gPGData,        PG_DATA_STRUCT)                                      \
    X(gFVCSpeed,      FVC_SPEED_STRUCT)                                    \
    X(gGetParVarable, MOTOR_PARA_EST)                                      \
    X(gIPMZero,       IPM_ZERO_POS_STRUCT)                                 \
    X(gUVWPG,         IPM_UVW_PG_STRUCT)                                   \
    X(gIPMInitPos,    IPM_INITPOS_PULSE_STR)                               \
    X(gPmParEst,      PMSM_EST_PARAM_DATA)                                 \
    X(gError,         struct FAULT_CODE_INFOR_STRUCT_DEF)

struct IMAGE {
#define IMG_MEM(name, type) type name;
    ST_LIST(IMG_MEM)
    SH_LIST(IMG_MEM)
#undef IMG_MEM
    struct EQEP_REGS       eqep;
    struct PIE_CTRL_REGS   pie;
    struct GPIO_DATA_REGS  gpio;
};

/* One snapshot of everything that is compared. */
struct SNAP {
    struct IMAGE img;
    Uint16       cpuctl[7];
    unsigned short call[MC_COUNT];
    unsigned long  arg[MC_COUNT];
};

static struct IMAGE imgIn;      /* the input image; reloaded before each side */
static struct SNAP  snapRef;
static struct SNAP  snapNew;

static long g_cases;
static long g_fail;
static int  g_stop;
static int  g_quiet;

/* ---- reach ledger -------------------------------------------------------- */
/* One slot per ARM the module can take, incremented from the driver when the
 * input combination that selects that arm is fed in.  This is an INPUT-side
 * count, not an executed-line count: the module is not instrumented, so the
 * driver cannot know which line ran.  What it does prove is that the input
 * class each arm depends on was actually presented, and -- after the rewrite --
 * that the same classes were presented to both sides.  An arm whose count is 0
 * is an arm this bench never opened, and that is worth knowing before anyone
 * claims it is covered. */

enum REACH_SLOT {
    R_Z_GUARD = 0,          /* zFilterCnt != 0 -> early return              */
    R_Z_ABZ,                /* RunStep != GET_PAR, PGType == ABZ            */
    R_Z_UVW,                /* RunStep != GET_PAR, PGType == UVW            */
    R_Z_OTHERPG,            /* RunStep != GET_PAR, other PGType             */
    R_Z_TUNE_NOLOAD,        /* RunStep == GET_PAR, TUNE_STEP_NOW == NO_LOAD */
    R_Z_TUNE_WITHLOAD,      /* RunStep == GET_PAR, == WITH_LOAD             */
    R_Z_TUNE_OTHER,         /* RunStep == GET_PAR, any other tune step      */
    R_C_FIRST,              /* ParaCalTimes == 0                            */
    R_C_EARLY,              /* PGType != UVW -> return                      */
    R_C_LOW_HIGH,           /* |SpeedTemp| < 100, Cnt > 128                 */
    R_C_LOW_ZERO,           /* |SpeedTemp| < 100, Cnt == 0                  */
    R_C_LOW_MID,            /* |SpeedTemp| < 100, 0 < Cnt <= 128            */
    R_C_NOTLOW,             /* |SpeedTemp| >= 100                           */
    R_R_EARLY,              /* RunCaseIpmInitPos early return               */
    R_R_CASE1, R_R_CASE2, R_R_CASE3, R_R_CASE4, R_R_DEFAULT,
    R_P_ABZ, R_P_UVW,       /* SynCalRealPos: the ABZ_QepTotal block        */
    R_L_RAN,                /* SynCalLdAndLq non-zero divisor               */
    R_D_RAN,                /* IPMCalAcrPIDCoff                             */
    R_PREP_SIMPLE,          /* AbsFrqLpf <= FreqPer                         */
    R_PREP_GAIN,            /* AbsFrqLpf >  FreqPer                         */
    R_F_MODE2,              /* PmFluxWeakDeal Mode == 2                     */
    R_F_OTHER,              /* PmFluxWeakDeal Mode != 2                     */
    R_COUNT
};

static long g_reach[R_COUNT];

static void reach_dump(void)
{
    static const char *name[R_COUNT] = {
        "zGuard", "zAbz", "zUvw", "zOtherPg", "zTuneNoLoad", "zTuneWithLoad",
        "zTuneOther", "cFirst", "cEarly", "cLowHigh", "cLowZero", "cLowMid",
        "cNotLow", "rEarly", "rCase1", "rCase2", "rCase3", "rCase4", "rDefault",
        "pAbz", "pUvw", "lRan", "dRan", "prepSimple", "prepGain",
        "fMode2", "fOther"
    };
    int i;

    printf("reach:");
    for (i = 0; i < R_COUNT; i++)
    {
        printf(" %s=%ld", name[i], g_reach[i]);
    }
    printf("\n");
}

/* ---- load / capture ----------------------------------------------------- */

static void img_load(const struct IMAGE *in)
{
#define ST_LOAD(name, type)  ref_##name = in->name; new_##name = in->name;
    ST_LIST(ST_LOAD)
#undef ST_LOAD
#define SH_LOAD(name, type)  name = in->name;
    SH_LIST(SH_LOAD)
#undef SH_LOAD

    memcpy((void *)EQepRegs,       &in->eqep, sizeof in->eqep);
    memcpy((void *)&PieCtrlRegs,   &in->pie,  sizeof in->pie);
    memcpy((void *)&GpioDataRegs,  &in->gpio, sizeof in->gpio);

    memset((void *)gHostCpuCtl, 0, sizeof(Uint16) * 7);
    memset(gMockCall, 0, sizeof gMockCall);
    memset(gMockArg,  0, sizeof gMockArg);
}

static void capture(struct SNAP *s, int isref)
{
    if (isref)
    {
#define ST_SAVE_REF(name, type) s->img.name = ref_##name;
        ST_LIST(ST_SAVE_REF)
#undef ST_SAVE_REF
    }
    else
    {
#define ST_SAVE_NEW(name, type) s->img.name = new_##name;
        ST_LIST(ST_SAVE_NEW)
#undef ST_SAVE_NEW
    }
#define SH_SAVE(name, type) s->img.name = name;
    SH_LIST(SH_SAVE)
#undef SH_SAVE

    memcpy(&s->img.eqep, (const void *)EQepRegs,      sizeof s->img.eqep);
    memcpy(&s->img.pie,  (const void *)&PieCtrlRegs,  sizeof s->img.pie);
    memcpy(&s->img.gpio, (const void *)&GpioDataRegs, sizeof s->img.gpio);

    memcpy(s->cpuctl, (const void *)gHostCpuCtl, sizeof s->cpuctl);
    memcpy(s->call, gMockCall, sizeof s->call);
    memcpy(s->arg,  gMockArg,  sizeof s->arg);
}

/* ---- compare ------------------------------------------------------------ */

static int cmp_block(const void *a, const void *b, size_t n,
                     const char *what, const char *which)
{
    if (memcmp(a, b, n) != 0)
    {
        if (!g_quiet)
        {
            printf("MISMATCH %s: %s differs\n", what, which);
        }
        return 1;
    }
    return 0;
}

static int cmp_snap(const struct SNAP *a, const struct SNAP *b, const char *what)
{
#define CMP_ONE(name, type)                                                \
    if (cmp_block(&a->img.name, &b->img.name, sizeof a->img.name,          \
                  what, #name)) return 1;
    ST_LIST(CMP_ONE)
    SH_LIST(CMP_ONE)
#undef CMP_ONE

    if (cmp_block(&a->img.eqep, &b->img.eqep, sizeof a->img.eqep, what, "EQepRegs"))  return 1;
    if (cmp_block(&a->img.pie,  &b->img.pie,  sizeof a->img.pie,  what, "PieCtrlRegs")) return 1;
    if (cmp_block(&a->img.gpio, &b->img.gpio, sizeof a->img.gpio, what, "GpioDataRegs")) return 1;
    if (cmp_block(a->cpuctl, b->cpuctl, sizeof a->cpuctl, what, "gHostCpuCtl")) return 1;
    if (cmp_block(a->call,   b->call,   sizeof a->call,   what, "gMockCall")) return 1;
    if (cmp_block(a->arg,    b->arg,    sizeof a->arg,    what, "gMockArg")) return 1;
    return 0;
}

/* ---- one case ----------------------------------------------------------- */

enum ENTRY {
    E_ZERO, E_CHKPOS, E_REST, E_RUNCASE, E_REALPOS, E_LDLQ,
    E_PID, E_PREP, E_FLUXWEAK, E_DECOUPLE
};

static void run_side(int isref, enum ENTRY e, Uint arg)
{
    switch (e)
    {
    case E_ZERO:     if (isref) ref_PG_Zero_isr();        else new_PG_Zero_isr();        break;
    case E_CHKPOS:   if (isref) ref_IPMCheckInitPos();    else new_IPMCheckInitPos();    break;
    case E_REST:     if (isref) ref_PmChkInitPosRest();   else new_PmChkInitPosRest();   break;
    case E_RUNCASE:  if (isref) ref_RunCaseIpmInitPos();  else new_RunCaseIpmInitPos();  break;
    case E_REALPOS:  if (isref) ref_SynCalRealPos();      else new_SynCalRealPos();      break;
    case E_LDLQ:     if (isref) ref_SynCalLdAndLq(arg);   else new_SynCalLdAndLq(arg);   break;
    case E_PID:      if (isref) ref_IPMCalAcrPIDCoff();   else new_IPMCalAcrPIDCoff();   break;
    case E_PREP:     if (isref) ref_PrepPmsmCsrPrar();    else new_PrepPmsmCsrPrar();    break;
    case E_FLUXWEAK: if (isref) ref_PmFluxWeakDeal();     else new_PmFluxWeakDeal();     break;
    case E_DECOUPLE: if (isref) ref_PmDecoupleDeal();     else new_PmDecoupleDeal();     break;
    default: break;
    }
}

static void one(enum ENTRY e, Uint arg, const char *what)
{
    if (g_stop)
    {
        return;
    }

    g_cases++;

    img_load(&imgIn);
    run_side(1, e, arg);
    capture(&snapRef, 1);

    img_load(&imgIn);
    run_side(0, e, arg);
    capture(&snapNew, 0);

    if (cmp_snap(&snapRef, &snapNew, what) != 0)
    {
        g_fail++;
        if (g_fail > 20)
        {
            printf("FATAL: too many mismatches, stopping\n");
            /* Not exit(1): main() distinguishes rc 1 (mismatches) from rc 2
             * (bench self-check) and rc 3 (a branch never reached), so "gave
             * up early" has to stay inside the mismatch class. */
            g_stop = 1;
        }
    }
}

/* ---- input defaults ----------------------------------------------------- */

static void pin_inputs(struct IMAGE *in)
{
    /* Divisors.  See the header comment. */
    in->gPGData.PulseNum      = 2500;      /* % and / in SynCalRealPos      */
    in->gMotorInfo.Votage     = 540;       /* / in IPMCalAcrPIDCoff (x2)    */
    in->gMotorInfo.FreqPer    = 100;       /* 32767 - FreqPer != 0          */
    in->gMotorExtPer.LD       = 1200;      /* / in PmFluxWeakDeal           */
    in->gMotorExtPer.LQ       = 1500;
    in->gMotorExtPer.FluxRotor = 900;

    /* GetQepCnt() is (*EQepRegs).QPOSCNT -- a plain word here. */
    in->eqep.QPOSCNT = 0;
}

static void setup_defaults(struct IMAGE *in)
{
    /* 0xA5 everywhere first: every field the module only writes then differs
     * from the "did not write" state, on both sides equally. */
    memset(in, 0xA5, sizeof *in);

    /* Fields the module READS get real values.  Anything not listed here keeps
     * 0xA5A5, which is a legal input and a legal "not written" marker. */

    /* PG_Zero_isr */
    in->gIPMZero.zFilterCnt        = 0;
    in->gIPMZero.Flag              = 0;
    in->gMainStatus.RunStep        = STATUS_RUN;
    in->gPGData.PGType             = PG_TYPE_UVW;
    in->gIPMPos.RotorZero          = 1234;
    in->gIPMPos.ABZ_RotorPos_Ref   = 1234;
    in->gIPMPos.RotorPos           = 4321;
    in->gUVWPG.UVWAngle            = 1000;
    in->gUVWPG.UvwZeroPos          = 200;
    in->gUVWPG.UvwEstStep          = 1;
    in->gUVWPG.UvwCnt2             = 0;
    in->gGetParVarable.ParEstMstep = 0;
    in->gGetParVarable.ParEstContent[0] = PM_EST_NO_LOAD;

    /* IPMCheckInitPos */
    in->gMainStatus.ParaCalTimes   = 1;
    in->gIPMPos.PowerOffPos        = 9000;
    in->gIPMPos.PowerOffPosDeg     = 0xA5A5;
    in->gIPMPos.ZeroPosLast        = 0xA5A5;
    in->gFVCSpeed.SpeedTemp        = 0;
    in->gIPMPosCheck.Cnt           = 200;
    in->gIPMPosCheck.FirstPos      = 4000;
    in->gIPMPosCheck.TotalErr      = 0;
    in->gIPMPosCheck.TotalErrAbs   = 0;

    /* RunCaseIpmInitPos */
    in->gError.ErrorCode.all       = 0;
    in->gMainCmd.Command.all       = 0;
    in->gMainCmd.Command.bit.Start = 1;
    in->gIPMInitPos.Step           = 0;
    in->gIPMInitPos.Flag           = 0;
    in->gMainStatus.SubStep        = 1;
    in->gMainStatus.PrgStatus.all  = 0;
    in->gIPMPos.InitPos            = 777;
    in->gIPMPos.PowerOffPosDeg     = 800;

    /* SynCalRealPos */
    in->gIPMPos.QepBak             = 0;
    in->gIPMPos.QepTotal           = 0;
    in->gIPMPos.ABZ_QepBak         = 0;
    in->gIPMPos.ABZ_QepTotal       = 0;
    in->gMotorExtInfo.Poles        = 4;

    /* SynCalLdAndLq */
    in->gIPMInitPos.LPhase[0]      = 1000;
    in->gIPMInitPos.LPhase[1]      = 1100;
    in->gIPMInitPos.LPhase[2]      = 1200;
    in->gIPMInitPos.Ld             = 0xA5A5;
    in->gIPMInitPos.Lq             = 0xA5A5;
    in->gMotorExtReg.LD            = 0xA5A5;
    in->gMotorExtReg.LQ            = 0xA5A5;

    /* IPMCalAcrPIDCoff */
    in->gMotorInfo.CurrentGet      = 300;
    in->gMotorExtReg.RsPm          = 400;
    in->gPmParEst.IdKp             = 0xA5A5;
    in->gPmParEst.IdKi             = 0xA5A5;
    in->gPmParEst.IqKp             = 0xA5A5;
    in->gPmParEst.IqKi             = 0xA5A5;

    /* PrepPmsmCsrPrar */
    in->gBasePar.FcSetApply        = 50;
    in->gVCPar.AcrImKp             = 200;
    in->gVCPar.AcrImKi             = 300;
    in->gVCPar.AcrItKp             = 400;
    in->gVCPar.AcrItKi             = 500;
    in->gFluxWeak.AbsFrqLpf        = 10;   /* <= FreqPer -> the simple arm */
    in->gFluxWeak.CoefKI           = 100;
    in->gImAcrQ24.KP               = 0xA5A5;
    in->gImAcrQ24.KI               = 0xA5A5;
    in->gItAcrQ24.KP               = 0xA5A5;
    in->gItAcrQ24.KI               = 0xA5A5;

    /* PmFluxWeakDeal */
    in->gFluxWeak.Mode             = 0;
    in->gFluxWeak.IqLpf            = 0;
    in->gFluxWeak.VoltLpf          = 0;
    in->gFluxWeak.AbsFrqLpf        = 10;
    in->gFluxWeak.CoefFlux         = 100;
    in->gFluxWeak.IdMax            = 100;
    in->gFluxWeak.CoefAdj          = 100;
    in->gFluxWeak.CoefKI           = 100;
    in->gFluxWeak.AdjustId         = 0;
    in->gMainCmd.FreqSyn           = 500;
    in->gIMTSet.T                  = 0;
    in->gOutVolt.VoltApply         = 1000;
    in->gOutVolt.MaxOutVolt        = 6000;
    in->gIMTSet.M                  = 0xA5A5;

    /* PmDecoupleDeal */
    in->gPmDecoup.Omeg             = 0;
    in->gPmDecoup.Isd              = 0;
    in->gPmDecoup.Isq              = 0;
    in->gIMTQ24.M                  = 0;
    in->gIMTQ24.T                  = 0;

    /* Registers: the module writes QCLR.all / PIEACK.all / GPACLR / GPASET. */
    in->eqep.QCLR.all              = 0;
    in->pie.PIEACK.all             = 0;
    in->gpio.GPACLEAR.all          = 0;
    in->gpio.GPASET.all            = 0;

    pin_inputs(in);
}

/* ---- phases ------------------------------------------------------------- */

/* phase 1: PG_Zero_isr
 * The interrupt has three mutually exclusive entry conditions and a fourth
 * (guard) that returns before touching anything:
 *   zFilterCnt != 0            -> immediate return
 *   RunStep != STATUS_GET_PAR  -> the zero-index check (ABZ or UVW branch)
 *   RunStep == STATUS_GET_PAR  -> the tuning branch (two TUNE_STEP_NOW values)
 */
static void phase_zero(void)
{
    static const Uint16 zfilt[] = { 0, 1, 8, 0xFFFF };
    static const Uint16 runstep[] = { STATUS_GET_PAR, STATUS_RUN, 0, 0xFFFF };
    static const Uint16 pgtype[] = { PG_TYPE_ABZ, PG_TYPE_UVW, 3, 0xFFFF };
    static const Uint16 tune[] = { PM_EST_NO_LOAD, PM_EST_WITH_LOAD, 0, 0xFFFF };
    static const Uint16 eststep[] = { 0, 1, 2, 3 };
    static const Uint16 cnt2[] = { 0, 2, 3, 4 };
    /* QPOSCNT is an INPUT here: the tuning branch does
     *     gIPMPos.QepBak = GetQepCnt();
     * so with QPOSCNT pinned at 0 that assignment is indistinguishable from
     * "assign 0" and the mutation "drop GetQepCnt()" would be invisible. */
    static const Uint16 qcnt[] = { 0, 1, 0x5A5A };

    unsigned i, j, k, m, n, p, q;

    for (i = 0; i < sizeof zfilt / sizeof zfilt[0]; i++)
    for (j = 0; j < sizeof runstep / sizeof runstep[0]; j++)
    for (k = 0; k < sizeof pgtype / sizeof pgtype[0]; k++)
    {
        for (m = 0; m < sizeof tune / sizeof tune[0]; m++)
        for (n = 0; n < sizeof eststep / sizeof eststep[0]; n++)
        for (p = 0; p < sizeof cnt2 / sizeof cnt2[0]; p++)
        for (q = 0; q < sizeof qcnt / sizeof qcnt[0]; q++)
        {
            setup_defaults(&imgIn);
            imgIn.gIPMZero.zFilterCnt        = zfilt[i];
            imgIn.gMainStatus.RunStep        = runstep[j];
            imgIn.gPGData.PGType             = pgtype[k];
            imgIn.gGetParVarable.ParEstContent[0] = tune[m];
            imgIn.gUVWPG.UvwEstStep          = eststep[n];
            imgIn.gUVWPG.UvwCnt2             = (int)cnt2[p];
            imgIn.eqep.QPOSCNT               = qcnt[q];

            if (zfilt[i] != 0)
            {
                g_reach[R_Z_GUARD]++;
            }
            else if (runstep[j] != STATUS_GET_PAR)
            {
                if (pgtype[k] == PG_TYPE_ABZ)      g_reach[R_Z_ABZ]++;
                else if (pgtype[k] == PG_TYPE_UVW) g_reach[R_Z_UVW]++;
                else                               g_reach[R_Z_OTHERPG]++;
            }
            else
            {
                if (tune[m] == PM_EST_NO_LOAD)        g_reach[R_Z_TUNE_NOLOAD]++;
                else if (tune[m] == PM_EST_WITH_LOAD) g_reach[R_Z_TUNE_WITHLOAD]++;
                else                                  g_reach[R_Z_TUNE_OTHER]++;
            }

            one(E_ZERO, 0, "zero");
        }
    }

    /* The two error thresholds inside the ABZ / UVW arms are compared against
     * constants derived from 65536, so sweep the error right across them.
     *   35*65536/360 = 6371      (ABZ)
     *   65*65536/360 = 11832     (UVW)
     *   65*32767/180 = 11832     (UVW, UVWAngle space)
     * The values below straddle all three.
     */
    {
        static const Uint16 dpos[] = { 0, 1, 6370, 6371, 6372,
                                       11831, 11832, 11833, 32767, 65535 };
        for (i = 0; i < sizeof dpos / sizeof dpos[0]; i++)
        for (k = 0; k < 2; k++)
        {
            setup_defaults(&imgIn);
            imgIn.gMainStatus.RunStep  = STATUS_RUN;
            imgIn.gPGData.PGType       = (k == 0) ? PG_TYPE_ABZ : PG_TYPE_UVW;
            if (k == 0)
            {
                imgIn.gIPMPos.RotorZero        = 0;
                imgIn.gIPMPos.ABZ_RotorPos_Ref = dpos[i];
            }
            else
            {
                imgIn.gIPMPos.RotorPos  = dpos[i];
                imgIn.gIPMPos.RotorZero = 0;
                imgIn.gUVWPG.UVWAngle   = 0;
                imgIn.gUVWPG.UvwZeroPos = 0;
            }
            one(E_ZERO, 0, "zero-thr");
        }

        /* ZErrCnt wrapping: the counter is a Uint, so run it at the top. */
        for (i = 0; i < sizeof dpos / sizeof dpos[0]; i++)
        {
            setup_defaults(&imgIn);
            imgIn.gMainStatus.RunStep   = STATUS_RUN;
            imgIn.gPGData.PGType        = PG_TYPE_UVW;
            imgIn.gIPMPos.RotorPos      = dpos[i];
            imgIn.gIPMPos.RotorZero     = 0;
            imgIn.gIPMPos.ZErrCnt       = 0xFFFF;
            imgIn.gUVWPG.UVWAngle       = 0;
            imgIn.gUVWPG.UvwZeroPos     = 0;
            one(E_ZERO, 0, "zero-wrap");
        }
    }
}

/* phase 2: IPMCheckInitPos
 * Two independent switches: ParaCalTimes == 0 (first call) and PGType.  The
 * live `#if 1` block has four arms on gFVCSpeed.SpeedTemp and Cnt.
 */
static void phase_chkpos(void)
{
    static const Uint16 times[] = { 0, 1, 2, 0xFFFF };
    static const Uint16 pgtype[] = { PG_TYPE_ABZ, PG_TYPE_UVW, 2, 0xFFFF };
    static const Uint16 speed[] = { 0, 99, 100, 101, 0xFF9C /* -100 */,
                                    0xFF9D /* -101 */, 32767, 0x8000 };
    static const Uint16 cnt[] = { 0, 1, 127, 128, 129, 0xFFFF };
    static const Uint16 totabs[] = { 0, 5460, 5461, 5462, 0x7FFFFFFF };

    unsigned i, j, k, m, n, p;

    for (i = 0; i < sizeof times / sizeof times[0]; i++)
    for (j = 0; j < sizeof pgtype / sizeof pgtype[0]; j++)
    for (k = 0; k < sizeof speed / sizeof speed[0]; k++)
    for (m = 0; m < sizeof cnt / sizeof cnt[0]; m++)
    {
        setup_defaults(&imgIn);
        imgIn.gMainStatus.ParaCalTimes     = times[i];
        imgIn.gPGData.PGType               = pgtype[j];
        imgIn.gFVCSpeed.SpeedTemp          = (int)speed[k];
        imgIn.gIPMPosCheck.Cnt             = cnt[m];
        imgIn.gIPMPosCheck.TotalErrAbs     = 0;
        imgIn.gIPMPosCheck.TotalErr        = 0;
        {
            /* 16-bit abs, matching the hostlib_mock definition: abs(-32768)
             * stays negative, exactly as the target's ABS does. */
            int v  = (int)(short)speed[k];
            int av = (v < 0) ? (int)(short)(-v) : v;

            if (times[i] == 0) g_reach[R_C_FIRST]++;
            if (pgtype[j] != PG_TYPE_UVW)
            {
                g_reach[R_C_EARLY]++;
            }
            else if (av < 100)
            {
                if (cnt[m] > 128)      g_reach[R_C_LOW_HIGH]++;
                else if (cnt[m] == 0)  g_reach[R_C_LOW_ZERO]++;
                else                   g_reach[R_C_LOW_MID]++;
            }
            else
            {
                g_reach[R_C_NOTLOW]++;
            }
        }
        one(E_CHKPOS, 0, "chkpos");
    }

    /* The `(TotalErrAbs>>7) < 5461` guard and the two position errors.
     *   36*32768/180 = 6553  (UvwStopErr threshold) */
    for (i = 0; i < sizeof totabs / sizeof totabs[0]; i++)
    for (m = 0; m < sizeof cnt / sizeof cnt[0]; m++)
    for (p = 0; p < 4; p++)
    {
        static const Uint16 err[] = { 0, 6552, 6553, 6554 };
        setup_defaults(&imgIn);
        imgIn.gPGData.PGType               = PG_TYPE_UVW;
        imgIn.gFVCSpeed.SpeedTemp          = 0;
        imgIn.gIPMPosCheck.Cnt             = cnt[m];
        imgIn.gIPMPosCheck.TotalErrAbs     = (Ulong)totabs[i];
        imgIn.gIPMPosCheck.TotalErr        = 0;
        imgIn.gIPMPos.RotorPos             = 0;
        imgIn.gUVWPG.UVWAngle              = 0;
        imgIn.gUVWPG.UvwZeroPos            = err[p];
        one(E_CHKPOS, 0, "chkpos-thr");
    }

    /* (TotalErrAbs>>7) < 5461 -- the boundary is TotalErrAbs == 5461*128 =
     * 699008, and the ">>7" itself only matters in [5461*128, 5461*256).
     * The first version of this phase fed TotalErrAbs values of 5460/5461/5462,
     * i.e. the SHIFTED quantity, not the operand -- 42 after the shift instead
     * of 5461 -- so both mutations were invisible.  The table below is in the
     * operand's units on purpose. */
    {
        static const Ulong absv[] = { 0, 5460L*128, 5461L*128 - 1, 5461L*128,
                                      5461L*128 + 1, 5462L*128,
                                      5461L*256 - 1, 5461L*256, 0x7FFFFFFF };
        static const long terr[] = { 0, 1, -1, 128, -128, 100000, -100000,
                                     0x7FFFFF, -0x7FFFFF };
        unsigned a, b;

        for (a = 0; a < sizeof absv / sizeof absv[0]; a++)
        for (b = 0; b < sizeof terr / sizeof terr[0]; b++)
        {
            setup_defaults(&imgIn);
            imgIn.gPGData.PGType           = PG_TYPE_UVW;
            imgIn.gFVCSpeed.SpeedTemp      = 0;
            imgIn.gIPMPosCheck.Cnt         = 200;
            imgIn.gIPMPosCheck.TotalErrAbs = absv[a];
            imgIn.gIPMPosCheck.TotalErr    = terr[b];
            imgIn.gIPMPosCheck.FirstPos    = 0;
            imgIn.gIPMPos.RotorPos         = 0;
            imgIn.gUVWPG.UVWAngle          = 0;
            imgIn.gUVWPG.UvwZeroPos        = 0;
            one(E_CHKPOS, 0, "chkpos-acc");
        }
    }

    /* |UvwStopErr| against 36*32768/180 = 6553.  With FirstPos and TotalErr
     * both zero, uvwPos == UvwZeroPos, so the error is exactly -UvwZeroPos and
     * the boundary can be hit dead on.  (The earlier sweep left FirstPos at
     * 4000, which shifted the error by 4000 and never landed on 6553.) */
    {
        static const Uint upos[] = { 0, 1, 6552, 6553, 6554, 32767, 65535 };
        unsigned a;

        for (a = 0; a < sizeof upos / sizeof upos[0]; a++)
        {
            setup_defaults(&imgIn);
            imgIn.gPGData.PGType           = PG_TYPE_UVW;
            imgIn.gFVCSpeed.SpeedTemp      = 0;
            imgIn.gIPMPosCheck.Cnt         = 200;
            imgIn.gIPMPosCheck.TotalErrAbs = 0;
            imgIn.gIPMPosCheck.TotalErr    = 0;
            imgIn.gIPMPosCheck.FirstPos    = 0;
            imgIn.gIPMPos.RotorPos         = 0;
            imgIn.gUVWPG.UVWAngle          = 0;
            imgIn.gUVWPG.UvwZeroPos        = upos[a];
            one(E_CHKPOS, 0, "chkpos-stop");
        }
    }
}

/* phase 3: PmChkInitPosRest -- two stores, nothing else. */
static void phase_rest(void)
{
    static const long tot[] = { 0, 1, -1, 0x7FFFFFFF };
    unsigned i;

    for (i = 0; i < sizeof tot / sizeof tot[0]; i++)
    {
        setup_defaults(&imgIn);
        imgIn.gIPMPosCheck.Cnt        = 5;
        imgIn.gIPMPosCheck.TotalErr   = tot[i];
        imgIn.gIPMPosCheck.TotalErrAbs = (Ulong)tot[i];
        one(E_REST, 0, "rest");
    }
}

/* phase 4: RunCaseIpmInitPos
 * Early return on error/stop, then the SubStep switch 1..4 + default.  Each
 * case also has its own guard on gIPMInitPos.Step.
 */
static void phase_runcase(void)
{
    static const Uint16 err[] = { 0, 1, 0xFFFF };
    static const Uint16 start[] = { 0, 1 };
    static const Uint16 substep[] = { 0, 1, 2, 3, 4, 5, 0xFFFF };
    static const Uint16 step[] = { 0, 1, 2 };

    unsigned i, j, k, m;

    for (i = 0; i < sizeof err / sizeof err[0]; i++)
    for (j = 0; j < sizeof start / sizeof start[0]; j++)
    for (k = 0; k < sizeof substep / sizeof substep[0]; k++)
    for (m = 0; m < sizeof step / sizeof step[0]; m++)
    {
        setup_defaults(&imgIn);
        imgIn.gError.ErrorCode.all     = err[i];
        imgIn.gMainCmd.Command.bit.Start = (start[j] != 0) ? 1 : 0;
        imgIn.gMainStatus.SubStep      = substep[k];
        imgIn.gIPMInitPos.Step         = step[m];
        if (err[i] != 0 || start[j] == 0)
        {
            g_reach[R_R_EARLY]++;
        }
        else
        {
            switch (substep[k])
            {
            case 1:  g_reach[R_R_CASE1]++;   break;
            case 2:  g_reach[R_R_CASE2]++;   break;
            case 3:  g_reach[R_R_CASE3]++;   break;
            case 4:  g_reach[R_R_CASE4]++;   break;
            default: g_reach[R_R_DEFAULT]++; break;
            }
        }
        one(E_RUNCASE, 0, "runcase");
    }

    /* The empty `if (abs(PowerOffPosDeg - RotorPos) < 3641) { }` in case 2 has
     * no effect by construction, but the subtraction is still evaluated, so
     * drive the difference across the constant and let the comparison prove
     * that removing the statement would be invisible. */
    {
        static const Uint16 delta[] = { 0, 3640, 3641, 3642, 65535 };
        for (i = 0; i < sizeof delta / sizeof delta[0]; i++)
        {
            setup_defaults(&imgIn);
            imgIn.gMainStatus.SubStep      = 2;
            imgIn.gIPMInitPos.Step         = 0;
            imgIn.gIPMPos.InitPos          = 1000;
            imgIn.gIPMPos.PowerOffPosDeg   = (Uint)(1000 + delta[i]);
            one(E_RUNCASE, 0, "runcase-delta");
        }
    }
}

/* phase 5: SynCalRealPos
 * QEP accumulation, the two modulo/divide pairs, and the two wrap-back
 * corrections at 50*4*PulseNum (subtract 30*4*PulseNum).
 */
static void phase_realpos(void)
{
    static const Uint16 pulse[] = { 1, 2, 100, 2500, 4096, 0xFFFF };
    static const long qep[] = { 0, 1, -1, 9999, 10000, 10001, -9999, -10000,
                                -10001, 0x7FFFFFFF, -0x7FFFFFFF };
    static const Uint16 pgtype[] = { PG_TYPE_ABZ, PG_TYPE_UVW };

    /* Poles is an INPUT, not decoration.  The two angle chains end in
     *     gIPMZero.FeedPos    = m_Pos    * Poles;      (always)
     *     gIPMZero.FeedABZPos = m_ABZPos * Poles;      (ABZ only)
     * and both products are truncated to the 16-bit Uint fields.  With
     * Poles == 4 a difference of exactly 32768 in the angle is multiplied by 4
     * and lands back on 0 mod 65536 -- i.e. a whole class of angle errors is
     * invisible.  Poles must therefore include ODD values, where 32768*Poles
     * is 32768 mod 65536 and stays visible.  (This is what hid "ABZ four-times
     * shift 2 -> 1": it changes the angle by exactly 32768.) */
    static const Uint16 poles[] = { 1, 3, 4 };

    unsigned i, j, k, m;

    for (i = 0; i < sizeof pulse / sizeof pulse[0]; i++)
    for (j = 0; j < sizeof qep / sizeof qep[0]; j++)
    for (k = 0; k < 2; k++)
    for (m = 0; m < sizeof poles / sizeof poles[0]; m++)
    {
        setup_defaults(&imgIn);
        imgIn.gPGData.PulseNum       = pulse[i];
        imgIn.gPGData.PGType         = pgtype[k];
        imgIn.gMotorExtInfo.Poles    = poles[m];
        imgIn.eqep.QPOSCNT           = (Uint)qep[j];
        imgIn.gIPMPos.QepBak         = qep[j];
        imgIn.gIPMPos.QepTotal       = qep[j];
        imgIn.gIPMPos.ABZ_QepBak     = qep[j];
        imgIn.gIPMPos.ABZ_QepTotal   = qep[j];
        if (pgtype[k] == PG_TYPE_ABZ) g_reach[R_P_ABZ]++;
        else                          g_reach[R_P_UVW]++;
        one(E_REALPOS, 0, "realpos");
    }

    /* The wrap thresholds are 50*4*PulseNum = 200*PulseNum, tested on the
     * ALREADY-UPDATED accumulator:
     *     gIPMPos.QepTotal += m_DetaCnt;      then
     *     if (labs(gIPMPos.QepTotal) > 200*PulseNum) { ... }
     * The first version of this sweep pinned QPOSCNT at 0 and set QepBak to
     * the threshold value, so the delta was -threshold and QepTotal collapsed
     * to 0 before the comparison -- the threshold was never actually reached
     * and both "> vs >=" mutations were invisible.  QepBak must equal QPOSCNT
     * (delta 0) so the accumulator really sits on the boundary. */
    {
        static const Uint16 wpulse[] = { 1, 100 };
        static const long woff[] = { -1, 0, 1 };
        unsigned a, b, s;

        for (a = 0; a < sizeof wpulse / sizeof wpulse[0]; a++)
        for (b = 0; b < sizeof woff / sizeof woff[0]; b++)
        for (s = 0; s < 2; s++)
        for (k = 0; k < 2; k++)
        {
            long thr = 200L * (long)wpulse[a];
            long v   = (s == 0) ? (thr + woff[b]) : -(thr + woff[b]);

            setup_defaults(&imgIn);
            imgIn.gPGData.PulseNum       = wpulse[a];
            imgIn.gPGData.PGType         = pgtype[k];
            imgIn.gMotorExtInfo.Poles    = 3;
            imgIn.eqep.QPOSCNT           = 0;
            imgIn.gIPMPos.QepBak         = 0;
            imgIn.gIPMPos.QepTotal       = v;
            imgIn.gIPMPos.ABZ_QepBak     = 0;
            imgIn.gIPMPos.ABZ_QepTotal   = v;
            one(E_REALPOS, 0, "realpos-wrap");
        }
    }

    /* Negative QepTotal: `%` on a negative dividend truncates toward zero on
     * both compilers, but the result feeds `<<14` and a divide, so it is worth
     * a dedicated sweep rather than trusting the language. */
    for (i = 1; i < 40; i++)
    {
        setup_defaults(&imgIn);
        imgIn.gPGData.PulseNum     = 7;
        imgIn.gPGData.PGType       = PG_TYPE_ABZ;
        imgIn.eqep.QPOSCNT         = (Uint)(0 - (int)i);
        imgIn.gIPMPos.QepBak       = 0;
        imgIn.gIPMPos.QepTotal     = -(long)i;
        imgIn.gIPMPos.ABZ_QepBak   = 0;
        imgIn.gIPMPos.ABZ_QepTotal = -(long)i;
        one(E_REALPOS, 0, "realpos-neg");
    }
}

/* phase 6: SynCalLdAndLq
 * The divisor is m_Cos1 - m_Cos2, a function of the argument, so it cannot be
 * pinned.  Compute the same denominator here and skip the arguments where it is
 * zero -- on the target that division does not trap, on the host it does.
 */
static void phase_ldlq(void)
{
    Uint pos;
    unsigned step;
    long skipped = 0;
    long ran = 0;

    /* `unsigned long` for the counter on purpose: the case shim folds `int` to
     * 16 bits, so a plain `unsigned i` would be a 16-bit counter and
     * `i < 0x10000` would never become false.  The argument itself is a Uint
     * and is derived from i by an explicit cast, so no wrap can hide an
     * argument. */
    static const unsigned steps[] = { 1, 7, 97 };

    for (step = 0; step < sizeof steps / sizeof steps[0]; step++)
    {
        unsigned long i;

        for (i = 0; i < 0x10000UL; i += (unsigned long)steps[step])
        {
            Uint a;
            int  c1;
            int  c2;

            pos = (Uint)i;

            a = (Uint)(pos << 1);
            c1 = qsin(16384 - (int)a);
            a = (Uint)(a + 43691);
            c2 = qsin(16384 - (int)a);

            if (c1 - c2 == 0)
            {
                skipped++;
                continue;
            }

            setup_defaults(&imgIn);
            imgIn.gIPMInitPos.LPhase[0] = 1000;
            imgIn.gIPMInitPos.LPhase[1] = 1100;
            imgIn.gIPMInitPos.LPhase[2] = 1200;
            one(E_LDLQ, pos, "ldlq");
            ran++;
            g_reach[R_L_RAN]++;
        }
    }

    printf("ldlq: ran=%ld skipped(zero divisor)=%ld\n", ran, skipped);

    /* Also sweep the phase inductances, since the numerator is their
     * difference and a zero there is fine but a wrap is not. */
    {
        static const Uint16 lph[] = { 0, 1, 1000, 0x7FFF, 0x8000, 0xFFFF };
        unsigned i, j;

        for (i = 0; i < sizeof lph / sizeof lph[0]; i++)
        for (j = 0; j < sizeof lph / sizeof lph[0]; j++)
        {
            Uint a = (Uint)(0x1234 << 1);
            int  c1 = qsin(16384 - (int)a);
            int  c2;
            a = (Uint)(a + 43691);
            c2 = qsin(16384 - (int)a);
            if (c1 - c2 == 0)
            {
                continue;
            }

            setup_defaults(&imgIn);
            imgIn.gIPMInitPos.LPhase[0] = lph[i];
            imgIn.gIPMInitPos.LPhase[1] = lph[j];
            imgIn.gIPMInitPos.LPhase[2] = lph[i];
            one(E_LDLQ, 0x1234, "ldlq-phase");
        }
    }
}

/* phase 7: IPMCalAcrPIDCoff -- two divides and one shift. */
static void phase_pid(void)
{
    static const Uint16 ld[] = { 0, 1, 1200, 0x7FFF, 0xFFFF };
    static const Uint16 cur[] = { 0, 1, 300, 0x7FFF, 0xFFFF };
    static const Uint16 volt[] = { 1, 2, 540, 0x7FFF, 0xFFFF };
    static const Uint16 rs[] = { 0, 1, 400, 0x7FFF, 0xFFFF };

    unsigned i, j, k, m;

    for (i = 0; i < sizeof ld / sizeof ld[0]; i++)
    for (j = 0; j < sizeof cur / sizeof cur[0]; j++)
    for (k = 0; k < sizeof volt / sizeof volt[0]; k++)
    for (m = 0; m < sizeof rs / sizeof rs[0]; m++)
    {
        setup_defaults(&imgIn);
        imgIn.gMotorExtReg.LD      = ld[i];
        imgIn.gMotorExtReg.LQ      = ld[i];
        imgIn.gMotorInfo.CurrentGet = cur[j];
        imgIn.gMotorInfo.Votage    = volt[k];
        imgIn.gMotorExtReg.RsPm    = rs[m];
        g_reach[R_D_RAN]++;
        one(E_PID, 0, "pid");
    }
}

/* phase 8: PrepPmsmCsrPrar
 * Two arms on AbsFrqLpf <= FreqPer.  The else arm divides by
 * (32767 - FreqPer), so FreqPer == 32767 must not be fed to it.
 */
static void phase_prep(void)
{
    static const Uint16 fc[] = { 0, 1, 39, 40, 41, 1000 };
    static const Uint16 freqper[] = { 0, 1, 100, 32766 };
    static const Uint16 absfrq[] = { 0, 1, 99, 100, 101, 1000, 32767 };
    static const Uint16 coefki[] = { 0, 1, 100, 32767, 0xFFFF };

    unsigned i, j, k, m;

    for (i = 0; i < sizeof fc / sizeof fc[0]; i++)
    for (j = 0; j < sizeof freqper / sizeof freqper[0]; j++)
    for (k = 0; k < sizeof absfrq / sizeof absfrq[0]; k++)
    for (m = 0; m < sizeof coefki / sizeof coefki[0]; m++)
    {
        setup_defaults(&imgIn);
        imgIn.gBasePar.FcSetApply   = fc[i];
        imgIn.gMotorInfo.FreqPer    = freqper[j];
        imgIn.gFluxWeak.AbsFrqLpf   = (int)absfrq[k];
        imgIn.gFluxWeak.CoefKI      = (int)coefki[m];
        if ((int)(short)absfrq[k] <= (int)(short)freqper[j]) g_reach[R_PREP_SIMPLE]++;
        else                                                 g_reach[R_PREP_GAIN]++;
        one(E_PREP, 0, "prep");
    }
}

/* phase 9: PmFluxWeakDeal
 * One long chain of filters, two clamps and, in Mode 2, a division by CoefAdj
 * (which is clamped to >= 10 first, so it cannot be zero).
 */
static void phase_fluxweak(void)
{
    static const Uint16 mode[] = { 0, 1, 2, 3 };
    static const Uint16 iq[] = { 0, 1, 1000, 0x7FFF, 0x8000, 0xFFFF };
    static const Uint16 volt[] = { 0, 1, 1000, 0x7FFF, 0x8000 };
    static const Uint16 freqsyn[] = { 0, 1, 4, 5, 6, 500, -500, 0x7FFF, 0x8000 };
    static const Uint16 maxvolt[] = { 0, 400, 401, 6000, 0x7FFF, 0x8000 };
    static const Uint16 idmax[] = { 0, 1, 100, 0x7FFF };
    static const Uint16 coefadj[] = { 0, 1, 10, 11, 1000, 1001, 0x7FFF };

    unsigned i, j, k, m, n, p, q;

    for (i = 0; i < sizeof mode / sizeof mode[0]; i++)
    for (j = 0; j < sizeof iq / sizeof iq[0]; j++)
    for (k = 0; k < sizeof volt / sizeof volt[0]; k++)
    for (m = 0; m < sizeof freqsyn / sizeof freqsyn[0]; m++)
    {
        setup_defaults(&imgIn);
        imgIn.gFluxWeak.Mode        = mode[i];
        imgIn.gFluxWeak.IqLpf       = (long)(short)iq[j];
        imgIn.gFluxWeak.VoltLpf     = (int)(short)volt[k];
        imgIn.gMainCmd.FreqSyn      = (int)(short)freqsyn[m];
        imgIn.gOutVolt.VoltApply    = (int)(short)volt[k];
        if (mode[i] == 2) g_reach[R_F_MODE2]++;
        else              g_reach[R_F_OTHER]++;
        one(E_FLUXWEAK, 0, "fluxweak");
    }

    /* `frq = (abs(FreqSyn) > minFrq) ? abs(FreqSyn) : minFrq;` with minFrq = 5.
     * The only inputs that separate `: minFrq` from `: (minFrq - 1)` are
     * abs(FreqSyn) <= 5, and there the difference is a single count -- which the
     * first-order filter rounds away unless the previous state happens to sit on
     * the right side of a 4-block boundary.  Filter4(x,total) is
     *     ((total<<16) + (x<<14) - (total<<14)) >> 16   ==  (3*total + x)/4
     * in UNSIGNED 32-bit arithmetic, so (3*9 + 5)/4 = 8 but (3*9 + 4)/4 = 7,
     * while (3*10 + 5)/4 = (3*10 + 4)/4 = 8.  Sweeping the previous state is
     * therefore mandatory, not extra: with AbsFrqLpf pinned at 10 -- which is
     * what setup_defaults supplies -- the mutation is invisible. */
    {
        static const int abs0[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
                                    1000, 1001, 30000 };
        static const Uint16 f2[] = { 0, 1, 2, 3, 4, 5, 6 };
        long skipped = 0;
        unsigned a, b;

        for (a = 0; a < sizeof abs0 / sizeof abs0[0]; a++)
        for (b = 0; b < sizeof f2 / sizeof f2[0]; b++)
        {
            int fs  = (int)(short)f2[b];
            int afs = (fs < 0) ? (int)(short)(-fs) : fs;
            int frq = (afs > 5) ? afs : 5;
            int nf  = (int)Filter4(frq, abs0[a]);

            if (nf == 0)
            {
                /* Host-only gap: the module divides by this value later on.
                 * With abs0 >= 0 and frq >= 5 it cannot happen, but the guard
                 * is what makes that claim checkable instead of asserted. */
                skipped++;
                continue;
            }

            setup_defaults(&imgIn);
            imgIn.gFluxWeak.AbsFrqLpf = abs0[a];
            imgIn.gMainCmd.FreqSyn    = fs;
            g_reach[R_F_OTHER]++;
            one(E_FLUXWEAK, 0, "fluxweak-minfrq");
        }

        printf("fluxweak: skipped(zero filtered AbsFrqLpf)=%ld\n", skipped);
    }

    for (n = 0; n < sizeof maxvolt / sizeof maxvolt[0]; n++)
    for (p = 0; p < sizeof idmax / sizeof idmax[0]; p++)
    for (q = 0; q < sizeof coefadj / sizeof coefadj[0]; q++)
    {
        setup_defaults(&imgIn);
        imgIn.gFluxWeak.Mode        = 2;
        imgIn.gOutVolt.MaxOutVolt   = (int)(short)maxvolt[n];
        imgIn.gFluxWeak.IdMax       = (int)(short)idmax[p];
        imgIn.gFluxWeak.CoefAdj     = (int)(short)coefadj[q];
        imgIn.gFluxWeak.AdjustId    = 100;
        g_reach[R_F_MODE2]++;
        one(E_FLUXWEAK, 0, "fluxweak-adj");
    }

    /* AdjustId / AdjustLimit sign and wrap behaviour. */
    {
        static const int adj[] = { 0, 1, -1, 32767, -32768 };
        unsigned a, b;
        for (a = 0; a < sizeof adj / sizeof adj[0]; a++)
        for (b = 0; b < sizeof adj / sizeof adj[0]; b++)
        {
            setup_defaults(&imgIn);
            imgIn.gFluxWeak.Mode      = 2;
            imgIn.gFluxWeak.AdjustId  = adj[a];
            imgIn.gOutVolt.MaxOutVolt = adj[b];
            imgIn.gFluxWeak.CoefAdj   = 100;
            one(E_FLUXWEAK, 0, "fluxweak-adjid");
        }
    }
}

/* phase 10: PmDecoupleDeal -- two filters, four multiplies, no divide. */
static void phase_decouple(void)
{
    static const Uint16 omeg[] = { 0, 1, -1, 500, -500, 32767, -32768 };
    static const Uint16 mt[] = { 0, 1, 4096, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF };
    static const Uint16 ld[] = { 0, 1, 1200, 0x7FFF, 0xFFFF };
    static const Uint16 flux[] = { 0, 1, 900, 0x7FFF, 0xFFFF };

    unsigned i, j, k, m;

    for (i = 0; i < sizeof omeg / sizeof omeg[0]; i++)
    for (j = 0; j < sizeof mt / sizeof mt[0]; j++)
    for (k = 0; k < sizeof ld / sizeof ld[0]; k++)
    for (m = 0; m < sizeof flux / sizeof flux[0]; m++)
    {
        setup_defaults(&imgIn);
        imgIn.gMainCmd.FreqSyn        = (int)(short)omeg[i];
        imgIn.gIMTQ24.M               = (long)mt[j];
        imgIn.gIMTQ24.T               = (long)(0 - (long)mt[j]);
        imgIn.gMotorExtPer.LD         = ld[k];
        imgIn.gMotorExtPer.LQ         = ld[k];
        imgIn.gMotorExtPer.FluxRotor  = flux[m];
        one(E_DECOUPLE, 0, "decouple");
    }
}

/* phase 11: random
 * Deliberately sloppy on purpose: every field gets a random value, then
 * pin_inputs() restores the divisors.  This is what catches the interactions
 * the hand-built phases cannot enumerate -- a stale field that a phase forgot
 * to reset, a branch that only opens when two unrelated fields agree.
 */
static unsigned long g_seed;

static unsigned long rnd(void)
{
    g_seed = g_seed * 1103515245UL + 12345UL;
    return (g_seed >> 16) & 0xFFFFUL;
}

static void phase_random(long n)
{
    long i;

    for (i = 0; i < n; i++)
    {
        int e;

        memset(&imgIn, 0xA5, sizeof imgIn);
        pin_inputs(&imgIn);

        imgIn.gIPMZero.zFilterCnt     = (Uint)rnd();
        imgIn.gIPMZero.Flag           = (Uint)rnd();
        imgIn.gMainStatus.RunStep     = (Uint)rnd();
        imgIn.gMainStatus.SubStep     = (Uint)rnd();
        imgIn.gMainStatus.ParaCalTimes = (Uint)rnd();
        imgIn.gPGData.PGType          = (Uint)rnd();
        imgIn.gPGData.PulseNum        = (Uint)(rnd() | 1);
        imgIn.gIPMPos.RotorZero       = (Uint)rnd();
        imgIn.gIPMPos.RotorPos        = (Uint)rnd();
        imgIn.gIPMPos.ABZ_RotorPos_Ref = (Uint)rnd();
        imgIn.gIPMPos.InitPos         = (Uint)rnd();
        imgIn.gIPMPos.PowerOffPos     = (Uint)rnd();
        imgIn.gIPMPos.PowerOffPosDeg  = (Uint)rnd();
        imgIn.gIPMPos.ZErrCnt         = (Uint)rnd();
        imgIn.gIPMPos.QepBak          = (long)(int)(short)rnd();
        imgIn.gIPMPos.QepTotal        = (long)(int)(short)rnd();
        imgIn.gIPMPos.ABZ_QepBak      = (long)(int)(short)rnd();
        imgIn.gIPMPos.ABZ_QepTotal    = (long)(int)(short)rnd();
        imgIn.gUVWPG.UVWAngle         = (Uint)rnd();
        imgIn.gUVWPG.UvwZeroPos       = (Uint)rnd();
        imgIn.gUVWPG.UvwEstStep       = (Uint)rnd();
        imgIn.gUVWPG.UvwCnt2          = (int)(short)rnd();
        imgIn.gFVCSpeed.SpeedTemp     = (int)(short)rnd();
        imgIn.gIPMPosCheck.Cnt        = (Uint)rnd();
        imgIn.gIPMPosCheck.FirstPos   = (Uint)rnd();
        imgIn.gIPMPosCheck.TotalErr   = (long)(int)(short)rnd();
        imgIn.gIPMPosCheck.TotalErrAbs = (Ulong)rnd();
        imgIn.gError.ErrorCode.all    = (Uint)rnd();
        imgIn.gMainCmd.Command.all    = (Uint)rnd();
        imgIn.gMainCmd.FreqSyn        = (int)(short)rnd();
        imgIn.gIPMInitPos.Step        = (Uint)rnd();
        imgIn.gIPMInitPos.LPhase[0]   = (Uint)rnd();
        imgIn.gIPMInitPos.LPhase[1]   = (Uint)rnd();
        imgIn.gIPMInitPos.LPhase[2]   = (Uint)rnd();
        imgIn.gMotorInfo.CurrentGet   = (Uint)rnd();
        imgIn.gMotorInfo.Votage       = (Uint)(rnd() | 1);
        imgIn.gMotorInfo.FreqPer      = (Uint)(rnd() % 32766);
        imgIn.gMotorExtReg.LD         = (Uint)rnd();
        imgIn.gMotorExtReg.LQ         = (Uint)rnd();
        imgIn.gMotorExtReg.RsPm       = (Uint)rnd();
        imgIn.gMotorExtPer.LD         = (Uint)(rnd() | 1);
        imgIn.gMotorExtPer.LQ         = (Uint)rnd();
        imgIn.gMotorExtPer.FluxRotor  = (Uint)rnd();
        imgIn.gMotorExtInfo.Poles     = (Uint)rnd();
        imgIn.gBasePar.FcSetApply     = (int)(short)rnd();
        imgIn.gVCPar.AcrImKp          = (Uint)rnd();
        imgIn.gVCPar.AcrImKi          = (Uint)rnd();
        imgIn.gVCPar.AcrItKp          = (Uint)rnd();
        imgIn.gVCPar.AcrItKi          = (Uint)rnd();
        imgIn.gFluxWeak.Mode          = (Uint)(rnd() % 4);
        imgIn.gFluxWeak.IqLpf         = (long)(int)(short)rnd();
        imgIn.gFluxWeak.VoltLpf       = (int)(short)rnd();
        imgIn.gFluxWeak.AbsFrqLpf     = (int)(short)rnd();
        imgIn.gFluxWeak.CoefFlux      = (int)(short)rnd();
        imgIn.gFluxWeak.IdMax         = (int)(short)rnd();
        imgIn.gFluxWeak.CoefAdj       = (int)(short)rnd();
        imgIn.gFluxWeak.CoefKI        = (int)(short)rnd();
        imgIn.gFluxWeak.AdjustId      = (int)(short)rnd();
        imgIn.gOutVolt.VoltApply      = (int)(short)rnd();
        imgIn.gOutVolt.MaxOutVolt     = (int)(short)rnd();
        imgIn.gIMTQ24.M               = (long)(int)(short)rnd();
        imgIn.gIMTQ24.T               = (long)(int)(short)rnd();
        imgIn.gGetParVarable.ParEstMstep = (Uint16)(rnd() % 4);
        imgIn.gGetParVarable.ParEstContent[imgIn.gGetParVarable.ParEstMstep] =
            (PAR_EST_MAIN_STEP)(rnd() % 8);
        imgIn.eqep.QPOSCNT            = (Uint)rnd();

        e = (int)(rnd() % 10);

        switch (e)
        {
        case 0:  one(E_ZERO,     0,           "rnd-zero");     break;
        case 1:  one(E_CHKPOS,   0,           "rnd-chkpos");   break;
        case 2:  one(E_REST,     0,           "rnd-rest");     break;
        case 3:  one(E_RUNCASE,  0,           "rnd-runcase");  break;
        case 4:  one(E_REALPOS,  0,           "rnd-realpos");  break;
        case 5:
        {
            /* Skip the arguments whose divisor is zero -- see phase_ldlq(). */
            Uint pos = (Uint)rnd();
            Uint a = (Uint)(pos << 1);
            int  c1 = qsin(16384 - (int)a);
            int  c2;
            a = (Uint)(a + 43691);
            c2 = qsin(16384 - (int)a);
            if (c1 - c2 != 0)
            {
                one(E_LDLQ, pos, "rnd-ldlq");
            }
            break;
        }
        case 6:  one(E_PID,      0,           "rnd-pid");      break;
        case 7:  one(E_PREP,     0,           "rnd-prep");     break;
        case 8:  one(E_FLUXWEAK, 0,           "rnd-fluxweak"); break;
        default: one(E_DECOUPLE, 0,           "rnd-decouple"); break;
        }
    }
}

/* ---- bench self-check --------------------------------------------------- */

static int bench_selfcheck(void)
{
    struct IMAGE s;

    /* 1. The int fold must be in place.  Without it several 16-bit truncations
     *    (PowerOffPosDeg, the `(int)` casts in the QEP math) keep 32 bits on
     *    the host and a truncation-only mutation would hide. */
    if (sizeof(int) != 2 || sizeof(Uint) != 2 || sizeof(long) != 4)
    {
        printf("SELFCHECK: width fold lost (int=%u Uint=%u long=%u)\n",
               (unsigned)sizeof(int), (unsigned)sizeof(Uint),
               (unsigned)sizeof(long));
        return 0;
    }

    /* 2. TUNE_STEP_NOW must point where we think it points.  The macro is
     *    `gGetParVarable.ParEstContent[gGetParVarable.ParEstMstep]`; if the
     *    driver poked a different field the whole tuning branch would be
     *    driven from the wrong place and every case would still pass. */
    setup_defaults(&s);
    s.gGetParVarable.ParEstMstep = 1;
    s.gGetParVarable.ParEstContent[1] = PM_EST_WITH_LOAD;
    img_load(&s);
    if (TUNE_STEP_NOW != PM_EST_WITH_LOAD)
    {
        printf("SELFCHECK: TUNE_STEP_NOW does not follow ParEstMstep\n");
        return 0;
    }

    /* 3. The register images must round-trip: if poking EQepRegs.QPOSCNT did
     *    not reach GetQepCnt(), SynCalRealPos would be driven from a constant. */
    img_load(&s);
    EQepRegs->QPOSCNT = 0x1234;
    if (GetQepCnt() != 0x1234)
    {
        printf("SELFCHECK: GetQepCnt() does not read EQepRegs.QPOSCNT\n");
        return 0;
    }

    /* 4. cmp_snap must be able to see a difference at all. */
    img_load(&s);
    capture(&snapRef, 1);
    new_gIPMPos.RotorPos = (Uint)(ref_gIPMPos.RotorPos + 1);
    capture(&snapNew, 0);
    g_quiet = 1;
    if (cmp_snap(&snapRef, &snapNew, "selfcheck") == 0)
    {
        g_quiet = 0;
        printf("SELFCHECK: cmp_snap did not see a planted difference\n");
        return 0;
    }
    g_quiet = 0;

    /* 5. The call ledger must be wired: a stubbed callee that is never counted
     *    would make the four argument-carrying entry points unobservable.
     *    SetIPMPos is the mock's own symbol (not renamed -- it is not part of
     *    the module), so it is called by its real name here. */
    img_load(&s);
    SetIPMPos(0x4242);
    if (gMockCall[MC_SETIPMPOS] != 1 || gMockArg[MC_SETIPMPOS] != 0x4242)
    {
        printf("SELFCHECK: SetIPMPos stub is not recording\n");
        return 0;
    }

    g_cases = 0;
    g_fail  = 0;
    g_stop  = 0;
    printf("SELFCHECK: ok\n");
    return 1;
}

/* ---- main --------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long nrand = 20000;

    /* Unbuffered: if a case ever faults, the last phase printed is the one that
     * faulted.  A fully-buffered stdout would lose all of it to the crash and
     * leave nothing but a return code. */
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc > 1)
    {
        nrand = atol(argv[1]);
        if (nrand < 0)
        {
            nrand = 0;
        }
    }

    g_seed = (unsigned long)(argc > 2 ? atol(argv[2]) : 1);

    printf("sizeof: int=%u Uint=%u long=%u void*=%u\n",
           (unsigned)sizeof(int), (unsigned)sizeof(Uint),
           (unsigned)sizeof(long), (unsigned)sizeof(void *));

    if (!bench_selfcheck())
    {
        return 2;
    }

    printf("phase zero\n");      phase_zero();
    printf("phase chkpos\n");    phase_chkpos();
    printf("phase rest\n");      phase_rest();
    printf("phase runcase\n");   phase_runcase();
    printf("phase realpos\n");   phase_realpos();
    printf("phase ldlq\n");      phase_ldlq();
    printf("phase pid\n");       phase_pid();
    printf("phase prep\n");      phase_prep();
    printf("phase fluxweak\n");  phase_fluxweak();
    printf("phase decouple\n");  phase_decouple();
    printf("phase random\n");    phase_random(nrand);

    printf("MotorPmsmMain difftest: cases=%ld mismatches=%ld\n", g_cases, g_fail);
    reach_dump();

    if (g_fail != 0)
    {
        return 1;
    }
    return 0;
}
