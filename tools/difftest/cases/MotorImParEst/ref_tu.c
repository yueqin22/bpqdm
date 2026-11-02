/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define ChgParForEst          ref_ChgParForEst
#define EnableTimer           ref_EnableTimer
#define ImCalKpKi             ref_ImCalKpKi
#define InitLmIoVaribale      ref_InitLmIoVaribale
#define InitRrLoVariable      ref_InitRrLoVariable
#define LmIoCalValue          ref_LmIoCalValue
#define LmIoIdentify          ref_LmIoIdentify
#define LmIoInPeriodInt       ref_LmIoInPeriodInt
#define RrGetVoltReBegin      ref_RrGetVoltReBegin
#define RrLoCalVaule          ref_RrLoCalVaule
#define RrLoIdentify          ref_RrLoIdentify
#define RrLoInPeriodAndCmpInt ref_RrLoInPeriodAndCmpInt
#define RrWaitAddGetUdc       ref_RrWaitAddGetUdc
#define RsIdentify            ref_RsIdentify
#define SetPwmForRrLoId       ref_SetPwmForRrLoId

#include "MotorImParEst_ref.c"
