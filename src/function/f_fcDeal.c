
#include "f_menu.h"
#include "f_main.h"
#include "f_eeprom.h"
#include "f_invPara.h"
#include "f_fcDeal.h"
#include "f_runSrc.h"
#include "f_frqSrc.h"

#if F_DEBUG_RAM
#define DEBUG_F_P_OFF_REM               0
#elif 1
#define DEBUG_F_P_OFF_REM               1
#endif

#define LIMIT_PASS_NUM                  2

#define PLC_TIME_HIGH_MASK              0xFFFF0000UL
#define PLC_TIME_LOW_MASK               0x0000FFFFUL
#define PLC_TIME_HIGH_SHIFT             16

#define CLEAR_RECORD_SINGLE_NUM         6
#define CLEAR_RECORD_ERROR_FRQ_VALUE    0x222

#define RATING_VOLTAGE_T1140            1140
#define SVC_MODE_FORBIDDEN              0
#define SVC_MODE_ALLOWED                1

#if DEBUG_F_POSITION_CTRL
#pragma DATA_SECTION(limitedByOtherCodeIndex, "data_ram");
#endif

#define LIMITED_BY_OTHER_CODE_NUM_MAX   150

Uint16 limitedByOtherCodeIndex[LIMITED_BY_OTHER_CODE_NUM_MAX];
Uint16 limitedByOtherCodeIndexNum;
#if DEBUG_F_LIMIT_OTHER_CODE

#define LIMIT_OTHER_CODE_NUM  13
LOCALF Uint16 const limitOtherCodeIndex[LIMIT_OTHER_CODE_NUM] =
{
    PULSE_IN_MAX_INDEX,
    PULSE_IN_MIN_INDEX,
    CURVE2_MAX_INDEX,
    CURVE2_MIN_INDEX,
    CURVE1_MAX_INDEX,
    CURVE1_MIN_INDEX,
    VF_FRQ3_INDEX,
    VF_FRQ2_INDEX,
    VF_FRQ1_INDEX,
    RATING_FRQ_INDEX,
    LOWER_FRQ_INDEX,
    UPPER_FRQ_INDEX,
    MAX_FRQ_INDEX,
};
#endif

#if DEBUG_F_RESTORE_COMPANY_PARA_DEAL

static Uint16 IsInitExceptSeries(Uint16 index, Uint16 type)
{
    if (((INIT_EXCEPT_SERIES_S_0 <= index) && (index <= INIT_EXCEPT_SERIES_E_0))
        || ((INIT_EXCEPT_SERIES_S_1 <= index) && (index <= INIT_EXCEPT_SERIES_E_1))
        || ((INIT_EXCEPT_SERIES_S_4 <= index) && (index <= INIT_EXCEPT_SERIES_E_4))
        || ((INIT_EXCEPT_SERIES_S_5 <= index) && (index <= INIT_EXCEPT_SERIES_E_5))
        || ((((INIT_EXCEPT_SERIES_S_2 <= index) && (index <= INIT_EXCEPT_SERIES_E_2))
            || ((INIT_EXCEPT_SERIES_S_3 <= index) && (index <= INIT_EXCEPT_SERIES_E_3))
            || ((INIT_EXCEPT_SERIES_S_6 <= index) && (index <= INIT_EXCEPT_SERIES_E_6))
            || ((INIT_EXCEPT_SERIES_S_7 <= index) && (index <= INIT_EXCEPT_SERIES_E_7)))
            && (!type)))
    {
        return 1;
    }

    return 0;
}

static Uint16 IsInitExceptSingle(Uint16 index, Uint16 type)
{
    if ((INIT_EXCEPT_SINGLE_0 == index)
        || (INIT_EXCEPT_SINGLE_1 == index)
        || (INIT_EXCEPT_SINGLE_2 == index)
        || (INIT_EXCEPT_SINGLE_3 == index)
        || (INIT_EXCEPT_SINGLE_4 == index)
        || ((INIT_EXCEPT_SINGLE_5 == index) && (!type))
        || (INIT_EXCEPT_SINGLE_6 == index)
        || (INIT_EXCEPT_SINGLE_7 == index)
        || (INIT_EXCEPT_SINGLE_8 == index)
        || (INIT_EXCEPT_SINGLE_9 == index)
        || (INIT_EXCEPT_SINGLE_10 == index)
        || (INIT_EXCEPT_SINGLE_11 == index))
    {
        return 1;
    }

    return 0;
}

