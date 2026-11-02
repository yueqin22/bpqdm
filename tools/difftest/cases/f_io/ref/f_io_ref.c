
#include "f_io.h"
#include "f_main.h"
#include "f_runSrc.h"
#include "f_comm.h"
#include "f_menu.h"
#include "f_frqSrc.h"
#include "f_posCtrl.h"
#include "f_error.h"

#if F_DEBUG_RAM

#define DEBUG_F_DI              0
#define DEBUG_F_DO              0
#define DEBUG_F_AI              0
#define DEBUG_F_AO              0
#define DEBUG_F_HDI             0
#define DEBUG_F_HDO             0
#define DEBUG_F_TEMPERATURE     0

#elif 1

#define DEBUG_F_DI              1
#define DEBUG_F_DO              1
#define DEBUG_F_AI              1
#define DEBUG_F_AO              1
#define DEBUG_F_HDI             1
#define DEBUG_F_HDO             1

#if !DEBUG_F_POSITION_CTRL
#define DEBUG_F_TEMPERATURE     1
#elif 1
#define DEBUG_F_TEMPERATURE     0
#endif

#endif

struct DI_FUNC diFunc;
struct DI_FUNC diSelectFunc;
struct DI_STATUS_ALL diStatus;
LOCALF union DI_DELAY_TICKER_UNION diDelayTicker;

struct DO_FUNC doFunc;
Uint16  oCurrentCheckFlag;
struct DO_STATUS_ALL doStatus;
union DO_HW_STATUS doHwStatus;
LOCALF union DO_DELAY_TICKER_UNION doDelayTicker;

#define DO_STATUS_VDO   5

#define PULSE_OUT_CTR_PERIOD       (MAX_UINT16 - 2)

#define AI_VOLTAGE_CALC_SHIFT   5
#define AI_SAMPLE_Q             16
#define AI_MAX_SAMPLE1          ((0x0FFFUL-0)<<4)
#define AI_MAX_SAMPLE           ((0x0FFFUL-3)<<4)

#define AI_MAX_VOLTAGE_HW       1057
#define AI_MIN_VOLTAGE_HW       (int16)-1057

LowPassFilter aiLpf[AI_NUMBER] = {LPF_DEFALUTS, LPF_DEFALUTS, LPF_DEFALUTS};
struct AI_DEAL aiDeal[AI_NUMBER];

#define AO_PWM_FRQ          10000
#define AO_PWM_PERIOD       (DSP_CLOCK * 1000000UL / AO_PWM_FRQ)

#define AO_MAX_VOLTAGE_IDEA     10000

#define FAN_CONTROL_STOP_DELAY  2500
#define FAN_CONTROL_START_TEMP  50
#define FAN_CONTROL_STOP_TEMP   45

int32 aoFmpValue;
int32 aoFmpMax;

#define PULSE_IN_ZERO                           1000
#define COUNTER_IN_USE_CAPTURE_TICKER_LIMIT     21000

int16 pulseInSet;
Uint32 pulseInFrq;
Uint32 pulseInFrq1;
Uint16 lineSpeed;

LOCALF LowPassFilter pulseInLpf = LPF_DEFALUTS;
LOCALF Uint32 capturePeriodSum;
LOCALF Uint16 captureTicker;
LOCALF Uint16 noCaptureTicker;

LOCALF Uint16 bOverFlowCounterTicker;
LOCALF Uint16 bOverFlowLengthCurrent;

LOCALF Uint32 oCurrentChkTicker;
Uint16 softOcDoFlag = 0;

void TemperatureDeal(void);
void GetTemperature(void);
Uint16 GetTemperatureCalc(Uint16 voltage, const Uint16 *p, Uint16 len);
void TemperatureAfterDeal(void);
void TemperatureSensorDeal(void);
Uint16 motorForeOT;
Uint16 motorOT;
Uint16 temperature;
Uint16 tickerTempDeal;
Uint32 tempSampleSum;
Uint16 temperatureVoltage;
Uint16 temperatureVoltageOrigin;
#define TEMPERATURE_CALC_PERIOD     100
#define TEMPERATURE_CALL_PERIOD     2

#if DEBUG_F_TEMPERATURE

const Uint16 voltageTempPT100[] =
{
    1650 	,
    1675 	,
    1700 	,
    1724 	,
    1749 	,
    1774 	,
    1798 	,
    1823 	,
    1847 	,
    1872 	,
    1896 	,
    1920 	,
    1945 	,
    1969 	,
    1993 	,
    2017 	,
    2041 	,
    2066 	,
    2090 	,
    2114 	,
    2138 	,
    2161 	,
    2185 	,
    2209 	,
    2233 	,
    2257 	,
    2280 	,
    2304 	,
    2328 	,
    2351 	,
    2375 	,
    2398 	,
    2422 	,
    2445 	,
    2469 	,
    2492 	,
    2515 	,
    2538 	,
    2562 	,
    2585 	,
    2608 	,
    2631 	,
    2654 	,
    2677 	,
    2700 	,
    2723 	,
    2746 	,
    2769 	,
    2791 	,
    2814 	,
    2837 	,

};

const Uint16 voltageTempPT1000[] =
{
    1648	,
    1673	,
    1698	,
    1722	,
    1748	,
    1772	,
    1795	,
    1820	,
    1844	,
    1868	,
    1891	,
    1916	,
    1941	,
    1964	,
    1988	,
    2012	,
    2036	,
    2059	,
    2083	,
    2108	,
    2130	,
    2155	,
    2177	,
    2202	,
    2225	,
    2249	,
    2272	,
    2295	,
    2317	,
    2341	,
    2364	,
    2387	,
    2411	,
    2433	,
    2456	,
    2478	,
    2502	,
    2524	,
    2547	,
    2570	,
    2592	,
    2616	,
    2638	,
    2661	,
    2683	,
    2706	,
    2729	,
    2750	,
    2772	,
    2795	,
    2818	,
};
#endif

LOCALD void UpdateDoFunc(void);

void  newUpdateDoFunc(Uint16 index);

LOCALD void UpdateFmpAoValue(Uint16 func, Uint16 aoOrFmp);
void PulseOutDeal(void);
void PulseOutCalc(void);
LOCALD void Di2Bin(void);
LOCALD void Do2Bin(void);
LOCALD void getDiHwStatus(void);
LOCALD void setDOStatus(void);

Uint32 diLogic;
Uint16 vdiSrc;
Uint16 vdiFcSet;
void DiCalc(void)
{
    static Uint16 diXFilterCounter[DI_NUMBER_PHSIC];
    int16 i;
    Uint32 tmp;
    Uint32 tmpFc;

    Uint16 fc1[DI_TERMINAL_NUMBER];
    Uint32 *pDiSelectFunc = &diSelectFunc.f1.all;
    Uint32 *pDiFunc = &diFunc.f1.all;

    diSelectFunc.f1.all = 0;
    diSelectFunc.f2.all = 0;
    diFunc.f1.all = 0;
    diFunc.f2.all = 0;
#if DEBUG_F_POSITION_CTRL
    diSelectFunc.f3.all = 0;
    diFunc.f3.all = 0;
#endif

#if DEBUG_F_DI
    Di2Bin();

#if DSP_2803X
    getDiHwStatus();
#endif
    asm(" nop");

    for (i = DI_NUMBER_PHSIC - 1; i >= 0; i--)
    {
        tmp = 0x01UL << i;

        if ((diStatus.a.all ^ (~diHwStatus.all)) & tmp)
        {

            if (++diXFilterCounter[i] >= funcCode.code.diFilterTime / DI_CALC_PERIOD)
            {
                diStatus.a.all ^= tmp;
                diXFilterCounter[i] = 0;
            }
        }
        else
        {
            diXFilterCounter[i] = 0;
        }

        fc1[i] = funcCode.code.diFunc[i];
    }

    for (i = DI_NUMBER_V - 1; i >= 0; i--)
    {
        Uint16 vdiTmp = (0x01U << i);
        Uint16 vdi;

        tmp = 0x01UL << (i + DI_NUMBER_PHSIC);

        if (!(vdiSrc & vdiTmp))
        {
            vdi = doStatus.c.all >> DO_STATUS_VDO;
        }
        else
        {
            vdi = vdiFcSet;
        }

        if (vdi & vdiTmp)
        {
            diStatus.a.all |= tmp;
        }
        else
        {
            diStatus.a.all &= ~tmp;
        }

        fc1[i + DI_NUMBER_PHSIC] = funcCode.code.vdiFunc[i];
    }

    for (i = DI_NUMBER_AI_AS_DI - 1; i >= 0; i--)
    {
        tmp = 0x01UL << (i + DI_NUMBER_PHSIC + DI_NUMBER_V);

#define AI_AS_DI_LOW    300
#define AI_AS_DI_HIGH   700
        if (aiDeal[i].voltage >= AI_AS_DI_HIGH)
        {
            diStatus.a.all |= tmp;
        }
        else if (aiDeal[i].voltage <= AI_AS_DI_LOW)
        {
            diStatus.a.all &= ~tmp;
        }

        fc1[i + DI_NUMBER_PHSIC + DI_NUMBER_V] = funcCode.code.aiAsDiFunc[i];
    }

    for (i = 3-1; i >= 0; i--)
    {
        int32 delayTickerMax;

        tmp = 0x01UL << i;

        delayTickerMax = (Uint32)funcCode.code.diDelayTime[i] * (TIME_UNIT_DI_DELAY / DI_CALC_PERIOD);

        if (diStatus.a.all & tmp)
        {
            diDelayTicker.all[i].low = 0;
            if (++diDelayTicker.all[i].high >= delayTickerMax)
                diStatus.b.all |= tmp;
        }
        else
        {
            diDelayTicker.all[i].high = 0;
            if (++diDelayTicker.all[i].low >= delayTickerMax)
                diStatus.b.all &= ~tmp;
        }
    }
    diStatus.b.all = (diStatus.a.all & 0xFFFFFFF8UL) |  (diStatus.b.all & 0x00000007UL);

    diStatus.c.all = diStatus.b.all ^ diLogic;

    for (i = DI_TERMINAL_NUMBER - 1; i >= 0; i--)
    {
        tmp = 0x01UL << i;

        tmpFc = 0x01UL << (fc1[i] % 32);

        *(pDiSelectFunc + fc1[i] / 32) |= tmpFc;

        if (diStatus.c.all & tmp)
        {
            *(pDiFunc + fc1[i] / 32) |= tmpFc;
        }
        else
        {
            *(pDiFunc + fc1[i] / 32) &= ~tmpFc;
        }
    }

    if ((DI_FUNC_COUNTER_TICKER_IN != funcCode.code.diFunc[4]) &&
        (DI_FUNC_LENGTH_TICKER_IN != funcCode.code.diFunc[4]) &&
        (
         (FUNCCODE_frqXySrc_PULSE == funcCode.code.frqXSrc) ||
         (FUNCCODE_frqXySrc_PULSE == funcCode.code.frqYSrc) ||
         (FUNCCODE_upperFrqSrc_PULSE  == funcCode.code.upperFrqSrc) ||
         (FUNCCODE_pidSetSrc_PULSE == funcCode.code.pidSetSrc) ||
         (FUNCCODE_pidFdbSrc_PULSE == funcCode.code.pidFdbSrc) ||
         (FUNCCODE_plcFrq0Src_PULSE == funcCode.code.plcFrq0Src)
        )
       )
    {
        *(pDiFunc) &= ~(0x01UL << funcCode.code.diFunc[4]);
    }
#endif
}

