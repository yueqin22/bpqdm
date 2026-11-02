#include "DSP2803x_EPwm_defines.h"
#include "MotorInvProtectInclude.h"

OVER_LOAD_PROTECT		gOverLoad;
PHASE_LOSE_STRUCT		gPhaseLose;
INPUT_LOSE_STRUCT		gInLose;
LOAD_LOSE_STRUCT		gLoadLose;
FAN_CTRL_STRUCT			gFanCtrl;
Ulong					gBuffResCnt;
CBC_PROTECT_STRUCT		gCBCProtect;
struct FAULT_CODE_INFOR_STRUCT_DEF  gError;

Uint const gInvOverLoadTable[9] =
{
		36000,
		18000,
        6000,
        1800,
        600,
        200,
        120,
        20,
        20,
};
Uint const gInvOverLoadTableForP[9] =
{
		36000,
		15000,
        6000,
        1800,
        600,
        300,
        100,
        30,
        10,
};

Ulong const gInvOverLoadDecTable[12] =
{
        (65536L*60/7),
        (65536L*60/8),
        (65536L*60/9),
        (65536L*60/10),
        (65536L*60/11),
        (65536L*60/13),
        (65536L*60/16),
        (65536L*60/19),
        (65536L*60/24),
        (65536L*60/34),
		(65536L*60/56),
};

#define C_MOTOR_OV_TAB_NUM      7
Uint const gMotorOverLoadTable[C_MOTOR_OV_TAB_NUM] =
{
		48000,
		24000,
		9000,
		3000,
		1200,
		1200,
		1200
};
#define C_MOTOR_OV_MAX_CUR      1750
#define C_MOTOR_OV_MIN_CUR      1150
#define C_MOTOR_OV_STEP_CUR     100

Ulong const gMotorOverLoadDecTable[12] =
{
        (65536L*60/30),
        (65536L*60/40),
        (65536L*60/50),
        (65536L*60/60),
        (65536L*60/70),
        (65536L*60/80),
        (65536L*60/90),
        (65536L*60/100),
        (65536L*60/110),
        (65536L*60/120),
		(65536L*60/130),
};

Uint const gTempTableP44X[23] =
{
		624,614,603,590,576,561,
		544,525,506,485,464,442,
		419,395,373,350,327,305,
		284,264,244,226,208
};
Uint const gTempTableP8XX[23] =
{
		475,451,426,400,374,348,
		323,299,275,253,232,212,
		193,176,161,146,133,121,
		110,100,91, 83, 76
};
Uint const gTempTableBSMXX[23] =
{
		486,461,435,412,386,361,
		337,313,291,269,248,228,
		209,193,176,161,148,135,
		123,113,103,94, 86
};
Uint const gTempTableSEMIKON[23] =
{
		558,519,480,451,418,392,
		369,350,331,314,302,288,
		278,269,262,254,247,243,
		237,233,229,226,224
};
Uint const gTempTableWAIZHI[23] =
{
		655,609,563,518,473,430,
		389,350,314,282,251,224,
		199,177,158,140,124,111,
		99, 88, 78, 70, 62
};

void    SoftWareErrorDeal(void);
void	TemperatureCheck(void);
void	OutputPhaseLoseDetect(void);
void 	OutputLoseReset(void);
void	InputPhaseLoseDetect(void);
void	ControlFan(void);
void	OverLoadProtect(void);
void	CBCLimitCurProtect(void);
void 	SetCBCEnable(void);
void 	SetCBCDisable(void);
void	LoadLoseDetect(void);
void 	BufferResProtect(void);

void InvDeviceControl(void)
{
	SoftWareErrorDeal();
	TemperatureCheck();
	OutputPhaseLoseDetect();
	InputPhaseLoseDetect();
	ControlFan();
	OverLoadProtect();

	CBCLimitCurPrepare();
	CBCLimitCurProtect();

	LoadLoseDetect();
	BufferResProtect();
}

