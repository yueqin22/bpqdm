/* Differential test driver for f_dspcan.
 *
 * Six entry points, and the interesting one is the first:
 *
 *   InitdspECan(baud)                    a FOUR-STATE MACHINE driven by two
 *                                        FUNCTION-LEVEL statics (`con`, `count`).
 *                                        One call is not a test -- the state
 *                                        machine has to be walked.
 *   InitTranMbox(mbox, msgid, dataPi)    mailbox + CANMD/CANME + eCanTranEnFlag
 *   InitRecMbox(mbox, msgid, lam)        mailbox + CANOPC/CANMD/CANME + eCanReEnFlag
 *   eCanDataTran(mbox, len, msgid, pi)   gated on eCanTranEnFlag and CANTRS
 *   eCanDataRec(mbox, dataPi)            gated on CANRMP, branches on CANRML
 *
 * ---- why this driver is a SEQUENCER, not a per-call harness ---------------
 *
 * `InitdspECan` holds `static Uint16 con` and `static Uint16 count` across
 * calls, and they are per translation unit. ref and new therefore each have
 * their own pair, and the ONLY thing that keeps them comparable is that both
 * sides see the identical call sequence. So the driver steps ref and new in
 * lockstep, one call at a time, and compares after every single call -- a
 * divergence shows up on the call that caused it, not three calls later.
 *
 * The reachable paths, all of which are exercised:
 *
 *   con=0  configure GpioCtrlRegs + CANMC, zero the 32 mailbox words and the 32
 *          LAM words, preset CANTRR/CANTA/CANRMP/CANGIF0/CANGIF1/CANOPC, con=1
 *   con=1  read CANES.CCE: != 1 -> count++, EDIS, return CAN_INIT_TIME (stay at 1)
 *                          == 1 -> con=2
 *   con=2  write CANBTC from eCanBaud[baud], clear CANMC.CCR, con=3
 *   con=3  read CANES.CCE: != 0 -> count++, EDIS, return CAN_INIT_TIME (stay at 3)
 *                          == 0 -> fall through
 *   tail   con=0, count=0, CANME.all=0, EDIS, both flags 0, return CAN_INIT_SUCC
 *
 * plus the two early exits: baud >= CAN_BAUD_SUM -> CAN_INIT_BAUD_ERR, and
 * count > IINIT_CAN_TIME -> CAN_INIT_TIMEOUT.
 *
 * ---- the latch, and why the timeout phase runs LAST ----------------------
 *
 * Once `count > 3` the function returns CAN_INIT_TIMEOUT at the TOP, before the
 * `con == 0` branch -- so the statics are stuck forever. There is no way to
 * reset them from outside. That is the original behaviour, not something this
 * harness introduced. Consequence: the timeout phase is the final phase, and
 * every earlier phase uses call sequences that drive `con` back to 0 (which
 * also zeroes `count`), so the machine never latches until we ask it to.
 *
 * ---- what the driver owns -------------------------------------------------
 *
 * The peripheral register blocks. The module reads them, writes them, and never
 * allocates them, so the driver plays the chip: before each call it installs a
 * complete register image (either all-ones, or a deterministic PRNG byte
 * stream), optionally forces the bits the state machine reads (CANES.CCE,
 * CANMC.CCR) or the bits the gating tests read (CANTRS, CANRMP, CANRML,
 * mailbox MSGID), and after the call it snapshots ALL FOUR BLOCKS back.
 *
 * Comparing whole register blocks -- not just the bits the module "should"
 * touch -- is what makes "the rewrite stopped writing CANME" or "wrote the
 * wrong bit into CANTRS" visible. All-ones seeding is what makes a write that
 * *clears* a bit visible at all; a zero image cannot distinguish "cleared it"
 * from "never touched it".
 *
 * ---- what is compared -----------------------------------------------------
 *
 *   ret          the return code
 *   eCanTranEnFlag / eCanReEnFlag   the module's only private storage
 *   cpuCtl[7]    EALLOW/EDIS occurrences (see shim/DSP28x_Project.h)
 *   obuf[4]      the caller's buffer for eCanDataRec -- an OUTPUT
 *   periph       ECanaRegs + ECanaMboxes + ECanaLAMRegs + GpioCtrlRegs, whole
 *
 * ---- what is NOT covered --------------------------------------------------
 *
 *   - the const table eCanBaud[] is not read directly; it is observed through
 *     the CANBTC value it produces in state con=2, which is the only way the
 *     module ever uses it;
 *   - EALLOW/EDIS as instructions -- only that they occurred. Interrupt
 *     masking itself is not modelled on the host;
 *   - the target's 16-bit `Uint16`: on C28x `1ul << mbox` with mbox >= 32 is
 *     undefined; MSVC masks the shift count to 5 bits. A handful of mbox
 *     values >= 32 are included, and both sides are host code so the
 *     comparison stays valid, but that is not a claim about C28x;
 *   - real bus traffic: nothing here transmits or receives a frame. The
 *     mailbox contents and the flag registers are whatever the driver put
 *     there;
 *   - what happens when the module runs concurrently with the CAN ISR.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DSP28x_Project.h"
#include "f_dspcan.h"

#define FAIL_LIMIT 20

/* ---- ref side ------------------------------------------------------------ */

