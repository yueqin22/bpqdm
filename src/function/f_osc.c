
#include "f_osc.h"
#include "f_comm.h"

#define     DEBUG_F_OSC         1

#if DEBUG_F_OSC

#if (SCI_INT_LOAD_RAM == 1)
	#if (OSC_TX_INT_EN == 1)
		#pragma CODE_SECTION(sciaTxFifoIsr, "ramfuncs");
	#endif
	#pragma CODE_SECTION(sciaRxFifoIsr, "ramfuncs");
#endif

#if OSC_CON_CHECK
extern Uint16 CrcValueByteCalc(const Uint16 *data, Uint16 length);
#endif

#if FC_CODE_CONTROL_EN
Uint16 OscControlWriter(Uint16 addr, Uint16 data);
Uint16 OscControlRead(Uint16 addr, Uint16* result);
#endif

#define	OSC_SCI_CH_NUM			4
#define	OSC_SCI_CHUNK			4
#define	OSC_TX_FRAME_LEN		68
#define	OSC_CON_FRAME_LEN		8

#if 	(1 == OSC_SCI_SEL)
	#define	OSC_PIE_RX_MASK			0x0001
	#define	OSC_PIE_TX_MASK			0x0002
#else
	#define	OSC_PIE_RX_MASK			0x0004
	#define	OSC_PIE_TX_MASK			0x0008
#endif

static Uint16 *OscQuqData = (Uint16 *)((&funcCode.group.u0[0]) -1);

#define 	GET_OSC_DATA(addr)				OscQuqData[addr]

static const	Uint16	BautRateCfg[3] = {
									(Uint16)(PERIPHERAL_CLK / 115200 / 8 -1),\
									(Uint16)(PERIPHERAL_CLK / 57600 / 8 -1),\
									(Uint16)(PERIPHERAL_CLK / 19200 / 8 -1)
								};

static const	DSP_OSC_CON_DATA	OscCongDefault = {
									0,
									3,
									1,
									1,
									1,
									0,
									0,
									0,
									1
								};

static DSP_OSC_CON_DATA	OscCongData;
static OSC_DATA_TYPE	OscDataBufA, OscDataBufB;
static uint8		OscConFrameBuf[10];
static uint8		BaudCfg = 0;

static SCI_RT_CON_DATA 	SciTxBufStr, SciRxBufStr;

static void InitOsc(void);
static void InitSciOSC(uint8);
static uint8 SciDataTX(uint8 *buf, uint8 len);

#if OSC_TX_INT_EN == 1
static interrupt void sciaTxFifoIsr(void);
#endif
static interrupt void sciaRxFifoIsr(void);
static uint8 SciDataRx(uint8 *buf, uint8 len, uint8 timeout);
static void SciDataRxDeal(void);
static void OscDataQcq(void);
static void OscDataTxDeal(void);
static uint8 GetOscData(uint8 *databuf);
static void OscSciIoInit(void);
#if OSC_DATA_CHECK
static void ClearOscBufEnd(void);

#endif

void OscSciFunction(void)
{
	static uint8 count = 0;
	static uint8 countRx = 0;
	static uint8 OscCfg = 0;

	if (BaudCfg)
	{
		count ++;
		if (count > 2)
		{
			InitSciOSC(OscCongData.baudRate);
			BaudCfg = 0;
			count = 0;
		}
		return;
	}

	if (0 == OscCfg)
	{
		OscDataBufA.frameHead1 = OSC_DATA_FRAME_HEAD_A;
		OscDataBufA.frameHead2 = OSC_DATA_FRAME_HEAD_B;
		OscDataBufB.frameHead1 = OSC_DATA_FRAME_HEAD_A;
		OscDataBufB.frameHead2 = OSC_DATA_FRAME_HEAD_B;
		OscDataBufA.rwPI = 0;
		OscDataBufB.rwPI = 0;
		OscDataBufA.full = 0;
		OscDataBufB.full = 0;

		InitOsc();
		OscCfg = 1;
		return;
	}

	countRx ++;
	if (countRx > 4)
	{
		countRx = 0;
		if (TRUE == SciDataRx((uint8 *)(&OscConFrameBuf), OSC_CON_FRAME_LEN, 0))
		{
			SciDataRxDeal();
			return;
		}
	}

	OscDataQcq();
	OscDataTxDeal();
}

