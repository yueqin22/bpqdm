/* Differential test driver for MotorInfoCollect.
 *
 * The module is the ADC-facing acquisition layer: three entry points, and one
 * hardware boundary that all of them share -- the ADC result registers.
 *
 *   GetCurExcursion()   reads gMainStatus.RunStep + gExcursionInfo.Iu/Iv
 *                       writes gExcursionInfo.*, gMainStatus.StatusWord.bit.RunEnable,
 *                              gError.ErrorCode.all
 *   GetUDCInfo()        reads AdcResult.ADCRESULT3 + gUDC.{Coff,uDC,uDCFilter,uDCBigFilter}
 *                       writes gUDC.{uDC,uDCFilter,uDCBigFilter}
 *   GetCurrentInfo()    reads AdcResult.ADCRESULT1/5/4/6/7/8, gExcursionInfo.Iu/Iv/ErrIu/ErrIv,
 *                              gCurSamp.Coff, gUVCoff.UDivV, gShortGnd.ShortCur
 *                       writes gExcursionInfo.{Iu,Iv,IuValue,IvValue}, gShortGnd.ShortCur,
 *                              gIUVWQ24.U/V/W, gTemperature.TempAD,
 *                              gAI.{ai1Total,ai2Total,ai3Total,aiCounter}
 *
 * Two kinds of state, and the driver treats them differently:
 *
 *   per-side   gExcursionInfo / gUDC / gIUVWQ24 / gTemperature / gAI / gCurSamp
 *              are DEFINED by this module, so ref and new each own a copy
 *              (renamed ref_/new_). Both are installed from the same input
 *              image and then compared field by field.
 *   shared     AdcResult / gMainStatus / gError / gShortGnd / gUVCoff belong to
 *              other modules. mock_infocollect.c owns one instance of each, so
 *              both sides write into the SAME memory -- which is exactly what
 *              makes "the rewrite stopped writing a field" observable.
 *
 * Every input is pre-filled with non-trivial values before each side runs. The
 * structures are zeroed first and then filled through memcpy of a fully
 * initialised input image, so any padding byte is 0 on both sides and a plain
 * memcmp of the whole structure is meaningful.
 *
 * ---- why the shared objects are pre-set to all-ones ----
 *
 * A ref-vs-new comparison only sees a change in the NEW side; a mutation that
 * makes new stop writing a field is visible only if the value already sitting
 * there differs from the value the correct code would have written. So the two
 * objects a rewrite might silently stop touching -- gMainStatus.StatusWord and
 * gError.ErrorCode -- are installed with every bit set (dirty_status()). Against
 * an all-zero image, `StatusWord.bit.RunEnable = 1` mutated to `= 0` produces
 * byte-identical state on both sides and is invisible.
 *
 * ---- the `int` / `Uint` width caveat ----
 *
 * MotorDefine.h types `Uint` as `unsigned int` and the structures use plain
 * `int` freely. On C28x both are 16 bits; on the host both are 32. Both sides of
 * this test are host code, so the COMPARISON stays valid, but the host does not
 * reproduce the target's truncation and wrap points. Concretely: gUDC.uDC and
 * gUDC.uDCFilter are Uint, so `(a + b) >> 1` wraps at 65536 on the target and at
 * 4G on the host, and CUR_EXCURSION_STRUCT's int fields saturate at different
 * places. This is a known, documented limitation of the host harness -- it is
 * stated here rather than papered over with casts, because casts would change
 * the expression the rewrite is supposed to preserve.
 *
 * ---- what is NOT covered ----
 *
 *   - the target's 16-bit wrap/truncation points (above);
 *   - `__IQsat`: the host gets a semantically equivalent replacement from the
 *     case shim, not the C28x builtin. Saturation VALUES are compared, so a
 *     rewrite that changes the clamp arguments is still caught; what is not
 *     proven is that the host clamp is bit-identical to MINL/MAXL on every
 *     input. The boundary values that matter (C_MAX_PER, -C_MAX_PER) are in the
 *     enumeration;
 *   - float behaviour: this module has none.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MotorInfoCollectInclude.h"

#define FAIL_LIMIT  20

extern void ref_GetCurExcursion(void);
extern void ref_GetUDCInfo(void);
extern void ref_GetCurrentInfo(void);
extern CUR_EXCURSION_STRUCT   ref_gExcursionInfo;
extern UDC_STRUCT             ref_gUDC;
extern UVW_STRUCT_Q24         ref_gIUVWQ24;
extern TEMPLETURE_STRUCT      ref_gTemperature;
extern AI_STRUCT              ref_gAI;
extern IUVW_SAMPLING_STRUCT   ref_gCurSamp;

extern void new_GetCurExcursion(void);
extern void new_GetUDCInfo(void);
extern void new_GetCurrentInfo(void);
extern CUR_EXCURSION_STRUCT   new_gExcursionInfo;
extern UDC_STRUCT             new_gUDC;
extern UVW_STRUCT_Q24         new_gIUVWQ24;
extern TEMPLETURE_STRUCT      new_gTemperature;
extern AI_STRUCT              new_gAI;
extern IUVW_SAMPLING_STRUCT   new_gCurSamp;

/* ---- per-side snapshot --------------------------------------------------- */

