
#include "MotorParaIDinclude.h"

interrupt void RrLoInPeriodAndCmpInt(void);

void RsIdentify(void);
void RrLoIdentify(void);
void LmIoIdentify(void);
void SetPwmForRrLoId(void);
void InitRrLoVariable(void);
void EnableTimer(void);
void RrWaitAddGetUdc(void);
void RrGetVoltReBegin(void);
void RrLoCalVaule(void);
void InitLmIoVaribale(void);
void LmIoCalValue(void);

void RsIdentify(void)
{
	Uint  m_UData1,m_UData2,m_MaxCur;
	Ulong m_ULong1,m_ULong2;

    m_MaxCur = ((Ulong)gInvInfo.InvCurrent * 3277)>>12;
	switch(gGetParVarable.IdSubStep)
	{
		case 1:
		    if(INV_VOLTAGE_1140V == gInvInfo.InvVoltageType)
              {
               gUVCoff.Rs_PRD =SHORT_GND_PERIOD_1140;
              }
            else if(INV_VOLTAGE_690V == gInvInfo.InvVoltageType)
              {
               gUVCoff.Rs_PRD =SHORT_GND_PERIOD_690;
              }
            else
              {
               gUVCoff.Rs_PRD =SHORT_GND_PERIOD;
              }
			gUVCoff.Comper     = gUVCoff.Rs_PRD/2;
			gUVCoff.Number     = 0;
			gUVCoff.TotalU     = 0;
			gUVCoff.TotalV     = 0;
			gUVCoff.TotalI     = 0;
			gUVCoff.TotalIL    = 0;
			gUVCoff.TotalVolt  = 0;
			gUVCoff.TotalVoltL = 0;

			EALLOW;
            EPwm1Regs.ETSEL.bit.INTEN = 0;
			EPwm1Regs.TBPRD = gUVCoff.Rs_PRD;
			EPwm1Regs.CMPB  = EPwm1Regs.TBPRD - gADC.DelayApply;
			EPwm2Regs.TBPRD = gUVCoff.Rs_PRD;
			EPwm3Regs.TBPRD = gUVCoff.Rs_PRD;

			EPwm1Regs.CMPA.half.CMPA = gUVCoff.Comper;
			EPwm2Regs.CMPA.half.CMPA = gUVCoff.Comper;

			EPwm3Regs.AQCSFRC.all = 0x0A;
			EPwm3Regs.DBCTL.all   = 0x0000;
			EDIS;

			gGetParVarable.IdSubStep = 2;
			break;

		case 2:
            if(0 == gUVCoff.IdRsDelay)
            {
			    gGetParVarable.IdSubStep = 3;
			    EnableDrive();
            }
            else
            {
                gUVCoff.IdRsDelay--;
            }
			break;

		case 4:
			gUVCoff.Number++;
			if(gUVCoff.Number >= 512)
			{
				gUVCoff.TotalVoltL += gUDC.uDCFilter;
				gUVCoff.TotalIL    += abs(gIUVWQ12.U);
			}
			if(gUVCoff.Number >= 1024)
			{
				gUVCoff.ComperL = gUVCoff.Comper;
				gGetParVarable.IdSubStep = 5;
			}
			break;

		case 3:
		case 5:

			m_UData1 = (1024UL * (Ulong)gMotorInfo.CurrentGet)/gMotorInfo.Current ;
			if(gGetParVarable.IdSubStep == 5)
            {
                m_UData1 = (4096UL * (Ulong)gMotorInfo.CurrentGet)/gMotorInfo.Current ;
                if( 16 < gInvInfo.InvTypeApply )
                {
                    m_UData1 = (3500UL * (Ulong)gMotorInfo.CurrentGet)/gMotorInfo.Current ;
                }
            }
            if( gMotorInfo.Current > m_MaxCur )
            {
                m_UData1 = (((long)m_MaxCur) * (long)m_UData1) / gMotorInfo.Current;
            }

			gUVCoff.Number = 0;
			if(abs(gIUVWQ12.U) >= m_UData1)
			{
				gGetParVarable.IdSubStep++;
			}
			else
			{
			    if(gInvInfo.InvTypeApply > 16 )
                {
                   gUVCoff.Comper += 1;
                }
                else
                {
				   gUVCoff.Comper += 4;
                }
				if(gUVCoff.Comper >= gUVCoff.Rs_PRD)
				{
			        gUVCoff.Comper = gUVCoff.Rs_PRD/2;

			        gError.ErrorCode.all |= ERROR_OUTPUT_LACK_PHASE;
                    gError.ErrorInfo[1].bit.Fault1 = 11;
				}
				else
				{
					EALLOW;
					EPwm1Regs.CMPA.half.CMPA = gUVCoff.Comper;
					EPwm2Regs.CMPA.half.CMPA = gUVCoff.Rs_PRD - gUVCoff.Comper;
					EDIS;
				}
			}
			break;

		case 6:
			gUVCoff.TotalU += abs(gIUVWQ12.U);
			gUVCoff.TotalV += abs(gIUVWQ12.V);
			gUVCoff.Number++;
			if(gUVCoff.Number >= 512)
			{
				gUVCoff.TotalVolt += gUDC.uDCFilter;
				gUVCoff.TotalI += abs(gIUVWQ12.U);

				if(gUVCoff.Number >= 1024)
				{
                    DisableDrive();
					gUVCoff.Number = 0;
					m_UData1 = (gUVCoff.TotalU << 4) / (Uint)(gUVCoff.TotalV >> 8);
					gUVCoff.UDivVSave = ((Ulong)m_UData1 * 1000)>>12;

					m_UData2 = gUVCoff.Rs_PRD/2;

					m_UData1 = (gUVCoff.TotalVoltL<<5)/m_UData2;
					m_ULong1 = ((Ulong)m_UData1 * (Ulong)(gUVCoff.ComperL - m_UData2));
					m_UData1 = (gUVCoff.TotalVolt<<5)/m_UData2;
					m_ULong1 = ((Ulong)m_UData1 * (Ulong)(gUVCoff.Comper  - m_UData2)) - m_ULong1;
                    m_ULong1 =  m_ULong1 * 10;

					m_UData2 = (gUVCoff.TotalI - gUVCoff.TotalIL)>>7;
					m_ULong2 = ((Ulong)m_UData2 * (Ulong)gMotorInfo.Current)>>10;

					while((m_ULong2>>16) != 0)
					{
						m_ULong1 = m_ULong1>>1;
						m_ULong2 = m_ULong2>>1;
					}
					m_UData2 = m_ULong1/(Uint)m_ULong2;
                    m_UData2 = ((Ulong)m_UData2 * (Ulong)1000)>>11;
                    if(0 == gUVCoff.IdRsCnt)
                    {
                        gUVCoff.IdRsBak = m_UData2;
                        gGetParVarable.IdSubStep = 1;
                        gUVCoff.IdRsCnt++;
                        gUVCoff.IdRsDelay = 1000;
                    }
                    else
                    {
                        if(MOTOR_TYPE_PM == gMotorInfo.MotorType)
                        {
                           gMotorExtReg.RsPm = m_UData2;
                        }
                        else
                        {
                    	    gMotorExtReg.R1 = m_UData2;
                        }
					    gGetParVarable.IdSubStep = 7;
                    }
				}
			}
			break;

		case 7:
		default:
			DisableDrive();
  			gGetParVarable.IdSubStep = 1;
            gGetParVarable.ParEstMstep++;
            InitSetPWM();
			break;
	}
}

