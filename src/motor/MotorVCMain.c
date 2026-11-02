
#include "MotorVCInclude.h"
#include "MotorEncoder.h"
#include "MotorPmsmParEst.h"
#include "MotorPmsmMain.h"

VC_INFO_STRUCT			gVCPar;
MT_STRUCT_Q24           gIMTSet;
MT_STRUCT_Q24           gIMTSetApply;
MT_STRUCT				gUMTSet;
AMPTHETA_STRUCT			gUAmpTheta;

PID32_STRUCT        gImAcrQ24;
PID32_STRUCT        gItAcrQ24;

ASR_STRUCT				gAsr;
VC_CSR_PARA             gVcCsrPara;
PID_STRUCT              gIMSetAcr;
PID_STRUCT              gWspid;

MT_STRUCT               gPWMVAlphBeta;
UDC_LIMIT_IT_STRUCT     gUdcLimitIt;
PID_STRUCT              gSvcVoltAdj;
ALPHABETA_STRUCT        gABVoltSet;
SVC0_STRUCT             SVC0Signal;

void PrepareAsrPar(void);
void ResetAsrPar(void);
void ResetCsrPar(void);
void VcAsrControl(void);
void CalTorqueLimitPar(void);
void CalWsAndSynFreq(void);
void CalUdcLimitIT(void);

static void ResetAsrTotals(void)
{
    gAsr.Asr.Total           = 0;
    gAsr.TorqueLimitPid.Total = 0;
    gWspid.Total             = 0;
    gIMTSet.T                = 0;
    gIMTSet.M                = 0;
}

static void ResetVoltOut(void)
{
    gVcCsrPara.ImModify     = 0;
    gUAmpTheta.Amp          = 0;
    gUAmpTheta.Theta        = 0;
    gOutVolt.VoltApply      = 0;
    gOutVolt.VoltPhaseApply = 0;
}

void ResetParForVC(void)
{
    if( gMainCmd.Command.bit.ControlMode == IDC_SVC_CTL )
    {
	    ResetSVC();
    }
	ResetAsrPar();
	ResetCsrPar();
}

void ResetAsrPar(void)
{
    ResetAsrTotals();
}

void ResetCsrPar(void)
{
    Uint    IsPm;

	gIMTSetApply.M = 0;
	gIMTSetApply.T = 0;

	gUMTSet.M = 0;
	gUMTSet.T = 0;

    IsPm = (MOTOR_TYPE_PM == gMotorInfo.MotorType);

    if(0 != IsPm)
    {
        gIMSetAcr.Total = 0;
#if 0

        m_ITAcrTotal = (((long)gMainCmd.FreqFeed << 12) / gMotorInfo.FreqPer);
        gItAcrQ24.Total = (long)m_ITAcrTotal << 12;
#else
        gImAcrQ24.Total = (long)gPmDecoup.RotVd << 12;
        gItAcrQ24.Total = (long)gPmDecoup.RotVq << 12;
#endif
    }
    else
    {
        gIMSetAcr.Total = ((((long)gMotorExtPer.IoVsFreq)<<12)/gMotorInfo.Current)<<15;

        gImAcrQ24.Total = 0;
        gItAcrQ24.Total = 0;
    }

    ResetVoltOut();

    gMainCmd.FreqSyn = gMainCmd.FreqFeed;
}

void CalIMSet(void)
{
	Uint    Im_AbsFreq,m_AbsFreq,tempU;
    int     m_TempVar;
	Uint 	m_UData2,m_UData3;
    Ulong   m_MaxI0Freq,m_Im;
    Ulong   m_HigFreq,m_LowFreq;

    Im_AbsFreq = ((Ulong)(abs(gMainCmd.FreqSyn)) * gBasePar.FullFreq01) >>15;
    m_HigFreq  = __IQsat(gVFPar.SVCTorqueUpLim,gMotorInfo.Motor_HFreq,0);
    m_LowFreq  = __IQsat(gVFPar.SVCTorqueUpLim - 1000, gMotorInfo.Motor_LFreq, 0);

    if(1 == gMainCmd.Command.bit.PreExcFlux)
    {
        gIMTSet.M = (long)gComPar.StartDCBrakeCur * 167772L;
        return;
    }

    m_AbsFreq = abs(gMainCmd.FreqSyn);

    if(m_AbsFreq < gMotorInfo.FreqPer)
    {
        m_MaxI0Freq = gMotorExtPer.I0;
    }
    else
    {
        m_AbsFreq   = ((long)( abs(gMainCmd.FreqSyn) - gMotorInfo.FreqPer) * (long)gMotorExtPer.FluxLeakCurveGain/100L) +gMotorInfo.FreqPer;
        m_MaxI0Freq = ((long)gMotorExtPer.I0 * (long)gMotorInfo.FreqPer) / m_AbsFreq ;
    }

    if(0 == gVFPar.SVCTorqueUp)
    {
        gIMTSet.M = m_MaxI0Freq <<12;
        return;
    }

    m_Im = (Ulong)((Ulong)m_MaxI0Freq *(1024L + (Ulong)gVFPar.SVCTorqueUp) >>10);

    if(Im_AbsFreq < m_LowFreq)
    {
        gIMTSet.M = m_Im <<12;
    }
	else if((Im_AbsFreq >= m_LowFreq)&&(Im_AbsFreq < m_HigFreq))
	{
        m_Im = m_Im - (Im_AbsFreq-m_LowFreq)*(m_Im-m_MaxI0Freq)/(m_HigFreq-m_LowFreq);
        gIMTSet.M = m_Im <<12;
	}
	else
	{
	    gIMTSet.M = m_MaxI0Freq <<12;
	}
}

