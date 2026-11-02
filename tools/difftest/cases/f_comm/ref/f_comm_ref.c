
#include "f_comm.h"
#include "f_p2p.h"
#include "f_ui.h"

#if F_DEBUG_RAM
#define DEBUG_F_MODBUS          0
#elif 1
#define DEBUG_F_MODBUS          1
#endif

#define COMM_PARITY_MAX            3

enum COMM_STATUS commStatus;
COMM_SEND_DATA commSendData;
COMM_RCV_DATA commRcvData;
Uint16 commType;
Uint16 commProtocol;
Uint32 commTicker;
Uint32 canLinkTicker;
Uint16 commRunCmd;
Uint16 aoComm[AO_NUMBER+1];
union DO_SCI doComm;
Uint16 userPwdPass4Comm;
Uint16 companyPwdPass4Comm;
Uint16 groupHidePassComm;
LOCALF Uint16 sendFrame[35];
LOCALF Uint16 rcvFrame[35];
LOCALF Uint16 commReadData[RTU_READ_DATA_NUM_MAX];
LOCALF union SCI_FLAG sciFlag;

#if DEBUG_F_MODBUS

#if (DSP_CLOCK == 100)

#if !DSP_2803X
LOCALF const DSP_BAUD_REGISTER_DATA dspBaudRegData[BAUD_NUM_MAX + 1] =
{
    {   3, 0x0028, 0x00af},
    {   6, 0x0014, 0x0057},
    {  12, 0x000a, 0x002b},
    {  24, 0x0005, 0x0015},
    {  48, 0x0002, 0x008a},
    {  96, 0x0001, 0x0044},
    { 192, 0x0000, 0x00a1},
    { 384, 0x0000, 0x0050},
    { 576, 0x0000, 0x0035},
    {1152, 0x0000, 0x001a},
    {1280, 0x0000, 0x000E},
    {2560, 0x0000, 0x000b},
    {5120, 0x0000, 0x0005},
};
#endif
#elif (DSP_CLOCK == 60)

LOCALF const DSP_BAUD_REGISTER_DATA dspBaudRegData[BAUD_NUM_MAX + 1] =
{
#if DSP_2803X
    {   3, 0x0000, 0x1869,  0},
    {   6, 0x0000, 0x0C34,  0},
    {  12, 0x0000, 0x0619,  8},
    {  24, 0x0000, 0x030c,  4},
    {  48, 0x0000, 0x0185, 10},
    {  96, 0x0000, 0x00c2,  5},
    { 192, 0x0000, 0x0060, 11},
    { 384, 0x0000, 0x002f, 13},
    { 576, 0x0000, 0x001f,  9},
    {1152, 0x0000, 0x000f,  4},
    {1280, 0x0000, 0x0008,  0},
    {2560, 0x0000, 0x0006, 15},
    {5120, 0x0000, 0x0002, 11},
#else
    {   3, 0x0018, 0x0069},
    {   6, 0x000c, 0x0034},
    {  12, 0x0006, 0x0019},
    {  24, 0x0003, 0x000c},
    {  48, 0x0001, 0x0085},
    {  96, 0x0000, 0x00c2},
    { 192, 0x0000, 0x0060},
    { 384, 0x0000, 0x002f},
    { 576, 0x0000, 0x001f},
    {1152, 0x0000, 0x000f},
    {1280, 0x0000, 0x0008},
    {2560, 0x0000, 0x0006},
    {5120, 0x0000, 0x0002},
#endif
};
#endif

#if DSP_2803X
const Uint16 commParitys[4] = {0x10, 0x0c, 0x04, 0x00};
#else
const Uint16 commParitys[4] = {0x87, 0x67, 0x27, 0x07};
#endif

#define COMM_NO_W_FC_0  GetCodeIndex(funcCode.code.tuneCmd)
#define COMM_NO_W_FC_1  GetCodeIndex(funcCode.code.menuMode)
#define COMM_NO_W_FC_2  GetCodeIndex(funcCode.code.motorFcM2.tuneCmd)
#define COMM_NO_W_FC_3  GetCodeIndex(funcCode.code.motorFcM3.tuneCmd)
#define COMM_NO_W_FC_4  GetCodeIndex(funcCode.code.motorFcM4.tuneCmd)
#define COMM_NO_W_FC_5  GetCodeIndex(funcCode.code.funcParaView)

