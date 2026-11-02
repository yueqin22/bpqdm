
#include "f_menu.h"
#include "f_main.h"
#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_io.h"
#include "f_ui.h"
#include "f_eeprom.h"
#include "f_comm.h"
#include "f_posCtrl.h"
#include "f_invPara.h"
#include "f_fcDeal.h"
#include "f_p2p.h"

#if F_DEBUG_RAM

#define DEBUG_F_USER_MENU_MODE              0
#define DEBUG_F_CHECK_MENU_MODE             0
#define DEBUG_F_NO_SAME                     0
#define DEBUG_F_DISP_DIDO_STATUS_SPECIAL    0
#define DEBUG_F_GROUP_HIDE                  0
#define DEBUG_F_MOTOR_FUNCCODE1             0
#define DEBUG_F_MFK                         0
#define DEBUG_F_PASSWORD                    0
#define DEBUG_F_ERROR_TUNE_USE_SHIFT        0
#define DEBUG_FRQ_POINT                     0
#define DEBUG_ACC_DEC_TIME_POINT            0

#elif 1

#define DEBUG_F_NO_SAME                     1
#define DEBUG_F_USER_MENU_MODE              1
#define DEBUG_F_CHECK_MENU_MODE             1
#define DEBUG_F_DISP_DIDO_STATUS_SPECIAL    1
#define DEBUG_F_GROUP_HIDE                  1
#define DEBUG_F_MOTOR_FUNCCODE1             1
#define DEBUG_F_MFK                         1
#define DEBUG_F_PASSWORD                    1
#define DEBUG_F_ERROR_TUNE_USE_SHIFT        1
#define DEBUG_FRQ_POINT                     1
#define DEBUG_ACC_DEC_TIME_POINT            1

#endif

#define MENU_DIGIT_NUM                  5
#define MENU_OPERATE_DIGIT_GRADE        1
#define MENU_OPERATE_DIGIT_HUNDRED      2
#define MENU_OPERATE_DIGIT_GROUP        3
#define MENU_OPERATE_DIGIT_MAX          4

#define MENU_GROUP_GRADE_BASE           100
#define MENU_GROUP_LETTER_NUM           16
#define MENU_STEP_ONES                  1
#define MENU_STEP_TENS                  10

#define MENU_WINK_FLAG_SHIFT            3
#define MENU_WINK_FLAG_ALT              0x01
#define MENU_WINK_FLAG_HINT             0x08
#define MENU_WINK_FLAG_STOP             0xF8

#define MENU_PWD_DIGIT_NUM              4
#define MENU_PWD_LIMIT                  65535

#define MENU_DIDO_BIT_MASK              0x000FFFFF
#define MENU_DIFUNC_BYTE_MASK           0x00FF
#define MENU_DIFUNC_SPLIT_SHIFT         1
#define MENU_DIFUNC_HIGH_SHIFT          9

#define MENU_VIEW_ATTRIBUTE_PLC         0x1040

#define MENU_FRQ_UNIT_NIBBLE_MASK       0x000F
#define MENU_FRQ_UNIT_NIBBLE_SHIFT      4

#define MENU_PID_DISP_ROUND             (1 << 14)
#define MENU_PID_DISP_SHIFT             15
#define MENU_SPEED_DISP_UNIT            10000
#define MENU_PULSE_FRQ_ROUND            5
#define MENU_PULSE_FRQ_DIV              10
#define MENU_TORQUE_CURRENT_SHIFT       12

const int16 decNumber[5] = {1, 10, 100, 1000, 10000};

const int16 hexNumber[4] = {0x0001, 0x0010, 0x0100, 0x1000};
const int16 deltaK[3] = {0, 1, -1};

int16 userMenuModeFcIndex;

#if 0
struct RESTORE_COMPANY_PARA_EXCEPT_INDEX
{
    Uint16 start;
    Uint16 end;
};

#define RESTORE_COMPANY_PARA_EXCEPT_NUMBER  6
static const struct RESTORE_COMPANY_PARA_EXCEPT_INDEX exceptRestoreSeries[RESTORE_COMPANY_PARA_EXCEPT_NUMBER] =
{
{GetCodeIndex(funcCode.group.ff[0]), GetCodeIndex(funcCode.group.ff[FFNUM-1])},
{GetCodeIndex(funcCode.group.fp[0]), GetCodeIndex(funcCode.group.fp[FPNUM-1])},
{GetCodeIndex(funcCode.group.f1[0]), GetCodeIndex(funcCode.group.f1[F1NUM-2])},
{GetCodeIndex(funcCode.group.a3[0]), GetCodeIndex(funcCode.group.a3[F1NUM-2])},
{GetCodeIndex(funcCode.group.ae[0]), GetCodeIndex(funcCode.group.ae[AENUM-1])},
{GetCodeIndex(funcCode.code.errorLatest1), LAST_ERROR_RECORD_INDEX},
};
#endif

Uint16 GetDispDigits(Uint16 index);

struct FC_MOTOR_DEBUG
{
    Uint16 fc;
    Uint16 u;
};
struct FC_MOTOR_DEBUG motorDebugFc;

enum MENU0_DISP_STATUS menu0DispStatus;
void UpdateMenu0DispStatus(void);

struct MENU_FUNC_CODE
{
    Uint16 group;
    Uint16 grade;
};
struct MENU_FUNC_CODE menuFc[MENU_MODE_MAX+1];

enum MENU_MODE menuMode;
enum MENU_MODE menuModeTmp;
enum MENU_MODE menuModeOld;

enum MENU_MODE_OPERATE menuModeStatus;
enum FAC_PASS_RANDOM_VIEW_OPERATE facPassViewStatus;

#if DEBUG_F_NO_SAME

#define FRQ_SRC_NO_SAME_NUMER   2
const Uint16 frqSrcFuncIndex[] =
{
    GetCodeIndex(funcCode.code.frqXSrc),
    GetCodeIndex(funcCode.code.frqYSrc),
};

const Uint16 diFuncIndex[] =
{
    GetCodeIndex(funcCode.code.diFunc[0]),
    GetCodeIndex(funcCode.code.diFunc[1]),
    GetCodeIndex(funcCode.code.diFunc[2]),
    GetCodeIndex(funcCode.code.diFunc[3]),
    GetCodeIndex(funcCode.code.diFunc[4]),

    GetCodeIndex(funcCode.code.diFunc[5]),
    GetCodeIndex(funcCode.code.diFunc[6]),
    GetCodeIndex(funcCode.code.diFunc[7]),
    GetCodeIndex(funcCode.code.diFunc[8]),
    GetCodeIndex(funcCode.code.diFunc[9]),

    GetCodeIndex(funcCode.code.vdiFunc[0]),
    GetCodeIndex(funcCode.code.vdiFunc[1]),
    GetCodeIndex(funcCode.code.vdiFunc[2]),
    GetCodeIndex(funcCode.code.vdiFunc[3]),
    GetCodeIndex(funcCode.code.vdiFunc[4]),

    GetCodeIndex(funcCode.code.aiAsDiFunc[0]),
    GetCodeIndex(funcCode.code.aiAsDiFunc[1]),
    GetCodeIndex(funcCode.code.aiAsDiFunc[2]),
};
#endif

#define DECIMAL_DISPLAY_UPDATE_TIME     6
#define UP_DOWN_DEAL_DONE_TIME          800

#define ONE_PLACE           0
#define TEN_PLACE           1
#define HUNDRED_PLACE       2
#define THOUSAND_PLACE      3
#define TEN_THOUSAND_PLACE  4

enum MENU_LEVEL menuLevel;
Uint16 menu3Number;
Uint16 menuPwdNumber;

struct CURRENT_FUNC_CODE
{
    Uint16 index;

    Uint16 group;
    Uint16 grade;
};
struct CURRENT_FUNC_CODE curFc;
Uint16 curFcDispDigits;

LOCALF Uint16 ticker4LowerDisp;

Uint16 superFactoryPass;
Uint16 groupHidePwdStatus;
Uint16 groupHideChkOkFlag;

#define FC_READ_ONLY_FLAG                                               \
    ((!funcCode.code.userPasswordReadOnly) ||                           \
    (curFc.index == GetCodeIndex(funcCode.code.userPassword)) ||        \
    (curFc.index == GetCodeIndex(funcCode.code.userPasswordReadOnly))   \
    )

#define IsWritable(attribute)                                       \
 ((FC_READ_ONLY_FLAG) &&                                            \
 ((ATTRIBUTE_READ_AND_WRITE == (attribute).bit.writable) ||         \
 ((ATTRIBUTE_READ_ONLY_WHEN_RUN == (attribute).bit.writable) &&     \
 (!runFlag.bit.run)))                                               \
)                                                                   \

enum FACTORY_PWD_STATUS
{
    FACTORY_PWD_LOCK,
    FACTORY_PWD_UNLOCK
};
LOCALF enum FACTORY_PWD_STATUS factoryPwdStatus;

Uint16 accDecFrqPrcFlag;
Uint16 bFrqDigitalDone4WaitDelay;
LOCALF Uint16 accDecFrqTicker;
Uint16 frqDisp;
Uint16 frqAimDisp;
Uint16 frqPLCDisp;
Uint16 frqAimPLCDisp;
Uint16 pidFuncRefDisp;
Uint16 pidFuncFdbDisp;
Uint16 outVoltageDisp;
Uint16 outCurrentDisp;
Uint16 itDisp;
Uint16 loadSpeedDisp;
Uint16 currentOcDisp;
Uint16 pulseInFrqDisp;
Uint16 frqRunDisp;
Uint16 pcOriginDisp;
Uint16 frqXDisp;
Uint16 frqYDisp;
Uint32 torqueCurrentAct;

#include "f_funcCode_disp.c"

LOCALD void MenuOnRun(void);
LOCALD void MenuOnStop(void);
LOCALD void MenuOnMfk(void);

LOCALD void Menu0OnPrg(void);
void Menu1OnPrg(void);
LOCALD void Menu2OnPrg(void);
LOCALD void Menu3OnPrg(void);

LOCALD void Menu0OnUp(void);
LOCALD void Menu1OnUp(void);
LOCALD void Menu2OnUp(void);
LOCALD void Menu3OnUp(void);

LOCALD void Menu0OnDown(void);
LOCALD void Menu1OnDown(void);
LOCALD void Menu2OnDown(void);
LOCALD void Menu3OnDown(void);

LOCALD void Menu0OnEnter(void);
LOCALD void Menu1OnEnter(void);
LOCALD void Menu2OnEnter(void);
LOCALD void Menu3OnEnter(void);

LOCALD void Menu0OnShift(void);
LOCALD void Menu1OnShift(void);
LOCALD void Menu2OnShift(void);
LOCALD void Menu3OnShift(void);

LOCALD void MenuOnQuick(void);

LOCALD void Menu0OnUpDown(void);
LOCALD void Menu1OnUpDown(Uint16 flag);
LOCALD void Menu2OnUpDown(Uint16 flag);
LOCALD void Menu3OnUpDown(Uint16 flag);

LOCALD void UpdateMenu0DisplayBuffer(void);
LOCALD void UpdateMenu1DisplayBuffer(void);
LOCALD void UpdateMenu2DisplayBuffer(void);
LOCALD void UpdateMenu3DisplayBuffer(void);
LOCALD void UpdateDisplayBufferAttribute(const Uint16 data, const union FUNC_ATTRIBUTE attribute);
void UpdateDisplayBufferVisualIoStatus(Uint32 value);
void UpdateDisplayBufferVisualDiFunc(Uint16 valueH, Uint32 valueL);
LOCALD void UpdateErrorDisplayBuffer(void);
LOCALD void UpdateTuneDisplayBuffer(void);

