/* Differential test driver for f_p2p.
 *
 * Same idea as the zeroPosCtl case: identical inputs to both sides, then compare
 * every field of the resulting state (the module's own p2pData plus the two
 * error globals it can write).
 *
 * Note: maxFrq == 0 is excluded on purpose. The original performs
 * `frqRun * 10000 / maxFrq` without a guard, so a zero divisor is undefined
 * behaviour on both sides and would abort the run rather than reveal anything.
 * maxFrq is a configured full-scale frequency and is never zero in practice.
 */

#include <stdio.h>
#include <string.h>

#include "p2p_common.h"
#include "f_p2p.h"

extern P2P_DATA_STRUCT ref_p2pData;
extern P2P_DATA_STRUCT new_p2pData;
extern void ref_P2PDataDeal(void);
extern void new_P2PDataDeal(void);

typedef struct {
    P2P_DATA_STRUCT pd;
    struct FUNCCODE_P2P_MOCK fc;
    Uint16 itDisp;
    int32  frqRun;
    int32  frqAimTmp;
    Uint16 frqFdb;
    Uint16 maxFrq;
    Uint16 errorOther;
    Uint16 errorInfo;
} CASE;

static long g_cases = 0;
static long g_fail  = 0;

static unsigned long long g_rng = 0x9E3779B97F4A7C15ULL;
static Uint16 rnd16(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return (Uint16)(g_rng >> 17);
}

static void load(const CASE *c)
{
    ref_p2pData = c->pd;
    new_p2pData = c->pd;
    funcCode.code = c->fc;
    itDisp    = c->itDisp;
    frqRun    = c->frqRun;
    frqAimTmp = c->frqAimTmp;
    frqFdb    = c->frqFdb;
    maxFrq    = c->maxFrq;
    errorOther = c->errorOther;
    errorInfo  = c->errorInfo;
}

static void dump(const char *tag, const P2P_DATA_STRUCT *p, const CASE *c)
{
    printf("  %s: en=%u send=%u rev=%u proc=%u per=%u tcnt=%u err=%u\n",
           tag, p->p2pEnable, p->P2PSendData, p->P2PRevData, p->processValue,
           p->p2pSendPeriod, p->p2pSendTcnt, p->p2pCommErrTcnt);
    printf("      in : type=%u sel=%u over=%u gain=%u off=%u itDisp=%u "
           "frqRun=%ld frqAim=%ld frqFdb=%u maxFrq=%u\n",
           c->fc.p2pTypeSel, c->fc.p2pSendDataSel, c->fc.p2pOverTime,
           c->fc.p2pRevGain, c->fc.p2pRevOffset, c->itDisp,
           (long)c->frqRun, (long)c->frqAimTmp, c->frqFdb, c->maxFrq);
}

static void one(const CASE *c)
{
    P2P_DATA_STRUCT rp, np;
    Uint16 re, ne, ri, ni;

    g_cases++;

    load(c);
    ref_P2PDataDeal();
    rp = ref_p2pData;
    re = errorOther;
    ri = errorInfo;

    load(c);
    new_P2PDataDeal();
    np = new_p2pData;
    ne = errorOther;
    ni = errorInfo;

    if (memcmp(&rp, &np, sizeof rp) != 0 || re != ne || ri != ni) {
        g_fail++;
        if (g_fail <= 20) {
            printf("MISMATCH P2PDataDeal\n");
            dump("ref", &rp, c);
            dump("new", &np, c);
            if (re != ne || ri != ni) {
                printf("     side: errorOther %u/%u  errorInfo %u/%u\n", re, ne, ri, ni);
            }
        }
    }
}

