/* f_menu 差分测试的共享 mock。
 *
 * 一份实例, ref 侧与 new 侧**共用** —— 两侧看到的输入与"外部世界的反应"完全一样,
 * 唯一不同的只有被测实现本身。
 *
 * include 顺序与 src/function/f_menu.c 一致, 于是这里用到的都是**真类型**:
 * 模块用 GetCodeIndex() 在 funcCode 这个联合体的成员之间做指针减法取下标, 又把
 * funcCodeAttribute[] 的 909 项当成限值表来查 —— 换成替身类型就会算出替身偏移,
 * 测试等于什么都没测。
 *
 * 模块自己的导出 (70 个函数 + 46 个 .ebss + 9 个 .econst, 共 125 个) 由
 * ref_tu.c / new_tu.c 各复制一份 —— 那些才是要比的东西, 不在这里定义。
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include "mock_menu.h"

/* ---------------------------------------------------------------------------
 * funcCode —— 全模块最核心的共享状态
 *
 * 用真类型 FUNCCODE_ALL。静态存储期, 初值全 0, 也就是目标板上电、EEPROM 还没读
 * 进来时的样子。driver 在每侧跑之前从同一份快照恢复它。
 *
 * 本模块读写的成员 (经 GetCodeIndex 取下标):
 *   code.userPassword / userPasswordReadOnly / menuMode / mfkKeyFunc / runSrc /
 *   frqPoint / maxFrq / speedDispPointPos / speedDispCoeff / pidDisp /
 *   inverterGpType / dispParaRunBit / dispParaRun1 / dispParaRun2 /
 *   dispParaStopBit / ledDispParaRun1 / ledDispParaRun2 / ledDispParaStop /
 *   userCustom[] / funcParaView / aiaoCalibrateDisp / motorDebugFc /
 *   diFunc[] / vdiFunc[] / aiAsDiFunc[] / frqXSrc / frqYSrc /
 *   sCurveStartPhaseTime / sCurveEndPhaseTime / presetFrq / frqCalcSrc /
 *   errorFrqUnit / errorScene1/2/3 / saveUserParaFlag1/2 / tuneCmd /
 *   motorFcM2/3/4.tuneCmd / motorParaM1 / motorFcM2/3/4.motorPara /
 *   pgParaM1 / pcPulseLogic / pcPulseType / pcPulseSwap / pcZeroSelect /
 *   inverterType / accTime1..4 / decTime1..4 / jogAccTime / jogDecTime /
 *   accDecTimeUnit / diUpDownSlope / commOverTime
 * ------------------------------------------------------------------------ */
FUNCCODE_ALL funcCode;

/* ---------------------------------------------------------------------------
 * 运行态: 命令 / 状态 / 标志
 *
 * 这些是模块的**输入**, 也是它顺手改写的**输出**:
 *   MenuOnMfk()  改 keyFunc (KEY_SWITCH / KEY_REV / KEY_FWD_JOG / KEY_REV_JOG)
 *   Menu0OnUpDown() 改 accDecFrqPrcFlag / frqKeyUpDownDelta (后者在 f_frqSrc.c)
 *   ValidateTuneCmd() 改 tuneCmd
 *   ModifyFunccodeEnter() 改 funcCodeRwModeTmp
 * ------------------------------------------------------------------------ */
union RUN_FLAG runFlag;
Uint16 runMode;
Uint16 runSrc;
Uint16 tuneCmd;
Uint16 keyFunc;
Uint16 upDownDelta;
union INVT_STATUS invtStatus;
Uint16 setRunLostTime;
int16  torqueAim;

/* ---------------------------------------------------------------------------
 * 数字输入 / 输出 / 模拟量
 *
 * UpdateMenu3DisplayBuffer() 的 #if DEBUG_F_DISP_DIDO_STATUS_SPECIAL 分支直接读
 * diStatus.a.all / doStatus.a.all / diFunc.f1.all / diFunc.f2.all, 把它们按位拆成
 * 5 位十六进制显示; UpdateU0Data() 又把 diStatus/doStatus/aiDeal 抄进
 * funcCode.group.u0[]。所以这三组的位图必须是**非平凡**的。
 * ------------------------------------------------------------------------ */
struct DI_FUNC diFunc;
struct DI_STATUS_ALL diStatus;
struct DO_STATUS_ALL doStatus;
struct AI_DEAL aiDeal[AI_NUMBER];