LOCALD void MenuPwdOnPrg(void);
LOCALD void MenuPwdHintOnUp(Uint16 flag);
LOCALD void MenuPwdHint2Input(void);
LOCALD void MenuPwdHintOnDown(Uint16 flag);
LOCALD void MenuPwdHintOnShift(void);
LOCALD void MenuPwdHintOnQuick(void);
LOCALD void UpdateMenuPwdHintDisplayBuffer(void);

LOCALD void MenuPwdInputOnPrg(void);
LOCALD void MenuPwdInputOnUp(void);
LOCALD void MenuPwdInputOnEnter(void);
LOCALD void MenuPwdInputOnDown(void);
LOCALD void MenuPwdInputOnShift(void);
LOCALD void MenuPwdInputOnQuick(void);
LOCALD void UpdateMenuPwdInputDisplayBuffer(void);

void MenuPwdInputOnUpDown(Uint16 flag);
void MenuModeSwitch(void);

struct MENU_ATTRIBUTE menuAttri[MENU_LEVEL_NUM];
#if F_DEBUG_RAM
#pragma DATA_SECTION(menu, "const_zone");
#endif
const sysMenu menu[MENU_LEVEL_NUM] =
{
    {Menu0OnPrg,        Menu0OnUpDown,      Menu0OnEnter,
     MenuOnMfk,         Menu0OnUpDown,      Menu0OnShift,
     MenuOnRun,         MenuOnStop,         MenuOnQuick,
     UpdateMenu0DisplayBuffer},

    {Menu1OnPrg,        Menu1OnUp,          Menu1OnEnter,
     MenuOnMfk,         Menu1OnDown,        Menu1OnShift,
     MenuOnRun,         MenuOnStop,         MenuOnQuick,
     UpdateMenu1DisplayBuffer},

    {Menu2OnPrg,        Menu2OnUp,          Menu2OnEnter,
     MenuOnMfk,         Menu2OnDown,        Menu2OnShift,
     MenuOnRun,         MenuOnStop,         MenuOnQuick,
     UpdateMenu2DisplayBuffer},

    {Menu3OnPrg,        Menu3OnUp,          Menu3OnEnter,
     MenuOnMfk,         Menu3OnDown,        Menu3OnShift,
     MenuOnRun,         MenuOnStop,         MenuOnQuick,
     UpdateMenu3DisplayBuffer},

    {MenuPwdOnPrg,      MenuPwdHint2Input,  MenuPwdHint2Input,
     MenuOnMfk,         MenuPwdHint2Input,  MenuPwdHintOnShift,
     MenuOnRun,         MenuOnStop,         MenuPwdHintOnQuick,
     UpdateMenuPwdHintDisplayBuffer},

    {MenuPwdOnPrg,      MenuPwdInputOnUp,   MenuPwdInputOnEnter,
     MenuOnMfk,         MenuPwdInputOnDown, MenuPwdInputOnShift,
     MenuOnRun,         MenuOnStop,         MenuPwdInputOnQuick,
     UpdateMenuPwdInputDisplayBuffer},
};

LOCALD Uint16 OverTurnDeal(Uint16 data, Uint16 upper, Uint16 lower, Uint16 flag);

LOCALD void cycleShiftDeal(Uint16 flag);

struct GROUP_DISPLAY
{
    Uint16 dispF;
    Uint16 disp0;
};
struct GROUP_DISPLAY groupDisplay;
void UpdateGroupDisplay(Uint16 group);

Uint16 GroupUpDown(const Uint16 funcCodeGrade[], Uint16 group, Uint16 flag);
void DealUserMenuModeGroupGrade(Uint16 flag);
void GetGroupGrade(Uint16 index);

Uint16 checkMenuModeCmd;
Uint16 checkMenuModePara;
enum CHECK_MENU_MODE_DEAL checkMenuModeDealStatus;
Uint16 checkMenuModeSerachNone;

void DealCheckMenuModeGroupGrade(Uint16 flag);

Uint16 LimitOverTurnDeal(const Uint16 limit[], Uint16 data, Uint16 upper, Uint16 low, Uint16 flag);
void MotorDebugFcDeal(void);
void fghldf(Uint16 dest[], const Uint16 src[], Uint16 length);
void Menu0AddMenuLevel(void);
Uint16 ValidateTuneCmd(Uint16 value, Uint16 motorIndex);

void GroupHideDeal(Uint16 funcCodeGrade[]);

static Uint16 HandleQuickMenuSwitch(void)
{
    if (MENU_MODE_ON_QUICK != menuModeStatus)
    {
        return 0;
    }

    MenuModeSwitch();
    return 1;
}

static void SetWinkFlag(enum MENU_LEVEL level)
{
    menuAttri[level].winkFlag = (Uint16)(MENU_WINK_FLAG_ALT
                                         << (MENU_WINK_FLAG_SHIFT
                                             + menuAttri[level].operateDigit));
}

LOCALF void MenuOnRun(void)
{
    ;
}

LOCALF void MenuOnStop(void)
{
    ;
}

void MenuOnQuick(void)
{
    Uint16 digit[MENU_DIGIT_NUM];
    enum MENU_MODE menuModeNext[MENU_MODE_MAX];

    if (!funcCode.code.menuMode)
    {
        menuModeTmp = MENU_MODE_BASE;
        return;
    }

    if (MENU_MODE_ON_QUICK != menuModeStatus)
    {
        menuModeStatus = MENU_MODE_ON_QUICK;
        return;
    }

    GetNumberDigit1(digit, funcCode.code.menuMode);

    menuModeNext[0] = digit[0] ? MENU_MODE_USER : MENU_MODE_CHECK;
    menuModeNext[1] = digit[1] ? MENU_MODE_CHECK : MENU_MODE_BASE;
    menuModeNext[2] = MENU_MODE_BASE;

    menuModeTmp = menuModeNext[((Uint16)menuModeTmp) - 1];
    menuModeStatus = MENU_MODE_ON_QUICK;
}

LOCALF void Menu1OnUp(void)
{
    Menu1OnUpDown(ON_UP_KEY);
}

LOCALF void Menu2OnUp(void)
{
    Menu2OnUpDown(ON_UP_KEY);
}

LOCALF void Menu3OnUp(void)
{
    Menu3OnUpDown(ON_UP_KEY);
}

LOCALF void Menu1OnDown(void)
{
    Menu1OnUpDown(ON_DOWN_KEY);
}

LOCALF void Menu2OnDown(void)
{
    Menu2OnUpDown(ON_DOWN_KEY);
}

LOCALF void Menu3OnDown(void)
{
    Menu3OnUpDown(ON_DOWN_KEY);
}

LOCALF void MenuOnMfk(void)
{
#if DEBUG_F_MFK
    if (tuneCmd)
    {
        return;
    }

    switch (funcCode.code.mfkKeyFunc)
    {
        case FUNCCODE_mfkKeyFunc_SWITCH:
            if (FUNCCODE_runSrc_PANEL != funcCode.code.runSrc)
            {
                keyFunc = KEY_SWITCH;
            }
            break;

        case FUNCCODE_mfkKeyFunc_REVERSE:
            if (FUNCCODE_runSrc_PANEL == runSrc)
            {
                keyFunc = KEY_REV;
            }
            break;

        case FUNCCODE_mfkKeyFunc_FWD_JOG:
            keyFunc = KEY_FWD_JOG;
            break;

        case FUNCCODE_mfkKeyFunc_REV_JOG:
            keyFunc = KEY_REV_JOG;
            break;

        default:
            break;
    }
#endif
}

LOCALF void Menu0OnPrg(void)
{
    if (tuneCmd)
    {
        if (!runFlag.bit.tune)
        {
            menuLevel = MENU_LEVEL_2;
            tuneCmd = 0;
        }
        else
        {
            Menu0AddMenuLevel();
        }
    }
#if DEBUG_F_PASSWORD
    else if ((funcCode.code.userPassword) && (menuMode != MENU_MODE_USER))
    {
        menuLevel = MENU_LEVEL_PWD_HINT;
    }
    else
#endif
    {
        Menu0AddMenuLevel();
    }

    menuAttri[menuLevel].operateDigit = 0;
}

LOCALF void Menu0OnEnter(void)
{
    HandleQuickMenuSwitch();
}

void MenuModeSwitch(void)
{
    menuModeOld = menuMode;
    menuModeStatus = MENU_MODE_NONE;

    menuFc[menuMode].group = curFc.group;
    menuFc[menuMode].grade = curFc.grade;
    menuMode = menuModeTmp;
    curFc.group = menuFc[menuModeTmp].group;
    curFc.grade = menuFc[menuModeTmp].grade;

    if ((menuModeOld != menuMode) && (FC_GROUP_FACTORY == curFc.group))
    {
        factoryPwdStatus = FACTORY_PWD_LOCK;
        curFc.grade = 0;
    }

    MenuModeDeal();

    if (menuModeOld == MENU_MODE_USER)
    {
        Menu0OnPrg();
    }
    else if ((menuLevel == MENU_LEVEL_3) && (menuModeOld != menuMode))
    {
        menuLevel = MENU_LEVEL_2;
    }
    else if (((menuLevel == MENU_LEVEL_0) || (menuLevel == MENU_LEVEL_1))
             && (MENU_MODE_CHECK == menuMode))
    {
        Menu0OnPrg();
    }
    else if (((menuLevel == MENU_LEVEL_0) || (menuLevel == MENU_LEVEL_1))
             && (MENU_MODE_USER == menuMode))
    {
        Menu0OnPrg();
    }
    else if (menuLevel == MENU_LEVEL_0)
    {
        Menu0OnPrg();
    }

    if (menuModeOld != menuMode)
    {
        menuAttri[menuLevel].operateDigit = 0;
    }
}

LOCALF void Menu0OnUpDown(void)
{
    if (tuneCmd)
    {
        return;
    }

    accDecFrqPrcFlag = ACC_DEC_FRQ_WAIT;
    accDecFrqTicker = 0;
    frqKeyUpDownDelta = upDownDelta;
}

LOCALF void Menu0OnShift(void)
{
    ticker4LowerDisp = 0;

#if DEBUG_F_ERROR_TUNE_USE_SHIFT

    if (MENU0_DISP_STATUS_RUN_STOP != menu0DispStatus)
    {
        menu0DispStatus = MENU0_DISP_STATUS_RUN_STOP;
    }
    else
#elif 1
    if ((!tuneCmd) && (!errorCode))
#endif
    {
        cycleShiftDeal(1);
    }
}

void Menu1OnPrg(void)
{
    menuLevel = MENU_LEVEL_0;
    groupHideChkOkFlag = 0;

    ticker4LowerDisp = 0;
}

LOCALF void Menu1OnEnter()
{
    if (HandleQuickMenuSwitch())
    {
        return;
    }

    menuLevel = MENU_LEVEL_2;
    menuAttri[menuLevel].operateDigit = 0;

    if (FC_GROUP_FACTORY == curFc.group)
    {
        curFc.grade = 0;
    }
}

LOCALF void Menu1OnUpDown(Uint16 flag)
{
    curFc.group = GroupUpDown(funcCodeGradeCurMenuMode, curFc.group, flag);

    curFc.grade = 0;
}

LOCALF void Menu1OnShift(void)
{
    ;
}

LOCALF void Menu2OnPrg(void)
{
    menuLevel = MENU_LEVEL_1;

    if ((MENU_MODE_CHECK == menuMode) || (MENU_MODE_USER == menuMode))
    {
        Menu1OnPrg();
    }

    factoryPwdStatus = FACTORY_PWD_LOCK;
}

