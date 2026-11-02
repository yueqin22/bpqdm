/* Differential test driver for mainInit_2803x.
 *
 * The module is the board's bring-up sequence: 16 entry points that write
 * constants into peripheral registers, install 128 interrupt vectors, and call
 * five other modules. There is almost no data flow, so the bench has to make
 * every WRITE and every CALL observable:
 *
 *   registers   SysCtrlRegs / FlashRegs / AdcRegs / PieCtrlRegs / GpioCtrlRegs /
 *               GpioDataRegs, compared word by word, plus IER and IFR.
 *   vectors     PieVectTable, 128 slots, compared after canonicalisation (see
 *               below -- the module's own rsvd_ISR has a different address on
 *               each side).
 *   control     gHostCpuCtl[7] (EALLOW/EDIS/DINT/... totals) and the event log
 *               (id + register fingerprint per event), which pins each register
 *               write down to the CPU-control instruction it sits between.
 *   calls       MovePrgFrFlashToRam's three arguments, and the call counts of
 *               InitSetPWM / InitCpuTimers / StartCpuTimer1 / Device_cal.
 *   module data gErrorIntCnt, incremented by rsvd_ISR.
 *
 * ---- why the inputs are driven, not just the call ----
 *
 * InitPll's behaviour depends on what it READS: PLLSTS.MCLKSTS selects the whole
 * outer block, PLLSTS.DIVSEL selects the inner reset, PLLSTS.PLLLOCKS guards the
 * spin loop. Those bits are therefore inputs, swept over their whole range
 * (MCLKSTS 0/1, DIVSEL 0..3, PLLLOCKS pinned to 1 -- see the note below).
 *
 * copy_prg reads the copy table the driver fills, so src/dst/size are inputs:
 * `size = (Uint)crp->size - 1` truncates to 16 bits and wraps at zero, which is
 * only observable if the table carries values above 0xFFFF and zero itself.
 *
 * InitPll's `val` is DSP_CLOCK/10 in the real call chain, but the driver calls
 * InitPll directly with the full 0..65535 range: PLLCR.DIV is a 4-bit field, so
 * the write truncates and the truncation is behaviour.
 *
 * ---- the PLLLOCKS pin ----
 *
 * The `while(SysCtrlRegs.PLLSTS.bit.PLLLOCKS != 1) {}` spin has an EMPTY body:
 * with PLLLOCKS pinned to 0 the baseline itself never returns, so that state is
 * not a usable input. Pinned to 1 the loop is entered zero times and the only
 * remaining observable is termination. Both are recorded as a coverage gap in
 * the module's registration entry rather than papered over: a mutation of that
 * condition would show up as "runs forever", which this bench cannot turn into
 * field-level evidence. Every other statement of InitPll is driven.
 *
 * PIE vector canonicalisation: 123 slots get rsvd_ISR, which the module defines,
 * so each translation unit renames it and the two sides hold DIFFERENT
 * addresses. Each slot is therefore reduced to "which shared stub is here"
 * (1..5) or "the module's own handler / anything else" (9); slots in class 9
 * are additionally compared raw, so a slot the rewrite fails to touch still
 * shows up.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DSP2803x_Device.h"
#include "mock_mainInit.h"

#define FAIL_LIMIT  20

/* ---- the module's own entry points, one pair per side -------------------- */

extern void ref_InitSysCtrl(void);
extern void ref_InitInterrupt(void);
extern void ref_SetInterruptEnable(void);
extern void ref_InitPeripherals(void);
extern void ref_copy_prg(COPY_TABLE *tp);
extern void ref_InitFlash(void);
extern void ref_KickDog(void);
extern void ref_DisableDog(void);
extern void ref_EnableDog(void);
extern void ref_InitPll(Uint16 val);
extern void ref_InitPeripheralClocks(void);
extern void ref_InitPieCtrl(void);
extern void ref_InitPieVectTable(void);
extern void ref_InitSetGpio(void);
extern void ref_InitSetAdc(void);
extern void ref_rsvd_ISR(void);
extern int  ref_gErrorIntCnt;

