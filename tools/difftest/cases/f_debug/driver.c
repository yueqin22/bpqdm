/* Differential test driver for f_debug.c.
 *
 * Feed the same inputs to the frozen baseline (ref) and the delivered source
 * (new), then compare every observable field of the module's state.
 *
 * ---------------------------------------------------------------------------
 * WHAT THIS MODULE ACTUALLY IS
 *
 * Nine externally linked entry points; eight of them are empty.  UniversalDebug32
 * / PowerOnDebug / InitKeyDebug / InitI2cDebug / ChgAttr / DealKeyDebug /
 * TestI2cDebug have their entire bodies inside `#if 0`, and InitDebug only calls
 * three of those empty functions.  On the target each of the eight compiles to a
 * bare LRETR, and cl2000 even deletes InitDebug's three calls.  Measured .text
 * is 0x66 = 102 words, of which UniversalDebug alone is 0x5F = 95.
 *
 * So the differential test is essentially about UniversalDebug, and the other
 * eight are covered by phase_empty: call them, assert the state does not move,
 * assert both sides agree.
 *
 * UniversalDebug is also NOT reachable from the shipped firmware: its only call
 * site (f_runSrc.c:928-936) is inside `#if DEBUG_F_POSITION_CTRL`, and
 * DEBUG_F_POSITION_CTRL is 0 because ZJ_MD380M is never defined.  Confirmed from
 * the object file, not from reading the macro: nm2000 on f_runSrc.obj lists no
 * undefined _UniversalDebug.  The bench therefore drives the function directly.
 * That is a statement about how the test reaches the code, not a weakening of
 * it -- the function's own semantics are what has to be preserved.
 *
 * ---------------------------------------------------------------------------
 * THE STATE MACHINE
 *
 *     if (sn >= 6) return;                              (guard)
 *     select pScope / scopeNum from sn                  (6-way switch)
 *     if (!runOld[sn] && run) {                         (rising edge)
 *         cnt[sn] = 0; scopeIndex[sn] = 0;
 *         for (j = scopeNum-1; j >= 0; j--) pScope[j] = 0;
 *     }
 *     runOld[sn] = run;
 *     if (!run) return;                                 (idle)
 *     if (++cnt[sn] >= scopeInterval[sn]) {             (cadence)
 *         cnt[sn] = 0;
 *         if (scopeIndex[sn] < scopeNum) {              (bounds)
 *             pScope[scopeIndex[sn]] = *matlabDebug[sn];
 *             if (++scopeIndex[sn] >= scopeNum) scopeIndex[sn] = 0;
 *         }
 *     }
 *
 * runOld[] and cnt[] are function-local statics: the driver cannot read or write
 * them.  That is fine and is handled by an induction argument -- both sides start
 * from zero, receive the identical call sequence, and are compared after EVERY
 * call.  Any divergence shows up on the call that causes it, before later calls
 * can mask it.  No attempt is made to force cnt[] to a chosen value; instead the
 * rising edge is made deterministic by prime(), which calls with the run bit
 * clear so that runOld[sn] becomes 0.
 *
 * scopeNum is 1 for all six channels (SCOPE_NUM_KK == 1 and scope4/scope5 are
 * literally 1), so two consequences are worth stating up front, because they
 * decide which branches exist at all:
 *
 *   - `if (scopeIndex[sn] < scopeNum)` is `index < 1`, i.e. only index == 0
 *     passes.  Everything else takes the "no store" path.
 *   - `if (++scopeIndex[sn] >= scopeNum)` can only be evaluated after index was
 *     0, so it is always `1 >= 1` -- TRUE.  The false arm is UNREACHABLE, and
 *     the follow-up `scopeIndex[sn] = 0` always fires.  The bench asserts the
 *     observable consequence instead of the unreachable branch: whenever a store
 *     happened, scopeIndex must read 0 afterwards (cov.wrapMiss stays 0).
 *
 * The `#elif 1` arm that would assign `scopeNum - 1` instead of 0 is therefore
 * dead AND equivalent under this configuration (scopeNum - 1 == 0), which is
 * recorded in the case README rather than tested.
 *
 * ---------------------------------------------------------------------------
 * INPUT SPACE
 *
 * Driven as inputs: sn (including out of range), runFlag.all (all 16 bits),
 * scopeInterval[6], scopeIndex[6], the six scope[] words, matlabDebugRsvd, and
 * frq / frqRun / frqTmp.
 *
 * scopeInterval has no writer anywhere in the project, so the shipped value is
 * {2,2,2,2,2,2}.  It is nevertheless driven over a wide set, because it is a
 * plain (non-static) global that the module treats as data; a rewrite that read
 * the wrong element or the wrong comparison would differ on those values even if
 * the firmware only ever presents 2.  Both are covered: the shipped 2 is swept
 * hard, and the extra values only make the test stronger.
 *
 * scopeIndex is likewise an input, and it is the only way to reach the
 * "no store" arm of the bounds test.
 *
 * ---------------------------------------------------------------------------
 * LOW-WORD TRUNCATION
 *
 * matlabDebug[] entries are `(Uint16 *)&frq` -- the module reads only the low
 * 16-bit word of a 32-bit object.  phase_source therefore uses values whose high
 * and low words differ, and bench_selfcheck asserts the premise directly
 * (frq = 0x12345678 must read back as 0x5678).  Without that, a rewrite that
 * read the high word, or that stored 32 bits, could pass unnoticed.
 */

