/* Differential test driver for MotorVF.
 *
 * src/motor/MotorVF.c is the V/f (voltage-per-hertz) branch of the drive: the
 * V/f curve, torque boost, winding-temperature / slip compensation, over-current
 * and over-DC-bus step limiters, the high-frequency dead-band compensator and
 * the high-frequency current reconstruction.  Sixteen entry points, all driven
 * directly here; fifteen of them are `void f(void)`, and
 * CalOutVotInVFStatus(int) returns the volts-to-apply, which is compared too.
 *
 * ---------------------------------------------------------------------------
 * Shape of this bench: two independent copies of fifteen globals
 * ---------------------------------------------------------------------------
 * MotorVF DEFINES fifteen data objects (gVFPar, gVFAutoVar, gWsComp, gVarAvr,
 * gVfOsc, gOscAmp, gVfOverUdc, gOvCur2, gVfFreq2, gOvCur, gOvUdc, gHVfOscDamp,
 * gHVfOscIndex, gHVfDeadBandCompOpt, gHVfCur).  ref_tu.c / new_tu.c rename them
 * per side, so each translation unit owns a full set.
 *
 * Every case is therefore: zero BOTH sides, poke the same state inputs into
 * both, snapshot the shared ledger, call ref, capture; zero both again, restore
 * the ledger and the outside world, call new, capture; then memcmp the two
 * images.  The memcmp is byte-wise on purpose -- both sides are memset to zero
 * first, so padding is deterministic and a field the rewrite forgot to write
 * shows up as a byte difference instead of being masked by a field list that
 * also forgot it.  Field names are recovered afterwards only for the message.
 *
 * ---------------------------------------------------------------------------
 * What is compared
 * ---------------------------------------------------------------------------
 *   15 module globals            byte-wise, plus field names on failure
 *   gOutVolt / gPhase / gDeadBand / gIAmpTheta / gMainCmd
 *                                owned by other modules but WRITTEN here
 *   CalOutVotInVFStatus result   the return value
 *   PID / AlphBetaToDQ ledger    call count and the arguments handed in
 *
 * ---------------------------------------------------------------------------
 * What is NOT compared, and why
 * ---------------------------------------------------------------------------
 *   The bodies of PID / AlphBetaToDQ (other modules; ported faithfully in
 *   mock_vf.c so their feedback path stays alive, but not under test here).
 *   qsin / qsqrt / atan beyond "both sides call the same approximation".
 *   gVfOverUdc and gOvCur2 are declared and defined but no function in
 *   MotorVF.c reads or writes them; they are still in the image, they just
 *   cannot differ.
 *   gHostCpuCtl: MotorVF uses no EALLOW / EDIS / DINT / EINT, so the table is
 *   defined for the linker and never compared.
 *
 * ---------------------------------------------------------------------------
 * Divisions by zero: the region the bench refuses to enter
 * ---------------------------------------------------------------------------
 * The module divides by these quantities:
 *
 *   gMotorInfo.FreqPer        CalOutVotInVFStatus (4 places) and the
 *                             m_MotorFreq2 term for VFLineType 2
 *   gMotorInfo.Votage         CalOutVotInVFStatus, VFLineType 10/11
 *   VFLineFreq1/2/3 staircase CalOutVotInVFStatus, VFLineType 1 (the segment
 *                             width is a DIFFERENCE, so equal break points
 *                             divide by zero)
 *   gBasePar.FullFreq         CalTorqueUp
 *   gBasePar.FullFreq01       VfOverCurDeal
 *   gInvInfo.BaseUdc          VFOverMagneticControl
 *   gVFPar.ocGain             VfOverCurDeal (10000/6/ocGain and m_Data/6/ocGain)
 *   gMainCmd.FreqReal         VfOscIndexCalc
 *   VfCurrentIs               VFWsTorqueBoostComm
 *   m_WsCurrentIm             VFWSCompControl -- but this one IS guarded by
 *                             `if(0 == m_WsCurrentIm) m_WsCurrentIm = 1;`, i.e.
 *                             the guard is code under test.  See below.
 *   DbOptActHFreq - DbOptActLFreq + 1
 *                             HVfDeadBandComp
 *
 * On the target a zero divisor silently produces garbage; on the host it raises
 * #DE and kills the process, and the negative-control ledger records any rc != 0
 * as CAUGHT.  A crash is therefore NOT a verdict.  keep_divisors_nonzero() keeps
 * every one of them >= 1 from the same starting point on both sides, so the
 * comparison itself is untouched.  Stated plainly: the "divisor is zero" region
 * is not covered by any result below.
 *
 * THE ONE EXCEPTION is m_WsCurrentIm.  The driver deliberately feeds inputs that
 * make it 0 (so the guard fires and the `= 1` repair is exercised), because
 * "the rewrite dropped the repair" is exactly the class of bug this bench is
 * for.  What that costs: a negative-control variant that *removes* the guard
 * would divide by zero and be recorded as CAUGHT without ever comparing.  Such
 * a variant is therefore not legal here; the ones that ARE legal change the
 * repair value (1 -> 2), which is observable through the clamped m_WsWindage.
 *
 * ---------------------------------------------------------------------------
 * Loop shape: one factor at a time, plus the crosses that actually interact
 * ---------------------------------------------------------------------------
 * Each phase walks the factors that appear together in a condition (a threshold
 * against the value it is compared with, a mode against the branch it selects)
 * and sweeps the rest one at a time from a fixed baseline.  A full cross product
 * over these inputs is in the 10^7 range and would make every negative-control
 * variant pay for it.  The coverage table at the bottom is the real gate: a
 * branch counter left at zero fails the run even with zero mismatches.
 */

#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "MotorInclude.h"

/* ---- per-side exports ---------------------------------------------------- */

extern VF_INFO_STRUCT            ref_gVFPar;
extern VF_AUTO_TORQUEBOOST_VAR   ref_gVFAutoVar;
extern VF_WS_COMP_STRUCT         ref_gWsComp;
extern VAR_AVR_STRUCT            ref_gVarAvr;
extern OVER_SHOCK_STRUCT         ref_gVfOsc;
extern THREE_ORDER_FILTER        ref_gOscAmp;
extern VF_OVER_UDC_DAMP          ref_gVfOverUdc;
extern OVER_CURRENT_DAMP2        ref_gOvCur2;
extern VF_FREQ_DEAL              ref_gVfFreq2;
extern OVER_CURRENT_DAMP         ref_gOvCur;
extern OVER_UDC_CTL_STRUCT       ref_gOvUdc;
extern HVF_OSC_DAMP_STRUCT       ref_gHVfOscDamp;
extern HVF_OSC_JUDGE_INDEX       ref_gHVfOscIndex;
extern HVF_DB_COMP_OPT           ref_gHVfDeadBandCompOpt;
extern MT_STRUCT                 ref_gHVfCur;

extern VF_INFO_STRUCT            new_gVFPar;
extern VF_AUTO_TORQUEBOOST_VAR   new_gVFAutoVar;
extern VF_WS_COMP_STRUCT         new_gWsComp;
extern VAR_AVR_STRUCT            new_gVarAvr;
extern OVER_SHOCK_STRUCT         new_gVfOsc;
extern THREE_ORDER_FILTER        new_gOscAmp;
extern VF_OVER_UDC_DAMP          new_gVfOverUdc;
extern OVER_CURRENT_DAMP2        new_gOvCur2;
extern VF_FREQ_DEAL              new_gVfFreq2;
extern OVER_CURRENT_DAMP         new_gOvCur;
extern OVER_UDC_CTL_STRUCT       new_gOvUdc;
extern HVF_OSC_DAMP_STRUCT       new_gHVfOscDamp;
extern HVF_OSC_JUDGE_INDEX       new_gHVfOscIndex;
extern HVF_DB_COMP_OPT           new_gHVfDeadBandCompOpt;
extern MT_STRUCT                 new_gHVfCur;

extern void ref_VfVarInitiate(void);
extern void ref_VFOVUdcLimit(void);
extern int  ref_CalOutVotInVFStatus(int freq);
extern void ref_CalTorqueUp(void);
extern void ref_VFWsTorqueBoostComm(void);
extern void ref_VFAutoTorqueBoost(void);
extern void ref_VFWSCompControl(void);
extern void ref_VFOverMagneticControl(void);
extern void ref_VFSpeedControl(void);
extern void ref_VfOverCurDeal(void);
extern void ref_VfOverUdcDeal(void);
extern void ref_VfFreqDeal(void);
extern void ref_VfOscIndexCalc(void);
extern void ref_HVfDeadBandComp(void);
extern void ref_HVfOscDampDeal(void);
extern void ref_HVfCurReDecomp(void);

extern void new_VfVarInitiate(void);
extern void new_VFOVUdcLimit(void);
extern int  new_CalOutVotInVFStatus(int freq);
extern void new_CalTorqueUp(void);
extern void new_VFWsTorqueBoostComm(void);
extern void new_VFAutoTorqueBoost(void);
extern void new_VFWSCompControl(void);
extern void new_VFOverMagneticControl(void);
extern void new_VFSpeedControl(void);
extern void new_VfOverCurDeal(void);
extern void new_VfOverUdcDeal(void);
extern void new_VfFreqDeal(void);
extern void new_VfOscIndexCalc(void);
extern void new_HVfDeadBandComp(void);
extern void new_HVfOscDampDeal(void);
extern void new_HVfCurReDecomp(void);

/* ---- the shared mock's ledger ------------------------------------------- */

extern volatile Uint16 gHostCpuCtl[7];
extern unsigned short  gVfPidCount;
extern PID_STRUCT      gVfPidArg;
extern unsigned short  gVfAbDqCount;
extern int             gVfAbDqAngle;
extern long            gVfAbDqAlph;
extern long            gVfAbDqBeta;

#define FAIL_LIMIT   20

static long g_cases;
static int  g_fails;
static int  g_reported;

/* ---- comparison image ---------------------------------------------------- */

typedef struct IMG
{
    VF_INFO_STRUCT          vfPar;
    VF_AUTO_TORQUEBOOST_VAR vfAutoVar;
    VF_WS_COMP_STRUCT       wsComp;
    VAR_AVR_STRUCT          varAvr;
    OVER_SHOCK_STRUCT       vfOsc;
    THREE_ORDER_FILTER      oscAmp;
    VF_OVER_UDC_DAMP        vfOverUdc;
    OVER_CURRENT_DAMP2      ovCur2;
    VF_FREQ_DEAL            vfFreq2;
    OVER_CURRENT_DAMP       ovCur;
    OVER_UDC_CTL_STRUCT     ovUdc;
    HVF_OSC_DAMP_STRUCT     hvFoscDamp;
    HVF_OSC_JUDGE_INDEX     hvFoscIndex;
    HVF_DB_COMP_OPT         hvFdeadBandCompOpt;
    MT_STRUCT               hvFCur;

    /* Owned by other translation units, written by this one.  They have to be
     * in the WORLD snapshot anyway (ref's leftovers must not become new's
     * inputs); putting them here as well is what makes "the rewrite wrote the
     * wrong field" visible. */
    OUT_VOLT_STRUCT         outVolt;
    ANGLE_STRUCT            phase;
    DEAD_BAND_STRUCT        deadBand;
    AMPTHETA_STRUCT         iAmpTheta;
    BASE_COMMAND_STRUCT     mainCmd;

    /* CalOutVotInVFStatus returns the volts to apply. */
    long                    ret;

    unsigned short          pidCnt;
    PID_STRUCT              pidArg;
    unsigned short          abDqCnt;
    int                     abDqAngle;
    long                    abDqAlph;
    long                    abDqBeta;
} IMG;

typedef struct FIELD
{
    const char *name;
    unsigned    off;
    unsigned    size;
} FIELD;

#define F(n, m) { n, (unsigned)offsetof(IMG, m), (unsigned)sizeof(((IMG *)0)->m) }

static const FIELD g_fields[] =
{
    F("gVFPar",           vfPar),
    F("gVFAutoVar",       vfAutoVar),
    F("gWsComp",          wsComp),
    F("gVarAvr",          varAvr),
    F("gVfOsc",           vfOsc),
    F("gOscAmp",          oscAmp),
    F("gVfOverUdc",       vfOverUdc),
    F("gOvCur2",          ovCur2),
    F("gVfFreq2",         vfFreq2),
    F("gOvCur",           ovCur),
    F("gOvUdc",           ovUdc),
    F("gHVfOscDamp",      hvFoscDamp),
    F("gHVfOscIndex",     hvFoscIndex),
    F("gHVfDeadBandComp", hvFdeadBandCompOpt),
    F("gHVfCur",          hvFCur),
    F("gOutVolt",         outVolt),
    F("gPhase",           phase),
    F("gDeadBand",        deadBand),
    F("gIAmpTheta",       iAmpTheta),
    F("gMainCmd",         mainCmd),
    F("return value",     ret),
    F("PID count",        pidCnt),
    F("PID arg log",      pidArg),
    F("AlphBetaToDQ cnt", abDqCnt),
    F("AlphBetaToDQ ang", abDqAngle),
    F("AlphBetaToDQ Alph",abDqAlph),
    F("AlphBetaToDQ Beta",abDqBeta)
};

#define NFIELDS ((int)(sizeof(g_fields) / sizeof(g_fields[0])))

static void cov_observe(const char *fn, const IMG *a);

static void capture_ref(IMG *p)
{
    p->vfPar             = ref_gVFPar;
    p->vfAutoVar         = ref_gVFAutoVar;
    p->wsComp            = ref_gWsComp;
    p->varAvr            = ref_gVarAvr;
    p->vfOsc             = ref_gVfOsc;
    p->oscAmp            = ref_gOscAmp;
    p->vfOverUdc         = ref_gVfOverUdc;
    p->ovCur2            = ref_gOvCur2;
    p->vfFreq2           = ref_gVfFreq2;
    p->ovCur             = ref_gOvCur;
    p->ovUdc             = ref_gOvUdc;
    p->hvFoscDamp        = ref_gHVfOscDamp;
    p->hvFoscIndex       = ref_gHVfOscIndex;
    p->hvFdeadBandCompOpt= ref_gHVfDeadBandCompOpt;
    p->hvFCur            = ref_gHVfCur;

    p->outVolt           = gOutVolt;
    p->phase             = gPhase;
    p->deadBand          = gDeadBand;
    p->iAmpTheta         = gIAmpTheta;
    p->mainCmd           = gMainCmd;

    p->pidCnt            = gVfPidCount;
    p->pidArg            = gVfPidArg;
    p->abDqCnt           = gVfAbDqCount;
    p->abDqAngle         = gVfAbDqAngle;
    p->abDqAlph          = gVfAbDqAlph;
    p->abDqBeta          = gVfAbDqBeta;
}

static void capture_new(IMG *p)
{
    p->vfPar             = new_gVFPar;
    p->vfAutoVar         = new_gVFAutoVar;
    p->wsComp            = new_gWsComp;
    p->varAvr            = new_gVarAvr;
    p->vfOsc             = new_gVfOsc;
    p->oscAmp            = new_gOscAmp;
    p->vfOverUdc         = new_gVfOverUdc;
    p->ovCur2            = new_gOvCur2;
    p->vfFreq2           = new_gVfFreq2;
    p->ovCur             = new_gOvCur;
    p->ovUdc             = new_gOvUdc;
    p->hvFoscDamp        = new_gHVfOscDamp;
    p->hvFoscIndex       = new_gHVfOscIndex;
    p->hvFdeadBandCompOpt= new_gHVfDeadBandCompOpt;
    p->hvFCur            = new_gHVfCur;

    p->outVolt           = gOutVolt;
    p->phase             = gPhase;
    p->deadBand          = gDeadBand;
    p->iAmpTheta         = gIAmpTheta;
    p->mainCmd           = gMainCmd;

    p->pidCnt            = gVfPidCount;
    p->pidArg            = gVfPidArg;
    p->abDqCnt           = gVfAbDqCount;
    p->abDqAngle         = gVfAbDqAngle;
    p->abDqAlph          = gVfAbDqAlph;
    p->abDqBeta          = gVfAbDqBeta;
}

/* ---- module-owned globals: zeroed before every case, poked as inputs ----- */

static void mod_zero(void)
{
    memset(&ref_gVFPar,             0, sizeof(ref_gVFPar));
    memset(&ref_gVFAutoVar,         0, sizeof(ref_gVFAutoVar));
    memset(&ref_gWsComp,            0, sizeof(ref_gWsComp));
    memset(&ref_gVarAvr,            0, sizeof(ref_gVarAvr));
    memset(&ref_gVfOsc,             0, sizeof(ref_gVfOsc));
    memset(&ref_gOscAmp,            0, sizeof(ref_gOscAmp));
    memset(&ref_gVfOverUdc,         0, sizeof(ref_gVfOverUdc));
    memset(&ref_gOvCur2,            0, sizeof(ref_gOvCur2));
    memset(&ref_gVfFreq2,           0, sizeof(ref_gVfFreq2));
    memset(&ref_gOvCur,             0, sizeof(ref_gOvCur));
    memset(&ref_gOvUdc,             0, sizeof(ref_gOvUdc));
    memset(&ref_gHVfOscDamp,        0, sizeof(ref_gHVfOscDamp));
    memset(&ref_gHVfOscIndex,       0, sizeof(ref_gHVfOscIndex));
    memset(&ref_gHVfDeadBandCompOpt,0, sizeof(ref_gHVfDeadBandCompOpt));
    memset(&ref_gHVfCur,            0, sizeof(ref_gHVfCur));

    memset(&new_gVFPar,             0, sizeof(new_gVFPar));
    memset(&new_gVFAutoVar,         0, sizeof(new_gVFAutoVar));
    memset(&new_gWsComp,            0, sizeof(new_gWsComp));
    memset(&new_gVarAvr,            0, sizeof(new_gVarAvr));
    memset(&new_gVfOsc,             0, sizeof(new_gVfOsc));
    memset(&new_gOscAmp,            0, sizeof(new_gOscAmp));
    memset(&new_gVfOverUdc,         0, sizeof(new_gVfOverUdc));
    memset(&new_gOvCur2,            0, sizeof(new_gOvCur2));
    memset(&new_gVfFreq2,           0, sizeof(new_gVfFreq2));
    memset(&new_gOvCur,             0, sizeof(new_gOvCur));
    memset(&new_gOvUdc,             0, sizeof(new_gOvUdc));
    memset(&new_gHVfOscDamp,        0, sizeof(new_gHVfOscDamp));
    memset(&new_gHVfOscIndex,       0, sizeof(new_gHVfOscIndex));
    memset(&new_gHVfDeadBandCompOpt,0, sizeof(new_gHVfDeadBandCompOpt));
    memset(&new_gHVfCur,            0, sizeof(new_gHVfCur));
}

/* ---- the module's own state, carried in as INPUT ------------------------- *
 * Nearly every function reads its own previous output:
 *   VfVarInitiate          reads gMainCmd.FreqDesired / IMPhase / Command bits
 *   VFOVUdcLimit           reads OvUdcLimitTime, StepApply, Limit
 *   CalOutVotInVFStatus    reads gVFPar entirely
 *   CalTorqueUp            reads AutoTorquePID.Out, VfTorqueEnableTime
 *   VFWsTorqueBoostComm    reads VfCurrentIs, VfReverseAngle, VfRIsSinFai,
 *                          VfRVCosFai, WsCompMT.T/.M
 *   VFAutoTorqueBoost      reads VfRIsSinFai, VfRVCosFai, VfAngleSin,
 *                          VfTorqueEnableTime, VfReverseVolt, DestinationVolt,
 *                          AutoTorquePID.Total
 *   VFWSCompControl        reads DelayTime, WsCompMT, WsCompMTApply, CompFreq
 *   VFOverMagneticControl  reads UDCFilter
 *   VfOverCurDeal          reads preSpdFlag, Flag, StepLow, StepLowLim,
 *                          StepHigh, CurBak
 *   VfOverUdcDeal          reads CoffApply, Flag, StepBak, ExeCnt, UdcBak,
 *                          FreqMax, StepApply, LastStepApply, AccTimes
 *   VfFreqDeal             reads StepApply of both limiters, LastStepApply,
 *                          AccTimes, FreqApply, OvUdcLimitTime
 *   VfOscIndexCalc         reads AnglePowerFactor, wCntUse, wCntRltm,
 *                          maxAngle, minAngle
 *   HVfDeadBandComp        reads CurPhaseFeed, CurPhaseFeed_pre,
 *                          CurPhasePredict, DbOptActHFreq/LFreq,
 *                          DbCompCpwmWidth
 *
 * Those are inputs to the step under test, so they must be poked into BOTH sides
 * identically -- otherwise "the rewrite reads a stale value" would compare two
 * different starting states instead of one implementation.  They cannot be poked
 * at the call site because run() zeroes both sides first, so the driver keeps the
 * intended state in `st` and run() applies it right after the zeroing.
 *
 * The whole struct is carried, not a hand-picked field list: a list has to be
 * maintained, and the field that gets forgotten is the field the mutation
 * writes. */
