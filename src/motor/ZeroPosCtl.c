
#include "MotorInclude.h"
#include "ZeroPosCtl.h"

ZERO_POS_STRUCT         gZeroPos;

u32 GetPGPos(void)
{
    struct EQEP_REGS *pQep;

    pQep = EQepRegs;
    return pQep->QPOSCNT;
}

void ZeroPosJudge(void)
{
    if(gMainCmd.FreqSet == 0)
    {
        gZeroPos.PosInit = GetPGPos();
        gZeroPos.Flag = 1;
    }
    else
    {
        gZeroPos.Flag = 0;
    }
}

void ZeroPosControl(void)
{
    u16 m_QepCnt;
    u16 m_Flag;
    s16 m_Pos;
    s32 m_ITKpPos;
    s32 m_ItLimit;
    u32 m_CpuTime;

    if(gZeroPos.Flag == 0)        return;

    m_Flag = gZeroPos.Flag + 1;
    if(m_Flag > 10)               m_Flag = 10;
    gZeroPos.Flag = m_Flag;

    m_QepCnt = GetPGPos();
    m_CpuTime = GetTime();

    m_Pos = m_QepCnt - gZeroPos.PosInit;
    gZeroPos.PosInit = m_QepCnt;
    gZeroPos.Pos = m_Pos;

    gZeroPos.DetaTime = gZeroPos.TimeLast - m_CpuTime;
    gZeroPos.TimeLast = m_CpuTime;

    m_ITKpPos = ((s32)m_Pos * (s32)gZeroPos.KPPos)<<4;
    m_ItLimit = 8192;
    if(m_ITKpPos < m_ItLimit)     m_ItLimit = m_ITKpPos;
    gZeroPos.ItKpPos = m_ItLimit;
}
