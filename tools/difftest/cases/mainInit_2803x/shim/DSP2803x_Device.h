/* Host-side replacement for DSP2803x_Device.h -- mainInit_2803x 用例私有。
 *
 * 直接照抄**真文件** (include/device/DSP2803x_Device.h) 的结构, 只改宿主编不过
 * 或宿主测不出来的那几处。真文件 153 行, 这里逐段对应:
 *
 *   真文件第 29~30 行  `extern cregister volatile unsigned int IFR; IER;`
 *        -> cregister 是 C28x 专有关键字, MSVC 不认 (C2059/C2061)。IER/IFR 换成
 *           普通 volatile 对象 (由 mock 提供实例), 并用宏映射过去。这样
 *           `IER = 0x0000;` / `IER |= (...)` 这类读写两侧看到的是**同一块内存**,
 *           "少写一次" / "写错值" / "读写顺序" 全部可观测。
 *
 *   真文件第 33~38 行  EINT/DINT/EALLOW/... 定义成 `asm(" clrc INTM")`
 *        -> C28x **指令**, 没有内存操作数, MSVC 也不认。换成对共享计数表
 *           gHostCpuCtl[] 的自增。本模块几乎每条语句都夹在 EALLOW/EDIS 之间
 *           (InitPll 里甚至有 12 对), 不对它们建模的话, "漏掉一对 EALLOW/EDIS"
 *           或 "把 DINT 挪了位置" 在宿主上两侧状态完全相同, 差异检测不到。
 *
 *   新增 `#define interrupt` (空)
 *        -> C28x 关键字, MSVC 不认。真 Device.h 没定义它 (cl2000 内建), 但下游
 *           DSP2803x_PieVect.h 的 `typedef interrupt void(*PINT)(void);` 和
 *           include/motor/MotorInclude.h 的 4 条 `extern interrupt void ..._isr`
 *           都用到它。置空后两种写法都退化成普通函数/函数指针, 语义等价
 *           (中断属性在宿主上没有对应物, 两侧同样对待)。
 *
 *   新增 M_INT1..M_INT14 / M_DLOG / M_RTOS
 *        -> 与真文件第 40~53 行逐值相同。SetInterruptEnable 的
 *           `IER |= (M_INT1 | M_INT2 | M_INT3 | M_INT5)` 直接吃这些常量。
 *
 *   末尾 `#define int short`
 *        -> 宽度钉死 (第二部分)。C28x 的 `int` 是 16 位, MSVC 是 32 位; 本模块
 *           `int waite;` / `int16 i;` 与 `Uint size;` 都踩在宽度上。放在末尾,
 *           让上面include 的那批外设头保持宿主原宽度 (与 MotorEncoder 用例同一
 *           取舍, 见那边的长注释)。`long` 两侧都是 32 位, 不折。
 *
 * 没有照抄真文件里那段 `#if (DSP28_28035PN||...)` 的选择块的必要 —— 直接把
 * 本板 (28035PN) 那一支的 DSP28_* 宏固定下来, 只有一支生效, 不可能选错。
 */
#ifndef DSP2803x_DEVICE_H
#define DSP2803x_DEVICE_H

#include <stdint.h>

#ifndef DSP28_DATA_TYPES
#define DSP28_DATA_TYPES
typedef int16_t             int16;
typedef int32_t             int32;
typedef int64_t             int64;
typedef uint16_t            Uint16;
typedef uint32_t            Uint32;
typedef uint64_t            Uint64;
typedef float               float32;
typedef double              float64;
#endif

#define   TARGET   1

#define   DSP28_28030PAG   0
#define   DSP28_28030PN    0
#define   DSP28_28031PAG   0
#define   DSP28_28031PN    0
#define   DSP28_28032PAG   0
#define   DSP28_28032PN    0
#define   DSP28_28033PAG   0
#define   DSP28_28033PN    0
#define   DSP28_28034PAG   0
#define   DSP28_28034PN    0
#define   DSP28_28035PAG   0
#define   DSP28_28035PN    TARGET

/* C28x 关键字: 中断函数限定符。宿主置空 (见文件头)。 */
#define interrupt

/* IER / IFR: 真文件是 cregister, 这里换成普通 volatile 对象 + 宏映射。
 * 放在这里 (而不是用 extern 声明) 是为了让 mock 里的实例与这里的声明**同一类型**:
 * 声明写成 Uint16, mock 里也写成 Uint16, 不存在"声明 32 位 / 定义 16 位"的错配。 */
