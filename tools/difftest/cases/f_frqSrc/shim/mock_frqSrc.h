/* f_frqSrc 差分测试的 mock 接口。

 * mock_frqSrc.c 提供模块读到的全部外部数据, 以及三个外部函数桩。数据用**真类型**
 * (从 include/ 里的 extern 声明抄来的), 原因见 mock_frqSrc.c 顶部。
 *
 * 这里额外提供三样东西给 driver 用:
 *
 *   1. mockPidFrq      -- FrqPidSetDeal() 的返回值 (PID 频率源);
 *   2. mockDigit[5]    -- GetNumberDigit1() 往 digit[] 里写的五位数字
 *                         (模块用 digit[runSrc] 选运行命令绑定号);
 *   3. mockTorqueAim   -- TorqueCalc() 写进 torqueAim 的值, 用来分别走
 *                         "转矩给定 >= 0" 和 "< 0" 两条分支。
 *
 * 再加一条**调用轨迹**: 三个桩各自记一条 (谁、什么实参、什么顺序)。只比终值会漏掉
 * "整次调用被删掉"或"两个桩调换顺序"这类错 —— 那些错在终值上可能完全看不出来。
 */
#ifndef MOCK_FRQSRC_H
#define MOCK_FRQSRC_H

#include "DataTypeDef.h"

#define FRQTRACE_N 8

#define FRQTRACE_PID     0
#define FRQTRACE_DIGIT   1
#define FRQTRACE_TORQUE  2

struct FRQTRACE
{
    Uint16 which;
    Uint16 arg;
};

extern struct FRQTRACE frqTrace[FRQTRACE_N];
extern Uint16 frqTraceN;

extern int32  mockPidFrq;
extern Uint16 mockDigit[5];
extern int16  mockTorqueAim;

#endif
