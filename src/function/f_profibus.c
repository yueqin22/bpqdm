
#include "f_comm.h"

#define DEBUG_F_DP1     1

#if DEBUG_F_DP1

#define PROFIBUS_START_HIGH    0xAA
#define PROFIBUS_START_LOW     0x55
#define PROFIBUS_CHECK         0x5A

#define DPHighStart       rcvFrame[0]
#define DPlowStart        rcvFrame[1]

#define DPFuncCmd         rcvFrame[2]>>4
#define DPhighFuncAddr    rcvFrame[3]
#define DPlowFuncAddr     rcvFrame[4]

#define PZDHighCmd        rcvFrame[6]
#define PZDlowCmd         rcvFrame[7]
#define DPhighFuncData    rcvFrame[8]
#define DPlowFuncData     rcvFrame[9]

#define DP_FUNC_READ          0x1
#define DP_FUNC_WRITE_RAM     0x2
#define DP_FUNC_WRITE_EEPROM  0xE

#define PPO1                     0
#define PPO2                     1
#define PPO3                     2
#define PPO5                     3
#define DP_RCV_CRC_ERR_NUMBER   10

#define DP_DATA_NUM_OFFSET         4
#define DP_TEST_FRAME_LEN          5
#define DP_TEST_PROTO_REPLY        5
#define DP_PROTO_REPLY_BASE        1
#define DP_PROTO_REPLY_MAX         PPO5
#define DP_PZD_HEADER_LEN          16
#define DP_PZD_DATA_OFFSET         10
#define DP_PPO3_DATA_OFFSET        2
#define DP_PZD_READ_SLOT           10
#define DP_RESP_FUNC_CODE          0x10
#define DP_RESP_ERR_CODE           0x70
#define DP_ADDR_HIGH_NIBBLE_MASK   0xF000
#define DP_ADDR_GROUP_MASK         0x0F
#define DP_ADDR_GRADE_MASK         0xFF
#define DP_GROUP_BASE_A0           0xA000
#define DP_GROUP_BASE_B0           0xB000
#define BYTE_MASK_LOW              0x00FF
#define FUNC_GROUP_SCALE           100

struct DP_CONTROL_CMD_BITS
{
    Uint16 stop:1;
    Uint16 freeStop:1;
    Uint16 fwdRun:1;
    Uint16 revRun:1;
    Uint16 fwdJog:1;
    Uint16 revJog:1;
    Uint16 reset:1;
#if 0
    Uint16 DpCmdEnable:1;
#endif
    Uint16 rsvd1:9;
};
union DP_CONTROL_CMD
{
    Uint16 all;
    struct DP_CONTROL_CMD_BITS bit;
};

struct DP_STATUS_BITS
{
    Uint16 run:1;
    Uint16 fwdRev:1;
    Uint16 error:1;
    Uint16 frqArrive:1;
    Uint16 rsvd1:12;

};

union DP_STATUS
{
    Uint16 all;
    struct DP_STATUS_BITS bit;
};

union DP_CONTROL_CMD dpControlCmd;
union DP_CONTROL_CMD dpControlCmdBak;
union DP_STATUS dpStatus;

Uint16 dpFrqAim;
const Uint16 DPDataNum[4] = {12, 20, 4, 32};
Uint16 rcvRigthFlag;
Uint16 DPRcvTest;

Uint16 saveEepromIndex = 0;
Uint16 saveEepromFlag = 0;