#include <stdio.h>
#include <string.h>

/* Types first: the real f_debug.h spells its two prototypes with Uint16/Uint32
 * and has no includes of its own (it relies on the including TU having pulled
 * the type header in already). */
#include "DSP28x_Project.h"
#include "f_debug.h"
#include "f_runSrc.h"

/* atol is declared by hand rather than pulled from <stdlib.h>: this driver does
 * not include SubPrgInclude.h, but keeping <stdlib.h> out of case drivers is the
 * house rule (its `int abs(int)` collides with the project's
 * `extern short abs(short)` under the shim's `#define int short`). */
extern long atol(const char *);

/* ------------------------------------------------------------------ */
/* The two sides.  Every symbol the module defines externally is renamed */
/* ref_/new_ by ref_tu.c / new_tu.c; the list below mirrors that table.  */
/* ------------------------------------------------------------------ */

extern Uint16  ref_scope0[1];
extern Uint16  ref_scope1[1];
extern Uint16  ref_scope2[1];
extern Uint16  ref_scope3[1];
extern Uint16  ref_scope4[1];
extern Uint16  ref_scope5[1];
extern Uint16  ref_scopeIndex[6];
extern Uint16  ref_scopeInterval[6];
extern Uint16 *ref_matlabDebug[6];
extern Uint16  ref_matlabDebugRsvd;
extern Uint32  ref_vDL[1];
extern int16   ref_keyValueDebug[1];
extern int16   ref_keyValueDebugIndex;
extern Uint16  ref_I2CWrongTimer;
extern Uint16  ref_sendDataI2C[1];
extern Uint16  ref_rcvDataI2C[1];
extern int16   ref_i;
extern void    ref_UniversalDebug(Uint16 sn);
extern void    ref_UniversalDebug32(Uint32 a);
extern void    ref_PowerOnDebug(Uint16 v);
extern void    ref_InitDebug(void);
extern void    ref_InitKeyDebug(void);
extern void    ref_DealKeyDebug(void);
extern void    ref_InitI2cDebug(void);
extern void    ref_TestI2cDebug(void);
extern void    ref_ChgAttr(void);

extern Uint16  new_scope0[1];
extern Uint16  new_scope1[1];
extern Uint16  new_scope2[1];
extern Uint16  new_scope3[1];
extern Uint16  new_scope4[1];
extern Uint16  new_scope5[1];
extern Uint16  new_scopeIndex[6];
extern Uint16  new_scopeInterval[6];
extern Uint16 *new_matlabDebug[6];
extern Uint16  new_matlabDebugRsvd;
extern Uint32  new_vDL[1];
extern int16   new_keyValueDebug[1];
extern int16   new_keyValueDebugIndex;
extern Uint16  new_I2CWrongTimer;
extern Uint16  new_sendDataI2C[1];
extern Uint16  new_rcvDataI2C[1];
extern int16   new_i;
extern void    new_UniversalDebug(Uint16 sn);
extern void    new_UniversalDebug32(Uint32 a);
extern void    new_PowerOnDebug(Uint16 v);
extern void    new_InitDebug(void);
extern void    new_InitKeyDebug(void);
extern void    new_DealKeyDebug(void);
extern void    new_InitI2cDebug(void);
extern void    new_TestI2cDebug(void);
extern void    new_ChgAttr(void);

/* frq / frqRun / frqTmp / runFlag are extern in the module, so they are NOT
 * renamed and both sides bind to the single copy in mock_debug.c. */
extern int32 frq;
extern int32 frqRun;
extern int32 frqTmp;

/* ------------------------------------------------------------------ */
/* Observable state                                                     */
/* ------------------------------------------------------------------ */

/* matlabDebug[] is compared in normalised form: entry k is replaced by which
 * canonical object it points at.  Raw pointers cannot be compared across sides
 * because entries 3..5 point at matlabDebugRsvd, which IS per-side. */
#define MDB_FRQ   0u
#define MDB_FRQRN 1u
#define MDB_FRQTP 2u
#define MDB_RSVD  3u
#define MDB_OTHER 0xFFu

