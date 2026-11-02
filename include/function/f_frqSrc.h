#ifndef __F_FRQSRC_H__
#define __F_FRQSRC_H__

#include "f_funcCode.h"

#define ACC_DEC_FRQ_NONE    0
#define ACC_DEC_FRQ_WAIT    1
#define ACC_DEC_FRQ_DONE    2

typedef struct
{

    Uint16 frq;
    Uint16 range;

    Uint32 low;
    Uint32 high;
} JUMP_FRQ;

typedef struct
{
    void  (*calc)(void *);

    int32  ref;
    int32  fdb;

    int32  Kp;
    int32  Ki;
    int32  Kd;

    int32  Kp2;
    int32  Ki2;
    int32  Kd2;

    int16  Qp;
    int16  Qi;
    int16  Qd;

    int32  deltaMax;
    int32  deltaMin;
    int32  outMax;
    int32  outMin;
    int32  pidDLimit;
    int32  errorDead;
    int32  errorSmall;
    int32  errorBig;

    int32  error;
    int32  error1;
    int32  error2;

    int32  delta;

    int32  deltaPRem;
    int64  deltaIRem;
    int32  deltaDRem;
    int32  deltaRemainder;

    int32  out;
    Uint16 sampleTime;
    Uint16 sampleTcnt;
} PID_FUNC;

#define PID_FUNC_DEFAULTS         \
{                                 \
    (void (*)(void *))PidFuncCalc \
}

extern PID_FUNC pidFunc;

extern int32 frq;
extern int32 frqTmp;
extern int32 frqFrac;
extern int32 frqTmpFrac;
extern int32 frqCurAimFrac;
extern int32 frqDroop;
extern int32 frqAim;
extern int32 frqAiPu;
extern int32 frqAimTmp;
extern int32 frqAimTmp0;
extern int32 frqCurAim;
extern Uint16 frqKeyUpDownDelta;
extern Uint16 upperFrq;
extern Uint16 lowerFrq;
extern Uint16 maxFrq;
extern Uint16 benchFrq;
extern Uint16 frqPuQ15;
extern int32 uPDownFrqMax;
extern int32 uPDownFrqMin;
extern Uint16 upDownFrqInit;
extern Uint16 plcStep;
extern Uint32 plcTime;
extern Uint16 bPlcEndOneLoop;
extern Uint16 plcStepRemOld;
extern Uint32 plcTimeRemOld;

extern Uint16 timeBench;

extern int32 frqDigitalTmp;
extern Uint16 bFrqDigital;

extern Uint16 frqCalcSrcOld;

extern Uint16 bStopPlc;
extern Uint16 bAntiReverseRun;

extern int32 frqAimOld4Dir;

#define RUN_MODE_SPEED_CTRL     0
#define RUN_MODE_TORQUE_CTRL    1
#define RUN_MODE_POSITION_CTRL  2
extern Uint16 runMode;

#define UP_DN_OPERATION_ON          1
#define UP_DN_OPERATION_OFF         0
struct FRQ_FLAG_BITS
{
    Uint16 comp:1;
    Uint16 upDown:1;
    Uint16 x:1;
    Uint16 y:1;

    Uint16 fcPosLimit:1;
    Uint16 fcNegLimit:1;

    Uint16 upDownoperationStatus:1;

    Uint16 frqSetLimit:1;
};
union FRQ_FLAG
{
    Uint16 all;
    struct FRQ_FLAG_BITS bit;
};

extern union FRQ_FLAG frqFlag;

struct FRQ_XY
{
    int32 x;
    int32 y;
    int32 z;
};

extern struct FRQ_XY frqXy;

void FrqSrcDeal(void);
void UpdateFrqAim(void);
void PidFuncCalc(PID_FUNC *);

extern Uint16 dpFrqAim;
extern Uint16 frqCalcSrc;
extern Uint16 plcStepOld;
int32 FrqPidSetDeal(void);
int32 UpdateMultiSetFrq(Uint16 step);
void ResetUpDownFrq(void);

extern int32 upDownFrq;
extern int32 upDownFrqTmp;

#endif
