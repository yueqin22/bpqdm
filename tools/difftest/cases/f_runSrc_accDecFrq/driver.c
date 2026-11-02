/* f_runSrc_accDecFrq 差分测试驱动。
 *
 * 被测模块: src/function/f_runSrc_accDecFrq.c (615 行), 四个函数
 *
 *   void  AccDecFrqCalc      (int32 accTime, int32 decTime, Uint16 mode)   调度 + 守卫
 *   int32 SCurveOridinaryCalc(int32 frq0, int32 accDecTime)                三段 S 曲线 A
 *   int32 SCurveSpecialCalc  (int32 frq0, int32 accDecTime)                三段 S 曲线 B
 *   void  LineChangeCalc     (LINE_CHANGE_STRUCT *p)                       线性斜坡
 *
 * ---------------------------------------------------------------------------
 * 一、两侧怎么隔离
 * ---------------------------------------------------------------------------
 * ref_tu.c / new_tu.c 把模块的 9 个外部符号改名后各包含一份实现。函数与
 * **文件级全局量** (frqLine / frqCurAimOld / accTimeOld / decTimeOld /
 * bFrqCurAimChg) 因此各有两份, 那是比对对象。
 *
 * 模块读写的**外部量** (frqTmp / frqTmpFrac / frqVFRunRemainder / benchFrq /
 * runFlag) 只有一份 (定义在 mock_accdec.c 里), 两侧写的是同一个对象。所以
 * 每次调用前必须按侧装回去, 调用后按侧快照 —— 否则 ref 侧的写入会变成 new 侧
 * 的输入, 差分测试立刻失去意义。这就是 IMG / load_side / save_side 的存在理由。
 *
 * ---------------------------------------------------------------------------
 * 二、函数内的 static 变量 (本模块与 44 号模块最大的不同)
 * ---------------------------------------------------------------------------
 * 两个 S 曲线函数各有一串函数内 static:
 *
 *   SCurveOridinaryCalc: sCurveAStatus, ticker, t[3], a[3], q[3], remainder
 *   SCurveSpecialCalc  : sCurveBStatus, ticker, tickerAll1/2/3, inflexionFrq,
 *                        pointFrq, rem1, remainder, delta, deltaFrqAim
 *
 * 它们**不是外部符号**, 驱动没有任何句柄, 装不进去也读不出来。这与 f_frqSrc /
 * f_eeprom / f_error 等用例的处理方式一致 (见 f_frqSrc/driver.c 顶部 "state the
 * module carries, and how the driver reaches it"): 两侧从同一个零初态出发, 每次
 * 场景**两侧都跑完全相同的调用序列**, 所以只要两个实现等价, 它们的 static 就
 * 同步演化。驱动唯一必须守住的是"绝不给两侧不同的调用序列", run_tick() 写成了
 * 不可能违反这一点。
 *
 * 代价要说清楚: 一旦某一格出现差异, 两侧的 static 就此分叉, **后面的格子全部
 * 作废**。所以 run_tick() 在第一次不一致之后仍然继续跑 (便于看清差异的形状),
 * 但结论只认第一处 —— 与所有既有用例同一个约定。
 *
 * 为了把 static 摆到想要的起点, 驱动用了两个手段:
 *   * bFrqCurAimChg 是模块自己的全局量, 驱动可以直接设成 1。两个 S 曲线函数
 *     在 `if (bFrqCurAimChg)` 里重新初始化绝大部分 static (SCurveOridinaryCalc
 *     是**全部**), 所以每个序列相的第一拍设 1、后面设 0, 就等于把状态机拉回
 *     起点再走一遍。
 *   * SCurveOridinaryCalc / SCurveSpecialCalc 是外部链接的 (f_debug.h 的 LOCALF
 *     在 `#if 1` 分支里展开成空), 所以驱动可以**直接调它们**, 把 accDecTime /
 *     frq0 / benchFrq / accDecStatus 摆好, 不必绕 AccDecFrqCalc 的守卫链。
 *
 * ---------------------------------------------------------------------------
 * 三、除法与"预存缺陷"
 * ---------------------------------------------------------------------------
 * 本模块有 14 处除法, 其中 6 处的除数在可达状态空间里可能为 0。C28x 与宿主在
 * 除零上的表现不同 (前者陷阱/未定义, 后者 0xC0000094 直接终止), 所以驱动必须
 * 把输入约束在"真机上也到不了"的区域之外。逐条:
 *
 *   a. SCurveOridinaryCalc: `/(int32)benchFrq`  —— 驱动保证 benchFrq != 0。
 *      benchFrq 是 Uint16, 模块给它赋 ABS_INT32(frqCurAim) / maxFrq /
 *      100*decNumber[frqPoint], 三者都可能被截断成 0 (frqCurAim 是 65536 的
 *      整数倍, maxFrq == 0, ...), 所以驱动显式钉住它。
 *   b. SCurveOridinaryCalc: `/((int64)accDecTime * lineTime)` —— lineTime =
 *      2000 - startTime - endTime。驱动保证 startTime + endTime != 2000,
 *      并让 accDecTime != 0 (accDecTime == 0 时函数在入口就 return 了)。
 *   c. SCurveOridinaryCalc: `/tmp3` 与 `/startTime` / `/endTime` —— tmp3 =
 *      ((int64)accDecTime * |frqCurAim - frq0| << 16) / benchFrq。进入 S 曲线时
 *      accDecTime >= 1 且 |frqCurAim - frq0| >= 1, 所以 accDecTime*|delta| >= 1,
 *      左移 16 位后 >= 65536 > benchFrq, tmp3 >= 1。startTime / endTime 由驱动
 *      钉成 >= 1。
 *   d. SCurveSpecialCalc: `/(int32)benchFrq` 与 `/inflexionFrq` —— inflexionFrq
 *      是 ±ratingFrq, 驱动保证 ratingFrq >= 1。
 *   e. SCurveSpecialCalc: `/(9 * tickerAll2)` 与 `/(tickerAll2 - tickerAll1)`
 *      —— tickerAll1 = tickerAll2*13848>>15 < tickerAll2 (对 tickerAll2 >= 1 恒
 *      成立), 所以差值不为 0; 两处除法都在 `if (flag)` 里, 而 flag 需要
 *      ticker 越过 tickerAll 边界, tickerAll2 == 0 时该边界是 0、永远越不过去。
 *      驱动另外保证 accTime/decTime >= 0 (负的 accDecTime 会让 tickerAll2 变负,
 *      tickerAll2 == -1 时 tickerAll1 也是 -1, 差值为 0 —— 那是真机也到不了的
 *      状态, 不在测试范围内, 记在 README 的"测不出来的十件事"里)。
 *   f. AccDecFrqCalc 的降级判据: `/(int32)timeBench` —— 驱动保证 timeBench >= 1。
 *
 * ---------------------------------------------------------------------------
 * 四、比对的镜像
 * ---------------------------------------------------------------------------
 * IMG 里放三组东西:
 *   * 模块自有的文件级全局量 (frqCurAimOld / accTimeOld / decTimeOld /
 *     bFrqCurAimChg / frqLine 五个字段);
 *   * 模块写的外部量 (frqTmp / frqTmpFrac / frqVFRunRemainder / benchFrq /
 *     runFlag.all);
 *   * 调用结果 (SCurve* 的返回值、LineChangeCalc 出口的整个结构体)。
 *
 * frqLine.calc 是**函数指针**, 两侧必然不同 (ref 指向 ref_LineChangeCalc, new
 * 指向 new_LineChangeCalc), 直接比地址会永远不一致。所以归一化成 calcKind:
 * 0 = NULL, 1 = 指向本侧自己的 LineChangeCalc, 2 = 别的非空地址 (原始地址另外
 * 记下来供报告用)。比 calcKind 而不是比地址, 既公平又能抓到"指针被清空"和
 * "指针被改成别的东西"。
 */

#include <stdio.h>
#include <string.h>

/* <stdlib.h> 刻意不包含: 见 MotorParChange/driver.c 顶部 —— hostlib_mock.c 自带
 * `abs` / `labs` 的定义, 让 stdlib 的原型参与进来只会制造无关的噪声。 */
extern long atol(const char *);

#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_main.h"
#include "f_menu.h"
#include "f_debug.h"
#include "f_interface.h"

