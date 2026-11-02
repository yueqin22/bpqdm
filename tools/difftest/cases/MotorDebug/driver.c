/* Differential test driver for MotorDebug.
 *
 * Coverage notes
 * --------------
 * 1. `gDebugBuffer` is patched to `Uint16` on the host (host_patch.json) because
 *    `Uint` is 1 word on C28x and 4 bytes on the host.  Without that patch
 *    `SaveDebugData32`'s `p = (Ulong *)&gDebugBuffer; p[gDebugPoint++] = data;`
 *    would clobber one host element instead of the two 16-bit words it clobbers
 *    on the target, so the buffer contents could never be compared faithfully.
 *    The driver therefore declares and compares the buffer as Uint16 words.
 * 2. All inputs stay inside the target's widths: `int` fields are drawn from
 *    [-32768, 32767] and `Uint` fields from [0, 65535].  Feeding a host-only
 *    width (e.g. TestData17 = 40000) would make the two sides compute different
 *    values for the *same* source text -- a host/target model artefact, not a
 *    difference in the code under test.
 * 3. `DebugSaveDeal` is stateful (SaveTmCnt / saveRstMem / gDebugPoint), so the
 *    driver runs **sequences** of calls against one installed snapshot and
 *    compares after every call.  That is what makes the `SaveTmCnt >=
 *    TestData17/2` half-period gate and the TestData15 rising/falling edge on
 *    `saveRstMem` reachable at all.  TestData17 and SaveTmCnt are swept
 *    together over every divisor of TestData17 (the only way to land exactly
 *    on the boundary, which is the single point where `>=` and `>` differ).
 * 4. `pVD1..pVD4` are module globals renamed per side, so the driver declares
 *    both and points them at the same four ints -- the module only reads
 *    through them (`SaveDebugData16(*pVD1)`), never writes.
 */

#include <stdio.h>
#include <string.h>

#include "MotorInclude.h"

extern long atol(const char *);

/* Must equal the module's C_DEBUG_BUFF_SIZE (1000 for this 28035 project; the
 * TMS320F2808 branch is dead code).  Only used for the host snapshot size. */
#define C_DBG_SIZE 1000

extern void ref_DebugSaveDeal(Uint savePrgPos);
extern void new_DebugSaveDeal(Uint savePrgPos);
extern void ref_SaveDebugData16(Uint data);
extern void new_SaveDebugData16(Uint data);
extern void ref_SaveDebugData32(Ulong data);
extern void new_SaveDebugData32(Ulong data);
extern void ref_ResetDebugBuffer(void);
extern void new_ResetDebugBuffer(void);

extern Uint16 ref_gDebugBuffer[C_DBG_SIZE];
extern Uint16 new_gDebugBuffer[C_DBG_SIZE];
extern Uint   ref_gDebugPoint;
extern Uint   new_gDebugPoint;
extern int    ref_gDebugFlag;
extern int    new_gDebugFlag;
extern Ulong  ref_saveIndexFunc;
extern Ulong  new_saveIndexFunc;
extern Ulong  ref_saveIndexP;
extern Ulong  new_saveIndexP;
extern Uint   ref_saveCurveNum;
extern Uint   new_saveCurveNum;
extern Uint   ref_saveRstMem;
extern Uint   new_saveRstMem;
extern Uint   ref_SaveTmCnt;
extern Uint   new_SaveTmCnt;
extern int    ref_startSave;
extern int    new_startSave;
extern int   *ref_pVD1;
extern int   *new_pVD1;
extern int   *ref_pVD2;
extern int   *new_pVD2;
extern int   *ref_pVD3;
extern int   *new_pVD3;
extern int   *ref_pVD4;
extern int   *new_pVD4;

/* ---------- input side: gTestDataReceive / gMainCmd (shared mock objects) --- */

typedef struct {
    int td12, td13, td14, td15, td17;
    int start;                 /* gMainCmd.Command.bit.Start */
} IN;

/* the four ints pVD1..pVD4 point at -- one instance, both sides read it */
static int g_pv[4];

static void install_in(const IN *in)
{
    memset(&gTestDataReceive, 0, sizeof gTestDataReceive);
    gTestDataReceive.TestData12 = in->td12;
    gTestDataReceive.TestData13 = in->td13;
    gTestDataReceive.TestData14 = in->td14;
    gTestDataReceive.TestData15 = in->td15;
    gTestDataReceive.TestData17 = in->td17;
    gMainCmd.Command.all = 0;
    gMainCmd.Command.bit.Start = (Uint)in->start;
    g_pv[0] = 0x1000;
    g_pv[1] = (int)0x9ABC;
    g_pv[2] = (int)0xDEF0;
    g_pv[3] = -12345;
}

