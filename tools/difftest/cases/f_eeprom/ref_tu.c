/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define EepromDeal                  ref_EepromDeal
#define EepromOperateTimeDeal       ref_EepromOperateTimeDeal
#define GetEepromIndexFromFcIndex   ref_GetEepromIndexFromFcIndex
#define GetFcIndexFromEepromIndex   ref_GetFcIndexFromEepromIndex
#define I2CNoACK                    ref_I2CNoACK
#define I2CRcvByte                  ref_I2CRcvByte
#define I2CStop                     ref_I2CStop
#define I2cDealBeforeInit           ref_I2cDealBeforeInit
#define I2cIntDeal                  ref_I2cIntDeal
#define I2cMsg                      ref_I2cMsg
#define InitI2CGpio                 ref_InitI2CGpio
#define InitSetI2ca                 ref_InitSetI2ca
#define ReadFuncCode                ref_ReadFuncCode
#define RwEeprom                    ref_RwEeprom
#define RwI2cBus                    ref_RwI2cBus
#define UpdateIndexForEepromDeal    ref_UpdateIndexForEepromDeal
#define WriteFuncCode               ref_WriteFuncCode
#define eeprom2Fc                   ref_eeprom2Fc
#define eepromOperateTime           ref_eepromOperateTime
#define endIndexRwFuncCode          ref_endIndexRwFuncCode
#define endIndexWriteSeries         ref_endIndexWriteSeries
#define funcCodeOneWriteIndex       ref_funcCodeOneWriteIndex
#define funcCodeOneWriteNum         ref_funcCodeOneWriteNum
#define funcCodeRead                ref_funcCodeRead
#define funcCodeRwMode              ref_funcCodeRwMode
#define funcCodeRwModeTmp           ref_funcCodeRwModeTmp
#define funcCodeRwStatus            ref_funcCodeRwStatus
#define funcCodeWrite               ref_funcCodeWrite
#define index4EepromDeal            ref_index4EepromDeal
#define number4EepromDeal           ref_number4EepromDeal
#define rwFuncCodeRepeatDelayTicker ref_rwFuncCodeRepeatDelayTicker
#define startIndexWriteSeries       ref_startIndexWriteSeries

#include "f_eeprom_ref.c"
