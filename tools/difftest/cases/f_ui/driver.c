/* Differential test driver for f_ui.

 * Ten entry points. Nine of them are ordinary "call once, look at the state"
 * functions; two carry FILE-SCOPE STATICS and therefore have to be walked as
 * sequences rather than sampled:

 *   WinkDeal             static enflashOld / winkFlagOld / menuLevelOld /
 *                        winkFlagLedOld, plus the flash tickers. The tickers
 *                        only ever reach their thresholds (42 for the 8-LED
 *                        digit, flashMaxSDSD / 45 for the LEDs) after that many
 *                        consecutive calls with the same scanStep. One call
 *                        exercises none of the toggling.
 *   DisplayScanPrepare   static tcnt, which counts 0..KEY_SEL_TEST_TIME (750)
 *                        and only then wraps; the keySelType edge detector
 *                        (`keySelType == 1 && keySelTypeBak == 0` -> keyChecking)
 *                        is on the other side of that wrap. Also static j, which
 *                        under DSP_2803X is reset to 0 on every loop iteration.

 * Both are stepped in LOCKSTEP: ref and new each get the identical call
 * sequence, and the comparison happens after every single call, so a
 * divergence surfaces on the call that caused it rather than 700 calls later.

 * ---- the two-worlds split -------------------------------------------------

 * The module owns a large amount of state (displayBuffer, spiData, the whole
 * key state machine, the flash tickers). All of it has external linkage and is
 * therefore renamed ref_ / new_, so each side has its own copy: the driver
 * installs both from one input image and snapshots both back.

 * Everything else is SHARED and must look identical to both sides on every
 * call. That includes the peripheral blocks (SpiaRegs, GpioCtrlRegs,
 * GpioDataRegs) -- the module reads, writes and never allocates them, so the
 * driver plays the chip -- and the 21 globals it borrows from other modules.

 * `menuAttri` is the reason the shared image has to be REINSTALLED before each
 * side rather than just once: UpdateDisplayBuffer and WinkDeal both write
 * `menuAttri[menuLevel].winkFlag(Led)`. Install once and the second side would
 * see the first side's output as its input.

 * ---- what the driver owns -------------------------------------------------

 * The peripheral image. Before every call it installs a complete register image
 * (a deterministic PRNG byte stream, optionally with specific bits forced), and
 * after the call it snapshots all three blocks back. Comparing whole blocks --
 * not just the bits the module "should" touch -- is what makes "the rewrite
 * stopped writing SPIBRR" or "wrote the wrong bit into GPAMUX2" visible.

 * ---- what is compared -----------------------------------------------------

 *   per-side : spiData, diHwStatus, displayBuffer, the whole key state machine,
 *              the flash tickers, scanStep, the keySel* group, the tickers
 *   shared   : SpiaRegs + GpioCtrlRegs + GpioDataRegs, cpuCtl[7] (EALLOW/EDIS),
 *              menuAttri, runFlag, runSrc, runMode, tuneCmd, errorCode,
 *              errorCodeOld, errAutoRstNum, accDecFrqPrcFlag, bFrqDigital,
 *              checkMenuModeCmd, checkMenuModePara, funcCode,
 *              and the CALL LOG -- as a SEQUENCE, not a count, so that
 *              `menu[0].onUpFunc()` where the source says `menu[menuLevel]`
 *              is visible.
 *   const    : DISPLAY_CODE / LED_CODE / dispCodeMenuMode / minRate /
 *              repeatNumMax, so that a rewrite which corrupts a table is caught
 *              even though both sides would otherwise share the corruption.

 * ---- host-unrepresentable regions, stated rather than glossed over ---------

 *   (1) `UpdateMenuModeDisplayBuffer` indexes `dispCodeMenuMode[menuModeTmp-1]`.
 *       menuModeTmp == 0 would read one row before the array. On the target that
 *       is a harmless read of whatever precedes it in .econst; on the host it is
 *       an access violation. The bench therefore feeds menuModeTmp in 1..6 only.
 *       It is not a claim that 0 is unreachable in the firmware -- only that this
 *       bench does not test it.

 *   (2) `KeyScan`'s `keyPreFunc` reaches `1 << (keyPreFunc - 1)`. Values above 9
 *       would shift past the width of an int; the module itself only ever stores
 *       0 or a funcVal in 1..9, so the bench stays inside that set.

 *   (3) `keyStatus` and `bEnflash` are `enum KEY_STATUS` / `enum EN_FLASH`,
 *       declared inside f_ui.c and not in any header. On the host an MSVC enum is
 *       an `int`; on C28x it is 16 bits. The driver re-declares both tags with
 *       the same enumerators, which C treats as compatible with the same
 *       underlying type, so the comparison is exact -- but the *storage width*
 *       is a host/target difference and this bench says nothing about it.

 *   (4) The dead branch in KeyScan. `ctrlKeyFlag` is `#define ctrlKeyFlag 0`, so
 *       `(keyValueTmp & ctrlKeyFlag)` is always 0 and `keyTemp.ctrlCnt` never
 *       leaves 0. Consequently `keyTemp.ctrlCnt > 1`, `keyTemp.ctrlVal`, and
 *       `((!keyPreCtrl) && keyTemp.ctrlVal)` are all unreachable. They are NOT
 *       covered by this bench and are NOT claimed to be; a mutation anchored
 *       there would be equivalent on the reachable set, not missed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DSP28x_Project.h"
#include "mock_ui.h"

#define FAIL_LIMIT 20

/* "Exactly key k is pressed, no other key" as the module wants to read it.
 *
 * KeyScan derives its key number from the COMPLEMENT of keyValue:
 *     keyValueTmp = ((~keyValue) & ((1 << KEY_NUM) - 1)) << 1;
 * so a set bit j of keyValueTmp -- which is what becomes `funcVal` -- means bit
 * (j-1) of keyValue is ZERO. One key down therefore has to be all-ones except
 * that one bit; `1U << (k - 1)` is the inverse and yields funcCnt == KEY_NUM - 1.
 * Stated as a macro so no phase can get it backwards again.
 */
#define KEYV_ONE(k)  ((Uint16)((((1U << KEY_NUM) - 1) ^ (1U << ((k) - 1)))))

/* ---- the two file-private enums of f_ui.c ---------------------------------
 *
 * Not in any header, so re-declared here with the same enumerators. C says an
 * enumerated type is compatible with its underlying integer type, and MSVC
 * picks `int` for both sides, so the objects agree in size and linkage.
 */
enum KEY_STATUS
{
    KEY_STATUS_JITTER_DEAL,
    KEY_STATUS_WAIT_REPEAT,
    KEY_STATUS_REPEAT,
    KEY_STATUS_WAIT_CARRY
};

enum EN_FLASH
{
    FLASH_YES,
    FLASH_NO
};

/* ---- ref side ------------------------------------------------------------ */

extern void ref_UpdateDisplayBuffer(void);
extern void ref_WinkDeal(void);
extern void ref_DisplayScanPrepare(void);
extern void ref_DisplayScan(void);
extern void ref_KeyScan(void);
extern void ref_KeyBordTest(void);
extern void ref_KeyProcess(void);
extern void ref_InitSpiaGpio(void);
extern void ref_InitSetSpiForFunc(void);
extern void ref_UpdateMenuModeDisplayBuffer(void);

