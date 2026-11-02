/* Differential test driver for f_plc.
 *
 * The module is a PLC/CAN access-control layer: it validates a funcCode address
 * against the project's attribute tables, maintains four translation/record
 * tables, and builds CAN frames. Nothing in it touches a peripheral register
 * directly -- the entire hardware boundary is three functions
 * (CanControlWriter / CanControlRead / CanlinkDataTran), which the shared mock
 * records as an ordered call log.
 *
 * What is compared after every call:
 *
 *   return value                    the three Uint16-returning entry points
 *   dataPi                          the whole CANLINK_DATA_BUF, every word
 *   funcCode                        the entire union, every word
 *   InvTrFastTab / InvTrSlowTab     the two translation tables
 *   InvRecFastTab / InvRecSlowTab   the two record tables
 *   InvTrFastIndex / InvTrSlowIndex the two rotating indices
 *   canLog                          the hardware call sequence, in order
 *
 * Why the call log matters: the three hardware functions return void or a status
 * the module ignores, so none of the compared state would change if a rewrite
 * dropped a call or passed it the wrong address. The log is what makes
 * "少调一次 / 调错参数 / 顺序变了" observable at all.
 *
 * funcCode is SHARED state, not per-side: it is defined once in mock_plc.c, which
 * also owns the project's two grade tables (funcCodeGradeAll / funcCodeGradeSum).
 * Those tables live in src/function/f_funcCode.c on the target, but that file
 * cannot be linked here -- its static initialisers are differences of two address
 * constants, which MSVC rejects (C2099). mock_plc.c therefore rebuilds
 * funcCodeGradeSum[] from a sizeof-based formulation, and phase 0 below proves the
 * equivalence by re-deriving all 83 entries through the module's own GetCodeIndex.
 * The driver restores funcCode from one snapshot before each side runs, so both
 * sides still see identical inputs.
 *
 * ---- pre-existing out-of-bounds reads ----
 *
 * PlcDataFramDeal() reads past the end of all four tables on paths the firmware
 * can reach; the case README lists the reachable index sets. Those reads are
 * undefined behaviour and the host cannot reproduce what the target's adjacent
 * memory holds, so this driver deliberately keeps the tables such that the reads
 * stay in bounds:
 *
 *   InvTrFastTab[6]   == 0   blocks InvTrFastTab[7]   at InvTrFastIndex == 4
 *   InvTrSlowTab[30]  == 0   blocks InvTrSlowTab[31]  at InvTrSlowIndex == 28
 *   InvRecFastTab[6]  == 0   when addr >= 5, blocks InvRecFastTab[7] / [8]
 *   InvRecSlowTab[30] == 0   when addr >= 29, blocks InvRecSlowTab[31] / [32]
 *
 * Those four are enforced and asserted in enforce_bounds(), so relaxing them
 * cannot happen silently. Under them, every branch of the depth chain ([+1] /
 * [+2] / [+3] taken or not), both branches of the index update (reset vs += 3)
 * and the "record entry is 0" early-out remain covered.
 *
 * What is NOT covered is the out-of-bounds read itself -- the values the
 * target's adjacent memory happens to hold when the chain is entered at an
 * index where [+3] runs past the end. Those reads are undefined behaviour, so
 * no host driver can reproduce them; this driver stays inside the tables by
 * construction and lists the gap instead of quietly dropping it.
 *
 * Phase 4 (phase_index) is the one place that does not call enforce_bounds():
 * it derives its depth cap from the table length instead of hard-coding the
 * guard, so its reads are in bounds by construction. It exists because the two
 * rotating indices and the two update branches were otherwise only reachable by
 * luck -- see the comment above phase_index().
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Same include order as src/function/f_plc.c. */
#include "f_funcCode.h"
#include "f_dspcan.h"
#include "f_canlink.h"
#include "f_comm.h"
#include "f_plc.h"

#include "mock_plc.h"
#include "grade_tables.inc"

