#ifndef __F_COMMAND_H__
#define __F_COMMAND_H__

#include "f_funcCode.h"

#define CONST_SPEED     0
#define ACC_SPEED       1
#define DEC_SPEED       2

#define FORWARD_DIR         0
#define REVERSE_DIR         1

#define RUN_CMD_NO_JOG      0
#define RUN_CMD_FWD_JOG     1
#define RUN_CMD_REV_JOG     2
#define RUN_CMD_FWD_REV_JOG 3
struct RUN_CMD_BITS
{
    Uint16 common0:1;
    Uint16 common:1;
    Uint16 jog:2;
    Uint16 rsvd1:2;

    Uint16 dir:1;

    Uint16 pause:1;

    Uint16 freeStop:1;
    Uint16 hurryStop:1;

    Uint16 otherStop:1;
    Uint16 startProtect:1;

    Uint16 errorReset:1;

    Uint16 rsvd:3;
};

union RUN_CMD
{
    Uint16              all;
    struct RUN_CMD_BITS bit;
};

extern union RUN_CMD runCmd;

struct INVT_STATUS_BITS
{
    Uint16 run:2;
    Uint16 accDecStatus:2;
    Uint16 uv:1;
};

union INVT_STATUS
{
    Uint16               all;
    struct INVT_STATUS_BITS bit;
};
extern union INVT_STATUS invtStatus;

struct RUN_FLAG_BITS
{
    Uint16 run:1;

    Uint16 common:1;
    Uint16 jog:1;
    Uint16 tune:1;

    Uint16 jogWhenRun:1;

    Uint16 accDecStatus:2;

    Uint16 plc:1;
    Uint16 pid:1;
    Uint16 torque:1;

    Uint16 dir:1;
    Uint16 curDir:1;
    Uint16 dirReversing:1;
    Uint16 dirFinal:1;

    Uint16 servo:1;
    Uint16 rsvd:1;
};

union RUN_FLAG
{
    Uint16               all;
    struct RUN_FLAG_BITS bit;
};
extern union RUN_FLAG runFlag;

struct RUN_STATUS_FIRST_STEP_BITS
{
    Uint16 accStep:1;
    Uint16 invarianceStep:1;
    Uint16 decStep:1;
};

union RUN_STATUS_FIRST_STEP
{
    Uint16 all;
    struct RUN_STATUS_FIRST_STEP_BITS bit;
};

extern union RUN_STATUS_FIRST_STEP runStatus1Step;

struct DSP_MAIN_COMMAND_BITS
{
    Uint16 run:1;
    Uint16 startBrake:1;
    Uint16 stopBrake:1;
    Uint16 motorCtrlMode:2;
    Uint16 startFlux:1;
    Uint16 torqueCtrl:1;
    Uint16 accDecStatus:2;
    Uint16 spdLoopI1:1;
};

union DSP_MAIN_COMMAND
{
    Uint16                       all;
    struct DSP_MAIN_COMMAND_BITS bit;
};

extern union DSP_MAIN_COMMAND dspMainCmd;

struct DSP_MAIN_COMMAND1_BITS
{
    Uint16 pgLocation:2;
    Uint16 fvcPgLogic:1;
    Uint16 deadCompMode:3;
    Uint16 modulationMode:1;
    Uint16 rsvd:1;
    Uint16 rsvd1:1;
    Uint16 frqPoint:2;
    Uint16 speedTrack:2;
    Uint16 shortGnd:1;
};

union DSP_MAIN_COMMAND1
{
    Uint16                       all;
    struct DSP_MAIN_COMMAND1_BITS bit;
};

extern union DSP_MAIN_COMMAND1 dspMainCmd1;

struct DSP_SUB_COMMAND_BITS
{
    Uint16 errorDealing:1;
    Uint16 outPhaseLossProtect:1;
    Uint16 inPhaseLossProtect:1;
    Uint16 overloadMode:1;
    Uint16 loseLoadProtectMode:1;
    Uint16 poffTransitoryNoStop:1;
    Uint16 cbc:1;
    Uint16 varFcByTem:1;
    Uint16 fanRunWhenWaitStopBrake:1;
    Uint16 contactorMode:1;
};

union DSP_SUB_COMMAND
{
    Uint16                      all;
    struct DSP_SUB_COMMAND_BITS bit;
};

extern union DSP_SUB_COMMAND dspSubCmd;