typedef struct {
    Uint16 scope[6];
    Uint16 index[6];
    Uint16 interval[6];
    Uint16 rsvd;
    Uint16 mdb[6];
    Uint32 vdl;
    int16  kv[1];
    int16  kvIdx;
    Uint16 i2cWrong;
    Uint16 send[1];
    Uint16 rcv[1];
    int16  i;
} STATE;

static long g_cases = 0;
static long g_fail  = 0;

/* Coverage / reachability counters.  Every one of these is asserted at the end
 * of main(), so "the interesting branch was exercised" is a checked fact rather
 * than an intention. */
typedef struct {
    long guardHit;      /* sn >= SCOPE_NUMBER, early return              */
    long idleHit;       /* run bit clear, second early return            */
    long edgeHit;       /* !runOld && run, the clear-the-array path      */
    long storeHit;      /* the channel word was actually written         */
    long storeMiss;     /* bounds test rejected the store                */
    long wrapHit;       /* store happened and index came back to 0       */
    long wrapMiss;      /* store happened but index did NOT come back    */
    long snUsed[6];     /* per-channel call count                        */
} COV;

static COV cov;

/* Which bit of runFlag.all does `bit.run` land on?  Measured in
 * bench_selfcheck(), not assumed: the C28x and MSVC need not agree on bit-field
 * allocation order, and this driver drives runFlag.all.  The module only ever
 * tests `runFlag.bit.run` for zero/non-zero, so all that has to be faithful is
 * this one mapping. */
static Uint16 g_runBit = 1u;

/* ------------------------------------------------------------------ */
/* Snapshot                                                             */
/* ------------------------------------------------------------------ */

static Uint16 norm_mdb(const Uint16 *p, const Uint16 *rsvd)
{
    if (p == (const Uint16 *)(const void *)&frq)    return MDB_FRQ;
    if (p == (const Uint16 *)(const void *)&frqRun) return MDB_FRQRN;
    if (p == (const Uint16 *)(const void *)&frqTmp) return MDB_FRQTP;
    if (p == rsvd)                                  return MDB_RSVD;
    return MDB_OTHER;
}

static void snap_ref(STATE *s)
{
    int k;
    memset(s, 0, sizeof *s);
    s->scope[0] = ref_scope0[0];
    s->scope[1] = ref_scope1[0];
    s->scope[2] = ref_scope2[0];
    s->scope[3] = ref_scope3[0];
    s->scope[4] = ref_scope4[0];
    s->scope[5] = ref_scope5[0];
    for (k = 0; k < 6; k++) {
        s->index[k]    = ref_scopeIndex[k];
        s->interval[k] = ref_scopeInterval[k];
        s->mdb[k]      = norm_mdb(ref_matlabDebug[k], &ref_matlabDebugRsvd);
    }
    s->rsvd     = ref_matlabDebugRsvd;
    s->vdl      = ref_vDL[0];
    s->kv[0]    = ref_keyValueDebug[0];
    s->kvIdx    = ref_keyValueDebugIndex;
    s->i2cWrong = ref_I2CWrongTimer;
    s->send[0]  = ref_sendDataI2C[0];
    s->rcv[0]   = ref_rcvDataI2C[0];
    s->i        = ref_i;
}

static void snap_new(STATE *s)
{
    int k;
    memset(s, 0, sizeof *s);
    s->scope[0] = new_scope0[0];
    s->scope[1] = new_scope1[0];
    s->scope[2] = new_scope2[0];
    s->scope[3] = new_scope3[0];
    s->scope[4] = new_scope4[0];
    s->scope[5] = new_scope5[0];
    for (k = 0; k < 6; k++) {
        s->index[k]    = new_scopeIndex[k];
        s->interval[k] = new_scopeInterval[k];
        s->mdb[k]      = norm_mdb(new_matlabDebug[k], &new_matlabDebugRsvd);
    }
    s->rsvd     = new_matlabDebugRsvd;
    s->vdl      = new_vDL[0];
    s->kv[0]    = new_keyValueDebug[0];
    s->kvIdx    = new_keyValueDebugIndex;
    s->i2cWrong = new_I2CWrongTimer;
    s->send[0]  = new_sendDataI2C[0];
    s->rcv[0]   = new_rcvDataI2C[0];
    s->i        = new_i;
}

/* ------------------------------------------------------------------ */
/* Input description                                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    Uint16 runAll;      /* value written to runFlag.all                  */
    Uint16 interval[6];
    Uint16 index[6];
    Uint16 scope[6];
    Uint16 rsvd;
    int32  src[3];      /* frq, frqRun, frqTmp                           */
} SETUP;