void InitOsc(void)
{
	OscCongData = OscCongDefault;
	InitSciOSC(OscCongData.baudRate);
}

void InitSciOSC(uint8 baudRate)
{
	Uint16 baud;

	SciTxBufStr.busy = 0;
	SciRxBufStr.busy = 0;
	SciRxBufStr.len = 0xff;

	PieCtrlRegs.PIEIER9.all &= (Uint16)(~OSC_PIE_RX_MASK);
	PieCtrlRegs.PIEIER9.all &= (Uint16)(~OSC_PIE_TX_MASK);

	OscSciIoInit();

	SCI_OSC_REGS.SCICCR.all = 0x0007;
	SCI_OSC_REGS.SCICTL1.all = 0x0002 | 1<<6;

	baud = BautRateCfg[baudRate-1];
	SCI_OSC_REGS.SCILBAUD = baud;
	SCI_OSC_REGS.SCIHBAUD = baud>>8;

	SCI_OSC_REGS.SCICTL2.all = 0;

	SCI_OSC_REGS.SCIFFTX.all = 0xC000;

	SCI_OSC_REGS.SCIFFRX.all = 0x0004;
	SCI_OSC_REGS.SCIFFCT.all = 0x00;
	SCI_OSC_REGS.SCIPRI.bit.FREE = 1;

	SCI_OSC_REGS.SCICTL1.bit.SWRESET = 1;

	PieCtrlRegs.PIEIFR9.all &= (Uint16)(~OSC_PIE_RX_MASK);
	PieCtrlRegs.PIEIFR9.all &= (Uint16)(~OSC_PIE_TX_MASK);

	EALLOW;
#if 	(1 == OSC_SCI_SEL)
	PieVectTable.SCIRXINTA = &sciaRxFifoIsr;
	#if OSC_TX_INT_EN == 1
		PieVectTable.SCITXINTA = &sciaTxFifoIsr;
	#endif
#else
	PieVectTable.SCIRXINTB = &sciaRxFifoIsr;
	#if OSC_TX_INT_EN == 1
		PieVectTable.SCITXINTB = &sciaTxFifoIsr;
	#endif
#endif
	EDIS;

	PieCtrlRegs.PIEIER9.all |= OSC_PIE_RX_MASK;
#if OSC_TX_INT_EN == 1
	PieCtrlRegs.PIEIER9.all |= OSC_PIE_TX_MASK;
#else
	PieCtrlRegs.PIEIER9.all &= (Uint16)(~OSC_PIE_TX_MASK);
#endif

	PieCtrlRegs.PIECTRL.bit.ENPIE = 1;
	IER |= M_INT9;
}

uint8 SciDataTX(uint8 *buf, uint8 len)
{
	uint8 i;

#if RS485_ENABLE == 1
	RS485_RTS_O = RS485_T_O;
#endif
	if (TRUE == SciTxBufStr.busy)
	{
		return	FALSE;
	}

	SCI_OSC_REGS.SCIFFTX.all = 0xC000;
	SCI_OSC_REGS.SCIFFTX.bit.TXFIFOXRESET = 1;

	for (i = 0; i < OSC_SCI_CHUNK; i++)
	{
		SCI_OSC_REGS.SCITXBUF = *buf++;
	}

	SciTxBufStr.buf = buf;
	SciTxBufStr.len = len - OSC_SCI_CHUNK;
	SciTxBufStr.busy = TRUE;

#if OSC_TX_INT_EN == 1
	SCI_OSC_REGS.SCIFFTX.bit.TXFFINTCLR = 1;
	SCI_OSC_REGS.SCIFFTX.bit.TXFFIENA = 1;
#endif

	return TRUE;
}

