#ifndef __F_MAIN_H__
#define __F_MAIN_H__

#include "f_funcCode.h"
#include "f_error.h"
#include "f_common.h"
#include "f_canlink.h"

#define ABS_INT16(a) (((a) >= 0) ? (a) : (-(a)))
#define ABS_INT32(a) (((a) >= 0L) ? (a) : (-(a)))
#define ABS_INT64(a) (((a) >= (int64)0) ? (a) : (-(a)))

#define  GetMax( x, y ) ( ((x) > (y)) ? (x) : (y) )
#define  GetMin( x, y ) ( ((x) < (y)) ? (x) : (y) )

#define DoNothing()

#define  GetTime() 	(CpuTimer1.RegsAddr->TIM.all)

#define MAX_UINT32  0xFFFFFFFF
#define MAX_UINT16  0xFFFF

#define OpenDrive()     (GpioDataRegs.GPACLEAR.bit.GPIO6 = 1)
#define CloseDrive()    (GpioDataRegs.GPASET  .bit.GPIO6 = 1)

#define Nop()   asm(" nop")

#define FUNC_DEAL_PERIOD        2
#define RUN_CTRL_PERIOD         2

#define TORQUE_CTRL_PERIOD      2
#define COMM_DEAL_PERIOD        2
#define ERROR_DEAL_PERIOD       2
#define FRQ_SRC_PERIOD          2
#define PULSE_IN_CALC_PERIOD    2
#define AI_CALC_PERIOD          2
#define DI_CALC_PERIOD          2
#define DO_CALC_PERIOD          2
#define PID_CALC_PERIOD         2
#define SERVO_CALC_PERIOD       RUN_CTRL_PERIOD
#define VF_CALC_PERIOD          2
#define CORE_FUNC_PERIOD        2
#define DISP_SCAN_PERIOD        2
#define RUN_TIME_CAL_PERIOD     2

enum POWER_ON_STATUS
{
    POWER_ON_WAIT,
    POWER_ON_CORE_OK,
    POWER_ON_FUNC_WAIT_OT
};
extern enum POWER_ON_STATUS powerOnStatus;

extern Uint16 limitedByOtherCodeIndex[];
extern Uint16 limitedByOtherCodeIndexNum;

#define FRQ_REF_I   0
#define FRQ_FDB_I   1
#define PC_REF_I    2
#define PC_FDB_I    3
extern Uint16 driveCoeff[2][4];

#define DECIMAL     0
#define HEX         1
Uint16 GetNumberDigit(Uint16 digit[5], Uint16 number, Uint16 mode);
void GetNumberDigit1(Uint16 digit[5], Uint16 number);
void GetNumberDigit2(Uint16 digit[5], Uint16 number);

extern struct MOTOR_FC motorFc;
extern enum MOTOR_SN motorSn;
extern Uint16 polePair;

extern Uint16 torqueCurrent;

extern Uint16 memoryValue;

#define AIAO_CHK_CHECK    0
#define AIAO_CHK_START    1
#define AIAO_CHK_END      2
#define AIAO_CHK_WORD    0xA5
#define AIAO_CHK_READ_NULL   0
#define AIAO_CHK_READ_OK     1
extern Uint16 aiaoChckReadOK;

void InitForFunctionApp(void);

void Main0msFunction(void);
void Main05msFunction(void);
void Main2msFunction(void);

#endif
