/* Differential test driver for MotorPWM.c -- reference vs rewritten.
 *
 * Both translation units (cases/MotorPWM/ref_tu.c and new_tu.c) include the
 * same header chain and expose the same five entry points under ref_/new_
 * prefixes.  This file drives both with byte-identical inputs and compares
 * everything that can differ:
 *
 *   - the three EPWM register blocks, field for field (whole-struct memcmp).
 *     This is the module's real output: InitSetPWM() writes ~20 fields per
 *     block, SendPWM() writes AQCTLA/CMPCTL/TBPRD/CMPB/CMPA.  Comparing the
 *     struct is what makes "wrote the right value to the wrong register"
 *     visible -- a summary value would not.
 *   - gPWM (U/V/W + gZeroLengthPhase), gRatio, EPWMFlagU/V/W  (module-owned,
 *     renamed per side)
 *   - gOutVolt (module-owned but never touched: kept in the compared set so an
 *     accidental write is caught, see the dead-code note below)
 *   - the five iTrig* sector tables (module-owned constants: compared every
 *     step so a corrupted initialiser is caught even though nothing writes
 *     them at run time)
 *   - the EALLOW/EDIS counter table the case shim substitutes for the two
 *     `asm(...)` macros, so "moved a register write outside the window" is
 *     observable.
 *
 * Inputs the module reads: gPhase.OutPhase, gADC.DelayApply,
 * gDeadBand.CompU/V/W, plus the module's own globals (gRatio, gPWM.*) and the
 * *previous* EPwm*.CMPA.half.CMPA (SendPWM branches on it before overwriting
 * it -- source lines 397/405 etc., so the register file is genuinely part of
 * the input and the whole image is saved/restored around every call).
 *
 * Phases: init / db / vc / vf / send are hand-built and hit each branch as an
 * input (asserted at the end); seq carries state across calls so the
 * EPWMFlag state machine is exercised the way the firmware runs it; random is
 * uniform.
 */

#include <stdio.h>
#include <string.h>

/* <stdlib.h> is deliberately NOT included: the case shim folds `int` to the
 * target's 16 bits, and stdlib's 32-bit `int abs(int)` then collides with the
 * project's folded `extern short abs(short)` from SubPrgInclude.h (MSVC C2371,
 * "different basic types").  Only atol is needed, so it is declared by hand. */
extern long atol(const char *);

#include "MotorPwmInclude.h"

/* ------------------------------------------------------------------------- */
/* the two sides                                                             */
/* ------------------------------------------------------------------------- */

void ref_InitSetPWM(void);
void ref_OutPutPWMVC(void);
void ref_OutPutPWMVF(void);
void ref_DeadBandComp(void);
void ref_SendPWM(void);

extern OUT_VOLT_STRUCT ref_gOutVolt;
extern PWM_OUT_STRUCT  ref_gPWM;
extern Uint            ref_gRatio;
extern Uint            ref_EPWMFlagU;
extern Uint            ref_EPWMFlagV;
extern Uint            ref_EPWMFlagW;
extern Uint            ref_iTrig1_Cpwm[12];
extern Uint            ref_iTrig1_Dpwm[12];
extern Uint            ref_iTrig2_Pwm[12];
extern Uint            ref_iTrig3_Cpwm[12];
extern Uint            ref_iTrig3_Dpwm[12];

void new_InitSetPWM(void);
void new_OutPutPWMVC(void);
void new_OutPutPWMVF(void);
void new_DeadBandComp(void);
void new_SendPWM(void);

extern OUT_VOLT_STRUCT new_gOutVolt;
extern PWM_OUT_STRUCT  new_gPWM;
extern Uint            new_gRatio;
extern Uint            new_EPWMFlagU;
extern Uint            new_EPWMFlagV;
extern Uint            new_EPWMFlagW;
extern Uint            new_iTrig1_Cpwm[12];
extern Uint            new_iTrig1_Dpwm[12];
extern Uint            new_iTrig2_Pwm[12];
extern Uint            new_iTrig3_Cpwm[12];
extern Uint            new_iTrig3_Dpwm[12];

/* ---- mock-owned ---------------------------------------------------------- */

extern volatile Uint16 gHostCpuCtl[7];

/* ---- the shared image: one entry per undefined DATA symbol -------------- */
/* Order and types are identical to mock_pwm.c's definitions.  X(name, type)
 * must stay a macro so the struct, the save, the load and the compare are all
 * generated from this single list.
 *
 * The three EPWM blocks are `volatile struct EPWM_REGS` in the real device
 * header; the IMAGE members are non-volatile copies, which is exactly the
 * save/restore semantics wanted here (read the register file out, put it
 * back).  The real struct is used, not a stand-in, so the bitfield layout and
 * the CMPA union are the firmware's. */

#define IMG_LIST(X)                                                        \
    X(EPwm1Regs,               struct EPWM_REGS)                           \
    X(EPwm2Regs,               struct EPWM_REGS)                           \
    X(EPwm3Regs,               struct EPWM_REGS)                           \
    X(gADC,                    ADC_STRUCT)                                 \
    X(gDeadBand,               DEAD_BAND_STRUCT)                           \
    X(gPhase,                  ANGLE_STRUCT)

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

/* ---- the module-owned state, renamed per side --------------------------- */
/* These are defined inside MotorPWM.c itself, so ref_tu.c / new_tu.c rename
 * them; they cannot live in the mock.  The iTrig* tables are handled apart
 * from this list because C cannot assign arrays -- see compare_tables(). */

