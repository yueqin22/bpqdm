/* f_io case mock: 外设对象 + 工程全局量 + 被调函数替身, 两侧共享一份。
 *
 * 这里的东西都**不按侧重命名** —— 它们是模块的输入面与输出面, 不是任一侧的
 * 实现。任何一处按侧复制, 都会让"差异来自输入"和"差异来自代码"分不开。
 *
 * 注意: f_io.c **自己定义**了它那三十多个状态量 (diFunc / diSelectFunc / diStatus /
 * diDelayTicker / doFunc / doStatus / doHwStatus / doDelayTicker / aiLpf / aiDeal /
 * aoFmpValue / aoFmpMax / pulseInFrq / pulseInFrq1 / pulseInSet / lineSpeed /
 * pulseInLpf / capturePeriodSum / captureTicker / noCaptureTicker /
 * bOverFlowCounterTicker / bOverFlowLengthCurrent / oCurrentChkTicker /
 * oCurrentCheckFlag / softOcDoFlag / motorForeOT / motorOT / temperature /
 * tickerTempDeal / tempSampleSum / temperatureVoltage / temperatureVoltageOrigin /
 * diLogic / vdiSrc / vdiFcSet / doLogic / diHwStatus / 两张温度表 / doFuncCal)。
 * 那些量经 ref_tu.c / new_tu.c 改名成 ref_* / new_*, 由 driver.c 用 extern 分别
 * 访问 —— 本文件**不能**定义它们, 否则链接期撞名。
 *
 * ---------------------------------------------------------------------------
 * 1. 外设对象 (真寄存器结构体, 实例化而不是造替身)
 *
 *   ECap1Regs     PulseInSample 读 CAP1..CAP4 (非零才算一次捕获, 读完写回 0),
 *                 InitSetEcap1 写 ECEINT / ECCLR / ECCTL1 / ECCTL2。
 *                 CAP1..CAP4 在这里**既是输入又是输出**, 必须用真布局。
 *   EPwm4Regs     AoCalcChannel(AO2) 写 TBPRD / CMPA.half.CMPA
 *   EPwm5Regs     FMPDeal 写 TBPRD / CMPB / TBCTR / TBCTL; PulseOutCalc 还写
 *                 CMPCTL 与 AQCTLB 之外的分频位; InitSetEPWM5
 *   EPwm6Regs     AoCalcChannel(AO1)
 *   GpioCtrlRegs  InitDIGpio / InitDOGpio / InitEPwm{4,5,6}Gpio / InitECap1Gpio
 *   GpioDataRegs  getDiHwStatus 读十个位; setDOStatus 写 GPxSET / GPxCLEAR 的五个位
 *
 *   EALLOW / EDIS / DINT / EINT 做成 gHostCpuCtl[] 的计数器而不是空宏。PulseOutCalc
 *   的临界区里同时有 DINT / EALLOW / EDIS / EINT 四条 —— "改写把寄存器写挪到
 *   EALLOW 窗口外面"或"少写一个 EDIS"只比结果永远看不出来, 比计数才看得见。
 *   代价: 改写时不能为了看着顺眼把 DINT/EINT 或 EALLOW/EDIS 合并或重排。
 *
 * ---------------------------------------------------------------------------
 * 2. 工程全局量
 *
 *   funcCode       f_funcCode.c 定义。那份文件不能链: funcCodeGradeSum 的静态
 *                  初始化式是"两个地址常量相减", MSVC 报 C2099 (见
 *                  tools/difftest/gen_plc_grade_tables.py 的长注释)。
 *                  本模块读它约 60 个字段, 也**写**三个 (PulseInCalc 的
 *                  counterTicker / lengthCurrent / lengthTickerRemainder), 所以它
 *                  同时是输入和输出 —— driver 每例整体灌入, 跑完整体取出比对。
 *   runFlag / errorCode / errorOther / errorDealStatus / errorAttribute /
 *   errAutoRstNum / dspStatus / frq / frqAim / frqRun / frqDisp / maxFrq /
 *   upperFrq / lowerFrq / swingFrqLimit / tuneCmd / outCurrentDisp / outPower /
 *   outVoltage / generatrixVoltage / itDisp / motorFc / aoComm / doComm /
 *   bPlcEndOneLoop / bUv / curTime / setRunLostTime / setRunTimeAim /
 *   mainLoopTicker
 *                  分别来自 f_runSrc.c / f_error.c / f_frqSrc.c / f_comm.c /
 *                  f_main.c。本模块只读它们, 由 driver 供值。
 *
 * ---------------------------------------------------------------------------
 * 3. 被调函数替身
 *
 *   TorqueLimitCalc    Dout14 的唯一判据。做成**可参数化**的返回值而不是抄实现:
 *                      真实现 (f_torqueCtrl.c:189) 带两个 static 计数器, 抄进来
 *                      等于把另一个模块的状态机塞进本用例, 出错时指错模块。
 *                      参数化之后 `if (TorqueLimitCalc())` 与 `if (!...)` 仍然
 *                      分得开 —— 那才是 Dout14 里唯一该由本用例负责的判据。
 *
 *   FcDigit2Bin / GetNumberDigit / GetNumberDigit1
 *                      逐字抄自冻结基准 (f_menu.c:2554 / f_main.c:831 / f_main.c:854)。
 *                      不能换成常量: Di2Bin / Do2Bin 用它把 funcCode 里的十进制
 *                      编码位串折成二进制掩码, diLogic / doLogic 是**被比对的状态**,
 *                      换成常量会让"哪个字段喂进去"和"移了几位"同时变得不可观测。
 */
