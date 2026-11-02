
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

static Uint16 *OscQuqData = (Uint16 *)((&funcCode.group.u0[0]) -1);

#define 	GET_OSC_DATA(addr)				OscQuqData[addr]

static const	Uint16	BautRateCfg[3] = {
									(Uint16)(PERIPHERAL_CLK / 115200 / 8 -1),\
									(Uint16)(PERIPHERAL_CLK / 57600 / 8 -1),\
									(Uint16)(PERIPHERAL_CLK / 19200 / 8 -1)
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
		if (SciDataRx((uint8 *)(&OscConFrameBuf), 8, 0) == TRUE)
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
	OscCongData.baudRate = 3;
    OscCongData.interval = 1;
    OscCongData.status = 0;
    OscCongData.runContinue = 1;
    OscCongData.ch1Addr = 1;
    OscCongData.ch2Addr = 0;
    OscCongData.ch3Addr = 0;
    OscCongData.ch4Addr = 0;
	OscCongData.chSum = 1;

    InitSciOSC(OscCongData.baudRate);
}

void InitSciOSC(uint8 baudRate)
{
    SciTxBufStr.busy = 0;
    SciRxBufStr.busy = 0;

	SciRxBufStr.len = 0xff;
#if 	(1 == OSC_SCI_SEL)
    PieCtrlRegs.PIEIER9.bit.INTx1 = 0;
	PieCtrlRegs.PIEIER9.bit.INTx2 = 0;
#else
    PieCtrlRegs.PIEIER9.bit.INTx3 = 0;
	PieCtrlRegs.PIEIER9.bit.INTx4 = 0;
#endif

	OscSciIoInit();

	SCI_OSC_REGS.SCICCR.all = 0x0007;

	SCI_OSC_REGS.SCICTL1.all = 0x0002 | 1<<6;

   SCI_OSC_REGS.SCILBAUD = BautRateCfg[baudRate-1];
   SCI_OSC_REGS.SCIHBAUD = BautRateCfg[baudRate-1]>>8;

   SCI_OSC_REGS.SCICTL2.all = 0;

   SCI_OSC_REGS.SCIFFTX.all = 0xC000;

   SCI_OSC_REGS.SCIFFRX.all = 0x0004;
   SCI_OSC_REGS.SCIFFCT.all = 0x00;
   SCI_OSC_REGS.SCIPRI.bit.FREE = 1;

   SCI_OSC_REGS.SCICTL1.bit.SWRESET = 1;

#if 	(1 == OSC_SCI_SEL)
	PieCtrlRegs.PIEIFR9.bit.INTx1 = 0;
    PieCtrlRegs.PIEIFR9.bit.INTx2 = 0;
#else
	PieCtrlRegs.PIEIFR9.bit.INTx3 = 0;
    PieCtrlRegs.PIEIFR9.bit.INTx4 = 0;
#endif

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

#if 	(1 == OSC_SCI_SEL)
	PieCtrlRegs.PIEIER9.bit.INTx1 = 1;
	#if OSC_TX_INT_EN == 1
		PieCtrlRegs.PIEIER9.bit.INTx2 = 1;
	#else
		PieCtrlRegs.PIEIER9.bit.INTx2 = 0;
	#endif
#else
	PieCtrlRegs.PIEIER9.bit.INTx3 = 1;
	#if OSC_TX_INT_EN == 1
		PieCtrlRegs.PIEIER9.bit.INTx4 = 1;
	#else
		PieCtrlRegs.PIEIER9.bit.INTx4 = 0;
	#endif
#endif

	PieCtrlRegs.PIECTRL.bit.ENPIE = 1;
	IER |= M_INT9;
}