#if OSC_TX_INT_EN == 0
void OscTxFifoTask(void)
{
	uint8 fifo, len;
	uint8 *buf;

	if (SciTxBufStr.busy == 0)
		return;

	len = SciTxBufStr.len;
	buf = SciTxBufStr.buf;
	if (len == 0)
	{
		SciTxBufStr.busy = 0;
		return;
	}
	fifo = 4 - SCI_OSC_REGS.SCIFFTX.bit.TXFFST;
	if (len < fifo)
	{
		fifo = len;
		len = 0;
	}
	else
	{
		len -= fifo;
	}
	while (fifo)
	{
		SCI_OSC_REGS.SCITXBUF = *buf++;
		fifo--;
	}
	SciTxBufStr.len = len;
	SciTxBufStr.buf = buf;
}
#endif

#if OSC_TX_INT_EN == 1
interrupt void sciaTxFifoIsr(void)
{
	uint8 *buf;
	uint8 len;
	uint8 i;

	buf = SciTxBufStr.buf;
	len = SciTxBufStr.len;
#if SCI_TX_INT_SPEEDUP == 1
	if (len > 3)
	{
		for (i = 0; i < OSC_SCI_CHUNK; i++)
		{
			SCI_OSC_REGS.SCITXBUF = *buf++;
		}
		SciTxBufStr.buf = buf;
		len -= OSC_SCI_CHUNK;
		SciTxBufStr.len = len;
	}
	else
	{
		SciTxBufStr.busy = 0;
		SCI_OSC_REGS.SCIFFTX.bit.TXFFIENA = 0;
	}
#else
	if (len > 3)
	{
		for (i = 0; i < OSC_SCI_CHUNK; i++)
		{
			SCI_OSC_REGS.SCITXBUF = *buf++;
		}
		len -= OSC_SCI_CHUNK;
	}
	else
	{
		while (len)
		{
			SCI_OSC_REGS.SCITXBUF = *buf++;
			len--;
		}
	}
	if (0 == SCI_OSC_REGS.SCIFFTX.bit.TXFFST)
	{
		SciTxBufStr.busy = 0;
		SCI_OSC_REGS.SCIFFTX.bit.TXFFIENA = 0;
	}
	else
	{
		SciTxBufStr.buf = buf;
		SciTxBufStr.len = len;
	}
#endif
	SCI_OSC_REGS.SCIFFTX.bit.TXFFINTCLR = 1;
	PieCtrlRegs.PIEACK.all |= PIEACK_GROUP9;
}
#endif

uint8 SciDataRx(uint8 *buf, uint8 len, uint8 timeout)
{
	static uint8 count = 0;
	static uint8 timeOut = 0;
	static uint8 countOut = 0;

#if RS485_ENABLE == 1
	if (SciTxBufStr.busy)
		return SCI_RS485_TX_BUSY;
	if (SCI_OSC_REGS.SCICTL2.bit.TXEMPTY)
		RS485_RTS_O = RS485_R_O;
	else
		return SCI_RS485_TX_BUSY;
#endif

	count ++;

	if ((timeOut != 0) && (count > timeOut))
	{
		SciRxBufStr.busy = 0;
		SCI_OSC_REGS.SCICTL1.bit.RXENA = 0;
		SCI_OSC_REGS.SCIFFRX.all = 4;
		count = 0;
		return FALSE;
	}

	if (TRUE == SciRxBufStr.busy)
	{
		if (SciRxBufStr.rxflag)
		{
			countOut = 0;
			SciRxBufStr.rxflag &= 0x10;
		}
		else
		{
			if (++countOut > 2)
			{
				SciRxBufStr.busy = 0;
				SCI_OSC_REGS.SCICTL1.bit.RXENA = 0;
				SCI_OSC_REGS.SCIFFRX.all = 4;
				return FALSE;
			}
		}
		return SCI_RT_BUSY;
	}

	if (0 == SciRxBufStr.len)
	{
		SCI_OSC_REGS.SCIFFRX.all = 4;
		SciRxBufStr.len = 0xff;
		return TRUE;
	}

	countOut = 0;

	SciRxBufStr.buf = buf;
	SciRxBufStr.len = len;
	SciRxBufStr.busy = TRUE;
	SciRxBufStr.rxflag = 0x10;
	count = 0;
	timeOut = timeout;

	SCI_OSC_REGS.SCIFFRX.bit.RXFIFORESET = 0;
	SCI_OSC_REGS.SCICTL1.bit.RXENA = 1;

	SCI_OSC_REGS.SCIFFRX.all = 1<<14 | 1<<13 | 1<<6 | 1<<5 | 4;

	return SCI_RT_BUSY;
}

