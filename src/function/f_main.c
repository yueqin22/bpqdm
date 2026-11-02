
#include "f_main.h"
#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_eeprom.h"
#include "f_io.h"
#include "f_menu.h"
#include "f_ui.h"
#include "f_comm.h"
#include "f_posCtrl.h"
#include "f_osc.h"
#include "f_canlink.h"
#include "f_p2p.h"

#define PRODUCT_VERSION      38000
#define SOFTWARE_VERSION     72

#if F_DEBUG_RAM

#define DEBUG_F_FC_INIT_OVER        0

#define DEBUG_F_CHK_FUNC_CODE       0

#elif 1

#define DEBUG_F_FC_INIT_OVER        1

#define DEBUG_F_CHK_FUNC_CODE       0

#endif

#define AIAO_CHK            (0x5A5A)

Uint16 memoryValue;
void getMomeryValue(void);
#define MEMORY_ADDRESS funcCode.code.memoryAddr

#if 0
Uint16 dataDigit;
Uint16 kk;
Uint16 jj;
Uint16 upper1;
Uint16 lower1;
Uint16 index;
#endif

#define START_DSP_DELAY_TIME_MS     300

#define WAIT_POWER_ON_TIME          10000
enum POWER_ON_STATUS powerOnStatus;

enum FUNC_CODE_INIT_STATUS
{
    FUNCCODE_INIT_STATUS_VERIFY_EEPROM,
    FUNCCODE_INIT_STATUS_NEW_EEPROM,
    FUNCCODE_INIT_STATUS_OLD_EEPROM,
    FUNCCODE_INIT_STATUS_OVER
};
LOCALF enum FUNC_CODE_INIT_STATUS funcCodeInitStatus;

Uint16 driveCoeff[2][4];

extern Uint32 allTime;
extern Uint32 upTimes;

union MOTOR_PARA motorPara;
struct MOTOR_FC motorFc;
enum MOTOR_SN motorSn;
extern void UpdateMotorPara(void);

Uint16 aiaoChckReadOK;

Uint16 mainLoopTicker;

extern void InitSetFvcIndexIntEQep(void);

LOCALD void FuncVariableInit(void);

LOCALD void InitFuncCode(void);
void InitDspFunc(void);

extern void UpdateU0Data(void);

extern void SoftVersionDeal(void);

extern void UpdateDataCore2Func(void);
extern void UpdateDataFunc2Core0p5ms(void);
extern void UpdateDataFunc2Core2ms(void);
extern void TemperatureDeal(void);

Uint32 timeBase;
Uint32 timeBase05msOld;
Uint32 ticker05msFuncInternal;
Uint32 ticker05msFuncIn[4];
Uint16 ticker05msFuncInternalDisp[4];

Uint32 ticker05msFuncEach;
Uint32 ticker05msFuncEa[4];
Uint16 ticker05msFuncEachDisp[4];
void CodeRunMonitor(void);

void Main05msFunctionEachStart(void);
void Main05msFunctionA(void);
void Main05msFunctionB(void);
void Main05msFunctionC(void);
void Main05msFunctionD(void);
void Main05msFunctionEachEnd(void);

void InitForFunctionApp(void)
{
#if DEBUG_F_CHK_FUNC_CODE
    extern void ValidateFuncCode(void);

    ValidateFuncCode();
#endif

    InitDspFunc();

    InitFuncCode();

    FuncVariableInit();
}