void Dout1(void)
{
    if (runFlag.bit.run)
    {
        doFunc.f1.bit.run = 1;
    }
}

void Dout2(void)
{
    if

       (errorCode
       && ( errorDealStatus != ERROR_DEAL_OK)
       && (funcCode.code.errAutoRstRelayAct || (errAutoRstNum >= funcCode.code.errAutoRstNumMax))
       )
    {

        if (ERROR_LEVEL_RUN != errorAttribute.bit.level)
        {
            doFunc.f1.bit.error = 1;
        }
    }
}

void Dout38(void)
{
    if

       (errorCode
       && ( errorDealStatus != ERROR_DEAL_OK)
       && (funcCode.code.errAutoRstRelayAct || (errAutoRstNum >= funcCode.code.errAutoRstNumMax))
       )
    {
        doFunc.f2.bit.errorOnStop = 1;
    }
}

void Dout41(void)
{
    if

       (errorCode
       && ( errorDealStatus != ERROR_DEAL_OK)
       && (errorCode != ERROR_UV)
       && (funcCode.code.errAutoRstRelayAct || (errAutoRstNum >= funcCode.code.errAutoRstNumMax))
       )
    {

        if (ERROR_LEVEL_RUN != errorAttribute.bit.level)
        {
            doFunc.f2.bit.errorOnNoUV = 1;
        }
    }
}

void Dout3(void)
{
    static Uint16 frqFdtArriveOld;

    if (ABS_INT32(frq) >= funcCode.code.frqFdtValue)
    {
        frqFdtArriveOld = 1;
    }
    else if (ABS_INT32(frq) < ((Uint32)funcCode.code.frqFdtValue * (1000 - funcCode.code.frqFdtLag) / 1000))
    {
        frqFdtArriveOld = 0;
    }

    if (!runFlag.bit.run)
    {
        frqFdtArriveOld = 0;
    }

    if (frqFdtArriveOld)
    {
        doFunc.f1.bit.frqFdtArrive = 1;
    }
}

void Dout4(void)
{
    if ((ABS_INT32(frq) <= (ABS_INT32(frqAim) + (int32)maxFrq * funcCode.code.frqArriveRange / 1000))
        && (ABS_INT32(frq) >= (ABS_INT32(frqAim) - (int32)maxFrq * funcCode.code.frqArriveRange / 1000))
        && (runFlag.bit.run)
        && ((int64)frq * frqAim >= 0)
        && (!tuneCmd)
        )
    {
        doFunc.f1.bit.frqArrive = 1;
    }
}

void Dout5(void)
{
    if (!frq)
    {
        if(runFlag.bit.run)
        {
            doFunc.f1.bit.zeroSpeedRun = 1;
        }
    }
}

void Dout23(void)
{
    if (!frq)
    {
        doFunc.f1.bit.zeroSpeedRun1 = 1;
    }
}

void Dout6(void)
{
    if (dspStatus.bit.motorPreOl)
    {
        doFunc.f1.bit.motorPreOl = 1;
    }
}

void Dout7(void)
{
    if (dspStatus.bit.inverterPreOl)
    {
        doFunc.f1.bit.inverterPreOl = 1;
    }
}

void Dout8(void)
{
    if (funcCode.code.counterTicker >= funcCode.code.counterSet)
    {
        doFunc.f1.bit.counterSetArrive = 1;
    }

    if (bOverFlowCounterTicker)
    {
        doFunc.f1.bit.counterSetArrive = 1;
    }
}

void Dout9(void)
{
    if (funcCode.code.counterTicker >= funcCode.code.counterPoint)
    {
        doFunc.f1.bit.counterPointArrive = 1;
    }

    if (bOverFlowCounterTicker)
    {
        doFunc.f1.bit.counterPointArrive = 1;
    }
}

void Dout10(void)
{
    if ((funcCode.code.lengthCurrent >= funcCode.code.lengthSet)
        || bOverFlowLengthCurrent)
    {
        doFunc.f1.bit.lengthArrive = 1;
    }
}

void Dout11(void)
{
    if (bPlcEndOneLoop)
    {
        doFunc.f1.bit.plcEndLoop = 1;
    }
}

void Dout12(void)
{
    if ((funcCode.code.runTimeAddup >= funcCode.code.runTimeArriveSet)
        && funcCode.code.runTimeArriveSet)
    {
        doFunc.f1.bit.runTimeArrive = 1;
    }
}

void Dout13(void)
{
	if (swingFrqLimit)
	{
		doFunc.f1.bit.frqLimit = 1;
	}
}

void Dout14(void)
{
    if (TorqueLimitCalc())
    {
        doFunc.f1.bit.torqueLimit = 1;
    }
}

void Dout15(void)
{
    if ((!errorCode)
        && (!bUv)
        && (!diFunc.f1.bit.closePwm)
        )
    {
        doFunc.f1.bit.runReadyOk = 1;
    }
}

void Dout16(void)
{
    if (ABS_INT16(aiDeal[0].set) > ABS_INT16(aiDeal[1].set))
    {
        doFunc.f1.bit.ai1GreaterThanAi2 = 1;
    }
}

void Dout17(void)
{
	if ((ABS_INT32(frqAim) >= upperFrq)
      && (ABS_INT32(frq) >= upperFrq))
	{
		doFunc.f1.bit.upperFrqArrive = 1;
	}
}

void Dout18(void)
{
	if ((ABS_INT32(frqAim) <= lowerFrq)
		&& (ABS_INT32(frq) <= lowerFrq)
		&& runFlag.bit.run)
	{
		doFunc.f1.bit.lowerFrqArrive = 1;
	}
}

void Dout19(void)
{
    if (!dspStatus.bit.uv)
    {
        doFunc.f1.bit.uv = 1;
    }
}

void DoutRSVD(void)
{
}

void Dout21(void)
{
#if DEBUG_F_POSITION_CTRL
    if (bPcErrorOk)
    {
        doFunc.f1.bit.pcOk = 1;
    }
#endif
}

void Dout22(void)
{
#if DEBUG_F_POSITION_CTRL
    if (bPcErrorNear)
    {
        doFunc.f1.bit.pcNear = 1;
    }
#endif
}

void Dout24(void)
{
    if ((funcCode.code.powerUpTimeAddup >= funcCode.code.powerUpTimeArriveSet)
        && funcCode.code.powerUpTimeArriveSet)
    {
        doFunc.f1.bit.powerUpTimeArrive = 1;
    }
}

void Dout25(void)
{
    static Uint16 frqFdt1ArriveOld;

    if (ABS_INT32(frq) >= funcCode.code.frqFdt1Value)
    {
        frqFdt1ArriveOld = 1;
    }
    else if (ABS_INT32(frq) < ((Uint32)funcCode.code.frqFdt1Value * (1000 - funcCode.code.frqFdt1Lag) / 1000))
    {
        frqFdt1ArriveOld = 0;
    }
    if (!runFlag.bit.run)
    {
        frqFdt1ArriveOld = 0;
    }
    if (frqFdt1ArriveOld)
    {
        doFunc.f1.bit.frqFdtArrive1 = 1;
    }
}

void Dout26(void)
{
    if ((ABS_INT32(frq) <= (funcCode.code.frqArriveValue1 + (int32)maxFrq * funcCode.code.frqArriveRange1 / 1000))
        && (ABS_INT32(frq) >= (funcCode.code.frqArriveValue1 - (int32)maxFrq * funcCode.code.frqArriveRange1 / 1000))
        && runFlag.bit.run
        )
    {
        doFunc.f1.bit.frqArrive1 = 1;
    }
}

void Dout27(void)
{
    if ((ABS_INT32(frq) <= (funcCode.code.frqArriveValue2 + (int32)maxFrq * funcCode.code.frqArriveRange2 / 1000))
        && (ABS_INT32(frq) >= (funcCode.code.frqArriveValue2 - (int32)maxFrq * funcCode.code.frqArriveRange2 / 1000))
        && runFlag.bit.run
        )
    {
        doFunc.f1.bit.frqArrive2 = 1;
    }
}

void Dout28(void)
{
    if ((outCurrentDisp >= (((int32)funcCode.code.currentArriveValue1 - funcCode.code.currentArriveRange1)*motorFc.motorPara.elem.ratingCurrent/1000))
        && (outCurrentDisp <= ((funcCode.code.currentArriveValue1 + funcCode.code.currentArriveRange1)*motorFc.motorPara.elem.ratingCurrent/1000))
        && runFlag.bit.run
        )
    {
        doFunc.f1.bit.currentArrive1 = 1;
    }
}

void Dout29(void)
{
    if ((outCurrentDisp >= (((int32)funcCode.code.currentArriveValue2 - funcCode.code.currentArriveRange2)*motorFc.motorPara.elem.ratingCurrent/1000))
        && (outCurrentDisp <= ((Uint32)(funcCode.code.currentArriveValue2 + funcCode.code.currentArriveRange2)*motorFc.motorPara.elem.ratingCurrent/1000))
        && runFlag.bit.run
        )
    {
        doFunc.f1.bit.currentArrive2 = 1;
    }
}

