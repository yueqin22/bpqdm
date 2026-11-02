/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define BufferResProtect       ref_BufferResProtect
#define CBCLimitCurProtect     ref_CBCLimitCurProtect
#define ControlFan             ref_ControlFan
#define InputPhaseLoseDetect   ref_InputPhaseLoseDetect
#define InvDeviceControl       ref_InvDeviceControl
#define LoadLoseDetect         ref_LoadLoseDetect
#define OutputLoseAdd          ref_OutputLoseAdd
#define OutputLoseReset        ref_OutputLoseReset
#define OutputPhaseLoseDetect  ref_OutputPhaseLoseDetect
#define OverLoadProtect        ref_OverLoadProtect
#define SetCBCDisable          ref_SetCBCDisable
#define SetCBCEnable           ref_SetCBCEnable
#define SoftWareErrorDeal      ref_SoftWareErrorDeal
#define TemperatureCheck       ref_TemperatureCheck
#define gBuffResCnt            ref_gBuffResCnt
#define gCBCProtect            ref_gCBCProtect
#define gError                 ref_gError
#define gFanCtrl               ref_gFanCtrl
#define gInLose                ref_gInLose
#define gInvOverLoadDecTable   ref_gInvOverLoadDecTable
#define gInvOverLoadTable      ref_gInvOverLoadTable
#define gInvOverLoadTableForP  ref_gInvOverLoadTableForP
#define gLoadLose              ref_gLoadLose
#define gMotorOverLoadDecTable ref_gMotorOverLoadDecTable
#define gMotorOverLoadTable    ref_gMotorOverLoadTable
#define gOverLoad              ref_gOverLoad
#define gPhaseLose             ref_gPhaseLose
#define gTempTableBSMXX        ref_gTempTableBSMXX
#define gTempTableP44X         ref_gTempTableP44X
#define gTempTableP8XX         ref_gTempTableP8XX
#define gTempTableSEMIKON      ref_gTempTableSEMIKON
#define gTempTableWAIZHI       ref_gTempTableWAIZHI

#include "MotorInvProtect_ref.c"