extern Uint16 ref_InitdspECan(Uint16 baud);
extern void   ref_InitTranMbox(Uint16 mbox, Uint32 msgid, Uint32 *dataPi);
extern void   ref_InitRecMbox(Uint16 mbox, Uint32 msgid, Uint32 lam);
extern Uint16 ref_eCanDataTran(Uint16 mbox, Uint16 len, Uint32 msgid,
                               Uint32 *dataPi);
extern Uint16 ref_eCanDataRec(Uint16 mbox, Uint32 *dataPi);
extern Uint32 ref_eCanTranEnFlag;
extern Uint32 ref_eCanReEnFlag;

/* ---- new side ------------------------------------------------------------ */

extern Uint16 new_InitdspECan(Uint16 baud);
extern void   new_InitTranMbox(Uint16 mbox, Uint32 msgid, Uint32 *dataPi);
extern void   new_InitRecMbox(Uint16 mbox, Uint32 msgid, Uint32 lam);
extern Uint16 new_eCanDataTran(Uint16 mbox, Uint16 len, Uint32 msgid,
                               Uint32 *dataPi);
extern Uint16 new_eCanDataRec(Uint16 mbox, Uint32 *dataPi);
extern Uint32 new_eCanTranEnFlag;
extern Uint32 new_eCanReEnFlag;

/* ---- the chip ------------------------------------------------------------ */

struct PERIPH
{
    struct ECAN_REGS      r;
    struct ECAN_MBOXES    m;
    struct LAM_REGS       l;
    struct GPIO_CTRL_REGS g;
};

static void periph_install(const struct PERIPH *p)
{
    memcpy((void *)&ECanaRegs,    &p->r, sizeof p->r);
    memcpy((void *)&ECanaMboxes,  &p->m, sizeof p->m);
    memcpy((void *)&ECanaLAMRegs, &p->l, sizeof p->l);
    memcpy((void *)&GpioCtrlRegs, &p->g, sizeof p->g);
}

static void periph_snap(struct PERIPH *p)
{
    memcpy(&p->r, (const void *)&ECanaRegs,    sizeof p->r);
    memcpy(&p->m, (const void *)&ECanaMboxes,  sizeof p->m);
    memcpy(&p->l, (const void *)&ECanaLAMRegs, sizeof p->l);
    memcpy(&p->g, (const void *)&GpioCtrlRegs, sizeof p->g);
}

/* seed == 0 -> all ones (so that a write which CLEARS a bit is visible).
 * seed != 0 -> deterministic byte stream. Either way the image is identical
 * for ref and new, because it is regenerated from the seed for each call. */
static void periph_seed(unsigned long s)
{
    struct PERIPH p;
    unsigned char *b = (unsigned char *)&p;
    unsigned long r = s ? s : 1UL;
    size_t i;

    if (s == 0UL)
    {
        memset(&p, 0xFF, sizeof p);
    }
    else
    {
        for (i = 0; i < sizeof p; i++)
        {
            r = r * 1103515245UL + 12345UL;
            b[i] = (unsigned char)(r >> 16);
        }
    }
    periph_install(&p);
}

static Uint32 *mbox_w(int box, int k)
{
    return ((Uint32 *)&ECanaMboxes) + (box * 4) + k;
}

static Uint32 *lam_w(int idx)
{
    return ((Uint32 *)&ECanaLAMRegs) + idx;
}

/* ---- one call ------------------------------------------------------------ */

#define F_INIT   0
#define F_TRANM  1
#define F_RECM   2
#define F_TRAN   3
#define F_RECV   4

struct STEP
{
    int      fn;
    Uint16   u1;      /* baud (F_INIT) | mbox (F_TRANM/F_RECM/F_TRAN/F_RECV) */
    Uint16   len;     /* F_TRAN */
    Uint32   w1;      /* msgid | lam */
    Uint32   d0;      /* dataPi[0] */
    Uint32   d1;      /* dataPi[1] */
    Uint32   tranEn;  /* initial eCanTranEnFlag */
    Uint32   reEn;    /* initial eCanReEnFlag */
    unsigned long seed;
    int      keep;    /* 1 = do not reinstall the image / reset the flags */
    int      cce;     /* -1 leave random, else force CANES.CCE */
    int      ccr;     /* -1 leave random, else force CANMC.CCR */
    int      trs;     /* -1 leave random, 0/1 force CANTRS[mbox] clear/set */
    int      rmp;     /* -1 leave random, 0/1 force CANRMP[mbox] */
    int      rml;     /* -1 leave random, 0/1 force CANRML[mbox] */
    int      poke;    /* -1 none, else mailbox index to overwrite */
    Uint32   pm_id, pm_ctrl, pm_mdl, pm_mdh;
};

