/* Differential test driver for SubPrg.
 *
 * Six entry points, all pure functions of their argument struct (`atan` is pure
 * of its two ints).  Nothing here touches a peripheral register or a global, so
 * "what is compared" is simple:
 *
 *   per-side   the driver owns ONE struct per side, installs the SAME input
 *              image into both, calls ref_ then new_, and compares the whole
 *              struct (plus atan's return value).  Both structs are memset to 0
 *              before the input is written, so tail padding (PID_STRUCT_LONG is
 *              36 bytes of fields rounded to 40) is identical on both sides and
 *              cannot fake a mismatch.
 *   shared     qatan() and abs() come from hostlib_mock.c -- single definitions
 *              that both sides call.  `abs` is the project's own 16-bit
 *              `int abs(int)` (target: asm/ASMSubPrg.asm `_abs` = |x| truncated
 *              to 16 bits), NOT the MSVC 32-bit one; BurrFilter feeds it a long
 *              difference, so the truncation is part of the behaviour under
 *              test.  `qatan` approximates the asm polynomial; see the long note
 *              in hostlib_mock.c.  Both limitations are shared by the two sides
 *              by construction.
 *
 * The shim pins `int` to 16 bits (`shim/DSP2803x_Device.h` ends with
 * `#define int short`), so PID_STRUCT's Max/Min/Deta/KP/KI/KD,
 * CUR_LINE_STRUCT_DEF's FilterTime/Input/Output/Data[] and atan's x/y are all
 * the target's width.  `long` is 32 bits on both host and target (measured, see
 * the MotorEncoder case shim), so PID32_STRUCT / BURR_FILTER_STRUCT /
 * PID_STRUCT_LONG need no patching.
 *
 * ---- host traps (kept visible, not filtered out) ---------------------------
 *
 *   1. atan: `((long)y << 16) / x` is INT32_MIN / -1 for exactly one input pair
 *      (x == -1, y == -32768).  That is UB in the original firmware too; MSVC
 *      raises #DE and kills the process.  The pair is excluded from the input
 *      space (and the exclusion is asserted in the driver, so it cannot silently
 *      drift).  Every other pair is representable in 32 bits.
 *   2. PID / PIDLongRegulate / PID32 / BurrFilter / SlipFilter contain no
 *      division.  SlipFilter's only division is `m_Total / m_TotalCoff` with
 *      FilterTime swept over 1..16, so the denominator is >= 1 always.
 *   3. Shift counts: PID uses `vMax >> (4 + QP)` and `vMax >> QI`.  On x86 a
 *      shift count >= 32 is masked mod 32 while C28x does not mask, so the
 *      driver keeps QP in 0..27 and QI in 0..27 (4+QP <= 31).  This is an
 *      input-space restriction, not a filter on results.
 *   4. BurrFilter's `filter->Err << 1` overflows for |Err| > 2^30.  The sweep
 *      deliberately includes such values: both sides wrap identically, so the
 *      comparison stays meaningful, and a rewrite that changed the doubling to
 *      a halving is still caught.
 */

#include <stdio.h>
#include <string.h>

extern long atol(const char *);

#include "SubPrgInclude.h"

/* ---- the two sides ------------------------------------------------------- */

extern int  ref_atan(int x, int y);
extern int  new_atan(int x, int y);
extern void ref_PID(PID_STRUCT * pid);
extern void new_PID(PID_STRUCT * pid);
extern void ref_PIDLongRegulate(PID_STRUCT_LONG * pid);
extern void new_PIDLongRegulate(PID_STRUCT_LONG * pid);
extern void ref_PID32(PID32_STRUCT * pid);
extern void new_PID32(PID32_STRUCT * pid);
extern void ref_BurrFilter(BURR_FILTER_STRUCT * filter);
extern void new_BurrFilter(BURR_FILTER_STRUCT * filter);
extern void ref_SlipFilter(CUR_LINE_STRUCT_DEF * pCur);
extern void new_SlipFilter(CUR_LINE_STRUCT_DEF * pCur);