LOCALF void Menu2OnEnter(void)
{
    if (HandleQuickMenuSwitch())
    {
        return;
    }

    if ((MENU_MODE_CHECK == menuMode) && (checkMenuModeSerachNone))
    {
        return;
    }

    if (curFc.group != FC_GROUP_FACTORY)
    {
        factoryPwdStatus = FACTORY_PWD_LOCK;
    }

    menuAttri[MENU_LEVEL_2].operateDigit = 0;
    curFc.index = GetGradeIndex(curFc.group, curFc.grade);

    menuLevel = MENU_LEVEL_3;
    menuAttri[MENU_LEVEL_3].operateDigit = 0;
    menu3Number = funcCode.all[curFc.index];
    curFcDispDigits = GetDispDigits(curFc.index);
}

LOCALF void Menu2OnUpDown(Uint16 flag)
{
    if (MENU_MODE_USER == menuMode)
    {
        DealUserMenuModeGroupGrade(flag);
    }
    else if (MENU_MODE_CHECK == menuMode)
    {
        checkMenuModeCmd = 1;
        checkMenuModePara = flag;
    }
    else if (menuAttri[MENU_LEVEL_2].operateDigit >= MENU_OPERATE_DIGIT_GROUP)
    {
        Menu1OnUpDown(flag);
    }
    else
    {
        int16 delta = MENU_STEP_ONES;
        Uint16 tmp;

        if ((FC_GROUP_FACTORY == curFc.group) && (FACTORY_PWD_LOCK == factoryPwdStatus))
        {
            Menu2OnEnter();
            return;
        }

#if 1
        tmp = OverTurnDeal(curFc.grade, funcCodeGradeCurMenuMode[curFc.group] - 1, 0, flag);
        if (curFc.grade != tmp)
        {
            curFc.grade = tmp;
        }
        else
        {
            if (MENU_OPERATE_DIGIT_GRADE == menuAttri[MENU_LEVEL_2].operateDigit)
            {
                delta = MENU_STEP_TENS;
            }

            if (ON_DOWN_KEY == flag)
            {
                delta = -delta;
            }

            curFc.grade = LimitDeal(0, curFc.grade,
                                    funcCodeGradeCurMenuMode[curFc.group] - 1, 0, delta);
        }
#elif 1
        if (1 == menuAttri[MENU_LEVEL_2].operateDigit)
            delta = 10;

        if (ON_DOWN_KEY == flag)
            delta = -delta;

        curFc.grade += delta;
        if ((int16)curFc.grade >= (int16)(funcCodeGradeCurMenuMode[curFc.group])
        {
            curFc.grade = 0;
        }
        else if ((int16)curFc.grade < 0)
        {
            curFc.grade = funcCodeGradeCurMenuMode[curFc.group] - 1;
        }
#endif
    }
}

LOCALF void Menu2OnShift(void)
{
    if (MENU_MODE_BASE != menuMode)
    {
        return;
    }

    if (0 == menuAttri[MENU_LEVEL_2].operateDigit)
    {
        menuAttri[MENU_LEVEL_2].operateDigit = MENU_OPERATE_DIGIT_GROUP;
    }
    else if (MENU_OPERATE_DIGIT_GRADE == menuAttri[MENU_LEVEL_2].operateDigit)
    {
        menuAttri[MENU_LEVEL_2].operateDigit = 0;
    }
    else if (MENU_OPERATE_DIGIT_GROUP == menuAttri[MENU_LEVEL_2].operateDigit)
    {
        menuAttri[MENU_LEVEL_2].operateDigit = MENU_OPERATE_DIGIT_GRADE;
    }
}

LOCALF void Menu3OnPrg(void)
{
    menuLevel = MENU_LEVEL_2;
    menuAttri[MENU_LEVEL_2].operateDigit = 0;
}

LOCALF void Menu3OnEnter(void)
{
    Uint16 writable = funcCodeAttribute[curFc.index].attribute.bit.writable;

    if (HandleQuickMenuSwitch())
    {
        return;
    }

    if ((FUNCCODE_RW_MODE_NO_OPERATION != funcCodeRwMode)
        && (FACTORY_PWD_INDEX != curFc.index)
        && (ATTRIBUTE_READ_ONLY_ANYTIME != writable))
    {
        return;
    }

    if ((runFlag.bit.run)
        && (ATTRIBUTE_READ_ONLY_WHEN_RUN == writable)
        && (funcCode.all[curFc.index] != menu3Number))
    {
        return;
    }

    if (curFc.group >= FUNCCODE_GROUP_U0)
    {
        return;
    }

#if DEBUG_F_RESTORE_COMPANY_PARA_DEAL

    if (GetCodeIndex(funcCode.code.paraInitMode) == curFc.index)
    {
        if (FUNCCODE_paraInitMode_CLEAR_RECORD == menu3Number)
        {
            ClearRecordDeal();
        }
        else
        {
            if (FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA == menu3Number)
            {
                funcCodeRwModeTmp = FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA;
            }
            else if (FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA_ALL == menu3Number)
            {

            }
            else if (FUNCCODE_paraInitMode_SAVE_USER_PARA == menu3Number)
            {
                funcCodeRwModeTmp = FUNCCODE_paraInitMode_SAVE_USER_PARA;
            }
            else if (FUNCCODE_paraInitMode_RESTORE_USER_PARA == menu3Number)
            {
                if ((funcCode.code.saveUserParaFlag1 == USER_PARA_SAVE_FLAG1)
                    && (funcCode.code.saveUserParaFlag2 == USER_PARA_SAVE_FLAG2))
                {

                    funcCodeRwModeTmp = FUNCCODE_paraInitMode_RESTORE_USER_PARA;
                }
                else
                {

                    return;
                }
            }
        }
    }
    else
#endif
    if (FACTORY_PWD_INDEX == curFc.index)
    {
        if (COMPANY_PASSWORD != menu3Number)
        {
            menu3Number = 0;
            menuAttri[MENU_LEVEL_3].operateDigit = 0;
            return;
        }

        factoryPwdStatus = FACTORY_PWD_UNLOCK;
    }
    else if (GetCodeIndex(funcCode.code.tuneCmd) == curFc.index)
    {
        if (COMM_ERR_PARA == ValidateTuneCmd(menu3Number, MOTOR_SN_1))
        {
            return;
        }
    }
    else if (GetCodeIndex(funcCode.code.motorFcM2.tuneCmd) == curFc.index)
    {
        if (COMM_ERR_PARA == ValidateTuneCmd(menu3Number, MOTOR_SN_2))
        {
            return;
        }
    }
    else if (GetCodeIndex(funcCode.code.motorFcM3.tuneCmd) == curFc.index)
    {
        if (COMM_ERR_PARA == ValidateTuneCmd(menu3Number, MOTOR_SN_3))
        {
            return;
        }
    }
    else if (GetCodeIndex(funcCode.code.motorFcM4.tuneCmd) == curFc.index)
    {
        if (COMM_ERR_PARA == ValidateTuneCmd(menu3Number, MOTOR_SN_4))
        {
            return;
        }
    }
    else if (ATTRIBUTE_READ_ONLY_ANYTIME != writable)
    {
        if (COMM_ERR_PARA == ModifyFunccodeEnter(curFc.index, menu3Number))
        {
            return;
        }

        SaveOneFuncCode(curFc.index);
    }

    if (MENU_MODE_USER == menuMode)
    {
        DealUserMenuModeGroupGrade(ON_UP_KEY);
    }
    else if (MENU_MODE_CHECK == menuMode)
    {
        checkMenuModeCmd = 1;
        checkMenuModePara = ON_UP_KEY;
    }
    else if (++curFc.grade >= funcCodeGradeCurMenuMode[curFc.group])
    {
        curFc.grade = 0;
    }

    if (tuneCmd
        && ((curFc.index == GetCodeIndex(funcCode.code.tuneCmd))
            || (curFc.index == GetCodeIndex(funcCode.code.motorFcM2.tuneCmd))
            || (curFc.index == GetCodeIndex(funcCode.code.motorFcM3.tuneCmd))
            || (curFc.index == GetCodeIndex(funcCode.code.motorFcM4.tuneCmd))))
    {
        menuLevel = MENU_LEVEL_0;
    }
    else
    {
        menuLevel = MENU_LEVEL_2;
        menuAttri[MENU_LEVEL_2].operateDigit = 0;
    }
}

Uint16 ValidateTuneCmd(Uint16 value, Uint16 motorsN)
{
    if (motorsN != motorSn)
    {
        return COMM_ERR_PARA;
    }

    if (MOTOR_TYPE_PMSM == motorFc.motorPara.elem.motorType)
    {
        if ((FUNCCODE_tuneCmd_PMSM_11 != value)
            && (FUNCCODE_tuneCmd_PMSM_12 != value)
            && (FUNCCODE_tuneCmd_PMSM_13 != value))
        {
            return COMM_ERR_PARA;
        }
    }
    else
    {
        if ((FUNCCODE_tuneCmd_ACI_STATIC != value)
            && (FUNCCODE_tuneCmd_ACI_WHOLE != value))
        {
            return COMM_ERR_PARA;
        }
    }

    if ((errorCode == ERROR_NONE) && (FUNCCODE_runSrc_PANEL == runSrc))
    {
        tuneCmd = value;
        return COMM_ERR_NONE;
    }

    return COMM_ERR_PARA;
}

LOCALF void Menu3OnUpDown(Uint16 flag)
{
    int16 delta;
    Uint16 flag1 = 1;

    if (curFc.group >= FUNCCODE_GROUP_U0)
    {
        Menu2OnUpDown(flag);
        curFc.index = GetGradeIndex(curFc.group, curFc.grade);

        flag1 = 0;
    }
#if DEBUG_F_USER_MENU_MODE
    else if (USER_MENU_GROUP == curFc.group)
    {
        if (MENU_OPERATE_DIGIT_HUNDRED == menuAttri[MENU_LEVEL_3].operateDigit)
        {
            Uint16 group;
            Uint16 funcCodeGrade[FUNCCODE_GROUP_NUM];

            memcpy(funcCodeGrade, funcCodeGradeAll, (FUNCCODE_GROUP_NUM));
            funcCodeGrade[FC_GROUP_FACTORY] = 0;
            funcCodeGrade[USER_MENU_GROUP] = 0;

#if DEBUG_F_MOTOR_FUNCCODE
            MotorDebugFcDeal();
            funcCodeGrade[FUNCCODE_GROUP_CF] = motorDebugFc.fc;
            funcCodeGrade[FUNCCODE_GROUP_UF] = motorDebugFc.u;
#endif

            group = GroupUpDown(funcCodeGrade, menu3Number / MENU_GROUP_GRADE_BASE, flag);
            menu3Number = group * MENU_GROUP_GRADE_BASE;

            flag1 = 0;
        }
    }
#endif

    if (!flag1)
    {
        return;
    }

    if (ATTRIBUTE_MULTI_LIMIT_HEX != funcCodeAttribute[curFc.index].attribute.bit.multiLimit)
    {
        delta = decNumber[menuAttri[MENU_LEVEL_3].operateDigit];
    }
    else
    {
        delta = hexNumber[menuAttri[MENU_LEVEL_3].operateDigit];
    }

    if (ON_DOWN_KEY == flag)
    {
        delta = -delta;
    }

    ModifyFunccodeUpDown(curFc.index, &menu3Number, delta);
}

LOCALF void Menu3OnShift(void)
{
    Uint16 max = curFcDispDigits;

#if DEBUG_F_USER_MENU_MODE
    if (USER_MENU_GROUP == curFc.group)
    {
        max = MENU_OPERATE_DIGIT_GROUP;
    }
#endif

    if (!menuAttri[MENU_LEVEL_3].operateDigit)
    {
        menuAttri[MENU_LEVEL_3].operateDigit = max - 1;
    }
    else
    {
        menuAttri[MENU_LEVEL_3].operateDigit--;
    }
}

Uint16 LimitDeal(Uint16 signal, Uint16 data, Uint16 upper, Uint16 lower, int16 delta)
{
    int32 dataSigned;
    int32 upperSigned;
    int32 lowerSigned;

    if (signal)
    {
        dataSigned = (int32)(int16)data;
        upperSigned = (int32)(int16)upper;
        lowerSigned = (int32)(int16)lower;
    }
    else
    {
        dataSigned = (int32)data;
        upperSigned = (int32)upper;
        lowerSigned = (int32)lower;
    }

    dataSigned += delta;

    if (dataSigned > upperSigned)
    {
        dataSigned = upperSigned;
    }

    if (dataSigned < lowerSigned)
    {
        dataSigned = lowerSigned;
    }

    return (Uint16)dataSigned;
}

Uint16 NoSameDeal(Uint16 index, const Uint16 funcIndex[], int16 number, int16 data, int16 upper, int16 lower, int16 delta)
{
#if DEBUG_F_NO_SAME
    Uint16 i;

#if 1

    if ((data == upper) && (delta > 0))
    {
        data = lower;
    }
    else if ((data == lower) && (delta < 0))
    {
        data = upper;
    }
    else
#endif
    {
        data = LimitDeal(0, data, upper, lower, delta);
    }

    if (data == lower)
    {
        return data;
    }

    for (;;)
    {
        for (i = 0; i < number; i++)
        {
            if ((funcIndex[i] == index) || (data != funcCode.all[funcIndex[i]]))
            {
                continue;
            }

            if (delta > 0)
            {
                data += 1;
                if (data > upper)
                {
                    data = lower;
                }
            }
            else if (delta < 0)
            {
                data -= 1;
                if (data < lower)
                {
                    data = upper;
                }
            }
            else
            {
                data = lower;
            }

            break;
        }

        if ((i >= number) || (data == lower))
        {
            break;
        }
    }

    return data;
#endif
}

LOCALF void cycleShiftDeal(Uint16 flag)
{
    Uint16 *pDispBit = &funcCode.code.dispParaStopBit;
    Uint32 dispPara = funcCode.code.ledDispParaStop;
    Uint16 dispParaNum = STOP_DISPLAY_NUM;

    if (runFlag.bit.run)
    {
        pDispBit = &funcCode.code.dispParaRunBit;
        dispPara = funcCode.code.ledDispParaRun1
                 + ((Uint32)funcCode.code.ledDispParaRun2 << 16);
        dispParaNum = RUN_DISPLAY_NUM;
    }

    if (!dispPara)
    {
        dispPara = 1;
    }

    if ((!flag) && (dispPara & (0x1UL << *pDispBit)))
    {
        return;
    }

    do
    {
        if (++(*pDispBit) >= dispParaNum)
        {
            if (errorCode)
            {
                menu0DispStatus = MENU0_DISP_STATUS_ERROR;
            }

            if (tuneCmd)
            {
                menu0DispStatus = MENU0_DISP_STATUS_TUNE;
            }

            *pDispBit = 0;
        }
    }
    while (!(dispPara & (0x1UL << *pDispBit)));
}

LOCALF Uint16 OverTurnDeal(Uint16 data, Uint16 upper, Uint16 lower, Uint16 flag)
{
    if (ON_UP_KEY == flag)
    {
        if (upper == data)
        {
            data = lower;
        }
    }
    else if (lower == data)
    {
        data = upper;
    }

    return data;
}

LOCALF void UpdateMenu0DisplayBuffer(void)
{
    Uint16 dispNumber;
    union FUNC_ATTRIBUTE dispAttribute;

    UpdateMenu0DispStatus();

    if (MENU0_DISP_STATUS_ERROR == menu0DispStatus)
    {
        UpdateErrorDisplayBuffer();
        return;
    }

    if (MENU0_DISP_STATUS_TUNE == menu0DispStatus)
    {
        UpdateTuneDisplayBuffer();
        return;
    }

    if (MENU0_DISP_STATUS_RUN_STOP != menu0DispStatus)
    {
        return;
    }

    if (bFrqDigital || bFrqDigitalDone4WaitDelay)
    {
        if (!bFrqDigital)
        {
            if (++accDecFrqTicker >= UP_DOWN_DEAL_DONE_TIME / 12)
            {
                accDecFrqTicker = 0;
                bFrqDigitalDone4WaitDelay = 0;
            }
        }

        dispNumber = frqAimDisp;
        dispAttribute = funcCodeAttribute[MAX_FRQ_INDEX].attribute;
        dispAttribute.bit.point = funcCode.code.frqPoint;
    }
    else
    {
        Uint16 dispIndex;

        if (runFlag.bit.run)
        {
            dispIndex = funcCode.code.dispParaRunBit;
        }
        else
        {
            if (funcCode.code.dispParaStopBit >= STOP_DISPLAY_NUM)
            {
                funcCode.code.dispParaStopBit = 0;
            }

            dispIndex = stopDispIndex[funcCode.code.dispParaStopBit];
            menuAttri[MENU_LEVEL_0].winkFlag = MENU_WINK_FLAG_STOP;
        }

        dispAttribute = dispAttributeU0[dispIndex];

        if ((DISP_FRQ_RUN == dispIndex)
            || (DISP_FRQ_AIM == dispIndex)
            || (DISP_FRQ_RUN_FDB == dispIndex))
        {
            dispAttribute.bit.point = funcCode.code.frqPoint;
        }
        else if (DISP_OUT_CURRENT == dispIndex)
        {
            if (invPara.type > invPara.pointLimit)
            {
                dispAttribute.bit.point--;
            }
        }
        else if (DISP_LOAD_SPEED == dispIndex)
        {
            dispAttribute.bit.point = funcCode.code.speedDispPointPos;
        }

        dispNumber = funcCode.group.u0[dispIndex];
    }

    UpdateDisplayBufferAttribute(dispNumber, dispAttribute);
}

void UpdateMenu0DispStatus(void)
{
    static Uint16 errorCodePrev;
    static Uint16 tuneCmdPrev;

    if ((!errorCodePrev) && errorCode)
    {
        menu0DispStatus = MENU0_DISP_STATUS_ERROR;
    }
    errorCodePrev = errorCode;

    if ((!tuneCmdPrev) && tuneCmd)
    {
        menu0DispStatus = MENU0_DISP_STATUS_TUNE;
    }
    tuneCmdPrev = tuneCmd;

    if ((!errorCode) && (!tuneCmd))
    {
        menu0DispStatus = MENU0_DISP_STATUS_RUN_STOP;
    }
}

LOCALF void UpdateMenu1DisplayBuffer(void)
{
    UpdateGroupDisplay(curFc.group);

    displayBuffer[0] = DISPLAY_CODE[DISPLAY_NULL];
    displayBuffer[1] = DISPLAY_CODE[DISPLAY_NULL];
    displayBuffer[2] = DISPLAY_CODE[DISPLAY_NULL];
    displayBuffer[3] = DISPLAY_CODE[groupDisplay.dispF];
    displayBuffer[4] = DISPLAY_CODE[groupDisplay.disp0];
}

LOCALF void UpdateMenu2DisplayBuffer(void)
{
    Uint16 digit[MENU_DIGIT_NUM];
    Uint16 flag = 0;

    if ((MENU_MODE_CHECK == menuMode) && (checkMenuModeSerachNone))
    {
        flag = 1;
    }
    else if ((MENU_MODE_USER == menuMode)
             && (0 == funcCode.code.userCustom[userMenuModeFcIndex]))
    {
        flag = 2;
    }

    if (flag)
    {
        if (1 == flag)
        {
            displayBuffer[0] = DISPLAY_CODE[DISPLAY_c] & DISPLAY_CODE[DISPLAY_DOT];
        }
        else
        {
            displayBuffer[0] = DISPLAY_CODE[DISPLAY_u] & DISPLAY_CODE[DISPLAY_DOT];
        }

        displayBuffer[1] = DISPLAY_CODE[DISPLAY_N];
        displayBuffer[2] = DISPLAY_CODE[DISPLAY_U];
        displayBuffer[3] = DISPLAY_CODE[DISPLAY_L];
        displayBuffer[4] = DISPLAY_CODE[DISPLAY_L];
        return;
    }

    UpdateGroupDisplay(curFc.group);

    GetNumberDigit(digit, curFc.grade, DECIMAL);

    displayBuffer[0] = DISPLAY_CODE[groupDisplay.dispF];
    displayBuffer[1] = DISPLAY_CODE[groupDisplay.disp0];
    displayBuffer[2] = DISPLAY_CODE[DISPLAY_LINE];
    displayBuffer[3] = DISPLAY_CODE[digit[1]];
    displayBuffer[4] = DISPLAY_CODE[digit[0]];

    if (MENU_MODE_CHECK == menuMode)
    {
        displayBuffer[0] = DISPLAY_CODE[DISPLAY_c];
        displayBuffer[1] = DISPLAY_CODE[groupDisplay.dispF];
        displayBuffer[2] = DISPLAY_CODE[groupDisplay.disp0] & DISPLAY_CODE[DISPLAY_DOT];
    }
    else if (MENU_MODE_USER == menuMode)
    {
        displayBuffer[0] = DISPLAY_CODE[DISPLAY_u];
        displayBuffer[1] = DISPLAY_CODE[groupDisplay.dispF];
        displayBuffer[2] = DISPLAY_CODE[groupDisplay.disp0] & DISPLAY_CODE[DISPLAY_DOT];
    }

    SetWinkFlag(MENU_LEVEL_2);
}

LOCALF void UpdateMenu3DisplayBuffer(void)
{
    union FUNC_ATTRIBUTE attribute;
    Uint16 tmp = menu3Number;

#if (DEBUG_F_DISP_DIDO_STATUS_SPECIAL)

    if ((curFc.index == GetCodeIndex(funcCode.group.u0[DISP_DI_STATUS_SPECIAL1]))
        || (curFc.index == GetCodeIndex(funcCode.group.u0[DISP_DO_STATUS_SPECIAL1])))
    {
        Uint32 value;

        if (GetCodeIndex(funcCode.group.u0[DISP_DI_STATUS_SPECIAL1]) == curFc.index)
        {
            value = diStatus.a.all & MENU_DIDO_BIT_MASK;
        }
        else if (GetCodeIndex(funcCode.group.u0[DISP_DO_STATUS_SPECIAL1]) == curFc.index)
        {
            value = doStatus.a.all & MENU_DIDO_BIT_MASK;
        }

        UpdateDisplayBufferVisualIoStatus(value);
        return;
    }
    else if ((curFc.index == GetCodeIndex(funcCode.group.u0[DISP_DI_FUNC_SPECIAL1]))
             || (curFc.index == GetCodeIndex(funcCode.group.u0[DISP_DI_FUNC_SPECIAL2])))
    {
        Uint16 high;
        Uint32 low;

        if (GetCodeIndex(funcCode.group.u0[DISP_DI_FUNC_SPECIAL1]) == curFc.index)
        {
            high = (diFunc.f2.all >> MENU_DIFUNC_SPLIT_SHIFT) & MENU_DIFUNC_BYTE_MASK;
            low = ((diFunc.f2.all & 0x0001) << 31)
                + (diFunc.f1.all >> MENU_DIFUNC_SPLIT_SHIFT);
        }
        else if (GetCodeIndex(funcCode.group.u0[DISP_DI_FUNC_SPECIAL2]) == curFc.index)
        {
            high = 0;
            low = diFunc.f2.all >> MENU_DIFUNC_HIGH_SHIFT;
        }

        UpdateDisplayBufferVisualDiFunc(high, low);
        return;
    }
#endif

    if (FUNCCODE_GROUP_U0 == curFc.group)
    {
        attribute = dispAttributeU0[curFc.grade];
    }
#if DEBUG_F_POSITION_CTRL
    else if (FUNCCODE_GROUP_U0 + 1 == curFc.group)
    {
        attribute = dispAttributeU1[curFc.grade];
    }
#endif
#if DEBUG_F_MOTOR_FUNCCODE
    else if (FUNCCODE_GROUP_UF == curFc.group)
    {
        attribute.all = UF_VIEW_ATTRIBUTE;
    }
#endif
#if DEBUG_F_PLC_CTRL
    else if (FUNCCODE_GROUP_U0 + 3 == curFc.group)
    {
        attribute.all = MENU_VIEW_ATTRIBUTE_PLC;
    }
#endif
    else
    {
        attribute = funcCodeAttribute[curFc.index].attribute;
    }

    if (((TUNE_CMD_INDEX_1 == curFc.index)
         || (TUNE_CMD_INDEX_2 == curFc.index)
         || (TUNE_CMD_INDEX_3 == curFc.index)
         || (TUNE_CMD_INDEX_4 == curFc.index))
        && (FUNCCODE_runSrc_PANEL != runSrc))
    {
        attribute.bit.writable = ATTRIBUTE_READ_ONLY_ANYTIME;
    }

    if (IsWritable(attribute))
    {
        SetWinkFlag(MENU_LEVEL_3);
    }
    else
    {
        tmp = funcCode.all[curFc.index];
    }

    if (GetCodeIndex(funcCode.code.errorScene3.elem.errorFrq) == curFc.index)
    {
        attribute.bit.point = funcCode.code.errorFrqUnit & MENU_FRQ_UNIT_NIBBLE_MASK;
    }
    else if (GetCodeIndex(funcCode.code.errorScene2.elem.errorFrq) == curFc.index)
    {
        attribute.bit.point = (funcCode.code.errorFrqUnit >> MENU_FRQ_UNIT_NIBBLE_SHIFT)
                            & MENU_FRQ_UNIT_NIBBLE_MASK;
    }
    else if (GetCodeIndex(funcCode.code.errorScene1.elem.errorFrq) == curFc.index)
    {
        attribute.bit.point = (funcCode.code.errorFrqUnit >> (2 * MENU_FRQ_UNIT_NIBBLE_SHIFT))
                            & MENU_FRQ_UNIT_NIBBLE_MASK;
    }
    else if (GetCodeIndex(funcCode.group.u0[DISP_LOAD_SPEED]) == curFc.index)
    {
        attribute.bit.point = funcCode.code.speedDispPointPos;
    }
#if DEBUG_ACC_DEC_TIME_POINT

    else if ((GetCodeIndex(funcCode.code.accTime1) == curFc.index)
        || (GetCodeIndex(funcCode.code.accTime2) == curFc.index)
        || (GetCodeIndex(funcCode.code.accTime3) == curFc.index)
        || (GetCodeIndex(funcCode.code.accTime4) == curFc.index)
        || (GetCodeIndex(funcCode.code.jogAccTime) == curFc.index)
        || (GetCodeIndex(funcCode.code.decTime1) == curFc.index)
        || (GetCodeIndex(funcCode.code.decTime2) == curFc.index)
        || (GetCodeIndex(funcCode.code.decTime3) == curFc.index)
        || (GetCodeIndex(funcCode.code.decTime4) == curFc.index)
        || (GetCodeIndex(funcCode.code.jogDecTime) == curFc.index))
    {
        attribute.bit.point = funcCode.code.accDecTimeUnit;
    }
#endif

#if DEBUG_FRQ_POINT
    else if (((attribute.bit.point == 2) && (attribute.bit.unit == 1))
        || (GetCodeIndex(funcCode.code.diUpDownSlope) == curFc.index))
    {
        attribute.bit.point -= (2 - funcCode.code.frqPoint);
    }
#endif

    else if ((GetCodeIndex(funcCode.code.motorParaM1.elem.ratingCurrent) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorParaM1.elem.zeroLoadCurrent) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.ratingCurrent) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.zeroLoadCurrent) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM3.motorPara.elem.ratingCurrent) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM3.motorPara.elem.zeroLoadCurrent) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM4.motorPara.elem.ratingCurrent) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM4.motorPara.elem.zeroLoadCurrent) == curFc.index)
        || (GetCodeIndex(funcCode.code.errorScene3.elem.errorCurrent) == curFc.index)
        || (GetCodeIndex(funcCode.code.errorScene2.elem.errorCurrent) == curFc.index)
        || (GetCodeIndex(funcCode.group.u0[DISP_OUT_CURRENT]) == curFc.index))
    {
        if (invPara.type > invPara.pointLimit)
        {
            attribute.bit.point--;
        }
    }
    else if ((GetCodeIndex(funcCode.code.motorParaM1.elem.statorResistance) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorParaM1.elem.rotorResistance) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorParaM1.elem.leakInductance) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorParaM1.elem.mutualInductance) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorParaM1.elem.pmsmRs) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorParaM1.elem.pmsmLd) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorParaM1.elem.pmsmLq) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.statorResistance) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.rotorResistance) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.leakInductance) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.mutualInductance) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.pmsmRs) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.pmsmLd) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.pmsmLq) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM3.motorPara.elem.statorResistance) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM3.motorPara.elem.rotorResistance) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM3.motorPara.elem.leakInductance) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM3.motorPara.elem.mutualInductance) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM3.motorPara.elem.pmsmRs) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM3.motorPara.elem.pmsmLd) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM3.motorPara.elem.pmsmLq) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM4.motorPara.elem.statorResistance) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM4.motorPara.elem.rotorResistance) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM4.motorPara.elem.leakInductance) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM4.motorPara.elem.mutualInductance) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM4.motorPara.elem.pmsmRs) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM4.motorPara.elem.pmsmLd) == curFc.index)
        || (GetCodeIndex(funcCode.code.motorFcM4.motorPara.elem.pmsmLq) == curFc.index))
    {
        if (invPara.type > invPara.pointLimit)
        {
            attribute.bit.point++;
        }
    }

    UpdateDisplayBufferAttribute(tmp, attribute);
}

