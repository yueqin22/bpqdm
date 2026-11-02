/* f_main 差分测试的共享声明。

 * 放在 shim/ 是因为 shim/ 是唯一排在 include 路径上的"用例私有头文件"位置
 * (用例目录本身不在路径上, 只有 case/ref 与交付源码目录在)。它**不做任何头文件
 * 替换**, 只是让 mock_main.c 与 driver.c 共用同一份声明, 免得两边各抄一份然后漂移。

 * include 链与 src/function/f_main.c 完全一致, 于是这里用到的都是**真类型**。
 * 这对本模块是硬要求: 观测量里有一大半是 `FUNCCODE_ALL` / `union DSP_STATUS` /
 * `struct SPI_IN_OUT` / `union MOTOR_PARA` 的**具体布局** ——
 *   * InitFuncCode 用 GetCodeIndex() 把字段地址差当**下标**用;
 *   * 它顺着 funcCodeAttribute[].upper / .lower 这两个索引回 funcCode.all[] 取限值;
 *   * FuncVariableInit 把 funcCode.code.* 拆成十几个全局量。
 * 替身类型会让偏移整体错位, 测试就失去意义。

 * 注意这个头文件会被 build_variant 跟着 shim/ 一起复制到负向对照的变体目录 ——
 * 漏掉它的后果和漏掉 real.txt 一样: 变体编不过, 被误记成 BROKEN。
 */
#ifndef MOCK_MAIN_H
#define MOCK_MAIN_H

#include "f_main.h"
#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_eeprom.h"
#include "f_io.h"
#include "f_menu.h"
#include "f_ui.h"
#include "f_comm.h"
#include "f_posCtrl.h"
#include "f_osc.h"
#include "f_canlink.h"
#include "f_p2p.h"
#include "f_interface.h"

/* ---- 被测试台**刻意放大**的两张数组 ----------------------------------------
 *
 * 交付源码里它们是:
 *     Uint16 funcCodeOneWriteIndex[200];    (src/function/f_eeprom.c)
 *     Uint16 limitedByOtherCodeIndex[150];  (src/function/f_fcDeal.c)
 *
 * 200 / 150 是"真实运行中同时待写/待限的功能码个数"的上界, 不是数组语义的一部分。
 * 但本测试台会把 InitFuncCode 的 DEBUG_F_FC_INIT_OVER 分支喂进**随机** funcCode:
 * 那个分支对 909 项逐个夹限值, 每一项都可能 `SaveOneFuncCode(i)` 或
 * `limitedByOtherCodeIndex[j++] = i` —— 随机输入下最坏 909 / 1818 次。
 *
 * 目标端跑到那个数会踩坏相邻内存, 而宿主上"踩坏相邻内存"是不可复现的未定义行为。
 * 所以这里按**最坏情况**给足容量: 溢出变成不可能, 于是"写了几次、写了什么"成为
 * 确定且可比的观测量。这是测试台的容量选择, 不是对交付语义的改动 —— 交付源码里
 * 的 200 / 150 一个字都没动。
 */
#define FMAIN_ONE_WRITE_MAX     2048
#define FMAIN_LIMITED_MAX       2048

/* ---- 调用日志 --------------------------------------------------------------
 *
 * 本模块 18 个函数里, 13 个的主要行为就是**按顺序调用别的模块**:
 * Main05msFunctionA/B/C/D 是四相调度器, InitDspFunc 是一串外设初始化,
 * InitFuncCode 的每轮循环里也要跑一遍显示/采样。这些调用的**顺序**和**参数**
 * 就是它们的语义, 所以替身函数逐条记日志。
 *
 * 用序列而不是计数: `SciDeal()` 与 `OscSciFunction()` 对调这种错法, 计数完全一样。
 *
 * 长度取 5120 是被 InitFuncCode 逼出来的: DEBUG_F_FC_INIT_OVER 分支对 909 项
 * 各调 DiCalc + PulseInCalc + AiCalc, 再加 909 次 LimitDeal, 一次就是 ~3700 条。
 */
#define MAIN_LOG_LEN    5120