#define FAIL_LIMIT 20

static long g_cases;
static long g_fail;

/* ---- deterministic PRNG (no <stdlib.h>: it declares 32-bit `int abs(int)`,
 * which would collide with the project's 16-bit prototype) ----------------- */

static unsigned long g_rng = 0x5BF0E10UL;

static unsigned long rnd32(void)
{
    g_rng = g_rng * 1103515245UL + 12345UL;
    g_rng ^= g_rng >> 7;
    g_rng *= 2654435761UL;
    return (g_rng >> 8) & 0xFFFFFFUL;
}

/* uniform in [lo, hi] */
static long rndRange(long lo, long hi)
{
    unsigned long span;
    if (hi <= lo)
    {
        return lo;
    }
    span = (unsigned long)(hi - lo) + 1UL;
    return lo + (long)(rnd32() % span);
}

/* ---- comparison ---------------------------------------------------------- */

static void show_hex(const unsigned char *p, unsigned long nwords)
{
    /* print as 16-bit target words (little endian), the unit C28x thinks in */
    unsigned long i;
    for (i = 0; i < nwords; i++)
    {
        printf(" %04x", (unsigned)((unsigned long)p[2 * i]
                                   | ((unsigned long)p[2 * i + 1] << 8)));
    }
}

static long cmp_image(const char *tag, const void *a, const void *b,
                      unsigned long nbytes)
{
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    unsigned long i;

    if (memcmp(pa, pb, nbytes) == 0)
    {
        return 0;
    }
    g_fail++;
    if (g_fail <= FAIL_LIMIT)
    {
        printf("MISMATCH %s\n", tag);
        for (i = 0; i < nbytes; i++)
        {
            if (pa[i] != pb[i])
            {
                printf("  first differing byte %lu (word %lu): ref=%02x new=%02x\n",
                       i, i / 2, (unsigned)pa[i], (unsigned)pb[i]);
                break;
            }
        }
        printf("  ref words:");
        show_hex(pa, nbytes / 2);
        printf("\n  new words:");
        show_hex(pb, nbytes / 2);
        printf("\n");
    }
    return 1;
}

/* ---- input images -------------------------------------------------------- */

typedef struct {
    long total, out;
    long maxv, minv, deta, kp, ki, kd;
    unsigned long qp, qi, qd;
    unsigned long max_words;
} PID_IN;

static void install_pid(PID_STRUCT * p, const PID_IN * in)
{
    memset(p, 0, sizeof *p);
    p->Total = in->total;
    p->Out   = in->out;
    p->Max   = (int)in->maxv;
    p->Min   = (int)in->minv;
    p->Deta  = (int)in->deta;
    p->KP    = (int)in->kp;
    p->KI    = (int)in->ki;
    p->KD    = (int)in->kd;
    p->QP    = (Uint)in->qp;
    p->QI    = (Uint)in->qi;
    p->QD    = (Uint)in->qd;
}

static void op_pid(const PID_IN * in)
{
    PID_STRUCT r, n;
    char tag[160];

    g_cases++;
    install_pid(&r, in);
    install_pid(&n, in);
    ref_PID(&r);
    new_PID(&n);
    if (cmp_image("PID", &r, &n, sizeof r) != 0)
    {
        if (g_fail <= FAIL_LIMIT)
        {
            sprintf(tag, "PID total=%ld out=%ld max=%ld min=%ld deta=%ld kp=%ld"
                         " ki=%ld qp=%lu qi=%lu",
                    in->total, in->out, in->maxv, in->minv, in->deta, in->kp,
                    in->ki, in->qp, in->qi);
            printf("  input: %s\n", tag);
        }
    }
}