static void apply(const SETUP *s)
{
    int k;

    runFlag.all = s->runAll;
    frq    = s->src[0];
    frqRun = s->src[1];
    frqTmp = s->src[2];

    for (k = 0; k < 6; k++) {
        ref_scopeIndex[k] = s->index[k];
        new_scopeIndex[k] = s->index[k];
        ref_scopeInterval[k] = s->interval[k];
        new_scopeInterval[k] = s->interval[k];
    }
    ref_scope0[0] = s->scope[0];
    new_scope0[0] = s->scope[0];
    ref_scope1[0] = s->scope[1];
    new_scope1[0] = s->scope[1];
    ref_scope2[0] = s->scope[2];
    new_scope2[0] = s->scope[2];
    ref_scope3[0] = s->scope[3];
    new_scope3[0] = s->scope[3];
    ref_scope4[0] = s->scope[4];
    new_scope4[0] = s->scope[4];
    ref_scope5[0] = s->scope[5];
    new_scope5[0] = s->scope[5];

    ref_matlabDebugRsvd = s->rsvd;
    new_matlabDebugRsvd = s->rsvd;
}

static void setup_defaults(SETUP *s)
{
    int k;
    memset(s, 0, sizeof *s);
    for (k = 0; k < 6; k++) s->interval[k] = 2;   /* shipped value */
    s->src[0] = 0x11112222L;
    s->src[1] = 0x33334444L;
    s->src[2] = 0x55556666L;
    s->rsvd   = 0x7777;
}

static void dump(const char *tag, const SETUP *s, Uint16 sn, const STATE *st)
{
    printf("  %s: sn=%u runAll=0x%04X rsvd=0x%04X\n",
           tag, (unsigned)sn, (unsigned)s->runAll, (unsigned)s->rsvd);
    printf("      scope   = %04X %04X %04X %04X %04X %04X\n",
           st->scope[0], st->scope[1], st->scope[2],
           st->scope[3], st->scope[4], st->scope[5]);
    printf("      index   = %u %u %u %u %u %u\n",
           (unsigned)st->index[0], (unsigned)st->index[1], (unsigned)st->index[2],
           (unsigned)st->index[3], (unsigned)st->index[4], (unsigned)st->index[5]);
    printf("      intervl = %u %u %u %u %u %u\n",
           (unsigned)st->interval[0], (unsigned)st->interval[1], (unsigned)st->interval[2],
           (unsigned)st->interval[3], (unsigned)st->interval[4], (unsigned)st->interval[5]);
    printf("      mdb     = %02X %02X %02X %02X %02X %02X  vdl=%08lX kvIdx=%d i=%d\n",
           st->mdb[0], st->mdb[1], st->mdb[2], st->mdb[3], st->mdb[4], st->mdb[5],
           (unsigned long)st->vdl, (int)st->kvIdx, (int)st->i);
    printf("      other   = i2cWrong=%u send=%u rcv=%u kv=%d\n",
           (unsigned)st->i2cWrong, (unsigned)st->send[0], (unsigned)st->rcv[0],
           (int)st->kv[0]);
}