extern void I2cDealBeforeInit(void);
void InitDspFunc(void)
{
    EALLOW;
#if DSP_2803X
    SysCtrlRegs.PCLKCR0.bit.ECANAENCLK = 1;
    SysCtrlRegs.PCLKCR1.bit.EPWM4ENCLK = 1;
    SysCtrlRegs.PCLKCR1.bit.EPWM5ENCLK = 1;
    SysCtrlRegs.PCLKCR1.bit.EPWM6ENCLK = 1;
    SysCtrlRegs.PCLKCR1.bit.ECAP1ENCLK = 1;
    SysCtrlRegs.PCLKCR0.bit.LINAENCLK = 1;
#else
    SysCtrlRegs.PCLKCR1.bit.ECAP3ENCLK = 1;
    SysCtrlRegs.PCLKCR1.bit.ECAP4ENCLK = 1;
    SysCtrlRegs.PCLKCR1.bit.EQEP2ENCLK = 1;
    SysCtrlRegs.PCLKCR0.bit.ECANAENCLK = 1;
#endif

    EDIS;

    InitSpiaGpio();
    InitSetSpiForFunc();

    I2cDealBeforeInit();
    InitI2CGpio();
    InitSetI2ca();

#if 1
    InitSciaGpio();
    InitSetScia();
#elif 1
    InitScibGpioDp();
    InitSetSciDp(&sciM380DpData);
#endif

#if DSP_2803X
    InitEPwm6Gpio();
    InitSetEPWM6();
#else
    InitECap3Gpio();
    InitSetEcap3();
#endif

#if DSP_2803X

    InitDIGpio();
    InitDOGpio();
#endif

#if DSP_2803X
    InitECap1Gpio();
    InitSetEcap1();
#else
    InitECap4Gpio();
    InitSetEcap4();
#endif

#if DSP_2803X
    InitEPwm4Gpio();
    InitSetEPWM4();
    InitEPwm5Gpio();
    InitSetEPWM5();
#else
    InitECap2Gpio();
    InitSetEcap2();
#endif

}