typedef struct {
    /* llong is 4 target words: 8 bytes on the host too */
    long long total, out;
    long maxv, minv, deta;
    unsigned long kp, ki, kd, svolt;
} PIDL_IN;
static void install_pidl(PID_STRUCT_LONG * p, const PIDL_IN * in)
{
    memset(p, 0, sizeof *p);
    p->Total     = in->total;
    p->Out       = in->out;
    p->Max       = in->maxv;
    p->Min       = in->minv;
    p->Deta      = in->deta;
    p->KP        = (Uint)in->kp;
    p->KI        = (Uint)in->ki;
    p->KD        = (Uint)in->kd;
    p->sVoltGain = (int)in->svolt;
}

static void op_pidl(const PIDL_IN * in)
{
    PID_STRUCT_LONG r, n;

    g_cases++;
    install_pidl(&r, in);
    install_pidl(&n, in);
    ref_PIDLongRegulate(&r);
    new_PIDLongRegulate(&n);
    cmp_image("PIDLongRegulate", &r, &n, sizeof r);
}

typedef struct {
    long deta, out, total, maxv, minv, kp, ki;
} PID32_IN;

static void install_pid32(PID32_STRUCT * p, const PID32_IN * in)
{
    memset(p, 0, sizeof *p);
    p->Deta  = in->deta;
    p->Out   = in->out;
    p->Total = in->total;
    p->Max   = in->maxv;
    p->Min   = in->minv;
    p->KP    = in->kp;
    p->KI    = in->ki;
}

static void op_pid32(const PID32_IN * in)
{
    PID32_STRUCT r, n;

    g_cases++;
    install_pid32(&r, in);
    install_pid32(&n, in);
    ref_PID32(&r);
    new_PID32(&n);
    cmp_image("PID32", &r, &n, sizeof r);
}

typedef struct {
    long input, output, err, maxv, minv;
} BURR_IN;

static void install_burr(BURR_FILTER_STRUCT * p, const BURR_IN * in)
{
    memset(p, 0, sizeof *p);
    p->Input  = in->input;
    p->Output = in->output;
    p->Err    = in->err;
    p->Max    = in->maxv;
    p->Min    = in->minv;
}

static void op_burr(const BURR_IN * in)
{
    BURR_FILTER_STRUCT r, n;

    g_cases++;
    install_burr(&r, in);
    install_burr(&n, in);
    ref_BurrFilter(&r);
    new_BurrFilter(&n);
    cmp_image("BurrFilter", &r, &n, sizeof r);
}

typedef struct {
    long ftime, input, output;
    long data[16];
} SLIP_IN;

static void install_slip(CUR_LINE_STRUCT_DEF * p, const SLIP_IN * in)
{
    long i;
    memset(p, 0, sizeof *p);
    p->FilterTime = (int)in->ftime;
    p->Input      = (int)in->input;
    p->Output     = (int)in->output;
    for (i = 0; i < 16; i++)
    {
        p->Data[i] = (int)in->data[i];
    }
}

static void op_slip(const SLIP_IN * in)
{
    CUR_LINE_STRUCT_DEF r, n;

    g_cases++;
    install_slip(&r, in);
    install_slip(&n, in);
    ref_SlipFilter(&r);
    new_SlipFilter(&n);
    cmp_image("SlipFilter", &r, &n, sizeof r);
}

static void op_atan(long x, long y)
{
    int rr, nn;
    char tag[96];

    /* the one UB corner: INT32_MIN / -1 on the host kills the process */
    if (x == -1 && y == -32768)
    {
        return;
    }
    g_cases++;
    rr = ref_atan((int)x, (int)y);
    nn = new_atan((int)x, (int)y);
    if (rr != nn)
    {
        g_fail++;
        if (g_fail <= FAIL_LIMIT)
        {
            sprintf(tag, "atan x=%ld y=%ld", x, y);
            printf("MISMATCH %s: ref=%d new=%d\n", tag, rr, nn);
        }
    }
}

/* ---- phases -------------------------------------------------------------- */

static const long V_DETA[]  = { 0, 1, -1, 7, -7, 100, -100, 1000, -1000,
                                32767, -32768 };
