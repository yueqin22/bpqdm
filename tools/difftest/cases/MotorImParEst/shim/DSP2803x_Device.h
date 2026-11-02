/* Host-side replacement for DSP2803x_Device.h -- MotorImParEst 用例私有。
 *
 * 真的 DSP2803x_Device.h 不能转发:
 *   - `extern cregister volatile unsigned int IFR;` —— `cregister` 是 C28x 专有
 *     关键字, MSVC 不认;
 *   - EINT/DINT/ERTM/DRTM/EALLOW/EDIS/ESTOP0 被定义成 `asm(" clrc INTM")` 一类
 *     的内联汇编宏。
 *
 * 本模块**真的会调** DINT / EINT / EALLOW / EDIS:
 *   - RsIdentify 每条写 EPwm 寄存器的支路都用 EALLOW/EDIS 包着 (case 1, 3/5);
 *   - RrLoIdentify case 1 关 PIEIER1.INTx1、case 4 重开 PIEIER3 + 重指向量;
 *   - SetPwmForRrLoId 整段在 EALLOW/EDIS 里;
 *   - EnableTimer 是 EALLOW/EDIS 之后接 EINT;
 *   - RrLoInPeriodAndCmpInt 是 EALLOW/EDIS -> EINT -> ... -> DINT -> PIEACK。
 * 抹成空语句的话, "改写把某个 EALLOW 挪出了临界区" / "少了一次 EDIS" 在宿主上
 * 完全不可见。这里把它们改成对一张共享计数表的自增, 于是"这段 CPU 控制序列是否
 * 原样发生、顺序对不对"变成可观测量。计数表由用例 mock 提供, 两侧共用一份, 每侧
 * 调用前由 driver 清零。
 *
 * 注意: 这不是在宿主上**执行**这些指令, 而是记录它们**出现过**。目标端语义
 * (关中断/开写保护) 不在宿主测试的覆盖范围内, 与寄存器 mock 同一性质。
 *
 * 外设头照旧原样拉进来 (它们只是结构体 + extern 声明, MSVC 实测编得过), 寄存器
 * 结构体因此保持真身。本模块写 EPwm1/2/3Regs / AdcRegs / AdcResult /
 * PieCtrlRegs / PieVectTable / GpioDataRegs, 这些结构体的字段顺序与位域布局必须
 * 是芯片的。PieVect.h 是个例外 —— 它的 `PINT` 是
 * `typedef interrupt void(*PINT)(void);`, 由 host_patch.json 去掉 `interrupt`
 * 后放在 gen_dir 里 (见本用例 real.txt 的说明)。
 *
 * 类型宽度必须钉死: 真文件把 Uint16 定义成 `unsigned int`(C28x 16 位, 宿主 32 位)。
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
typedef long double         float64;
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
#include "DSP2803x_Cla.h"
#include "DSP2803x_Comp.h"
#include "DSP2803x_CpuTimers.h"
#include "DSP2803x_DevEmu.h"
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

/* ---------------------------------------------------------------------------
 * `__IQsat` —— C28x **编译器内建**, 不是头文件里的宏。
 *
 * 项目里没有任何一处定义它 (include/ 与 src/ 全文只有调用点), 而目标端编得过、
 * 一条告警都没有 —— 说明它是 cl2000 的内建函数。实测确认 (见 MotorInfoCollect
 * 用例的同名注释): `__IQsat(A,Pos,Neg) = min(max(A,Neg),Pos)`, 在 **32 位有符号
 * ACC** 上做, 操作数按 32 位看待。
 *
 * 宿主没有这个内建, 所以这里补一份语义相同的实现。用 static 函数而不是宏: 宏会
 * 重复求值实参, 而内建只求值一次 —— 这一点在实参带副作用时会不一样。
 *
 * 本模块三处调用:
 *   RrLoInPeriodAndCmpInt  __IQsat(m_Iu, 32767, -32767)            long 实参
 *   RrLoInPeriodAndCmpInt  __IQsat(SampleTimes, 15, 0)             int  实参
 *   RrGetVoltReBegin       __IQsat(PwmCompareValue, 2000, 1)       Uint 实参
 * 第一处的 m_Iu 是 `long`, 在 C28x 上是 32 位, 与内建的 ACC 宽度一致; 第二处
 * 第三处的实参是 16 位, 提升到 32 位后仍是非负。三种情形下 min(max(A,Neg),Pos)
 * 与目标端逐位一致 —— 但这仍然是**测试台自己引入的不确定源**, 它只保证宿主上
 * 两侧用同一个 __IQsat。目标端那一半由 target_check.py 的符号集合/告警比对覆盖。
 * ------------------------------------------------------------------------- */
