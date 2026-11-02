/* Shared mock state for the f_plc differential test.
 *
 * One instance, read by both the ref and the new side, so both observe exactly the
 * same inputs and the same hardware responses. The module's own exports
 * (InvTrFastTab, InvRecSlowTab, InvTrFastIndex, ...) are duplicated per side by
 * ref_tu.c / new_tu.c -- those are what the test compares.
 *
 * Include order mirrors src/function/f_plc.c.
 */
#include "f_funcCode.h"
#include "f_dspcan.h"
#include "f_canlink.h"
#include "f_comm.h"
#include "f_plc.h"

#include "grade_tables.inc"
#include "mock_plc.h"

/* ---------------------------------------------------------------------------
 * funcCode
 *
 * Defined here with its REAL type (FUNCCODE_ALL from f_funcCode.h) rather than a
 * stand-in, because f_plc's access control (FuncCodeRwAtrrib) computes
 * GetCodeIndex(funcCode.code.<member>) and compares the result against an index
 * derived from the CAN address. Stand-in types would produce stand-in offsets and
 * the test would be checking nothing.
 *
 * Static storage duration, so it starts zeroed -- which is also the state the
 * target sees at power-up before EEPROM is read.
 * ------------------------------------------------------------------------ */
FUNCCODE_ALL funcCode;

/* ---------------------------------------------------------------------------
 * The two grade tables
 *
 * src/function/f_funcCode.c owns these on the target. Linking it into this test
 * was the first attempt and it does not work: funcCodeGradeSum[] is statically
 * initialised with GetCodeIndex(funcCode.group.fX[0]), which expands to a
 * difference of two address constants. MSVC's C front end rejects that (error
 * C2099), and so does the whole of funcCodeAttribute[]. host_patch.json cannot
 * help either -- it patches the ref/new copies of the module under test, not a
 * third-party data module.
 *
 * So the table is rebuilt from an equivalent *constant* formulation. Two
 * candidates were tried and measured before the one below was chosen; both are
 * recorded here because both look reasonable and neither works:
 *
 *   (1) offsetof(FUNCCODE_ALL, x) / sizeof(Uint16)
 *       MSVC folds offsetof (and &((T*)0)->m) to an ADDRESS constant, and then
 *       refuses to apply arithmetic to it. Measured with
 *       tools/difftest/probe_const_offset.py: `(U16)offsetof(...)`,
 *       `(U16)(size_t)&((T*)0)->m` and `(U16)((char*)&((T*)0)->m - (char*)0)`
 *       all compile, while any `/`, `>>` or difference of two such constants is
 *       C2099. So the division -- the one operation we actually need -- is the
 *       one operation that is refused.
 *
 *   (2) cumulative sum of funcCodeGradeAll[]
 *       funcCodeGradeAll[] counts how many *usable grade entries* a group has
 *       (FENUM - 2, U9NUM - 15, ...), not how long the group's array is. The two
 *       differ, so the prefix sums are not the group offsets.
 *
 * What does work is sizeof arithmetic, because sizeof is an integer constant
 * expression:
 *
 *   funcCodeGradeSum[k] == (sum of sizeof of the first k members) / sizeof(Uint16)
 *
 * struct FUNCCODE_GROUP is a flat sequence of Uint16 arrays -- no nesting, no
 * padding -- so a member's offset is exactly the total length of the members
 * before it. GRADE_SUM_INIT in grade_tables.inc builds that chain from the
 * header's own member list; nothing about the layout is duplicated by hand, and
 * the generator refuses to run if the 83 grade-sum entries and the 83 members
 * ever stop corresponding one-to-one in order.
 *
 * This equivalence is TESTED, not assumed: driver.c re-derives all 83 entries at
 * run time using the module's own GetCodeIndex and compares them against this
 * table element by element. It matters because FuncCodeRwAtrrib's GetGradeIndex()
 * reads funcCodeGradeSum[] directly -- if this table were wrong, the module under
 * test would be reading wrong indices.
 *
 * (On the host sizeof(Uint16) is 2 bytes; on C28x it is 1 word. Both sides of the
 * differential test are host-compiled and share this one table, so the test
 * compares host-consistent values -- which is the property that matters here.)
 * ------------------------------------------------------------------------ */
const Uint16 funcCodeGradeSum[FUNCCODE_GROUP_NUM] =
{
    GRADE_SUM_INIT
};

const Uint16 funcCodeGradeAll[FUNCCODE_GROUP_NUM] =
{
    GRADE_ALL_INIT
};