static Uint16 IsCalibrateCurveCode(Uint16 index)
{
    if ((GetCodeIndex(funcCode.code.aiCalibrateCurve[0].before1) <= index)
        && (index <= GetCodeIndex(funcCode.code.aoCalibrateCurve[AO_NUMBER - 1].after2)))
    {
        return 1;
    }

    return 0;
}

static Uint16 GetCalibrateCurveInit(Uint16 index)
{
    if ((GetCodeIndex(funcCode.code.aiCalibrateCurve[2].before1) <= index)
        && (index <= GetCodeIndex(funcCode.code.aiCalibrateCurve[2].after2)))
    {
        return funcCode.all[index];
    }

    if ((GetCodeIndex(funcCode.code.aoCalibrateCurve[1].before1) <= index)
        && (index <= GetCodeIndex(funcCode.code.aoCalibrateCurve[1].after2)))
    {
        return funcCode.all[index];
    }

    return funcCode.group.ae[index - GetCodeIndex(funcCode.code.aiCalibrateCurve[0].before1)];
}

static Uint16 IsAccDecTimeCode(Uint16 index)
{
    if ((ACC_TIME1_INDEX == index)
        || (DEC_TIME1_INDEX == index)
        || (ACC_TIME2_INDEX == index)
        || (DEC_TIME2_INDEX == index)
        || (ACC_TIME3_INDEX == index)
        || (DEC_TIME3_INDEX == index)
        || (ACC_TIME4_INDEX == index)
        || (DEC_TIME4_INDEX == index))
    {
        return 1;
    }

    return 0;
}

#endif

Uint16 GetFuncCodeInit(Uint16 index, Uint16 type)
{

#if DEBUG_F_RESTORE_COMPANY_PARA_DEAL
    Uint16 value;

    if (IsInitExceptSeries(index, type) || IsInitExceptSingle(index, type))
    {
        value = funcCode.all[index];
    }

    else if (IsCalibrateCurveCode(index))
    {
        value = GetCalibrateCurveInit(index);
    }

    else if (IsAccDecTimeCode(index))
    {
        if (invPara.type < invPara.bitAccDecStart)
        {
            value = ACC_DEC_T_INIT1;
        }
        else
        {
            value = ACC_DEC_T_INIT2;
        }
    }
#if (DEBUG_F_INV_TYPE_RELATE || DEBUG_F_MOTOR_POWER_RELATE)

    else if (CARRIER_FRQ_INDEX == index)
    {
        value = GetInvParaPointer(invPara.type)->elem.carrierFrq;
    }

    else if ((ANTI_VIBRATE_GAIN_INDEX == index)
            || (ANTI_VIBRATE_GAIN_MOTOR2_INDEX == index)
            || (ANTI_VIBRATE_GAIN_MOTOR3_INDEX == index)
            || (ANTI_VIBRATE_GAIN_MOTOR4_INDEX == index)
            )
    {
        value = GetInvParaPointer(invPara.type)->elem.antiVibrateGain;
    }

    else if (TORQUE_BOOST_INDEX == index)
    {
        value = TorqueBoostDeal(funcCode.code.motorParaM1.elem.ratingPower);
    }

    else if (TORQUE_BOOST_MOTOR2_INDEX == index)
    {
        value = TorqueBoostDeal(funcCode.code.motorFcM2.motorPara.elem.ratingPower);
    }

    else if (TORQUE_BOOST_MOTOR3_INDEX == index)
    {
        value = TorqueBoostDeal(funcCode.code.motorFcM3.motorPara.elem.ratingPower);
    }

    else if (TORQUE_BOOST_MOTOR4_INDEX == index)
    {
        value = TorqueBoostDeal(funcCode.code.motorFcM4.motorPara.elem.ratingPower);
    }

    else if (SVC_MODE_INDX == index)
    {
        if (invPara.ratingVoltage == RATING_VOLTAGE_T1140)
        {
            value = SVC_MODE_FORBIDDEN;
        }
        else
        {
            value = SVC_MODE_ALLOWED;
        }
    }

    else if (OV_POINT_SET_INDEX == index)
    {
        value = ovVoltageInitValue[invPara.volLevel];
    }
#endif

    else
    {
        value = GetFuncCodeInitOriginal(index);
    }

    return value;
#endif
}

