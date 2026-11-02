
#ifndef MOTOR_SPEED_CHECK_INCLUDE_H
#define MOTOR_SPEED_CHECK_INCLUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "SystemDefine.h"
#include "MotorInclude.h"
#include "MotorDefine.h"

struct FEISU_SIGNAL1 {
    Uint16  UdcHigh:1;
	Uint16  CHECK_GET:1;
    Uint16  REM2:1;
    Uint16  REM3:1;
    Uint16  REM4:1;
    Uint16  REM5:1;
    Uint16  REM6:1;
    Uint16  REM7:1;
    Uint16  REM8:1;
    Uint16  REM9:1;
	Uint16  REM10:1;
	Uint16  REM11:1;
	Uint16  REM12:1;
	Uint16  SpdSig:1;
	Uint16  CHECK_BIT14:1;
	Uint16  CHECK_BIT15:1;
};

union FEISU_SIG1_DEF {
   Uint16                all;
   struct FEISU_SIGNAL1  bit;
};

struct FEISU_SIGNAL2 {
    Uint16  FEISUS2REM0:1;
    Uint16  FEISUS2REM1:1;
	Uint16  FEISUS2REM2:1;
    Uint16  FEISUS2REM3:1;
    Uint16  FEISUS2REM4:1;
    Uint16  FEISUS2REM5:1;
    Uint16  AlmostOver:1;
    Uint16  DelayOver:1;
    Uint16  FEISUS2REM8:1;
    Uint16  FEISUS2REM9:1;
    Uint16  FEISUS2REM10:1;
	Uint16  FEISUS2REM11:1;
	Uint16  FEISUS2REM12:1;
	Uint16  FEISUS2REM13:1;
	Uint16  FEISUS2REM14:1;
	Uint16  TwoCyclesOver:1;
};

union FEISU_SIG2_DEF {
   Uint16                all;
   struct FEISU_SIGNAL2  bit;
};

typedef struct FEISU_STRUCT_DEF{
	union 	FEISU_SIG1_DEF Case4Sig1;
	union 	FEISU_SIG2_DEF Case4Sig2;
	Uint	CycleCnt;
	Uint	UdcRiseCnt;
	Uint	UdcOvCnt;
	Uint	GuoduCnt;
	Uint	VoltCNT;
	Uint	AlmostCNT;
	Uint	LowFreqCNT;
	Uint	UdcBak;
	Uint	CheckMode;
	int		SpeedLast;
	Uint	Speed5hz;
	int		SpeedCheck;
	int		SpeedMaxPu;
	Uint	VoltCheck;
	Uint	VoltCheckAim;
    Uint    VoltTemp;
	int		CurDelta;
	int		Ger4A;
	int     CsrWtOver;
	int		UdcDelta;
	Uint	UdcOld;
}FEISU_STRUCT;

typedef struct FEISU_STRUCT_NEW_DEF{
	Uint  t_DetaTime;
	Uint  stop_time;
	Uint  inh_mag ;
	int   the_endspeed;
	int   open_cl;
	int   wsre;
	int   gDebugFlag;
	Uint  xisspeed;
	int   jicicg;
	int   jicics;
	int   dbctime;
	Uint  xwjdbcbz;
	int	  gTheta;
	int   gThteta1;
	int   gWs;
	int   gWs_out;
	long  ang_amu;
	int   Ialph;
	int   Ibeta;

    Uint  TimeTotal;
    Uint  VoltageDec;
}FEISU_STRUCT_NEW;

extern FEISU_STRUCT			    gFeisu;
extern FEISU_STRUCT_NEW         gFeisuNew;
extern PID_STRUCT				gSpeedCheckPID;
extern PID_STRUCT				gSpeedCheckPID1;

void RunCaseSpeedCheck(void);
void RunCaseSpeedCheck2(void);

#ifdef __cplusplus
}
#endif

#endif
