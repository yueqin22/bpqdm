
#include "f_comm_DP.h"
#include "f_main.h"

#define DEBUG_F_DP             1

#if DEBUG_F_DP

#define SCI_M380_DP_DATA_DEFAULTS   \
{                                   \
    &SciaRegs                       \
}

struct SCI_DATA_DP sciM380DpData = SCI_M380_DP_DATA_DEFAULTS;

Uint16 sendData_DP[SEND_DATA_NUMBER];
Uint16 rcvData_DP[RCV_DATA_NUMBER];
enum DP_SCI_COMM_RCV_FLAG dpSciCommRcvFlag;
enum DP_SCI_COMM_SEND_FLAG dpSciCommSendFlag;

DP_PARAMETER dpParameter = {3,5};

Uint16 dataByteNum;

#define FIFO_NUM  16;

void InitSciDpBaudRate(struct SCI_DATA_DP *p);
interrupt void SCI_DP_RXD_isr(void);
interrupt void SCI_DP_TXD_isr(void);

void DpDataDeal(void);
void ErrAgainSend(struct SCI_DATA_DP *p);

void UpdateSciDpFormat(struct SCI_DATA_DP *p);
void GetDataByteNum(void);

static void ResetDpChannel(struct SCI_DATA_DP *p)
{
    p->rcvCrcErrCounter = 0;
    p->commDpRcvNumber = 0;
    p->commDpSendNum = 0;
    p->commDpSendNumMax = 0;
    p->rcvDataJuageFlag = 0;
    p->frameFlagDp = 0;
    p->sciRcvFlag = SCI_RCV_NO;
    UpdateSciDpFormat(&sciM380DpData);
}

static void CompleteDpFrame(struct SCI_DATA_DP *p, Uint16 length)
{
    p->rcvDataJuageFlag = 0;
    p->commDpRcvNumber = 0;
    p->sciRcvFlag = SCI_RCV_YES;
    p->commDpSendNumMax = length;
}

static void StoreCrc(Uint16 *pBuf, Uint16 loIndex, Uint16 crc)
{
    pBuf[loIndex + 1] = (crc >> 8) & 0x00ff;
    pBuf[loIndex] = crc & 0x00ff;
}

interrupt void SCI_DP_RXD_isr(void)
{
    Uint16 tmp;
    struct SCI_DATA_DP *p = &sciM380DpData;
    tmp = p->pSciRegs->SCIRXBUF.all;

    if(p->rcvDataJuageFlag == 0)
    {
#if BUG_SCI_BACK_DATA

        if(tmp == 0x5A)
        {
            p->frameFlagDp  = 1;
            p->rcvDataJuageFlag = 1;
        }
        else
#endif
        if(tmp == 0xAA)
        {
            p->rcvRigthFlag = 1;
            p->rcvData_SCI[0] = tmp;

        }
        else if(p->rcvRigthFlag == 1)
        {
            p->rcvRigthFlag = 0;
            if(tmp == 0x55)
            {
                p->commDpRcvNumber = 1;
                p->frameFlagDp  = 2;
                p->rcvDataJuageFlag = 1;
            }
        }
        else
        {
            ResetDpChannel(p);
        }

     }
#if BUG_SCI_BACK_DATA

    if(p->frameFlagDp == 1)
    {

        if (p->commDpRcvNumber < SCI_SEND_READ_NUMBER)
        {
            p->rcvData_SCI[p->commDpRcvNumber++] = tmp;
            if(p->commDpRcvNumber >= SCI_SEND_READ_NUMBER)
            {
                p->rcvDataJuageFlag = 0;
                p->commDpRcvNumber = 0;
                p->sciRcvFlag = SCI_RCV_YES;
                p->commDpSendNumMax = SCI_SEND_READ_NUMBER;
            }
        }
    }

    else
#endif
    if(p->frameFlagDp == 2)
    {
        Uint16 frameLen = (Uint16)(dataByteNum + 4);

        if (p->commDpRcvNumber < frameLen)
        {
            p->rcvData_SCI[p->commDpRcvNumber++] = tmp;
            if(p->commDpRcvNumber >= frameLen)
            {
                CompleteDpFrame(p, frameLen);
            }
        }
    }
    PieCtrlRegs.PIEACK.bit.ACK9 = 1;
}