void RrLoIdentify(void)
{
    switch( gGetParVarable.IdSubStep )
    {
        case 1:
       	    gRrLoIdentify.WaitCnt++;
            EALLOW;
            PieCtrlRegs.PIEIER1.bit.INTx1 = 0;
            EDIS;
			if( gRrLoIdentify.WaitCnt < 1000 )
			{
			    RrWaitAddGetUdc();
			}
			else
			{
                SetPwmForRrLoId();
			    InitRrLoVariable();
                EnableTimer();

	            gRrLoIdentify.PwmCompareValue = 200;

	            gRrLoIdentify.CurrentRatio = 8192L * (long)gMotorInfo.Current / gInvInfo.InvCurrent;
	            gRrLoIdentify.CurrentRatio = (gRrLoIdentify.CurrentRatio > 8992)?8992:gRrLoIdentify.CurrentRatio;
                gRrLoIdentify.RrL0CycleCnt = 0;
	            gRrLoIdentify.RrAndRsAccValue = 0;
 	            gRrLoIdentify.LoAccValue = 0;
				gGetParVarable.IdSubStep = 2;
			}
            break;

        case 2:

            if(gRrLoIdentify.WaitCnt != 0)
			{
				gRrLoIdentify.WaitCnt--;
				if(gRrLoIdentify.WaitCnt != 0)
				{
					RrWaitAddGetUdc();
				}
				else
				{
					InitRrLoVariable();
                    EnableTimer();
				}
			}
			else
			{
				if((gRrLoIdentify.CurrentMax - 3200) > gRrLoIdentify.CurrentRatio)
				{
                    gRrLoIdentify.PwmCompareValue -= 20;
					gRrLoIdentify.WaitCnt = 20;
					RrGetVoltReBegin();
				}
				else if(1 == gRrLoIdentify.RrL07PulseOverSig)
				{

						if(gRrLoIdentify.CurrentMax > gRrLoIdentify.CurrentRatio)
						{
							gGetParVarable.IdSubStep = 3;
						}
						else
						{
							if((gRrLoIdentify.CurrentMax * 2) > gRrLoIdentify.CurrentRatio)
							{
									gRrLoIdentify.PwmCompareValue += 50;
							}
							else
							{
									gRrLoIdentify.PwmCompareValue += 100;
							}
							gRrLoIdentify.WaitCnt = 20;
							RrGetVoltReBegin();
						}

			    }
			}
            break;

        case 3:
			if(gRrLoIdentify.WaitCnt != 0)
			{
				gRrLoIdentify.WaitCnt--;
				if(gRrLoIdentify.WaitCnt != 0)
				{
					RrWaitAddGetUdc();
				}
				else
				{
					InitRrLoVariable();
                    EnableTimer();
				}
			}
			else
			{
				if(1 == gRrLoIdentify.RrL07PulseOverSig)
				{
					if(14 == gRrLoIdentify.SampleTimes)
					{
						RrLoCalVaule();
						if(6 <= gRrLoIdentify.RrL0CycleCnt)
						{
							gMotorExtReg.R2 = ((gRrLoIdentify.RrAndRsAccValue - gRrLoIdentify.RrAndRsMax - gRrLoIdentify.RrAndRsMin) / 4) - gMotorExtReg.R1;
							gMotorExtReg.L0 = (gRrLoIdentify.LoAccValue - gRrLoIdentify.LoMax - gRrLoIdentify.LoMin) / 4;
							if((gInvInfo.InvType > 18) && (gInvInfo.InvType < 23))
							{
								gMotorExtReg.R2 = ((long)gMotorExtReg.R2 * 768L)>>10;
							}
							else if(gInvInfo.InvType > 23)
							{
								gMotorExtReg.R2 = ((long)gMotorExtReg.R2 * 666L)>>10;
							}
							gGetParVarable.IdSubStep = 4;
						}
						else
						{
							gRrLoIdentify.WaitCnt = 20;
							RrGetVoltReBegin();
						}
					}
					else
					{
						gRrLoIdentify.WaitCnt = 20;
						RrGetVoltReBegin();
					}
				}
			}
            break;

        case 4:
        default:
	        DisableDrive();
            EALLOW;
            PieCtrlRegs.PIEIER3.all = 1;
            PieVectTable.EPWM1_INT 	= &EPWM1_zero_isr;
            EDIS;
            gGetParVarable.IdSubStep = 1;
            gGetParVarable.ParEstMstep++;
	        InitSetPWM();
   	        InitSetAdc();
	        SetInterruptEnable();
            break;
    }
}

