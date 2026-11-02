
#include "MotorInclude.h"

INV_STRUCT 				    gInvInfo;
MOTOR_STRUCT 			    gMotorInfo;
MOTOR_EXTERN_STRUCT		    gMotorExtInfo;
MOTOR_EXTERN_STRUCT		    gMotorExtPer;
RUN_STATUS_STRUCT 		    gMainStatus;
BASE_COMMAND_STRUCT		    gMainCmd;
MAIN_COMMAND_EXTEND_UNION   gExtendCmd;
SUB_COMMAND_UNION           gSubCommand;

CONTROL_MOTOR_TYPE_ENUM     gCtrMotorType;
MOTOR_POWER_TORQUE          gPowerTrq;

BASE_PAR_STRUCT			    gBasePar;
COM_PAR_INFO_STRUCT		    gComPar;

CPU_TIME_STRUCT			    gCpuTime;

MOTOR_DEBUG_DATA_RECEIVE_STRUCT     gTestDataReceive;
MOTOR_DEBUG_DATA_DISPLAY_STRUCT     gTestDataDisplay;
