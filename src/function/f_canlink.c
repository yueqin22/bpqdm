
#include "f_funcCode.h"
#include "f_dspcan.h"
#include "f_canlink.h"
#include "f_comm.h"
#include "f_plc.h"
#include "f_p2p.h"

#define DEBUG_F_CANLINK              1

#define CANLINK_TRAN_TIMEOUT         1000
#define CANLINK_REC_TIMEOUT_MAX      4
#define CANLINK_PLC_PWRON_SEC        10
#define CANLINK_COUNTER_MAX          65535
#define CANLINK_INIT_DONE            0xcc
#define CANLINK_CODE_ERR             0xff
#define CANLINK_RSP_ERR_HI           0x8001
#define CANLINK_RSP_PWD_OK           0x8888
#define CANLINK_REMOTE_LEN           8
#define CANLINK_CONTROL_LEN          4
#define CANLINK_MBOX_OVR_BIT         0x40

#define CANLINK_TRAN_ID_TEMPLATE     (0xff | (0xfful<<16) | (1ul<<24) | (1ul<<29) | (0ul<<30) | (1ul<<31))
#define CANLINK_REC_ID_TEMPLATE      ((1ul<<30) | (1ul<<31))
#define CANLINK_REC_ACCEPT_MASK      ((~(0xfful<<8)) & ( ~(7ul<<29)))
#define CANLINK_IDENTITY_LO          0x00000000
#define CANLINK_IDENTITY_HI          0x00002774

#if DEBUG_F_CANLINK

Uint16 CanRxTxCon = 0;

struct CANLINK_REC_BUF CanlinkRecBuf;

#if (0 == CAN_REMOTE_EN)
void CanLinkRemoteDeal(struct CANLINK_DATA_BUF *frameBuf);
#endif
void CanLlinkDataDeal(struct CANLINK_DATA_BUF *frameBuf);
void CanLinkConDeal(struct CANLINK_DATA_BUF *frameBuf);

void CanHostTx(Uint16 addr, Uint16 data);

Uint32 CanLinkChara[2] = {
    CANLINK_IDENTITY_LO,
    CANLINK_IDENTITY_HI,
};

static Uint16 s_canBaudIndex = 0, s_canStationId = 0;

void InitCanlinkTran(Uint16 addr, Uint32 *frameBuf)
{
    union CANLINK_MsgID canId;
    Uint16 mboxIndex;

    addr &= CANLINK_ID_MASK;

    canId.all = CANLINK_TRAN_ID_TEMPLATE;

    canId.bit.destSta = addr;

    canId.bit.framf = CAN_REMOTE_FRAME;

#if CAN_REMOTE_EN
	InitTranMbox(AAM_MBOX_N, msgId.all, dataPi);
#endif
    canId.bit.aam = 0;

    for (mboxIndex=0; mboxIndex<TRAN_MBOX_NUM; mboxIndex++)
    {
        InitTranMbox(TRAN_BOX_N-mboxIndex, canId.all, frameBuf);
    }
}

Uint16 CanlinkDataTran(Uint32 *frameBuf, Uint16 frameLen, Uint16 timeOut)
{
    Uint16 mboxStatus, mboxIndex;
    Uint32 msgWord;
    static Uint16 s_tranRetryCount = 0;

    msgWord = *frameBuf++;
    for (mboxIndex=0; mboxIndex<TRAN_MBOX_NUM; mboxIndex++)
    {
        mboxStatus = eCanDataTran(TRAN_BOX_N-mboxIndex, frameLen, msgWord, frameBuf);
        if (CAN_MBOX_TRAN_SUCC == mboxStatus)
        {
            s_tranRetryCount = 0;
            return (CANLINK_RT_SUCC);
        }
    }
    if (++s_tranRetryCount < timeOut)
        return (CANLINK_RT_BUSY);

    s_tranRetryCount = 0;
    return (CANLINK_RT_TIMEOUT);
}

void InitCanlinkRec(Uint16 addr)
{
    union CANLINK_MsgID canId;
    Uint32 acceptMask;
    Uint16 mboxIndex;

    addr &= CANLINK_ID_MASK;

    canId.all = CANLINK_REC_ID_TEMPLATE;

    canId.bit.destSta = addr;

    acceptMask = CANLINK_REC_ACCEPT_MASK;

    for (mboxIndex=0; mboxIndex<REC_MBOX_NUM-1; mboxIndex++)
    {
        InitRecMbox((REC_BOX_N - mboxIndex) | CANLINK_MBOX_OVR_BIT, canId.all, acceptMask);
    }
    InitRecMbox((REC_BOX_N - mboxIndex), canId.all, acceptMask);
}

Uint16 CanlinkDataRec(struct CANLINK_REC_BUF *frameBuf)
{
    Uint16 mboxIndex, mboxStatus;

    frameBuf->bufFull = 0;
    for (mboxIndex=0; mboxIndex<REC_MBOX_NUM; mboxIndex++)
    {
        mboxStatus = eCanDataRec(REC_BOX_N-mboxIndex, (Uint32 *)(&(frameBuf->buf[mboxIndex])) );
        if (CAN_MBOX_EMPTY != mboxStatus)
            frameBuf->bufFull |= 1<<mboxIndex;
    }
    if ( 0 == frameBuf->bufFull)
        return (CANLINK_R_EMPTY);

    if (CAN_MBOX_REC_OVER == mboxStatus)
        return (CANLINK_R_OVER);

    return (CANLINK_RT_SUCC);
}