struct SIDE
{
    CUR_EXCURSION_STRUCT exc;
    UDC_STRUCT           udc;
    UVW_STRUCT_Q24       q24;
    TEMPLETURE_STRUCT    tmp;
    AI_STRUCT            ai;
    IUVW_SAMPLING_STRUCT samp;
};

static struct SIDE S[2];

/* ---- input image --------------------------------------------------------- */

static CUR_EXCURSION_STRUCT   inExc;
static UDC_STRUCT             inUdc;
static IUVW_SAMPLING_STRUCT   inSamp;
static TEMPLETURE_STRUCT      inTmp;
static AI_STRUCT              inAi;
static RUN_STATUS_STRUCT      inSt;
static struct FAULT_CODE_INFOR_STRUCT_DEF inErr;
static SHORT_GND_STRUCT       inGnd;
static UV_BIAS_COFF_STRUCT    inUvc;
static Uint16                 inAdc[16];

static void install_shared(void)
{
    memcpy(&gMainStatus, &inSt, sizeof inSt);
    memcpy(&gError, &inErr, sizeof inErr);
    memcpy(&gShortGnd, &inGnd, sizeof inGnd);
    memcpy(&gUVCoff, &inUvc, sizeof inUvc);

    AdcResult.ADCRESULT0  = inAdc[0];
    AdcResult.ADCRESULT1  = inAdc[1];
    AdcResult.ADCRESULT2  = inAdc[2];
    AdcResult.ADCRESULT3  = inAdc[3];
    AdcResult.ADCRESULT4  = inAdc[4];
    AdcResult.ADCRESULT5  = inAdc[5];
    AdcResult.ADCRESULT6  = inAdc[6];
    AdcResult.ADCRESULT7  = inAdc[7];
    AdcResult.ADCRESULT8  = inAdc[8];
    AdcResult.ADCRESULT9  = inAdc[9];
    AdcResult.ADCRESULT10 = inAdc[10];
    AdcResult.ADCRESULT11 = inAdc[11];
    AdcResult.ADCRESULT12 = inAdc[12];
}

static void install_side(int side)
{
    if (side == 0)
    {
        memcpy(&ref_gExcursionInfo, &inExc, sizeof inExc);
        memcpy(&ref_gUDC, &inUdc, sizeof inUdc);
        memcpy(&ref_gCurSamp, &inSamp, sizeof inSamp);
        memcpy(&ref_gTemperature, &inTmp, sizeof inTmp);
        memcpy(&ref_gAI, &inAi, sizeof inAi);
    }
    else
    {
        memcpy(&new_gExcursionInfo, &inExc, sizeof inExc);
        memcpy(&new_gUDC, &inUdc, sizeof inUdc);
        memcpy(&new_gCurSamp, &inSamp, sizeof inSamp);
        memcpy(&new_gTemperature, &inTmp, sizeof inTmp);
        memcpy(&new_gAI, &inAi, sizeof inAi);
    }
}

