/* f_runSrc 差分测试的共享 mock。
 *
 * 一份实例, ref 与 new 两侧都读它, 所以两侧看到的是**完全相同**的输入。
 * 被测模块自己导出的 45 个符号由 ref_tu.c / new_tu.c 各自复制一份 ——
 * 那才是比对的对象。
 *
 * include 顺序与 src/function/f_runSrc.c 一致。
 *
 * 类型用**真类型**, 不是替身。这张清单不是"猜"出来的: 把交付源码的未定义符号
 * (nm2000 -u) 逐个拿去 include/ 里查 extern 声明, 再让链接器把漏掉的一个个报出来。
 * 每一条都标出定义它的头文件, 便于以后核对。
 *
 * 用真类型的理由: 本模块的运算建立在若干宽度前提上 ——
 *   * (Uint32)motorFc.motorPara.elem.ratingFrq * 4 / 5 的定点标定;
 *   * (Uint32)invPara.ratingVoltage * 14482 >> 10 的标定;
 *   * funcCode.code.* 全是 16 位, 溢出点与截断点都在 16 位上;
 *   * diFunc.f1.all / f2.all 是 **Uint32** (DI_FUNC1_BITS 到 bit 27)。
 * 换成替身类型会移动截断点, 测试就变成在验证另一份代码。
 *
 * 分成两组:
 *   A. 只读的外部量 (21 个) —— 模块只读不写, driver 每次调用前按输入装好;
 *   B. 会被写的外部量 (26 个) —— 两侧写的是**同一个**对象, 所以必须按侧快照,
 *      由 driver 的 sh0 / SH 机制在每次调用前装回去。
 *      本文件只负责定义它们 (初值无所谓, driver 会覆盖)。
 */

#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_io.h"
#include "f_comm.h"
#include "f_ui.h"
#include "f_main.h"
#include "f_menu.h"
#include "f_eeprom.h"
#include "f_posCtrl.h"

#include "mock_runsrc.h"

/* ===========================================================================
 * A. 只读的外部量 (21 个)
 * ======================================================================== */

FUNCCODE_ALL funcCode;              /* f_funcCode.h  模块读 code.* 的 55 个字段,
                                                     写 code.plcCmd 与 4 个累计量。
                                                     所以 funcCode **两边都算**:
                                                     读的字段由 apply_inputs 装,
                                                     写的 5 个字段由 SH 按侧装。 */
struct MOTOR_FC motorFc;            /* f_main.h      motorCtrlMode / accDecTimeMotor /
                                                     motorPara.elem.{motorType,ratingFrq,ratingSpeed} */
struct INV_PARA invPara;            /* f_menu.h      ratingVoltage */
struct AI_DEAL aiDeal[AI_NUMBER];   /* f_io.h        .set (setTimeRun), [0].voltage (DormantDeal) */
struct DI_FUNC diFunc;              /* f_io.h        f1.bit.* / f2.bit.* */
struct DI_FUNC diSelectFunc;        /* f_io.h        f1.bit.accDecTimeSrc */
union ERROR_ATTRIBUTE errorAttribute;  /* f_error.h  bit.level */
Uint16 errorCode;                   /* f_error.h     */
Uint16 funcCodeRwMode;              /* f_eeprom.h    */
Uint16 generatrixVoltage;           /* f_interface.h PowerOffNoStopDeal */
Uint16 mainLoopTicker;              /* f_menu.h      UpdateRunCmd 的 lower-frq 分支 */
Uint16 bAntiReverseRun;             /* f_frqSrc.h    */
int32  frqAim;                      /* f_frqSrc.h    */
int32  frqAimTmp;                   /* f_frqSrc.h    */
int32  frqFdbTmp;                   /* f_interface.h StartRunCtrl 的 FVC 分支 */
int32  frqRun;                      /* f_interface.h */
Uint16 lowerFrq;                    /* f_frqSrc.h    */
Uint16 maxFrq;                      /* f_frqSrc.h    UpdateSwingPara */
Uint16 upperFrq;                    /* f_frqSrc.h    */
Uint16 runMode;                     /* f_frqSrc.h    */
enum MOTOR_SN motorSn;              /* f_main.h      */
enum POWER_ON_STATUS powerOnStatus; /* f_main.h      */

/* ===========================================================================
 * B. 会被写的外部量 (26 个)
 *
 * 定义在这里只是为了让链接器满意 —— 每次调用前的值由 driver 按侧装。
 * ======================================================================== */

int32  accTimeOld;                  /* f_runSrc.h    (真身: f_runSrc_accDecFrq.c) */
int32  decTimeOld;                  /* f_runSrc.h    (真身: f_runSrc_accDecFrq.c) */
int32  frqCurAimOld;                /* f_runSrc.h    (真身: f_runSrc_accDecFrq.c) */
Uint16 bFrqCurAimChg;               /* f_runSrc.c:1327 extern (真身: f_runSrc_accDecFrq.c:52) */

