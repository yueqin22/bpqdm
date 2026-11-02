/* Differential test driver for MotorVCMain.
 *
 * The module is the vector-control core of the drive: the ASR (speed loop), the
 * CSR (current loop), the SVC stator-voltage calculator and the alpha/beta
 * projection.  Sixteen functions, all `void f(void)`, all driven directly here.
 *
 * ---------------------------------------------------------------------------
 * Shape of this bench: two independent copies of sixteen globals
 * ---------------------------------------------------------------------------
 * MotorVCMain DEFINES sixteen data objects (gVCPar, gIMTSet, gIMTSetApply,
 * gUMTSet, gUAmpTheta, gImAcrQ24, gItAcrQ24, gAsr, gVcCsrPara, gIMSetAcr,
 * gWspid, gPWMVAlphBeta, gUdcLimitIt, gSvcVoltAdj, gABVoltSet, SVC0Signal).
 * ref_tu.c / new_tu.c rename them per side, so the two translation units each
 * own a full set -- unlike cases/MotorImParEst, where the module owned nothing
 * and one shared image sufficed.
 *
 * So every case is: zero BOTH sides, poke the same state inputs into both,
 * snapshot the shared mock ledger, call ref, capture; zero both again, restore
 * the ledger, call new, capture; then memcmp the two 16-global images.
 *
 * The memcmp is byte-wise on purpose.  Both sides are memset to zero before
 * every call, so padding bytes are deterministic too, and a field the rewrite
 * forgot to write shows up as a byte difference rather than being masked by a
 * field-by-field list that also forgot it.  Field names are recovered afterwards
 * only for the error message.
 *
 * ---------------------------------------------------------------------------
 * What is compared
 * ---------------------------------------------------------------------------
 *   16 module globals          byte-wise, plus field names on failure
 *   gHostCpuCtl[7]             DINT / EINT around the gIMTSetApply snapshot
 *   PID / PID32 / ResetSVC     call count, last argument struct, hashed output
 *
 * ---------------------------------------------------------------------------
 * What is NOT compared, and why
 * ---------------------------------------------------------------------------
 *   The bodies of PID / PID32 / ResetSVC (other modules, not under test).
 *   qsin / qsqrt / atan beyond "both sides call the same approximation".
 *   The real-time meaning of DINT/EINT on the target (host counters only).
 *   Divisions by zero: the firmware divides by gMotorInfo.Current,
 *   gInvInfo.InvCurrent, gBasePar.FullFreq / FullFreq01, gMotorExtPer.L1 and by
 *   (gIMTSet.M>>12).  All of those are zero on an unconfigured drive, and on the
 *   target a zero divisor silently returns garbage rather than trapping.  The
 *   host would raise #DE and kill the process, so this driver keeps every one of
 *   them >= 1 (or, for the shifts, non-zero).  The consequence is stated plainly:
 *   the "divisor is zero" region is not covered by any result below.
 *
 * ---------------------------------------------------------------------------
 * Loop shape: one-factor-at-a-time plus a few focused crosses
 * ---------------------------------------------------------------------------
 * A full cross product over these inputs is in the 10^6 range and would make
 * every one of the 356 negative-control variants pay for it.  Each phase
 * therefore walks the factors that interact (a threshold against the value it is
 * compared with) and sweeps the rest one at a time from a fixed baseline, which
 * is what actually drives branch coverage here: none of these functions has a
 * branch whose condition mixes three independent knobs.
 */

#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "MotorInclude.h"
#include "MotorVCInclude.h"

/* ---- per-side exports ---------------------------------------------------- */

extern VC_INFO_STRUCT           ref_gVCPar;
extern MT_STRUCT_Q24            ref_gIMTSet;
extern MT_STRUCT_Q24            ref_gIMTSetApply;
extern MT_STRUCT                ref_gUMTSet;
extern AMPTHETA_STRUCT          ref_gUAmpTheta;
extern PID32_STRUCT             ref_gImAcrQ24;
extern PID32_STRUCT             ref_gItAcrQ24;
extern ASR_STRUCT               ref_gAsr;
extern VC_CSR_PARA              ref_gVcCsrPara;
extern PID_STRUCT               ref_gIMSetAcr;
extern PID_STRUCT               ref_gWspid;
extern MT_STRUCT                ref_gPWMVAlphBeta;
extern UDC_LIMIT_IT_STRUCT      ref_gUdcLimitIt;
extern PID_STRUCT               ref_gSvcVoltAdj;
extern ALPHABETA_STRUCT         ref_gABVoltSet;
extern SVC0_STRUCT              ref_SVC0Signal;

extern VC_INFO_STRUCT           new_gVCPar;
extern MT_STRUCT_Q24            new_gIMTSet;
extern MT_STRUCT_Q24            new_gIMTSetApply;
extern MT_STRUCT                new_gUMTSet;
extern AMPTHETA_STRUCT          new_gUAmpTheta;
extern PID32_STRUCT             new_gImAcrQ24;
extern PID32_STRUCT             new_gItAcrQ24;
extern ASR_STRUCT               new_gAsr;
extern VC_CSR_PARA              new_gVcCsrPara;
extern PID_STRUCT               new_gIMSetAcr;
extern PID_STRUCT               new_gWspid;
extern MT_STRUCT                new_gPWMVAlphBeta;
extern UDC_LIMIT_IT_STRUCT      new_gUdcLimitIt;
extern PID_STRUCT               new_gSvcVoltAdj;
extern ALPHABETA_STRUCT         new_gABVoltSet;
extern SVC0_STRUCT              new_SVC0Signal;

extern void ref_ResetParForVC(void);
extern void ref_ResetAsrPar(void);
extern void ref_ResetCsrPar(void);
extern void ref_CalIMSet(void);
extern void ref_CalTorqueLimitPar(void);
extern void ref_PrepareAsrPar(void);
extern void ref_CalUdcLimitIT(void);
extern void ref_VcAsrControl(void);
extern void ref_CalWsAndSynFreq(void);
extern void ref_CalWsAndSynFreq_380(void);
extern void ref_PrepImCsrPara(void);
extern void ref_VCCsrControl(void);
extern void ref_SvcCalOutVolt(void);
extern void ref_SvcCalOutVolt_380(void);
extern void ref_CalcABVolt(void);
extern void ref_VCCsrControl_380(void);

extern void new_ResetParForVC(void);
extern void new_ResetAsrPar(void);
extern void new_ResetCsrPar(void);
extern void new_CalIMSet(void);
extern void new_CalTorqueLimitPar(void);
extern void new_PrepareAsrPar(void);
extern void new_CalUdcLimitIT(void);
extern void new_VcAsrControl(void);
extern void new_CalWsAndSynFreq(void);
extern void new_CalWsAndSynFreq_380(void);
extern void new_PrepImCsrPara(void);
extern void new_VCCsrControl(void);
extern void new_SvcCalOutVolt(void);
extern void new_SvcCalOutVolt_380(void);
extern void new_CalcABVolt(void);
extern void new_VCCsrControl_380(void);

/* ---- the shared mock's ledger ------------------------------------------- */

extern volatile Uint16 gHostCpuCtl[7];
extern unsigned short  gPidCount;
extern unsigned short  gPid32Count;
extern unsigned short  gResetSvcCount;
extern PID_STRUCT      gPidLog;
extern PID32_STRUCT    gPid32Log;
extern long            gPidOutForce;
extern long            gPid32OutForce;
extern long            gPid32Force2;

#define FAIL_LIMIT   20

static long g_cases;
static int  g_fails;
static int  g_reported;

/* ---- comparison image ---------------------------------------------------- */

typedef struct IMG
{
    VC_INFO_STRUCT          vcPar;
    MT_STRUCT_Q24           imtSet;
    MT_STRUCT_Q24           imtSetApply;
    MT_STRUCT               umtSet;
    AMPTHETA_STRUCT         uAmpTheta;
    PID32_STRUCT            imAcrQ24;
    PID32_STRUCT            itAcrQ24;
    ASR_STRUCT              asr;
    VC_CSR_PARA             vcCsrPara;
    PID_STRUCT              imSetAcr;
    PID_STRUCT              wspid;
    MT_STRUCT               pwmVAlphBeta;
    UDC_LIMIT_IT_STRUCT     udcLimitIt;
    PID_STRUCT              svcVoltAdj;
    ALPHABETA_STRUCT        abVoltSet;
    SVC0_STRUCT             svc0Signal;

    /* The next two are owned by other translation units, but this module
     * writes them.  They have always been in the WORLD snapshot (they
     * MUST be restored, or ref's leftovers become new's inputs) yet were
     * never compared -- so any mutation whose only effect is a write to
     * one of them was invisible. */
    OUT_VOLT_STRUCT         outVolt;
    BASE_COMMAND_STRUCT     mainCmd;

    Uint16                  cpuCtl[7];
    unsigned short          pidCnt;
    unsigned short          pid32Cnt;
    unsigned short          resetSvcCnt;
    PID_STRUCT              pidLog;
    PID32_STRUCT            pid32Log;
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
    F("gVCPar",          vcPar),
    F("gIMTSet",         imtSet),
    F("gIMTSetApply",    imtSetApply),
    F("gUMTSet",         umtSet),
    F("gUAmpTheta",      uAmpTheta),
    F("gImAcrQ24",       imAcrQ24),
    F("gItAcrQ24",       itAcrQ24),
    F("gAsr",            asr),
    F("gVcCsrPara",      vcCsrPara),
    F("gIMSetAcr",       imSetAcr),
    F("gWspid",          wspid),
    F("gPWMVAlphBeta",   pwmVAlphBeta),
    F("gUdcLimitIt",     udcLimitIt),
    F("gSvcVoltAdj",     svcVoltAdj),
    F("gABVoltSet",      abVoltSet),
    F("SVC0Signal",      svc0Signal),
    F("gOutVolt",        outVolt),
    F("gMainCmd",        mainCmd),
    F("gHostCpuCtl",     cpuCtl),
    F("PID count",       pidCnt),
    F("PID32 count",     pid32Cnt),
    F("ResetSVC count",  resetSvcCnt),
    F("PID arg log",     pidLog),
    F("PID32 arg log",   pid32Log)
};

#define NFIELDS ((int)(sizeof(g_fields) / sizeof(g_fields[0])))

/* ---- capture / compare --------------------------------------------------- */

/* Defined further down with the coverage table; declared here because run() and
 * run_seq() classify before the definition appears (an implicit declaration
 * would make it `int f()` and then collide with the real `void f(...)`). */
static void cov_observe(const char *fn, const IMG *a);

static void capture_ref(IMG *p)
{
    p->vcPar         = ref_gVCPar;
    p->imtSet        = ref_gIMTSet;
    p->imtSetApply   = ref_gIMTSetApply;
    p->umtSet        = ref_gUMTSet;
    p->uAmpTheta     = ref_gUAmpTheta;
    p->imAcrQ24      = ref_gImAcrQ24;
    p->itAcrQ24      = ref_gItAcrQ24;
    p->asr           = ref_gAsr;
    p->vcCsrPara     = ref_gVcCsrPara;
    p->imSetAcr      = ref_gIMSetAcr;
    p->wspid         = ref_gWspid;
    p->pwmVAlphBeta  = ref_gPWMVAlphBeta;
    p->udcLimitIt    = ref_gUdcLimitIt;
    p->svcVoltAdj    = ref_gSvcVoltAdj;
    p->abVoltSet     = ref_gABVoltSet;
    p->svc0Signal    = ref_SVC0Signal;

    /* Shared objects: read the scene right after the ref call.
     * world_load() puts them back to the pre-call state before new runs,
     * so both sides start identical and any difference is the module's. */
    p->outVolt       = gOutVolt;
    p->mainCmd       = gMainCmd;

    p->cpuCtl[0]     = gHostCpuCtl[0];
    p->cpuCtl[1]     = gHostCpuCtl[1];
    p->cpuCtl[2]     = gHostCpuCtl[2];
    p->cpuCtl[3]     = gHostCpuCtl[3];
    p->cpuCtl[4]     = gHostCpuCtl[4];
    p->cpuCtl[5]     = gHostCpuCtl[5];
    p->cpuCtl[6]     = gHostCpuCtl[6];
    p->pidCnt        = gPidCount;
    p->pid32Cnt      = gPid32Count;
    p->resetSvcCnt   = gResetSvcCount;
    p->pidLog        = gPidLog;
    p->pid32Log      = gPid32Log;
}

static void capture_new(IMG *p)
{
    p->vcPar         = new_gVCPar;
    p->imtSet        = new_gIMTSet;
    p->imtSetApply   = new_gIMTSetApply;
    p->umtSet        = new_gUMTSet;
    p->uAmpTheta     = new_gUAmpTheta;
    p->imAcrQ24      = new_gImAcrQ24;
    p->itAcrQ24      = new_gItAcrQ24;
    p->asr           = new_gAsr;
    p->vcCsrPara     = new_gVcCsrPara;
    p->imSetAcr      = new_gIMSetAcr;
    p->wspid         = new_gWspid;
    p->pwmVAlphBeta  = new_gPWMVAlphBeta;
    p->udcLimitIt    = new_gUdcLimitIt;
    p->svcVoltAdj    = new_gSvcVoltAdj;
    p->abVoltSet     = new_gABVoltSet;
    p->svc0Signal    = new_SVC0Signal;

    p->outVolt       = gOutVolt;
    p->mainCmd       = gMainCmd;

    p->cpuCtl[0]     = gHostCpuCtl[0];
    p->cpuCtl[1]     = gHostCpuCtl[1];
    p->cpuCtl[2]     = gHostCpuCtl[2];
    p->cpuCtl[3]     = gHostCpuCtl[3];
    p->cpuCtl[4]     = gHostCpuCtl[4];
    p->cpuCtl[5]     = gHostCpuCtl[5];
    p->cpuCtl[6]     = gHostCpuCtl[6];
    p->pidCnt        = gPidCount;
    p->pid32Cnt      = gPid32Count;
    p->resetSvcCnt   = gResetSvcCount;
    p->pidLog        = gPidLog;
    p->pid32Log      = gPid32Log;
}

