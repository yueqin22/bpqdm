
#ifndef MOTOR_VC_INCLUDE_H
#define MOTOR_VC_INCLUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "SystemDefine.h"
#include "MotorDefine.h"
#include "SubPrgInclude.h"
#include "MotorInclude.h"

typedef struct VC_INFO_STRUCT_DEF {
	Uint 	ASRKpLow;
	Uint 	ASRTILow;
	Uint 	ASRKpHigh;
	Uint 	ASRTIHigh;
	Uint 	ASRSwitchLow;
	Uint 	ASRSwitchHigh;

    Uint    AcrImKp;
    Uint    AcrImKi;
    Uint    AcrItKp;
    Uint    AcrItKi;
    Uint    SvcMode;

    Uint    VcOverExc;
	Uint 	VCTorqueLim;
	Uint 	VCWsCoff;
	Uint 	VCSpeedFilter;
	Uint    FreqWsCalMode;
	Uint    FreqWsVolSub;
	Uint    FreqWsVolModify;
}VC_INFO_STRUCT;
typedef struct UDC_LIMIT_IT_STRUCT_DEF {
    int    UDCLimit;
    int    UDCBak;
    int    UDCBakCnt;
    int    UDCDeta;
    int    FirstOvUdcFlag;
    PID_STRUCT  UdcPid;
}UDC_LIMIT_IT_STRUCT;
typedef struct ASR_STRUCT_DEF {
	PID_STRUCT	Asr;
    Uint        AsrQpi;
    PID_STRUCT  TorqueLimitPid;
	int  	KPHigh;
	int  	KPLow;
	int  	KIHigh;
	int  	KILow;
	int  	SwitchHigh;
	int  	SwitchLow;
	int  	TorqueLimit;
}ASR_STRUCT;
typedef struct VC_CSR_PARA_DEF{
    long   ImModify;
    Uint   ExecSetMode;
    Uint   ImAcrKp;
    Uint   ImAcrKi;
    Uint   ImModefyAdd;
    Uint   ImModefySub;
    Uint   LmGain;
}VC_CSR_PARA;

typedef struct MODIFYWS_STRUCT_DEF{
	int    Faiq;
    Uint   WsMax;
    Uint   Kp;
    int    Amp;
	int    Wsout;
    int    Tmp;
	int    Delta;
	int    tc;
 	int    tmp1;
 	int 	tmp2;
 	Uint    Theta;
 	int    Ea;
 	int    Eb;
 	int    Utheta;
	int    Xztotal;
}MODIFYWS_STRUCT;
typedef struct SVC0_STRUCT_DEF {
	Uint	VoltSVCCalSignal;
	Uint	VoltSVCCalSignalB;
}SVC0_STRUCT;

extern VC_INFO_STRUCT			gVCPar;
extern MT_STRUCT_Q24            gIMTSet;
extern MT_STRUCT_Q24            gIMTSetApply;
extern MT_STRUCT				gUMTSet;

extern PID32_STRUCT     gImAcrQ24;
extern PID32_STRUCT     gItAcrQ24;

extern ALPHABETA_STRUCT         gABVoltSet;

extern void CalcABVolt(void);

extern void ResetParForVC(void);
extern void CalIMSet(void);
extern void VCSpeedControl(void);
extern void PrepImCsrPara(void);
extern void PrepPmsmCsrPrar(void);
extern void PrepareCsrPara(void);
extern void VCCsrControl(void);

#ifdef __cplusplus
}
#endif

#endif
