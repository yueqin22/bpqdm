
#include "f_ui.h"
#include "f_menu.h"
#include "f_frqSrc.h"
#include "f_comm.h"
#include "f_runSrc.h"
#include "f_io.h"
#include "f_error.h"

Uint16 flashMaxSDSD = 10;

#if F_DEBUG_RAM

#define DEBUG_F_KEY_ANIMATE                     0
#define DEBUG_F_NEWMENU_USE_DOT_REVERSE_DIR     0

#define DEBUG_F_ERROR_INDICATION                1
#elif 1

#define DEBUG_F_KEY_ANIMATE                     0
#define DEBUG_F_NEWMENU_USE_DOT_REVERSE_DIR     0

#define DEBUG_F_ERROR_INDICATION                1
#endif

#if DSP_2803X
#define KEY_SEL_TEST_TIME  750
Uint16 keySelStatus;
Uint16 keySelType;
Uint16 keySelTypeBak;
Uint16 keyChecking;
#endif

struct SPI_IN_OUT spiData;

const Uint16 dispCodeMenuMode[6][4] =
{
    {DISPLAY_F,     DISPLAY_u,      DISPLAY_n,      DISPLAY_C},
    {DISPLAY_U,     DISPLAY_5,      DISPLAY_E,      DISPLAY_t},
    {DISPLAY_U,     DISPLAY_LINE,   DISPLAY_LINE,   DISPLAY_C},
};

Uint16 onQuickTicker;
Uint16 onQuickTickerMax = 1000;

Uint16 onShiftPassViewTicker;
Uint16 onShiftPassViewTickerMax = 1000;

Uint16 factoryPwd = 0;

typedef struct
{
    Uint16 ctrlCnt;
    Uint16 ctrlVal;
    Uint16 funcCnt;
    Uint16 funcVal;
} keyRet;

const Uint16 DISPLAY_CODE[DISPLAY_8LED_CODE_NUM]=
{   0xc0,   0xf9,   0xa4,   0xb0,   0x99,

    0x92,   0x82,   0xf8,   0x80,   0x90,

    0x88,   0x83,   0xc6,   0xa1,   0x86,

    0x8e,   0x8c,   0x89,   0xf1,   0xc7,

    0xAB,   0xaf,   0xc1,   0x91,   0xc2,

    0x8b,   0xcf,   0xc8,   0xa3,   0x98,

    0xce,   0x87,   0xe3,   0xff,   0xbf,

    0x7f,   0x00,   0xa7

};

const Uint16 LED_CODE[DISPLAY_LED_CODE_NUM] =
{0xBF,   0xDF,  0xEF, 0xF7, 0xFB, 0xFD, 0xFE, 0xFC, 0xF9, 0x80, 0xFF};

Uint16 displayBuffer[DISPLAY_8LED_NUM + 1] = {0xbf, 0x89, 0xBF, 0xC6, 0xbf, 0x80};

Uint16 scanStep;

Uint16 keyBordTestFlag;
Uint16 keyBordValue;

LOCALF Uint16 keyPreFunc;
Uint16 keyPreCtrl;
Uint16 keyFunc;

LOCALF Uint16 keyAge;
LOCALF Uint16 bKeyEsc;
LOCALF Uint16 bKeyProc;

enum KEY_STATUS
{
    KEY_STATUS_JITTER_DEAL,
    KEY_STATUS_WAIT_REPEAT,
    KEY_STATUS_REPEAT,
    KEY_STATUS_WAIT_CARRY
};
LOCALF enum KEY_STATUS keyStatus;

union DI_HW_STATUS diHwStatus;
LOCALF Uint16 keyValue;

enum EN_FLASH
{
    FLASH_YES,
    FLASH_NO
};
LOCALF enum EN_FLASH bEnflash;
LOCALF Uint16 flashTicker[DISPLAY_8LED_NUM];
LOCALF Uint16 bLight[DISPLAY_8LED_NUM];
LOCALF Uint16 flashTickerLed[DISPLAY_LED_NUM];

