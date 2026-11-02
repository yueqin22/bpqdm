/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define CalOutVotInVFStatus   new_CalOutVotInVFStatus
#define CalTorqueUp           new_CalTorqueUp
#define HVfCurReDecomp        new_HVfCurReDecomp
#define HVfDeadBandComp       new_HVfDeadBandComp
#define HVfOscDampDeal        new_HVfOscDampDeal
#define VFAutoTorqueBoost     new_VFAutoTorqueBoost
#define VFOVUdcLimit          new_VFOVUdcLimit
#define VFOverMagneticControl new_VFOverMagneticControl
#define VFSpeedControl        new_VFSpeedControl
#define VFWSCompControl       new_VFWSCompControl
#define VFWsTorqueBoostComm   new_VFWsTorqueBoostComm
#define VfFreqDeal            new_VfFreqDeal
#define VfOscIndexCalc        new_VfOscIndexCalc
#define VfOverCurDeal         new_VfOverCurDeal
#define VfOverUdcDeal         new_VfOverUdcDeal
#define VfVarInitiate         new_VfVarInitiate
#define gHVfCur               new_gHVfCur
#define gHVfDeadBandCompOpt   new_gHVfDeadBandCompOpt
#define gHVfOscDamp           new_gHVfOscDamp
#define gHVfOscIndex          new_gHVfOscIndex
#define gOscAmp               new_gOscAmp
#define gOvCur                new_gOvCur
#define gOvCur2               new_gOvCur2
#define gOvUdc                new_gOvUdc
#define gVFAutoVar            new_gVFAutoVar
#define gVFPar                new_gVFPar
#define gVarAvr               new_gVarAvr
#define gVfFreq2              new_gVfFreq2
#define gVfOsc                new_gVfOsc
#define gVfOverUdc            new_gVfOverUdc
#define gWsComp               new_gWsComp

#include "MotorVF.c"
