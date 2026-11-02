#ifndef __F_IO_H__
#define __F_IO_H__

#include "f_funcCode.h"

#define DI_TERMINAL_NUMBER          (DI_NUMBER_PHSIC +     \
                                     DI_NUMBER_V +         \
                                     DI_NUMBER_AI_AS_DI)

#if !DEBUG_F_POSITION_CTRL
#define DI_FUNC_NUMBER              60
#elif 1
#define DI_FUNC_NUMBER              100
#endif

#define DI_FUNC_NO_FUNC             0
#define DI_FUNC_ACC_DEC_TIME_SRC1   16
#define DI_FUNC_ACC_DEC_TIME_SRC2   17
#define DI_FUNC_COUNTER_TICKER_IN   25
#define DI_FUNC_LENGTH_TICKER_IN    27
#define DI_FUNC_FVC                 31
#define DI_FUNC_APTP_ZERO           76

struct DI_FUNC1_BITS
{
    Uint16 noFunc:1;
    Uint16 fwd:1;
    Uint16 rev:1;
    Uint16 tripleLineCtrl:1;
    Uint16 fwdJog:1;
    Uint16 revJog:1;
    Uint16 up:1;
    Uint16 down:1;

    Uint16 closePwm:1;
    Uint16 errorReset:1;
    Uint16 runPause:1;
    Uint16 externalErrOpenIn:1;
    Uint16 multiSet:4;

    Uint16 accDecTimeSrc:2;
    Uint16 frqSrcSwitch:1;
    Uint16 clearUpDownFrq:1;
    Uint16 localOrRemote:1;
    Uint16 forbidAccDecSpd:1;
    Uint16 pidPause:1;
    Uint16 resetPLC:1;

    Uint16 swingPause:1;
    Uint16 counterTickerIn:1;
    Uint16 resetCounter:1;
    Uint16 lengthTickerIn:1;
    Uint16 resetLengthCounter:1;
    Uint16 forbidTorqueCtrl:1;
    Uint16 pulseIn:1;
    Uint16 motorCtrlMode2Fvc:1;
};
union DI_FUNC1
{
    Uint32                  all;
    struct DI_FUNC1_BITS    bit;
};

struct DI_FUNC2_BITS
{
    Uint16 brake:1;
    Uint16 externalErrCloseIn:1;
    Uint16 frqOk:1;
    Uint16 pidDirRev:1;
    Uint16 stopPanel:1;
    Uint16 diOrComm:1;
    Uint16 pidPauseI:1;
    Uint16 frqXSrc2Preset:1;
    Uint16 frqYSrc2Preset:1;
    Uint16 motorSnDi:2;
    Uint16 pidChg:1;
    Uint16 userError1:1;
    Uint16 userError2:1;
    Uint16 SpdTorqSwitch:1;
    Uint16 emergencyStop:1;
    Uint16 stop4dec:1;
    Uint16 decBrake:1;
    Uint16 clearSetRunTime:1;
    Uint16 rsvd2:13;
};
union DI_FUNC2
{
    Uint32                  all;
    struct DI_FUNC2_BITS    bit;
};

struct DI_FUNC
{
    union DI_FUNC1  f1;
    union DI_FUNC2  f2;
};

struct DI_STATUS_BITS
{
    Uint16 di1:1;
    Uint16 di2:1;
    Uint16 di3:1;
    Uint16 di4:1;

    Uint16 di5:1;
    Uint16 di6:1;
    Uint16 di7:1;
    Uint16 di8:1;

    Uint16 di9:1;
    Uint16 di10:1;
    Uint16 vdi1:1;
    Uint16 vdi2:1;

    Uint16 vdi3:1;
    Uint16 vdi4:1;
    Uint16 vdi5:1;
    Uint16 ai1:1;

    Uint16 ai2:1;
    Uint16 ai3:1;
    Uint16 rsvd:14;
};

union DI_STATUS
{
    Uint32 all;
    struct DI_STATUS_BITS bit;
};

struct DI_STATUS_ALL
{
    union DI_STATUS a;
    union DI_STATUS b;
    union DI_STATUS c;
};

#define DO_TERMINAL_NUMBER           (DO_NUMBER_PHSIC + DO_NUMBER_V)

#define DO_FUNC_COMM_CTRL            20
#define DO_FUNC_NUMBER               42

#if DSP_2803X
#define FUNCCODE_FM_OUT_SELECT_MAX   1
#else
#define FUNCCODE_FM_OUT_SELECT_MAX   2
#endif

