/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define AsynPWMAngleCal    ref_AsynPWMAngleCal
#define CalCarrierWaveFreq ref_CalCarrierWaveFreq
#define CalDeadBandComp    ref_CalDeadBandComp
#define CalOutputPhase     ref_CalOutputPhase
#define SoftPWMProcess     ref_SoftPWMProcess
#define SynPWMAngleCal     ref_SynPWMAngleCal
#define gDeadBand          ref_gDeadBand
#define gFcCal             ref_gFcCal
#define gPhase             ref_gPhase
#define gSynFreqTable      ref_gSynFreqTable
#define gSynNcTable        ref_gSynNcTable
#define gSynPWM            ref_gSynPWM
#define gSynStepAngleTable ref_gSynStepAngleTable

#include "MotorCarrier_ref.c"
