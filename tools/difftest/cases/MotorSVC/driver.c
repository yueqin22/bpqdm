/* Differential-test driver for MotorSVC.
 *
 * The frozen baseline (cases/MotorSVC/ref/MotorSVC_ref.c) and the delivered
 * source (src/motor/MotorSVC.c) are compiled into two symbol namespaces,
 * ref_* and new_*.  MotorSVC.c defines 8 data symbols of its own
 * (gFluxR / gFluxS / gCalFlux / gSVCSpeed / gABFluxS_LowFilter / gABFluxS_comp
 * / gIMTQ12_obs / gIMTQ24_obs); the four entry points plus the six live state
 * globals are renamed per side, so those six are per-side private memory,
 * while the 14 globals the module reads/writes but does not define are ONE
 * shared instance provided by mock_svc.c.
 *
 * ---------------------------------------------------------------------------
 * What is compared, per call
 * ---------------------------------------------------------------------------
 *   - the six per-side state globals, whole struct, byte for byte
 *     (that is where the module's output lives: gFluxR, gFluxS, gCalFlux,
 *     gSVCSpeed, gABFluxS_LowFilter, gABFluxS_comp);
 *   - the 14 shared globals after the call (gMainCmd.FreqFeed / FreqReal /
 *     FirstCnt, gABVoltSet, ...), whole struct, byte for byte;
 *   - gHostCpuCtl[7], the DINT/EINT pair inside SVCCalRotorSpeed.
 *
 * ---------------------------------------------------------------------------
 * Division-by-zero is a real input-space hazard, not a style point
 * ---------------------------------------------------------------------------
 * Nine divisions take a field as the divisor, and a zero there does not
 * produce a "mismatch" -- it kills the process (MSVC #DE) and silently
 * destroys the whole ledger:
 *     SVCCalFlux_380     / gBasePar.FcSetApply
 *                        / gMotorExtPer.LM     (inside m_LrDivLm)
 *                        / gBasePar.FullFreq01 (twice)
 *                        / gMotorExtPer.L1     (SvcMode != 0 branch)
 *     SVCCalRotorSpeed   / gBasePar.FullFreq01
 *                        / m_DetaTimer          (== gSVCSpeed.DetaTimer)
 *                        / gMotorExtPer.I0  or  / (gIMTSetApply.M >> 12)
 *                        / gMotorExtPer.L1
 *     SVCCalRotorSpeed_380 / abs(gMainCmd.FreqSyn)
 *                        / (gIMTSet.M >> 12)
 * Every scenario pins those non-zero (pin_*() below); the hand-built phases
 * re-pin after their own pokes so no draw can turn a division into a crash.
 *
 * One divisor cannot be pinned without deleting a branch: in
 * SVCCalRotorSpeed_380 the divisor abs(gMainCmd.FreqSyn) is unconditional,
 * yet gMainCmd.FreqSyn == 0 is also a branch condition of that same function
 * (`if (gMainCmd.FreqSyn == 0) SvcRotorSpeed = 0; else SvcSignal = 1;`).
 * On the target a division by zero does not trap, so the arm is live there;
 * on the host it kills the process.  The bench therefore pins FreqSyn != 0 on
 * every _380 call and records the FreqSyn == 0 arm as an open coverage gap
 * (target-only).  g_cov380NoSyn counts how often it was taken and must be 0.
 *
 * ---------------------------------------------------------------------------
 * The one uninitialized local in the whole tree
 * ---------------------------------------------------------------------------
 * SVCCalRotorSpeed_380 line 340 reads `m_Speed` before assigning it (the
 * non-380 twin assigns it first -- a pre-existing firmware bug, kept by
 * contract).  What it reads is whatever the callee frame happens to contain,
 * which is not a semantic input, so it is pinned: before every call the driver
 * fills 8 KB of stack below its own frame with a constant.  Whatever slot the
 * compiler picks for m_Speed then holds the same 32-bit pattern on both sides,
 * and the comparison is about the EXPRESSION (order of operations, widths,
 * shifts), not about garbage.  A CONSTANT fill is required: a
 * position-dependent fill would put different values in the same slot if the
 * two sides lay their frames out differently.
 *
 * ---------------------------------------------------------------------------
 * Width pins
 * ---------------------------------------------------------------------------
 * The case shim folds `int` to the target's 16 bits so int/Uint storage
 * matches the C28x.  main() asserts the widths and fails loudly if the fold is
 * ever lost, because without it several truncation-only mutants would hide:
 * m_DetaTimer = ((Timer - GetTime()) & 0xFFFFFF) >> 3 truncated to 16 bits,
 * DetaPhase = gFluxR.Theta - SvcLastFluxPos wrapping at 16 bits, and
 * (Uint)(IMPhase >> 16) in ResetSVC.  bench_selfcheck() then verifies a few of
 * those truncations against an independently computed value, so a bench that
 * silently stopped pinning them would fail on its own.
 *
 * ---------------------------------------------------------------------------
 * 0xA5 discipline
 * ---------------------------------------------------------------------------
 * Every compared word starts at 0xA5 before each scenario, so a mutation that
 * makes the new side STOP writing a field is visible instead of masked by a
 * pre-existing equal value.
 */

#include <stdio.h>
#include <string.h>

/* <stdlib.h> is deliberately NOT included: the case shim folds `int` to the
 * target's 16 bits and stdlib's 32-bit prototypes would then collide with the
 * project's folded ones.  Only atol is needed, declared by hand. */
extern long atol(const char *);

#include "MotorInclude.h"

/* ---- renamed entry points ------------------------------------------------ */

extern void ref_ResetSVC(void);
extern void ref_SVCCalFlux_380(void);
extern void ref_SVCCalRotorSpeed(void);
extern void ref_SVCCalRotorSpeed_380(void);

extern void new_ResetSVC(void);
extern void new_SVCCalFlux_380(void);
extern void new_SVCCalRotorSpeed(void);
extern void new_SVCCalRotorSpeed_380(void);

/* ---- renamed state globals ------------------------------------------------ */

extern FLUX_STRUCT         ref_gFluxR;
extern FLUX_STRUCT         ref_gFluxS;
extern SVC_FLUX_CAL_STRUCT ref_gCalFlux;
extern SVC_SPEED_STRUCT    ref_gSVCSpeed;
extern ALPHABETA_STRUCT    ref_gABFluxS_LowFilter;
extern ALPHABETA_STRUCT    ref_gABFluxS_comp;
extern MT_STRUCT_Q24       ref_gIMTQ24_obs;
extern MT_STRUCT_Q24       ref_gIMTQ12_obs;

extern FLUX_STRUCT         new_gFluxR;
extern FLUX_STRUCT         new_gFluxS;
extern SVC_FLUX_CAL_STRUCT new_gCalFlux;
extern SVC_SPEED_STRUCT    new_gSVCSpeed;
extern ALPHABETA_STRUCT    new_gABFluxS_LowFilter;
extern ALPHABETA_STRUCT    new_gABFluxS_comp;
extern MT_STRUCT_Q24       new_gIMTQ24_obs;
extern MT_STRUCT_Q24       new_gIMTQ12_obs;

/* ---- mock-owned ---------------------------------------------------------- */

extern volatile Uint16     gHostCpuCtl[7];
extern volatile struct CPUTIMER_REGS gHostCpuTimer1Regs;

/* ---- the shared input image: the 14 mock-owned globals ------------------- */
/* Order and types are identical to mock_svc.c's definitions.  X(name, type)
 * must stay a macro so the struct, the save, the load and the compare are all
 * generated from this single list. */

#define IMG_LIST(X)                                                        \
    X(gMainCmd,           BASE_COMMAND_STRUCT)                             \
    X(gPhase,             ANGLE_STRUCT)                                    \
    X(gBasePar,           BASE_PAR_STRUCT)                                 \
    X(gVCPar,             VC_INFO_STRUCT)                                  \
    X(gMotorExtPer,       MOTOR_EXTERN_STRUCT)                             \
    X(gIAlphBetaQ12,      ALPHABETA_STRUCT)                                \
    X(gIMTSetApply,       MT_STRUCT_Q24)                                   \
    X(gIMTSet,            MT_STRUCT_Q24)                                   \
    X(gIMTQ12,            MT_STRUCT)                                       \
    X(gTestDataReceive,   MOTOR_DEBUG_DATA_RECEIVE_STRUCT)                 \
    X(gInvInfo,           INV_STRUCT)                                      \
    X(gRatio,             Uint)                                            \
    X(gMotorInfo,         MOTOR_STRUCT)                                    \
    X(gABVoltSet,         ALPHABETA_STRUCT)

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

/* ---- the per-side state image: the six renamed state globals ------------- */

#define ST_LIST(X)                                                         \
    X(gFluxR,             FLUX_STRUCT)                                     \
    X(gFluxS,             FLUX_STRUCT)                                     \
    X(gCalFlux,           SVC_FLUX_CAL_STRUCT)                             \
    X(gSVCSpeed,          SVC_SPEED_STRUCT)                                \
    X(gABFluxS_LowFilter, ALPHABETA_STRUCT)                                \
    X(gABFluxS_comp,      ALPHABETA_STRUCT)                                \
    X(gIMTQ24_obs,        MT_STRUCT_Q24)                                   \
    X(gIMTQ12_obs,        MT_STRUCT_Q24)