void ProfibusRcvDataDeal(void)
{
    commRcvData.commAddr = 0;
    commRcvData.commData = 0;
    commRcvData.commCmd = 0;
    saveEepromFlag = 0;

    if ((commProtocol != PPO3) && (!DPRcvTest))
    {

        commRcvData.commAddr = (DPhighFuncAddr << 8) + DPlowFuncAddr;

        commRcvData.commData = (DPhighFuncData << 8) + DPlowFuncData;

        if (DPFuncCmd == DP_FUNC_READ)
        {
            commRcvData.commCmd = SCI_CMD_READ;
            commRcvData.commData = 1;
        }

        else if (DPFuncCmd == DP_FUNC_WRITE_RAM)
        {
            commRcvData.commCmd = SCI_CMD_WRITE;
            commRcvData.commCmdSaveEeprom = SCI_WRITE_NO_EEPROM;
        }

        else if (DPFuncCmd == DP_FUNC_WRITE_EEPROM)
        {

            saveEepromFlag = 1;
            commRcvData.commCmd = SCI_CMD_WRITE;
            commRcvData.commCmdSaveEeprom = SCI_WRITE_NO_EEPROM;
        }

    }
#if 0
    else
    {

    }
#endif

    commRcvData.slaveAddr = 1;

    commRcvData.crcRcv = (rcvFrame[commRcvData.rcvNumMax - 1] << 8) + rcvFrame[commRcvData.rcvNumMax - 2];

    commRcvData.crcSize = commRcvData.rcvNumMax - 2;
}

Uint16 ProfibusStartDeal(Uint16 rxByte)
{

    if (!commRcvData.rcvDataJuageFlag)
    {

        DPRcvTest = 0;

        commRcvData.rcvNumMax = DPDataNum[commProtocol] + DP_DATA_NUM_OFFSET;

        if (rxByte == PROFIBUS_CHECK)
        {
            rcvFrame[0] = rxByte;
            commRcvData.rcvFlag = 0;
            rcvRigthFlag = 2;
        }

        else if (rcvRigthFlag == 2)
        {
            rcvFrame[1] = rxByte;
            commRcvData.rcvFlag = 0;
            rcvRigthFlag = 3;
        }
        else if (rcvRigthFlag == 3)
        {
            rcvRigthFlag = 0;

            if (rxByte == PROFIBUS_START_LOW)
            {
                DPRcvTest = 1;
                rcvFrame[2] = rxByte;
                commRcvData.rcvDataJuageFlag = 1;
                commRcvData.rcvFlag = 2;
                commRcvData.rcvNum = 3;
                commRcvData.rcvNumMax = DP_TEST_FRAME_LEN;
            }
        }

        else if (rxByte == PROFIBUS_START_HIGH)
        {
            rcvRigthFlag = 1;
            rcvFrame[0] = rxByte;
            commRcvData.rcvFlag = 0;
        }
        else if (rcvRigthFlag == 1)
        {
            rcvRigthFlag = 0;
            if (rxByte == PROFIBUS_START_LOW)
            {
                rcvFrame[1] = rxByte;
                commRcvData.rcvDataJuageFlag = 1;
                commRcvData.rcvFlag = 2;
                commRcvData.rcvNum = 2;
            }
        }
        else
        {
            rcvRigthFlag = 0;
            commRcvData.rcvCrcErrCounter = 0;
            commRcvData.rcvFlag = 0;
        }
        return 0;
    }
    return 1;
}

void getDPControlCmd(void)
{
#if 0
    if (!dpControlCmd.bit.DpCmdEnable)
    {
        commRunCmd = SCI_RUN_CMD_NONE;
    }
    else

    commRunCmd = SCI_RUN_CMD_NONE;

    if (dpControlCmd.bit.stop)
    {
        commRunCmd = SCI_RUN_CMD_STOP;
    }
    else if(dpControlCmd.bit.freeStop)
    {
        commRunCmd = SCI_RUN_CMD_FREE_STOP;
    }
    else if(dpControlCmd.bit.fwdRun)
    {
        commRunCmd = SCI_RUN_CMD_FWD_RUN;
    }
    else if(dpControlCmd.bit.revRun)
    {
        commRunCmd = SCI_RUN_CMD_REV_RUN;
    }
    else if(dpControlCmd.bit.fwdJog)
    {
        commRunCmd = SCI_RUN_CMD_FWD_JOG;
    }
    else if(dpControlCmd.bit.revJog)
    {
        commRunCmd = SCI_RUN_CMD_REV_JOG;
    }
    else if(dpControlCmd.bit.reset)
    {
        if (!dpControlCmdBak.bit.reset)
        {
            commRunCmd = SCI_RUN_CMD_RESET_ERROR;
        }
    }

    dpControlCmdBak = dpControlCmd;

    #endif
}