void SetPwmForRrLoId(void)
{
    EALLOW;
    PieVectTable.EPWM1_INT = &RrLoInPeriodAndCmpInt;
	EPwm1Regs.ETSEL.all = 0x0000A;
    PieVectTable.EPWM2_INT = &RrLoInPeriodAndCmpInt;
    EPwm2Regs.ETSEL.all = 0x000C;
    EPwm2Regs.ETPS.all  = 1;

	EPwm1Regs.TBPRD = 12000;
	EPwm1Regs.TBCTL.bit.CTRMODE = 3;
#ifdef DSP_CLOCK100
	EPwm1Regs.TBCTL.bit.CLKDIV = 1;
    EPwm1Regs.TBCTL.bit.HSPCLKDIV = 5;
#endif
#ifdef DSP_CLOCK60
    EPwm1Regs.TBCTL.bit.CLKDIV = 0;
    EPwm1Regs.TBCTL.bit.HSPCLKDIV = 6;
#endif
	EPwm1Regs.TBCTR = 0;

	EPwm1Regs.CMPA.half.CMPA = 200;
	EPwm1Regs.CMPCTL.all = 0x0100;
	EPwm1Regs.AQSFRC.all = 0x000C0;
	EPwm1Regs.AQCSFRC.all = 0x00A;
	EPwm1Regs.DBCTL.all = 0x0000;
	EPwm1Regs.ETCLR.bit.INT = 1;

	EPwm2Regs.TBPRD = 12000;
    EPwm2Regs.TBCTL.bit.CTRMODE = 3;
#ifdef DSP_CLOCK100
	EPwm2Regs.TBCTL.bit.CLKDIV = 1;
    EPwm2Regs.TBCTL.bit.HSPCLKDIV = 5;
#endif
#ifdef DSP_CLOCK60
    EPwm2Regs.TBCTL.bit.CLKDIV = 0;
    EPwm2Regs.TBCTL.bit.HSPCLKDIV = 6;
#endif
    EPwm2Regs.TBCTR = 0;
    EPwm2Regs.CMPA.half.CMPA = 200;
    EPwm2Regs.CMPCTL.all = 0x0100;
    EPwm2Regs.AQCTLA.all = 0;
	EPwm2Regs.AQSFRC.all = 0x00C0;
	EPwm2Regs.AQCSFRC.all = 0x0006;
	EPwm2Regs.DBCTL.all = 0x0000;
	EPwm2Regs.ETCLR.bit.INT = 1;

#ifdef DSP_CLOCK100
	EPwm3Regs.TBCTL.bit.CLKDIV = 1;
    EPwm3Regs.TBCTL.bit.HSPCLKDIV = 5;
#endif
#ifdef DSP_CLOCK60
    EPwm3Regs.TBCTL.bit.CLKDIV = 0;
    EPwm3Regs.TBCTL.bit.HSPCLKDIV = 6;
#endif
	EPwm3Regs.TBPRD = 12000;
    EPwm3Regs.AQCTLA.all = 0;
	EPwm3Regs.AQSFRC.all = 0x00C0;
	EPwm3Regs.AQCSFRC.all = 0x0006;
	EPwm3Regs.DBCTL.all = 0x0000;
    PieCtrlRegs.PIEIER3.all = 3;
	EDIS;
}

