
#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_io.h"
#include "f_comm.h"
#include "f_ui.h"
#include "f_main.h"
#include "f_menu.h"
#include "f_eeprom.h"
#include "f_posCtrl.h"

extern int32 pcRef;
extern int32 pcFdb;

#define RS_PERCENT_SCALE        1000
#define RS_PERCENT              100
#define RS_AI_Q15_ROUND         (1 << 14)
#define RS_AI_Q15_SHIFT         15
#define RS_LOSE_LOAD_FRQ_NUM    7
#define RS_TUNE_IM_FRQ_NUM      4
#define RS_TUNE_IM_FRQ_DEN      5
#define RS_DC_BUS_SCALE         14482
#define RS_DC_BUS_SHIFT         10
#define RS_POFF_THRESHOLD_SCALE 131
#define RS_POFF_THRESHOLD_SHIFT 17
#define RS_POFF_SLOPE_SCALE     1000
#define RS_STOP_BRAKE_MIN_FRQ   50
#define RS_LOWER_FRQ_STOP_TICK  2
#define RS_RUN_TIME_MIN_SCALE   10
#define RS_TENTH_MIN_SEC        6
#define RS_RUN_TIME_TICK_MAX    ((Uint16)(TIME_UNIT_MS_PER_SEC / RUN_TIME_CAL_PERIOD))
#define RS_SWING_DELTA_SHIFT    5
#define RS_TIME_BENCH_0POINT    1000
#define RS_TIME_BENCH_1POINT    100
#define RS_TIME_BENCH_2POINT    10
#define RS_DORMANT_HIGH_FC      31
#define RS_DORMANT_LOW_FC       32
#define RS_DORMANT_STOP_FC      33
#define RS_DORMANT_START_FC     34
#define RS_ACC_DEC_SEL_1        0
#define RS_ACC_DEC_SEL_2        1
#define RS_ACC_DEC_SEL_3        2
#define RS_ACC_DEC_SEL_4        3

#if F_DEBUG_RAM

#define DEBUG_F_RUN_SRC_DI          0
#define DEBUG_F_RUN_SRC_COMM        0
#define DEBUG_F_SWING               0
#define DEBUG_F_TUNE                0
#define DEBUG_F_POWER_OFF_NO_STOP   0
#define DEBUG_F_LOSE_LOAD           0
#define DEBUG_F_LOWER_FRQ           0
#define DEBUG_F_JOG                 0
#define DEBUG_F_RUN_TIME            0
#define DEBUG_F_START_RUN           0
#define DEBUG_F_STOP_RUN            0
#define DEBUG_F_ACC_DEC_TIME        0
#define DEBUG_F_RUN_POWERUP_TIME    0
#define DEBUG_F_ERROR_RUN_FRQ       0
#define DEBUG_F_DORMANT_DEAL        0

#elif 1

#define DEBUG_F_RUN_SRC_DI          1
#define DEBUG_F_RUN_SRC_COMM        1
#define DEBUG_F_SWING               1
#define DEBUG_F_TUNE                1
#define DEBUG_F_POWER_OFF_NO_STOP   1
#define DEBUG_F_LOSE_LOAD           1
#define DEBUG_F_LOWER_FRQ           1
#define DEBUG_F_JOG                 1
#define DEBUG_F_RUN_TIME            1
#define DEBUG_F_START_RUN           1
#define DEBUG_F_STOP_RUN            1
#define DEBUG_F_ACC_DEC_TIME        1
#define DEBUG_F_RUN_POWERUP_TIME    1
#define DEBUG_F_ERROR_RUN_FRQ       1
#define DEBUG_F_DORMANT_DEAL        1
#endif

union RUN_CMD runCmd;
union RUN_FLAG runFlag;
union RUN_STATUS_FIRST_STEP runStatus1Step;
union DSP_MAIN_COMMAND dspMainCmd;
union DSP_MAIN_COMMAND1 dspMainCmd1;

union DSP_SUB_COMMAND dspSubCmd;
union DSP_STATUS dspStatus;
union DSP_SUB_STATUS dspSubStatus;

union INVT_STATUS invtStatus;

Uint16 startProtectSrc;
Uint16 otherStopSrc;

Uint16 otherStopLowerSrc;

enum RUN_STATUS runStatus;
enum START_RUN_STATUS startRunStatus;
enum STOP_RUN_STATUS stopRunStatus;
LOCALF enum RUN_STATUS runStatusOld4LoseLoad;
LOCALF enum RUN_STATUS runStatusOld4Jog;
LOCALF enum RUN_STATUS runStatusOld4POffNoStop;

Uint16 runSrc;
Uint32 accFrqTime;
Uint32 decFrqTime;

enum SWING_STATUS swingStatus;
LOCALF Uint32 swingDoubleAw;
LOCALF int32 swingMaxFrq;
LOCALF int32 swingMinFrq;
LOCALF int32 swingJumpFrq;
LOCALF Uint32 swingAccTime;
LOCALF Uint32 swingDecTime;
Uint16 swingFrqLimit;

Uint16 setRunLostTime;
Uint16 setRunTimeAim;

LOCALF Uint32 runTimeTicker;
LOCALF Uint32 lowerDelayTicker;
LOCALF Uint32 shuntTicker;
LOCALF Uint16 runTimeAddupTicker;
LOCALF Uint16 powerUpTimeAddupTicker;

Uint16 shuntFlag;

Uint16 accDecTimeSrcPlc;

#define STOP_REM_NONE           0
#define STOP_REM_WAIT           1
#define STOP_REM_PAUSE_JUDGE    2
LOCALF Uint16 stopRemFlag;
Uint16 runDirPanelOld;

LOCALF Uint16 bRunJog;

#define TUNE_STEP_OVER_TIME_MAX     60UL
#define TUNE_RUN_OVER_TIME_MAX      180UL
enum TUNE_RUN_STATUS
{
    TUNE_RUN_STATUS_WAIT,
    TUNE_RUN_STATUS_ACC,
    TUNE_RUN_STATUS_DEC,
    TUNE_RUN_STATUS_END
};
Uint16 tuneCmd;
Uint16 saveTuneDataFlag;

enum DORMANT_STATUS
{
    DORMANT_RESPOND,
    DORMANT_NO_RESPOND,
    DORMANT_2
};
enum DORMANT_STATUS dormantStatus;

LOCALD void DormantDeal(void);

LOCALD void StartRunCtrl(void);
LOCALD void NormalRunCtrl(void);
LOCALD void StopRunCtrl(void);
LOCALD void JogRunCtrl(void);
LOCALD void TuneRunCtrl(void);
LOCALD void ShutDownRunCtrl(void);
LOCALD void LoseLoadRunCtrl(void);
LOCALD void LowerThanLowerFrqDeal(void);
LOCALD void PowerOffNoStopDeal(void);
LOCALD void SwingDeal(void);
LOCALD void UpdateSwingPara(void);
void SaveTuneData(void);

void AccCalc(int32 frq0);

void RunSrcUpdateFrq(void);

extern Uint16 spdLoopOut;

void setTimeRun(void);

