/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define BufferResProtect       new_BufferResProtect
#define CBCLimitCurProtect     new_CBCLimitCurProtect
#define ControlFan             new_ControlFan
#define InputPhaseLoseDetect   new_InputPhaseLoseDetect
#define InvDeviceControl       new_InvDeviceControl
#define LoadLoseDetect         new_LoadLoseDetect
#define OutputLoseAdd          new_OutputLoseAdd
#define OutputLoseReset        new_OutputLoseReset
#define OutputPhaseLoseDetect  new_OutputPhaseLoseDetect
#define OverLoadProtect        new_OverLoadProtect
#define SetCBCDisable          new_SetCBCDisable
#define SetCBCEnable           new_SetCBCEnable
#define SoftWareErrorDeal      new_SoftWareErrorDeal
#define TemperatureCheck       new_TemperatureCheck
#define gBuffResCnt            new_gBuffResCnt
#define gCBCProtect            new_gCBCProtect
#define gError                 new_gError
#define gFanCtrl               new_gFanCtrl
#define gInLose                new_gInLose
#define gInvOverLoadDecTable   new_gInvOverLoadDecTable
#define gInvOverLoadTable      new_gInvOverLoadTable
#define gInvOverLoadTableForP  new_gInvOverLoadTableForP
#define gLoadLose              new_gLoadLose
#define gMotorOverLoadDecTable new_gMotorOverLoadDecTable
#define gMotorOverLoadTable    new_gMotorOverLoadTable
#define gOverLoad              new_gOverLoad
#define gPhaseLose             new_gPhaseLose
#define gTempTableBSMXX        new_gTempTableBSMXX
#define gTempTableP44X         new_gTempTableP44X
#define gTempTableP8XX         new_gTempTableP8XX
#define gTempTableSEMIKON      new_gTempTableSEMIKON
#define gTempTableWAIZHI       new_gTempTableWAIZHI

#include "MotorInvProtect.c"
