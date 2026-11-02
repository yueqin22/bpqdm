
#ifndef MOTOR_PUBLIC_CAL_INCLUDE_H
#define  MOTOR_PUBLIC_CAL_INCLUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "SystemDefine.h"
#include "MotorInclude.h"
#include "MotorDefine.h"

typedef struct SHORT_GND_STRUCT_DEF {
	Uint 	Comper;
	Uint 	BaseUDC;
	Uint	ocFlag;
	int		ShortCur;
}SHORT_GND_STRUCT;
typedef struct BRAKE_CONTROL_STRUCT_DEF{
	Uint	Flag;

	Uint	Cnt;
	Uint	OnCnt;
	Uint	OffCnt;
	Uint    VoltageLim;
}BRAKE_CONTROL_STRUCT;
typedef struct DC_BRAKE_STRUCT_DEF{
	int			Time;
	PID_STRUCT	PID;
}DC_BRAKE_STRUCT;
typedef struct JUDGE_POWER_LOW_DEF {
	Uint 	PowerUpFlag;
	Uint 	WaiteTime;
	Uint 	UDCOld;
}JUDGE_POWER_LOW;

extern DC_BRAKE_STRUCT			gDCBrake;
extern BRAKE_CONTROL_STRUCT	    gBrake;
extern SHORT_GND_STRUCT		    gShortGnd;
extern JUDGE_POWER_LOW			gLowPower;

extern void CBCLimitCurPrepare(void);
extern void HardWareOverUDCDeal(void);
extern void BrakeResControl(void);
extern void RunCaseDcBrake(void);
extern void RunCaseLowPower(void);

extern void RunCaseShortGnd(void);
extern void ChangeCurrent(void);
extern void ChangeMotorPar(void);
extern void SystemParChg2Ms();
extern void RunStateParChg2Ms();
extern void SystemParChg05Ms();
extern void RunStateParChg05Ms();
extern void InvCalcPower();

#ifdef __cplusplus
}
#endif

#endif