interrupt void SCI_DP_TXD_isr(void)
{
    struct SCI_DATA_DP *p = &sciM380DpData;

#if NO_FIFO

    if (p->commDpSendNum < p->commDpSendNumMax)
    {
         p->pSciRegs->SCITXBUF = p->sendData_SCI[p->commDpSendNum++];
    }

#else

    Uint16 i;
    if (p->commDpSendNumMax < FIFO_NUM)
    {
        for (i = 0; i < p->commDpSendNumMax; i++)
        {
            p->pSciRegs->SCITXBUF = p->sendData_SCI[i];
        }
    }
    else
    {
        for (i = 0; i < FIFO_NUM; i++)
        {
            p->pSciRegs->SCITXBUF = p->sendData_SCI[i];
        }
    }
#endif
    PieCtrlRegs.PIEACK.bit.ACK9 = 1;
}

void inline CommDpStartSend(struct SCI_DATA_DP *p)
{
    p->pSciRegs->SCITXBUF = p->sendData_SCI[0];
    p->commDpSendNum = 1;
}

void CommDpRcvDataDeal(struct SCI_DATA_DP *p)
{
    Uint16 i;
    Uint16 crcValue;
    if (p->sciRcvFlag == SCI_RCV_YES)
    {
        p->sciRcvFlag = SCI_RCV_NO;

#if BUG_SCI_BACK_DATA
        if (p->frameFlagDp == 1)
        {
            crcValue = (p->rcvData_SCI[SCI_SEND_READ_NUMBER - 1] << 8) + p->rcvData_SCI[SCI_SEND_READ_NUMBER - 2];
            if (crcValue == CrcValueByteCalc(p->rcvData_SCI,(SCI_SEND_READ_NUMBER-2)))
            {

                p->frameSendStart.bit.frameType = DSP_TO_DP_PARAMETER;
                p->sendData_SCI[0] = 0x5A;
                p->sendData_SCI[1] = dpParameter.dpAddress;
                p->sendData_SCI[2] = dpParameter.dpDataFormat;
                crcValue = CrcValueByteCalc(p->sendData_SCI,(SCI_SEND_READ_NUMBER-2));
                p->sendData_SCI[SCI_SEND_READ_NUMBER-1] = (crcValue >> 8)&0x00ff;
                p->sendData_SCI[SCI_SEND_READ_NUMBER-2] = (crcValue&0x00ff);
                CommDpStartSend(&sciM380DpData);
            }
            else
            {
                p->rcvCrcErrCounter ++;
            }

            p->frameFlagDp = 0;

        }
        else
#endif
        if (p->frameFlagDp == 2)
        {
            Uint16 crcLow = (Uint16)(dataByteNum + 2);

            crcValue = (p->rcvData_SCI[crcLow + 1] << 8) + p->rcvData_SCI[crcLow];
            if(crcValue == CrcValueByteCalc(p->rcvData_SCI,crcLow))
            {
                for( i = 0; i < dataByteNum; i++)
                {
                    rcvData_DP[i] = p->rcvData_SCI[i+2];
                    #if BUG_SCI_BACK_DATA
                    sendData_DP[i] = rcvData_DP[i];
                    #endif
                }
                #if BUG_SCI_BACK_DATA
                funcCode.code.dpDataBuffer[1] = (((rcvData_DP[0] << 8)&0xff00) + rcvData_DP[1]);
                funcCode.code.dpDataBuffer[2] = (((rcvData_DP[2] << 8)&0xff00) + rcvData_DP[3]);
                funcCode.code.dpDataBuffer[3] = (((rcvData_DP[4] << 8)&0xff00) + rcvData_DP[5]);
                #endif
                dpSciCommRcvFlag = DP_SCI_COMM_RCV_YES;
            }
            else
            {
                p->rcvCrcErrCounter++;
            }

            p->frameFlagDp = 0;

        }

    }

    if(p->rcvCrcErrCounter > RCV_CRC_ERR_NUMBER)
    {
        ResetDpChannel(p);
    }
}

