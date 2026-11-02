
#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_io.h"
#include "f_main.h"
#include "f_p2p.h"

#if F_DEBUG_RAM
#define DEBUG_F_TORQUE      0
#elif 1
#define DEBUG_F_TORQUE      1
#endif

int16 upperTorque;
int16 torqueAim;

Uint16 torqueSetEthDp;
Uint16 torqueUpperEthDp;

#define TORQUE_LIMIT_TIME   800
extern Uint16 itDisp;

#if DEBUG_F_TORQUE

#define TORQUE_PU_SHIFT                 15
#define TORQUE_PU_BASE                  10000
#define RATING_TORQUE                   2000
#define TORQUE_LIMIT_CUR_PERCENT        85
#define TORQUE_LIMIT_CUR_PERCENT_BASE   100
#define TORQUE_LIMIT_SPD_ERR            200
#define TORQUE_LIMIT_TICKS              (TORQUE_LIMIT_TIME / DO_CALC_PERIOD)
#define TORQUE_RAMP_STEP                ((Uint16)(TIME_UNIT_TORQUE_CTRL_ACC_DEC / TORQUE_CTRL_PERIOD))

LINE_CHANGE_STRUCT torqurLine = LINE_CHANGE_STRTUCT_DEFALUTS;

void UpdateTorqueAim(void);

void AccDecTorqueCalc(void);

void TorqueCalc(void)
{
    UpdateTorqueAim();

    AccDecTorqueCalc();
}

void UpdateTorqueAim(void)
{
    Uint16 torqueSrc;
    int16 torqueFullScale;
    int16 torqueSet;
    int16 torqueAi2;

    if (RUN_MODE_TORQUE_CTRL == runMode)
    {
        torqueSrc = funcCode.code.driveUpperTorqueSrc;
        torqueFullScale = funcCode.code.driveUpperTorque;
    }
    else
    {
        torqueSrc = motorFc.vcPara.spdCtrlDriveTorqueLimitSrc;
        torqueFullScale = motorFc.vcPara.spdCtrlDriveTorqueLimit;
    }

    switch (torqueSrc)
    {
        case FUNCCODE_upperTorqueSrc_FC:
            torqueSet = torqueFullScale;
            break;

        case FUNCCODE_upperTorqueSrc_AI1:
        case FUNCCODE_upperTorqueSrc_AI2:
        case FUNCCODE_upperTorqueSrc_AI3:
            torqueSet = ((int32)aiDeal[torqueSrc - FUNCCODE_upperTorqueSrc_AI1].set
                         * torqueFullScale) >> TORQUE_PU_SHIFT;
            break;

        case FUNCCODE_upperTorqueSrc_PULSE:
            torqueSet = ((int32)pulseInSet * torqueFullScale) >> TORQUE_PU_SHIFT;
            break;

        case FUNCCODE_upperTorqueSrc_COMM:

#if DEBUG_F_PLC_CTRL
            if (funcCode.code.plcEnable)
            {
                torqueSet = (int16)funcCode.code.plcTorqueSet;
            }
            else
#endif
#if DEBUG_F_P2P_CTRL
            if ((CanRxTxCon == P2P_COMM_SLAVE)
                && (funcCode.code.p2pRevDataSel == P2P_REV_TORQUE_SET)
                )
            {
                torqueSet = ((int32)(int16)p2pData.processValue * RATING_TORQUE)
                            / TORQUE_PU_BASE;
            }
            else
#endif
            {
                torqueSet = ((int32)(int16)funcCode.code.frqComm * torqueFullScale)
                            / TORQUE_PU_BASE;
            }
            break;

        case FUNCCODE_upperTorqueSrc_MIN_AI1_AI2:
            torqueSet = ((int32)aiDeal[0].set * torqueFullScale) >> TORQUE_PU_SHIFT;
            torqueAi2 = ((int32)aiDeal[1].set * torqueFullScale) >> TORQUE_PU_SHIFT;

            if (torqueSet > torqueAi2)
            {
                torqueSet = torqueAi2;
            }
            break;

        case FUNCCODE_upperTorqueSrc_MAX_AI1_AI2:
            torqueSet = ((int32)aiDeal[0].set * torqueFullScale) >> TORQUE_PU_SHIFT;
            torqueAi2 = ((int32)aiDeal[1].set * torqueFullScale) >> TORQUE_PU_SHIFT;

            if (torqueSet < torqueAi2)
            {
                torqueSet = torqueAi2;
            }
            break;

        default:
            break;
    }

    torqueAim = torqueSet;

#if 0
#define RATING_TORQUE      2000
#define TORQUE_LINE_FILTER funcCode.code.torqueFilter*5

    torqurLine.aimValue = ABS_INT16(torqueAim);
    torqurLine.tickerAll = TORQUE_LINE_FILTER;
    torqurLine.maxValue = RATING_TORQUE;
    torqurLine.curValue = upperTorque;
    torqurLine.calc(&torqurLine);
    upperTorque = torqurLine.curValue;

    AccDecTorqueCalc();
#endif
}

