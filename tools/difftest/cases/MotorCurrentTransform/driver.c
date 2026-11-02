/* Differential test driver for MotorCurrentTransform.
 *
 * Both sides are fed byte-identical input snapshots; every field of every
 * struct the module touches is compared after each call.
 *
 * Coverage notes
 * --------------
 * 1. `UVWToAlphBetaAxes` is declared `void inline`.  On the TARGET (TI cl2000)
 *    that yields no external symbol, but MSVC emits a linkonce/COMDAT copy in
 *    BOTH translation units -- and the linker then **folds the two same-named
 *    definitions into one**, silently keeping only the first.  The first run
 *    proved it: MC1..MC6 (every mutation inside that body) all came back MISSED
 *    while `build/_me_repro_inline.py` showed `side_b()` returning the OTHER
 *    translation unit's value.  So the body was dead code on the new side and
 *    "transitive coverage through ChangeCurrent" was a fiction.
 *    Fix: each TU renames the symbol (see ref_tu.c / new_tu.c), which makes the
 *    two definitions distinct, and `sweep_uvw()` now drives the function
 *    DIRECTLY so the coverage no longer depends on ChangeCurrent at all.
 * 2. `AlphBetaToDQ` is driven on the full cross product of its three inputs,
 *    including the `qsin(16384 - angle)` / `qsin(angle)` pairing, so both
 *    trig paths and the sign of `angle` are exercised.
 * 3. `DQToAmpTheta` is driven over M x T including both signs and the
 *    M == 0 / T == 0 corners, which is where `atan`'s x == 0 special case and
 *    the `x < 0` quadrant fixup live.
 * 4. `ChangeCurrent` is driven over U/V/W x IMPhase x VoltPhaseApply.  IMPhase
 *    is a `long` that is shifted right by 16 before use, so the sweep includes
 *    values that straddle the shift boundary.
 */

#include <stdio.h>
#include <string.h>

#include "MotorCurrentTransform.h"
#include "MotorInclude.h"

extern long atol(const char *);

extern void ref_AlphBetaToDQ(ALPHABETA_STRUCT *, int, MT_STRUCT_Q24 *);
extern void new_AlphBetaToDQ(ALPHABETA_STRUCT *, int, MT_STRUCT_Q24 *);
extern void ref_DQToAmpTheta(MT_STRUCT *, AMPTHETA_STRUCT *);
extern void new_DQToAmpTheta(MT_STRUCT *, AMPTHETA_STRUCT *);
extern void ref_ChangeCurrent(void);
extern void new_ChangeCurrent(void);
/* Renamed per TU (see ref_tu.c / new_tu.c): without a distinct name the linker
 * folds the two `inline` definitions into one and only the first side's body
 * survives.  These are the same function the module itself calls internally. */
extern void ref_UVWToAlphBetaAxes(UVW_STRUCT_Q24 *, ALPHABETA_STRUCT *);
extern void new_UVWToAlphBetaAxes(UVW_STRUCT_Q24 *, ALPHABETA_STRUCT *);

typedef struct {
    UVW_STRUCT_Q24   uvw;
    ALPHABETA_STRUCT alph;
    ALPHABETA_STRUCT alph12;
    MT_STRUCT_Q24    mt24;
    MT_STRUCT        mt12;
    AMPTHETA_STRUCT  amp;
    OUT_VOLT_STRUCT  volt;
    ANGLE_STRUCT     phase;
} SNAP;

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

static void install(const SNAP *s)
{
    gIUVWQ24     = s->uvw;
    gIAlphBeta   = s->alph;
    gIAlphBetaQ12 = s->alph12;
    gIMTQ24      = s->mt24;
    gIMTQ12      = s->mt12;
    gIAmpTheta   = s->amp;
    gOutVolt     = s->volt;
    gPhase       = s->phase;
}

static void snapshot(SNAP *s)
{
    s->uvw    = gIUVWQ24;
    s->alph   = gIAlphBeta;
    s->alph12 = gIAlphBetaQ12;
    s->mt24   = gIMTQ24;
    s->mt12   = gIMTQ12;
    s->amp    = gIAmpTheta;
    s->volt   = gOutVolt;
    s->phase  = gPhase;
}

static int cmp_snap(const SNAP *a, const SNAP *b)
{
    return memcmp(a, b, sizeof *a) != 0;
}

static void show(const char *tag, const SNAP *s)
{
    printf("  %s: uvw=%ld/%ld/%ld alph=%ld/%ld a12=%ld/%ld mt24=%ld/%ld\n",
           tag, (long)s->uvw.U, (long)s->uvw.V, (long)s->uvw.W,
           (long)s->alph.Alph, (long)s->alph.Beta,
           (long)s->alph12.Alph, (long)s->alph12.Beta,
           (long)s->mt24.M, (long)s->mt24.T);
    printf("      mt12=%d/%d amp=%u theta=%d pw=%d imphase=%ld vpa=%d\n",
           (int)s->mt12.M, (int)s->mt12.T, (unsigned)s->amp.Amp,
           (int)s->amp.Theta, (int)s->amp.PowerAngle,
           (long)s->phase.IMPhase, (int)s->volt.VoltPhaseApply);
}