void CommDpSendDataDeal(struct SCI_DATA_DP *p)
{
     Uint16 i;
     Uint16 crcValue;
	 if ( dpSciCommSendFlag == DP_SCI_COMM_SEND_YES )
     {
        dpSciCommSendFlag = DP_SCI_COMM_SEND_NO;

        p->frameSendStart.bit.frameType = DSP_TO_DP;
        p->sendData_SCI[0] = 0xAA;
        p->sendData_SCI[1] = 0x55;

        for (i = 0; i < dataByteNum; i++)
        {
            p->sendData_SCI[i + 2] = rcvData_DP[i];
        }
        crcValue = CrcValueByteCalc(p->sendData_SCI,(dataByteNum+2));
        StoreCrc(p->sendData_SCI, (Uint16)(dataByteNum + 2), crcValue);
        CommDpStartSend(&sciM380DpData);
    }
}

void SciDpDeal(struct SCI_DATA_DP *p)
{

    dpParameter.dpAddress = 3;
    dpParameter.dpDataFormat = 5;

    GetDataByteNum();

    UpdateSciDpFormat(&sciM380DpData);
    CommDpRcvDataDeal(&sciM380DpData);

    #if BUG_SCI_BACK_DATA
    if( dpSciCommRcvFlag == DP_SCI_COMM_RCV_YES)
    {
        dpSciCommRcvFlag = DP_SCI_COMM_RCV_NO;
        dpSciCommSendFlag = DP_SCI_COMM_SEND_YES;
    }
    #else
    DpDataDeal();
    #endif
    CommDpSendDataDeal(&sciM380DpData);

}

void ErrAgainSend(struct SCI_DATA_DP *p)
{
     Uint16 crcValue;
     Uint16 i;

     p->frameSendStart.bit.frameType = ERR_FRAME;
     p->sendData_SCI[0] = p->frameSendStart.all;

     i = 0;
     while (i < dataByteNum)
     {
        p->sendData_SCI[i+1] = 0x55;
        i++;
     }
     crcValue = CrcValueByteCalc(p->sendData_SCI,(dataByteNum+1));
     StoreCrc(p->sendData_SCI, (Uint16)(dataByteNum + 1), crcValue);

     CommDpStartSend(&sciM380DpData);
}

void InitSetSciDp(struct SCI_DATA_DP *p)
{
    volatile struct SCI_REGS *pReg = p->pSciRegs;

#if NO_FIFO
    pReg->SCICTL1.all = 0x0001;
    pReg->SCICTL2.all = 0x00C2;
    pReg->SCICCR.all = 0x0087;
    pReg->SCIPRI.bit.FREE = 1;
    pReg->SCICTL1.all = 0x0023;
    pReg->SCICTL2.all = 0x00C3;
    pReg->SCICCR.all = 0x0087;
    InitSciDpBaudRate(&sciM380DpData);
#else
    pReg->SCICTL1.all = 0x0001;
    pReg->SCICTL2.all = 0x0082;
    pReg->SCIFFTX.all = 0xE060;
    pReg->SCICCR.all = 0x0087;
    pReg->SCIPRI.bit.FREE = 1;
    pReg->SCICTL1.all = 0x0023;
    pReg->SCICTL2.all = 0x0083;
    pReg->SCICCR.all = 0x0087;
    InitSciDpBaudRate(&sciM380DpData);
#endif
}

