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
    Uint16 highByte, lowByte;

    commRcvData.slaveAddr = RTUslaveAddress;
    commRcvData.commCmd = RTUcmd;

    highByte = RTUhighAddr;
    lowByte = RTUlowAddr;
    commRcvData.commAddr = (highByte << 8) + lowByte;

    highByte = RTUhighData;
    lowByte = RTUlowData;
    commRcvData.commData = (highByte << 8) + lowByte;

    highByte = RTUhighCrc;
    lowByte = RTUlowCrc;
    commRcvData.crcRcv = (highByte << 8) + lowByte;

    commRcvData.crcSize = 6;
    commRcvData.commCmdSaveEeprom = SCI_WRITE_WITH_EEPROM;
}

Uint16 ModbusStartDeal(Uint16 tmp)
{
    if (commTicker <= commRcvData.frameSpaceTime)
        return 1;

    RTUslaveAddress = tmp;

    if (RTUslaveAddress == 0)
    {
        commRcvData.rcvNum = 1;
        commRcvData.rcvFlag = 1;
    }
    else if ( ((funcCode.code.commProtocolSec == CAN_OPEN) && (RTUslaveAddress == 1))
              || (RTUslaveAddress == funcCode.code.commSlaveAddress) )
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

void UpdateModbusCommFormat(Uint16 baudRate)
{
    Uint16 frameSpaceTime;

    frameSpaceTime = 385 * 2 / baudRate + 1;

    commRcvData.rcvNumMax = RTU_MASTER_FRAME_NUM_MAX;
    commRcvData.frameSpaceTime = frameSpaceTime;
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
        return;
    }

    if (sciFlag.bit.pwdPass || sciFlag.bit.write)
    {
        sendFrame[2] = RTUhighAddr;
        sendFrame[3] = RTUlowAddr;

        if (sciFlag.bit.pwdPass)
        {
            sendFrame[4] = 0x88;
            sendFrame[5] = 0x88;
        }
        else
        {
            sendFrame[4] = RTUhighData;
            sendFrame[5] = RTUlowData;
        }
        return;
    }

    if (sciFlag.bit.read)
    {
        Uint16 sendNum;
        Uint16 readDataStartIndex;

        sendNum = commRcvData.commData << 1;

        if (0 == commProtocol)
        {
            sendFrame[2] = sendNum >> 8;
            sendFrame[3] = sendNum & 0x00ff;
            commSendData.sendNumMax = sendNum + 6;
            readDataStartIndex = 4;
        }
        else
        {
            sendFrame[2] = sendNum;
            commSendData.sendNumMax = sendNum + 5;
            readDataStartIndex = 3;
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
    Uint32 overTimeLimit;

    if (0 == funcCode.code.commOverTime)
        return 0;

    overTimeLimit = (Uint32)funcCode.code.commOverTime * 2 * TIME_UNIT_sciCommOverTime;

    if (commTicker < overTimeLimit)
        return 0;

    commTicker = overTimeLimit;
    return 1;
}