static void snapshot(int side)
{
    if (side == 0)
    {
        memcpy(&S[0].exc,  &ref_gExcursionInfo, sizeof S[0].exc);
        memcpy(&S[0].udc,  &ref_gUDC, sizeof S[0].udc);
        memcpy(&S[0].q24,  &ref_gIUVWQ24, sizeof S[0].q24);
        memcpy(&S[0].tmp,  &ref_gTemperature, sizeof S[0].tmp);
        memcpy(&S[0].ai,   &ref_gAI, sizeof S[0].ai);
        memcpy(&S[0].samp, &ref_gCurSamp, sizeof S[0].samp);
    }
    else
    {
        memcpy(&S[1].exc,  &new_gExcursionInfo, sizeof S[1].exc);
        memcpy(&S[1].udc,  &new_gUDC, sizeof S[1].udc);
        memcpy(&S[1].q24,  &new_gIUVWQ24, sizeof S[1].q24);
        memcpy(&S[1].tmp,  &new_gTemperature, sizeof S[1].tmp);
        memcpy(&S[1].ai,   &new_gAI, sizeof S[1].ai);
        memcpy(&S[1].samp, &new_gCurSamp, sizeof S[1].samp);
    }
}

/* Shared mocks are written by both sides; they are compared separately because
 * they live outside the per-side structs. */
struct SHARED
{
    RUN_STATUS_STRUCT st;
    struct FAULT_CODE_INFOR_STRUCT_DEF err;
    SHORT_GND_STRUCT  gnd;
    UV_BIAS_COFF_STRUCT uvc;
    Uint16 adc[13];
};

static struct SHARED SH[2];

static void snap_shared(int side)
{
    memcpy(&SH[side].st,  &gMainStatus, sizeof SH[side].st);
    memcpy(&SH[side].err, &gError, sizeof SH[side].err);
    memcpy(&SH[side].gnd, &gShortGnd, sizeof SH[side].gnd);
    memcpy(&SH[side].uvc, &gUVCoff, sizeof SH[side].uvc);
    SH[side].adc[0]  = AdcResult.ADCRESULT0;
    SH[side].adc[1]  = AdcResult.ADCRESULT1;
    SH[side].adc[2]  = AdcResult.ADCRESULT2;
    SH[side].adc[3]  = AdcResult.ADCRESULT3;
    SH[side].adc[4]  = AdcResult.ADCRESULT4;
    SH[side].adc[5]  = AdcResult.ADCRESULT5;
    SH[side].adc[6]  = AdcResult.ADCRESULT6;
    SH[side].adc[7]  = AdcResult.ADCRESULT7;
    SH[side].adc[8]  = AdcResult.ADCRESULT8;
    SH[side].adc[9]  = AdcResult.ADCRESULT9;
    SH[side].adc[10] = AdcResult.ADCRESULT10;
    SH[side].adc[11] = AdcResult.ADCRESULT11;
    SH[side].adc[12] = AdcResult.ADCRESULT12;
}

/* ---- case runner --------------------------------------------------------- */

static long g_cases = 0;
static int  g_fails = 0;

static void report(const char *fn, const char *what)
{
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s: %s\n", fn, what);
    printf("  adc=%u,%u,%u,%u,%u,%u,%u,%u,%u  RunStep=%u\n",
           inAdc[0], inAdc[1], inAdc[2], inAdc[3], inAdc[4],
           inAdc[5], inAdc[6], inAdc[7], inAdc[8], inSt.RunStep);
    printf("  excIn: Iu=%d Iv=%d ErrIu=%d ErrIv=%d Cnt=%d EnCnt=%d ErrCnt=%d TotIu=%ld TotIv=%ld\n",
           inExc.Iu, inExc.Iv, inExc.ErrIu, inExc.ErrIv,
           inExc.Count, inExc.EnableCount, inExc.ErrCnt,
           (long)inExc.TotalIu, (long)inExc.TotalIv);
    printf("  udcIn: Coff=%u uDC=%u uDCF=%u uDCBF=%u   samp.Coff=%ld  uvc.UDivV=%u  gnd=%d\n",
           (unsigned)inUdc.Coff, (unsigned)inUdc.uDC, (unsigned)inUdc.uDCFilter,
           (unsigned)inUdc.uDCBigFilter, (long)inSamp.Coff,
           (unsigned)inUvc.UDivV, inGnd.ShortCur);
}

static void cmp(const char *fn, const char *what, const void *a, const void *b,
                size_t n)
{
    if (memcmp(a, b, n) != 0)
    {
        report(fn, what);
    }
}

