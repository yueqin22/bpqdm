/* f_menu 差分测试驱动。
 *
 * 本文件 (以及 mock_menu.c / shim/mock_menu.h) 存成 **UTF-8 with BOM**。
 * 这不是随手加的: MSVC 在源文件没有 BOM 时按系统代码页 (本机 936/GBK) 解码,
 * 而 GBK 的双字节规则是"首字节 0x81-0xFE, 次字节 0x40-0xFE" —— UTF-8 的汉字
 * (E4-E9 80-BF 80-BF) 被这样解就会**串位**, 某些汉字的末字节会被当成首字节,
 * 把紧随其后的 ASCII 字节一起吃掉。实测本文件里
 *     report("menu[] 槽位指向的函数")
 * 的收尾双引号正好被吃掉, 于是字符串跨行, MSVC 报 C2001 "常量中有换行符",
 * 后面一串 C2143 全是它的余波; printf 里的 "\n" 也会丢掉反斜杠。
 * 加 BOM 让 MSVC 按 UTF-8 读, 汉字、字符串、转义序列就都保真了。
 * (tools/ 下的脚手架不受 verify_tree.py 的"纯 ASCII"约束, 那是交付源码的要求。)
 *
 * 被测模块是"功能码的菜单 / 编辑 / 显示"这一层。70 个函数:
 *
 *   基础工具   FcDigit2Bin, LimitDeal, NoSameDeal, OverTurnDeal,
 *              LimitOverTurnDeal, GetDispDigits
 *   功能码编辑 ModifyFunccodeUpDown, ModifyFunccodeEnter
 *   显示缓冲   UpdateDisplayBufferAttribute, UpdateGroupDisplay,
 *              UpdateMenu0DispStatus, UpdateMenu0/1/2/3DisplayBuffer,
 *              UpdateErrorDisplayBuffer, UpdateTuneDisplayBuffer,
 *              UpdateMenuPwdHintDisplayBuffer, UpdateMenuPwdInputDisplayBuffer,
 *              UpdateDisplayBufferVisualIoStatus, UpdateDisplayBufferVisualDiFunc,
 *              UpdateU0Data
 *   菜单状态机 MenuInit, MenuModeSwitch, MenuModeDeal, Menu0AddMenuLevel,
 *              Menu0/1/2/3OnPrg, Menu0/1/2/3OnEnter, Menu1/2/3OnUp,
 *              Menu1/2/3OnDown, Menu0/1/2/3OnShift, Menu0/1/2/3OnUpDown,
 *              MenuOnMfk, MenuOnRun, MenuOnStop, MenuOnQuick
 *   分级/查找  UpdataFuncCodeGrade, GroupUpDown, DealUserMenuModeGroupGrade,
 *              DealCheckMenuModeGroupGrade
 *   口令       MenuPwdOnPrg, MenuPwdHint2Input, MenuPwdHintOnShift,
 *              MenuPwdHintOnQuick, MenuPwdInputOnEnter, MenuPwdInputOnUp,
 *              MenuPwdInputOnDown, MenuPwdInputOnUpDown, MenuPwdInputOnShift,
 *              MenuPwdInputOnQuick
 *   运行量换算 DispDataDeal, MotorDebugFcDeal, cycleShiftDeal, ValidateTuneCmd
 *
 * ---- 比对什么 ----
 *
 *  返回值                    9 个非 void 函数的返回值
 *  模块自己的 125 个导出:
 *    46 个 .ebss 标量/结构体  菜单层级 / 模式 / 口令 / 显示值 / 计数器 ...
 *     9 个 .econst 表          decNumber / hexNumber / deltaK / diFuncIndex /
 *                              frqSrcFuncIndex / stopDispIndex / commDispIndex /
 *                              dispAttributeU0 / menu (函数指针表, 按符号比)
 *   70 个函数本体             由 build/_fn_coverage.py 逐个核对"两侧都调到了"
 *  共享状态里会被改写的部分:
 *    funcCode                 整个联合体逐字节 —— ModifyFunccodeEnter 直接写
 *                              funcCode.all[index], UpdateU0Data 写
 *                              funcCode.group.u0[0..64] 与 83..99
 *    displayBuffer[6]         显示缓冲 (段码)
 *    funcCodeGradeCurMenuMode[83]  UpdataFuncCodeGrade 的产物
 *    keyFunc / tuneCmd / frqKeyUpDownDelta / funcCodeRwModeTmp
 *    invPara                   ModifyFunccodeEnter 的间接产物
 *  十个替身的调用次数与末次实参
 *
 * ---- 为什么这些量都要比 ----
 *
 * f_menu 的绝大多数函数是 void, 全部作用都落在**共享状态**上。少写一个字段、
 * 写错一个字段、把 menuLevel 与 menuMode 写反 —— 这些错法只有把 125 个导出 +
 * 整个 funcCode + displayBuffer 一起比才看得见。
 *
 * 菜单状态机是**序列相关**的: menu[] 表把每个按键分派到 9 个槽位, 槽位里的函数
 * 又回写 menuLevel/menuMode/curFc, 下一次按键的走向由上一次的结果决定。所以
 * phase_menu_handlers 有意把"按下 → 再按下 → 再按下"打成轨迹, 让状态机的
 * 转移边变成可观测量。
 *
 * ---- 三张真表 ----
 *
 * funcCodeAttribute[909] / funcCodeGradeAll[83] / funcCodeGradeSum[83] 由
 * build/_gen_fmenu_tables.py 从 src/function/f_funcCode.c **逐字**抄出 (见
 * mock_menu.c 的 #include)。本模块的整套限值/分级语义都挂在这三张表上, 用合成表
 * 就等于换了一套语义。check_invariants() 会核对:
 *   - sizeof(FUNCCODE_ALL)/sizeof(Uint16) >= FNUM_ALL —— **不是 ==**。宿主 sizeof
 *     返回字节数, 而 f_funcCode.h 里那批 `Uint16 all[sizeof(struct X)]` 的"按字
 *     重叠"写法依赖 C28x 的"sizeof 返回字数", 于是宿主上 FUNCCODE_ALL 被撑到
 *     1290 字 (目标端 1116 字)。两边共用同一份布局, 分歧在比较中抵消; 目标端
 *     .econst 的逐字节一致性由 build_and_compare.py 的 Tier B 独立证明。
 *   - 两张 GetCodeIndex 表的宿主取值都落在 union 实际宽度内
 *   - FMENU_ATTR_N == FNUM_PARA
 *   - funcCodeAttribute 的 upperLimit / lowerLimit 链**无环** (GetDispDigits 会
 *     顺着链走, 有环就死循环)
 *   - 驱动实际会喂进去的下标全都 < FNUM_PARA; frqPoint 的值域 <= 4
 *   - FUNC_ATTRIBUTE 的位序与驱动假设一致
 *
 * ---- 有意不测的既有缺陷 (宿主上不可复现) ----
 *
 * 1. Menu3OnEnter() 第 758 行 `Uint16 writable = funcCodeAttribute[curFc.index]
 *    .attribute.bit.writable;` 出现在第 784 行的 `if (curFc.group >=
 *    FUNCCODE_GROUP_U0) return;` **之前**。而 U 组的 curFc.index =
 *    GetGradeIndex(group, grade) 里 funcCodeGradeSum[U0] >= FNUM_PARA (= 909,
 *    见 check_invariants 打印), 也就是**越界读**。越界读到的内容宿主与目标端
 *    必然不同 (后面跟着的常量表布局不一样), 所以这条路径只能排除。
 *    驱动对 Menu3OnEnter 只喂 curFc.group < FUNCCODE_GROUP_U0。
 * 2. MenuOnQuick() 的 `menuModeNext[((Uint16)menuModeTmp) - 1]`:
 *    `(Uint16)x - 1` 在目标端是**无符号 16 位**减法 (x=0 时得 65535),
 *    在宿主上是 32 位有符号减法 (x=0 时得 -1)。两者都是数组越界, 但越界位置
 *    不同 —— 不可复现。驱动保证 ON_QUICK 时 menuModeTmp ∈ {1,2,3}。
 * 3. cycleShiftDeal() 的 do-while:
 *    `while (!(para & (0x1UL << *bit)));` —— para 的 [0, max) 位若**一位都没置**,
 *    这就是死循环 (para 只保证非 0, 不保证低位有 1)。真固件同样死。
 *    驱动保证 para & ((1 << max) - 1) != 0, 见 phase_cycle_shift 的构造。
 * 4. NoSameDeal() 的 `for (i = 0; i < number; i++)`: i 是 Uint16, number 是 int16。
 *    目标端比较是**无符号**的, number < 0 会变成 65535 次循环; 宿主端是有符号的,
 *    一次都不循环。真调用点只传 sizeof(...) >= 0, 所以驱动也只喂 number >= 0。
 *    (这一条与 host_patch 的 `i < (Uint16)number` 是同一条; 补丁把两边都拉回
 *     目标端的无符号语义, 于是 number < 0 时两边**都**跑 65535 次 —— 但那是
 *     几万次循环里再套一层循环, 用例会变得很慢, 所以驱动仍然不喂负数。)
 * 5. NoSameDeal() 在 **upper < lower** 时死循环。循环体里
 *        data -= 1; if (data < lower) data = upper;
 *    会把 data 送回 upper; 若 upper 恰好命中表值, 就 upper -> upper-1 -> upper
 *    无限打转。真调用点的 upper/lower 来自属性表, 恒满足 upper >= lower, 走不到;
 *    但这是原工程的既有缺陷 (与第 3 条同类), 且宿主上表现为**测试台挂死**,
 *    所以驱动只喂有序的 (lower, upper) 对, 见 phase_basics 的 pair[][]。
 * 6. ModifyFunccodeEnter(index, dataNew) 把 dataNew **原样写进 funcCode**, 而紧随
 *    其后的分支会把它当**表下标**用: index == GetCodeIndex(frqPoint) 那一支立刻
 *    求值 `decNumber[funcCode.code.frqPoint]`, decNumber 只有 5 项。喂 65535 就是
 *    131 KB 越界读 (宿主 SIGSEGV; 目标端读相邻 .econst, 内容与宿主不同, 不可比)。
 *    真调用点的 dataNew 都经过 ModifyFunccodeUpDown 的 LimitDeal 夹紧, 恒在
 *    [lower, upper] 内。驱动用 fc_value_range() 顺 upper/lower 链解析值域后再喂。
 * 7. UpdateGroupDisplay() 的组号分档在 FUNCCODE_GROUP_CF (=64) 与
 *    FUNCCODE_GROUP_U0 (=67) 之间**留了 65 / 66 两个空号**: 走 else 分支会算出
 *    disp0 = group - U0 = 65534 / 65535, 紧接着
 *    `DISPLAY_CODE[groupDisplay.disp0]` 越界 131 KB (宿主 SIGSEGV)。目标端同样会
 *    去读 DISPLAY_CODE 后面很远的地方, 只是不触发页错误 —— 不可比。若
 *    funcCodeGradeCurMenuMode[65]/[66] 为 0, GroupUpDown 真的可能停在空号上,
 *    所以这是可达的既有缺陷。驱动只扫合法组号 [0, CF] ∪ [U0, FUNCCODE_GROUP_NUM)。
 * 8. 反汇编层面的既有缺陷 (GetDispDigits 的链、funcCodeGradeSum 的越界) 见 README。
 *
 * ---- 另外两条"契约域"约束 (不是缺陷, 但喂错就崩) ----
 *
 * 9. **curFc 必须是一个真实存在的 (group, grade) 对**。
 *    Menu2OnEnter() 第 662 行、Menu3OnUpDown() 第 949 行都写
 *        curFc.index = GetGradeIndex(curFc.group, curFc.grade);
 *    紧接着拿 curFc.index 去索引 funcCode.all[] / GetDispDigits()。
 *    而 GetGradeIndex(group, grade) 就是 funcCodeGradeSum[group] + grade,
 *    **没有任何边界检查**: 用户组 (>= FUNCCODE_GROUP_U0 = 67) 的
 *    funcCodeGradeSum 已经 >= FNUM_PARA (= 909, 见 check_invariants 打印),
 *    一进去就是 131 KB 越界读。真固件靠"游标永远合法"这个不变式活着。
 *    phase_random 因此从"计数 > 0 的厂家组 x 该组 grade 数"里取 curFc;
 *    用户组那几条路径交给 phase_menu_handlers / phase_display_menu123 定向覆盖。
 * 10. **flag 只可能是 ON_UP_KEY(1) / ON_DOWN_KEY(2)**。
 *    DealUserMenuModeGroupGrade / DealCheckMenuModeGroupGrade / GroupUpDown
 *    都把 flag 原样转给 LimitOverTurnDeal(), 后者做 `data += deltaK[flag]`,
 *    而 `const int16 deltaK[3] = {0, 1, -1}` 只有 3 项。真机上 flag 全部来自
 *    按键分派 (Menu2OnUpDown(ON_UP_KEY|ON_DOWN_KEY)), 所以恒在 {1, 2}。
 *    phase_random 只给这几个 kind 喂 {0,1,2} / {1,2}, 见 clamp_random_args。
 *    (DealCheckMenuModeGroupGrade 连 0 都不喂: deltaK[0] = 0 时
 *     LimitOverTurnDeal 原地空转 upper-low 次后原样返回起始组, 语义上等于
 *     "没按过键", 真机不存在这种调用。)
 *
 * ---- 一个**差分测试盖不到**的函数: UpdateU0Data ----
 *
 * f_menu.c 第 202 行有 `#include "f_funcCode_disp.c"`, 该文件在
 * include/function/ 下 (Debug.lkf 里没有对应 .obj, 所以它不是独立编译单元,
 * 只是被 f_menu.c 拉进来一起编 —— 它的代码确实进 f_menu.obj)。
 *
 * 问题: `host_ref_f_menu.c` 与 `host_new_f_menu.c` 都生成在同一个 gen 目录里,
 * 而带引号的 `#include "f_funcCode_disp.c"` 先按"包含它的那个文件所在目录"
 * 查找 —— 于是两侧拿到的是**同一份** include/function/f_funcCode_disp.c。
 * 也就是说 UpdateU0Data 的函数体在两侧逐字相同, 差分测试对它恒等,
 * 改动它**永远**报 0 处不一致。
 *
 * 已实测确认: 把 f_funcCode_disp.c 里 `funcCode.group.u0[0] = frqDisp;` 改成
 * `= frqDisp + 1;`, run_case.py 仍然报 `mismatches=0`。
 *
 * 处置: **不改写该文件**, 保持与重写前逐字节相同 —— 源码相同 + 同一编译器,
 * 行为必然相同, 这比差分测试更强 (差分测试只是"没观测到差异")。
 * 它的等价性由 verify_tree.py (编码/注释) 与 build_and_compare.py (目标端
 * 产物级比对) 独立保证。这个洞写进 README, 免得后人以为它被测过。
 *
 * ---- 负向对照要注意的: 死代码 ----
 *
 * f_menu.c 有 **13 块**死代码 (行区间以冻结基准为准, 见 build/_rewrite_fmenu.py 的
 * DEAD_RANGES, 共 246 行 / 基准 2815 行里的 2569 行活代码):
 *
 *   16-28    `#if F_DEBUG_RAM` 的 0 号分支体
 *   53-68    两个 DEBUG_F_USER_MENU_MODE / DEBUG_F_CHECK_MENU_MODE 定义
 *   274      `#pragma DATA_SECTION(menu, "const_zone")`
 *   717-731  Menu2OnUpDown 里 `#elif 1` 的另一支 (curFc.grade += delta)
 *   790-827  `#if 0` 的 RESTORE_COMPANY_PARA_EXCEPT_INDEX 结构体
 *   2006-2010 `#if 0` 的 paraInitMode 分支
 *   2066-2146 `#if 0` 的 commOverTime 分支
 *   2208-2210 `#if 0` 的 diFunc[4] 分支
 *   2218-2249 `#if 0` 的 fvcPgLogic (outPower 那一段)
 *   2460-2464 `#if 0` 的 RestoreCompanyParaRamDeal
 *   2703-2716 `#if 0` 的 DealCheckMenuMode 收尾
 *   2755-2767 `#if 0` 的 facPassViewStatus 分支
 *   2797-2806 文件末尾 `#elif 1` 的空口令函数组 (MenuPwdOnPrg / MenuPwdHint2Input)
 *
 * 死代码在行为上不可观测, 所以 negative_control.py 的 CASES 条目用
 * `live_span: None` + 逐条核对行号 —— 与 f_fcDeal / f_error 同一套做法。
 * 这 13 块的**整块文本**在改写前后逐字节相同, 由 build/_rewrite_fmenu.py 断言
 * (DEAD_RANGES 切块 + DEAD_SNIPPETS 逐块取代表句)。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 与 src/function/f_menu.c 相同的 include 顺序。 */
#include "f_menu.h"
#include "f_main.h"
#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_io.h"
#include "f_ui.h"
#include "f_eeprom.h"
#include "f_comm.h"
#include "f_posCtrl.h"
#include "f_invPara.h"
#include "f_fcDeal.h"
#include "f_p2p.h"

#include "mock_menu.h"

/* ---- 驱动自己的常量 ---- */

#define FAIL_LIMIT      20
#define Q_SNAP          FMENU_WRITE_QUEUE_LEN
#define MENU_SLOT_N     10

/* funcCodeGradeCurMenuMode 的长度 = FUNCCODE_GROUP_NUM (83)。 */
#define GRADE_N         FUNCCODE_GROUP_NUM

/* f_menu.c 私有的 UP_DOWN_DEAL_DONE_TIME / DECIMAL_DISPLAY_UPDATE_TIME,
 * 头文件里没有, 只能镜像 (见 mock_menu.h)。 */
#define FMENU_UP_DOWN_DONE_TIME     800

/* 这四个下标来自 include/function/f_funcCode_disp.c —— 那个文件被 f_menu.c
 * 第 202 行 `#include` 进来, 是**模块 TU 的一部分**, 不在公共头文件里, 所以
 * driver 用不到它们, 只能镜像。值必须逐个对应 (U 组显示区的四个特殊功能码)。 */
#define DISP_DI_STATUS_SPECIAL1     41
#define DISP_DO_STATUS_SPECIAL1     42
#define DISP_DI_FUNC_SPECIAL1       43
#define DISP_DI_FUNC_SPECIAL2       44

/* ---- 被测函数 (70 个, 两侧各一份) ---- */

extern void   ref_DealCheckMenuModeGroupGrade(Uint16 flag);
extern void   new_DealCheckMenuModeGroupGrade(Uint16 flag);
extern void   ref_DealUserMenuModeGroupGrade(Uint16 flag);
extern void   new_DealUserMenuModeGroupGrade(Uint16 flag);
extern void   ref_DispDataDeal(void);
extern void   new_DispDataDeal(void);
extern Uint16 ref_FcDigit2Bin(Uint16 value);
extern Uint16 new_FcDigit2Bin(Uint16 value);
extern Uint16 ref_GetDispDigits(Uint16 index);
extern Uint16 new_GetDispDigits(Uint16 index);
extern Uint16 ref_GroupUpDown(const Uint16 funcCodeGrade[], Uint16 group, Uint16 flag);
extern Uint16 new_GroupUpDown(const Uint16 funcCodeGrade[], Uint16 group, Uint16 flag);
extern Uint16 ref_LimitDeal(Uint16 signal, Uint16 data, Uint16 upper, Uint16 lower, int16 delta);
extern Uint16 new_LimitDeal(Uint16 signal, Uint16 data, Uint16 upper, Uint16 lower, int16 delta);
extern Uint16 ref_LimitOverTurnDeal(const Uint16 limit[], Uint16 data, Uint16 upper, Uint16 low, Uint16 flag);
extern Uint16 new_LimitOverTurnDeal(const Uint16 limit[], Uint16 data, Uint16 upper, Uint16 low, Uint16 flag);
extern void   ref_Menu0AddMenuLevel(void);
extern void   new_Menu0AddMenuLevel(void);
extern void   ref_Menu0OnEnter(void);
extern void   new_Menu0OnEnter(void);
extern void   ref_Menu0OnPrg(void);
extern void   new_Menu0OnPrg(void);
extern void   ref_Menu0OnShift(void);
extern void   new_Menu0OnShift(void);
extern void   ref_Menu0OnUpDown(void);
extern void   new_Menu0OnUpDown(void);
extern void   ref_Menu1OnDown(void);
extern void   new_Menu1OnDown(void);
extern void   ref_Menu1OnEnter(void);
extern void   new_Menu1OnEnter(void);
extern void   ref_Menu1OnPrg(void);
extern void   new_Menu1OnPrg(void);
extern void   ref_Menu1OnShift(void);
extern void   new_Menu1OnShift(void);
extern void   ref_Menu1OnUp(void);
extern void   new_Menu1OnUp(void);
extern void   ref_Menu1OnUpDown(Uint16 flag);
extern void   new_Menu1OnUpDown(Uint16 flag);
extern void   ref_Menu2OnDown(void);
extern void   new_Menu2OnDown(void);
extern void   ref_Menu2OnEnter(void);
extern void   new_Menu2OnEnter(void);
extern void   ref_Menu2OnPrg(void);
extern void   new_Menu2OnPrg(void);
extern void   ref_Menu2OnShift(void);
extern void   new_Menu2OnShift(void);
extern void   ref_Menu2OnUp(void);
extern void   new_Menu2OnUp(void);
extern void   ref_Menu2OnUpDown(Uint16 flag);
extern void   new_Menu2OnUpDown(Uint16 flag);
extern void   ref_Menu3OnDown(void);
extern void   new_Menu3OnDown(void);
extern void   ref_Menu3OnEnter(void);
extern void   new_Menu3OnEnter(void);
extern void   ref_Menu3OnPrg(void);
extern void   new_Menu3OnPrg(void);
extern void   ref_Menu3OnShift(void);
extern void   new_Menu3OnShift(void);
extern void   ref_Menu3OnUp(void);
extern void   new_Menu3OnUp(void);
extern void   ref_Menu3OnUpDown(Uint16 flag);
extern void   new_Menu3OnUpDown(Uint16 flag);
extern void   ref_MenuInit(void);
extern void   new_MenuInit(void);
extern void   ref_MenuModeDeal(void);
extern void   new_MenuModeDeal(void);
extern void   ref_MenuModeSwitch(void);
extern void   new_MenuModeSwitch(void);
extern void   ref_MenuOnMfk(void);
extern void   new_MenuOnMfk(void);
extern void   ref_MenuOnQuick(void);
extern void   new_MenuOnQuick(void);
extern void   ref_MenuOnRun(void);
extern void   new_MenuOnRun(void);
extern void   ref_MenuOnStop(void);
extern void   new_MenuOnStop(void);
extern void   ref_MenuPwdHint2Input(void);
extern void   new_MenuPwdHint2Input(void);
extern void   ref_MenuPwdHintOnQuick(void);
extern void   new_MenuPwdHintOnQuick(void);
extern void   ref_MenuPwdHintOnShift(void);
extern void   new_MenuPwdHintOnShift(void);
extern void   ref_MenuPwdInputOnDown(void);
extern void   new_MenuPwdInputOnDown(void);
extern void   ref_MenuPwdInputOnEnter(void);
extern void   new_MenuPwdInputOnEnter(void);
extern void   ref_MenuPwdInputOnQuick(void);
extern void   new_MenuPwdInputOnQuick(void);
extern void   ref_MenuPwdInputOnShift(void);
extern void   new_MenuPwdInputOnShift(void);
extern void   ref_MenuPwdInputOnUp(void);
extern void   new_MenuPwdInputOnUp(void);
extern void   ref_MenuPwdInputOnUpDown(Uint16 flag);
extern void   new_MenuPwdInputOnUpDown(Uint16 flag);
extern void   ref_MenuPwdOnPrg(void);
extern void   new_MenuPwdOnPrg(void);
extern Uint16 ref_ModifyFunccodeEnter(Uint16 index, Uint16 data);
extern Uint16 new_ModifyFunccodeEnter(Uint16 index, Uint16 data);
extern Uint16 ref_ModifyFunccodeUpDown(Uint16 index, Uint16 *data, int16 delta);
extern Uint16 new_ModifyFunccodeUpDown(Uint16 index, Uint16 *data, int16 delta);
extern void   ref_MotorDebugFcDeal(void);
extern void   new_MotorDebugFcDeal(void);
extern Uint16 ref_NoSameDeal(Uint16 index, const Uint16 funcIndex[], int16 number,
                             int16 data, int16 upper, int16 lower, int16 delta);
extern Uint16 new_NoSameDeal(Uint16 index, const Uint16 funcIndex[], int16 number,
                             int16 data, int16 upper, int16 lower, int16 delta);
extern Uint16 ref_OverTurnDeal(Uint16 data, Uint16 upper, Uint16 lower, Uint16 flag);
extern Uint16 new_OverTurnDeal(Uint16 data, Uint16 upper, Uint16 lower, Uint16 flag);
extern void   ref_UpdataFuncCodeGrade(Uint16 funcCodeGrade[]);
extern void   new_UpdataFuncCodeGrade(Uint16 funcCodeGrade[]);
extern void   ref_UpdateDisplayBufferAttribute(const Uint16 data, union FUNC_ATTRIBUTE attribute);
extern void   new_UpdateDisplayBufferAttribute(const Uint16 data, union FUNC_ATTRIBUTE attribute);
extern void   ref_UpdateDisplayBufferVisualDiFunc(Uint16 valueH, Uint32 valueL);
extern void   new_UpdateDisplayBufferVisualDiFunc(Uint16 valueH, Uint32 valueL);
extern void   ref_UpdateDisplayBufferVisualIoStatus(Uint32 value);
extern void   new_UpdateDisplayBufferVisualIoStatus(Uint32 value);
extern void   ref_UpdateErrorDisplayBuffer(void);
extern void   new_UpdateErrorDisplayBuffer(void);
extern void   ref_UpdateGroupDisplay(Uint16 group);
extern void   new_UpdateGroupDisplay(Uint16 group);
extern void   ref_UpdateMenu0DispStatus(void);
extern void   new_UpdateMenu0DispStatus(void);
extern void   ref_UpdateMenu0DisplayBuffer(void);
extern void   new_UpdateMenu0DisplayBuffer(void);
extern void   ref_UpdateMenu1DisplayBuffer(void);
extern void   new_UpdateMenu1DisplayBuffer(void);
extern void   ref_UpdateMenu2DisplayBuffer(void);
extern void   new_UpdateMenu2DisplayBuffer(void);
extern void   ref_UpdateMenu3DisplayBuffer(void);
extern void   new_UpdateMenu3DisplayBuffer(void);
extern void   ref_UpdateMenuPwdHintDisplayBuffer(void);
extern void   new_UpdateMenuPwdHintDisplayBuffer(void);
extern void   ref_UpdateMenuPwdInputDisplayBuffer(void);
extern void   new_UpdateMenuPwdInputDisplayBuffer(void);
extern void   ref_UpdateTuneDisplayBuffer(void);
extern void   new_UpdateTuneDisplayBuffer(void);
extern void   ref_UpdateU0Data(void);
extern void   new_UpdateU0Data(void);
extern Uint16 ref_ValidateTuneCmd(Uint16 value, Uint16 motorsN);
extern Uint16 new_ValidateTuneCmd(Uint16 value, Uint16 motorsN);
extern void   ref_cycleShiftDeal(Uint16 flag);
extern void   new_cycleShiftDeal(Uint16 flag);