LOCALF void UpdateDisplayBufferAttribute(const Uint16 data, union FUNC_ATTRIBUTE attribute)
{
    static Uint16 digit[MENU_DIGIT_NUM];
    int16 i;
    Uint16 digits;
    Uint16 bNegative = 0;
    Uint16 absData;
    Uint16 digitOld1;
    Uint16 digitOld0;
    Uint16 mode;

    if ((attribute.bit.signal) && ((int16)(data) < 0))
    {
        absData = -(int16)(data);
        bNegative = 1;

        if ((attribute.bit.point) && ((int16)data < (-9999)))
        {
            attribute.bit.point--;
            absData = absData / 10;
        }
    }
    else
    {
        absData = data;
    }

    digitOld1 = digit[1];
    digitOld0 = digit[0];

    mode = DECIMAL;
    if (ATTRIBUTE_MULTI_LIMIT_HEX == attribute.bit.multiLimit)
    {
        mode = HEX;
    }
    digits = GetNumberDigit(digit, absData, mode);

    if (ticker4LowerDisp)
    {
        digit[1] = digitOld1;
        digit[0] = digitOld0;
    }

#if DEBUG_F_USER_MENU_MODE

    if ((USER_MENU_GROUP == curFc.group) && (MENU_LEVEL_3 == menuLevel))
    {
        Uint16 group;

        digit[4] = DISPLAY_u;

        group = menu3Number / MENU_GROUP_GRADE_BASE;
        UpdateGroupDisplay(group);
        digit[3] = groupDisplay.dispF;
        digit[2] = groupDisplay.disp0;

        attribute.bit.point = 2;
    }
#endif

    if ((MENU_LEVEL_0 == menuLevel)
        || ((MENU_LEVEL_3 == menuLevel) && (curFc.group >= FUNCCODE_GROUP_U0)))
    {
        if (mode == HEX)
        {
            if (attribute.bit.displayBits < DISPLAY_8LED_NUM)
            {
                digits = attribute.bit.displayBits + 1;
                digit[attribute.bit.displayBits] = DISPLAY_H;
                attribute.bit.point = attribute.bit.displayBits;
            }
        }

        if (++ticker4LowerDisp >= DECIMAL_DISPLAY_UPDATE_TIME)
        {
            ticker4LowerDisp = 0;
        }
    }
    else
    {
        ticker4LowerDisp = 0;

        if (mode == HEX)
        {
            if (curFcDispDigits < DISPLAY_8LED_NUM)
            {
                digits = curFcDispDigits + 1;
                digit[curFcDispDigits] = DISPLAY_H;
                attribute.bit.point = curFcDispDigits;
            }
            else
            {
                digits = curFcDispDigits;
            }
        }
        else
        {
            digits = curFcDispDigits;
        }
    }

    if (attribute.bit.point >= digits)
    {
        digits = attribute.bit.point + 1;
    }

    for (i = DISPLAY_8LED_NUM - 1; i >= 0; i--)
    {
        if (i < digits)
        {
            displayBuffer[(DISPLAY_8LED_NUM - 1) - i] = DISPLAY_CODE[digit[i]];
        }
        else
        {
            displayBuffer[(DISPLAY_8LED_NUM - 1) - i] = DISPLAY_CODE[DISPLAY_NULL];
        }
    }

    if (bNegative)
    {
        int16 tmp = DISPLAY_8LED_NUM - 1 - digits;

        if (tmp < 0)
        {
            tmp = 0;
        }

        if (!digit[4])
        {
            displayBuffer[tmp] = DISPLAY_CODE[DISPLAY_LINE];
        }
        else
        {
            attribute.bit.point = 0;
        }
    }

    if (attribute.bit.point)
    {
        displayBuffer[(DISPLAY_8LED_NUM - 1) - attribute.bit.point] &= DISPLAY_CODE[DISPLAY_DOT];
    }

    if (attribute.bit.unit & (0x01U << ATTRIBUTE_UNIT_HZ_BIT))
    {
        displayBuffer[5] &= LED_CODE[LED_HZ];
    }

    if (attribute.bit.unit & (0x01U << ATTRIBUTE_UNIT_A_BIT))
    {
        displayBuffer[5] &= LED_CODE[LED_A];
    }

    if (attribute.bit.unit & (0x01U << ATTRIBUTE_UNIT_V_BIT))
    {
        displayBuffer[5] &= LED_CODE[LED_V];
    }
}

