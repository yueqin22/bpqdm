/* Driver for the main case.
 *
 * main.c has four entry points: main() and the three ISRs.  Two things make it
 * different from a plain struct-in/struct-out module:
 *
 *   1. main() is `while(1)` -- it never returns on the target either.  The
 *      termination oracle is the mock's call budget: each GetTime() call
 *      increments a counter, and once it exceeds the driver-set budget the
 *      mock longjmps out (see mock_main.c).  The budget counts GetTime()
 *      calls; with CPU_TIME_DEBUG undefined there is exactly one GetTime()
 *      call per loop iteration, so budget == iterations.
 *
 *   2. Everything the module touches outside its own frame is a mock: the
 *      task functions, the CPU-control macros (EALLOW/EINT/...) and the
 *      peripheral registers.  All of them append to one ordered event log,
 *      and the driver compares the *sequences* plus the final register and
 *      gCpuTime state -- not just final values.
 *
 * Clock model (the single most important part of this bench):
 *   on the target TIM is a DOWN counter and every time difference in the
 *   project is written base-minus-now, so the loop's elapsed time grows as
 *   the clock falls.  gHostGetTime() decrements a virtual clock by a
 *   driver-set step on every call, which makes `m_DetaTime` grow by `step`
 *   per iteration.  Sweeping `step` over the exact threshold (C_TIME_05MS =
 *   30000) and its divisors/multiples is what makes the `>=` gate, the
 *   `m_BaseTime -= C_TIME_05MS` resync, the m_LoopFlag&3 phase splitter and
 *   the Det05msClk saturation all observable:
 *     step = 0      -> gate never fires, only the 0 ms tasks repeat
 *     step = 15000  -> fires every 2nd iteration
 *     step = 30000  -> fires every iteration (the >= vs > edge)
 *     step = 90000  -> m_DetaTime passes 65535 -> Det05msClk saturates
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "MotorInclude.h"
#include "mock_main.h"

/* renamed per-side entry points (see ref_tu.c / new_tu.c) */
extern void ref_main(void);
extern void ref_ADC_Over_isr(void);
extern void ref_EPWM1_zero_isr(void);
extern void ref_EPWM1_TZ_isr(void);
extern void new_main(void);
extern void new_ADC_Over_isr(void);
extern void new_EPWM1_zero_isr(void);
extern void new_EPWM1_TZ_isr(void);

static long g_cases = 0;
static long g_fail  = 0;

static unsigned long long g_rng = 0x9E3779B97F4A7C15ULL;
static unsigned long rnd32(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return (unsigned long)(g_rng >> 16);
}

struct SNAP {
    CPU_TIME_STRUCT t;
    struct ADC_REGS adc;
    struct EPWM_REGS epwm;
    struct PIE_CTRL_REGS pie;
    struct GPIO_DATA_REGS gpio;
    Uint16 ctl[7];
    Uint16 log[HOST_LOG_MAX];
    Uint   log_len;
    Uint16 trace[HOST_TRACE_MAX][HOST_TRACE_WORDS];
    Uint   trace_len;
    Uint32 cnt[HOST_CNT_MAX];
    Uint32 time;
    Uint32 calls;
    int    aborted;
};

static struct SNAP g_r;
static struct SNAP g_n;

static void snapshot(struct SNAP *s)
{
    memcpy(&s->t, &gCpuTime, sizeof s->t);
    memcpy(&s->adc, &AdcRegs, sizeof s->adc);
    memcpy(&s->epwm, &EPwm1Regs, sizeof s->epwm);
    memcpy(&s->pie, &PieCtrlRegs, sizeof s->pie);
    memcpy(&s->gpio, &GpioDataRegs, sizeof s->gpio);
    memcpy((void *)s->ctl, (const void *)gHostCpuCtl, sizeof s->ctl);
    memcpy(s->log, gHostLog, sizeof s->log);
    s->log_len = gHostLogLen;
    memcpy(s->trace, gHostRegTrace, sizeof s->trace);
    s->trace_len = gHostTraceLen;
    memcpy(s->cnt, gHostCnt, sizeof s->cnt);
    s->time = gHostTime;
    s->calls = gHostTimeCalls;
}

static const char *ctl_name(int i)
{
    static const char *n[7] = { "EINT", "DINT", "ERTM", "DRTM", "EALLOW", "EDIS", "-" };
    return (i >= 0 && i < 7) ? n[i] : "?";
}

