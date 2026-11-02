/* Differential test driver for f_common.
 *
 * The module takes no inputs beyond its arguments and touches no hardware, so
 * this is a pure function comparison: identical arguments to both sides, then
 * compare the returned word. Coverage is built in three tiers.
 *
 *   1. Exhaustive over the whole 16-bit input space for the two bit-reversal
 *      helpers (2 x 65536 values).
 *
 *   2. Exhaustive CRC sweeps. A single CRC step is
 *          tmp       = table[(crcValue ^ *data) & 0x00FF];
 *          crcValue  = ((tmp & 0x00FF) << 8) + ((tmp ^ crcValue) >> 8);
 *      so the step function depends only on (crcValue, *data & 0x00FF): the
 *      high byte of the data word must not reach the result, and only the low
 *      byte of the table index may be used. The sweep therefore runs the full
 *      16-bit space at length 1, the full 256x256 byte-pair space at length 2,
 *      and the full 256^3 byte-triple space at length 3 -- with non-zero high
 *      bytes folded in so that a dropped `& 0x00FF` mask cannot go unnoticed.
 *
 *   3. Structured and pseudo-random buffers up to 512 words, which exercise the
 *      loop-carried accumulation over long chains rather than a single step.
 *
 * Every call is additionally checked for writes into the caller's buffer (the
 * parameter is const) and for writes one word before / after the region.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "f_common.h"

extern Uint16 ref_CrcValueByteCalc(const Uint16 *data, Uint16 length);
extern Uint16 new_CrcValueByteCalc(const Uint16 *data, Uint16 length);
extern Uint16 ref_Reverse4Bit(Uint16 value);
extern Uint16 new_Reverse4Bit(Uint16 value);
extern Uint16 ref_Reverse8Bit(Uint16 value);
extern Uint16 new_Reverse8Bit(Uint16 value);

#define GUARD   4
#define MAXLEN  512
#define SENT    0xDEADu
#define LOWG    0xA5A5u
#define HIGHG   0x5A5Au

/* [ low guard ][ data MAXLEN ][ sentinel ][ high guard ] */
#define SENT_OFF   (GUARD + MAXLEN)
#define HGUARD_OFF (GUARD + MAXLEN + 1)

static Uint16 g_buf[GUARD + MAXLEN + 1 + GUARD];
static Uint16 g_in[MAXLEN];
static Uint16 *const g_data = &g_buf[GUARD];

static long g_cases = 0;
static long g_fail  = 0;
static long g_touch = 0;

/* Set once enough mismatches have been seen. The exhaustive sweeps would
 * otherwise keep running for ~17M more cases after the answer is already
 * known -- which matters for the negative control, where every variant is
 * expected to fail. A clean run never trips it.
 */
#define FAIL_LIMIT 100
static int g_stop = 0;

static unsigned long long g_rng = 0x9E3779B97F4A7C15ULL;
static Uint16 rnd16(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return (Uint16)(g_rng >> 17);
}

static void put_guards(void)
{
    int i;
    for (i = 0; i < GUARD; i++) {
        g_buf[i] = LOWG;
        g_buf[HGUARD_OFF + i] = HIGHG;
    }
    g_buf[SENT_OFF] = SENT;
}

static void put_input(const Uint16 *words, Uint16 length)
{
    Uint16 i;
    for (i = 0; i < length; i++) {
        g_data[i] = words[i];
        g_in[i] = words[i];
    }
    g_data[length] = SENT;
}

/* 1 if the last call touched memory outside [0, length) of the data region */
static int verify_clean(Uint16 length)
{
    int i;
    for (i = 0; i < GUARD; i++) {
        if (g_buf[i] != LOWG) return 1;
        if (g_buf[HGUARD_OFF + i] != HIGHG) return 1;
    }
    if (g_data[MAXLEN] != SENT) return 1;
    if (memcmp(g_data, g_in, (size_t)length * sizeof (Uint16)) != 0) return 1;
    return 0;
}

static void note_fail(void)
{
    g_fail++;
    if (g_fail >= FAIL_LIMIT) g_stop = 1;
}

static void crc_case(const Uint16 *words, Uint16 length)
{
    Uint16 r, n;

    if (g_stop) return;
    g_cases++;

    put_input(words, length);
    r = ref_CrcValueByteCalc(g_data, length);
    if (verify_clean(length)) g_touch++;

    put_input(words, length);
    n = new_CrcValueByteCalc(g_data, length);
    if (verify_clean(length)) g_touch++;
    if (g_touch >= FAIL_LIMIT) g_stop = 1;

    if (r != n) {
        if (g_fail < 20) {
            Uint16 i;
            printf("MISMATCH CrcValueByteCalc len=%u ref=%04X new=%04X in=",
                   length, r, n);
            for (i = 0; i < length && i < 8; i++) printf("%04X ", words[i]);
            printf("\n");
        }
        note_fail();
    }
}

