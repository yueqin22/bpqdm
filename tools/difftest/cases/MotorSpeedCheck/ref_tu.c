/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define PrepareForSpeedCheck  ref_PrepareForSpeedCheck
#define RunCaseSpeedCheck     ref_RunCaseSpeedCheck
#define SpeedCheckCycleOver   ref_SpeedCheckCycleOver
#define SpeedCheckCycleSUB2   ref_SpeedCheckCycleSUB2
#define SpeedCheckSUB1        ref_SpeedCheckSUB1
#define SpeedCheckUdcRiseDeal ref_SpeedCheckUdcRiseDeal
#define gFeisu                ref_gFeisu
#define gFeisuNew             ref_gFeisuNew
#define gSpeedCheckPID        ref_gSpeedCheckPID
#define gSpeedCheckPID1       ref_gSpeedCheckPID1

#include "MotorSpeedCheck_ref.c"