static void PrepareTorqueLimitPid(int TorLimit)
{
    gAsr.TorqueLimitPid.Max  = TorLimit;
    gAsr.TorqueLimitPid.Min  = 2;
    gAsr.TorqueLimitPid.KP   = 1000;
    gAsr.TorqueLimitPid.KI   = 1000;
    gAsr.TorqueLimitPid.QP   = 0;
    gAsr.TorqueLimitPid.QI   = 0;
    gAsr.TorqueLimitPid.Deta = TorLimit - abs(gIMTSetApply.T>>12);

    if( gInvInfo.InvTypeApply > 20 )
    {
        gAsr.TorqueLimitPid.KP = 100;
        gAsr.TorqueLimitPid.KI = 100;
    }

    PID( (PID_STRUCT *)&gAsr.TorqueLimitPid);
    gAsr.TorqueLimit = gAsr.TorqueLimitPid.Out>>16;
}

void CalTorqueLimitPar(void)
{
	int m_TorLimit,m_MaxLimit;
	int m_InvIM;
	Ulong m_Long,m_SpeedFilter;

	m_InvIM = ((long)(gIMTSet.M>>12) * (long)gMotorInfo.Current)/gInvInfo.InvCurrent;
	m_InvIM = ((long)m_InvIM * 1000)>>12;

    if(15 == gInvInfo.InvTypeApply)
    {
      m_Long  = (1700L * 1700L) - ((long)m_InvIM * (long)m_InvIM);
    }
    else
    {
      m_Long  = (1800L * 1800L) - ((long)m_InvIM * (long)m_InvIM);
    }
    m_MaxLimit = qsqrt(m_Long);

    m_TorLimit = (gVCPar.VCTorqueLim > m_MaxLimit)?m_MaxLimit : gVCPar.VCTorqueLim;

	if ((1 == gMainCmd.Command.bit.TorqueCtl)&&
        (gMotorInfo.MotorType != MOTOR_TYPE_PM)&&
        (abs(gMainCmd.FreqSyn) >= gMotorInfo.FreqPer))
	{
		m_TorLimit = ((long)m_TorLimit * (long)gMotorExtPer.I0) / (gIMTSet.M>>12);
	}

    if((gMainCmd.Command.bit.ControlMode ==IDC_SVC_CTL) &&
        (gMotorInfo.MotorType != MOTOR_TYPE_PM)&&(0 == gVCPar.SvcMode))
    {
	    gMainCmd.FreqSynFilter = Filter16(gMainCmd.FreqFeed,gMainCmd.FreqSynFilter);
	    m_SpeedFilter = abs(gMainCmd.FreqSynFilter);
	    m_SpeedFilter = ((llong)m_SpeedFilter * (llong)gBasePar.FullFreq01)>>15;

	    if(m_SpeedFilter < 40)
	    {
		    m_TorLimit = (m_TorLimit > 1200)?1200:m_TorLimit;
	    }

	    m_SpeedFilter = abs(gIMTSet.T>>12);

	    if(m_SpeedFilter >= gAsr.TorqueLimit)
	    {
		    m_SpeedFilter = abs(gIMTQ12.T);

		    if((m_SpeedFilter + 2048) < gAsr.TorqueLimit)
		    {
			    m_TorLimit = (m_TorLimit > 1000)?1000:m_TorLimit;
		    }
	    }
    }

	m_TorLimit = ((long)m_TorLimit * (long)gInvInfo.InvCurrForP)/gMotorInfo.Current;

	m_TorLimit = ((long)m_TorLimit<<12)/1000;

	if((1 == gMainCmd.Command.bit.TorqueCtl)
	    || (IDC_FVC_CTL == gMainCmd.Command.bit.ControlMode))
	{
		gAsr.TorqueLimit = m_TorLimit;
	}
	else
	{
        PrepareTorqueLimitPid(m_TorLimit);
	}

}

