
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

        else if(DPFuncCmd == DP_FUNC_WRITE_RAM)
        {
            commRcvData.commCmd = SCI_CMD_WRITE;
            commRcvData.commCmdSaveEeprom = SCI_WRITE_NO_EEPROM;
        }

        else if(DPFuncCmd == DP_FUNC_WRITE_EEPROM)
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

Uint16 ProfibusStartDeal(Uint16 tmp)
{

    if (!commRcvData.rcvDataJuageFlag)
    {

        DPRcvTest = 0;

        commRcvData.rcvNumMax = DPDataNum[commProtocol] + 4;

        if (tmp == PROFIBUS_CHECK)
        {
            rcvFrame[0] = tmp;
            commRcvData.rcvFlag = 0;
            rcvRigthFlag = 2;
        }

        else if(rcvRigthFlag == 2)
        {
            rcvFrame[1] = tmp;
            commRcvData.rcvFlag = 0;
            rcvRigthFlag = 3;
        }
        else if(rcvRigthFlag == 3)
        {
			rcvRigthFlag = 0;

            if(tmp == PROFIBUS_START_LOW)
            {
				DPRcvTest = 1;
                rcvFrame[2] = tmp;
                commRcvData.rcvDataJuageFlag = 1;
                commRcvData.rcvFlag = 2;
                commRcvData.rcvNum = 3;
                commRcvData.rcvNumMax = 5;
            }
        }

        else if(tmp == PROFIBUS_START_HIGH)
        {
            rcvRigthFlag = 1;
            rcvFrame[0] = tmp;
            commRcvData.rcvFlag = 0;
        }
        else if(rcvRigthFlag == 1)
        {
            rcvRigthFlag = 0;
            if(tmp == PROFIBUS_START_LOW)
            {
                rcvFrame[1] = tmp;
                commRcvData.rcvDataJuageFlag = 1;
                commRcvData.rcvFlag = 2;
                commRcvData.rcvNum = 2;
            }
        }
        else
        {
            rcvRigthFlag = 0;
            commRcvData.rcvCrcErrCounter  = 0;
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
        commRcvData.rcvNumMax = 5;
    }
    else
    {
        commRcvData.rcvNumMax = DPDataNum[commProtocol] + 4;
    }

    commRcvData.frameSpaceTime = 0;

    commRcvData.delay = 0;
}

void ProfibusSendDataDeal(Uint16 err)
{
    Uint16 PZD2ReturnData;
    Uint16 readDataStartIndex;
	Uint16 i, pzdNum, indexRead,indexWrite, group, grade;
    Uint16 pzdDataSaveCmd, pzdSaveData;
    commSendData.sendNumMax = commRcvData.rcvNumMax;

    if (DPRcvTest)
    {
        sendFrame[0] = PROFIBUS_CHECK;

        sendFrame[1] = funcCode.code.commSlaveAddress;

        sendFrame[2] = commProtocol + 1;
        if (sendFrame[2] > 3)
        {
            sendFrame[2] = 5;
        }
        return;
    }

    sendFrame[0] = PROFIBUS_START_HIGH;
    sendFrame[1] = PROFIBUS_START_LOW;
    sendFrame[2] = 0x10;
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
    else if (err)
    {
        sendFrame[2] = 0x70;
        sendFrame[8] = 0x00;
        sendFrame[9] = err;
    }

    else if (sciFlag.bit.read)
    {

        sendFrame[8] = commReadData[0] >> 8;
        sendFrame[9] = commReadData[0] & 0x00ff;
    }
    else if (sciFlag.bit.write)
    {

        if (saveEepromFlag)
        {
            Uint16 highH,group,grade,index;

            highH = (commRcvData.commAddr & 0xF000);

            group = (DPhighFuncAddr >> 8) & 0x0F;
            grade = DPlowFuncAddr & 0xFF;

            if (0xA000 == highH)
            {
                group += FUNCCODE_GROUP_A0;
            }
            else if(0xB000 == highH)
            {
                group += FUNCCODE_GROUP_B0;
            }

            index = GetGradeIndex(group, grade);

            if ((saveEepromIndex)
                && (index != saveEepromIndex)
                )
            {

                SaveOneFuncCode(saveEepromIndex);
            }
            saveEepromIndex = index;

        }
    }

    if (commProtocol != PPO3)
    {
        readDataStartIndex = 10;
		pzdNum = (commSendData.sendNumMax - 16) >> 1;
    }
	else
	{
		readDataStartIndex = 2;
		pzdNum = 0;
	}

    if (!sciFlag.bit.crcChkErr)
    {
        Uint16 frq;

        commRunCmd = rcvFrame[readDataStartIndex + 1];

        frq = (rcvFrame[readDataStartIndex + 2] << 8) + rcvFrame[readDataStartIndex + 3];

        if (frq <= maxFrq)
        {
            dpFrqAim = frq;
        }
    }

    setDPStatusInfo();
	if (motorFc.motorCtrlMode == FUNCCODE_motorCtrlMode_FVC)
	{
        PZD2ReturnData = frqFdb;
	}
    else
    {
        PZD2ReturnData = frqRunDisp;
    }

    sendFrame[readDataStartIndex++]     = (dpStatus.all >> 8);
    sendFrame[readDataStartIndex++] = (dpStatus.all & 0x00ff);

	sendFrame[readDataStartIndex++] = (PZD2ReturnData >> 8);
    sendFrame[readDataStartIndex++] = (PZD2ReturnData & 0x00ff);

	pzdDataSaveCmd = (PZDHighCmd << 8) + PZDlowCmd;

	for (i = 0; i < pzdNum; i++)
	{

		group = funcCode.group.fe[i] / 100;
        grade = funcCode.group.fe[i] % 100;
		indexWrite = GetGradeIndex(group, grade);

	    group = funcCode.group.fe[i + 10] / 100;
        grade = funcCode.group.fe[i + 10] % 100;
		indexRead = GetGradeIndex(group, grade);

        if (!sciFlag.bit.crcChkErr)
        {
            pzdSaveData = (rcvFrame[readDataStartIndex] << 8) + rcvFrame[readDataStartIndex + 1];
            if (ModifyFunccodeUpDown(indexWrite, &pzdSaveData, 0) == COMM_ERR_NONE)
            {
                ModifyFunccodeEnter(indexWrite, pzdSaveData);
            }
        }

		sendFrame[readDataStartIndex++] = (funcCode.all[indexRead] >> 8);
    	sendFrame[readDataStartIndex++] = (funcCode.all[indexRead] & 0x00ff);
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
