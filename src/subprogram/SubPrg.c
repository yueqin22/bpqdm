#include "MotorDefine.h"
#include "SubPrgInclude.h"

#define  SETOVM asm(" SETC OVM")
#define  CLROVM asm(" CLRC OVM")

int atan(int x, int y)
{
	int  m_Angle;
	long m_Input;

	if(x == 0)
	{
		return (y < 0) ? (-16384) : 16384;
	}
	m_Input = (((long)y)<<16)/x;
	m_Angle = qatan(m_Input) + ((x < 0) ? 32768 : 0);
	return m_Angle;
}

void PID(PID_STRUCT * pid)
{
	long m_Max,m_Min,m_Out,m_OutKp,m_OutKi,m_KpSat,m_KiSat;
    long vMax = 0x7FFFFFFF;

    SETOVM;
	m_KpSat   = (vMax>> (4+ pid->QP));
	m_KiSat   = (vMax >> pid->QI);
	m_Max     = ((long)pid->Max)<<16;
	m_Min     = ((long)pid->Min)<<16;
	m_OutKp   = (long)pid->KP * (long)pid->Deta;
	m_OutKp   = __IQsat(m_OutKp, m_KpSat, -(m_KpSat));
    m_OutKp   = m_OutKp << (4+ pid->QP);
	m_OutKi   = (long)pid->KI * (long)pid->Deta;
    m_OutKi   = __IQsat(m_OutKi, m_KiSat, -(m_KiSat));
    m_OutKi   = m_OutKi << pid->QI;

    if((m_OutKp > m_Max) && (pid->Total > 0))
    {
        pid->Total -= (pid->Total>>8) + 1;
        m_OutKi = 0;
    }
    else if((m_OutKp < m_Min) && (pid->Total < 0))
    {
        pid->Total -= (pid->Total>>8) - 1;
        m_OutKi = 0;
    }
	pid->Total += m_OutKi;
	m_Out       = pid->Total + m_OutKp;
 	pid->Out    = __IQsat(m_Out,m_Max,m_Min);
    pid->Total  = __IQsat(pid->Total,m_Max,m_Min);

    CLROVM;
}

void PIDLongRegulate(PID_STRUCT_LONG * pid)
{
	llong  m_Max,m_Min,m_Out,m_OutKp,m_OutKi;

 	m_Max   = ((llong)pid->Max)<<16;
	m_Min   = ((llong)pid->Min)<<16;
	m_OutKp = ((llong)pid->KP * (llong)pid->Deta) << 4;
	m_OutKi = (llong)pid->KI * (llong)pid->Deta;

    if(((m_OutKp > m_Max) && (pid->Total > 0)) ||
       ((m_OutKp < m_Min) && (pid->Total < 0)))
    {
        pid->Total -= (pid->Total>>8);
        m_OutKi = 0;
    }

	pid->Total += m_OutKi;
	m_Out      = pid->Total + m_OutKp;
    pid->Out   = (m_Out < m_Min) ? m_Min
                : ((pid->Out > m_Max) ? m_Max : m_Out);

    pid->Total = (pid->Total < m_Min) ? m_Min
                : ((pid->Total > m_Max) ? m_Max : pid->Total);
}

void PID32(PID32_STRUCT * pid)
{
	long  m_OutKp,m_OutKi;
	long  mTotalMax,mTotalMin;

	m_OutKp = ((llong)pid->KP * (llong)pid->Deta) >> (16-4);
	m_OutKi = ((llong)pid->KI * (llong)pid->Deta)>>16;

	if(pid->Deta > 0)
	{
		mTotalMax = pid->Max - m_OutKp;
		mTotalMax = (mTotalMax < 0) ? 0 : mTotalMax;

		mTotalMin = pid->Min;
	}
	else
	{
		mTotalMin = pid->Min - m_OutKp;
		mTotalMin = (mTotalMin > 0) ? 0 : mTotalMin;

		mTotalMax = pid->Max;
	}

	pid->Total = pid->Total + m_OutKi;
	pid->Total = __IQsat(pid->Total, mTotalMax, mTotalMin);

	pid->Out = pid->Total + m_OutKp;
	pid->Out = __IQsat(pid->Out, pid->Max, pid->Min);
}

void BurrFilter(BURR_FILTER_STRUCT * filter)
{
	int 	m_Deta;

	m_Deta = abs((filter->Input) - (filter->Output));
	if(m_Deta > filter->Err)
	{
		filter->Err = filter->Err << 1;
	}
	else
	{
		filter->Output = filter->Input;
	  	if(m_Deta < (filter->Err >> 1))
		{
			filter->Err = filter->Err >> 1;
		}
	}
	filter->Err = (filter->Err > filter->Max) ? filter->Max : filter->Err;
	filter->Err = (filter->Err < filter->Min) ? filter->Min : filter->Err;
}

void SlipFilter(CUR_LINE_STRUCT_DEF * pCur)
{
	int  m_Index,m_IndexStart,m_Coff,m_TotalCoff;
	long m_Total;
	CUR_LINE_STRUCT_DEF * m_pCur = pCur;
	int * m_pData = m_pCur->Data;

	m_Total = 0;
	m_TotalCoff = 0;

	for(m_Index = 1;m_Index<16;m_Index++)
	{
		m_pData[m_Index-1] = m_pData[m_Index];
	}
	m_pData[15] = m_pCur->Input;

	m_IndexStart = 16 - m_pCur->FilterTime;
	m_Coff = 0;
	while(m_Coff < m_pCur->FilterTime)
	{
		m_Total += (long)m_pData[m_IndexStart + m_Coff] * (m_Coff +1);
		m_TotalCoff += (m_Coff+1);
		m_Coff ++;
	}
	m_pCur->Output = m_Total/m_TotalCoff;
}