enum MAIN_OP
{
    MOP_ACC_DEC_TIME_CALC = 0,
    MOP_AI_CALC,
    MOP_AO_CALC_CHANNEL,
    MOP_CANLINK_FUN,
    MOP_DI_CALC,
    MOP_DISPLAY_SCAN,
    MOP_DISPLAY_SCAN_PREPARE,
    MOP_DO_CALC,
    MOP_EEPROM_DEAL,
    MOP_EEPROM_OPERATE_TIME_DEAL,
    MOP_ERROR_DEAL,
    MOP_ERROR_RESET,
    MOP_FMP_DEAL,
    MOP_FRQ_SRC_DEAL,
    MOP_GET_EEPROM_INDEX_FROM_FC_INDEX,
    MOP_GET_FC_INDEX_FROM_EEPROM_INDEX,
    MOP_I2C_DEAL_BEFORE_INIT,
    MOP_INIT_DI_GPIO,
    MOP_INIT_DO_GPIO,
    MOP_INIT_ECAP1_GPIO,
    MOP_INIT_EPWM4_GPIO,
    MOP_INIT_EPWM5_GPIO,
    MOP_INIT_EPWM6_GPIO,
    MOP_INIT_I2C_GPIO,
    MOP_INIT_SCIA_GPIO,
    MOP_INIT_SET_EPWM4,
    MOP_INIT_SET_EPWM5,
    MOP_INIT_SET_EPWM6,
    MOP_INIT_SET_ECAP1,
    MOP_INIT_SET_I2CA,
    MOP_INIT_SET_SCIA,
    MOP_INIT_SET_SPI_FOR_FUNC,
    MOP_INIT_SPIA_GPIO,
    MOP_KEY_PROCESS,
    MOP_KEY_SCAN,
    MOP_LIMIT_DEAL,
    MOP_MENU_INIT,
    MOP_OSC_SCI_FUNCTION,
    MOP_P2P_DATA_DEAL,
    MOP_PULSE_IN_CALC,
    MOP_PULSE_IN_SAMPLE,
    MOP_READ_FUNC_CODE,
    MOP_RUN_SRC_DEAL,
    MOP_SCI_DEAL,
    MOP_TEMPERATURE_DEAL,
    MOP_TORQUE_CALC,
    MOP_UPDATE_DATA_CORE2FUNC,
    MOP_UPDATE_DATA_FUNC2CORE_05MS,
    MOP_UPDATE_DATA_FUNC2CORE_2MS,
    MOP_UPDATE_DISPLAY_BUFFER,
    MOP_UPDATE_ERROR_DISPLAY_BUFFER,
    MOP_UPDATE_INV_TYPE,
    MOP_UPDATE_MOTOR_PARA,
    MOP_UPDATE_U0_DATA,
    MOP_WINK_DEAL,
    MOP_RUN_TIME_CAL,
    MOP_SET_TIME_RUN,
    MOP_VF_SEPARATE_DEAL,
    MOP_COUNT
};

struct MAIN_LOG_ENT
{
    Uint16 op;
    Uint16 a;
    Uint16 b;
};

extern struct MAIN_LOG_ENT gMainLog[MAIN_LOG_LEN];
extern Uint16              gMainLogN;
extern Uint16              gMainLogOverflow;

/* ---- 时间轴 -----------------------------------------------------------------
 *
 * f_main.h 里 `#define GetTime() (CpuTimer1.RegsAddr->TIM.all)`, 所以"时间"就是
 * 一个被读的 32 位寄存器。driver 扮演芯片: 每次调用前装一份完整映像, 而每个替身
 * 函数被调用时把计时器**减**掉 gTimeStep 拍 (TIM 是下计数器) —— 于是一次调用内部
 * GetTime() 的多次读数之间的差, 就由"这一趟调了几个替身"决定, 两侧完全一致。
 *
 * InitFuncCode 的循环正是靠它: `baseTime - GetTime() >= (TIME_1MS << 1)` 决定要不要
 * 走每 2 ms 一次的显示/采样那一块 (TIME_1MS = 60000UL @ DSP_CLOCK 60, 门槛 120000)。
 * 差值 = "自 baseTime 起走了几个替身边" x gTimeStep, 所以:
 *
 *   gTimeStep == 0     差值恒 0 -> 那块永不执行;
 *   gTimeStep  > 0     差值随替身边数**线性增长**, 于是"走到第几个替身边刚好跨过
 *                      120000"就成了一个真实可测的边界。
 *
 * driver 因此把 gTimeStep 当成**输入**来扫, 而且**刻意取 120000 的整除因子**
 * (0 / 1 / 59999 / 60000 / 120000 / 200000 / 40000 / 30000 / 24000 / 20000 /
 * 15000 / 12000): 一圈循环体里有 k 个替身边, 只要某个 gTimeStep == 120000 / k,
 * 差值就会**正好落在**门槛上 —— 那是 `>=` 与 `>` 唯一分歧的地方 (FM43)。整除
 * 因子少了就测不到那条边界, 这正是 FM43 首轮 MISSED 的原因。
 */
