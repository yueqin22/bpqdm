
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
    int16 tmp,tmp1;
    Uint16 torqueSrc;
    int16 torquePu;

    if (RUN_MODE_TORQUE_CTRL != runMode)
    {

        torqueSrc = motorFc.vcPara.spdCtrlDriveTorqueLimitSrc;
        torquePu = motorFc.vcPara.spdCtrlDriveTorqueLimit;
    }

    else
    {

        torqueSrc = funcCode.code.driveUpperTorqueSrc;
        torquePu = funcCode.code.driveUpperTorque;
    }

    switch (torqueSrc)
    {
        case FUNCCODE_upperTorqueSrc_FC:
            tmp = torquePu;
            break;

        case FUNCCODE_upperTorqueSrc_AI1:
        case FUNCCODE_upperTorqueSrc_AI2:
        case FUNCCODE_upperTorqueSrc_AI3:
            tmp = torqueSrc - FUNCCODE_upperTorqueSrc_AI1;
            tmp = ((int32)aiDeal[tmp].set * torquePu) >> 15;
            break;

        case FUNCCODE_upperTorqueSrc_PULSE:
            tmp = ((int32)pulseInSet * torquePu) >> 15;
            break;

        case FUNCCODE_upperTorqueSrc_COMM:

#if DEBUG_F_PLC_CTRL
            if (funcCode.code.plcEnable)
            {
                tmp = (int16)funcCode.code.plcTorqueSet;
            }
            else
#endif
#if DEBUG_F_P2P_CTRL
            if ((CanRxTxCon == P2P_COMM_SLAVE)
                && (funcCode.code.p2pRevDataSel == P2P_REV_TORQUE_SET)
                )
            {
                tmp = ((int32)(int16)p2pData.processValue * 2000) / 10000;
            }
            else
#endif
            {
                tmp = ((int32)(int16)funcCode.code.frqComm * torquePu) / 10000;
            }
            break;

        case FUNCCODE_upperTorqueSrc_MIN_AI1_AI2:
            tmp = ((int32)aiDeal[0].set * torquePu) >> 15;
            tmp1 = ((int32)aiDeal[1].set * torquePu) >> 15;

            if (tmp > tmp1)
            {
                tmp = tmp1;
            }
            break;

        case FUNCCODE_upperTorqueSrc_MAX_AI1_AI2:
            tmp = ((int32)aiDeal[0].set * torquePu) >> 15;
            tmp1 = ((int32)aiDeal[1].set * torquePu) >> 15;

            if (tmp < tmp1)
            {
                tmp = tmp1;
            }
            break;

        default:
            break;
    }

    torqueAim = tmp;

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
    int32 accDecTime;

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
        accDecTime = (int32)funcCode.code.torqueCtrlAccTime;
    }
    else
    {
        accDecTime = (int32)funcCode.code.torqueCtrlDecTime;
    }

    accDecTime = accDecTime * (Uint16)(TIME_UNIT_TORQUE_CTRL_ACC_DEC / TORQUE_CTRL_PERIOD);

#define RATING_TORQUE   2000

    torqurLine.aimValue = torqueAim;
    torqurLine.tickerAll = accDecTime;
    torqurLine.maxValue = RATING_TORQUE;
    torqurLine.curValue = upperTorque;
    torqurLine.calc(&torqurLine);
    upperTorque = torqurLine.curValue;
}

Uint16 TorqueLimitCalc(void)
{
    static Uint16 torqueLimitCurTicker;
    static Uint16 torqueLimitSpdTicker;
    Uint16 ret = 0;

    if (itDisp > (int32)(ABS_INT32(upperTorque) * 85) / 100)
    {
        if (++torqueLimitCurTicker >= TORQUE_LIMIT_TIME / DO_CALC_PERIOD)
        {
            torqueLimitCurTicker = TORQUE_LIMIT_TIME / DO_CALC_PERIOD + 1;
        }
    }
    else
    {
        torqueLimitCurTicker = 0;
    }

    if ( (ABS_INT32(frq - frqRun)) > 200 )
    {
        if (++torqueLimitSpdTicker >= TORQUE_LIMIT_TIME / DO_CALC_PERIOD)
        {
            torqueLimitSpdTicker = TORQUE_LIMIT_TIME / DO_CALC_PERIOD + 1;
        }
    }
    else
    {
        torqueLimitSpdTicker = 0;
    }

    if ((torqueLimitCurTicker >= TORQUE_LIMIT_TIME / DO_CALC_PERIOD)

        && (torqueLimitSpdTicker >= TORQUE_LIMIT_TIME / DO_CALC_PERIOD)
        )
    {
        ret = 1;
    }

    return (ret);
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
