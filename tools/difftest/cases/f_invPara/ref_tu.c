/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define INV_TYPE_BIG_ACC_DEC_START_TABLE       ref_INV_TYPE_BIG_ACC_DEC_START_TABLE
#define INV_TYPE_POINT_LIMIT_TABLE             ref_INV_TYPE_POINT_LIMIT_TABLE
#define InverterTypeRelatedNoMotorParaDeal     ref_InverterTypeRelatedNoMotorParaDeal
#define InverterTypeRelatedNoMotorParaSaveDeal ref_InverterTypeRelatedNoMotorParaSaveDeal
#define InverterTypeRelatedParaDeal            ref_InverterTypeRelatedParaDeal
#define MotorPowerRelatedParaDeal              ref_MotorPowerRelatedParaDeal
#define SaveTuneData                           ref_SaveTuneData
#define TorqueBoostDeal                        ref_TorqueBoostDeal
#define UpdateInvType                          ref_UpdateInvType
#define ValidateInvType                        ref_ValidateInvType
#define invPara                                ref_invPara
#define invTypeLimitTable                      ref_invTypeLimitTable
#define invTypeParaTableS220                   ref_invTypeParaTableS220
#define invTypeParaTableT1140                  ref_invTypeParaTableT1140
#define invTypeParaTableT220                   ref_invTypeParaTableT220
#define invTypeParaTableT380                   ref_invTypeParaTableT380
#define invTypeParaTableT480                   ref_invTypeParaTableT480
#define invTypeParaTableT690                   ref_invTypeParaTableT690
#define motor1ParaIndex                        ref_motor1ParaIndex
#define motorSnIndexIncToMotor1                ref_motorSnIndexIncToMotor1
#define ovVoltageInitValue                     ref_ovVoltageInitValue
#define pInvTypeParaTable                      ref_pInvTypeParaTable

#include "host_ref_f_invPara.c"