#define TAB_FAST_N   (INV_TRAN_FAST_NUM + 1)
#define TAB_SLOW_N   (INV_TRAN_SLOW_NUM + 1)
#define CAN_SNAP_N   16
#define FAIL_LIMIT   20

extern Uint16 ref_InvTrFastIndex, ref_InvTrSlowIndex;
extern Uint16 new_InvTrFastIndex, new_InvTrSlowIndex;
extern Uint16 ref_InvTrFastTab[], ref_InvTrSlowTab[];
extern Uint16 ref_InvRecFastTab[], ref_InvRecSlowTab[];
extern Uint16 new_InvTrFastTab[], new_InvTrSlowTab[];
extern Uint16 new_InvRecFastTab[], new_InvRecSlowTab[];

extern Uint16 ref_FuncCodeRwAtrrib(Uint16 addr, Uint16 rwMode);
extern Uint16 new_FuncCodeRwAtrrib(Uint16 addr, Uint16 rwMode);
extern Uint16 ref_InvTranTabCfg(struct CANLINK_DATA_BUF *dataPi);
extern Uint16 new_InvTranTabCfg(struct CANLINK_DATA_BUF *dataPi);
extern Uint16 ref_InvRecTabCfg(struct CANLINK_DATA_BUF *dataPi);
extern Uint16 new_InvRecTabCfg(struct CANLINK_DATA_BUF *dataPi);
extern void   ref_PlcDataFramDeal(struct CANLINK_DATA_BUF *dataPi);
extern void   new_PlcDataFramDeal(struct CANLINK_DATA_BUF *dataPi);

/* ---- side plumbing ------------------------------------------------------- */

static Uint16 *g_trFast[2], *g_trSlow[2], *g_recFast[2], *g_recSlow[2];
static Uint16 *g_idxFast[2], *g_idxSlow[2];

static Uint16 (*g_rwAtrrib[2])(Uint16, Uint16);
static Uint16 (*g_tranCfg[2])(struct CANLINK_DATA_BUF *);
static Uint16 (*g_recCfg[2])(struct CANLINK_DATA_BUF *);
static void   (*g_framDeal[2])(struct CANLINK_DATA_BUF *);

static void init_ptrs(void)
{
    g_trFast[0]  = ref_InvTrFastTab;   g_trFast[1]  = new_InvTrFastTab;
    g_trSlow[0]  = ref_InvTrSlowTab;   g_trSlow[1]  = new_InvTrSlowTab;
    g_recFast[0] = ref_InvRecFastTab;  g_recFast[1] = new_InvRecFastTab;
    g_recSlow[0] = ref_InvRecSlowTab;  g_recSlow[1] = new_InvRecSlowTab;
    g_idxFast[0] = &ref_InvTrFastIndex; g_idxFast[1] = &new_InvTrFastIndex;
    g_idxSlow[0] = &ref_InvTrSlowIndex; g_idxSlow[1] = &new_InvTrSlowIndex;

    g_rwAtrrib[0] = ref_FuncCodeRwAtrrib; g_rwAtrrib[1] = new_FuncCodeRwAtrrib;
    g_tranCfg[0]  = ref_InvTranTabCfg;    g_tranCfg[1]  = new_InvTranTabCfg;
    g_recCfg[0]   = ref_InvRecTabCfg;     g_recCfg[1]   = new_InvRecTabCfg;
    g_framDeal[0] = ref_PlcDataFramDeal;  g_framDeal[1] = new_PlcDataFramDeal;
}

struct SIDE
{
    Uint16 ret;
    struct CANLINK_DATA_BUF buf;
    FUNCCODE_ALL fc;
    Uint16 idxFast, idxSlow;
    Uint16 trFast[TAB_FAST_N];
    Uint16 trSlow[TAB_SLOW_N];
    Uint16 recFast[TAB_FAST_N];
    Uint16 recSlow[TAB_SLOW_N];
    Uint16 canN;
    struct CANCALL can[CAN_SNAP_N];
};

static struct SIDE S[2];

/* ---- input state -------------------------------------------------------- */

