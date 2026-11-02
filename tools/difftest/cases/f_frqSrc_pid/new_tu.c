/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define FrqPidSetDeal       new_FrqPidSetDeal
#define PidFuncCalc         new_PidFuncCalc
#define UpdatePidFuncPara   new_UpdatePidFuncPara
#define UpdatePidFuncRefFdb new_UpdatePidFuncRefFdb
#define bPidFdbLose         new_bPidFdbLose
#define pidCalcFlag         new_pidCalcFlag
#define pidError            new_pidError
#define pidFdbLoseTicker    new_pidFdbLoseTicker
#define pidFunc             new_pidFunc
#define pidFuncRef          new_pidFuncRef
#define pidInitTicker       new_pidInitTicker
#define pidLpf              new_pidLpf
#define pidParaLine         new_pidParaLine

#include "f_frqSrc_pid.c"
