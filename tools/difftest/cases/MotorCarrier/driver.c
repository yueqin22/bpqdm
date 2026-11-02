/* Differential test driver for src/motor/MotorCarrier.c.
 *
 * The module owns the carrier-frequency / PWM-angle machinery:
 *
 *   CalCarrierWaveFreq  2 ms task -- picks the carrier frequency from the
 *                       temperature and the overload level, then hands the
 *                       angle calculation to either the asynchronous or the
 *                       synchronous path.
 *   AsynPWMAngleCal     asynchronous: carrier period + per-carrier phase step.
 *   SynPWMAngleCal      synchronous: same, but the step comes from a table and
 *                       the carrier frequency walks to the target in 0.02 kHz
 *                       increments while Flag says "still transitioning".
 *   SoftPWMProcess      soft-PWM period compensation (Vf, 1-6 kHz only).
 *   CalOutputPhase      output voltage phase: rotor position or free-running
 *                       accumulation, plus the compensation angle.
 *   CalDeadBandComp     dead-band compensation per phase (U/V/W).
 *
 * ---- What the bench has to be able to see ----
 *
 * 1. Field-by-field comparison of everything the module writes. The module's
 *    own globals (gFcCal / gSynPWM / gPhase / gDeadBand) are renamed per side
 *    by ref_tu.c / new_tu.c, so the two instances are compared directly. The
 *    globals it *borrows* (gBasePar.FcSetApply, gPWM, gIPMPos, gIAmpTheta) are
 *    captured per side as well: a one-sided write must not be masked by the
 *    other side running second.
 *
 * 2. The DINT ... EINT pair. Both angle functions wrap their phase writes in
 *    it, because they run in the carrier interrupt while the 0.5 ms task may be
 *    reading gPhase / gPWM.gPWMPrd. The shim turns DINT/EINT into increments of
 *    gHostCpuCtl[], and the counters are compared -- so dropping the pair, or
 *    swapping it, is a visible difference rather than an invisible one.
 *
 * 3. The SynCalRealPos() call. CalOutputPhase calls it in exactly one branch
 *    (PM motor, PGMode == 0). The stub logs the call, so "was it called" is
 *    observable even though the stub has no other effect.
 *
 * 4. Nothing is left at its reset value. Every input is seeded non-trivially
 *    before each case, so "the rewrite stopped writing field X" shows up as a
 *    difference immediately instead of "both sides happen to still be 0".
 *
 * ---- Input domain: two restrictions, both proved reachable-only ----
 *
 * (a) gBasePar.FcSetApply is swept over [8, 120] only.
 *     It is initialised to 50 (MotorMain.c:29) and this module is the only
 *     writer: it moves by +-1 per call toward gFcCal.FcBak, and FcBak is
 *     clamped by __IQsat into [m_MinFc, m_MaxFc] with m_MinFc in {8, 20} and
 *     m_MaxFc in {10, 40, 50, 80, 100, 120}. So the reachable set is [8, 120]
 *     and FcSetApply == 0 is NOT reachable -- which matters, because
 *     AsynPWMAngleCal computes m_LData / m_Fc and FcApply is built as
 *     FcSetApply << 9; feeding 0 would divide by zero on the host and crash
 *     the bench for a state the firmware cannot be in.
 *
 * (b) gBasePar.FullFreq01 is swept over >= 2001.
 *     MotorParChange.c:155 sets FullFreq01 = MaxFreq * pu2siCoeff + 2000 with
 *     MaxFreq >= 1 and pu2siCoeff >= 1. SynPWMAngleCal computes m_LData / m_Long
 *     and m_Long is proportional to FullFreq01 * AbsFreq, so FullFreq01 == 0
 *     would divide by zero -- again unreachable.
 *
 * Both restrictions are about *reachability*, not about convenience: they
 * exclude states the firmware cannot reach. Everything else is swept at its
 * boundaries, including the values that make the internal arithmetic wrap.
 *
 * ---- Host-unrepresentable region (一): integer division by zero ----
 *
 * SynPWMAngleCal ends with `m_HalfTc = m_LData / m_Long`, and
 *
 *     m_Long = (((FullFreq01 * abs(FreqSyn)) >> 7) * gSynNcTable[Index] / 100) >> 8
 *
 * is zero not only when FreqSyn is 0: it is zero for every abs(FreqSyn) below a
 * FullFreq01-dependent floor (with FullFreq01 == 2001 and Index == 0 that floor
 * is about 21). The region is reachable. MotorMain.c assigns
 * gMainCmd.FreqSyn = 0 in its DC_CONTROL and default branches while leaving
 * gMainCmd.Command.bit.ControlMode at IDC_VF_CTL -- which is exactly the
 * combination CalCarrierWaveFreq needs to latch gSynPWM.ModuleApply to 1, and
 * that is the only path that reaches the division.
 *
 * This is not something the rewrite introduces: the divisor expression is
 * identical on both sides, and identical in the original firmware. What it
 * means for the bench is narrower, and worth stating precisely:
 *
 *   * on the host an integer division by zero raises STATUS_INTEGER_DIVIDE_BY_ZERO
 *     and, if nothing catches it, kills the process -- so the case cannot be
 *     judged by comparing fields;
 *   * on the C28x there is no trap at all -- the runtime division routine
 *     returns an unspecified value and the firmware keeps running with a
 *     garbage gPWM.gPWMPrd.
 *
 * So the bench *classifies* these cases instead of excluding them, in
 * call_side() / note_hosttrap(). Both sides trapping is counted and skipped;
 * one side trapping while the other does not is reported as a MISMATCH, because
 * on the host "does it trap" is itself observable behaviour. Input coverage is
 * therefore not reduced, and the number of skipped cases is printed in the
 * summary -- a rewrite that moved the trap cannot hide behind the skip.
 *
 * ---- Host-unrepresentable region (二): gSynFreqTable[10] ----
 *
 * SynPWMAngleCal opens with an index scan over gSynFreqTable = {75, 113, 169,
 * 250, 355, 535, 854, 1281, 1922, 65535}:
 *
 *     m_Index = 0;
 *     while ((m_Index < 10) && (gSynPWM.AbsFreq >= gSynFreqTable[m_Index])) { m_Index++; }
 *     m_Index--;
 *
 * For AbsFreq < 75 the body never runs, m_Index stays 0, and `m_Index--` wraps
 * to 65535 (Uint is 16-bit). The next statement evaluates
 * gSynFreqTable[gSynPWM.Index + 1] whenever `m_Index > gSynPWM.Index`, which
 * 65535 always is. What that costs depends entirely on gSynPWM.Index:
 *
 *   * Index <= 8 -- the read is *in bounds* (the next entry is 113 or more),
 *     the guard `AbsFreq > gSynFreqTable[Index+1] + 10` is false because
 *     AbsFreq < 75, gSynPWM.Index is left alone, and everything after it
 *     (gSynNcTable[gSynPWM.Index] included) stays in range. The underflow is
 *     then not merely harmless on the target, it is *representable on the
 *     host*. The bench therefore feeds AbsFreq < 75 freely, and that is the
 *     only way the two +-10 latch thresholds at gSynFreqTable[0] (CW35 / CW39,
 *     which sit at AbsFreq == 64) can be observed at all.
 *
 *   * Index == 9 -- the read is gSynFreqTable[10], one past the end. On the
 *     target this is still benign, for a layout reason: .econst holds
 *     gSynFreqTable then gSynNcTable, both `Uint const`, so gSynFreqTable[10]
 *     *is* gSynNcTable[0] == 80, the guard is false, and Index is again left
 *     untouched. On the host the two arrays are separate globals and
 *     gSynFreqTable[10] is undefined: if it happens to be small the branch
 *     fires, gSynPWM.Index becomes 65535, and the next use
 *     (gSynNcTable[gSynPWM.Index]) reads far past a 20-byte array and takes an
 *     access violation. That is a *layout-dependent* hazard -- the bench could
 *     pass today and fault after an unrelated rebuild.
 *
 * So the excluded combination is narrow and can be stated exactly: *Index == 9
 * together with AbsFreq < 75*. avoid_host_oob_read() enforces it by clamping
 * Index to 8 in that one case. Every other (AbsFreq, Index) pair is fed,
 * including AbsFreq < 75 with Index <= 8, so the below-75 behaviour of
 * SynPWMAngleCal is covered rather than skipped.
 *
 * Consequence for the evidence, stated plainly: the single combination
 * (AbsFreq < 75, Index == 9) is not verified by this bench. Everything else is.
 *
 * ---- The accumulator gate at the top of CalCarrierWaveFreq ----
 *
 *     gFcCal.Cnt += gBasePar.FcSetApply;
 *     if (gFcCal.Cnt < 100) { return; }
 *     gFcCal.Cnt = 0;
 *
 * Every branch this module has sits behind that gate, and Cnt is *latched*: it
 * is only ever reset when the gate opens, so at entry it holds a multiple of
 * FcSetApply below 100, never an arbitrary small number. The bench originally
 * left Cnt at 3 and FcSetApply at 50, so `3 + 50 = 53 < 100` and the gate never
 * opened -- which meant *eight of the nine loops in phase_carrier_bounds
 * returned without executing a single line of the code they were written to
 * test*. It looked like coverage (695 cases, no mismatches) and was in fact
 * almost none.
 *
 * The negative-control sweep is what exposed it: 34 of 114 mutations came back
 * MISSED, and almost all of them were inside CalCarrierWaveFreq, interleaved
 * with CAUGHT ones so that the pattern read as "some mutations are equivalent"
 * rather than "the loop is dead". The mutations that were still caught were
 * caught by phase_random, which sets Cnt itself.
 *
 * open_fc_gate() now sets Cnt to the largest multiple of the current
 * FcSetApply that is below 100, so that adding FcSetApply once more reaches the
 * threshold by construction and the gate is guaranteed to open on this call --
 * and the (Cnt, FcSetApply) pair it feeds is one the firmware really passes
 * through. Every body loop calls it. The first loop and phase_random set Cnt
 * themselves, because those are the two that walk the gate boundary itself.
 *
 * Same trap, second form: any loop that wants to test the FcBak clamps has to
 * get past the opening `if` as well, because its else arm resets
 * `FcBak = FcSet` and would silently discard the value the loop just fed.
 * keep_fcbak() arranges for the branch to be entered without the step firing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <excpt.h>      /* __try/__except + GetExceptionCode() */

