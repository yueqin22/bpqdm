/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define ChgParForEst          new_ChgParForEst
#define EnableTimer           new_EnableTimer
#define ImCalKpKi             new_ImCalKpKi
#define InitLmIoVaribale      new_InitLmIoVaribale
#define InitRrLoVariable      new_InitRrLoVariable
#define LmIoCalValue          new_LmIoCalValue
#define LmIoIdentify          new_LmIoIdentify
#define LmIoInPeriodInt       new_LmIoInPeriodInt
#define RrGetVoltReBegin      new_RrGetVoltReBegin
#define RrLoCalVaule          new_RrLoCalVaule
#define RrLoIdentify          new_RrLoIdentify
#define RrLoInPeriodAndCmpInt new_RrLoInPeriodAndCmpInt
#define RrWaitAddGetUdc       new_RrWaitAddGetUdc
#define RsIdentify            new_RsIdentify
#define SetPwmForRrLoId       new_SetPwmForRrLoId

#include "MotorImParEst.c"
