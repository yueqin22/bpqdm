
#include "MotorVCInclude.h"
#include "MotorSvcInclude.h"
#include "MotorEncoder.h"

FLUX_STRUCT             gFluxR;
FLUX_STRUCT             gFluxS;
SVC_FLUX_CAL_STRUCT		gCalFlux;
SVC_SPEED_STRUCT        gSVCSpeed;

ALPHABETA_STRUCT		gABFluxS_LowFilter;
ALPHABETA_STRUCT		gABFluxS_comp;
MT_STRUCT_Q24           gIMTQ24_obs;
MT_STRUCT_Q24           gIMTQ12_obs;

void ResetSVC(void)
{
	gABVoltSet.Alph = 0;
	gABVoltSet.Beta = 0;

	gFluxR.Amp = 0;
	gFluxR.Theta = 0;
	gFluxS.Amp = 0;
	gFluxS.Theta = 0;

	gSVCSpeed.SvcRotorSpeed 	= 0;
    gMainCmd.FreqFeed      = 0;
	gSVCSpeed.SvcSynSpeed 	= 0;
    gMainCmd.FreqSynFilter = 0;

	gSVCSpeed.SvcWs 			= 0;
	gSVCSpeed.SvcSignal = 0;
	gMainCmd.FreqReal = 0;
	gSVCSpeed.SvcLastFluxPos = 0;
	gMainCmd.FirstCnt = 0;

    gABFluxS_LowFilter.Alph = 0;
    gABFluxS_LowFilter.Beta = 0;
	gABFluxS_comp.Alph = 0;
	gABFluxS_comp.Beta = 0;

	gFluxR.Theta = (Uint)((long)gPhase.IMPhase>>16);
}