void UpdateSciDpFormat(struct SCI_DATA_DP *p)
{
    volatile struct SCI_REGS *pReg = p->pSciRegs;

    if (pReg->SCIRXST.bit.RXERROR)
    {
        InitSetSciDp(&sciM380DpData);
    }

    pReg->SCICCR.all = 0x0087;
    InitSciDpBaudRate(&sciM380DpData);
}

void InitSciaGpioDp(void)
{
    EALLOW;

    GpioCtrlRegs.GPAPUD.bit.GPIO28 = 0;
    GpioCtrlRegs.GPAPUD.bit.GPIO29 = 0;

    GpioCtrlRegs.GPAQSEL2.bit.GPIO28 = 3;

    GpioCtrlRegs.GPAMUX2.bit.GPIO28 = 1;
    GpioCtrlRegs.GPAMUX2.bit.GPIO29 = 1;

    EDIS;

    EALLOW;
    PieVectTable.SCIRXINTA = SCI_DP_RXD_isr;
	PieVectTable.SCITXINTA = SCI_DP_TXD_isr;
    EDIS;
	IER |= M_INT9;
	PieCtrlRegs.PIEIER9.bit.INTx1 = 1;
	PieCtrlRegs.PIEIER9.bit.INTx2 = 1;
}

void InitScibGpioDp(void)
{
    InitSciaGpioDp();
}

void InitSciDpBaudRate(struct SCI_DATA_DP *p)
{
#if 0
    switch(funcCode.group.c2[3])
    {
         case SCI_BAUD_RATE1:
            p->pSciRegs->SCIHBAUD = 0x0000;
            p->pSciRegs->SCILBAUD = 0x001a;
            break;

         case SCI_BAUD_RATE2:
            p->pSciRegs->SCIHBAUD = 0x0000;
            p->pSciRegs->SCILBAUD = 0x000e;
            break;

         case SCI_BAUD_RATE3:
            p->pSciRegs->SCIHBAUD = 0x0000;
            p->pSciRegs->SCILBAUD = 0x000b;
            break;

		 case SCI_BAUD_RATE4:
            p->pSciRegs->SCIHBAUD = 0x0000;
            p->pSciRegs->SCILBAUD = 0x0005;
            break;

         default:
            break;
    }
#elif 1
			p->pSciRegs->SCIHBAUD = 0x0000;
            p->pSciRegs->SCILBAUD = 0x000e;
#endif
}

const Uint16 dataByteNums[5] = {12, 20, 0, 4, 32};
void GetDataByteNum(void)
{
    Uint16 format = dpParameter.dpDataFormat;

    dataByteNum = dataByteNums[format - 1];
}

Uint16 dpTest = 2;
void DpDataDeal(void)
{

    if (DP_SCI_COMM_RCV_NO == dpSciCommRcvFlag)
    {
        return;
    }
#if 0
    for (i = 0; i < RCV_DATA_NUMBER; i++)
    {
        sendData_DP[i] = rcvData_DP[i] + dpTest;
    }
#elif 0
    ethDpPara.rcvFlag = 1;
    for (i = 0; i < 16; i++)
    {
       ethDpPara.rcv[i] = (rcvData_DP[2*i+1]&0x00ff)  + (rcvData_DP[2*i]<< 8&0xff00);
    }
    ethDpDeal();
    for (i = 0; i < ETH_PLC_RCV_LENGTH; i++)
    {
        sendData_DP[2*i] = ((ethDpPara.reply[ i ] >> 8)&0x00ff);
        sendData_DP[2*i + 1] = (ethDpPara.reply[ i ]&0x00ff);
    }
#endif

    dpSciCommRcvFlag = DP_SCI_COMM_RCV_NO;
    dpSciCommSendFlag = DP_SCI_COMM_SEND_YES;
}

#elif 1

void InitScibGpioDp(void){}
void InitSetSciDp(struct SCI_DATA_DP *p){}
void SciDpDeal(struct SCI_DATA_DP *p){}

#endif