static void log_dump(const char *tag, const struct SNAP *s)
{
    Uint i;
    printf("  %s len=%u calls=%u time=0x%lx det05=%u tmp0=%u\n", tag,
           (unsigned)s->log_len, (unsigned)s->calls, (unsigned long)s->time,
           (unsigned)s->t.Det05msClk, (unsigned)s->t.tmp0Ms);
    printf("    log:");
    for (i = 0; i < s->log_len && i < 48; i++)
    {
        printf(" %u", (unsigned)s->log[i]);
    }
    if (s->log_len > 48)
    {
        printf(" ...(+%u)", (unsigned)(s->log_len - 48));
    }
    printf("\n");
}

static void diff_snap(const char *tag, const struct SNAP *r, const struct SNAP *n)
{
    Uint i;
    int shown = 0;

    g_fail++;
    if (g_fail > 20)
    {
        return;
    }
    printf("MISMATCH %s\n", tag);
    log_dump("ref", r);
    log_dump("new", n);

    if (r->aborted != n->aborted)
    {
        printf("  aborted: ref=%d new=%d\n", r->aborted, n->aborted);
        shown = 1;
    }
    if (r->calls != n->calls)
    {
        printf("  GetTime calls: ref=%u new=%u\n",
               (unsigned)r->calls, (unsigned)n->calls);
        shown = 1;
    }
    if (r->time != n->time)
    {
        printf("  final clock: ref=0x%lx new=0x%lx\n",
               (unsigned long)r->time, (unsigned long)n->time);
        shown = 1;
    }
    if (memcmp(&r->t, &n->t, sizeof r->t) != 0)
    {
        const Ulong *a = (const Ulong *)&r->t;
        const Ulong *b = (const Ulong *)&n->t;
        for (i = 0; i < sizeof(CPU_TIME_STRUCT) / sizeof(Ulong); i++)
        {
            if (a[i] != b[i])
            {
                printf("  gCpuTime word[%u]: ref=%lu new=%lu\n",
                       (unsigned)i, (unsigned long)a[i], (unsigned long)b[i]);
                shown = 1;
            }
        }
    }
    if (memcmp(&r->adc, &n->adc, sizeof r->adc) != 0)
    {
        printf("  AdcRegs: ref intflgclr=0x%lx socprictl=0x%lx  new intflgclr=0x%lx socprictl=0x%lx\n",
               (unsigned long)r->adc.ADCINTFLGCLR.all, (unsigned long)r->adc.SOCPRICTL.all,
               (unsigned long)n->adc.ADCINTFLGCLR.all, (unsigned long)n->adc.SOCPRICTL.all);
        shown = 1;
    }
    if (memcmp(&r->epwm, &n->epwm, sizeof r->epwm) != 0)
    {
        printf("  EPwm1Regs.ETCLR: ref=0x%lx new=0x%lx\n",
               (unsigned long)r->epwm.ETCLR.all, (unsigned long)n->epwm.ETCLR.all);
        shown = 1;
    }
    if (memcmp(&r->pie, &n->pie, sizeof r->pie) != 0)
    {
        printf("  PieCtrlRegs.PIEACK: ref=0x%lx new=0x%lx\n",
               (unsigned long)r->pie.PIEACK.all, (unsigned long)n->pie.PIEACK.all);
        shown = 1;
    }
    if (memcmp(&r->gpio, &n->gpio, sizeof r->gpio) != 0)
    {
        printf("  GpioDataRegs.GPASET: ref=0x%lx new=0x%lx\n",
               (unsigned long)r->gpio.GPASET.all, (unsigned long)n->gpio.GPASET.all);
        shown = 1;
    }
    for (i = 0; i < 7; i++)
    {
        if (r->ctl[i] != n->ctl[i])
        {
            printf("  ctl %s: ref=%u new=%u\n", ctl_name((int)i),
                   (unsigned)r->ctl[i], (unsigned)n->ctl[i]);
            shown = 1;
        }
    }
    for (i = 0; i < HOST_CNT_MAX; i++)
    {
        if (r->cnt[i] != n->cnt[i])
        {
            printf("  event cnt[%u]: ref=%u new=%u\n", (unsigned)i,
                   (unsigned)r->cnt[i], (unsigned)n->cnt[i]);
            shown = 1;
        }
    }
    if (r->log_len != n->log_len)
    {
        printf("  log length: ref=%u new=%u\n",
               (unsigned)r->log_len, (unsigned)n->log_len);
        shown = 1;
    }
    for (i = 0; i < r->log_len && i < n->log_len && i < 64; i++)
    {
        if (r->log[i] != n->log[i])
        {
            printf("  log[%u]: ref=%u new=%u\n", (unsigned)i,
                   (unsigned)r->log[i], (unsigned)n->log[i]);
            shown = 1;
        }
    }
    if (r->trace_len != n->trace_len)
    {
        printf("  trace length: ref=%u new=%u\n",
               (unsigned)r->trace_len, (unsigned)n->trace_len);
        shown = 1;
    }
    for (i = 0; i < r->trace_len && i < n->trace_len && i < 64; i++)
    {
        if (memcmp(r->trace[i], n->trace[i], sizeof r->trace[i]) != 0)
        {
            printf("  reg trace[%u]: ref=%u/%u/%u/%u/%u  new=%u/%u/%u/%u/%u\n",
                   (unsigned)i,
                   (unsigned)r->trace[i][0], (unsigned)r->trace[i][1],
                   (unsigned)r->trace[i][2], (unsigned)r->trace[i][3],
                   (unsigned)r->trace[i][4],
                   (unsigned)n->trace[i][0], (unsigned)n->trace[i][1],
                   (unsigned)n->trace[i][2], (unsigned)n->trace[i][3],
                   (unsigned)n->trace[i][4]);
            shown = 1;
        }
    }
    if (!shown)
    {
        printf("  (no scalar difference reported)\n");
    }
}