static int CalAsrKi(Uint KpPar, Uint TiPar, Uint Shift)
{
    if((KpPar>>5) >= TiPar)
    {
        return(32767);
    }
    return((int)(((Ulong)KpPar<<Shift)/TiPar));
}

void PrepareAsrPar(void)
{
    int	  m_AbsFreq, m_FreqUp;
    int   m_DetaKP, m_DetaKI, m_DetaFreq;
    int   m_KpLimit;
    long  tempKp;
    long  tempKi;
    Uint  m_KpShift;
    Uint  m_KiShift;

    if(MOTOR_TYPE_PM ==gMotorInfo.MotorType)
    {
        m_KpShift = 7;
        m_KiShift = 9;
    }
    else
    {
        m_KpShift = 8;
        m_KiShift = 10;
    }

    gAsr.KPHigh = gVCPar.ASRKpHigh<<m_KpShift;
    gAsr.KPLow  = gVCPar.ASRKpLow<<m_KpShift;
    gAsr.KIHigh = CalAsrKi(gVCPar.ASRKpHigh, gVCPar.ASRTIHigh, m_KiShift);
    gAsr.KILow  = CalAsrKi(gVCPar.ASRKpLow, gVCPar.ASRTILow, m_KiShift);

	gAsr.SwitchHigh = ((Ulong)gVCPar.ASRSwitchHigh<<15)/gBasePar.FullFreq;
	gAsr.SwitchLow  = ((Ulong)gVCPar.ASRSwitchLow<<15)/gBasePar.FullFreq;

	m_DetaKP   = gAsr.KPHigh - gAsr.KPLow;
	m_DetaKI   = gAsr.KIHigh - gAsr.KILow;
    m_AbsFreq  = abs(gMainCmd.FreqSyn);

	if(m_AbsFreq <= gAsr.SwitchLow)
	{
		gAsr.Asr.KP = gAsr.KPLow;
		gAsr.Asr.KI = gAsr.KILow;
	}
	else if(m_AbsFreq >= gAsr.SwitchHigh)
	{
		gAsr.Asr.KP = gAsr.KPHigh;
		gAsr.Asr.KI = gAsr.KIHigh;
	}
	else
	{
		m_FreqUp    = m_AbsFreq - gAsr.SwitchLow;
		m_DetaFreq  = gAsr.SwitchHigh - gAsr.SwitchLow;
		gAsr.Asr.KP = ((long)m_DetaKP * (long)m_FreqUp)/m_DetaFreq + gAsr.KPLow;
		gAsr.Asr.KI = ((long)m_DetaKI * (long)m_FreqUp)/m_DetaFreq + gAsr.KILow;
	}

    tempKp = ( (long)gAsr.Asr.KP * (long)gBasePar.FullFreq01 / 10L)>>12;
    tempKi = ( (long)gAsr.Asr.KI * (long)gBasePar.FullFreq01 / 10L)>>12;
    gAsr.Asr.KP = __IQsat(tempKp, 32767, 1);
    gAsr.Asr.KI = __IQsat(tempKi, 32767, 1);

    m_KpLimit = ((long)abs(gMainCmd.FreqDesired - gMainCmd.FreqFeed) *(long)gAsr.Asr.KP) >>16;

    if(( 64 < m_KpLimit ) &&
        (0 == gMainCmd.Command.bit.TorqueCtl) &&
        (1 == gMainCmd.Command.bit.IntegralDiscrete))
    {
        gAsr.Asr.KI = 0;
    }
    gAsr.Asr.KD = 0;

    if(gMotorInfo.MotorType != MOTOR_TYPE_PM)   gAsr.Asr.QP = 3;
    else                                        gAsr.Asr.QP = 2;
    gAsr.Asr.QI = 3;

}

static void PrepareUdcPid(int Limit)
{
    gUdcLimitIt.UdcPid.KP  = 10000 + 200*gVFPar.ovGain;
    gUdcLimitIt.UdcPid.KI  = 2000 + 100*gVFPar.ovGain;
    gUdcLimitIt.UdcPid.KD  = 0;
    gUdcLimitIt.UdcPid.QP  = 0;
    gUdcLimitIt.UdcPid.QI  = 0;
    gUdcLimitIt.UdcPid.Max = Limit;
    gUdcLimitIt.UdcPid.Min = 0;
}