/* ---------------- value sets ---------------- */

static const long V32[] = {
    0L, 1L, -1L, 2L, -2L, 100L, -100L, 23170L, -23170L,
    65536L, -65536L, 0x7FFFL, -32768L, 0x10000L, -65537L,
    0x7FFFFFFFL, (long)0x80000000L, -1L
};
#define NV32 ((int)(sizeof V32 / sizeof V32[0]))

static const int V16[] = {
    0, 1, -1, 2, -2, 100, -100, 4096, -4096, 8192, -8192,
    16384, -16384, 32767, -32768
};
#define NV16 ((int)(sizeof V16 / sizeof V16[0]))

static const long PHASE[] = {
    0L, 1L, 0xFFFFL, 0x10000L, 0x1FFFFL, 0x7FFF0000L,
    0x80000000L, 0xFFFF0000L, 0x00010000L, (long)0xFFFEFFFFL
};
#define NPHASE ((int)(sizeof PHASE / sizeof PHASE[0]))

/* ---------------- UVWToAlphBetaAxes ---------------- */

static void one_uvw(const UVW_STRUCT_Q24 *uvw)
{
    UVW_STRUCT_Q24  ru, nu;
    ALPHABETA_STRUCT ra, na;

    g_cases++;

    ru = *uvw; nu = *uvw;
    ra.Alph = ra.Beta = 0;
    na.Alph = na.Beta = 0;
    ref_UVWToAlphBetaAxes(&ru, &ra);
    new_UVWToAlphBetaAxes(&nu, &na);

    if (memcmp(&ra, &na, sizeof ra) != 0 ||
        memcmp(&ru, &nu, sizeof ru) != 0) {
        g_fail++;
        if (g_fail <= 20) {
            printf("MISMATCH UVWToAlphBetaAxes U=%ld V=%ld W=%ld\n",
                   (long)uvw->U, (long)uvw->V, (long)uvw->W);
            printf("  ref: Alph=%ld Beta=%ld\n",
                   (long)ra.Alph, (long)ra.Beta);
            printf("  new: Alph=%ld Beta=%ld\n",
                   (long)na.Alph, (long)na.Beta);
        }
    }
}

/* U/V/W cross the same value set AlphBetaToDQ gets, plus the pairs that make
 * V - W hit each sign and each magnitude corner (that difference is the only
 * thing the second output depends on). */
static void sweep_uvw(void)
{
    int a, b, c;
    UVW_STRUCT_Q24 u;

    for (a = 0; a < NV32; a++)
    for (b = 0; b < NV32; b++)
    for (c = 0; c < NV32; c++) {
        u.U = V32[a];
        u.V = V32[b];
        u.W = V32[c];
        one_uvw(&u);
    }
}

/* ---------------- AlphBetaToDQ ---------------- */

static void one_dq(const ALPHABETA_STRUCT *ab, int angle, const MT_STRUCT_Q24 *mt)
{
    ALPHABETA_STRUCT rab, nab;
    MT_STRUCT_Q24 rmt, nmt;

    g_cases++;

    rab = *ab; rmt = *mt;
    ref_AlphBetaToDQ(&rab, angle, &rmt);

    nab = *ab; nmt = *mt;
    new_AlphBetaToDQ(&nab, angle, &nmt);

    if (memcmp(&rab, &nab, sizeof rab) != 0 ||
        memcmp(&rmt, &nmt, sizeof rmt) != 0) {
        g_fail++;
        if (g_fail <= 20) {
            printf("MISMATCH AlphBetaToDQ angle=%d in Alph=%ld Beta=%ld mt=%ld/%ld\n",
                   angle, (long)ab->Alph, (long)ab->Beta,
                   (long)mt->M, (long)mt->T);
            printf("  ref: alph=%ld/%ld mt=%ld/%ld\n",
                   (long)rab.Alph, (long)rab.Beta, (long)rmt.M, (long)rmt.T);
            printf("  new: alph=%ld/%ld mt=%ld/%ld\n",
                   (long)nab.Alph, (long)nab.Beta, (long)nmt.M, (long)nmt.T);
        }
    }
}

/* ---------------- DQToAmpTheta ---------------- */