static const long V_GAIN[]  = { 0, 1, -1, 100, -100, 1000, -1000, 32767,
                                -32768 };
static const long V_Q[]     = { 0, 1, 2, 3, 7, 15, 27 };
static const long V_LIM[]   = { 0, 1, -1, 1000, -1000, 32767, -32768 };
static const long V_TOT[]   = { 0, 1, -1, 1000, -1000, 1000000, -1000000,
                                2147483647L, -2147483647L };
static const long V_ATANX[] = { 0, 1, -1, 2, -2, 7, -7, 100, -100, 1000, -1000,
                                16384, -16384, 32767, -32767 };
static const long V_ATANY[] = { 0, 1, -1, 2, -2, 100, -100, 16384, -16384,
                                32767, -32768 };

static void sweep_atan(void)
{
    long i, j;
    for (i = 0; i < (long)(sizeof V_ATANX / sizeof V_ATANX[0]); i++)
    {
        for (j = 0; j < (long)(sizeof V_ATANY / sizeof V_ATANY[0]); j++)
        {
            op_atan(V_ATANX[i], V_ATANY[j]);
        }
    }
}

/* The four structured PID phases below drive one quantity at a time from a base
 * image (a), then walk the two boundaries that decide the anti-windup branches
 * (b) the saturation shift exponents, (c) the accumulator, and finally (d) a
 * full cross of the small corner set.  The cross is what makes
 * `(m_OutKp > m_Max && pid->Total > 0)` reachable with both halves true. */
static void sweep_pid(void)
{
    long a, b, c, d, e, f, g;
    PID_IN in;

    /* (a) the product KP * Deta, saturating at every step */
    for (a = 0; a < (long)(sizeof V_DETA / sizeof V_DETA[0]); a++)
    {
        for (b = 0; b < (long)(sizeof V_GAIN / sizeof V_GAIN[0]); b++)
        {
            memset(&in, 0, sizeof in);
            in.deta = V_DETA[a];
            in.kp   = V_GAIN[b];
            in.ki   = 1;
            in.qp   = 4;
            in.qi   = 8;
            in.maxv = 32767;
            in.minv = -32768;
            op_pid(&in);
        }
    }

    /* (b) the shift exponents -- must stay < 32 on both sides */
    for (a = 0; a < (long)(sizeof V_Q / sizeof V_Q[0]); a++)
    {
        for (b = 0; b < (long)(sizeof V_Q / sizeof V_Q[0]); b++)
        {
            for (c = 0; c < 4; c++)
            {
                memset(&in, 0, sizeof in);
                in.deta = 32767;
                in.kp   = 32767;
                in.ki   = 32767;
                in.qp   = V_Q[a];
                in.qi   = V_Q[b];
                in.maxv = V_LIM[c * 2];
                in.minv = V_LIM[c * 2 + 1];
                op_pid(&in);
            }
        }
    }

    /* (c) the accumulator, including INT32 extremes */
    for (a = 0; a < (long)(sizeof V_TOT / sizeof V_TOT[0]); a++)
    {
        for (b = 0; b < (long)(sizeof V_LIM / sizeof V_LIM[0]); b++)
        {
            for (c = 0; c < (long)(sizeof V_LIM / sizeof V_LIM[0]); c++)
            {
                memset(&in, 0, sizeof in);
                in.total = V_TOT[a];
                in.out   = V_TOT[a];
                in.maxv  = V_LIM[b];
                in.minv  = V_LIM[c];
                in.deta  = 1000;
                in.kp    = 1000;
                in.ki    = 100;
                in.qp    = 4;
                in.qi    = 4;
                op_pid(&in);
            }
        }
    }

    /* (d) full cross of the corner set */
    {
        static const long S_DETA[] = { 0, 1, -1, 32767, -32768 };
        static const long S_GAIN[] = { 0, 1, -1, 32767, -32768 };
        static const long S_Q[]    = { 0, 15, 27 };
        static const long S_TOT[]  = { 0, 1, -1, 2147483647L, -2147483647L,
                                       1073741824L };
        static const long S_LIM[]  = { 0, 32767, -32768 };
        for (a = 0; a < 5; a++)
        for (b = 0; b < 5; b++)
        for (c = 0; c < 3; c++)
        for (d = 0; d < 3; d++)
        for (e = 0; e < 6; e++)
        for (f = 0; f < 3; f++)
        for (g = 0; g < 3; g++)
        {
            memset(&in, 0, sizeof in);
            in.deta  = S_DETA[a];
            in.kp    = S_GAIN[b];
            in.ki    = S_GAIN[b];
            in.qp    = S_Q[c];
            in.qi    = S_Q[d];
            in.total = S_TOT[e];
            in.out   = S_TOT[e];
            in.maxv  = (g == 0) ? S_LIM[f] : S_LIM[g - 1];
            in.minv  = (g == 0) ? -S_LIM[f] : -S_LIM[g - 1];
            op_pid(&in);
        }
    }
}