/* 70 个函数名的清单 —— 只用来建"函数指针 -> 序号"的同一性表 (比 menu[] 表用)。 */
#define FMENU_FN_LIST(X)                     \
    X(DealCheckMenuModeGroupGrade)           \
    X(DealUserMenuModeGroupGrade)            \
    X(DispDataDeal)                          \
    X(FcDigit2Bin)                           \
    X(GetDispDigits)                         \
    X(GroupUpDown)                           \
    X(LimitDeal)                             \
    X(LimitOverTurnDeal)                     \
    X(Menu0AddMenuLevel)                     \
    X(Menu0OnEnter)                          \
    X(Menu0OnPrg)                            \
    X(Menu0OnShift)                          \
    X(Menu0OnUpDown)                         \
    X(Menu1OnDown)                           \
    X(Menu1OnEnter)                          \
    X(Menu1OnPrg)                            \
    X(Menu1OnShift)                          \
    X(Menu1OnUp)                             \
    X(Menu1OnUpDown)                         \
    X(Menu2OnDown)                           \
    X(Menu2OnEnter)                          \
    X(Menu2OnPrg)                            \
    X(Menu2OnShift)                          \
    X(Menu2OnUp)                             \
    X(Menu2OnUpDown)                         \
    X(Menu3OnDown)                           \
    X(Menu3OnEnter)                          \
    X(Menu3OnPrg)                            \
    X(Menu3OnShift)                          \
    X(Menu3OnUp)                             \
    X(Menu3OnUpDown)                         \
    X(MenuInit)                              \
    X(MenuModeDeal)                          \
    X(MenuModeSwitch)                        \
    X(MenuOnMfk)                             \
    X(MenuOnQuick)                           \
    X(MenuOnRun)                             \
    X(MenuOnStop)                            \
    X(MenuPwdHint2Input)                     \
    X(MenuPwdHintOnQuick)                    \
    X(MenuPwdHintOnShift)                    \
    X(MenuPwdInputOnDown)                    \
    X(MenuPwdInputOnEnter)                   \
    X(MenuPwdInputOnQuick)                   \
    X(MenuPwdInputOnShift)                   \
    X(MenuPwdInputOnUp)                      \
    X(MenuPwdInputOnUpDown)                  \
    X(MenuPwdOnPrg)                          \
    X(ModifyFunccodeEnter)                   \
    X(ModifyFunccodeUpDown)                  \
    X(MotorDebugFcDeal)                      \
    X(NoSameDeal)                            \
    X(OverTurnDeal)                          \
    X(UpdataFuncCodeGrade)                   \
    X(UpdateDisplayBufferAttribute)          \
    X(UpdateDisplayBufferVisualDiFunc)       \
    X(UpdateDisplayBufferVisualIoStatus)     \
    X(UpdateErrorDisplayBuffer)              \
    X(UpdateGroupDisplay)                    \
    X(UpdateMenu0DispStatus)                 \
    X(UpdateMenu0DisplayBuffer)              \
    X(UpdateMenu1DisplayBuffer)              \
    X(UpdateMenu2DisplayBuffer)              \
    X(UpdateMenu3DisplayBuffer)              \
    X(UpdateMenuPwdHintDisplayBuffer)        \
    X(UpdateMenuPwdInputDisplayBuffer)       \
    X(UpdateTuneDisplayBuffer)               \
    X(UpdateU0Data)                          \
    X(ValidateTuneCmd)                       \
    X(cycleShiftDeal)

#define FMENU_FN_N_ONE(n) + 1
enum { FMENU_FN_N = 0 FMENU_FN_LIST(FMENU_FN_N_ONE) };

static void (*g_fnid[2][FMENU_FN_N])(void);
static const char *g_fnname[FMENU_FN_N];

#define FMENU_FN_NAME(n) #n,
static const char *g_fnname_init[FMENU_FN_N] = { FMENU_FN_LIST(FMENU_FN_NAME) };

static void init_fn_ids(void)
{
    int k = 0;
    int i;

    for (i = 0; i < FMENU_FN_N; i++)
    {
        g_fnname[i] = g_fnname_init[i];
    }

/* 只做地址比较, 不通过这个类型调用 —— C11 6.3.2.3p8 允许函数指针类型的互转。 */
#define FMENU_ID_ONE(n)                                              \
    g_fnid[0][k] = (void (*)(void))ref_##n;                          \
    g_fnid[1][k] = (void (*)(void))new_##n;                          \
    k++;
    FMENU_FN_LIST(FMENU_ID_ONE)
#undef FMENU_ID_ONE

    if (k != FMENU_FN_N)
    {
        printf("FATAL: 函数同一性表长度 %d != %d\n", k, FMENU_FN_N);
        exit(2);
    }
}

/* ---- 模块自己的导出 (每侧一份) ---- */

/* 29 个 Uint16 标量。 */
#define FMENU_U16_LIST(X)              \
    X(accDecFrqPrcFlag)                \
    X(accDecFrqTicker)                 \
    X(bFrqDigitalDone4WaitDelay)       \
    X(checkMenuModeCmd)                \
    X(checkMenuModePara)               \
    X(checkMenuModeSerachNone)         \
    X(curFcDispDigits)                 \
    X(currentOcDisp)                   \
    X(frqAimDisp)                      \
    X(frqAimPLCDisp)                   \
    X(frqDisp)                         \
    X(frqPLCDisp)                      \
    X(frqRunDisp)                      \
    X(frqXDisp)                        \
    X(frqYDisp)                        \
    X(groupHideChkOkFlag)              \
    X(groupHidePwdStatus)              \
    X(itDisp)                          \
    X(loadSpeedDisp)                   \
    X(menu3Number)                     \
    X(menuPwdNumber)                   \
    X(outCurrentDisp)                  \
    X(outVoltageDisp)                  \
    X(pcOriginDisp)                    \
    X(pidFuncFdbDisp)                  \
    X(pidFuncRefDisp)                  \
    X(pulseInFrqDisp)                  \
    X(superFactoryPass)                \
    X(ticker4LowerDisp)

#define FMENU_U16_N_ONE(n) + 1
enum { FMENU_U16_N = 0 FMENU_U16_LIST(FMENU_U16_N_ONE) };

#define FMENU_DECL_U16(n) extern Uint16 ref_##n, new_##n;
FMENU_U16_LIST(FMENU_DECL_U16)
#undef FMENU_DECL_U16

/* 枚举 / 有符号标量。枚举在宿主上是 32 位, 所以**必须**按真类型声明 —— 声明成
 * Uint16 会只读到低 16 位 (对当前取值恰好也一样, 但那是巧合, 不该依赖)。 */
extern enum MENU0_DISP_STATUS        ref_menu0DispStatus, new_menu0DispStatus;
extern enum MENU_LEVEL               ref_menuLevel, new_menuLevel;
extern enum MENU_MODE                ref_menuMode, new_menuMode;
extern enum MENU_MODE                ref_menuModeTmp, new_menuModeTmp;
extern enum MENU_MODE                ref_menuModeOld, new_menuModeOld;
extern enum MENU_MODE_OPERATE        ref_menuModeStatus, new_menuModeStatus;
extern enum FAC_PASS_RANDOM_VIEW_OPERATE ref_facPassViewStatus, new_facPassViewStatus;
extern enum CHECK_MENU_MODE_DEAL     ref_checkMenuModeDealStatus, new_checkMenuModeDealStatus;
extern int16                         ref_userMenuModeFcIndex, new_userMenuModeFcIndex;

/* factoryPwdStatus 的类型 enum FACTORY_PWD_STATUS 是 f_menu.c 的文件局部类型,
 * 头文件里没有, 只能镜像一份 (值必须逐个对应)。宿主上两者都是 int。 */
enum FMENU_FACTORY_PWD_STATUS
{
    FMENU_FACTORY_PWD_LOCK,
    FMENU_FACTORY_PWD_UNLOCK
};
extern enum FMENU_FACTORY_PWD_STATUS ref_factoryPwdStatus, new_factoryPwdStatus;

extern Uint32 ref_torqueCurrentAct, new_torqueCurrentAct;

/* 结构体 —— 三个文件局部类型同样要镜像。 */
struct FMENU_CURRENT_FUNC_CODE
{
    Uint16 index;
    Uint16 group;
    Uint16 grade;
};
struct FMENU_GROUP_DISPLAY
{
    Uint16 dispF;
    Uint16 disp0;
};
struct FMENU_FC_MOTOR_DEBUG
{
    Uint16 fc;
    Uint16 u;
};
struct FMENU_MENU_FUNC_CODE
{
    Uint16 group;
    Uint16 grade;
};

extern struct FMENU_CURRENT_FUNC_CODE ref_curFc, new_curFc;
extern struct FMENU_GROUP_DISPLAY    ref_groupDisplay, new_groupDisplay;
extern struct FMENU_FC_MOTOR_DEBUG    ref_motorDebugFc, new_motorDebugFc;
extern LowPassFilter                 ref_torQueFrqDispLpf, new_torQueFrqDispLpf;
extern struct MENU_ATTRIBUTE         ref_menuAttri[MENU_LEVEL_NUM];
extern struct MENU_ATTRIBUTE         new_menuAttri[MENU_LEVEL_NUM];
extern struct FMENU_MENU_FUNC_CODE   ref_menuFc[MENU_MODE_MAX + 1];
extern struct FMENU_MENU_FUNC_CODE   new_menuFc[MENU_MODE_MAX + 1];

/* .econst 表 */
extern const int16  ref_decNumber[5], new_decNumber[5];
extern const int16  ref_hexNumber[4], new_hexNumber[4];
extern const int16  ref_deltaK[3], new_deltaK[3];
extern const Uint16 ref_diFuncIndex[18], new_diFuncIndex[18];
extern const Uint16 ref_frqSrcFuncIndex[2], new_frqSrcFuncIndex[2];
extern const Uint16 ref_stopDispIndex[STOP_DISPLAY_NUM], new_stopDispIndex[STOP_DISPLAY_NUM];
extern const Uint16 ref_commDispIndex[COMM_PARA_NUM], new_commDispIndex[COMM_PARA_NUM];
extern const union FUNC_ATTRIBUTE ref_dispAttributeU0[U0NUM], new_dispAttributeU0[U0NUM];
extern const sysMenu ref_menu[MENU_LEVEL_NUM], new_menu[MENU_LEVEL_NUM];

/* ---- 用例种类 ---- */

enum FMENU_KIND
{
    K_FCDIGIT2BIN, K_LIMITDEAL, K_NOSAME, K_OVERTURN, K_LIMITOVTURN, K_GETDIGITS,
    K_MODUPDOWN, K_MODENTER,
    K_UPDATTR, K_UPDGROUP, K_UPDMENU0ST, K_UPDMENU0, K_UPDMENU1, K_UPDMENU2,
    K_UPDMENU3, K_UPDERR, K_UPDTUNE, K_UPDPWDHINT, K_UPDPWDINPUT,
    K_UPDIOSTATUS, K_UPDDIFUNC, K_UPDU0,
    K_DISPDATA, K_CYCLESHIFT, K_MOTORDEBUG,
    K_UPDGRADE, K_GROUPUPDOWN, K_DEALUSERGRADE, K_DEALCHECKGRADE,
    K_MENUINIT, K_MENUMODESW, K_MENUMODEDEAL, K_MENU0ADDLEVEL,
    K_MENU0PRG, K_MENU1PRG, K_MENU2PRG, K_MENU3PRG,
    K_MENU0ENTER, K_MENU1ENTER, K_MENU2ENTER, K_MENU3ENTER,
    K_MENU0UP, K_MENU1UP, K_MENU2UP, K_MENU3UP,
    K_MENU1DOWN, K_MENU2DOWN, K_MENU3DOWN,
    K_MENU1UPDN, K_MENU2UPDN, K_MENU3UPDN,
    K_MENU0SHIFT, K_MENU1SHIFT, K_MENU2SHIFT, K_MENU3SHIFT,
    K_MENUMFK, K_MENURUN, K_MENUSTOP, K_MENUQUICK,
    K_PWDONPRG, K_PWDHINT2INPUT, K_PWDHINTONSHIFT, K_PWDHINTONQUICK,
    K_PWDINPUTENTER, K_PWDINPUTUP, K_PWDINPUTDOWN, K_PWDINPUTUPDN,
    K_PWDINPUTSHIFT, K_PWDINPUTQUICK,
    K_VALIDTUNE,
    K_N
};

static enum FMENU_KIND g_kind;

static Uint16 (*g_fc2bin[2])(Uint16);
static Uint16 (*g_limit[2])(Uint16, Uint16, Uint16, Uint16, int16);
static Uint16 (*g_nosame[2])(Uint16, const Uint16[], int16, int16, int16, int16, int16);
static Uint16 (*g_overturn[2])(Uint16, Uint16, Uint16, Uint16);
static Uint16 (*g_limitovt[2])(const Uint16[], Uint16, Uint16, Uint16, Uint16);
static Uint16 (*g_getdigits[2])(Uint16);
static Uint16 (*g_modupdown[2])(Uint16, Uint16 *, int16);
static Uint16 (*g_modenter[2])(Uint16, Uint16);
static void   (*g_updattr[2])(const Uint16, union FUNC_ATTRIBUTE);
static void   (*g_updgroup[2])(Uint16);
static void   (*g_updmenust[2])(void);
static void   (*g_updmenu[2][4])(void);
static void   (*g_upderr[2])(void);
static void   (*g_updtune[2])(void);
static void   (*g_updpwdhint[2])(void);
static void   (*g_updpwdinput[2])(void);
static void   (*g_updio[2])(Uint32);
static void   (*g_upddifunc[2])(Uint16, Uint32);
static void   (*g_updu0[2])(void);
static void   (*g_dispdata[2])(void);
static void   (*g_cycleshift[2])(Uint16);
static void   (*g_motordebug[2])(void);
static void   (*g_updgrade[2])(Uint16 []);
static Uint16 (*g_groupupdown[2])(const Uint16[], Uint16, Uint16);
static void   (*g_dealuser[2])(Uint16);
static void   (*g_dealcheck[2])(Uint16);
static void   (*g_menuinit[2])(void);
static void   (*g_menumodesw[2])(void);
static void   (*g_menumodedeal[2])(void);
static void   (*g_menu0add[2])(void);
static void   (*g_menuprg[2][4])(void);
static void   (*g_menuenter[2][4])(void);
static void   (*g_menushift[2][4])(void);
static void   (*g_menuupdown[2][4])(void);
static void   (*g_menudown[2][3])(void);
static void   (*g_menufk[2])(void);
static void   (*g_menurun[2])(void);
static void   (*g_menustop[2])(void);
static void   (*g_menuquick[2])(void);
static void   (*g_pwdprg[2])(void);
static void   (*g_pwdhint2input[2])(void);
static void   (*g_pwdhintshift[2])(void);
static void   (*g_pwdhintquick[2])(void);
static void   (*g_pwdinputenter[2])(void);
static void   (*g_pwdinputup[2])(void);
static void   (*g_pwdinputdown[2])(void);
static void   (*g_pwdinputupdown[2])(Uint16);
static void   (*g_pwdinputshift[2])(void);
static void   (*g_pwdinputquick[2])(void);
static Uint16 (*g_validtune[2])(Uint16, Uint16);

/* 按侧索引的 Uint16 导出。 */
static Uint16 *g_u16[2][FMENU_U16_N];

static void init_ptrs(void)
{
    int i = 0;

#define FMENU_FILL_U16(n)                          \
    g_u16[0][i] = &ref_##n;                        \
    g_u16[1][i] = &new_##n;                        \
    i++;
    FMENU_U16_LIST(FMENU_FILL_U16)
#undef FMENU_FILL_U16

    g_fc2bin[0] = ref_FcDigit2Bin;                   g_fc2bin[1] = new_FcDigit2Bin;
    g_limit[0] = ref_LimitDeal;                      g_limit[1] = new_LimitDeal;
    g_nosame[0] = ref_NoSameDeal;                    g_nosame[1] = new_NoSameDeal;
    g_overturn[0] = ref_OverTurnDeal;                g_overturn[1] = new_OverTurnDeal;
    g_limitovt[0] = ref_LimitOverTurnDeal;           g_limitovt[1] = new_LimitOverTurnDeal;
    g_getdigits[0] = ref_GetDispDigits;              g_getdigits[1] = new_GetDispDigits;
    g_modupdown[0] = ref_ModifyFunccodeUpDown;       g_modupdown[1] = new_ModifyFunccodeUpDown;
    g_modenter[0] = ref_ModifyFunccodeEnter;         g_modenter[1] = new_ModifyFunccodeEnter;
    g_updattr[0] = ref_UpdateDisplayBufferAttribute; g_updattr[1] = new_UpdateDisplayBufferAttribute;
    g_updgroup[0] = ref_UpdateGroupDisplay;          g_updgroup[1] = new_UpdateGroupDisplay;
    g_updmenust[0] = ref_UpdateMenu0DispStatus;      g_updmenust[1] = new_UpdateMenu0DispStatus;
    g_updmenu[0][0] = ref_UpdateMenu0DisplayBuffer;  g_updmenu[1][0] = new_UpdateMenu0DisplayBuffer;
    g_updmenu[0][1] = ref_UpdateMenu1DisplayBuffer;  g_updmenu[1][1] = new_UpdateMenu1DisplayBuffer;
    g_updmenu[0][2] = ref_UpdateMenu2DisplayBuffer;  g_updmenu[1][2] = new_UpdateMenu2DisplayBuffer;
    g_updmenu[0][3] = ref_UpdateMenu3DisplayBuffer;  g_updmenu[1][3] = new_UpdateMenu3DisplayBuffer;
    g_upderr[0] = ref_UpdateErrorDisplayBuffer;      g_upderr[1] = new_UpdateErrorDisplayBuffer;
    g_updtune[0] = ref_UpdateTuneDisplayBuffer;      g_updtune[1] = new_UpdateTuneDisplayBuffer;
    g_updpwdhint[0] = ref_UpdateMenuPwdHintDisplayBuffer;
    g_updpwdhint[1] = new_UpdateMenuPwdHintDisplayBuffer;
    g_updpwdinput[0] = ref_UpdateMenuPwdInputDisplayBuffer;
    g_updpwdinput[1] = new_UpdateMenuPwdInputDisplayBuffer;
    g_updio[0] = ref_UpdateDisplayBufferVisualIoStatus;
    g_updio[1] = new_UpdateDisplayBufferVisualIoStatus;
    g_upddifunc[0] = ref_UpdateDisplayBufferVisualDiFunc;
    g_upddifunc[1] = new_UpdateDisplayBufferVisualDiFunc;
    g_updu0[0] = ref_UpdateU0Data;                   g_updu0[1] = new_UpdateU0Data;
    g_dispdata[0] = ref_DispDataDeal;                g_dispdata[1] = new_DispDataDeal;
    g_cycleshift[0] = ref_cycleShiftDeal;            g_cycleshift[1] = new_cycleShiftDeal;
    g_motordebug[0] = ref_MotorDebugFcDeal;          g_motordebug[1] = new_MotorDebugFcDeal;
    g_updgrade[0] = ref_UpdataFuncCodeGrade;         g_updgrade[1] = new_UpdataFuncCodeGrade;
    g_groupupdown[0] = ref_GroupUpDown;              g_groupupdown[1] = new_GroupUpDown;
    g_dealuser[0] = ref_DealUserMenuModeGroupGrade;  g_dealuser[1] = new_DealUserMenuModeGroupGrade;
    g_dealcheck[0] = ref_DealCheckMenuModeGroupGrade;
    g_dealcheck[1] = new_DealCheckMenuModeGroupGrade;
    g_menuinit[0] = ref_MenuInit;                    g_menuinit[1] = new_MenuInit;
    g_menumodesw[0] = ref_MenuModeSwitch;            g_menumodesw[1] = new_MenuModeSwitch;
    g_menumodedeal[0] = ref_MenuModeDeal;            g_menumodedeal[1] = new_MenuModeDeal;
    g_menu0add[0] = ref_Menu0AddMenuLevel;           g_menu0add[1] = new_Menu0AddMenuLevel;
    g_menuprg[0][0] = ref_Menu0OnPrg;   g_menuprg[1][0] = new_Menu0OnPrg;
    g_menuprg[0][1] = ref_Menu1OnPrg;   g_menuprg[1][1] = new_Menu1OnPrg;
    g_menuprg[0][2] = ref_Menu2OnPrg;   g_menuprg[1][2] = new_Menu2OnPrg;
    g_menuprg[0][3] = ref_Menu3OnPrg;   g_menuprg[1][3] = new_Menu3OnPrg;
    g_menuenter[0][0] = ref_Menu0OnEnter; g_menuenter[1][0] = new_Menu0OnEnter;
    g_menuenter[0][1] = ref_Menu1OnEnter; g_menuenter[1][1] = new_Menu1OnEnter;
    g_menuenter[0][2] = ref_Menu2OnEnter; g_menuenter[1][2] = new_Menu2OnEnter;
    g_menuenter[0][3] = ref_Menu3OnEnter; g_menuenter[1][3] = new_Menu3OnEnter;
    g_menushift[0][0] = ref_Menu0OnShift; g_menushift[1][0] = new_Menu0OnShift;
    g_menushift[0][1] = ref_Menu1OnShift; g_menushift[1][1] = new_Menu1OnShift;
    g_menushift[0][2] = ref_Menu2OnShift; g_menushift[1][2] = new_Menu2OnShift;
    g_menushift[0][3] = ref_Menu3OnShift; g_menushift[1][3] = new_Menu3OnShift;
    g_menuupdown[0][0] = ref_Menu0OnUpDown; g_menuupdown[1][0] = new_Menu0OnUpDown;
    g_menuupdown[0][1] = ref_Menu1OnUp;     g_menuupdown[1][1] = new_Menu1OnUp;
    g_menuupdown[0][2] = ref_Menu2OnUp;     g_menuupdown[1][2] = new_Menu2OnUp;
    g_menuupdown[0][3] = ref_Menu3OnUp;     g_menuupdown[1][3] = new_Menu3OnUp;
    g_menudown[0][0] = ref_Menu1OnDown;     g_menudown[1][0] = new_Menu1OnDown;
    g_menudown[0][1] = ref_Menu2OnDown;     g_menudown[1][1] = new_Menu2OnDown;
    g_menudown[0][2] = ref_Menu3OnDown;     g_menudown[1][2] = new_Menu3OnDown;
    g_menufk[0] = ref_MenuOnMfk;                     g_menufk[1] = new_MenuOnMfk;
    g_menurun[0] = ref_MenuOnRun;                    g_menurun[1] = new_MenuOnRun;
    g_menustop[0] = ref_MenuOnStop;                  g_menustop[1] = new_MenuOnStop;
    g_menuquick[0] = ref_MenuOnQuick;                g_menuquick[1] = new_MenuOnQuick;
    g_pwdprg[0] = ref_MenuPwdOnPrg;                  g_pwdprg[1] = new_MenuPwdOnPrg;
    g_pwdhint2input[0] = ref_MenuPwdHint2Input;      g_pwdhint2input[1] = new_MenuPwdHint2Input;
    g_pwdhintshift[0] = ref_MenuPwdHintOnShift;      g_pwdhintshift[1] = new_MenuPwdHintOnShift;
    g_pwdhintquick[0] = ref_MenuPwdHintOnQuick;      g_pwdhintquick[1] = new_MenuPwdHintOnQuick;
    g_pwdinputenter[0] = ref_MenuPwdInputOnEnter;    g_pwdinputenter[1] = new_MenuPwdInputOnEnter;
    g_pwdinputup[0] = ref_MenuPwdInputOnUp;          g_pwdinputup[1] = new_MenuPwdInputOnUp;
    g_pwdinputdown[0] = ref_MenuPwdInputOnDown;      g_pwdinputdown[1] = new_MenuPwdInputOnDown;
    g_pwdinputupdown[0] = ref_MenuPwdInputOnUpDown;
    g_pwdinputupdown[1] = new_MenuPwdInputOnUpDown;
    g_pwdinputshift[0] = ref_MenuPwdInputOnShift;    g_pwdinputshift[1] = new_MenuPwdInputOnShift;
    g_pwdinputquick[0] = ref_MenuPwdInputOnQuick;    g_pwdinputquick[1] = new_MenuPwdInputOnQuick;
    g_validtune[0] = ref_ValidateTuneCmd;            g_validtune[1] = new_ValidateTuneCmd;
}

/* ---- 输入快照 / 结果快照 ---- */

struct FMENU_IN
{
    /* 模块自己的导出 (既是输入也是输出) */
    Uint16 u16[FMENU_U16_N];
    Uint32 torqueCurrentAct;

    enum MENU0_DISP_STATUS menu0DispStatus;
    enum MENU_LEVEL menuLevel;
    enum MENU_MODE menuMode, menuModeTmp, menuModeOld;
    enum MENU_MODE_OPERATE menuModeStatus;
    enum FAC_PASS_RANDOM_VIEW_OPERATE facPassViewStatus;
    enum CHECK_MENU_MODE_DEAL checkMenuModeDealStatus;
    enum FMENU_FACTORY_PWD_STATUS factoryPwdStatus;
    int16 userMenuModeFcIndex;

