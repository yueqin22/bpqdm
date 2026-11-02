#ifndef __F_MENU_H__
#define __F_MENU_H__

#include "f_funcCode.h"

enum MENU_MODE
{
    MENU_MODE_NULL,
    MENU_MODE_BASE,
    MENU_MODE_USER,
    MENU_MODE_CHECK
};

Uint16 FcDigit2Bin(Uint16 value);

extern enum MENU_MODE menuModeTmp;
extern enum MENU_MODE menuMode;

extern Uint16 superFactoryPass;

enum MENU_MODE_OPERATE
{
    MENU_MODE_NONE,
    MENU_MODE_ON_QUICK
};
extern enum MENU_MODE_OPERATE menuModeStatus;

enum FAC_PASS_RANDOM_VIEW_OPERATE
{
    FAC_PASS_NONE,
    FAC_PASS_VIEW
};
extern enum FAC_PASS_RANDOM_VIEW_OPERATE facPassViewStatus;

#define DISP_OUT_CURRENT        4

#define DEBUG_RANDOM_FACPASS    0

#if F_DEBUG_RAM
#define COMPANY_PASSWORD        1430
#else
#define COMPANY_PASSWORD        1430
#endif

#if DEBUG_RANDOM_FACPASS
#define SUPER_USER_PASSWORD     superFactoryPass
#else
#define SUPER_USER_PASSWORD     0
#endif

#define SUPER_USER_PASSWORD_SOURCE1    0

#define ON_UP_KEY       ACC_SPEED
#define ON_DOWN_KEY     DEC_SPEED

#define STOP_DISPLAY_NUM 16
#define RUN_DISPLAY_NUM  32
#define COMM_PARA_NUM    33

#define USER_PARA_SAVE_FLAG1   1
#define USER_PARA_SAVE_FLAG2   0xFFFF - USER_PARA_SAVE_FLAG1

typedef struct tagSysMenu
{
    void (*onPrgFunc)();
    void (*onUpFunc)();
    void (*onEnterFunc)();
    void (*onMfkFunc)();
    void (*onDownFunc)();
    void (*onShiftFunc)();
    void (*onRunFunc)();
    void (*onStopFunc)();
    void (*onQuickFunc)();

    void (*UpdateDisplayBuffer)();
}sysMenu, *sysMenuHandle;

extern const sysMenu menu[];

#define MENU_LEVEL_NUM  6
enum MENU_LEVEL
{
    MENU_LEVEL_0,
    MENU_LEVEL_1,
    MENU_LEVEL_2,
    MENU_LEVEL_3,
    MENU_LEVEL_PWD_HINT,
    MENU_LEVEL_PWD_INPUT
};
extern enum MENU_LEVEL menuLevel;

struct MENU_ATTRIBUTE
{
    Uint16 operateDigit;

    Uint16 winkFlag;

    Uint16 winkFlagLed;

};
extern struct MENU_ATTRIBUTE menuAttri[];

struct INV_PARA
{
    Uint16 type;

    Uint16 ratingVoltage;
    Uint16 volLevel;
    Uint16 pointLimit;
    Uint16 bitAccDecStart;
};
extern struct INV_PARA invPara;

enum MENU0_DISP_STATUS
{
    MENU0_DISP_STATUS_RUN_STOP,
    MENU0_DISP_STATUS_UP_DOWN,
    MENU0_DISP_STATUS_ERROR,
    MENU0_DISP_STATUS_TUNE
};
extern enum MENU0_DISP_STATUS menu0DispStatus;

extern Uint16 accDecFrqPrcFlag;
extern Uint16 bFrqDigital;
extern Uint16 bFrqDigitalDone4WaitDelay;
extern Uint16 outCurrentDisp;
extern Uint16 outCurrentDispView;
extern Uint16 currentOcDisp;
extern Uint16 frqAimDisp;
extern Uint16 frqPLCDisp;
extern Uint16 frqAimPLCDisp;
extern Uint16 outVoltageDisp;
extern Uint16 frqRunDisp;
extern Uint16 pcOriginDisp;

extern Uint16 itDisp;
extern Uint16 frqDisp;
extern Uint16 frqXDisp;
extern Uint16 frqYDisp;
extern Uint32 torqueCurrentAct;

extern Uint16 * const pDispValueU0[];
extern Uint16 * const pOscValue[];
extern Uint16 const commDispIndex[];

typedef struct
{
    Uint16 flag;
    Uint16 data;

    Uint16 signal;
    Uint16 upper;
    Uint16 lower;
    int16  delta;

    Uint16 index;
} LIMIT_DEAL_STRUCT;
extern LIMIT_DEAL_STRUCT limitDealData;

Uint16 LimitDeal(Uint16 signal, Uint16 data, Uint16 upper, Uint16 lower, int16 delta);

Uint16 NoSameDeal(Uint16 index, const Uint16 funcIndex[], int16 number, int16 data, int16 upper, int16 lower, int16 delta);

Uint16 ModifyFunccodeUpDown(Uint16 index, Uint16 *data, int16 delta);
Uint16 ModifyFunccodeEnter(Uint16 index, Uint16 data);
Uint16 TorqueBoostDeal(Uint16 power);
void DispDataDeal(void);
void UpdateErrorDisplayBuffer(void);
void ClearRecordDeal(void);

void MenuModeDeal(void);
void UpdataFuncCodeGrade(Uint16 funcCodeGrade[]);

extern const int16 decNumber[];

extern Uint16 loadSpeedDisp;
extern Uint16 pidFuncRefDisp;
extern Uint16 pidFuncFdbDisp;
extern Uint16 pulseInFrqDisp;
extern Uint16 vfSeparateVol;
extern Uint16 vfSeprateVolAim;

void RestoreCompanyParaOtherDeal(void);
Uint16 GetFuncCodeInit(Uint16 index, Uint16 type);
Uint16 GetFuncCodeInitOriginal(Uint16 index);

LOCALD void LimitOtherCodeDeal(Uint16 index);

void MenuInit(void);

void UpdateInvType(void);

extern const union FUNC_ATTRIBUTE dispAttributeU0[];

enum CHECK_MENU_MODE_DEAL
{
    CHECK_MENU_MODE_DEAL_CMD,
    CHECK_MENU_MODE_DEAL_SERACHING,
    CHECK_MENU_MODE_DEAL_END_NONE,
    CHECK_MENU_MODE_DEAL_END_ONCE
};

extern Uint16 mainLoopTicker;

#endif