LOCALF void InitFuncCode(void)
{
    Uint32 baseTime;
    Uint16 readAIAOChck = AIAO_CHK_CHECK;
    Uint16 fcIndex;
    Uint16 readIndex;
    aiaoChckReadOK = AIAO_CHK_READ_NULL;

    baseTime = GetTime();
    while ((errorEeprom == 0)
           && ((FUNCCODE_INIT_STATUS_OVER != funcCodeInitStatus)

               )
           )
    {
        switch (funcCodeInitStatus)
        {
            case FUNCCODE_INIT_STATUS_VERIFY_EEPROM:
                funcCodeRead.index = GetEepromIndexFromFcIndex(EEPROM_CHECK_INDEX1);
                funcCodeRead.number = 2;
                EepromDeal();
                if (FUNCCODE_READ_RET_OK == ReadFuncCode(&funcCodeRead))
                {
                    if ((EEPROM_CHECK1 == funcCodeRead.data[0])
                        && (EEPROM_CHECK2 == funcCodeRead.data[1]))
                    {
                        funcCodeInitStatus = FUNCCODE_INIT_STATUS_OLD_EEPROM;
                    }
                    else
                    {
                        funcCodeInitStatus = FUNCCODE_INIT_STATUS_NEW_EEPROM;
                        readAIAOChck = AIAO_CHK_CHECK;
                    }
                }
                break;

            case FUNCCODE_INIT_STATUS_NEW_EEPROM:

                if (readAIAOChck == AIAO_CHK_CHECK)
                {

                    readIndex = GetEepromIndexFromFcIndex(AI_AO_CHK_FLAG);
                    funcCodeRead.index = readIndex;
                    funcCodeRead.number = 1;
                    EepromDeal();
                    if (FUNCCODE_READ_RET_OK == ReadFuncCode(&funcCodeRead))
                    {

                        if (funcCodeRead.data[0] == AIAO_CHK_WORD)
                        {

                            readAIAOChck = AIAO_CHK_START;
                            readIndex = GetEepromIndexFromFcIndex(AI_AO_CALIB_START);
                        }
                        else
                        {

                            readAIAOChck = AIAO_CHK_END;
                        }
                    }
                }
                else if (readAIAOChck == AIAO_CHK_START)
                {
                    static Uint16 flag = 0;
                    funcCodeRead.index = readIndex;
                    funcCodeRead.number = 2;
                    EepromDeal();
                    if (FUNCCODE_READ_RET_OK == ReadFuncCode(&funcCodeRead))
                    {
                        fcIndex = GetFcIndexFromEepromIndex(readIndex);
                        funcCode.all[fcIndex] = funcCodeRead.data[0];
                        funcCode.all[fcIndex + 1] = funcCodeRead.data[1];

                        readIndex += 2;
                        flag++;

                        if (flag == 4)
                        {
                            readIndex += 4;
                        }

                        if (readIndex > GetEepromIndexFromFcIndex(AI_AO_CALIB_STOP))
                        {
                            aiaoChckReadOK = AIAO_CHK_READ_OK;
                            readAIAOChck = AIAO_CHK_END;
                        }
                    }
                }
                else
                {
                    funcCodeRwModeTmp = FUNCCODE_RW_MODE_WRITE_ALL;
                    EepromDeal();
                    if (funcCodeRwModeTmp == 0)
                    {
                        funcCodeInitStatus = FUNCCODE_INIT_STATUS_OVER;

                        funcCode.code.eepromCheckWord1 = EEPROM_CHECK1;
                        funcCode.code.eepromCheckWord2 = EEPROM_CHECK2;
                        funcCode.code.aiaoChkWord = FUNCCODE_READ_RET_OK;
                        SaveOneFuncCode(EEPROM_CHECK_INDEX1);
                        SaveOneFuncCode(EEPROM_CHECK_INDEX2);
                        SaveOneFuncCode(AI_AO_CHK_FLAG);
                    }
                }
                break;

            case FUNCCODE_INIT_STATUS_OLD_EEPROM:
                funcCodeRwModeTmp = FUNCCODE_RW_MODE_READ_ALL;
                EepromDeal();
                if (funcCodeRwModeTmp == 0)
                {
                    funcCodeInitStatus = FUNCCODE_INIT_STATUS_OVER;
                }
                break;

            default:
                break;
        }

        PulseInSample();

        if (baseTime - GetTime() >= (TIME_1MS << 1))
        {
            spiData.out.a.bit.dispCode = (displayBuffer[scanStep] & 0x00ff);
            DisplayScanPrepare();

            baseTime = GetTime();

            EepromOperateTimeDeal();

            UpdateDataCore2Func();
            DisplayScan();
        }
    }

    SoftVersionDeal();

#if DEBUG_F_FC_INIT_OVER

    if (FUNCCODE_INIT_STATUS_OVER == funcCodeInitStatus)
    {
        int16 i;
        Uint16 upper, lower;
        Uint16 tmp;
        int16 j;
        const FUNCCODE_ATTRIBUTE *maxFrqAttribute = &funcCodeAttribute[MAX_FRQ_INDEX];

        j = 0;
        for (i = FNUM_PARA - 1; i >= 0; i--)
        {
            const FUNCCODE_ATTRIBUTE *pAttribute = &funcCodeAttribute[i];

            if (pAttribute->attribute.bit.upperLimit != 0)
            {
                limitedByOtherCodeIndex[j++] = i;

                upper = funcCode.all[pAttribute->upper];
            }
            else
            {
                upper = pAttribute->upper;
            }

            if (pAttribute->attribute.bit.lowerLimit != 0)
            {

                if (pAttribute->attribute.bit.upperLimit == 0)
                {
                    limitedByOtherCodeIndex[j++] = i;
                }

                lower = funcCode.all[pAttribute->lower];
            }
            else
            {
                lower = pAttribute->lower;
            }

            tmp = LimitDeal(pAttribute->attribute.bit.signal, funcCode.all[i], upper, lower, 0);
            if (funcCode.all[i] != tmp)
            {
                funcCode.all[i] = tmp;
                SaveOneFuncCode(i);
            }

            DiCalc();
            PulseInCalc();
            AiCalc();
        }
        limitedByOtherCodeIndexNum = j;

        upper = maxFrqAttribute->upper;
        lower = 50 * decNumber[funcCode.code.frqPoint];
        tmp = LimitDeal(maxFrqAttribute->attribute.bit.signal, funcCode.all[MAX_FRQ_INDEX], upper, lower, 0);
        if (funcCode.all[MAX_FRQ_INDEX] != tmp)
        {
            funcCode.all[MAX_FRQ_INDEX] = tmp;
            SaveOneFuncCode(MAX_FRQ_INDEX);
        }

    }
    else
#endif
        if (errorEeprom != 0)
    {
        errorCode = ERROR_EEPROM;
        UpdateErrorDisplayBuffer();

        for (;;)
        {
            if (baseTime - GetTime() >= (TIME_1MS << 1))
            {
                spiData.out.a.bit.dispCode = (displayBuffer[scanStep] & 0x00ff);
                DisplayScanPrepare();

                baseTime = GetTime();
                PulseInCalc();
                AiCalc();

                DisplayScan();
            }
        }
    }
}

