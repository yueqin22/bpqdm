/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define AsynPWMAngleCal    new_AsynPWMAngleCal
#define CalCarrierWaveFreq new_CalCarrierWaveFreq
#define CalDeadBandComp    new_CalDeadBandComp
#define CalOutputPhase     new_CalOutputPhase
#define SoftPWMProcess     new_SoftPWMProcess
#define SynPWMAngleCal     new_SynPWMAngleCal
#define gDeadBand          new_gDeadBand
#define gFcCal             new_gFcCal
#define gPhase             new_gPhase
#define gSynFreqTable      new_gSynFreqTable
#define gSynNcTable        new_gSynNcTable
#define gSynPWM            new_gSynPWM
#define gSynStepAngleTable new_gSynStepAngleTable

#include "MotorCarrier.c"