struct STATE {
#define ST_MEM(name, type) type name;
    ST_LIST(ST_MEM)
#undef ST_MEM
};

static struct STATE stIn;
static struct STATE stRef;
static struct STATE stNew;

static void st_load_ref(const struct STATE *s)
{
#define ST_LREF(name, type) ref_##name = s->name;
    ST_LIST(ST_LREF)
#undef ST_LREF
}

static void st_load_new(const struct STATE *s)
{
#define ST_LNEW(name, type) new_##name = s->name;
    ST_LIST(ST_LNEW)
#undef ST_LNEW
}

static void st_snap_ref(struct STATE *s)
{
#define ST_SREF(name, type) s->name = ref_##name;
    ST_LIST(ST_SREF)
#undef ST_SREF
}

static void st_snap_new(struct STATE *s)
{
#define ST_SNEW(name, type) s->name = new_##name;
    ST_LIST(ST_SNEW)
#undef ST_SNEW
}

/* ---- counters ------------------------------------------------------------ */

static long g_cases;
static long g_mism;
static long g_mismShown;
static unsigned long g_seedAcc;
static Ulong g_seqTimerStep = 64UL;

/* input-reach counters, asserted at the end (see main) */
static long g_covFn[4];
static long g_covSvcMode[2];
static long g_covFluxTimerTrunc;   /* the 16-bit DetaTimer += wrapped          */
static long g_covFluxTimerWrap2;   /* ... in the negative direction too         */
static long g_covFluxComp;         /* the SvcMode != 0 comp branch ran        */
static long g_covFirst[3];         /* FirstCnt: <10 / 10..999 / >=1000        */
static long g_covInv22;            /* InvTypeApply >= 22                     */
static long g_covRatioClamp;       /* gRatio < m_LowVolt                     */
static long g_covIoBranch;         /* I0 > (M >> 12)                         */
static long g_covSynZero;          /* SvcSignal==0 && FreqSyn==0             */
static long g_covSynSet;           /* SvcSignal set to 1                     */
static long g_covSignFlip;         /* FreqSet * m_Speed < 0                  */
static long g_covFilterCtl;        /* VCSpeedFilter <= 1                     */
static long g_covFilterAvg;        /* VCSpeedFilter > 1                      */
static long g_covSpeedZero;        /* m_Speed == 0 reached                   */
static long g_covFreqZeroFilter; /* FreqSyn==0 且 VCF>1 臂           */
static long g_covIqSat[5];         /* >Max / ==Max / inside / ==Min / <Min    */
static long g_covClamp1024;        /* m_ULong > 1024 clamp in _380           */
static long g_covIntData[3];       /* ternary: < / == / >                    */
static long g_cov380NoSyn;         /* must stay 0 (target-only branch)       */
static long g_covCpuCtl;           /* the DINT/EINT pair ran                 */
static long g_covSeq;              /* sequence phase steps                   */
static long g_covUninitSpeed;      /* the m_Speed-before-assign arm ran      */

/* ---- comparison ---------------------------------------------------------- */

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

static unsigned short refCpu[7];
static unsigned short newCpu[7];

static void compare(const char *tag)
{
#define ST_CMP(name, type) CMPM(stRef.name, stNew.name, #name);
    ST_LIST(ST_CMP)
#undef ST_CMP

    CMPM(refCpu, newCpu, "gHostCpuCtl");
}

static void compare_img(const char *tag)
{
#define IMG_CMP(name, type) CMPM(imgRef.name, imgNew.name, #name);
    IMG_LIST(IMG_CMP)
#undef IMG_CMP
}

/* ---- stack seeding for the one uninitialized read ------------------------ */

static void seed_stack(void)
{
    volatile unsigned char pad[8192];
    unsigned int i;

    for (i = 0; i < sizeof(pad); i++)
    {
        pad[i] = 0x5Au;
    }
    g_seedAcc += (unsigned long)(pad[0] + pad[sizeof(pad) - 1]);
}

static void zero_counters(void)
{
    memset((void *)gHostCpuCtl, 0, sizeof(Uint16) * 7U);
}

/* ---- one call on one side ------------------------------------------------ */

static void call_side(int side, int fn)
{
    if (side == 0)
    {
        switch (fn)
        {
            case 0: ref_ResetSVC(); break;
            case 1: ref_SVCCalFlux_380(); break;
            case 2: ref_SVCCalRotorSpeed(); break;
            default: ref_SVCCalRotorSpeed_380(); break;
        }
    }
    else
    {
        switch (fn)
        {
            case 0: new_ResetSVC(); break;
            case 1: new_SVCCalFlux_380(); break;
            case 2: new_SVCCalRotorSpeed(); break;
            default: new_SVCCalRotorSpeed_380(); break;
        }
    }
}

/* One isolated call: the same state image and the same timer go to both
 * sides, the shared globals are restored in between, and both outputs are
 * compared field by field. */
static void do_step(int fn, Ulong timerVal, const char *tag)
{
    g_cases++;
    g_covFn[fn]++;

    /* ref side */
    img_load(&imgIn);
    st_load_ref(&stIn);
    gHostCpuTimer1Regs.TIM.all = timerVal;
    zero_counters();
    seed_stack();
    call_side(0, fn);
    img_save(&imgRef);
    st_snap_ref(&stRef);
    memcpy(refCpu, (const void *)gHostCpuCtl, sizeof(refCpu));

    /* new side */
    img_load(&imgIn);
    st_load_new(&stIn);
    gHostCpuTimer1Regs.TIM.all = timerVal;
    zero_counters();
    seed_stack();
    call_side(1, fn);
    img_save(&imgNew);
    st_snap_new(&stNew);
    memcpy(newCpu, (const void *)gHostCpuCtl, sizeof(newCpu));

    if (refCpu[0] != 0U || refCpu[1] != 0U)
    {
        g_covCpuCtl++;
    }

    compare(tag);
    compare_img(tag);
}

/* ---- deterministic-random helpers --------------------------------------- */

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

/* Separate stream for seeding inputs: a scenario's inputs depend only on that
 * scenario's seed, not on how many rnd() calls the phases made. */
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
    unsigned long span;

    if (hi <= lo)
    {
        return lo;
    }
    /* unsigned arithmetic: the 32-bit spans used here overflow signed long */
    span = ((unsigned long)hi - (unsigned long)lo) + 1UL;
    return lo + (long)(drnd() % span);
}

/* ---- divisor pins ------------------------------------------------------- */

static void pin_for_flux(void)
{
    if (imgIn.gBasePar.FcSetApply == 0U)   imgIn.gBasePar.FcSetApply = 2U;
    if (imgIn.gMotorExtPer.LM == 0U)       imgIn.gMotorExtPer.LM = 2U;
    if (imgIn.gMotorExtPer.L1 == 0U)       imgIn.gMotorExtPer.L1 = 2U;
    if (imgIn.gBasePar.FullFreq01 == 0UL)  imgIn.gBasePar.FullFreq01 = 3000UL;
}

static void pin_for_speed(void)
{
    if (imgIn.gBasePar.FullFreq01 == 0UL)  imgIn.gBasePar.FullFreq01 = 3000UL;
    if (imgIn.gMotorExtPer.L1 == 0U)       imgIn.gMotorExtPer.L1 = 2U;
    if (stIn.gSVCSpeed.DetaTimer == 0)    stIn.gSVCSpeed.DetaTimer = 1;
    if (imgIn.gMotorExtPer.I0 == 0U)       imgIn.gMotorExtPer.I0 = 2U;
    if (imgIn.gIMTSetApply.M > -4096L && imgIn.gIMTSetApply.M < 4096L) imgIn.gIMTSetApply.M = 4096L;
}

static void pin_for_speed380(void)
{
    if (imgIn.gBasePar.FullFreq01 == 0UL)  imgIn.gBasePar.FullFreq01 = 3000UL;
    if (imgIn.gMotorExtPer.L1 == 0U)       imgIn.gMotorExtPer.L1 = 2U;
    if (imgIn.gMainCmd.FreqSyn == 0)       imgIn.gMainCmd.FreqSyn = 1;
    if (imgIn.gIMTSet.M > -4096L && imgIn.gIMTSet.M < 4096L) imgIn.gIMTSet.M = 4096L;
}

/* ---- scenario setup ----------------------------------------------------- */

