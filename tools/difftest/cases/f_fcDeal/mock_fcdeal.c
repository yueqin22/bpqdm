/* f_fcDeal 差分测试的共享 mock。
 *
 * 一份实例, ref 侧与 new 侧**共用** —— 两侧看到的输入与"外部世界的反应"完全一样,
 * 唯一不同的只有被测实现本身。
 *
 * include 顺序与 src/function/f_fcDeal.c 一致, 于是这里用到的都是**真类型**:
 * 模块靠 GetCodeIndex() 在 funcCode 这个联合体的成员之间做指针减法取下标,
 * 换成替身类型就会算出替身偏移, 测试等于什么都没测。
 *
 * 模块自己的导出 (limitedByOtherCodeIndex / limitedByOtherCodeIndexNum /
 * limitOtherCodeIndex) 由 ref_tu.c / new_tu.c 各复制一份 —— 那些才是要比的东西。
 */

#include "f_menu.h"
#include "f_main.h"
#include "f_eeprom.h"
#include "f_invPara.h"
#include "f_fcDeal.h"
#include "f_runSrc.h"
#include "f_frqSrc.h"

#include "mock_fcdeal.h"

/* ---------------------------------------------------------------------------
 * funcCode —— 全模块最核心的共享状态
 *
 * 用真类型 FUNCCODE_ALL。静态存储期, 初值全 0, 也就是目标板上电、EEPROM 还没读
 * 进来时的样子。driver 在每侧跑之前从同一份快照恢复它。
 * ------------------------------------------------------------------------ */
FUNCCODE_ALL funcCode;

/* ---------------------------------------------------------------------------
 * 写队列 (funcCodeOneWriteIndex / funcCodeOneWriteNum)
 *
 * 长度 200 = 真值, **故意不放大**, 理由见 mock_fcdeal.h 的 FCD_WRITE_QUEUE_LEN。
 * 队列内容与游标都是观测量: SaveOneFuncCode() 少写一次、写错下标、写错顺序,
 * 都在这里暴露。
 * ------------------------------------------------------------------------ */
Uint16 funcCodeOneWriteIndex[FCD_WRITE_QUEUE_LEN];
Uint16 funcCodeOneWriteNum;

/* 串行写模式的两个端点, 由 ClearRecordDeal() / PowerOffRemDeal() 设置。 */
Uint16 funcCodeRwModeTmp;
Uint16 startIndexWriteSeries;
Uint16 endIndexWriteSeries;

/* ---------------------------------------------------------------------------
 * 模块读写的其余全局量
 * ------------------------------------------------------------------------ */
struct INV_PARA invPara;
union FRQ_FLAG frqFlag;
union RUN_CMD runCmd;
int32  upDownFrq;
Uint16 plcStep;
Uint32 plcTime;
Uint16 pmsmRotorPos;
Uint16 saveEepromIndex;
Uint16 runDirPanelOld;

/* ---------------------------------------------------------------------------
 * 逆变器类型相关的三张表
 *
 * 前两张是**纯数据**, 没有地址运算, 所以直接照抄真值
 * (src/function/f_invPara.c:15-25)。抄真值而不是编造, 是因为
 * GetFuncCodeInit() 里 `invPara.ratingVoltage == 1140` 与
 * `ovVoltageInitValue[invPara.volLevel]` 两条分支都直接依赖它们的具体数字。
 * ------------------------------------------------------------------------ */
const Uint16 ovVoltageInitValue[INV_TYPE_VOLTAGE_NUM] =
    { 8100, 4000, 4000, 8900, 13000, 20000 };

const struct INV_TYPE_LIMIT invTypeLimitTable[INV_TYPE_VOLTAGE_NUM] =
{
    { 380,  INV_TYPE_LOWER_LIMIT_T380,  INV_TYPE_UPPER_LIMIT_T380  },
    { 220,  INV_TYPE_LOWER_LIMIT_T220,  INV_TYPE_UPPER_LIMIT_T220  },
    { 220,  INV_TYPE_LOWER_LIMIT_S220,  INV_TYPE_UPPER_LIMIT_S220  },
    { 480,  INV_TYPE_LOWER_LIMIT_T480,  INV_TYPE_UPPER_LIMIT_T480  },
    { 690,  INV_TYPE_LOWER_LIMIT_T690,  INV_TYPE_UPPER_LIMIT_T690  },
    { 1140, INV_TYPE_LOWER_LIMIT_T1140, INV_TYPE_UPPER_LIMIT_T1140 },
};