#if 0
LOCALF const Uint16 flashTickerMax[DISPLAY_8LED_NUM] = {40, 40, 40, 40, 40};
LOCALF const Uint16 flashTickerLedMax[DISPLAY_LED_NUM] = {45, 45, 45, 45, 45};
#elif 1
#define flashTickerMax      42
#define flashTickerLedMax   45
#endif

#define keyAgeMax   20

#define maxRate     100
#define continueKeyFlag ((1 << KEY_UP) | (1 << KEY_DOWN))

#define ctrlKeyFlag     0

#define minRatePause    100

LOCALF const Uint16 minRate[5] = {50, 80, 130, 180, 200};
LOCALF const Uint16 repeatNumMax[5] = {9, 9, 9, 9, 9};
LOCALF Uint16 repeatNum;
LOCALF Uint16 repeatStep;
LOCALF Uint16 bOnShift;
Uint16 upDownDelta;

#if DEBUG_F_KEY_ANIMATE
Uint16 keyAnimateTime = 0;
Uint16 keyAnimateTime1 = 50;
Uint16 keyAnimateTimeAll = 500;
#endif

void UpdateMenuModeDisplayBuffer();

extern void Menu1OnPrg(void);
extern Uint16 checkMenuModePara;
extern Uint16 checkMenuModeCmd;
extern void DealCheckMenuModeGroupGrade(Uint16 flag);
void UpdateDisplayBuffer(void)
{

    displayBuffer[5] |= ~LED_CODE[LED_RUN];
    if (runFlag.bit.run)
    {
        displayBuffer[5] &= LED_CODE[LED_RUN];
    }

    menuAttri[menuLevel].winkFlagLed &= ~(0x1U << LED_LOCAL);
    if (FUNCCODE_runSrc_DI == runSrc)
    {
        displayBuffer[5] &= LED_CODE[LED_LOCAL];
    }
    else if (FUNCCODE_runSrc_COMM == runSrc)
    {
        menuAttri[menuLevel].winkFlagLed |= (0x1U << LED_LOCAL);
    }
    else
    {
        displayBuffer[5] |= ~LED_CODE[LED_LOCAL];
    }

    if (runFlag.bit.dirReversing)
    {
        menuAttri[menuLevel].winkFlagLed |= (0x1U << LED_DIR);
    }
    else
    {
        displayBuffer[5] |= ~LED_CODE[LED_DIR];

        if (REVERSE_DIR == runFlag.bit.dir)
        {
        displayBuffer[5] &= LED_CODE[LED_DIR];
        }
    }

    menuAttri[menuLevel].winkFlagLed &= ~(0x1U << LED_TUNE);
    if ((tuneCmd)
#if DEBUG_F_ERROR_INDICATION
        || errorCode
#endif
        )
    {
        menuAttri[menuLevel].winkFlagLed |= (0x1U << LED_TUNE);
    }
    else if (runMode == RUN_MODE_TORQUE_CTRL)
    {
        displayBuffer[5] &= LED_CODE[LED_TUNE];
    }
    else
    {
        displayBuffer[5] |= ~LED_CODE[LED_TUNE];
    }

    DispDataDeal();

    if (checkMenuModeCmd)
    {
        DealCheckMenuModeGroupGrade(checkMenuModePara);
    }

    if ((!errorCodeOld)
        && (errorCode)
        && ((errAutoRstNum == 0)
            || (errAutoRstNum == funcCode.code.errAutoRstNumMax)
            || (funcCode.code.errAutoRstNumMax == 0))
        )
    {
        Menu1OnPrg();
    }

    if (!scanStep)
    {

        displayBuffer[5] |= ~LED_CODE[LED_HZ];
        displayBuffer[5] |= ~LED_CODE[LED_A];
        displayBuffer[5] |= ~LED_CODE[LED_V];

        menuAttri[menuLevel].winkFlag = 0;

        if (MENU_MODE_NONE == menuModeStatus)
        {
            menu[menuLevel].UpdateDisplayBuffer();
        }
        else
        {
            UpdateMenuModeDisplayBuffer();
        }
    }
}