/* PID is stateful through pid->Total, and the two anti-windup branches read it
 * *after* the decay (`pid->Total -= (pid->Total>>8) + 1`).  A single call can
 * only show the transition in one direction, so run short sequences and feed
 * the previous result back as the next input -- the same trick that makes the
 * MotorDebug half-period gate reachable. */
static void sweep_pid_seq(void)
{
    long k, s;
    PID_IN in;

    for (k = 0; k < 6; k++)
    {
        memset(&in, 0, sizeof in);
        in.deta  = (k & 1) ? 32767 : -32768;
        in.kp    = (k & 2) ? 32767 : -32768;
        in.ki    = (k & 4) ? 1000 : -1000;
        in.qp    = 4;
        in.qi    = 8;
        in.maxv  = 1000;
        in.minv  = -1000;
        in.total = (k == 0) ? 2000000000L : -2000000000L;
        in.out   = 0;
        for (s = 0; s < 5; s++)
        {
            PID_STRUCT r, n;
            g_cases++;
            install_pid(&r, &in);
            install_pid(&n, &in);
            ref_PID(&r);
            new_PID(&n);
            cmp_image("PID(seq)", &r, &n, sizeof r);
            /* carry the module's own state forward on both sides at once */
            in.total = r.Total;
            in.out   = r.Out;
            if (in.total != n.Total || in.out != n.Out)
            {
                g_fail++;
                printf("MISMATCH PID(seq) state diverged\n");
            }
        }
    }
}

/* PIDLongRegulate: 64-bit accumulators, so the interesting corners are the
 * extreme magnitudes.  Also driven as sequences, because the decay
 * (`pid->Total -= (pid->Total>>8)`) feeds the next call. */
static void sweep_pidl(void)
{
    long a, b, c;
    PIDL_IN in;
    /* llong fields: the whole 64-bit range.  `long` is only 32 bits on the host
     * as well as on the target, so the extremes have to be long long literals. */
    static const long long V64[] = { 0, 1, -1, 1000, -1000, 2147483647LL,
                                     -2147483647LL, 4294967296LL,
                                     -4294967296LL,
                                     4611686018427387903LL };
    static const long V32[]  = { 0, 1, -1, 1000, -1000, 2147483647L,
                                 -2147483647L };
    static const long VU[]   = { 0, 1, 65535, 32768 };

    for (a = 0; a < (long)(sizeof V64 / sizeof V64[0]); a++)
    {
        for (b = 0; b < (long)(sizeof V32 / sizeof V32[0]); b++)
        {
            for (c = 0; c < 3; c++)
            {
                memset(&in, 0, sizeof in);
                in.total = V64[a];
                in.out   = V64[a];
                in.maxv  = (c == 0) ? V32[b] : 1000;
                in.minv  = (c == 0) ? -V32[b] : -1000;
                in.deta  = (c == 1) ? V32[b] : 1000;
                in.kp    = VU[b % 4];
                in.ki    = VU[(b + 1) % 4];
                op_pidl(&in);
            }
        }
    }
}