extern struct SPI_IN_OUT      ref_spiData;
extern union DI_HW_STATUS     ref_diHwStatus;
extern Uint16                 ref_displayBuffer[DISPLAY_8LED_NUM + 1];
extern Uint16                 ref_keyValue;
extern enum KEY_STATUS        ref_keyStatus;
extern Uint16                 ref_keyAge;
extern Uint16                 ref_bKeyEsc;
extern Uint16                 ref_bKeyProc;
extern Uint16                 ref_keyPreFunc;
extern Uint16                 ref_keyFunc;
extern Uint16                 ref_keyPreCtrl;
extern Uint16                 ref_repeatNum;
extern Uint16                 ref_repeatStep;
extern Uint16                 ref_bOnShift;
extern Uint16                 ref_upDownDelta;
extern enum EN_FLASH          ref_bEnflash;
extern Uint16                 ref_bLight[DISPLAY_8LED_NUM];
extern Uint16                 ref_flashTicker[DISPLAY_8LED_NUM];
extern Uint16                 ref_flashTickerLed[DISPLAY_LED_NUM];
extern Uint16                 ref_scanStep;
extern Uint16                 ref_keySelStatus;
extern Uint16                 ref_keySelType;
extern Uint16                 ref_keySelTypeBak;
extern Uint16                 ref_keyChecking;
extern Uint16                 ref_keyBordTestFlag;
extern Uint16                 ref_keyBordValue;
extern Uint16                 ref_onQuickTicker;
extern Uint16                 ref_onQuickTickerMax;
extern Uint16                 ref_onShiftPassViewTicker;
extern Uint16                 ref_onShiftPassViewTickerMax;
extern Uint16                 ref_factoryPwd;
extern Uint16                 ref_flashMaxSDSD;

extern const Uint16 ref_DISPLAY_CODE[DISPLAY_8LED_CODE_NUM];
extern const Uint16 ref_LED_CODE[DISPLAY_LED_CODE_NUM];
extern const Uint16 ref_dispCodeMenuMode[6][4];
extern const Uint16 ref_minRate[FUI_MIN_RATE_N];
extern const Uint16 ref_repeatNumMax[FUI_REPEAT_NUM_MAX_N];

/* ---- new side ------------------------------------------------------------ */

extern void new_UpdateDisplayBuffer(void);
extern void new_WinkDeal(void);
extern void new_DisplayScanPrepare(void);
extern void new_DisplayScan(void);
extern void new_KeyScan(void);
extern void new_KeyBordTest(void);
extern void new_KeyProcess(void);
extern void new_InitSpiaGpio(void);
extern void new_InitSetSpiForFunc(void);
extern void new_UpdateMenuModeDisplayBuffer(void);

extern struct SPI_IN_OUT      new_spiData;
extern union DI_HW_STATUS     new_diHwStatus;
extern Uint16                 new_displayBuffer[DISPLAY_8LED_NUM + 1];
extern Uint16                 new_keyValue;
extern enum KEY_STATUS        new_keyStatus;
extern Uint16                 new_keyAge;
extern Uint16                 new_bKeyEsc;
extern Uint16                 new_bKeyProc;
extern Uint16                 new_keyPreFunc;
extern Uint16                 new_keyFunc;
extern Uint16                 new_keyPreCtrl;
extern Uint16                 new_repeatNum;
extern Uint16                 new_repeatStep;
extern Uint16                 new_bOnShift;
extern Uint16                 new_upDownDelta;
extern enum EN_FLASH          new_bEnflash;
extern Uint16                 new_bLight[DISPLAY_8LED_NUM];
extern Uint16                 new_flashTicker[DISPLAY_8LED_NUM];
extern Uint16                 new_flashTickerLed[DISPLAY_LED_NUM];
extern Uint16                 new_scanStep;
extern Uint16                 new_keySelStatus;
extern Uint16                 new_keySelType;
extern Uint16                 new_keySelTypeBak;
extern Uint16                 new_keyChecking;
extern Uint16                 new_keyBordTestFlag;
extern Uint16                 new_keyBordValue;
extern Uint16                 new_onQuickTicker;
extern Uint16                 new_onQuickTickerMax;
extern Uint16                 new_onShiftPassViewTicker;
extern Uint16                 new_onShiftPassViewTickerMax;
extern Uint16                 new_factoryPwd;
extern Uint16                 new_flashMaxSDSD;

extern const Uint16 new_DISPLAY_CODE[DISPLAY_8LED_CODE_NUM];
extern const Uint16 new_LED_CODE[DISPLAY_LED_CODE_NUM];
extern const Uint16 new_dispCodeMenuMode[6][4];
extern const Uint16 new_minRate[FUI_MIN_RATE_N];
extern const Uint16 new_repeatNumMax[FUI_REPEAT_NUM_MAX_N];

/* ---- per-side snapshot --------------------------------------------------- */

struct SIDE
{
    struct SPI_IN_OUT  spi;
    union DI_HW_STATUS di;
    Uint16             displayBuffer[DISPLAY_8LED_NUM + 1];
    Uint16             keyValue;
    Uint16             keyStatus;
    Uint16             keyAge;
    Uint16             bKeyEsc;
    Uint16             bKeyProc;
    Uint16             keyPreFunc;
    Uint16             keyFunc;
    Uint16             keyPreCtrl;
    Uint16             repeatNum;
    Uint16             repeatStep;
    Uint16             bOnShift;
    Uint16             upDownDelta;
    Uint16             bEnflash;
    Uint16             bLight[DISPLAY_8LED_NUM];
    Uint16             flashTicker[DISPLAY_8LED_NUM];
    Uint16             flashTickerLed[DISPLAY_LED_NUM];
    Uint16             scanStep;
    Uint16             keySelStatus;
    Uint16             keySelType;
    Uint16             keySelTypeBak;
    Uint16             keyChecking;
    Uint16             keyBordTestFlag;
    Uint16             keyBordValue;
    Uint16             onQuickTicker;
    Uint16             onQuickTickerMax;
    Uint16             onShiftPassViewTicker;
    Uint16             onShiftPassViewTickerMax;
    Uint16             factoryPwd;
    Uint16             flashMaxSDSD;
};

static struct SIDE inSide;
static struct SIDE S[2];