/* GetInvParaPointer(invType) =
 *     pInvTypeParaTable[invPara.volLevel] + (invType - invTypeLimitTable[volLevel].lower)
 *
 * 它**没有边界检查**: invType 小于 lower 时那个减法按 Uint16 回绕, 指针直接飞到
 * 不知道哪里去。真表是 f_invPara.c 里六段静态数组 (T380 30 项 / T220 16 / S220 7 /
 * T480 27 / T690 16 / T1140 19)。这里给一块 64 宽的私有池, 由 driver 负责把
 * invPara.type 限制在 [lower, upper] 内 —— 与真固件调用点的前置条件一致。
 *
 * 池子**非 const**, 因为 `const T *p` 指向非 const 对象是合法的, 而 driver 需要
 * 在每轮用例前把 carrierFrq / antiVibrateGain 填成确定值。全零池会让
 * "读错成员"这类错法不可见 (0 == 0)。
 */
INV_TYPE_RELATED_PARA fcdInvParaPool[INV_TYPE_VOLTAGE_NUM][FCD_INVPOOL_STRIDE];

const INV_TYPE_RELATED_PARA * pInvTypeParaTable[INV_TYPE_VOLTAGE_NUM] =
{
    fcdInvParaPool[0],
    fcdInvParaPool[1],
    fcdInvParaPool[2],
    fcdInvParaPool[3],
    fcdInvParaPool[4],
    fcdInvParaPool[5],
};

/* ---------------------------------------------------------------------------
 * gSendToFunctionDataBuff —— PowerOffRemDeal() 从 [30 .. 34] 读五个电机的掉电
 * 保持量 (REM_P_OFF_MOTOR = 5, MOTOR_TO_Func_2MS_DATA_NUM = 30)。
 * ------------------------------------------------------------------------ */
int16 gSendToFunctionDataBuff[FCD_F2M_LEN];

/* ---------------------------------------------------------------------------
 * funcCodeAttribute —— 替身表
 *
 * 由 build/_gen_fcd_attr.py 生成到 attr_tables.inc。生成物自带 ATTR_TABLE_N,
 * 下面那条编译期断言把它和真头文件算出来的 FNUM_PARA 对账: 表长 / 头文件任一侧
 * 改动而另一侧没跟上, 编译直接失败。
 *
 * 为什么不链接真表: 真表在 src/function/f_funcCode.c 里, 链进来会带一大坨无关
 * 符号, 而且它的静态初始化式是"两个地址常量相减", MSVC 报 C2099。
 * ------------------------------------------------------------------------ */
const FUNCCODE_ATTRIBUTE funcCodeAttribute[FCD_ATTR_N] =
{
#include "attr_tables.inc"
};

/* 编译期对账。用"负长度的数组"报错 —— 这是 C89 里唯一不需要任何扩展的做法
 * (MSVC 的 /W3 不会因为一个没人引用的 typedef 报警)。 */
typedef char fcd_attr_table_len_must_equal_FNUM_PARA[
    (ATTR_TABLE_N == FCD_ATTR_N) ? 1 : -1];

/* ---------------------------------------------------------------------------
 * 两个外部函数的替身
 * ------------------------------------------------------------------------ */

/* TorqueBoostDeal() 的返回值被 GetFuncCodeInit() 当作 TORQUE_BOOST_*_INDEX 的
 * 初值。替身返回 `base + power`, 于是"取了错的那个电机的额定功率"会通过返回值
 * 暴露 —— 固定返回值的话, 参数错了也看不出来。参数另记一份便于报差异。 */
Uint16 mockTorqueBoostDealBase;
Uint16 mockTorqueBoostDealCalls;
Uint16 mockTorqueBoostDealLastArg;

Uint16 TorqueBoostDeal(Uint16 power)
{
    mockTorqueBoostDealCalls++;
    mockTorqueBoostDealLastArg = power;
    return (Uint16)(mockTorqueBoostDealBase + power);
}

/* MenuModeDeal() 返回 void, 所以观测量只能是调用次数。
 * RestoreCompanyParaOtherDeal() 里它在两次 SaveOneFuncCode 之后、LimitOtherCodeDeal
 * 之前被调用 —— 顺序靠 SaveOneFuncCode 的队列序列已经能看出来, 这里只补上"调没调"。 */
Uint16 mockMenuModeDealCalls;

void MenuModeDeal(void)
{
    mockMenuModeDealCalls++;
}
