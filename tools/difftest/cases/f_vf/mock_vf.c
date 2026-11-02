/* f_vf 差分测试的共享 mock。
 *
 * 一份实例, ref 与 new 两侧都读它, 所以两侧看到的是**完全相同**的输入。
 * 被测模块自己导出的符号 (vfSeparateDeal / vfSeprateVolAim / vfSeparateVol /
 * vfSeprateVolAccLine) 由 ref_tu.c / new_tu.c 各自复制一份 —— 那才是比对的对象。
 *
 * include 顺序与 src/function/f_vf.c 一致。
 */
#include "f_main.h"
#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_io.h"

#include "mock_vf.h"

/* ---------------------------------------------------------------------------
 * 模块读到的全部外部数据
 *
 * 这张清单不是"猜"出来的: 把交付源码的标识符逐个拿去 include/ 里查 extern 声明,
 * 再让链接器把漏掉的一个个报出来。每一条都给出定义它的头文件, 便于以后核对。
 *
 * 类型用**真类型**而不是替身。原因和 f_torqueCtrl 一样: 本模块的关键运算都建立在
 * "这个字段是 16 位" 的前提上 (例如 (int32)(int16)funcCode.code.frqComm 的符号
 * 扩展、ABS_INT16((int16)vfSeprateVolAim) 的截断), 替身类型会改变截断点,
 * 测试就变成在验证另一份代码。
 * ------------------------------------------------------------------------ */

FUNCCODE_ALL funcCode;              /* f_funcCode.h:1521  code.vfCurve /
                                                          code.vfVoltageSrc /
                                                          code.vfVoltageDigtalSet /
                                                          code.vfVoltageAccTime /
                                                          code.frqComm /
                                                          code.motorParaM1.elem.ratingVoltage */
struct MOTOR_FC motorFc;            /* f_main.h:69        motorCtrlMode /
                                                          motorPara.elem.ratingVoltage /
                                                          motorPara.elem.ratingFrq */
struct AI_DEAL aiDeal[AI_NUMBER];   /* f_io.h:453         .set, AI_NUMBER = 3 */
int16 pulseInSet;                   /* f_io.h:441 */
struct DI_FUNC diFunc;              /* f_io.h:433         f1.bit.multiSet */
Uint16 maxFrq;                      /* f_frqSrc.h:85 */
int32 frq;                          /* f_frqSrc.h:71 */
union RUN_FLAG runFlag;             /* f_runSrc.h:91      bit.run */

/* ---- 三个值源桩的驱动值 (见 mock_vf.h 的说明) ---------------------------- */

int32 mockMultiSetFrq;
int32 mockPlcFrq;
int32 mockPidFrq;

/* 调用轨迹 (见 mock_vf.h)。三个桩各记一条, 含实参。 */
struct VFTRACE vfTrace[VFTRACE_N];
Uint16 vfTraceN;

static void trace(Uint16 which, Uint16 arg)
{
    if (vfTraceN < VFTRACE_N)
    {
        vfTrace[vfTraceN].which = which;
        vfTrace[vfTraceN].arg   = arg;
        vfTraceN++;
    }
}

int32 UpdateMultiSetFrq(Uint16 step)
{
    trace(VFTRACE_MULTISET, step);
    return mockMultiSetFrq;
}

int32 FrqPlcSetDeal(void)
{
    trace(VFTRACE_PLC, 0);
    return mockPlcFrq;
}

int32 FrqPidSetDeal(void)
{
    trace(VFTRACE_PID, 0);
    return mockPidFrq;
}

/* ---- LineChangeCalc 调用日志 -------------------------------------------- */

struct VFCALL vfLog[VFLOG_N];
Uint16 vfLogN;

/* ---------------------------------------------------------------------------
 * LineChangeCalc
 *
 * 这不是被测模块, 是它依赖的外部函数 (定义在 src/function/f_runSrc_accDecFrq.c:586,
 * 属于 f_runSrc 家族, 不在本次改写范围内)。
 *
 * 这里**逐字照抄**原实现, 而不是给个替身值。理由是 vfSeparateVol 的最终值完全由
 * 这个函数决定 —— 它是模块自己的斜坡滤波器, 换个"差不多"的替身, 等于把被测模块
 * 的输出换掉了, 差分测试就失去了意义。照抄外部依赖在差分测试里是正当的: 两侧跑的
 * 是同一份外部实现, 差异只可能来自被测模块。
 *
 * 唯一在照抄之外加的是一段入参快照 (见 mock_vf.h 的说明)。
 * ------------------------------------------------------------------------ */
void LineChangeCalc(LINE_CHANGE_STRUCT *p)
{
    int32 delta;

    if (vfLogN < VFLOG_N)
    {
        struct VFCALL *e = &vfLog[vfLogN++];

        e->aimValue   = p->aimValue;
        e->curValue   = p->curValue;
        e->maxValue   = p->maxValue;
        e->tickerAll  = p->tickerAll;
        e->remainder  = p->remainder;
    }

    if (p->curValue != p->aimValue)
    {
        if (!p->tickerAll)
        {
            p->curValue = p->aimValue;
        }
        else
        {
            delta = ((int32)p->maxValue + p->remainder) / p->tickerAll;
            p->remainder = ((int32)p->maxValue + p->remainder) % p->tickerAll;

            if (p->aimValue > p->curValue)
            {
                p->curValue += delta;
                if (p->curValue > p->aimValue)
                    p->curValue = p->aimValue;
            }
            else
            {
                p->curValue -= delta;
                if (p->curValue < p->aimValue)
                    p->curValue = p->aimValue;
            }
        }
    }
}