typedef struct STATE
{
    VF_INFO_STRUCT          vfPar;
    VF_AUTO_TORQUEBOOST_VAR vfAutoVar;
    VF_WS_COMP_STRUCT       wsComp;
    VAR_AVR_STRUCT          varAvr;
    OVER_SHOCK_STRUCT       vfOsc;
    THREE_ORDER_FILTER      oscAmp;
    VF_OVER_UDC_DAMP        vfOverUdc;
    OVER_CURRENT_DAMP2      ovCur2;
    VF_FREQ_DEAL            vfFreq2;
    OVER_CURRENT_DAMP       ovCur;
    OVER_UDC_CTL_STRUCT     ovUdc;
    HVF_OSC_DAMP_STRUCT     hvFoscDamp;
    HVF_OSC_JUDGE_INDEX     hvFoscIndex;
    HVF_DB_COMP_OPT         hvFdeadBandCompOpt;
    MT_STRUCT               hvFCur;
} STATE;

static STATE st;

static void state_default(void)
{
    memset(&st, 0, sizeof(st));

    st.vfPar.VFLineType    = 0;
    st.vfPar.VFTorqueUp    = 100;
    st.vfPar.VFTorqueUpLim = 800;
    st.vfPar.VFWsComp      = 0;
    st.vfPar.VFOvShock     = 0;
    st.vfPar.VFLineFreq1   = 200;
    st.vfPar.VFLineFreq2   = 400;
    st.vfPar.VFLineFreq3   = 600;
    st.vfPar.VFLineVolt1   = 300;
    st.vfPar.VFLineVolt2   = 600;
    st.vfPar.VFLineVolt3   = 900;
    st.vfPar.ovGain        = 10;
    st.vfPar.ovPoint       = 100;
    st.vfPar.ocGain        = 20;
    st.vfPar.ocPoint       = 100;
    st.vfPar.FreqApply     = 100;

    st.vfAutoVar.VfCurrentIs        = 1000;
    st.vfAutoVar.VfReverseAngle     = 1000;
    st.vfAutoVar.VfRIsSinFai        = 100;
    st.vfAutoVar.VfRVCosFai         = 3000;
    st.vfAutoVar.VfAngleSin         = 16000;
    st.vfAutoVar.VfReverseVolt      = 100;
    st.vfAutoVar.VfTorqueEnableTime = 0;
    st.vfAutoVar.DestinationVolt    = 3000;
    st.vfAutoVar.AutoTorquePID.Total = 0;
    st.vfAutoVar.AutoTorquePID.Out   = 100 << 16;

    st.wsComp.Coff     = 0;
    st.wsComp.CompFreq = 0;
    st.wsComp.DelayTime = 200;
    st.wsComp.FilterCoff = 100;
    st.wsComp.WsCompMT.M = 100;
    st.wsComp.WsCompMT.T = 200;
    st.wsComp.WsCompMTApply.M = 100;
    st.wsComp.WsCompMTApply.T = 200;

    st.varAvr.UDCFilter = 4000;
    st.varAvr.CoffApply = 10;

    st.oscAmp.InData1 = 10;
    st.oscAmp.Indata2 = 10;
    st.oscAmp.Indata3 = 10;
    st.oscAmp.OutData1 = 10;
    st.oscAmp.OutData2 = 10;
    st.oscAmp.OutData3 = 10;

    st.vfFreq2.preSpdFlag = C_SPEED_FLAG_CON;
    st.vfFreq2.spedChg    = 0;

    st.ovCur.StepLow     = 10;
    st.ovCur.StepLowLim  = 20;
    st.ovCur.StepHigh    = 0;
    st.ovCur.CurBak      = 1000;
    st.ovCur.CurLim      = 4096;
    st.ovCur.SubStep     = 1;
    st.ovCur.Flag        = 0;

    st.ovUdc.CoffApply   = 20;
    st.ovUdc.Limit       = 4000;
    st.ovUdc.StepBak     = 0;
    st.ovUdc.ExeCnt      = 0;
    st.ovUdc.UdcBak      = 5000;
    st.ovUdc.Flag        = 0;
    st.ovUdc.FreqMax     = 100;
    st.ovUdc.AccTimes    = 0;

    st.hvFoscDamp.CurMagSet = 100;
    st.hvFoscDamp.VoltAmp   = 3000;

    st.hvFoscIndex.AnglePowerFactor = 1000;
    st.hvFoscIndex.wCntUse          = 0;
    st.hvFoscIndex.wCntRltm         = 200;
    st.hvFoscIndex.maxAngle         = 1000;
    st.hvFoscIndex.minAngle         = 1000;

    st.hvFdeadBandCompOpt.CurPhaseFeed     = 0;
    st.hvFdeadBandCompOpt.CurPhaseFeed_pre = 0;
    st.hvFdeadBandCompOpt.CurPhasePredict  = 0;
    st.hvFdeadBandCompOpt.StepPhaseSet     = 100;
    st.hvFdeadBandCompOpt.CurPhaseStepFed  = 100;
    st.hvFdeadBandCompOpt.DbOptActHFreq    = 1000;
    st.hvFdeadBandCompOpt.DbOptActLFreq    = 300;
    st.hvFdeadBandCompOpt.DbCompCpwmWidth  = 10;

    st.hvFCur.M = 100;
    st.hvFCur.T = 100;
}

static void state_apply(void)
{
    ref_gVFPar = st.vfPar;
    new_gVFPar = st.vfPar;

    ref_gVFAutoVar = st.vfAutoVar;
    new_gVFAutoVar = st.vfAutoVar;

    ref_gWsComp = st.wsComp;
    new_gWsComp = st.wsComp;

    ref_gVarAvr = st.varAvr;
    new_gVarAvr = st.varAvr;

    ref_gVfOsc = st.vfOsc;
    new_gVfOsc = st.vfOsc;

    ref_gOscAmp = st.oscAmp;
    new_gOscAmp = st.oscAmp;

    ref_gVfOverUdc = st.vfOverUdc;
    new_gVfOverUdc = st.vfOverUdc;

    ref_gOvCur2 = st.ovCur2;
    new_gOvCur2 = st.ovCur2;

    ref_gVfFreq2 = st.vfFreq2;
    new_gVfFreq2 = st.vfFreq2;

    ref_gOvCur = st.ovCur;
    new_gOvCur = st.ovCur;

    ref_gOvUdc = st.ovUdc;
    new_gOvUdc = st.ovUdc;

    ref_gHVfOscDamp = st.hvFoscDamp;
    new_gHVfOscDamp = st.hvFoscDamp;

    ref_gHVfOscIndex = st.hvFoscIndex;
    new_gHVfOscIndex = st.hvFoscIndex;

    ref_gHVfDeadBandCompOpt = st.hvFdeadBandCompOpt;
    new_gHVfDeadBandCompOpt = st.hvFdeadBandCompOpt;

    ref_gHVfCur = st.hvFCur;
    new_gHVfCur = st.hvFCur;
}

/* ---- the outside world must be restored between the two sides, too -------
 * This module WRITES globals owned by other translation units:
 *
 *   gMainCmd.FreqSyn          VFWSCompControl, VFSpeedControl
 *   gOutVolt.Volt             CalTorqueUp, VFSpeedControl
 *   gOutVolt.VoltApply        VFOverMagneticControl
 *   gOutVolt.VoltPhaseApply   VfVarInitiate
 *   gOutVolt.detVfVoltUp      CalTorqueUp
 *   gOutVolt.antiVolt         HVfCurReDecomp
 *   gPhase.OutPhase           VfVarInitiate
 *   gDeadBand.CompU/V/W       HVfDeadBandComp
 *   gIAmpTheta.ThetaFilter    HVfDeadBandComp
 *
 * Those are SINGLE objects shared by both sides, so without a restore the ref
 * call's leftovers become the new call's inputs.  The symptom is not subtle but
 * it is misleading: ref and new are the SAME code at that point, so any mismatch
 * is proof the bench is comparing two different starting states.
 *
 * Everything else in the list is read-only for this module and is carried along
 * because a save/restore pair that has to be maintained is a save/restore pair
 * that will eventually be wrong. */
typedef struct WORLD
{
    MOTOR_STRUCT                    motorInfo;
    MOTOR_EXTERN_STRUCT             motorExtPer;
    MOTOR_EXTERN_STRUCT             motorExtInfo;
    RUN_STATUS_STRUCT               mainStatus;
    BASE_COMMAND_STRUCT             mainCmd;
    BASE_PAR_STRUCT                 basePar;
    CONTROL_MOTOR_TYPE_ENUM         ctrMotorType;
    INV_STRUCT                      invInfo;
    UDC_STRUCT                      udc;
    LINE_CURRENT_STRUCT             lineCur;
    MT_STRUCT                       imtq12;
    MT_STRUCT_Q24                   imtq24;
    ALPHABETA_STRUCT                iAlphBeta;
    AMPTHETA_STRUCT                 iAmpTheta;
    MAIN_COMMAND_EXTEND_UNION       extendCmd;
    OUT_VOLT_STRUCT                 outVolt;
    ANGLE_STRUCT                    phase;
    DEAD_BAND_STRUCT                deadBand;
} WORLD;

static void world_save(WORLD *w)
{
    w->motorInfo    = gMotorInfo;
    w->motorExtPer  = gMotorExtPer;
    w->motorExtInfo = gMotorExtInfo;
    w->mainStatus   = gMainStatus;
    w->mainCmd      = gMainCmd;
    w->basePar      = gBasePar;
    w->ctrMotorType = gCtrMotorType;
    w->invInfo      = gInvInfo;
    w->udc          = gUDC;
    w->lineCur      = gLineCur;
    w->imtq12       = gIMTQ12;
    w->imtq24       = gIMTQ24;
    w->iAlphBeta    = gIAlphBeta;
    w->iAmpTheta    = gIAmpTheta;
    w->extendCmd    = gExtendCmd;
    w->outVolt      = gOutVolt;
    w->phase        = gPhase;
    w->deadBand     = gDeadBand;
}

static void world_load(const WORLD *w)
{
    gMotorInfo     = w->motorInfo;
    gMotorExtPer   = w->motorExtPer;
    gMotorExtInfo  = w->motorExtInfo;
    gMainStatus    = w->mainStatus;
    gMainCmd       = w->mainCmd;
    gBasePar       = w->basePar;
    gCtrMotorType  = w->ctrMotorType;
    gInvInfo       = w->invInfo;
    gUDC           = w->udc;
    gLineCur       = w->lineCur;
    gIMTQ12        = w->imtq12;
    gIMTQ24        = w->imtq24;
    gIAlphBeta     = w->iAlphBeta;
    gIAmpTheta     = w->iAmpTheta;
    gExtendCmd     = w->extendCmd;
    gOutVolt       = w->outVolt;
    gPhase         = w->phase;
    gDeadBand      = w->deadBand;
}

/* ---- the shared ledger must be restored between the two sides ------------ */

typedef struct LEDGER
{
    unsigned short pid;
    PID_STRUCT     pidArg;
    unsigned short abDq;
    int            abDqAngle;
    long           abDqAlph;
    long           abDqBeta;
} LEDGER;

static void ledger_save(LEDGER *l)
{
    l->pid       = gVfPidCount;
    l->pidArg    = gVfPidArg;
    l->abDq      = gVfAbDqCount;
    l->abDqAngle = gVfAbDqAngle;
    l->abDqAlph  = gVfAbDqAlph;
    l->abDqBeta  = gVfAbDqBeta;
}

static void ledger_restore(const LEDGER *l)
{
    gVfPidCount  = l->pid;
    gVfPidArg    = l->pidArg;
    gVfAbDqCount = l->abDq;
    gVfAbDqAngle = l->abDqAngle;
    gVfAbDqAlph  = l->abDqAlph;
    gVfAbDqBeta  = l->abDqBeta;
}

/* ---- input baseline ------------------------------------------------------ */

static void baseline(void)
{
    state_default();

    memset(&gMotorInfo,   0, sizeof(gMotorInfo));
    memset(&gMotorExtPer, 0, sizeof(gMotorExtPer));
    memset(&gMotorExtInfo,0, sizeof(gMotorExtInfo));
    memset(&gMainStatus,  0, sizeof(gMainStatus));
    memset(&gMainCmd,     0, sizeof(gMainCmd));
    memset(&gBasePar,     0, sizeof(gBasePar));
    memset(&gInvInfo,     0, sizeof(gInvInfo));
    memset(&gUDC,         0, sizeof(gUDC));
    memset(&gLineCur,     0, sizeof(gLineCur));
    memset(&gIMTQ12,      0, sizeof(gIMTQ12));
    memset(&gIMTQ24,      0, sizeof(gIMTQ24));
    memset(&gIAlphBeta,   0, sizeof(gIAlphBeta));
    memset(&gIAmpTheta,   0, sizeof(gIAmpTheta));
    memset(&gExtendCmd,   0, sizeof(gExtendCmd));
    memset(&gOutVolt,     0, sizeof(gOutVolt));
    memset(&gPhase,       0, sizeof(gPhase));
    memset(&gDeadBand,    0, sizeof(gDeadBand));

    gMotorInfo.MotorType  = MOTOR_TYPE_IM;
    gMotorInfo.FreqPer    = 500;
    gMotorInfo.Votage     = 380;
    gMotorInfo.Frequency  = 800;
    gMotorInfo.Current    = 1000;
    gMotorInfo.CurrentGet = 1000;

    gMotorExtPer.UnitCoff   = 1;
    gMotorExtPer.R1         = 300;
    gMotorExtPer.I0         = 300;
    gMotorExtPer.IoVsFreq   = 250;
    gMotorExtPer.RatedComp  = 100;

    gMotorExtInfo.UnitCoff  = 1;
    gMotorExtInfo.R1        = 300;

    gBasePar.FullFreq    = 100;
    gBasePar.FullFreq01  = 32768L;
    gBasePar.FcSetApply  = 100;

    gInvInfo.InvTypeApply = 18;
    gInvInfo.BaseUdc      = 4000;
    gInvInfo.InvCurrent   = 1000;

    gUDC.uDC       = 5000;
    gUDC.uDCFilter = 5000;

    gLineCur.CurPer     = 1000;
    gLineCur.CurBaseInv = 1000;

    gIMTQ12.M = 100;
    gIMTQ12.T = 100;
    gIMTQ24.M = 100L << 12;
    gIMTQ24.T = 100L << 12;

    gIAlphBeta.Alph = 1000;
    gIAlphBeta.Beta = 500;

    gIAmpTheta.Theta      = 100;
    gIAmpTheta.PowerAngle = 100;
    gIAmpTheta.Amp        = 1000;

    gMainCmd.pu2siCoeff     = 1;
    gMainCmd.FreqReal       = 1000;
    gMainCmd.FreqDesired    = 100;
    gMainCmd.FreqDesiredReal= 100;
    gMainCmd.FreqSet        = 100;
    gMainCmd.FreqSetApply   = 100;
    gMainCmd.FreqSyn        = 100;
    gMainCmd.Command.bit.SpeedFlag = C_SPEED_FLAG_CON;

    gMainStatus.RunStep = STATUS_RUN;
    gCtrMotorType       = ASYNC_VF;

    gOutVolt.Volt       = 3000;
    gOutVolt.VoltApply  = 3000;
    gOutVolt.MaxOutVolt = 4000;
    gOutVolt.vfSplit    = 3000;

    gPhase.IMPhase   = 0;
    gPhase.StepPhase = 0;
    gPhase.CompPhase = 0;

    gDeadBand.Comp = 10;

    gVfPidCount  = 0;
    gVfAbDqCount = 0;
    memset(&gVfPidArg, 0, sizeof(gVfPidArg));
    gVfAbDqAngle = 0;
    gVfAbDqAlph  = 0;
    gVfAbDqBeta  = 0;
}

/* ---- keeping the divisors non-zero -------------------------------------- *
 * See the long note at the top of this file.  Applied once per case, before
 * either side runs, so both sides see the same input and the comparison itself
 * is untouched.  `fn` decides which repairs are needed: ocGain == 0 is a legal
 * input to VfFreqDeal (`if(gVFPar.ocGain == 0) m_StepOI = 32767;`) but a divide
 * fault in VfOverCurDeal, so it must not be repaired globally.
 */
static void keep_divisors_nonzero(const char *fn)
{
    /* FreqPer is a divisor in CalOutVotInVFStatus (four sites) and sets
     * m_MotorFreq2 = (FreqPer*FreqPer)>>15 for VFLineType 2, which is also a
     * divisor.  200 is the smallest value that keeps the latter >= 1. */
    if (gMotorInfo.FreqPer < 200)
    {
        gMotorInfo.FreqPer = 200;
    }
    if (gMotorInfo.Votage == 0)
    {
        gMotorInfo.Votage = 1;
    }
    if (gBasePar.FullFreq == 0)
    {
        gBasePar.FullFreq = 1;
    }
    if (gBasePar.FullFreq01 == 0)
    {
        gBasePar.FullFreq01 = 1;
    }
    if (gInvInfo.BaseUdc == 0)
    {
        gInvInfo.BaseUdc = 1;
    }

    /* VFLineType 1 divides by a DIFFERENCE of break points, so they have to be
     * strictly increasing -- equal neighbours divide by zero. */
    if (st.vfPar.VFLineType == 1)
    {
        if (gMainCmd.pu2siCoeff == 0)
        {
            gMainCmd.pu2siCoeff = 1;
        }
        if (st.vfPar.VFLineFreq1 == 0)
        {
            st.vfPar.VFLineFreq1 = 1;
        }
        if (st.vfPar.VFLineFreq2 <= st.vfPar.VFLineFreq1)
        {
            st.vfPar.VFLineFreq2 = (Uint)(st.vfPar.VFLineFreq1 + 1);
        }
        if (st.vfPar.VFLineFreq3 <= st.vfPar.VFLineFreq2)
        {
            st.vfPar.VFLineFreq3 = (Uint)(st.vfPar.VFLineFreq2 + 1);
        }
        if (gMotorInfo.Frequency <= st.vfPar.VFLineFreq3)
        {
            gMotorInfo.Frequency = (Uint)(st.vfPar.VFLineFreq3 + 1);
        }
    }

    if (0 == strcmp(fn, "CalTorqueUp"))
    {
        /* m_MaxFreqUp is a divisor at
         *   m_VoltUp = ((Ulong)m_DetaFreq * (Ulong)m_ZeroUp)/(Uint)m_MaxFreqUp;
         * In the original code m_MaxFreqUp == 0 is harmless: the guard
         * `m_DetaFreq > 0` cannot hold when the minuend is 0.  A variant that
         * perturbs m_DetaFreq (CU5: `-` -> `+`) does reach the division and
         * dies with 0xC0000094 (integer divide by zero) -- and a crash is not
         * a verdict.  Pin the truncated quotient non-zero.  VFTorqueUpLim is
         * 16 bits, so `need` below is bounded by 2 for any FullFreq. */
        int up = (int)((((Ulong)st.vfPar.VFTorqueUpLim) << 15)
                       / (Ulong)gBasePar.FullFreq);
        if (up == 0)
        {
            Ulong need = (((Ulong)gBasePar.FullFreq) * 1000UL + 32767UL) / 32768UL;
            if (need == 0UL)    need = 1UL;
            if (need > 65535UL) need = 65535UL;
            st.vfPar.VFTorqueUpLim = (Uint)need;
            up = (int)((((Ulong)st.vfPar.VFTorqueUpLim) << 15)
                       / (Ulong)gBasePar.FullFreq);
            if (up == 0)
            {
                st.vfPar.VFTorqueUpLim = 65535;
            }
        }
    }

    if (0 == strcmp(fn, "VfOverCurDeal") && st.vfPar.ocGain == 0)
    {
        st.vfPar.ocGain = 1;
    }
    if (0 == strcmp(fn, "VfOscIndexCalc") && gMainCmd.FreqReal == 0)
    {
        gMainCmd.FreqReal = 1;
    }
    if (0 == strcmp(fn, "HVfDeadBandComp")
        && (gMainCmd.FreqReal >= 800) && (gMainCmd.FreqReal < 1200)
        && (0 == (long)st.hvFdeadBandCompOpt.DbOptActHFreq
                    - (long)st.hvFdeadBandCompOpt.DbOptActLFreq + 1))
    {
        st.hvFdeadBandCompOpt.DbOptActLFreq = (int)(st.hvFdeadBandCompOpt.DbOptActHFreq + 2);
    }
}

