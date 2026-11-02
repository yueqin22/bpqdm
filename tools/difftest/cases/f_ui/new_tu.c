/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define DISPLAY_CODE                new_DISPLAY_CODE
#define DisplayScan                 new_DisplayScan
#define DisplayScanPrepare          new_DisplayScanPrepare
#define InitSetSpiForFunc           new_InitSetSpiForFunc
#define InitSpiaGpio                new_InitSpiaGpio
#define KeyBordTest                 new_KeyBordTest
#define KeyProcess                  new_KeyProcess
#define KeyScan                     new_KeyScan
#define LED_CODE                    new_LED_CODE
#define UpdateDisplayBuffer         new_UpdateDisplayBuffer
#define UpdateMenuModeDisplayBuffer new_UpdateMenuModeDisplayBuffer
#define WinkDeal                    new_WinkDeal
#define bEnflash                    new_bEnflash
#define bKeyEsc                     new_bKeyEsc
#define bKeyProc                    new_bKeyProc
#define bLight                      new_bLight
#define bOnShift                    new_bOnShift
#define diHwStatus                  new_diHwStatus
#define dispCodeMenuMode            new_dispCodeMenuMode
#define displayBuffer               new_displayBuffer
#define factoryPwd                  new_factoryPwd
#define flashMaxSDSD                new_flashMaxSDSD
#define flashTicker                 new_flashTicker
#define flashTickerLed              new_flashTickerLed
#define keyAge                      new_keyAge
#define keyBordTestFlag             new_keyBordTestFlag
#define keyBordValue                new_keyBordValue
#define keyChecking                 new_keyChecking
#define keyFunc                     new_keyFunc
#define keyPreCtrl                  new_keyPreCtrl
#define keyPreFunc                  new_keyPreFunc
#define keySelStatus                new_keySelStatus
#define keySelType                  new_keySelType
#define keySelTypeBak               new_keySelTypeBak
#define keyStatus                   new_keyStatus
#define keyValue                    new_keyValue
#define minRate                     new_minRate
#define onQuickTicker               new_onQuickTicker
#define onQuickTickerMax            new_onQuickTickerMax
#define onShiftPassViewTicker       new_onShiftPassViewTicker
#define onShiftPassViewTickerMax    new_onShiftPassViewTickerMax
#define repeatNum                   new_repeatNum
#define repeatNumMax                new_repeatNumMax
#define repeatStep                  new_repeatStep
#define scanStep                    new_scanStep
#define spiData                     new_spiData
#define upDownDelta                 new_upDownDelta

#include "f_ui.c"
