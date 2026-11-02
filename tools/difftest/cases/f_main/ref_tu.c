/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define CODE_RUN_DISPLAY_UPDATE_TIME ref_CODE_RUN_DISPLAY_UPDATE_TIME
#define CodeRunMonitor               ref_CodeRunMonitor
#define FuncVariableInit             ref_FuncVariableInit
#define GetNumberDigit               ref_GetNumberDigit
#define GetNumberDigit1              ref_GetNumberDigit1
#define GetNumberDigit2              ref_GetNumberDigit2
#define InitDspFunc                  ref_InitDspFunc
#define InitForFunctionApp           ref_InitForFunctionApp
#define InitFuncCode                 ref_InitFuncCode
#define Main05msFunctionA            ref_Main05msFunctionA
#define Main05msFunctionB            ref_Main05msFunctionB
#define Main05msFunctionC            ref_Main05msFunctionC
#define Main05msFunctionD            ref_Main05msFunctionD
#define Main05msFunctionEachEnd      ref_Main05msFunctionEachEnd
#define Main05msFunctionEachStart    ref_Main05msFunctionEachStart
#define Main0msFunction              ref_Main0msFunction
#define Main2msFunction              ref_Main2msFunction
#define SoftVersionDeal              ref_SoftVersionDeal
#define aiaoChckReadOK               ref_aiaoChckReadOK
#define driveCoeff                   ref_driveCoeff
#define funcCodeInitStatus           ref_funcCodeInitStatus
#define getMomeryValue               ref_getMomeryValue
#define mainLoopTicker               ref_mainLoopTicker
#define memoryValue                  ref_memoryValue
#define motorFc                      ref_motorFc
#define motorPara                    ref_motorPara
#define motorSn                      ref_motorSn
#define powerOnStatus                ref_powerOnStatus
#define ticker05msFuncEa             ref_ticker05msFuncEa
#define ticker05msFuncEach           ref_ticker05msFuncEach
#define ticker05msFuncEachDisp       ref_ticker05msFuncEachDisp
#define ticker05msFuncIn             ref_ticker05msFuncIn
#define ticker05msFuncInternal       ref_ticker05msFuncInternal
#define ticker05msFuncInternalDisp   ref_ticker05msFuncInternalDisp
#define timeBase                     ref_timeBase
#define timeBase05msOld              ref_timeBase05msOld

#include "f_main_ref.c"
