/* mock_cputimers.c -- DSP2803x_CpuTimers 用例的寄存器 mock。

 * 目标端上这三个寄存器块由 DSP2803x_GlobalVariableDefs.c 用
 * `#pragma DATA_SECTION` 钉在固定地址 (0x0C00 / 0x0C08 / 0x0C10)。宿主上没有
 * 那些地址, 也不需要 —— 差分测试要的不是"地址对不对", 而是"同一串操作之后,
 * 寄存器里剩下的内容一不一样"。
 *
 * 所以这里只提供**三个普通内存实例**。ref 与 new 两侧通过同一份实例写读:
 *   - 调用前由 driver 清零;
 *   - 调用后由 driver 逐字段取快照比对。
 * 两侧看到的是同一块内存, 所以任何"少写一个位 / 写错字段 / 写错顺序"都会现形。
 *
 * 声明来自 DSP2803x_CpuTimers.h (真身, 经 real.txt 转发), 所以这里的定义与
 * 目标端的是同一个结构体 —— 位域布局不会在这里被偷换。
 */
#include "DSP2803x_Device.h"

volatile struct CPUTIMER_REGS CpuTimer0Regs;
volatile struct CPUTIMER_REGS CpuTimer1Regs;
volatile struct CPUTIMER_REGS CpuTimer2Regs;