void ClearRecordDeal(void)
{
#if DEBUG_F_CLEAR_RECORD
    int16 i;
    Uint16 n;
    const Uint16 clearRecordSingleIndex[CLEAR_RECORD_SINGLE_NUM] =
    {
        CLEAR_RECORD_SINGLE_0,
        CLEAR_RECORD_SINGLE_1,
        CLEAR_RECORD_SINGLE_2,
        CLEAR_RECORD_SINGLE_3,
        CLEAR_RECORD_SINGLE_4,
        CLEAR_RECORD_SINGLE_5,
    };
    const Uint16 clearRecordSingleValue[CLEAR_RECORD_SINGLE_NUM] =
    {
        0,
        0,
        0,
        0,
        0,
        CLEAR_RECORD_ERROR_FRQ_VALUE,
    };

    funcCodeRwModeTmp = FUNCCODE_RW_MODE_WRITE_SERIES;
    startIndexWriteSeries = CLEAR_RECORD_SERIES_S_0;
    endIndexWriteSeries = CLEAR_RECORD_SERIES_E_0;

    for (i = startIndexWriteSeries; i <= endIndexWriteSeries; i++)
    {
        funcCode.all[i] = 0;
    }

    for (n = 0; n < CLEAR_RECORD_SINGLE_NUM; n++)
    {
        funcCode.all[clearRecordSingleIndex[n]] = clearRecordSingleValue[n];
        SaveOneFuncCode(clearRecordSingleIndex[n]);
    }
#endif
}

void RestoreCompanyParaOtherDeal(void)
{
#if DEBUG_F_RESTORE_COMPANY_PARA_DEAL
    Uint16 motorDebugFcIndex;
    Uint16 aiaoCalibrateDispIndex;

    motorDebugFcIndex = GetCodeIndex(funcCode.code.motorDebugFc);
    aiaoCalibrateDispIndex = GetCodeIndex(funcCode.code.aiaoCalibrateDisp);

    funcCode.code.motorDebugFc = 0;
    funcCode.code.aiaoCalibrateDisp = 0;

    SaveOneFuncCode(motorDebugFcIndex);
    SaveOneFuncCode(aiaoCalibrateDispIndex);
    MenuModeDeal();

    LimitOtherCodeDeal(RATING_FRQ_INDEX);

    upDownFrq = 0;
    frqFlag.bit.upDownoperationStatus = UP_DN_OPERATION_OFF;
    runCmd.all = 0;
    runDirPanelOld = 0;
#endif
}

void PowerOffRemDeal(void)
{
#if DEBUG_F_P_OFF_REM

#if DEBUG_F_MOTOR_FUNCCODE
    int16 i;
#endif

    SaveOneFuncCode(GetCodeIndex(funcCode.code.lengthCurrent));

    SaveOneFuncCode(RUN_TIME_ADDUP_INDEX);

    SaveOneFuncCode(POWER_TIME_ADDUP_INDEX);

    SaveOneFuncCode(POWER_ADDUP_INDEX);

    if (saveEepromIndex)
    {
        SaveOneFuncCode(saveEepromIndex);
    }

    funcCode.code.upDownFrqRem = upDownFrq;

    funcCode.code.plcStepRem = plcStep;
    if (plcStep >= PLC_STEP_MAX)
    {
        funcCode.code.plcStepRem = 0;
    }
    funcCode.code.plcTimeHighRem = (plcTime & PLC_TIME_HIGH_MASK) >> PLC_TIME_HIGH_SHIFT;
    funcCode.code.plcTimeLowRem = plcTime & PLC_TIME_LOW_MASK;

    funcCode.code.pmsmRotorPos = pmsmRotorPos;

#if DEBUG_F_MOTOR_FUNCCODE
#if !F_DEBUG_RAM
    for (i = REM_P_OFF_MOTOR - 1; i >= 0; i--)
    {
        funcCode.code.remPOffMotorCtrl[i] = gSendToFunctionDataBuff[i + MOTOR_TO_Func_2MS_DATA_NUM];
    }
#endif
#endif

    funcCodeRwModeTmp = FUNCCODE_RW_MODE_WRITE_SERIES;
    startIndexWriteSeries = GetCodeIndex(funcCode.group.remember[0]);
    endIndexWriteSeries = GetCodeIndex(funcCode.group.remember[REM_NUM - 1]);

#endif
}