/* frqLine 的真身在 f_runSrc_accDecFrq.c 里带着初始化式
 * (LINE_FRQ_CHANGE_STRTUCT_DEFALUTS 把 calc 指向 LineFrqChangeCalc)。这里**故意**
 * 不复制那个函数指针: LineFrqChangeCalc 也在 f_runSrc_accDecFrq.c 里, 本用例不链接它,
 * 写上就是未定义符号。f_runSrc.c 对 frqLine 只有一句 `frqLine.remainder = 0;`,
 * 从不经 .calc 调用, 所以留 0 不影响被测行为 —— 但这是一个**与真机不同的值**,
 * 记在这里以免以后误以为它等价。 */
LINE_CHANGE_STRUCT frqLine;         /* f_runSrc.h    */

int32  frq;                         /* f_frqSrc.h    */
int32  frqTmp;                      /* f_frqSrc.h    */
int32  frqFrac;                     /* f_frqSrc.h    */
int32  frqTmpFrac;                  /* f_frqSrc.h    */
int32  frqCurAim;                   /* f_frqSrc.h    */
int32  frqAimTmp0;                  /* f_frqSrc.h    */
int32  frqAimOld4Dir;               /* f_frqSrc.h    */
union FRQ_FLAG frqFlag;             /* f_frqSrc.h    */
int32  upDownFrq;                   /* f_frqSrc.h    :158  (int32, not Uint16) */
Uint16 plcStep;                     /* f_frqSrc.h    */
Uint32 plcTime;                     /* f_frqSrc.h    */
Uint16 plcStepRemOld;               /* f_frqSrc.h    */
Uint32 plcTimeRemOld;               /* f_frqSrc.h    */
Uint16 bStopPlc;                    /* f_frqSrc.h    */
Uint16 timeBench;                   /* f_frqSrc.h    */
Uint16 keyFunc;                     /* f_ui.h        UpdateRunCmd 会清它 */
enum MENU0_DISP_STATUS menu0DispStatus;  /* f_menu.h */
Uint16 commRunCmd;                  /* f_comm.h      UpdateRunCmd 读它又清它 */
struct CUR_TIME curTime;            /* f_error.h     runTimeCal / setTimeRun */
Uint16 errorOther;                  /* f_error.h     */

/* ===========================================================================
 * 四个外部函数桩
 *
 * 它们都**不在**被测模块里, 所以给固定返回值 + 记轨迹就够了 —— 这里要测的是
 * 模块**怎么用**它们的结果, 不是它们本身。
 *
 * 桩一律**尽量少做事**: 桩做得越多, 就越可能把模块自己的状态差异盖掉。
 * 逐个说明为什么"少做"是安全的:
 *
 *   AccDecFrqCalc  真身会写 frqCurAimOld / bFrqCurAimChg。本模块对这两个量**只写
 *                  不读** (frqCurAimOld 只在 f_runSrc.c:1460 和 :1884 被赋值,
 *                  bFrqCurAimChg 只在 :1334 被赋值), 所以它们不参与任何控制流。
 *                  真身的其余效果 (改 frqTmp/frqTmpFrac) 由 driver 直接按输入装,
 *                  比让桩去算更容易把分支走全。
 *   UpdateFrqAim   真身 (f_frqSrc.c) 由 frqAimTmp 算出 frqAim。frqAim 是本用例的
 *                  **输入**, driver 直接装, 所以桩不写它反而能让 frqCurAim = frqAim
 *                  这条路径取到更多不同的值。
 *   GetErrorAttribute  只在一处用 (LoseLoadRunCtrl), 返回值可设即可。
 *   SaveTuneData   真身写 EEPROM。本用例没有硬件边界, 记一次调用就够。
 * ======================================================================== */

struct RUNTRACE runTrace[RUNTRACE_N];
Uint16 runTraceN;
Uint16 mockErrorAttrLevel;

static void trace(Uint16 which, int32 a, int32 b, int32 c)
{
    if (runTraceN < RUNTRACE_N)
    {
        runTrace[runTraceN].which = which;
        runTrace[runTraceN].a = a;
        runTrace[runTraceN].b = b;
        runTrace[runTraceN].c = c;
        runTraceN++;
    }
}

void UpdateFrqAim(void)
{
    trace(RUNTRACE_UPD_FRQ_AIM, 0, 0, 0);
}

void AccDecFrqCalc(int32 accTime, int32 decTime, Uint16 mode)
{
    /* 三个实参全记 —— 只记一个的话, "换错了时间源"这类变异会漏网。 */
    trace(RUNTRACE_ACC_DEC_FRQ, accTime, decTime, (int32)mode);
}

Uint16 GetErrorAttribute(Uint16 errCode)
{
    trace(RUNTRACE_GET_ERR_ATTR, (int32)errCode, 0, 0);
    return mockErrorAttrLevel;
}

void SaveTuneData(void)
{
    trace(RUNTRACE_SAVE_TUNE_DATA, 0, 0, 0);
}