void UpdateRunCmd(void)
{
    static Uint16 bMfkSwitchOn;
    static Uint16 diFwdOld, diRevOld, diFwdJogOld, diRevJogOld;
    static Uint16 startProtectSrcBak;
    static Uint16 otherStopSrcBak;
    static Uint16 otherStopLowerSrcBak;
    static Uint16 bJogAccepted, bRunAccepted;
    static Uint16 jogPulseCmd;
    static Uint16 runSrcOld;
    Uint16 jogLevelCmd;
    Uint16 jogCmdAll;
    Uint16 protectSrcKind;
    Uint16 diFwd, diRev, diFwdJog, diRevJog;

    jogLevelCmd = 0;
    protectSrcKind = 0;

    diFwd = diFunc.f1.bit.fwd;
    diRev = diFunc.f1.bit.rev;
    diFwdJog = diFunc.f1.bit.fwdJog;
    diRevJog = diFunc.f1.bit.revJog;

    if (POWER_ON_WAIT == powerOnStatus)
    {
        diFwdOld = diFwd;
        diRevOld = diRev;
        diFwdJogOld = diFwdJog;
        diRevJogOld = diRevJog;
    }

    if (KEY_SWITCH == keyFunc)
    {
        bMfkSwitchOn = !bMfkSwitchOn;
        keyFunc = 0;
    }
    if (FUNCCODE_mfkKeyFunc_SWITCH != funcCode.code.mfkKeyFunc)
    {
        bMfkSwitchOn = 0;
    }

    runSrc = funcCode.code.runSrc;

    if (bMfkSwitchOn)
    {
        runSrc = FUNCCODE_runSrc_PANEL;
    }

    if (diFunc.f1.bit.localOrRemote)
    {
        if (FUNCCODE_runSrc_PANEL != runSrc)
        {
            runSrc = FUNCCODE_runSrc_PANEL;
        }
#if 0
        else
        {
            runSrc = funcCode.code.runSrc;
        }
#endif
    }

    else if (diFunc.f2.bit.diOrComm)
    {
        if (FUNCCODE_runSrc_DI == funcCode.code.runSrc)
        {
            runSrc = FUNCCODE_runSrc_COMM;
        }
        else if (FUNCCODE_runSrc_COMM == funcCode.code.runSrc)
        {
            runSrc = FUNCCODE_runSrc_DI;
        }
    }

    if (runSrcOld != runSrc)
    {
        runCmd.bit.common0 = 0;
        runCmd.bit.common = 0;
    }
    runSrcOld = runSrc;

    runCmd.bit.freeStop = 0;
    runCmd.bit.errorReset = 0;
    runCmd.bit.pause = 0;

    switch (runSrc)
    {
        case FUNCCODE_runSrc_PANEL:
            jogPulseCmd = 0;

            if ((KEY_STOP == keyFunc) || (diFunc.f2.bit.stopPanel))
            {
                runCmd.bit.common0 = 0;
            }
            else if (KEY_REV == keyFunc)
            {
                runCmd.bit.dir = ~runCmd.bit.dir;
                runDirPanelOld = runCmd.bit.dir;
                keyFunc = 0;
            }
            else if (KEY_RUN == keyFunc)
            {
                runCmd.bit.common0 = 1;
                keyFunc = 0;
            }

            if (!runFlag.bit.run)
            {
                runCmd.bit.dir = runDirPanelOld;
            }

            break;

#if DEBUG_F_RUN_SRC_DI
        case FUNCCODE_runSrc_DI:
            jogPulseCmd = 0;

            switch (funcCode.code.diControlMode)
            {
                case FUNCCODE_diControlMode_2LINE1:
                case FUNCCODE_diControlMode_2LINE2:
                    if (FUNCCODE_diControlMode_2LINE1 == funcCode.code.diControlMode)
                    {
                        if (diFwd && (!diRev))
                        {
                            runCmd.bit.dir = FORWARD_DIR;
                            runCmd.bit.common0 = 1;
                        }
                        else if ((!diFwd) && (diRev))
                        {
                            runCmd.bit.dir = REVERSE_DIR;
                            runCmd.bit.common0 = 1;
                        }
                        else
                        {
                            runCmd.bit.common0 = 0;
                        }

                        protectSrcKind = 1;

                    }
                    else
                    {
                        runCmd.bit.common0 = diFwd;

                        if (diRev)
                            runCmd.bit.dir = REVERSE_DIR;
                        else
                            runCmd.bit.dir = FORWARD_DIR;

                        protectSrcKind = 2;

                    }
#if 0
                    if ((protectCmdOld != protectCmd)
                        && (((runFlag.bit.run) && (RUN_STATUS_STOP == runStatus))
                            || (!runFlag.bit.run)
                            )
                        )
                        startProtectSrc++;
#endif
                    break;

                case FUNCCODE_diControlMode_3LINE1:
                case FUNCCODE_diControlMode_3LINE2:
                    if (diFwdOld != diFwd)
                    {
                        if (diFwd)
                        {
                            runCmd.bit.common0 = 1;

                            if (FUNCCODE_diControlMode_3LINE1 == funcCode.code.diControlMode)
                            {
                                runCmd.bit.dir = FORWARD_DIR;
                            }
                        }
                    }

                    if (FUNCCODE_diControlMode_3LINE1 == funcCode.code.diControlMode)
                    {
                        if (diRevOld != diRev)
                        {
                            if (diRev)
                            {
                                runCmd.bit.common0 = 1;
                                runCmd.bit.dir = REVERSE_DIR;
                            }
                        }
                    }
                    else
                    {
                        diRev ? (runCmd.bit.dir = REVERSE_DIR) : (runCmd.bit.dir = FORWARD_DIR);
                        #if 0
                        if (rev)
                            runCmd.bit.dir = REVERSE_DIR;
                        else
                            runCmd.bit.dir = FORWARD_DIR;
                        #endif
                    }

                    if (!diFunc.f1.bit.tripleLineCtrl)
                    {
                        runCmd.bit.common0 = 0;
                    }

                    break;

                default:
                    break;
            }

            break;
#endif

#if DEBUG_F_RUN_SRC_COMM
        case FUNCCODE_runSrc_COMM:
#if DEBUG_F_PLC_CTRL

            if (funcCode.code.plcEnable)
            {
               commRunCmd = funcCode.code.plcCmd;
            }
#endif
            switch (commRunCmd)
            {
#if 1
                case SCI_RUN_CMD_FWD_RUN:
                    if (!runFlag.bit.jog)
                    {
                        runCmd.bit.common0 = 1;
                        runCmd.bit.dir = FORWARD_DIR;
                    }
                    break;

                case SCI_RUN_CMD_REV_RUN:
                    if (!runFlag.bit.jog)
                    {
                        runCmd.bit.common0 = 1;
                        runCmd.bit.dir = REVERSE_DIR;
                    }
                    break;

#elif 1
                case SCI_RUN_CMD_FWD_RUN:

                    if ((!runFlag.bit.jog) || (jogCmdPulse))
                    {
                        runCmd.bit.common0 = 1;
                        runCmd.bit.dir = FORWARD_DIR;
                        jogCmdPulse = 0;
                    }
                    break;
                case SCI_RUN_CMD_REV_RUN:
                    if ((!runFlag.bit.jog) || (jogCmdPulse))
                    {
                        runCmd.bit.common0 = 1;
                        runCmd.bit.dir = REVERSE_DIR;
                        jogCmdPulse = 0;
                    }
                    break;
#endif
                case SCI_RUN_CMD_FWD_JOG:
                    jogPulseCmd = RUN_CMD_FWD_JOG;
                    break;

                case SCI_RUN_CMD_REV_JOG:
                    jogPulseCmd = RUN_CMD_REV_JOG;
                    break;

                case SCI_RUN_CMD_FREE_STOP:
                    otherStopSrc++;
                    runCmd.bit.freeStop = 1;

                case SCI_RUN_CMD_STOP:
                    jogPulseCmd = 0;
                    runCmd.bit.common0 = 0;
                    break;

                case SCI_RUN_CMD_RESET_ERROR:
                    runCmd.bit.errorReset = 1;
                    break;

                default:
                    break;
            }

            break;
#endif

#if 0

        case FUNCCODE_runSrc_AUTO_RUN:
            runCmd.bit.common0 = 1;
            break;
#endif
        default:
            break;
    }

#if DEBUG_F_ACC_DEC_TIME

    if (((errorCode && (errorAttribute.bit.level != ERROR_LEVEL_RUN))
          || (POWER_ON_WAIT == powerOnStatus)
          )
        )
    {
        startProtectSrc++;
    }

    if (bStopPlc)
    {
        if (runCmd.bit.common0)
        {
            otherStopSrc++;
        }

        bStopPlc = 0;
    }

    if ((diFunc.f1.bit.closePwm)
        ||(diFunc.f2.bit.emergencyStop)
        ||(diFunc.f2.bit.stop4dec)
        || ((FUNCCODE_stopKeyFunc_ALL == funcCode.code.stopKeyFunc)
            && (KEY_STOP == keyFunc)
            )
        )
    {
        otherStopSrc++;
        jogPulseCmd = 0;
    }

    if (startProtectSrcBak != startProtectSrc)
    {
        bJogAccepted = 0;
        bRunAccepted = 0;

        runCmd.bit.startProtect = 1;
    }

    if ((otherStopSrcBak != otherStopSrc)
        || (otherStopLowerSrcBak != otherStopLowerSrc))
    {
        bJogAccepted = 0;
        bRunAccepted = 0;

        runCmd.bit.otherStop = 1;
    }

    if (!funcCode.code.startProtect)
    {
        runCmd.bit.startProtect = 0;
    }

    if (runCmd.bit.startProtect || runCmd.bit.otherStop)
    {
        if ((diFwdJogOld != diFwdJog)
            || (diRevJogOld != diRevJog)
            || ((!diFwdJog) && (!diRevJog))
            )
        {
            bJogAccepted = 1;
        }

        if (!bJogAccepted)
        {
            diFwdJog = 0;
            diRevJog = 0;
        }

        if (!runCmd.bit.common0)
        {
            bRunAccepted = 1;
        }

        if ((!runFlag.bit.common)
            && (FUNCCODE_pidCalcMode_YES == funcCode.code.pidCalcMode)
            && (otherStopLowerSrcBak != otherStopLowerSrc)
            )
        {
            otherStopLowerSrcBak = otherStopLowerSrc;

            bRunAccepted = 1;
        }

        if (!bRunAccepted)
        {
            runCmd.bit.common0 = 0;
        }

        if (!protectSrcKind)
        {
            if (bJogAccepted)
            {
                runCmd.bit.startProtect = 0;
                runCmd.bit.otherStop = 0;

                otherStopLowerSrcBak = otherStopLowerSrc;
            }
        }
        else
        {
            if (bRunAccepted && bJogAccepted)
            {
                runCmd.bit.startProtect = 0;
                runCmd.bit.otherStop = 0;

                otherStopLowerSrcBak = otherStopLowerSrc;
            }
        }
    }

    diFwdOld = diFunc.f1.bit.fwd;
    diRevOld = diFunc.f1.bit.rev;
    diFwdJogOld = diFunc.f1.bit.fwdJog;
    diRevJogOld = diFunc.f1.bit.revJog;

    startProtectSrcBak = startProtectSrc;
    otherStopSrcBak = otherStopSrc;
#endif

    commRunCmd = 0;

#if DEBUG_F_PLC_CTRL

   funcCode.code.plcCmd = 0;
#endif

#if DEBUG_F_JOG

    if ((KEY_FWD_JOG == keyFunc) && (runSrc == FUNCCODE_runSrc_PANEL))
    {
        jogLevelCmd = RUN_CMD_FWD_JOG;
    }
    else if ((KEY_REV_JOG == keyFunc) && (runSrc == FUNCCODE_runSrc_PANEL))
    {
        jogLevelCmd = RUN_CMD_REV_JOG;
    }
    else if ((diFwdJog) && (runSrc == FUNCCODE_runSrc_DI))
    {
        jogLevelCmd = RUN_CMD_FWD_JOG;
    }
    else if ((diRevJog) && (runSrc == FUNCCODE_runSrc_DI))
    {
        jogLevelCmd = RUN_CMD_REV_JOG;
    }

#endif

    if (runCmd.bit.common0
        && ((ERROR_LEVEL_FREE_STOP == errorAttribute.bit.level)
            || (((runStatus == RUN_STATUS_TUNE)
                 || (dspMainCmd1.bit.speedTrack))
                && errorCode)
            || (ERROR_LEVEL_STOP == errorAttribute.bit.level)))
    {
        menu0DispStatus = MENU0_DISP_STATUS_ERROR;
    }

    if ((ERROR_LEVEL_FREE_STOP == errorAttribute.bit.level)
        || (((runStatus == RUN_STATUS_TUNE)
             || (dspMainCmd1.bit.speedTrack))
            && errorCode))
    {
        runCmd.bit.freeStop = 1;
    }
    else if (ERROR_LEVEL_STOP == errorAttribute.bit.level)
    {
        runCmd.bit.common0 = 0;
    }

    DormantDeal();

    runCmd.bit.common = 0;
    if ((runCmd.bit.common0)
        && (DORMANT_RESPOND == dormantStatus)
        )
    {
        Uint16 bCommonOk = 0;

        if ((runFlag.bit.common) || (runFlag.bit.tune))
        {
            bCommonOk = 1;
        }
        else if ((!runFlag.bit.jog) && (!runFlag.bit.tune))
        {
            if (((ABS_INT32(frqAimTmp0) >= funcCode.code.startFrq)
                 || (runFlag.bit.pid)
                 )
                && ((FUNCCODE_lowerDeal_DELAY_STOP != funcCode.code.lowerDeal)
                    || (ABS_INT32(frqAimTmp) >= lowerFrq)
                    )
                )
            {
                bCommonOk = 1;
            }
            else
            {
                if (RS_LOWER_FRQ_STOP_TICK == mainLoopTicker)
                {
                    otherStopLowerSrc++;
                }

                runCmd.bit.common0 = 0;
            }
        }
        else
        {
            if (!runFlag.bit.jogWhenRun)
            {
                runCmd.bit.common0 = 0;
            }
        }

        if (bCommonOk &&
            (!diFunc.f1.bit.runPause)
            )
        {
            runCmd.bit.common = 1;
        }
    }

    if ((diFunc.f1.bit.closePwm)
        || (POWER_ON_WAIT == powerOnStatus)
        || (FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA == funcCodeRwMode)
        || (FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA_ALL == funcCodeRwMode))
    {
        runCmd.bit.freeStop = 1;
    }

    if (diFunc.f2.bit.emergencyStop || diFunc.f2.bit.stop4dec)
    {
        runCmd.bit.common0 = 0;
        runCmd.bit.common = 0;
    }

    if ((diFunc.f1.bit.runPause) && (!runCmd.bit.freeStop))
    {
        runCmd.bit.pause = 1;
    }

    if ((runFlag.bit.common)
        && (!runCmd.bit.common)
        && (!runFlag.bit.jogWhenRun)
        && (FUNCCODE_stopMode_FREESTOP == funcCode.code.stopMode)
        && (DORMANT_NO_RESPOND != dormantStatus)
        )
    {
        runCmd.bit.freeStop = 1;
    }

    if ((runCmd.bit.freeStop)
        && (!runCmd.bit.pause)
        )
    {
        runCmd.bit.common0 = 0;
        runCmd.bit.common = 0;
        jogPulseCmd = 0;
        jogLevelCmd = 0;
    }

    jogCmdAll = jogLevelCmd | jogPulseCmd;

    bRunJog = 0;
    if ((jogCmdAll)
        && (runFlag.bit.common
            || ((runCmd.bit.pause) && (runCmd.bit.common0))
            || runCmd.bit.common))
    {
        if (funcCode.code.jogWhenRun)
        {
            bRunJog = 1;
        }

        if (bRunJog
            && (!dspMainCmd.bit.startBrake)
            && (!dspMainCmd.bit.stopBrake)
            && (!dspMainCmd.bit.startFlux))
        {
            runFlag.bit.jogWhenRun = 1;
        }
        else
        {
            jogCmdAll = 0;
        }
    }

#if DEBUG_F_JOG
    if (RUN_CMD_FWD_REV_JOG == jogCmdAll)
    {
        jogCmdAll = RUN_CMD_FWD_JOG;
    }

    runCmd.bit.jog = jogCmdAll;

    if (jogPulseCmd != jogCmdAll)
    {
        jogPulseCmd = 0;
    }
#endif

    if (STOP_REM_PAUSE_JUDGE == stopRemFlag)
    {
        if (!runCmd.bit.pause)
        {
            stopRemFlag = STOP_REM_NONE;
        }
        else if (!runCmd.bit.common0)
        {
            stopRemFlag = STOP_REM_WAIT;
        }
    }

}