void WinkDeal(void)
{
    static Uint16 enflashOld;
    static Uint16 winkFlagOld;
    static Uint16 menuLevelOld;
    static Uint16 winkFlagLedOld;
    Uint16 winkFlag1 = menuAttri[menuLevel].winkFlag;
    Uint16 winkFlagLed1 = menuAttri[menuLevel].winkFlagLed;
    int16 i;

#if DSP_2803X
    if (keyChecking == 1)
    {
        return;
    }
#endif

#if DSP_2803X
    spiData.out.a.bit.dispCode = displayBuffer[scanStep];
#else
    spiData.out.a.bit.doData = (doHwStatus.all);
    spiData.out.a.bit.dispCode = displayBuffer[scanStep];
#endif

    if ((winkFlagOld != winkFlag1)
        || (menuLevelOld != menuLevel)
        || (enflashOld != bEnflash)
        )
    {
        for (i = DISPLAY_8LED_NUM - 1; i >= 0; i--)
        {

            if ((bOnShift)

                )
            {
                bLight[i] = 0;

                flashTicker[i] = flashTickerMax >> 2;
            }
            else
            {
                bLight[i] = !0;

                flashTicker[i] = flashTickerMax >> 2;
            }
        }
    }

    if (winkFlagLedOld != winkFlagLed1)
    {
        for (i = DISPLAY_LED_NUM - 1; i >= 0; i--)
        {
            if ((winkFlagLed1 ^ winkFlagLedOld) & (0x01U << i))
            {
                flashTickerLed[i] = 0;

                displayBuffer[5] ^= ~LED_CODE[i];
            }
        }

        winkFlagLedOld = winkFlagLed1;
    }

    winkFlagOld = winkFlag1;
    winkFlagLedOld = winkFlagLed1;
    menuLevelOld = menuLevel;
    enflashOld = bEnflash;

    if (scanStep < DISPLAY_8LED_NUM)
    {
        if ((FLASH_YES == bEnflash) && (winkFlag1 & (0x01U << (7 - scanStep))))
        {

            if (++flashTicker[scanStep] >= flashTickerMax)
            {
                flashTicker[scanStep] = 0;
                bLight[scanStep] = !bLight[scanStep];
            }

            if (!bLight[scanStep])
            {
                spiData.out.a.bit.dispCode = DISPLAY_CODE[DISPLAY_NULL];
            }
        }
    }
    else
    {
        Uint16 flashMax;

        for (i = DISPLAY_LED_NUM - 1; i >= 0; i--)
        {
#if DEBUG_F_ERROR_INDICATION
            flashMax = flashTickerLedMax;

            if (((i == LED_TUNE)
                && (errorCode))
#if 0
                || ((i == LED_LOCAL)
                    && (FUNCCODE_runSrc_AUTO_RUN == runSrc))
#endif
                )
            {
                flashMax = flashMaxSDSD;

                if (++flashTickerLed[i] >= flashMax)
                {
                    flashTickerLed[i] = 0;
                    displayBuffer[5] ^= ~LED_CODE[i];
                }
            }
            else
#endif

            if (winkFlagLed1 & (0x01U << i))
            {

                if (++flashTickerLed[i] >= flashMax)
                {
                    flashTickerLed[i] = 0;
                    displayBuffer[5] ^= ~LED_CODE[i];
                }
            }
        }
    }

#if DEBUG_F_NEWMENU_USE_DOT_REVERSE_DIR
    if (((0 == scanStep) && runFlag.bit.curDir)
        || ((4 == scanStep) && runFlag.bit.dir))
    {
        spiData.out.a.bit.dispCode &= (DISPLAY_CODE[DISPLAY_DOT]);
    }
#endif
}