/* ---------------- one main() run, both sides ---------------- */

static void run_main(const char *tag, Uint32 start, Uint32 step, Uint32 budget)
{
    g_cases++;
    gMockAbortArmed = 1;
    if (setjmp(gMockAbort) == 0)
    {
        gHostResetState();
        gHostTime = start;
        gHostTimeStep = step;
        gHostBudget = budget;
        ref_main();
        snapshot(&g_r);
        g_r.aborted = 0;
    }
    else
    {
        snapshot(&g_r);
        g_r.aborted = 1;
    }
    if (setjmp(gMockAbort) == 0)
    {
        gHostResetState();
        gHostTime = start;
        gHostTimeStep = step;
        gHostBudget = budget;
        new_main();
        snapshot(&g_n);
        g_n.aborted = 0;
    }
    else
    {
        snapshot(&g_n);
        g_n.aborted = 1;
    }
    gMockAbortArmed = 0;

    if (memcmp(&g_r.t, &g_n.t, sizeof g_r.t) != 0 ||
        memcmp(&g_r.adc, &g_n.adc, sizeof g_r.adc) != 0 ||
        memcmp(&g_r.epwm, &g_n.epwm, sizeof g_r.epwm) != 0 ||
        memcmp(&g_r.pie, &g_n.pie, sizeof g_r.pie) != 0 ||
        memcmp(&g_r.gpio, &g_n.gpio, sizeof g_r.gpio) != 0 ||
        memcmp(g_r.ctl, g_n.ctl, sizeof g_r.ctl) != 0 ||
        g_r.log_len != g_n.log_len ||
        memcmp(g_r.log, g_n.log, sizeof(Uint16) * (g_r.log_len < g_n.log_len ? g_r.log_len : g_n.log_len)) != 0 ||
        g_r.trace_len != g_n.trace_len ||
        memcmp(g_r.trace, g_n.trace, sizeof(Uint16) * HOST_TRACE_WORDS * (g_r.trace_len < g_n.trace_len ? g_r.trace_len : g_n.trace_len)) != 0 ||
        memcmp(g_r.cnt, g_n.cnt, sizeof g_r.cnt) != 0 ||
        g_r.time != g_n.time || g_r.calls != g_n.calls ||
        g_r.aborted != g_n.aborted)
    {
        char buf[160];
    sprintf(buf, "main start=0x%lx step=%lu budget=%lu",
                (unsigned long)start, (unsigned long)step, (unsigned long)budget);
        diff_snap(buf, &g_r, &g_n);
    }
}

/* ---------------- one ISR run, both sides ---------------- */

typedef void (*isr_fn)(void);

