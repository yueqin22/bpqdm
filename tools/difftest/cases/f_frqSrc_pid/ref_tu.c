/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define FrqPidSetDeal       ref_FrqPidSetDeal
#define PidFuncCalc         ref_PidFuncCalc
#define UpdatePidFuncPara   ref_UpdatePidFuncPara
#define UpdatePidFuncRefFdb ref_UpdatePidFuncRefFdb
#define bPidFdbLose         ref_bPidFdbLose
#define pidCalcFlag         ref_pidCalcFlag
#define pidError            ref_pidError
#define pidFdbLoseTicker    ref_pidFdbLoseTicker
#define pidFunc             ref_pidFunc
#define pidFuncRef          ref_pidFuncRef
#define pidInitTicker       ref_pidInitTicker
#define pidLpf              ref_pidLpf
#define pidParaLine         ref_pidParaLine

#include "f_frqSrc_pid_ref.c"
