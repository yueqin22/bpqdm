/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define ADCOverInterrupt new_ADCOverInterrupt
#define CalRatioFromVot  new_CalRatioFromVot
#define InitForMotorApp  new_InitForMotorApp
#define Main05msMotor    new_Main05msMotor
#define Main0msMotor     new_Main0msMotor
#define Main2msMotorA    new_Main2msMotorA
#define Main2msMotorB    new_Main2msMotorB
#define Main2msMotorC    new_Main2msMotorC
#define Main2msMotorD    new_Main2msMotorD
#define PWMZeroInterrupt new_PWMZeroInterrupt
#define PrepareParForRun new_PrepareParForRun
#define RunCaseRun2Ms    new_RunCaseRun2Ms
#define RunCaseStop      new_RunCaseStop
#define SendDataPrepare  new_SendDataPrepare
#define TurnToStopStatus new_TurnToStopStatus

#include "MotorMain.c"
