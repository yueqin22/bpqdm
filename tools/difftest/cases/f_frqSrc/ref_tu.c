/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define FrqAimUpDownDeal  ref_FrqAimUpDownDeal
#define FrqPlcSetDeal     ref_FrqPlcSetDeal
#define FrqSrcDeal        ref_FrqSrcDeal
#define FrqSrcOperate     ref_FrqSrcOperate
#define FrqXyCalc         ref_FrqXyCalc
#define GetFrq            ref_GetFrq
#define GetFrqComp        ref_GetFrqComp
#define GetFrqX           ref_GetFrqX
#define GetFrqY           ref_GetFrqY
#define JumpFrqDeal       ref_JumpFrqDeal
#define JumpFrqEndCalc    ref_JumpFrqEndCalc
#define ResetUpDownFrq    ref_ResetUpDownFrq
#define UpdateFrqAim      ref_UpdateFrqAim
#define UpdateLimitFrq    ref_UpdateLimitFrq
#define UpdateMultiSetFrq ref_UpdateMultiSetFrq
#define UpdateRunMode     ref_UpdateRunMode
#define bAntiReverseRun   ref_bAntiReverseRun
#define bFrqDigital       ref_bFrqDigital
#define bPlcEndOneLoop    ref_bPlcEndOneLoop
#define bStopPlc          ref_bStopPlc
#define benchFrq          ref_benchFrq
#define frq               ref_frq
#define frqAiPu           ref_frqAiPu
#define frqAim            ref_frqAim
#define frqAimOld4Dir     ref_frqAimOld4Dir
#define frqAimTmp         ref_frqAimTmp
#define frqAimTmp0        ref_frqAimTmp0
#define frqCalcSrc        ref_frqCalcSrc
#define frqCalcSrcOld     ref_frqCalcSrcOld
#define frqCurAim         ref_frqCurAim
#define frqCurAimFrac     ref_frqCurAimFrac
#define frqDigitalPlc     ref_frqDigitalPlc
#define frqDigitalTmp     ref_frqDigitalTmp
#define frqDroop          ref_frqDroop
#define frqFlag           ref_frqFlag
#define frqFrac           ref_frqFrac
#define frqKeyUpDownDelta ref_frqKeyUpDownDelta
#define frqMax            ref_frqMax
#define frqMin            ref_frqMin
#define frqPlcType        ref_frqPlcType
#define frqPuQ15          ref_frqPuQ15
#define frqTmp            ref_frqTmp
#define frqTmpFrac        ref_frqTmpFrac
#define frqXy             ref_frqXy
#define getUpDownLimit    ref_getUpDownLimit
#define jumpFrq1          ref_jumpFrq1
#define jumpFrq2          ref_jumpFrq2
#define lowerFrq          ref_lowerFrq
#define maxFrq            ref_maxFrq
#define plcStep           ref_plcStep
#define plcStepOld        ref_plcStepOld
#define plcStepRemOld     ref_plcStepRemOld
#define plcTime           ref_plcTime
#define plcTimeRemOld     ref_plcTimeRemOld
#define presetFrqOld      ref_presetFrqOld
#define reverseTicker     ref_reverseTicker
#define runCmdBind        ref_runCmdBind
#define runMode           ref_runMode
#define timeBench         ref_timeBench
#define uPDownFrqLower1   ref_uPDownFrqLower1
#define uPDownFrqLower2   ref_uPDownFrqLower2
#define uPDownFrqMax      ref_uPDownFrqMax
#define uPDownFrqMin      ref_uPDownFrqMin
#define upDownFrq         ref_upDownFrq
#define upDownFrqInit     ref_upDownFrqInit
#define upDownFrqTmp      ref_upDownFrqTmp
#define upperFrq          ref_upperFrq

#include "f_frqSrc_ref.c"
