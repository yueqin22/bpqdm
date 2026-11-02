
#include "f_error.h"
#include "f_main.h"
#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_eeprom.h"
#include "f_io.h"
#include "f_menu.h"
#include "f_ui.h"
#include "f_comm.h"
#include "f_posCtrl.h"
#include "f_invPara.h"

#if F_DEBUG_RAM

#define DEBUG_F_ERROR                   0
#define DEBUG_F_ERROR_FROM_MOTOR        0

#elif 1

#define DEBUG_F_ERROR                   1

#if defined(FUNC_DEBUG)
#define DEBUG_F_ERROR_FROM_MOTOR        0
#else
#define DEBUG_F_ERROR_FROM_MOTOR        1
#endif

#define DEBUG_F_ERROR_LEVEL             1

#endif

#define ERROR_EXTERNAL_INFO_OPEN        1
#define ERROR_EXTERNAL_INFO_CLOSE       2

#define DEV_CHK_TIME_MIN                5
#define ERROR_SCENE_NUMBER              8

#define ERR_LIST_LOW_WORD_BITS          16
#define ERR_INFO_BITS_PER_CODE          4
#define ERR_INFO_MASK                   0xF

#define ERROR_NONE_LEVEL                4

#define ERR_AUTO_RST_CLR_UNIT_NUM       10
#define ERR_AUTO_RST_MIN_SPACE          5

#define DEV_CHK_TIME_UNIT               100
#define OS_CHK_TIME_UNIT                100

#define ERROR_FRQ_UNIT_MASK             0x0FFF
#define ERROR_FRQ_UNIT_SHIFT            4

Uint16 errorEeprom;
Uint16 errorOther;
Uint16 errorOtherOld;
Uint16 errorCode;

union ERROR_ATTRIBUTE errorAttribute;
Uint16 errorInfo;

struct CUR_TIME curTime;

LOCALF enum ERROR_DEAL_STATUS errorDealStatus;

enum UV_DEAL_STATUS
{
    UV_READY_FOR_WRITE_EEPROM,
    UV_WAIT_WRITE_EEPROM,
    UV_WRITE_EEPROM_OK
};
LOCALF enum UV_DEAL_STATUS uvDealStatus;

Uint16 bUv;
LOCALF Uint32 errAutoRstClrWaitTicker;
LOCALF Uint16 errorSpaceTicker;

Uint16 errAutoRstNum;

Uint16 errorInfoFromMotor;
Uint16 errorCodeOld;
Uint16 errorInfoOld;
union ERROR_ATTRIBUTE errorAttributeOld;

const Uint16 errorCodeFromMotorList[] =
{
    ERROR_OC_ACC_SPEED,
    ERROR_OV_ACC_SPEED,
    ERROR_BUFFER_RES,
    ERROR_UV,

    ERROR_OL_INVERTER,
    ERROR_OL_MOTOR,
    ERROR_LOSE_PHASE_INPUT,
    ERROR_LOSE_PHASE_OUTPUT,

    ERROR_OT_IGBT,
    ERROR_CONTACTOR,
    ERROR_CURRENT_SAMPLE,
    ERROR_MOTOR_SHORT_TO_GND,

    ERROR_LOSE_LOAD,
    ERROR_CBC,
    ERROR_INIT_POSITION,
    ERROR_RSVD,

    ERROR_RSVD,
    ERROR_TUNE,
    ERROR_UVW_FDB
};

LOCALD void underVoltageDeal(void);
void PowerOffRemDeal(void);
void ErrorSceneSave(void);
void UpdateError(void);
Uint16 AutoResetErrorTrueDeal(void);
Uint16 AutoResetErrorFcDeal(void);
Uint16 AutoResetErrorDeal(void);
void GetNewErrorAttribute(void);
void GetNewError(void);
void GetErrorResetDeal(void);

#if DEBUG_F_ERROR

void ErrorDeal(void)
{
    underVoltageDeal();
    UpdateError();
    ErrorSceneSave();
}

