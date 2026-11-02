
#include "f_frqSrc.h"
#include "f_main.h"
#include "f_runSrc.h"
#include "f_io.h"
#include "f_menu.h"
#include "f_ui.h"
#include "f_p2p.h"
#include "f_comm.h"

#define FRQ_PERCENT_SCALE       10000
#define FRQ_PERCENT_ROUND       5000
#define FRQ_PERCENT             100
#define FRQ_SLOPE_SCALE         10000
#define FRQ_Q15_ROUND           (1 << 14)
#define FRQ_Q15_SHIFT           15
#define FRQ_PU_HEADROOM_STEPS   20
#define FRQ_SRC_SELECT_DIVISOR  10
#define FRQ_PLC_SET_SCALE       1000

int32 upDownFrq;
Uint16 upDownFrqInit;
int32 upDownFrqTmp;

int32 uPDownFrqMax;
int32 uPDownFrqMin;
int32 uPDownFrqLower1;
int32 uPDownFrqLower2;

#if F_DEBUG_RAM

#define DEBUG_F_MULTI_SET_PLC       0
#define DEBUG_F_JUMP_FRQ            0
#define DEBUG_F_FRQ_SRC_COMPOSE     0

#elif 1

#define DEBUG_F_MULTI_SET_PLC       1
#define DEBUG_F_JUMP_FRQ            1
#define DEBUG_F_FRQ_SRC_COMPOSE     1

#endif

union FRQ_FLAG frqFlag;

#define PLC_END_LOOP_TIME_MAX   250
Uint16 plcStep;
Uint32 plcTime;
Uint16 bPlcEndOneLoop;
Uint16 plcStepRemOld;
Uint32 plcTimeRemOld;

int32 frq;
int32 frqTmp;
int32 frqFrac;
int32 frqTmpFrac;
int32 frqDroop;
int32 frqCurAimFrac;

int32 frqAim;
int32 frqAimTmp;
int32 frqAimTmp0;

int32 frqCurAim;

Uint16 timeBench;

int32 frqDigitalTmp;

struct FRQ_XY frqXy;

LOCALF int32 frqDigitalPlc;
LOCALF Uint16 presetFrqOld;

Uint16 upperFrq;
Uint16 lowerFrq;
Uint16 maxFrq;
Uint16 benchFrq;
int32 frqAiPu;

Uint16 frqPuQ15;

Uint16 frqCalcSrc;
Uint16 frqCalcSrcOld;
Uint16 plcStepOld;
Uint16 bStopPlc;

Uint16 bFrqDigital;
Uint16 frqKeyUpDownDelta;

LOCALF Uint32 reverseTicker;

Uint16 bAntiReverseRun;

LOCALF JUMP_FRQ jumpFrq1;
LOCALF JUMP_FRQ jumpFrq2;

Uint16 runMode;
int32 frqAimOld4Dir;

Uint16 runCmdBind;

LOCALD int32 FrqAimUpDownDeal(void);
LOCALD int32 FrqSrcOperate(void);
LOCALD int32 FrqXyCalc(Uint16 src);
LOCALD int32 FrqPlcSetDeal(void);
LOCALD void JumpFrqDeal(void);
LOCALD void JumpFrqEndCalc(JUMP_FRQ *p);
LOCALD void UpdateLimitFrq(void);
LOCALD void UpdateRunMode(void);

int32 GetFrqX(void);
int32 GetFrqY(void);
int32 GetFrqComp(void);
int32 GetFrq(void);

Uint16 frqPlcType;

void FrqSrcDeal(void)
{
    static int32 frqAimTmpBak;

    UpdateLimitFrq();

    UpdateRunMode();

    runFlag.bit.plc = 0;
    runFlag.bit.pid = 0;
    runFlag.bit.torque = 0;

    frqPlcType = 0;

    if (RUN_MODE_TORQUE_CTRL == runMode)
    {
        runFlag.bit.torque = 1;
    }

    frqCalcSrc = funcCode.code.frqCalcSrc;

    if (frqCalcSrcOld != frqCalcSrc)
    {
        frqCurAimFrac = 0;
    }

    if (presetFrqOld != funcCode.code.presetFrq)
    {
        presetFrqOld = funcCode.code.presetFrq;
    }

    if ((!runFlag.bit.jog) && (RUN_MODE_TORQUE_CTRL != runMode))
    {
        frqAimTmp0 = FrqSrcOperate();
    }
    else
    {
        accDecFrqPrcFlag = ACC_DEC_FRQ_NONE;
    }

    if (diFunc.f1.bit.resetPLC)
    {
        plcStep = 0;
        plcTime = 0;
    }

    frqCalcSrcOld = frqCalcSrc;
    plcStepOld = plcStep;

    if (frqAimTmp0 > upperFrq)
    {
        frqAimTmp0 = upperFrq;
    }
    else if (frqAimTmp0 < -(int32)upperFrq)
    {
        frqAimTmp0 = -(int32)upperFrq;
    }

#if 0

    if (RUN_MODE_TORQUE_CTRL == runMode)
    {

        if (frqAimTmp0 >= 0)
        {
            if (frqAimTmp0 > funcCode.code.torqueCtrlFwdMaxFrq)
            {
                frqAimTmp0 = funcCode.code.torqueCtrlFwdMaxFrq;
            }
        }

        else
        {
            if (frqAimTmp0 < -(int32)funcCode.code.torqueCtrlRevMaxFrq)
            {
                (frqAimTmp0 = -(int32)funcCode.code.torqueCtrlRevMaxFrq);
            }
        }
    }

#endif

    if (diSelectFunc.f2.bit.frqOk && (!diFunc.f2.bit.frqOk))
    {
        frqAimTmp0 = frqAimTmpBak;
    }
    else
    {
        frqAimTmpBak = frqAimTmp0;
    }

    frqAimTmp = frqAimTmp0;

    frqAimDisp = ABS_INT32(frqAimTmp);

    JumpFrqDeal();
}