#define COMM_NO_R_FC_0  GetCodeIndex(funcCode.code.userPassword)

#define COMM_NO_RW_FC_0 GetCodeIndex(funcCode.code.userPasswordReadOnly)
#define COMM_READ_CURRENT_FC GetCodeIndex(funcCode.group.u0[4])

const Uint16 COMM_ERR_INDEX[8] = {10, 8, 2, 7, 6, 9, 12, 13};

LOCALD void CommRcvDataDeal(void);
LOCALD void CommSendDataDeal(void);
LOCALD void resetLinSci(void);
LOCALD void closeRTX(void);
LOCALD void setRxConfig(void);
LOCALD void setTxConfig(void);
LOCALD void commErrorDeal(void);
LOCALD void commStatusDeal(void);
LOCALD void CommDataReRcv(Uint16 tmp);
LOCALD void CommDataSend(void);
LOCALD Uint16 CommRead(Uint16, Uint16);
LOCALD Uint16 CommRwFuncCode(Uint16, Uint16, Uint16 rwMode);
LOCALD void inline CommStartSend(void);

#define COMM_READ_FC    0
#define COMM_WRITE_FC   1

#if DSP_2803X
void SCI_RXD_isr(void);
void SCI_TXD_isr(void);
interrupt void Lina_Level0_ISR(void);
interrupt void Lina_Level1_ISR(void);
#else
interrupt void SCI_RXD_isr(void);
interrupt void SCI_TXD_isr(void);
#endif

#if DSP_2803X
#define RTS (GpioDataRegs.GPBDAT.bit.GPIO39)
#else
#define RTS (GpioDataRegs.GPADAT.bit.GPIO27)
#endif
#define RS485_R     0
#define RS485_T     1

#define COMM_SET_VALUE_ADDR     0x1000
#define COMM_CMD1_ADDR          0x2000
#define COMM_STATUS_ADDR        0x3000
#define COMM_DO_ADDR            0x2001
#define COMM_HDO_ADDR           0x2004
#define COMM_AO1_ADDR           0x2002
#define COMM_AO2_ADDR           0x2003
#define COMM_INV_ERROR          0x8000

#define COMM_KEYBORD_TEST       0xFFFF

#define PROTOCOL_NUM    2
const protocolDeal protocolFunc[PROTOCOL_NUM] =
{

    { ModbusRcvDataDeal,       ModbusStartDeal,
      UpdateModbusCommFormat,  ModbusSendDataDeal,
      ModbusCommErrCheck,
    },

    { ProfibusRcvDataDeal,     ProfibusStartDeal,
      UpdateProfibusCommFormat,  ProfibusSendDataDeal,
      ProfibusCommErrCheck,
    }
};

void SciDeal(void)
{
    Uint16 commErrFlag;

    UpdateSciFormat();

    if (commTicker >= (Uint32)30 * TIME_UNIT_MS_PER_SEC * 2)
    {
        userPwdPass4Comm = 0;
        companyPwdPass4Comm = 0;
        groupHidePassComm = 0;
    }

    commErrFlag = protocolFunc[commType].CommErrCheck();

    if (commErrFlag)
    {

        commErrorDeal();
    }
    else
    {

        commStatusDeal();
    }
}

#if DSP_2803X
void SCI_RXD_isr(void)
#else
interrupt void SCI_RXD_isr(void)
#endif
{
    Uint16 tmp;

#if DSP_2803X
    tmp = LinaRegs.SCIRD;
#else
    tmp = SciaRegs.SCIRXBUF.all;
#endif

    if (protocolFunc[commType].StartDeal(tmp))
    {

		if (commRcvData.rcvFlag)
		{

	        CommDataReRcv(tmp);
		}
    }

    commTicker = 0;
    PieCtrlRegs.PIEACK.bit.ACK9 = 1;
}

#if DSP_2803X
void SCI_TXD_isr(void)
#else
interrupt void SCI_TXD_isr(void)
#endif
{

    CommDataSend();
    commTicker = 0;
    PieCtrlRegs.PIEACK.bit.ACK9 = 1;
}

#if DSP_2803X

interrupt void Lina_Level0_ISR(void)
{
	Uint32 LinL0IntVect;

	LinL0IntVect = LinaRegs.SCIINTVECT0.all;

	if(LinL0IntVect == 11)
	{
		SCI_RXD_isr();
	}

	else if(LinL0IntVect == 12)
	{
		SCI_TXD_isr();
	}

    else
    {
        PieCtrlRegs.PIEACK.bit.ACK9 = 1;
    }
}

