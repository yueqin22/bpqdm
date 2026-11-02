
#include "MotorVCInclude.h"
#include "MotorInclude.h"

IPM_POSITION_STRUCT		gIPMPos;
PM_FLUX_WEAK            gFluxWeak;
PM_INIT_POSITION        gPMInitPos;
IPM_POS_CHECK_STRUCT	gIPMPosCheck;
PM_DECOUPLE             gPmDecoup;

interrupt void PG_Zero_isr(void)
{
    int  errZFlag;
    Uint mPos;

	EALLOW;
	(*EQepRegs).QCLR.all = 0x0401;
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP5;
    EDIS;

    if(gIPMZero.zFilterCnt != 0)
	{

		return;
	}

    gIPMZero.zFilterCnt = 8;
	gIPMZero.Flag = (gIPMZero.Flag | 0x01);
    gIPMPos.Zcounter ++;

	if(gMainStatus.RunStep != STATUS_GET_PAR)
	{
	    errZFlag = 0;
		if(gPGData.PGType == PG_TYPE_ABZ)
		{
			gIPMPos.AbzErrPos = (int)(gIPMPos.RotorZero- gIPMPos.ABZ_RotorPos_Ref);
			if(abs(gIPMPos.AbzErrPos) > (35*65536L/360))
			{
	            errZFlag ++;
			}
		}
        else if(gPGData.PGType == PG_TYPE_UVW)
        {
            gIPMPos.AbzErrPos = (int)(gIPMPos.RotorPos - gIPMPos.RotorZero);
			if(abs(gIPMPos.AbzErrPos) > (65*65536L/360))
			{
	            errZFlag ++;
			}

            GetUvwPhase();
            mPos = gUVWPG.UVWAngle + gUVWPG.UvwZeroPos;
            gUVWPG.UvwZIntErr = (int)(gIPMPos.RotorPos  - mPos);
            if(abs(gUVWPG.UvwZIntErr ) > (65L*32767/180))
            {
                 errZFlag ++;
            }
            #if 0
            else    errZFlag = 0;
            #endif
        }

        if(errZFlag)
            {
            gIPMPos.ZErrCnt ++;
            }
        else
            {

              SetIPMPos(gIPMPos.RotorZero);
            }
	}
    else
	{
		if(TUNE_STEP_NOW == PM_EST_NO_LOAD)
		{
			gIPMZero.FeedPos = 0;
			gIPMPos.QepTotal = 0;
			gIPMPos.QepBak   = GetQepCnt();
		}
		else if(TUNE_STEP_NOW == PM_EST_WITH_LOAD)
		{
			gIPMPos.PosInZInfo = gIPMPos.RotorPos;

            if(gUVWPG.UvwEstStep == 1)
            {
                gUVWPG.UvwEstStep ++;
                gUVWPG.UvwCnt2 = 0;
            }
            else if(gUVWPG.UvwEstStep == 2)
            {
                gUVWPG.UvwCnt2 ++;
                if(gUVWPG.UvwCnt2 >= 3)
                {
                    gUVWPG.UvwEstStep ++;
                }
            }
		}

        if(gPGData.PGType == PG_TYPE_UVW)
        {
            GetUvwPhase();

        }
	}
}

