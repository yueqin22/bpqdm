/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define AlphBetaToDQ  ref_AlphBetaToDQ
#define ChangeCurrent ref_ChangeCurrent
#define DQToAmpTheta  ref_DQToAmpTheta
#define UVWToAlphBetaAxes ref_UVWToAlphBetaAxes
/* also renamed: MSVC emits a C11 `inline` definition as a linkonce/COMDAT
 * symbol, so two TUs defining the same name are silently FOLDED into one by
 * the linker (build/_me_repro_inline.py: side_b() returned 111, the other
 * side's body).  nm2000 reports no external symbol for it on the target, so
 * the nm2000-derived rename list missed it and the whole body of the inline
 * function was dead on the new side. */

#include "MotorCurrentTransform_ref.c"
