/* f_plc 差分测试的共享状态与 CAN 调用日志。
 *
 * 放在 shim/ 是因为 shim/ 是唯一排在 include 路径上的"用例私有头文件"位置
 * (用例目录本身不在路径上, 只有 case/ref 与交付源码目录在)。它**不做任何头文件
 * 替换**, 只是让 mock_plc.c 与 driver.c 共用同一份声明, 免得两边各抄一份结构体
 * 然后慢慢漂移。
 *
 * 注意这个头文件会被 build_variant 跟着 shim/ 一起复制到负向对照的变体目录 ——
 * 漏掉它的后果和漏掉 real.txt 一样: 变体编不过, 被误记成 BROKEN。
 */
#ifndef MOCK_PLC_H
#define MOCK_PLC_H

#include "f_funcCode.h"
#include "f_canlink.h"

/* UpdataFuncCodeGrade() 的替身控制。
 *
 * 默认走 funcCodeGradeAll (由 mock_plc.c 从 grade_tables.inc 定义, 与工程真表
 * 同源), 所以模块看到的等级表就是工程自己的那一张。需要探测真表掩盖掉的边界时,
 * driver 把 mockUseOverride 置 1 并填 mockFuncCodeGradeOverride[]。
 *
 * 两侧共用同一份 mock, 所以"相同输入 -> 相同输出"的判据不受影响。但要说清:
 * 本用例证明的是"给定同一张等级表, ref 与 new 的判定逻辑相同", 不是"在真表下
 * 判定结果符合规格"。 */
extern Uint16 mockUseOverride;
extern Uint16 mockFuncCodeGradeOverride[FUNCCODE_GROUP_NUM];

/* ---- CAN 调用日志 ----
 *
 * f_plc 通过 CanControlWriter / CanControlRead / CanlinkDataTran 三个函数与硬件
 * 交互。它们本身是副作用, 没有返回值可比, 所以把**调用序列**记下来当观测量:
 * 少调一次、调错参数、顺序变了, 都会在比对时暴露。
 */
#define CANLOG_N       1024
#define CANSNAP_WORDS  8

#define CANCALL_WRITER 0
#define CANCALL_READ   1
#define CANCALL_TRAN   2

struct CANCALL
{
    Uint16 kind;
    Uint16 a;
    Uint16 b;
    Uint16 c;
    Uint16 nword;
    Uint16 snap[CANSNAP_WORDS];
};

extern struct CANCALL canLog[CANLOG_N];
extern Uint16 canLogN;

#endif
