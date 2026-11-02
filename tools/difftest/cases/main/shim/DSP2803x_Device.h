/* Host-side replacement for DSP2803x_Device.h -- main 用例私有。
 *
 * 与 MotorDebug / MotorCurrentTransform 用例同一份底子 (类型宽度钉死 + TARGET +
 * BIT0..BIT15), 另外补两样本模块要用的东西:
 *
 *   1. `static int32 __IQsat(int32 A, int32 Pos, int32 Neg)` —— C28x 编译器内建,
 *      不是头文件宏 (工程里只有调用点没有定义, 目标端零告警编过即证)。
 *      main.c 第 110 行 `gCpuTime.Det05msClk = __IQsat(m_DetaTime, 65535,
 *      C_TIME_05MS);` 是**活代码** (CPU_TIME_DEBUG 没定义, 但那行在 #ifdef 外面)。
 *      语义与 SubPrg 用例同一份记录: min(max(A,Neg),Pos), 32 位 ACC。
 *      用 static 函数而不是宏: 宏会重复求值实参。
 *
 *   2. 不提供 EALLOW/EINT/... —— 那些由本用例的 MotorInclude.h 定义成
 *      "计数 + 事件日志" 的形式 (main.c 的 ISR 里成对出现, 顺序可观测)。
 *
 * 真文件不能转发: `extern cregister volatile unsigned int IFR;` (cregister 是
 * C28x 关键字) + EINT/DINT/EALLOW 被定义成 asm(" clrc INTM") 这类内联汇编。
 * SystemDefine.h 无条件 include 它, 绕不开, 所以整份换掉。
 */

#ifndef DSP2803X_DEVICE_H
#define DSP2803X_DEVICE_H

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

#endif
