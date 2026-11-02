
#ifndef MOTOR_VF_INCLUDE_H
#define MOTOR_VF_INCLUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "SystemDefine.h"
#include "MotorDefine.h"
#include "SubPrgInclude.h"
#include "MotorCurrentTransform.h"

typedef struct VF_INFO_STRUCT_DEF {
	Uint 	VFLineType;
	Uint 	VFTorqueUp;
	Uint 	VFTorqueUpLim;
	Uint 	VFOverExc;
	Uint 	VFWsComp;
	Uint 	VFOvShock;
	Uint 	VFLineFreq1;
	Uint 	VFLineVolt1;
	Uint 	VFLineFreq2;
	Uint 	VFLineVolt2;
	Uint 	VFLineFreq3;
	Uint 	VFLineVolt3;
	Uint 	SVCTorqueUp;
	Uint 	SVCTorqueUpLim;

    Uint    vfResComp;
    Uint    ovGain;
    Uint    ovPoint;
    Uint    ovPointCoff;
    Uint    ocGain;
    Uint    ocPoint;
    int     FreqApply;

    int     vfMode;
    int     tpLst;
}VF_INFO_STRUCT;

typedef struct  VF_AUTO_TORQUEBOOST_VAR_DEF{
    Uint        VfAutoTorqueEnable;
    int         DestinationVolt;
    int         VfCurrentIs;
    int         VfReverseAngle;
    int         VfRIsSinFai;
    int         VfRVCosFai;
    int         VfAngleSin;
    int         VfReverseVolt;
    int         VfTorqueEnableTime;
    PID_STRUCT  AutoTorquePID;
}VF_AUTO_TORQUEBOOST_VAR;

typedef struct OVER_SHOCK_STRUCT_DEF{
	int				IMFilter;
    int             IO;
	PID_STRUCT		pid;
    Uint            TimesNub;
    Uint            oscMode;
    int             ShockDecrease;

    int             OscVolt;

}OVER_SHOCK_STRUCT;

typedef struct VF_WS_COMP_STRUCT_DEF {
	Uint 	   Coff;
	long       CompFreq;
    int        DelayTime;
    int        FilterCoff;
    MT_STRUCT  WsCompMT;
    MT_STRUCT  WsCompMTApply;
}VF_WS_COMP_STRUCT;

typedef struct VAR_AVR_STRUCT_DEF{
	int		UDCFilter;
	int		CoffApply;
	int		Cnt;
}VAR_AVR_STRUCT;

#if 0
typedef struct VF_CURRENT_CONTROL_DEF{
    Uint      disVfCsr;

    Uint      ocPoint;
    Uint      currentLimit;
    Uint      active;
    Uint      vfCsrKP;
    Uint      vfCsrKI;
    Uint      vfCsrKD;

    int16       detaVolt;
    PID_STRUCT	pid;
}VF_CURRENT_CONTROL;
#endif

typedef struct THREE_ORDER_FILTER_DEF{
    long    OutK1;
    long    OutK2;
    long    OutK3;

    long    InK1;
    long    InK2;
    long    InK3;

    long    OutData1;
    long    OutData2;
    long    OutData3;

    long    InData1;
    long    Indata2;
    long    Indata3;
}THREE_ORDER_FILTER;

typedef struct VF_VAR_CALC_DEF{
    int     vfTq;
    int     vfPt;
}VF_VAR_CALC;

typedef struct VF_OVER_UDC_DAMP_DEF
{

    int16 uDcBrakePt;
    int16 vfOvUdcPt;

    PID_STRUCT pidUdc;
    PID_STRUCT pidPower;

    int16       powerFdb;
    int16       powerSet;
    Uint    maxPower;
} VF_OVER_UDC_DAMP;

typedef struct VF_FREQ_DEAL_DEF
{
    int    stepCur;
    int    stepUdc;
    int    stepSet;
    int    stepEnd;
    Uint    freqDir;
    Uint    freqSet;
    Uint    freqAim;
    Uint    freqApply;
    Uint    preSpdFlag;
    Uint    spedChg;
}VF_FREQ_DEAL;