void UpdateFrqAim(void)
{
    int32 frqDirRef;

    if (FORWARD_DIR != runCmd.bit.dir)
    {
        frqAimTmp = -frqAimTmp;
    }

    if (runFlag.bit.jog)
    {
        if (RUN_CMD_FWD_JOG == runCmd.bit.jog)
        {
            frqAimTmp = funcCode.code.jogFrq;
        }
        else if (RUN_CMD_REV_JOG == runCmd.bit.jog)
        {
            frqAimTmp = -(int16)funcCode.code.jogFrq;
        }

        frqAimDisp = funcCode.code.jogFrq;
    }

#if DEBUG_F_PLC_CTRL
    frqAimPLCDisp = (int16)(frqAimTmp * FRQ_PERCENT_SCALE / maxFrq);
#endif

    frqFlag.bit.frqSetLimit = 0;
    if (frqAimTmp >= upperFrq)
    {
        frqAimTmp = upperFrq;
        frqFlag.bit.frqSetLimit = 1;
    }
    else if (frqAimTmp <= -(int32)upperFrq)
    {
        frqAimTmp = -(int32)upperFrq;
        frqFlag.bit.frqSetLimit = 1;
    }

    frqAim = frqAimTmp;

    if (frqAim > 0)
    {
        runFlag.bit.dir = FORWARD_DIR;
    }
    else if (frqAim < 0)
    {
        runFlag.bit.dir = REVERSE_DIR;
    }
    else
    {
        runFlag.bit.dir = runCmd.bit.dir;
    }

    bAntiReverseRun = 0;
    if (funcCode.code.antiReverseRun)
    {
        if (frqAimTmp < 0)
        {
            frqAimDisp = 0;
            frqFlag.bit.frqSetLimit = 1;
        }

        if ((frqAimTmp < 0) || (frqAim < 0))
        {
            frqAim = 0;
            frqFlag.bit.frqSetLimit = 1;
            bAntiReverseRun = 1;
        }
    }

    frqDirRef = frq;

    runFlag.bit.dirFinal = runFlag.bit.dir;
    if (FUNCCODE_runDir_REVERSE == funcCode.code.runDir)
    {
        frqAim = -frqAim;
        frqDirRef = -frqDirRef;

        runFlag.bit.dirFinal = ~runFlag.bit.dirFinal;
    }

    if (frqDirRef > 0)
    {
        runFlag.bit.curDir = FORWARD_DIR;
    }
    else if (frqDirRef < 0)
    {
        runFlag.bit.curDir = REVERSE_DIR;
    }
    else if (runFlag.bit.run && ((int64)frqAimOld4Dir * frqAim < 0))
    {
        if (++reverseTicker >= (Uint32)funcCode.code.zeroSpeedDeadTime
            * (Uint16)(TIME_UNIT_ZERO_SPEED_DEAD / RUN_CTRL_PERIOD))
        {
            runFlag.bit.curDir = runFlag.bit.dir;
            frqAimOld4Dir = frqAim;
        }
    }
    else
    {
        runFlag.bit.curDir = runFlag.bit.dir;
        frqAimOld4Dir = frqAim;
    }

    if ((runFlag.bit.curDir != runFlag.bit.dir)

        )
    {
        runFlag.bit.dirReversing = 1;
    }
    else
    {
        runFlag.bit.dirReversing = 0;
        reverseTicker = 0;
    }
}

