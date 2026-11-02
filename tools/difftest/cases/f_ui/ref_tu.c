/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define DISPLAY_CODE                ref_DISPLAY_CODE
#define DisplayScan                 ref_DisplayScan
#define DisplayScanPrepare          ref_DisplayScanPrepare
#define InitSetSpiForFunc           ref_InitSetSpiForFunc
#define InitSpiaGpio                ref_InitSpiaGpio
#define KeyBordTest                 ref_KeyBordTest
#define KeyProcess                  ref_KeyProcess
#define KeyScan                     ref_KeyScan
#define LED_CODE                    ref_LED_CODE
#define UpdateDisplayBuffer         ref_UpdateDisplayBuffer
#define UpdateMenuModeDisplayBuffer ref_UpdateMenuModeDisplayBuffer
#define WinkDeal                    ref_WinkDeal
#define bEnflash                    ref_bEnflash
#define bKeyEsc                     ref_bKeyEsc
#define bKeyProc                    ref_bKeyProc
#define bLight                      ref_bLight
#define bOnShift                    ref_bOnShift
#define diHwStatus                  ref_diHwStatus
#define dispCodeMenuMode            ref_dispCodeMenuMode
#define displayBuffer               ref_displayBuffer
#define factoryPwd                  ref_factoryPwd
#define flashMaxSDSD                ref_flashMaxSDSD
#define flashTicker                 ref_flashTicker
#define flashTickerLed              ref_flashTickerLed
#define keyAge                      ref_keyAge
#define keyBordTestFlag             ref_keyBordTestFlag
#define keyBordValue                ref_keyBordValue
#define keyChecking                 ref_keyChecking
#define keyFunc                     ref_keyFunc
#define keyPreCtrl                  ref_keyPreCtrl
#define keyPreFunc                  ref_keyPreFunc
#define keySelStatus                ref_keySelStatus
#define keySelType                  ref_keySelType
#define keySelTypeBak               ref_keySelTypeBak
#define keyStatus                   ref_keyStatus
#define keyValue                    ref_keyValue
#define minRate                     ref_minRate
#define onQuickTicker               ref_onQuickTicker
#define onQuickTickerMax            ref_onQuickTickerMax
#define onShiftPassViewTicker       ref_onShiftPassViewTicker
#define onShiftPassViewTickerMax    ref_onShiftPassViewTickerMax
#define repeatNum                   ref_repeatNum
#define repeatNumMax                ref_repeatNumMax
#define repeatStep                  ref_repeatStep
#define scanStep                    ref_scanStep
#define spiData                     ref_spiData
#define upDownDelta                 ref_upDownDelta

#include "f_ui_ref.c"
