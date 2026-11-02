/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define AsynParEstRotor   ref_AsynParEstRotor
#define AsynParEstStatic  ref_AsynParEstStatic
#define EndOfParIdentify  ref_EndOfParIdentify
#define PrepareParForTune ref_PrepareParForTune
#define RunCaseGetPar     ref_RunCaseGetPar
#define debugTuneProcess  ref_debugTuneProcess
#define gGetParVarable    ref_gGetParVarable
#define gLmIoIdentify     ref_gLmIoIdentify
#define gMotorExtReg      ref_gMotorExtReg
#define gRrLoIdentify     ref_gRrLoIdentify
#define gUVCoff           ref_gUVCoff
#define noTuneProgress    ref_noTuneProgress
#define pmTuneProgLoad    ref_pmTuneProgLoad
#define pmTuneProgNoLoad  ref_pmTuneProgNoLoad

#include "MotorParEst_ref.c"
