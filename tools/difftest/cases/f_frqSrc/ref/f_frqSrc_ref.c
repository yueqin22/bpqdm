
#include "f_frqSrc.h"
#include "f_main.h"
#include "f_runSrc.h"
#include "f_io.h"
#include "f_menu.h"
#include "f_ui.h"
#include "f_p2p.h"
#include "f_comm.h"

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

    if ((!runFlag.bit.jog)

        && (RUN_MODE_TORQUE_CTRL != runMode)
        )
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
    int32 frq4Dir;

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
    frqAimPLCDisp = (int16)(frqAimTmp*10000/maxFrq);
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

    frq4Dir = frq;

    runFlag.bit.dirFinal = runFlag.bit.dir;
    if (FUNCCODE_runDir_REVERSE == funcCode.code.runDir)
    {
        frqAim = -frqAim;
        frq4Dir = -frq4Dir;

        runFlag.bit.dirFinal = ~runFlag.bit.dirFinal;
    }

    if (frq4Dir > 0)
    {
        runFlag.bit.curDir = FORWARD_DIR;
    }
    else if (frq4Dir < 0)
    {
        runFlag.bit.curDir = REVERSE_DIR;
    }
    else
    {
        if (runFlag.bit.run
            && ((int64)frqAimOld4Dir * frqAim < 0)
            )
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

            {
                frqAimOld4Dir = frqAim;
            }
        }
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
    int32 frqDeltaDi;
    Uint16 up = diFunc.f1.bit.up;
    Uint16 down = diFunc.f1.bit.down;
    int32 delta = 0;
    static Uint16 bFrqDigitalOld;
    static int16 frqDeltaDiRemainder;

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

        frqDeltaDiRemainder = 0;
    }

    if (bFrqDigital)
    {
        upDownFrqInit = 0;
        frqFlag.bit.upDownoperationStatus = UP_DN_OPERATION_ON;

        if (ACC_DEC_FRQ_WAIT == accDecFrqPrcFlag)
        {
            if (KEY_UP == keyFunc)
            {
                delta = (int32)frqKeyUpDownDelta;
            }
            else
            {
                delta = -(int32)frqKeyUpDownDelta;
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

            frqDeltaDi = (diUpDownSlope * FRQ_SRC_PERIOD + frqDeltaDiRemainder) / (10000);
            frqDeltaDiRemainder = (diUpDownSlope * FRQ_SRC_PERIOD + frqDeltaDiRemainder) % (10000);

            delta = frqDeltaDi;
        }
    }

    frqFlag.bit.upDown = 1;
    upDownFrq += delta;

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
    Uint16 plcStep1;
    int32 frqPlc;

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

    plcStep1 = plcStep;
    plcStepEnable = plcStep;
    if (plcStep1 >= PLC_STEP_MAX)
    {
        plcStep1 = PLC_STEP_MAX - 1;
    }

    accDecTimeSrcPlc = funcCode.code.plcAttribute[plcStep1].accDecTimeSet;

    frqPlc = UpdateMultiSetFrq(plcStep1);

    if (bPlcEndOneLoop)
    {
        if (++plcEndLoopDelayTicker >= PLC_END_LOOP_TIME_MAX / FRQ_SRC_PERIOD)
        {

            bPlcEndOneLoop = 0;
        }
    }

    frqPlcType = 1;

    return frqPlc;
#else
    return 0;
#endif
}