void IPMCheckInitPos(void)
{
	Uint m_Pos;
	int  m_Deta;
    Uint uvwPos;
    int  uvwPowOn;

	uvwPowOn = 0;
    if(gMainStatus.ParaCalTimes == 0)
	{
        gMainStatus.ParaCalTimes = 1;

	    gIPMPos.PowerOffPosDeg = ((Ulong)gIPMPos.PowerOffPos << 16)/3600;
	    SetIPMPos((Uint)gIPMPos.PowerOffPosDeg);
        SetIPMPos_ABZRef((Uint)gIPMPos.PowerOffPosDeg);

        PmChkInitPosRest();
        uvwPowOn ++;
        gIPMPos.ZeroPosLast = gIPMPos.RotorZero;
    }

	if(gPGData.PGType != PG_TYPE_UVW)
	{
        return;
	}
    #if 0

    if(gIPMPosCheck.Cnt == 0)
	{
		gIPMPosCheck.FirstPos = gIPMPos.RotorPos;
		gIPMPosCheck.Cnt++;
	}
	else if(gIPMPosCheck.Cnt <= 128)
	{
		m_Deta = (int)(gIPMPos.RotorPos - gIPMPosCheck.FirstPos);
		gIPMPosCheck.TotalErr += m_Deta;
		gIPMPosCheck.TotalErrAbs += abs(m_Deta);
		gIPMPosCheck.Cnt++;
    }

    if((gIPMPosCheck.Cnt > 128) ||
    (uvwPowOn == 1))
	{
		if((gIPMPosCheck.TotalErrAbs>>7) < ((5L<<15)/180))
		{
		    GetUvwPhase();
			m_Pos = (Uint)((int)gIPMPosCheck.FirstPos + (gIPMPosCheck.TotalErr>>7));
            uvwPos = gUVWPG.UVWAngle + gUVWPG.UvwZeroPos;

            gIPMPosCheck.UvwStopErr = (int)(m_Pos - uvwPos);
			if(abs(gIPMPosCheck.UvwStopErr) > (36L*32768L/180))
            {
				SetIPMPos(uvwPos);
				SetIPMPos_ABZRef(uvwPos);
                gIPMPosCheck.UvwRevCnt ++;
			}
		}

		PmChkInitPosRest();
	}
    #endif
    #if 1
    GetUvwPhase();

    if(abs(gFVCSpeed.SpeedTemp)<100)
    {
         if(gIPMPosCheck.Cnt > 128)
        {
            if((gIPMPosCheck.TotalErrAbs>>7) < 5461)
            {
                m_Pos = (Uint)((int)gIPMPosCheck.FirstPos + (gIPMPosCheck.TotalErr>>7));

                uvwPos = m_Pos + gUVWPG.UvwZeroPos;

                gIPMPosCheck.UvwStopErr = (int)(gIPMPos.RotorPos - uvwPos);
    			if(abs(gIPMPosCheck.UvwStopErr) > (36L*32768L/180))
                {
    				SetIPMPos(uvwPos);

    			}
            }
            PmChkInitPosRest();
        }
        else if(gIPMPosCheck.Cnt == 0)
        {
            gIPMPosCheck.FirstPos = gUVWPG.UVWAngle;
		    gIPMPosCheck.Cnt++;
        }
        else
        {
            m_Deta = (int)(gUVWPG.UVWAngle - gIPMPosCheck.FirstPos);
    		gIPMPosCheck.TotalErr += m_Deta;
    		gIPMPosCheck.TotalErrAbs += abs(m_Deta);
    		gIPMPosCheck.Cnt++;
        }
    }
    #endif
}

void PmChkInitPosRest(void)
{
	gIPMPosCheck.Cnt = 0;

	gIPMPosCheck.TotalErr = 0;
	gIPMPosCheck.TotalErrAbs = 0;
}

void RunCaseIpmInitPos(void)
{
	if((gError.ErrorCode.all != 0) ||
	   (gMainCmd.Command.bit.Start == 0))
	{
		DisableDrive();

	    SynInitPosDetSetPwm(6);

		gIPMInitPos.Step = 0;
		TurnToStopStatus();
		return;
	}

    switch(gMainStatus.SubStep)
    {
        case 1:
            if(gIPMInitPos.Step == 0)
            {
                gIPMInitPos.Step = 1;

                gMainStatus.PrgStatus.bit.PWMDisable = 1;

                gMainStatus.SubStep ++;
            }
            else
            {
                gError.ErrorCode.all |= ERROR_PROGRAM_LOGIC;
                gError.ErrorInfo[4].bit.Fault1 = 1;
            }
            break;

        case 2:
            if(gIPMInitPos.Step == 0)
        	{
    			SetIPMPos((Uint)gIPMPos.InitPos);
                SetIPMPos_ABZRef((Uint)gIPMPos.InitPos);

        		if(abs((int)(gIPMPos.PowerOffPosDeg - gIPMPos.RotorPos)) < 3641)
        		{

        		}

                gIPMInitPos.Flag = 1;
        		DisableDrive();
        		gMainStatus.SubStep ++;
        	}
            break;

        case 3:
            	InitSetPWM();
   	            InitSetAdc();
                SetInterruptEnable();
                gMainStatus.SubStep ++;
                break;

        case 4:
            PrepareParForRun();
            gMainStatus.RunStep = STATUS_RUN;
            gMainStatus.SubStep = 0;
            gMainStatus.PrgStatus.all = 0;
            EnableDrive();
            break;

        default:
            gError.ErrorCode.all |= ERROR_PROGRAM_LOGIC;
            gError.ErrorInfo[4].bit.Fault1 = 2;

            break;
    }
}