#define SET_TIME_ARRIVE_TCNT_MAX   250
void Dout30(void)
{
    static Uint16 arriveTcnt;
    if ((!setRunLostTime) && setRunTimeAim)
    {
        arriveTcnt = SET_TIME_ARRIVE_TCNT_MAX;
    }

    if (arriveTcnt > 0)
    {
        doFunc.f1.bit.setTimeArrive = 1;
        arriveTcnt--;
    }
}

void Dout31(void)
{
    if ((ABS_INT16(aiDeal[0].voltage) > funcCode.code.ai1VoltageUpper) ||
        (ABS_INT16(aiDeal[0].voltage) < funcCode.code.ai1VoltageLimit))
    {
        doFunc.f1.bit.ai1limit = 1;
    }
}

void Dout32(void)
{
    if (dspStatus.bit.outAirSwitchOff)
    {
        doFunc.f2.bit.loseLoad = 1;
    }
}

void Dout33(void)
{
    if ( runFlag.bit.run
    	&& ((frqRun < 0)
        	|| ((frqRun == 0) && (frqAim < 0)))
		)
    {
        doFunc.f2.bit.speedDir = 1;
    }
}

void Dout34(void)
{
    if (!oCurrentCheckFlag)
    {
        if ((runFlag.bit.run) &&
            (outCurrentDisp <= ((Uint32)funcCode.code.oCurrentChkValue*motorFc.motorPara.elem.ratingCurrent/1000))
            )
        {

            if (oCurrentChkTicker < ((Uint32)funcCode.code.oCurrentChkTime*TIME_UNIT_CURRENT_CHK / DO_CALC_PERIOD))
            {
                oCurrentChkTicker++;
            }
        }
        else
        {
            oCurrentChkTicker = 0;
        }

        if (oCurrentChkTicker >= ((Uint32)funcCode.code.oCurrentChkTime*TIME_UNIT_CURRENT_CHK / DO_CALC_PERIOD))
        {
            doFunc.f2.bit.oCurrent = 1;
        }

        oCurrentCheckFlag = 1;
    }
}

void Dout35(void)
{
    if (funcCode.code.radiatorTemp >= funcCode.code.temperatureArrive)
    {
        doFunc.f2.bit.tempArrive= 1;
    }
}

void Dout36(void)
{
    if (softOcDoFlag == 1)
    {
        doFunc.f2.bit.softOc= 1;
    }
    else
    {
        doFunc.f2.bit.softOc= 0;
    }
}

void Dout37(void)
{
	if ((ABS_INT32(frqAim) <= lowerFrq)
		&& (ABS_INT32(frq) <= lowerFrq))
	{
		doFunc.f2.bit.lowerFrqArrive = 1;
	}
}

void Dout39(void)
{

    if (motorForeOT)
    {
        doFunc.f2.bit.motorForeOT = 1;
    }
}

void Dout40(void)
{

    if (funcCode.code.setTimeArrive
        && (curTime.runTime >= funcCode.code.setTimeArrive))
    {
        doFunc.f2.bit.setRunTimeArrive = 1;
    }
}

typedef struct
{
    void (*calc)();
} DO_FUNC_CAL;

const DO_FUNC_CAL doFuncCal[DO_FUNC_NUMBER] = {
    DoutRSVD,  Dout1,  Dout2,  Dout3,  Dout4,
    Dout5,     Dout6,  Dout7,  Dout8,  Dout9,
    Dout10,    Dout11, Dout12, Dout13, Dout14,
    Dout15,    Dout16, Dout17, Dout18, Dout19,
    DoutRSVD,  Dout21, Dout22, Dout23, Dout24,
    Dout25,    Dout26, Dout27, Dout28, Dout29,
    Dout30,    Dout31, Dout32, Dout33, Dout34,
    Dout35,    Dout36, Dout37, Dout38, Dout39,
    Dout40,    Dout41,
};

Uint16 doLogic;
void DoCalc(void)
{
#if DEBUG_F_DO
    Uint32 delayTickerMax;
    Uint16 doCommIndex[DO_TERMINAL_NUMBER] = {4, 2, 3, 0, 1, 5, 6, 7, 8, 9};
    int16 i;

    doFunc.f1.all = 0;
    doFunc.f2.all = 0;
    oCurrentCheckFlag = 0;

    doStatus.a.all = 0;
    for (i = DO_TERMINAL_NUMBER - 1; i >= 0; i--)
    {
        Uint16 fc;
        Uint16 tmpSci;
        Uint16 tmp;
        Uint16 flag = 0;

        if (i < DO_NUMBER_PHSIC)
        {
            fc = funcCode.code.doFunc[i];
    		tmp = 1U << i;
            delayTickerMax = (Uint32)funcCode.code.doDelayTime[i] * (TIME_UNIT_DO_DELAY / DO_CALC_PERIOD);
        }
        else
        {
            Uint16 vdoTmp = i - DO_NUMBER_PHSIC;
            fc = funcCode.code.vdoFunc[vdoTmp];
    		tmp = 1U << (vdoTmp + DO_STATUS_VDO);
            delayTickerMax = (Uint32)funcCode.code.vdoDelayTime[vdoTmp] * (TIME_UNIT_DO_DELAY / DO_CALC_PERIOD);
        }

        tmpSci = 1U << doCommIndex[i];

        if ((i >= DO_NUMBER_PHSIC)
            && (fc == 0))
        {
            flag = (diStatus.c.all>>(i - DO_NUMBER_PHSIC)) & 0x0001;
        }
        else if (DO_FUNC_COMM_CTRL != fc)
        {
            doComm.all &= ~tmpSci;
#if 0
            flag = doFuncCal[fc].calc();
#else
            doFuncCal[fc].calc();

            if(fc > 31)
            {
                if (doFunc.f2.all & (0x1UL << (fc-32)))
                    flag = 1;
            }
            else
            {
                if (doFunc.f1.all & (0x1UL << fc))
                    flag = 1;
            }
#endif
        }
        else
        {
            if (doComm.all & tmpSci)
            {
                flag = 1;
            }
        }

        if (flag)
        {
            doStatus.a.all |= tmp;

            doDelayTicker.all[i].low = 0;
            if (++doDelayTicker.all[i].high >= delayTickerMax)
                doStatus.b.all |= tmp;
        }
        else
        {

            doDelayTicker.all[i].high = 0;
            if (++doDelayTicker.all[i].low >= delayTickerMax)
                doStatus.b.all &= ~tmp;
        }
    }

    Do2Bin();
    doStatus.c.all = doStatus.b.all ^ doLogic;

    doHwStatus.bit.relay1 = doStatus.c.bit.relay1;
    doHwStatus.bit.relay2 = doStatus.c.bit.relay2;
    doHwStatus.bit.do1    = doStatus.c.bit.do1;
    doHwStatus.bit.do2    = doStatus.c.bit.do2;
    doHwStatus.bit.error  = doFunc.f1.bit.error;
    doHwStatus.bit.run    = doFunc.f1.bit.run;
    doHwStatus.bit.fan    = (dspStatus.bit.fan | funcCode.code.fanControl);

#if DEBUG_F_PLC_CTRL
    if (funcCode.code.outPortControl && funcCode.code.plcEnable)
    {
        Uint16 digit1[5];
        Uint16 digit2[5];

        GetNumberDigit1(digit1, funcCode.code.outPortControl);

        GetNumberDigit1(digit2, funcCode.code.inPortOut);

        if (digit1[0])
        {
            doStatus.c.bit.do3 = digit2[0];
        }

        if (digit1[1])
        {
            doHwStatus.bit.relay1 = digit2[1];
        }

        if (digit1[2])
        {
            doHwStatus.bit.do1 = digit2[2];
        }
    }
#endif

#if DSP_2803X
    setDOStatus();
#endif

#endif
}

#if DEBUG_F_TEMPERATURE

void TemperatureDeal(void)
{

    if (!funcCode.code.motorOtMode)
    {
        motorForeOT = 0;
        motorOT = 0;

        tickerTempDeal = 0;
        tempSampleSum = 0;
        temperature = 0;

        if (ERROR_MOTOR_OT == errorOther)
        {
            errorOther = ERROR_NONE;
        }
        return;
    }

    TemperatureSensorDeal();

    TemperatureAfterDeal();
}

void TemperatureSensorDeal()
{

    if (FUNCCODE_tempSenorType_NONE == funcCode.code.motorOtMode)
    {
        return;
    }

    tempSampleSum += aiDeal[2].sample;

    tickerTempDeal++;
    if (tickerTempDeal >= TEMPERATURE_CALC_PERIOD / TEMPERATURE_CALL_PERIOD)
    {
        Uint32 k;
        LINE_STRUCT aiLine = LINE_STRTUCT_DEFALUTS;

        Uint16 *p = &funcCode.code.aiCalibrateCurve[2].before1;
#define MOTOR_T_SENSOR_AI_MAX_VOLTAGE_IDEA  3300

        k = (4095L << 4) * tickerTempDeal / MOTOR_T_SENSOR_AI_MAX_VOLTAGE_IDEA;
        temperatureVoltageOrigin = tempSampleSum / k;

        aiLine.mode = 1;
        aiLine.y1 = ((int32)(int16)(*(p + 0)) * 0x7FFF) / MOTOR_T_SENSOR_AI_MAX_VOLTAGE_IDEA;
        aiLine.x1 = ((int32)(int16)(*(p + 1)) * 0x7FFF) / MOTOR_T_SENSOR_AI_MAX_VOLTAGE_IDEA;
        aiLine.y2 = ((int32)(int16)(*(p + 2)) * 0x7FFF) / MOTOR_T_SENSOR_AI_MAX_VOLTAGE_IDEA;
        aiLine.x2 = ((int32)(int16)(*(p + 3)) * 0x7FFF) / MOTOR_T_SENSOR_AI_MAX_VOLTAGE_IDEA;
        aiLine.x = ((int32)temperatureVoltageOrigin * 0x7FFF) / MOTOR_T_SENSOR_AI_MAX_VOLTAGE_IDEA;
        aiLine.calc(&aiLine);
        if (aiLine.y < 0)
        {
            aiLine.y =0;
        }

        temperatureVoltage = aiLine.y * MOTOR_T_SENSOR_AI_MAX_VOLTAGE_IDEA / 0x7FFF;

        GetTemperature();

        tickerTempDeal = 0;
        tempSampleSum = 0;

        aiDeal[2].voltageOrigin = temperatureVoltageOrigin;
        aiDeal[2].voltage = temperatureVoltage/10;
    }
}