interrupt void Lina_Level1_ISR(void)
{
	PieCtrlRegs.PIEACK.bit.ACK9 = 1;
}
#endif

LOCALD void CommRcvDataDeal(void)
{
    Uint16 writeErr;

    protocolFunc[commType].RcvDataDeal();

    sciFlag.all = 0;

    if (!commRcvData.slaveAddr)
    {

        if ((SCI_CMD_WRITE == commRcvData.commCmd)
          || (SCI_CMD_WRITE_RAM == commRcvData.commCmd)
           )
        {

            sciFlag.bit.write = 1;
        }
    }
    else
    {

        if (commRcvData.crcRcv != CrcValueByteCalc(rcvFrame, commRcvData.crcSize))
        {
            sciFlag.bit.crcChkErr = 1;
			commRcvData.rcvCrcErrCounter++;
        }
        else if (SCI_CMD_READ == commRcvData.commCmd)
        {
            sciFlag.bit.read = 1;
        }
        else if ((SCI_CMD_WRITE == commRcvData.commCmd) || (SCI_CMD_WRITE_RAM == commRcvData.commCmd))
        {
            sciFlag.bit.write = 1;
        }
        else
        {
            sciFlag.bit.cmdErr = 1;
        }
    }

    if (sciFlag.bit.write)
    {

#if 0

        if (SCI_CMD_WRITE == commRcvData.commCmd)
        {
            commRcvData.commCmdSaveEeprom = SCI_WRITE_WITH_EEPROM;
        }
#endif
        writeErr = CommWrite(commRcvData.commAddr, commRcvData.commData);

        if (writeErr)
        {

            sciFlag.all |= (0x0001 << COMM_ERR_INDEX[writeErr - 1]);
        }
    }
}

Uint16 SciErrCheck(void)
{
    Uint16 readErr;
	Uint16 operateErr;

	operateErr = COMM_ERR_NONE;

    if (sciFlag.bit.read)
    {
        if(commRcvData.commData > RTU_READ_DATA_NUM_MAX)
        {
            sciFlag.bit.paraOver = 1;
        }
        else
        {
            readErr = CommRead(commRcvData.commAddr, commRcvData.commData);
            if (readErr)
            {

                sciFlag.all |= (0x0001 << COMM_ERR_INDEX[readErr - 1]);
            }
        }
    }

    if (sciFlag.bit.pwdErr)
    {
        operateErr = COMM_ERR_PWD;
    }
    else if (sciFlag.bit.cmdErr)
    {
        operateErr = COMM_ERR_CMD;
    }
    else if (sciFlag.bit.crcChkErr)
    {
        operateErr = COMM_ERR_CRC;
    }
    else if (sciFlag.bit.addrOver)
    {
        operateErr = COMM_ERR_ADDR;
    }
    else if (sciFlag.bit.paraOver)
    {
        operateErr = COMM_ERR_PARA;
    }
    else if (sciFlag.bit.paraReadOnly)
    {
        operateErr = COMM_ERR_READ_ONLY;
    }
    else if (sciFlag.bit.systemLocked)
    {
        operateErr = COMM_ERR_SYSTEM_LOCKED;
    }
#if 1
    else if (sciFlag.bit.saveFunccodeBusy)
    {
        operateErr = COMM_ERR_SAVE_FUNCCODE_BUSY;
    }
#endif

	return operateErr;
}

LOCALF void CommSendDataDeal(void)
{
    int16 error;
    Uint16 crcSend;

    error = SciErrCheck();

    protocolFunc[commType].SendDataDeal(error);

    crcSend = CrcValueByteCalc(sendFrame, commSendData.sendNumMax - 2);
    sendFrame[commSendData.sendNumMax - 2] = crcSend & 0x00ff;
    sendFrame[commSendData.sendNumMax - 1] = crcSend >> 8;
}

