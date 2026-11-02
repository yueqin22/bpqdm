#include "f_main.h"
#include "f_menu.h"
#include "f_eeprom.h"

#define DEBUG_F_TABLE_FC2EEPROM_CONST    1

#if DEBUG_F_TABLE_FC2EEPROM_CONST
#include "f_table_eeprom2Fc.c"
#endif

#define EEPROM_24LC08    0
#define EEPROM_24LC16    1
#define EEPROM_24LC32    2

#define EEPROM_TYPE      EEPROM_24LC32

#define PARA_INIT_MODE_SAVE_ALL_CODE     1

#define DEBUG_F_EEPROM                   1

#define FUNCCODE_READ_VERIFY_RIGHT_TIME_MAX             3
#define FUNCCODE_READ_VERIFY_WRONG_TIME_MAX             4
#define FUNCCODE_READ_AFTER_WRITE_VERIFY_RIGHT_TIME_MAX 2
#define FUNCCODE_READ_AFTER_WRITE_VERIFY_WRONG_TIME_MAX 3
#define FUNCCODE_READ_TIME_MAX                          5

#define FUNCCODE_WRITE_STATUS_WRITE                  0
#define FUNCCODE_WRITE_STATUS_WRITE_REMAINDER         1
#define FUNCCODE_WRITE_STATUS_READ_TO_VERIFY_WRITE   2

#define FUNCCODE_RW_STATUS_PREPARE_DATA_FOR_EAD    0
#define FUNCCODE_RW_STATUS_READ_CURRENT_DATA       1
#define FUNCCODE_RW_STATUS_PREPARE_DATA_FOR_WRITE  2
#define FUNCCODE_RW_STATUS_WRITE_DATA              3

#define FUNC_CODE_RW_STATUS_INIT       FUNCCODE_RW_STATUS_PREPARE_DATA_FOR_EAD

#define FUNCCODE_EEPROM_START_INDEX   16
#define USER_PARA_START_INDEX         (2048 + FUNCCODE_EEPROM_START_INDEX)

#define FUNCCODE_ONE_WRITE_NUMBER_MAX  200

#define FUNCCODE_RW_DELAY_TIME_MAX     100

#define EEPROM_READ_SINGLE_MAX         250
#define EEPROM_WRITE_SINGLE_MAX        500
#define EEPROM_RW_FUNCCODE_MAX         5000

#define I2C_JOB_RETRY_MAX              100

#define RW_I2C_BUS_WRITE    0
#define RW_I2C_BUS_READ     1
#define RW_I2C_BUS_ACK      2

#define RW_EEPROM_WRITE     0
#define RW_EEPROM_READ      1

Uint16 funcCodeOneWriteNum;
Uint16 funcCodeOneWriteIndex[FUNCCODE_ONE_WRITE_NUMBER_MAX];
Uint16 funcCodeRwMode;
Uint16 funcCodeRwModeTmp;
LOCALF struct I2C_MSG I2cMsg;
Uint16 startIndexWriteSeries;
Uint16 endIndexWriteSeries;
Uint16 endIndexRwFuncCode;
LOCALF Uint16 funcCodeRwStatus;
FUNCCODE_RW funcCodeRead;
LOCALF FUNCCODE_RW funcCodeWrite;
Uint16 rwFuncCodeRepeatDelayTicker;
LOCALF Uint16 index4EepromDeal;
LOCALF Uint16 number4EepromDeal;
struct EEPROM_OPERATE_TIME eepromOperateTime;

LOCALD Uint16 RwI2cBus(Uint16 mode);
LOCALD Uint16 RwEeprom(Uint16 mode);
LOCALD void I2cIntDeal(void);
LOCALD void UpdateIndexForEepromDeal(void);
LOCALD void I2cAddrHighLow(Uint16 index);

void I2cDealBeforeInit(void);
void I2CNoACK(void);
void I2CStop(void);
void I2CRcvByte(void);