void SVCCalFlux_380(void)
{
	long  m_Long;
	Ulong m_ULong;
	int   m_LrDivLm,m_LLou;
    Ulong m_Timer;
	int   m_DetaTimer;
    int     m_Angle,m_Sin,m_Cos;
    ALPHABETA_STRUCT		m_ABFluxS_cmd;
    ALPHABETA_STRUCT		m_ABFluxR_cmd;
    ALPHABETA_STRUCT		m_IAlphBeta_cmd;
    ALPHABETA_STRUCT		m_ABFluxS;
    ALPHABETA_STRUCT		m_ABFluxR;

	gCalFlux.SampleTime = 25736L / (long)gBasePar.FcSetApply;

   	if(0 == gVCPar.SvcMode)
	{
        gCalFlux.FilterTime = (gMainCmd.FreqReal * 839L >> 11) + 300;
        m_Timer = GetTime();
        m_DetaTimer = (((gSVCSpeed.Timer - m_Timer) & 0xFFFFFFul)>>3);
        gSVCSpeed.Timer = m_Timer;
	}
	else
	{

	    gCalFlux.FilterTime = gTestDataReceive.TestData3;
	    gCalFlux.FilterTime = __IQsat(gCalFlux.FilterTime,2000,100);
	}

	m_LrDivLm = ((Ulong)gMotorExtPer.L1 << 14)/gMotorExtPer.LM;

	m_LLou = ((Ulong)gMotorExtPer.L0 * 10000)/gBasePar.FullFreq01;

	m_Long = (long)gMotorExtPer.R1 * (long)gIAlphBetaQ12.Alph;

	m_Long = (long)gABFluxS_LowFilter.Alph * (long)gCalFlux.FilterTime + m_Long;

	m_Long = (((long) gABVoltSet.Alph<<16) - m_Long)>>16;

	m_Long = (long)m_Long * (long)gCalFlux.SampleTime;

	gABFluxS_LowFilter.Alph = (((long)gABFluxS_LowFilter.Alph<<11) + m_Long)>>11;

	m_Long = (long)gMotorExtPer.R1 * ((long)gIAlphBetaQ12.Beta);

	m_Long = (long)gABFluxS_LowFilter.Beta * (long)gCalFlux.FilterTime + m_Long;

	m_Long = (((long)gABVoltSet.Beta<<16) - m_Long)>>16;

	m_Long = (long)m_Long * (long)gCalFlux.SampleTime;

	gABFluxS_LowFilter.Beta = (((long)gABFluxS_LowFilter.Beta<<11) + m_Long)>>11;

   	if(0 == gVCPar.SvcMode)
    {
         m_ABFluxS.Alph = (long)gABFluxS_LowFilter.Alph;
         m_ABFluxS.Beta = (long)gABFluxS_LowFilter.Beta;
    }
    else
    {

    	    m_Angle = (int)(((long)gPhase.IMPhase>>16)+((long)gPhase.StepPhaseApply>>16));

    	m_Sin  = qsin(m_Angle);
    	m_Cos  = qsin(16384 - m_Angle);

    	m_ABFluxR_cmd.Alph = ((((long)m_Cos * ((long)gIMTSetApply.M>>12))>>11)
    	                    * ((long)gMotorExtPer.LM*5000L/(long)gBasePar.FullFreq01))>>11;

    	m_ABFluxR_cmd.Beta = ((((long)m_Sin * ((long)gIMTSetApply.M>>12))>>11)
    	                    * ((long)gMotorExtPer.LM*5000L/(long)gBasePar.FullFreq01))>>11;

        m_IAlphBeta_cmd.Alph = (((long)gIMTSetApply.M>>12) * (long)m_Cos
                              - ((long)gIMTSetApply.T>>12) * (long)m_Sin)>>15;

        m_IAlphBeta_cmd.Beta = (((long)gIMTSetApply.M>>12) * (long)m_Sin
                              + ((long)gIMTSetApply.T>>12) * (long)m_Cos)>>15;

    	m_Long = ((long)m_LLou * (long)m_IAlphBeta_cmd.Alph)>>12;

    	m_Long = (long)m_ABFluxR_cmd.Alph + m_Long;

    	m_Long = (long)m_Long * (long)gMotorExtPer.LM;

        m_ABFluxS_cmd.Alph = (long)m_Long / (long)gMotorExtPer.L1;

    	m_Long = ((long)m_LLou * (long)m_IAlphBeta_cmd.Beta)>>12;

    	m_Long = (long)m_ABFluxR_cmd.Beta + m_Long;

    	m_Long = (long)m_Long * (long)gMotorExtPer.LM;

        m_ABFluxS_cmd.Beta = (long)m_Long / (long)gMotorExtPer.L1;

    	m_Long = (long)m_ABFluxS_cmd.Alph - (long)gABFluxS_comp.Alph;

    	m_Long = ((long)m_Long * (long)gCalFlux.SampleTime)>>12;

        gABFluxS_comp.Alph += ((long)m_Long * (long)gCalFlux.FilterTime)>>15;

    	m_Long = (long)m_ABFluxS_cmd.Beta - (long)gABFluxS_comp.Beta;

    	m_Long = ((long)m_Long * (long)gCalFlux.SampleTime)>>12;

        gABFluxS_comp.Beta += ((long)m_Long * (long)gCalFlux.FilterTime)>>15;

         m_ABFluxS.Alph = (long)gABFluxS_LowFilter.Alph + ((long)gABFluxS_comp.Alph * (long)gTestDataReceive.TestData2)/100L;
         m_ABFluxS.Beta = (long)gABFluxS_LowFilter.Beta + ((long)gABFluxS_comp.Beta * (long)gTestDataReceive.TestData2)/100L;
    }

	m_Long = ((long)gIAlphBetaQ12.Alph * (long)m_LLou)<<2;

	m_Long = ((long)m_ABFluxS.Alph * (long)m_LrDivLm) - (long)m_Long;

	m_ABFluxR.Alph = m_Long>>14;

	m_Long = ((long)gIAlphBetaQ12.Beta * (long)m_LLou)<<2;

	m_Long = ((long)m_ABFluxS.Beta * (long)m_LrDivLm) - m_Long;

	m_ABFluxR.Beta = m_Long>>14;

	m_ULong = (((long)m_ABFluxR.Alph * (long)m_ABFluxR.Alph) +
	          ((long)m_ABFluxR.Beta * (long)m_ABFluxR.Beta));

	gFluxR.Amp = (Ulong)qsqrt(m_ULong);

    gFluxR.Theta = atan(m_ABFluxR.Alph,m_ABFluxR.Beta);

	m_ULong = (((long)gABFluxS_LowFilter.Alph * (long)gABFluxS_LowFilter.Alph) +
	          ((long)gABFluxS_LowFilter.Beta * (long)gABFluxS_LowFilter.Beta));

	gFluxS.Amp = (Ulong)qsqrt(m_ULong);

    gFluxS.Theta = atan(gABFluxS_LowFilter.Alph,gABFluxS_LowFilter.Beta);

    gSVCSpeed.DetaTimer += m_DetaTimer;

}

