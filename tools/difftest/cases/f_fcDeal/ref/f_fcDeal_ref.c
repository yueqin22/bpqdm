
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

Uint16 GetFuncCodeInit(Uint16 index, Uint16 type)
{

#if DEBUG_F_RESTORE_COMPANY_PARA_DEAL
    Uint16 init;

    if (((INIT_EXCEPT_SERIES_S_0 <= index)
        && (index <= INIT_EXCEPT_SERIES_E_0))
      || ((INIT_EXCEPT_SERIES_S_1 <= index)
        && (index <= INIT_EXCEPT_SERIES_E_1))
      || ((INIT_EXCEPT_SERIES_S_4 <= index)
        && (index <= INIT_EXCEPT_SERIES_E_4))
      || ((INIT_EXCEPT_SERIES_S_5 <= index)
        && (index <= INIT_EXCEPT_SERIES_E_5))
      || (( ((INIT_EXCEPT_SERIES_S_2 <= index)
            && (index <= INIT_EXCEPT_SERIES_E_2))
         || ((INIT_EXCEPT_SERIES_S_3 <= index)
            && (index <= INIT_EXCEPT_SERIES_E_3))
         || ((INIT_EXCEPT_SERIES_S_6 <= index)
            && (index <= INIT_EXCEPT_SERIES_E_6))
         || ((INIT_EXCEPT_SERIES_S_7 <= index)
            && (index <= INIT_EXCEPT_SERIES_E_7))
        )
        && (!type))
        )
    {
        init = funcCode.all[index];
    }

    else if ((INIT_EXCEPT_SINGLE_0 == index)
            || (INIT_EXCEPT_SINGLE_1 == index)
            || (INIT_EXCEPT_SINGLE_2 == index)
            || (INIT_EXCEPT_SINGLE_3 == index)
            || (INIT_EXCEPT_SINGLE_4 == index)
            || ((INIT_EXCEPT_SINGLE_5 == index)
            	&& (!type)
			   )
            || (INIT_EXCEPT_SINGLE_6 == index)
            || (INIT_EXCEPT_SINGLE_7 == index)
            || (INIT_EXCEPT_SINGLE_8 == index)
            || (INIT_EXCEPT_SINGLE_9 == index)
            || (INIT_EXCEPT_SINGLE_10 == index)
            || (INIT_EXCEPT_SINGLE_11 == index)
        )
    {
        init = funcCode.all[index];
    }

    else if (((GetCodeIndex(funcCode.code.aiCalibrateCurve[0].before1) <= index))
            && (index <= GetCodeIndex(funcCode.code.aoCalibrateCurve[AO_NUMBER-1].after2))
            )
    {

        if (((GetCodeIndex(funcCode.code.aiCalibrateCurve[2].before1) <= index))
            && (index <= GetCodeIndex(funcCode.code.aiCalibrateCurve[2].after2))
            )
        {
            init = funcCode.all[index];
        }

        else if (((GetCodeIndex(funcCode.code.aoCalibrateCurve[1].before1) <= index))
            && (index <= GetCodeIndex(funcCode.code.aoCalibrateCurve[1].after2))
            )
        {
            init = funcCode.all[index];
        }
        else
        {
            init = funcCode.group.ae[index - GetCodeIndex(funcCode.code.aiCalibrateCurve[0].before1)];
        }
    }

    else if ((ACC_TIME1_INDEX == index) ||
        (DEC_TIME1_INDEX == index) ||
        (ACC_TIME2_INDEX == index) ||
        (DEC_TIME2_INDEX == index) ||
        (ACC_TIME3_INDEX == index) ||
        (DEC_TIME3_INDEX == index) ||
        (ACC_TIME4_INDEX == index) ||
        (DEC_TIME4_INDEX == index)
        )
    {
        if (invPara.type < invPara.bitAccDecStart)
        {
            init = ACC_DEC_T_INIT1;
        }
        else
        {
            init = ACC_DEC_T_INIT2;
        }
    }
#if (DEBUG_F_INV_TYPE_RELATE || DEBUG_F_MOTOR_POWER_RELATE)

    else if (CARRIER_FRQ_INDEX == index)
    {
        init = GetInvParaPointer(invPara.type)->elem.carrierFrq;
    }

    else if ((ANTI_VIBRATE_GAIN_INDEX == index)
            || (ANTI_VIBRATE_GAIN_MOTOR2_INDEX == index)
            || (ANTI_VIBRATE_GAIN_MOTOR3_INDEX == index)
            || (ANTI_VIBRATE_GAIN_MOTOR4_INDEX == index)
            )
    {
        init = GetInvParaPointer(invPara.type)->elem.antiVibrateGain;
    }

    else if (TORQUE_BOOST_INDEX == index)
    {
        init = TorqueBoostDeal(funcCode.code.motorParaM1.elem.ratingPower);
    }
    else if (TORQUE_BOOST_MOTOR2_INDEX == index)
    {
        init = TorqueBoostDeal(funcCode.code.motorFcM2.motorPara.elem.ratingPower);
    }
    else if (TORQUE_BOOST_MOTOR3_INDEX == index)
    {
        init = TorqueBoostDeal(funcCode.code.motorFcM3.motorPara.elem.ratingPower);
    }
    else if (TORQUE_BOOST_MOTOR4_INDEX == index)
    {
        init = TorqueBoostDeal(funcCode.code.motorFcM4.motorPara.elem.ratingPower);
    }
    else if (SVC_MODE_INDX == index)
    {

        if (invPara.ratingVoltage == 1140)
        {
            init = 0;
        }
        else
        {
            init = 1;
        }
    }

    else if (OV_POINT_SET_INDEX == index)
    {
        init = ovVoltageInitValue[invPara.volLevel];
    }
#endif
    else
    {
        init = GetFuncCodeInitOriginal(index);
    }

    return init;
#endif
}

