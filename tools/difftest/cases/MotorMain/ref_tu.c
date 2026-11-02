/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define ADCOverInterrupt ref_ADCOverInterrupt
#define CalRatioFromVot  ref_CalRatioFromVot
#define InitForMotorApp  ref_InitForMotorApp
#define Main05msMotor    ref_Main05msMotor
#define Main0msMotor     ref_Main0msMotor
#define Main2msMotorA    ref_Main2msMotorA
#define Main2msMotorB    ref_Main2msMotorB
#define Main2msMotorC    ref_Main2msMotorC
#define Main2msMotorD    ref_Main2msMotorD
#define PWMZeroInterrupt ref_PWMZeroInterrupt
#define PrepareParForRun ref_PrepareParForRun
#define RunCaseRun2Ms    ref_RunCaseRun2Ms
#define RunCaseStop      ref_RunCaseStop
#define SendDataPrepare  ref_SendDataPrepare
#define TurnToStopStatus ref_TurnToStopStatus

#include "MotorMain_ref.c"