/* ---- the shared ledger must be restored between the two sides ------------ */

/* ---- the outside world must be restored between the two sides, too -------
 *
 * The module under test WRITES globals owned by other translation units:
 *
 *   gMainCmd.FreqSyn / FreqWs      CalWsAndSynFreq, VcAsrControl, ResetCsrPar
 *   gMainCmd.FreqSynFilter         CalTorqueLimitPar (Filter16)
 *   gMainCmd.FirstCnt              VcAsrControl (the low-frequency dead zone)
 *   gMainCmd.FreqFeed              VcAsrControl (same dead zone)
 *   gOutVolt.VoltApply / Phase     VCCsrControl, SvcCalOutVolt
 *
 * Those are SINGLE objects shared by both sides, so without a restore the ref
 * call's leftovers become the new call's inputs.  The symptom is not subtle but
 * it is misleading: ref and new are the SAME code at this point, so any mismatch
 * is proof the bench is comparing two different starting states, not two
 * implementations.  (Measured before this fix: gAsr and the PID argument log
 * differed at byte 36 / 72 and byte 8 -- the leftover FreqSynFilter.)
 *
 * The snapshot is taken once per case, after the driver has poked its inputs and
 * before either side runs. */

typedef struct WORLD
{
    MOTOR_STRUCT                    motorInfo;
    MOTOR_EXTERN_STRUCT             motorExtPer;
    RUN_STATUS_STRUCT               mainStatus;
    BASE_COMMAND_STRUCT             mainCmd;
    BASE_PAR_STRUCT                 basePar;
    COM_PAR_INFO_STRUCT             comPar;
    CONTROL_MOTOR_TYPE_ENUM         ctrMotorType;
    INV_STRUCT                      invInfo;
    VF_INFO_STRUCT                  vfPar;
    UDC_STRUCT                      udc;
    OVER_UDC_CTL_STRUCT             ovUdc;
    ANGLE_STRUCT                    phase;
    PM_DECOUPLE                     pmDecoup;
    PM_FLUX_WEAK                    fluxWeak;
    MOTOR_DEBUG_DATA_RECEIVE_STRUCT testData;
    OUT_VOLT_STRUCT                 outVolt;
    MT_STRUCT                       imtq12;
    MT_STRUCT                       imtSetQ12;
    MT_STRUCT_Q24                   imtq24;
    MT_STRUCT_Q24                   imtq12obs;
    MT_STRUCT_Q24                   imtq24obs;
} WORLD;

static void world_save(WORLD *w)
{
    w->motorInfo    = gMotorInfo;
    w->motorExtPer  = gMotorExtPer;
    w->mainStatus   = gMainStatus;
    w->mainCmd      = gMainCmd;
    w->basePar      = gBasePar;
    w->comPar       = gComPar;
    w->ctrMotorType = gCtrMotorType;
    w->invInfo      = gInvInfo;
    w->vfPar        = gVFPar;
    w->udc          = gUDC;
    w->ovUdc        = gOvUdc;
    w->phase        = gPhase;
    w->pmDecoup     = gPmDecoup;
    w->fluxWeak     = gFluxWeak;
    w->testData     = gTestDataReceive;
    w->outVolt      = gOutVolt;
    w->imtq12       = gIMTQ12;
    w->imtSetQ12    = gIMTSetQ12;
    w->imtq24       = gIMTQ24;
    w->imtq12obs    = gIMTQ12_obs;
    w->imtq24obs    = gIMTQ24_obs;
}

static void world_load(const WORLD *w)
{
    gMotorInfo      = w->motorInfo;
    gMotorExtPer    = w->motorExtPer;
    gMainStatus     = w->mainStatus;
    gMainCmd        = w->mainCmd;
    gBasePar        = w->basePar;
    gComPar         = w->comPar;
    gCtrMotorType   = w->ctrMotorType;
    gInvInfo        = w->invInfo;
    gVFPar          = w->vfPar;
    gUDC            = w->udc;
    gOvUdc          = w->ovUdc;
    gPhase          = w->phase;
    gPmDecoup       = w->pmDecoup;
    gFluxWeak       = w->fluxWeak;
    gTestDataReceive = w->testData;
    gOutVolt        = w->outVolt;
    gIMTQ12         = w->imtq12;
    gIMTSetQ12      = w->imtSetQ12;
    gIMTQ24         = w->imtq24;
    gIMTQ12_obs     = w->imtq12obs;
    gIMTQ24_obs     = w->imtq24obs;
}

typedef struct LEDGER
{
    Uint16         cpuCtl[7];
    unsigned short pid;
    unsigned short pid32;
    unsigned short resetSvc;
    PID_STRUCT     pidLog;
    PID32_STRUCT   pid32Log;
} LEDGER;

static void ledger_save(LEDGER *l)
{
    l->cpuCtl[0] = gHostCpuCtl[0];
    l->cpuCtl[1] = gHostCpuCtl[1];
    l->cpuCtl[2] = gHostCpuCtl[2];
    l->cpuCtl[3] = gHostCpuCtl[3];
    l->cpuCtl[4] = gHostCpuCtl[4];
    l->cpuCtl[5] = gHostCpuCtl[5];
    l->cpuCtl[6] = gHostCpuCtl[6];
    l->pid       = gPidCount;
    l->pid32     = gPid32Count;
    l->resetSvc  = gResetSvcCount;
    l->pidLog    = gPidLog;
    l->pid32Log  = gPid32Log;
}

static void ledger_restore(const LEDGER *l)
{
    gHostCpuCtl[0] = l->cpuCtl[0];
    gHostCpuCtl[1] = l->cpuCtl[1];
    gHostCpuCtl[2] = l->cpuCtl[2];
    gHostCpuCtl[3] = l->cpuCtl[3];
    gHostCpuCtl[4] = l->cpuCtl[4];
    gHostCpuCtl[5] = l->cpuCtl[5];
    gHostCpuCtl[6] = l->cpuCtl[6];
    gPidCount      = l->pid;
    gPid32Count    = l->pid32;
    gResetSvcCount = l->resetSvc;
    gPidLog        = l->pidLog;
    gPid32Log      = l->pid32Log;
}

/* ---- module-owned globals: zeroed before every case, poked as inputs ----- */

static void mod_zero(void)
{
    memset(&ref_gVCPar,        0, sizeof(ref_gVCPar));
    memset(&ref_gIMTSet,       0, sizeof(ref_gIMTSet));
    memset(&ref_gIMTSetApply,  0, sizeof(ref_gIMTSetApply));
    memset(&ref_gUMTSet,       0, sizeof(ref_gUMTSet));
    memset(&ref_gUAmpTheta,    0, sizeof(ref_gUAmpTheta));
    memset(&ref_gImAcrQ24,     0, sizeof(ref_gImAcrQ24));
    memset(&ref_gItAcrQ24,     0, sizeof(ref_gItAcrQ24));
    memset(&ref_gAsr,          0, sizeof(ref_gAsr));
    memset(&ref_gVcCsrPara,    0, sizeof(ref_gVcCsrPara));
    memset(&ref_gIMSetAcr,     0, sizeof(ref_gIMSetAcr));
    memset(&ref_gWspid,        0, sizeof(ref_gWspid));
    memset(&ref_gPWMVAlphBeta, 0, sizeof(ref_gPWMVAlphBeta));
    memset(&ref_gUdcLimitIt,   0, sizeof(ref_gUdcLimitIt));
    memset(&ref_gSvcVoltAdj,   0, sizeof(ref_gSvcVoltAdj));
    memset(&ref_gABVoltSet,    0, sizeof(ref_gABVoltSet));
    memset(&ref_SVC0Signal,    0, sizeof(ref_SVC0Signal));

    memset(&new_gVCPar,        0, sizeof(new_gVCPar));
    memset(&new_gIMTSet,       0, sizeof(new_gIMTSet));
    memset(&new_gIMTSetApply,  0, sizeof(new_gIMTSetApply));
    memset(&new_gUMTSet,       0, sizeof(new_gUMTSet));
    memset(&new_gUAmpTheta,    0, sizeof(new_gUAmpTheta));
    memset(&new_gImAcrQ24,     0, sizeof(new_gImAcrQ24));
    memset(&new_gItAcrQ24,     0, sizeof(new_gItAcrQ24));
    memset(&new_gAsr,          0, sizeof(new_gAsr));
    memset(&new_gVcCsrPara,    0, sizeof(new_gVcCsrPara));
    memset(&new_gIMSetAcr,     0, sizeof(new_gIMSetAcr));
    memset(&new_gWspid,        0, sizeof(new_gWspid));
    memset(&new_gPWMVAlphBeta, 0, sizeof(new_gPWMVAlphBeta));
    memset(&new_gUdcLimitIt,   0, sizeof(new_gUdcLimitIt));
    memset(&new_gSvcVoltAdj,   0, sizeof(new_gSvcVoltAdj));
    memset(&new_gABVoltSet,    0, sizeof(new_gABVoltSet));
    memset(&new_SVC0Signal,    0, sizeof(new_SVC0Signal));
}

/* State inputs that live in the module's OWN globals.  Several functions read
 * their own previous output (CalUdcLimitIT reads UDCBakCnt / UDCBak /
 * FirstOvUdcFlag, CalTorqueLimitPar reads gIMTSet.M, VcAsrControl reads
 * gAsr.Asr.Total, CalTorqueLimitPar and CalUdcLimitIT read gAsr.TorqueLimit,
 * nearly everything reads gVCPar ...).  Those are inputs to the step under test,
 * so they are poked into BOTH sides identically -- otherwise "the rewrite reads
 * a stale value" would compare two different starting states instead of one.
 *
 * They cannot be poked at the call site, because run() zeroes both sides first:
 * anything written before run() would be erased.  So the driver keeps the
 * intended state in `st` (its own copy) and run() / run_seq() apply it right
 * after the zeroing, on both sides.  Same pattern as the `IMG` snapshot in
 * cases/MotorImParEst, only in the other direction.
 *
 * `gVCPar` is in this group too.  In the firmware it is filled in by the
 * function-code layer and MotorVCMain only reads it -- but MotorVCMain is the
 * translation unit that DEFINES it, so each side owns a private copy and the
 * driver must write both. */

typedef struct STATE
{
    long            imM;
    long            imT;
    long            apM;
    long            apT;
    int             umM;
    int             umT;
    long            asrTotal;
    long            asrOut;
    int             udcBakCnt;
    int             udcBak;
    int             udcFirst;
    long            udcPidTotal;
    long            acrImOut;
    long            acrItOut;
    int             torqueLimit;
    int             udcLimit;
    int             udcDeta;
    Uint            svcCal;
    Uint            svcCalB;
    long            wspidTotal;
    long            tqLimPidTotal;
    VC_INFO_STRUCT  vcPar;
} STATE;

static STATE st;

static void state_default(void)
{
    memset(&st, 0, sizeof(st));
    st.torqueLimit = 900;
    st.vcPar.VCTorqueLim   = 2000;
    st.vcPar.ASRKpHigh     = 100;
    st.vcPar.ASRKpLow      = 20;
    st.vcPar.ASRTIHigh     = 50;
    st.vcPar.ASRTILow      = 10;
    st.vcPar.ASRSwitchHigh = 2000;
    st.vcPar.ASRSwitchLow  = 500;
    st.vcPar.SvcMode       = 0;
    st.vcPar.AcrImKp       = 50;
    st.vcPar.AcrImKi       = 10;
    st.vcPar.AcrItKp       = 50;
    st.vcPar.AcrItKi       = 10;
    st.vcPar.VCWsCoff      = 100;
}

static void state_apply(void)
{
    ref_gIMTSet.M = st.imM;             new_gIMTSet.M = st.imM;
    ref_gIMTSet.T = st.imT;             new_gIMTSet.T = st.imT;
    ref_gIMTSetApply.M = st.apM;        new_gIMTSetApply.M = st.apM;
    ref_gIMTSetApply.T = st.apT;        new_gIMTSetApply.T = st.apT;
    ref_gUMTSet.M = st.umM;             new_gUMTSet.M = st.umM;
    ref_gUMTSet.T = st.umT;             new_gUMTSet.T = st.umT;

    ref_gAsr.Asr.Total = st.asrTotal;   new_gAsr.Asr.Total = st.asrTotal;
    ref_gAsr.Asr.Out   = st.asrOut;     new_gAsr.Asr.Out   = st.asrOut;
    ref_gAsr.TorqueLimit = st.torqueLimit;
    new_gAsr.TorqueLimit = st.torqueLimit;
    ref_gAsr.TorqueLimitPid.Total = st.tqLimPidTotal;
    new_gAsr.TorqueLimitPid.Total = st.tqLimPidTotal;

    ref_gUdcLimitIt.UDCBakCnt = st.udcBakCnt;
    new_gUdcLimitIt.UDCBakCnt = st.udcBakCnt;
    ref_gUdcLimitIt.UDCBak = st.udcBak;
    new_gUdcLimitIt.UDCBak = st.udcBak;
    ref_gUdcLimitIt.FirstOvUdcFlag = st.udcFirst;
    new_gUdcLimitIt.FirstOvUdcFlag = st.udcFirst;
    ref_gUdcLimitIt.UdcPid.Total = st.udcPidTotal;
    new_gUdcLimitIt.UdcPid.Total = st.udcPidTotal;

    ref_gUdcLimitIt.UDCLimit = st.udcLimit;
    new_gUdcLimitIt.UDCLimit = st.udcLimit;
    ref_gUdcLimitIt.UDCDeta  = st.udcDeta;
    new_gUdcLimitIt.UDCDeta  = st.udcDeta;

    /* SVC0Signal is module-owned, but these two flags are INPUTS:
     * SvcCalOutVolt writes them only when a condition holds and leaves
     * them alone otherwise.  Never driving them means "original keeps
     * the value, mutant writes 0" reads 0 on both sides. */
    ref_SVC0Signal.VoltSVCCalSignal  = st.svcCal;
    new_SVC0Signal.VoltSVCCalSignal  = st.svcCal;
    ref_SVC0Signal.VoltSVCCalSignalB = st.svcCalB;
    new_SVC0Signal.VoltSVCCalSignalB = st.svcCalB;

    ref_gImAcrQ24.Out = st.acrImOut;    new_gImAcrQ24.Out = st.acrImOut;
    ref_gItAcrQ24.Out = st.acrItOut;    new_gItAcrQ24.Out = st.acrItOut;

    ref_gWspid.Total = st.wspidTotal;   new_gWspid.Total = st.wspidTotal;

    ref_gVCPar = st.vcPar;              new_gVCPar = st.vcPar;
}

