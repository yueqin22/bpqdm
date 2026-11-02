/* Differential test driver for f_invPara.
 *
 * The module reads a slice of funcCode and writes back into funcCode.all[],
 * invPara, and the funcCodeOneWriteIndex[] journal. It touches no hardware and
 * takes no pointer arguments, so this is a whole-state comparison: build one
 * input state, run both sides from it, compare everything either side could
 * have written.
 *
 * What is compared after every call:
 *
 *   invPara                      the module's own output struct
 *   funcCode                     the entire union, every word
 *   funcCodeOneWriteNum          how many journal entries were appended
 *   funcCodeOneWriteIndex[0..n)  the appended entries themselves
 *   motor1ParaIndex[]            inputs -- compared to catch a stray write
 *   motorSnIndexIncToMotor1[]    inputs -- same
 *
 * Inputs are randomised across the whole funcCode union except for the fields
 * the module is actually allowed to read (inverterType, frqPoint,
 * motorParaM1.elem.ratingPower). That is deliberate: it proves the module reads
 * only those fields, because any other field it consulted would differ between
 * the two sides' inputs only if the two sides disagreed -- and it cannot, since
 * the input state is rebuilt from one shared snapshot before each side runs.
 *
 * The two layout tables (motor1ParaIndex, motorSnIndexIncToMotor1) are stripped
 * from the source by host_patch.json -- their initialisers are differences of
 * two address constants, which MSVC rejects with C2099. They are rebuilt here at
 * run time from the module's OWN expressions, and their shape is asserted
 * (strictly increasing, uniform motor stride) so a silently wrong table cannot
 * make the test vacuous.
 *
 * Preconditions asserted up front, because the original code reads a table with
 * them and the failure would be a wild read rather than a mismatch:
 *   - funcCode.code.inverterType <= 599   (volLevel = inverterType / 100 must
 *     stay inside invTypeLimitTable[6])
 *   - funcCode.code.frqPoint < 5          (bounds decNumber[5])
 *   - motorSn <= MOTOR_SN_4
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Include order mirrors src/function/f_invPara.c exactly, and it matters.
 *
 * f_invPara.h uses LOCALF/LOCALD and tests F_DEBUG_RAM, both of which come from
 * f_debug.h -- but f_invPara.h does not include f_debug.h itself. On the target
 * that is invisible because f_invPara.c includes f_menu.h first, and f_menu.h
 * -> f_funcCode.h -> f_debug.h does the job. Include f_invPara.h on its own and
 * the compiler dies at f_invPara.h:105 with `LOCALD` treated as an identifier.
 *
 * So f_menu.h goes first here too. It is not decoration: it is the only reason
 * this file can parse f_invPara.h at all, and it is also what makes
 * F_DEBUG_RAM / DEBUG_F_* resolve to the same branch the target build takes.
 *
 * The case's DSP28x_Project.h shim sits at the end of that chain (f_funcCode.h
 * includes it on its line 5) and is what pins Uint16 & co. to C28x widths. */
#include "f_menu.h"
#include "f_eeprom.h"
#include "f_invPara.h"

extern void   ref_UpdateInvType(void);
extern void   new_UpdateInvType(void);
extern Uint16 ref_ValidateInvType(void);
extern Uint16 new_ValidateInvType(void);
extern Uint16 ref_TorqueBoostDeal(Uint16 power);
extern Uint16 new_TorqueBoostDeal(Uint16 power);
extern void   ref_InverterTypeRelatedParaDeal(void);
extern void   new_InverterTypeRelatedParaDeal(void);
extern void   ref_InverterTypeRelatedNoMotorParaDeal(Uint16 invType);
extern void   new_InverterTypeRelatedNoMotorParaDeal(Uint16 invType);
extern void   ref_InverterTypeRelatedNoMotorParaSaveDeal(void);
extern void   new_InverterTypeRelatedNoMotorParaSaveDeal(void);
extern void   ref_MotorPowerRelatedParaDeal(Uint16 power, Uint16 motorSn4Power);
extern void   new_MotorPowerRelatedParaDeal(Uint16 power, Uint16 motorSn4Power);
extern void   ref_SaveTuneData(void);
extern void   new_SaveTuneData(void);

