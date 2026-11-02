
#ifndef MOTOR_SVC_INCLUDE_H
#define MOTOR_SVC_INCLUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "MotorInclude.h"

typedef struct SVC_FLUX_CAL_STRUCT_DEF {
	long 	FilterTime;
	Uint 	SampleTime;
}SVC_FLUX_CAL_STRUCT;

typedef struct FLUX_STRUCT_DEF {
	Uint	Amp;
	Uint	Theta;
}FLUX_STRUCT;
typedef struct SVC_SPEED_STRUCT_DEF {
	int		SvcSynSpeed;
	int		SvcWs;
	int		SvcRotorSpeed;

	int		SvcLastFluxPos;
	Uint	SvcSignal;
    int     DetaTimer;
	Ulong	Timer;
    int     DetaPhase;

}SVC_SPEED_STRUCT;

extern FLUX_STRUCT              gFluxR;
extern FLUX_STRUCT              gFluxS;
extern MT_STRUCT_Q24           gIMTQ24_obs;
extern MT_STRUCT_Q24           gIMTQ12_obs;

void ResetSVC(void);
void SVCCalRotorSpeed(void);
void SvcCalOutVolt(void);
void SVCCalFlux_380(void);
void SvcCalOutVolt_380(void);
void SVCCalRotorSpeed_380(void);

#ifdef __cplusplus
}
#endif
#endif
