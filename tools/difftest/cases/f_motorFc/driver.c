/* Differential test driver for f_motorFc.UpdateMotorPara.
 *
 * Unlike the f_p2p / zeroPosCtl cases, this module does not own its state: it
 * reads funcCode / diFunc / diSelectFunc / errorCode and writes the single
 * shared global motorFc. So the shape is load -> run ref -> capture -> load
 * again -> run new -> capture -> compare, where load() re-establishes the SAME
 * input state in front of each side.
 *
 * What is compared:
 *   - every byte of motorFc            (the module's output object)
 *   - motorSn                          (the module assigns it)
 *   - polePair                         (one per side, so they cannot mask each other)
 *   - every byte of funcCode           (the module must NOT write it; a stray
 *                                       write would otherwise go unnoticed)
 *
 * motorFc is poisoned with 0xA5 before each run. The M1 branch only writes 7 of
 * the struct's fields (tuneCmd and rsvdA21 are left alone), so the poison is
 * what proves the two sides write *the same subset* -- not merely that they end
 * up equal because both happened to touch everything.
 *
 * ------------------------------------------------------------------ preconditions
 *
 * The original code has two inputs it uses without checking. Neither is a bug
 * the rewrite introduces, and neither is fixed by the rewrite (behaviour must
 * stay identical), so the driver pins both down and asserts them:
 *
 *  1. `ratingSpeed` is a divisor. Zero would be UB on both sides. Every
 *     reachable source of it is forced non-zero by
 *     ensure_rating_speed_nonzero(), and assert_preconditions() verifies it.
 *
 *  2. `motorSn` indexes a THREE-element table:
 *         motorSnIndexNIncToMotor2[motorSn - MOTOR_SN_2]
 *     motorSn is either the 2-bit diFunc field (always 0..3) or
 *     funcCode.code.motorSn, which is an EEPROM-backed configuration value.
 *     The module range-checks neither, so a stored value > MOTOR_SN_4 walks off
 *     the end of the table and reads a wild address. In range, MOTOR_SN_1 takes
 *     the if-branch and MOTOR_SN_2..4 give index 0..2. The driver therefore
 *     keeps funcCode.code.motorSn within the enum and asserts it -- an
 *     out-of-range value is a pre-existing firmware hazard, not something this
 *     differential test can compare.
 *
 *  3. `decNumber[frqPoint]` indexes a 5-element table, so frqPoint is kept in
 *     0..4; decNumber is non-zero for all five, so the second divisor is safe.
 *
 * ------------------------------------------------------------------ layout
 *
 * The host inflates `Uint16 all[sizeof(struct X)]` (3 of them in f_funcCode.h)
 * because sizeof counts 8-bit bytes there and 16-bit address units on C28x.
 * Both sides of this test share the host layout, so the comparison is
 * unaffected; the affected quantity is *which* funcCode.all[] element gets
 * indexed. tools/difftest/target_layout.py measures that gap.
 *
 * The module's own layout table is a static initializer in the source; MSVC's C
 * front end cannot fold `&a.m1 - &a.m2` (error C2099), so the host copy drops the
 * initializer and init_tables() fills it at run time. See host_patch.json.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "f_main.h"
#include "f_io.h"
#include "f_funcCode.h"

/* Provided by ref_tu.c / new_tu.c (the module's own file-scope symbols). */
extern Uint16 ref_polePair;
extern Uint16 new_polePair;
extern void   ref_UpdateMotorPara(void);
extern void   new_UpdateMotorPara(void);

/* The module's layout table. In the source it is a static initializer; on the
 * host it is filled by init_tables() below -- see host_patch.json. */
extern Uint16 ref_motorSnIndexNIncToMotor2[3];
extern Uint16 new_motorSnIndexNIncToMotor2[3];

/* Shared input/output globals, defined once in mock_motorfc.c. */
extern FUNCCODE_ALL     funcCode;
extern struct MOTOR_FC  motorFc;
extern enum MOTOR_SN    motorSn;
extern Uint16           errorCode;
extern struct DI_FUNC   diFunc;
extern struct DI_FUNC   diSelectFunc;
extern const int16      decNumber[];

#define POLE_POISON  0xDEADu
#define FC_POISON    0xA5u

/* 最多详细报告多少条不一致; 之后只计数。见 one() 里的说明。 */
#define FAIL_REPORT_LIMIT  20

/* ------------------------------------------------------------------ layout */

