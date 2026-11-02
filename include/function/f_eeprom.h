#ifndef __F_EEPROM_H__
#define __F_EEPROM_H__

#include "f_funcCode.h"

#define SaveOneFuncCode(index)  (funcCodeOneWriteIndex[funcCodeOneWriteNum++] = (index))

#define I2C_SLAVE_ADDR          0x50

#if DSP_2803X
#define EEPROM_PAGE_BYTE        4
#elif 1
#define EEPROM_PAGE_BYTE        16
#endif

#define FUNC_CODE_BYTE          2
#define EEPROM_PAGE_NUM_FUNC_CODE (EEPROM_PAGE_BYTE/FUNC_CODE_BYTE)

#define FUNCCODE_paraInitMode_NO_OPERATION              0
#define FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA      1
#define FUNCCODE_paraInitMode_CLEAR_RECORD              2
#define FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA_ALL  3
#define FUNCCODE_paraInitMode_SAVE_USER_PARA            4
#define FUNCCODE_paraInitMode_RESTORE_USER_PARA         501

#define FUNCCODE_RW_MODE_WRITE_ALL                  10
#define FUNCCODE_RW_MODE_WRITE_SERIES               11
#define FUNCCODE_RW_MODE_WRITE_ONE                  12
#define FUNCCODE_RW_MODE_READ_ALL                   13
#define FUNCCODE_RW_MODE_READ_SERIES                14
#define FUNCCODE_RW_MODE_READ_ONE                   15
#define FUNCCODE_RW_MODE_NO_OPERATION   FUNCCODE_paraInitMode_NO_OPERATION

#if DSP_2803X
#define I2C_MSG_BUFFER_SIZE 4
#elif 1
#define I2C_MSG_BUFFER_SIZE 16
#endif
enum I2C_MSG_STATUS
{
    I2C_MSG_STATUS_IDLE,
    I2C_MSG_STATUS_WRITE_BUSY,
    I2C_MSG_STATUS_SEND_NOSTOP_BUSY,
    I2C_MSG_STATUS_RESTART,
    I2C_MSG_STATUS_READ_BUSY,
    I2C_MSG_STATUS_RW_OK
};

struct I2C_MSG
{
  enum I2C_MSG_STATUS status;
  Uint16 bytes;
  Uint16 highAddr;
  Uint16 lowAddr;
  Uint16 buffer[I2C_MSG_BUFFER_SIZE];

};

typedef struct
{
    Uint16 index;
    Uint16 number;

    Uint16 data[EEPROM_PAGE_NUM_FUNC_CODE];
} FUNCCODE_RW;
extern FUNCCODE_RW funcCodeRead;

struct EEPROM_OPERATE_TIME
{

    Uint16 readFlag;
    Uint16 readTicker;

    Uint16 writeFlag;
    Uint16 writeTicker;

    Uint16 rwFlag;
    Uint16 rwTicker;
};
extern struct EEPROM_OPERATE_TIME eepromOperateTime;

#if F_DEBUG_RAM
#define FUNCCODE_DEBUG              5555
#define FUNCCODE_RW_STATUS_DEBUG    5555
#endif

#define FUNCCODE_READ_RET_READING           0
#define FUNCCODE_READ_RET_OK                1

#define FUNCCODE_WRITE_RET_WRITING          0
#define FUNCCODE_WRITE_RET_OK               1

#define FUNCCODE_RW_RET_PARA_ERROR          4
#define FUNCCODE_RW_RET_EEPROM_ERROR        5

extern Uint16 funcCodeOneWriteIndex[];
extern Uint16 funcCodeOneWriteNum;
extern Uint16 rwFuncCodeRepeatDelayTicker;
extern Uint16 funcCodeRwMode;
extern Uint16 funcCodeRwModeTmp;

extern Uint16 startIndexWriteSeries;
extern Uint16 endIndexWriteSeries;

#define ERROR_EEPROM_NONE               0
#define ERROR_EEPROM_WRITE              1
#define ERROR_EEPROM_READ               2
#define ERROR_EEPROM_RW_OVER_TIME       3
#define ERROR_EEPROM_WRITE_NUM_OVER     4
extern Uint16 errorEeprom;

Uint16 ReadFuncCode(FUNCCODE_RW *funcCodeRw);
Uint16 WriteFuncCode(FUNCCODE_RW *funcCodeRw);
void EepromDeal(void);
void InitSetI2ca(void);
void EepromOperateTimeDeal(void);

Uint16 GetEepromIndexFromFcIndex(Uint16 index);
Uint16 GetFcIndexFromEepromIndex(Uint16 a);

#endif
