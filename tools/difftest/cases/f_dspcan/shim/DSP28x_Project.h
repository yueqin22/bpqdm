/* Host-side replacement for DSP28x_Project.h -- f_dspcan 用例私有。

 * f_dspcan.c 第 2 行 `#include "DSP28x_Project.h"` 是整条 include 链的起点, 所以
 * 这里既要把类型宽度钉死, 也要把**真**外设模型挂上。
 *
 * 与 f_error / f_menu 那批纯逻辑用例的 shim 不同: 本模块整篇都在读写 eCAN 寄存器,
 * 所以 ECanaRegs / ECanaMboxes / ECanaLAMRegs 与 GpioCtrlRegs 必须是**真结构体**
 * (位域布局错了, 差分测试测的就是仿制品)。这两个头 (DSP2803x_ECan.h /
 * DSP2803x_Gpio.h) 实测 MSVC 直接编得过 —— 它们只是结构体 + extern 声明, 没有
 * `cregister` / `interrupt` / `asm(...)`。真文件在 include/device 下, 而该目录**不在**
 * 宿主 include 路径上 (run_case 只挂了 motor/common/function), 所以由 real.txt 生成
 * 转发头放进 gen_dir (排最前)。
 *
 * 类型宽度必须钉死: 项目 DataTypeDef.h 把 Uint16 定义成 `unsigned int`
 * (C28x 16 位, 宿主 32 位)。转发进来会让每个 u16 悄悄变宽, 溢出与截断点整体位移。
 *
 * EALLOW/EDIS 在真 DSP2803x_Device.h 里是 `asm(" EALLOW")`, MSVC 不认。本模块在
 * InitdspECan 里成对调用它们, 所以这里改成对一张共享计数表自增 —— 这样"改写把
 * EDIS 弄丢了"是可观测的, 而不是完全看不见。这不是在宿主上执行指令, 只是记录
 * 它出现过。
 *
 * 有意**不**挂 DSP2803x_Device.h 与 DSP2803x_Examples.h (前者有 cregister, 后者
 * 拉进 `__interrupt` 原型)。本模块用到的只有 DSP_CLOCK 与上面两个外设头。
 */
#ifndef DSP28X_PROJECT_H
#define DSP28X_PROJECT_H

#include "DataTypeDef.h"

/* 真 DSP28x_Project.h 会拉 DSP2803x_Examples.h, 那里第 9 行是 DSP_CLOCK 60。
 * f_dspcan.c 第 11 行的 `#if (DSP_CLOCK == 100)` 用它选 DSPCAN_CLK; 少了它
 * DSP_CLOCK 未定义 -> 表达式按 0 求值 -> 静默选到 30000 那一支。60 MHz 是这块
 * 板子的真实主频。 */
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

#include "DSP2803x_ECan.h"
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