interrupt void sciaRxFifoIsr(void)
{
	uint8 *buf;
	uint8 len;
	uint8 i;

	if (SCI_OSC_REGS.SCIRXST.bit.RXERROR)
	{
		BaudCfg = 1;
	}
	else
	{
		SciRxBufStr.rxflag = 0x01;
		buf = SciRxBufStr.buf;
		len = SciRxBufStr.len;

		for (i = 0; i < OSC_SCI_CHUNK; i++)
		{
			*buf++ = (uint8)(SCI_OSC_REGS.SCIRXBUF.bit.RXDT);
		}

		len -= OSC_SCI_CHUNK;
		if (0 == len)
		{
			SciRxBufStr.busy = 0;
			SCI_OSC_REGS.SCICTL1.bit.RXENA = 0;
			SCI_OSC_REGS.SCIFFRX.all = 1<<14 | 0<<13 | 1<<6 | 0<<5 | 4;
		}
		SciRxBufStr.buf = buf;
		SciRxBufStr.len = len;

		SCI_OSC_REGS.SCIFFRX.all = 1<<14 | 1<<13 | 1<<6 | 1<<5 | 4;
	}
	PieCtrlRegs.PIEACK.all |= PIEACK_GROUP9;
}

void SciDataRxDeal(void)
{
	Uint16 err = 0;
#if FC_CODE_CONTROL_EN
	Uint16 dat, rwaddr;
#endif
#if OSC_CON_CHECK
	Uint16 crcResult;
	Uint16 crcFrame;
#endif

#if OSC_CON_CHECK
	crcResult = CrcValueByteCalc((const Uint16 *)OscConFrameBuf, FC_FRAME_LEN-2);
	crcFrame = (Uint16)(( (CONTROL_FRAME_CRCH<<8) & 0xff00) | (CONTROL_FRAME_CRCL & 0x00ff) );

	if (crcResult != crcFrame)
	{
		return ;
	}
#endif

	if (CONTROL_FRAME_HEAD != OSC_CON_FRAME_HEAD)
	{
		return;
	}

	if (CONTROL_FRAME_FC == FC_STOP_OSC)
	{
		OscDataBufA.rwPI = 0;
		OscDataBufB.rwPI = 0;
		OscDataBufA.full = 0;
		OscDataBufB.full = 0;
		OscCongData.status = 0;
	}
	else if (OscCongData.status)
	{
		err = 0x80;
	}
	else
	{
		switch (CONTROL_FRAME_FC)
		{
			case FC_COMM_TEST:
				break;

			case FC_CHANNL_SEL:
				{
					uint8 sel[OSC_SCI_CH_NUM];
					uint8 sum = 0;
					uint8 i;

					sel[0] = CONTROL_FRAME_P1;
					sel[1] = CONTROL_FRAME_P2;
					sel[2] = CONTROL_FRAME_P3;
					sel[3] = CONTROL_FRAME_P4;

					for (i = 0; i < OSC_SCI_CH_NUM; i++)
					{
						if (sel[i])
						{
							sum ++;
						}
					}

					OscCongData.ch1Addr = sel[0];
					OscCongData.ch2Addr = sel[1];
					OscCongData.ch3Addr = sel[2];
					OscCongData.ch4Addr = sel[3];
					OscCongData.chSum = sum;
				}
#if OSC_DATA_CHECK
				if (OscCongData.chSum == 3)
				{
					ClearOscBufEnd();
				}
#endif
				break;

			case FC_PARA_CFG:
				if ((CONTROL_FRAME_P1>0) && (CONTROL_FRAME_P1<4))
				{
					OscCongData.baudRate = CONTROL_FRAME_P1;
					BaudCfg = 1;
				}
				if ((CONTROL_FRAME_P2>0) && (CONTROL_FRAME_P2<101))
				{
					OscCongData.interval = CONTROL_FRAME_P2;
				}

				break;
			case FC_START_OSC:
				OscCongData.status = 1;
				break;
#if FC_CODE_CONTROL_EN
			case FC_READ_FC_DATA:
				rwaddr = ( (CONTROL_FRAME_P2<<8) & 0xff00) | (CONTROL_FRAME_P1 & 0x00ff);

				err = OscControlRead(rwaddr, &dat);
				CONTROL_FRAME_P3 = dat&0x00ff;
				CONTROL_FRAME_P4 = dat>>8;

				break;
			case FC_WRITE_FC_DATA:
				rwaddr =  ((CONTROL_FRAME_P2<<8) & 0xff00) | (CONTROL_FRAME_P1 & 0x00ff);
				dat = ( (CONTROL_FRAME_P4<<8) & 0xff00) | (CONTROL_FRAME_P3 & 0x00ff);
				err = OscControlWriter(rwaddr, dat);

				break;
#endif
			default:
				err = 0x80;
				break;
		}
		if (err > 0)
		{
			CONTROL_FRAME_FC |= 0x80;
			CONTROL_FRAME_P2 = 0x80;
			CONTROL_FRAME_P1 = 0x01;
			CONTROL_FRAME_P4 = 0;
			CONTROL_FRAME_P3 = err;
		}
		else if (sciFlag.bit.pwdPass)
		{
			CONTROL_FRAME_P4 = 0x88;
			CONTROL_FRAME_P3 = 0x88;
		}

#if OSC_CON_CHECK
		crcResult = CrcValueByteCalc((const Uint16 *)OscConFrameBuf, FC_FRAME_LEN-2);
		CONTROL_FRAME_CRCL = crcResult;
		CONTROL_FRAME_CRCH = crcResult>>8;
#endif
		SciDataTX(OscConFrameBuf, OSC_CON_FRAME_LEN);
	}

}

