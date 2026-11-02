/* Differential test driver for DSP2803x_CpuTimers.
 *
 * The module has two entry points and one hardware boundary:
 *
 *   InitCpuTimers()                  writes three CPUTIMER_VARS and three
 *                                    register blocks
 *   ConfigCpuTimer(Timer, F, P)      writes the register block Timer points at,
 *                                    plus two float fields in *Timer
 *
 * Everything either function can do lands in one of six places, and all six are
 * compared field by field after every call:
 *
 *   CpuTimer0/1/2Regs    TIM.all, PRD.all, TCR.all, rsvd1, TPR.all, TPRH.all
 *   CpuTimer0/1/2        RegsAddr, InterruptCount, CPUFreqInMHz, PeriodInUSec
 *
 * The register blocks are the mock's memory, shared by both sides, so "wrote the
 * wrong bit", "wrote TCR twice", "forgot TPRH" all show up. The three
 * CPUTIMER_VARS objects are separate per side (each translation unit defines its
 * own, renamed), and each is zeroed before its side runs.
 *
 * ---- why the registers are read as .all, not through the bitfields ----
 *
 * .all is the whole 16-bit word. Reading TCR.bit.TSS would only observe the one
 * bit the baseline happens to touch; .all observes every bit, including the ones
 * a rewrite might start writing by accident. The bitfields are still exercised --
 * the module writes through them, and the resulting word is what is compared.
 *
 * ---- the float boundary ----
 *
 * ConfigCpuTimer does `temp = (long)(Freq * Period)` and stores the result in
 * PRD.all, and stores Freq / Period themselves into two floats. Both the
 * multiplication and the float->long conversion sit on values the caller
 * chooses, so the enumeration below walks the interesting edges explicitly:
 * zero, subnormal-ish, exact powers of two, values whose product crosses the
 * 32-bit boundary, and negative. Freq and Period are also compared as raw bits
 * (not with ==) so that 0.0 vs -0.0 counts as a difference.
 *
 * Extreme products overflow to inf, and `(long)inf` is undefined behaviour. Both
 * sides are the same source before the rewrite and both are compiled by the same
 * host compiler, so the comparison is still meaningful; what it is NOT is a claim
 * about what the C28x would do. That is stated rather than hidden.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DSP2803x_Device.h"

#define FAIL_LIMIT  20

extern void ref_InitCpuTimers(void);
extern void ref_ConfigCpuTimer(struct CPUTIMER_VARS *Timer, float Freq, float Period);
extern struct CPUTIMER_VARS ref_CpuTimer0;
extern struct CPUTIMER_VARS ref_CpuTimer1;
extern struct CPUTIMER_VARS ref_CpuTimer2;

extern void new_InitCpuTimers(void);
extern void new_ConfigCpuTimer(struct CPUTIMER_VARS *Timer, float Freq, float Period);
extern struct CPUTIMER_VARS new_CpuTimer0;
extern struct CPUTIMER_VARS new_CpuTimer1;
extern struct CPUTIMER_VARS new_CpuTimer2;

/* ---- plain snapshots ----------------------------------------------------- */

struct REGS_SNAP
{
    Uint32 tim;
    Uint32 prd;
    Uint16 tcr;
    Uint16 rsvd1;
    Uint16 tpr;
    Uint16 tprh;
};

struct VARS_SNAP
{
    uintptr_t addr;
    Uint32 ic;
    float  freq;
    float  period;
};

struct SIDE
{
    struct REGS_SNAP regs[3];
    struct VARS_SNAP vars[3];
};

static struct SIDE S[2];

static volatile struct CPUTIMER_REGS *regs_of(int i)
{
    if (i == 0) return &CpuTimer0Regs;
    if (i == 1) return &CpuTimer1Regs;
    return &CpuTimer2Regs;
}

static void reset_regs(void)
{
    int i;
    for (i = 0; i < 3; i++)
    {
        volatile struct CPUTIMER_REGS *r = regs_of(i);
        r->TIM.all  = 0;
        r->PRD.all  = 0;
        r->TCR.all  = 0;
        r->rsvd1    = 0;
        r->TPR.all  = 0;
        r->TPRH.all = 0;
    }
}

