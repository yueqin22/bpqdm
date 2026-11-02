#ifndef __F_COMM_H__
#define __F_COMM_H__

#include "f_funcCode.h"
#include "f_main.h"
#include "f_menu.h"
#include "f_runSrc.h"
#include "f_eeprom.h"
#include "f_frqSrc.h"

#define SCI_CMD_READ                0x03
#define SCI_CMD_WRITE               0x06
#define SCI_CMD_WRITE_RAM           0x07

#define SCI_WRITE_NO_EEPROM         0
#define SCI_WRITE_WITH_EEPROM       1

#define MODBUS                        0
#define PROFIBUS                      1
#define CAN_OPEN                      2
#define CANLINK                       3

#define RTU_READ_DATA_NUM_MAX     12

#define COMM_ERR_NONE               0
#define COMM_ERR_PWD                1
#define COMM_ERR_CMD                2
#define COMM_ERR_CRC                3
#define COMM_ERR_ADDR               4
#define COMM_ERR_PARA               5
#define COMM_ERR_READ_ONLY          6
#define COMM_ERR_SYSTEM_LOCKED      7
#define COMM_ERR_SAVE_FUNCCODE_BUSY 8

typedef struct
{
    Uint16 baudRate;

    Uint16 high;
    Uint16 low;
#if DSP_2803X
    Uint16 M;
#endif
} DSP_BAUD_REGISTER_DATA;

struct SCI_FLAG_BITS
{
    Uint16 read:1;
    Uint16 write:1;
    Uint16 crcChkErr:1;
    Uint16 rcvDataEnd:1;

    Uint16 send:1;
    Uint16 sendDataStart:1;
    Uint16 paraOver:1;
    Uint16 addrOver:1;

    Uint16 cmdErr:1;
    Uint16 paraReadOnly:1;
    Uint16 pwdErr:1;
    Uint16 pwdPass:1;

    Uint16 systemLocked:1;
    Uint16 saveFunccodeBusy:1;
    Uint16 rsvd:2;
};

union SCI_FLAG
{
    Uint16               all;
    struct SCI_FLAG_BITS bit;
};

struct DO_SCI_BITS
{
    Uint16 do1:1;
    Uint16 do2:1;
    Uint16 relay1:1;
    Uint16 relay2:1;
    Uint16 fmr:1;

    Uint16 vdo1:1;
    Uint16 vdo2:1;
    Uint16 vdo3:1;
    Uint16 vdo4:1;
    Uint16 vdo5:1;

    Uint16 rsvd:6;
};

union DO_SCI
{
    Uint16 all;
    struct DO_SCI_BITS bit;
};

#define SCI_RUN_CMD_NONE        0
#define SCI_RUN_CMD_FWD_RUN     1
#define SCI_RUN_CMD_REV_RUN     2
#define SCI_RUN_CMD_FWD_JOG     3
#define SCI_RUN_CMD_REV_JOG     4
#define SCI_RUN_CMD_FREE_STOP   5
#define SCI_RUN_CMD_STOP        6
#define SCI_RUN_CMD_RESET_ERROR 7

extern Uint16 commRunCmd;

enum COMM_STATUS
{
    SCI_RECEIVE_DATA,
    SCI_RECEIVE_OK,
    SCI_SEND_DATA_PREPARE,
    SCI_SEND_DATA,
    SCI_SEND_OK
};

extern enum COMM_STATUS commStatus;
extern Uint32 commTicker;
extern union DO_SCI doComm;
extern Uint16 aoComm[];
extern Uint16 sendFrame[];
extern Uint16 rcvFrame[];
extern Uint16 commReadData[RTU_READ_DATA_NUM_MAX];
extern union SCI_FLAG sciFlag;
extern Uint16 commProtocol;
void InitSetScia(void);
void UpdateSciFormat(void);
void SciDeal(void);
Uint16 CommRead(Uint16 addr, Uint16 data);
Uint16 CommWrite(Uint16, Uint16);

typedef struct
{
	Uint16 slaveAddr;
    Uint16 commAddr;
    Uint16 commData;
    Uint16 rcvNum;
    Uint16 rcvFlag;
    Uint16 crcRcv;
    Uint16 commCmd;
    Uint16 crcSize;
    Uint16 rcvNumMax;
    Uint16 frameSpaceTime;
    Uint16 delay;
    Uint16 rcvCrcErrCounter;
    Uint16 rcvDataJuageFlag;
    Uint16 commCmdSaveEeprom;
} COMM_RCV_DATA;
extern COMM_RCV_DATA commRcvData;

typedef struct
{
    Uint16 sendNumMax;
    Uint16 sendNum;
    Uint16 commData;
    Uint16 crcRcv;
    Uint16 commCmd;
    Uint16 crcSize;
} COMM_SEND_DATA;
extern COMM_SEND_DATA commSendData;

void ProfibusRcvDataDeal(void);
void ModbusRcvDataDeal(void);
Uint16 ModbusStartDeal(Uint16 tmp);
Uint16 ProfibusStartDeal(Uint16 tmp);
void UpdateModbusCommFormat(Uint16 baudRate);
void UpdateProfibusCommFormat(Uint16 baudRate);
void ModbusSendDataDeal(Uint16 err);
void ProfibusSendDataDeal(Uint16 err);
Uint16 ModbusCommErrCheck(void);
Uint16 ProfibusCommErrCheck(void);

typedef struct CommProtocolDeal
{
    void (*RcvDataDeal)();
    Uint16 (*StartDeal)();
    void (*UpdateCommFormat)();
    void (*SendDataDeal)();
    Uint16 (*CommErrCheck)();
}protocolDeal;
#endif
