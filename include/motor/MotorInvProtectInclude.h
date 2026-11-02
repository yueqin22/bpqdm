
#ifndef MOTORINV_PROTECT_INCLUDE_H
#define MOTORINV_PROTECT_INCLUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "MotorInclude.h"
#include "SystemDefine.h"
#include "MotorDefine.h"

#define  ERROR_OVER_CURRENT         (1L<< (1-1))
#define  ERROR_OVER_UDC             (1L<< (2-1))
#define  ERROR_RESISTER_HOT         (1L<< (3-1))
#define  ERROR_LOW_UDC              (1L<< (4-1))
#define  ERROR_INV_OVER_LAOD        (1L<< (5-1))

#define  ERROR_MOTOR_OVER_LOAD      (1L<< (6-1))
#define  ERROR_INPUT_LACK_PHASE     (1L<< (7-1))
#define  ERROR_OUTPUT_LACK_PHASE    (1L<< (8-1))
#define  ERROR_INV_TEMPERTURE       (1L<< (9-1))
#define  ERROR_RESISTANCE_CONTACK   (1L<< (10-1))

#define  ERROR_CURRENT_CHECK        (1L<< (11-1))
#define  ERROR_SHORT_EARTH          (1L<< (12-1))
#define  ERROR_LOAD_LOST            (1L<< (13-1))
#define  ERROR_TRIP_ZONE            (1L<< (14-1))
#define  ERROR_INIT_POS			    (1L<< (15-1))

#define  ERROR_SPEED_LOSE           (1L<< (16-1))
#define  ERROR_PROGRAM_LOGIC        (1L<< (17-1))
#define  ERROR_TUNE_FAIL            (1L<< (18-1))

#define  ERROR_ENCODER              (1L<< (19-1))

struct FAULT_INFOR_MODE_STRUCT_DEF{
    Uint    Fault1:4;
    Uint    Fault2:4;
    Uint    Fault3:4;
    Uint    Fault4:4;
};
typedef union FAULT_INFOR_MODE_UNION_DEF{
    Uint    all;
    struct  FAULT_INFOR_MODE_STRUCT_DEF bit;
}FAULT_STOP_MODE_UNION;
struct INV_ERROR_CODE_STRUCT{
    Uint    ErrorCode1;
    Uint    ErrorCode2;
};
union FAULT_CODE_UNION_DEF{
    Uint32  all;
    struct INV_ERROR_CODE_STRUCT  ErrorCodeStruct;
};
struct FAULT_CODE_INFOR_STRUCT_DEF{
    union FAULT_CODE_UNION_DEF ErrorCode;
    FAULT_STOP_MODE_UNION       ErrorInfo[5];

	Uint32                      LastErrorCode;
};
typedef struct OVER_LOAD_PROTECT_DEF{
   int		Cnt;
   long		FilterRealFreq;
   int		FilterInvCur;
   int		FilterMotorCur;
   BIT32_GROUP		InvTotal;
   BIT32_GROUP		MotorTotal;
}OVER_LOAD_PROTECT;
typedef struct PHASE_LOSE_STRUCT_DEF{
   Ulong	Time;
   Ulong	TotalU;
   Ulong	TotalV;
   Ulong	TotalW;
   Uint		Cnt;
   Uint     errMaxCur;
   Uint     errMinCur;
}PHASE_LOSE_STRUCT;
typedef struct INPUT_LOSE_STRUCT_DEF{
   Uint		Cnt;
   Uint		UpCnt;
   Uint		ErrCnt;
   Uint		CntRes;
   Uint		UpCntRes;
   Uint     ForeInvType;
}INPUT_LOSE_STRUCT;
typedef struct LOAD_LOSE_STRUCT_DEF {
	Uint 	ErrCnt;
    Uint    ChkLevel;
    Uint    ChkTime;
}LOAD_LOSE_STRUCT;
typedef struct FAN_CTRL_STRUCT_DEF{
   Uint		EnableCnt;
   Uint		RunCnt;
}FAN_CTRL_STRUCT;
struct CBC_FLAG_STRUCT{
    Uint16  CBC_U:1;
    Uint16  CBC_V:1;
    Uint16  CBC_W:1;
    Uint16  RESV:13;
};
typedef union CBC_FLAG_UNION_DEF {
   Uint16                  all;
   struct CBC_FLAG_STRUCT  bit;
}CBC_FLAG_UNION;
typedef struct CBC_PROTECT_STRUCT_DEF{
   CBC_FLAG_UNION	Flag;
   int		TotalU;
   int		TotalV;
   int     	TotalW;
   int     	Total;
   int		EnableFlag;
   int		CntU;
   int		CntV;
   int     	CntW;

   Uint     maxCBCTime;
   Uint     minCBCTime;
}CBC_PROTECT_STRUCT;

extern OVER_LOAD_PROTECT		gOverLoad;
extern PHASE_LOSE_STRUCT		gPhaseLose;
extern INPUT_LOSE_STRUCT		gInLose;
extern LOAD_LOSE_STRUCT		    gLoadLose;
extern FAN_CTRL_STRUCT			gFanCtrl;
extern Ulong					gBuffResCnt;
extern CBC_PROTECT_STRUCT		gCBCProtect;

void InvDeviceControl(void);
void OutputLoseAdd(void);
extern struct FAULT_CODE_INFOR_STRUCT_DEF  gError;
#ifdef __cplusplus
}
#endif

#endif
