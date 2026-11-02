
#include "MotorPublicCalInclude.h"

DC_BRAKE_STRUCT			gDCBrake;
BRAKE_CONTROL_STRUCT	gBrake;
SHORT_GND_STRUCT		gShortGnd;
JUDGE_POWER_LOW			gLowPower;

void RunCaseShortGnd(void)
{
	switch(gMainStatus.SubStep)
	{
		case 1:
            gMainStatus.PrgStatus.bit.PWMDisable = 1;
			gShortGnd.Comper = SHORT_GND_PERIOD;
			gShortGnd.ocFlag = 0;
			gShortGnd.BaseUDC = gUDC.uDC;
			gShortGnd.ShortCur = 0;

			EALLOW;
			EPwm1Regs.TBPRD = SHORT_GND_PERIOD;
			EPwm1Regs.CMPA.half.CMPA = SHORT_GND_PERIOD;
			EPwm1Regs.CMPB  = EPwm1Regs.TBPRD - gADC.DelayApply;
			EPwm2Regs.TBPRD = SHORT_GND_PERIOD;
			EPwm3Regs.TBPRD = SHORT_GND_PERIOD;

			EPwm1Regs.AQCSFRC.all = 0x08;

			EPwm2Regs.AQCSFRC.all = 0x0A;

			EPwm3Regs.AQCSFRC.all = 0x0A;

			EPwm1Regs.DBCTL.all = 0;
			EPwm2Regs.DBCTL.all = 0;
			EPwm3Regs.DBCTL.all = 0;

			EDIS;

			gMainStatus.SubStep = 2;
			break;

		case 2:
			gMainStatus.SubStep = 3;
			EnableDrive();
			break;

		case 3:
			if((gShortGnd.ocFlag != 0) ||
			  (abs(gShortGnd.ShortCur) > (30 * 32)) ||
			  (gUDC.uDC > gShortGnd.BaseUDC + 650))
			{

				DisableDrive();
				gError.ErrorCode.all |= ERROR_SHORT_EARTH;
				gMainStatus.SubStep = 4;
				break;
			}

			if(gShortGnd.Comper <= SHORT_GND_CMPR_INC)
			{
				gMainStatus.SubStep = 4;
			}
			else
			{
				gShortGnd.Comper -= SHORT_GND_CMPR_INC;
				EALLOW;
				EPwm1Regs.CMPA.half.CMPA = gShortGnd.Comper;
				EDIS;
			}
			break;

		case 4:
			DisableDrive();
			EALLOW;
			EPwm1Regs.DBCTL.all = 0x0007;
			EPwm2Regs.DBCTL.all = 0x0007;
			EPwm3Regs.DBCTL.all = 0x0007;

			EPwm1Regs.AQCSFRC.all = 0x0;
			EPwm2Regs.AQCSFRC.all = 0x0;
			EPwm3Regs.AQCSFRC.all = 0x0;

			EDIS;
			gMainStatus.StatusWord.bit.ShortGndOver = 1;
			gMainStatus.RunStep = STATUS_STOP;
            gMainStatus.PrgStatus.bit.PWMDisable = 0;
			gMainStatus.SubStep = 0;
			if(gMainStatus.ErrFlag.bit.OvCurFlag == 1)
			{
				gMainStatus.ErrFlag.bit.OvCurFlag = 0;
				EALLOW;
				EPwm1Regs.TZCLR.bit.INT = 1;
				EDIS;
			}
			break;

		default:
			break;
	}
}

