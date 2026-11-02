/* Host-side replacement for DSP28x_Project.h -- f_interface 用例私有。

 * f_interface.c 第 2 行 `#include "f_funcCode.h"`, 真 f_funcCode.h 第 5 行就是
 * `#include "DSP28x_Project.h"` —— 所以这里既要把类型宽度钉死, 也要把本模块
 * 真正用到的外设模型挂上。

 * ---- 为什么钉宽度 ------------------------------------------------------------

 * 工程 DataTypeDef.h 把 `Uint16` 定义成 `unsigned int`: C28x 上 16 位, 宿主上 32 位。
 * 沿用真文件会让每个 u16 悄悄变宽。本模块有大量"先在 32 位里算、再截回 16 位"的地方,
 * 宽度一变结论就没了, 例如
 *   frq2CoreTmp = (frqVFRunRemainder + (frqDroop << 15)) / ((int32)frqPuQ15);
 * 左边是 `static Uint16`, 截断发生在**比较之前** (下一行的
 * `ABS_INT16((int16)frq2CoreTmp)`), 所以 Uint16 的宽度直接决定分支走向。
 * 这里钉成 uint16_t。

 * ---- 为什么挂 EQep -----------------------------------------------------------

 * UpdateDataCore2Func 在 `#if DSP_2803X` 里读
 *     ABZPos = EQep1Regs.QPOSCNT & 0x0000FFFF;
 * QPOSCNT 是 `Uint32` (DSP2803x_EQep.h:153), ABZPos 是 Uint16。掩码与截断的
 * 组合必须落在真布局上, 否则"改写把掩码改错"测不出来。该头由 real.txt 转发。

 * 有意**不**挂 DSP2803x_Device.h (`extern cregister volatile unsigned int IFR;`) 与
 * DSP2803x_Examples.h (拉进 DSP2803x_DefaultISR.h 的 `__interrupt` 原型)。
 * 本模块只用到 DSP_CLOCK 与 BITn 两样, 都在下面自己给。

 * ---- 本模块**不**需要的东西 --------------------------------------------------

 * 与 f_main 用例不同, 这里不挂 SysCtrl / CpuTimers, 也不需要 EALLOW/EDIS 计数表:
 * f_interface.c 不写任何外设寄存器, 唯一的硬件接触点就是上面那个 QPOSCNT 读。
 * 它也不用 `GetTime()` —— f_main.h 里那个宏虽然展开成 CpuTimer1, 但从未被本模块
 * 使用, 所以不需要 CpuTimer1 的真身。
 */
#ifndef DSP28X_PROJECT_H
#define DSP28X_PROJECT_H

#include "DataTypeDef.h"

/* 真 DSP28x_Project.h 会拉 DSP2803x_Examples.h, 那里第 9 行是 `#define DSP_CLOCK 60`。
 * f_debug.h 的 TIME_10US / TIME_1MS 按它选值。本模块间接用到 TIME_UNIT_CURRENT_CHK
 * (f_funcCode.h:1348, 固定值 10), 但 f_debug.h 的 `#if (DSP_CLOCK == 60)` 仍然要求
 * DSP_CLOCK 有定义 —— 少了它 TIME_* 一类的宏全部变成未定义。60 MHz 是这块板子的
 * 真实主频。 */
#define DSP_CLOCK   60

#ifndef DSP28_DATA_TYPES
#define DSP28_DATA_TYPES
typedef int16_t             int16;
typedef int32_t             int32;
typedef int64_t             int64;
typedef uint16_t            Uint16;
typedef uint32_t            Uint32;
typedef uint64_t            Uint64;
typedef float               float32;
typedef long double         float64;
#endif

/* Device.h 里的 BITn; 工程头文件到处在用。 */
#define BIT0    0x0001
#define BIT1    0x0002
#define BIT2    0x0004
#define BIT3    0x0008
#define BIT4    0x0010
#define BIT5    0x0020
#define BIT6    0x0040
#define BIT7    0x0080
#define BIT8    0x0100
#define BIT9    0x0200
#define BIT10   0x0400
#define BIT11   0x0800
#define BIT12   0x1000
#define BIT13   0x2000
#define BIT14   0x4000
#define BIT15   0x8000

/* EQep1Regs.QPOSCNT 是本模块唯一的硬件读点。 */
#include "DSP2803x_EQep.h"

#endif
