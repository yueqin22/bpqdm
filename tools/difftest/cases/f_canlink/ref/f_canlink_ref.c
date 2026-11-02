
#include "f_funcCode.h"
#include "f_dspcan.h"
#include "f_canlink.h"
#include "f_comm.h"
#include "f_plc.h"
#include "f_p2p.h"

#define DEBUG_F_CANLINK              1

#if DEBUG_F_CANLINK

Uint16 CanRxTxCon = 0;

struct CANLINK_REC_BUF	CanlinkRecBuf;

#if (0 == CAN_REMOTE_EN)
void CanLinkRemoteDeal(struct CANLINK_DATA_BUF *dataPi);
#endif
void CanLlinkDataDeal(struct CANLINK_DATA_BUF *dataPi);
void CanLinkConDeal(struct CANLINK_DATA_BUF *dataPi);

void CanHostTx(Uint16 addr, Uint16 data);

Uint32	CanLinkChara[2] = {
							0x00000000,
							0x00002774,
							};

static Uint16 sBaud = 0, sSourID = 0;

void InitCanlinkTran(Uint16	addr, Uint32 *dataPi)
{
	union CANLINK_MsgID msgId;
	Uint16	i;

	addr &= CANLINK_ID_MASK;

	msgId.all = 0xff | (0xfful<<16) | (1ul<<24) | (1ul<<29) | (0ul<<30) | (1ul<<31);

    msgId.bit.destSta = addr;

    msgId.bit.framf = CAN_REMOTE_FRAME;

#if CAN_REMOTE_EN
	InitTranMbox(AAM_MBOX_N, msgId.all, dataPi);
#endif
    msgId.bit.aam = 0;

	for (i=0; i<TRAN_MBOX_NUM; i++)
	{
		InitTranMbox(TRAN_BOX_N-i, msgId.all, dataPi);
	}
}

Uint16 CanlinkDataTran(Uint32 *dataPi, Uint16 len, Uint16 timeOut)
{
	Uint16 stat, i;
	Uint32 msgid;
	static Uint16 count = 0;

	msgid = *dataPi++;
	for (i=0; i<TRAN_MBOX_NUM; i++)
	{
		stat = eCanDataTran(TRAN_BOX_N-i, len, msgid, dataPi);
		if (CAN_MBOX_TRAN_SUCC == stat)
		{
			count = 0;
			return (CANLINK_RT_SUCC);
		}
	}
	if (++count >= timeOut)
	{
		count = 0;
		return (CANLINK_RT_TIMEOUT);
	}
	else
	{
		return (CANLINK_RT_BUSY);
	}
}

void InitCanlinkRec(Uint16 addr)
{
	union CANLINK_MsgID msgId;
	Uint32 lam;
	Uint16	i;

	addr &= CANLINK_ID_MASK;

	msgId.all =  (1ul<<30) | (1ul<<31);

    msgId.bit.destSta = addr;

	lam = (~(0xfful<<8)) & ( ~(7ul<<29));

	for (i=0; i<REC_MBOX_NUM-1; i++)
	{
		InitRecMbox((REC_BOX_N-i) | 0x40, msgId.all, lam);
	}
	InitRecMbox((REC_BOX_N-i), msgId.all, lam);

}

Uint16 CanlinkDataRec(struct CANLINK_REC_BUF *dataPi)
{
	Uint16 i, stat;

	dataPi->bufFull = 0;
	for (i=0; i<REC_MBOX_NUM; i++)
	{
		stat = eCanDataRec(REC_BOX_N-i, (Uint32 *)(&(dataPi->buf[i])) );
		if (CAN_MBOX_EMPTY != stat)
			dataPi->bufFull |= 1<<i;
	}
	if ( 0 == dataPi->bufFull)
		return (CANLINK_R_EMPTY);
	if (CAN_MBOX_REC_OVER == stat)
		return (CANLINK_R_OVER);
	else
		return (CANLINK_RT_SUCC);
}