void CalUdcLimitIT(void)
{
    int m_SoftLimit;

    if(0 == gVFPar.ovGain)
    {
        gUdcLimitIt.UDCLimit = gAsr.TorqueLimit;
        gUdcLimitIt.UdcPid.Total = 0;
        gUdcLimitIt.FirstOvUdcFlag = 0;
        return;
    }

    gUdcLimitIt.UDCBakCnt++;

    if(gUdcLimitIt.UDCBakCnt >= 5)
    {
        gUdcLimitIt.UDCBakCnt = 0;
        gUdcLimitIt.UDCDeta = gUDC.uDCFilter - gUdcLimitIt.UDCBak;
        gUdcLimitIt.UDCBak = gUDC.uDCFilter;

        if(gUdcLimitIt.UDCDeta < 200)
        {
            gUdcLimitIt.UDCDeta = 0;
        }
    }

    m_SoftLimit = gOvUdc.Limit - 100 - (gVFPar.ovGain<<2) - gUdcLimitIt.UDCDeta;

    if(gUDC.uDCFilter < m_SoftLimit)
    {
        gUdcLimitIt.UdcPid.Total = (long)-gIMTQ12.T<<16;
        gUdcLimitIt.UDCLimit = gAsr.TorqueLimit;
    }
    else if((gUDC.uDCFilter < gOvUdc.Limit) && (gUdcLimitIt.FirstOvUdcFlag == 0))
    {
        gUdcLimitIt.FirstOvUdcFlag = 1;
        gUdcLimitIt.UDCLimit = gUdcLimitIt.UdcPid.Total>>16;
    }
    else
    {
        PrepareUdcPid(gAsr.TorqueLimit);

        gUdcLimitIt.UdcPid.Deta = gOvUdc.Limit - gUDC.uDCFilter;
        PID((PID_STRUCT *) &gUdcLimitIt.UdcPid);
        gUdcLimitIt.UDCLimit = gUdcLimitIt.UdcPid.Out>>16;
    }
}

static void VcLowFreqHold(void)
{
    gIMTSet.T         = 0;
    gAsr.Asr.Total    = 0;
    gMainCmd.FirstCnt = 0;
    gMainCmd.FreqSyn  = 0;
    gMainCmd.FreqFeed = 0;
}

void VcAsrControl(void)
{
	long m_Long;
	llong    m_LLong;
    int     m05HzPu;
    int     m03HzPu;
    int     m_LowFreq;
    long    m_AsrOut;

    m05HzPu = (50L<<15) / gBasePar.FullFreq01;
    m03HzPu = (30L<<15) / gBasePar.FullFreq01;

    if((1 == gMainCmd.Command.bit.PreExcFlux)
        && (MOTOR_TYPE_PM != gMotorInfo.MotorType))
    {
        gIMTSet.T = 0;
        gAsr.Asr.Total = 0;
        return;
    }

	gAsr.Asr.Max = gUdcLimitIt.UDCLimit;
	gAsr.Asr.Min = -gAsr.Asr.Max;

	m_Long = (long)gMainCmd.FreqSetApply - (long)gMainCmd.FreqFeed;

	gAsr.Asr.Deta = __IQsat(m_Long, 16383, -16383);
	PID((PID_STRUCT *)&gAsr.Asr);

    m_AsrOut = gAsr.Asr.Out>>(16-12);
    m_LowFreq = 0;

    if((abs(gMainCmd.FreqSetApply) < m03HzPu &&
        abs(gMainCmd.FreqFeed) < m05HzPu))
    {
        m_LowFreq = 1;
    }

    if(gCtrMotorType == ASYNC_SVC)
    {
        if(1 == gVCPar.SvcMode)
		{
            if((1 != gMainCmd.Command.bit.TorqueCtl)&&(0 != m_LowFreq))
			{
                VcLowFreqHold();
			}
			else
			{
                m_LLong = (llong)m_AsrOut;
                gIMTSet.T = ((llong)m_LLong*(32L-gTestDataReceive.TestData8)
                          + (llong)gIMTSet.T*((llong)gTestDataReceive.TestData8))>>5;
			}
		}
		else
		{
            if(0 != m_LowFreq)
            {
                VcLowFreqHold();
            }
			else
			{
	            gIMTSet.T = m_AsrOut;
			}
		}
	}
	else
	{
	    gIMTSet.T = m_AsrOut;
	}

    if( 1 == gMainStatus.StatusWord.bit.OutOff )
    {
	    ResetParForVC();
    }
}

void CalWsAndSynFreq(void)
{
	int m_UData,m_FreqWs;
	int m_ImSet;
    int m_WsOut = 0;
    int	m_Angle,m_Sin,m_Cos;

    if(gMainCmd.Command.bit.PreExcFlux == 1)
    {
        gMainCmd.FreqWs = 0;
    	gMainCmd.FreqSyn = gMainCmd.FreqFeed;
        return;
    }
    m_ImSet = (gIMTSet.M>>12);
    m_UData = ((long)gMotorExtPer.R2<<11)/m_ImSet;
    m_UData = ((long)m_UData<<11)/gMotorExtPer.L1;
    m_UData = ((long)m_UData * (long)(gIMTSet.T>>12))>>14;
    m_FreqWs = ((long)m_UData * (long)gVCPar.VCWsCoff)/100 ;

    gMainCmd.FreqWs = m_FreqWs;
    gMainCmd.FreqSyn = gMainCmd.FreqFeed + gMainCmd.FreqWs;
}

