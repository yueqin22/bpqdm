
#include "MotorPwmInclude.h"

OUT_VOLT_STRUCT			gOutVolt;
PWM_OUT_STRUCT			gPWM;
Uint 					gRatio;

Uint 	EPWMFlagU;
Uint	EPWMFlagV;
Uint	EPWMFlagW;

Uint 	iTrig1_Cpwm[12] = {0,0,1,1,1,1,2,2,2,2,0,0};
Uint 	iTrig1_Dpwm[12] = {0,2,2,1,1,0,0,2,2,1,1,0};
Uint 	iTrig2_Pwm[12]  = {1,1,0,0,2,2,1,1,0,0,2,2};
Uint 	iTrig3_Cpwm[12] = {2,2,2,2,0,0,0,0,1,1,1,1};
Uint 	iTrig3_Dpwm[12] = {2,0,1,2,0,1,2,0,1,2,0,1};

void InitSetPWM(void)
{
	EALLOW;

	EPwm1Regs.TBPRD = C_INIT_PRD;
	EPwm1Regs.CMPB = EPwm1Regs.TBPRD - gADC.DelayApply;
	EPwm1Regs.TBPHS.all = 0;
	EPwm1Regs.TBCTL.all = 0xE012;

	EPwm1Regs.TBCTL.bit.HSPCLKDIV = PWM_CLK_DIV;

	EPwm1Regs.CMPCTL.all = 0x0100;

	EPwm1Regs.CMPA.half.CMPA = C_INIT_PRD/2;

	EPwm1Regs.AQCTLA.all = 0x0090;

	EPwm1Regs.AQSFRC.all  = 0x0;
	EPwm1Regs.AQCSFRC.all = 0x0;
	EPwm1Regs.DBCTL.all = 0x0007;

	EPwm1Regs.DBFED = C_MAX_DB;
	EPwm1Regs.DBRED = C_MAX_DB;

#ifdef TMS320F2808
	EPwm1Regs.TZCLR.all = 0x07;

	EPwm1Regs.TZSEL.all = 0x010E;

	EPwm1Regs.TZCTL.all = 0x0005;

	EPwm1Regs.TZEINT.all = 0x0004;

#else
    EPwm1Regs.TZCLR.all = 0x07;
    EPwm1Regs.TZSEL.all = 0x0102;

    EPwm1Regs.TZCTL.all = 0x0005;
    EPwm1Regs.TZEINT.all = 0x0004;
#endif

	EPwm1Regs.ETCLR.bit.INT = 1;
	EPwm1Regs.ETSEL.all = 0x0F09;

	EPwm1Regs.ETPS.all = 0x0101;

	EPwm2Regs.TBPRD = C_INIT_PRD;
	EPwm2Regs.TBPHS.all = 0;
	EPwm2Regs.TBCTL.all = 0xE006;

	EPwm2Regs.TBCTL.bit.HSPCLKDIV = PWM_CLK_DIV;

	EPwm2Regs.CMPCTL.all = 0x0100;

	EPwm2Regs.CMPA.half.CMPA = C_INIT_PRD/2;

	EPwm2Regs.AQCTLA.all = 0x0090;

	EPwm2Regs.AQSFRC.all  = 0x0;
	EPwm2Regs.AQCSFRC.all = 0x0;
	EPwm2Regs.DBCTL.all = 0x0007;

	EPwm2Regs.DBFED = C_MAX_DB;
	EPwm2Regs.DBRED = C_MAX_DB;

#ifdef TMS320F2808

	EPwm2Regs.TZSEL.all = 0x010E;

	EPwm2Regs.TZCTL.all = 0x0005;

#else
    EPwm2Regs.TZSEL.all = 0x0102;

    EPwm2Regs.TZCTL.all = 0x0005;
    EPwm2Regs.TZEINT.all = 0x0004;
#endif

    EPwm2Regs.ETSEL.all = 0;
    EPwm2Regs.ETPS.all  = 0;

	EPwm3Regs.TBPRD = C_INIT_PRD;
	EPwm3Regs.TBPHS.all = 0;
	EPwm3Regs.TBCTL.all = 0xE036;

	EPwm3Regs.TBCTL.bit.HSPCLKDIV = PWM_CLK_DIV;

	EPwm3Regs.CMPCTL.all = 0x0100;

	EPwm3Regs.CMPA.half.CMPA = C_INIT_PRD/2;

	EPwm3Regs.AQCTLA.all = 0x0090;

	EPwm3Regs.AQSFRC.all  = 0x0;
	EPwm3Regs.AQCSFRC.all = 0x00;
	EPwm3Regs.DBCTL.all = 0x0007;

	EPwm3Regs.DBFED = C_MAX_DB;
	EPwm3Regs.DBRED = C_MAX_DB;

#ifdef TMS320F2808

	EPwm3Regs.TZSEL.all = 0x010E;

	EPwm3Regs.TZCTL.all = 0x0005;

#else
    EPwm3Regs.TZSEL.all = 0x0102;

    EPwm3Regs.TZCTL.all = 0x0005;
    EPwm3Regs.TZEINT.all = 0x0004;
#endif

	EDIS;
}