LOCALF void underVoltageDeal(void)
{
#if DEBUG_F_ERROR_FROM_MOTOR

    if (((!dspStatus.bit.uv) && (POWER_ON_WAIT != powerOnStatus))
        || (POWER_ON_FUNC_WAIT_OT == powerOnStatus))
    {
        bUv = 1;
    }
#endif

    if (!bUv)
    {
        return;
    }

    if (runFlag.bit.run)
    {
        errorOther = ERROR_UV;
    }

    switch (uvDealStatus)
    {
        case UV_READY_FOR_WRITE_EEPROM:

            if (!funcCodeRwModeTmp)
            {
                PowerOffRemDeal();

                uvDealStatus = UV_WAIT_WRITE_EEPROM;
            }
            break;

        case UV_WAIT_WRITE_EEPROM:

            if (!funcCodeRwMode)
            {
                uvDealStatus = UV_WRITE_EEPROM_OK;
            }
            break;

        case UV_WRITE_EEPROM_OK:

            if (dspStatus.bit.uv)
            {
                uvDealStatus = UV_READY_FOR_WRITE_EEPROM;
                bUv = 0;
            }
            break;

        default:
            break;
    }
}

static void RollbackError(void)
{
    errorCode = errorCodeOld;
    errorAttribute.all = errorAttributeOld.all;
}

void UpdateError(void)
{
    Uint16 levelNew;
    Uint16 levelOld;

    errorCodeOld = errorCode;
    errorAttributeOld.all = errorAttribute.all;
    errorInfoOld = errorInfo;

    GetNewError();

    GetNewErrorAttribute();

    if (errorCodeOld == errorCode)
    {
        return;
    }

    levelNew = errorAttribute.bit.level;
    levelOld = errorAttributeOld.bit.level;

    if (ERROR_LEVEL_NO_ERROR == levelNew)
    {
        RollbackError();
        return;
    }

    if (ERROR_LEVEL_NO_ERROR == levelOld)
    {
        return;
    }

#if DEBUG_F_ERROR_LEVEL

    if (levelNew < levelOld)
    {
        return;
    }

    if (ERROR_LEVEL_RUN == levelOld)
    {
        return;
    }
#endif

    RollbackError();
}

#if 0
Uint16 getMotorErrorInfo(Uint16 index)
{
    Uint16 i, j, errorInfo;
#if DEBUG_F_ERROR_FROM_MOTOR

#if 1
    errorInfo = (motorErrInfor[index/4] << ((index%4)*4)) >> 12;
#else

    i = index%4;
    j = index/4;

    switch(i)
    {
        case 0:
            errorInfo = motorErrInfor[j].bits.errorInfo1;
            break;

        case 1:
            errorInfo = motorErrInfor[j].bits.errorInfo2;
            break;

        case 2:
            errorInfo = motorErrInfor[j].bits.errorInfo3;
            break;

        case 3:
            errorInfo = motorErrInfor[j].bits.errorInfo4;
            break;

        default:
           errorInfo = 0;
            break;
    }
#endif

#endif
    return errorInfo;

}
#endif

void getMotorError(void)
{
#if DEBUG_F_ERROR_FROM_MOTOR
    Uint16 errorStatus;
    Uint16 errorCodeTmp, errorCode;
    Uint16 errorAttributeTmp, errorAttribute;
    Uint16 errorInfo;
    Uint16 i;

    errorCode = ERROR_NONE;
    errorAttribute = GetErrorAttribute(errorCode);

    if ((!errorsCodeFromMotor[0]) && (!errorsCodeFromMotor[1]))
    {
        errorInfo = 0;

        errorCodeFromMotor = errorCode;

        errorInfoFromMotor = errorInfo;
        return;
    }

    for (i = 0; i < sizeof(errorCodeFromMotorList); i++)
    {
        if (i < ERR_LIST_LOW_WORD_BITS)
        {
            errorStatus = errorsCodeFromMotor[0] >> i;
        }
        else
        {
            errorStatus = errorsCodeFromMotor[1] >> (i - ERR_LIST_LOW_WORD_BITS);
        }

        if (!(errorStatus & 0x01))
        {
            continue;
        }

        errorCodeTmp = errorCodeFromMotorList[i];

        errorAttributeTmp = GetErrorAttribute(errorCodeTmp);

        if (errorAttributeTmp >= errorAttribute)
        {
            continue;
        }

        errorAttribute = errorAttributeTmp;
        errorCode = errorCodeTmp;

        errorInfo = (motorErrInfor[i / ERR_INFO_BITS_PER_CODE]
                     >> ((i % ERR_INFO_BITS_PER_CODE) * ERR_INFO_BITS_PER_CODE))
                    & ERR_INFO_MASK;

        if (ERROR_LEVEL_FREE_STOP == errorAttribute)
        {
            break;
        }
    }
#if 0
    else
    {
        errorCode = 0;
    }
#endif

    errorCodeFromMotor = errorCode;

    errorInfoFromMotor = errorInfo;
#endif
}

