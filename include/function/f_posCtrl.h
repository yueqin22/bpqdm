#ifndef __F_POSCTRL_H__
#define __F_POSCTRL_H__

#include "f_funcCode.h"

#if (DEBUG_F_POSITION_CTRL)

#if F_DEBUG_RAM
#define PC_APTP_CTRL    0
#define PC_PCMD_CTRL    0
#define PC_RC_CTRL      0
#elif 1
#define PC_APTP_CTRL    1
#define PC_PCMD_CTRL    1
#define PC_RC_CTRL      1
#endif

#elif 1

#define PC_APTP_CTRL    0
#define PC_PCMD_CTRL    0
#define PC_RC_CTRL      0

#endif

enum APTP_RUN_STATUS
{
    APTP_RUN_STATUS_POSITION_ZERO,
    APTP_RUN_STATUS_ACC_SPEED,
    APTP_RUN_STATUS_CONST_SPEED,
    APTP_RUN_STATUS_DEC_SPEED,
    APTP_RUN_STATUS_WAIT_STOP
};
#define APTP_RUN_STATUS_INIT   APTP_RUN_STATUS_POSITION_ZERO

extern int32 frqPcOut;
extern int32 frqPcOutFrac;

extern int32 aptpRef;
extern int32 pCmdRef;

extern Uint32 pcRefStart;
extern Uint32 pcOrigin;

extern Uint16 aptpAbsZeroOk;
extern int32 aptpCurPos;

extern Uint32 ppr;

void PcRunCtrl(void);

void InitEQep2Gpio(void);
void InitSetPcEQep(void);
void pulseInCalcPcEQep(void);
void pulseInSamplePcQep(void);

extern int32 frqAimPg;
extern int16 jerk;

extern Uint16 pcCurrentPulse;
extern int32 pcError;

extern Uint16 aptpSetDisp;
extern Uint16 pcErrorDisp;
extern Uint16 pulseInFrqPg2Disp;

extern Uint16 pCmdRefBeforeEGear;
extern Uint16 pCmdRefAfterEGear;

void UpdatePcPara(void);

#define FRQ_UNIT        100
#define MULTIPLE_PULSE  4

extern volatile struct EQEP_REGS *pEQepRegsFvc;
extern volatile struct EQEP_REGS *pEQepRegsPc;

extern Uint16 bPcErrorOk;
extern Uint16 bPcErrorNear;

void InitPosCtrlPara(void);

#endif