extern void new_InitSysCtrl(void);
extern void new_InitInterrupt(void);
extern void new_SetInterruptEnable(void);
extern void new_InitPeripherals(void);
extern void new_copy_prg(COPY_TABLE *tp);
extern void new_InitFlash(void);
extern void new_KickDog(void);
extern void new_DisableDog(void);
extern void new_EnableDog(void);
extern void new_InitPll(Uint16 val);
extern void new_InitPeripheralClocks(void);
extern void new_InitPieCtrl(void);
extern void new_InitPieVectTable(void);
extern void new_InitSetGpio(void);
extern void new_InitSetAdc(void);
extern void new_rsvd_ISR(void);
extern int  new_gErrorIntCnt;

/* ---- snapshots ----------------------------------------------------------- */

#define REGBUF  256

struct SNAP
{
    unsigned char sys[REGBUF];
    unsigned char flash[REGBUF];
    unsigned char adc[REGBUF];
    unsigned char pie[REGBUF];
    unsigned char gpioctl[REGBUF];
    unsigned char gpiodat[REGBUF];
    unsigned char tmr1[REGBUF];
    Uint16 ier;
    Uint16 ifr;
    Uint16 ctl[7];
    int     logN;
    int     logId[HOST_LOG_MAX];
    unsigned long logFp[HOST_LOG_MAX];
    int     moveN;
    unsigned long moveSrc[HOST_MOVE_MAX];
    unsigned long moveDst[HOST_MOVE_MAX];
    unsigned long moveSize[HOST_MOVE_MAX];
    int     devCalN;
    int     errCnt;
    unsigned char vectCls[128];
    uintptr_t     vectRaw[128];
};

static struct SNAP S[2];
static long g_cases;
static int  g_fails;
static const char *g_what;
static int  g_inMclksts;
static int  g_inDivsel;

/* ---- deterministic per-case prefill ------------------------------------- */

static unsigned long g_seed;

static unsigned long seedrnd(void)
{
    g_seed ^= g_seed << 13;
    g_seed ^= g_seed >> 7;
    g_seed ^= g_seed << 17;
    return g_seed;
}

/* Four register-file variants, because a single fixed pattern hides half the
 * possible mutations:
 *
 *   variant 0  every word 0xFFFF   -> a statement that writes 0, or stops
 *                                    writing, changes the word;
 *   variant 1  every word 0x0000   -> a statement that writes 1 changes the
 *                                    word (with variant 0 it would not!);
 *   variant 2  0xAAAA / 0x5555 alternating -> mixed edges, and it keeps every
 *                                    half-byte non-zero so a wrong mask shows;
 *   variant 3  seeded random        -> the long tail.
 *
 * The first version of this bench used ONE pattern with every low half-byte set
 * to 0x0F0F, and four mutations missed exactly because of it: writing 1 to a bit
 * that the pattern already had at 1 is invisible (MI68: AdcRegs.ADCINTFLGCLR),
 * and `IER |= mask` cannot show up when the masked bits are already 1 (MI10). */
static unsigned g_patVariant;

static void fill_words(void *base, unsigned nbytes, unsigned short pat)
{
    volatile unsigned short *p = (volatile unsigned short *)base;
    unsigned n = nbytes / 2;
    unsigned i;
    for (i = 0; i < n; i++)
    {
        switch (g_patVariant)
        {
        case 0:  p[i] = 0xFFFF; break;
        case 1:  p[i] = 0x0000; break;
        case 2:  p[i] = (i & 1) ? 0xAAAA : 0x5555; break;
        default: p[i] = (unsigned short)(pat + i * 0x1111u); break;
        }
    }
}

/* PLLLOCKS is pinned to 1 (see the header comment); MCLKSTS / DIVSEL are the
 * two selection inputs and are set after the junk fill so the fill cannot
 * accidentally move them. */