void OutPutPWMVC(void)
{
	int   	SinU,SinV,SinW,Comp3;
	int   	phase,phase3;
	int     m_Ratio;

	phase = gPhase.OutPhase;
	phase3= 16384 - phase;
	phase3= phase3 + (phase3<<1);
	Comp3 = qsin(phase3)>>2;

	SinU  = qsin(16384 - phase) + Comp3;
	phase = phase - 21845;
	SinV  = qsin(16384 - phase) + Comp3;
	phase = phase - 21845;
	SinW  = qsin(16384 - phase) + Comp3;

	m_Ratio = ((long)gRatio * 4710)>>12;
	SinU = 4096 - (((long)m_Ratio * (long)SinU) >> 15);
	SinV = 4096 - (((long)m_Ratio * (long)SinV) >> 15);
	SinW = 4096 - (((long)m_Ratio * (long)SinW) >> 15);

	gPWM.U = (((long)SinU * (long)gPWM.gPWMPrdApply)>>13);
	gPWM.V = (((long)SinV * (long)gPWM.gPWMPrdApply)>>13);
	gPWM.W = (((long)SinW * (long)gPWM.gPWMPrdApply)>>13);

	DeadBandComp();
	SendPWM();
}

void OutPutPWMVF(void)
{
	Uint 	m_iSec30;
	Uint 	m_iSec60;
	Uint 	m_iRamainPhase;
	Uint 	m_FlagDPWMInverse;
	Ulong*	m_pUVW;

	Uint 	SinAlfa,SinAlfa60;
	Uint	m_iT1Length,m_iT2Length,m_iZeroLength,m_TotalLength;
	Uint 	m_UVWSel1,m_UVWSel2;
	Uint	phase;

	phase = gPhase.OutPhase;
	phase = (phase > 65531)?65531:phase;

	m_iSec30 = phase/5461;
	if (m_iSec30 > 11)
    {
        m_iSec30 = 0;
    }
	m_iSec60 = m_iSec30 >> 1;

	m_iRamainPhase = phase - (m_iSec30 * 5461);
	if((m_iSec30 & 0x01)  == 1)
	{
		if(gPWM.PWMModle == MODLE_CPWM)
		{
			m_iRamainPhase = 5461 + m_iRamainPhase;
		}
		else
		{
			m_iRamainPhase = 5461 - m_iRamainPhase;
		}
	}

	m_FlagDPWMInverse = 0;
	if(gPWM.PWMModle == MODLE_DPWM)
	{
		if((((m_iSec30+1)>>1) & 0x01) == 1) m_FlagDPWMInverse = 1;
	}
	else
	{
		if((m_iSec60 & 0x01) == 1)	m_iRamainPhase = 10922 - m_iRamainPhase;
	}

	SinAlfa60 = qsin(10922 - m_iRamainPhase);
	SinAlfa = qsin(m_iRamainPhase);

	m_iT1Length = ((long)SinAlfa60 * (long)gRatio)>>15;
	m_iT1Length = ((long)m_iT1Length * (long)gPWM.gPWMPrdApply)>>12;

	m_iT2Length = ((long)SinAlfa * (long)gRatio)>>15;
	m_iT2Length = ((long)m_iT2Length * (long)gPWM.gPWMPrdApply)>>12;

	m_TotalLength = m_iT1Length + m_iT2Length;
	if(m_TotalLength > gPWM.gPWMPrdApply)
	{
		m_iT1Length = ((Ulong)m_iT1Length * (Ulong)gPWM.gPWMPrdApply)/m_TotalLength;
		m_iT2Length = gPWM.gPWMPrdApply - m_iT1Length;
		m_iZeroLength = 0;
	}
	gPWM.gZeroLengthPhase = ZERO_VECTOR_NONE;
	if(gPWM.PWMModle == MODLE_CPWM)
	{
		m_iZeroLength = (Uint)((gPWM.gPWMPrdApply - m_iT1Length - m_iT2Length)>>1);
		m_UVWSel1 = iTrig1_Cpwm[m_iSec30];
		m_UVWSel2 = iTrig3_Cpwm[m_iSec30];
	}
	else
	{
		m_iZeroLength = 0;
		m_UVWSel1 = iTrig1_Dpwm[m_iSec30];
		gPWM.gZeroLengthPhase = (ZERO_LENGTH_PHASE_SELECT_ENUM)m_UVWSel1;
		m_UVWSel2 = iTrig3_Dpwm[m_iSec30];
	}

	m_iT1Length = m_iZeroLength + m_iT1Length;
	m_iT2Length = m_iT1Length + m_iT2Length;

	if(m_FlagDPWMInverse == 1)
	{
		m_iZeroLength = gPWM.gPWMPrdApply - m_iZeroLength;
		m_iT1Length   = gPWM.gPWMPrdApply - m_iT1Length;
		m_iT2Length   = gPWM.gPWMPrdApply - m_iT2Length;
	}

	m_pUVW = (Ulong *)&gPWM.U;
	*(m_pUVW + m_UVWSel1) = m_iZeroLength;
	*(m_pUVW + iTrig2_Pwm[m_iSec30]) = m_iT1Length;
	*(m_pUVW + m_UVWSel2) = m_iT2Length;

	DeadBandComp();
	SendPWM();
}