/* Word offset of a MOTOR_FC member, measured on this platform. Hardcoding 5
 * would be wrong on the host: sizeof is 8-bit there, so the `Uint16 all[...]`
 * unions inside MOTOR_FC are twice as wide as on C28x. */
#define WORD_OFF_FC(field) \
    ((Uint16)((Uint16 *)&motorFc.field - (Uint16 *)&motorFc))

#define OFF_RSPEED   WORD_OFF_FC(motorPara.elem.ratingSpeed)
#define OFF_RFRQ     WORD_OFF_FC(motorPara.elem.ratingFrq)

static Uint16 fc_words(void)   { return (Uint16)(sizeof(struct MOTOR_FC) / sizeof(Uint16)); }
static Uint16 motor2_base(void){ return GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.motorType); }

/* ------------------------------------------------------------------ tables */

/* Fill the module's layout table.
 *
 * In the delivered source this table is a static initializer built from
 * GetCodeIndex(); MSVC's C front end cannot fold `&a.m1 - &a.m2` into a constant
 * (error C2099), so the host copy drops the initializer and we compute the very
 * same values here, at run time, with the module's own expression. Nothing is
 * re-derived by hand: if the two MOTOR_FC members ever moved, this would move
 * with them exactly as the static initializer would.
 *
 * The extra assertions pin down the one assumption the target-side initializer
 * also makes -- that motorFcM2/M3/M4 are consecutive and equally sized, so each
 * step is one whole MOTOR_FC. If that ever stops holding, this fails loudly
 * instead of silently exercising the wrong region of funcCode.all[].
 */
static void init_tables(void)
{
    Uint16 stride = fc_words();

    ref_motorSnIndexNIncToMotor2[0] = 0;
    ref_motorSnIndexNIncToMotor2[1] =
        (Uint16)(GetCodeIndex(funcCode.code.motorFcM3.motorCtrlMode)
                 - GetCodeIndex(funcCode.code.motorFcM2.motorCtrlMode));
    ref_motorSnIndexNIncToMotor2[2] =
        (Uint16)(GetCodeIndex(funcCode.code.motorFcM4.motorCtrlMode)
                 - GetCodeIndex(funcCode.code.motorFcM2.motorCtrlMode));

    new_motorSnIndexNIncToMotor2[0] = ref_motorSnIndexNIncToMotor2[0];
    new_motorSnIndexNIncToMotor2[1] = ref_motorSnIndexNIncToMotor2[1];
    new_motorSnIndexNIncToMotor2[2] = ref_motorSnIndexNIncToMotor2[2];

    if (ref_motorSnIndexNIncToMotor2[1] != stride ||
        ref_motorSnIndexNIncToMotor2[2] != (Uint16)(2 * stride)) {
        printf("FATAL: 布局表 {0,%u,%u} 与 sizeof(MOTOR_FC)/2=%u 不符\n",
               ref_motorSnIndexNIncToMotor2[1], ref_motorSnIndexNIncToMotor2[2], stride);
        exit(2);
    }
    if (memcmp(ref_motorSnIndexNIncToMotor2, new_motorSnIndexNIncToMotor2,
               sizeof ref_motorSnIndexNIncToMotor2) != 0) {
        printf("FATAL: ref/new 两侧布局表不一致 (宿主补丁填错)\n");
        exit(2);
    }
}

/* ------------------------------------------------------------------- inputs */

typedef struct {
    FUNCCODE_ALL    fc;
    struct DI_FUNC  diFunc;
    struct DI_FUNC  diSelectFunc;
    enum MOTOR_SN   motorSnIn;
    Uint16          errorCode;
} CASE;

typedef struct {
    struct MOTOR_FC fc;
    enum MOTOR_SN   sn;
    Uint16          pole;
    FUNCCODE_ALL    code;
} OUT;

static OUT g_ref, g_new;

static long g_cases = 0;
static long g_fail  = 0;

static unsigned long long g_rng = 0x243F6A8885A308D3ULL;
static Uint16 rnd16(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return (Uint16)(g_rng >> 17);
}

/* Force every reachable ratingSpeed to a non-zero value.
 *
 * There are exactly four sources: funcCode.code.motorParaM1 (the if-branch) and
 * the three MOTOR_FC structs the else-branch can land on. The three are walked
 * via the SAME stride the module's own index table uses, derived here from
 * sizeof rather than assumed, so this stays correct if the struct grows. */
