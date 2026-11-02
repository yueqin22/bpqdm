
#ifndef		__f_dspcan__
#define		__f_dspcan__

#define		CAN_BAUD_SUM							7
#define		CAN_BAUD_1M								6
#define		CAN_BAUD_500K							5
#define		CAN_BAUD_250K							4
#define		CAN_BAUD_125K							3
#define		CAN_BAUD_100K							2
#define		CAN_BAUD_50K							1
#define		CAN_BAUD_20K							0

#define		CAN_INIT_TIME							1
#define		CAN_INIT_SUCC							2
#define		CAN_INIT_TIMEOUT						3
#define		CAN_INIT_BAUD_ERR						4

#define		CAN_MBOX_TRAN_SUCC						0
#define		CAN_MBOX_NUM_ERROR						1
#define		CAN_MBOX_BUSY							2
#define		CAN_MBOX_EMPTY							3
#define		CAN_MBOX_REC_SUCC						4
#define		CAN_MBOX_REC_OVER						5

typedef struct
{
	Uint16	BRPREG;
	Uint16	TSEG2REG;
	Uint16 	TSEG1REG;
}CAN_BAUD;

#define		ECANREGS			ECanaRegs
#define		ECANMBOXES			ECanaMboxes
#define		ECANLAMS			ECanaLAMRegs

extern Uint16 InitdspECan(Uint16 baud);
extern void InitTranMbox(Uint16 mbox, Uint32 msgid, Uint32 *dataPi);

extern void InitRecMbox(Uint16 mbox, Uint32 msgid, Uint32 lam);

Uint16 eCanDataTran(Uint16 mbox, Uint16 len, Uint32 msgid, Uint32 *dataPi);

extern Uint16 eCanDataRec(Uint16 mbox, Uint32 *dataPi);

#endif
