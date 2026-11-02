/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define INV_TYPE_BIG_ACC_DEC_START_TABLE       new_INV_TYPE_BIG_ACC_DEC_START_TABLE
#define INV_TYPE_POINT_LIMIT_TABLE             new_INV_TYPE_POINT_LIMIT_TABLE
#define InverterTypeRelatedNoMotorParaDeal     new_InverterTypeRelatedNoMotorParaDeal
#define InverterTypeRelatedNoMotorParaSaveDeal new_InverterTypeRelatedNoMotorParaSaveDeal
#define InverterTypeRelatedParaDeal            new_InverterTypeRelatedParaDeal
#define MotorPowerRelatedParaDeal              new_MotorPowerRelatedParaDeal
#define SaveTuneData                           new_SaveTuneData
#define TorqueBoostDeal                        new_TorqueBoostDeal
#define UpdateInvType                          new_UpdateInvType
#define ValidateInvType                        new_ValidateInvType
#define invPara                                new_invPara
#define invTypeLimitTable                      new_invTypeLimitTable
#define invTypeParaTableS220                   new_invTypeParaTableS220
#define invTypeParaTableT1140                  new_invTypeParaTableT1140
#define invTypeParaTableT220                   new_invTypeParaTableT220
#define invTypeParaTableT380                   new_invTypeParaTableT380
#define invTypeParaTableT480                   new_invTypeParaTableT480
#define invTypeParaTableT690                   new_invTypeParaTableT690
#define motor1ParaIndex                        new_motor1ParaIndex
#define motorSnIndexIncToMotor1                new_motorSnIndexIncToMotor1
#define ovVoltageInitValue                     new_ovVoltageInitValue
#define pInvTypeParaTable                      new_pInvTypeParaTable

#include "host_new_f_invPara.c"