void setTimeRun()
{
    Uint16 aiIndex;

    if (!funcCode.code.setTimeMode)
    {

        setRunLostTime = 0;
        setRunTimeAim = 0;
        return;
    }

    setRunTimeAim = funcCode.code.setTimeValue;

    switch (funcCode.code.setTimeSource)
    {
        case FUNCCODE_plcFrq0Src_AI1:
        case FUNCCODE_plcFrq0Src_AI2:
        case FUNCCODE_plcFrq0Src_AI3:
            aiIndex = funcCode.code.setTimeSource - FUNCCODE_plcFrq0Src_AI1;
            setRunTimeAim = ((int32)ABS_INT32(aiDeal[aiIndex].set) * setRunTimeAim + (RS_AI_Q15_ROUND)) >> RS_AI_Q15_SHIFT;
            break;

        default:
            break;
    }

    if (setRunTimeAim > 0)
    {

        if (curTime.runTime >= setRunTimeAim)
        {

            runCmd.bit.common0 = 0;
               runCmd.bit.common = 0;
            otherStopSrc++;
            setRunLostTime = 0;
        }
        else
        {
            setRunLostTime = setRunTimeAim - curTime.runTime;
        }
    }
    else
    {
        setRunLostTime = 0;
    }

}

void runTimeCal(void)
{
#if DEBUG_F_RUN_POWERUP_TIME
    if(++powerUpTimeAddupTicker >= RS_RUN_TIME_TICK_MAX)
    {
        powerUpTimeAddupTicker = 0;

        if(++curTime.powerOnTimeSec>= TIME_UNIT_SEC_PER_MIN)
        {
            curTime.powerOnTimeSec = 0;
            curTime.powerOnTimeM++;
        }

        if (++funcCode.code.powerUpTimeAddupSec >= TIME_UNIT_SEC_PER_HOUR)
        {
            funcCode.code.powerUpTimeAddupSec = 0;
            funcCode.code.powerUpTimeAddup++;
        }
    }

    if (funcCode.code.powerUpTimeArriveSet
        && (funcCode.code.powerUpTimeAddup >= funcCode.code.powerUpTimeArriveSet)
        )
    {
        errorOther = ERROR_POWER_UP_TIME_OVER;
    }
#endif

#if DEBUG_F_RUN_TIME

    if ((runFlag.bit.run)
        && ((!diFunc.f2.bit.clearSetRunTime)
        || ((curTime.runTime >= funcCode.code.setTimeArrive)
            &&(funcCode.code.setTimeArrive)))
        )
    {
        if (++runTimeAddupTicker >= RS_RUN_TIME_TICK_MAX)
        {
            runTimeAddupTicker = 0;

            if(++curTime.runTimeSec >= TIME_UNIT_SEC_PER_MIN)
            {
                curTime.runTimeSec = 0;
                curTime.runTimeM++;
            }

            if (++funcCode.code.runTimeAddupSec >= TIME_UNIT_SEC_PER_HOUR)
            {
                funcCode.code.runTimeAddupSec = 0;
                funcCode.code.runTimeAddup++;
            }
        }

    }
    else
    {

        curTime.runTimeSec = 0;
        curTime.runTimeM = 0;
    }

    curTime.runTime = curTime.runTimeM * RS_RUN_TIME_MIN_SCALE + curTime.runTimeSec / RS_TENTH_MIN_SEC;
    curTime.powerOnTime = curTime.powerOnTimeM;

    if (funcCode.code.runTimeArriveSet
        && (funcCode.code.runTimeAddup >= funcCode.code.runTimeArriveSet)
        )
    {
        errorOther = ERROR_RUN_TIME_OVER;
    }

#endif
}