void GetTemperature()
{
    const Uint16 *p;
    Uint16 size;

    if (FUNCCODE_tempSenorType_PTC100 == funcCode.code.motorOtMode)
    {
        p = voltageTempPT100;
        size = sizeof(voltageTempPT100);
    }
    else if (FUNCCODE_tempSenorType_PTC1000 == funcCode.code.motorOtMode)
    {
        p = voltageTempPT1000;
        size = sizeof(voltageTempPT1000);
    }

    temperature = GetTemperatureCalc(temperatureVoltage, p, size);
}

Uint16 GetTemperatureCalc(Uint16 voltage, const Uint16 *p, Uint16 len)
{
    int16 i;
    LINE_STRUCT temperatureLine = LINE_STRTUCT_DEFALUTS;

    for (i = 1; i < len; i++)
    {
        if (voltage < *(p + i))
            break;
    }
    if (i >= len)
    {
        i = len - 1;
    }

    temperatureLine.mode = 0;
    temperatureLine.x1 = *(p + i - 1);
    temperatureLine.y1 = (i - 1) * 4;
    temperatureLine.x2 = *(p + i);
    temperatureLine.y2 = i * 4;
    temperatureLine.x = voltage;
    temperatureLine.calc(&temperatureLine);

    return (temperatureLine.y);
}

void TemperatureAfterDeal(void)
{

    if (temperature >= funcCode.code.motorOtCoef)
    {
        motorForeOT = 1;
    }
    else
    {
        motorForeOT = 0;
    }

    if (temperature >= funcCode.code.motorOtProtect)
    {
        motorOT = 1;
    }
    else
    {
        motorOT = 0;
    }

    if (motorOT)
    {
        if (!errorOther)
        {
            errorOther = ERROR_MOTOR_OT;
        }
    }
}
#else
void TemperatureDeal(void)
{
}
#endif

void AiCalc(void)
{
#if DEBUG_F_AI
    LINE_STRUCT aiLine = LINE_STRTUCT_DEFALUTS;
    Uint16 * pAiSetCurve[5] =
    {
        &funcCode.code.curveSet2P1[0],
        &funcCode.code.curveSet2P2[0],
        &funcCode.code.curveSet2P3[0],
        &funcCode.code.curveSet4P1[0],
        &funcCode.code.curveSet4P2[0],
    };
    Uint16 aiFilterTime[AI_NUMBER];
    Uint16 aiNum;
    int32 point;
    int32 range;
    Uint16 *p = &funcCode.code.curveSet2P1[0];
    int16 i;
    int16 j;
    int32 aiMin;

    if (1 == mainLoopTicker)
    {
        aiLpf[0].out = aiDeal[0].sample;
        aiLpf[1].out = aiDeal[1].sample;
        aiLpf[2].out = aiDeal[2].sample;

        mainLoopTicker = 2;
    }

#if F_DEBUG_RAM
#if 0
#define AI1_NUMBER  256
    static Uint16 ai1Array[AI1_NUMBER];
    Uint32 ai1Sum;
#endif

#if 0
#define AI1_RESULT  (AdcRegs.ADCRESULT1)
#define AI2_RESULT  (AdcRegs.ADCRESULT3)
#define AI3_RESULT  (AdcRegs.ADCRESULT5)
    aiDeal[0].sample = AI1_RESULT;
    aiDeal[1].sample = AI2_RESULT;
    aiDeal[2].sample = AI3_RESULT;
#endif
#if 0
    ai1Sum = 0;
    for (i = AI1_NUMBER-1; i > 0; i--)
    {
        ai1Array[i] = ai1Array[i - 1];
        ai1Sum += ai1Array[i];
    }
    ai1Array[0] = aiDeal[0].sample;
    ai1Sum += ai1Array[0];
    aiDeal[0].sample = ai1Sum / AI1_NUMBER;
#endif
#endif

    aiFilterTime[0] = funcCode.code.ai1FilterTime;
    aiFilterTime[1] = funcCode.code.ai2FilterTime;
    aiFilterTime[2] = funcCode.code.ai3FilterTime;

    aiMin = (int32)AI_MIN_VOLTAGE_HW*10;
    for (j = AI_NUMBER - 1; j >= 0; j--)

    {

        if ((j == AI_NUMBER - 1)
            && (FUNCCODE_tempSenorType_NONE != funcCode.code.motorOtMode)
            )
        {
            aiMin = 0;
            continue;
        }

        aiLpf[j].t = aiFilterTime[j] * (TIME_UNIT_AI_PULSE_IN_FILTER / AI_CALC_PERIOD);
        aiLpf[j].in = aiDeal[j].sample;
        aiLpf[j].calc(&aiLpf[j]);

        aiLine.mode = 0;

        aiLine.x1 = 0;
        aiLine.y1 = aiMin;
        aiLine.x2 = (4095L << 4);
        aiLine.y2 = (int32)AI_MAX_VOLTAGE_HW*10;
        aiLine.x = aiLpf[j].out;
        aiLine.calc(&aiLine);
        aiDeal[j].voltageOrigin = aiLine.y;
        aiMin = 0;

#define SIZE_AI_CALIBRATE   sizeof(struct ANALOG_CALIBRATE_CURVE)
        p = (&funcCode.code.aiCalibrateCurve[0].before1 + SIZE_AI_CALIBRATE * j);
#undef SIZE_AI_CALIBRATE
        aiLine.mode = 1;
        aiLine.y1 = (int16)(*(p + 0));
        aiLine.x1 = (int16)(*(p + 1));
        aiLine.y2 = (int16)(*(p + 2));
        aiLine.x2 = (int16)(*(p + 3));
        aiLine.x = aiDeal[j].voltageOrigin;
        aiLine.calc(&aiLine);

        if (aiDeal[j].voltageOrigin == 0)
        {
            aiDeal[j].voltage = 0;
        }
        else
        {
            aiDeal[j].voltage = aiLine.y / 10;
        }

        aiNum = (funcCode.code.aiCurveSrc >> (j * 4)) & 0x000F;
        p = pAiSetCurve[aiNum - 1];
        if (aiNum >= 4)
        {

            for (i = 2; i >= 0; i--)
            {
                if (aiDeal[j].voltage  >= (int16)(*(p + i*2)))
                {
                    break;
                }
            }
            if (i < 0)
            {
                i = 0;
            }

            p += i * 2;
        }

        if((aiNum >= 4) && (i > 0))
        {
            aiLine.mode = 0;
        }
        else
        {
            if((funcCode.code.aiLimitSrc >> (j * 4)) & 0x000F)
            {
                aiLine.mode = 2;
            }
            else
            {
                aiLine.mode = 0;
            }
        }

        aiLine.x1 = ((int32)(int16)(*(p + 0)) * 33553)>>10;
        aiLine.y1 = ((int32)(int16)(*(p + 1)) * 33553)>>10;
        aiLine.x2 = ((int32)(int16)(*(p + 2)) * 33553)>>10;
        aiLine.y2 = ((int32)(int16)(*(p + 3)) * 33553)>>10;

        aiLine.x = aiDeal[j].voltage * 33553>>10;
        aiLine.calc(&aiLine);
        aiDeal[j].set = aiLine.y;

#define SIZE_AI_JUMP sizeof(struct AI_JUMP)
        p = (&funcCode.code.aiJumpSet[0].point + SIZE_AI_JUMP * j);
        point = (int32)(int16)(*(p + 0)) * 33553 >> 10;
        range = (int32)(int16)(*(p + 1)) * 33553 >> 10;
        if ((point - range < aiLine.y) &&
            (aiLine.y < point + range)
            )
        {
            aiDeal[j].set = point;
        }
#undef SIZE_AI_JUMP
    }

#endif
}

void AoCalcChannel(Uint16 channel)
{
#if DEBUG_F_AO
    int32 outCoeff;
    Uint32 tmpAo;

#if DSP_2803X
	volatile struct EPWM_REGS *EPwmRegs  = &EPwm6Regs;
#else
    volatile struct ECAP_REGS *pECapRegs = &ECap3Regs;
#endif

    LINE_STRUCT aoLine = LINE_STRTUCT_DEFALUTS;
    struct ANALOG_CALIBRATE_CURVE *pAo = &funcCode.code.aoCalibrateCurve[0];

    if (AOFMP_AO1 == channel)
    {
        ;
    }
    else
    {
#if DSP_2803X
        EPwmRegs =  &EPwm4Regs;
#else
        pECapRegs = &ECap2Regs;
#endif
    }

#if DEBUG_F_PLC_CTRL
    if (funcCode.code.plcEnable
        && (AOFMP_AO1 == channel)
        && (funcCode.code.outPortControl >= 10000)
        )
    {
        aoFmpValue = funcCode.code.ao1Value;
        aoFmpMax = 1000;
    }
    else
#endif
    {
        UpdateFmpAoValue(funcCode.code.aoFunc[channel], channel);
    }
    outCoeff = (aoFmpValue << 15) / aoFmpMax;

    outCoeff = (outCoeff * 1000) / 1047;

    outCoeff = ((outCoeff * (int32)(int16)funcCode.code.aoPara[channel-1].gain) * 327 >> 15)
        + (((int32)(int16)funcCode.code.aoPara[channel-1].offset * 2097) >> 6);

    if (outCoeff < 0)
    {
        outCoeff = 0;
    }
    else if (outCoeff > (Uint32)(1.5*32768))
    {
        outCoeff = (Uint32)(1.5*32768);
    }

    pAo = &funcCode.code.aoCalibrateCurve[channel-1];
    aoLine.mode = 1;
    aoLine.x1 = ((int32)(int16)pAo->after1  * 13421 ) >> 12;
    aoLine.y1 = ((int32)(int16)pAo->before1 * 13421 ) >> 12;
    aoLine.x2 = ((int32)(int16)pAo->after2  * 13421 ) >> 12;
    aoLine.y2 = ((int32)(int16)pAo->before2 * 13421 ) >> 12;
    aoLine.x = outCoeff;
    aoLine.calc(&aoLine);
    outCoeff = aoLine.y;

    if (outCoeff < 0)
    {
        outCoeff = 0;
    }
    else if (outCoeff >= (1U << 15))
    {
        outCoeff = (1U << 15) + 100;
    }

    tmpAo = (outCoeff * (AO_PWM_PERIOD >> 3)) >> (15 - 3);

#if DSP_2803X
    EPwmRegs->TBPRD = AO_PWM_PERIOD;
    EPwmRegs->CMPA.half.CMPA  = tmpAo;
#else
    pECapRegs->CAP1 = AO_PWM_PERIOD;
    pECapRegs->CAP4 = tmpAo;
#endif

#endif
}