void DeadBandComp(void)
{
	long m_Max,m_Min,m_Deta;

    if(gRatio != 0)
    {
        if(ZERO_VECTOR_U != gPWM.gZeroLengthPhase )
        {
            gPWM.U += (long)gDeadBand.CompU;
        }
        if(ZERO_VECTOR_V != gPWM.gZeroLengthPhase )
        {
            gPWM.V += (long)gDeadBand.CompV;
        }
        if(ZERO_VECTOR_W != gPWM.gZeroLengthPhase )
        {
            gPWM.W += (long)gDeadBand.CompW;
        }
    }

    gPWM.U = __IQsat(gPWM.U, gPWM.gPWMPrdApply, 0);
    gPWM.V = __IQsat(gPWM.V, gPWM.gPWMPrdApply, 0);
    gPWM.W = __IQsat(gPWM.W, gPWM.gPWMPrdApply, 0);

#if 0
    if(gSubCommand.bit.CancelNarrorP == 0)
    {
        gPWM.U = __IQsat(gPWM.U, gPWM.gPWMPrdApply, 0);
        gPWM.V = __IQsat(gPWM.V, gPWM.gPWMPrdApply, 0);
        gPWM.W = __IQsat(gPWM.W, gPWM.gPWMPrdApply, 0);
    }
    else
    {
        m_Min = (long)gDeadBand.DeadBand + 1;
        m_Max = (long)(gPWM.gPWMPrdApply - m_Min);

        m_Deta = gPWM.U - m_Max;
        if(gPWM.U < (m_Min>>1))
        {
            gPWM.U = 0;
        }
        else if(gPWM.U < m_Min)
        {
            gPWM.U = m_Min;
        }
        else if(m_Deta < 0)
        {
            ;
        }
        else if(m_Deta < (m_Min>>1))
        {
            gPWM.U = m_Max;
        }
        else
        {
            gPWM.U = gPWM.gPWMPrdApply;
        }

        m_Deta = gPWM.V - m_Max;
        if(gPWM.V < (m_Min>>1))
        {
            gPWM.V = 0;
        }
        else if(gPWM.V < m_Min)
        {
            gPWM.V = m_Min;
        }
        else if(m_Deta < 0)
        {
            ;
        }
        else if(m_Deta < (m_Min>>1))
        {
            gPWM.V = m_Max;
        }
        else
        {
            gPWM.V = gPWM.gPWMPrdApply;
        }

        m_Deta = gPWM.W - m_Max;
        if(gPWM.W < (m_Min>>1))
        {
            gPWM.W = 0;
        }
        else if(gPWM.W < m_Min)
        {
            gPWM.W = m_Min;
        }
        else if(m_Deta < 0)
        {
            ;
        }
        else if(m_Deta < (m_Min>>1))
        {
            gPWM.W = m_Max;
        }
        else
        {
            gPWM.W = gPWM.gPWMPrdApply;
        }
    }
#endif

}