Uint16 CommWrite(Uint16 addr, Uint16 data)
{
    Uint16 errType = COMM_ERR_NONE;
    Uint16 high = (addr & 0xF000);
    static  Uint16 canOpen = 0;

    if (COMM_KEYBORD_TEST == addr)
    {
        if (data == 1)
        {

            keyBordTestFlag = 1;
            keyBordValue = 0;
        }
    }

    else if (COMM_SET_VALUE_ADDR == addr)
    {
        if ((-10000 <= (int16)data) && ((int16)data <= 10000))
        {
            funcCode.code.frqComm = data;
        }
        else
        {
            errType = COMM_ERR_PARA;
        }
    }

    else if (COMM_P2P_COMM_ADDRESS_DATA == addr)
    {
        if ((-10000 <= (int16)data) && ((int16)data <= 10000))
        {
            p2pData.P2PRevData = data;
        }
        else
        {
            errType = COMM_ERR_PARA;
        }
    }

    else if (COMM_CMD1_ADDR == addr)
    {
        if (data == 8)
        {
            if (canOpen == 0)
            {
                commRunCmd = 1;
                canOpen = 1;
            }
            else
            {
                commRunCmd = 2;
                canOpen = 0;
            }
        }else if ((1 <= data) && (data <= 7))
        {
            commRunCmd = data;
        }
        else
        {
            commRunCmd = 0;
            errType = COMM_ERR_PARA;
        }
    }

    else if (COMM_DO_ADDR == addr)
    {
        doComm.all = data;
    }

    else if (COMM_HDO_ADDR == addr)
    {
        if (data <= 0x7FFF)
        {
            aoComm[0] = data;
        }
        else
        {
            errType = COMM_ERR_PARA;
        }
    }

    else if (COMM_AO1_ADDR == addr)
    {
        if (data <= 0x7FFF)
        {
            aoComm[1] = data;
        }
        else
        {
            errType = COMM_ERR_PARA;
        }
    }

    else if (COMM_AO2_ADDR == addr)
    {
        if (data <= 0x7FFF)
        {
            aoComm[2] = data;
        }
        else
        {
            errType = COMM_ERR_PARA;
        }
    }

    else if ((high == 0x0000)
             || (high == 0xF000)
             || (high == 0xA000)
             || (high == 0x4000)
             || (high == 0xB000)
             || (high == 0x5000)
             || (high == 0xC000)
             || (high == 0x6000)
             || ((addr & 0xFF00) == 0x1F00)
             || ((addr & 0xFF00) == 0x7300)
        )
    {
        errType = CommRwFuncCode(addr, data, COMM_WRITE_FC);
    }

    else
    {
        errType = COMM_ERR_ADDR;
    }

    return errType;
}

LOCALD Uint16 CommRead(Uint16 addr, Uint16 data)
{
    int16 i;
    Uint16 high = (addr & 0xF000);
    Uint16 low = (addr & 0x00FF);
    Uint16 errType = COMM_ERR_NONE;

    if ((addr & 0xFF00) == 0x1000)
    {
        if (low + data > COMM_PARA_NUM)
        {
            errType = COMM_ERR_ADDR;
        }
        else
        {
            for (i = 0; i < data; i++)
            {
                commReadData[i] = funcCode.group.u0[commDispIndex[i + low]];

                if ((i + low) == DISP_OUT_CURRENT)
                {

                    if (funcCode.code.commReadCurrentPoint)
                    {
                        commReadData[i] = commReadData[i] / 10;
                    }
                }
            }
        }
    }

    else if (COMM_STATUS_ADDR == addr)
    {
        if (data > 0x01)
        {
            errType = COMM_ERR_PARA;
        }
        else if (runFlag.bit.run)
        {
            if (FORWARD_DIR == runFlag.bit.dir)
            {
                commReadData[0] = 0x0001;
            }
            else
            {
                commReadData[0] = 0x0002;
            }
        }
        else
        {
            commReadData[0] = 0x0003;
        }
    }

    else if (COMM_INV_ERROR == addr)
    {
        if (data > 0x01)
        {
            errType = COMM_ERR_PARA;
        }
        else
        {
            commReadData[0] = errorCode;
        }
    }

    else if (COMM_KEYBORD_TEST == addr)
    {
        if (data > 0x01)
        {
            errType = COMM_ERR_PARA;
        }
        else
        {
            if (keyBordValue == 0x01FF)
            {
                commReadData[0] = 1;
            }
            else
            {
                commReadData[0] = 0;
            }
        }
    }

    else if ((high == 0xF000) ||
             (high == 0xA000) ||
             (high == 0xB000) ||
             (high == 0xC000) ||
             (high == 0x7000) ||
             ((addr & 0xFF00) == 0x1F00)
        )
    {
        errType = CommRwFuncCode(addr, data, COMM_READ_FC);
    }

    else
    {
        errType = COMM_ERR_PARA;
    }

    return errType;
}