void setDPStatusInfo(void)
{
    dpStatus.all = 0;

    dpStatus.bit.run = runFlag.bit.run;

    dpStatus.bit.fwdRev = runFlag.bit.dir;

    if ((runFlag.bit.run)
        && (frq == frqAim))
    {

        dpStatus.bit.frqArrive = 1;
    }

    if (errorCode)
    {
        dpStatus.bit.error = 1;
    }

}

void UpdateProfibusCommFormat(Uint16 baudRate)
{

    if (DPRcvTest)
    {
        commRcvData.rcvNumMax = DP_TEST_FRAME_LEN;
    }
    else
    {
        commRcvData.rcvNumMax = DPDataNum[commProtocol] + DP_DATA_NUM_OFFSET;
    }

    commRcvData.frameSpaceTime = 0;

    commRcvData.delay = 0;
}

static void DpLatchEepromIndex(void)
{
    Uint16 addrNibble, eepromGroup, eepromGrade, eepromIndex;

    addrNibble = (commRcvData.commAddr & DP_ADDR_HIGH_NIBBLE_MASK);

    eepromGroup = (DPhighFuncAddr >> 8) & DP_ADDR_GROUP_MASK;
    eepromGrade = DPlowFuncAddr & DP_ADDR_GRADE_MASK;

    if (DP_GROUP_BASE_A0 == addrNibble)
    {
        eepromGroup += FUNCCODE_GROUP_A0;
    }
    else if (DP_GROUP_BASE_B0 == addrNibble)
    {
        eepromGroup += FUNCCODE_GROUP_B0;
    }

    eepromIndex = GetGradeIndex(eepromGroup, eepromGrade);

    if ((saveEepromIndex)
        && (eepromIndex != saveEepromIndex)
        )
    {

        SaveOneFuncCode(saveEepromIndex);
    }

    saveEepromIndex = eepromIndex;
}