Uint16 InitCanlink(Uint16 addr, Uint16 baud, Uint32 *dataPi)
{
	Uint16 stat;
	addr &= CANLINK_ID_MASK;

	stat = InitdspECan(baud);
	if (stat != CAN_INIT_SUCC)
		return (stat);

	InitCanlinkTran(addr, dataPi);
	InitCanlinkRec(addr);
	return (CAN_INIT_SUCC);
}

#if (1 == CAN_LINK_TIME_DEBUG)
Uint32	CanTime1, CanTime2, CanTime;
#endif
void CanlinkFun(void)
{
	Uint16 stat;
	static Uint16 timeOutErr = 0, initFlag = 0;

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
        if ( (sBaud != CAN_BAUD_1M) || (sSourID != INV_PLC_ID) )
		{
			sBaud = CAN_BAUD_1M;
			sSourID = INV_PLC_ID;
			initFlag = 0;
		}
    }

    else if (funcCode.code.p2pEnable)
    {
        Uint16 p2pID;
        Uint16 p2pBaud;

        p2pBaud = CAN_LINK_BAUD_SEL;

        if (funcCode.code.p2pTypeSel)        {
            p2pID = COMM_P2P_SLAVE_ADDRESS;
        }

        else
        {
            p2pID = COMM_P2P_MASTER_ADDRESS;
        }

        if ( (sBaud != p2pBaud) || (sSourID != p2pID) )
        {
            sBaud = p2pBaud;
            sSourID = p2pID;
            initFlag = 0;
        }
    }
    else if ( (sBaud != CAN_LINK_BAUD_SEL) || (sSourID != CAN_LINK_S_ADDR) )
    {
        sBaud = CAN_LINK_BAUD_SEL;
        sSourID = CAN_LINK_S_ADDR;
        initFlag = 0;
    }

	if (initFlag == 0)
	{
        funcCode.code.u3[10] = 0;
        funcCode.code.u3[11] = 0;
        if (CAN_INIT_SUCC != InitCanlink(sSourID, sBaud, CanLinkChara) )
			return ;
		else
			initFlag = 0xcc;

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

	stat = CanlinkDataRec(&CanlinkRecBuf);

	if ((CANLINK_RT_SUCC == stat) || (CANLINK_R_OVER == stat) )
	{
		for (stat=0; stat<REC_MBOX_NUM; stat++)
		{
			if ( (CanlinkRecBuf.bufFull & (1u<<stat) )== (1<<stat))
			{

				CanLlinkDataDeal((struct CANLINK_DATA_BUF *)(&CanlinkRecBuf.buf[stat]) );
			}
		}
        timeOutErr = 0;
        funcCode.code.u3[10] = 0;
	}
    else
    {
        if (++timeOutErr > 4)
        {
            if (funcCode.code.u3[10] < 65535)
                funcCode.code.u3[10]++;

            if ((funcCode.code.plcEnable)
                && ((curTime.powerOnTimeSec > 10)
                || (curTime.powerOnTimeM > 0))
                )
            {
                errorOther = ERROR_COMM;
                errorInfo = COMM_ERROR_PLC;
            }

            timeOutErr = 0;
        }
    }
#if (1 == CAN_LINK_TIME_DEBUG)
	CanTime2 = ECANREGS.CANTSC;
	CanTime = CanTime2 - CanTime1;
#endif
}

void CanLlinkDataDeal(struct CANLINK_DATA_BUF *dataPi)
{
	switch (dataPi->msgid.bit.framf)
	{
#if (0 == CAN_REMOTE_EN)
        case CAN_REMOTE_FRAME:
			CanLinkRemoteDeal(dataPi);
			break;
#endif
        case CAN_CONTROL_FRAME:
			CanLinkConDeal(dataPi);
			break;
		case CAN_CONFIG_FRAME:

			break;
		case CAN_DATA_FRAME:
			PlcDataFramDeal(dataPi);

			break;

		default:
            if (funcCode.code.u3[11] < 65535)
                funcCode.code.u3[11]++;
			break;

	}
}