LOCALF Uint16 RwI2cBus(Uint16 mode)
{
    int16 i;

    if (I2caRegs.I2CMDR.bit.STP == 1)
    {
        return I2C_STP_NOT_READY_ERROR;
    }

    if ((I2caRegs.I2CSTR.bit.BB == 1) && (I2cMsg.status != I2C_MSG_STATUS_RESTART))
    {
        return I2C_BUS_BUSY_ERROR;
    }

#if (EEPROM_TYPE == EEPROM_24LC32)
    I2caRegs.I2CSAR = I2C_SLAVE_ADDR;
#else
    I2caRegs.I2CSAR = (I2C_SLAVE_ADDR | (I2cMsg.highAddr >> 8)) & 0xff;
#endif

    if (mode == RW_I2C_BUS_WRITE)
    {

#if (EEPROM_TYPE == EEPROM_24LC32)
        I2caRegs.I2CCNT = I2cMsg.bytes + 2;
#else
        I2caRegs.I2CCNT = I2cMsg.bytes + 1;
#endif

        I2caRegs.I2CDXR = I2cMsg.highAddr;
#if (EEPROM_TYPE == EEPROM_24LC32)
        I2caRegs.I2CDXR = I2cMsg.lowAddr;
#endif
        for (i = 0; i < I2cMsg.bytes; i++)
        {
            I2caRegs.I2CDXR = I2cMsg.buffer[i];
        }

        I2caRegs.I2CMDR.all = 0x6E20;
    }
    else if ((mode == RW_I2C_BUS_READ) && (I2C_MSG_STATUS_RESTART == I2cMsg.status))
    {
        I2caRegs.I2CCNT = I2cMsg.bytes;

        I2caRegs.I2CMDR.all = 0x6C20;

    }
    else
    {
#if (EEPROM_TYPE == EEPROM_24LC32)
        I2caRegs.I2CCNT = 2;
        I2caRegs.I2CDXR = I2cMsg.highAddr;
        I2caRegs.I2CDXR = I2cMsg.lowAddr;
#else
        I2caRegs.I2CCNT = 1;
        I2caRegs.I2CDXR = I2cMsg.highAddr;
#endif

        I2caRegs.I2CMDR.all = 0x6620;

    }

    return I2C_SUCCESS;
}

LOCALF void I2cIntDeal(void)
{
    Uint16 IntSource;

    IntSource = I2caRegs.I2CISRC.all;

    if (IntSource == I2C_NO_ISRC)
        return;

    if (IntSource == I2C_SCD_ISRC)
    {
        if (I2cMsg.status == I2C_MSG_STATUS_WRITE_BUSY)
        {
            Uint16 tmp;

            for (tmp = 0; tmp < I2C_JOB_RETRY_MAX; tmp++)
            {
                if (RwI2cBus(RW_I2C_BUS_ACK) == I2C_SUCCESS)
                    break;
            }
        }

        else if (I2cMsg.status == I2C_MSG_STATUS_SEND_NOSTOP_BUSY)
        {
            I2cMsg.status = I2C_MSG_STATUS_IDLE;
        }

        else if (I2cMsg.status == I2C_MSG_STATUS_READ_BUSY)
        {
            int16 i;

            for (i = 0; i < I2cMsg.bytes; i++)
            {
                I2cMsg.buffer[i] = I2caRegs.I2CDRR;
            }

            I2cMsg.status = I2C_MSG_STATUS_RW_OK;
        }
    }

    else if (IntSource == I2C_ARDY_ISRC)
    {
        if (I2caRegs.I2CSTR.bit.NACK == 1)
        {
            I2caRegs.I2CMDR.bit.STP = 1;
            I2caRegs.I2CSTR.all = I2C_CLR_NACK_BIT;
        }
        else if (I2cMsg.status == I2C_MSG_STATUS_SEND_NOSTOP_BUSY)
        {
            I2cMsg.status = I2C_MSG_STATUS_RESTART;
        }
        else if (I2cMsg.status == I2C_MSG_STATUS_WRITE_BUSY)
        {
            I2cMsg.status = I2C_MSG_STATUS_RW_OK;

            InitSetI2ca();

        }
    }
}

LOCALF Uint16 RwEeprom(Uint16 mode)
{
    Uint16 tmp = 0;

    if (I2C_MSG_STATUS_IDLE == I2cMsg.status)
    {
        if (RW_EEPROM_WRITE == mode)
        {
            if (RwI2cBus(RW_I2C_BUS_WRITE) == I2C_SUCCESS)
            {
                I2cMsg.status = I2C_MSG_STATUS_WRITE_BUSY;
            }
        }
        else
        {
            for (tmp = 0; tmp < I2C_JOB_RETRY_MAX; tmp++)
            {
                if (RwI2cBus(RW_I2C_BUS_READ) == I2C_SUCCESS)
                    break;
            }
            if (tmp >= I2C_JOB_RETRY_MAX)
                return 0;

            I2cMsg.status = I2C_MSG_STATUS_SEND_NOSTOP_BUSY;
        }
        return 0;
    }
    else if (I2C_MSG_STATUS_RESTART == I2cMsg.status)
    {
            for (tmp = 0; tmp < I2C_JOB_RETRY_MAX; tmp++)
            {
                if (RwI2cBus(RW_I2C_BUS_READ) == I2C_SUCCESS)
                    break;
            }
            if (tmp >= I2C_JOB_RETRY_MAX)
                return 0;

        I2cMsg.status = I2C_MSG_STATUS_READ_BUSY;
        return 0;
    }
    else if (I2C_MSG_STATUS_RW_OK == I2cMsg.status)
    {
        I2cMsg.status = I2C_MSG_STATUS_IDLE;
        return 1;
    }
    else
        return 0;
}