void InitRrLoVariable(void)
{
    int m_index;

	gRrLoIdentify.WaitCnt = 0;
    for(m_index=0; m_index<14; m_index++)
    {
        gRrLoIdentify.IsSampleValue[m_index] = 0;
    }
	gRrLoIdentify.UdcVoltage = gUDC.uDCFilter;
	gRrLoIdentify.CurrentMax = 0;
	gRrLoIdentify.SampleTimes = 0;
    gRrLoIdentify.RrL07PulseOverSig = 0;
}

void EnableTimer(void)
{
	EALLOW;
	EPwm1Regs.AQCTLA.all = 0x0021;
	EPwm1Regs.AQCSFRC.all = 0x008;
    EPwm1Regs.TBCTR = 0;
    EPwm2Regs.TBCTR = 0;
	EPwm1Regs.TBCTL.bit.CTRMODE = 0;
	EPwm2Regs.TBCTL.bit.CTRMODE = 0;
	EDIS;
	EINT;
    EnableDrive();
}

interrupt void RrLoInPeriodAndCmpInt(void)
{
	long		m_Iu;

   	EALLOW;
   	EPwm1Regs.ETCLR.bit.INT = 1;
    EPwm2Regs.ETCLR.bit.INT = 1;
   	EDIS;

	EINT;
    RrWaitAddGetUdc();
    gRrLoIdentify.UdcVoltage = Filter4(gUDC.uDCFilter, gRrLoIdentify.UdcVoltage);
	m_Iu = (int)(ADC_IU - (Uint)32768);
	m_Iu =  (m_Iu - (long)gExcursionInfo.ErrIu)>>1;

	gRrLoIdentify.IsSampleValue[gRrLoIdentify.SampleTimes] = __IQsat(m_Iu, 32767, -32767);
    if(gRrLoIdentify.CurrentMax < abs(m_Iu))
	{
		gRrLoIdentify.CurrentMax = abs(m_Iu);
	}
	gRrLoIdentify.SampleTimes++;
    gRrLoIdentify.SampleTimes = __IQsat(gRrLoIdentify.SampleTimes, 15, 0);

	if(gRrLoIdentify.SampleTimes >= 14)
	{
		DisableDrive();
        EALLOW;
        EPwm1Regs.TBCTL.bit.CTRMODE = 3;
		EPwm1Regs.TBCTR = 0;
		EPwm1Regs.AQCSFRC.all = 0x00A;
		EPwm2Regs.TBCTL.bit.CTRMODE = 3;
        EPwm2Regs.TBCTR = 0;

		EDIS;
		gRrLoIdentify.RrL07PulseOverSig = 1;
    }
    CBCLimitCurPrepare();
    DINT;

   	PieCtrlRegs.PIEACK.all = PIEACK_GROUP3;
}