LOCALF int32 FrqAimUpDownDeal(void)
{
    int32 diStepFrq;
    Uint16 up = diFunc.f1.bit.up;
    Uint16 down = diFunc.f1.bit.down;
    int32 frqDelta = 0;
    static Uint16 bFrqDigitalOld;
    static int16 diStepRemainder;

    if ((up || down || accDecFrqPrcFlag)
        && (MENU0_DISP_STATUS_RUN_STOP == menu0DispStatus)
        && (!runFlag.bit.dirReversing)
        && ((diSelectFunc.f2.bit.frqOk&&diFunc.f2.bit.frqOk) || (!diSelectFunc.f2.bit.frqOk))
        && ((ERROR_LEVEL_RUN != errorAttribute.bit.level)
           || ((ERROR_LEVEL_RUN == errorAttribute.bit.level) && (funcCode.code.errorRunFrqSrc == ERR_RUN_FRQ_AIM)))
        )
    {
        bFrqDigital = 1;
    }
    else
    {
        bFrqDigital = 0;
    }

    if (bFrqDigitalOld != bFrqDigital)
    {
        if (bFrqDigital
            && runFlag.bit.common
            && (SWING_NONE == swingStatus)
            )
        {

            if(funcCode.code.updnBenchmark

              )
            {

            }
            else
            {

    			if ((ABS_INT32(frqAim) < lowerFrq) && (ABS_INT32(frq) <= lowerFrq))
    			{
                    if (runFlag.bit.curDir^runDirPanelOld)
                    {
                        upDownFrq = uPDownFrqLower2;
                    }
                    else
                    {
                        upDownFrq = uPDownFrqLower1;
                    }
    			}
    			else
                {

                    if ((frqFlag.bit.comp)
						&& ((((frqXy.x + frqXy.z) < 0)
                        	&& (funcCode.code.frqYSrc < FUNCCODE_frqXySrc_AI1))
						 || (((frqXy.y + frqXy.z + funcCode.code.presetFrq) < 0)
                        	&& (funcCode.code.frqXSrc < FUNCCODE_frqXySrc_AI1))
							)
                        )
        			{
    					upDownFrq += ABS_INT32(frqAim) - ABS_INT32(frq);
    				}

                    else
    				{
    					upDownFrq += ABS_INT32(frq) - ABS_INT32(frqAim);
    				}
                }
			}
        }

        if (!bFrqDigital)
        {
            bFrqDigitalDone4WaitDelay = 1;
        }

        diStepRemainder = 0;
    }

    if (bFrqDigital)
    {
        upDownFrqInit = 0;
        frqFlag.bit.upDownoperationStatus = UP_DN_OPERATION_ON;

        if (ACC_DEC_FRQ_WAIT == accDecFrqPrcFlag)
        {
            if (KEY_UP == keyFunc)
            {
                frqDelta = (int32)frqKeyUpDownDelta;
            }
            else
            {
                frqDelta = -(int32)frqKeyUpDownDelta;
            }

            accDecFrqPrcFlag = ACC_DEC_FRQ_DONE;
        }
        else if ((up || down)
                && (!(up && down) && (!frqFlag.bit.upDown))
                )
        {
            int32 diUpDownSlope = funcCode.code.diUpDownSlope;

            if (down)
            {
                diUpDownSlope = -diUpDownSlope;
            }

            diStepFrq = (diUpDownSlope * FRQ_SRC_PERIOD + diStepRemainder) / FRQ_SLOPE_SCALE;
            diStepRemainder = (diUpDownSlope * FRQ_SRC_PERIOD + diStepRemainder) % FRQ_SLOPE_SCALE;

            frqDelta = diStepFrq;
        }
    }

    frqFlag.bit.upDown = 1;
    upDownFrq += frqDelta;

    if (frqFlag.bit.upDownoperationStatus == UP_DN_OPERATION_OFF)
    {
        upDownFrq = (int16)upDownFrqInit;
    }

    if (upDownFrq > uPDownFrqMax)
    {
        upDownFrq = uPDownFrqMax;
    }
    else if (upDownFrq < uPDownFrqMin)
    {
        upDownFrq = uPDownFrqMin;
    }

#if 1
    if ((diFunc.f1.bit.clearUpDownFrq)

        )
    {
        upDownFrq = 0;
        frqFlag.bit.upDownoperationStatus = UP_DN_OPERATION_OFF;
    }
#endif

    bFrqDigitalOld = bFrqDigital;

    if ((frqFlag.bit.comp) && (funcCode.code.frqYSrc < FUNCCODE_frqXySrc_AI1))
    {

        return upDownFrq;
    }
    else
    {

        return (upDownFrq + (int32)funcCode.code.presetFrq);
    }
}

LOCALF int32 FrqPlcSetDeal(void)
{
#if DEBUG_F_MULTI_SET_PLC
    static Uint16 plcEndLoopDelayTicker;
    static Uint16 plcTimeTicker;
    static Uint16 plcFlag;
    static Uint16 plcStepEnable;
    static Uint16 bPlcEndOneLoopEnable = 1;
    Uint32 plcTimeMax = 0;
    Uint16 loop = 0;
    Uint16 plcStepClamped;
    int32 frqPlcResult;

    runFlag.bit.plc = 1;
    bStopPlc = 0;

#define PLC_TIME_TICKER     5

    if (((RUN_STATUS_START == runStatus) && (START_RUN_STATUS_HOLD_START_FRQ == startRunStatus))
        || (RUN_STATUS_NORMAL == runStatus)
        )
    {
        if ((!frqPlcType)
            && (++plcTimeTicker >= PLC_TIME_TICKER))
        {
            plcTimeTicker = 0;
            plcTime++;
        }

        plcFlag = 1;
    }
    else if (!runFlag.bit.run)
    {

        if (((FUNCCODE_plcPowerOffRemMode_REM != (funcCode.code.plcPowerOffRemMode%10))
             || (plcFlag))
             && (FUNCCODE_plcStopRemMode_REM != (funcCode.code.plcPowerOffRemMode/10))
            && (!runCmd.bit.pause)
            )
        {
		    plcStep = 0;
		    plcTime = 0;
        }
        else
        {
            if (plcStep >= PLC_STEP_MAX)
            {
                plcStepRemOld = 0;
                plcTimeRemOld = 0;
            }
            plcStep = plcStepRemOld;
            plcTime = plcTimeRemOld;
        }
    }

    if (!plcStep)
    {
        bPlcEndOneLoopEnable = 1;
    }

    if (plcStep < PLC_STEP_MAX)
    {
        plcTimeMax = (Uint32)funcCode.code.plcAttribute[plcStep].runTime
            * ((TIME_UNIT_PLC / PLC_TIME_TICKER) / FRQ_SRC_PERIOD);
        if (FUNCCODE_plcTimeUnit_H == funcCode.code.plcTimeUnit)
            plcTimeMax *= TIME_UNIT_SEC_PER_HOUR;
    }
#undef PLC_TIME_TICKER

    if (plcTime >= plcTimeMax)
    {
        Uint16 bEndSearch = 0;

        plcTime = 0;
        plcTimeTicker = 0;

        for (;;)
        {
            if (++plcStep >= PLC_STEP_MAX)
            {
                plcStep = PLC_STEP_MAX;

                if (FUNCCODE_plcRunMode_REPEAT == funcCode.code.plcRunMode)
                {
                    plcStep = 0;
                }

                if (FUNCCODE_plcRunMode_ONCE_RUN == funcCode.code.plcRunMode)
                {
                    plcStep = plcStepEnable;
					bEndSearch = 1;
                }

                if ((plcStepOld < PLC_STEP_MAX) && (runFlag.bit.common) && (!runFlag.bit.jog))
                {
                    plcEndLoopDelayTicker = 0;
                    if (bPlcEndOneLoopEnable)
                    {
                        bPlcEndOneLoop = 1;
                        bPlcEndOneLoopEnable = 0;
                    }
                }
            }

            if (plcStep < PLC_STEP_MAX)
            {
                if (funcCode.code.plcAttribute[plcStep].runTime)
                {
                    bEndSearch = 1;
                }
            }
            else
            {
                bEndSearch = 1;
            }

            if (++loop > PLC_STEP_MAX)
            {
                bStopPlc = 1;
                bEndSearch = 1;
            }

            if (bEndSearch)
            {
                break;
            }
        }
    }

    if (plcStep >= PLC_STEP_MAX)
    {
        if (FUNCCODE_plcRunMode_ONCE_STOP == funcCode.code.plcRunMode)
        {
            bStopPlc = 1;

            if (!runFlag.bit.common)
            {
                plcStep = 0;
            }
        }

    }

    plcStepClamped = plcStep;
    plcStepEnable = plcStep;
    if (plcStepClamped >= PLC_STEP_MAX)
    {
        plcStepClamped = PLC_STEP_MAX - 1;
    }

    accDecTimeSrcPlc = funcCode.code.plcAttribute[plcStepClamped].accDecTimeSet;

    frqPlcResult = UpdateMultiSetFrq(plcStepClamped);

    if (bPlcEndOneLoop)
    {
        if (++plcEndLoopDelayTicker >= PLC_END_LOOP_TIME_MAX / FRQ_SRC_PERIOD)
        {

            bPlcEndOneLoop = 0;
        }
    }

    frqPlcType = 1;

    return frqPlcResult;
#else
    return 0;
#endif
}