#include "MotorInclude.h"

/* ---- both sides ---- */

extern void ref_CalCarrierWaveFreq(void);
extern void ref_AsynPWMAngleCal(Ulong FcApply);
extern void ref_SynPWMAngleCal(void);
extern void ref_SoftPWMProcess(void);
extern void ref_CalOutputPhase(void);
extern void ref_CalDeadBandComp(void);

extern void new_CalCarrierWaveFreq(void);
extern void new_AsynPWMAngleCal(Ulong FcApply);
extern void new_SynPWMAngleCal(void);
extern void new_SoftPWMProcess(void);
extern void new_CalOutputPhase(void);
extern void new_CalDeadBandComp(void);

extern FC_CAL_STRUCT    ref_gFcCal;
extern SYN_PWM_STRUCT   ref_gSynPWM;
extern ANGLE_STRUCT     ref_gPhase;
extern DEAD_BAND_STRUCT ref_gDeadBand;

extern FC_CAL_STRUCT    new_gFcCal;
extern SYN_PWM_STRUCT   new_gSynPWM;
extern ANGLE_STRUCT     new_gPhase;
extern DEAD_BAND_STRUCT new_gDeadBand;

/* ---- mock-owned ---- */

#define CARRIER_LOG_LEN 64

extern volatile Uint16 gHostCpuCtl[7];
extern int             gSynCalPosLog[CARRIER_LOG_LEN];
extern Uint16          gSynCalPosCount;
extern volatile struct CPUTIMER_REGS CpuTimer1Regs;
extern struct CPUTIMER_VARS          CpuTimer1;

/* ---- driver constants ---- */

#define FAIL_LIMIT      20
#define CPU_CTL_N       7
#define PAREST_LEN      IDENTIFY_PROGRESS_LENGTH

/* Which entry point this case exercises. */
enum { F_CARRIER, F_ASYN, F_SYN, F_SOFT, F_OUTPHASE, F_DEADBAND, F_N };

static const char *g_fn_name[F_N] =
{
    "CalCarrierWaveFreq", "AsynPWMAngleCal", "SynPWMAngleCal",
    "SoftPWMProcess", "CalOutputPhase", "CalDeadBandComp"
};

/* ---- inputs ---- */

typedef struct MC_IN
{
    /* CalCarrierWaveFreq */
    Uint16 cnt;             /* gFcCal.Cnt                        */
    Uint16 fcTime;          /* gFcCal.Time                       */
    Uint16 fcBak;           /* gFcCal.FcBak                      */
    Uint16 fcLimit;         /* gFcCal.FcLimitOvLoad              */
    Uint16 fcSet;           /* gBasePar.FcSet                    */
    Uint16 fcSetApply;      /* gBasePar.FcSetApply               */
    Uint16 varFcByTem;      /* gSubCommand.bit.VarFcByTem        */
    Uint16 runStep;         /* gMainStatus.RunStep               */
    Uint16 temp;            /* gTemperature.Temp                 */
    Uint16 invType;         /* gInvInfo.InvTypeApply             */
    Uint16 invVolt;         /* gInvInfo.InvVoltageType           */
    Uint16 ctrlMode;        /* gMainCmd.Command.bit.ControlMode  */
    Uint16 startDC;         /* gMainCmd.Command.bit.StartDC      */
    Uint16 stopDC;          /* gMainCmd.Command.bit.StopDC       */
    Uint16 msw;             /* gOverLoad.InvTotal.half.MSW       */
    Uint16 modType;         /* gExtendCmd.bit.ModulateType       */
    Uint16 ctrMotor;        /* gCtrMotorType                     */
    Uint16 switchLF;        /* gPWM.PwmModeSwitchLF              */
    int32  freqReal;        /* gMainCmd.FreqReal                 */
    int32  freqSyn;         /* gMainCmd.FreqSyn                  */

    /* asynchronous / synchronous angle */
    Uint16 synAbsFreq;      /* gSynPWM.AbsFreq                   */
    Uint16 synModule;       /* gSynPWM.ModuleApply               */
    Uint16 synIndex;        /* gSynPWM.Index                     */
    Uint16 synFlag;         /* gSynPWM.Flag                      */
    Uint32 synFcApply;      /* gSynPWM.FcApply                   */
    Uint32 asynFc;          /* argument of AsynPWMAngleCal       */
    Uint32 fullFreq01;      /* gBasePar.FullFreq01               */

    /* SoftPWMProcess */
    Uint16 softTune;        /* gPWM.SoftPWMTune                  */
    int32  softCoff;        /* gPWM.SoftPWMCoff                  */
    Uint32 timerTim;        /* CpuTimer1Regs.TIM.all             */

    /* CalOutputPhase */
    Uint16 motorType;       /* gMotorInfo.MotorType              */
    Uint16 pgMode;          /* gPGData.PGMode                    */
    Uint16 pgType;          /* gPGData.PGType                    */
    Uint16 estMstep;        /* gGetParVarable.ParEstMstep        */
    Uint16 tuneStep;        /* gGetParVarable.ParEstContent[]    */
    int32  stepApply;       /* gPhase.StepPhaseApply             */
    int32  imPhase;         /* gPhase.IMPhase                    */
    int32  rotPos;          /* gIPMPos.RotorPos                  */
    int32  rotZero;         /* gIPMPos.RotorZero                 */
    int32  abzRef;          /* gIPMPos.ABZ_RotorPos_Ref          */
    int32  feedPos;         /* gIPMZero.FeedPos                  */
    int32  feedABZ;         /* gIPMZero.FeedABZPos               */
    int32  rtPos;           /* gRotorTrans.RTPos                 */
    int32  posComp;         /* gRotorTrans.PosComp               */
    int32  estZero;         /* gPmParEst.EstZero                 */
    int32  voltPhaseApply;  /* gOutVolt.VoltPhaseApply           */

    /* CalDeadBandComp */
    int32  deadComp;        /* gDeadBand.Comp                    */
    int32  invCurFilter;    /* gDeadBand.InvCurFilter            */
    int32  theta;           /* gIAmpTheta.Theta                  */
    int32  thetaFilter;     /* gIAmpTheta.ThetaFilter            */
    int32  compPhase;       /* gPhase.CompPhase                  */
} MC_IN;

static MC_IN g_in;

/* ---- snapshot ---- */

typedef struct MC_SIDE
{
    FC_CAL_STRUCT    fcCal;
    SYN_PWM_STRUCT   syn;
    ANGLE_STRUCT     phase;
    DEAD_BAND_STRUCT dead;
    PWM_OUT_STRUCT   pwm;
    Uint16           fcSetApply;
    int32            rotorPos;
    int32            abzRef;
    int32            thetaFilter;
    Uint16           cpu[CPU_CTL_N];
    Uint16           synCalPosCount;
} MC_SIDE;

static MC_SIDE S[2];

static long g_cases;
static long g_mismatches;
static int  g_fails;
static int  g_fnSel;

/* ---- per-side module-owned state ---- */

static void load_side(FC_CAL_STRUCT *fc, SYN_PWM_STRUCT *sy,
                      ANGLE_STRUCT *an, DEAD_BAND_STRUCT *db)
{
    fc->Cnt           = g_in.cnt;
    fc->Time          = g_in.fcTime;
    fc->FcBak         = g_in.fcBak;
    fc->FcLimitOvLoad = g_in.fcLimit;

    sy->FcApply       = g_in.synFcApply;
    sy->ModuleApply   = g_in.synModule;
    sy->Index         = g_in.synIndex;
    sy->AbsFreq       = g_in.synAbsFreq;
    sy->Flag          = g_in.synFlag;

    an->StepPhase     = g_in.stepApply;
    an->StepPhaseApply = g_in.stepApply;
    an->IMPhase       = g_in.imPhase;
    an->OutPhase      = (int)g_in.imPhase;
    an->CompPhase     = (int)g_in.compPhase;
    an->RotorPhase    = (int)g_in.rotPos;

    db->Comp          = g_in.deadComp;
    db->CompU         = 0;
    db->CompV         = 0;
    db->CompW         = 0;
    db->InvCurFilter  = g_in.invCurFilter;
}