static struct CANLINK_DATA_BUF g_inBuf;
static FUNCCODE_ALL g_inFc;
static Uint16 g_inTrFast[TAB_FAST_N];
static Uint16 g_inTrSlow[TAB_SLOW_N];
static Uint16 g_inRecFast[TAB_FAST_N];
static Uint16 g_inRecSlow[TAB_SLOW_N];
static Uint16 g_inIdxFast, g_inIdxSlow;

static void install(int side)
{
    memcpy(&funcCode, &g_inFc, sizeof g_inFc);
    memcpy(g_trFast[side], g_inTrFast, sizeof g_inTrFast);
    memcpy(g_trSlow[side], g_inTrSlow, sizeof g_inTrSlow);
    memcpy(g_recFast[side], g_inRecFast, sizeof g_inRecFast);
    memcpy(g_recSlow[side], g_inRecSlow, sizeof g_inRecSlow);
    *g_idxFast[side] = g_inIdxFast;
    *g_idxSlow[side] = g_inIdxSlow;
    canLogN = 0;
}

static void snapshot(int side)
{
    struct SIDE *s = &S[side];

    memcpy(&s->fc, &funcCode, sizeof s->fc);
    memcpy(&s->buf, &g_inBuf, sizeof s->buf);
    s->idxFast = *g_idxFast[side];
    s->idxSlow = *g_idxSlow[side];
    memcpy(s->trFast, g_trFast[side], sizeof s->trFast);
    memcpy(s->trSlow, g_trSlow[side], sizeof s->trSlow);
    memcpy(s->recFast, g_recFast[side], sizeof s->recFast);
    memcpy(s->recSlow, g_recSlow[side], sizeof s->recSlow);

    if (canLogN > CAN_SNAP_N)
    {
        printf("FATAL: canLog overflow (%u > %d)\n", canLogN, CAN_SNAP_N);
        exit(2);
    }
    s->canN = canLogN;
    memcpy(s->can, canLog, canLogN * sizeof(struct CANCALL));
}

/* ---- case runner -------------------------------------------------------- */

static long g_cases = 0;
static int g_fails = 0;

static void report(const char *fn, const char *what)
{
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s: %s\n", fn, what);
    printf("  in: mdl.datal=%u mdl.datah=%u mdh.datal=%u mdh.datah=%u\n",
           g_inBuf.mdl.data.datal, g_inBuf.mdl.data.datah,
           g_inBuf.mdh.data.datal, g_inBuf.mdh.data.datah);
    printf("      idxFast=%u idxSlow=%u\n", g_inIdxFast, g_inIdxSlow);
}

/* kind: 0 = FuncCodeRwAtrrib, 1 = InvTranTabCfg, 2 = InvRecTabCfg,
 *       3 = PlcDataFramDeal.  Only kind 0 has no buffer argument. */
static void run_case(const char *fn, int kind, Uint16 rwMode)
{
    int side;

    g_cases++;

    for (side = 0; side < 2; side++)
    {
        install(side);
        S[side].buf = g_inBuf;

        switch (kind)
        {
        case 0:
            S[side].ret = g_rwAtrrib[side](g_inBuf.mdl.data.datal, rwMode);
            break;
        case 1:
            S[side].ret = g_tranCfg[side](&S[side].buf);
            break;
        case 2:
            S[side].ret = g_recCfg[side](&S[side].buf);
            break;
        default:
            g_framDeal[side](&S[side].buf);
            S[side].ret = 0;
            break;
        }

        snapshot(side);
    }

    if (S[0].ret != S[1].ret)
    {
        report(fn, "return value");
        return;
    }
    if (memcmp(&S[0].buf, &S[1].buf, sizeof S[0].buf) != 0)
    {
        report(fn, "dataPi");
        return;
    }
    if (memcmp(&S[0].fc, &S[1].fc, sizeof S[0].fc) != 0)
    {
        report(fn, "funcCode");
        return;
    }
    if (S[0].idxFast != S[1].idxFast)
    {
        report(fn, "InvTrFastIndex");
        return;
    }
    if (S[0].idxSlow != S[1].idxSlow)
    {
        report(fn, "InvTrSlowIndex");
        return;
    }
    if (memcmp(S[0].trFast, S[1].trFast, sizeof S[0].trFast) != 0)
    {
        report(fn, "InvTrFastTab");
        return;
    }
    if (memcmp(S[0].trSlow, S[1].trSlow, sizeof S[0].trSlow) != 0)
    {
        report(fn, "InvTrSlowTab");
        return;
    }
    if (memcmp(S[0].recFast, S[1].recFast, sizeof S[0].recFast) != 0)
    {
        report(fn, "InvRecFastTab");
        return;
    }
    if (memcmp(S[0].recSlow, S[1].recSlow, sizeof S[0].recSlow) != 0)
    {
        report(fn, "InvRecSlowTab");
        return;
    }
    if (S[0].canN != S[1].canN)
    {
        report(fn, "CAN call count");
        return;
    }
    if (S[0].canN && memcmp(S[0].can, S[1].can, S[0].canN * sizeof(struct CANCALL)) != 0)
    {
        report(fn, "CAN call log");
        return;
    }
}