void RrWaitAddGetUdc(void)
{
    Uint counter;

	EALLOW;
	ADC_RESET_SEQUENCE;
	ADC_CLEAR_INT_FLAG;
	ADC_START_CONVERSION;
	EDIS;

    counter = 0;
	while(ADC_END_CONVERSIN == 0)
    {
        counter ++;
        if(counter > 50)
        {
            gError.ErrorCode.all |= ERROR_TUNE_FAIL;
            break;
        }
	}

    EALLOW;
    ADC_CLEAR_INT_FLAG;
    EDIS;

    GetUDCInfo();
}

void RrGetVoltReBegin(void)
{
	DisableDrive();
    gRrLoIdentify.PwmCompareValue = __IQsat(gRrLoIdentify.PwmCompareValue,2000,1);

    EALLOW;
	EPwm1Regs.CMPA.half.CMPA = gRrLoIdentify.PwmCompareValue;
    EPwm2Regs.CMPA.half.CMPA = gRrLoIdentify.PwmCompareValue;

	EPwm1Regs.TBCTL.bit.CTRMODE = 3;
	EPwm1Regs.TBCTR = 0;
	EPwm1Regs.AQCSFRC.all = 0x00A;
	EPwm2Regs.TBCTL.bit.CTRMODE = 3;
    EPwm2Regs.TBCTR = 0;

	EDIS;
}