/* ---------------------------------------------------------------------------
 * 显示缓冲与两个编码表
 *
 * displayBuffer 的初值逐字取自 src/function/f_ui.c:81 —— 模块大量用 `&=` / `|=`
 * 往它上面叠段码, 初值不同结果就不同。driver 每轮从快照恢复, 所以这个初值只
 * 影响"没被写到的位", 但既然有真值就照抄。
 * ------------------------------------------------------------------------ */
Uint16 displayBuffer[DISPLAY_8LED_NUM + 1] =
#include "tbl_display_buffer.inc"

const Uint16 DISPLAY_CODE[DISPLAY_8LED_CODE_NUM] =
#include "tbl_display_code.inc"

const Uint16 LED_CODE[DISPLAY_LED_CODE_NUM] =
#include "tbl_led_code.inc"

/* ---------------------------------------------------------------------------
 * 故障 / 时刻
 *
 * errorCode 是 UpdateMenu0DispStatus() 与 cycleShiftDeal() 的判据;
 * errorAttribute.bit.level 决定 UpdateErrorDisplayBuffer() 显示 "Err" 还是 "A"。
 * ------------------------------------------------------------------------ */
struct CUR_TIME curTime;
union ERROR_ATTRIBUTE errorAttribute;
Uint16 errorCode;
Uint16 errorInfo;

/* ---------------------------------------------------------------------------
 * 频率 / 电机
 *
 * DispDataDeal() 是唯一一个"把内部量换算成显示量"的函数, 它读 frqDroop / frq /
 * frqAim / pidFunc.ref / pidFunc.fdb / pulseInFrq / torqueCurrent / currentPu,
 * 所以这些量都要非平凡 —— 全 0 会让乘除法在 0 处求值, 阈值差 1 这类错法不可分辨。
 * ------------------------------------------------------------------------ */
int32  frq;
int32  frqAim;
int32  frqDroop;
int32  frqRun;
Uint16 frqFdb;
Uint16 frqKeyUpDownDelta;
Uint16 bFrqDigital;
Uint16 maxFrq;
Uint16 plcStep;
PID_FUNC pidFunc;

Uint16 generatrixVoltage;
Uint16 currentPu;
Uint16 enCoderPosition;
Uint16 ABZPos;
int16  gPhiRtDisp;
int16  outPower;
Uint32 pulseInFrq;
Uint16 lineSpeed;
Uint16 memoryValue;
Uint16 torqueCurrent;
Uint16 pmsmRotorPos;

struct MOTOR_FC motorFc;
enum MOTOR_SN motorSn;

Uint16 temperature;
Uint16 ZCount;
Uint16 vfSeparateVol;
Uint16 vfSeprateVolAim;
Uint16 ticker05msFuncEachDisp[4];
Uint16 ticker05msFuncInternalDisp[4];

P2P_DATA_STRUCT p2pData;

/* ---------------------------------------------------------------------------
 * 逆变器参数
 *
 * invPara.type > invPara.pointLimit 决定 UpdateMenu0DisplayBuffer() 与
 * UpdateMenu3DisplayBuffer() 要不要把小数点挪一位; invPara.volLevel 索引
 * invTypeLimitTable。
 * ------------------------------------------------------------------------ */
struct INV_PARA invPara;

const struct INV_TYPE_LIMIT invTypeLimitTable[INV_TYPE_VOLTAGE_NUM] =
#include "tbl_inv_type_limit.inc"

/* ---------------------------------------------------------------------------
 * EEPROM 串行写的握手变量
 *
 * f_menu.c 不碰写队列本身, 它设的是 funcCodeRwModeTmp。队列仍然带进快照, 因为
 * ModifyFunccodeEnter() 是"把功能码写下去"的入口, 把队列一起比可以确认它没有
 * 顺手写坏别的。
 * ------------------------------------------------------------------------ */
Uint16 funcCodeRwMode;
Uint16 funcCodeRwModeTmp;
Uint16 funcCodeOneWriteIndex[FMENU_WRITE_QUEUE_LEN];
Uint16 funcCodeOneWriteNum;

/* ---------------------------------------------------------------------------
 * 三张真表 + 一张运行期分级表
 *
 * 由 build/_gen_fmenu_tables.py 从 src/function/f_funcCode.c **逐字**抄出:
 * 909 项属性表 (lower/upper/init/attribute/eepromIndex)、83 项每组个数、
 * 83 项每组起始下标。driver 的 check_invariants() 会核对链无环、下标不越界。
 * ------------------------------------------------------------------------ */