static void prefill_pll(void)
{
    SysCtrlRegs.PLLSTS.bit.PLLLOCKS = 1;
    SysCtrlRegs.PLLSTS.bit.MCLKSTS  = (Uint16)g_inMclksts;
    SysCtrlRegs.PLLSTS.bit.DIVSEL   = (Uint16)g_inDivsel;
}

/* Junk-fill everything the module can possibly touch, so a field the rewrite
 * stops writing is immediately visible instead of "both sides still zero". */
static void prefill_regs(void)
{
    fill_words((void *)&SysCtrlRegs,  sizeof(struct SYS_CTRL_REGS),  0x1000);
    fill_words((void *)&FlashRegs,    sizeof(struct FLASH_REGS),     0x2000);
    fill_words((void *)&AdcRegs,      sizeof(struct ADC_REGS),       0x3000);
    fill_words((void *)&PieCtrlRegs,  sizeof(struct PIE_CTRL_REGS),  0x4000);
    fill_words((void *)&GpioCtrlRegs, sizeof(struct GPIO_CTRL_REGS), 0x5000);
    fill_words((void *)&GpioDataRegs, sizeof(struct GPIO_DATA_REGS), 0x6000);
    /* StartCpuTimer1() is a MACRO in DSP2803x_CpuTimers.h (it writes
     * CpuTimer1Regs.TCR.bit.TSS), so this block is a write target too. */
    fill_words((void *)&CpuTimer1Regs, sizeof(struct CPUTIMER_REGS), 0x7000);
    /* IER/IFR: the low five bits are M_INT1..M_INT5, and SetInterruptEnable does
     * `IER |= (M_INT1|M_INT2|M_INT3|M_INT5)`. If the prefill already has those
     * bits at 1 the OR changes nothing and the mutation is invisible, so the
     * prefill keeps them at 0 while still being non-trivial elsewhere. */
    gHostIER = (Uint16)0x5A00;
    gHostIFR = (Uint16)0xA500;
    {
        unsigned i;
        for (i = 0; i < 128; i++)
        {
            *(uintptr_t *)((char *)&PieVectTable + i * sizeof(void *)) =
                (uintptr_t)(0xDEAD0000UL + g_patVariant * 0x10000UL + i);
        }
    }
}

static unsigned long g_tblSize;

static void prefill_table(COPY_TABLE *t, unsigned long seed)
{
    t->rec_size      = (Uint16)(0x0100 + (seed & 0x00FF));
    t->num_recs      = (Uint16)(0x0200 + (seed & 0x00FF));
    t->recs[0].src_pgid = (Uint16)(0x0300 + (seed & 0x00FF));
    t->recs[0].dst_pgid = (Uint16)(0x0400 + (seed & 0x00FF));
    t->recs[0].src_addr = 0x3F0000UL + seed;
    t->recs[0].dst_addr = 0x008000UL + seed * 3UL;
    t->recs[0].size     = g_tblSize;
}

/* ---- snapshot ------------------------------------------------------------ */

static void snap_regs(unsigned char *dst, const void *src, unsigned nbytes)
{
    memset(dst, 0, REGBUF);
    if (nbytes > REGBUF)
    {
        nbytes = REGBUF;
    }
    memcpy(dst, src, nbytes);
}