void PulseInCalc(void)
{
#if DEBUG_F_HDI
    LINE_STRUCT pulseInLine = LINE_STRTUCT_DEFALUTS;

    static Uint16 pulseCounterRemainder;
#if 1
    static Uint32 lineSpeedCounter;
    static Uint16 lineSpeedCounterRemainder;
    static Uint32 lineSpeedTicker;
#endif
    static Uint16 lengthTickerInOld;
    static Uint16 counterTickerInOld;
    Uint16 pulseCounter;
    Uint16 frq1;
    Uint16 captureTickerTmp = captureTicker;

    Uint16 counterTickerOld;
    Uint16 counterTickerDelta;
    Uint16 lengthCurrentOld;
    Uint16 lengthTickerDelta;

    if (!captureTicker)
    {
        ++noCaptureTicker;
    }

    if (noCaptureTicker)
    {
        frq1 = ((TIME_UNIT_MS_PER_SEC << 1) / (noCaptureTicker * PULSE_IN_CALC_PERIOD) + 1) >> 1;

        if (pulseInFrq1 > frq1)
        {
            pulseInFrq1 = frq1;
        }
    }

    if (captureTicker)
    {
#if (DSP_CLOCK == 100)
        pulseInFrq1 = ((((100UL * 1000000UL) << 5) / capturePeriodSum) * captureTicker + (1 << 4)) >> 5;
#elif (DSP_CLOCK == 60)
        pulseInFrq1 = ((((60UL * 1000000UL) << 5) / capturePeriodSum) * captureTicker + (1 << 4)) >> 5;
#endif

        capturePeriodSum = 0;
        captureTicker = 0;

        noCaptureTicker = 0;
    }

#if (TIME_UNIT_MS_PER_SEC / PULSE_IN_ZERO > 1)
    if (pulseInFrq1 < TIME_UNIT_MS_PER_SEC / PULSE_IN_ZERO)
    {
        pulseInFrq1 = 0;
    }
#endif

#if 1
    pulseCounter = ((Uint32)pulseInFrq1 * PULSE_IN_CALC_PERIOD + pulseCounterRemainder) / TIME_UNIT_MS_PER_SEC;
    pulseCounterRemainder = ((Uint32)pulseInFrq1 * PULSE_IN_CALC_PERIOD + pulseCounterRemainder) % TIME_UNIT_MS_PER_SEC;

    if (pulseInFrq1 > COUNTER_IN_USE_CAPTURE_TICKER_LIMIT)
    {
        counterTickerDelta = pulseCounter;
        lengthTickerDelta = pulseCounter;
    }
    else
    {
        counterTickerDelta = captureTickerTmp;
        lengthTickerDelta = captureTickerTmp;
    }
#endif

    pulseInLpf.t = (int32)funcCode.code.pulseInFilterTime * (TIME_UNIT_AI_PULSE_IN_FILTER / PULSE_IN_CALC_PERIOD);
    pulseInLpf.in = pulseInFrq1;

    pulseInLpf.calc(&pulseInLpf);
    pulseInFrq = pulseInLpf.out;

    pulseInLine.x1 = (int32)(int16)funcCode.code.curveSet2P4[0] * 10 / 2;
    pulseInLine.x2 = (int32)(int16)funcCode.code.curveSet2P4[2] * 10 / 2;
    pulseInLine.y1 = ((int32)(int16)funcCode.code.curveSet2P4[1] * 33553) >> 10;
    pulseInLine.y2 = ((int32)(int16)funcCode.code.curveSet2P4[3] * 33553) >> 10;

    pulseInLine.x = pulseInFrq / 2;
    pulseInLine.calc(&pulseInLine);
    pulseInSet = pulseInLine.y;

#if 0
    pulseCounter = ((Uint32)pulseInFrq * PULSE_IN_CALC_PERIOD + pulseCounterRemainder) / TIME_UNIT_MS_PER_SEC;
    pulseCounterRemainder = ((Uint32)pulseInFrq * PULSE_IN_CALC_PERIOD + pulseCounterRemainder) % TIME_UNIT_MS_PER_SEC;

    if (pulseInFrq > COUNTER_IN_USE_CAPTURE_TICKER_LIMIT)
    {
        counterTickerDelta = pulseCounter;
        lengthTickerDelta = pulseCounter;
    }
    else
    {
        counterTickerDelta = captureTickerTmp;
        lengthTickerDelta = captureTickerTmp;
    }
#endif

#if 1
#define LINE_SPEED_CALC_PERIOD  100
    lineSpeedCounter += pulseCounter;
    if (++lineSpeedTicker >= LINE_SPEED_CALC_PERIOD / PULSE_IN_CALC_PERIOD)
    {
        lineSpeed = (lineSpeedCounter * 10 * 60 * 10 + lineSpeedCounterRemainder) / funcCode.code.lengthPulsePerMeter;
        lineSpeedCounterRemainder = (lineSpeedCounter * 10 * 60 * 10 + lineSpeedCounterRemainder)
            % funcCode.code.lengthPulsePerMeter;

        lineSpeedCounter = 0;
        lineSpeedTicker = 0;
    }
#endif

    if (DI_FUNC_COUNTER_TICKER_IN != funcCode.code.diFunc[4])
    {
        if (counterTickerInOld != diFunc.f1.bit.counterTickerIn)
        {
            counterTickerDelta = diFunc.f1.bit.counterTickerIn;

            counterTickerInOld = diFunc.f1.bit.counterTickerIn;
        }
        else
        {
            counterTickerDelta = 0;
        }
    }

    counterTickerOld = funcCode.code.counterTicker;
    funcCode.code.counterTicker += counterTickerDelta;
    bOverFlowCounterTicker = 0;
    if (diFunc.f1.bit.resetCounter)
    {
        funcCode.code.counterTicker = 0;
    }
    else if (funcCode.code.counterTicker < counterTickerOld)
    {
        bOverFlowCounterTicker = 1;
    }

    if (DI_FUNC_LENGTH_TICKER_IN != funcCode.code.diFunc[4])
    {
        if (lengthTickerInOld != diFunc.f1.bit.lengthTickerIn)
        {
            lengthTickerDelta = diFunc.f1.bit.lengthTickerIn;

            lengthTickerInOld = diFunc.f1.bit.lengthTickerIn;
        }
        else
        {
            lengthTickerDelta = 0;
        }
    }

    lengthCurrentOld = funcCode.code.lengthCurrent;
    funcCode.code.lengthCurrent += ((Uint32)lengthTickerDelta * 10 + funcCode.code.lengthTickerRemainder)
        / funcCode.code.lengthPulsePerMeter;
    funcCode.code.lengthTickerRemainder = ((Uint32)lengthTickerDelta * 10 + funcCode.code.lengthTickerRemainder)
        % funcCode.code.lengthPulsePerMeter;
    bOverFlowLengthCurrent = 0;
    if (diFunc.f1.bit.resetLengthCounter)
    {
        funcCode.code.lengthCurrent = 0;
        funcCode.code.lengthTickerRemainder = 0;
    }
    else if (funcCode.code.lengthCurrent < lengthCurrentOld)
    {
        bOverFlowLengthCurrent = 1;
    }
#endif
}

void PulseInSample(void)
{
#if DEBUG_F_HDI
    Uint32 ts1, ts2, ts3, ts4;
#if DSP_2803X
    volatile struct ECAP_REGS *pECapRegs = &ECap1Regs;
#else
    volatile struct ECAP_REGS *pECapRegs = &ECap4Regs;
#endif
    ts1 = pECapRegs->CAP1;
    if (ts1)
    {
        pECapRegs->CAP1 = 0;
        captureTicker++;
    }

    ts2 = pECapRegs->CAP2;
    if (ts2)
    {
        pECapRegs->CAP2 = 0;
        captureTicker++;
    }

    ts3 = pECapRegs->CAP3;
    if (ts3)
    {
        pECapRegs->CAP3 = 0;
        captureTicker++;
    }

    ts4 = pECapRegs->CAP4;
    if (ts4)
    {
        pECapRegs->CAP4 = 0;
        captureTicker++;
    }

    capturePeriodSum += ts1 + ts2 + ts3 + ts4;
#endif
}

#if DSP_2803X
#define HDO_PRD     (EPwm5Regs.TBPRD)
#define HDO_CMP     (EPwm5Regs.CMPB)
#define HDO_CTR     (EPwm5Regs.TBCTR)
#endif

void FMPDeal(void)
{
#if (DEBUG_F_AO || DEBUG_F_HDO || DEBUG_F_DO)

#if DSP_2803X
    static Uint16 fmOutSelectOld;

    if (fmOutSelectOld != funcCode.code.fmOutSelect)
    {
        fmOutSelectOld = funcCode.code.fmOutSelect;
        EPwm5Regs.TBCTL.bit.HSPCLKDIV = TB_DIV1;
        EPwm5Regs.TBCTL.bit.CLKDIV = TB_DIV1;
        HDO_CTR = 0;
    }

    if (FUNCCODE_fmOutSelect_PULSE == funcCode.code.fmOutSelect)
    {
        PulseOutCalc();
    }
    else if (FUNCCODE_fmOutSelect_DO == funcCode.code.fmOutSelect)
    {
        HDO_CTR = 0;
        HDO_PRD = PULSE_OUT_CTR_PERIOD;

        if (doStatus.c.bit.do3)
        {
            HDO_CMP = PULSE_OUT_CTR_PERIOD + 1;
        }
        else
        {
            HDO_CMP = 0;
        }
    }
#if 0
    else
    {
        AoCalcChannel(AOFMP_AO2);
    }
#endif
#else
    static Uint16 fmOutSelectOld;

    if (fmOutSelectOld != funcCode.code.fmOutSelect)
    {
        ECap2Regs.TSCTR = 0;
        fmOutSelectOld = funcCode.code.fmOutSelect;
    }

    if (FUNCCODE_fmOutSelect_PULSE == funcCode.code.fmOutSelect)
    {
        PulseOutDeal();
    }
    else if (FUNCCODE_fmOutSelect_DO == funcCode.code.fmOutSelect)
    {

        ECap2Regs.TSCTR = 0;
        ECap2Regs.CAP1 = PULSE_OUT_CTR_PERIOD;

        if (doStatus.c.bit.do3)
        {
            ECap2Regs.CAP2 = PULSE_OUT_CTR_PERIOD + 1;
        }
        else
        {
            ECap2Regs.CAP2 = 0;
        }
    }
    else
    {
        AoCalcChannel(AOFMP_AO2);
    }
#endif
#endif
}