static void begin_scenario(unsigned long seed)
{
    long i;

    memset(&imgIn, 0, sizeof(imgIn));
    memset(&stIn, 0, sizeof(stIn));

    /* 1. every compared word starts at 0xA5 */
#define IMG_DIRTY(name, type) memset(&imgIn.name, 0xA5, sizeof(imgIn.name));
    IMG_LIST(IMG_DIRTY)
#undef IMG_DIRTY

#define ST_DIRTY(name, type) memset(&stIn.name, 0xA5, sizeof(stIn.name));
    ST_LIST(ST_DIRTY)
#undef ST_DIRTY

    /* 2. ... then a scenario-specific stream lays down real values */
    g_ds = seed * 2654435761UL + 0x9E3779B9UL;

    /* shared globals */
    imgIn.gMainCmd.FreqSet        = (int)drnd_int(-32768, 32767);
    imgIn.gMainCmd.FreqSyn        = (int)drnd_int(-32768, 32767);
    imgIn.gMainCmd.FreqFeed       = (int)drnd_int(-32768, 32767);
    imgIn.gMainCmd.FreqSynFilter  = (int)drnd_int(-32768, 32767);
    imgIn.gMainCmd.FreqReal       = (long)drnd_int(-100000, 100000);
    imgIn.gMainCmd.FirstCnt       = (Uint)drnd_int(0, 1100);

    imgIn.gPhase.IMPhase          = (long)drnd_int(-2147483647L, 2147483647L);
    imgIn.gPhase.StepPhaseApply   = (long)drnd_int(-2147483647L, 2147483647L);

    imgIn.gBasePar.FullFreq01     = (Ulong)drnd_int(1000, 60000);
    imgIn.gBasePar.FullFreq       = (Uint)drnd_int(1, 6000);
    imgIn.gBasePar.FcSetApply     = (Uint)drnd_int(1, 16);

    imgIn.gVCPar.SvcMode          = (Uint)(drnd() & 1UL);
    imgIn.gVCPar.VCWsCoff         = (Uint)drnd_int(0, 300);
    imgIn.gVCPar.VCSpeedFilter    = (int)drnd_int(0, 20);

    imgIn.gMotorExtPer.R1         = (Uint)drnd_int(0, 65535);
    imgIn.gMotorExtPer.R2         = (Uint)drnd_int(0, 65535);
    imgIn.gMotorExtPer.L0         = (Uint)drnd_int(0, 65535);
    imgIn.gMotorExtPer.LM         = (Uint)drnd_int(1, 65535);
    imgIn.gMotorExtPer.L1         = (Uint)drnd_int(1, 65535);
    imgIn.gMotorExtPer.L2         = (Uint)drnd_int(0, 65535);
    imgIn.gMotorExtPer.I0         = (Uint)drnd_int(0, 65535);

    imgIn.gIAlphBetaQ12.Alph      = (long)drnd_int(-32768, 32767);
    imgIn.gIAlphBetaQ12.Beta      = (long)drnd_int(-32768, 32767);

    imgIn.gIMTSetApply.M          = (long)drnd_int(-1000000L, 1000000L);
    imgIn.gIMTSetApply.T          = (long)drnd_int(-1000000L, 1000000L);
    imgIn.gIMTSet.M               = (long)drnd_int(-1000000L, 1000000L);
    imgIn.gIMTSet.T               = (long)drnd_int(-1000000L, 1000000L);
    imgIn.gIMTQ12.M               = (int)drnd_int(-32768, 32767);
    imgIn.gIMTQ12.T               = (int)drnd_int(-32768, 32767);

    imgIn.gTestDataReceive.TestData2 = (int)drnd_int(-32768, 32767);
    imgIn.gTestDataReceive.TestData3 = (int)drnd_int(-32768, 32767);

    imgIn.gInvInfo.InvTypeApply   = (Uint)drnd_int(0, 40);
    imgIn.gRatio                  = (Uint)drnd_int(0, 2000);
    imgIn.gMotorInfo.Frequency    = (Uint)drnd_int(0, 6000);

    imgIn.gABVoltSet.Alph         = (long)drnd_int(-32768, 32767);
    imgIn.gABVoltSet.Beta         = (long)drnd_int(-32768, 32767);

    /* state image */
    stIn.gSVCSpeed.Timer         = (Ulong)drnd();
    stIn.gSVCSpeed.DetaTimer     = (int)drnd_int(-32768, 32767);
    stIn.gSVCSpeed.DetaPhase     = (int)drnd_int(-32768, 32767);
    stIn.gSVCSpeed.SvcSynSpeed   = (int)drnd_int(-32768, 32767);
    stIn.gSVCSpeed.SvcWs         = (int)drnd_int(-32768, 32767);
    stIn.gSVCSpeed.SvcRotorSpeed = (int)drnd_int(-32768, 32767);
    stIn.gSVCSpeed.SvcLastFluxPos= (int)drnd_int(-32768, 32767);
    stIn.gSVCSpeed.SvcSignal     = (Uint)(drnd() & 1UL);
    stIn.gFluxR.Amp              = (Uint)drnd_int(0, 65535);
    stIn.gFluxR.Theta            = (Uint)drnd_int(0, 65535);
    stIn.gFluxS.Amp              = (Uint)drnd_int(0, 65535);
    stIn.gFluxS.Theta            = (Uint)drnd_int(0, 65535);
    stIn.gCalFlux.FilterTime     = (long)drnd_int(-100000, 100000);
    stIn.gCalFlux.SampleTime     = (Uint)drnd_int(0, 65535);
    stIn.gABFluxS_LowFilter.Alph = (long)drnd_int(-32768, 32767);
    stIn.gABFluxS_LowFilter.Beta = (long)drnd_int(-32768, 32767);
    stIn.gABFluxS_comp.Alph      = (long)drnd_int(-32768, 32767);
    stIn.gABFluxS_comp.Beta      = (long)drnd_int(-32768, 32767);
    /* the observer current: SVCCalRotorSpeed_380 multiplies the slip by
     * (gIMTQ24_obs.T >> 12), so both fields must be live, and M != T, or the
     * "used M instead of T" mutant would be invisible */
    stIn.gIMTQ24_obs.M           = (long)drnd_int(-1000000L, 0L);
    stIn.gIMTQ24_obs.T           = (long)drnd_int(1L, 1000000L);
    stIn.gIMTQ12_obs.M           = (long)drnd_int(-1000000L, 1000000L);
    stIn.gIMTQ12_obs.T           = (long)drnd_int(-1000000L, 1000000L);

    /* the 0xA5 prefill above leaves the enums/unions dirty too; re-lay the
     * union members that carry the random bits the module can read */
    for (i = 0; i < 4; i++)
    {
        /* nothing extra: MotorSVC reads no union bitfields */
    }
}

/* ------------------------------------------------------------------------- */
/* phases                                                                    */
/* ------------------------------------------------------------------------- */

/* ResetSVC: one path through the function, but imgIn.gPhase.IMPhase drives a real
 * 32->16 truncation (`(Uint)((long)imgIn.gPhase.IMPhase >> 16)`), so the table walks
 * the interesting corners of that cast, including the sign-extension ones. */
static void phase_reset(void)
{
    static const long v[] = {
        0L, 1L, 0x7FFFL, 0x8000L, 0xFFFFL, 0x10000L, 0x10001L, 0x7FFFFFFFL,
        -1L, -32768L, -65536L, 0xFFFF0000L, 0x00018000L, 0x80000000L
    };
    long i;

    begin_scenario(0x52535400UL);   /* "RST" */

    for (i = 0; i < (long)(sizeof(v) / sizeof(v[0])); i++)
    {
        imgIn.gPhase.IMPhase = v[i];
        imgIn.gABVoltSet.Alph = 123456L;
        imgIn.gABVoltSet.Beta  = -654321L;
        do_step(0, 0x00000000UL, "reset.phase");
        do_step(0, 0xFFFFFFFFUL, "reset.phase2");
    }

    /* reset with a dirty everything else: ResetSVC zeroes 20 fields, the
     * 0xA5 prefill guarantees those are non-zero before the call, so a
     * "forgot to clear one field" mutation is visible */
    for (i = 0; i < 8; i++)
    {
        imgIn.gPhase.IMPhase = (long)drnd_int(-2147483647L, 2147483647L);
        do_step(0, (Ulong)drnd(), "reset.rand");
    }
}

/* SVCCalFlux_380: the flux observer.  Two big branches (SvcMode 0 / 1), the
 * __IQsat clamp on FilterTime, the 24->16 bit fold of m_DetaTimer, and the
 * SvcMode != 1 angle path with qsin / qatan. */
