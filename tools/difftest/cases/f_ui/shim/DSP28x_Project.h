/* Host-side replacement for DSP28x_Project.h -- f_ui 用例私有。

 * f_ui.c 第 2 行 `#include "f_ui.h"`, 真 f_ui.h 再 `#include "f_funcCode.h"`, 后者
 * 第 5 行就是 `#include "DSP28x_Project.h"` —— 所以这里既要把类型宽度钉死, 也要把
 * 本模块真正用到的两个外设模型挂上。

 * ---- 为什么钉宽度 ------------------------------------------------------------

 * 工程 DataTypeDef.h 把 `Uint16` 定义成 `unsigned int`: C28x 上 16 位, 宿主上 32 位。
 * 沿用真文件会让每个 u16 悄悄变宽, 溢出与截断点整体位移。这里钉成 uint16_t,
 * 之后解析的所有工程头文件都按 C28x 宽度走。

 * ---- 为什么挂 Spi / Gpio, 而不挂 Device -------------------------------------

 * 本模块读写 SpiaRegs (SPI_REGS) 与 GpioCtrlRegs / GpioDataRegs
 * (GPIO_CTRL_REGS / GPIO_DATA_REGS)。位域布局必须是芯片的真布局, 否则差分测试测的
 * 就是仿制品 —— 例如 f_ui.h 把 RCK 定义成 `GpioDataRegs.GPADAT.bit.GPIO22`、
 * KEYSEL 定义成 `GpioDataRegs.AIODAT.bit.AIO4`, 位域错一位就测错了对象。
 * 这两个头实测 MSVC 直接编得过 (纯结构体 + extern 声明), 由 real.txt 生成转发头。

 * 有意**不**挂 DSP2803x_Device.h (`extern cregister volatile unsigned int IFR;`) 与
 * DSP2803x_Examples.h (拉进 DSP2803x_DefaultISR.h 的 `__interrupt` 原型)。
 * 本模块用到的只有 DSP_CLOCK、BITn、EALLOW/EDIS 三样, 都在下面自己给。

 * ---- EALLOW / EDIS ----------------------------------------------------------

 * 真 Device.h 里它们是 `asm(" EALLOW")` / `asm(" EDIS")`, MSVC 不认。
 * InitSpiaGpio 成对调用它们, 所以这里改成对一张共享计数表自增 —— 这样"改写把 EDIS
 * 弄丢了"是可观测的, 而不是完全看不见。这不是在宿主上执行指令, 只是记录它出现过。
 * 计数表由 mock_ui.c 定义, 两侧共用; 快照后逐格比对。
 */
#ifndef DSP28X_PROJECT_H
#define DSP28X_PROJECT_H

#include "DataTypeDef.h"

/* 真 DSP28x_Project.h 会拉 DSP2803x_Examples.h, 那里第 9 行是 `#define DSP_CLOCK 60`。
 * f_debug.h 的 TIME_10US / TIME_1MS 按它选值, 而 f_ui.c 的 InitSetSpiForFunc 用
 * `#if (DSP_CLOCK == 100) ... #elif (DSP_CLOCK == 60)` 选 SPIBRR。少了它
 * DSP_CLOCK 按 0 求值 -> 两个分支都不成立 -> SPIBRR 一次都不写, 而"没写"和
 * "写了但值一样"在寄存器快照里分不开。60 MHz 是这块板子的真实主频。 */
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

#include "DSP2803x_Spi.h"
#include "DSP2803x_Gpio.h"

/* CPU 控制序列 -> 共享计数表 (下标与真 Device.h 里那 7 个宏一一对应)。 */
extern volatile Uint16 gHostCpuCtl[7];

#define  EINT    (gHostCpuCtl[0]++)
#define  DINT    (gHostCpuCtl[1]++)
#define  ERTM    (gHostCpuCtl[2]++)
#define  DRTM    (gHostCpuCtl[3]++)
#define  EALLOW  (gHostCpuCtl[4]++)
#define  EDIS    (gHostCpuCtl[5]++)
#define  ESTOP0  (gHostCpuCtl[6]++)

#endif
