/* Differential-test driver for MotorPmsmParEst.
 *
 * The frozen baseline (cases/MotorPmsmParEst/ref/MotorPmsmParEst_ref.c) and the
 * delivered source (src/motor/MotorPmsmParEst.c) are compiled into two symbol
 * namespaces, ref_* and new_*. Both are handed identical inputs and every
 * observable is compared after every call.
 *
 * ---------------------------------------------------------------------------
 * Shape of the module
 * ---------------------------------------------------------------------------
 * Thirteen entry points, no parameters worth speaking of, all state in globals:
 *
 *   InitSetPosTune          SynTuneInitPos        SynInitPosDetect
 *   SetIPMPos(Uint)         SetIPMPos_ABZRef(Uint)
 *   SynCalLabAndLbc         SynInitPosDetCal      SynInitPosDetSetTs
 *   SynInitPosDetSetPwm(Uint)
 *   DetectZeroPosOnce       SynTunePGZero_No_Load SynTunePGZero_Load
 *   SynTuneBemf
 *
 * Four globals are *owned* by the module (gIPMZero / gIPMInitPos / gEstBemf /
 * gPmParEst). Because ref_tu.c / new_tu.c rename every externally-linked
 * file-scope symbol, each side gets its own physical copy; the driver keeps
 * them in `struct PRIV` and syncs ref -> new before each scenario.
 *
 * Everything else the module touches -- peripherals and other modules' globals
 * -- exists once (`struct SHARED`). do_step() snapshots that image, runs ref,
 * snapshots again, *restores the input image*, runs new, and compares. Without
 * the restore the ref side's writes would leak into the new side and mask a
 * difference.
 *
 * ---------------------------------------------------------------------------
 * Turning constants into inputs
 * ---------------------------------------------------------------------------
 * Several branches are gated on a field another module would compute:
 *
 *     gIPMInitPos.Cur[]  / Ld / Lq   SynCalLdAndLq() (MotorPmsmMain.c) and
 *                                    IPMCalAcrPIDCoff() (MotorPmsmMain.c) would
 *                                    write Ld/Lq and gPmParEst.IdKp. Both are
 *                                    callees we cannot model per-side (see
 *                                    mock_pmsmparest.c group 3), so the driver
 *                                    pokes the fields directly. Strictly more
 *                                    coverage, not less.
 *     gMockAbDir/UvwDir/RtDir        the three Judge*Direction() results; the
 *                                    driver pokes them so every DIR_ERROR /
 *                                    DIR_FORWARD / DIR_BACKWARD arm is reached.
 *     gPGData.PGMode / PGType        the encoder mode; poked to sweep both the
 *                                    ABZ and the resolver branches.
 *     gUVWPG.*                       the UVW phase table state that
 *                                    DetectZeroPosOnce / SynTunePGZero_* read.
 *
 * ---------------------------------------------------------------------------
 * Division-by-zero is an input-space hazard, not a style point
 * ---------------------------------------------------------------------------
 * Five divisions have a variable divisor:
 *
 *   SetIPMPos / SetIPMPos_ABZRef        / gMotorExtInfo.Poles
 *   SynCalLabAndLbc                     / ((Ulong)m_DetaI * 100)
 *   SynTunePGZero_No_Load case 1        / gMotorInfo.Current   (unguarded)
 *   SynTuneBemf case 2 / case 6         / gEstBemf.AccDecTick
 *   SynTuneBemf case 8                  / temp3
 *
 * (SynInitPosDetect case 1 divides by gMotorInfo.Current too, but only inside
 * `if(gInvInfo.InvCurrent < gMotorInfo.Current)`, which already forces it > 0.)
 *
 * A zero there does not produce a "mismatch", it kills the process, which would
 * silently destroy the whole ledger. pin_divisors() therefore keeps every one
 * of them non-zero, on *every* scenario, after the phase's pokes and before
 * sync_priv(). See its comment for the arithmetic it replicates.
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

/* <stdlib.h> is deliberately NOT included: the case shim folds `int` to the
 * target's 16 bits and stdlib's 32-bit `int abs(int)` would collide with the
 * project's folded 16-bit prototype in SubPrgInclude.h. Only atol is needed. */
extern long atol(const char *);

#include "MotorInclude.h"
#include "mock_calls.h"

/* ---- renamed entry points ------------------------------------------------ */

extern void ref_InitSetPosTune(void);
extern void ref_SynTuneInitPos(void);
extern void ref_SynInitPosDetect(void);
extern void ref_SetIPMPos(Uint Pos);
extern void ref_SetIPMPos_ABZRef(Uint Pos);
extern void ref_SynCalLabAndLbc(void);
extern void ref_SynInitPosDetCal(void);
extern void ref_SynInitPosDetSetTs(void);
extern void ref_SynInitPosDetSetPwm(Uint Section);
extern void ref_DetectZeroPosOnce(void);
extern void ref_SynTunePGZero_No_Load(void);
extern void ref_SynTunePGZero_Load(void);
extern void ref_SynTuneBemf(void);

extern void new_InitSetPosTune(void);
extern void new_SynTuneInitPos(void);
extern void new_SynInitPosDetect(void);
extern void new_SetIPMPos(Uint Pos);
extern void new_SetIPMPos_ABZRef(Uint Pos);
extern void new_SynCalLabAndLbc(void);
extern void new_SynInitPosDetCal(void);
extern void new_SynInitPosDetSetTs(void);
extern void new_SynInitPosDetSetPwm(Uint Section);
extern void new_DetectZeroPosOnce(void);
extern void new_SynTunePGZero_No_Load(void);
extern void new_SynTunePGZero_Load(void);
extern void new_SynTuneBemf(void);

/* ---- private per-side state (owned by the module) ------------------------ */

extern IPM_ZERO_POS_STRUCT    ref_gIPMZero;
extern IPM_INITPOS_PULSE_STR  ref_gIPMInitPos;
extern PMSM_EST_BEMF          ref_gEstBemf;
extern PMSM_EST_PARAM_DATA    ref_gPmParEst;

extern IPM_ZERO_POS_STRUCT    new_gIPMZero;
extern IPM_INITPOS_PULSE_STR  new_gIPMInitPos;
extern PMSM_EST_BEMF          new_gEstBemf;
extern PMSM_EST_PARAM_DATA    new_gPmParEst;

/* ---- mock-owned ---------------------------------------------------------- */

extern volatile struct GPIO_DATA_REGS GpioDataRegs;
extern volatile struct EPWM_REGS      EPwm1Regs;
extern volatile struct EPWM_REGS      EPwm2Regs;
extern volatile struct EPWM_REGS      EPwm3Regs;
extern struct EQEP_REGS               gHostEQep;
extern struct EQEP_REGS              *EQepRegs;
extern volatile Uint16                gHostCpuCtl[7];
extern unsigned short                 gMockCall[MC_COUNT];
extern unsigned short                 gMockAbDir;
extern unsigned short                 gMockUvwDir;
extern unsigned short                 gMockRtDir;

/* ---- counters ------------------------------------------------------------ */

static long g_cases;
static long g_mism;
static long g_mismShown;

/* ------------------------------------------------------------------------- */
/* shared state (owned by other modules, read and written by the module)     */
/* ------------------------------------------------------------------------- */

struct SHARED {
    IPM_PG_DIR_STRUCT        pgDir;
    IPM_UVW_PG_STRUCT        uvwpg;
    RUN_STATUS_STRUCT        mainStatus;
    MOTOR_PARA_EST           getParVarable;
    IPM_POSITION_STRUCT      ipmPos;
    MT_STRUCT_Q24            imtSet;
    MOTOR_STRUCT             motorInfo;
    INV_STRUCT               invInfo;
    MOTOR_EXTERN_STRUCT      motorExtInfo;
    MOTOR_EXTERN_STRUCT      motorExtReg;
    UDC_STRUCT               udc;
    ADC_STRUCT               adc;
    BASE_COMMAND_STRUCT      mainCmd;
    LINE_CURRENT_STRUCT      lineCur;
    OUT_VOLT_STRUCT          outVolt;
    ROTOR_TRANS_STRUCT       rotorTrans;
    PG_DATA_STRUCT           pgData;
    struct FAULT_CODE_INFOR_STRUCT_DEF error;
    UVW_STRUCT_Q24           iuvw24;
    CONTROL_MOTOR_TYPE_ENUM  ctrMotorType;
    struct GPIO_DATA_REGS    gpio;
    struct EPWM_REGS         epwm1;
    struct EPWM_REGS         epwm2;
    struct EPWM_REGS         epwm3;
    struct EQEP_REGS         eqep;
    Uint16                   cpu[7];
    unsigned short           mockCall[MC_COUNT];
};

static struct SHARED sRef;
static struct SHARED sNew;
static struct SHARED sIn;

static void shared_save(struct SHARED *s)
{
    memset(s, 0, sizeof(*s));
    s->pgDir        = gPGDir;
    s->uvwpg        = gUVWPG;
    s->mainStatus   = gMainStatus;
    s->getParVarable = gGetParVarable;
    s->ipmPos       = gIPMPos;
    s->imtSet       = gIMTSet;
    s->motorInfo    = gMotorInfo;
    s->invInfo      = gInvInfo;
    s->motorExtInfo = gMotorExtInfo;
    s->motorExtReg  = gMotorExtReg;
    s->udc          = gUDC;
    s->adc          = gADC;
    s->mainCmd      = gMainCmd;
    s->lineCur      = gLineCur;
    s->outVolt      = gOutVolt;
    s->rotorTrans   = gRotorTrans;
    s->pgData       = gPGData;
    s->error        = gError;
    s->iuvw24       = gIUVWQ24;
    s->ctrMotorType = gCtrMotorType;
    memcpy(&s->gpio,   (const void *)&GpioDataRegs, sizeof(s->gpio));
    memcpy(&s->epwm1,  (const void *)&EPwm1Regs,   sizeof(s->epwm1));
    memcpy(&s->epwm2,  (const void *)&EPwm2Regs,   sizeof(s->epwm2));
    memcpy(&s->epwm3,  (const void *)&EPwm3Regs,   sizeof(s->epwm3));
    memcpy(&s->eqep,   (const void *)&gHostEQep,   sizeof(s->eqep));
    memcpy(s->cpu,     (const void *)gHostCpuCtl,  sizeof(s->cpu));
    memcpy(s->mockCall, (const void *)gMockCall,   sizeof(s->mockCall));
}

static void shared_load(const struct SHARED *s)
{
    gPGDir        = s->pgDir;
    gUVWPG        = s->uvwpg;
    gMainStatus   = s->mainStatus;
    gGetParVarable = s->getParVarable;
    gIPMPos       = s->ipmPos;
    gIMTSet       = s->imtSet;
    gMotorInfo    = s->motorInfo;
    gInvInfo      = s->invInfo;
    gMotorExtInfo = s->motorExtInfo;
    gMotorExtReg  = s->motorExtReg;
    gUDC          = s->udc;
    gADC          = s->adc;
    gMainCmd      = s->mainCmd;
    gLineCur      = s->lineCur;
    gOutVolt      = s->outVolt;
    gRotorTrans   = s->rotorTrans;
    gPGData       = s->pgData;
    gError        = s->error;
    gIUVWQ24      = s->iuvw24;
    gCtrMotorType = s->ctrMotorType;
    memcpy((void *)&GpioDataRegs, &s->gpio,  sizeof(s->gpio));
    memcpy((void *)&EPwm1Regs,   &s->epwm1, sizeof(s->epwm1));
    memcpy((void *)&EPwm2Regs,   &s->epwm2, sizeof(s->epwm2));
    memcpy((void *)&EPwm3Regs,   &s->epwm3, sizeof(s->epwm3));
    memcpy((void *)&gHostEQep,   &s->eqep,  sizeof(s->eqep));
    memcpy((void *)gHostCpuCtl,  s->cpu,    sizeof(s->cpu));
    memcpy((void *)gMockCall,    s->mockCall, sizeof(s->mockCall));
}