void UpdateErrorDisplayBuffer(void)
{
    Uint16 digit[MENU_DIGIT_NUM];

    GetNumberDigit1(digit, errorCode);

    displayBuffer[0] = DISPLAY_CODE[DISPLAY_E];
    displayBuffer[1] = DISPLAY_CODE[DISPLAY_r];
    displayBuffer[2] = DISPLAY_CODE[DISPLAY_r];
    displayBuffer[3] = DISPLAY_CODE[digit[1]];
    displayBuffer[4] = DISPLAY_CODE[digit[0]];

    if ((runFlag.bit.run) || (ERROR_LEVEL_RUN == errorAttribute.bit.level))
    {
        displayBuffer[0] = DISPLAY_CODE[DISPLAY_NULL];
        displayBuffer[1] = DISPLAY_CODE[DISPLAY_NULL];
        displayBuffer[2] = DISPLAY_CODE[DISPLAY_A];
    }
}

LOCALF void UpdateTuneDisplayBuffer(void)
{
    displayBuffer[0] = DISPLAY_CODE[DISPLAY_NULL];
    displayBuffer[1] = DISPLAY_CODE[DISPLAY_T];
    displayBuffer[2] = DISPLAY_CODE[DISPLAY_U];
    displayBuffer[3] = DISPLAY_CODE[DISPLAY_N];
    displayBuffer[4] = DISPLAY_CODE[DISPLAY_E];
}