void RrLoCalVaule(void)
{
    int     m_IdVoltage,m_zeroVoltageTime,m_index,m_CurrMultiple;
    long    m_RrRsCalValue,m_LoCalVAlue,m_IsUdcVoltDiff,m_IsZeroVoltDiff,m_IsUdcVoltageSum,m_IsZeroVoltageSum;
    long    m_Value1,m_Value2;
    m_IsUdcVoltageSum = 0;
    m_IsUdcVoltDiff = 0;
    m_IsZeroVoltageSum = 0;
    m_IsZeroVoltDiff = 0;
	m_IdVoltage = ((long)gRrLoIdentify.UdcVoltage * 43690L)>>16;
	m_zeroVoltageTime = 12000 - gRrLoIdentify.PwmCompareValue;
    for(m_index=2;m_index<7;m_index++)
        m_IsUdcVoltDiff += ((long)gRrLoIdentify.IsSampleValue[2*m_index]-(long)gRrLoIdentify.IsSampleValue[2*m_index-1]);
    m_IsUdcVoltDiff = abs(m_IsUdcVoltDiff);
    for(m_index=2;m_index<7;m_index++)
        m_IsZeroVoltDiff += ((long)gRrLoIdentify.IsSampleValue[2*m_index+1]-(long)gRrLoIdentify.IsSampleValue[2*m_index]);
    m_IsZeroVoltDiff = abs(m_IsZeroVoltDiff);
	for(m_index=3;m_index<13;m_index++)
        m_IsUdcVoltageSum += (long)abs(gRrLoIdentify.IsSampleValue[m_index]);
    m_IsUdcVoltageSum = m_IsUdcVoltageSum>>1;
    for(m_index=4;m_index<14;m_index++)
        m_IsZeroVoltageSum += (long)abs(gRrLoIdentify.IsSampleValue[m_index]);
    m_IsZeroVoltageSum = m_IsZeroVoltageSum>>1;

	m_Value1 = m_IsUdcVoltageSum + ((long long)m_IsZeroVoltageSum * (long long)m_IsUdcVoltDiff * (long long)m_zeroVoltageTime) /
                (m_IsZeroVoltDiff * (long)gRrLoIdentify.PwmCompareValue);

    m_Value2 = (m_Value1 * (llong)gInvInfo.InvCurrent)>>14;

    #ifdef TMS320F2808
    m_Value2 = ((llong)m_Value2 * 5793L)>>11;
    #else
    m_Value2 = ((llong)m_Value2 * 6372L)>>11;
    #endif

    m_Value2 = ((llong)m_Value2 * (long)gInvInfo.CurrentCoff) / 1000;
    m_RrRsCalValue = ((long)m_IdVoltage * 50000L)/ m_Value2;
    m_LoCalVAlue = ((long long)m_RrRsCalValue * (long long)m_IsZeroVoltageSum * (long long)m_zeroVoltageTime) / (m_IsZeroVoltDiff * 50000L);
    m_LoCalVAlue = m_LoCalVAlue>>1;

	if(0 == gRrLoIdentify.RrL0CycleCnt)
	{
        gRrLoIdentify.LoMax = m_LoCalVAlue;
        gRrLoIdentify.LoMin = m_LoCalVAlue;
        gRrLoIdentify.RrAndRsMax = m_RrRsCalValue;
        gRrLoIdentify.RrAndRsMin = m_RrRsCalValue;
	}
	else
	{
	    gRrLoIdentify.LoMax = (m_LoCalVAlue > gRrLoIdentify.LoMax)?m_LoCalVAlue:gRrLoIdentify.LoMax;
        gRrLoIdentify.LoMin = (m_LoCalVAlue < gRrLoIdentify.LoMin)?m_LoCalVAlue:gRrLoIdentify.LoMin;
        gRrLoIdentify.RrAndRsMax = (m_RrRsCalValue > gRrLoIdentify.RrAndRsMax)?m_RrRsCalValue:gRrLoIdentify.RrAndRsMax;
        gRrLoIdentify.RrAndRsMin = (m_RrRsCalValue < gRrLoIdentify.RrAndRsMin)?m_RrRsCalValue:gRrLoIdentify.RrAndRsMin;
	}
	gRrLoIdentify.RrAndRsAccValue += m_RrRsCalValue;
	gRrLoIdentify.LoAccValue += m_LoCalVAlue;
    gRrLoIdentify.RrL0CycleCnt++;
}

void ImCalKpKi(void)
{
	Ulong tempL;
    Ulong tempL1;
    Ulong temp;

    tempL1 = gMotorExtReg.L0/10 + gMotorExtReg.LM;
    tempL  = gMotorExtReg.L0 * gMotorExtReg.LM / tempL1 ;
    tempL += gMotorExtReg.L0;

	temp = ((Ulong)tempL * (Ulong)gMotorInfo.CurrentGet) / gMotorInfo.Votage;
    gPmParEst.IdKp = (Ulong)temp * 1290L >> 10;
	gPmParEst.IqKp = gPmParEst.IdKp;

	gPmParEst.IdKi = (((Ulong)gMotorExtReg.R1 * (Ulong)gMotorInfo.CurrentGet)/gMotorInfo.Votage)>>1;
	gPmParEst.IqKi = gPmParEst.IdKi >> 1;
}

