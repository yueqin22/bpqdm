
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

    if (funcCode.code.p2pEnable)
    {

        p2pData.p2pEnable = 1;
    }
    else
    {
        p2pData.p2pEnable = 0;
        p2pData.p2pSendTcnt = 0;
        p2pData.p2pCommErrTcnt = 0;
    }

    if (p2pData.p2pEnable)
    {

        if (!funcCode.code.p2pTypeSel)
        {

            switch(funcCode.code.p2pSendDataSel)
            {

                case P2P_OUT_TORQUE:
                    p2pData.P2PSendData = (int32)itDisp * 10000 / 2000;
                    break;

                case P2P_FRQ_SET:
                    p2pData.P2PSendData = frqRun * 10000 / maxFrq;
                    break;

                case P2P_FRQ_AIM:
                    p2pData.P2PSendData = frqAimTmp * 10000 / maxFrq;
                    break;

                case P2P_FRQ_FDB:
                    p2pData.P2PSendData = (int32)frqFdb * 10000 / maxFrq;
                    break;

                default:
                    break;
            }

            p2pData.p2pSendPeriod = (funcCode.code.p2pSendPeriod + 1) >> 1;

        }

        else
        {

            p2pData.p2pCommErrTcnt++;

            p2pData.processValue = ((int32)(int16)p2pData.P2PRevData*funcCode.code.p2pRevGain / 100) + ((int16)funcCode.code.p2pRevOffset);

            if (((int16)p2pData.processValue) >= ((int16)10000))
            {
                p2pData.processValue = ((int16)10000);
            }
            else if (((int16)p2pData.processValue) <= ((int16)-10000))
            {
                p2pData.processValue = (Uint16)((int16)-10000);
            }
        }

        if ((p2pData.p2pCommErrTcnt >= (funcCode.code.p2pOverTime*50))
            && (funcCode.code.p2pOverTime))
        {
            errorOther = ERROR_COMM;
            errorInfo = COMM_ERROR_MODBUS;
            p2pData.p2pCommErrTcnt = 0;
        }
    }
}
#else
void P2PDataDeal(void);
#endif