/* ---- input construction -------------------------------------------------- */

static Uint32 g_seed = 0x12345678UL;

static Uint16 rnd(void)
{
    g_seed = g_seed * 1103515245UL + 12345UL;
    return (Uint16)(g_seed >> 16);
}

/* Zero everything, then let the caller fill in the fields it cares about. */
static void reset_input(void)
{
    memset(&g_inBuf, 0, sizeof g_inBuf);
    memset(&g_inFc, 0, sizeof g_inFc);
    memset(g_inTrFast, 0, sizeof g_inTrFast);
    memset(g_inTrSlow, 0, sizeof g_inTrSlow);
    memset(g_inRecFast, 0, sizeof g_inRecFast);
    memset(g_inRecSlow, 0, sizeof g_inRecSlow);
    g_inIdxFast = 1;
    g_inIdxSlow = 1;
}

/* The four table slots that must stay zero so the module's pre-existing
 * out-of-bounds reads are not reached. Asserted rather than assumed -- if a
 * future edit relaxes one of these, the test must stop instead of silently
 * reading undefined memory. */
static void enforce_bounds(Uint16 addr)
{
    g_inTrFast[TAB_FAST_N - 1] = 0;
    g_inTrSlow[TAB_SLOW_N - 1] = 0;

    if ((addr > 0) && (addr <= INV_REC_FAST_NUM))
    {
        if (addr >= (INV_REC_FAST_NUM - 1))
        {
            g_inRecFast[TAB_FAST_N - 1] = 0;
        }
    }
    else if ((addr > INV_SLOW_TAB_ADDR)
             && (addr <= (INV_SLOW_TAB_ADDR + INV_REC_SLOW_NUM)))
    {
        if ((addr - INV_SLOW_TAB_ADDR) >= (INV_REC_SLOW_NUM - 1))
        {
            g_inRecSlow[TAB_SLOW_N - 1] = 0;
        }
    }
}

/* ---- phase 0: funcCodeGradeSum[] == GetCodeIndex() ----------------------- */

/* grade_tables.inc holds the 83 member designators of funcCodeGradeSum[] exactly
 * once, as GRADE_SUM_LIST. mock_plc.c builds the table from a sizeof-based
 * constant formulation (see its comment); here the same list is expanded through
 * the module's own GetCodeIndex to produce *run-time* values, and the two are
 * compared element by element.
 *
 * This phase exists because the substitution is load-bearing, not cosmetic:
 * FuncCodeRwAtrrib calls GetGradeIndex(group, grade), which is
 * `funcCodeGradeSum[group] + grade`. If the rebuilt table disagreed with what
 * GetCodeIndex computes, every access-control decision in the module would be
 * taken against a wrong index and phase 1 would be comparing two sides that are
 * both wrong. So the equivalence has to be measured, not assumed.
 *
 * It also guards a structural invariant: the largest index must still land inside
 * the union, otherwise funcCodeGradeSum[] would be pointing outside funcCode. */
