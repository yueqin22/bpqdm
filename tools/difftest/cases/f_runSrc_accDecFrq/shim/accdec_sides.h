/* f_runSrc_accDecFrq 差分测试: 两侧符号的声明。
 *
 * ref_tu.c / new_tu.c 各自把模块的 9 个外部符号加了 ref_ / new_ 前缀
 * (清单来自 nm2000 读出的符号表, 不是人眼数的), 这里为 driver 声明出来。
 *
 * 注意 SCurveOridinaryCalc / SCurveSpecialCalc **是外部链接的**: f_debug.h
 * 里 `LOCALF` 在当前配置下展开成空 (f_debug.h:27-33 的 `#if 1` 分支),
 * 所以它们是普通的外部函数, 不是 static。这一点很重要 —— 驱动可以直接调它们,
 * 从而把状态机的入口状态 (bFrqCurAimChg / accDecStatus / benchFrq / accDecTime)
 * 精确摆好, 不用绕 AccDecFrqCalc 的守卫链。
 *
 * 拆成函数与数据两组, 只因为 driver 用它们的方式不同: 数据是"装进去 / 读回来"的
 * 观测对象, 函数是"调哪一侧"的入口。
 */
#ifndef ACCDEC_SIDES_H
#define ACCDEC_SIDES_H

#include "f_runSrc.h"

/* ---- 函数 ---------------------------------------------------------------- */

void  ref_AccDecFrqCalc(int32 accTime, int32 decTime, Uint16 mode);
void  new_AccDecFrqCalc(int32 accTime, int32 decTime, Uint16 mode);

int32 ref_SCurveOridinaryCalc(int32 frq0, int32 accDecTime);
int32 new_SCurveOridinaryCalc(int32 frq0, int32 accDecTime);

int32 ref_SCurveSpecialCalc(int32 frq0, int32 accDecTime);
int32 new_SCurveSpecialCalc(int32 frq0, int32 accDecTime);

void  ref_LineChangeCalc(LINE_CHANGE_STRUCT *p);
void  new_LineChangeCalc(LINE_CHANGE_STRUCT *p);

/* ---- 数据 (模块自己定义的文件级全局量) ---------------------------------- */

extern int32  ref_frqCurAimOld;
extern int32  new_frqCurAimOld;
extern int32  ref_accTimeOld;
extern int32  new_accTimeOld;
extern int32  ref_decTimeOld;
extern int32  new_decTimeOld;
extern Uint16 ref_bFrqCurAimChg;
extern Uint16 new_bFrqCurAimChg;

extern LINE_CHANGE_STRUCT ref_frqLine;
extern LINE_CHANGE_STRUCT new_frqLine;

#endif