LOCALD Uint16 CommRwFuncCode(Uint16 addr, Uint16 data, Uint16 rwMode)
{
    Uint16 index;
    Uint16 group;
    Uint16 grade;
    Uint16 gradeAdd;
    int16 i;
    Uint16 tmp;
    Uint16 highH;
    Uint16 funcCodeGradeComm[FUNCCODE_GROUP_NUM];
    Uint16 errType = COMM_ERR_NONE;

    highH = (addr & 0xF000);

    group = (addr >> 8) & 0x0F;
    grade = addr & 0xFF;

#if 0
    if ((0x0000 == highH) || (0xF000 == highH))
    {
        ;
    }
    else
#endif
    if ((0xA000 == highH) || (0x4000 == highH))
    {
        group += FUNCCODE_GROUP_A0;
    }
    else if ((0xB000 == highH) || (0x5000 == highH))
    {
        group += FUNCCODE_GROUP_B0;
    }
    else if ((0xC000 == highH) || (0x6000 == highH))
    {
        group += FUNCCODE_GROUP_C0;
    }
    else if ((addr & 0xFF00) == 0x1F00)
    {
        group = FUNCCODE_GROUP_FP;
    }
    else if (0x7000 == highH)
    {
        group += FUNCCODE_GROUP_U0;
    }

    UpdataFuncCodeGrade(funcCodeGradeComm);

    if (COMM_READ_FC == rwMode)
    {
        gradeAdd = data - 1;
    }
    else
    {
        gradeAdd = 0;
    }

    if (grade + gradeAdd >= funcCodeGradeComm[group])
    {
        errType = COMM_ERR_ADDR;
        return errType;
    }

    index = GetGradeIndex(group, grade);

    if ((COMM_NO_RW_FC_0 == index) ||

        ((COMM_WRITE_FC == rwMode) &&
         ((COMM_NO_W_FC_0 == index) ||
          (COMM_NO_W_FC_1 == index) ||
          (COMM_NO_W_FC_2 == index) ||
          (COMM_NO_W_FC_3 == index) ||
          (COMM_NO_W_FC_4 == index) ||
          (COMM_NO_W_FC_5 == index)
          )
         ) ||
        ((COMM_READ_FC == rwMode) &&
         ((COMM_NO_R_FC_0 == index)
          )
         )
        )
    {
        errType = COMM_ERR_ADDR;
        return errType;
    }

    if (COMM_WRITE_FC == rwMode)
    {

        if (group == FUNCCODE_GROUP_U3)
        {

            funcCode.all[index] = data;
            return errType;
        }
        else if (GetCodeIndex(funcCode.code.factoryPassword) == index)
        {
            if (COMPANY_PASSWORD == data)
            {
                companyPwdPass4Comm = 1;
            }
            else
            {
                errType = COMM_ERR_PARA;
            }

            return errType;
        }
        else if (GetCodeIndex(funcCode.code.userPassword) == index)
        {
            if (data == funcCode.code.userPassword)
            {
                sciFlag.bit.pwdPass = 1;
                userPwdPass4Comm = 1;
            }
            else
            {
                errType = COMM_ERR_PWD;
            }

            return errType;
        }

    }

    if ((group == FC_GROUP_FACTORY) && (!companyPwdPass4Comm))
    {
        errType = COMM_ERR_SYSTEM_LOCKED;
        return errType;
    }

    if ((group == FUNCCODE_GROUP_AF) && (!groupHidePassComm))
    {
        errType = COMM_ERR_SYSTEM_LOCKED;
        return errType;
    }

    if (COMM_READ_FC == rwMode)
    {
        for (i = 0; i < data; i++, index++)
        {
            commReadData[i] = funcCode.all[index];

            if (COMM_READ_CURRENT_FC == index)
            {

                if (funcCode.code.commReadCurrentPoint)
                {
                    commReadData[i] = commReadData[i] / 10;
                }
            }
        }
        return errType;
    }

    tmp = ModifyFunccodeUpDown(index, &data, 0);
    if (COMM_ERR_PARA == tmp)
    {
        errType = COMM_ERR_PARA;
    }
    else if (COMM_ERR_READ_ONLY == tmp)
    {
        errType = COMM_ERR_READ_ONLY;
    }
    else
    {
        if (GetCodeIndex(funcCode.code.paraInitMode) == index)
        {
            if (!userPwdPass4Comm)
            {
                errType = COMM_ERR_SYSTEM_LOCKED;
            }
            else if (FUNCCODE_RW_MODE_NO_OPERATION != funcCodeRwMode)
            {
                errType = COMM_ERR_SAVE_FUNCCODE_BUSY;
            }
            else if (FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA == data)
            {
                funcCodeRwModeTmp = FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA;
            }
            else if (FUNCCODE_paraInitMode_SAVE_USER_PARA == data)
            {
                funcCodeRwModeTmp = FUNCCODE_paraInitMode_SAVE_USER_PARA;
            }
            else if (FUNCCODE_paraInitMode_RESTORE_USER_PARA == data)
            {

                if ((funcCode.code.saveUserParaFlag1 == USER_PARA_SAVE_FLAG1)
                    && (funcCode.code.saveUserParaFlag2 == USER_PARA_SAVE_FLAG2))
                {
                    funcCodeRwModeTmp = FUNCCODE_paraInitMode_RESTORE_USER_PARA;
                }
                else
                {
                    errType = COMM_ERR_PARA;
                }
            }
            else if (FUNCCODE_paraInitMode_CLEAR_RECORD == data)
            {
                ClearRecordDeal();
            }

            else if (FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA_ALL == data)
            {

            }
            return errType;
        }

        if (COMM_ERR_PARA == ModifyFunccodeEnter(index, data))
        {
            errType = COMM_ERR_PARA;
        }
        else
        {
            if ((highH >= 0xA000)
                && (commRcvData.commCmdSaveEeprom == SCI_WRITE_WITH_EEPROM)
			)
            {
                if (FUNCCODE_RW_MODE_NO_OPERATION == funcCodeRwMode)
                {
                    SaveOneFuncCode(index);
                }
                else
                {
                    errType = COMM_ERR_SAVE_FUNCCODE_BUSY;
                }
            }

        }
    }

    return errType;
}