Uint16 ReadFuncCode(FUNCCODE_RW *funcCodeRw)
{
    static Uint16 readTime;
    static Uint16 readRightTime;
    static Uint16 bVerifyWrong;
    Uint16 dataRead[EEPROM_PAGE_NUM_FUNC_CODE];
    int16 readRightTimeMax;
    int16 i;
    Uint16 tmp;

    eepromOperateTime.readFlag = 1;

    if ((0 == funcCodeRw->number) || (funcCodeRw->number > EEPROM_PAGE_NUM_FUNC_CODE))
        return FUNCCODE_RW_RET_PARA_ERROR;

    if (bVerifyWrong && (rwFuncCodeRepeatDelayTicker < FUNCCODE_RW_DELAY_TIME_MAX))
        return FUNCCODE_READ_RET_READING;

    rwFuncCodeRepeatDelayTicker = 0;
    bVerifyWrong = 0;

#if (EEPROM_TYPE == EEPROM_24LC32)
    tmp = (funcCodeRw->index + FUNCCODE_EEPROM_START_INDEX) << 1;
    I2cMsg.highAddr = tmp >> 8;
    I2cMsg.lowAddr = tmp & 0x00FF;
#else
    I2cMsg.highAddr = (funcCodeRw->index + FUNCCODE_EEPROM_START_INDEX) << 1;
#endif

    I2cMsg.bytes = funcCodeRw->number << 1;
    if (1 == RwEeprom(RW_EEPROM_READ))
    {
        readTime++;

        for (i = funcCodeRw->number - 1; i >= 0 ; i--)
        {
            dataRead[i] = (I2cMsg.buffer[i<<1] << 8) + I2cMsg.buffer[(i<<1)+1];
            if (1 == readTime)
            {
                funcCodeRw->data[i] = dataRead[i];
            }
            else
            {
                if (funcCodeRw->data[i] != dataRead[i])
                    break;
            }
        }

        if (1 == readTime)
        {
            readRightTime = 1;
            return FUNCCODE_READ_RET_READING;
        }

        if (i < 0)
        {
#if 0
            if (!repeatTime)
                readRightTimeMax = FUNCCODE_READ_VERIFY_RIGHT_TIME_MAX;
            else
                readRightTimeMax = FUNCCODE_READ_VERIFY_WRONG_TIME_MAX;

            if (FUNCCODE_RW_STATUS_WRITE_DATA == funcCodeRwStatus)
                readRightTimeMax--;
#else
            readRightTimeMax = FUNCCODE_READ_VERIFY_RIGHT_TIME_MAX;
            if (FUNCCODE_RW_STATUS_WRITE_DATA == funcCodeRwStatus)
                readRightTimeMax = FUNCCODE_READ_AFTER_WRITE_VERIFY_RIGHT_TIME_MAX;
#endif

            if (++readRightTime >= readRightTimeMax)
            {
                readTime = 0;
                eepromOperateTime.readFlag = 0;
                eepromOperateTime.readTicker = 0;

                return FUNCCODE_READ_RET_OK;
            }
        }
        else
        {
            readTime = 0;
            bVerifyWrong = 1;

            return FUNCCODE_READ_RET_READING;
        }
    }

    return FUNCCODE_READ_RET_READING;
}