LOCALF int32 UpdateMultiSetFrq(Uint16 step)
{
#if DEBUG_F_MULTI_SET_PLC
    static Uint16 stepOld;
    int32 frqPlcResult;
    int16 aiIndex;

    if (!step)
    {
        switch (funcCode.code.plcFrq0Src)
        {
            case FUNCCODE_plcFrq0Src_FC:
                frqPlcResult = (int32)(int16)funcCode.code.plcFrq[0] * maxFrq / FRQ_PLC_SET_SCALE;
                break;

            case FUNCCODE_plcFrq0Src_AI1:
            case FUNCCODE_plcFrq0Src_AI2:
            case FUNCCODE_plcFrq0Src_AI3:
                aiIndex = funcCode.code.plcFrq0Src - FUNCCODE_plcFrq0Src_AI1;
                frqPlcResult = ((int32)aiDeal[aiIndex].set * maxFrq + FRQ_Q15_ROUND) >> FRQ_Q15_SHIFT;
                break;

            case FUNCCODE_plcFrq0Src_PULSE:
                frqPlcResult = ((int32)pulseInSet * maxFrq + FRQ_Q15_ROUND) >> FRQ_Q15_SHIFT;
                break;

            case FUNCCODE_plcFrq0Src_PID:
                frqPlcResult = FrqPidSetDeal();
                break;

            case FUNCCODE_plcFrq0Src_PRESET_FRQ:
                if (stepOld)
                {
                    frqDigitalTmp = frqDigitalPlc;
                }

                frqPlcResult = FrqAimUpDownDeal();
                break;

            default:
                break;
        }
    }
    else
    {
        if (!stepOld)
        {
            frqDigitalPlc = frqDigitalTmp;
        }

        frqPlcResult = (int32)(int16)funcCode.code.plcFrq[step] * maxFrq / FRQ_PLC_SET_SCALE;
    }

    stepOld = step;

    return frqPlcResult;
#endif
}

LOCALF void UpdateLimitFrq(void)
{
    int32 upperFrqCalc;
    Uint16 aiIndex;

    maxFrq = funcCode.code.maxFrq;
    frqPuQ15 = maxFrq + FRQ_PU_HEADROOM_STEPS * decNumber[funcCode.code.frqPoint];

    upperFrqCalc = funcCode.code.upperFrq;
    switch (funcCode.code.upperFrqSrc)
    {
        case FUNCCODE_upperFrqSrc_FC:
            break;

        case FUNCCODE_upperFrqSrc_AI1:
        case FUNCCODE_upperFrqSrc_AI2:
        case FUNCCODE_upperFrqSrc_AI3:
            aiIndex = funcCode.code.upperFrqSrc - FUNCCODE_upperFrqSrc_AI1;
            upperFrqCalc = ((int32)aiDeal[aiIndex].set * maxFrq + FRQ_Q15_ROUND) >> FRQ_Q15_SHIFT;
            upperFrqCalc += funcCode.code.upperFrqOffset;
            break;

        case FUNCCODE_upperFrqSrc_PULSE:
            upperFrqCalc = ((int32)pulseInSet * maxFrq + FRQ_Q15_ROUND) >> FRQ_Q15_SHIFT;
            upperFrqCalc += funcCode.code.upperFrqOffset;
            break;

        case FUNCCODE_upperFrqSrc_COMM:

            upperFrqCalc = ((int32)(int16)funcCode.code.frqComm * maxFrq + FRQ_PERCENT_ROUND) / FRQ_PERCENT_SCALE;
            break;

        default:
            break;
    }

    if (upperFrqCalc < 0L)
        upperFrqCalc = 0;
    if (upperFrqCalc > (int32)maxFrq)
        upperFrqCalc = maxFrq;

    upperFrq = upperFrqCalc;

    lowerFrq = funcCode.code.lowerFrq;

    if (upperFrq < lowerFrq)
    {
        upperFrq = lowerFrq;
    }
}