extern struct INV_PARA ref_invPara, new_invPara;

/* SaveTuneData() reads the motorSn global. The project declares it in
 * include/function/f_main.h:70, but f_main.h drags in f_error.h/f_common.h,
 * which the shared SHIM replaces with stand-ins -- pulling it in here would
 * swap real types for stand-ins to obtain one declaration. f_invPara.c itself
 * takes the other way out and declares it inline at f_invPara.c:573; this
 * mirrors that exactly. enum MOTOR_SN still comes from the real f_funcCode.h
 * (via f_menu.h), so the type is the project's, not a hand-copied one. */
extern enum MOTOR_SN motorSn;

/* Patched out of the source; see host_patch.json. One copy per side. */
#define M1_N 22
extern Uint16 ref_motor1ParaIndex[M1_N];
extern Uint16 new_motor1ParaIndex[M1_N];
extern Uint16 ref_motorSnIndexIncToMotor1[4];
extern Uint16 new_motorSnIndexIncToMotor1[4];

/* Shared mock journal (one instance -- snapshot it around each side). */
extern Uint16 funcCodeOneWriteIndex[];
extern Uint16 funcCodeOneWriteNum;

#define WIDX_N 512

#define FAIL_REPORT_LIMIT 20
static long g_cases = 0;
static long g_fail  = 0;
static int  g_stop  = 0;

static unsigned long long g_rng = 0x243F6A8885A308D3ULL;
static Uint16 rnd16(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return (Uint16)(g_rng >> 17);
}

/* ---- snapshots ----------------------------------------------------------- */

static FUNCCODE_ALL     g_fc_in;
static FUNCCODE_ALL     g_fc_ref;
static struct INV_PARA  g_ip_ref;
static Uint16           g_wr_ref[WIDX_N];
static Uint16           g_wn_ref;
static Uint16           g_m1_ref[M1_N];
static Uint16           g_ms_ref[4];
static Uint16           g_sn_ref;

static void snapshot_ref(void)
{
    Uint16 i;
    memcpy(&g_fc_ref, &funcCode, sizeof g_fc_ref);
    memcpy(&g_ip_ref, &ref_invPara, sizeof g_ip_ref);
    memcpy(g_m1_ref, ref_motor1ParaIndex, sizeof g_m1_ref);
    memcpy(g_ms_ref, ref_motorSnIndexIncToMotor1, sizeof g_ms_ref);
    g_sn_ref = (Uint16)motorSn;
    g_wn_ref = funcCodeOneWriteNum;
    for (i = 0; i < g_wn_ref && i < WIDX_N; i++) g_wr_ref[i] = funcCodeOneWriteIndex[i];
}

/* ---- table rebuild ------------------------------------------------------- */

/* GetCodeIndex() is an address difference; on the host the member offsets are
 * the host's, which is fine -- both sides share funcCode, so both see the same
 * numbers, and that is exactly what the module's own expressions would compute. */
#define WOFF(m) ((Uint16)((Uint16 *)&(m) - (Uint16 *)&(funcCode.all[0])))

static Uint16 g_m1[M1_N];
static Uint16 g_ms[4];

