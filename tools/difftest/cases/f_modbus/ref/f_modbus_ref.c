
#include "f_comm.h"

#define RTU_MASTER_FRAME_NUM_MAX  8

#define RTUslaveAddress rcvFrame[0]
#define RTUcmd          rcvFrame[1]
#define RTUhighAddr     rcvFrame[2]
#define RTUlowAddr      rcvFrame[3]
#define RTUhighData     rcvFrame[4]
#define RTUlowData      rcvFrame[5]
#define RTUlowCrc       rcvFrame[6]
#define RTUhighCrc      rcvFrame[7]

void ModbusRcvDataDeal(void)
{
    commRcvData.slaveAddr = RTUslaveAddress;
    commRcvData.commCmd = RTUcmd;
    commRcvData.commAddr = (RTUhighAddr << 8) + RTUlowAddr;
    commRcvData.commData = (RTUhighData << 8) + RTUlowData;
    commRcvData.crcRcv = (RTUhighCrc << 8) + RTUlowCrc;
	commRcvData.crcSize = 6;
	commRcvData.commCmdSaveEeprom = SCI_WRITE_WITH_EEPROM;
}

Uint16 ModbusStartDeal(Uint16 tmp)
{
    if ((commTicker > commRcvData.frameSpaceTime))
    {
        RTUslaveAddress = tmp;

        if (RTUslaveAddress == 0)
        {
            commRcvData.rcvNum = 1;
            commRcvData.rcvFlag = 1;
        }
        else if ( (funcCode.code.commProtocolSec == CAN_OPEN) && (RTUslaveAddress == 1) )
        {
            commRcvData.rcvNum = 1;
            commRcvData.rcvFlag = 2;
        }

        else if (RTUslaveAddress == funcCode.code.commSlaveAddress)
        {
            commRcvData.rcvNum = 1;
            commRcvData.rcvFlag = 2;
        }

        else
        {
            commRcvData.rcvFlag = 0;
        }

        return 0;
    }

    return 1;
}

void UpdateModbusCommFormat(Uint16 baudRate)
{
    commRcvData.rcvNumMax= RTU_MASTER_FRAME_NUM_MAX;
    commRcvData.frameSpaceTime = 385 * 2 / baudRate+ 1;
    commRcvData.delay = funcCode.code.commDelay * 2;
}

void ModbusSendDataDeal(Uint16 err)
{
	int16 i;
    sendFrame[0] = rcvFrame[0];
    sendFrame[1] = rcvFrame[1];
    commSendData.sendNumMax = 8;
    if (err)
    {
        sendFrame[2] = 0x80;
	    sendFrame[3] = 0x01;
	    sendFrame[4] = 0x00;
        sendFrame[5] = err;
    }
    else if (sciFlag.bit.pwdPass)
    {
        sendFrame[2] = RTUhighAddr;
        sendFrame[3] = RTUlowAddr;
        sendFrame[4] = 0x88;
        sendFrame[5] = 0x88;
	}
    else if (sciFlag.bit.write)
    {
        sendFrame[2] = RTUhighAddr;
        sendFrame[3] = RTUlowAddr;
        sendFrame[4] = RTUhighData;
        sendFrame[5] = RTUlowData;
	}
    else if (sciFlag.bit.read)
    {
        Uint16 sendNum;
        Uint16 readDataStartIndex;
        sendNum = commRcvData.commData << 1;

        if (commProtocol)
        {
            sendFrame[2] = sendNum;
            commSendData.sendNumMax = sendNum + 5;
            readDataStartIndex = 3;
        }

        else if (commProtocol == 0)
        {
            sendFrame[2] = sendNum >> 8;
            sendFrame[3] = sendNum & 0x00ff;
            commSendData.sendNumMax = sendNum + 6;
            readDataStartIndex = 4;
        }

        for (i = commRcvData.commData - 1; i >= 0; i--)
        {
            sendFrame[(i << 1) + readDataStartIndex] = commReadData[i] >> 8;
            sendFrame[(i << 1) + readDataStartIndex + 1] = commReadData[i] & 0x00ff;
        }
    }
}

Uint16 ModbusCommErrCheck(void)
{
    if ((funcCode.code.commOverTime) && (commTicker >= (Uint32)funcCode.code.commOverTime * 2 * TIME_UNIT_sciCommOverTime))
    {

        commTicker = (Uint32)funcCode.code.commOverTime * 2 * TIME_UNIT_sciCommOverTime;
        return 1;
    }
    else
    {
        return 0;
    }
}
