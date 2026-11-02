
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
    static Uint16 bMfkSwitch;
    static Uint16 fwdOld, revOld, fJogOld, rJogOld;
    static Uint16 startProtectSrcOld;
    static Uint16 otherStopSrcOld;
    static Uint16 otherStopLowerSrcOld;
    static Uint16 bJog, bRun;
    static Uint16 jogCmdPulse;
    static Uint16 runSrcBak;
    Uint16 jogCmdLevel;
    Uint16 jogCmd;
    Uint16 protectCmd;
    Uint16 fwd, rev, fJog, rJog;

    jogCmdLevel = 0;
    protectCmd = 0;

    fwd = diFunc.f1.bit.fwd;
    rev = diFunc.f1.bit.rev;
    fJog = diFunc.f1.bit.fwdJog;
    rJog = diFunc.f1.bit.revJog;

    if (POWER_ON_WAIT == powerOnStatus)
    {
        fwdOld = fwd;
        revOld = rev;
        fJogOld = fJog;
        rJogOld = rJog;
    }

    if (KEY_SWITCH == keyFunc)
    {
        bMfkSwitch = !bMfkSwitch;
        keyFunc = 0;
    }
    if (FUNCCODE_mfkKeyFunc_SWITCH != funcCode.code.mfkKeyFunc)
    {
        bMfkSwitch = 0;
    }

    runSrc = funcCode.code.runSrc;

    if (bMfkSwitch)
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

    if (runSrcBak != runSrc)
    {
		runCmd.bit.common0 = 0;
        runCmd.bit.common = 0;
    }
    runSrcBak = runSrc;

    runCmd.bit.freeStop = 0;
    runCmd.bit.errorReset = 0;
    runCmd.bit.pause = 0;

    switch (runSrc)
    {
        case FUNCCODE_runSrc_PANEL:
            jogCmdPulse = 0;

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
            jogCmdPulse = 0;

            switch (funcCode.code.diControlMode)
            {
                case FUNCCODE_diControlMode_2LINE1:
                case FUNCCODE_diControlMode_2LINE2:
                    if (FUNCCODE_diControlMode_2LINE1 == funcCode.code.diControlMode)
                    {
                        if (fwd && (!rev))
                        {
                            runCmd.bit.dir = FORWARD_DIR;
                            runCmd.bit.common0 = 1;
                        }
                        else if ((!fwd) && (rev))
                        {
                            runCmd.bit.dir = REVERSE_DIR;
                            runCmd.bit.common0 = 1;
                        }
                        else
                        {
                            runCmd.bit.common0 = 0;
                        }

                        protectCmd = 1;

                    }
                    else
                    {
                        runCmd.bit.common0 = fwd;

                        if (rev)
                            runCmd.bit.dir = REVERSE_DIR;
                        else
                            runCmd.bit.dir = FORWARD_DIR;

                        protectCmd = 2;

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
                    if (fwdOld != fwd)
                    {
                        if (fwd)
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
                        if (revOld != rev)
                        {
                            if (rev)
                            {
                                runCmd.bit.common0 = 1;
                                runCmd.bit.dir = REVERSE_DIR;
                            }
                        }
                    }
                    else
                    {
                        rev ? (runCmd.bit.dir = REVERSE_DIR) : (runCmd.bit.dir = FORWARD_DIR);
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
                    jogCmdPulse = RUN_CMD_FWD_JOG;
                    break;

                case SCI_RUN_CMD_REV_JOG:
                    jogCmdPulse = RUN_CMD_REV_JOG;
                    break;

                case SCI_RUN_CMD_FREE_STOP:
                    otherStopSrc++;
                    runCmd.bit.freeStop = 1;

                case SCI_RUN_CMD_STOP:
                    jogCmdPulse = 0;
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
        jogCmdPulse = 0;
    }

    if (startProtectSrcOld != startProtectSrc)
    {
        bJog = 0;
        bRun = 0;

        runCmd.bit.startProtect = 1;
    }

    if ((otherStopSrcOld != otherStopSrc)
        || (otherStopLowerSrcOld != otherStopLowerSrc))
    {
        bJog = 0;
        bRun = 0;

        runCmd.bit.otherStop = 1;
    }

    if (!funcCode.code.startProtect)
    {
        runCmd.bit.startProtect = 0;
    }

    if (runCmd.bit.startProtect || runCmd.bit.otherStop)
    {
        if ((fJogOld != fJog)
            || (rJogOld != rJog)
            || ((!fJog) && (!rJog))
            )
        {
            bJog = 1;
        }

        if (!bJog)
        {
            fJog = 0;
            rJog = 0;
        }

        if (!runCmd.bit.common0)
        {
            bRun = 1;
        }

        if ((!runFlag.bit.common)
            && (FUNCCODE_pidCalcMode_YES == funcCode.code.pidCalcMode)
            && (otherStopLowerSrcOld != otherStopLowerSrc)
            )
        {
            otherStopLowerSrcOld = otherStopLowerSrc;

            bRun = 1;
        }

        if (!bRun)
        {
            runCmd.bit.common0 = 0;
        }

        if (!protectCmd)
        {
            if (bJog)
            {
                runCmd.bit.startProtect = 0;
                runCmd.bit.otherStop = 0;

                otherStopLowerSrcOld = otherStopLowerSrc;
            }
        }
        else
        {
            if (bRun && bJog)
            {
                runCmd.bit.startProtect = 0;
                runCmd.bit.otherStop = 0;

                otherStopLowerSrcOld = otherStopLowerSrc;
            }
        }
    }

    fwdOld = diFunc.f1.bit.fwd;
    revOld = diFunc.f1.bit.rev;
    fJogOld = diFunc.f1.bit.fwdJog;
    rJogOld = diFunc.f1.bit.revJog;

    startProtectSrcOld = startProtectSrc;
    otherStopSrcOld = otherStopSrc;
#endif

    commRunCmd = 0;

#if DEBUG_F_PLC_CTRL

   funcCode.code.plcCmd = 0;
#endif

#if DEBUG_F_JOG

    if ((KEY_FWD_JOG == keyFunc) && (runSrc == FUNCCODE_runSrc_PANEL))
    {
        jogCmdLevel = RUN_CMD_FWD_JOG;
    }
    else if ((KEY_REV_JOG == keyFunc) && (runSrc == FUNCCODE_runSrc_PANEL))
    {
        jogCmdLevel = RUN_CMD_REV_JOG;
    }
    else if ((fJog) && (runSrc == FUNCCODE_runSrc_DI))
    {
        jogCmdLevel = RUN_CMD_FWD_JOG;
    }
    else if ((rJog) && (runSrc == FUNCCODE_runSrc_DI))
    {
        jogCmdLevel = RUN_CMD_REV_JOG;
    }

#endif

    if ((ERROR_LEVEL_FREE_STOP == errorAttribute.bit.level)
       	|| (((runStatus == RUN_STATUS_TUNE)
       	    || (dspMainCmd1.bit.speedTrack)
       	    )
       	    &&  errorCode
       	   )
		)
    {
        if (runCmd.bit.common0)
        {
            menu0DispStatus = MENU0_DISP_STATUS_ERROR;
        }

        runCmd.bit.freeStop = 1;
    }
    else if (ERROR_LEVEL_STOP == errorAttribute.bit.level)
    {
        if (runCmd.bit.common0)
        {
            menu0DispStatus = MENU0_DISP_STATUS_ERROR;
        }

        runCmd.bit.common0 = 0;
    }
    else if (ERROR_LEVEL_RUN == errorAttribute.bit.level)
    {
        ;
    }

    DormantDeal();

    runCmd.bit.common = 0;
    if ((runCmd.bit.common0)
        && (DORMANT_RESPOND == dormantStatus)
        )
    {
        Uint16 common = 0;

        if ((runFlag.bit.common) || (runFlag.bit.tune))
        {
            common = 1;
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
                common = 1;
            }
            else
            {
                if (2 == mainLoopTicker)
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

        if (common &&
            (!diFunc.f1.bit.runPause)
            )
        {
            runCmd.bit.common = 1;
        }
    }

    if (diFunc.f1.bit.closePwm

        || (POWER_ON_WAIT == powerOnStatus)
        || (FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA == funcCodeRwMode)
        || (FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA_ALL == funcCodeRwMode)

        )
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
        jogCmdPulse = 0;
        jogCmdLevel = 0;
    }

    jogCmd = jogCmdLevel | jogCmdPulse;

    bRunJog = 0;
    if ((jogCmd)
        && (runFlag.bit.common
            || ((runCmd.bit.pause) && (runCmd.bit.common0))
            || runCmd.bit.common
            )
        )
    {
        if (funcCode.code.jogWhenRun)
        {
            bRunJog = 1;

            if ((!dspMainCmd.bit.startBrake)
                && (!dspMainCmd.bit.stopBrake)
                && (!dspMainCmd.bit.startFlux)
                )
            {
                runFlag.bit.jogWhenRun = 1;
            }
            else
            {
                jogCmd = 0;
            }
        }
        else
        {
            jogCmd = 0;
        }
    }

#if DEBUG_F_JOG
    if (RUN_CMD_FWD_REV_JOG == jogCmd)
    {
        jogCmd = RUN_CMD_FWD_JOG;
    }

    runCmd.bit.jog = jogCmd;

    if (jogCmdPulse != jogCmd)
    {
        jogCmdPulse = 0;
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
	Uint16 i;

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
            i = funcCode.code.setTimeSource - FUNCCODE_plcFrq0Src_AI1;
            setRunTimeAim = ((int32)ABS_INT32(aiDeal[i].set) * setRunTimeAim + (1 << 14)) >> 15;
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
    if(++powerUpTimeAddupTicker >= ((Uint16)(TIME_UNIT_MS_PER_SEC / RUN_TIME_CAL_PERIOD)))
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
        if (++runTimeAddupTicker >= ((Uint16)(TIME_UNIT_MS_PER_SEC / RUN_TIME_CAL_PERIOD)))
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

    curTime.runTime = curTime.runTimeM*10 + curTime.runTimeSec/6;
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
            timeBench = 1000;
            break;

        case FUNCCODE_accDecTimeUnit_1POINT:
            timeBench = 100;
            break;

        default:
            timeBench = 10;
            break;
    }
}

void RunSrcUpdateFrq(void)
{
    static Uint16 errorLevelOld, runOldFrqFlag;
    static int32 frqOld;

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

    if (runFlag.bit.run && (ERROR_LEVEL_RUN == errorAttribute.bit.level) && (ERROR_LEVEL_RUN != errorLevelOld))
    {
        runOldFrqFlag = 1;
        frqOld = frq;
    }

    if ((errorCode == ERROR_NONE)
        ||(ERROR_LEVEL_RUN != errorAttribute.bit.level)
        )
    {
        runOldFrqFlag = 0;
    }

    errorLevelOld = errorAttribute.bit.level;

    if ((ERROR_LEVEL_RUN == errorAttribute.bit.level)
        && (errorCode != ERROR_LOSE_LOAD)
        )
    {

        switch (funcCode.code.errorRunFrqSrc)
        {

            case ERR_RUN_FRQ_RUN:
                if (runOldFrqFlag)
                {
                    frqCurAim = frqOld;
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
    int32 tmp;
    Uint16 enableStopBrake = 1;

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

            if ((!funcCode.code.ovGain)
                && (!funcCode.code.ocGain)
                && (motorFc.motorCtrlMode == FUNCCODE_motorCtrlMode_VF)
                )
            {
                tmp = frqTmp;
            }
            else
            {
                tmp = frqRun;
            }

            if (((ABS_INT32(tmp) <= funcCode.code.stopBrakeFrq)
			    || (ABS_INT32(tmp) < 50))

                && enableStopBrake)
            {
                stopRunStatus = STOP_RUN_STATUS_WAIT_BRAKE;

            }
            else
            {
                if ((!frqTmp)
                    && (ABS_INT32(tmp) < 50)
                    )
                    runStatus = RUN_STATUS_SHUT_DOWN;

                break;
            }

        case STOP_RUN_STATUS_WAIT_BRAKE:
            frqTmp = 0;
            if ((!enableStopBrake)
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
            if ((!enableStopBrake)
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
    Uint16 motorCtrlTuneStatusTmp = motorCtrlTuneStatus;
    static Uint16 stopFlag = 0;

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
        motorCtrlTuneStatusTmp = TUNE_STEP_DEC;
    }

    if (!runCmd.bit.common)
    {
        stopFlag = 1;
    }

    if (stopFlag)
    {
        motorCtrlTuneStatusTmp = TUNE_STEP_DEC;
    }

    switch (motorCtrlTuneStatusTmp)
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
                frqCurAim = (Uint32)motorFc.motorPara.elem.ratingFrq * 4 / 5;
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
            if (ABS_INT32(frqTmp) < 50)
            {
                if (TUNE_STEP_DEC != motorCtrlTuneStatus)
                {
                    runCmd.bit.common0 = 0;
                    runStatus = RUN_STATUS_SHUT_DOWN;
                    stopFlag = 0;
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
        int32 tmp;

        if (GetErrorAttribute(ERROR_LOSE_LOAD) == ERROR_LEVEL_RUN)
        {
            tmp = ((int32)motorFc.motorPara.elem.ratingFrq * 7) / 100;

            if (ABS_INT32(frqAim) > tmp)
            {
                frqTmp = (FORWARD_DIR == runFlag.bit.dirFinal) ? (tmp) : (-tmp);
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
        Uint16 pause = runCmd.bit.pause;

        if (runFlag.bit.common)
        {
            if (!pause)
            {
                stopRemFlag = STOP_REM_WAIT;
            }
            else
            {
                stopRemFlag = STOP_REM_PAUSE_JUDGE;
            }
        }

        if (runFlag.bit.common
            && (!errorCode)

            )
        {

            if ((!pause) && (FUNCCODE_plcStopRemMode_REM != (funcCode.code.plcPowerOffRemMode/10)))
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
    int32 swingAwBase;
    int32 swingAw;
    Uint32 tmp;

	swingFrqLimit = 0;

    if (FUNCCODE_swingBaseMode_AGAIN_MAXFRQ == funcCode.code.swingBaseMode)
    {
        swingAwBase = maxFrq;
    }
    else
    {
        swingAwBase = ABS_INT32(frqAim);
    }

    swingAw = (swingAwBase * funcCode.code.swingAmplitude) / 1000;

    swingMaxFrq = ABS_INT32(frqAim) + swingAw;
    if (swingMaxFrq > upperFrq)
    {
    	swingFrqLimit = 1;
        swingMaxFrq = upperFrq;
    }

    swingMinFrq = ABS_INT32(frqAim) - swingAw;
    if (swingMinFrq < lowerFrq)
    {
    	swingFrqLimit = 1;
        swingMinFrq = lowerFrq;
    }

    swingJumpFrq = (swingAw * funcCode.code.swingJumpRange) / 1000;

    if (REVERSE_DIR == runFlag.bit.dirFinal)
    {
        swingMaxFrq = -swingMaxFrq;
        swingMinFrq = -swingMinFrq;
        swingJumpFrq = -swingJumpFrq;
    }

    swingDoubleAw = swingAw << 1;

    tmp = ((Uint32)maxFrq * funcCode.code.swingPeriod)
        / (swingDoubleAw - swingJumpFrq) * TIME_UNIT_SWING_PERIOD / timeBench;
    swingAccTime = (tmp * funcCode.code.swingRiseTimeCoeff) / 1000;
    swingDecTime = tmp - swingAccTime;
#endif
}

LOCALF void SwingDeal(void)
{
#if DEBUG_F_SWING
    static int32 frqAimOld;
    static Uint32 frqDeltaOldMax;

    UpdateSwingPara();

    if (funcCode.code.swingPeriod
        && funcCode.code.swingAmplitude
        && (!diFunc.f1.bit.swingPause)
        )
    {
        if (SWING_NONE == swingStatus)
        {
            frqAimOld = frqAim;
            frqDeltaOldMax = swingDoubleAw >> 5;

            swingStatus = SWING_UP;
        }
    }
    else
    {
        swingStatus = SWING_NONE;
        return;
    }

    if (ABS_INT32(frqAimOld - frqAim) > frqDeltaOldMax)
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

    Uint16 ratingUdc;

    if ((!funcCode.code.pOffTransitoryNoStop)
        || ((!runCmd.bit.common) && (!runCmd.bit.jog)))
    {
        shuntTicker = 0;
        shuntFlag = 0;

        return;
    }

    runFlag.bit.run = 1;

    ratingUdc = (Uint32)invPara.ratingVoltage * 14482 >> 10;

#if DSP_2803X
    if (generatrixVoltage < (Uint32)funcCode.code.pOffThresholdVol * ratingUdc * 131  >> 17)
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

        if (generatrixVoltage < ((Uint32)funcCode.code.pOffTransitoryFrqDecSlope * ratingUdc / 1000))
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
    Uint16 accDecTimeSrc;
	static Uint16 stop4DecFlag;

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
            accDecTimeSrc = accDecTimeSrcPlc;
        }
        else
        {
            accDecTimeSrc = diFunc.f1.bit.accDecTimeSrc;

            if (!diSelectFunc.f1.bit.accDecTimeSrc)
            {

    			if (runFlag.bit.accDecStatus == ACC_SPEED)
    			{
    				if (ABS_INT32(frq) >= funcCode.code.accTimefrqChgValue)
    				{
    					accDecTimeSrc = 0;
    				}
    				else
    				{
    					accDecTimeSrc = 1;
    				}
    			}

    			else if (runFlag.bit.accDecStatus == DEC_SPEED)
    			{
    				if (ABS_INT32(frq) >= funcCode.code.decTimefrqChgValue)
    				{
    					accDecTimeSrc = 0;
    				}
    				else
    				{
    					accDecTimeSrc = 1;
    				}
    			}

    			else
    			{
    				accDecTimeSrc = 0;
    			}
            }

        }

        if ((MOTOR_SN_1 != motorSn) &&
            (0 != motorFc.accDecTimeMotor))
        {
            accDecTimeSrc = motorFc.accDecTimeMotor - 1;
        }

        if (3 == accDecTimeSrc)
        {
            accFrqTime = funcCode.code.accTime4;
            decFrqTime = funcCode.code.decTime4;
        }
        else if (2 == accDecTimeSrc)
        {
            accFrqTime = funcCode.code.accTime3;
            decFrqTime = funcCode.code.decTime3;
        }
        else if (1 == accDecTimeSrc)
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
        stop4DecFlag = 1;
    }

    if (!runFlag.bit.run)
    {
        stop4DecFlag = 0;
    }

    if (stop4DecFlag)
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

    int16  ai1Volt = aiDeal[0].voltage;
    Uint16 highThr = funcCode.group.f9[31];
    Uint16 lowThr  = funcCode.group.f9[32];
    Uint16 stopTimeSet  = funcCode.group.f9[33];
    Uint16 startTimeSet = funcCode.group.f9[34];

    Uint32 stopTimeTick  = (Uint32)stopTimeSet  * TIME_UNIT_WAKE_UP / RUN_CTRL_PERIOD;
    Uint32 startTimeTick = (Uint32)startTimeSet * TIME_UNIT_WAKE_UP / RUN_CTRL_PERIOD;

    if (runFlag.bit.run && (ai1Volt >= (int16)highThr))
    {
        if (stopTimeTick == 0)
        {
            dormantStatus = DORMANT_NO_RESPOND;
            stopTicker = 0;
        }
        else
        {
            if (++stopTicker >= stopTimeTick)
            {
                dormantStatus = DORMANT_NO_RESPOND;
                stopTicker = 0;
            }
        }
    }
    else
    {
        stopTicker = 0;
    }

    if (!runFlag.bit.run
        && (dormantStatus == DORMANT_NO_RESPOND)
        && (ai1Volt <= (int16)lowThr))
    {
        if (startTimeTick == 0)
        {

            frqAimTmp0 = (int32)funcCode.code.wakeUpFrq;
            dormantStatus = DORMANT_RESPOND;
            startTicker = 0;
        }
        else
        {
            if (++startTicker >= startTimeTick)
            {
                frqAimTmp0 = (int32)funcCode.code.wakeUpFrq;
                dormantStatus = DORMANT_RESPOND;
                startTicker = 0;
            }
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