const Uint16 funcCodeGradeAll[FMENU_GRADE_N] =
#include "tbl_grade_all.inc"

const Uint16 funcCodeGradeSum[FMENU_GRADE_N] =
#include "tbl_grade_sum.inc"

const FUNCCODE_ATTRIBUTE funcCodeAttribute[FMENU_ATTR_N] =
#include "tbl_attribute.inc"

/* UpdataFuncCodeGrade() 的输出, 也是 MenuModeDeal / GroupUpDown /
 * DealUserMenuModeGroupGrade / DealCheckMenuModeGroupGrade 的输入。 */
Uint16 funcCodeGradeCurMenuMode[FMENU_GRADE_N];

/* ---------------------------------------------------------------------------
 * 替身的记账量
 * ------------------------------------------------------------------------ */
Uint16 mockDigitCalls, mockDigitLastArg, mockDigitLastMode;
Uint16 mockGetInitCalls, mockGetInitLastIndex, mockGetInitLastType;
Uint16 mockLimitOtherCalls, mockLimitOtherLastIndex;
Uint16 mockClearRecordCalls;
Uint16 mockMotorPowerCalls, mockMotorPowerLastPower, mockMotorPowerLastSn;
Uint16 mockInvTypeRelatedCalls;
Uint16 mockValidateInvTypeCalls, mockValidateInvTypeRet;
Uint16 mockResetUpDownCalls;
Uint16 mockLpfCalls, mockLpfLastT, mockLpfLastIn;

/* ---------------------------------------------------------------------------
 * 替身实现
 * ------------------------------------------------------------------------ */

/* GetNumberDigit() / GetNumberDigit1() / GetNumberDigit2() 逐字照抄
 * src/function/f_main.c:832/855/880。
 *
 * 拆位规则必须和真实现一样: f_menu 的整套编辑逻辑 (ModifyFunccodeUpDown 的
 * "按位改数"、UpdateDisplayBufferAttribute 的段码查表、FcDigit2Bin) 都建立在
 * digit[] 的十进制/十六进制拆法上。替身若换了规则, 测的就不是同一张表。
 *
 * 宽度: `(Uint32)number * 52429 >> 19` 是"除以 10"的定点近似, number <= 65535
 * 时乘积 <= 3.43e9 < 2^32, 目标端与宿主都不溢出。 */
static void GetNumberDigit2(Uint16 digit[5], Uint16 number)
{
    Uint16 tmp;

    tmp = number >> 4;
    digit[0] = number - (tmp << 4);
    number = tmp;

    tmp = number >> 4;
    digit[1] = number - (tmp << 4);
    number = tmp;

    tmp = number >> 4;
    digit[2] = number - (tmp << 4);
    number = tmp;

    tmp = number >> 4;
    digit[3] = number - (tmp << 4);
    number = tmp;

    digit[4] = 0;
}

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

Uint16 GetNumberDigit(Uint16 digit[5], Uint16 number, Uint16 mode)
{
    mockDigitLastMode = mode;

    if (!mode)
    {
        GetNumberDigit1(digit, number);
    }
    else
    {
        GetNumberDigit2(digit, number);
    }

    if (digit[4])
            return 5;
    else if (digit[3])
        return 4;
    else if (digit[2])
        return 3;
    else if (digit[1])
        return 2;
    else
        return 1;
}

/* GetFuncCodeInit() 的真实现在 src/function/f_fcDeal.c:146 (已单独做过差分测试)。
 * 它有一大串特例 (标定曲线 / 加减速时间 / 变频器类型相关 / 电机功率相关 ...)。
 *
 * 这里用它的**主干语义**: 功能码的出厂初值就是属性表里的 init 字段
 * (funcCodeAttribute[index].init) —— 909 项里绝大部分走的正是这一条。
 *
 * 为什么替身就够了: 它在 ref 侧与 new 侧是**同一份**, 所以它的精度不影响差分
 * 测试的正确性, 只影响覆盖 —— DealCheckMenuModeGroupGrade() 里
 * `funcCode.all[idx] != GetFuncCodeInit(idx, 0)` 这个判据的两个方向, 由 driver
 * 直接摆 funcCode.all[idx] 来覆盖 (既取 == init 也取 != init)。
 *
 * 下标越界保护: funcCodeAttribute 只有 909 项, driver 保证只喂 < 909 的下标
 * (check_invariants 会断言这一点), 越界就直接停, 不静默读表外。 */
