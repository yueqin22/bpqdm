/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define AccDecTimeCalc          new_AccDecTimeCalc
#define DormantDeal             new_DormantDeal
#define JogRunCtrl              new_JogRunCtrl
#define LoseLoadRunCtrl         new_LoseLoadRunCtrl
#define LowerThanLowerFrqDeal   new_LowerThanLowerFrqDeal
#define NormalRunCtrl           new_NormalRunCtrl
#define PowerOffNoStopDeal      new_PowerOffNoStopDeal
#define RunSrcDeal              new_RunSrcDeal
#define RunSrcUpdateFrq         new_RunSrcUpdateFrq
#define ShutDownRunCtrl         new_ShutDownRunCtrl
#define StartRunCtrl            new_StartRunCtrl
#define StopRunCtrl             new_StopRunCtrl
#define SwingDeal               new_SwingDeal
#define TuneRunCtrl             new_TuneRunCtrl
#define UpdateBenchTime         new_UpdateBenchTime
#define UpdateRunCmd            new_UpdateRunCmd
#define UpdateSwingPara         new_UpdateSwingPara
#define accDecTimeSrcPlc        new_accDecTimeSrcPlc
#define accFrqTime              new_accFrqTime
#define bRunJog                 new_bRunJog
#define decFrqTime              new_decFrqTime
#define dormantStatus           new_dormantStatus
#define dspMainCmd              new_dspMainCmd
#define dspMainCmd1             new_dspMainCmd1
#define dspStatus               new_dspStatus
#define dspSubCmd               new_dspSubCmd
#define dspSubStatus            new_dspSubStatus
#define invtStatus              new_invtStatus
#define lowerDelayTicker        new_lowerDelayTicker
#define motorCtrlTuneStatus     new_motorCtrlTuneStatus
#define otherStopLowerSrc       new_otherStopLowerSrc
#define otherStopSrc            new_otherStopSrc
#define powerUpTimeAddupTicker  new_powerUpTimeAddupTicker
#define runCmd                  new_runCmd
#define runDirPanelOld          new_runDirPanelOld
#define runFlag                 new_runFlag
#define runSrc                  new_runSrc
#define runStatus               new_runStatus
#define runStatus1Step          new_runStatus1Step
#define runStatusOld4Jog        new_runStatusOld4Jog
#define runStatusOld4LoseLoad   new_runStatusOld4LoseLoad
#define runStatusOld4POffNoStop new_runStatusOld4POffNoStop
#define runTimeAddupTicker      new_runTimeAddupTicker
#define runTimeCal              new_runTimeCal
#define runTimeTicker           new_runTimeTicker
#define saveTuneDataFlag        new_saveTuneDataFlag
#define setRunLostTime          new_setRunLostTime
#define setRunTimeAim           new_setRunTimeAim
#define setTimeRun              new_setTimeRun
#define shuntFlag               new_shuntFlag
#define shuntTicker             new_shuntTicker
#define startProtectSrc         new_startProtectSrc
#define startRunStatus          new_startRunStatus
#define stopRemFlag             new_stopRemFlag
#define stopRunStatus           new_stopRunStatus
#define swingAccTime            new_swingAccTime
#define swingDecTime            new_swingDecTime
#define swingDoubleAw           new_swingDoubleAw
#define swingFrqLimit           new_swingFrqLimit
#define swingJumpFrq            new_swingJumpFrq
#define swingMaxFrq             new_swingMaxFrq
#define swingMinFrq             new_swingMinFrq
#define swingStatus             new_swingStatus
#define tuneCmd                 new_tuneCmd

#include "f_runSrc.c"