/* ---- one comparison ------------------------------------------------------ */

static void report(const char *name, const IMG *a, const IMG *b, int step)
{
    int i;
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;

    g_fails++;
    if (g_reported >= FAIL_LIMIT)
    {
        return;
    }
    g_reported++;

    if (step < 0)
    {
        printf("  MISMATCH %s\n", name);
    }
    else
    {
        printf("  MISMATCH %s (step %d)\n", name, step);
    }

    for (i = 0; i < NFIELDS; i++)
    {
        unsigned k;
        int first = -1;

        for (k = 0; k < g_fields[i].size; k++)
        {
            if (pa[g_fields[i].off + k] != pb[g_fields[i].off + k])
            {
                first = (int)k;
                break;
            }
        }
        if (first >= 0)
        {
            printf("    %-20s first diff at byte %2d\n", g_fields[i].name, first);
        }
    }
}

/* Call both sides from the identical starting state and compare.
 *
 * The order matters: ref runs, its image is captured, then BOTH sides are zeroed
 * again and the shared ledger and the outside world are restored before new
 * runs.  Without the restore the second side would start with the first side's
 * call counter, and every count comparison would be "1 vs 2" forever. */
static void run(void (*rf)(void), void (*nf)(void), const char *name)
{
    IMG a;
    IMG b;
    LEDGER l;
    WORLD w;

    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));

    keep_divisors_nonzero(name);

    mod_zero();
    state_apply();
    ledger_save(&l);
    world_save(&w);

    rf();
    capture_ref(&a);

    mod_zero();
    state_apply();
    ledger_restore(&l);
    world_load(&w);

    /* Classified here, not right after rf(): the ref call may have written
     * gMainCmd / gOutVolt / gDeadBand, and the classification has to see the
     * inputs the module actually got. */
    cov_observe(name, &a);

    nf();
    capture_new(&b);

    g_cases++;
    if (memcmp(&a, &b, sizeof(IMG)) != 0)
    {
        report(name, &a, &b, -1);
    }
}

/* Same, for the one entry point that returns a value. */
static void run_i(int (*rf)(int), int (*nf)(int), int arg, const char *name)
{
    IMG a;
    IMG b;
    LEDGER l;
    WORLD w;

    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));

    keep_divisors_nonzero(name);

    mod_zero();
    state_apply();
    ledger_save(&l);
    world_save(&w);

    a.ret = (long)rf(arg);
    capture_ref(&a);   /* does not touch `ret`, so the value above survives */

    mod_zero();
    state_apply();
    ledger_restore(&l);
    world_load(&w);

    cov_observe(name, &a);

    b.ret = (long)nf(arg);
    capture_new(&b);

    g_cases++;
    if (memcmp(&a, &b, sizeof(IMG)) != 0)
    {
        report(name, &a, &b, -1);
    }
}

#define SEQ_MAX 8

static IMG g_seqA[SEQ_MAX];
static IMG g_seqB[SEQ_MAX];

/* Sequence form: these functions carry state between calls (VfOverCurDeal's
 * StepLow / StepHigh, VfOverUdcDeal's StepBak / ExeCnt / AccTimes, VfFreqDeal's
 * FreqApply), so the state has to be advanced by the module itself rather than
 * poked.  Both sides get the same number of calls from the same start, and
 * EVERY step is compared -- comparing only the last step would hide a
 * divergence that later re-converges. */
static void run_seq(void (*rf)(void), void (*nf)(void), const char *name, int n)
{
    LEDGER l;
    WORLD w;
    int i;

    if (n > SEQ_MAX)
    {
        n = SEQ_MAX;
    }

    mod_zero();
    state_apply();
    ledger_save(&l);
    world_save(&w);
    for (i = 0; i < n; i++)
    {
        rf();
        capture_ref(&g_seqA[i]);
    }

    mod_zero();
    state_apply();
    ledger_restore(&l);
    world_load(&w);
    cov_observe(name, &g_seqA[n - 1]);
    for (i = 0; i < n; i++)
    {
        nf();
        capture_new(&g_seqB[i]);
    }

    for (i = 0; i < n; i++)
    {
        g_cases++;
        if (memcmp(&g_seqA[i], &g_seqB[i], sizeof(IMG)) != 0)
        {
            report(name, &g_seqA[i], &g_seqB[i], i);
        }
    }
}

/* ---- deterministic pseudo-random ---------------------------------------- */

static unsigned long g_seed = 0x5F3759DFUL;

static unsigned long rnd(void)
{
    g_seed = g_seed * 1103515245UL + 12345UL;
    return (g_seed >> 8) & 0x00FFFFFFUL;
}

static int rnd_range(int lo, int hi)
{
    unsigned long span;

    if (hi <= lo)
    {
        return lo;
    }
    span = (unsigned long)(hi - lo) + 1UL;
    return lo + (int)(rnd() % span);
}

static long rnd_range_l(long lo, long hi)
{
    unsigned long span;

    if (hi <= lo)
    {
        return lo;
    }
    span = (unsigned long)(hi - lo) + 1UL;
    return lo + (long)(rnd() % span);
}

/* ========================================================================== */
/* Coverage table                                                            */
/* ========================================================================== */

/* An X-list so the enum and the name array cannot drift apart: adding a row
 * adds both.  A counter left at zero fails the run even with zero mismatches,
 * because a branch that was never reached cannot be reported on. */
#define COV_LIST(X) \
    X(VI_POS,        "VfVarInitiate FreqDesired > 0") \
    X(VI_ZERO,       "VfVarInitiate FreqDesired == 0") \
    X(VI_NEG,        "VfVarInitiate FreqDesired < 0") \
    X(VU_OVER,       "VFOVUdcLimit over + StepApply<=0") \
    X(VU_OVER_CLAMP, "VFOVUdcLimit time clamp 10000") \
    X(VU_CLEAR,      "VFOVUdcLimit else -> time 0") \
    X(CF_TYPE0,      "CalOutVot VFLineType 0") \
    X(CF_SPDCHK,     "CalOutVot RunStep SPEED_CHECK") \
    X(CF_T1_S1,      "CalOutVot type1 seg1") \
    X(CF_T1_S2,      "CalOutVot type1 seg2") \
    X(CF_T1_S3,      "CalOutVot type1 seg3") \
    X(CF_T1_S4,      "CalOutVot type1 seg4") \
    X(CF_TYPE2,      "CalOutVot VFLineType 2") \
    X(CF_T38,        "CalOutVot VFLineType 3..8") \
    X(CF_PTV2,       "CalOutVot ptV remap -> 2") \
    X(CF_PTV3,       "CalOutVot ptV remap -> 3") \
    X(CF_POSCLAMP,   "CalOutVot posFt clamp 127") \
    X(CF_TYPE10,     "CalOutVot VFLineType 10") \
    X(CF_TYPE11,     "CalOutVot VFLineType 11") \
    X(CF_ZERO,       "CalOutVot |freq| == 0") \
    X(CF_GE,         "CalOutVot |freq| >= FreqPer") \
    X(CF_GE_4096,    "CalOutVot clamp 4096") \
    X(CF_GE_MAXVOLT, "CalOutVot clamp MaxOutVolt") \
    X(CF_LT_NOCLAMP, "CalOutVot |freq| < FreqPer") \
    X(CU_SKIP_LT1,   "CalTorqueUp skip VFLineType 1") \
    X(CU_SKIP_LT10,  "CalTorqueUp skip VFLineType >= 10") \
    X(CU_SKIP_GETPAR,"CalTorqueUp skip GET_PAR") \
    X(CU_ACTIVE,     "CalTorqueUp active") \
    X(CU_DETA_POS,   "CalTorqueUp DetaFreq>0 && !SpeedSearch") \
    X(CU_PID,        "CalTorqueUp PID boost") \
    X(CU_PID_OFF,    "CalTorqueUp PID off (FreqDesiredReal<=40)") \
    X(CU_RAMP,       "CalTorqueUp linear boost") \
    X(WQ_CLAMP_CUR,  "VFWsBoost clamp to VfCurrentIs") \
    X(WQ_NEG_RV,     "VFWsBoost VfRVCosFai < 0 -> 0") \
    X(WQ_ANGLE_CLAMP,"VFWsBoost angle clamp") \
    X(WQ_COFF_HI,    "VFWsBoost Coff > 1400") \
    X(WQ_COFF_LO,    "VFWsBoost Coff <= 1400") \
    X(WQ_FREQSET_NEG,"VFWsBoost FreqSet < 0") \
    X(WQ_CUR_LT4096, "VFWsBoost CurPer < 4096") \
    X(AT_LIM50,      "AutoTqBoost lower limit 50") \
    X(AT_LIM35,      "AutoTqBoost lower limit 35") \
    X(AT_LIM0,       "AutoTqBoost lower limit 0") \
    X(AT_TEMP_ES,    "AutoTqBoost InvType>27 -> EsVolt") \
    X(AT_FR_GT150,   "AutoTqBoost FreqReal > 150") \
    X(AT_FR_LT2000,  "AutoTqBoost FreqReal < 2000") \
    X(AT_R1_LOW,     "AutoTqBoost R1 < 2000*UnitCoff") \
    X(AT_FR_LT950,   "AutoTqBoost FreqReal < 950") \
    X(AT_ENABLE,     "AutoTqBoost enable-time increment") \
    X(AT_DETA_GT19,  "AutoTqBoost Deta >> 5") \
    X(AT_DETA_GT13,  "AutoTqBoost Deta >> 3 & FFFC") \
    X(AT_DETA_ELSE,  "AutoTqBoost Deta >> 1 & FFF0") \
    X(WC_EARLY_FREQ, "WSComp early FreqDesiredReal < 40") \
    X(WC_EARLY_TYPE, "WSComp early InvType > 16") \
    X(WC_ACTIVE,     "WSComp active") \
    X(WC_DELAY_WAIT, "WSComp DelayTime <= 100") \
    X(WC_APPLY,      "WSComp DelayTime > 100") \
    X(WC_FILTER16,   "WSComp Filter16 branch") \
    X(WC_I0,         "WSComp I0 branch") \
    X(WC_ZERO_GUARD, "WSComp WsCurrentIm == 0 repair") \
    X(WC_CLAMP_HI,   "WSComp WsWindage clamp 0x2000") \
    X(WC_CLAMP_LO,   "WSComp WsWindage clamp 0") \
    X(WC_INC_UP,     "WSComp CompFreq rise") \
    X(WC_INC_DOWN,   "WSComp CompFreq fall") \
    X(WC_SNAP,       "WSComp CompFreq snap") \
    X(OM_SKIP_SPDCHK,"OvMag skip SPEED_CHECK") \
    X(OM_SKIP_TYPE,  "OvMag skip VFLineType >= 10") \
    X(OM_DEC,        "OvMag speed_DEC -> uDC") \
    X(OM_SNAP,       "OvMag |UDCFilter-desired| < 10") \
    X(OM_DEC1,       "OvMag UDCFilter--") \
    X(OM_INC3,       "OvMag UDCFilter += 3") \
    X(SC_POS,        "SpeedControl FreqSet > 0") \
    X(SC_NONPOS,     "SpeedControl FreqSet <= 0") \
    X(OC_CHG,        "OverCur SpeedFlag changed") \
    X(OC_NOCHG,      "OverCur SpeedFlag unchanged") \
    X(OC_LOWGAIN,    "OverCur Coff<10 && !speed_CON") \
    X(OC_ADD_GAIN,   "OverCur SubStep 1") \
    X(OC_ADD_ONE,    "OverCur SubStep 2") \
    X(OC_MAX_LOW,    "OverCur MaxStep Low (Coff<40)") \
    X(OC_MAX_HIGH,   "OverCur MaxStep High (Coff>=40)") \
    X(OC_LIM_170,    "OverCur CurLim 170 (InvType 15)") \
    X(OC_LIM_POINT,  "OverCur CurLim ocPoint") \
    X(OC_BELOW_LOW,  "OverCur cur < LimLow") \
    X(OC_MID,        "OverCur LimLow <= cur < LimHigh") \
    X(OC_ABOVE,      "OverCur cur >= LimHigh") \
    X(OC_FLAG0,      "OverCur Flag bit0 clear") \
    X(OC_FREQLOW,    "OverCur |FreqSyn| < LowFreq") \
    X(OC_STEPLIM,    "OverCur StepLowLim > MaxStep") \
    X(OC_DETA_NEG,   "OverCur mid DetaCur < 0") \
    X(OC_HIGH_POS,   "OverCur mid StepHigh >= 0") \
    X(OC_HIGH_NEG,   "OverCur mid StepHigh < 0") \
    X(OC_TOP_DNEG,   "OverCur top DetaCur < 0") \
    X(OC_TOP_DPOS,   "OverCur top DetaCur >= 0") \
    X(UD_R_NOTDEC,   "OverUdc reset !speed_DEC") \
    X(UD_R_CHG,      "OverUdc reset spedChg") \
    X(UD_R_COFF0,    "OverUdc reset CoffApply == 0") \
    X(UD_FREQMAX,    "OverUdc |FreqApply| > |FreqMax|") \
    X(UD_RESTORE1,   "OverUdc restore (first)") \
    X(UD_EXEC_WAIT,  "OverUdc ExeCnt < Coff") \
    X(UD_ADD_SMALL,  "OverUdc Coff < 10 add") \
    X(UD_LOW_DEC,    "OverUdc uDC<=Low DetaU<0") \
    X(UD_LOW_INC,    "OverUdc uDC<=Low DetaU>=0") \
    X(UD_MID_DEC,    "OverUdc mid DetaU<0") \
    X(UD_HIGH_UP,    "OverUdc high DetaU>5") \
    X(UD_HIGH_DOWN,  "OverUdc high DetaU<-5") \
    X(UD_TOP_UP,     "OverUdc top DetaU>3") \
    X(UD_COFFADD_ADJ,"OverUdc CoffApply += 6") \
    X(UD_BAK_HI,     "OverUdc StepBak clamp 20000") \
    X(UD_BAK_LO,     "OverUdc StepBak clamp -20000") \
    X(FD_DEC,        "FreqDeal speed_DEC") \
    X(FD_STEPAPPLY,  "FreqDeal StepApply != 0") \
    X(FD_ACCTIMES,   "FreqDeal AccTimes > 10") \
    X(FD_OCGAIN0,    "FreqDeal ocGain == 0") \
    X(FD_OU_FORCE,   "FreqDeal StepOU forced 32767") \
    X(FD_OI_SMALL,   "FreqDeal StepOI < StepOU") \
    X(FD_OU_SMALL,   "FreqDeal StepOU <= StepOI") \
    X(FD_PASS,       "FreqDeal pass-through 32767") \
    X(FD_STEPSET,    "FreqDeal VfStepSet <= StepOU") \
    X(FD_DIR_NEG,    "FreqDeal direction negative") \
    X(FD_FREQ_ZERO,  "FreqDeal Freq*FreqApply < 0 -> 0") \
    X(FD_DETA_GE,    "FreqDeal Deta >= 0 -> FreqSet") \
    X(FD_DETA_LT,    "FreqDeal Deta < 0 -> Freq") \
    X(OI_SKIP_TYPE,  "OscIndex skip non-ASYNC_VF") \
    X(OI_SKIP_START, "OscIndex skip Start == 0") \
    X(OI_ACCUM,      "OscIndex accumulate") \
    X(OI_WRAP,       "OscIndex window complete") \
    X(DB_LOWFREQ,    "DeadBand FreqReal <= 40000") \
    X(DB_HIFREQ,     "DeadBand FreqReal > 40000") \
    X(DB_DC,         "DeadBand DC start/stop -> 0") \
    X(DB_DEAD_150,   "DeadBand coeff 0 (freq<=150)") \
    X(DB_DEAD_800,   "DeadBand coeff 100 (freq<800)") \
    X(DB_DEAD_INTERP,"DeadBand coeff interpolate") \
    X(DB_SECT_CROSS, "DeadBand sector crossing 5461") \
    X(DB_SECT_NO,    "DeadBand no crossing") \
    X(DB_PH0,        "DeadBand phase near 0") \
    X(DB_PHNEG,      "DeadBand phase < 0") \
    X(DB_PHPOS,      "DeadBand phase > 0") \
    X(OD_SYN_NEG,    "OscDamp FreqSyn < 0") \
    X(OD_SYN_POS,    "OscDamp FreqSyn >= 0") \
    X(CD_RES_CLAMP,  "CurReDecomp ResVolt clamp") \
    X(CD_DIR_NEG,    "CurReDecomp dir -1") \
    X(CD_DIR_POS,    "CurReDecomp dir +1")

enum
{
#define X(id, s) COV_##id,
    COV_LIST(X)
#undef X
    COV_COUNT
};

static const char *const g_covName[COV_COUNT] =
{
#define X(id, s) s,
    COV_LIST(X)
#undef X
};

static unsigned long g_cov[COV_COUNT];

static void hit(int i)
{
    if (i >= 0 && i < COV_COUNT)
    {
        g_cov[i]++;
    }
}

static int iabs(int v)
{
    return (v < 0) ? -v : v;
}

/* Set by the phases that drive CalOutVotInVFStatus, so the classifier can see
 * the argument the module was called with. */
static int g_argFreq;

/* Classify what the ref side just did.  `a` is the ref image, `st` the state the
 * case started from, and the shared inputs are still untouched (world_load has
 * not run yet), so both are exactly what the module saw. */