static void phase_grade_sum(void)
{
    Uint16 idx[FUNCCODE_GROUP_NUM];
    Uint16 i = 0;
    Uint16 bad = 0;

    /* Non-static on purpose: a static array would need an address-difference
     * initialiser, which MSVC rejects -- that is the very reason this whole
     * offsetof scheme exists. */
#define GRADE_SUM_DRIVER_ENTRY(m)   idx[i++] = GetCodeIndex(funcCode.m);
    GRADE_SUM_LIST(GRADE_SUM_DRIVER_ENTRY)
#undef GRADE_SUM_DRIVER_ENTRY

    if (i != FUNCCODE_GROUP_NUM)
    {
        printf("FATAL: GRADE_SUM_LIST 展开了 %u 项, 期望 %u\n", i, FUNCCODE_GROUP_NUM);
        exit(2);
    }

    for (i = 0; i < FUNCCODE_GROUP_NUM; i++)
    {
        if (funcCodeGradeSum[i] != idx[i])
        {
            if (bad < 8)
            {
                printf("FATAL: funcCodeGradeSum[%u]=%u, GetCodeIndex 得 %u\n",
                       i, funcCodeGradeSum[i], idx[i]);
            }
            bad++;
        }
    }
    if (bad)
    {
        printf("FATAL: offsetof 表与 GetCodeIndex 有 %u 处不一致 —— "
               "mock 里的等级表不可信, 后续比对没有意义\n", bad);
        exit(2);
    }
    if (idx[FUNCCODE_GROUP_NUM - 1] >= FNUM_ALL)
    {
        printf("FATAL: 末项索引 %u 越出 funcCode (%u 字)\n",
               idx[FUNCCODE_GROUP_NUM - 1], FNUM_ALL);
        exit(2);
    }

    printf("  grade table: 83/83 与 GetCodeIndex 一致, 末项索引 %u / %u\n",
           idx[FUNCCODE_GROUP_NUM - 1], FNUM_ALL);
}

/* ---- phase 1: FuncCodeRwAtrrib, exhaustive over addr --------------------- */

static void phase_rwattr(void)
{
    Uint32 addr;
    Uint16 mode;
    Uint16 i;

    /* Two grade tables. 0 = the project's real funcCodeGradeAll (so the decision
     * is taken against the real thresholds); 1 = every group wide open, which
     * pushes every address past the first check and so exercises the
     * forbidden-index check on the widest possible input set. */
    for (i = 0; i < 2; i++)
    {
        if (i == 0)
        {
            mockUseOverride = 0;
        }
        else
        {
            Uint16 g;

            mockUseOverride = 1;
            for (g = 0; g < FUNCCODE_GROUP_NUM; g++)
            {
                mockFuncCodeGradeOverride[g] = 0xFFFF;
            }
        }

        for (mode = 0; mode <= 1; mode++)
        {
            for (addr = 0; addr <= 0xFFFFUL; addr++)
            {
                reset_input();
                g_inBuf.mdl.data.datal = (Uint16)addr;
                run_case("FuncCodeRwAtrrib", 0, mode);
            }
        }
    }

    mockUseOverride = 0;
}

/* ---- phase 2: InvTranTabCfg / InvRecTabCfg ------------------------------- */

/* Addresses chosen to land in each branch of FuncCodeRwAtrrib: 0 (the early
 * out), each accepted high nibble, the 0x7300 write-only window, and a few
 * rejects. The table-config functions call FuncCodeRwAtrrib first and bail out
 * when it fails, so these decide which of them reach the table write. */
static const Uint16 ADDRS[] = {
    0x0000, 0x7000, 0x7001, 0x7300, 0x7301, 0x7305, 0x1234,
    0xA000, 0xA001, 0xB000, 0xB0FF, 0xC000, 0xC001, 0xF000, 0xFFFF,
};
#define NADDR (sizeof ADDRS / sizeof ADDRS[0])

