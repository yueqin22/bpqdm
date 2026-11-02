/* Host-side replacement for DSP2803x_Device.h -- MotorInfoCollect 用例私有。

 * 真的 DSP2803x_Device.h 不能转发:
 *   - 第 30~31 行 `extern cregister volatile unsigned int IFR;` —— `cregister`
 *     是 C28x 专有关键字, MSVC 不认;
 *   - 第 33~38 行把 EINT/DINT/EALLOW/... 定义成 `asm(" clrc INTM")` 这类内联汇编宏。
 *
 * 本模块一个字都不碰这些。但它**要**外设寄存器结构体: 模块通过 SystemDefine.h 的
 * `#define ADC_IU (AdcResult.ADCRESULT1<<4)` 一类的宏读 ADC 结果, 而 AdcResult 是
 * `extern volatile struct ADC_RESULT_REGS`。所以下面把真设备头挂的那批外设头
 * 原样拉进来 (它们只是结构体定义 + extern 声明, MSVC 实测编得过), 寄存器结构体
 * 因此保持**真身** —— 位域布局不会被仿制品替换。
 *
 * 类型宽度必须钉死: 真文件把 Uint16 定义成 `unsigned int`(C28x 16 位, 宿主 32 位),
 * 转发进来会让每个 u16 悄悄变宽, 溢出与截断点整体位移。DSP28_DATA_TYPES 卫哨与
 * 真文件同名, 两边都包含也只生效一次。
 *
 * 有意**不**挂的两个: DSP2803x_GlobalPrototypes.h 与 DSP2803x_DefaultISR.h
 * (全是 `__interrupt` 原型)。真的 Examples.h 会拉它们, 所以 Examples.h 也由
 * 用例私有 shim 顶上。
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
#include "DSP2803x_Spi.h"
#include "DSP2803x_Sci.h"
#include "DSP2803x_SysCtrl.h"
#include "DSP2803x_XIntrupt.h"

/* ---------------------------------------------------------------------------
 * `__IQsat` —— C28x **编译器内建**, 不是头文件里的宏。
 *
 * 项目里没有任何一处定义它 (include/ 与 src/ 全文只有调用点), 而目标端编得过、
 * 一条告警都没有 —— 说明它是 cl2000 的内建函数。实测确认: 单独编一个
 * `long f(long a){return __IQsat(a,100,-100);}` 得到
 *     MOVB XAR6, #0x64        ; Pos
 *     MOV  PL, #0xff9c        ; Neg = -100
 *     MINL ACC, XAR6          ; ACC = min(ACC, Pos)
 *     MAXL ACC, P             ; ACC = max(ACC, Neg)
 * 即 `__IQsat(A,Pos,Neg) = min(max(A,Neg),Pos)`, 在 **32 位有符号 ACC** 上做,
 * 操作数按 32 位看待。
 *
 * 宿主没有这个内建, 所以这里补一份语义相同的实现。用 static 函数而不是宏: 宏会
 * 重复求值实参, 而内建只求值一次 —— 这一点在实参带副作用时会不一样。
 *
 * 这是**测试台自己引入的不确定源**, 必须明说: 它保证了"宿主上两侧用同一个
 * __IQsat", 但没有证明宿主版与 C28x 内建逐位一致。差分测试的结论因此只在
 * "两侧都按宿主版语义" 这个前提下成立; 目标端那一半由 target_check.py 的
 * 符号集合/告警比对覆盖, 覆盖不到饱和点的取值。
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

/* 与真文件 `#if (DSP28_28035PN||...)` 那一支逐条对齐 (本板是 28035PN, 该支生效)。
 * 不能凭印象写: 项目头文件拿这些宏选分支, 少一条就可能悄悄换掉一段实现。 */
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