void LmIoIdentify(void)
{

    switch(gGetParVarable.IdSubStep)
    {
        case 1:
            InitLmIoVaribale();
            gGetParVarable.IdSubStep++;

        case 2:
            gLmIoIdentify.WaitCnt++;
            if( 800 < gLmIoIdentify.WaitCnt )
            {
                gLmIoIdentify.WaitCnt = 0;
                gGetParVarable.IdSubStep++;
			    gGetParVarable.StatusWord = TUNE_ACC;
                EnableDrive();

                gPGData.imPgEstTick = 0;
                gPGData.imDirAdder = 0;
                gPGData.imFreqErr = 0;
                gPGData.imFrqEncoder = 0;
                gLmIoIdentify.VFOvShock = gVFPar.VFOvShock;
            }
            break;

        case 3:
            if(gMainCmd.FreqSyn >= (gLmIoIdentify.DestinationFreq - 11))
            {

                if(gLmIoIdentify.VFOvShock != 0)
                 {
                   gLmIoIdentify.VFOvShock--;
                   gVFPar.VFOvShock = gLmIoIdentify.VFOvShock;
                    break;
                  }
                else
                 {
                    if(gLmIoIdentify.WaitCnt < 500)
                    {
                        gLmIoIdentify.WaitCnt ++;
                        break;
                    }
                 }

                gPGData.imPgEstTick ++;

                if((long)gFVCSpeed.SpeedEncoder * gMainCmd.FreqSyn < 0)
                {

                    gPGData.imDirAdder ++;
                }
                else
                {
                    gPGData.imDirAdder --;
                }

                gPGData.imFreqErr += (gMainCmd.FreqSyn - abs(gFVCSpeed.SpeedEncoder));
                gPGData.imFrqEncoder += abs(gFVCSpeed.SpeedEncoder);

                if(gPGData.imPgEstTick > 128)
                {
                    int maxFrqErr;

                    if(gPGData.imDirAdder > 50)
                    {
                        gPGData.PGDir = (gPGData.SpeedDir ^ 0x01);
                    }
                    else if(gPGData.imDirAdder > -50)
                    {
                        gPGData.PGErrorFlag = 1;
                    }

                    maxFrqErr = (long)gLmIoIdentify.DestinationFreq * 100L >> 10;
                    if((gPGData.imFreqErr >> 7) > maxFrqErr)
                    {
                        gPGData.PGErrorFlag = 2;
                    }
                    if((gPGData.imFrqEncoder>>7) < maxFrqErr)
                    {
                        gPGData.PGErrorFlag = 1;
                    }

                    gLmIoIdentify.WaitCnt = 0;
                    gLmIoIdentify.lImAccValue = 0;
                    gLmIoIdentify.lIsAccValue = 0;
                    gLmIoIdentify.DataSavedNum = 0;
			        gGetParVarable.IdSubStep++;
                    }
                }

            break;

        case 4:

			if(gMainCmd.FreqSyn < (gLmIoIdentify.DestinationFreq - 20))
			{
                gGetParVarable.IdSubStep++;
            }

	        if(120 <= gLmIoIdentify.WaitCnt)
	        {
				gMotorExtReg.LM = (((gLmIoIdentify.LmAccValue * 10L) / gLmIoIdentify.WaitCnt) - (long)gMotorExtReg.L0) / 10;
				gMotorExtReg.I0 = gLmIoIdentify.IoAccValue / gLmIoIdentify.WaitCnt;
                #ifdef TMS320F28035
                gMotorExtReg.LM = ((long)gMotorExtReg.LM * 7447L) >> 13;
                gMotorExtReg.I0 = ((long)gMotorExtReg.I0 * 9011L) >> 13;
                #endif

	            gGetParVarable.StatusWord = TUNE_DEC;
                gLmIoIdentify.WaitCnt = 0;
				gGetParVarable.IdSubStep++;
			}
            break;

        case 5:
            if( 8 > gMainCmd.FreqSyn )
            {
                DisableDrive();
                gGetParVarable.ParEstMstep++;
                gGetParVarable.IdSubStep = 1;

                ImCalKpKi();
            }
            break;

    }

}