struct OBS
{
    Uint16        ret;
    Uint32        tran;
    Uint32        re;
    Uint16        cpu[7];
    Uint32        obuf[4];
    struct PERIPH p;
};

static struct OBS O[2];

/* The caller's buffers. Per side, so that a call on one side cannot be seen
 * through the other side's buffer. */
static Uint32 g_in[2][2];
static Uint32 g_out[2][4];

static const struct STEP *g_cur;
static long g_cases = 0;
static int  g_fails = 0;

/* Which return codes were actually produced. Reachability has to be measured,
 * not assumed: if the CCE scripting were wrong, every InitdspECan call would
 * return CAN_INIT_TIME and the success tail (CANME.all = 0, both flags
 * cleared, con/count reset) would be dead code in the bench -- and a mutation
 * there would report MISSED for the wrong reason. This histogram is printed at
 * the end so that claim is checkable. */
static long g_retHist[8][8];

static void report(const char *fn, const char *what)
{
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s: %s\n", fn, what);
    printf("  step fn=%d u1=%u len=%u w1=0x%08lX seed=%lu keep=%d\n",
           g_cur->fn, (unsigned)g_cur->u1, (unsigned)g_cur->len,
           (unsigned long)g_cur->w1, g_cur->seed, g_cur->keep);
    printf("  cce=%d ccr=%d trs=%d rmp=%d rml=%d poke=%d\n",
           g_cur->cce, g_cur->ccr, g_cur->trs, g_cur->rmp, g_cur->rml,
           g_cur->poke);
    printf("  tranEn=0x%08lX reEn=0x%08lX\n",
           (unsigned long)g_cur->tranEn, (unsigned long)g_cur->reEn);
    printf("  ret %u/%u  tran 0x%08lX/0x%08lX  re 0x%08lX/0x%08lX\n",
           (unsigned)O[0].ret, (unsigned)O[1].ret,
           (unsigned long)O[0].tran, (unsigned long)O[1].tran,
           (unsigned long)O[0].re, (unsigned long)O[1].re);
    printf("  cpu  ref %u,%u,%u,%u,%u,%u,%u  new %u,%u,%u,%u,%u,%u,%u\n",
           O[0].cpu[0], O[0].cpu[1], O[0].cpu[2], O[0].cpu[3],
           O[0].cpu[4], O[0].cpu[5], O[0].cpu[6],
           O[1].cpu[0], O[1].cpu[1], O[1].cpu[2], O[1].cpu[3],
           O[1].cpu[4], O[1].cpu[5], O[1].cpu[6]);
    printf("  obuf ref %08lX %08lX %08lX %08lX\n",
           (unsigned long)O[0].obuf[0], (unsigned long)O[0].obuf[1],
           (unsigned long)O[0].obuf[2], (unsigned long)O[0].obuf[3]);
    printf("  obuf new %08lX %08lX %08lX %08lX\n",
           (unsigned long)O[1].obuf[0], (unsigned long)O[1].obuf[1],
           (unsigned long)O[1].obuf[2], (unsigned long)O[1].obuf[3]);
}

/* ---- stepping ------------------------------------------------------------ */