LOCALF int32 UpdateMultiSetFrq(Uint16 step)
{
#if DEBUG_F_MULTI_SET_PLC
    static Uint16 stepOld;
    int32 frqPlc;
    int16 i;

    if (!step)
    {
        switch (funcCode.code.plcFrq0Src)
        {
            case FUNCCODE_plcFrq0Src_FC:
                frqPlc = (int32)(int16)funcCode.code.plcFrq[0] * maxFrq / 1000;
                break;

            case FUNCCODE_plcFrq0Src_AI1:
            case FUNCCODE_plcFrq0Src_AI2:
            case FUNCCODE_plcFrq0Src_AI3:
                i = funcCode.code.plcFrq0Src - FUNCCODE_plcFrq0Src_AI1;
                frqPlc = ((int32)aiDeal[i].set * maxFrq + (1 << 14)) >> 15;
                break;

            case FUNCCODE_plcFrq0Src_PULSE:
                frqPlc = ((int32)pulseInSet * maxFrq + (1 << 14)) >> 15;
                break;

            case FUNCCODE_plcFrq0Src_PID:
                frqPlc = FrqPidSetDeal();
                break;

            case FUNCCODE_plcFrq0Src_PRESET_FRQ:
                if (stepOld)
                {
                    frqDigitalTmp = frqDigitalPlc;
                }

                frqPlc = FrqAimUpDownDeal();
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

        frqPlc = (int32)(int16)funcCode.code.plcFrq[step] * maxFrq / 1000;
    }

    stepOld = step;

    return frqPlc;
#endif
}

LOCALF void UpdateLimitFrq(void)
{
    int32 tmp;
    Uint16 i;

    maxFrq = funcCode.code.maxFrq;
    frqPuQ15 = maxFrq + 20 * decNumber[funcCode.code.frqPoint];

    tmp = funcCode.code.upperFrq;
    switch (funcCode.code.upperFrqSrc)
    {
        case FUNCCODE_upperFrqSrc_FC:
            break;

        case FUNCCODE_upperFrqSrc_AI1:
        case FUNCCODE_upperFrqSrc_AI2:
        case FUNCCODE_upperFrqSrc_AI3:
            i = funcCode.code.upperFrqSrc - FUNCCODE_upperFrqSrc_AI1;
            tmp = ((int32)aiDeal[i].set * maxFrq + (1<<14)) >> 15;
            tmp += funcCode.code.upperFrqOffset;
            break;

        case FUNCCODE_upperFrqSrc_PULSE:
            tmp = ((int32)pulseInSet * maxFrq + (1<<14)) >> 15;
            tmp += funcCode.code.upperFrqOffset;
            break;

        case FUNCCODE_upperFrqSrc_COMM:

            tmp = ((int32)(int16)funcCode.code.frqComm * maxFrq + 5000) / 10000;
            break;

        default:
            break;
    }

    if (tmp < 0L)
        tmp = 0;
    if (tmp > (int32)maxFrq)
        tmp = maxFrq;

    upperFrq = tmp;

    lowerFrq = funcCode.code.lowerFrq;

    if (upperFrq < lowerFrq)
    {
        upperFrq = lowerFrq;
    }
}

LOCALF void JumpFrqDeal(void)
{
#if DEBUG_F_JUMP_FRQ
    int32 tmp = ABS_INT32(frqAimTmp);
    int32 tmp1 = ABS_INT32(frqTmp);

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

    if ((jumpFrq1.low < tmp) && (tmp < jumpFrq1.high))
    {

        if ((tmp < ABS_INT32(frq)) && (tmp < ABS_INT32(frqAim)))
        {
            tmp = jumpFrq1.high;
        }
        else
        {
            tmp = jumpFrq1.low;
        }

    }
    if ((jumpFrq2.low < tmp) && (tmp < jumpFrq2.high))
    {

        if ((tmp < ABS_INT32(frq)) && (tmp < ABS_INT32(frqAim)))
        {
            tmp = jumpFrq2.high;
        }
        else
        {
            tmp = jumpFrq2.low;
        }
    }

    if (tmp > (int32)upperFrq)
    {
        tmp = (int32)upperFrq;
    }
    frqAimTmp = (frqAimTmp >= 0) ? (tmp) : (-tmp);

    if (funcCode.code.jumpFrqMode)
    {
        if ((jumpFrq1.low < tmp1) && (tmp1 < jumpFrq1.high))
        {
            if((ABS_INT16(frqAimTmp) >= ABS_INT16(frqTmp))
                    && (runFlag.bit.run)
                    && (runCmd.bit.common)
                )
            {
                tmp1 = jumpFrq1.high;
            }
            else
            {
                tmp1 = jumpFrq1.low;
            }
        }
        if ((jumpFrq2.low < tmp1) && (tmp1 < jumpFrq2.high))
        {
            if((ABS_INT16(frqAimTmp) >= ABS_INT16(frqTmp))
                && (runFlag.bit.run)
                && (runCmd.bit.common)
            )
            {
                tmp1 = jumpFrq2.high;
            }
            else
            {
                tmp1 = jumpFrq2.low;
            }
        }

        if (tmp1 > (int32)upperFrq)
        {
            tmp1 = (int32)upperFrq;
        }

        frqTmp = (frqTmp >= 0) ? (tmp1) : (-tmp1);
    }
#endif
}

#define JUMP_FRQ_RANGE_ALL  0
#define JUMP_FRQ_RANGE_HALF 1
#define JUMP_FRQ_RANGE  JUMP_FRQ_RANGE_ALL

LOCALF void JumpFrqEndCalc(JUMP_FRQ *p)
{
#if DEBUG_F_JUMP_FRQ
    int32 low;

#if 0
    if (0 == p->frq)
    {
        p->low = 0;
        p->high = 0;
        return;
    }
#endif

    low = (int32)p->frq - ((int32)p->range >> JUMP_FRQ_RANGE);
    if (low < 0)
        low = 0;
    p->low = low;

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
    int32 frq;
    Uint16 digit[5];
    GetNumberDigit1(digit, funcCode.code.frqRunCmdBind);
    runCmdBind = digit[runSrc];

    if (runCmdBind > 0)
    {
        frqAiPu = maxFrq;
        frqFlag.bit.comp = 0;
        frq = FrqXyCalc(runCmdBind);
        frqXDisp = 0;
        frqYDisp = 0;
    }

    else
    {
        frq = GetFrq();

        frqXDisp = ABS_INT16(frqXy.x);
        frqYDisp = ABS_INT16(frqXy.y);
    }
    return frq;
}

#define UP_DOWN_LIMIT_X 0
#define UP_DOWN_LIMIT_Y 1
#define UP_DOWN_LIMIT_NO_COMP 0
#define UP_DOWN_LIMIT_COMP 1
int32 frqMax;
int32 frqMin;
void getUpDownLimit(Uint16 comp, Uint16 type)
{
    int32 noUpDownFrq;
    int32 min;
    int32 upDownFrqPu;
    int32 minFrq;
    minFrq = frqMin;

    if (comp == UP_DOWN_LIMIT_COMP)
    {

        if (type == UP_DOWN_LIMIT_Y)
        {

            noUpDownFrq = frqXy.x + frqXy.z;
        }

        else if (type == UP_DOWN_LIMIT_X)
        {
            noUpDownFrq = frqXy.y + frqXy.z + (int32)funcCode.code.presetFrq;

            {
                min = 0 + frqXy.y + frqXy.z;
                if (min > minFrq)
                {
                    minFrq = min;
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
        noUpDownFrq = frqXy.z + (int32)funcCode.code.presetFrq;
        minFrq = frqXy.z;
    }

    if (minFrq > frqMax)
    {
        minFrq = frqMax;
    }

    uPDownFrqMax = frqMax - noUpDownFrq;
    uPDownFrqMin = minFrq - noUpDownFrq;
    uPDownFrqLower1 = lowerFrq - noUpDownFrq;
    uPDownFrqLower2 = (int16)(-lowerFrq) - noUpDownFrq;

    if ((comp == UP_DOWN_LIMIT_COMP) && (type == UP_DOWN_LIMIT_Y))
    {

        if (funcCode.code.frqYRangeBase)
        {
            upDownFrqPu  = ((Uint32)ABS_INT32(frqXy.x)*funcCode.code.frqYRange) / 100;
        }
        else
        {
            upDownFrqPu = ((Uint32)maxFrq*funcCode.code.frqYRange) / 100;
        }

        if (uPDownFrqMax > upDownFrqPu)
        {
            uPDownFrqMax = upDownFrqPu;
        }

        if (uPDownFrqMin < (-upDownFrqPu))
        {
            uPDownFrqMin = -upDownFrqPu;
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
    int32 frq;

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

    switch (funcCode.code.frqCalcSrc%10)
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

        if ((funcCode.code.frqCalcSrc/10) == 0)
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
        frq = GetFrqComp();
    }
    else if (frqFlag.bit.x)
    {
        frq = frqXy.x;
    }
    else if (frqFlag.bit.y)
    {
        frq = frqXy.y;
    }

    if (frq > frqMax)
    {
        frq = frqMax;
    }
    else if (frq < frqMin)
    {
        frq = frqMin;
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
    return frq;
}

int32 GetFrqComp(void)
{
#if DEBUG_F_FRQ_SRC_COMPOSE
    int32 frq;
    int32 x;
    int32 y;
    int32 frqYCompMax;

    if (funcCode.code.frqYRangeBase)
    {
        frqYCompMax  = ((Uint32)ABS_INT32(frqXy.x)*funcCode.code.frqYRange) / 100;
    }
    else
    {
        frqYCompMax = ((Uint32)maxFrq*funcCode.code.frqYRange) / 100;
    }

    if (frqXy.y > frqYCompMax)
    {
        frqXy.y = frqYCompMax;
    }

    switch (funcCode.code.frqCalcSrc/10)
    {
        case FUNCCODE_frqCalcSrc_ADD:
            x = frqXy.x;

            if ((funcCode.code.frqXSrc < FUNCCODE_frqXySrc_AI1) &&
                (funcCode.code.frqYSrc < FUNCCODE_frqXySrc_AI1))
            {
                frqXy.y = 0;
            }
            y = frqXy.y;

            frq = x + y;

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
            x = frqXy.x;
            y = frqXy.y;

            frq = x - y;

            break;

        case FUNCCODE_frqCalcSrc_MAX:
            x = frqXy.x;
            y = frqXy.y;

            if (ABS_INT32(x) >= ABS_INT32(y))
            {
                frq = x;
            }
            else
            {
                frq = y;
            }

            break;

        case FUNCCODE_frqCalcSrc_MIN:
            x = frqXy.x;
            y = frqXy.y;

            if (ABS_INT32(x) < ABS_INT32(y))
            {
                frq = x;
            }
            else
            {
                frq = y;
            }

            break;

        default:
            break;
    }

    frq += frqXy.z;

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

    return frq;
#endif
}

int32 GetFrqX(void)
{
    int32 frq;

    frqAiPu = maxFrq;

    if (diFunc.f2.bit.frqXSrc2Preset)
    {
        frq = funcCode.code.presetFrq;
    }
    else
    {
        frq = FrqXyCalc(funcCode.code.frqXSrc);
    }

    frqXy.x = frq;

    return frq;
}

int32 GetFrqY(void)
{
#if DEBUG_F_FRQ_SRC_COMPOSE
    int32 frq;

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

        frqAiPu = ((Uint32)frqAiPu * funcCode.code.frqYRange) / 100;
    }

    else
    {
        frqAiPu = maxFrq;
    }

    if (diFunc.f2.bit.frqYSrc2Preset)
    {
        frq = funcCode.code.presetFrq;
    }
    else
    {
        frq = FrqXyCalc(funcCode.code.frqYSrc);
    }

    if (frqFlag.bit.comp)
    {
        if (frq > frqAiPu)
        {
            frq = frqAiPu;
        }
        else if (frq < -frqAiPu)
        {
            frq = - frqAiPu;
        }
    }

    frqXy.y = frq;

    return frq;
#endif
}

LOCALF int32 FrqXyCalc(Uint16 src)
{
    int32 tmp = 0;

    switch (src)
    {
        case FUNCCODE_frqXySrc_FC:
        case FUNCCODE_frqXySrc_FC_P_OFF_REM:
            tmp = FrqAimUpDownDeal();
            break;

        case FUNCCODE_frqXySrc_AI1:
        case FUNCCODE_frqXySrc_AI2:
        case FUNCCODE_frqXySrc_AI3:
            tmp = src - FUNCCODE_frqXySrc_AI1;
            tmp = ((int32)aiDeal[tmp].set * frqAiPu + (1 << 14)) >> 15;
            break;

        case FUNCCODE_frqXySrc_PULSE:
            tmp = ((int32)pulseInSet * frqAiPu + (1 << 14)) >> 15;
            break;

        case FUNCCODE_frqXySrc_MULTI_SET:
            tmp = UpdateMultiSetFrq(diFunc.f1.bit.multiSet);
            break;

        case FUNCCODE_frqXySrc_PLC:
            tmp = FrqPlcSetDeal();
            break;

        case FUNCCODE_frqXySrc_PID:
            tmp = FrqPidSetDeal();
            break;

        case FUNCCODE_frqXySrc_COMM:

#if DEBUG_F_PLC_CTRL
            if (funcCode.code.plcEnable)
            {
                tmp = (int32)(int16)funcCode.code.plcFrqSet * maxFrq / 10000;
            }
            else
#endif
#if DEBUG_F_P2P_CTRL
            if ((CanRxTxCon == P2P_COMM_SLAVE)
                && (funcCode.code.p2pRevDataSel == P2P_REV_FRQ_SET)
                )
            {
                tmp = ((int32)(int16)p2pData.processValue * maxFrq) / 10000;
            }
            else
#endif
            if (funcCode.code.commProtocolSec == PROFIBUS)
            {
                tmp = (int32)(Uint16)dpFrqAim;
            }
            else
            {
                tmp = (int32)(int16)funcCode.code.frqComm * maxFrq / 10000;
            }
            break;

        default:
            break;
    }

    return tmp;
}