Uint16 ModifyFunccodeUpDown(Uint16 index, Uint16 *data, int16 delta)
{
    union FUNC_ATTRIBUTE attribute = funcCodeAttribute[index].attribute;
    Uint16 upper;
    Uint16 lower;
    Uint16 tmp = *data;
    Uint16 flag = COMM_ERR_NONE;
    int16 i;
    int16 flag1 = 0;
    Uint16 multiLimit;

    if (!IsWritable(attribute))
    {
        return COMM_ERR_READ_ONLY;
    }

#if DEBUG_F_USER_MENU_MODE
    if ((USER_MENU_GROUP == curFc.group)
        || ((index >= GetCodeIndex(funcCode.code.userCustom[0]))
            && (index <= GetCodeIndex(funcCode.code.userCustom[FENUM-1]))))
    {
        Uint16 group = *data / MENU_GROUP_GRADE_BASE;

        if ((group >= FUNCCODE_GROUP_NUM) || (group == USER_MENU_GROUP))
        {
            return COMM_ERR_PARA;
        }

        upper = group * MENU_GROUP_GRADE_BASE + funcCodeGradeAll[group] - 1;
        lower = group * MENU_GROUP_GRADE_BASE;

        if (upper < lower)
        {
            return COMM_ERR_PARA;
        }
    }
    else
#endif
    {
        upper = funcCodeAttribute[index].upper;
        if (attribute.bit.upperLimit)
        {
            upper = funcCode.all[upper];
        }

        if (index == GetCodeIndex(funcCode.code.sCurveStartPhaseTime))
        {
            upper = 1000 - funcCode.code.sCurveEndPhaseTime;
        }
        else if (index == GetCodeIndex(funcCode.code.sCurveEndPhaseTime))
        {
            upper = 1000 - funcCode.code.sCurveStartPhaseTime;
        }

        lower = funcCodeAttribute[index].lower;
        if (attribute.bit.lowerLimit)
        {
            lower = funcCode.all[lower];
        }

        multiLimit = funcCodeAttribute[index].attribute.bit.multiLimit;

        if ((ATTRIBUTE_MULTI_LIMIT_SINGLE != multiLimit) && (!delta))
        {
            Uint16 dataDigit[MENU_DIGIT_NUM];
            Uint16 upperDigit[MENU_DIGIT_NUM];
            Uint16 lowerDigit[MENU_DIGIT_NUM];
            Uint16 bit;
            Uint16 mode = DECIMAL;

            if (ATTRIBUTE_MULTI_LIMIT_HEX == multiLimit)
            {
                mode = HEX;
            }

            GetNumberDigit(dataDigit, *data, mode);
            GetNumberDigit(upperDigit, upper, mode);
            GetNumberDigit(lowerDigit, lower, mode);

            for (bit = 0; bit < MENU_DIGIT_NUM; bit++)
            {
                if ((dataDigit[bit] > upperDigit[bit])
                    || (dataDigit[bit] < lowerDigit[bit]))
                {
                    return COMM_ERR_PARA;
                }
            }

            return COMM_ERR_NONE;
        }

        if ((ATTRIBUTE_MULTI_LIMIT_SINGLE != multiLimit) && (delta))
        {
            Uint16 digit[MENU_DIGIT_NUM];
            Uint16 dataBase;
            Uint16 bit = menuAttri[MENU_LEVEL_3].operateDigit;
            const int16 *p = decNumber;
            Uint16 mode = DECIMAL;

            if (ATTRIBUTE_MULTI_LIMIT_HEX == multiLimit)
            {
                p = hexNumber;
                mode = HEX;
            }

            GetNumberDigit(digit, *data, mode);
            dataBase = *data - digit[bit] * (*(p + bit));

            GetNumberDigit(digit, upper, mode);
            upper = dataBase + digit[bit] * (*(p + bit));

            GetNumberDigit(digit, lower, mode);
            lower = dataBase + digit[bit] * (*(p + bit));
        }

#if DEBUG_F_INV_TYPE_RELATE

        if (GetCodeIndex(funcCode.code.inverterType) == index)
        {
            if ((100 != delta) && (-100 != delta))
            {
                Uint16 i;
                i = *data / 100;

                upper = invTypeLimitTable[i].upper + (i * 100);
                lower = invTypeLimitTable[i].lower + (i * 100);
            }
        }
        else
#endif

        if (GetCodeIndex(funcCode.code.maxFrq) == index)
        {
            lower = 50 * decNumber[funcCode.code.frqPoint];
        }
    }

#if DEBUG_F_NO_SAME

    for (i = sizeof(frqSrcFuncIndex) - 1; i >= 0; i--)
    {
        if (frqSrcFuncIndex[i] == index)
        {
            *data = NoSameDeal(index, frqSrcFuncIndex, sizeof(frqSrcFuncIndex), *data, upper, lower, delta);
            flag1 = 1;
        }

        if (flag1)
        {
            break;
        }
    }

    for (i = sizeof(diFuncIndex) - 1; i >= 0; i--)
    {
        if (diFuncIndex[i] == index)
        {
            *data = NoSameDeal(index, diFuncIndex, sizeof(diFuncIndex), *data, upper, lower, delta);
            flag1 = 1;
        }

        if (flag1)
        {
            break;
        }
    }

#endif
    if (!flag1)
    {
        *data = LimitDeal(attribute.bit.signal, *data, upper, lower, delta);
    }

    if (*data != tmp)
    {
        flag = COMM_ERR_PARA;
    }

    return flag;
}

Uint16 ModifyFunccodeEnter(Uint16 index, Uint16 dataNew)
{
    Uint16 dataPrev;
    Uint16 ret = COMM_ERR_NONE;

    dataPrev = funcCode.all[index];
    funcCode.all[index] = dataNew;

    if (dataPrev != dataNew)
    {
        if ((GetCodeIndex(funcCode.code.presetFrq) == index)
            || (GetCodeIndex(funcCode.code.frqCalcSrc) == index))
        {
            ResetUpDownFrq();
        }
#if DEBUG_F_MOTOR_POWER_RELATE
        else if (GetCodeIndex(funcCode.code.motorParaM1.elem.ratingPower) == index)
        {
            MotorPowerRelatedParaDeal(dataNew, MOTOR_SN_1);
        }
        else if (GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.ratingPower) == index)
        {
            MotorPowerRelatedParaDeal(dataNew, MOTOR_SN_2);
        }
        else if (GetCodeIndex(funcCode.code.motorFcM3.motorPara.elem.ratingPower) == index)
        {
            MotorPowerRelatedParaDeal(dataNew, MOTOR_SN_3);
        }
        else if (GetCodeIndex(funcCode.code.motorFcM4.motorPara.elem.ratingPower) == index)
        {
            MotorPowerRelatedParaDeal(dataNew, MOTOR_SN_4);
        }
#endif
        else if (GetCodeIndex(funcCode.code.menuMode) == index)
        {
            Uint16 digit[MENU_DIGIT_NUM];

            GetNumberDigit(digit, menu3Number, DECIMAL);

            menuModeTmp = menuMode;

            if (((menuModeTmp == MENU_MODE_USER) && (!digit[0]))
                || ((menuModeTmp == MENU_MODE_CHECK) && (!digit[1])))
            {
                menuModeTmp = MENU_MODE_BASE;
                MenuModeSwitch();
            }
        }
        else if ((GetCodeIndex(funcCode.code.aiaoCalibrateDisp) == curFc.index)
            || (GetCodeIndex(funcCode.code.funcParaView) == curFc.index))
        {
            MenuModeDeal();
        }
#if DEBUG_F_INV_TYPE_RELATE
        else if (GetCodeIndex(funcCode.code.inverterType) == index)
        {
            ret = ValidateInvType();

            if (!ret)
            {
                InverterTypeRelatedParaDeal();
            }
        }
#endif
#if 0
        else if (GetCodeIndex(funcCode.code.commOverTime) == index)
        {
            if ((dataNew) && (!dataOld))
                commTicker = 0;
        }
#endif
    }

    if ((GetCodeIndex(funcCode.code.diFunc[0]) == index)
        || (GetCodeIndex(funcCode.code.diFunc[1]) == index)
        || (GetCodeIndex(funcCode.code.diFunc[2]) == index)
        || (GetCodeIndex(funcCode.code.diFunc[3]) == index)
        || (GetCodeIndex(funcCode.code.diFunc[5]) == index)
        || (GetCodeIndex(funcCode.code.diFunc[6]) == index)
        || (GetCodeIndex(funcCode.code.diFunc[7]) == index)
        || (GetCodeIndex(funcCode.code.diFunc[8]) == index)
        || (GetCodeIndex(funcCode.code.diFunc[9]) == index))
    {
        if (dataNew == DI_FUNC_APTP_ZERO)
        {
            ret = COMM_ERR_PARA;
        }
    }
#if DEBUG_F_GROUP_HIDE
    else if (GetCodeIndex(funcCode.code.funcParaView) == curFc.index)
    {
        MenuModeDeal();
    }
#endif
#if DEBUG_F_MOTOR_FUNCCODE1

    else if (GetCodeIndex(funcCode.code.motorDebugFc) == curFc.index)
    {
        MenuModeDeal();
    }
#endif

    else if (GetCodeIndex(funcCode.code.aiaoCalibrateDisp) == curFc.index)
    {
        MenuModeDeal();
    }
    else if ((GetCodeIndex(funcCode.code.ledDispParaRun1) == index)
        || (GetCodeIndex(funcCode.code.ledDispParaRun2) == index)
        || (GetCodeIndex(funcCode.code.ledDispParaStop) == index))
    {
        cycleShiftDeal(0);
    }
    else if (GetCodeIndex(funcCode.code.frqPoint) == index)
    {
        if (funcCode.code.maxFrq < 50 * decNumber[funcCode.code.frqPoint])
        {
            funcCode.code.maxFrq = 50 * decNumber[funcCode.code.frqPoint];

            LimitOtherCodeDeal(MAX_FRQ_INDEX);
        }
    }
