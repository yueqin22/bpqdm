/* Host-side replacement for DSP2803x_Device.h -- DSP2803x_CpuTimers 用例私有。

 * 为什么不用真身:
 *   真的 DSP2803x_Device.h 第 30~31 行是
 *       extern cregister volatile unsigned int IFR;
 *       extern cregister volatile unsigned int IER;
 *   `cregister` 是 C28x 专有关键字, MSVC 不认; 第 33~38 行还把 EINT/DINT/EALLOW
 *   定义成 `asm(" clrc INTM")` 这种内联汇编宏。宿主编不过, 也没有意义 ——
 *   本模块一个字都不碰这些。
 *
 * 为什么只摘掉外围、保留类型:
 *   模块用到 Uint16 / Uint32 / float。**类型宽度必须钉死**: 真 Device.h 把
 *   Uint16 定义成 `unsigned int`(C28x 上 16 位, 宿主上 32 位)。转发进来会让每个
 *   u16 悄悄变宽, 溢出与截断点全部位移 —— 那正是差分测试要测的东西, 不能让它
 *   先被测试台自己改掉。DSP28_DATA_TYPES 这个卫哨与真文件同名, 所以两边都包含
 *   也只会生效一次。
 *
 * 寄存器结构体走真身:
 *   DSP2803x_CpuTimers.h 里的 TCR_BITS / CPUTIMER_REGS 用**位域**描述寄存器。
 *   位域布局是"宿主和目标端可能不一致"的典型位置, 手抄一份就等于把被测对象换成
 *   了仿制品。所以这一份仍然转发到真头文件 (见 real.txt), 布局由
 *   tools/difftest/target_layout.py 用实测尺寸/偏移复核。
 */
#ifndef DSP2803x_DEVICE_H
#define DSP2803x_DEVICE_H

/* int16_t / uint16_t 这些来自 <stdint.h>。真 Device.h 不需要它, 因为它直接
 * 拿 `int` / `unsigned int` 当 16 位用 —— 宿主上那是 32 位, 所以这里必须走
 * 精确宽度类型, 而精确宽度类型要先把这个头拉进来。 */
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

/* 真 Device.h 的 BITn 宏, 项目头文件随手就用。 */
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

#include "DSP2803x_CpuTimers.h"

#endif