/* ------------------------------------------------------------------------- */
/* private state                                                            */
/* ------------------------------------------------------------------------- */

struct PRIV {
    IPM_ZERO_POS_STRUCT      ipmZero;
    IPM_INITPOS_PULSE_STR    ipmInitPos;
    PMSM_EST_BEMF            estBemf;
    PMSM_EST_PARAM_DATA      pmParEst;
};

static struct PRIV pRef;
static struct PRIV pNew;

static void priv_snap(struct PRIV *p, int isref)
{
    memset(p, 0, sizeof(*p));
    if (isref)
    {
        p->ipmZero    = ref_gIPMZero;
        p->ipmInitPos = ref_gIPMInitPos;
        p->estBemf    = ref_gEstBemf;
        p->pmParEst   = ref_gPmParEst;
    }
    else
    {
        p->ipmZero    = new_gIPMZero;
        p->ipmInitPos = new_gIPMInitPos;
        p->estBemf    = new_gEstBemf;
        p->pmParEst   = new_gPmParEst;
    }
}

/* Copy the whole private image ref -> new. Called once per scenario, *after*
 * all the input pokes and the divisor pins, so both sides are handed
 * byte-identical state. */
static void sync_priv(void)
{
    new_gIPMZero    = ref_gIPMZero;
    new_gIPMInitPos = ref_gIPMInitPos;
    new_gEstBemf    = ref_gEstBemf;
    new_gPmParEst   = ref_gPmParEst;
}

/* ------------------------------------------------------------------------- */
/* entry-point dispatch                                                     */
/* ------------------------------------------------------------------------- */

enum {
    F_ITUNE = 0, F_TIP, F_DET, F_SETPOS, F_SETPOSREF, F_CALLD, F_DETCAL,
    F_SETTS, F_SETPWM, F_DZERO, F_NOLOAD, F_LOAD, F_BEMF,
    F_COUNT
};

static void call_ref(int fn)
{
    switch (fn)
    {
        case F_ITUNE:     ref_InitSetPosTune();        break;
        case F_TIP:       ref_SynTuneInitPos();        break;
        case F_DET:       ref_SynInitPosDetect();      break;
        case F_SETPOS:    ref_SetIPMPos(gIPMPos.InitPos); break;
        case F_SETPOSREF: ref_SetIPMPos_ABZRef(gIPMPos.InitPos); break;
        case F_CALLD:     ref_SynCalLabAndLbc();       break;
        case F_DETCAL:    ref_SynInitPosDetCal();      break;
        case F_SETTS:     ref_SynInitPosDetSetTs();    break;
        case F_SETPWM:    ref_SynInitPosDetSetPwm(gIPMInitPos.Section); break;
        case F_DZERO:     ref_DetectZeroPosOnce();     break;
        case F_NOLOAD:    ref_SynTunePGZero_No_Load(); break;
        case F_LOAD:      ref_SynTunePGZero_Load();    break;
        default:          ref_SynTuneBemf();           break;
    }
}

static void call_new(int fn)
{
    switch (fn)
    {
        case F_ITUNE:     new_InitSetPosTune();        break;
        case F_TIP:       new_SynTuneInitPos();        break;
        case F_DET:       new_SynInitPosDetect();      break;
        case F_SETPOS:    new_SetIPMPos(gIPMPos.InitPos); break;
        case F_SETPOSREF: new_SetIPMPos_ABZRef(gIPMPos.InitPos); break;
        case F_CALLD:     new_SynCalLabAndLbc();       break;
        case F_DETCAL:    new_SynInitPosDetCal();      break;
        case F_SETTS:     new_SynInitPosDetSetTs();    break;
        case F_SETPWM:    new_SynInitPosDetSetPwm(gIPMInitPos.Section); break;
        case F_DZERO:     new_DetectZeroPosOnce();     break;
        case F_NOLOAD:    new_SynTunePGZero_No_Load(); break;
        case F_LOAD:      new_SynTunePGZero_Load();    break;
        default:          new_SynTuneBemf();           break;
    }
}

/* ------------------------------------------------------------------------- */
/* coverage: one slot per intended branch, asserted in main()                */
/* ------------------------------------------------------------------------- */

#define COV_LIST(X)                                                          \
    X(itune)                                                                 \
    X(tip_c1) X(tip_c2le) X(tip_c2eq) X(tip_c2gt) X(tip_c3s0) X(tip_c3sn)    \
    X(tip_c4) X(tip_c5lt) X(tip_c5ge) X(tip_def)                             \
    X(det_s1) X(det_s1scale) X(det_s2p0) X(det_s2p3) X(det_s2p1)             \
    X(det_s2p4) X(det_s2pd) X(det_s2cf) X(det_s2lack) X(det_s2done)          \
    X(det_s2g2) X(det_s2gp) X(det_s34p0) X(det_s34p1) X(det_s34p2)           \
    X(det_s34u) X(det_s34v) X(det_s34w) X(det_s5p) X(det_s5e) X(det_s6)      \
    X(det_s70) X(det_s714) X(det_s72u) X(det_s72v) X(det_s73) X(det_s75w)    \
    X(det_s75v) X(det_s7d) X(det_def)                                        \
    X(setpos) X(setposref) X(calld) X(detcal_in) X(detcal_set) X(detcal_out) \
    X(setts)                                                                 \
    X(setpwm0) X(setpwm1) X(setpwm2) X(setpwm3) X(setpwm4) X(setpwm5)        \
    X(setpwm6) X(setpwm_d)                                                   \
    X(dz_m0) X(dz_m1) X(dz_first) X(dz_acc)                                  \
    X(nl_1a) X(nl_1s) X(nl_1b) X(nl_1c) X(nl_1lt) X(nl_1ge) X(nl_1geq)       \
    X(nl_2m0) X(nl_2eq) X(nl_2m1) X(nl_2d1e) X(nl_2d2e) X(nl_2d2b) X(nl_2d1b) \
    X(nl_2d3e) X(nl_2d3b) X(nl_3) X(nl_3err) X(nl_3eq) X(nl_3flag) X(nl_4) X(nl_4wrap) \
    X(nl_4hit) X(nl_4err) X(nl_5lt) X(nl_5ge) X(nl_5m0) X(nl_5m1)            \
    X(nl_6lt) X(nl_6ge) X(nl_d)                                             \
    X(ld_1m0) X(ld_1m1) X(ld_2) X(ld_3err) X(ld_3eq) X(ld_3done) X(ld_4m0) X(ld_4uvw) \
    X(ld_4ab) X(ld_4abuvw) X(ld_4m1) X(ld_4rt) X(ld_5e0) X(ld_5e2) X(ld_5e3) \
    X(ld_5f0) X(ld_5flt) X(ld_5feq) X(ld_5done) X(ld_6in) X(ld_6out)          \
    X(ld_6done) X(ld_7lt) X(ld_7ge) X(ld_d)                                 \
    X(be_1sc) X(be_1no) X(be_1lt) X(be_1ge) X(be_2lt) X(be_2ge) X(be_2done)  \
    X(be_3acc) X(be_3done) X(be_3eq) X(be_4sc) X(be_4no) X(be_4done) X(be_5acc) \
    X(be_5done) X(be_6gt) X(be_6le) X(be_7acc) X(be_7done) X(be_8) X(be_d)

#define COV_ENUM(name) COV_##name,
enum { COV_LIST(COV_ENUM) COV_COUNT };
#undef COV_ENUM

static long g_cov[COV_COUNT];
static long g_fnCases[F_COUNT];

#define COV_INC(name) (g_cov[COV_##name]++)

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

static void compare(const char *tag)
{
    CMPM(sRef.pgDir,       sNew.pgDir,       "gPGDir");
    CMPM(sRef.uvwpg,       sNew.uvwpg,       "gUVWPG");
    CMPM(sRef.mainStatus,  sNew.mainStatus,  "gMainStatus");
    CMPM(sRef.getParVarable, sNew.getParVarable, "gGetParVarable");
    CMPM(sRef.ipmPos,      sNew.ipmPos,      "gIPMPos");
    CMPM(sRef.imtSet,      sNew.imtSet,      "gIMTSet");
    CMPM(sRef.motorInfo,   sNew.motorInfo,   "gMotorInfo");
    CMPM(sRef.invInfo,     sNew.invInfo,     "gInvInfo");
    CMPM(sRef.motorExtInfo, sNew.motorExtInfo, "gMotorExtInfo");
    CMPM(sRef.motorExtReg, sNew.motorExtReg, "gMotorExtReg");
    CMPM(sRef.udc,         sNew.udc,         "gUDC");
    CMPM(sRef.adc,         sNew.adc,         "gADC");
    CMPM(sRef.mainCmd,     sNew.mainCmd,     "gMainCmd");
    CMPM(sRef.lineCur,     sNew.lineCur,     "gLineCur");
    CMPM(sRef.outVolt,     sNew.outVolt,     "gOutVolt");
    CMPM(sRef.rotorTrans,  sNew.rotorTrans,  "gRotorTrans");
    CMPM(sRef.pgData,      sNew.pgData,      "gPGData");
    CMPM(sRef.error,       sNew.error,       "gError");
    CMPM(sRef.iuvw24,      sNew.iuvw24,      "gIUVWQ24");
    CMPM(sRef.ctrMotorType, sNew.ctrMotorType, "gCtrMotorType");
    CMPM(sRef.gpio,        sNew.gpio,        "GpioDataRegs");
    CMPM(sRef.epwm1,       sNew.epwm1,       "EPwm1Regs");
    CMPM(sRef.epwm2,       sNew.epwm2,       "EPwm2Regs");
    CMPM(sRef.epwm3,       sNew.epwm3,       "EPwm3Regs");
    CMPM(sRef.eqep,        sNew.eqep,        "EQEP_REGS");
    CMPM(sRef.cpu,         sNew.cpu,         "cpuCtl");
    CMPM(sRef.mockCall,    sNew.mockCall,    "mockCall");

    CMPM(pRef.ipmZero,     pNew.ipmZero,     "gIPMZero");
    CMPM(pRef.ipmInitPos,  pNew.ipmInitPos,  "gIPMInitPos");
    CMPM(pRef.estBemf,     pNew.estBemf,     "gEstBemf");
    CMPM(pRef.pmParEst,    pNew.pmParEst,    "gPmParEst");
}

/* ------------------------------------------------------------------------- */
/* one step                                                                 */
/* ------------------------------------------------------------------------- */