static void snapshot(int side)
{
    struct SNAP *s = &S[side];
    int i;

    snap_regs(s->sys,     (const void *)&SysCtrlRegs,  sizeof(struct SYS_CTRL_REGS));
    snap_regs(s->flash,   (const void *)&FlashRegs,    sizeof(struct FLASH_REGS));
    snap_regs(s->adc,     (const void *)&AdcRegs,      sizeof(struct ADC_REGS));
    snap_regs(s->pie,     (const void *)&PieCtrlRegs,  sizeof(struct PIE_CTRL_REGS));
    snap_regs(s->gpioctl, (const void *)&GpioCtrlRegs, sizeof(struct GPIO_CTRL_REGS));
    snap_regs(s->gpiodat, (const void *)&GpioDataRegs, sizeof(struct GPIO_DATA_REGS));
    snap_regs(s->tmr1,    (const void *)&CpuTimer1Regs, sizeof(struct CPUTIMER_REGS));
    s->ier = gHostIER;
    s->ifr = gHostIFR;

    for (i = 0; i < 7; i++)
    {
        s->ctl[i] = gHostCpuCtl[i];
    }

    s->logN = gHostLogN;
    if (s->logN > HOST_LOG_MAX)
    {
        s->logN = HOST_LOG_MAX;
    }
    for (i = 0; i < s->logN; i++)
    {
        s->logId[i] = gHostLogId[i];
        s->logFp[i] = gHostLogFp[i];
    }

    s->moveN = gHostMoveN;
    for (i = 0; i < s->moveN && i < HOST_MOVE_MAX; i++)
    {
        s->moveSrc[i]  = gHostMoveSrc[i];
        s->moveDst[i]  = gHostMoveDst[i];
        s->moveSize[i] = gHostMoveSize[i];
    }

    s->devCalN = gHostDevCalN;
    s->errCnt  = (side == 0) ? ref_gErrorIntCnt : new_gErrorIntCnt;

    for (i = 0; i < 128; i++)
    {
        void *p = *(void **)((char *)&PieVectTable + i * sizeof(void *));
        void *mine = (side == 0) ? (void *)ref_rsvd_ISR : (void *)new_rsvd_ISR;
        if      (p == (void *)ADC_Over_isr)   s->vectCls[i] = 1;
        else if (p == (void *)EPWM1_TZ_isr)   s->vectCls[i] = 2;
        else if (p == (void *)EPWM1_zero_isr) s->vectCls[i] = 3;
        else if (p == (void *)SCI_RXD_isr)    s->vectCls[i] = 4;
        else if (p == (void *)SCI_TXD_isr)    s->vectCls[i] = 5;
        else if (p == mine)                   s->vectCls[i] = 9;
        else                                  s->vectCls[i] = 0;
        s->vectRaw[i] = (uintptr_t)p;
    }
}

/* ---- reporting ----------------------------------------------------------- */

static void report(const char *what, int idx)
{
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s: %s[%d]\n", g_what ? g_what : "?", what, idx);
    printf("  in: mclksts=%d divsel=%d tblsize=%lu\n",
           g_inMclksts, g_inDivsel, (unsigned long)g_tblSize);
}

static void report2(const char *what, const char *detail)
{
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s: %s -- %s\n", g_what ? g_what : "?", what, detail);
}

static void cmp_words(const char *name, const unsigned char *a,
                      const unsigned char *b, unsigned nbytes)
{
    unsigned n = nbytes / 2;
    unsigned i;
    for (i = 0; i < n; i++)
    {
        unsigned short x, y;
        memcpy(&x, a + i * 2, 2);
        memcpy(&y, b + i * 2, 2);
        if (x != y)
        {
            char buf[128];
            sprintf(buf, "w%u ref=%04X new=%04X", i, x, y);
            report2(name, buf);
        }
    }
}

/* ---- comparison ---------------------------------------------------------- */