static void reset_vars(int side)
{
    struct CPUTIMER_VARS *v = (side == 0)
        ? &ref_CpuTimer0 : &new_CpuTimer0;
    struct CPUTIMER_VARS *w = (side == 0)
        ? &ref_CpuTimer1 : &new_CpuTimer1;
    struct CPUTIMER_VARS *x = (side == 0)
        ? &ref_CpuTimer2 : &new_CpuTimer2;

    memset(v, 0, sizeof *v);
    memset(w, 0, sizeof *w);
    memset(x, 0, sizeof *x);
}

/* Pre-fill the variables with non-zero junk before each side runs. Without this
 * a field the rewrite stops writing would compare equal by accident (both sides
 * would be sitting at the same zero). */
static void prefill_vars(int side)
{
    struct CPUTIMER_VARS *v[3];
    int i;

    v[0] = (side == 0) ? &ref_CpuTimer0 : &new_CpuTimer0;
    v[1] = (side == 0) ? &ref_CpuTimer1 : &new_CpuTimer1;
    v[2] = (side == 0) ? &ref_CpuTimer2 : &new_CpuTimer2;

    for (i = 0; i < 3; i++)
    {
        v[i]->RegsAddr       = (volatile struct CPUTIMER_REGS *)0;
        v[i]->InterruptCount = (Uint32)(0xA5A50000UL + i);
        v[i]->CPUFreqInMHz   = (float)(123.0 + i);
        v[i]->PeriodInUSec   = (float)(456.0 + i);
    }
}

static void snapshot(int side)
{
    int i;

    for (i = 0; i < 3; i++)
    {
        volatile struct CPUTIMER_REGS *r = regs_of(i);
        S[side].regs[i].tim   = r->TIM.all;
        S[side].regs[i].prd   = r->PRD.all;
        S[side].regs[i].tcr   = r->TCR.all;
        S[side].regs[i].rsvd1 = r->rsvd1;
        S[side].regs[i].tpr   = r->TPR.all;
        S[side].regs[i].tprh  = r->TPRH.all;
    }

    {
        const struct CPUTIMER_VARS *v[3];
        v[0] = (side == 0) ? &ref_CpuTimer0 : &new_CpuTimer0;
        v[1] = (side == 0) ? &ref_CpuTimer1 : &new_CpuTimer1;
        v[2] = (side == 0) ? &ref_CpuTimer2 : &new_CpuTimer2;

        for (i = 0; i < 3; i++)
        {
            S[side].vars[i].addr   = (uintptr_t)v[i]->RegsAddr;
            S[side].vars[i].ic     = v[i]->InterruptCount;
            S[side].vars[i].freq   = v[i]->CPUFreqInMHz;
            S[side].vars[i].period = v[i]->PeriodInUSec;
        }
    }
}

/* ---- case runner --------------------------------------------------------- */

static long g_cases = 0;
static int  g_fails = 0;

static float g_inFreq;
static float g_inPeriod;
static int   g_inDoInit;

static int bits_differ(float a, float b)
{
    return memcmp(&a, &b, sizeof a) != 0;
}