#include "f_io.h"
#include "f_main.h"
#include "f_runSrc.h"
#include "f_comm.h"
#include "f_menu.h"
#include "f_frqSrc.h"
#include "f_posCtrl.h"
#include "f_error.h"

/* ------------------------------------------------------------------------- */
/* 1. 外设对象                                                                */
/* ------------------------------------------------------------------------- */

volatile struct ECAP_REGS         ECap1Regs;
volatile struct EPWM_REGS         EPwm4Regs;
volatile struct EPWM_REGS         EPwm5Regs;
volatile struct EPWM_REGS         EPwm6Regs;
volatile struct GPIO_CTRL_REGS    GpioCtrlRegs;
volatile struct GPIO_DATA_REGS    GpioDataRegs;
volatile Uint16                   gHostCpuCtl[7];

/* ------------------------------------------------------------------------- */
/* 2. 工程全局量                                                              */
/* ------------------------------------------------------------------------- */

FUNCCODE_ALL funcCode;

union RUN_FLAG runFlag;

Uint16 errorCode;
Uint16 errorOther;
enum ERROR_DEAL_STATUS errorDealStatus;
union ERROR_ATTRIBUTE errorAttribute;
Uint16 errAutoRstNum;

union DSP_STATUS dspStatus;

int32 frq;
int32 frqAim;
int32 frqRun;
Uint16 frqDisp;
Uint16 maxFrq;
Uint16 upperFrq;
Uint16 lowerFrq;
Uint16 swingFrqLimit;
Uint16 tuneCmd;

Uint16 outCurrentDisp;
int16 outPower;
Uint16 outVoltage;
Uint16 generatrixVoltage;
Uint16 itDisp;

struct MOTOR_FC motorFc;

Uint16 aoComm[AO_NUMBER + 2];
union DO_SCI doComm;

/* diHwStatus 的真身在 src/function/f_ui.c:105, 不在本模块里, 所以它既不是
 * f_io.c 的私有量 (不能靠 TU 改名分侧), 也不是纯输入 —— getDiHwStatus 每轮都
 * 整体重写它。于是它落进 ENV: driver 每例灌一份, 跑完取出来比, 两侧互不干扰。 */
union DI_HW_STATUS diHwStatus;

Uint16 bPlcEndOneLoop;
Uint16 bUv;

struct CUR_TIME curTime;

Uint16 setRunLostTime;
Uint16 setRunTimeAim;

Uint16 mainLoopTicker;

/* ------------------------------------------------------------------------- */
/* 3. 被调函数替身                                                            */
/* ------------------------------------------------------------------------- */

Uint16 gMockTorqueLimitRet;

Uint16 TorqueLimitCalc(void)
{
    return gMockTorqueLimitRet;
}

/* 逐字抄自 src/function/f_main.c:854。digit[0] 是**个位**。 */
void GetNumberDigit1(Uint16 digit[5], Uint16 number)
{
    Uint16 tmp;

    tmp = (Uint16)((Uint32)number * 52429 >> 19);
    digit[0] = number - tmp * 10;
    number = tmp;

    tmp = (Uint16)((Uint32)number * 52429 >> 19);
    digit[1] = number - tmp * 10;
    number = tmp;

    tmp = (Uint16)((Uint32)number * 52429 >> 19);
    digit[2] = number - tmp * 10;
    number = tmp;

    tmp = (Uint16)((Uint32)number * 52429 >> 19);
    digit[3] = number - tmp * 10;
    number = tmp;

    tmp = (Uint16)((Uint32)number * 52429 >> 19);
    digit[4] = number - tmp * 10;
}

/* 逐字抄自 src/function/f_main.c:831。 */
Uint16 GetNumberDigit(Uint16 digit[5], Uint16 number, Uint16 mode)
{
    (void)mode;
    GetNumberDigit1(digit, number);

    if (digit[4] != 0)
        return 5;
    else if (digit[3] != 0)
        return 4;
    else if (digit[2] != 0)
        return 3;
    else if (digit[1] != 0)
        return 2;
    else
        return 1;
}

/* 逐字抄自 src/function/f_menu.c:2554。
 *
 * MENU_DIGIT_NUM 是 f_menu.c:45 的**私有** #define (头文件里没有), 所以这里照抄
 * 一遍数值 5。抄错的后果不是"测错东西": FcDigit2Bin 的返回值直接就是被比对的
 * diLogic / doLogic, 两侧用同一个副本, 差异仍然可比 —— 只是覆盖的位数变了。 */
#define MENU_DIGIT_NUM                  5

Uint16 FcDigit2Bin(Uint16 value)
{
    Uint16 tmp = 0;
    Uint16 digit[MENU_DIGIT_NUM];
    int16 i;

    GetNumberDigit(digit, value, DECIMAL);

    for (i = MENU_DIGIT_NUM - 1; i >= 0; i--)
    {
        if (digit[i])
        {
            tmp += 1 << i;
        }
    }

    return tmp;
}
