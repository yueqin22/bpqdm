
#include "f_ui.h"
#include "f_menu.h"
#include "f_frqSrc.h"
#include "f_comm.h"
#include "f_runSrc.h"
#include "f_io.h"
#include "f_error.h"
#include "f_p2p.h"

#if F_DEBUG_RAM
#else
#endif

#if DEBUG_F_P2P_CTRL

P2P_DATA_STRUCT p2pData;

void P2PDataDeal(void)
{
    Uint16 m_SendSel;
    int16  m_Proc;
    int16  m_RevOffset;

    if (funcCode.code.p2pEnable == 0)
    {
        p2pData.p2pEnable = 0;
        p2pData.p2pSendTcnt = 0;
        p2pData.p2pCommErrTcnt = 0;
    }
    else
    {
        p2pData.p2pEnable = 1;
    }

    if (p2pData.p2pEnable == 0)     return;

    if (funcCode.code.p2pTypeSel)
    {
        p2pData.p2pCommErrTcnt++;

        m_RevOffset = (int16)funcCode.code.p2pRevOffset;
        p2pData.processValue = ((int32)(int16)p2pData.P2PRevData*funcCode.code.p2pRevGain / 100) + m_RevOffset;

        m_Proc = (int16)p2pData.processValue;
        if (m_Proc >= (int16)10000)
        {
            p2pData.processValue = (int16)10000;
        }
        else if (m_Proc <= (int16)-10000)
        {
            p2pData.processValue = (Uint16)((int16)-10000);
        }
    }
    else
    {
        m_SendSel = funcCode.code.p2pSendDataSel;

        if (m_SendSel == P2P_OUT_TORQUE)
        {
            p2pData.P2PSendData = (int32)itDisp * 10000 / 2000;
        }
        else if (m_SendSel == P2P_FRQ_SET)
        {
            p2pData.P2PSendData = frqRun * 10000 / maxFrq;
        }
        else if (m_SendSel == P2P_FRQ_AIM)
        {
            p2pData.P2PSendData = frqAimTmp * 10000 / maxFrq;
        }
        else if (m_SendSel == P2P_FRQ_FDB)
        {
            p2pData.P2PSendData = (int32)frqFdb * 10000 / maxFrq;
        }

        p2pData.p2pSendPeriod = (funcCode.code.p2pSendPeriod + 1) >> 1;
    }

    if ((funcCode.code.p2pOverTime)
        && (p2pData.p2pCommErrTcnt >= (funcCode.code.p2pOverTime*50)))
    {
        errorOther = ERROR_COMM;
        errorInfo = COMM_ERROR_MODBUS;
        p2pData.p2pCommErrTcnt = 0;
    }
}
#else
void P2PDataDeal(void);
#endif