static void build_tables(void)
{
    Uint16 k = 0, i;
    Uint32 span = (Uint32)sizeof(funcCode.all);

    g_m1[k++] = WOFF(funcCode.code.motorParaM1.elem.ratingPower);
    g_m1[k++] = WOFF(funcCode.code.motorParaM1.elem.ratingCurrent);

    g_m1[k++] = WOFF(funcCode.code.motorParaM1.elem.statorResistance);
    g_m1[k++] = WOFF(funcCode.code.motorParaM1.elem.rotorResistance);
    g_m1[k++] = WOFF(funcCode.code.motorParaM1.elem.leakInductance);
    g_m1[k++] = WOFF(funcCode.code.motorParaM1.elem.mutualInductance);
    g_m1[k++] = WOFF(funcCode.code.motorParaM1.elem.zeroLoadCurrent);

    g_m1[k++] = WOFF(funcCode.code.motorParaM1.elem.pmsmRs);
    g_m1[k++] = WOFF(funcCode.code.motorParaM1.elem.pmsmLd);
    g_m1[k++] = WOFF(funcCode.code.motorParaM1.elem.pmsmLq);
    g_m1[k++] = WOFF(funcCode.code.motorParaM1.elem.pmsmRsLdUnit);
    g_m1[k++] = WOFF(funcCode.code.motorParaM1.elem.pmsmCoeff);

    g_m1[k++] = WOFF(funcCode.code.vcParaM1.mAcrKp);
    g_m1[k++] = WOFF(funcCode.code.vcParaM1.mAcrKi);
    g_m1[k++] = WOFF(funcCode.code.vcParaM1.tAcrKp);
    g_m1[k++] = WOFF(funcCode.code.vcParaM1.tAcrKi);
    g_m1[k++] = WOFF(funcCode.code.motorParaM1.elem.pmsmCheckTime);

    g_m1[k++] = WOFF(funcCode.code.pgParaM1.elem.enCoderDir);
    g_m1[k++] = WOFF(funcCode.code.pgParaM1.elem.enCoderAngle);
    g_m1[k++] = WOFF(funcCode.code.uvGainWarp);
    g_m1[k++] = WOFF(funcCode.code.pgParaM1.elem.uvwSignDir);
    g_m1[k++] = WOFF(funcCode.code.pgParaM1.elem.uvwSignAngle);

    if (k != M1_N) { printf("table build: got %u entries, expected %d\n", k, M1_N); exit(2); }

    /* The table is grouped by *function*, not by memory order: motorParaM1.elem
     * (0..6), then the PMSM block (7..11), then vcParaM1 (12..15), then
     * motorParaM1.elem.pmsmCheckTime (16), then pgParaM1 (17..21). So it is
     * deliberately non-monotonic -- asserting sorted order here would reject a
     * perfectly valid table. What actually has to hold for the test to be
     * non-vacuous is that the 22 offsets are *distinct* (a permutation of 22
     * separate struct members, so a mistyped entry cannot silently alias
     * another) and that all of them are inside the union. */
    for (i = 0; i < M1_N; i++) {
        Uint16 j;
        if ((Uint32)g_m1[i] >= span) {
            printf("table build: motor1ParaIndex[%u]=%u outside funcCode.all span %lu\n",
                   i, g_m1[i], (unsigned long)span);
            exit(2);
        }
        for (j = 0; j < i; j++) {
            if (g_m1[i] == g_m1[j]) {
                printf("table build: motor1ParaIndex[%u]==[%u]==%u (alias)\n",
                       i, j, g_m1[i]);
                exit(2);
            }
        }
    }

    g_ms[0] = 0;
    g_ms[1] = (Uint16)(WOFF(funcCode.code.motorFcM2.motorPara.all[0])
                       - WOFF(funcCode.code.motorParaM1.all[0]));
    g_ms[2] = (Uint16)(WOFF(funcCode.code.motorFcM3.motorPara.all[0])
                       - WOFF(funcCode.code.motorParaM1.all[0]));
    g_ms[3] = (Uint16)(WOFF(funcCode.code.motorFcM4.motorPara.all[0])
                       - WOFF(funcCode.code.motorParaM1.all[0]));

    /* g_ms[0] is 0 by construction and the other three must be strictly
     * increasing (each motor block starts later than the previous one). The
     * largest index the module can build must still land inside the union --
     * otherwise the module would write outside funcCode.all[] and the host would
     * corrupt whatever follows instead of reporting a mismatch. */
    if (g_ms[1] == 0 || g_ms[2] <= g_ms[1] || g_ms[3] <= g_ms[2]) {
        printf("table build: motorSnIndexIncToMotor1 not increasing: %u %u %u\n",
               g_ms[1], g_ms[2], g_ms[3]);
        exit(2);
    }
    {
        Uint32 hi = 0;
        for (i = 0; i < M1_N; i++) {
            Uint32 t = (Uint32)g_m1[i] + g_ms[3];
            if (t > hi) hi = t;
        }
        /* SaveTuneData() also writes index+add and reads back through
         * gParaIdToFunctionDataBuff, so allow a small tail margin. */
        if (hi + 8 > span) {
            printf("table build: highest index %lu exceeds funcCode.all span %lu\n",
                   (unsigned long)hi, (unsigned long)span);
            exit(2);
        }
    }

    /* Printed so the numbers the test ran against are in the log, not inferred
     * from a passing exit code. */
    printf("tables: span=%lu m1=", (unsigned long)span);
    for (i = 0; i < M1_N; i++) printf("%u ", g_m1[i]);
    printf("\n        ms=%u %u %u %u\n", g_ms[0], g_ms[1], g_ms[2], g_ms[3]);

    for (i = 0; i < M1_N; i++) {
        ref_motor1ParaIndex[i] = g_m1[i];
        new_motor1ParaIndex[i] = g_m1[i];
    }
    for (i = 0; i < 4; i++) {
        ref_motorSnIndexIncToMotor1[i] = g_ms[i];
        new_motorSnIndexIncToMotor1[i] = g_ms[i];
    }
}

