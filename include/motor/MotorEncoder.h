
#ifndef MOTOR_ENCODER_INCLUDE_H
#define MOTOR_ENCODER_INCLUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "MotorInclude.h"
#include "SystemDefine.h"
#include "MotorDefine.h"

#define C_MAX_C_ZERO            200
#define C_MIN_D_ZERO            19000
#define C_UVW_FOR_ZERO          5
#define C_UVW_BACK_ZERO         6

#define DIR_FORWARD   1
#define DIR_BACKWARD  2
#define DIR_ERROR     0

#define GetQepCnt()			(*EQepRegs).QPOSCNT
#define SetQepCnt(X)		    (*EQepRegs).QPOSCNT = X;

#define ACTIVE_HARDWARE_LOGICAL_U   1
#define ACTIVE_HARDWARE_LOGICAL_V   1
#define ACTIVE_HARDWARE_LOGICAL_W   1

typedef enum PG_TYPE_ENUM_STRUCT_DEF{
    PG_TYPE_ABZ,
    PG_TYPE_UVW,
    PG_TYPE_RESOLVER,
    PG_TYPE_SC,
    PG_TYPE_SPECIAL_UVW,
    PG_TYPE_NULL=100,
    PG_TYPE_RT
}PG_TYPE_ENUM_STRUCT;

typedef enum QEP_INDEX_ENUM_STRUCT_DEF{
    QEP_SELECT_1,
    QEP_SELECT_2,
    QEP_SELECT_PULSEIN,
    QEP_SELECT_NONE=100
}QEP_INDEX_ENUM_STRUCT;

typedef struct PG_DATA_STRUCT_DEF {
	Uint 	PGType;
    QEP_INDEX_ENUM_STRUCT   QEPIndex;

    Uint    PGMode;
    Uint    PGTypeGetFromFun;
	Uint 	PulseNum;
    Uint    SpeedDir;
    Uint    SpeedDirLast;
    Uint    PGDir;
    Uint    PGErrorFlag;

    Uint    imPgEstTick;
    int     imDirAdder;
    long    imFreqErr;
    long    imFrqEncoder;
}PG_DATA_STRUCT;

typedef struct ROTOR_SPEED_SMOOTH_DEF {
    int     LastSpeed;
    int     SpeedMaxErr;
}ROTOR_SPEED_SMOOTH;

typedef struct FVC_SPEED_STRUCT_DEF {

	int		Flag;
    Uint    TransRatio;
    Uint    MTZeroCnt;
	int 	SpeedEncoder;
	int     SpeedTemp;

	long 	MLastPos;
	int 	MDetaPos;
	int     MDetaPosBak;
	int 	MSpeed;

	Uint 	MTPulseNum;
	Ulong 	MTTime;
	Uint	MTCnt;
	int 	MTSpeed;
	Uint	MTLimitTime;
    Uint    IntCnt;
    Uint    IntNum;

    ROTOR_SPEED_SMOOTH  MSpeedSmooth;
    ROTOR_SPEED_SMOOTH  MTSpeedSmooth;
}FVC_SPEED_STRUCT;

typedef struct IPM_UVW_PG_STRUCT_DEF {
	Uint	UVWData;
	Uint	UVWAngle;

	Uint    U_Value;
	Uint    V_Value;
	Uint    W_Value;
	Uint	ZeroCnt;

    Uint    LogicU;
    Uint    LogicV;
    Uint    LogicW;

    Uint	UvwDir;

    Uint    debugdeta1;
    Uint    debugdeta2;

    Uint    lastAgl;
    Uint    NewAgl;
    Uint    ErrAgl;
    Uint    TuneFlag;

    llong   TotalErr;
    Ulong   UvwCnt;

    int     UvwZIntErr;
    int     UvwZIntErr_deg;

    Uint    UvwZeroPos;
    Uint    UvwZeroPos_deg;

    Uint    UvwEstStep;
    int     UvwCnt2;
}IPM_UVW_PG_STRUCT;

typedef struct IPM_PG_DIR_STRUCT_DEF {
	int	    ABAngleBak;
    int     ABDirCnt;
    int     ABDir;
	int	    UVWAngleBak;
    int     UVWDirCnt;
    int     UVWDir;
	int	    CDAngleBak;
    int     CDDirCnt;
    int     CDDir;
    int     CDErr;
    int     RtPhaseBak;
    int     RtDirCnt;
}IPM_PG_DIR_STRUCT;

typedef struct ROTOR_TRANS_STRUCT_DEF{
    Uint    RTPos;
    Uint    RtorBuffer;
    int     RealTimeSpeed;
    Ulong 	TimeBak;
    Ulong	DetaTime;
	Uint    PosBak;
	int		DetaPos;
	int		FreqFeed;
	Uint	Flag;
    Uint    Poles;
    Uint    PolesRatio;
    int     PosComp;
	int     ConFlag;
    Uint    SimuZBack;
    Uint    SimuZBack2;

    Uint    IntNum;
    Uint    IntCnt;

    Uint    RtRealPos;

    Uint    AbsRotPos;
}ROTOR_TRANS_STRUCT;

extern struct EQEP_REGS        *EQepRegs;
extern IPM_UVW_PG_STRUCT        gUVWPG;
extern PG_DATA_STRUCT			gPGData;
extern IPM_PG_DIR_STRUCT        gPGDir;
extern BURR_FILTER_STRUCT		gSpeedFilter;
extern CUR_LINE_STRUCT_DEF		gSpeedLine;
extern ROTOR_TRANS_STRUCT		gRotorTrans;
extern FVC_SPEED_STRUCT		gFVCSpeed;

extern void GetUvwPhase(void);
extern Uint JudgeABDir(void);
extern Uint JudgeUVWDir(void);
extern Uint JudgeRTDir(void);
extern void GetMTTimeNum(void);
extern void GetMDetaPos(void);
extern void RotorTransCalVel(void);
extern void GetRotorTransPos(void);
extern void RotorTransSamplePos(void);
extern void VCGetFeedBackSpeed(void);
extern void ReInitForPG(void);
extern void InitSetQEP(void);

#ifdef __cplusplus
}
#endif

#endif