#define ST_LIST(X)                                                         \
    X(gPWM,        PWM_OUT_STRUCT)                                         \
    X(gOutVolt,    OUT_VOLT_STRUCT)                                        \
    X(gRatio,      Uint)                                                   \
    X(EPWMFlagU,   Uint)                                                   \
    X(EPWMFlagV,   Uint)                                                   \
    X(EPWMFlagW,   Uint)

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
static unsigned long g_rng;

static long g_covFn[5];

/* branches the hand-built phases exist to reach */
static long g_covSector[12];       /* m_iSec30 == 0..11 in OutPutPWMVF          */
static long g_covVfCpwm;           /* PWMModle == MODLE_CPWM arm                */
static long g_covVfDpwm;           /* PWMModle == MODLE_DPWM arm                */
static long g_covDpwmInverse;      /* m_FlagDPWMInverse == 1                    */
static long g_covSec60Odd;         /* (m_iSec60 & 1) == 1 -> 10922 - rem        */
static long g_covOddCpwm;          /* odd sector, CPWM: 5461 + rem              */
static long g_covOddDpwm;          /* odd sector, DPWM: 5461 - rem              */
static long g_covTotalOver;        /* m_TotalLength > gPWMPrdApply              */
static long g_covPhaseClamp;       /* OutPhase reinterpreted to 65532..65535    */
static long g_covDbRatioZero;      /* DeadBandComp: gRatio == 0 arm             */
static long g_covDbRatioNonZero;   /* DeadBandComp: gRatio != 0 arm             */
static long g_covZeroSel[4];       /* gZeroLengthPhase: U / V / W / NONE        */
static long g_covIqSat[3];         /* > Prd / inside / < 0                      */
static long g_covSendZeroCmpa;     /* gPWM.x == 0 && CMPA != 0 -> AQCTLA 0x91   */
static long g_covSendNzCmpaZero;   /* gPWM.x != 0 && CMPA == 0 -> 0x12 + CMPCTL 2 */
static long g_covSendFlagClear;    /* EPWMFlag != 0 at entry -> cleared         */
static long g_covSendFlagSet;      /* EPWMFlag set to 1 by SendPWM              */
static long g_covEallow;           /* EALLOW/EDIS pair ran (InitSetPWM or Send) */
static long g_covDelayUnder;       /* gADC.DelayApply > C_INIT_PRD (CMPB wraps)  */
static long g_covSeq;              /* sequence phase steps                      */
/* must stay 0 -- unreachable by construction, see main() */
static long g_covSec30Clamp;       /* the `if (m_iSec30 > 11)` arm              */

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

static void compare_tables(const char *tag)
{
    CMPM(ref_iTrig1_Cpwm, new_iTrig1_Cpwm, "iTrig1_Cpwm");
    CMPM(ref_iTrig1_Dpwm, new_iTrig1_Dpwm, "iTrig1_Dpwm");
    CMPM(ref_iTrig2_Pwm,  new_iTrig2_Pwm,  "iTrig2_Pwm");
    CMPM(ref_iTrig3_Cpwm, new_iTrig3_Cpwm, "iTrig3_Cpwm");
    CMPM(ref_iTrig3_Dpwm, new_iTrig3_Dpwm, "iTrig3_Dpwm");
    (void)tag;
}

static void compare(const char *tag)
{
#define ST_CMP(name, type) CMPM(stRef.name, stNew.name, #name);
    ST_LIST(ST_CMP)
#undef ST_CMP

    CMPM(refCpu, newCpu, "gHostCpuCtl");
    compare_tables(tag);
}

static void compare_img(const char *tag)
{
#define IMG_CMP(name, type) CMPM(imgRef.name, imgNew.name, #name);
    IMG_LIST(IMG_CMP)
#undef IMG_CMP
}

static void zero_counters(void)
{
    memset((void *)gHostCpuCtl, 0, sizeof(Uint16) * 7U);
}

/* Observed on the ref side only -- the counters describe what the firmware
 * does, and the two sides are already known to agree when they are read. */
static void post_ref_stats(void)
{
    if (refCpu[4] != 0U || refCpu[5] != 0U)
    {
        g_covEallow++;
    }
    if (stRef.EPWMFlagU != 0U || stRef.EPWMFlagV != 0U ||
        stRef.EPWMFlagW != 0U)
    {
        g_covSendFlagSet++;
    }
}

/* ---- one call on one side ------------------------------------------------ */

static void call_side(int side, int fn)
{
    if (side == 0)
    {
        switch (fn)
        {
            case 0: ref_InitSetPWM();   break;
            case 1: ref_OutPutPWMVC();  break;
            case 2: ref_OutPutPWMVF();  break;
            case 3: ref_DeadBandComp(); break;
            default: ref_SendPWM();     break;
        }
    }
    else
    {
        switch (fn)
        {
            case 0: new_InitSetPWM();   break;
            case 1: new_OutPutPWMVC();  break;
            case 2: new_OutPutPWMVF();  break;
            case 3: new_DeadBandComp(); break;
            default: new_SendPWM();     break;
        }
    }
}

/* One isolated call: the same image goes to both sides, the shared globals are
 * restored in between, and both results are compared field by field. */
