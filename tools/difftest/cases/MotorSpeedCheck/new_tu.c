/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define PrepareForSpeedCheck  new_PrepareForSpeedCheck
#define RunCaseSpeedCheck     new_RunCaseSpeedCheck
#define SpeedCheckCycleOver   new_SpeedCheckCycleOver
#define SpeedCheckCycleSUB2   new_SpeedCheckCycleSUB2
#define SpeedCheckSUB1        new_SpeedCheckSUB1
#define SpeedCheckUdcRiseDeal new_SpeedCheckUdcRiseDeal
#define gFeisu                new_gFeisu
#define gFeisuNew             new_gFeisuNew
#define gSpeedCheckPID        new_gSpeedCheckPID
#define gSpeedCheckPID1       new_gSpeedCheckPID1

#include "MotorSpeedCheck.c"