struct DO_FUNC1_BITS
{
    Uint16 noFunc:1;
    Uint16 run:1;
    Uint16 error:1;
    Uint16 frqFdtArrive:1;
    Uint16 frqArrive:1;
    Uint16 zeroSpeedRun:1;
    Uint16 motorPreOl:1;
    Uint16 inverterPreOl:1;

    Uint16 counterSetArrive:1;
    Uint16 counterPointArrive:1;
    Uint16 lengthArrive:1;
    Uint16 plcEndLoop:1;
    Uint16 runTimeArrive:1;
    Uint16 frqLimit:1;
    Uint16 torqueLimit:1;
    Uint16 runReadyOk:1;

    Uint16 ai1GreaterThanAi2:1;
    Uint16 upperFrqArrive:1;
    Uint16 lowerFrqArrive:1;
    Uint16 uv:1;
    Uint16 commCtrl:1;
    Uint16 pcOk:1;
    Uint16 pcNear:1;
    Uint16 zeroSpeedRun1:1;
    Uint16 powerUpTimeArrive:1;
    Uint16 frqFdtArrive1:1;
    Uint16 frqArrive1:1;
    Uint16 frqArrive2:1;
    Uint16 currentArrive1:1;
    Uint16 currentArrive2:1;
    Uint16 setTimeArrive:1;
    Uint16 ai1limit:1;
};

union DO_FUNC1
{
    Uint32                  all;
    struct DO_FUNC1_BITS    bit;
};

struct DO_FUNC2_BITS
{
    Uint16 loseLoad:1;
    Uint16 speedDir:1;
    Uint16 oCurrent:1;
    Uint16 tempArrive:1;
    Uint16 softOc:1;
    Uint16 lowerFrqArrive:1;
    Uint16 errorOnStop:1;
    Uint16 motorForeOT:1;
    Uint16 setRunTimeArrive:1;
    Uint16 errorOnNoUV;
    Uint16 rsvd0:6;
	Uint16 rsvd1:16;
};

union DO_FUNC2
{
    Uint32                  all;
    struct DO_FUNC2_BITS    bit;
};

struct DO_FUNC
{
    union DO_FUNC1  f1;
    union DO_FUNC2  f2;
};

struct DO_STATUS_BITS
{

    Uint16 do3:1;
    Uint16 relay1:1;
    Uint16 relay2:1;
    Uint16 do1:1;

    Uint16 do2:1;
    Uint16 vdo1:1;
    Uint16 vdo2:1;
    Uint16 vdo3:1;

    Uint16 vdo4:1;
    Uint16 vdo5:1;

    Uint16 rsvd2:6;
};

union DO_STATUS
{
    Uint16 all;
    struct DO_STATUS_BITS bit;
};

struct DO_STATUS_ALL
{
    union DO_STATUS a;
    union DO_STATUS b;
    union DO_STATUS c;
};

struct DO_HW_STATUS_BITS
{

    Uint16 relay1:1;
    Uint16 do2:1;
    Uint16 do1:1;
    Uint16 relay2:1;

    Uint16 error:1;
    Uint16 run:1;
    Uint16 rsvd:1;
    Uint16 fan:1;
};
union DO_HW_STATUS
{
    Uint16 all;
    struct DO_HW_STATUS_BITS bit;
};
extern union DO_HW_STATUS doHwStatus;

struct DIDO_DELAY_STRUCT
{
    Uint32 high;
    Uint32 low;
};
union DI_DELAY_TICKER_UNION
{
    struct DIDO_DELAY_STRUCT all[3];

    struct
    {
        struct DIDO_DELAY_STRUCT di1;
        struct DIDO_DELAY_STRUCT di2;
        struct DIDO_DELAY_STRUCT di3;
    } single;
};
union DO_DELAY_TICKER_UNION
{
    struct DIDO_DELAY_STRUCT all[DO_TERMINAL_NUMBER];

    struct
    {
        struct DIDO_DELAY_STRUCT do3;
        struct DIDO_DELAY_STRUCT relay1;
        struct DIDO_DELAY_STRUCT relay2;
        struct DIDO_DELAY_STRUCT do1;
        struct DIDO_DELAY_STRUCT do2;
    } single;
};

struct DI_HW_STATUS_BITS
{
    Uint16 di1:1;
    Uint16 di2:1;
    Uint16 di3:1;
    Uint16 di4:1;

    Uint16 di5:1;
    Uint16 di6:1;
    Uint16 di7:1;
    Uint16 di8:1;

    Uint16 di9:1;
    Uint16 di10:1;
};