static void take_side(int side, const FC_CAL_STRUCT *fc, const SYN_PWM_STRUCT *sy,
                      const ANGLE_STRUCT *an, const DEAD_BAND_STRUCT *db)
{
    S[side].fcCal  = *fc;
    S[side].syn    = *sy;
    S[side].phase  = *an;
    S[side].dead   = *db;
    S[side].pwm    = gPWM;
    S[side].fcSetApply   = gBasePar.FcSetApply;
    S[side].rotorPos     = gIPMPos.RotorPos;
    S[side].abzRef       = gIPMPos.ABZ_RotorPos_Ref;
    S[side].thetaFilter  = gIAmpTheta.ThetaFilter;
    S[side].synCalPosCount = gSynCalPosCount;
    memcpy(S[side].cpu, (const void *)gHostCpuCtl, sizeof S[side].cpu);
}

static void install(int side)
{
    int i;

    /* per-side state (renamed globals -> two independent instances) */
    if (side == 0)
    {
        load_side(&ref_gFcCal, &ref_gSynPWM, &ref_gPhase, &ref_gDeadBand);
    }
    else
    {
        load_side(&new_gFcCal, &new_gSynPWM, &new_gPhase, &new_gDeadBand);
    }

    /* shared state -- restored for BOTH sides so the ref side's writes cannot
     * leak into the new side. */
    gBasePar.FcSet        = g_in.fcSet;
    gBasePar.FcSetApply   = g_in.fcSetApply;
    gBasePar.FullFreq01   = g_in.fullFreq01;

    gSubCommand.all       = 0;
    gSubCommand.bit.VarFcByTem = g_in.varFcByTem;

    gMainStatus.RunStep   = g_in.runStep;
    gTemperature.Temp     = g_in.temp;
    gInvInfo.InvTypeApply = g_in.invType;
    gInvInfo.InvVoltageType = g_in.invVolt;

    gMainCmd.Command.all  = 0;
    gMainCmd.Command.bit.ControlMode = g_in.ctrlMode;
    gMainCmd.Command.bit.StartDC     = g_in.startDC;
    gMainCmd.Command.bit.StopDC      = g_in.stopDC;
    gMainCmd.FreqReal     = g_in.freqReal;
    gMainCmd.FreqSyn      = g_in.freqSyn;

    gExtendCmd.all        = 0;
    gExtendCmd.bit.ModulateType = g_in.modType;

    gOverLoad.InvTotal.all = 0;
    gOverLoad.InvTotal.half.MSW = g_in.msw;

    gCtrMotorType         = (CONTROL_MOTOR_TYPE_ENUM)g_in.ctrMotor;

    gPWM.gPWMPrd          = 0x1111;
    gPWM.gPWMPrdApply     = 0x2222;
    gPWM.PWMModle         = 0x3333;
    gPWM.SoftPWMTune      = g_in.softTune;
    gPWM.SoftPWMCoff      = g_in.softCoff;
    gPWM.PwmModeSwitchLF  = (int)g_in.switchLF;
    gPWM.PwmModeSwitchHF  = 0x4444;

    gMotorInfo.MotorType  = g_in.motorType;
    gPGData.PGMode        = g_in.pgMode;
    gPGData.PGType        = g_in.pgType;

    for (i = 0; i < PAREST_LEN; i++)
    {
        gGetParVarable.ParEstContent[i] = (PAR_EST_MAIN_STEP)(i * 7 + 1);
    }
    gGetParVarable.ParEstMstep = g_in.estMstep;
    gGetParVarable.ParEstContent[g_in.estMstep] = (PAR_EST_MAIN_STEP)g_in.tuneStep;

    gIPMPos.RotorPos          = g_in.rotPos;
    gIPMPos.RotorZero         = g_in.rotZero;
    gIPMPos.ABZ_RotorPos_Ref  = g_in.abzRef;
    gIPMZero.FeedPos          = g_in.feedPos;
    gIPMZero.FeedABZPos       = g_in.feedABZ;
    gRotorTrans.RTPos         = g_in.rtPos;
    gRotorTrans.PosComp       = g_in.posComp;
    gPmParEst.EstZero         = g_in.estZero;
    gOutVolt.VoltPhaseApply   = g_in.voltPhaseApply;

    gIAmpTheta.Theta          = g_in.theta;
    gIAmpTheta.ThetaFilter    = g_in.thetaFilter;

    CpuTimer1.RegsAddr = &CpuTimer1Regs;
    CpuTimer1Regs.TIM.all = g_in.timerTim;

    memset((void *)gHostCpuCtl, 0, sizeof gHostCpuCtl[0] * CPU_CTL_N);
    memset(gSynCalPosLog, 0, sizeof gSynCalPosLog);
    gSynCalPosCount = 0;
}

/* ---- comparison ---- */

static void report(const char *what)
{
    g_mismatches++;
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s: %s\n", g_fn_name[g_fnSel], what);
    printf("  fcCal Cnt=%u/%u Time=%u/%u FcBak=%u/%u FcLimit=%u/%u\n",
           S[0].fcCal.Cnt, S[1].fcCal.Cnt, S[0].fcCal.Time, S[1].fcCal.Time,
           S[0].fcCal.FcBak, S[1].fcCal.FcBak,
           S[0].fcCal.FcLimitOvLoad, S[1].fcCal.FcLimitOvLoad);
    printf("  syn   FcApply=%lu/%lu Module=%u/%u Index=%u/%u AbsFreq=%u/%u Flag=%u/%u\n",
           (unsigned long)S[0].syn.FcApply, (unsigned long)S[1].syn.FcApply,
           S[0].syn.ModuleApply, S[1].syn.ModuleApply,
           S[0].syn.Index, S[1].syn.Index,
           S[0].syn.AbsFreq, S[1].syn.AbsFreq,
           S[0].syn.Flag, S[1].syn.Flag);
    printf("  phase Step=%ld/%ld StepApply=%ld/%ld IM=%ld/%ld Out=%d/%d Comp=%d/%d\n",
           S[0].phase.StepPhase, S[1].phase.StepPhase,
           S[0].phase.StepPhaseApply, S[1].phase.StepPhaseApply,
           S[0].phase.IMPhase, S[1].phase.IMPhase,
           S[0].phase.OutPhase, S[1].phase.OutPhase,
           S[0].phase.CompPhase, S[1].phase.CompPhase);
    printf("  dead  Comp=%d/%d U=%d/%d V=%d/%d W=%d/%d\n",
           S[0].dead.Comp, S[1].dead.Comp,
           S[0].dead.CompU, S[1].dead.CompU,
           S[0].dead.CompV, S[1].dead.CompV,
           S[0].dead.CompW, S[1].dead.CompW);
    printf("  pwm   Prd=%u/%u PrdApply=%u/%u Modle=%u/%u SoftCoff=%d/%d\n",
           S[0].pwm.gPWMPrd, S[1].pwm.gPWMPrd,
           S[0].pwm.gPWMPrdApply, S[1].pwm.gPWMPrdApply,
           S[0].pwm.PWMModle, S[1].pwm.PWMModle,
           S[0].pwm.SoftPWMCoff, S[1].pwm.SoftPWMCoff);
    printf("  other FcSetApply=%u/%u RotorPos=%ld/%ld ABZRef=%ld/%ld "
           "ThetaF=%ld/%ld cpu=%u,%u,%u,%u,%u,%u,%u / %u,%u,%u,%u,%u,%u,%u "
           "synCalPos=%u/%u\n",
           S[0].fcSetApply, S[1].fcSetApply,
           S[0].rotorPos, S[1].rotorPos, S[0].abzRef, S[1].abzRef,
           S[0].thetaFilter, S[1].thetaFilter,
           S[0].cpu[0], S[0].cpu[1], S[0].cpu[2], S[0].cpu[3],
           S[0].cpu[4], S[0].cpu[5], S[0].cpu[6],
           S[1].cpu[0], S[1].cpu[1], S[1].cpu[2], S[1].cpu[3],
           S[1].cpu[4], S[1].cpu[5], S[1].cpu[6],
           S[0].synCalPosCount, S[1].synCalPosCount);
}

static void compare(const char *tag)
{
    int i;

    (void)tag;

    if (memcmp(&S[0].fcCal, &S[1].fcCal, sizeof S[0].fcCal) != 0)
    { report("gFcCal"); return; }
    if (memcmp(&S[0].syn, &S[1].syn, sizeof S[0].syn) != 0)
    { report("gSynPWM"); return; }
    if (memcmp(&S[0].phase, &S[1].phase, sizeof S[0].phase) != 0)
    { report("gPhase"); return; }
    if (memcmp(&S[0].dead, &S[1].dead, sizeof S[0].dead) != 0)
    { report("gDeadBand"); return; }
    if (memcmp(&S[0].pwm, &S[1].pwm, sizeof S[0].pwm) != 0)
    { report("gPWM"); return; }
    if (S[0].fcSetApply != S[1].fcSetApply)
    { report("gBasePar.FcSetApply"); return; }
    if (S[0].rotorPos != S[1].rotorPos)
    { report("gIPMPos.RotorPos"); return; }
    if (S[0].abzRef != S[1].abzRef)
    { report("gIPMPos.ABZ_RotorPos_Ref"); return; }
    if (S[0].thetaFilter != S[1].thetaFilter)
    { report("gIAmpTheta.ThetaFilter"); return; }
    if (S[0].synCalPosCount != S[1].synCalPosCount)
    { report("SynCalRealPos 调用次数"); return; }
    for (i = 0; i < CPU_CTL_N; i++)
    {
        if (S[0].cpu[i] != S[1].cpu[i])
        { report("CPU 控制序列 (DINT/EINT 计数)"); return; }
    }
}