void ClearRecordDeal(void)
{
#if DEBUG_F_CLEAR_RECORD
    int16 i;

    funcCodeRwModeTmp = FUNCCODE_RW_MODE_WRITE_SERIES;
    startIndexWriteSeries = CLEAR_RECORD_SERIES_S_0;
    endIndexWriteSeries = CLEAR_RECORD_SERIES_E_0;
    for (i = startIndexWriteSeries; i <= endIndexWriteSeries; i++)
    {
        funcCode.all[i] = 0;
    }

    funcCode.all[CLEAR_RECORD_SINGLE_0] = 0;
    SaveOneFuncCode(CLEAR_RECORD_SINGLE_0);

    funcCode.all[CLEAR_RECORD_SINGLE_1] = 0;
    SaveOneFuncCode(CLEAR_RECORD_SINGLE_1);

    funcCode.all[CLEAR_RECORD_SINGLE_2] = 0;
    SaveOneFuncCode(CLEAR_RECORD_SINGLE_2);

    funcCode.all[CLEAR_RECORD_SINGLE_3] = 0;
    SaveOneFuncCode(CLEAR_RECORD_SINGLE_3);

    funcCode.all[CLEAR_RECORD_SINGLE_4] = 0;
    SaveOneFuncCode(CLEAR_RECORD_SINGLE_4);

    funcCode.all[CLEAR_RECORD_SINGLE_5] = 0x222;
    SaveOneFuncCode(CLEAR_RECORD_SINGLE_5);
#endif
}

void RestoreCompanyParaOtherDeal(void)
{
#if DEBUG_F_RESTORE_COMPANY_PARA_DEAL

    funcCode.code.motorDebugFc = 0;
	funcCode.code.aiaoCalibrateDisp = 0;
    SaveOneFuncCode(GetCodeIndex(funcCode.code.motorDebugFc));
    SaveOneFuncCode(GetCodeIndex(funcCode.code.aiaoCalibrateDisp));
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
    funcCode.code.plcTimeHighRem = (plcTime & 0xFFFF0000UL) >> 16;
    funcCode.code.plcTimeLowRem = plcTime & 0x0000FFFFUL;

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
    endIndexWriteSeries = GetCodeIndex(funcCode.group.remember[REM_NUM-1]);

#endif
}

LOCALF void LimitOtherCodeDeal(Uint16 index)
{
#if DEBUG_F_LIMIT_OTHER_CODE
    int16 i, j, k;

    for (j = LIMIT_OTHER_CODE_NUM - 1; j >= 0; j--)
    {
        if (limitOtherCodeIndex[j] == index)
        {
            for (k = 2 - 1; k >= 0; k--)
            {
                i = (int16)limitedByOtherCodeIndexNum - 1;
                for (; i >= 0; i--)
                {
                    const FUNCCODE_ATTRIBUTE *pAttribute = &funcCodeAttribute[limitedByOtherCodeIndex[i]];
                    Uint16 *pCode = &funcCode.all[limitedByOtherCodeIndex[i]];
                    Uint16 tmp = 0;
                    Uint16 upper;
                    Uint16 lower;

                    if (pAttribute->attribute.bit.upperLimit)
                    {
                        int32 fc1, upper1;

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
                            SaveOneFuncCode(limitedByOtherCodeIndex[i]);

                            tmp = 1;
                        }
                    }

                    if (pAttribute->attribute.bit.lowerLimit)
                    {
                        int32 fc1, lower1;

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
                            if (!tmp)
                            {
                                *pCode = lower;
                                SaveOneFuncCode(limitedByOtherCodeIndex[i]);
                            }
                            else
                            {
                                funcCode.all[pAttribute->lower] = *pCode;
                                SaveOneFuncCode(pAttribute->lower);
                            }
                        }
                    }
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