static void step(int side, const struct STEP *s)
{
    int i;

    if (!s->keep)
    {
        periph_seed(s->seed);

        if (s->cce >= 0)
        {
            ECanaRegs.CANES.bit.CCE = (Uint16)s->cce;
        }
        if (s->ccr >= 0)
        {
            ECanaRegs.CANMC.bit.CCR = (Uint16)s->ccr;
        }
        if (s->trs >= 0)
        {
            if (s->trs)
            {
                ECanaRegs.CANTRS.all |= 1UL << (s->u1 & 0x1F);
            }
            else
            {
                ECanaRegs.CANTRS.all &= ~(1UL << (s->u1 & 0x1F));
            }
        }
        if (s->rmp >= 0)
        {
            if (s->rmp)
            {
                ECanaRegs.CANRMP.all |= 1UL << (s->u1 & 0x1F);
            }
            else
            {
                ECanaRegs.CANRMP.all &= ~(1UL << (s->u1 & 0x1F));
            }
        }
        if (s->rml >= 0)
        {
            if (s->rml)
            {
                ECanaRegs.CANRML.all |= 1UL << (s->u1 & 0x1F);
            }
            else
            {
                ECanaRegs.CANRML.all &= ~(1UL << (s->u1 & 0x1F));
            }
        }
        if (s->poke >= 0)
        {
            *mbox_w(s->poke, 0) = s->pm_id;
            *mbox_w(s->poke, 1) = s->pm_ctrl;
            *mbox_w(s->poke, 2) = s->pm_mdl;
            *mbox_w(s->poke, 3) = s->pm_mdh;
        }

        if (side == 0)
        {
            ref_eCanTranEnFlag = s->tranEn;
            ref_eCanReEnFlag   = s->reEn;
        }
        else
        {
            new_eCanTranEnFlag = s->tranEn;
            new_eCanReEnFlag   = s->reEn;
        }
    }

    for (i = 0; i < 7; i++)
    {
        gHostCpuCtl[i] = 0;
    }

    g_in[side][0] = s->d0;
    g_in[side][1] = s->d1;
    g_out[side][0] = 0xA5A5A5A5UL;
    g_out[side][1] = 0xA5A5A5A6UL;
    g_out[side][2] = 0xA5A5A5A7UL;
    g_out[side][3] = 0xA5A5A5A8UL;

    if (side == 0)
    {
        switch (s->fn)
        {
            case F_INIT:
                O[0].ret = ref_InitdspECan(s->u1);
                break;
            case F_TRANM:
                ref_InitTranMbox(s->u1, s->w1, g_in[0]);
                O[0].ret = 0;
                break;
            case F_RECM:
                ref_InitRecMbox(s->u1, s->w1, s->d0);
                O[0].ret = 0;
                break;
            case F_TRAN:
                O[0].ret = ref_eCanDataTran(s->u1, s->len, s->w1, g_in[0]);
                break;
            default:
                O[0].ret = ref_eCanDataRec(s->u1, g_out[0]);
                break;
        }
        O[0].tran = ref_eCanTranEnFlag;
        O[0].re   = ref_eCanReEnFlag;
    }
    else
    {
        switch (s->fn)
        {
            case F_INIT:
                O[1].ret = new_InitdspECan(s->u1);
                break;
            case F_TRANM:
                new_InitTranMbox(s->u1, s->w1, g_in[1]);
                O[1].ret = 0;
                break;
            case F_RECM:
                new_InitRecMbox(s->u1, s->w1, s->d0);
                O[1].ret = 0;
                break;
            case F_TRAN:
                O[1].ret = new_eCanDataTran(s->u1, s->len, s->w1, g_in[1]);
                break;
            default:
                O[1].ret = new_eCanDataRec(s->u1, g_out[1]);
                break;
        }
        O[1].tran = new_eCanTranEnFlag;
        O[1].re   = new_eCanReEnFlag;
    }

    for (i = 0; i < 7; i++)
    {
        O[side].cpu[i] = gHostCpuCtl[i];
    }
    for (i = 0; i < 4; i++)
    {
        O[side].obuf[i] = g_out[side][i];
    }
    periph_snap(&O[side].p);
}

static void cmp_step(const char *fn, const struct STEP *s)
{
    g_cur = s;
    g_cases++;

    if (s->fn < 8 && O[1].ret < 8)
    {
        g_retHist[s->fn][O[1].ret]++;
    }

    if (O[0].ret != O[1].ret)
    {
        report(fn, "ret");
    }
    if (O[0].tran != O[1].tran)
    {
        report(fn, "eCanTranEnFlag");
    }
    if (O[0].re != O[1].re)
    {
        report(fn, "eCanReEnFlag");
    }
    if (memcmp(O[0].cpu, O[1].cpu, sizeof O[0].cpu) != 0)
    {
        report(fn, "cpuCtl");
    }
    if (memcmp(O[0].obuf, O[1].obuf, sizeof O[0].obuf) != 0)
    {
        report(fn, "dataPi(out)");
    }
    if (memcmp(&O[0].p, &O[1].p, sizeof O[0].p) != 0)
    {
        report(fn, "peripheral image");
    }
}

static void run_seq(const char *fn, const struct STEP *ss, int n)
{
    int i;

    for (i = 0; i < n; i++)
    {
        step(0, &ss[i]);
        step(1, &ss[i]);
        cmp_step(fn, &ss[i]);
    }
}

/* ---- phases -------------------------------------------------------------- */

/* InitdspECan, the paths that terminate in CAN_INIT_SUCC.
 *
 * Four CCE scripts, all of which return `con` to 0 (and therefore `count` to 0)
 * so the machine never latches:
 *
 *   {0,1,0}    split path: configure, then advance, then finish
 *   {1,1,0}    con goes 0->1->2->3 inside the FIRST call
 *   {0,0,1,0}  two retries at con=1 before advancing
 *   {0,1,1,0}  one retry at con=3
 *
 * baud runs over all seven table entries, so every eCanBaud[] row is turned
 * into a CANBTC value. Seeds alternate between all-ones (so that the clears
 * in the con=0 branch are visible) and three PRNG streams. */