#include "accdec_sides.h"

/* ------------------------------------------------------------------------- */
/* 计数器                                                                     */
/* ------------------------------------------------------------------------- */

static long g_cases;
static long g_mism;
static long g_mismShown;

/* ------------------------------------------------------------------------- */
/* 调用结果 (save_side 里合并进 IMG)                                          */
/* ------------------------------------------------------------------------- */

struct CALLRES
{
    int32 ret;
    LINE_CHANGE_STRUCT line;
};

static struct CALLRES g_call;

/* ------------------------------------------------------------------------- */
/* 比较镜像                                                                   */
/* ------------------------------------------------------------------------- */

struct IMG
{
    /* --- 模块自有的文件级全局量 --- */
    int32  frqCurAimOld;
    int32  accTimeOld;
    int32  decTimeOld;
    Uint16 bFrqCurAimChg;

    /* frqLine 的五个字段 + 归一化后的函数指针 */
    int32  maxValue;
    int32  aimValue;
    int32  curValue;
    Uint32 tickerAll;
    int32  remainder;
    Uint16 calcKind;
    unsigned long calcRaw;      /* 只在报告里出现, 不参与相等判定 */

    /* --- 模块写的外部量 --- */
    int32  frqTmp;
    int32  frqTmpFrac;
    int32  frqVFRunRemainder;
    Uint16 benchFrq;
    Uint16 runFlagAll;

    /* --- 调用结果 --- */
    int32  ret;
    int32  oMaxValue;
    int32  oAimValue;
    int32  oCurValue;
    Uint32 oTickerAll;
    int32  oRemainder;
    Uint16 oCalcKind;
};

static struct IMG sRef;
static struct IMG sNew;
static struct IMG g_carry;      /* 序列相: 上一次调用的结果, 也就是下一拍的起点 */

/* ------------------------------------------------------------------------- */
/* 侧相关存取                                                                 */
/* ------------------------------------------------------------------------- */

#define CALCKIND_NULL   0
#define CALCKIND_OWN    1
#define CALCKIND_OTHER  2

/* "别的非空地址"用一个**真的函数**当哨兵, 不能用 0x1234 这类假地址:
 * AccDecFrqCalc 在 LINE 模式下会 `frqLine.calc(&frqLine)`, 假地址就是
 * ACCESS_VIOLATION (实测 rc = 0xC0000005)。
 *
 * 用真函数反而更有价值: 它把"模块是**通过指针**调 LineChangeCalc, 还是直接
 * 调函数名"区分开来。若改写把 `frqLine.calc(&frqLine)` 换成直接调用, ref 侧
 * 什么也不做、new 侧照常爬坡, 差分立刻显形。 */
static void other_calc(void *p)
{
    (void)p;
}

#define CALC_SENTINEL   ((void (*)(void *))other_calc)

static void (*own_line(int side))(void *)
{
    return side ? (void (*)(void *))new_LineChangeCalc
                : (void (*)(void *))ref_LineChangeCalc;
}

#define PA(n)  (side ? &new_##n : &ref_##n)

static void save_side(int side, struct IMG *s)
{
    LINE_CHANGE_STRUCT *ln = PA(frqLine);

    memset(s, 0, sizeof(*s));

    s->frqCurAimOld    = *PA(frqCurAimOld);
    s->accTimeOld      = *PA(accTimeOld);
    s->decTimeOld      = *PA(decTimeOld);
    s->bFrqCurAimChg   = *PA(bFrqCurAimChg);

    s->maxValue  = ln->maxValue;
    s->aimValue  = ln->aimValue;
    s->curValue  = ln->curValue;
    s->tickerAll = ln->tickerAll;
    s->remainder = ln->remainder;

    if (ln->calc == 0)
    {
        s->calcKind = CALCKIND_NULL;
    }
    else if (ln->calc == own_line(side))
    {
        s->calcKind = CALCKIND_OWN;
    }
    else
    {
        s->calcKind = CALCKIND_OTHER;
        s->calcRaw  = (unsigned long)ln->calc;
    }

    s->frqTmp             = frqTmp;
    s->frqTmpFrac         = frqTmpFrac;
    s->frqVFRunRemainder  = frqVFRunRemainder;
    s->benchFrq           = benchFrq;
    s->runFlagAll         = runFlag.all;

    s->ret       = g_call.ret;
    s->oMaxValue = g_call.line.maxValue;
    s->oAimValue = g_call.line.aimValue;
    s->oCurValue = g_call.line.curValue;
    s->oTickerAll = g_call.line.tickerAll;
    s->oRemainder = g_call.line.remainder;
    s->oCalcKind  = (g_call.line.calc == 0) ? CALCKIND_NULL : CALCKIND_OTHER;
}

static void load_side(int side, const struct IMG *s)
{
    LINE_CHANGE_STRUCT *ln = PA(frqLine);

    *PA(frqCurAimOld)  = s->frqCurAimOld;
    *PA(accTimeOld)    = s->accTimeOld;
    *PA(decTimeOld)    = s->decTimeOld;
    *PA(bFrqCurAimChg) = s->bFrqCurAimChg;

    ln->maxValue  = s->maxValue;
    ln->aimValue  = s->aimValue;
    ln->curValue  = s->curValue;
    ln->tickerAll = s->tickerAll;
    ln->remainder = s->remainder;

    if (s->calcKind == CALCKIND_NULL)
    {
        ln->calc = 0;
    }
    else if (s->calcKind == CALCKIND_OWN)
    {
        ln->calc = own_line(side);
    }
    else
    {
        ln->calc = CALC_SENTINEL;
    }

    frqTmp            = s->frqTmp;
    frqTmpFrac        = s->frqTmpFrac;
    frqVFRunRemainder = s->frqVFRunRemainder;
    benchFrq          = s->benchFrq;
    runFlag.all       = s->runFlagAll;
}

/* ------------------------------------------------------------------------- */
/* 报告                                                                       */
/* ------------------------------------------------------------------------- */

static void report(const char *field, const char *detail)
{
    g_mism++;
    if (g_mismShown < 60)
    {
        g_mismShown++;
        printf("MISMATCH %s: %s\n", field, detail);
    }
}

