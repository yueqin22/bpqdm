/* Host-side wrapper for DSP2803x_Gpio.h -- f_eeprom 用例私有。

 * 真文件由本文件用绝对路径 include 进来 (run_case.py 的 real.txt 会再生成一层
 * 转发头, 但本用例**故意不把 DSP2803x_Gpio.h 列进 real.txt** —— 列进去的话
 * 生成目录 (排在最前) 里的转发头会先被找到, 下面这个包装就失效了)。

 * ---- 为什么要包装 ---------------------------------------------------------

 * C28x 的 GPIO 数据寄存器是**三件套**: GPBDAT (读回当前电平/输出锁存)、
 * GPBSET (写 1 置位)、GPBCLEAR (写 1 清零)。在真芯片上, 写 1 到 GPBSET 的第 n
 * 位之后, 再读 GPBDAT 的第 n 位就会看到 1; 写 GPBCLEAR 则是 0。这是寄存器的
 * 写语义, 但一个普通内存结构体不会自动实现它。

 * f_eeprom 的软件模拟 I2C **整个协议本体**就是对这些寄存器的读写:
 *
 *     I2cDataHigh()   -> GpioDataRegs.GPBSET.bit.GPIO32   = 1
 *     I2cDataLow()    -> GpioDataRegs.GPBCLEAR.bit.GPIO32 = 1
 *     I2cClkHigh()    -> GpioDataRegs.GPBSET.bit.GPIO33   = 1
 *     I2cClkLow()     -> GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1
 *     #define SDA     (GpioDataRegs.GPBDAT.bit.GPIO32)
 *     #define SCL     (GpioDataRegs.GPBDAT.bit.GPIO33)

 * 若不实现这个联动, GPBDAT 从初始化之后就永远不动, 于是 I2CStop / I2CNoACK /
 * I2CRcvByte 的整段位翻转 —— 以及 I2cDealBeforeInit 里两次 `SDA` 读 ——
 * 在宿主上都变成"没变化", 那些函数的改写就完全测不出来。这不是"给测试台加
 * 便利", 而是**复刻被测对象所依赖的硬件语义**, 与提供寄存器结构体本身同理。

 * ---- 怎么做 ---------------------------------------------------------------

 * 把 `GpioDataRegs` 重定义成一个函数调用: 每次**访问** GpioDataRegs 时, 先
 * 由 gpio_apply() 把 GPBSET/GPBCLEAR 的待处理位搬进 GPBDAT, 再把真结构体返回。
 * 于是
 *     GpioDataRegs.GPBSET.bit.GPIO32 = 1;      // 写进 GPBSET 字段
 *     x = GpioDataRegs.GPBDAT.bit.GPIO32;      // 这一次访问把上面那位搬过来
 * 的语义就对了。搬运规则见 mock_eeprom.c 的 gpio_apply()。

 * `GpioCtrlRegs` **不**需要包装: GPBDIR/GPBPUD/GPBQSEL1/GPBMUX1 都是普通
 * 可读写寄存器, 没有 set/clear 侧写语义。它们的位域布局必须保持真身 ——
 * I2cDealBeforeInit 读 `SDA` 的结果依赖 GPBDIR.bit.GPIO32 的方向, 所以这个
 * 结构体不能被替换成简化版。

 * 注意: 真 DSP2803x_Gpio.h 有 `#ifdef __cplusplus` 包裹与自己的 include guard,
 * include 进来是安全的 (它不 include 任何别的东西 —— 纯结构体 + extern)。
 */
#ifndef F_EEPROM_HOST_GPIO_H
#define F_EEPROM_HOST_GPIO_H

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

/* 真头文件。绝对路径硬写: 用例目录以后挪位置也不会断。 */
#include "E:/software/bpqdm/bpqdm_rewrite/include/device/DSP2803x_Gpio.h"

/* gpio_apply() 把 GPBSET/GPBCLEAR 的待处理位应用到 GPBDAT, 并把两个寄存器
 * 的"待处理"状态清空 (真硬件是瞬时的: 写进去那一拍就生效了)。
 *
 * 返回 &GpioDataRegs, 于是 `#define GpioDataRegs (*gpio_data_access())` 里
 * `GpioDataRegs.GPBSET.bit.GPIO32 = 1` 展开成
 * `(*gpio_data_access()).GPBSET.bit.GPIO32 = 1` —— 左值仍然可写。
 *
 * 它只碰 GPBDAT / GPBSET / GPBCLEAR (GPIO32、GPIO33 两位, 以及 GPBCLEAR 的
 * 整字), 其余位原样保留。 */
volatile struct GPIO_DATA_REGS *gpio_data_access(void);

#ifdef GpioDataRegs
#undef GpioDataRegs
#endif
#define GpioDataRegs (*gpio_data_access())

#endif