static void phase_flux(void)
{
    static const long td3[] = { -32768, -1000, 99, 100, 101, 500, 1999, 2000,
                                2001, 32767 };
    static const long td2[] = { -32768, -100, 0, 1, 99, 100, 101, 32767 };
    /* (Timer, TIM) pairs that walk the m_DetaTimer truncation corners:
     * delta = (Timer - TIM) & 0xFFFFFF; m_DetaTimer = (int)(delta >> 3) */
    static const Ulong tt[][2] = {
        { 0x00000000UL, 0x00000000UL },   /* delta 0        -> 0            */
        { 0x00000008UL, 0x00000000UL },   /* delta 8        -> 1            */
        { 0x0003FFF8UL, 0x00000000UL },   /* >>3 = 0x7FFF   -> 32767        */
        { 0x00040000UL, 0x00000000UL },   /* >>3 = 0x8000   -> -32768       */
        { 0x000FFFF8UL, 0x00000000UL },   /* >>3 = 0x1FFFF  -> -1 (folded)  */
        { 0x00FFFFF8UL, 0x00000000UL },   /* >>3 = 0x1FFFFF -> -1 (folded)  */
        { 0x00FFFFFFUL, 0x00000000UL },   /* >>3 = 0x1FFFF  -> -1           */
        { 0x04000000UL, 0x00000000UL },   /* masked to 0    -> 0            */
        { 0x00000000UL, 0x00FFFFFFUL },   /* wraps: 1       -> 0            */
        { 0x00000000UL, 0x00000FFFUL },   /* wraps: 0xFFFFF001 -> -2        */
        { 0xFFFFFFFFUL, 0x00000000UL },   /* masked: 0xFFFFFF -> -1         */
        { 0x12345678UL, 0x12345600UL }    /* delta 0x78     -> 15           */
    };
    long i;

    /* ---- SvcMode == 0: FilterTime comes from FreqReal ------------------- */
    static const long fr[] = { 0L, 1L, -1L, 1000L, 100000L, -100000L,
                               2000000000L, -2000000000L };

    begin_scenario(0x464C5530UL);   /* "FLU0" */
    for (i = 0; i < 8; i++)
    {
        imgIn.gVCPar.SvcMode = 0U;
        imgIn.gMainCmd.FreqReal = fr[i];
        stIn.gSVCSpeed.Timer = tt[i % 12][0];
        pin_for_flux();
        do_step(1, tt[i % 12][1], "flux.mode0");
        g_covSvcMode[0]++;
    }

    /* ---- m_DetaTimer truncation + DetaTimer accumulation wrap ----------- */
    for (i = 0; i < (long)(sizeof(tt) / sizeof(tt[0])); i++)
    {
        imgIn.gVCPar.SvcMode = 0U;
        imgIn.gMainCmd.FreqReal = 50000L;
        stIn.gSVCSpeed.Timer = tt[i][0];
        stIn.gSVCSpeed.DetaTimer = 32767;          /* +1 wraps to -32768       */
        pin_for_flux();
        do_step(1, tt[i][1], "flux.timerPos");
        if (stRef.gSVCSpeed.DetaTimer == -32768)
        {
            g_covFluxTimerTrunc++;    /* the 16-bit += really wrapped      */
        }
        stIn.gSVCSpeed.DetaTimer = -32768;         /* +(-1) wraps to 32767      */
        do_step(1, tt[i][1], "flux.timerNeg");
        if (stRef.gSVCSpeed.DetaTimer == 32767)
        {
            g_covFluxTimerWrap2++;
        }
    }
    /* the same with a random DetaTimer so the += itself is exercised */
    for (i = 0; i < 8; i++)
    {
        imgIn.gVCPar.SvcMode = 0U;
        stIn.gSVCSpeed.Timer = (Ulong)drnd();
        stIn.gSVCSpeed.DetaTimer = (int)drnd_int(-32768, 32767);
        pin_for_flux();
        do_step(1, (Ulong)drnd(), "flux.timerRnd");
    }

    /* ---- SvcMode != 0: FilterTime = __IQsat(TestData3, 2000, 100) ------- */
    begin_scenario(0x464C5531UL);   /* "FLU1" */
    imgIn.gVCPar.SvcMode = 1U;
    for (i = 0; i < (long)(sizeof(td3) / sizeof(td3[0])); i++)
    {
        long v;
        imgIn.gTestDataReceive.TestData3 = (int)td3[i];
        /* classification for the coverage counter */
        if (td3[i] > 2000L)            v = 0;   /* upper clamp        */
        else if (td3[i] == 2000L)      v = 1;   /* exact upper bound  */
        else if (td3[i] > 100L)        v = 2;   /* in range           */
        else if (td3[i] == 100L)       v = 3;   /* exact lower bound  */
        else                           v = 4;   /* lower clamp        */
        g_covIqSat[v]++;
        pin_for_flux();
        do_step(1, 0x00001000UL, "flux.iqsat");
    }
    g_covSvcMode[1]++;

    /* ---- the SvcMode != 0 angle path ------------------------------------ */
    for (i = 0; i < (long)(sizeof(td2) / sizeof(td2[0])); i++)
    {
        imgIn.gTestDataReceive.TestData2 = (int)td2[i];
        g_covFluxComp++;
        pin_for_flux();
        do_step(1, 0x00001000UL, "flux.comp");
    }

    /* angle sum corners: m_Angle = (int)((IMPhase>>16) + (StepPhaseApply>>16))
     * is 16-bit arithmetic, so the corners of that fold are worth walking */
    {
        static const long ang[][2] = {
            { 0x00000000L, 0x00000000L },   { 0x7FFF0000L, 0x00000000L },
            { 0x7FFF0000L, 0x00010000L },   { 0x80000000L, 0x00000000L },
            { 0xFFFF0000L, 0x00010000L },   { 0x40000000L, 0x40000000L },
            { -65536L,     0x00000000L },   { 0x00018000L, 0x00018000L }
        };
        for (i = 0; i < (long)(sizeof(ang) / sizeof(ang[0])); i++)
        {
            imgIn.gPhase.IMPhase        = ang[i][0];
            imgIn.gPhase.StepPhaseApply = ang[i][1];
            imgIn.gTestDataReceive.TestData2 = 100;
            imgIn.gTestDataReceive.TestData3 = 500;
            pin_for_flux();
            do_step(1, 0x00020000UL, "flux.angle");
        }
    }

    /* wide random inside SvcMode 1, with every field of the angle path live */
    for (i = 0; i < 24; i++)
    {
        imgIn.gVCPar.SvcMode = 1U;
        imgIn.gPhase.IMPhase        = (long)drnd_int(-2147483647L, 2147483647L);
        imgIn.gPhase.StepPhaseApply = (long)drnd_int(-2147483647L, 2147483647L);
        imgIn.gTestDataReceive.TestData2 = (int)drnd_int(-32768, 32767);
        imgIn.gTestDataReceive.TestData3 = (int)drnd_int(-32768, 32767);
        imgIn.gMainCmd.FreqReal     = (long)drnd_int(-100000, 100000);
        imgIn.gIMTSetApply.M        = (long)drnd_int(-1000000L, 1000000L);
        imgIn.gIMTSetApply.T        = (long)drnd_int(-1000000L, 1000000L);
        imgIn.gIAlphBetaQ12.Alph    = (long)drnd_int(-32768, 32767);
        imgIn.gIAlphBetaQ12.Beta    = (long)drnd_int(-32768, 32767);
        stIn.gABFluxS_LowFilter.Alph = (long)drnd_int(-32768, 32767);
        stIn.gABFluxS_LowFilter.Beta = (long)drnd_int(-32768, 32767);
        stIn.gABFluxS_comp.Alph    = (long)drnd_int(-32768, 32767);
        stIn.gABFluxS_comp.Beta    = (long)drnd_int(-32768, 32767);
        stIn.gSVCSpeed.Timer       = (Ulong)drnd();
        stIn.gSVCSpeed.DetaTimer   = (int)drnd_int(-32768, 32767);
        pin_for_flux();
        do_step(1, (Ulong)drnd(), "flux.rand1");
    }
}

/* SVCCalRotorSpeed: the non-380 speed estimator.  FirstCnt ramp, the low-bus
 * fallback (gRatio < m_LowVolt), the slip-frequency branch, the sign-reversal
 * clamp and the 2nd-order speed filter. */