union DI_HW_STATUS
{
    Uint32 all;
    struct DI_HW_STATUS_BITS bit;
};

extern union DI_HW_STATUS diHwStatus;

#define AOFMP_FMP   0
#define AOFMP_AO1   1
#define AOFMP_AO2   2

#define AO_FMP_FUNC_NUMBER          13
#define AO_FMP_FUNC_FRQ_SET         0
#define AO_FMP_FUNC_FRQ_AIM         1
#define AO_FMP_FUNC_OUT_CURRENT     2
#define AO_FMP_FUNC_OUT_TORQUE      3
#define AO_FMP_FUNC_OUT_POWER       4
#define AO_FMP_FUNC_OUT_VOLTAGE     5
#define AO_FMP_FUNC_PULSE_IN        6
#define AO_FMP_FUNC_AI1             7
#define AO_FMP_FUNC_AI2             8
#define AO_FMP_FUNC_AI3             9
#define AO_FMP_FUNC_LENGTH          10
#define AO_FMP_FUNC_COUNTER         11
#define AO_FMP_FUNC_COMM            12
#define AO_FMP_FUNC_SPEED           13
#define AO_FMP_FUNC_OUT_CURRENT_1   14
#define AO_FMP_FUNC_OUT_VOLTAGE_1   15
#define AO_FMP_FUNC_OUT_TORQUE_DIR  16

typedef struct
{
    void (*calc)(void *);

    int16 mode;

    int32 x1;
    int32 y1;
    int32 x2;
    int32 y2;

    int32 x;
    int32 y;
} LINE_STRUCT;

void LineCalc(LINE_STRUCT *p);

#define LINE_STRTUCT_DEFALUTS       \
{                                   \
    (void (*)(void *))LineCalc      \
}

typedef struct
{
    void (*calc)(void *);

    int32 t;

    int32 in;
    int32 out;

    int32 outOld;
    int32 remainder;
} LowPassFilter;

void LpfCalc(LowPassFilter *p);

#define LPF_DEFALUTS            \
{                               \
    (void (*)(void *))LpfCalc   \
}

#define   EC_RISING       0x0
#define   EC_FALLING      0x1

#define   EC_ABS_MODE     0x0
#define   EC_DELTA_MODE   0x1

#define   EC_BYPASS       0x0
#define   EC_DIV1         0x0
#define   EC_DIV2         0x1
#define   EC_DIV4         0x2
#define   EC_DIV6         0x3
#define   EC_DIV8         0x4
#define   EC_DIV10        0x5

#define   EC_CONTINUOUS   0x0
#define   EC_ONESHOT      0x1

#define   EC_EVENT1       0x0
#define   EC_EVENT2       0x1
#define   EC_EVENT3       0x2
#define   EC_EVENT4       0x3

#define   EC_ARM          0x1

#define   EC_FREEZE       0x0
#define   EC_RUN          0x1

#define   EC_SYNCIN       0x0
#define   EC_CTR_PRD      0x1
#define   EC_SYNCO_DIS    0x2

#define   EC_CAP_MODE     0x0
#define   EC_APWM_MODE    0x1

#define   EC_ACTV_HI      0x0
#define   EC_ACTV_LO      0x1

#define   EC_DISABLE      0x0
#define   EC_ENABLE       0x1
#define   EC_FORCE        0x1

extern struct DI_FUNC diFunc;
extern struct DI_FUNC diSelectFunc;
extern struct DI_STATUS_ALL diStatus;

extern struct DO_FUNC doFunc;
extern struct DO_STATUS_ALL doStatus;

extern Uint32 pulseInFrq;
extern int16 pulseInSet;
extern Uint16 lineSpeed;

struct AI_DEAL
{
    Uint16 sample;

    int16 voltageOrigin;
    int16 voltage;

    int16 set;
};
extern struct AI_DEAL aiDeal[];

void InitSetEcap4(void);
void InitSetEcap2(void);
void InitSetEcap1();

void DiCalc(void);
void DoCalc(void);
void PulseInCalc(void);
void PulseInSample(void);
void FMPDeal(void);
void AiCalc(void);
void AoCalcChannel(Uint16);

void InitDIGpio(void);
void InitDOGpio(void);
void InitECap3Gpio(void);
void InitSetEcap3(void);
void InitECap4Gpio(void);

void InitSetEPWM4(void);
void InitSetEPWM5(void);
void InitSetEPWM6(void);

void InitEPwm4Gpio(void);
void InitEPwm5Gpio(void);
void InitEPwm6Gpio(void);

#endif