static void cov_observe(const char *fn, const IMG *a)
{
    if (0 == strcmp(fn, "VfVarInitiate"))
    {
        if (gMainCmd.FreqDesired > 0)      hit(COV_VI_POS);
        else if (gMainCmd.FreqDesired == 0) hit(COV_VI_ZERO);
        else                                hit(COV_VI_NEG);
        return;
    }

    if (0 == strcmp(fn, "VFOVUdcLimit"))
    {
        long mid = ((Ulong)st.ovUdc.Limit * 3932L) >> 12;

        if (((long)gUDC.uDCFilter > mid) && (st.ovUdc.StepApply <= 0))
        {
            hit(COV_VU_OVER);
            if ((long)st.ovUdc.OvUdcLimitTime > 10000)
            {
                hit(COV_VU_OVER_CLAMP);
            }
        }
        else
        {
            hit(COV_VU_CLEAR);
        }
        return;
    }

    if (0 == strcmp(fn, "CalOutVotInVFStatus"))
    {
        int af = iabs(g_argFreq);

        if ((st.vfPar.VFLineType == 0) || (gMainStatus.RunStep == STATUS_SPEED_CHECK))
        {
            if (st.vfPar.VFLineType == 0)                 hit(COV_CF_TYPE0);
            if (gMainStatus.RunStep == STATUS_SPEED_CHECK) hit(COV_CF_SPDCHK);
        }
        else if (st.vfPar.VFLineType == 1)
        {
            long p = (long)gMainCmd.pu2siCoeff;
            long l1 = (long)st.vfPar.VFLineFreq1 * p;
            long l2 = (long)st.vfPar.VFLineFreq2 * p;
            long l3 = (long)st.vfPar.VFLineFreq3 * p;

            if (gMainCmd.FreqReal < l1)      hit(COV_CF_T1_S1);
            else if (gMainCmd.FreqReal < l2) hit(COV_CF_T1_S2);
            else if (gMainCmd.FreqReal < l3) hit(COV_CF_T1_S3);
            else                             hit(COV_CF_T1_S4);
        }
        else if (st.vfPar.VFLineType == 2)
        {
            hit(COV_CF_TYPE2);
        }
        else if ((st.vfPar.VFLineType >= 3) && (st.vfPar.VFLineType <= 8))
        {
            Uint16 ptV = (Uint16)(st.vfPar.VFLineType - 3);

            hit(COV_CF_T38);
            if ((ptV == 3) || (ptV == 5)) hit(COV_CF_PTV2);
            if (ptV == 5)                 hit(COV_CF_PTV3);
            if (ptV == 3)                 hit(COV_CF_PTV2);

            /* posFt = (|f|*4096/FreqPer) >> 5, clamped at 127 */
            {
                Ulong f2 = (Ulong)af * 4096UL / (Ulong)gMotorInfo.FreqPer;
                if ((f2 >> 5) >= 127UL) hit(COV_CF_POSCLAMP);
            }
        }
        else if (st.vfPar.VFLineType == 10)
        {
            hit(COV_CF_TYPE10);
        }
        else if (st.vfPar.VFLineType == 11)
        {
            hit(COV_CF_TYPE11);
        }

        if (af == 0)
        {
            hit(COV_CF_ZERO);
        }
        else if (af >= (int)gMotorInfo.FreqPer)
        {
            hit(COV_CF_GE);
            if (st.vfPar.VFLineType < 10)
            {
                /* The clamp is `(mVolt > 4096) ? 4096 : mVolt`, so the return
                 * value can never exceed 4096 -- the arm has to be classified
                 * on the UNCLAMPED mVolt.  For VFLineType 0 that is
                 * |f|*4096/FreqPer, so it exceeds 4096 exactly when
                 * |f| > FreqPer. */
                if (st.vfPar.VFLineType == 0)
                {
                    long v = (long)((Ulong)af * 4096UL / (Ulong)gMotorInfo.FreqPer);

                    if (v > 4096) hit(COV_CF_GE_4096);
                }
                else if (st.vfPar.VFLineType == 2)
                {
                    Ulong f2 = ((Ulong)af * (Ulong)af) >> 15;
                    Ulong m2 = ((Ulong)gMotorInfo.FreqPer
                                * (Ulong)gMotorInfo.FreqPer) >> 15;

                    if ((m2 != 0UL) && ((long)(f2 * 4096UL / m2) > 4096))
                    {
                        hit(COV_CF_GE_4096);
                    }
                }
            }
            else
            {
                if (a->ret >= gOutVolt.MaxOutVolt) hit(COV_CF_GE_MAXVOLT);
            }
        }
        else
        {
            hit(COV_CF_LT_NOCLAMP);
        }
        return;
    }

    if (0 == strcmp(fn, "CalTorqueUp"))
    {
        if (st.vfPar.VFLineType == 1)            hit(COV_CU_SKIP_LT1);
        else if (st.vfPar.VFLineType >= 10)      hit(COV_CU_SKIP_LT10);
        else if (gMainStatus.RunStep == STATUS_GET_PAR) hit(COV_CU_SKIP_GETPAR);
        else
        {
            /* The module declares `int m_DetaFreq, m_VoltUp, m_ZeroUp,
             * m_MaxFreqUp;` -- all four are 16 bits on C28x and on the host
             * shim.  Computing maxUp in `long` here made the observer disagree
             * with the code it observes: with the default 800/100 the real
             * m_MaxFreqUp is (800<<15)/100 == 262144 truncated to 0, so the
             * module skips the whole boost block, while a `long` observer
             * reports deta == 262144 > 0 and claims the branch was taken.
             * Three mutations (CU7 / CU8 / CU9) came back MISSED because of
             * exactly this, and the coverage gate still read 141/141.
             * Truncate the same way the module does. */
            int maxUp = (int)((((Ulong)st.vfPar.VFTorqueUpLim) << 15)
                              / gBasePar.FullFreq);
            int deta  = (int)(maxUp - iabs(gMainCmd.FreqSyn));

            hit(COV_CU_ACTIVE);
            if ((deta > 0) && (0 == gExtendCmd.bit.SpeedSearch))
            {
                hit(COV_CU_DETA_POS);
                if ((st.vfPar.VFTorqueUp == 0) && (st.vfPar.VFOvShock == 0))
                {
                    if (gMainCmd.FreqDesiredReal > 40) hit(COV_CU_PID);
                    else                               hit(COV_CU_PID_OFF);
                }
                else
                {
                    hit(COV_CU_RAMP);
                }
            }
        }
        return;
    }

    if (0 == strcmp(fn, "VFWsTorqueBoostComm"))
    {
        if (iabs(gIMTQ12.T) > st.vfAutoVar.VfCurrentIs) hit(COV_WQ_CLAMP_CUR);
        if (a->vfAutoVar.VfRVCosFai == 0)               hit(COV_WQ_NEG_RV);
        if (a->vfAutoVar.VfReverseAngle < st.vfAutoVar.VfReverseAngle)
        {
            hit(COV_WQ_ANGLE_CLAMP);
        }
        if (st.vfPar.VFWsComp > 1400) hit(COV_WQ_COFF_HI);
        else                        hit(COV_WQ_COFF_LO);
        if ((st.vfPar.VFWsComp <= 1400) && (gMainCmd.FreqSet < 0)) hit(COV_WQ_FREQSET_NEG);
        if ((st.vfPar.VFWsComp <= 1400) && (gLineCur.CurPer < 4096)) hit(COV_WQ_CUR_LT4096);
        return;
    }

    if (0 == strcmp(fn, "VFAutoTorqueBoost"))
    {
        if (gInvInfo.InvTypeApply <= 15)      hit(COV_AT_LIM50);
        else if (gInvInfo.InvTypeApply <= 27) hit(COV_AT_LIM35);
        else                                  hit(COV_AT_LIM0);

        if (gInvInfo.InvTypeApply > 27) hit(COV_AT_TEMP_ES);
        if (gMainCmd.FreqReal > 150)
        {
            hit(COV_AT_FR_GT150);
            if (gMainCmd.FreqReal < 2000)
            {
                hit(COV_AT_FR_LT2000);
                if ((long)gMotorExtInfo.R1 < 2000L * (long)gMotorExtInfo.UnitCoff)
                {
                    hit(COV_AT_R1_LOW);
                    if (gMainCmd.FreqReal < 950) hit(COV_AT_FR_LT950);
                }
            }
        }
        if (((long)gMotorExtInfo.R1 < 2000L * (long)gMotorExtInfo.UnitCoff)
            && (350 > st.vfAutoVar.VfTorqueEnableTime))
        {
            hit(COV_AT_ENABLE);
        }
        if (gInvInfo.InvTypeApply > 19)      hit(COV_AT_DETA_GT19);
        else if (gInvInfo.InvTypeApply > 13) hit(COV_AT_DETA_GT13);
        else                                 hit(COV_AT_DETA_ELSE);
        return;
    }

    if (0 == strcmp(fn, "VFWSCompControl"))
    {
        if (gMainCmd.FreqDesiredReal < 40)  hit(COV_WC_EARLY_FREQ);
        else if (16 < (int)gInvInfo.InvTypeApply) hit(COV_WC_EARLY_TYPE);
        else if (speed_CON && (0 != st.vfPar.VFWsComp))
        {
            hit(COV_WC_ACTIVE);
            if (st.wsComp.DelayTime > 100)
            {
                hit(COV_WC_APPLY);
                if ((0 == st.vfPar.VFOvShock) && (20 <= gBasePar.FcSetApply))
                {
                    hit(COV_WC_FILTER16);
                    if (0 == iabs(st.wsComp.WsCompMTApply.M)) hit(COV_WC_ZERO_GUARD);
                }
                else
                {
                    hit(COV_WC_I0);
                    if (0 == gMotorExtPer.I0) hit(COV_WC_ZERO_GUARD);
                }
            }
            else
            {
                hit(COV_WC_DELAY_WAIT);
            }
        }

        /* The three arms of the CompFreq slew are judged on m_WsWindage, which
         * is a LOCAL of the module.  It is recomputed here from the inputs,
         * mirroring the source exactly -- including the `int` truncation at
         * every assignment, which is 16-bit on the host too (the shim folds
         * `int`).  This is a classifier, not an oracle: if it disagrees with
         * the module the counter is misattributed, the comparison is not
         * affected.  The dedicated slew sweep in phase_wscomp() is what makes
         * all three counters non-zero, and a misattribution there would show up
         * as one counter stuck at zero. */
        if ((!(gMainCmd.FreqDesiredReal < 40))
            && ((int)gInvInfo.InvTypeApply <= 16)
            && speed_CON && (0 != st.vfPar.VFWsComp)
            && (st.wsComp.DelayTime > 100))
        {
            int wind = 0;
            int curIm;

            if ((0 == st.vfPar.VFOvShock) && (20 <= gBasePar.FcSetApply))
            {
                Ulong f = (((Ulong)st.wsComp.WsCompMTApply.M) << 16)
                        + (((Ulong)st.wsComp.WsCompMT.M) << 12)
                        - (((Ulong)st.wsComp.WsCompMTApply.M) << 12);
                int v = (int)(f >> 16);

                curIm = (v < 0) ? -v : v;
            }
            else
            {
                curIm = (int)gMotorExtPer.I0;
            }
            if (0 == curIm)
            {
                curIm = 1;
            }
            wind = (int)((3038L * (long)st.wsComp.WsCompMTApply.T) / (long)curIm);
            wind = (int)(((long)wind * (long)st.wsComp.Coff) / 1000L);
            if (wind > 0x2000) hit(COV_WC_CLAMP_HI);
            if (wind < 0)      hit(COV_WC_CLAMP_LO);
            if (wind > 0x2000) wind = 0x2000;
            if (wind < 0)      wind = 0;
            wind = (int)(((long)wind * (long)gMotorExtPer.RatedComp) >> 12);

            if (((st.wsComp.CompFreq >> 16) + 2) < wind)
            {
                hit(COV_WC_INC_UP);
            }
            else if (((st.wsComp.CompFreq >> 16) - 2) > wind)
            {
                hit(COV_WC_INC_DOWN);
            }
            else
            {
                hit(COV_WC_SNAP);
            }
        }
        return;
    }

    if (0 == strcmp(fn, "VFOverMagneticControl"))
    {
        if (gMainStatus.RunStep == STATUS_SPEED_CHECK) hit(COV_OM_SKIP_SPDCHK);
        else if (st.vfPar.VFLineType >= 10)              hit(COV_OM_SKIP_TYPE);
        else
        {
            long des = (long)gInvInfo.BaseUdc;
            long cur = st.varAvr.UDCFilter;
            long dif;

            if (speed_DEC)
            {
                hit(COV_OM_DEC);
                des = (long)gUDC.uDC;
            }
            dif = cur - des;
            if (dif < 0) dif = -dif;
            if (dif < 10)            hit(COV_OM_SNAP);
            else if (cur > des)      hit(COV_OM_DEC1);
            else                     hit(COV_OM_INC3);
        }
        return;
    }

    if (0 == strcmp(fn, "VFSpeedControl"))
    {
        if (gMainCmd.FreqSet > 0) hit(COV_SC_POS);
        else                      hit(COV_SC_NONPOS);
        return;
    }

    if (0 == strcmp(fn, "VfOverCurDeal"))
    {
        int coff = (int)st.vfPar.ocGain;

        if (st.vfFreq2.preSpdFlag != gMainCmd.Command.bit.SpeedFlag) hit(COV_OC_CHG);
        else                                                         hit(COV_OC_NOCHG);

        if ((coff < 10) && (!speed_CON)) hit(COV_OC_LOWGAIN);
        if (coff < 20)                   hit(COV_OC_ADD_GAIN);
        else                             hit(COV_OC_ADD_ONE);
        if (coff < 40)                   hit(COV_OC_MAX_LOW);
        else                             hit(COV_OC_MAX_HIGH);
        if (15 == gInvInfo.InvTypeApply) hit(COV_OC_LIM_170);
        else                             hit(COV_OC_LIM_POINT);

        {
            long lim   = (15 == gInvInfo.InvTypeApply)
                         ? (((long)170L << 12) / 100)
                         : (((long)st.vfPar.ocPoint << 12) / 100);
            long low   = (lim * 3605L) >> 12;
            long high  = (lim * 3891L) >> 12;
            int  cur   = (int)gLineCur.CurBaseInv;
            int  addStep = (coff < 20) ? (20 - coff) : 1;
            long maxStep;

            if (cur < low)
            {
                hit(COV_OC_BELOW_LOW);
                if ((st.ovCur.Flag & 0x01) == 0) hit(COV_OC_FLAG0);
                if (iabs(gMainCmd.FreqSyn) < st.ovCur.LowFreq) hit(COV_OC_FREQLOW);
                maxStep = (iabs(gMainCmd.FreqSyn) < st.ovCur.LowFreq)
                          ? st.ovCur.MaxStepLow : st.ovCur.MaxStepHigh;
                if ((st.ovCur.StepLowLim + addStep) > maxStep) hit(COV_OC_STEPLIM);
            }
            else if (cur < high)
            {
                hit(COV_OC_MID);
                if ((cur - st.ovCur.CurBak) < 0) hit(COV_OC_DETA_NEG);
                if (st.ovCur.StepHigh >= 0)      hit(COV_OC_HIGH_POS);
                else                             hit(COV_OC_HIGH_NEG);
            }
            else
            {
                hit(COV_OC_ABOVE);
                if ((cur - st.ovCur.CurBak) < 0) hit(COV_OC_TOP_DNEG);
                else                             hit(COV_OC_TOP_DPOS);
            }
        }
        return;
    }

    if (0 == strcmp(fn, "VfOverUdcDeal"))
    {
        int coff = st.ovUdc.CoffApply;

        if (!speed_DEC)               hit(COV_UD_R_NOTDEC);
        else if (st.vfFreq2.spedChg)  hit(COV_UD_R_CHG);
        else if (coff == 0)           hit(COV_UD_R_COFF0);
        else if (iabs(st.vfPar.FreqApply) > iabs(st.ovUdc.FreqMax))
        {
            hit(COV_UD_FREQMAX);
        }
        else
        {
            long mid = ((Ulong)st.ovUdc.Limit * 3932L) >> 12;
            long low = 6100;
            long detaU;

            if (((long)gUDC.uDC <= mid) && ((st.ovUdc.Flag & 0x01) == 0x01))
            {
                hit(COV_UD_RESTORE1);
            }
            else if ((st.ovUdc.ExeCnt + 6) < coff)
            {
                hit(COV_UD_EXEC_WAIT);
            }
            else
            {
                /* NOTE: the module has a SECOND `if((m_Udc <= m_Mid) &&
                 * ((Flag & 0x01) == 0x01))` right after the ExeCnt divider
                 * (source line 692).  It is a verbatim duplicate of the one
                 * above, and the only things written between them are
                 * gOvUdc.ExeCnt, gOvUdc.StepApply, the local m_DetaU and
                 * gOvUdc.UdcBak -- none of which appear in the condition
                 * (m_Udc, m_Mid, Flag).  So it can never be taken and there is
                 * no counter for it; see README section 6.  Dead code, not a
                 * coverage gap. */
                if (coff < 10) hit(COV_UD_ADD_SMALL);

                detaU = (long)gUDC.uDC - st.ovUdc.UdcBak;
                if ((long)gUDC.uDC <= low)
                {
                    if (detaU < 0) hit(COV_UD_LOW_DEC);
                    else           hit(COV_UD_LOW_INC);
                }
                else if ((long)gUDC.uDC <= mid)
                {
                    if (detaU < 0) hit(COV_UD_MID_DEC);
                }
                else if ((long)gUDC.uDC <= st.ovUdc.Limit)
                {
                    if (detaU > 5)       hit(COV_UD_HIGH_UP);
                    else if (detaU < -5) hit(COV_UD_HIGH_DOWN);
                }
                else
                {
                    if (detaU > 3) hit(COV_UD_TOP_UP);
                }

                if (st.ovUdc.StepBak > 20000)  hit(COV_UD_BAK_HI);
                if (st.ovUdc.StepBak < -20000) hit(COV_UD_BAK_LO);

                if (((st.ovUdc.Flag & 0x02) == 0) && (st.ovUdc.CoffAdd != 0)
                    && (st.vfPar.ovGain >= 10))
                {
                    hit(COV_UD_COFFADD_ADJ);
                }
            }
        }
        return;
    }

    if (0 == strcmp(fn, "VfFreqDeal"))
    {
        int  stepOI = st.ovCur.StepApply;
        int  stepOU = st.ovUdc.StepApply;
        int  force  = 0;
        int  step;
        int  freq;
        int  dir;

        if (speed_DEC)
        {
            hit(COV_FD_DEC);
            if (stepOI < 1) stepOI = 1;
            if (0 != st.ovUdc.StepApply) hit(COV_FD_STEPAPPLY);
            if (st.ovUdc.AccTimes > 10) hit(COV_FD_ACCTIMES);
        }
        if (st.vfPar.ocGain == 0)
        {
            stepOI = 32767;
            hit(COV_FD_OCGAIN0);
        }

        /* OvUdcLimitTime is read AFTER VFOVUdcLimit ran, so the image -- not
         * the starting state -- is the value the condition saw. */
        if ((st.vfPar.ovGain == 0) || (a->ovUdc.OvUdcLimitTime > 5000)
            || (!speed_DEC)
            || (iabs(st.vfPar.FreqApply) - iabs(gMainCmd.FreqDesired) < 37))
        {
            hit(COV_FD_OU_FORCE);
            stepOU = 32767;
            force  = 1;
        }

        if (stepOI < stepOU) hit(COV_FD_OI_SMALL);
        else                 hit(COV_FD_OU_SMALL);

        step = (stepOI < stepOU) ? stepOI : stepOU;
        if (step == 32767) hit(COV_FD_PASS);

        if (speed_DEC)
        {
            int vfStepSet = iabs(iabs(gMainCmd.FreqSet) - iabs(st.vfPar.FreqApply));

            if (vfStepSet <= stepOU) hit(COV_FD_STEPSET);
        }

        dir = speed_DEC ? st.vfPar.FreqApply : gMainCmd.FreqSet;
        if (dir < 0) hit(COV_FD_DIR_NEG);

        /* the zero-crossing clamp only exists on the non-pass path */
        if (step != 32767)
        {
            if (speed_DEC) step = -step;
            freq = iabs(st.vfPar.FreqApply) + step;
            if (dir < 0) freq = -freq;
            if (((long)freq * (long)st.vfPar.FreqApply) < 0) hit(COV_FD_FREQ_ZERO);
        }
        (void)force;

        /* the final arm: m_Deta >= 0 assigns FreqSet, else m_Freq */
        if (a->vfPar.FreqApply == gMainCmd.FreqSet) hit(COV_FD_DETA_GE);
        else                                        hit(COV_FD_DETA_LT);
        return;
    }

    if (0 == strcmp(fn, "VfOscIndexCalc"))
    {
        if (gCtrMotorType != ASYNC_VF)          hit(COV_OI_SKIP_TYPE);
        else if (gMainCmd.Command.bit.Start == 0) hit(COV_OI_SKIP_START);
        else if (st.hvFoscIndex.wCntUse < st.hvFoscIndex.wCntRltm) hit(COV_OI_ACCUM);
        else                                                        hit(COV_OI_WRAP);
        return;
    }

    if (0 == strcmp(fn, "HVfDeadBandComp"))
    {
        if (gMainCmd.FreqReal <= 40000) hit(COV_DB_LOWFREQ);
        else                            hit(COV_DB_HIFREQ);
        if ((gMainCmd.Command.bit.StartDC == 1)
            || (gMainCmd.Command.bit.StopDC == 1)) hit(COV_DB_DC);

        if ((gMainCmd.FreqReal <= 150)
            || (gMainCmd.FreqReal >= st.hvFdeadBandCompOpt.DbOptActHFreq))
        {
            hit(COV_DB_DEAD_150);
        }
        else if (gMainCmd.FreqReal < 800)  hit(COV_DB_DEAD_800);
        else if (gMainCmd.FreqReal < 1200) hit(COV_DB_DEAD_INTERP);

        /* sector crossing: the module compares phase_sect_pre and phase_sect
         * against 5461, both derived from the 16-bit CurPhaseFeed */
        {
            long f  = (long)st.hvFdeadBandCompOpt.CurPhaseFeed;
            long fp = (long)st.hvFdeadBandCompOpt.CurPhaseFeed_pre;
            long sp, spp;

            if (f >= 0)  sp  = f  % 10922; else sp  = f;
            if (fp >= 0) spp = fp % 10922; else spp = fp;
            if (((spp <= 5461) && (sp >= 5461)) || ((spp >= 5461) && (sp <= 5461)))
            {
                hit(COV_DB_SECT_CROSS);
            }
            else
            {
                hit(COV_DB_SECT_NO);
            }
        }

        {
            int phase = st.hvFdeadBandCompOpt.CurPhasePredict;
            int w     = st.hvFdeadBandCompOpt.DbCompCpwmWidth;

            if (iabs(phase) < w)      hit(COV_DB_PH0);
            else if (phase < 0)       hit(COV_DB_PHNEG);
            else if (phase > 0)       hit(COV_DB_PHPOS);
        }
        return;
    }

    if (0 == strcmp(fn, "HVfOscDampDeal"))
    {
        if (gMainCmd.FreqSyn < 0) hit(COV_OD_SYN_NEG);
        else                      hit(COV_OD_SYN_POS);
        return;
    }

    if (0 == strcmp(fn, "HVfCurReDecomp"))
    {
        long res = (long)((Ulong)gLineCur.CurPer * gMotorExtPer.R1 >> 16);

        if (res >= (long)gOutVolt.VoltApply) hit(COV_CD_RES_CLAMP);
        if (gMainCmd.FreqSyn >= 0) hit(COV_CD_DIR_POS);
        else                       hit(COV_CD_DIR_NEG);
        return;
    }
}

