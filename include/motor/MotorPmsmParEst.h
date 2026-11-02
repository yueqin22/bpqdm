
#ifndef MOTOR_PM_INCLUDE_H
#define MOTOR_PM_INCLUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#define INIT_POS_NULL			0
#define INIT_POS_VOLT_PULSE		1

typedef struct IPM_ZERO_POS_STRUCT_DEF {
	Uint	FeedPos;
	Uint    FeedABZPos;
	Uint	FirstPos;
	Uint	Flag;

	Uint	zFilterCnt;
	Uint	Cnt;

	long	QepBak;
	int		DetectCnt;

	long	TotalErr;

    Uint    CurLimit;
    Uint    time;
}IPM_ZERO_POS_STRUCT;

typedef struct IPM_INITPOS_PULSE_STRUCT_DEF {
	Uint	Waite;
	Uint	Step;
	Uint	Flag;
	Uint	PeriodCnt;
	Uint	Section;
	Uint	PWMTs;
	Uint	InitPWMTs;
	Uint	LPhase[3];
	Uint    Ld;
	Uint    Lq;
	Uint	CurLimit;
	int		CurFirst;
	int		Cur[12];
    int     PhsChkStep;
}IPM_INITPOS_PULSE_STR;

typedef struct PMSM_EST_PARAM_DEF
{
	Uint    IdKp;
	Uint    IdKi;
	Uint    IqKp;
	Uint    IqKi;

    Uint    CoderPos_deg;
    Uint    EstZero;

    Uint    UvwDir;
    Uint    UvwZeroAng;
    Uint    UvwZeroAng_deg;

    Uint   UvwZPos;
}PMSM_EST_PARAM_DATA;

typedef struct PMSM_EST_BEMF_DEF
{
    Uint    TuneFreq;
    Ulong   TotalId1;
    Ulong   TotalId2;
    Ulong   TotalVq1;
    Ulong   TotalVq2;

    int     IdSet;
    int     IdSetFilt;
    int     IqSet;
    int     TuneFreqSet;
    int     TuneFreqAim;
    Ulong   AccDecTick;
    int     FreqStep;
    long    FreqRem;

    Uint    BemfVolt;
    Uint    Cnt;
}PMSM_EST_BEMF;

extern IPM_ZERO_POS_STRUCT		gIPMZero;
extern IPM_INITPOS_PULSE_STR	gIPMInitPos;
extern PMSM_EST_PARAM_DATA		gPmParEst;
extern PMSM_EST_BEMF            gEstBemf;

extern void SynInitPosDetSetPwm(Uint Section);
extern void SynTuneInitPos(void);
extern void SynInitPosDetect(void);
extern void SynTunePGZero_No_Load(void);
extern void SynTunePGZero_Load(void);
extern void SetIPMPos(Uint Pos);
extern void SetIPMPos_ABZRef(Uint Pos);
extern void SynTuneBemf(void);

#ifdef __cplusplus
}
#endif

#endif
