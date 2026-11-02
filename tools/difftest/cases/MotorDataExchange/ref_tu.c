/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define ParGet05Ms                  ref_ParGet05Ms
#define ParGet2Ms                   ref_ParGet2Ms
#define ParSend05Ms                 ref_ParSend05Ms
#define ParSend2Ms                  ref_ParSend2Ms
#define ParSendTune                 ref_ParSendTune
#define gParaIdToFunctionDataBuff   ref_gParaIdToFunctionDataBuff
#define gRealTimeToFunctionDataBuff ref_gRealTimeToFunctionDataBuff
#define gSendToFunctionDataBuff     ref_gSendToFunctionDataBuff
#define gSendToMotor05MsDataBuff    ref_gSendToMotor05MsDataBuff
#define gSendToMotor2MsDataBuff     ref_gSendToMotor2MsDataBuff
#define gSoftVersion                ref_gSoftVersion
#define uReservedData               ref_uReservedData

#include "MotorDataExchange_ref.c"