#define SIDE_INSTALL(P)                                                       \
    do {                                                                      \
        memcpy(&P##spiData, &inSide.spi, sizeof inSide.spi);                  \
        memcpy(&P##diHwStatus, &inSide.di, sizeof inSide.di);                 \
        memcpy(P##displayBuffer, inSide.displayBuffer,                        \
               sizeof inSide.displayBuffer);                                  \
        memcpy(P##bLight, inSide.bLight, sizeof inSide.bLight);               \
        memcpy(P##flashTicker, inSide.flashTicker,                            \
               sizeof inSide.flashTicker);                                    \
        memcpy(P##flashTickerLed, inSide.flashTickerLed,                      \
               sizeof inSide.flashTickerLed);                                 \
        P##keyValue          = inSide.keyValue;                               \
        P##keyStatus         = (enum KEY_STATUS)inSide.keyStatus;             \
        P##keyAge            = inSide.keyAge;                                 \
        P##bKeyEsc           = inSide.bKeyEsc;                                \
        P##bKeyProc          = inSide.bKeyProc;                               \
        P##keyPreFunc        = inSide.keyPreFunc;                             \
        P##keyFunc           = inSide.keyFunc;                                \
        P##keyPreCtrl        = inSide.keyPreCtrl;                             \
        P##repeatNum         = inSide.repeatNum;                              \
        P##repeatStep        = inSide.repeatStep;                             \
        P##bOnShift          = inSide.bOnShift;                               \
        P##upDownDelta       = inSide.upDownDelta;                            \
        P##bEnflash          = (enum EN_FLASH)inSide.bEnflash;                \
        P##scanStep          = inSide.scanStep;                               \
        P##keySelStatus      = inSide.keySelStatus;                           \
        P##keySelType        = inSide.keySelType;                             \
        P##keySelTypeBak     = inSide.keySelTypeBak;                          \
        P##keyChecking       = inSide.keyChecking;                            \
        P##keyBordTestFlag   = inSide.keyBordTestFlag;                        \
        P##keyBordValue      = inSide.keyBordValue;                           \
        P##onQuickTicker     = inSide.onQuickTicker;                          \
        P##onQuickTickerMax  = inSide.onQuickTickerMax;                       \
        P##onShiftPassViewTicker    = inSide.onShiftPassViewTicker;           \
        P##onShiftPassViewTickerMax = inSide.onShiftPassViewTickerMax;        \
        P##factoryPwd        = inSide.factoryPwd;                             \
        P##flashMaxSDSD      = inSide.flashMaxSDSD;                           \
    } while (0)

#define SIDE_SNAPSHOT(P, s)                                                   \
    do {                                                                      \
        memcpy(&(s)->spi, &P##spiData, sizeof (s)->spi);                      \
        memcpy(&(s)->di, &P##diHwStatus, sizeof (s)->di);                     \
        memcpy((s)->displayBuffer, P##displayBuffer,                          \
               sizeof (s)->displayBuffer);                                    \
        memcpy((s)->bLight, P##bLight, sizeof (s)->bLight);                   \
        memcpy((s)->flashTicker, P##flashTicker, sizeof (s)->flashTicker);    \
        memcpy((s)->flashTickerLed, P##flashTickerLed,                        \
               sizeof (s)->flashTickerLed);                                   \
        (s)->keyValue        = P##keyValue;                                   \
        (s)->keyStatus       = (Uint16)P##keyStatus;                          \
        (s)->keyAge          = P##keyAge;                                     \
        (s)->bKeyEsc         = P##bKeyEsc;                                    \
        (s)->bKeyProc        = P##bKeyProc;                                   \
        (s)->keyPreFunc      = P##keyPreFunc;                                 \
        (s)->keyFunc         = P##keyFunc;                                    \
        (s)->keyPreCtrl      = P##keyPreCtrl;                                 \
        (s)->repeatNum       = P##repeatNum;                                  \
        (s)->repeatStep      = P##repeatStep;                                 \
        (s)->bOnShift        = P##bOnShift;                                   \
        (s)->upDownDelta     = P##upDownDelta;                                \
        (s)->bEnflash        = (Uint16)P##bEnflash;                           \
        (s)->scanStep        = P##scanStep;                                   \
        (s)->keySelStatus    = P##keySelStatus;                               \
        (s)->keySelType      = P##keySelType;                                 \
        (s)->keySelTypeBak   = P##keySelTypeBak;                              \
        (s)->keyChecking     = P##keyChecking;                                \
        (s)->keyBordTestFlag = P##keyBordTestFlag;                            \
        (s)->keyBordValue    = P##keyBordValue;                               \
        (s)->onQuickTicker   = P##onQuickTicker;                              \
        (s)->onQuickTickerMax = P##onQuickTickerMax;                          \
        (s)->onShiftPassViewTicker    = P##onShiftPassViewTicker;             \
        (s)->onShiftPassViewTickerMax = P##onShiftPassViewTickerMax;          \
        (s)->factoryPwd      = P##factoryPwd;                                 \
        (s)->flashMaxSDSD    = P##flashMaxSDSD;                               \
    } while (0)

static void install_side(int side)
{
    if (side == 0)
    {
        SIDE_INSTALL(ref_);
    }
    else
    {
        SIDE_INSTALL(new_);
    }
}

static void snapshot(int side)
{
    if (side == 0)
    {
        SIDE_SNAPSHOT(ref_, &S[0]);
    }
    else
    {
        SIDE_SNAPSHOT(new_, &S[1]);
    }
}

/* ---- the shared world ---------------------------------------------------- */

struct WORLD
{
    struct SPI_REGS        spi;
    struct GPIO_CTRL_REGS  gctrl;
    struct GPIO_DATA_REGS  gdata;

    FUNCCODE_ALL           funcCode;
    struct MENU_ATTRIBUTE  menuAttri[MENU_LEVEL_NUM];
    enum MENU_LEVEL        menuLevel;
    enum MENU_MODE         menuMode;
    enum MENU_MODE         menuModeTmp;
    enum MENU_MODE_OPERATE menuModeStatus;

    union RUN_FLAG         runFlag;
    Uint16                 runMode;
    Uint16                 runSrc;
    Uint16                 tuneCmd;

    Uint16                 errorCode;
    Uint16                 errorCodeOld;
    Uint16                 errAutoRstNum;

    Uint16                 accDecFrqPrcFlag;
    Uint16                 bFrqDigital;

    Uint16                 checkMenuModeCmd;
    Uint16                 checkMenuModePara;

    Uint16                 cpu[7];

    struct UI_LOG_ENT      log[UI_LOG_LEN];
    Uint16                 logN;
    Uint16                 logOverflow;
};

static struct WORLD inW;
static struct WORLD SH[2];

static void install_world(void)
{
    int i;

    memcpy((void *)&SpiaRegs,    &inW.spi,    sizeof inW.spi);
    memcpy((void *)&GpioCtrlRegs, &inW.gctrl, sizeof inW.gctrl);
    memcpy((void *)&GpioDataRegs, &inW.gdata, sizeof inW.gdata);

    memcpy(&funcCode, &inW.funcCode, sizeof funcCode);
    memcpy(menuAttri, inW.menuAttri, sizeof menuAttri);
    menuLevel      = inW.menuLevel;
    menuMode       = inW.menuMode;
    menuModeTmp    = inW.menuModeTmp;
    menuModeStatus = inW.menuModeStatus;

    runFlag.all = inW.runFlag.all;
    runMode     = inW.runMode;
    runSrc      = inW.runSrc;
    tuneCmd     = inW.tuneCmd;

    errorCode     = inW.errorCode;
    errorCodeOld  = inW.errorCodeOld;
    errAutoRstNum = inW.errAutoRstNum;

    accDecFrqPrcFlag = inW.accDecFrqPrcFlag;
    bFrqDigital      = inW.bFrqDigital;

    checkMenuModeCmd  = inW.checkMenuModeCmd;
    checkMenuModePara = inW.checkMenuModePara;

    for (i = 0; i < 7; i++)
    {
        gHostCpuCtl[i] = 0;
    }

    /* The call log is a SHARED resource, so it has to be cleared for each side.
     * Counting alone would not be enough: the interesting error class is "called
     * the same functions in a different order / through a different menu row",
     * which is only visible in the sequence. */
    gUiLogN        = 0;
    gUiLogOverflow = 0;
}

static void snap_world(int side)
{
    struct WORLD *s = &SH[side];
    int i;

    memcpy(&s->spi,    (const void *)&SpiaRegs,    sizeof s->spi);
    memcpy(&s->gctrl,  (const void *)&GpioCtrlRegs, sizeof s->gctrl);
    memcpy(&s->gdata,  (const void *)&GpioDataRegs, sizeof s->gdata);

    memcpy(&s->funcCode, &funcCode, sizeof s->funcCode);
    memcpy(s->menuAttri, menuAttri, sizeof s->menuAttri);
    s->menuLevel      = menuLevel;
    s->menuMode       = menuMode;
    s->menuModeTmp    = menuModeTmp;
    s->menuModeStatus = menuModeStatus;

    s->runFlag.all = runFlag.all;
    s->runMode     = runMode;
    s->runSrc      = runSrc;
    s->tuneCmd     = tuneCmd;

    s->errorCode     = errorCode;
    s->errorCodeOld  = errorCodeOld;
    s->errAutoRstNum = errAutoRstNum;

    s->accDecFrqPrcFlag = accDecFrqPrcFlag;
    s->bFrqDigital      = bFrqDigital;

    s->checkMenuModeCmd  = checkMenuModeCmd;
    s->checkMenuModePara = checkMenuModePara;

    for (i = 0; i < 7; i++)
    {
        s->cpu[i] = gHostCpuCtl[i];
    }

    memcpy(s->log, gUiLog, sizeof s->log);
    s->logN        = gUiLogN;
    s->logOverflow = gUiLogOverflow;
}

/* ---- reporting ----------------------------------------------------------- */

static long g_cases = 0;
static int  g_fails = 0;
static const char *g_cur = "";

static void report(const char *what)
{
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s: %s\n", g_cur, what);
    printf("  run=%u runSrc=%u runMode=%u dirRev=%u dir=%u tuneCmd=%u\n",
           (unsigned)inW.runFlag.bit.run, (unsigned)inW.runSrc,
           (unsigned)inW.runMode, (unsigned)inW.runFlag.bit.dirReversing,
           (unsigned)inW.runFlag.bit.dir, (unsigned)inW.tuneCmd);
    printf("  err=%u errOld=%u errRst=%u errRstMax=%u ckCmd=%u ckPara=%u\n",
           (unsigned)inW.errorCode, (unsigned)inW.errorCodeOld,
           (unsigned)inW.errAutoRstNum,
           (unsigned)funcCode.code.errAutoRstNumMax,
           (unsigned)inW.checkMenuModeCmd, (unsigned)inW.checkMenuModePara);
    printf("  menuLevel=%d menuMode=%d menuModeTmp=%d menuModeStatus=%d\n",
           (int)inW.menuLevel, (int)inW.menuMode, (int)inW.menuModeTmp,
           (int)inW.menuModeStatus);
    printf("  scanStep=%u keyValue=0x%03X keyStatus=%u keyAge=%u keyFunc=%u\n",
           (unsigned)inSide.scanStep, (unsigned)inSide.keyValue,
           (unsigned)inSide.keyStatus, (unsigned)inSide.keyAge,
           (unsigned)inSide.keyFunc);
    printf("  keyPreFunc=%u keyPreCtrl=%u bKeyEsc=%u bKeyProc=%u\n",
           (unsigned)inSide.keyPreFunc, (unsigned)inSide.keyPreCtrl,
           (unsigned)inSide.bKeyEsc, (unsigned)inSide.bKeyProc);
    printf("  repeatStep=%u repeatNum=%u bFrqDigital=%u bOnShift=%u\n",
           (unsigned)inSide.repeatStep, (unsigned)inSide.repeatNum,
           (unsigned)inW.bFrqDigital, (unsigned)inSide.bOnShift);
    printf("  bEnflash=%u keyChecking=%u keySelStatus=%u keySelType=%u\n",
           (unsigned)inSide.bEnflash, (unsigned)inSide.keyChecking,
           (unsigned)inSide.keySelStatus, (unsigned)inSide.keySelType);
    printf("  RXFFST=%u SPIRXBUF=0x%04X logN %u/%u ovf %u/%u cpu %u/%u\n",
           (unsigned)inW.spi.SPIFFRX.bit.RXFFST,
           (unsigned)inW.spi.SPIRXBUF,
           (unsigned)SH[0].logN, (unsigned)SH[1].logN,
           (unsigned)SH[0].logOverflow, (unsigned)SH[1].logOverflow,
           (unsigned)SH[0].cpu[4], (unsigned)SH[1].cpu[4]);
}

static void cmp(const char *what, const void *a, const void *b, size_t n)
{
    if (memcmp(a, b, n) != 0)
    {
        report(what);
    }
}

static void compare(void)
{
    cmp("spiData",       &S[0].spi, &S[1].spi, sizeof S[0].spi);
    cmp("diHwStatus",    &S[0].di,  &S[1].di,  sizeof S[0].di);
    cmp("displayBuffer", S[0].displayBuffer, S[1].displayBuffer,
        sizeof S[0].displayBuffer);
    cmp("keyValue",      &S[0].keyValue,  &S[1].keyValue,  sizeof S[0].keyValue);
    cmp("keyStatus",     &S[0].keyStatus, &S[1].keyStatus, sizeof S[0].keyStatus);
    cmp("keyAge",        &S[0].keyAge,    &S[1].keyAge,    sizeof S[0].keyAge);
    cmp("bKeyEsc",       &S[0].bKeyEsc,   &S[1].bKeyEsc,   sizeof S[0].bKeyEsc);
    cmp("bKeyProc",      &S[0].bKeyProc,  &S[1].bKeyProc,  sizeof S[0].bKeyProc);
    cmp("keyPreFunc",    &S[0].keyPreFunc, &S[1].keyPreFunc,
        sizeof S[0].keyPreFunc);
    cmp("keyFunc",       &S[0].keyFunc,   &S[1].keyFunc,   sizeof S[0].keyFunc);
    cmp("keyPreCtrl",    &S[0].keyPreCtrl, &S[1].keyPreCtrl,
        sizeof S[0].keyPreCtrl);
    cmp("repeatNum",     &S[0].repeatNum, &S[1].repeatNum, sizeof S[0].repeatNum);
    cmp("repeatStep",    &S[0].repeatStep, &S[1].repeatStep,
        sizeof S[0].repeatStep);
    cmp("bOnShift",      &S[0].bOnShift,  &S[1].bOnShift,  sizeof S[0].bOnShift);
    cmp("upDownDelta",   &S[0].upDownDelta, &S[1].upDownDelta,
        sizeof S[0].upDownDelta);
    cmp("bEnflash",      &S[0].bEnflash,  &S[1].bEnflash,  sizeof S[0].bEnflash);
    cmp("bLight",        S[0].bLight, S[1].bLight, sizeof S[0].bLight);
    cmp("flashTicker",   S[0].flashTicker, S[1].flashTicker,
        sizeof S[0].flashTicker);
    cmp("flashTickerLed", S[0].flashTickerLed, S[1].flashTickerLed,
        sizeof S[0].flashTickerLed);
    cmp("scanStep",      &S[0].scanStep,  &S[1].scanStep,  sizeof S[0].scanStep);
    cmp("keySelStatus",  &S[0].keySelStatus, &S[1].keySelStatus,
        sizeof S[0].keySelStatus);
    cmp("keySelType",    &S[0].keySelType, &S[1].keySelType,
        sizeof S[0].keySelType);
    cmp("keySelTypeBak", &S[0].keySelTypeBak, &S[1].keySelTypeBak,
        sizeof S[0].keySelTypeBak);
    cmp("keyChecking",   &S[0].keyChecking, &S[1].keyChecking,
        sizeof S[0].keyChecking);
    cmp("keyBordTestFlag", &S[0].keyBordTestFlag, &S[1].keyBordTestFlag,
        sizeof S[0].keyBordTestFlag);
    cmp("keyBordValue",  &S[0].keyBordValue, &S[1].keyBordValue,
        sizeof S[0].keyBordValue);
    cmp("onQuickTicker", &S[0].onQuickTicker, &S[1].onQuickTicker,
        sizeof S[0].onQuickTicker);
    cmp("onQuickTickerMax", &S[0].onQuickTickerMax, &S[1].onQuickTickerMax,
        sizeof S[0].onQuickTickerMax);
    cmp("onShiftPassViewTicker", &S[0].onShiftPassViewTicker,
        &S[1].onShiftPassViewTicker, sizeof S[0].onShiftPassViewTicker);
    cmp("onShiftPassViewTickerMax", &S[0].onShiftPassViewTickerMax,
        &S[1].onShiftPassViewTickerMax, sizeof S[0].onShiftPassViewTickerMax);
    cmp("factoryPwd",    &S[0].factoryPwd, &S[1].factoryPwd,
        sizeof S[0].factoryPwd);
    cmp("flashMaxSDSD",  &S[0].flashMaxSDSD, &S[1].flashMaxSDSD,
        sizeof S[0].flashMaxSDSD);

    cmp("SpiaRegs",      &SH[0].spi,   &SH[1].spi,   sizeof SH[0].spi);
    cmp("GpioCtrlRegs",  &SH[0].gctrl, &SH[1].gctrl, sizeof SH[0].gctrl);
    cmp("GpioDataRegs",  &SH[0].gdata, &SH[1].gdata, sizeof SH[0].gdata);
    cmp("cpuCtl",        SH[0].cpu, SH[1].cpu, sizeof SH[0].cpu);
    cmp("funcCode",      &SH[0].funcCode, &SH[1].funcCode,
        sizeof SH[0].funcCode);
    cmp("menuAttri",     SH[0].menuAttri, SH[1].menuAttri,
        sizeof SH[0].menuAttri);
    cmp("runFlag",       &SH[0].runFlag, &SH[1].runFlag, sizeof SH[0].runFlag);
    cmp("runMode",       &SH[0].runMode, &SH[1].runMode, sizeof SH[0].runMode);
    cmp("runSrc",        &SH[0].runSrc,  &SH[1].runSrc,  sizeof SH[0].runSrc);
    cmp("tuneCmd",       &SH[0].tuneCmd, &SH[1].tuneCmd, sizeof SH[0].tuneCmd);
    cmp("errorCode",     &SH[0].errorCode, &SH[1].errorCode,
        sizeof SH[0].errorCode);
    cmp("errorCodeOld",  &SH[0].errorCodeOld, &SH[1].errorCodeOld,
        sizeof SH[0].errorCodeOld);
    cmp("errAutoRstNum", &SH[0].errAutoRstNum, &SH[1].errAutoRstNum,
        sizeof SH[0].errAutoRstNum);
    cmp("accDecFrqPrcFlag", &SH[0].accDecFrqPrcFlag, &SH[1].accDecFrqPrcFlag,
        sizeof SH[0].accDecFrqPrcFlag);
    cmp("bFrqDigital",   &SH[0].bFrqDigital, &SH[1].bFrqDigital,
        sizeof SH[0].bFrqDigital);
    cmp("checkMenuModeCmd", &SH[0].checkMenuModeCmd, &SH[1].checkMenuModeCmd,
        sizeof SH[0].checkMenuModeCmd);
    cmp("checkMenuModePara", &SH[0].checkMenuModePara, &SH[1].checkMenuModePara,
        sizeof SH[0].checkMenuModePara);

    cmp("uiLog",     SH[0].log, SH[1].log, sizeof SH[0].log);
    cmp("uiLogN",    &SH[0].logN, &SH[1].logN, sizeof SH[0].logN);
    cmp("uiLogOverflow", &SH[0].logOverflow, &SH[1].logOverflow,
        sizeof SH[0].logOverflow);

    cmp("DISPLAY_CODE", ref_DISPLAY_CODE, new_DISPLAY_CODE,
        sizeof ref_DISPLAY_CODE);
    cmp("LED_CODE",     ref_LED_CODE, new_LED_CODE, sizeof ref_LED_CODE);
    cmp("dispCodeMenuMode", ref_dispCodeMenuMode, new_dispCodeMenuMode,
        sizeof ref_dispCodeMenuMode);
    cmp("minRate",      ref_minRate, new_minRate, sizeof ref_minRate);
    cmp("repeatNumMax", ref_repeatNumMax, new_repeatNumMax,
        sizeof ref_repeatNumMax);
}

/* ---- runner -------------------------------------------------------------- */

typedef void (*void_fn)(void);

#define FN_N 10

static void_fn g_refFn[FN_N];
static void_fn g_newFn[FN_N];
static const char *g_fnName[FN_N] = {
    "UpdateDisplayBuffer", "WinkDeal", "DisplayScanPrepare", "DisplayScan",
    "KeyScan", "KeyBordTest", "KeyProcess", "InitSpiaGpio",
    "InitSetSpiForFunc", "UpdateMenuModeDisplayBuffer"
};

static void init_ptrs(void)
{
    g_refFn[0] = ref_UpdateDisplayBuffer;        g_newFn[0] = new_UpdateDisplayBuffer;
    g_refFn[1] = ref_WinkDeal;                   g_newFn[1] = new_WinkDeal;
    g_refFn[2] = ref_DisplayScanPrepare;         g_newFn[2] = new_DisplayScanPrepare;
    g_refFn[3] = ref_DisplayScan;                g_newFn[3] = new_DisplayScan;
    g_refFn[4] = ref_KeyScan;                    g_newFn[4] = new_KeyScan;
    g_refFn[5] = ref_KeyBordTest;                g_newFn[5] = new_KeyBordTest;
    g_refFn[6] = ref_KeyProcess;                 g_newFn[6] = new_KeyProcess;
    g_refFn[7] = ref_InitSpiaGpio;               g_newFn[7] = new_InitSpiaGpio;
    g_refFn[8] = ref_InitSetSpiForFunc;          g_newFn[8] = new_InitSetSpiForFunc;
    g_refFn[9] = ref_UpdateMenuModeDisplayBuffer; g_newFn[9] = new_UpdateMenuModeDisplayBuffer;
}

/* One lockstep step: install the world, run ref, snapshot; install the SAME
 * world again, run new, snapshot; compare. Module-private state is NOT
 * reinstalled, so a sequence of steps carries it forward on both sides at once.
 */
static void step(int which)
{
    install_world();
    g_refFn[which]();
    snapshot(0);
    snap_world(0);

    install_world();
    g_newFn[which]();
    snapshot(1);
    snap_world(1);

    g_cases++;
    compare();
}

/* A one-shot call: module state is reinstalled from the input image first. */
static void fresh(int which)
{
    install_side(0);
    install_side(1);
    step(which);
}

/* ---- deterministic input fill -------------------------------------------- */

static unsigned long g_rng = 0x20261009UL;

static unsigned long rnd(void)
{
    g_rng = g_rng * 1103515245UL + 12345UL;
    g_rng ^= g_rng >> 7;
    g_rng *= 2654435761UL;
    return (g_rng >> 8) & 0xFFFFFFUL;
}

static void rnd_fill(void *p, size_t n)
{
    unsigned char *b = (unsigned char *)p;
    size_t i;

    for (i = 0; i < n; i++)
    {
        b[i] = (unsigned char)(rnd() >> 4);
    }
}

/* Non-trivial prefill for every module-owned input. The reason is the one
 * stated in the bench README: ref-vs-new only ever sees a change on the NEW
 * side, so "the rewrite stopped writing this field" is visible only if the
 * position already held something other than what correct code writes.
 */
static void side_begin(unsigned long seed)
{
    g_rng = seed ? seed : 1UL;

    rnd_fill(&inSide.spi, sizeof inSide.spi);
    rnd_fill(&inSide.di,  sizeof inSide.di);
    rnd_fill(inSide.displayBuffer, sizeof inSide.displayBuffer);
    rnd_fill(inSide.bLight, sizeof inSide.bLight);
    rnd_fill(inSide.flashTicker, sizeof inSide.flashTicker);
    rnd_fill(inSide.flashTickerLed, sizeof inSide.flashTickerLed);

    inSide.keyValue      = (Uint16)(rnd() & 0x1FFU);
    inSide.keyStatus     = (Uint16)(rnd() % 4);
    inSide.keyAge        = (Uint16)(rnd() % 256);
    inSide.bKeyEsc       = (Uint16)(rnd() % 2);
    inSide.bKeyProc      = (Uint16)(rnd() % 2);
    inSide.keyPreFunc    = (Uint16)(rnd() % 10);
    inSide.keyFunc       = (Uint16)(rnd() % 10);
    inSide.keyPreCtrl    = (Uint16)(rnd() % 2);
    inSide.repeatNum     = (Uint16)(rnd() % 16);
    /* repeatStep indexes minRate[] / repeatNumMax[]; keep it inside them. */
    inSide.repeatStep    = (Uint16)(rnd() % FUI_MIN_RATE_N);
    inSide.bOnShift      = (Uint16)(rnd() % 2);
    inSide.upDownDelta   = (Uint16)(rnd() % 64);
    inSide.bEnflash      = (Uint16)(rnd() % 2);
    /* scanStep indexes displayBuffer[] and drives `5 - scanStep`. */
    inSide.scanStep      = (Uint16)(rnd() % (DISPLAY_8LED_NUM + 1));
    inSide.keySelStatus  = (Uint16)(rnd() % 2);
    inSide.keySelType    = (Uint16)(rnd() % 2);
    inSide.keySelTypeBak = (Uint16)(rnd() % 2);
    inSide.keyChecking   = (Uint16)(rnd() % 2);
    inSide.keyBordTestFlag = (Uint16)(rnd() % 2);
    inSide.keyBordValue  = (Uint16)rnd();
    inSide.onQuickTicker = (Uint16)(rnd() % 2048);
    inSide.onQuickTickerMax = (Uint16)(1 + rnd() % 2048);
    inSide.onShiftPassViewTicker = (Uint16)(rnd() % 2048);
    inSide.onShiftPassViewTickerMax = (Uint16)(1 + rnd() % 2048);
    inSide.factoryPwd    = (Uint16)rnd();
    inSide.flashMaxSDSD  = (Uint16)(1 + rnd() % 64);
}

static void world_begin(unsigned long seed)
{
    int i;

    g_rng = seed ? seed : 1UL;

    rnd_fill(&inW.spi,    sizeof inW.spi);
    rnd_fill(&inW.gctrl,  sizeof inW.gctrl);
    rnd_fill(&inW.gdata,  sizeof inW.gdata);
    rnd_fill(&inW.funcCode, sizeof inW.funcCode);

    for (i = 0; i < MENU_LEVEL_NUM; i++)
    {
        inW.menuAttri[i].operateDigit = (Uint16)rnd();
        inW.menuAttri[i].winkFlag     = (Uint16)rnd();
        inW.menuAttri[i].winkFlagLed  = (Uint16)rnd();
    }

    inW.menuLevel      = (enum MENU_LEVEL)(rnd() % MENU_LEVEL_NUM);
    inW.menuMode       = (enum MENU_MODE)(rnd() % 4);
    /* 1..MENU_LEVEL_NUM: see host-unrepresentable region (1) in the header. */
    inW.menuModeTmp    = (enum MENU_MODE)(1 + rnd() % MENU_LEVEL_NUM);
    inW.menuModeStatus = (enum MENU_MODE_OPERATE)(rnd() % 2);

    inW.runFlag.all = (Uint16)rnd();
    inW.runMode     = (Uint16)(rnd() % 4);
    inW.runSrc      = (Uint16)(rnd() % 6);
    inW.tuneCmd     = (Uint16)(rnd() % 2);

    inW.errorCode     = (Uint16)(rnd() % 2);
    inW.errorCodeOld  = (Uint16)(rnd() % 2);
    inW.errAutoRstNum = (Uint16)(rnd() % 6);

    inW.accDecFrqPrcFlag = (Uint16)rnd();
    inW.bFrqDigital      = (Uint16)(rnd() % 2);

    inW.checkMenuModeCmd  = (Uint16)(rnd() % 2);
    inW.checkMenuModePara = (Uint16)(rnd() % 8);
}

static void case_begin(unsigned long seed)
{
    side_begin(seed);
    world_begin(seed ^ 0x5A5A5A5AUL);
}

/* ---- 1. UpdateDisplayBuffer ----------------------------------------------
 *
 * The body is a chain of independent one-bit tests, each of which either clears
 * or sets a single LED bit in displayBuffer[5], plus two gated calls and the
 * `!scanStep` block that dispatches to the menu's own UpdateDisplayBuffer.
 * Every one of those bits is swept both ways, and the error-reset triple is
 * swept over its three-way disjunction.
 */

static void menuAttri_set(int level, Uint16 winkFlag, Uint16 winkFlagLed);

static void phase_display_buffer(void)
{
    static const Uint16 runsrces[4] = {
        0, FUNCCODE_runSrc_DI, FUNCCODE_runSrc_COMM, FUNCCODE_runSrc_AUTO_RUN
    };
    static const Uint16 rstnum[3] = { 0, 3, 5 };
    static const Uint16 rstmax[3] = { 0, 3, 7 };
    int run, rs, dr, dir, tc, ec, rm, ck, eo, ss, ms, a, b;

    g_cur = g_fnName[0];

    for (run = 0; run < 2; run++)
    for (rs = 0; rs < 4; rs++)
    for (dr = 0; dr < 2; dr++)
    for (dir = 0; dir < 2; dir++)
    for (tc = 0; tc < 2; tc++)
    for (ec = 0; ec < 2; ec++)
    for (rm = 0; rm < 2; rm++)
    for (ck = 0; ck < 2; ck++)
    for (eo = 0; eo < 2; eo++)
    for (ss = 0; ss < 2; ss++)
    for (ms = 0; ms < 2; ms++)
    {
        case_begin(0x1000UL + (unsigned long)g_cases);

        inW.runFlag.all = 0;
        inW.runFlag.bit.run          = (Uint16)run;
        inW.runFlag.bit.dirReversing = (Uint16)dr;
        inW.runFlag.bit.dir          = (Uint16)dir;
        inW.runSrc    = runsrces[rs];
        inW.tuneCmd   = (Uint16)tc;
        inW.errorCode = (Uint16)ec;
        inW.runMode   = (Uint16)(rm ? RUN_MODE_TORQUE_CTRL : 0);
        inW.checkMenuModeCmd = (Uint16)ck;
        inW.errorCodeOld     = (Uint16)eo;
        inSide.scanStep      = (Uint16)ss;
        inW.menuModeStatus   = (enum MENU_MODE_OPERATE)(ms ? MENU_MODE_ON_QUICK
                                                          : MENU_MODE_NONE);

        fresh(0);
    }

    /* The error-reset triple, swept on its own: the condition is a disjunction
     * of three terms and the interesting state is where exactly one holds. */
    for (a = 0; a < 3; a++)
    for (b = 0; b < 3; b++)
    for (eo = 0; eo < 2; eo++)
    for (ec = 0; ec < 2; ec++)
    {
        case_begin(0x2000UL + (unsigned long)g_cases);

        inW.errorCodeOld = (Uint16)eo;
        inW.errorCode    = (Uint16)ec;
        inW.errAutoRstNum = rstnum[a];
        funcCode.code.errAutoRstNumMax = rstmax[b];
        inW.runFlag.all = 0;
        inSide.scanStep = 1;

        fresh(0);
    }
}

/* ---- 2. WinkDeal ---------------------------------------------------------
 *
 * Gated at the top on `keyChecking == 1` (returns immediately), then splits on
 * `scanStep < DISPLAY_8LED_NUM`. Below the split the 8-LED flash ticker runs
 * against flashTickerMax; at and above it the LED ticker runs against
 * flashTickerLedMax, or against flashMaxSDSD for the error indication.
 *
 * The tickers need 42 / 45 / flashMaxSDSD consecutive calls to reach a
 * threshold, so those are sequences, not samples.
 */
static void phase_wink_deal(void)
{
    static const Uint16 wf[6] = { 0, 0x1F, 0x0A, 0x15, 0x20, 0xFF };
    static const Uint16 wl[6] = { 0, 0x7F, 0x55, 0x2A, 0x40, 0x01 };
    int kc, a, b, ss, ml, be, bo, ec, i;

    g_cur = g_fnName[1];

    for (kc = 0; kc < 2; kc++)
    for (a = 0; a < 6; a++)
    for (b = 0; b < 6; b++)
    for (ss = 0; ss <= DISPLAY_8LED_NUM; ss++)
    for (ml = 0; ml < MENU_LEVEL_NUM; ml++)
    for (be = 0; be < 2; be++)
    for (bo = 0; bo < 2; bo++)
    for (ec = 0; ec < 2; ec++)
    {
        case_begin(0x3000UL + (unsigned long)g_cases);

        inSide.keyChecking = (Uint16)kc;
        inSide.scanStep    = (Uint16)ss;
        inSide.bEnflash    = (Uint16)be;
        inSide.bOnShift    = (Uint16)bo;
        inW.errorCode      = (Uint16)ec;
        inW.menuLevel      = (enum MENU_LEVEL)ml;
        menuAttri_set(ml, wf[a], wl[b]);

        fresh(1);
    }

    /* Sequence: hold the inputs still so the statics settle into the "unchanged"
     * arm, then let the tickers run past their thresholds.
     *
     * The order matters and is the whole point of this phase: `case_begin()`
     * re-randomises `inSide` on every iteration, so `install_side()` has to run
     * AFTER the inputs are pinned. Install first and the module is handed the
     * random prefill instead of scanStep == ss, `bEnflash == FLASH_YES`; the
     * increment guard then almost never fires and the ticker walks are never
     * exercised at all (measured: flashTicker stayed at its prefill value). */
    for (ss = 0; ss <= DISPLAY_8LED_NUM; ss++)
    {
        case_begin(0x3100UL + (unsigned long)ss);

        inSide.scanStep = (Uint16)ss;
        inSide.keyChecking = 0;
        inSide.bEnflash = FLASH_YES;
        inSide.bOnShift = 0;
        inW.errorCode = 0;
        inW.menuLevel = MENU_LEVEL_0;
        menuAttri_set(MENU_LEVEL_0, 0x1FU, 0x7FU);

        install_side(0);
        install_side(1);

        for (i = 0; i < 3 * FUI_FLASH_TICKER_LED_MAX; i++)
        {
            step(1);
        }
    }

    /* Sequence with the error indication on, so flashMax = flashMaxSDSD and the
     * LED path takes its other arm. Same ordering rule as above. */
    for (i = 0; i < 3 * FUI_FLASH_TICKER_LED_MAX; i++)
    {
        if (i == 0)
        {
            case_begin(0x3200UL);

            inSide.scanStep = DISPLAY_8LED_NUM;
            inSide.keyChecking = 0;
            inSide.bEnflash = FLASH_YES;
            inSide.bOnShift = 0;
            inSide.flashMaxSDSD = 10;
            inW.errorCode = 1;
            inW.menuLevel = MENU_LEVEL_0;
            menuAttri_set(MENU_LEVEL_0, 0x1FU, 0x7FU);

            install_side(0);
            install_side(1);
        }
        step(1);
    }
}

/* ---- 3. DisplayScanPrepare ----------------------------------------------
 *
 * The tcnt counter runs 0..KEY_SEL_TEST_TIME and only then wraps; the
 * keySelType edge detector sits on the far side of that wrap. Also the
 * `!keyChecking` guard on the scanStep advance, and the `RXFFST` loop.
 */
static void phase_scan_prepare(void)
{
    static const Uint16 rxffst[8] = { 0, 1, 2, 3, 7, 15, 16, 31 };
    static const Uint16 keys[6] = { 0x0000, 0x0080, 0x00FF, 0x7F80, 0x8080, 0xFFFF };
    int i, j, k;

    g_cur = g_fnName[2];

    /* Single calls across the whole input space, including the keySelStatus /
     * keySelType combinations and the keyChecking guard. */
    for (i = 0; i < 8; i++)
    for (j = 0; j < 6; j++)
    for (k = 0; k < 4; k++)
    {
        case_begin(0x4000UL + (unsigned long)g_cases);
        inW.spi.SPIFFRX.bit.RXFFST = rxffst[i];
        inW.spi.SPIRXBUF = keys[j];
        inSide.keySelStatus = (Uint16)(k & 1);
        inSide.keySelType   = (Uint16)((k >> 1) & 1);
        inSide.keyChecking  = (Uint16)(g_cases & 1);
        fresh(2);
    }

    /* Sequence: walk tcnt through a full KEY_SEL_TEST_TIME period, with the key
     * input toggling so the edge detector fires. `install_side()` again has to
     * run after the inputs are pinned -- `case_begin()` re-randomises `inSide`,
     * so installing first throws away keySelStatus / keySelType / keyChecking. */
    for (k = 0; k < 4; k++)
    {
        case_begin(0x4100UL + (unsigned long)k);

        inSide.keySelStatus = 0;
        inSide.keySelType   = 0;
        inSide.keyChecking  = 0;
        inW.spi.SPIFFRX.bit.RXFFST = (Uint16)(1 + (k & 3));
        inW.spi.SPIRXBUF = 0x8080;

        install_side(0);
        install_side(1);

        for (i = 0; i < FUI_KEY_SEL_TEST_TIME + 40; i++)
        {
            /* Toggle the received key word so that keySelType flips and the
             * keySelType == 1 && keySelTypeBak == 0 edge is exercised. */
            inW.spi.SPIRXBUF = ((i / 20) & 1) ? 0x0000 : 0x8080;
            step(2);
        }
    }
}

/* ---- 4. DisplayScan ------------------------------------------------------
 *
 * Three writes: RCK, KEYSEL and SPITXBUF. Small, but it is the only place RCK
 * and KEYSEL are driven, and it is the only writer of SPITXBUF.
 */
static void phase_display_scan(void)
{
    int kt, i;

    g_cur = g_fnName[3];

    for (kt = 0; kt < 2; kt++)
    for (i = 0; i < 8; i++)
    {
        case_begin(0x5000UL + (unsigned long)g_cases);
        inSide.keySelType = (Uint16)kt;
        inSide.spi.out.a.all = (Uint16)(0x1111U * (unsigned)i);
        inSide.spi.out.b.all = (Uint16)rnd();
        fresh(3);
    }
}

/* ---- 5. KeyScan ----------------------------------------------------------
 *
 * The decode is `keyValueTmp = ((~keyValue) & 511) << 1`, then a downward scan
 * over bits 9..1 that leaves funcVal holding the LOWEST set bit. So:
 *
 *   P5a  the whole 9-bit input space (512 patterns) with neutral state, which
 *        covers funcCnt = 0..9 and every funcVal;
 *   P5b  the state machine: keyStatus x the keyAge thresholds x repeatStep x
 *        bFrqDigital x repeatNum x the bKeyEsc / keyPreCtrl arms, all with a
 *        single-key input so the branch under test is actually entered;
 *   P5c  random.
 */
static void phase_key_scan(void)
{
    static const Uint16 ages[16] = {
        0, 1, 19, 20, 21, 49, 50, 79, 80, 99, 100, 101, 129, 130, 199, 200
    };
    static const Uint16 rnums[5] = { 0, 8, 9, 10, 11 };
    int kv, ks, ai, rst, bd, rn, ke, kc, i;

    g_cur = g_fnName[4];

    /* P5a: the full decode space. keyValue bit n -> keyValueTmp bit n+1, so
     * funcVal = n+1 and funcCnt = popcount. */
    for (kv = 0; kv < 512; kv++)
    {
        case_begin(0x6000UL + (unsigned long)kv);
        inSide.keyValue    = (Uint16)kv;
        inSide.keyStatus   = KEY_STATUS_JITTER_DEAL;
        inSide.keyAge      = 0;
        inSide.bKeyEsc     = 0;
        inSide.bKeyProc    = 0;
        inSide.keyPreFunc  = 0;
        inSide.keyPreCtrl  = 0;
        inSide.repeatStep  = 0;
        inSide.repeatNum   = 0;
        inW.bFrqDigital    = 0;
        fresh(4);
    }

    /* P5b: the state machine.
     *
     * `kc` picks the (keyPreFunc, keyPreCtrl) pair, and that pair decides whether
     * control reaches the `switch (keyStatus)` block AT ALL. The guard chain is
     *   funcCnt == 1, ctrlCnt <= 1, bKeyEsc == 0,
     *   funcVal == keyPreFunc, and NOT (keyPreCtrl == 0 && ctrlVal != 0),
     * and ctrlVal is always 0 here because ctrlKeyFlag is 0 (the ctrl arm can
     * never match). So reaching the switch needs exactly
     *   keyPreFunc == funcVal  AND  keyPreCtrl == 0  AND  bKeyEsc == 0.
     * kc == 0/1/2 deliberately miss that (keyPreFunc != funcVal, or
     * keyPreCtrl != 0) and take the "a different key arrived" arm, which resets
     * keyAge and returns. kc == 3 is the matching one. Without it the whole
     * switch is unreachable input space and the four keyAge thresholds plus both
     * repeat counters are invisible -- measured: FU34..FU40 (the `>= -> >` family
     * and the repeat ladder) were all MISSED until this arm was added.
     *
     * Getting `funcVal` right needs care, and the first version of this phase got
     * it backwards. The module computes
     *     keyValueTmp = ((~keyValue) & ((1 << KEY_NUM) - 1)) << 1;
     * so bit j of keyValueTmp -- and therefore `funcVal == j` -- corresponds to
     * bit (j-1) of keyValue being ZERO. "Key k pressed, nothing else" is thus
     * all-ones-except-bit-(k-1), i.e. KEYV_ONE(k) below, NOT `1 << (k-1)`.
     * Writing `1U << (KEY_UP - 1)` set eight bits of keyValueTmp instead of one:
     * funcCnt came out 8, the `funcCnt > 1` arm set bKeyEsc and returned, and the
     * switch stayed unreachable -- which is exactly why the fix has to be stated
     * in terms of the quantity the module actually compares (funcVal), not the
     * raw input (keyValue). */
    for (ks = 0; ks < 4; ks++)
    for (ai = 0; ai < 16; ai++)
    for (rst = 0; rst < FUI_MIN_RATE_N; rst++)
    for (bd = 0; bd < 2; bd++)
    for (rn = 0; rn < 5; rn++)
    for (ke = 0; ke < 2; ke++)
    for (kc = 0; kc < 4; kc++)
    {
        case_begin(0x6100UL + (unsigned long)g_cases);
        inSide.keyValue    = KEYV_ONE(KEY_UP);
        inSide.keyStatus   = (Uint16)ks;
        inSide.keyAge      = ages[ai];
        inSide.repeatStep  = (Uint16)rst;
        inSide.repeatNum   = rnums[rn];
        inW.bFrqDigital    = (Uint16)bd;
        inSide.bKeyEsc     = (Uint16)ke;
        inSide.keyPreCtrl  = (Uint16)(kc == 1 ? 1 : (kc == 2 ? 7 : 0));
        inSide.keyPreFunc  = (Uint16)((kc == 2) || (kc == 3) ? KEY_UP : 0);
        inSide.upDownDelta = 1;
        fresh(4);
    }

    /* P5b2: a key NOT in continueKeyFlag, so the JITTER_DEAL arm takes bKeyEsc=1
     * instead of moving to WAIT_REPEAT. KEY_PRG is the first key number. */
    for (ai = 0; ai < 16; ai++)
    {
        case_begin(0x6200UL + (unsigned long)ai);
        inSide.keyValue   = KEYV_ONE(KEY_PRG);
        inSide.keyStatus  = KEY_STATUS_JITTER_DEAL;
        inSide.keyAge     = ages[ai];
        inSide.bKeyEsc    = 0;
        inSide.keyPreFunc = 0;
        inSide.keyPreCtrl = 0;
        inW.bFrqDigital   = 0;
        fresh(4);
    }

    /* P5c: random. */
    for (i = 0; i < 4000; i++)
    {
        case_begin(0x6300UL + (unsigned long)i);
        fresh(4);
    }
}

/* ---- 6. KeyBordTest ------------------------------------------------------ */
static void phase_key_bord_test(void)
{
    int f, t, pf;

    g_cur = g_fnName[5];

    for (f = 0; f < 2; f++)
    for (t = 0; t < 2; t++)
    for (pf = 0; pf < 10; pf++)
    {
        case_begin(0x7000UL + (unsigned long)g_cases);
        inSide.keyBordTestFlag = (Uint16)f;
        inSide.keyBordValue    = (Uint16)rnd();
        inSide.keyPreFunc      = (Uint16)pf;
        fresh(5);
    }
}

/* ---- 7. KeyProcess -------------------------------------------------------
 *
 * Calls KeyBordTest first (so keyBordValue is part of the observable state),
 * then the `bKeyProc == 0` early return with its two tickers, then the
 * on-quick guard, then the ten-way switch that dispatches into menu[].
 */
static void phase_key_process(void)
{
    static const Uint16 kfs[12] = {
        0, KEY_PRG, KEY_UP, KEY_ENTER, KEY_MFK, KEY_DOWN,
        KEY_SHIFT, KEY_RUN, KEY_STOP, KEY_QUICK, KEY_REV_JOG, KEY_FWD_JOG
    };
    static const Uint16 tick[5] = { 0, 1, 999, 1000, 1001 };
    int bp, ms, kf, ti, ml, kt, i;

    g_cur = g_fnName[6];

    for (bp = 0; bp < 2; bp++)
    for (ms = 0; ms < 2; ms++)
    for (kf = 0; kf < 12; kf++)
    for (ti = 0; ti < 5; ti++)
    for (ml = 0; ml < MENU_LEVEL_NUM; ml++)
    for (kt = 0; kt < 2; kt++)
    {
        case_begin(0x8000UL + (unsigned long)g_cases);
        inSide.bKeyProc = (Uint16)bp;
        inW.menuModeStatus = (enum MENU_MODE_OPERATE)(ms ? MENU_MODE_ON_QUICK
                                                          : MENU_MODE_NONE);
        inSide.keyPreFunc = kfs[kf];
        inSide.keyFunc    = (Uint16)(kt ? KEY_REV_JOG : KEY_FWD_JOG);
        inSide.onQuickTicker = tick[ti];
        inSide.onQuickTickerMax = 1000;
        inW.menuLevel = (enum MENU_LEVEL)ml;
        inW.menuMode  = (enum MENU_MODE)(rnd() % 4);
        inSide.keyBordTestFlag = (Uint16)(g_cases & 1);
        fresh(6);
    }

    for (i = 0; i < 2000; i++)
    {
        case_begin(0x8100UL + (unsigned long)i);
        fresh(6);
    }
}

/* ---- 8/9. the two init functions ---------------------------------------- */

static void phase_init_spia_gpio(void)
{
    int i;

    g_cur = g_fnName[7];

    for (i = 0; i < 16; i++)
    {
        case_begin(0x9000UL + (unsigned long)i);
        fresh(7);
    }
}

static void phase_init_spi_func(void)
{
    int i;

    g_cur = g_fnName[8];

    for (i = 0; i < 16; i++)
    {
        case_begin(0x9800UL + (unsigned long)i);
        fresh(8);
    }
}

/* ---- 10. UpdateMenuModeDisplayBuffer ------------------------------------
 *
 * `menuModeTmp - 1` indexes dispCodeMenuMode; see host-unrepresentable region
 * (1). Every row the table actually has is walked, plus the rows that are only
 * zero-initialised, so the DISPLAY_CODE indirection is covered at both ends.
 */
static void phase_menu_mode_display(void)
{
    int mt, i;

    g_cur = g_fnName[9];

    for (mt = 1; mt <= MENU_LEVEL_NUM; mt++)
    {
        case_begin(0xA000UL + (unsigned long)mt);
        inW.menuModeTmp = (enum MENU_MODE)mt;
        fresh(9);
    }

    for (i = 0; i < 64; i++)
    {
        case_begin(0xA100UL + (unsigned long)i);
        fresh(9);
    }
}

/* ---- random sweep over everything --------------------------------------- */
static void phase_random(long n)
{
    long i;

    for (i = 0; i < n; i++)
    {
        case_begin(0xB0000000UL + (unsigned long)i);
        fresh((int)(i % FN_N));
    }
}

/* ---- helpers used by the phases ----------------------------------------- */

static void menuAttri_set(int level, Uint16 winkFlag, Uint16 winkFlagLed)
{
    inW.menuLevel = (enum MENU_LEVEL)level;
    inW.menuAttri[level].winkFlag    = winkFlag;
    inW.menuAttri[level].winkFlagLed = winkFlagLed;
}

/* ---- main ---------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long nrand = 20000;

    if (argc > 1)
    {
        nrand = strtol(argv[1], NULL, 10);
        if (nrand < 0)
        {
            nrand = 0;
        }
    }

    init_ptrs();

    phase_display_buffer();
    phase_wink_deal();
    phase_scan_prepare();
    phase_display_scan();
    phase_key_scan();
    phase_key_bord_test();
    phase_key_process();
    phase_init_spia_gpio();
    phase_init_spi_func();
    phase_menu_mode_display();
    phase_random(nrand);

    printf("f_ui difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    return g_fails ? 1 : 0;
}