Uint16 InitCanlink(Uint16 addr, Uint16 baud, Uint32 *frameBuf)
{
    Uint16 initStatus;
    addr &= CANLINK_ID_MASK;

    initStatus = InitdspECan(baud);
    if (initStatus != CAN_INIT_SUCC)
        return (initStatus);

    InitCanlinkTran(addr, frameBuf);
    InitCanlinkRec(addr);
    return (CAN_INIT_SUCC);
}

#if (1 == CAN_LINK_TIME_DEBUG)
Uint32	CanTime1, CanTime2, CanTime;
#endif
void CanlinkFun(void)
{
    Uint16 recStatus;
    Uint16 wantBaud;
    Uint16 wantId;
    Uint16 mbox;
    static Uint16 s_recTimeoutCount = 0, s_canInitFlag = 0;

#if (1 == CAN_LINK_TIME_DEBUG)
	CanTime1 = ECANREGS.CANTSC;
#endif

#if DEBUG_F_P2P_CTRL

    if (p2pData.p2pEnable)
    {
        CanRxTxCon = p2pData.p2pEnable + funcCode.code.p2pTypeSel;
    }
    else
    {
        CanRxTxCon = 0;
    }

#endif

    if (funcCode.code.plcEnable)
    {
        wantBaud = CAN_BAUD_1M;
        wantId   = INV_PLC_ID;
    }
    else if (funcCode.code.p2pEnable)
    {
        wantBaud = CAN_LINK_BAUD_SEL;

        if (funcCode.code.p2pTypeSel)
            wantId = COMM_P2P_SLAVE_ADDRESS;
        else
            wantId = COMM_P2P_MASTER_ADDRESS;
    }
    else
    {
        wantBaud = CAN_LINK_BAUD_SEL;
        wantId   = CAN_LINK_S_ADDR;
    }

    if ( (s_canBaudIndex != wantBaud) || (s_canStationId != wantId) )
    {
        s_canBaudIndex = wantBaud;
        s_canStationId = wantId;
        s_canInitFlag = 0;
    }

    if (s_canInitFlag == 0)
    {
        funcCode.code.u3[10] = 0;
        funcCode.code.u3[11] = 0;

        if (CAN_INIT_SUCC != InitCanlink(s_canStationId, s_canBaudIndex, CanLinkChara) )
            return;

        s_canInitFlag = CANLINK_INIT_DONE;
        return;
    }

#if DEBUG_F_P2P_CTRL

    if (CanRxTxCon == P2P_COMM_HOST)
    {
        p2pData.p2pSendTcnt++;
        if (p2pData.p2pSendTcnt >= p2pData.p2pSendPeriod)
        {
            CanHostTx(COMM_P2P_COMM_ADDRESS_DATA, p2pData.P2PSendData);
            p2pData.p2pSendTcnt = 0;
        }
        return;
    }
#endif

    recStatus = CanlinkDataRec(&CanlinkRecBuf);

    if ((CANLINK_RT_SUCC == recStatus) || (CANLINK_R_OVER == recStatus) )
    {
        for (mbox = 0; mbox < REC_MBOX_NUM; mbox++)
        {
            if ( (CanlinkRecBuf.bufFull & (1u<<mbox) ) == (1<<mbox) )
            {
                CanLlinkDataDeal((struct CANLINK_DATA_BUF *)(&CanlinkRecBuf.buf[mbox]) );
            }
        }

        s_recTimeoutCount = 0;
        funcCode.code.u3[10] = 0;
    }
    else
    {
        if (++s_recTimeoutCount > CANLINK_REC_TIMEOUT_MAX)
        {
            if (funcCode.code.u3[10] < CANLINK_COUNTER_MAX)
                funcCode.code.u3[10]++;

            if ((funcCode.code.plcEnable)
                && ((curTime.powerOnTimeSec > CANLINK_PLC_PWRON_SEC)
                || (curTime.powerOnTimeM > 0))
                )
            {
                errorOther = ERROR_COMM;
                errorInfo = COMM_ERROR_PLC;
            }

            s_recTimeoutCount = 0;
        }
    }
#if (1 == CAN_LINK_TIME_DEBUG)
	CanTime2 = ECANREGS.CANTSC;
	CanTime = CanTime2 - CanTime1;
#endif
}

void CanLlinkDataDeal(struct CANLINK_DATA_BUF *frameBuf)
{
    switch (frameBuf->msgid.bit.framf)
    {
#if (0 == CAN_REMOTE_EN)
        case CAN_REMOTE_FRAME:
            CanLinkRemoteDeal(frameBuf);
            break;
#endif
        case CAN_CONTROL_FRAME:
            CanLinkConDeal(frameBuf);
            break;
        case CAN_CONFIG_FRAME:
            break;
        case CAN_DATA_FRAME:
            PlcDataFramDeal(frameBuf);
            break;
        default:
            if (funcCode.code.u3[11] < CANLINK_COUNTER_MAX)
                funcCode.code.u3[11]++;
            break;
    }
}