static void ensure_rating_speed_nonzero(FUNCCODE_ALL *fc)
{
    Uint16 base = motor2_base();
    Uint16 stride = fc_words();
    int k;

    fc->code.motorParaM1.elem.ratingSpeed |= 1u;

    for (k = 0; k < 3; k++) {
        Uint16 at = (Uint16)(base + stride * k + OFF_RSPEED);
        if (at < FNUM_ALL)
            fc->all[at] |= 1u;
    }
}

/* Assert the preconditions listed in the file header, so a driver bug fails
 * loudly here instead of faulting somewhere inside the module. */
static void assert_preconditions(void)
{
    Uint16 base = motor2_base();
    Uint16 stride = fc_words();
    int k;

    /* (2) motorSn must stay inside enum MOTOR_SN, or the else-branch indexes
     * motorSnIndexNIncToMotor2[] out of bounds. */
    if ((Uint16)funcCode.code.motorSn > (Uint16)MOTOR_SN_4) {
        printf("FATAL: funcCode.code.motorSn=%u 超出 enum MOTOR_SN (驱动 bug)\n",
               (unsigned)funcCode.code.motorSn);
        exit(2);
    }
    if ((Uint16)diFunc.f2.bit.motorSnDi > (Uint16)MOTOR_SN_4) {
        printf("FATAL: diFunc.motorSnDi=%u 超出 enum MOTOR_SN\n",
               (unsigned)diFunc.f2.bit.motorSnDi);
        exit(2);
    }

    /* (1) every reachable ratingSpeed non-zero */
    if (funcCode.code.motorParaM1.elem.ratingSpeed == 0) {
        printf("FATAL: motorParaM1.ratingSpeed == 0 (驱动 bug)\n");
        exit(2);
    }
    for (k = 0; k < 3; k++) {
        Uint16 at = (Uint16)(base + stride * k + OFF_RSPEED);
        if (at >= FNUM_ALL) {
            printf("FATAL: ratingSpeed 探针越界 at=%u (FNUM_ALL=%d)\n", at, FNUM_ALL);
            exit(2);
        }
        if (funcCode.all[at] == 0) {
            printf("FATAL: all[%u] ratingSpeed == 0 (驱动 bug)\n", at);
            exit(2);
        }
    }

    /* (3) frqPoint inside decNumber[] */
    if (funcCode.code.frqPoint >= 5) {
        printf("FATAL: frqPoint=%u 超出 decNumber[5]\n", funcCode.code.frqPoint);
        exit(2);
    }
    if (decNumber[funcCode.code.frqPoint] == 0) {
        printf("FATAL: decNumber[%u] == 0 (驱动 bug)\n", funcCode.code.frqPoint);
        exit(2);
    }
}

/* Write motorSnDi through f2.all rather than the named bitfield, so the driver
 * stays honest about what it is driving: if the host laid the bitfield out
 * differently, diFunc_check_layout() below catches it instead of the driver
 * silently feeding the module the wrong bits. */
static Uint32 sn_bits(Uint16 v)
{
    return ((Uint32)(v & 3u)) << 9;   /* motorSnDi occupies bits 9..10 of f2 */
}

static void diFunc_check_layout(void)
{
    struct DI_FUNC probe;
    memset(&probe, 0, sizeof probe);
    probe.f2.all = sn_bits(3u);
    if (probe.f2.bit.motorSnDi != 3u) {
        printf("FATAL: motorSnDi 不在 f2 的 bit 9..10 (读到 %u)\n",
               (unsigned)probe.f2.bit.motorSnDi);
        exit(2);
    }
    if (sizeof(struct DI_FUNC) != 8) {
        printf("FATAL: sizeof(struct DI_FUNC)=%u, 期望 8\n",
               (unsigned)sizeof(struct DI_FUNC));
        exit(2);
    }
}

static void load(const CASE *c)
{
    funcCode     = c->fc;
    diFunc       = c->diFunc;
    diSelectFunc = c->diSelectFunc;
    motorSn      = c->motorSnIn;
    errorCode    = c->errorCode;

    memset(&motorFc, FC_POISON, sizeof motorFc);

    ref_polePair = POLE_POISON;
    new_polePair = POLE_POISON;
}

static void capture(OUT *o)
{
    o->fc   = motorFc;
    o->sn   = motorSn;
    o->pole = 0;
    o->code = funcCode;
}

/* ------------------------------------------------------------------ compare */

