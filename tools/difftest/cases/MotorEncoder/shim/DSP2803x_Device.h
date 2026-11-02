/* Host-side replacement for DSP2803x_Device.h -- MotorEncoder 用例私有。

 * 真的 DSP2803x_Device.h 不能转发:
 *   - 第 30~31 行 `extern cregister volatile unsigned int IFR;` —— `cregister`
 *     是 C28x 专有关键字, MSVC 不认;
 *   - 第 33~38 行把 EINT/DINT/EALLOW/... 定义成 `asm(" clrc INTM")` 这类内联汇编宏。
 *
 * 本模块一个字都不碰这些。但它**要**外设寄存器结构体:
 *   - InitRtInterface / ReInitForPG 写 GpioCtrlRegs.GPAMUX2 / GPADIR / AIOMUX1 /
 *     AIODIR 与 SysCtrlRegs.PCLKCR1.bit.EQEP1ENCLK;
 *   - GetUvwPhase 经 SystemDefine.h 的 `#define UVW_PG_U (AdcResult.ADCRESULT9<<4)`
 *     读 ADC 结果 (三个霍尔输入就是这个);
 *   - GetRotorTransPos 位拼 12 位, 数据位来自
 *     `GpioDataRegs.GPADAT.bit.GPIO21` (ROTOR_TRANS_SO), 时钟/片选写
 *     GpioDataRegs.GPADAT.bit.GPIO23 / GpioDataRegs.AIODAT.bit.AIO12;
 *   - InitSetQEP / GetMDetaPos / GetMTTimeNum / VCGetFeedBackSpeed 整块操作
 *     `(*EQepRegs)` (QEP 寄存器, 指针由 driver 指到 mock 块)。
 * 所以下面把真设备头挂的那批外设头原样拉进来 (它们只是结构体定义 + extern 声明,
 * MSVC 实测编得过), 寄存器结构体因此保持**真身** —— 位域布局不会被仿制品替换。
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
typedef double               float64;
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

/* CPU 控制序列 -> 共享计数表。下标与真文件里的那 7 个宏一一对应。
 *
 * 真文件把 EINT/DINT/EALLOW/... 定义成 `asm(" clrc INTM")` 一类内联汇编, MSVC
 * 不认识 (C2054/C2065)。这些是 C28x **指令**, 没有内存操作数 —— 不替换成可观测量
 * 的话, "改写时漏掉一对 EALLOW/EDIS" 或 "DINT 挪了位置" 在宿主上两侧状态完全相同,
 * 差异检测不到。计数表由 mock 提供实例, driver 在每一次调用前后清零点。
 *
 * 本模块用到的有: EALLOW/EDIS (InitSetQEP / ReInitForPG / GetMDetaPos /
 * GetMTTimeNum), DINT/EINT (GetSpeedMTMethod 的临界区、GetRotorTransPos 的
 * 位拼全程)。driver 记录的是整段序列的快照, 不只是总和 —— "少一次但多一次"
 * 与 "顺序换了两步" 在总数上是同一答案。
 */
extern volatile Uint16 gHostCpuCtl[7];

#define  EINT    (gHostCpuCtl[0]++)
#define  DINT    (gHostCpuCtl[1]++)
#define  ERTM    (gHostCpuCtl[2]++)
#define  DRTM    (gHostCpuCtl[3]++)
#define  EALLOW  (gHostCpuCtl[4]++)
#define  EDIS    (gHostCpuCtl[5]++)
#define  ESTOP0  (gHostCpuCtl[6]++)

/* ---------------------------------------------------------------------------
 * `labs` —— 同样是 C28x **编译器内建**, 工程里没有它的声明。
 *
 * MotorEncoder.c:738 `mDetaTime = labs((long)(gRotorTrans.TimeBak - mTime));`
 * 是全工程唯一一处调用。目标端编这一行**一条告警都没有** (见
 * docs/warning_list.txt: 本模块只有 `mUlong was declared but never referenced`
 * 一条), 说明 cl2000 认得它。实测确认:
 *     long f(long a){ return labs(a); }   ->   ABS ACC / LRETR
 * 即 32 位有符号绝对值, 不是 C89 那种"隐式声明返回 int"。
 *
 * 宿主侧 MSVC 的 CRT 有 `long labs(long)`, 但**没有声明就不会被当成内置** ——
 * 不声明的话 C4013 (隐式声明) 会把返回类型当 int (宿主 32 位, 目标 16 位),
 * 两边就分岔了。所以这里显式声明成 `long labs(long)`: 宿主 long 与目标 long
 * 都是 32 位, 与 ABS ACC 等宽。
 *
 * 不直接 `#include <stdlib.h>`: 那会同时把 `int abs(int)` 拉进来, 与本项目
 * SubPrgInclude.h 的 `extern int abs(int)` (经 `#define int short` 折成 16 位)
 * 冲突。只声明需要的那一个函数。
 * ------------------------------------------------------------------------- */
extern long labs(long);

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
 * 形参显式写成 `int32` 而不是 `long`: 本文件末尾会把 `int` 折成 16 位, 而这里
 * 要的是**与目标机 ACC 等宽的 32 位有符号**, 写 `long` 会让"恰好也是 32 位"
 * 这件事变成隐式依赖。
 *
 * 这是**测试台自己引入的不确定源**, 必须明说: 它保证了"宿主上两侧用同一个
 * __IQsat", 但没有证明宿主版与 C28x 内建逐位一致。差分测试的结论因此只在
 * "两侧都按宿主版语义" 这个前提下成立; 目标端那一半由 target_check.py 的
 * 符号集合/告警比对覆盖, 覆盖不到饱和点的取值。
 * ------------------------------------------------------------------------- */
