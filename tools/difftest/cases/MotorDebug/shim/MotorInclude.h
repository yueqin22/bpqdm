/* Case-private host-side MotorInclude.h -- MotorDebug 用例。
 *
 * 为什么不用公共 shim: 公共的 MotorInclude.h 只是个 DataTypes + EQEP/CPU timer
 * + gMainCmd 的最小桩, 而本模块读 gTestDataReceive.TestData12..17 和
 * gMainCmd.Command.bit.Start, 写 pVD1..pVD4 / saveIndex* / gDebugBuffer,
 * 必须让声明这些量的真实结构体定义可见。
 *
 * 为什么不用真的 MotorInclude.h: 它拉起 17 个头文件外加整套外设寄存器模型,
 * 并且声明 `extern interrupt void ..._isr(void)` —— MSVC 不认 interrupt 存储类
 * (MotorEncoder 的 host_patch.json 里记着同一件事)。
 *
 * 本文件做的事: 按依赖顺序重放真实链条的一个子集, 到够用为止。
 *   DataTypeDef.h -> SystemDefine.h -> MotorDefine.h -> MotorStructDefine.h
 * 再补两个 extern。MotorStructDefine.h 自己不含 #include, 只需要
 * Uint/Uint16/Ulong/int 这几个基础类型, 所以放在 MotorDefine.h 之后就能用。
 *
 * `MOTOR_INCLUDE_H` 卫哨的作用与 MotorCurrentTransform 用例相同: 本模块 include
 * 的 SubPrgInclude.h 也会 include 它, 卫哨保证不会递归; 而 SubPrgInclude.h 自己
 * 仍然提供 Filter 宏 (本模块其实不用)。
 *
 * 这里不声明任何硬件寄存器对象 —— 本模块四个函数全是在 DebugBuffer 和
 * gTestDataReceive 上的定点逻辑, 只有 C_DEBUG_BUFF_SIZE 这个编译期常量。
 */

#ifndef MOTOR_INCLUDE_H
#define MOTOR_INCLUDE_H

/* s16/u16/s32/u32 在这里 (公共 shim 的 DataTypeDef.h), 必须先来。 */
#include "DataTypeDef.h"

#include "SystemDefine.h"
#include "MotorDefine.h"
#include "MotorStructDefine.h"

extern MOTOR_DEBUG_DATA_RECEIVE_STRUCT     gTestDataReceive;

extern BASE_COMMAND_STRUCT                 gMainCmd;

/* Declared by the real MotorInclude.h (lines 68-70) -- MotorDebug.c calls
 * SaveDebugData16 and ResetDebugBuffer *before* their own definitions further
 * down the file, so without these prototypes MSVC invents an implicit
 * `extern int ...()` declaration that then collides with the real definition
 * (C2371).  Copied verbatim from the real header.  DebugSaveDeal is NOT
 * declared anywhere in the real tree (MotorMain.c calls it implicitly too);
 * nothing inside this module calls it, so nothing is needed for it here. */
extern void SaveDebugData16(Uint);
extern void SaveDebugData32(unsigned long);
extern void ResetDebugBuffer(void);

#endif