static void phase_speed(void)
{
    static const long firstCnt[] = { 0, 1, 5, 9, 10, 11, 999, 1000, 1001 };
    static const long invT[]     = { 0, 21, 22, 23, 40 };
    static const long ratio[]    = { 0, 79, 80, 81, 299, 300, 301, 2000 };
    static const long vcf[]      = { 0, 1, 2, 3, 20 };
    long i;

    /* ---- FirstCnt ramp (the < 10 early-return arm) ---------------------- */
    begin_scenario(0x53504430UL);   /* "SPD0" */
    for (i = 0; i < (long)(sizeof(firstCnt) / sizeof(firstCnt[0])); i++)
    {
        imgIn.gMainCmd.FirstCnt = (Uint)firstCnt[i];
        if (firstCnt[i] < 10)       g_covFirst[0]++;
        else if (firstCnt[i] < 1000) g_covFirst[1]++;
        else                        g_covFirst[2]++;
        pin_for_speed();
        do_step(2, 0x00001000UL, "speed.firstcnt");
    }

    /* ---- InvTypeApply -> m_LowVolt, and the gRatio fallback ---------------- */
    for (i = 0; i < (long)(sizeof(invT) / sizeof(invT[0])); i++)
    {
        long k;
        imgIn.gInvInfo.InvTypeApply = (Uint)invT[i];
        if (invT[i] >= 22) g_covInv22++;
        for (k = 0; k < (long)(sizeof(ratio) / sizeof(ratio[0])); k++)
        {
            imgIn.gRatio = (Uint)ratio[k];
            if ((invT[i] >= 22 && ratio[k] < 80) ||
                (invT[i] < 22 && ratio[k] < 300))
            {
                g_covRatioClamp++;
            }
            imgIn.gMainCmd.FirstCnt = 500;
            imgIn.gMainCmd.FreqSyn  = 400;
            pin_for_speed();
            do_step(2, 0x00002000UL, "speed.ratio");
        }
    }

    /* ---- the two slip-frequency divisors -------------------------------- */
    {
        static const long io_v[]  = { 0, 100, 4095, 4096, 5000, 65535 };
        static const long mm_v[]  = { -100000L, -4096L, -4095L, 0L, 4095L,
                                      4096L, 100000L };
        long a, b;
        for (a = 0; a < (long)(sizeof(io_v) / sizeof(io_v[0])); a++)
        {
            for (b = 0; b < (long)(sizeof(mm_v) / sizeof(mm_v[0])); b++)
            {
                imgIn.gMotorExtPer.I0 = (Uint)io_v[a];
                imgIn.gIMTSetApply.M  = mm_v[b];
                imgIn.gMainCmd.FirstCnt = 500;   /* clear the < 10 ramp */
                if (io_v[a] > (mm_v[b] >> 12))
                {
                    g_covIoBranch++;
                }
                pin_for_speed();     /* re-pins M if the poke made it small */
                do_step(2, 0x00000800UL, "speed.slip");
            }
        }
    }

    /* ---- m_Speed == 0 -------------------------------------------------- */
    /* The module WRITES stIn.gSVCSpeed.DetaPhase (stIn.gFluxR.Theta - SvcLastFluxPos),
     * so "DetaPhase == 0" is arranged through those two input fields, and the
     * resulting |DetaPhase| * 25000 / FullFreq01 term is what must vanish.
     * Filter16(0, 0) == 0 and VCWsCoff == 0 forces SvcWs == 0, so the
     * subtraction that feeds the == 0 arm is exactly zero. */
    {
        stIn.gFluxR.Theta        = 12345U;
        stIn.gSVCSpeed.SvcLastFluxPos = 12345;
        stIn.gSVCSpeed.SvcSynSpeed = 0;
        imgIn.gVCPar.VCWsCoff     = 0U;
        imgIn.gVCPar.VCSpeedFilter = 1;
        imgIn.gRatio              = 2000U;      /* no low-bus fallback */
        imgIn.gMainCmd.FirstCnt   = 500;
        pin_for_speed();
        do_step(2, 0x00001000UL, "speed.zero");
        g_covSpeedZero++;
    }

    /* ---- SvcSignal / FreqSyn ------------------------------------------- */
    {
        static const long fs[] = { 0, 1, -1, 32767, -32768 };
        long k;
        for (k = 0; k < (long)(sizeof(fs) / sizeof(fs[0])); k++)
        {
            imgIn.gVCPar.VCWsCoff = 100U;
            stIn.gSVCSpeed.SvcSignal = 0U;
            imgIn.gMainCmd.FreqSyn = (int)fs[k];
            if (fs[k] == 0)
            {
                g_covSynZero++;      /* SvcRotorSpeed = 0 arm */
            }
            else
            {
                g_covSynSet++;       /* SvcSignal = 1 arm */
            }
            imgIn.gMainCmd.FirstCnt = 500;
            stIn.gFluxR.Theta = 200U;          /* DetaPhase = 200 - 100 */
            stIn.gSVCSpeed.SvcLastFluxPos = 100;
            imgIn.gVCPar.VCSpeedFilter = 1;
            imgIn.gRatio = 2000U;
            pin_for_speed();
            do_step(2, 0x00001000UL, "speed.signal");
        }
        /* SvcSignal already 1: neither arm of the inner if fires */
        stIn.gSVCSpeed.SvcSignal = 1U;
        pin_for_speed();
        do_step(2, 0x00001000UL, "speed.signal1");
    }

    /* ---- FreqSyn == 0 with the filter arm active (VCSpeedFilter > 1) ----- */
    /* In SVCCalRotorSpeed the FreqSyn == 0 arm writes SvcRotorSpeed = 0, and
     * that value is READ two statements later by the filter arm
     *     m_Speed = SvcRotorSpeed * (VCF - 1) + 2 * m_Speed;
     * With VCSpeedFilter <= 1 the value is overwritten by SvcRotorSpeed =
     * m_Speed, so a bench that only ever pins VCSpeedFilter = 1 cannot see the
     * write at all.  That is exactly how "SvcRotorSpeed = 0 -> 1" hid on the
     * first negative-control run (MS133).  SvcSynSpeed is swept too, because
     * it is the `total` argument of Filter16 and therefore what decides how
     * large the surviving difference is. */
    /* Three things have to hold at once or the write is invisible, and the
     * first version of this block got two of them wrong -- the probe in the
     * variant driver printed SvcRotorSpeed = 0 for every draw:
     *   1. VCSpeedFilter > 1, otherwise the arm below overwrites the value;
     *   2. gMainCmd.FreqSet * m_Speed must NOT be negative, otherwise the
     *      sign-reversal clamp sets m_Speed = 0 two statements later and the
     *      difference (VCF-1)/(VCF+1) collapses to 0 -- FreqSet is therefore
     *      pinned to 0, which makes the product 0 (not < 0) for any sign;
     *   3. m_Speed must be non-zero, otherwise the numerator difference
     *      (VCF-1) divided by (VCF+1) truncates back to 0.
     * Theta is swept so m_Speed lands on many different residues. */
    {
        static const long vcf[]   = { 2, 3, 5, 20 };
        static const int  ss[]    = { 0, 1000, -1000 };
        static const long theta[] = { 901, 950, 1000, 1100, 1300, 2000 };
        long a, b, c;

        for (a = 0; a < (long)(sizeof(vcf) / sizeof(vcf[0])); a++)
        {
            for (b = 0; b < (long)(sizeof(ss) / sizeof(ss[0])); b++)
            {
                for (c = 0; c < (long)(sizeof(theta) / sizeof(theta[0])); c++)
                {
                    stIn.gSVCSpeed.SvcSignal = 0U;
                    stIn.gSVCSpeed.SvcSynSpeed = (int)ss[b];
                    stIn.gSVCSpeed.SvcWs = 0;
                    stIn.gFluxR.Theta = (Uint)theta[c];
                    stIn.gSVCSpeed.SvcLastFluxPos = 900;
                    stIn.gSVCSpeed.DetaTimer = 8;
                    imgIn.gMainCmd.FirstCnt = 500;
                    imgIn.gMainCmd.FreqSyn = 0;      /* the arm under test */
                    imgIn.gMainCmd.FreqSet = 0;      /* no sign-reversal clamp */
                    imgIn.gVCPar.VCSpeedFilter = (int)vcf[a];
                    imgIn.gVCPar.VCWsCoff = 100U;
                    imgIn.gRatio = 2000U;
                    imgIn.gInvInfo.InvTypeApply = 20U;
                    imgIn.gBasePar.FullFreq01 = 3000UL;
                    imgIn.gMotorExtPer.I0 = 500U;
                    imgIn.gMotorExtPer.R2 = 3000U;
                    imgIn.gMotorExtPer.L1 = 1000U;
                    imgIn.gIMTSetApply.M = 200000L;
                    imgIn.gIMTQ12.T = 20000;
                    pin_for_speed();
                    do_step(2, 0x00001000UL, "speed.freqzero");
                    g_covFreqZeroFilter++;
                }
            }
        }
    }

    /* ---- sign reversal: FreqSet * m_Speed < 0 --------------------------- */
    {
        stIn.gSVCSpeed.SvcSynSpeed = 0;      /* Filter16 keeps it >= 0 */
        imgIn.gVCPar.VCWsCoff = 200U;
        imgIn.gIMTQ12.T = -30000;             /* negative -> SvcWs < 0 */
        imgIn.gMainCmd.FreqSet = -1000;       /* opposite sign -> clamp */
        stIn.gFluxR.Theta = 32767U;          /* DetaPhase > 0 -> m_Speed_raw > 0 */
        stIn.gSVCSpeed.SvcLastFluxPos = 0;
        imgIn.gMainCmd.FirstCnt = 500;
        imgIn.gRatio = 2000U;
        stIn.gSVCSpeed.SvcSignal = 1U;
        imgIn.gVCPar.VCSpeedFilter = 1;
        pin_for_speed();
        do_step(2, 0x00001000UL, "speed.signflip");
        g_covSignFlip++;

        imgIn.gMainCmd.FreqSet = 1000;        /* same sign -> no clamp */
        do_step(2, 0x00001000UL, "speed.signsame");
    }

    /* ---- VCSpeedFilter: control arm vs filter arm ----------------------- */
    begin_scenario(0x53504431UL);   /* "SPD1" */
    for (i = 0; i < (long)(sizeof(vcf) / sizeof(vcf[0])); i++)
    {
        imgIn.gVCPar.VCSpeedFilter = (int)vcf[i];
        imgIn.gMainCmd.FirstCnt = 500;
        if (vcf[i] <= 1) g_covFilterCtl++;
        else             g_covFilterAvg++;
        pin_for_speed();
        do_step(2, 0x00000800UL, "speed.vcfilter");
    }

    /* ---- wide random ---------------------------------------------------- */
    for (i = 0; i < 40; i++)
    {
        imgIn.gMainCmd.FirstCnt = (Uint)drnd_int(0, 1100);
        imgIn.gInvInfo.InvTypeApply = (Uint)drnd_int(0, 40);
        imgIn.gRatio = (Uint)drnd_int(0, 2000);
        imgIn.gVCPar.VCWsCoff = (Uint)drnd_int(0, 300);
        imgIn.gVCPar.VCSpeedFilter = (int)drnd_int(0, 20);
        stIn.gSVCSpeed.DetaPhase = (int)drnd_int(-32768, 32767);
        stIn.gSVCSpeed.SvcSynSpeed = (int)drnd_int(-32768, 32767);
        stIn.gSVCSpeed.SvcWs = (int)drnd_int(-32768, 32767);
        stIn.gSVCSpeed.SvcRotorSpeed = (int)drnd_int(-32768, 32767);
        stIn.gSVCSpeed.SvcSignal = (Uint)(drnd() & 1UL);
        imgIn.gMainCmd.FreqSyn = (int)drnd_int(-32768, 32767);
        imgIn.gMainCmd.FreqSet = (int)drnd_int(-32768, 32767);
        imgIn.gMotorExtPer.I0 = (Uint)drnd_int(0, 65535);
        imgIn.gMotorExtPer.R2 = (Uint)drnd_int(0, 65535);
        imgIn.gIMTSetApply.M = (long)drnd_int(-1000000L, 1000000L);
        imgIn.gIMTQ12.T = (int)drnd_int(-32768, 32767);
        stIn.gSVCSpeed.DetaTimer = (int)drnd_int(-32768, 32767);
        pin_for_speed();
        do_step(2, (Ulong)drnd(), "speed.rand");
    }
}