void DisplayScanPrepare(void)
{
    Uint16 RXFFST;
    int16 i;
    static int16 j;
    static Uint16 tcnt;

    RXFFST = SpiaRegs.SPIFFRX.bit.RXFFST;

#if DSP_2803X
    RCK = 0;
#else
    if (RXFFST >= SPI_LENGTH)
    {
        RCK = 0;
    }
#endif

    for (i = 0; i < RXFFST; i++)
    {
        spiData.in.all[j] = SpiaRegs.SPIRXBUF;
#if DSP_2803X
            j = 0;
#else
        if (++j >= SPI_LENGTH)
            j = 0;
#endif
    }

#if DSP_2803X
    if (!keySelStatus)
    {
         keyValue = spiData.in.key >> 7;
    }

    spiData.out.a.bit.dispSect = 0x7F & (~(1U << (5 - scanStep)));
#else
    diHwStatus.all = ((spiData.in.c.di & 0x00FF) << 8) + (spiData.in.c.di >> 8);
    keyValue = spiData.in.c.key >> 7;

    spiData.out.b.bit.dispSect = 0x7F & (~(1U << (5 - scanStep)));
#endif

#if DSP_2803X
    keySelTypeBak = keySelType;
    if (!tcnt)
    {
        keySelType = 1;
        keySelStatus = 1;
    }
    else
    {
        if (keySelStatus)
        {
            (!(spiData.in.key >> 7)) ? (keySelType = 0) : (keySelType = 1);
            keySelStatus = 0;
        }
    }
	(tcnt >= KEY_SEL_TEST_TIME) ? (tcnt = 0) : (tcnt++);

    if ((keySelType == 1) && (keySelTypeBak == 0))
    {
        keyChecking = 1;
    }
    else
    {
        keyChecking = 0;
    }

#endif

#if DSP_2803X
	if (!keyChecking)
#endif
	{
	    if (++scanStep >= DISPLAY_8LED_NUM + 1)
	    {
	        scanStep = 0;
	    }
	}
}

void DisplayScan(void)
{

    RCK = 1;

#if DSP_2803X
    KEYSEL = keySelType;
    SpiaRegs.SPITXBUF = ~spiData.out.a.all;

#else
    SpiaRegs.SPITXBUF = spiData.out.b.all;
    SpiaRegs.SPITXBUF = spiData.out.a.all;
#endif
}