static void do_step(int fn, const char *tag)
{
    g_cases++;
    g_fnCases[fn]++;

    shared_save(&sIn);

    /* ref side: reset the ledger and the CPU-control counters so both sides
     * start the call from the same observable zero. */
    memset((void *)gMockCall,   0, sizeof(gMockCall));
    memset((void *)gHostCpuCtl, 0, sizeof(gHostCpuCtl));
    call_ref(fn);
    shared_save(&sRef);
    priv_snap(&pRef, 1);

    /* new side: restore the input image, reset the ledgers again, replay. */
    shared_load(&sIn);
    memset((void *)gMockCall,   0, sizeof(gMockCall));
    memset((void *)gHostCpuCtl, 0, sizeof(gHostCpuCtl));
    call_new(fn);
    shared_save(&sNew);
    priv_snap(&pNew, 0);

    compare(tag);

    /* Flush after every scenario. A mutation that makes the module divide by
     * zero (or index past an array) only shows up when the *module* runs, and
     * the fault kills the process. Without this flush the whole stdout buffer
     * -- including the deterministic-phase mismatches printed before the fault
     * -- is discarded, so negative_control.py would record a crash with empty
     * output and no evidence of where it came from. */
    fflush(stdout);
}

/* @@END@@ */

/* ========================================================================= */
/* everything below is appended: divisor pins, dirty image, the 13 phases,   */
/* the random sweep and main().                                              */
/* ========================================================================= */

/* Argument holder for the F_SETPWM dispatch: call_ref() / call_new() pass
 * gIPMInitPos.Section straight into SynInitPosDetSetPwm().  The module owns a
 * *renamed* copy per side (ref_/new_gIPMInitPos), so the unrenamed name has to
 * exist here, in the driver, as the argument carrier. */
IPM_INITPOS_PULSE_STR gIPMInitPos;

/* ------------------------------------------------------------------------- */
/* divisor pins                                                              */
/* ------------------------------------------------------------------------- */
/* Every variable divisor in the module, made non-zero on every scenario,
 * after the phase's pokes and before sync_priv().  See the header comment.
 *
 *   gMotorExtInfo.Poles            SetIPMPos / SetIPMPos_ABZRef
 *   gMotorInfo.Current             SynInitPosDetect c1, No_Load c1, Bemf 1/4
 *   ((Ulong)m_DetaI * 100)         SynCalLabAndLbc
 *   ref_gEstBemf.AccDecTick            SynTuneBemf c2 / c6
 *   temp3 = (Id2-Id1)*TuneFreq     SynTuneBemf c8
 *
 * m_DetaI is computed by the module from gIPMInitPos.Cur[]; the arithmetic is
 * replicated here exactly (signed-min pairs, >>1, *Current, >>12) so the pin
 * is a real guard and not a guess. */
static void pin_divisors(void)
{
    int i;

    if (gMotorExtInfo.Poles == 0) gMotorExtInfo.Poles = 4;
    if (gMotorInfo.Current  == 0) gMotorInfo.Current  = 1000;
    if (ref_gEstBemf.AccDecTick  == 0) ref_gEstBemf.AccDecTick = 1;

    for (i = 0; i < 3; i++)
    {
        Uint sel = (Uint)(i << 1);
        Uint c1, c2, s;

        c1 = (ref_gIPMInitPos.Cur[sel] <= ref_gIPMInitPos.Cur[sel + 1])
                 ? (Uint)ref_gIPMInitPos.Cur[sel]
                 : (Uint)ref_gIPMInitPos.Cur[sel + 1];
        c2 = (ref_gIPMInitPos.Cur[sel + 6] <= ref_gIPMInitPos.Cur[sel + 7])
                 ? (Uint)ref_gIPMInitPos.Cur[sel + 6]
                 : (Uint)ref_gIPMInitPos.Cur[sel + 7];
        s = (Uint)((c1 >> 1) + (c2 >> 1));

        if (s == 0)
        {
            ref_gIPMInitPos.Cur[sel]     = 2;
            ref_gIPMInitPos.Cur[sel + 1] = 2;
            ref_gIPMInitPos.Cur[sel + 6] = 2;
            ref_gIPMInitPos.Cur[sel + 7] = 2;
            s = 2;
        }

        if ((((Ulong)s * (Ulong)gMotorInfo.Current) >> 12) == 0)
        {
            gMotorInfo.Current = 4096;
        }
    }

    if (((ref_gEstBemf.TotalId2 - ref_gEstBemf.TotalId1)
         * ref_gEstBemf.TuneFreq) == 0)
    {
        ref_gEstBemf.TotalId1 = 0;
        ref_gEstBemf.TotalId2 = 1000;
        ref_gEstBemf.TuneFreq = 1;
    }
}

/* ------------------------------------------------------------------------- */
/* dirty image                                                               */
/* ------------------------------------------------------------------------- */

static void dirty_all(void)
{
    /* shared globals */
    memset(&gPGDir,         0xA5, sizeof(gPGDir));
    memset(&gUVWPG,         0xA5, sizeof(gUVWPG));
    memset(&gMainStatus,    0xA5, sizeof(gMainStatus));
    memset(&gGetParVarable, 0xA5, sizeof(gGetParVarable));
    memset(&gIPMPos,        0xA5, sizeof(gIPMPos));
    memset(&gIMTSet,        0xA5, sizeof(gIMTSet));
    memset(&gMotorInfo,     0xA5, sizeof(gMotorInfo));
    memset(&gInvInfo,       0xA5, sizeof(gInvInfo));
    memset(&gMotorExtInfo,  0xA5, sizeof(gMotorExtInfo));
    memset(&gMotorExtReg,   0xA5, sizeof(gMotorExtReg));
    memset(&gUDC,           0xA5, sizeof(gUDC));
    memset(&gADC,           0xA5, sizeof(gADC));
    memset(&gMainCmd,       0xA5, sizeof(gMainCmd));
    memset(&gLineCur,       0xA5, sizeof(gLineCur));
    memset(&gOutVolt,       0xA5, sizeof(gOutVolt));
    memset(&gRotorTrans,    0xA5, sizeof(gRotorTrans));
    memset(&gPGData,        0xA5, sizeof(gPGData));
    memset(&gError,         0xA5, sizeof(gError));
    memset(&gIUVWQ24,       0xA5, sizeof(gIUVWQ24));
    memset(&gCtrMotorType,  0xA5, sizeof(gCtrMotorType));

    /* peripheral registers */
    memset((void *)&GpioDataRegs, 0xA5, sizeof(GpioDataRegs));
    memset((void *)&EPwm1Regs,    0xA5, sizeof(EPwm1Regs));
    memset((void *)&EPwm2Regs,    0xA5, sizeof(EPwm2Regs));
    memset((void *)&EPwm3Regs,    0xA5, sizeof(EPwm3Regs));
    memset((void *)&gHostEQep,    0xA5, sizeof(gHostEQep));
    memset((void *)gHostCpuCtl,   0xA5, sizeof(Uint16) * 7);
    memset((void *)gMockCall,     0xA5, sizeof(gMockCall));

    /* the driver-local argument holder */
    memset(&gIPMInitPos, 0xA5, sizeof(gIPMInitPos));

    /* the module-owned per-side globals */
    memset(&ref_gIPMZero,    0xA5, sizeof(ref_gIPMZero));
    memset(&ref_gIPMInitPos, 0xA5, sizeof(ref_gIPMInitPos));
    memset(&ref_gEstBemf,    0xA5, sizeof(ref_gEstBemf));
    memset(&ref_gPmParEst,   0xA5, sizeof(ref_gPmParEst));
    memset(&new_gIPMZero,    0xA5, sizeof(new_gIPMZero));
    memset(&new_gIPMInitPos, 0xA5, sizeof(new_gIPMInitPos));
    memset(&new_gEstBemf,    0xA5, sizeof(new_gEstBemf));
    memset(&new_gPmParEst,   0xA5, sizeof(new_gPmParEst));

    /* The GPIO image is prefilled with 0xA5, so the drive-enable bit
     * (GPASET/GPACLEAR.GPIO7) is *already* 1 on both sides: DisableDrive()
     * (GPASET=1) and EnableDrive() (GPACLEAR=1) would then be invisible.
     * Clear both so the macros' traffic -- and a dropped call -- shows up. */
    GpioDataRegs.GPASET.bit.GPIO7   = 0;
    GpioDataRegs.GPACLEAR.bit.GPIO7 = 0;

    /* the Judge* direction inputs (not part of struct SHARED; deterministic) */
    gMockAbDir  = 0;
    gMockUvwDir = 0;
    gMockRtDir  = 0;
}

/* pin, sync, step -- the shared tail of every scenario */
static void scn_run(int fn, const char *tag)
{
    pin_divisors();
    sync_priv();
    do_step(fn, tag);
}

/* ========================================================================= */
/* phase 1: InitSetPosTune  (no branches)                                    */
/* ========================================================================= */

static void phase_itune(void)
{
    dirty_all();
    COV_INC(itune);
    scn_run(F_ITUNE, "itune");
}

/* ========================================================================= */
/* phase 2: SynTuneInitPos  (IdSubStep 1..5 + default)                       */
/* ========================================================================= */

static void phase_tip(void)
{
    /* case 1 */
    dirty_all();
    gGetParVarable.IdSubStep = 1;
    COV_INC(tip_c1);
    scn_run(F_TIP, "tip_c1");

    /* case 2, Waite <= 9 */
    dirty_all();
    gGetParVarable.IdSubStep = 2;
    ref_gIPMInitPos.Waite = 0;
    COV_INC(tip_c2le);
    scn_run(F_TIP, "tip_c2le");

    /* case 2, Waite input 9 -> ++ makes it exactly 10, the `>` / `>=` boundary */
    dirty_all();
    gGetParVarable.IdSubStep = 2;
    ref_gIPMInitPos.Waite = 9;
    COV_INC(tip_c2eq);
    scn_run(F_TIP, "tip_c2eq");

    /* case 2, Waite > 10 */
    dirty_all();
    gGetParVarable.IdSubStep = 2;
    ref_gIPMInitPos.Waite = 10;
    COV_INC(tip_c2gt);
    scn_run(F_TIP, "tip_c2gt");

    /* case 2, Waite == 9. The module increments Waite *before* the test, so
     * this is the only input that lands on the `> 10` boundary: Waite++ makes
     * it exactly 10, where `> 10` is false and `>= 10` is true. With Waite 10
     * (the probe above) Waite++ is 11, which both forms accept. */
    dirty_all();
    gGetParVarable.IdSubStep = 2;
    ref_gIPMInitPos.Waite = 9;
    COV_INC(tip_c2gt);
    scn_run(F_TIP, "tip_c2b");

    /* case 3, Step == 0 */
    dirty_all();
    gGetParVarable.IdSubStep = 3;
    ref_gIPMInitPos.Step = 0;
    COV_INC(tip_c3s0);
    scn_run(F_TIP, "tip_c3s0");

    /* case 3, Step != 0 */
    dirty_all();
    gGetParVarable.IdSubStep = 3;
    ref_gIPMInitPos.Step = 1;
    COV_INC(tip_c3sn);
    scn_run(F_TIP, "tip_c3sn");

    /* case 4 */
    dirty_all();
    gGetParVarable.IdSubStep = 4;
    COV_INC(tip_c4);
    scn_run(F_TIP, "tip_c4");

    /* case 5, Waite < 500 */
    dirty_all();
    gGetParVarable.IdSubStep = 5;
    ref_gIPMInitPos.Waite = 0;
    COV_INC(tip_c5lt);
    scn_run(F_TIP, "tip_c5lt");

    /* case 5, Waite >= 500 */
    dirty_all();
    gGetParVarable.IdSubStep = 5;
    ref_gIPMInitPos.Waite = 500;
    COV_INC(tip_c5ge);
    scn_run(F_TIP, "tip_c5ge");

    /* default */
    dirty_all();
    gGetParVarable.IdSubStep = 0;
    COV_INC(tip_def);
    scn_run(F_TIP, "tip_def");
}