static void phase_tabcfg(void)
{
    Uint32 datah;
    Uint16 k;

    for (datah = 0; datah <= 0x200; datah++)
    {
        for (k = 0; k < NADDR; k++)
        {
            reset_input();
            g_inBuf.mdl.data.datah = (Uint16)datah;
            g_inBuf.mdl.data.datal = ADDRS[k];
            run_case("InvTranTabCfg", 1, 0);

            reset_input();
            g_inBuf.mdl.data.datah = (Uint16)datah;
            g_inBuf.mdl.data.datal = ADDRS[k];
            run_case("InvRecTabCfg", 2, 0);
        }
    }

    /* Randomised: the tables start non-zero so the write path is taken with
     * existing content, and datah covers the whole 16-bit range. */
    for (k = 0; k < 20000; k++)
    {
        Uint16 i;

        reset_input();
        for (i = 0; i < TAB_FAST_N; i++)
        {
            g_inTrFast[i] = rnd();
            g_inRecFast[i] = rnd();
        }
        for (i = 0; i < TAB_SLOW_N; i++)
        {
            g_inTrSlow[i] = rnd();
            g_inRecSlow[i] = rnd();
        }
        g_inIdxFast = (Uint16)(1 + 3 * (rnd() % 2));
        g_inIdxSlow = (Uint16)(1 + 3 * (rnd() % 10));
        g_inBuf.mdl.data.datah = rnd();
        g_inBuf.mdl.data.datal = ADDRS[rnd() % NADDR];
        enforce_bounds(g_inBuf.mdl.data.datal);

        run_case("InvTranTabCfg", 1, 0);
        run_case("InvRecTabCfg", 2, 0);
    }
}

/* ---- phase 3: PlcDataFramDeal -------------------------------------------- */

/* Fill the two translation tables from a bit pattern, so the depth chain
 * ([+1] / [+2] / [+3] taken or not) is walked systematically rather than left to
 * chance. 32 patterns x both tables is enough to hit every combination the chain
 * can see. */
static void fill_tr(Uint16 *tr, Uint16 n, Uint32 pattern)
{
    Uint16 i;

    for (i = 0; i < n; i++)
    {
        tr[i] = (Uint16)((pattern >> (i % 16)) & 1u ? (Uint16)(0x4000 + i) : 0);
    }
}

static void phase_framedeal(void)
{
    Uint32 addr;
    Uint32 p;
    Uint16 k;

    /* (a) sweep the record-address space with a fixed pattern, both tables. */
    for (addr = 0; addr <= 0x200; addr++)
    {
        for (p = 0; p < 8; p++)
        {
            Uint16 i;

            reset_input();
            g_inBuf.mdl.data.datal = (Uint16)addr;
            g_inBuf.mdl.data.datah = (Uint16)(addr * 7 + 1);
            g_inBuf.mdh.data.datal = (Uint16)(addr * 13 + 3);
            g_inBuf.mdh.data.datah = (Uint16)(addr * 29 + 5);

            for (i = 0; i < TAB_FAST_N; i++)
            {
                g_inRecFast[i] = (Uint16)(((p >> i) & 1u) ? (0x5000 + i) : 0);
            }
            for (i = 0; i < TAB_SLOW_N; i++)
            {
                g_inRecSlow[i] = (Uint16)(((p >> (i % 8)) & 1u) ? (0x6000 + i) : 0);
            }
            fill_tr(g_inTrFast, TAB_FAST_N, p * 3 + 1);
            fill_tr(g_inTrSlow, TAB_SLOW_N, p * 5 + 2);

            enforce_bounds((Uint16)addr);
            run_case("PlcDataFramDeal", 3, 0);
        }
    }

    /* (b) sweep the rotation indices against the depth chain. */
    for (k = 0; k < 4096; k++)
    {
        Uint16 i;

        reset_input();
        g_inBuf.mdl.data.datal = (Uint16)(1 + (rnd() % (INV_REC_SLOW_NUM + 2)));
        g_inBuf.mdl.data.datah = rnd();
        g_inBuf.mdh.data.datal = rnd();
        g_inBuf.mdh.data.datah = rnd();

        for (i = 0; i < TAB_FAST_N; i++)
        {
            g_inRecFast[i] = rnd();
        }
        for (i = 0; i < TAB_SLOW_N; i++)
        {
            g_inRecSlow[i] = rnd();
        }
        fill_tr(g_inTrFast, TAB_FAST_N, rnd());
        fill_tr(g_inTrSlow, TAB_SLOW_N, rnd());

        g_inIdxFast = (Uint16)(1 + 3 * (rnd() % 2));
        g_inIdxSlow = (Uint16)(1 + 3 * (rnd() % 10));

        enforce_bounds(g_inBuf.mdl.data.datal);
        run_case("PlcDataFramDeal", 3, 0);
    }
}