static void one_amp(const MT_STRUCT *mt, const AMPTHETA_STRUCT *am)
{
    MT_STRUCT rmt, nmt;
    AMPTHETA_STRUCT ra, na;

    g_cases++;

    rmt = *mt; nmt = *mt;
    ra  = *am; na  = *am;

    ref_DQToAmpTheta(&rmt, &ra);
    new_DQToAmpTheta(&nmt, &na);

    if (memcmp(&ra, &na, sizeof ra) != 0 || memcmp(&rmt, &nmt, sizeof rmt) != 0) {
        g_fail++;
        if (g_fail <= 20) {
            printf("MISMATCH DQToAmpTheta M=%d T=%d\n", (int)mt->M, (int)mt->T);
            printf("  ref: Amp=%u Theta=%d\n", (unsigned)ra.Amp, (int)ra.Theta);
            printf("  new: Amp=%u Theta=%d\n", (unsigned)na.Amp, (int)na.Theta);
        }
    }
}

/* ---------------- ChangeCurrent ---------------- */

static void one_change(const SNAP *in)
{
    SNAP r, n;

    g_cases++;

    install(in);
    ref_ChangeCurrent();
    snapshot(&r);

    install(in);
    new_ChangeCurrent();
    snapshot(&n);

    if (cmp_snap(&r, &n)) {
        g_fail++;
        if (g_fail <= 20) {
            printf("MISMATCH ChangeCurrent\n");
            show("ref", &r);
            show("new", &n);
            show("in ", in);
        }
    }
}

static void sweep_dq(void)
{
    int a, b, c;
    ALPHABETA_STRUCT ab;
    MT_STRUCT_Q24 mt;

    for (a = 0; a < NV32; a++)
    for (b = 0; b < NV32; b++)
    for (c = 0; c < NV16; c++) {
        ab.Alph = V32[a];
        ab.Beta = V32[b];
        mt.M = V32[a];
        mt.T = V32[b];
        one_dq(&ab, V16[c], &mt);
    }
}

static void sweep_amp(void)
{
    int a, b;
    MT_STRUCT mt;
    AMPTHETA_STRUCT am;

    memset(&am, 0, sizeof am);
    for (a = 0; a < NV16; a++)
    for (b = 0; b < NV16; b++) {
            mt.M = V16[a];
        mt.T = V16[b];
        one_amp(&mt, &am);
    }
}

static void sweep_change(void)
{
    int a, b, c, d, e;
    SNAP s;

    for (a = 0; a < 10; a++)
    for (b = 0; b < 10; b++)
    for (c = 0; c < 10; c++)
    for (d = 0; d < NPHASE; d++)
    for (e = 0; e < 6; e++) {
        memset(&s, 0, sizeof s);
        s.uvw.U = V32[a];
        s.uvw.V = V32[b];
        s.uvw.W = V32[c];
        s.phase.IMPhase = PHASE[d];
        s.volt.VoltPhaseApply = V16[e];
        s.mt12.M = V16[(a + b) % NV16];
        s.mt12.T = V16[(b + c) % NV16];
        s.amp.PowerAngle = V16[(a + c) % NV16];
        one_change(&s);
    }
}

static void random_cases(long n)
{
    long i;
    SNAP s;

    for (i = 0; i < n; i++) {
        memset(&s, 0, sizeof s);
        s.uvw.U = (long)rnd32() << 16 | (long)(rnd32() & 0xFFFF);
        s.uvw.V = (long)rnd32() << 16 | (long)(rnd32() & 0xFFFF);
        s.uvw.W = (long)rnd32() << 16 | (long)(rnd32() & 0xFFFF);
        s.phase.IMPhase = (long)rnd32() << 16 | (long)(rnd32() & 0xFFFF);
        s.volt.VoltPhaseApply = (int)(rnd32() & 0xFFFF);
        s.mt12.M = (int)(rnd32() & 0xFFFF);
        s.mt12.T = (int)(rnd32() & 0xFFFF);
        s.amp.PowerAngle = (int)(rnd32() & 0xFFFF);
        s.amp.Theta = (int)(rnd32() & 0xFFFF);
        s.amp.Amp = (Uint)(rnd32() & 0xFFFF);
        one_change(&s);

        {
            ALPHABETA_STRUCT ab;
            MT_STRUCT_Q24 mt;
            ab.Alph = (long)rnd32() << 16 | (long)(rnd32() & 0xFFFF);
            ab.Beta = (long)rnd32() << 16 | (long)(rnd32() & 0xFFFF);
            mt.M = (long)rnd32() << 16 | (long)(rnd32() & 0xFFFF);
            mt.T = (long)rnd32() << 16 | (long)(rnd32() & 0xFFFF);
            one_dq(&ab, (int)(rnd32() & 0xFFFF), &mt);
        }
        {
            MT_STRUCT mt;
            AMPTHETA_STRUCT am;
            mt.M = (int)(rnd32() & 0xFFFF);
            mt.T = (int)(rnd32() & 0xFFFF);
            am = s.amp;
            one_amp(&mt, &am);
        }
    }
}

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 20000L;

    sweep_uvw();
    sweep_dq();
    sweep_amp();
    sweep_change();
    random_cases(n);

    printf("MotorCurrentTransform difftest: cases=%ld mismatches=%ld\n",
           g_cases, g_fail);
    return g_fail ? 1 : 0;
}