LOCALF void inline CommStartSend(void)
{
#if DSP_2803X
    LinaRegs.SCITD = sendFrame[0];
#else
    SciaRegs.SCITXBUF = sendFrame[0];
#endif
}

#if DSP_2803X
void resetLinSci(void)
{
    LinaRegs.SCIGCR0.bit.RESET = 0;
    LinaRegs.SCIGCR0.bit.RESET = 1;
    LinaRegs.SCIGCR1.bit.SWnRST = 0;
}

void closeRTX(void)
{
    LinaRegs.SCIGCR1.bit.RXENA = 0;
    LinaRegs.SCIGCR1.bit.TXENA = 0;
}

void setRxConfig(void)
{
    LinaRegs.SCIGCR1.bit.RXENA = 1;
    LinaRegs.SCIGCR1.bit.TXENA = 0;
}

void setTxConfig(void)
{
    LinaRegs.SCIGCR1.bit.TXENA = 1;
    LinaRegs.SCIGCR1.bit.RXENA = 0;
}
#endif

void commErrorDeal(void)
{
    errorOther = ERROR_COMM;
    errorInfo = COMM_ERROR_MODBUS;
    commStatus = SCI_RECEIVE_DATA;
    RTS = RS485_R;

    #if DSP_2803X
    EALLOW;
    setRxConfig();
    EDIS;
    #else
    SciaRegs.SCICTL1.all = 0x0021;
    SciaRegs.SCICTL2.all = 0x00C2;
    #endif
}

void commStatusDeal(void)
{
    switch (commStatus)
    {
        case SCI_RECEIVE_OK:
            CommRcvDataDeal();

            if (commRcvData.slaveAddr)
            {
                CommSendDataDeal();
                commStatus = SCI_SEND_DATA_PREPARE;
            }
            else
            {
                commStatus = SCI_RECEIVE_DATA;
                break;
            }

        case SCI_SEND_DATA_PREPARE:
            if ((commTicker >= commRcvData.delay)
                && (commTicker > commRcvData.frameSpaceTime))
            {
                RTS = RS485_T;
                #if DSP_2803X
                EALLOW;
                setTxConfig();
                EDIS;
                #else
                SciaRegs.SCICTL1.all = 0x0022;
                SciaRegs.SCICTL2.all = 0x00C1;
                #endif
                commStatus = SCI_SEND_DATA;
                commSendData.sendNum = 1;
                CommStartSend();
            }
            break;

        case SCI_SEND_OK:
            #if DSP_2803X
			if (LinaRegs.SCIFLR.bit.TXEMPTY)
            #else
            if (SciaRegs.SCICTL2.bit.TXEMPTY)
            #endif
            {
                commStatus = SCI_RECEIVE_DATA;
                RTS = RS485_R;
                #if DSP_2803X
                EALLOW;
                setRxConfig();
                EDIS;
                #else
                SciaRegs.SCICTL1.all = 0x0021;
                SciaRegs.SCICTL2.all = 0x00C2;
                #endif
            }
            break;

        default:
            break;
    }
}

