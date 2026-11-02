/* Case-private host-side MotorInclude.h -- main 用例。
 *
 * main.c 只 include 这一个头, 于是本文件要把整条链补齐:
 *   DataTypeDef.h (公共 shim, 宽度钉死) -> SystemDefine.h (真) -> MotorDefine.h (真)
 *   -> MotorStructDefine.h (真, CPU_TIME_STRUCT 在这里)。
 * 三个真头文件由 run_case.py 的 include 顺序从改写树 include/common 找到
 * (与 MotorDebug 用例同一手法); Device.h 被本目录下的私有 shim 顶替。
 *
 * 相对于真链, 这里补/改的都在下面, 每一条都只为"让宿主编得过 + 让原本不可观的
 * 东西变得可观测", 不改被测代码的语义:
 *
 * 1. **EALLOW/EDIS/EINT/DINT/ERTM -> 计数 + 事件日志**。真 Device.h 里是
 *    asm(" clrc INTM"), MSVC 不认。改成调 gHostCtlEvent(idx): 既给一张计数表,
 *    也往调用日志里追加一条事件 —— main.c 的 ISR 里这些宏成对出现, "把 EDIS 弄丢了"
 *    / "把 EINT 挪到调用之后" 因此可观测 (顺序日志比只看计数强)。
 *
 * 2. **`#define interrupt` 空**。`interrupt` 是 C28x 存储类关键字, main.c 的
 *    三个 ISR 用它。MSVC 不认识这个名字, 定义成空即可 (三个 ISR 仍按普通函数
 *    被 driver 直接调用)。
 *
 * 3. **GetTime() 换成 gHostGetTime()**。真 MotorDefine.h 第 68 行把它定义成
 *    `(CpuTimer1.RegsAddr->TIM.all)`; 目标端 TIM 是**下计数器**, 全工程的时间差
 *    都写成"先读的减后读的"。宿主上要模拟"时间在走", 还要给 while(1) 一个
 *    终止判据, 所以改成 mock 里的函数: 每次调用按 driver 设定的步长递减虚拟
 *    时钟, 并在超出调用预算时 longjmp 出去 (终止神谕, 见 mock_main.c)。
 *    两侧调同一个函数, 时钟序列由 driver 灌进去, 仍然可比。
 *
 * 4. **AdcRegs / EPwm1Regs / PieCtrlRegs 三件外设寄存器**。main.c 的 ISR 写
 *    AdcRegs.ADCINTFLGCLR.bit.ADCINT1 / AdcRegs.SOCPRICTL.bit.RRPOINTER /
 *    EPwm1Regs.ETCLR.bit.INT / PieCtrlRegs.PIEACK.all。这里给最小结构 (成员名与
 *    真芯片一致; 位域基类型用 Uint 而不是 Uint16, 因为 MSVC 对非 int 基类型的
 *    位域只给 C4214 警告且布局不保证 —— 两侧共用同一份 shim, 写的值就是比对的
 *    值, 容器宽度不影响结论)。实体在 mock_main.c, driver 快照比对。
 *
 * 5. **PIEACK_GROUP1/2/3**。TI 头里的常量 (0x0001/0x0002/0x0004)。
 *
 * 6. **CPU_TIME_STRUCT 与 extern gCpuTime**。MotorStructDefine.h 提供类型,
 *    gCpuTime 的实体在 mock_main.c (原工程里它在 MotorVar.c, 本用例不链那个文件)。
 *    原工程 MotorInclude.h 第 54 行就是这个声明, 照抄。
 *
 * 7. **系统初始终例的 9 个原型** (原 MotorInclude.h 第 96~104 行): InitSysCtrl /
 *    InitInterrupt / InitPeripherals / InitForMotorApp / InitForFunctionApp /
 *    SetInterruptEnable / EnableDog / DisableDog / KickDog。main.c 自己不声明
 *    它们, 靠的就是真头这一串。DisableDog 本模块不调, 一并照抄 (零成本)。
 *
 * 8. **GpioDataRegs**。`DisableDrive()` 在真 SystemDefine.h 第 123 行是**宏**:
 *    `#define DisableDrive() GpioDataRegs.GPASET.bit.GPIO7 = 1` —— 不是函数调用!
 *    EPWM1_TZ_isr 调它, 所以寄存器必须真的在, 且 driver 要快照比对。
 *    MotorDefine.h 的 TurnOn/OffBrake 宏写 GPIO8, 也一并在模型里 (本模块不调,
 *    但头文件是共享的, 少了会在别处炸)。
 */

#ifndef MOTOR_INCLUDE_H
#define MOTOR_INCLUDE_H

#include "DataTypeDef.h"
#include "MotorDefine.h"
#include "MotorStructDefine.h"

extern void gHostCtlEvent(int idx);
extern volatile Uint16 gHostCpuCtl[7];

#define  EINT    gHostCtlEvent(0)
#define  DINT    gHostCtlEvent(1)
#define  ERTM    gHostCtlEvent(2)
#define  DRTM    gHostCtlEvent(3)
#define  EALLOW  gHostCtlEvent(4)
#define  EDIS    gHostCtlEvent(5)

#define  interrupt

#undef  GetTime
#define GetTime() gHostGetTime()

extern Ulong gHostGetTime(void);

#define PIEACK_GROUP1   0x0001
#define PIEACK_GROUP2   0x0002
#define PIEACK_GROUP3   0x0004

extern CPU_TIME_STRUCT gCpuTime;