struct DSP_STATUS_BITS
{
    Uint16 runEnable:1;
    Uint16 uv:1;
    Uint16 run:1;
    Uint16 rsvd:1;
    Uint16 speedTrackEnd:1;
    Uint16 inverterPreOl:1;
    Uint16 motorPreOl:1;
    Uint16 fan:1;
    Uint16 outAirSwitchOff:1;
};

union DSP_STATUS
{
   Uint16                   all;
   struct DSP_STATUS_BITS   bit;
};

extern union DSP_STATUS dspStatus;

struct DSP_SUB_STATUS_BITS
{
    Uint16 accDecStatus:2;
    Uint16 rsvd:14;
};

union DSP_SUB_STATUS
{
   Uint16                       all;
   struct DSP_SUB_STATUS_BITS   bit;
};

extern union DSP_SUB_STATUS dspSubStatus;

#define ERR_RUN_FRQ_RUN       0
#define ERR_RUN_FRQ_AIM       1
#define ERR_RUN_FRQ_UPPER     2
#define ERR_RUN_FRQ_LOWER     3
#define ERR_RUN_FRQ_SECOND    4

enum RUN_STATUS
{
    RUN_STATUS_WAIT,
    RUN_STATUS_ZERO,
    RUN_STATUS_START,
    RUN_STATUS_NORMAL,
    RUN_STATUS_STOP,
    RUN_STATUS_JOG,

    RUN_STATUS_POS_CTRL,

    RUN_STATUS_TUNE,
    RUN_STATUS_DI_BRAKE_DEC,
    RUN_STATUS_DI_BRAKE,
    RUN_STATUS_LOSE_LOAD,
    RUN_STATUS_SHUT_DOWN
};
extern enum RUN_STATUS runStatus;

enum START_RUN_STATUS
{
    START_RUN_STATUS_SPEED_TRACK,
    START_RUN_STATUS_BRAKE,
    START_RUN_STATUS_PRE_FLUX,
    START_RUN_STATUS_HOLD_START_FRQ
};
#define START_RUN_STATUS_INIT           START_RUN_STATUS_SPEED_TRACK
extern enum START_RUN_STATUS startRunStatus;

enum STOP_RUN_STATUS
{
    STOP_RUN_STATUS_DEC_STOP,
    STOP_RUN_STATUS_WAIT_BRAKE,
    STOP_RUN_STATUS_BRAKE
};
#define STOP_RUN_STATUS_INIT            STOP_RUN_STATUS_DEC_STOP
extern enum STOP_RUN_STATUS stopRunStatus;

typedef struct
{
    void (*calc)(void *);

    int32 maxValue;
    int32 aimValue;
    int32 curValue;

    Uint32 tickerAll;
    int32 remainder;
} LINE_CHANGE_STRUCT;

#define LINE_CHANGE_STRTUCT_DEFALUTS       \
{                                          \
    (void (*)(void *))LineChangeCalc       \
}

extern Uint32 accFrqTime;
extern Uint32 decFrqTime;
extern Uint16 startProtectSrc;
extern Uint16 otherStopSrc;

extern Uint16 runSrc;

extern Uint16 bDiAccDecTime;
extern Uint16 accDecTimeSrcPlc;

extern Uint16 runDirPanelOld;
extern Uint16 shuntFlag;
extern Uint16 setRunLostTime;
extern Uint16 setRunTimeAim;
extern Uint16 swingFrqLimit;

enum SWING_STATUS
{
    SWING_NONE,
    SWING_UP,
    SWING_DOWN
};
extern enum SWING_STATUS swingStatus;

extern Uint16 tuneCmd;
extern int32 accel;
extern Uint16 accelDisp;

extern Uint16 pcFdbDisp;

extern int32 frqCurAimOld;
extern int32 accTimeOld;
extern int32 decTimeOld;

extern LINE_CHANGE_STRUCT frqLine;

void UpdateRunCmd(void);
void RunSrcDeal(void);
void AccDecTimeCalc(void);
void LineChangeCalc(LINE_CHANGE_STRUCT *p);
void LineFrqChangeCalc(LINE_CHANGE_STRUCT *p);

void AccDecFrqCalc(int32 accTime, int32 decTime, Uint16 mode);

void setTimeRun(void);
void runTimeCal(void);

extern int16 upperTorque;
extern int16 torqueAim;
void TorqueCalc(void);
Uint16 TorqueLimitCalc(void);

#endif
