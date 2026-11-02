
#ifndef MOTOR_PARA_ID_INCLUDE_H
#define MOTOR_PARA_ID_INCLUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "SystemDefine.h"
#include "MotorInclude.h"
#include "MotorDefine.h"

#define TUNE_STEP_NOW (gGetParVarable.ParEstContent[gGetParVarable.ParEstMstep])

typedef enum TUNE_FLOW_ENUM_DEF{
    TUNE_NULL = 0,
    TUNE_IM_STATIC,
    TUNE_IM_ROTOR,
    TUNE_PM_COMP_LOAD = 11,
    TUNE_PM_COMP_NO_LOAD,
    TUNE_PM_PARA_temp,
    TUNE_INERTIA = 20
}TUNE_FLOW_ENUM;

typedef struct UV_AMP_COFF_STRUCT_DEF {
	Ulong	TotalU;
	Ulong	TotalV;
	Ulong	TotalVoltL;
	Ulong	TotalIL;
	Ulong	TotalVolt;
	Ulong	TotalI;
	Uint	Number;
	Uint	Comper;
	Uint	ComperL;
	Uint 	UDivVGet;
	Uint 	UDivV;
	Uint 	UDivVSave;
    Uint    IdRsCnt;
    Uint    IdRsDelay;
    Uint    IdRsBak;
    Uint    Rs_PRD;
}UV_BIAS_COFF_STRUCT;

typedef enum PAR_EST_MAIN_STEP_ENUM{
    IDENTIFY_RS,
    IDENTIFY_RR_LO,
    IDENTIFY_LM_IO,

    PM_EST_POLSE_POS,
    PM_EST_NO_LOAD,
    PM_EST_WITH_LOAD,
    PM_EST_BEMF,

    IDENTIFY_END
}PAR_EST_MAIN_STEP;

typedef enum TUNE_TO_FUNCTION_WORLD_ENUM_DEF{
    TUNE_INITIAL,
    TUNE_ACC = 50,
    TUNE_DEC = 51,
    TUNE_SUCCESS = 100,
    TUNE_FINISH = 1000
}TUNE_TO_FUNCTION_WORLD_ENUM;

typedef struct MOTOR_PARAMETER_IDENTIFY_STRUCT{
    PAR_EST_MAIN_STEP    ParEstContent[IDENTIFY_PROGRESS_LENGTH];
    Uint16                              ParEstMstep;
    Uint16                              IdSubStep;
    TUNE_TO_FUNCTION_WORLD_ENUM         StatusWord;
    TUNE_FLOW_ENUM                      TuneType;

    int  QtEstDelay;
}MOTOR_PARA_EST;

typedef struct IDENTIFY_RRLO_VARIABLE_STRUCT{
int     WaitCnt;
int     IsSampleValue[20];
int     CurrentMax;
int     CurrentRatio;
int     RrL07PulseOverSig;
int     RrL0CycleCnt;
int     SampleTimes;
Uint    PwmCompareValue;
int     UdcVoltage;
long    RrAndRsMax;
long    RrAndRsMin;
long    LoMax;
long    LoMin;
long    RrAndRsAccValue;
long    LoAccValue;
} IDENTIFY_RRLO_VARIABLE;

typedef struct IDENTIFY_LMIO_VARIABLE_STRUCT{
int     WaitCnt;
int     DestinationFreq;
int     DataSavedNum;
Uint    VFOvShock;
long    lIsValue;
long    LmAccValue;
long    IoAccValue;
long    lImAccValue;
long    lIsAccValue;
}IDENTIFY_LMIO_VARIABLE;

extern UV_BIAS_COFF_STRUCT		 gUVCoff;
extern MOTOR_PARA_EST            gGetParVarable;
extern MOTOR_EXTERN_STRUCT		 gMotorExtReg;
extern IDENTIFY_RRLO_VARIABLE    gRrLoIdentify;
extern IDENTIFY_LMIO_VARIABLE    gLmIoIdentify;

extern void RunCaseGetPar(void);
extern void PrepareParForTune(void);
extern void ChgParForEst(void);
extern void LmIoInPeriodInt(void);

#ifdef __cplusplus
}
#endif
#endif
