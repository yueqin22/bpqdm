/* Differential test driver for MotorDataExchange.
 *
 * The module is the motor<->function parameter marshalling layer: five entry
 * points, and every statement in all five is `lhs = rhs;`. There is no control
 * flow, no arithmetic, and no hardware -- the entire module is a hand-written
 * copy table:
 *
 *   ParSend05Ms()   empty body (declared, never implemented)
 *   ParGet05Ms()    buffer[0..13]              -> 14 named struct fields
 *   ParSend2Ms()    59 named struct fields     -> buffer[0..59]
 *   ParGet2Ms()     buffer[0..114]             -> 115 named struct fields
 *   ParSendTune()   19 named struct fields     -> buffer[0..19]
 *
 * so "behaviour equivalent" here means "writes the same value into the same
 * destination field for every input", and nothing else.
 *
 * ---- how the input space is built ---------------------------------------
 *
 * Because the module is pure copying, the naive "randomise everything, compare"
 * is not enough: if the destination already holds the value that is about to be
 * written, a rewrite that *drops* the write is invisible. The driver therefore
 * fills the three roles from three INDEPENDENT deterministic streams:
 *
 *   stream 1  the 49 shared externals + uReservedData + gSoftVersion
 *             (sources for ParSend2Ms / ParSendTune, destinations for
 *              ParGet05Ms / ParGet2Ms)
 *   stream 2  gSendToMotor05MsDataBuff / gSendToMotor2MsDataBuff
 *             (sources for ParGet05Ms / ParGet2Ms)
 *   stream 3  gSendToFunctionDataBuff / gParaIdToFunctionDataBuff /
 *             gRealTimeToFunctionDataBuff  (destinations for ParSend2Ms /
 *             ParSendTune)
 *
 * A dropped write then leaves the destination holding a value from a *different*
 * stream than the one the reference wrote, so ref and new disagree. Dropped
 * writes, swapped sources and wrong indices are all covered by this; the
 * negative control confirms it empirically rather than by argument.
 *
 * Random fills are complemented by a boundary phase (all-ones, all-zeros, the
 * 0x00008000 / 0x00007FFF sign pair, and the 16/32-bit split patterns) because
 * this module's whole job is copying between differently sized fields -- see the
 * width note below.
 *
 * ---- the shared-externals problem ---------------------------------------
 *
 * The 49 externals are shared by both sides (only the module's own exports get
 * per-side copies). So the sequence per trial is:
 *
 *     seed();  call(ref);  capture(0);
 *     seed();  call(new);  capture(1);   compare
 *
 * The re-seed between the two calls is what makes the comparison meaningful: it
 * puts the shared state back exactly as it was, so the only difference left is
 * what each side wrote.
 *
 * ---- width note (must be stated, not hidden) ----------------------------
 *
 * On C28x `int` and `unsigned int` are 16 bits; on the host they are 32. The
 * project's own headers are used here, so `int` stays 32-bit and `Uint` (from
 * MotorDefine.h) stays 32-bit -- only Uint16/int16/int32/Uint32 are pinned to
 * exact widths by shim/DSP2803x_Device.h. That is fine for a differential test:
 * both sides are compiled the same way, so every truncation point is the same on
 * both, and a difference between them is still a real difference. What it does
 * NOT license is *changing* a width during the rewrite -- the driver cannot see
 * a host-only equivalence. The rewrite below is renames + macro naming +
 * whitespace only, so no expression width moves.
 *
 * ---- what is deliberately not tested ------------------------------------
 *
 *   a. ParSend05Ms is empty. Rather than skip it, phase_empty() asserts the
 *      invariant it stands for: calling it changes nothing at all. A rewrite
 *      that "helpfully" filled it in would fail there.
 *   b. gRealTimeToFunctionDataBuff is declared, defined, and never referenced by
 *      any function. It is carried in the snapshot so that a mutant which starts
 *      writing it is caught, but no phase can distinguish ref from new through
 *      it otherwise.
 *   c. The 49 externals are compared as whole structs, so padding bytes are part
 *      of the comparison. That is intended: both sides start from the same seeded
 *      bytes, so equal padding means "neither side touched it", and a rewrite that
 *      began writing a previously untouched byte would be visible.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "mock_mde.h"

#define FAIL_LIMIT 20

/* ---- per-side exports ---------------------------------------------------- */

extern void ref_ParSend05Ms(void);
extern void ref_ParGet05Ms(void);
extern void ref_ParSend2Ms(void);
extern void ref_ParGet2Ms(void);
extern void ref_ParSendTune(void);

