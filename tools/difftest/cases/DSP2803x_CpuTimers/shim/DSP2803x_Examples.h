/* Host-side replacement for DSP2803x_Examples.h -- DSP2803x_CpuTimers 用例私有。

 * 真的 DSP2803x_Examples.h 里本模块唯一用得上的东西是第 9 行的
 *     #define DSP_CLOCK 60
 * (60 MHz 是这块板子的真实主频, 不是猜的)。
 *
 * 真文件其余部分不能转发: 它 `#include "DSP2803x_GlobalPrototypes.h"`, 而那里
 * 全是 `__interrupt void ...` 形式的中断原型 —— `__interrupt` 是 C28x 关键字,
 * MSVC 编不过; 紧接着的 DSP2803x_DefaultISR.h 同理。
 *
 * 本模块不引用 DELAY_US / CPU_RATE / Device_cal, 所以只保留 DSP_CLOCK。
 */
#ifndef DSP2803x_EXAMPLES_H
#define DSP2803x_EXAMPLES_H

#define DSP_CLOCK   60

#endif