/* Compare both sides and report.  Returns 1 when they agree. */
static int compare(const SETUP *s, Uint16 sn, const char *op)
{
    STATE rs, ns;

    snap_ref(&rs);
    snap_new(&ns);
    g_cases++;

    if (memcmp(&rs, &ns, sizeof rs) == 0) return 1;

    g_fail++;
    if (g_fail <= 20) {
        printf("MISMATCH op=%s\n", op);
        dump("ref", s, sn, &rs);
        dump("new", s, sn, &ns);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Calling helpers                                                      */
/* ------------------------------------------------------------------ */

/* Make runOld[sn] == 0 without disturbing anything else the module tracks.
 *
 * A call with the run bit clear executes `if ((!runOld[sn]) && run)` -> false,
 * then `runOld[sn] = 0`, then returns at the idle test.  So runOld[sn] is 0
 * afterwards and cnt[sn] is untouched.  The next call with the run bit set is
 * therefore a guaranteed rising edge.  Both sides get the same prime call, so
 * the induction argument still holds. */
static void prime(Uint16 sn)
{
    runFlag.all = 0;
    ref_UniversalDebug(sn);
    new_UniversalDebug(sn);
}

/* Apply the setup, optionally force a rising edge, then make `ncall` calls,
 * comparing after every one.  Coverage is read off the ref side (the oracle)
 * between consecutive calls. */
static void seq(const SETUP *s, Uint16 sn, int ncall, int do_prime)
{
    STATE before, after;
    int i;
    int run;

    apply(s);
    if (do_prime) {
        prime(sn);
        runFlag.all = s->runAll;   /* prime left it at 0 */
    }

    for (i = 0; i < ncall; i++) {
        snap_ref(&before);
        run = (s->runAll & g_runBit) != 0;

        ref_UniversalDebug(sn);
        new_UniversalDebug(sn);

        if (!compare(s, sn, "UniversalDebug")) return;

        snap_ref(&after);

        if (sn >= 6) {
            cov.guardHit++;
            continue;
        }
        cov.snUsed[sn]++;

        if (!run) cov.idleHit++;

        if (after.scope[sn] != before.scope[sn]) {
            cov.storeHit++;
            if (after.index[sn] == 0) cov.wrapHit++;
            else                      cov.wrapMiss++;
        }
        /* The "bounds test rejected the store" arm is NOT inferred here: from
         * the outside it is indistinguishable from "the cadence did not fire".
         * phase_index arranges the one situation where the difference is
         * knowable and counts it there. */

        if (run && before.scope[sn] != 0 && after.scope[sn] == 0) cov.edgeHit++;
    }
}

/* ------------------------------------------------------------------ */
/* Random numbers: reproducible xorshift, not rand()                    */
/* ------------------------------------------------------------------ */

static unsigned long long g_rng = 0x9E3779B97F4A7C15ULL;

static Uint32 rnd32(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return (Uint32)(g_rng >> 16);
}

static Uint16 rnd16(void) { return (Uint16)rnd32(); }

/* ------------------------------------------------------------------ */
/* Phases                                                               */
/* ------------------------------------------------------------------ */

/* sn out of range: nothing at all may change. */
static void phase_guard(void)
{
    static const Uint16 bad[] = { 6u, 7u, 8u, 11u, 12u, 100u, 255u, 256u,
                                  1000u, 32767u, 32768u, 65534u, 65535u };
    SETUP s;
    STATE before, after;
    int i;

    for (i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) {
        setup_defaults(&s);
        s.runAll = g_runBit;          /* running, so only the guard can stop it */
        s.interval[0] = 1;
        s.index[0] = 0;
        s.scope[0] = 0x5A5A;
        apply(&s);
        snap_ref(&before);
        ref_UniversalDebug(bad[i]);
        new_UniversalDebug(bad[i]);
        if (!compare(&s, bad[i], "guard")) return;
        snap_ref(&after);
        if (memcmp(&before, &after, sizeof before) != 0) {
            g_fail++;
            printf("MISMATCH op=guard-changed-state sn=%u\n", (unsigned)bad[i]);
        }
        cov.guardHit++;
    }
}

/* The six channels are six separate one-word arrays: writing channel k must not
 * touch channel j, and the rising edge must clear exactly the selected one.
 * Several dirty patterns, because "cleared" is only visible against a value that
 * is neither 0 nor the sampled one. */
static void phase_edge(void)
{
    static const Uint16 dirty[4][6] = {
        { 0xAAAA, 0xBBBB, 0xCCCC, 0xDDDD, 0xEEEE, 0xFFFF },
        { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF },
        { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
        { 0x1111, 0x0000, 0x2222, 0x0000, 0x3333, 0x0000 }
    };
    SETUP s;
    Uint16 sn;
    int pass, k;

    for (pass = 0; pass < 4; pass++) {
        for (sn = 0; sn < 6; sn++) {
            setup_defaults(&s);
            s.runAll = g_runBit;
            for (k = 0; k < 6; k++) s.scope[k] = dirty[pass][k];
            s.interval[sn] = 2;   /* shipped value: the edge call clears but does
                                   * not store; the call after it stores       */
            s.index[sn] = 0;
            seq(&s, sn, 4, 1);    /* prime + edge + 3 more */
        }
    }
}

/* Cadence: how often the sample is taken. */
static void phase_cadence(void)
{
    static const Uint16 iv[] = { 0u, 1u, 2u, 3u, 4u, 5u, 8u, 16u, 100u,
                                 255u, 256u, 1000u, 32767u, 65534u, 65535u };
    SETUP s;
    int i, n;
    Uint16 sn;

    for (i = 0; i < (int)(sizeof iv / sizeof iv[0]); i++) {
        for (sn = 0; sn < 6; sn++) {
            for (n = 1; n <= 6; n++) {
                setup_defaults(&s);
                s.runAll = g_runBit;
                s.interval[sn] = iv[i];
                s.index[sn] = 0;
                seq(&s, sn, n, 1);
            }
        }
    }
}

/* Bounds test: only index 0 can pass when scopeNum == 1.
 *
 * The rising edge RESETS scopeIndex to 0, so an initial index cannot survive
 * into the cadence check -- a first attempt that set index before the edge saw
 * the bounds-rejection arm zero times, which is what the reachability gate is
 * for.  The index therefore has to be installed AFTER the edge, which is done
 * explicitly here.  That is also why storeMiss is counted here and not in seq():
 * only this phase can predict that the cadence fired while the bounds test
 * rejected the store. */
static void phase_index(void)
{
    static const Uint16 ix[] = { 0u, 1u, 2u, 3u, 5u, 6u, 100u, 255u,
                                 256u, 32767u, 65534u, 65535u };
    SETUP s;
    int i, n;
    Uint16 sn;

    for (i = 0; i < (int)(sizeof ix / sizeof ix[0]); i++) {
        for (sn = 0; sn < 6; sn++) {
            STATE before, after;

            setup_defaults(&s);
            s.runAll = g_runBit;
            s.interval[sn] = 1;   /* the cadence fires on every call */
            s.index[sn] = 0;
            s.scope[sn] = 0;
            apply(&s);

            /* Force a rising edge and let the module settle: after this call
             * cnt is back to 0 and scopeIndex is 0. */
            prime(sn);
            runFlag.all = s.runAll;
            ref_UniversalDebug(sn);
            new_UniversalDebug(sn);
            if (!compare(&s, sn, "index-edge")) return;

            /* Install the index under test, with the edge behind us. */
            ref_scopeIndex[sn] = ix[i];
            new_scopeIndex[sn] = ix[i];

            for (n = 1; n <= 4; n++) {
                snap_ref(&before);
                ref_UniversalDebug(sn);
                new_UniversalDebug(sn);
                if (!compare(&s, sn, "index")) return;
                snap_ref(&after);

                cov.snUsed[sn]++;
                if (after.scope[sn] != before.scope[sn]) {
                    cov.storeHit++;
                    if (after.index[sn] == 0) cov.wrapHit++;
                    else                      cov.wrapMiss++;
                } else if (ix[i] != 0 &&
                           after.index[sn] == before.index[sn]) {
                    cov.storeMiss++;
                }
            }
        }
    }
}

/* Which object is sampled, and how many bits of it survive.  Every source
 * pattern has high word != low word, so reading the high word or storing 32
 * bits would be visible. */
static void phase_source(void)
{
    SETUP s;
    Uint16 sn;
    int i;

    for (sn = 0; sn < 6; sn++) {
        setup_defaults(&s);
        s.runAll = g_runBit;
        s.interval[sn] = 1;
        s.index[sn] = 0;
        s.scope[sn] = 0;
        s.src[0] = 0x12345678L;
        s.src[1] = 0x9ABCDEF0L;
        s.src[2] = 0x0F1E2D3CL;
        s.rsvd   = 0xC3A5;
        seq(&s, sn, 3, 1);
    }

    /* Every 32-bit pattern: 0xA5A5xxxx, so the low word is always nonzero and
     * never equal to the high word. */
    for (i = 0; i < 16; i++) {
        setup_defaults(&s);
        s.runAll = g_runBit;
        s.interval[0] = 1;
        s.index[0] = 0;
        s.scope[0] = 0;
        s.src[0] = (int32)(0xA5A50000u | (Uint32)((unsigned)i * 0x1111u));
        seq(&s, 0, 2, 1);
    }
}

/* All 16 bits of runFlag, exhaustively.  Only one of them may matter. */
static void phase_runbits(void)
{
    SETUP s;
    Uint32 v;
    Uint16 sn;

    for (v = 0; v <= 0xFFFFu; v++) {
        sn = (Uint16)(v % 6u);
        setup_defaults(&s);
        s.runAll = (Uint16)v;
        s.interval[sn] = 1;
        s.index[sn] = 0;
        seq(&s, sn, 1, 0);
    }
}

/* The eight empty entry points.  They must not move anything, on either side. */
static void phase_empty(void)
{
    SETUP s;
    STATE before, after;
    int i;

    for (i = 0; i < 3; i++) {
        setup_defaults(&s);
        s.runAll = (Uint16)((i == 0) ? 0u : ((i == 1) ? g_runBit : 0xFFFFu));
        s.scope[0] = 0x1234;
        s.scope[3] = 0x5678;
        s.index[2] = 1;
        apply(&s);
        snap_ref(&before);

        ref_UniversalDebug32(0x12345678UL);
        ref_PowerOnDebug(0x1234);
        ref_InitDebug();
        ref_InitKeyDebug();
        ref_DealKeyDebug();
        ref_InitI2cDebug();
        ref_TestI2cDebug();
        ref_ChgAttr();

        new_UniversalDebug32(0x12345678UL);
        new_PowerOnDebug(0x1234);
        new_InitDebug();
        new_InitKeyDebug();
        new_DealKeyDebug();
        new_InitI2cDebug();
        new_TestI2cDebug();
        new_ChgAttr();

        if (!compare(&s, 0, "empty-entry-points")) return;
        snap_ref(&after);
        if (memcmp(&before, &after, sizeof before) != 0) {
            g_fail++;
            printf("MISMATCH op=empty-entry-points-changed-state pass=%d\n", i);
        }
    }
}

/* Randomised sequences: the run bit toggles mid-sequence, so edges and idle
 * returns interleave with the cadence counter, which is where a rewrite is most
 * likely to drift. */
static void phase_seq(long n)
{
    SETUP s;
    long i;
    int k;

    for (i = 0; i < n; i++) {
        setup_defaults(&s);
        for (k = 0; k < 6; k++) {
            s.interval[k] = (rnd32() & 3u) ? (Uint16)(rnd32() & 7u) : rnd16();
            s.index[k]    = (rnd32() & 1u) ? 0u : rnd16();
            s.scope[k]    = rnd16();
        }
        s.rsvd   = rnd16();
        s.src[0] = (int32)rnd32();
        s.src[1] = (int32)rnd32();
        s.src[2] = (int32)rnd32();
        /* start idle, so the first running call is a guaranteed edge */
        s.runAll = 0;

        apply(&s);
        {
            int ncall = (int)(rnd32() % 24u) + 1;
            int c;
            for (c = 0; c < ncall; c++) {
                Uint16 sn;
                if (rnd32() & 1u) {
                    runFlag.all = (Uint16)(runFlag.all ^ g_runBit);
                }
                sn = (Uint16)(rnd32() % 7u);   /* 6 is out of range */
                ref_UniversalDebug(sn);
                new_UniversalDebug(sn);
                if (!compare(&s, sn, "seq")) return;
            }
        }
    }
}

/* Random single calls with a fully random setup, including out-of-range sn. */
static void phase_random(long n)
{
    SETUP s;
    long i;
    int k;
    Uint16 sn;

    for (i = 0; i < n; i++) {
        setup_defaults(&s);
        for (k = 0; k < 6; k++) {
            s.interval[k] = rnd16();
            s.index[k]    = rnd16();
            s.scope[k]    = rnd16();
        }
        s.rsvd   = rnd16();
        s.src[0] = (int32)rnd32();
        s.src[1] = (int32)rnd32();
        s.src[2] = (int32)rnd32();
        s.runAll = rnd16();
        sn = (Uint16)(rnd32() % 9u);          /* 6,7,8 are out of range */

        apply(&s);
        ref_UniversalDebug(sn);
        new_UniversalDebug(sn);
        if (!compare(&s, sn, "random")) return;
    }
}

/* ------------------------------------------------------------------ */
/* Bench self-check                                                     */
/* ------------------------------------------------------------------ */

/* These check the BENCH, not the module.  If any of them fails the differential
 * result means nothing, so they run first and abort the run.
 * Returns 1 when everything holds. */
static int bench_selfcheck(void)
{
    int k;

    if (sizeof (Uint16) != 2 || sizeof (Uint32) != 4 ||
        sizeof (int16) != 2 || sizeof (int32) != 4) {
        printf("FATAL: host widths are wrong (Uint16=%u Uint32=%u int16=%u int32=%u)\n",
               (unsigned)sizeof (Uint16), (unsigned)sizeof (Uint32),
               (unsigned)sizeof (int16), (unsigned)sizeof (int32));
        return 0;
    }

    /* Measure which bit `runFlag.bit.run` lands on. */
    runFlag.all = 0;
    runFlag.bit.run = 1;
    if (runFlag.all == 0 || (runFlag.all & (Uint16)(runFlag.all - 1u)) != 0) {
        printf("FATAL: bit.run is not a single bit (runFlag.all=0x%04X)\n",
               (unsigned)runFlag.all);
        return 0;
    }
    g_runBit = runFlag.all;
    runFlag.all = 0;

    /* The matlabDebug[] table must point where the module's own initialiser
     * says.  A rewrite that repointed it would make the rest of the test
     * compare two different sources. */
    if (norm_mdb(ref_matlabDebug[0], &ref_matlabDebugRsvd) != MDB_FRQ ||
        norm_mdb(ref_matlabDebug[1], &ref_matlabDebugRsvd) != MDB_FRQRN ||
        norm_mdb(ref_matlabDebug[2], &ref_matlabDebugRsvd) != MDB_FRQTP ||
        norm_mdb(ref_matlabDebug[3], &ref_matlabDebugRsvd) != MDB_RSVD ||
        norm_mdb(ref_matlabDebug[4], &ref_matlabDebugRsvd) != MDB_RSVD ||
        norm_mdb(ref_matlabDebug[5], &ref_matlabDebugRsvd) != MDB_RSVD) {
        printf("FATAL: ref matlabDebug[] does not match the source initialiser\n");
        return 0;
    }
    if (norm_mdb(new_matlabDebug[0], &new_matlabDebugRsvd) != MDB_FRQ ||
        norm_mdb(new_matlabDebug[1], &new_matlabDebugRsvd) != MDB_FRQRN ||
        norm_mdb(new_matlabDebug[2], &new_matlabDebugRsvd) != MDB_FRQTP ||
        norm_mdb(new_matlabDebug[3], &new_matlabDebugRsvd) != MDB_RSVD ||
        norm_mdb(new_matlabDebug[4], &new_matlabDebugRsvd) != MDB_RSVD ||
        norm_mdb(new_matlabDebug[5], &new_matlabDebugRsvd) != MDB_RSVD) {
        printf("FATAL: new matlabDebug[] does not match the source initialiser\n");
        return 0;
    }

    /* The low-word premise: (Uint16 *)&frq must read the LOW half.  This is what
     * makes "the module samples 16 of the 32 bits" a fact on this host. */
    frq = 0x12345678L;
    if (*ref_matlabDebug[0] != 0x5678u || *new_matlabDebug[0] != 0x5678u) {
        printf("FATAL: (Uint16 *)&frq does not read the low word (got 0x%04X/0x%04X)\n",
               (unsigned)*ref_matlabDebug[0], (unsigned)*new_matlabDebug[0]);
        return 0;
    }
    frq = 0;

    /* Shipped configuration: scopeInterval is {2,2,2,2,2,2}.  If that ever
     * changed, the coverage claims in this file would describe a different
     * program. */
    for (k = 0; k < 6; k++) {
        if (ref_scopeInterval[k] != 2u || new_scopeInterval[k] != 2u) {
            printf("FATAL: scopeInterval[%d] is %u/%u, expected the shipped 2\n",
                   k, (unsigned)ref_scopeInterval[k],
                   (unsigned)new_scopeInterval[k]);
            return 0;
        }
    }

    /* A channel write must be confined to that channel, and must carry the value
     * of the object THAT channel samples.  Channel 3 samples matlabDebugRsvd
     * (matlabDebug[3] points at the reserved word, not at frq -- the table is
     * {frq, frqRun, frqTmp, rsvd, rsvd, rsvd}), so the expected value is rsvd.
     * Getting this wrong the first time is exactly why the check exists: an
     * assumption about which source a channel reads is not evidence. */
    {
        SETUP s;
        STATE after;
        setup_defaults(&s);
        s.runAll = g_runBit;
        s.interval[3] = 1;
        s.index[3] = 0;
        s.scope[0] = 0x0101;
        s.scope[1] = 0x0202;
        s.scope[2] = 0x0303;
        s.scope[3] = 0x0404;
        s.scope[4] = 0x0505;
        s.scope[5] = 0x0606;
        s.rsvd = 0xBBBB;
        seq(&s, 3, 2, 1);
        snap_ref(&after);
        if (after.scope[0] != 0x0101 || after.scope[1] != 0x0202 ||
            after.scope[2] != 0x0303 || after.scope[4] != 0x0505 ||
            after.scope[5] != 0x0606) {
            printf("FATAL: a store on channel 3 disturbed another channel "
                   "(%04X %04X %04X %04X %04X %04X)\n",
                   after.scope[0], after.scope[1], after.scope[2],
                   after.scope[3], after.scope[4], after.scope[5]);
            return 0;
        }
        if (after.scope[3] != 0xBBBBu) {
            printf("FATAL: channel 3 did not receive matlabDebugRsvd "
                   "(got 0x%04X, expected 0xBBBB)\n", after.scope[3]);
            return 0;
        }
    }

    return 1;
}

/* ------------------------------------------------------------------ */
/* Main                                                                 */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 20000L;

    if (!bench_selfcheck()) {
        printf("SELFCHECK FAILED\n");
        return 1;
    }

    phase_guard();
    phase_empty();
    phase_edge();
    phase_cadence();
    phase_index();
    phase_source();
    phase_runbits();
    phase_seq(n / 4 + 1);
    phase_random(n);

    printf("f_debug difftest: cases=%ld mismatches=%ld\n", g_cases, g_fail);
    printf("  reach: guard=%ld idle=%ld edge=%ld store=%ld storeMiss=%ld "
           "wrap=%ld wrapMiss=%ld\n",
           cov.guardHit, cov.idleHit, cov.edgeHit, cov.storeHit,
           cov.storeMiss, cov.wrapHit, cov.wrapMiss);
    printf("  per-channel calls: %ld %ld %ld %ld %ld %ld\n",
           cov.snUsed[0], cov.snUsed[1], cov.snUsed[2],
           cov.snUsed[3], cov.snUsed[4], cov.snUsed[5]);

    /* Reachability is part of the result: a green diff over a branch that was
     * never taken proves nothing. */
    if (cov.guardHit == 0 || cov.idleHit == 0 || cov.edgeHit == 0 ||
        cov.storeHit == 0 || cov.storeMiss == 0 || cov.wrapHit == 0) {
        printf("REACH: a required branch was never exercised\n");
        return 1;
    }
    if (cov.wrapMiss != 0) {
        printf("REACH: a store left scopeIndex != 0 -- the scopeNum == 1 "
               "premise is wrong\n");
        return 1;
    }
    if (cov.snUsed[0] == 0 || cov.snUsed[1] == 0 || cov.snUsed[2] == 0 ||
        cov.snUsed[3] == 0 || cov.snUsed[4] == 0 || cov.snUsed[5] == 0) {
        printf("REACH: a channel was never exercised\n");
        return 1;
    }

    return g_fail ? 1 : 0;
}
