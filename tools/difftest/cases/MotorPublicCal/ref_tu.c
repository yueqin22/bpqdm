/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define BrakeResControl     ref_BrakeResControl
#define CBCLimitCurPrepare  ref_CBCLimitCurPrepare
#define HardWareErrorDeal   ref_HardWareErrorDeal
#define HardWareOverUDCDeal ref_HardWareOverUDCDeal
#define MaxUVWCurrent       ref_MaxUVWCurrent
#define RunCaseDcBrake      ref_RunCaseDcBrake
#define RunCaseLowPower     ref_RunCaseLowPower
#define RunCaseShortGnd     ref_RunCaseShortGnd
#define gBrake              ref_gBrake
#define gDCBrake            ref_gDCBrake
#define gLowPower           ref_gLowPower
#define gShortGnd           ref_gShortGnd

#include "MotorPublicCal_ref.c"