/* ---- host division-by-zero traps ---- */

/* See "Host-unrepresentable region" in the file header. A trap is not a
 * mismatch and not a bench failure: it is the host refusing to evaluate a state
 * the target evaluates (to a garbage value). So it is caught, counted and
 * reported, and the case is skipped -- but never silently.
 *
 * Mechanism: SEH, not signal(SIGFPE). Measured, not assumed -- a three-line
 * probe (build/_fpetest.c) showed that MSVC's CRT does *not* translate
 * STATUS_INTEGER_DIVIDE_BY_ZERO into SIGFPE, so the handler never ran and the
 * process died exactly as before. __try/__except catches it directly, and
 * unlike a longjmp out of a signal handler it unwinds properly: execution
 * simply resumes after the __except block.
 *
 * Only integer-divide-by-zero is caught. Anything else -- an out-of-bounds
 * read, say -- is passed on with EXCEPTION_CONTINUE_SEARCH and still kills the
 * bench, because that would be a real defect in the module rather than a
 * host/target representability gap. */

#define HOST_EXC_INT_DIVIDE_BY_ZERO 0xC0000094UL

#define HOSTTRAP_PRINT  8

static int  g_trapped[2];
static long g_hosttraps;

/* One side's call, with the trap caught. */
static void call_side(int side)
{
    g_trapped[side] = 0;

    __try
    {
        switch (g_fnSel)
        {
        case F_CARRIER:  if (side == 0) ref_CalCarrierWaveFreq();
                         else           new_CalCarrierWaveFreq();        break;
        case F_ASYN:     if (side == 0) ref_AsynPWMAngleCal(g_in.asynFc);
                         else           new_AsynPWMAngleCal(g_in.asynFc); break;
        case F_SYN:      if (side == 0) ref_SynPWMAngleCal();
                         else           new_SynPWMAngleCal();            break;
        case F_SOFT:     if (side == 0) ref_SoftPWMProcess();
                         else           new_SoftPWMProcess();            break;
        case F_OUTPHASE: if (side == 0) ref_CalOutputPhase();
                         else           new_CalOutputPhase();            break;
        default:         if (side == 0) ref_CalDeadBandComp();
                         else           new_CalDeadBandComp();           break;
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
        printf("HOSTTRAP %s (宿主除零, 跳过比较): FreqSyn=%ld FreqReal=%ld "
               "FullFreq01=%lu FcSetApply=%u Module=%u Index=%u AbsFreq=%u "
               "Flag=%u\n",
               g_fn_name[g_fnSel], (long)g_in.freqSyn, (long)g_in.freqReal,
               (unsigned long)g_in.fullFreq01, g_in.fcSetApply,
               g_in.synModule, g_in.synIndex, g_in.synAbsFreq, g_in.synFlag);
    }
}

/* ---- input fix-ups that every case goes through ---- */

/* See "Host-unrepresentable region (二)" in the file header.  The scan in
 * SynPWMAngleCal wraps m_Index to 65535 whenever AbsFreq < 75, and the next
 * statement reads gSynFreqTable[gSynPWM.Index + 1].  For Index <= 8 that read is
 * in bounds and both sides do the same thing; for Index == 9 it is
 * gSynFreqTable[10], which the target resolves to gSynNcTable[0] inside .econst
 * but the host cannot represent at all.  Clamp Index to 8 in exactly that one
 * combination -- nothing else about AbsFreq < 75 is skipped. */
static void avoid_host_oob_read(void)
{
    int32 absfreq;

    if (g_fnSel == F_SYN)
    {
        absfreq = (int32)g_in.synAbsFreq;
    }
    else if (g_fnSel == F_CARRIER)
    {
        absfreq = g_in.freqReal / 100;      /* CalCarrierWaveFreq recomputes it */
    }
    else
    {
        return;                             /* SynPWMAngleCal is not reached */
    }

    if ((absfreq < 75) && (g_in.synIndex == 9))
    {
        g_in.synIndex = 8;
    }
}

/* Open the accumulator gate at the top of CalCarrierWaveFreq.  See the section
 * of that name in the file header: with the gate shut, the loop that calls this
 * never reaches the code it was written to test.
 *
 * Cnt is latched, so the entry value is not free -- it is a multiple of
 * FcSetApply below 100.  Take the largest such multiple: adding FcSetApply once
 * more is then >= 100 by construction, so the gate opens on this call, and the
 * (Cnt, FcSetApply) pair fed in is one the firmware really passes through
 * rather than a state invented to make the bench see something. */
static void open_fc_gate(void)
{
    Uint16 fa = g_in.fcSetApply;

    if (fa == 0)
    {
        return;                     /* 0 can never open the gate; not reachable */
    }
    if (fa >= 100)
    {
        g_in.cnt = 0;               /* 0 + FcSetApply >= 100 */
        return;
    }
    g_in.cnt = (Uint16)((99u / fa) * fa);
}

/* Let a fed FcBak survive CalCarrierWaveFreq's opening block, so that a loop
 * testing the FcBak clamps actually sees the value it fed.  Two clamps pull
 * FcBak back down before the m_MaxFc block is reached:
 *
 *     `if (FcBak > FcSet) { FcBak = FcSet; }`
 *     `if (FcBak > (FcLimitOvLoad/10)) { FcBak = FcLimitOvLoad/10; }`
 *
 * so FcSet has to be raised to the fed FcBak and FcLimitOvLoad to FcBak*10.
 * (Miss either one and the sweep silently collapses onto the baseline value --
 * which is exactly what made CW18 / CW19 / CW21 / CW22 / CW26 MISSED.)
 *
 * The temperature branch must also be entered without its step firing, or the
 * else arm resets `FcBak = FcSet`: Temp just above m_TempLim - 5 does that,
 * while Time (== fcTime + 1) stays below the 10000 step gate.
 *
 * Call this *after* g_in.fcBak is set.  A loop that wants to sweep
 * FcLimitOvLoad itself overrides g_in.fcLimit after the call. */
static void keep_fcbak(void)
{
    g_in.fcSet   = g_in.fcBak;
    g_in.fcLimit = (Uint16)(g_in.fcBak * 10);
    g_in.temp    = 70;              /* == m_TempLim: 65 < Temp and Temp <= 70 */
    g_in.fcTime  = 7;               /* Time becomes 8, below the 10000 gate */
}

static void run_case(void)
{
    int side;

    g_cases++;

    avoid_host_oob_read();

    for (side = 0; side < 2; side++)
    {
        install(side);

        call_side(side);

        if (side == 0)
        {
            take_side(0, &ref_gFcCal, &ref_gSynPWM, &ref_gPhase, &ref_gDeadBand);
        }
        else
        {
            take_side(1, &new_gFcCal, &new_gSynPWM, &new_gPhase, &new_gDeadBand);
        }
    }

    /* Trapping is observable behaviour on the host. The two sides run the same
     * divisor expression, so they must agree on it; disagreement is a real
     * finding, not a bench artefact. */
    if (g_trapped[0] != g_trapped[1])
    {
        report("宿主除零陷阱: 只有一侧触发 (两侧的除数表达式应当相同)");
        return;
    }
    if (g_trapped[0])
    {
        note_hosttrap();
        return;
    }

    compare(g_fn_name[g_fnSel]);
}

/* ---- deterministic pseudo random ---- */

/* 32-bit LCG + xorshift mix.
 *
 * The first version of this bench used a 64-bit-ish LCG whose low bits were
 * weak enough that `rnd() % 65536` degenerated -- the same mistake that cost
 * MotorSpeedCheck 74 MISSED records. The mix below puts entropy in the high
 * bits before truncating, so every 16-bit consumer really does see a varied
 * value. */
static Uint32 g_rng = 0x12345678UL;

static Uint32 rnd32(void)
{
    g_rng = g_rng * 1103515245UL + 12345UL;
    {
        Uint32 x = g_rng;
        x ^= x >> 16;
        x *= 2246822519UL;
        x ^= x >> 13;
        x *= 3266489917UL;
        x ^= x >> 16;
        return x;
    }
}

static Uint16 rnd16(void)
{
    return (Uint16)(rnd32() >> 8);
}

/* ---- value pools ---- */

/* 16-bit pool. Contains every boundary the module compares against:
 *   75/85/65  gSynFreqTable[0] and its +-10 hysteresis
 *   10/11     `10 < gBasePar.FcSet`
 *   15/16/30/31/60/61  FcSetApply thresholds
 *   65/70/75  m_TempLim and its +-5
 *   100/101   the `Cnt < 100` early return
 *   400/401   the FcLimitOvLoad floor
 *   799/800/801  `gMainCmd.FreqReal < 800`
 *   39999/40000/40001  the FreqReal dead-band threshold
 *   15999/16000/16001/17999/18000/18001  the overload windows
 * plus the small integers that appear as indices and flags. */
static const Uint16 g_pool16[] =
{
    0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 12, 14, 15, 16, 19, 20, 21,
    29, 30, 31, 40, 41, 49, 50, 51, 59, 60, 61, 64, 65, 66, 69, 70,
    74, 75, 76, 79, 80, 81, 84, 85, 86, 99, 100, 101, 113, 119, 120,
    121, 168, 169, 170, 249, 250, 251, 354, 355, 356, 399, 400, 401,
    534, 535, 536, 798, 799, 800, 801, 853, 854, 855, 1280, 1281,
    1282, 1921, 1922, 1923, 4095, 4096, 4097, 15999, 16000, 16001,
    17999, 18000, 18001, 32767, 32768, 39999, 40000, 40001, 61439,
    61440, 61441, 65534, 65535
};

#define POOL16_N (sizeof g_pool16 / sizeof g_pool16[0])

/* 32-bit pool for the Ulong inputs. FullFreq01 >= 2001 and FcApply >= 4096
 * are the reachable floors (see the header comment). */
static const Uint32 g_pool32[] =
{
    2001, 2002, 2048, 2500, 4096, 5000, 10000, 16384, 20000, 30000,
    32768, 40000, 65535, 65536, 100000, 200000, 1000000, 4002000,
    16777216, 33553920, 4294967295UL
};

#define POOL32_N (sizeof g_pool32 / sizeof g_pool32[0])

static Uint16 pool16(int i)
{
    return g_pool16[((i % (int)POOL16_N) + (int)POOL16_N) % (int)POOL16_N];
}

static Uint32 pool32(int i)
{
    return g_pool32[((i % (int)POOL32_N) + (int)POOL32_N) % (int)POOL32_N];
}

/* signed pool for the angle/frequency quantities */
static int32 rnd_signed(void)
{
    static const int32 sp[] =
    {
        0, 1, -1, 2, -2, 100, -100, 32767, -32768, 40000, -40000,
        65535, -65535, 100000, -100000, 1073741824, -1073741824,
        2147483647, -2147483647
    };
    return sp[rnd32() % (sizeof sp / sizeof sp[0])];
}

static int32 rnd_wide(void)
{
    /* any 32-bit pattern */
    return (int32)rnd32();
}

/* ---- baseline: every input non-trivial ---- */

static void base_state(void)
{
    memset(&g_in, 0, sizeof g_in);

    g_in.cnt        = 3;
    g_in.fcTime     = 7;
    g_in.fcBak      = 41;
    g_in.fcLimit    = 501;
    g_in.fcSet      = 53;
    g_in.fcSetApply = 50;
    g_in.varFcByTem = 1;
    g_in.runStep    = 1;
    g_in.temp       = 63;
    g_in.invType    = 13;
    g_in.invVolt    = 1;
    g_in.ctrlMode   = IDC_VF_CTL;
    g_in.startDC    = 0;
    g_in.stopDC     = 0;
    g_in.msw        = 17001;
    g_in.modType    = 1;
    g_in.ctrMotor   = ASYNC_VF;
    g_in.switchLF   = 5000;
    g_in.freqReal   = 9000;
    g_in.freqSyn    = 7000;

    g_in.synAbsFreq = 200;
    g_in.synModule  = 1;
    g_in.synIndex   = 3;
    g_in.synFlag    = 0;
    g_in.synFcApply = 25600;
    g_in.asynFc     = 25600;
    g_in.fullFreq01 = 52000;

    g_in.softTune   = 9;
    g_in.softCoff   = 12345;
    g_in.timerTim   = 0x01234567UL;

    g_in.motorType  = MOTOR_TYPE_PM;
    g_in.pgMode     = 0;
    g_in.pgType     = 1;
    g_in.estMstep   = 2;
    g_in.tuneStep   = (Uint16)PM_EST_NO_LOAD;
    g_in.stepApply  = 123456;
    g_in.imPhase    = 0x11223344L;
    g_in.rotPos     = 60000;
    g_in.rotZero    = 1234;
    g_in.abzRef     = 7000;
    g_in.feedPos    = 80000;
    g_in.feedABZ    = 9000;
    g_in.rtPos      = 110000;
    g_in.posComp    = 321;
    g_in.estZero    = 4321;
    g_in.voltPhaseApply = 137;

    g_in.deadComp    = 250;
    g_in.invCurFilter = 700;
    g_in.theta       = 0x0F0F0F0F;
    g_in.thetaFilter = 0x00110022;
    g_in.compPhase   = 600;
}

/* ---- phases ---- */

/* 1. CalCarrierWaveFreq: one boundary at a time, everything else at baseline.
 *
 * Every loop except the first calls open_fc_gate() -- see "The accumulator gate
 * at the top of CalCarrierWaveFreq" in the file header for why that is not
 * optional -- and every loop that feeds FcBak calls keep_fcbak() so the value
 * survives the opening block. */
static void phase_carrier_bounds(void)
{
    static const Uint16 fcs[] = { 0, 1, 8, 10, 11, 15, 16, 20, 30, 40,
                                  50, 60, 61, 80, 100, 120 };
    static const Uint16 cnts[] = { 0, 1, 50, 99, 100, 101, 219, 65535 };
    static const Uint16 temps[] = { 0, 1, 64, 65, 66, 69, 70, 71, 74, 75,
                                    76, 100, 65535 };
    static const Uint16 ms[] = { 0, 1, 15999, 16000, 16001, 17000, 17999,
                                 18000, 18001, 32767, 65535 };
    static const Uint16 invts[] = { 0, 1, 11, 12, 13, 14, 15, 16, 17, 18,
                                    19, 20, 255 };
    static const Uint16 frs[] = { 0, 1, 100, 400, 799, 800, 801, 6500,
                                  7500, 8500, 39999, 40000, 40001, 65535 };
    static const Uint16 absfr[] = { 0, 1, 64, 65, 66, 70, 74, 75, 76, 84,
                                    85, 86, 87, 112, 113, 114, 168, 169,
                                    170, 1919, 1920, 1921, 1922, 1923,
                                    65534, 65535 };
    static const Uint16 lims[] = { 0, 399, 400, 401, 500, 1200, 65535 };
    static const Uint16 baks[] = { 0, 1, 4, 5, 9, 10, 11, 14, 15, 20, 40,
                                   41, 51, 60, 80, 81, 100, 101, 120, 121,
                                   65535 };
    static const Uint16 idxs[] = { 0, 1, 2, 3, 8, 9 };

    /* FcBak values sitting one step past a clamp: 120/121 against m_MaxFc,
     * 100/101 against the >12 class, 80/81 against the non-VF class, 50/51
     * against the low-frequency class, 40/41 against the 690 V class and 10/11
     * against the 1140 V class.  Each pair is what separates `X` from `X+1`. */
    static const Uint16 cbaks[] = { 0, 11, 41, 51, 60, 80, 81, 100, 101,
                                    120, 121 };

    /* FcSet on both sides of the `10 < FcSet` gate, and FcBak on both sides of
     * the cooling floor (FcBak - 5 against 10, so 14 and 15) and of the
     * `FcBak > FcSet` clamp. */
    static const Uint16 tfcss[] = { 10, 11, 12, 60 };
    static const Uint16 tfbaks[] = { 0, 9, 10, 11, 12, 14, 15, 60 };

    /* 1 = 380 V, 3 = 690 V, 5 = 1140 V, plus out-of-range values. */
    static const Uint16 vvolts[] = { 0, 1, INV_VOLTAGE_690V,
                                     INV_VOLTAGE_1140V, 255 };

    /* PwmModeSwitchLF offsets: both sides of the `< LF` and `> LF + 300`
     * thresholds, including exactly on each. */
    static const int32  swlfoff[] = { -1, 0, 1, 299, 300, 301, 302 };
    static const Uint16 swlf[] = { 0, 5000, 7000, 8500, 9000, 65535 };
    static const Uint16 cms[] = { ASYNC_VF, SYNC_VF, SYNC_SVC, 0, 1, 2, 9, 11 };

    int i, j, k, m, n;

    g_fnSel = F_CARRIER;

    /* The gate itself: Cnt + FcSetApply against 100.  This is the one loop that
     * must NOT call open_fc_gate(): it walks that boundary on purpose. */
    for (i = 0; i < (int)(sizeof cnts / sizeof cnts[0]); i++)
    {
        for (j = 0; j < (int)(sizeof fcs / sizeof fcs[0]); j++)
        {
            base_state();
            g_in.cnt        = cnts[i];
            g_in.fcSetApply = fcs[j];
            g_in.fcSet      = fcs[j];
            run_case();
        }
    }

    /* The temperature branch.  Its entry tests are
     *   VarFcByTem != 0 && RunStep != STATUS_GET_PAR
     *   && Temp > m_TempLim-5 && 10 < FcSet
     * and inside it `Time >= 10000` picks the step, `Temp <= m_TempLim` the
     * heating arm and `Temp >= m_TempLim+5` the cooling arm.  So Temp has to
     * straddle 65, 70 and 75; FcSet has to straddle 10 (11, not just 10 and
     * 60); and Time has to straddle 10000, which means fcTime 9998 / 9999 /
     * 10000, because Time is incremented before the test. */
    for (i = 0; i < (int)(sizeof temps / sizeof temps[0]); i++)
    {
        for (j = 0; j < 3; j++)
        {
            for (k = 0; k < (int)(sizeof tfcss / sizeof tfcss[0]); k++)
            {
                for (m = 0; m < (int)(sizeof tfbaks / sizeof tfbaks[0]); m++)
                {
                    for (n = 0; n < 3; n++)
                    {
                        base_state();
                        g_in.temp       = temps[i];
                        g_in.varFcByTem = (Uint16)j;
                        g_in.runStep    = (Uint16)((j == 2) ? STATUS_GET_PAR : 1);
                        g_in.fcSet      = tfcss[k];
                        g_in.fcBak      = tfbaks[m];
                        g_in.fcTime     = (Uint16)(9998 + n);
                        open_fc_gate();
                        run_case();
                    }
                }
            }
        }
    }

    /* FcBak walking toward / away from the overload limit, and the two /10
     * clamps after it.  40 against a limit of 400 is the pair that separates
     * `/10` from `/11` in the *condition*: 40 > 40 is false, 40 > 36 is true. */
    for (i = 0; i < (int)(sizeof baks / sizeof baks[0]); i++)
    {
        for (j = 0; j < (int)(sizeof lims / sizeof lims[0]); j++)
        {
            base_state();
            g_in.fcBak   = baks[i];
            keep_fcbak();
            g_in.fcLimit = lims[j];
            open_fc_gate();
            run_case();
        }
    }

    /* The separating inputs for `FcBak > FcLimitOvLoad/10`.
     *
     * The condition's divisor and the assignment's divisor are two separate
     * tokens in the source, and a mutation of one of them is only visible where
     * the two quotients differ -- that is, where
     *
     *     floor(L/11) < FcBak < floor(L/10).
     *
     * For L = 400 that band is {37, 38, 39}, and baks[] jumps 20 -> 40, so the
     * hand-written table walks straight past it.  The value it does feed, 40,
     * sits exactly ON the boundary and is therefore useless: 40 > 40 is false
     * and 40 > 36 is true, but the body still assigns 400/10 = 40, which is
     * what the ref side leaves behind anyway.  That is why the first version of
     * this bench reported CW16 as MISSED.
     *
     * Rather than add 37/38/39 by hand and leave the next divisor unguarded,
     * the band is COMPUTED from the two divisors for every L.  The cap keeps
     * L = 65535 from contributing thousands of cases that cannot be told apart
     * anyway (m_MaxFc is at most 120, so both quotients saturate to the same
     * value).  invType is swept because L = 1200 puts the band at 110..119,
     * which is only observable while m_MaxFc is 120 rather than 100. */
    for (j = 0; j < (int)(sizeof lims / sizeof lims[0]); j++)
    {
        Uint16 lo = (Uint16)(lims[j] / 11);
        Uint16 hi = (Uint16)(lims[j] / 10);

        if ((Uint16)(hi - lo) > 16)
        {
            hi = (Uint16)(lo + 16);
        }

        for (m = (int)lo + 1; m < (int)hi; m++)
        {
            for (k = 0; k < 2; k++)
            {
                base_state();
                g_in.invType = (Uint16)(k ? 13 : 12);
                g_in.fcBak   = (Uint16)m;
                keep_fcbak();
                g_in.fcLimit = lims[j];
                open_fc_gate();
                run_case();
            }
        }
    }

    /* the overload window itself, with FcLimitOvLoad walked across the 400
     * floor */
    for (i = 0; i < (int)(sizeof ms / sizeof ms[0]); i++)
    {
        for (j = 0; j < (int)(sizeof lims / sizeof lims[0]); j++)
        {
            base_state();
            g_in.fcBak   = 60;
            keep_fcbak();
            g_in.msw     = ms[i];
            g_in.fcLimit = lims[j];
            open_fc_gate();
            run_case();
        }
    }

    /* m_MaxFc / m_MinFc selection, and the FcSetApply +-1 adjust.  Every clamp
     * in this block is only visible where FcBak sits just outside it, so FcBak
     * is swept here too. */
    for (i = 0; i < (int)(sizeof invts / sizeof invts[0]); i++)
    {
        for (j = 0; j < 2; j++)
        {
            for (k = 0; k < 2; k++)
            {
                for (m = 0; m < (int)(sizeof vvolts / sizeof vvolts[0]); m++)
                {
                    for (n = 0; n < (int)(sizeof cbaks / sizeof cbaks[0]); n++)
                    {
                        base_state();
                        g_in.invType    = invts[i];
                        g_in.ctrlMode   = (Uint16)(j ? IDC_VF_CTL
                                                     : (IDC_VF_CTL + 1));
                        g_in.freqReal   = k ? 799 : 9000;
                        g_in.invVolt    = vvolts[m];
                        g_in.fcBak      = cbaks[n];
                        keep_fcbak();
                        g_in.fcSetApply = 60;
                        open_fc_gate();
                        run_case();
                    }
                }
            }
        }
    }

    /* the gMainCmd.FreqReal thresholds, again with FcBak swept so the 50 clamp
     * is observable */
    for (i = 0; i < (int)(sizeof frs / sizeof frs[0]); i++)
    {
        for (j = 0; j < (int)(sizeof cbaks / sizeof cbaks[0]); j++)
        {
            base_state();
            g_in.freqReal   = frs[i];
            g_in.fcBak      = cbaks[j];
            keep_fcbak();
            g_in.fcSetApply = 60;
            open_fc_gate();
            run_case();
        }
    }

    /* Synchronous / asynchronous selection: the two +-10 latch thresholds at
     * gSynFreqTable[0], the FcApply latch, and the DPWM/CPWM decision that
     * follows it.  AbsFreq below 75 is fed as well -- see "Host-unrepresentable
     * region (二)" -- because with Index <= 8 the underflow is representable on
     * the host, and 64 / 74 are exactly where the two thresholds sit. */
    for (i = 0; i < (int)(sizeof absfr / sizeof absfr[0]); i++)
    {
        for (j = 0; j < 2; j++)
        {
            for (k = 0; k < 2; k++)
            {
                for (m = 0; m < (int)(sizeof idxs / sizeof idxs[0]); m++)
                {
                    base_state();
                    g_in.synAbsFreq = absfr[i];
                    g_in.synModule  = (Uint16)j;
                    g_in.modType    = (Uint16)k;
                    g_in.synIndex   = idxs[m];
                    g_in.synFlag    = (Uint16)((i + j + k + m) % 3);
                    g_in.synFcApply = pool32(i + j + k + m);
                    g_in.fcSetApply = 60;
                    g_in.freqReal   = (int32)absfr[i] * 100;
                    open_fc_gate();
                    run_case();
                }
            }
        }
    }

    /* DPWM / CPWM selection.  Three conditions force CPWM -- FcSetApply <= 15,
     * ModulateType == 1, CtrMotorType != ASYNC_VF -- and otherwise FreqReal is
     * compared with PwmModeSwitchLF and with PwmModeSwitchLF + 300.  Walk both
     * thresholds from both sides. */
    for (i = 0; i < (int)(sizeof swlf / sizeof swlf[0]); i++)
    {
        for (j = 0; j < (int)(sizeof cms / sizeof cms[0]); j++)
        {
            for (k = 0; k < (int)(sizeof swlfoff / sizeof swlfoff[0]); k++)
            {
                for (m = 0; m < 2; m++)
                {
                    base_state();
                    g_in.switchLF   = swlf[i];
                    g_in.ctrMotor   = cms[j];
                    g_in.modType    = (Uint16)((k + m) & 1);
                    g_in.fcSetApply = (Uint16)(m ? 16 : 15);
                    g_in.freqReal   = (int32)swlf[i] + swlfoff[k];
                    open_fc_gate();
                    run_case();
                }
            }
        }
    }

    /* Index / Flag on the synchronous path, including the Flag 1 and Flag 2
     * arms that walk FcApply by +-100.  AbsFreq is not an input here:
     * CalCarrierWaveFreq recomputes it from FreqReal, so it is FreqReal that
     * has to carry the value. */
    for (i = 0; i < (int)(sizeof idxs / sizeof idxs[0]); i++)
    {
        for (j = 0; j < 3; j++)
        {
            for (k = 0; k < 2; k++)
            {
                base_state();
                g_in.synModule  = 1;
                g_in.synIndex   = idxs[i];
                g_in.synFlag    = (Uint16)j;
                g_in.synFcApply = k ? 10000 : 100000;
                g_in.freqReal   = (k ? 1922 : 113) * 100;
                open_fc_gate();
                run_case();
            }
        }
    }
}

/* 2. AsynPWMAngleCal: the FcApply > 61440 halving, the FreqSyn sign, and the
 *    64-bit product. */
static void phase_asyn_bounds(void)
{
    static const Uint16 fcs[] = { 4096, 5000, 10000, 30000, 32767, 61439,
                                  61440, 61441, 61442, 65535 };
    static const int32 syn[] = { 0, 1, -1, 100, -100, 40000, -40000,
                                 65535, -65535, 2147483647, -2147483647 };
    int i, j, k;

    g_fnSel = F_ASYN;

    for (i = 0; i < (int)(sizeof fcs / sizeof fcs[0]); i++)
    {
        for (j = 0; j < (int)(sizeof syn / sizeof syn[0]); j++)
        {
            for (k = 0; k < 3; k++)
            {
                base_state();
                g_in.asynFc     = (Uint32)fcs[i];
                g_in.freqSyn    = syn[j];
                g_in.fullFreq01 = pool32(k * 5 + i);
                run_case();
            }
        }
    }

    /* the FcApply argument is 32-bit: exercise the high half too */
    for (i = 0; i < (int)POOL32_N; i++)
    {
        base_state();
        g_in.asynFc  = pool32(i);
        g_in.freqSyn = (int32)pool16(i);
        run_case();
    }
}

/* 3. SynPWMAngleCal: the index scan, the hysteresis window, the Flag
 *    transitions, and the divide by m_Long. */
static void phase_syn_bounds(void)
{
    /* 每个档位两边各取 11 / 10 / 10 / 11:
     *   -11 / -10 是 `AbsFreq <= gSynFreqTable[Index]-10` 的翻转点 (把它改成 -11
     *              就在 AbsFreq == 档位-10 处分歧);
     *   +10 / +11 是 `AbsFreq > gSynFreqTable[Index+1]+10` 的翻转点。
     * 再加档位本身 ±1, 用来走下标扫描 `>=` 的边界。
     *
     * 低于 75 的值 (64, 65) 也喂: 见文件头 "宿主不可表示区 (二)" —— 只要
     * Index <= 8, 下溢在宿主上是可表示的 (越界读落在数组内), 而 64 正是
     * `AbsFreq < gSynFreqTable[0]-10` 的翻转点。avoid_host_oob_read() 负责挡掉
     * Index == 9 的那一种组合。只喂 75 及以上是抓不到那两个回差常量的。 */
    static const Uint16 absfr[] = { 64, 65, 75, 76, 85, 86, 102, 103, 112,
                                    113, 114, 123, 124, 158, 159, 168, 169,
                                    170, 179, 180, 239, 240, 249, 250, 251,
                                    260, 261, 344, 345, 354, 355, 356, 365,
                                    366, 524, 525, 534, 535, 536, 545, 546,
                                    843, 844, 853, 854, 855, 864, 865, 1270,
                                    1271, 1280, 1281, 1282, 1291, 1292, 1911,
                                    1912, 1921, 1922, 1923, 1932, 1933,
                                    65524, 65525, 65534, 65535 };
    static const Uint16 idxs[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    int i, j, k;

    g_fnSel = F_SYN;

    for (i = 0; i < (int)(sizeof absfr / sizeof absfr[0]); i++)
    {
        for (j = 0; j < (int)(sizeof idxs / sizeof idxs[0]); j++)
        {
            for (k = 0; k < 3; k++)
            {
                base_state();
                g_in.synModule = 1;
                g_in.synAbsFreq = absfr[i];
                g_in.synIndex   = idxs[j];
                g_in.synFlag    = (Uint16)k;
                g_in.synFcApply = (Uint32)(1000 + i * 137 + j * 11);
                g_in.freqSyn    = (int32)absfr[i] * 100;
                run_case();
            }
        }
    }

    /* Flag 1 / Flag 2 walk-to-target: FcApply just below / at / above m_Fc,
     * and the `FcApply +- 100` wrap at the ends of the Ulong range. */
    {
        static const Uint32 fca[] = { 0, 1, 99, 100, 101, 25599, 25600,
                                      25601, 65535, 65536, 4294967195UL,
                                      4294967295UL };
        for (i = 0; i < (int)(sizeof fca / sizeof fca[0]); i++)
        {
            for (j = 1; j <= 2; j++)
            {
                base_state();
                g_in.synModule  = 1;
                g_in.synIndex   = (Uint16)(3 + (j - 1));
                g_in.synFlag    = (Uint16)j;
                g_in.synFcApply = fca[i];
                g_in.synAbsFreq = 250;
                g_in.freqSyn    = 25000;
                g_in.fullFreq01 = 52000;
                run_case();
            }
        }
    }

    /* FullFreq01 sweep (>= 2001: see the header comment) */
    for (i = 0; i < (int)POOL32_N; i++)
    {
        base_state();
        g_in.synModule  = 1;
        g_in.fullFreq01 = pool32(i);
        g_in.synAbsFreq = 250;
        g_in.synIndex   = 3;
        g_in.synFlag    = 0;
        g_in.freqSyn    = 25000;
        run_case();
    }
}

/* 4. SoftPWMProcess: the four-way guard, the 3277/1966 choice, and GetTime(). */
static void phase_soft_bounds(void)
{
    static const Uint16 fca[] = { 8, 9, 10, 11, 29, 30, 31, 50, 59, 60,
                                  61, 120 };
    static const Uint16 tune[] = { 0, 1, 2, 8, 15, 16, 17, 31, 32, 65535 };
    static const Uint32 tim[] = { 0, 1, 2, 0x7FFFFFFFUL, 0x80000000UL,
                                  0xFFFFFFFFUL, 0x01234567UL };
    int i, j, k;

    g_fnSel = F_SOFT;

    for (i = 0; i < (int)(sizeof fca / sizeof fca[0]); i++)
    {
        for (j = 0; j < (int)(sizeof tune / sizeof tune[0]); j++)
        {
            for (k = 0; k < 4; k++)
            {
                base_state();
                g_in.fcSetApply = fca[i];
                g_in.softTune   = tune[j];
                g_in.ctrlMode   = (k & 1) ? IDC_VF_CTL : (Uint16)(IDC_VF_CTL + 1);
                g_in.synModule  = (Uint16)((k >> 1) & 1);
                g_in.timerTim   = tim[(i + j) % (int)(sizeof tim / sizeof tim[0])];
                g_in.softCoff   = (int32)pool32(i + j);
                run_case();
            }
        }
    }

    /* SoftPWMCoff / gPWMPrd / StepPhase extremes */
    for (i = 0; i < (int)POOL32_N; i++)
    {
        for (j = 0; j < 3; j++)
        {
            base_state();
            g_in.softCoff   = (int32)pool32(i);
            g_in.stepApply  = (int32)pool32((i + j * 7) % (int)POOL32_N);
            g_in.timerTim   = pool32((i + j) % (int)POOL32_N);
            g_in.fcSetApply = (Uint16)(10 + j * 25);
            g_in.softTune   = (Uint16)(j * 16);
            run_case();
        }
    }
}

/* 5. CalOutputPhase: the rotor-position branches and TUNE_STEP_NOW. */
static void phase_outphase_bounds(void)
{
    static const Uint16 steps[] = { 0, 1, 2, 3, 7, 8, 100 };
    static const Uint16 pgm[] = { 0, 1, 2, 65535 };
    static const Uint16 pgt[] = { 0, 1, 2, 65535 };
    static const Uint16 motors[] = { MOTOR_TYPE_IM, MOTOR_TYPE_VARFREQ,
                                     MOTOR_TYPE_PM, 3, 65535 };
    static const Uint16 cm[] = { ASYNC_VF, SYNC_VF, SYNC_SVC, 0, 9 };
    static const Uint16 mstep[] = { 0, 1, 2, 3, 4 };
    int i, j, k;

    g_fnSel = F_OUTPHASE;

    /* ParEstMstep indexes ParEstContent[IDENTIFY_PROGRESS_LENGTH] -- the index
     * is an input, so sweep it and the step value it selects. */
    for (i = 0; i < (int)(sizeof mstep / sizeof mstep[0]); i++)
    {
        for (j = 0; j < 6; j++)
        {
            base_state();
            g_in.estMstep = mstep[i];
            g_in.tuneStep = (Uint16)j;
            g_in.motorType = MOTOR_TYPE_PM;
            g_in.pgMode   = (j & 1) ? 1 : 0;
            g_in.pgType   = (Uint16)((j >> 1) & 1);
            g_in.runStep  = (Uint16)((j & 2) ? STATUS_GET_PAR : 1);
            g_in.ctrMotor = (j & 4) ? SYNC_VF : ASYNC_VF;
            run_case();
        }
    }

    for (i = 0; i < (int)(sizeof motors / sizeof motors[0]); i++)
    {
        for (j = 0; j < (int)(sizeof steps / sizeof steps[0]); j++)
        {
            for (k = 0; k < (int)(sizeof cm / sizeof cm[0]); k++)
            {
                base_state();
                g_in.motorType = motors[i];
                g_in.runStep   = steps[j];
                g_in.ctrMotor  = cm[k];
                g_in.pgMode    = 0;
                g_in.estMstep  = 1;
                g_in.tuneStep  = (Uint16)PM_EST_BEMF;
                run_case();
            }
        }
    }

    for (i = 0; i < (int)(sizeof pgm / sizeof pgm[0]); i++)
    {
        for (j = 0; j < (int)(sizeof pgt / sizeof pgt[0]); j++)
        {
            for (k = 0; k < 3; k++)
            {
                base_state();
                g_in.motorType = MOTOR_TYPE_PM;
                g_in.pgMode    = pgm[i];
                g_in.pgType    = pgt[j];
                g_in.estMstep  = 0;
                g_in.tuneStep  = (Uint16)(k == 0 ? PM_EST_NO_LOAD
                                          : k == 1 ? PM_EST_BEMF : PM_EST_WITH_LOAD);
                g_in.runStep   = (Uint16)(k == 1 ? STATUS_GET_PAR : 1);
                run_case();
            }
        }
    }

    /* wide values through the shift and the accumulation */
    for (i = 0; i < (int)POOL32_N; i++)
    {
        base_state();
        g_in.motorType = MOTOR_TYPE_PM;
        g_in.pgMode    = 1;
        g_in.pgType    = PG_TYPE_RESOLVER;
        g_in.runStep   = 1;
        g_in.rtPos     = (int32)pool32(i);
        g_in.posComp   = (int32)pool32((i + 3) % (int)POOL32_N);
        g_in.rotZero   = (int32)pool32((i + 5) % (int)POOL32_N);
        g_in.estZero   = (int32)pool32((i + 7) % (int)POOL32_N);
        g_in.stepApply = (int32)pool32((i + 11) % (int)POOL32_N);
        g_in.imPhase   = (int32)pool32((i + 13) % (int)POOL32_N);
        g_in.voltPhaseApply = (int32)pool32((i + 17) % (int)POOL32_N);
        run_case();
    }
}

/* 6. CalDeadBandComp: the 40000 threshold, the InvCurFilter scaling, the DC
 *    inhibit, and the three 21845 offsets. */
static void phase_deadband_bounds(void)
{
    static const int32 frs[] = { 0, 1, 399, 40000, 40001, 65535, 65536,
                                 100000, 200000 };
    static const int32 comps[] = { 0, 1, -1, 250, -250, 32767, -32768,
                                   65535, 100000, -100000 };
    static const int32 filters[] = { 0, 1, 255, 511, 512, 513, 1024,
                                     65535, 100000 };
    static const int32 imps[] = { 0, 1, 16384, 32767, 65535, 100000,
                                  2147483647, -2147483647 };
    int i, j, k;

    g_fnSel = F_DEADBAND;

    for (i = 0; i < (int)(sizeof frs / sizeof frs[0]); i++)
    {
        for (j = 0; j < (int)(sizeof comps / sizeof comps[0]); j++)
        {
            for (k = 0; k < 6; k++)
            {
                base_state();
                g_in.freqReal     = frs[i];
                g_in.deadComp     = comps[j];
                g_in.invCurFilter = filters[(i + j) % (int)(sizeof filters / sizeof filters[0])];
                g_in.motorType    = (k & 1) ? MOTOR_TYPE_PM : MOTOR_TYPE_IM;
                g_in.startDC      = (Uint16)((k >> 1) & 1);
                g_in.stopDC       = (Uint16)((k >> 2) & 1);
                run_case();
            }
        }
    }

    /* the phase sum that decides the sign of CompU/V/W */
    for (i = 0; i < (int)(sizeof imps / sizeof imps[0]); i++)
    {
        for (j = 0; j < (int)POOL32_N; j++)
        {
            base_state();
            g_in.imPhase     = imps[i];
            g_in.theta       = (int32)pool32(j);
            g_in.thetaFilter = (int32)pool32((j + 4) % (int)POOL32_N);
            g_in.compPhase   = (int32)pool32((j + 8) % (int)POOL32_N);
            run_case();
        }
    }

    /* The compared quantity is
     *   phase = (IMPhase >> 16) + Theta + CompPhase + 16384
     * and the branch tests `phase <= 0`.  Note that it is Theta that enters the
     * sum, not ThetaFilter: CalDeadBandComp assigns `ThetaFilter = Theta`
     * before computing it, so the ThetaFilter fed in is overwritten.  Sweeping
     * imPhase / theta / compPhase independently essentially never lands on
     * phase == 0, and that is the only place the 16384 offset can be seen -- so
     * solve for the sum instead of hoping to hit it.  Theta is pinned to 0 and
     * CompPhase places the sum on -1 / 0 / +1. */
    for (i = 0; i < (int)(sizeof imps / sizeof imps[0]); i++)
    {
        for (j = -1; j <= 1; j++)
        {
            base_state();
            g_in.imPhase   = imps[i];
            g_in.theta     = 0;
            g_in.compPhase = -16384 + j - (int32)(g_in.imPhase >> 16);
            run_case();
        }
    }
}

/* 7. Random sweep over the whole input space. */
static void phase_random(int n)
{
    int i;

    for (i = 0; i < n; i++)
    {
        base_state();

        g_in.cnt        = pool16((int)(rnd32() % POOL16_N));
        g_in.fcTime     = rnd16();
        g_in.fcBak      = rnd16();
        g_in.fcLimit    = pool16((int)(rnd32() % POOL16_N));
        g_in.fcSet      = rnd16();
        g_in.fcSetApply = (Uint16)(8 + (rnd32() % 113));   /* reachable [8,120] */
        g_in.varFcByTem = (Uint16)(rnd32() & 1);
        g_in.runStep    = rnd16();
        g_in.temp       = rnd16();
        g_in.invType    = rnd16();
        g_in.invVolt    = rnd16();
        g_in.ctrlMode   = rnd16();
        g_in.startDC    = (Uint16)(rnd32() & 1);
        g_in.stopDC     = (Uint16)(rnd32() & 1);
        g_in.msw        = rnd16();
        g_in.modType    = (Uint16)(rnd32() & 1);
        g_in.ctrMotor   = rnd16();
        g_in.switchLF   = rnd16();
        g_in.freqReal   = rnd_signed();
        g_in.freqSyn    = rnd_signed();

        g_in.synAbsFreq = rnd16();
        g_in.synModule  = (Uint16)(rnd32() & 1);
        g_in.synIndex   = (Uint16)(rnd32() % 10);
        g_in.synFlag    = (Uint16)(rnd32() % 3);
        g_in.synFcApply = pool32((int)(rnd32() % POOL32_N));
        g_in.asynFc     = pool32((int)(rnd32() % POOL32_N));
        g_in.fullFreq01 = 2001 + (rnd32() % 4002000);      /* reachable >= 2001 */

        g_in.softTune   = rnd16();
        g_in.softCoff   = rnd_wide();
        g_in.timerTim   = rnd32();

        g_in.motorType  = (Uint16)(rnd32() % 4);
        g_in.pgMode     = (Uint16)(rnd32() % 3);
        g_in.pgType     = (Uint16)(rnd32() % 4);
        g_in.estMstep   = (Uint16)(rnd32() % PAREST_LEN);
        g_in.tuneStep   = (Uint16)(rnd32() % 8);
        g_in.stepApply  = rnd_wide();
        g_in.imPhase    = rnd_wide();
        g_in.rotPos     = rnd_wide();
        g_in.rotZero    = rnd_wide();
        g_in.abzRef     = rnd_wide();
        g_in.feedPos    = rnd_wide();
        g_in.feedABZ    = rnd_wide();
        g_in.rtPos      = rnd_wide();
        g_in.posComp    = rnd_wide();
        g_in.estZero    = rnd_wide();
        g_in.voltPhaseApply = rnd_wide();

        g_in.deadComp     = rnd_wide();
        g_in.invCurFilter = rnd_wide();
        g_in.theta        = rnd_wide();
        g_in.thetaFilter  = rnd_wide();
        g_in.compPhase    = rnd_wide();

        g_fnSel = (int)(rnd32() % F_N);
        run_case();
    }
}

int main(int argc, char **argv)
{
    long nrand = 200000;
    long base;

    /* stdout unbuffered: if the bench ever dies the last printed line is the
     * scene, and a buffered block would be lost with the process. */
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc > 1)
    {
        nrand = atol(argv[1]);
    }

    /* The invariant the domain restrictions rest on: FullFreq01 is never 0, so
     * SynPWMAngleCal never divides by zero. (FcSetApply's floor of 8 is checked
     * where it is generated -- see the comment on phase_random.) Asserted here
     * rather than assumed: a future edit to the pool that lets 0 through would
     * otherwise show up as a bare crash with no explanation. */
    {
        int i;
        for (i = 0; i < (int)POOL32_N; i++)
        {
            if (g_pool32[i] == 0)
            {
                printf("FATAL: 32 位取值池里有 0 —— SynPWMAngleCal 会除零\n");
                return 2;
            }
        }
    }

    base = g_cases;
    phase_carrier_bounds();
    printf("  CalCarrierWaveFreq 边界         : %ld\n", g_cases - base);

    base = g_cases;
    phase_asyn_bounds();
    printf("  AsynPWMAngleCal 边界            : %ld\n", g_cases - base);

    base = g_cases;
    phase_syn_bounds();
    printf("  SynPWMAngleCal 边界             : %ld\n", g_cases - base);

    base = g_cases;
    phase_soft_bounds();
    printf("  SoftPWMProcess 边界             : %ld\n", g_cases - base);

    base = g_cases;
    phase_outphase_bounds();
    printf("  CalOutputPhase 边界             : %ld\n", g_cases - base);

    base = g_cases;
    phase_deadband_bounds();
    printf("  CalDeadBandComp 边界            : %ld\n", g_cases - base);

    base = g_cases;
    phase_random((int)nrand);
    printf("  随机                            : %ld\n", g_cases - base);

    printf("  宿主除零 (跳过比较)             : %ld\n", g_hosttraps);

    printf("MotorCarrier difftest: cases=%ld mismatches=%ld\n",
           g_cases, g_mismatches);
    return g_mismatches == 0 ? 0 : 1;
}