void RunSrcDeal(void)
{

    UpdateRunCmd();

    RunSrcUpdateFrq();

#if DEBUG_F_POSITION_CTRL
{
    UniversalDebug(0);
    UniversalDebug(1);
    UniversalDebug(2);
    UniversalDebug(3);
    UniversalDebug(4);
    UniversalDebug(5);
}
#endif
}

void UpdateBenchTime(void)
{
    switch (funcCode.code.accDecTimeUnit)
    {
        case FUNCCODE_accDecTimeUnit_0POINT:
            timeBench = RS_TIME_BENCH_0POINT;
            break;

        case FUNCCODE_accDecTimeUnit_1POINT:
            timeBench = RS_TIME_BENCH_1POINT;
            break;

        default:
            timeBench = RS_TIME_BENCH_2POINT;
            break;
    }
}

void RunSrcUpdateFrq(void)
{
    static Uint16 errorLevelBak, bRunFrqLatched;
    static int32 frqAtError;

#if (DEBUG_F_POSITION_CTRL)
    if (FUNCCODE_pcZeroSelect_ENCODER == funcCode.code.pcZeroSelect)
    {
        if ((FUNCCODE_aptpMode_INDEX == funcCode.code.aptpMode)
            && (1 == pEQepRegsFvc->QFLG.bit.IEL)
            )
        {
            aptpAbsZeroOk = 1;
            pcOrigin = pEQepRegsFvc->QPOSILAT;
        }
        else if ((!aptpAbsZeroOk)
                && (1 == pEQepRegsFvc->QFLG.bit.IEL)
                )
        {
            aptpAbsZeroOk = 1;
            pcOrigin = pEQepRegsFvc->QPOSILAT;
        }
    }
    else if (FUNCCODE_pcZeroSelect_DI == funcCode.code.pcZeroSelect)
    {
#if 0
        if (diFunc.f3.bit.aptpZero)

        {
            pcOrigin = GetCurPos();
            aptpAbsZeroOk = 1;
        }
#endif
    }
    else if ((FUNCCODE_aptpMode_ABSOLUTE == funcCode.code.aptpMode)
        && (2 == funcCode.code.pcZeroSelect)
        )
    {
        pcOrigin = 0 - qposcntLastPOff;

        aptpAbsZeroOk = 1;
    }

    if (aptpAbsZeroOk)
    {
        aptpCurPos = (int32)(GetCurPos() - pcOrigin);

        pcCurrentPulse = (aptpCurPos / 1);
    }

    RcCutPointDeal();
#endif

    if (runCmd.bit.freeStop)
    {
        runStatus = RUN_STATUS_SHUT_DOWN;
    }
#if DEBUG_F_TUNE

    else if ((runCmd.bit.common) && (tuneCmd))
    {
        if (RUN_STATUS_WAIT == runStatus)
        {
            runStatus = RUN_STATUS_TUNE;
            saveTuneDataFlag = 0;
        }
    }
#endif
    else if (diFunc.f2.bit.brake)
    {
        runStatus = RUN_STATUS_DI_BRAKE;
    }
    else if (diFunc.f2.bit.decBrake)
    {
        if (runFlag.bit.run)
        {
            runStatus = RUN_STATUS_DI_BRAKE_DEC;
        }
        else
        {
            runStatus = RUN_STATUS_DI_BRAKE;
        }
    }
#if DEBUG_F_LOSE_LOAD

    else if (dspStatus.bit.outAirSwitchOff
             && runFlag.bit.run
             && funcCode.code.loseLoadProtectMode)
    {
        if (RUN_STATUS_LOSE_LOAD != runStatus)
        {
            runStatusOld4LoseLoad = runStatus;
        }

        runStatus = RUN_STATUS_LOSE_LOAD;
    }
#endif
#if DEBUG_F_JOG
    else if (runCmd.bit.jog)
    {
        if (RUN_STATUS_JOG != runStatus)
        {
            runStatusOld4Jog = runStatus;
        }

        runStatus = RUN_STATUS_JOG;

        runFlag.bit.jog = 1;
    }
#endif
    else if (runCmd.bit.common)
    {
        if (RUN_STATUS_WAIT == runStatus)
        {
            runStatus = RUN_STATUS_START;
            startRunStatus = START_RUN_STATUS_INIT;
        }
    }

    UpdateFrqAim();

    PowerOffNoStopDeal();

#if DEBUG_F_ERROR_RUN_FRQ

    if (runFlag.bit.run && (ERROR_LEVEL_RUN == errorAttribute.bit.level) && (ERROR_LEVEL_RUN != errorLevelBak))
    {
        bRunFrqLatched = 1;
        frqAtError = frq;
    }

    if ((errorCode == ERROR_NONE)
        ||(ERROR_LEVEL_RUN != errorAttribute.bit.level)
        )
    {
        bRunFrqLatched = 0;
    }

    errorLevelBak = errorAttribute.bit.level;

    if ((ERROR_LEVEL_RUN == errorAttribute.bit.level)
        && (errorCode != ERROR_LOSE_LOAD)
        )
    {

        switch (funcCode.code.errorRunFrqSrc)
        {

            case ERR_RUN_FRQ_RUN:
                if (bRunFrqLatched)
                {
                    frqCurAim = frqAtError;
                }
                else
                {
                    frqCurAim = frqAim;
                }
                break;

            case ERR_RUN_FRQ_AIM:
                frqCurAim = frqAim;
                break;

            case ERR_RUN_FRQ_UPPER:
                frqCurAim = upperFrq;
                break;

            case ERR_RUN_FRQ_LOWER:
                frqCurAim = lowerFrq;
                break;

            case ERR_RUN_FRQ_SECOND:

                frqCurAim = ((Uint32)funcCode.code.errorSecondFrq * funcCode.code.maxFrq) / 1000;

                if (frqAim < 0)
                {
                    frqCurAim = -frqCurAim;
                }
                break;

            default:
                break;
        }
    }
    else
#endif
    {
        frqCurAim = frqAim;
    }

#if (DEBUG_F_POSITION_CTRL)
    AccCalc(frqTmp);

#endif

    if (!shuntFlag)
    {
        switch (runStatus)
        {
            case RUN_STATUS_START:
                StartRunCtrl();
                break;

            case RUN_STATUS_JOG:
                JogRunCtrl();
                break;

            case RUN_STATUS_TUNE:
                TuneRunCtrl();
                break;

            case RUN_STATUS_DI_BRAKE_DEC:

                frqCurAim = 0;
                AccDecFrqCalc(accFrqTime, decFrqTime, funcCode.code.accDecSpdCurve);
                if (!diFunc.f2.bit.decBrake)
                {
                    runStatus = RUN_STATUS_NORMAL;
                }

                else if (ABS_INT32(frqTmp) <= funcCode.code.stopBrakeFrq)
                {

                    runStatus = RUN_STATUS_DI_BRAKE;
                }
                break;

            case RUN_STATUS_DI_BRAKE:
                frqTmp = 0;

                runFlag.bit.run = 1;
                runFlag.bit.common = 1;
                dspMainCmd.bit.run = 1;
                dspMainCmd.bit.stopBrake = 1;
                if ((!diFunc.f2.bit.brake)
                    && (!diFunc.f2.bit.decBrake)
                    )
                {

                    if (runCmd.bit.common)
                    {
                        dspMainCmd.bit.stopBrake = 0;
                        runStatus = RUN_STATUS_NORMAL;
                    }
                    else
                    {
                        runCmd.bit.common0 = 0;
                        runStatus = RUN_STATUS_SHUT_DOWN;
                    }
                }
                break;

            case RUN_STATUS_LOSE_LOAD:
                LoseLoadRunCtrl();
                break;

            case RUN_STATUS_WAIT:
                break;

            case RUN_STATUS_NORMAL:
                break;

            case RUN_STATUS_STOP:
                break;

            case RUN_STATUS_SHUT_DOWN:
                break;

#if DEBUG_F_POSITION_CTRL
            case RUN_STATUS_POS_CTRL:
                PcRunCtrl();
                break;
#endif
            default:
                break;
        }

        if (RUN_STATUS_NORMAL == runStatus)
        {
            NormalRunCtrl();
        }
    }

    if (RUN_STATUS_STOP == runStatus)
    {
        StopRunCtrl();
    }

    if (RUN_STATUS_SHUT_DOWN == runStatus)
    {
        ShutDownRunCtrl();
    }

#if 0

    if (tunePGflag)
    {

        if (motorFc.motorCtrlMode == FUNCCODE_motorCtrlMode_FVC)
        {

            if (tunePGflag == 1)
            {

                errorOther = ERROR_PG_LOST;
			   tunePGflag = 0;
            }

            else if(tunePGflag == 2)
            {

                errorOther = ERROR_PG_PARA_ERROR;
			   tunePGflag = 0;
            }
        }
        else
        {
            if(tunePGflag == 2)
            {
                tunePGflag = 0;
            }
        }
    }

#endif

#if DEBUG_F_POSITION_CTRL

    if (RUN_STATUS_POS_CTRL == runStatus)
    {
        frq = frqPcOut;
        frqFrac = frqPcOutFrac;

        frqCurAim = frq;
        frqCurAimFrac = frqFrac;

#if 0
        if ((frq > (int32)upperFrq + 500)
            || (frq < -(int32)upperFrq - 500)
            )
        {
            errorOther = 97;
        }
#endif
    }
    else
#endif
    {
        frq = frqTmp;
        frqFrac = frqTmpFrac;

    }

#if 0
    if (RUN_STATUS_TUNE != runStatus)
    {
        if (frq > upperFrq)
        {
            frq = upperFrq;
            frqTmpFrac = 0;
        }
        else if (frq < -(int32)upperFrq)
        {
            frq = -(int32)upperFrq;
            frqTmpFrac = 0;
        }
    }
#endif

    if (STOP_REM_WAIT == stopRemFlag)
    {
        if ((FUNCCODE_frqRemMode_STOP_NO == funcCode.code.frqRemMode)
            && (dormantStatus != DORMANT_NO_RESPOND)
            )
        {
            upDownFrq = 0;
            frqFlag.bit.upDownoperationStatus = UP_DN_OPERATION_OFF;
        }
        stopRemFlag = STOP_REM_NONE;
    }
}

