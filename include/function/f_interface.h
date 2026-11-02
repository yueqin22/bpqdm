
#ifndef __F_INTERFACE_H__
#define __F_INTERFACE_H__

#define FUNC_TO_MOTOR_05MS_DATA_NUM     14
extern int16 gSendToMotor05MsDataBuff[];

#define FUNC_TO_MOTOR_2MS_DATA_NUM      75
#define FUNC_TO_CORE_DEBUG_DATA_NUM     40
extern int16 gSendToMotor2MsDataBuff[];

#define MOTOR_TO_Func_2MS_DATA_NUM      30
#define CORE_TO_FUNC_DISP_DATA_NUM	    30
extern int16 gSendToFunctionDataBuff[];

#define MOTOR_TO_Func_05MS_DATA_NUM       0
extern int16 gRealTimeToFunctionDataBuff[];

#define TUNE_DATA_NUM                  20
extern int16 gParaIdToFunctionDataBuff[];

extern Uint16 frq2Core;
extern Uint16 frqCurAim2Core;

extern Uint16 errorCodeFromMotor;
extern Uint16 currentOc;
extern Uint16 generatrixVoltage;
extern Uint16 outVoltage;
extern Uint16 outCurrent;
extern int16 outPower;
extern Uint16 currentPu;
extern Uint16 rsvdData;

extern int32 frqRun;
extern int32 frqVFRun;
extern int32 frqVFRunRemainder;
extern Uint16 frqFdb;
extern Uint16 frqFdbDisp;
extern int32  frqFdbTmp;
extern Uint16 frqFdbFlag;
extern Uint16 motorRun;

extern Uint16 outCurrentDispOld;

extern Uint16 enCoderPosition;
extern Uint16 ABZPos;

extern int16 gPhiRtDisp;

extern Uint16 pmsmRotorPos;

#endif
