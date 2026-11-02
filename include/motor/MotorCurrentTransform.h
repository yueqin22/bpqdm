
#ifndef MOTOR_CURRENT_INCLUDE_H
#define  MOTOR_CURRENT_INCLUDE_H
#ifdef  __cplusplus
extern "C" {
#endif

#include "SystemDefine.h"
#include "MotorDefine.h"

typedef struct I_STRUCT_DEF{
	Uint16     iuPrd;
	Uint16     ivPrd;
    Uint16 	   iuZero;
    Uint16     ivZero;
    Uint16     iuZeroOld;
    Uint16     ivZeroOld;
    Uint16     iuAver;
    Uint16     ivAver;
    Uint16     iuApply;
    Uint16     ivApply;
}I_STRUCT;
typedef struct MT_STRUCT_DEF{
	int  	M;
	int  	T;
}MT_STRUCT;
typedef struct MT_STRUCT_Q24_DEF{
    long    M;
    long    T;
}MT_STRUCT_Q24;
typedef struct ALPHABETA_STRUCT_DEF{
	long  	Alph;
	long  	Beta;
}ALPHABETA_STRUCT;
typedef struct ADC_STRUCT_DEF{
	Uint	DelaySet;
	Uint	DelayApply;

	long	ZeroTotal;
	int		ZeroCnt;
	int		Comp;
}ADC_STRUCT;
typedef struct UDC_STRUCT_DEF {
	int		DetaUdc;
	Uint	uDCBak;
	Uint 	uDC;
	Uint 	uDCFilter;
	Uint 	uDCBigFilter;
	Uint 	Coff;
	Uint    uDCADCoff;
	Uint    uDcCalMax;
}UDC_STRUCT;
typedef struct IUVW_SAMPLING_STRUCT_DEF{
	long  	U;
	long  	V;
	long  	W;
	long	UErr;
	long 	VErr;
	long    Coff;
}IUVW_SAMPLING_STRUCT;
typedef struct UVW_STRUCT_DEF_Q24{
	long  	U;
	long  	V;
	long  	W;
}UVW_STRUCT_Q24;
typedef struct UVW_STRUCT_DEF{
	int  	U;
	int  	V;
	int  	W;
}UVW_STRUCT;

typedef struct LINE_CURRENT_STRUCT_DEF{
	Uint  	CurPer;
	Uint  	CurBaseInv;
	Ulong  	CurPerFilter;
	Uint  	CurPerShow;
	int    CurTorque;
	Uint  	ErrorShow;

    Uint    Cur_Ft4;
}LINE_CURRENT_STRUCT;
typedef struct AMPTHETA_STRUCT_DEF{
	Uint  	Amp;
	int  	Theta;
	int     ThetaFilter;
	int     PowerAngle;

    int     CurTmpM;
    int     CurTmpT;
}AMPTHETA_STRUCT;

void AlphBetaToDQ(ALPHABETA_STRUCT* , int , MT_STRUCT_Q24* );

#ifdef __cplusplus
}
#endif

#endif
