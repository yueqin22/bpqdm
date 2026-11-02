/* f_vf 差分测试的共享状态与 LineChangeCalc 调用日志。
 *
 * 放在 shim/ 是因为 shim/ 是唯一排在 include 路径上的"用例私有头文件"位置
 * (用例目录本身不在路径上, 只有 case/ref 与交付源码目录在)。它**不做任何头文件
 * 替换**, 只是让 mock_vf.c 与 driver.c 共用同一份声明, 免得两边各抄一份
 * 结构体然后慢慢漂移。
 *
 * 注意这个头文件会被 build_variant 跟着 shim/ 一起复制到负向对照的变体目录 ——
 * 漏掉它的后果和漏掉 real.txt 一样: 变体编不过, 被误记成 BROKEN。
 */
#ifndef MOCK_VF_H
#define MOCK_VF_H

#include "f_runSrc.h"       /* LINE_CHANGE_STRUCT */

/* ---- LineChangeCalc 调用日志 ----
 *
 * vfSeparateDeal 与外部世界的**唯一**函数级依赖就是 LineChangeCalc: 模块把
 * vfSeprateVolAim 与 vfSeparateVol 搬进 vfSeprateVolAccLine 的四个字段, 调
 * .calc(&vfSeprateVolAccLine), 再把 curValue 取回 vfSeparateVol。
 *
 * 这个日志记的是**调用前**的入参快照。为什么光比对 vfSeprateVolAccLine 的终值
 * 不够: LineChangeCalc 是纯函数, 终值由入参决定, 所以比对终值确实能间接暴露
 * 入参差异 —— 但前提是"调用发生了"。若改写把整次调用删掉 (例如把 calc 那行注释
 * 掉), 终值仍会因为模块自己写入的四个字段而留在原处, 而 vfSeparateVol 只是回读
 * curValue, 于是一处"少了滤波"的错误可能被终值比对掩盖。日志把"调用了几次、每次
 * 入参是什么"变成可观测量, 和 f_torqueCtrl / f_plc 的做法一致。
 */
#define VFLOG_N     8

struct VFCALL
{
    int32  aimValue;
    int32  curValue;
    int32  maxValue;
    Uint32 tickerAll;
    int32  remainder;
};

extern struct VFCALL vfLog[VFLOG_N];
extern Uint16 vfLogN;

/* ---- 三个"值源"函数的驱动值 --------------------------------------------
 *
 * UpdateMultiSetFrq / FrqPlcSetDeal / FrqPidSetDeal 定义在 src/function/f_frqSrc.c
 * (f_frqSrc 家族, 不在本次改写范围内)。它们在本模块里只出现在
 * `(int32)X * ratingVoltage / maxFrq` 这个式子里 —— 也就是被测模块把它们当成
 * **一个数字**, 拿到手就做算术。所以这里用驱动可控的桩, 而不是逐字照抄原实现。
 *
 * 这与 f_torqueCtrl 里照抄 LineChangeCalc 的选择**不同**, 理由是两者角色不同:
 *   - LineChangeCalc 是本模块**自己的斜坡滤波器**, 它决定模块的输出语义,
 *     所以必须忠实 (照抄);
 *   - 这三个是**取值来源**, 它们的返回值是模块的输入。逐字照抄反而更糟: 会把
 *     f_frqSrc 自己的多段速表、PLC 段位状态一并拖进来, 那边任何一处不一致都会
 *     被误报成 f_vf 的不一致。
 *
 * 桩的返回值由 driver.c 逐用例设置, 两侧读到的是同一个数, 比对才有意义。
 */
extern int32 mockMultiSetFrq;
extern int32 mockPlcFrq;
extern int32 mockPidFrq;

/* ---- 桩的调用轨迹 --------------------------------------------------------
 *
 * 桩的返回值是驱动给的, 但**调用本身**仍是被测模块的行为, 必须可观:
 *
 *   - 有没有调 (改写把某个 case 的调用删掉/挪到别的 case);
 *   - 传了什么实参 —— 这里就是 diFunc.f1.bit.multiSet 的值。若只比对返回值,
 *     一个把 `diFunc.f1.bit.multiSet` 换成别的位域 (例如 f2.bit.multiSet)
 *     的改写, 只要那个位域恰好取到同值就抓不住。把实参记下来才闭合这个缺口;
 *   - 调了几次、什么顺序 (正常情况下每次 vfSeparateDeal 至多命中一个 case,
 *     所以轨迹长度 ≤ 1, 长度变了就是改写动了控制流)。
 */
#define VFTRACE_N   4

#define VFTRACE_MULTISET    1
#define VFTRACE_PLC         2
#define VFTRACE_PID         3

struct VFTRACE
{
    Uint16 which;
    Uint16 arg;
};

extern struct VFTRACE vfTrace[VFTRACE_N];
extern Uint16 vfTraceN;

#endif