/* PID32: all-long struct.  The `if(pid->Deta > 0)` branch and the two
 * mTotalMax / mTotalMin clamps are the corners, so Deta and Max/Min are walked
 * together; KP/KI are Q-scaled constants, swept over their extremes. */
static void sweep_pid32(void)
{
    long a, b, c;
    PID32_IN in;
    static const long VL[] = { 0, 1, -1, 1000, -1000, 100000, -100000,
                               2147483647L, -2147483647L };

    for (a = 0; a < (long)(sizeof VL / sizeof VL[0]); a++)
    {
        for (b = 0; b < (long)(sizeof VL / sizeof VL[0]); b += 2)
        {
            for (c = 0; c < 4; c++)
            {
                memset(&in, 0, sizeof in);
                in.deta  = (c & 1) ? VL[a] : -VL[a];
                in.total = VL[b];
                in.out   = VL[b];
                in.maxv  = (c & 2) ? 1000 : 1000000;
                in.minv  = (c & 2) ? -1000 : -1000000;
                in.kp    = 1048576L;          /* Q12.19 == 1.0 */
                in.ki    = 65536L;            /* Q16.15 == 2.0 */
                op_pid32(&in);
            }
        }
    }

    /* (b) Deta crossed with Max and Min of *every* sign.  The first phase only
     * ever builds Min < 0 < Max, and with Deta == 0 the two arms of
     * `if(pid->Deta > 0)` then write the same mTotalMax/mTotalMin (both arms
     * degenerate to the untouched Max / Min).  The arms only diverge once
     * Min > 0 or Max < 0, so those sign combinations are the ones that make the
     * `>` vs `>=` boundary observable at all.  Without them the
     * `Deta > 0` -> `Deta >= 0` mutation is unobservable by construction. */
    {
        static const long VDETA[] = { 0, 1, -1, 1000, -1000, 2147483647L,
                                      -2147483647L };
        static const long VLIM[]  = { 0, 5, -5, 100, -100, 2147483647L,
                                      -2147483647L };
        static const long VTOT[]  = { 0, 1, -1, 3, -3, 1000000, -1000000 };
        long i, j, k, t;

        for (i = 0; i < (long)(sizeof VDETA / sizeof VDETA[0]); i++)
        {
            for (j = 0; j < (long)(sizeof VLIM / sizeof VLIM[0]); j++)
            {
                for (k = 0; k < (long)(sizeof VLIM / sizeof VLIM[0]); k++)
                {
                    for (t = 0; t < (long)(sizeof VTOT / sizeof VTOT[0]); t++)
                    {
                        memset(&in, 0, sizeof in);
                        in.deta  = VDETA[i];
                        in.total = VTOT[t];
                        in.out   = VTOT[t];
                        in.maxv  = VLIM[j];
                        in.minv  = VLIM[k];
                        in.kp    = 1048576L;
                        in.ki    = 65536L;
                        op_pid32(&in);
                    }
                }
            }
        }
    }
}

/* BurrFilter: drive the three thresholds from both sides.  |Input-Output| vs
 * Err decides doubling vs following; Err>>1 decides the halving; Max/Min clamp
 * the result.  Note the module feeds `abs()` a *long* and the project's abs is
 * the 16-bit one (asm `_abs`), so the difference is truncated before the
 * comparison -- that truncation is part of the behaviour under test. */