LOCALF void FuncVariableInit(void)
{
#if DEBUG_F_MOTOR_FUNCCODE
    int16 i;
#endif

    funcCode.code.inverterGpTypeDisp = funcCode.code.inverterGpType;

    UpdateInvType();

    frqCalcSrcOld = funcCode.code.frqCalcSrc;

    if ((funcCode.code.frqXSrc == FUNCCODE_frqXySrc_FC_P_OFF_REM) ||
        (funcCode.code.frqYSrc == FUNCCODE_frqXySrc_FC_P_OFF_REM))
    {
        upDownFrqInit = (int16)funcCode.code.upDownFrqRem;
    }
    else
    {
        upDownFrqInit = 0;
    }

    pmsmRotorPos = funcCode.code.pmsmRotorPos;

    frqAimTmp0 = funcCode.code.presetFrq;

    if (FUNCCODE_plcPowerOffRemMode_REM == (funcCode.code.plcPowerOffRemMode % 10))
    {
        plcStep = funcCode.code.plcStepRem;
        plcStepRemOld = plcStep;

        plcTime = ((Uint32)funcCode.code.plcTimeHighRem << 16) + funcCode.code.plcTimeLowRem;
        plcTimeRemOld = plcTime;
    }

#if DEBUG_F_POSITION_CTRL
    InitPosCtrl();
#endif

#if 0
    displayBuffer[5] = LED_CODE[LED_NULL];

    spiData.out.a.bit.dispCode = (displayBuffer[scanStep] & 0x00ff);

#elif 0
    displayBuffer[0] = DISPLAY_CODE[DISPLAY_NULL];
    displayBuffer[1] = DISPLAY_CODE[DISPLAY_NULL];
    displayBuffer[2] = DISPLAY_CODE[DISPLAY_NULL];
    displayBuffer[3] = DISPLAY_CODE[DISPLAY_NULL];
    displayBuffer[4] = DISPLAY_CODE[DISPLAY_NULL];
    displayBuffer[5] = LED_CODE[LED_NULL];
    scanStep = 5;
    spiData.out.a.bit.dispCode = (displayBuffer[scanStep] & 0x00ff);
#elif 1
    scanStep = 5;
#endif

    dspMainCmd1.bit.shortGnd = funcCode.code.shortCheckMode;
    dspMainCmd1.bit.pgLocation = motorFc.pgPara.elem.fvcPgSrc;

#if DEBUG_F_MOTOR_FUNCCODE
#if !F_DEBUG_RAM

    for (i = REM_P_OFF_MOTOR - 1; i >= 0; i--)
    {
        gSendToFunctionDataBuff[i + MOTOR_TO_Func_2MS_DATA_NUM] = funcCode.code.remPOffMotorCtrl[i];
    }
#endif
#endif

    UpdateDataFunc2Core0p5ms();
    UpdateDataFunc2Core2ms();

#if 0

    for (i = 0; i < FNUM_PARA; i++)
    {
        kk = 0;
        index = i;
#if 0
        upper = funcCodeAttribute[index].upper;
        if (funcCodeAttribute[index].attribute.bit.upperLimit)
        {
            index = upper;
            upper = funcCodeAttribute[index].upper;
        }
#elif 1
        while (funcCodeAttribute[index].attribute.bit.upperLimit)
        {
            index = funcCodeAttribute[index].upper;
            if (++kk >= 5)
            {
                asm(" nop");
                break;
            }
        }
        upper1 = funcCodeAttribute[index].upper;
        asm(" nop");
#endif

        if (upper1 >= 10000)
            dataDigit = 5;
        else if (upper1 >= 1000)
            dataDigit = 4;
        else if (upper1 >= 100)
            dataDigit = 3;
        else if (upper1 >= 10)
            dataDigit = 2;
        else
            dataDigit = 1;

        if (funcCodeAttribute[index].attribute.bit.point >= dataDigit)
            dataDigit = funcCodeAttribute[index].attribute.bit.point + 1;

        if (dataDigit != funcCodeAttribute[index].attribute.bit.displayBits)
        {
            jj++;
            asm(" nop");
        }
    }
    asm(" nop");
#endif

    MenuInit();
}