void SVCCalRotorSpeed(void)
{
	int     m_DetaTimer;
	int    m_Speed;
	int     m_LowVolt;
	long		m_Long;

	DINT;
    m_DetaTimer = gSVCSpeed.DetaTimer;
    gSVCSpeed.DetaPhase = gFluxR.Theta - gSVCSpeed.SvcLastFluxPos;
	gSVCSpeed.SvcLastFluxPos = gFluxR.Theta;
    gSVCSpeed.DetaTimer = 0;
	EINT;

	if(gMainCmd.FirstCnt < 10)
	{
		gMainCmd.FirstCnt +=1;
		gSVCSpeed.SvcRotorSpeed = 0;
		gSVCSpeed.SvcSynSpeed	= 0;
		return;
	}

	m_Speed = ((Ulong)abs(gSVCSpeed.DetaPhase) * 25000L) / gBasePar.FullFreq01;
    if(gSVCSpeed.DetaPhase < 0)
	{
		m_Speed = -m_Speed;
	}
	m_Speed = ((long)m_Speed * (DSP_CLOCK*250L))/m_DetaTimer;
	gSVCSpeed.SvcSynSpeed = Filter16((int)m_Speed, gSVCSpeed.SvcSynSpeed) ;

	m_LowVolt = 300;
	if(gInvInfo.InvTypeApply >= 22)
    {
        m_LowVolt = 80;
    }
	if(gRatio < m_LowVolt)
    {
        gSVCSpeed.SvcSynSpeed = gMainCmd.FreqSyn;
    }
    if(gMotorExtPer.I0 > (gIMTSetApply.M>>12))
	{
	    m_Long = ((long)gMotorExtPer.R2<<11)/gMotorExtPer.I0;
	}
    else
	{
	    m_Long = ((long)gMotorExtPer.R2<<11)/(gIMTSetApply.M>>12);
	}

	m_Long = ((long)m_Long<<11)/gMotorExtPer.L1;
	m_Long =  ((long)m_Long * (long)gIMTQ12.T)>>14;
	gSVCSpeed.SvcWs = ((long)m_Long * (long)gVCPar.VCWsCoff)/100;

	m_Speed = gSVCSpeed.SvcSynSpeed - gSVCSpeed.SvcWs;
        if(m_Speed==0)
        {
            gSVCSpeed.SvcRotorSpeed = m_Speed;

        }
        if(gSVCSpeed.SvcSignal==0)
        {
            if(gMainCmd.FreqSyn==0)
            {
                gSVCSpeed.SvcRotorSpeed = 0;

            }
            else
            {
                gSVCSpeed.SvcSignal=1;
            }
        }

       if(((long)gMainCmd.FreqSet * (long)m_Speed) <0)
        {
            m_Speed = 0;
        }

    	if(gVCPar.VCSpeedFilter <= 1)
    	{
    		gSVCSpeed.SvcRotorSpeed = m_Speed;
    	}
    	else
    	{
        	m_Speed =  (long)gSVCSpeed.SvcRotorSpeed * (gVCPar.VCSpeedFilter-1L) + 2L * m_Speed;
        	gSVCSpeed.SvcRotorSpeed = m_Speed / (gVCPar.VCSpeedFilter + 1L);
    	}
        gMainCmd.FreqFeed = gSVCSpeed.SvcRotorSpeed;
	}

void SVCCalRotorSpeed_380(void)
{
	Ulong    m_ULong;
	long    m_Long;
	int     m_IntData;

	long    m_Speed;

        m_IntData = (gSVCSpeed.SvcSynSpeed < gMainCmd.FreqSyn) ? 1 : -1;
        m_IntData = (gSVCSpeed.SvcSynSpeed == gMainCmd.FreqSyn) ? 0 : m_IntData;

        m_ULong = (((Ulong)gMotorInfo.Frequency<<15)/gBasePar.FullFreq01)>>1;

        m_ULong = ((Ulong)m_ULong<<7)/(abs(gMainCmd.FreqSyn));
	    if(m_ULong>1024)
    	{
	        m_ULong = 1024;
    	}
        gSVCSpeed.SvcSynSpeed=(((long)gSVCSpeed.SvcSynSpeed*(1024-m_ULong)+(long)gMainCmd.FreqSyn*m_ULong)>>10) + m_IntData;

   if(gMainCmd.FirstCnt < 10)
	{
		gMainCmd.FirstCnt +=1;
		gSVCSpeed.SvcRotorSpeed = 0;
		gSVCSpeed.SvcSynSpeed	= 0;
		return;
	}
	else if(gMainCmd.FirstCnt < 1000)
	{
		gMainCmd.FirstCnt +=1;
	}
	else
	{
		gMainCmd.FirstCnt = 1000;

	}

    m_Long = ((long)gMotorExtPer.R2<<11)/(long)((gIMTSet.M>>12));

    m_Long = ((long)m_Long<<11)/(long)gMotorExtPer.L1;

    m_Long =  ((long)m_Long * (long)(gIMTQ24_obs.T>>12))>>14;

	gSVCSpeed.SvcWs = ((long)m_Long * (long)gVCPar.VCWsCoff)/100;

	gSVCSpeed.SvcRotorSpeed = gSVCSpeed.SvcSynSpeed - gSVCSpeed.SvcWs;

        if(gSVCSpeed.SvcSignal==0)
        {
            if(gMainCmd.FreqSyn==0)
            {
                gSVCSpeed.SvcRotorSpeed = 0;

            }
            else
            {
                gSVCSpeed.SvcSignal=1;
            }
        }

   	if(gVCPar.VCSpeedFilter <= 1)
	{
		gSVCSpeed.SvcRotorSpeed = gSVCSpeed.SvcRotorSpeed;
	}
	else
	{
       	m_Speed =  (long)gSVCSpeed.SvcRotorSpeed * (gVCPar.VCSpeedFilter-1L) + 2L * m_Speed;
       	gSVCSpeed.SvcRotorSpeed = m_Speed / (gVCPar.VCSpeedFilter + 1L);
	}
    gMainCmd.FreqFeed = gSVCSpeed.SvcRotorSpeed;
 }