void KeyScan(void)
{
    keyRet keyTemp = {0, 0, 0, 0};
    int16 j = 0;
    Uint16 keyValueTmp;

#if DEBUG_F_KEY_ANIMATE
    keyValue = (~((0x1U << (KEY_UP - 1)))) & ((1 << KEY_NUM) - 1);
#endif

    keyValueTmp = ((~keyValue) & ((1 << KEY_NUM) - 1)) << 1;
    if (keyValueTmp)
    {
        for (j = KEY_NUM; j >= 1; j--)
        {
            if ((keyValueTmp & (0x01U << j)))
            {
                if (((keyValueTmp) & ctrlKeyFlag) && (ctrlKeyFlag == (0x01 << j)))
                {
                    keyTemp.ctrlCnt++;
                    keyTemp.ctrlVal = j;
                }
                else
                {
                    keyTemp.funcCnt++;
                    keyTemp.funcVal = j;
                }
            }
        }
    }

    if ((!keyTemp.funcCnt) || (keyTemp.ctrlCnt > 1))
    {
        keyPreFunc = 0;
        keyPreCtrl = 0;
        keyFunc = 0;
        keyAge = 0;
        keyStatus = KEY_STATUS_JITTER_DEAL;
        bKeyEsc = 0;
        bEnflash = FLASH_YES;

        accDecFrqPrcFlag = ACC_DEC_FRQ_NONE;

        upDownDelta = 1;
    }
    else if (!bKeyEsc)
    {
        if (keyTemp.funcCnt > 1)
            bKeyEsc = 1;
        else if ((keyTemp.funcVal != keyPreFunc)
                 || ((keyTemp.ctrlVal != keyPreCtrl) && keyPreCtrl)
                 )

        {
            keyPreFunc = keyTemp.funcVal;
            keyPreCtrl = keyTemp.ctrlVal;
            keyAge = 0;
        }
        else if ((!keyPreCtrl) && keyTemp.ctrlVal)
        {
            bKeyEsc = 1;
        }
        else
        {
            keyAge++;
            switch (keyStatus)
            {
                case KEY_STATUS_JITTER_DEAL:
                    if (keyAge >= keyAgeMax)
                    {
                        bKeyProc = 1;
                        if ((((continueKeyFlag) & (0x01U << keyTemp.funcVal)) == 0)

                            )
                            bKeyEsc = 1;
                        else
                        {
                            keyStatus = KEY_STATUS_WAIT_REPEAT;
                            keyAge = 0;
                        }
                    }
                    break;

                case KEY_STATUS_WAIT_REPEAT:
                    if (keyAge >= maxRate)
                    {
                        bKeyProc = 1;
                        keyAge = 0;
                        keyStatus = KEY_STATUS_REPEAT;
                        repeatNum = 0;
                        repeatStep = 0;
                    }
                    break;

                case KEY_STATUS_REPEAT:
                    if (keyAge >= minRate[repeatStep])
                    {
                        keyAge = 0;
                        bKeyProc = 1;
                        repeatNum++;

                        if (bFrqDigital)
                        {

                            if (repeatNum >= repeatNumMax[repeatStep])
                            {
                                if (repeatStep < 2)
                                {
                                    bKeyProc = 0;
                                    keyStatus = KEY_STATUS_WAIT_CARRY;
                                }
                            }
                        }
                    }
                    break;

                case KEY_STATUS_WAIT_CARRY:
                    if (keyAge >= minRatePause)
                    {
                        keyAge = 0;
                        bKeyProc = 1;
                        keyStatus = KEY_STATUS_REPEAT;
                        upDownDelta *= 10;
                        repeatStep++;
                        repeatNum = 0;
                    }
                    break;

                default:
                    break;
            }
        }
    }

#if DEBUG_F_KEY_ANIMATE
    {
        static Uint16 i;
        if (++i >= keyAnimateTime)
        {
            bKeyProc = 1;
            keyPreFunc = KEY_UP;
            i = 0;
        }
    }
#endif
}

void KeyBordTest(void)
{

    if (keyBordTestFlag)
    {

        if (keyPreFunc)
        {
            keyBordValue |= (1<<(keyPreFunc - 1));
        }
    }
    else
    {
        keyBordValue = 0;
    }
}

void KeyProcess(void)
{
    KeyBordTest();

    if ((KEY_REV_JOG != keyFunc) && (KEY_FWD_JOG != keyFunc))
    {
        keyFunc = 0;
    }

    if (!bKeyProc)
    {

        if (MENU_MODE_ON_QUICK == menuModeStatus)
        {
            if (++onQuickTicker >= onQuickTickerMax)
            {
                onQuickTicker = 0;

                menuModeStatus = MENU_MODE_NONE;
                menuModeTmp = menuMode;
            }
        }

#if DEBUG_RANDOM_FACPASS

        if (facPassViewStatus == FAC_PASS_VIEW)
        {
            if (++onShiftPassViewTicker >= onShiftPassViewTickerMax)
            {
                onShiftPassViewTicker = 0;
                facPassViewStatus = FAC_PASS_NONE;
            }
        }
#endif

        return;
    }

    bKeyProc = 0;
    bOnShift = 0;
    keyFunc = keyPreFunc;

    if ((MENU_MODE_ON_QUICK == menuModeStatus) &&
        (KEY_QUICK != keyPreFunc) &&
        ((KEY_ENTER != keyPreFunc)

        )
        )
    {
        onQuickTicker = 0;

        menuModeStatus = MENU_MODE_NONE;
        menuModeTmp = menuMode;

        return;
    }

    switch (keyPreFunc)
    {
        case KEY_PRG:
            menu[menuLevel].onPrgFunc();
            break;

        case KEY_UP:
            bEnflash = FLASH_NO;
            menu[menuLevel].onUpFunc();
            break;

        case KEY_ENTER:
            menu[menuLevel].onEnterFunc();
            break;

        case KEY_MFK:
            menu[menuLevel].onMfkFunc();
            break;

        case KEY_DOWN:
            bEnflash = FLASH_NO;
            menu[menuLevel].onDownFunc();
            break;

        case KEY_SHIFT:
            bOnShift = 1;
            menu[menuLevel].onShiftFunc();
            break;

        case KEY_RUN:
            menu[menuLevel].onRunFunc();
            break;

        case KEY_STOP:
            menu[menuLevel].onStopFunc();
            break;

        case KEY_QUICK:
            onQuickTicker = 0;
            menu[menuLevel].onQuickFunc();
            break;

        default:
            break;
    }
}

