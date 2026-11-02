
#include "MotorSpeedCheceInclude.h"

FEISU_STRUCT			gFeisu;
FEISU_STRUCT_NEW        gFeisuNew;
PID_STRUCT				gSpeedCheckPID;
PID_STRUCT				gSpeedCheckPID1;

void SpeedCheckUdcRiseDeal(void);
void SpeedCheckCycleOver(void);
void SpeedCheckSUB1(void);
void SpeedCheckCycleSUB2(void);
void PrepareForSpeedCheck(void);

void Cur_PID(void);
void Cur_PIDonefive(void);
void SaveTheta(void);

void PrepareForSpeedCheck(void)
{
    gFeisu.Case4Sig2.all = 0;
	gFeisu.Case4Sig1.all = 0;
	gFeisu.AlmostCNT     = 0;
	gFeisu.CycleCnt      = 0;
	gFeisu.GuoduCnt      = 0;
	gFeisu.LowFreqCNT    = 0;
	gFeisu.CurDelta      = 0;
	gFeisu.SpeedCheck    = 0;
	gFeisu.UdcOvCnt      = 0;
	gFeisu.UdcRiseCnt    = 0;
	gFeisu.VoltCheck     = 0;
	gFeisu.VoltCNT       = 0;
	gFeisu.UdcBak        = gUDC.uDC;
	gFeisu.CheckMode     = gComPar.SpdSearchMethod;
	gFeisu.SpeedMaxPu    = (((Ulong)gBasePar.MaxFreq)<<15)/gBasePar.FullFreq;
	gFeisu.Speed5hz      = (500L <<15)/gBasePar.FullFreq01;
    gIMTSetApply.T       = 0;
    gSpeedCheckPID.Total = 0;
	gSpeedCheckPID1.Total = 0;

    gFeisu.Case4Sig1.all = 0x00;
    gFeisu.Case4Sig1.bit.SpdSig = (gFeisu.SpeedLast >= 0) ? 1 : 0;
    gFeisu.CsrWtOver = 0;

	if(gFeisu.CheckMode == 2)
	{
		gFeisu.Case4Sig1.bit.CHECK_BIT15 = 1;

        gFeisu.SpeedCheck = (gFeisu.SpeedLast < 0) ? (-gFeisu.SpeedMaxPu) : gFeisu.SpeedMaxPu;
	}
	else if((gFeisu.CheckMode == 1) ||	(abs(gFeisu.SpeedLast) < gFeisu.Speed5hz))
	{

        gFeisu.SpeedCheck = (gFeisu.SpeedLast < 0) ? (-gFeisu.Speed5hz) : gFeisu.Speed5hz;
	}
	else
	{
		gFeisu.Case4Sig1.bit.CHECK_BIT14 = 1;
		gFeisu.SpeedCheck = gFeisu.SpeedLast;
	}
	gMainCmd.FreqSyn = gFeisu.SpeedCheck;
	gFeisu.VoltTemp = CalOutVotInVFStatus(gMainCmd.FreqSyn);

}