void CalWsAndSynFreq_380(void)
{
	int m_UData,m_FreqWs;
	int m_ImSet;
    int m_WsOut = 0;
    int	m_Angle,m_Sin,m_Cos;
	long m_Long;

    if(gMainCmd.Command.bit.PreExcFlux == 1)
    {
        gMainCmd.FreqWs = 0;
    	gMainCmd.FreqSyn = gMainCmd.FreqFeed;
        return;
    }

    m_ImSet = (gIMTSetApply.M>>12);

    m_Long = (long)(gMotorExtPer.R2) * (long)(gIMTSet.T>>12);

	m_Long = (long)m_Long / ((long)m_ImSet);

	m_Long = (long)(m_Long<<8) / (long)gMotorExtPer.L1;

    m_FreqWs = ((long)m_Long * (long)gVCPar.VCWsCoff)/100L;
    gMainCmd.FreqWs = m_FreqWs;
    gMainCmd.FreqSyn = gMainCmd.FreqFeed + gMainCmd.FreqWs;
}

void PrepImCsrPara()
{

    gImAcrQ24.KI= gVCPar.AcrImKi;

    gItAcrQ24.KI = gVCPar.AcrItKi;
    gImAcrQ24.KP = (long)gVCPar.AcrImKp * gBasePar.FcSetApply / 80;
    gItAcrQ24.KP = (long)gVCPar.AcrItKp * gBasePar.FcSetApply / 80;

}

#if 0
void PrepareCsrPara()
{
    long    ImKp, ImKi, ItKp, ItKi, temp;
    int     Nf;
    int     sGain;

    if(gMotorInfo.MotorType != MOTOR_TYPE_PM)
    {

        gImAcrQ24.KI= gVCPar.AcrImKi;

        gItAcrQ24.KI = gVCPar.AcrItKi;
        gImAcrQ24.KP = (long)gVCPar.AcrImKp * gBasePar.FcSetApply / 80;
        gItAcrQ24.KP = (long)gVCPar.AcrItKp * gBasePar.FcSetApply / 80;

    }
    else
    {

    	ImKp = (long)gVCPar.AcrImKp * gBasePar.FcSetApply / 80;
        ImKi = gVCPar.AcrImKi;

        ItKp = (long)gVCPar.AcrItKp * gBasePar.FcSetApply / 80;
        ItKi = gVCPar.AcrItKi;

        if(gFluxWeak.Mode == 0)
        {
            gImAcrQ24.KP = ImKp;
            gImAcrQ24.KI = ImKi;
            gItAcrQ24.KP = ItKp;
            gItAcrQ24.KI = ItKi;
            return;
        }

        if(gFluxWeak.AbsFrqLpf < gMotorInfo.FreqPer)
        {
            Nf = 40;
        }
        else
        {
            temp = (gBasePar.FullFreq01/100) * gFluxWeak.AbsFrqLpf >>15;
            Nf = ((long)gBasePar.FcSetApply * 100) / temp;
        }
        if(Nf >= 40)
        {
            gImAcrQ24.KP = ImKp;
            gItAcrQ24.KP = ItKp;
        }
        else if(Nf >= 20)
        {
            gImAcrQ24.KP = ImKp * 40L / Nf;
            gItAcrQ24.KP = ItKp * 40L / Nf;
        }
        else
        {
            gImAcrQ24.KP = ImKp * 2;
            gItAcrQ24.KP = ItKp * 2;
        }

        if(gFluxWeak.AbsFrqLpf <= gMotorInfo.FreqPer)
        {
            gImAcrQ24.KI = ImKi;
            gItAcrQ24.KI = ItKi;
        }
        else
        {
            sGain = gFluxWeak.CoefKI;
            temp = 32767L - gMotorInfo.FreqPer;
            temp = (((long)gFluxWeak.AbsFrqLpf -gMotorInfo.FreqPer)<<5) / temp;
            gImAcrQ24.KI = ((1L<<5) + temp * sGain) * ImKi >>5;
            gItAcrQ24.KI = ((1L<<5) + (temp * sGain<<2)) * ItKi >>5;
        }
    }
}
#endif

static void SnapshotImTs(void)
{
	DINT;
	gIMTSetApply.T = gIMTSet.T;
	gIMTSetApply.M = gIMTSet.M;
    EINT;
}