extern Uint16 bFrqCurAimChg;
LOCALF void StartRunCtrl(void)
{
    runFlag.bit.run = 1;
    runFlag.bit.common = 1;
    dspMainCmd.bit.run = 1;

    bFrqCurAimChg = 1;

    if (!runCmd.bit.common)
    {
        if (START_RUN_STATUS_HOLD_START_FRQ == startRunStatus)
        {
            runStatus = RUN_STATUS_STOP;
            stopRunStatus = STOP_RUN_STATUS_INIT;
        }
        else
            runStatus = RUN_STATUS_SHUT_DOWN;

        runTimeTicker = 0;

        return;
    }

#if DEBUG_F_START_RUN
    if (bAntiReverseRun)
    {
        runTimeTicker = 0;
        runStatus = RUN_STATUS_NORMAL;
        return;
    }

    switch (startRunStatus)
    {
        case START_RUN_STATUS_SPEED_TRACK:
            if (FUNCCODE_startMode_SPEED_TRACK == funcCode.code.startMode)
            {

                if ((motorFc.motorPara.elem.motorType == MOTOR_TYPE_PMSM)
                    || (motorFc.motorCtrlMode == FUNCCODE_motorCtrlMode_FVC)
                    )
                {
                    startRunStatus = START_RUN_STATUS_BRAKE;
                }
                else
                {
                    dspMainCmd1.bit.speedTrack = 1;

                    if (dspStatus.bit.speedTrackEnd)
                    {
                        dspMainCmd1.bit.speedTrack = 0;
                        frqAimOld4Dir = frqRun;
                        runStatus = RUN_STATUS_NORMAL;
                    }
                    frqTmp = frqRun;
                    break;
                }
            }
            else  if (FUNCCODE_startMode_DIRECT_START == funcCode.code.startMode)
            {
                startRunStatus = START_RUN_STATUS_BRAKE;
            }
            else   if (FUNCCODE_startMode_FORE_MAG == funcCode.code.startMode)
            {

                if (motorFc.motorCtrlMode == FUNCCODE_motorCtrlMode_VF)
                {
                    startRunStatus = START_RUN_STATUS_BRAKE;
                }
                else
                {
                    startRunStatus = START_RUN_STATUS_PRE_FLUX;
                }
            }

        case START_RUN_STATUS_BRAKE:
            if(startRunStatus == START_RUN_STATUS_BRAKE)
            {
                if ((++runTimeTicker >= (Uint32)funcCode.code.startBrakeTime
                     * (Uint16)(TIME_UNIT_START_BRAKE / RUN_CTRL_PERIOD))
                    )
                {
                    runTimeTicker = 0;
                    dspMainCmd.bit.startBrake = 0;
                    startRunStatus = START_RUN_STATUS_HOLD_START_FRQ;
                }
                else
                {
                    dspMainCmd.bit.startBrake = 1;
                    break;
                }
            }

        case START_RUN_STATUS_PRE_FLUX:
            if(startRunStatus == START_RUN_STATUS_PRE_FLUX)
            {
                if ((++runTimeTicker >= (Uint32)funcCode.code.startBrakeTime
                     * (Uint16)(TIME_UNIT_START_BRAKE / RUN_CTRL_PERIOD))
                     )
                {
                    runTimeTicker = 0;
                    dspMainCmd.bit.startFlux = 0;
                    startRunStatus = START_RUN_STATUS_HOLD_START_FRQ;
                }
                else
                {

                    dspMainCmd.bit.startFlux = 1;
                    break;
                }
            }

        case START_RUN_STATUS_HOLD_START_FRQ:
            if (bRunJog)
                break;

            frqTmp = (FORWARD_DIR == runFlag.bit.dirFinal) ? (funcCode.code.startFrq) : (-(int32)funcCode.code.startFrq);
            if (++runTimeTicker >= (Uint32)funcCode.code.startFrqTime
                 * (Uint16)(TIME_UNIT_START_FRQ_WAIT / RUN_CTRL_PERIOD))
            {

                if (FUNCCODE_motorCtrlMode_FVC == motorFc.motorCtrlMode)
                {

                    frqTmp = frqFdbTmp;
                }

                runTimeTicker = 0;
                runStatus = RUN_STATUS_NORMAL;
            }
            else
            {
                frqCurAim = frqTmp;
                frqCurAimOld = frqCurAim;
            }

            break;

        default:
            break;
    }
#elif 1

    runTimeTicker = 0;
    runStatus = RUN_STATUS_NORMAL;

#endif
}