#if DEBUG_F_LIMIT_OTHER_CODE

static Uint16 LimitOneCode(Uint16 codeIndex)
{
    const FUNCCODE_ATTRIBUTE *pAttribute = &funcCodeAttribute[codeIndex];
    Uint16 *pCode = &funcCode.all[codeIndex];
    Uint16 clamped = 0;

    if (pAttribute->attribute.bit.upperLimit)
    {
        int32 fc1;
        int32 upper1;
        Uint16 upper;

        upper = funcCode.all[pAttribute->upper];

        if (pAttribute->attribute.bit.signal)
        {
            fc1 = (int32)(int16)*pCode;
            upper1 = (int32)(int16)upper;
        }
        else
        {
            fc1 = *pCode;
            upper1 = upper;
        }

        if (fc1 > upper1)
        {
            *pCode = upper;
            SaveOneFuncCode(codeIndex);

            clamped = 1;
        }
    }

    if (pAttribute->attribute.bit.lowerLimit)
    {
        int32 fc1;
        int32 lower1;
        Uint16 lower;

        lower = funcCode.all[pAttribute->lower];

        if (pAttribute->attribute.bit.signal)
        {
            fc1 = (int32)(int16)*pCode;
            lower1 = (int32)(int16)lower;
        }
        else
        {
            fc1 = *pCode;
            lower1 = lower;
        }

        if (fc1 < lower1)
        {
            if (!clamped)
            {
                *pCode = lower;
                SaveOneFuncCode(codeIndex);
            }
            else
            {
                funcCode.all[pAttribute->lower] = *pCode;
                SaveOneFuncCode(pAttribute->lower);
            }
        }
    }

    return clamped;
}

#endif

LOCALF void LimitOtherCodeDeal(Uint16 index)
{
#if DEBUG_F_LIMIT_OTHER_CODE
    int16 scan;
    int16 pass;

    for (scan = LIMIT_OTHER_CODE_NUM - 1; scan >= 0; scan--)
    {
        if (limitOtherCodeIndex[scan] == index)
        {
            for (pass = LIMIT_PASS_NUM - 1; pass >= 0; pass--)
            {
                int16 entry;

                entry = (int16)limitedByOtherCodeIndexNum - 1;
                for (; entry >= 0; entry--)
                {
                    LimitOneCode(limitedByOtherCodeIndex[entry]);
                }
            }

            break;
        }
    }
#endif
}

Uint16 GetFuncCodeInitOriginal(Uint16 index)
{
    Uint16 init;

    if (index < FNUM_PARA)
    {
        init = (funcCodeAttribute[index].init);
    }
#if 0
    else if (EEPROM_CHECK_INDEX1 == index)
    {
        init = EEPROM_CHECK1;
    }
    else if (EEPROM_CHECK_INDEX2 == index)
    {
        init = EEPROM_CHECK2;
    }
    else if (AI_AO_CHK_FLAG == index)
    {
        init = AIAO_CHK_WORD;
    }
#endif
    else if (SAVE_USER_PARA_PARA1 == index)
    {
        init = funcCode.code.saveUserParaFlag1;
    }
    else if (SAVE_USER_PARA_PARA2 == index)
    {
        init = funcCode.code.saveUserParaFlag2;
    }
    else
    {
        init = 0;
    }

    return init;
}