extern void new_ParSend05Ms(void);
extern void new_ParGet05Ms(void);
extern void new_ParSend2Ms(void);
extern void new_ParGet2Ms(void);
extern void new_ParSendTune(void);

#define MDE_BUFFER_DIMS(F) \
    F(gSendToMotor05MsDataBuff, 14)   \
    F(gSendToMotor2MsDataBuff, 115)   \
    F(gSendToFunctionDataBuff, 60)    \
    F(gRealTimeToFunctionDataBuff, 5) \
    F(gParaIdToFunctionDataBuff, 20)

#define DECL_REF(name, n) extern int ref_##name[n];
#define DECL_NEW(name, n) extern int new_##name[n];
MDE_BUFFER_DIMS(DECL_REF)
MDE_BUFFER_DIMS(DECL_NEW)

extern Uint ref_uReservedData, new_uReservedData;
extern Uint ref_gSoftVersion,  new_gSoftVersion;

/* ---- snapshot ------------------------------------------------------------ */

struct SNAP
{
#define DECL_BUFFER(name, n) int name[n];
    MDE_BUFFER_DIMS(DECL_BUFFER)
    Uint reserved;
    Uint softVersion;
#define DECL_GLOBAL(name, type) type name;
    MDE_EXTERN_GLOBALS(DECL_GLOBAL)
};

static struct SNAP snap[2];
static struct SNAP before;

/* ---- deterministic streams ---------------------------------------------- */

static uint32_t st_src, st_in, st_dst;

static uint32_t nxt(uint32_t *s)
{
    *s = (*s) * 1103515245u + 12345u;
    return (*s) >> 8;
}

static void fill_rand(void *p, size_t n, uint32_t *s)
{
    unsigned char *b = (unsigned char *)p;
    size_t i;

    for (i = 0; i < n; i++)
    {
        b[i] = (unsigned char)(nxt(s) & 0xFFu);
    }
}

static void fill_word(void *p, size_t n, uint32_t w)
{
    unsigned char *b = (unsigned char *)p;
    size_t i;

    for (i = 0; i < n; i++)
    {
        b[i] = (unsigned char)((w >> ((i % 4u) * 8u)) & 0xFFu);
    }
}

/* ref and new must be handed the SAME bytes; filling them from one advancing
 * stream would hand them different ones and make every comparison fail. */
static void fill_pair_rand(void *a, void *b, size_t n, uint32_t *s)
{
    uint32_t save = *s;

    fill_rand(a, n, s);
    *s = save;
    fill_rand(b, n, s);
}

static void fill_pair_word(void *a, void *b, size_t n, uint32_t w)
{
    fill_word(a, n, w);
    fill_word(b, n, w);
}

/* ---- seeding ------------------------------------------------------------- */

#define SEED_SRC(name, type) fill_rand(&name, sizeof(name), &st_src);

static void seed_inputs(void)
{
    MDE_EXTERN_GLOBALS(SEED_SRC)
}

/* ---- capture ------------------------------------------------------------- */

static void capture(int side, struct SNAP *s)
{
    if (side == 0)
    {
        memcpy(s->gSendToMotor05MsDataBuff,   ref_gSendToMotor05MsDataBuff,   sizeof(s->gSendToMotor05MsDataBuff));
        memcpy(s->gSendToMotor2MsDataBuff,   ref_gSendToMotor2MsDataBuff,   sizeof(s->gSendToMotor2MsDataBuff));
        memcpy(s->gSendToFunctionDataBuff,   ref_gSendToFunctionDataBuff,   sizeof(s->gSendToFunctionDataBuff));
        memcpy(s->gRealTimeToFunctionDataBuff, ref_gRealTimeToFunctionDataBuff, sizeof(s->gRealTimeToFunctionDataBuff));
        memcpy(s->gParaIdToFunctionDataBuff, ref_gParaIdToFunctionDataBuff, sizeof(s->gParaIdToFunctionDataBuff));
        s->reserved    = ref_uReservedData;
        s->softVersion = ref_gSoftVersion;
    }
    else
    {
        memcpy(s->gSendToMotor05MsDataBuff,   new_gSendToMotor05MsDataBuff,   sizeof(s->gSendToMotor05MsDataBuff));
        memcpy(s->gSendToMotor2MsDataBuff,   new_gSendToMotor2MsDataBuff,   sizeof(s->gSendToMotor2MsDataBuff));
        memcpy(s->gSendToFunctionDataBuff,   new_gSendToFunctionDataBuff,   sizeof(s->gSendToFunctionDataBuff));
        memcpy(s->gRealTimeToFunctionDataBuff, new_gRealTimeToFunctionDataBuff, sizeof(s->gRealTimeToFunctionDataBuff));
        memcpy(s->gParaIdToFunctionDataBuff, new_gParaIdToFunctionDataBuff, sizeof(s->gParaIdToFunctionDataBuff));
        s->reserved    = new_uReservedData;
        s->softVersion = new_gSoftVersion;
    }

    /* The 49 externals are shared, so both captures see the same storage -- but
     * ParGet05Ms / ParGet2Ms write them, so they are outputs, not constants. */
#define CAP_GLOBAL(name, type) memcpy(&s->name, &name, sizeof(s->name));
    MDE_EXTERN_GLOBALS(CAP_GLOBAL)
}

