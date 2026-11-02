/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define FrqAimUpDownDeal  new_FrqAimUpDownDeal
#define FrqPlcSetDeal     new_FrqPlcSetDeal
#define FrqSrcDeal        new_FrqSrcDeal
#define FrqSrcOperate     new_FrqSrcOperate
#define FrqXyCalc         new_FrqXyCalc
#define GetFrq            new_GetFrq
#define GetFrqComp        new_GetFrqComp
#define GetFrqX           new_GetFrqX
#define GetFrqY           new_GetFrqY
#define JumpFrqDeal       new_JumpFrqDeal
#define JumpFrqEndCalc    new_JumpFrqEndCalc
#define ResetUpDownFrq    new_ResetUpDownFrq
#define UpdateFrqAim      new_UpdateFrqAim
#define UpdateLimitFrq    new_UpdateLimitFrq
#define UpdateMultiSetFrq new_UpdateMultiSetFrq
#define UpdateRunMode     new_UpdateRunMode
#define bAntiReverseRun   new_bAntiReverseRun
#define bFrqDigital       new_bFrqDigital
#define bPlcEndOneLoop    new_bPlcEndOneLoop
#define bStopPlc          new_bStopPlc
#define benchFrq          new_benchFrq
#define frq               new_frq
#define frqAiPu           new_frqAiPu
#define frqAim            new_frqAim
#define frqAimOld4Dir     new_frqAimOld4Dir
#define frqAimTmp         new_frqAimTmp
#define frqAimTmp0        new_frqAimTmp0
#define frqCalcSrc        new_frqCalcSrc
#define frqCalcSrcOld     new_frqCalcSrcOld
#define frqCurAim         new_frqCurAim
#define frqCurAimFrac     new_frqCurAimFrac
#define frqDigitalPlc     new_frqDigitalPlc
#define frqDigitalTmp     new_frqDigitalTmp
#define frqDroop          new_frqDroop
#define frqFlag           new_frqFlag
#define frqFrac           new_frqFrac
#define frqKeyUpDownDelta new_frqKeyUpDownDelta
#define frqMax            new_frqMax
#define frqMin            new_frqMin
#define frqPlcType        new_frqPlcType
#define frqPuQ15          new_frqPuQ15
#define frqTmp            new_frqTmp
#define frqTmpFrac        new_frqTmpFrac
#define frqXy             new_frqXy
#define getUpDownLimit    new_getUpDownLimit
#define jumpFrq1          new_jumpFrq1
#define jumpFrq2          new_jumpFrq2
#define lowerFrq          new_lowerFrq
#define maxFrq            new_maxFrq
#define plcStep           new_plcStep
#define plcStepOld        new_plcStepOld
#define plcStepRemOld     new_plcStepRemOld
#define plcTime           new_plcTime
#define plcTimeRemOld     new_plcTimeRemOld
#define presetFrqOld      new_presetFrqOld
#define reverseTicker     new_reverseTicker
#define runCmdBind        new_runCmdBind
#define runMode           new_runMode
#define timeBench         new_timeBench
#define uPDownFrqLower1   new_uPDownFrqLower1
#define uPDownFrqLower2   new_uPDownFrqLower2
#define uPDownFrqMax      new_uPDownFrqMax
#define uPDownFrqMin      new_uPDownFrqMin
#define upDownFrq         new_upDownFrq
#define upDownFrqInit     new_upDownFrqInit
#define upDownFrqTmp      new_upDownFrqTmp
#define upperFrq          new_upperFrq

#include "f_frqSrc.c"
