/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define CalIMSet            ref_CalIMSet
#define CalTorqueLimitPar   ref_CalTorqueLimitPar
#define CalUdcLimitIT       ref_CalUdcLimitIT
#define CalWsAndSynFreq     ref_CalWsAndSynFreq
#define CalWsAndSynFreq_380 ref_CalWsAndSynFreq_380
#define CalcABVolt          ref_CalcABVolt
#define PrepImCsrPara       ref_PrepImCsrPara
#define PrepareAsrPar       ref_PrepareAsrPar
#define ResetAsrPar         ref_ResetAsrPar
#define ResetCsrPar         ref_ResetCsrPar
#define ResetParForVC       ref_ResetParForVC
#define SVC0Signal          ref_SVC0Signal
#define SvcCalOutVolt       ref_SvcCalOutVolt
#define SvcCalOutVolt_380   ref_SvcCalOutVolt_380
#define VCCsrControl        ref_VCCsrControl
#define VCCsrControl_380    ref_VCCsrControl_380
#define VcAsrControl        ref_VcAsrControl
#define gABVoltSet          ref_gABVoltSet
#define gAsr                ref_gAsr
#define gIMSetAcr           ref_gIMSetAcr
#define gIMTSet             ref_gIMTSet
#define gIMTSetApply        ref_gIMTSetApply
#define gImAcrQ24           ref_gImAcrQ24
#define gItAcrQ24           ref_gItAcrQ24
#define gPWMVAlphBeta       ref_gPWMVAlphBeta
#define gSvcVoltAdj         ref_gSvcVoltAdj
#define gUAmpTheta          ref_gUAmpTheta
#define gUMTSet             ref_gUMTSet
#define gUdcLimitIt         ref_gUdcLimitIt
#define gVCPar              ref_gVCPar
#define gVcCsrPara          ref_gVcCsrPara
#define gWspid              ref_gWspid

#include "MotorVCMain_ref.c"