#define CMPI(f)                                                               \
    do {                                                                      \
        if (a->f != b->f)                                                     \
        {                                                                     \
            char d[160];                                                      \
            sprintf(d, "ref=%ld new=%ld", (long)a->f, (long)b->f);            \
            report(#f, d);                                                    \
        }                                                                     \
    } while (0)

#define CMPU(f)                                                               \
    do {                                                                      \
        if (a->f != b->f)                                                     \
        {                                                                     \
            char d[160];                                                      \
            sprintf(d, "ref=%lu new=%lu", (unsigned long)a->f,                \
                    (unsigned long)b->f);                                     \
            report(#f, d);                                                    \
        }                                                                     \
    } while (0)

static void cmp_img(const char *tag, const struct IMG *a, const struct IMG *b,
                    const char *what)
{
    long before = g_mism;

    CMPI(frqCurAimOld);
    CMPI(accTimeOld);
    CMPI(decTimeOld);
    CMPU(bFrqCurAimChg);

    CMPI(maxValue);
    CMPI(aimValue);
    CMPI(curValue);
    CMPU(tickerAll);
    CMPI(remainder);
    CMPU(calcKind);

    CMPI(frqTmp);
    CMPI(frqTmpFrac);
    CMPI(frqVFRunRemainder);
    CMPU(benchFrq);
    CMPU(runFlagAll);

    CMPI(ret);
    CMPI(oMaxValue);
    CMPI(oAimValue);
    CMPI(oCurValue);
    CMPU(oTickerAll);
    CMPI(oRemainder);
    CMPU(oCalcKind);

    if (g_mism != before)
    {
        printf("  ^ in %s\n", what);
    }
}

/* ------------------------------------------------------------------------- */
/* 输入                                                                       */
/* ------------------------------------------------------------------------- */

#define W_ACCDEC   0
#define W_SCA      1
#define W_SCB      2
#define W_LINE     3

struct IN
{
    int which;

    /* 调用实参 */
    int32 accTime;
    int32 decTime;
    Uint16 mode;
    int32 frq0in;
    int32 accDecTime;
    LINE_CHANGE_STRUCT line;

    /* 模块读的共享量 */
    int32  frqRun;
    int32  frqCurAim;
    int32  frqCurAimFrac;
    Uint16 maxFrq;
    Uint16 timeBench;
    Uint16 runStatusE;
    Uint16 motorCtrlMode;
    Uint16 motorType;
    Uint16 ratingFrq;
    Uint16 ovGain;
    Uint16 ocGain;
    Uint16 droopCtrl;
    Uint16 accDecBenchmark;
    Uint16 frqPoint;
    Uint16 sCurveStartPhaseTime;
    Uint16 sCurveEndPhaseTime;

    /* 起始镜像 */
    struct IMG start;
};

static void apply_common(const struct IN *in)
{
    frqRun         = in->frqRun;
    frqCurAim      = in->frqCurAim;
    frqCurAimFrac  = in->frqCurAimFrac;
    maxFrq         = in->maxFrq;
    timeBench      = in->timeBench;
    runStatus      = (enum RUN_STATUS)in->runStatusE;

    motorFc.motorCtrlMode          = in->motorCtrlMode;
    motorFc.motorPara.elem.motorType = in->motorType;
    motorFc.motorPara.elem.ratingFrq = in->ratingFrq;

    funcCode.code.ovGain            = in->ovGain;
    funcCode.code.ocGain            = in->ocGain;
    funcCode.code.droopCtrl         = in->droopCtrl;
    funcCode.code.accDecBenchmark   = in->accDecBenchmark;
    funcCode.code.frqPoint          = in->frqPoint;
    funcCode.code.sCurveStartPhaseTime = in->sCurveStartPhaseTime;
    funcCode.code.sCurveEndPhaseTime   = in->sCurveEndPhaseTime;
}

static void call_side(int side, const struct IN *in)
{
    g_call.ret = 0;
    memset(&g_call.line, 0, sizeof(g_call.line));

    switch (in->which)
    {
        case W_ACCDEC:
            if (side)
            {
                new_AccDecFrqCalc(in->accTime, in->decTime, in->mode);
            }
            else
            {
                ref_AccDecFrqCalc(in->accTime, in->decTime, in->mode);
            }
            break;

        case W_SCA:
            g_call.ret = side
                ? new_SCurveOridinaryCalc(in->frq0in, in->accDecTime)
                : ref_SCurveOridinaryCalc(in->frq0in, in->accDecTime);
            break;

        case W_SCB:
            g_call.ret = side
                ? new_SCurveSpecialCalc(in->frq0in, in->accDecTime)
                : ref_SCurveSpecialCalc(in->frq0in, in->accDecTime);
            break;

        case W_LINE:
            g_call.line = in->line;
            if (side)
            {
                new_LineChangeCalc(&g_call.line);
            }
            else
            {
                ref_LineChangeCalc(&g_call.line);
            }
            break;

        default:
            break;
    }
}

/* ------------------------------------------------------------------------- */
/* 覆盖计数                                                                   */
/* ------------------------------------------------------------------------- */

/* 只统计**从镜像里直接读得出来**的事实, 不统计"我以为走到了"的路径。
 * 每条都能被读者用 sRef 的字段复核。 */
#define COV_LIST(X)                                                           \
    X(GUARD_FRQ_RUN)        /* 守卫成立 -> frq0 = frqRun */                   \
    X(GUARD_FRQ_TMP)        /* 守卫不成立 -> frq0 = frqTmp */                 \
    X(STATUS_CONST)         /* frqCurAim == frq0 那条 */                      \
    X(STATUS_ACC)                                                             \
    X(STATUS_DEC)                                                             \
    X(BENCH_FROM_MAX)       /* accDecBenchmark == 0 */                        \
    X(BENCH_FROM_DECN)      /* accDecBenchmark == 2 */                        \
    X(BENCH_OTHER)          /* 两条都没覆盖 (含 ACC 分支写 |frqCurAim|) */     \
    X(FRAC_NONZERO)         /* S 曲线小数余量真的动了 */                       \
    X(TICKER_ZERO)          /* frqLine.tickerAll == 0 那条 */                  \
    X(TICKER_NONZERO)                                                         \
    X(RET_EQ_AIM)           /* S 曲线返回 clamp 到 frqCurAim */                \
    X(RET_NE_AIM)                                                             \
    X(LINE_REM_NONZERO)     /* LineChangeCalc 的余数真的动了 */                \
    X(LINE_OUT_EQ_AIM)      /* LineChangeCalc 走到 aimValue */                 \
    X(CALC_OWN)                                                               \
    X(CALC_NULL)                                                              \
    X(CALC_OTHER)

#define COV_ENUM(name) COV_##name,
enum { COV_LIST(COV_ENUM) COV_COUNT };
#undef COV_ENUM

static long g_cov[COV_COUNT];

#define COV_INC(name) (g_cov[COV_##name]++)

static const char *g_covName[COV_COUNT] = {
#define COV_NAME(name) #name,
    COV_LIST(COV_NAME)
#undef COV_NAME
};

/* runFlag.bit.accDecStatus 落在 bit5..bit6 (run:1, common:1, jog:1, tune:1,
 * jogWhenRun:1 之后)。宿主是小端, 位域从低位往高位排。
 *
 * ACCDEC_BITS() 是它的逆。**没有它就会静默装错** —— 第一版直接写
 * `runFlagAll = ACC_SPEED`, 那是把 1 放到 bit0 (run), accDecStatus 仍然是
 * CONST_SPEED, 于是整个 S 曲线状态机的 ACC 分支一条都没走到。覆盖计数当场
 * 露馅: STATUS_ACC 只有 18625 而 phase_scurve_a/b 有 20 多万拍。 */
#define ACCDEC_STATUS_OF(all)   ((Uint16)(((all) >> 5) & 3))
#define ACCDEC_BITS(v)          ((Uint16)(((v) & 3) << 5))

static void coverage(const struct IN *in, const struct IMG *before,
                     const struct IMG *after)
{
    Uint16 st = ACCDEC_STATUS_OF(after->runFlagAll);

    if (st == CONST_SPEED) COV_INC(STATUS_CONST);
    else if (st == ACC_SPEED) COV_INC(STATUS_ACC);
    else if (st == DEC_SPEED) COV_INC(STATUS_DEC);

    if (after->frqTmpFrac != 0)
    {
        COV_INC(FRAC_NONZERO);
    }

    if (after->tickerAll == 0)
    {
        COV_INC(TICKER_ZERO);
    }
    else
    {
        COV_INC(TICKER_NONZERO);
    }

    switch (after->calcKind)
    {
        case CALCKIND_NULL:  COV_INC(CALC_NULL);  break;
        case CALCKIND_OWN:   COV_INC(CALC_OWN);   break;
        default:             COV_INC(CALC_OTHER); break;
    }

    if (in->which == W_ACCDEC)
    {
        /* frqVFRunRemainder 只在守卫**不**成立那条路上被清 0, 所以
         * "起始非零、出口仍等于起始值" 就是"走了 frq0 = frqRun"的充分证据。 */
        if (before->frqVFRunRemainder != 0 &&
            after->frqVFRunRemainder == before->frqVFRunRemainder)
        {
            COV_INC(GUARD_FRQ_RUN);
        }
        else
        {
            COV_INC(GUARD_FRQ_TMP);
        }

        if (after->benchFrq == in->maxFrq)
        {
            COV_INC(BENCH_FROM_MAX);
        }
        else if (after->benchFrq == (Uint16)(100 * decNumber[in->frqPoint]))
        {
            COV_INC(BENCH_FROM_DECN);
        }
        else
        {
            COV_INC(BENCH_OTHER);
        }
    }

    if (in->which == W_SCA || in->which == W_SCB)
    {
        if (after->ret == in->frqCurAim)
        {
            COV_INC(RET_EQ_AIM);
        }
        else
        {
            COV_INC(RET_NE_AIM);
        }
    }

    if (in->which == W_LINE)
    {
        if (after->oRemainder != 0)
        {
            COV_INC(LINE_REM_NONZERO);
        }
        if (after->oCurValue == in->line.aimValue)
        {
            COV_INC(LINE_OUT_EQ_AIM);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* 除数安全判据                                                               */
/* ------------------------------------------------------------------------- */

/* 本模块有 14 处除法, 其中 6 处的除数在可达状态空间里可能为 0 (见文件头第三节)。
 *
 * 判据集中在这里, 由 run_tick2() **每一拍**都过一遍。第一版把它们散在各个相里,
 * 结果 phase_mode 漏了 SCurveSpecialCalc 的 tickerAll2 >= 1, 宿主以
 * 0xC0000094 (INTEGER_DIVIDE_BY_ZERO) 直接终止。集中之后新加一个相也不可能
 * 绕过它。
 *
 * 被跳过的拍数最后会打出来 (skipped=...), 所以"排除了多少"永远可见, 不会变成
 * 静默降级。 */

/* 模块将要用的 benchFrq: ACC 分支写 |frqCurAim|, DEC 分支保留起始值,
 * accDecBenchmark 0/2 再覆盖一次。三次赋值都落在 Uint16 上, 都可能截断成 0。 */
static Uint16 bench_effective(const struct IN *in, int acc)
{
    Uint16 b;

    if (acc)
    {
        b = (Uint16)((in->frqCurAim < 0) ? -in->frqCurAim : in->frqCurAim);
    }
    else
    {
        b = in->start.benchFrq;
    }

    if (in->accDecBenchmark == 0)
    {
        b = in->maxFrq;
    }
    else if (in->accDecBenchmark == 2)
    {
        b = (Uint16)(100 * decNumber[in->frqPoint]);
    }
    return b;
}

/* start 是这一拍的起始镜像: 单发相是 in->start, 序列相是 g_carry ——
 * 后者意味着模块自己的输出已经变成下一拍的输入, 所以 frqTmp / benchFrq
 * 必须从**携带状态**里取, 不能从 in->start 里取。 */
static int tick_safe(const struct IN *in, const struct IMG *start)
{
    Uint16 startTime = in->sCurveStartPhaseTime;
    Uint16 endTime   = in->sCurveEndPhaseTime;
    int32  accDecTime = 0;
    int32  frq0 = 0;
    int32  d;
    int    needScb = 0;
    Uint16 bench = 0;

    if (in->which == W_LINE)
    {
        return 1;                    /* tickerAll == 0 有自己的分支, 不除零 */
    }

    if (in->timeBench == 0)
    {
        return 0;                    /* 降级判据里的 100*1000/timeBench */
    }

    if (in->which == W_SCA || in->which == W_SCB)
    {
        if (startTime == 0 || endTime == 0)
        {
            return 0;                /* tmp1/startTime, tmp1/endTime */
        }
        if ((((Uint32)startTime + endTime) & 0xFFFFUL) == 2000UL)
        {
            /* lineTime 是 **Uint16**:
             *     Uint16 lineTime = 2000 - startTime - endTime;
             * 所以"和 == 2000"是错的判据 —— startTime = 65535、endTime = 2001
             * 时 2000-65535-2001 = -65536, 截成 Uint16 正好是 **0**。
             * 这是实测撞出来的第三处除零 (rc = 0xC0000094)。
             * 正确判据是 (startTime + endTime) mod 65536 == 2000。 */
            return 0;
        }
        if (start->benchFrq == 0)
        {
            return 0;
        }

        accDecTime = in->accDecTime;
        frq0       = in->frq0in;
        bench      = start->benchFrq;
        needScb    = (in->which == W_SCB);
    }
    else                             /* W_ACCDEC */
    {
        int guard = (in->ovGain || in->ocGain)
                    && (in->motorCtrlMode == FUNCCODE_motorCtrlMode_VF)
                    && (in->motorType != MOTOR_TYPE_PMSM)
                    && (!in->droopCtrl)
                    && (in->runStatusE != RUN_STATUS_TUNE)
                    && (!((in->frqCurAim == 0)
                          && (((in->frqRun < 0) ? -in->frqRun : in->frqRun)
                              <= 50)));
        int32 raw;
        int   downgrade;
        int   acc;

        frq0 = guard ? in->frqRun : start->frqTmp;
        if (frq0 == in->frqCurAim)
        {
            return 1;                /* CONST_SPEED 那条, 不进 S 曲线 */
        }

        acc = ((frq0 >= 0) && (in->frqCurAim > frq0))
              || ((frq0 <= 0) && (in->frqCurAim < frq0));
        raw = acc ? in->accTime : in->decTime;

        /* 降级判据用的是**缩放之前**的 accDecTime。 */
        downgrade = (in->mode == FUNCCODE_accDecSpdCurve_S_CURVE_B)
                    && (((int64)raw > (int64)100 * TIME_UNIT_MS_PER_SEC
                                      / (int64)in->timeBench)
                        || (((in->frqCurAim < 0) ? -in->frqCurAim
                                                 : in->frqCurAim)
                            > (int32)6 * (int32)in->ratingFrq));

        accDecTime = raw * (int32)(in->timeBench / RUN_CTRL_PERIOD);

        if (downgrade || in->mode == FUNCCODE_accDecSpdCurve_LINE)
        {
            /* 这条路会 `frqLine.calc(&frqLine)`。calc == NULL 就是空指针调用,
             * 真机上到不了 (初始值设定项永远是 LineChangeCalc), 驱动排除掉。 */
            return start->calcKind != CALCKIND_NULL;
        }

        /* 剩下的都是 S 曲线。SCurveOridinaryCalc 只要求 |delta| >= 1;
         * SCurveSpecialCalc (mode == S_CURVE_B, 以及 DEBUG_F_S_CURVE_B 那个
         * else 分支接住的任何其它值) 还要求 tickerAll2 >= 1。 */
        needScb = (in->mode != FUNCCODE_accDecSpdCurve_S_CURVE_A);

        if (accDecTime <= 0)
        {
            return 1;                /* 入口就 return */
        }
        if (startTime == 0 || endTime == 0)
        {
            return 0;
        }
        if ((((Uint32)startTime + endTime) & 0xFFFFUL) == 2000UL)
        {
            return 0;                /* lineTime == 0 (见上面 W_SCA 分支的长注释) */
        }

        bench = bench_effective(in, acc);
        if (bench == 0)
        {
            return 0;
        }
    }

    if (accDecTime <= 0)
    {
        return 1;                    /* 入口就 return, 不进除法 */
    }

    d = in->frqCurAim - frq0;
    if (d < 0)
    {
        d = -d;
    }
    if (d < 1)
    {
        return 0;                    /* tmp3 == 0 */
    }

    if (needScb)
    {
        if ((int64)accDecTime * (int64)in->ratingFrq < (int64)bench)
        {
            return 0;                /* tickerAll2 == 0 -> /(9*tickerAll2) */
        }
    }
    return 1;
}

/* ------------------------------------------------------------------------- */
/* 一拍                                                                       */
/* ------------------------------------------------------------------------- */

static long g_skipped;

/* 两侧从**同一个**起始镜像出发各跑一次, 比对, 然后用 ref 侧的结果推进携带
 * 状态。起始镜像由调用方事先放进 g_carry。 */
static int run_tick2(const struct IN *in, const char *what)
{
    if (!tick_safe(in, &g_carry))
    {
        g_skipped++;
        return 0;
    }

    apply_common(in);

    load_side(0, &g_carry);
    call_side(0, in);
    save_side(0, &sRef);

    load_side(1, &g_carry);
    call_side(1, in);
    save_side(1, &sNew);

    g_cases++;
    cmp_img(what, &sRef, &sNew, what);
    coverage(in, &g_carry, &sRef);

    g_carry = sRef;
    return 1;
}

/* ------------------------------------------------------------------------- */
/* 起始镜像                                                                   */
/* ------------------------------------------------------------------------- */

static void img_reset(struct IMG *s)
{
    memset(s, 0, sizeof(*s));
    s->calcKind = CALCKIND_OWN;      /* 与 LINE_FRQ_CHANGE_STRTUCT_DEFALUTS 一致 */
}

static void in_reset(struct IN *in)
{
    memset(in, 0, sizeof(*in));
    img_reset(&in->start);

    in->which     = W_ACCDEC;
    in->frq0in    = 0;
    in->accDecTime = 0;
    in->line.calc = 0;

    in->frqRun        = 0;
    in->frqCurAim     = 0;
    in->frqCurAimFrac = 0;
    in->maxFrq        = 5000;
    in->timeBench     = 2;           /* timeBench/RUN_CTRL_PERIOD == 1, 不缩放 */
    in->runStatusE    = RUN_STATUS_NORMAL;
    in->motorCtrlMode = FUNCCODE_motorCtrlMode_VF;
    in->motorType     = MOTOR_TYPE_ACI_GENERAL;
    in->ratingFrq     = 500;         /* 非 0, 否则 inflexionFrq 除零 */

    in->ovGain          = 0;         /* 守卫链默认不成立 -> frq0 = frqTmp */
    in->ocGain          = 0;
    in->droopCtrl       = 0;
    in->accDecBenchmark = 1;         /* 不覆盖 benchFrq */
    in->frqPoint        = 2;         /* decNumber[2] == 100 */
    in->sCurveStartPhaseTime = 500;
    in->sCurveEndPhaseTime   = 500;

    /* 非零的初值, 好让"守卫成立 -> 不清零"与"守卫不成立 -> 清零"在每个相里
     * 都能从镜像上分辨出来。留 0 的话两条路长得一样, 覆盖计数也没法判。 */
    in->start.frqVFRunRemainder = 0x5A5A;
}

/* ------------------------------------------------------------------------- */
/* 预热: 把两个 S 曲线状态机的 static 摆到一个已知非零的起点                     */
/* ------------------------------------------------------------------------- */

/* 为什么必须有这一步 —— 这是实测撞出来的第二处除零 (rc = 0xC0000094):
 *
 * SCurveSpecialCalc 的 `if (bFrqCurAimChg)` 块是**唯一**重新计算 tickerAll2 的
 * 地方。当驱动把 bFrqCurAimChg 设成 0 (也就是真机每一拍都会走的稳态路径),
 * 那块被整个跳过, tickerAll2 用的是**上一次留下的值**。程序刚起来时它是 0,
 * 于是:
 *     START_S(DEC) -> SB_STEP_OK                      (无除法)
 *     而 ACC 起步则 START_S -> LINE -> END_S          (无除法, 因为 flag=0)
 *     到 END_S 且 accDecStatus == DEC 时:
 *         ticker--;  if (ticker > tickerAll2 + 0) flag = 1;
 *         -> `/(9 * tickerAll2)` = /0
 *
 * 也就是说"稳态拍 + tickerAll2 从未初始化"是一个可达状态组合, 而它在真机上
 * 同样会除零。驱动的处理办法不是绕过它, 而是**先把 static 预热到非零**:
 * 下面这一拍用 bFrqCurAimChg = 1 且参数保证 tickerAll2 >= 1, 之后 tick_safe()
 * 保证任何一次真正的重新初始化也都 >= 1, 于是"陈旧值"永远非零。
 *
 * 预热本身也过 run_tick2(), 所以它同样在比对。 */
static void prime(void)
{
    struct IN in;

    /* SCurveOridinaryCalc: tickerAll = (1000*3000<<16)/4000 >> 16 = 750,
     * t[0] = t[2] = 375, t[1] = 0 —— 三个都不全是 0, 状态机会正常推进。 */
    in_reset(&in);
    in.which     = W_SCA;
    in.frq0in    = 0;
    in.accDecTime = 1000;
    in.frqCurAim = 3000;
    in.ratingFrq = 500;
    in.sCurveStartPhaseTime = 500;
    in.sCurveEndPhaseTime   = 500;
    in.start.benchFrq     = 4000;
    in.start.runFlagAll   = ACCDEC_BITS(ACC_SPEED);
    in.start.bFrqCurAimChg = 1;
    g_carry = in.start;
    run_tick2(&in, "prime-sca");

    /* SCurveSpecialCalc: tickerAll2 = 1000*500/4000 = 125, tickerAll1 = 52. */
    in_reset(&in);
    in.which     = W_SCB;
    in.frq0in    = 0;
    in.accDecTime = 1000;
    in.frqCurAim = 3000;
    in.ratingFrq = 500;
    in.sCurveStartPhaseTime = 500;
    in.sCurveEndPhaseTime   = 500;
    in.start.benchFrq     = 4000;
    in.start.runFlagAll   = ACCDEC_BITS(ACC_SPEED);
    in.start.bFrqCurAimChg = 1;
    g_carry = in.start;
    run_tick2(&in, "prime-scb");
}

/* ------------------------------------------------------------------------- */
/* 相 1: LineChangeCalc 直调                                                   */
/* ------------------------------------------------------------------------- */

static void phase_line(void)
{
    static const int32 aims[]  = {0, 1, -1, 999, -3000, 60000, -60000};
    static const int32 curs[]  = {0, 1, -1, 1000, -1000, 30000};
    static const int32 maxs[]  = {0, 1, 3000, 65535, -3000};
    static const Uint32 ticks[] = {0, 1, 2, 7, 100, 0xFFFFFFFFUL};
    static const int32 rems[]  = {0, 1, 6, 99, -99, 3000};
    unsigned i, j, k, m, n;
    struct IN in;

    for (i = 0; i < sizeof(aims) / sizeof(aims[0]); i++)
    for (j = 0; j < sizeof(curs) / sizeof(curs[0]); j++)
    for (k = 0; k < sizeof(maxs) / sizeof(maxs[0]); k++)
    for (m = 0; m < sizeof(ticks) / sizeof(ticks[0]); m++)
    for (n = 0; n < sizeof(rems) / sizeof(rems[0]); n++)
    {
        in_reset(&in);
        in.which = W_LINE;

        in.line.calc      = 0;
        in.line.maxValue  = maxs[k];
        in.line.aimValue  = aims[i];
        in.line.curValue  = curs[j];
        in.line.tickerAll = ticks[m];
        in.line.remainder = rems[n];

        g_carry = in.start;
        run_tick2(&in, "line");
    }
}

/* ------------------------------------------------------------------------- */
/* 相 2: AccDecFrqCalc 的守卫链 (6 个 && 条件)                                 */
/* ------------------------------------------------------------------------- */

/* 守卫链: ((ovGain)||(ocGain)) && motorCtrlMode==VF && motorType!=PMSM
 *         && !droopCtrl && runStatus!=TUNE
 *         && !((frqCurAim==0) && (|frqRun| <= 50))
 * 成立时 frq0 = frqRun, 不成立时 frq0 = frqTmp 且 frqVFRunRemainder 被清 0。
 * 起始镜像里把 frqVFRunRemainder 放一个非零值, 于是"守卫走哪条路"在镜像里
 * **直接可见** —— 不用猜。 */

static void guard_base(struct IN *in)
{
    in_reset(in);
    in->which           = W_ACCDEC;
    in->ovGain          = 1;
    in->ocGain          = 0;
    in->motorCtrlMode   = FUNCCODE_motorCtrlMode_VF;
    in->motorType       = MOTOR_TYPE_ACI_GENERAL;
    in->droopCtrl       = 0;
    in->runStatusE      = RUN_STATUS_NORMAL;
    in->frqCurAim       = 3000;
    in->frqRun          = 1000;

    in->accTime = 100;
    in->decTime = 100;
    in->mode    = FUNCCODE_accDecSpdCurve_LINE;

    in->start.frqTmp            = 1200;   /* != frqRun, 两条路结果不同 */
    in->start.frqVFRunRemainder = 0x1234;
    in->start.benchFrq          = 4000;
    in->start.frqCurAimOld      = 3000;   /* == frqCurAim, 不触发 bFrqCurAimChg */
    in->start.accTimeOld        = 100;
    in->start.decTimeOld        = 100;
    in->start.runFlagAll        = 0;
}

static void phase_guard(void)
{
    static const Uint16 runSt[] = {
        RUN_STATUS_WAIT, RUN_STATUS_ZERO, RUN_STATUS_START, RUN_STATUS_NORMAL,
        RUN_STATUS_STOP, RUN_STATUS_JOG, RUN_STATUS_POS_CTRL, RUN_STATUS_TUNE,
        RUN_STATUS_DI_BRAKE_DEC, RUN_STATUS_DI_BRAKE, RUN_STATUS_LOSE_LOAD,
        RUN_STATUS_SHUT_DOWN
    };
    static const int32 aims[] = {0, 3000, -3000, 50, -50};
    static const int32 runs[] = {0, 50, 51, -50, -51, 1000};
    struct IN in;
    unsigned i, j, k, m;
    int ov, oc, cm, mt, dr;

    /* 2a. 一次只动一个因子, 其余保持在"守卫成立"的位置。 */
    for (i = 0; i < 3; i++)
    {
        guard_base(&in); in.ovGain = (Uint16)i; g_carry = in.start; run_tick2(&in, "guard-ovGain");
    }
    for (i = 0; i < 2; i++)
    {
        guard_base(&in); in.ocGain = (Uint16)i; g_carry = in.start; run_tick2(&in, "guard-ocGain");
    }
    {
        static const Uint16 cmv[] = {FUNCCODE_motorCtrlMode_SVC,
                                     FUNCCODE_motorCtrlMode_FVC,
                                     FUNCCODE_motorCtrlMode_VF};
        for (i = 0; i < 3; i++)
        {
            guard_base(&in); in.motorCtrlMode = cmv[i];
            g_carry = in.start; run_tick2(&in, "guard-motorCtrlMode");
        }
    }
    {
        static const Uint16 mtv[] = {MOTOR_TYPE_ACI_GENERAL, MOTOR_TYPE_ACI_INV,
                                     MOTOR_TYPE_PMSM};
        for (i = 0; i < 3; i++)
        {
            guard_base(&in); in.motorType = mtv[i];
            g_carry = in.start; run_tick2(&in, "guard-motorType");
        }
    }
    for (i = 0; i < 2; i++)
    {
        guard_base(&in); in.droopCtrl = (Uint16)i;
        g_carry = in.start; run_tick2(&in, "guard-droopCtrl");
    }
    for (i = 0; i < sizeof(runSt) / sizeof(runSt[0]); i++)
    {
        guard_base(&in); in.runStatusE = runSt[i];
        g_carry = in.start; run_tick2(&in, "guard-runStatus");
    }
    for (i = 0; i < sizeof(aims) / sizeof(aims[0]); i++)
    for (j = 0; j < sizeof(runs) / sizeof(runs[0]); j++)
    {
        guard_base(&in);
        in.frqCurAim = aims[i];
        in.frqRun    = runs[j];
        g_carry = in.start; run_tick2(&in, "guard-frqPair");
    }

    /* 2b. 小因子的全交叉。 */
    for (ov = 0; ov < 2; ov++)
    for (oc = 0; oc < 2; oc++)
    for (cm = 0; cm < 3; cm++)
    for (mt = 0; mt < 3; mt++)
    for (dr = 0; dr < 2; dr++)
    for (i = 0; i < 4; i++)
    for (j = 0; j < 6; j++)
    {
        static const Uint16 rs[4] = {RUN_STATUS_NORMAL, RUN_STATUS_START,
                                     RUN_STATUS_TUNE, RUN_STATUS_STOP};
        static const Uint16 cmv[3] = {FUNCCODE_motorCtrlMode_SVC,
                                      FUNCCODE_motorCtrlMode_FVC,
                                      FUNCCODE_motorCtrlMode_VF};
        static const Uint16 mtv[3] = {MOTOR_TYPE_ACI_GENERAL,
                                      MOTOR_TYPE_ACI_INV, MOTOR_TYPE_PMSM};
        guard_base(&in);
        in.ovGain        = (Uint16)ov;
        in.ocGain        = (Uint16)oc;
        in.motorCtrlMode = cmv[cm];
        in.motorType     = mtv[mt];
        in.droopCtrl     = (Uint16)dr;
        in.runStatusE    = rs[i];
        in.frqCurAim     = aims[j % 5];
        in.frqRun        = runs[j];
        g_carry = in.start; run_tick2(&in, "guard-cross");
    }
}

/* 2c. 守卫**成立**那条路 (frq0 = frqRun) 再往下走。
 *
 * 2a/2b 用的都是 guard_base 的 mode = LINE, 所以 "frq0 来自 frqRun" 与
 * "进 S 曲线" 这个组合一次都没出现过。这里把它补上: 守卫钉死成立, 只变
 * frqRun / frqCurAim / mode / accDecBenchmark / timeBench / accTime。
 *
 * 覆盖计数里 GUARD_FRQ_RUN 从 420 涨到上万, 就是这一相加出来的。 */
static void phase_guard_run(void)
{
    static const Uint16 modes[]  = {FUNCCODE_accDecSpdCurve_LINE,
                                    FUNCCODE_accDecSpdCurve_S_CURVE_A,
                                    FUNCCODE_accDecSpdCurve_S_CURVE_B};
    static const Uint16 bms[]    = {0, 1, 2};
    static const Uint16 tbs[]    = {2, 10};
    static const int32  ats[]    = {0, 1, 10, 100};
    static const int32  runs[]   = {0, 50, 51, 1000, -1000, 3000};
    static const int32  aims[]   = {3000, -3000, 500};
    unsigned m, b, t, a, r, g;
    struct IN in;

    for (m = 0; m < 3; m++)
    for (b = 0; b < 3; b++)
    for (t = 0; t < 2; t++)
    for (a = 0; a < 4; a++)
    for (r = 0; r < 6; r++)
    for (g = 0; g < 3; g++)
    {
        guard_base(&in);
        in.mode             = modes[m];
        in.accDecBenchmark  = bms[b];
        in.timeBench        = tbs[t];
        in.accTime          = ats[a];
        in.decTime          = ats[a];
        in.frqRun           = runs[r];
        in.frqCurAim        = aims[g];
        in.ratingFrq        = 500;
        in.maxFrq           = 5000;
        in.frqPoint         = 2;

        in.start.frqTmp     = 1200;
        in.start.benchFrq   = 4000;
        in.start.accTimeOld = ats[a];
        in.start.decTimeOld = ats[a];
        in.start.frqCurAimOld = aims[g];
        in.start.frqVFRunRemainder = 0x5A5A;

        g_carry = in.start;
        run_tick2(&in, "guard-run");
    }
}

/* ------------------------------------------------------------------------- */
/* 相 3: 曲线模式 / accDecBenchmark / 降级判据                                 */
/* ------------------------------------------------------------------------- */

static void phase_mode(void)
{
    static const Uint16 modes[]   = {FUNCCODE_accDecSpdCurve_LINE,
                                     FUNCCODE_accDecSpdCurve_S_CURVE_A,
                                     FUNCCODE_accDecSpdCurve_S_CURVE_B,
                                     3};                 /* 越界值: 落到 else 分支 */
    static const Uint16 benchs[]  = {0, 1, 2, 3};
    static const Uint16 tb[]      = {1, 2, 3, 5, 10, 100};
    static const int32  at[]      = {0, 1, 2, 10, 100, 1000};
    /* (frq0, frqCurAim): ACC / DEC / 零起点 / 反向 / 跨零 */
    static const int32  p0[]      = {0, 1000, -1000, 3000, -3000, 1000};
    static const int32  aim[]     = {3000, 3000, 3000, -3000, 1000, -3000};
    unsigned i, j, k, m, n;
    struct IN in;

    for (i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
    for (j = 0; j < sizeof(benchs) / sizeof(benchs[0]); j++)
    for (k = 0; k < 5; k++)              /* frqPoint 0..4 */
    for (m = 0; m < sizeof(tb) / sizeof(tb[0]); m++)
    for (n = 0; n < sizeof(at) / sizeof(at[0]); n++)
    {
        unsigned q;

        for (q = 0; q < sizeof(p0) / sizeof(p0[0]); q++)
        {
            in_reset(&in);
            in.which = W_ACCDEC;

            in.mode             = modes[i];
            in.accDecBenchmark  = benchs[j];
            in.frqPoint         = (Uint16)k;
            in.timeBench        = tb[m];
            in.accTime          = at[n];
            in.decTime          = at[n];

            in.frqCurAim  = aim[q];
            in.frqRun     = p0[q];
            in.maxFrq     = 5000;
            in.ratingFrq  = 500;

            in.start.frqTmp     = p0[q];
            in.start.benchFrq   = 4000;
            in.start.accTimeOld = at[n];
            in.start.decTimeOld = at[n];
            in.start.frqCurAimOld = aim[q];
            in.start.runFlagAll = 0;

            g_carry = in.start;
            run_tick2(&in, "mode");
        }
    }

    /* 3b. 降级判据的两条边: accDecTime > 100*TIME_UNIT_MS_PER_SEC/timeBench,
     * 以及 |frqCurAim| > 6*ratingFrq。两条都只把 S_CURVE_B 降成 LINE, 所以
     * 必须用 S_CURVE_B 起手才能观察到。 */
    {
        static const Uint16 tb2[] = {1, 2, 5, 10};
        static const int32  at2[] = {0, 1, 2, 3, 100, 1000, 5000, 60000};
        static const int32  aim2[] = {500, 3000, 30000, -30000};
        static const Uint16 rf2[] = {1, 10, 500, 5000};
        unsigned a, b, c, d;

        for (a = 0; a < sizeof(tb2) / sizeof(tb2[0]); a++)
        for (b = 0; b < sizeof(at2) / sizeof(at2[0]); b++)
        for (c = 0; c < sizeof(aim2) / sizeof(aim2[0]); c++)
        for (d = 0; d < sizeof(rf2) / sizeof(rf2[0]); d++)
        {
            in_reset(&in);
            in.which = W_ACCDEC;
            in.mode = FUNCCODE_accDecSpdCurve_S_CURVE_B;
            in.timeBench = tb2[a];
            in.accTime = at2[b];
            in.decTime = at2[b];
            in.frqCurAim = aim2[c];
            in.frqRun = 0;
            in.ratingFrq = rf2[d];
            in.maxFrq = 5000;

            in.start.frqTmp = 0;
            in.start.benchFrq = 4000;
            in.start.accTimeOld = at2[b];
            in.start.decTimeOld = at2[b];
            in.start.frqCurAimOld = aim2[c];

            g_carry = in.start;
            run_tick2(&in, "downgrade");
        }
    }
}

/* ------------------------------------------------------------------------- */
/* 相 4/5: 两个 S 曲线状态机的直调序列                                          */
/* ------------------------------------------------------------------------- */

/* 一个序列: 第一拍 bFrqCurAimChg = 1 (重新初始化 static), 之后 0, 走 n 拍。
 * 每拍都过 run_tick2, 所以每一拍都在比对。 */
static void scurve_seq(int which, const struct IN *in, int n, const char *tag)
{
    struct IN sIn2;
    int i;

    g_carry = in->start;
    g_carry.bFrqCurAimChg = 1;

    sIn2 = *in;
    sIn2.which = which;

    for (i = 0; i < n; i++)
    {
        run_tick2(&sIn2, tag);
        g_carry.bFrqCurAimChg = 0;      /* 后面几拍不再重新初始化 */
    }
}

static void phase_scurve_a(void)
{
    static const Uint16 starts[] = {1, 10, 100, 500, 999};
    static const Uint16 ends[]   = {1, 10, 100, 500, 999};
    static const int32  adts[]   = {1, 2, 10, 100, 1000};
    static const Uint16 bfrs[]   = {1, 100, 4000, 30000};
    /* (frq0, aim): 上坡 / 下坡 / 跨零 / 负到正 */
    static const int32  p0[]     = {0, 1000, 3000, -3000, 1000};
    static const int32  aims[]   = {3000, 3000, -3000, 3000, -3000};
    static const Uint16 accs[]   = {ACC_SPEED, DEC_SPEED};
    unsigned a, b, c, d, e, f;
    struct IN in;

    for (f = 0; f < sizeof(accs) / sizeof(accs[0]); f++)
    for (a = 0; a < sizeof(starts) / sizeof(starts[0]); a++)
    for (b = 0; b < sizeof(ends) / sizeof(ends[0]); b++)
    for (c = 0; c < sizeof(adts) / sizeof(adts[0]); c++)
    for (d = 0; d < sizeof(bfrs) / sizeof(bfrs[0]); d++)
    for (e = 0; e < sizeof(p0) / sizeof(p0[0]); e++)
    {
        if (starts[a] + ends[b] == 2000)
        {
            continue;                    /* lineTime == 0 -> 除零 */
        }
        in_reset(&in);
        in.frqCurAim      = aims[e];
        in.ratingFrq      = 500;
        in.sCurveStartPhaseTime = starts[a];
        in.sCurveEndPhaseTime   = ends[b];
        in.frq0in         = p0[e];
        in.accDecTime     = adts[c];
        in.start.benchFrq = bfrs[d];
        in.start.runFlagAll = ACCDEC_BITS(accs[f]);
        in.start.frqTmpFrac = 0;

        scurve_seq(W_SCA, &in, 40, "scurveA");
    }
}

static void phase_scurve_b(void)
{
    static const Uint16 starts[] = {1, 10, 100, 500};
    static const Uint16 ends[]   = {1, 10, 100, 500};
    static const int32  adts[]   = {1, 2, 10, 100, 1000};
    static const Uint16 bfrs[]   = {1, 100, 4000, 30000};
    /* 入口状态三分支由 |frq0| 相对 |pointFrq| / |inflexionFrq| 决定:
     * ratingFrq=500 时 inflexionFrq=500, pointFrq=500*11483>>15=175。
     * 所以 frq0 取 0 / 100 / 300 / 500 / 1000 / -100 / -300 / -1000 覆盖三支。 */
    static const int32  p0[]   = {0, 100, 300, 500, 1000, -100, -300, -1000};
    static const int32  aims[] = {3000, 3000, 3000, 3000, 3000, -3000, -3000, -3000};
    static const Uint16 accs[] = {ACC_SPEED, DEC_SPEED};
    unsigned a, b, c, d, e, f;
    struct IN in;

    for (f = 0; f < sizeof(accs) / sizeof(accs[0]); f++)
    for (a = 0; a < sizeof(starts) / sizeof(starts[0]); a++)
    for (b = 0; b < sizeof(ends) / sizeof(ends[0]); b++)
    for (c = 0; c < sizeof(adts) / sizeof(adts[0]); c++)
    for (d = 0; d < sizeof(bfrs) / sizeof(bfrs[0]); d++)
    for (e = 0; e < sizeof(p0) / sizeof(p0[0]); e++)
    {
        if (starts[a] + ends[b] == 2000)
        {
            continue;
        }
        in_reset(&in);
        in.frqCurAim    = aims[e];
        in.ratingFrq    = 500;
        in.sCurveStartPhaseTime = starts[a];
        in.sCurveEndPhaseTime   = ends[b];
        in.frq0in       = p0[e];
        in.accDecTime   = adts[c];
        in.start.benchFrq   = bfrs[d];
        in.start.runFlagAll = ACCDEC_BITS(accs[f]);

        scurve_seq(W_SCB, &in, 60, "scurveB");
    }

    /* 5b. 反向 / 跨零的短序列: 先加速到正, 再反向。用 AccDecFrqCalc 闭环走,
     * 因为直接调 SCurveSpecialCalc 无法改变 frqCurAim 而不重开状态机。 */
    {
        static const Uint16 modes[] = {FUNCCODE_accDecSpdCurve_S_CURVE_A,
                                       FUNCCODE_accDecSpdCurve_S_CURVE_B,
                                       FUNCCODE_accDecSpdCurve_LINE};
        unsigned m;
        for (m = 0; m < 3; m++)
        {
            int t;
            in_reset(&in);
            in.which = W_ACCDEC;
            in.mode = modes[m];
            in.accTime = 50;
            in.decTime = 50;
            in.timeBench = 2;
            in.maxFrq = 5000;
            in.ratingFrq = 500;
            in.frqCurAim = 3000;
            in.start.frqTmp = 0;
            in.start.frqCurAimOld = 0;
            in.start.accTimeOld = 50;
            in.start.decTimeOld = 50;
            in.start.benchFrq = 4000;

            g_carry = in.start;
            for (t = 0; t < 400; t++)
            {
                /* 0..99 爬到 3000, 100..199 停, 200..299 掉到 -3000, 之后停 */
                if (t < 100)      in.frqCurAim = 3000;
                else if (t < 200) in.frqCurAim = 3000;
                else if (t < 300) in.frqCurAim = -3000;
                else              in.frqCurAim = -3000;

                run_tick2(&in, "ramp");
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* 相 6: AccDecFrqCalc 闭环 (模块自己的输出就是下一拍的输入)                     */
/* ------------------------------------------------------------------------- */

static void phase_closed_loop(void)
{
    static const Uint16 modes[] = {FUNCCODE_accDecSpdCurve_LINE,
                                   FUNCCODE_accDecSpdCurve_S_CURVE_A,
                                   FUNCCODE_accDecSpdCurve_S_CURVE_B};
    static const int32  ats[]   = {1, 5, 20, 100, 500};
    static const Uint16 bms[]   = {0, 1, 2};
    static const int32  targs[] = {500, 3000, 12000, -3000, 0};
    unsigned m, a, b, c;
    struct IN in;

    for (m = 0; m < 3; m++)
    for (a = 0; a < sizeof(ats) / sizeof(ats[0]); a++)
    for (b = 0; b < sizeof(bms) / sizeof(bms[0]); b++)
    for (c = 0; c < sizeof(targs) / sizeof(targs[0]); c++)
    {
        int t;
        int32 tgt = targs[c];

        in_reset(&in);
        in.which = W_ACCDEC;
        in.mode = modes[m];
        in.accTime = ats[a];
        in.decTime = ats[a];
        in.timeBench = 2;
        in.maxFrq = 5000;
        in.ratingFrq = 500;
        in.accDecBenchmark = bms[b];
        in.frqPoint = 2;
        in.frqCurAim = 0;

        in.start.frqTmp = 0;
        in.start.frqCurAimOld = 0;
        in.start.accTimeOld = ats[a];
        in.start.decTimeOld = ats[a];
        in.start.benchFrq = 4000;
        in.start.runFlagAll = 0;

        g_carry = in.start;

        for (t = 0; t < 300; t++)
        {
            /* 每 60 拍换一个目标, 让加速段/匀速段/减速段都出现 */
            if (t == 0)        in.frqCurAim = tgt;
            else if (t == 60)  in.frqCurAim = -tgt;
            else if (t == 120) in.frqCurAim = 0;
            else if (t == 180) in.frqCurAim = tgt;
            else if (t == 240) in.frqCurAim = tgt / 2;

            run_tick2(&in, "closed");
        }
    }
}

/* ------------------------------------------------------------------------- */
/* 相 7: 随机                                                              */
/* ------------------------------------------------------------------------- */

static unsigned long g_rng = 20261019UL;

static unsigned long rnd(void)
{
    g_rng = g_rng * 1103515245UL + 12345UL;
    return (g_rng >> 8) & 0xFFFFFFUL;
}

static int32 pick32(void)
{
    static const int32 pool[] = {
        0, 1, -1, 2, -2, 50, -50, 51, -51, 100, -100, 175, -175, 300, -300,
        500, -500, 999, -999, 1000, -1000, 3000, -3000, 6000, -6000,
        12000, -12000, 30000, -30000, 32767, -32768, 60000, -60000,
        65535, -65535, 65536, 131072, 1000000, -1000000, 0x7FFFFFFFL,
        (int32)0x80000000UL
    };
    return pool[rnd() % (sizeof(pool) / sizeof(pool[0]))];
}

static Uint16 pick16(void)
{
    static const Uint16 pool[] = {
        0, 1, 2, 3, 4, 5, 10, 50, 51, 100, 175, 200, 255, 256, 500, 999,
        1000, 1999, 2000, 2001, 4000, 5000, 16384, 32767, 32768, 65534, 65535
    };
    return pool[rnd() % (sizeof(pool) / sizeof(pool[0]))];
}

static void phase_random(long n)
{
    static const Uint16 modes[] = {FUNCCODE_accDecSpdCurve_LINE,
                                   FUNCCODE_accDecSpdCurve_S_CURVE_A,
                                   FUNCCODE_accDecSpdCurve_S_CURVE_B, 3};
    long i;
    struct IN in;

    for (i = 0; i < n; i++)
    {
        in_reset(&in);

        in.which = (int)(rnd() % 4);
        in.mode  = modes[rnd() % 4];

        in.accTime = pick32();
        in.decTime = pick32();
        if (in.accTime < 0) in.accTime = -in.accTime;
        if (in.decTime < 0) in.decTime = -in.decTime;
        in.accTime &= 0xFFFF;
        in.decTime &= 0xFFFF;

        in.frqCurAim     = pick32();
        in.frqCurAimFrac = pick32();
        in.frqRun        = pick32();
        in.maxFrq        = pick16();
        in.timeBench     = pick16();
        if (in.timeBench == 0) in.timeBench = 1;
        in.ratingFrq     = pick16();
        if (in.ratingFrq == 0) in.ratingFrq = 1;

        in.ovGain          = (Uint16)(rnd() % 2);
        in.ocGain          = (Uint16)(rnd() % 2);
        in.droopCtrl       = (Uint16)(rnd() % 2);
        in.accDecBenchmark = (Uint16)(rnd() % 4);
        in.frqPoint        = (Uint16)(rnd() % 5);
        in.motorCtrlMode   = (Uint16)(rnd() % 3);
        in.motorType       = (Uint16)(rnd() % 3);
        in.runStatusE      = (Uint16)(rnd() % 12);

        in.sCurveStartPhaseTime = pick16();
        in.sCurveEndPhaseTime   = pick16();
        if (in.sCurveStartPhaseTime == 0) in.sCurveStartPhaseTime = 1;
        if (in.sCurveEndPhaseTime == 0)   in.sCurveEndPhaseTime = 1;
        if ((((Uint32)in.sCurveStartPhaseTime + in.sCurveEndPhaseTime)
             & 0xFFFFUL) == 2000UL)
        {
            in.sCurveEndPhaseTime = 1999;
        }

        in.frq0in     = pick32();
        in.accDecTime = pick32();
        if (in.accDecTime < 0) in.accDecTime = 0;

        in.start.frqCurAimOld = pick32();
        in.start.accTimeOld   = pick32();
        in.start.decTimeOld   = pick32();
        in.start.bFrqCurAimChg = (Uint16)(rnd() % 2);
        in.start.frqTmp       = pick32();
        in.start.frqTmpFrac   = pick32();
        in.start.frqVFRunRemainder = pick32() | 1;
        in.start.benchFrq     = pick16();
        in.start.runFlagAll   = ACCDEC_BITS((Uint16)(rnd() % 3));
        in.start.maxValue     = pick32();
        in.start.aimValue     = pick32();
        in.start.curValue     = pick32();
        in.start.tickerAll    = (Uint32)rnd() * 7919UL;
        in.start.remainder    = pick32();
        in.start.calcKind     = (Uint16)(rnd() % 3);

        in.line.calc      = 0;
        in.line.maxValue  = pick32();
        in.line.aimValue  = pick32();
        in.line.curValue  = pick32();
        in.line.tickerAll = (Uint32)rnd() * 7919UL;
        in.line.remainder = pick32();

        if (in.which == W_ACCDEC)
        {
        }
        if (in.which == W_SCA)
        {
        }
        if (in.which == W_SCB)
        {
        }

        g_carry = in.start;
        run_tick2(&in, "random");
    }
}

/* ------------------------------------------------------------------------- */
/* 主程序                                                                     */
/* ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long nrand = 20000;

    if (argc > 1)
    {
        nrand = atol(argv[1]);
    }

    /* 不缓冲: 崩在中途时 stdout 不能跟着一起丢 (第一次跑就吃过这个亏 ——
     * 除零终止, 落盘的 stdout 是 0 字节, 完全看不出崩在哪一相)。 */
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("f_runSrc_accDecFrq differential bench\n");

    prime();

    printf("phase phase_line\n");
    phase_line();
    printf("phase phase_guard\n");
    phase_guard();
    phase_guard_run();

    printf("phase phase_mode\n");
    phase_mode();
    printf("phase phase_scurve_a\n");
    phase_scurve_a();
    printf("phase phase_scurve_b\n");
    phase_scurve_b();
    printf("phase phase_closed_loop\n");
    phase_closed_loop();
    printf("phase random %ld\n", nrand);
    phase_random(nrand);

    printf("---- coverage ----\n");
    {
        int i;
        for (i = 0; i < COV_COUNT; i++)
        {
            printf("  %-18s %ld\n", g_covName[i], g_cov[i]);
        }
    }

    printf("skipped=%ld  (除数安全判据排除的拍数)\n", g_skipped);
    printf("cases=%ld mismatches=%ld\n", g_cases, g_mism);
    return (g_mism == 0) ? 0 : 1;
}
