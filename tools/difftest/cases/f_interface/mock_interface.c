/* f_interface 差分测试的共享 mock —— 两侧 (ref / new) 看到同一份状态。

 * 这里定义的是 f_interface.c **引用但不拥有**的东西: 它们由别的模块定义
 * (f_io.c / f_menu.c / f_runSrc.c / f_funcCode.c ...), 在本用例里不参与编译,
 * 所以由本文件提供唯一一份。两侧共享同一份, 于是"改写有没有多读一个字段 /
 * 少写一个字段"是可观测的。

 * 清单不是人眼数的, 是 tools/difftest/cases/f_interface/README.md 里记的那条
 * `nm2000 --undefined-only build/Debug/f_interface.obj` 读出来的 (34 个外部引用),
 * 逐个对照本文件, 一个不多一个不少。
 */
#include <string.h>

#include "mock_interface.h"

/* ---- 被 f_interface.c 引用的 34 个外部量 ---------------------------------- */

volatile struct EQEP_REGS EQep1Regs;

struct AI_DEAL  aiDeal[AI_NUMBER];

Uint16 bUv;
Uint16 currentOcDisp;

union DSP_MAIN_COMMAND  dspMainCmd;
union DSP_MAIN_COMMAND1 dspMainCmd1;
union DSP_STATUS        dspStatus;
union DSP_SUB_COMMAND   dspSubCmd;

int32  frq;
int32  frqCurAim;
int32  frqCurAimFrac;
int32  frqDroop;
Uint16 frqPuQ15;
Uint16 frqRunDisp;
int32  frqTmp;

FUNCCODE_ALL funcCode;

/* 容量见 shim/mock_interface.h。 */
int16 gSendToFunctionDataBuff[IFACE_FUNC_2MS_NUM];
int16 gSendToMotor05MsDataBuff[IFACE_MOTOR_05MS_NUM];
int16 gSendToMotor2MsDataBuff[IFACE_MOTOR_2MS_NUM];

struct INV_PARA invPara;
union INVT_STATUS invtStatus;
Uint16 itDisp;

Uint16 motorCtrlTuneStatus;
struct MOTOR_FC motorFc;

Uint16 outCurrentDisp;
Uint16 outVoltageDisp;
union RUN_FLAG runFlag;
enum RUN_STATUS runStatus;
Uint16 softOcDoFlag;
Uint32 torqueCurrentAct;
Uint16 tuneCmd;
int16  upperTorque;
Uint16 vfSeparateVol;

/* errorInfoFromMotor 被 extern 声明但本模块从未引用; 仍然给一份定义,
 * 免得将来改写引入引用时无声地缺符号。 */
Uint16 errorInfoFromMotor;

/* ---- LpfCalc --------------------------------------------------------------
 *
 * `LowPassFilter.calc` 是一个**函数指针**, 由 `LPF_DEFALUTS`
 * (include/function/f_io.h:387) 初始化成 `(void (*)(void *))LpfCalc`。也就是说
 * UpdateDataCore2Func 里那句
 *     frqFdbLpf.calc(&frqFdbLpf);
 * 是一次**间接调用**, 目标函数在本模块之外。
 *
 * 它是两侧共享的外部依赖, 所以按交付源码逐字复制一份 (src/function/f_io.c:2123
 * 的 `#elif 1` 活分支), 而不是自己写一个"差不多"的滤波器 —— 滤波器一不一样,
 * frqFdb 的每一次取值都会变, 那才是真的测不准。
 *
 * 活分支只用到 AI_CALC_PERIOD (f_main.h: 2), 没有其它全局量, 所以可以整段搬。
 * 原文件的 `#if 0` 分支 (用 TIME_UNIT_AI_PULSE_IN_FILTER 与 t1) 是死代码, 不搬。
 *
 * 注意这里**只**提供被调用的那个函数。它不构成"改写安全"的证据 ——
 * 它和被测模块一样, 两侧看到的完全相同。
 */
void LpfCalc(LowPassFilter *p)
{
    int32 tmp;
    int32 out;

    p->outOld = p->out;
    if (!p->t)
    {
        p->out = p->in;
    }
    else
    {
        tmp = ((int32)p->in - p->outOld + p->remainder) * AI_CALC_PERIOD;
        out = tmp / (p->t * 1) + p->outOld;

        p->out = tmp / (p->t * 1) + p->outOld;
        p->remainder = tmp % (p->t * 1);
    }
}
