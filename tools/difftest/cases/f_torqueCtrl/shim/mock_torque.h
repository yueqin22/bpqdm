/* f_torqueCtrl 差分测试的共享状态与 LineChangeCalc 调用日志。
 *
 * 放在 shim/ 是因为 shim/ 是唯一排在 include 路径上的"用例私有头文件"位置
 * (用例目录本身不在路径上, 只有 case/ref 与交付源码目录在)。它**不做任何头文件
 * 替换**, 只是让 mock_torque.c 与 driver.c 共用同一份声明, 免得两边各抄一份
 * 结构体然后慢慢漂移。
 *
 * 注意这个头文件会被 build_variant 跟着 shim/ 一起复制到负向对照的变体目录 ——
 * 漏掉它的后果和漏掉 real.txt 一样: 变体编不过, 被误记成 BROKEN。
 */
#ifndef MOCK_TORQUE_H
#define MOCK_TORQUE_H

#include "f_runSrc.h"       /* LINE_CHANGE_STRUCT */

/* itDisp 的真身在 f_menu.h, 但 src/function/f_torqueCtrl.c **不 include f_menu.h** ——
 * 它在自己文件里写了一句 `extern Uint16 itDisp;` (见交付源码第 21 行)。
 * driver.c 与 mock_torque.c 沿用同一句声明, 免得为了一个变量把整个 f_menu.h 拉进来
 * (那会带进一堆与本案无关的符号声明)。 */
extern Uint16 itDisp;

/* ---- LineChangeCalc 调用日志 ----
 *
 * f_torqueCtrl 与外部世界的**唯一**函数级依赖就是 LineChangeCalc: 模块在
 * AccDecTorqueCalc 里设置 torqurLine 的四个字段, 然后 torqurLine.calc(&torqurLine),
 * 再把 torqurLine.curValue 取回 upperTorque。
 *
 * 这个日志记的是**调用前**的入参快照。为什么光比对 torqurLine 的终值不够:
 * LineChangeCalc 是纯函数, 终值由入参决定, 所以比对终值确实能间接暴露入参差异 ——
 * 但前提是"调用发生了"。若改写把整次调用删掉 (例如把 calc 那行注释掉), 终值仍会
 * 因为模块自己写入的四个字段而留在原处, 而 upperTorque 只是回读 curValue,
 * 于是一处"少了滤波"的错误可能被终值比对掩盖。日志把"调用了几次、每次入参是什么"
 * 变成可观测量, 和 f_plc 的 CAN 调用日志是同一个道理。
 */
#define TORQUELOG_N     8

struct TORQUECALL
{
    int32  aimValue;
    int32  curValue;
    int32  maxValue;
    Uint32 tickerAll;
    int32  remainder;
};

extern struct TORQUECALL torqueLog[TORQUELOG_N];
extern Uint16 torqueLogN;

#endif