void PulseOutCalc(void)
{
#if DEBUG_F_HDO
#if DSP_2803X
    static Uint16 periodOld = MAX_UINT16;
    static Uint16 beforeFrq = 200;
    static Uint16 CLOCK_BENCH = 1;
    Uint16 period;
    Uint64 tmp;
    Uint16 newCompare;
    Uint16 oldCompare;
    Uint16 HDOCTR;

    Uint16 curFrq;

#if DEBUG_F_PLC_CTRL
    if (funcCode.code.plcEnable
        && ((funcCode.code.outPortControl >= 11000)
            || ((funcCode.code.outPortControl < 10000) && (funcCode.code.outPortControl >= 1000)))
        )
    {
        aoFmpMax = 1000;
        aoFmpValue = funcCode.code.fmpValue;
    }
    else
#endif
    {
        UpdateFmpAoValue(funcCode.code.aoFunc[AOFMP_FMP], AOFMP_FMP);
    }
    curFrq = aoFmpValue * funcCode.code.fmpOutMaxFrq / aoFmpMax;
    HDOCTR = HDO_CTR;

    if ((curFrq >= 200) && (beforeFrq < 200))
    {

        if (HDOCTR > 292)
        {
            HDOCTR = 0xFFFF;
        }
        else
        {
            HDOCTR = HDOCTR * 224;
        }

        if (periodOld > 292)
        {
            periodOld = 0xFFFF;
        }
        else
        {
            periodOld = periodOld * 224;
        }

        CLOCK_BENCH = 1;
    }
    else if ((curFrq < 200) && (beforeFrq >= 200))
    {

        HDOCTR = HDOCTR / 224;
        periodOld = periodOld / 224;

        CLOCK_BENCH = 224;
    }

    tmp  = (Uint64)(DSP_CLOCK/10) * 1000000UL * aoFmpMax / (aoFmpValue * funcCode.code.fmpOutMaxFrq);
    tmp /= CLOCK_BENCH;
    if ((tmp > MAX_UINT16) || (!aoFmpValue))
    {
        tmp = MAX_UINT16;
    }
    period = tmp;

    if (period)
        period--;

    oldCompare = periodOld >> 1;
    newCompare = period >> 1;

    if (((MAX_UINT16 - 1) != period) && period)
    {

        if (period != HDO_PRD)
        {

            if (periodOld < period)
            {
                if (HDOCTR > oldCompare)
                {
                    HDOCTR = HDOCTR - oldCompare + newCompare;
                }
            }

            else
            {

                if (HDOCTR > newCompare + oldCompare)
                {

                    HDOCTR = 0;
                }

                else if (HDOCTR > oldCompare)
                {

                    HDOCTR = HDOCTR - oldCompare + newCompare;
                }

                else if (HDOCTR > newCompare)
                {

                    HDOCTR = newCompare;
                }

            }

            if (HDOCTR > period)
	        {
	            HDOCTR = 0;
	        }

	        DINT;
            EALLOW;
			if (CLOCK_BENCH == 1)
            {
                EPwm5Regs.TBCTL.bit.HSPCLKDIV = TB_DIV1;
                EPwm5Regs.TBCTL.bit.CLKDIV = TB_DIV1;
            }
            else
            {
                EPwm5Regs.TBCTL.bit.HSPCLKDIV = 7;
                EPwm5Regs.TBCTL.bit.CLKDIV = 4;
            }
            HDO_PRD = period;
			HDO_CMP = newCompare;
			HDO_CTR = HDOCTR;

            EDIS;
	        EINT;
        }

    }

    else
    {
        HDO_CTR = 0;
        HDO_PRD = PULSE_OUT_CTR_PERIOD;

        if (!period)
        {
            HDO_CMP = PULSE_OUT_CTR_PERIOD + 1;
        }
        else
        {
            HDO_CMP = 0;
        }
    }

    periodOld = period;
    beforeFrq = curFrq;
#endif
#endif
}

#if !DSP_2803X
void PulseOutDeal(void)
{
#if DEBUG_F_HDO
#if !DSP_2803X
    static Uint32 periodOld = MAX_UINT32;
    Uint32 period;
    Uint64 tmp;

    Uint32 newCompare;
    Uint32 oldCompare;

#if DEBUG_F_PLC_CTRL
    if (funcCode.code.plcEnable
        && ((funcCode.code.outPortControl >= 11000)
            || ((funcCode.code.outPortControl < 10000) && (funcCode.code.outPortControl >= 1000)))
        )
    {
        aoFmpMax = 1000;
        aoFmpValue = funcCode.code.fmpValue;
    }
    else
#endif
    {
        UpdateFmpAoValue(funcCode.code.aoFunc[AOFMP_FMP], AOFMP_FMP);
    }

    tmp  = (Uint64)(DSP_CLOCK/10) * 1000000UL * aoFmpMax / (aoFmpValue * funcCode.code.fmpOutMaxFrq);

    if ((tmp > MAX_UINT32) || (!aoFmpValue))
    {
        tmp = MAX_UINT32;
    }

    period = tmp;
    if (period)
        period--;

    oldCompare = periodOld >> 1;
    newCompare = period >> 1;

    if (((MAX_UINT32-1) != period) && period)
    {
        ECap2Regs.CAP2 = newCompare;
        ECap2Regs.CAP1 = period;

        if (periodOld < period)
        {
            DINT;

            if (ECap2Regs.TSCTR > oldCompare)
                ECap2Regs.TSCTR = ECap2Regs.TSCTR - oldCompare + newCompare;

            EINT;
        }
        else if (periodOld > period)
        {
            DINT;

            if (ECap2Regs.TSCTR > newCompare + oldCompare)
            {

                ECap2Regs.TSCTR = 0;
            }

            else if (ECap2Regs.TSCTR > oldCompare)
            {

                ECap2Regs.TSCTR = ECap2Regs.TSCTR - oldCompare + newCompare;
            }

            else if (ECap2Regs.TSCTR > newCompare)
            {

                ECap2Regs.TSCTR = newCompare;
            }

            EINT;
        }

        if (ECap2Regs.TSCTR > period)
        {
            ECap2Regs.TSCTR = 0;
        }
    }
    else
    {
        ECap2Regs.TSCTR = 0;
        ECap2Regs.CAP1 = PULSE_OUT_CTR_PERIOD;

        if (!period)
        {
            ECap2Regs.CAP2 = PULSE_OUT_CTR_PERIOD + 1;
        }
        else
        {
            ECap2Regs.CAP2 = 0;
        }
    }

    periodOld = period;
#endif
#endif
}
#endif

LOCALF void UpdateFmpAoValue(Uint16 func, Uint16 aoOrFmp)
{
#if (DEBUG_F_AO || DEBUG_F_HDO)
    switch (func)
    {
        case AO_FMP_FUNC_FRQ_SET:
            aoFmpValue = frqDisp;

            aoFmpMax = maxFrq;
            break;

        case AO_FMP_FUNC_FRQ_AIM:
            aoFmpValue = frqAim;
            aoFmpMax = maxFrq;
            break;

        case AO_FMP_FUNC_OUT_CURRENT:
            aoFmpValue = outCurrentDisp;
            aoFmpMax = motorFc.motorPara.elem.ratingCurrent << 1;
            break;

        case AO_FMP_FUNC_OUT_TORQUE:
            aoFmpValue = ABS_INT16((int16)itDisp);
            aoFmpMax = 2000;
            break;

        case AO_FMP_FUNC_OUT_POWER:
            aoFmpValue = outPower >> 1;
            aoFmpMax = motorFc.motorPara.elem.ratingPower;
            break;

        case AO_FMP_FUNC_OUT_VOLTAGE:
            aoFmpValue = outVoltage;
            aoFmpMax = (int32)(1.2*4096);
            break;

        case AO_FMP_FUNC_PULSE_IN:
            aoFmpValue = pulseInFrq / 2;
            aoFmpMax = 50000;
            break;

        case AO_FMP_FUNC_AI1:
        case AO_FMP_FUNC_AI2:
        case AO_FMP_FUNC_AI3:
            aoFmpValue = aiDeal[func - AO_FMP_FUNC_AI1].voltage;
            aoFmpMax = 1000;
            break;

        case AO_FMP_FUNC_LENGTH:
            aoFmpValue = funcCode.code.lengthCurrent;
            aoFmpMax = funcCode.code.lengthSet;
            break;

        case AO_FMP_FUNC_COUNTER:
            aoFmpValue = funcCode.code.counterTicker;
            aoFmpMax = funcCode.code.counterSet;
            break;

        case AO_FMP_FUNC_COMM:
            aoFmpValue = aoComm[aoOrFmp];
            aoFmpMax = 0x7FFF;
            break;

        case AO_FMP_FUNC_SPEED:
            aoFmpValue = frqRun;
            aoFmpMax = maxFrq;
            break;

        case AO_FMP_FUNC_OUT_CURRENT_1:
            aoFmpValue = outCurrentDisp;
            aoFmpMax = 10000;
             break;

        case AO_FMP_FUNC_OUT_VOLTAGE_1:
            aoFmpValue = generatrixVoltage;
            aoFmpMax = 10000;
            break;

        case AO_FMP_FUNC_OUT_TORQUE_DIR:
            aoFmpValue = (int16)(itDisp) + 2000;
            aoFmpMax = 4000;
            break;

        default:
            break;
    }

    aoFmpValue = ABS_INT32(aoFmpValue);
    aoFmpMax = ABS_INT32(aoFmpMax);

    if (aoFmpValue > aoFmpMax)
    {
        aoFmpValue = aoFmpMax;
    }
#endif
}

