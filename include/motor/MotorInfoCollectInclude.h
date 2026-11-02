
#ifndef MOTORINFO_COLLECT_INCLUDE_H
#define  MOTORINFO_COLLECT_INCLUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "MotorInclude.h"

typedef struct CUR_EXCURSION_STRUCT_DEF{
	long	TotalIu;
	long	TotalIv;
	int		Iu;
	int		Iv;
    long    IuValue;
    long    IvValue;
	int		ErrIu;
	int		ErrIv;
	int  	Count;
	int  	EnableCount;
	int  	ErrCnt;
}CUR_EXCURSION_STRUCT;

typedef struct TEMPLETURE_STRUCT_DEF{
	Uint	TempAD;
	Uint	Temp;
	Uint	TempBak;
	Uint	ErrCnt;
}TEMPLETURE_STRUCT;

typedef struct AI_STRUCT_DEF {
	Uint 	gAI1;
	Uint 	gAI2;
    Uint    gAI3;

    Ulong   ai1Total;
    Ulong   ai2Total;
    Ulong   ai3Total;
    int     aiCounter;
}AI_STRUCT;

extern I_STRUCT				    ISamp;
extern ADC_STRUCT				gADC;
extern UDC_STRUCT				gUDC;
extern IUVW_SAMPLING_STRUCT	    gCurSamp;
extern UVW_STRUCT				gIUVWQ12;
extern UVW_STRUCT_Q24           gIUVWQ24;
extern ALPHABETA_STRUCT		    gIAlphBeta;
extern ALPHABETA_STRUCT		    gIAlphBetaQ12;
extern MT_STRUCT				gIMTQ12;
extern MT_STRUCT                gIMTSetQ12;
extern MT_STRUCT_Q24            gIMTQ24;
extern AMPTHETA_STRUCT			gIAmpTheta;
extern LINE_CURRENT_STRUCT		gLineCur;
extern CUR_EXCURSION_STRUCT	    gExcursionInfo;
extern TEMPLETURE_STRUCT		gTemperature;
extern AI_STRUCT				gAI;

extern PID_STRUCT               gWspid;

void GetCurExcursion(void);
void GetUDCInfo(void);
void ADCProcess(void);
void GetCurrentInfo(void);

#ifdef __cplusplus
}
#endif

#endif
