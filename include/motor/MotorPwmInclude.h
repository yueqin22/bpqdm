
#ifndef MOTOR_PWM_INCLUDE_H
#define MOTOR_PWM_INCLUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "SystemDefine.h"
#include "MotorInclude.h"
#include "MotorDefine.h"

typedef struct FC_CAL_STRUCT_DEF{
	Uint	Cnt;
	Uint	Time;
	Uint	FcBak;
    Uint    FcLimitOvLoad;
}FC_CAL_STRUCT;
typedef struct ANGLE_STRUCT_DEF {
	long 	StepPhase;
	long 	StepPhaseApply;
	long 	IMPhase;
	int 	OutPhase;
	int  	CompPhase;
	int  	RotorPhase;
}ANGLE_STRUCT;
typedef struct OUT_VOLT_STRUCT_DEF {
	int  	Volt;
	int     MaxOutVolt;
	int     vfSplit;
	int   	VoltPhase;

	int  	VoltApply;
	int     VoltDisplay;
	int   	VoltPhaseApply;
	Uint	VoltSVCCalSignal;
	Uint	VoltSVCCalSignalB;

    Uint    antiVolt;
    int16   detVfVoltUp;
}OUT_VOLT_STRUCT;
typedef enum ZERO_LENGTH_PHASE_SELECT_ENUM_DEF{
    ZERO_VECTOR_U,
    ZERO_VECTOR_V,
    ZERO_VECTOR_W,
    ZERO_VECTOR_NONE=100
}ZERO_LENGTH_PHASE_SELECT_ENUM;
typedef struct PWM_OUT_STRUCT_DEF {
	long	U;
	long	V;
	long	W;
    ZERO_LENGTH_PHASE_SELECT_ENUM  gZeroLengthPhase;

	Uint	gPWMPrd;
	Uint	gPWMPrdApply;

	Uint	AsynModle;
	Uint	PWMModle;
	Uint    SoftPWMTune;
	int    	SoftPWMCoff;

    int     PwmModeSwitchHF;
    int     PwmModeSwitchLF;
}PWM_OUT_STRUCT;

typedef struct DEAD_BAND_STRUCT_DEF{
   int		DeadBand;
   Uint     DeadTimeSet;
   int		Comp;
   Uint     CompCoff;
   int     	CompU;
   int     	CompV;
   int     	CompW;
   int		MTPhase;
   long     InvCurFilter;
}DEAD_BAND_STRUCT;
typedef struct SYN_PWM_STRUCT_DEF {
	Ulong   FcApply;
	Uint	ModuleApply;
	Uint 	Index;
	Uint	AbsFreq;
	Uint	Flag;
}SYN_PWM_STRUCT;

extern FC_CAL_STRUCT			gFcCal;
extern ANGLE_STRUCT			    gPhase;
extern OUT_VOLT_STRUCT			gOutVolt;
extern Uint					    gRatio;
extern PWM_OUT_STRUCT			gPWM;
extern SYN_PWM_STRUCT			gSynPWM;
extern DEAD_BAND_STRUCT		    gDeadBand;

void DeadBandComp(void);
void SendPWM(void);
void CalCarrierWaveFreq(void);
void SoftPWMProcess(void);
void CalOutputPhase(void);
void CalDeadBandComp(void);
void InitSetPWM(void);
void OutPutPWMVC(void);
void OutPutPWMVF(void);

void ImSvcDeadBandComp(void);
void HVfDeadBandComp(void);
void ImFvcDeadBandComp(void);
void PmFvcDeadBandComp(void);

#ifdef __cplusplus
}
#endif

#endif
