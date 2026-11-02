
#ifndef MOTOR_STRUCT_DEFINE_H
#define  MOTOR_STRUCT_DEFINE_H
#ifdef __cplusplus
extern "C"[
#endif

struct MAIN_COMMAND_STRUCT_DEF{
   Uint    Start:1;
   Uint    StartDC:1;
   Uint    StopDC:1;
   Uint    ControlMode:2;
   Uint    PreExcFlux:1;
   Uint	   TorqueCtl:1;
   Uint    SpeedFlag:2;
   Uint    IntegralDiscrete:1;
};

union MAIN_COMMAND_UNION_DEF {
   Uint   all;
   struct MAIN_COMMAND_STRUCT_DEF  	bit;
};

struct MAIN_COMMAND_EXTEND_STRUCT_DEF{
   Uint    QepIndex:2;
   Uint    SpeedRev:1;
   Uint    DeadCompMode:3;
   Uint    ModulateType:1;
   Uint    Reserved:1;
   Uint    Reserved1:1;
   Uint    FreqUint:2;
   Uint    SpeedSearch:2;
   Uint    ShortGnd:1;
};

typedef union MAIN_COMMAND_EXTEND_UNION_DEF{
    Uint all;
    struct MAIN_COMMAND_EXTEND_STRUCT_DEF bit;
}MAIN_COMMAND_EXTEND_UNION;

struct SUB_COMMAND_STRUCT_DEF{
   Uint    ErrorOK:1;
   Uint    OutputLost:1;
   Uint    InputLost:1;
   Uint    MotorOvLoad:1;
   Uint	   LoadLose:1;
   Uint    NoStop:1;
   Uint	   CBCEnable:1;
   Uint    VarFcByTem:1;
   Uint    FanNoStop:1;
};
typedef union SUB_COMMAND_UNION_DEF {
   Uint   all;
   struct SUB_COMMAND_STRUCT_DEF  	bit;
}SUB_COMMAND_UNION;

struct SEND_STATUS_STRUCT_DEF{
   Uint    RunEnable:1;
   Uint    LowUDC:1;
   Uint    StartStop:1;
   Uint    ShortGndOver:1;
   Uint    SpeedSearchOver:1;
   Uint    PerOvLoadInv:1;
   Uint    PerOvLoadMotor:1;
   Uint    FanControl:1;
   Uint    OutOff:1;
};

typedef union SEND_STATUS_UNION_DEF {
   Uint   all;
   struct SEND_STATUS_STRUCT_DEF  	bit;
}SEND_STATUS_UNION;

typedef struct INV_STRUCT_DEF {
	Uint 	InvTypeApply;
	Uint    InvTypeSet;
	Uint    InvVoltageType;
	Uint 	InvType;
	Uint    GpTypeSet;
	Uint    GPType;
	Uint 	InvCurrent;
	Uint    InvCurrentOvload;
	Uint    InvCurrForP;
	Uint 	InvVolt;
	Uint 	CurrentCoff;
	Uint 	UDCCoff;
	Uint 	TempType;
	Uint 	InvUpUDC;
	Uint    InvLowUdcStad;
	Uint 	InvLowUDC;
	Uint    LowUdcCoff;
	Uint 	BaseUdc;
}INV_STRUCT;

typedef enum CONTROL_MOTOR_TYPE_ENUM_DEF{
    ASYNC_SVC,
    ASYNC_FVC,
    ASYNC_VF,

    SYNC_SVC = 10,
    SYNC_FVC,
    SYNC_VF,

    DC_CONTROL = 20,
    RUN_SYNC_TUNE
}CONTROL_MOTOR_TYPE_ENUM;

typedef struct MOTOR_STRUCT_DEF {
    Uint    MotorType;
    Uint    LastMotorType;
	Uint 	Power;
	Uint 	Votage;
	Uint 	CurrentGet;
	Uint 	Frequency;
	Uint 	FreqPer;
	Uint 	Current;
	Uint	CurBaseCoff;
	Uint    Motor_HFreq;
    Uint    Motor_LFreq;
}MOTOR_STRUCT;

typedef struct MOTOR_EXTERN_STRUCT_DEF {
    Uint    UnitCoff;
	Uint 	R1;
	Uint 	R2;
	Uint 	L0;
	Uint 	LM;
	Uint 	I0;
	Uint    IoVsFreq;
	Uint 	Rpm;
    Uint    RatedComp;
	Uint 	Poles;
	Uint 	L1;
	Uint 	L2;
    Uint    RsPm;
	Uint 	LD;
	Uint 	LQ;
	Uint    BemfVolt;
	Uint    FluxRotor;
    Uint    FluxRotor1;
    Uint    ItRated;
    Uint    FluxLeakCurveGain;
}MOTOR_EXTERN_STRUCT;

struct ERROR_FLAG_SIGNAL {
    Uint16  OvCurFlag:1;
    Uint16  OvUdcFlag:1;
	Uint16  Res:14;
};

union ERROR_FLAG_SIGNAL_DEF {
   Uint16                	all;
   struct ERROR_FLAG_SIGNAL  bit;
};

typedef struct PRG_STATUS_STRUCT_DEF{
   Uint    PWMDisable:1;
   Uint    ACRDisable:1;
   Uint    ASRDisable:1;
}PRG_STATUS_STRUCT;

typedef union PRG_STATUS_UNION_DEF {
   Uint   all;
   PRG_STATUS_STRUCT bit;
}PRG_STATUS_UNION;

typedef struct RUN_STATUS_STRUCT_DEF {
	Uint 	RunStep;
	Uint 	SubStep;
	Uint    ParaCalTimes;
	PRG_STATUS_UNION 	PrgStatus;
	union   ERROR_FLAG_SIGNAL_DEF	ErrFlag;
    SEND_STATUS_UNION		StatusWord;
}RUN_STATUS_STRUCT;

typedef struct BASE_COMMAND_STRUCT_DEF {
	union MAIN_COMMAND_UNION_DEF Command;
	int 	FreqSet;
	int     FreqSetApply;
	int		FreqSetBak;
	int 	FreqSyn;
    int     FreqSynFilter;
	int     FreqWs;
	int 	FreqDesired;
	int 	VCTorqueLim;
	int     FreqToFunc;

    int     FreqFeed;
	long 	FreqReal;
	long    FreqDesiredReal;
    long    FreqSetReal;

    long    FreqRealFilt;

	Uint    pu2siCoeff;

	Uint    si2puCoeff;
	Uint	FirstCnt;

}BASE_COMMAND_STRUCT;

typedef struct BASE_PAR_STRUCT_DEF {
	Ulong 	FullFreq01;
	Uint    FullFreq;
	Uint 	MaxFreq;
	Uint 	FcSet;
	Uint 	FcSetApply;
}BASE_PAR_STRUCT;

typedef struct COM_PAR_INFO_STRUCT_DEF {
	Uint 	StartDCBrakeCur;
	Uint 	StopDCBrakeCur;
	Uint 	BrakeCoff;
	Uint 	MotorOvLoad;
	Uint 	PerMotorOvLoad;
	Uint 	SpdSearchMethod;
	Uint 	SpdSearchTimeSet;
	Uint 	SpdSearchTime;
}COM_PAR_INFO_STRUCT;

typedef struct MOTOR_POWER_TORQUE_DEF{
    long    InvPowerPU;
    int     InvPower_si;
    int     TrqOut_pu;

    Uint    rpItRated;
    int     anglePF;
    int     Cur_Ft4;
}MOTOR_POWER_TORQUE;

typedef struct CPU_TIME_STRUCT_DEF {
	Ulong 	MFA2msBase;
    Ulong 	MFB2msBase;
    Ulong 	MFC2msBase;
    Ulong 	MFD2msBase;
    Ulong   Motor05MsBase;
	Ulong	ADCIntBase;
	Ulong	PWMIntBase;
	Uint 	MFA2ms;
    Uint 	MFB2ms;
    Uint 	MFC2ms;
    Uint 	MFD2ms;
	Uint    Motor05Ms;
	Uint	ADCInt;
	Uint	PWMInt;
    Uint    tmpBase;
    Uint    tmpTime;
    Uint    CpuCoff0Ms;
    Uint    tmp0Ms;
    Uint    Det05msClk;
    Uint    CpuBusyCoff;
}CPU_TIME_STRUCT;

typedef struct MOTOR_DEBUG_DATA_RECEIVE_STRUCT_DEF{
    int     TestData0;
    int     TestData1;
    int     TestData2;
    int     TestData3;
    int     TestData4;
    int     TestData5;
    int     TestData6;
    int     TestData7;
    int     TestData8;
    int     TestData9;
    int     TestData10;
    int     TestData11;
    int     TestData12;
    int     TestData13;
    int     TestData14;
    int     TestData15;
    int     TestData16;
    int     TestData17;
    int     TestData18;
    int     TestData19;
    int     TestData20;
    int     TestData21;
    int     TestData22;
    int     TestData23;
    int     TestData24;
    int     TestData25;
    int     TestData26;
    int     TestData27;
    int     TestData28;
    int     TestData29;
    int     TestData30;
    int     TestData31;
    int     TestData32;
    int     TestData33;
    int     TestData34;
    int     TestData35;
    int     TestData36;
    int     TestData37;
    int     TestData38;
    int     TestData39;
}MOTOR_DEBUG_DATA_RECEIVE_STRUCT;

typedef struct MOTOR_DEBUG_DATA_DISPLAY_STRUCT_DEF{
    int     DisplayData1;
    int     DisplayData2;
    int     DisplayData3;
    int     DisplayData4;
    int     DisplayData5;
    int     DisplayData6;
    int     DisplayData7;
    int     DisplayData8;
    int     DisplayData9;
    int     DisplayData10;
    int     DisplayData11;
    int     DisplayData12;
    int     DisplayData13;
    int     DisplayData14;
    int     DisplayData15;
    int     DisplayData16;
    int     DisplayData17;
    int     DisplayData18;
    int     DisplayData19;
    int     DisplayData20;
    int     DisplayData21;
    int     DisplayData22;
    int     DisplayData23;
    int     DisplayData24;
    int     DisplayData25;
    int     DisplayData26;
    int     DisplayData27;
    int     DisplayData28;
    int     DisplayData29;
    int     DisplayData30;
}MOTOR_DEBUG_DATA_DISPLAY_STRUCT;

#ifdef __cplusplus
}
#endif

#endif