static void phase_init_success(void)
{
    static const int PAT[4][4] = {
        { 0, 1, 0, 0 },
        { 1, 1, 0, 0 },
        { 0, 0, 1, 0 },
        { 0, 1, 1, 0 }
    };
    static const int PATLEN[4] = { 3, 3, 4, 4 };
    static const unsigned long SEED[4] = { 0UL, 0x11111111UL,
                                           0x22222222UL, 0x33333333UL };
    struct STEP ss[4];
    unsigned baud, p, sd;
    int i, k;

    for (baud = 0; baud < CAN_BAUD_SUM; baud++)
    {
        for (p = 0; p < 4; p++)
        {
            for (sd = 0; sd < 4; sd++)
            {
                for (i = 0; i < PATLEN[p]; i++)
                {
                    memset(&ss[i], 0, sizeof ss[i]);
                    ss[i].fn    = F_INIT;
                    ss[i].u1    = (Uint16)baud;
                    ss[i].seed  = SEED[sd];
                    ss[i].cce   = PAT[p][i];
                    ss[i].ccr   = -1;
                    ss[i].trs   = -1;
                    ss[i].rmp   = -1;
                    ss[i].rml   = -1;
                    ss[i].poke  = -1;
                }
                run_seq("InitdspECan", ss, PATLEN[p]);
                k = 0;
                (void)k;
            }
        }
    }
}

/* baud >= CAN_BAUD_SUM is rejected at the very top, before the static state is
 * consulted -- so these are safe to run at any point, including after the
 * timeout latch. */
static void phase_baud_err(void)
{
    static const Uint16 bad[] = { 7, 8, 40, 64, 255, 0xFFFF };
    static const unsigned long SEED[2] = { 0UL, 0x44444444UL };
    struct STEP s;
    unsigned i, k;

    for (i = 0; i < sizeof bad / sizeof bad[0]; i++)
    {
        for (k = 0; k < 2; k++)
        {
            memset(&s, 0, sizeof s);
            s.fn   = F_INIT;
            s.u1   = bad[i];
            s.seed = SEED[k];
            s.cce  = -1;
            s.ccr  = -1;
            s.trs  = -1;
            s.rmp  = -1;
            s.rml  = -1;
            s.poke = -1;
            run_seq("InitdspECan(baudErr)", &s, 1);
        }
    }
}

/* InitdspECan, the timeout latch. MUST BE THE LAST InitdspECan PHASE.
 *
 * count has to reach 4 (IINIT_CAN_TIME is 3, the test is `>`), so four calls
 * that fail the CCE test are needed before the fifth returns CAN_INIT_TIMEOUT.
 * After that the statics are latched for the rest of the process. */
static void phase_timeout(void)
{
    static const unsigned long SEED[2] = { 0UL, 0x55555555UL };
    struct STEP ss[6];
    unsigned k;
    int i;

    for (k = 0; k < 2; k++)
    {
        for (i = 0; i < 6; i++)
        {
            memset(&ss[i], 0, sizeof ss[i]);
            ss[i].fn   = F_INIT;
            ss[i].u1   = 4;             /* a valid baud: the timeout check
                                         * comes after the baud check */
            ss[i].seed = SEED[k];
            ss[i].cce  = 0;             /* CCE never satisfies either test */
            ss[i].ccr  = -1;
            ss[i].trs  = -1;
            ss[i].rmp  = -1;
            ss[i].rml  = -1;
            ss[i].poke = -1;
        }
        run_seq("InitdspECan(timeout)", ss, 6);
    }
}

/* InitTranMbox: the mailbox write, the CANMD clear, the CANME set, and the
 * `eCanTranEnFlag |= 1ul << mbox` -- note it shifts by mbox, not by `id`. */
static void phase_tran_mbox(void)
{
    static const Uint32 IDS[3] = { 0x00000000UL, 0x12345678UL, 0xFFFFFFFFUL };
    static const Uint16 BIG[6] = { 32, 33, 63, 64, 127, 255 };
    static const unsigned long SEED[2] = { 0UL, 0x66666666UL };
    struct STEP s;
    unsigned m, k, v;

    for (m = 0; m < 32; m++)
    {
        for (k = 0; k < 2; k++)
        {
            for (v = 0; v < 3; v++)
            {
                memset(&s, 0, sizeof s);
                s.fn    = F_TRANM;
                s.u1    = (Uint16)m;
                s.w1    = IDS[v];
                s.d0    = 0xDEADBEEFUL;
                s.d1    = 0x0F1E2D3CUL;
                s.tranEn = 0xAAAAAAAAUL;
                s.reEn   = 0x55555555UL;
                s.seed  = SEED[k];
                s.cce   = -1;
                s.ccr   = -1;
                s.trs   = -1;
                s.rmp   = -1;
                s.rml   = -1;
                s.poke  = -1;
                run_seq("InitTranMbox", &s, 1);
            }
        }
    }

    /* mbox >= 32: `mbox & 0x1f` keeps the mailbox index in range, but the flag
     * update shifts by the raw mbox. See the file header. */
    for (v = 0; v < sizeof BIG / sizeof BIG[0]; v++)
    {
        for (k = 0; k < 2; k++)
        {
            memset(&s, 0, sizeof s);
            s.fn    = F_TRANM;
            s.u1    = BIG[v];
            s.w1    = 0x87654321UL;
            s.d0    = 1UL;
            s.d1    = 2UL;
            s.tranEn = 0UL;
            s.reEn   = 0UL;
            s.seed  = SEED[k];
            s.cce   = -1;
            s.ccr   = -1;
            s.trs   = -1;
            s.rmp   = -1;
            s.rml   = -1;
            s.poke  = -1;
            run_seq("InitTranMbox(bigMbox)", &s, 1);
        }
    }
}