static void do_step(int fn, const char *tag)
{
    g_cases++;
    g_covFn[fn]++;

    /* ref side */
    img_load(&imgIn);
    st_load_ref(&stIn);
    zero_counters();
    call_side(0, fn);
    img_save(&imgRef);
    st_snap_ref(&stRef);
    memcpy(refCpu, (const void *)gHostCpuCtl, sizeof(refCpu));
    post_ref_stats();

    /* new side */
    img_load(&imgIn);
    st_load_new(&stIn);
    zero_counters();
    call_side(1, fn);
    img_save(&imgNew);
    st_snap_new(&stNew);
    memcpy(newCpu, (const void *)gHostCpuCtl, sizeof(newCpu));

    compare(tag);
    compare_img(tag);
}

/* ---- random -------------------------------------------------------------- */

static unsigned long rnd(void)
{
    /* xorshift32: deterministic, so a failing run is reproducible */
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

static long rnd_int(long lo, long hi)
{
    return lo + (long)(rnd() % (unsigned long)(hi - lo + 1));
}

/* ---- input helpers ------------------------------------------------------- */

/* gPhase.OutPhase is `int` (16 bits after the shim's fold).  Writing a 16-bit
 * pattern through it is how the module's `Uint phase = gPhase.OutPhase;`
 * acquires values above 32767, i.e. how the `phase > 65531` clamp is reached. */
static void set_phase(Uint ph)
{
    imgIn.gPhase.OutPhase = (int)ph;
}

static Uint mk_phase(int sec, int rem)
{
    return (Uint)((Uint)sec * 5461U + (Uint)rem);
}

static void base_inputs(void)
{
    imgIn.gADC.DelayApply = 60;
    imgIn.gDeadBand.CompU = 0;
    imgIn.gDeadBand.CompV = 0;
    imgIn.gDeadBand.CompW = 0;
    imgIn.gDeadBand.DeadBand = 96;
    imgIn.gPhase.OutPhase = 0;

    stIn.gRatio = 4096;
    stIn.gPWM.U = 0;
    stIn.gPWM.V = 0;
    stIn.gPWM.W = 0;
    stIn.gPWM.gPWMPrd = 6000;
    stIn.gPWM.gPWMPrdApply = 6000;
    stIn.gPWM.AsynModle = 0;
    stIn.gPWM.PWMModle = MODLE_CPWM;
    stIn.gPWM.SoftPWMTune = 0;
    stIn.gPWM.SoftPWMCoff = 0;
    stIn.gPWM.PwmModeSwitchHF = 0;
    stIn.gPWM.PwmModeSwitchLF = 0;
    stIn.gPWM.gZeroLengthPhase = ZERO_VECTOR_NONE;
    stIn.gOutVolt.Volt = 0;
    stIn.EPWMFlagU = 0;
    stIn.EPWMFlagV = 0;
    stIn.EPWMFlagW = 0;
}

/* ---- 0. InitSetPWM ------------------------------------------------------- */

static void phase_init(void)
{
    static const Uint delays[] = {
        0U, 1U, 2U, 60U, 96U, 191U, 192U, 2999U, 3000U, 5999U, 6000U,
        6001U, 12000U, 65534U, 65535U
    };
    unsigned int i;

    for (i = 0; i < sizeof(delays) / sizeof(delays[0]); i++)
    {
        base_inputs();
        imgIn.gADC.DelayApply = delays[i];
        /* the register file starts dirty so "InitSetPWM forgot a field" cannot
         * hide behind a zero-initialised block */
        memset(&imgIn.EPwm1Regs, 0x5A, sizeof(imgIn.EPwm1Regs));
        memset(&imgIn.EPwm2Regs, 0x5A, sizeof(imgIn.EPwm2Regs));
        memset(&imgIn.EPwm3Regs, 0x5A, sizeof(imgIn.EPwm3Regs));

        if (delays[i] > 6000U)
        {
            g_covDelayUnder++;
        }
        do_step(0, "init");
    }
}

/* ---- 3. DeadBandComp ---------------------------------------------------- */

static void phase_db(void)
{
    static const Uint ratios[]  = { 0U, 1U, 4096U, 65535U };
    static const Uint prds[]    = { 0U, 100U, 6000U, 65535U };
    static const ZERO_LENGTH_PHASE_SELECT_ENUM zlps[4] = {
        ZERO_VECTOR_U, ZERO_VECTOR_V, ZERO_VECTOR_W, ZERO_VECTOR_NONE
    };
    static const long uvw[6][3] = {
        {     0L,     0L,     0L },
        {   100L,   200L,   300L },
        {  -100L,  -200L,  -300L },
        {  3000L,  3000L,  3000L },
        { 70000L, 65535L,     1L },
        { 2147483647L, -2147483647L, 0L }
    };
    static const int comp[4][3] = {
        {   0,   0,   0 },
        { 192, 192, 192 },
        { -192, -192, -192 },
        { 192, -96,   0 }
    };
    unsigned int ir, ip, iz, iu, ic;

    for (ir = 0; ir < 4; ir++)
    {
        for (ip = 0; ip < 4; ip++)
        {
            for (iz = 0; iz < 4; iz++)
            {
                for (iu = 0; iu < 6; iu++)
                {
                    for (ic = 0; ic < 4; ic++)
                    {
                        base_inputs();
                        stIn.gRatio = ratios[ir];
                        stIn.gPWM.gPWMPrdApply = prds[ip];
                        stIn.gPWM.gZeroLengthPhase = zlps[iz];
                        stIn.gPWM.U = uvw[iu][0];
                        stIn.gPWM.V = uvw[iu][1];
                        stIn.gPWM.W = uvw[iu][2];
                        imgIn.gDeadBand.CompU = (int)comp[ic][0];
                        imgIn.gDeadBand.CompV = (int)comp[ic][1];
                        imgIn.gDeadBand.CompW = (int)comp[ic][2];

                        if (ratios[ir] == 0U) { g_covDbRatioZero++; }
                        else                  { g_covDbRatioNonZero++; }
                        g_covZeroSel[iz]++;
                        if (stIn.gPWM.U > (long)prds[ip]) { g_covIqSat[0]++; }
                        else if (stIn.gPWM.U < 0L)        { g_covIqSat[2]++; }
                        else                              { g_covIqSat[1]++; }

                        do_step(3, "db");
                    }
                }
            }
        }
    }
}

/* ---- 1. OutPutPWMVC ----------------------------------------------------- */

static void phase_vc(void)
{
    static const Uint ratios[] = { 0U, 1U, 2048U, 4096U, 8192U, 65535U };
    static const Uint prds[]   = { 0U, 100U, 6000U, 65535U };
    static const ZERO_LENGTH_PHASE_SELECT_ENUM zlps[4] = {
        ZERO_VECTOR_U, ZERO_VECTOR_V, ZERO_VECTOR_W, ZERO_VECTOR_NONE
    };
    static const int comps[4] = { 0, 192, -192, 1000 };
    Uint ph;
    unsigned int ir, ip, iz, ic, k;

    /* 1a. phase sweep x ratio x period, NONE selected.
     *
     * `ph` is a 16-bit Uint, so `for (ph = 0; ph < 65536; ph += 997)` would
     * never terminate (the loop variable wraps before it can reach 65536).
     * The sweep is therefore driven by a 32-bit counter: 96 evenly spaced
     * points, 65536 / 96 = 682.67. */
    for (k = 0; k < 96U; k++)
    {
        ph = (Uint)((unsigned long)k * 683UL);

        for (ir = 0; ir < 6; ir++)
        {
            for (ip = 0; ip < 4; ip++)
            {
                base_inputs();
                set_phase(ph);
                stIn.gRatio = ratios[ir];
                stIn.gPWM.gPWMPrdApply = prds[ip];
                stIn.gPWM.gZeroLengthPhase = ZERO_VECTOR_NONE;
                do_step(1, "vc.sweep");
            }
        }
    }

    /* 1b. every quadrant boundary of the 16384 / 21845 arithmetic */
    {
        static const Uint edge[] = {
            0U, 1U, 16383U, 16384U, 16385U, 21844U, 21845U, 21846U,
            32767U, 32768U, 32769U, 43690U, 49151U, 54612U, 65531U,
            65532U, 65533U, 65534U, 65535U
        };
        unsigned int ie;

        for (ie = 0; ie < sizeof(edge) / sizeof(edge[0]); ie++)
        {
            for (iz = 0; iz < 4; iz++)
            {
                for (ic = 0; ic < 4; ic++)
                {
                    base_inputs();
                    set_phase(edge[ie]);
                    stIn.gRatio = 4096U;
                    stIn.gPWM.gPWMPrdApply = 6000U;
                    stIn.gPWM.gZeroLengthPhase = zlps[iz];
                    imgIn.gDeadBand.CompU = (int)comps[ic];
                    imgIn.gDeadBand.CompV = (int)-comps[ic];
                    imgIn.gDeadBand.CompW = (int)(comps[ic] / 2);
                    g_covZeroSel[iz]++;
                    do_step(1, "vc.edge");
                }
            }
        }
    }
}

/* ---- 2. OutPutPWMVF ----------------------------------------------------- */

static void phase_vf(void)
{
    static const int rems[] = { 0, 1, 1000, 2730, 5460 };
    static const Uint ratios[] = { 0U, 1U, 2048U, 4096U, 8192U, 65535U };
    static const Uint prds[] = { 0U, 100U, 6000U, 65535U };
    static const Uint modules[3] = { MODLE_CPWM, MODLE_DPWM, 2U };
    static const int comps[3] = { 0, 192, -192 };
    int sec, irem, imod, ir, ip, ic;
    Uint ph;

    for (sec = 0; sec < 12; sec++)
    {
        for (irem = 0; irem < 5; irem++)
        {
            for (imod = 0; imod < 3; imod++)
            {
                for (ir = 0; ir < 6; ir++)
                {
                    for (ip = 0; ip < 4; ip++)
                    {
                        for (ic = 0; ic < 3; ic++)
                        {
                            base_inputs();
                            ph = mk_phase(sec, rems[irem]);
                            set_phase(ph);
                            stIn.gPWM.PWMModle = modules[imod];
                            stIn.gRatio = ratios[ir];
                            stIn.gPWM.gPWMPrdApply = prds[ip];
                            imgIn.gDeadBand.CompU = (int)comps[ic];
                            imgIn.gDeadBand.CompV = (int)-comps[ic];
                            imgIn.gDeadBand.CompW = 0;

                            g_covSector[sec]++;
                            if (modules[imod] == MODLE_CPWM)
                            {
                                g_covVfCpwm++;
                                if ((sec & 1) != 0) { g_covOddCpwm++; }
                            }
                            else if (modules[imod] == MODLE_DPWM)
                            {
                                g_covVfDpwm++;
                                if ((sec & 1) != 0) { g_covOddDpwm++; }
                                if ((((sec + 1) >> 1) & 1) != 0)
                                {
                                    g_covDpwmInverse++;
                                }
                            }
                            if ((sec >> 1) & 1) { g_covSec60Odd++; }

                            do_step(2, "vf");
                        }
                    }
                }
            }
        }
    }

    /* 2b. the phase clamp: OutPhase reinterpreted as 65532..65535 */
    {
        unsigned int k;

        for (k = 0; k < 4U; k++)
        {
            base_inputs();
            set_phase((Uint)(65532U + k));
            stIn.gPWM.PWMModle = MODLE_DPWM;
            stIn.gRatio = 4096U;
            stIn.gPWM.gPWMPrdApply = 6000U;
            g_covPhaseClamp++;
            do_step(2, "vf.clamp");
        }
    }

    /* 2c. force m_TotalLength > gPWMPrdApply: tiny period, big ratio */
    {
        static const Uint tinyPrd[] = { 1U, 2U, 3U, 7U, 50U };
        unsigned int it, is;

        for (it = 0; it < sizeof(tinyPrd) / sizeof(tinyPrd[0]); it++)
        {
            for (is = 0; is < 12U; is++)
            {
                base_inputs();
                set_phase(mk_phase((int)is, 2000));
                stIn.gPWM.PWMModle = (is & 1U) ? MODLE_DPWM : MODLE_CPWM;
                stIn.gRatio = 65535U;
                stIn.gPWM.gPWMPrdApply = tinyPrd[it];
                g_covTotalOver++;
                do_step(2, "vf.over");
            }
        }
    }
}

/* ---- 4. SendPWM --------------------------------------------------------- */

static void phase_send(void)
{
    static const long uvwPat[6][3] = {
        {     0L,     0L,     0L },
        {     1L,     1L,     1L },
        {    -1L,    -1L,    -1L },
        {  3000L,  3000L,  3000L },
        {  7000L,  7000L,  7000L },
        { 65536L,     0L,    -5L }
    };
    static const Uint cmpaPat[3] = { 0U, 1U, 3000U };
    unsigned int iflag, iu, ic;

    for (iflag = 0; iflag < 8; iflag++)
    {
        for (iu = 0; iu < 6; iu++)
        {
            for (ic = 0; ic < 3; ic++)
            {
                base_inputs();
                stIn.EPWMFlagU = (Uint)(iflag & 1U);
                stIn.EPWMFlagV = (Uint)((iflag >> 1) & 1U);
                stIn.EPWMFlagW = (Uint)((iflag >> 2) & 1U);
                stIn.gPWM.U = uvwPat[iu][0];
                stIn.gPWM.V = uvwPat[iu][1];
                stIn.gPWM.W = uvwPat[iu][2];
                stIn.gPWM.gPWMPrdApply = 6000U;
                imgIn.gADC.DelayApply = 60;

                /* the read-back side of the state machine */
                memset(&imgIn.EPwm1Regs, 0x11, sizeof(imgIn.EPwm1Regs));
                memset(&imgIn.EPwm2Regs, 0x22, sizeof(imgIn.EPwm2Regs));
                memset(&imgIn.EPwm3Regs, 0x33, sizeof(imgIn.EPwm3Regs));
                imgIn.EPwm1Regs.CMPA.half.CMPA = cmpaPat[ic];
                imgIn.EPwm2Regs.CMPA.half.CMPA = cmpaPat[ic];
                imgIn.EPwm3Regs.CMPA.half.CMPA = cmpaPat[ic];

                if (iflag != 0U) { g_covSendFlagClear++; }
                if (stIn.gPWM.U == 0L && cmpaPat[ic] != 0U)
                {
                    g_covSendZeroCmpa++;
                }
                if (stIn.gPWM.U != 0L && cmpaPat[ic] == 0U)
                {
                    g_covSendNzCmpaZero++;
                }

                do_step(4, "send");
            }
        }
    }

    /* 4b. delay > period on the CMPB line */
    {
        static const Uint dly[] = { 6000U, 6001U, 65535U };
        unsigned int id;

        for (id = 0; id < sizeof(dly) / sizeof(dly[0]); id++)
        {
            base_inputs();
            imgIn.gADC.DelayApply = dly[id];
            stIn.gPWM.gPWMPrdApply = 100U;
            stIn.gPWM.U = 50L;
            stIn.gPWM.V = 50L;
            stIn.gPWM.W = 50L;
            memset(&imgIn.EPwm1Regs, 0, sizeof(imgIn.EPwm1Regs));
            memset(&imgIn.EPwm2Regs, 0, sizeof(imgIn.EPwm2Regs));
            memset(&imgIn.EPwm3Regs, 0, sizeof(imgIn.EPwm3Regs));
            g_covDelayUnder++;
            do_step(4, "send.delay");
        }
    }
}

/* ---- 5. sequence: state carried from call to call ---------------------- */

static struct IMAGE imgCarryRef;
static struct IMAGE imgCarryNew;
static struct STATE stCarryRef;
static struct STATE stCarryNew;

/* The inputs split across two structs: gPhase / gADC / gDeadBand are owned by
 * other translation units (the mock), gPWM / gRatio are defined by MotorPWM.c
 * itself and renamed per side.  Two macros, because the members live in
 * different structs. */
#define CARRY_IMG(field, val)                                              \
    do {                                                                   \
        imgCarryRef.field = (val);                                         \
        imgCarryNew.field = (val);                                         \
    } while (0)

#define CARRY_ST(field, val)                                               \
    do {                                                                   \
        stCarryRef.field = (val);                                          \
        stCarryNew.field = (val);                                          \
    } while (0)

static void carry_begin(void)
{
    imgCarryRef = imgIn;
    imgCarryNew = imgIn;
    stCarryRef = stIn;
    stCarryNew = stIn;
}

/* Same as do_step, except each side loads and saves its OWN carry: the state
 * call N produced is the state call N+1 sees.  That is the only way to test
 * the EPWMFlag handshake, which is a two-call protocol (SendPWM sets the flag
 * and switches AQCTLA away from the normal mode; the next call sees the flag
 * set and switches it back). */
static void do_seq_step(int fn, const char *tag)
{
    g_cases++;
    g_covFn[fn]++;
    g_covSeq++;

    img_load(&imgCarryRef);
    st_load_ref(&stCarryRef);
    zero_counters();
    call_side(0, fn);
    img_save(&imgCarryRef);
    st_snap_ref(&stCarryRef);
    memcpy(refCpu, (const void *)gHostCpuCtl, sizeof(refCpu));
    post_ref_stats();

    img_load(&imgCarryNew);
    st_load_new(&stCarryNew);
    zero_counters();
    call_side(1, fn);
    img_save(&imgCarryNew);
    st_snap_new(&stCarryNew);
    memcpy(newCpu, (const void *)gHostCpuCtl, sizeof(newCpu));

    compare(tag);
    compare_img(tag);
}

static void phase_seq(void)
{
    Uint ph = 0U;
    int i;

    base_inputs();
    imgIn.gADC.DelayApply = 60;
    memset(&imgIn.EPwm1Regs, 0x5A, sizeof(imgIn.EPwm1Regs));
    memset(&imgIn.EPwm2Regs, 0x5A, sizeof(imgIn.EPwm2Regs));
    memset(&imgIn.EPwm3Regs, 0x5A, sizeof(imgIn.EPwm3Regs));
    carry_begin();

    do_seq_step(0, "seq.init");

    for (i = 0; i < 240; i++)
    {
        ph = (Uint)(ph + 2731U);
        CARRY_IMG(gPhase.OutPhase, (int)ph);
        CARRY_IMG(gADC.DelayApply, 60U);
        CARRY_IMG(gDeadBand.CompU, 96);
        CARRY_IMG(gDeadBand.CompV, -48);
        CARRY_IMG(gDeadBand.CompW, 192);
        CARRY_ST(gPWM.PWMModle, (Uint)((i / 60) & 1));
        CARRY_ST(gPWM.gPWMPrdApply, (i % 7 == 0) ? 3000U : 6000U);
        CARRY_ST(gPWM.gPWMPrd, 6000U);
        CARRY_ST(gRatio, (i % 5 == 0) ? 0U : 4096U);

        /* every third step run the VC path so the two public entry points
         * share one state machine, exactly as the firmware does */
        do_seq_step((i % 3 == 2) ? 1 : 2, "seq");

        /* now and then force a zero output so the flag-clear arm is reached on
         * the following call */
        if ((i % 17) == 16)
        {
            CARRY_ST(gPWM.U, 0L);
            CARRY_ST(gPWM.V, 0L);
            CARRY_ST(gPWM.W, 0L);
            do_seq_step(4, "seq.zero");
        }
    }
}

/* ---- 6. random ---------------------------------------------------------- */

static void phase_random(long n)
{
    long i;

    for (i = 0; i < n; i++)
    {
        unsigned long fn = rnd() % 5UL;

        base_inputs();
        set_phase((Uint)(rnd() & 0xFFFFUL));
        imgIn.gADC.DelayApply = (Uint)(rnd() & 0xFFFFUL);
        imgIn.gDeadBand.CompU = (int)rnd_int(-32768, 32767);
        imgIn.gDeadBand.CompV = (int)rnd_int(-32768, 32767);
        imgIn.gDeadBand.CompW = (int)rnd_int(-32768, 32767);

        stIn.gRatio = (Uint)(rnd() & 0xFFFFUL);
        stIn.gPWM.gPWMPrd = (Uint)(rnd() & 0xFFFFUL);
        stIn.gPWM.gPWMPrdApply = (Uint)(rnd() & 0xFFFFUL);
        stIn.gPWM.PWMModle = (Uint)(rnd() & 1UL);
        stIn.gPWM.gZeroLengthPhase = (ZERO_LENGTH_PHASE_SELECT_ENUM)(rnd() % 4UL);
        stIn.gPWM.U = (long)rnd();
        stIn.gPWM.V = (long)rnd();
        stIn.gPWM.W = (long)rnd();
        stIn.EPWMFlagU = (Uint)(rnd() & 1UL);
        stIn.EPWMFlagV = (Uint)(rnd() & 1UL);
        stIn.EPWMFlagW = (Uint)(rnd() & 1UL);

        imgIn.EPwm1Regs.CMPA.half.CMPA = (Uint)(rnd() & 0xFFFFUL);
        imgIn.EPwm2Regs.CMPA.half.CMPA = (Uint)(rnd() & 0xFFFFUL);
        imgIn.EPwm3Regs.CMPA.half.CMPA = (Uint)(rnd() & 0xFFFFUL);
        imgIn.EPwm1Regs.TBPRD = (Uint)(rnd() & 0xFFFFUL);
        imgIn.EPwm2Regs.TBPRD = (Uint)(rnd() & 0xFFFFUL);
        imgIn.EPwm3Regs.TBPRD = (Uint)(rnd() & 0xFFFFUL);
        imgIn.EPwm1Regs.AQCTLA.all = (Uint)(rnd() & 0xFFFFUL);
        imgIn.EPwm2Regs.AQCTLA.all = (Uint)(rnd() & 0xFFFFUL);
        imgIn.EPwm3Regs.AQCTLA.all = (Uint)(rnd() & 0xFFFFUL);
        imgIn.EPwm1Regs.CMPCTL.all = (Uint)(rnd() & 0xFFFFUL);
        imgIn.EPwm2Regs.CMPCTL.all = (Uint)(rnd() & 0xFFFFUL);
        imgIn.EPwm3Regs.CMPCTL.all = (Uint)(rnd() & 0xFFFFUL);

        do_step((int)fn, "random");
    }
}

/* ------------------------------------------------------------------------- */
/* harness self-check                                                        */
/* ------------------------------------------------------------------------- */

/* Everything here tests the BENCH, not the module.  A bench that silently
 * stopped folding `int` to 16 bits, or whose compare macro stopped comparing,
 * would report a clean run for a wrong rewrite -- so those two properties are
 * verified from the outside on every run. */
static int bench_selfcheck(void)
{
    long saved;
    Uint a16;
    Uint b16;
    struct STATE s1;
    struct STATE s2;

    /* (1) the shim's `#define int short` fold is what makes Uint/int storage
     * match the C28x.  sizeof(Uint) is checked in main(); check that an
     * out-of-range constant really truncates rather than widening. */
    a16 = (Uint)0x00018000UL;
    if (a16 != 0x8000U)
    {
        printf("SELFCHECK: Uint does not truncate to 16 bits (got 0x%04X)\n",
               (unsigned)a16);
        return 1;
    }
    /* the same truncation the module performs on m_iT1Length:
     *   (Uint)(((long)30000 * (long)6000) >> 12) */
    b16 = (Uint)(((long)30000 * (long)6000) >> 12);
    if (b16 != 43945U)
    {
        printf("SELFCHECK: long arithmetic is not 32-bit (got %u, want 43945)\n",
               (unsigned)b16);
        return 1;
    }

    /* (2) CMPM must actually report.  Corrupt one byte, expect exactly one
     * extra mismatch, then restore the counter. */
    memset(&s1, 0x11, sizeof(s1));
    memset(&s2, 0x11, sizeof(s2));
    saved = g_mism;
    {
        long shown = g_mismShown;
        s2.gPWM.U = 0x1234L;
        /* same expression CMPM uses, kept local so the counter is not shared */
        if (memcmp(&s1.gPWM, &s2.gPWM, sizeof(s1.gPWM)) == 0)
        {
            printf("SELFCHECK: the compare macro cannot see a 1-field diff\n");
            return 1;
        }
        g_mism = saved;
        g_mismShown = shown;
    }

    /* (3) the per-side load/snap pair must round-trip, or the two sides would
     * not be given the same inputs. */
    stIn.gPWM.U = 0x0BADF00DL;
    stIn.gRatio = 0x1234U;
    stIn.EPWMFlagV = 1U;
    st_load_new(&stIn);
    st_snap_new(&s1);
    if (s1.gPWM.U != 0x0BADF00DL || s1.gRatio != 0x1234U ||
        s1.EPWMFlagV != 1U)
    {
        printf("SELFCHECK: st_load/st_snap does not round-trip\n");
        return 1;
    }

    /* (4) the register image must round-trip too -- it is the module's main
     * output, and a struct copy that silently dropped a field would make every
     * InitSetPWM comparison vacuous.  This exercises BOTH directions
     * (img_load writes the global, img_save reads it) because the two are
     * generated from the same list and a mistake in either would be invisible
     * to the other. */
    memset(&imgIn.EPwm1Regs, 0x3C, sizeof(imgIn.EPwm1Regs));
    imgIn.EPwm1Regs.TBPRD = 0xABCDU;
    imgIn.EPwm1Regs.CMPA.half.CMPA = 0x1357U;
    img_load(&imgIn);                    /* global := imgIn  */
    img_save(&imgRef);                   /* imgRef := global */
    memset(&imgIn.EPwm1Regs, 0, sizeof(imgIn.EPwm1Regs));
    img_load(&imgIn);                    /* global := zeros  */
    img_load(&imgRef);                   /* global := imgRef */
    if (EPwm1Regs.TBPRD != 0xABCDU || EPwm1Regs.CMPA.half.CMPA != 0x1357U)
    {
        printf("SELFCHECK: the EPWM register image does not round-trip "
               "(TBPRD=0x%04X CMPA=0x%04X)\n",
               (unsigned)EPwm1Regs.TBPRD, (unsigned)EPwm1Regs.CMPA.half.CMPA);
        return 1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* main                                                                      */
/* ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long random = (argc > 1) ? atol(argv[1]) : 20000L;
    long i;

    g_rng = 0x12345678UL;

    /* Unbuffered stdout: a trapping input loses the whole progress log on a
     * fully buffered stream. */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Width pins.  The case shim folds `int` to the target's 16 bits so that
     * Uint/int storage matches the C28x; if that fold is ever lost, host-only
     * 32-bit storage silently hides truncation-only mutants. */
    if (sizeof(Uint) != 2U || sizeof(int) != 2U || sizeof(long) != 4U ||
        sizeof(Uint16) != 2U || sizeof(Uint32) != 4U || sizeof(Ulong) != 4U)
    {
        printf("FATAL: host type widths differ from the target "
               "(Uint=%u int=%u long=%u Ulong=%u)\n",
               (unsigned)sizeof(Uint), (unsigned)sizeof(int),
               (unsigned)sizeof(long), (unsigned)sizeof(Ulong));
        return 2;
    }

    /* Layout pin: the EPWM block is compared as a whole struct, so its host
     * size must not silently change between the two sides (it cannot -- both
     * are the same host struct -- but a header edit that reorders the real
     * device struct would change what is compared, so pin the value that the
     * real include/device/DSP2803x_EPwm.h produces on the host).
     *
     * PWM_OUT_STRUCT is deliberately NOT size-pinned: it contains an enum,
     * which the C28x stores in 16 bits and MSVC in 32, so its sizeof differs
     * between host and target.  The padding is identical on the ref and the
     * new side, so it cannot manufacture or hide a difference; it only means
     * the compared footprint is the host's. */
    if (sizeof(struct EPWM_REGS) != 116U)
    {
        printf("FATAL: EPWM_REGS host layout changed (expected 116, got %u)\n",
               (unsigned)sizeof(struct EPWM_REGS));
        return 2;
    }

    if (bench_selfcheck() != 0)
    {
        printf("FATAL: bench self-check failed, results would be meaningless\n");
        return 2;
    }

    phase_init();
    phase_db();
    phase_vc();
    phase_vf();
    phase_send();
    phase_seq();
    phase_random(random);

    printf("MotorPWM difftest: cases=%ld mismatches=%ld\n", g_cases, g_mism);
    printf("input reach: fn[]=%ld/%ld/%ld/%ld/%ld seq=%ld\n",
           g_covFn[0], g_covFn[1], g_covFn[2], g_covFn[3], g_covFn[4],
           g_covSeq);
    printf("input reach: cpwm=%ld dpwm=%ld inv=%ld sec60odd=%ld "
           "oddCpwm=%ld oddDpwm=%ld totalOver=%ld clamp=%ld\n",
           g_covVfCpwm, g_covVfDpwm, g_covDpwmInverse, g_covSec60Odd,
           g_covOddCpwm, g_covOddDpwm, g_covTotalOver, g_covPhaseClamp);
    printf("input reach: dbRatio=%ld/%ld zeroSel=%ld/%ld/%ld/%ld "
           "iqsat=%ld/%ld/%ld delayUnder=%ld eallow=%ld\n",
           g_covDbRatioZero, g_covDbRatioNonZero,
           g_covZeroSel[0], g_covZeroSel[1], g_covZeroSel[2], g_covZeroSel[3],
           g_covIqSat[0], g_covIqSat[1], g_covIqSat[2],
           g_covDelayUnder, g_covEallow);
    printf("input reach: sendZeroCmpa=%ld sendNzCmpaZero=%ld "
           "flagClear=%ld flagSet=%ld sec30Clamp=%ld\n",
           g_covSendZeroCmpa, g_covSendNzCmpaZero,
           g_covSendFlagClear, g_covSendFlagSet, g_covSec30Clamp);
    printf("input reach: sector=");
    for (i = 0; i < 12; i++)
    {
        printf("%ld%s", g_covSector[i], (i == 11) ? "\n" : "/");
    }

    /* Every entry point must have been driven. */
    for (i = 0; i < 5; i++)
    {
        if (g_covFn[i] == 0)
        {
            printf("FATAL: entry point %ld was never driven\n", i);
            return 2;
        }
    }
    /* Every branch the hand-built phases exist to reach. */
    if (g_covVfCpwm == 0 || g_covVfDpwm == 0 || g_covDpwmInverse == 0 ||
        g_covSec60Odd == 0 || g_covOddCpwm == 0 || g_covOddDpwm == 0 ||
        g_covTotalOver == 0 || g_covPhaseClamp == 0 ||
        g_covDbRatioZero == 0 || g_covDbRatioNonZero == 0 ||
        g_covZeroSel[0] == 0 || g_covZeroSel[1] == 0 ||
        g_covZeroSel[2] == 0 || g_covZeroSel[3] == 0 ||
        g_covIqSat[0] == 0 || g_covIqSat[1] == 0 || g_covIqSat[2] == 0 ||
        g_covSendZeroCmpa == 0 || g_covSendNzCmpaZero == 0 ||
        g_covSendFlagClear == 0 || g_covSendFlagSet == 0 ||
        g_covDelayUnder == 0 || g_covSeq == 0)
    {
        printf("FATAL: a phase branch was never reached as an input\n");
        return 2;
    }
    for (i = 0; i < 12; i++)
    {
        if (g_covSector[i] == 0)
        {
            printf("FATAL: sector %ld was never driven\n", i);
            return 2;
        }
    }

    /* The one arm that is unreachable by construction:
     *     if (m_iSec30 > 11) { m_iSec30 = 0; }
     * `phase` is clamped to 65531 two lines earlier and 65531 / 5461 == 11,
     * so m_iSec30 can never exceed 11.  It is dead code, kept because the
     * rewrite must not delete behaviour -- if a mutant ever makes it reachable
     * that is a real behavioural change and must show up as a mismatch, not as
     * this counter. */
    if (g_covSec30Clamp != 0)
    {
        printf("FATAL: the m_iSec30 > 11 clamp became reachable\n");
        return 2;
    }

    return g_mism == 0 ? 0 : 1;
}