LOCALF void JumpFrqDeal(void)
{
#if DEBUG_F_JUMP_FRQ
    int32 frqAimAbs = ABS_INT32(frqAimTmp);
    int32 frqAbs = ABS_INT32(frqTmp);

    jumpFrq1.frq = funcCode.code.jumpFrq1;
    jumpFrq1.range = funcCode.code.jumpFrqRange;
    JumpFrqEndCalc(&jumpFrq1);

    jumpFrq2.frq = funcCode.code.jumpFrq2;
    jumpFrq2.range = funcCode.code.jumpFrqRange;
    JumpFrqEndCalc(&jumpFrq2);

    if ((jumpFrq2.low <= jumpFrq1.low) && (jumpFrq1.low <= jumpFrq2.high))
    {
        jumpFrq1.low = jumpFrq2.low;
    }
    if ((jumpFrq1.low <= jumpFrq2.low) && (jumpFrq2.low <= jumpFrq1.high))
    {
        jumpFrq2.low = jumpFrq1.low;
    }
    if ((jumpFrq2.low <= jumpFrq1.high) && (jumpFrq1.high <= jumpFrq2.high))
    {
        jumpFrq1.high = jumpFrq2.high;
    }
    if ((jumpFrq1.low <= jumpFrq2.high) && (jumpFrq2.high <= jumpFrq1.high))
    {
        jumpFrq2.high = jumpFrq1.high;
    }

    if ((jumpFrq1.low < frqAimAbs) && (frqAimAbs < jumpFrq1.high))
    {

        if ((frqAimAbs < ABS_INT32(frq)) && (frqAimAbs < ABS_INT32(frqAim)))
        {
            frqAimAbs = jumpFrq1.high;
        }
        else
        {
            frqAimAbs = jumpFrq1.low;
        }

    }
    if ((jumpFrq2.low < frqAimAbs) && (frqAimAbs < jumpFrq2.high))
    {

        if ((frqAimAbs < ABS_INT32(frq)) && (frqAimAbs < ABS_INT32(frqAim)))
        {
            frqAimAbs = jumpFrq2.high;
        }
        else
        {
            frqAimAbs = jumpFrq2.low;
        }
    }

    if (frqAimAbs > (int32)upperFrq)
    {
        frqAimAbs = (int32)upperFrq;
    }
    frqAimTmp = (frqAimTmp >= 0) ? (frqAimAbs) : (-frqAimAbs);

    if (funcCode.code.jumpFrqMode)
    {
        if ((jumpFrq1.low < frqAbs) && (frqAbs < jumpFrq1.high))
        {
            if((ABS_INT16(frqAimTmp) >= ABS_INT16(frqTmp))
                    && (runFlag.bit.run)
                    && (runCmd.bit.common)
                )
            {
                frqAbs = jumpFrq1.high;
            }
            else
            {
                frqAbs = jumpFrq1.low;
            }
        }
        if ((jumpFrq2.low < frqAbs) && (frqAbs < jumpFrq2.high))
        {
            if((ABS_INT16(frqAimTmp) >= ABS_INT16(frqTmp))
                && (runFlag.bit.run)
                && (runCmd.bit.common)
            )
            {
                frqAbs = jumpFrq2.high;
            }
            else
            {
                frqAbs = jumpFrq2.low;
            }
        }

        if (frqAbs > (int32)upperFrq)
        {
            frqAbs = (int32)upperFrq;
        }

        frqTmp = (frqTmp >= 0) ? (frqAbs) : (-frqAbs);
    }
#endif
}

#define JUMP_FRQ_RANGE_ALL  0
#define JUMP_FRQ_RANGE_HALF 1
#define JUMP_FRQ_RANGE  JUMP_FRQ_RANGE_ALL

LOCALF void JumpFrqEndCalc(JUMP_FRQ *p)
{
#if DEBUG_F_JUMP_FRQ
    int32 lowFrq;

#if 0
    if (0 == p->frq)
    {
        p->low = 0;
        p->high = 0;
        return;
    }
#endif

    lowFrq = (int32)p->frq - ((int32)p->range >> JUMP_FRQ_RANGE);
    if (lowFrq < 0)
        lowFrq = 0;
    p->low = lowFrq;

    p->high = (int32)p->frq + ((int32)p->range >> JUMP_FRQ_RANGE);
#endif
}
#undef JUMP_FRQ_RANGE_ALL
#undef JUMP_FRQ_RANGE_HALF
#undef JUMP_FRQ_RANGE