void SynCalRealPos(void)
{
    long    mQepPos;
    long    mCntMod;
    long    m_ABZCntMod;
	long    m_4Pluse;
    long    m_DetaCnt;
	int     m_Pos;
    int     m_ABZPos;
    int     mSign;

    long  m_ABZQepPos;
    long  m_ABZDetaCnt;

    DINT;
    mQepPos = GetQepCnt();
    m_DetaCnt = mQepPos - gIPMPos.QepBak;
    gIPMPos.QepBak = mQepPos;
    gIPMPos.QepTotal += m_DetaCnt;

	m_4Pluse = ((long)gPGData.PulseNum)<<2;
	mCntMod = gIPMPos.QepTotal % m_4Pluse;
	m_Pos = (mCntMod<<14)/(int)gPGData.PulseNum;
	gIPMZero.FeedPos = m_Pos * gMotorExtInfo.Poles;

    if(gPGData.PGType == PG_TYPE_ABZ)
    {
        m_ABZQepPos = GetQepCnt();
        m_ABZDetaCnt = m_ABZQepPos - gIPMPos.ABZ_QepBak;
        gIPMPos.ABZ_QepBak = m_ABZQepPos;
        gIPMPos.ABZ_QepTotal += m_ABZDetaCnt;

        m_4Pluse = ((long)gPGData.PulseNum)<<2;
	    m_ABZCntMod = gIPMPos.ABZ_QepTotal % m_4Pluse;
        m_ABZPos = (m_ABZCntMod<<14)/(int)gPGData.PulseNum;
        gIPMZero.FeedABZPos = m_ABZPos* gMotorExtInfo.Poles;

    }
    EINT;

    if(labs(gIPMPos.QepTotal) > (50L*4L* gPGData.PulseNum))
    {
        mSign = (gIPMPos.QepTotal > 0) ? 1 : -1;
        gIPMPos.QepTotal -= (30L*4L* gPGData.PulseNum * mSign);
    }
    if(labs(gIPMPos.ABZ_QepTotal)>(50L*4L* gPGData.PulseNum))
    {
        mSign = (gIPMPos.ABZ_QepTotal > 0) ? 1 : -1;
        gIPMPos.ABZ_QepTotal -= (30L*4L* gPGData.PulseNum * mSign);
    }

}

void SynCalLdAndLq(Uint m_Pos)
{
	int m_Cos1,m_Cos2;
	int m_CoffA,m_CoffB;
	Uint m_Angle;

	m_Angle = (m_Pos<<1);
	m_Cos1 = qsin(16384 - (int)m_Angle);
	m_Angle += 43691;
	m_Cos2 = qsin(16384 - (int)m_Angle);

	m_CoffB = gIPMInitPos.LPhase[2] - gIPMInitPos.LPhase[1];
	m_CoffB = ((long)m_CoffB<<15)/(m_Cos1 - m_Cos2);
	m_CoffA = gIPMInitPos.LPhase[1] + (((long)m_CoffB * (long)m_Cos1)>>15);

	m_CoffA = m_CoffA>>1;
	m_CoffB = m_CoffB>>1;
	gIPMInitPos.Ld = m_CoffA - m_CoffB;
	gIPMInitPos.Lq = m_CoffA + m_CoffB;

    gMotorExtReg.LD = gIPMInitPos.Ld;
    gMotorExtReg.LQ = gIPMInitPos.Lq;
}

void IPMCalAcrPIDCoff(void)
{
    Uint temp;

#if 1

	temp = ((Ulong)gMotorExtReg.LD * (Ulong)gMotorInfo.CurrentGet) / gMotorInfo.Votage;
    gPmParEst.IdKp = (Ulong)temp * 1290L >> 10;

	temp = ((Ulong)gMotorExtReg.LQ * (Ulong)gMotorInfo.CurrentGet) / gMotorInfo.Votage;
    gPmParEst.IqKp = (Ulong)temp * 1290L >> 10;

	gPmParEst.IdKi = (((Ulong)gMotorExtReg.RsPm * (Ulong)gMotorInfo.CurrentGet)/gMotorInfo.Votage)>>(1);
	gPmParEst.IqKi = gPmParEst.IdKi;
#else
;   gPmParEst.IdKp = 1000;
    gPmParEst.IdKi = 1000;
    gPmParEst.IqKp = 1000;
    gPmParEst.IqKi = 1000;
#endif

}

