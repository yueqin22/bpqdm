/* f_fcDeal 差分测试的共享状态。
 *
 * 放在 shim/ 是因为 shim/ 是唯一排在 include 路径上的"用例私有头文件"位置
 * (用例目录本身不在路径上, 只有 case/ref 与交付源码目录在)。它**不做任何头文件
 * 替换**, 只是让 mock_fcdeal.c 与 driver.c 共用同一份声明, 免得两边各抄一份然后
 * 慢慢漂移。
 *
 * 注意这个头文件会被 build_variant 跟着 shim/ 一起复制到负向对照的变体目录 ——
 * 漏掉它的后果和漏掉 real.txt 一样: 变体编不过, 被误记成 BROKEN。
 *
 * include 链与 src/function/f_fcDeal.c 保持一致, 于是类型都是**真类型**。
 * 这一点对本模块是硬要求: 模块靠 GetCodeIndex() 做"联合体成员之间的指针减法"
 * 来得到功能码下标, 替身类型会给出替身偏移, 测试就等于什么都没测。
 */
#ifndef MOCK_FCDEAL_H
#define MOCK_FCDEAL_H

#include "f_menu.h"
#include "f_main.h"
#include "f_eeprom.h"
#include "f_invPara.h"
#include "f_fcDeal.h"
#include "f_runSrc.h"
#include "f_frqSrc.h"

/* 写队列容量 —— 与工程真值一致 (src/function/f_eeprom.c:49 `FUNCCODE_ONE_WRITE_NUMBER_MAX`)。
 *
 * 这里**故意不放大**: SaveOneFuncCode() 是个没有边界检查的宏
 * (`funcCodeOneWriteIndex[funcCodeOneWriteNum++] = index`), 而 LimitOtherCodeDeal()
 * 的嵌套循环最多能连调 4 * limitedByOtherCodeIndexNum 次。真队列只有 200 格,
 * limitedByOtherCodeIndexNum 上限却是 LIMITED_BY_OTHER_CODE_NUM_MAX = 150
 * —— 也就是真固件里存在"写穿队列"的路径。把队列放大就等于把这条路径藏起来,
 * 所以保持 200, 由 driver 把 limitedByOtherCodeIndexNum 压在 16 以内来避开它。
 * 这一条写进用例 README 的"有意不测的既有缺陷"。 */
#define FCD_WRITE_QUEUE_LEN        200

/* 属性替身表长度 = 真表长度 = FNUM_PARA (宿主探针实测 909)。
 *
 * 真表在 f_funcCode.c 里, 链进来会带一大坨无关符号, 而且它的静态初始化式是
 * "两个地址常量相减", MSVC 报 C2099。
 *
 * 为什么取真值而不是取大: LimitOtherCodeDeal() 读 funcCodeAttribute[] **没有
 * 边界检查**。取大就等于把"真表其实只有 FNUM_PARA 项"这件事藏起来。见
 * build/_gen_fcd_attr.py 的说明。
 *
 * 直接写 FNUM_PARA (头文件算出来的) 而不是 909: 生成物 attr_tables.inc 里
 * 自带 ATTR_TABLE_N, mock_fcdeal.c 有一条编译期断言把它和这里对账, 一旦
 * 表长 / 真头文件任一改动, 编译当场失败。 */
#define FCD_ATTR_N                FNUM_PARA

/* gSendToFunctionDataBuff 的替身长度。真数组是 60 格
 * (MOTOR_TO_Func_2MS_DATA_NUM 30 + CORE_TO_FUNC_DISP_DATA_NUM 30),
 * PowerOffRemDeal() 读 [30 .. 34], 取 64 留余量。 */
#define FCD_F2M_LEN               64

/* f_fcDeal.c 私有的 LIMIT_OTHER_CODE_NUM —— 它在源码里, 不在头文件里,
 * 所以这里只能镜像一个。driver 用它遍历 limitOtherCodeIndex[]。 */
#define FCD_LIMIT_OTHER_CODE_NUM  13

/* f_fcDeal.c 私有的 LIMITED_BY_OTHER_CODE_NUM_MAX, 同样只能镜像。 */
#define FCD_LIMITED_INDEX_MAX     150

/* ---- 模块引用的 20 个全局量 + 2 个被调函数, 全部由 mock_fcdeal.c 定义 ---- */
extern FUNCCODE_ALL funcCode;
extern Uint16 funcCodeOneWriteIndex[FCD_WRITE_QUEUE_LEN];
extern Uint16 funcCodeOneWriteNum;
extern Uint16 funcCodeRwModeTmp;
extern Uint16 startIndexWriteSeries;
extern Uint16 endIndexWriteSeries;

extern struct INV_PARA invPara;
extern union FRQ_FLAG frqFlag;
extern union RUN_CMD runCmd;
extern int32  upDownFrq;
extern Uint16 plcStep;
extern Uint32 plcTime;
extern Uint16 pmsmRotorPos;
extern Uint16 saveEepromIndex;
extern Uint16 runDirPanelOld;

extern const struct INV_TYPE_LIMIT invTypeLimitTable[INV_TYPE_VOLTAGE_NUM];
extern const Uint16 ovVoltageInitValue[INV_TYPE_VOLTAGE_NUM];
extern const INV_TYPE_RELATED_PARA * pInvTypeParaTable[INV_TYPE_VOLTAGE_NUM];

/* GetInvParaPointer(invType) 会解引用到 pInvTypeParaTable[volLevel] + (invType - lower),
 * 也就是一个 INV_TYPE_RELATED_PARA 数组。真表在 f_invPara.c 里, 这里给一块
 * **非 const** 的私有池让 driver 能填 carrierFrq / antiVibrateGain。
 * `const T *p` 指向非 const 对象是合法的, 所以这样既满足头文件的 const 声明,
 * 又保住了 driver 的控制权。
 *
 * 池宽 64 >= 真表最长的一段 (T380 长度 30)。driver 把 invPara.type 限制在
 * [lower, upper] 内, 于是 GetInvParaPointer() 不会跑到池外 —— 它同样没有边界检查。 */
#define FCD_INVPOOL_STRIDE        64
extern INV_TYPE_RELATED_PARA
    fcdInvParaPool[INV_TYPE_VOLTAGE_NUM][FCD_INVPOOL_STRIDE];

extern int16 gSendToFunctionDataBuff[FCD_F2M_LEN];
extern const FUNCCODE_ATTRIBUTE funcCodeAttribute[FCD_ATTR_N];

/* ---- 两个被调函数是模块的**输入**而不是被测逻辑, 所以用替身 ----
 *
 * 真实现分别在 f_menu.c / f_invPara.c 里。
 *
 * MenuModeDeal() 返回 void, 所以它的观测量只能是**调用次数**。
 *
 * TorqueBoostDeal(power) 的返回值被模块当作 TORQUE_BOOST_*_INDEX 的初值。
 * 替身返回 `base + power`, 于是"传错电机的额定功率"这种错法会通过返回值
 * 暴露出来 —— 只记一个固定返回值的话, 参数错了也看不出来。
 * 参数本身另外单独记一份 (lastArg / calls), 便于 driver 报差异时报得清楚。 */
extern Uint16 mockTorqueBoostDealBase;
extern Uint16 mockTorqueBoostDealCalls;
extern Uint16 mockTorqueBoostDealLastArg;
extern Uint16 mockMenuModeDealCalls;

#endif