void Main2msFunction(void)
{

}

void Main0msFunction(void)
{

    PulseInSample();

#if DEBUG_F_POSITION_CTRL
    pulseInSamplePcQep();
#endif
}

#if 0

Uint32 allTime;
Uint32 upTimes;
Uint32 maxTime;
Uint32 minTime;
void Main05msFunction(void)
{
    static Uint16 ticker;
    Uint32 timeBase;

    timeBase = GetTime();
    ticker05msFuncInternal = timeBase05msOld - timeBase;
    timeBase05msOld = timeBase;

    if (0 == ticker)
    {
        Main05msFunctionA();
    }
    else if (1 == ticker)
    {
        Main05msFunctionB();
    }
    else if (2 == ticker)
    {
        Main05msFunctionC();
    }
    else if (3 == ticker)
    {
        Main05msFunctionD();
    }
    Main05msFunctionEachEnd();

    ticker05msFuncEach = timeBase - GetTime();

    if (3 == ticker)
    {
        allTime = 0;
        allTime += ticker05msFuncEa[0];
        allTime += ticker05msFuncEa[1];
        allTime += ticker05msFuncEa[2];
        allTime += ticker05msFuncEa[3];

        if (maxTime < allTime)
        {
            maxTime = allTime;
        }

        if (minTime > allTime)
        {
            minTime = allTime;
        }
    }

    ticker05msFuncEa[ticker] = ticker05msFuncEach;
    ticker05msFuncIn[ticker] = ticker05msFuncInternal;

    ticker++;
    if (ticker >= 4)
    {
        ticker = 0;
    }
}

#endif

void Main05msFunctionEachStart(void)
{
    static Uint16 waitPowerOnTicker;

    timeBase = GetTime();
    ticker05msFuncInternal = timeBase05msOld - timeBase;
    timeBase05msOld = timeBase;

    if ((dspStatus.bit.runEnable != 0)
        && (dspStatus.bit.uv != 0)
        )
    {
        powerOnStatus = POWER_ON_CORE_OK;
    }

    if (++waitPowerOnTicker >= WAIT_POWER_ON_TIME)
    {
        if (POWER_ON_WAIT == powerOnStatus)
        {
            powerOnStatus = POWER_ON_FUNC_WAIT_OT;
        }
    }

    commTicker++;

    canLinkTicker++;

    EepromDeal();
}

void Main05msFunctionEachEnd(void)
{
    static Uint16 ticker = 0;

    ticker05msFuncEach = timeBase - GetTime();
    ticker05msFuncEa[ticker] = ticker05msFuncEach * 3413 >> 11;
    ticker05msFuncIn[ticker] = ticker05msFuncInternal * 3413 >> 11;

    if (++ticker >= 4)
    {
        ticker = 0;
    }
}

