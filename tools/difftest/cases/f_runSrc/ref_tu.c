/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define AccDecTimeCalc          ref_AccDecTimeCalc
#define DormantDeal             ref_DormantDeal
#define JogRunCtrl              ref_JogRunCtrl
#define LoseLoadRunCtrl         ref_LoseLoadRunCtrl
#define LowerThanLowerFrqDeal   ref_LowerThanLowerFrqDeal
#define NormalRunCtrl           ref_NormalRunCtrl
#define PowerOffNoStopDeal      ref_PowerOffNoStopDeal
#define RunSrcDeal              ref_RunSrcDeal
#define RunSrcUpdateFrq         ref_RunSrcUpdateFrq
#define ShutDownRunCtrl         ref_ShutDownRunCtrl
#define StartRunCtrl            ref_StartRunCtrl
#define StopRunCtrl             ref_StopRunCtrl
#define SwingDeal               ref_SwingDeal
#define TuneRunCtrl             ref_TuneRunCtrl
#define UpdateBenchTime         ref_UpdateBenchTime
#define UpdateRunCmd            ref_UpdateRunCmd
#define UpdateSwingPara         ref_UpdateSwingPara
#define accDecTimeSrcPlc        ref_accDecTimeSrcPlc
#define accFrqTime              ref_accFrqTime
#define bRunJog                 ref_bRunJog
#define decFrqTime              ref_decFrqTime
#define dormantStatus           ref_dormantStatus
#define dspMainCmd              ref_dspMainCmd
#define dspMainCmd1             ref_dspMainCmd1
#define dspStatus               ref_dspStatus
#define dspSubCmd               ref_dspSubCmd
#define dspSubStatus            ref_dspSubStatus
#define invtStatus              ref_invtStatus
#define lowerDelayTicker        ref_lowerDelayTicker
#define motorCtrlTuneStatus     ref_motorCtrlTuneStatus
#define otherStopLowerSrc       ref_otherStopLowerSrc
#define otherStopSrc            ref_otherStopSrc
#define powerUpTimeAddupTicker  ref_powerUpTimeAddupTicker
#define runCmd                  ref_runCmd
#define runDirPanelOld          ref_runDirPanelOld
#define runFlag                 ref_runFlag
#define runSrc                  ref_runSrc
#define runStatus               ref_runStatus
#define runStatus1Step          ref_runStatus1Step
#define runStatusOld4Jog        ref_runStatusOld4Jog
#define runStatusOld4LoseLoad   ref_runStatusOld4LoseLoad
#define runStatusOld4POffNoStop ref_runStatusOld4POffNoStop
#define runTimeAddupTicker      ref_runTimeAddupTicker
#define runTimeCal              ref_runTimeCal
#define runTimeTicker           ref_runTimeTicker
#define saveTuneDataFlag        ref_saveTuneDataFlag
#define setRunLostTime          ref_setRunLostTime
#define setRunTimeAim           ref_setRunTimeAim
#define setTimeRun              ref_setTimeRun
#define shuntFlag               ref_shuntFlag
#define shuntTicker             ref_shuntTicker
#define startProtectSrc         ref_startProtectSrc
#define startRunStatus          ref_startRunStatus
#define stopRemFlag             ref_stopRemFlag
#define stopRunStatus           ref_stopRunStatus
#define swingAccTime            ref_swingAccTime
#define swingDecTime            ref_swingDecTime
#define swingDoubleAw           ref_swingDoubleAw
#define swingFrqLimit           ref_swingFrqLimit
#define swingJumpFrq            ref_swingJumpFrq
#define swingMaxFrq             ref_swingMaxFrq
#define swingMinFrq             ref_swingMinFrq
#define swingStatus             ref_swingStatus
#define tuneCmd                 ref_tuneCmd

#include "f_runSrc_ref.c"