/* ---- phase 4: rotation index across the wrap threshold ------------------- */

/* The two rotating indices are part of this function's input state, and the
 * update at the end of PlcDataFramDeal has exactly two branches: reset to 1, or
 * step by 3. The step branch is only taken when indexReset == 0, and indexReset
 * is only cleared when the depth chain reaches [+3] -- four consecutive
 * non-zero slots. Neither earlier phase produces that state on purpose:
 *
 *   phase (a) pins the index at 1 and fills the tables from 8 fixed bit
 *     patterns (p*3+1 and p*5+2); none of them has four consecutive bits set,
 *     so indexReset stays 1 and the update always resets;
 *   phase (b) sets datal to 1..(INV_REC_SLOW_NUM+2) = 1..32, which never lands
 *     in the slow range (INV_SLOW_TAB_ADDR+1 .. +INV_REC_SLOW_NUM = 257..286),
 *     so the slow branch is not exercised there at all.
 *
 * So the step size and the wrap threshold were never actually observed, and the
 * negative control found exactly that by MISSING the two mutations that change
 * them. This phase removes the luck: for every index the tables can be safely
 * probed at, it places a chain of a chosen depth (0..4) starting exactly at
 * that index, so both update branches and the threshold are hit deliberately.
 *
 * Bounds are computed rather than asserted. The module reads [idx+j] for
 * j <= depth capped at 3, so "idx + min(depth,3) <= last" is exactly the
 * condition for every read to stay inside the table. The depth cap below is
 * that condition solved for depth, and it automatically keeps the tail slot
 * zero whenever the index sits too close to it -- the same guard the other
 * phases hard-code, here derived instead. Two consequences worth stating:
 *
 *   idx == last-3 (fast 3, slow 27) is the last index where a 4-deep chain
 *     fits, and so the only index that can clear indexReset AND still take the
 *     step branch (the threshold test is "idx > last-3").
 *   idx == 4 (fast) and idx == 28 (slow) are the indices the firmware itself
 *     reaches; there the cap is 2, the chain stops at depth 2, indexReset stays
 *     1 and the update resets -- covered here with that cap.
 *
 * Reachability, stated rather than implied: the firmware only ever holds the
 * fast index at 1 or 4 (it starts at 1, and "idx > 3" wraps 4 back to 1), so
 * the fast threshold is only ever evaluated at those two values -- where "-3"
 * and "-4" agree, i.e. that mutation is harmless in the firmware's own state
 * space. It is still a real function-level difference, and it is caught here
 * through idx == 3, which the firmware's stepping never produces. The slow
 * index does reach the step branch (1,4,...,25 are all <= 27), so the slow
 * mutations are reachable ones. The case README records this split. */