void PrepPmsmCsrPrar()
{
    long    ImKp, ImKi, ItKp, ItKi, temp;
    int     Nf;
    int     sGain;
    int     tempFreq;

    tempFreq = (gBasePar.FcSetApply > 40) ? gBasePar.FcSetApply : 40;

	ImKp = (long)gVCPar.AcrImKp * tempFreq  / 60;
    ImKi = gVCPar.AcrImKi;

    ItKp = (long)gVCPar.AcrItKp * tempFreq  / 60;
    ItKi = gVCPar.AcrItKi;

    gImAcrQ24.KP = ImKp;
    gItAcrQ24.KP = ItKp;

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

void PmFluxWeakDeal()
{
    int     frq;
    int     minFrq;
    long    mVd;

    long    temp1, temp2;
    int     maxCur;

    gFluxWeak.IqLpf   = Filter4(gIMTSet.T>>12, gFluxWeak.IqLpf);
    gFluxWeak.VoltLpf = Filter4(gOutVolt.VoltApply, gFluxWeak.VoltLpf);

    minFrq = 5;
    frq = (abs(gMainCmd.FreqSyn) > minFrq) ? abs(gMainCmd.FreqSyn) : minFrq;
    gFluxWeak.AbsFrqLpf = Filter4(frq, gFluxWeak.AbsFrqLpf);

    temp1 = (long)gFluxWeak.AbsFrqLpf * gMotorExtPer.LQ >> 9;
    mVd   = (long)gFluxWeak.IqLpf * temp1 >> 15;
    gFluxWeak.Vd = __IQsat(mVd, 32767, -32767);

    temp1 = (long)gOutVolt.MaxOutVolt - 400L;
    temp2 = temp1 * temp1 - (long)gFluxWeak.Vd * gFluxWeak.Vd;
    temp2 = __IQsat(temp2, 0x7FFFFFFF, 0);
    gFluxWeak.Vq   = qsqrt(temp2);

    temp2 = ((long)gFluxWeak.Vq<<15)/gFluxWeak.AbsFrqLpf - gMotorExtPer.FluxRotor;
    gFluxWeak.FluxSd = __IQsat(temp2, 0, -32767);
    temp1 = (((long)gFluxWeak.FluxSd <<15) /gMotorExtPer.LD) >>6;
    gFluxWeak.IdSet = temp1 * gFluxWeak.CoefFlux / 100L;

    maxCur = 4096L * gFluxWeak.IdMax /100;
    gFluxWeak.IdSet = __IQsat(gFluxWeak.IdSet, 0, -maxCur);

    if(gFluxWeak.Mode == 2)
    {
        gFluxWeak.CoefAdj = __IQsat(gFluxWeak.CoefAdj, 1000, 10);
        gFluxWeak.AdjustLimit = (long)maxCur * 100L / gFluxWeak.CoefAdj;

        gFluxWeak.AdjustId += (gOutVolt.MaxOutVolt - gFluxWeak.VoltLpf) / 4;
        gFluxWeak.AdjustId = __IQsat(gFluxWeak.AdjustId, gFluxWeak.AdjustLimit, -gFluxWeak.AdjustLimit);

        temp1 = (long)gFluxWeak.AdjustId * gFluxWeak.CoefAdj / (100L);
        gFluxWeak.IdSet += temp1;
        gFluxWeak.IdSet = __IQsat(gFluxWeak.IdSet, 0, -maxCur);
    }

    gIMTSet.M = (gFluxWeak.Mode) ? ((long)gFluxWeak.IdSet << 12) : 0;
    return;
}

void PmDecoupleDeal()
{
    long temp;

    gPmDecoup.Omeg = Filter2(gMainCmd.FreqSyn, gPmDecoup.Omeg);
    gPmDecoup.Isd  = Filter2((gIMTQ24.M>>12), gPmDecoup.Isd);
    gPmDecoup.Isq  = Filter2((gIMTQ24.T>>12), gPmDecoup.Isq);

    temp = (long)gPmDecoup.Isd * gMotorExtPer.LD >> 9;
    gPmDecoup.PhiSd = temp + gMotorExtPer.FluxRotor;
    gPmDecoup.RotVq = (long)gPmDecoup.Omeg * gPmDecoup.PhiSd >> 15;

    gPmDecoup.PhiSq = (long)gPmDecoup.Isq * gMotorExtPer.LQ >> 9;
    gPmDecoup.RotVd = - (long)gPmDecoup.Omeg * gPmDecoup.PhiSq >> 15;
}