Uint16 WriteFuncCode(FUNCCODE_RW *funcCodeRw)
{
    static Uint16 writeStatus;
    static Uint16 bVerifyWrong;
    static Uint16 number;
    int16 i;
    Uint16 tmp;

    eepromOperateTime.writeFlag = 1;

#if DSP_2803X
    if ((funcCodeRw->index + funcCodeRw->number > (funcCodeRw->index & 0xFFFE) + 0x0002)
#else
    if ((funcCodeRw->index + funcCodeRw->number > (funcCodeRw->index & 0xFFF8) + 0x0008)
#endif
        || (0 == funcCodeRw->number) || (funcCodeRw->number > EEPROM_PAGE_NUM_FUNC_CODE))
    {
        return FUNCCODE_RW_RET_PARA_ERROR;
    }

    if (bVerifyWrong && (rwFuncCodeRepeatDelayTicker < FUNCCODE_RW_DELAY_TIME_MAX))
        return FUNCCODE_WRITE_RET_WRITING;

    rwFuncCodeRepeatDelayTicker = 0;
    bVerifyWrong = 0;

    if (FUNCCODE_WRITE_STATUS_WRITE == writeStatus)
    {
        if (EEPROM_PAGE_NUM_FUNC_CODE == funcCodeRw->number)
            number = EEPROM_PAGE_NUM_FUNC_CODE - 1;
        else
            number = funcCodeRw->number;

        for (i = number - 1; i >= 0; i--)
        {
            I2cMsg.buffer[i << 1] = funcCodeRw->data[i] >> 8;
            I2cMsg.buffer[(i << 1) + 1] = funcCodeRw->data[i] & 0x00ff;
        }

#if (EEPROM_TYPE == EEPROM_24LC32)
        tmp = (funcCodeRw->index + FUNCCODE_EEPROM_START_INDEX) << 1;
        I2cMsg.highAddr = tmp >> 8;
        I2cMsg.lowAddr = tmp & 0x00FF;
#else
        I2cMsg.highAddr = (funcCodeRw->index + FUNCCODE_EEPROM_START_INDEX) << 1;
#endif
        I2cMsg.bytes = number << 1;

        if (1 == RwEeprom(RW_EEPROM_WRITE))
        {
            if (number == funcCodeRw->number)
                writeStatus = FUNCCODE_WRITE_STATUS_READ_TO_VERIFY_WRITE;
            else
                writeStatus = FUNCCODE_WRITE_STATUS_WRITE_REMAINDER;
        }
        else
            return FUNCCODE_WRITE_RET_WRITING;
    }

    if (FUNCCODE_WRITE_STATUS_WRITE_REMAINDER == writeStatus)
    {
        I2cMsg.buffer[0] = funcCodeRw->data[EEPROM_PAGE_NUM_FUNC_CODE - 1] >> 8;
        I2cMsg.buffer[1] = funcCodeRw->data[EEPROM_PAGE_NUM_FUNC_CODE - 1] & 0x00ff;

#if (EEPROM_TYPE == EEPROM_24LC32)
        tmp = (funcCodeRw->index + FUNCCODE_EEPROM_START_INDEX + EEPROM_PAGE_NUM_FUNC_CODE - 1) << 1;
        I2cMsg.highAddr = tmp >> 8;
        I2cMsg.lowAddr = tmp & 0x00FF;
#else
        I2cMsg.highAddr = (funcCodeRw->index + FUNCCODE_EEPROM_START_INDEX + EEPROM_PAGE_NUM_FUNC_CODE - 1) << 1;
#endif
        I2cMsg.bytes = 1 << 1;

        if (1 == RwEeprom(RW_EEPROM_WRITE))
            writeStatus = FUNCCODE_WRITE_STATUS_READ_TO_VERIFY_WRITE;
        else
            return FUNCCODE_WRITE_RET_WRITING;
    }

    if (FUNCCODE_WRITE_STATUS_READ_TO_VERIFY_WRITE == writeStatus)
    {
        funcCodeRead.index = funcCodeRw->index;
        funcCodeRead.number = funcCodeRw->number;

        if (FUNCCODE_READ_RET_OK == ReadFuncCode(&funcCodeRead))
        {
            for (i = funcCodeRw->number - 1; i >= 0; i--)
            {
                if (funcCodeRead.data[i] != funcCodeRw->data[i])
                    break;
            }

            if (i < 0)
            {
                eepromOperateTime.writeFlag = 0;
                eepromOperateTime.writeTicker = 0;
                writeStatus = FUNCCODE_WRITE_STATUS_WRITE;

                return FUNCCODE_WRITE_RET_OK;
            }
            else
            {
                bVerifyWrong = 1;
                writeStatus = FUNCCODE_WRITE_STATUS_WRITE;

                return FUNCCODE_WRITE_RET_WRITING;
            }
        }
        else
            return FUNCCODE_WRITE_RET_WRITING;
    }

    return FUNCCODE_WRITE_RET_WRITING;
}

void EepromDeal(void)
{
    int16 i;
    Uint16 fcIndex;

#if F_DEBUG_RAM

    if (((FNUM_EEPROM + 0 + FUNCCODE_EEPROM_START_INDEX) << 1) > 4096)
    {

        errorEeprom = FNUM_EEPROM + 1 + FUNCCODE_EEPROM_START_INDEX;
        errorCode = 01;
    }
#endif

    if (funcCodeOneWriteNum >= (FUNCCODE_ONE_WRITE_NUMBER_MAX >> 1))
    {
        funcCodeOneWriteNum = FUNCCODE_ONE_WRITE_NUMBER_MAX >> 1;

        if (POWER_ON_WAIT != powerOnStatus)
        {
            errorEeprom = ERROR_EEPROM_WRITE_NUM_OVER;
        }
    }

#if 0
    if (errorEeprom)
        return;
#endif

    I2cIntDeal();

    if (FUNCCODE_RW_MODE_NO_OPERATION == funcCodeRwMode)
    {

        if (funcCodeRwModeTmp)
        {
            funcCodeRwMode = funcCodeRwModeTmp;
            if (FUNCCODE_RW_MODE_READ_ALL == funcCodeRwMode)
            {
                index4EepromDeal = 0;
            }
            else
            {
                index4EepromDeal = EEPROM_INDEX_USE_INDEX;
            }
            endIndexRwFuncCode = EEPROM_INDEX_USE_LENGTH;
            if (FUNCCODE_RW_MODE_WRITE_SERIES == funcCodeRwMode)
            {

                index4EepromDeal = GetEepromIndexFromFcIndex(startIndexWriteSeries);

                endIndexRwFuncCode = GetEepromIndexFromFcIndex(endIndexWriteSeries) + 1;
            }
        }
        else if (funcCodeOneWriteNum)
        {
            funcCodeRwMode = FUNCCODE_RW_MODE_WRITE_ONE;

            funcCodeOneWriteNum--;

            index4EepromDeal = GetEepromIndexFromFcIndex(funcCodeOneWriteIndex[funcCodeOneWriteNum]);
        }
        else
        {

            return;
        }
    }

    if (FUNCCODE_RW_STATUS_PREPARE_DATA_FOR_EAD == funcCodeRwStatus)
    {
        if (FUNCCODE_RW_MODE_WRITE_ONE == funcCodeRwMode)
        {
            number4EepromDeal = 1;
        }
        else
        {
            Uint16 tmp;
#if DSP_2803X
            tmp = (index4EepromDeal & 0xFFFE) + EEPROM_PAGE_NUM_FUNC_CODE;
#else
            tmp = (index4EepromDeal & 0xFFF8) + EEPROM_PAGE_NUM_FUNC_CODE;
#endif
            if (tmp > endIndexRwFuncCode)
            {
                tmp = endIndexRwFuncCode;
            }
            number4EepromDeal = tmp - index4EepromDeal;
        }

        funcCodeRead.index = index4EepromDeal;
        funcCodeRead.number = number4EepromDeal;

        if (FUNCCODE_RW_MODE_WRITE_ALL == funcCodeRwMode)
        {
            funcCodeRwStatus = FUNCCODE_RW_STATUS_PREPARE_DATA_FOR_WRITE;
        }
        else
        {
#if PARA_INIT_MODE_SAVE_ALL_CODE
            if (FUNCCODE_paraInitMode_SAVE_USER_PARA == funcCodeRwMode)
                funcCodeRead.index = index4EepromDeal + USER_PARA_START_INDEX;
#endif

            for (i = 0; i < number4EepromDeal; i++)
            {
                fcIndex = GetFcIndexFromEepromIndex(index4EepromDeal + i);
                if (FUNCCODE_RSVD4ALL_INDEX != fcIndex)
                    break;
            }

            if (i == number4EepromDeal)
            {
                UpdateIndexForEepromDeal();
            }
            else
            {
                funcCodeRwStatus = FUNCCODE_RW_STATUS_READ_CURRENT_DATA;
            }
        }
    }

    if (FUNCCODE_RW_STATUS_READ_CURRENT_DATA == funcCodeRwStatus)
    {
        if (FUNCCODE_READ_RET_OK == ReadFuncCode(&funcCodeRead))
        {
            funcCodeRwStatus = FUNCCODE_RW_STATUS_PREPARE_DATA_FOR_WRITE;
        }
    }

    if (FUNCCODE_RW_STATUS_PREPARE_DATA_FOR_WRITE == funcCodeRwStatus)
    {
#if PARA_INIT_MODE_SAVE_ALL_CODE
        if ((FUNCCODE_paraInitMode_SAVE_USER_PARA == funcCodeRwMode)
            || (FUNCCODE_paraInitMode_RESTORE_USER_PARA == funcCodeRwMode))
        {
            if (FUNCCODE_paraInitMode_RESTORE_USER_PARA == funcCodeRwMode)
                funcCodeWrite.index = index4EepromDeal + USER_PARA_START_INDEX;
            else
                funcCodeWrite.index = index4EepromDeal;

            funcCodeWrite.number = number4EepromDeal;

            if (FUNCCODE_READ_RET_OK != ReadFuncCode(&funcCodeWrite))
            {
                return;
            }
        }
#endif

        for (i = 0; i < number4EepromDeal; i++)
        {
            fcIndex = GetFcIndexFromEepromIndex(index4EepromDeal + i);

            if (FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA == funcCodeRwMode)
            {
                funcCode.all[fcIndex] = GetFuncCodeInit(fcIndex, 0);
            }
            else if(FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA_ALL == funcCodeRwMode)
            {
                funcCode.all[fcIndex] = GetFuncCodeInit(fcIndex, 1);
            }
            else if (FUNCCODE_RW_MODE_READ_ALL == funcCodeRwMode)
            {
                funcCode.all[fcIndex] = funcCodeRead.data[i];
            }
            else if (FUNCCODE_RW_MODE_WRITE_ALL == funcCodeRwMode)
            {

                if ((((fcIndex >= AI1_CALB_START) && (fcIndex <= AI2_CALB_STOP))
                    || ((fcIndex >= AO1_CALB_START) && (fcIndex <= AO1_CALB_STOP)))
                    && (aiaoChckReadOK == AIAO_CHK_READ_OK)
					)
                {
                    ;
                }
                else if ((fcIndex == SAVE_USER_PARA_PARA1)
                    || (fcIndex == SAVE_USER_PARA_PARA2))
                {
                    ;
                }
                else
                {
                    funcCode.all[fcIndex] = GetFuncCodeInitOriginal(fcIndex);
                }

                funcCodeRwStatus = FUNCCODE_RW_STATUS_WRITE_DATA;
            }
            else if (FUNCCODE_RW_MODE_WRITE_ONE == funcCodeRwMode)
            {
                ;
            }
            else if (FUNCCODE_RW_MODE_WRITE_SERIES == funcCodeRwMode)
            {
                ;
            }
#if PARA_INIT_MODE_SAVE_ALL_CODE
            else if (FUNCCODE_paraInitMode_RESTORE_USER_PARA == funcCodeRwMode)
            {
                funcCode.all[fcIndex] = funcCodeWrite.data[i];
            }
#endif

            funcCodeWrite.data[i] = funcCode.all[fcIndex];

            if (funcCodeWrite.data[i] != funcCodeRead.data[i])
            {
                funcCodeRwStatus = FUNCCODE_RW_STATUS_WRITE_DATA;
            }
        }

        if (FUNCCODE_RW_STATUS_WRITE_DATA != funcCodeRwStatus)
        {
            UpdateIndexForEepromDeal();
        }
        else
        {
            funcCodeWrite.index = index4EepromDeal;
            funcCodeWrite.number = number4EepromDeal;

#if PARA_INIT_MODE_SAVE_ALL_CODE
            if (FUNCCODE_paraInitMode_SAVE_USER_PARA == funcCodeRwMode)
                funcCodeWrite.index = index4EepromDeal + USER_PARA_START_INDEX;
#endif
        }
    }

    if (FUNCCODE_RW_STATUS_WRITE_DATA == funcCodeRwStatus)
    {
        if (FUNCCODE_WRITE_RET_OK == WriteFuncCode(&funcCodeWrite))
        {
            UpdateIndexForEepromDeal();
        }
    }

#if (F_DEBUG_RAM && (DEBUG_F_EEPROM))

    if (FUNCCODE_RW_STATUS_DEBUG == funcCodeRwStatus)
    {
        Uint16 all = FNUM_EEPROM;

        funcCodeRwMode = FUNCCODE_DEBUG;
        funcCodeWrite.index = index4EepromDeal;
        funcCodeWrite.number = all - funcCodeWrite.index;
        if (funcCodeWrite.number > EEPROM_PAGE_NUM_FUNC_CODE)
            funcCodeWrite.number = EEPROM_PAGE_NUM_FUNC_CODE;

        for (i = funcCodeWrite.number - 1; i >= 0; i--)
            funcCodeWrite.data[i] = ((index4EepromDeal+i) << 1) + 0x0aec;
        if (FUNCCODE_WRITE_RET_OK == WriteFuncCode(&funcCodeWrite))
        {
            index4EepromDeal += funcCodeWrite.number;
            if (index4EepromDeal >= all)
                funcCodeRwMode = FUNCCODE_RW_MODE_NO_OPERATION;
        }
    }
#endif
}

LOCALF void UpdateIndexForEepromDeal(void)
{
    funcCodeRwStatus = FUNC_CODE_RW_STATUS_INIT;

    if (FUNCCODE_RW_MODE_WRITE_ONE == funcCodeRwMode)
    {
        funcCodeRwMode = FUNCCODE_RW_MODE_NO_OPERATION;
    }
    else
    {
        index4EepromDeal += number4EepromDeal;
        if (index4EepromDeal >= endIndexRwFuncCode)
        {
            if ((FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA == funcCodeRwMode) ||
                (FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA_ALL == funcCodeRwMode))
            {
                RestoreCompanyParaOtherDeal();
            }

            if (FUNCCODE_paraInitMode_SAVE_USER_PARA == funcCodeRwMode)
            {
                funcCode.code.saveUserParaFlag1 = USER_PARA_SAVE_FLAG1;
                funcCode.code.saveUserParaFlag2 = USER_PARA_SAVE_FLAG2;
            }

            funcCodeRwMode = FUNCCODE_RW_MODE_NO_OPERATION;
            funcCodeRwModeTmp = FUNCCODE_RW_MODE_NO_OPERATION;
        }
    }
}

void EepromOperateTimeDeal(void)
{
    rwFuncCodeRepeatDelayTicker++;

    if (eepromOperateTime.readFlag)
    {
        if (++eepromOperateTime.readTicker > EEPROM_READ_SINGLE_MAX)
        {
            eepromOperateTime.readTicker = 0;
            errorEeprom = ERROR_EEPROM_READ;
        }
    }

    if (eepromOperateTime.writeFlag)
    {
        if (++eepromOperateTime.writeTicker > EEPROM_WRITE_SINGLE_MAX)
        {
            eepromOperateTime.writeTicker = 0;
            errorEeprom = ERROR_EEPROM_WRITE;
        }
    }

    if (FUNCCODE_RW_MODE_NO_OPERATION != funcCodeRwMode)
    {
        if (++eepromOperateTime.rwTicker >= EEPROM_RW_FUNCCODE_MAX)
        {
            eepromOperateTime.rwTicker = 0;
            errorEeprom = ERROR_EEPROM_RW_OVER_TIME;
        }
    }
    else
    {
        eepromOperateTime.rwTicker = 0;
    }
}

void InitI2CGpio(void)
{
   EALLOW;

    GpioCtrlRegs.GPBPUD.bit.GPIO32 = 0;
    GpioCtrlRegs.GPBPUD.bit.GPIO33 = 0;

    GpioCtrlRegs.GPBQSEL1.bit.GPIO32 = 3;
    GpioCtrlRegs.GPBQSEL1.bit.GPIO33 = 3;

    GpioCtrlRegs.GPBMUX1.bit.GPIO32 = 1;
    GpioCtrlRegs.GPBMUX1.bit.GPIO33 = 1;

    EDIS;
}

void InitSetI2ca(void)
{

    I2caRegs.I2CMDR.all = 0x4000;

#if (DSP_CLOCK == 100)
    I2caRegs.I2CPSC.all = 9;
#elif (DSP_CLOCK == 60)
    I2caRegs.I2CPSC.all = 5;
#endif

#if 0
    I2caRegs.I2CCLKL = 55;
    I2caRegs.I2CCLKH = 55;
#elif 1
    I2caRegs.I2CCLKL = 25;
    I2caRegs.I2CCLKH = 25;
#elif 1
    I2caRegs.I2CCLKL = 20;
    I2caRegs.I2CCLKH = 20;
#elif 1
    I2caRegs.I2CCLKL = 10;
    I2caRegs.I2CCLKH = 10;
#elif 1
    I2caRegs.I2CCLKL = 4;
    I2caRegs.I2CCLKH = 9;
#endif

    I2caRegs.I2CIER.all = 0x0024;

    I2caRegs.I2CMDR.all = 0x4020;

    I2caRegs.I2CFFTX.all = 0x6040;
    I2caRegs.I2CFFRX.all = 0x2040;
}

#define SDA (GpioDataRegs.GPBDAT.bit.GPIO32)
#define SCL (GpioDataRegs.GPBDAT.bit.GPIO33)
#define SdaIoAsInput()  {                               \
        EALLOW;                                         \
        GpioCtrlRegs.GPBDIR.bit.GPIO32 = 0;             \
        EDIS;                                           \
    }
