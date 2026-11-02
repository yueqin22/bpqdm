/* Differential-test driver for MotorParChange.
 *
 * The frozen baseline (cases/MotorParChange/ref/MotorParChange_ref.c) and the
 * delivered source (src/motor/MotorParChange.c) are compiled into two symbol
 * namespaces, ref_* and new_*. Both are handed identical inputs and every
 * observable is compared after every call.
 *
 * ---------------------------------------------------------------------------
 * Shape of the module
 * ---------------------------------------------------------------------------
 * Five entry points, no parameters, all state in globals:
 *
 *   SystemParChg2Ms     operator settings -> inverter scaling (2 ms task)
 *   RunStateParChg2Ms   run-time derived quantities (2 ms task)
 *   SystemParChg05Ms    encoder mode / QEP select re-init (0.5 ms task)
 *   ChangeMotorPar      motor parameter normalisation
 *   InvCalcPower        power / torque reconstruction
 *
 * Unlike MotorPmsmMain / MotorPmsmParEst, this module owns NOTHING. Every one
 * of the 33 globals it touches belongs to another module, and the three
 * peripherals it writes are the chip's. So there is no per-side private state:
 * one shared image, snapshotted around each side.
 *
 * ---------------------------------------------------------------------------
 * The one thing that must not go wrong: divisor pinning
 * ---------------------------------------------------------------------------
 * This module divides by a dozen different globals, most of them unguarded:
 *
 *   SystemParChg2Ms   / gMotorInfo.CurrentGet        (m_ULong)
 *                     / gBasePar.FullFreq            (FreqPer)
 *                     / gRotorTrans.Poles            (PolesRatio)
 *   RunStateParChg2Ms / abs(gMainCmd.FreqSyn)        (IoVsFreq)  <- 0 at standstill
 *                     / gInvInfo.InvCurrentOvload    (CurBaseInv)
 *                     / gMotorInfo.Votage            (MaxOutVolt)
 *                     / gMotorInfo.Current           (Coff)
 *   ChangeMotorPar    / gMotorInfo.Current           (m_BaseL)
 *                     / gBasePar.FullFreq01          (m_BaseL)
 *                     / gMotorInfo.Votage            (R1/R2/RsPm)
 *                     / m_BaseL                      (L1/L2/LM/LD/LQ)
 *                     / gMotorExtPer.ItRated         (rpItRated)
 *                     / (gMotorInfo.Votage * 10)     (FluxRotor)
 *                     / gMotorInfo.FreqPer           (FluxRotor)
 *                     / gMotorExtInfo.Rpm            (Poles)
 *
 * On the host a zero divisor is #DE and kills the process -- not a mismatch,
 * a dead ledger. On the C28x it is a defined (saturated) result, so the target
 * build tolerates inputs the bench cannot. pin_divisors() therefore forces
 * every one of them non-zero on every scenario, and two of them (m_BaseL,
 * ItRated) are computed *by the module*, so their inputs are constrained
 * instead: gMotorInfo.Votage/Current/FullFreq01 and gMotorExtInfo.I0 are
 * chosen so both stay > 0.
 *
 * ---------------------------------------------------------------------------
 * dirty_* discipline
 * ---------------------------------------------------------------------------
 * The comparison only sees what the NEW side leaves behind. A rewrite that
 * stops writing a field is invisible if the pre-existing value already equals
 * what correct code writes, so every word is pre-filled with 0xA5.
 */

#include <stdio.h>
#include <string.h>

/* <stdlib.h> is deliberately NOT included: the case shim folds `int` to the
 * target's 16 bits and stdlib's 32-bit `int abs(int)` would collide with the
 * project's folded 16-bit prototype in SubPrgInclude.h. */
extern long atol(const char *);

#include "MotorInclude.h"
#include "mock_calls.h"

/* ---- renamed entry points ------------------------------------------------ */

extern void ref_SystemParChg2Ms(void);
extern void ref_RunStateParChg2Ms(void);
extern void ref_SystemParChg05Ms(void);
extern void ref_ChangeMotorPar(void);
extern void ref_InvCalcPower(void);

extern void new_SystemParChg2Ms(void);
extern void new_RunStateParChg2Ms(void);
extern void new_SystemParChg05Ms(void);
extern void new_ChangeMotorPar(void);
extern void new_InvCalcPower(void);

/* ---- mock-owned ---------------------------------------------------------- */

extern volatile struct EPWM_REGS     EPwm1Regs;
extern volatile struct EPWM_REGS     EPwm2Regs;
extern volatile struct EPWM_REGS     EPwm3Regs;
extern volatile struct EQEP_REGS     EQep1Regs;
extern struct PIE_VECT_TABLE         PieVectTable;
extern volatile struct PIE_CTRL_REGS PieCtrlRegs;
extern volatile struct SYS_CTRL_REGS SysCtrlRegs;
extern struct EQEP_REGS              gHostEQep;
extern struct EQEP_REGS             *EQepRegs;
extern volatile unsigned short       gHostCpuCtl[7];
extern unsigned short                gMockCall[MC_COUNT];
extern unsigned long                 gMockArg[MC_COUNT];

/* ---- counters ------------------------------------------------------------ */

static long g_cases;
static long g_mism;
static long g_mismShown;

/* ------------------------------------------------------------------------- */
/* the compared image                                                       */
/* ------------------------------------------------------------------------- */

/* One line per observable: name, global, type. The same list drives the
 * struct, the save, the load and the human-readable diff, so a field can never
 * be compared in one place and forgotten in another. */
#define SH_LIST(X)                                                            \
    X(adc,            gADC,            ADC_STRUCT)                            \
    X(basePar,        gBasePar,        BASE_PAR_STRUCT)                       \
    X(curSamp,        gCurSamp,        IUVW_SAMPLING_STRUCT)                  \
    X(deadBand,       gDeadBand,       DEAD_BAND_STRUCT)                      \
    X(extendCmd,      gExtendCmd,      MAIN_COMMAND_EXTEND_UNION)             \
    X(ampTheta,       gIAmpTheta,      AMPTHETA_STRUCT)                       \
    X(imtq12,         gIMTQ12,         MT_STRUCT)                             \
    X(imtSetApply,    gIMTSetApply,    MT_STRUCT_Q24)                         \
    X(imtSetQ12,      gIMTSetQ12,      MT_STRUCT)                             \
    X(ipmInitPos,     gIPMInitPos,     IPM_INITPOS_PULSE_STR)                 \
    X(ipmPos,         gIPMPos,         IPM_POSITION_STRUCT)                   \
    X(ipmPosCheck,    gIPMPosCheck,    IPM_POS_CHECK_STRUCT)                  \
    X(iuvwq12,        gIUVWQ12,        UVW_STRUCT)                            \
    X(iuvwq24,        gIUVWQ24,        UVW_STRUCT_Q24)                        \
    X(inLose,         gInLose,         INPUT_LOSE_STRUCT)                     \
    X(invInfo,        gInvInfo,        INV_STRUCT)                            \
    X(lineCur,        gLineCur,        LINE_CURRENT_STRUCT)                   \
    X(mainCmd,        gMainCmd,        BASE_COMMAND_STRUCT)                   \
    X(mainStatus,     gMainStatus,     RUN_STATUS_STRUCT)                     \
    X(motorExtInfo,   gMotorExtInfo,   MOTOR_EXTERN_STRUCT)                   \
    X(motorExtPer,    gMotorExtPer,    MOTOR_EXTERN_STRUCT)                   \
    X(motorInfo,      gMotorInfo,      MOTOR_STRUCT)                          \
    X(outVolt,        gOutVolt,        OUT_VOLT_STRUCT)                       \
    X(ovUdc,          gOvUdc,          OVER_UDC_CTL_STRUCT)                   \
    X(pgData,         gPGData,         PG_DATA_STRUCT)                        \
    X(pmDecoup,       gPmDecoup,       PM_DECOUPLE)                           \
    X(powerTrq,       gPowerTrq,       MOTOR_POWER_TORQUE)                    \
    X(rotorTrans,     gRotorTrans,     ROTOR_TRANS_STRUCT)                    \
    X(udc,            gUDC,            UDC_STRUCT)                            \
    X(uvCoff,         gUVCoff,         UV_BIAS_COFF_STRUCT)                   \
    X(uvwpg,          gUVWPG,          IPM_UVW_PG_STRUCT)                     \
    X(vfPar,          gVFPar,          VF_INFO_STRUCT)                        \
    X(varAvr,         gVarAvr,         VAR_AVR_STRUCT)

