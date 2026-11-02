/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define CalOutVotInVFStatus   ref_CalOutVotInVFStatus
#define CalTorqueUp           ref_CalTorqueUp
#define HVfCurReDecomp        ref_HVfCurReDecomp
#define HVfDeadBandComp       ref_HVfDeadBandComp
#define HVfOscDampDeal        ref_HVfOscDampDeal
#define VFAutoTorqueBoost     ref_VFAutoTorqueBoost
#define VFOVUdcLimit          ref_VFOVUdcLimit
#define VFOverMagneticControl ref_VFOverMagneticControl
#define VFSpeedControl        ref_VFSpeedControl
#define VFWSCompControl       ref_VFWSCompControl
#define VFWsTorqueBoostComm   ref_VFWsTorqueBoostComm
#define VfFreqDeal            ref_VfFreqDeal
#define VfOscIndexCalc        ref_VfOscIndexCalc
#define VfOverCurDeal         ref_VfOverCurDeal
#define VfOverUdcDeal         ref_VfOverUdcDeal
#define VfVarInitiate         ref_VfVarInitiate
#define gHVfCur               ref_gHVfCur
#define gHVfDeadBandCompOpt   ref_gHVfDeadBandCompOpt
#define gHVfOscDamp           ref_gHVfOscDamp
#define gHVfOscIndex          ref_gHVfOscIndex
#define gOscAmp               ref_gOscAmp
#define gOvCur                ref_gOvCur
#define gOvCur2               ref_gOvCur2
#define gOvUdc                ref_gOvUdc
#define gVFAutoVar            ref_gVFAutoVar
#define gVFPar                ref_gVFPar
#define gVarAvr               ref_gVarAvr
#define gVfFreq2              ref_gVfFreq2
#define gVfOsc                ref_gVfOsc
#define gVfOverUdc            ref_gVfOverUdc
#define gWsComp               ref_gWsComp

#include "MotorVF_ref.c"