static void set_imtset(long M, long T)
{
    st.imM = M; st.imT = T;
}

static void set_imtset_apply(long M, long T)
{
    st.apM = M; st.apT = T;
}

static void set_umtset(int M, int T)
{
    st.umM = M; st.umT = T;
}

static void set_asr_state(long total, long out)
{
    st.asrTotal = total; st.asrOut = out;
}

static void set_udc_state(int bakCnt, int bak, int firstFlag, long pidTotal)
{
    st.udcBakCnt = bakCnt;
    st.udcBak    = bak;
    st.udcFirst  = firstFlag;
    st.udcPidTotal = pidTotal;
}

static void set_acr_state(long imOut, long itOut)
{
    st.acrImOut = imOut;
    st.acrItOut = itOut;
}

/* gAsr.TorqueLimit is READ by CalUdcLimitIT (as the PID Max and the direct
 * assignment) and is a state input like the ones above -- and because gAsr is
 * module-owned it has to be poked on both sides, not just one.  (Writing plain
 * `gAsr` here does not even compile: the symbol only exists as ref_gAsr /
 * new_gAsr.) */
static void set_torque_limit(int lim)
{
    st.torqueLimit = lim;
}

/* ---- input baseline ------------------------------------------------------ */

static void baseline(void)
{
    /* Reset the driver-side state first: every phase overrides only the fields
     * it is sweeping, and the rest has to come from here. */
    state_default();

    /* ---- other modules' globals ---- */
    memset(&gMotorInfo,    0, sizeof(gMotorInfo));
    memset(&gMotorExtPer,  0, sizeof(gMotorExtPer));
    memset(&gMainCmd,      0, sizeof(gMainCmd));
    memset(&gMainStatus,   0, sizeof(gMainStatus));
    memset(&gBasePar,      0, sizeof(gBasePar));
    memset(&gComPar,       0, sizeof(gComPar));
    memset(&gInvInfo,      0, sizeof(gInvInfo));
    memset(&gVFPar,        0, sizeof(gVFPar));
    memset(&gUDC,          0, sizeof(gUDC));
    memset(&gOvUdc,        0, sizeof(gOvUdc));
    memset(&gPhase,        0, sizeof(gPhase));
    memset(&gPmDecoup,     0, sizeof(gPmDecoup));
    memset(&gFluxWeak,     0, sizeof(gFluxWeak));
    memset(&gTestDataReceive, 0, sizeof(gTestDataReceive));
    memset(&gOutVolt,      0, sizeof(gOutVolt));
    memset(&gIMTQ12,       0, sizeof(gIMTQ12));
    memset(&gIMTSetQ12,    0, sizeof(gIMTSetQ12));
    memset(&gIMTQ24,       0, sizeof(gIMTQ24));
    memset(&gIMTQ12_obs,   0, sizeof(gIMTQ12_obs));
    memset(&gIMTQ24_obs,   0, sizeof(gIMTQ24_obs));

    gMotorInfo.MotorType   = MOTOR_TYPE_PM;
    gMotorInfo.Current     = 1000;
    gMotorInfo.CurrentGet  = 1000;
    gMotorInfo.FreqPer     = 500;
    gMotorInfo.Motor_HFreq = 800;
    gMotorInfo.Motor_LFreq = 200;

    gMotorExtPer.I0                = 300;
    gMotorExtPer.IoVsFreq          = 250;
    gMotorExtPer.FluxLeakCurveGain = 100;
    gMotorExtPer.R1                = 300;
    gMotorExtPer.R2                = 400;
    gMotorExtPer.L0                = 450;
    gMotorExtPer.L1                = 500;

    gBasePar.FullFreq01 = 32768L;
    gBasePar.FullFreq   = 100;
    gBasePar.FcSetApply = 100;

    gInvInfo.InvCurrent   = 1000;
    gInvInfo.InvTypeApply = 18;
    gInvInfo.InvCurrForP  = 1000;

    gVFPar.SVCTorqueUpLim = 800;
    gVFPar.SVCTorqueUp    = 100;
    gVFPar.ovGain         = 10;

    st.vcPar.VCTorqueLim   = 2000;
    st.vcPar.ASRKpHigh     = 100;
    st.vcPar.ASRKpLow      = 20;
    st.vcPar.ASRTIHigh     = 50;
    st.vcPar.ASRTILow      = 10;
    st.vcPar.ASRSwitchHigh = 2000;
    st.vcPar.ASRSwitchLow  = 500;
    st.vcPar.SvcMode       = 0;
    st.vcPar.AcrImKp       = 50;
    st.vcPar.AcrImKi       = 10;
    st.vcPar.AcrItKp       = 50;
    st.vcPar.AcrItKi       = 10;
    st.vcPar.VCWsCoff      = 100;

    gUDC.uDCFilter = 3000;
    gOvUdc.Limit   = 4000;

    gPmDecoup.RotVd = 100;
    gPmDecoup.RotVq = 100;
    gFluxWeak.DecoupleMode = 1;

    gMainStatus.RunStep = STATUS_RUN;

    gCtrMotorType = ASYNC_SVC;

    gComPar.StartDCBrakeCur = 100;

    gOutVolt.MaxOutVolt = 1000;

    gIMTQ12.T    = 100;
    gIMTSetQ12.M = 100;
    gIMTSetQ12.T = 100;

    gPhase.IMPhase = 0;

    gPidOutForce   = -1;
    gPid32OutForce = -1;
    gPid32Force2   = -1;
    gPidCount      = 0;
    gPid32Count    = 0;
    gResetSvcCount = 0;
    memset(&gPidLog,   0, sizeof(gPidLog));
    memset(&gPid32Log, 0, sizeof(gPid32Log));
    memset((void *)gHostCpuCtl, 0, sizeof(gHostCpuCtl));
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
            printf("    %-18s first diff at byte %2d\n", g_fields[i].name, first);
        }
    }
}

/* ---- keeping the VARIANTS' divisors non-zero ---------------------------- *
 * Two mutations in build/_vc_negctl.py turn a field that the original only
 * multiplies by into a divisor:
 *
 *   TL1  `... / gInvInfo.InvCurrent`  ->  `... / gInvInfo.InvCurrForP`
 *   IM9  `|FreqSyn| < FreqPer`        ->  `|FreqSyn| > FreqPer`, which sends
 *        |FreqSyn| < FreqPer into the flux-leak branch, whose divisor is
 *        recomputed as  ((|FreqSyn| - FreqPer) * FluxLeakCurveGain)/100
 *                       + FreqPer
 *
 * Both divisors can be 0 on inputs the original never divides by.  The
 * original's own divide is safe by construction: it only runs when
 * |FreqSyn| >= FreqPer, and there the expression is >= FreqPer.  A divide
 * fault (0xC0000094) is NOT a verdict -- the ledger treats any rc != 0 as
 * CAUGHT, so a process that dies before comparing anything would be recorded
 * as a catch.  Both of these were exactly that.
 *
 * So the bench refuses to feed the corner.  The adjustment is applied once,
 * before either side runs, so both sides see the same input and the
 * comparison is untouched.  What is given up is only "does the variant divide
 * by zero", which is not a behavioural claim about the firmware. */

static long flux_divisor(void)
{
    long f = (long)gMainCmd.FreqSyn;
    long p = (long)gMotorInfo.FreqPer;

    if (f < 0) { f = -f; }
    return (((f - p) * (long)gMotorExtPer.FluxLeakCurveGain) / 100) + p;
}

static void keep_divisors_nonzero(void)
{
    long f = (long)gMainCmd.FreqSyn;

    /* TL1 */
    if (0 == gInvInfo.InvCurrForP)
    {
        gInvInfo.InvCurrForP = 1;
    }

    /* IM9 */
    if (f < 0) { f = -f; }
    if ((f < (long)gMotorInfo.FreqPer) && (0 == flux_divisor()))
    {
        gMotorExtPer.FluxLeakCurveGain = 0;
    }
}

/* Call both sides from the identical starting state and compare.
 *
 * The order matters: ref runs, its image is captured, then BOTH sides are zeroed
 * again and the shared ledger is restored before new runs.  Without the restore
 * the second side would start with the first side's PID counter, and every
 * count comparison would be "1 vs 2" forever. */
static void run(void (*rf)(void), void (*nf)(void), const char *name)
{
    IMG a;
    IMG b;
    LEDGER l;
    WORLD w;

    /* Every field is assigned below; the padding bytes are not. */
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));

    keep_divisors_nonzero();

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

    /* classified here, not right after rf(): the ref call may have written
     * gMainCmd / gOutVolt, and the classification must see the inputs the
     * module actually got. */
    cov_observe(name, &a);

    nf();
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

/* Sequence form: the module carries state between calls (CalUdcLimitIT's
 * UDCBakCnt divider is the reason this exists), so the state has to be advanced
 * by the module itself rather than poked.  Both sides get the same number of
 * calls from the same start, and EVERY step is compared -- comparing only the
 * last step would hide a divergence that later re-converges. */
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

static unsigned long g_seed = 0x12345678UL;

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

/* ========================================================================== */
/* Phase 1: ResetParForVC / ResetAsrPar / ResetCsrPar                         */
/* ========================================================================== */

/* Branches of interest:
 *   ResetParForVC : ControlMode == IDC_SVC_CTL -> ResetSVC() called, else not
 *   ResetCsrPar   : MOTOR_TYPE_PM -> the PM pair of assignments, else the
 *                   non-PM one (a division by gMotorInfo.Current)
 *   ResetCsrPar   : the `#if 0` block is compiled out, so the live pair is
 *                   gPmDecoup.RotVd / RotVq -- driving those is what proves it
 *   every path    : FreqSyn = FreqFeed at the end
 */
static void phase_reset(void)
{
    static const int modes[]    = {0, 1, 2, 3};
    static const int types[]    = {2, 1};
    static const int rot[]      = {-1000, -1, 0, 1, 1000};
    static const int currents[] = {1, 1000, 65535};
    static const int feeds[]    = {-1000, 0, 1000};
    int m, t, r, cu, f;

    for (m = 0; m < 4; m++)
    {
        for (t = 0; t < 2; t++)
        {
            for (r = 0; r < 5; r++)
            {
                for (cu = 0; cu < 3; cu++)
                {
                    for (f = 0; f < 3; f++)
                    {
                        baseline();
                        gMainCmd.Command.bit.ControlMode = (Uint)modes[m];
                        gMotorInfo.MotorType = (Uint)types[t];
                        gPmDecoup.RotVd = rot[r];
                        gPmDecoup.RotVq = -rot[r];
                        gMotorInfo.Current = (Uint)currents[cu];
                        gMainCmd.FreqFeed = feeds[f];
                        gMainCmd.FreqSyn = 1234;

                        set_imtset(4096L, 2048L);
                        set_imtset_apply(1024L, 512L);
                        set_umtset(11, 12);
                        set_asr_state(777L, 888L);
                        st.wspidTotal = 5L;
                        st.tqLimPidTotal = 6L;

                        run(ref_ResetParForVC, new_ResetParForVC, "ResetParForVC");
                        run(ref_ResetAsrPar,   new_ResetAsrPar,   "ResetAsrPar");
                        run(ref_ResetCsrPar,   new_ResetCsrPar,   "ResetCsrPar");
                    }
                }
            }
        }
    }
}

/* ========================================================================== */
/* Phase 2: CalIMSet                                                          */
/* ========================================================================== */

/* Branches:
 *   PreExcFlux == 1              -> gIMTSet.M = StartDCBrakeCur * 167772, return
 *   |FreqSyn| < FreqPer          -> m_MaxI0Freq = I0
 *   else                         -> flux-leak curve + a division by m_AbsFreq
 *   SVCTorqueUp == 0             -> gIMTSet.M = m_MaxI0Freq << 12
 *   SVCTorqueUp != 0 and
 *       Im_AbsFreq <  m_LowFreq  -> boost
 *       in [m_LowFreq, m_HigFreq)-> linear ramp (two divisions)
 *       >= m_HigFreq             -> clamp
 *   m_HigFreq / m_LowFreq come from SVCTorqueUpLim clamped by Motor_HFreq /
 *   Motor_LFreq, so group B walks the clamp against the same frequencies group
 *   A uses.
 */