static long __IQsat(long A, long Pos, long Neg)
{
    if (A > Pos)
    {
        return Pos;
    }
    if (A < Neg)
    {
        return Neg;
    }
    return A;
}

/* CPU 控制序列 -> 共享计数表。下标与真文件里的那 7 个宏一一对应。 */
extern volatile Uint16 gHostCpuCtl[7];

#define  EINT    (gHostCpuCtl[0]++)
#define  DINT    (gHostCpuCtl[1]++)
#define  ERTM    (gHostCpuCtl[2]++)
#define  DRTM    (gHostCpuCtl[3]++)
#define  EALLOW  (gHostCpuCtl[4]++)
#define  EDIS    (gHostCpuCtl[5]++)
#define  ESTOP0  (gHostCpuCtl[6]++)

/* 与真文件 `#if (DSP28_28035PN||...)` 那一支逐条对齐 (本板是 28035PN, 该支生效)。 */
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

/* ---------------------------------------------------------------------------
 * `interrupt` —— C28x 的中断函数限定符, MSVC 不认。
 *
 * 本模块是**第一个**在**自己的源码里**写 `interrupt void ...(void)` 的用例
 * (第 4 行的原型与第 444 行的定义)。别的用例要么 ISR 不带这个关键字, 要么原型
 * 全在头文件里 (那几条由 host_patch.json 逐条去掉)。
 *
 * 这里定义成**空宏**: `interrupt void f(void)` 展开成 `void f(void)`, 函数名、
 * 参数、返回类型一个都不变。全库只有 5 处 `interrupt` 且**全部**是
 * `interrupt void ...` 形式 (2026-10-30 核对过), 所以空宏不会误伤任何标识符。
 *
 * 位置很关键: 必须放在**所有头文件展开之后**。放前面的话
 * DSP2803x_PieVect.h 的 `typedef interrupt void(*PINT)(void);` 会被这个宏改掉,
 * 而那一份是 host_patch.json 打过补丁的整份副本 —— 两条修补同时生效就会互相
 * 覆盖, 排查起来很费劲。
 * ------------------------------------------------------------------------- */
#define interrupt

/* ---------------------------------------------------------------------------
 * 把 `int` 折叠成目标端的 16 位 —— 与 SubPrg / MotorInvProtect / MotorMain /
 * MotorEncoder / MotorPmsmParEst 同一套纪律。
 *
 * `MotorDefine.h` 第 13 行是 `typedef unsigned int Uint;`。C28x 上 `int` 是 16 位,
 * 宿主上是 32 位。这不是提升规则差异 (那条去不掉, 见 tools/difftest/README.md 3.2),
 * 而是**存储宽度**, 属 README 3.1 要求必须钉死的那一类。本模块有 9 处这样的截断点:
 *
 *   RsIdentify       Uint m_UData1 = (1024UL * CurrentGet) / Current;
 *                    Uint m_UData2;  gUVCoff.Number (Uint) 累加到 1024;
 *   RrLoIdentify     gRrLoIdentify.PwmCompareValue (Uint) 每次 -20 / +50 / +100;
 *   RrGetVoltReBegin gRrLoIdentify.PwmCompareValue = __IQsat(...) 赋回 Uint;
 *   RrLoInPeriodAndCmpInt  gRrLoIdentify.SampleTimes = __IQsat(..., 15, 0);
 *   LmIoIdentify     gLmIoIdentify.VFOvShock (Uint) 自减。
 *
 * 不折叠的话 `m_UData1` 保留 32 位, "比值超过 64 倍后截成低 16 位" 这类差异在宿主
 * 上看不见 —— 而 driver 恰好能把 CurrentGet/Current 推到那个区间。
 *
 * 代价: SubPrgInclude.h 的 `long int` 原型会变成 `long short` (MSVC C2632),
 * host_patch.json 修掉它, 宽度不变。
 *
 * 上面那些外设头在**这一行之前**就已经展开, 所以它们的宽度不受影响;
 * int16/uint16 在本文件顶部钉死。
 * ------------------------------------------------------------------------- */
#define int   short

#endif