static void run_isr(const char *name, isr_fn rf, isr_fn nf,
                    Uint32 start, Uint32 step)
{
    char buf[160];
    g_cases++;
    gMockAbortArmed = 0;
    gHostBudget = 0;
    gHostResetState();
    gHostTime = start;
    gHostTimeStep = step;
    rf();
    snapshot(&g_r);
    g_r.aborted = 0;

    gHostResetState();
    gHostTime = start;
    gHostTimeStep = step;
    nf();
    snapshot(&g_n);
    g_n.aborted = 0;

    sprintf(buf, "%s start=0x%lx step=%lu", name,
            (unsigned long)start, (unsigned long)step);
    if (memcmp(&g_r.t, &g_n.t, sizeof g_r.t) != 0 ||
        memcmp(&g_r.adc, &g_n.adc, sizeof g_r.adc) != 0 ||
        memcmp(&g_r.epwm, &g_n.epwm, sizeof g_r.epwm) != 0 ||
        memcmp(&g_r.pie, &g_n.pie, sizeof g_r.pie) != 0 ||
        memcmp(&g_r.gpio, &g_n.gpio, sizeof g_r.gpio) != 0 ||
        memcmp(g_r.ctl, g_n.ctl, sizeof g_r.ctl) != 0 ||
        g_r.log_len != g_n.log_len ||
        memcmp(g_r.log, g_n.log, sizeof(Uint16) * (g_r.log_len < g_n.log_len ? g_r.log_len : g_n.log_len)) != 0 ||
        g_r.trace_len != g_n.trace_len ||
        memcmp(g_r.trace, g_n.trace, sizeof(Uint16) * HOST_TRACE_WORDS * (g_r.trace_len < g_n.trace_len ? g_r.trace_len : g_n.trace_len)) != 0 ||
        memcmp(g_r.cnt, g_n.cnt, sizeof g_r.cnt) != 0 ||
        g_r.time != g_n.time || g_r.calls != g_n.calls)
    {
        diff_snap(buf, &g_r, &g_n);
    }
}

static void sweep_isr(void)
{
    static const Uint32 steps[] = { 0, 1, 30000, 65535, 0xFFFFFFFFU };
    Uint i;
    for (i = 0; i < sizeof steps / sizeof steps[0]; i++)
    {
        run_isr("ADC_Over_isr", ref_ADC_Over_isr, new_ADC_Over_isr,
                0x00010000U, steps[i]);
        run_isr("EPWM1_zero_isr", ref_EPWM1_zero_isr, new_EPWM1_zero_isr,
                0x00020000U, steps[i]);
        run_isr("EPWM1_TZ_isr", ref_EPWM1_TZ_isr, new_EPWM1_TZ_isr,
                0x00030000U, steps[i]);
    }
    /* a couple of wrapped clocks: on the target TIM is 32-bit down-counting,
     * so base-minus-now wrapping is reachable; both sides must wrap alike. */
    run_isr("ADC_Over_isr", ref_ADC_Over_isr, new_ADC_Over_isr, 0x00000000U, 0xFFFFFFFFU);
    run_isr("EPWM1_zero_isr", ref_EPWM1_zero_isr, new_EPWM1_zero_isr, 0x00000000U, 0x80000000U);
}

static void sweep_main(void)
{
    static const Uint32 steps[] = {
        0, 1, 15000, 10000, 6000, 5000, 3000, 1000, 100,
        29999, 30000, 30001, 60000, 65535, 90000, 120000, 200000,
        0x7FFFFFFFU, 0x80000000U, 0xFFFFFFFFU
    };
    static const Uint32 budgets[] = { 2, 4, 8, 17, 40 };
    Uint i, j;
    for (i = 0; i < sizeof steps / sizeof steps[0]; i++)
    {
        for (j = 0; j < sizeof budgets / sizeof budgets[0]; j++)
        {
            run_main("main", 0x00012345U, steps[i], budgets[j]);
        }
    }
    /* clock starting at 0: every decrement wraps, exercising the unsigned
     * wrap of m_DetaTime and of __IQsat's comparison on both sides alike. */
    run_main("main-wrap", 0x00000000U, 30000, 40);
    run_main("main-wrap", 0x00000000U, 90000, 40);
    run_main("main-wrap", 0x80000000U, 30000, 40);
}

static void random_cases(long n)
{
    long k;
    for (k = 0; k < n; k++)
    {
        Uint32 step;
        Uint32 budget;
        Uint32 start;
        switch (rnd32() % 8)
        {
        case 0:  step = 0; break;
        case 1:  step = 30000; break;
        case 2:  step = 30000 + (rnd32() % 3) - 1; break;
        case 3:  step = 30000 / (1 + (rnd32() % 30)); break;
        case 4:  step = 30000 * (1 + (rnd32() % 8)); break;
        case 5:  step = 65535; break;
        case 6:  step = rnd32(); break;
        default: step = rnd32() & 0xFFFFU; break;
        }
        budget = 4 + (rnd32() % 61);
        start = (rnd32() & 1) ? rnd32() : (rnd32() & 0xFFFFU);
        run_main("main-rand", start, step, budget);
    }
}

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 20000L;

    gMockAbortArmed = 0;
    gHostBudget = 0;

    sweep_isr();
    sweep_main();
    random_cases(n);

    printf("main difftest: cases=%ld mismatches=%ld\n", g_cases, g_fail);
    return g_fail ? 1 : 0;
}