/* InitRecMbox: bit 7 of mbox selects RTR, bit 6 selects the CANOPC polarity,
 * and here the flag update DOES shift by `id`. */
static void phase_rec_mbox(void)
{
    static const Uint16 MODE[4] = { 0x00, 0x40, 0x80, 0xC0 };
    static const Uint32 LAMS[3] = { 0UL, 0x1FFFFFFFUL, 0xFFFFFFFFUL };
    static const Uint16 BIG[4] = { 32, 63, 64, 255 };
    static const unsigned long SEED[2] = { 0UL, 0x77777777UL };
    struct STEP s;
    unsigned m, k, v;

    for (m = 0; m < 32; m++)
    {
        for (v = 0; v < 4; v++)
        {
            for (k = 0; k < 3; k++)
            {
                memset(&s, 0, sizeof s);
                s.fn    = F_RECM;
                s.u1    = (Uint16)(m | MODE[v]);
                s.w1    = 0x0BADF00DUL;
                s.d0    = LAMS[k];
                s.tranEn = 0x0F0F0F0FUL;
                s.reEn   = 0xF0F0F0F0UL;
                s.seed  = SEED[v & 1];
                s.cce   = -1;
                s.ccr   = -1;
                s.trs   = -1;
                s.rmp   = -1;
                s.rml   = -1;
                s.poke  = -1;
                run_seq("InitRecMbox", &s, 1);
            }
        }
    }

    for (v = 0; v < sizeof BIG / sizeof BIG[0]; v++)
    {
        for (k = 0; k < 2; k++)
        {
            memset(&s, 0, sizeof s);
            s.fn    = F_RECM;
            s.u1    = BIG[v];
            s.w1    = 0x11111111UL;
            s.d0    = 0x22222222UL;
            s.tranEn = 0UL;
            s.reEn   = 0UL;
            s.seed  = SEED[k];
            s.cce   = -1;
            s.ccr   = -1;
            s.trs   = -1;
            s.rmp   = -1;
            s.rml   = -1;
            s.poke  = -1;
            run_seq("InitRecMbox(bigMbox)", &s, 1);
        }
    }
}

/* eCanDataTran: three gates (the flag, CANTRS, and the mailbox's own MSGID
 * bits 29..31 which are preserved rather than overwritten). */
static void phase_data_tran(void)
{
    static const Uint32 IDS[4] = { 0x00000000UL, 0x12345678UL,
                                   0xFFFFFFFFUL, 0xE0000000UL };
    static const Uint32 POKE[4] = { 0x00000000UL, 0xE0000000UL,
                                    0x1FFFFFFFUL, 0xFFFFFFFFUL };
    static const unsigned long SEED[2] = { 0UL, 0x88888888UL };
    struct STEP s;
    unsigned m, k, v, g;

    for (m = 0; m < 8; m++)
    {
        for (g = 0; g < 3; g++)          /* g = eCanTranEnFlag state */
        {
            for (k = 0; k < 2; k++)      /* k = CANTRS state */
            {
                for (v = 0; v < 4; v++)  /* v = msgid / mailbox content */
                {
                    memset(&s, 0, sizeof s);
                    s.fn    = F_TRAN;
                    s.u1    = (Uint16)m;
                    s.len   = (Uint16)(v * 3);
                    s.w1    = IDS[v];
                    s.d0    = 0xCAFEBABEUL;
                    s.d1    = 0xFEEDFACEUL;
                    s.tranEn = (g == 0) ? 0UL
                             : (g == 1) ? (1UL << m) : 0xFFFFFFFFUL;
                    s.reEn   = 0x12345678UL;
                    s.seed  = SEED[v & 1];
                    s.cce   = -1;
                    s.ccr   = -1;
                    s.trs   = (int)k;
                    s.rmp   = -1;
                    s.rml   = -1;
                    s.poke  = (int)m;
                    s.pm_id   = POKE[v];
                    s.pm_ctrl = 0x11223344UL;
                    s.pm_mdl  = 0x55667788UL;
                    s.pm_mdh  = 0x99AABBCCUL;
                    run_seq("eCanDataTran", &s, 1);
                }
            }
        }
    }

    /* mailbox index out of range: `mbox &= 0x1f` should fold it in */
    for (v = 0; v < 4; v++)
    {
        memset(&s, 0, sizeof s);
        s.fn    = F_TRAN;
        s.u1    = (Uint16)(32 + v);
        s.len   = 8;
        s.w1    = 0x80000000UL;
        s.d0    = 1UL;
        s.d1    = 2UL;
        s.tranEn = 0xFFFFFFFFUL;
        s.reEn   = 0UL;
        s.seed  = 0UL;
        s.cce   = -1;
        s.ccr   = -1;
        s.trs   = 0;
        s.rmp   = -1;
        s.rml   = -1;
        s.poke  = -1;
        run_seq("eCanDataTran(bigMbox)", &s, 1);
    }
}