static void phase_index(void)
{
    Uint16 k;

    for (k = 0; k < 2; k++)
    {
        Uint16 *tr  = k ? g_inTrSlow : g_inTrFast;
        Uint16 last = (Uint16)((k ? TAB_SLOW_N : TAB_FAST_N) - 1);
        Uint16 idx;

        for (idx = 0; idx <= last; idx++)
        {
            Uint16 room = (Uint16)(last - idx);
            Uint16 maxDepth;
            Uint16 depth;

            if (room >= 3)
                maxDepth = 4;
            else if (room == 2)
                maxDepth = 2;
            else if (room == 1)
                maxDepth = 1;
            else
                maxDepth = 0;

            for (depth = 0; depth <= maxDepth; depth++)
            {
                Uint16 i;

                /* reset_input() zeroes the buffer, funcCode and all four
                 * tables, and pins both indices at 1. */
                reset_input();

                for (i = 0; i < depth; i++)
                {
                    tr[idx + i] = (Uint16)(0x4000 + idx + i);
                }

                /* Record tables stay zero, so the CanControlWriter chain emits
                 * no calls and the index update is the only thing under test. */
                if (k)
                {
                    g_inBuf.mdl.data.datal = (Uint16)(INV_SLOW_TAB_ADDR + 1);
                    g_inIdxSlow = idx;
                }
                else
                {
                    g_inBuf.mdl.data.datal = 1;
                    g_inIdxFast = idx;
                }
                g_inBuf.mdl.data.datah = 0x3333;
                g_inBuf.mdh.data.datal = 0x1111;
                g_inBuf.mdh.data.datah = 0x2222;

                run_case("PlcDataFramDeal", 3, 0);
            }
        }
    }
}

/* ---- phase 5: the unrecognised-address counter --------------------------- */

/* When addr matches neither table range, PlcDataFramDeal bumps
 * funcCode.code.u3[11] and returns -- guarded by "< 65535", so the counter
 * saturates at 65535 instead of wrapping.
 *
 * The guard is only observable when the counter is already at the boundary, and
 * every earlier phase starts it at 0 (reset_input() zeroes funcCode, and nothing
 * else in the module writes it). So "< 65535" and "< 65534" are indistinguishable
 * everywhere else in this driver, and the negative control missed exactly the
 * mutation that changes it. Sweeping the counter's top values against one address
 * in each of the three regions fixes that: the counter has to be an input, not
 * always zero.
 *
 * The addresses cover all three outcomes: 0 is the early return, 1..6 the fast
 * table, 7..255 and 256 and 287..0xFFFF the counter path, 257..286 the slow
 * table. */
static void phase_counter(void)
{
    static const Uint16 CNT[] = { 0, 1, 65532, 65533, 65534, 65535 };
    static const Uint16 ADDR2[] = { 0, 1, 6, 7, 100, 256, 257, 286, 287, 0xFFFF };
    Uint16 c;
    Uint16 a;

    for (c = 0; c < (Uint16)(sizeof CNT / sizeof CNT[0]); c++)
    {
        for (a = 0; a < (Uint16)(sizeof ADDR2 / sizeof ADDR2[0]); a++)
        {
            reset_input();
            g_inFc.code.u3[11] = CNT[c];
            g_inBuf.mdl.data.datal = ADDR2[a];
            g_inBuf.mdl.data.datah = 0x3333;
            g_inBuf.mdh.data.datal = 0x1111;
            g_inBuf.mdh.data.datah = 0x2222;
            enforce_bounds(ADDR2[a]);
            run_case("PlcDataFramDeal", 3, 0);
        }
    }
}

/* ---- main ---------------------------------------------------------------- */

int main(void)
{
    long base;

    init_ptrs();

    phase_grade_sum();

    phase_rwattr();
    printf("  FuncCodeRwAtrrib (exhaustive) : %ld\n", g_cases);

    base = g_cases;
    phase_tabcfg();
    printf("  InvTranTabCfg/InvRecTabCfg   : %ld\n", g_cases - base);

    base = g_cases;
    phase_framedeal();
    printf("  PlcDataFramDeal              : %ld\n", g_cases - base);

    base = g_cases;
    phase_index();
    printf("  PlcDataFramDeal 索引扫描     : %ld\n", g_cases - base);

    base = g_cases;
    phase_counter();
    printf("  PlcDataFramDeal 未识别计数   : %ld\n", g_cases - base);

    printf("f_plc difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    return g_fails ? 1 : 0;
}
