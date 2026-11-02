/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define AutoResetErrorDeal      new_AutoResetErrorDeal
#define AutoResetErrorFcDeal    new_AutoResetErrorFcDeal
#define AutoResetErrorTrueDeal  new_AutoResetErrorTrueDeal
#define ErrorDeal               new_ErrorDeal
#define ErrorReset              new_ErrorReset
#define ErrorSceneSave          new_ErrorSceneSave
#define GetErrorAttribute       new_GetErrorAttribute
#define GetErrorResetDeal       new_GetErrorResetDeal
#define GetNewError             new_GetNewError
#define GetNewErrorAttribute    new_GetNewErrorAttribute
#define UpdateError             new_UpdateError
#define bUv                     new_bUv
#define curTime                 new_curTime
#define errAutoRstClrWaitTicker new_errAutoRstClrWaitTicker
#define errAutoRstNum           new_errAutoRstNum
#define errorAttribute          new_errorAttribute
#define errorAttributeOld       new_errorAttributeOld
#define errorCode               new_errorCode
#define errorCodeFromMotorList  new_errorCodeFromMotorList
#define errorCodeOld            new_errorCodeOld
#define errorDealStatus         new_errorDealStatus
#define errorEeprom             new_errorEeprom
#define errorInfo               new_errorInfo
#define errorInfoFromMotor      new_errorInfoFromMotor
#define errorInfoOld            new_errorInfoOld
#define errorOther              new_errorOther
#define errorOtherOld           new_errorOtherOld
#define errorSpaceTicker        new_errorSpaceTicker
#define getMotorError           new_getMotorError
#define underVoltageDeal        new_underVoltageDeal
#define uvDealStatus            new_uvDealStatus

#include "host_new_f_error.c"