/* eCanDataRec: gated on CANRMP, branches on CANRML, and writes FOUR words into
 * the caller's buffer -- the buffer is pre-filled with a value the module never
 * writes, so a dropped store is visible. */
static void phase_data_rec(void)
{
    static const unsigned long SEED[2] = { 0UL, 0x99999999UL };
    struct STEP s;
    unsigned m, k, e, r, v;

    for (m = 0; m < 8; m++)
    {
        for (k = 0; k < 2; k++)          /* k = CANRMP */
        {
            for (e = 0; e < 2; e++)      /* e = CANRML */
            {
                for (v = 0; v < 2; v++)  /* v = seed */
                {
                    memset(&s, 0, sizeof s);
                    s.fn    = F_RECV;
                    s.u1    = (Uint16)m;
                    s.tranEn = 0xFFFFFFFFUL;
                    s.reEn   = 0UL;
                    s.seed  = SEED[v];
                    s.cce   = -1;
                    s.ccr   = -1;
                    s.trs   = -1;
                    s.rmp   = (int)k;
                    s.rml   = (int)e;
                    s.poke  = (int)m;
                    s.pm_id   = 0x13572468UL;
                    s.pm_ctrl = 0xABCDEF01UL;
                    s.pm_mdl  = 0x0F1E2D3CUL;
                    s.pm_mdh  = 0xF0E1D2C3UL;
                    run_seq("eCanDataRec", &s, 1);
                }
            }
        }
    }

    for (r = 0; r < 4; r++)
    {
        memset(&s, 0, sizeof s);
        s.fn    = F_RECV;
        s.u1    = (Uint16)(32 + r);
        s.seed  = 0UL;
        s.cce   = -1;
        s.ccr   = -1;
        s.trs   = -1;
        s.rmp   = 1;
        s.rml   = 1;
        s.poke  = -1;
        run_seq("eCanDataRec(bigMbox)", &s, 1);
    }
}

/* ---- random sweep -------------------------------------------------------- */

static unsigned long g_rng = 0x20261009UL;

static unsigned long rnd(void)
{
    g_rng = g_rng * 1103515245UL + 12345UL;
    return (g_rng >> 8) & 0xFFFFFFUL;
}

static void rnd_step(struct STEP *s, int fn)
{
    memset(s, 0, sizeof *s);
    s->fn    = fn;
    s->u1    = (Uint16)(rnd() & 0x3F);
    s->len   = (Uint16)rnd();
    s->w1    = (Uint32)rnd() * 0x00010001UL;
    s->d0    = (Uint32)rnd() * 0x00010001UL;
    s->d1    = (Uint32)rnd() * 0x00010001UL;
    s->tranEn = (Uint32)rnd() * 0x00010001UL;
    s->reEn   = (Uint32)rnd() * 0x00010001UL;
    s->seed  = rnd();
    s->cce   = -1;
    s->ccr   = -1;
    s->trs   = (int)(rnd() % 3) - 1;
    s->rmp   = (int)(rnd() % 3) - 1;
    s->rml   = (int)(rnd() % 3) - 1;
    s->poke  = (rnd() & 1) ? (int)(rnd() & 0x1F) : -1;
    s->pm_id   = (Uint32)rnd() * 0x00010001UL;
    s->pm_ctrl = (Uint32)rnd() * 0x00010001UL;
    s->pm_mdl  = (Uint32)rnd() * 0x00010001UL;
    s->pm_mdh  = (Uint32)rnd() * 0x00010001UL;
}