void RunCaseSpeedCheck(void)
{
	Uint m_UData;

	if((gMainCmd.Command.bit.Start == 0) ||
	   (gError.ErrorCode.all != 0))
	{
		gMainStatus.RunStep = STATUS_STOP;
		gMainStatus.SubStep = 1;
		gMainStatus.StatusWord.bit.StartStop = 0;
		DisableDrive();
		return;
	}
	gMainStatus.StatusWord.bit.StartStop = 1;

    if(ASYNC_SVC == gCtrMotorType)
    {

        gFeisu.VoltCheckAim = 4096;
    }
    else
    {
        gFeisu.VoltTemp = CalOutVotInVFStatus(gFeisu.SpeedCheck);
        CalTorqueUp();
        gFeisu.VoltTemp += gOutVolt.detVfVoltUp;
	    gFeisu.VoltCheckAim = (((Ulong)gMotorExtPer.R1 * 2048L)>>15) + gFeisu.VoltTemp;
        gComPar.SpdSearchTime = gComPar.SpdSearchTimeSet;

        if(gInvInfo.InvTypeApply > 13)
        {
           gComPar.SpdSearchTime = (long)gComPar.SpdSearchTimeSet * 10L/(gInvInfo.InvTypeApply + 10);
           if(gFeisu.CheckMode == 1)
            {
              gFeisu.VoltCheckAim = (((Ulong)gMotorExtPer.R1 * 5125L)>>15) + gFeisu.VoltTemp;
            }
        }
    }

	if(gFeisu.Case4Sig1.bit.CHECK_GET == 1)
	{
		if(gFeisu.VoltCheck > (gFeisu.VoltCheckAim + 3))
		{
			gFeisu.VoltCheck -= 2;
			gFeisu.GuoduCnt = 0;
		}
		else if(gFeisu.VoltCheck < (gFeisu.VoltCheckAim - 3))
		{
			gFeisu.VoltCheck += 2;
			gFeisu.GuoduCnt = 0;

		}
		gFeisu.GuoduCnt++;
		if(gFeisu.GuoduCnt > 400)
		{
			gMainStatus.RunStep = STATUS_RUN;
			gMainStatus.SubStep = 1;
			gMainStatus.StatusWord.bit.SpeedSearchOver = 1;
			gFeisu.Case4Sig1.all=0;
		}
		SpeedCheckSUB1();
		return;
	}

	gSpeedCheckPID.KP = 200;
	gSpeedCheckPID.KI = 400;
    gSpeedCheckPID.QP = 0;
    gSpeedCheckPID.QI = 0;
	gSpeedCheckPID.KD = 0;
	gSpeedCheckPID.Max = 4096;
	gSpeedCheckPID.Min = 0;
  #if 0
	if(gInvInfo.InvCurrent < gMotorInfo.Current)
	{
		gFeisu.CurDelta = ((Ulong)gInvInfo.InvCurrent * 5125L) / gMotorInfo.Current;
		gFeisu.CurDelta = (gFeisu.CurDelta > 5125)?5125:gFeisu.CurDelta;
	}
	else
  #endif
	{
		gFeisu.CurDelta = 5125;
	}
	gSpeedCheckPID.Deta = gFeisu.CurDelta - (int)gLineCur.CurPer;
	PID((PID_STRUCT *)&gSpeedCheckPID);
	gFeisu.VoltCheck = (gSpeedCheckPID.Out>>16);

	if(gFeisu.Case4Sig2.bit.DelayOver == 1)
	{
		gFeisu.VoltCNT++;
		if(gFeisu.VoltCNT > 100)
		{
			gFeisu.Case4Sig1.bit.CHECK_GET = 1;
		}
		SpeedCheckSUB1();
		return;
	}

	if(gFeisu.Case4Sig2.bit.AlmostOver == 1)
	{
		gFeisu.AlmostCNT++;
		if((gFeisu.AlmostCNT > 1000) ||
		   ( (gFeisu.Case4Sig2.bit.TwoCyclesOver == 0) &&
		     (gFeisu.VoltCheck < gFeisu.VoltCheckAim) ) )
		{
			gFeisu.Case4Sig2.bit.DelayOver = 1;
		}
		SpeedCheckSUB1();
		return;
	}
	gFeisu.AlmostCNT = 0;

	m_UData = abs(gFeisu.SpeedCheck)>>8;
	m_UData = m_UData + 512;
	m_UData = ((Ulong)m_UData * (Ulong)gFeisu.VoltCheck)>>9;
	if(m_UData > gFeisu.VoltCheckAim)
	{
		gFeisu.Case4Sig2.bit.AlmostOver =1;
		SpeedCheckSUB1();
		return;
	}

	gFeisu.LowFreqCNT=0;

	if(gFeisu.Case4Sig1.bit.UdcHigh == 0)
	{

		if(((gLineCur.CurPer + 1000) < gFeisu.CurDelta) &&
		    (gFeisu.CsrWtOver == 0))
		{
			gFeisu.Ger4A = 0;
		}
		else
		{
			gFeisu.Ger4A = gComPar.SpdSearchTime + 1;
            gFeisu.CsrWtOver = 1;
		}

		if(((gFeisu.Case4Sig1.all & 0x0C000) == 0) ||
		   ((gFeisu.Case4Sig1.all & 0x0C000) == 0x0C000))
		{
			if(gFeisu.Case4Sig1.bit.SpdSig == 0)
			{
				gFeisu.SpeedCheck -= gFeisu.Ger4A;
			}
			else
			{
				gFeisu.SpeedCheck += gFeisu.Ger4A;
			}

			if(abs(gFeisu.SpeedCheck) >= gFeisu.SpeedMaxPu)
			{
				if(gFeisu.CycleCnt == 0)
				{
					gFeisu.CycleCnt = 0x1111;
					gFeisu.Case4Sig1.all ^= 0x2000;
					if(gFeisu.Case4Sig1.bit.SpdSig == 1)
					{
						gFeisu.SpeedCheck = gFeisu.Speed5hz;
					}
					else
					{
						gFeisu.SpeedCheck = -gFeisu.Speed5hz;
					}
					SpeedCheckCycleSUB2();
				}
				else
				{
					SpeedCheckCycleOver();
				}
			}
		}

		else
		{
			if(gFeisu.Case4Sig1.bit.SpdSig == 1)
			{
				gFeisu.SpeedCheck -= gFeisu.Ger4A;
				if(gFeisu.SpeedCheck < 0)

				{
					if(gFeisu.CycleCnt == 0)
					{
						gFeisu.CycleCnt = 0x1111;
						gFeisu.Case4Sig1.bit.SpdSig = 0;
						gFeisu.SpeedCheck = -gFeisu.SpeedMaxPu;
						SpeedCheckCycleSUB2();
					}
					else
					{
						SpeedCheckCycleOver();
					}
				}
			}
			else
			{
				gFeisu.SpeedCheck += gFeisu.Ger4A;
				if(gFeisu.SpeedCheck > 0)

				{
					if(gFeisu.CycleCnt == 0)
					{
						gFeisu.CycleCnt = 0x1111;
						gFeisu.Case4Sig1.bit.SpdSig = 1;
						gFeisu.SpeedCheck = gFeisu.SpeedMaxPu;
						SpeedCheckCycleSUB2();
					}
					else
					{
						SpeedCheckCycleOver();
					}
				}
			}
		}

		gFeisu.UdcDelta = gUDC.uDCFilter - gFeisu.UdcOld;
		gFeisu.UdcOld   = gUDC.uDCFilter;
		if((gUDC.uDCFilter - 800) > gFeisu.UdcBak)
		{

			if((gFeisu.CheckMode != 1)||(gMainCmd.FreqReal> 1000))
			{
				gFeisu.Case4Sig1.bit.UdcHigh = 1;
				gFeisu.Ger4A = 400;
			}
			else
			{
    			gFeisu.Ger4A = 0;
			}
			SpeedCheckUdcRiseDeal();
			SpeedCheckSUB1();
			return;
		}
		else
		{
			if(gFeisu.UdcDelta > 0)
			{
				gFeisu.UdcRiseCnt++;
				if(gFeisu.UdcRiseCnt >= 1000)
				{
					gFeisu.Ger4A=(gComPar.SpdSearchTime << 3)+5;
					gFeisu.Case4Sig1.bit.UdcHigh=1;
					SpeedCheckUdcRiseDeal();
					SpeedCheckSUB1();
					return;
				}
				else
				{
					SpeedCheckSUB1();
					return;
				}
			}
			else
			{
				gFeisu.Case4Sig1.bit.UdcHigh=0;
				gFeisu.UdcRiseCnt=0;
				SpeedCheckSUB1();
				return;
			}
		}
	}
	else
	{
		gFeisu.UdcOvCnt ++;
		if(gFeisu.UdcOvCnt > 1)
		{
			gFeisu.UdcOvCnt = 0;
			gFeisu.UdcDelta = gUDC.uDCFilter - gFeisu.UdcOld;
			gFeisu.UdcOld   = gUDC.uDCFilter;
			if(gFeisu.UdcDelta > 0)
			{
				gFeisu.Ger4A = (gComPar.SpdSearchTime << 3)+5;
				SpeedCheckUdcRiseDeal();
			}
			else if(gUDC.uDCFilter - gFeisu.UdcBak <= 350)
			{
				gFeisu.Case4Sig2.bit.AlmostOver = 1;
			}
		}
		SpeedCheckSUB1();
		return;
	}
}