    struct FMENU_CURRENT_FUNC_CODE curFc;
    struct FMENU_GROUP_DISPLAY groupDisplay;
    struct FMENU_FC_MOTOR_DEBUG motorDebugFc;
    LowPassFilter lpf;
    struct MENU_ATTRIBUTE menuAttri[MENU_LEVEL_NUM];
    struct FMENU_MENU_FUNC_CODE menuFc[MENU_MODE_MAX + 1];

    /* 共享的 mock 状态 */
    FUNCCODE_ALL fc;
    Uint16 displayBuffer[DISPLAY_8LED_NUM + 1];
    union RUN_FLAG runFlag;
    Uint16 runMode, runSrc, tuneCmd, keyFunc, upDownDelta;
    union INVT_STATUS invtStatus;
    Uint16 setRunLostTime;
    int16 torqueAim;
    struct DI_FUNC diFunc;
    struct DI_STATUS_ALL diStatus;
    struct DO_STATUS_ALL doStatus;
    struct AI_DEAL aiDeal[AI_NUMBER];
    struct CUR_TIME curTime;
    union ERROR_ATTRIBUTE errorAttribute;
    Uint16 errorCode, errorInfo;
    int32 frq, frqAim, frqDroop, frqRun;
    Uint16 frqFdb, frqKeyUpDownDelta, bFrqDigital, maxFrq, plcStep;
    PID_FUNC pidFunc;
    Uint16 generatrixVoltage, currentPu, enCoderPosition, ABZPos;
    int16 gPhiRtDisp, outPower;
    Uint32 pulseInFrq;
    Uint16 lineSpeed, memoryValue, torqueCurrent, pmsmRotorPos;
    struct MOTOR_FC motorFc;
    enum MOTOR_SN motorSn;
    Uint16 temperature, ZCount, vfSeparateVol, vfSeprateVolAim;
    Uint16 ticker05msFuncEachDisp[4];
    Uint16 ticker05msFuncInternalDisp[4];
    P2P_DATA_STRUCT p2pData;
    struct INV_PARA invPara;
    Uint16 rwMode, rwModeTmp, qNum;
    Uint16 q[Q_SNAP];
    Uint16 gradeCur[GRADE_N];

    /* 替身的输入 */
    Uint16 validateInvTypeRet;

    /* 被测函数的入参 */
    Uint16 arg0, arg1, arg2, arg3, arg4;
    int16  sarg;
    Uint32 larg;
    union FUNC_ATTRIBUTE attr;
    const Uint16 *tbl;
    Uint16 dataLocal;
};

struct FMENU_OUT
{
    Uint16 ret;
    Uint16 dataOut;

    Uint16 u16[FMENU_U16_N];
    Uint32 torqueCurrentAct;

    enum MENU0_DISP_STATUS menu0DispStatus;
    enum MENU_LEVEL menuLevel;
    enum MENU_MODE menuMode, menuModeTmp, menuModeOld;
    enum MENU_MODE_OPERATE menuModeStatus;
    enum FAC_PASS_RANDOM_VIEW_OPERATE facPassViewStatus;
    enum CHECK_MENU_MODE_DEAL checkMenuModeDealStatus;
    enum FMENU_FACTORY_PWD_STATUS factoryPwdStatus;
    int16 userMenuModeFcIndex;

    struct FMENU_CURRENT_FUNC_CODE curFc;
    struct FMENU_GROUP_DISPLAY groupDisplay;
    struct FMENU_FC_MOTOR_DEBUG motorDebugFc;
    LowPassFilter lpf;
    struct MENU_ATTRIBUTE menuAttri[MENU_LEVEL_NUM];
    struct FMENU_MENU_FUNC_CODE menuFc[MENU_MODE_MAX + 1];

    Uint16 tblDec[5], tblHex[4], tblDelta[3];
    Uint16 tblDiFunc[18], tblFrqSrc[2];
    Uint16 tblStop[STOP_DISPLAY_NUM], tblComm[COMM_PARA_NUM];
    Uint16 tblDispU0[U0NUM];
    int menuSlot[MENU_LEVEL_NUM][MENU_SLOT_N];

    FUNCCODE_ALL fc;
    Uint16 displayBuffer[DISPLAY_8LED_NUM + 1];
    Uint16 runFlagAll, invtAll;
    Uint16 keyFunc, tuneCmd, frqKeyUpDownDelta;
    Uint16 rwMode, rwModeTmp, qNum;
    Uint16 q[Q_SNAP];
    Uint16 gradeCur[GRADE_N];
    Uint16 invParaAll[sizeof(struct INV_PARA) / sizeof(Uint16)];
    Uint16 motorFcAll[sizeof(struct MOTOR_FC) / sizeof(Uint16)];

    /* 替身记账 */
    Uint16 digitCalls, digitLastArg, digitLastMode;
    Uint16 getInitCalls, getInitLastIndex, getInitLastType;
    Uint16 limitOtherCalls, limitOtherLastIndex;
    Uint16 clearRecordCalls;
    Uint16 motorPowerCalls, motorPowerLastPower, motorPowerLastSn;
    Uint16 invTypeRelatedCalls;
    Uint16 validateInvTypeCalls;
    Uint16 resetUpDownCalls;
    Uint16 lpfCalls, lpfLastT, lpfLastIn;
};

static struct FMENU_IN  g_in;
static struct FMENU_OUT S[2];

/* ---- 当前用例描述 ---- */

static const char *g_fn;
static Uint16 g_arg0, g_arg1;

static const char *g_kind_name[K_N];

static void init_kind_names(void)
{
    g_kind_name[K_FCDIGIT2BIN] = "FcDigit2Bin";
    g_kind_name[K_LIMITDEAL] = "LimitDeal";
    g_kind_name[K_NOSAME] = "NoSameDeal";
    g_kind_name[K_OVERTURN] = "OverTurnDeal";
    g_kind_name[K_LIMITOVTURN] = "LimitOverTurnDeal";
    g_kind_name[K_GETDIGITS] = "GetDispDigits";
    g_kind_name[K_MODUPDOWN] = "ModifyFunccodeUpDown";
    g_kind_name[K_MODENTER] = "ModifyFunccodeEnter";
    g_kind_name[K_UPDATTR] = "UpdateDisplayBufferAttribute";
    g_kind_name[K_UPDGROUP] = "UpdateGroupDisplay";
    g_kind_name[K_UPDMENU0ST] = "UpdateMenu0DispStatus";
    g_kind_name[K_UPDMENU0] = "UpdateMenu0DisplayBuffer";
    g_kind_name[K_UPDMENU1] = "UpdateMenu1DisplayBuffer";
    g_kind_name[K_UPDMENU2] = "UpdateMenu2DisplayBuffer";
    g_kind_name[K_UPDMENU3] = "UpdateMenu3DisplayBuffer";
    g_kind_name[K_UPDERR] = "UpdateErrorDisplayBuffer";
    g_kind_name[K_UPDTUNE] = "UpdateTuneDisplayBuffer";
    g_kind_name[K_UPDPWDHINT] = "UpdateMenuPwdHintDisplayBuffer";
    g_kind_name[K_UPDPWDINPUT] = "UpdateMenuPwdInputDisplayBuffer";
    g_kind_name[K_UPDIOSTATUS] = "UpdateDisplayBufferVisualIoStatus";
    g_kind_name[K_UPDDIFUNC] = "UpdateDisplayBufferVisualDiFunc";
    g_kind_name[K_UPDU0] = "UpdateU0Data";
    g_kind_name[K_DISPDATA] = "DispDataDeal";
    g_kind_name[K_CYCLESHIFT] = "cycleShiftDeal";
    g_kind_name[K_MOTORDEBUG] = "MotorDebugFcDeal";
    g_kind_name[K_UPDGRADE] = "UpdataFuncCodeGrade";
    g_kind_name[K_GROUPUPDOWN] = "GroupUpDown";
    g_kind_name[K_DEALUSERGRADE] = "DealUserMenuModeGroupGrade";
    g_kind_name[K_DEALCHECKGRADE] = "DealCheckMenuModeGroupGrade";
    g_kind_name[K_MENUINIT] = "MenuInit";
    g_kind_name[K_MENUMODESW] = "MenuModeSwitch";
    g_kind_name[K_MENUMODEDEAL] = "MenuModeDeal";
    g_kind_name[K_MENU0ADDLEVEL] = "Menu0AddMenuLevel";
    g_kind_name[K_MENU0PRG] = "Menu0OnPrg";
    g_kind_name[K_MENU1PRG] = "Menu1OnPrg";
    g_kind_name[K_MENU2PRG] = "Menu2OnPrg";
    g_kind_name[K_MENU3PRG] = "Menu3OnPrg";
    g_kind_name[K_MENU0ENTER] = "Menu0OnEnter";
    g_kind_name[K_MENU1ENTER] = "Menu1OnEnter";
    g_kind_name[K_MENU2ENTER] = "Menu2OnEnter";
    g_kind_name[K_MENU3ENTER] = "Menu3OnEnter";
    g_kind_name[K_MENU0UP] = "Menu0OnUpDown";
    g_kind_name[K_MENU1UP] = "Menu1OnUp";
    g_kind_name[K_MENU2UP] = "Menu2OnUp";
    g_kind_name[K_MENU3UP] = "Menu3OnUp";
    g_kind_name[K_MENU1DOWN] = "Menu1OnDown";
    g_kind_name[K_MENU2DOWN] = "Menu2OnDown";
    g_kind_name[K_MENU3DOWN] = "Menu3OnDown";
    g_kind_name[K_MENU1UPDN] = "Menu1OnUpDown";
    g_kind_name[K_MENU2UPDN] = "Menu2OnUpDown";
    g_kind_name[K_MENU3UPDN] = "Menu3OnUpDown";
    g_kind_name[K_MENU0SHIFT] = "Menu0OnShift";
    g_kind_name[K_MENU1SHIFT] = "Menu1OnShift";
    g_kind_name[K_MENU2SHIFT] = "Menu2OnShift";
    g_kind_name[K_MENU3SHIFT] = "Menu3OnShift";
    g_kind_name[K_MENUMFK] = "MenuOnMfk";
    g_kind_name[K_MENURUN] = "MenuOnRun";
    g_kind_name[K_MENUSTOP] = "MenuOnStop";
    g_kind_name[K_MENUQUICK] = "MenuOnQuick";
    g_kind_name[K_PWDONPRG] = "MenuPwdOnPrg";
    g_kind_name[K_PWDHINT2INPUT] = "MenuPwdHint2Input";
    g_kind_name[K_PWDHINTONSHIFT] = "MenuPwdHintOnShift";
    g_kind_name[K_PWDHINTONQUICK] = "MenuPwdHintOnQuick";
    g_kind_name[K_PWDINPUTENTER] = "MenuPwdInputOnEnter";
    g_kind_name[K_PWDINPUTUP] = "MenuPwdInputOnUp";
    g_kind_name[K_PWDINPUTDOWN] = "MenuPwdInputOnDown";
    g_kind_name[K_PWDINPUTUPDN] = "MenuPwdInputOnUpDown";
    g_kind_name[K_PWDINPUTSHIFT] = "MenuPwdInputOnShift";
    g_kind_name[K_PWDINPUTQUICK] = "MenuPwdInputOnQuick";
    g_kind_name[K_VALIDTUNE] = "ValidateTuneCmd";
}

static void set_kind(enum FMENU_KIND k, Uint16 a0, Uint16 a1,
                        Uint16 a2, Uint16 a3, Uint16 a4)
{
    g_kind = k;
    g_arg0 = a0;
    g_arg1 = a1;
    g_fn = g_kind_name[k];
    /* 五个标量入参一次设全 —— 分派 switch 读的就是这几个。
     * 有些调用点下面还会手写 g_in.argN, 那是同一批值的复述, 无害。 */
    g_in.arg0 = a0;
    g_in.arg1 = a1;
    g_in.arg2 = a2;
    g_in.arg3 = a3;
    g_in.arg4 = a4;
}

static void install(int side)
{
    int i;

    for (i = 0; i < FMENU_U16_N; i++)
    {
        *g_u16[side][i] = g_in.u16[i];
    }
    if (side == 0)
    {
        ref_torqueCurrentAct = g_in.torqueCurrentAct;
        ref_menu0DispStatus = g_in.menu0DispStatus;
        ref_menuLevel = g_in.menuLevel;
        ref_menuMode = g_in.menuMode;
        ref_menuModeTmp = g_in.menuModeTmp;
        ref_menuModeOld = g_in.menuModeOld;
        ref_menuModeStatus = g_in.menuModeStatus;
        ref_facPassViewStatus = g_in.facPassViewStatus;
        ref_checkMenuModeDealStatus = g_in.checkMenuModeDealStatus;
        ref_factoryPwdStatus = (enum FMENU_FACTORY_PWD_STATUS)g_in.factoryPwdStatus;
        ref_userMenuModeFcIndex = g_in.userMenuModeFcIndex;
        ref_curFc = g_in.curFc;
        ref_groupDisplay = g_in.groupDisplay;
        ref_motorDebugFc = g_in.motorDebugFc;
        ref_torQueFrqDispLpf = g_in.lpf;
        for (i = 0; i < MENU_LEVEL_NUM; i++)
        {
            ref_menuAttri[i] = g_in.menuAttri[i];
        }
        for (i = 0; i <= MENU_MODE_MAX; i++)
        {
            ref_menuFc[i] = g_in.menuFc[i];
        }
    }
    else
    {
        new_torqueCurrentAct = g_in.torqueCurrentAct;
        new_menu0DispStatus = g_in.menu0DispStatus;
        new_menuLevel = g_in.menuLevel;
        new_menuMode = g_in.menuMode;
        new_menuModeTmp = g_in.menuModeTmp;
        new_menuModeOld = g_in.menuModeOld;
        new_menuModeStatus = g_in.menuModeStatus;
        new_facPassViewStatus = g_in.facPassViewStatus;
        new_checkMenuModeDealStatus = g_in.checkMenuModeDealStatus;
        new_factoryPwdStatus = (enum FMENU_FACTORY_PWD_STATUS)g_in.factoryPwdStatus;
        new_userMenuModeFcIndex = g_in.userMenuModeFcIndex;
        new_curFc = g_in.curFc;
        new_groupDisplay = g_in.groupDisplay;
        new_motorDebugFc = g_in.motorDebugFc;
        new_torQueFrqDispLpf = g_in.lpf;
        for (i = 0; i < MENU_LEVEL_NUM; i++)
        {
            new_menuAttri[i] = g_in.menuAttri[i];
        }
        for (i = 0; i <= MENU_MODE_MAX; i++)
        {
            new_menuFc[i] = g_in.menuFc[i];
        }
    }

    /* 共享状态 */
    memcpy(&funcCode, &g_in.fc, sizeof g_in.fc);
    memcpy(displayBuffer, g_in.displayBuffer, sizeof g_in.displayBuffer);
    runFlag = g_in.runFlag;
    runMode = g_in.runMode;
    runSrc = g_in.runSrc;
    tuneCmd = g_in.tuneCmd;
    keyFunc = g_in.keyFunc;
    upDownDelta = g_in.upDownDelta;
    invtStatus = g_in.invtStatus;
    setRunLostTime = g_in.setRunLostTime;
    torqueAim = g_in.torqueAim;
    diFunc = g_in.diFunc;
    diStatus = g_in.diStatus;
    doStatus = g_in.doStatus;
    memcpy(aiDeal, g_in.aiDeal, sizeof g_in.aiDeal);
    curTime = g_in.curTime;
    errorAttribute = g_in.errorAttribute;
    errorCode = g_in.errorCode;
    errorInfo = g_in.errorInfo;
    frq = g_in.frq;
    frqAim = g_in.frqAim;
    frqDroop = g_in.frqDroop;
    frqRun = g_in.frqRun;
    frqFdb = g_in.frqFdb;
    frqKeyUpDownDelta = g_in.frqKeyUpDownDelta;
    bFrqDigital = g_in.bFrqDigital;
    maxFrq = g_in.maxFrq;
    plcStep = g_in.plcStep;
    pidFunc = g_in.pidFunc;
    generatrixVoltage = g_in.generatrixVoltage;
    currentPu = g_in.currentPu;
    enCoderPosition = g_in.enCoderPosition;
    ABZPos = g_in.ABZPos;
    gPhiRtDisp = g_in.gPhiRtDisp;
    outPower = g_in.outPower;
    pulseInFrq = g_in.pulseInFrq;
    lineSpeed = g_in.lineSpeed;
    memoryValue = g_in.memoryValue;
    torqueCurrent = g_in.torqueCurrent;
    pmsmRotorPos = g_in.pmsmRotorPos;
    motorFc = g_in.motorFc;
    motorSn = g_in.motorSn;
    temperature = g_in.temperature;
    ZCount = g_in.ZCount;
    vfSeparateVol = g_in.vfSeparateVol;
    vfSeprateVolAim = g_in.vfSeprateVolAim;
    memcpy(ticker05msFuncEachDisp, g_in.ticker05msFuncEachDisp,
           sizeof g_in.ticker05msFuncEachDisp);
    memcpy(ticker05msFuncInternalDisp, g_in.ticker05msFuncInternalDisp,
           sizeof g_in.ticker05msFuncInternalDisp);
    p2pData = g_in.p2pData;
    invPara = g_in.invPara;
    funcCodeRwMode = g_in.rwMode;
    funcCodeRwModeTmp = g_in.rwModeTmp;
    memcpy(funcCodeOneWriteIndex, g_in.q, sizeof g_in.q);
    funcCodeOneWriteNum = g_in.qNum;
    memcpy(funcCodeGradeCurMenuMode, g_in.gradeCur, sizeof g_in.gradeCur);

    mockValidateInvTypeRet = g_in.validateInvTypeRet;

    mockDigitCalls = 0; mockDigitLastArg = 0; mockDigitLastMode = 0;
    mockGetInitCalls = 0; mockGetInitLastIndex = 0; mockGetInitLastType = 0;
    mockLimitOtherCalls = 0; mockLimitOtherLastIndex = 0;
    mockClearRecordCalls = 0;
    mockMotorPowerCalls = 0; mockMotorPowerLastPower = 0; mockMotorPowerLastSn = 0;
    mockInvTypeRelatedCalls = 0;
    mockValidateInvTypeCalls = 0;
    mockResetUpDownCalls = 0;
    mockLpfCalls = 0; mockLpfLastT = 0; mockLpfLastIn = 0;
}

/* 把 menu[] 表的 10 个槽位解析成"函数序号", 于是两侧的**地址差异**被归一化,
 * 比的是"这个槽位指向哪个函数" —— 正是要看的东西。 */
static int fn_index(int side, const void (*pf)(void))
{
    int i;

    for (i = 0; i < FMENU_FN_N; i++)
    {
        if (g_fnid[side][i] == pf)
        {
            return i;
        }
    }
    return -1;
}

static void snapshot(int side)
{
    struct FMENU_OUT *s = &S[side];
    const sysMenu *m = (side == 0) ? ref_menu : new_menu;
    int i;

    for (i = 0; i < FMENU_U16_N; i++)
    {
        s->u16[i] = *g_u16[side][i];
    }
    s->torqueCurrentAct = (side == 0) ? ref_torqueCurrentAct : new_torqueCurrentAct;

    s->menu0DispStatus = (side == 0) ? ref_menu0DispStatus : new_menu0DispStatus;
    s->menuLevel = (side == 0) ? ref_menuLevel : new_menuLevel;
    s->menuMode = (side == 0) ? ref_menuMode : new_menuMode;
    s->menuModeTmp = (side == 0) ? ref_menuModeTmp : new_menuModeTmp;
    s->menuModeOld = (side == 0) ? ref_menuModeOld : new_menuModeOld;
    s->menuModeStatus = (side == 0) ? ref_menuModeStatus : new_menuModeStatus;
    s->facPassViewStatus = (side == 0) ? ref_facPassViewStatus : new_facPassViewStatus;
    s->checkMenuModeDealStatus =
        (side == 0) ? ref_checkMenuModeDealStatus : new_checkMenuModeDealStatus;
    s->factoryPwdStatus = (side == 0)
        ? (enum FMENU_FACTORY_PWD_STATUS)ref_factoryPwdStatus
        : (enum FMENU_FACTORY_PWD_STATUS)new_factoryPwdStatus;
    s->userMenuModeFcIndex =
        (side == 0) ? ref_userMenuModeFcIndex : new_userMenuModeFcIndex;

    s->curFc = (side == 0) ? ref_curFc : new_curFc;
    s->groupDisplay = (side == 0) ? ref_groupDisplay : new_groupDisplay;
    s->motorDebugFc = (side == 0) ? ref_motorDebugFc : new_motorDebugFc;
    s->lpf = (side == 0) ? ref_torQueFrqDispLpf : new_torQueFrqDispLpf;
    for (i = 0; i < MENU_LEVEL_NUM; i++)
    {
        s->menuAttri[i] = (side == 0) ? ref_menuAttri[i] : new_menuAttri[i];
    }
    for (i = 0; i <= MENU_MODE_MAX; i++)
    {
        s->menuFc[i] = (side == 0) ? ref_menuFc[i] : new_menuFc[i];
    }

    /* .econst 表 */
    for (i = 0; i < 5; i++)
        s->tblDec[i] = (Uint16)((side == 0) ? ref_decNumber[i] : new_decNumber[i]);
    for (i = 0; i < 4; i++)
        s->tblHex[i] = (Uint16)((side == 0) ? ref_hexNumber[i] : new_hexNumber[i]);
    for (i = 0; i < 3; i++)
        s->tblDelta[i] = (Uint16)((side == 0) ? ref_deltaK[i] : new_deltaK[i]);
    for (i = 0; i < 18; i++)
        s->tblDiFunc[i] = (side == 0) ? ref_diFuncIndex[i] : new_diFuncIndex[i];
    for (i = 0; i < 2; i++)
        s->tblFrqSrc[i] = (side == 0) ? ref_frqSrcFuncIndex[i] : new_frqSrcFuncIndex[i];
    for (i = 0; i < STOP_DISPLAY_NUM; i++)
        s->tblStop[i] = (side == 0) ? ref_stopDispIndex[i] : new_stopDispIndex[i];
    for (i = 0; i < COMM_PARA_NUM; i++)
        s->tblComm[i] = (side == 0) ? ref_commDispIndex[i] : new_commDispIndex[i];
    for (i = 0; i < U0NUM; i++)
    {
        s->tblDispU0[i] = (side == 0) ? ref_dispAttributeU0[i].all
                                      : new_dispAttributeU0[i].all;
    }

    /* menu[] 表: 10 个槽位各解析成函数序号 */
    for (i = 0; i < MENU_LEVEL_NUM; i++)
    {
        s->menuSlot[i][0] = fn_index(side, (void (*)(void))m[i].onPrgFunc);
        s->menuSlot[i][1] = fn_index(side, (void (*)(void))m[i].onUpFunc);
        s->menuSlot[i][2] = fn_index(side, (void (*)(void))m[i].onEnterFunc);
        s->menuSlot[i][3] = fn_index(side, (void (*)(void))m[i].onMfkFunc);
        s->menuSlot[i][4] = fn_index(side, (void (*)(void))m[i].onDownFunc);
        s->menuSlot[i][5] = fn_index(side, (void (*)(void))m[i].onShiftFunc);
        s->menuSlot[i][6] = fn_index(side, (void (*)(void))m[i].onRunFunc);
        s->menuSlot[i][7] = fn_index(side, (void (*)(void))m[i].onStopFunc);
        s->menuSlot[i][8] = fn_index(side, (void (*)(void))m[i].onQuickFunc);
        s->menuSlot[i][9] = fn_index(side, (void (*)(void))m[i].UpdateDisplayBuffer);
    }

    /* 共享状态 */
    memcpy(&s->fc, &funcCode, sizeof s->fc);
    memcpy(s->displayBuffer, displayBuffer, sizeof s->displayBuffer);
    s->runFlagAll = runFlag.all;
    s->invtAll = invtStatus.all;
    s->keyFunc = keyFunc;
    s->tuneCmd = tuneCmd;
    s->frqKeyUpDownDelta = frqKeyUpDownDelta;
    s->rwMode = funcCodeRwMode;
    s->rwModeTmp = funcCodeRwModeTmp;
    s->qNum = funcCodeOneWriteNum;
    memcpy(s->q, funcCodeOneWriteIndex, sizeof s->q);
    memcpy(s->gradeCur, funcCodeGradeCurMenuMode, sizeof s->gradeCur);
    memcpy(s->invParaAll, &invPara, sizeof s->invParaAll);
    memcpy(s->motorFcAll, &motorFc, sizeof s->motorFcAll);

    s->digitCalls = mockDigitCalls;
    s->digitLastArg = mockDigitLastArg;
    s->digitLastMode = mockDigitLastMode;
    s->getInitCalls = mockGetInitCalls;
    s->getInitLastIndex = mockGetInitLastIndex;
    s->getInitLastType = mockGetInitLastType;
    s->limitOtherCalls = mockLimitOtherCalls;
    s->limitOtherLastIndex = mockLimitOtherLastIndex;
    s->clearRecordCalls = mockClearRecordCalls;
    s->motorPowerCalls = mockMotorPowerCalls;
    s->motorPowerLastPower = mockMotorPowerLastPower;
    s->motorPowerLastSn = mockMotorPowerLastSn;
    s->invTypeRelatedCalls = mockInvTypeRelatedCalls;
    s->validateInvTypeCalls = mockValidateInvTypeCalls;
    s->resetUpDownCalls = mockResetUpDownCalls;
    s->lpfCalls = mockLpfCalls;
    s->lpfLastT = mockLpfLastT;
    s->lpfLastIn = mockLpfLastIn;
}

/* ---- 差异上报 ---- */

static long g_cases = 0;
static int  g_fails = 0;