static void phase_random(long n)
{
    static const int PAT[2][4] = { { 0, 1, 0, 0 }, { 1, 1, 0, 0 } };
    static const int PATLEN[2] = { 3, 3 };
    struct STEP ss[4];
    long k;
    int i;

    for (k = 0; k < n; k++)
    {
        unsigned pick = (unsigned)(rnd() % 5);

        if (pick == 0)
        {
            /* InitdspECan: ALWAYS a complete sequence, so the machine returns
             * to con=0 and the random sweep cannot latch it.
             *
             * This is not a stylistic choice. An earlier version also let a
             * single InitdspECan call through with a random baud and a random
             * CANES.CCE; over a few thousand draws `count` climbed past
             * IINIT_CAN_TIME and the statics latched, after which every later
             * InitdspECan call returned CAN_INIT_TIMEOUT regardless of its
             * input -- the rest of the sweep silently stopped testing the
             * state machine. The return-code histogram at the end of main is
             * what caught it. */
            int p = (int)(rnd() & 1);
            Uint16 baud = (Uint16)(rnd() % CAN_BAUD_SUM);

            for (i = 0; i < PATLEN[p]; i++)
            {
                rnd_step(&ss[i], F_INIT);
                ss[i].u1  = baud;
                ss[i].cce = PAT[p][i];
            }
            run_seq("random/InitdspECan", ss, PATLEN[p]);
        }
        else
        {
            struct STEP s;
            rnd_step(&s, (int)pick);
            run_seq("random", &s, 1);
        }
    }
}

/* ---- main ---------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 20000;
    long base;

    /* The register blocks are compared by memcpy, so their layout has to be
     * the chip's. A short block would silently hide registers. */
    printf("  sizeof: ECAN_REGS=%u ECAN_MBOXES=%u LAM_REGS=%u GPIO_CTRL_REGS=%u\n",
           (unsigned)sizeof(struct ECAN_REGS),
           (unsigned)sizeof(struct ECAN_MBOXES),
           (unsigned)sizeof(struct LAM_REGS),
           (unsigned)sizeof(struct GPIO_CTRL_REGS));
    printf("  stride MBOX0->MBOX1 = %d words\n",
           (int)(((Uint32 *)&ECanaMboxes.MBOX1) -
                 ((Uint32 *)&ECanaMboxes.MBOX0)));
    if (sizeof(struct ECAN_MBOXES) != 32u * 4u * sizeof(Uint32) ||
        ((Uint32 *)&ECanaMboxes.MBOX1) - ((Uint32 *)&ECanaMboxes.MBOX0) != 4)
    {
        printf("FATAL: mailbox block is not the packed 32x4 word layout the "
               "module's `Uint32 *` aliasing assumes\n");
        return 2;
    }

    base = g_cases;
    phase_init_success();
    printf("  InitdspECan (succ paths)     : %ld\n", g_cases - base);

    base = g_cases;
    phase_baud_err();
    printf("  InitdspECan (baud err)       : %ld\n", g_cases - base);

    base = g_cases;
    phase_tran_mbox();
    printf("  InitTranMbox                 : %ld\n", g_cases - base);

    base = g_cases;
    phase_rec_mbox();
    printf("  InitRecMbox                  : %ld\n", g_cases - base);

    base = g_cases;
    phase_data_tran();
    printf("  eCanDataTran                 : %ld\n", g_cases - base);

    base = g_cases;
    phase_data_rec();
    printf("  eCanDataRec                  : %ld\n", g_cases - base);

    base = g_cases;
    phase_random(n);
    printf("  random mixed                 : %ld\n", g_cases - base);

    /* LAST: this one latches the statics. See the file header. */
    base = g_cases;
    phase_timeout();
    printf("  InitdspECan (timeout latch)  : %ld\n", g_cases - base);

    /* Return-code histogram: proves which paths the bench actually reached.
     * Columns are the raw return value; what each value MEANS depends on the
     * function (1/2/3/4 for InitdspECan, 0/1/2 for eCanDataTran, 3/4/5 for
     * eCanDataRec). */
    printf("  ret hist:  fn           ret0  ret1  ret2 ret3 ret4 ret5\n");
    {
        int f;
        for (f = 0; f <= F_RECV; f++)
        {
            if (g_retHist[f][0] == 0 && g_retHist[f][1] == 0 &&
                g_retHist[f][2] == 0 && g_retHist[f][3] == 0 &&
                g_retHist[f][4] == 0 && g_retHist[f][5] == 0)
            {
                continue;
            }
            printf("             %-12s %8ld %8ld %ld %ld %ld %ld\n",
                   f == F_INIT ? "InitdspECan" :
                   f == F_TRANM ? "InitTranMbox" :
                   f == F_RECM ? "InitRecMbox" :
                   f == F_TRAN ? "eCanDataTran" : "eCanDataRec",
                   g_retHist[f][0], g_retHist[f][1], g_retHist[f][2],
                   g_retHist[f][3], g_retHist[f][4], g_retHist[f][5]);
        }
    }

    printf("f_dspcan difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    return g_fails ? 1 : 0;
}
