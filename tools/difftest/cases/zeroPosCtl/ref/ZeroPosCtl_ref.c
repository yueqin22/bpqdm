
#include "MotorInclude.h"
#include "ZeroPosCtl.h"

ZERO_POS_STRUCT         gZeroPos;

u32 GetPGPos(void)
{
    return ((*EQepRegs).QPOSCNT);
}

void ZeroPosJudge(void)
{
    if(gMainCmd.FreqSet != 0)
    {
        gZeroPos.Flag = 0;
    }
    else
    {
        gZeroPos.Flag = 1;
        gZeroPos.PosInit = GetPGPos();
    }
}

void ZeroPosControl(void)
{
    u16 m_QepCnt;
    s32 m_ITKpPos;
    u32 m_CpuTime;

    if(gZeroPos.Flag == 0)        return;

    gZeroPos.Flag++;
    gZeroPos.Flag = Min(gZeroPos.Flag,10);

    m_QepCnt = GetPGPos();
    gZeroPos.Pos = m_QepCnt - gZeroPos.PosInit;
    gZeroPos.PosInit = m_QepCnt;

    m_CpuTime = GetTime();
    gZeroPos.DetaTime = gZeroPos.TimeLast - m_CpuTime;
    gZeroPos.TimeLast = m_CpuTime;

    m_ITKpPos = ((s32)gZeroPos.Pos * (s32)gZeroPos.KPPos)<<4;
    gZeroPos.ItKpPos = Min(m_ITKpPos,8192);

}