extern void vfSeparateDeal(void);
void Main05msFunctionA(void)
{
    asm(" nop");

    Main05msFunctionEachStart();

    DisplayScanPrepare();

    UpdateDataCore2Func();

    DiCalc();
    PulseInCalc();

    Main05msFunctionEachEnd();
}

void Main05msFunctionB(void)
{
    Main05msFunctionEachStart();

    AiCalc();

    UpdateMotorPara();

#if DEBUG_F_POSITION_CTRL
    pulseInCalcPcEQep();
    PcMiscDeal();
#endif

    getMomeryValue();

    DisplayScan();

    KeyScan();
    KeyProcess();
    P2PDataDeal();
#if DSP_2803X
    CanlinkFun();
    #if 0
    if ( (funcCode.code.commProtocolSec < CANLINK) && (funcCode.code.plcEnable != 1) )
    #endif
    {
        SciDeal();
    }
    OscSciFunction();
#else

    CanlinkFun();
    if(funcCode.code.commMaster)
    {
        OscSciFunction();
    }
    else
    {

        if ( (funcCode.code.commProtocolSec < CANLINK) && (funcCode.code.plcEnable != 1) )
        {
            SciDeal();
        }
    }
#endif

    FrqSrcDeal();

    vfSeparateDeal();

    Main05msFunctionEachEnd();

}

void Main05msFunctionC(void)
{
    asm(" nop");

    Main05msFunctionEachStart();

    AccDecTimeCalc();

    runTimeCal();

    setTimeRun();

    RunSrcDeal();

    TorqueCalc();

    UpdateDataFunc2Core0p5ms();

    EepromOperateTimeDeal();

    ErrorDeal();

    AoCalcChannel(AOFMP_AO1);
#if DSP_2803X
    AoCalcChannel(AOFMP_AO2);
#endif
    FMPDeal();

    Main05msFunctionEachEnd();

}

void Main05msFunctionD(void)
{
    Main05msFunctionEachStart();

    DoCalc();

    TemperatureDeal();

    UpdateU0Data();

    UpdateDisplayBuffer();

    WinkDeal();

    ErrorReset();

    UpdateDataFunc2Core2ms();

    CodeRunMonitor();

    if (0 == mainLoopTicker)
    {
        mainLoopTicker = 1;
    }

    Main05msFunctionEachEnd();

}

#define CODE_MAX_RUN_DISPLAY_UPDATE_TIME    5000
Uint16 CODE_RUN_DISPLAY_UPDATE_TIME = 200;

void CodeRunMonitor(void)
{
    static Uint16 ticker;

    if (++ticker >= CODE_RUN_DISPLAY_UPDATE_TIME / FUNC_DEAL_PERIOD)
    {
        ticker = 0;

        CODE_RUN_DISPLAY_UPDATE_TIME += 2;
        if (CODE_RUN_DISPLAY_UPDATE_TIME >= 300)
        {
            CODE_RUN_DISPLAY_UPDATE_TIME = 200;
        }

        ticker05msFuncEachDisp[0] = ticker05msFuncEa[0] * 3276 >> 15;
        ticker05msFuncEachDisp[1] = ticker05msFuncEa[1] * 3276 >> 15;
        ticker05msFuncEachDisp[2] = ticker05msFuncEa[2] * 3276 >> 15;
        ticker05msFuncEachDisp[3] = ticker05msFuncEa[3] * 3276 >> 15;

        ticker05msFuncInternalDisp[0] = ticker05msFuncIn[0] * 3276 >> 15;
        ticker05msFuncInternalDisp[1] = ticker05msFuncIn[1] * 3276 >> 15;
        ticker05msFuncInternalDisp[2] = ticker05msFuncIn[2] * 3276 >> 15;
        ticker05msFuncInternalDisp[3] = ticker05msFuncIn[3] * 3276 >> 15;

    }
}