void InitSpiaGpio(void)
{
   EALLOW;

    GpioCtrlRegs.GPAPUD.bit.GPIO16 = 0;
    GpioCtrlRegs.GPAPUD.bit.GPIO17 = 0;
    GpioCtrlRegs.GPAPUD.bit.GPIO18 = 0;

    GpioCtrlRegs.GPAQSEL2.bit.GPIO16 = 3;
    GpioCtrlRegs.GPAQSEL2.bit.GPIO17 = 3;
    GpioCtrlRegs.GPAQSEL2.bit.GPIO18 = 3;

    GpioCtrlRegs.GPAMUX2.bit.GPIO16 = 1;
    GpioCtrlRegs.GPAMUX2.bit.GPIO17 = 1;
    GpioCtrlRegs.GPAMUX2.bit.GPIO18 = 1;

#if DSP_2803X
    GpioCtrlRegs.GPAMUX2.bit.GPIO22 = 0;
    GpioCtrlRegs.GPADIR.bit.GPIO22 = 1;

    GpioDataRegs.GPADAT.bit.GPIO22 = 0;

    GpioCtrlRegs.AIOMUX1.bit.AIO4 = 0;
    GpioCtrlRegs.AIODIR.bit.AIO4 = 1;
    GpioDataRegs.AIODAT.bit.AIO4 = 1;

#else

    GpioCtrlRegs.GPAMUX2.bit.GPIO19 = 0;
    GpioCtrlRegs.GPADIR.bit.GPIO19 = 1;

    GpioDataRegs.GPADAT.bit.GPIO19 = 0;
#endif
    EDIS;
}

void InitSetSpiForFunc(void)
{

    SpiaRegs.SPIFFTX.all = 0xE040;
    SpiaRegs.SPIFFRX.all = 0x405F;
    SpiaRegs.SPIFFRX.all = 0x205F;
    SpiaRegs.SPIFFCT.all = 0x0000;

    SpiaRegs.SPICCR.all = 0x000F;
    SpiaRegs.SPICTL.all = 0x000E;

#if (DSP_CLOCK == 100)
    SpiaRegs.SPIBRR = 0x007F;
#elif (DSP_CLOCK == 60)
    SpiaRegs.SPIBRR = 0x004C;
#endif

    SpiaRegs.SPICCR.bit.SPISWRESET = 1;
    SpiaRegs.SPIPRI.bit.FREE = 1;

}

void UpdateMenuModeDisplayBuffer()
{

    displayBuffer[0] = DISPLAY_CODE[DISPLAY_LINE];
    displayBuffer[1] = DISPLAY_CODE[dispCodeMenuMode[menuModeTmp-1][0]];
    displayBuffer[2] = DISPLAY_CODE[dispCodeMenuMode[menuModeTmp-1][1]];
    displayBuffer[3] = DISPLAY_CODE[dispCodeMenuMode[menuModeTmp-1][2]];
    displayBuffer[4] = DISPLAY_CODE[dispCodeMenuMode[menuModeTmp-1][3]];
}