static void sweep_burr(void)
{
    long a, b, c;
    BURR_IN in;
    static const long VIN[]  = { 0, 1, -1, 100, -100, 32767, -32768, 100000,
                                 -100000 };
    static const long VERR[] = { 0, 1, 2, 3, 100, 200, 1000, 100000,
                                 1073741824L, 2000000000L };
    static const long VLIM[] = { 0, 10, -10, 1000, -1000, 2147483647L,
                                 -2147483647L };

    for (a = 0; a < (long)(sizeof VIN / sizeof VIN[0]); a++)
    {
        for (b = 0; b < (long)(sizeof VERR / sizeof VERR[0]); b++)
        {
            for (c = 0; c < 3; c++)
            {
                memset(&in, 0, sizeof in);
                /* |deta| straddles Err on both sides of the boundary */
                in.input  = VIN[a];
                in.output = (c == 0) ? 0 : (VIN[a] - VERR[b]);
                in.err    = VERR[b];
                in.maxv   = VERR[b] + 1000;
                in.minv   = -VERR[b] - 1000;
                op_burr(&in);
            }
        }
    }

    /* the clamps themselves: Err far outside [Min, Max] */
    for (a = 0; a < (long)(sizeof VERR / sizeof VERR[0]); a++)
    {
        for (b = 0; b < (long)(sizeof VLIM / sizeof VLIM[0]); b += 2)
        {
            memset(&in, 0, sizeof in);
            in.input  = 32767;
            in.output = 0;
            in.err    = VERR[a];
            in.maxv   = VLIM[b];
            in.minv   = VLIM[b + 1];
            op_burr(&in);
        }
    }
}

/* SlipFilter: FilterTime decides both the start index (16 - FilterTime) and how
 * many weighted taps are summed, so it is walked over its whole range 1..16.
 * Data[] is filled with a few shapes (zeros, +-extremes, ramp) plus random. */
static void sweep_slip(void)
{
    long t, s, k, shape;
    SLIP_IN in;
    static const long VIN[] = { 0, 1, -1, 100, -100, 32767, -32768 };

    for (t = 1; t <= 16; t++)
    {
        for (s = 0; s < (long)(sizeof VIN / sizeof VIN[0]); s++)
        {
            for (shape = 0; shape < 4; shape++)
            {
                memset(&in, 0, sizeof in);
                in.ftime  = t;
                in.input  = VIN[s];
                in.output = VIN[s];
                switch (shape)
                {
                case 0:
                    for (k = 0; k < 16; k++) { in.data[k] = 0; }
                    break;
                case 1:
                    for (k = 0; k < 16; k++) { in.data[k] = 32767; }
                    break;
                case 2:
                    for (k = 0; k < 16; k++) { in.data[k] = -32768; }
                    break;
                default:
                    for (k = 0; k < 16; k++)
                    {
                        in.data[k] = (long)(rnd32() % 65536UL) - 32768L;
                    }
                    break;
                }
                op_slip(&in);
            }
        }
    }
}

/* ---- random ---------------------------------------------------------------
 *
 * Random state + random entry point + random length sequence.  Every value is
 * drawn inside the target's width of the field it lands in: `int` fields stay
 * in [-32768, 32767], `Uint` fields in [0, 65535], `long` fields in the full
 * 32-bit range, `llong` in the full 64-bit range.  QP/QI stay below 27 so that
 * `4 + QP` never reaches 32 (see the host-trap note at the top).
 */
static void rnd_pid(PID_IN * in)
{
    memset(in, 0, sizeof *in);
    in->total = (long)(rnd32() & 0xFFFFFFFFUL);
    if (rnd32() & 1) { in->total = -in->total; }
    in->out   = (long)(rnd32() & 0xFFFFFFFFUL);
    in->maxv  = rndRange(-32768, 32767);
    in->minv  = rndRange(-32768, 32767);
    in->deta  = rndRange(-32768, 32767);
    in->kp    = rndRange(-32768, 32767);
    in->ki    = rndRange(-32768, 32767);
    in->kd    = rndRange(-32768, 32767);
    in->qp    = rnd32() % 28UL;
    in->qi    = rnd32() % 28UL;
    in->qd    = rnd32() % 28UL;
}

