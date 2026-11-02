/* Host-side replacement for f_funcCode.h -- f_dspcan 用例私有。

 * f_dspcan.c 第 4 行 include 了 f_funcCode.h, 但**一个符号都没用它**: 该模块用到的
 * CAN_BAUD / CAN_BAUD_SUM / CAN_INIT_* / CAN_MBOX_* / ECANREGS / ECANMBOXES /
 * ECANLAMS 全来自 f_dspcan.h, 类型与 EALLOW/EDIS 来自 DSP28x_Project.h。
 *
 * 真的 f_funcCode.h 会拉进 f_debug.h / f_interface.h / f_p2p.h 三条链 (f_menu 用例
 * 为此不得不把整棵头文件树转发进来), 对本模块而言全是负担, 而且它们引用的一堆
 * 全局量本模块根本不碰。所以这里给一个空壳 —— 语义上等价 (没有符号被使用), 而且
 * 万一以后改写真的开始用 f_funcCode.h 里的东西, 宿主会立刻编不过, 是**响亮**的
 * 失败而不是静默的错。
 */
#ifndef __F_FUNCCODE_H__
#define __F_FUNCCODE_H__

#endif
