/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define EepromDeal                  new_EepromDeal
#define EepromOperateTimeDeal       new_EepromOperateTimeDeal
#define GetEepromIndexFromFcIndex   new_GetEepromIndexFromFcIndex
#define GetFcIndexFromEepromIndex   new_GetFcIndexFromEepromIndex
#define I2CNoACK                    new_I2CNoACK
#define I2CRcvByte                  new_I2CRcvByte
#define I2CStop                     new_I2CStop
#define I2cDealBeforeInit           new_I2cDealBeforeInit
#define I2cIntDeal                  new_I2cIntDeal
#define I2cMsg                      new_I2cMsg
#define InitI2CGpio                 new_InitI2CGpio
#define InitSetI2ca                 new_InitSetI2ca
#define ReadFuncCode                new_ReadFuncCode
#define RwEeprom                    new_RwEeprom
#define RwI2cBus                    new_RwI2cBus
#define UpdateIndexForEepromDeal    new_UpdateIndexForEepromDeal
#define WriteFuncCode               new_WriteFuncCode
#define eeprom2Fc                   new_eeprom2Fc
#define eepromOperateTime           new_eepromOperateTime
#define endIndexRwFuncCode          new_endIndexRwFuncCode
#define endIndexWriteSeries         new_endIndexWriteSeries
#define funcCodeOneWriteIndex       new_funcCodeOneWriteIndex
#define funcCodeOneWriteNum         new_funcCodeOneWriteNum
#define funcCodeRead                new_funcCodeRead
#define funcCodeRwMode              new_funcCodeRwMode
#define funcCodeRwModeTmp           new_funcCodeRwModeTmp
#define funcCodeRwStatus            new_funcCodeRwStatus
#define funcCodeWrite               new_funcCodeWrite
#define index4EepromDeal            new_index4EepromDeal
#define number4EepromDeal           new_number4EepromDeal
#define rwFuncCodeRepeatDelayTicker new_rwFuncCodeRepeatDelayTicker
#define startIndexWriteSeries       new_startIndexWriteSeries

#include "f_eeprom.c"