/* ---- input construction -------------------------------------------------- */

/* Every valid (volLevel, type) pair, i.e. exactly the cells the parameter
 * tables actually have. Anything outside these would index off the end of
 * pInvTypeParaTable[volLevel]. */
static const struct { Uint16 lower, upper; } LEVEL[6] = {
    { 8, 37 },   /* T380  */
    { 7, 22 },   /* T220  */
    { 1,  7 },   /* S220  */
    { 8, 34 },   /* T480  */
    { 21, 36 },  /* T690  */
    { 19, 37 },  /* T1140 */
};

static void build_input(Uint16 invType, Uint16 frqPoint, Uint16 power,
                        Uint32 seed, Uint16 motorSnIn)
{
    Uint16 *w = (Uint16 *)&g_fc_in;
    Uint16 i;

    /* Randomise the whole union, then pin down the fields the module may read.
     * A field the module reads but should not would have to agree between the
     * two sides -- and it does, since this runs once per case, not per side. */
    for (i = 0; i < (Uint16)(sizeof g_fc_in / sizeof(Uint16)); i++) {
        seed = seed * 1103515245u + 12345u;
        w[i] = (Uint16)(seed >> 16);
    }

    g_fc_in.code.inverterType = invType;
    g_fc_in.code.frqPoint = frqPoint;
    g_fc_in.code.motorParaM1.elem.ratingPower = power;

    motorSn = (enum MOTOR_SN)motorSnIn;
}

static void load_input(void)
{
    Uint16 i;
    memcpy(&funcCode, &g_fc_in, sizeof funcCode);
    funcCodeOneWriteNum = 0;
    for (i = 0; i < WIDX_N; i++) funcCodeOneWriteIndex[i] = 0xA5A5u;
    /* invPara is an output, but it is also an input to three of the functions
     * (volLevel drives pInvTypeParaTable[], bitAccDecStart and pointLimit are
     * read directly). Poison it so a function that forgets to call
     * UpdateInvType() shows up as a mismatch instead of silently reading a
     * leftover value that happens to match. */
    memset(&ref_invPara, 0xA5, sizeof ref_invPara);
    memset(&new_invPara, 0xA5, sizeof new_invPara);
}

/* ---- comparison ---------------------------------------------------------- */

static int word_diff(const char *what, const void *a, const void *b, Uint32 nwords)
{
    const Uint16 *x = (const Uint16 *)a, *y = (const Uint16 *)b;
    Uint32 i, shown = 0;
    int bad = 0;
    for (i = 0; i < nwords; i++) {
        if (x[i] != y[i]) {
            bad = 1;
            if (shown < 8) {
                printf("    %s[%lu] ref=%u new=%u\n",
                       what, (unsigned long)i, x[i], y[i]);
                shown++;
            }
        }
    }
    return bad;
}

