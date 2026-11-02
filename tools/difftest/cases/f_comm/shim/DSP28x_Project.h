/* Host-side replacement for DSP28x_Project.h -- f_comm case private.
 *
 * f_comm.c 的第一行是 `#include "f_comm.h"`, 真 f_comm.h 又拉进 f_funcCode.h,
 * 而 f_funcCode.h 的第 5 行就是 `#include "DSP28x_Project.h"`。所以这个头是
 * **整条包含链上第一个被解析的东西**, 宽度必须在这里钉死, 否则后面每个
 * `Uint16` 都已经是 32 位了, 再钉也没用。
 *
 * 真 DSP28x_Project.h 只有两行 include:
 *     DSP2803x_Device.h   -- 器件头, 宿主编不过 (cregister / asm() / interrupt)
 *     DSP2803x_Examples.h -- DSP_CLOCK 60 + 一堆原型, 宿主可以照收
 * 这里把两者都接上: 器件头换成用例 shim (case_shim 在 include 路径上先于
 * include/device), Examples 走 real.txt 的转发。
 *
 * 与 f_modbus 的 shim 不同, **不能**把外设模型丢掉: f_comm.c 直接写
 * LinaRegs / GpioCtrlRegs / GpioDataRegs / PieCtrlRegs / PieVectTable / IER,
 * 这些寄存器既是它的输出面也是它的输入面 (SCIFLR 的错误位决定要不要重新
 * 初始化 SCI)。所以器件头里那一串 DSP2803x_*.h 必须真转发进来。
 *
 * 宽度钉法与 f_modbus 一致: 用公共 shim 的 DataTypeDef.h 把 u16/u32 钉到
 * uint16_t/uint32_t; DSP2803x_Device.h (用例 shim) 再提供 Uint16/Uint32/int16/
 * int32。两者都带 DSP28_DATA_TYPES 之外的独立守卫, 不会互相顶掉。
 */
#ifndef DSP28X_PROJECT_H
#define DSP28X_PROJECT_H

#include "DataTypeDef.h"

/* 真 DSP28x_Project.h 经由 DSP2803x_Examples.h 带来 DSP_CLOCK 60。
 * f_debug.h 的 TIME_10US / TIME_1MS 全靠它, 缺了会让时间比较静默失效。
 * 60 MHz 是这块板子的真实主频 (project/jt380.pjt 里 DSP_CLOCK 也是 60)。 */
#define DSP_CLOCK   60

/* 器件模型 + 外设寄存器结构。cregister / asm() / interrupt 三个 C28x 专有构造
 * 都在那份 shim 里逐条答复过了, 理由见该文件顶部。 */
#include "DSP2803x_Device.h"

/* DSP_CLOCK / CPU_RATE / PARTNO_* / EPwm / I2C / Cla 的常量都在这儿。
 * 它自己 include 的 GlobalPrototypes.h 等一律按相对 include/device/ 解析,
 * 所以只需要把这一份转发出来 (见 real.txt)。 */
#include "DSP2803x_Examples.h"

#endif