void SoftWareErrorDeal(void)
{
	struct FAULT_CODE_INFOR_STRUCT_DEF * pErr = &gError;

	if((gMainStatus.RunStep == STATUS_LOW_POWER) ||
	   ((pErr->ErrorCode.all & ERROR_SHORT_EARTH) == ERROR_SHORT_EARTH))
	{
		gUDC.uDCBigFilter = gUDC.uDCFilter;
		return;
	}

	if((STATUS_STOP == gMainStatus.RunStep) &&
	   (pErr->LastErrorCode != pErr->ErrorCode.all) &&
	   (pErr->ErrorCode.all != 0))
	{
		pErr->LastErrorCode = pErr->ErrorCode.all;
		gPhase.IMPhase += 0x40000000L;
	}

	if(gSubCommand.bit.ErrorOK == 1)
	{
		pErr->ErrorCode.all = 0;
		pErr->ErrorInfo[0].all = 0;
		pErr->ErrorInfo[1].all = 0;
		pErr->ErrorInfo[2].all = 0;
		pErr->ErrorInfo[3].all = 0;
		pErr->ErrorInfo[4].all = 0;
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

	if(gLineCur.CurBaseInv > 10240)
	{
		DINT;
		DisableDrive();
		pErr->ErrorCode.all |= ERROR_OVER_CURRENT;
		pErr->ErrorInfo[0].bit.Fault1 = 2;
		gLineCur.ErrorShow = gLineCur.CurPer;
		EINT;
	}

	if((gUDC.uDCBigFilter > gInvInfo.InvUpUDC) ||
	   GetOverUdcFlag())
	{
		DisableDrive();
		pErr->ErrorCode.all |= ERROR_OVER_UDC;
		pErr->ErrorInfo[0].bit.Fault2 = 2;
		return;
	}

	if(gUDC.uDCBigFilter < gInvInfo.InvLowUDC)
	{
		DisableDrive();
		gMainStatus.RunStep = STATUS_LOW_POWER;
		gMainStatus.SubStep = 0;
		DisConnectRelay();
		pErr->ErrorCode.all |= ERROR_LOW_UDC;
		gMainStatus.StatusWord.bit.LowUDC = 0;
	}
}

void TemperatureCheck(void)
{
	TEMPLETURE_STRUCT * pT = &gTemperature;
	Uint    * m_pTable;
	Uint    m_TempAD,m_IndexLow,m_IndexHigh,m_Index,m_ErrTemp;
	Uint    m_LimtCnt;
	Uint    mType;

	mType = (INV_VOLTAGE_690V == gInvInfo.InvVoltageType)
	        ? (gInvInfo.InvTypeApply + 27)
	        : gInvInfo.InvTypeApply;

	m_ErrTemp = (mType >= 19) ? 85 : 95;
	if(INV_VOLTAGE_1140V == gInvInfo.InvVoltageType)
	{
		m_ErrTemp = 70;
		gInvInfo.TempType = 1;
	}

	if(mType <= 10)
	{
		if(gInvInfo.TempType == 1)
		{
			m_pTable = (Uint *)gTempTableP44X;
		}
		else if(gInvInfo.TempType == 2)
		{
			m_pTable = (Uint *)gTempTableP8XX;
		}
		else if(gInvInfo.TempType == 3)
		{
			m_pTable = (Uint *)gTempTableSEMIKON;
		}
		else
		{
			m_pTable = (Uint *)gTempTableBSMXX;
		}
	}
	else if(mType <= 18)
	{
		if(gInvInfo.TempType == 1)
		{
			m_pTable = (Uint *)gTempTableBSMXX;
		}
		else if(gInvInfo.TempType == 2)
		{
			m_pTable = (Uint *)gTempTableP44X;
		}
		else
		{
			m_pTable = (Uint *)gTempTableSEMIKON;
		}
	}
	else if(mType <= 26)
	{
		m_pTable = (Uint *)gTempTableWAIZHI;
	}
	else
	{
		m_pTable = (Uint *)gTempTableBSMXX;
	}

	m_TempAD = (pT->TempAD>>6);

#ifdef  TMS320F2808
	if(mType >= 27)
	{
		m_TempAD = ((long)pT->TempAD * 465)>>15;
	}
#endif

	m_IndexLow  = 0;
	m_IndexHigh = 22;
	m_Index = 11;
	if(m_TempAD >= m_pTable[m_IndexLow])
	{
		mType = 12 * 16;
	}
	else if(m_TempAD <= m_pTable[m_IndexHigh])
	{
		mType = 100 * 16;
	}
	else
	{
		for(m_LimtCnt = 0; m_LimtCnt < 7; m_LimtCnt++)
		{
			if(m_TempAD == m_pTable[m_Index])
			{
				mType = (m_Index<<6) + (12 * 16);
				break;
			}
			if(m_IndexLow+1 == m_IndexHigh)
			{
				mType = (m_IndexLow<<6) + (12 * 16)
							+ ((m_pTable[m_IndexLow] - m_TempAD)<<6)
				                /(m_pTable[m_IndexLow] - m_pTable[m_IndexHigh]);
				break;
			}

			if(m_TempAD > m_pTable[m_Index])
			{
				m_IndexHigh = m_Index;
			}
			else
			{
				m_IndexLow = m_Index;
			}
			m_Index = (m_IndexLow + m_IndexHigh)>>1;
		}
	}
	if(mType - pT->TempBak >= 8)
	{
		pT->TempBak = mType;
		pT->Temp = mType>>4;
	}

	pT->ErrCnt++;
	if(pT->Temp < m_ErrTemp)
	{
		pT->ErrCnt = 0;
	}

	if(pT->ErrCnt >= 5)
	{
		pT->ErrCnt = 0;
		gError.ErrorCode.all |= ERROR_INV_TEMPERTURE;
	}
}

void OutputPhaseLoseDetect(void)
{
	PHASE_LOSE_STRUCT * pPL = &gPhaseLose;
	Uint m_U,m_V,m_W;
	Uint m_Max,m_Min;

	if((gSubCommand.bit.OutputLost == 0) ||
	   (gMainCmd.FreqReal < 80) ||
	   (gMainCmd.Command.bit.StartDC == 1) ||
	   (gMainCmd.Command.bit.StopDC  == 1) ||
	   ((gMainStatus.RunStep != STATUS_RUN) &&
	    (gMainStatus.RunStep != STATUS_SPEED_CHECK)))
	{
		OutputLoseReset();
		return;
	}

	if((pPL->Time < (Ulong)DSP_CLOCK * 1000000L) && (pPL->Cnt < 50000))
	{
		return;
	}

	m_U = pPL->TotalU/pPL->Cnt;
	m_V = pPL->TotalV/pPL->Cnt;
	m_W = pPL->TotalW/pPL->Cnt;

	m_Max = m_U;
	m_Min = m_U;
	if(m_V > m_Max)
	{
		m_Max = m_V;
	}
	if(m_V < m_Min)
	{
		m_Min = m_V;
	}
	if(m_W > m_Max)
	{
		m_Max = m_W;
	}
	if(m_W < m_Min)
	{
		m_Min = m_W;
	}

	if((m_Max > 500) && (m_Max/m_Min > 10))
	{
		gError.ErrorCode.all |= ERROR_OUTPUT_LACK_PHASE;
		pPL->errMaxCur = m_Max;
		pPL->errMinCur = m_Min;
	}
	OutputLoseReset();
}

void OutputLoseAdd(void)
{
	PHASE_LOSE_STRUCT * pPL = &gPhaseLose;

	pPL->TotalU += abs(gIUVWQ24.U >> 12);
	pPL->TotalV += abs(gIUVWQ24.V >> 12);
	pPL->TotalW += abs(gIUVWQ24.W >> 12);

	pPL->Time   += gPWM.gPWMPrdApply;

	pPL->Cnt++;
}
void OutputLoseReset(void)
{
	PHASE_LOSE_STRUCT * pPL = &gPhaseLose;

	pPL->Cnt = 0;
	pPL->TotalU = 0;
	pPL->TotalV = 0;
	pPL->TotalW = 0;
	pPL->Time   = 0;
}

void InputPhaseLoseDetect(void)
{
	INPUT_LOSE_STRUCT * pIL = &gInLose;

	if((gSubCommand.bit.InputLost == 0) ||
	   ((STATUS_RUN!=gMainStatus.RunStep)&&(STATUS_SPEED_CHECK!=gMainStatus.RunStep))||
	   (gInvInfo.InvTypeApply < pIL->ForeInvType))
	{
		pIL->Cnt = 0;
		pIL->UpCnt = 0;
		pIL->ErrCnt = 0;
		pIL->CntRes = 0;
		pIL->UpCntRes = 0;
		return;
	}

	if(PL_INPUT_HIGH)
	{
		pIL->UpCnt ++;
		pIL->UpCntRes ++;
	}

	if(pIL->UpCntRes != 0)
	{
		pIL->CntRes++;
		if(pIL->CntRes >= 250)
		{
			if(pIL->UpCntRes >= 249)
			{
				gError.ErrorCode.all |= ERROR_RESISTANCE_CONTACK;
			}
			pIL->CntRes = 0;
			pIL->UpCntRes = 0;
		}
	}

	pIL->Cnt++;
	if(pIL->Cnt < 500)
	{
		return;
	}

	if((pIL->UpCnt <= 5) || (pIL->UpCnt >= 485))
	{
		pIL->ErrCnt = 0;
	}
	else
	{
		pIL->ErrCnt++;
		if(pIL->ErrCnt >= 2)
		{
			gError.ErrorCode.all |= ERROR_INPUT_LACK_PHASE;
			pIL->ErrCnt = 0;
		}
	}
	pIL->Cnt = 0;
	pIL->UpCnt = 0;
}

void ControlFan(void)
{
	FAN_CTRL_STRUCT * pFan = &gFanCtrl;

	if(gMainStatus.RunStep == STATUS_LOW_POWER)
	{
		TurnOffFan();
		pFan->EnableCnt = 0;
		return;
	}

	pFan->EnableCnt++;
	if(pFan->EnableCnt < 500)
	{
		TurnOffFan();
		return;
	}
	pFan->EnableCnt = 500;

	if((gMainCmd.Command.bit.Start == 1) ||
	   (gTemperature.Temp > 42) ||
	   (gSubCommand.bit.FanNoStop == 1))
	{
		pFan->RunCnt = 0;
		TurnOnFan();
		return;
	}

	if(gTemperature.Temp < 40)
	{
		pFan->RunCnt++;
		if(pFan->RunCnt >= 5000)
		{
			pFan->RunCnt = 5000;
			TurnOffFan();
		}
	}
}

void OverLoadProtect(void)
{
	OVER_LOAD_PROTECT * pOv = &gOverLoad;
	Ulong m_LDeta = 0;
	Uint m_Cur,m_Data,m_Index,m_CurBaseInv;
	Uint m_TorCurBottom,m_TorCurUpper,m_TorCurStep,m_TorCurData;
	Uint *m_TorCurLine;

	m_CurBaseInv = gLineCur.CurBaseInv;
	if( 28 <= gInvInfo.InvTypeApply )
	{
		m_CurBaseInv = (long)gLineCur.CurBaseInv * (long)gInvInfo.InvCurrent / gInvInfo.InvCurrForP;
	}

	pOv->FilterInvCur = Filter16(m_CurBaseInv,pOv->FilterInvCur);
	pOv->FilterMotorCur = Filter16(gLineCur.CurPer,pOv->FilterMotorCur);
	pOv->FilterRealFreq = Filter16(gMainCmd.FreqReal,pOv->FilterRealFreq);

	if(gMainStatus.RunStep == STATUS_LOW_POWER)
	{
		pOv->InvTotal.all = 0;
		pOv->MotorTotal.all = 0;
		pOv->Cnt = 0;
		gMainStatus.StatusWord.bit.PerOvLoadInv = 0;
		gMainStatus.StatusWord.bit.PerOvLoadMotor = 0;
		return;
	}

	pOv->Cnt++;
	if(pOv->Cnt < 5)
	{
		return;
	}
	pOv->Cnt = 0;

	if(1 == gInvInfo.GPType)
	{
		m_TorCurLine    = (Uint *)gInvOverLoadTable;
		m_TorCurBottom  = 1150;
		m_TorCurUpper   = 1870;
		m_TorCurStep    = 90;
		m_TorCurData    = 20;
	}
	else
	{
		m_TorCurLine    = (Uint *)gInvOverLoadTableForP;
		m_TorCurBottom  = 1050;
		m_TorCurUpper   = 1370;
		m_TorCurStep    = 40;
		m_TorCurData    = 10;
	}

	m_Cur = ((Ulong)pOv->FilterInvCur * 1000L) >> 12;
	if(m_Cur < m_TorCurBottom)
	{
		if(pOv->InvTotal.half.MSW < 10)
		{
			pOv->InvTotal.all = 0;
		}
		else if(gMainStatus.RunStep == STATUS_STOP)
		{
			pOv->InvTotal.all -= gInvOverLoadDecTable[0];
		}
		else if(m_Cur < 1000)
		{
			pOv->InvTotal.all -= gInvOverLoadDecTable[m_Cur/100 + 1];
		}
	}
	else
	{
		if(pOv->FilterRealFreq < 500)
		{
			m_Data = pOv->FilterRealFreq * 13 + 26214;
			m_Cur  = (((Ulong)m_Cur)<<15)/m_Data;
		}
		if(m_Cur >= m_TorCurUpper)
		{
			m_Data = m_TorCurData;
		}
		else
		{
			m_Index = (m_Cur - m_TorCurBottom)/m_TorCurStep;
			m_Data = *(m_TorCurLine + m_Index) -
			         (((long)(*(m_TorCurLine + m_Index)) - (*(m_TorCurLine + m_Index + 1))) *
					  (long)(m_Cur - m_TorCurBottom - m_Index * m_TorCurStep))/m_TorCurStep;
		}
		m_LDeta = ((Ulong)3600<<16)/(Uint)m_Data;
		pOv->InvTotal.all += m_LDeta;

		if(pOv->InvTotal.half.MSW >= 36000)
		{
			pOv->InvTotal.half.MSW = 36000;

			gMainStatus.StatusWord.bit.PerOvLoadInv = 0;
			gError.ErrorCode.all |= ERROR_INV_OVER_LAOD;
			gError.ErrorInfo[1].bit.Fault1 = 2;
		}
	}

	gMainStatus.StatusWord.bit.PerOvLoadInv =
	    (((pOv->InvTotal.all + m_LDeta * 1000UL)>>16) > 36000) ? 1 : 0;

	if(gSubCommand.bit.MotorOvLoad == 0)
	{
		pOv->MotorTotal.all = 0;
		gMainStatus.StatusWord.bit.PerOvLoadMotor = 0;
		return;
	}

	m_Cur = ((Ulong)pOv->FilterMotorCur * 1000L)>>12;
	m_Cur = ((Ulong)m_Cur * 100L)/gComPar.MotorOvLoad;

	m_LDeta = (Ulong)m_Cur * (Ulong)gMotorInfo.CurBaseCoff;
	if(m_LDeta >= (C_MOTOR_OV_MAX_CUR * 256L))
	{
		m_Cur = C_MOTOR_OV_MAX_CUR;
	}
	else
	{
		m_Cur = m_LDeta>>8;
	}

	if(m_Cur < C_MOTOR_OV_MIN_CUR)
	{
		if(pOv->MotorTotal.half.MSW < 10)
		{
			pOv->MotorTotal.all = 0;
		}
		else if(gMainStatus.RunStep == STATUS_STOP)
		{
			pOv->MotorTotal.all -= gMotorOverLoadDecTable[0];
		}
		else if(m_Cur < 1000)
		{
			pOv->MotorTotal.all -= gMotorOverLoadDecTable[m_Cur/100 + 1];
		}
	}
	else
	{
		if(m_Cur >= C_MOTOR_OV_MAX_CUR)
		{
			m_Data = gMotorOverLoadTable[C_MOTOR_OV_TAB_NUM - 1];
		}
		else
		{
			m_Index = (m_Cur - C_MOTOR_OV_MIN_CUR)/C_MOTOR_OV_STEP_CUR;
			m_Data = gMotorOverLoadTable[m_Index] -
			         ((long)(gMotorOverLoadTable[m_Index] - gMotorOverLoadTable[m_Index+1]) *
					  (long)(m_Cur - C_MOTOR_OV_MIN_CUR - m_Index * C_MOTOR_OV_STEP_CUR))/C_MOTOR_OV_STEP_CUR;
		}
		m_LDeta = ((Ulong)3600<<16)/(Uint)m_Data;
		pOv->MotorTotal.all += m_LDeta;

		if(pOv->MotorTotal.half.MSW > 36000)
		{
			pOv->MotorTotal.half.MSW = 36000;

			gMainStatus.StatusWord.bit.PerOvLoadMotor = 0;
			gError.ErrorCode.all |= ERROR_MOTOR_OVER_LOAD;
		}
	}

	gMainStatus.StatusWord.bit.PerOvLoadMotor =
	    (pOv->MotorTotal.half.MSW > gComPar.PerMotorOvLoad * 360) ? 1 : 0;
}

void CBCLimitCurProtect(void)
{
	CBC_PROTECT_STRUCT * pCbc = &gCBCProtect;
	int     m_CurU,m_CurV,m_CurW,m_Coff;
	int	    m_Max,m_Add,m_Sub;
    Uint    m_Limit;

	if(gSubCommand.bit.CBCEnable == 1)
	{
		if(pCbc->EnableFlag == 0) SetCBCEnable();
	}
	else
	{
		if(pCbc->EnableFlag == 1)  SetCBCDisable();
	}

	m_Coff = (((long)gMotorInfo.Current)<<10) / gInvInfo.InvCurrent;
	m_CurU = (int)(((long)gIUVWQ12.U * (long)m_Coff)>>10);
	m_CurU = abs(m_CurU);
	m_CurV = (int)(((long)gIUVWQ12.V * (long)m_Coff)>>10);
	m_CurV = abs(m_CurV);
	m_CurW = (int)(((long)gIUVWQ12.W * (long)m_Coff)>>10);
	m_CurW = abs(m_CurW);

	if(m_CurU > 9267)
	{
		pCbc->CntU++;
	}
	else
	{
		pCbc->CntU = pCbc->CntU>>1;
	}
	if(pCbc->CntU > 3000)
	{
		pCbc->CntU = 3000;
	}

	if(m_CurV > 9267)
	{
		pCbc->CntV++;
	}
	else
	{
		pCbc->CntV = pCbc->CntV>>1;
	}
	if(pCbc->CntV > 3000)
	{
		pCbc->CntV = 3000;
	}

	if(m_CurW > 9267)
	{
		pCbc->CntW++;
	}
	else
	{
		pCbc->CntW = pCbc->CntW>>1;
	}
	if(pCbc->CntW > 3000)
	{
		pCbc->CntW = 3000;
	}

	if(gMainCmd.FreqReal > 20)
	{
		pCbc->CntU = 0;
		pCbc->CntV = 0;
		pCbc->CntW = 0;
	}

	if((pCbc->CntU > 2500) || (pCbc->CntV > 2500) || (pCbc->CntW > 2500))
	{
		gError.ErrorCode.all |= ERROR_INV_OVER_LAOD;
		gError.ErrorInfo[1].bit.Fault1 = 1;
	}

	if(pCbc->EnableFlag == 0)
	{
		pCbc->TotalU = 0;
		pCbc->TotalV = 0;
		pCbc->TotalW = 0;
		return;
	}

	m_Limit = 500;

	m_Add = 2;
	m_Sub = (gMainStatus.RunStep == STATUS_STOP) ? 2 : 1;

	pCbc->TotalU += (pCbc->Flag.bit.CBC_U == 1) ? m_Add : (-m_Sub);
	pCbc->TotalV += (pCbc->Flag.bit.CBC_V == 1) ? m_Add : (-m_Sub);
	pCbc->TotalW += (pCbc->Flag.bit.CBC_W == 1) ? m_Add : (-m_Sub);

	pCbc->Flag.all = 0;

	pCbc->TotalU = __IQsat(pCbc->TotalU, m_Limit, 0);
	pCbc->TotalV = __IQsat(pCbc->TotalV, m_Limit, 0);
	pCbc->TotalW = __IQsat(pCbc->TotalW, m_Limit, 0);

	m_Max = (pCbc->TotalU > pCbc->TotalV) ? pCbc->TotalU : pCbc->TotalV;
	m_Max = (m_Max > pCbc->TotalW) ? m_Max : pCbc->TotalW;
	if(m_Max >= m_Limit)
	{
		gError.ErrorCode.all |= ERROR_TRIP_ZONE;
	}
}

void SetCBCEnable(void)
{
	CBC_PROTECT_STRUCT * pCbc = &gCBCProtect;

	pCbc->EnableFlag = 1;
	EALLOW;

#ifdef TMS320F2808
	EPwm1Regs.TZSEL.bit.CBC2 = TZ_ENABLE;
	EPwm1Regs.TZSEL.bit.CBC3 = TZ_ENABLE;
	EPwm1Regs.TZSEL.bit.CBC4 = TZ_ENABLE;

	EPwm2Regs.TZSEL.bit.CBC2 = TZ_ENABLE;
	EPwm2Regs.TZSEL.bit.CBC3 = TZ_ENABLE;
	EPwm2Regs.TZSEL.bit.CBC4 = TZ_ENABLE;

	EPwm3Regs.TZSEL.bit.CBC2 = TZ_ENABLE;
	EPwm3Regs.TZSEL.bit.CBC3 = TZ_ENABLE;
	EPwm3Regs.TZSEL.bit.CBC4 = TZ_ENABLE;
#else
	EPwm1Regs.TZSEL.bit.CBC2 = TZ_ENABLE;
	EPwm2Regs.TZSEL.bit.CBC2 = TZ_ENABLE;
	EPwm3Regs.TZSEL.bit.CBC2 = TZ_ENABLE;
#endif

	EDIS;
}

void SetCBCDisable(void)
{
	CBC_PROTECT_STRUCT * pCbc = &gCBCProtect;

	pCbc->EnableFlag = 0;

	EALLOW;

#ifdef TMS320F2808
	EPwm1Regs.TZSEL.bit.CBC2 = TZ_DISABLE;
	EPwm1Regs.TZSEL.bit.CBC3 = TZ_DISABLE;
	EPwm1Regs.TZSEL.bit.CBC4 = TZ_DISABLE;

	EPwm2Regs.TZSEL.bit.CBC2 = TZ_DISABLE;
	EPwm2Regs.TZSEL.bit.CBC3 = TZ_DISABLE;
	EPwm2Regs.TZSEL.bit.CBC4 = TZ_DISABLE;

	EPwm3Regs.TZSEL.bit.CBC2 = TZ_DISABLE;
	EPwm3Regs.TZSEL.bit.CBC3 = TZ_DISABLE;
	EPwm3Regs.TZSEL.bit.CBC4 = TZ_DISABLE;
#else
	EPwm1Regs.TZSEL.bit.CBC2 = TZ_DISABLE;
	EPwm2Regs.TZSEL.bit.CBC2 = TZ_DISABLE;
	EPwm3Regs.TZSEL.bit.CBC2 = TZ_DISABLE;
#endif

	EDIS;
}

void LoadLoseDetect(void)
{
	LOAD_LOSE_STRUCT * pLL = &gLoadLose;
	Uint m_Limit,m_LimitFreq;
	int  m_Skip;

	m_Limit = (pLL->ChkLevel * (Ulong)gMotorInfo.CurrentGet) / gMotorInfo.Current << 2;
	m_Limit = (long)m_Limit * 1447L >> 10;
	if(m_Limit < 20)
	{
		m_Limit = 20;
	}
	m_LimitFreq = ((Ulong)gMotorInfo.FreqPer * 1638) >> 15;

	m_Skip = (gMotorInfo.MotorType == MOTOR_TYPE_PM)
	      || (gSubCommand.bit.LoadLose != 1)
	      || (abs(gMainCmd.FreqSyn) < m_LimitFreq)
	      || (gMainCmd.Command.bit.StartDC == 1)
	      || (gMainCmd.Command.bit.StopDC  == 1)
	      || (gMainStatus.RunStep != STATUS_RUN)
	      || (abs(gIUVWQ12.U) >= m_Limit)
	      || (abs(gIUVWQ12.V) >= m_Limit)
	      || (abs(gIUVWQ12.W) >= m_Limit)
	      ;

	if(m_Skip)
	{
		pLL->ErrCnt = 0;
		gMainStatus.StatusWord.bit.OutOff = 0;
		return;
	}

	pLL->ErrCnt++;
	if((pLL->ErrCnt<<1) > (pLL->ChkTime*100))
	{
		pLL->ErrCnt = 50;
		gError.ErrorCode.all |= ERROR_LOAD_LOST;
		gMainStatus.StatusWord.bit.OutOff = 1;
	}
}

void BufferResProtect(void)
{
	gError.ErrorCode.all |= (gBuffResCnt >= 150000) ? ERROR_RESISTER_HOT : 0;
	gBuffResCnt--;
	gBuffResCnt = __IQsat(gBuffResCnt,200000,0);
}