void RunCaseLowPower(void)
{
	Uint uDCLowLimt;

	uDCLowLimt = gInvInfo.InvLowUDC + 200;
	if(INV_VOLTAGE_220V == gInvInfo.InvVoltageType)
	{
		uDCLowLimt = gInvInfo.InvLowUDC + 160;
	}

	switch(gMainStatus.SubStep)
	{
		case 2:
			if((gUDC.uDCFilter > uDCLowLimt) &&
			   (gUDC.uDCFilter <= gLowPower.UDCOld))
			{
				if((gLowPower.WaiteTime++) >= 100)
				{
					gExcursionInfo.EnableCount = 199;
					gMainStatus.SubStep ++;
					gLowPower.WaiteTime = 0;
				}
			}
			else
			{
				gLowPower.WaiteTime = 0;
				gLowPower.UDCOld = gUDC.uDCFilter;
			}

			break;

		case 3:
			if(((gLowPower.WaiteTime++) >= 100) &&
			   (gExcursionInfo.EnableCount >= 200))
			{
				gMainStatus.RunStep = STATUS_STOP;
				gMainStatus.SubStep = 1;
                gMainStatus.StatusWord.bit.LowUDC = 1;
				gMainStatus.StatusWord.bit.RunEnable = 1;
        		if((gError.ErrorCode.all & ERROR_LOW_UDC) == ERROR_LOW_UDC)
                {
                    gError.ErrorCode.all = 0;
		            if(gMainStatus.ErrFlag.bit.OvCurFlag == 1)
		            {
			            gMainStatus.ErrFlag.bit.OvCurFlag = 0;
			            EALLOW;
			            EPwm2Regs.TZCLR.bit.OST = 1;
			            EPwm3Regs.TZCLR.bit.OST = 1;
			            EPwm1Regs.TZCLR.bit.OST = 1;
			            EPwm1Regs.TZCLR.bit.INT = 1;
			            EDIS;
		            }
        		}
				ConnectRelay();
				gBuffResCnt += 30000;
				gPhase.IMPhase = GetTime() << 28;
			}
			break;

		default:
			DisConnectRelay();
            gError.ErrorCode.all |= ERROR_LOW_UDC;
        	gMainStatus.StatusWord.bit.LowUDC = 0;
			gMainStatus.StatusWord.bit.StartStop = 0;
			gLowPower.WaiteTime = 0;
			gLowPower.UDCOld = gUDC.uDCFilter;
			gMainStatus.SubStep = 2;
			break;
	}
}

void CBCLimitCurPrepare(void)
{
	EALLOW;
	if(EPwm1Regs.TZFLG.bit.CBC == 1)
	{
		EPwm1Regs.TZCLR.bit.CBC = 1;
		gCBCProtect.Flag.bit.CBC_U = 1;
	}

	if(EPwm2Regs.TZFLG.bit.CBC == 1)
	{
		EPwm2Regs.TZCLR.bit.CBC = 1;
		gCBCProtect.Flag.bit.CBC_V = 1;
	}

	if(EPwm3Regs.TZFLG.bit.CBC == 1)
	{
		EPwm3Regs.TZCLR.bit.CBC = 1;
		gCBCProtect.Flag.bit.CBC_W = 1;
	}
	EDIS;
}

Uint MaxUVWCurrent(void)
{
	Uint m_IU,m_IV,m_IW;

	m_IU = abs(gIUVWQ24.U>>12);
	m_IV = abs(gIUVWQ24.V>>12);
	m_IW = abs(gIUVWQ24.W>>12);

	m_IU = (m_IU >= m_IV)?m_IU:m_IV;
	m_IU = (m_IU >= m_IW)?m_IU:m_IW;

	return m_IU;
}

void HardWareErrorDeal()
{
#ifdef TMS320F28035
    int sum = 0, i;

    gMainStatus.ErrFlag.bit.OvCurFlag = 1;

    for(i = 0; i < 10; i++)
    {
        sum += GpioDataRegs.AIODAT.bit.AIO2;
    }
    if(sum < 5)
    {
        HardWareOverUDCDeal();
        return;
    }

#else
	gMainStatus.ErrFlag.bit.OvCurFlag = 1;
#endif

    if(gMainStatus.RunStep == STATUS_SHORT_GND)
	{
		gShortGnd.ocFlag = 1;
	}
	else if((gError.ErrorCode.all & ERROR_OVER_CURRENT) != ERROR_OVER_CURRENT)
	{
		gError.ErrorCode.all |= ERROR_OVER_CURRENT;
        gError.ErrorInfo[0].bit.Fault1 = 1;
		gLineCur.ErrorShow = MaxUVWCurrent();
	}

	EALLOW;
	EPwm1Regs.TZCLR.bit.OST = 1;
	EPwm2Regs.TZCLR.bit.OST = 1;
	EPwm3Regs.TZCLR.bit.OST = 1;
	EDIS;

    return;
}

