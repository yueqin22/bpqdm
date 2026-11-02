/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define CalIMSet            new_CalIMSet
#define CalTorqueLimitPar   new_CalTorqueLimitPar
#define CalUdcLimitIT       new_CalUdcLimitIT
#define CalWsAndSynFreq     new_CalWsAndSynFreq
#define CalWsAndSynFreq_380 new_CalWsAndSynFreq_380
#define CalcABVolt          new_CalcABVolt
#define PrepImCsrPara       new_PrepImCsrPara
#define PrepareAsrPar       new_PrepareAsrPar
#define ResetAsrPar         new_ResetAsrPar
#define ResetCsrPar         new_ResetCsrPar
#define ResetParForVC       new_ResetParForVC
#define SVC0Signal          new_SVC0Signal
#define SvcCalOutVolt       new_SvcCalOutVolt
#define SvcCalOutVolt_380   new_SvcCalOutVolt_380
#define VCCsrControl        new_VCCsrControl
#define VCCsrControl_380    new_VCCsrControl_380
#define VcAsrControl        new_VcAsrControl
#define gABVoltSet          new_gABVoltSet
#define gAsr                new_gAsr
#define gIMSetAcr           new_gIMSetAcr
#define gIMTSet             new_gIMTSet
#define gIMTSetApply        new_gIMTSetApply
#define gImAcrQ24           new_gImAcrQ24
#define gItAcrQ24           new_gItAcrQ24
#define gPWMVAlphBeta       new_gPWMVAlphBeta
#define gSvcVoltAdj         new_gSvcVoltAdj
#define gUAmpTheta          new_gUAmpTheta
#define gUMTSet             new_gUMTSet
#define gUdcLimitIt         new_gUdcLimitIt
#define gVCPar              new_gVCPar
#define gVcCsrPara          new_gVcCsrPara
#define gWspid              new_gWspid

#include "MotorVCMain.c"
