/* f_runSrc 差分测试的 mock 接口。
 *
 * mock_runsrc.c 提供模块读到的全部 47 个外部数据全局量, 以及 4 个外部函数桩。
 * 数据用**真类型** (从 include/ 里的 extern 声明抄来的, 每条都在 .c 里标了出处),
 * 理由见 mock_runsrc.c 顶部。
 *
 * 这里额外提供两样东西给 driver 用:
 *
 *   1. mockErrorAttrLevel -- GetErrorAttribute() 的返回值。模块只在一处用它
 *      (LoseLoadRunCtrl: `GetErrorAttribute(ERROR_LOSE_LOAD) == ERROR_LEVEL_RUN`),
 *      所以给一个可设的等级就够, 不用真的实现那张错误属性表。
 *
 *   2. 一条**调用轨迹**: 四个桩各自记一条 (谁、什么实参、什么顺序)。
 *      只比终值会漏掉"整次调用被删掉"或"两个桩调换顺序"这类错 ——
 *      那些错在终值上可能完全看不出来。
 */
#ifndef MOCK_RUNSRC_H
#define MOCK_RUNSRC_H

#include "DataTypeDef.h"

#define RUNTRACE_N 16

#define RUNTRACE_UPD_FRQ_AIM    0
#define RUNTRACE_ACC_DEC_FRQ    1
#define RUNTRACE_GET_ERR_ATTR   2
#define RUNTRACE_SAVE_TUNE_DATA 3

struct RUNTRACE
{
    Uint16 which;
    int32  a;
    int32  b;
    int32  c;
};

extern struct RUNTRACE runTrace[RUNTRACE_N];
extern Uint16 runTraceN;

extern Uint16 mockErrorAttrLevel;

/* Defined in mock_runsrc.c. Declared here because no project header carries it:
 * f_runSrc.c declares it locally at line 1327 (`extern Uint16 bFrqCurAimChg;`)
 * and its real definition is in f_runSrc_accDecFrq.c. The driver needs to name
 * it to snapshot it per side. */
extern Uint16 bFrqCurAimChg;

#endif