void AccDecTorqueCalc(void)
{
    int32 rampTicks;

    if (RUN_MODE_TORQUE_CTRL != runMode)
    {
        upperTorque = torqueAim;
        return;
    }

    if (!runFlag.bit.run)
    {
        upperTorque = 0;
        return;
    }

    if (torqueAim == upperTorque)
    {
        return;
    }

    if (((upperTorque >= 0) && (torqueAim > upperTorque))
        || ((upperTorque <= 0) && (torqueAim < upperTorque)))
    {
        rampTicks = (int32)funcCode.code.torqueCtrlAccTime;
    }
    else
    {
        rampTicks = (int32)funcCode.code.torqueCtrlDecTime;
    }

    rampTicks = rampTicks * TORQUE_RAMP_STEP;

    torqurLine.aimValue = torqueAim;
    torqurLine.tickerAll = rampTicks;
    torqurLine.maxValue = RATING_TORQUE;
    torqurLine.curValue = upperTorque;
    torqurLine.calc(&torqurLine);
    upperTorque = torqurLine.curValue;
}

Uint16 TorqueLimitCalc(void)
{
    static Uint16 torqueLimitCurTicker;
    static Uint16 torqueLimitSpdTicker;
    Uint16 limitActive = 0;
    int32 curLimit;

    curLimit = (int32)(ABS_INT32(upperTorque) * TORQUE_LIMIT_CUR_PERCENT)
               / TORQUE_LIMIT_CUR_PERCENT_BASE;

    if (itDisp > curLimit)
    {
        if (++torqueLimitCurTicker >= TORQUE_LIMIT_TICKS)
        {
            torqueLimitCurTicker = TORQUE_LIMIT_TICKS + 1;
        }
    }
    else
    {
        torqueLimitCurTicker = 0;
    }

    if (ABS_INT32(frq - frqRun) > TORQUE_LIMIT_SPD_ERR)
    {
        if (++torqueLimitSpdTicker >= TORQUE_LIMIT_TICKS)
        {
            torqueLimitSpdTicker = TORQUE_LIMIT_TICKS + 1;
        }
    }
    else
    {
        torqueLimitSpdTicker = 0;
    }

    if ((torqueLimitCurTicker >= TORQUE_LIMIT_TICKS)
        && (torqueLimitSpdTicker >= TORQUE_LIMIT_TICKS))
    {
        limitActive = 1;
    }

    return (limitActive);
}

#elif 1

void TorqueCalc(void)
{
    upperTorque = motorFc.spdCtrlTorqueLimit;
}

void UpdateTorqueAim(void)
{
    torqueAim = motorFc.spdCtrlTorqueLimit;
}

Uint16 TorqueLimitCalc(void)
{
    return 0;
}
#endif