void LineCalc(LINE_STRUCT *p)
{
#if 0
    if (p->x <= p->x1)
        p->y = p->y1;
    else if (p->x >= p->x2)
        p->y = p->y2;
    else
        p->y = ((((int32)(p->x - p->x1) << 15) / (p->x2 - p->x1)) * (p->y2 - p->y1)
                 + ((int32)p->y1 << 15) + (1L << 14)) >> 15;
#elif 1
    int32 tmp;
    tmp = ((((int32)(p->x - p->x1) << 15) / (p->x2 - p->x1)) * (p->y2 - p->y1)
                 + ((int32)p->y1 << 15) + (1L << 14)) >> 15;

    if((p->mode == 2) && (p->x < p->x1))
        p->y = 0;
    else if (p->x <= p->x1)
        p->y = p->y1;
    else if (p->x >= p->x2)
        p->y = p->y2;
    else
        p->y = tmp;

    if (p->mode == 1)
        p->y = tmp;
#elif 1
    int32 max, min;

    max = MAX(p->x1, p->x2);
    min = MIN(p->x1, p->x2);

    if (p->x <= min)
        p->y = p->y1;
    else if (p->x >= max)
        p->y = p->y2;
    else
        p->y = ((((int32)(p->x - p->x1) << 15) / (p->x2 - p->x1)) * (p->y2 - p->y1)
                 + ((int32)p->y1 << 15) + (1L << 14)) >> 15;
#endif
}

void LpfCalc(LowPassFilter *p)
{
#if 0
    int32 tmp;
    int16 t1 = p->t * TIME_UNIT_AI_PULSE_IN_FILTER;

    p->outOld = p->out;
    if (!p->t)
    {
        p->out = p->in;
    }
    else
    {
        tmp = ((int32)p->in - p->outOld + p->remainder) * AI_CALC_PERIOD;
        p->out = tmp / t1 + p->outOld;
        p->remainder = tmp % t1;
    }

#elif 1
    int32 tmp;
    int32 out;

    p->outOld = p->out;
    if (!p->t)
    {
        p->out = p->in;
    }
    else
    {
        tmp = ((int32)p->in - p->outOld + p->remainder) * AI_CALC_PERIOD;
        out = tmp / (p->t * 1) + p->outOld;

        p->out = tmp / (p->t * 1) + p->outOld;
        p->remainder = tmp % (p->t * 1);
    }
#endif
}

void InitSetEPWM4(void)
{
#if (DEBUG_F_AO)
#if DSP_2803X
    EALLOW;
    EPwm4Regs.TBPRD = 0;
    EPwm4Regs.CMPA.half.CMPA = 0;
    EPwm4Regs.TBPHS.all = 0;
    EPwm4Regs.TBCTR = 0;
    EPwm4Regs.TBCTL.bit.CTRMODE = TB_COUNT_UP;
    EPwm4Regs.TBCTL.bit.PHSEN = TB_DISABLE;
    EPwm4Regs.TBCTL.bit.PRDLD = TB_SHADOW;
    EPwm4Regs.TBCTL.bit.SYNCOSEL = TB_SYNC_DISABLE;
    EPwm4Regs.TBCTL.bit.HSPCLKDIV = TB_DIV1;
    EPwm4Regs.TBCTL.bit.CLKDIV = TB_DIV1;
    EPwm4Regs.CMPCTL.bit.SHDWAMODE = CC_SHADOW;
    EPwm4Regs.CMPCTL.bit.SHDWBMODE = CC_SHADOW;
    EPwm4Regs.CMPCTL.bit.LOADAMODE = CC_CTR_ZERO;
    EPwm4Regs.CMPCTL.bit.LOADBMODE = CC_CTR_ZERO;
    EPwm4Regs.AQCTLA.bit.ZRO = AQ_SET;
    EPwm4Regs.AQCTLA.bit.CAU = AQ_CLEAR;

    EDIS;
#endif
#endif
}

void InitSetEPWM5(void)
{
 #if ( DEBUG_F_HDO || DEBUG_F_DO)
 #if DSP_2803X
    EALLOW;
    EPwm5Regs.TBPRD = 0;
    EPwm5Regs.CMPB = 0;
    EPwm5Regs.TBPHS.all = 0;
    EPwm5Regs.TBCTR = 0;

    EPwm5Regs.TBCTL.bit.FREE_SOFT = 2;

    EPwm5Regs.TBCTL.bit.CTRMODE = TB_COUNT_UP;
    EPwm5Regs.TBCTL.bit.PHSEN = TB_DISABLE;
    EPwm5Regs.TBCTL.bit.PRDLD = TB_IMMEDIATE;
    EPwm5Regs.TBCTL.bit.SYNCOSEL = TB_SYNC_DISABLE;
    EPwm5Regs.TBCTL.bit.HSPCLKDIV = TB_DIV1;
    EPwm5Regs.TBCTL.bit.CLKDIV = TB_DIV1;
    EPwm5Regs.CMPCTL.bit.SHDWAMODE = CC_IMMEDIATE;
    EPwm5Regs.CMPCTL.bit.SHDWBMODE = CC_IMMEDIATE;
    EPwm5Regs.CMPCTL.bit.LOADAMODE = CC_CTR_ZERO;
    EPwm5Regs.CMPCTL.bit.LOADBMODE = CC_CTR_ZERO;

    EPwm5Regs.AQCTLB.bit.ZRO = AQ_CLEAR;
    EPwm5Regs.AQCTLB.bit.CBU = AQ_SET;

    EDIS;
#endif
#endif
}

void InitSetEPWM6(void)
{
#if (DEBUG_F_AO)
#if DSP_2803X
    EALLOW;
    EPwm6Regs.TBPRD = 0;
    EPwm6Regs.CMPA.half.CMPA = 0;
    EPwm6Regs.TBPHS.all = 0;
    EPwm6Regs.TBCTR = 0;
    EPwm6Regs.TBCTL.bit.CTRMODE = TB_COUNT_UP;
    EPwm6Regs.TBCTL.bit.PHSEN = TB_DISABLE;
    EPwm6Regs.TBCTL.bit.PRDLD = TB_SHADOW;
    EPwm6Regs.TBCTL.bit.SYNCOSEL = TB_SYNC_DISABLE;
    EPwm6Regs.TBCTL.bit.HSPCLKDIV = TB_DIV1;
    EPwm6Regs.TBCTL.bit.CLKDIV = TB_DIV1;
    EPwm6Regs.CMPCTL.bit.SHDWAMODE = CC_SHADOW;
    EPwm6Regs.CMPCTL.bit.SHDWBMODE = CC_SHADOW;
    EPwm6Regs.CMPCTL.bit.LOADAMODE = CC_CTR_ZERO;
    EPwm6Regs.CMPCTL.bit.LOADBMODE = CC_CTR_ZERO;
    EPwm6Regs.AQCTLA.bit.ZRO = AQ_SET;
    EPwm6Regs.AQCTLA.bit.CAU = AQ_CLEAR;

    EDIS;
#endif
#endif
}

#if !DSP_2803X
void InitSetEcap2(void)
{
#if (DEBUG_F_AO || DEBUG_F_HDO || DEBUG_F_DO)
#if !DSP_2803X

    ECap2Regs.ECCTL1.bit.FREE_SOFT = 2;
    ECap2Regs.ECCLR.all = 0xFFFF;
    ECap2Regs.ECCTL2.all = 0x0216;

    ECap2Regs.CAP1 = 0;
    ECap2Regs.CAP2 = 0;
#endif
#endif
}
#endif

#if !DSP_2803X
void InitSetEcap3(void)
{
#if DEBUG_F_AO
#if !DSP_2803X
    ECap3Regs.ECCTL1.bit.FREE_SOFT = 2;

    ECap3Regs.ECCLR.all = 0xFFFF;
    ECap3Regs.ECCTL2.all = 0x0216;

    ECap3Regs.CAP1 = 0;
    ECap3Regs.CAP2 = 0;
#endif
#endif
}
#endif

void InitSetEcap1(void)
{
#if DEBUG_F_HDI
#if DSP_2803X
    ECap1Regs.ECEINT.all = 0x0000;
    ECap1Regs.ECCLR.all = 0xFFFF;
    ECap1Regs.ECCTL1.all = 0x8000;
    ECap1Regs.ECCTL2.bit.TSCTRSTOP = EC_FREEZE;

    ECap1Regs.ECCTL1.all = 0x81AA;
    ECap1Regs.ECCTL2.all = 0x0096;
#endif
#endif
}

#if !DSP_2803X
void InitSetEcap4(void)
{
#if DEBUG_F_HDI || DEBUG_F_POSITION_CTRL
    ECap4Regs.ECEINT.all = 0x0000;
    ECap4Regs.ECCLR.all = 0xFFFF;
    ECap4Regs.ECCTL1.all = 0x8000;
    ECap4Regs.ECCTL2.bit.TSCTRSTOP = EC_FREEZE;

    ECap4Regs.ECCTL1.all = 0x81AA;
    ECap4Regs.ECCTL2.all = 0x0096;
#endif
}
#endif