void OscDataQcq(void)
{
	static uint8 count = 0;
	static uint8 bufSel = 0;

	OSC_DATA_TYPE *buf;
	uint8 *dataBuf;

	if (OscCongData.status != 1)
	{
		bufSel = 0;
		count = 0;
		OscDataBufA.rwPI = 0;
		OscDataBufA.full = 0;
		OscDataBufB.rwPI = 0;
		OscDataBufB.full = 0;
		return;
	}
	count ++;
	if (count != OscCongData.interval)
	{
		return;
	}

	buf = (bufSel == 0) ? &OscDataBufA : &OscDataBufB;

#if OSC_DATA_CHECK
	if (buf->rwPI == 0)
	{
		buf->check = OSC_DATA_FRAME_HEAD_A ^ OSC_DATA_FRAME_HEAD_B;
	}
#endif
	dataBuf = (uint8 *)(&(buf->oscDataBuf[buf->rwPI]));
	buf->check ^= GetOscData(dataBuf);
	buf->rwPI += OscCongData.chSum << 1;
	if (buf->rwPI > (OSC_BUF_DATA_LEN - (OscCongData.chSum << 1) ) )
	{
		buf->rwPI = 0;
		buf->full = 1;
		bufSel = (bufSel == 0) ? 1 : 0;
	}

	count = 0;
}