#define SdaIoAsOutput() {                               \
        EALLOW;                                         \
        GpioCtrlRegs.GPBDIR.bit.GPIO32 = 1;             \
        EDIS;                                           \
    }

#define I2cClkHigh()    (GpioDataRegs.GPBSET.bit.GPIO33 = 1)
#define I2cClkLow()     (GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1)

#define I2cDataHigh()   (GpioDataRegs.GPBSET.bit.GPIO32 = 1)
#define I2cDataLow()    (GpioDataRegs.GPBCLEAR.bit.GPIO32 = 1)

void I2cDealBeforeInit(void)
{
    Uint16 a = 0;

    EALLOW;
	GpioCtrlRegs.GPBPUD.bit.GPIO32 = 0;
	GpioCtrlRegs.GPBPUD.bit.GPIO33 = 0;

	GpioCtrlRegs.GPBQSEL1.bit.GPIO32 = 3;
    GpioCtrlRegs.GPBQSEL1.bit.GPIO33 = 3;

	GpioCtrlRegs.GPBMUX1.bit.GPIO32 = 0;
	GpioCtrlRegs.GPBMUX1.bit.GPIO33 = 0;

    GpioCtrlRegs.GPBDIR.bit.GPIO32 = 1;
    GpioCtrlRegs.GPBDIR.bit.GPIO33 = 1;

    GpioDataRegs.GPBDAT.bit.GPIO32 = 0;
    GpioDataRegs.GPBDAT.bit.GPIO33 = 0;
    EDIS;

    DELAY_US(20000);
    I2CStop();

    SdaIoAsOutput();
    I2cDataHigh();
    SdaIoAsInput();
    if (!SDA)
    {
        a = 1;
    }
    else
    {
        SdaIoAsOutput();
        I2cDataLow();
        SdaIoAsInput();
        if (SDA)
        {
            a = 1;
        }
    }

    if (a == 1)
    {
        I2CRcvByte();
        I2CNoACK();
        I2CStop();
    }
}