void UpdateRunMode(void)
{
    static Uint16 runModeOld;

#if (DEBUG_F_POSITION_CTRL)
    if ((FUNCCODE_posCtrl_POSITION_CTRL == funcCode.code.posCtrl)
        || ((FUNCCODE_posCtrl_SWITCH_TO_PC == funcCode.code.posCtrl)
            && (diFunc.f3.bit.posCtrl))
        || ((FUNCCODE_posCtrl_SWITCH_FROM_PC == funcCode.code.posCtrl)
            && (!diFunc.f3.bit.posCtrl))
        )
    {
        if (FUNCCODE_motorCtrlMode_FVC == motorFc.motorCtrlMode)
        {
            runMode = RUN_MODE_POSITION_CTRL;
        }
        else if (runFlag.bit.run)
        {
            errorOther = 98;
        }
    }
    else
#endif
    {

        runMode = funcCode.code.torqueCtrl ^ diFunc.f2.bit.SpdTorqSwitch;
        if (FUNCCODE_motorCtrlMode_VF == motorFc.motorCtrlMode)
        {
            runMode = RUN_MODE_SPEED_CTRL;
        }

        runMode = runMode & (~diFunc.f1.bit.forbidTorqueCtrl);

        if (RUN_MODE_TORQUE_CTRL == runMode)
        {

			TorqueCalc();

            if (torqueAim >= 0)
            {
                frqAimTmp0 = funcCode.code.torqueCtrlFwdMaxFrq;
            }
            else
            {
                frqAimTmp0 = -(int32)funcCode.code.torqueCtrlRevMaxFrq;
            }
        }

        else if (RUN_MODE_TORQUE_CTRL == runModeOld)
        {
            frqTmp = frqRun;
        }
    }

    runModeOld = runMode;
}

int32 FrqSrcOperate(void)
{
    int32 frqResult;
    Uint16 digit[5];
    GetNumberDigit1(digit, funcCode.code.frqRunCmdBind);
    runCmdBind = digit[runSrc];

    if (runCmdBind > 0)
    {
        frqAiPu = maxFrq;
        frqFlag.bit.comp = 0;
        frqResult = FrqXyCalc(runCmdBind);
        frqXDisp = 0;
        frqYDisp = 0;
    }

    else
    {
        frqResult = GetFrq();

        frqXDisp = ABS_INT16(frqXy.x);
        frqYDisp = ABS_INT16(frqXy.y);
    }
    return frqResult;
}

#define UP_DOWN_LIMIT_X 0
#define UP_DOWN_LIMIT_Y 1
#define UP_DOWN_LIMIT_NO_COMP 0
#define UP_DOWN_LIMIT_COMP 1
int32 frqMax;
int32 frqMin;
void getUpDownLimit(Uint16 comp, Uint16 type)
{
    int32 baseFrq;
    int32 compMinFrq;
    int32 puRangeFrq;
    int32 minFrq;
    minFrq = frqMin;

    if (comp == UP_DOWN_LIMIT_COMP)
    {

        if (type == UP_DOWN_LIMIT_Y)
        {

            baseFrq = frqXy.x + frqXy.z;
        }

        else if (type == UP_DOWN_LIMIT_X)
        {
            baseFrq = frqXy.y + frqXy.z + (int32)funcCode.code.presetFrq;

            {
                compMinFrq = 0 + frqXy.y + frqXy.z;
                if (compMinFrq > minFrq)
                {
                    minFrq = compMinFrq;
                }
            }
            #if 0
            else
            {
                max = 0 + frqXy.y - frqXy.z;
                if (max < frqMax)
                {
                    frqMax = max;
                }
            }
            #endif

        }
    }
    else
    {
        baseFrq = frqXy.z + (int32)funcCode.code.presetFrq;
        minFrq = frqXy.z;
    }

    if (minFrq > frqMax)
    {
        minFrq = frqMax;
    }

    uPDownFrqMax = frqMax - baseFrq;
    uPDownFrqMin = minFrq - baseFrq;
    uPDownFrqLower1 = lowerFrq - baseFrq;
    uPDownFrqLower2 = (int16)(-lowerFrq) - baseFrq;

    if ((comp == UP_DOWN_LIMIT_COMP) && (type == UP_DOWN_LIMIT_Y))
    {

        if (funcCode.code.frqYRangeBase)
        {
            puRangeFrq  = ((Uint32)ABS_INT32(frqXy.x)*funcCode.code.frqYRange) / FRQ_PERCENT;
        }
        else
        {
            puRangeFrq = ((Uint32)maxFrq*funcCode.code.frqYRange) / FRQ_PERCENT;
        }

        if (uPDownFrqMax > puRangeFrq)
        {
            uPDownFrqMax = puRangeFrq;
        }

        if (uPDownFrqMin < (-puRangeFrq))
        {
            uPDownFrqMin = -puRangeFrq;
        }
    }
}

void ResetUpDownFrq(void)
{
    frqFlag.bit.upDownoperationStatus = UP_DN_OPERATION_OFF;
    upDownFrq = 0;
    uPDownFrqMax = 0;
    uPDownFrqMin = 0;
    uPDownFrqLower1= 0;
    uPDownFrqLower2= 0;

}