static void compare(const char *fn)
{
    cmp(fn, "gExcursionInfo", &S[0].exc, &S[1].exc, sizeof S[0].exc);
    cmp(fn, "gUDC", &S[0].udc, &S[1].udc, sizeof S[0].udc);
    cmp(fn, "gIUVWQ24", &S[0].q24, &S[1].q24, sizeof S[0].q24);
    cmp(fn, "gTemperature", &S[0].tmp, &S[1].tmp, sizeof S[0].tmp);
    cmp(fn, "gAI", &S[0].ai, &S[1].ai, sizeof S[0].ai);
    cmp(fn, "gCurSamp", &S[0].samp, &S[1].samp, sizeof S[0].samp);

    cmp(fn, "gMainStatus", &SH[0].st, &SH[1].st, sizeof SH[0].st);
    cmp(fn, "gError", &SH[0].err, &SH[1].err, sizeof SH[0].err);
    cmp(fn, "gShortGnd", &SH[0].gnd, &SH[1].gnd, sizeof SH[0].gnd);
    cmp(fn, "gUVCoff", &SH[0].uvc, &SH[1].uvc, sizeof SH[0].uvc);
    cmp(fn, "AdcResult", SH[0].adc, SH[1].adc, sizeof SH[0].adc);
}

typedef void (*entry_t)(void);

static entry_t g_refFn[3];
static entry_t g_newFn[3];
static const char *g_fnName[3] = { "GetCurExcursion", "GetUDCInfo", "GetCurrentInfo" };

static void init_ptrs(void)
{
    g_refFn[0] = ref_GetCurExcursion; g_newFn[0] = new_GetCurExcursion;
    g_refFn[1] = ref_GetUDCInfo;      g_newFn[1] = new_GetUDCInfo;
    g_refFn[2] = ref_GetCurrentInfo;  g_newFn[2] = new_GetCurrentInfo;
}

static void run_case(int which)
{
    install_shared();
    install_side(0);
    g_refFn[which]();
    snapshot(0);
    snap_shared(0);

    install_shared();
    install_side(1);
    g_newFn[which]();
    snapshot(1);
    snap_shared(1);

    g_cases++;
    compare(g_fnName[which]);
}

/* ---- phases -------------------------------------------------------------- */

static void clear_inputs(void)
{
    memset(&inExc, 0, sizeof inExc);
    memset(&inUdc, 0, sizeof inUdc);
    memset(&inSamp, 0, sizeof inSamp);
    memset(&inTmp, 0, sizeof inTmp);
    memset(&inAi, 0, sizeof inAi);
    memset(&inSt, 0, sizeof inSt);
    memset(&inErr, 0, sizeof inErr);
    memset(&inGnd, 0, sizeof inGnd);
    memset(&inUvc, 0, sizeof inUvc);
    memset(inAdc, 0, sizeof inAdc);
}

/* Every bit of the two shared status objects set, so that a rewrite which stops
 * writing one of them -- or writes the wrong constant into it -- leaves the
 * pre-existing 1s behind and shows up. With an all-zero image, `RunEnable = 1`
 * changed to `RunEnable = 0` is indistinguishable from doing nothing at all, and
 * the `|=` in the fault path would have nothing to OR against. */
static void dirty_status(void)
{
    memset(&inSt, 0xFF, sizeof inSt);
    memset(&inErr, 0xFF, sizeof inErr);
}

/* GetCurExcursion: RunStep gate, the EnableCount ramp, the Count>=32 window, the
 * two -32768 guards and the 5120 comparison against abs(). All of them are
 * caller-visible edges, so all of them get walked.
 *
 * The interesting quantities are NOT the raw inputs but `TotalIu >> 5` and
 * `TotalIv >> 5`: that is what the two -32768 guards test and what the 5120
 * comparison sees. So the enumeration below is written in terms of the shifted
 * result and multiplied back out by 32 -- 5120*32, -5120*32, and the whole
 * 32-value window [-1048576, -1048545] that shifts to exactly -32768. Feeding
 * ErrIu/ErrIv (which GetCurExcursion only WRITES, never reads) with the 5120
 * edges, as an earlier revision of this driver did, exercises nothing here.
 *
 * TotalIu and TotalIv are swept independently (e x f) because the 5120 test is a
 * conjunction: `&&` and `||` only differ when exactly one of the two magnitudes
 * is below the threshold.
 *
 * `TotalIu` is written as `tot[e] - Iu`, not `tot[e]`: the module accumulates
 * (`TotalIu += Iu`) BEFORE shifting, so what the 5120 test and the -32768 guard
 * actually see is `(TotalIu + Iu) >> 5`. Setting the pre-accumulation value so
 * that the post-accumulation value is exactly tot[e] is what makes the boundary
 * land on the boundary. Getting this wrong is not a cosmetic slip: the first
 * version of this phase set TotalIu = 5120*32 with Iu = 0x1234, so the shifted
 * value was 5265 and `< 5120` vs `< 5121` were indistinguishable -- N15/N16 came
 * back MISSED. */