void CommDataReRcv(Uint16 tmp)
{
    if (commRcvData.rcvNum< commRcvData.rcvNumMax)
    {
        rcvFrame[commRcvData.rcvNum++] = tmp;
    }

    if (commRcvData.rcvNum >= commRcvData.rcvNumMax)
    {

        commRcvData.rcvDataJuageFlag = 0;

        if (2 == commRcvData.rcvFlag)
        {
            #if DSP_2803X
            EALLOW;
            closeRTX();
            EDIS;
            #else
            SciaRegs.SCICTL1.all = 0x0004;
            SciaRegs.SCICTL2.all = 0x00C0;
            #endif
        }

        commStatus = SCI_RECEIVE_OK;
        commRcvData.rcvFlag = 0;
    }
}

void CommDataSend(void)
{

    if (commSendData.sendNum< commSendData.sendNumMax)
    {
#if DSP_2803X
        LinaRegs.SCITD = sendFrame[commSendData.sendNum++];
#else
        SciaRegs.SCITXBUF = sendFrame[commSendData.sendNum++];
#endif
    }

    else if (commSendData.sendNum >= commSendData.sendNumMax)
    {

        commStatus = SCI_SEND_OK;
    }
}

void InitSetScia(void)
{

    commStatus = SCI_RECEIVE_DATA;
#if DSP_2803X
EALLOW;

	resetLinSci();

	LinaRegs.SCIGCR1.bit.SLEEP = 0;
    LinaRegs.SCIFLR.bit.TXWAKE = 0;
    LinaRegs.SCIFLR.bit.TXEMPTY = 1;
    LinaRegs.SCIFLR.bit.TXRDY = 1;

    setRxConfig();

	LinaRegs.SCIGCR1.bit.TIMINGMODE = 1;
	LinaRegs.SCIGCR1.bit.CLK_MASTER = 1;
	LinaRegs.SCIGCR1.bit.CONT = 1;
	if (funcCode.code.commProtocolSec == CAN_OPEN)
        LinaRegs.SCIGCR1.all = (LinaRegs.SCIGCR1.all&0xFFFFFFE3)^commParitys[0];
    else
        LinaRegs.SCIGCR1.all = (LinaRegs.SCIGCR1.all&0xFFFFFFE3)^commParitys[funcCode.code.commParity];
    LinaRegs.SCISETINT.bit.SETRXINT = 1;
    LinaRegs.SCISETINT.bit.SETTXINT = 1;
    LinaRegs.SCIFORMAT.bit.CHAR = 0x7;
    LinaRegs.SCIGCR1.bit.SWnRST = 1;

EDIS;
#else
    SciaRegs.SCICTL1.all = 0x0001;
    SciaRegs.SCICTL2.all = 0x00C2;
    SciaRegs.SCICCR.all = 0x0087;
    SciaRegs.SCIPRI.bit.FREE = 1;
    SciaRegs.SCICTL1.all = 0x0021;
#endif
}