static void compare(void)
{
    int i;

    cmp_words("SysCtrlRegs",  S[0].sys,     S[1].sys,
              sizeof(struct SYS_CTRL_REGS));
    cmp_words("FlashRegs",    S[0].flash,   S[1].flash,
              sizeof(struct FLASH_REGS));
    cmp_words("AdcRegs",      S[0].adc,     S[1].adc,
              sizeof(struct ADC_REGS));
    cmp_words("PieCtrlRegs",  S[0].pie,     S[1].pie,
              sizeof(struct PIE_CTRL_REGS));
    cmp_words("GpioCtrlRegs", S[0].gpioctl, S[1].gpioctl,
              sizeof(struct GPIO_CTRL_REGS));
    cmp_words("GpioDataRegs", S[0].gpiodat, S[1].gpiodat,
              sizeof(struct GPIO_DATA_REGS));
    cmp_words("CpuTimer1Regs", S[0].tmr1,   S[1].tmr1,
              sizeof(struct CPUTIMER_REGS));

    if (S[0].ier != S[1].ier) report2("IER", "value");
    if (S[0].ifr != S[1].ifr) report2("IFR", "value");

    for (i = 0; i < 7; i++)
    {
        if (S[0].ctl[i] != S[1].ctl[i])
        {
            char buf[64];
            sprintf(buf, "slot%d ref=%u new=%u", i, S[0].ctl[i], S[1].ctl[i]);
            report2("CpuCtl", buf);
        }
    }

    if (S[0].logN != S[1].logN) report2("EventLog", "length");
    {
        int n = (S[0].logN < S[1].logN) ? S[0].logN : S[1].logN;
        for (i = 0; i < n; i++)
        {
            if (S[0].logId[i] != S[1].logId[i] || S[0].logFp[i] != S[1].logFp[i])
            {
                char buf[160];
                sprintf(buf, "ev%d ref=(id%d fp%08lX) new=(id%d fp%08lX)",
                        i, S[0].logId[i], S[0].logFp[i],
                        S[1].logId[i], S[1].logFp[i]);
                report2("EventLog", buf);
                break;
            }
        }
    }

    if (S[0].moveN != S[1].moveN) report2("MovePrg", "count");
    {
        int n = (S[0].moveN < S[1].moveN) ? S[0].moveN : S[1].moveN;
        for (i = 0; i < n; i++)
        {
            if (S[0].moveSrc[i] != S[1].moveSrc[i] ||
                S[0].moveDst[i] != S[1].moveDst[i] ||
                S[0].moveSize[i] != S[1].moveSize[i])
            {
                char buf[160];
                sprintf(buf, "call%d ref=(%08lX,%08lX,%lu) new=(%08lX,%08lX,%lu)",
                        i, S[0].moveSrc[i], S[0].moveDst[i], S[0].moveSize[i],
                        S[1].moveSrc[i], S[1].moveDst[i], S[1].moveSize[i]);
                report2("MovePrg", buf);
                break;
            }
        }
    }

    if (S[0].devCalN != S[1].devCalN) report2("DeviceCal", "count");
    if (S[0].errCnt  != S[1].errCnt)  report2("gErrorIntCnt", "value");

    for (i = 0; i < 128; i++)
    {
        if (S[0].vectCls[i] != S[1].vectCls[i])
        {
            char buf[96];
            sprintf(buf, "slot%d ref=cls%d new=cls%d",
                    i, S[0].vectCls[i], S[1].vectCls[i]);
            report2("PieVectTable", buf);
        }
        else if (S[0].vectCls[i] == 0 && S[0].vectRaw[i] != S[1].vectRaw[i])
        {
            char buf[96];
            sprintf(buf, "slot%d raw ref=%llX new=%llX", i,
                    (unsigned long long)S[0].vectRaw[i],
                    (unsigned long long)S[1].vectRaw[i]);
            report2("PieVectTable", buf);
        }
    }
}

/* ---- the two-phase runner ------------------------------------------------ */

static void run_pll(Uint16 val)
{
    g_what = "InitPll";
    g_patVariant = 2;                 /* alternating edges */

    g_seed = 0x9E3779B9UL;
    prefill_regs();
    prefill_pll();
    gHostResetLog();
    ref_InitPll(val);
    snapshot(0);

    g_seed = 0x9E3779B9UL;
    prefill_regs();
    prefill_pll();
    gHostResetLog();
    new_InitPll(val);
    snapshot(1);

    g_cases++;
    compare();
}

