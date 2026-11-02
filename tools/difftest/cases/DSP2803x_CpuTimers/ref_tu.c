/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define ConfigCpuTimer ref_ConfigCpuTimer
#define CpuTimer0      ref_CpuTimer0
#define CpuTimer1      ref_CpuTimer1
#define CpuTimer2      ref_CpuTimer2
#define InitCpuTimers  ref_InitCpuTimers

#include "DSP2803x_CpuTimers_ref.c"
