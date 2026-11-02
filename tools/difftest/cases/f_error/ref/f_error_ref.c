
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

    if (bUv)
    {
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

                if ((dspStatus.bit.uv)

                    )
                {

                    uvDealStatus = UV_READY_FOR_WRITE_EEPROM;
                    bUv = 0;
                }
                break;

            default:
                break;
        }
    }
}

void UpdateError(void)
{

    {
        errorCodeOld = errorCode;
        errorAttributeOld.all = errorAttribute.all;
        errorInfoOld = errorInfo;
    }

    GetNewError();

    GetNewErrorAttribute();

    if (errorCodeOld != errorCode)
    {
        if (ERROR_LEVEL_NO_ERROR != errorAttribute.bit.level)
        {
            if (ERROR_LEVEL_NO_ERROR == errorAttributeOld.bit.level)
            {
                ;
            }
#if DEBUG_F_ERROR_LEVEL

            else if (errorAttribute.bit.level < errorAttributeOld.bit.level)
            {
                ;
            }

            else if (errorAttributeOld.bit.level == ERROR_LEVEL_RUN)
			{
				;
			}
#endif

            else
            {
                errorCode = errorCodeOld;
                errorAttribute.all = errorAttributeOld.all;
            }
        }
        else
        {
            errorCode = errorCodeOld;
            errorAttribute.all = errorAttributeOld.all;
        }
    }
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

    if (errorsCodeFromMotor[0] || errorsCodeFromMotor[1])
    {
        for (i = 0; i < sizeof(errorCodeFromMotorList); i++)
        {

            if ( i < 16)
            {

                errorStatus = errorsCodeFromMotor[0] >> i;
            }

            else
            {

                errorStatus = errorsCodeFromMotor[1] >> (i - 16);
            }

            if (errorStatus & 0x01)
            {

                errorCodeTmp = errorCodeFromMotorList[i];

                errorAttributeTmp = GetErrorAttribute(errorCodeTmp);

                if (errorAttributeTmp < errorAttribute)
                {
                    errorAttribute = errorAttributeTmp;
                    errorCode = errorCodeTmp;

                    errorInfo = (motorErrInfor[i/4] >> ((i%4)*4)) & 0xF;

                    if (ERROR_LEVEL_FREE_STOP == errorAttribute)
                    {
                        break;
                    }
                }
            }

        }
    }
    else
    {
        errorInfo = 0;
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

#define ERROR_EXTERNAL_INFO_OPEN    1
#define ERROR_EXTERNAL_INFO_CLOSE   2
#define DEV_CHK_TIME_MIN            5
void GetNewError(void)
{
    static Uint16 devTcnt = 0;
    static Uint16  osTcnt = 0;
    static Uint16 motorSnOld;
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
        if (runFlag.bit.accDecStatus == ACC_SPEED)
        {
            errorCode = ERROR_OC_ACC_SPEED;
        }
        else if (runFlag.bit.accDecStatus == DEC_SPEED)
        {
            errorCode = ERROR_OC_DEC_SPEED;
        }
        else
        {
            errorCode = ERROR_OC_CONST_SPEED;
        }
    }
    else if ((ERROR_OV_ACC_SPEED == errorCodeFromMotor)
            || (errorOther == ERROR_OV_ACC_SPEED)
            )
    {
        if (runFlag.bit.accDecStatus == ACC_SPEED)
        {
            errorCode = ERROR_OV_ACC_SPEED;
        }
        else if (runFlag.bit.accDecStatus == DEC_SPEED)
        {
            errorCode = ERROR_OV_DEC_SPEED;
        }
        else
        {
            errorCode = ERROR_OV_CONST_SPEED;
        }

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

    if ((motorSnOld != motorSn)
        && (runFlag.bit.run)
        )
    {
        errorOther = ERROR_SWITCH_MOTOR_WHEN_RUN;
        errorLevel = ERROR_LEVEL_FREE_STOP;
    }
    motorSnOld = motorSn;

	if ((diFunc.f1.bit.externalErrOpenIn) ||
        ( (!diFunc.f2.bit.externalErrCloseIn) &&
          (diSelectFunc.f2.bit.externalErrCloseIn)
         )
        )
    {
	    if(GetErrorAttribute(ERROR_EXTERNAL) < errorLevel)
	    {
            errorOther = ERROR_EXTERNAL;
			errorLevel = GetErrorAttribute(errorOther);

            if (diFunc.f1.bit.externalErrOpenIn)
            {
                errorInfo = ERROR_EXTERNAL_INFO_OPEN;
            }
            else
            {
                errorInfo = ERROR_EXTERNAL_INFO_CLOSE;
            }
        }
    }

	if(diFunc.f2.bit.userError1)
    {
	    if(GetErrorAttribute(ERROR_USER_1) < errorLevel)
	    {
            errorOther = ERROR_USER_1;
			errorLevel = GetErrorAttribute(errorOther);
        }
    }

	if(diFunc.f2.bit.userError2)
    {
	    if(GetErrorAttribute(ERROR_USER_2) < errorLevel)
	    {
            errorOther = ERROR_USER_2;
			errorLevel = GetErrorAttribute(errorOther);
        }
    }

	if (errorEeprom)
    {
	    if(GetErrorAttribute(ERROR_EEPROM) < errorLevel)
	    {
            errorOther = ERROR_EEPROM;
			errorLevel = GetErrorAttribute(errorOther);
        }
    }

    if( (ABS_INT32(frqRun - frq) > ((Uint32)maxFrq*funcCode.code.devChkValue/1000))
	    && (RUN_MODE_TORQUE_CTRL != runMode)
	    && runFlag.bit.run
	    && funcCode.code.devChkTime
	    && (motorFc.motorCtrlMode == FUNCCODE_motorCtrlMode_FVC)
	    && (!runFlag.bit.tune)
       )
    {
        if(devTcnt <= (funcCode.code.devChkTime*(100/ERROR_DEAL_PERIOD) + DEV_CHK_TIME_MIN))
        {
            devTcnt++;
        }
    }
    else
    {
        devTcnt = 0;
    }

    if (devTcnt > (funcCode.code.devChkTime*(100/ERROR_DEAL_PERIOD)))
    {
	    if (GetErrorAttribute(ERROR_DEV) < errorLevel)
	    {
            errorOther = ERROR_DEV;
			errorLevel = GetErrorAttribute(errorOther);
        }
    }

    if ((ABS_INT32(frqFdbTmp) > ((Uint32)maxFrq * (1000 + funcCode.code.osChkValue)/1000))
        && (motorFc.motorCtrlMode == FUNCCODE_motorCtrlMode_FVC)
        && funcCode.code.osChkTime
        && (!runFlag.bit.tune)
        && (runFlag.bit.run)
       )
    {
        if (osTcnt <= (funcCode.code.osChkTime*(100/ERROR_DEAL_PERIOD)))
        {
            osTcnt++;
        }
    }
    else
    {
        osTcnt = 0;
    }

	if (osTcnt > (funcCode.code.osChkTime*(100/ERROR_DEAL_PERIOD)))
    {
    	if (GetErrorAttribute(ERROR_OS) < errorLevel)
    	{
           errorOther = ERROR_OS;
		   errorLevel = GetErrorAttribute(errorOther);
        }
    }

#if DEBUG_F_PLC_CTRL

    if (funcCode.code.plcEnable && funcCode.code.plcErrorCode)
    {
		if((funcCode.code.plcErrorCode >= PLC_DEFINE_ERROR_START)
		 && (funcCode.code.plcErrorCode <= PLC_DEFINE_ERROR_END)
		 )
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
    if (errorCode == ERROR_NONE)
    {
       errorAttribute.bit.level = ERROR_LEVEL_NO_ERROR;
    }
    else
    {
        errorAttribute.bit.level = GetErrorAttribute(errorCode);
    }
}

void ErrorSceneSave(void)
{
    if (errorCode
        && (errorCode != errorCodeOld)
        && ((errorCode < PLC_DEFINE_ERROR_START) || (errorCode > PLC_DEFINE_ERROR_END))
        )
    {
        int16 i;

        funcCode.code.errorLatest1 = funcCode.code.errorLatest3;
        funcCode.code.errorLatest3 = errorCode;

#define ERROR_SCENE_NUMBER  8

        funcCode.code.errorScene3.elem.errorFrq = ABS_INT32(frqRun);
        funcCode.code.errorScene3.elem.errorCurrent = outCurrentDispOld;
        funcCode.code.errorScene3.elem.errorGeneratrixVoltage = generatrixVoltage;
        funcCode.code.errorScene3.elem.errorDiStatus = diStatus.a.all;
        funcCode.code.errorScene3.elem.errorDoStatus = doStatus.a.all;
        funcCode.code.errorScene3.elem.errorInverterStatus = runFlag.all;
        funcCode.code.errorScene3.elem.errorTimeFromPowerUp = curTime.powerOnTime;
        funcCode.code.errorScene3.elem.errorTimeFromRun = curTime.runTime;

        funcCode.code.errorFrqUnit = ((funcCode.code.errorFrqUnit & 0x0FFF) << 4) + funcCode.code.frqPoint;

        if (ERROR_OC_ACC_SPEED == errorCodeFromMotor)
        {

            if (currentOcDisp > outCurrentDispOld)
            {
                funcCode.code.errorScene3.elem.errorCurrent = currentOcDisp;
            }
        }
    }
}

void ErrorReset(void)
{
    GetErrorResetDeal();

    {

        if (++errAutoRstClrWaitTicker > 10*TIME_UNIT_ERR_AUTO_RST_CLR / ERROR_DEAL_PERIOD)
        {
            errAutoRstClrWaitTicker = 0;
            errAutoRstNum = 0;
        }
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
    static Uint16 diFuncErrRstOld;

    if ((!diFuncErrRstOld) && (diFunc.f1.bit.errorReset))
    {
        runCmd.bit.errorReset = 1;
    }
    diFuncErrRstOld = diFunc.f1.bit.errorReset;

    if ((KEY_STOP == keyFunc)
        && ((ERROR_LEVEL_RUN != errorAttribute.bit.level)
            || (!runFlag.bit.run)
            )
        )
    {
        runCmd.bit.errorReset = 1;
    }
}

Uint16 AutoResetErrorDeal(void)
{
    Uint16 bResetError = 0;

    if ((AutoResetErrorTrueDeal())
        || AutoResetErrorFcDeal())
        bResetError = 1;

    return bResetError;
}

Uint16 AutoResetErrorTrueDeal(void)
{
    Uint16 bResetError = 0;

    if (ERROR_UV == errorCode)
    {
        if (!bUv)
        {
            bResetError = 1;
        }
    }
    else if (ERROR_EEPROM == errorCode)
    {
        if (ERROR_EEPROM_NONE == errorEeprom)
        {
            bResetError = 1;
        }
    }
#if DEBUG_F_ERROR_LEVEL

    else if (ERROR_LEVEL_RUN == errorAttribute.bit.level)
    {

        if((errorCode != errorOtherOld) &&
           (errorCode != errorCodeFromMotor))
        {
            bResetError = 1;
        }
    }
#endif
    return bResetError;
}

Uint16 AutoResetErrorFcDeal(void)
{
    Uint16 bResetError = 0;
    Uint16 errAutoRstSpaceTime;

    if (ERROR_MOTOR_SHORT_TO_GND != errorCode)
    {
        if (runCmd.bit.errorReset)
        {

            errAutoRstNum = 0;

            if (((ERROR_LEVEL_FREE_STOP == errorAttribute.bit.level) || (!runFlag.bit.run))
               )
            {
                bResetError = 1;
            }
        }
        else

        {

            if (errAutoRstNum < funcCode.code.errAutoRstNumMax)
            {

                if ((errorCode == ERROR_OC_ACC_SPEED)
                    || (errorCode == ERROR_OC_DEC_SPEED)
                    || (errorCode == ERROR_OC_CONST_SPEED))
                {
                    errAutoRstSpaceTime = ((funcCode.code.errAutoRstSpaceTime > 5) ? funcCode.code.errAutoRstSpaceTime : 5);
                }
                else
                {
                    errAutoRstSpaceTime = funcCode.code.errAutoRstSpaceTime;
                }

                if (++errorSpaceTicker >= (Uint32)errAutoRstSpaceTime
                    * (Uint16)(TIME_UNIT_ERR_AUTO_RST_DELAY / RUN_CTRL_PERIOD))
                {
                    errAutoRstNum++;
                    runCmd.bit.startProtect = 0;

                    bResetError = 1;
                }
            }
        }
    }

    return bResetError;
}

Uint16 GetErrorAttribute(Uint16 errCode)
{
    Uint16 digit[5];
    Uint16 level;

    level = ERROR_LEVEL_FREE_STOP;
#if DEBUG_F_ERROR_LEVEL
    if (errCode == ERROR_NONE)
	{
        level = 4;
    }

    else if ((errCode == ERROR_OL_MOTOR)
    	||  (errCode == ERROR_LOSE_PHASE_INPUT)
    	||  (errCode == ERROR_LOSE_PHASE_OUTPUT)
    	||  (errCode == ERROR_EXTERNAL)
   		||  (errCode == ERROR_COMM)
		)
	{
        GetNumberDigit1(digit, funcCode.code.errorAction[0]);
        if(errCode < ERROR_OT_IGBT)
        {
            level = digit[errCode - ERROR_OL_MOTOR] + 1;
        }
        else
        {
            level = digit[errCode - ERROR_LOSE_PHASE_INPUT] + 1;
        }
	}
    else if( (errCode == ERROR_ENCODER)
        ||  (errCode == ERROR_EEPROM)
        ||  (errCode == ERROR_24)
        ||  (errCode == ERROR_25)
        ||  (errCode == ERROR_RUN_TIME_OVER)
		)
	{
        GetNumberDigit1(digit, funcCode.code.errorAction[1]);
        if(errCode < ERROR_22)
        {
            level = digit[errCode - ERROR_ENCODER] + 1;
        }
        else
        {
            level = digit[errCode - ERROR_24 + 2] + 1;
        }
    }
    else if( (errCode == ERROR_USER_1)
    	||  (errCode == ERROR_USER_2)
    	||  (errCode == ERROR_POWER_UP_TIME_OVER)
    	||  (errCode == ERROR_LOSE_LOAD)
    	||  (errCode == ERROR_FDB_LOSE)
		)
	{
        GetNumberDigit1(digit, funcCode.code.errorAction[2]);
        level = digit[errCode - ERROR_USER_1] + 1;
    }
    else if ((errCode == ERROR_DEV)
    	||  (errCode == ERROR_OS)
		)
	{
        GetNumberDigit1(digit, funcCode.code.errorAction[3]);
        level = digit[errCode - ERROR_DEV] + 1;
    }
    else if (errCode == ERROR_INIT_POSITION)
	{
        GetNumberDigit1(digit, funcCode.code.errorAction[3]);
        level = digit[2] + 1;
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