static void run_prg(unsigned long size)
{
    g_what = "copy_prg";
    g_tblSize = size;
    g_patVariant = 3;                 /* seeded random */

    g_seed = 0x9E3779B9UL;
    prefill_regs();
    prefill_pll();
    prefill_table(&prginRAM, 0x12345678UL);
    gHostResetLog();
    ref_copy_prg(&prginRAM);
    snapshot(0);

    g_seed = 0x9E3779B9UL;
    prefill_regs();
    prefill_pll();
    prefill_table(&prginRAM, 0x12345678UL);
    gHostResetLog();
    new_copy_prg(&prginRAM);
    snapshot(1);

    g_cases++;
    compare();
    g_tblSize = 0;
}

static void run_rsvd(int times)
{
    g_what = "rsvd_ISR";
    g_patVariant = 0;

    prefill_regs();
    gHostResetLog();
    {
        int i;
        for (i = 0; i < times; i++)
        {
            ref_rsvd_ISR();
        }
    }
    snapshot(0);

    prefill_regs();
    gHostResetLog();
    {
        int i;
        for (i = 0; i < times; i++)
        {
            new_rsvd_ISR();
        }
    }
    snapshot(1);

    g_cases++;
    compare();
}

/* ---- phases -------------------------------------------------------------- */

typedef void (*FN0)(void);

static const struct { FN0 rf; FN0 nf; const char *name; } SINGLES[] = {
    { ref_InitSysCtrl,          new_InitSysCtrl,          "InitSysCtrl" },
    { ref_InitInterrupt,        new_InitInterrupt,        "InitInterrupt" },
    { ref_SetInterruptEnable,   new_SetInterruptEnable,   "SetInterruptEnable" },
    { ref_InitPeripherals,      new_InitPeripherals,      "InitPeripherals" },
    { ref_InitFlash,            new_InitFlash,            "InitFlash" },
    { ref_KickDog,              new_KickDog,              "KickDog" },
    { ref_DisableDog,           new_DisableDog,           "DisableDog" },
    { ref_EnableDog,            new_EnableDog,            "EnableDog" },
    { ref_InitPeripheralClocks, new_InitPeripheralClocks, "InitPeripheralClocks" },
    { ref_InitPieCtrl,          new_InitPieCtrl,          "InitPieCtrl" },
    { ref_InitPieVectTable,     new_InitPieVectTable,     "InitPieVectTable" },
    { ref_InitSetGpio,          new_InitSetGpio,          "InitSetGpio" },
    { ref_InitSetAdc,           new_InitSetAdc,           "InitSetAdc" },
};

static void phase_singles(void)
{
    unsigned k;
    for (k = 0; k < sizeof SINGLES / sizeof SINGLES[0]; k++)
    {
        /* four dirty register files, so "wrote 0" and "did not write" differ */
        int variant;
        for (variant = 0; variant < 4; variant++)
        {
            g_patVariant = (unsigned)variant;
            g_seed = 0x9E3779B9UL + variant * 0x85EBCA6BUL;
            prefill_regs();
            prefill_pll();
            prefill_table(&prginRAM, 0x5A5A5A5AUL + variant);
            gHostResetLog();
            SINGLES[k].rf();
            snapshot(0);

            g_patVariant = (unsigned)variant;
            g_seed = 0x9E3779B9UL + variant * 0x85EBCA6BUL;
            prefill_regs();
            prefill_pll();
            prefill_table(&prginRAM, 0x5A5A5A5AUL + variant);
            gHostResetLog();
            SINGLES[k].nf();
            snapshot(1);

            g_cases++;
            g_what = SINGLES[k].name;
            compare();
        }
    }
}