void UpdateSciFormat(void)
{
    Uint16 digit[5];
    Uint16 tmp;

	if (funcCode.code.commProtocolSec == CAN_OPEN)
        GetNumberDigit1(digit, 8);
    else
        GetNumberDigit1(digit, funcCode.code.commBaudRate);

    if (funcCode.code.commProtocolSec == PROFIBUS)
    {
        commType = PROFIBUS;
		tmp = digit[commType] + 9;
    }

    else
    {
        commType = MODBUS;
		tmp = digit[commType];
    }

    if (funcCode.code.commProtocolSec == CAN_OPEN)
        GetNumberDigit1(digit, 0);
    else
        GetNumberDigit1(digit, funcCode.code.commProtocol);

    commProtocol = digit[commType];

    protocolFunc[commType].UpdateCommFormat(dspBaudRegData[tmp].baudRate);

#if DSP_2803X
	if ( LinaRegs.SCIFLR.bit.BRKDT
        || LinaRegs.SCIFLR.bit.PE
        || LinaRegs.SCIFLR.bit.OE
        || LinaRegs.SCIFLR.bit.FE)
#else
    if (SciaRegs.SCIRXST.bit.RXERROR)
#endif
    {
        InitSetScia();
    }
#if DSP_2803X

EALLOW;
    if (funcCode.code.commProtocolSec == CAN_OPEN)
        LinaRegs.SCIGCR1.all = (LinaRegs.SCIGCR1.all&0xFFFFFFE3)^commParitys[0];
    else
        LinaRegs.SCIGCR1.all = (LinaRegs.SCIGCR1.all&0xFFFFFFE3)^commParitys[funcCode.code.commParity];

    LinaRegs.BRSR.bit.SCI_LIN_PSH = dspBaudRegData[tmp].high;
    LinaRegs.BRSR.bit.SCI_LIN_PSL = dspBaudRegData[tmp].low;
    LinaRegs.BRSR.bit.M = dspBaudRegData[tmp].M;
EDIS;
#else
    if (funcCode.code.commProtocolSec == CAN_OPEN)
        SciaRegs.SCICCR.all = commParitys[0];
    else
        SciaRegs.SCICCR.all = commParitys[funcCode.code.commParity];

    SciaRegs.SCIHBAUD = dspBaudRegData[tmp].high;
    SciaRegs.SCILBAUD = dspBaudRegData[tmp].low;
    SciaRegs.SCICTL1.bit.SWRESET = 1;
#endif

}

#if 1
void InitSciaGpio(void)
{
#if DSP_2803X
    EALLOW;
    GpioCtrlRegs.GPAPUD.bit.GPIO14 = 0;
    GpioCtrlRegs.GPAPUD.bit.GPIO15 = 0;
    GpioCtrlRegs.GPAQSEL1.bit.GPIO15 = 3;
    GpioCtrlRegs.GPAMUX1.bit.GPIO14 = 2;
    GpioCtrlRegs.GPAMUX1.bit.GPIO15 = 2;
    GpioCtrlRegs.GPBPUD.bit.GPIO39 = 0;
    GpioCtrlRegs.GPBMUX1.bit.GPIO39 = 0;
    GpioCtrlRegs.GPBDIR.bit.GPIO39 = 1;
    GpioDataRegs.GPBDAT.bit.GPIO39 = RS485_R;
    EDIS;

    EALLOW;
    PieVectTable.LIN0INTA = &Lina_Level0_ISR;
    PieVectTable.LIN1INTA = &Lina_Level1_ISR;
    EDIS;
	IER |= M_INT9;
	PieCtrlRegs.PIEIER9.bit.INTx3=1;
	PieCtrlRegs.PIEIER9.bit.INTx4=1;
#else
    EALLOW;
    GpioCtrlRegs.GPAPUD.bit.GPIO28 = 0;
    GpioCtrlRegs.GPAPUD.bit.GPIO29 = 0;
    GpioCtrlRegs.GPAQSEL2.bit.GPIO28 = 3;
    GpioCtrlRegs.GPAMUX2.bit.GPIO28 = 1;
    GpioCtrlRegs.GPAMUX2.bit.GPIO29 = 1;
    GpioCtrlRegs.GPAPUD.bit.GPIO27 = 0;
    GpioCtrlRegs.GPAMUX2.bit.GPIO27 = 0;
    GpioCtrlRegs.GPADIR.bit.GPIO27 = 1;
    GpioDataRegs.GPADAT.bit.GPIO27 = RS485_R;
    EDIS;

    EALLOW;
    PieVectTable.SCIRXINTA = SCI_RXD_isr;
	PieVectTable.SCITXINTA = SCI_TXD_isr;
    EDIS;
	IER |= M_INT9;
	PieCtrlRegs.PIEIER9.bit.INTx1 = 1;
	PieCtrlRegs.PIEIER9.bit.INTx2 = 1;
 #endif

}
#endif

#else
void InitSetScia(void){}
void InitSciaGpio(void){}
void SciDeal(void){}
void UpdateSciFormat(void){}
interrupt void SCI_RXD_isr(void){}
interrupt void SCI_TXD_isr(void){}
Uint16 CommWrite(Uint16 addr, Uint16 data){}
Uint16 CommRead(Uint16 addr, Uint16 data){}
#endif