static int32 __IQsat(int32 A, int32 Pos, int32 Neg)
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

/* ---------------------------------------------------------------------------
 * 类型宽度钉死 (第二部分): plain `int` 与 `long`
 *
 * 上面 DSP28_DATA_TYPES 那一组 (Uint16/Uint32/Ulong/Ullong/...) 钉的是 typedef
 * 出来的名字。而 `int` 与 `long` 是**语言关键字**, 上面那一组管不到 —— 但本模块
 * 到处在用它们, 而且用的正是宽度敏感的地方。
 *
 * 目标机宽度是**实测**出来的, 不是照印象写的。`build/_widths3.c` 把
 * `char p_x[sizeof(X)];` 逐个编译成 COFF 再读 `ofd2000` 的节长度:
 *
 *     char 1    short 1    int 1    long 2    unsigned long 2    pointer 2
 *     float 2   double 2   long long 4       unsigned long long 4   long double 4
 *
 * 单位是 **16 位字** (C28x 的 `sizeof` 按可寻址单元计, 一个单元 16 位)。所以
 *
 *     C28x (cl2000)      int = 16 位     long = 32 位     long long = 64 位
 *     MSVC (宿主)        int = 32 位     long = 32 位     long long = 64 位
 *
 * —— `long` **两侧都是 32 位**, 只有 `int` 差了。这一点很反直觉 (很多人以为
 * C28x 的 long 是 64 位), 所以这里留下实测方法而不只是结论。
 *
 * 不钉 `int` 会让下面这些判定悄悄换语义 (ref 与 new 两侧**都**换, 所以差分测试
 * 仍然能发现改写差异; 但测的就不再是目标机上那段行为了):
 *
 *   * `JudgeABDir` / `JudgeUVWDir` / `JudgeRTDir` 的 `int m_Deta` 与 ±1820 判据,
 *     以及 `int m_Speed` 与 ±32767 饱和 —— 16 位回绕/不回绕是两个结果;
 *   * `include/motor/MotorEncoder.h` 的 FVC_SPEED_STRUCT / ROTOR_TRANS_STRUCT
 *     成员 (`int SpeedEncoder; Uint MTTime; int MDetaPos; ...`) —— 这些正是本用例
 *     逐字段比对的对象, 宽度不同比的就是另一份结构体;
 *   * `int m_UData` / `int m_DetaPos` 参与 64 位乘除时的中间值。
 *
 * 所以这里在本设备头**末尾**把 `int` 折到钉好的宽度上:
 *     int -> short   (16 位有符号)
 * `unsigned int` 随之变成 `unsigned short` = 16 位无符号 (`Uint` 正是它), 也是
 * 目标机的宽度。`long` 不折 —— 宿主本来就是 32 位, 与目标机一致。
 *
 * **为什么是 `short` 而不是 `int16`**: 展开后的词必须能继续和 `unsigned` 组合。
 * C 的语法里 `unsigned` 只肯和 `int`/`char`/`short`/`long` 这几个**关键字**搭配,
 * `unsigned <typedef 名字>` 不是合法 C (gcc 当扩展收下, MSVC 直接报
 * C2061/C2059)。`include/common/MotorDefine.h` 的 `typedef unsigned int Uint;`
 * 展开成 `typedef unsigned int16 Uint;` 就编不过了。实测确认过:
 *     typedef short myint;  typedef unsigned myint Uint;   -> C2061
 *     typedef short myint;  typedef myint Uint;            -> OK
 * 所以这里换成关键字 `short`。`short` 与 `int16_t` 同为 16 位有符号, 位宽不变。
 *
 * 这一条会顺带把 `long int` 变成 `long short` (非法), 工程里只有
 * `include/motor/SubPrgInclude.h` 的 `absl(long int)` / `qatan(long int)` 两处,
 * 由 host_patch.json 把 `(long int)` 换成 `(long)` —— 语义完全相同。
 *
 * 放在设备头末尾而不是开头, 是为了让上面那批外设头 (`DSP2803x_SysCtrl.h` 的
 * `unsigned int src_pgid` 等) 保持**宿主原宽度** —— 那些结构体本模块一个字段都
 * 不碰, 而它们与目标机的布局差异不该被这次钉宽度顺手改掉 (两侧仍一致, 但不该
 * 悄悄改)。本模块真正操作的外设 (GpioCtrl/GpioData/SysCtrl.PCLKCR/AdcResult/
 * EQep/CpuTimer) 全都在这个宏生效之前就定义完了。
 *
 * `llong` / `Ullong` / `Ulong` / `Uint` 都是 typedef 名字, 不含 `int` 这个**词**,
 * 不受本段影响; `1000L` 这类字面量后缀是另一个 token, 同样不受影响。
 * 唯一需要 host_patch.json 配合的是 `include/common/MotorDefine.h` 里那两条
 * `typedef long long llong;` / `typedef unsigned long long Ullong;` —— 见那里
 * 的说明 (与本段无关, 纯粹是 `#define long int32` 会让 `long long` 展开成
 * `int32 int32` 才需要改)。
 */
#define int   short

#endif