extern Uint32 gTimeStep;

/* ---- EEPROM 映像 -------------------------------------------------------------
 *
 * ReadFuncCode() 从这张表按 index 取值填进 funcCodeRead.data[]。driver 预置内容,
 * 就能决定 InitFuncCode 走 OLD_EEPROM 还是 NEW_EEPROM 分支 (看那两个 check word
 * 位置里放的是不是 EEPROM_CHECK1 / EEPROM_CHECK2)。
 */
#define FMAIN_EEPROM_MAX    4096
extern Uint16 gEeprom[FMAIN_EEPROM_MAX];

/* 返回 FUNCCODE_READ_RET_OK 还是 READING, 由 driver 控制 ——
 * 它是 InitFuncCode 状态机的握手信号, 返回 READING 会让循环原地打转。 */
extern Uint16 gReadFuncCodeRet;

/* ---- 外设 -------------------------------------------------------------------
 *
 * 寄存器块用真头文件里的名字: DSP2803x_CpuTimers.h 已经 `extern volatile
 * struct CPUTIMER_REGS CpuTimer1Regs;`, mock_main.c 只负责**定义**它。
 * driver 在 main() 里把 `CpuTimer1.RegsAddr` 指向它 —— GetTime() 就是
 * `CpuTimer1.RegsAddr->TIM.all`。
 */

/* ---- 停机预算: 测试台的**终止性观测点** -------------------------------------
 *
 * 测试台的比对时机是"入口函数返回之后"。可本模块里有一个**握手循环**:
 *
 *     while ((!errorEeprom)
 *            && (FUNCCODE_INIT_STATUS_OVER != funcCodeInitStatus))
 *
 * 只要有一条变异让它退不出去 (最典型的是把那个 `&&` 写成 `||`), 循环就永远不返回
 * —— 现场里一条字段级证据都没有, 只能靠 ccenv 的 300 s 超时记成 HUNG。
 * 而 HUNG 既不算 CAUGHT 也不算 BROKEN, 总账判失败, 而且"它到底哪里不一样"永远问
 * 不出来。这不是"变异不成立", 是测试台缺一个终止性观测点。
 *
 * 补法: 每个替身被调用时 tick() 累加一次, 累计超过 MOCK_TICK_BUDGET 就 longjmp 回
 * driver 的 step()。driver 于是照常比字段 —— 新增两个观测量:
 *
 *     ticks    这一侧这次调用一共推了多少拍替身 (= 走过了多少条调用边)
 *     aborted  这一侧是不是**没自己退出**、被预算强停的
 *
 * 预算两侧共用同一段代码, 不可能偏向任何一侧。而且基准侧单次调用最多用掉约 8 千拍
 * (InitFuncCode 的 DEBUG_F_FC_INIT_OVER 分支对 909 项逐个夹限值), 预算给到 50 万拍
 * —— 基准侧永远够不着, 预算对基准侧完全透明。driver 末尾会打印实测最大拍数,
 * "预算远高于实际用量"这件事是**落盘可查**的, 不是靠这里一句声明。
 */
#include <setjmp.h>

#define MOCK_TICK_BUDGET    500000UL

extern jmp_buf         gMockAbort;
extern int             gMockAbortArmed;
extern unsigned long   gMockTicks;

/* 每次调用前把计时器、日志、CPU 控制计数清成输入映像。 */
void main_world_reset(void);

#endif
