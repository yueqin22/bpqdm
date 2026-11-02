/* Host-side replacement for DSP2803x_Examples.h -- MotorInvProtect 用例私有。

 * 真文件不能转发: 它 `#include "DSP2803x_GlobalPrototypes.h"` (全是
 * `__interrupt void ...` 原型) 与 `DSP2803x_DefaultISR.h`, 两者都是 C28x 关键字,
 * MSVC 编不过。
 *
 * 真文件里被本项目用到的东西只有三样: DSP_CLOCK / CPU_RATE / DELAY_US 宏, 以及
 * 那一批 PARTNO_* 常量。这里按原值保留 (60 MHz 是这块板子的真实主频, CPU_RATE
 * 16.667L 与真文件一致)。Device_cal 那个函数指针常量也照抄, 它被启动代码引用。
 *
 * 本模块用到 DSP_CLOCK 的地方只有一处:
 *     OutputPhaseLoseDetect: `gPhaseLose.Time < (Ulong)DSP_CLOCK * 1000000L`
 * DSP_CLOCK 漏了, 那个 `#if` 不会静默出错 —— 它是直接用的宏, 漏了就是编译错误。
 * 但 CPU_RATE / PARTNO_* 是别的模块的头文件链要的, 一并保留以保持同一套环境。
 */
#ifndef DSP2803x_EXAMPLES_H
#define DSP2803x_EXAMPLES_H

#define     DSP_CLOCK           60

#define     CPU_TYPE            28034

#define DSP28_DIVSEL   2

#define DSP28_PLLCR   12

#define CPU_RATE   16.667L

#define Device_cal (void   (*)(void))0x3D7C80

#define PARTNO_28030PAG   0xAA
#define PARTNO_28030PN    0xAB
#define PARTNO_28031PAG   0xAE
#define PARTNO_28031PN    0xAF
#define PARTNO_28032PAG   0xB2
#define PARTNO_28032PN    0xB3
#define PARTNO_28033PAG   0xB6
#define PARTNO_28033PN    0xB7
#define PARTNO_28034PAG   0xBA
#define PARTNO_28034PN    0xBB
#define PARTNO_28035PAG   0xBE
#define PARTNO_28035PN    0xBF

#define DELAY_US(A)  DSP28x_usDelay(((((long double) A * 1000.0L) / (long double)CPU_RATE) - 9.0L) / 5.0L)

#endif
