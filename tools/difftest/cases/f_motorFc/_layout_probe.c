/* Layout probe for the f_motorFc differential test.
 *
 * ONE source, compiled twice:
 *   - host   (MSVC, -DHOST_PROBE): an executable that prints the numbers
 *   - target (cl2000, no macro):   an object whose g_layout lands in .econst,
 *                                  read back byte-for-byte by target_layout.py
 *
 * The expression list appears exactly once, in LAYOUT_LIST. Both builds expand
 * the same list through different X macros, so a mismatch means the compilers
 * disagree about layout -- not that two hand-maintained lists drifted apart.
 *
 * Why this matters here: f_motorFc.c indexes funcCode.all[] by
 * GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.motorType). If the host
 * put that member at a different word offset than the target, the host test
 * would exercise a different array element than the firmware does. The
 * differential test would still be self-consistent (both sides share the host
 * layout), but the gap has to be measured rather than assumed.
 *
 * WORD_OFF is used for the offset entries instead of GetCodeIndex: GetCodeIndex
 * subtracts `enum MOTOR_SN *` from `Uint16 *`, so it scales the difference by
 * sizeof(enum) -- 2 on C28x, 4 on the host. Casting to Uint16* first yields the
 * true word offset on both. The last entry keeps the raw GetCodeIndex value so
 * the divergence it causes is visible.
 *
 * The host path fills a local array at run time because MSVC refuses a
 * pointer-difference in a static initializer (C2099); the TI compiler folds it
 * happily, which is why the target path can use a const array.
 */
#include "f_main.h"
#include "f_io.h"
#include "f_funcCode.h"

#define WORD_OFF(field) ((Uint16)((Uint16 *)&(field) - (Uint16 *)&funcCode.all[0]))

#define LAYOUT_LIST(X)                                                    \
    X((Uint16)sizeof(struct MOTOR_FC))                                    \
    X((Uint16)sizeof(struct MOTOR_PARA_STRUCT))                           \
    X((Uint16)sizeof(struct PG_PARA_STRUCT))                              \
    X((Uint16)sizeof(struct VC_PARA))                                     \
    X((Uint16)sizeof(struct DI_FUNC))                                     \
    X((Uint16)sizeof(FUNCCODE_ALL))                                       \
    X((Uint16)sizeof(struct FUNCCODE_CODE))                               \
    X((Uint16)FNUM_ALL)                                                   \
    X(WORD_OFF(funcCode.code.motorFcM2.motorPara.elem.motorType))          \
    X(WORD_OFF(funcCode.code.motorParaM1.elem.ratingSpeed))                \
    X(WORD_OFF(funcCode.code.motorParaM1.elem.ratingFrq))                  \
    X(WORD_OFF(funcCode.code.frqPoint))                                    \
    X(WORD_OFF(funcCode.code.motorSn))                                     \
    X((Uint16)GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.motorType))

#define NLAYOUT 14

#ifdef HOST_PROBE

#include <stdio.h>

int main(void)
{
    Uint16 vals[32];
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
