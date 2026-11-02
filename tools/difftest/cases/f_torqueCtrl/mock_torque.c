/* f_torqueCtrl 差分测试的共享 mock。
 *
 * 一份实例, ref 与 new 两侧都读它, 所以两侧看到的是**完全相同**的输入。
 * 被测模块自己导出的符号 (upperTorque / torqueAim / torqurLine) 由
 * ref_tu.c / new_tu.c 各自复制一份 —— 那才是比对的对象。
 *
 * include 顺序与 src/function/f_torqueCtrl.c 一致。
 */
#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_io.h"
#include "f_main.h"
#include "f_p2p.h"

#include "mock_torque.h"

/* ---------------------------------------------------------------------------
 * 模块读到的全部外部数据
 *
 * 这张清单不是"猜"出来的: 把交付源码的标识符逐个拿去 include/ 里查 extern 声明,
 * 再让链接器把漏掉的一个个报出来。每一条都给出定义它的头文件, 便于以后核对。
 *
 * 类型用**真类型**而不是替身。原因和 f_plc 一样: 本模块的关键运算都建立在
 * "这个字段是 16 位" 的前提上 (例如 (int32)(int16)p2pData.processValue),
 * 替身类型会改变截断点, 测试就变成在验证另一份代码。
 * ------------------------------------------------------------------------ */

FUNCCODE_ALL funcCode;              /* f_funcCode.h:1521  driveUpperTorqueSrc /
                                                          driveUpperTorque /
                                                          plcEnable / plcTorqueSet /
                                                          p2pRevDataSel / frqComm /
                                                          torqueCtrlAccTime /
                                                          torqueCtrlDecTime */
struct MOTOR_FC motorFc;            /* f_main.h:69        vcPara.spdCtrlDriveTorqueLimitSrc /
                                                          vcPara.spdCtrlDriveTorqueLimit */
struct AI_DEAL aiDeal[AI_NUMBER];   /* f_io.h:453         .set, AI_NUMBER = 3 */
int16 pulseInSet;                   /* f_io.h:441 */
Uint16 runMode;                     /* f_frqSrc.h:112 */
union RUN_FLAG runFlag;             /* f_runSrc.h:91      .bit.run */
Uint16 itDisp;                      /* f_menu.h:136 */
P2P_DATA_STRUCT p2pData;            /* f_p2p.h:30         .processValue */
Uint16 CanRxTxCon;                  /* f_canlink.h:98, 经 f_main.h 拉进来 */
int32 frq;                          /* f_frqSrc.h:71 */
int32 frqRun;                       /* f_interface.h:34 */

/* ---- LineChangeCalc 调用日志 -------------------------------------------- */

struct TORQUECALL torqueLog[TORQUELOG_N];
Uint16 torqueLogN;

/* ---------------------------------------------------------------------------
 * LineChangeCalc
 *
 * 这不是被测模块, 是它依赖的外部函数 (定义在 src/function/f_runSrc_accDecFrq.c:586,
 * 属于 f_runSrc 家族, 不在本次改写范围内)。
 *
 * 这里**逐字照抄**原实现, 而不是给个替身值。理由是 upperTorque 的最终值完全由
 * 这个函数决定 —— 换个"差不多"的替身, 等于把被测模块的输出换掉了, 差分测试就
 * 失去了意义。照抄外部依赖在差分测试里是正当的: 两侧跑的是同一份外部实现,
 * 差异只可能来自被测模块。
 *
 * 唯一在照抄之外加的是一段入参快照 (见 mock_torque.h 的说明)。
 * ------------------------------------------------------------------------ */
void LineChangeCalc(LINE_CHANGE_STRUCT *p)
{
    int32 delta;

    if (torqueLogN < TORQUELOG_N)
    {
        struct TORQUECALL *e = &torqueLog[torqueLogN++];

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