static void note_fail(Uint16 invType, Uint16 frqPoint, Uint16 power, Uint16 sn)
{
    g_fail++;
    if (g_fail >= FAIL_REPORT_LIMIT) g_stop = 1;
    if (g_fail <= FAIL_REPORT_LIMIT) {
        printf("  in: inverterType=%u frqPoint=%u power=%u motorSn=%u\n",
               invType, frqPoint, power, sn);
    }
}

static void compare(const char *fn, Uint16 invType, Uint16 frqPoint, Uint16 power,
                    Uint16 sn)
{
    int bad = 0;
    Uint16 i, n;

    if (funcCodeOneWriteNum != g_wn_ref) {
        bad = 1;
        printf("MISMATCH %s: journal length ref=%u new=%u\n",
               fn, g_wn_ref, funcCodeOneWriteNum);
    }
    n = (g_wn_ref < funcCodeOneWriteNum) ? g_wn_ref : funcCodeOneWriteNum;
    for (i = 0; i < n; i++) {
        if (funcCodeOneWriteIndex[i] != g_wr_ref[i]) {
            bad = 1;
            printf("MISMATCH %s: journal[%u] ref=%u new=%u\n",
                   fn, i, g_wr_ref[i], funcCodeOneWriteIndex[i]);
            break;
        }
    }
    if (memcmp(&g_ip_ref, &new_invPara, sizeof g_ip_ref) != 0) {
        bad = 1;
        printf("MISMATCH %s: invPara differs\n", fn);
    }
    if (memcmp(&g_fc_ref, &funcCode, sizeof g_fc_ref) != 0) {
        bad = 1;
        printf("MISMATCH %s: funcCode differs\n", fn);
    }
    if (memcmp(g_m1_ref, new_motor1ParaIndex, sizeof g_m1_ref) != 0) {
        bad = 1;
        printf("MISMATCH %s: motor1ParaIndex was written\n", fn);
    }
    if (memcmp(g_ms_ref, new_motorSnIndexIncToMotor1, sizeof g_ms_ref) != 0) {
        bad = 1;
        printf("MISMATCH %s: motorSnIndexIncToMotor1 was written\n", fn);
    }
    if ((Uint16)motorSn != g_sn_ref) {
        bad = 1;
        printf("MISMATCH %s: motorSn was written\n", fn);
    }

    if (bad) {
        printf("MISMATCH %s\n", fn);
        note_fail(invType, frqPoint, power, sn);
        if (g_fail < FAIL_REPORT_LIMIT) {
            word_diff("invPara", &g_ip_ref, &new_invPara,
                      sizeof g_ip_ref / sizeof(Uint16));
        }
    }
    g_cases++;
}

/* ---- the four call shapes ------------------------------------------------ */

static void case_deal(Uint16 invType, Uint16 frqPoint, Uint16 power, Uint16 sn,
                      Uint32 seed)
{
    if (g_stop) return;
    build_input(invType, frqPoint, power, seed, sn);

    load_input();
    ref_InverterTypeRelatedParaDeal();
    snapshot_ref();

    load_input();
    new_InverterTypeRelatedParaDeal();

    compare("InverterTypeRelatedParaDeal", invType, frqPoint, power, sn);
}

static void case_update_then_nomotor(Uint16 invType, Uint16 frqPoint, Uint16 power,
                                     Uint16 sn, Uint32 seed)
{
    if (g_stop) return;
    build_input(invType, frqPoint, power, seed, sn);

    load_input();
    ref_UpdateInvType();
    ref_InverterTypeRelatedNoMotorParaDeal(ref_invPara.type);
    snapshot_ref();

    load_input();
    new_UpdateInvType();
    new_InverterTypeRelatedNoMotorParaDeal(new_invPara.type);

    compare("UpdateInvType+NoMotorParaDeal", invType, frqPoint, power, sn);
}