extern volatile Uint16 gHostIER;
extern volatile Uint16 gHostIFR;
#define  IER   gHostIER
#define  IFR   gHostIFR

/* CPU 控制序列 -> 共享计数表 + 事件日志。下标与真文件里那 7 个宏一一对应。
 *
 * 本模块几乎每条语句都夹在 EALLOW/EDIS 之间 (InitPll 里有 6 对), 所以这两个
 * 宏必须**同时**做两件事: 自增计数, 以及往事件日志里追加一条记录
 * (事件号 + 那一刻所有寄存器的指纹)。只自增计数是不够的 —— "少一次但多一次"
 * 与 "顺序换了两步" 在总数上是同一个答案; 而指纹让"寄存器写值 vs EALLOW 的
 * 先后"变成可观测的 (实测: 只做计数时, MI28/MI29/MI34 三条全部漏掉)。
 *
 * gHostCtlEvent 由 mock 提供实现, 这里只留下声明 —— 声明放在文件里而不是
 * mock_mainInit.h, 是因为本 shim 被 mock_mainInit.h 包含, 反向依赖会成环。 */
extern void gHostCtlEvent(int idx);
extern volatile Uint16 gHostCpuCtl[7];

#define  EINT    gHostCtlEvent(0)
#define  DINT    gHostCtlEvent(1)
#define  ERTM    gHostCtlEvent(2)
#define  DRTM    gHostCtlEvent(3)
#define  EALLOW  gHostCtlEvent(4)
#define  EDIS    gHostCtlEvent(5)
#define  ESTOP0  gHostCtlEvent(6)

#define M_INT1  0x0001
#define M_INT2  0x0002
#define M_INT3  0x0004
#define M_INT4  0x0008
#define M_INT5  0x0010
#define M_INT6  0x0020
#define M_INT7  0x0040
#define M_INT8  0x0080
#define M_INT9  0x0100
#define M_INT10 0x0200
#define M_INT11 0x0400
#define M_INT12 0x0800
#define M_INT13 0x1000
#define M_INT14 0x2000
#define M_DLOG  0x4000
#define M_RTOS  0x8000

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

#include "DSP2803x_Adc.h"
#include "DSP2803x_BootVars.h"
#include "DSP2803x_DevEmu.h"
#include "DSP2803x_Cla.h"
#include "DSP2803x_Comp.h"
#include "DSP2803x_CpuTimers.h"
#include "DSP2803x_ECan.h"
#include "DSP2803x_ECap.h"
#include "DSP2803x_EPwm.h"
#include "DSP2803x_EQep.h"
#include "DSP2803x_Gpio.h"
#include "DSP2803x_I2c.h"
#include "DSP2803x_Lin.h"
#include "DSP2803x_NmiIntrupt.h"
#include "DSP2803x_PieCtrl.h"
#include "DSP2803x_PieVect.h"
#include "DSP2803x_Spi.h"
#include "DSP2803x_Sci.h"
#include "DSP2803x_SysCtrl.h"
#include "DSP2803x_XIntrupt.h"

#define DSP28_COMP1 1
#define DSP28_COMP2 1
#define DSP28_COMP3 1
#define DSP28_EPWM1 1
#define DSP28_EPWM2 1
#define DSP28_EPWM3 1
#define DSP28_EPWM4 1
#define DSP28_EPWM5 1
#define DSP28_EPWM6 1
#define DSP28_EPWM7 1
#define DSP28_ECAP1 1
#define DSP28_EQEP1 1
#define DSP28_ECANA 1
#define DSP28_SPIA  1
#define DSP28_SPIB  1
#define DSP28_SCIA  1
#define DSP28_I2CA  1
#define DSP28_LINA  1

/* 类型宽度钉死 (第二部分): plain `int` 折成 16 位。
 * 见 MotorEncoder 用例同一段的长注释 —— `unsigned int` 随之变 16 位无符号
 * (`Uint` 正是它), 也是目标机的宽度; `long` 两侧都是 32 位, 不折。
 * 用 `short` 而不是 `int16`: 展开后的词必须还能和 `unsigned` 组合
 * (`typedef unsigned int Uint;` -> `typedef unsigned short Uint;`)。
 *
 * 注意它对外设头的影响: 上面那批外设头 (DSP2803x_SysCtrl.h 的
 * `unsigned int src_pgid` / COPY_RECORD, DSP2803x_PieVect.h 的 PINT 等) 都在
 * 这个宏生效**之前**展开完, 保持宿主原宽度。两侧一致, 与 MotorEncoder 同取舍。
 */
#define int   short

#endif