static void phase_imset(void)
{
    static const int pre[]  = {0, 1};
    static const int freq[] = {-2000, -1000, -501, -500, -499, -1, 0,
                               1, 499, 500, 501, 1000, 2000, 32767};
    static const int per[]  = {1, 500, 1000};
    static const int i0[]   = {0, 1, 300, 30000};
    static const int gain[] = {0, 50, 100, 200};
    static const int up[]   = {0, 1, 100, 1023};
    static const int uplim[]= {0, 100, 800, 5000};
    static const int hf[]   = {100, 800, 32767};
    static const int lf[]   = {0, 200, 700};
    int p, f, i, k;

    /* A: PreExcFlux x FreqSyn x FreqPer -- the flux-leak division and the
     *    |FreqSyn| < FreqPer boundary. */
    for (p = 0; p < 2; p++)
    {
        for (f = 0; f < 14; f++)
        {
            for (i = 0; i < 3; i++)
            {
                baseline();
                gMainCmd.Command.bit.PreExcFlux = (Uint)pre[p];
                gMainCmd.FreqSyn = freq[f];
                gMotorInfo.FreqPer = (Uint)per[i];
                gComPar.StartDCBrakeCur = 100;
                run(ref_CalIMSet, new_CalIMSet, "CalIMSet");
            }
        }
    }

    /* B: SVCTorqueUp x SVCTorqueUpLim at each frequency -- the three-way
     *    boost / ramp / clamp split. */
    for (p = 0; p < 2; p++)
    {
        for (f = 0; f < 14; f++)
        {
            for (i = 0; i < 4; i++)
            {
                for (k = 0; k < 4; k++)
                {
                    baseline();
                    gMainCmd.Command.bit.PreExcFlux = (Uint)pre[p];
                    gMainCmd.FreqSyn = freq[f];
                    gVFPar.SVCTorqueUp = (Uint)up[i];
                    gVFPar.SVCTorqueUpLim = (Uint)uplim[k];
                    run(ref_CalIMSet, new_CalIMSet, "CalIMSet");
                }
            }
        }
    }

    /* C: I0 x FluxLeakCurveGain at each frequency. */
    for (f = 0; f < 14; f++)
    {
        for (i = 0; i < 4; i++)
        {
            for (k = 0; k < 4; k++)
            {
                baseline();
                gMainCmd.FreqSyn = freq[f];
                gMotorExtPer.I0 = (Uint)i0[i];
                gMotorExtPer.FluxLeakCurveGain = (Uint)gain[k];
                run(ref_CalIMSet, new_CalIMSet, "CalIMSet");
            }
        }
    }

    /* D: the two clamps that bound m_LowFreq / m_HigFreq. */
    for (f = 0; f < 14; f++)
    {
        for (i = 0; i < 3; i++)
        {
            for (k = 0; k < 3; k++)
            {
                baseline();
                gMainCmd.FreqSyn = freq[f];
                gMotorInfo.Motor_HFreq = (Uint)hf[i];
                gMotorInfo.Motor_LFreq = (Uint)lf[k];
                run(ref_CalIMSet, new_CalIMSet, "CalIMSet");
            }
        }
    }
}

/* ========================================================================== */
/* Phase 3: CalTorqueLimitPar                                                 */
/* ========================================================================== */

/* Branches:
 *   InvTypeApply == 15            -> 1700^2 base, else 1800^2
 *   VCTorqueLim > m_MaxLimit      -> clamp
 *   TorqueCtl && !PM && |FreqSyn| >= FreqPer -> divide by (gIMTSet.M>>12)
 *   ControlMode == IDC_SVC_CTL && !PM && SvcMode == 0 -> Filter16 + two clamps
 *   TorqueCtl || ControlMode == IDC_FVC_CTL -> straight assignment, else PID
 *   InvTypeApply > 20             -> KP/KI 100 instead of 1000
 */
static void phase_torquelim(void)
{
    static const int invtype[] = {0, 15, 16, 20, 21, 30};
    static const int invcur[]  = {1, 1000, 65535};
    static const int cur[]     = {1, 1000};
    static const int tc[]      = {0, 1};
    static const int fsyn[]    = {-1000, -500, 0, 500, 1000};
    static const long mval[]   = {-4096000L, -4096L, 4096L, 4096000L};
    static const int mode[]    = {0, 1};
    static const int svcmode[] = {0, 1};
    static const int tlim[]    = {0, 1, 1200, 2000, 32767};
    static const int mt[]      = {2, 1};
    static const long force[]  = {-1, 0, 1L << 16};
    int a, b, c, d, e, g, h, k, l, m;

    /* A: the two "which converter" inputs against the frequency boundary. */
    for (a = 0; a < 6; a++)
    {
        for (b = 0; b < 3; b++)
        {
            for (c = 0; c < 2; c++)
            {
                for (d = 0; d < 2; d++)
                {
                    for (e = 0; e < 5; e++)
                    {
                        for (k = 0; k < 2; k++)
                        {
                            baseline();
                            gInvInfo.InvTypeApply = (Uint)invtype[a];
                            gInvInfo.InvCurrent = (Uint)invcur[b];
                            gMotorInfo.Current = (Uint)cur[c];
                            gMainCmd.Command.bit.TorqueCtl = (Uint)tc[d];
                            gMainCmd.FreqSyn = fsyn[e];
                            gMotorInfo.MotorType = (Uint)mt[k];
                            gMainCmd.FreqFeed = fsyn[e];
                            gMainCmd.FreqSynFilter = 77;
                            set_torque_limit(123);

                            set_imtset(8192L, 8192L);
                            set_imtset_apply(8192L, 4096L);

                            run(ref_CalTorqueLimitPar, new_CalTorqueLimitPar,
                                "CalTorqueLimitPar");
                        }
                    }
                }
            }
        }
    }

    /* B: the SVC-only block (ControlMode / SvcMode) and the Kp/Ki switch. */
    for (g = 0; g < 2; g++)
    {
        for (h = 0; h < 2; h++)
        {
            for (a = 0; a < 6; a++)
            {
                for (l = 0; l < 5; l++)
                {
                    baseline();
                    gMainCmd.Command.bit.ControlMode = (Uint)mode[g];
                    st.vcPar.SvcMode = (Uint)svcmode[h];
                    gInvInfo.InvTypeApply = (Uint)invtype[a];
                    st.vcPar.VCTorqueLim = (Uint)tlim[l];
                    gMotorInfo.MotorType = 1;
                    gMainCmd.FreqFeed = 300;
                    gMainCmd.FreqSynFilter = 77;
                    set_torque_limit(123);
                    set_imtset(8192L, 8192L);
                    run(ref_CalTorqueLimitPar, new_CalTorqueLimitPar,
                        "CalTorqueLimitPar");
                }
            }
        }
    }

    /* C: the M-magnitude boundary (the divide by gIMTSet.M>>12) and the
     *    PID output boundary. */
    for (m = 0; m < 4; m++)
    {
        for (k = 0; k < 2; k++)
        {
            for (l = 0; l < 3; l++)
            {
                baseline();
                gMainCmd.Command.bit.TorqueCtl = 1;
                gMainCmd.FreqSyn = 1000;
                gMotorInfo.MotorType = 1;
                gPidOutForce = force[l];
                set_imtset((long)mval[m], 8192L);
                run(ref_CalTorqueLimitPar, new_CalTorqueLimitPar,
                    "CalTorqueLimitPar");
            }
        }
    }
}

/* ========================================================================== */
/* Phase 4: PrepareAsrPar                                                     */
/* ========================================================================== */

/* Branches:
 *   MOTOR_TYPE_PM                 -> Kp << 7 / Ki << 9, else << 8 / << 10
 *   (ASRKpHigh>>5) >= ASRTIHigh   -> KIHigh = 32767, else the division
 *   same for ASRTILow
 *   |FreqSyn| <= SwitchLow        -> KPLow/KILow
 *   |FreqSyn| >= SwitchHigh       -> KPHigh/KIHigh
 *   else                          -> interpolation (two divisions)
 *   __IQsat on tempKp / tempKi
 *   KpLimit > 64 && !TorqueCtl && IntegralDiscrete -> KI = 0
 */
static void phase_asrpar(void)
{
    static const int ty[]    = {2, 1};
    static const int kphi[]  = {0, 1, 100, 1023};
    static const int kplo[]  = {0, 1, 20, 500};
    static const int tih[]   = {0, 1, 3, 50, 1000};
    static const int til[]   = {0, 1, 3, 10, 1000};
    static const int swh[]   = {0, 500, 2000, 32767};
    static const int swl[]   = {0, 500, 2000};
    static const int fsyn[]  = {-4000, -2000, -500, 0, 500, 2000, 4000};
    static const int fdes[]  = {-4000, 0, 4000};
    static const int ffeed[] = {-4000, 0, 4000};
    static const int tc[]    = {0, 1};
    static const int integ[] = {0, 1};
    static const long ff01[] = {1L, 32768L, 100000L};
    int a, b, c, d, e, f, g, h, i, j, k;

    /* A: the gain chain -- type, the two Kp inputs, the two Ti inputs. */
    for (a = 0; a < 2; a++)
    {
        for (b = 0; b < 4; b++)
        {
            for (c = 0; c < 4; c++)
            {
                for (d = 0; d < 5; d++)
                {
                    for (e = 0; e < 5; e++)
                    {
                        baseline();
                        gMotorInfo.MotorType = (Uint)ty[a];
                        st.vcPar.ASRKpHigh = (Uint)kphi[b];
                        st.vcPar.ASRKpLow = (Uint)kplo[c];
                        st.vcPar.ASRTIHigh = (Uint)tih[d];
                        st.vcPar.ASRTILow = (Uint)til[e];
                        gMainCmd.FreqSyn = 1000;
                        run(ref_PrepareAsrPar, new_PrepareAsrPar, "PrepareAsrPar");
                    }
                }
            }
        }
    }

    /* B: the three-way switch on |FreqSyn|. */
    for (f = 0; f < 4; f++)
    {
        for (g = 0; g < 3; g++)
        {
            for (h = 0; h < 7; h++)
            {
                for (a = 0; a < 2; a++)
                {
                    baseline();
                    st.vcPar.ASRSwitchHigh = (Uint)swh[f];
                    st.vcPar.ASRSwitchLow = (Uint)swl[g];
                    gMainCmd.FreqSyn = fsyn[h];
                    gMotorInfo.MotorType = (Uint)ty[a];
                    run(ref_PrepareAsrPar, new_PrepareAsrPar, "PrepareAsrPar");
                }
            }
        }
    }

    /* C: the KI = 0 gate and the FullFreq01 scaling. */
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 3; j++)
        {
            for (k = 0; k < 2; k++)
            {
                for (a = 0; a < 2; a++)
                {
                    for (b = 0; b < 2; b++)
                    {
                        baseline();
                        gMainCmd.FreqDesired = fdes[i];
                        gMainCmd.FreqFeed = ffeed[j];
                        gMainCmd.Command.bit.TorqueCtl = (Uint)tc[k];
                        gMainCmd.Command.bit.IntegralDiscrete = (Uint)integ[a];
                        gBasePar.FullFreq01 = (Ulong)ff01[b];
                        run(ref_PrepareAsrPar, new_PrepareAsrPar, "PrepareAsrPar");
                    }
                }
            }
        }
    }
}

/* ========================================================================== */
/* Phase 5: CalUdcLimitIT                                                     */
/* ========================================================================== */

/* Branches:
 *   ovGain == 0                   -> reset path
 *   UDCBakCnt reaching 5          -> sample UDCDeta, clear the counter
 *   UDCDeta < 200                 -> forced to 0
 *   uDCFilter below the soft knee -> UdcPid.Total = -T<<16, UDCLimit = torque
 *   below Limit with FirstOvUdcFlag == 0 -> latch UDCLimit from UdcPid.Total
 *   otherwise                     -> PID with ovGain-scaled gains
 *
 * The UDCBakCnt divider makes this a SEQUENCE test: the counter is module state
 * and only reaches 5 by being incremented five times, so a single call can
 * never cover the sampling branch.  Six calls per case, every step compared.
 */
static void phase_udclimit(void)
{
    static const int gain[]  = {0, 1, 10, 1000};
    static const int fil[]   = {0, 1000, 3000, 3900, 4100};
    static const int lim[]   = {0, 1000, 4000};
    static const int flag[]  = {0, 1};
    static const int bak[]   = {0, 3500};
    static const int tq[]    = {-1000, 1000};
    static const long force[] = {-1, 3L << 16};
    int a, b, c, d, e, f, g;

    for (a = 0; a < 4; a++)
    {
        for (b = 0; b < 5; b++)
        {
            for (c = 0; c < 3; c++)
            {
                for (d = 0; d < 2; d++)
                {
                    for (e = 0; e < 2; e++)
                    {
                        for (f = 0; f < 2; f++)
                        {
                            for (g = 0; g < 2; g++)
                            {
                                baseline();
                                gVFPar.ovGain = (Uint)gain[a];
                                gUDC.uDCFilter = (Uint)fil[b];
                                gOvUdc.Limit = lim[c];
                                gIMTQ12.T = tq[f];
                                set_torque_limit(900);
                                gPidOutForce = force[g];

                                set_udc_state(0, bak[e], flag[d], 7L);

                                run_seq(ref_CalUdcLimitIT, new_CalUdcLimitIT,
                                        "CalUdcLimitIT", 6);
                            }
                        }
                    }
                }
            }
        }
    }
}

/* ========================================================================== */
/* Phase 6: VcAsrControl                                                      */
/* ========================================================================== */