typedef struct OVER_CURRENT_DAMP_DEF2{
	int		LowFreq;

	int		stepApply;
	int		stepLAmp;
	int		stepLAmpLim;
	int		stepHAmp;
	int		CurBak;

	int		maxStepLF;
	int		maxStepHF;
	int		ocPointQ12;
	int		subStep;
	int     addStep;
	int		Flag;

} OVER_CURRENT_DAMP2;

typedef struct OVER_CURRENT_DAMP_DEF{
	int		StepApply;
	int		StepLow;
	int		StepLowLim;
	int		StepHigh;
	int		CurBak;
	int		LowFreq;
	int		MaxStepLow;
	int		MaxStepHigh;
	int		CurLim;
	int		SubStep;
	int		Flag;

}OVER_CURRENT_DAMP;

typedef struct OVER_UDC_CTL_STRUCT_DEF{
	int		CoffApply;
	int		CoffAdd;
	int		Limit;
	int		StepApply;
    int     LastStepApply;
	int		StepBak;
	int		ExeCnt;
	int		UdcBak;
	int		Flag;
	int		FreqMax;
	int     AccTimes;
	int		OvUdcLimitTime;

}OVER_UDC_CTL_STRUCT;

typedef struct HVF_OSC_DAMP_STRUCT_DEF
{
#if 0
    int FreqSet;
    int FreqSynApply;
    int FreqSpliEst;
    int VoltOsc;
    int CompLeakageLs;
#endif

    int OscDampGain;
    int VoltSmSet;
    int VoltEmf;

    int VoltAmp;
    int VoltPhase;

#if 0

    int detVoltT;
    int detVoltM;

    int Rs;
    int Lt;
    int Lg;
    int LgPerSet;
    int CurM;
    int CurMDealy;
    int taoCurMDealy;
    int CurTDealy;
#endif

    int Rs;
    int CurMagSet;
    int VoltAmpAdjActive;

}HVF_OSC_DAMP_STRUCT ;

typedef struct HVF_OSC_JUDGE_INDEX_DEF
{
    int AnglePowerFactor;

    int wCntUse;
    int wCntRltm;
    int maxAngle;
    int minAngle;
    int oscIndex;

}HVF_OSC_JUDGE_INDEX;

typedef struct HVF_DB_COMP_OPT_DEF
{
    int     HVfDbCompOptActive;
    int     PhasPredictGain;

    int     CurPhaseFeed;
    int     CurPhaseFeed_pre;
    int     CurPhasePredict;
    int     StepPhaseSet;
    int     CurPhaseStepFed;
    int     CurPhaseStepPredict;
    int     PhaseFwdFedCoeff;

    int     DbOptActHFreq;
    int     DbOptActLFreq;

    int     DbCompCpwmWidth;
}HVF_DB_COMP_OPT;

extern VF_INFO_STRUCT			gVFPar;
extern VF_AUTO_TORQUEBOOST_VAR  gVFAutoVar;
extern OVER_SHOCK_STRUCT		gVfOsc;
extern VF_WS_COMP_STRUCT		gWsComp;
extern VAR_AVR_STRUCT			gVarAvr;

extern MT_STRUCT				gHVfCur;

extern THREE_ORDER_FILTER      gOscAmp;
extern OVER_UDC_CTL_STRUCT		gOvUdc;
extern Uint const gExponentVf[][129];

extern HVF_OSC_DAMP_STRUCT      gHVfOscDamp;
extern HVF_OSC_JUDGE_INDEX      gHVfOscIndex;
extern HVF_DB_COMP_OPT          gHVfDeadBandCompOpt;

int  CalOutVotInVFStatus(int);
void OverShockControl(void);
void VFOverMagneticControl(void);
void VfVarInitiate(void);
void VFWsTorqueBoostComm( void );
void VFWSCompControl(void);
void VfFreqDeal2(void);
void VFSpeedControl(void);
void VFAutoTorqueBoost(void);

void VfFreqDeal(void);
void VfOverCurDeal();
void VfOverUdcDeal();

#ifdef __cplusplus
}
#endif

#endif
