
#ifndef SUBPRG_INCLUDE_H
#define SUBPRG_INCLUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "SystemDefine.h"
#include "MotorDefine.h"

#define	Filter1(x,total)    (x)
#define	Filter2(x,total)   (( (((Ulong)total)<<16) + (((Ulong)x)<<15) - (((Ulong)total)<<15) )>>16)
#define	Filter4(x,total)   (( (((Ulong)total)<<16) + (((Ulong)x)<<14) - (((Ulong)total)<<14) )>>16)
#define	Filter8(x,total)   (( (((Ulong)total)<<16) + (((Ulong)x)<<13) - (((Ulong)total)<<13) )>>16)
#define	Filter16(x,total)  (( (((Ulong)total)<<16) + (((Ulong)x)<<12) - (((Ulong)total)<<12) )>>16)
#define	Filter32(x,total)  (( (((Ulong)total)<<16) + (((Ulong)x)<<11) - (((Ulong)total)<<11) )>>16)
#define	Filter64(x,total) (( (((Ulong)total)<<16) + (((Ulong)x)<<10) - (((Ulong)total)<<10) )>>16)
#define Filter128(x,total) (( (((Ulong)total)<<16) + (((Ulong)x)<<9 ) - (((Ulong)total)<<9 ) )>>16)
#define Filter256(x,total) (( (((Ulong)total)<<16) + (((Ulong)x)<<8 ) - (((Ulong)total)<<8 ) )>>16)

typedef struct PID_STRUCT_DEF {
	long 	Total;
	long 	Out;
	int  	Max;
	int  	Min;
	int  	Deta;
	int  	KP;
	int  	KI;
	int  	KD;

    Uint    QP;
    Uint    QI;
    Uint    QD;
}PID_STRUCT;
typedef struct PID_STRUCT_LONG_DEF {
	llong 	Total;
	llong 	Out;
	long  	Max;
	long  	Min;
	long  	Deta;
	Uint  	KP;
	Uint  	KI;
	Uint  	KD;

    int     sVoltGain;
}PID_STRUCT_LONG;

typedef struct PID32_STRUCT_DEF{
	long  	Deta;
	long 	Out;
	long 	Total;
	long  	Max;
	long  	Min;
	long  	KP;
	long  	KI;
}PID32_STRUCT;

typedef struct BURR_FILTER_STRUCT_DEF{
	long 	Input;
	long	Output;
	long 	Err;
	long  	Max;
	long  	Min;
}BURR_FILTER_STRUCT;

typedef struct CUR_LINE_STRUCT_DEF{
	int		FilterTime;
	int 	Input;
	int		Output;
	int		Data[16];
}CUR_LINE_STRUCT_DEF;

extern Uint swap(Uint);
extern int 	abs(int);
extern int 	absl(long int);
extern int 	qsin(int);
extern int 	qatan(long int);
extern int  atan(int x, int y);
extern Uint qsqrt(Ulong);

extern Uint GetInvCurrent(Uint);
extern void PID(PID_STRUCT * pid);
extern void BurrFilter(BURR_FILTER_STRUCT * );
extern int Filter(int , int , int );

#ifdef __cplusplus
}
#endif

#endif