/* SVCCalRotorSpeed_380: the 380 V variant.  Different FirstCnt ladder, the
 * ternary + 1024 clamp on the frequency ramp, and the one place in the tree
 * that reads a local before assigning it. */
static void phase_speed380(void)
{
    static const long firstCnt[] = { 0, 1, 9, 10, 11, 500, 999, 1000, 1001 };
    static const long vcf[]      = { 0, 1, 2, 3, 20 };
    long i;

    /* ---- m_IntData ternary (3 arms) ------------------------------------- */
    begin_scenario(0x53503830UL);   /* "SP80" */
    {
        /* FreqSyn is the divisor, so 0 is not allowed here; the == arm is
         * reached with a non-zero value on both sides instead */
        static const long ss[] = { -100, 0, 100 };
        static const long sy[] = { -100, 100, 250 };
        long a, b;
        for (a = 0; a < 3; a++)
        {
            for (b = 0; b < 3; b++)
            {
                stIn.gSVCSpeed.SvcSynSpeed = (int)ss[a];
                imgIn.gMainCmd.FreqSyn      = (int)sy[b];
                if (ss[a] < sy[b])       g_covIntData[0]++;
                else if (ss[a] == sy[b]) g_covIntData[1]++;
                else                     g_covIntData[2]++;
                pin_for_speed380();
                do_step(3, 0x00001000UL, "speed380.ternary");
            }
        }
    }

    /* ---- the m_ULong > 1024 clamp --------------------------------------- */
    {
        imgIn.gMotorInfo.Frequency = 6000;
        imgIn.gBasePar.FullFreq01 = 1000;
        imgIn.gMainCmd.FreqSyn = 1;             /* tiny divisor -> big ratio */
        pin_for_speed380();
        do_step(3, 0x00001000UL, "speed380.clamp");
        g_covClamp1024++;

        imgIn.gMotorInfo.Frequency = 0;         /* m_ULong == 0 -> no clamp */
        pin_for_speed380();
        do_step(3, 0x00001000UL, "speed380.noclamp");
    }

    /* ---- FirstCnt ladder: <10 / <1000 / else ---------------------------- */
    for (i = 0; i < (long)(sizeof(firstCnt) / sizeof(firstCnt[0])); i++)
    {
        imgIn.gMainCmd.FirstCnt = (Uint)firstCnt[i];
        if (firstCnt[i] < 10)        g_covFirst[0]++;
        else if (firstCnt[i] < 1000) g_covFirst[1]++;
        else                         g_covFirst[2]++;
        imgIn.gMotorInfo.Frequency = 500;
        imgIn.gBasePar.FullFreq01 = 3000;
        imgIn.gMainCmd.FreqSyn = 500;
        pin_for_speed380();
        do_step(3, 0x00001000UL, "speed380.firstcnt");
    }

    /* ---- SvcSignal / FreqSyn (FreqSyn pinned non-zero: see header) ------ */
    {
        imgIn.gMainCmd.FirstCnt = 500;
        stIn.gSVCSpeed.SvcSignal = 0U;
        imgIn.gMainCmd.FreqSyn = 700;
        pin_for_speed380();
        do_step(3, 0x00001000UL, "speed380.signal");
        g_covSynSet++;

        stIn.gSVCSpeed.SvcSignal = 1U;
        pin_for_speed380();
        do_step(3, 0x00001000UL, "speed380.signal1");
    }

    /* ---- VCSpeedFilter: control arm vs the uninitialized-m_Speed arm ---- */
    for (i = 0; i < (long)(sizeof(vcf) / sizeof(vcf[0])); i++)
    {
        imgIn.gVCPar.VCSpeedFilter = (int)vcf[i];
        imgIn.gMainCmd.FirstCnt = 500;
        if (vcf[i] <= 1) g_covFilterCtl++;
        else
        {
            g_covFilterAvg++;
            g_covUninitSpeed++;   /* the arm that reads m_Speed unassigned */
        }
        pin_for_speed380();
        do_step(3, 0x00001000UL, "speed380.vcfilter");
    }

    /* ---- wide random ---------------------------------------------------- */
    for (i = 0; i < 40; i++)
    {
        imgIn.gMainCmd.FirstCnt = (Uint)drnd_int(0, 1100);
        imgIn.gMainCmd.FreqSyn = (int)(drnd_int(-32768, 32767) | 1);  /* never 0 */
        stIn.gSVCSpeed.SvcSynSpeed = (int)drnd_int(-32768, 32767);
        stIn.gSVCSpeed.SvcWs = (int)drnd_int(-32768, 32767);
        stIn.gSVCSpeed.SvcRotorSpeed = (int)drnd_int(-32768, 32767);
        stIn.gSVCSpeed.SvcSignal = (Uint)(drnd() & 1UL);
        imgIn.gVCPar.VCWsCoff = (Uint)drnd_int(0, 300);
        imgIn.gVCPar.VCSpeedFilter = (int)drnd_int(0, 20);
        imgIn.gMotorInfo.Frequency = (Uint)drnd_int(0, 6000);
        imgIn.gBasePar.FullFreq01 = (Ulong)drnd_int(1000, 60000);
        imgIn.gMotorExtPer.R2 = (Uint)drnd_int(0, 65535);
        imgIn.gIMTSet.M = (long)drnd_int(-1000000L, 1000000L);
        pin_for_speed380();
        do_step(3, (Ulong)drnd(), "speed380.rand");
    }
}

/* ------------------------------------------------------------------------- */
/* sequence phase: state evolution across calls                              */
/* ------------------------------------------------------------------------- */
/* The four functions are used together in the firmware: ResetSVC once at
 * start-up, then the flux observer and the speed estimator feeding each other
 * through gSVCSpeed.Timer (the last GetTime()), gSVCSpeed.DetaTimer (the
 * accumulated sample period) and gABFluxS_LowFilter.  do_step isolates every
 * call, which cannot see "state evolved wrongly but consistently".  This phase
 * runs one evolving pass per side, snapshots after every call, and compares
 * step by step.  The timer for call N is derived from the state the PREVIOUS
 * call produced, so a state divergence also shows up as a timer divergence. */

#define SEQ_MAX 32

static struct STATE   seqStRef[SEQ_MAX];
static struct IMAGE   seqImgRef[SEQ_MAX];
static unsigned short seqCpuRef[SEQ_MAX][7];

static void do_seq(const int *fns, int n, const char *tag)
{
    struct STATE stCur;
    int i;

    if (n > SEQ_MAX)
    {
        n = SEQ_MAX;
    }
    g_cases += n;

    /* the start of the sequence, identical for both sides */
    img_save(&imgIn);

    /* ---- ref side: one evolving pass, snapshots kept ---- */
    img_load(&imgIn);
    stCur = stIn;
    st_load_ref(&stCur);
    for (i = 0; i < n; i++)
    {
        gHostCpuTimer1Regs.TIM.all = stCur.gSVCSpeed.Timer + g_seqTimerStep;
        zero_counters();
        seed_stack();
        g_covFn[fns[i]]++;
        call_side(0, fns[i]);
        img_save(&seqImgRef[i]);
        st_snap_ref(&stCur);
        seqStRef[i] = stCur;
        memcpy(seqCpuRef[i], (const void *)gHostCpuCtl, sizeof(seqCpuRef[i]));
        g_covSeq++;
    }

    /* ---- new side: identical pass, compared step by step ---- */
    img_load(&imgIn);
    stCur = stIn;
    st_load_new(&stCur);
    for (i = 0; i < n; i++)
    {
        gHostCpuTimer1Regs.TIM.all = stCur.gSVCSpeed.Timer + g_seqTimerStep;
        zero_counters();
        seed_stack();
        call_side(1, fns[i]);
        img_save(&imgNew);
        st_snap_new(&stNew);
        memcpy(newCpu, (const void *)gHostCpuCtl, sizeof(newCpu));

#define SEQ_ST_CMP(name, type) CMPM(seqStRef[i].name, stNew.name, #name);
        ST_LIST(SEQ_ST_CMP)
#undef SEQ_ST_CMP

#define SEQ_IMG_CMP(name, type) CMPM(seqImgRef[i].name, imgNew.name, #name);
        IMG_LIST(SEQ_IMG_CMP)
#undef SEQ_IMG_CMP

        CMPM(seqCpuRef[i], newCpu, "gHostCpuCtl");

        stCur = stNew;
    }
}

