/* Layout probe for the f_invPara differential test.
 *
 * ONE source, compiled twice:
 *   - host   (MSVC, -DHOST_PROBE): an executable that prints the numbers
 *   - target (cl2000, no macro):   an object whose g_layout lands in a section
 *                                  named "layout_probe", read back by
 *                                  tools/difftest/target_layout.py
 *
 * Why this case needs it more than the others: f_invPara.c does not merely read
 * funcCode -- it builds *array indices* out of funcCode's member addresses.
 * motor1ParaIndex[22] is a table of GetCodeIndex(...) values, and every write in
 * InverterTypeRelatedParaDeal / MotorPowerRelatedParaDeal / SaveTuneData is
 * `funcCode.all[<one of those values> + <motor stride>]`. If the host put those
 * members at different word offsets than the target, then the host test would be
 * exercising different array elements than the firmware does: still
 * self-consistent (both sides share the host layout), but no longer a faithful
 * proxy. So the offsets are measured, not assumed.
 *
 * The expression list appears exactly once, in LAYOUT_LIST. Both builds expand
 * the same list through different X macros, so a mismatch means the compilers
 * disagree about layout -- not that two hand-maintained lists drifted apart.
 * The label/unit list lives in cases/f_invPara/layout_labels.txt and must stay
 * in the same order; target_layout.py errors out if the counts disagree.
 *
 * GetCodeIndex is used verbatim rather than a hand-rolled WORD_OFF, because the
 * question is precisely "what does the module's own expression evaluate to".
 * Its operands here are all Uint16 members, so the pointer difference is already
 * in 16-bit units and needs no rescaling -- unlike f_motorFc's motorType, which
 * is an enum and scales by sizeof(enum) (2 on C28x, 4 on MSVC).
 *
 * The host path fills a local array at run time because MSVC rejects a
 * pointer-difference in a static initializer (C2099); the TI compiler folds it
 * happily, which is why the target path can use a const array.
 *
 * Include order mirrors src/function/f_invPara.c: f_invPara.h uses LOCALF/LOCALD
 * and F_DEBUG_RAM from f_debug.h without including it, and only gets away with
 * that because f_menu.h is included first (f_menu.h -> f_funcCode.h ->
 * f_debug.h). Same reason here.
 */
#include "f_menu.h"
#include "f_eeprom.h"
#include "f_invPara.h"

#define LAYOUT_LIST(X)                                                        \
    X((Uint16)sizeof(FUNCCODE_ALL))                                           \
    X((Uint16)sizeof(struct FUNCCODE_CODE))                                   \
    X((Uint16)FNUM_ALL)                                                       \
    X((Uint16)GetCodeIndex(funcCode.code.motorParaM1.elem.ratingPower))        \
    X((Uint16)GetCodeIndex(funcCode.code.motorParaM1.elem.ratingCurrent))      \
    X((Uint16)GetCodeIndex(funcCode.code.motorParaM1.elem.statorResistance))   \
    X((Uint16)GetCodeIndex(funcCode.code.motorParaM1.elem.rotorResistance))    \
    X((Uint16)GetCodeIndex(funcCode.code.motorParaM1.elem.leakInductance))     \
    X((Uint16)GetCodeIndex(funcCode.code.motorParaM1.elem.mutualInductance))   \
    X((Uint16)GetCodeIndex(funcCode.code.motorParaM1.elem.zeroLoadCurrent))    \
    X((Uint16)GetCodeIndex(funcCode.code.motorParaM1.elem.pmsmRs))             \
    X((Uint16)GetCodeIndex(funcCode.code.motorParaM1.elem.pmsmLd))             \
    X((Uint16)GetCodeIndex(funcCode.code.motorParaM1.elem.pmsmLq))             \
    X((Uint16)GetCodeIndex(funcCode.code.motorParaM1.elem.pmsmRsLdUnit))       \
    X((Uint16)GetCodeIndex(funcCode.code.motorParaM1.elem.pmsmCoeff))          \
    X((Uint16)GetCodeIndex(funcCode.code.vcParaM1.mAcrKp))                     \
    X((Uint16)GetCodeIndex(funcCode.code.vcParaM1.mAcrKi))                     \
    X((Uint16)GetCodeIndex(funcCode.code.vcParaM1.tAcrKp))                     \
    X((Uint16)GetCodeIndex(funcCode.code.vcParaM1.tAcrKi))                     \
    X((Uint16)GetCodeIndex(funcCode.code.motorParaM1.elem.pmsmCheckTime))      \
    X((Uint16)GetCodeIndex(funcCode.code.pgParaM1.elem.enCoderDir))            \
    X((Uint16)GetCodeIndex(funcCode.code.pgParaM1.elem.enCoderAngle))          \
    X((Uint16)GetCodeIndex(funcCode.code.uvGainWarp))                          \
    X((Uint16)GetCodeIndex(funcCode.code.pgParaM1.elem.uvwSignDir))            \
    X((Uint16)GetCodeIndex(funcCode.code.pgParaM1.elem.uvwSignAngle))          \
    X((Uint16)(GetCodeIndex(funcCode.code.motorFcM2.motorPara.all[0]) -        \
               GetCodeIndex(funcCode.code.motorParaM1.all[0])))                \
    X((Uint16)(GetCodeIndex(funcCode.code.motorFcM3.motorPara.all[0]) -        \
               GetCodeIndex(funcCode.code.motorParaM1.all[0])))                \
    X((Uint16)(GetCodeIndex(funcCode.code.motorFcM4.motorPara.all[0]) -        \
               GetCodeIndex(funcCode.code.motorParaM1.all[0])))

#define NLAYOUT 28

#ifdef HOST_PROBE

#include <stdio.h>

int main(void)
{
    Uint16 vals[40];
    int i = 0, j;

#define EMIT(e) vals[i++] = (e);
    LAYOUT_LIST(EMIT)
#undef EMIT

    printf("%d\n", i);
    for (j = 0; j < i; j++)
        printf("%u\n", (unsigned)vals[j]);
    return 0;
}

#else

/* Own section, so target_layout.py can find the array by name instead of
 * guessing which .econst is ours -- f_funcCode.h pulls in enough const data
 * that "the first .econst" is not a safe assumption. */
#pragma DATA_SECTION(g_layout, "layout_probe")

const Uint16 g_layout[NLAYOUT] =
{
#define EMIT(e) e,
    LAYOUT_LIST(EMIT)
#undef EMIT
};

#endif