/* ========================================================================== */
/* Phase 1: VfVarInitiate                                                     */
/* ========================================================================== */

static void phase_varinit(void)
{
    static const int fd[] = {-1000, -1, 0, 1, 1000};
    int i;

    for (i = 0; i < 5; i++)
    {
        baseline();
        gMainCmd.FreqDesired = fd[i];
        gMainCmd.Command.bit.SpeedFlag = C_SPEED_FLAG_DEC;
        gPhase.IMPhase = 0x12340000L;
        gIMTQ12.M = -777;
        st.vfPar.VFWsComp = 555;
        run(ref_VfVarInitiate, new_VfVarInitiate, "VfVarInitiate");
    }

    /* IMPhase >> 16 is an int: exercise the sign and the 16-bit wrap */
    {
        static const long im[] = {0L, 0x00010000L, 0x7FFF0000L, 0x80000000L, -1L};
        for (i = 0; i < 5; i++)
        {
            baseline();
            gMainCmd.FreqDesired = 1;
            gPhase.IMPhase = im[i];
            gMainCmd.Command.bit.SpeedFlag = C_SPEED_FLAG_CON;
            run(ref_VfVarInitiate, new_VfVarInitiate, "VfVarInitiate");
        }
    }
}

/* ========================================================================== */
/* Phase 2: VFOVUdcLimit                                                      */
/* ========================================================================== */

static void phase_ovudclimit(void)
{
    static const int limits[] = {0, 1000, 4000, 16000};
    static const int steps[]  = {-5, 0, 1};
    static const int times[]  = {0, 9999, 10000, 10001, 30000};
    int li, si, ti, ui;
    long mid;

    for (li = 0; li < 4; li++)
    {
        mid = ((Ulong)limits[li] * 3932L) >> 12;
        for (ui = 0; ui < 5; ui++)
        {
            static const long dlt[] = {-1L, 0L, 1L, 65535L};
            long u = mid + dlt[ui];

            if (u < 0) u = 0;
            if (u > 65535L) u = 65535L;
            for (si = 0; si < 3; si++)
            {
                for (ti = 0; ti < 5; ti++)
                {
                    baseline();
                    st.ovUdc.Limit = limits[li];
                    st.ovUdc.StepApply = steps[si];
                    st.ovUdc.OvUdcLimitTime = times[ti];
                    gUDC.uDCFilter = (Uint)u;
                    run(ref_VFOVUdcLimit, new_VFOVUdcLimit, "VFOVUdcLimit");
                }
            }
        }
    }
}

/* ========================================================================== */
/* Phase 3: CalOutVotInVFStatus                                               */
/* ========================================================================== */