/* ---------- module state, one copy per side ---------- */

typedef struct {
    Uint16 dbg[C_DBG_SIZE];
    Uint   point;
    int    flag;
    Ulong  indexFunc;
    Ulong  indexP;
    Uint   curveNum;
    Uint   rstMem;
    Uint   tmCnt;
    int    startSave;
    int   *pVD1;
    int   *pVD2;
    int   *pVD3;
    int   *pVD4;
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
static int   rnd16(void)  { return (int)(rnd32() & 0xFFFF); }
static Uint  rndUint(void) { return (Uint)(rnd32() & 0xFFFF); }

static void install(const SNAP *s)
{
    memcpy(ref_gDebugBuffer, s->dbg, sizeof s->dbg);
    memcpy(new_gDebugBuffer, s->dbg, sizeof s->dbg);
    ref_gDebugPoint  = new_gDebugPoint  = s->point;
    ref_gDebugFlag   = new_gDebugFlag   = s->flag;
    ref_saveIndexFunc = new_saveIndexFunc = s->indexFunc;
    ref_saveIndexP    = new_saveIndexP    = s->indexP;
    ref_saveCurveNum = new_saveCurveNum = s->curveNum;
    ref_saveRstMem   = new_saveRstMem   = s->rstMem;
    ref_SaveTmCnt    = new_SaveTmCnt    = s->tmCnt;
    ref_startSave    = new_startSave    = s->startSave;
    ref_pVD1 = new_pVD1 = &g_pv[0];
    ref_pVD2 = new_pVD2 = &g_pv[1];
    ref_pVD3 = new_pVD3 = &g_pv[2];
    ref_pVD4 = new_pVD4 = &g_pv[3];
}

static void snapshot_ref(SNAP *s)
{
    memcpy(s->dbg, ref_gDebugBuffer, sizeof s->dbg);
    s->point     = ref_gDebugPoint;
    s->flag      = ref_gDebugFlag;
    s->indexFunc = ref_saveIndexFunc;
    s->indexP    = ref_saveIndexP;
    s->curveNum  = ref_saveCurveNum;
    s->rstMem    = ref_saveRstMem;
    s->tmCnt     = ref_SaveTmCnt;
    s->startSave = ref_startSave;
    s->pVD1 = ref_pVD1;
    s->pVD2 = ref_pVD2;
    s->pVD3 = ref_pVD3;
    s->pVD4 = ref_pVD4;
}

static void snapshot_new(SNAP *s)
{
    memcpy(s->dbg, new_gDebugBuffer, sizeof s->dbg);
    s->point     = new_gDebugPoint;
    s->flag      = new_gDebugFlag;
    s->indexFunc = new_saveIndexFunc;
    s->indexP    = new_saveIndexP;
    s->curveNum  = new_saveCurveNum;
    s->rstMem    = new_saveRstMem;
    s->tmCnt     = new_SaveTmCnt;
    s->startSave = new_startSave;
    s->pVD1 = new_pVD1;
    s->pVD2 = new_pVD2;
    s->pVD3 = new_pVD3;
    s->pVD4 = new_pVD4;
}

static void show(const char *tag, const SNAP *s)
{
    printf("  %s: point=%u flag=%d curveNum=%u rstMem=%u tmCnt=%u idxP=%lu\n",
           tag, (unsigned)s->point, s->flag, (unsigned)s->curveNum,
           (unsigned)s->rstMem, (unsigned)s->tmCnt,
           (unsigned long)s->indexP);
}

/* ---------------- one operation, both sides ---------------- */

static void op_save16(Uint data)
{
    SNAP r, n;
    g_cases++;
    ref_SaveDebugData16(data);
    new_SaveDebugData16(data);
    memset(&r, 0, sizeof r);
    memset(&n, 0, sizeof n);
    snapshot_ref(&r);
    snapshot_new(&n);
    if (r.point != n.point || memcmp(r.dbg, n.dbg, sizeof r.dbg) != 0) {
        g_fail++;
        if (g_fail <= 20) {
            printf("MISMATCH SaveDebugData16 data=%u\n", (unsigned)data);
            printf("  ref point=%u dbg[0..3]=%u/%u/%u/%u\n",
                   (unsigned)r.point, (unsigned)r.dbg[0], (unsigned)r.dbg[1],
                   (unsigned)r.dbg[2], (unsigned)r.dbg[3]);
            printf("  new point=%u dbg[0..3]=%u/%u/%u/%u\n",
                   (unsigned)n.point, (unsigned)n.dbg[0], (unsigned)n.dbg[1],
                   (unsigned)n.dbg[2], (unsigned)n.dbg[3]);
        }
    }
}

static void op_save32(Ulong data)
{
    SNAP r, n;
    g_cases++;
    ref_SaveDebugData32(data);
    new_SaveDebugData32(data);
    memset(&r, 0, sizeof r);
    memset(&n, 0, sizeof n);
    snapshot_ref(&r);
    snapshot_new(&n);
    if (r.point != n.point || memcmp(r.dbg, n.dbg, sizeof r.dbg) != 0) {
        g_fail++;
        if (g_fail <= 20) {
            printf("MISMATCH SaveDebugData32 data=%lu (0x%lx)\n",
                   (unsigned long)data, (unsigned long)data);
            printf("  ref point=%u words[0..5]=%u/%u/%u/%u/%u/%u\n",
                   (unsigned)r.point, (unsigned)r.dbg[0], (unsigned)r.dbg[1],
                   (unsigned)r.dbg[2], (unsigned)r.dbg[3],
                   (unsigned)r.dbg[4], (unsigned)r.dbg[5]);
            printf("  new point=%u words[0..5]=%u/%u/%u/%u/%u/%u\n",
                   (unsigned)n.point, (unsigned)n.dbg[0], (unsigned)n.dbg[1],
                   (unsigned)n.dbg[2], (unsigned)n.dbg[3],
                   (unsigned)n.dbg[4], (unsigned)n.dbg[5]);
        }
    }
}

static void op_reset(void)
{
    SNAP r, n;
    g_cases++;
    ref_ResetDebugBuffer();
    new_ResetDebugBuffer();
    memset(&r, 0, sizeof r);
    memset(&n, 0, sizeof n);
    snapshot_ref(&r);
    snapshot_new(&n);
    if (r.point != n.point || memcmp(r.dbg, n.dbg, sizeof r.dbg) != 0) {
        g_fail++;
        if (g_fail <= 20) {
            printf("MISMATCH ResetDebugBuffer ref point=%u new point=%u\n",
                   (unsigned)r.point, (unsigned)n.point);
        }
    }
}

static void op_dsdeal(Uint pos)
{
    SNAP r, n;
    g_cases++;
    ref_DebugSaveDeal(pos);
    new_DebugSaveDeal(pos);
    memset(&r, 0, sizeof r);
    memset(&n, 0, sizeof n);
    snapshot_ref(&r);
    snapshot_new(&n);
    if (memcmp(&r, &n, sizeof r) != 0) {
        g_fail++;
        if (g_fail <= 20) {
            printf("MISMATCH DebugSaveDeal pos=%u\n", (unsigned)pos);
            show("ref", &r);
            show("new", &n);
        }
    }
}

/* ---------------- value sets ---------------- */

static const Uint V_POINT[] = { 0, 1, 2, 3, 497, 498, 499, 500, 501, 997, 998,
                               999, 1000, 1001, 4096, 65535 };
#define NPOINT ((int)(sizeof V_POINT / sizeof V_POINT[0]))

static const Uint V_DATA[] = { 0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000,
                               0x12345678 };
#define NDATA ((int)(sizeof V_DATA / sizeof V_DATA[0]))

static const Ulong V_LONG[] = { 0UL, 1UL, 2UL, 0xFFFFUL, 0x10000UL,
                                0x7FFFFFFFUL, 0x80000000UL, 0xFFFFFFFFUL,
                                0xDEADBEEFUL };
#define NLONG ((int)(sizeof V_LONG / sizeof V_LONG[0]))

/* TestData17 values and, for each, the SaveTmCnt values that sit exactly on
 * and around every divisor of half of it. */
static const int V_TD17[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 16, 100, -1,
                              -4, 32767, -32768 };
#define NTD17 ((int)(sizeof V_TD17 / sizeof V_TD17[0]))

static const Uint V_POS[] = { 0, 1, 2, 3, 4, 5, 32767, 65535 };
#define NPOS ((int)(sizeof V_POS / sizeof V_POS[0]))

static void sweep_save16(void)
{
    int a, b;
    for (a = 0; a < NPOINT; a++)
    for (b = 0; b < NDATA; b++) {
        SNAP s;
        memset(&s, 0, sizeof s);
        s.point = V_POINT[a];
        install(&s);
        op_save16(V_DATA[b]);
    }
}

static void sweep_save32(void)
{
    int a, b;
    for (a = 0; a < NPOINT; a++)
    for (b = 0; b < NLONG; b++) {
        SNAP s;
        memset(&s, 0, sizeof s);
        s.point = V_POINT[a];
        s.flag = 1;
        install(&s);
        op_save32(V_LONG[b]);
    }
}

static void sweep_reset(void)
{
    int a;
    for (a = 0; a < 8; a++) {
        SNAP s;
        int i;
        memset(&s, 0, sizeof s);
        for (i = 0; i < C_DBG_SIZE; i++)
            s.dbg[i] = (Uint16)(a ? (0xA500 + i) : 0);
        s.point = (Uint)(a * 137);
        install(&s);
        op_reset();
    }
}

/* DebugSaveDeal's own gates.  The SaveTmCnt axis is built from the divisor set
 * of td17/2 for each td17, because the half-period gate only disagrees between
 * `>=` and `>` when SaveTmCnt lands exactly on TestData17/2. */
static void sweep_dsdeal_gates(void)
{
    int a, b, c, d, e, f, g;
    for (a = 0; a < NTD17; a++) {
        int td17 = V_TD17[a];
        int half = td17 / 2;
        for (b = 0; b < 5; b++) {          /* SaveTmCnt around half */
            int tm;
            if (half < 0) tm = 0;
            else switch (b) {
                case 0:  tm = half - 2; break;
                case 1:  tm = half - 1; break;
                case 2:  tm = half;     break;
                case 3:  tm = half + 1; break;
                default: tm = half + 2; break;
            }
            if (tm < 0) continue;
            for (c = 0; c < 5; c++)        /* TestData12 */
            for (d = 0; d < 2; d++)        /* Start bit */
            for (e = 0; e < 2; e++)        /* TestData14 */
            for (f = 0; f < 3; f++)        /* TestData13 */
            for (g = 0; g < NPOS; g++) {   /* savePrgPos */
                SNAP s;
                IN in;
                memset(&s, 0, sizeof s);
                memset(&in, 0, sizeof in);
                in.td12 = (c == 4) ? -1 : c;
                in.td13 = f;
                in.td14 = e;
                in.td15 = 0;
                in.td17 = td17;
                in.start = d;
                install_in(&in);
                s.tmCnt = (Uint)tm;
                s.rstMem = 0;
                s.point = (g == 0) ? C_DBG_SIZE : 3;
                install(&s);
                op_dsdeal(V_POS[g]);
            }
        }
    }
}

/* The `if(save) { ... }` curve-counting block.
 *
 * The first run of this sweep missed every mutation inside that block, and the
 * reason was a hole in THIS sweep, not in the code: it only swept the gates, so
 * saveIndexP was always 0 and saveIndexFunc always 0.  With both at zero the
 * pointer path saves nothing and the bit path counts nothing, `saveCurveNum`
 * stays 0 and the whole block is invisible -- exactly the "常量必须同时是输入"
 * trap.  Here the three quantities the block reads are all swept. */
static void sweep_save_block(void)
{
    static const int   V_TD12[]  = { 0, 1, 2, 3, 7, -1 };
    static const Ulong V_POS17[] = { 0, 1, 2, 3, 4, 5, 8, 16, 100 };
    static const Ulong V_IDXP[]  = { 0, 1, 2, 3, 4, 5, 32 };
    static const Ulong V_IDXF[]  = { 0, 1, 2, 0x80000000UL, 0xFFFFFFFFUL,
                                     0x55555555UL, 0xAAAAAAAAUL, 0x0000FFFFUL,
                                     0xFFFF0000UL, 0x00010001UL };
    int a, b, c, d, e, f;
    for (a = 0; a < 9; a++)            /* td17, with pos == td17 so the gate passes */
    for (b = 0; b < 7; b++)            /* saveIndexP */
    for (c = 0; c < 10; c++)           /* saveIndexFunc */
    for (d = 0; d < 3; d++)            /* TestData13 */
    for (e = 0; e < 6; e++)            /* TestData12 */
    for (f = 0; f < 2; f++) {          /* Start bit */
        SNAP s;
        IN in;
        memset(&s, 0, sizeof s);
        memset(&in, 0, sizeof in);
        in.td12 = V_TD12[e];
        in.td13 = d;
        in.td14 = 0;
        in.td15 = 0;
        in.td17 = (int)V_POS17[a];
        in.start = f;
        install_in(&in);
        s.indexP = V_IDXP[b];
        s.indexFunc = V_IDXF[c];
        s.point = 40;                  /* mid-buffer: writes are visible */
        install(&s);
        op_dsdeal((Uint)V_POS17[a]);
    }
}

/* The `saveRstMem` latch and the ResetDebugBuffer() call inside
 * DebugSaveDeal.  Both edges matter: TestData15 rising while saveRstMem is 0
 * (the reset fires) and falling while it is non-zero (the latch clears).  A
 * single call can only exercise one edge, so this is driven as a **sequence**
 * against one installed state -- the same trick that makes the SaveTmCnt
 * half-period gate reachable. */
static void sweep_reset_latch(void)
{
    static const int   V_TD15[][4] = {
        { 0, 1, 1, 0 }, { 1, 1, 0, 0 }, { 0, 0, 1, 1 }, { 1, 0, 1, 0 },
        { 1, 1, 1, 0 }, { 0, 1, 0, 1 }, { 1, 1, 1, 1 }, { 0, 0, 0, 0 },
    };
    static const int  V_RST[] = { 0, 1, 5, 32767 };
    static const Ulong V_POS17[] = { 0, 1, 2, 3, 4 };
    int p, r, s, k;

    for (p = 0; p < 5; p++)
    for (r = 0; r < 4; r++)
    for (s = 0; s < 8; s++) {
        SNAP st;
        IN in;
        memset(&st, 0, sizeof st);
        memset(&in, 0, sizeof in);
        in.td12 = 1;                   /* save = 1: the reset still has to fire */
        in.td13 = 0;
        in.td14 = 0;
        in.td17 = (int)V_POS17[p];
        in.start = 1;
        st.rstMem = (Uint)V_RST[r];
        st.point = 60;
        st.curveNum = 0x1234;
        install(&st);
        for (k = 0; k < 4; k++) {
            in.td15 = V_TD15[s][k];
            install_in(&in);
            op_dsdeal((Uint)V_POS17[p]);
        }
    }
}

/* Random state + a random sequence of operations against it, comparing after
 * every call.  This is what covers the interactions: DebugSaveDeal's point
 * clamp feeding SaveDebugData16, ResetDebugBuffer in the middle of a run, and
 * the saveRstMem latch across several calls. */
static void random_cases(long n)
{
    long i;
    int k;
    Uint pos;
    for (i = 0; i < n; i++) {
        SNAP s;
        IN in;
        int steps = 1 + (int)(rnd32() % 6);

        memset(&s, 0, sizeof s);
        memset(&in, 0, sizeof in);

        if (rnd32() & 1) {
            int j;
            for (j = 0; j < C_DBG_SIZE; j++)
                s.dbg[j] = (Uint16)(rnd32() & 0xFFFF);
        }
        s.point    = rndUint() % (C_DBG_SIZE + 8);
        s.flag     = (int)(rnd32() & 3);
        s.indexFunc = ((Ulong)rnd32() << 16) | rnd32();
        s.indexP   = (Ulong)(rnd32() % 6);
        s.curveNum = rndUint();
        s.rstMem   = rndUint();
        s.tmCnt    = rndUint();
        s.startSave = rnd16();

        in.td12 = rnd16() % 8;
        in.td13 = (int)(rnd32() & 1);
        in.td14 = (int)(rnd32() & 1);
        in.td15 = (int)(rnd32() & 1);
        in.td17 = (rnd32() & 1) ? (int)(rnd32() % 9) : rnd16();
        in.start = (int)(rnd32() & 1);
        /* Half the time make the position gate pass at all, otherwise `pos` is
         * a random 16-bit value against a random (or tiny) td17 and
         * DebugSaveDeal returns at the second gate almost every time -- which
         * would leave the whole tail of the function untested. */
        pos = rndUint();
        if (rnd32() & 1) {
            int td = (int)(rnd32() % 9);
            if (rnd32() & 1) in.td17 = td;
            if (rnd32() & 1) pos = (Uint)td;
            else if (td <= 3) pos = 2;
        }

        install_in(&in);
        install(&s);

        for (k = 0; k < steps; k++) {
            switch (rnd32() % 4) {
            case 0: op_save16(rndUint()); break;
            case 1: op_save32(((Ulong)rnd32() << 16) | rnd32()); break;
            case 2: op_reset(); break;
            default: op_dsdeal(pos); break;
            }
        }
    }
}

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 20000L;

    sweep_save16();
    sweep_save32();
    sweep_reset();
    sweep_dsdeal_gates();
    sweep_save_block();
    sweep_reset_latch();
    random_cases(n);

    printf("MotorDebug difftest: cases=%ld mismatches=%ld\n", g_cases, g_fail);
    return g_fail ? 1 : 0;
}
