
#ifndef MAIN_INCLUDE_H
#define MAIN_INCLUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "SystemDefine.h"
#include "MotorDefine.h"

typedef	long long 				llong;
typedef	unsigned int			Uint;
typedef	unsigned long			Ulong;
typedef	unsigned long long 		Ullong;

typedef struct BIT32_REG_DEF {
   Uint16  LSW;
   Uint16  MSW;
}BIT32_REG;

typedef union BIT32_GROUP_DEF {
   Uint32     all;
   BIT32_REG  half;
}BIT32_GROUP;

#define  C_CMD_WRITE	0
#define  C_CMD_READ		1
#define  C_CMD_DEBUG	2
#define  C_COM_OVER_TIME	   100

#define  GetTime() 	(CpuTimer1.RegsAddr->TIM.all)

#define MOTOR_TYPE_IM			0
#define MOTOR_TYPE_VARFREQ      1
#define MOTOR_TYPE_PM			2
#define MOTOR_NONE              100

#define IDC_SVC_CTL				0
#define IDC_FVC_CTL				1
#define IDC_VF_CTL				2

#define INV_VOLTAGE_220V        0
#define INV_VOLTAGE_380V        1
#define INV_VOLTAGE_480V        2
#define INV_VOLTAGE_690V        3
#define INV_VOLTAGE_1140V       5

#define STATUS_LOW_POWER		1
#define STATUS_GET_PAR			2
#define STATUS_STOP				3
#define STATUS_SPEED_CHECK		4
#define STATUS_RUN				5
#define STATUS_SHORT_GND		6
#define STATUS_IPM_INIT_POS		7

#define C_SPEED_FLAG_CON 		0
#define C_SPEED_FLAG_ACC 		1
#define C_SPEED_FLAG_DEC 		2

#define speed_ACC (gMainCmd.Command.bit.SpeedFlag == C_SPEED_FLAG_ACC)
#define speed_DEC (gMainCmd.Command.bit.SpeedFlag == C_SPEED_FLAG_DEC)
#define speed_CON (gMainCmd.Command.bit.SpeedFlag == C_SPEED_FLAG_CON)

#define  MODLE_CPWM				0
#define  MODLE_DPWM				1

#define  MODLE_SYN				0
#define  MODLE_ASYN				1

#define  CANCEL_DB_COMP_NO      0
#define  DEADBAND_COMP_280      1
#define  DEADBAND_COMP_380      2

#define  IDENTIFY_PROGRESS_LENGTH 5

#ifdef	DSP_CLOCK100
	#define C_INIT_PRD			10000
	#define C_MAX_DB			320
	#define SHORT_GND_PERIOD 	12500
    #define TUNE_Rs_PRD         25000
    #define SHORT_GND_CMPR_INC	100
	#define SHORT_GND_PERIOD_1140 	50000
    #define SHORT_GND_PERIOD_690    33000
#else
	#define C_INIT_PRD			6000
	#define C_MAX_DB			192
	#define SHORT_GND_PERIOD 	7500
    #define TUNE_Rs_PRD         15000
	#define SHORT_GND_CMPR_INC	60
    #define SHORT_GND_PERIOD_1140 	30000
    #define SHORT_GND_PERIOD_690    20000
#endif
#define C_MAX_LONG_VALUE		(0x7FFFFFFF)
#define C_MAX_PER               (1073741824L)

#define  COM_PHASE_DEADTIME		600

#define TurnOnFan()    		gMainStatus.StatusWord.bit.FanControl = 1
#define TurnOffFan()     	    gMainStatus.StatusWord.bit.FanControl = 0

#define TurnOnBrake()    		GpioDataRegs.GPACLEAR.bit.GPIO8 = 1
#define TurnOffBrake()     	GpioDataRegs.GPASET.bit.GPIO8 = 1

#ifdef __cplusplus
}
#endif

#endif