void InitDIGpio(void)
{
#if DSP_2803X
#if DEBUG_F_DI
    EALLOW;
    GpioCtrlRegs.GPBMUX1.bit.GPIO42 = 0;
    GpioCtrlRegs.GPBDIR.bit.GPIO42 = 0;
    GpioDataRegs.GPBDAT.bit.GPIO42 = 0;

    GpioCtrlRegs.GPBMUX1.bit.GPIO43 = 0;
    GpioCtrlRegs.GPBDIR.bit.GPIO43 = 0;
    GpioDataRegs.GPBDAT.bit.GPIO43 = 0;

    GpioCtrlRegs.GPAMUX2.bit.GPIO27 = 0;
    GpioCtrlRegs.GPADIR.bit.GPIO27 = 0;
    GpioDataRegs.GPADAT.bit.GPIO27 = 0;

    GpioCtrlRegs.GPBMUX1.bit.GPIO40 = 0;
    GpioCtrlRegs.GPBDIR.bit.GPIO40 = 0;
    GpioDataRegs.GPBDAT.bit.GPIO40 = 0;

    GpioCtrlRegs.GPAMUX2.bit.GPIO25 = 0;
    GpioCtrlRegs.GPADIR.bit.GPIO25 = 0;
    GpioDataRegs.GPADAT.bit.GPIO25 = 0;

    GpioCtrlRegs.GPBMUX1.bit.GPIO44 = 0;
    GpioCtrlRegs.GPBDIR.bit.GPIO44 = 0;
    GpioDataRegs.GPBDAT.bit.GPIO44 = 0;

    GpioCtrlRegs.GPBMUX1.bit.GPIO35 = 0;
    GpioCtrlRegs.GPBDIR.bit.GPIO35 = 0;
    GpioDataRegs.GPBDAT.bit.GPIO35 = 0;

    GpioCtrlRegs.GPBMUX1.bit.GPIO36 = 0;
    GpioCtrlRegs.GPBDIR.bit.GPIO36 = 0;
    GpioDataRegs.GPBDAT.bit.GPIO36 = 0;

    GpioCtrlRegs.GPBMUX1.bit.GPIO38 = 0;
    GpioCtrlRegs.GPBDIR.bit.GPIO38 = 0;
    GpioDataRegs.GPBDAT.bit.GPIO38 = 0;
    EDIS;
#endif
#endif
}

void InitDOGpio(void)
{
#if DSP_2803X
#if DEBUG_F_DO
    EALLOW;

    GpioCtrlRegs.GPBMUX1.bit.GPIO34 = 0;
    GpioCtrlRegs.GPBDIR.bit.GPIO34 = 1;
    GpioDataRegs.GPBDAT.bit.GPIO34 = 1;

    GpioCtrlRegs.GPAMUX2.bit.GPIO24 = 0;
    GpioCtrlRegs.GPADIR.bit.GPIO24 = 1;
    GpioDataRegs.GPADAT.bit.GPIO24 = 1;

    GpioCtrlRegs.GPBMUX1.bit.GPIO41 = 0;
    GpioCtrlRegs.GPBDIR.bit.GPIO41 = 1;
    GpioDataRegs.GPBDAT.bit.GPIO41 = 1;

    GpioCtrlRegs.GPAMUX2.bit.GPIO26 = 0;
    GpioCtrlRegs.GPADIR.bit.GPIO26 = 1;
    GpioDataRegs.GPADAT.bit.GPIO26 = 1;

    GpioCtrlRegs.GPBMUX1.bit.GPIO37 = 0;
    GpioCtrlRegs.GPBDIR.bit.GPIO37 = 1;
    GpioDataRegs.GPBDAT.bit.GPIO37 = 1;

    EDIS;
#endif
#endif
}

void InitECap2Gpio(void)
{
#if (DEBUG_F_AO || DEBUG_F_HDO || DEBUG_F_DO)
#if !DSP_2803X
EALLOW;
   GpioCtrlRegs.GPAPUD.bit.GPIO7 = 1;
   GpioCtrlRegs.GPAQSEL1.bit.GPIO7 = 0;
   GpioCtrlRegs.GPAMUX1.bit.GPIO7 = 3;
EDIS;
#endif
#endif
}

void InitEPwm5Gpio(void)
{
#if (DEBUG_F_HDO || DEBUG_F_DO)
#if DSP_2803X
EALLOW;
   GpioCtrlRegs.GPAPUD.bit.GPIO9 = 1;
   GpioCtrlRegs.GPAQSEL1.bit.GPIO9 = 0;
   GpioCtrlRegs.GPAMUX1.bit.GPIO9 = 1;
   GpioCtrlRegs.GPADIR.bit.GPIO9 = 1;

EDIS;
#endif
#endif
}

void InitEPwm4Gpio(void)
{
#if (DEBUG_F_AO)
#if DSP_2803X
   EALLOW;
    GpioCtrlRegs.GPAPUD.bit.GPIO6 = 1;
    GpioCtrlRegs.GPAQSEL1.bit.GPIO6 = 0;
    GpioCtrlRegs.GPAMUX1.bit.GPIO6 = 1;
    GpioCtrlRegs.GPADIR.bit.GPIO6 = 1;
    EDIS;
#endif
#endif
}

void InitEPwm6Gpio(void)
{
#if (DEBUG_F_AO)
#if DSP_2803X
EALLOW;
   GpioCtrlRegs.GPAPUD.bit.GPIO10 = 1;
   GpioCtrlRegs.GPAQSEL1.bit.GPIO10 = 0;
   GpioCtrlRegs.GPAMUX1.bit.GPIO10 = 1;
   GpioCtrlRegs.GPADIR.bit.GPIO10 = 1;
EDIS;
#endif
#endif
}

void InitECap3Gpio(void)
{
#if DEBUG_F_AO
#if !DSP_2803X
EALLOW;
   GpioCtrlRegs.GPAPUD.bit.GPIO9 = 0;
   GpioCtrlRegs.GPAQSEL1.bit.GPIO9 = 0;
   GpioCtrlRegs.GPAMUX1.bit.GPIO9 = 3;
EDIS;
#endif
#endif
}

void InitECap1Gpio(void)
{
#if (DEBUG_F_HDI || DEBUG_F_DI)
#if DSP_2803X
   EALLOW;
   GpioCtrlRegs.GPAPUD.bit.GPIO19 = 0;
   GpioCtrlRegs.GPAQSEL2.bit.GPIO19 = 0;
   GpioCtrlRegs.GPAMUX2.bit.GPIO19 = 3;
   EDIS;
#endif
#endif
}

void InitECap4Gpio(void)
{
#if DEBUG_F_HDI || DEBUG_F_POSITION_CTRL
#if !DSP_2803X
    EALLOW;
    GpioCtrlRegs.GPAPUD.bit.GPIO11 = 0;
    GpioCtrlRegs.GPAQSEL1.bit.GPIO11 = 0;
    GpioCtrlRegs.GPAMUX1.bit.GPIO11 = 3;
    EDIS;
#endif
#endif
}

LOCALF void Di2Bin(void)
{
#if DEBUG_F_DI
    static Uint16 diLogicOld[2];
    static Uint16 diLogicAiAsDiOld;
    static Uint16 vdiSrcOld;
    static Uint16 vdiFcSetOld;

    if ((diLogicOld[0] != funcCode.code.diLogic[0]) ||
        (diLogicOld[1] != funcCode.code.diLogic[1]) ||
        (diLogicAiAsDiOld != funcCode.code.diLogicAiAsDi)
        )
    {
        diLogicOld[0] = funcCode.code.diLogic[0];
        diLogicOld[1] = funcCode.code.diLogic[1];
        diLogicAiAsDiOld = funcCode.code.diLogicAiAsDi;

        diLogic = FcDigit2Bin(funcCode.code.diLogic[0]) +
        ((FcDigit2Bin(funcCode.code.diLogic[1])) << 5) +
        ((Uint32)(FcDigit2Bin(funcCode.code.diLogicAiAsDi)) << (DI_NUMBER_PHSIC + DI_NUMBER_V));
    }

    if (vdiSrcOld != funcCode.code.vdiSrc)
    {
        vdiSrcOld = funcCode.code.vdiSrc;

        vdiSrc = FcDigit2Bin(funcCode.code.vdiSrc);
    }

    if (vdiFcSetOld != funcCode.code.vdiFcSet)
    {
        vdiFcSetOld = funcCode.code.vdiFcSet;

        vdiFcSet = FcDigit2Bin(funcCode.code.vdiFcSet);
    }
#endif
}

LOCALF void getDiHwStatus()
{
#if DEBUG_F_DI
#if DSP_2803X
	diHwStatus.all = 0;
    diHwStatus.bit.di1 |= GpioDataRegs.GPBDAT.bit.GPIO42;
    diHwStatus.bit.di2 |= GpioDataRegs.GPBDAT.bit.GPIO43;
    diHwStatus.bit.di3 |= GpioDataRegs.GPADAT.bit.GPIO27;
    diHwStatus.bit.di4 |= GpioDataRegs.GPBDAT.bit.GPIO40;
    diHwStatus.bit.di5 |= GpioDataRegs.GPADAT.bit.GPIO19;
    diHwStatus.bit.di6 |= GpioDataRegs.GPADAT.bit.GPIO25;
    diHwStatus.bit.di7 |= GpioDataRegs.GPBDAT.bit.GPIO44;
    diHwStatus.bit.di8 |= GpioDataRegs.GPBDAT.bit.GPIO35;
    diHwStatus.bit.di9 |= GpioDataRegs.GPBDAT.bit.GPIO36;
    diHwStatus.bit.di10 |= GpioDataRegs.GPBDAT.bit.GPIO38;
#endif
#endif
}

LOCALF void setDOStatus()
{
#if DEBUG_F_DO
#if DSP_2803X
EALLOW;

	doHwStatus.bit.do1 ? (GpioDataRegs.GPBCLEAR.bit.GPIO34 = 1): (GpioDataRegs.GPBSET.bit.GPIO34 = 1);
	doHwStatus.bit.do2 ? (GpioDataRegs.GPACLEAR.bit.GPIO24 = 1): (GpioDataRegs.GPASET.bit.GPIO24 = 1);

	doHwStatus.bit.fan ? (GpioDataRegs.GPBCLEAR.bit.GPIO37 = 1): (GpioDataRegs.GPBSET.bit.GPIO37 = 1);

	doHwStatus.bit.relay1 ? (GpioDataRegs.GPBCLEAR.bit.GPIO41 = 1): (GpioDataRegs.GPBSET.bit.GPIO41 = 1);
	doHwStatus.bit.relay2 ? (GpioDataRegs.GPACLEAR.bit.GPIO26 = 1): (GpioDataRegs.GPASET.bit.GPIO26 = 1);
EDIS;
#endif
#endif
}

LOCALF void Do2Bin(void)
{
#if DEBUG_F_DO
    static Uint16 doLogicOld;
    static Uint16 vdoLogicOld;

    if ((doLogicOld != funcCode.code.doLogic) ||
        (vdoLogicOld != funcCode.code.vdoLogic)
        )
    {
        doLogicOld = funcCode.code.doLogic;
        vdoLogicOld = funcCode.code.vdoLogic;

        doLogic = FcDigit2Bin(funcCode.code.doLogic) +
            ((FcDigit2Bin(funcCode.code.vdoLogic)) << 5);
    }
#endif
}