Uint16 GetFuncCodeInit(Uint16 index, Uint16 type)
{
    mockGetInitCalls++;
    mockGetInitLastIndex = index;
    mockGetInitLastType = type;

    if (index >= FMENU_ATTR_N)
    {
        printf("FATAL: GetFuncCodeInit 收到越界下标 %u (表长 %d)\n",
               index, FMENU_ATTR_N);
        exit(2);
    }
    return funcCodeAttribute[index].init;
}

/* LimitOtherCodeDeal() 的真实现在 src/function/f_fcDeal.c:426 (已单独做过差分
 * 测试)。它按 index 决定要不要把一批"被别的功能码限制"的码重新夹一遍, 返回
 * void, 所以观测量只能是**调用次数 + 末次实参**。
 *
 * 调用点: ModifyFunccodeEnter() 的最后那个 else 分支 —— 也就是"这个 index 不在
 * 任何特例清单里"时。索引错了就说明特例链走错了, 这正是要抓的。 */
void LimitOtherCodeDeal(Uint16 index)
{
    mockLimitOtherCalls++;
    mockLimitOtherLastIndex = index;
}

/* ClearRecordDeal() 的真实现在 src/function/f_fcDeal.c:236 (已单独做过差分
 * 测试), 返回 void。调用点: Menu3OnEnter() 里 paraInitMode == CLEAR_RECORD 那
 * 一支。 */
void ClearRecordDeal(void)
{
    mockClearRecordCalls++;
}

/* MotorPowerRelatedParaDeal() 的真实现在 src/function/f_invPara.c:402。返回
 * void, 记调用次数与两个实参 (功率 / 电机序号)。 */
void MotorPowerRelatedParaDeal(Uint16 power, Uint16 motorSn4Power)
{
    mockMotorPowerCalls++;
    mockMotorPowerLastPower = power;
    mockMotorPowerLastSn = motorSn4Power;
}

/* InverterTypeRelatedParaDeal() 的真实现在 src/function/f_invPara.c:294。返回
 * void, 只记次数。 */
void InverterTypeRelatedParaDeal(void)
{
    mockInvTypeRelatedCalls++;
}

/* ValidateInvType() 的真实现在 src/function/f_invPara.c:514。它**有返回值**, 而
 * ModifyFunccodeEnter() 用 `if (!ret) InverterTypeRelatedParaDeal();` 分支 ——
 * 所以返回值必须可控。driver 每轮从 mockValidateInvTypeRet 摆好再跑。 */
Uint16 ValidateInvType(void)
{
    mockValidateInvTypeCalls++;
    return mockValidateInvTypeRet;
}

/* ResetUpDownFrq() 的真实现在 src/function/f_frqSrc.c:1079 (已单独做过差分
 * 测试)。返回 void, 只记次数。 */
void ResetUpDownFrq(void)
{
    mockResetUpDownCalls++;
}

/* LpfCalc() 逐字照抄 src/function/f_io.c:2123 (取 #elif 1 那一支, 目标端就是
 * 这一支)。
 *
 * **必须逐字**: DispDataDeal() 在 RUN_MODE_TORQUE_CTRL 下把 torQueFrqDispLpf
 * 交给它, 再把 torQueFrqDispLpf.out 抄进 frqDisp —— 它的输出是**观测量**。
 * 注意源文件里 `int32 out;` 与 `out = ...` 那一行是**赋值后不使用**的死代码
 * (真实现也这样), 照抄即可。
 *
 * 除零保护: p->t 为 0 时走 `p->out = p->in` 那一支, 不进入除法。 */
void LpfCalc(LowPassFilter *p)
{
    int32 tmp;
    int32 out;

    mockLpfCalls++;
    mockLpfLastT = (Uint16)p->t;
    mockLpfLastIn = (Uint16)p->in;

    p->outOld = p->out;
    if (!p->t)
    {
        p->out = p->in;
    }
    else
    {
        tmp = ((int32)p->in - p->outOld + p->remainder) * AI_CALC_PERIOD;
        out = tmp / (p->t * 1) + p->outOld;

        p->out = tmp / (p->t * 1) + p->outOld;
        p->remainder = tmp % (p->t * 1);
    }
}