static void phase_excursion(void)
{
    static const Uint step[] = { STATUS_LOW_POWER, STATUS_STOP, 0, 2, 4, 255, 65535 };
    static const int enCnt[] = { 0, 1, 199, 200, 201, 65535 };
    static const int cnt[]   = { 0, 1, 31, 32, 33 };
    static const int errCnt[] = { 0, 5, 6, 7 };
    static const int iu[]    = { 0, 1234, -1234 };
    static const long tot[]  = {
        0L,
        5119L * 32L, 5120L * 32L, 5121L * 32L,
        -5119L * 32L, -5120L * 32L, -5121L * 32L,
        32767L * 32L, -32767L * 32L,
        1048575L, -1048576L, -1048545L, -1048577L, 0x12340000L
    };
    unsigned a, b, c, d, e, f, g;

    for (a = 0; a < sizeof step / sizeof step[0]; a++)
    {
        for (b = 0; b < sizeof enCnt / sizeof enCnt[0]; b++)
        {
            for (c = 0; c < sizeof cnt / sizeof cnt[0]; c++)
            {
                for (d = 0; d < sizeof errCnt / sizeof errCnt[0]; d++)
                {
                    for (e = 0; e < sizeof tot / sizeof tot[0]; e++)
                    {
                        for (f = 0; f < sizeof tot / sizeof tot[0]; f++)
                        {
                            for (g = 0; g < sizeof iu / sizeof iu[0]; g++)
                            {
                                clear_inputs();
                                dirty_status();
                                inSt.RunStep = step[a];
                                inExc.EnableCount = enCnt[b];
                                inExc.Count = cnt[c];
                                inExc.ErrCnt = errCnt[d];
                                inExc.Iu = iu[g];
                                inExc.Iv = -iu[(g + 1) % 3];
                                inExc.TotalIu = tot[e] - (long)inExc.Iu;
                                inExc.TotalIv = tot[f] - (long)inExc.Iv;
                                inExc.ErrIu = (int)(0x1111 + (int)d);
                                inExc.ErrIv = (int)(0x2222 - (int)c);
                                run_case(0);
                            }
                        }
                    }
                }
            }
        }
    }
}

/* GetUDCInfo: a pure fixed-point chain over four caller-chosen values. */
static void phase_udc(void)
{
    static const unsigned coff[] = { 0, 1, 1000, 32768, 65535, 65534 };
    static const unsigned udc[]  = { 0, 1, 32767, 32768, 65535, 65534 };
    unsigned a, b, c;

    for (a = 0; a < sizeof coff / sizeof coff[0]; a++)
    {
        for (b = 0; b < sizeof udc / sizeof udc[0]; b++)
        {
            for (c = 0; c < 3; c++)
            {
                clear_inputs();
                dirty_status();
                inAdc[3] = (Uint16)((udc[b] & 0x0FFF) ^ (c * 0x111));
                inUdc.Coff = coff[a];
                inUdc.uDC = udc[b];
                inUdc.uDCFilter = (unsigned)(udc[(b + 1) % 6]);
                inUdc.uDCBigFilter = (unsigned)(udc[(b + 2) % 6]);
                run_case(1);
            }
        }
    }
}

/* GetCurrentInfo: ADC -> offset -> two coefficient multiplies -> saturate, plus
 * the W reconstruction, the temperature filter and the AI accumulators. */
