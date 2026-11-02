/* Host-side replacement for DSP2803x_Examples.h -- f_eeprom 用例私有。

 * 真文件不能转发: 它 `#include "DSP2803x_GlobalPrototypes.h"` (全是
 * `__interrupt void ...` 原型) 与 `DSP2803x_DefaultISR.h`, 两者都是 C28x 关键字,
 * MSVC 编不过。

 * 本模块用到的东西就两样: DSP_CLOCK (InitSetI2ca 的 `#if (DSP_CLOCK == 100)`
 * 选 I2CPSC) 与 DELAY_US (I2C 位翻转延时). 这里按原值保留 (60 MHz 是这块板子的
 * 真实主频, CPU_RATE 16.667L 与真文件一致)。

 * ---- 为什么 DELAY_US 必须留成真宏 ---------------------------------------

 * DELAY_US(A) 展开成一次 DSP28x_usDelay(N) 调用。N 由 A 算出:
 *     A=20000 -> (20000*1000/16.667 - 9)/5 = 239994
 *     A=5     -> (5*1000/16.667 - 9)/5     = 58.2 -> 58 (截断)
 * 它是一个**可观测的调用**: mock 侧把 DSP28x_usDelay 换成对计数表的累加, 于是
 * "改写把某处 DELAY_US 删了 / 改了参数" 会体现为那张表对不上。若在这里把
 * DELAY_US 变成空语句, 这类变异就全部不可见 —— 而本模块里 DELAY_US 出现了 16 次,
 * 是 I2C 位操作时序的全部内容。

 * 参数类型与真文件一致 (long double), 截断点才不会移动。
 */
#ifndef DSP2803x_EXAMPLES_H
#define DSP2803x_EXAMPLES_H

/* 真 DSP2803x_Examples.h 里 I2C 那一路是这样的:
 *     21  #include "DSP2803x_GlobalPrototypes.h"
 *     23  #include "DSP2803x_EPwm_defines.h"
 *     24  #include "DSP2803x_I2C_defines.h"     <-- I2C_SUCCESS / I2C_NO_ISRC /
 *     25  #include "DSP2803x_Cla_Defines.h"         I2C_SCD_ISRC / I2C_ARDY_ISRC /
 *     46  #include "DSP2803x_DefaultISR.h"          I2C_CLR_NACK_BIT / I2C_BUS_BUSY_ERROR /
 *                                                   I2C_STP_NOT_READY_ERROR
 * 前两个 (GlobalPrototypes / DefaultISR) 是 `__interrupt` 原型, MSVC 编不过,
 * 刻意不拉; 但 **I2C_defines 必须拉** —— f_eeprom.c 直接用了上面那 6 个常量,
 * 而且没有任何一处显式 include 它, 真工程里正是靠这条链带进来的。
 * 少了它, f_eeprom.c 在宿主上根本编不过 (而目标端却编得过) —— 那会让本用例
 * 从"测不出差异"变成"压根没测"。 */
#include "DSP2803x_I2C_defines.h"

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

/* DSP28x_usDelay 由用例 mock 提供 (签名与真 DSP2803x_usDelay.asm 的 C 原型一致:
 * `void DSP28x_usDelay(long LoopCount)`)。它**不是**空函数 —— 见文件头说明。 */
void DSP28x_usDelay(long LoopCount);

#define DELAY_US(A)  DSP28x_usDelay(((((long double) A * 1000.0L) / (long double)CPU_RATE) - 9.0L) / 5.0L)

#endif