void I2CNoACK(void)
{
    DELAY_US(5);

    I2cDataHigh();
    DELAY_US(5);

    I2cClkHigh();
    DELAY_US(5);

    I2cClkLow();
    DELAY_US(5);
}

void I2CStop(void)
{
    I2cDataLow();
    DELAY_US(5);

    I2cClkHigh();
    DELAY_US(5);

    I2cDataHigh();
    DELAY_US(5);
}

void I2CRcvByte(void)
{
    int i;

    SdaIoAsInput();
    for (i = 0; i < 800; i++)
    {
        I2cClkLow();
        DELAY_US(5);
        I2cClkHigh();
        DELAY_US(5);
    }

    I2cClkLow();
    DELAY_US(5);

    SdaIoAsOutput();
    DELAY_US(5);
}

Uint16 GetFcIndexFromEepromIndex(Uint16 a)
{
    int16 i;

#if DEBUG_F_TABLE_FC2EEPROM_CONST
    i = eeprom2Fc[a];
#else
    for (i = 0; i < EEPROM_INDEX_USE_LENGTH; i++)
    {
        if (GetEepromIndexFromFcIndex(i) == a)
            break;
    }
    if (i == EEPROM_INDEX_USE_LENGTH)
    {
        i = FUNCCODE_RSVD4ALL_INDEX;
    }
#endif

    return i;
}

extern const Uint16 fcNoAttri2Eeprom[];

Uint16 GetEepromIndexFromFcIndex(Uint16 index)
{
    if (index >= GetCodeIndex(funcCode.group.fChk[0]))
    {
        return fcNoAttri2Eeprom[index - GetCodeIndex(funcCode.group.fChk[0])];
    }
    else
    {
        return funcCodeAttribute[index].eepromIndex;
    }
}