static Uint16 ClassifySpeedFault(Uint16 accCode, Uint16 decCode,
                                 Uint16 constCode)
{
    if (runFlag.bit.accDecStatus == ACC_SPEED)
    {
        return accCode;
    }

    if (runFlag.bit.accDecStatus == DEC_SPEED)
    {
        return decCode;
    }

    return constCode;
}

static void ConsiderError(Uint16 code, Uint16 *pErrorLevel)
{
    if (GetErrorAttribute(code) < *pErrorLevel)
    {
        errorOther = code;
        *pErrorLevel = GetErrorAttribute(errorOther);
    }
}

void GetNewError(void)
{
    static Uint16 devChkTicker = 0;
    static Uint16 osChkTicker = 0;
    static Uint16 motorSnPrev;
    Uint16 errorLevel;

#if DEBUG_F_ERROR_FROM_MOTOR

    getMotorError();
    errorInfo = errorInfoFromMotor;

    if ((generatrixVoltage >= funcCode.code.ovPointSet)
        && (powerOnStatus == POWER_ON_CORE_OK)
        )
    {
        errorOther = ERROR_OV_ACC_SPEED;
        errorInfo = 0;
    }

    if (ERROR_OC_ACC_SPEED == errorCodeFromMotor)
    {
        errorCode = ClassifySpeedFault(ERROR_OC_ACC_SPEED,
                                       ERROR_OC_DEC_SPEED,
                                       ERROR_OC_CONST_SPEED);
    }
    else if ((ERROR_OV_ACC_SPEED == errorCodeFromMotor)
            || (errorOther == ERROR_OV_ACC_SPEED))
    {
        errorCode = ClassifySpeedFault(ERROR_OV_ACC_SPEED,
                                       ERROR_OV_DEC_SPEED,
                                       ERROR_OV_CONST_SPEED);
    }
    else if (ERROR_UVW_FDB == errorCodeFromMotor)
    {
        errorCode = ERROR_ENCODER;
        errorCodeFromMotor = ERROR_ENCODER;
    }
    else if (errorCodeFromMotor && (ERROR_UV != errorCodeFromMotor))
    {
        errorCode = errorCodeFromMotor;
    }
    else
    {
        errorInfo = 0;
    }
#endif

    errorLevel = GetErrorAttribute(errorOther);

    if ((motorSnPrev != motorSn) && (runFlag.bit.run))
    {
        errorOther = ERROR_SWITCH_MOTOR_WHEN_RUN;
        errorLevel = ERROR_LEVEL_FREE_STOP;
    }
    motorSnPrev = motorSn;

    if ((diFunc.f1.bit.externalErrOpenIn)
        || ((!diFunc.f2.bit.externalErrCloseIn)
            && (diSelectFunc.f2.bit.externalErrCloseIn)))
    {
        if (GetErrorAttribute(ERROR_EXTERNAL) < errorLevel)
        {
            errorOther = ERROR_EXTERNAL;
            errorLevel = GetErrorAttribute(errorOther);

            errorInfo = (diFunc.f1.bit.externalErrOpenIn)
                      ? ERROR_EXTERNAL_INFO_OPEN
                      : ERROR_EXTERNAL_INFO_CLOSE;
        }
    }

    if (diFunc.f2.bit.userError1)
    {
        ConsiderError(ERROR_USER_1, &errorLevel);
    }

    if (diFunc.f2.bit.userError2)
    {
        ConsiderError(ERROR_USER_2, &errorLevel);
    }

    if (errorEeprom)
    {
        ConsiderError(ERROR_EEPROM, &errorLevel);
    }

    if ((ABS_INT32(frqRun - frq) > ((Uint32)maxFrq * funcCode.code.devChkValue / 1000))
        && (RUN_MODE_TORQUE_CTRL != runMode)
        && runFlag.bit.run
        && funcCode.code.devChkTime
        && (motorFc.motorCtrlMode == FUNCCODE_motorCtrlMode_FVC)
        && (!runFlag.bit.tune))
    {
        if (devChkTicker <= (funcCode.code.devChkTime
                             * (DEV_CHK_TIME_UNIT / ERROR_DEAL_PERIOD)
                             + DEV_CHK_TIME_MIN))
        {
            devChkTicker++;
        }
    }
    else
    {
        devChkTicker = 0;
    }

    if (devChkTicker > (funcCode.code.devChkTime
                        * (DEV_CHK_TIME_UNIT / ERROR_DEAL_PERIOD)))
    {
        ConsiderError(ERROR_DEV, &errorLevel);
    }

    if ((ABS_INT32(frqFdbTmp) > ((Uint32)maxFrq * (1000 + funcCode.code.osChkValue) / 1000))
        && (motorFc.motorCtrlMode == FUNCCODE_motorCtrlMode_FVC)
        && funcCode.code.osChkTime
        && (!runFlag.bit.tune)
        && (runFlag.bit.run))
    {
        if (osChkTicker <= (funcCode.code.osChkTime
                            * (OS_CHK_TIME_UNIT / ERROR_DEAL_PERIOD)))
        {
            osChkTicker++;
        }
    }
    else
    {
        osChkTicker = 0;
    }

    if (osChkTicker > (funcCode.code.osChkTime
                       * (OS_CHK_TIME_UNIT / ERROR_DEAL_PERIOD)))
    {
        ConsiderError(ERROR_OS, &errorLevel);
    }

#if DEBUG_F_PLC_CTRL

    if (funcCode.code.plcEnable && funcCode.code.plcErrorCode)
    {
        if ((funcCode.code.plcErrorCode >= PLC_DEFINE_ERROR_START)
            && (funcCode.code.plcErrorCode <= PLC_DEFINE_ERROR_END))
        {
            errorOther = funcCode.code.plcErrorCode;
            errorLevel = ERROR_LEVEL_FREE_STOP;
        }
    }

    funcCode.code.plcErrorCode = 0;
#endif

    if (errorLevel < GetErrorAttribute(errorCode))
    {
        errorCode = errorOther;
    }

    errorOtherOld = errorOther;
    errorOther = ERROR_NONE;
}