void OscDataTxDeal(void)
{
	static uint8  frame = 0;
	static uint8  bufSel = 0;

	OSC_DATA_TYPE *buf;

	if (OscCongData.status != 1)
	{
		bufSel = 0;
		return;
	}

	buf = (bufSel == 0) ? &OscDataBufA : &OscDataBufB;
	if (0 == buf->full)
	{
		return;
	}

	buf->frameNum = frame;
#if OSC_DATA_CHECK
	buf->check ^= frame ;
#endif

	if (SciDataTX((uint8 *)buf, OSC_TX_FRAME_LEN))
	{
		buf->full = 0;
		frame++;
		bufSel = (bufSel == 0) ? 1 : 0;
	}
}

#if OSC_DATA_CHECK
	#define OSC_GET_CHECK		check ^= *databuf;	databuf++
#else
	#define OSC_GET_CHECK		databuf++
#endif

uint8 GetOscData(uint8 *databuf)
{
	uint8 check = 0;
	uint8 addr[OSC_SCI_CH_NUM];
	uint8 i;

	addr[0] = OscCongData.ch1Addr;
	addr[1] = OscCongData.ch2Addr;
	addr[2] = OscCongData.ch3Addr;
	addr[3] = OscCongData.ch4Addr;

	for (i = 0; i < OSC_SCI_CH_NUM; i++)
	{
		if (addr[i])
		{
			*databuf = GET_OSC_DATA(addr[i]);
			OSC_GET_CHECK;
			*databuf = GET_OSC_DATA(addr[i]) >> 8;
			OSC_GET_CHECK;
		}
	}

	return(check);
}

#if OSC_DATA_CHECK
void ClearOscBufEnd(void)
{
	OSC_DATA_TYPE *buf[2];
	uint8 i;
	uint8 k;

	buf[0] = &OscDataBufA;
	buf[1] = &OscDataBufB;

	for (i = 0; i < 2; i++)
	{
		for (k = 1; k <= 4; k++)
		{
			buf[i]->oscDataBuf[OSC_BUF_DATA_LEN-k] = 0;
		}
	}
}
#endif

void OscSciIoInit(void)
{
	EALLOW;

	SysCtrlRegs.PCLKCR0.bit.SCIAENCLK = 1;
	GpioCtrlRegs.GPAPUD.all &= (Uint32)(~((Uint32)0x3 << 28));
	GpioCtrlRegs.GPAQSEL2.bit.GPIO28 = 3;
	GpioCtrlRegs.GPAMUX2.bit.GPIO28 = 1;
	GpioCtrlRegs.GPAMUX2.bit.GPIO29 = 1;

#if RS485_ENABLE == 1

	GpioCtrlRegs.GPAPUD.bit.GPIO27 = 0;
	GpioCtrlRegs.GPAMUX2.bit.GPIO27 = 0;
	GpioCtrlRegs.GPADIR.bit.GPIO27 = 1;
	GpioDataRegs.GPADAT.bit.GPIO27 = RS485_R_O;

#endif

	EDIS;
}

Uint16 OscControlWriter(Uint16 addr, Uint16 data)
{
	Uint16 oscReturn;

	commRcvData.commCmdSaveEeprom = SCI_WRITE_WITH_EEPROM;
	sciFlag.all = 0;
	oscReturn = CommWrite(addr, data);
	commRcvData.commCmdSaveEeprom = SCI_WRITE_NO_EEPROM;

	return oscReturn;
}

Uint16 OscControlRead(Uint16 addr, Uint16* result)
{
	Uint16 oscReturn;

	sciFlag.all = 0;
	oscReturn = CommRead(addr, 1);
	*result = commReadData[0];

	return oscReturn;
}

#else

void OscSciFunction(void)
{}

#endif