#if (DEBUG_F_POSITION_CTRL)

    else if (GetCodeIndex(funcCode.code.diFunc[4]) == index)
    {

        if ((dataOld != DI_FUNC_APTP_ZERO) && (dataNew == DI_FUNC_APTP_ZERO))
        {
            InitSetEcap4WithInt();
            InitSetAptpZero();
        }
        else if ((dataOld == DI_FUNC_APTP_ZERO) && (dataNew != DI_FUNC_APTP_ZERO))
        {
            ;
        }
    }

    else if (GetCodeIndex(funcCode.code.pgParaM1.elem.fvcPgSrc) == index)
    {
        if ((dataOld != FUNCCODE_fvcPgSrc_QEP1) && (dataNew == FUNCCODE_fvcPgSrc_QEP1))
        {

            aptpAbsZeroOk = 0;
            pEQepRegsFvc = &EQep1Regs;

            EALLOW;
            pEQepRegsFvc->QEPCTL.bit.IEL = 01;
            EDIS;

            InitSetPcEQep();
        }
        else if ((dataOld == FUNCCODE_fvcPgSrc_QEP1) && (dataNew != FUNCCODE_fvcPgSrc_QEP1))
        {

            aptpAbsZeroOk = 0;
            pEQepRegsPc = &EQep1Regs;

            EALLOW;
            pEQepRegsFvc->QEPCTL.bit.IEL = 01;
            EDIS;

            InitSetPcEQep();
        }
    }

    else if (GetCodeIndex(funcCode.code.pcPulseLogic) == index)
    {

        pEQepRegsPc->QDECCTL.bit.QBP = dataNew;
    }

    else if (GetCodeIndex(funcCode.code.pcPulseType) == index)
    {
        if (FUNCCODE_pcPulseType_PULSE_AND_DIR == funcCode.code.pcPulseType)
        {
            pEQepRegsPc->QDECCTL.bit.QSRC = 01;
        }
        else if (FUNCCODE_pcPulseType_QUADRATURE == funcCode.code.pcPulseType)
        {
            pEQepRegsPc->QDECCTL.bit.QSRC = 00;
        }
    }

    else if (GetCodeIndex(funcCode.code.pcPulseSwap) == index)
    {
        pEQepRegsPc->QDECCTL.bit.SWAP = dataNew;
    }

#if 0
    else if (GetCodeIndex(funcCode.code.fvcPgLogic) == index)
    {

        pEQepRegsFvc->QDECCTL.bit.SWAP = dataNew;
    }
#endif

    else if (GetCodeIndex(funcCode.code.pcZeroSelect) == index)
    {
        if (dataOld != dataNew)
        {
            aptpAbsZeroOk = 0;
        }
    }
#endif
    else
    {
        LimitOtherCodeDeal(index);
    }

    if (COMM_ERR_PARA == ret)
    {
        funcCode.all[index] = dataPrev;
    }

    return ret;
}

#define OUT_TORQUE_FRQ_DISP_FILTER_TIME 75
LOCALF LowPassFilter torQueFrqDispLpf = LPF_DEFALUTS;
void DispDataDeal(void)
{
    frqDisp = ABS_INT32(frqDroop);

#if DEBUG_F_PLC_CTRL
    frqPLCDisp = (int16)(frqDroop * MENU_SPEED_DISP_UNIT / maxFrq);
#endif

    if (RUN_MODE_TORQUE_CTRL == runMode)
    {
        torQueFrqDispLpf.t = OUT_TORQUE_FRQ_DISP_FILTER_TIME;
        torQueFrqDispLpf.in = ABS_INT32(frqRun);
        torQueFrqDispLpf.calc(&torQueFrqDispLpf);
        frqDisp = torQueFrqDispLpf.out;

#if DEBUG_F_PLC_CTRL
        frqPLCDisp = (int16)(torQueFrqDispLpf.out * MENU_SPEED_DISP_UNIT / maxFrq);
#endif
    }

#if DEBUG_F_PLC_CTRL
    if (funcCode.code.runDir == FUNCCODE_runDir_REVERSE)
    {
        frqPLCDisp = -((int16)frqPLCDisp);
    }
#endif

    pidFuncRefDisp = ((Uint32)funcCode.code.pidDisp * ABS_INT32(pidFunc.ref)
                      + MENU_PID_DISP_ROUND) >> MENU_PID_DISP_SHIFT;
    pidFuncFdbDisp = ((Uint32)funcCode.code.pidDisp * ABS_INT32(pidFunc.fdb)
                      + MENU_PID_DISP_ROUND) >> MENU_PID_DISP_SHIFT;

    if (runFlag.bit.run)
    {
        loadSpeedDisp = (ABS_INT32(frq) * funcCode.code.speedDispCoeff) / MENU_SPEED_DISP_UNIT;
    }
    else
    {
        loadSpeedDisp = (ABS_INT32(frqAim) * funcCode.code.speedDispCoeff)
                      / MENU_SPEED_DISP_UNIT;
    }

    pulseInFrqDisp = (pulseInFrq + MENU_PULSE_FRQ_ROUND) / MENU_PULSE_FRQ_DIV;

    torqueCurrentAct = (Uint32)torqueCurrent * currentPu >> MENU_TORQUE_CURRENT_SHIFT;

#if 0

    outPower = (Uint32)torqueCurrentAct * outVoltageDisp * 135 / (100UL * 1000);
    itDisp = (Uint32)torqueCurrentAct * 1000 / motorFc.motorPara.elem.ratingCurrent;
#endif

    funcCode.code.inverterGpTypeDisp = funcCode.code.inverterGpType;
}

#if 0

void RestoreCompanyParaRamDeal(Uint16 i)
{
    Uint16 flag = 0;

    if (indexRestoreExceptSeries < RESTORE_COMPANY_PARA_EXCEPT_NUMBER)
    {
        if ((exceptRestoreSeries[indexRestoreExceptSeries].start <= i) &&
            (i <= exceptRestoreSeries[indexRestoreExceptSeries].end))
        {
            flag = 1;
        }
        if (i >= exceptRestoreSeries[indexRestoreExceptSeries].end)
        {
            indexRestoreExceptSeries++;
        }
    }

    if (indexRestoreExceptSingle < CLEAR_RECORD_NUM)
    {
        if (clearRecord[indexRestoreExceptSingle] == i)
        {
            flag = 1;
            indexRestoreExceptSingle++;
        }
    }

    if (!flag)
    {
        funcCode.all[i] = funcCodeInit.all[i];
    }
}
#endif

void Menu0AddMenuLevel(void)
{
    menuLevel = MENU_LEVEL_1;

    if ((MENU_MODE_CHECK == menuMode) || (MENU_MODE_USER == menuMode))
    {
        Menu1OnEnter();
    }
}

Uint16 GetDispDigits(Uint16 index)
{
    Uint16 upper;
    Uint16 lower;
    Uint16 value;
    Uint16 digits;
    Uint16 mode;
    Uint16 digit[MENU_DIGIT_NUM];
    const FUNCCODE_ATTRIBUTE *p = &funcCodeAttribute[index];

    while (p->attribute.bit.upperLimit)
    {
        p = &funcCodeAttribute[p->upper];
    }
    upper = p->upper;

    if (!p->attribute.bit.signal)
    {
        value = upper;
    }
    else
    {
        while (p->attribute.bit.lowerLimit)
        {
            p = &funcCodeAttribute[p->lower];
        }
        lower = p->lower;

        upper = ABS_INT16((int16)upper);
        lower = ABS_INT16((int16)lower);

        value = (upper > lower) ? upper : lower;
    }

    mode = DECIMAL;

    if (ATTRIBUTE_MULTI_LIMIT_HEX == p->attribute.bit.multiLimit)
    {
        mode = HEX;
    }

    digits = GetNumberDigit(digit, value, mode);

    return digits;
}

void UpdateGroupDisplay(Uint16 group)
{
    Uint16 groupDispF = DISPLAY_F;
    Uint16 groupDisp0;

    if (group <= FUNCCODE_GROUP_FP)
    {
        groupDisp0 = group;
    }
    else if (group <= FUNCCODE_GROUP_CF)
    {
        Uint16 letterIndex;
        Uint16 letterDigit;

        letterIndex = (group - FUNCCODE_GROUP_FP - 1) / MENU_GROUP_LETTER_NUM;
        letterDigit = (group - FUNCCODE_GROUP_FP - 1) % MENU_GROUP_LETTER_NUM;

        groupDispF = DISPLAY_A + letterIndex;
        groupDisp0 = letterDigit;
    }
    else
    {
        groupDispF = DISPLAY_U;
        groupDisp0 = group - FUNCCODE_GROUP_U0;
    }

    groupDisplay.dispF = groupDispF;
    groupDisplay.disp0 = groupDisp0;
}

Uint16 GroupUpDown(const Uint16 funcCodeGrade[], Uint16 group, Uint16 flag)
{
    group = LimitOverTurnDeal(funcCodeGrade, group, FUNCCODE_GROUP_NUM, FC_START_GROUP, flag);

    return group;
}

void DealUserMenuModeGroupGrade(Uint16 flag)
{
#if DEBUG_F_USER_MENU_MODE
    userMenuModeFcIndex = LimitOverTurnDeal(funcCode.code.userCustom, userMenuModeFcIndex,
                                            FENUM, 0, flag);

    curFc.group = funcCode.code.userCustom[userMenuModeFcIndex] / MENU_GROUP_GRADE_BASE;
    curFc.grade = funcCode.code.userCustom[userMenuModeFcIndex] % MENU_GROUP_GRADE_BASE;
#endif
}

Uint16 LimitOverTurnDeal(const Uint16 limit[], Uint16 data, Uint16 upper, Uint16 low, Uint16 flag)
{
    Uint16 loopNumber = 0;

    do
    {
        data += deltaK[flag];

        if ((int16)data >= (int16)upper)
        {
            data = low;
        }
        else if ((int16)data < (int16)low)
        {
            data = upper - 1;
        }

        loopNumber++;
    }
    while ((!limit[data]) && (loopNumber < upper - low));

    return data;
}

#define CHECK_MENU_MODE_NUMBER_ONCE     70
void DealCheckMenuModeGroupGrade(Uint16 flag)
{
#if DEBUG_F_CHECK_MENU_MODE
    int16 delta = 1;
    static Uint16 group;
    static Uint16 grade;
    Uint16 gradePrev;
    Uint16 loopNumber = 0;
    Uint16 searchFcIndex;

    if (CHECK_MENU_MODE_DEAL_CMD == checkMenuModeDealStatus)
    {
        group = curFc.group;
        grade = curFc.grade;
    }
    checkMenuModeDealStatus = CHECK_MENU_MODE_DEAL_CMD;

    if (ON_DOWN_KEY == flag)
    {
        delta = -delta;
    }

    do
    {
        gradePrev = grade;

        if (funcCodeGradeCurMenuMode[group] > 1)
        {
            grade = LimitDeal(0, grade, funcCodeGradeCurMenuMode[group] - 1, 0, delta);
        }

        if (grade == gradePrev)
        {
            group = LimitOverTurnDeal(funcCodeGradeCurMenuMode, group,
                                      FUNCCODE_GROUP_U0 - 1, FUNCCODE_GROUP_F0, flag);

            if (ON_UP_KEY == flag)
            {
                grade = 0;
            }
            else
            {
                grade = funcCodeGradeCurMenuMode[group] - 1;
            }
        }

        searchFcIndex = GetGradeIndex(group, grade);

        if (++loopNumber >= CHECK_MENU_MODE_NUMBER_ONCE)
        {
            checkMenuModeDealStatus = CHECK_MENU_MODE_DEAL_SERACHING;
        }

        if ((curFc.grade == grade) && (curFc.group == group))
        {
            checkMenuModeDealStatus = CHECK_MENU_MODE_DEAL_END_NONE;
            checkMenuModeSerachNone = 1;
        }

        if ((funcCode.all[searchFcIndex] != GetFuncCodeInit(searchFcIndex, 0))
            && (funcCodeAttribute[searchFcIndex].attribute.bit.writable != 2))
        {
            checkMenuModeDealStatus = CHECK_MENU_MODE_DEAL_END_ONCE;
            checkMenuModeSerachNone = 0;
        }
    }
    while (CHECK_MENU_MODE_DEAL_CMD == checkMenuModeDealStatus);

#if 0
    if (CHECK_MENU_MODE_DEAL_END_NONE == checkMenuModeDealStatus)
    {
        group = 0;
        grade = 0;
    }
#endif

    if (CHECK_MENU_MODE_DEAL_SERACHING != checkMenuModeDealStatus)
    {
        curFc.group = group;
        curFc.grade = grade;

        checkMenuModeCmd = 0;
        checkMenuModeDealStatus = CHECK_MENU_MODE_DEAL_CMD;
    }
#endif
}