int32 GetFrq(void)
{
    int32 frqResult;

    frqMax = upperFrq;
    frqMin = -(int32)upperFrq;

#if 0
    if ((upperFrqBak != upperFrqBak)
        || (lowerFrqBak != lowerFrq)
        )
    {
        ResetUpDownFrq();
    }

	upperFrqBak = upperFrq;
	lowerFrqBak = lowerFrq;
#endif

    frqFlag.bit.comp = 0;
    frqFlag.bit.upDown = 0;
    frqFlag.bit.x = 0;
    frqFlag.bit.y = 0;
    frqXy.x = 0;
    frqXy.y = 0;
    frqXy.z = 0;

    switch (funcCode.code.frqCalcSrc % FRQ_SRC_SELECT_DIVISOR)
    {
        case FUNCCODE_frqCalcSrc_X:
            frqFlag.bit.x = 1;

            break;

        case FUNCCODE_frqCalcSrc_COMPOSE:
            frqFlag.bit.x = 1;
            frqFlag.bit.y = 1;

            break;

        case FUNCCODE_frqCalcSrc_X_OR_Y:
            if (!diFunc.f1.bit.frqSrcSwitch)
            {
                frqFlag.bit.x = 1;
            }
            else
            {
                frqFlag.bit.y = 1;
            }

            break;

        case FUNCCODE_frqCalcSrc_X_OR_COMPOSE:
            if (!diFunc.f1.bit.frqSrcSwitch)
            {
                frqFlag.bit.x = 1;
            }
            else
            {
                frqFlag.bit.x = 1;
                frqFlag.bit.y = 1;
            }

            break;

        case FUNCCODE_frqCalcSrc_Y_OR_COMPOSE:
            if (!diFunc.f1.bit.frqSrcSwitch)
            {
                frqFlag.bit.y = 1;
            }
            else
            {
                frqFlag.bit.x = 1;
                frqFlag.bit.y = 1;
            }
            break;

        default:
            break;
    }

    if (frqFlag.bit.x && frqFlag.bit.y)
    {
        frqXy.z = funcCode.code.frqYOffsetFc;

        if ((funcCode.code.frqCalcSrc / FRQ_SRC_SELECT_DIVISOR) == 0)
        {
            if ((funcCode.code.frqXSrc >= FUNCCODE_frqXySrc_AI1) ||
                (funcCode.code.frqYSrc >= FUNCCODE_frqXySrc_AI1))
            {
                frqFlag.bit.comp = 1;
            }
        }
    }

    if (frqFlag.bit.comp)
    {

        if (funcCode.code.frqXSrc < FUNCCODE_frqXySrc_AI1)
        {
            GetFrqY();
            getUpDownLimit(UP_DOWN_LIMIT_COMP, UP_DOWN_LIMIT_X);
            GetFrqX();
        }

        else if (funcCode.code.frqYSrc < FUNCCODE_frqXySrc_AI1)
        {
            GetFrqX();
            getUpDownLimit(UP_DOWN_LIMIT_COMP, UP_DOWN_LIMIT_Y);
            GetFrqY();
        }

        else
        {
            GetFrqX();
            GetFrqY();
        }
    }

    else
    {

        if (frqFlag.bit.x)
        {
            if (funcCode.code.frqXSrc < FUNCCODE_frqXySrc_AI1)
            {
                getUpDownLimit(UP_DOWN_LIMIT_NO_COMP, UP_DOWN_LIMIT_X);
            }
            GetFrqX();
        }

        if (frqFlag.bit.y)
        {

            if (funcCode.code.frqYRangeBase)
            {
                GetFrqX();
            }

            if (funcCode.code.frqYSrc < FUNCCODE_frqXySrc_AI1)
            {
                getUpDownLimit(UP_DOWN_LIMIT_NO_COMP, UP_DOWN_LIMIT_Y);
            }

            GetFrqY();
        }
    }

    if (frqFlag.bit.x && frqFlag.bit.y)
    {
        frqResult = GetFrqComp();
    }
    else if (frqFlag.bit.x)
    {
        frqResult = frqXy.x;
    }
    else if (frqFlag.bit.y)
    {
        frqResult = frqXy.y;
    }

    if (frqResult > frqMax)
    {
        frqResult = frqMax;
    }
    else if (frqResult < frqMin)
    {
        frqResult = frqMin;
    }

    if (frqFlag.bit.upDown)
    {
        if(!((funcCode.code.frqXSrc < FUNCCODE_frqXySrc_AI1) ||
            (funcCode.code.frqYSrc < FUNCCODE_frqXySrc_AI1)
            ))
        {
            frqFlag.bit.fcPosLimit = 0;
            frqFlag.bit.fcNegLimit = 0;
            frqFlag.bit.upDownoperationStatus = UP_DN_OPERATION_OFF;
            upDownFrqTmp = 0;
            upDownFrq = 0;

        }
    }
    return frqResult;
}

int32 GetFrqComp(void)
{
#if DEBUG_F_FRQ_SRC_COMPOSE
    int32 frqResult;
    int32 frqXComp;
    int32 frqYComp;
    int32 frqYCompMax;

    if (funcCode.code.frqYRangeBase)
    {
        frqYCompMax  = ((Uint32)ABS_INT32(frqXy.x)*funcCode.code.frqYRange) / FRQ_PERCENT;
    }
    else
    {
        frqYCompMax = ((Uint32)maxFrq*funcCode.code.frqYRange) / FRQ_PERCENT;
    }

    if (frqXy.y > frqYCompMax)
    {
        frqXy.y = frqYCompMax;
    }

    switch (funcCode.code.frqCalcSrc / FRQ_SRC_SELECT_DIVISOR)
    {
        case FUNCCODE_frqCalcSrc_ADD:
            frqXComp = frqXy.x;

            if ((funcCode.code.frqXSrc < FUNCCODE_frqXySrc_AI1) &&
                (funcCode.code.frqYSrc < FUNCCODE_frqXySrc_AI1))
            {
                frqXy.y = 0;
            }
            frqYComp = frqXy.y;

            frqResult = frqXComp + frqYComp;

#if 0
            if (frq > upperFrq)
            {
                frq = upperFrq;
                frqXy.y = upperFrq - x - z;
            }
            else if (frq < -(int32)upperFrq)
            {
                frq = -(int32)upperFrq;
                frqXy.y = -(int32)upperFrq - x - z;
            }
#endif
            break;

        case FUNCCODE_frqCalcSrc_SUBTRATION:
            frqXComp = frqXy.x;
            frqYComp = frqXy.y;

            frqResult = frqXComp - frqYComp;

            break;

        case FUNCCODE_frqCalcSrc_MAX:
            frqXComp = frqXy.x;
            frqYComp = frqXy.y;

            if (ABS_INT32(frqXComp) >= ABS_INT32(frqYComp))
            {
                frqResult = frqXComp;
            }
            else
            {
                frqResult = frqYComp;
            }

            break;

        case FUNCCODE_frqCalcSrc_MIN:
            frqXComp = frqXy.x;
            frqYComp = frqXy.y;

            if (ABS_INT32(frqXComp) < ABS_INT32(frqYComp))
            {
                frqResult = frqXComp;
            }
            else
            {
                frqResult = frqYComp;
            }

            break;

        default:
            break;
    }

    frqResult += frqXy.z;

#if 0
    if (frq > upperFrq)
    {
        frq = upperFrq;
    }
    else if (frq < -(int32)upperFrq)
    {
        frq = -(int32)upperFrq;
    }
#endif

    return frqResult;
#endif
}