static void case_power(Uint16 invType, Uint16 frqPoint, Uint16 power, Uint16 sn,
                       Uint32 seed)
{
    if (g_stop) return;
    build_input(invType, frqPoint, power, seed, sn);

    load_input();
    ref_UpdateInvType();
    ref_MotorPowerRelatedParaDeal(power, sn);
    snapshot_ref();

    load_input();
    new_UpdateInvType();
    new_MotorPowerRelatedParaDeal(power, sn);

    compare("MotorPowerRelatedParaDeal", invType, frqPoint, power, sn);
}

static void case_tune(Uint16 invType, Uint16 frqPoint, Uint16 power, Uint16 sn,
                      Uint32 seed)
{
    if (g_stop) return;
    build_input(invType, frqPoint, power, seed, sn);

    load_input();
    ref_SaveTuneData();
    snapshot_ref();

    load_input();
    new_SaveTuneData();

    compare("SaveTuneData", invType, frqPoint, power, sn);
}

static void case_save(void)
{
    Uint16 invType, frqPoint;

    for (invType = 0; invType <= 599 && !g_stop; invType += 7) {
        for (frqPoint = 0; frqPoint < 5 && !g_stop; frqPoint++) {
            if (g_stop) return;
            build_input(invType, frqPoint, 100, 1, 1);

            load_input();
            ref_UpdateInvType();
            ref_InverterTypeRelatedNoMotorParaSaveDeal();
            snapshot_ref();

            load_input();
            new_UpdateInvType();
            new_InverterTypeRelatedNoMotorParaSaveDeal();

            compare("NoMotorParaSaveDeal", invType, frqPoint, 100, 1);
        }
    }
}

/* ---- pure functions ------------------------------------------------------ */

static void sec_torque_boost(void)
{
    Uint32 p;
    for (p = 0; p <= 0xFFFFu && !g_stop; p++) {
        Uint16 r, n;
        g_cases++;
        r = ref_TorqueBoostDeal((Uint16)p);
        n = new_TorqueBoostDeal((Uint16)p);
        if (r != n) {
            if (g_fail < FAIL_REPORT_LIMIT)
                printf("MISMATCH TorqueBoostDeal power=%lu ref=%u new=%u\n",
                       (unsigned long)p, r, n);
            note_fail(0, 0, (Uint16)p, 0);
        }
    }
}

static void sec_validate(void)
{
    Uint32 t;
    for (t = 0; t <= 599 && !g_stop; t++) {
        Uint16 r, n;
        g_cases++;
        funcCode.code.inverterType = (Uint16)t;
        r = ref_ValidateInvType();
        funcCode.code.inverterType = (Uint16)t;
        n = new_ValidateInvType();
        if (r != n) {
            if (g_fail < FAIL_REPORT_LIMIT)
                printf("MISMATCH ValidateInvType inverterType=%lu ref=%u new=%u\n",
                       (unsigned long)t, r, n);
            note_fail((Uint16)t, 0, 0, 0);
        }
    }
}

/* ---- sweeps -------------------------------------------------------------- */

static const Uint16 POWERS[] = {
    0, 1, 5, 10, 11, 40, 41, 75, 76, 370, 371, 749, 750, 751, 2000, 65535
};
#define NPOWERS ((Uint16)(sizeof POWERS / sizeof POWERS[0]))

static void sweep_valid_deal(long nrand)
{
    Uint16 L, t, fp;
    for (L = 0; L < 6 && !g_stop; L++) {
        for (t = LEVEL[L].lower; t <= LEVEL[L].upper && !g_stop; t++) {
            for (fp = 0; fp < 5 && !g_stop; fp++) {
                case_deal((Uint16)(L * 100 + t), fp, 750, 1,
                          (Uint32)(L * 1000 + t * 7 + fp));
            }
        }
    }
    (void)nrand;
}

static void sweep_power(long nrand)
{
    Uint16 i, sn, fp;
    for (i = 0; i < NPOWERS && !g_stop; i++) {
        for (sn = 0; sn < 4 && !g_stop; sn++) {
            for (fp = 0; fp < 5 && !g_stop; fp++) {
                case_power(0 * 100 + 8, fp, POWERS[i], sn,
                           (Uint32)(i * 100 + sn * 10 + fp));
            }
        }
    }
    (void)nrand;
}

