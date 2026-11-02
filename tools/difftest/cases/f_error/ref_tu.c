/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 *
 * include 的是 host_patch.json 生成的**宿主副本**, 不是 ref/ 下的原文件:
 * getMotorError() 的循环上界写成 sizeof(errorCodeFromMotorList), 目标端是 19
 * (元素个数), 宿主是 38 (字节数), 会读到表外。补丁换成
 * sizeof(...)/sizeof(Uint16), 两边都是 19 (详见 host_patch.json 的 why)。
 * 宿主副本放在 gen_dir 里, 而 gen_dir 排在 include 路径最前面。
 */

#define AutoResetErrorDeal      ref_AutoResetErrorDeal
#define AutoResetErrorFcDeal    ref_AutoResetErrorFcDeal
#define AutoResetErrorTrueDeal  ref_AutoResetErrorTrueDeal
#define ErrorDeal               ref_ErrorDeal
#define ErrorReset              ref_ErrorReset
#define ErrorSceneSave          ref_ErrorSceneSave
#define GetErrorAttribute       ref_GetErrorAttribute
#define GetErrorResetDeal       ref_GetErrorResetDeal
#define GetNewError             ref_GetNewError
#define GetNewErrorAttribute    ref_GetNewErrorAttribute
#define UpdateError             ref_UpdateError
#define bUv                     ref_bUv
#define curTime                 ref_curTime
#define errAutoRstClrWaitTicker ref_errAutoRstClrWaitTicker
#define errAutoRstNum           ref_errAutoRstNum
#define errorAttribute          ref_errorAttribute
#define errorAttributeOld       ref_errorAttributeOld
#define errorCode               ref_errorCode
#define errorCodeFromMotorList  ref_errorCodeFromMotorList
#define errorCodeOld            ref_errorCodeOld
#define errorDealStatus         ref_errorDealStatus
#define errorEeprom             ref_errorEeprom
#define errorInfo               ref_errorInfo
#define errorInfoFromMotor      ref_errorInfoFromMotor
#define errorInfoOld            ref_errorInfoOld
#define errorOther              ref_errorOther
#define errorOtherOld           ref_errorOtherOld
#define errorSpaceTicker        ref_errorSpaceTicker
#define getMotorError           ref_getMotorError
#define underVoltageDeal        ref_underVoltageDeal
#define uvDealStatus            ref_uvDealStatus

#include "host_ref_f_error.c"