struct SHARED {
#define DECL(name, glob, type) type name;
    SH_LIST(DECL)
#undef DECL
    struct EPWM_REGS      epwm1;
    struct EPWM_REGS      epwm2;
    struct EPWM_REGS      epwm3;
    struct EQEP_REGS      eqep1;
    struct EQEP_REGS      eqepHost;
    struct PIE_VECT_TABLE pieVect;
    struct PIE_CTRL_REGS  pieCtrl;
    struct SYS_CTRL_REGS  sysCtrl;
    struct EQEP_REGS     *eqepPtr;
    unsigned short        mockCall[MC_COUNT];
    unsigned long         mockArg[MC_COUNT];
    unsigned short        cpuCtl[7];
};

static struct SHARED sRef;
static struct SHARED sNew;
static struct SHARED sIn;

static void shared_save(struct SHARED *s)
{
    memset(s, 0, sizeof(*s));
#define SAVE(name, glob, type) s->name = glob;
    SH_LIST(SAVE)
#undef SAVE
    memcpy(&s->epwm1, (const void *)&EPwm1Regs, sizeof(s->epwm1));
    memcpy(&s->epwm2, (const void *)&EPwm2Regs, sizeof(s->epwm2));
    memcpy(&s->epwm3, (const void *)&EPwm3Regs, sizeof(s->epwm3));
    memcpy(&s->eqep1, (const void *)&EQep1Regs, sizeof(s->eqep1));
    memcpy(&s->eqepHost, (const void *)&gHostEQep, sizeof(s->eqepHost));
    memcpy(&s->pieVect, (const void *)&PieVectTable, sizeof(s->pieVect));
    memcpy(&s->pieCtrl, (const void *)&PieCtrlRegs, sizeof(s->pieCtrl));
    memcpy(&s->sysCtrl, (const void *)&SysCtrlRegs, sizeof(s->sysCtrl));
    s->eqepPtr = EQepRegs;
    memcpy(s->mockCall, (const void *)gMockCall, sizeof(s->mockCall));
    memcpy(s->mockArg, (const void *)gMockArg, sizeof(s->mockArg));
    memcpy(s->cpuCtl, (const void *)gHostCpuCtl, sizeof(s->cpuCtl));
}

static void shared_load(const struct SHARED *s)
{
#define LOAD(name, glob, type) glob = s->name;
    SH_LIST(LOAD)
#undef LOAD
    memcpy((void *)&EPwm1Regs, &s->epwm1, sizeof(s->epwm1));
    memcpy((void *)&EPwm2Regs, &s->epwm2, sizeof(s->epwm2));
    memcpy((void *)&EPwm3Regs, &s->epwm3, sizeof(s->epwm3));
    memcpy((void *)&EQep1Regs, &s->eqep1, sizeof(s->eqep1));
    memcpy((void *)&gHostEQep, &s->eqepHost, sizeof(s->eqepHost));
    memcpy((void *)&PieVectTable, &s->pieVect, sizeof(s->pieVect));
    memcpy((void *)&PieCtrlRegs, &s->pieCtrl, sizeof(s->pieCtrl));
    memcpy((void *)&SysCtrlRegs, &s->sysCtrl, sizeof(s->sysCtrl));
    EQepRegs = s->eqepPtr;
    memcpy((void *)gMockCall, s->mockCall, sizeof(s->mockCall));
    memcpy((void *)gMockArg, s->mockArg, sizeof(s->mockArg));
    memcpy((void *)gHostCpuCtl, s->cpuCtl, sizeof(s->cpuCtl));
}

/* ------------------------------------------------------------------------- */
/* dirty image                                                              */
/* ------------------------------------------------------------------------- */

static void dirty_all(void)
{
#define DIRTY(name, glob, type) memset((void *)&glob, 0xA5, sizeof(glob));
    SH_LIST(DIRTY)
#undef DIRTY
    memset((void *)&EPwm1Regs,    0xA5, sizeof(EPwm1Regs));
    memset((void *)&EPwm2Regs,    0xA5, sizeof(EPwm2Regs));
    memset((void *)&EPwm3Regs,    0xA5, sizeof(EPwm3Regs));
    memset((void *)&EQep1Regs,    0xA5, sizeof(EQep1Regs));
    memset((void *)&gHostEQep,    0xA5, sizeof(gHostEQep));
    memset((void *)&PieVectTable, 0xA5, sizeof(PieVectTable));
    memset((void *)&PieCtrlRegs,  0xA5, sizeof(PieCtrlRegs));
    memset((void *)&SysCtrlRegs,  0xA5, sizeof(SysCtrlRegs));
    memset((void *)gMockCall,     0,    sizeof(gMockCall));
    memset((void *)gMockArg,      0,    sizeof(gMockArg));
    memset((void *)gHostCpuCtl,   0,    sizeof(gHostCpuCtl));
    EQepRegs = &gHostEQep;
}

/* ------------------------------------------------------------------------- */
/* comparison                                                               */
/* ------------------------------------------------------------------------- */