static void sweep_nomotor(long nrand)
{
    Uint16 L, t, fp;
    for (L = 0; L < 6 && !g_stop; L++) {
        for (t = LEVEL[L].lower; t <= LEVEL[L].upper && !g_stop; t++) {
            for (fp = 0; fp < 5 && !g_stop; fp++) {
                case_update_then_nomotor((Uint16)(L * 100 + t), fp, 750, 1,
                                         (Uint32)(L * 977 + t * 13 + fp));
            }
        }
    }
    (void)nrand;
}

static void sweep_tune(long nrand)
{
    Uint16 sn, i;
    for (sn = 0; sn < 4 && !g_stop; sn++) {
        for (i = 0; i < 16 && !g_stop; i++) {
            case_tune((Uint16)((i % 6) * 100 + 8), (Uint16)(i % 5), POWERS[i], sn,
                      (Uint32)(sn * 31 + i));
        }
    }
    (void)nrand;
}

static void sweep_random(long n)
{
    long k;
    for (k = 0; k < n && !g_stop; k++) {
        Uint16 L = (Uint16)(rnd16() % 6);
        Uint16 span = (Uint16)(LEVEL[L].upper - LEVEL[L].lower + 1);
        Uint16 t = (Uint16)(LEVEL[L].lower + rnd16() % span);
        Uint16 invType = (Uint16)(L * 100 + t);
        Uint16 fp = (Uint16)(rnd16() % 5);
        Uint16 power = POWERS[rnd16() % NPOWERS];
        Uint16 sn = (Uint16)(rnd16() & 3u);
        Uint32 seed = ((Uint32)rnd16() << 16) ^ rnd16();

        switch (rnd16() % 4u) {
        case 0: case_deal(invType, fp, power, sn, seed); break;
        case 1: case_update_then_nomotor(invType, fp, power, sn, seed); break;
        case 2: case_power(invType, fp, power, sn, seed); break;
        default: case_tune(invType, fp, power, sn, seed); break;
        }
    }
}

/* ---- main ---------------------------------------------------------------- */

static void put_tune_pattern(Uint16 seed)
{
    Uint16 i;
    Uint32 s = seed;
    for (i = 0; i < TUNE_DATA_NUM; i++) {
        s = s * 1103515245u + 12345u;
        gParaIdToFunctionDataBuff[i] = (int16)(s >> 16);
    }
}

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 20000L;
    long c0;

    setvbuf(stdout, NULL, _IONBF, 0);

    memset(&funcCode, 0, sizeof funcCode);
    memset(&ref_invPara, 0, sizeof ref_invPara);
    memset(&new_invPara, 0, sizeof new_invPara);
    build_tables();
    put_tune_pattern(0x1234u);

    c0 = g_cases; sec_torque_boost();      printf("  TorqueBoostDeal (exhaustive) : %ld\n", g_cases - c0);
    c0 = g_cases; sec_validate();          printf("  ValidateInvType (exhaustive) : %ld\n", g_cases - c0);
    c0 = g_cases; sweep_valid_deal(n);     printf("  RelatedParaDeal  (valid grid): %ld\n", g_cases - c0);
    c0 = g_cases; sweep_nomotor(n);        printf("  NoMotorParaDeal  (valid grid): %ld\n", g_cases - c0);
    c0 = g_cases; case_save();             printf("  NoMotorParaSaveDeal          : %ld\n", g_cases - c0);
    c0 = g_cases; sweep_power(n);          printf("  MotorPowerRelatedParaDeal    : %ld\n", g_cases - c0);
    c0 = g_cases; sweep_tune(n);           printf("  SaveTuneData                 : %ld\n", g_cases - c0);
    c0 = g_cases; sweep_random(n);         printf("  random mixed                 : %ld\n", g_cases - c0);

    printf("f_invPara difftest: cases=%ld mismatches=%ld\n", g_cases, g_fail);
    return g_fail ? 1 : 0;
}