/* ========================================================================= */
/* phase 3: SynInitPosDetect                                                 */
/* ========================================================================= */

static void phase_det(void)
{
    /* SynCalLabAndLbc with a *spread-out* Cur[], run before any other scenario
     * reaches that function.
     *
     * Why this shape: `SynCalLabAndLbc` divides by `(Ulong)m_DetaI * 100` with
     * no guard, so any input that drives m_DetaI to zero kills the host process
     * with #DE instead of producing a mismatch. The three phases below pick
     * Cur[] from small tables, and a spread-out case makes the three LPhase[]
     * results differ from each other, so "which phase got which value" is
     * observable at all.
     *
     * History (2026-10-19): two mutations used to crash here -- PE124
     * (`m_Sel = m_Index<<2`, reads Cur[15] of a 12-element array) and PE126
     * (`+` -> `-`, can make m_DetaI zero). Both were replaced with well-defined
     * forms in build/_me_pmsmparest_gen.py rather than worked around here; see
     * that file's docstring. This scenario stays because the coverage is worth
     * having on its own. */
    dirty_all();
    ref_gIPMInitPos.Cur[0]  = 100;   ref_gIPMInitPos.Cur[1]  = 200;
    ref_gIPMInitPos.Cur[2]  = 300;   ref_gIPMInitPos.Cur[3]  = 400;
    ref_gIPMInitPos.Cur[4]  = 500;   ref_gIPMInitPos.Cur[5]  = 600;
    ref_gIPMInitPos.Cur[6]  = 700;   ref_gIPMInitPos.Cur[7]  = 800;
    ref_gIPMInitPos.Cur[8]  = 30000; ref_gIPMInitPos.Cur[9]  = 30000;
    ref_gIPMInitPos.Cur[10] = 1000;  ref_gIPMInitPos.Cur[11] = 2000;
    gMotorInfo.Current = 1000;
    gUDC.uDCFilter = 3000;
    gMotorExtReg.RsPm = 100;
    ref_gIPMInitPos.PWMTs = 3000;
    COV_INC(calld);
    scn_run(F_CALLD, "calld0");

    /* case 1, no scale */
    dirty_all();
    ref_gIPMInitPos.Step = 1;
    gInvInfo.InvCurrent = 0;
    gMotorInfo.Current  = 1000;
    COV_INC(det_s1);
    scn_run(F_DET, "det_s1");

    /* case 1 with InvCurrent >= Current: the scaling arm is skipped, so
     * CurLimit keeps its literal 5792. Needed so `5792 -> 5793` is visible --
     * with the arm on, both map to the same truncated quotient. */
    dirty_all();
    ref_gIPMInitPos.Step = 1;
    gInvInfo.InvCurrent = 1000;
    gMotorInfo.Current  = 1000;
    COV_INC(det_s1);
    scn_run(F_DET, "det_s1raw");

    /* case 1, InvCurrent < Current -> CurLimit scaled */
    dirty_all();
    ref_gIPMInitPos.Step = 1;
    gInvInfo.InvCurrent = 100;
    gMotorInfo.Current  = 1000;
    COV_INC(det_s1scale);
    scn_run(F_DET, "det_s1scale");

    /* case 2 / PeriodCnt == 0 (input 5).  Safe "gp" arm so nothing else fires. */
    dirty_all();
    ref_gIPMInitPos.Step = 2;
    ref_gIPMInitPos.PeriodCnt = 5;
    ref_gIPMInitPos.CurLimit = 0xFFFF;
    ref_gIPMInitPos.CurFirst = 0;
    gIUVWQ24.U = 0;
    COV_INC(det_s2p0);
    scn_run(F_DET, "det_s2p0");

    /* case 2 / PeriodCnt == 3 (input 2) */
    dirty_all();
    ref_gIPMInitPos.Step = 2;
    ref_gIPMInitPos.PeriodCnt = 2;
    COV_INC(det_s2p3);
    scn_run(F_DET, "det_s2p3");

    /* case 2 / PeriodCnt == 1 (input 0) */
    dirty_all();
    ref_gIPMInitPos.Step = 2;
    ref_gIPMInitPos.PeriodCnt = 0;
    COV_INC(det_s2p1);
    scn_run(F_DET, "det_s2p1");

    /* case 2 / PeriodCnt == 4 (input 3) */
    dirty_all();
    ref_gIPMInitPos.Step = 2;
    ref_gIPMInitPos.PeriodCnt = 3;
    COV_INC(det_s2p4);
    scn_run(F_DET, "det_s2p4");

    /* case 2 / default (input 1 -> 2) */
    dirty_all();
    ref_gIPMInitPos.Step = 2;
    ref_gIPMInitPos.PeriodCnt = 1;
    COV_INC(det_s2pd);
    scn_run(F_DET, "det_s2pd");

    /* case 2 / period 0 / CurFirst clamp */
    dirty_all();
    ref_gIPMInitPos.Step = 2;
    ref_gIPMInitPos.PeriodCnt = 5;
    ref_gIPMInitPos.CurLimit = 0xFFFF;
    ref_gIPMInitPos.CurFirst = 1000;
    gIUVWQ24.U = 0;
    COV_INC(det_s2cf);
    scn_run(F_DET, "det_s2cf");

    /* case 2 / period 0 / lack-phase error */
    dirty_all();
    ref_gIPMInitPos.Step = 2;
    ref_gIPMInitPos.PeriodCnt = 5;
    ref_gIPMInitPos.PWMTs = 65000;
    ref_gIPMInitPos.CurLimit = 0xFFFF;
    ref_gIPMInitPos.CurFirst = 0;
    gIUVWQ24.U = 0;
    COV_INC(det_s2lack);
    scn_run(F_DET, "det_s2lack");

    /* case 2 / period 0 / done (m_Cur > CurLimit) */
    dirty_all();
    ref_gIPMInitPos.Step = 2;
    ref_gIPMInitPos.PeriodCnt = 5;
    ref_gIPMInitPos.PWMTs = 60000;
    ref_gIPMInitPos.CurLimit = 0;
    ref_gIPMInitPos.CurFirst = 0;
    gIUVWQ24.U = 2457600L;           /* >>12 == 600 */
    COV_INC(det_s2done);
    scn_run(F_DET, "det_s2done");

    /* case 2 / period 0 / m_Cur > CurLimit>>1 -> += 2*DSP_CLOCK */
    dirty_all();
    ref_gIPMInitPos.Step = 2;
    ref_gIPMInitPos.PeriodCnt = 5;
    ref_gIPMInitPos.PWMTs = 0;
    ref_gIPMInitPos.CurLimit = 1000;
    ref_gIPMInitPos.CurFirst = 0;
    gIUVWQ24.U = 2457600L;           /* 600 > 500 */
    COV_INC(det_s2g2);
    scn_run(F_DET, "det_s2g2");

    /* case 2 / period 0 / else -> += Poles*DSP_CLOCK*2 */
    dirty_all();
    ref_gIPMInitPos.Step = 2;
    ref_gIPMInitPos.PeriodCnt = 5;
    ref_gIPMInitPos.PWMTs = 0;
    ref_gIPMInitPos.CurLimit = 1000;
    ref_gIPMInitPos.CurFirst = 0;
    gIUVWQ24.U = 0;
    gMotorExtInfo.Poles = 4;
    COV_INC(det_s2gp);
    scn_run(F_DET, "det_s2gp");

    /* Step 3/4 / PeriodCnt 0 + Section U */
    dirty_all();
    ref_gIPMInitPos.Step = 3;
    ref_gIPMInitPos.Section = 0;
    ref_gIPMInitPos.PeriodCnt = 2;
    gIUVWQ24.U = 0x1000;
    COV_INC(det_s34p0);
    COV_INC(det_s34u);
    scn_run(F_DET, "det_s34p0u");

    /* Step 3/4 / Section V / PeriodCnt lands on 0 (2 + 1 -> 3 -> reset to 0),
     * so this is the arm where m_Cur is actually consumed (Cur[m_Index]). U and
     * V differ, which is what tells `gIUVWQ24.V >> 12` from `U >> 12`. */
    dirty_all();
    ref_gIPMInitPos.Step = 3;
    ref_gIPMInitPos.Section = 2;
    ref_gIPMInitPos.PeriodCnt = 2;
    ref_gIPMInitPos.CurFirst = 0;
    gIUVWQ24.U = 0;
    gIUVWQ24.V = 0x1000;
    COV_INC(det_s34p0);
    COV_INC(det_s34v);
    scn_run(F_DET, "det_s34p0v");

    /* Step 3/4 / PeriodCnt 1 + Section V */
    dirty_all();
    ref_gIPMInitPos.Step = 3;
    ref_gIPMInitPos.Section = 2;
    ref_gIPMInitPos.PeriodCnt = 0;
    gIUVWQ24.V = 0x1000;
    COV_INC(det_s34p1);
    COV_INC(det_s34v);
    scn_run(F_DET, "det_s34p1v");

    /* Step 3/4 / PeriodCnt 2 + Section W */
    dirty_all();
    ref_gIPMInitPos.Step = 3;
    ref_gIPMInitPos.Section = 4;
    ref_gIPMInitPos.PeriodCnt = 1;
    gIUVWQ24.W = 0x1000;
    COV_INC(det_s34p2);
    COV_INC(det_s34w);
    scn_run(F_DET, "det_s34p2w");

    /* case 5 / RunStep == STATUS_GET_PAR -> Step++ */
    dirty_all();
    ref_gIPMInitPos.Step = 5;
    gMainStatus.RunStep = STATUS_GET_PAR;
    COV_INC(det_s5p);
    scn_run(F_DET, "det_s5p");

    /* case 5 / else -> Step = 0 */
    dirty_all();
    ref_gIPMInitPos.Step = 5;
    gMainStatus.RunStep = 0;
    COV_INC(det_s5e);
    scn_run(F_DET, "det_s5e");

    /* case 6 */
    dirty_all();
    ref_gIPMInitPos.Step = 6;
    COV_INC(det_s6);
    scn_run(F_DET, "det_s6");

    /* case 7 / PhsChkStep 0 */
    dirty_all();
    ref_gIPMInitPos.Step = 7;
    ref_gIPMInitPos.PhsChkStep = 0;
    COV_INC(det_s70);
    scn_run(F_DET, "det_s70");

    /* case 7 / PhsChkStep 1 (== 4) */
    dirty_all();
    ref_gIPMInitPos.Step = 7;
    ref_gIPMInitPos.PhsChkStep = 1;
    COV_INC(det_s714);
    scn_run(F_DET, "det_s714");

    /* case 7 / PhsChkStep 2, U low */
    dirty_all();
    ref_gIPMInitPos.Step = 7;
    ref_gIPMInitPos.PhsChkStep = 2;
    ref_gIPMInitPos.CurLimit = 0xFFFF;      /* >>3 == 8191 */
    gIUVWQ24.U = 0;
    gIUVWQ24.V = 33550336L;                 /* >>12 == 8191, not < 8191 */
    COV_INC(det_s72u);
    scn_run(F_DET, "det_s72u");

    /* case 7 / PhsChkStep 2, V low */
    dirty_all();
    ref_gIPMInitPos.Step = 7;
    ref_gIPMInitPos.PhsChkStep = 2;
    ref_gIPMInitPos.CurLimit = 0xFFFF;
    gIUVWQ24.U = 33550336L;
    gIUVWQ24.V = 0;
    COV_INC(det_s72v);
    scn_run(F_DET, "det_s72v");

    /* case 7 / PhsChkStep 3 */
    dirty_all();
    ref_gIPMInitPos.Step = 7;
    ref_gIPMInitPos.PhsChkStep = 3;
    COV_INC(det_s73);
    scn_run(F_DET, "det_s73");

    /* case 7 / PhsChkStep 5, W low */
    dirty_all();
    ref_gIPMInitPos.Step = 7;
    ref_gIPMInitPos.PhsChkStep = 5;
    ref_gIPMInitPos.CurLimit = 0xFFFF;
    gIUVWQ24.W = 0;
    gIUVWQ24.V = 33550336L;
    COV_INC(det_s75w);
    scn_run(F_DET, "det_s75w");

    /* case 7 / PhsChkStep 5, V low */
    dirty_all();
    ref_gIPMInitPos.Step = 7;
    ref_gIPMInitPos.PhsChkStep = 5;
    ref_gIPMInitPos.CurLimit = 0xFFFF;
    gIUVWQ24.W = 33550336L;
    gIUVWQ24.V = 0;
    COV_INC(det_s75v);
    scn_run(F_DET, "det_s75v");

    /* case 7 / default */
    dirty_all();
    ref_gIPMInitPos.Step = 7;
    ref_gIPMInitPos.PhsChkStep = 6;
    COV_INC(det_s7d);
    scn_run(F_DET, "det_s7d");

    /* default (Step not 1..7) */
    dirty_all();
    ref_gIPMInitPos.Step = 0;
    COV_INC(det_def);
    scn_run(F_DET, "det_def");
}