void SendPWM()
{

	EALLOW;
	if(EPWMFlagU != 0)
	{
		EPWMFlagU = 0;

		EPwm1Regs.AQCTLA.all = 0x90;

		EPwm1Regs.CMPCTL.all = 0;
	}
	if(EPWMFlagV != 0)
	{
		EPWMFlagV = 0;

		EPwm2Regs.AQCTLA.all = 0x90;

		EPwm2Regs.CMPCTL.all = 0;
	}
	if(EPWMFlagW != 0)
	{
		EPWMFlagW = 0;

		EPwm3Regs.AQCTLA.all = 0x90;

		EPwm3Regs.CMPCTL.all = 0;
	}

	if(gPWM.U == 0)
	{
		if(EPwm1Regs.CMPA.half.CMPA != 0)
		{
			EPwm1Regs.AQCTLA.all = 0x91;
			EPWMFlagU = 1;
		}
	}
	else
	{
		if(EPwm1Regs.CMPA.half.CMPA == 0)
		{
			EPwm1Regs.AQCTLA.all = 0x12;
			EPwm1Regs.CMPCTL.all = 2;
			EPWMFlagU = 1;
		}
	}

	if(gPWM.V == 0)
	{
		if(EPwm2Regs.CMPA.half.CMPA != 0)
		{
			EPwm2Regs.AQCTLA.all = 0x91;
			EPWMFlagV = 1;
		}
	}
	else
	{
		if(EPwm2Regs.CMPA.half.CMPA == 0)
		{
			EPwm2Regs.AQCTLA.all = 0x12;
			EPwm2Regs.CMPCTL.all = 2;
			EPWMFlagV = 1;
		}
	}

	if(gPWM.W == 0)
	{
		if(EPwm3Regs.CMPA.half.CMPA != 0)
		{
			EPwm3Regs.AQCTLA.all = 0x91;
			EPWMFlagW = 1;
		}
	}
	else
	{
		if(EPwm3Regs.CMPA.half.CMPA == 0)
		{
			EPwm3Regs.AQCTLA.all = 0x12;
			EPwm3Regs.CMPCTL.all = 2;
			EPWMFlagW = 1;
		}
	}

	EPwm1Regs.TBPRD = gPWM.gPWMPrdApply;
	EPwm1Regs.CMPB  = EPwm1Regs.TBPRD - gADC.DelayApply;
	EPwm2Regs.TBPRD = gPWM.gPWMPrdApply;
	EPwm3Regs.TBPRD = gPWM.gPWMPrdApply;

	EPwm1Regs.CMPA.half.CMPA = gPWM.U;
	EPwm2Regs.CMPA.half.CMPA = gPWM.V;
	EPwm3Regs.CMPA.half.CMPA = gPWM.W;
	EDIS;
}