static void phase_calout(void)
{
    static const int types[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    static const int fr[]    = {0, 1, -1, 100, -100, 250, 499, 500, -500,
                                501, 1000, 32767, -32767, -32768};
    static const int steps[] = {STATUS_RUN, STATUS_SPEED_CHECK, STATUS_GET_PAR};
    int t, f, s;

    for (t = 0; t < 13; t++)
    {
        for (f = 0; f < 14; f++)
        {
            for (s = 0; s < 3; s++)
            {
                baseline();
                st.vfPar.VFLineType = (Uint)types[t];
                gMainStatus.RunStep = steps[s];
                gMainCmd.FreqReal = 1000;
                g_argFreq = fr[f];
                run_i(ref_CalOutVotInVFStatus, new_CalOutVotInVFStatus,
                      fr[f], "CalOutVotInVFStatus");
            }
        }
    }

    /* VFLineType 1: the four segments.  The staircase is 200/400/600/800
     * (see state_default) and FreqReal is compared against break*p2s. */
    {
        static const long realv[] = {-1000L, 0L, 199L, 200L, 399L, 400L,
                                     599L, 600L, 799L, 800L, 5000L};
        static const int  p2s[]   = {1, 2, 4};
        int r, p, f;

        for (p = 0; p < 3; p++)
        {
            for (r = 0; r < 11; r++)
            {
                for (f = 0; f < 3; f++)
                {
                    baseline();
                    st.vfPar.VFLineType = 1;
                    gMainCmd.pu2siCoeff = (Uint)p2s[p];
                    gMainCmd.FreqReal = realv[r];
                    st.vfPar.VFTorqueUp = 50;
                    g_argFreq = (f == 0) ? 0 : ((f == 1) ? 250 : -500);
                    run_i(ref_CalOutVotInVFStatus, new_CalOutVotInVFStatus,
                          g_argFreq, "CalOutVotInVFStatus");
                }
            }
        }
    }

    /* VFLineType 1 break point, hit exactly: FreqReal == break*p2s.
     * The two neighbouring segments agree there iff
     *   ((V1-V0)*4096)/1000 + (V0*4096)/1000 == (V1*4096)/1000
     * because each segment scales its volt step separately and truncates.
     * With the default 100/300 staircase it holds (409 + 819 == 1228), so the
     * `<` / `<=` variant at segment 1 is invisible; 100/110 breaks it
     * (409 + 40 == 449 != 450).  The staircase is data, not code -- this is a
     * bench hole, not an equivalent mutation. */
    {
        static const int  v0[]  = {100, 100, 5};
        static const int  v1[]  = {110, 105, 15};
        static const long bp[]  = {200L, 400L, 600L};
        static const int  p2s[] = {1, 2};
        int i, j, k;

        for (i = 0; i < 3; i++)
        {
            for (j = 0; j < 3; j++)
            {
                for (k = 0; k < 2; k++)
                {
                    baseline();
                    st.vfPar.VFLineType = 1;
                    st.vfPar.VFTorqueUp = (Uint)v0[i];
                    st.vfPar.VFLineVolt1 = (Uint)v1[i];
                    gMainCmd.pu2siCoeff = (Uint)p2s[k];
                    gMainCmd.FreqReal = bp[j] * (long)p2s[k];
                    g_argFreq = (int)gMainCmd.FreqReal;
                    run_i(ref_CalOutVotInVFStatus, new_CalOutVotInVFStatus,
                          g_argFreq, "CalOutVotInVFStatus");
                }
            }
        }
    }

    /* VFLineType 10/11 clamp against MaxOutVolt, and Votage as the divisor */
    {
        static const int maxv[]  = {0, 100, 4000, 32767};
        static const int split[] = {0, 1000, 4000, 20000};
        static const int volt[]  = {1, 380, 690};
        int m, sp, v;

        for (m = 0; m < 4; m++)
        {
            for (sp = 0; sp < 4; sp++)
            {
                for (v = 0; v < 3; v++)
                {
                    baseline();
                    st.vfPar.VFLineType = 10;
                    gOutVolt.MaxOutVolt = maxv[m];
                    gOutVolt.vfSplit = split[sp];
                    gMotorInfo.Votage = (Uint)volt[v];
                    g_argFreq = 500;
                    run_i(ref_CalOutVotInVFStatus, new_CalOutVotInVFStatus,
                          500, "CalOutVotInVFStatus");
                }
            }
        }
    }

    /* posFt clamp: |f|*4096/FreqPer >= 127*32 = 4064 */
    {
        static const int af[]  = {0, 4063, 4064, 4095, 4096};
        static const int fp[]  = {200, 500, 1000};
        int i, j;

        for (i = 0; i < 5; i++)
        {
            for (j = 0; j < 3; j++)
            {
                baseline();
                st.vfPar.VFLineType = 5;
                gMotorInfo.FreqPer = (Uint)fp[j];
                g_argFreq = af[i];
                run_i(ref_CalOutVotInVFStatus, new_CalOutVotInVFStatus,
                      af[i], "CalOutVotInVFStatus");
            }
        }
    }
}

/* ========================================================================== */
/* Phase 4: CalTorqueUp                                                       */
/* ========================================================================== */

static void phase_torqueup(void)
{
    static const int types[] = {0, 1, 9, 10, 11};
    static const int steps[] = {STATUS_RUN, STATUS_GET_PAR};
    static const int tup[]   = {0, 1, 100};
    static const int ovsh[]  = {0, 1};
    static const int fdr[]   = {-100, 0, 40, 41, 1000};
    static const int syn[]   = {0, 100, -100, 5000};
    static const int ssrch[] = {0, 1};
    int a, b, c, d, e, f, g;

    for (a = 0; a < 5; a++)
    {
        for (b = 0; b < 2; b++)
        {
            for (c = 0; c < 3; c++)
            {
                for (d = 0; d < 2; d++)
                {
                    for (e = 0; e < 5; e++)
                    {
                        for (f = 0; f < 4; f++)
                        {
                            for (g = 0; g < 2; g++)
                            {
                                baseline();
                                st.vfPar.VFLineType = (Uint)types[a];
                                gMainStatus.RunStep = steps[b];
                                st.vfPar.VFTorqueUp = (Uint)tup[c];
                                st.vfPar.VFOvShock = (Uint)ovsh[d];
                                gMainCmd.FreqDesiredReal = fdr[e];
                                gMainCmd.FreqSyn = syn[f];
                                gExtendCmd.bit.SpeedSearch = (Uint)ssrch[g];
                                gOutVolt.Volt = 3000;
                                st.vfAutoVar.AutoTorquePID.Out = 500L << 16;
                                run(ref_CalTorqueUp, new_CalTorqueUp, "CalTorqueUp");
                            }
                        }
                    }
                }
            }
        }
    }

    /* VFTorqueUpLim / FullFreq: m_MaxFreqUp straddling |FreqSyn|, and the
     * boost step saturation at MaxFreqUp == 0 (impossible: DetaFreq would be
     * <= 0, which is exactly the branch boundary -- covered above). */
    {
        static const int lim[]  = {0, 1, 100, 800, 32767};
        static const int full[] = {1, 2, 100, 1000};
        int i, j;

        for (i = 0; i < 5; i++)
        {
            for (j = 0; j < 4; j++)
            {
                baseline();
                st.vfPar.VFLineType = 0;
                st.vfPar.VFTorqueUp = 50;
                st.vfPar.VFTorqueUpLim = (Uint)lim[i];
                gBasePar.FullFreq = (Uint)full[j];
                gMainCmd.FreqSyn = 10;
                run(ref_CalTorqueUp, new_CalTorqueUp, "CalTorqueUp");
            }
        }
    }

    /* C: the PID arm.  m_MaxFreqUp is a 16-bit int, so
     * (VFTorqueUpLim<<15)/FullFreq is TRUNCATED -- the default pair 800/100
     * gives 262144 -> 0 and m_DetaFreq <= 0 skips the whole boost block, so
     * the first 1200 cases never reach the PID arm at all.  These pairs keep
     * the truncated value in 3276..26214, which is above |FreqSyn| = 0 and so
     * does reach it.  FreqDesiredReal 40/41 straddles the PID threshold, and
     * Out is swept because `>>16` and `>>17` only differ when it is non-zero.
     * gOutVolt.Volt (3000) differs from VoltApply (0) so the "wrong field"
     * variant is visible too. */
    {
        static const int  lim2[]  = {100, 200, 400};
        static const int  full2[] = {1000, 500};
        static const int  fdr2[]  = {-100, 0, 40, 41, 1000};
        static const long out2[]  = {0L, 500L << 16, -500L << 16};
        int i, j, k, l;

        for (i = 0; i < 3; i++)
        {
            for (j = 0; j < 2; j++)
            {
                for (k = 0; k < 5; k++)
                {
                    for (l = 0; l < 3; l++)
                    {
                        baseline();
                        st.vfPar.VFLineType = 0;
                        st.vfPar.VFTorqueUp = 0;
                        st.vfPar.VFOvShock = 0;
                        st.vfPar.VFTorqueUpLim = (Uint)lim2[i];
                        gBasePar.FullFreq = (Uint)full2[j];
                        gMainCmd.FreqSyn = 0;
                        gExtendCmd.bit.SpeedSearch = 0;
                        gMainCmd.FreqDesiredReal = fdr2[k];
                        gOutVolt.Volt = 3000;
                        gOutVolt.VoltApply = 0;
                        st.vfAutoVar.AutoTorquePID.Out = out2[l];
                        st.vfAutoVar.DestinationVolt = 3000;
                        st.vfAutoVar.VfTorqueEnableTime = 5;
                        run(ref_CalTorqueUp, new_CalTorqueUp, "CalTorqueUp");
                    }
                }
            }
        }
    }
}

/* ========================================================================== */
/* Phase 5: VFWsTorqueBoostComm                                               */
/* ========================================================================== */

static void phase_wsboost(void)
{
    static const int wscomp[] = {0, 1000, 1400, 1401, 2000, 3000};
    static const int fs[]     = {-1000, -1, 0, 1, 1000};
    static const int curper[] = {0, 1, 1000, 4095, 4096, 5000, 65535};
    static const int im[]     = {0, 1, -1, 100, -100, 30000, -30000};
    int a, b, c, d, e;

    for (a = 0; a < 6; a++)
    {
        for (b = 0; b < 5; b++)
        {
            for (c = 0; c < 7; c++)
            {
                for (d = 0; d < 7; d++)
                {
                    baseline();
                    st.vfPar.VFWsComp = (Uint)wscomp[a];
                    gMainCmd.FreqSet = fs[b];
                    gLineCur.CurPer = (Uint)curper[c];
                    gIMTQ12.M = im[d];
                    gIMTQ12.T = im[(d + 3) % 7];
                    st.vfAutoVar.VfCurrentIs = 1000;
                    gPhase.IMPhase = 0x01000000L;
                    gOutVolt.VoltApply = 3000;
                    gMotorExtPer.R1 = 300;
                    gIAlphBeta.Alph = 1000;
                    gIAlphBeta.Beta = 500;
                    run(ref_VFWsTorqueBoostComm, new_VFWsTorqueBoostComm,
                        "VFWsTorqueBoostComm");
                }
            }
        }
    }

    /* the VfCurrentIs filter and the VfRVCosFai clamp */
    for (e = 0; e < 5; e++)
    {
        static const int is0[] = {0, 1, 100, 1000, 30000};
        baseline();
        st.vfPar.VFWsComp = 2000;
        gLineCur.CurPer = 1000;
        st.vfAutoVar.VfCurrentIs = is0[e];
        gIMTQ12.M = 300;
        gIMTQ12.T = 300;
        run(ref_VFWsTorqueBoostComm, new_VFWsTorqueBoostComm,
            "VFWsTorqueBoostComm");
    }

    /* VfRVCosFai < 0 needs VoltApply - 20 - m_RVcos < 0, i.e. a large R1 and a
     * large VfCurrentIs with a small angle */
    {
        static const int r1[]  = {0, 300, 30000, 65535};
        static const int va[]  = {0, 10, 20, 21, 3000};
        int i, j;

        for (i = 0; i < 4; i++)
        {
            for (j = 0; j < 5; j++)
            {
                baseline();
                st.vfPar.VFWsComp = 0;
                st.vfAutoVar.VfCurrentIs = 30000;
                gLineCur.CurPer = 30000;
                gIMTQ12.M = 1;
                gIMTQ12.T = 1;
                gMotorExtPer.R1 = (Uint)r1[i];
                gOutVolt.VoltApply = va[j];
                run(ref_VFWsTorqueBoostComm, new_VFWsTorqueBoostComm,
                    "VFWsTorqueBoostComm");
            }
        }
    }
}

/* ========================================================================== */
/* Phase 6: VFAutoTorqueBoost                                                 */
/* ========================================================================== */

static void phase_autotq(void)
{
    static const int invtype[] = {0, 13, 14, 15, 19, 20, 27, 28, 30};
    static const int freal[]   = {0, 150, 151, 949, 950, 1999, 2000, 4000};
    static const int unit[]    = {0, 1, 100};
    /* 2000 is the R1 threshold itself (`R1 < 2000 * UnitCoff`); 3392 and 3492
     * are what 2000*100 and 2001*100 truncate to once `int` is 16 bits, which
     * is the only way the UnitCoff == 100 arm can separate the two. */
    static const int r1[]      = {0, 100, 199, 200, 300, 2000, 3392, 3492,
                                  30000};
    static const int entime[]  = {0, 349, 350, 351};
    int a, b, c, d, e;

    for (a = 0; a < 9; a++)
    {
        for (b = 0; b < 8; b++)
        {
            for (c = 0; c < 3; c++)
            {
                for (d = 0; d < 9; d++)
                {
                    for (e = 0; e < 4; e++)
                    {
                        baseline();
                        gInvInfo.InvTypeApply = (Uint)invtype[a];
                        gMainCmd.FreqReal = freal[b];
                        gMotorExtInfo.UnitCoff = (Uint)unit[c];
                        gMotorExtInfo.R1 = (Uint)r1[d];
                        st.vfAutoVar.VfTorqueEnableTime = entime[e];
                        st.vfAutoVar.DestinationVolt = 3000;
                        st.vfAutoVar.VfRIsSinFai = 100;
                        st.vfAutoVar.VfRVCosFai = 3000;
                        st.vfAutoVar.VfAngleSin = 16000;
                        st.vfAutoVar.VfReverseVolt = 100;
                        st.vfAutoVar.AutoTorquePID.Total = 0;
                        gOutVolt.VoltApply = 3000;
                        run(ref_VFAutoTorqueBoost, new_VFAutoTorqueBoost,
                            "VFAutoTorqueBoost");
                    }
                }
            }
        }
    }

    /* the __IQsat around DestinationVolt - m_TempVolt, and the Deta forms */
    {
        static const int dst[] = {0, 1000, 3000, 32000};
        static const int rv[]  = {0, 1000, 32767, -32767};
        int i, j;

        for (i = 0; i < 4; i++)
        {
            for (j = 0; j < 4; j++)
            {
                baseline();
                gInvInfo.InvTypeApply = 20;
                gMainCmd.FreqReal = 4000;
                st.vfAutoVar.DestinationVolt = dst[i];
                st.vfAutoVar.VfReverseVolt = rv[j];
                run(ref_VFAutoTorqueBoost, new_VFAutoTorqueBoost,
                    "VFAutoTorqueBoost");
            }
        }
    }
}

/* ========================================================================== */
/* Phase 7: VFWSCompControl                                                   */
/* ========================================================================== */

static void phase_wscomp(void)
{
    static const int fdr[]    = {0, 39, 40, 1000};
    static const int invtype[]= {0, 16, 17};
    static const int spd[]    = {C_SPEED_FLAG_CON, C_SPEED_FLAG_ACC,
                                 C_SPEED_FLAG_DEC};
    static const int wscomp[] = {0, 1000};
    static const int delay[]  = {0, 100, 101, 200};
    static const int ovsh[]   = {0, 1};
    static const int fcset[]  = {0, 19, 20, 100};
    static const int i0[]     = {0, 1, 300};
    static const int applyM[] = {0, 1, -1, 1000, -1000, 32767};
    int a, b, c, d, e, f, g, h, i;

    /* A: the two early returns, against the gate that guards them */
    for (a = 0; a < 4; a++)
    {
        for (b = 0; b < 3; b++)
        {
            baseline();
            gMainCmd.FreqDesiredReal = fdr[a];
            gInvInfo.InvTypeApply = (Uint)invtype[b];
            gMainCmd.Command.bit.SpeedFlag = C_SPEED_FLAG_CON;
            st.vfPar.VFWsComp = 1000;
            st.wsComp.Coff = 1000;
            st.wsComp.DelayTime = 200;
            st.wsComp.WsCompMTApply.M = 1000;
            st.wsComp.WsCompMTApply.T = 1000;
            gMainCmd.FreqSetApply = 123;
            run(ref_VFWSCompControl, new_VFWSCompControl, "VFWSCompControl");
        }
    }

    /* B: the `speed_CON && VFWsComp != 0` gate */
    for (c = 0; c < 3; c++)
    {
        for (d = 0; d < 2; d++)
        {
            baseline();
            gMainCmd.FreqDesiredReal = 1000;
            gInvInfo.InvTypeApply = 0;
            gMainCmd.Command.bit.SpeedFlag = (Uint)spd[c];
            st.vfPar.VFWsComp = (Uint)wscomp[d];
            st.wsComp.Coff = (Uint)wscomp[d];
            st.wsComp.DelayTime = 200;
            st.wsComp.WsCompMTApply.M = 1000;
            st.wsComp.WsCompMTApply.T = 1000;
            run(ref_VFWSCompControl, new_VFWSCompControl, "VFWSCompControl");
        }
    }

    /* C: DelayTime > 100, then the Filter16 / I0 split and the zero repair */
    for (e = 0; e < 4; e++)
    {
        for (f = 0; f < 2; f++)
        {
            for (g = 0; g < 4; g++)
            {
                for (h = 0; h < 3; h++)
                {
                    for (i = 0; i < 6; i++)
                    {
                        baseline();
                        gMainCmd.FreqDesiredReal = 1000;
                        gInvInfo.InvTypeApply = 0;
                        gMainCmd.Command.bit.SpeedFlag = C_SPEED_FLAG_CON;
                        st.vfPar.VFWsComp = 1000;
                        st.wsComp.Coff = 1000;
                        st.wsComp.DelayTime = delay[e];
                        st.vfPar.VFOvShock = (Uint)ovsh[f];
                        gBasePar.FcSetApply = (Uint)fcset[g];
                        gMotorExtPer.I0 = (Uint)i0[h];
                        st.wsComp.WsCompMTApply.M = applyM[i];
                        st.wsComp.WsCompMTApply.T = applyM[i];
                        st.wsComp.WsCompMT.M = applyM[i];
                        st.wsComp.WsCompMT.T = applyM[i];
                        gMotorExtPer.RatedComp = 100;
                        run(ref_VFWSCompControl, new_VFWSCompControl,
                            "VFWSCompControl");
                    }
                }
            }
        }
    }

    /* the three CompFreq slew arms: CompFreq>>16 against m_WsWindage */
    {
        static const long cf[] = {0L, 1L << 16, 100L << 16, -100L << 16,
                                  300L << 16, 0x7FFFFFFFL};
        static const int  apT[] = {0, 100, 1000, 20000};
        int i, j;

        for (i = 0; i < 6; i++)
        {
            for (j = 0; j < 4; j++)
            {
                baseline();
                gMainCmd.FreqDesiredReal = 1000;
                gInvInfo.InvTypeApply = 0;
                gMainCmd.Command.bit.SpeedFlag = C_SPEED_FLAG_CON;
                st.vfPar.VFWsComp = 1000;
                st.wsComp.Coff = 1000;
                st.wsComp.DelayTime = 200;
                st.wsComp.CompFreq = cf[i];
                st.vfPar.VFOvShock = 0;
                gBasePar.FcSetApply = 100;
                st.wsComp.WsCompMTApply.M = 1000;
                st.wsComp.WsCompMTApply.T = apT[j];
                st.wsComp.WsCompMT.M = 1000;
                gMotorExtPer.RatedComp = 100;
                run(ref_VFWSCompControl, new_VFWSCompControl, "VFWSCompControl");
            }
        }
    }

    /* The slew arms need m_WsWindage within a few units of CompFreq>>16.
     * m_WsWindage is derived, not poked:
     *   3038*T/Im, then *Coff/1000, then *RatedComp>>12
     * With Im == 1000 (Filter16 of 1000 with total 1000 is exactly 1000),
     * Coff == 1000, RatedComp == 100 and T == 100 it comes out at exactly 7.
     * CompFreq>>16 in 4..10 then straddles the +2 / -2 bands:
     *   4 -> +2 arm boundary (the `+2` and `+3` variants part company),
     *   5..9 -> the else arm with a non-zero windage (the `<<16` vs `<<15`
     *           snapshot),
     *  10 -> -2 arm boundary (the `-2` and `-3` variants part company).
     * T == 0 pins the windage at 0, which keeps the else arm covered on the
     * zero side. */
    {
        static const long cf2[] = {4L << 16, 5L << 16, 6L << 16, 7L << 16,
                                   8L << 16, 9L << 16, 10L << 16,
                                   0L, 2L << 16, 3L << 16};
        static const int  apT2[] = {100, 0};
        int i, j;

        for (i = 0; i < 10; i++)
        {
            for (j = 0; j < 2; j++)
            {
                baseline();
                gMainCmd.FreqDesiredReal = 1000;
                gInvInfo.InvTypeApply = 0;
                gMainCmd.Command.bit.SpeedFlag = C_SPEED_FLAG_CON;
                st.vfPar.VFWsComp = 1000;
                st.wsComp.Coff = 1000;
                st.wsComp.DelayTime = 200;
                st.wsComp.CompFreq = cf2[i];
                st.vfPar.VFOvShock = 0;
                gBasePar.FcSetApply = 100;
                st.wsComp.WsCompMTApply.M = 1000;
                st.wsComp.WsCompMTApply.T = apT2[j];
                st.wsComp.WsCompMT.M = 1000;
                st.wsComp.WsCompMT.T = apT2[j];
                gMotorExtPer.I0 = 300;
                gMotorExtPer.RatedComp = 100;
                run(ref_VFWSCompControl, new_VFWSCompControl,
                    "VFWSCompControl");
            }
        }
    }
}

/* ========================================================================== */
/* Phase 8: VFOverMagneticControl                                             */
/* ========================================================================== */

static void phase_ovmag(void)
{
    static const int steps[]  = {STATUS_RUN, STATUS_SPEED_CHECK};
    static const int type[]   = {0, 9, 10};
    static const int spd[]    = {C_SPEED_FLAG_CON, C_SPEED_FLAG_DEC};
    static const int udcf[]   = {0, 1000, 3990, 3991, 3999, 4000, 4001,
                                 4010, 4020, 5000, 65535};
    static const int base[]   = {1, 4000};
    static const int coff[]   = {0, 1, 100, 32767};
    static const int udc[]    = {0, 2000, 4000, 65535};
    int a, b, c, d, e, f, g;

    for (a = 0; a < 2; a++)
    {
        for (b = 0; b < 3; b++)
        {
            for (c = 0; c < 2; c++)
            {
                for (d = 0; d < 11; d++)
                {
                    for (e = 0; e < 2; e++)
                    {
                        for (f = 0; f < 4; f++)
                        {
                            for (g = 0; g < 4; g++)
                            {
                                baseline();
                                gMainStatus.RunStep = steps[a];
                                st.vfPar.VFLineType = (Uint)type[b];
                                gMainCmd.Command.bit.SpeedFlag = (Uint)spd[c];
                                st.varAvr.UDCFilter = udcf[d];
                                gInvInfo.BaseUdc = (Uint)base[e];
                                st.varAvr.CoffApply = coff[f];
                                gUDC.uDC = (Uint)udc[g];
                                gOutVolt.Volt = 3000;
                                run(ref_VFOverMagneticControl,
                                    new_VFOverMagneticControl,
                                    "VFOverMagneticControl");
                            }
                        }
                    }
                }
            }
        }
    }
}

/* ========================================================================== */
/* Phase 9: VFSpeedControl                                                    */
/* ========================================================================== */

static void phase_speedctl(void)
{
    static const int fs[] = {-1000, -1, 0, 1, 1000};
    static const long cf[] = {0L, 1L << 16, -1L << 16, 100L << 16};
    int i, j;

    for (i = 0; i < 5; i++)
    {
        for (j = 0; j < 4; j++)
        {
            baseline();
            gMainCmd.FreqSet = fs[i];
            st.wsComp.CompFreq = cf[j];
            st.vfPar.FreqApply = 100;
            st.vfPar.VFLineType = 0;
            run(ref_VFSpeedControl, new_VFSpeedControl, "VFSpeedControl");
        }
    }
}

/* ========================================================================== */
/* Phase 10: VfOverCurDeal                                                    */
/* ========================================================================== */

static void phase_overcur(void)
{
    static const int coff[]   = {1, 5, 9, 10, 19, 20, 39, 40, 50, 100};
    static const int invt[]   = {15, 18};
    static const int ocpt[]   = {0, 1, 100, 200, 1000};
    static const int cur[]    = {0, 1, 1000, 3000, 3604, 3605, 3800, 3890,
                                 3891, 5000, 65535};
    static const int pre[]    = {C_SPEED_FLAG_CON, C_SPEED_FLAG_ACC,
                                 C_SPEED_FLAG_DEC};
    static const int spd[]    = {C_SPEED_FLAG_CON, C_SPEED_FLAG_ACC,
                                 C_SPEED_FLAG_DEC};
    static const int flag[]   = {0, 1, 0x8000, 0x8001};
    static const int fsyn[]   = {0, 100, -100, 5000, -5000};
    int a, b, c, d, e, f, g, h;

    /* A: (ocGain, InvTypeApply, ocPoint, CurBaseInv) -> SubStep, MaxStep and
     *    the three current arms.  LimLow = CurLim*3605>>12 and
     *    LimHigh = CurLim*3891>>12, so the sweep straddles both boundaries. */
    for (a = 0; a < 10; a++)
    {
        for (b = 0; b < 2; b++)
        {
            for (c = 0; c < 5; c++)
            {
                for (d = 0; d < 11; d++)
                {
                    baseline();
                    st.vfPar.ocGain = (Uint)coff[a];
                    gInvInfo.InvTypeApply = (Uint)invt[b];
                    st.vfPar.ocPoint = (Uint)ocpt[c];
                    gLineCur.CurBaseInv = (Uint)cur[d];
                    st.vfFreq2.preSpdFlag = C_SPEED_FLAG_CON;
                    gMainCmd.Command.bit.SpeedFlag = C_SPEED_FLAG_CON;
                    st.ovCur.Flag = 0;
                    gMainCmd.FreqSyn = 100;
                    st.ovCur.CurBak = 1000;
                    st.ovCur.StepLow = 10;
                    st.ovCur.StepLowLim = 20;
                    st.ovCur.StepHigh = 0;
                    st.ovCur.LowFreq = 100;
                    st.ovCur.MaxStepLow = 1000;
                    st.ovCur.MaxStepHigh = 5000;
                    gBasePar.FullFreq01 = 32768L;
                    run(ref_VfOverCurDeal, new_VfOverCurDeal, "VfOverCurDeal");
                }
            }
        }
    }

    /* B: (preSpdFlag, SpeedFlag, Flag, FreqSyn) -> the change latch, the
     *    bit0 gate and the LowFreq split */
    for (e = 0; e < 3; e++)
    {
        for (f = 0; f < 3; f++)
        {
            for (g = 0; g < 4; g++)
            {
                for (h = 0; h < 5; h++)
                {
                    baseline();
                    st.vfPar.ocGain = 20;
                    st.vfPar.ocPoint = 100;
                    gLineCur.CurBaseInv = 100;      /* below LimLow */
                    st.vfFreq2.preSpdFlag = (Uint)pre[e];
                    gMainCmd.Command.bit.SpeedFlag = (Uint)spd[f];
                    st.ovCur.Flag = flag[g];
                    gMainCmd.FreqSyn = fsyn[h];
                    st.ovCur.CurBak = 1000;
                    st.ovCur.StepLow = 10;
                    st.ovCur.StepLowLim = 20;
                    st.ovCur.StepHigh = 0;
                    st.ovCur.LowFreq = 100;
                    st.ovCur.MaxStepLow = 1000;
                    st.ovCur.MaxStepHigh = 5000;
                    gBasePar.FullFreq01 = 32768L;
                    run(ref_VfOverCurDeal, new_VfOverCurDeal, "VfOverCurDeal");
                }
            }
        }
    }

    /* C: StepHigh sign in the mid band */
    {
        static const int sh[]  = {-6000, -5000, -1, 0, 1, 5000, 20000};
        static const int low[] = {0, 1, 100, 1000, 32767};
        int i, j;

        for (i = 0; i < 7; i++)
        {
            for (j = 0; j < 5; j++)
            {
                baseline();
                st.vfPar.ocGain = 20;
                st.vfPar.ocPoint = 100;
                gLineCur.CurBaseInv = 3700;      /* between 3605 and 3891 */
                st.ovCur.StepHigh = sh[i];
                st.ovCur.StepLow = low[j];
                st.ovCur.StepLowLim = low[j];
                st.ovCur.CurBak = 4000;
                /* MaxStepLow / MaxStepHigh are recomputed from ocGain before
                 * they are read, so these two lines are dead writes.  Kept
                 * because they document the intent; the split they were
                 * supposed to drive is covered by block D, which uses the
                 * recomputed values (83 / 10000). */
                st.ovCur.MaxStepLow = low[j];
                st.ovCur.MaxStepHigh = low[j];
                gMainCmd.FreqSyn = 100;
                st.ovCur.LowFreq = 50;
                run(ref_VfOverCurDeal, new_VfOverCurDeal, "VfOverCurDeal");
            }
        }
    }

    /* D: StepLowLim > MaxStep and the |FreqSyn| < LowFreq split.
     *
     * None of LowFreq / MaxStepLow / MaxStepHigh is an input: the module
     * recomputes all three from ocGain and FullFreq01 before the comparison.
     *     LowFreq     = ((ocGain+50)*10)<<15 / FullFreq01   -> 700 at 20, 900 at 40
     *     MaxStepLow  = 10000/6/ocGain                     -> 83 at 20
     *     MaxStepHigh = 10000                              -> at ocGain < 40
     * So this block used to poke LowFreq = 50 and MaxStepLow == MaxStepHigh,
     * which are all dead writes -- the `<` / `<=` boundary sits at 700, not 50,
     * and OC13 came back MISSED.  StepLowLim 500 (+1 from m_AddStep) lands
     * between 83 and 10000, so whichever arm is taken is visible in StepApply:
     *   orig at |FreqSyn| == 700 -> MaxStepHigh -> StepLowLim stays 501
     *   mut  at |FreqSyn| == 700 -> MaxStepLow  -> StepLowLim -> 475 */
    {
        static const int fsy[] = {0, 699, 700, 701, 899, 900, 901};
        static const int ocg[] = {20, 40};
        int i, j;

        for (j = 0; j < 2; j++)
        {
            for (i = 0; i < 7; i++)
            {
                baseline();
                st.vfPar.ocGain = (Uint)ocg[j];
                st.vfPar.ocPoint = 100;
                gLineCur.CurBaseInv = 100;       /* below LimLow */
                st.vfFreq2.preSpdFlag = C_SPEED_FLAG_CON;
                gMainCmd.Command.bit.SpeedFlag = C_SPEED_FLAG_CON;
                st.ovCur.Flag = 0;
                st.ovCur.StepLowLim = 500;
                st.ovCur.StepLow = 500;
                st.ovCur.CurBak = 1000;
                gMainCmd.FreqSyn = fsy[i];
                gBasePar.FullFreq01 = 32768L;
                run(ref_VfOverCurDeal, new_VfOverCurDeal, "VfOverCurDeal");
            }
        }
    }

    /* sequence: the limiter carries StepLow / StepHigh across calls */
    {
        int k;
        for (k = 0; k < 6; k++)
        {
            static const int cs[] = {100, 3600, 3700, 3900, 5000, 100};
            baseline();
            st.vfPar.ocGain = 20;
            st.vfPar.ocPoint = 100;
            gLineCur.CurBaseInv = (Uint)cs[k];
            st.ovCur.StepHigh = 0;
            st.ovCur.StepLow = 100;
            st.ovCur.StepLowLim = 100;
            run_seq(ref_VfOverCurDeal, new_VfOverCurDeal, "VfOverCurDeal", 6);
        }
    }
}

/* ========================================================================== */
/* Phase 11: VfOverUdcDeal                                                    */
/* ========================================================================== */

static void phase_overudc(void)
{
    static const int spd[]   = {C_SPEED_FLAG_CON, C_SPEED_FLAG_DEC};
    static const int chg[]   = {0, 1};
    static const int coff[]  = {0, 1, 5, 9, 10, 20, 100};
    static const int lim[]   = {1000, 4000, 16000};
    static const int udc[]   = {0, 1, 6099, 6100, 6101, 8000, 9000, 15000,
                                16000, 20000, 65535};
    static const int udcb[]  = {0, 5000, 8000, 12000, 65535};
    int a, b, c, d, e, f;

    /* A: (CoffApply, Limit, uDC, UdcBak) -> the band selection and the four
     *    DetaU arms.  m_Low is the constant 6100, m_Mid = Limit*3932>>12. */
    for (c = 0; c < 7; c++)
    {
        for (d = 0; d < 3; d++)
        {
            for (e = 0; e < 11; e++)
            {
                for (f = 0; f < 5; f++)
                {
                    baseline();
                    gMainCmd.Command.bit.SpeedFlag = C_SPEED_FLAG_DEC;
                    st.vfFreq2.spedChg = 0;
                    st.ovUdc.CoffApply = coff[c];
                    st.ovUdc.Limit = lim[d];
                    gUDC.uDC = (Uint)udc[e];
                    st.ovUdc.UdcBak = udcb[f];
                    st.ovUdc.Flag = 0;
                    st.ovUdc.ExeCnt = 0;
                    st.ovUdc.FreqMax = 5000;
                    st.vfPar.FreqApply = 100;
                    st.vfPar.ovGain = 10;
                    st.ovUdc.CoffAdd = 0;
                    st.ovUdc.StepBak = 0;
                    run(ref_VfOverUdcDeal, new_VfOverUdcDeal, "VfOverUdcDeal");
                }
            }
        }
    }

    /* A2: the same, but with ExeCnt already past the divider, so the band code
     *     is actually reached.  With CoffApply = 20 the first pass exits at
     *     `ExeCnt + 6 < Coff` (COV_UD_EXEC_WAIT), which is why this block
     *     exists -- and why the high band's DetaU < -5 arm needs
     *     m_Mid < uDC <= Limit with UdcBak well above uDC. */
    {
        static const int exe2[] = {0, 14, 20, 100};
        static const int lim2[] = {4000, 16000};
        /* 20000 / 65535 are above Limit, i.e. the top band
         * (`m_Udc > m_High`) where CoffAdd += 2. */
        static const int udc2[] = {3839, 3840, 4000, 15359, 15360, 16000,
                                   20000, 65535};
        static const int bak2[] = {0, 4000, 8000, 16000, 65535};
        int i, j, k, l;

        for (i = 0; i < 4; i++)
        {
            for (j = 0; j < 2; j++)
            {
                for (k = 0; k < 8; k++)
                {
                    for (l = 0; l < 5; l++)
                    {
                        baseline();
                        gMainCmd.Command.bit.SpeedFlag = C_SPEED_FLAG_DEC;
                        st.vfFreq2.spedChg = 0;
                        st.ovUdc.CoffApply = 20;
                        st.ovUdc.Limit = lim2[j];
                        gUDC.uDC = (Uint)udc2[k];
                        st.ovUdc.UdcBak = bak2[l];
                        st.ovUdc.Flag = 0;
                        st.ovUdc.ExeCnt = exe2[i];
                        st.ovUdc.FreqMax = 5000;
                        st.vfPar.FreqApply = 100;
                        st.vfPar.ovGain = 10;
                        st.ovUdc.CoffAdd = 0;
                        st.ovUdc.StepBak = 0;
                        run(ref_VfOverUdcDeal, new_VfOverUdcDeal,
                            "VfOverUdcDeal");
                    }
                }
            }
        }
    }

    /* B: the three reset arms, the |FreqApply| > |FreqMax| guard, the Flag
     *    restore and the ExeCnt divider */
    {
        static const int flag[] = {0, 1, 2, 3, 0xFFFC, 0xFFFF};
        static const int exe[]  = {0, 5, 10, 20};
        static const int fmax[] = {0, 100, 5000};
        static const int fa[]   = {-5000, -100, 0, 100, 5000};
        int g, h, i, j;

        for (a = 0; a < 2; a++)
        {
            for (b = 0; b < 2; b++)
            {
                for (g = 0; g < 6; g++)
                {
                    for (h = 0; h < 4; h++)
                    {
                        for (i = 0; i < 3; i++)
                        {
                            for (j = 0; j < 5; j++)
                            {
                                baseline();
                                gMainCmd.Command.bit.SpeedFlag = (Uint)spd[a];
                                st.vfFreq2.spedChg = (Uint)chg[b];
                                st.ovUdc.CoffApply = 20;
                                st.ovUdc.Limit = 16000;
                                gUDC.uDC = 9000;
                                st.ovUdc.UdcBak = 8000;
                                st.ovUdc.Flag = flag[g];
                                st.ovUdc.ExeCnt = exe[h];
                                st.ovUdc.FreqMax = fmax[i];
                                st.vfPar.FreqApply = fa[j];
                                st.vfPar.ovGain = 10;
                                st.ovUdc.CoffAdd = 0;
                                st.ovUdc.StepBak = 0;
                                run(ref_VfOverUdcDeal, new_VfOverUdcDeal,
                                    "VfOverUdcDeal");
                            }
                        }
                    }
                }
            }
        }
    }

    /* C: CoffAdd adjustment, StepBak clamps, ovGain gate */
    {
        static const int ca[]  = {0, 1, 6, 200};
        static const int sb[]  = {-25000, -20000, 0, 20000, 25000};
        static const int fa[]  = {-5000, 0, 5000};
        static const int fm[]  = {0, 5000};
        static const int og[]  = {0, 10};
        int i, j, k, l, m;

        for (i = 0; i < 4; i++)
        {
            for (j = 0; j < 5; j++)
            {
                for (k = 0; k < 3; k++)
                {
                    for (l = 0; l < 2; l++)
                    {
                        for (m = 0; m < 2; m++)
                        {
                            baseline();
                            gMainCmd.Command.bit.SpeedFlag = C_SPEED_FLAG_DEC;
                            st.vfFreq2.spedChg = 0;
                            st.ovUdc.CoffApply = 20;
                            st.ovUdc.Limit = 16000;
                            gUDC.uDC = 9000;
                            st.ovUdc.UdcBak = 8000;
                            st.ovUdc.Flag = 0;
                            /* past the `ExeCnt + 6 < Coff` divider, otherwise
                             * the clamps below are never reached */
                            st.ovUdc.ExeCnt = 20;
                            st.ovUdc.CoffAdd = ca[i];
                            st.ovUdc.StepBak = sb[j];
                            st.vfPar.FreqApply = fa[k];
                            st.ovUdc.FreqMax = fm[l];
                            st.vfPar.ovGain = (Uint)og[m];
                            run(ref_VfOverUdcDeal, new_VfOverUdcDeal,
                                "VfOverUdcDeal");
                        }
                    }
                }
            }
        }
    }

    /* E: DetaU = uDC - UdcBak landing exactly on the +-5 / +3 thresholds.
     *    `m_DetaU > 5`, `m_DetaU < -5` and `m_DetaU > 3` only separate from
     *    their neighbours ON those values, so they are swept explicitly rather
     *    than left to the coarse grid above. */
    {
        static const long deta[] = {-6L, -5L, -4L, 3L, 4L, 5L, 6L};
        static const int  base[] = {6100, 9000, 15360, 16000, 20000};
        int i, j;

        for (i = 0; i < 7; i++)
        {
            for (j = 0; j < 5; j++)
            {
                long u = (long)base[j];

                baseline();
                gMainCmd.Command.bit.SpeedFlag = C_SPEED_FLAG_DEC;
                st.vfFreq2.spedChg = 0;
                st.ovUdc.CoffApply = 20;
                st.ovUdc.Limit = 16000;
                gUDC.uDC = (Uint)u;
                st.ovUdc.UdcBak = (int)(u - deta[i]);
                st.ovUdc.Flag = 0;
                st.ovUdc.ExeCnt = 20;
                st.ovUdc.FreqMax = 5000;
                st.vfPar.FreqApply = 100;
                st.vfPar.ovGain = 10;
                st.ovUdc.CoffAdd = 0;
                st.ovUdc.StepBak = 0;
                run(ref_VfOverUdcDeal, new_VfOverUdcDeal, "VfOverUdcDeal");
            }
        }
    }

    /* F: CoffApply near its 200 clamp.  CoffApply is also the ExeCnt divider,
     *    so ExeCnt has to be lifted with it or the run exits at
     *    `ExeCnt + 6 < Coff`. */
    {
        static const int ca[]  = {195, 196, 199, 200};
        static const int exe[] = {190, 194, 200, 300};
        static const int add[] = {7, 20, 101};
        int i, j, k;

        for (i = 0; i < 4; i++)
        {
            for (j = 0; j < 4; j++)
            {
                for (k = 0; k < 3; k++)
                {
                    baseline();
                    gMainCmd.Command.bit.SpeedFlag = C_SPEED_FLAG_DEC;
                    st.vfFreq2.spedChg = 0;
                    st.ovUdc.CoffApply = ca[i];
                    st.ovUdc.Limit = 16000;
                    gUDC.uDC = 9000;
                    st.ovUdc.UdcBak = 8000;
                    st.ovUdc.Flag = 0;
                    st.ovUdc.ExeCnt = exe[j];
                    st.ovUdc.FreqMax = 5000;
                    st.vfPar.FreqApply = 100;
                    st.vfPar.ovGain = 10;
                    st.ovUdc.CoffAdd = add[k];
                    st.ovUdc.StepBak = 0;
                    run(ref_VfOverUdcDeal, new_VfOverUdcDeal, "VfOverUdcDeal");
                }
            }
        }
    }

    /* sequence: ExeCnt / StepBak / CoffApply carry across calls */
    {
        int k;
        for (k = 0; k < 4; k++)
        {
            static const int u[] = {5000, 7000, 12000, 20000};
            baseline();
            gMainCmd.Command.bit.SpeedFlag = C_SPEED_FLAG_DEC;
            st.vfFreq2.spedChg = 0;
            st.ovUdc.CoffApply = 20;
            st.ovUdc.Limit = 16000;
            st.ovUdc.FreqMax = 5000;
            st.vfPar.FreqApply = 100;
            gUDC.uDC = (Uint)u[k];
            run_seq(ref_VfOverUdcDeal, new_VfOverUdcDeal, "VfOverUdcDeal", 8);
        }
    }
}

/* ========================================================================== */
/* Phase 12: VfFreqDeal                                                       */
/* ========================================================================== */

/* The limiter pair is shared by every call, so the baseline below sets both
 * limiters to a state where nothing else dominates the branch under test. */
static void fd_base(void)
{
    baseline();
    st.vfPar.ocGain = 20;
    st.vfPar.ovGain = 10;
    st.vfPar.FreqApply = 100;
    gMainCmd.FreqDesired = 100;
    gMainCmd.FreqSet = 1000;
    st.ovCur.StepApply = 10;
    st.ovCur.StepLow = 100;
    st.ovCur.StepLowLim = 100;
    st.ovCur.Flag = 0;
    st.ovCur.MaxStepLow = 1000;
    st.ovCur.MaxStepHigh = 1000;
    st.ovUdc.StepApply = 10;
    st.ovUdc.LastStepApply = 0;
    st.ovUdc.AccTimes = 0;
    st.ovUdc.OvUdcLimitTime = 0;
    st.ovUdc.CoffApply = 20;
    st.ovUdc.Limit = 4000;
    st.ovUdc.StepBak = 0;
    st.ovUdc.ExeCnt = 0;
    st.ovUdc.UdcBak = 5000;
    st.ovUdc.FreqMax = 5000;
    gUDC.uDC = 5000;
    gUDC.uDCFilter = 5000;
    gBasePar.FullFreq01 = 32768L;
}

static void phase_freqdeal(void)
{
    static const int spd[]  = {C_SPEED_FLAG_CON, C_SPEED_FLAG_DEC};
    static const int step[] = {-5000, -1, 0, 1, 5000, 32767};
    static const int ocg[]  = {0, 1, 20};
    static const int ovg[]  = {0, 9, 10};
    static const int limt[] = {0, 4999, 5000, 5001, 5002, 10000};
    static const int fd[]   = {0, 36, 37, 100};
    /* 37 is deliberate: the StepOU force condition is
     * `|FreqApply| - |FreqDesired| < 37`, so the difference has to land
     * exactly on 37 to separate `<` from `< 38`. */
    static const int fa[]   = {-5000, -1, 0, 1, 37, 5000};
    static const int fs[]   = {-5000, -1, 0, 1, 5000};
    int a, b, c, d, e, f, g, h;

    /* A: (speed flag, StepOI, StepOU) -> the min() and the 32767 pass-through */
    for (a = 0; a < 2; a++)
    {
        for (b = 0; b < 6; b++)
        {
            for (c = 0; c < 6; c++)
            {
                fd_base();
                gMainCmd.Command.bit.SpeedFlag = (Uint)spd[a];
                st.ovCur.StepApply = step[b];
                st.ovUdc.StepApply = step[c];
                run(ref_VfFreqDeal, new_VfFreqDeal, "VfFreqDeal");
            }
        }
    }

    /* B: the four terms of the StepOU force condition.
     * VfFreqDeal calls VFOVUdcLimit() first, and that either increments
     * OvUdcLimitTime or resets it to 0 -- so poking 5000 and reading it back
     * is not possible.  StepApply is held at 0 so the increment arm is taken
     * (uDCFilter 5000 > Limit*3932>>12 == 3840), which makes the value the
     * comparison sees limt[b] + 1.  4999/5000/5001/5002 therefore straddle the
     * `> 5000` threshold from below and from above. */
    for (a = 0; a < 3; a++)
    {
        for (b = 0; b < 6; b++)
        {
            for (c = 0; c < 4; c++)
            {
                for (d = 0; d < 6; d++)
                {
                    fd_base();
                    gMainCmd.Command.bit.SpeedFlag = C_SPEED_FLAG_DEC;
                    st.vfPar.ovGain = (Uint)ovg[a];
                    st.ovUdc.OvUdcLimitTime = limt[b];
                    st.ovUdc.StepApply = 0;
                    gMainCmd.FreqDesired = fd[c];
                    st.vfPar.FreqApply = fa[d];
                    run(ref_VfFreqDeal, new_VfFreqDeal, "VfFreqDeal");
                }
            }
        }
    }

    /* C: ocGain == 0 forcing StepOI to 32767 */
    for (e = 0; e < 3; e++)
    {
        for (a = 0; a < 2; a++)
        {
            fd_base();
            gMainCmd.Command.bit.SpeedFlag = (Uint)spd[a];
            st.vfPar.ocGain = (Uint)ocg[e];
            run(ref_VfFreqDeal, new_VfFreqDeal, "VfFreqDeal");
        }
    }

    /* D: direction, zero crossing, and the Deta arm */
    for (a = 0; a < 2; a++)
    {
        for (f = 0; f < 5; f++)
        {
            for (g = 0; g < 5; g++)
            {
                fd_base();
                gMainCmd.Command.bit.SpeedFlag = (Uint)spd[a];
                st.vfPar.FreqApply = fa[f];
                gMainCmd.FreqSet = fs[g];
                gMainCmd.FreqDesired = fs[g];
                st.ovCur.StepApply = 100;
                st.ovUdc.StepApply = 100;
                run(ref_VfFreqDeal, new_VfFreqDeal, "VfFreqDeal");
            }
        }
    }

    /* E: the AccTimes latch and LastStepApply sign flip */
    {
        static const int acc[] = {0, 10, 11, 12};
        static const int lst[] = {-5000, -1, 0, 1, 5000};
        int i, j;

        for (i = 0; i < 4; i++)
        {
            for (j = 0; j < 5; j++)
            {
                fd_base();
                gMainCmd.Command.bit.SpeedFlag = C_SPEED_FLAG_DEC;
                st.ovUdc.AccTimes = acc[i];
                st.ovUdc.LastStepApply = lst[j];
                st.ovUdc.StepApply = -100;
                run(ref_VfFreqDeal, new_VfFreqDeal, "VfFreqDeal");
            }
        }
    }

    /* F: VfStepSet = | |FreqSet| - |FreqApply| | compared against StepOU */
    {
        static const int fs2[] = {-1000, -101, 0, 100, 1000};
        static const int fa2[] = {-1000, -1, 0, 1, 1000};
        static const int ou2[] = {0, 1, 100, 500, 1000, 5000};
        int i, j, k;

        for (i = 0; i < 5; i++)
        {
            for (j = 0; j < 5; j++)
            {
                for (k = 0; k < 6; k++)
                {
                    fd_base();
                    gMainCmd.Command.bit.SpeedFlag = C_SPEED_FLAG_DEC;
                    gMainCmd.FreqSet = fs2[i];
                    gMainCmd.FreqDesired = fs2[i];
                    st.vfPar.FreqApply = fa2[j];
                    st.ovCur.StepApply = 1;
                    st.ovUdc.StepApply = ou2[k];
                    run(ref_VfFreqDeal, new_VfFreqDeal, "VfFreqDeal");
                }
            }
        }
    }

    /* sequence: FreqApply walks towards FreqSet, and the AccTimes latch */
    {
        int k;
        for (k = 0; k < 4; k++)
        {
            static const int sp[] = {C_SPEED_FLAG_DEC, C_SPEED_FLAG_DEC,
                                     C_SPEED_FLAG_CON, C_SPEED_FLAG_ACC};
            fd_base();
            gMainCmd.Command.bit.SpeedFlag = (Uint)sp[k];
            st.vfPar.FreqApply = 0;
            gMainCmd.FreqSet = 1000;
            gMainCmd.FreqDesired = 1000;
            st.ovCur.StepApply = 10;
            st.ovUdc.StepApply = 10;
            st.ovUdc.LastStepApply = -10;
            st.ovUdc.AccTimes = 0;
            run_seq(ref_VfFreqDeal, new_VfFreqDeal, "VfFreqDeal", 8);
        }
    }
}

/* ========================================================================== */
/* Phase 13: VfOscIndexCalc                                                   */
/* ========================================================================== */

static void phase_oscindex(void)
{
    static const int type[]  = {ASYNC_VF, ASYNC_SVC, ASYNC_FVC};
    static const int start[] = {0, 1};
    /* long, not int: the host folds `int` to 16 bits (case shim), so
     * 200000 / 400000 would truncate and never exercise wCntRltm == 0. */
    static const long real[] = {0L, 1L, 1000L, 20000L, 200000L, 400000L};
    static const int use[]   = {0, 1, 199, 200, 201};
    static const int rltm[]  = {0, 1, 200};
    static const int pa[]    = {-20000, -1, 0, 1, 20000};
    int a, b, c, d, e, f;

    for (a = 0; a < 3; a++)
    {
        for (b = 0; b < 2; b++)
        {
            for (c = 0; c < 6; c++)
            {
                for (d = 0; d < 5; d++)
                {
                    for (e = 0; e < 3; e++)
                    {
                        for (f = 0; f < 5; f++)
                        {
                            baseline();
                            gCtrMotorType = (CONTROL_MOTOR_TYPE_ENUM)type[a];
                            gMainCmd.Command.bit.Start = (Uint)start[b];
                            gMainCmd.FreqReal = real[c];
                            st.hvFoscIndex.wCntUse = use[d];
                            st.hvFoscIndex.wCntRltm = rltm[e];
                            gIAmpTheta.PowerAngle = pa[f];
                            st.hvFoscIndex.maxAngle = 1000;
                            st.hvFoscIndex.minAngle = -1000;
                            st.hvFoscIndex.AnglePowerFactor = 1000;
                            run(ref_VfOscIndexCalc, new_VfOscIndexCalc,
                                "VfOscIndexCalc");
                        }
                    }
                }
            }
        }
    }

    /* sequence: the measurement window opens and closes */
    baseline();
    gCtrMotorType = ASYNC_VF;
    gMainCmd.Command.bit.Start = 1;
    gMainCmd.FreqReal = 1000;
    st.hvFoscIndex.wCntUse = 0;
    st.hvFoscIndex.wCntRltm = 3;
    run_seq(ref_VfOscIndexCalc, new_VfOscIndexCalc, "VfOscIndexCalc", 8);
}

/* ========================================================================== */
/* Phase 14: HVfDeadBandComp                                                  */
/* ========================================================================== */

static void phase_deadband(void)
{
    /* long: FreqReal is a 32-bit field and the >40000 arm needs values the
     * 16-bit host `int` cannot hold. */
    static const long real[] = {0L, 150L, 151L, 799L, 800L, 1199L, 1200L,
                                40000L, 40001L, 80000L};
    static const int comp[]  = {0, 1, 100, 1000, -100};
    static const int dc[]    = {0, 1, 2, 3};
    static const int feed[]  = {-32768, -21845, -10922, -5461, 0, 5461,
                                10922, 21845, 32766};
    static const int feedp[] = {-5461, 0, 5461, 10922};
    static const int hf[]    = {1000, 1200, 300};
    static const int lf[]    = {300, 800, 1000};
    static const int width[] = {0, 1, 10, 100};
    int a, b, c, d, e, f, g, h;

    /* A: FreqReal against 40000 and against DbOptActHFreq, and the Comp scale */
    for (a = 0; a < 10; a++)
    {
        for (b = 0; b < 5; b++)
        {
            for (c = 0; c < 4; c++)
            {
                for (f = 0; f < 3; f++)
                {
                    baseline();
                    gMainCmd.FreqReal = real[a];
                    gDeadBand.Comp = comp[b];
                    gMainCmd.Command.bit.StartDC = (Uint)(dc[c] & 1);
                    gMainCmd.Command.bit.StopDC = (Uint)((dc[c] >> 1) & 1);
                    st.hvFdeadBandCompOpt.DbOptActHFreq = hf[f];
                    st.hvFdeadBandCompOpt.DbOptActLFreq = lf[f];
                    gIAmpTheta.Theta = 100;
                    gPhase.IMPhase = 0x00020000L;
                    gPhase.CompPhase = 50;
                    gPhase.StepPhase = 100L << 16;
                    run(ref_HVfDeadBandComp, new_HVfDeadBandComp,
                        "HVfDeadBandComp");
                }
            }
        }
    }

    /* B: the interpolation band needs DbOptActHFreq - DbOptActLFreq + 1 != 0 */
    for (f = 0; f < 3; f++)
    {
        for (g = 0; g < 3; g++)
        {
            baseline();
            gMainCmd.FreqReal = 1000;
            gDeadBand.Comp = 100;
            st.hvFdeadBandCompOpt.DbOptActHFreq = hf[f];
            st.hvFdeadBandCompOpt.DbOptActLFreq = lf[g];
            gPhase.IMPhase = 0x00020000L;
            gPhase.StepPhase = 100L << 16;
            run(ref_HVfDeadBandComp, new_HVfDeadBandComp, "HVfDeadBandComp");
        }
    }

    /* C: the 5461 sector crossing, and the three phase arms */
    for (d = 0; d < 9; d++)
    {
        for (e = 0; e < 4; e++)
        {
            for (h = 0; h < 4; h++)
            {
                baseline();
                gMainCmd.FreqReal = 1000;
                gDeadBand.Comp = 100;
                st.hvFdeadBandCompOpt.CurPhaseFeed = feed[d];
                st.hvFdeadBandCompOpt.CurPhaseFeed_pre = feedp[e];
                st.hvFdeadBandCompOpt.DbCompCpwmWidth = width[h];
                st.hvFdeadBandCompOpt.CurPhasePredict = feed[d];
                st.hvFdeadBandCompOpt.DbOptActHFreq = 1000;
                st.hvFdeadBandCompOpt.DbOptActLFreq = 300;
                gIAmpTheta.Theta = 100;
                gPhase.IMPhase = 0x00020000L;
                gPhase.CompPhase = 50;
                gPhase.StepPhase = 100L << 16;
                run(ref_HVfDeadBandComp, new_HVfDeadBandComp,
                    "HVfDeadBandComp");
            }
        }
    }

    /* D: the predicted phase itself, swept across -21845 / 0 / +21845 */
    {
        static const int pred[] = {-32768, -21846, -21845, -21844, -1, 0, 1,
                                   21844, 21845, 21846, 32767};
        int i, j;

        for (i = 0; i < 11; i++)
        {
            for (j = 0; j < 4; j++)
            {
                baseline();
                gMainCmd.FreqReal = 1000;
                gDeadBand.Comp = 100;
                st.hvFdeadBandCompOpt.CurPhasePredict = pred[i];
                st.hvFdeadBandCompOpt.DbCompCpwmWidth = width[j];
                st.hvFdeadBandCompOpt.CurPhaseFeed = 0;
                st.hvFdeadBandCompOpt.CurPhaseFeed_pre = 0;
                st.hvFdeadBandCompOpt.StepPhaseSet = 0;
                st.hvFdeadBandCompOpt.CurPhaseStepFed = 0;
                gIAmpTheta.Theta = 0;
                gPhase.IMPhase = 0;
                gPhase.CompPhase = -16384;
                gPhase.StepPhase = 0;
                run(ref_HVfDeadBandComp, new_HVfDeadBandComp,
                    "HVfDeadBandComp");
            }
        }
    }

    /* E: the 5461 sector crossing, landed on exactly.
     * CurPhaseFeed_pre is NOT an input -- the module overwrites it from the
     * previous CurPhaseFeed on its first line (block C's feedp[] was a dead
     * write).  So the crossing is produced by poking CurPhaseFeed (which the
     * module reads before overwriting it) and letting the inputs compute the
     * new value:
     *   CurPhaseFeed = (IMPhase>>16) + Theta + CompPhase + 16384
     * With IMPhase = 0, Theta = 0 and CompPhase = -10923 that is exactly 5461,
     * and an old value of 5462 makes phase_sect_pre 5462.  The second clause
     * `(pre >= 5461 && sect <= 5461)` then holds on the `<=` form only. */
    {
        static const int oldf[] = {5460, 5461, 5462, 5463, 10923, 10924};
        int i;

        for (i = 0; i < 6; i++)
        {
            baseline();
            gMainCmd.FreqReal = 1000;
            gDeadBand.Comp = 100;
            st.hvFdeadBandCompOpt.CurPhaseFeed = oldf[i];
            st.hvFdeadBandCompOpt.CurPhasePredict = 0;
            st.hvFdeadBandCompOpt.DbCompCpwmWidth = 1;
            st.hvFdeadBandCompOpt.DbOptActHFreq = 1000;
            st.hvFdeadBandCompOpt.DbOptActLFreq = 300;
            gIAmpTheta.Theta = 0;
            gPhase.IMPhase = 0;
            gPhase.CompPhase = -10923;
            gPhase.StepPhase = 100L << 16;
            run(ref_HVfDeadBandComp, new_HVfDeadBandComp, "HVfDeadBandComp");
        }
    }
}

/* ========================================================================== */
/* Phase 15: HVfOscDampDeal                                                   */
/* ========================================================================== */

static void phase_oscdamp(void)
{
    static const int ovsh[] = {0, 1, 10, 100};
    static const int fsyn[] = {-5000, -1, 0, 1, 5000};
    static const int r1[]   = {0, 300, 30000, 65535};
    static const long im[]  = {0L, 1L << 12, -1L << 12, 100L << 12};
    static const int volt[] = {0, 1, 3000, 32767};
    int a, b, c, d, e;

    for (a = 0; a < 4; a++)
    {
        for (b = 0; b < 5; b++)
        {
            for (c = 0; c < 4; c++)
            {
                for (d = 0; d < 4; d++)
                {
                    for (e = 0; e < 4; e++)
                    {
                        baseline();
                        st.vfPar.VFOvShock = (Uint)ovsh[a];
                        gMainCmd.FreqSyn = fsyn[b];
                        gMotorExtPer.R1 = (Uint)r1[c];
                        gIMTQ24.M = im[d];
                        gOutVolt.Volt = volt[e];
                        st.hvFoscDamp.CurMagSet = 100;
                        st.hvFoscDamp.VoltAmp = volt[e];
                        gMotorExtPer.IoVsFreq = 250;
                        run(ref_HVfOscDampDeal, new_HVfOscDampDeal,
                            "HVfOscDampDeal");
                    }
                }
            }
        }
    }
}

/* ========================================================================== */
/* Phase 16: HVfCurReDecomp                                                   */
/* ========================================================================== */

static void phase_curre(void)
{
    static const int curper[] = {0, 1, 1000, 30000, 65535};
    static const int r1[]     = {0, 300, 30000, 65535};
    static const int va[]     = {0, 1, 100, 3000, 32767};
    static const int pa[]     = {-32768, -16384, -1, 0, 1, 16384, 32767};
    static const int fsyn[]   = {-5000, 0, 5000};
    int a, b, c, d, e;

    for (a = 0; a < 5; a++)
    {
        for (b = 0; b < 4; b++)
        {
            for (c = 0; c < 5; c++)
            {
                for (d = 0; d < 7; d++)
                {
                    for (e = 0; e < 3; e++)
                    {
                        baseline();
                        gLineCur.CurPer = (Uint)curper[a];
                        gMotorExtPer.R1 = (Uint)r1[b];
                        gOutVolt.VoltApply = va[c];
                        gIAmpTheta.PowerAngle = pa[d];
                        gMainCmd.FreqSyn = fsyn[e];
                        run(ref_HVfCurReDecomp, new_HVfCurReDecomp,
                            "HVfCurReDecomp");
                    }
                }
            }
        }
    }
}

/* ========================================================================== */
/* Phase 17: random                                                            */
/* ========================================================================== */

typedef void (*VF_FN)(void);

static VF_FN const g_fnsRef[] =
{
    ref_VfVarInitiate, ref_VFOVUdcLimit, ref_CalTorqueUp,
    ref_VFWsTorqueBoostComm, ref_VFAutoTorqueBoost, ref_VFWSCompControl,
    ref_VFOverMagneticControl, ref_VFSpeedControl, ref_VfOverCurDeal,
    ref_VfOverUdcDeal, ref_VfFreqDeal, ref_VfOscIndexCalc,
    ref_HVfDeadBandComp, ref_HVfOscDampDeal, ref_HVfCurReDecomp
};

static VF_FN const g_fnsNew[] =
{
    new_VfVarInitiate, new_VFOVUdcLimit, new_CalTorqueUp,
    new_VFWsTorqueBoostComm, new_VFAutoTorqueBoost, new_VFWSCompControl,
    new_VFOverMagneticControl, new_VFSpeedControl, new_VfOverCurDeal,
    new_VfOverUdcDeal, new_VfFreqDeal, new_VfOscIndexCalc,
    new_HVfDeadBandComp, new_HVfOscDampDeal, new_HVfCurReDecomp
};

static const char *const g_fnsName[] =
{
    "VfVarInitiate", "VFOVUdcLimit", "CalTorqueUp",
    "VFWsTorqueBoostComm", "VFAutoTorqueBoost", "VFWSCompControl",
    "VFOverMagneticControl", "VFSpeedControl", "VfOverCurDeal",
    "VfOverUdcDeal", "VfFreqDeal", "VfOscIndexCalc",
    "HVfDeadBandComp", "HVfOscDampDeal", "HVfCurReDecomp"
};

#define NFNS ((int)(sizeof(g_fnsRef) / sizeof(g_fnsRef[0])))

static void rand_baseline(void)
{
    baseline();

    st.vfPar.VFLineType = (Uint)rnd_range(0, 12);
    st.vfPar.VFTorqueUp = (Uint)rnd_range(0, 2000);
    st.vfPar.VFTorqueUpLim = (Uint)rnd_range(0, 20000);
    st.vfPar.VFWsComp = (Uint)rnd_range(0, 3000);
    st.vfPar.VFOvShock = (Uint)rnd_range(0, 100);
    st.vfPar.ovGain = (Uint)rnd_range(0, 200);
    st.vfPar.ovPoint = (Uint)rnd_range(0, 1000);
    st.vfPar.ocGain = (Uint)rnd_range(0, 200);
    st.vfPar.ocPoint = (Uint)rnd_range(0, 1000);
    st.vfPar.FreqApply = rnd_range(-5000, 5000);
    st.vfPar.VFLineFreq1 = (Uint)rnd_range(0, 400);
    st.vfPar.VFLineFreq2 = (Uint)rnd_range(0, 800);
    st.vfPar.VFLineFreq3 = (Uint)rnd_range(0, 1200);
    st.vfPar.VFLineVolt1 = (Uint)rnd_range(0, 4096);
    st.vfPar.VFLineVolt2 = (Uint)rnd_range(0, 4096);
    st.vfPar.VFLineVolt3 = (Uint)rnd_range(0, 4096);

    st.vfAutoVar.VfCurrentIs = rnd_range(0, 30000);
    st.vfAutoVar.VfReverseAngle = rnd_range(-16384, 16384);
    st.vfAutoVar.VfRIsSinFai = rnd_range(-30000, 30000);
    st.vfAutoVar.VfRVCosFai = rnd_range(-30000, 30000);
    st.vfAutoVar.VfAngleSin = rnd_range(-16384, 16384);
    st.vfAutoVar.VfReverseVolt = rnd_range(-32767, 32767);
    st.vfAutoVar.VfTorqueEnableTime = rnd_range(0, 400);
    st.vfAutoVar.DestinationVolt = rnd_range(-32767, 32767);
    st.vfAutoVar.AutoTorquePID.Total = rnd_range(-1000000, 1000000);
    st.vfAutoVar.AutoTorquePID.Out = rnd_range(-1000, 1000) << 16;

    st.wsComp.Coff = (Uint)rnd_range(0, 3000);
    st.wsComp.CompFreq = rnd_range(-400L, 400L) << 16;
    st.wsComp.DelayTime = rnd_range(0, 300);
    st.wsComp.WsCompMT.M = rnd_range(-30000, 30000);
    st.wsComp.WsCompMT.T = rnd_range(-30000, 30000);
    st.wsComp.WsCompMTApply.M = rnd_range(-30000, 30000);
    st.wsComp.WsCompMTApply.T = rnd_range(-30000, 30000);

    st.varAvr.UDCFilter = rnd_range(0, 65535);
    st.varAvr.CoffApply = rnd_range(-1000, 1000);

    st.vfFreq2.preSpdFlag = (Uint)rnd_range(0, 3);
    st.vfFreq2.spedChg = (Uint)rnd_range(0, 1);

    st.ovCur.StepApply = rnd_range(-6000, 6000);
    st.ovCur.StepLow = rnd_range(0, 20000);
    st.ovCur.StepLowLim = rnd_range(0, 20000);
    st.ovCur.StepHigh = rnd_range(-6000, 20000);
    st.ovCur.CurBak = rnd_range(0, 65535);
    st.ovCur.LowFreq = rnd_range(0, 3000);
    st.ovCur.MaxStepLow = rnd_range(0, 20000);
    st.ovCur.MaxStepHigh = rnd_range(0, 20000);
    st.ovCur.SubStep = rnd_range(1, 3);
    st.ovCur.Flag = rnd_range(-32768, 32767);

    st.ovUdc.CoffApply = rnd_range(0, 300);
    st.ovUdc.CoffAdd = rnd_range(-10, 300);
    st.ovUdc.Limit = rnd_range(0, 20000);
    st.ovUdc.StepApply = rnd_range(-6000, 6000);
    st.ovUdc.LastStepApply = rnd_range(-6000, 6000);
    st.ovUdc.StepBak = rnd_range(-25000, 25000);
    st.ovUdc.ExeCnt = rnd_range(0, 300);
    st.ovUdc.UdcBak = rnd_range(0, 65535);
    st.ovUdc.Flag = rnd_range(-32768, 32767);
    st.ovUdc.FreqMax = rnd_range(-5000, 5000);
    st.ovUdc.AccTimes = rnd_range(0, 20);
    st.ovUdc.OvUdcLimitTime = rnd_range(0, 12000);

    st.hvFoscDamp.CurMagSet = rnd_range(-30000, 30000);
    st.hvFoscDamp.VoltAmp = rnd_range(-30000, 30000);

    st.hvFoscIndex.AnglePowerFactor = rnd_range(-30000, 30000);
    st.hvFoscIndex.wCntUse = rnd_range(0, 400);
    st.hvFoscIndex.wCntRltm = rnd_range(0, 400);
    st.hvFoscIndex.maxAngle = rnd_range(-30000, 30000);
    st.hvFoscIndex.minAngle = rnd_range(-30000, 30000);

    st.hvFdeadBandCompOpt.CurPhaseFeed = rnd_range(-32768, 32767);
    st.hvFdeadBandCompOpt.CurPhaseFeed_pre = rnd_range(-32768, 32767);
    st.hvFdeadBandCompOpt.CurPhasePredict = rnd_range(-32768, 32767);
    st.hvFdeadBandCompOpt.StepPhaseSet = rnd_range(-30000, 30000);
    st.hvFdeadBandCompOpt.CurPhaseStepFed = rnd_range(-30000, 30000);
    st.hvFdeadBandCompOpt.DbOptActHFreq = rnd_range(0, 30000);
    st.hvFdeadBandCompOpt.DbOptActLFreq = rnd_range(0, 30000);
    st.hvFdeadBandCompOpt.DbCompCpwmWidth = rnd_range(0, 200);

    st.hvFCur.M = rnd_range(-30000, 30000);
    st.hvFCur.T = rnd_range(-30000, 30000);

    gMotorInfo.FreqPer = (Uint)rnd_range(200, 2000);
    gMotorInfo.Votage = (Uint)rnd_range(1, 1000);
    gMotorInfo.Frequency = (Uint)rnd_range(0, 2000);
    gMotorInfo.Current = (Uint)rnd_range(1, 65535);

    gMotorExtPer.UnitCoff = (Uint)rnd_range(0, 100);
    gMotorExtPer.R1 = (Uint)rnd_range(0, 65535);
    gMotorExtPer.I0 = (Uint)rnd_range(0, 3000);
    gMotorExtPer.IoVsFreq = (Uint)rnd_range(0, 3000);
    gMotorExtPer.RatedComp = (Uint)rnd_range(0, 3000);
    gMotorExtInfo.UnitCoff = (Uint)rnd_range(0, 100);
    gMotorExtInfo.R1 = (Uint)rnd_range(0, 65535);

    gBasePar.FullFreq = (Uint)rnd_range(1, 1000);
    gBasePar.FullFreq01 = (Ulong)rnd_range(1, 65535);
    gBasePar.FcSetApply = (Uint)rnd_range(0, 1000);

    gInvInfo.InvTypeApply = (Uint)rnd_range(0, 40);
    gInvInfo.BaseUdc = (Uint)rnd_range(1, 20000);

    gUDC.uDC = (Uint)rnd_range(0, 65535);
    gUDC.uDCFilter = (Uint)rnd_range(0, 65535);

    gLineCur.CurPer = (Uint)rnd_range(0, 65535);
    gLineCur.CurBaseInv = (Uint)rnd_range(0, 65535);

    gIMTQ12.M = rnd_range(-30000, 30000);
    gIMTQ12.T = rnd_range(-30000, 30000);
    gIMTQ24.M = rnd_range(-30000, 30000) << 12;
    gIMTQ24.T = rnd_range(-30000, 30000) << 12;

    gIAlphBeta.Alph = rnd_range(-30000, 30000);
    gIAlphBeta.Beta = rnd_range(-30000, 30000);

    gIAmpTheta.Theta = rnd_range(-16384, 16384);
    gIAmpTheta.PowerAngle = rnd_range(-16384, 16384);
    gIAmpTheta.Amp = (Uint)rnd_range(0, 30000);

    gMainCmd.pu2siCoeff = (Uint)rnd_range(1, 4);
    gMainCmd.FreqReal = rnd_range_l(-5000L, 80000L);
    gMainCmd.FreqDesired = rnd_range(-5000, 5000);
    gMainCmd.FreqDesiredReal = rnd_range(-5000, 5000);
    gMainCmd.FreqSet = rnd_range(-5000, 5000);
    gMainCmd.FreqSetApply = rnd_range(-5000, 5000);
    gMainCmd.FreqSyn = rnd_range(-5000, 5000);
    gMainCmd.Command.all = (Uint)rnd_range(0, 1023);
    gMainCmd.Command.bit.SpeedFlag = (Uint)rnd_range(0, 2);

    gMainStatus.RunStep = (Uint)rnd_range(0, 8);
    gCtrMotorType = (CONTROL_MOTOR_TYPE_ENUM)rnd_range(0, 3);

    gOutVolt.Volt = rnd_range(-5000, 5000);
    gOutVolt.VoltApply = rnd_range(-5000, 5000);
    gOutVolt.MaxOutVolt = rnd_range(0, 5000);
    gOutVolt.vfSplit = rnd_range(0, 5000);

    gPhase.IMPhase = rnd_range(-32768, 32767) << 16;
    gPhase.StepPhase = rnd_range(-32768, 32767) << 16;
    gPhase.CompPhase = rnd_range(-30000, 30000);

    gDeadBand.Comp = rnd_range(-1000, 1000);

    gExtendCmd.bit.SpeedSearch = (Uint)rnd_range(0, 3);
}

static void phase_random(long n)
{
    long i;

    for (i = 0; i < n; i++)
    {
        int k;

        rand_baseline();
        k = rnd_range(0, NFNS - 1);
        run(g_fnsRef[k], g_fnsNew[k], g_fnsName[k]);
    }

    /* the int-returning one separately, so the return value is exercised on
     * arbitrary inputs too */
    for (i = 0; i < n / 4; i++)
    {
        int af;

        rand_baseline();
        af = rnd_range(-32768, 32767);
        g_argFreq = af;
        run_i(ref_CalOutVotInVFStatus, new_CalOutVotInVFStatus, af,
              "CalOutVotInVFStatus");
    }
}

/* ========================================================================== */

int main(void)
{
    long base;
    int i;
    int covZero = 0;

    base = g_cases;
    phase_varinit();
    printf("  VfVarInitiate               : %ld\n", g_cases - base);

    base = g_cases;
    phase_ovudclimit();
    printf("  VFOVUdcLimit                : %ld\n", g_cases - base);

    base = g_cases;
    phase_calout();
    printf("  CalOutVotInVFStatus         : %ld\n", g_cases - base);

    base = g_cases;
    phase_torqueup();
    printf("  CalTorqueUp                 : %ld\n", g_cases - base);

    base = g_cases;
    phase_wsboost();
    printf("  VFWsTorqueBoostComm         : %ld\n", g_cases - base);

    base = g_cases;
    phase_autotq();
    printf("  VFAutoTorqueBoost           : %ld\n", g_cases - base);

    base = g_cases;
    phase_wscomp();
    printf("  VFWSCompControl             : %ld\n", g_cases - base);

    base = g_cases;
    phase_ovmag();
    printf("  VFOverMagneticControl       : %ld\n", g_cases - base);

    base = g_cases;
    phase_speedctl();
    printf("  VFSpeedControl              : %ld\n", g_cases - base);

    base = g_cases;
    phase_overcur();
    printf("  VfOverCurDeal               : %ld\n", g_cases - base);

    base = g_cases;
    phase_overudc();
    printf("  VfOverUdcDeal               : %ld\n", g_cases - base);

    base = g_cases;
    phase_freqdeal();
    printf("  VfFreqDeal                  : %ld\n", g_cases - base);

    base = g_cases;
    phase_oscindex();
    printf("  VfOscIndexCalc              : %ld\n", g_cases - base);

    base = g_cases;
    phase_deadband();
    printf("  HVfDeadBandComp             : %ld\n", g_cases - base);

    base = g_cases;
    phase_oscdamp();
    printf("  HVfOscDampDeal              : %ld\n", g_cases - base);

    base = g_cases;
    phase_curre();
    printf("  HVfCurReDecomp              : %ld\n", g_cases - base);

    base = g_cases;
    phase_random(2000L);
    printf("  random                      : %ld\n", g_cases - base);

    printf("  -- branch coverage --\n");
    for (i = 0; i < COV_COUNT; i++)
    {
        if (g_cov[i] == 0)
        {
            printf("  [ZERO] %s\n", g_covName[i]);
            covZero++;
        }
    }
    printf("  covered %d / %d\n", COV_COUNT - covZero, COV_COUNT);

    printf("MotorVF difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    return (g_fails || covZero) ? 1 : 0;
}