void HardWareOverUDCDeal(void)
{

	DisableDrive();
	if(gMainStatus.RunStep == STATUS_SHORT_GND)
	{
		gShortGnd.ocFlag = 2;
	}
	else
	{
		gError.ErrorCode.all |= ERROR_OVER_UDC;
        gError.ErrorInfo[0].bit.Fault2 = 1;
	}

    EALLOW;
	EPwm1Regs.TZCLR.bit.OST = 1;
	EPwm2Regs.TZCLR.bit.OST = 1;
	EPwm3Regs.TZCLR.bit.OST = 1;
	EDIS;

}

void RunCaseDcBrake(void)
{
	int m_BrakeCur;

	if(gMainCmd.Command.bit.StartDC == 1)
	{

		m_BrakeCur = gComPar.StartDCBrakeCur * 41;
	}
	else if(gMainCmd.Command.bit.StopDC == 1)
	{

		m_BrakeCur = gComPar.StopDCBrakeCur * 41;
	}

	gDCBrake.Time++;
	if(gDCBrake.Time < 2)
	{
		gOutVolt.Volt = ((Ulong)m_BrakeCur * (Ulong)gMotorExtPer.R1)>>16;
		gDCBrake.PID.Total = ((long)gOutVolt.Volt<<16);
	}
	else
	{
		gDCBrake.Time = 10;
		gDCBrake.PID.Deta = m_BrakeCur - (int)gLineCur.CurPer;
        gDCBrake.PID.KP   = 1600/16;

		gDCBrake.PID.KI   = 300;
        if( 16 < gInvInfo.InvTypeApply )
        {
            gDCBrake.PID.KI = 150;
        }
        gDCBrake.PID.QP = 0;
        gDCBrake.PID.QI = 0;
		gDCBrake.PID.KD   = 0;
		gDCBrake.PID.Max  = 4096;
		gDCBrake.PID.Min  = 0;
		PID((PID_STRUCT *)&gDCBrake.PID);
		gOutVolt.Volt = gDCBrake.PID.Out>>16;
	}
	gOutVolt.VoltApply = gOutVolt.Volt;

}

void BrakeResControl(void)
{

    if(gComPar.BrakeCoff == 100)
    {
        gBrake.OnCnt = 65535;
        gBrake.OffCnt = 0;
    }
    else if(gComPar.BrakeCoff == 0)
    {
        gBrake.OnCnt = 0;
        gBrake.OffCnt = 65535;
    }
    else if(gComPar.BrakeCoff <= 50)
    {
        gBrake.OnCnt = 1;
        gBrake.OffCnt = (100 - gComPar.BrakeCoff) / gComPar.BrakeCoff;
    }
    else
    {
        gBrake.OnCnt = (gComPar.BrakeCoff) / (100 - gComPar.BrakeCoff);
        gBrake.OffCnt = 1;
    }

	if(gBrake.OnCnt == 0)
	{
		TurnOffBrake();
		return;
	}

    gBrake.VoltageLim = gVFPar.ovPoint * gVFPar.ovPointCoff;

	if(gUDC.uDC < (gBrake.VoltageLim + 90))
	{
		gBrake.Flag &= 0x0FFFC;
		gBrake.Cnt = 0;
	}
	else if((gBrake.Flag & 0x01) == 0)
	{
		if(gUDC.uDC > (gBrake.VoltageLim + 110))
		{
			gBrake.Flag |= 0x03;
			gBrake.Cnt = 0;
		}
	}
	else
	{
		gBrake.Cnt++;
		if((gBrake.Flag & 0x02) == 0)
		{
			if(gBrake.Cnt > gBrake.OffCnt)
			{
				gBrake.Flag |= 0x02;
				gBrake.Cnt = 0;
			}
		}
		else
		{
			if(gBrake.Cnt > gBrake.OnCnt)
			{
				gBrake.Flag &= 0x0FFFD;
				gBrake.Cnt = 0;
			}
		}
	}

	if((gBrake.Flag & 0x02) == 0x02)
	{
		TurnOnBrake();
	}
	else
	{
		TurnOffBrake();
	}
}