static void report(const char *what)
{
    int i;

    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s: %s\n", g_fn, what);
    printf("  arg0=%u arg1=%u data=%u ret=%u/%u dataOut=%u/%u\n",
           g_arg0, g_arg1, g_in.dataLocal,
           S[0].ret, S[1].ret, S[0].dataOut, S[1].dataOut);
    for (i = 0; i < FMENU_U16_N; i++)
    {
        if (S[0].u16[i] != S[1].u16[i])
        {
            printf("  u16[%d] = %u/%u\n", i, S[0].u16[i], S[1].u16[i]);
        }
    }
    printf("  menuLevel=%d/%d menuMode=%d/%d modeTmp=%d/%d modeOld=%d/%d "
           "modeStatus=%d/%d menu0Disp=%d/%d\n",
           (int)S[0].menuLevel, (int)S[1].menuLevel,
           (int)S[0].menuMode, (int)S[1].menuMode,
           (int)S[0].menuModeTmp, (int)S[1].menuModeTmp,
           (int)S[0].menuModeOld, (int)S[1].menuModeOld,
           (int)S[0].menuModeStatus, (int)S[1].menuModeStatus,
           (int)S[0].menu0DispStatus, (int)S[1].menu0DispStatus);
    printf("  curFc=%u/%u/%u vs %u/%u/%u  menu3Number=%u/%u\n",
           S[0].curFc.index, S[0].curFc.group, S[0].curFc.grade,
           S[1].curFc.index, S[1].curFc.group, S[1].curFc.grade,
           S[0].u16[19], S[1].u16[19]);
    printf("  groupDisplay=%u,%u/%u,%u  pwdStatus=%d/%d userFcIdx=%d/%d\n",
           S[0].groupDisplay.dispF, S[0].groupDisplay.disp0,
           S[1].groupDisplay.dispF, S[1].groupDisplay.disp0,
           (int)S[0].factoryPwdStatus, (int)S[1].factoryPwdStatus,
           (int)S[0].userMenuModeFcIndex, (int)S[1].userMenuModeFcIndex);
    printf("  displayBuffer=%04X %04X %04X %04X %04X %04X\n",
           S[0].displayBuffer[0], S[0].displayBuffer[1], S[0].displayBuffer[2],
           S[0].displayBuffer[3], S[0].displayBuffer[4], S[0].displayBuffer[5]);
    printf("                %04X %04X %04X %04X %04X %04X\n",
           S[1].displayBuffer[0], S[1].displayBuffer[1], S[1].displayBuffer[2],
           S[1].displayBuffer[3], S[1].displayBuffer[4], S[1].displayBuffer[5]);
    printf("  calls digit=%u/%u init=%u/%u limitOther=%u/%u clear=%u/%u\n",
           S[0].digitCalls, S[1].digitCalls,
           S[0].getInitCalls, S[1].getInitCalls,
           S[0].limitOtherCalls, S[1].limitOtherCalls,
           S[0].clearRecordCalls, S[1].clearRecordCalls);
    printf("        motorPower=%u/%u invType=%u/%u validateInv=%u/%u "
           "resetUpDown=%u/%u lpf=%u/%u\n",
           S[0].motorPowerCalls, S[1].motorPowerCalls,
           S[0].invTypeRelatedCalls, S[1].invTypeRelatedCalls,
           S[0].validateInvTypeCalls, S[1].validateInvTypeCalls,
           S[0].resetUpDownCalls, S[1].resetUpDownCalls,
           S[0].lpfCalls, S[1].lpfCalls);
    printf("  keyFunc=%u/%u tuneCmd=%u/%u frqKeyUpDownDelta=%u/%u rwModeTmp=%u/%u\n",
           S[0].keyFunc, S[1].keyFunc, S[0].tuneCmd, S[1].tuneCmd,
           S[0].frqKeyUpDownDelta, S[1].frqKeyUpDownDelta,
           S[0].rwModeTmp, S[1].rwModeTmp);
    printf("  runFlag=0x%04X/0x%04X invt=0x%04X/0x%04X torqueAct=%lu/%lu\n",
           S[0].runFlagAll, S[1].runFlagAll, S[0].invtAll, S[1].invtAll,
           (unsigned long)S[0].torqueCurrentAct, (unsigned long)S[1].torqueCurrentAct);
}

/* ---- 跑一轮并比对 ---- */

static void run_case(void)
{
    int side;

    g_cases++;

    for (side = 0; side < 2; side++)
    {
        install(side);
        S[side].ret = 0;
        g_in.dataLocal = g_in.arg4;

        switch (g_kind)
        {
        case K_FCDIGIT2BIN:
            S[side].ret = g_fc2bin[side](g_in.arg0);
            break;
        case K_LIMITDEAL:
            S[side].ret = g_limit[side](g_in.arg0, g_in.arg1, g_in.arg2,
                                        g_in.arg3, g_in.sarg);
            break;
        case K_NOSAME:
            S[side].ret = g_nosame[side](g_in.arg0, g_in.tbl, g_in.sarg,
                                         (int16)g_in.arg1, (int16)g_in.arg2,
                                         (int16)g_in.arg3, (int16)g_in.arg4);
            break;
        case K_OVERTURN:
            S[side].ret = g_overturn[side](g_in.arg0, g_in.arg1, g_in.arg2, g_in.arg3);
            break;
        case K_LIMITOVTURN:
            S[side].ret = g_limitovt[side](g_in.tbl, g_in.arg0, g_in.arg1,
                                           g_in.arg2, g_in.arg3);
            break;
        case K_GETDIGITS:
            S[side].ret = g_getdigits[side](g_in.arg0);
            break;
        case K_MODUPDOWN:
            S[side].ret = g_modupdown[side](g_in.arg0, &g_in.dataLocal, g_in.sarg);
            break;
        case K_MODENTER:
            S[side].ret = g_modenter[side](g_in.arg0, g_in.arg1);
            break;
        case K_UPDATTR:
            g_updattr[side](g_in.arg0, g_in.attr);
            break;
        case K_UPDGROUP:
            g_updgroup[side](g_in.arg0);
            break;
        case K_UPDMENU0ST:
            g_updmenust[side]();
            break;
        case K_UPDMENU0: g_updmenu[side][0](); break;
        case K_UPDMENU1: g_updmenu[side][1](); break;
        case K_UPDMENU2: g_updmenu[side][2](); break;
        case K_UPDMENU3: g_updmenu[side][3](); break;
        case K_UPDERR:      g_upderr[side]();      break;
        case K_UPDTUNE:     g_updtune[side]();     break;
        case K_UPDPWDHINT:  g_updpwdhint[side]();  break;
        case K_UPDPWDINPUT: g_updpwdinput[side](); break;
        case K_UPDIOSTATUS: g_updio[side](g_in.larg); break;
        case K_UPDDIFUNC:   g_upddifunc[side](g_in.arg0, g_in.larg); break;
        case K_UPDU0:       g_updu0[side]();       break;
        case K_DISPDATA:    g_dispdata[side]();    break;
        case K_CYCLESHIFT:  g_cycleshift[side](g_in.arg0); break;
        case K_MOTORDEBUG:  g_motordebug[side]();  break;
        case K_UPDGRADE:    g_updgrade[side](funcCodeGradeCurMenuMode); break;
        case K_GROUPUPDOWN:
            S[side].ret = g_groupupdown[side](g_in.tbl, g_in.arg0, g_in.arg1);
            break;
        case K_DEALUSERGRADE:  g_dealuser[side](g_in.arg0);  break;
        case K_DEALCHECKGRADE: g_dealcheck[side](g_in.arg0); break;
        case K_MENUINIT:       g_menuinit[side]();      break;
        case K_MENUMODESW:     g_menumodesw[side]();    break;
        case K_MENUMODEDEAL:   g_menumodedeal[side]();  break;
        case K_MENU0ADDLEVEL:  g_menu0add[side]();      break;
        case K_MENU0PRG: g_menuprg[side][0](); break;
        case K_MENU1PRG: g_menuprg[side][1](); break;
        case K_MENU2PRG: g_menuprg[side][2](); break;
        case K_MENU3PRG: g_menuprg[side][3](); break;
        case K_MENU0ENTER: g_menuenter[side][0](); break;
        case K_MENU1ENTER: g_menuenter[side][1](); break;
        case K_MENU2ENTER: g_menuenter[side][2](); break;
        case K_MENU3ENTER: g_menuenter[side][3](); break;
        case K_MENU0UP: g_menuupdown[side][0](); break;
        case K_MENU1UP: g_menuupdown[side][1](); break;
        case K_MENU2UP: g_menuupdown[side][2](); break;
        case K_MENU3UP: g_menuupdown[side][3](); break;
        case K_MENU1DOWN: g_menudown[side][0](); break;
        case K_MENU2DOWN: g_menudown[side][1](); break;
        case K_MENU3DOWN: g_menudown[side][2](); break;
        case K_MENU1UPDN: g_menuupdown[side][1](); break;
        case K_MENU2UPDN: g_menuupdown[side][2](); break;
        case K_MENU3UPDN: g_menuupdown[side][3](); break;
        case K_MENU0SHIFT: g_menushift[side][0](); break;
        case K_MENU1SHIFT: g_menushift[side][1](); break;
        case K_MENU2SHIFT: g_menushift[side][2](); break;
        case K_MENU3SHIFT: g_menushift[side][3](); break;
        case K_MENUMFK:    g_menufk[side]();    break;
        case K_MENURUN:    g_menurun[side]();   break;
        case K_MENUSTOP:   g_menustop[side]();  break;
        case K_MENUQUICK:  g_menuquick[side](); break;
        case K_PWDONPRG:      g_pwdprg[side]();      break;
        case K_PWDHINT2INPUT: g_pwdhint2input[side](); break;
        case K_PWDHINTONSHIFT: g_pwdhintshift[side](); break;
        case K_PWDHINTONQUICK: g_pwdhintquick[side](); break;
        case K_PWDINPUTENTER:  g_pwdinputenter[side](); break;
        case K_PWDINPUTUP:     g_pwdinputup[side]();   break;
        case K_PWDINPUTDOWN:   g_pwdinputdown[side](); break;
        case K_PWDINPUTUPDN:   g_pwdinputupdown[side](g_in.arg0); break;
        case K_PWDINPUTSHIFT:  g_pwdinputshift[side](); break;
        case K_PWDINPUTQUICK:  g_pwdinputquick[side](); break;
        case K_VALIDTUNE:
            S[side].ret = g_validtune[side](g_in.arg0, g_in.arg1);
            break;
        default:
            break;
        }

        S[side].dataOut = g_in.dataLocal;
        snapshot(side);
    }

    if (S[0].ret != S[1].ret) { report("return value"); return; }
    if (S[0].dataOut != S[1].dataOut) { report("*data (ModifyFunccodeUpDown)"); return; }

    {
        int i;
        for (i = 0; i < FMENU_U16_N; i++)
        {
            if (S[0].u16[i] != S[1].u16[i]) { report("module u16 export"); return; }
        }
    }

    if (S[0].torqueCurrentAct != S[1].torqueCurrentAct) { report("torqueCurrentAct"); return; }
    if (S[0].menu0DispStatus != S[1].menu0DispStatus) { report("menu0DispStatus"); return; }
    if (S[0].menuLevel != S[1].menuLevel) { report("menuLevel"); return; }
    if (S[0].menuMode != S[1].menuMode) { report("menuMode"); return; }
    if (S[0].menuModeTmp != S[1].menuModeTmp) { report("menuModeTmp"); return; }
    if (S[0].menuModeOld != S[1].menuModeOld) { report("menuModeOld"); return; }
    if (S[0].menuModeStatus != S[1].menuModeStatus) { report("menuModeStatus"); return; }
    if (S[0].facPassViewStatus != S[1].facPassViewStatus) { report("facPassViewStatus"); return; }
    if (S[0].checkMenuModeDealStatus != S[1].checkMenuModeDealStatus)
    { report("checkMenuModeDealStatus"); return; }
    if (S[0].factoryPwdStatus != S[1].factoryPwdStatus) { report("factoryPwdStatus"); return; }
    if (S[0].userMenuModeFcIndex != S[1].userMenuModeFcIndex)
    { report("userMenuModeFcIndex"); return; }

    if (memcmp(&S[0].curFc, &S[1].curFc, sizeof S[0].curFc) != 0) { report("curFc"); return; }
    if (memcmp(&S[0].groupDisplay, &S[1].groupDisplay, sizeof S[0].groupDisplay) != 0)
    { report("groupDisplay"); return; }
    if (memcmp(&S[0].motorDebugFc, &S[1].motorDebugFc, sizeof S[0].motorDebugFc) != 0)
    { report("motorDebugFc"); return; }
    if (memcmp(&S[0].lpf, &S[1].lpf, sizeof S[0].lpf) != 0)
    { report("torQueFrqDispLpf"); return; }
    if (memcmp(S[0].menuAttri, S[1].menuAttri, sizeof S[0].menuAttri) != 0)
    { report("menuAttri"); return; }
    if (memcmp(S[0].menuFc, S[1].menuFc, sizeof S[0].menuFc) != 0)
    { report("menuFc"); return; }

    if (memcmp(S[0].tblDec, S[1].tblDec, sizeof S[0].tblDec) != 0)
    { report("decNumber"); return; }
    if (memcmp(S[0].tblHex, S[1].tblHex, sizeof S[0].tblHex) != 0)
    { report("hexNumber"); return; }
    if (memcmp(S[0].tblDelta, S[1].tblDelta, sizeof S[0].tblDelta) != 0)
    { report("deltaK"); return; }
    if (memcmp(S[0].tblDiFunc, S[1].tblDiFunc, sizeof S[0].tblDiFunc) != 0)
    { report("diFuncIndex"); return; }
    if (memcmp(S[0].tblFrqSrc, S[1].tblFrqSrc, sizeof S[0].tblFrqSrc) != 0)
    { report("frqSrcFuncIndex"); return; }
    if (memcmp(S[0].tblStop, S[1].tblStop, sizeof S[0].tblStop) != 0)
    { report("stopDispIndex"); return; }
    if (memcmp(S[0].tblComm, S[1].tblComm, sizeof S[0].tblComm) != 0)
    { report("commDispIndex"); return; }
    if (memcmp(S[0].tblDispU0, S[1].tblDispU0, sizeof S[0].tblDispU0) != 0)
    { report("dispAttributeU0"); return; }
    if (memcmp(S[0].menuSlot, S[1].menuSlot, sizeof S[0].menuSlot) != 0)
    { report("menu[] 槽位指向的函数"); return; }

    if (memcmp(&S[0].fc, &S[1].fc, sizeof S[0].fc) != 0) { report("funcCode"); return; }
    if (memcmp(S[0].displayBuffer, S[1].displayBuffer, sizeof S[0].displayBuffer) != 0)
    { report("displayBuffer"); return; }
    if (S[0].runFlagAll != S[1].runFlagAll) { report("runFlag.all"); return; }
    if (S[0].invtAll != S[1].invtAll) { report("invtStatus.all"); return; }
    if (S[0].keyFunc != S[1].keyFunc) { report("keyFunc"); return; }
    if (S[0].tuneCmd != S[1].tuneCmd) { report("tuneCmd"); return; }
    if (S[0].frqKeyUpDownDelta != S[1].frqKeyUpDownDelta) { report("frqKeyUpDownDelta"); return; }
    if (S[0].rwMode != S[1].rwMode) { report("funcCodeRwMode"); return; }
    if (S[0].rwModeTmp != S[1].rwModeTmp) { report("funcCodeRwModeTmp"); return; }
    if (S[0].qNum != S[1].qNum) { report("funcCodeOneWriteNum"); return; }
    if (memcmp(S[0].q, S[1].q, sizeof S[0].q) != 0) { report("funcCodeOneWriteIndex"); return; }
    if (memcmp(S[0].gradeCur, S[1].gradeCur, sizeof S[0].gradeCur) != 0)
    { report("funcCodeGradeCurMenuMode"); return; }
    if (memcmp(S[0].invParaAll, S[1].invParaAll, sizeof S[0].invParaAll) != 0)
    { report("invPara"); return; }
    if (memcmp(S[0].motorFcAll, S[1].motorFcAll, sizeof S[0].motorFcAll) != 0)
    { report("motorFc"); return; }

    if (S[0].digitCalls != S[1].digitCalls) { report("GetNumberDigit 调用次数"); return; }
    if (S[0].digitLastArg != S[1].digitLastArg) { report("GetNumberDigit 末次实参"); return; }
    if (S[0].digitLastMode != S[1].digitLastMode) { report("GetNumberDigit 末次模式"); return; }
    if (S[0].getInitCalls != S[1].getInitCalls) { report("GetFuncCodeInit 调用次数"); return; }
    if (S[0].getInitLastIndex != S[1].getInitLastIndex)
    { report("GetFuncCodeInit 末次下标"); return; }
    if (S[0].getInitLastType != S[1].getInitLastType)
    { report("GetFuncCodeInit 末次类型"); return; }
    if (S[0].limitOtherCalls != S[1].limitOtherCalls)
    { report("LimitOtherCodeDeal 调用次数"); return; }
    if (S[0].limitOtherLastIndex != S[1].limitOtherLastIndex)
    { report("LimitOtherCodeDeal 末次下标"); return; }
    if (S[0].clearRecordCalls != S[1].clearRecordCalls)
    { report("ClearRecordDeal 调用次数"); return; }
    if (S[0].motorPowerCalls != S[1].motorPowerCalls)
    { report("MotorPowerRelatedParaDeal 调用次数"); return; }
    if (S[0].motorPowerLastPower != S[1].motorPowerLastPower)
    { report("MotorPowerRelatedParaDeal 末次功率"); return; }
    if (S[0].motorPowerLastSn != S[1].motorPowerLastSn)
    { report("MotorPowerRelatedParaDeal 末次电机号"); return; }
    if (S[0].invTypeRelatedCalls != S[1].invTypeRelatedCalls)
    { report("InverterTypeRelatedParaDeal 调用次数"); return; }
    if (S[0].validateInvTypeCalls != S[1].validateInvTypeCalls)
    { report("ValidateInvType 调用次数"); return; }
    if (S[0].resetUpDownCalls != S[1].resetUpDownCalls)
    { report("ResetUpDownFrq 调用次数"); return; }
    if (S[0].lpfCalls != S[1].lpfCalls) { report("LpfCalc 调用次数"); return; }
    if (S[0].lpfLastT != S[1].lpfLastT) { report("LpfCalc 末次 t"); return; }
    if (S[0].lpfLastIn != S[1].lpfLastIn) { report("LpfCalc 末次 in"); return; }
}

/* ---- 随机数 ---- */

static Uint32 g_seed = 0x9E3779B9UL;

static Uint16 rnd(void)
{
    g_seed = g_seed * 1103515245UL + 12345UL;
    return (Uint16)((g_seed >> 16) & 0xFFFFUL);
}

static Uint16 rnd_n(Uint16 n)
{
    return (Uint16)(rnd() % n);
}

/* 闭区间 [lo, hi] 上的均匀取值, hi - lo + 1 可能是 65536 —— 这个数在 Uint16 里
 * 就是 0, 直接用 rnd_n((Uint16)(hi - lo + 1)) 会变成 rnd() % 0, 触发
 * 0xC0000094 整数除零。属性表里确实存在 upper = 65535 / lower = 0 的功能码
 * (表示"无上下限"), 所以这条路是能走到的。这里全程用 32 位算。 */
static Uint16 rnd_in(Uint16 lo, Uint16 hi)
{
    Uint32 span = (Uint32)hi - (Uint32)lo + 1UL;      /* 1 .. 65536 */

    return (Uint16)((Uint32)lo + ((Uint32)rnd() % span));
}

/* ---- 不变量检查 ---- */

/* 解析一个功能码的**合法取值域** [lo, hi]。
 *
 * 属性表里的 upper/lower 未必是数值: 当 bit.upperLimit / bit.lowerLimit 置位时,
 * 它们是"另一个功能码的下标", 链尾那一项的 upper/lower 才是真正的端点 ——
 * GetDispDigits() 就是这么顺链走的 (先顺 upper 链, 再从那一点顺 lower 链)。
 *
 * 驱动为什么需要它: ModifyFunccodeEnter(index, dataNew) 会把 dataNew **原样写进
 * funcCode.all[index]**。若喂进去的是非法值, 后面立刻有代码把它当**表下标**用 ——
 * 最典型的是 frqPoint: index == GetCodeIndex(frqPoint) 那一支马上求值
 * `decNumber[funcCode.code.frqPoint]`, 而 decNumber 只有 5 项。喂 65535 就是
 * 131 KB 的越界读 (宿主上直接 SIGSEGV)。真调用点的 dataNew 都来自
 * ModifyFunccodeUpDown, 已被 LimitDeal 夹进 [lower, upper], 所以永远合法。
 */
static void fc_value_range(int idx, Uint16 *lo, Uint16 *hi)
{
    int p = idx;
    int step;

    for (step = 0; step <= FMENU_ATTR_N; step++)
    {
        if (!funcCodeAttribute[p].attribute.bit.upperLimit)
        {
            break;
        }
        p = funcCodeAttribute[p].upper;
        if (p >= FMENU_ATTR_N)
        {
            break;
        }
    }
    *hi = funcCodeAttribute[p].upper;

    p = idx;
    for (step = 0; step <= FMENU_ATTR_N; step++)
    {
        if (!funcCodeAttribute[p].attribute.bit.lowerLimit)
        {
            break;
        }
        p = funcCodeAttribute[p].lower;
        if (p >= FMENU_ATTR_N)
        {
            break;
        }
    }
    *lo = funcCodeAttribute[p].lower;

    /* 表里若有畸形项 (hi < lo), 退化成单点, 免得驱动自己造出反向区间。 */
    if (*hi < *lo)
    {
        *hi = *lo;
    }
}