/* Interesting values. maxFrq deliberately excludes 0 (see file header). */
static const Uint16 EN  = 0, ON = 1;
static const Uint16 MAXFRQ[] = { 1, 2, 50, 100, 5000, 10000, 30000, 40000, 65535 };
static const Uint16 SEL[]    = { 0, 1, 2, 3, 4, 65535 };
static const Uint16 OVER[]   = { 0, 1, 2, 10, 65535 };
static const Uint16 CNT[]    = { 0, 1, 49, 50, 51, 99, 100, 101, 65535 };
static const Uint16 GAIN[]   = { 0, 1, 100, 1000, 10000, 32767, 65535 };
static const Uint16 OFF[]    = { 0, 1, 100, 32767, 40000, 65535 };
static const Uint16 PROC[]   = { 0, 1, 9999, 10000, 10001, 32767, 55536, 65535 };
static const Uint16 ITD[]    = { 0, 1, 1000, 2000, 2001, 32767, 65535 };

#define N(a) ((int)(sizeof (a) / sizeof (a)[0]))

static void sweep(void)
{
    int a, b, c2, d, e, f, g, h, i;
    CASE c;

    for (a = 0; a < 2; a++)
    for (b = 0; b < 2; b++)
    for (c2 = 0; c2 < N(SEL); c2++)
    for (d = 0; d < N(OVER); d++)
    for (e = 0; e < N(CNT); e++)
    for (f = 0; f < N(GAIN); f++)
    for (g = 0; g < N(OFF); g++)
    for (h = 0; h < N(PROC); h++)
    for (i = 0; i < N(ITD); i++) {
        memset(&c, 0, sizeof c);
        c.fc.p2pEnable      = a ? ON : EN;
        c.fc.p2pTypeSel     = b ? ON : EN;
        c.fc.p2pSendDataSel = SEL[c2];
        c.fc.p2pOverTime    = OVER[d];
        c.fc.p2pRevGain     = GAIN[f];
        c.fc.p2pRevOffset   = OFF[g];
        c.fc.p2pSendPeriod  = (Uint16)(h * 7);
        c.pd.p2pEnable      = (Uint16)(i & 1);
        c.pd.p2pSendTcnt    = (Uint16)(e * 3);
        c.pd.p2pCommErrTcnt = CNT[e];
        c.pd.P2PSendData    = (Uint16)(f * 11);
        c.pd.P2PRevData     = (Uint16)(g * 13);
        c.pd.processValue   = PROC[h];
        c.pd.p2pSendPeriod  = (Uint16)(i * 5);
        c.itDisp    = ITD[i];
        c.frqRun    = (int32)((long)ITD[i] * 1000 - 30000);
        c.frqAimTmp = (int32)((long)GAIN[f] * 7 - 10000);
        c.frqFdb    = (Uint16)(g * 997);
        c.maxFrq    = MAXFRQ[h % N(MAXFRQ)];
        one(&c);
    }
}

static void random_cases(long n)
{
    CASE c;
    long i;
    for (i = 0; i < n; i++) {
        memset(&c, 0, sizeof c);
        c.pd.p2pEnable      = rnd16();
        c.pd.P2PSendData    = rnd16();
        c.pd.P2PRevData     = rnd16();
        c.pd.processValue   = rnd16();
        c.pd.p2pSendPeriod  = rnd16();
        c.pd.p2pSendTcnt    = rnd16();
        c.pd.p2pCommErrTcnt = rnd16();
        c.fc.p2pEnable      = rnd16();
        c.fc.p2pTypeSel     = rnd16();
        c.fc.p2pSendDataSel = rnd16();
        c.fc.p2pRevDataSel  = rnd16();
        c.fc.p2pRevOffset   = rnd16();
        c.fc.p2pRevGain     = rnd16();
        c.fc.p2pOverTime    = rnd16();
        c.fc.p2pSendPeriod  = rnd16();
        c.itDisp    = rnd16();
        c.frqRun    = (int32)(((unsigned long)rnd16() << 16) | rnd16());
        c.frqAimTmp = (int32)(((unsigned long)rnd16() << 16) | rnd16());
        c.frqFdb    = rnd16();
        c.maxFrq    = (Uint16)(rnd16() | 1u);
        c.errorOther = rnd16();
        c.errorInfo  = rnd16();
        one(&c);
    }
}

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 200000L;

    sweep();
    random_cases(n);

    printf("f_p2p difftest: cases=%ld mismatches=%ld\n", g_cases, g_fail);
    return g_fail ? 1 : 0;
}