/* Branches:
 *   PreExcFlux && !PM             -> early return with two clears
 *   gCtrMotorType == ASYNC_SVC    -> SvcMode == 1 -> low-frequency dead zone
 *                                    (with the TestData8 blend), else the
 *                                    plain dead zone
 *   else                          -> straight copy
 *   OutOff                        -> ResetParForVC()
 * The dead zone compares |FreqSetApply| < m03HzPu and |FreqFeed| < m05HzPu,
 * where m03HzPu / m05HzPu are derived from FullFreq01 -- so the sweep walks the
 * threshold from both sides.
 */
static void phase_vcasr(void)
{
    static const int ctr[]   = {0, 1, 2, 10, 11, 12, 20, 21};
    static const int mode[]  = {0, 1};
    static const int fsa[]   = {-16384, -3, 0, 3, 16384};
    static const int ffd[]   = {-16384, -2, 0, 2, 16384};
    static const int td8[]   = {0, 1, 16, 31, 32, 100};
    static const int off[]   = {0, 1};
    static const int pre[]   = {0, 1};
    static const int ty[]    = {2, 1};
    static const long force[] = {-1, 0, 5L << 16};
    int a, b, c, d, e, f, g, h;

    /* A: the SVC dead zone against the two frequency thresholds. */
    for (a = 0; a < 8; a++)
    {
        for (b = 0; b < 2; b++)
        {
            for (c = 0; c < 5; c++)
            {
                for (d = 0; d < 5; d++)
                {
                    for (e = 0; e < 2; e++)
                    {
                        baseline();
                        gCtrMotorType = (CONTROL_MOTOR_TYPE_ENUM)ctr[a];
                        st.vcPar.SvcMode = (Uint)mode[b];
                        gMainCmd.FreqSetApply = fsa[c];
                        gMainCmd.FreqFeed = ffd[d];
                        gMainStatus.StatusWord.bit.OutOff = (Uint)off[e];
                        set_asr_state(100L, 200L);
                        run(ref_VcAsrControl, new_VcAsrControl, "VcAsrControl");
                    }
                }
            }
        }
    }

    /* B: the pre-excitation early return and the TestData8 blend. */
    for (f = 0; f < 2; f++)
    {
        for (g = 0; g < 2; g++)
        {
            for (h = 0; h < 6; h++)
            {
                for (b = 0; b < 2; b++)
                {
                    baseline();
                    gMainCmd.Command.bit.PreExcFlux = (Uint)pre[f];
                    gMotorInfo.MotorType = (Uint)ty[g];
                    gTestDataReceive.TestData8 = td8[h];
                    st.vcPar.SvcMode = (Uint)mode[b];
                    gCtrMotorType = ASYNC_SVC;
                    gMainCmd.FreqSetApply = 1000;
                    gMainCmd.FreqFeed = 1000;
                    set_asr_state(100L, 200L);
                    run(ref_VcAsrControl, new_VcAsrControl, "VcAsrControl");
                }
            }
        }
    }

    /* C: the PID output boundary. */
    for (a = 0; a < 3; a++)
    {
        for (c = 0; c < 5; c++)
        {
            baseline();
            gPidOutForce = force[a];
            gMainCmd.FreqSetApply = fsa[c];
            gMainCmd.FreqFeed = ffd[c];
            set_asr_state(100L, 200L);
            run(ref_VcAsrControl, new_VcAsrControl, "VcAsrControl");
        }
    }
}

/* ========================================================================== */
/* Phase 7: CalWsAndSynFreq / CalWsAndSynFreq_380                             */
/* ========================================================================== */

/* Branches:
 *   PreExcFlux == 1               -> FreqWs = 0, FreqSyn = FreqFeed, return
 *   otherwise                     -> two chained divisions
 * The plain version reads gIMTSet.M>>12, the _380 version reads
 * gIMTSetApply.M>>12 -- driving those two to DIFFERENT values is what makes
 * "the rewrite used the wrong one of the two" observable.
 */