static void phase_seq(void)
{
    /* A: ResetSVC, then the SvcMode == 0 observer feeding the non-380
     * estimator.  Twelve (flux, speed) pairs walk FirstCnt through the < 10
     * ramp and past it. */
    static const int seqA[] = {
        0, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2,
        1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2
    };
    /* B: ResetSVC, then the SvcMode != 0 observer with the 380 V estimator.
     * SvcMode != 0 never accumulates DetaTimer, which is exactly why the 380
     * estimator must not divide by it -- and it does not. */
    static const int seqB[] = {
        0, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3
    };

    begin_scenario(0x53515531UL);   /* "SEQ1" */
    imgIn.gVCPar.SvcMode = 0U;
    imgIn.gMainCmd.FreqSyn = 400;
    imgIn.gMainCmd.FreqSet = 400;
    imgIn.gBasePar.FullFreq01 = 3000UL;
    imgIn.gBasePar.FcSetApply = 2U;
    imgIn.gMotorExtPer.L1 = 1000U;
    imgIn.gMotorExtPer.LM = 800U;
    imgIn.gMotorExtPer.L0 = 500U;
    imgIn.gMotorExtPer.R1 = 3000U;
    imgIn.gMotorExtPer.R2 = 3000U;
    imgIn.gMotorExtPer.I0 = 500U;
    imgIn.gIMTSetApply.M = 200000L;
    imgIn.gIMTSetApply.T = -50000L;
    imgIn.gIMTQ12.T = 20000;
    imgIn.gIMTQ12.M = 10000;
    imgIn.gVCPar.VCWsCoff = 100U;
    imgIn.gVCPar.VCSpeedFilter = 3;
    imgIn.gRatio = 800U;
    imgIn.gInvInfo.InvTypeApply = 20U;
    imgIn.gMotorInfo.Frequency = 500U;
    imgIn.gIAlphBetaQ12.Alph = 1000L;
    imgIn.gIAlphBetaQ12.Beta = -1000L;
    imgIn.gTestDataReceive.TestData2 = 100;
    imgIn.gTestDataReceive.TestData3 = 500;
    imgIn.gABVoltSet.Alph = 200L;
    imgIn.gABVoltSet.Beta = -200L;
    stIn.gSVCSpeed.SvcSynSpeed = 0;
    stIn.gSVCSpeed.SvcWs = 0;
    stIn.gSVCSpeed.SvcRotorSpeed = 0;
    stIn.gSVCSpeed.SvcSignal = 0U;
    stIn.gSVCSpeed.SvcLastFluxPos = 0;
    stIn.gSVCSpeed.DetaTimer = 0;
    stIn.gSVCSpeed.Timer = 0UL;
    stIn.gFluxR.Amp = 0U;
    stIn.gFluxR.Theta = 0U;
    stIn.gFluxS.Amp = 0U;
    stIn.gFluxS.Theta = 0U;
    stIn.gCalFlux.FilterTime = 0L;
    stIn.gCalFlux.SampleTime = 25736U / 2U;
    stIn.gABFluxS_LowFilter.Alph = 0L;
    stIn.gABFluxS_LowFilter.Beta = 0L;
    stIn.gABFluxS_comp.Alph = 0L;
    stIn.gABFluxS_comp.Beta = 0L;
    do_seq(seqA, (int)(sizeof(seqA) / sizeof(seqA[0])), "seq.fluxA");

    begin_scenario(0x53515532UL);   /* "SEQ2" */
    imgIn.gVCPar.SvcMode = 1U;
    imgIn.gMainCmd.FreqSyn = 350;
    imgIn.gMainCmd.FreqSet = 350;
    imgIn.gBasePar.FullFreq01 = 3000UL;
    imgIn.gBasePar.FcSetApply = 2U;
    imgIn.gMotorExtPer.L1 = 1000U;
    imgIn.gMotorExtPer.LM = 800U;
    imgIn.gMotorExtPer.L0 = 500U;
    imgIn.gMotorExtPer.R1 = 3000U;
    imgIn.gMotorExtPer.R2 = 3000U;
    imgIn.gMotorExtPer.I0 = 500U;
    imgIn.gIMTSetApply.M = 200000L;
    imgIn.gIMTSetApply.T = -50000L;
    imgIn.gIMTSet.M = 200000L;
    imgIn.gIMTSet.T = -50000L;
    imgIn.gIMTQ12.T = 20000;
    imgIn.gIMTQ12.M = 10000;
    imgIn.gVCPar.VCWsCoff = 100U;
    imgIn.gVCPar.VCSpeedFilter = 3;
    imgIn.gRatio = 800U;
    imgIn.gInvInfo.InvTypeApply = 20U;
    imgIn.gMotorInfo.Frequency = 500U;
    imgIn.gIAlphBetaQ12.Alph = 1000L;
    imgIn.gIAlphBetaQ12.Beta = -1000L;
    imgIn.gTestDataReceive.TestData2 = 100;
    imgIn.gTestDataReceive.TestData3 = 500;
    imgIn.gABVoltSet.Alph = 200L;
    imgIn.gABVoltSet.Beta = -200L;
    stIn.gSVCSpeed.SvcSynSpeed = 0;
    stIn.gSVCSpeed.SvcWs = 0;
    stIn.gSVCSpeed.SvcRotorSpeed = 0;
    stIn.gSVCSpeed.SvcSignal = 0U;
    stIn.gSVCSpeed.SvcLastFluxPos = 0;
    stIn.gSVCSpeed.DetaTimer = 0;
    stIn.gSVCSpeed.Timer = 0UL;
    stIn.gFluxR.Amp = 0U;
    stIn.gFluxR.Theta = 0U;
    stIn.gFluxS.Amp = 0U;
    stIn.gFluxS.Theta = 0U;
    stIn.gCalFlux.FilterTime = 0L;
    stIn.gCalFlux.SampleTime = 25736U / 2U;
    stIn.gABFluxS_LowFilter.Alph = 0L;
    stIn.gABFluxS_LowFilter.Beta = 0L;
    stIn.gABFluxS_comp.Alph = 0L;
    stIn.gABFluxS_comp.Beta = 0L;
    do_seq(seqB, (int)(sizeof(seqB) / sizeof(seqB[0])), "seq.fluxB");
}

/* ------------------------------------------------------------------------- */
/* bench self-check                                                          */
/* ------------------------------------------------------------------------- */
/* The differential test can only prove "ref and new agree".  It cannot notice
 * when the BENCH itself stopped modelling the target -- e.g. if the case shim's
 * `#define int short` were lost, both sides would widen to 32 bits and still
 * agree, while several truncation-only mutants would hide.  These checks pin a
 * handful of values that are computed by hand from the source, so a bench that
 * silently drifted fails on its own. */

static int bench_selfcheck(void)
{
    int bad = 0;

    /* ---- 1. ResetSVC: gFluxR.Theta = (Uint)((long)gPhase.IMPhase >> 16) --- */
    memset(&stIn, 0, sizeof(stIn));
    memset(&imgIn, 0, sizeof(imgIn));
    imgIn.gPhase.IMPhase = 0x00180000L;      /* >> 16 == 0x0018 */
    do_step(0, 0UL, "selfcheck.reset18");
    if (stRef.gFluxR.Theta != 0x0018U || stNew.gFluxR.Theta != 0x0018U)
    {
        printf("SELFCHECK FAIL: (Uint)(0x00018000 >> 16) should be 0x0018, "
               "got ref=0x%04X new=0x%04X\n",
               (unsigned)stRef.gFluxR.Theta, (unsigned)stNew.gFluxR.Theta);
        bad++;
    }

    imgIn.gPhase.IMPhase = -1L;              /* >> 16 == -1 -> (Uint)0xFFFF */
    do_step(0, 0UL, "selfcheck.resetFF");
    if (stRef.gFluxR.Theta != 0xFFFFU || stNew.gFluxR.Theta != 0xFFFFU)
    {
        printf("SELFCHECK FAIL: (Uint)(-1 >> 16) should be 0xFFFF, "
               "got ref=0x%04X new=0x%04X\n",
               (unsigned)stRef.gFluxR.Theta, (unsigned)stNew.gFluxR.Theta);
        bad++;
    }

    imgIn.gPhase.IMPhase = 0x7FFF8000L;      /* >> 16 == 0x7FFF */
    do_step(0, 0UL, "selfcheck.reset7F");
    if (stRef.gFluxR.Theta != 0x7FFFU)
    {
        printf("SELFCHECK FAIL: (Uint)(0x7FFFFF80 >> 16) should be 0x7FFF\n");
        bad++;
    }

    /* ---- 2. SVCCalRotorSpeed: DetaPhase = gFluxR.Theta - SvcLastFluxPos --- */
    /* 0x8000 is a Uint; the subtraction is 16-bit, so 32768 - 0 must land as
     * -32768, NOT as 32768.  This is the fold that the shim provides. */
    memset(&stIn, 0, sizeof(stIn));
    memset(&imgIn, 0, sizeof(imgIn));
    stIn.gFluxR.Theta = 0x8000U;
    stIn.gSVCSpeed.SvcLastFluxPos = 0;
    stIn.gSVCSpeed.DetaTimer = 8;         /* divisor: pin non-zero */
    imgIn.gMainCmd.FirstCnt = 500;
    imgIn.gBasePar.FullFreq01 = 3000UL;
    imgIn.gMotorExtPer.L1 = 1000U;
    imgIn.gMotorExtPer.I0 = 500U;
    imgIn.gIMTSetApply.M = 200000L;
    imgIn.gMotorExtPer.R2 = 3000U;
    imgIn.gVCPar.VCWsCoff = 0U;
    imgIn.gVCPar.VCSpeedFilter = 1;
    imgIn.gRatio = 2000U;
    imgIn.gIMTQ12.T = 0;
    imgIn.gIMTQ12.M = 0;
    imgIn.gMotorInfo.Frequency = 500U;
    imgIn.gInvInfo.InvTypeApply = 20U;
    imgIn.gMainCmd.FreqSyn = 0;
    imgIn.gMainCmd.FreqSet = 0;
    do_step(2, 0x00001000UL, "selfcheck.dphase");
    if (stRef.gSVCSpeed.DetaPhase != -32768)
    {
        printf("SELFCHECK FAIL: DetaPhase (0x8000 - 0) should be -32768, "
               "got %d\n", (int)stRef.gSVCSpeed.DetaPhase);
        bad++;
    }

    return bad;
}