static void LimitVoltAmp(long Limit, MT_STRUCT *pOut)
{
	if(gUAmpTheta.Amp < Limit)
	{
		pOut->M = gUMTSet.M;
		pOut->T = gUMTSet.T;
		gOutVolt.VoltApply = gUAmpTheta.Amp;
	}
	else
	{
		pOut->M = gUMTSet.M >> 2;
		pOut->T = gUMTSet.T >> 2;
		gOutVolt.VoltApply = Limit;
	}
}

static void ClearVoltWhenNotRun(MT_STRUCT *pOut)
{
	if((gMainStatus.RunStep != STATUS_SPEED_CHECK) && (gMainStatus.RunStep != STATUS_RUN))
	{
		pOut->M = 0;
		pOut->T = 0;
		gOutVolt.VoltApply = 0;
		gUMTSet.M = 0;
		gUMTSet.T = 0;
	}
}

static void SetVoltPhase(const MT_STRUCT *pOut)
{
	gUAmpTheta.Theta = atan(pOut->M, pOut->T);
	gOutVolt.VoltPhaseApply = gUAmpTheta.Theta;
}

void VCCsrControl(void)
{
	Ulong 		m_Long;
    long        m_Data;
	long        m_MaxVolt, maxVolt2;
    long        m_DriveState;
	MT_STRUCT	m_UMT;
    long        tempL;

    m_MaxVolt = gOutVolt.MaxOutVolt;
    maxVolt2  = m_MaxVolt + 500;

    SnapshotImTs();

    gImAcrQ24.Max = m_MaxVolt << 12;
    gImAcrQ24.Min = - gImAcrQ24.Max;
    gImAcrQ24.Deta = gIMTSetApply.M - gIMTQ24.M;
    PID32(&gImAcrQ24);

    gUMTSet.M = (int)(gImAcrQ24.Out >> 12);

    gItAcrQ24.Max = m_MaxVolt << 12;
    gItAcrQ24.Min = - gItAcrQ24.Max;
    gItAcrQ24.Deta = gIMTSetApply.T - gIMTQ24.T;
    PID32(&gItAcrQ24);

    gUMTSet.T = (int)(gItAcrQ24.Out >> 12);

    #if 0
    gPmDecoup.EnableDcp = 0;
    if(gPmDecoup.EnableDcp)
    {
        gUMTSet.T += gPmDecoup.RotVq;
        gUMTSet.M += gPmDecoup.RotVd;
    }
    #endif

	m_Long = (((long)gUMTSet.M * (long)gUMTSet.M) +
	          ((long)gUMTSet.T * (long)gUMTSet.T));
	gUAmpTheta.Amp = (Uint)qsqrt(m_Long);

	if(gUAmpTheta.Amp < maxVolt2)
	{
		LimitVoltAmp(maxVolt2, &m_UMT);
	}
	else
	{
	    if(gMotorInfo.MotorType != MOTOR_TYPE_PM)
        {
            LimitVoltAmp(maxVolt2, &m_UMT);
        }
        else
        {
            long temp;
            long minVq, maxVd;

            minVq = 100;

            if(abs(minVq) < m_MaxVolt)
            {
                temp = (long)m_MaxVolt * m_MaxVolt - (long)minVq * minVq;
                maxVd = qsqrt(temp);
            }
            else
            {
                maxVd = 0;
            }

            gFluxWeak.DecoupleMode = 1;
            if(gFluxWeak.DecoupleMode ==1)
            {
                m_UMT.M = (gUMTSet.M > maxVd) ? maxVd : gUMTSet.M;
            }
            else
            {
                m_UMT.M = gUMTSet.M;
            }

            temp = (long)maxVolt2 * maxVolt2 - (long)m_UMT.M * m_UMT.M;
            temp = __IQsat(temp, 0x7FFFFFFF, 0);
            temp = qsqrt(temp);
            m_UMT.T = (gUMTSet.T > 0) ? temp : (-temp);
        }
        gUMTSet.M = m_UMT.M;
        gUMTSet.T = m_UMT.T;
		gOutVolt.VoltApply = maxVolt2;
	}

	SetVoltPhase(&m_UMT);
}

