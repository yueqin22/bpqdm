
#ifndef MOTOR_PMSM_INCLUDE_H
#define MOTOR_PMSM_INCLUDE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct IPM_POS_CHECK_STRUCT_DEF {
	Uint	FirstPos;
	Uint	Cnt;
	long	TotalErr;
	Ulong	TotalErrAbs;
	int     UvwStopErr;
    int     UvwStopErr_deg;
    Uint    UvwRevCnt;
}IPM_POS_CHECK_STRUCT;

typedef struct IPM_POSITION_STRUCT_DEF {
    Uint	InitPosMethod;

	Uint	RotorPos;
	Uint    ABZ_RotorPos_Ref;
	Uint	RealPos;

    Uint    InitPos;
    Uint    InitAngle_deg;

	Uint	RotorZero;
	Uint	RotorZeroGet;
	Uint    ZeroPosLast;

    Uint    PowerOffPos;
	Uint	PowerOffPosDeg;

	Uint	PosInZInfo;
	long	QepBak;
	long	QepTotal;

    long      ABZ_QepTotal;
    long      ABZ_QepBak;

    Uint    Zcounter;

	Uint    ZErrCnt;
	int     AbzErrPos;
    int     AbzErrPos_deg;
}IPM_POSITION_STRUCT;

typedef struct PM_INIT_POSITION_DEF
{
	int SubStep;
	int PeriodCnt;
	int Section;
	int CurFirst;
	int Cur[12];
	int Timer;
	unsigned int PWMTs;
	unsigned int PWMTGet;
	unsigned int PWMTSet;
}PM_INIT_POSITION;

typedef struct PM_FLUX_WEAK_DEF
{
    Uint    Mode;
    int     DecoupleMode;
    int     CsrGainMode;
    int     CoefFlux;
	long     VoltCoef;
    long    FluxD;
    int     FluxSd;
	long     IqLpf;
    int     VoltLpf;
    int     AbsFrqLpf;
    int     IdSet;
    int     IdMax;
    int     Vd;
    int     Vq;
	int     AdjustId;
    int     AdjustLimit;
	int     CoefAdj;
    int     CoefKI;

    long    VoltMax;
    long    CurrCoef;
    long    Iq;
    long    Omg;
    long    Id;
    long    IqFBLpf;
    long    IqLimit;
    long    TorqeCurr;
    long    AdjustId1;
    int     CoefIdComp;
    long    IqErr;
    int     IqErrAbs;
    long    OmgQ15;
    int     Ratio;
    int     VoltOut;

    long    data0;
    long    data1;
    long    data2;
    long    data3;
    long    data4;
    long    data5;
    long    data6;
    long    data7;
    long    data8;
    long    data9;
    long    ud;
    long    uq;

}PM_FLUX_WEAK;

typedef struct PM_DECOUPLE_DEF{
    int Omeg;
    int Isd;
    int Isq;
    int PhiSd;
    int PhiSq;
    int RotVd;
    int RotVq;
    int EnableDcp;
}PM_DECOUPLE;

typedef struct PM_FW_IN_DEF
{
	int  Time;
	int  Bemf;
	int  Udc;
	int  R;
	int  IsSetMax;
	int  IsAsrSetMax;
	int  IqSet;
	int  IqFeed;
	int  IdSet;
	int  IdFeed;
	int  VolCsrOut;
	int  UdCsrOut;
	int  UqCsrOut;

	int  IsAsrOut;
	int  MotorPoles;
	int  FullFreq;
	int  FreqFeed;
	int  MotorVol;
	long MotorCurr;

	int  MaxSetUdc;
	int  RatioSet;

	long KpId;

	long MaxDelId;
	int  AdjuMode;

	int  UdForIqMax;

	long KpIqMax;
	long KpIqMax1;

	int  UqForIdMax;

	long KpIdMax;
	int  CsrMaxVolt;

}PM_FW_IN;

typedef struct PM_FW_DEF
{

	int  IdMax;
	int  IdMax1;
	int  PhiPerLd;
	int  UdcLpf;
	int  UdLpf;
	int  UqLpf;
	int  RatioLpf;
	int  UdForIqMax;
	long  RatioLpf1;
	int  CurPerLpf;
	int  TorqPerAmp;
	int  IqErrLpf;
	int  IdErrLpf;
	int  IdForTorq;

	long AbsOmgPer;
	long Omg;
	long OmgLpf;
	long CurrCoef;
	long CoefIqMax;

	long AdId;
	long AdIdIntg;

	long AdIqMax;
	long AdIqMaxIntg;
	long AdIqMaxIntgMax;
	long AdIqMaxIntgMin;
	long AdIdMaxIntg;
	long TorqueCurr;
	long TorqueCurrMax;
	long MinUqLpf;
	long IqMax;
	long IqMax1;
	long PowerLpf;
	long TorqueEst;
	long Ld;
	long Lq;
	long Lq1;
	int  TorqRevi;
	int  TReviCoef1;
	int  PosComp;
	int  MaxPosComp;
	int  MpcBack;
	int  MinPosComp;
	long IqSetLpf;
	int  UqCompStat;
	int  UqRatio;
	int  imSet;
	int  itSet;
	int  umSet;
	int  utSet;
	int  utSetLpf;
}PM_FW;
typedef struct PM_FW_OUT_DEF
{

	int  IdSet;
	int  IqSet;
	int  IsLimit;
	int  UqComp;
    int  PosComp;
	int  ClearKID;
}PM_FW_OUT;

extern IPM_POSITION_STRUCT		gIPMPos;
extern PM_INIT_POSITION         gPMInitPos;
extern PM_FLUX_WEAK             gFluxWeak;
extern PM_DECOUPLE              gPmDecoup;
extern IPM_POS_CHECK_STRUCT	    gIPMPosCheck;
extern PM_FW_IN				   gPMFwIn;
extern PM_FW				   gPMFwCtrl;
extern PM_FW_OUT			   gPMFwOut;

extern void SynCalRealPos(void);
extern void IPMCheckInitPos(void);
extern void PmChkInitPosRest(void);
extern void RunCaseIpmInitPos(void);
extern void SynCalLdAndLq(Uint m_Pos);
extern void IPMCalAcrPIDCoff(void);
extern void PmFluxWeakDeal();
extern void PmDecoupleDeal(void);
extern void PMClacFluxWeakId(void);
extern void PMSetFwPara1(void);
extern void PMSetFwPara(void);
#ifdef __cplusplus
}
#endif

#endif