Uint16 GetNumberDigit(Uint16 digit[5], Uint16 number, Uint16 mode)
{
    if (mode != 0)
    {
        GetNumberDigit2(digit, number);
    }
    else
    {
        GetNumberDigit1(digit, number);
    }

    if (digit[4] != 0)
        return 5;
    else if (digit[3] != 0)
        return 4;
    else if (digit[2] != 0)
        return 3;
    else if (digit[1] != 0)
        return 2;
    else
        return 1;
}

void GetNumberDigit1(Uint16 digit[5], Uint16 number)
{
    Uint16 tmp;

    tmp = (Uint32)number * 52429 >> 19;
    digit[0] = number - tmp * 10;
    number = tmp;

    tmp = (Uint32)number * 52429 >> 19;
    digit[1] = number - tmp * 10;
    number = tmp;

    tmp = (Uint32)number * 52429 >> 19;
    digit[2] = number - tmp * 10;
    number = tmp;

    tmp = (Uint32)number * 52429 >> 19;
    digit[3] = number - tmp * 10;
    number = tmp;

    tmp = (Uint32)number * 52429 >> 19;
    digit[4] = number - tmp * 10;
    number = tmp;
}

void GetNumberDigit2(Uint16 digit[5], Uint16 number)
{
    Uint16 tmp;

    tmp = number >> 4;
    digit[0] = number - (tmp << 4);
    number = tmp;

    tmp = number >> 4;
    digit[1] = number - (tmp << 4);
    number = tmp;

    tmp = number >> 4;
    digit[2] = number - (tmp << 4);
    number = tmp;

    tmp = number >> 4;
    digit[3] = number - (tmp << 4);
    number = tmp;

    digit[4] = 0;
}

void SoftVersionDeal(void)
{
    Uint16 m_funcSoftVersion;
    Uint16 m_motorSoftVersion;

    funcCode.code.productVersion = PRODUCT_VERSION;
    funcCode.code.funcSoftVersion = SOFTWARE_VERSION;

    m_funcSoftVersion = funcCode.code.funcSoftVersion;
    m_motorSoftVersion = funcCode.code.motorSoftVersion;

    if (m_funcSoftVersion < m_motorSoftVersion)
    {
        funcCode.code.softVersion = m_motorSoftVersion;
    }
    else
    {
        funcCode.code.softVersion = m_funcSoftVersion;
    }
}

#if DEBUG_F_CHK_FUNC_CODE

Uint16 sizeOfGroup;
Uint16 sizeOfGrade;
Uint16 sizeOfAll;
Uint16 sizeOfFuncCodeAttribute;
Uint16 sizeOfEeprom;
Uint16 sizeOfPara;
Uint16 errorFuncCode;
void ValidateFuncCode(void);

Uint16 eeprom2FcAutoGenerate[EEPROM_INDEX_USE_LENGTH];
extern const Uint16 fcNoAttri2Eeprom[];
Uint16 sizeOfTable_eeprom2Fc = EEPROM_INDEX_USE_LENGTH;
Uint16 maxEepromAddr;
Uint16 append;
void ValidateEepromAddr(void);
void GetTableEeprom2Fc(void);
void CheckEeprom2Fc(void);