void SvcCalOutVolt(void)
{
	MT_STRUCT	m_UMT;
    long        m_Data;
	int			m_Data1,m_Umt_Kp,m_UMBak,m_UTBak;
	Ulong		m_Long;
	Uint        m_MaxVolt;
    int         m05HzPu;

	gIMTSetApply.T = gIMTSet.T;
	gIMTSetApply.M = gIMTSet.M;

    m05HzPu = (50L<<15) / gBasePar.FullFreq01;

	if(gMainCmd.FreqReal < 700)
	{
		SVC0Signal.VoltSVCCalSignal = 1;
	}
	else if(gMainCmd.FreqReal > 1050)
	{
		SVC0Signal.VoltSVCCalSignal = 0;
	}

	m_Data  = ((gIMTSetApply.M>>12) * (long)gMainCmd.FreqSyn)>>12;
	m_Data  = (m_Data * (long)gMotorExtPer.L1)>>12;
	m_Data1 = ((long)gMotorExtPer.R1 * (gIMTSetApply.T>>12))>>16;
	m_UTBak = (int)((long)m_Data1 + m_Data);

	m_Data  = ((gIMTSetApply.T>>12) * (long)gMainCmd.FreqSyn)>>15;
	m_Data  = (m_Data * (long)gMotorExtPer.L0)>>13;
	m_Data1 = ((long)gMotorExtPer.R1 * (gIMTSetApply.M>>12))>>16;

	if(SVC0Signal.VoltSVCCalSignal==0)
	{
		m_UMBak = m_Data1;
	}
	else
	{
		m_UMBak = m_Data1 - m_Data;
	}

	m_MaxVolt = 200;
	if(gInvInfo.InvTypeApply > 12)
	{
		m_MaxVolt = 100;
	}
	if(gMainCmd.FreqRealFilt < m_MaxVolt)
	{
		SVC0Signal.VoltSVCCalSignalB = 0;
	}
	else if(gMainCmd.FreqRealFilt > (m_MaxVolt +120))
	{
		SVC0Signal.VoltSVCCalSignalB = 1;
	}

    if((gMainStatus.RunStep != STATUS_RUN) || (SVC0Signal.VoltSVCCalSignalB == 1) || (abs(gMainCmd.FreqSetApply) < m05HzPu))
    {
        m_Umt_Kp = 0;
        gSvcVoltAdj.Total = 0;
        SVC0Signal.VoltSVCCalSignalB = 1;
    }
    else
    {
        gSvcVoltAdj.KP = 1500;
        gSvcVoltAdj.KI = 5000/4;
        gSvcVoltAdj.Max = 4096L;
        gSvcVoltAdj.Min = -(gSvcVoltAdj.Max);
        if(gInvInfo.InvTypeApply >= 18)
        {
            gSvcVoltAdj.KP = 5000;
            gSvcVoltAdj.KI = 1300/4;
        }
        gSvcVoltAdj.Deta = (gIMTSetApply.M - gIMTQ24.M) >> 12;
        PID((PID_STRUCT *)&gSvcVoltAdj);
        m_Umt_Kp = gSvcVoltAdj.Out >> 16;
    }

	m_Umt_Kp = m_Umt_Kp + 4096;
	gUMTSet.T = ((long)m_UTBak * (long)m_Umt_Kp)>>12;
	gUMTSet.M = ((long)m_UMBak * (long)m_Umt_Kp)>>12;

	m_Long = (((long)gUMTSet.M * (long)gUMTSet.M) +
	          ((long)gUMTSet.T * (long)gUMTSet.T));
	gUAmpTheta.Amp = (Uint)qsqrt(m_Long);

	LimitVoltAmp(4916, &m_UMT);
	ClearVoltWhenNotRun(&m_UMT);

	SetVoltPhase(&m_UMT);

}

void SvcCalOutVolt_380(void)
{
	MT_STRUCT	m_UMT;
    long        m_Data;
	long		m_Data1,m_UMBak,m_UTBak;

	Ulong		m_Long;
	Uint        m_MaxVolt;

        gImAcrQ24.KI= gVCPar.AcrImKi;

        gItAcrQ24.KI = gVCPar.AcrItKi;
        gImAcrQ24.KP = (long)gVCPar.AcrImKp * gBasePar.FcSetApply / 80;
        gItAcrQ24.KP = (long)gVCPar.AcrItKp * gBasePar.FcSetApply / 80;

	gIMTSetApply.T = gIMTSet.T;
	gIMTSetApply.M = gIMTSet.M;

	m_Data  = ((long)gIMTSetQ12.M * (long)gMainCmd.FreqSyn)>>15 ;
	m_Data  = ((long)m_Data * (long)gMotorExtPer.L1)>>9;
	m_Data1 = ((long)gMotorExtPer.R1 * (long)gIMTSetQ12.T)>>16;

	m_UTBak = (int)((long)m_Data1 + (long)m_Data);

if(0 == gTestDataReceive.TestData0)
{
    m_Data = (long)gIMTSetQ12.T - (long)gIMTQ12_obs.T;
}
else
{
    m_Data = (long)gIMTSetQ12.T - (long)gIMTQ12.T;
}
    m_Data = ((((long)gMotorExtPer.R1)*(long)gTestDataReceive.TestData5 / 100L) * (long)m_Data)>>16;
    m_UTBak = (int)((long)m_UTBak + (long)m_Data);

    m_Data = ((long)gIMTSetQ12.T * (long)gMainCmd.FreqSyn)>>15;
    m_Data = ((long)gMotorExtPer.L0 * (long)m_Data)>>13;

	m_Data1 = ((long)gMotorExtPer.R1 * (long)gIMTSetQ12.M)>>16;

	m_UMBak = (int)((long)m_Data1 - (long)m_Data);

if(0 == gTestDataReceive.TestData0)
{
    m_Data = (long)gIMTSetQ12.M - (long)gIMTQ12_obs.M;
}
else
{
    m_Data = (long)gIMTSetQ12.M - (long)gIMTQ12.M;
}
    m_Data = (((long)gMotorExtPer.R1*(long)gTestDataReceive.TestData4 /100L ) * (long)m_Data)>>16;

    m_UMBak = (int)((long)m_UMBak + (long)m_Data);

	gUMTSet.T = m_UTBak;
	gUMTSet.M = m_UMBak;

	m_Long = (((long)gUMTSet.M * (long)gUMTSet.M) +
	          ((long)gUMTSet.T * (long)gUMTSet.T));
	gUAmpTheta.Amp = (Uint)qsqrt(m_Long);

	LimitVoltAmp(4916, &m_UMT);
	ClearVoltWhenNotRun(&m_UMT);

	SetVoltPhase(&m_UMT);

}