void GetNewErrorAttribute(void)
{
    if (ERROR_NONE == errorCode)
    {
        errorAttribute.bit.level = ERROR_LEVEL_NO_ERROR;
        return;
    }

    errorAttribute.bit.level = GetErrorAttribute(errorCode);
}

static void SaveErrorScene(void)
{
    funcCode.code.errorScene3.elem.errorFrq = ABS_INT32(frqRun);
    funcCode.code.errorScene3.elem.errorCurrent = outCurrentDispOld;
    funcCode.code.errorScene3.elem.errorGeneratrixVoltage = generatrixVoltage;
    funcCode.code.errorScene3.elem.errorDiStatus = diStatus.a.all;
    funcCode.code.errorScene3.elem.errorDoStatus = doStatus.a.all;
    funcCode.code.errorScene3.elem.errorInverterStatus = runFlag.all;
    funcCode.code.errorScene3.elem.errorTimeFromPowerUp = curTime.powerOnTime;
    funcCode.code.errorScene3.elem.errorTimeFromRun = curTime.runTime;
}

void ErrorSceneSave(void)
{
    if (!errorCode)
    {
        return;
    }

    if (errorCode == errorCodeOld)
    {
        return;
    }

    if ((errorCode >= PLC_DEFINE_ERROR_START)
        && (errorCode <= PLC_DEFINE_ERROR_END))
    {
        return;
    }

    funcCode.code.errorLatest1 = funcCode.code.errorLatest3;
    funcCode.code.errorLatest3 = errorCode;

    SaveErrorScene();

    funcCode.code.errorFrqUnit = ((funcCode.code.errorFrqUnit & ERROR_FRQ_UNIT_MASK)
                                  << ERROR_FRQ_UNIT_SHIFT)
                                 + funcCode.code.frqPoint;

    if ((ERROR_OC_ACC_SPEED == errorCodeFromMotor)
        && (currentOcDisp > outCurrentDispOld))
    {
        funcCode.code.errorScene3.elem.errorCurrent = currentOcDisp;
    }
}