static int word_diff(const char *what, const void *a, const void *b, size_t bytes)
{
    const Uint16 *x = (const Uint16 *)a, *y = (const Uint16 *)b;
    size_t n = bytes / sizeof(Uint16), i;
    int shown = 0;
    for (i = 0; i < n; i++) {
        if (x[i] != y[i]) {
            if (shown < 8)
                printf("    %s[%u] ref=%u new=%u\n", what, (unsigned)i, x[i], y[i]);
            shown++;
        }
    }
    return shown;
}

static void dump(const CASE *c)
{
    printf("    in : motorSnIn=%d code.motorSn=%d sel=%u dir=%u err=%u "
           "frqPoint=%u ctrlMode=%u\n",
           (int)c->motorSnIn, (int)c->fc.code.motorSn,
           (unsigned)c->diSelectFunc.f2.bit.motorSnDi,
           (unsigned)c->diFunc.f2.bit.motorSnDi, c->errorCode,
           c->fc.code.frqPoint, c->fc.code.motorCtrlMode);
    printf("         m1.ratingSpeed=%u m1.ratingFrq=%u\n",
           c->fc.code.motorParaM1.elem.ratingSpeed,
           c->fc.code.motorParaM1.elem.ratingFrq);
}

static void one(const CASE *c)
{
    g_cases++;

    load(c);
    assert_preconditions();
    ref_UpdateMotorPara();
    capture(&g_ref);
    g_ref.pole = ref_polePair;

    load(c);
    assert_preconditions();
    new_UpdateMotorPara();
    capture(&g_new);
    g_new.pole = new_polePair;

    {
        /* 逐字比对只在报告预算内做。word_diff 打印差异明细, 若不加这道闸, 一个错
           得很彻底的变体会刷出几十万行 (实测单个变体 20 MB stdout), 既拖慢负向
           对照, 也把证据文件撑爆。超出预算后改用 memcmp, 只判"是否不同"。 */
        int report = (g_fail < FAIL_REPORT_LIMIT);
        int d_fc, d_code, d_sn, d_pole;

        if (report) {
            d_fc   = word_diff("motorFc", &g_ref.fc, &g_new.fc, sizeof(struct MOTOR_FC));
            d_code = word_diff("funcCode", &g_ref.code, &g_new.code, sizeof(FUNCCODE_ALL));
        } else {
            d_fc   = memcmp(&g_ref.fc, &g_new.fc, sizeof(struct MOTOR_FC)) != 0;
            d_code = memcmp(&g_ref.code, &g_new.code, sizeof(FUNCCODE_ALL)) != 0;
        }
        d_sn   = (g_ref.sn != g_new.sn);
        d_pole = (g_ref.pole != g_new.pole);

        if (d_fc || d_code || d_sn || d_pole) {
            g_fail++;
            if (report) {
                printf("MISMATCH UpdateMotorPara\n");
                dump(c);
                if (d_sn)
                    printf("    motorSn ref=%d new=%d\n", (int)g_ref.sn, (int)g_new.sn);
                if (d_pole)
                    printf("    polePair ref=%u new=%u\n", g_ref.pole, g_new.pole);
                if (d_code)
                    printf("    !! 模块改动了 funcCode\n");
            }
        }
    }
}

/* -------------------------------------------------------------------- sweep */

static const enum MOTOR_SN SN[]  = { MOTOR_SN_1, MOTOR_SN_2, MOTOR_SN_3, MOTOR_SN_4 };
static const Uint16 SELDI[]      = { 0, 1, 2, 3 };          /* diSelectFunc 门控 */
static const Uint16 DIRECT[]     = { 0, 1, 2, 3 };          /* diFunc 直给 */
static const Uint16 ERR[]        = { 0, 1, 19, 20, 21, 65535 };  /* 20 == ERROR_ENCODER */
static const Uint16 FRQP[]       = { 0, 1, 2, 3, 4 };       /* decNumber 下标 */
static const Uint16 CTRLMODE[]   = { 0, 1, 2, 3, 65535 };
static const Uint16 RSPEED[]     = { 1, 2, 60, 3000, 65535 };
static const Uint16 RFRQ[]       = { 0, 1, 50, 60, 65535 };

#define N(a) ((int)(sizeof (a) / sizeof (a)[0]))

/* Deterministic filler for the parts of funcCode the sweep does not pin down.
 * A plain counter keeps every case distinct without a PRNG, so the sweep is
 * reproducible even if the RNG is ever reseeded. */
static Uint16 g_filler = 1;

static void fill_pattern(FUNCCODE_ALL *fc)
{
    int i;
    for (i = 0; i < FNUM_ALL; i++) {
        g_filler = (Uint16)(g_filler * 25173u + 13849u);
        fc->all[i] = g_filler;
    }
}

