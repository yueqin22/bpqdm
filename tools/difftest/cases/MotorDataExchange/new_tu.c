/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define ParGet05Ms                  new_ParGet05Ms
#define ParGet2Ms                   new_ParGet2Ms
#define ParSend05Ms                 new_ParSend05Ms
#define ParSend2Ms                  new_ParSend2Ms
#define ParSendTune                 new_ParSendTune
#define gParaIdToFunctionDataBuff   new_gParaIdToFunctionDataBuff
#define gRealTimeToFunctionDataBuff new_gRealTimeToFunctionDataBuff
#define gSendToFunctionDataBuff     new_gSendToFunctionDataBuff
#define gSendToMotor05MsDataBuff    new_gSendToMotor05MsDataBuff
#define gSendToMotor2MsDataBuff     new_gSendToMotor2MsDataBuff
#define gSoftVersion                new_gSoftVersion
#define uReservedData               new_uReservedData

#include "MotorDataExchange.c"
