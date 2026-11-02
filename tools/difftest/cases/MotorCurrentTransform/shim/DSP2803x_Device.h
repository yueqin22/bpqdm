/* Host-side replacement for DSP2803x_Device.h -- MotorCurrentTransform 用例私有。
 *
 * 真的 DSP2803x_Device.h 不能转发:
 *   - `extern cregister volatile unsigned int IFR;` —— `cregister` 是 C28x 专有
 *     关键字, MSVC 不认;
 *   - EINT/DINT/EALLOW/... 被定义成 `asm(" clrc INTM")` 这类内联汇编宏。
 *
 * 本模块 (MotorCurrentTransform) 一个字都不碰外设寄存器: 四个函数全是
 * UVW/AlphaBeta/MT/AmpTheta 结构体上的定点算术, 只有 qsin/qatan/qsqrt/atan
 * 这几个纯函数依赖。它之所以还是会被拉进 DSP2803x_Device.h, 是因为
 * SystemDefine.h 无条件 `#include "DSP2803x_Device.h"`, 而
 * MotorCurrentTransform.h 又无条件 include SystemDefine.h。
 *
 * 所以这里只提供链路上真正要用的东西:
 *   - DSP28_DATA_TYPES 那一组 int16/int32/Uint16/... (宽度与 C28x 一致);
 *   - TARGET 与 BIT0..BIT15 (SystemDefine.h 的宏会用到);
 *   - TMS320F28035 等型号开关留给 SystemDefine.h 自己定义, 这里不碰。
 *
 * 类型宽度必须钉死: 真文件把 Uint16 定义成 `unsigned int`(C28x 16 位,
 * 宿主 32 位), 不钉会让每个 u16 悄悄变宽。DSP28_DATA_TYPES 卫哨与真文件同名,
 * 两边都包含也只生效一次。
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
typedef double              float64;
#endif

#define   TARGET   1

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

#endif