LOCALF void NormalRunCtrl(void)
{
    if (!runCmd.bit.common)
    {

        if (runMode == RUN_MODE_TORQUE_CTRL)
        {
            frq = frqRun;
        }

        swingStatus = SWING_NONE;
        runStatus = RUN_STATUS_STOP;
        stopRunStatus = STOP_RUN_STATUS_INIT;
        return;
    }

    if (runFlag.bit.dirReversing)
    {
        frqCurAim = 0;
    }
    else
    {

#if DEBUG_F_LOWER_FRQ

        LowerThanLowerFrqDeal();
#endif

#if DEBUG_F_SWING

        if (((frq == frqAim)
             || (SWING_NONE != swingStatus))
            && (!bAntiReverseRun)
            && (runMode != RUN_MODE_TORQUE_CTRL)
            )
        {
            SwingDeal();
        }
#endif
    }

    if (!diFunc.f1.bit.forbidAccDecSpd)
    {
#if DEBUG_F_SWING
        if (SWING_NONE != swingStatus)
        {
            AccDecFrqCalc(swingAccTime, swingDecTime, ACC_DEC_LINE);
        }
        else
#endif
        {
            AccDecFrqCalc(accFrqTime, decFrqTime, funcCode.code.accDecSpdCurve);
        }
    }
}

LOCALF void StopRunCtrl(void)
{
    int32 frqStopJudge;
    Uint16 bStopBrakeEnabled = 1;

    if (runCmd.bit.common)
    {
        if (STOP_RUN_STATUS_WAIT_BRAKE == stopRunStatus)
        {

            runStatus = RUN_STATUS_START;
            startRunStatus = START_RUN_STATUS_INIT;
        }
        else
        {
            dspMainCmd.bit.stopBrake = 0;
            runStatus = RUN_STATUS_NORMAL;
        }

        runTimeTicker = 0;

        return;
    }

#if DEBUG_F_STOP_RUN
    switch (stopRunStatus)
    {
        case STOP_RUN_STATUS_DEC_STOP:
            frqCurAim = 0;
            AccDecFrqCalc(accFrqTime, decFrqTime, funcCode.code.accDecSpdCurve);

            frqStopJudge = ((!funcCode.code.ovGain)
                            && (!funcCode.code.ocGain)
                            && (motorFc.motorCtrlMode == FUNCCODE_motorCtrlMode_VF))
                           ? frqTmp : frqRun;

            if (((ABS_INT32(frqStopJudge) <= funcCode.code.stopBrakeFrq)
                 || (ABS_INT32(frqStopJudge) < RS_STOP_BRAKE_MIN_FRQ))
                && bStopBrakeEnabled)
            {
                stopRunStatus = STOP_RUN_STATUS_WAIT_BRAKE;
            }
            else
            {
                if ((!frqTmp)
                    && (ABS_INT32(frqStopJudge) < RS_STOP_BRAKE_MIN_FRQ))
                {
                    runStatus = RUN_STATUS_SHUT_DOWN;
                }

                break;
            }

        case STOP_RUN_STATUS_WAIT_BRAKE:
            frqTmp = 0;
            if ((!bStopBrakeEnabled)
                || (++runTimeTicker >= (Uint32)funcCode.code.stopBrakeWaitTime
                    * (Uint16)(TIME_UNIT_WAIT_STOP_BRAKE / RUN_CTRL_PERIOD)))
            {
                runTimeTicker = 0;
                dspMainCmd.bit.run = 1;
                dspSubCmd.bit.fanRunWhenWaitStopBrake = 0;
                stopRunStatus = STOP_RUN_STATUS_BRAKE;
            }
            else
            {
                dspMainCmd.bit.run = 0;
                dspSubCmd.bit.fanRunWhenWaitStopBrake = 1;
                break;
            }

        case STOP_RUN_STATUS_BRAKE:
            if ((!bStopBrakeEnabled)
                || (++runTimeTicker >= (Uint32)funcCode.code.stopBrakeTime
                    * (Uint16)(TIME_UNIT_STOP_BRAKE / RUN_CTRL_PERIOD)))
            {

                runStatus = RUN_STATUS_SHUT_DOWN;
            }
            else
            {
                dspMainCmd.bit.stopBrake = 1;
            }

            break;

        default:
            break;
    }
#elif 1
    frqCurAim = 0;
    AccDecFrqCalc(accFrqTime, decFrqTime, funcCode.code.accDecSpdCurve);
    if ((!frqTmp)
            && (ABS_INT32(frqRun) < 50)
        )
        runStatus = RUN_STATUS_SHUT_DOWN;
#endif
}

LOCALF void JogRunCtrl(void)
{
#if DEBUG_F_JOG
    runFlag.bit.run = 1;
    runFlag.bit.jog = 1;

    dspMainCmd.bit.run = 1;

    if (runFlag.bit.dirReversing)
    {
        frqCurAim = 0;
    }
#if 0
    else
    {
        frqCurAim = frqAim;
    }
#endif

    if (!runCmd.bit.jog)
    {
        if (runFlag.bit.jogWhenRun)
        {
            runFlag.bit.jog = 0;
            runFlag.bit.jogWhenRun = 0;

            runStatus = runStatusOld4Jog;

            return;
        }
        else
        {
            if (!frqTmp)
            {
                runStatus = RUN_STATUS_SHUT_DOWN;
            }

            frqCurAim = 0;
        }
    }

    if (!diFunc.f1.bit.forbidAccDecSpd)
    {
        AccDecFrqCalc(funcCode.code.jogAccTime, funcCode.code.jogDecTime, ACC_DEC_LINE);
    }
#endif
}

#define TUNE_STEP_WAIT      0
#define TUNE_STEP_ACC       50
#define TUNE_STEP_DEC       51
#define TUNE_STEP_END       100
#define PMSM_TUNE_SPEED     10UL
Uint16 motorCtrlTuneStatus;
LOCALF void TuneRunCtrl(void)
{
#if DEBUG_F_TUNE
    Uint16 tuneStepNext = motorCtrlTuneStatus;
    static Uint16 bTuneStopReq = 0;

#if 0

    if (runFlag.bit.tune)
    {
        motorCtrlTuneStatusTmp = motorCtrlTuneStatus;
    }
#endif

    runFlag.bit.run = 1;
    runFlag.bit.tune = 1;

    dspMainCmd.bit.run = 1;

    if (++runTimeTicker >= TUNE_RUN_OVER_TIME_MAX * (Uint16)(TIME_UNIT_MS_PER_SEC / RUN_CTRL_PERIOD))
    {
        errorOther = ERROR_TUNE;
        return;
    }

    if (saveTuneDataFlag)
    {
        tuneStepNext = TUNE_STEP_DEC;
    }

    if (!runCmd.bit.common)
    {
        bTuneStopReq = 1;
    }

    if (bTuneStopReq)
    {
        tuneStepNext = TUNE_STEP_DEC;
    }

    switch (tuneStepNext)
    {
        case TUNE_STEP_WAIT:
            break;

        case TUNE_STEP_ACC:
            if (MOTOR_TYPE_PMSM == motorFc.motorPara.elem.motorType)
            {

                if (motorFc.motorPara.elem.ratingSpeed > PMSM_TUNE_SPEED)
                {
                    frqCurAim = PMSM_TUNE_SPEED * motorFc.motorPara.elem.ratingFrq / motorFc.motorPara.elem.ratingSpeed;
                }
                else
                {
                    frqCurAim = motorFc.motorPara.elem.ratingFrq;
                }
            }
            else
            {
                frqCurAim = (Uint32)motorFc.motorPara.elem.ratingFrq * RS_TUNE_IM_FRQ_NUM / RS_TUNE_IM_FRQ_DEN;
            }

            if ((FUNCCODE_runDir_REVERSE == funcCode.code.runDir)
                && (MOTOR_TYPE_PMSM == motorFc.motorPara.elem.motorType))
            {
                frqCurAim = -frqCurAim;
            }

            AccDecFrqCalc(accFrqTime, decFrqTime, FUNCCODE_accDecSpdCurve_LINE);
            break;

        case TUNE_STEP_END:
            saveTuneDataFlag = 1;

            SaveTuneData();

        case TUNE_STEP_DEC:
            frqCurAim = 0;
            AccDecFrqCalc(accFrqTime, decFrqTime, FUNCCODE_accDecSpdCurve_LINE);
            if (ABS_INT32(frqTmp) < RS_STOP_BRAKE_MIN_FRQ)
            {
                if (TUNE_STEP_DEC != motorCtrlTuneStatus)
                {
                    runCmd.bit.common0 = 0;
                    runStatus = RUN_STATUS_SHUT_DOWN;
                    bTuneStopReq = 0;
                }
            }
            break;

        default:
            break;
    }
#endif
}

