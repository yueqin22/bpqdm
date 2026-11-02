
#ifndef __F_ERROR_H__
#define __F_ERROR_H__

#include "f_funcCode.h"

#define ERROR_NONE                      0
#define ERROR_INVERTER_UNIT             1
#define ERROR_OC_ACC_SPEED              2
#define ERROR_OC_DEC_SPEED              3
#define ERROR_OC_CONST_SPEED            4
#define ERROR_OV_ACC_SPEED              5
#define ERROR_OV_DEC_SPEED              6
#define ERROR_OV_CONST_SPEED            7
#define ERROR_BUFFER_RES                8
#define ERROR_UV                        9
#define ERROR_OL_INVERTER               10
#define ERROR_OL_MOTOR                  11
#define ERROR_LOSE_PHASE_INPUT          12
#define ERROR_LOSE_PHASE_OUTPUT         13
#define ERROR_OT_IGBT                   14
#define ERROR_EXTERNAL                  15
#define ERROR_COMM                      16
#define ERROR_CONTACTOR                 17
#define ERROR_CURRENT_SAMPLE            18
#define ERROR_TUNE                      19
#define ERROR_ENCODER                   20
#define ERROR_EEPROM                    21
#define ERROR_22                        22
#define ERROR_MOTOR_SHORT_TO_GND        23
#define ERROR_24                        24
#define ERROR_25                        25
#define ERROR_RUN_TIME_OVER             26

#define ERROR_USER_1                    27
#define ERROR_USER_2                    28
#define ERROR_POWER_UP_TIME_OVER        29
#define ERROR_LOSE_LOAD                 30
#define ERROR_FDB_LOSE                  31
#define ERROR_CBC                       40
#define ERROR_SWITCH_MOTOR_WHEN_RUN     41
#define ERROR_DEV                       42
#define ERROR_OS                        43
#define ERROR_MOTOR_OT                  45
#define ERROR_INIT_POSITION             51

#define ERROR_UVW_FDB                   53

#define ERROR_RSVD                      9999

#define PLC_DEFINE_ERROR_START          80
#define PLC_DEFINE_ERROR_END            89

#define ERROR_LEVEL_NO_ERROR    0
#define ERROR_LEVEL_FREE_STOP   1
#define ERROR_LEVEL_STOP        2
#define ERROR_LEVEL_RUN         3

#define COMM_ERROR_MODBUS     1
#define COMM_ERROR_CANLINK    2
#define COMM_ERROR_CANOPEN    3
#define COMM_ERROR_PROFIBUS   4
#define COMM_ERROR_P2P        5
#define COMM_ERROR_PLC        6

struct ERROR_ATTRIBUTE_BITS
{
    Uint16 reset:1;
    Uint16 level:3;
};
union ERROR_ATTRIBUTE
{
    Uint16 all;
    struct ERROR_ATTRIBUTE_BITS bit;
};

struct CUR_TIME
{
    Uint16 runTimeM;
    Uint16 runTimeSec;

    Uint16 powerOnTimeM;
    Uint16 powerOnTimeSec;

    Uint16 runTime;
    Uint16 powerOnTime;

};

struct ERROR_ATTRIBUTE_LEVEL_BITS
{
    Uint16 Err1:3;
    Uint16 Err2:3;
    Uint16 Err3:3;
    Uint16 Err4:3;
    Uint16 Err5:3;
};
union ERROR_ATTRIBUTE_LEVEL
{
    Uint16 all;
    struct ERROR_ATTRIBUTE_LEVEL_BITS bit;
};

extern union ERROR_ATTRIBUTE errorAttribute;

extern Uint16 motorErrInfor[5];

extern struct CUR_TIME curTime;

extern Uint16 errorCode;
extern Uint16 errorCodeOld;
extern Uint16 errAutoRstNum;
extern Uint16 bUv;
extern Uint16 errorOther;
extern Uint16 errorsCodeFromMotor[2];
extern Uint16 errorInfo;

enum ERROR_DEAL_STATUS
{
    ERROR_DEAL_PREPARE_FOR_WRITE_EEPROM,
    ERROR_DEAL_WAIT_FOR_WRITE_EEPROM,
    ERROR_DEAL_WRITE_EEPROM_OK,
    ERROR_DEAL_OK
};
extern enum ERROR_DEAL_STATUS errorDealStatus;

#define ERROR_DEALING (dspSubCmd.bit.errorDealing)

void ErrorDeal(void);
void ErrorReset(void);
Uint16 GetErrorAttribute(Uint16 errCode);

#endif
