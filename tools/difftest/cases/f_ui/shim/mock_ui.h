/* f_ui 差分测试的共享状态声明。

 * 放在 shim/ 是因为 shim/ 是唯一排在 include 路径上的"用例私有头文件"位置
 * (用例目录本身不在路径上, 只有 case/ref 与交付源码目录在)。它**不做任何头文件
 * 替换**, 只是让 mock_ui.c 与 driver.c 共用同一份声明, 免得两边各抄一份然后漂移。

 * include 链与 src/function/f_ui.c 一致, 于是这里用到的都是**真类型**。
 * 这对本模块是硬要求: 观测量里有一大半是 `struct MENU_ATTRIBUTE` /
 * `union RUN_FLAG` / `struct SPI_IN_OUT` 的**具体布局** (例如 RCK 是
 * `GpioDataRegs.GPADAT.bit.GPIO22`、KEYSEL 是 `GpioDataRegs.AIODAT.bit.AIO4`),
 * 替身类型会让测试失去意义。

 * 注意这个头文件会被 build_variant 跟着 shim/ 一起复制到负向对照的变体目录 ——
 * 漏掉它的后果和漏掉 real.txt 一样: 变体编不过, 被误记成 BROKEN。
 */
#ifndef MOCK_UI_H
#define MOCK_UI_H

#include "f_ui.h"
#include "f_menu.h"
#include "f_frqSrc.h"
#include "f_comm.h"
#include "f_runSrc.h"
#include "f_io.h"
#include "f_error.h"

/* ---- 模块私有常量 (头文件里没有) ------------------------------------------
 *
 * f_ui.c 自己定义的那一批。driver 要用它们来算边界, 而不是把数字硬编码一遍 ——
 * 硬编码的守卫一旦和源文件脱节就会静默失效。
 * 下面每一个都逐字对应 src/function/f_ui.c 的同名定义。
 */
#define FUI_KEY_AGE_MAX         20
#define FUI_MAX_RATE            100
#define FUI_MIN_RATE_PAUSE      100
#define FUI_FLASH_TICKER_MAX    42
#define FUI_FLASH_TICKER_LED_MAX 45
#define FUI_KEY_SEL_TEST_TIME   750
#define FUI_MIN_RATE_N          5
#define FUI_REPEAT_NUM_MAX_N    5

/* `continueKeyFlag = ((1 << KEY_UP) | (1 << KEY_DOWN))`, `ctrlKeyFlag = 0`。
 * 两者都是模块自己的宏, driver 要用它们构造"落在回差两侧"的键值。 */
#define FUI_CONTINUE_KEY_FLAG   ((1U << KEY_UP) | (1U << KEY_DOWN))
#define FUI_CTRL_KEY_FLAG       0

/* ---- 模块引用的 24 个外部符号: 全部由 mock_ui.c 定义, 两侧**共用** --------
 *
 * 清单来自 `nm2000 -u build/Debug/f_ui.obj`, 不是人眼数的。
 */

extern volatile Uint16 gHostCpuCtl[7];

extern FUNCCODE_ALL              funcCode;
extern struct MENU_ATTRIBUTE     menuAttri[MENU_LEVEL_NUM];
extern const sysMenu             menu[MENU_LEVEL_NUM];
extern enum MENU_LEVEL           menuLevel;
extern enum MENU_MODE            menuMode;
extern enum MENU_MODE            menuModeTmp;
extern enum MENU_MODE_OPERATE    menuModeStatus;

extern union RUN_FLAG runFlag;
extern Uint16 runMode;
extern Uint16 runSrc;
extern Uint16 tuneCmd;

extern Uint16 errorCode;
extern Uint16 errorCodeOld;
extern Uint16 errAutoRstNum;

extern Uint16 accDecFrqPrcFlag;
extern Uint16 bFrqDigital;

extern Uint16 checkMenuModeCmd;
extern Uint16 checkMenuModePara;

/* ---- 被调函数的替身: 模块的**输入**而不是被测逻辑 -------------------------
 *
 * f_ui 调出去的三个外部函数, 以及 menu[] 里的 10 个函数指针 (每个菜单层一套,
 * 一共 6 x 10 = 60 个)。它们全部逐字记进一张**序列**日志 —— 用序列而不是计数,
 * 是因为 `menu[0].onUpFunc()` 写成 `menu[menuLevel].onUpFunc()` 这种错法,
 * 计数完全一样, 只有"哪一层、按什么顺序"能分开。
 *
 * 让 6 层各指一套不同的替身 (而不是全指同一套) 是刻意的: 索引写错的 bug 类别
 * 只有这样才可观测。
 */
#define UI_LOG_LEN  64

enum UI_OP
{
    UI_OP_PRG = 0,
    UI_OP_UP,
    UI_OP_ENTER,
    UI_OP_MFK,
    UI_OP_DOWN,
    UI_OP_SHIFT,
    UI_OP_RUN,
    UI_OP_STOP,
    UI_OP_QUICK,
    UI_OP_DISP,
    UI_OP_DISP_DATA_DEAL,
    UI_OP_MENU1_ON_PRG,
    UI_OP_DEAL_CHECK_GROUP_GRADE
};

struct UI_LOG_ENT
{
    Uint16 op;
    Uint16 level;
    Uint16 arg;
};

extern struct UI_LOG_ENT gUiLog[UI_LOG_LEN];
extern Uint16            gUiLogN;
extern Uint16            gUiLogOverflow;

#endif