uint8 SciDataTX(uint8 *buf, uint8 len)
{

#if RS485_ENABLE == 1
	RS485_RTS_O = RS485_T_O;
#endif
	if (TRUE == SciTxBufStr.busy)
		return	FALSE;
	SCI_OSC_REGS.SCIFFTX.all = 0xC000;
	SCI_OSC_REGS.SCIFFTX.bit.TXFIFOXRESET = 1;

	SCI_OSC_REGS.SCITXBUF = *buf++;
	SCI_OSC_REGS.SCITXBUF = *buf++;
	SCI_OSC_REGS.SCITXBUF = *buf++;
	SCI_OSC_REGS.SCITXBUF = *buf++;
	SciTxBufStr.buf = buf;
    SciTxBufStr.len = len - 4;
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

	buf = SciTxBufStr.buf;
	len = SciTxBufStr.len;
#if SCI_TX_INT_SPEEDUP == 1
	if (len > 3)
    {
    	SCI_OSC_REGS.SCITXBUF = *buf++;
    	SCI_OSC_REGS.SCITXBUF = *buf++;
    	SCI_OSC_REGS.SCITXBUF = *buf++;
    	SCI_OSC_REGS.SCITXBUF = *buf++;
		SciTxBufStr.buf = buf;
		len -= 4;
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
    	SCI_OSC_REGS.SCITXBUF = *buf++;
    	SCI_OSC_REGS.SCITXBUF = *buf++;
    	SCI_OSC_REGS.SCITXBUF = *buf++;
    	SCI_OSC_REGS.SCITXBUF = *buf++;
		len -= 4;
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

	if (( count > timeOut) && (timeOut != 0) )
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
			countOut =0;
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

	if (SCI_OSC_REGS.SCIRXST.bit.RXERROR)
	{
		BaudCfg = 1;
	}
	else
	{
		SciRxBufStr.rxflag = 0x01;
		buf = SciRxBufStr.buf;
		len = SciRxBufStr.len;

		*buf++ = (uint8)(SCI_OSC_REGS.SCIRXBUF.bit.RXDT);
		*buf++ = (uint8)(SCI_OSC_REGS.SCIRXBUF.bit.RXDT);
		*buf++ = (uint8)(SCI_OSC_REGS.SCIRXBUF.bit.RXDT);
		*buf++ = (uint8)(SCI_OSC_REGS.SCIRXBUF.bit.RXDT);
		len -= 4;
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
	Uint16  crcResult;
	crcResult = CrcValueByteCalc((const Uint16 *)OscConFrameBuf, FC_FRAME_LEN-2);

	if (crcResult != (Uint16)(( (CONTROL_FRAME_CRCH<<8) & 0xff00) | (CONTROL_FRAME_CRCL & 0x00ff) ))
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
				OscCongData.chSum = 0;

				OscCongData.ch1Addr = CONTROL_FRAME_P1;
				if (CONTROL_FRAME_P1)	OscCongData.chSum ++;
				OscCongData.ch2Addr = CONTROL_FRAME_P2;
				if (CONTROL_FRAME_P2)	OscCongData.chSum ++;
				OscCongData.ch3Addr = CONTROL_FRAME_P3;
				if (CONTROL_FRAME_P3)	OscCongData.chSum ++;
				OscCongData.ch4Addr = CONTROL_FRAME_P4;
				if (CONTROL_FRAME_P4)	OscCongData.chSum ++;
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
		SciDataTX(OscConFrameBuf, 8);
	}

}

void OscDataQcq(void)
{
	static uint8 count = 0;
	static uint8 bufSel = 0;

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
	if (count == OscCongData.interval)
	{
		if (bufSel == 0)
		{
#if OSC_DATA_CHECK
			if (OscDataBufA.rwPI == 0)
				OscDataBufA.check = OSC_DATA_FRAME_HEAD_A ^ OSC_DATA_FRAME_HEAD_B;
#endif
			dataBuf = (uint8 *)(&(OscDataBufA.oscDataBuf[OscDataBufA.rwPI]) );
			OscDataBufA.check ^= GetOscData(dataBuf);
			OscDataBufA.rwPI += OscCongData.chSum << 1;
			if (OscDataBufA.rwPI > (OSC_BUF_DATA_LEN - (OscCongData.chSum << 1) ) )
			{
				OscDataBufA.rwPI = 0;
				OscDataBufA.full = 1;
				bufSel = 1;
			}
		}
		else
		{
#if OSC_DATA_CHECK
			if (OscDataBufB.rwPI == 0)
				OscDataBufB.check = OSC_DATA_FRAME_HEAD_A ^ OSC_DATA_FRAME_HEAD_B;
#endif
			dataBuf = (uint8 *)(&OscDataBufB.oscDataBuf[OscDataBufB.rwPI]);
			OscDataBufB.check ^= GetOscData(dataBuf);
			OscDataBufB.rwPI += OscCongData.chSum << 1;
			if (OscDataBufB.rwPI > (OSC_BUF_DATA_LEN - (OscCongData.chSum << 1) ) )
			{
				OscDataBufB.rwPI = 0;
				OscDataBufB.full = 1;
				bufSel = 0;
			}
		}
		count = 0;
	}
}

void OscDataTxDeal(void)
{
	static uint8  frame = 0;
	static uint8  bufSel = 0;
	if (OscCongData.status != 1)
	{
		bufSel = 0;
		return;
	}

	if (bufSel == 0)
	{
		if (OscDataBufA.full)
		{
			OscDataBufA.frameNum = frame;
#if OSC_DATA_CHECK
			OscDataBufA.check ^= frame ;
#endif

			if (SciDataTX((uint8 *)(&OscDataBufA), 68))
			{
				OscDataBufA.full = 0;
				frame++;
				bufSel = 1;
			}
		}
		return;
	}
	if (OscDataBufB.full)
	{
		OscDataBufB.frameNum = frame;
#if OSC_DATA_CHECK
			OscDataBufB.check ^= frame;
#endif

		if (SciDataTX((uint8 *)(&OscDataBufB), 68))
		{
			OscDataBufB.full = 0;
			frame++;
			bufSel = 0;
		}
	}
}

#if OSC_DATA_CHECK
	#define OSC_GET_CHECK		check ^= *databuf;	databuf++
#else
	#define OSC_GET_CHECK		databuf++
#endif

uint8 GetOscData(uint8 *databuf)
{
	uint8 check = 0;;
	if (OscCongData.ch1Addr)
	{
		*databuf = GET_OSC_DATA(OscCongData.ch1Addr);
		OSC_GET_CHECK;
		*databuf = GET_OSC_DATA(OscCongData.ch1Addr) >> 8;
		OSC_GET_CHECK;
	}
	if (OscCongData.ch2Addr)
	{
		*databuf = GET_OSC_DATA(OscCongData.ch2Addr);
		OSC_GET_CHECK;
		*databuf = GET_OSC_DATA(OscCongData.ch2Addr) >> 8;
		OSC_GET_CHECK;
	}
	if (OscCongData.ch3Addr)
	{
		*databuf = GET_OSC_DATA(OscCongData.ch3Addr);
		OSC_GET_CHECK;
		*databuf = GET_OSC_DATA(OscCongData.ch3Addr) >> 8;
		OSC_GET_CHECK;
	}
	if (OscCongData.ch4Addr)
	{
		*databuf = GET_OSC_DATA(OscCongData.ch4Addr);
		OSC_GET_CHECK;
		*databuf = GET_OSC_DATA(OscCongData.ch4Addr) >> 8;
		OSC_GET_CHECK;
	}
	return(check);
}

#if OSC_DATA_CHECK
void ClearOscBufEnd(void)
{
	OscDataBufA.oscDataBuf[OSC_BUF_DATA_LEN-1] = 0;
	OscDataBufA.oscDataBuf[OSC_BUF_DATA_LEN-2] = 0;
	OscDataBufA.oscDataBuf[OSC_BUF_DATA_LEN-3] = 0;
	OscDataBufA.oscDataBuf[OSC_BUF_DATA_LEN-4] = 0;
	OscDataBufB.oscDataBuf[OSC_BUF_DATA_LEN-1] = 0;
	OscDataBufB.oscDataBuf[OSC_BUF_DATA_LEN-2] = 0;
	OscDataBufB.oscDataBuf[OSC_BUF_DATA_LEN-3] = 0;
	OscDataBufB.oscDataBuf[OSC_BUF_DATA_LEN-4] = 0;
}
#endif

void OscSciIoInit(void)
{
	EALLOW;

	SysCtrlRegs.PCLKCR0.bit.SCIAENCLK = 1;
	GpioCtrlRegs.GPAPUD.bit.GPIO28 = 0;
	GpioCtrlRegs.GPAPUD.bit.GPIO29 = 0;
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

    Uint16 oscReturn = 0;

    sciFlag.all = 0;
    oscReturn = CommRead(addr, 1);
    *result = commReadData[0];
    return oscReturn;
}

#else

void OscSciFunction(void)
{}

#endif