static void phase_ws(void)
{
    static const int pre[] = {0, 1};
    static const long mM[] = {-8192000L, -4096L, 4096L, 8192000L};
    static const long mT[] = {-4096L, 0, 4096L};
    static const long apM[] = {-4096L, 4096L, 8192000L};
    static const int r2[]  = {0, 1, 400, 32767};
    static const int l1[]  = {1, 500, 32767};
    static const int cof[] = {-1000, 0, 100, 1000};
    static const int ffd[] = {-1000, 0, 1000};
    int a, b, c, d, e, f, g, h;

    for (a = 0; a < 2; a++)
    {
        for (b = 0; b < 4; b++)
        {
            for (c = 0; c < 3; c++)
            {
                for (d = 0; d < 3; d++)
                {
                    for (e = 0; e < 4; e++)
                    {
                        for (f = 0; f < 3; f++)
                        {
                            for (g = 0; g < 4; g++)
                            {
                                for (h = 0; h < 3; h++)
                                {
                                    baseline();
                                    gMainCmd.Command.bit.PreExcFlux = (Uint)pre[a];
                                    gMainCmd.FreqFeed = ffd[h];
                                    gMotorExtPer.R2 = (Uint)r2[e];
                                    gMotorExtPer.L1 = (Uint)l1[f];
                                    st.vcPar.VCWsCoff = (Uint)cof[g];

                                    set_imtset((long)mM[b], (long)mT[c]);
                                    set_imtset_apply((long)apM[d], (long)mT[c]);

                                    run(ref_CalWsAndSynFreq, new_CalWsAndSynFreq,
                                        "CalWsAndSynFreq");
                                    run(ref_CalWsAndSynFreq_380,
                                        new_CalWsAndSynFreq_380,
                                        "CalWsAndSynFreq_380");
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/* ========================================================================== */
/* Phase 8: PrepImCsrPara / VCCsrControl / VCCsrControl_380                   */
/* ========================================================================== */

/* Branches:
 *   PrepImCsrPara       : four straight assignments from VCPar and FcSetApply
 *   VCCsrControl        : |Amp| < maxVolt2 -> passthrough
 *                         else !PM       -> >>2 scaling
 *                         else PM        -> the d-axis clamp + qsqrt rebuild
 *                         (inside which `minVq` is hard-set to 100 and
 *                          gFluxWeak.DecoupleMode is hard-set to 1 -- both are
 *                          dead stores in the original; a rewrite that "cleans
 *                          them up" must still land on the same gUMTSet)
 *   VCCsrControl_380    : the same tail
 * VCCsrControl_380 has NO caller in the firmware (grep over src/ finds none).
 * It is driven directly anyway -- it is live code in the .lkf, so it is part of
 * the delivered image and has to be equivalent.
 */
static void phase_csr(void)
{
    static const int maxv[]  = {0, 100, 1000, 32767};
    static const int ty[]    = {2, 1};
    static const int uM[]    = {-30000, -1000, 0, 1000, 30000};
    static const int uT[]    = {-30000, 0, 30000};
    static const int fc[]    = {1, 100, 32767};
    static const int kp[]    = {0, 50, 1000};
    static const long force[] = {-1, 0, 7L << 12};
    int a, b, c, d, e, f, g;

    for (a = 0; a < 4; a++)
    {
        for (b = 0; b < 2; b++)
        {
            for (c = 0; c < 5; c++)
            {
                for (d = 0; d < 3; d++)
                {
                    for (e = 0; e < 3; e++)
                    {
                        for (f = 0; f < 3; f++)
                        {
                            for (g = 0; g < 3; g++)
                            {
                                baseline();
                                gOutVolt.MaxOutVolt = maxv[a];
                                gMotorInfo.MotorType = (Uint)ty[b];
                                gBasePar.FcSetApply = (Uint)fc[e];
                                st.vcPar.AcrImKp = (Uint)kp[f];
                                st.vcPar.AcrItKp = (Uint)kp[f];
                                gPmDecoup.RotVq = 200;
                                gFluxWeak.DecoupleMode = 0;
                                gPid32OutForce = force[g];

                                set_umtset(uM[c], uT[d]);
                                set_imtset(4096L, 4096L);
                                set_imtset_apply(4096L, 4096L);
                                set_acr_state(0L, 0L);

                                run(ref_PrepImCsrPara, new_PrepImCsrPara,
                                    "PrepImCsrPara");
                                run(ref_VCCsrControl, new_VCCsrControl,
                                    "VCCsrControl");
                                run(ref_VCCsrControl_380, new_VCCsrControl_380,
                                    "VCCsrControl_380");
                            }
                        }
                    }
                }
            }
        }
    }
}

/* ========================================================================== */
/* Phase 9: SvcCalOutVolt / SvcCalOutVolt_380                                 */
/* ========================================================================== */

/* Branches:
 *   FreqReal < 700 / > 1050       -> the two VoltSVCCalSignal latches
 *   VoltSVCCalSignal == 0         -> m_UMBak without the L0 term
 *   InvTypeApply > 12             -> m_MaxVolt 100 instead of 200
 *   FreqRealFilt vs m_MaxVolt(+120) -> VoltSVCCalSignalB latch
 *   RunStep / FreqSetApply gate   -> the PID branch or the zero branch
 *   InvTypeApply >= 18            -> the second gain pair
 *   |Amp| < 4916                  -> passthrough, else >>2
 *   RunStep not in {SPEED_CHECK, RUN} -> everything forced to zero
 * The _380 version additionally selects gIMTQ12 vs gIMTQ12_obs on TestData0
 * and scales by TestData4 / TestData5 -- driven with those two different.
 */
static void phase_svcvolt(void)
{
    static const int freal[] = {0, 699, 700, 701, 1049, 1050, 1051, 5000};
    static const int filt[]  = {0, 99, 100, 199, 200, 219, 220, 221, 5000};
    static const int itype[] = {0, 12, 13, 17, 18, 30};
    static const int step[]  = {3, 4, 5, 6};
    static const int fsa[]   = {-16384, 0, 16384};
    static const int td0[]   = {0, 1};
    static const int td45[]  = {0, 50, 100, 200};
    static const long force[] = {-1, 0, 9L << 16};
    int a, b, c, d, e, f, g, h;

    for (a = 0; a < 8; a++)
    {
        for (b = 0; b < 9; b++)
        {
            baseline();
            gMainCmd.FreqReal = freal[a];
            gMainCmd.FreqRealFilt = filt[b];
            gMainCmd.FreqSyn = 1000;
            set_imtset(4096000L, 8192000L);
            set_imtset_apply(4096000L, 8192000L);
            run(ref_SvcCalOutVolt, new_SvcCalOutVolt, "SvcCalOutVolt");
            run(ref_SvcCalOutVolt_380, new_SvcCalOutVolt_380,
                "SvcCalOutVolt_380");
        }
    }

    for (c = 0; c < 6; c++)
    {
        for (d = 0; d < 4; d++)
        {
            for (e = 0; e < 3; e++)
            {
                baseline();
                gInvInfo.InvTypeApply = (Uint)itype[c];
                gMainStatus.RunStep = (Uint)step[d];
                gMainCmd.FreqSetApply = fsa[e];
                gMainCmd.FreqReal = 800;
                gMainCmd.FreqRealFilt = 300;
                set_imtset(4096000L, 8192000L);
                set_imtset_apply(4096000L, 8192000L);
                run(ref_SvcCalOutVolt, new_SvcCalOutVolt, "SvcCalOutVolt");
                run(ref_SvcCalOutVolt_380, new_SvcCalOutVolt_380,
                    "SvcCalOutVolt_380");
            }
        }
    }

    for (f = 0; f < 2; f++)
    {
        for (g = 0; g < 4; g++)
        {
            for (d = 0; d < 4; d++)
            {
                baseline();
                gTestDataReceive.TestData0 = td0[f];
                gTestDataReceive.TestData4 = td45[g];
                gTestDataReceive.TestData5 = td45[g];
                gMainStatus.RunStep = (Uint)step[d];
                gMainCmd.FreqReal = 800;
                gMainCmd.FreqRealFilt = 300;
                gMainCmd.FreqSetApply = 1000;
                gMainCmd.FreqSyn = 1000;
                gIMTSetQ12.M = 1000;
                gIMTSetQ12.T = 2000;
                gIMTQ12.T = 100;
                gIMTQ12_obs.T = 900;
                gIMTQ12_obs.M = 900;
                gIMTQ24.M = 100;
                gIMTQ24.T = 100;
                set_imtset(4096000L, 8192000L);
                set_imtset_apply(4096000L, 8192000L);
                run(ref_SvcCalOutVolt, new_SvcCalOutVolt, "SvcCalOutVolt");
                run(ref_SvcCalOutVolt_380, new_SvcCalOutVolt_380,
                    "SvcCalOutVolt_380");
            }
        }
    }

    for (h = 0; h < 3; h++)
    {
        for (e = 0; e < 3; e++)
        {
            baseline();
            gPidOutForce = force[h];
            gMainCmd.FreqSetApply = fsa[e];
            gMainCmd.FreqReal = 800;
            gMainCmd.FreqRealFilt = 300;
            set_imtset(4096000L, 8192000L);
            set_imtset_apply(4096000L, 8192000L);
            run(ref_SvcCalOutVolt, new_SvcCalOutVolt, "SvcCalOutVolt");
        }
    }
}

/* ========================================================================== */
/* Phase 10: CalcABVolt                                                       */
/* ========================================================================== */

/* Branches: none -- two rotations and two __IQsat calls.  The interesting part
 * is the phase field: `m_Angle = (int)(gPhase.IMPhase>>16)`, so the low sixteen
 * bits of IMPhase are DEAD.  The sweep therefore puts the angle in the HIGH half
 * and fills the low half with a changing pattern: if a rewrite ever dropped the
 * shift, the two sides would still agree here (both see the same dead bits) --
 * that gap is listed in the case README, not hidden.
 */
static void phase_ab(void)
{
    static const int ang[] = {-32768, -16384, -1, 0, 1, 8192, 16384, 32767};
    static const int uM[]  = {-32767, -1000, 0, 1000, 32767};
    static const int uT[]  = {-32767, 0, 32767};
    int a, b, c;

    for (a = 0; a < 8; a++)
    {
        for (b = 0; b < 5; b++)
        {
            for (c = 0; c < 3; c++)
            {
                baseline();
                gPhase.IMPhase = ((long)(ang[a] & 0xFFFF) << 16) | 0x1234L;
                set_umtset(uM[b], uT[c]);
                run(ref_CalcABVolt, new_CalcABVolt, "CalcABVolt");
            }
        }
    }
}

/* ========================================================================== */
/* Phase 11: random combinations over every function                          */
/* ========================================================================== */

static void rand_inputs(void)
{
    static const int ty[]  = {2, 1};
    static const int ctr[] = {0, 1, 2, 10, 11, 12, 20, 21};

    baseline();

    gMotorInfo.MotorType   = (Uint)ty[rnd() % 2];
    gMotorInfo.Current     = (Uint)rnd_range(1, 40000);
    gMotorInfo.CurrentGet  = (Uint)rnd_range(0, 40000);
    gMotorInfo.FreqPer     = (Uint)rnd_range(1, 3000);
    gMotorInfo.Motor_HFreq = (Uint)rnd_range(0, 32767);
    gMotorInfo.Motor_LFreq = (Uint)rnd_range(0, 3000);

    gMotorExtPer.I0                = (Uint)rnd_range(0, 32767);
    gMotorExtPer.IoVsFreq          = (Uint)rnd_range(0, 32767);
    gMotorExtPer.FluxLeakCurveGain = (Uint)rnd_range(0, 400);
    gMotorExtPer.R1                = (Uint)rnd_range(0, 32767);
    gMotorExtPer.R2                = (Uint)rnd_range(0, 32767);
    gMotorExtPer.L0                = (Uint)rnd_range(0, 32767);
    gMotorExtPer.L1                = (Uint)rnd_range(1, 32767);

    gBasePar.FullFreq01 = (Ulong)rnd_range(1, 32767);
    gBasePar.FullFreq   = (Uint)rnd_range(1, 32767);
    gBasePar.FcSetApply = (Uint)rnd_range(1, 32767);

    gInvInfo.InvCurrent   = (Uint)rnd_range(1, 40000);
    gInvInfo.InvTypeApply = (Uint)rnd_range(0, 40);
    gInvInfo.InvCurrForP  = (Uint)rnd_range(0, 40000);

    gVFPar.SVCTorqueUpLim = (Uint)rnd_range(0, 32767);
    gVFPar.SVCTorqueUp    = (Uint)rnd_range(0, 1023);
    gVFPar.ovGain         = (Uint)rnd_range(0, 1000);

    st.vcPar.VCTorqueLim   = (Uint)rnd_range(0, 32767);
    st.vcPar.ASRKpHigh     = (Uint)rnd_range(0, 1023);
    st.vcPar.ASRKpLow      = (Uint)rnd_range(0, 1023);
    st.vcPar.ASRTIHigh     = (Uint)rnd_range(0, 1000);
    st.vcPar.ASRTILow      = (Uint)rnd_range(0, 1000);
    st.vcPar.ASRSwitchHigh = (Uint)rnd_range(0, 32767);
    st.vcPar.ASRSwitchLow  = (Uint)rnd_range(0, 32767);
    st.vcPar.SvcMode       = (Uint)rnd_range(0, 1);
    st.vcPar.AcrImKp       = (Uint)rnd_range(0, 1000);
    st.vcPar.AcrImKi       = (Uint)rnd_range(0, 1000);
    st.vcPar.AcrItKp       = (Uint)rnd_range(0, 1000);
    st.vcPar.AcrItKi       = (Uint)rnd_range(0, 1000);
    st.vcPar.VCWsCoff      = (Uint)rnd_range(0, 1000);

    gUDC.uDCFilter = (Uint)rnd_range(0, 5000);
    gOvUdc.Limit   = rnd_range(0, 5000);

    gPmDecoup.RotVd = rnd_range(-2000, 2000);
    gPmDecoup.RotVq = rnd_range(-2000, 2000);
    gFluxWeak.DecoupleMode = rnd_range(0, 1);

    gMainStatus.RunStep = (Uint)rnd_range(0, 8);
    gMainStatus.StatusWord.bit.OutOff = (Uint)rnd_range(0, 1);

    gCtrMotorType = (CONTROL_MOTOR_TYPE_ENUM)ctr[rnd() % 8];

    gMainCmd.Command.all = (Uint)rnd_range(0, 0x3FF);
    gMainCmd.FreqFeed     = rnd_range(-3000, 3000);
    gMainCmd.FreqSyn      = rnd_range(-3000, 3000);
    gMainCmd.FreqSetApply = rnd_range(-3000, 3000);
    gMainCmd.FreqDesired  = rnd_range(-3000, 3000);
    gMainCmd.FreqSynFilter = rnd_range(-3000, 3000);
    gMainCmd.FreqReal      = rnd_range(0, 3000);
    gMainCmd.FreqRealFilt  = rnd_range(0, 3000);

    gOutVolt.MaxOutVolt = rnd_range(0, 32767);

    gComPar.StartDCBrakeCur = (Uint)rnd_range(0, 1000);

    gTestDataReceive.TestData0 = rnd_range(0, 1);
    gTestDataReceive.TestData4 = rnd_range(0, 200);
    gTestDataReceive.TestData5 = rnd_range(0, 200);
    gTestDataReceive.TestData8 = rnd_range(0, 40);

    gIMTQ12.T     = rnd_range(-3000, 3000);
    gIMTSetQ12.M  = rnd_range(-3000, 3000);
    gIMTSetQ12.T  = rnd_range(-3000, 3000);
    gIMTQ12_obs.T = rnd_range(-3000, 3000);
    gIMTQ12_obs.M = rnd_range(-3000, 3000);
    gIMTQ24.M     = rnd_range(-3000, 3000);
    gIMTQ24.T     = rnd_range(-3000, 3000);

    gPhase.IMPhase = (long)(rnd() << 8) | (long)(rnd() & 0xFFFFUL);

    set_imtset((long)rnd_range(-2000, 2000) * 4096L,
               (long)rnd_range(-3000, 3000) * 4096L);
    set_imtset_apply((long)rnd_range(-2000, 2000) * 4096L,
                     (long)rnd_range(-3000, 3000) * 4096L);
    set_umtset(rnd_range(-32767, 32767), rnd_range(-32767, 32767));
    set_asr_state((long)rnd_range(-100000, 100000),
                  (long)rnd_range(-100000, 100000));
    set_udc_state(rnd_range(0, 6), rnd_range(0, 5000), rnd_range(0, 1),
                  (long)rnd_range(-100000, 100000));
    set_acr_state((long)rnd_range(-100000, 100000),
                  (long)rnd_range(-100000, 100000));

    gPidOutForce   = (rnd() & 1) ? (long)(rnd_range(0, 20) << 16) : -1;
    gPid32OutForce = (rnd() & 1) ? (long)(rnd_range(0, 20) << 12) : -1;
}

static void phase_random(long n)
{
    long i;

    for (i = 0; i < n; i++)
    {
        rand_inputs();
        run(ref_ResetParForVC,       new_ResetParForVC,       "ResetParForVC");
        run(ref_CalIMSet,            new_CalIMSet,            "CalIMSet");
        run(ref_CalTorqueLimitPar,   new_CalTorqueLimitPar,   "CalTorqueLimitPar");
        run(ref_PrepareAsrPar,       new_PrepareAsrPar,       "PrepareAsrPar");
        run(ref_CalUdcLimitIT,       new_CalUdcLimitIT,       "CalUdcLimitIT");
        run(ref_VcAsrControl,        new_VcAsrControl,        "VcAsrControl");
        run(ref_CalWsAndSynFreq,     new_CalWsAndSynFreq,     "CalWsAndSynFreq");
        run(ref_CalWsAndSynFreq_380, new_CalWsAndSynFreq_380, "CalWsAndSynFreq_380");
        run(ref_PrepImCsrPara,       new_PrepImCsrPara,       "PrepImCsrPara");
        run(ref_VCCsrControl,        new_VCCsrControl,        "VCCsrControl");
        run(ref_VCCsrControl_380,    new_VCCsrControl_380,    "VCCsrControl_380");
        run(ref_SvcCalOutVolt,       new_SvcCalOutVolt,       "SvcCalOutVolt");
        run(ref_SvcCalOutVolt_380,   new_SvcCalOutVolt_380,   "SvcCalOutVolt_380");
        run(ref_CalcABVolt,          new_CalcABVolt,          "CalcABVolt");
    }
}

/* ========================================================================== */
/* Branch coverage                                                            */
/* ========================================================================== */

/* A differential test that never reaches a branch cannot say anything about it,
 * and "0 mismatches" would then be read as "that branch is fine".  These
 * counters exist so that cannot happen silently: every one of them must be
 * non-zero on the baseline run, and a zero one makes the program exit 1 (see
 * main), i.e. the case fails even though no mismatch was found.
 *
 * HOW the classification is done, stated plainly: each counter is incremented
 * from the condition that DECIDES the branch, evaluated on the same inputs the
 * module saw, plus -- where the deciding value is an intermediate -- from the
 * result the branch produced (e.g. "gOutVolt.VoltApply == maxVolt2" proves the
 * scale-down branch ran, because the passthrough branch would have stored the
 * computed amplitude instead).  It is therefore reach evidence, not a
 * re-implementation: nothing here recomputes the module's arithmetic to decide
 * what "should" have happened.
 *
 * What this does NOT prove: that the branch was reached with a value that
 * exercises its edge.  That is what the negative control is for -- a mutation
 * inside a branch only shows up as CAUGHT if the bench actually drove it.
 */

enum
{
    COV_IM_PREEXC = 0,
    COV_IM_I0,
    COV_IM_CURVE,
    COV_IM_UP_ZERO,
    COV_IM_UP_ACTIVE,
    COV_TL_BASE1700,
    COV_TL_BASE1800,
    COV_TL_CLAMP,
    COV_TL_TORQ_DIV,
    COV_TL_SVC_BLOCK,
    COV_TL_STRAIGHT,
    COV_TL_PID,
    COV_TL_KP100,
    COV_ASR_PM,
    COV_ASR_ASYNC,
    COV_ASR_KI_CLAMP_HI,
    COV_ASR_KI_DIV_HI,
    COV_ASR_KI_CLAMP_LO,
    COV_ASR_KI_DIV_LO,
    COV_ASR_LOW,
    COV_ASR_HIGH,
    COV_ASR_INTERP,
    COV_ASR_KI_ZERO,
    COV_UDC_OFF,
    COV_UDC_CNT5,
    COV_UDC_DETA_SMALL,
    COV_UDC_SOFT,
    COV_UDC_LATCH,
    COV_UDC_PID,
    COV_VCA_PREEXC,
    COV_VCA_SVC_MODE1_DZ,
    COV_VCA_SVC_MODE1_BLEND,
    COV_VCA_SVC_MODE0_DZ,
    COV_VCA_PLAIN,
    COV_VCA_OUTOFF,
    COV_WS_PREEXC,
    COV_WS_NORMAL,
    COV_CSR_PREP,
    COV_CSR_PASS,
    COV_CSR_NONPM_SCALE,
    COV_CSR_PM_CLAMP,
    COV_SVC_SIG_SET,
    COV_SVC_SIG_CLR,
    COV_SVC_UMBAK_SIG0,
    COV_SVC_UMBAK_SIG1,
    COV_SVC_MAXV100,
    COV_SVC_MAXV200,
    COV_SVC_B_SET,
    COV_SVC_B_CLR,
    COV_SVC_ZERO,
    COV_SVC_PID,
    COV_SVC_GAIN18,
    COV_SVC_AMP_PASS,
    COV_SVC_AMP_SCALE,
    COV_AB_NORMAL,
    COV_COUNT
};

static const char *const g_covName[COV_COUNT] =
{
    "CalIMSet PreExcFlux",
    "CalIMSet below FreqPer",
    "CalIMSet flux-leak curve",
    "CalIMSet SVCTorqueUp==0",
    "CalIMSet SVCTorqueUp!=0",
    "CalTorqueLimitPar 1700 base",
    "CalTorqueLimitPar 1800 base",
    "CalTorqueLimitPar clamp",
    "CalTorqueLimitPar torque divide",
    "CalTorqueLimitPar SVC block",
    "CalTorqueLimitPar straight",
    "CalTorqueLimitPar PID",
    "CalTorqueLimitPar Kp/Ki 100",
    "PrepareAsrPar PM",
    "PrepareAsrPar non-PM",
    "PrepareAsrPar KIHigh clamp",
    "PrepareAsrPar KIHigh divide",
    "PrepareAsrPar KILow clamp",
    "PrepareAsrPar KILow divide",
    "PrepareAsrPar SwitchLow side",
    "PrepareAsrPar SwitchHigh side",
    "PrepareAsrPar interpolation",
    "PrepareAsrPar KI=0 gate",
    "CalUdcLimitIT ovGain==0",
    "CalUdcLimitIT sample at 5",
    "CalUdcLimitIT Deta<200",
    "CalUdcLimitIT soft knee",
    "CalUdcLimitIT latch",
    "CalUdcLimitIT PID",
    "VcAsrControl pre-excitation",
    "VcAsrControl SVC mode1 dead zone",
    "VcAsrControl SVC mode1 blend",
    "VcAsrControl SVC mode0 dead zone",
    "VcAsrControl plain copy",
    "VcAsrControl OutOff reset",
    "CalWsAndSynFreq pre-excitation",
    "CalWsAndSynFreq normal",
    "PrepImCsrPara",
    "VCCsrControl passthrough",
    "VCCsrControl non-PM scale",
    "VCCsrControl PM clamp",
    "SvcCalOutVolt signal set",
    "SvcCalOutVolt signal clear",
    "SvcCalOutVolt UMBak signal 0",
    "SvcCalOutVolt UMBak signal 1",
    "SvcCalOutVolt MaxVolt 100",
    "SvcCalOutVolt MaxVolt 200",
    "SvcCalOutVolt B set",
    "SvcCalOutVolt B clear",
    "SvcCalOutVolt zero branch",
    "SvcCalOutVolt PID branch",
    "SvcCalOutVolt gain >=18",
    "SvcCalOutVolt amp passthrough",
    "SvcCalOutVolt amp scale",
    "CalcABVolt"
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

/* Classify what the ref side just did.  `a` is the ref image, `st` the state the
 * case started from, and the shared inputs are still untouched (world_load has
 * not run yet), so both are exactly what the module saw. */
static void cov_observe(const char *fn, const IMG *a)
{
    if (0 == strcmp(fn, "CalIMSet"))
    {
        if (gMainCmd.Command.bit.PreExcFlux)
        {
            hit(COV_IM_PREEXC);
        }
        else if (iabs(gMainCmd.FreqSyn) < (int)gMotorInfo.FreqPer)
        {
            hit(COV_IM_I0);
        }
        else
        {
            hit(COV_IM_CURVE);
        }
        if (gVFPar.SVCTorqueUp == 0)
        {
            hit(COV_IM_UP_ZERO);
        }
        else
        {
            hit(COV_IM_UP_ACTIVE);
        }
        return;
    }

    if (0 == strcmp(fn, "CalTorqueLimitPar"))
    {
        if (gInvInfo.InvTypeApply == 15) hit(COV_TL_BASE1700);
        else                             hit(COV_TL_BASE1800);

        /* the clamp leaves gAsr.TorqueLimit at the pre-scaled limit, the other
         * path always rewrites it; both are compared, this only classifies */
        if (st.vcPar.VCTorqueLim > 1700)   hit(COV_TL_CLAMP);

        if ((1 == gMainCmd.Command.bit.TorqueCtl)
            && (gMotorInfo.MotorType != MOTOR_TYPE_PM)
            && (iabs(gMainCmd.FreqSyn) >= (int)gMotorInfo.FreqPer))
        {
            hit(COV_TL_TORQ_DIV);
        }
        if ((gMainCmd.Command.bit.ControlMode == IDC_SVC_CTL)
            && (gMotorInfo.MotorType != MOTOR_TYPE_PM)
            && (0 == st.vcPar.SvcMode))
        {
            hit(COV_TL_SVC_BLOCK);
        }
        if ((1 == gMainCmd.Command.bit.TorqueCtl)
            || (IDC_FVC_CTL == gMainCmd.Command.bit.ControlMode))
        {
            hit(COV_TL_STRAIGHT);
        }
        else
        {
            hit(COV_TL_PID);
        }
        if (gInvInfo.InvTypeApply > 20) hit(COV_TL_KP100);
        return;
    }

    if (0 == strcmp(fn, "PrepareAsrPar"))
    {
        if (MOTOR_TYPE_PM == gMotorInfo.MotorType) hit(COV_ASR_PM);
        else                                      hit(COV_ASR_ASYNC);

        if ((int)(st.vcPar.ASRKpHigh >> 5) >= (int)st.vcPar.ASRTIHigh) hit(COV_ASR_KI_CLAMP_HI);
        else                                                       hit(COV_ASR_KI_DIV_HI);
        if ((int)(st.vcPar.ASRKpLow >> 5) >= (int)st.vcPar.ASRTILow)   hit(COV_ASR_KI_CLAMP_LO);
        else                                                      hit(COV_ASR_KI_DIV_LO);

        /* The switch is on |FreqSyn| against SwitchLow / SwitchHigh, and both
         * thresholds come straight from the inputs, so this is the deciding
         * expression itself -- not an inference from the result. */
        {
            int f = iabs(gMainCmd.FreqSyn);
            int lo = a->asr.SwitchLow;
            int hi = a->asr.SwitchHigh;

            if (f <= lo)      hit(COV_ASR_LOW);
            else if (f >= hi) hit(COV_ASR_HIGH);
            else              hit(COV_ASR_INTERP);
        }
        if ((0 == gMainCmd.Command.bit.TorqueCtl)
            && (1 == gMainCmd.Command.bit.IntegralDiscrete))
        {
            hit(COV_ASR_KI_ZERO);
        }
        return;
    }

    if (0 == strcmp(fn, "CalUdcLimitIT"))
    {
        if (0 == gVFPar.ovGain)
        {
            hit(COV_UDC_OFF);
            return;
        }
        /* The three inner branches are distinguished by the result: the soft
         * knee latches UDCLimit to the torque limit, the FirstOvUdcFlag branch
         * latches it to UdcPid.Total>>16, the PID branch to its own output. */
        if (a->udcLimitIt.UDCBakCnt == 0 && st.udcBakCnt >= 4) hit(COV_UDC_CNT5);
        if (a->udcLimitIt.UDCDeta == 0 && st.udcBak != 0)      hit(COV_UDC_DETA_SMALL);
        if (a->udcLimitIt.UDCLimit == (int)(a->asr.TorqueLimit)) hit(COV_UDC_SOFT);
        else if (a->udcLimitIt.FirstOvUdcFlag == 1 && st.udcFirst == 0) hit(COV_UDC_LATCH);
        else                                                   hit(COV_UDC_PID);
        return;
    }

    if (0 == strcmp(fn, "VcAsrControl"))
    {
        if ((1 == gMainCmd.Command.bit.PreExcFlux)
            && (MOTOR_TYPE_PM != gMotorInfo.MotorType))
        {
            hit(COV_VCA_PREEXC);
            return;
        }
        if (gCtrMotorType == ASYNC_SVC)
        {
            if (1 == st.vcPar.SvcMode)
            {
                if (0 == a->imtSet.T) hit(COV_VCA_SVC_MODE1_DZ);
                else                  hit(COV_VCA_SVC_MODE1_BLEND);
            }
            else
            {
                if (0 == a->imtSet.T) hit(COV_VCA_SVC_MODE0_DZ);
            }
        }
        else
        {
            hit(COV_VCA_PLAIN);
        }
        if (1 == gMainStatus.StatusWord.bit.OutOff) hit(COV_VCA_OUTOFF);
        return;
    }

    if ((0 == strcmp(fn, "CalWsAndSynFreq")) || (0 == strcmp(fn, "CalWsAndSynFreq_380")))
    {
        if (1 == gMainCmd.Command.bit.PreExcFlux) hit(COV_WS_PREEXC);
        else                                      hit(COV_WS_NORMAL);
        return;
    }

    if (0 == strcmp(fn, "PrepImCsrPara"))
    {
        hit(COV_CSR_PREP);
        return;
    }

    if ((0 == strcmp(fn, "VCCsrControl")) || (0 == strcmp(fn, "VCCsrControl_380")))
    {
        int maxVolt2 = gOutVolt.MaxOutVolt + 500;

        if (a->uAmpTheta.Amp < maxVolt2 || a->uAmpTheta.Amp < 4916)
        {
            hit(COV_CSR_PASS);
        }
        else if (gMotorInfo.MotorType != MOTOR_TYPE_PM)
        {
            hit(COV_CSR_NONPM_SCALE);
        }
        else
        {
            hit(COV_CSR_PM_CLAMP);
        }
        return;
    }

    if ((0 == strcmp(fn, "SvcCalOutVolt")) || (0 == strcmp(fn, "SvcCalOutVolt_380")))
    {
        if (a->svc0Signal.VoltSVCCalSignal == 1) hit(COV_SVC_SIG_SET);
        else                                     hit(COV_SVC_SIG_CLR);

        if (a->svc0Signal.VoltSVCCalSignalB == 1) hit(COV_SVC_B_SET);
        else                                      hit(COV_SVC_B_CLR);

        if (gInvInfo.InvTypeApply > 12) hit(COV_SVC_MAXV100);
        else                            hit(COV_SVC_MAXV200);
        if (gInvInfo.InvTypeApply >= 18) hit(COV_SVC_GAIN18);

        if (0 == a->umtSet.M && 0 == a->umtSet.T) hit(COV_SVC_ZERO);
        else if (a->uAmpTheta.Amp < 4916)         hit(COV_SVC_AMP_PASS);
        else                                      hit(COV_SVC_AMP_SCALE);

        if (gMainCmd.FreqReal < 700)         hit(COV_SVC_UMBAK_SIG1);
        else if (gMainCmd.FreqReal > 1050)   hit(COV_SVC_UMBAK_SIG0);

        if (a->svcVoltAdj.KP != 0) hit(COV_SVC_PID);
        return;
    }

    if (0 == strcmp(fn, "CalcABVolt"))
    {
        hit(COV_AB_NORMAL);
        return;
    }
}

/* ========================================================================== */

/* ========================================================================== */
/* Phase 12: boundary hunting                                                 */
/* ========================================================================== */

/* The first negative-control pass left 60 of 306 MISSED.  Groups A and B (the
 * observation surface and the state inputs) are fixed in IMG / state_apply;
 * everything else lives here, and each block names the mutation it exists for.
 *
 * baseline() pins FullFreq01 = 32768, so the two ASR dead-zone thresholds are
 * exactly m03HzPu = 30 and m05HzPu = 50 -- the sweeps walk 29/30/31 and
 * 49/50/51, i.e. from both sides of the equality point.
 */
static void phase_boundary(void)
{
    /* SV3/SV28/VA1/VA2/VA14/VA15/VA22/VA23: the dead-zone equality points. */
    static const int fsa[] = {-51, -50, -49, -31, -30, -29, 0,
                              29, 30, 31, 49, 50, 51, 1000, 16384};
    static const int ffd[] = {-51, -50, -49, -31, -30, -29, 0,
                              29, 30, 31, 49, 50, 51, 1000};
    /* SV33: the `InvTypeApply >= 18` equality point, with the SvcVoltAdj branch
     * actually reached. */
    static const int itype[] = {17, 18, 19, 20, 21};
    /* SV5/SV19/SV21/SV23: the two voltage limits' equality points. */
    static const int freal[] = {699, 700, 701, 1049, 1050, 1051};
    static const int filt[] = {99, 100, 101, 199, 200, 201,
                               219, 220, 221, 319, 320, 321};
    static const int cal[] = {0, 1};
    static const int calb[] = {0, 1};
    /* SV19 clamps m_MaxVolt to 200 only when InvTypeApply <= 12; pinning 18
     * made the 201 mutation invisible. */
    static const int ity2[] = {0, 18};
    /* VA14/VA15 live in the SvcMode == 1 dead zone; SvcMode was pinned to 0. */
    static const int svcm[] = {0, 1};
    /* IM3/IM14: the relative size of the two clamp arguments, and
     * SVCTorqueUp taking 0 / 1 / 2. */
    static const int mhf[] = {100, 800, 2000};
    static const int lim[] = {0, 800, 1500};
    static const int up[] = {0, 1, 2};
    /* IM14 needs m_MaxI0Freq >= 1024 for the 1025/1024 scaling to survive the
     * shift; baseline() pins I0 = 300, which halves to 150. */
    static const int i0v[] = {300, 2000, 5000};
    /* D: R1 values that put (R1*(Q>>12))>>16 on 4884 and 560. */
    static const int r1amp[] = {36690, 36695, 36700, 36702, 36705, 36710};
    int a, b, c, d, e, f;

    /* A1: dead-zone thresholds x InvTypeApply x the SVC0Signal inputs. */
    for (a = 0; a < 15; a++)
    {
        for (b = 0; b < 14; b++)
        {
            for (c = 0; c < 5; c++)
            {
                for (d = 0; d < 2; d++)
                {
                    for (e = 0; e < 2; e++)
                    {
                        for (f = 0; f < 2; f++)
                        {
                        baseline();
                        gMainCmd.FreqSetApply = fsa[a];
                        gMainCmd.FreqFeed = ffd[b];
                        gMainCmd.FreqReal = 800;
                        gMainCmd.FreqRealFilt = 0;
                        gMainStatus.RunStep = STATUS_RUN;
                        gInvInfo.InvTypeApply = (Uint)itype[c];
                        st.vcPar.SvcMode = (Uint)svcm[f];
                        st.svcCal = (Uint)cal[d];
                        st.svcCalB = (Uint)calb[e];
                        st.udcLimit = 900;
                        set_imtset(4096000L, 8192000L);
                        set_imtset_apply(4096000L, 8192000L);
                        set_asr_state(100L, 200L);
                        run(ref_SvcCalOutVolt, new_SvcCalOutVolt,
                            "SvcCalOutVolt");
                        run(ref_VcAsrControl, new_VcAsrControl,
                            "VcAsrControl");
                        }
                    }
                }
            }
        }
    }

    /* A2: the FreqReal / FreqRealFilt limits, with the SVC0Signal inputs. */
    for (a = 0; a < 6; a++)
    {
        for (b = 0; b < 12; b++)
        {
            for (d = 0; d < 2; d++)
            {
                for (e = 0; e < 2; e++)
                {
                    for (c = 0; c < 2; c++)
                    {
                    baseline();
                    gMainCmd.FreqReal = freal[a];
                    gMainCmd.FreqRealFilt = filt[b];
                    gMainCmd.FreqSetApply = 1000;
                    gMainCmd.FreqSyn = 1000;
                    gInvInfo.InvTypeApply = (Uint)ity2[c];
                    st.svcCal = (Uint)cal[d];
                    st.svcCalB = (Uint)calb[e];
                    set_imtset(4096000L, 8192000L);
                    set_imtset_apply(4096000L, 8192000L);
                    run(ref_SvcCalOutVolt, new_SvcCalOutVolt, "SvcCalOutVolt");
                    }
                }
            }
        }
    }

    /* B: the two clamps in CalIMSet, and the `SVCTorqueUp == 0` early return. */
    for (a = 0; a < 3; a++)
    {
        for (b = 0; b < 3; b++)
        {
            for (c = 0; c < 3; c++)
            {
                for (e = 0; e < 3; e++)
                {
                    baseline();
                    gMotorInfo.Motor_HFreq = (Uint)mhf[a];
                    gMotorInfo.Motor_LFreq = 200;
                    gMotorExtPer.I0 = (Uint)i0v[e];
                    gMotorExtPer.FluxLeakCurveGain = 100;
                    gVFPar.SVCTorqueUpLim = (Uint)lim[b];
                    gVFPar.SVCTorqueUp = (Uint)up[c];
                    gMainCmd.FreqSyn = 1000;
                    run(ref_CalIMSet, new_CalIMSet, "CalIMSet");
                }
            }
        }
    }

    /* C: the four clamps in CalTorqueLimitPar.
     *   outer test |gIMTSet.T>>12| >= TorqueLimit sits on equality (TL15);
     *   inner test |gIMTQ12.T| + 2048 < TorqueLimit sits on equality (TL17),
     *   with gIMTQ12.M different from gIMTQ12.T (TL16);
     *   m_TorLimit must exceed 1200 / 1000 to reach those clamps (TL14/TL18);
     *   m_InvIM walks 1..300 so (x*1000)>>12 crosses a multiple of 4096 and
     *   parts company with (x*999)>>12 -- the first split is at x = 41 (TL2). */
    for (a = 0; a < 12; a++)
    {
        for (b = 0; b < 3; b++)
        {
            baseline();
            gMainCmd.Command.bit.ControlMode = IDC_SVC_CTL;
            gMainCmd.Command.bit.TorqueCtl = 0;
            gMotorInfo.MotorType = 1;
            st.vcPar.SvcMode = 0;
            st.vcPar.VCTorqueLim = 20000;
            gMainCmd.FreqFeed = 0;
            gMainCmd.FreqSyn = 0;
            /* a = 0 -> m_InvIM = 0, so m_MaxLimit = 1800 and the 1200 clamp
             *          fires (TL14/TL18 read the value that gets stored).
             * a > 0 -> m_InvIM lands near 1800, where qsqrt(1800^2 - m^2) is
             *          steep enough for the (x*1000)>>12 vs (x*999)>>12 split
             *          to survive (TL2); 7373 is the first x that splits. */
            set_imtset((0 == a) ? (1L << 12) : (((long)(7369 + a)) << 12),
                       3000L << 12);
            st.torqueLimit = 3000;
            gIMTQ12.M = 5000;
            /* 900 -> the inner clamp fires (TL15/TL16/TL18)
             * 952 -> |T| + 2048 == TorqueLimit (TL17)
             * 953 -> just past it                                          */
            gIMTQ12.T = (0 == b) ? 900 : ((1 == b) ? 952 : 953);
            run(ref_CalTorqueLimitPar, new_CalTorqueLimitPar,
                "CalTorqueLimitPar");
        }
    }

    /* D: the `Amp == 4916` equality point in SvcCalOutVolt / _380.
     *   (4884, 560) is a Pythagorean decomposition of 4916 (4916 = 4*1229 and
     *   1229 = 1221^2 + 140^2 is a triple), so both M and T are non-zero.
     *   T has to be non-zero: "over the limit, halve M and T" only changes
     *   atan() -- and therefore VoltPhaseApply -- when both are non-zero.
     *   R1 = 36702 lands (R1*(Q>>12))>>16 on 4884 and 560. */
    for (a = 0; a < 6; a++)
    {
        baseline();
        gMotorExtPer.R1 = (Uint)r1amp[a];
        gMainCmd.FreqSyn = 0;
        gMainCmd.FreqSetApply = 0;
        gMainCmd.FreqRealFilt = 0;
        gMainStatus.RunStep = STATUS_RUN;
        set_imtset(8721L << 12, 1000L << 12);
        set_imtset_apply(8721L << 12, 1000L << 12);
        gIMTSetQ12.M = 8721;
        gIMTSetQ12.T = 1000;
        gTestDataReceive.TestData4 = 0;
        gTestDataReceive.TestData5 = 0;
        run(ref_SvcCalOutVolt, new_SvcCalOutVolt, "SvcCalOutVolt");
        run(ref_SvcCalOutVolt_380, new_SvcCalOutVolt_380,
            "SvcCalOutVolt_380");
    }

    /* E: `Amp == maxVolt2` in VCCsrControl (VC10) and the q-axis sign test at
     *    gUMTSet.T == 0 (VC20).  The two PID32 calls are pinned to 1500<<12 and
     *    0, so M = 1500 = MaxOutVolt + 500 and T = 0. */
    for (c = 0; c < 2; c++)
    {
        baseline();
        gOutVolt.MaxOutVolt = 1000;
        gMotorInfo.MotorType = (0 == c) ? MOTOR_TYPE_PM : 1;
        gPid32OutForce = 1500L << 12;
        gPid32Force2 = 0;
        run(ref_VCCsrControl, new_VCCsrControl, "VCCsrControl");
    }

    /* F: VCCsrControl_380 reading obs or non-obs (V314), plus its own Amp
     *    equality point (V316).  gIMTQ24.M and gIMTQ24_obs.M differ. */
    for (c = 0; c < 2; c++)
    {
        baseline();
        gOutVolt.MaxOutVolt = 1000;
        /* Both axes pinned to 0: gUMTSet.M/T then come straight from
         * m_UMBak/m_UTBak, which block D above drove to (4884, 560). */
        gPid32OutForce = 0;
        gPid32Force2 = 0;
        gIMTQ24.M = 700;
        gIMTQ24_obs.M = (0 == c) ? 700 : 1900;
        gIMTQ24.T = 0;
        gIMTQ24_obs.T = 0;
        set_imtset_apply(4096000L, 8192000L);
        gIMTSetQ12.M = 8721;
        gIMTSetQ12.T = 1000;
        gMotorExtPer.R1 = 36702;
        run(ref_VCCsrControl_380, new_VCCsrControl_380, "VCCsrControl_380");
    }

    /* F2: V316 needs Amp == 4916 AND a pair whose `>> 2` does not cancel in
     *     the atan ratio.  See build/_vc_driver_patch5.py for the arithmetic.
     *     M = 3476, T = 3477, 3476^2 + 3477^2 = 24173205, qsqrt -> 4916.
     *     m_UMBak == m_UTBak == 0 because gIMTSetQ12 is zero and FreqSyn is
     *     zero, so the two axes land exactly on the two pinned PID32 outputs. */
    baseline();
    gOutVolt.MaxOutVolt = 1000;
    gIMTSetQ12.M = 0;
    gIMTSetQ12.T = 0;
    gMainCmd.FreqSyn = 0;
    gIMTQ24_obs.M = 0;
    gIMTQ24_obs.T = 0;
    gPid32OutForce = 3476L << 12;
    gPid32Force2   = 3477L << 12;
    run(ref_VCCsrControl_380, new_VCCsrControl_380, "VCCsrControl_380");

    /* UD6: `UDCDeta < 200` -> `< 201` differs only at UDCDeta == 200, which is
     *   computed inside the `UDCBakCnt >= 5` divider.  UDCBakCnt must already be
     *   4 so the divider fires on this very call. */
    for (a = 0; a < 3; a++)
    {
        baseline();
        st.udcBakCnt = 4;
        st.udcBak = 1000;
        st.udcDeta = 0;
        gUDC.uDCFilter = (0 == a) ? 1200 : ((1 == a) ? 1199 : 1201);
        gOvUdc.Limit = 4000;
        st.torqueLimit = 900;
        run(ref_CalUdcLimitIT, new_CalUdcLimitIT, "CalUdcLimitIT");
    }

    /* G: a non-zero UDCLimit makes the sign in `Asr.Min = -Asr.Max` visible
     *    (VA8). */
    for (f = 0; f < 3; f++)
    {
        baseline();
        st.udcLimit = (0 == f) ? 0 : ((1 == f) ? 900 : -900);
        set_asr_state(100L, 200L);
        run(ref_VcAsrControl, new_VcAsrControl, "VcAsrControl");
    }

    /* H: the interpolation numerator in PrepareAsrPar (PA19, `+ 1`).
     *   gAsr.Asr.KP = (DetaKP * FreqUp) / DetaFreq + KPLow.  Adding 1 to the
     *   numerator only changes the quotient when
     *        DetaKP * FreqUp == DetaFreq - 1   (mod DetaFreq),
     *   a measure-zero set, which is why a random sweep never hits it.
     *   FullFreq01 = 32768 makes SwitchHigh/Low equal to the raw parameters,
     *   so ASRSwitchHigh - ASRSwitchLow is DetaFreq directly.  With
     *   DetaFreq = 127 and DetaKP = 126<<7 = 16128 (FreqUp = 1) the residue is
     *   exactly 126: 16128/127 = 126 remainder 126, and 16129/127 = 127. */
    for (a = 0; a < 2; a++)
    {
        baseline();
        /* SwitchHigh/Low divide by FullFreq, NOT by FullFreq01 -- setting only
         * FullFreq01 leaves the switch points at (param<<15)/100, which wraps
         * through Uint and puts DetaFreq nowhere near 127. */
        gBasePar.FullFreq = 32768;
        gBasePar.FullFreq01 = 32768L;
        gMotorInfo.MotorType = MOTOR_TYPE_PM;
        st.vcPar.ASRKpHigh = 126;
        st.vcPar.ASRKpLow = 0;
        st.vcPar.ASRTIHigh = 100;
        st.vcPar.ASRTILow = 100;
        st.vcPar.ASRSwitchLow = (0 == a) ? 1000 : 5000;
        st.vcPar.ASRSwitchHigh = (Uint)(st.vcPar.ASRSwitchLow + 127);
        gMainCmd.FreqSyn = (int)st.vcPar.ASRSwitchLow + 1;
        run(ref_PrepareAsrPar, new_PrepareAsrPar, "PrepareAsrPar");
    }

    /* I: the __IQsat limits on tempKp / tempKi (PA24/PA25).
     *   tempKp = (Asr.KP * FullFreq01 / 10) >> 12 reaches 32767 only when
     *   Asr.KP * FullFreq01 >= 32767 * 4096 * 10.  Asr.KP tops out at
     *   ASRKpHigh<<7 = 32640 (255 is the largest shiftable value that still
     *   fits int), so the base frequency has to exceed ~411 Hz -- 420 Hz here.
     *   FullFreq = 32768 makes SwitchHigh = ASRSwitchHigh = 32767, so
     *   |FreqSyn| = 32767 takes the KPHigh branch with no interpolation. */
    baseline();
    gBasePar.FullFreq = 32768;
    gBasePar.FullFreq01 = 42000L;
    gMotorInfo.MotorType = MOTOR_TYPE_PM;
    st.vcPar.ASRKpHigh = 255;
    st.vcPar.ASRKpLow = 0;
    st.vcPar.ASRTIHigh = 1;
    st.vcPar.ASRTILow = 1;
    st.vcPar.ASRSwitchHigh = 32767;
    st.vcPar.ASRSwitchLow = 0;
    gMainCmd.FreqSyn = 32767;
    run(ref_PrepareAsrPar, new_PrepareAsrPar, "PrepareAsrPar");
}

int main(void)
{
    long base;
    int i;
    int covZero = 0;

    base = g_cases;
    phase_reset();
    printf("  ResetParForVC/AsrPar/CsrPar : %ld\n", g_cases - base);

    base = g_cases;
    phase_imset();
    printf("  CalIMSet                    : %ld\n", g_cases - base);

    base = g_cases;
    phase_torquelim();
    printf("  CalTorqueLimitPar           : %ld\n", g_cases - base);

    base = g_cases;
    phase_asrpar();
    printf("  PrepareAsrPar               : %ld\n", g_cases - base);

    base = g_cases;
    phase_udclimit();
    printf("  CalUdcLimitIT               : %ld\n", g_cases - base);

    base = g_cases;
    phase_vcasr();
    printf("  VcAsrControl                : %ld\n", g_cases - base);

    base = g_cases;
    phase_ws();
    printf("  CalWsAndSynFreq(+380)       : %ld\n", g_cases - base);

    base = g_cases;
    phase_csr();
    printf("  PrepImCsrPara/VCCsrControl  : %ld\n", g_cases - base);

    base = g_cases;
    phase_svcvolt();
    printf("  SvcCalOutVolt(+380)         : %ld\n", g_cases - base);

    base = g_cases;
    phase_ab();
    printf("  CalcABVolt                  : %ld\n", g_cases - base);

    base = g_cases;
    phase_boundary();
    printf("  boundary hunting            : %ld\n", g_cases - base);

    base = g_cases;
    phase_random(2000L);
    printf("  random                      : %ld\n", g_cases - base);

    /* Coverage gate: a branch that was never reached cannot be reported on, so
     * a zero counter fails the run even with zero mismatches. */
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

    printf("MotorVCMain difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    return (g_fails || covZero) ? 1 : 0;
}
