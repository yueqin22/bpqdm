/* Differential test driver: feed the same inputs to the ref and new side and
 * compare every field of the resulting state.
 *
 * Once the implementation has been reformulated the byte-level equivalence
 * proof no longer applies, so what is established here is BEHAVIOURAL
 * equivalence: over the enumerated input space, both sides agree on return
 * values, on all global state, and on hardware register side effects.
 */

#include <stdio.h>
#include <string.h>

#include "ZeroPosCtl.h"
#include "MotorInclude.h"

/* Renamed symbols, see the rename tables in ref_tu.c / new_tu.c */
extern ZERO_POS_STRUCT ref_gZeroPos;
extern ZERO_POS_STRUCT new_gZeroPos;
extern u32 ref_GetPGPos(void);
extern u32 new_GetPGPos(void);
extern void ref_ZeroPosJudge(void);
extern void new_ZeroPosJudge(void);
extern void ref_ZeroPosControl(void);
extern void new_ZeroPosControl(void);

typedef struct {
    ZERO_POS_STRUCT zp;
    u32 qpos;
    u32 tim;
    s16 freqset;
} CASE;

static long g_cases = 0;
static long g_fail  = 0;

/* Reproducible xorshift64. Not rand(), so the sequence cannot change with the
 * host CRT. */
static unsigned long long g_rng = 0x2545F4914F6CDD1DULL;
static u32 rnd32(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return (u32)(g_rng >> 16);
}

static void load(const CASE *c)
{
    ref_gZeroPos = c->zp;
    new_gZeroPos = c->zp;
    EQepRegs->QPOSCNT = c->qpos;
    CpuTimer1.RegsAddr->TIM.all = c->tim;
    gMainCmd.FreqSet = c->freqset;
}

static void dump(const char *tag, const CASE *c, const ZERO_POS_STRUCT *z)
{
    printf("  %s: Flag=%u PosInit=%d Pos=%d KPPos=%d TimeLast=%lu DetaTime=%lu ItKpPos=%d\n",
           tag, (unsigned)z->Flag, (int)z->PosInit, (int)z->Pos, (int)z->KPPos,
           (unsigned long)z->TimeLast, (unsigned long)z->DetaTime, (int)z->ItKpPos);
    printf("      in : qpos=%lu tim=%lu freqset=%d\n",
           (unsigned long)c->qpos, (unsigned long)c->tim, (int)c->freqset);
}

/* which: 0 = ZeroPosControl, 1 = ZeroPosJudge */
static void one(const CASE *c, int which)
{
    ZERO_POS_STRUCT rz, nz;
    u32 rq, nq, rret, nret;
    u32 rtim, ntim;
    s16 rf, nf;

    g_cases++;

    load(c);
    if (which == 0) { ref_ZeroPosControl(); } else { ref_ZeroPosJudge(); }
    rz = ref_gZeroPos;
    rq = EQepRegs->QPOSCNT;
    rtim = CpuTimer1.RegsAddr->TIM.all;
    rf = gMainCmd.FreqSet;
    rret = ref_GetPGPos();

    load(c);
    if (which == 0) { new_ZeroPosControl(); } else { new_ZeroPosJudge(); }
    nz = new_gZeroPos;
    nq = EQepRegs->QPOSCNT;
    ntim = CpuTimer1.RegsAddr->TIM.all;
    nf = gMainCmd.FreqSet;
    nret = new_GetPGPos();

    if (memcmp(&rz, &nz, sizeof rz) != 0 || rq != nq || rtim != ntim ||
        rf != nf || rret != nret) {
        g_fail++;
        if (g_fail <= 20) {
            printf("MISMATCH op=%s\n", which == 0 ? "ZeroPosControl" : "ZeroPosJudge");
            dump("ref", c, &rz);
            dump("new", c, &nz);
            if (rq != nq || rtim != ntim || rf != nf || rret != nret) {
                printf("     side: qpos %lu/%lu  tim %lu/%lu  freqset %d/%d  GetPGPos %lu/%lu\n",
                       (unsigned long)rq, (unsigned long)nq,
                       (unsigned long)rtim, (unsigned long)ntim,
                       (int)rf, (int)nf, (unsigned long)rret, (unsigned long)nret);
            }
        }
    }
}

/* Interesting values per dimension: boundaries, sign flips, overflow points */
static const u32 V32[] = { 0u, 1u, 2u, 0x7FFFu, 0x8000u, 0xFFFFu, 0x10000u,
                           0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFEu, 0xFFFFFFFFu };
static const s16 V16[] = { 0, 1, -1, 2, -2, 10, 11, 8191, 8192, 8193,
                           0x7FFF, (s16)0x8000, 100, -100 };

#define NV32 ((int)(sizeof V32 / sizeof V32[0]))
#define NV16 ((int)(sizeof V16 / sizeof V16[0]))

static void sweep(void)
{
    int a, b, cc, d, e;
    CASE c;

    /* Full cross product: Flag x PosInit x KPPos x QPOSCNT x TIME */
    for (a = 0; a < NV16; a++)
    for (b = 0; b < NV16; b++)
    for (cc = 0; cc < NV16; cc++)
    for (d = 0; d < NV32; d++)
    for (e = 0; e < 4; e++) {
        memset(&c, 0, sizeof c);
        c.zp.Flag    = (u16)V16[a];
        c.zp.PosInit = V16[b];
        c.zp.KPPos   = V16[cc];
        c.zp.TimeLast = V32[d];
        c.zp.DetaTime = V32[(d + 3) % NV32];
        c.qpos = V32[d];
        c.tim  = V32[e];
        c.freqset = V16[a];
        one(&c, 0);
        one(&c, 1);
    }
}

static void random_cases(long n)
{
    CASE c;
    long i;
    for (i = 0; i < n; i++) {
        memset(&c, 0, sizeof c);
        c.zp.Flag    = (u16)rnd32();
        c.zp.PosInit = (s16)rnd32();
        c.zp.PosLast = (s16)rnd32();
        c.zp.Pos     = (s16)rnd32();
        c.zp.KPPos   = (s16)rnd32();
        c.zp.KPSpeed = (s16)rnd32();
        c.zp.KPDecCoff = (s16)rnd32();
        c.zp.ItSet   = (s16)rnd32();
        c.zp.ItKpPos = (s16)rnd32();
        c.zp.ItKpSpeed = (s16)rnd32();
        c.zp.Speed   = (s16)rnd32();
        c.zp.TimeLast = rnd32();
        c.zp.DetaTime = rnd32();
        c.qpos = rnd32();
        c.tim  = rnd32();
        c.freqset = (s16)rnd32();
        one(&c, 0);
        one(&c, 1);
    }
}

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 200000L;

    sweep();
    random_cases(n);

    printf("zeroPosCtl difftest: cases=%ld mismatches=%ld\n", g_cases, g_fail);
    return g_fail ? 1 : 0;
}