/* ---- 把四个"写寄存器却什么都不说"的宏接上事件日志 ------------------------
 *
 * 首轮负向对照漏掉两条 (MN25/MN27), 病因相同: 这些副作用之间以及它们与调用
 * 日志之间的**先后**在宿主上看不见。修法不是改被测代码, 而是给 bench 的模型
 * 补上可观测性 —— 写入本身一字不改, 只是多记一条事件; 再加上 mock 在每个
 * 事件点存一份寄存器踪迹 (gHostRegTrace), 连 `PieCtrlRegs.PIEACK.all = ...`
 * 这种**源码里直接写**的赋值, 其相对于邻句的次序也能比出来。
 *
 *   DisableDrive       真 SystemDefine.h:123  GpioDataRegs.GPASET.bit.GPIO7 = 1
 *   ADC_CLEAR_INT_FLAG 真 SystemDefine.h:107  AdcRegs.ADCINTFLGCLR.bit.ADCINT1 = 1
 *   ADC_RESET_SEQUENCE 真 SystemDefine.h:108  AdcRegs.SOCPRICTL.bit.RRPOINTER = 0x20
 *
 * 目标端由 target_check.py 用真 cl2000 编真源码, 这三个宏在目标端仍是原样,
 * 不受影响。
 */
#undef  DisableDrive
#define DisableDrive()        gHostDisableDrive()
extern void gHostDisableDrive(void);

#undef  ADC_CLEAR_INT_FLAG
#define ADC_CLEAR_INT_FLAG    gHostAdcClearIntFlag()
extern void gHostAdcClearIntFlag(void);

#undef  ADC_RESET_SEQUENCE
#define ADC_RESET_SEQUENCE    gHostAdcResetSequence()
extern void gHostAdcResetSequence(void);

extern void InitSysCtrl(void);
extern void InitInterrupt(void);
extern void InitPeripherals(void);
extern void InitForMotorApp(void);
extern void InitForFunctionApp(void);
extern void SetInterruptEnable(void);
extern void EnableDog(void);
extern void DisableDog(void);
extern void KickDog(void);

struct GPIO_GPA_BITS {
    Uint GPIO0  : 1;
    Uint GPIO1  : 1;
    Uint GPIO2  : 1;
    Uint GPIO3  : 1;
    Uint GPIO4  : 1;
    Uint GPIO5  : 1;
    Uint GPIO6  : 1;
    Uint GPIO7  : 1;
    Uint GPIO8  : 1;
    Uint GPIO9  : 1;
    Uint GPIO10 : 1;
    Uint GPIO11 : 1;
    Uint GPIO12 : 1;
    Uint GPIO13 : 1;
    Uint GPIO14 : 1;
    Uint GPIO15 : 1;
    Uint GPIO16 : 1;
    Uint GPIO17 : 1;
    Uint GPIO18 : 1;
    Uint GPIO19 : 1;
    Uint GPIO20 : 1;
    Uint GPIO21 : 1;
    Uint GPIO22 : 1;
    Uint GPIO23 : 1;
    Uint GPIO24 : 1;
    Uint GPIO25 : 1;
    Uint GPIO26 : 1;
    Uint GPIO27 : 1;
    Uint GPIO28 : 1;
    Uint GPIO29 : 1;
    Uint GPIO30 : 1;
    Uint GPIO31 : 1;
};
union GPASET_REG {
    Uint32 all;
    struct GPIO_GPA_BITS bit;
};
struct GPIO_DATA_REGS {
    union GPASET_REG GPASET;
    union GPASET_REG GPACLEAR;
    union GPASET_REG GPADAT;
    union GPASET_REG GPATOGGLE;
};
extern struct GPIO_DATA_REGS GpioDataRegs;

struct ADCINTFLGCLR_BITS {
    Uint ADCINT1 : 1;
    Uint ADCINT2 : 1;
    Uint ADCINT3 : 1;
    Uint ADCINT4 : 1;
    Uint rsvd1   : 28;
};
union ADCINTFLGCLR_REG {
    Uint all;
    struct ADCINTFLGCLR_BITS bit;
};
struct SOCPRICTL_BITS {
    Uint RRPOINTER : 6;
    Uint rsvd1     : 26;
};
union SOCPRICTL_REG {
    Uint all;
    struct SOCPRICTL_BITS bit;
};
struct ADC_REGS {
    union ADCINTFLGCLR_REG ADCINTFLGCLR;
    Uint rsvd1[3];
    union SOCPRICTL_REG SOCPRICTL;
};
extern struct ADC_REGS AdcRegs;

struct ETCLR_BITS {
    Uint INT   : 1;
    Uint SOCA  : 1;
    Uint rsvd1 : 30;
};
/* 真 DSP2803x_EPwm.h 第 500 行起: ETPS/ETFLG/ETCLR/ETFRC 都是 EPWM_REGS 的
 * **直接成员** (没有 ET 子结构), 这里照这个形状给最小模型。 */
union ETCLR_REG {
    Uint all;
    struct ETCLR_BITS bit;
};
struct EPWM_REGS {
    Uint rsvd1[8];
    union ETCLR_REG ETCLR;
    Uint rsvd2[3];
};
extern struct EPWM_REGS EPwm1Regs;

struct PIEACK_BITS {
    Uint ACK1  : 1;
    Uint rsvd1 : 31;
};
union PIEACK_REG {
    Uint all;
    struct PIEACK_BITS bit;
};
struct PIE_CTRL_REGS {
    Uint rsvd1[4];
    union PIEACK_REG PIEACK;
};
extern struct PIE_CTRL_REGS PieCtrlRegs;

#endif