void ValidateFuncCode(void)
{

    sizeOfGroup = sizeof(struct FUNCCODE_GROUP);
    sizeOfGrade = sizeof(struct FUNCCODE_CODE);
    if (sizeOfGroup != sizeOfGrade)
    {
        asm(" ESTOP0");
    }

    sizeOfAll = sizeof(FUNCCODE_ALL);
    if (sizeOfAll != sizeOfGrade)
    {
        asm(" ESTOP0");
    }

    sizeOfPara = FNUM_PARA;
    sizeOfEeprom = FNUM_EEPROM;
    if (funcCodeAttribute[FNUM_PARA - 1].upper == 0)
    {
        asm(" ESTOP0");
    }
    if (sizeOfFuncCodeAttribute != sizeOfPara)
    {
        asm(" nop");

    }

#if 0

    if (sizeof(dispAttributeU0) != U0NUM)
    {
        asm(" nop");
        asm(" ESTOP0");
    }

    if (sizeof(dispAttributeU2) != U2NUM)
    {
        asm(" nop");
        asm(" ESTOP0");
    }

    if (sizeof(dispAttributeU3) != U3NUM)
    {
        asm(" nop");
        asm(" ESTOP0");
    }
#endif

    append = CHK_NUM + REM_NUM;

    ValidateEepromAddr();

    GetTableEeprom2Fc();

    CheckEeprom2Fc();

    errorCode = errorFuncCode;
    if (errorFuncCode != 0)
    {
        for (;;)
            ;
    }

    for (;;)
    {
        asm(" nop");
        asm(" ESTOP0");
    }
}

void ValidateEepromAddr(void)
{
    int16 i;
    int16 j;

    for (i = 0; i < FNUM_PARA; i++)
    {

        for (j = 0; j < FNUM_PARA; j++)
        {
            if ((i != j)
                && (funcCodeAttribute[i].eepromIndex == funcCodeAttribute[j].eepromIndex)
                )
            {
                asm(" ESTOP0");
            }
        }

        for (j = 0; j < append; j++)
        {
            if (funcCodeAttribute[i].eepromIndex == fcNoAttri2Eeprom[j])
            {
                asm(" ESTOP0");
            }
        }
    }

    for (i = 0; i < append; i++)
    {
        for (j = 0; j < append; j++)
        {
            if ((i != j)
                && (fcNoAttri2Eeprom[i] == fcNoAttri2Eeprom[j])
                )
            {
                asm(" ESTOP0");
            }
        }
    }

    maxEepromAddr = 0;
    for (i = 0; i < append; i++)
    {
        if (maxEepromAddr < fcNoAttri2Eeprom[i])
            maxEepromAddr = fcNoAttri2Eeprom[i];
    }
    for (i = 0; i < FNUM_PARA; i++)
    {
        if (maxEepromAddr < funcCodeAttribute[i].eepromIndex)
            maxEepromAddr = funcCodeAttribute[i].eepromIndex;
    }

    if (maxEepromAddr >= EEPROM_INDEX_USE_LENGTH)
    {
        asm(" ESTOP0");
    }
}

#define EEPROM2FC_NONE      FUNCCODE_RSVD4ALL_INDEX

void GetTableEeprom2Fc(void)
{
    int16 i;
    int16 j;

    for (i = 0; i <= EEPROM_INDEX_USE_LENGTH; i++)
    {
        for (j = 0; j < FNUM_PARA; j++)
        {
            if (i == funcCodeAttribute[j].eepromIndex)
            {
                eeprom2FcAutoGenerate[i] = j;
                break;
            }
        }

        if (j == FNUM_PARA)
        {
            for (j = 0; j < append; j++)
            {
                if (i == fcNoAttri2Eeprom[j])
                {
                    eeprom2FcAutoGenerate[i] = j + FNUM_PARA;
                    break;
                }
            }

            if (j == append)
            {
                eeprom2FcAutoGenerate[i] = EEPROM2FC_NONE;

            }
        }
    }
}

void CheckEeprom2Fc(void)
{
    int16 i;

    for (i = 0; i < FNUM_PARA; i++)
    {
        if (eeprom2FcAutoGenerate[funcCodeAttribute[i].eepromIndex] != i)
        {
            asm(" ESTOP0");
        }
    }

    for (i = 0; i < append; i++)
    {
        if (eeprom2FcAutoGenerate[fcNoAttri2Eeprom[i]] != i + GetCodeIndex(funcCode.group.fChk[0]))
        {
            asm(" ESTOP0");
        }
    }
}

#endif

void getMomeryValue(void)
{
    Uint16 *a = (Uint16*)MEMORY_ADDRESS;

    memoryValue = *a;
}