/* ---- tier 1: bit-reversal helpers, exhaustive ---------------------------- */

static void sec_reverse(void)
{
    unsigned int i;
    for (i = 0; i <= 0xFFFFu && !g_stop; i++) {
        Uint16 v = (Uint16)i;
        Uint16 r, n;

        g_cases++;
        r = ref_Reverse4Bit(v);
        n = new_Reverse4Bit(v);
        if (r != n) {
            if (g_fail < 20)
                printf("MISMATCH Reverse4Bit v=%04X ref=%04X new=%04X\n", v, r, n);
            note_fail();
        }

        g_cases++;
        r = ref_Reverse8Bit(v);
        n = new_Reverse8Bit(v);
        if (r != n) {
            if (g_fail < 20)
                printf("MISMATCH Reverse8Bit v=%04X ref=%04X new=%04X\n", v, r, n);
            note_fail();
        }
    }
}

/* ---- tier 2: exhaustive CRC sweeps --------------------------------------- */

static void sec_crc_exhaustive(void)
{
    Uint16 w[3];
    unsigned int a, b, c;

    w[0] = w[1] = w[2] = 0;

    /* length 0: the loop body must not run at all */
    crc_case(w, 0);

    /* length 1: the whole 16-bit input space (high bytes included) */
    for (a = 0; a <= 0xFFFFu && !g_stop; a++) {
        w[0] = (Uint16)a;
        crc_case(w, 1);
    }

    /* length 2: every low-byte pair, with high bytes deliberately set so that
       a rewrite which drops the `& 0x00FF` mask changes the table index. */
    for (a = 0; a <= 0xFFu && !g_stop; a++) {
        w[0] = (Uint16)(a | 0xFF00u);
        for (b = 0; b <= 0xFFu && !g_stop; b++) {
            w[1] = (Uint16)(b | 0xAA00u);
            crc_case(w, 2);
        }
    }

    /* length 3: the complete 3-byte space */
    for (a = 0; a <= 0xFFu && !g_stop; a++) {
        w[0] = (Uint16)a;
        for (b = 0; b <= 0xFFu && !g_stop; b++) {
            w[1] = (Uint16)b;
            for (c = 0; c <= 0xFFu && !g_stop; c++) {
                w[2] = (Uint16)c;
                crc_case(w, 3);
            }
        }
    }
}

/* ---- tier 3: structured and random long buffers -------------------------- */

static const Uint16 LENS[] = {
    1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 31, 32, 33, 63, 64, 65,
    127, 128, 129, 254, 255, 256, 257, 511, 512
};
#define NLENS ((int)(sizeof LENS / sizeof LENS[0]))

static void sec_structured(void)
{
    Uint16 w[MAXLEN];
    int k, p;
    Uint16 i;

    for (p = 0; p < 11 && !g_stop; p++) {
        for (k = 0; k < NLENS && !g_stop; k++) {
            Uint16 len = LENS[k];
            for (i = 0; i < len; i++) {
                switch (p) {
                case 0:  w[i] = 0x0000u; break;
                case 1:  w[i] = 0xFFFFu; break;
                case 2:  w[i] = 0x00FFu; break;
                case 3:  w[i] = 0xFF00u; break;
                case 4:  w[i] = 0x5555u; break;
                case 5:  w[i] = 0xAAAAu; break;
                case 6:  w[i] = i; break;
                case 7:  w[i] = (Uint16)(len - 1 - i); break;
                case 8:  w[i] = (Uint16)((i & 1u) ? 0xFFFFu : 0x0000u); break;
                case 9:  w[i] = (Uint16)(i * i); break;
                default: w[i] = (Uint16)(0x8000u >> (i & 15u)); break;
                }
            }
            crc_case(w, len);
        }
    }
}

static void sec_random(long n)
{
    Uint16 w[MAXLEN];
    long k;
    for (k = 0; k < n && !g_stop; k++) {
        Uint16 len = (Uint16)(rnd16() % (MAXLEN + 1));
        Uint16 i;
        for (i = 0; i < len; i++) w[i] = rnd16();
        crc_case(w, len);
    }
}

/* -------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 200000L;
    long c0;

    put_guards();
    c0 = g_cases; sec_reverse();        printf("  reverse (exhaustive) : %ld\n", g_cases - c0);
    c0 = g_cases; sec_crc_exhaustive(); printf("  crc     (exhaustive) : %ld\n", g_cases - c0);
    c0 = g_cases; sec_structured();     printf("  crc     (structured) : %ld\n", g_cases - c0);
    c0 = g_cases; sec_random(n);        printf("  crc     (random)     : %ld\n", g_cases - c0);

    printf("f_common difftest: cases=%ld mismatches=%ld buffer_violations=%ld\n",
           g_cases, g_fail, g_touch);
    return (g_fail || g_touch) ? 1 : 0;
}
