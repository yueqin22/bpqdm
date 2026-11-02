/* f_frqSrc 差分测试的共享 mock。
 *
 * 一份实例, ref 与 new 两侧都读它, 所以两侧看到的是**完全相同**的输入。
 * 被测模块自己导出的 67 个外部符号由 ref_tu.c / new_tu.c 各自复制一份 ——
 * 那才是比对的对象。
 *
 * include 顺序与 src/function/f_frqSrc.c 一致。
 *
 * 类型用**真类型**, 不是替身。这张清单不是"猜"出来的: 把交付源码的未定义符号逐个
 * 拿去 include/ 里查 extern 声明, 再让链接器把漏掉的一个个报出来。每一条都标出定义
 * 它的头文件, 便于以后核对。
 *
 * 用真类型的理由: 本模块的运算建立在"这个字段是 16 位"的前提上 ——
 *   * (int32)(int16)funcCode.code.frqComm 的符号扩展;
 *   * ((int32)aiDeal[i].set * maxFrq + (1 << 14)) >> 15 的定点标定;
 *   * ABS_INT16(...) 的截断。
 * 换成替身类型会移动截断点, 测试就变成在验证另一份代码。
 */
#include "f_frqSrc.h"
#include "f_main.h"
#include "f_runSrc.h"
#include "f_io.h"
#include "f_menu.h"
#include "f_ui.h"
#include "f_p2p.h"
#include "f_comm.h"

#include "mock_frqSrc.h"

/* ---------------------------------------------------------------------------
 * 模块读到的全部外部数据
 * ------------------------------------------------------------------------ */

FUNCCODE_ALL funcCode;                    /* f_funcCode.h  code.frqCalcSrc / frqXSrc /
                                                           frqYSrc / frqXySrc / presetFrq /
                                                           upperFrq / upperFrqSrc / ... */
struct MOTOR_FC motorFc;                  /* f_main.h      motorCtrlMode */
struct AI_DEAL aiDeal[AI_NUMBER];         /* f_io.h        .set, AI_NUMBER = 3 */
int16 pulseInSet;                         /* f_io.h        (f_frqSrc 不 include f_io.h? 它 include 了) */
struct DI_FUNC diFunc;                    /* f_io.h        f1.bit.* / f2.bit.* / f3.bit.* */
struct DI_FUNC diSelectFunc;              /* f_io.h        f2.bit.frqOk */
union RUN_FLAG runFlag;                   /* f_runSrc.h    bit.run / jog / common / dirReversing ... */
union RUN_CMD runCmd;                     /* f_runSrc.h    bit.dir / jog / pause / common */
Uint16 runDirPanelOld;                    /* f_runSrc.h    */
Uint16 accDecFrqPrcFlag;                  /* f_runSrc.h    **既是输入也是输出** */
Uint16 accDecTimeSrcPlc;                  /* f_runSrc.h    模块写它 */
enum RUN_STATUS runStatus;                /* f_runSrc.h    */
enum START_RUN_STATUS startRunStatus;     /* f_runSrc.h    */
enum SWING_STATUS swingStatus;            /* f_runSrc.h    SWING_NONE */
enum MENU0_DISP_STATUS menu0DispStatus;   /* f_menu.h      MENU0_DISP_STATUS_RUN_STOP */
Uint16 keyFunc;                           /* f_menu.h      KEY_UP */
const int16 decNumber[5] = {1, 10, 100, 1000, 10000};   /* f_menu.h */
Uint16 frqAimDisp;                        /* f_ui.h        模块写它 */
Uint16 frqXDisp;                          /* f_ui.h        模块写它 */
Uint16 frqYDisp;                          /* f_ui.h        模块写它 */
Uint16 frqAimPLCDisp;                     /* f_ui.h        模块写它 (DEBUG_F_PLC_CTRL) */
Uint16 runSrc;                            /* f_ui.h        */
int32 frqRun;                             /* f_frqSrc.h    */
int16 torqueAim;                          /* f_torqueCtrl.h  由 TorqueCalc 桩写 */
union ERROR_ATTRIBUTE errorAttribute;     /* f_error.h     bit.level */
Uint16 bFrqDigitalDone4WaitDelay;         /* f_frqSrc.h    模块写它 */
Uint16 dpFrqAim;                          /* f_comm.h      PROFIBUS 给定 */
P2P_DATA_STRUCT p2pData;                  /* f_p2p.h       processValue */
Uint16 CanRxTxCon;                        /* f_p2p.h       P2P_COMM_SLAVE */

/* ---------------------------------------------------------------------------
 * 三个外部函数桩的驱动值
 * ------------------------------------------------------------------------ */

int32  mockPidFrq;
Uint16 mockDigit[5];
int16  mockTorqueAim;

/* 调用轨迹。三个桩各记一条, 含实参。 */
struct FRQTRACE frqTrace[FRQTRACE_N];
Uint16 frqTraceN;

static void trace(Uint16 which, Uint16 arg)
{
    if (frqTraceN < FRQTRACE_N)
    {
        frqTrace[frqTraceN].which = which;
        frqTrace[frqTraceN].arg   = arg;
        frqTraceN++;
    }
}

/* ---------------------------------------------------------------------------
 * 三个桩
 *
 * 它们都**不在**被测模块里 (分别属于 f_frqSrc_pid.c / f_menu.c / f_torqueCtrl.c),
 * 所以给固定返回值 + 记轨迹就够了 —— 这里要测的是模块**怎么用**它们的结果,
 * 不是它们本身。
 *
 * 注意 f_torqueCtrl.c 已经被改写过 (见 docs/rewritten_modules.json), 但那是它自己的
 * 差分用例的事; 本用例把它当外部依赖, 用桩替掉, 免得两个模块的错误互相掩盖。
 * ------------------------------------------------------------------------ */

int32 FrqPidSetDeal(void)
{
    trace(FRQTRACE_PID, 0);
    return mockPidFrq;
}

void GetNumberDigit1(Uint16 digit[5], Uint16 number)
{
    int i;

    trace(FRQTRACE_DIGIT, number);

    for (i = 0; i < 5; i++)
    {
        digit[i] = mockDigit[i];
    }
}

void TorqueCalc(void)
{
    trace(FRQTRACE_TORQUE, 0);
    torqueAim = mockTorqueAim;
}