int32 GetFrqX(void)
{
    int32 frqResult;

    frqAiPu = maxFrq;

    frqResult = diFunc.f2.bit.frqXSrc2Preset
              ? (int32)funcCode.code.presetFrq
              : FrqXyCalc(funcCode.code.frqXSrc);

    frqXy.x = frqResult;

    return frqResult;
}

int32 GetFrqY(void)
{
#if DEBUG_F_FRQ_SRC_COMPOSE
    int32 frqResult;

    if (frqFlag.bit.comp)
    {

        if (FUNCCODE_frqYRangeBase_FRQ_X == funcCode.code.frqYRangeBase)
        {

            if ((funcCode.code.frqYSrc >= FUNCCODE_frqXySrc_AI1)
                && (funcCode.code.frqXSrc < FUNCCODE_frqXySrc_AI1)
                )
            {
                frqAiPu = funcCode.code.presetFrq;
            }
            else
            {
                frqAiPu = ABS_INT32(frqXy.x);
            }
        }

        else
        {
            frqAiPu = maxFrq;
        }

        frqAiPu = ((Uint32)frqAiPu * funcCode.code.frqYRange) / FRQ_PERCENT;
    }

    else
    {
        frqAiPu = maxFrq;
    }

    frqResult = diFunc.f2.bit.frqYSrc2Preset
              ? (int32)funcCode.code.presetFrq
              : FrqXyCalc(funcCode.code.frqYSrc);

    if (frqFlag.bit.comp)
    {
        if (frqResult > frqAiPu)
        {
            frqResult = frqAiPu;
        }
        else if (frqResult < -frqAiPu)
        {
            frqResult = - frqAiPu;
        }
    }

    frqXy.y = frqResult;

    return frqResult;
#endif
}

LOCALF int32 FrqXyCalc(Uint16 src)
{
    int32 frqSrcValue = 0;

    switch (src)
    {
        case FUNCCODE_frqXySrc_FC:
        case FUNCCODE_frqXySrc_FC_P_OFF_REM:
            frqSrcValue = FrqAimUpDownDeal();
            break;

        case FUNCCODE_frqXySrc_AI1:
        case FUNCCODE_frqXySrc_AI2:
        case FUNCCODE_frqXySrc_AI3:
            frqSrcValue = src - FUNCCODE_frqXySrc_AI1;
            frqSrcValue = ((int32)aiDeal[frqSrcValue].set * frqAiPu + FRQ_Q15_ROUND) >> FRQ_Q15_SHIFT;
            break;

        case FUNCCODE_frqXySrc_PULSE:
            frqSrcValue = ((int32)pulseInSet * frqAiPu + FRQ_Q15_ROUND) >> FRQ_Q15_SHIFT;
            break;

        case FUNCCODE_frqXySrc_MULTI_SET:
            frqSrcValue = UpdateMultiSetFrq(diFunc.f1.bit.multiSet);
            break;

        case FUNCCODE_frqXySrc_PLC:
            frqSrcValue = FrqPlcSetDeal();
            break;

        case FUNCCODE_frqXySrc_PID:
            frqSrcValue = FrqPidSetDeal();
            break;

        case FUNCCODE_frqXySrc_COMM:

#if DEBUG_F_PLC_CTRL
            if (funcCode.code.plcEnable)
            {
                frqSrcValue = (int32)(int16)funcCode.code.plcFrqSet * maxFrq / FRQ_PERCENT_SCALE;
            }
            else
#endif
#if DEBUG_F_P2P_CTRL
            if ((CanRxTxCon == P2P_COMM_SLAVE)
                && (funcCode.code.p2pRevDataSel == P2P_REV_FRQ_SET)
                )
            {
                frqSrcValue = ((int32)(int16)p2pData.processValue * maxFrq) / FRQ_PERCENT_SCALE;
            }
            else
#endif
            if (funcCode.code.commProtocolSec == PROFIBUS)
            {
                frqSrcValue = (int32)(Uint16)dpFrqAim;
            }
            else
            {
                frqSrcValue = (int32)(int16)funcCode.code.frqComm * maxFrq / FRQ_PERCENT_SCALE;
            }
            break;

        default:
            break;
    }

    return frqSrcValue;
}