static void check_invariants(void)
{
    int g, i;
    Uint16 maxIdx = 0;
    int maxIdxGroup = -1;
    int firstOob = -1;

    /* 宽度前提。
     *
     * 这里**不能**要求 `sizeof(FUNCCODE_ALL)/sizeof(Uint16) == FNUM_ALL`。
     * 原因是一处已知的宿主/目标模型分歧, 不是缺陷:
     *
     *   f_funcCode.h 里有一批"按字重叠"的 union, 写法是
     *       union MOTOR_PARA { Uint16 all[sizeof(struct MOTOR_PARA_STRUCT)];
     *                           struct MOTOR_PARA_STRUCT elem; };
     *   这个 all[] 的长度依赖 **C28x 的 sizeof 返回"字"数**。宿主 sizeof 返回
     *   字节数, 于是 all[] 变成目标端的 2 倍, union 随之被撑大: 目标端
     *   FUNCCODE_ALL = 1116 字, 宿主上 = 1290 字。这类 union 在 struct
     *   FUNCCODE_CODE 里出现 4 组电机参数 + 1 组故障现场, 合计正好把结构体
     *   撑大 174 字。
     *
     * 为什么这不影响本测试台的结论:
     *   1) ref 与 new 两侧用的是**同一份**宿主布局, 分歧在比较中完全抵消;
     *   2) 目标端 .econst 里那几张表的**逐字节**一致性由
     *      build_and_compare.py 的 Tier B (coff_cmp 按节比较) 独立证明,
     *      本测试台不承担这个职责;
     *   3) 真正影响安全性的只有"重叠数组没被截断"这一条 —— 下面检查它。
     *      (若 all[] 比 code 小, 某些 GetCodeIndex 派生的下标就会越过 union。)
     */
    printf("FNUM_PARA = %d, FNUM_ALL = %d, FUNCCODE_GROUP_NUM = %d\n",
           (int)FNUM_PARA, (int)FNUM_ALL, (int)FUNCCODE_GROUP_NUM);
    printf("sizeof(FUNCCODE_ALL)/sizeof(Uint16) = %u (目标端 %d; 差值见上方注释)\n",
           (unsigned)(sizeof(FUNCCODE_ALL) / sizeof(Uint16)), (int)FNUM_ALL);
    if ((Uint16)(sizeof(FUNCCODE_ALL) / sizeof(Uint16)) < (Uint16)FNUM_ALL)
    {
        printf("FATAL: FUNCCODE_ALL 的宿主宽度 %u < FNUM_ALL %d, all[] 被截断\n",
               (unsigned)(sizeof(FUNCCODE_ALL) / sizeof(Uint16)), (int)FNUM_ALL);
        exit(2);
    }

    /* 两张 GetCodeIndex 表的实际取值。
     *
     * 宿主下标空间与目标端**不同** (原因见上): 这些值由 offsetof 派生, 是宿主的
     * 字偏移。它们必须落在 funcCode 这个 union 对象的实际宽度内 —— NoSameDeal
     * 会拿它们去读 `funcCode.all[funcIndex[i]]`, 越出去就是在读别的全局变量。
     */
    {
        int i;
        Uint16 mx = 0;
        Uint16 unionWords = (Uint16)(sizeof(FUNCCODE_ALL) / sizeof(Uint16));

        printf("frqSrcFuncIndex:");
        for (i = 0; i < 2; i++)
        {
            printf(" %u", (unsigned)ref_frqSrcFuncIndex[i]);
            if (ref_frqSrcFuncIndex[i] > mx)
            {
                mx = ref_frqSrcFuncIndex[i];
            }
        }
        printf("\ndiFuncIndex   :");
        for (i = 0; i < 18; i++)
        {
            printf(" %u", (unsigned)ref_diFuncIndex[i]);
            if (ref_diFuncIndex[i] > mx)
            {
                mx = ref_diFuncIndex[i];
            }
        }
        printf("\nGetCodeIndex 表最大下标 %u (宿主 union 实宽 %u 字)\n", mx, unionWords);
        if (mx >= unionWords)
        {
            printf("FATAL: GetCodeIndex 表下标 %u 越过宿主 union 宽度 %u\n",
                   mx, unionWords);
            exit(2);
        }
    }

    /* ModifyFunccodeEnter / ModifyFunccodeUpDown 里那些"特例分支"命中的成员下标。
     *
     * 打印出来是为了**能对照**: 驱动按 idx 遍历属性表 (0..908), 某一轮究竟走进了
     * 哪个 else-if 分支, 全靠这张对照表; 崩溃定位和负向对照挑锚点都要用它。
     * 注意这些是**宿主**下标 (offsetof 派生), 与目标端不同, 见上面的说明。 */
    {
#define FMENU_SP(tag, expr) \
        printf("  %-24s = %u\n", tag, (unsigned)GetCodeIndex(expr))
        printf("ModifyFunccodeEnter 特例分支的成员下标:\n");
        FMENU_SP("presetFrq", funcCode.code.presetFrq);
        FMENU_SP("frqCalcSrc", funcCode.code.frqCalcSrc);
        FMENU_SP("motorM1.ratingPower", funcCode.code.motorParaM1.elem.ratingPower);
        FMENU_SP("motorM2.ratingPower", funcCode.code.motorFcM2.motorPara.elem.ratingPower);
        FMENU_SP("motorM3.ratingPower", funcCode.code.motorFcM3.motorPara.elem.ratingPower);
        FMENU_SP("motorM4.ratingPower", funcCode.code.motorFcM4.motorPara.elem.ratingPower);
        FMENU_SP("menuMode", funcCode.code.menuMode);
        FMENU_SP("aiaoCalibrateDisp", funcCode.code.aiaoCalibrateDisp);
        FMENU_SP("funcParaView", funcCode.code.funcParaView);
        FMENU_SP("inverterType", funcCode.code.inverterType);
        FMENU_SP("motorDebugFc", funcCode.code.motorDebugFc);
        FMENU_SP("ledDispParaRun1", funcCode.code.ledDispParaRun1);
        FMENU_SP("ledDispParaRun2", funcCode.code.ledDispParaRun2);
        FMENU_SP("ledDispParaStop", funcCode.code.ledDispParaStop);
        FMENU_SP("frqPoint", funcCode.code.frqPoint);
        {
            /* frqPoint 的值域必须落在 decNumber[5] 内 —— ModifyFunccodeEnter 写它
             * 之后立刻就会求值 decNumber[frqPoint]。 */
            Uint16 lo, hi;
            fc_value_range((int)GetCodeIndex(funcCode.code.frqPoint), &lo, &hi);
            printf("  frqPoint 值域           = [%u, %u]  (decNumber 共 5 项)\n",
                   (unsigned)lo, (unsigned)hi);
            if (hi > 4)
            {
                printf("FATAL: frqPoint 上界 %u 超出 decNumber 的 5 项\n", (unsigned)hi);
                exit(2);
            }
        }
        FMENU_SP("diFunc[0]", funcCode.code.diFunc[0]);
        FMENU_SP("diFunc[4]", funcCode.code.diFunc[4]);
        FMENU_SP("diFunc[9]", funcCode.code.diFunc[9]);
#undef FMENU_SP
    }

    if (FMENU_ATTR_N != FNUM_PARA)
    {
        printf("FATAL: 属性表长度 %d != FNUM_PARA %d\n", FMENU_ATTR_N, (int)FNUM_PARA);
        exit(2);
    }

    /* 分级表自洽: GetGradeIndex(group, grade) = funcCodeGradeSum[group] + grade,
     * grade < funcCodeGradeCurMenuMode[group] (由 funcCodeGradeAll 派生)。
     * 把每个组可能取到的最大下标算出来, 看有没有越过属性表。 */
    for (g = 0; g < FUNCCODE_GROUP_NUM; g++)
    {
        Uint16 sum = funcCodeGradeSum[g];
        Uint16 n = funcCodeGradeAll[g];
        Uint16 hi;

        if (sum > FNUM_ALL)
        {
            printf("FATAL: funcCodeGradeSum[%d] = %u > FNUM_ALL\n", g, sum);
            exit(2);
        }
        if (n == 0)
        {
            continue;
        }
        hi = (Uint16)(sum + n - 1);
        if (hi > maxIdx)
        {
            maxIdx = hi;
            maxIdxGroup = g;
        }
        if (firstOob < 0 && sum >= FNUM_PARA)
        {
            firstOob = g;
        }
    }
    printf("分级表: 最大下标 %u (组 %d), 第一个越界组 %d (FUNCCODE_GROUP_U0 = %d)\n",
           maxIdx, maxIdxGroup, firstOob, (int)FUNCCODE_GROUP_U0);
    if (firstOob < 0)
    {
        printf("FATAL: 没有任何组的起始下标越过属性表 —— 与预期不符, 请复核\n");
        exit(2);
    }
    if (firstOob < FUNCCODE_GROUP_U0)
    {
        printf("FATAL: 越界组 %d 落在 U 组之前 —— 驱动只保证 U 组之后的路径安全\n",
               firstOob);
        exit(2);
    }

    /* 驱动实际会喂给属性表的最大下标: 只走 g < FUNCCODE_GROUP_U0 的组。 */
    {
        Uint16 drvMax = 0;
        for (g = 0; g < FUNCCODE_GROUP_U0; g++)
        {
            Uint16 hi = (Uint16)(funcCodeGradeSum[g] + funcCodeGradeAll[g]);
            if (funcCodeGradeAll[g] == 0)
            {
                continue;
            }
            if ((Uint16)(hi - 1) > drvMax)
            {
                drvMax = (Uint16)(hi - 1);
            }
        }
        printf("驱动可达的最大功能码下标 %u (属性表长 %d)\n", drvMax, FMENU_ATTR_N);
        if (drvMax >= FMENU_ATTR_N)
        {
            printf("FATAL: 驱动可达下标 %u 越过属性表 %d\n", drvMax, FMENU_ATTR_N);
            exit(2);
        }
    }

    /* upperLimit / lowerLimit 链必须无环 —— GetDispDigits 会顺着链走。 */
    for (i = 0; i < FMENU_ATTR_N; i++)
    {
        int step, p = i;
        for (step = 0; step <= FMENU_ATTR_N; step++)
        {
            if (!funcCodeAttribute[p].attribute.bit.upperLimit)
            {
                break;
            }
            p = funcCodeAttribute[p].upper;
            if (p >= FMENU_ATTR_N)
            {
                printf("FATAL: 属性表 upper 链从 %d 越界到 %d\n", i, p);
                exit(2);
            }
        }
        if (step > FMENU_ATTR_N)
        {
            printf("FATAL: 属性表 upper 链从 %d 起有环\n", i);
            exit(2);
        }

        p = i;
        for (step = 0; step <= FMENU_ATTR_N; step++)
        {
            if (!funcCodeAttribute[p].attribute.bit.lowerLimit)
            {
                break;
            }
            p = funcCodeAttribute[p].lower;
            if (p >= FMENU_ATTR_N)
            {
                printf("FATAL: 属性表 lower 链从 %d 越界到 %d\n", i, p);
                exit(2);
            }
        }
        if (step > FMENU_ATTR_N)
        {
            printf("FATAL: 属性表 lower 链从 %d 起有环\n", i);
            exit(2);
        }
    }
    printf("属性表: %d 项, upper/lower 链无环\n", FMENU_ATTR_N);

    /* FUNC_ATTRIBUTE 的位序。驱动全程用 .bit.* 赋值, 这里把"位序"这件事显式钉住
     * —— 万一头文件改了位序, 用例立刻报错而不是悄悄测错。 */
    {
        union FUNC_ATTRIBUTE probe;

        probe.all = 0;
        probe.bit.point = 5;
        if (probe.all != (Uint16)(5 << 0))
        {
            printf("FATAL: FUNC_ATTRIBUTE.point 不在第 0~2 位 (all=0x%04X)\n", probe.all);
            exit(2);
        }
        probe.all = 0;
        probe.bit.writable = 3;
        if (probe.all != (Uint16)(3 << 11))
        {
            printf("FATAL: FUNC_ATTRIBUTE.writable 不在第 11~12 位 (all=0x%04X)\n",
                   probe.all);
            exit(2);
        }
        probe.all = 0;
        probe.bit.multiLimit = 3;
        if (probe.all != (Uint16)(3 << 14))
        {
            printf("FATAL: FUNC_ATTRIBUTE.multiLimit 不在第 14~15 位 (all=0x%04X)\n",
                   probe.all);
            exit(2);
        }
    }
    printf("FUNC_ATTRIBUTE 位序: point[0:2] unit[3:5] displayBits[6:8] "
           "upperLimit[9] lowerLimit[10] writable[11:12] signal[13] multiLimit[14:15]\n");

    /* 编码表 */
    if (DISPLAY_CODE[0] != 0xc0 || DISPLAY_CODE[37] != 0xa7)
    {
        printf("FATAL: DISPLAY_CODE 首尾值不符 (0x%04X / 0x%04X)\n",
               DISPLAY_CODE[0], DISPLAY_CODE[37]);
        exit(2);
    }
    if (LED_CODE[0] != 0xBF || LED_CODE[10] != 0xFF)
    {
        printf("FATAL: LED_CODE 首尾值不符 (0x%04X / 0x%04X)\n",
               LED_CODE[0], LED_CODE[10]);
        exit(2);
    }
    printf("DISPLAY_CODE[%d] LED_CODE[%d] displayBuffer[%d]\n",
           (int)DISPLAY_8LED_CODE_NUM, (int)DISPLAY_LED_CODE_NUM,
           (int)(sizeof displayBuffer / sizeof displayBuffer[0]));

    /* menu[] 表的每个槽位都必须解析到一个已知函数 —— 解析不出来说明
     * 同一性表漏了函数 (new_case.py 的改名清单与本驱动不同步)。 */
    for (i = 0; i < MENU_LEVEL_NUM; i++)
    {
        int j;
        for (j = 0; j < MENU_SLOT_N; j++)
        {
            int a = fn_index(0, (void (*)(void))((j == 0) ? ref_menu[i].onPrgFunc :
                (j == 1) ? ref_menu[i].onUpFunc :
                (j == 2) ? ref_menu[i].onEnterFunc :
                (j == 3) ? ref_menu[i].onMfkFunc :
                (j == 4) ? ref_menu[i].onDownFunc :
                (j == 5) ? ref_menu[i].onShiftFunc :
                (j == 6) ? ref_menu[i].onRunFunc :
                (j == 7) ? ref_menu[i].onStopFunc :
                (j == 8) ? ref_menu[i].onQuickFunc :
                           ref_menu[i].UpdateDisplayBuffer));
            int b = fn_index(1, (void (*)(void))((j == 0) ? new_menu[i].onPrgFunc :
                (j == 1) ? new_menu[i].onUpFunc :
                (j == 2) ? new_menu[i].onEnterFunc :
                (j == 3) ? new_menu[i].onMfkFunc :
                (j == 4) ? new_menu[i].onDownFunc :
                (j == 5) ? new_menu[i].onShiftFunc :
                (j == 6) ? new_menu[i].onRunFunc :
                (j == 7) ? new_menu[i].onStopFunc :
                (j == 8) ? new_menu[i].onQuickFunc :
                           new_menu[i].UpdateDisplayBuffer));
            if (a < 0 || b < 0 || a != b)
            {
                printf("FATAL: menu[%d] 槽位 %d 解析失败 (%d/%d)\n", i, j, a, b);
                exit(2);
            }
        }
    }
    printf("menu[] 表: %d 层 x %d 槽位全部解析到已知函数\n",
           (int)MENU_LEVEL_NUM, MENU_SLOT_N);

    /* 计数斜坡的余量 */
    if (FMENU_UP_DOWN_DONE_TIME / 12 <= 0)
    {
        printf("FATAL: UP_DOWN_DEAL_DONE_TIME/12 退化\n");
        exit(2);
    }
    printf("UP_DOWN_DEAL_DONE_TIME/12 = %d\n", FMENU_UP_DOWN_DONE_TIME / 12);
}

/* ---- 输入构造 ---- */

static void zero_in(void)
{
    memset(&g_in, 0, sizeof g_in);
}

static Uint16 *u16p(int idx)
{
    return &g_in.u16[idx];
}

/* funcCode_setup() 定义在 base_state() 之后 (那一节是"功能码整表填充"), 但
 * base_state() 要用它, 所以先声明。 */
static void funcCode_setup(void);

/* 索引常量: 与 FMENU_U16_LIST 的顺序一一对应。 */
enum
{
    I_ACCDECFRQPRCFLAG = 0, I_ACCDECFRQTICKER, I_BFRQDIGITALDONE4WAITDELAY,
    I_CHECKMENUMODECMD, I_CHECKMENUMODEPARA, I_CHECKMENUMODESERACHNONE,
    I_CURFCDISPDIGITS, I_CURRENTOCDISP, I_FRQAIMDISP, I_FRQAIMPLCDISP,
    I_FRQDISP, I_FRQPLCDISP, I_FRQRUNDISP, I_FRQXDISP, I_FRQYDISP,
    I_GROUPHIDECHKOKFLAG, I_GROUPHIDEPWDSTATUS, I_ITDISP, I_LOADSPEEDDISP,
    I_MENU3NUMBER, I_MENUPWDNUMBER, I_OUTCURRENTDISP, I_OUTVOLTAGEDISP,
    I_PCORIGINDISP, I_PIDFUNCFDBDISP, I_PIDFUNCREFDISP, I_PULSEINFRQDISP,
    I_SUPERFACTORYPASS, I_TICKER4LOWERDISP
};

/* 把全部输入填成**非平凡**的基线值。
 *
 * 每一条都刻意避开"恰好落在判据边界上"或"恰好落在掩码盲区里"的取值:
 *   - menuAttri[].operateDigit 非 0, 否则 Menu2OnShift / Menu3OnShift 的
 *     "0 -> 3 / 0 -> max-1"那一支永远走不到;
 *   - menuAttri[].winkFlag 的三档都非 0;
 *   - curFc 的 index/group/grade 两两不同, 于是"把 group 与 grade 写反"立刻可见;
 *   - frqDroop / frq / frqAim / pidFunc.ref / pidFunc.fdb 全非 0, 否则
 *     DispDataDeal 的乘除法在 0 处求值;
 *   - diStatus / doStatus / diFunc 的位图非零, 否则 UpdateMenu3DisplayBuffer 的
 *     逐位拆解不可分辨;
 *   - invPara.type > invPara.pointLimit, 让"小数点挪一位"那一支可达。
 */
static void base_state(void)
{


    int i;

    zero_in();

    g_in.u16[I_ACCDECFRQPRCFLAG] = 0;
    g_in.u16[I_ACCDECFRQTICKER] = 3;
    g_in.u16[I_BFRQDIGITALDONE4WAITDELAY] = 0;
    g_in.u16[I_CHECKMENUMODECMD] = 0;
    g_in.u16[I_CHECKMENUMODEPARA] = 0;
    g_in.u16[I_CHECKMENUMODESERACHNONE] = 0;
    g_in.u16[I_CURFCDISPDIGITS] = 5;
    g_in.u16[I_CURRENTOCDISP] = 1234;
    g_in.u16[I_FRQAIMDISP] = 4321;
    g_in.u16[I_FRQAIMPLCDISP] = 7000;
    g_in.u16[I_FRQDISP] = 5000;
    g_in.u16[I_FRQPLCDISP] = 6000;
    g_in.u16[I_FRQRUNDISP] = 5100;
    g_in.u16[I_FRQXDISP] = 111;
    g_in.u16[I_FRQYDISP] = 222;
    g_in.u16[I_GROUPHIDECHKOKFLAG] = 0;
    g_in.u16[I_GROUPHIDEPWDSTATUS] = 0;
    g_in.u16[I_ITDISP] = 777;
    g_in.u16[I_LOADSPEEDDISP] = 888;
    g_in.u16[I_MENU3NUMBER] = 12345;
    g_in.u16[I_MENUPWDNUMBER] = 6789;
    g_in.u16[I_OUTCURRENTDISP] = 456;
    g_in.u16[I_OUTVOLTAGEDISP] = 2200;
    g_in.u16[I_PCORIGINDISP] = 33;
    g_in.u16[I_PIDFUNCFDBDISP] = 2000;
    g_in.u16[I_PIDFUNCREFDISP] = 1000;
    g_in.u16[I_PULSEINFRQDISP] = 1500;
    g_in.u16[I_SUPERFACTORYPASS] = 0;
    g_in.u16[I_TICKER4LOWERDISP] = 0;

    g_in.torqueCurrentAct = 0x12345678UL;

    g_in.menu0DispStatus = MENU0_DISP_STATUS_RUN_STOP;
    g_in.menuLevel = MENU_LEVEL_2;
    g_in.menuMode = MENU_MODE_BASE;
    g_in.menuModeTmp = MENU_MODE_BASE;
    g_in.menuModeOld = MENU_MODE_NULL;
    g_in.menuModeStatus = MENU_MODE_NONE;
    g_in.facPassViewStatus = FAC_PASS_NONE;
    g_in.checkMenuModeDealStatus = CHECK_MENU_MODE_DEAL_CMD;
    g_in.factoryPwdStatus = FMENU_FACTORY_PWD_LOCK;
    g_in.userMenuModeFcIndex = 1;

    g_in.curFc.index = 40;
    g_in.curFc.group = 5;
    g_in.curFc.grade = 7;

    g_in.groupDisplay.dispF = 3;
    g_in.groupDisplay.disp0 = 4;

    g_in.motorDebugFc.fc = 12;
    g_in.motorDebugFc.u = 3;

    /* 低通滤波器: t 非 0 (否则走 `out = in` 那一支), 且 out/outOld/remainder
     * 两两不同, 让 LpfCalc 的每一步都可分辨。 */
    g_in.lpf.calc = LpfCalc;
    g_in.lpf.t = 75;
    g_in.lpf.in = 12345;
    g_in.lpf.out = 6000;
    g_in.lpf.outOld = 5000;
    g_in.lpf.remainder = 137;

    for (i = 0; i < MENU_LEVEL_NUM; i++)
    {
        g_in.menuAttri[i].operateDigit = (Uint16)(i % 3);
        g_in.menuAttri[i].winkFlag = (Uint16)(0x0011U << i);
        g_in.menuAttri[i].winkFlagLed = (Uint16)(0x0002U << i);
    }
    for (i = 0; i <= MENU_MODE_MAX; i++)
    {
        g_in.menuFc[i].group = (Uint16)(20 + i);
        g_in.menuFc[i].grade = (Uint16)(3 + i);
    }

    /* ---- 共享状态 ---- */
    g_in.fc.all[0] = 0;
    funcCode_setup();

    g_in.displayBuffer[0] = 0xbf;
    g_in.displayBuffer[1] = 0x89;
    g_in.displayBuffer[2] = 0xBF;
    g_in.displayBuffer[3] = 0xC6;
    g_in.displayBuffer[4] = 0xbf;
    g_in.displayBuffer[5] = 0x80;

    g_in.runFlag.all = 0;
    g_in.runMode = 0;
    g_in.runSrc = FUNCCODE_runSrc_PANEL;
    g_in.tuneCmd = 0;
    g_in.keyFunc = KEY_STOP;
    g_in.upDownDelta = 2;
    g_in.invtStatus.all = 0x0033;
    g_in.setRunLostTime = 42;
    g_in.torqueAim = 900;

    g_in.diFunc.f1.all = 0x1234;
    g_in.diFunc.f2.all = 0x5678;
    g_in.diStatus.a.all = 0x0ABCDE;     /* union DI_STATUS.all 是 Uint32 */
    g_in.doStatus.a.all = 0x4321;       /* union DO_STATUS.all 是 Uint16 —— 只能 16 位 */
    for (i = 0; i < AI_NUMBER; i++)
    {
        g_in.aiDeal[i].sample = (Uint16)(100 + i);
        g_in.aiDeal[i].voltageOrigin = (int16)(200 + i);
        g_in.aiDeal[i].voltage = (int16)(300 + i);
        g_in.aiDeal[i].set = (int16)(400 + i);
    }

    g_in.curTime.runTimeM = 7;
    g_in.curTime.runTimeSec = 31;
    g_in.curTime.powerOnTimeM = 3;
    g_in.curTime.powerOnTimeSec = 59;
    g_in.errorAttribute.all = 0;
    g_in.errorAttribute.bit.level = ERROR_LEVEL_NO_ERROR;
    g_in.errorCode = ERROR_NONE;
    g_in.errorInfo = 0x2468;

    g_in.frq = 12345;
    g_in.frqAim = 12000;
    g_in.frqDroop = 11900;
    g_in.frqRun = 12100;
    g_in.frqFdb = 5000;
    g_in.frqKeyUpDownDelta = 7;
    g_in.bFrqDigital = 0;
    g_in.maxFrq = 5000;
    g_in.plcStep = 2;
    g_in.pidFunc.ref = 1234;
    g_in.pidFunc.fdb = 2345;

    g_in.generatrixVoltage = 5300;
    g_in.currentPu = 4096;
    g_in.enCoderPosition = 1111;
    g_in.ABZPos = 2222;
    g_in.gPhiRtDisp = -3333;
    g_in.outPower = -1234;
    g_in.pulseInFrq = 123456UL;
    g_in.lineSpeed = 999;
    g_in.memoryValue = 0xABCD;
    g_in.torqueCurrent = 2048;
    g_in.pmsmRotorPos = 3000;

    g_in.motorFc.motorPara.elem.motorType = MOTOR_TYPE_PMSM;
    g_in.motorFc.motorPara.elem.ratingCurrent = 100;
    g_in.motorSn = MOTOR_SN_1;

    g_in.temperature = 45;
    g_in.ZCount = 66;
    g_in.vfSeparateVol = 2000;
    g_in.vfSeprateVolAim = 2100;
    for (i = 0; i < 4; i++)
    {
        g_in.ticker05msFuncEachDisp[i] = (Uint16)(1000 + i);
        g_in.ticker05msFuncInternalDisp[i] = (Uint16)(2000 + i);
    }

    g_in.p2pData.P2PSendData = 0x1111;
    g_in.p2pData.P2PRevData = 0x2222;

    g_in.invPara.type = 0x0305;
    g_in.invPara.ratingVoltage = 380;
    g_in.invPara.volLevel = 2;
    g_in.invPara.pointLimit = 0x0101;
    g_in.invPara.bitAccDecStart = 0x0302;

    g_in.rwMode = FUNCCODE_RW_MODE_NO_OPERATION;
    g_in.rwModeTmp = 0;
    for (i = 0; i < Q_SNAP; i++)
    {
        g_in.q[i] = (Uint16)i;
    }
    g_in.qNum = 0;

    /* funcCodeGradeCurMenuMode: 用真表填满, 于是 GetGradeIndex 的下标与真固件
     * 同一量级 (不填的话全是 0, 所有"按分组走"的分支都退化成一条)。 */
    for (i = 0; i < GRADE_N; i++)
    {
        g_in.gradeCur[i] = funcCodeGradeAll[i];
    }

    g_in.validateInvTypeRet = 0;

    g_in.arg0 = 0;
    g_in.arg1 = 0;
    g_in.arg2 = 0;
    g_in.arg3 = 0;
    g_in.arg4 = 0;
    g_in.sarg = 0;
    g_in.larg = 0;
    g_in.attr.all = 0;
    g_in.tbl = funcCodeGradeCurMenuMode;
}

/* funcCode 的基线内容: 每个成员都填成"和别的成员不同"的非平凡值。
 *
 * 用真表的 init 当起点, 再按组打散 —— 于是 ModifyFunccodeUpDown 的夹取边界、
 * ModifyFunccodeEnter 的"旧值 != 新值"判据、DealCheckMenuModeGroupGrade 的
 * "!= GetFuncCodeInit()"判据都能走到两个方向。 */
static void funcCode_setup(void)
{


    int i;

    for (i = 0; i < FNUM_ALL; i++)
    {
        g_in.fc.all[i] = (Uint16)((i * 37 + 11) & 0xFFFF);
    }

    /* 几个模块直接当"索引"用的成员必须是合法下标。 */
    g_in.fc.code.userPassword = 1234;
    g_in.fc.code.userPasswordReadOnly = 0;
    g_in.fc.code.menuMode = 0x0101;
    g_in.fc.code.mfkKeyFunc = 1;
    g_in.fc.code.runSrc = FUNCCODE_runSrc_PANEL;
    g_in.fc.code.frqPoint = 1;
    g_in.fc.code.speedDispPointPos = 2;
    g_in.fc.code.speedDispCoeff = 100;
    g_in.fc.code.pidDisp = 1000;
    g_in.fc.code.dispParaStopBit = 0;
    g_in.fc.code.dispParaRunBit = 0;
    g_in.fc.code.ledDispParaStop = 0xFFFF;      /* Uint16 */
    g_in.fc.code.ledDispParaRun1 = 0xFFFF;      /* Uint16, 低半 */
    g_in.fc.code.ledDispParaRun2 = 0x0000;      /* Uint16, 高半 */
    g_in.fc.code.funcParaView = 0x1111;
    g_in.fc.code.aiaoCalibrateDisp = 0;
    g_in.fc.code.motorDebugFc = 1203;
    g_in.fc.code.errorFrqUnit = 0x3210;
    g_in.fc.code.sCurveStartPhaseTime = 300;
    g_in.fc.code.sCurveEndPhaseTime = 400;
    g_in.fc.code.presetFrq = 2500;
    g_in.fc.code.frqCalcSrc = 0;
    g_in.fc.code.saveUserParaFlag1 = 1;
    g_in.fc.code.saveUserParaFlag2 = 0xFFFE;
    g_in.fc.code.maxFrq = 5000;
    g_in.fc.code.inverterType = 0x0305;

    /* userCustom[] 是"用户自定义菜单"的下标表, 每一项要是一个合法的
     * group*100 + grade。 */
    for (i = 0; i < FENUM; i++)
    {
        g_in.fc.code.userCustom[i] = (Uint16)((i % 20) * 100 + (i % 9));
    }

    /* diFunc / vdiFunc / aiAsDiFunc 被 diFuncIndex[] 索引, 值要能区分。 */
    for (i = 0; i < DI_NUMBER_PHSIC && i < 16; i++)
    {
        g_in.fc.code.diFunc[i] = (Uint16)(i + 1);
    }
    for (i = 0; i < 8; i++)
    {
        g_in.fc.code.vdiFunc[i] = (Uint16)(100 + i);
    }
    for (i = 0; i < 4; i++)
    {
        g_in.fc.code.aiAsDiFunc[i] = (Uint16)(200 + i);
    }
    g_in.fc.code.frqXSrc = 0;
    g_in.fc.code.frqYSrc = 0;
}

/* ---- 各阶段 ---- */

static const Uint16 g_various[12] =
{
    0, 1, 2, 5, 9, 10, 99, 100, 1000, 32767, 32768, 65535
};