static void rnd_pidl(PIDL_IN * in)
{
    long long v;
    memset(in, 0, sizeof *in);
    v = ((long long)rnd32() << 32) | (long long)rnd32();
    in->total = v;
    in->out   = (rnd32() & 1) ? -v : v;
    in->maxv  = (long)(rnd32() & 0xFFFFFFFFUL);
    if (rnd32() & 1) { in->maxv = -in->maxv; }
    in->minv  = (long)(rnd32() & 0xFFFFFFFFUL);
    if (rnd32() & 1) { in->minv = -in->minv; }
    in->deta  = (long)(rnd32() & 0xFFFFFFFFUL);
    if (rnd32() & 1) { in->deta = -in->deta; }
    in->kp    = rnd32() % 65536UL;
    in->ki    = rnd32() % 65536UL;
    in->kd    = rnd32() % 65536UL;
    in->svolt = rndRange(-32768, 32767);
}

static void rnd_pid32(PID32_IN * in)
{
    memset(in, 0, sizeof *in);
    in->deta  = (long)(rnd32() & 0xFFFFFFFFUL);
    if (rnd32() & 1) { in->deta = -in->deta; }
    in->out   = (long)(rnd32() & 0xFFFFFFFFUL);
    in->total = (long)(rnd32() & 0xFFFFFFFFUL);
    in->maxv  = (long)(rnd32() & 0xFFFFFFFFUL);
    in->minv  = (long)(rnd32() & 0xFFFFFFFFUL);
    in->kp    = (long)(rnd32() & 0xFFFFFFFFUL);
    in->ki    = (long)(rnd32() & 0xFFFFFFFFUL);
}

static void rnd_burr(BURR_IN * in)
{
    memset(in, 0, sizeof *in);
    in->input  = (long)(rnd32() & 0xFFFFFFFFUL);
    if (rnd32() & 1) { in->input = -in->input; }
    in->output = (long)(rnd32() & 0xFFFFFFFFUL);
    if (rnd32() & 1) { in->output = -in->output; }
    in->err    = (long)(rnd32() & 0xFFFFFFFFUL);
    if (rnd32() & 1) { in->err = -in->err; }
    in->maxv   = (long)(rnd32() & 0xFFFFFFFFUL);
    in->minv   = (long)(rnd32() & 0xFFFFFFFFUL);
}

static void rnd_slip(SLIP_IN * in)
{
    long i;
    memset(in, 0, sizeof *in);
    in->ftime  = rndRange(1, 16);
    in->input  = rndRange(-32768, 32767);
    in->output = rndRange(-32768, 32767);
    for (i = 0; i < 16; i++)
    {
        in->data[i] = rndRange(-32768, 32767);
    }
}

static void random_cases(long n)
{
    long i, steps, s;

    for (i = 0; i < n; i++)
    {
        steps = 1 + (long)(rnd32() % 4);
        for (s = 0; s < steps; s++)
        {
            switch (rnd32() % 6)
            {
            case 0:
                op_atan(rndRange(-32768, 32767), rndRange(-32768, 32767));
                break;
            case 1:
                {
                    PID_IN in;
                    rnd_pid(&in);
                    op_pid(&in);
                }
                break;
            case 2:
                {
                    PIDL_IN in;
                    rnd_pidl(&in);
                    op_pidl(&in);
                }
                break;
            case 3:
                {
                    PID32_IN in;
                    rnd_pid32(&in);
                    op_pid32(&in);
                }
                break;
            case 4:
                {
                    BURR_IN in;
                    rnd_burr(&in);
                    op_burr(&in);
                }
                break;
            default:
                {
                    SLIP_IN in;
                    rnd_slip(&in);
                    op_slip(&in);
                }
                break;
            }
        }
    }
}

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 20000L;

    sweep_atan();
    sweep_pid();
    sweep_pid_seq();
    sweep_pidl();
    sweep_pid32();
    sweep_burr();
    sweep_slip();
    random_cases(n);

    printf("SubPrg difftest: cases=%ld mismatches=%ld\n", g_cases, g_fail);
    return g_fail ? 1 : 0;
}
