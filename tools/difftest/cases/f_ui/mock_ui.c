/* f_ui 差分测试的共享 mock。

 * 这个模块自己拥有一大批状态 (displayBuffer / spiData / 键盘状态机 / 闪烁计数器
 * 等等), 那些都带 ref_ / new_ 前缀, 两侧各自独立, 由 driver 灌初值与快照。
 * 这里只定义**两侧必须看到同一份**的东西:

 *   1. 三个外设寄存器块 (SpiaRegs / GpioCtrlRegs / GpioDataRegs)。模块读它们、
 *      写它们, 从不分配它们, 所以由 driver 扮演芯片: 每次调用前装一份完整映像,
 *      调用后整块快照。结构体来自真 device 头 (见 real.txt), 位域布局就是芯片的。
 *   2. 模块引用但不拥有的 21 个全局量 (funcCode / menuAttri / menu / menuLevel /
 *      runFlag / runSrc / errorCode ... )。其中 menuAttri 是**被写的** ——
 *      UpdateDisplayBuffer 与 WinkDeal 都会改 menuAttri[menuLevel].winkFlag(Led)。
 *      正因为它被写, driver 才必须在每一侧调用**之前**把它装回同一份输入映像,
 *      否则第二侧看到的是第一侧写过的值。
 *   3. 被调函数的替身与它们的**序列**日志。

 * 一个刻意的设计: menu[] 的 6 层各指一套**不同**的替身函数, 而不是全指同一套。
 * 这样 `menu[menuLevel].onUpFunc()` 被误写成 `menu[0].onUpFunc()` 是可观测的 ——
 * 若 6 层共用一套替身, 这种错法在调用计数与顺序上完全看不出来。
 */
#include "DSP28x_Project.h"
#include "mock_ui.h"

/* ---- 外设 ---------------------------------------------------------------- */

volatile struct SPI_REGS        SpiaRegs;
volatile struct GPIO_CTRL_REGS  GpioCtrlRegs;
volatile struct GPIO_DATA_REGS  GpioDataRegs;

volatile Uint16 gHostCpuCtl[7];

/* ---- 共享全局量 ---------------------------------------------------------- */

FUNCCODE_ALL             funcCode;
struct MENU_ATTRIBUTE    menuAttri[MENU_LEVEL_NUM];
enum MENU_LEVEL          menuLevel;
enum MENU_MODE           menuMode;
enum MENU_MODE           menuModeTmp;
enum MENU_MODE_OPERATE   menuModeStatus;

union RUN_FLAG runFlag;
Uint16 runMode;
Uint16 runSrc;
Uint16 tuneCmd;

Uint16 errorCode;
Uint16 errorCodeOld;
Uint16 errAutoRstNum;

Uint16 accDecFrqPrcFlag;
Uint16 bFrqDigital;

Uint16 checkMenuModeCmd;
Uint16 checkMenuModePara;

/* ---- 调用日志 ------------------------------------------------------------ */

struct UI_LOG_ENT gUiLog[UI_LOG_LEN];
Uint16            gUiLogN;
Uint16            gUiLogOverflow;

static void ui_log_put(Uint16 op, Uint16 level, Uint16 arg)
{
    if (gUiLogN < UI_LOG_LEN)
    {
        gUiLog[gUiLogN].op    = op;
        gUiLog[gUiLogN].level = level;
        gUiLog[gUiLogN].arg   = arg;
        gUiLogN++;
    }
    else
    {
        gUiLogOverflow++;
    }
}

/* ---- menu[] 的 6 x 10 个替身 --------------------------------------------
 *
 * 每层一套, 层号写死在函数体里。名字里的 L 就是菜单层, 与 menu[] 的下标一致。
 * 这样日志里的 (op, level) 同时记录"调了哪个操作"和"从哪一层调的"。
 */
#define UI_STUB_SET(L)                                                        \
    static void s##L##_prg  (void) { ui_log_put(UI_OP_PRG,   L, 0); }         \
    static void s##L##_up   (void) { ui_log_put(UI_OP_UP,    L, 0); }         \
    static void s##L##_enter(void) { ui_log_put(UI_OP_ENTER, L, 0); }         \
    static void s##L##_mfk  (void) { ui_log_put(UI_OP_MFK,   L, 0); }         \
    static void s##L##_down (void) { ui_log_put(UI_OP_DOWN,  L, 0); }         \
    static void s##L##_shift(void) { ui_log_put(UI_OP_SHIFT, L, 0); }         \
    static void s##L##_run  (void) { ui_log_put(UI_OP_RUN,   L, 0); }         \
    static void s##L##_stop (void) { ui_log_put(UI_OP_STOP,  L, 0); }         \
    static void s##L##_quick(void) { ui_log_put(UI_OP_QUICK, L, 0); }         \
    static void s##L##_disp (void) { ui_log_put(UI_OP_DISP,  L, 0); }

UI_STUB_SET(0)
UI_STUB_SET(1)
UI_STUB_SET(2)
UI_STUB_SET(3)
UI_STUB_SET(4)
UI_STUB_SET(5)

#define UI_ROW(L)                                                             \
    { s##L##_prg, s##L##_up, s##L##_enter, s##L##_mfk, s##L##_down,           \
      s##L##_shift, s##L##_run, s##L##_stop, s##L##_quick, s##L##_disp }

const sysMenu menu[MENU_LEVEL_NUM] =
{
    UI_ROW(0),
    UI_ROW(1),
    UI_ROW(2),
    UI_ROW(3),
    UI_ROW(4),
    UI_ROW(5)
};

/* ---- 另外三个外部函数 ----------------------------------------------------
 *
 * DispDataDeal / Menu1OnPrg 由 UpdateDisplayBuffer 调用;
 * DealCheckMenuModeGroupGrade(checkMenuModePara) 的**实参**要记下来 ——
 * "传错参数"这种错法只有把实参也记进日志才看得见。
 */
void DispDataDeal(void)
{
    ui_log_put(UI_OP_DISP_DATA_DEAL, 0, 0);
}

void Menu1OnPrg(void)
{
    ui_log_put(UI_OP_MENU1_ON_PRG, 0, 0);
}

void DealCheckMenuModeGroupGrade(Uint16 flag)
{
    ui_log_put(UI_OP_DEAL_CHECK_GROUP_GRADE, 0, flag);
}