static void phase_basics(void)
{
    int i, j, k, n;

    /* FcDigit2Bin: 全 16 位扫描 (步长 997, 与 65536 互素) + 边界值。 */
    for (i = 0; i < 65536; i += 997)
    {
        set_kind(K_FCDIGIT2BIN, (Uint16)i, 0, 0, 0, 0);
        run_case();
    }
    for (i = 0; i < 12; i++)
    {
        set_kind(K_FCDIGIT2BIN, g_various[i], 0, 0, 0, 0);
        run_case();
    }

    /* LimitDeal: signal x data x upper x lower x delta。 */
    for (n = 0; n < 2; n++)
    {
        for (i = 0; i < 12; i++)
        {
            for (j = 0; j < 12; j++)
            {
                for (k = 0; k < 6; k++)
                {
                    static const int16 dl[6] = {0, 1, -1, 10, -1000, 32767};
                    set_kind(K_LIMITDEAL, (Uint16)n, g_various[i], g_various[j], g_various[k % 12], 0);
                    g_in.arg3 = g_various[k];
                    g_in.sarg = dl[k];
                    run_case();
                }
            }
        }
    }

    /* NoSameDeal: 两张真表 + 一张"全不相等"的假表, number 覆盖 2/18/18/0。
     *
     * **number 必须 >= 0**: 它是 `for (i = 0; i < number; i++)` 的上界, 而 i 是
     * Uint16 —— C28x 上 `i < number` 是**无符号**比较 (int 转 unsigned int),
     * 所以 number 取负会变成 6 万多, funcIndex[i] 一路读到表外。真机上那只是读
     * 相邻全局, 宿主上直接 SIGSEGV。真调用点传的都是 sizeof(...), 恒非负。
     * (首轮就是栽在这: 曾想"再跑一遍负 delta", 却把负值塞进了 number 通道。)
     *
     * 负 delta 的覆盖不靠这里 —— g_various 里的 32768 / 65535 当 int16 用就是
     * 负数, 下面 arg4 已经扫到了。
     *
     * **upper/lower 取有序对且都在 [0, 32767]**, 这是**终止性前提**, 不是图省事:
     *   1) delta < 0 且 upper < lower 时, 循环里的
     *          data -= 1; if (data < lower) data = upper;
     *      会把 data 送回 upper; 若 upper 又恰好命中表值, 就成了
     *      upper -> upper-1 -> upper -> ... 的死循环。真机上同样会挂 —— 这是原
     *      工程的既有缺陷 (与 cycleShiftDeal 那个同类, 已登记在文件头)。正常调用点
     *      的 upper/lower 来自属性表, 恒满足 upper >= lower, 所以走不到。
     *   2) 都 <= 32767 时 data ± 1 不会发生 int16 回绕, 上面"单调递减到 lower
     *      就停"的论证才闭合。
     */
    for (n = 0; n < 4; n++)
    {
        static const Uint16 fake[18] =
        {
            900, 901, 902, 903, 904, 905, 906, 907, 908, 909,
            910, 911, 912, 913, 914, 915, 916, 917
        };
        /* {lower, upper} —— 恒 lower <= upper <= 32767。 */
        static const Uint16 pair[8][2] =
        {
            {0, 1}, {0, 99}, {1, 100}, {0, 1000},
            {5, 10}, {9, 32767}, {0, 32767}, {100, 32767}
        };
        const Uint16 *tbl = (n == 0) ? ref_frqSrcFuncIndex
                          : (n == 1) ? ref_diFuncIndex : fake;
        /* n = 0 -> 2 项真表; n = 1 -> 18 项真表; n = 2 -> 18 项假表;
         * n = 3 -> 假表但 number = 0 (内层循环一次都不进, 专门测前面那段
         *          "撞到 upper/lower 就翻面"的逻辑)。 */
        int16 number = (int16)((n == 0) ? 2 : (n == 3) ? 0 : 18);

        for (i = 0; i < 8; i++)
        {
            for (j = 0; j < 12; j++)
            {
                for (k = 0; k < 8; k++)
                {
                    set_kind(K_NOSAME, (Uint16)i, g_various[j],
                             pair[k][1], pair[k][0], g_various[(k + 5) % 12]);
                    g_in.tbl = tbl;
                    g_in.sarg = number;
                    run_case();
                }
            }
        }
    }

    /* OverTurnDeal: data x upper x lower x flag(0..2)。 */
    for (i = 0; i < 12; i++)
    {
        for (j = 0; j < 12; j++)
        {
            for (k = 0; k < 12; k++)
            {
                int f;
                for (f = 0; f < 3; f++)
                {
                    set_kind(K_OVERTURN, g_various[i], g_various[j], g_various[k], (Uint16)f, 0);
                    run_case();
                }
            }
        }
    }

    /* LimitOverTurnDeal: limit 表 x data x upper x low x flag。
     *
     * **实参域是硬约束, 不是"顺手取几个边界值"**: 函数体里读 `limit[data]`,
     * 而 data 每次循环都被 (upper, low) 夹进 [low, upper - 1]。所以必须
     *     upper <= 表长   且   low <= upper - 1
     * 原工程的两个调用点天然满足 —— upper 取 FUNCCODE_GROUP_NUM = 83 或
     * FUNCCODE_GROUP_U0 - 1 = 66, 表就是那两张 83 项的表 (userCustom 那处
     * upper 取 FENUM, 表也是 FENUM 项)。这里若把 upper 放开到 g_various 里
     * 的 65535, `limit[65534]` 立刻越过 83 项的表尾 —— 而且是 ref 侧先崩,
     * 连一次对比都走不到 (首轮就是栽在这里)。 */
    {
        static Uint16 allOnes[GRADE_N];
        static Uint16 allZeros[GRADE_N];
        static const Uint16 upSet[6] = {1, 2, 3, 8, 66, 83};
        static const Uint16 lowSet[4] = {0, 1, 2, 5};
        int t;
        for (i = 0; i < GRADE_N; i++)
        {
            allOnes[i] = 1;
            allZeros[i] = 0;
        }
        for (t = 0; t < 3; t++)
        {
            const Uint16 *tb = (t == 0) ? funcCodeGradeCurMenuMode
                             : (t == 1) ? allOnes : allZeros;
            int u;
            for (u = 0; u < 6; u++)
            {
                Uint16 upper = upSet[u];
                int l;
                if (upper > (Uint16)GRADE_N)
                {
                    continue;
                }
                for (l = 0; l < 4; l++)
                {
                    Uint16 low = lowSet[l];
                    int f;
                    if (low > (Uint16)(upper - 1))
                    {
                        continue;
                    }
                    for (i = 0; i < 12; i++)
                    {
                        for (f = 0; f < 3; f++)
                        {
                            set_kind(K_LIMITOVTURN, g_various[i], upper, low,
                                     (Uint16)f, 0);
                            g_in.tbl = tb;
                            run_case();
                        }
                    }
                }
            }
        }
    }

    /* GetDispDigits: 全 909 项扫描。 */
    for (i = 0; i < FMENU_ATTR_N; i++)
    {
        set_kind(K_GETDIGITS, (Uint16)i, 0, 0, 0, 0);
        run_case();
    }
}

/* NoSameDeal 的**回卷**判据: `data > upper` 与 `data < lower`。
 *
 * 为什么要单独一个阶段: 这两条判据只有在"data 步进后**恰好**落在界上"时才与
 * `>=` / `<=` 写法分得开。上面 phase_basics 那段用的是真表 + 随机 data —— 撞值
 * (data 等于别的功能码的值) 本来就不常有, 撞值**又恰好**在 upper - 1 / lower + 1
 * 上更几乎不可能。实测: M130 (`data > upper` 改 `>=`) 在 20000 个随机用例下是
 * MISSED, 补了这个阶段才抓到。
 *
 * 构造 (用真表 ref_frqSrcFuncIndex = {frqXSrc, frqYSrc} 的两个下标):
 *   上翻: 把 funcCode.all[frqXSrc] 设成 upper - 1, 用 index = frqYSrc 调用,
 *         delta = +1, data 起点 = upper - 2。
 *         LimitDeal 先把 data 顶到 upper - 1, 内层正好撞上 -> `data += 1` 得 upper。
 *         原写法 `upper > upper` 不成立, 保留 upper; 变异 `upper >= upper` 成立,
 *         回卷成 lower -> 外层 `data == lower` 立刻 break, 返回 lower。
 *         两侧返回值 upper vs lower, 必然不同。
 *   下翻: 把 funcCode.all[frqXSrc] 设成 lower + 1, delta = -1,
 *         data 起点 = lower + 2, upper = lower + 3。
 *         LimitDeal 把 data 拉到 lower + 1, 内层正好撞上 -> `data -= 1` 得 lower。
 *         原写法保留 lower (外层随即 break); 变异回卷成 upper, 外层不 break,
 *         再扫一遍后返回 upper。
 *
 * upper - lower 固定为 3 (奇数), 两个方向的落点都恰好压在界上; 取值 <= 8, 不碰
 * int16 边界。写的是 g_in.fc —— 也就是**输入状态**, 由 install() 每侧各 memcpy
 * 一次, 所以两侧拿到完全相同的 funcCode, 不构成"喂不同输入"。base_state() 在
 * 下一个阶段之前会把 g_in 整个复位, 不会泄漏到别的阶段。
 *
 * 注: `upper < lower` 与"步长改 2"这两类**不终止**的形态刻意不喂 (见文件头第 5 条
 * 与 negative_control.py 里 M132 的原处注释) —— 那会让测试台挂死而不是报差异。
 */
static void phase_no_same_wrap(void)
{
    Uint16 idxCollide = ref_frqSrcFuncIndex[0];
    Uint16 idxSelf = ref_frqSrcFuncIndex[1];
    static const Uint16 lo[4] = {0, 1, 2, 5};
    int t;

    for (t = 0; t < 4; t++)
    {
        Uint16 lower = lo[t];
        Uint16 upper = (Uint16)(lower + 3);

        /* 上翻: 起点 upper - 2, LimitDeal 顶到 upper - 1 = 碰撞值。 */
        g_in.fc.all[idxCollide] = (Uint16)(upper - 1);
        set_kind(K_NOSAME, idxSelf, (Uint16)(upper - 2), upper, lower, 1);
        g_in.tbl = ref_frqSrcFuncIndex;
        g_in.sarg = 2;
        run_case();

        /* 下翻: 起点 lower + 2, LimitDeal 拉到 lower + 1 = 碰撞值。 */
        g_in.fc.all[idxCollide] = (Uint16)(lower + 1);
        set_kind(K_NOSAME, idxSelf, (Uint16)(lower + 2), upper, lower,
                 (Uint16)0xFFFF);
        g_in.tbl = ref_frqSrcFuncIndex;
        g_in.sarg = 2;
        run_case();
    }
}

static void phase_modify_updown(void)
{
    int idx, d;

    static const int16 deltas[9] = {0, 1, -1, 10, -10, 100, -100, 1000, -1000};

    for (idx = 0; idx < FMENU_ATTR_N; idx++)
    {
        for (d = 0; d < 9; d++)
        {
            Uint16 start;

            /* 起点取三个有代表性的位置: 属性表的 init、下界、上界附近。 */
            start = funcCodeAttribute[idx].init;
            set_kind(K_MODUPDOWN, (Uint16)idx, (Uint16)(d + 1), 0, 0, 0);
            g_in.arg0 = (Uint16)idx;
            g_in.sarg = deltas[d];
            g_in.arg4 = start;
            run_case();

            set_kind(K_MODUPDOWN, (Uint16)idx, (Uint16)(d + 100), 0, 0, 0);
            g_in.arg0 = (Uint16)idx;
            g_in.sarg = deltas[d];
            g_in.arg4 = 0;
            run_case();

            set_kind(K_MODUPDOWN, (Uint16)idx, (Uint16)(d + 200), 0, 0, 0);
            g_in.arg0 = (Uint16)idx;
            g_in.sarg = deltas[d];
            g_in.arg4 = 65535;
            run_case();

            /* operateDigit 也扫一遍 —— 它是"按位改数"的位选。 */
            {
                int od;
                for (od = 0; od < 5; od++)
                {
                    set_kind(K_MODUPDOWN, (Uint16)idx, (Uint16)(300 + d * 8 + od), 0, 0, 0);
                    g_in.arg0 = (Uint16)idx;
                    g_in.sarg = deltas[d];
                    g_in.arg4 = start;
                    g_in.menuAttri[MENU_LEVEL_3].operateDigit = (Uint16)od;
                    run_case();
                }
            }
        }
    }
}

static void phase_modify_enter(void)
{
    int idx, v;

    for (idx = 0; idx < FMENU_ATTR_N; idx++)
    {
        Uint16 lo, hi;

        /* ModifyFunccodeEnter 把 dataNew 原样写进 funcCode, 所以喂进去的必须是
         * 该功能码的合法值 —— 否则后续会把它当表下标用 (frqPoint -> decNumber[])。
         * 值域从属性表解析, 见 fc_value_range() 的注释。 */
        fc_value_range(idx, &lo, &hi);

        for (v = 0; v < 5; v++)
        {
            Uint16 data = (v == 0) ? funcCodeAttribute[idx].init
                        : (v == 1) ? funcCode.all[idx]
                        : (v == 2) ? lo
                        : (v == 3) ? hi
                                   : (Uint16)(funcCodeAttribute[idx].init ^ 0x0001);

            /* v == 4 想测"差一位"的邻居值, 但必须留在值域内。 */
            if (data < lo)
            {
                data = lo;
            }
            if (data > hi)
            {
                data = hi;
            }

            set_kind(K_MODENTER, (Uint16)idx, (Uint16)(v + 1), 0, 0, 0);
            g_in.arg0 = (Uint16)idx;
            g_in.arg1 = data;
            run_case();
        }
    }

    /* 再扫一遍: 用**值域端点**做 data, 并把 ValidateInvType 的两个方向都摆一遍。
     * 值必须留在 [lo, hi] 内, 理由同上一段 (dataNew 会被原样写进 funcCode)。 */
    for (idx = 0; idx < FMENU_ATTR_N; idx += 7)
    {
        Uint16 lo, hi;

        fc_value_range(idx, &lo, &hi);

        set_kind(K_MODENTER, (Uint16)idx, hi, 0, 0, 0);
        g_in.arg0 = (Uint16)idx;
        g_in.arg1 = hi;
        g_in.validateInvTypeRet = 0;
        run_case();

        g_in.validateInvTypeRet = 1;
        set_kind(K_MODENTER, (Uint16)idx, lo, 0, 0, 0);
        g_in.arg1 = lo;
        run_case();
    }
}

static void phase_display_attr(void)
{
    int i, j, k;

    static const Uint16 datas[10] =
    {
        0, 1, 9, 10, 999, 1000, 9999, 32767, 32768, 65535
    };

    /* attribute 覆盖: signal x point x multiLimit x unit 的组合。 */
    for (i = 0; i < 10; i++)
    {
        for (j = 0; j < 8; j++)
        {
            for (k = 0; k < 6; k++)
            {
                union FUNC_ATTRIBUTE a;
                a.all = 0;
                a.bit.signal = (Uint16)(j & 1);
                a.bit.point = (Uint16)(k % 8);
                a.bit.multiLimit = (Uint16)(j & 3);
                a.bit.unit = (Uint16)(k % 8);
                a.bit.displayBits = (Uint16)((j + k) % 8);

                set_kind(K_UPDATTR, datas[i], (Uint16)(j * 100 + k), 0, 0, 0);
                g_in.arg0 = datas[i];
                g_in.attr = a;
                g_in.menuLevel = MENU_LEVEL_0;
                run_case();

                /* 换到 MENU_LEVEL_3 + U 组 (走"低位显示"那一支) */
                set_kind(K_UPDATTR, datas[i], (Uint16)(10000 + j * 100 + k), 0, 0, 0);
                g_in.arg0 = datas[i];
                g_in.attr = a;
                g_in.menuLevel = MENU_LEVEL_3;
                g_in.curFc.group = FUNCCODE_GROUP_U0;
                g_in.curFc.grade = (Uint16)(k % U0NUM);
                run_case();

                /* 换到 MENU_LEVEL_2 (走 curFcDispDigits 那一支) */
                set_kind(K_UPDATTR, datas[i], (Uint16)(20000 + j * 100 + k), 0, 0, 0);
                g_in.arg0 = datas[i];
                g_in.attr = a;
                g_in.menuLevel = MENU_LEVEL_2;
                g_in.curFc.group = 5;
                g_in.u16[I_CURFCDISPDIGITS] = (Uint16)(1 + (k % 5));
                run_case();
            }
        }
    }

    /* ticker4LowerDisp 的两侧: 0 与 非 0 */
    set_kind(K_UPDATTR, 12345, 30000, 0, 0, 0);
    g_in.arg0 = 12345;
    g_in.attr.all = 0x094A;
    g_in.menuLevel = MENU_LEVEL_0;
    g_in.u16[I_TICKER4LOWERDISP] = 5;
    run_case();
}

static void phase_display_group(void)
{
    int g;

    for (g = 0; g < FUNCCODE_GROUP_NUM + 8; g++)
    {
        set_kind(K_UPDGROUP, (Uint16)g, 0, 0, 0, 0);
        g_in.arg0 = (Uint16)g;
        run_case();
    }
}

static void phase_display_menu0(void)
{
    int i, b;

    /* UpdateMenu0DispStatus: errorCode/tuneCmd 的四种组合 + 前一拍的值 */
    for (i = 0; i < 4; i++)
    {
        for (b = 0; b < 4; b++)
        {
            int j;
            for (j = 0; j < 3; j++)
            {
                set_kind(K_UPDMENU0ST, (Uint16)(i * 100 + b * 10 + j), 0, 0, 0, 0);
                g_in.errorCode = (Uint16)((i & 1) ? ERROR_UV : ERROR_NONE);
                g_in.tuneCmd = (Uint16)((i & 2) ? 1 : 0);
                g_in.menu0DispStatus = (enum MENU0_DISP_STATUS)j;
                run_case();
            }
        }
    }

    /* UpdateMenu0DisplayBuffer: 三个分支 (RUN_STOP / ERROR / TUNE) x
     * bFrqDigital 两态 x dispParaRunBit/StopBit 若干。 */
    for (i = 0; i < 3; i++)
    {
        int bd;
        for (b = 0; b < 2; b++)
        {
            for (bd = 0; bd < 6; bd++)
            {
                static const Uint16 bits[6] = {0, 1, 4, 14, 19, 31};
                set_kind(K_UPDMENU0, (Uint16)(i * 1000 + b * 100 + bd), 0, 0, 0, 0);
                g_in.menu0DispStatus = (enum MENU0_DISP_STATUS)i;
                g_in.bFrqDigital = (Uint16)b;
                g_in.u16[I_BFRQDIGITALDONE4WAITDELAY] = (Uint16)(b ? 1 : 0);
                g_in.runFlag.all = (Uint16)(b ? 1 : 0);
                g_in.fc.code.dispParaRunBit = bits[bd];
                g_in.fc.code.dispParaStopBit = (Uint16)(bd % STOP_DISPLAY_NUM);
                g_in.fc.code.frqPoint = (Uint16)(bd % 5);
                g_in.fc.code.speedDispPointPos = (Uint16)(bd % 5);
                run_case();
            }
        }
    }
}

static void phase_display_menu123(void)
{
    int i, b;

    /* UpdateMenu1DisplayBuffer: group 扫描。
     *
     * **group 必须是真实存在的组号**。UpdateGroupDisplay() 的分档是
     *     group <= FUNCCODE_GROUP_FP  -> disp0 = group
     *     group <= FUNCCODE_GROUP_CF  -> disp0 = (group - FP - 1) % 16
     *     else                        -> disp0 = group - FUNCCODE_GROUP_U0
     * 而 CF = 64 与 U0 = 67 之间夹着 **65 / 66 两个空号**: 走 else 会算出
     * disp0 = 65534 / 65535, 紧接着 UpdateMenu1DisplayBuffer 里的
     * `DISPLAY_CODE[groupDisplay.disp0]` 就是 131 KB 的越界读 (宿主上 SIGSEGV)。
     *
     * 真机上这属于**既有缺陷**: 若 funcCodeGradeCurMenuMode[65]/[66] 为 0,
     * GroupUpDown 的 LimitOverTurnDeal 就可能停在空号上, 目标端同样会去读
     * DISPLAY_CODE 后面老远的地方 (只是不触发页错误)。宿主无法复现"读相邻
     * .econst"这件事, 所以这里按**合法组号集合**扫 (已登记在文件头缺陷表)。
     */
    for (i = 0; i <= FUNCCODE_GROUP_CF; i++)
    {
        set_kind(K_UPDMENU1, (Uint16)i, 0, 0, 0, 0);
        g_in.curFc.group = (Uint16)i;
        run_case();
    }
    for (i = FUNCCODE_GROUP_U0; i < FUNCCODE_GROUP_NUM; i++)
    {
        set_kind(K_UPDMENU1, (Uint16)i, 0, 0, 0, 0);
        g_in.curFc.group = (Uint16)i;
        run_case();
    }

    /* UpdateMenu2DisplayBuffer: menuMode x checkMenuModeSerachNone x
     * userCustom[userMenuModeFcIndex] x grade */
    for (i = 0; i < 3; i++)
    {
        for (b = 0; b < 2; b++)
        {
            int g;
            for (g = 0; g < 6; g++)
            {
                set_kind(K_UPDMENU2, (Uint16)(i * 100 + b * 10 + g), 0, 0, 0, 0);
                g_in.menuMode = (enum MENU_MODE)i;
                g_in.u16[I_CHECKMENUMODESERACHNONE] = (Uint16)b;
                g_in.curFc.group = (Uint16)(g + 1);
                g_in.curFc.grade = (Uint16)(g * 3);
                g_in.fc.code.userCustom[g_in.userMenuModeFcIndex] = (Uint16)(b ? 0 : 1);
                g_in.menuAttri[MENU_LEVEL_2].operateDigit = (Uint16)(g % 4);
                run_case();
            }
        }
    }

    /* UpdateMenu3DisplayBuffer: 四个组分支 + DIDO/DiFunc 特例 + 各特例 index */
    for (i = 0; i < 5; i++)
    {
        int g;
        for (g = 0; g < 8; g++)
        {
            set_kind(K_UPDMENU3, (Uint16)(i * 100 + g), 0, 0, 0, 0);
            g_in.curFc.group = (i == 0) ? FUNCCODE_GROUP_U0
                            : (i == 1) ? (Uint16)(FUNCCODE_GROUP_U0 + 1)
                            : (i == 2) ? FUNCCODE_GROUP_UF
                            : (i == 3) ? (Uint16)(FUNCCODE_GROUP_U0 + 3)
                                       : 5;
            g_in.curFc.grade = (Uint16)(g * 12);
            g_in.curFc.index = (Uint16)(g * 50);
            g_in.u16[I_MENU3NUMBER] = (Uint16)(0x1000 + g);
            g_in.menuAttri[MENU_LEVEL_3].operateDigit = (Uint16)(g % 5);
            run_case();
        }
    }

    /* 直接把 index 摆到 diStatus/doStatus/diFunc 的特例上。 */
    {
        Uint16 specials[4];
        int t;
        specials[0] = GetCodeIndex(funcCode.group.u0[DISP_DI_STATUS_SPECIAL1]);
        specials[1] = GetCodeIndex(funcCode.group.u0[DISP_DO_STATUS_SPECIAL1]);
        specials[2] = GetCodeIndex(funcCode.group.u0[DISP_DI_FUNC_SPECIAL1]);
        specials[3] = GetCodeIndex(funcCode.group.u0[DISP_DI_FUNC_SPECIAL2]);
        for (t = 0; t < 4; t++)
        {
            int v;
            for (v = 0; v < 4; v++)
            {
                static const Uint32 vals[4] =
                {
                    0x0UL, 0x000FFFFFUL, 0x0000AAAAUL, 0x00055555UL
                };
                set_kind(K_UPDMENU3, specials[t], (Uint16)(v + 1), 0, 0, 0);
                g_in.curFc.index = specials[t];
                g_in.curFc.group = FUNCCODE_GROUP_U0;
                g_in.curFc.grade = 0;
                g_in.diStatus.a.all = vals[v];
                g_in.doStatus.a.all = vals[(v + 1) & 3];
                g_in.diFunc.f1.all = vals[v];
                g_in.diFunc.f2.all = vals[(v + 2) & 3];
                run_case();
            }
        }
    }

    /* UpdateMenu3DisplayBuffer 的 "tuneCmd 索引 -> 只读" 那一支:
     * 遍历全部 index, 把 runSrc 两个方向都摆一遍。 */
    for (i = 0; i < FMENU_ATTR_N; i += 3)
    {
        set_kind(K_UPDMENU3, (Uint16)i, 900, 0, 0, 0);
        g_in.curFc.index = (Uint16)i;
        g_in.curFc.group = 5;
        g_in.curFc.grade = (Uint16)(i % 40);
        g_in.runSrc = FUNCCODE_runSrc_PANEL;
        run_case();
        set_kind(K_UPDMENU3, (Uint16)i, 901, 0, 0, 0);
        g_in.curFc.index = (Uint16)i;
        g_in.curFc.group = 5;
        g_in.curFc.grade = (Uint16)(i % 40);
        g_in.runSrc = (Uint16)(FUNCCODE_runSrc_PANEL + 1);
        run_case();
    }
}