static void phase_pll(void)
{
    static const Uint16 vals[] = {
        0, 1, 2, 3, 6, 9, 10, 12, 15, 16, 17, 31, 32, 60, 100, 127, 128,
        255, 256, 1000, 4095, 4096, 32767, 32768, 65534, 65535
    };
    unsigned v;
    int mclk;
    int div;

    for (v = 0; v < sizeof vals / sizeof vals[0]; v++)
    {
        for (mclk = 0; mclk <= 1; mclk++)
        {
            for (div = 0; div <= 3; div++)
            {
                g_inMclksts = mclk;
                g_inDivsel  = div;
                run_pll(vals[v]);
            }
        }
    }
}

static void phase_prg(void)
{
    static const unsigned long sizes[] = {
        0, 1, 2, 3, 0x0F, 0x10, 0x7F, 0x80, 0x7FFE, 0x7FFF,
        0x8000, 0x8001, 0xFFFE, 0xFFFF, 0x10000, 0x10001, 0x12345,
        0x7FFFFFFFUL, 0xFFFFFFFEUL, 0xFFFFFFFFUL
    };
    unsigned k;
    for (k = 0; k < sizeof sizes / sizeof sizes[0]; k++)
    {
        run_prg(sizes[k]);
    }
}

static void phase_rsvd(void)
{
    run_rsvd(1);
    run_rsvd(2);
    run_rsvd(3);
}

static void phase_random(long n)
{
    long k;
    unsigned long rs = 0x243F6A8885A308D3UL;
    int nsing = (int)(sizeof SINGLES / sizeof SINGLES[0]);

    for (k = 0; k < n; k++)
    {
        int variant = (int)(k % 4);
        int pick;

        rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17;
        pick = (int)((rs >> 8) % (unsigned long)(nsing + 2));

        g_inMclksts = (int)((rs >> 20) & 1);
        g_inDivsel  = (int)((rs >> 22) & 3);
        g_tblSize   = (rs >> 24) & 0xFFFFUL;
        if (((rs >> 40) & 7) == 0) g_tblSize = 0;
        if (((rs >> 43) & 7) == 1) g_tblSize = 0xFFFFFFFFUL;

        g_patVariant = (unsigned)variant;
        g_seed = 0x9E3779B9UL + variant * 0x85EBCA6BUL + rs;
        prefill_regs();
        prefill_pll();
        prefill_table(&prginRAM, rs);
        gHostResetLog();
        if (pick == 0)
        {
            ref_InitPll((Uint16)(rs & 0xFFFF));
        }
        else if (pick == 1)
        {
            ref_copy_prg(&prginRAM);
        }
        else
        {
            SINGLES[pick - 2].rf();
        }
        snapshot(0);

        g_patVariant = (unsigned)variant;
        g_seed = 0x9E3779B9UL + variant * 0x85EBCA6BUL + rs;
        prefill_regs();
        prefill_pll();
        prefill_table(&prginRAM, rs);
        gHostResetLog();
        if (pick == 0)
        {
            new_InitPll((Uint16)(rs & 0xFFFF));
        }
        else if (pick == 1)
        {
            new_copy_prg(&prginRAM);
        }
        else
        {
            SINGLES[pick - 2].nf();
        }
        snapshot(1);

        g_cases++;
        g_what = (pick == 0) ? "InitPll"
               : (pick == 1) ? "copy_prg"
               : SINGLES[pick - 2].name;
        compare();
    }
}

/* ---- main ---------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 20000;
    long base;

    phase_singles();
    base = g_cases;
    phase_pll();
    printf("  single entry points (4 dirty files each): %ld\n", base);
    printf("  InitPll (val x MCLKSTS x DIVSEL)        : %ld\n", g_cases - base);

    base = g_cases;
    phase_prg();
    printf("  copy_prg (size edges)                    : %ld\n", g_cases - base);

    base = g_cases;
    phase_rsvd();
    printf("  rsvd_ISR (counter)                       : %ld\n", g_cases - base);

    base = g_cases;
    phase_random(n);
    printf("  random                                   : %ld\n", g_cases - base);

    printf("mainInit_2803x difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    return g_fails ? 1 : 0;
}