/* ========================================================================= */
/* phase 4: SetIPMPos / SetIPMPos_ABZRef                                    */
/* ========================================================================= */

static void phase_setpos(void)
{
    dirty_all();
    gIPMPos.InitPos = 0x1234;
    gMotorExtInfo.Poles = 4;
    gPGData.PulseNum = 2500;
    gHostEQep.QPOSCNT = 0x55;
    COV_INC(setpos);
    scn_run(F_SETPOS, "setpos");

    /* Same, but (Pos / Poles) == 0: the `(2<<13)` rounding term alone decides
     * the `>> 14`, so 2<<13 -> 2<<12 changes the result (1 -> 0). With the
     * first probe the term is swamped by the quotient. */
    dirty_all();
    gIPMPos.InitPos = 3;               /* 3 / 4 == 0 */
    gMotorExtInfo.Poles = 4;
    gPGData.PulseNum = 2500;
    gHostEQep.QPOSCNT = 0x55;
    COV_INC(setpos);
    scn_run(F_SETPOS, "setpos_rnd");

    dirty_all();
    gIPMPos.InitPos = 0xFFFF;
    gMotorExtInfo.Poles = 3;
    gPGData.PulseNum = 4096;
    gHostEQep.QPOSCNT = 0x1234;
    COV_INC(setposref);
    scn_run(F_SETPOSREF, "setposref");
}

/* ========================================================================= */
/* phase 5: SynCalLabAndLbc / SynInitPosDetCal                              */
/* ========================================================================= */

static void phase_calld(void)
{
    int i;

    dirty_all();
    for (i = 0; i < 12; i++)
    {
        ref_gIPMInitPos.Cur[i] = (int)(100 + i);
    }
    gMotorInfo.Current = 1000;
    gUDC.uDCFilter = 3000;
    gMotorExtReg.RsPm = 100;
    ref_gIPMInitPos.PWMTs = 3000;
    COV_INC(calld);
    scn_run(F_CALLD, "calld");
}

static void phase_detcal(void)
{
    int i;

    /* all-zero Cur -> m_X == m_Y == 0 -> the small-error arm */
    dirty_all();
    for (i = 0; i < 12; i++)
    {
        ref_gIPMInitPos.Cur[i] = 0;
    }
    COV_INC(detcal_in);
    scn_run(F_DETCAL, "detcal_in");

    /* the same small-error arm, but with ERROR_INIT_POS already latched:
     * `|=` is then a no-op while `^=` clears the bit. */
    dirty_all();
    for (i = 0; i < 12; i++)
    {
        ref_gIPMInitPos.Cur[i] = 0;
    }
    gError.ErrorCode.all |= ERROR_INIT_POS;
    COV_INC(detcal_set);
    scn_run(F_DETCAL, "detcal_set");

    /* Cur[2] - Cur[4] large -> the error arm */
    dirty_all();
    for (i = 0; i < 12; i++)
    {
        ref_gIPMInitPos.Cur[i] = 0;
    }
    ref_gIPMInitPos.Cur[2] = 0;
    ref_gIPMInitPos.Cur[3] = 10000;
    ref_gIPMInitPos.Cur[4] = 0;
    ref_gIPMInitPos.Cur[5] = 0;
    COV_INC(detcal_out);
    scn_run(F_DETCAL, "detcal_out");

    /* small-error arm with ERROR_INIT_POS *already* set in the error word.
     * `all |= ERROR_INIT_POS` leaves the bit set, `all ^= ERROR_INIT_POS`
     * clears it -- the only input that tells the two forms apart (they agree
     * on 0 -> 1 when the bit starts clear). */
    dirty_all();
    for (i = 0; i < 12; i++)
    {
        ref_gIPMInitPos.Cur[i] = 0;
    }
    gError.ErrorCode.all = ERROR_INIT_POS;
    COV_INC(detcal_in);
    scn_run(F_DETCAL, "detcal_or");

    /* The order/sign arm.  `Cur[3] - Cur[2]` (209) survives the pair loop and
     * the +Cur[*+6] fold, so it drives every sensitive site at once:
     *   - m_Y's 28378 coefficient (28378 -> 28379 moves the `>>15` result by
     *     one count for this magnitude),
     *   - the m_X sign, where `- (Cur[2]>>1)` and `+ (Cur[2]>>1)` differ,
     *   - InitPos 18567, the smallest value whose *3600 and *3601 forms land
     *     on different `>>16` results,
     *   - Cur[0]/Cur[1] (0 / -58), so the `Cur[m_Index+1] - Cur[m_Index]`
     *     subtraction order is observable.
     * Every per-sel min is non-zero, so pin_divisors() leaves this Cur[]
     * exactly as written. */
    dirty_all();
    ref_gIPMInitPos.Cur[0]  = 0;
    ref_gIPMInitPos.Cur[1]  = -58;
    ref_gIPMInitPos.Cur[2]  = 0;
    ref_gIPMInitPos.Cur[3]  = 209;
    ref_gIPMInitPos.Cur[4]  = 0;
    ref_gIPMInitPos.Cur[5]  = 0;
    ref_gIPMInitPos.Cur[6]  = 100;
    ref_gIPMInitPos.Cur[7]  = 100;
    ref_gIPMInitPos.Cur[8]  = 100;
    ref_gIPMInitPos.Cur[9]  = 100;
    ref_gIPMInitPos.Cur[10] = 100;
    ref_gIPMInitPos.Cur[11] = 100;
    COV_INC(detcal_in);
    scn_run(F_DETCAL, "detcal_ord");
}

/* ========================================================================= */
/* phase 6: SynInitPosDetSetTs / SynInitPosDetSetPwm                        */
/* ========================================================================= */

static void phase_setts(void)
{
    dirty_all();
    ref_gIPMInitPos.PWMTs = 3000;
    gADC.DelayApply = 0x44;
    COV_INC(setts);
    scn_run(F_SETTS, "setts");

    /* the eight SynInitPosDetSetPwm arms, driven through the F_SETPWM entry */
    dirty_all();
    gIPMInitPos.Section = 0;
    COV_INC(setpwm0);
    scn_run(F_SETPWM, "setpwm0");

    dirty_all();
    gIPMInitPos.Section = 1;
    COV_INC(setpwm1);
    scn_run(F_SETPWM, "setpwm1");

    dirty_all();
    gIPMInitPos.Section = 2;
    COV_INC(setpwm2);
    scn_run(F_SETPWM, "setpwm2");

    dirty_all();
    gIPMInitPos.Section = 3;
    COV_INC(setpwm3);
    scn_run(F_SETPWM, "setpwm3");

    dirty_all();
    gIPMInitPos.Section = 4;
    COV_INC(setpwm4);
    scn_run(F_SETPWM, "setpwm4");

    dirty_all();
    gIPMInitPos.Section = 5;
    COV_INC(setpwm5);
    scn_run(F_SETPWM, "setpwm5");

    dirty_all();
    gIPMInitPos.Section = 6;
    COV_INC(setpwm6);
    scn_run(F_SETPWM, "setpwm6");

    dirty_all();
    gIPMInitPos.Section = 7;
    COV_INC(setpwm_d);
    scn_run(F_SETPWM, "setpwm_d");
}

/* ========================================================================= */
/* phase 7: DetectZeroPosOnce                                               */
/* ========================================================================= */

static void phase_dzero(void)
{
    /* PGMode == 0.  FeedPos / RTPos / RotorPos are given three *distinct*
     * values so that picking the wrong one (or the wrong branch) is a real
     * difference -- under the all-0xA5 dirty image they would coincide. */
    dirty_all();
    gPGData.PGMode = 0;
    ref_gIPMZero.FeedPos = 0x0111;
    gRotorTrans.RTPos    = 0x0222;
    gIPMPos.RotorPos     = 0x0333;
    ref_gIPMZero.DetectCnt = 0;
    COV_INC(dz_m0);
    scn_run(F_DZERO, "dz_m0");

    /* PGMode != 0 */
    dirty_all();
    gPGData.PGMode = 1;
    ref_gIPMZero.FeedPos = 0x0111;
    gRotorTrans.RTPos    = 0x0222;
    gIPMPos.RotorPos     = 0x0333;
    ref_gIPMZero.DetectCnt = 0;
    COV_INC(dz_m1);
    scn_run(F_DZERO, "dz_m1");

    /* DetectCnt == 2000 -> FirstPos */
    dirty_all();
    gPGData.PGMode = 0;
    ref_gIPMZero.FeedPos = 0x0111;
    gIPMPos.RotorPos     = 0x0333;
    ref_gIPMZero.DetectCnt = 2000;
    COV_INC(dz_first);
    scn_run(F_DZERO, "dz_first");

    /* else -> TotalErr */
    dirty_all();
    gPGData.PGMode = 0;
    ref_gIPMZero.DetectCnt = 10;
    ref_gIPMZero.FeedPos = 0x0333;
    ref_gIPMZero.FirstPos = 0x0222;
    gIPMPos.RotorPos = 0x0444;
    COV_INC(dz_acc);
    scn_run(F_DZERO, "dz_acc");
}

/* ========================================================================= */
/* phase 8: SynTunePGZero_No_Load                                           */
/* ========================================================================= */