/* Give the three else-branch targets distinct, non-zero speeds, so the
 * divide-by-zero precondition holds no matter which motorSn is selected. */
static void seed_motor_speeds(FUNCCODE_ALL *fc, int h)
{
    Uint16 base = motor2_base(), stride = fc_words();
    int k;
    for (k = 0; k < 3; k++) {
        Uint16 at = (Uint16)(base + stride * k + OFF_RSPEED);
        if (at < FNUM_ALL)
            fc->all[at] = RSPEED[(h + k) % N(RSPEED)];
    }
}

static void sweep(void)
{
    CASE c;
    int a, b, d, e, f, g, h, i;

    for (a = 0; a < N(SN); a++)
    for (b = 0; b < N(SELDI); b++)
    for (d = 0; d < N(DIRECT); d++)
    for (e = 0; e < N(ERR); e++)
    for (f = 0; f < N(FRQP); f++)
    for (g = 0; g < N(CTRLMODE); g++)
    for (h = 0; h < N(RSPEED); h++)
    for (i = 0; i < N(RFRQ); i++) {
        memset(&c, 0, sizeof c);
        fill_pattern(&c.fc);

        /* 模块在两个分支里都会给 motorSn 赋值, 所以初值只用来证明它确实被覆盖:
           给一个范围内但不同的值。真正决定分支的是 code.motorSn。 */
        c.motorSnIn             = SN[(a + 1) & 3];
        c.fc.code.motorSn       = SN[a];
        c.diSelectFunc.f2.all   = sn_bits(SELDI[b]);
        c.diFunc.f2.all         = sn_bits(DIRECT[d]);
        c.errorCode             = ERR[e];
        c.fc.code.frqPoint      = FRQP[f];
        c.fc.code.motorCtrlMode = CTRLMODE[g];
        c.fc.code.torqueBoost     = (Uint16)(g * 3u + 7u);
        c.fc.code.antiVibrateGain = (Uint16)(h * 11u + 1u);

        c.fc.code.motorParaM1.elem.ratingSpeed = RSPEED[h];
        c.fc.code.motorParaM1.elem.ratingFrq   = RFRQ[i];

        seed_motor_speeds(&c.fc, h);
        ensure_rating_speed_nonzero(&c.fc);
        one(&c);
    }
}

static void random_cases(long n)
{
    CASE c;
    long i;
    int k;

    for (i = 0; i < n; i++) {
        memset(&c, 0, sizeof c);
        for (k = 0; k < FNUM_ALL; k++)
            c.fc.all[k] = rnd16();

        c.motorSnIn             = (enum MOTOR_SN)(rnd16() & 3u);
        c.fc.code.motorSn       = (enum MOTOR_SN)(rnd16() & 3u);
        c.diSelectFunc.f2.all   = sn_bits((Uint16)(rnd16() & 3u));
        c.diFunc.f2.all         = sn_bits((Uint16)(rnd16() & 3u));
        c.errorCode             = rnd16();
        c.fc.code.frqPoint      = (Uint16)(rnd16() % 5u);
        c.fc.code.motorCtrlMode = rnd16();
        c.fc.code.torqueBoost   = rnd16();
        c.fc.code.antiVibrateGain = rnd16();

        ensure_rating_speed_nonzero(&c.fc);
        one(&c);
    }
}

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 200000L;

    /* Unbuffered: if a case ever faults, the progress line printed just before it
     * is the only clue to which input triggered it -- with the default block
     * buffering that line is lost with the process. */
    setvbuf(stdout, NULL, _IONBF, 0);

    diFunc_check_layout();
    init_tables();

    printf("布局: sizeof(MOTOR_FC)=%u 字节, %u 字; OFF_RSPEED=%u OFF_RFRQ=%u; "
           "motorFcM2 基址=%u 字; FNUM_ALL=%d\n",
           (unsigned)sizeof(struct MOTOR_FC), (unsigned)fc_words(),
           (unsigned)OFF_RSPEED, (unsigned)OFF_RFRQ,
           (unsigned)motor2_base(), FNUM_ALL);
    printf("布局表: {%u,%u,%u}\n",
           ref_motorSnIndexNIncToMotor2[0], ref_motorSnIndexNIncToMotor2[1],
           ref_motorSnIndexNIncToMotor2[2]);

    sweep();
    random_cases(n);

    printf("f_motorFc difftest: cases=%ld mismatches=%ld\n", g_cases, g_fail);
    return g_fail ? 1 : 0;
}
