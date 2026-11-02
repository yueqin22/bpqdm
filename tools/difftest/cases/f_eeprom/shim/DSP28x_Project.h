/* Host-side replacement for DSP28x_Project.h -- f_eeprom 用例私有。

 * f_eeprom.c 第 2 行 `#include "f_eeprom.h"`, 真 f_eeprom.h 第 4 行是
 * `#include "f_funcCode.h"`, 而真 f_funcCode.h 第 5 行是
 * `#include "DSP28x_Project.h"` —— 所以这里既要把类型宽度钉死, 也要把本模块
 * 真正用到的外设模型挂上。

 * ---- 为什么钉宽度 ------------------------------------------------------------

 * 工程 DataTypeDef.h 把 `Uint16` 定义成 `unsigned int`: C28x 上 16 位, 宿主上 32 位。
 * 沿用真文件会让每个 u16 悄悄变宽。本模块有大量"先在 32 位里算、再截回 16 位"
 * 的地方, 例如 WriteFuncCode 的
 *     tmp = (funcCodeRw->index + FUNCCODE_EEPROM_START_INDEX) << 1;
 *     I2cMsg.highAddr = tmp >> 8;
 * 以及 ReadFuncCode 的
 *     dataRead[i] = (I2cMsg.buffer[i<<1] << 8) + I2cMsg.buffer[(i<<1)+1];
 * 左边是 `Uint16`, 位移与截断都发生在 16 位宽上。宽度一变结论就没了。
 * 这里钉成 uint16_t。

 * ---- 与真 DSP28x_Project.h 的差别 -------------------------------------------

 * 真文件会拉 DSP2803x_Device.h (有 `cregister` 与 `asm()` 宏) 与
 * DSP2803x_Examples.h (有 `__interrupt` 原型), 宿主上都编不过。本用例用私有的
 * DSP2803x_Device.h / DSP2803x_Examples.h 顶替 (同目录), 前者把 EALLOW/EDIS
 * 改成共享计数表自增, 后者保留真的 DELAY_US 宏。

 * 然后按真 DSP2803x_Device.h 里那条 `#if (DSP28_28035PN||...)` 的**同一支**
 * 把外设头拉进来 (顺序与真文件一致, 便于对照):

 *   DSP2803x_I2c.h          本模块的 I2C 寄存器 (真身, 由 real.txt 转发)
 *   DSP2803x_Gpio.h         本模块的 GPIO 寄存器。**这一份是用例私有 shim**,
 *                           它 include 真文件并给 GpioDataRegs 加了一层
 *                           "GPBSET/GPBCLEAR 侧写语义" 的包装 —— 见该文件头。
 *                           它其实已被 real.txt 的机制排除在外, 但这里显式
 *                           include 一次, 免得将来 real.txt 被改动后失去保护。
 *   DSP2803x_PieVect.h      真文件里 `PINT` 是
 *                           `typedef interrupt void(*PINT)(void);`, MSVC 不认。
 *                           本模块**不引用** PieVect 的任何东西, 所以这里
 *                           干脆不拉它 (真 DSP2803x_Device.h 拉它只是为了
 *                           DSP2803x_DefaultIsr.c, 与 f_eeprom.c 无关)。
 *   DSP2803x_CpuTimers.h    本模块不读 CpuTimer, 但 f_main.h 的 GetTime 宏
 *                           展开成 `CpuTimer1.RegsAddr->TIM.all`; 本模块从不
 *                           调用 GetTime, 所以**不**需要 —— 不拉。
 *   DSP2803x_SysCtrl.h      本模块不读 SysCtrl, 不拉。
 */
#ifndef DSP28X_PROJECT_H
#define DSP28X_PROJECT_H

#include "DataTypeDef.h"

/* 真 DSP28x_Project.h 会拉 DSP2803x_Examples.h, 那里第 14 行是
 * `#define DSP_CLOCK 60`。f_debug.h 的 TIME_* 按它选值; InitSetI2ca 的
 * `#if (DSP_CLOCK == 100) ... #elif (DSP_CLOCK == 60)` 也按它选 I2CPSC。
 * 60 MHz 是这块板子的真实主频, 不是猜的。 */
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

/* 本模块真正摸到的两个外设。顺序与真 DSP2803x_Device.h 一致。 */
#include "DSP2803x_Device.h"      /* 私有版: 类型宽度 + BITn + EALLOW/EDIS 计数 */
#include "DSP2803x_I2c.h"
#include "DSP2803x_Gpio.h"

/* 真 DSP28x_Project.h 的最后一步是 `#include "DSP2803x_Examples.h"`。这里照做
 * (用同目录的私有版本), 因为 f_eeprom.c 用到的 I2C 常量 (I2C_SUCCESS 等 6 个)
 * 与 DELAY_US 宏都是经这条链进来的 —— 少一个, 模块在宿主上就编不过。 */
#include "DSP2803x_Examples.h"

#endif
