/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define AsynParEstRotor   new_AsynParEstRotor
#define AsynParEstStatic  new_AsynParEstStatic
#define EndOfParIdentify  new_EndOfParIdentify
#define PrepareParForTune new_PrepareParForTune
#define RunCaseGetPar     new_RunCaseGetPar
#define debugTuneProcess  new_debugTuneProcess
#define gGetParVarable    new_gGetParVarable
#define gLmIoIdentify     new_gLmIoIdentify
#define gMotorExtReg      new_gMotorExtReg
#define gRrLoIdentify     new_gRrLoIdentify
#define gUVCoff           new_gUVCoff
#define noTuneProgress    new_noTuneProgress
#define pmTuneProgLoad    new_pmTuneProgLoad
#define pmTuneProgNoLoad  new_pmTuneProgNoLoad

#include "MotorParEst.c"