void ProfibusSendDataDeal(Uint16 replyErr)
{
    Uint16 pzd2Feedback;
    Uint16 frameIndex;
    Uint16 pzdIndex, pzdCount, readFuncIndex, writeFuncIndex;
    Uint16 pzdDataSaveCmd, pzdWriteValue;
    commSendData.sendNumMax = commRcvData.rcvNumMax;

    if (DPRcvTest)
    {
        sendFrame[0] = PROFIBUS_CHECK;

        sendFrame[1] = funcCode.code.commSlaveAddress;

        sendFrame[2] = commProtocol + DP_PROTO_REPLY_BASE;
        if (sendFrame[2] > DP_PROTO_REPLY_MAX)
        {
            sendFrame[2] = DP_TEST_PROTO_REPLY;
        }
        return;
    }

    sendFrame[0] = PROFIBUS_START_HIGH;
    sendFrame[1] = PROFIBUS_START_LOW;
    sendFrame[2] = DP_RESP_FUNC_CODE;
    sendFrame[3] = DPhighFuncAddr;
    sendFrame[4] = DPlowFuncAddr;
    sendFrame[5] = 0x00;
    sendFrame[6] = 0x00;
    sendFrame[7] = 0x00;
    sendFrame[8] = DPhighFuncData;
    sendFrame[9] = DPlowFuncData;

    if (!(DPFuncCmd))
    {
        sendFrame[2] = 0x0;
        sendFrame[3] = 0x0;
        sendFrame[4] = 0x0;
    }
    else if (replyErr)
    {
        sendFrame[2] = DP_RESP_ERR_CODE;
        sendFrame[8] = 0x00;
        sendFrame[9] = replyErr;
    }

    else if (sciFlag.bit.read)
    {

        sendFrame[8] = commReadData[0] >> 8;
        sendFrame[9] = commReadData[0] & BYTE_MASK_LOW;
    }
    else if (sciFlag.bit.write)
    {

        if (saveEepromFlag)
        {
            DpLatchEepromIndex();
        }
    }

    if (commProtocol != PPO3)
    {
        frameIndex = DP_PZD_DATA_OFFSET;
        pzdCount = (commSendData.sendNumMax - DP_PZD_HEADER_LEN) >> 1;
    }
    else
    {
        frameIndex = DP_PPO3_DATA_OFFSET;
        pzdCount = 0;
    }

    if (!sciFlag.bit.crcChkErr)
    {
        Uint16 frq;

        commRunCmd = rcvFrame[frameIndex + 1];

        frq = (rcvFrame[frameIndex + 2] << 8) + rcvFrame[frameIndex + 3];

        if (frq <= maxFrq)
        {
            dpFrqAim = frq;
        }
    }

    setDPStatusInfo();
    if (motorFc.motorCtrlMode == FUNCCODE_motorCtrlMode_FVC)
    {
        pzd2Feedback = frqFdb;
    }
    else
    {
        pzd2Feedback = frqRunDisp;
    }

    sendFrame[frameIndex++] = (dpStatus.all >> 8);
    sendFrame[frameIndex++] = (dpStatus.all & BYTE_MASK_LOW);

    sendFrame[frameIndex++] = (pzd2Feedback >> 8);
    sendFrame[frameIndex++] = (pzd2Feedback & BYTE_MASK_LOW);

    pzdDataSaveCmd = (PZDHighCmd << 8) + PZDlowCmd;

    for (pzdIndex = 0; pzdIndex < pzdCount; pzdIndex++)
    {
        writeFuncIndex = GetGradeIndex(
            funcCode.group.fe[pzdIndex] / FUNC_GROUP_SCALE,
            funcCode.group.fe[pzdIndex] % FUNC_GROUP_SCALE);
        readFuncIndex = GetGradeIndex(
            funcCode.group.fe[pzdIndex + DP_PZD_READ_SLOT] / FUNC_GROUP_SCALE,
            funcCode.group.fe[pzdIndex + DP_PZD_READ_SLOT] % FUNC_GROUP_SCALE);

        if (!sciFlag.bit.crcChkErr)
        {
            pzdWriteValue = (rcvFrame[frameIndex] << 8) + rcvFrame[frameIndex + 1];
            if (ModifyFunccodeUpDown(writeFuncIndex, &pzdWriteValue, 0) == COMM_ERR_NONE)
            {
                ModifyFunccodeEnter(writeFuncIndex, pzdWriteValue);
            }
        }

        sendFrame[frameIndex++] = (funcCode.all[readFuncIndex] >> 8);
        sendFrame[frameIndex++] = (funcCode.all[readFuncIndex] & BYTE_MASK_LOW);
    }

}

Uint16 ProfibusCommErrCheck(void)
{
#if 1

    if ((funcCode.code.commOverTime) && (commTicker >= (Uint32)funcCode.code.commOverTime * 2 * TIME_UNIT_sciCommOverTime))
    {

        commTicker = (Uint32)funcCode.code.commOverTime * 2 * TIME_UNIT_sciCommOverTime;
        return 1;
    }

    if (commRcvData.rcvCrcErrCounter > DP_RCV_CRC_ERR_NUMBER)
    {

        commRcvData.rcvDataJuageFlag = 0;
        commRcvData.rcvCrcErrCounter = 0;
       return 1;
    }

    return 0;
#else
	return 0;
#endif
}

#elif 1

Uint16 ProfibusCommErrCheck(void){}
void ProfibusRcvDataDeal(void){}
void ProfibusSendDataDeal(Uint16 err){}
Uint16 ProfibusStartDeal(Uint16 tmp){}
void UpdateProfibusCommFormat(Uint16 baudRate){}

#endif