static void report(const char *fn, int i, const char *what)
{
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s[%d]: %s\n", fn, i, what);
    printf("  in: freq=%g period=%g init=%d\n",
           (double)g_inFreq, (double)g_inPeriod, g_inDoInit);
    printf("  regs ref: tim=%08lX prd=%08lX tcr=%04X rsvd1=%04X tpr=%04X tprh=%04X\n",
           (unsigned long)S[0].regs[i].tim, (unsigned long)S[0].regs[i].prd,
           S[0].regs[i].tcr, S[0].regs[i].rsvd1,
           S[0].regs[i].tpr, S[0].regs[i].tprh);
    printf("  regs new: tim=%08lX prd=%08lX tcr=%04X rsvd1=%04X tpr=%04X tprh=%04X\n",
           (unsigned long)S[1].regs[i].tim, (unsigned long)S[1].regs[i].prd,
           S[1].regs[i].tcr, S[1].regs[i].rsvd1,
           S[1].regs[i].tpr, S[1].regs[i].tprh);
    printf("  vars ref: addr=%llX ic=%lu freq=%g period=%g\n",
           (unsigned long long)S[0].vars[i].addr, (unsigned long)S[0].vars[i].ic,
           (double)S[0].vars[i].freq, (double)S[0].vars[i].period);
    printf("  vars new: addr=%llX ic=%lu freq=%g period=%g\n",
           (unsigned long long)S[1].vars[i].addr, (unsigned long)S[1].vars[i].ic,
           (double)S[1].vars[i].freq, (double)S[1].vars[i].period);
}

static void compare(void)
{
    int i;

    for (i = 0; i < 3; i++)
    {
        if (S[0].regs[i].tim   != S[1].regs[i].tim)   report("regs", i, "TIM.all");
        if (S[0].regs[i].prd   != S[1].regs[i].prd)   report("regs", i, "PRD.all");
        if (S[0].regs[i].tcr   != S[1].regs[i].tcr)   report("regs", i, "TCR.all");
        if (S[0].regs[i].rsvd1 != S[1].regs[i].rsvd1) report("regs", i, "rsvd1");
        if (S[0].regs[i].tpr   != S[1].regs[i].tpr)   report("regs", i, "TPR.all");
        if (S[0].regs[i].tprh  != S[1].regs[i].tprh)  report("regs", i, "TPRH.all");

        if (S[0].vars[i].addr != S[1].vars[i].addr)   report("vars", i, "RegsAddr");
        if (S[0].vars[i].ic   != S[1].vars[i].ic)     report("vars", i, "InterruptCount");
        if (bits_differ(S[0].vars[i].freq,   S[1].vars[i].freq))
            report("vars", i, "CPUFreqInMHz");
        if (bits_differ(S[0].vars[i].period, S[1].vars[i].period))
            report("vars", i, "PeriodInUSec");
    }
}

/* One case: run both sides on the same inputs, compare all six objects. */
static void run_case(void)
{
    int i;

    reset_regs();
    reset_vars(0);
    prefill_vars(0);
    if (g_inDoInit)
    {
        ref_InitCpuTimers();
    }
    if (g_inDoInit)
    {
        for (i = 0; i < 3; i++)
        {
            ref_ConfigCpuTimer(i == 0 ? &ref_CpuTimer0
                             : i == 1 ? &ref_CpuTimer1 : &ref_CpuTimer2,
                               g_inFreq, g_inPeriod);
        }
    }
    snapshot(0);

    reset_regs();
    reset_vars(1);
    prefill_vars(1);
    if (g_inDoInit)
    {
        new_InitCpuTimers();
    }
    if (g_inDoInit)
    {
        for (i = 0; i < 3; i++)
        {
            new_ConfigCpuTimer(i == 0 ? &new_CpuTimer0
                             : i == 1 ? &new_CpuTimer1 : &new_CpuTimer2,
                               g_inFreq, g_inPeriod);
        }
    }
    snapshot(1);

    g_cases++;
    compare();
}

/* ---- phases -------------------------------------------------------------- */

/* InitCpuTimers on its own: no ConfigCpuTimer call at all. This is the phase
 * that pins down the initial register state, including the two bits that are
 * easy to drop (TSS and TRB on all three timers) and the PRD/TPR/TPRH values. */