void ErrorReset(void)
{
    GetErrorResetDeal();

    if (++errAutoRstClrWaitTicker > ERR_AUTO_RST_CLR_UNIT_NUM
                                     * TIME_UNIT_ERR_AUTO_RST_CLR
                                     / ERROR_DEAL_PERIOD)
    {
        errAutoRstClrWaitTicker = 0;
        errAutoRstNum = 0;
    }

    if (errorCode)
    {
        tuneCmd = 0;
        if (ERROR_LEVEL_FREE_STOP == errorAttribute.bit.level)
        {
            runFlag.bit.run = 0;

            dspMainCmd.bit.run = 0;
            dspMainCmd1.bit.speedTrack = 0;
            dspMainCmd.bit.stopBrake = 0;
            dspMainCmd.bit.startBrake = 0;
            dspMainCmd.bit.startFlux = 0;
            dspMainCmd.bit.accDecStatus = 0;
        }

        errAutoRstClrWaitTicker = 0;

        switch (errorDealStatus)
        {
            case ERROR_DEAL_PREPARE_FOR_WRITE_EEPROM:

                if (!funcCodeRwModeTmp)
                {
                    funcCodeRwModeTmp = FUNCCODE_RW_MODE_WRITE_SERIES;
                    startIndexWriteSeries = GetCodeIndex(funcCode.code.errorLatest1);
                    endIndexWriteSeries = GetCodeIndex(funcCode.code.errorScene1.all[sizeof(struct ERROR_SCENE_STRUCT) - 1]);

                    errorDealStatus = ERROR_DEAL_WAIT_FOR_WRITE_EEPROM;
                }
                break;

            case ERROR_DEAL_WAIT_FOR_WRITE_EEPROM:
                if (!funcCodeRwModeTmp)
                {
                    errorDealStatus = ERROR_DEAL_WRITE_EEPROM_OK;

                    errorEeprom = ERROR_EEPROM_NONE;
                }
                break;

            case ERROR_DEAL_WRITE_EEPROM_OK:
                if (AutoResetErrorDeal())
                {
                    errorSpaceTicker = 0;

                    if (errorCodeFromMotor)
                    {
                        ERROR_DEALING = 1;
                    }

                    errorDealStatus = ERROR_DEAL_OK;
                }

                break;

            case ERROR_DEAL_OK:

                ERROR_DEALING = 0;

                if (errorCode == errorOther)
                {
                    errorOther = ERROR_NONE;
                }
                if (errorCode == errorCodeFromMotor)
                {
                    errorCodeFromMotor = ERROR_NONE;
                }

                errorCode = ERROR_NONE;

                errorAttribute.bit.level = ERROR_LEVEL_NO_ERROR;

                errorDealStatus = ERROR_DEAL_PREPARE_FOR_WRITE_EEPROM;
                break;

            default:
                break;
        }
    }
}

extern Uint16 parallelMasterSendErrorResetTicker;
void GetErrorResetDeal(void)
{
    static Uint16 errorResetInOld;

    if ((!errorResetInOld) && (diFunc.f1.bit.errorReset))
    {
        runCmd.bit.errorReset = 1;
    }
    errorResetInOld = diFunc.f1.bit.errorReset;

    if ((KEY_STOP == keyFunc)
        && ((ERROR_LEVEL_RUN != errorAttribute.bit.level)
            || (!runFlag.bit.run)))
    {
        runCmd.bit.errorReset = 1;
    }
}

Uint16 AutoResetErrorDeal(void)
{
    if (AutoResetErrorTrueDeal())
    {
        return 1;
    }

    if (AutoResetErrorFcDeal())
    {
        return 1;
    }

    return 0;
}

Uint16 AutoResetErrorTrueDeal(void)
{
    if (ERROR_UV == errorCode)
    {
        return (Uint16)(!bUv);
    }

    if (ERROR_EEPROM == errorCode)
    {
        return (Uint16)(ERROR_EEPROM_NONE == errorEeprom);
    }

#if DEBUG_F_ERROR_LEVEL

    if (ERROR_LEVEL_RUN == errorAttribute.bit.level)
    {
        if ((errorCode != errorOtherOld) && (errorCode != errorCodeFromMotor))
        {
            return 1;
        }
    }
#endif

    return 0;
}