Uint16 mockUseOverride;
Uint16 mockFuncCodeGradeOverride[FUNCCODE_GROUP_NUM];

struct CANCALL canLog[CANLOG_N];
Uint16 canLogN;

/* UpdataFuncCodeGrade() lives in f_menu.c. Neither translation unit under test
 * defines it, so this stand-in is what both sides call.
 *
 * Default path copies the project's REAL funcCodeGradeAll, so the grade table the
 * logic sees is the real one. mockUseOverride lets the driver swap in a synthetic
 * table when it wants to probe a boundary the real table hides.
 *
 * Two deliberate simplifications, both of which must be kept in mind:
 *
 *  1. The real one memcpy's `FUNCCODE_GROUP_NUM` bytes. On C28x that is exactly
 *     83 *words* (sizeof returns 16-bit units there), so it copies the whole
 *     table. On the host the same literal is 83 *bytes*, which would copy only
 *     half of it. This loop copies element by element, matching the TARGET's
 *     behaviour -- i.e. it does the right thing rather than reproducing the
 *     host's unit confusion. That is a third instance of the sizeof-unit gap
 *     documented in the case README.
 *  2. The real one then zeroes whole groups according to funcParaView's digits
 *     (via GetNumberDigit / MotorDebugFcDeal). That part is not reproduced. So
 *     the grade table seen here is the un-gated one; the driver compensates by
 *     sweeping every (group, grade) pair directly.
 */
void UpdataFuncCodeGrade(Uint16 funcCodeGrade[])
{
    Uint16 i;

    if (mockUseOverride)
    {
        for (i = 0; i < FUNCCODE_GROUP_NUM; i++)
        {
            funcCodeGrade[i] = mockFuncCodeGradeOverride[i];
        }
    }
    else
    {
        for (i = 0; i < FUNCCODE_GROUP_NUM; i++)
        {
            funcCodeGrade[i] = funcCodeGradeAll[i];
        }
    }
}

/* ---- hardware stand-ins -------------------------------------------------- */

static void log_call(Uint16 kind, Uint16 a, Uint16 b, Uint16 c)
{
    if (canLogN < CANLOG_N)
    {
        struct CANCALL *e = &canLog[canLogN++];

        e->kind = kind;
        e->a = a;
        e->b = b;
        e->c = c;
        e->nword = 0;
    }
}

Uint16 CanControlWriter(Uint16 addr, Uint16 data, Uint16 eeprom)
{
    log_call(CANCALL_WRITER, addr, data, eeprom);
    return CANLINK_RT_SUCC;
}

/* The module does `CanControlRead(addr, &result)` and then copies *result into
 * the frame. The value has to be deterministic and distinct per address --
 * otherwise a rewrite that read the wrong address, or dropped the read entirely,
 * would still produce matching frames. */
Uint16 CanControlRead(Uint16 addr, Uint16 *result)
{
    Uint16 value = (Uint16)(0x1000 + (addr & 0x0FFF));

    if (result != 0)
    {
        *result = value;
    }
    log_call(CANCALL_READ, addr, value, 0);
    return CANLINK_RT_SUCC;
}

/* Records len / timeout and a snapshot of the frame, NOT the raw pointer.
 *
 * The module calls CanlinkDataTran((Uint32*)(dataPi), len, 1000) with its own
 * frame buffer. The ref side and the new side are handed two *different* local
 * buffers by the driver, so their addresses differ by construction. Logging the
 * pointer would turn "different addresses" into a reported behaviour difference
 * -- a false positive that would look like a real bug. The frame's *contents*
 * are the observable, and the driver compares those anyway.
 *
 * struct CANLINK_DATA_BUF is 16 bytes = 8 16-bit words, which is what CANSNAP_WORDS
 * covers. */
Uint16 CanlinkDataTran(Uint32 *dataPi, Uint16 len, Uint16 timeOut)
{
    const Uint16 *w = (const Uint16 *)dataPi;
    Uint16 i;

    if (canLogN < CANLOG_N)
    {
        struct CANCALL *e = &canLog[canLogN++];

        e->kind = CANCALL_TRAN;
        e->a = len;
        e->b = timeOut;
        e->c = 0;
        e->nword = CANSNAP_WORDS;
        for (i = 0; i < CANSNAP_WORDS; i++)
        {
            e->snap[i] = w[i];
        }
    }
    return CANLINK_RT_SUCC;
}