LOCALF void LoseLoadRunCtrl(void)
{
#if DEBUG_F_LOSE_LOAD

    if ((!runCmd.bit.common) && (!runCmd.bit.jog))
    {
        runStatus = RUN_STATUS_STOP;
        stopRunStatus = STOP_RUN_STATUS_INIT;
    }

    else  if (!dspStatus.bit.outAirSwitchOff)
    {
        runStatus = runStatusOld4LoseLoad;
    }
    else
    {
        int32 loseLoadFrqLimit;

        if (GetErrorAttribute(ERROR_LOSE_LOAD) == ERROR_LEVEL_RUN)
        {
            loseLoadFrqLimit = ((int32)motorFc.motorPara.elem.ratingFrq * RS_LOSE_LOAD_FRQ_NUM) / RS_PERCENT;

            if (ABS_INT32(frqAim) > loseLoadFrqLimit)
            {
                frqTmp = (FORWARD_DIR == runFlag.bit.dirFinal) ? (loseLoadFrqLimit) : (-loseLoadFrqLimit);
            }
            else
            {
                frqTmp = frqAim;
            }
        }
    }
#endif
}

LOCALF void ShutDownRunCtrl(void)
{
    dspMainCmd.bit.run = 0;
    dspMainCmd1.bit.speedTrack = 0;
    dspMainCmd.bit.stopBrake = 0;
    dspMainCmd.bit.startBrake = 0;
    dspMainCmd.bit.startFlux = 0;
    dspMainCmd.bit.accDecStatus = 0;
    tuneCmd = 0;

    {
        Uint16 bPaused = runCmd.bit.pause;

        if (runFlag.bit.common)
        {
            stopRemFlag = bPaused ? STOP_REM_PAUSE_JUDGE : STOP_REM_WAIT;
        }

        if (runFlag.bit.common
            && (!errorCode)

            )
        {

            if ((!bPaused) && (FUNCCODE_plcStopRemMode_REM != (funcCode.code.plcPowerOffRemMode/10)))
            {
                plcStep = 0;
                plcTime = 0;
            }
            else
            {
                plcStepRemOld = plcStep;
                plcTimeRemOld = plcTime;
            }
        }

        runTimeTicker = 0;
        lowerDelayTicker = 0;
        shuntTicker = 0;

        runFlag.bit.run = 0;
        runFlag.bit.common = 0;
        runFlag.bit.jog = 0;
        runFlag.bit.tune = 0;
        runFlag.bit.jogWhenRun = 0;
        runFlag.bit.accDecStatus = 0;
        runFlag.bit.servo = 0;

        frqTmp = 0;
        frqTmpFrac = 0;
        swingStatus = SWING_NONE;

        frqCurAimOld = 0;
        accTimeOld = 0;
        decTimeOld = 0;
        frqLine.remainder = 0;

        runStatus = RUN_STATUS_WAIT;
    }
}

LOCALF void UpdateSwingPara(void)
{
#if DEBUG_F_SWING
    int32 swingAmpBase;
    int32 swingAmp;
    Uint32 swingPeriodTick;

    swingFrqLimit = 0;

    if (FUNCCODE_swingBaseMode_AGAIN_MAXFRQ == funcCode.code.swingBaseMode)
    {
        swingAmpBase = maxFrq;
    }
    else
    {
        swingAmpBase = ABS_INT32(frqAim);
    }

    swingAmp = (swingAmpBase * funcCode.code.swingAmplitude) / RS_PERCENT_SCALE;

    swingMaxFrq = ABS_INT32(frqAim) + swingAmp;
    if (swingMaxFrq > upperFrq)
    {
        swingFrqLimit = 1;
        swingMaxFrq = upperFrq;
    }

    swingMinFrq = ABS_INT32(frqAim) - swingAmp;
    if (swingMinFrq < lowerFrq)
    {
        swingFrqLimit = 1;
        swingMinFrq = lowerFrq;
    }

    swingJumpFrq = (swingAmp * funcCode.code.swingJumpRange) / RS_PERCENT_SCALE;

    if (REVERSE_DIR == runFlag.bit.dirFinal)
    {
        swingMaxFrq = -swingMaxFrq;
        swingMinFrq = -swingMinFrq;
        swingJumpFrq = -swingJumpFrq;
    }

    swingDoubleAw = swingAmp << 1;

    swingPeriodTick = ((Uint32)maxFrq * funcCode.code.swingPeriod)
        / (swingDoubleAw - swingJumpFrq) * TIME_UNIT_SWING_PERIOD / timeBench;
    swingAccTime = (swingPeriodTick * funcCode.code.swingRiseTimeCoeff) / RS_PERCENT_SCALE;
    swingDecTime = swingPeriodTick - swingAccTime;
#endif
}

LOCALF void SwingDeal(void)
{
#if DEBUG_F_SWING
    static int32 frqAimBak;
    static Uint32 frqDeltaTol;

    UpdateSwingPara();

    if (funcCode.code.swingPeriod
        && funcCode.code.swingAmplitude
        && (!diFunc.f1.bit.swingPause)
        )
    {
        if (SWING_NONE == swingStatus)
        {
            frqAimBak = frqAim;
            frqDeltaTol = swingDoubleAw >> RS_SWING_DELTA_SHIFT;

            swingStatus = SWING_UP;
        }
    }
    else
    {
        swingStatus = SWING_NONE;
        return;
    }

    if (ABS_INT32(frqAimBak - frqAim) > frqDeltaTol)
    {
        swingStatus = SWING_NONE;
        return;
    }

#if 0
    if (!frqAim)
    {
        frqCurAim = 0;
    }
    else
#endif
    if (SWING_UP == swingStatus)
    {
        frqCurAim = swingMaxFrq;

        if (frqTmp == swingMaxFrq)
        {
            swingStatus = SWING_DOWN;

            frqTmp -= swingJumpFrq;
        }
    }

    if (SWING_DOWN == swingStatus)
    {
        frqCurAim = swingMinFrq;

        if (frqTmp == swingMinFrq)
        {
            swingStatus = SWING_UP;
            frqCurAim = swingMaxFrq;

            frqTmp += swingJumpFrq;
        }
    }
#endif
}

LOCALF void LowerThanLowerFrqDeal(void)
{
#if DEBUG_F_LOWER_FRQ
    if (ABS_INT32(frqAim) < lowerFrq)
    {

        if (!bAntiReverseRun)
        {
            if (FUNCCODE_lowerDeal_RUN_ZERO == funcCode.code.lowerDeal)
            {
                frqCurAim = 0;
            }

            else
            {
                frqCurAim = (FORWARD_DIR == runFlag.bit.dirFinal) ? (lowerFrq) : (-(int32)lowerFrq);
            }

        }

        if (FUNCCODE_lowerDeal_DELAY_STOP == funcCode.code.lowerDeal)
        {

            if (ABS_INT32(frq) < lowerFrq)
            {
                lowerDelayTicker = 0;
                otherStopLowerSrc++;

                runStatus = RUN_STATUS_STOP;
                stopRunStatus = STOP_RUN_STATUS_INIT;
            }

            else if (ABS_INT32(frq) == lowerFrq)
            {
                {
                    lowerDelayTicker = 0;
                    otherStopLowerSrc++;

                    runStatus = RUN_STATUS_STOP;
                    stopRunStatus = STOP_RUN_STATUS_INIT;
                }
            }
        }
        else if (FUNCCODE_lowerDeal_RUN_LOWER == funcCode.code.lowerDeal)
        {
            lowerDelayTicker = 0;
        }
        else
        {
            ;
        }
    }
    else
    {
        lowerDelayTicker = 0;
    }
#endif
}