/* ------------------------------------------------------------------------- */
/* random phase                                                              */
/* ------------------------------------------------------------------------- */
/* The broad safety net: a mix of entry points and raw values the hand-built
 * phases do not enumerate.  Divisors are re-pinned per entry point so no draw
 * can turn a division into a crash. */

static void phase_random(long count)
{
    long c;

    for (c = 0; c < count; c++)
    {
        long fn = (long)(rnd() % 4UL);
        unsigned long seed = 1000000UL + (unsigned long)c;

        begin_scenario(seed);

        /* extra raw pokes on top of begin_scenario's own draw */
        imgIn.gVCPar.SvcMode = (Uint)(rnd() & 1UL);
        imgIn.gVCPar.VCWsCoff = (Uint)(rnd() % 301UL);
        imgIn.gVCPar.VCSpeedFilter = (int)(rnd() % 21UL);
        imgIn.gMainCmd.FirstCnt = (Uint)(rnd() % 1101UL);
        imgIn.gMainCmd.FreqSyn = (int)rnd_int(-32768, 32767);
        imgIn.gMainCmd.FreqSet = (int)rnd_int(-32768, 32767);
        imgIn.gMainCmd.FreqReal = (long)rnd_int(-100000, 100000);
        imgIn.gInvInfo.InvTypeApply = (Uint)(rnd() % 41UL);
        imgIn.gRatio = (Uint)(rnd() % 2001UL);
        imgIn.gMotorInfo.Frequency = (Uint)(rnd() % 6001UL);
        imgIn.gTestDataReceive.TestData2 = (int)rnd_int(-32768, 32767);
        imgIn.gTestDataReceive.TestData3 = (int)rnd_int(-32768, 32767);
        imgIn.gPhase.IMPhase = (long)rnd_int(-2000000000L, 2000000000L);
        imgIn.gPhase.StepPhaseApply = (long)rnd_int(-2000000000L, 2000000000L);
        stIn.gSVCSpeed.SvcSignal = (Uint)(rnd() & 1UL);
        stIn.gSVCSpeed.DetaPhase = (int)rnd_int(-32768, 32767);
        stIn.gSVCSpeed.DetaTimer = (int)rnd_int(-32768, 32767);
        stIn.gSVCSpeed.Timer = (Ulong)rnd();

        switch (fn)
        {
            case 0:
                break;
            case 1:
                pin_for_flux();
                break;
            case 2:
                pin_for_speed();
                break;
            default:
                imgIn.gMainCmd.FreqSyn = (int)(rnd_int(-32768, 32767) | 1);
                pin_for_speed380();
                break;
        }

        do_step((int)fn, (Ulong)rnd(), "random");
    }
}

/* ------------------------------------------------------------------------- */
/* main                                                                      */
/* ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long random = (argc > 1) ? atol(argv[1]) : 20000L;
    long i;

    g_rng = 0x12345678UL;

    /* Unbuffered stdout: this bench has inputs that can trap the process
     * (division by zero is pinned away, but the harness is the wrong place to
     * discover that), and a fully buffered stream loses the whole progress log
     * when that happens. */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* GetTime() is `CpuTimer1.RegsAddr->TIM.all` (MotorInclude.h): the struct
     * is a handle, the register window is whatever RegsAddr points at.  Wire
     * it to the case's CPUTIMER_REGS instance before any call, or the first
     * GetTime() dereferences NULL. */
    CpuTimer1.RegsAddr = &gHostCpuTimer1Regs;

    /* Width pins: the case shim folds `int` to the target's 16 bits so that
     * `Uint`/`int` storage matches the C28x.  If that fold is ever lost,
     * host-only 32-bit storage silently hides truncation-only mutants, so
     * fail loudly. */
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

    /* Layout pin: FLUX_STRUCT is two Uint = 4 bytes on both host and target,
     * so if a header change reorders or widens it, the whole-struct compare
     * would silently compare different things.  The structs that contain a
     * long (SVC_FLUX_CAL_STRUCT, SVC_SPEED_STRUCT) are deliberately NOT pinned
     * by size: MSVC pads a struct containing a 32-bit member to a 4-byte
     * boundary while the C28x aligns to 2, so their sizeof differs between host
     * (8 / 20) and target (6 / 18).  That padding is identical on the ref and
     * the new side -- both are the same host struct -- so it cannot manufacture
     * or hide a difference; it only means the compared footprint is the host's,
     * never the target's, and this bench is explicit about that. */
    if (sizeof(FLUX_STRUCT) != 4U)
    {
        printf("FATAL: FLUX_STRUCT layout differs (expected 4, got %u)\n",
               (unsigned)sizeof(FLUX_STRUCT));
        return 2;
    }

    if (bench_selfcheck() != 0)
    {
        printf("FATAL: bench self-check failed, results would be meaningless\n");
        return 2;
    }

    phase_reset();
    phase_flux();
    phase_speed();
    phase_speed380();
    phase_seq();
    phase_random(random);

    printf("MotorSVC difftest: cases=%ld mismatches=%ld\n", g_cases, g_mism);
    printf("input reach: fn[]=%ld/%ld/%ld/%ld svcMode=%ld/%ld "
           "first=%ld/%ld/%ld comp=%ld timerWrap=%ld/%ld\n",
           g_covFn[0], g_covFn[1], g_covFn[2], g_covFn[3],
           g_covSvcMode[0], g_covSvcMode[1],
           g_covFirst[0], g_covFirst[1], g_covFirst[2],
           g_covFluxComp, g_covFluxTimerTrunc, g_covFluxTimerWrap2);
    printf("input reach: inv22=%ld ratioClamp=%ld io=%ld synZero=%ld synSet=%ld "
           "signFlip=%ld speedZero=%ld cpuctl=%ld\n",
           g_covInv22, g_covRatioClamp, g_covIoBranch, g_covSynZero,
           g_covSynSet, g_covSignFlip, g_covSpeedZero, g_covCpuCtl);
    printf("input reach: freqZeroFilter=%ld\n", g_covFreqZeroFilter);
    printf("input reach: iqsat=%ld/%ld/%ld/%ld/%ld clamp1024=%ld "
           "intData=%ld/%ld/%ld vcf=%ld/%ld uninit=%ld seq=%ld\n",
           g_covIqSat[0], g_covIqSat[1], g_covIqSat[2], g_covIqSat[3],
           g_covIqSat[4], g_covClamp1024,
           g_covIntData[0], g_covIntData[1], g_covIntData[2],
           g_covFilterCtl, g_covFilterAvg, g_covUninitSpeed, g_covSeq);

    /* Every entry point must have been driven. */
    for (i = 0; i < 4; i++)
    {
        if (g_covFn[i] == 0)
        {
            printf("FATAL: entry point %ld was never driven\n", i);
            return 2;
        }
    }
    /* Every branch the hand-built phases exist to reach. */
    if (g_covSvcMode[0] == 0 || g_covSvcMode[1] == 0 ||
        g_covFirst[0] == 0 || g_covFirst[1] == 0 || g_covFirst[2] == 0 ||
        g_covFluxComp == 0 || g_covFluxTimerTrunc == 0 || g_covFluxTimerWrap2 == 0 ||
        g_covInv22 == 0 || g_covRatioClamp == 0 || g_covIoBranch == 0 ||
        g_covSynZero == 0 || g_covSynSet == 0 || g_covSignFlip == 0 ||
        g_covSpeedZero == 0 || g_covFreqZeroFilter == 0 || g_covCpuCtl == 0 ||
        g_covIqSat[0] == 0 || g_covIqSat[1] == 0 || g_covIqSat[2] == 0 ||
        g_covIqSat[3] == 0 || g_covIqSat[4] == 0 ||
        g_covClamp1024 == 0 ||
        g_covIntData[0] == 0 || g_covIntData[1] == 0 || g_covIntData[2] == 0 ||
        g_covFilterCtl == 0 || g_covFilterAvg == 0 || g_covUninitSpeed == 0 ||
        g_covSeq == 0)
    {
        printf("FATAL: a phase branch was never reached as an input\n");
        return 2;
    }
    /* The one arm that cannot be driven on the host (division by zero on the
     * host traps; on the C28x it does not) must NOT have been driven. */
    if (g_cov380NoSyn != 0)
    {
        printf("FATAL: the FreqSyn == 0 arm of SVCCalRotorSpeed_380 was driven "
               "-- that input kills the host process\n");
        return 2;
    }

    return g_mism == 0 ? 0 : 1;
}