static void phase_display_misc(void)
{
    int i;

    /* UpdateErrorDisplayBuffer: errorCode x runFlag.bit.run x errorAttribute.level */
    for (i = 0; i < 8; i++)
    {
        int r;
        for (r = 0; r < 3; r++)
        {
            set_kind(K_UPDERR, (Uint16)(i * 10 + r), 0, 0, 0, 0);
            g_in.errorCode = (Uint16)(i * 111);
            g_in.runFlag.all = (Uint16)(r == 1 ? 1 : 0);
            g_in.errorAttribute.all = 0;
            g_in.errorAttribute.bit.level = (Uint16)(r == 2 ? ERROR_LEVEL_RUN
                                                            : ERROR_LEVEL_NO_ERROR);
            run_case();
        }
    }

    /* UpdateTuneDisplayBuffer 无输入, 只跑一次也够 —— 但为了统一仍摆几个状态。 */
    for (i = 0; i < 3; i++)
    {
        set_kind(K_UPDTUNE, (Uint16)i, 0, 0, 0, 0);
        g_in.menuLevel = (enum MENU_LEVEL)i;
        run_case();
    }

    /* UpdateMenuPwdHintDisplayBuffer / UpdateMenuPwdInputDisplayBuffer:
     * DEBUG_RANDOM_FACPASS = 0, 所以 hint 走的是"全横线"那一支。 */
    for (i = 0; i < 4; i++)
    {
        set_kind(K_UPDPWDHINT, (Uint16)i, 0, 0, 0, 0);
        g_in.facPassViewStatus = (enum FAC_PASS_RANDOM_VIEW_OPERATE)(i & 1);
        g_in.menuAttri[MENU_LEVEL_PWD_HINT].winkFlag = (Uint16)i;
        run_case();
    }
    for (i = 0; i < 6; i++)
    {
        set_kind(K_UPDPWDINPUT, (Uint16)i, 0, 0, 0, 0);
        g_in.u16[I_MENUPWDNUMBER] = (Uint16)(i * 11111);
        g_in.menuAttri[MENU_LEVEL_PWD_INPUT].operateDigit = (Uint16)(i % 5);
        run_case();
    }

    /* UpdateDisplayBufferVisualIoStatus / DiFunc */
    for (i = 0; i < 8; i++)
    {
        set_kind(K_UPDIOSTATUS, (Uint16)i, 0, 0, 0, 0);
        g_in.larg = (Uint32)(0x01234567UL * (Uint32)(i + 1));
        run_case();
    }
    for (i = 0; i < 8; i++)
    {
        set_kind(K_UPDDIFUNC, (Uint16)(i * 31), (Uint16)(i + 1), 0, 0, 0);
        g_in.arg0 = (Uint16)(i * 31);
        g_in.larg = (Uint32)(0x89ABCDEFUL >> (i & 7));
        run_case();
    }

    /* UpdateU0Data: 全部输入非平凡时跑一遍, 再把几个"会被清零"的槽位摆满。 */
    set_kind(K_UPDU0, 0, 0, 0, 0, 0);
    run_case();
}

static void phase_disp_data(void)
{
    int i;

    /* DispDataDeal: runFlag.bit.run x runMode x 各种输入 */
    for (i = 0; i < 2; i++)
    {
        int m;
        for (m = 0; m < 3; m++)
        {
            set_kind(K_DISPDATA, (Uint16)(i * 10 + m), 0, 0, 0, 0);
            g_in.runFlag.all = (Uint16)i;
            g_in.runMode = (m == 1) ? RUN_MODE_TORQUE_CTRL : (Uint16)m;
            g_in.frq = (int32)((i * 1000) + 12345);
            g_in.frqAim = (int32)(-i * 777);
            g_in.frqDroop = (int32)(-m * 555);
            g_in.frqRun = (int32)(m * 999);
            g_in.pidFunc.ref = (int32)((m - 1) * 1234);
            g_in.pidFunc.fdb = (int32)((i - 1) * 4321);
            g_in.pulseInFrq = (Uint32)(1000 * (i + 1) + m);
            g_in.torqueCurrent = (Uint16)(1234 + i);
            g_in.currentPu = (Uint16)(4096 - m);
            g_in.fc.code.speedDispCoeff = (Uint16)(100 * (i + 1));
            g_in.fc.code.pidDisp = (Uint16)(1000 + m);
            g_in.fc.code.runDir = (Uint16)m;
            run_case();
        }
    }

    /* 负值 / 极值 */
    set_kind(K_DISPDATA, 100, 0, 0, 0, 0);
    g_in.frqDroop = -32768;
    g_in.frq = -65535;
    g_in.frqAim = 65535;
    g_in.pidFunc.ref = -100000;
    g_in.pidFunc.fdb = 100000;
    run_case();
}

static void phase_cycle_shift(void)
{
    int i;

    /* cycleShiftDeal 的 do-while 要求 para 在 [0, max) 里**至少有一位**。
     * 这里把 para 摆成"只有低位有 1"或"低 16 位全 1"两种, 于是循环一定终止。 */
    for (i = 0; i < 6; i++)
    {
        /* 掩码必须是 **Uint16** —— cycleShiftDeal 里 para 是这样拼出来的:
         *   stop 模式  para = ledDispParaStop;                      (16 位, max=16)
         *   run  模式  para = ledDispParaRun1 | (ledDispParaRun2<<16); (max=32)
         * 所以 Run1 是低 16 位、Run2 是高 16 位。 */
        static const Uint16 stopMask[6] =
        {
            0x0001, 0x000F, 0x8000, 0xFFFF, 0x0000, 0x0003
        };
        int f;
        for (f = 0; f < 2; f++)
        {
            int b;
            for (b = 0; b < 4; b++)
            {
                static const Uint16 bits[4] = {0, 3, 7, 15};
                set_kind(K_CYCLESHIFT, (Uint16)(i * 100 + f * 10 + b), 0, 0, 0, 0);
                g_in.runFlag.all = (Uint16)f;
                g_in.fc.code.dispParaStopBit = bits[b];
                g_in.fc.code.dispParaRunBit = bits[b];
                g_in.fc.code.ledDispParaStop = stopMask[i] ? stopMask[i] : 1;
                g_in.fc.code.ledDispParaRun1 = stopMask[i] ? stopMask[i] : 1;
                g_in.fc.code.ledDispParaRun2 = 0;
                g_in.errorCode = (Uint16)((b & 1) ? ERROR_UV : ERROR_NONE);
                g_in.tuneCmd = (Uint16)((b & 2) ? 1 : 0);
                run_case();
            }
        }
    }
}

static void phase_menu_grade(void)
{
    int i, f;

    /* MotorDebugFcDeal: motorDebugFc 由 funcCode.code.motorDebugFc 派生 */
    for (i = 0; i < 12; i++)
    {
        static const Uint16 mv[12] =
        {
            0, 1, 99, 100, 101, 1203, 1299, 1300, 9999, 65535, 500, 12
        };
        set_kind(K_MOTORDEBUG, mv[i], 0, 0, 0, 0);
        g_in.fc.code.motorDebugFc = mv[i];
        run_case();
    }

    /* UpdataFuncCodeGrade: funcParaView 的 4 个十进制位两两不同 */
    for (i = 0; i < 12; i++)
    {
        static const Uint16 fv[12] =
        {
            0, 1, 9, 10, 1111, 2222, 9999, 1000, 100, 10, 65535, 1234
        };
        set_kind(K_UPDGRADE, fv[i], 0, 0, 0, 0);
        g_in.fc.code.funcParaView = fv[i];
        g_in.fc.code.aiaoCalibrateDisp = (Uint16)(i & 1);
        run_case();
    }

    /* GroupUpDown: limit 表 x group x flag (flag 只能是 0/1/2 -> deltaK[flag]) */
    for (f = 0; f < 3; f++)
    {
        for (i = 0; i < 12; i++)
        {
            set_kind(K_GROUPUPDOWN, g_various[i], (Uint16)f, 0, 0, 0);
            g_in.tbl = funcCodeGradeCurMenuMode;
            run_case();
        }
    }

    /* DealUserMenuModeGroupGrade: userCustom 的每一项都当一次起点 */
    for (i = 0; i < FENUM; i++)
    {
        for (f = 0; f < 3; f++)
        {
            set_kind(K_DEALUSERGRADE, (Uint16)(i * 10 + f), 0, 0, 0, 0);
            g_in.userMenuModeFcIndex = (int16)(i % FENUM);
            g_in.arg0 = (Uint16)f;
            run_case();
        }
    }

    /* DealCheckMenuModeGroupGrade: checkMenuModeDealStatus 四态 x flag x
     * "找得到/找不到"两种 funcCode.all 摆法。
     * 注意 curFc.group 必须 < FUNCCODE_GROUP_U0 —— 该函数会顺着
     * funcCodeGradeCurMenuMode 走到 funcCodeAttribute, U 组越界。 */
    for (i = 0; i < 4; i++)
    {
        for (f = 0; f < 2; f++)
        {
            int g, none;
            for (g = 0; g < 4; g++)
            {
                for (none = 0; none < 2; none++)
                {
                    set_kind(K_DEALCHECKGRADE, (Uint16)(i * 1000 + f * 100 + g * 10 + none), 0, 0, 0, 0);
                    g_in.checkMenuModeDealStatus = (enum CHECK_MENU_MODE_DEAL)i;
                    g_in.curFc.group = (Uint16)(1 + g * 5);
                    g_in.curFc.grade = (Uint16)(g * 2);
                    g_in.u16[I_CHECKMENUMODESERACHNONE] = (Uint16)none;
                    g_in.arg0 = (Uint16)f;
                    /* none == 1: 把整条链上的值都摆成 != init, 让搜索立即结束;
                     * none == 0: 摆成 == init, 让搜索一直找不到。 */
                    {
                        int k;
                        for (k = 0; k < FNUM_ALL; k++)
                        {
                            g_in.fc.all[k] = (Uint16)(none ? (k + 1) : k);
                        }
                    }
                    run_case();
                }
            }
        }
    }
}

static void phase_menu_state(void)
{
    int i;

    /* MenuInit */
    set_kind(K_MENUINIT, 0, 0, 0, 0, 0);
    run_case();

    /* MenuModeDeal: menuMode x menuModeOld x userCustom/menuMode 的十进制位 */
    for (i = 0; i < 3; i++)
    {
        int o, mm;
        for (o = 0; o < 3; o++)
        {
            for (mm = 0; mm < 6; mm++)
            {
                static const Uint16 mmv[6] = {0, 1, 9, 10, 11, 99};
                set_kind(K_MENUMODEDEAL, (Uint16)(i * 100 + o * 10 + mm), 0, 0, 0, 0);
                g_in.menuMode = (enum MENU_MODE)i;
                g_in.menuModeOld = (enum MENU_MODE)o;
                g_in.fc.code.menuMode = mmv[mm];
                g_in.userMenuModeFcIndex = (int16)(mm % FENUM);
                run_case();
            }
        }
    }

    /* MenuModeSwitch: menuMode/menuModeTmp/menuModeOld/menuLevel/curFc 的组合 */
    for (i = 0; i < 3; i++)
    {
        int t, l;
        for (t = 0; t < 3; t++)
        {
            for (l = 0; l < MENU_LEVEL_NUM; l++)
            {
                set_kind(K_MENUMODESW, (Uint16)(i * 100 + t * 10 + l), 0, 0, 0, 0);
                g_in.menuMode = (enum MENU_MODE)i;
                g_in.menuModeTmp = (enum MENU_MODE)t;
                g_in.menuModeOld = (enum MENU_MODE)((i + 1) % 4);
                g_in.menuLevel = (enum MENU_LEVEL)l;
                g_in.curFc.group = (Uint16)(1 + (l * 7));
                g_in.curFc.grade = (Uint16)(l * 3);
                g_in.fc.code.userPassword = (Uint16)((l & 1) ? 1234 : 0);
                g_in.fc.code.userPasswordReadOnly = (Uint16)((l & 2) ? 1 : 0);
                run_case();
            }
        }
    }

    /* Menu0AddMenuLevel: menuMode 三个方向 x tuneCmd */
    for (i = 0; i < 3; i++)
    {
        int t;
        for (t = 0; t < 2; t++)
        {
            set_kind(K_MENU0ADDLEVEL, (Uint16)(i * 10 + t), 0, 0, 0, 0);
            g_in.menuMode = (enum MENU_MODE)i;
            g_in.tuneCmd = (Uint16)t;
            g_in.curFc.group = FC_GROUP_FACTORY;
            g_in.curFc.grade = 5;
            run_case();
        }
    }
}

static void phase_menu_handlers(void)
{
    int i;

    /* 每个 handler 都在"状态机各档"下跑一遍, 而不是只跑一次 —— 状态机是序列
     * 相关的, 单次调用看不出差别。 */
    for (i = 0; i < MENU_LEVEL_NUM; i++)
    {
        int mm;
        for (mm = 0; mm < 4; mm++)
        {
            Uint16 lv = (Uint16)i;
            Uint16 mode = (Uint16)mm;

            /* OnPrg */
            set_kind(K_MENU0PRG, lv, mode, 0, 0, 0);
            g_in.menuLevel = (enum MENU_LEVEL)i;
            g_in.menuMode = (enum MENU_MODE)mm;
            g_in.tuneCmd = (Uint16)(mm & 1);
            g_in.runFlag.all = (Uint16)((mm >> 1) & 1);
            g_in.fc.code.userPassword = (Uint16)((mm & 2) ? 1234 : 0);
            run_case();

            set_kind(K_MENU1PRG, lv, mode, 0, 0, 0);
            g_in.menuLevel = (enum MENU_LEVEL)i;
            g_in.menuMode = (enum MENU_MODE)mm;
            run_case();

            set_kind(K_MENU2PRG, lv, mode, 0, 0, 0);
            g_in.menuLevel = (enum MENU_LEVEL)i;
            g_in.menuMode = (enum MENU_MODE)mm;
            g_in.factoryPwdStatus = FMENU_FACTORY_PWD_UNLOCK;
            run_case();

            set_kind(K_MENU3PRG, lv, mode, 0, 0, 0);
            g_in.menuLevel = (enum MENU_LEVEL)i;
            g_in.menuMode = (enum MENU_MODE)mm;
            run_case();

            /* OnEnter */
            set_kind(K_MENU0ENTER, lv, mode, 0, 0, 0);
            g_in.menuLevel = (enum MENU_LEVEL)i;
            g_in.menuMode = (enum MENU_MODE)mm;
            g_in.menuModeStatus = (enum MENU_MODE_OPERATE)(mm & 1);
            run_case();

            set_kind(K_MENU1ENTER, lv, mode, 0, 0, 0);
            g_in.menuLevel = (enum MENU_LEVEL)i;
            g_in.menuMode = (enum MENU_MODE)mm;
            g_in.menuModeStatus = (enum MENU_MODE_OPERATE)(mm & 1);
            g_in.curFc.group = FC_GROUP_FACTORY;
            g_in.curFc.grade = (Uint16)(mm + 1);
            run_case();

            set_kind(K_MENU2ENTER, lv, mode, 0, 0, 0);
            g_in.menuLevel = (enum MENU_LEVEL)i;
            g_in.menuMode = (enum MENU_MODE)mm;
            g_in.menuModeStatus = (enum MENU_MODE_OPERATE)(mm & 1);
            g_in.u16[I_CHECKMENUMODESERACHNONE] = (Uint16)((mm >> 1) & 1);
            g_in.curFc.group = (Uint16)(1 + mm * 3);
            g_in.curFc.grade = (Uint16)(mm * 2);
            g_in.factoryPwdStatus = FMENU_FACTORY_PWD_UNLOCK;
            run_case();

            set_kind(K_MENU3ENTER, lv, mode, 0, 0, 0);
            g_in.menuLevel = (enum MENU_LEVEL)i;
            g_in.menuMode = (enum MENU_MODE)mm;
            g_in.menuModeStatus = (enum MENU_MODE_OPERATE)(mm & 1);
            g_in.curFc.index = (Uint16)(20 + mm);
            g_in.curFc.group = (Uint16)(1 + mm * 3);
            g_in.curFc.grade = (Uint16)(mm * 2);
            g_in.u16[I_MENU3NUMBER] = (Uint16)(0x100 + mm);
            g_in.rwMode = (Uint16)(mm & 1);
            run_case();

            /* OnUp / OnDown */
            set_kind(K_MENU1UP, lv, mode, 0, 0, 0);
            g_in.curFc.group = (Uint16)(1 + mm * 3);
            g_in.curFc.grade = (Uint16)(mm + 2);
            run_case();

            set_kind(K_MENU2UP, lv, mode, 0, 0, 0);
            g_in.curFc.group = (Uint16)(1 + mm * 3);
            g_in.curFc.grade = (Uint16)(mm + 2);
            run_case();

            set_kind(K_MENU3UP, lv, mode, 0, 0, 0);
            g_in.curFc.group = (Uint16)(1 + mm * 3);
            g_in.curFc.grade = (Uint16)(mm + 2);
            run_case();

            set_kind(K_MENU1DOWN, lv, mode, 0, 0, 0);
            g_in.curFc.group = (Uint16)(1 + mm * 3);
            g_in.curFc.grade = (Uint16)(mm + 2);
            run_case();

            set_kind(K_MENU2DOWN, lv, mode, 0, 0, 0);
            g_in.curFc.group = (Uint16)(1 + mm * 3);
            g_in.curFc.grade = (Uint16)(mm + 2);
            run_case();

            set_kind(K_MENU3DOWN, lv, mode, 0, 0, 0);
            g_in.curFc.group = (Uint16)(1 + mm * 3);
            g_in.curFc.grade = (Uint16)(mm + 2);
            run_case();

            /* OnUpDown (带 flag) */
            set_kind(K_MENU0UP, lv, mode, 0, 0, 0);
            g_in.tuneCmd = (Uint16)(mm & 1);
            g_in.upDownDelta = (Uint16)(mm + 1);
            run_case();

            set_kind(K_MENU1UPDN, lv, (Uint16)(mode * 2), 0, 0, 0);
            g_in.curFc.group = (Uint16)(1 + mm * 3);
            run_case();
            set_kind(K_MENU1UPDN, lv, (Uint16)(mode * 2 + 1), 0, 0, 0);
            g_in.curFc.group = (Uint16)(1 + mm * 3);
            run_case();

            set_kind(K_MENU2UPDN, lv, (Uint16)(mode * 2), 0, 0, 0);
            g_in.menuMode = (enum MENU_MODE)mm;
            g_in.curFc.group = (Uint16)(1 + mm * 3);
            g_in.curFc.grade = (Uint16)(mm + 1);
            g_in.menuAttri[MENU_LEVEL_2].operateDigit = (Uint16)(mm % 4);
            g_in.userMenuModeFcIndex = (int16)(mm % FENUM);
            run_case();

            set_kind(K_MENU2UPDN, lv, (Uint16)(mode * 2 + 1), 0, 0, 0);
            g_in.menuMode = (enum MENU_MODE)mm;
            g_in.curFc.group = (Uint16)(1 + mm * 3);
            g_in.curFc.grade = (Uint16)(mm + 1);
            g_in.menuAttri[MENU_LEVEL_2].operateDigit = (Uint16)(mm % 4);
            g_in.userMenuModeFcIndex = (int16)(mm % FENUM);
            run_case();

            set_kind(K_MENU3UPDN, lv, (Uint16)(mode * 2), 0, 0, 0);
            g_in.curFc.index = (Uint16)(20 + mm);
            g_in.curFc.group = (Uint16)(1 + mm * 3);
            g_in.curFc.grade = (Uint16)(mm + 1);
            g_in.menuAttri[MENU_LEVEL_3].operateDigit = (Uint16)(mm % 5);
            g_in.u16[I_MENU3NUMBER] = (Uint16)(mm * 1234);
            run_case();

            set_kind(K_MENU3UPDN, lv, (Uint16)(mode * 2 + 1), 0, 0, 0);
            g_in.curFc.index = (Uint16)(20 + mm);
            g_in.curFc.group = (Uint16)(1 + mm * 3);
            g_in.curFc.grade = (Uint16)(mm + 1);
            g_in.menuAttri[MENU_LEVEL_3].operateDigit = (Uint16)(mm % 5);
            g_in.u16[I_MENU3NUMBER] = (Uint16)(mm * 1234);
            run_case();

            /* OnShift */
            set_kind(K_MENU0SHIFT, lv, mode, 0, 0, 0);
            g_in.menu0DispStatus = (enum MENU0_DISP_STATUS)(mm % 4);
            g_in.u16[I_TICKER4LOWERDISP] = (Uint16)(mm * 2);
            g_in.runFlag.all = (Uint16)(mm & 1);
            g_in.fc.code.dispParaStopBit = 0;
            g_in.fc.code.ledDispParaStop = 0xFFFF;      /* Uint16 */
            run_case();

            set_kind(K_MENU1SHIFT, lv, mode, 0, 0, 0);
            run_case();

            set_kind(K_MENU2SHIFT, lv, mode, 0, 0, 0);
            g_in.menuMode = (enum MENU_MODE)mm;
            g_in.menuAttri[MENU_LEVEL_2].operateDigit = (Uint16)(mm % 4);
            run_case();

            set_kind(K_MENU3SHIFT, lv, mode, 0, 0, 0);
            g_in.curFc.group = (Uint16)(1 + mm * 3);
            g_in.u16[I_CURFCDISPDIGITS] = (Uint16)(1 + (mm % 5));
            g_in.menuAttri[MENU_LEVEL_3].operateDigit = (Uint16)(mm % 5);
            run_case();

            /* 通用 */
            set_kind(K_MENURUN, lv, mode, 0, 0, 0);
            run_case();
            set_kind(K_MENUSTOP, lv, mode, 0, 0, 0);
            run_case();

            set_kind(K_MENUMFK, lv, mode, 0, 0, 0);
            g_in.tuneCmd = (Uint16)(mm & 1);
            g_in.fc.code.mfkKeyFunc = (Uint16)(mm % 4);
            g_in.fc.code.runSrc = (Uint16)(mm & 1);
            g_in.runSrc = (Uint16)(mm & 1);
            run_case();

            /* MenuOnQuick: menuModeStatus == ON_QUICK 时 menuModeTmp 必须是
             * 1..3 —— 否则 `menuModeNext[(Uint16)menuModeTmp - 1]` 越界, 见文件
             * 头的既有缺陷 2。 */
            set_kind(K_MENUQUICK, lv, mode, 0, 0, 0);
            g_in.fc.code.menuMode = (Uint16)(0x0101 * ((mm % 3) + 1));
            g_in.menuModeStatus = (enum MENU_MODE_OPERATE)(mm & 1);
            g_in.menuModeTmp = (enum MENU_MODE)(1 + (mm % 3));
            run_case();
        }
    }
}

/* Menu3OnEnter 的两条"厂家口令 / 调参口令"支路 —— 由负向对照 M080/M081/M083
 * 逼出来的一相。
 *
 * 原来 phase_menu_handlers 的 K_MENU3ENTER 只喂 `curFc.index = 20 + mm`, 这个
 * 下标既不是 FACTORY_PWD_INDEX 也不是 GetCodeIndex(funcCode.code.tuneCmd), 于是
 * 基准第 829 行与第 843 行两个判据一次都没成立, 第 832~847 行整段在差分测试里
 * 成了死代码 —— M080/M081/M083 三条因此全被记成 MISSED。不是语义等价, 是测试台
 * 没走到。两条支路的入口条件不同, 分开写:
 *
 *   A. 厂家口令支路 (基准 829~842)
 *      - curFc.index == FACTORY_PWD_INDEX = GetCodeIndex(funcCode.code.factoryPassword)。
 *        这是个**编译期固定的结构体偏移**, 与 funcCode 的运行时内容无关, 所以
 *        驱动可以直接用同一个宏算出来 (两侧的 funcCode 是同一个对象)。
 *      - menuModeStatus != MENU_MODE_ON_QUICK, 否则第 760 行就转走了。
 *      - 第 766 行的门: 内层判据的第一项是 `FACTORY_PWD_INDEX != curFc.index`,
 *        此刻为假, 所以 funcCodeRwMode 取什么都不可能提前 return。
 *      - 第 776 行: runFlag.bit.run 两态各来一遍 (置 0 时绕过)。
 *      - curFc.group < FUNCCODE_GROUP_U0 (= 67): 用 FC_GROUP_FACTORY (= 15)。
 *      - 第 789 行 `#if DEBUG_F_RESTORE_COMPANY_PARA_DEAL` 在工程里为 1
 *        (F_DEBUG_RAM == 0, 见 f_debug.h / f_fcDeal.h), 但 paraInitMode 的下标
 *        与 factoryPassword 不同, 所以第 791 行判据为假 -> 走 else -> 第 829 行。
 *      - menu3Number 扫 COMPANY_PASSWORD (= 1430) 的两侧: 等号处第 832 行的
 *        `!=` 与 `==` 分道 (M080), 同时第 840 行真的执行到 (M081)。
 *      - factoryPwdStatus **预置成 LOCK**: M081 把 UNLOCK 改成 LOCK, 若预置成
 *        UNLOCK 则两侧都写 UNLOCK, 变异不可观测。
 *
 *   B. 调参口令支路 (基准 843~847)
 *      - curFc.index == GetCodeIndex(funcCode.code.tuneCmd)。
 *      - 第 766 行的门: 这时内层第一项为真, 必须靠第二项绕过 —— 要么
 *        ATTRIBUTE_READ_ONLY_ANYTIME == writable, 要么 funcCodeRwMode ==
 *        FUNCCODE_RW_MODE_NO_OPERATION。驱动选后者: 属性表是从真工程逐字抄来的,
 *        不便于按需改。
 *      - ValidateTuneCmd 的两侧各来一遍, 因为 M083 把 `==` 改成 `!=`, 两个方向
 *        都会分道。COMM_ERR_NONE 的条件 (基准 930 行): motorSn == MOTOR_SN_1,
 *        motorType == PMSM 且 value ∈ {11,12,13} (或 ACI 且 value ∈ {1,2}),
 *        errorCode == ERROR_NONE, runSrc == FUNCCODE_runSrc_PANEL。 */