static void phase_current(void)
{
    static const Uint16 adc[] = { 0, 1, 0x7FFF, 0x8000, 0xFFF0, 0xFFFF, 0x1234, 0xABC0 };
    static const int    off[] = { 0, 1, -1, 5120, -5120, 32767, -32768 };
    static const long   cof[] = { 0, 1, -1, 8, -8, 0x1000000L, -0x1000000L };
    unsigned a, b, c, d;

    for (a = 0; a < sizeof adc / sizeof adc[0]; a++)
    {
        for (b = 0; b < sizeof off / sizeof off[0]; b++)
        {
            for (c = 0; c < sizeof cof / sizeof cof[0]; c++)
            {
                for (d = 0; d < 3; d++)
                {
                    clear_inputs();
                    dirty_status();
                    inAdc[1] = adc[a];
                    inAdc[5] = (Uint16)(adc[a] ^ 0x0555);
                    inAdc[4] = (Uint16)(adc[(a + 3) % 8]);
                    inAdc[6] = adc[b % 8];
                    inAdc[7] = adc[(b + 1) % 8];
                    inAdc[8] = adc[(b + 2) % 8];
                    inExc.Iu = off[b];
                    inExc.Iv = (int)(-off[b]);
                    inExc.ErrIu = off[(b + 3) % 7];
                    inExc.ErrIv = off[(b + 5) % 7];
                    inSamp.Coff = cof[c];
                    inUvc.UDivV = (unsigned)(cof[(c + 2) % 7] & 0xFFFF);
                    inGnd.ShortCur = off[(b + 1) % 7];
                    inTmp.TempAD = (unsigned)(adc[(a + 4) % 8]);
                    inAi.ai1Total = (unsigned long)(0x10000000UL + a);
                    inAi.ai2Total = (unsigned long)(0x20000000UL + b);
                    inAi.ai3Total = (unsigned long)(0x30000000UL + c);
                    inAi.aiCounter = (int)d;
                    run_case(2);
                }
            }
        }
    }
}

/* ---- random sweep -------------------------------------------------------- */

static unsigned long g_rng = 0x20261009UL;

static unsigned long rnd(void)
{
    g_rng = g_rng * 1103515245UL + 12345UL;
    return (g_rng >> 8) & 0xFFFFFFUL;
}

static void phase_random(long n)
{
    long k;

    for (k = 0; k < n; k++)
    {
        int which = (int)(rnd() % 3);

        clear_inputs();
        dirty_status();
        inAdc[1] = (Uint16)rnd();
        inAdc[3] = (Uint16)rnd();
        inAdc[4] = (Uint16)rnd();
        inAdc[5] = (Uint16)rnd();
        inAdc[6] = (Uint16)rnd();
        inAdc[7] = (Uint16)rnd();
        inAdc[8] = (Uint16)rnd();

        inSt.RunStep = (Uint)(rnd() % 8);
        inExc.Iu = (int)(rnd() % 65536) - 32768;
        inExc.Iv = (int)(rnd() % 65536) - 32768;
        inExc.ErrIu = (int)(rnd() % 65536) - 32768;
        inExc.ErrIv = (int)(rnd() % 65536) - 32768;
        inExc.Count = (int)(rnd() % 40);
        inExc.EnableCount = (int)(rnd() % 220);
        inExc.ErrCnt = (int)(rnd() % 8);
        inExc.TotalIu = (long)rnd() * 3 - 100000;
        inExc.TotalIv = (long)rnd() * 3 - 100000;

        inUdc.Coff = (unsigned)(rnd() % 65536);
        inUdc.uDC = (unsigned)(rnd() % 65536);
        inUdc.uDCFilter = (unsigned)(rnd() % 65536);
        inUdc.uDCBigFilter = (unsigned)(rnd() % 65536);

        inSamp.Coff = (long)rnd() * 7 - 1000000;
        inUvc.UDivV = (unsigned)(rnd() % 65536);
        inGnd.ShortCur = (int)(rnd() % 65536) - 32768;
        inTmp.TempAD = (unsigned)(rnd() % 65536);
        inAi.ai1Total = (unsigned long)rnd() * 1000;
        inAi.ai2Total = (unsigned long)rnd() * 1000;
        inAi.ai3Total = (unsigned long)rnd() * 1000;
        inAi.aiCounter = (int)(rnd() % 4096);

        run_case(which);
    }
}

/* ---- main ---------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 20000;
    long base;

    init_ptrs();

    phase_excursion();
    printf("  GetCurExcursion              : %ld\n", g_cases);

    base = g_cases;
    phase_udc();
    printf("  GetUDCInfo                   : %ld\n", g_cases - base);

    base = g_cases;
    phase_current();
    printf("  GetCurrentInfo               : %ld\n", g_cases - base);

    base = g_cases;
    phase_random(n);
    printf("  random mixed                 : %ld\n", g_cases - base);

    printf("MotorInfoCollect difftest: cases=%ld mismatches=%d\n",
           g_cases, g_fails);
    return g_fails ? 1 : 0;
}
