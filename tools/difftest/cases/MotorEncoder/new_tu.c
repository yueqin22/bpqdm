/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define EQepRegs            new_EQepRegs
#define GetMDetaPos         new_GetMDetaPos
#define GetMTTimeNum        new_GetMTTimeNum
#define GetRotorTransPos    new_GetRotorTransPos
#define GetSpeedMMethod     new_GetSpeedMMethod
#define GetSpeedMTMethod    new_GetSpeedMTMethod
#define GetUvwPhase         new_GetUvwPhase
#define InitRtInterface     new_InitRtInterface
#define InitSetQEP          new_InitSetQEP
#define JudgeABDir          new_JudgeABDir
#define JudgeRTDir          new_JudgeRTDir
#define JudgeUVWDir         new_JudgeUVWDir
#define ReInitForPG         new_ReInitForPG
#define RotorTransCalVel    new_RotorTransCalVel
#define RotorTransSamplePos new_RotorTransSamplePos
#define SpeedSmoothDeal     new_SpeedSmoothDeal
#define VCGetFeedBackSpeed  new_VCGetFeedBackSpeed
#define gFVCSpeed           new_gFVCSpeed
#define gPGData             new_gPGData
#define gPGDir              new_gPGDir
#define gRotorTrans         new_gRotorTrans
#define gSpeedFilter        new_gSpeedFilter
#define gSpeedLine          new_gSpeedLine
#define gUVWAngleTable      new_gUVWAngleTable
#define gUVWPG              new_gUVWPG

#include "MotorEncoder.c"