void InitLmIoVaribale(void)
{
    gLmIoIdentify.WaitCnt = 0;
    gLmIoIdentify.DestinationFreq = ((long)gMotorInfo.FreqPer<<3)/10;
    gLmIoIdentify.LmAccValue = 0;
    gLmIoIdentify.IoAccValue = 0;
}

void LmIoCalValue(void)
{
    long    m_ComIm,m_ComLma,m_ComLmb,m_ComLmc;
	m_ComIm  =  gLmIoIdentify.lIsAccValue / 100;
	m_ComLma = ((gLmIoIdentify.lImAccValue / 200L ) * 1448L)>>10;
    m_ComLmb = (m_ComIm * m_ComIm)>>13;
	m_ComLmc = ((long)gOutVolt.Volt * m_ComLma) / m_ComLmb;
	m_ComLmc = m_ComLmc * (long)gMotorInfo.Votage * 10L / (long)gInvInfo.InvCurrent;
    m_ComLmc = (m_ComLmc * 739L)>>13;
    m_ComLmc = m_ComLmc * 1000L / (long)gInvInfo.CurrentCoff;
    m_ComLmb = (m_ComLmc * 10000L) / (long)gMainCmd.FreqReal;
	m_ComLmc = ((m_ComIm * (long)gInvInfo.InvCurrent)>>15) * (long)gInvInfo.CurrentCoff / 707L;
    gLmIoIdentify.LmAccValue += m_ComLmb;
    gLmIoIdentify.IoAccValue += m_ComLmc;
   	gLmIoIdentify.WaitCnt++;
}

void LmIoInPeriodInt(void)
{
    long    m_lIm,m_lIt,m_lIs;
    int     m_VoltagePhase;
    if(( 4 != gGetParVarable.IdSubStep )||
        ( 120 <= gLmIoIdentify.WaitCnt ))
    {
        return;
    }
    m_VoltagePhase = gPhase.IMPhase>>16;
   	m_lIm = abs(((long)gExcursionInfo.IvValue * (long)qsin(m_VoltagePhase)
        		- (long)gExcursionInfo.IuValue * ((long)qsin(m_VoltagePhase - 21845)))>>15);
	m_lIt = abs((long)gExcursionInfo.IvValue * (long)qsin(16384+m_VoltagePhase)
				- (long)gExcursionInfo.IuValue * (long)qsin(m_VoltagePhase - 5461)>>15);
	m_lIs = ( 827L * (long)qsqrt(m_lIm * m_lIm + m_lIt * m_lIt))>>10;
    gLmIoIdentify.lIsValue = Filter16(m_lIs,gLmIoIdentify.lIsValue);
    m_lIs = abs(((long)gLmIoIdentify.lIsValue * (long)qsin(16384 - gIAmpTheta.Theta))>>15);
    gLmIoIdentify.lIsAccValue += gLmIoIdentify.lIsValue;
	gLmIoIdentify.lImAccValue += m_lIs;

    gLmIoIdentify.DataSavedNum++;
    if( 200 <= gLmIoIdentify.DataSavedNum )
    {
        LmIoCalValue();
        gLmIoIdentify.lIsAccValue = 0;
        gLmIoIdentify.lImAccValue = 0;
        gLmIoIdentify.DataSavedNum = 0;
    }
}

void ChgParForEst(void)
{
    if(MOTOR_TYPE_PM != gMotorInfo.MotorType)
    {

        gCtrMotorType = ASYNC_VF;
       if (500 < gInvInfo.InvTypeSet)
        {
         gBasePar.FcSet = 10;
        }
       else if (400 < gInvInfo.InvTypeSet)
        {
         gBasePar.FcSet = 20;
        }
       else
        {
         gBasePar.FcSet = 30;
        }
         gVFPar.VFWsComp = 0;
         gVFPar.VFLineType = 0;
         gVFPar.VFTorqueUp = 10;
         gPWM.SoftPWMTune = 0;
    }
    else
    {
       if (500 < gInvInfo.InvTypeSet)
       {
        gBasePar.FcSet =  10;
       }
       else if (400 < gInvInfo.InvTypeSet)
       {
        gBasePar.FcSet = 20;
       }
       else
       {
        gBasePar.FcSet = (gBasePar.FcSet < 30) ? 30 : gBasePar.FcSet;
       }
    }
}