#if (0 == CAN_REMOTE_EN)
void CanLinkRemoteDeal(struct CANLINK_DATA_BUF *frameBuf)
{
    if (frameBuf->msgid.bit.aq == CANlink_ACK)
        return;

    frameBuf->msgid.bit.destSta = s_canStationId;
    frameBuf->msgid.bit.aq = CANlink_ACK;
    frameBuf->mdl.all = CanLinkChara[0];
    frameBuf->mdh.all = CanLinkChara[1];

    funcCode.code.u3[10] = 0;
    funcCode.code.u3[11] = 0;

    CanlinkDataTran((Uint32*)(frameBuf), CANLINK_REMOTE_LEN, CANLINK_TRAN_TIMEOUT);
}
#endif

Uint16 CanControlWriter(Uint16 addr, Uint16 data, Uint16 eeprom)
{
    Uint16 commStatus;

    if (eeprom)
        commRcvData.commCmdSaveEeprom = SCI_WRITE_WITH_EEPROM;
    sciFlag.all = 0;
    commStatus = CommWrite(addr, data);
    commRcvData.commCmdSaveEeprom = SCI_WRITE_NO_EEPROM;

    return commStatus;
}

Uint16 CanControlRead(Uint16 addr, Uint16* result)
{
    Uint16 commStatus;

    sciFlag.all = 0;
    commStatus = CommRead(addr, 1);
    *result = commReadData[0];
    return commStatus;
}

void CanLinkConDeal(struct CANLINK_DATA_BUF *frameBuf)
{
    Uint16 cmdResult, frameLen = CANLINK_CONTROL_LEN;

    if (frameBuf->msgid.bit.aq == CANlink_ACK)
        return;

    switch (frameBuf->msgid.bit.code)
    {
        case CAN_LINK_R_CFG:
            break;
        case CAN_LINK_R_REG:
            cmdResult = CanControlRead(frameBuf->mdl.data.datah, (Uint16*) (&frameBuf->mdl.data.datal) );
            break;
        case CAN_LINK_W_REG:
            cmdResult = CanControlWriter(frameBuf->mdl.data.datah, frameBuf->mdl.data.datal, 0);
            break;
        case CAN_LINK_R_INFO:
            break;
        case CAN_LINK_R_WAR:
            break;
        case CAN_LINK_W_EEP:
            cmdResult = CanControlWriter(frameBuf->mdl.data.datah, frameBuf->mdl.data.datal, 1);
            break;
        case CAN_TRAN_TAB_CFG:
            cmdResult = InvTranTabCfg(frameBuf);
            break;
        case CAN_REC_TAB_CFG:
            cmdResult = InvRecTabCfg(frameBuf);
            break;
        default:
            cmdResult = COMM_ERR_CMD;
            break;
    }

#if DEBUG_F_P2P_CTRL

    if (CanRxTxCon == P2P_COMM_SLAVE)
    {
        p2pData.p2pCommErrTcnt = 0;
        return;
    }
#endif

    frameBuf->msgid.bit.destSta = frameBuf->msgid.bit.srcSta;
    frameBuf->msgid.bit.srcSta = s_canStationId;
    frameBuf->msgid.bit.aq = CANlink_ACK;

    if (cmdResult)
    {
        frameBuf->msgid.bit.code = CANLINK_CODE_ERR;
        frameBuf->mdl.data.datah = CANLINK_RSP_ERR_HI;
        frameBuf->mdl.data.datal = cmdResult;

        if (funcCode.code.u3[11] < CANLINK_COUNTER_MAX)
            funcCode.code.u3[11]++;
    }
    else if (sciFlag.bit.pwdPass)
    {
        frameBuf->mdl.data.datal = CANLINK_RSP_PWD_OK;
    }

    CanlinkDataTran((Uint32*)(frameBuf), frameLen, CANLINK_TRAN_TIMEOUT);
}

void CanHostTx(Uint16 addr, Uint16 data)
{
    struct CANLINK_DATA_BUF hostFrame;
    Uint16 tranStatus;

    hostFrame.msgid.bit.destSta = COMM_P2P_SLAVE_ADDRESS;
    hostFrame.msgid.bit.srcSta = s_canStationId;
    hostFrame.msgid.bit.code = CAN_LINK_W_REG;
    hostFrame.msgid.bit.framf = CAN_CONTROL_FRAME;
    hostFrame.msgid.bit.aq = 1;
    hostFrame.mdl.data.datah = addr;
    hostFrame.mdl.data.datal = data;

    tranStatus = CanlinkDataTran((Uint32*)(&hostFrame), CANLINK_CONTROL_LEN, CANLINK_TRAN_TIMEOUT);

    if (tranStatus)
    {
        p2pData.p2pCommErrTcnt++;
    }
}
#elif 1

void CanlinkFun(void){}

#endif