/* ---- comparison ---------------------------------------------------------- */

static int ncase, nfail, nprint;

static void report(const char *tag, const char *what,
                   const void *a, const void *b, size_t n)
{
    size_t i;
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;

    nfail++;
    if (nprint++ >= FAIL_LIMIT)
    {
        return;
    }

    for (i = 0; i < n; i++)
    {
        if (pa[i] != pb[i])
        {
            printf("MISMATCH %s: %s byte %u: ref=%02x new=%02x\n",
                   tag, what, (unsigned)i, pa[i], pb[i]);
            return;
        }
    }
    printf("MISMATCH %s: %s (memcmp said different, no byte differs)\n", tag, what);
}

#define CMP_BUF(name, n) \
    if (memcmp(a->name, b->name, sizeof(a->name)) != 0) \
        report(tag, #name, a->name, b->name, sizeof(a->name));
#define CMP_ONE(name, type) \
    if (memcmp(&a->name, &b->name, sizeof(a->name)) != 0) \
        report(tag, #name, &a->name, &b->name, sizeof(a->name));

static void cmp_snap(const struct SNAP *a, const struct SNAP *b, const char *tag)
{
    ncase++;

    MDE_BUFFER_DIMS(CMP_BUF)

    if (a->reserved != b->reserved)
        report(tag, "uReservedData", &a->reserved, &b->reserved, sizeof(a->reserved));
    if (a->softVersion != b->softVersion)
        report(tag, "gSoftVersion", &a->softVersion, &b->softVersion, sizeof(a->softVersion));

    MDE_EXTERN_GLOBALS(CMP_ONE)
}

/* ---- calling ------------------------------------------------------------- */

static void call_all(int side)
{
    if (side == 0)
    {
        ref_ParSend05Ms();
        ref_ParGet05Ms();
        ref_ParSend2Ms();
        ref_ParGet2Ms();
        ref_ParSendTune();
    }
    else
    {
        new_ParSend05Ms();
        new_ParGet05Ms();
        new_ParSend2Ms();
        new_ParGet2Ms();
        new_ParSendTune();
    }
}

static void call_one(int side, int which)
{
    if (side == 0)
    {
        switch (which)
        {
        case 0:  ref_ParSend05Ms(); break;
        case 1:  ref_ParGet05Ms();  break;
        case 2:  ref_ParSend2Ms();  break;
        case 3:  ref_ParGet2Ms();   break;
        default: ref_ParSendTune(); break;
        }
    }
    else
    {
        switch (which)
        {
        case 0:  new_ParSend05Ms(); break;
        case 1:  new_ParGet05Ms();  break;
        case 2:  new_ParSend2Ms();  break;
        case 3:  new_ParGet2Ms();   break;
        default: new_ParSendTune(); break;
        }
    }
}

static const char *WHICH_NAME[5] =
{
    "ParSend05Ms", "ParGet05Ms", "ParSend2Ms", "ParGet2Ms", "ParSendTune"
};

static const uint32_t BOUND[] =
{
    0x00000000u, 0xFFFFFFFFu, 0x0000FFFFu, 0xFFFF0000u,
    0x00008000u, 0x00007FFFu, 0x80000000u, 0x7FFFFFFFu,
    0x00000001u, 0x00000100u
};
#define NBOUND (sizeof(BOUND) / sizeof(BOUND[0]))

/* ---- the three streams for one trial ------------------------------------ */

static void seed_trial(uint32_t trial, int boundary, int which)
{
    /* stream 1: the 49 shared externals */
    st_src = 0x1000u + trial * 7919u;
    seed_inputs();

    /* stream 2: the two receive buffers -- random, or a boundary word */
    if (boundary)
    {
        uint32_t w = BOUND[trial % NBOUND];
        uint32_t v = BOUND[(trial / NBOUND) % NBOUND];

        fill_pair_word(ref_gSendToMotor05MsDataBuff, new_gSendToMotor05MsDataBuff,
                       sizeof(ref_gSendToMotor05MsDataBuff), w);
        fill_pair_word(ref_gSendToMotor2MsDataBuff,  new_gSendToMotor2MsDataBuff,
                       sizeof(ref_gSendToMotor2MsDataBuff),  v);
    }
    else
    {
        st_in = 0x2000u + trial * 104729u;
        fill_pair_rand(ref_gSendToMotor05MsDataBuff, new_gSendToMotor05MsDataBuff,
                       sizeof(ref_gSendToMotor05MsDataBuff), &st_in);
        fill_pair_rand(ref_gSendToMotor2MsDataBuff,  new_gSendToMotor2MsDataBuff,
                       sizeof(ref_gSendToMotor2MsDataBuff),  &st_in);
    }

    /* uReservedData / gSoftVersion are per-side but pure inputs: both sides must
     * be handed the same value. */
    {
        uint32_t s = st_src ^ (0x9E3779B9u + trial + (uint32_t)which);

        ref_uReservedData = (Uint)nxt(&s);
        new_uReservedData = ref_uReservedData;
        ref_gSoftVersion  = (Uint)nxt(&s);
        new_gSoftVersion  = ref_gSoftVersion;
    }

    /* stream 3: the three send buffers */
    st_dst = 0x3000u + trial * 15485863u;
    fill_pair_rand(ref_gSendToFunctionDataBuff, new_gSendToFunctionDataBuff,
                   sizeof(ref_gSendToFunctionDataBuff), &st_dst);
    fill_pair_rand(ref_gParaIdToFunctionDataBuff, new_gParaIdToFunctionDataBuff,
                   sizeof(ref_gParaIdToFunctionDataBuff), &st_dst);
    fill_pair_rand(ref_gRealTimeToFunctionDataBuff, new_gRealTimeToFunctionDataBuff,
                   sizeof(ref_gRealTimeToFunctionDataBuff), &st_dst);
}

/* ---- phases -------------------------------------------------------------- */

static void phase_empty(unsigned reps)
{
    unsigned t;
    int side;

    for (t = 0; t < reps; t++)
    {
        for (side = 0; side < 2; side++)
        {
            seed_trial(t, 0, 0);
            capture(side, &before);
            call_one(side, 0);              /* ParSend05Ms -- empty body */
            capture(side, &snap[side]);

            ncase++;
            if (memcmp(&before, &snap[side], sizeof(before)) != 0)
            {
                nfail++;
                if (nprint < FAIL_LIMIT)
                {
                    nprint++;
                    printf("MISMATCH empty-%s: ParSend05Ms modified state\n",
                           side ? "new" : "ref");
                }
            }
        }
    }
}

static void phase_random(unsigned reps)
{
    unsigned t;
    char tag[48];

    for (t = 0; t < reps; t++)
    {
        seed_trial(t, 0, 0);
        call_all(0);
        capture(0, &snap[0]);

        seed_trial(t, 0, 0);
        call_all(1);
        capture(1, &snap[1]);

        sprintf(tag, "random[%u]", t);
        cmp_snap(&snap[0], &snap[1], tag);
    }
}

static void phase_boundary(void)
{
    unsigned t;
    char tag[48];

    for (t = 0; t < NBOUND * NBOUND; t++)
    {
        seed_trial(t, 1, 0);
        call_all(0);
        capture(0, &snap[0]);

        seed_trial(t, 1, 0);
        call_all(1);
        capture(1, &snap[1]);

        sprintf(tag, "boundary[%u]", t);
        cmp_snap(&snap[0], &snap[1], tag);
    }
}

static void phase_single(unsigned reps)
{
    unsigned t;
    int which;
    char tag[64];

    for (which = 0; which < 5; which++)
    {
        for (t = 0; t < reps; t++)
        {
            seed_trial(t, 0, which);
            call_one(0, which);
            capture(0, &snap[0]);

            seed_trial(t, 0, which);
            call_one(1, which);
            capture(1, &snap[1]);

            sprintf(tag, "%s[%u]", WHICH_NAME[which], t);
            cmp_snap(&snap[0], &snap[1], tag);
        }
    }
}

int main(int argc, char **argv)
{
    unsigned reps = 20000;

    if (argc > 1)
    {
        reps = (unsigned)atoi(argv[1]);
    }
    if (reps == 0)
    {
        reps = 20000;
    }

    phase_empty(64);
    phase_boundary();
    phase_single(reps / 20 + 1);
    phase_random(reps);

    printf("cases=%d mismatches=%d\n", ncase, nfail);
    return nfail ? 1 : 0;
}