#define OFF_THRESHOLD_STOP_DEC_VOL_RANGE  50
LOCALF void PowerOffNoStopDeal(void)
{
#if DEBUG_F_POWER_OFF_NO_STOP

    Uint16 ratingDcBus;

    if ((!funcCode.code.pOffTransitoryNoStop)
        || ((!runCmd.bit.common) && (!runCmd.bit.jog)))
    {
        shuntTicker = 0;
        shuntFlag = 0;

        return;
    }

    runFlag.bit.run = 1;

    ratingDcBus = (Uint32)invPara.ratingVoltage * RS_DC_BUS_SCALE >> RS_DC_BUS_SHIFT;

#if DSP_2803X
    if (generatrixVoltage < (Uint32)funcCode.code.pOffThresholdVol * ratingDcBus * RS_POFF_THRESHOLD_SCALE >> RS_POFF_THRESHOLD_SHIFT)
#else
    if (generatrixVoltage < (Uint32)funcCode.code.pOffThresholdVol * ratingUdc / 1000)
#endif
    {
        if (!shuntFlag)
        {
            runStatusOld4POffNoStop = runStatus;
        }
        shuntFlag = 1;
        shuntTicker = 0;
    }
    else
    {
        if (shuntFlag)
        {
            if (++shuntTicker >= (Uint32)funcCode.code.pOffVolBackTime
                * (Uint16)(TIME_UNIT_P_OFF_VOL_BACK / RUN_CTRL_PERIOD))
            {
                shuntTicker = 0;
                shuntFlag = 0;
                runStatus = runStatusOld4POffNoStop;
            }
        }
        else
        {
            shuntTicker = 0;
        }
    }

    if (shuntFlag)
    {
        frqCurAim = 0;

        if (generatrixVoltage < ((Uint32)funcCode.code.pOffTransitoryFrqDecSlope * ratingDcBus / RS_POFF_SLOPE_SCALE))
        {
            AccDecFrqCalc(accFrqTime, funcCode.code.decTime4, funcCode.code.accDecSpdCurve);
        }

        if ((0 == frqRun)
            && (funcCode.code.pOffTransitoryNoStop == 2)
            )
        {
            otherStopSrc++;
        }
    }

#endif
}

void AccDecTimeCalc(void)
{
    Uint16 accDecTimeSel;
    static Uint16 bStop4Dec;

#if DEBUG_F_ACC_DEC_TIME
    if (RUN_MODE_TORQUE_CTRL == runMode)
    {
        accFrqTime = 0;
        decFrqTime = 0;
    }
    else
    {
        if ((runFlag.bit.plc)
            && (!diSelectFunc.f1.bit.accDecTimeSrc)
            )
        {
            accDecTimeSel = accDecTimeSrcPlc;
        }
        else
        {
            accDecTimeSel = diFunc.f1.bit.accDecTimeSrc;

            if (!diSelectFunc.f1.bit.accDecTimeSrc)
            {
                if (runFlag.bit.accDecStatus == ACC_SPEED)
                {
                    accDecTimeSel = (ABS_INT32(frq) >= funcCode.code.accTimefrqChgValue)
                                    ? RS_ACC_DEC_SEL_1 : RS_ACC_DEC_SEL_2;
                }
                else if (runFlag.bit.accDecStatus == DEC_SPEED)
                {
                    accDecTimeSel = (ABS_INT32(frq) >= funcCode.code.decTimefrqChgValue)
                                    ? RS_ACC_DEC_SEL_1 : RS_ACC_DEC_SEL_2;
                }
                else
                {
                    accDecTimeSel = RS_ACC_DEC_SEL_1;
                }
            }

        }

        if ((MOTOR_SN_1 != motorSn) &&
            (0 != motorFc.accDecTimeMotor))
        {
            accDecTimeSel = motorFc.accDecTimeMotor - 1;
        }

        if (RS_ACC_DEC_SEL_4 == accDecTimeSel)
        {
            accFrqTime = funcCode.code.accTime4;
            decFrqTime = funcCode.code.decTime4;
        }
        else if (RS_ACC_DEC_SEL_3 == accDecTimeSel)
        {
            accFrqTime = funcCode.code.accTime3;
            decFrqTime = funcCode.code.decTime3;
        }
        else if (RS_ACC_DEC_SEL_2 == accDecTimeSel)
        {
            accFrqTime = funcCode.code.accTime2;
            decFrqTime = funcCode.code.decTime2;
        }
        else
#endif
        {
            accFrqTime = funcCode.code.accTime1;
            decFrqTime = funcCode.code.decTime1;
        }
    }

    if (diFunc.f2.bit.stop4dec)
    {
        bStop4Dec = 1;
    }

    if (!runFlag.bit.run)
    {
        bStop4Dec = 0;
    }

    if (bStop4Dec)
    {
         decFrqTime = funcCode.code.decTime4;
    }

    if (diFunc.f2.bit.emergencyStop)
    {
        decFrqTime = 0;
    }

    UpdateBenchTime();
}

void DormantDeal(void)
{
#if DEBUG_F_DORMANT_DEAL

    static Uint32 stopTicker = 0;
    static Uint32 startTicker = 0;

    int16  dormantAi1Volt = aiDeal[0].voltage;
    Uint16 dormantHighThr = funcCode.group.f9[RS_DORMANT_HIGH_FC];
    Uint16 dormantLowThr  = funcCode.group.f9[RS_DORMANT_LOW_FC];
    Uint16 dormantStopSet  = funcCode.group.f9[RS_DORMANT_STOP_FC];
    Uint16 dormantStartSet = funcCode.group.f9[RS_DORMANT_START_FC];

    Uint32 dormantStopTick  = (Uint32)dormantStopSet  * TIME_UNIT_WAKE_UP / RUN_CTRL_PERIOD;
    Uint32 dormantStartTick = (Uint32)dormantStartSet * TIME_UNIT_WAKE_UP / RUN_CTRL_PERIOD;

    if (runFlag.bit.run && (dormantAi1Volt >= (int16)dormantHighThr))
    {
        if ((dormantStopTick == 0) || (++stopTicker >= dormantStopTick))
        {
            dormantStatus = DORMANT_NO_RESPOND;
            stopTicker = 0;
        }
    }
    else
    {
        stopTicker = 0;
    }

    if (!runFlag.bit.run
        && (dormantStatus == DORMANT_NO_RESPOND)
        && (dormantAi1Volt <= (int16)dormantLowThr))
    {
        if ((dormantStartTick == 0) || (++startTicker >= dormantStartTick))
        {
            frqAimTmp0 = (int32)funcCode.code.wakeUpFrq;
            dormantStatus = DORMANT_RESPOND;
            startTicker = 0;
        }
    }
    else
    {
        startTicker = 0;
    }

#endif
}

#if DEBUG_F_POSITION_CTRL

#define ACCEL_NUM     10
int32 accelArray[ACCEL_NUM];
int32 accelTickerArray[ACCEL_NUM];
int32 accel;
Uint16 accelDisp;
void AccCalc(int32 frq0)
{
    int16 i;
    int32 accelSum;
    static int32 frqOld;
    static int16 k;
    int16 tickerSum;
    static Uint16 runOld;

    if ((!runOld) && (runFlag.bit.run))
    {
        for (i = ACCEL_NUM - 1; i >= 0; i--)
        {
            accelArray[i] = 0;
            accelTickerArray[i] = 0;
        }

        k = 0;
        frqOld = 0;
        accel = 0;
        accelSum = 0;
        tickerSum = 0;
    }
    runOld = runFlag.bit.run;

    if (!runFlag.bit.run)
    {
        accel = 0;
        return;
    }

    k++;
    if (frq0 != frqOld)
    {
        accelSum = 0;
        tickerSum = 0;
        for (i = ACCEL_NUM - 1; i > 0; i--)
        {
            accelArray[i] = accelArray[i-1];
            accelSum += accelArray[i];

            accelTickerArray[i] = accelTickerArray[i-1];
            tickerSum += accelTickerArray[i];
        }
        accelArray[0] = frq0 - frqOld;
        accelSum += accelArray[0];
        accelTickerArray[0] = k;
        tickerSum += accelTickerArray[0];

        accel = (accelSum << 8) / tickerSum;

        k = 0;
    }
    else if (k >= 200)
    {
        accel = 0;

        for (i = ACCEL_NUM - 1; i >= 0; i--)
        {
            accelArray[i] = 0;

        }
    }

    accelDisp = accel * 500 >> 8;

#if 0
    if (ABS_INT32((int16)accelDisp) > 900)
    {
        asm(" nop");
    }
#endif

#if 0
    if (ABS_INT32(accel) < 100)
    {
        for (i = ACCEL_NUM - 1; i >= 0; i--)
        {
            accelArray[i] = 0;
            accelTickerArray[i] = 0;
        }
        accelArray[0] = accel * ACCEL_NUM;
        accelTickerArray[0] = 1;
    }
#endif

#if 0
    if (ABS_INT32(frqTmp) < 50)
    {
        accel = accelArray[0];
    }
#endif

    frqOld = frq0;
}
#endif