static void phase_noload(void)
{
    int i;

    /* ---- case 1 ---- */
    /* DetectCnt == 0 */
    dirty_all();
    gGetParVarable.IdSubStep = 1;
    ref_gIPMZero.DetectCnt = 0;
    gMotorInfo.Current = 1000;
    gMotorInfo.CurrentGet = 300;
    COV_INC(nl_1a);
    scn_run(F_NOLOAD, "nl_1a");

    /* same DetectCnt == 0 arm, but InvCurrent < Current so the scaling
     * statement runs.  Current is deliberately small and InvCurrent is chosen
     * so that both a `4096 -> 4097` seed change and a `* -> +` change survive
     * the truncating `/Current`: with the plain nl_1a probe (no scaling) the
     * seed difference is visible but `temp = CurLimit * InvCurrent` never
     * executes at all. */
    dirty_all();
    gGetParVarable.IdSubStep = 1;
    ref_gIPMZero.DetectCnt = 0;
    gMotorInfo.Current = 100;
    gMotorInfo.CurrentGet = 300;
    gInvInfo.InvCurrent = 50;
    COV_INC(nl_1a);
    scn_run(F_NOLOAD, "nl_1a_s");

    /* DetectCnt == 0 with InvCurrent < Current, i.e. with the CurLimit
     * scale-down actually taken: the `*` / `+` swap is invisible when the
     * block is skipped. */
    dirty_all();
    gGetParVarable.IdSubStep = 1;
    ref_gIPMZero.DetectCnt = 0;
    gInvInfo.InvCurrent = 100;
    gMotorInfo.Current = 1000;
    gMotorInfo.CurrentGet = 300;
    COV_INC(nl_1s);
    scn_run(F_NOLOAD, "nl_1s");

    /* DetectCnt == 1 */
    dirty_all();
    gGetParVarable.IdSubStep = 1;
    ref_gIPMZero.DetectCnt = 1;
    COV_INC(nl_1b);
    scn_run(F_NOLOAD, "nl_1b");

    /* neither -> falls to M compare */
    dirty_all();
    gGetParVarable.IdSubStep = 1;
    ref_gIPMZero.DetectCnt = 2;
    COV_INC(nl_1c);
    scn_run(F_NOLOAD, "nl_1c");

    /* M < CurLimit<<12 */
    dirty_all();
    gGetParVarable.IdSubStep = 1;
    ref_gIPMZero.DetectCnt = 1;
    ref_gIPMZero.CurLimit = 1000;
    gIMTSet.M = 0;
    COV_INC(nl_1lt);
    scn_run(F_NOLOAD, "nl_1lt");

    /* M >= CurLimit<<12.  CurLimit is kept *non-zero* so the else-branch
     * assignment `M = CurLimit << 12` produces a value that a shifted-wrong
     * variant cannot reproduce; with CurLimit 0 both forms write 0. */
    dirty_all();
    gGetParVarable.IdSubStep = 1;
    ref_gIPMZero.DetectCnt = 1;
    ref_gIPMZero.CurLimit = 1000;
    gIMTSet.M = 0x40000000L;
    COV_INC(nl_1ge);
    scn_run(F_NOLOAD, "nl_1ge");

    /* M >= CurLimit<<12 with a *non-zero* CurLimit: the `<<12` / `<<11`
     * write-back collapses to 0 on both sides when CurLimit is 0. */
    dirty_all();
    gGetParVarable.IdSubStep = 1;
    ref_gIPMZero.DetectCnt = 1;
    ref_gIPMZero.CurLimit = 1000;
    gIMTSet.M = 0x40000000L;
    COV_INC(nl_1geq);
    scn_run(F_NOLOAD, "nl_1geq");

    /* ---- case 2 ---- */
    /* PGMode == 0, all benign */
    dirty_all();
    gGetParVarable.IdSubStep = 2;
    ref_gIPMZero.DetectCnt = 6000;
    gPGData.PGMode = 0;
    gPGData.PGType = PG_TYPE_ABZ;
    gMockAbDir = DIR_FORWARD;
    gMockUvwDir = DIR_FORWARD;
    COV_INC(nl_2m0);
    scn_run(F_NOLOAD, "nl_2m0");

    /* case 2 at the exact threshold: DetectCnt input 5999, ++ makes it 6000,
     * the `>` / `>=` boundary. */
    dirty_all();
    gGetParVarable.IdSubStep = 2;
    ref_gIPMZero.DetectCnt = 5999;
    gPGData.PGMode = 0;
    gPGData.PGType = PG_TYPE_ABZ;
    gMockAbDir = DIR_FORWARD;
    COV_INC(nl_2eq);
    scn_run(F_NOLOAD, "nl_2eq");

    /* same benign PGMode == 0 arm, but DetectCnt == 5999.  case 2 increments
     * DetectCnt before the test, so this is the exact `> 6000` boundary:
     * 6000 is not > 6000 (skip the arm) yet is >= 6000 (enter it). */
    dirty_all();
    gGetParVarable.IdSubStep = 2;
    ref_gIPMZero.DetectCnt = 5999;
    gPGData.PGMode = 0;
    gPGData.PGType = PG_TYPE_ABZ;
    gMockAbDir = DIR_FORWARD;
    gMockUvwDir = DIR_FORWARD;
    COV_INC(nl_2m0);
    scn_run(F_NOLOAD, "nl_2b");

    /* PGMode != 0, benign */
    dirty_all();
    gGetParVarable.IdSubStep = 2;
    ref_gIPMZero.DetectCnt = 6000;
    gPGData.PGMode = 1;
    gMockRtDir = DIR_FORWARD;
    COV_INC(nl_2m1);
    scn_run(F_NOLOAD, "nl_2m1");

    /* Dir1 == ERROR */
    dirty_all();
    gGetParVarable.IdSubStep = 2;
    ref_gIPMZero.DetectCnt = 6000;
    gPGData.PGMode = 0;
    gMockAbDir = DIR_ERROR;
    COV_INC(nl_2d1e);
    scn_run(F_NOLOAD, "nl_2d1e");

    /* Dir2 == ERROR */
    dirty_all();
    gGetParVarable.IdSubStep = 2;
    ref_gIPMZero.DetectCnt = 6000;
    gPGData.PGMode = 0;
    gPGData.PGType = PG_TYPE_UVW;
    gMockAbDir = DIR_FORWARD;
    gMockUvwDir = DIR_ERROR;
    COV_INC(nl_2d2e);
    scn_run(F_NOLOAD, "nl_2d2e");

    /* Dir2 == BACKWARD */
    dirty_all();
    gGetParVarable.IdSubStep = 2;
    ref_gIPMZero.DetectCnt = 6000;
    gPGData.PGMode = 0;
    gPGData.PGType = PG_TYPE_UVW;
    gMockAbDir = DIR_FORWARD;
    gMockUvwDir = DIR_BACKWARD;
    COV_INC(nl_2d2b);
    scn_run(F_NOLOAD, "nl_2d2b");

    /* Dir1 == BACKWARD */
    dirty_all();
    gGetParVarable.IdSubStep = 2;
    ref_gIPMZero.DetectCnt = 6000;
    gPGData.PGMode = 0;
    gMockAbDir = DIR_BACKWARD;
    COV_INC(nl_2d1b);
    scn_run(F_NOLOAD, "nl_2d1b");

    /* Dir3 == ERROR */
    dirty_all();
    gGetParVarable.IdSubStep = 2;
    ref_gIPMZero.DetectCnt = 6000;
    gPGData.PGMode = 1;
    gMockRtDir = DIR_ERROR;
    COV_INC(nl_2d3e);
    scn_run(F_NOLOAD, "nl_2d3e");

    /* Dir3 == BACKWARD */
    dirty_all();
    gGetParVarable.IdSubStep = 2;
    ref_gIPMZero.DetectCnt = 6000;
    gPGData.PGMode = 1;
    gMockRtDir = DIR_BACKWARD;
    COV_INC(nl_2d3b);
    scn_run(F_NOLOAD, "nl_2d3b");

    /* ---- case 3 ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 3;
    ref_gIPMZero.DetectCnt = 0;
    COV_INC(nl_3);
    scn_run(F_NOLOAD, "nl_3");

    dirty_all();
    gGetParVarable.IdSubStep = 3;
    ref_gIPMZero.DetectCnt = 6000;
    COV_INC(nl_3err);
    scn_run(F_NOLOAD, "nl_3err");

    /* case 3 at the exact threshold: DetectCnt input 5999, ++ makes it 6000,
     * the `>` / `>=` boundary.  Flag is cleared so the separate Flag&1 arm
     * cannot mask the error branch. */
    dirty_all();
    gGetParVarable.IdSubStep = 3;
    ref_gIPMZero.DetectCnt = 5999;
    ref_gIPMZero.Flag = 0;
    COV_INC(nl_3eq);
    scn_run(F_NOLOAD, "nl_3eq");

    /* case 3 increments DetectCnt before the test: 5999 -> exactly 6000, the
     * `> 6000` boundary (`> 6000` false, `>= 6000` true). */
    dirty_all();
    gGetParVarable.IdSubStep = 3;
    ref_gIPMZero.DetectCnt = 5999;
    COV_INC(nl_3err);
    scn_run(F_NOLOAD, "nl_3b");

    dirty_all();
    gGetParVarable.IdSubStep = 3;
    ref_gIPMZero.DetectCnt = 0;
    ref_gIPMZero.Flag = 1;
    COV_INC(nl_3flag);
    scn_run(F_NOLOAD, "nl_3flag");

    /* ---- case 4 ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 4;
    ref_gIPMZero.DetectCnt = 0;
    gPGData.PGType = PG_TYPE_ABZ;
    COV_INC(nl_4);
    scn_run(F_NOLOAD, "nl_4");

    /* window hit */
    dirty_all();
    gGetParVarable.IdSubStep = 4;
    gPGData.PGType = PG_TYPE_UVW;
    ref_gIPMZero.DetectCnt = 100;
    gUVWPG.NewAgl = 60073;
    gUVWPG.UVWAngle = 5461;
    COV_INC(nl_4hit);
    scn_run(F_NOLOAD, "nl_4hit");

    /* DetectCnt == 5461 -> UvwZeroAng */
    dirty_all();
    gGetParVarable.IdSubStep = 4;
    gPGData.PGType = PG_TYPE_ABZ;
    ref_gIPMZero.DetectCnt = 5460;
    COV_INC(nl_4err);
    scn_run(F_NOLOAD, "nl_4err");

    /* DetectCnt > 6096 -> RotorZero wrap */
    dirty_all();
    gGetParVarable.IdSubStep = 4;
    gPGData.PGType = PG_TYPE_ABZ;
    ref_gIPMZero.DetectCnt = 6096;
    COV_INC(nl_4wrap);
    scn_run(F_NOLOAD, "nl_4wrap");

    /* ---- case 5 ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 5;
    ref_gIPMZero.DetectCnt = 0;
    COV_INC(nl_5lt);
    scn_run(F_NOLOAD, "nl_5lt");

    dirty_all();
    gGetParVarable.IdSubStep = 5;
    ref_gIPMZero.DetectCnt = 500;
    gPGData.PGMode = 0;
    gMotorExtInfo.Poles = 4;
    gPGData.PulseNum = 2500;
    COV_INC(nl_5ge);
    COV_INC(nl_5m0);
    scn_run(F_NOLOAD, "nl_5ge_m0");

    dirty_all();
    gGetParVarable.IdSubStep = 5;
    ref_gIPMZero.DetectCnt = 500;
    gPGData.PGMode = 1;
    COV_INC(nl_5m1);
    scn_run(F_NOLOAD, "nl_5m1");

    /* ---- case 6 ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 6;
    ref_gIPMZero.DetectCnt = 0;
    COV_INC(nl_6lt);
    scn_run(F_NOLOAD, "nl_6lt");

    dirty_all();
    gGetParVarable.IdSubStep = 6;
    ref_gIPMZero.DetectCnt = 500;
    COV_INC(nl_6ge);
    scn_run(F_NOLOAD, "nl_6ge");

    /* ---- default ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 7;
    COV_INC(nl_d);
    scn_run(F_NOLOAD, "nl_d");

    (void)i;
}

/* ========================================================================= */
/* phase 9: SynTunePGZero_Load                                              */
/* ========================================================================= */

static void phase_load(void)
{
    /* ---- case 1 ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 1;
    gPGData.PGMode = 0;
    COV_INC(ld_1m0);
    scn_run(F_LOAD, "ld_1m0");

    dirty_all();
    gGetParVarable.IdSubStep = 1;
    gPGData.PGMode = 1;
    gIPMPos.InitPos = 1000;
    gRotorTrans.RTPos = 200;
    COV_INC(ld_1m1);
    scn_run(F_LOAD, "ld_1m1d");

    /* ---- case 2 ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 2;
    COV_INC(ld_2);
    scn_run(F_LOAD, "ld_2");

    /* ---- case 3 ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 3;
    ref_gIPMZero.time = 6000;
    COV_INC(ld_3err);
    scn_run(F_LOAD, "ld_3err");

    /* case 3 at the exact threshold: time input 5999, ++ makes it 6000, the
     * `>=` / `>` boundary. */
    dirty_all();
    gGetParVarable.IdSubStep = 3;
    ref_gIPMZero.time = 5999;
    COV_INC(ld_3eq);
    scn_run(F_LOAD, "ld_3eq");

    /* case 3 increments `time` before the test: 5999 -> exactly 6000, the
     * `>= 6000` boundary (`>= 6000` raises the fault, `> 6000` does not).
     * The fault word is cleared first: under the 0xA5 dirty image
     * ERROR_ENCODER is already set and Fault1 is already 5, so the ref-side
     * write would land on already-set bits and the boundary would be
     * invisible (that is exactly why the `ld_3err` probe above could not
     * catch it). */
    dirty_all();
    gGetParVarable.IdSubStep = 3;
    ref_gIPMZero.time = 5999;
    gError.ErrorCode.all = 0;
    gError.ErrorInfo[4].bit.Fault1 = 0;
    COV_INC(ld_3err);
    scn_run(F_LOAD, "ld_3b");

    dirty_all();
    gGetParVarable.IdSubStep = 3;
    ref_gIPMZero.time = 0;
    gMainCmd.Command.all = 0;          /* SpeedFlag == C_SPEED_FLAG_CON */
    ref_gIPMZero.Flag = 1;
    ref_gIPMZero.DetectCnt = 500;
    COV_INC(ld_3done);
    scn_run(F_LOAD, "ld_3done");

    /* ---- case 4 ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 4;
    gPGData.PGMode = 0;
    gPGData.PGType = PG_TYPE_ABZ;
    COV_INC(ld_4m0);
    scn_run(F_LOAD, "ld_4m0");

    dirty_all();
    gGetParVarable.IdSubStep = 4;
    gPGData.PGMode = 0;
    gPGData.PGType = PG_TYPE_UVW;
    gPGDir.UVWDirCnt = 3;
    gMockUvwDir = DIR_FORWARD;
    COV_INC(ld_4uvw);
    scn_run(F_LOAD, "ld_4uvw");

    dirty_all();
    gGetParVarable.IdSubStep = 4;
    gPGData.PGMode = 0;
    gPGData.PGType = PG_TYPE_ABZ;
    gPGDir.ABDirCnt = 4;
    COV_INC(ld_4ab);
    scn_run(F_LOAD, "ld_4ab");

    dirty_all();
    gGetParVarable.IdSubStep = 4;
    gPGData.PGMode = 0;
    gPGData.PGType = PG_TYPE_UVW;
    gPGDir.ABDirCnt = 4;
    gPGDir.UVWDirCnt = 4;
    COV_INC(ld_4abuvw);
    scn_run(F_LOAD, "ld_4abuvw");

    dirty_all();
    gGetParVarable.IdSubStep = 4;
    gPGData.PGMode = 1;
    gPGDir.RtDirCnt = 0;
    COV_INC(ld_4m1);
    scn_run(F_LOAD, "ld_4m1");

    dirty_all();
    gGetParVarable.IdSubStep = 4;
    gPGData.PGMode = 1;
    gPGDir.RtDirCnt = 6;
    COV_INC(ld_4rt);
    scn_run(F_LOAD, "ld_4rt");

    /* ---- case 5 ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 5;
    gPGData.PGType = PG_TYPE_UVW;
    gUVWPG.UvwEstStep = 0;
    ref_gIPMZero.DetectCnt = 0;
    COV_INC(ld_5e0);
    scn_run(F_LOAD, "ld_5e0");

    dirty_all();
    gGetParVarable.IdSubStep = 5;
    gPGData.PGType = PG_TYPE_UVW;
    gUVWPG.UvwEstStep = 2;
    gUVWPG.NewAgl = 0;
    gUVWPG.UVWAngle = 0;
    ref_gIPMZero.DetectCnt = 0;
    COV_INC(ld_5e2);
    scn_run(F_LOAD, "ld_5e2");

    dirty_all();
    gGetParVarable.IdSubStep = 5;
    gPGData.PGType = PG_TYPE_UVW;
    gUVWPG.UvwEstStep = 3;
    gUVWPG.ErrAgl = 111;
    gUVWPG.NewAgl = 222;
    ref_gIPMZero.DetectCnt = 0;
    COV_INC(ld_5e3);
    scn_run(F_LOAD, "ld_5e3d");

    /* Flag&1, DetectCnt == 0 */
    dirty_all();
    gGetParVarable.IdSubStep = 5;
    gPGData.PGType = PG_TYPE_ABZ;
    ref_gIPMZero.Flag = 1;
    ref_gIPMZero.DetectCnt = 0;
    COV_INC(ld_5f0);
    scn_run(F_LOAD, "ld_5f0");

    /* Flag&1, 0 < DetectCnt < 5 */
    dirty_all();
    gGetParVarable.IdSubStep = 5;
    gPGData.PGType = PG_TYPE_ABZ;
    ref_gIPMZero.Flag = 1;
    ref_gIPMZero.DetectCnt = 3;
    COV_INC(ld_5flt);
    scn_run(F_LOAD, "ld_5flt");

    /* Flag&1, DetectCnt == 5 */
    dirty_all();
    gGetParVarable.IdSubStep = 5;
    gPGData.PGType = PG_TYPE_UVW;
    gUVWPG.UvwEstStep = 0;
    ref_gIPMZero.Flag = 1;
    ref_gIPMZero.DetectCnt = 5;
    COV_INC(ld_5feq);
    scn_run(F_LOAD, "ld_5feq");

    /* DetectCnt > 5 and the >5 exit guard true */
    dirty_all();
    gGetParVarable.IdSubStep = 5;
    gPGData.PGType = PG_TYPE_ABZ;
    ref_gIPMZero.Flag = 0;
    ref_gIPMZero.DetectCnt = 6;
    COV_INC(ld_5done);
    scn_run(F_LOAD, "ld_5done");

    /* ---- case 6 ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 6;
    gMainCmd.FreqSet = 0;
    COV_INC(ld_6in);
    scn_run(F_LOAD, "ld_6in");

    dirty_all();
    gGetParVarable.IdSubStep = 6;
    gMainCmd.FreqSet = 100;
    COV_INC(ld_6out);
    scn_run(F_LOAD, "ld_6out");

    dirty_all();
    gGetParVarable.IdSubStep = 6;
    gMainCmd.FreqSet = 0;
    ref_gIPMZero.DetectCnt = 250;
    COV_INC(ld_6done);
    scn_run(F_LOAD, "ld_6done");

    /* ---- case 7 ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 7;
    ref_gIPMZero.DetectCnt = 0;
    COV_INC(ld_7lt);
    scn_run(F_LOAD, "ld_7lt");

    dirty_all();
    gGetParVarable.IdSubStep = 7;
    ref_gIPMZero.DetectCnt = 500;
    gMotorExtInfo.Poles = 4;
    gPGData.PulseNum = 2500;
    COV_INC(ld_7ge);
    scn_run(F_LOAD, "ld_7ge");

    /* ---- default ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 0;
    COV_INC(ld_d);
    scn_run(F_LOAD, "ld_d");
}

/* ========================================================================= */
/* phase 10: SynTuneBemf                                                     */
/* ========================================================================= */

static void phase_bemf(void)
{
    /* ---- case 1 ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 1;
    gInvInfo.InvCurrent = 100;
    gMotorInfo.Current = 1000;
    COV_INC(be_1sc);
    scn_run(F_BEMF, "be_1sc");

    dirty_all();
    gGetParVarable.IdSubStep = 1;
    gInvInfo.InvCurrent = 1000;
    gMotorInfo.Current = 1000;
    COV_INC(be_1no);
    scn_run(F_BEMF, "be_1no");

    dirty_all();
    gGetParVarable.IdSubStep = 1;
    gInvInfo.InvTypeApply = 20;
    COV_INC(be_1lt);
    scn_run(F_BEMF, "be_1lt");

    dirty_all();
    gGetParVarable.IdSubStep = 1;
    gInvInfo.InvTypeApply = 21;
    COV_INC(be_1ge);
    scn_run(F_BEMF, "be_1ge");

    /* ---- case 2 ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 2;
    ref_gEstBemf.AccDecTick = 1;
    ref_gEstBemf.TuneFreqSet = 0;
    ref_gEstBemf.TuneFreqAim = 100;
    ref_gEstBemf.Cnt = 0;
    COV_INC(be_2lt);
    scn_run(F_BEMF, "be_2lt");

    dirty_all();
    gGetParVarable.IdSubStep = 2;
    ref_gEstBemf.AccDecTick = 1;
    ref_gEstBemf.TuneFreqSet = 100;
    ref_gEstBemf.TuneFreqAim = 0;
    ref_gEstBemf.Cnt = 0;
    COV_INC(be_2ge);
    scn_run(F_BEMF, "be_2ge");

    dirty_all();
    gGetParVarable.IdSubStep = 2;
    ref_gEstBemf.AccDecTick = 1;
    ref_gEstBemf.TuneFreqSet = 100;
    ref_gEstBemf.TuneFreqAim = 0;
    ref_gEstBemf.Cnt = 500;
    COV_INC(be_2done);
    scn_run(F_BEMF, "be_2done");

    /* ---- case 3 ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 3;
    ref_gEstBemf.Cnt = 0;
    gLineCur.CurPer = 100;
    gOutVolt.VoltApply = 200;
    COV_INC(be_3acc);
    scn_run(F_BEMF, "be_3acc");

    dirty_all();
    gGetParVarable.IdSubStep = 3;
    ref_gEstBemf.Cnt = 2048;
    COV_INC(be_3done);
    scn_run(F_BEMF, "be_3done");

    /* case 3 at the exact threshold: Cnt input 2047, ++ makes it 2048, the
     * `<=` / `<` boundary. */
    dirty_all();
    gGetParVarable.IdSubStep = 3;
    ref_gEstBemf.Cnt = 2047;
    gLineCur.CurPer = 100;
    gOutVolt.VoltApply = 200;
    COV_INC(be_3eq);
    scn_run(F_BEMF, "be_3eq");

    /* case 3 increments Cnt before the test: 2047 -> exactly 2048, the
     * `<= 2048` boundary (`<= 2048` keeps accumulating, `< 2048` folds). */
    dirty_all();
    gGetParVarable.IdSubStep = 3;
    ref_gEstBemf.Cnt = 2047;
    gLineCur.CurPer = 100;
    gOutVolt.VoltApply = 200;
    COV_INC(be_3done);
    scn_run(F_BEMF, "be_3b");

    /* ---- case 4 ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 4;
    gInvInfo.InvCurrent = 100;
    gMotorInfo.Current = 1000;
    ref_gEstBemf.Cnt = 0;
    COV_INC(be_4sc);
    scn_run(F_BEMF, "be_4sc");

    dirty_all();
    gGetParVarable.IdSubStep = 4;
    gInvInfo.InvCurrent = 1000;
    gMotorInfo.Current = 1000;
    ref_gEstBemf.Cnt = 0;
    COV_INC(be_4no);
    scn_run(F_BEMF, "be_4no");

    dirty_all();
    gGetParVarable.IdSubStep = 4;
    gInvInfo.InvCurrent = 1000;
    gMotorInfo.Current = 1000;
    ref_gEstBemf.Cnt = 500;
    COV_INC(be_4done);
    scn_run(F_BEMF, "be_4done");

    /* ---- case 5 ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 5;
    ref_gEstBemf.Cnt = 0;
    gLineCur.CurPer = 100;
    gOutVolt.VoltApply = 200;
    COV_INC(be_5acc);
    scn_run(F_BEMF, "be_5acc");

    dirty_all();
    gGetParVarable.IdSubStep = 5;
    ref_gEstBemf.Cnt = 2048;
    COV_INC(be_5done);
    scn_run(F_BEMF, "be_5done");

    /* ---- case 6 ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 6;
    ref_gEstBemf.AccDecTick = 1;
    ref_gEstBemf.TuneFreqSet = 100;
    ref_gEstBemf.TuneFreqAim = 0;
    COV_INC(be_6gt);
    scn_run(F_BEMF, "be_6gt");

    dirty_all();
    gGetParVarable.IdSubStep = 6;
    ref_gEstBemf.AccDecTick = 1;
    ref_gEstBemf.TuneFreqSet = 0;
    ref_gEstBemf.TuneFreqAim = 100;
    COV_INC(be_6le);
    scn_run(F_BEMF, "be_6le");

    /* ---- case 7 ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 7;
    ref_gEstBemf.Cnt = 0;
    COV_INC(be_7acc);
    scn_run(F_BEMF, "be_7acc");

    dirty_all();
    gGetParVarable.IdSubStep = 7;
    ref_gEstBemf.Cnt = 500;
    COV_INC(be_7done);
    scn_run(F_BEMF, "be_7done");

    /* ---- case 8 ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 8;
    ref_gEstBemf.TotalId1 = 0;
    ref_gEstBemf.TotalId2 = 1000;
    ref_gEstBemf.TuneFreq = 1;
    ref_gEstBemf.TotalVq1 = 500;
    ref_gEstBemf.TotalVq2 = 900;
    gMotorInfo.FreqPer = 100;
    gMotorInfo.Votage = 540;
    gMotorExtInfo.Poles = 4;
    gPGData.PulseNum = 2500;
    COV_INC(be_8);
    scn_run(F_BEMF, "be_8");

    /* ---- default ---- */
    dirty_all();
    gGetParVarable.IdSubStep = 9;
    COV_INC(be_d);
    scn_run(F_BEMF, "be_d");
}

/* ========================================================================= */
/* phase 11: random sweep                                                    */
/* ========================================================================= */

static unsigned long g_seed = 0x20261019UL;

static unsigned long rnd(void)
{
    g_seed = g_seed * 1103515245UL + 12345UL;
    return (g_seed >> 8) & 0xFFFFFFUL;
}

static void phase_random(long n)
{
    long k;

    for (k = 0; k < n; k++)
    {
        int fn = (int)(rnd() % F_COUNT);

        dirty_all();

        /* module-private read/modify state */
        ref_gIPMInitPos.Step      = (Uint)(rnd() % 9);
        ref_gIPMInitPos.PeriodCnt = (Uint)(rnd() % 7);
        ref_gIPMInitPos.Section   = (Uint)(rnd() % 8);
        ref_gIPMInitPos.PhsChkStep = (int)(rnd() % 7);
        ref_gIPMInitPos.CurFirst  = (int)(short)rnd();
        ref_gIPMInitPos.PWMTs     = (Uint)rnd();
        ref_gIPMInitPos.InitPWMTs = (Uint)rnd();
        ref_gIPMInitPos.Waite     = (Uint)rnd();
        ref_gIPMInitPos.Flag      = (Uint)rnd();
        ref_gIPMInitPos.CurLimit  = (Uint)rnd();
        ref_gIPMInitPos.Ld        = (Uint)rnd();
        ref_gIPMInitPos.Lq        = (Uint)rnd();
        {
            int i;
            for (i = 0; i < 12; i++)
            {
                ref_gIPMInitPos.Cur[i] = (int)(short)rnd();
            }
        }

        ref_gIPMZero.DetectCnt = (int)(rnd() % 7000) - 100;
        ref_gIPMZero.Flag      = (Uint)rnd();
        ref_gIPMZero.CurLimit  = (Uint)rnd();
        ref_gIPMZero.time      = (Uint)rnd();
        ref_gIPMZero.FeedPos   = (Uint)rnd();
        ref_gIPMZero.FirstPos  = (Uint)rnd();
        ref_gIPMZero.TotalErr  = (long)(int)(short)rnd();

        ref_gEstBemf.TotalId1  = (Ulong)rnd();
        ref_gEstBemf.TotalId2  = (Ulong)rnd();
        ref_gEstBemf.TotalVq1  = (Ulong)rnd();
        ref_gEstBemf.TotalVq2  = (Ulong)rnd();
        ref_gEstBemf.TuneFreq  = (Uint)rnd();
        ref_gEstBemf.IdSet     = (int)(short)rnd();
        ref_gEstBemf.IqSet     = (int)(short)rnd();
        ref_gEstBemf.TuneFreqSet = (int)(short)rnd();
        ref_gEstBemf.TuneFreqAim = (int)(short)rnd();
        ref_gEstBemf.AccDecTick = (Ulong)rnd();
        ref_gEstBemf.FreqStep  = (int)(short)rnd();
        ref_gEstBemf.FreqRem   = (long)(int)(short)rnd();
        ref_gEstBemf.Cnt       = (Uint)rnd();

        ref_gPmParEst.UvwDir   = (Uint)rnd();

        /* shared input space */
        gGetParVarable.IdSubStep = (Uint16)(rnd() % 9);
        gGetParVarable.ParEstMstep = (Uint16)(rnd() % 8);

        gInvInfo.InvCurrent   = (Uint)rnd();
        gInvInfo.InvTypeApply = (Uint)rnd();

        gMotorInfo.Current    = (Uint)(rnd() % 900) + 1;
        gMotorInfo.CurrentGet = (Uint)rnd();
        gMotorInfo.FreqPer    = (Uint)rnd();
        gMotorInfo.Votage     = (Uint)(rnd() | 1);

        gMotorExtInfo.Poles   = (Uint)(rnd() % 8) + 1;
        gMotorExtInfo.RsPm    = (Uint)rnd();
        gMotorExtReg.RsPm     = (Uint)rnd();

        gPGData.PGMode        = (Uint)(rnd() % 2);
        gPGData.PGType        = (Uint)(rnd() % 2);
        gPGData.PulseNum      = (Uint)(rnd() | 1);

        gUVWPG.NewAgl         = (Uint)rnd();
        gUVWPG.UVWAngle       = (Uint)rnd();
        gUVWPG.lastAgl        = (Uint)rnd();
        gUVWPG.ErrAgl         = (Uint)rnd();
        gUVWPG.TuneFlag       = (Uint)rnd();
        gUVWPG.UvwEstStep     = (Uint)(rnd() % 6);

        gPGDir.ABDirCnt       = (int)(short)rnd();
        gPGDir.UVWDirCnt      = (int)(short)rnd();
        gPGDir.RtDirCnt       = (int)(short)rnd();
        gPGDir.ABAngleBak     = (int)(short)rnd();
        gPGDir.UVWAngleBak    = (int)(short)rnd();

        gIPMPos.RotorPos      = (Uint)rnd();
        gIPMPos.InitPos       = (Uint)rnd();
        gIPMPos.RotorZero     = (Uint)rnd();
        gIPMPos.PosInZInfo    = (Uint)rnd();

        gIMTSet.M             = (long)(int)(short)rnd();

        gMainCmd.Command.all  = (Uint)rnd();
        gMainCmd.FreqSet      = (int)(short)rnd();
        gMainCmd.FreqSyn      = (int)(short)rnd();

        gMainStatus.RunStep   = (Uint)(rnd() % 8);
        gMainStatus.SubStep   = (Uint)(rnd() % 8);

        gLineCur.CurPer       = (Uint)rnd();
        gOutVolt.VoltApply    = (int)(short)rnd();

        gUDC.uDCFilter        = (Uint)rnd();

        gIUVWQ24.U            = (long)(int)(short)rnd();
        gIUVWQ24.V            = (long)(int)(short)rnd();
        gIUVWQ24.W            = (long)(int)(short)rnd();

        gADC.DelayApply       = (Uint)rnd();

        gMockAbDir  = (unsigned short)(rnd() % 3);
        gMockUvwDir = (unsigned short)(rnd() % 3);
        gMockRtDir  = (unsigned short)(rnd() % 3);

        gHostEQep.QPOSCNT = (Uint)rnd();

        /* F_SETPWM takes its argument from the driver-local holder */
        gIPMInitPos.Section = (Uint)(rnd() % 8);

        scn_run(fn, "random");
    }
}

/* ========================================================================= */
/* names + main                                                              */
/* ========================================================================= */

static const char *g_fnName[F_COUNT] = {
    "InitSetPosTune", "SynTuneInitPos", "SynInitPosDetect", "SetIPMPos",
    "SetIPMPos_ABZRef", "SynCalLabAndLbc", "SynInitPosDetCal",
    "SynInitPosDetSetTs", "SynInitPosDetSetPwm", "DetectZeroPosOnce",
    "SynTunePGZero_No_Load", "SynTunePGZero_Load", "SynTuneBemf"
};

static const char *g_covName[COV_COUNT] = {
#define COV_NAME(name) #name,
    COV_LIST(COV_NAME)
#undef COV_NAME
};

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 20000;
    long hand;
    int i;
    int bad = 0;

    g_seed = 0x20261019UL;

    phase_itune();
    phase_tip();
    phase_det();
    phase_setpos();
    phase_calld();
    phase_detcal();
    phase_setts();
    phase_dzero();
    phase_noload();
    phase_load();
    phase_bemf();

    hand = g_cases;
    phase_random(n);

    printf("  hand=%ld random=%ld\n", hand, g_cases - hand);

    for (i = 0; i < F_COUNT; i++)
    {
        if (g_fnCases[i] <= 0)
        {
            printf("UNREACHED fn: %s\n", g_fnName[i]);
            bad = 1;
        }
    }
    for (i = 0; i < COV_COUNT; i++)
    {
        if (g_cov[i] <= 0)
        {
            printf("UNREACHED cov: %s\n", g_covName[i]);
            bad = 1;
        }
    }

    printf("MotorPmsmParEst difftest: cases=%ld mismatches=%ld\n",
           g_cases, g_mism);

    if (bad)
    {
        return 2;
    }
    return (g_mism == 0) ? 0 : 1;
}
