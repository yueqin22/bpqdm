/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 *
 * include 的是 host_patch.json 生成的**宿主副本**, 不是 ref/ 下的原文件:
 * limitOtherCodeIndex[] 的静态初始值设定项是两个地址常量相减 (GetCodeIndex),
 * MSVC 报 C2099 (详见 host_patch.json 的 why)。宿主副本放在 gen_dir 里,
 * 而 gen_dir 排在 include 路径最前面。
 */

#define ClearRecordDeal             ref_ClearRecordDeal
#define GetFuncCodeInit             ref_GetFuncCodeInit
#define GetFuncCodeInitOriginal     ref_GetFuncCodeInitOriginal
#define LimitOtherCodeDeal          ref_LimitOtherCodeDeal
#define PowerOffRemDeal             ref_PowerOffRemDeal
#define RestoreCompanyParaOtherDeal ref_RestoreCompanyParaOtherDeal
#define limitOtherCodeIndex         ref_limitOtherCodeIndex
#define limitedByOtherCodeIndex     ref_limitedByOtherCodeIndex
#define limitedByOtherCodeIndexNum  ref_limitedByOtherCodeIndexNum

#include "host_ref_f_fcDeal.c"
