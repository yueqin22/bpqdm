/* f_runSrc_accDecFrq 差分测试的共享 mock。
 *
 * 一份实例, ref 与 new 两侧都读它 —— 所以两侧看到的是**完全相同**的输入。
 * 被测模块自己导出的 9 个符号由 ref_tu.c / new_tu.c 各自复制一份, 那才是比对的
 * 对象。
 *
 * include 顺序与 src/function/f_runSrc_accDecFrq.c 一致 (再加上 f_interface.h,
 * 因为 frqRun / frqVFRunRemainder 声明在那里)。
 *
 * 这张清单不是猜的: `nm2000 -u build/Debug/f_runSrc_accDecFrq.obj` 只列出
 * 15 个数据符号 + qsqrt, 一个不多。逐条标出定义它的头文件, 便于以后核对。
 *
 * 类型全部用**真类型**。理由: 本模块的运算建立在若干宽度前提上, 换成替身会
 * 移动截断点, 测试就变成在验证另一份代码。具体有四个:
 *
 *   * benchFrq 是 **Uint16**, 而模块给它赋的是 ABS_INT32(frqCurAim) /
 *     100*decNumber[frqPoint] / maxFrq —— 三个都是可能超出 16 位的值。
 *     每一次赋值都是一处**截断点** (例: frqCurAim = 70000 -> benchFrq = 4464;
 *     frqPoint = 4 时 100*10000 = 1000000 -> benchFrq = 16960)。改写时这些
 *     截断点必须逐字节保持。
 *   * maxFrq / timeBench 也是 Uint16。
 *   * decNumber 是 **const int16[5]**, 真身定义在 src/function/f_menu.c:81,
 *     值 {1, 10, 100, 1000, 10000}。funcCode.code.frqPoint 索引它, 所以
 *     驱动必须把 frqPoint 钉在 0..4。
 *   * runStatus 是 **enum RUN_STATUS** (宿主上 4 字节), 不是 Uint16。
 *
 * 分两组:
 *   A. 只读 (11 个) —— 模块只读不写, 驱动每次调用前按输入装好。
 *   B. 会被写 (5 个) —— 两侧写的是**同一个**对象, 所以必须按侧快照, 由驱动的
 *      IMG 机制在每次调用前装回去。本文件只负责定义, 初值无所谓。
 */

#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_main.h"
#include "f_menu.h"
#include "f_debug.h"
#include "f_interface.h"

/* ===========================================================================
 * A. 只读的外部量 (11 个)
 * ======================================================================== */

FUNCCODE_ALL funcCode;              /* f_funcCode.h  code.ovGain / ocGain /
                                                     droopCtrl / accDecBenchmark /
                                                     frqPoint / sCurveStartPhaseTime /
                                                     sCurveEndPhaseTime —— 全是 Uint16 */
struct MOTOR_FC motorFc;            /* f_funcCode.h  motorCtrlMode,
                                                     motorPara.elem.{motorType,ratingFrq} */
enum RUN_STATUS runStatus;          /* f_runSrc.h    == RUN_STATUS_TUNE 是守卫链的一项 */

int32  frqRun;                      /* f_interface.h 守卫链的最后一项 / frq0 的来源之一 */
int32  frqCurAim;                   /* f_frqSrc.h    目标频率 */
int32  frqCurAimFrac;               /* f_frqSrc.h    frqCurAim 的小数部分 */
Uint16 maxFrq;                      /* f_frqSrc.h    accDecBenchmark==0 时的 benchFrq */
Uint16 timeBench;                   /* f_frqSrc.h    accDecTime 的换算基准 */

const int16 decNumber[5] = {1, 10, 100, 1000, 10000};   /* f_menu.h / f_menu.c:81 */

/* ===========================================================================
 * B. 会被写的外部量 (5 个) —— 按侧快照
 * ======================================================================== */

int32  frqTmp;                      /* f_frqSrc.h    入口是输入 (frq0 的来源之一),
                                                    出口是输出 (frqTmp = frq0) */
int32  frqTmpFrac;                  /* f_frqSrc.h    出口输出 (含 S 曲线的小数余量) */
int32  frqVFRunRemainder;           /* f_interface.h 走 frqTmp 那条路时被清 0 */
Uint16 benchFrq;                    /* f_frqSrc.h    加速时被写成 |frqCurAim|, 之后可被
                                                     accDecBenchmark 覆盖 */
union RUN_FLAG runFlag;             /* f_runSrc.h    bit.accDecStatus: CONST/ACC/DEC */

/* ===========================================================================
 * C. 库函数替身 (1 个)
 * ======================================================================== */

/* qsqrt 在目标端是 asm/ASMSubPrg.asm 里的定点开方, 本模块用它两次
 * (SCurveSpecialCalc 的 SB_STEP_START_S 入口与 SB_STEP_END_S)。
 *
 * 这里用**整数开方**代替。理由与 tools/difftest/hostlib_mock.c 顶部那段一致:
 * 汇编里 qsqrt 的 I/O 契约就是"32 位幅值的整数平方根" (Q15 的 SQRT2 因子在
 * 汇编内部折进折出), 位模式没有复刻。
 *
 * 为什么不用现成的 hostlib_mock.c: 它为了拿 Uint/Ulong 而 include 了真的
 * MotorDefine.h, 那条链会拉到 SystemDefine.h -> DSP2803x_Device.h, 而本用例
 * 不需要整个外设模型 (本模块一个寄存器都不碰), 拉进来只会多一份宿主补丁。
 * 直接在这里按 f_debug.h:55 的原型 `Uint16 qsqrt(Uint32)` 写一份, 签名反而是
 * **精确对上**的。
 *
 * 近似带来的后果: 数值与目标端不是逐位相同。对差分测试无害 —— 两侧调的是同一
 * 份替身, 被测模块的行为差异照样显形。真正会丢的是"输入不敏感"那种覆盖:
 * qsqrt 严格单调, 且下面每次调用都参与后续除法/移位, 所以这条不适用。
 * 如果以后需要绝对数值 (金标向量), 那要先把多项式从汇编里搬过来。 */
Uint16 qsqrt(Uint32 data)
{
    Uint32 r = 0;
    Uint32 bit = 1UL << 30;

    if (data == 0)
    {
        return 0;
    }
    while (bit > data)
    {
        bit >>= 2;
    }
    while (bit != 0)
    {
        if (data >= r + bit)
        {
            data -= r + bit;
            r = (r >> 1) + bit;
        }
        else
        {
            r >>= 1;
        }
        bit >>= 2;
    }
    return (Uint16)r;
}