Uint16 AutoResetErrorFcDeal(void)
{
    Uint16 spaceTime;

    if (ERROR_MOTOR_SHORT_TO_GND == errorCode)
    {
        return 0;
    }

    if (runCmd.bit.errorReset)
    {
        errAutoRstNum = 0;

        if ((ERROR_LEVEL_FREE_STOP == errorAttribute.bit.level) || (!runFlag.bit.run))
        {
            return 1;
        }

        return 0;
    }

    if (errAutoRstNum >= funcCode.code.errAutoRstNumMax)
    {
        return 0;
    }

    if ((errorCode == ERROR_OC_ACC_SPEED)
        || (errorCode == ERROR_OC_DEC_SPEED)
        || (errorCode == ERROR_OC_CONST_SPEED))
    {
        spaceTime = (funcCode.code.errAutoRstSpaceTime > ERR_AUTO_RST_MIN_SPACE)
                  ? funcCode.code.errAutoRstSpaceTime
                  : ERR_AUTO_RST_MIN_SPACE;
    }
    else
    {
        spaceTime = funcCode.code.errAutoRstSpaceTime;
    }

    if (++errorSpaceTicker >= (Uint32)spaceTime
        * (Uint16)(TIME_UNIT_ERR_AUTO_RST_DELAY / RUN_CTRL_PERIOD))
    {
        errAutoRstNum++;
        runCmd.bit.startProtect = 0;

        return 1;
    }

    return 0;
}

static Uint16 GetActionLevel(Uint16 group, Uint16 index)
{
    Uint16 digit[5];

    GetNumberDigit1(digit, funcCode.code.errorAction[group]);

    return digit[index] + 1;
}

Uint16 GetErrorAttribute(Uint16 errCode)
{
    Uint16 level;

    level = ERROR_LEVEL_FREE_STOP;
#if DEBUG_F_ERROR_LEVEL
    if (errCode == ERROR_NONE)
    {
        level = ERROR_NONE_LEVEL;
    }
    else if ((errCode == ERROR_OL_MOTOR)
        || (errCode == ERROR_LOSE_PHASE_INPUT)
        || (errCode == ERROR_LOSE_PHASE_OUTPUT)
        || (errCode == ERROR_EXTERNAL)
        || (errCode == ERROR_COMM))
    {
        if (errCode < ERROR_OT_IGBT)
        {
            level = GetActionLevel(0, errCode - ERROR_OL_MOTOR);
        }
        else
        {
            level = GetActionLevel(0, errCode - ERROR_LOSE_PHASE_INPUT);
        }
    }
    else if ((errCode == ERROR_ENCODER)
        || (errCode == ERROR_EEPROM)
        || (errCode == ERROR_24)
        || (errCode == ERROR_25)
        || (errCode == ERROR_RUN_TIME_OVER))
    {
        if (errCode < ERROR_22)
        {
            level = GetActionLevel(1, errCode - ERROR_ENCODER);
        }
        else
        {
            level = GetActionLevel(1, errCode - ERROR_24 + 2);
        }
    }
    else if ((errCode == ERROR_USER_1)
        || (errCode == ERROR_USER_2)
        || (errCode == ERROR_POWER_UP_TIME_OVER)
        || (errCode == ERROR_LOSE_LOAD)
        || (errCode == ERROR_FDB_LOSE))
    {
        level = GetActionLevel(2, errCode - ERROR_USER_1);
    }
    else if ((errCode == ERROR_DEV)
        || (errCode == ERROR_OS))
    {
        level = GetActionLevel(3, errCode - ERROR_DEV);
    }
    else if (errCode == ERROR_INIT_POSITION)
    {
        level = GetActionLevel(3, 2);
    }
#if 0
    else if (errCode == ERROR_SPEED_DETECT)
    {
        GetNumberDigit1(digit, funcCode.code.errorAction[3]);
        level = digit[3] + 1;
    }
    else if (errCode == ERROR_PROGRAM_LOGIC)
    {
        GetNumberDigit1(digit, funcCode.code.errorAction[3]);
        level = digit[4] + 1;
    }
#endif
#endif
    return level;
}

#elif 1

void ErrorDeal(void){}

#endif
