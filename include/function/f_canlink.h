
#ifndef	__f_canlink__
#define	__f_canlink__

#define		CAN_REMOTE_EN   		0

#define		CAN_REMOTE_FRAME		0xd
#define		CAN_CONTROL_FRAME		0x8
#define		CAN_CONFIG_FRAME		0xA
#define		CAN_DATA_FRAME			0xB

#define     CANlink_ASK             1
#define     CANlink_ACK             0

#define		CANLINK_RT_SUCC			0
#define		CANLINK_RT_BUSY			1
#define		CANLINK_RT_TIMEOUT		2
#define		CANLINK_R_EMPTY			3
#define		CANLINK_R_OVER			4

#define		CANLINK_ID_MASK			0x3f

#define		TRAN_MBOX_NUM			4
#define		REC_MBOX_NUM			4

#define		AAM_MBOX_N				31
#define		REC_BOX_N				30
#define		TRAN_BOX_N				15

#define		CAN_LINK_DEL			1
#define		CAN_LINK_INC			2
#define		CAN_LINK_R_CFG			3
#define		CAN_LINK_R_REG			4
#define		CAN_LINK_W_REG			5
#define		CAN_LINK_R_INFO			6
#define		CAN_LINK_R_WAR			7
#define		CAN_LINK_W_EEP			0x0A

#define		CAN_TRAN_TAB_CFG		0x0B
#define		CAN_REC_TAB_CFG			0x0C
#define		CAN_FUN_U3_CFG			0x0D
#define		CAN_FUN_C0_CFG			0x0E
#define		CAN_READ_PLC_INFO		0x0F

#define		CAN_LINK_S_WAR			0x10
#define		CAN_LINK_Q_CFG			0x20

#define		CAN_LINK_S_ADDR			(funcCode.code.commSlaveAddress & CANLINK_ID_MASK)
#define		CAN_LINK_BAUD_SEL		(funcCode.code.commBaudRate/1000)

#define     P2P_COMM_HOST           1
#define     P2P_COMM_SLAVE          2

struct	CANLINK_MsgID_BITS
{
	Uint16	srcSta:8;
	Uint16	destSta:8;
	Uint16	code:8;
	Uint16	aq:1;
	Uint16	framf:4;
	Uint16	aam:1;
	Uint16	ame:1;
	Uint16	ide:1;
};

union CANLINK_MsgID
{
	Uint32	all;
	struct	CANLINK_MsgID_BITS bit;
};

struct CANLINK_DATAHL
{
	Uint16 datah;
	Uint16 datal;
};

union CANLINK_DATA
{
	Uint32 all;
	struct CANLINK_DATAHL data;
};

struct CANLINK_DATA_BUF
{
	union CANLINK_MsgID msgid;
	union CANLINK_DATA mdl;
	union CANLINK_DATA mdh;
	Uint32 len;
};

struct CANLINK_REC_BUF
{
	Uint16 bufFull;
	struct CANLINK_DATA_BUF buf[REC_MBOX_NUM];
};

extern Uint16 CanRxTxCon;
extern Uint32 canLinkTicker;

extern void CanlinkFun(void);
extern Uint16 CanControlWriter(Uint16 addr, Uint16 data, Uint16 eeprom);
extern Uint16 CanControlRead(Uint16 addr, Uint16* result);
extern Uint16 CanlinkDataTran(Uint32 *dataPi, Uint16 len, Uint16 timeOut);

#define CAN_LINK_TIME_DEBUG                 0

#endif
