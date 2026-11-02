
#ifndef __F_P2P_H__
#define __F_P2P_H__

#define  COMM_P2P_COMM_ADDRESS_DATA     0x1001
#define  COMM_P2P_COMM_ADDRESS_COMMAND
#define  COMM_P2P_MASTER_ADDRESS      1
#define  COMM_P2P_SLAVE_ADDRESS       2

#define P2P_OUT_TORQUE      0
#define P2P_FRQ_SET         1
#define P2P_FRQ_AIM         2
#define P2P_FRQ_FDB         3

#define P2P_REV_TORQUE_SET  0
#define P2P_REV_FRQ_SET     1

#if DEBUG_F_P2P_CTRL
typedef struct
{
    Uint16 p2pEnable;
    Uint16 P2PSendData;
    Uint16 P2PRevData;
    Uint16 processValue;
    Uint16 p2pSendPeriod;
    Uint16 p2pSendTcnt;
    Uint16 p2pCommErrTcnt;
} P2P_DATA_STRUCT;

extern P2P_DATA_STRUCT p2pData;

void P2PDataDeal(void);
#elif
#endif
#endif