static void dump_diff(const char *tag, const char *what,
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

static void cmp_img(const char *tag, const struct SHARED *a,
                    const struct SHARED *b)
{
    if (memcmp(a, b, sizeof(*a)) == 0)
    {
        return;
    }

    g_mism++;

#define CMP(name, glob, type)                                                 \
    if (memcmp(&a->name, &b->name, sizeof(a->name)) != 0)                     \
    {                                                                         \
        if (g_mismShown < 40)                                                 \
        {                                                                     \
            printf("MISMATCH %s: %s\n", tag, #name);                           \
            dump_diff(tag, #name, &a->name, &b->name, sizeof(a->name));       \
        }                                                                     \
        g_mismShown++;                                                        \
    }
    SH_LIST(CMP)
#undef CMP

    if (memcmp(&a->cpuCtl, &b->cpuCtl, sizeof(a->cpuCtl)) != 0)
    {
        if (g_mismShown < 40)
        {
            printf("MISMATCH %s: cpuCtl\n", tag);
            dump_diff(tag, "cpuCtl", &a->cpuCtl, &b->cpuCtl, sizeof(a->cpuCtl));
        }
        g_mismShown++;
    }
    if (memcmp(&a->epwm1, &b->epwm1, sizeof(a->epwm1)) != 0)
    {
        if (g_mismShown < 40)
        {
            printf("MISMATCH %s: epwm1\n", tag);
            dump_diff(tag, "epwm1", &a->epwm1, &b->epwm1, sizeof(a->epwm1));
        }
        g_mismShown++;
    }
    if (memcmp(&a->epwm2, &b->epwm2, sizeof(a->epwm2)) != 0)
    {
        if (g_mismShown < 40)
        {
            printf("MISMATCH %s: epwm2\n", tag);
            dump_diff(tag, "epwm2", &a->epwm2, &b->epwm2, sizeof(a->epwm2));
        }
        g_mismShown++;
    }
    if (memcmp(&a->epwm3, &b->epwm3, sizeof(a->epwm3)) != 0)
    {
        if (g_mismShown < 40)
        {
            printf("MISMATCH %s: epwm3\n", tag);
            dump_diff(tag, "epwm3", &a->epwm3, &b->epwm3, sizeof(a->epwm3));
        }
        g_mismShown++;
    }
    if (memcmp(&a->eqep1, &b->eqep1, sizeof(a->eqep1)) != 0)
    {
        if (g_mismShown < 40)
        {
            printf("MISMATCH %s: eqep1\n", tag);
            dump_diff(tag, "eqep1", &a->eqep1, &b->eqep1, sizeof(a->eqep1));
        }
        g_mismShown++;
    }
    if (memcmp(&a->eqepHost, &b->eqepHost, sizeof(a->eqepHost)) != 0)
    {
        if (g_mismShown < 40)
        {
            printf("MISMATCH %s: eqepHost\n", tag);
            dump_diff(tag, "eqepHost", &a->eqepHost, &b->eqepHost,
                      sizeof(a->eqepHost));
        }
        g_mismShown++;
    }
    if (memcmp(&a->pieVect, &b->pieVect, sizeof(a->pieVect)) != 0)
    {
        if (g_mismShown < 40)
        {
            printf("MISMATCH %s: pieVect\n", tag);
            dump_diff(tag, "pieVect", &a->pieVect, &b->pieVect,
                      sizeof(a->pieVect));
        }
        g_mismShown++;
    }
    if (memcmp(&a->pieCtrl, &b->pieCtrl, sizeof(a->pieCtrl)) != 0)
    {
        if (g_mismShown < 40)
        {
            printf("MISMATCH %s: pieCtrl\n", tag);
            dump_diff(tag, "pieCtrl", &a->pieCtrl, &b->pieCtrl,
                      sizeof(a->pieCtrl));
        }
        g_mismShown++;
    }
    if (memcmp(&a->sysCtrl, &b->sysCtrl, sizeof(a->sysCtrl)) != 0)
    {
        if (g_mismShown < 40)
        {
            printf("MISMATCH %s: sysCtrl\n", tag);
            dump_diff(tag, "sysCtrl", &a->sysCtrl, &b->sysCtrl,
                      sizeof(a->sysCtrl));
        }
        g_mismShown++;
    }
    if (a->eqepPtr != b->eqepPtr)
    {
        if (g_mismShown < 40)
        {
            printf("MISMATCH %s: eqepPtr  ref=%p new=%p\n", tag,
                   (const void *)a->eqepPtr, (const void *)b->eqepPtr);
        }
        g_mismShown++;
    }
    if (memcmp(a->mockCall, b->mockCall, sizeof(a->mockCall)) != 0)
    {
        if (g_mismShown < 40)
        {
            printf("MISMATCH %s: mockCall\n", tag);
            dump_diff(tag, "mockCall", a->mockCall, b->mockCall,
                      sizeof(a->mockCall));
        }
        g_mismShown++;
    }
    if (memcmp(a->mockArg, b->mockArg, sizeof(a->mockArg)) != 0)
    {
        if (g_mismShown < 40)
        {
            printf("MISMATCH %s: mockArg\n", tag);
            dump_diff(tag, "mockArg", a->mockArg, b->mockArg,
                      sizeof(a->mockArg));
        }
        g_mismShown++;
    }
}

/* ------------------------------------------------------------------------- */
/* one scenario                                                             */
/* ------------------------------------------------------------------------- */

enum {
    F_SYS2MS = 0, F_RUN2MS, F_SYS05MS, F_CHGPAR, F_INVPOWER,
    F_COUNT
};

/* per-entry-point scenario counter, asserted in main() */
static long g_fnCases[F_COUNT];

static void call_ref(int fn)
{
    switch (fn)
    {
        case F_SYS2MS:   ref_SystemParChg2Ms();   break;
        case F_RUN2MS:   ref_RunStateParChg2Ms(); break;
        case F_SYS05MS:  ref_SystemParChg05Ms();  break;
        case F_CHGPAR:   ref_ChangeMotorPar();    break;
        default:         ref_InvCalcPower();      break;
    }
}

static void call_new(int fn)
{
    switch (fn)
    {
        case F_SYS2MS:   new_SystemParChg2Ms();   break;
        case F_RUN2MS:   new_RunStateParChg2Ms(); break;
        case F_SYS05MS:  new_SystemParChg05Ms();  break;
        case F_CHGPAR:   new_ChangeMotorPar();    break;
        default:         new_InvCalcPower();      break;
    }
}

static void pin_divisors(void);

/* Run both sides on the current input image and compare. */
static void scn_run(int fn, const char *tag)
{
    pin_divisors();
    shared_save(&sIn);

    call_ref(fn);
    shared_save(&sRef);

    shared_load(&sIn);          /* undo the ref side's writes */
    call_new(fn);
    shared_save(&sNew);

    cmp_img(tag, &sRef, &sNew);
    g_cases++;
    g_fnCases[fn]++;
}

/* ------------------------------------------------------------------------- */
/* divisor pinning                                                          */
/* ------------------------------------------------------------------------- */

/* Every divisor the module divides by, forced non-zero AFTER the scenario's
 * pokes. The two that the module computes itself (m_BaseL in ChangeMotorPar,
 * ItRated in ChangeMotorPar) cannot be pinned directly, so the inputs that
 * feed them are constrained here instead. */
static void pin_divisors(void)
{
    if (gMotorInfo.CurrentGet == 0)  gMotorInfo.CurrentGet = 1000;
    if (gBasePar.FullFreq == 0)      gBasePar.FullFreq = 4000;
    if (gRotorTrans.Poles == 0)      gRotorTrans.Poles = 4;
    if (gInvInfo.InvCurrentOvload == 0) gInvInfo.InvCurrentOvload = 1500;
    if (gMotorInfo.Votage == 0)      gMotorInfo.Votage = 380;
    if (gMotorInfo.Current == 0)     gMotorInfo.Current = 2000;
    if (gBasePar.FullFreq01 == 0)    gBasePar.FullFreq01 = 400000;
    if (gMotorInfo.FreqPer == 0)     gMotorInfo.FreqPer = 1000;
    if (gMotorExtInfo.Rpm == 0)      gMotorExtInfo.Rpm = 1500;

    /* abs(gMainCmd.FreqSyn) is a divisor and FreqSyn is legitimately 0 at
     * standstill; on the C28x that is a defined saturated quotient, on the
     * host it is #DE. Keep it non-zero. */
    if (gMainCmd.FreqSyn == 0)       gMainCmd.FreqSyn = 1;

    /* m_BaseL = ((Votage*3678)/Current * 5000) / FullFreq01. The intermediate
     * `(Votage*3678)/Current` truncates to 16 bits before the multiply, so a
     * large Votage with a large Current can still give 0. */
    if ((long)(((Ulong)gMotorInfo.Votage * 3678) / gMotorInfo.Current) == 0)
    {
        gMotorInfo.Current = 1;
    }
    if (((((Ulong)gMotorInfo.Votage * 3678) / gMotorInfo.Current) * 5000)
        / gBasePar.FullFreq01 == 0)
    {
        gBasePar.FullFreq01 = 1;
    }

    /* ItRated = qsqrt(4096*4096 - I0*I0) with I0 = (gMotorExtInfo.I0<<12)/Current.
     * I0 == 4096 makes it zero and the next line divides by it. */
    {
        Ulong i0 = ((Ulong)gMotorExtInfo.I0 << 12) / gMotorInfo.Current;
        if (i0 >= 4096)
        {
            gMotorExtInfo.I0 = 0;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* coverage: one slot per intended branch, asserted in main()                */
/* ------------------------------------------------------------------------- */

#define COV_LIST(X)                                                          \
    X(sys_lt100) X(sys_lt200) X(sys_lt300) X(sys_lt400) X(sys_lt500)          \
    X(sys_ge500) X(sys_gtmax) X(sys_lemin) X(sys_gp1) X(sys_gp0)              \
    X(sys_uv220) X(sys_uv380) X(sys_uv22) X(sys_ud_lt12) X(sys_ud_gt24)       \
    X(sys_db500) X(sys_db400) X(sys_dbother)                                  \
    X(sys_fu0) X(sys_fu1) X(sys_fu2)                                          \
    X(run_dc) X(run_nodc) X(run_getpar) X(run_stop) X(run_ovf) X(run_novf)    \
    X(run_pm) X(run_notpm) X(run_zchg) X(run_zsame) X(run_pcal1) X(run_pcal0) \
    X(run_qswap)                                                              \
    X(s05_typechg) X(s05_typeeq) X(s05_pgchg) X(s05_pgeq) X(s05_q1) X(s05_q0) \
    X(s05_pm) X(s05_notpm)                                                    \
    X(chg_pm) X(chg_notpm) X(chg_it0) X(chg_it1)                              \
    X(pow_on) X(pow_off) X(pow_ge22) X(pow_lt22)

#define COV_ENUM(name) COV_##name,
enum { COV_LIST(COV_ENUM) COV_COUNT };
#undef COV_ENUM

static long g_cov[COV_COUNT];

#define COV_INC(name) (g_cov[COV_##name]++)

static const char *g_covName[COV_COUNT] = {
#define COV_NAME(name) #name,
    COV_LIST(COV_NAME)
#undef COV_NAME
};

static const char *g_fnName[F_COUNT] = {
    "SystemParChg2Ms", "RunStateParChg2Ms", "SystemParChg05Ms",
    "ChangeMotorPar", "InvCalcPower"
};

/* ------------------------------------------------------------------------- */
/* defaults                                                                 */
/* ------------------------------------------------------------------------- */

/* A self-consistent operating point: 380 V / 2000 A inverter, 380 V motor at
 * 50 Hz, 4 poles. Every scenario starts here and then pokes one dimension, so
 * a mismatch points at the poked branch rather than at the defaults. */
static void setup_defaults(void)
{
    dirty_all();

    gInvInfo.InvTypeSet     = 8;
    gInvInfo.GpTypeSet      = 0;
    gInvInfo.InvType        = 8;
    gInvInfo.InvVoltageType = INV_VOLTAGE_380V;
    gInvInfo.GPType         = 0;
    gInvInfo.InvCurrent     = 2000;
    gInvInfo.InvCurrentOvload = 2400;
    gInvInfo.LowUdcCoff     = 1000;
    gInvInfo.CurrentCoff    = 1000;
    gInvInfo.UDCCoff        = 1000;
    gInvInfo.InvTypeApply   = 8;

    gMotorInfo.CurrentGet   = 2000;
    gMotorInfo.Current      = 2000;
    gMotorInfo.Votage       = 380;
    gMotorInfo.Frequency    = 50;
    gMotorInfo.FreqPer      = 1000;
    gMotorInfo.MotorType    = MOTOR_TYPE_PM;

    gBasePar.MaxFreq        = 4000;
    gBasePar.FullFreq       = 4000;
    gBasePar.FullFreq01     = 400000;

    gMainCmd.si2puCoeff     = 1;
    gMainCmd.pu2siCoeff     = 100;
    gMainCmd.FreqSyn        = 1000;
    gMainCmd.FreqDesired    = 1000;
    gMainCmd.FreqSet        = 1000;
    gMainCmd.FreqReal       = 0;
    gMainCmd.FreqRealFilt   = 0;

    gMotorExtInfo.Poles     = 4;
    gMotorExtInfo.Rpm       = 1500;
    gMotorExtInfo.R1        = 100;
    gMotorExtInfo.R2        = 100;
    gMotorExtInfo.LM        = 200;
    gMotorExtInfo.L0        = 20;
    gMotorExtInfo.L1        = 220;
    gMotorExtInfo.L2        = 220;
    gMotorExtInfo.I0        = 500;
    gMotorExtInfo.RsPm      = 100;
    gMotorExtInfo.LD        = 200;
    gMotorExtInfo.LQ        = 220;
    gMotorExtInfo.BemfVolt  = 350;
    gMotorExtInfo.UnitCoff  = 1;

    /* gMotorExtPer is the module's main OUTPUT (R1/R2/L1/L2/LM/L0/I0/ItRated/
     * LD/LQ/FluxRotor/Rpm/RatedComp). Leaving it at the 0xA5 dirt made every
     * output comparison start from a garbage base, which is fine for "did the
     * module write it" but useless as an INPUT: RunStateParChg2Ms reads
     * gMotorExtPer.I0 to build IoVsFreq, and with I0 = 0xA5A5 the two branches
     * of the `m_AbsFreq < FreqPer` test could agree by accident. Seed it with a
     * consistent per-unit operating point instead. */
    gMotorExtPer.UnitCoff   = 1;
    gMotorExtPer.R1         = 100;
    gMotorExtPer.R2         = 100;
    gMotorExtPer.L0         = 20;
    gMotorExtPer.LM         = 200;
    gMotorExtPer.I0         = 500;
    gMotorExtPer.IoVsFreq   = 500;
    gMotorExtPer.Rpm        = 1000;
    gMotorExtPer.RatedComp  = 100;
    gMotorExtPer.Poles      = 4;
    gMotorExtPer.L1         = 220;
    gMotorExtPer.L2         = 220;
    gMotorExtPer.RsPm       = 100;
    gMotorExtPer.LD         = 200;
    gMotorExtPer.LQ         = 220;
    gMotorExtPer.BemfVolt   = 350;
    gMotorExtPer.FluxRotor  = 1000;
    gMotorExtPer.FluxRotor1 = 1000;
    gMotorExtPer.ItRated    = 4000;
    gMotorExtPer.FluxLeakCurveGain = 1000;

    gRotorTrans.Poles       = 4;
    gRotorTrans.PolesRatio  = 256;

    gUDC.uDC                = 5400;
    gUDC.uDcCalMax          = 5400;
    gUDC.uDCADCoff          = 1000;
    gUDC.Coff               = 1000;

    gADC.DelaySet           = 100;
    gADC.DelayApply         = 600;

    gDeadBand.DeadTimeSet   = 10;
    gDeadBand.DeadBand      = 192;
    gDeadBand.Comp          = 78;
    gDeadBand.InvCurFilter  = 0;

    gLineCur.CurPer         = 1000;
    gLineCur.CurPerFilter   = 0;
    gLineCur.CurBaseInv     = 0;
    gLineCur.CurTorque      = 1000;

    gIAmpTheta.Amp          = 1000;
    gIAmpTheta.PowerAngle   = 1000;
    gIAmpTheta.CurTmpM      = 500;
    gIAmpTheta.CurTmpT      = 500;

    gIMTQ12.M               = 500;
    gIMTQ12.T               = 500;
    gIMTSetApply.M          = 500;
    gIMTSetApply.T          = 500;

    gOutVolt.MaxOutVolt     = 3000;
    gOutVolt.VoltApply      = 3000;

    gVFPar.ovPoint          = 100;
    gVFPar.ovPointCoff      = 100;
    gVFPar.VFOverExc        = 1000;

    gMainStatus.RunStep     = 0;
    gMainStatus.ParaCalTimes = 0;
    gMainStatus.StatusWord.all = 0;

    gPGData.PGMode          = 1;
    gPGData.PGType          = PG_TYPE_NULL;
    gPGData.PGTypeGetFromFun = PG_TYPE_NULL;
    gPGData.QEPIndex        = QEP_SELECT_1;
    gPGData.SpeedDir        = 0;

    gExtendCmd.bit.FreqUint = 0;
    gExtendCmd.bit.QepIndex = QEP_SELECT_1;

    gPowerTrq.rpItRated     = 1000;
    gPowerTrq.anglePF       = 0;
    gPowerTrq.Cur_Ft4       = 1000;
    gPowerTrq.InvPowerPU    = 0;
    gPowerTrq.InvPower_si   = 0;
    gPowerTrq.TrqOut_pu     = 0;

    gCurSamp.Coff           = 1000;
    gUVCoff.UDivV           = 4096;
    gUVCoff.UDivVGet        = 4096;
    gVarAvr.CoffApply       = 0;
    gOvUdc.Limit            = 7500;

    gIPMPos.RotorZero       = 0;
    gIPMPos.RotorZeroGet    = 0;
    gIPMPos.RotorPos        = 0;
    gIPMPos.ZeroPosLast     = 0;
    gIPMPos.AbzErrPos       = 0;

    gUVWPG.UvwZeroPos       = 0;
    gUVWPG.UvwZeroPos_deg   = 0;
    gUVWPG.UvwZIntErr       = 0;
    gUVWPG.UvwZIntErr_deg   = 0;

    gIPMPosCheck.UvwStopErr = 0;
    gIPMPosCheck.UvwStopErr_deg = 0;
    gPmDecoup.EnableDcp     = 1;
}

/* ------------------------------------------------------------------------- */
/* phase 1: SystemParChg2Ms                                                 */
/* ------------------------------------------------------------------------- */

/* InvType is NOT an input. Every arm of the band chain computes it itself:
 *
 *     gInvInfo.InvType = gInvInfo.InvTypeSet - 100;   (220T arm, etc.)
 *
 * so poking gInvInfo.InvType directly is dead -- the module overwrites it
 * before any of the interesting branches read it. The only way to drive it is
 * through InvTypeSet: InvTypeSet = band_base + InvType, with band_base one of
 * 0 / 100 / 200 / 300 / 400 / 500.
 *
 * (The first version of this bench poked InvType and eight mutations of that
 * field came back MISSED for exactly this reason.)
 */
#define INV_BASE_380T    0
#define INV_BASE_220T  100
#define INV_BASE_220S  200
#define INV_BASE_480   300
#define INV_BASE_690   400
#define INV_BASE_1140  500

static void set_inv(Uint16 base, Uint16 invtype)
{
    gInvInfo.InvTypeSet = (Uint)(base + invtype);
}

static void phase_sys2ms(void)
{
    /* The six inverter bands, plus every boundary value. 100/200/300/400/500
     * are the `else if` thresholds; 99/199/... are the last value of the band
     * below. */
    static const Uint16 band[] = { 0, 99, 100, 199, 200, 299, 300, 399,
                                   400, 499, 500, 600, 0xFFFF };
    unsigned i;

    for (i = 0; i < sizeof band / sizeof band[0]; i++)
    {
        setup_defaults();
        gInvInfo.InvTypeSet = band[i];
        COV_INC(sys_lt100);
        COV_INC(sys_lt200);
        COV_INC(sys_lt300);
        COV_INC(sys_lt400);
        COV_INC(sys_lt500);
        COV_INC(sys_ge500);
        scn_run(F_SYS2MS, "sys-band");
    }

    /* InvType clamped to [minType, maxType]. Driven through InvTypeSet so the
     * module's own `InvType = InvTypeSet - base` produces the value. 380T has
     * minType 8 / maxType 34, so 0..35 brackets both clamps. */
    {
        static const Uint16 it[] = { 0, 7, 8, 9, 21, 22, 23, 24, 25, 26,
                                     33, 34, 35, 40 };
        for (i = 0; i < sizeof it / sizeof it[0]; i++)
        {
            setup_defaults();
            set_inv(INV_BASE_380T, it[i]);
            if (it[i] > 34) COV_INC(sys_gtmax);
            if (it[i] <= 8) COV_INC(sys_lemin);
            scn_run(F_SYS2MS, "sys-clamp");
        }
    }

    /* GPType 0/1 selects InvTypeApply = InvType or InvType-1, and the
     * `InvType == 22 && GPType != 1` rescale. 22 is reachable only through
     * InvTypeSet, and only in a band whose clamp does not move it. */
    {
        static const Uint16 gp[] = { 0, 1 };
        static const Uint16 it[] = { 21, 22, 23 };
        unsigned j, k;
        for (j = 0; j < 2; j++)
        for (k = 0; k < sizeof it / sizeof it[0]; k++)
        {
            setup_defaults();
            gInvInfo.GpTypeSet = gp[j];
            set_inv(INV_BASE_380T, it[k]);
            if (gp[j]) { COV_INC(sys_gp1); } else { COV_INC(sys_gp0); }
            if (it[k] == 22 && gp[j] == 0) { COV_INC(sys_uv22); }
            scn_run(F_SYS2MS, "sys-gp");
        }
    }

    /* UnitCoff: 220 V uses `> 19`, everything else `> 21`. 20 and 21 are the
     * two values where the two thresholds disagree, so both must be driven in
     * both voltage families. */
    {
        static const Uint16 set[] = { 17, 18, 19, 20, 21, 22, 23 };
        for (i = 0; i < sizeof set / sizeof set[0]; i++)
        {
            setup_defaults();
            set_inv(INV_BASE_220T, set[i]);
            COV_INC(sys_uv220);
            scn_run(F_SYS2MS, "sys-unit220");

            setup_defaults();
            set_inv(INV_BASE_380T, set[i]);
            COV_INC(sys_uv380);
            scn_run(F_SYS2MS, "sys-unit380");
        }
    }

    /* m_UData clamp to [12, 24] feeding the dead-band table index. InvTypeApply
     * is InvType (GPType 1) or InvType-1 (GPType 0), so 12..26 covers both
     * ends of the clamp for both settings. */
    {
        static const Uint16 ap[] = { 0, 11, 12, 13, 14, 23, 24, 25, 26, 34 };
        for (i = 0; i < sizeof ap / sizeof ap[0]; i++)
        {
            setup_defaults();
            gInvInfo.GpTypeSet = 0;
            set_inv(INV_BASE_380T, ap[i]);
            if (ap[i] - 1 < 12) COV_INC(sys_ud_lt12);
            if (ap[i] - 1 > 24) COV_INC(sys_ud_gt24);
            scn_run(F_SYS2MS, "sys-udata");
        }
    }

    /* Dead-band arms: >= 500, >= 400, else. */
    {
        static const Uint16 set[] = { 399, 400, 499, 500, 8 };
        for (i = 0; i < sizeof set / sizeof set[0]; i++)
        {
            setup_defaults();
            gInvInfo.InvTypeSet = set[i];
            if (set[i] >= 500)      COV_INC(sys_db500);
            else if (set[i] >= 400) COV_INC(sys_db400);
            else                    COV_INC(sys_dbother);
            scn_run(F_SYS2MS, "sys-db");
        }
    }

    /* FreqUint 0 / 1 / other -> si2puCoeff, pu2siCoeff. */
    {
        static const Uint16 fu[] = { 0, 1, 2 };
        for (i = 0; i < 3; i++)
        {
            setup_defaults();
            gExtendCmd.bit.FreqUint = fu[i];
            if (fu[i] == 0)      COV_INC(sys_fu0);
            else if (fu[i] == 1) COV_INC(sys_fu1);
            else                 COV_INC(sys_fu2);
            scn_run(F_SYS2MS, "sys-fu");
        }
    }

    /* CurBaseCoff = min((Current<<8)/CurrentGet, 32767). The clamp only bites
     * above 32767, i.e. when Current/CurrentGet > 128.
     *
     * `Current` is max(CurrentGet, InvCurrent>>2) -- computed by the module one
     * line earlier -- and `InvCurrent` is itself read out of the inverter table
     * by InvTypeApply. So the only way to a large ratio is a SMALL CurrentGet
     * combined with a LARGE table entry, and the entry has to be selected
     * through InvTypeSet: poking gInvInfo.InvCurrent directly is a dead write,
     * exactly like poking gInvInfo.InvType. (The first version of this block
     * did poke InvCurrent and the "32767 -> 32766" mutation stayed MISSED.)
     *
     * 380T's largest entry is gInvCurrentTable380T[26] = 8200, reached with
     * GPType 1 and InvType 34 -> m_UData = 2050. */
    {
        static const Uint16 cg[] = { 1, 2, 15, 100, 2050 };
        for (i = 0; i < sizeof cg / sizeof cg[0]; i++)
        {
            setup_defaults();
            gInvInfo.GpTypeSet = 1;          /* InvTypeApply == InvType */
            set_inv(INV_BASE_380T, 34);      /* -> InvCurrent = 8200 */
            gMotorInfo.CurrentGet = cg[i];
            scn_run(F_SYS2MS, "sys-curbase");
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase 2: RunStateParChg2Ms                                               */
/* ------------------------------------------------------------------------- */

static void phase_run2ms(void)
{
    unsigned i;

    /* IoVsFreq: `m_AbsFreq < FreqPer ? I0 : (I0*FreqPer)/m_AbsFreq`.
     *
     * The two branches only differ when m_AbsFreq != FreqPer, and the bench
     * used to drive FreqSyn = FreqPer = 1000 for every scenario, which makes
     * the quotient exactly I0 and the test unobservable. Sweep both sides of
     * the equality.
     *
     * (`<` vs `<=` is a provably EQUIVALENT mutation and was removed from the
     * inventory: at m_AbsFreq == FreqPer the quotient is exactly I0, so both
     * branches store the same value. See _mpc_add_negctl.py.) */
    {
        static const Uint16 f[] = { 1, 500, 999, 1000, 1001, 2000, 0x7FFF };
        for (i = 0; i < sizeof f / sizeof f[0]; i++)
        {
            setup_defaults();
            gMotorExtPer.I0 = 500;
            gMotorInfo.FreqPer = 1000;
            gMainCmd.FreqSyn = f[i];
            scn_run(F_RUN2MS, "run-iovsfreq");
        }

        /* Negative FreqSyn: abs() folds it, and the divider is what abs()
         * feeds, so a sign-handling slip shows up here. */
        setup_defaults();
        gMotorExtPer.I0 = 500;
        gMainCmd.FreqSyn = -500;
        scn_run(F_RUN2MS, "run-iovsfreq-neg");
    }

    /* StartDC / StopDC gate the Amp rescale. */
    {
        static const Uint16 v[] = { 0, 1 };
        for (i = 0; i < 2; i++)
        {
            setup_defaults();
            gMainCmd.Command.bit.StartDC = v[i];
            if (v[i]) COV_INC(run_dc); else COV_INC(run_nodc);
            scn_run(F_RUN2MS, "run-dc");
        }
    }

    /* STATUS_GET_PAR -> UDivV = 4096, else the scaled form. */
    {
        setup_defaults();
        gMainStatus.RunStep = STATUS_GET_PAR;
        COV_INC(run_getpar);
        scn_run(F_RUN2MS, "run-getpar");

        setup_defaults();
        gMainStatus.RunStep = 3;
        scn_run(F_RUN2MS, "run-notgetpar");
    }

    /* Over-voltage recalibration: |uDC - uDcCalMax| > 200 OR RunStep == STOP.
     * 200 is the boundary, so 200 must be fed from both sides. */
    {
        static const long d[] = { 0, 199, 200, 201, -199, -200, -201 };
        for (i = 0; i < sizeof d / sizeof d[0]; i++)
        {
            setup_defaults();
            gUDC.uDcCalMax = 5400;
            gUDC.uDC = 5400 + d[i];
            if (d[i] > 200 || d[i] < -200) COV_INC(run_ovf);
            else                            COV_INC(run_novf);
            scn_run(F_RUN2MS, "run-ovf");
        }

        setup_defaults();
        gMainStatus.RunStep = STATUS_STOP;
        COV_INC(run_stop);
        scn_run(F_RUN2MS, "run-stop");
    }

    /* Motor type: PM block vs not. */
    {
        setup_defaults();
        gMotorInfo.MotorType = MOTOR_TYPE_PM;
        COV_INC(run_pm);
        scn_run(F_RUN2MS, "run-pm");

        setup_defaults();
        gMotorInfo.MotorType = MOTOR_TYPE_IM;
        COV_INC(run_notpm);
        scn_run(F_RUN2MS, "run-notpm");
    }

    /* The PM block's re-position call: PGMode == 0, ParaCalTimes == 1,
     * RunStep != GET_PAR, ZeroPosLast != RotorZero. RotorZero is recomputed
     * just above, so feed RotorZeroGet to move it. */
    {
        setup_defaults();
        gMotorInfo.MotorType = MOTOR_TYPE_PM;
        gPGData.PGMode = 0;
        gMainStatus.ParaCalTimes = 1;
        gIPMPos.ZeroPosLast = 0;
        gIPMPos.RotorZeroGet = 100;      /* -> RotorZero = 1820 */
        gIPMPos.RotorPos = 500;
        COV_INC(run_pcal1);
        COV_INC(run_zchg);
        scn_run(F_RUN2MS, "run-zchg");

        setup_defaults();
        gMotorInfo.MotorType = MOTOR_TYPE_PM;
        gPGData.PGMode = 0;
        gMainStatus.ParaCalTimes = 0;
        gIPMPos.ZeroPosLast = 0;
        gIPMPos.RotorZeroGet = 100;
        COV_INC(run_pcal0);
        scn_run(F_RUN2MS, "run-pcal0");

        /* ZeroPosLast already equals the recomputed RotorZero -> no call. */
        setup_defaults();
        gMotorInfo.MotorType = MOTOR_TYPE_PM;
        gPGData.PGMode = 0;
        gMainStatus.ParaCalTimes = 1;
        gIPMPos.RotorZeroGet = 100;
        gIPMPos.ZeroPosLast = (Uint)(((Ulong)100 * 18641L) >> 10);
        COV_INC(run_zsame);
        scn_run(F_RUN2MS, "run-zsame");

        /* The increment `RotorZero - ZeroPosLast` is only observable when
         * NEITHER operand is zero: with ZeroPosLast == 0 the difference and
         * the sum are literally the same number, so a `-` -> `+` slip stays
         * invisible. That is exactly how MC121 survived the first round. */
        setup_defaults();
        gMotorInfo.MotorType = MOTOR_TYPE_PM;
        gPGData.PGMode = 0;
        gMainStatus.ParaCalTimes = 1;
        gIPMPos.RotorZeroGet = 100;      /* -> RotorZero = 1820 */
        gIPMPos.ZeroPosLast = 300;
        gIPMPos.RotorPos = 500;
        COV_INC(run_zchg);
        scn_run(F_RUN2MS, "run-zdelta");

        /* Same call with a RotorZero below ZeroPosLast, so the increment is
         * negative -- the sign of the result is part of what is compared. */
        setup_defaults();
        gMotorInfo.MotorType = MOTOR_TYPE_PM;
        gPGData.PGMode = 0;
        gMainStatus.ParaCalTimes = 1;
        gIPMPos.RotorZeroGet = 20;       /* -> RotorZero = 364 */
        gIPMPos.ZeroPosLast = 3000;
        gIPMPos.RotorPos = 500;
        COV_INC(run_zchg);
        scn_run(F_RUN2MS, "run-zdelta-neg");
    }

    /* Fixed-point scaling of the position / angle errors.
     *
     * `x * 18641 >> 10` and `x * 180 >> 15` both round the coefficient's last
     * bit away for small inputs: 100*18641>>10 == 100*18642>>10, and
     * 50*180>>15 == 50*180>>14 == 0. The first version of this bench only ever
     * fed 0..1000-range values and three shift mutations plus two coefficient
     * mutations came back MISSED for that reason alone. Drive the operands
     * large enough that the LSB of the result moves, and both signs. */
    {
        static const Uint16 rz[] = { 1024, 2000, 4000, 20000, 30000 };
        static const Uint16 deg[] = { 1024, 2000, 4000, 20000, 30000 };
        static const int err[] = { 0, 91, 92, 182, 1000, -1000, 30000, -30000 };

        for (i = 0; i < sizeof rz / sizeof rz[0]; i++)
        {
            setup_defaults();
            gMotorInfo.MotorType = MOTOR_TYPE_PM;
            gIPMPos.RotorZeroGet = rz[i];
            scn_run(F_RUN2MS, "run-rotzero");
        }

        for (i = 0; i < sizeof deg / sizeof deg[0]; i++)
        {
            setup_defaults();
            gMotorInfo.MotorType = MOTOR_TYPE_PM;
            gUVWPG.UvwZeroPos_deg = deg[i];
            scn_run(F_RUN2MS, "run-uvwzero");
        }

        for (i = 0; i < sizeof err / sizeof err[0]; i++)
        {
            setup_defaults();
            gMotorInfo.MotorType = MOTOR_TYPE_PM;
            gUVWPG.UvwZIntErr = err[i];
            gIPMPosCheck.UvwStopErr = err[i];
            gIPMPos.AbzErrPos = err[i];
            scn_run(F_RUN2MS, "run-angerr");
        }
    }

    /* QDECCTL.SWAP is written unless RunStep == GET_PAR. */
    {
        static const Uint16 d[] = { 0, 1 };
        for (i = 0; i < 2; i++)
        {
            setup_defaults();
            gPGData.SpeedDir = d[i];
            gMainStatus.RunStep = 3;
            COV_INC(run_qswap);
            scn_run(F_RUN2MS, "run-qswap");
        }
    }

    /* ovPoint * ovPointCoff, clamped by the six InvTypeSet bands.
     *
     * The product has to sit ABOVE the largest clamp (19000) for the band
     * thresholds to be observable at all, and it has to stay inside `int`
     * (16-bit: 32767) to avoid the wrap described below. 200*100 = 20000 does
     * both, and it separates all five clamps 7500 / 3700 / 8300 / 13000 /
     * 19000 from one another.
     *
     * `gOvUdc.Limit` is `int`, and `ovPoint * ovPointCoff` is a Uint*Uint
     * product that promotes to 16-bit `unsigned int` on the target and to
     * 32-bit `int` on the host -- both end up storing the same 16-bit bit
     * pattern, but anything above 32767 lands NEGATIVE. 200*200 = 40000 is
     * kept as a scenario precisely because it exercises that: the clamps are
     * then never taken (`Limit > 3700` is false for -25536), which is real
     * target behaviour that the rewrite must reproduce. It is also why the
     * first version of this sweep -- which used 200*200 for every band -- could
     * not see MC138 at all. */
    {
        static const Uint16 set[] = { 8, 99, 100, 299, 300, 399, 400, 499,
                                      500, 600 };
        static const Uint16 pt[][2] = { { 200, 100 }, { 50, 50 }, { 200, 200 } };
        unsigned j;
        for (j = 0; j < sizeof pt / sizeof pt[0]; j++)
        for (i = 0; i < sizeof set / sizeof set[0]; i++)
        {
            setup_defaults();
            gInvInfo.InvTypeSet = set[i];
            gVFPar.ovPoint = pt[j][0];
            gVFPar.ovPointCoff = pt[j][1];
            scn_run(F_RUN2MS, "run-ovlimit");
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase 3: SystemParChg05Ms                                                */
/* ------------------------------------------------------------------------- */

static void phase_sys05ms(void)
{
    unsigned i;

    /* LastMotorType vs MotorType. */
    {
        setup_defaults();
        gMotorInfo.LastMotorType = MOTOR_TYPE_IM;
        gMotorInfo.MotorType = MOTOR_TYPE_PM;
        COV_INC(s05_typechg);
        scn_run(F_SYS05MS, "s05-typechg");

        setup_defaults();
        gMotorInfo.LastMotorType = MOTOR_TYPE_PM;
        gMotorInfo.MotorType = MOTOR_TYPE_PM;
        COV_INC(s05_typeeq);
        scn_run(F_SYS05MS, "s05-typeeq");
    }

    /* PGType vs PGTypeGetFromFun. */
    {
        static const Uint16 t[] = { PG_TYPE_NULL, PG_TYPE_ABZ, PG_TYPE_UVW };
        for (i = 0; i < 3; i++)
        {
            setup_defaults();
            gPGData.PGType = t[i];
            gPGData.PGTypeGetFromFun = (Uint)(t[(i + 1) % 3]);
            COV_INC(s05_pgchg);
            scn_run(F_SYS05MS, "s05-pgchg");
        }

        setup_defaults();
        gPGData.PGType = PG_TYPE_ABZ;
        gPGData.PGTypeGetFromFun = PG_TYPE_ABZ;
        COV_INC(s05_pgeq);
        scn_run(F_SYS05MS, "s05-pgeq");
    }

    /* QEPIndex: 0 (no switch), 1 (EQep1 + PIE + clock), and a value that is
     * neither -> only InitSetQEP. */
    {
        static const Uint16 q[] = { 0, 1, 2 };
        for (i = 0; i < 3; i++)
        {
            setup_defaults();
            gPGData.QEPIndex = (QEP_INDEX_ENUM_STRUCT)((i + 1) % 3);
            gExtendCmd.bit.QepIndex = q[i];
            if (q[i] == 1)      COV_INC(s05_q1);
            else if (q[i] == 0) COV_INC(s05_q0);
            scn_run(F_SYS05MS, "s05-qep");
        }
    }

    /* QEINT.all: 0x0400 only for PM + PGMode 0. */
    {
        setup_defaults();
        gMotorInfo.MotorType = MOTOR_TYPE_PM;
        gPGData.PGMode = 0;
        COV_INC(s05_pm);
        scn_run(F_SYS05MS, "s05-qeint-pm");

        setup_defaults();
        gMotorInfo.MotorType = MOTOR_TYPE_IM;
        gPGData.PGMode = 0;
        COV_INC(s05_notpm);
        scn_run(F_SYS05MS, "s05-qeint-notpm");

        setup_defaults();
        gMotorInfo.MotorType = MOTOR_TYPE_PM;
        gPGData.PGMode = 1;
        scn_run(F_SYS05MS, "s05-qeint-mode1");
    }
}

/* ------------------------------------------------------------------------- */
/* phase 4: ChangeMotorPar                                                  */
/* ------------------------------------------------------------------------- */

static void phase_chgpar(void)
{
    unsigned i;

    /* Non-PM: R1/R2/L1/L2/LM/L0/I0/ItRated/rpItRated. */
    {
        static const Uint16 cur[] = { 100, 500, 2000, 4000 };
        for (i = 0; i < sizeof cur / sizeof cur[0]; i++)
        {
            setup_defaults();
            gMotorInfo.MotorType = MOTOR_TYPE_IM;
            gMotorInfo.Current = cur[i];
            COV_INC(chg_notpm);
            scn_run(F_CHGPAR, "chg-notpm");
        }
    }

    /* PM: RsPm / LD / LQ / BemfVolt / FluxRotor. */
    {
        static const Uint16 bv[] = { 0, 1, 100, 350, 1000 };
        for (i = 0; i < sizeof bv / sizeof bv[0]; i++)
        {
            setup_defaults();
            gMotorInfo.MotorType = MOTOR_TYPE_PM;
            gMotorExtInfo.BemfVolt = bv[i];
            COV_INC(chg_pm);
            scn_run(F_CHGPAR, "chg-pm");
        }
    }

    /* The PM arm reads RsPm where the induction arm reads R1. Both defaulted
     * to 100, so picking the wrong one was invisible; sweep them apart, in
     * both orders. */
    {
        static const Uint16 rs[] = { 50, 100, 250, 1000 };
        static const Uint16 r1[] = { 100, 250 };
        unsigned j;
        for (j = 0; j < sizeof r1 / sizeof r1[0]; j++)
        for (i = 0; i < sizeof rs / sizeof rs[0]; i++)
        {
            setup_defaults();
            gMotorInfo.MotorType = MOTOR_TYPE_PM;
            gMotorExtInfo.RsPm = rs[i];
            gMotorExtInfo.R1 = r1[j];
            COV_INC(chg_pm);
            scn_run(F_CHGPAR, "chg-rspm");
        }
    }

    /* ItRated near zero: I0 close to 4096 in per-unit. I0 = (I0raw<<12)/Current,
     * so I0raw = Current gives exactly 4096. pin_divisors() then forces I0raw
     * to 0, which is the only value the host can run. */
    {
        setup_defaults();
        gMotorInfo.MotorType = MOTOR_TYPE_IM;
        gMotorExtInfo.I0 = gMotorInfo.Current;      /* I0 == 4096 -> clamp */
        COV_INC(chg_it0);
        scn_run(F_CHGPAR, "chg-it0");

        setup_defaults();
        gMotorInfo.MotorType = MOTOR_TYPE_IM;
        gMotorExtInfo.I0 = 100;
        COV_INC(chg_it1);
        scn_run(F_CHGPAR, "chg-it1");
    }

    /* Poles / RatedComp sweep: Rpm and Frequency decide the rounding. */
    {
        static const Uint16 rpm[] = { 1, 1500, 3000, 0x7FFF };
        static const Uint16 frq[] = { 1, 50, 400 };
        unsigned j, k;
        for (j = 0; j < sizeof rpm / sizeof rpm[0]; j++)
        for (k = 0; k < sizeof frq / sizeof frq[0]; k++)
        {
            setup_defaults();
            gMotorExtInfo.Rpm = rpm[j];
            gMotorInfo.Frequency = frq[k];
            scn_run(F_CHGPAR, "chg-poles");
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase 5: InvCalcPower                                                    */
/* ------------------------------------------------------------------------- */

static void phase_invpower(void)
{
    unsigned i;

    {
        static const Uint16 on[] = { 0, 1 };
        for (i = 0; i < 2; i++)
        {
            setup_defaults();
            gMainStatus.StatusWord.bit.StartStop = on[i];
            if (on[i]) COV_INC(pow_on); else COV_INC(pow_off);
            scn_run(F_INVPOWER, "pow-onoff");
        }
    }

    /* InvTypeApply >= 22 selects the un-scaled m_PowerN. */
    {
        static const Uint16 ap[] = { 8, 21, 22, 23 };
        for (i = 0; i < sizeof ap / sizeof ap[0]; i++)
        {
            setup_defaults();
            gMainStatus.StatusWord.bit.StartStop = 1;
            gInvInfo.InvTypeApply = ap[i];
            if (ap[i] >= 22) COV_INC(pow_ge22); else COV_INC(pow_lt22);
            scn_run(F_INVPOWER, "pow-type");
        }
    }

    /* Angle / current sweep so the two filters and qsin are exercised. */
    {
        static const int ang[] = { 0, 1, 8192, 16383, 16384, 24576, 32767,
                                   -1, -8192, -16384, -32768 };
        for (i = 0; i < sizeof ang / sizeof ang[0]; i++)
        {
            setup_defaults();
            gMainStatus.StatusWord.bit.StartStop = 1;
            gIAmpTheta.PowerAngle = ang[i];
            gPowerTrq.anglePF = 1000;
            scn_run(F_INVPOWER, "pow-angle");
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase 6: random                                                          */
/* ------------------------------------------------------------------------- */

static unsigned long g_rng;

static unsigned short rnd16(void)
{
    g_rng = g_rng * 1103515245UL + 12345UL;
    return (unsigned short)((g_rng >> 16) & 0xFFFF);
}

static void phase_random(long n)
{
    long i;

    for (i = 0; i < n; i++)
    {
        setup_defaults();

        gInvInfo.InvTypeSet   = rnd16();
        gInvInfo.GpTypeSet    = (unsigned short)(rnd16() & 1);
        gInvInfo.InvType      = rnd16();
        gInvInfo.InvTypeApply = rnd16();
        gInvInfo.GPType       = (unsigned short)(rnd16() & 1);
        gInvInfo.InvVoltageType = (unsigned short)(rnd16() & 1);
        gInvInfo.InvCurrent   = rnd16();
        gInvInfo.InvCurrentOvload = rnd16();
        gInvInfo.LowUdcCoff   = rnd16();
        gInvInfo.CurrentCoff  = rnd16();
        gInvInfo.UDCCoff      = rnd16();

        gMotorInfo.CurrentGet = rnd16();
        gMotorInfo.Current    = rnd16();
        gMotorInfo.Votage     = rnd16();
        gMotorInfo.Frequency  = rnd16();
        gMotorInfo.FreqPer    = rnd16();
        gMotorInfo.MotorType  = (unsigned short)(rnd16() & 1);
        gMotorInfo.LastMotorType = (unsigned short)(rnd16() & 1);

        gBasePar.MaxFreq      = rnd16();
        gBasePar.FullFreq     = rnd16();
        gBasePar.FullFreq01   = rnd16();

        gMainCmd.FreqSyn      = (int)rnd16();
        gMainCmd.FreqDesired  = (int)rnd16();
        gMainCmd.FreqSet      = (int)rnd16();
        gMainCmd.FreqReal     = (int)rnd16();
        gMainCmd.FreqRealFilt = (int)rnd16();
        gMainCmd.si2puCoeff   = rnd16();
        gMainCmd.pu2siCoeff   = rnd16();
        gMainCmd.Command.bit.StartDC = (unsigned short)(rnd16() & 1);
        gMainCmd.Command.bit.StopDC  = (unsigned short)(rnd16() & 1);

        gMotorExtInfo.Poles   = rnd16();
        gMotorExtInfo.Rpm     = rnd16();
        gMotorExtInfo.R1      = rnd16();
        gMotorExtInfo.R2      = rnd16();
        gMotorExtInfo.LM      = rnd16();
        gMotorExtInfo.L0      = rnd16();
        gMotorExtInfo.L1      = rnd16();
        gMotorExtInfo.I0      = rnd16();
        gMotorExtInfo.RsPm    = rnd16();
        gMotorExtInfo.LD      = rnd16();
        gMotorExtInfo.LQ      = rnd16();
        gMotorExtInfo.BemfVolt = rnd16();

        gRotorTrans.Poles     = rnd16();

        gUDC.uDC              = rnd16();
        gUDC.uDcCalMax        = rnd16();
        gUDC.uDCADCoff        = rnd16();

        gADC.DelaySet         = rnd16();
        gDeadBand.DeadTimeSet = rnd16();
        gDeadBand.InvCurFilter = rnd16();

        gLineCur.CurPer       = rnd16();
        gLineCur.CurPerFilter = rnd16();
        gLineCur.CurTorque    = rnd16();

        gIAmpTheta.Amp        = rnd16();
        gIAmpTheta.PowerAngle = (int)rnd16();
        gIAmpTheta.CurTmpM    = (int)rnd16();
        gIAmpTheta.CurTmpT    = (int)rnd16();

        gIMTQ12.M             = (int)rnd16();
        gIMTQ12.T             = (int)rnd16();
        gIMTSetApply.M        = (int)rnd16();
        gIMTSetApply.T        = (int)rnd16();

        gOutVolt.MaxOutVolt   = rnd16();
        gOutVolt.VoltApply    = (int)rnd16();

        gVFPar.ovPoint        = rnd16();
        gVFPar.ovPointCoff    = rnd16();
        gVFPar.VFOverExc      = rnd16();

        gMainStatus.RunStep   = rnd16();
        gMainStatus.ParaCalTimes = rnd16();
        gMainStatus.StatusWord.all = rnd16();

        gPGData.PGMode        = (unsigned short)(rnd16() & 1);
        gPGData.PGType        = (unsigned short)(rnd16() % 3);
        gPGData.PGTypeGetFromFun = (unsigned short)(rnd16() % 3);
        gPGData.QEPIndex      = (QEP_INDEX_ENUM_STRUCT)(rnd16() % 3);
        gPGData.SpeedDir      = (unsigned short)(rnd16() & 1);

        gExtendCmd.bit.FreqUint = (unsigned short)(rnd16() % 3);
        gExtendCmd.bit.QepIndex = (unsigned short)(rnd16() % 3);

        gPowerTrq.rpItRated   = rnd16();
        gPowerTrq.anglePF     = (int)rnd16();
        gPowerTrq.Cur_Ft4     = (int)rnd16();

        gCurSamp.Coff         = rnd16();
        gUVCoff.UDivV         = rnd16();
        gUVCoff.UDivVGet      = rnd16();
        gVarAvr.CoffApply     = rnd16();
        gOvUdc.Limit          = rnd16();

        gIPMPos.RotorZeroGet  = rnd16();
        gIPMPos.RotorPos      = rnd16();
        gIPMPos.ZeroPosLast   = rnd16();
        gIPMPos.AbzErrPos     = (int)rnd16();

        gUVWPG.UvwZeroPos_deg = (int)rnd16();
        gUVWPG.UvwZIntErr     = (int)rnd16();

        gIPMPosCheck.UvwStopErr = (int)rnd16();

        scn_run((int)(rnd16() % F_COUNT), "random");
    }
}

/* ------------------------------------------------------------------------- */
/* bench self-check                                                         */
/* ------------------------------------------------------------------------- */

static int bench_selfcheck(void)
{
    int bad = 0;

    /* The shim folds `int` to 16 bits; if that ever stops being true the whole
     * comparison changes meaning. */
    if (sizeof(int) != 2) { printf("SELFCHECK int is %u bytes\n",
                                   (unsigned)sizeof(int)); bad = 1; }
    if (sizeof(Ulong) != 4) { printf("SELFCHECK Ulong is %u bytes\n",
                                     (unsigned)sizeof(Ulong)); bad = 1; }
    if (sizeof(Uint) != 2) { printf("SELFCHECK Uint is %u bytes\n",
                                    (unsigned)sizeof(Uint)); bad = 1; }

    /* cmp_img must actually see a difference. */
    {
        static struct SHARED a, b;
        long before = g_mism;
        memset(&a, 0, sizeof a);
        memset(&b, 0, sizeof b);
        b.motorInfo.Current = 1;
        g_mismShown = 999;          /* suppress the dump */
        cmp_img("selfcheck", &a, &b);
        if (g_mism != before + 1)
        {
            printf("SELFCHECK cmp_img missed a planted difference\n");
            bad = 1;
        }
        g_mism = before;
        g_mismShown = 0;
    }

    /* The stubs must record. */
    {
        memset(gMockCall, 0, sizeof gMockCall);
        SetIPMPos(0x1234);
        if (gMockCall[MC_SETIPMPOS] != 1 || gMockArg[MC_SETIPMPOS] != 0x1234)
        {
            printf("SELFCHECK SetIPMPos stub did not record\n");
            bad = 1;
        }
    }

    printf("SELFCHECK %s\n", bad ? "FAIL" : "ok");
    return bad;
}

/* ------------------------------------------------------------------------- */
/* main                                                                     */
/* ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long nrand = 2000;
    int bad = 0;
    int i;

    if (argc > 1)
    {
        nrand = atol(argv[1]);
        if (nrand < 0) nrand = 0;
    }

    g_rng = 0x12345678UL;

    if (bench_selfcheck()) return 2;

    phase_sys2ms();
    phase_run2ms();
    phase_sys05ms();
    phase_chgpar();
    phase_invpower();
    phase_random(nrand);

    printf("  hand=%ld random=%ld\n", g_cases - nrand, nrand);

    for (i = 0; i < F_COUNT; i++)
    {
        if (g_fnCases[i] <= 0) { printf("UNREACHED fn: %s\n", g_fnName[i]); bad = 1; }
    }
    for (i = 0; i < COV_COUNT; i++)
    {
        if (g_cov[i] <= 0)
        {
            printf("UNREACHED cov: %s\n", g_covName[i]);
            bad = 1;
        }
    }

    printf("MotorParChange difftest: cases=%ld mismatches=%ld\n", g_cases, g_mism);

    if (bad) return 2;
    return (g_mism == 0) ? 0 : 1;
}