void SpeedCheckSUB1(void)
{

    gMainCmd.FreqSyn  = gFeisu.SpeedCheck;
	gMainCmd.FreqSet       = gFeisu.SpeedCheck;
	gMainCmd.FreqDesired   = gFeisu.SpeedCheck;

    if(IDC_VF_CTL == gMainCmd.Command.bit.ControlMode)
    {
        gFeisu.VoltTemp = CalOutVotInVFStatus(gMainCmd.FreqSyn);

    	if(gMainCmd.FreqSetBak > 0)
        {
            gOutVolt.VoltPhaseApply = 16384;
        }
    	else if(gMainCmd.FreqSetBak < 0)
        {
            gOutVolt.VoltPhaseApply = -16384;
        }
    	gOutVolt.VoltApply      = gFeisu.VoltCheck;
    }

}

void SpeedCheckUdcRiseDeal(void)
{

	if(gFeisu.SpeedCheck >= 0)
	{
		gFeisu.SpeedCheck += abs(gFeisu.Ger4A);
	}
	else
	{
		gFeisu.SpeedCheck -=abs( gFeisu.Ger4A);
	}

	if(((gFeisu.Case4Sig1.all & 0x0C000) == 0x4000)||
	   ((gFeisu.Case4Sig1.all & 0x0C000) == 0x8000))
	{
		if((gFeisu.Case4Sig1.bit.SpdSig == 1) && (gFeisu.SpeedCheck < 0))
		{
			gFeisu.SpeedCheck = 0;
		}
		if((gFeisu.Case4Sig1.bit.SpdSig == 0) && (gFeisu.SpeedCheck > 0))
		{
			gFeisu.SpeedCheck = 0;
		}
	}
	if(((gFeisu.Case4Sig1.all & 0x0C000) == 0x4000) &&
	   (gFeisu.SpeedLast >= 0) &&
	   (gFeisu.SpeedCheck > gFeisu.SpeedLast))
	{
		gFeisu.SpeedCheck = gFeisu.SpeedLast;
	}
	else if(((gFeisu.Case4Sig1.all & 0x0C000) == 0x4000) &&
	        (gFeisu.SpeedLast < 0) &&
	        (gFeisu.SpeedCheck < gFeisu.SpeedLast))
	{
		gFeisu.SpeedCheck = gFeisu.SpeedLast;
	}
	else if(((gFeisu.Case4Sig1.all & 0x0C000) != 0x4000) &&
	        (gFeisu.SpeedCheck > gFeisu.SpeedMaxPu))
	{
		gFeisu.SpeedCheck = gFeisu.SpeedMaxPu;
	}
	else if(((gFeisu.Case4Sig1.all & 0x0C000) != 0x4000) &&
	        (gFeisu.SpeedCheck < -gFeisu.SpeedMaxPu))
	{
		gFeisu.SpeedCheck = -gFeisu.SpeedMaxPu;
	}
}

void SpeedCheckCycleOver(void)
{
	if(gMainCmd.FreqDesired > 0)
	{
		gFeisu.SpeedCheck = (gFeisu.Speed5hz >> 3);
	}
	else
	{
		gFeisu.SpeedCheck = -(gFeisu.Speed5hz >> 3);
	}
	gFeisu.Case4Sig2.all |= 0x8040;
	gFeisu.VoltCheck      = 0;
	gSpeedCheckPID.Total  = 0;
	gFeisu.Case4Sig1.bit.CHECK_GET = 1;
}

void SpeedCheckCycleSUB2(void)
{
	gFeisu.VoltCheck     = 0;
	gSpeedCheckPID.Total = 0;
}
