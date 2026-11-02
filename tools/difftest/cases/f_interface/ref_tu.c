/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define ABZPos                   ref_ABZPos
#define UpdateDataCore2Func      ref_UpdateDataCore2Func
#define UpdateDataFunc2Core0p5ms ref_UpdateDataFunc2Core0p5ms
#define UpdateDataFunc2Core2ms   ref_UpdateDataFunc2Core2ms
#define ZCount                   ref_ZCount
#define antiVibrateCoeff         ref_antiVibrateCoeff
#define checkSpeedFdbDir         ref_checkSpeedFdbDir
#define copyDataCore2Func05ms    ref_copyDataCore2Func05ms
#define copyDataCore2Func2ms     ref_copyDataCore2Func2ms
#define copyDataCore2Uf2ms       ref_copyDataCore2Uf2ms
#define copyDataFunc2CF2ms       ref_copyDataFunc2CF2ms
#define copyDataFunc2Motor05ms   ref_copyDataFunc2Motor05ms
#define copyDataFunc2Motor2ms    ref_copyDataFunc2Motor2ms
#define currentOc                ref_currentOc
#define currentPu                ref_currentPu
#define driveCoeffFrqFdb         ref_driveCoeffFrqFdb
#define enCoderPosition          ref_enCoderPosition
#define errorCodeFromMotor       ref_errorCodeFromMotor
#define errorsCodeFromMotor      ref_errorsCodeFromMotor
#define frq2Core                 ref_frq2Core
#define frqCurAim2Core           ref_frqCurAim2Core
#define frqFdb                   ref_frqFdb
#define frqFdbDisp               ref_frqFdbDisp
#define frqFdbFlag               ref_frqFdbFlag
#define frqFdbTmp                ref_frqFdbTmp
#define frqRun                   ref_frqRun
#define frqVFRun                 ref_frqVFRun
#define frqVFRunRemainder        ref_frqVFRunRemainder
#define gPhiRt                   ref_gPhiRt
#define gPhiRtDisp               ref_gPhiRtDisp
#define generatrixVoltage        ref_generatrixVoltage
#define jerkFf                   ref_jerkFf
#define motorErrInfor            ref_motorErrInfor
#define motorRun                 ref_motorRun
#define outCurrent               ref_outCurrent
#define outCurrentDispOld        ref_outCurrentDispOld
#define outPower                 ref_outPower
#define outVoltage               ref_outVoltage
#define pmsmRotorPos             ref_pmsmRotorPos
#define powerAddDec              ref_powerAddDec
#define rsvdData                 ref_rsvdData
#define softOCTicker             ref_softOCTicker
#define spdLoopOut               ref_spdLoopOut
#define speedMotor               ref_speedMotor
#define torqueCurrent            ref_torqueCurrent
#define vcSpdLoopKp1             ref_vcSpdLoopKp1
#define vcSpdLoopKp2             ref_vcSpdLoopKp2

#include "f_interface_ref.c"