static void phase_init(void)
{
    int k;

    g_inDoInit = 1;

    /* Only InitCpuTimers: run it against a dirty register file, so "wrote 0" and
     * "did not write" are distinguishable. */
    for (k = 0; k < 8; k++)
    {
        int i;
        reset_regs();
        for (i = 0; i < 3; i++)
        {
            volatile struct CPUTIMER_REGS *r = regs_of(i);
            r->TIM.all  = (Uint32)(0x11110000UL + k * 0x101UL + i);
            r->PRD.all  = (Uint32)(0x22220000UL + k * 0x101UL + i);
            r->TCR.all  = (Uint16)(0x3333 + k + i);
            r->rsvd1    = (Uint16)(0x4444 + k + i);
            r->TPR.all  = (Uint16)(0x5555 + k + i);
            r->TPRH.all = (Uint16)(0x6666 + k + i);
        }
        reset_vars(0);
        prefill_vars(0);
        ref_InitCpuTimers();
        snapshot(0);

        reset_regs();
        for (i = 0; i < 3; i++)
        {
            volatile struct CPUTIMER_REGS *r = regs_of(i);
            r->TIM.all  = (Uint32)(0x11110000UL + k * 0x101UL + i);
            r->PRD.all  = (Uint32)(0x22220000UL + k * 0x101UL + i);
            r->TCR.all  = (Uint16)(0x3333 + k + i);
            r->rsvd1    = (Uint16)(0x4444 + k + i);
            r->TPR.all  = (Uint16)(0x5555 + k + i);
            r->TPRH.all = (Uint16)(0x6666 + k + i);
        }
        reset_vars(1);
        prefill_vars(1);
        new_InitCpuTimers();
        snapshot(1);

        g_cases++;
        compare();
    }
}

/* ConfigCpuTimer over an explicit edge table. Freq and Period are the two
 * caller-controlled inputs; the product is what ends up in PRD. */
static void phase_config_table(void)
{
    static const float freq[] = {
        0.0f, 1.0f, 60.0f, 0.5f, 2.0f, 100.0f, 1000.0f,
        -1.0f, -60.0f, 1e-6f, 1e6f, 1e9f, 3.0e9f, 4.0e9f,
        3.402823466e38f, -3.402823466e38f, 1.0e-30f, 65536.0f, 32768.0f
    };
    static const float period[] = {
        0.0f, 1.0f, 10.0f, 1000.0f, 1000000.0f, 1e9f, 0.5f, 2.5f,
        -1.0f, -1000.0f, 1e-6f, 3.402823466e38f, 65535.0f, 65536.0f,
        4.294967296e9f, 1.0e-30f
    };
    unsigned a, b;

    g_inDoInit = 1;

    for (a = 0; a < sizeof freq / sizeof freq[0]; a++)
    {
        for (b = 0; b < sizeof period / sizeof period[0]; b++)
        {
            g_inFreq = freq[a];
            g_inPeriod = period[b];
            run_case();
        }
    }
}

/* ---- random sweep -------------------------------------------------------- */

static unsigned long g_rng = 0x20260924UL;

static unsigned long rnd(void)
{
    g_rng = g_rng * 1103515245UL + 12345UL;
    return (g_rng >> 8) & 0xFFFFFFUL;
}

static float rnd_float(void)
{
    unsigned long r = rnd();
    switch (r % 5)
    {
    case 0:  return (float)(r % 100000) / 1000.0f;
    case 1:  return (float)(r % 1000) * 1000.0f;
    case 2:  return (float)((long)(r % 2000) - 1000);
    case 3:  return (float)(r % 65536);
    default: return (float)(r % 100) / 8.0f;
    }
}

static void phase_random(long n)
{
    long k;

    g_inDoInit = 1;

    for (k = 0; k < n; k++)
    {
        g_inFreq = rnd_float();
        g_inPeriod = rnd_float();
        run_case();
    }
}

/* ---- main ---------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 20000;
    long base;

    reset_regs();
    reset_vars(0);
    reset_vars(1);

    phase_init();
    printf("  InitCpuTimers                : %ld\n", g_cases);

    base = g_cases;
    phase_config_table();
    printf("  ConfigCpuTimer (edge table)  : %ld\n", g_cases - base);

    base = g_cases;
    phase_random(n);
    printf("  ConfigCpuTimer (random)      : %ld\n", g_cases - base);

    printf("DSP2803x_CpuTimers difftest: cases=%ld mismatches=%d\n",
           g_cases, g_fails);
    return g_fails ? 1 : 0;
}
