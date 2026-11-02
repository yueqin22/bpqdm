/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define EQepRegs            ref_EQepRegs
#define GetMDetaPos         ref_GetMDetaPos
#define GetMTTimeNum        ref_GetMTTimeNum
#define GetRotorTransPos    ref_GetRotorTransPos
#define GetSpeedMMethod     ref_GetSpeedMMethod
#define GetSpeedMTMethod    ref_GetSpeedMTMethod
#define GetUvwPhase         ref_GetUvwPhase
#define InitRtInterface     ref_InitRtInterface
#define InitSetQEP          ref_InitSetQEP
#define JudgeABDir          ref_JudgeABDir
#define JudgeRTDir          ref_JudgeRTDir
#define JudgeUVWDir         ref_JudgeUVWDir
#define ReInitForPG         ref_ReInitForPG
#define RotorTransCalVel    ref_RotorTransCalVel
#define RotorTransSamplePos ref_RotorTransSamplePos
#define SpeedSmoothDeal     ref_SpeedSmoothDeal
#define VCGetFeedBackSpeed  ref_VCGetFeedBackSpeed
#define gFVCSpeed           ref_gFVCSpeed
#define gPGData             ref_gPGData
#define gPGDir              ref_gPGDir
#define gRotorTrans         ref_gRotorTrans
#define gSpeedFilter        ref_gSpeedFilter
#define gSpeedLine          ref_gSpeedLine
#define gUVWAngleTable      ref_gUVWAngleTable
#define gUVWPG              ref_gUVWPG

#include "MotorEncoder_ref.c"
