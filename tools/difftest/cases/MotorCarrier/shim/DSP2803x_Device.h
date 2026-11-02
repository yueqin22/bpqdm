/* Host-side replacement for DSP2803x_Device.h -- MotorCarrier 用例私有。

 * 真的 DSP2803x_Device.h 不能转发:
 *   - `extern cregister volatile unsigned int IFR;` —— `cregister` 是 C28x 专有
 *     关键字, MSVC 不认;
 *   - EINT/DINT/ERTM/DRTM/EALLOW/EDIS/ESTOP0 被定义成 `asm(" clrc INTM")` 一类
 *     的内联汇编宏。
 *
 * 本模块**真的会调** DINT / EALLOW / EDIS / EINT / ERTM (RunCaseGetPar 的失败
 * 分支与 EndOfParIdentify 的成功分支), 所以不能简单抹成空语句 —— 那样"改写把
 * DINT 弄丢了"在宿主上完全不可见。这里把它们改成对一张共享计数表的自增, 于是
 * "这段 CPU 控制序列是否原样发生"变成可观测量。计数表由用例 mock 提供, 两侧
 * 共用一份, 每侧调用前清零。
 *
 * 注意: 这不是在宿主上**执行**这些指令, 而是记录它们**出现过**。目标端语义
 * (关中断/开写保护) 不在宿主测试的覆盖范围内, 与寄存器 mock 同一性质。
 *
 * 外设头照旧原样拉进来 (它们只是结构体 + extern 声明, MSVC 实测编得过), 寄存器
 * 结构体因此保持真身。PieVect.h 是个例外 —— 它的 `PINT` 是
 * `typedef interrupt void(*PINT)(void);`, 由 host_patch.json 去掉 `interrupt`
 * 后放在 gen_dir 里 (见该用例的 real.txt 说明)。
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
 * 本模块只有一处调用 (`gFcCal.FcBak = __IQsat(gFcCal.FcBak, m_MaxFc, m_MinFc)`),
 * 三个实参都是 Uint, 在 C28x 上按 32 位看都是非负数, 于是
 * min(max(A,Neg),Pos) 就是"夹进 [Neg,Pos]"。这一点让宿主的 long 版与目标端逐位
 * 一致 —— 但仍然要说清楚: 这是**测试台自己引入的不确定源**, 它只保证宿主上两侧
 * 用同一个 __IQsat。目标端那一半由 target_check.py 的符号集合/告警比对覆盖。
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

#endif
