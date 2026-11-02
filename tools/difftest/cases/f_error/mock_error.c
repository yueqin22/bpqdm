/* f_error 差分测试的共享 mock。
 *
 * 一份实例, ref 侧与 new 侧**共用** —— 两侧看到的输入与"外部世界的反应"完全一样,
 * 唯一不同的只有被测实现本身。
 *
 * include 顺序与 src/function/f_error.c 一致, 于是这里用到的都是**真类型**:
 * 模块把 funcCode.code.errorScene3.elem 的 8 个 Uint16 逐个写出来, 又用
 * GetCodeIndex() 在 funcCode 这个联合体的成员之间做指针减法取下标, 换成替身类型
 * 就会算出替身偏移, 测试等于什么都没测。
 *
 * 模块自己的导出 (errorCode / errorAttribute / errorDealStatus / ... 共 18 个)
 * 由 ref_tu.c / new_tu.c 各复制一份 —— 那些才是要比的东西, 不在这里定义。
 */

#include "f_error.h"
#include "f_main.h"
#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_eeprom.h"
#include "f_io.h"
#include "f_menu.h"
#include "f_ui.h"
#include "f_comm.h"
#include "f_posCtrl.h"
#include "f_invPara.h"

#include "mock_error.h"

/* ---------------------------------------------------------------------------
 * funcCode —— 全模块最核心的共享状态
 *
 * 用真类型 FUNCCODE_ALL。静态存储期, 初值全 0, 也就是目标板上电、EEPROM 还没读
 * 进来时的样子。driver 在每侧跑之前从同一份快照恢复它。
 *
 * 本模块读写的成员: code.ovPointSet / code.devChkValue / code.devChkTime /
 * code.osChkValue / code.osChkTime / code.plcEnable / code.plcErrorCode /
 * code.errAutoRstNumMax / code.errAutoRstSpaceTime / code.errorAction[0..3] /
 * code.errorLatest1 / code.errorLatest3 / code.errorScene1 / code.errorScene3 /
 * code.errorFrqUnit / code.frqPoint。
 * ------------------------------------------------------------------------ */
FUNCCODE_ALL funcCode;

/* ---------------------------------------------------------------------------
 * 运行态: 命令 / 状态 / 标志
 *
 * 这些是模块的**输入**, 也是它顺手改写的**输出** (ErrorReset() 会清
 * runFlag.bit.run、dspMainCmd 的 6 个位、dspMainCmd1.bit.speedTrack,
 * 还会置 dspSubCmd.bit.errorDealing)。
 * ------------------------------------------------------------------------ */
union RUN_FLAG runFlag;
Uint16 runMode;
union RUN_CMD runCmd;
union DSP_MAIN_COMMAND dspMainCmd;
union DSP_MAIN_COMMAND1 dspMainCmd1;
union DSP_SUB_COMMAND dspSubCmd;
union DSP_STATUS dspStatus;
Uint16 tuneCmd;

/* ---------------------------------------------------------------------------
 * EEPROM 串行写的握手变量
 *
 * f_error.c 不碰写队列本身, 它设的是这三个游标 + 两个模式字。ErrorReset() 里
 * `funcCodeRwModeTmp = FUNCCODE_RW_MODE_WRITE_SERIES` 与
 * `startIndexWriteSeries / endIndexWriteSeries = GetCodeIndex(...)` 是它唯一的
 * "对外部世界产生副作用"的地方 —— 三个值都是观测量。
 * ------------------------------------------------------------------------ */
Uint16 funcCodeRwMode;
Uint16 funcCodeRwModeTmp;
Uint16 startIndexWriteSeries;
Uint16 endIndexWriteSeries;
Uint16 funcCodeOneWriteIndex[FERR_WRITE_QUEUE_LEN];
Uint16 funcCodeOneWriteNum;

/* ---------------------------------------------------------------------------
 * 频率 / 电压 / 电机
 *
 * frq 与 frqRun 的差是 ERROR_DEV 的判据, frqFdbTmp 的绝对值是 ERROR_OS 的判据。
 * 两者都要非平凡: 全 0 会让 `ABS_INT32(frqRun - frq) > ...` 永远在 0 处求值,
 * "阈值差 1" 这种错法就不可分辨。
 * ------------------------------------------------------------------------ */
int32  frq;
int32  frqRun;
int32  frqFdbTmp;
Uint16 maxFrq;
Uint16 generatrixVoltage;

struct MOTOR_FC motorFc;

/* ---------------------------------------------------------------------------
 * 数字输入 / 输出
 *
 * diFunc.f1.bit.errorReset / externalErrOpenIn, diFunc.f2.bit.externalErrCloseIn /
 * userError1 / userError2 都是判据的一部分; diSelectFunc.f2.bit.externalErrCloseIn
 * 决定"常闭触点没动作"要不要算外部故障。
 * diStatus.a.all / doStatus.a.all 是 ErrorSceneSave() 录进故障现场的原始值。
 * ------------------------------------------------------------------------ */
struct DI_FUNC diFunc;
struct DI_FUNC diSelectFunc;
struct DI_STATUS_ALL diStatus;
struct DO_STATUS_ALL doStatus;
Uint16 outCurrentDispOld;
Uint16 currentOcDisp;

/* ---------------------------------------------------------------------------
 * 电机侧的故障位图与信息
 *
 * errorsCodeFromMotor[0]/[1] 是 32 位故障位图 (低 16 位 + 高 16 位),
 * errorCodeFromMotorList[] 把位映射成故障码, motorErrInfor[5] 每 4 位一个
 * 故障附加信息 (19 项 -> 最多用 5 个 Uint16)。
 * ------------------------------------------------------------------------ */
Uint16 errorCodeFromMotor;
Uint16 errorsCodeFromMotor[2];
Uint16 motorErrInfor[5];

Uint16 keyFunc;
enum POWER_ON_STATUS powerOnStatus;
enum MOTOR_SN motorSn;

/* ---------------------------------------------------------------------------
 * 两个替身
 * ------------------------------------------------------------------------ */

Uint16 mockDigitCalls;
Uint16 mockDigitLastArg;
Uint16 mockPowerOffCalls;

/* GetNumberDigit1() 逐字照抄 src/function/f_main.c:855。
 *
 * 拆位规则必须和真实现一样: GetErrorAttribute() 拿 digit[i] 当动作等级,
 * 替身若换了规则, 测的就是另一张表了。
 *
 * 宽度: `(Uint32)number * 52429 >> 19` 是"除以 10"的定点近似, number <= 65535
 * 时乘积 <= 3.43e9 < 2^32, 目标端与宿主都不溢出。 */
void GetNumberDigit1(Uint16 digit[5], Uint16 number)
{
    Uint16 tmp;

    mockDigitCalls++;
    mockDigitLastArg = number;

    tmp = (Uint32)number * 52429 >> 19;
    digit[0] = number - tmp * 10;
    number = tmp;

    tmp = (Uint32)number * 52429 >> 19;
    digit[1] = number - tmp * 10;
    number = tmp;

    tmp = (Uint32)number * 52429 >> 19;
    digit[2] = number - tmp * 10;
    number = tmp;

    tmp = (Uint32)number * 52429 >> 19;
    digit[3] = number - tmp * 10;
    number = tmp;

    tmp = (Uint32)number * 52429 >> 19;
    digit[4] = number - tmp * 10;
    number = tmp;
}

/* PowerOffRemDeal() 的真实现在 src/function/f_fcDeal.c, 已单独做过差分测试。
 * 返回 void, 所以观测量只能是调用次数。 */
void PowerOffRemDeal(void)
{
    mockPowerOffCalls++;
}
