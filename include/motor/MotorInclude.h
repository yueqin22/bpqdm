
#ifndef MOTOR_INCLUDE_H
#define MOTOR_INCLUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "MotorCurrentTransform.h"
#include "SubPrgInclude.h"
#include "MotorVFInclude.h"
#include "MotorVCInclude.h"
#include "MotorSvcInclude.h"
#include "MotorInvProtectInclude.h"
#include "MotorPwmInclude.h"
#include "MotorStructDefine.h"
#include "MotorInfoCollectInclude.h"
#include "MotorParaIDinclude.h"
#include "MotorPublicCalInclude.h"
#include "MotorSpeedCheceInclude.h"
#include "MotorDataExchange.h"
#include "MotorPmsmMain.h"
#include "MotorEncoder.h"
#include "MotorPmsmParEst.h"

extern INV_STRUCT 				gInvInfo;
extern CONTROL_MOTOR_TYPE_ENUM  gCtrMotorType;
extern MOTOR_STRUCT 			gMotorInfo;
extern MOTOR_EXTERN_STRUCT		gMotorExtInfo;
extern MOTOR_EXTERN_STRUCT		gMotorExtPer;
extern RUN_STATUS_STRUCT 		gMainStatus;
extern BASE_COMMAND_STRUCT		gMainCmd;
extern MAIN_COMMAND_EXTEND_UNION gExtendCmd;
extern SUB_COMMAND_UNION         gSubCommand;

extern BASE_PAR_STRUCT			gBasePar;
extern COM_PAR_INFO_STRUCT		gComPar;
extern MOTOR_POWER_TORQUE       gPowerTrq;

extern CPU_TIME_STRUCT			 gCpuTime;
extern MOTOR_DEBUG_DATA_RECEIVE_STRUCT     gTestDataReceive;
extern MOTOR_DEBUG_DATA_DISPLAY_STRUCT     gTestDataDisplay;

extern Uint const gDeadBandTable[];
extern Uint const gDeadCompTable[];
extern Uint const gInvCurrentTable220S[];

extern Uint const gInvTypeTable380To220T[16];
extern Uint const gInvCurrentTable380T[];
extern Uint const gInvCurrentTable690T[];
extern Uint const gInvCurrentTable1140T[];
extern Uint const gInvCurrentTable220T[];
extern Uint  const gInvVoltageInfo220S[];
extern Uint  const gInvVoltageInfo220T[];
extern Uint  const gInvVoltageInfo380T[];
extern Uint  const gInvVoltageInfo480T[];
extern Uint  const gInvVoltageInfo690T[];
extern Uint  const gInvVoltageInfo1140T[];

extern int * pVD1;
extern int * pVD2;
extern int * pVD3;
extern int * pVD4;
extern int * pVD5;
extern int * pVD6;
extern int   startSave;

extern void SaveDebugData16(Uint);
extern void SaveDebugData32(unsigned long);
extern void ResetDebugBuffer(void);

extern interrupt void ADC_Over_isr(void);
extern interrupt void EPWM1_TZ_isr(void);
extern interrupt void EPWM1_zero_isr(void);
extern interrupt void PG_Zero_isr(void);

extern void InitSysCtrl(void);
extern void InitInterrupt(void);
extern void InitPeripherals(void);
extern void InitForMotorApp(void);
extern void InitForFunctionApp(void);
extern void SetInterruptEnable(void);
extern void EnableDog(void);
extern void DisableDog(void);
extern void KickDog(void);
void InitSetAdc(void);

#ifdef __cplusplus
}
#endif

#endif
