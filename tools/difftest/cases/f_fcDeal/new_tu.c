/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 *
 * 注意这里 include 的是 host_patch.json 生成的**宿主副本** (host_new_f_fcDeal.c,
 * 由 src/function/f_fcDeal.c 打补丁而来), 不是交付源码本身: 那张表的静态初始值
 * 设定项 MSVC 编不过 (C2099)。详见 host_patch.json 的 why。
 */

#define ClearRecordDeal             new_ClearRecordDeal
#define GetFuncCodeInit             new_GetFuncCodeInit
#define GetFuncCodeInitOriginal     new_GetFuncCodeInitOriginal
#define LimitOtherCodeDeal          new_LimitOtherCodeDeal
#define PowerOffRemDeal             new_PowerOffRemDeal
#define RestoreCompanyParaOtherDeal new_RestoreCompanyParaOtherDeal
#define limitOtherCodeIndex         new_limitOtherCodeIndex
#define limitedByOtherCodeIndex     new_limitedByOtherCodeIndex
#define limitedByOtherCodeIndexNum  new_limitedByOtherCodeIndexNum

#include "host_new_f_fcDeal.c"