static void phase_menu3_pwd_tune(void)
{
    Uint16 pwdIndex  = (Uint16)(&funcCode.code.factoryPassword - &funcCode.all[0]);
    Uint16 tuneIndex = (Uint16)(&funcCode.code.tuneCmd - &funcCode.all[0]);

    static const Uint16 m3num[] =
    {
        COMPANY_PASSWORD - 1U, COMPANY_PASSWORD, COMPANY_PASSWORD + 1U,
        0U, 0xFFFFU
    };
    static const Uint16 tcmd[] =
    {
        0U, FUNCCODE_tuneCmd_PMSM_11, FUNCCODE_tuneCmd_PMSM_12,
        FUNCCODE_tuneCmd_PMSM_13, FUNCCODE_tuneCmd_ACI_STATIC,
        FUNCCODE_tuneCmd_ACI_WHOLE, 9U, 0xFFFFU
    };
    unsigned i, j, k;

    /* ---- A. 厂家口令 ---- */
    for (i = 0; i < sizeof m3num / sizeof m3num[0]; i++)
    {
        for (j = 0; j < 2; j++)              /* factoryPwdStatus 预置 */
        {
            for (k = 0; k < 2; k++)          /* runFlag.bit.run */
            {
                set_kind(K_MENU3ENTER, pwdIndex, 0, 0, 0, 0);
                g_in.menuLevel = MENU_LEVEL_3;
                g_in.menuMode = MENU_MODE_USER;
                g_in.menuModeStatus = MENU_MODE_NONE;
                g_in.curFc.index = pwdIndex;
                g_in.curFc.group = FC_GROUP_FACTORY;
                g_in.curFc.grade = 0;
                g_in.u16[I_MENU3NUMBER] = m3num[i];
                g_in.factoryPwdStatus = j ? FMENU_FACTORY_PWD_UNLOCK
                                          : FMENU_FACTORY_PWD_LOCK;
                g_in.runFlag.all = (Uint16)k;
                g_in.rwMode = FUNCCODE_RW_MODE_NO_OPERATION;
                run_case();
            }
        }
    }

    /* ---- B. 调参口令 ---- */
    for (i = 0; i < sizeof tcmd / sizeof tcmd[0]; i++)
    {
        for (j = 0; j < 2; j++)              /* motorType: PMSM / ACI */
        {
            for (k = 0; k < 2; k++)          /* errorCode+runSrc 组合 */
            {
                set_kind(K_MENU3ENTER, tuneIndex, 0, 0, 0, 0);
                g_in.menuLevel = MENU_LEVEL_3;
                g_in.menuMode = MENU_MODE_USER;
                g_in.menuModeStatus = MENU_MODE_NONE;
                g_in.curFc.index = tuneIndex;
                g_in.curFc.group = FC_GROUP_FACTORY;
                g_in.curFc.grade = 0;
                g_in.u16[I_MENU3NUMBER] = tcmd[i];
                g_in.motorSn = MOTOR_SN_1;
                g_in.motorFc.motorPara.elem.motorType =
                    (Uint16)(j ? MOTOR_TYPE_ACI_GENERAL : MOTOR_TYPE_PMSM);
                g_in.errorCode = k ? ERROR_NONE : 1U;
                g_in.runSrc = k ? FUNCCODE_runSrc_PANEL : 0U;
                g_in.factoryPwdStatus = FMENU_FACTORY_PWD_LOCK;
                g_in.runFlag.all = 0U;
                g_in.rwMode = FUNCCODE_RW_MODE_NO_OPERATION;
                run_case();
            }
        }
    }
}

/* Menu3OnUpDown 的两条"换组 / 换档"支路 —— 由负向对照 M098/M100/M101 逼出来
 * 的一相。
 *
 * 这两条支路原来一次都没被走到, 原因与 Menu3OnEnter 那次同类, 但更隐蔽:
 *
 *   1. **分派表少了一格。** run_case 里 K_MENU3UPDN 走的是 `g_menuupdown[side][3]`,
 *      而那张表第 3 格装的是 `Menu3OnUp` —— 它在基准里就是 `Menu3OnUpDown(ON_UP_KEY)`
 *      的一层薄包装 (第 402~405 行), 所以调用**没有丢**, 但 flag 恒为 ON_UP_KEY,
 *      phase_menu_handlers 想通过 a1 传 ON_DOWN_KEY 是传不进去的。方向由 K_MENU3DOWN
 *      (`g_menudown[side][2]` = Menu3OnDown) 覆盖, 这里不重复。
 *   2. **curFc.group 从来没取到那两条支路要的值。** phase_menu_handlers 喂的是
 *      `curFc.group = 1 + mm*3` ∈ {1,4,7,10}, 而
 *        - 第 946 行要 `group >= FUNCCODE_GROUP_U0` (= 67)  —— 厂家组支路 (M098)
 *        - 第 954 行要 `group == USER_MENU_GROUP` (= FUNCCODE_GROUP_FE = 14)
 *          且 `menuAttri[MENU_LEVEL_3].operateDigit == 2` —— 用户组支路 (M100/M101)
 *      两个都碰不到。group = 14 < 67, 所以 946 为假、954 为真, 这两条是**互斥**的,
 *      必须分两组喂。
 *
 * 用户组支路里 `menu3Number / 100` 是 LimitOverTurnDeal 的**起点组号**, 它沿
 * funcCodeGrade[] 走, 跳过计数为 0 的组。所以 M100 (`funcCodeGrade[FC_GROUP_FACTORY]`
 * 清 0 改 1, FC_GROUP_FACTORY = FF = 15) 与 M101 (`funcCodeGrade[FUNCCODE_GROUP_CF]`
 * 取 motorDebugFc.fc 改 0, CF = 64) 是否可见, 完全取决于起点组号扫没扫到那两个落点 ——
 * 于是起点组号扫遍 0..FUNCCODE_GROUP_NUM, 一个不漏。
 *
 * 厂家组支路 (946) 先调 Menu2OnUpDown, 它按 menuMode 分三条: MENU_MODE_USER 会进
 * DealUserMenuModeGroupGrade, 而 group >= 67 时 funcCodeGradeCurMenuMode[67] 是厂家
 * 组计数、其后续索引会越界 (文件头既有缺陷 9)。这里取 **MENU_MODE_CHECK**, 它只写
 * checkMenuModeCmd / checkMenuModePara 两个标量就返回, 安全且不影响 949 行。 */
static void phase_menu3_updown_grade(void)
{
    unsigned g;

    /* ---- 用户组支路 (基准 954~975): GroupUpDown 的落点扫描 ---- */
    for (g = 0; g <= FUNCCODE_GROUP_NUM; g++)
    {
        set_kind(K_MENU3UPDN, 0, ON_UP_KEY, 0, 0, 0);
        g_in.menuLevel = MENU_LEVEL_3;
        g_in.menuMode = MENU_MODE_USER;
        g_in.menuModeStatus = MENU_MODE_NONE;
        g_in.curFc.index = (Uint16)(20 + (g % 7));
        g_in.curFc.group = USER_MENU_GROUP;
        g_in.curFc.grade = (Uint16)(g % 9);
        g_in.menuAttri[MENU_LEVEL_3].operateDigit = 2;
        g_in.u16[I_MENU3NUMBER] = (Uint16)(g * 100);
        g_in.factoryPwdStatus = FMENU_FACTORY_PWD_UNLOCK;
        run_case();
    }

    /* ---- 厂家/用户组下移支路 (基准 946~951): GetGradeIndex 的两个实参 ---- */
    for (g = 0; g < 12; g++)
    {
        set_kind(K_MENU3UPDN, 0, ON_UP_KEY, 0, 0, 0);
        g_in.menuLevel = MENU_LEVEL_3;
        g_in.menuMode = MENU_MODE_CHECK;
        g_in.menuModeStatus = MENU_MODE_NONE;
        g_in.curFc.index = (Uint16)(20 + (g % 7));
        g_in.curFc.group = (Uint16)(FUNCCODE_GROUP_U0 + (g % 6));
        g_in.curFc.grade = (Uint16)(g % 9);
        g_in.menuAttri[MENU_LEVEL_3].operateDigit = (Uint16)(g % 5);
        g_in.u16[I_MENU3NUMBER] = (Uint16)(g * 100);
        g_in.factoryPwdStatus = FMENU_FACTORY_PWD_UNLOCK;
        run_case();
    }
}

static void phase_pwd(void)
{
    int i, f;

    /* MenuPwdOnPrg: groupHidePwdStatus 两态 */
    for (i = 0; i < 2; i++)
    {
        set_kind(K_PWDONPRG, (Uint16)i, 0, 0, 0, 0);
        g_in.u16[I_GROUPHIDEPWDSTATUS] = (Uint16)i;
        run_case();
    }

    /* MenuPwdHint2Input */
    set_kind(K_PWDHINT2INPUT, 0, 0, 0, 0, 0);
    run_case();

    /* MenuPwdHintOnShift: DEBUG_RANDOM_FACPASS = 0 -> 走 MenuPwdHint2Input */
    set_kind(K_PWDHINTONSHIFT, 0, 0, 0, 0, 0);
    run_case();

    /* MenuPwdHintOnQuick 是空函数 */
    set_kind(K_PWDHINTONQUICK, 0, 0, 0, 0, 0);
    run_case();

    /* MenuPwdInputOnEnter: menuPwdNumber 对/错 x userPassword 两态 */
    for (i = 0; i < 4; i++)
    {
        static const Uint16 pwd[4] = {0, 1234, 65535, 9999};
        set_kind(K_PWDINPUTENTER, pwd[i], 0, 0, 0, 0);
        g_in.u16[I_MENUPWDNUMBER] = pwd[i];
        g_in.fc.code.userPassword = (Uint16)((i & 1) ? 1234 : 5678);
        run_case();
    }

    /* MenuPwdInputOnUp / OnDown / UpDown / Shift */
    for (i = 0; i < 6; i++)
    {
        int od;
        for (od = 0; od < 5; od++)
        {
            set_kind(K_PWDINPUTUP, (Uint16)(i * 10 + od), 0, 0, 0, 0);
            g_in.u16[I_MENUPWDNUMBER] = (Uint16)(i * 11111);
            g_in.menuAttri[MENU_LEVEL_PWD_INPUT].operateDigit = (Uint16)od;
            run_case();

            set_kind(K_PWDINPUTDOWN, (Uint16)(i * 10 + od), 0, 0, 0, 0);
            g_in.u16[I_MENUPWDNUMBER] = (Uint16)(i * 11111);
            g_in.menuAttri[MENU_LEVEL_PWD_INPUT].operateDigit = (Uint16)od;
            run_case();

            set_kind(K_PWDINPUTUPDN, (Uint16)(i * 10 + od), (Uint16)od, 0, 0, 0);
            g_in.u16[I_MENUPWDNUMBER] = (Uint16)(i * 11111);
            g_in.menuAttri[MENU_LEVEL_PWD_INPUT].operateDigit = (Uint16)od;
            g_in.arg0 = (Uint16)(od & 1);
            run_case();

            set_kind(K_PWDINPUTSHIFT, (Uint16)(i * 10 + od), 0, 0, 0, 0);
            g_in.menuAttri[MENU_LEVEL_PWD_INPUT].operateDigit = (Uint16)od;
            run_case();
        }
    }

    /* MenuPwdInputOnQuick 是空函数 */
    set_kind(K_PWDINPUTQUICK, 0, 0, 0, 0, 0);
    run_case();

    /* ValidateTuneCmd: motorSn x motorType x tuneCmd 值 x errorCode x runSrc */
    for (i = 0; i < 4; i++)
    {
        for (f = 0; f < 6; f++)
        {
            static const Uint16 tv[6] =
            {
                0, FUNCCODE_tuneCmd_PMSM_11, FUNCCODE_tuneCmd_PMSM_12,
                FUNCCODE_tuneCmd_ACI_STATIC, FUNCCODE_tuneCmd_ACI_WHOLE, 9
            };
            set_kind(K_VALIDTUNE, tv[f], (Uint16)i, 0, 0, 0);
            g_in.arg0 = tv[f];
            g_in.arg1 = (Uint16)(MOTOR_SN_1 + (i % 4));
            g_in.motorSn = (enum MOTOR_SN)(MOTOR_SN_1 + (i % 4));
            g_in.motorFc.motorPara.elem.motorType =
                (Uint16)((i & 1) ? MOTOR_TYPE_PMSM : (MOTOR_TYPE_PMSM + 1));
            g_in.errorCode = (Uint16)((i & 2) ? ERROR_NONE : ERROR_UV);
            g_in.runSrc = (Uint16)((f & 1) ? FUNCCODE_runSrc_PANEL
                                           : FUNCCODE_runSrc_PANEL + 1);
            run_case();
        }
    }
}

/* 随机入参的**域约束**。
 *
 * 这些 kind 把实参当下标 / 表长 / 步进码用, 越界在宿主上会 SIGSEGV 或挂死 ——
 * 而且是 ref 侧先崩, 一次对比都走不到。所以随机阶段必须把实参夹回函数契约
 * 允许的范围。夹的是**域**, 域内仍然全随机。
 *
 * 这不是"把测试削弱到只喂合法值": 真机上没有任何调用点会传这种值, 越界输入
 * 属于**非法输入**, 真机上的行为 (读相邻全局 / 死循环) 本来就不在验收范围内。
 * 逐条理由见 README「入参域」。
 */
static void clamp_random_args(Uint16 k)
{
    switch (k)
    {
    case K_GETDIGITS:
    case K_MODUPDOWN:
        /* 直接索引 funcCodeAttribute[] (909 项)。 */
        g_in.arg0 = (Uint16)rnd_n(FMENU_ATTR_N);
        break;

    case K_MODENTER:
        /* index 索引属性表; dataNew 会被原样写进 funcCode, 必须落在该功能码的
         * 合法值域内 (否则后续有代码把它当表下标用)。 */
        {
            Uint16 lo, hi;
            g_in.arg0 = (Uint16)rnd_n(FMENU_ATTR_N);
            fc_value_range((int)g_in.arg0, &lo, &hi);
            g_in.arg1 = rnd_in(lo, hi);
        }
        break;

    case K_NOSAME:
        /* 随机阶段喂的表是 ref_diFuncIndex (18 项) -> number ∈ [0, 18]。
         * upper/lower 取有序对且 <= 32767: 终止性前提, 理由见 phase_basics
         * 里 NoSameDeal 那段的注释 (upper < lower 会死循环)。 */
        g_in.sarg = (int16)rnd_n(19);
        g_in.arg2 = (Uint16)rnd_n(32768);
        g_in.arg3 = (Uint16)rnd_n((Uint16)(g_in.arg2 + 1));
        break;

    case K_LIMITOVTURN:
        /* 随机阶段喂的表是 funcCodeGradeCurMenuMode (GRADE_N 项):
         *   arg1 = upper ∈ [1, GRADE_N], arg2 = low ∈ [0, upper - 1]
         *   arg3 = flag -> deltaK[flag], deltaK 只有 3 项。 */
        g_in.arg1 = (Uint16)(1 + rnd_n((Uint16)GRADE_N));
        g_in.arg2 = (Uint16)rnd_n(g_in.arg1);
        g_in.arg3 = (Uint16)rnd_n(3);
        break;

    case K_GROUPUPDOWN:
        /* 内部转发给 LimitOverTurnDeal(..., flag) -> deltaK[flag]。 */
        g_in.arg1 = (Uint16)rnd_n(3);
        break;

    case K_DEALUSERGRADE:
        /* DealUserMenuModeGroupGrade(flag) -> LimitOverTurnDeal(..., flag)
         * -> deltaK[flag] (3 项)。userMenuModeFcIndex 由 phase_random 取
         * [0, FENUM), 本身就是合法起点, 不用再夹。 */
        g_in.arg0 = (Uint16)rnd_n(3);
        break;

    case K_DEALCHECKGRADE:
        /* DealCheckMenuModeGroupGrade(flag) 里 flag 被用两次:
         *   (a) LimitOverTurnDeal(..., flag) -> deltaK[flag]  (3 项);
         *   (b) ON_DOWN_KEY == flag / ON_UP_KEY == flag 的分支。
         * 真机 flag 只有一个来源: Menu2OnUpDown(ON_UP_KEY|ON_DOWN_KEY),
         * 即 {ON_UP_KEY=1, ON_DOWN_KEY=2} —— 所以这里也只取 1/2。
         * flag = 0 会取到 deltaK[0] = 0, LimitOverTurnDeal 原地空转 66 次后
         * 原样返回起始组, 语义上等于"没按过键", 真机不存在这种调用。
         *
         * curFc 的约束不在这里做 —— phase_random 已经把 curFc 摆成合法的
         * (group, grade) 对 (见那里的注释), 本函数从 curFc 取起点即可。 */
        g_in.arg0 = (Uint16)(1 + rnd_n(2));
        break;

    default:
        break;
    }
}

static void phase_random(long n)
{
    long c;

    for (c = 0; c < n; c++)
    {
        Uint16 k = (Uint16)(rnd_n((Uint16)K_N));

        g_in.arg0 = rnd();
        g_in.arg1 = rnd();
        g_in.arg2 = rnd();
        g_in.arg3 = rnd();
        g_in.arg4 = rnd();
        g_in.sarg = (int16)rnd();
        g_in.larg = ((Uint32)rnd() << 16) | rnd();

        /* 共享状态也随机化, 但避开会越界/死循环的组合。 */
        g_in.errorCode = (Uint16)(rnd_n(2) ? ERROR_UV : ERROR_NONE);
        g_in.tuneCmd = (Uint16)rnd_n(2);
        g_in.runFlag.all = (Uint16)rnd_n(2);
        g_in.menuMode = (enum MENU_MODE)rnd_n(4);
        g_in.menuModeTmp = (enum MENU_MODE)rnd_n(4);
        g_in.menuModeOld = (enum MENU_MODE)rnd_n(4);
        g_in.menuLevel = (enum MENU_LEVEL)rnd_n(MENU_LEVEL_NUM);
        g_in.menu0DispStatus = (enum MENU0_DISP_STATUS)rnd_n(4);
        g_in.menuModeStatus = (enum MENU_MODE_OPERATE)rnd_n(2);
        g_in.checkMenuModeDealStatus = (enum CHECK_MENU_MODE_DEAL)rnd_n(4);
        g_in.factoryPwdStatus = (enum FMENU_FACTORY_PWD_STATUS)rnd_n(2);
        g_in.facPassViewStatus = (enum FAC_PASS_RANDOM_VIEW_OPERATE)rnd_n(2);
        g_in.userMenuModeFcIndex = (int16)rnd_n(FENUM);
        g_in.curFc.index = (Uint16)rnd_n(FMENU_ATTR_N);
        /* curFc 是"当前功能码游标", 契约上**永远**是一个真实存在的
         * (group, grade) 对。理由: 下游好几个函数直接
         *     curFc.index = GetGradeIndex(curFc.group, curFc.grade);
         *     ... funcCode.all[curFc.index] ...
         * 而 GetGradeIndex 只做 funcCodeGradeSum[group] + grade, 不做任何
         * 边界检查 (Menu2OnEnter 第 662 行、Menu3OnUpDown 第 949 行)。
         *   - 用户组 (>= FUNCCODE_GROUP_U0 = 67) 的 funcCodeGradeSum 已经
         *     >= FNUM_PARA (= 909), 一进去就把 funcCode.all[] 顶出表外;
         *   - CF(64) 与 U0(67) 之间的 65/66 是空号, 计数为 0;
         *   - 即便在 [0, 64] 里, 31 号组的计数也是 0。
         * 所以从"计数 > 0 的厂家组 x 该组 grade 数"里取。用户组那几条路径由
         * phase_menu_handlers / phase_display_menu123 定向覆盖 (它们自己会把
         * curFc.group 摆到 FUNCCODE_GROUP_U0 并把 grade 夹进 U0NUM)。 */
        {
            Uint16 g;

            do
            {
                g = (Uint16)rnd_n((Uint16)(FUNCCODE_GROUP_CF + 1));
            }
            while (!funcCodeGradeAll[g]);

            g_in.curFc.group = g;
            g_in.curFc.grade = (Uint16)rnd_n(funcCodeGradeAll[g]);
        }
        g_in.u16[I_MENU3NUMBER] = rnd();
        g_in.u16[I_MENUPWDNUMBER] = rnd();
        g_in.u16[I_CURFCDISPDIGITS] = (Uint16)(1 + rnd_n(5));
        g_in.u16[I_TICKER4LOWERDISP] = (Uint16)rnd_n(8);
        g_in.fc.code.frqPoint = (Uint16)rnd_n(5);
        g_in.fc.code.speedDispPointPos = (Uint16)rnd_n(5);
        g_in.fc.code.menuMode = rnd();
        g_in.fc.code.userPassword = rnd();
        g_in.fc.code.userPasswordReadOnly = (Uint16)rnd_n(2);
        g_in.fc.code.motorDebugFc = rnd();
        g_in.fc.code.funcParaView = rnd();
        g_in.fc.code.aiaoCalibrateDisp = (Uint16)rnd_n(2);
        g_in.fc.code.mfkKeyFunc = (Uint16)rnd_n(4);
        g_in.fc.code.runSrc = (Uint16)rnd_n(2);
        g_in.fc.code.pidDisp = rnd();
        g_in.fc.code.speedDispCoeff = rnd();
        g_in.fc.code.dispParaStopBit = (Uint16)rnd_n(STOP_DISPLAY_NUM);
        g_in.fc.code.dispParaRunBit = (Uint16)rnd_n(RUN_DISPLAY_NUM);
        /* cycleShiftDeal 的死循环防护: para 在 [0, max) 里至少有一位。
         * 三个字段都是 Uint16: stop 模式 para = ledDispParaStop (max=16),
         * run 模式 para = Run1 | (Run2 << 16) (max=32)。
         * stop 掩码保证非 0; run 模式下 Run1 保证非 0, 低位就有位, 循环必终止。 */
        g_in.fc.code.ledDispParaStop = (Uint16)(1 + rnd_n(0xFFFF));
        g_in.fc.code.ledDispParaRun1 = (Uint16)(1 + rnd_n(0xFFFF));
        g_in.fc.code.ledDispParaRun2 = (Uint16)rnd();
        g_in.menuAttri[MENU_LEVEL_2].operateDigit = (Uint16)rnd_n(4);
        g_in.menuAttri[MENU_LEVEL_3].operateDigit = (Uint16)rnd_n(5);
        g_in.menuAttri[MENU_LEVEL_PWD_INPUT].operateDigit = (Uint16)rnd_n(5);
        g_in.runSrc = (Uint16)rnd_n(2);
        g_in.runMode = (Uint16)rnd_n(3);
        g_in.maxFrq = (Uint16)(1 + rnd_n(60000));
        g_in.frqDroop = (int32)rnd() - 32768;
        g_in.frq = (int32)rnd() - 32768;
        g_in.frqAim = (int32)rnd() - 32768;
        g_in.frqRun = (int32)rnd() - 32768;
        g_in.pidFunc.ref = (int32)rnd() - 32768;
        g_in.pidFunc.fdb = (int32)rnd() - 32768;
        g_in.pulseInFrq = ((Uint32)rnd() << 8) | rnd_n(256);
        g_in.torqueCurrent = rnd();
        g_in.currentPu = rnd();
        g_in.invPara.type = rnd();
        g_in.invPara.pointLimit = rnd();
        g_in.rwMode = (Uint16)rnd_n(4);
        g_in.validateInvTypeRet = (Uint16)rnd_n(2);

        if (k >= (Uint16)K_N)
        {
            k = (Uint16)(K_N - 1);
        }

        /* MenuOnQuick 的越界防护 (见文件头缺陷 2)。 */
        if (k == K_MENUQUICK && g_in.menuModeTmp == MENU_MODE_NULL)
        {
            g_in.menuModeTmp = MENU_MODE_BASE;
        }

        /* 把"当下标用"的实参夹回契约域 (见 clamp_random_args 的注释)。
         * 必须放在 set_kind 之前 —— set_kind 会把 g_in.arg* 再抄一遍。 */
        clamp_random_args(k);

        /* 需要具体表的 kind 用真表。 */
        g_in.tbl = (k == K_NOSAME) ? ref_diFuncIndex
                 : (k == K_GROUPUPDOWN || k == K_LIMITOVTURN)
                     ? funcCodeGradeCurMenuMode
                     : funcCodeGradeCurMenuMode;

        /* 入参在循环开头已逐项随机化。这里把**同一批值**交给 set_kind: 它顺手把
         * 诊断用的 g_arg0/g_arg1 对齐到真正跑的那两个数。不再另抽随机数, 否则
         * 诊断打印的实参和实际入参会不一致。 */
        set_kind((enum FMENU_KIND)k, g_in.arg0, g_in.arg1, g_in.arg2,
                 g_in.arg3, g_in.arg4);
        run_case();
    }
}

int main(int argc, char **argv)
{
    long nrand = 200000;
    long base;

    /* stdout 关掉缓冲。理由: 测试台崩 (SIGSEGV) 时, 块缓冲里的内容会连同
     * 进程一起丢掉, 只剩一个空的日志, 完全没法定位。逐条 flush 之后,
     * 崩溃点前面最后打印的那一行就是现场。 */
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc > 1)
    {
        nrand = atol(argv[1]);
    }

    init_kind_names();
    init_ptrs();
    init_fn_ids();
    check_invariants();

    base_state();
    base = g_cases;
    phase_basics();
    printf("  基础工具 (FcDigit2Bin/LimitDeal/NoSameDeal/OverTurn/"
           "LimitOverTurn/GetDispDigits) : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_no_same_wrap();
    printf("  NoSameDeal 回卷 (落点恰在界上)  : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_modify_updown();
    printf("  ModifyFunccodeUpDown           : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_modify_enter();
    printf("  ModifyFunccodeEnter            : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_display_attr();
    printf("  UpdateDisplayBufferAttribute   : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_display_group();
    printf("  UpdateGroupDisplay             : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_display_menu0();
    printf("  UpdateMenu0*                   : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_display_menu123();
    printf("  UpdateMenu1/2/3DisplayBuffer   : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_display_misc();
    printf("  其它显示 (Err/Tune/Pwd/Visual/U0) : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_disp_data();
    printf("  DispDataDeal                   : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_cycle_shift();
    printf("  cycleShiftDeal                 : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_menu_grade();
    printf("  分级/查找 (Grade/GroupUpDown/Deal*) : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_menu_state();
    printf("  MenuInit/ModeDeal/ModeSwitch   : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_menu_handlers();
    printf("  菜单按键 handler               : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_menu3_pwd_tune();
    printf("  Menu3OnEnter 口令/调参支路     : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_menu3_updown_grade();
    printf("  Menu3OnUpDown 换组/换档支路    : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_pwd();
    printf("  口令 / ValidateTuneCmd         : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_random(nrand);
    printf("  随机混合                       : %ld\n", g_cases - base);

    printf("f_menu difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    return g_fails ? 1 : 0;
}