#if (0 == CAN_REMOTE_EN)
void CanLinkRemoteDeal(struct CANLINK_DATA_BUF *dataPi)
{
	if (dataPi->msgid.bit.aq == CANlink_ACK)
        return;

    dataPi->msgid.bit.destSta = sSourID;

    dataPi->msgid.bit.aq = CANlink_ACK;

	dataPi->mdl.all= CanLinkChara[0];
	dataPi->mdh.all= CanLinkChara[1];

    funcCode.code.u3[10] = 0;
    funcCode.code.u3[11] = 0;

	CanlinkDataTran((Uint32*)(dataPi), 8, 1000);

}
#endif

Uint16 CanControlWriter(Uint16 addr, Uint16 data, Uint16 eeprom)
{
    Uint16 oscReturn;

    if (eeprom)
        commRcvData.commCmdSaveEeprom = SCI_WRITE_WITH_EEPROM;
    sciFlag.all = 0;
    oscReturn = CommWrite(addr, data);
    commRcvData.commCmdSaveEeprom = SCI_WRITE_NO_EEPROM;

    return oscReturn;
}

Uint16 CanControlRead(Uint16 addr, Uint16* result)
{
    Uint16 oscReturn;

    sciFlag.all = 0;
    oscReturn = CommRead(addr, 1);
    *result = commReadData[0];
    return oscReturn;
}

void CanLinkConDeal(struct CANLINK_DATA_BUF *dataPi)
{
	Uint16 err, len = 4;

	if (dataPi->msgid.bit.aq == CANlink_ACK)
        return;

	switch (dataPi->msgid.bit.code)
	{
		case CAN_LINK_R_CFG:

			break;
		case CAN_LINK_R_REG:
			err = CanControlRead(dataPi->mdl.data.datah, (Uint16*) (&dataPi->mdl.data.datal) );

			break;
		case CAN_LINK_W_REG:
			err = CanControlWriter(dataPi->mdl.data.datah, dataPi->mdl.data.datal, 0);

			break;
		case CAN_LINK_R_INFO:

			break;
		case CAN_LINK_R_WAR:

			break;
		case CAN_LINK_W_EEP:
			err = CanControlWriter(dataPi->mdl.data.datah, dataPi->mdl.data.datal, 1);

			break;

		case CAN_TRAN_TAB_CFG:
			err = InvTranTabCfg(dataPi);

			break;
		case CAN_REC_TAB_CFG:
			err = InvRecTabCfg(dataPi);

			break;

		default:
			err = COMM_ERR_CMD;
			break;

	}

#if DEBUG_F_P2P_CTRL

    if (CanRxTxCon == P2P_COMM_SLAVE)
	{

        p2pData.p2pCommErrTcnt = 0;
        return;
	}
#endif

	dataPi->msgid.bit.destSta = dataPi->msgid.bit.srcSta;
	dataPi->msgid.bit.srcSta = sSourID;
	dataPi->msgid.bit.aq = CANlink_ACK;

    if (err)
    {
        dataPi->msgid.bit.code = 0xff;
        dataPi->mdl.data.datah = 0x8001;
	    dataPi->mdl.data.datal = err;
    }
    else if ( (sciFlag.bit.pwdPass)

    )
    {
        dataPi->mdl.data.datal = 0x8888;
	}

    if (err)
    {
        if (funcCode.code.u3[11] < 65535)
            funcCode.code.u3[11]++;
    }

	CanlinkDataTran((Uint32*)(dataPi), len, 1000);
}

void CanHostTx(Uint16 addr, Uint16 data)
{
    struct CANLINK_DATA_BUF databuf;
    Uint16 sendDataStatus;

    databuf.msgid.bit.destSta = COMM_P2P_SLAVE_ADDRESS;
	databuf.msgid.bit.srcSta = sSourID;
	databuf.msgid.bit.code = CAN_LINK_W_REG;
	databuf.msgid.bit.framf = CAN_CONTROL_FRAME;
	databuf.msgid.bit.aq = 1;
    databuf.mdl.data.datah = addr;
    databuf.mdl.data.datal = data;

	sendDataStatus = CanlinkDataTran((Uint32*)(&databuf), 4, 1000);

    if (sendDataStatus)
    {

        p2pData.p2pCommErrTcnt++;
    }
}
#elif 1

void CanlinkFun(void){}

#endif