void CalcABVolt(void)
{
    int     m_Angle,m_Sin,m_Cos;
    long    m_Long;

	m_Angle = (int)(gPhase.IMPhase>>16);
	m_Sin  = qsin(m_Angle);
	m_Cos  = qsin(16384 - m_Angle);
	m_Long = (((long)m_Cos * (long)gUMTSet.M) -
	           ((long)m_Sin * (long)gUMTSet.T))>>15;
	gABVoltSet.Alph = __IQsat(m_Long,32767,-32767);
	m_Long = (((long)m_Sin * (long)gUMTSet.M) +
	           ((long)m_Cos * (long)gUMTSet.T))>>15;
	gABVoltSet.Beta = __IQsat(m_Long,32767,-32767);
}

void VCCsrControl_380(void)
{
	Ulong 		m_Long;
    long        m_Data;
	long        m_MaxVolt, maxVolt2;
    long        m_DriveState;
	MT_STRUCT	m_UMT;
    long        tempL;
	long		m_Data1,m_UMBak,m_UTBak;

    m_MaxVolt = gOutVolt.MaxOutVolt;
    maxVolt2  = m_MaxVolt + 500;

    SnapshotImTs();

    m_Data  = ((long)gIMTSetQ12.T * (long)gMainCmd.FreqSyn)>>15;
    m_Data  = ((long)gMotorExtPer.L0 * (long)m_Data)>>13;
	m_Data1 = ((long)gMotorExtPer.R1 * (long)gIMTSetQ12.M)>>16;

	m_UMBak = (int)((long)m_Data1 - (long)m_Data);

	m_Data  = ((long)gIMTSetQ12.M * (long)gMainCmd.FreqSyn)>>15 ;
	m_Data  = ((long)m_Data * (long)gMotorExtPer.L1)>>9;
	m_Data1 = ((long)gMotorExtPer.R1 * (long)gIMTSetQ12.T)>>16;

	m_UTBak = (int)((long)m_Data1 + (long)m_Data);

    gImAcrQ24.Max = m_MaxVolt << 12;
    gImAcrQ24.Min = - gImAcrQ24.Max;
    gImAcrQ24.Deta = gIMTSetApply.M - gIMTQ24_obs.M;
    PID32(&gImAcrQ24);
    gUMTSet.M = (int)(gImAcrQ24.Out >> 12) + m_UMBak;

    gItAcrQ24.Max = m_MaxVolt << 12;
    gItAcrQ24.Min = - gItAcrQ24.Max;
    gItAcrQ24.Deta = gIMTSetApply.T - gIMTQ24_obs.T;
    PID32(&gItAcrQ24);

    gUMTSet.T = (int)(gItAcrQ24.Out >> 12) + m_UTBak;

	m_Long = (((long)gUMTSet.M * (long)gUMTSet.M) +
	          ((long)gUMTSet.T * (long)gUMTSet.T));
	gUAmpTheta.Amp = (Uint)qsqrt(m_Long);

	m_Long = (((long)gUMTSet.M * (long)gUMTSet.M) +
	          ((long)gUMTSet.T * (long)gUMTSet.T));
	gUAmpTheta.Amp = (Uint)qsqrt(m_Long);

	LimitVoltAmp(4916, &m_UMT);
	ClearVoltWhenNotRun(&m_UMT);

	SetVoltPhase(&m_UMT);
}