void MenuModeDeal(void)
{
    Uint16 digit[MENU_DIGIT_NUM];

    UpdataFuncCodeGrade(funcCodeGradeCurMenuMode);
    GetNumberDigit1(digit, funcCode.code.menuMode);

    switch (menuMode)
    {
        case MENU_MODE_BASE:
            break;

#if DEBUG_F_USER_MENU_MODE
        case MENU_MODE_USER:
            if (MENU_MODE_USER != menuModeOld)
            {
                DealUserMenuModeGroupGrade(ON_UP_KEY);
            }

            curFc.group = funcCode.code.userCustom[userMenuModeFcIndex] / MENU_GROUP_GRADE_BASE;
            curFc.grade = funcCode.code.userCustom[userMenuModeFcIndex] % MENU_GROUP_GRADE_BASE;
            break;
#endif

#if DEBUG_F_CHECK_MENU_MODE
        case MENU_MODE_CHECK:
            if (MENU_MODE_CHECK != menuModeOld)
            {
                checkMenuModeCmd = 1;
                checkMenuModePara = ON_UP_KEY;
            }
            break;
#endif
        default:
            break;
    }
}

void UpdataFuncCodeGrade(Uint16 funcCodeGrade[])
{
#if 1
    int16 i;
    Uint16 digit[MENU_DIGIT_NUM];

    memcpy(funcCodeGrade, funcCodeGradeAll, FUNCCODE_GROUP_NUM);

    GetNumberDigit(digit, funcCode.code.funcParaView, DECIMAL);

    if (0 == digit[0])
    {
        for (i = 0; i < MENU_GROUP_LETTER_NUM; i++)
        {
            funcCodeGrade[FUNCCODE_GROUP_U0 + i] = 0;
        }
    }

    if (0 == digit[1])
    {
        for (i = 0; i < MENU_GROUP_LETTER_NUM; i++)
        {
            funcCodeGrade[FUNCCODE_GROUP_A0 + i] = 0;
        }
    }

    if (0 == digit[2])
    {
        for (i = 0; i < MENU_GROUP_LETTER_NUM; i++)
        {
            funcCodeGrade[FUNCCODE_GROUP_B0 + i] = 0;
        }
    }

    if (0 == digit[3])
    {
        for (i = 0; i < MENU_GROUP_LETTER_NUM; i++)
        {
            funcCodeGrade[FUNCCODE_GROUP_C0 + i] = 0;
        }
    }

#if DEBUG_F_MOTOR_FUNCCODE
    MotorDebugFcDeal();
    funcCodeGrade[FUNCCODE_GROUP_CF] = motorDebugFc.fc;
    funcCodeGrade[FUNCCODE_GROUP_UF] = motorDebugFc.u;
#endif

    if (funcCode.code.aiaoCalibrateDisp)
    {
        funcCodeGrade[FUNCCODE_GROUP_AE] = AENUM;
    }
#endif
}

Uint16 FcDigit2Bin(Uint16 value)
{
    Uint16 tmp = 0;
    Uint16 digit[MENU_DIGIT_NUM];
    int16 i;

    GetNumberDigit(digit, value, DECIMAL);

    for (i = MENU_DIGIT_NUM - 1; i >= 0; i--)
    {
        if (digit[i])
        {
            tmp += 1 << i;
        }
    }

    return tmp;
}

void MotorDebugFcDeal(void)
{
#if DEBUG_F_MOTOR_FUNCCODE
#if DEBUG_F_MOTOR_FUNCCODE1
    Uint16 fc;
    Uint16 u;

    fc = funcCode.code.motorDebugFc % MENU_GROUP_GRADE_BASE;
    u = funcCode.code.motorDebugFc / MENU_GROUP_GRADE_BASE;

    if (fc > CFNUM)
    {
        fc = CFNUM;
    }

    if (u > UFNUM)
    {
        u = UFNUM;
    }

    motorDebugFc.fc = fc;
    motorDebugFc.u = u;
#endif
#endif
}

void MenuInit(void)
{
    menuAttri[MENU_LEVEL_0].winkFlag = MENU_WINK_FLAG_STOP;

    menuModeTmp = MENU_MODE_BASE;
    menuMode = menuModeTmp;
    MenuModeDeal();
    menuLevel = MENU_LEVEL_0;
}

#if DEBUG_F_DISP_DIDO_STATUS_SPECIAL

void UpdateDisplayBufferVisualIoStatus(Uint32 value)
{
    int16 i;
    Uint16 bit[4];

    for (i = 4; i >= 0; i--)
    {
        bit[0] = (value & (0x01 << 0)) >> 0;
        bit[1] = (value & (0x01 << 1)) >> 1;
        bit[2] = (value & (0x01 << 2)) >> 2;
        bit[3] = (value & (0x01 << 3)) >> 3;

        displayBuffer[i] = (bit[0] << 1)
                         | (bit[1] << 2)
                         | (bit[2] << 5)
                         | (bit[3] << 4)
                         | (~DISPLAY_CODE[DISPLAY_LINE]);
        displayBuffer[i] = ~displayBuffer[i];

        value >>= 4;
    }
}

void UpdateDisplayBufferVisualDiFunc(Uint16 valueH, Uint32 valueL)
{
    displayBuffer[4] = ~(Uint16)((valueL >> 0) & 0xff);
    displayBuffer[3] = ~(Uint16)((valueL >> 8) & 0xff);
    displayBuffer[2] = ~(Uint16)((valueL >> 16) & 0xff);
    displayBuffer[1] = ~(Uint16)((valueL >> 24) & 0xff);
    displayBuffer[0] = ~(Uint16)((valueH >> 0) & 0xff);
}
#endif

#if DEBUG_F_PASSWORD
void MenuPwdOnPrg(void)
{
    menuLevel = MENU_LEVEL_0;

#if DEBUG_F_GROUP_HIDE
    if (groupHidePwdStatus)
    {
        groupHidePwdStatus = 0;
        menuLevel = MENU_LEVEL_2;
    }
#endif
}

LOCALD void MenuPwdHint2Input(void)
{
    menuLevel = MENU_LEVEL_PWD_INPUT;
    menuAttri[MENU_LEVEL_PWD_INPUT].operateDigit = 0;
    menuPwdNumber = 0;
}

LOCALD void MenuPwdInputOnEnter(void)
{
    if ((menuPwdNumber == funcCode.code.userPassword)
        || (menuPwdNumber == SUPER_USER_PASSWORD))
    {
        Menu0AddMenuLevel();
    }
    else
    {
        menuLevel = MENU_LEVEL_PWD_HINT;
    }
}

LOCALD void MenuPwdHintOnShift(void)
{
#if DEBUG_RANDOM_FACPASS
     facPassViewStatus= FAC_PASS_VIEW;

    if (!factoryPwd)
    {
        Uint32 time;
        Uint16 pwd[4];
        time = GetTime();
        factoryPwd = (time >> 16) + (time & 0xFFFF);
        pwd[0] = factoryPwd>>8;
        pwd[1] = SUPER_USER_PASSWORD_SOURCE1&0xFF;
        pwd[2] = SUPER_USER_PASSWORD_SOURCE1>>8;
        pwd[3] = factoryPwd&0xFF;
        superFactoryPass = CrcValueByteCalc(pwd, 4);
    }
#else
    MenuPwdHint2Input();
#endif
}

LOCALD void MenuPwdInputOnUp(void)
{
    MenuPwdInputOnUpDown(ON_UP_KEY);
}

LOCALD void MenuPwdInputOnDown(void)
{
    MenuPwdInputOnUpDown(ON_DOWN_KEY);
}

void MenuPwdInputOnUpDown(Uint16 flag)
{
    int16 delta;

    delta = decNumber[menuAttri[MENU_LEVEL_PWD_INPUT].operateDigit];

    if (ON_DOWN_KEY == flag)
    {
        delta = -delta;
    }

    menuPwdNumber = LimitDeal(0, menuPwdNumber, MENU_PWD_LIMIT, 0, delta);
}

LOCALD void MenuPwdInputOnShift(void)
{
    if (0 == menuAttri[MENU_LEVEL_PWD_INPUT].operateDigit)
    {
        menuAttri[MENU_LEVEL_PWD_INPUT].operateDigit = MENU_OPERATE_DIGIT_MAX;
    }
    else
    {
        menuAttri[MENU_LEVEL_PWD_INPUT].operateDigit--;
    }
}

LOCALD void UpdateMenuPwdHintDisplayBuffer(void)
{
#if DEBUG_RANDOM_FACPASS
    if (facPassViewStatus == FAC_PASS_VIEW)
    {

        Uint16 digit[5];
        GetNumberDigit(digit, factoryPwd, 0);
        displayBuffer[0] = DISPLAY_CODE[digit[4]];
        displayBuffer[1] = DISPLAY_CODE[digit[3]];
        displayBuffer[2] = DISPLAY_CODE[digit[2]];
        displayBuffer[3] = DISPLAY_CODE[digit[1]];
        displayBuffer[4] = DISPLAY_CODE[digit[0]];
        menuAttri[MENU_LEVEL_PWD_HINT].winkFlag = 0;
    }
    else
#endif
    {
        displayBuffer[0] = DISPLAY_CODE[DISPLAY_LINE];
        displayBuffer[1] = DISPLAY_CODE[DISPLAY_LINE];
        displayBuffer[2] = DISPLAY_CODE[DISPLAY_LINE];
        displayBuffer[3] = DISPLAY_CODE[DISPLAY_LINE];
        displayBuffer[4] = DISPLAY_CODE[DISPLAY_LINE];
        menuAttri[MENU_LEVEL_PWD_HINT].winkFlag = MENU_WINK_FLAG_HINT;
    }
}

LOCALD void UpdateMenuPwdInputDisplayBuffer(void)
{
    Uint16 digit[MENU_DIGIT_NUM];

    GetNumberDigit(digit, menuPwdNumber, DECIMAL);

    displayBuffer[0] = DISPLAY_CODE[digit[4]];
    displayBuffer[1] = DISPLAY_CODE[digit[3]];
    displayBuffer[2] = DISPLAY_CODE[digit[2]];
    displayBuffer[3] = DISPLAY_CODE[digit[1]];
    displayBuffer[4] = DISPLAY_CODE[digit[0]];

    SetWinkFlag(MENU_LEVEL_PWD_INPUT);
}

#elif 1

LOCALD void MenuPwdOnPrg(void){}
LOCALD void MenuPwdHint2Input(void){}
LOCALD void MenuPwdInputOnEnter(void){}
LOCALD void MenuPwdInputOnUp(void){}
LOCALD void MenuPwdInputOnDown(void){}
LOCALD void MenuPwdInputOnShift(void){}
LOCALD void UpdateMenuPwdHintDisplayBuffer(void){}
LOCALD void UpdateMenuPwdInputDisplayBuffer(void){}

#endif

LOCALD void MenuPwdHintOnQuick(void)
{
}
LOCALD void MenuPwdInputOnQuick(void)
{
}
