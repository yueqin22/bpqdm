/* f_io 差分测试驱动。
 *
 * 被测模块是 I/O 层: 开关量输入 (DI) / 开关量输出 (DO) / 模拟量 (AI/AO) /
 * 高速脉冲入 (HDI) 与高速脉冲出 (HDO) / 温度传感器。它的"能做什么"分三块:
 *
 *   1. 写自己的状态: diFunc / diSelectFunc / diStatus(a,b,c) / diDelayTicker /
 *      doFunc / doStatus(a,b,c) / doHwStatus / doDelayTicker / aiLpf / aiDeal /
 *      aoFmpValue / aoFmpMax / pulseInFrq / pulseInFrq1 / pulseInSet / lineSpeed /
 *      pulseInLpf / capturePeriodSum / captureTicker / noCaptureTicker /
 *      bOverFlow* / oCurrentChkTicker / oCurrentCheckFlag / softOcDoFlag /
 *      motorForeOT / motorOT / temperature / tickerTempDeal / tempSampleSum /
 *      temperatureVoltage / temperatureVoltageOrigin / diLogic / vdiSrc / vdiFcSet /
 *      doLogic。
 *   2. 写外设寄存器: ECap1Regs (CAP1..CAP4 / ECCTL* / ECEINT / ECCLR)、
 *      EPwm4/5/6Regs (TBPRD / CMPA / CMPB / TBCTR / TBCTL / CMPCTL / AQCTL*)、
 *      GpioCtrlRegs (GPxMUX / GPxDIR / GPxPUD / GPxQSEL)、GpioDataRegs
 *      (GPxDAT / GPxSET / GPxCLEAR), 以及 EALLOW/EDIS/DINT/EINT 的 CPU 控制序列。
 *   3. 写**别的模块持有**的量: funcCode.code 的 counterTicker / lengthCurrent /
 *      lengthTickerRemainder (PulseInCalc), diHwStatus (getDiHwStatus),
 *      errorOther (TemperatureAfterDeal 的 ERROR_MOTOR_OT)。
 *
 * 三块都进比较镜像。只比状态会漏掉"寄存器写错/写丢", 只比寄存器会漏掉判据层。
 *
 * ---------------------------------------------------------------------------
 * 为什么 funcCode 与 diHwStatus 也在 ENV 里
 *
 * 它们既不是本模块的私有量 (定义在 f_funcCode.c / f_ui.c), 又不是纯输入 ——
 * 本模块会写它们。两侧共用同一份对象的话, 先跑的那一侧写下的值会被后跑的一侧
 * 覆盖, 差异就永远看不见。所以 ENV 里存的是**副本**: install() 灌进去,
 * snapshot() 取出来, compare() 比副本。
 *
 * ---------------------------------------------------------------------------
 * 输入必须留在**定义域内** (这一条踩过坑, 是本轮最大的教训)
 *
 * 这个模块有若干"函数自己保证成立"的前提, 一旦驱动喂出定义域外的值, 宿主上就是
 * 除零或越界写 —— 而总账把"rc != 0"一律记成 CAUGHT, 于是一整批崩溃会被读成
 * "变异全抓到了"。所以每条前提都在这里钉死, 并在 phase 里注明理由:
 *
 *   diFunc[] / vdiFunc[] / aiAsDiFunc[]   0..DI_FUNC_NUMBER-1 (60)
 *       DiCalc 用 `fc1[i] / 32` 去索引 diSelectFunc 的 f1/f2 两个字。fc >= 64 就
 *       写到结构体外去了。
 *   doFunc[] / vdoFunc[]                  0..DO_FUNC_NUMBER-1 (42)
 *       DoCalc 用它索引 doFuncCal[], 越界就是跳到任意地址。
 *   aiCurveSrc 的每个半字节             1..5
 *       AiCalc 算 `pAiSetCurve[aiNum - 1]`, aiNum = 0 时下标是 -1。
 *   lengthPulsePerMeter != 0
 *       PulseInCalc 拿它当除数两次 (lineSpeed 的商和余数)。
 *   fmpOutMaxFrq != 0 且 aoFmpValue != 0
 *       PulseOutCalc 的 `... / (aoFmpValue * fmpOutMaxFrq)` 在检查 `!aoFmpValue`
 *       **之前**就做了除法。
 *   maxFrq / ratingCurrent / ratingPower / lengthSet / counterSet != 0
 *       UpdateFmpAoValue 把它们当 aoFmpMax, AoCalcChannel 立刻拿 aoFmpMax 做除数。
 *   所有标定曲线 x1 != x2
 *       LineCalc 的 `tmp = ... / (p->x2 - p->x1)` 在**所有分支之前**无条件计算,
 *       所以哪怕 mode==1 也不能让 x1 == x2。
 *   motorOtMode 只能是 PTC100 / PTC1000 (直接调 GetTemperature 时)
 *       否则 p / size 两个局部变量没被赋值, GetTemperatureCalc 读到野指针。
 *   GetTemperatureCalc 的 len >= 2
 *       len == 1 时 `i = len - 1 = 0`, 接着读 `*(p + i - 1)` = p[-1]。
 *   captureTicker != 0 时 capturePeriodSum != 0
 *       PulseInCalc 拿 capturePeriodSum 当除数。
 *
 * ---------------------------------------------------------------------------
 * 函数内 static 变量: 不复位, 靠"两侧同步演化"
 *
 * DiCalc 的 diXFilterCounter[]、Dout3/Dout25 的 frqFdtArriveOld、Dout30 的
 * arriveTcnt、Di2Bin/Do2Bin 的四个 Old、FMPDeal 的 fmOutSelectOld、
 * PulseOutCalc 的 periodOld/beforeFrq/CLOCK_BENCH、PulseInCalc 的六个计数器
 * —— 这些都是**翻译单元私有**的, 改名管不到, 驱动也够不着, 所以每例都无法复位。
 *
 * 这没问题: 两侧调用序列逐例相同, 状态就同步演化; 一旦某例行为不同, 那一例立刻
 * 比出差异 (后面还会继续差, 但 FAIL_LIMIT 截断输出, 首个差异已经在最前面)。
 * 代价是**顺序有意义** —— 同一个 phase 里前后两例不是独立的, 删中间一例会改变
 * 后面所有例的输入。所以 phase 内部不要随便增删, 要么就整段重跑。
 *
 * ---------------------------------------------------------------------------
 * 覆盖台账数的是"入口被驱动", 不是"分支被执行"
 *
 * 分支是否真的走到, 由负向对照来证: 每条变异锚在某个分支里, CAUGHT 就说明那一支
 * 确实被喂到过。在驱动里重算判据等于把被测逻辑抄一遍, 抄错时会谎报命中
 * (MotorVF 的 CU7/CU8/CU9 就是这么谎报的)。宁可少说, 不可错说。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "f_io.h"
#include "f_main.h"
#include "f_runSrc.h"
#include "f_comm.h"
#include "f_menu.h"
#include "f_frqSrc.h"
#include "f_posCtrl.h"
#include "f_error.h"

#define AI_N            AI_NUMBER
#define DO_N            DO_TERMINAL_NUMBER
#define DI_N            DI_TERMINAL_NUMBER
#define AO_N            (AO_NUMBER + 1)
#define FAIL_LIMIT      20

/* ------------------------------------------------------------------------- */
/* 两侧的符号                                                                */
/* ------------------------------------------------------------------------- */

#define TWO(ret, name, args) \
    extern ret ref_##name args; \
    extern ret new_##name args;

TWO(void,   AiCalc,              (void))
TWO(void,   AoCalcChannel,       (Uint16))
TWO(void,   Di2Bin,              (void))
TWO(void,   DiCalc,              (void))
TWO(void,   Do2Bin,              (void))
TWO(void,   DoCalc,              (void))
TWO(void,   FMPDeal,             (void))
TWO(void,   GetTemperature,      (void))
TWO(Uint16, GetTemperatureCalc,  (Uint16, const Uint16 *, Uint16))
TWO(void,   InitDIGpio,          (void))
TWO(void,   InitDOGpio,          (void))
TWO(void,   InitECap1Gpio,       (void))
TWO(void,   InitECap2Gpio,       (void))
TWO(void,   InitECap3Gpio,       (void))
TWO(void,   InitECap4Gpio,       (void))
TWO(void,   InitEPwm4Gpio,       (void))
TWO(void,   InitEPwm5Gpio,       (void))
TWO(void,   InitEPwm6Gpio,       (void))
TWO(void,   InitSetEPWM4,        (void))
TWO(void,   InitSetEPWM5,        (void))
TWO(void,   InitSetEPWM6,        (void))
TWO(void,   InitSetEcap1,        (void))
TWO(void,   LineCalc,            (LINE_STRUCT *))
TWO(void,   LpfCalc,             (LowPassFilter *))
TWO(void,   PulseInCalc,         (void))
TWO(void,   PulseInSample,       (void))
TWO(void,   PulseOutCalc,        (void))
TWO(void,   TemperatureAfterDeal,(void))
TWO(void,   TemperatureDeal,     (void))
TWO(void,   TemperatureSensorDeal,(void))
TWO(void,   UpdateFmpAoValue,    (Uint16, Uint16))
TWO(void,   getDiHwStatus,       (void))
TWO(void,   setDOStatus,         (void))

/* Dout1..Dout41 + DoutRSVD, 顺序与 doFuncCal[] 一致 (下标即功能码)。 */
TWO(void,   DoutRSVD, (void))
TWO(void,   Dout1,    (void))
TWO(void,   Dout2,    (void))
TWO(void,   Dout3,    (void))
TWO(void,   Dout4,    (void))
TWO(void,   Dout5,    (void))
TWO(void,   Dout6,    (void))
TWO(void,   Dout7,    (void))
TWO(void,   Dout8,    (void))
TWO(void,   Dout9,    (void))
TWO(void,   Dout10,   (void))
TWO(void,   Dout11,   (void))
TWO(void,   Dout12,   (void))
TWO(void,   Dout13,   (void))
TWO(void,   Dout14,   (void))
TWO(void,   Dout15,   (void))
TWO(void,   Dout16,   (void))
TWO(void,   Dout17,   (void))
TWO(void,   Dout18,   (void))
TWO(void,   Dout19,   (void))
TWO(void,   Dout21,   (void))
TWO(void,   Dout22,   (void))
TWO(void,   Dout23,   (void))
TWO(void,   Dout24,   (void))
TWO(void,   Dout25,   (void))
TWO(void,   Dout26,   (void))
TWO(void,   Dout27,   (void))
TWO(void,   Dout28,   (void))
TWO(void,   Dout29,   (void))
TWO(void,   Dout30,   (void))
TWO(void,   Dout31,   (void))
TWO(void,   Dout32,   (void))
TWO(void,   Dout33,   (void))
TWO(void,   Dout34,   (void))
TWO(void,   Dout35,   (void))
TWO(void,   Dout36,   (void))
TWO(void,   Dout37,   (void))
TWO(void,   Dout38,   (void))
TWO(void,   Dout39,   (void))
TWO(void,   Dout40,   (void))
TWO(void,   Dout41,   (void))

/* 模块私有的量 (f_io.c 自己定义, 经 TU 改名分侧)。 */
extern struct DI_FUNC         ref_diFunc,         new_diFunc;
extern struct DI_FUNC         ref_diSelectFunc,   new_diSelectFunc;
extern struct DI_STATUS_ALL   ref_diStatus,       new_diStatus;
extern union DI_DELAY_TICKER_UNION ref_diDelayTicker, new_diDelayTicker;
extern struct DO_FUNC         ref_doFunc,         new_doFunc;
extern struct DO_STATUS_ALL   ref_doStatus,       new_doStatus;
extern union DO_HW_STATUS     ref_doHwStatus,     new_doHwStatus;
extern union DO_DELAY_TICKER_UNION ref_doDelayTicker, new_doDelayTicker;
extern Uint16                 ref_oCurrentCheckFlag, new_oCurrentCheckFlag;
extern Uint32                 ref_oCurrentChkTicker, new_oCurrentChkTicker;
extern Uint16                 ref_softOcDoFlag,   new_softOcDoFlag;
extern Uint16                 ref_motorForeOT,    new_motorForeOT;
extern Uint16                 ref_motorOT,        new_motorOT;
extern Uint16                 ref_temperature,    new_temperature;
extern Uint16                 ref_tickerTempDeal, new_tickerTempDeal;
extern Uint32                 ref_tempSampleSum,  new_tempSampleSum;
extern Uint16                 ref_temperatureVoltage,       new_temperatureVoltage;
extern Uint16                 ref_temperatureVoltageOrigin, new_temperatureVoltageOrigin;
extern Uint32                 ref_diLogic,        new_diLogic;
extern Uint16                 ref_vdiSrc,         new_vdiSrc;
extern Uint16                 ref_vdiFcSet,       new_vdiFcSet;
extern Uint16                 ref_doLogic,        new_doLogic;
extern int32                  ref_aoFmpValue,     new_aoFmpValue;
extern int32                  ref_aoFmpMax,       new_aoFmpMax;
extern int16                  ref_pulseInSet,     new_pulseInSet;
extern Uint32                 ref_pulseInFrq,     new_pulseInFrq;
extern Uint32                 ref_pulseInFrq1,    new_pulseInFrq1;
extern Uint16                 ref_lineSpeed,      new_lineSpeed;
extern Uint32                 ref_capturePeriodSum, new_capturePeriodSum;
extern Uint16                 ref_captureTicker,  new_captureTicker;
extern Uint16                 ref_noCaptureTicker, new_noCaptureTicker;
extern Uint16                 ref_bOverFlowCounterTicker, new_bOverFlowCounterTicker;
extern Uint16                 ref_bOverFlowLengthCurrent, new_bOverFlowLengthCurrent;
extern LowPassFilter          ref_aiLpf[AI_N],    new_aiLpf[AI_N];
extern LowPassFilter          ref_pulseInLpf,     new_pulseInLpf;
extern struct AI_DEAL         ref_aiDeal[AI_N],   new_aiDeal[AI_N];

/* 本用例 mock 持有的量, 两侧共用一份对象, 但每例都整体重灌。 */
extern FUNCCODE_ALL            funcCode;
extern union RUN_FLAG          runFlag;
extern Uint16                  errorCode;
extern Uint16                  errorOther;
extern enum ERROR_DEAL_STATUS  errorDealStatus;
extern union ERROR_ATTRIBUTE   errorAttribute;
extern Uint16                  errAutoRstNum;
extern union DSP_STATUS        dspStatus;
extern int32                   frq;
extern int32                   frqAim;
extern int32                   frqRun;
extern Uint16                  frqDisp;
extern Uint16                  maxFrq;
extern Uint16                  upperFrq;
extern Uint16                  lowerFrq;
extern Uint16                  swingFrqLimit;
extern Uint16                  tuneCmd;
extern Uint16                  outCurrentDisp;
extern int16                   outPower;
extern Uint16                  outVoltage;
extern Uint16                  generatrixVoltage;
extern Uint16                  itDisp;
extern struct MOTOR_FC         motorFc;
extern Uint16                  aoComm[];
extern union DO_SCI            doComm;
extern Uint16                  bPlcEndOneLoop;
extern Uint16                  bUv;
extern struct CUR_TIME         curTime;
extern Uint16                  setRunLostTime;
extern Uint16                  setRunTimeAim;
extern Uint16                  mainLoopTicker;
extern union DI_HW_STATUS      diHwStatus;
extern Uint16                  gMockTorqueLimitRet;

extern volatile struct ECAP_REGS      ECap1Regs;
extern volatile struct EPWM_REGS      EPwm4Regs;
extern volatile struct EPWM_REGS      EPwm5Regs;
extern volatile struct EPWM_REGS      EPwm6Regs;
extern volatile struct GPIO_CTRL_REGS GpioCtrlRegs;
extern volatile struct GPIO_DATA_REGS GpioDataRegs;
extern volatile Uint16                gHostCpuCtl[7];

/* ------------------------------------------------------------------------- */
/* 镜像                                                                      */
/* ------------------------------------------------------------------------- */

/* LowPassFilter 里的 calc 指针**不能**整体 memcpy: 两侧装的是各自的
 * ref_LpfCalc / new_LpfCalc, 地址必然不同。只比五个数值字段。 */
struct LPF_STATE
{
    int32 t;
    int32 in;
    int32 out;
    int32 outOld;
    int32 remainder;
};

struct AI_STATE
{
    Uint16 sample;
    int16  voltageOrigin;
    int16  voltage;
    int16  set;
};

struct MSTATE
{
    struct DI_FUNC        diFunc;
    struct DI_FUNC        diSelectFunc;
    struct DI_STATUS_ALL  diStatus;
    union DI_DELAY_TICKER_UNION diDelayTicker;
    struct DO_FUNC        doFunc;
    struct DO_STATUS_ALL  doStatus;
    union DO_HW_STATUS    doHwStatus;
    union DO_DELAY_TICKER_UNION doDelayTicker;

    Uint16 oCurrentCheckFlag;
    Uint32 oCurrentChkTicker;
    Uint16 softOcDoFlag;
    Uint16 motorForeOT;
    Uint16 motorOT;
    Uint16 temperature;
    Uint16 tickerTempDeal;
    Uint32 tempSampleSum;
    Uint16 temperatureVoltage;
    Uint16 temperatureVoltageOrigin;

    Uint32 diLogic;
    Uint16 vdiSrc;
    Uint16 vdiFcSet;
    Uint16 doLogic;

    int32  aoFmpValue;
    int32  aoFmpMax;
    int16  pulseInSet;
    Uint32 pulseInFrq;
    Uint32 pulseInFrq1;
    Uint16 lineSpeed;

    Uint32 capturePeriodSum;
    Uint16 captureTicker;
    Uint16 noCaptureTicker;
    Uint16 bOverFlowCounterTicker;
    Uint16 bOverFlowLengthCurrent;

    struct LPF_STATE aiLpf[AI_N];
    struct LPF_STATE pulseInLpf;
    struct AI_STATE  aiDeal[AI_N];
};

struct ENV
{
    FUNCCODE_ALL fc;

    union RUN_FLAG         run;
    Uint16                 errCode;
    Uint16                 errOther;
    enum ERROR_DEAL_STATUS errDealStatus;
    union ERROR_ATTRIBUTE  errAttr;
    Uint16                 errAutoRstNum;
    union DSP_STATUS       dsp;

    int32  frq;
    int32  frqAim;
    int32  frqRun;
    Uint16 frqDisp;
    Uint16 maxFrq;
    Uint16 upperFrq;
    Uint16 lowerFrq;
    Uint16 swingFrqLimit;
    Uint16 tuneCmd;

    Uint16 outCurrentDisp;
    int16  outPower;
    Uint16 outVoltage;
    Uint16 generatrixVoltage;
    Uint16 itDisp;

    Uint16 ratingCurrent;
    Uint16 ratingPower;

    Uint16 ao[AO_N];
    union DO_SCI doComm;

    Uint16 bPlcEndOneLoop;
    Uint16 bUv;

    struct CUR_TIME curTime;

    Uint16 setRunLostTime;
    Uint16 setRunTimeAim;
    Uint16 mainLoopTicker;

    Uint16 torqueRet;

    struct ECAP_REGS      ecap1;
    struct EPWM_REGS      epwm4;
    struct EPWM_REGS      epwm5;
    struct EPWM_REGS      epwm6;
    struct GPIO_CTRL_REGS gpioCtrl;
    struct GPIO_DATA_REGS gpioData;
    Uint16                ctl[7];

    union DI_HW_STATUS diHw;
};

struct SIDE
{
    Uint16         ret;
    struct MSTATE  m;
    struct ENV     e;
    /* LineCalc / LpfCalc 的**唯一输出**就是它们改的那一个结构体
     * (`p->y` / `p->out` + `p->outOld` + `p->remainder`)。这两个结构体是驱动
     * 自己的局部对象, 既不是模块全局量也不是入参快照 —— 原来没进比较镜像,
     * 于是 `K_LineCalc` / `K_LpfCalc` 两个入口等于"调了但没看结果",
     * 它们能被抓到全靠 AiCalc / PulseInCalc 顺带把同一个函数又调了一遍。
     * 单独驱动的那几十例一直是空转。 */
    LINE_STRUCT    line;
    LowPassFilter  lpf;
};

static struct SIDE S[2];

/* 阶段里临时构造的 LINE_STRUCT / LowPassFilter 参数。放在文件作用域是为了让
 * install() / snapshot() 都能读到 (它们是函数的入参, 不是模块全局状态)。
 * 必须声明在 snapshot 与 run1 **之前**。 */
static LINE_STRUCT   g_line;
static LowPassFilter g_lpf;
static Uint16        g_tbl[8];

static struct MSTATE g_inM;
static struct ENV    g_inE;

/* ------------------------------------------------------------------------- */
/* 装 / 取                                                                   */
/* ------------------------------------------------------------------------- */

static void lpf_in(LowPassFilter *dst, const struct LPF_STATE *s)
{
    dst->t         = s->t;
    dst->in        = s->in;
    dst->out       = s->out;
    dst->outOld    = s->outOld;
    dst->remainder = s->remainder;
}

static void lpf_out(struct LPF_STATE *s, const LowPassFilter *src)
{
    s->t         = src->t;
    s->in        = src->in;
    s->out       = src->out;
    s->outOld    = src->outOld;
    s->remainder = src->remainder;
}

static void install(int side)
{
    const struct MSTATE *m = &g_inM;
    const struct ENV    *e = &g_inE;
    int i;

    memcpy(&funcCode,          &e->fc,  sizeof funcCode);
    runFlag                    = e->run;
    errorCode                  = e->errCode;
    errorOther                 = e->errOther;
    errorDealStatus            = e->errDealStatus;
    errorAttribute             = e->errAttr;
    errAutoRstNum              = e->errAutoRstNum;
    dspStatus                  = e->dsp;

    frq                        = e->frq;
    frqAim                     = e->frqAim;
    frqRun                     = e->frqRun;
    frqDisp                    = e->frqDisp;
    maxFrq                     = e->maxFrq;
    upperFrq                   = e->upperFrq;
    lowerFrq                   = e->lowerFrq;
    swingFrqLimit              = e->swingFrqLimit;
    tuneCmd                    = e->tuneCmd;

    outCurrentDisp             = e->outCurrentDisp;
    outPower                   = e->outPower;
    outVoltage                 = e->outVoltage;
    generatrixVoltage          = e->generatrixVoltage;
    itDisp                     = e->itDisp;

    motorFc.motorPara.elem.ratingCurrent = e->ratingCurrent;
    motorFc.motorPara.elem.ratingPower   = e->ratingPower;

    for (i = 0; i < AO_N; i++)
    {
        aoComm[i] = e->ao[i];
    }
    doComm                     = e->doComm;

    bPlcEndOneLoop             = e->bPlcEndOneLoop;
    bUv                        = e->bUv;
    curTime                    = e->curTime;
    setRunLostTime             = e->setRunLostTime;
    setRunTimeAim              = e->setRunTimeAim;
    mainLoopTicker             = e->mainLoopTicker;

    gMockTorqueLimitRet        = e->torqueRet;

    diHwStatus                 = e->diHw;

    ECap1Regs   = e->ecap1;
    EPwm4Regs   = e->epwm4;
    EPwm5Regs   = e->epwm5;
    EPwm6Regs   = e->epwm6;
    GpioCtrlRegs = e->gpioCtrl;
    GpioDataRegs = e->gpioData;
    for (i = 0; i < 7; i++)
    {
        gHostCpuCtl[i] = e->ctl[i];
    }

    /* ---- 模块自己的状态 ----
     *
     * 这些量的**真名**在 f_io.c 里, 但两侧各自被 TU 改名成 ref_* / new_*, 而
     * 头文件只声明了未改名的那个 (f_io.h 声明 diFunc / diStatus / doFunc ... 的
     * 未改名版本), 所以驱动里既不能直接用 `diFunc`, 也不能靠一个全局指针表 ——
     * 用一对宏按 side 选边。 */
#define IO_SET(name, field) \
    do { if (side) { new_##name = m->field; } else { ref_##name = m->field; } } while (0)

    IO_SET(diFunc,        diFunc);
    IO_SET(diSelectFunc,  diSelectFunc);
    IO_SET(diStatus,      diStatus);
    IO_SET(diDelayTicker, diDelayTicker);
    IO_SET(doFunc,        doFunc);
    IO_SET(doStatus,      doStatus);
    IO_SET(doHwStatus,    doHwStatus);
    IO_SET(doDelayTicker, doDelayTicker);

    IO_SET(oCurrentCheckFlag, oCurrentCheckFlag);
    IO_SET(oCurrentChkTicker, oCurrentChkTicker);
    IO_SET(softOcDoFlag,      softOcDoFlag);
    IO_SET(motorForeOT,       motorForeOT);
    IO_SET(motorOT,           motorOT);
    IO_SET(temperature,       temperature);
    IO_SET(tickerTempDeal,    tickerTempDeal);
    IO_SET(tempSampleSum,     tempSampleSum);
    IO_SET(temperatureVoltage,       temperatureVoltage);
    IO_SET(temperatureVoltageOrigin, temperatureVoltageOrigin);

    IO_SET(diLogic,  diLogic);
    IO_SET(vdiSrc,   vdiSrc);
    IO_SET(vdiFcSet, vdiFcSet);
    IO_SET(doLogic,  doLogic);

    IO_SET(aoFmpValue,  aoFmpValue);
    IO_SET(aoFmpMax,    aoFmpMax);
    IO_SET(pulseInSet,  pulseInSet);
    IO_SET(pulseInFrq,  pulseInFrq);
    IO_SET(pulseInFrq1, pulseInFrq1);
    IO_SET(lineSpeed,   lineSpeed);

    IO_SET(capturePeriodSum,      capturePeriodSum);
    IO_SET(captureTicker,         captureTicker);
    IO_SET(noCaptureTicker,       noCaptureTicker);
    IO_SET(bOverFlowCounterTicker, bOverFlowCounterTicker);
    IO_SET(bOverFlowLengthCurrent, bOverFlowLengthCurrent);

    for (i = 0; i < AI_N; i++)
    {
        if (side)
        {
            lpf_in(&new_aiLpf[i], &m->aiLpf[i]);
            new_aiDeal[i].sample        = m->aiDeal[i].sample;
            new_aiDeal[i].voltageOrigin = m->aiDeal[i].voltageOrigin;
            new_aiDeal[i].voltage       = m->aiDeal[i].voltage;
            new_aiDeal[i].set           = m->aiDeal[i].set;
        }
        else
        {
            lpf_in(&ref_aiLpf[i], &m->aiLpf[i]);
            ref_aiDeal[i].sample        = m->aiDeal[i].sample;
            ref_aiDeal[i].voltageOrigin = m->aiDeal[i].voltageOrigin;
            ref_aiDeal[i].voltage       = m->aiDeal[i].voltage;
            ref_aiDeal[i].set           = m->aiDeal[i].set;
        }
    }

    if (side)
    {
        lpf_in(&new_pulseInLpf, &m->pulseInLpf);
    }
    else
    {
        lpf_in(&ref_pulseInLpf, &m->pulseInLpf);
    }
#undef IO_SET
}

static void snapshot(int side)
{
    struct MSTATE *m = &S[side].m;
    struct ENV    *e = &S[side].e;
    int i;

    memcpy(&e->fc, &funcCode, sizeof funcCode);
    e->run          = runFlag;
    e->errCode      = errorCode;
    e->errOther     = errorOther;
    e->errDealStatus = errorDealStatus;
    e->errAttr      = errorAttribute;
    e->errAutoRstNum = errAutoRstNum;
    e->dsp          = dspStatus;

    e->frq          = frq;
    e->frqAim       = frqAim;
    e->frqRun       = frqRun;
    e->frqDisp      = frqDisp;
    e->maxFrq       = maxFrq;
    e->upperFrq     = upperFrq;
    e->lowerFrq     = lowerFrq;
    e->swingFrqLimit = swingFrqLimit;
    e->tuneCmd      = tuneCmd;

    e->outCurrentDisp    = outCurrentDisp;
    e->outPower          = outPower;
    e->outVoltage        = outVoltage;
    e->generatrixVoltage = generatrixVoltage;
    e->itDisp            = itDisp;

    e->ratingCurrent = motorFc.motorPara.elem.ratingCurrent;
    e->ratingPower   = motorFc.motorPara.elem.ratingPower;

    for (i = 0; i < AO_N; i++)
    {
        e->ao[i] = aoComm[i];
    }
    e->doComm        = doComm;

    e->bPlcEndOneLoop = bPlcEndOneLoop;
    e->bUv            = bUv;
    e->curTime        = curTime;
    e->setRunLostTime = setRunLostTime;
    e->setRunTimeAim  = setRunTimeAim;
    e->mainLoopTicker = mainLoopTicker;

    e->diHw = diHwStatus;

    e->ecap1     = ECap1Regs;
    e->epwm4     = EPwm4Regs;
    e->epwm5     = EPwm5Regs;
    e->epwm6     = EPwm6Regs;
    e->gpioCtrl  = GpioCtrlRegs;
    e->gpioData  = GpioDataRegs;
    for (i = 0; i < 7; i++)
    {
        e->ctl[i] = gHostCpuCtl[i];
    }

#define IO_GET(name, field) \
    do { m->field = side ? new_##name : ref_##name; } while (0)

    IO_GET(diFunc,        diFunc);
    IO_GET(diSelectFunc,  diSelectFunc);
    IO_GET(diStatus,      diStatus);
    IO_GET(diDelayTicker, diDelayTicker);
    IO_GET(doFunc,        doFunc);
    IO_GET(doStatus,      doStatus);
    IO_GET(doHwStatus,    doHwStatus);
    IO_GET(doDelayTicker, doDelayTicker);

    IO_GET(oCurrentCheckFlag, oCurrentCheckFlag);
    IO_GET(oCurrentChkTicker, oCurrentChkTicker);
    IO_GET(softOcDoFlag,      softOcDoFlag);
    IO_GET(motorForeOT,       motorForeOT);
    IO_GET(motorOT,           motorOT);
    IO_GET(temperature,       temperature);
    IO_GET(tickerTempDeal,    tickerTempDeal);
    IO_GET(tempSampleSum,     tempSampleSum);
    IO_GET(temperatureVoltage,       temperatureVoltage);
    IO_GET(temperatureVoltageOrigin, temperatureVoltageOrigin);

    IO_GET(diLogic,  diLogic);
    IO_GET(vdiSrc,   vdiSrc);
    IO_GET(vdiFcSet, vdiFcSet);
    IO_GET(doLogic,  doLogic);

    IO_GET(aoFmpValue,  aoFmpValue);
    IO_GET(aoFmpMax,    aoFmpMax);
    IO_GET(pulseInSet,  pulseInSet);
    IO_GET(pulseInFrq,  pulseInFrq);
    IO_GET(pulseInFrq1, pulseInFrq1);
    IO_GET(lineSpeed,   lineSpeed);

    IO_GET(capturePeriodSum,       capturePeriodSum);
    IO_GET(captureTicker,          captureTicker);
    IO_GET(noCaptureTicker,        noCaptureTicker);
    IO_GET(bOverFlowCounterTicker, bOverFlowCounterTicker);
    IO_GET(bOverFlowLengthCurrent, bOverFlowLengthCurrent);

    for (i = 0; i < AI_N; i++)
    {
        if (side)
        {
            lpf_out(&m->aiLpf[i], &new_aiLpf[i]);
            m->aiDeal[i].sample        = new_aiDeal[i].sample;
            m->aiDeal[i].voltageOrigin = new_aiDeal[i].voltageOrigin;
            m->aiDeal[i].voltage       = new_aiDeal[i].voltage;
            m->aiDeal[i].set           = new_aiDeal[i].set;
        }
        else
        {
            lpf_out(&m->aiLpf[i], &ref_aiLpf[i]);
            m->aiDeal[i].sample        = ref_aiDeal[i].sample;
            m->aiDeal[i].voltageOrigin = ref_aiDeal[i].voltageOrigin;
            m->aiDeal[i].voltage       = ref_aiDeal[i].voltage;
            m->aiDeal[i].set           = ref_aiDeal[i].set;
        }
    }

    if (side)
    {
        lpf_out(&m->pulseInLpf, &new_pulseInLpf);
    }
    else
    {
        lpf_out(&m->pulseInLpf, &ref_pulseInLpf);
    }

    /* LineCalc / LpfCalc 的入参对象由驱动持有, 两侧共用同一个 —— 必须在这里
     * 各拷一份, 否则第二侧会覆盖第一侧的结果, 比较永远看不出差别。 */
    S[side].line = g_line;
    S[side].lpf  = g_lpf;
#undef IO_GET
}

/* ------------------------------------------------------------------------- */
/* 比对                                                                      */
/* ------------------------------------------------------------------------- */

static long g_cases = 0;
static int  g_fails = 0;

/* 当前这一例的入参, 供 dump_in 打印。 */
static int    g_curKind;
static Uint16 g_curA0;
static Uint16 g_curA1;

static void dump_in(void)
{
    printf("  in: kind=%d a0=%u a1=%u\n", g_curKind, (unsigned)g_curA0, (unsigned)g_curA1);
    printf("      frq=%ld aim=%ld run=%ld maxFrq=%u disp=%u tune=%u\n",
           (long)g_inE.frq, (long)g_inE.frqAim, (long)g_inE.frqRun,
           (unsigned)g_inE.maxFrq, (unsigned)g_inE.frqDisp, (unsigned)g_inE.tuneCmd);
    printf("      err=%u/%u deal=%u attr=%04X rst=%u dsp=%04X\n",
           (unsigned)g_inE.errCode, (unsigned)g_inE.errOther,
           (unsigned)g_inE.errDealStatus, (unsigned)g_inE.errAttr.all,
           (unsigned)g_inE.errAutoRstNum, (unsigned)g_inE.dsp.all);
    printf("      cur=%u setLen=%u setCnt=%u setRun=%u/%u plc=%u\n",
           (unsigned)g_inE.outCurrentDisp, (unsigned)g_inE.fc.code.lengthSet,
           (unsigned)g_inE.fc.code.counterSet,
           (unsigned)g_inE.fc.code.lengthCurrent, (unsigned)g_inE.fc.code.counterTicker,
           (unsigned)g_inE.fc.code.plcEnable);
    printf("      di: st=%08lX hw=%08lX logic=%08lX vsrc=%u vset=%u\n",
           (unsigned long)g_inM.diStatus.a.all, (unsigned long)g_inE.diHw.all,
           (unsigned long)g_inM.diLogic, (unsigned)g_inM.vdiSrc, (unsigned)g_inM.vdiFcSet);
    printf("      do: st=%04X logic=%04X com=%04X f=%04lX/%04lX\n",
           (unsigned)g_inM.doStatus.a.all, (unsigned)g_inM.doLogic,
           (unsigned)g_inE.doComm.all,
           (unsigned long)g_inM.doFunc.f1.all, (unsigned long)g_inM.doFunc.f2.all);
}

static void report(const char *fn, const char *what)
{
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s: %s\n", fn, what);
    dump_in();
}

#define CMP_U(field, name)                                                   \
    if (A->m.field != B->m.field)                                            \
    {                                                                        \
        report(fn, name);                                                    \
        printf("  ref=%lu new=%lu\n", (unsigned long)A->m.field,             \
               (unsigned long)B->m.field);                                   \
    }

#define CMP_S(field, name)                                                   \
    if (A->field != B->field)                                                \
    {                                                                        \
        report(fn, name);                                                    \
        printf("  ref=%lu new=%lu\n", (unsigned long)A->field,               \
               (unsigned long)B->field);                                     \
    }

#define CMP_MEM(a, b, n, name)                                               \
    if (memcmp((a), (b), (n)) != 0)                                          \
    {                                                                        \
        report(fn, name);                                                    \
    }

#define CMP_E(field, name)                                                   \
    if (memcmp(&A->e.field, &B->e.field, sizeof A->e.field) != 0)             \
    {                                                                        \
        report(fn, name);                                                    \
    }

static void compare(const char *fn)
{
    const struct SIDE *A = &S[0];
    const struct SIDE *B = &S[1];
    int i;

    CMP_S(ret, "return value");

    CMP_MEM(&A->m.diFunc,        &B->m.diFunc,        sizeof A->m.diFunc,        "diFunc");
    CMP_MEM(&A->m.diSelectFunc,  &B->m.diSelectFunc,  sizeof A->m.diSelectFunc,  "diSelectFunc");
    CMP_MEM(&A->m.diStatus,      &B->m.diStatus,      sizeof A->m.diStatus,      "diStatus");
    CMP_MEM(&A->m.diDelayTicker, &B->m.diDelayTicker, sizeof A->m.diDelayTicker, "diDelayTicker");
    CMP_MEM(&A->m.doFunc,        &B->m.doFunc,        sizeof A->m.doFunc,        "doFunc");
    CMP_MEM(&A->m.doStatus,      &B->m.doStatus,      sizeof A->m.doStatus,      "doStatus");
    CMP_MEM(&A->m.doHwStatus,    &B->m.doHwStatus,    sizeof A->m.doHwStatus,    "doHwStatus");
    CMP_MEM(&A->m.doDelayTicker, &B->m.doDelayTicker, sizeof A->m.doDelayTicker, "doDelayTicker");

    CMP_U(oCurrentCheckFlag, "oCurrentCheckFlag");
    CMP_U(oCurrentChkTicker, "oCurrentChkTicker");
    CMP_U(softOcDoFlag,      "softOcDoFlag");
    CMP_U(motorForeOT,       "motorForeOT");
    CMP_U(motorOT,           "motorOT");
    CMP_U(temperature,       "temperature");
    CMP_U(tickerTempDeal,    "tickerTempDeal");
    CMP_U(tempSampleSum,     "tempSampleSum");
    CMP_U(temperatureVoltage,       "temperatureVoltage");
    CMP_U(temperatureVoltageOrigin, "temperatureVoltageOrigin");

    CMP_U(diLogic,  "diLogic");
    CMP_U(vdiSrc,   "vdiSrc");
    CMP_U(vdiFcSet, "vdiFcSet");
    CMP_U(doLogic,  "doLogic");

    CMP_U(aoFmpValue,  "aoFmpValue");
    CMP_U(aoFmpMax,    "aoFmpMax");
    CMP_U(pulseInSet,  "pulseInSet");
    CMP_U(pulseInFrq,  "pulseInFrq");
    CMP_U(pulseInFrq1, "pulseInFrq1");
    CMP_U(lineSpeed,   "lineSpeed");

    CMP_U(capturePeriodSum, "capturePeriodSum");
    CMP_U(captureTicker,    "captureTicker");
    CMP_U(noCaptureTicker,  "noCaptureTicker");
    CMP_U(bOverFlowCounterTicker, "bOverFlowCounterTicker");
    CMP_U(bOverFlowLengthCurrent, "bOverFlowLengthCurrent");

    for (i = 0; i < AI_N; i++)
    {
        CMP_MEM(&A->m.aiLpf[i], &B->m.aiLpf[i], sizeof A->m.aiLpf[i], "aiLpf[]");
        CMP_MEM(&A->m.aiDeal[i], &B->m.aiDeal[i], sizeof A->m.aiDeal[i], "aiDeal[]");
    }
    CMP_MEM(&A->m.pulseInLpf, &B->m.pulseInLpf, sizeof A->m.pulseInLpf, "pulseInLpf");

    /* funcCode 既是输入也是输出 (PulseInCalc 写三个计数器字段)。 */
    CMP_MEM(&A->e.fc, &B->e.fc, sizeof A->e.fc, "funcCode");

    CMP_E(errCode,       "errorCode");
    CMP_E(errOther,      "errorOther");
    CMP_E(errDealStatus, "errorDealStatus");
    CMP_E(errAttr,       "errorAttribute");
    CMP_E(errAutoRstNum, "errAutoRstNum");
    CMP_E(dsp,           "dspStatus");

    CMP_E(frq,       "frq");
    CMP_E(frqAim,    "frqAim");
    CMP_E(frqRun,    "frqRun");
    CMP_E(frqDisp,   "frqDisp");
    CMP_E(maxFrq,    "maxFrq");
    CMP_E(upperFrq,  "upperFrq");
    CMP_E(lowerFrq,  "lowerFrq");
    CMP_E(swingFrqLimit, "swingFrqLimit");
    CMP_E(tuneCmd,   "tuneCmd");

    CMP_E(outCurrentDisp,    "outCurrentDisp");
    CMP_E(outPower,          "outPower");
    CMP_E(outVoltage,        "outVoltage");
    CMP_E(generatrixVoltage, "generatrixVoltage");
    CMP_E(itDisp,            "itDisp");

    CMP_E(ratingCurrent, "motorFc.ratingCurrent");
    CMP_E(ratingPower,   "motorFc.ratingPower");

    CMP_MEM(A->e.ao, B->e.ao, sizeof A->e.ao, "aoComm");
    CMP_E(doComm, "doComm");

    CMP_E(bPlcEndOneLoop, "bPlcEndOneLoop");
    CMP_E(bUv,            "bUv");
    CMP_E(curTime,        "curTime");
    CMP_E(setRunLostTime, "setRunLostTime");
    CMP_E(setRunTimeAim,  "setRunTimeAim");
    CMP_E(mainLoopTicker, "mainLoopTicker");

    CMP_MEM(&A->e.ecap1,    &B->e.ecap1,    sizeof A->e.ecap1,    "ECap1Regs");
    CMP_MEM(&A->e.epwm4,    &B->e.epwm4,    sizeof A->e.epwm4,    "EPwm4Regs");
    CMP_MEM(&A->e.epwm5,    &B->e.epwm5,    sizeof A->e.epwm5,    "EPwm5Regs");
    CMP_MEM(&A->e.epwm6,    &B->e.epwm6,    sizeof A->e.epwm6,    "EPwm6Regs");
    CMP_MEM(&A->e.gpioCtrl, &B->e.gpioCtrl, sizeof A->e.gpioCtrl, "GpioCtrlRegs");
    CMP_MEM(&A->e.gpioData, &B->e.gpioData, sizeof A->e.gpioData, "GpioDataRegs");
    CMP_MEM(A->e.ctl, B->e.ctl, sizeof A->e.ctl, "gHostCpuCtl (EALLOW/EDIS/DINT/EINT)");

    CMP_MEM(&A->e.diHw, &B->e.diHw, sizeof A->e.diHw, "diHwStatus");

    CMP_MEM(&A->line, &B->line, sizeof A->line, "LineCalc 的 p->y");
    CMP_MEM(&A->lpf,  &B->lpf,  sizeof A->lpf,  "LpfCalc 的 out/outOld/remainder");
}

/* ------------------------------------------------------------------------- */
/* 一例                                                                      */
/* ------------------------------------------------------------------------- */

enum
{
    K_DiCalc = 0, K_DoCalc, K_Dout, K_AiCalc, K_AoCalc,
    K_PulseInSample, K_PulseInCalc, K_FMPDeal, K_PulseOutCalc,
    K_LineCalc, K_LpfCalc,
    K_InitSetEPWM4, K_InitSetEPWM5, K_InitSetEPWM6, K_InitSetEcap1,
    K_InitDIGpio, K_InitDOGpio, K_InitEPwm4Gpio, K_InitEPwm5Gpio,
    K_InitEPwm6Gpio, K_InitECap1Gpio, K_InitECap2Gpio, K_InitECap3Gpio,
    K_InitECap4Gpio,
    K_TemperatureDeal, K_TemperatureSensorDeal, K_TemperatureAfterDeal,
    K_GetTemperature, K_GetTemperatureCalc,
    K_Di2Bin, K_Do2Bin, K_getDiHwStatus, K_setDOStatus, K_UpdateFmpAoValue,
    K_NUM
};

static const char *KIND_NAME[K_NUM] =
{
    "DiCalc", "DoCalc", "Dout", "AiCalc", "AoCalcChannel",
    "PulseInSample", "PulseInCalc", "FMPDeal", "PulseOutCalc",
    "LineCalc", "LpfCalc",
    "InitSetEPWM4", "InitSetEPWM5", "InitSetEPWM6", "InitSetEcap1",
    "InitDIGpio", "InitDOGpio", "InitEPwm4Gpio", "InitEPwm5Gpio",
    "InitEPwm6Gpio", "InitECap1Gpio", "InitECap2Gpio", "InitECap3Gpio",
    "InitECap4Gpio",
    "TemperatureDeal", "TemperatureSensorDeal", "TemperatureAfterDeal",
    "GetTemperature", "GetTemperatureCalc",
    "Di2Bin", "Do2Bin", "getDiHwStatus", "setDOStatus", "UpdateFmpAoValue"
};

static long g_kindN[K_NUM];

/* 42 个 Dout 入口, 顺序与 doFuncCal[] 一致。 */
static void (*const DOUT_REF[DO_FUNC_NUMBER])(void) =
{
    ref_DoutRSVD, ref_Dout1,  ref_Dout2,  ref_Dout3,  ref_Dout4,
    ref_Dout5,    ref_Dout6,  ref_Dout7,  ref_Dout8,  ref_Dout9,
    ref_Dout10,   ref_Dout11, ref_Dout12, ref_Dout13, ref_Dout14,
    ref_Dout15,   ref_Dout16, ref_Dout17, ref_Dout18, ref_Dout19,
    ref_DoutRSVD, ref_Dout21, ref_Dout22, ref_Dout23, ref_Dout24,
    ref_Dout25,   ref_Dout26, ref_Dout27, ref_Dout28, ref_Dout29,
    ref_Dout30,   ref_Dout31, ref_Dout32, ref_Dout33, ref_Dout34,
    ref_Dout35,   ref_Dout36, ref_Dout37, ref_Dout38, ref_Dout39,
    ref_Dout40,   ref_Dout41
};

static void (*const DOUT_NEW[DO_FUNC_NUMBER])(void) =
{
    new_DoutRSVD, new_Dout1,  new_Dout2,  new_Dout3,  new_Dout4,
    new_Dout5,    new_Dout6,  new_Dout7,  new_Dout8,  new_Dout9,
    new_Dout10,   new_Dout11, new_Dout12, new_Dout13, new_Dout14,
    new_Dout15,   new_Dout16, new_Dout17, new_Dout18, new_Dout19,
    new_DoutRSVD, new_Dout21, new_Dout22, new_Dout23, new_Dout24,
    new_Dout25,   new_Dout26, new_Dout27, new_Dout28, new_Dout29,
    new_Dout30,   new_Dout31, new_Dout32, new_Dout33, new_Dout34,
    new_Dout35,   new_Dout36, new_Dout37, new_Dout38, new_Dout39,
    new_Dout40,   new_Dout41
};

static void run1(const char *fn, int kind, Uint16 a0, Uint16 a1)
{
    int side;
    /* g_line / g_lpf 是**两侧共用**的入参对象。LineCalc 只写 p->y (每条路径都写,
     * 所以问题不大), 但 LpfCalc 会读 p->out 再写 p->out / p->outOld / p->remainder
     * —— 不还原的话第二侧拿到的就是第一侧的输出, 比较必然不等, 而且不等的原因
     * 与"改写对不对"毫无关系。每次 install 之前都恢复成驱动摆好的初值。 */
    LINE_STRUCT   line0 = g_line;
    LowPassFilter lpf0  = g_lpf;

    g_cases++;
    if ((kind >= 0) && (kind < K_NUM))
    {
        g_kindN[kind]++;
    }
    g_curKind = kind;
    g_curA0 = a0;
    g_curA1 = a1;

    for (side = 0; side < 2; side++)
    {
        g_line = line0;
        g_lpf  = lpf0;
        install(side);
        S[side].ret = 0;

        switch (kind)
        {
        case K_DiCalc:
            if (side) new_DiCalc(); else ref_DiCalc();
            break;
        case K_DoCalc:
            if (side) new_DoCalc(); else ref_DoCalc();
            break;
        case K_Dout:
            if (a0 < DO_FUNC_NUMBER)
            {
                if (side) DOUT_NEW[a0](); else DOUT_REF[a0]();
            }
            break;
        case K_AiCalc:
            if (side) new_AiCalc(); else ref_AiCalc();
            break;
        case K_AoCalc:
            if (side) new_AoCalcChannel(a0); else ref_AoCalcChannel(a0);
            break;
        case K_PulseInSample:
            if (side) new_PulseInSample(); else ref_PulseInSample();
            break;
        case K_PulseInCalc:
            if (side) new_PulseInCalc(); else ref_PulseInCalc();
            break;
        case K_FMPDeal:
            if (side) new_FMPDeal(); else ref_FMPDeal();
            break;
        case K_PulseOutCalc:
            if (side) new_PulseOutCalc(); else ref_PulseOutCalc();
            break;
        case K_LineCalc:
            if (side) new_LineCalc(&g_line); else ref_LineCalc(&g_line);
            break;
        case K_LpfCalc:
            if (side) new_LpfCalc(&g_lpf); else ref_LpfCalc(&g_lpf);
            break;
        case K_InitSetEPWM4:  if (side) new_InitSetEPWM4();  else ref_InitSetEPWM4();  break;
        case K_InitSetEPWM5:  if (side) new_InitSetEPWM5();  else ref_InitSetEPWM5();  break;
        case K_InitSetEPWM6:  if (side) new_InitSetEPWM6();  else ref_InitSetEPWM6();  break;
        case K_InitSetEcap1:  if (side) new_InitSetEcap1();  else ref_InitSetEcap1();  break;
        case K_InitDIGpio:    if (side) new_InitDIGpio();    else ref_InitDIGpio();    break;
        case K_InitDOGpio:    if (side) new_InitDOGpio();    else ref_InitDOGpio();    break;
        case K_InitEPwm4Gpio: if (side) new_InitEPwm4Gpio(); else ref_InitEPwm4Gpio(); break;
        case K_InitEPwm5Gpio: if (side) new_InitEPwm5Gpio(); else ref_InitEPwm5Gpio(); break;
        case K_InitEPwm6Gpio: if (side) new_InitEPwm6Gpio(); else ref_InitEPwm6Gpio(); break;
        case K_InitECap1Gpio: if (side) new_InitECap1Gpio(); else ref_InitECap1Gpio(); break;
        case K_InitECap2Gpio: if (side) new_InitECap2Gpio(); else ref_InitECap2Gpio(); break;
        case K_InitECap3Gpio: if (side) new_InitECap3Gpio(); else ref_InitECap3Gpio(); break;
        case K_InitECap4Gpio: if (side) new_InitECap4Gpio(); else ref_InitECap4Gpio(); break;
        case K_TemperatureDeal:
            if (side) new_TemperatureDeal(); else ref_TemperatureDeal();
            break;
        case K_TemperatureSensorDeal:
            if (side) new_TemperatureSensorDeal(); else ref_TemperatureSensorDeal();
            break;
        case K_TemperatureAfterDeal:
            if (side) new_TemperatureAfterDeal(); else ref_TemperatureAfterDeal();
            break;
        case K_GetTemperature:
            if (side) new_GetTemperature(); else ref_GetTemperature();
            break;
        case K_GetTemperatureCalc:
            S[side].ret = side ? new_GetTemperatureCalc(a0, g_tbl, a1)
                               : ref_GetTemperatureCalc(a0, g_tbl, a1);
            break;
        case K_Di2Bin:
            if (side) new_Di2Bin(); else ref_Di2Bin();
            break;
        case K_Do2Bin:
            if (side) new_Do2Bin(); else ref_Do2Bin();
            break;
        case K_getDiHwStatus:
            if (side) new_getDiHwStatus(); else ref_getDiHwStatus();
            break;
        case K_setDOStatus:
            if (side) new_setDOStatus(); else ref_setDOStatus();
            break;
        case K_UpdateFmpAoValue:
            if (side) new_UpdateFmpAoValue(a0, a1); else ref_UpdateFmpAoValue(a0, a1);
            break;
        default:
            break;
        }

        snapshot(side);
    }

    compare(fn);
}

/* ------------------------------------------------------------------------- */
/* 输入构造                                                                  */
/* ------------------------------------------------------------------------- */

static void zero_all(void)
{
    memset(&g_inM, 0, sizeof g_inM);
    memset(&g_inE, 0, sizeof g_inE);
}

/* 外设初值: 每一位都置成"非默认", 这样"改写漏写某个寄存器"立刻可见。
 *
 * 注意 ECap1Regs 的 CAP1..CAP4 在 PulseInSample 里是**输入** (非零才算一次捕获),
 * 所以这里刻意给非零值; 而 doHwStatus / doStatus 之类的状态位在别的 phase 里
 * 另有专门取值, 这里只保证"不是 0"。 */
static void prefill_env(void)
{
    int i;

    memset(&g_inE.ecap1,     0x5A, sizeof g_inE.ecap1);
    memset(&g_inE.epwm4,     0x3C, sizeof g_inE.epwm4);
    memset(&g_inE.epwm5,     0xC3, sizeof g_inE.epwm5);
    memset(&g_inE.epwm6,     0x96, sizeof g_inE.epwm6);
    memset(&g_inE.gpioCtrl,  0x69, sizeof g_inE.gpioCtrl);
    memset(&g_inE.gpioData,  0x00, sizeof g_inE.gpioData);
    for (i = 0; i < 7; i++)
    {
        g_inE.ctl[i] = 0;
    }

    g_inE.run.all          = 0;
    g_inE.errCode          = 0;
    g_inE.errOther         = 0;
    g_inE.errDealStatus    = ERROR_DEAL_OK;
    g_inE.errAttr.all      = 0;
    g_inE.errAutoRstNum    = 0;
    g_inE.dsp.all          = 0;

    g_inE.frq              = 0;
    g_inE.frqAim           = 0;
    g_inE.frqRun           = 0;
    g_inE.frqDisp          = 100;
    /* maxFrq 是 UpdateFmpAoValue 的 aoFmpMax, 而 AoCalcChannel 立刻拿 aoFmpMax
     * 做除数 —— 0 就是除零。funcCode 的校验保证它非零, 驱动照这个前提走。 */
    g_inE.maxFrq           = 1000;
    g_inE.upperFrq         = 500;
    g_inE.lowerFrq         = 10;
    g_inE.swingFrqLimit    = 0;
    g_inE.tuneCmd          = 0;

    g_inE.outCurrentDisp   = 100;
    g_inE.outPower         = 50;
    g_inE.outVoltage       = 300;
    g_inE.generatrixVoltage = 400;
    g_inE.itDisp           = 200;

    g_inE.ratingCurrent    = 1000;
    g_inE.ratingPower      = 2000;

    for (i = 0; i < AO_N; i++)
    {
        g_inE.ao[i] = (Uint16)(100 + i);
    }
    g_inE.doComm.all       = 0;

    g_inE.bPlcEndOneLoop   = 0;
    g_inE.bUv              = 0;

    memset(&g_inE.curTime, 0, sizeof g_inE.curTime);
    g_inE.curTime.runTime  = 0;

    g_inE.setRunLostTime   = 0;
    g_inE.setRunTimeAim    = 0;
    g_inE.mainLoopTicker   = 0;
    g_inE.torqueRet        = 0;

    g_inE.diHw.all         = 0;
}

/* funcCode 的预填: 每个本模块**读**的字段都给非平凡值。
 *
 * 不 memset 成随机字节: 那些数组字段会被当功能码用 (见文件头"输入必须留在
 * 定义域内"), 随机值等于把越界写喂进去。所以逐字段给安全值, 再让 phase 覆盖。
 */
static void prefill_fc(void)
{
    FUNCCODE_ALL *f = &g_inE.fc;
    int i;

    memset(f, 0, sizeof *f);

    f->code.diFilterTime = 20;              /* /DI_CALC_PERIOD(2) = 10 轮去抖 */
    for (i = 0; i < DI_NUMBER_PHSIC; i++)
    {
        f->code.diFunc[i] = (Uint16)(i % DI_FUNC_NUMBER);
    }
    for (i = 0; i < DI_NUMBER_V; i++)
    {
        f->code.vdiFunc[i] = (Uint16)((i + 10) % DI_FUNC_NUMBER);
    }
    for (i = 0; i < DI_NUMBER_AI_AS_DI; i++)
    {
        f->code.aiAsDiFunc[i] = (Uint16)((i + 15) % DI_FUNC_NUMBER);
    }
    f->code.diDelayTime[0] = 1;
    f->code.diDelayTime[1] = 2;
    f->code.diDelayTime[2] = 3;

    f->code.frqXSrc    = 0;
    f->code.frqYSrc    = 0;
    f->code.upperFrqSrc = 0;
    f->code.pidSetSrc  = 0;
    f->code.pidFdbSrc  = 0;
    f->code.plcFrq0Src = 0;

    for (i = 0; i < DO_NUMBER_PHSIC; i++)
    {
        f->code.doFunc[i]      = (Uint16)(i % DO_FUNC_NUMBER);
        f->code.doDelayTime[i] = 1;
    }
    for (i = 0; i < DO_NUMBER_V; i++)
    {
        f->code.vdoFunc[i]      = (Uint16)((i + 5) % DO_FUNC_NUMBER);
        f->code.vdoDelayTime[i] = 1;
    }

    f->code.fanControl    = 0;
    f->code.outPortControl = 0;
    f->code.plcEnable     = 0;
    f->code.inPortOut     = 0;

    f->code.motorOtMode   = FUNCCODE_tempSenorType_PTC100;

    /* 标定曲线: 每条都保证 before1 != before2 / 相邻点不同 —— LineCalc 的
     * `tmp = ... / (p->x2 - p->x1)` 在**所有分支之前**无条件算, 相等就是除零。 */
    for (i = 0; i < AI_NUMBER; i++)
    {
        f->code.aiCalibrateCurve[i].before1 = (Uint16)(100 + i * 10);
        f->code.aiCalibrateCurve[i].after1  = (Uint16)(200 + i * 10);
        f->code.aiCalibrateCurve[i].before2 = (Uint16)(3000 + i * 10);
        f->code.aiCalibrateCurve[i].after2  = (Uint16)(2000 + i * 10);
    }

    f->code.curveSet2P1[0] = 100;  f->code.curveSet2P1[1] = 50;
    f->code.curveSet2P1[2] = 3000; f->code.curveSet2P1[3] = 900;
    f->code.curveSet2P2[0] = 120;  f->code.curveSet2P2[1] = 60;
    f->code.curveSet2P2[2] = 2800; f->code.curveSet2P2[3] = 950;
    f->code.curveSet2P3[0] = 140;  f->code.curveSet2P3[1] = 70;
    f->code.curveSet2P3[2] = 2600; f->code.curveSet2P3[3] = 1000;
    f->code.curveSet2P4[0] = 200;  f->code.curveSet2P4[1] = 100;
    f->code.curveSet2P4[2] = 4000; f->code.curveSet2P4[3] = 1200;

    /* curveSet4P* 有 8 项, AiCalc 在 aiNum >= 4 时会先按电压找到 i, 再用
     * p += i*2 从 (0,2,4) 里挑一段, 最多读到 p[4..7] —— 8 项刚好够。 */
    for (i = 0; i < 8; i++)
    {
        f->code.curveSet4P1[i] = (Uint16)(10 + i * 37);
        f->code.curveSet4P2[i] = (Uint16)(20 + i * 41);
    }

    /* aiCurveSrc 的每个半字节都是 pAiSetCurve[] 的下标 +1, 必须落在 1..5。 */
    f->code.aiCurveSrc = 0x1111;
    f->code.aiLimitSrc = 0x0000;

    for (i = 0; i < AI_NUMBER; i++)
    {
        f->code.aiJumpSet[i].point   = 500;
        f->code.aiJumpSet[i].arrange = 50;
    }

    for (i = 0; i < AO_NUMBER; i++)
    {
        f->code.aoCalibrateCurve[i].before1 = (Uint16)(100 + i * 10);
        f->code.aoCalibrateCurve[i].after1  = (Uint16)(200 + i * 10);
        f->code.aoCalibrateCurve[i].before2 = (Uint16)(3000 + i * 10);
        f->code.aoCalibrateCurve[i].after2  = (Uint16)(2000 + i * 10);

        f->code.aoPara[i].gain   = 300;
        f->code.aoPara[i].offset = 5;
    }
    f->code.aoFunc[AOFMP_FMP] = AO_FMP_FUNC_FRQ_SET;
    f->code.aoFunc[AOFMP_AO1] = AO_FMP_FUNC_FRQ_AIM;
    f->code.aoFunc[AOFMP_AO2] = AO_FMP_FUNC_OUT_CURRENT;
    f->code.ao1Value          = 500;

    f->code.fmOutSelect  = FUNCCODE_fmOutSelect_PULSE;
    f->code.fmpValue     = 300;
    f->code.fmpOutMaxFrq = 1000;

    f->code.ai1FilterTime = 5;
    f->code.ai2FilterTime = 5;
    f->code.ai3FilterTime = 5;

    f->code.ai1VoltageUpper = 900;
    f->code.ai1VoltageLimit = 100;

    f->code.counterTicker = 0;
    f->code.counterSet    = 1000;
    f->code.counterPoint  = 500;

    f->code.lengthCurrent       = 0;
    f->code.lengthSet           = 1000;
    f->code.lengthPulsePerMeter = 100;   /* 除数, 不能是 0 */
    f->code.lengthTickerRemainder = 0;

    f->code.runTimeAddup      = 0;
    f->code.runTimeArriveSet  = 0;
    f->code.powerUpTimeAddup  = 0;
    f->code.powerUpTimeArriveSet = 0;

    f->code.frqFdtValue   = 100;
    f->code.frqFdtLag     = 100;
    f->code.frqFdt1Value  = 200;
    f->code.frqFdt1Lag    = 100;

    f->code.frqArriveRange   = 10;
    f->code.frqArriveValue1  = 300;
    f->code.frqArriveRange1  = 10;
    f->code.frqArriveValue2  = 400;
    f->code.frqArriveRange2  = 10;

    f->code.currentArriveValue1 = 500;
    f->code.currentArriveRange1 = 50;
    f->code.currentArriveValue2 = 600;
    f->code.currentArriveRange2 = 50;

    f->code.oCurrentChkValue = 300;
    f->code.oCurrentChkTime  = 5;

    f->code.radiatorTemp      = 30;
    f->code.temperatureArrive = 50;
    f->code.setTimeArrive     = 0;

    f->code.motorOtCoef    = 60;
    f->code.motorOtProtect = 80;

    f->code.doLogic      = 0;
    f->code.vdoLogic     = 0;
    f->code.diLogic[0]   = 0;
    f->code.diLogic[1]   = 0;
    f->code.diLogicAiAsDi = 0;
    f->code.vdiSrc       = 0;
    f->code.vdiFcSet     = 0;

    f->code.errAutoRstRelayAct = 0;
    f->code.errAutoRstNumMax   = 5;
}

/* 模块状态的预填: 每个字段都有值, 且互不相同。改写如果漏写某个字段, 它会保留
 * 这里的值, 与另一侧写过的值不同 -> 立刻可见。 */
static void prefill_state(void)
{
    int i;

    memset(&g_inM, 0, sizeof g_inM);

    g_inM.diFunc.f1.all      = 0x0F0F0F0FUL;
    g_inM.diFunc.f2.all      = 0x00FF00FFUL;
    g_inM.diSelectFunc.f1.all = 0x12345678UL;
    g_inM.diSelectFunc.f2.all = 0x9ABCDEF0UL;
    g_inM.diStatus.a.all     = 0x000002AAUL;
    g_inM.diStatus.b.all     = 0x00000155UL;
    g_inM.diStatus.c.all     = 0x000003FFUL;

    for (i = 0; i < 3; i++)
    {
        g_inM.diDelayTicker.all[i].high = (Uint32)(10 + i);
        g_inM.diDelayTicker.all[i].low  = (Uint32)(20 + i);
    }

    g_inM.doFunc.f1.all      = 0x0F0F0F0FUL;
    g_inM.doFunc.f2.all      = 0x00FF00FFUL;
    g_inM.doStatus.a.all     = 0x0055;
    g_inM.doStatus.b.all     = 0x00AA;
    g_inM.doStatus.c.all     = 0x03FF;
    g_inM.doHwStatus.all     = 0x00A5;

    for (i = 0; i < DO_N; i++)
    {
        g_inM.doDelayTicker.all[i].high = (Uint32)(30 + i);
        g_inM.doDelayTicker.all[i].low  = (Uint32)(40 + i);
    }

    g_inM.oCurrentCheckFlag = 1;
    g_inM.oCurrentChkTicker = 7;
    g_inM.softOcDoFlag      = 0;
    g_inM.motorForeOT       = 1;
    g_inM.motorOT           = 1;
    g_inM.temperature       = 55;
    g_inM.tickerTempDeal    = 13;
    g_inM.tempSampleSum     = 12345;
    g_inM.temperatureVoltage = 1500;
    g_inM.temperatureVoltageOrigin = 1600;

    g_inM.diLogic = 0x000000FFUL;
    g_inM.vdiSrc  = 0x0015;
    g_inM.vdiFcSet = 0x000A;

    g_inM.doLogic = 0x0015;

    g_inM.aoFmpValue = 250;
    g_inM.aoFmpMax   = 1000;
    g_inM.pulseInSet = 111;
    g_inM.pulseInFrq = 2222;
    g_inM.pulseInFrq1 = 3333;
    g_inM.lineSpeed  = 444;

    g_inM.capturePeriodSum = 5000;
    g_inM.captureTicker    = 3;
    g_inM.noCaptureTicker  = 2;
    g_inM.bOverFlowCounterTicker = 0;
    g_inM.bOverFlowLengthCurrent = 0;

    for (i = 0; i < AI_N; i++)
    {
        g_inM.aiLpf[i].t         = (int32)(5 + i);
        g_inM.aiLpf[i].in        = (int32)(100 + i * 10);
        g_inM.aiLpf[i].out       = (int32)(200 + i * 10);
        g_inM.aiLpf[i].outOld    = (int32)(150 + i * 10);
        g_inM.aiLpf[i].remainder = (int32)(7 + i);

        g_inM.aiDeal[i].sample        = (Uint16)(1000 + i * 100);
        g_inM.aiDeal[i].voltageOrigin = (int16)(500 + i * 10);
        g_inM.aiDeal[i].voltage       = (int16)(400 + i * 10);
        g_inM.aiDeal[i].set           = (int16)(300 + i * 10);
    }

    g_inM.pulseInLpf.t         = 5;
    g_inM.pulseInLpf.in        = 900;
    g_inM.pulseInLpf.out       = 800;
    g_inM.pulseInLpf.outOld    = 700;
    g_inM.pulseInLpf.remainder = 3;
}

static void reset_case(void)
{
    zero_all();
    prefill_fc();
    prefill_state();
    prefill_env();
}

/* 把 GpioDataRegs 的十个 DI 引脚按位掩码摆好。
 *
 * 位序就是 getDiHwStatus 里的读取顺序: di1..di10 =
 *   GPBDAT.42 / GPBDAT.43 / GPADAT.27 / GPBDAT.40 / GPADAT.19 /
 *   GPADAT.25 / GPBDAT.44 / GPBDAT.35 / GPBDAT.36 / GPBDAT.38 */
static void set_di_pins(Uint16 mask)
{
    /* 注意: GpioDataRegs 是**对象名**不是类型名 (真头里是
     * `extern volatile struct GPIO_DATA_REGS GpioDataRegs;`), 所以局部变量
     * 必须写 `struct GPIO_DATA_REGS gpio;`。写成 `GpioDataRegs gpio;` 会报
     * C2065 'gpio' 未声明 + C2224 .GPBDAT 不是结构成员。 */
    struct GPIO_DATA_REGS gpio;

    memset(&gpio, 0, sizeof gpio);

    gpio.GPBDAT.bit.GPIO42 = (Uint16)((mask >> 0) & 1);
    gpio.GPBDAT.bit.GPIO43 = (Uint16)((mask >> 1) & 1);
    gpio.GPADAT.bit.GPIO27 = (Uint16)((mask >> 2) & 1);
    gpio.GPBDAT.bit.GPIO40 = (Uint16)((mask >> 3) & 1);
    gpio.GPADAT.bit.GPIO19 = (Uint16)((mask >> 4) & 1);
    gpio.GPADAT.bit.GPIO25 = (Uint16)((mask >> 5) & 1);
    gpio.GPBDAT.bit.GPIO44 = (Uint16)((mask >> 6) & 1);
    gpio.GPBDAT.bit.GPIO35 = (Uint16)((mask >> 7) & 1);
    gpio.GPBDAT.bit.GPIO36 = (Uint16)((mask >> 8) & 1);
    gpio.GPBDAT.bit.GPIO38 = (Uint16)((mask >> 9) & 1);

    g_inE.gpioData = gpio;
}

/* ------------------------------------------------------------------------- */
/* phase: DiCalc                                                             */
/* ------------------------------------------------------------------------- */

static void phase_dicalc(void)
{
    static const Uint16 pins[] = { 0x000, 0x3FF, 0x155, 0x2AA, 0x001, 0x200 };
    static const Uint16 fts[]  = { 0, 1, 2, 20, 100, 0xFFFF };
    static const Uint16 asts[] = { 0x000, 0x3FF, 0x2AA, 0x155 };
    static const int16  vols[] = { 0, 299, 300, 301, 699, 700, 701, 1000 };
    static const Uint16 dly[]  = { 0, 1, 2, 10 };
    unsigned p, f, a, v, d, k;

    for (p = 0; p < sizeof pins / sizeof pins[0]; p++)
    {
        for (f = 0; f < sizeof fts / sizeof fts[0]; f++)
        {
            for (a = 0; a < sizeof asts / sizeof asts[0]; a++)
            {
                for (v = 0; v < sizeof vols / sizeof vols[0]; v++)
                {
                    reset_case();
                    set_di_pins(pins[p]);
                    g_inE.fc.code.diFilterTime = fts[f];
                    g_inM.diStatus.a.all = asts[a];
                    g_inE.diHw.all = 0;
                    g_inM.aiDeal[0].voltage = vols[v];
                    g_inM.aiDeal[1].voltage = (int16)(vols[v] ^ 0x00FF);
                    g_inM.aiDeal[2].voltage = (int16)(vols[v] ^ 0x0100);

                    /* 连跑 4 次: diXFilterCounter[] 是函数内 static, 只有连续
                     * 调用才会真的把去抖计数器推过门槛。 */
                    for (k = 0; k < 4; k++)
                    {
                        run1("DiCalc", K_DiCalc, 0, 0);
                    }
                }
            }
        }
    }

    /* 虚拟 DI: vdiSrc 决定 vdi 是取 doStatus.c 的高位还是 vdiFcSet。 */
    for (p = 0; p < 32; p++)
    {
        reset_case();
        set_di_pins(0x155);
        g_inM.vdiSrc  = (Uint16)(p * 0x0421);
        g_inM.vdiFcSet = (Uint16)(p * 0x1234);
        g_inM.doStatus.c.all = (Uint32)(p * 0x0F0F0F0FUL);
        run1("DiCalc/vdi", K_DiCalc, 0, 0);
    }

    /* 去抖延迟: delayTickerMax = diDelayTime[i] * (100 / 2) = 50 * n。
     * 门槛正好落在 50 / 100 / 150 —— 扫这三个值和它们的两侧。 */
    for (d = 0; d < sizeof dly / sizeof dly[0]; d++)
    {
        for (k = 0; k < 4; k++)
        {
            reset_case();
            set_di_pins(0x000);
            g_inE.fc.code.diDelayTime[0] = dly[d];
            g_inE.fc.code.diDelayTime[1] = dly[d];
            g_inE.fc.code.diDelayTime[2] = dly[d];
            g_inM.diStatus.a.all = (Uint32)(0x7 ^ (k & 1));
            g_inM.diStatus.b.all = (Uint32)(k * 0x1111);
            run1("DiCalc/delay", K_DiCalc, 0, 0);
        }
    }

    /* 去抖计数器只有"加一之后**正好等于**门槛"才分得出 `>=` 与 `>`。
     * 上面那组只摆了 diDelayTime, 而 diDelayTicker.high 的预填是 10 + i,
     * 加一次最多到 13, 离 50 / 100 / 500 还差得远 —— 门槛一次都没踩上。
     * 这里把 high 直接摆到 delayTickerMax - 1。 */
    {
        static const Uint16 dts[] = { 1, 2, 10 };
        unsigned t, i;

        for (t = 0; t < sizeof dts / sizeof dts[0]; t++)
        {
            reset_case();
            set_di_pins(0x3FF);
            g_inM.diStatus.a.all = 0x00000007UL;
            g_inM.diStatus.b.all = 0x00000000UL;
            for (i = 0; i < 3; i++)
            {
                g_inE.fc.code.diDelayTime[i]    = dts[t];
                g_inM.diDelayTicker.all[i].high = (Uint32)(dts[t] * 50 - 1);
            }
            run1("DiCalc/delayEdge", K_DiCalc, 0, 0);
        }
    }

    /* AI 当 DI 的三位在 diStatus.a 的 bit 15..17 (10 路 PHSIC + 5 路 V 之后),
     * 而上面 asts[] 最大只到 0x3FF —— 这三位从来没被置 1 过, "清位"于是完全
     * 不可观测 (清一个本来就是 0 的位)。门槛 300 / 700 又只有电压**正好等于**
     * 门槛才分得出 `<=` 与 `<`。 */
    {
        static const int16 volts[] = { 299, 300, 301, 699, 700, 701 };
        unsigned v, i;

        for (v = 0; v < sizeof volts / sizeof volts[0]; v++)
        {
            reset_case();
            set_di_pins(0x3FF);
            g_inM.diStatus.a.all = 0x00038000UL;   /* bit 15 / 16 / 17 */
            g_inM.diStatus.b.all = 0x00000000UL;
            g_inM.diStatus.c.all = 0x00000000UL;
            g_inE.fc.code.diFilterTime = 0;
            for (i = 0; i < AI_NUMBER; i++)
            {
                g_inM.aiDeal[i].voltage = volts[v];
                g_inE.fc.code.aiAsDiFunc[i] = (Uint16)(2 + i * 2);
            }
            run1("DiCalc/aiAsDi", K_DiCalc, 0, 0);
        }
    }

    /* 脉冲源冲突: diFunc[4] 不是 25/27 且某个 frq/pid 源选了 PULSE 时,
     * DiCalc 结尾会把 diFunc 的那一位清掉。 */
    {
        static const Uint16 srcs[] =
        {
            0, FUNCCODE_frqXySrc_PULSE, FUNCCODE_frqXySrc_PULSE + 1,
        };
        unsigned s;

        for (s = 0; s < 3; s++)
        {
            reset_case();
            g_inE.fc.code.diFunc[4] = 3;
            g_inE.fc.code.frqXSrc   = srcs[s];
            g_inE.fc.code.frqYSrc   = srcs[(s + 1) % 3];
            g_inE.fc.code.upperFrqSrc = srcs[(s + 2) % 3];
            g_inE.fc.code.pidSetSrc = srcs[s];
            g_inE.fc.code.pidFdbSrc = srcs[s];
            g_inE.fc.code.plcFrq0Src = srcs[s];
            run1("DiCalc/pulseSrc", K_DiCalc, 0, 0);

            reset_case();
            g_inE.fc.code.diFunc[4] = DI_FUNC_COUNTER_TICKER_IN;
            g_inE.fc.code.frqXSrc   = FUNCCODE_frqXySrc_PULSE;
            run1("DiCalc/pulseSrc25", K_DiCalc, 0, 0);

            reset_case();
            g_inE.fc.code.diFunc[4] = DI_FUNC_LENGTH_TICKER_IN;
            g_inE.fc.code.frqXSrc   = FUNCCODE_frqXySrc_PULSE;
            run1("DiCalc/pulseSrc27", K_DiCalc, 0, 0);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase: Dout (42 个入口逐个驱动)                                            */
/* ------------------------------------------------------------------------- */

/* 驱动侧自己用的小工具, 与被测代码无关。 */
#define FRQ_ABS(v)  (((v) < 0) ? (-(v)) : (v))

static void phase_dout(void)
{
    static const int32  frqs[]  = { 0, 50, 100, 200, -50, -200, 1000 };
    static const Uint16 runs[]  = { 0, 1 };
    static const Uint16 errs[]  = { 0, 0x0001, ERROR_UV, 0x0100 };
    static const Uint16 deals[] = { 0, 1, 2, 3 };
    static const Uint16 lvls[]  = { 0, 1, 2, 3 };
    static const Uint16 bits[]  = { 0, 1 };
    /* doFunc 是 Dout 的**输出**: 每个 DoutN 只把自己那一位**置 1**
     * (Dout36 例外, 它会写 0)。prefill_state 原来把它预填成
     * 0x0F0F0F0F / 0x00FF00FF, 而 counterSetArrive(bit8) / counterPointArrive(bit9) /
     * lengthArrive(bit10) / ai1GreaterThanAi2(bit16) / upperFrqArrive(bit17) /
     * lowerFrqArrive(bit18) / powerUpTimeArrive(bit24) / frqArrive1(bit26) /
     * frqArrive2(bit27) 恰好都在这个掩码里**已经是 1** —— "再置一次"于是完全
     * 不可观测, 一批边界运算符变异因此假 MISSED。
     *
     * 输出量要两种预填都跑: 全 0 才看得见"置 1", 全 1 才看得见"写 0"。
     * 只用其中一种都会漏掉一半方向的错写。 */
    static const Uint32 dopre[] = { 0x00000000UL, 0xFFFFFFFFUL };
    unsigned i, a, b, c, d, e, f, pf;

    for (i = 0; i < DO_FUNC_NUMBER; i++)
    {
      for (pf = 0; pf < 2; pf++)
      {
        for (a = 0; a < 2; a++)
        {
            for (b = 0; b < 2; b++)
            {
                for (c = 0; c < 4; c++)
                {
                    for (d = 0; d < 4; d++)
                    {
                        for (e = 0; e < 2; e++)
                        {
                            for (f = 0; f < 2; f++)
                            {
                                reset_case();

                                g_inM.doFunc.f1.all = dopre[pf];
                                g_inM.doFunc.f2.all = dopre[pf];

                                g_inE.run.bit.run = runs[a];
                                g_inE.run.bit.dir = (Uint16)(b & 1);
                                g_inE.frq         = frqs[(c + a) % 7];
                                g_inE.frqAim      = frqs[(d + b) % 7];
                                g_inE.frqRun      = frqs[(e + c) % 7];
                                g_inE.frqDisp     = (Uint16)(100 + c * 10);
                                g_inE.tuneCmd     = bits[f];

                                g_inE.errCode       = errs[c];
                                g_inE.errDealStatus = (enum ERROR_DEAL_STATUS)deals[d];
                                /* errorAttribute 的位域是 reset:1 / level:3, 所以
                                 * level 在 bit1..3。Dout2 / Dout41 只在
                                 * level != ERROR_LEVEL_RUN(3) 时置位, 而 3 正好是
                                 * 该字段的满量程 —— 必须扫到。 */
                                g_inE.errAttr.all   = (Uint16)((lvls[e] << 1) | (a & 1));
                                g_inE.errAutoRstNum = (Uint16)(d * 2);
                                g_inE.fc.code.errAutoRstRelayAct = bits[a];
                                g_inE.fc.code.errAutoRstNumMax   = (Uint16)(1 + c);

                                g_inE.dsp.all = (Uint16)(c * 0x0101);

                                g_inE.bUv = bits[b];
                                g_inM.diFunc.f1.bit.closePwm = bits[e];

                                g_inE.outCurrentDisp = (Uint16)(c * 400);
                                g_inE.itDisp         = (Uint16)(c * 500);
                                g_inE.outPower       = (int16)(c * 100);
                                g_inE.outVoltage     = (Uint16)(c * 100);
                                g_inE.generatrixVoltage = (Uint16)(c * 120);

                                g_inE.fc.code.counterTicker = (Uint16)(c * 500);
                                g_inE.fc.code.lengthCurrent = (Uint16)(c * 500);
                                g_inM.bOverFlowCounterTicker = bits[e];
                                g_inM.bOverFlowLengthCurrent = bits[f];

                                g_inE.fc.code.runTimeAddup     = (Uint16)(c * 500);
                                g_inE.fc.code.runTimeArriveSet = (Uint16)(d * 500);
                                g_inE.fc.code.powerUpTimeAddup = (Uint16)(c * 500);
                                g_inE.fc.code.powerUpTimeArriveSet = (Uint16)(d * 500);

                                g_inE.fc.code.setTimeArrive = (Uint16)(d * 100);
                                g_inE.curTime.runTime       = (Uint16)(c * 200);

                                g_inE.swingFrqLimit = bits[f];
                                g_inE.torqueRet     = bits[e];

                                g_inE.setRunLostTime = bits[a];
                                g_inE.setRunTimeAim  = (Uint16)(d * 100);

                                g_inE.fc.code.oCurrentChkValue = (Uint16)(c * 200);
                                g_inE.fc.code.oCurrentChkTime  = (Uint16)(d * 2);
                                g_inM.oCurrentCheckFlag = bits[b];
                                g_inM.oCurrentChkTicker = (Uint32)(c * 3);

                                g_inE.fc.code.radiatorTemp      = (Uint16)(c * 20);
                                g_inE.fc.code.temperatureArrive = (Uint16)(d * 20);
                                g_inM.softOcDoFlag = bits[f];
                                g_inM.motorForeOT  = bits[e];

                                g_inM.aiDeal[0].set     = (int16)(c * 100);
                                g_inM.aiDeal[1].set     = (int16)(d * 100);
                                g_inM.aiDeal[0].voltage = (int16)(c * 400);

                                g_inE.fc.code.frqFdtValue  = (Uint16)(c * 100);
                                g_inE.fc.code.frqFdt1Value = (Uint16)(d * 100);
                                g_inE.fc.code.frqFdtLag    = (Uint16)(e * 500);
                                g_inE.fc.code.frqFdt1Lag   = (Uint16)(f * 500);

                                /* Dout17 / Dout18 / Dout37 拿 upperFrq / lowerFrq 跟
                                 * |frq| 比, 而 `>=` 与 `>` 只在**恰好相等**处分歧。
                                 * prefill_env 把 upperFrq 钉在 500、lowerFrq 钉在 10,
                                 * 两个数都不在 frqs[] 里, 相等点一次都到不了 ——
                                 * 门槛必须取自 frqs[] 本身。 */
                                g_inE.upperFrq = (Uint16)FRQ_ABS(frqs[(c + a) % 7]);
                                g_inE.lowerFrq = (d & 1) ? 0
                                               : (Uint16)FRQ_ABS(frqs[(c + a) % 7]);

                                /* Dout26 / Dout27 的窗口是 [V1 - d, V1 + d],
                                 * d = maxFrq * range / 1000。默认 V1 = 300 同样不在
                                 * frqs[] 里, 窗口一次都没进过。V1 摆到 frqs 上, 并且
                                 * 让 d 取 50 (不是 0) —— d == 0 时 `+ d` 与 `- d`
                                 * 是同一个数, 那是可证等价。 */
                                g_inE.fc.code.frqArriveValue1 = (Uint16)FRQ_ABS(frqs[(c + a) % 7]);
                                g_inE.fc.code.frqArriveValue2 = (Uint16)FRQ_ABS(frqs[(c + a) % 7]);
                                g_inE.fc.code.frqArriveRange1 = (Uint16)((d & 1) ? 50 : 0);
                                g_inE.fc.code.frqArriveRange2 = (Uint16)((d & 1) ? 0 : 50);

                                run1("Dout", K_Dout, (Uint16)i, 0);
                            }
                        }
                    }
                }
            }
        }
      }
    }

    /* Dout3 / Dout25 的滞后带: 阈值 * (1000 - lag) / 1000 是**下**门槛,
     * 只有把 frq 摆到门槛上下两侧, 才分得出 `<` 与 `<=`。 */
    {
        static const int32 fr[] = { 0, 99, 100, 101, 199, 200, 201, -100, -101 };
        unsigned x;

        for (x = 0; x < sizeof fr / sizeof fr[0]; x++)
        {
            reset_case();
            g_inE.run.bit.run = 1;
            g_inE.frq = fr[x];
            g_inE.fc.code.frqFdtValue  = 100;
            g_inE.fc.code.frqFdtLag    = 100;   /* 下门槛 90 */
            g_inE.fc.code.frqFdt1Value = 200;
            g_inE.fc.code.frqFdt1Lag   = 500;   /* 下门槛 100 */
            run1("Dout3/lag", K_Dout, 3, 0);
            run1("Dout25/lag", K_Dout, 25, 0);
        }
    }

    /* Dout30 的 arriveTcnt 是 static, 初值 0; 置位后每调一次减一, 减到 0 为止。
     * 连跑 252 次刚好跨过 SET_TIME_ARRIVE_TCNT_MAX (250)。 */
    {
        unsigned k;

        reset_case();
        g_inE.setRunLostTime = 0;
        g_inE.setRunTimeAim  = 1;
        for (k = 0; k < 253; k++)
        {
            run1("Dout30/count", K_Dout, 30, 0);
        }

        reset_case();
        g_inE.setRunLostTime = 1;
        g_inE.setRunTimeAim  = 1;
        /* 3 次远远不够: arriveTcnt 这时是 249 (ref) / 248 (变异), 要一路减到 0
         * 才看得见"少 1" —— 300 次才跨过去。原来写 3 次, 两次调用都还 > 0,
         * `SET_TIME_ARRIVE_TCNT_MAX` 与 `MAX - 1` 完全等价, 于是假 MISSED。 */
        for (k = 0; k < 300; k++)
        {
            run1("Dout30/hold", K_Dout, 30, 0);
        }
    }

    /* Dout34 的 oCurrentChkTicker 上限 = oCurrentChkTime * 10 / 2 = 5 * time。
     * 门槛只在"差值正好等于门槛"处分得出 >= 与 >, 所以扫整除因子。 */
    {
        static const Uint16 tms[] = { 0, 1, 2, 4, 5, 10 };
        unsigned t, k;

        for (t = 0; t < sizeof tms / sizeof tms[0]; t++)
        {
            reset_case();
            g_inE.run.bit.run = 1;
            g_inE.fc.code.oCurrentChkTime  = tms[t];
            g_inE.fc.code.oCurrentChkValue = 1000;
            g_inE.outCurrentDisp = 0;
            g_inM.oCurrentCheckFlag = 0;
            g_inM.oCurrentChkTicker = 0;
            for (k = 0; k < 8; k++)
            {
                run1("Dout34/tick", K_Dout, 34, 0);
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase: DoCalc                                                             */
/* ------------------------------------------------------------------------- */

static void phase_docalc(void)
{
    static const Uint16 fcs[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 20, 41 };
    unsigned a, b, c, d, k;

    for (a = 0; a < sizeof fcs / sizeof fcs[0]; a++)
    {
        for (b = 0; b < sizeof fcs / sizeof fcs[0]; b++)
        {
            for (c = 0; c < 4; c++)
            {
                for (d = 0; d < 2; d++)
                {
                    reset_case();

                    g_inE.run.bit.run = (Uint16)(c & 1);
                    g_inE.frq         = (int32)(c * 200 - 200);
                    g_inE.frqAim      = (int32)(d * 300);
                    g_inE.frqRun      = (int32)(c * 100);
                    g_inE.errCode     = (Uint16)(c * 0x0101);

                    /* 十个物理 DO + 五个虚拟 DO, 功能码轮换 —— 让 doFuncCal
                     * 的 42 个槽都被走过一遍。 */
                    for (k = 0; k < DO_NUMBER_PHSIC; k++)
                    {
                        g_inE.fc.code.doFunc[k]      = fcs[(a + k) % 12];
                        g_inE.fc.code.doDelayTime[k] = (Uint16)(c + 1);
                    }
                    for (k = 0; k < DO_NUMBER_V; k++)
                    {
                        g_inE.fc.code.vdoFunc[k]      = fcs[(b + k) % 12];
                        g_inE.fc.code.vdoDelayTime[k] = (Uint16)(d + 1);
                    }

                    g_inE.doComm.all = (Uint16)(c * 0x0101);
                    g_inE.dsp.all    = (Uint16)(c * 0x0080);
                    g_inE.fc.code.fanControl = (Uint16)(d & 1);
                    g_inM.doStatus.a.all = (Uint32)(c * 0x000F);
                    g_inM.doStatus.b.all = (Uint32)(d * 0x00F0);
                    g_inM.diStatus.c.all = (Uint32)(c * 0x03FF);
                    g_inM.doLogic = (Uint16)(c * 0x0005);

                    /* PLC 输出口覆盖: outPortControl 的十进制位选通 inPortOut 的
                     * 对应位。digit1[0] / [1] / [2] 分别覆盖 do3 / relay1 / do1,
                     * 所以这三个位必须被单独摆到 0 和 1 上。 */
                    g_inE.fc.code.plcEnable      = (Uint16)(d & 1);
                    g_inE.fc.code.outPortControl = (Uint16)(c * 111);
                    g_inE.fc.code.inPortOut      = (Uint16)(c * 101);

                    for (k = 0; k < 4; k++)
                    {
                        run1("DoCalc", K_DoCalc, 0, 0);
                    }
                }
            }
        }
    }

    /* 延时门槛: delayTickerMax = doDelayTime * (100 / 2) = 50 * n。
     * 扫 n = 0..3 并把每档连跑 51 次, 让门槛 0 / 50 / 100 / 150 都被跨过。 */
    {
        static const Uint16 dl[] = { 0, 1, 2, 3 };
        unsigned x, k;

        for (x = 0; x < 4; x++)
        {
            reset_case();
            for (k = 0; k < DO_NUMBER_PHSIC; k++)
            {
                g_inE.fc.code.doFunc[k]      = 1;   /* Dout1: runFlag.bit.run */
                g_inE.fc.code.doDelayTime[k] = dl[x];
            }
            for (k = 0; k < DO_NUMBER_V; k++)
            {
                g_inE.fc.code.vdoFunc[k]      = 1;
                g_inE.fc.code.vdoDelayTime[k] = dl[x];
            }
            g_inE.run.bit.run = 1;
            for (k = 0; k < 52; k++)
            {
                run1("DoCalc/delay", K_DoCalc, 0, 0);
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase: AiCalc                                                             */
/* ------------------------------------------------------------------------- */

static void phase_aicalc(void)
{
    static const Uint16 modes[] = { FUNCCODE_tempSenorType_NONE,
                                    FUNCCODE_tempSenorType_PTC100,
                                    FUNCCODE_tempSenorType_PTC1000,
                                    FUNCCODE_tempSenorType_NTC };
    static const Uint16 filt[]  = { 0, 1, 5, 20 };
    /* aiCurveSrc 的**每个**半字节都是 pAiSetCurve[] 的下标 +1, 必须落在 1..5。
     * 0x0001 / 0x1000 这种看着"正常"的取值里就有 0 半字节, AiCalc 会算
     * `pAiSetCurve[-1]` 再解引用 —— 第一版就是这么 segfault 的 (rc=139), 而且
     * 崩在 phase_aicalc 里, 现场只留一行 "[phase] aicalc" 的进度打印。 */
    static const Uint16 curve[] = { 0x1111, 0x2345, 0x4444, 0x5555, 0x1234, 0x5432 };
    static const Uint16 limit[] = { 0x0000, 0x0001, 0x0010, 0x1111 };
    static const Uint16 mlt[]   = { 0, 1, 2 };
    unsigned m, f, c, l, t, k;

    for (m = 0; m < 4; m++)
    {
        for (f = 0; f < 4; f++)
        {
            for (c = 0; c < 6; c++)
            {
                for (l = 0; l < 4; l++)
                {
                    for (t = 0; t < 3; t++)
                    {
                        reset_case();

                        g_inE.fc.code.motorOtMode  = modes[m];
                        g_inE.fc.code.ai1FilterTime = filt[f];
                        g_inE.fc.code.ai2FilterTime = filt[(f + 1) % 4];
                        g_inE.fc.code.ai3FilterTime = filt[(f + 2) % 4];
                        g_inE.fc.code.aiCurveSrc    = curve[c];
                        g_inE.fc.code.aiLimitSrc    = limit[l];
                        g_inE.mainLoopTicker        = mlt[t];

                        g_inM.aiDeal[0].sample = (Uint16)(500 + m * 300);
                        g_inM.aiDeal[1].sample = (Uint16)(900 + f * 200);
                        g_inM.aiDeal[2].sample = (Uint16)(1200 + c * 100);

                        /* 跳跃带: point / arrange 必须让 point - arrange 与
                         * point + arrange 都落在 aiLine.y 的可能范围里, 否则
                         * 那两条判据恒真或恒假, 分不出边界运算符。 */
                        for (k = 0; k < AI_NUMBER; k++)
                        {
                            g_inE.fc.code.aiJumpSet[k].point   = (Uint16)(400 + k * 100);
                            g_inE.fc.code.aiJumpSet[k].arrange = (Uint16)(50 + k * 10);
                        }

                        /* mainLoopTicker == 1 那一支会把三路 LPF 的 out 直接
                         * 置成采样值, 并把 ticker 改成 2 —— 连跑两次正好覆盖
                         * "第一次进"和"第二次不进"。 */
                        run1("AiCalc", K_AiCalc, 0, 0);
                        run1("AiCalc/2", K_AiCalc, 0, 0);
                    }
                }
            }
        }
    }

    /* 曲线区间切换: 电压扫过 curveSet4P* 的每一段, 让 `p += i * 2` 的 i
     * 取到 2 / 1 / 0 三个值 (aiNum >= 4 时才有意义)。 */
    {
        static const Uint16 volt[] = { 0, 20, 50, 100, 200, 400, 800, 1600, 3200 };
        unsigned v;

        for (v = 0; v < sizeof volt / sizeof volt[0]; v++)
        {
            reset_case();
            g_inE.fc.code.aiCurveSrc = 0x4444;
            g_inM.aiDeal[0].voltage = (int16)volt[v];
            g_inM.aiDeal[1].voltage = (int16)volt[v];
            g_inM.aiDeal[2].voltage = (int16)volt[v];
            g_inM.aiDeal[0].sample  = (Uint16)(volt[v] * 4);
            g_inM.aiDeal[1].sample  = (Uint16)(volt[v] * 4);
            g_inM.aiDeal[2].sample  = (Uint16)(volt[v] * 4);
            run1("AiCalc/seg", K_AiCalc, 0, 0);
        }
    }

    /* 跳变窗口的**上沿**必须精确命中, 才分得出 `aiLine.y < point + range`
     * 与 `... + 1`。整条链要能算出来, 所以每一级都取"恰好"的构造:
     *
     *   1) ai1FilterTime = 0  ->  aiLpf[0].t == 0  ->  LpfCalc 直通, out = sample;
     *   2) sample = 65520 (== 4095 << 4) -> LineCalc 的 `x >= x2` 支 -> voltageOrigin = 10570;
     *   3) 标定曲线 {after1=0, before1=0, after2=10570, before2=5001}
     *      -> x2 = 10570*13421>>12 = 34633, y2 = 5001*13421>>12 = 16386
     *      -> (10570<<15)/34633 = 10000, (10000*16386 + 16384)>>15 = 5000
     *      -> aiDeal[0].voltage = 5000 / 10 = 500;
     *   4) 折线 {0, 0, 1000, 1000} -> x1 = 0, x2 = y2 = 32766,
     *      x = 500*33553>>10 = 16383, (16383<<15)/32766 = 16384 整除
     *      -> aiLine.y = 16383;
     *   5) point = 0, arrange = 500 -> point + range = 16383 == aiLine.y。
     *
     * 于是 ref 的第二条判据 `16383 < 16383` 为假 (保留 aiLine.y),
     * 变异成 `16383 < 16384` 为真 (改成 point = 0), 输出真的不一样。 */
    {
        reset_case();
        g_inE.fc.code.motorOtMode   = FUNCCODE_tempSenorType_NONE;
        g_inE.fc.code.ai1FilterTime = 0;
        g_inE.fc.code.aiCurveSrc    = 0x1111;
        g_inE.fc.code.aiLimitSrc    = 0x0000;
        g_inE.fc.code.aiCalibrateCurve[0].before1 = 0;
        g_inE.fc.code.aiCalibrateCurve[0].after1  = 0;
        g_inE.fc.code.aiCalibrateCurve[0].before2 = 5001;
        g_inE.fc.code.aiCalibrateCurve[0].after2  = 10570;
        g_inE.fc.code.curveSet2P1[0] = 0;
        g_inE.fc.code.curveSet2P1[1] = 0;
        g_inE.fc.code.curveSet2P1[2] = 1000;
        g_inE.fc.code.curveSet2P1[3] = 1000;
        g_inM.aiDeal[0].sample = 65520;
        g_inE.fc.code.aiJumpSet[0].point   = 0;
        g_inE.fc.code.aiJumpSet[0].arrange = 500;
        run1("AiCalc/jumpEdge", K_AiCalc, 0, 0);
        run1("AiCalc/jumpEdge2", K_AiCalc, 0, 0);
    }
}

/* ------------------------------------------------------------------------- */
/* phase: AoCalcChannel                                                      */
/* ------------------------------------------------------------------------- */

static void phase_aocalc(void)
{
    unsigned ch, fn, k, p;

    for (ch = 1; ch <= AO_NUMBER; ch++)
    {
        for (fn = 0; fn <= AO_FMP_FUNC_OUT_TORQUE_DIR; fn++)
        {
            for (k = 0; k < 3; k++)
            {
                reset_case();

                g_inE.fc.code.aoFunc[ch] = (Uint16)fn;
                g_inE.fc.code.aoPara[ch - 1].gain   = (Uint16)(100 + k * 400);
                g_inE.fc.code.aoPara[ch - 1].offset = (Uint16)(k * 10);

                /* 输出量给三档: 小于满量程 / 正好满量程 / 超满量程。 */
                g_inE.frqDisp         = (Uint16)(k * 1000);
                g_inE.frqAim          = (int32)(k * 1000);
                g_inE.outCurrentDisp  = (Uint16)(k * 1000);
                g_inE.itDisp          = (Uint16)(k * 2000);
                g_inE.outPower        = (int16)(k * 1000);
                g_inE.outVoltage      = (Uint16)(k * 1000);
                g_inE.generatrixVoltage = (Uint16)(k * 5000);
                g_inM.pulseInFrq      = (Uint32)(k * 40000);
                g_inM.aiDeal[0].voltage = (int16)(k * 500);
                g_inM.aiDeal[1].voltage = (int16)(k * 500);
                g_inM.aiDeal[2].voltage = (int16)(k * 500);
                g_inE.fc.code.lengthCurrent = (Uint16)(k * 500);
                g_inE.fc.code.lengthSet     = 1000;
                g_inE.fc.code.counterTicker = (Uint16)(k * 500);
                g_inE.fc.code.counterSet    = 1000;
                g_inE.ao[ch]          = (Uint16)(k * 20000);
                g_inE.frqRun          = (int32)(k * 500);

                run1("AoCalcChannel", K_AoCalc, (Uint16)ch, 0);
            }
        }
    }

    /* PLC 覆盖支: 只在 AO1 + plcEnable + outPortControl >= 10000 时走。 */
    for (p = 0; p < 6; p++)
    {
        reset_case();
        g_inE.fc.code.plcEnable      = 1;
        g_inE.fc.code.ao1Value       = (Uint16)(p * 200);
        g_inE.fc.code.outPortControl = (Uint16)(9999 + p);
        g_inE.fc.code.aoFunc[AOFMP_AO1] = AO_FMP_FUNC_FRQ_SET;
        run1("AoCalcChannel/plc", K_AoCalc, AOFMP_AO1, 0);
        run1("AoCalcChannel/plc2", K_AoCalc, AOFMP_AO2, 0);
    }

    /* 增益/偏移的符号位: gain 与 offset 都是 Uint16, AoCalcChannel 把它们当
     * int16 用。负增益会让 outCoeff 落到 0 那一支, 必须单独覆盖。 */
    {
        static const Uint16 gains[]   = { 0, 1, 32767, 32768, 65535, 0x8001 };
        static const Uint16 offsets[] = { 0, 1, 32767, 32768, 65535 };
        unsigned g, o;

        for (g = 0; g < 6; g++)
        {
            for (o = 0; o < 5; o++)
            {
                reset_case();
                g_inE.fc.code.aoPara[0].gain   = gains[g];
                g_inE.fc.code.aoPara[0].offset = offsets[o];
                g_inE.fc.code.aoFunc[AOFMP_AO1] = AO_FMP_FUNC_FRQ_AIM;
                g_inE.frqAim = 500;
                run1("AoCalcChannel/gain", K_AoCalc, AOFMP_AO1, 0);
            }
        }
    }

    /* 标定曲线把 outCoeff 拉过 0 与 32768 两个钳位: 让 after2 < after1,
     * 于是 LineCalc 的斜率是负的, 输出从大变小。 */
    {
        static const Uint16 aft[] = { 0, 1, 100, 32767 };
        unsigned a;

        for (a = 0; a < 4; a++)
        {
            reset_case();
            g_inE.fc.code.aoCalibrateCurve[0].before1 = 0;
            g_inE.fc.code.aoCalibrateCurve[0].after1  = aft[a];
            g_inE.fc.code.aoCalibrateCurve[0].before2 = 32000;
            g_inE.fc.code.aoCalibrateCurve[0].after2  = (Uint16)(aft[a] + 1);
            g_inE.fc.code.aoFunc[AOFMP_AO1] = AO_FMP_FUNC_FRQ_AIM;
            g_inE.frqAim = 500;
            run1("AoCalcChannel/cal", K_AoCalc, AOFMP_AO1, 0);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase: PulseInSample / PulseInCalc                                        */
/* ------------------------------------------------------------------------- */

static void phase_pulsein(void)
{
    static const Uint32 caps[] = { 0, 1, 100, 60000, 0xFFFFFFFFUL };
    unsigned a, b, c, k;

    for (a = 0; a < 5; a++)
    {
        for (b = 0; b < 5; b++)
        {
            for (c = 0; c < 3; c++)
            {
                reset_case();

                g_inE.ecap1.CAP1 = caps[a];
                g_inE.ecap1.CAP2 = caps[b];
                g_inE.ecap1.CAP3 = caps[(a + c) % 5];
                g_inE.ecap1.CAP4 = caps[(b + c) % 5];

                g_inM.captureTicker    = (Uint16)(c * 3);
                g_inM.capturePeriodSum = (Uint32)(1000 + c * 500);
                g_inM.noCaptureTicker  = (Uint16)(c * 5);
                g_inM.pulseInFrq1      = (Uint32)(c * 25000);
                g_inM.pulseInLpf.t     = (int32)(c * 5);

                g_inE.fc.code.lengthPulsePerMeter = (Uint16)(100 + c * 50);
                g_inE.fc.code.diFunc[4] = (Uint16)(c * 10);

                /* 采样 + 计算连跑: PulseInSample 决定 captureTicker /
                 * capturePeriodSum, PulseInCalc 再把它们消费掉并清零。 */
                run1("PulseInSample", K_PulseInSample, 0, 0);
                run1("PulseInCalc", K_PulseInCalc, 0, 0);
                run1("PulseInCalc/2", K_PulseInCalc, 0, 0);
            }
        }
    }

    /* 无捕获计数: noCaptureTicker 参与 `(2000 / (n * 2) + 1) >> 1`,
     * 扫 n = 1..8 让商从 1000 一路降到 125, 同时覆盖它和 pulseInFrq1 的比较。 */
    for (k = 1; k <= 8; k++)
    {
        reset_case();
        g_inM.captureTicker    = 0;
        g_inM.capturePeriodSum = 0;
        g_inM.noCaptureTicker  = (Uint16)k;
        g_inM.pulseInFrq1      = (Uint32)(k * 100);
        run1("PulseInCalc/noCap", K_PulseInCalc, 0, 0);
    }

    /* 计数器/长度: counterTicker 溢出与 reset 位。
     * 门槛在 `counterTicker < counterTickerOld` (Uint16 回绕) 与
     * `diFunc.f1.bit.resetCounter` 两处, 都要单独摆。 */
    {
        static const Uint16 olds[] = { 0, 1, 0xFFFE, 0xFFFF };
        unsigned o, r;

        for (o = 0; o < 4; o++)
        {
            for (r = 0; r < 2; r++)
            {
                reset_case();
                g_inE.fc.code.counterTicker = olds[o];
                g_inE.fc.code.lengthCurrent = olds[o];
                g_inM.diFunc.f1.bit.resetCounter       = (Uint16)r;
                g_inM.diFunc.f1.bit.resetLengthCounter = (Uint16)(1 - r);
                g_inM.captureTicker    = 2;
                g_inM.capturePeriodSum = 4000;
                g_inM.pulseInFrq1      = 30000;
                run1("PulseInCalc/cnt", K_PulseInCalc, 0, 0);
            }
        }
    }

    /* diFunc.f1.bit.counterTickerIn / lengthTickerIn 的边沿检测:
     * counterTickerInOld 是 static, 只有"变"才算一次计数, 所以要连跑两次
     * (第一次建立 Old, 第二次才判边沿)。 */
    {
        unsigned v, kk;

        for (v = 0; v < 2; v++)
        {
            for (kk = 0; kk < 3; kk++)
            {
                reset_case();
                g_inM.diFunc.f1.bit.counterTickerIn  = (Uint16)((kk & 1) ? 1 : v);
                g_inM.diFunc.f1.bit.lengthTickerIn   = (Uint16)((kk & 2) ? 1 : v);
                g_inE.fc.code.diFunc[4] = 3;
                g_inM.captureTicker    = 1;
                g_inM.capturePeriodSum = 2000;
                g_inM.pulseInFrq1      = 1000;
                run1("PulseInCalc/edge1", K_PulseInCalc, 0, 0);
                run1("PulseInCalc/edge2", K_PulseInCalc, 0, 0);
            }
        }
    }

    /* pulseInLpf.t 是 PulseInCalc **自己算出来的**
     * (`t = pulseInFilterTime * (10 / 2)`), 上面把它当输入写的那句是死写入。
     * 系数 +1 的变异只有 pulseInFilterTime != 0 才看得出来。
     *
     * 同一条路上顺手把 `lengthTickerDelta` 做出来: diFunc[4] 取
     * DI_FUNC_LENGTH_TICKER_IN (27) 时不会走"边沿检测覆盖 delta"那一支,
     * delta 保持 pulseCounter; pulseInFrq1 = 25000 超过
     * COUNTER_IN_USE_CAPTURE_TICKER_LIMIT, 于是 delta = (25000*2)/1000 = 50。
     * lengthPulsePerMeter 取 1, `50*10` 与 `50*11` 就不再被整除吞掉。 */
    {
        static const Uint16 fts[] = { 0, 1, 2, 5, 100 };
        unsigned f;

        for (f = 0; f < sizeof fts / sizeof fts[0]; f++)
        {
            reset_case();
            g_inE.fc.code.pulseInFilterTime   = fts[f];
            g_inE.fc.code.diFunc[4]           = DI_FUNC_LENGTH_TICKER_IN;
            g_inE.fc.code.lengthPulsePerMeter = 1;
            g_inM.captureTicker    = 2;
            g_inM.capturePeriodSum = 4000;
            g_inM.pulseInFrq1      = 25000;
            run1("PulseInCalc/filter", K_PulseInCalc, 0, 0);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase: FMPDeal / PulseOutCalc                                             */
/* ------------------------------------------------------------------------- */

static void phase_fmp(void)
{
    unsigned s, d, p, k;

    for (s = 0; s < 3; s++)
    {
        for (d = 0; d < 2; d++)
        {
            for (p = 0; p < 3; p++)
            {
                reset_case();

                g_inE.fc.code.fmOutSelect = (Uint16)s;
                g_inM.doStatus.c.bit.do3  = (Uint16)d;
                g_inE.fc.code.plcEnable   = (Uint16)(p & 1);
                /* fmpValue 与 frqDisp 都是 PulseOutCalc 里那个除数的因子
                 * (`... / (aoFmpValue * fmpOutMaxFrq)`), 而且检查 `!aoFmpValue`
                 * 的语句在除法**之后** —— 0 就是除零。第一版这里写的是
                 * `p * 300`, p=0 时立刻崩 (rc=139, 停在 "[phase] fmp")。 */
                g_inE.fc.code.fmpValue    = (Uint16)(100 + p * 300);
                g_inE.fc.code.outPortControl = (Uint16)(p * 5500);
                g_inE.fc.code.aoFunc[AOFMP_FMP] = AO_FMP_FUNC_FRQ_SET;
                g_inE.frqDisp = (Uint16)(100 + p * 200);
                /* FMPDeal 在 fmOutSelect == PULSE 时会调 PulseOutCalc, 而 plc 那
                 * 一支关掉时走的是 UpdateFmpAoValue 的 FRQ_SET —— 变异 UF1 会把
                 * aoFmpValue 换成 frqAim, 留 0 就除零。 */
                g_inE.frqAim  = 500;

                /* fmOutSelectOld 是 static, 连跑两次才覆盖"变了"和"没变"。 */
                run1("FMPDeal", K_FMPDeal, 0, 0);
                run1("FMPDeal/2", K_FMPDeal, 0, 0);
            }
        }
    }

    /* PulseOutCalc 的 200 Hz 分频切换: beforeFrq 是 static, 初值 200,
     * 只有 curFrq 跨过 200 才走 `* 224` / `/ 224` 两支。frqDisp 直接决定
     * curFrq (aoFmpMax = maxFrq = 1000, fmpOutMaxFrq = 1000)。 */
    {
        static const Uint16 disps[] = { 100, 199, 200, 201, 400, 50 };
        unsigned i;

        reset_case();
        g_inE.fc.code.aoFunc[AOFMP_FMP] = AO_FMP_FUNC_FRQ_SET;
        g_inE.fc.code.fmpOutMaxFrq = 1000;
        g_inE.maxFrq = 1000;
        /* PulseOutCalc 的分母是 `aoFmpValue * fmpOutMaxFrq`, 而 `!aoFmpValue`
         * 的检查写在除法**之后**。FRQ_SET 这一支只读 frqDisp, 但"设定频率
         * 取成频率给定"(UF1) 会把它换成 frqAim —— frqAim 留 0 就除零。 */
        g_inE.frqAim = 500;

        for (i = 0; i < 6; i++)
        {
            g_inE.frqDisp = disps[i];
            g_inE.fc.code.fmpValue = disps[i];
            run1("PulseOutCalc/frq", K_PulseOutCalc, 0, 0);
            run1("PulseOutCalc/frq2", K_PulseOutCalc, 0, 0);
        }

        /* 跨回来: 200 -> 100 -> 200 -> 400, 让 CLOCK_BENCH 在 1 / 224 之间翻转。 */
        for (i = 0; i < 4; i++)
        {
            g_inE.frqDisp = disps[(i + 1) % 6];
            run1("PulseOutCalc/back", K_PulseOutCalc, 0, 0);
        }
    }

    /* periodOld / HDOCTR 的四个分支: 靠 aoFmpValue 与 fmpOutMaxFrq 的比值
     * 让 period 相对 periodOld 增 / 减 / 相等。 */
    {
        static const Uint16 fmp[] = { 0, 1, 2, 10, 100, 500, 1000, 5000, 20000 };
        unsigned i;

        for (i = 0; i < 9; i++)
        {
            reset_case();
            g_inE.fc.code.aoFunc[AOFMP_FMP] = AO_FMP_FUNC_FRQ_SET;
            g_inE.fc.code.fmpOutMaxFrq = 1000;
            g_inE.maxFrq  = 1000;
            g_inE.frqDisp = (Uint16)(fmp[i] + 1);
            g_inE.frqAim  = 500;   /* UF1 会把 aoFmpValue 换成它, 留 0 就除零 */
            run1("PulseOutCalc/period", K_PulseOutCalc, 0, 0);
        }

        /* fmpOutMaxFrq = 0 会让 `aoFmpValue * fmpOutMaxFrq` 变成 0 而被当除数 ——
         * 这是**定义域外**的输入, 模块自己也不防 (检查 `!aoFmpValue` 在除法之后)。
         * 所以驱动不给 0, 见文件头"输入必须留在定义域内"。 */
    }

    /* ---- 分频切换 -----------------------------------------------------------
     *
     * HDOCTR 是从 EPwm5Regs.TBCTR **读**出来的 (`HDOCTR = HDO_CTR`), 而
     * install() 每次都把整个 EPwm5Regs 从 g_inE.epwm5 恢复 —— 也就是说 TBCTR
     * 完全由驱动控制, 预填的 0xC3C3 (= 50115) 一直大于 292, 于是
     * `HDOCTR > 292` 恒真, `* 224` 那一支在旧驱动下从来没被走到。
     *
     * 上穿 (`curFrq >= 200 && beforeFrq < 200`) 才进 `* 224` 支, 而
     * beforeFrq 是函数内 static, 只能用"先跑一次把 beforeFrq 摆好"的办法驱动。
     *
     * TBCTR 必须取得**小**: 放大后的 HDOCTR 若大于 period, 函数末尾那句
     * `if (HDOCTR > period) HDOCTR = 0;` 会把它清掉, 两个不同的值就都变成 0 了。
     * TBCTR = 10 -> HDOCTR 在 2240 / 2230 这个量级, 远小于 period (29999),
     * 能一路活到 HDO_CTR。
     *
     * 注意 `HDOCTR > 292` 本身 (PO3) 是**不可观测**的, 见 README §7.3:
     * 292 * 224 = 65408 与 0xFFFF = 65535 在后续三条判据上取值完全一致,
     * 最后都被清成 0。 */
    {
        reset_case();
        g_inE.fc.code.aoFunc[AOFMP_FMP] = AO_FMP_FUNC_FRQ_SET;
        g_inE.fc.code.fmpOutMaxFrq = 1000;
        g_inE.maxFrq  = 1000;
        g_inE.frqAim  = 500;   /* UF1 会把 aoFmpValue 换成它, 留 0 就除零 */

        /* 第一次: curFrq = 100 -> beforeFrq 变 100, CLOCK_BENCH 变 224。 */
        g_inE.frqDisp = 100;
        g_inE.epwm5.TBCTR = 292;
        run1("PulseOutCalc/up0", K_PulseOutCalc, 0, 0);

        /* 第二次: 100 -> 200 上穿, TBCTR = 10 -> HDOCTR = 10 * 224 / 10 * 223。
         * period = 6e6 / 200 - 1 = 29999, 不等于 65534 也不等于 0,
         * 所以走的是"写寄存器"那一支而不是 else。 */
        g_inE.frqDisp = 200;
        g_inE.epwm5.TBCTR = 10;
        run1("PulseOutCalc/up10", K_PulseOutCalc, 0, 0);

        /* PO2: `beforeFrq < 200` -> `<=` 只在 beforeFrq 正好 200 时分歧。
         * 先跑一次 curFrq = 200 把 beforeFrq 置成 200 (这一次 beforeFrq 是 200
         * 之前的值 100, ref 与变异都不进 —— 变异判据是 `<= 200`, 100 <= 200
         * 但 curFrq < 200 不成立, 所以两支一样), 再给一次 curFrq = 200。 */
        g_inE.frqDisp = 200;
        g_inE.epwm5.TBCTR = 100;
        run1("PulseOutCalc/bf200a", K_PulseOutCalc, 0, 0);
        run1("PulseOutCalc/bf200b", K_PulseOutCalc, 0, 0);
    }

    /* PO6 / PO9: 需要 period 正好等于 MAX_UINT16 - 1 (65534), 也就是
     * tmp 被钳到 MAX_UINT16 之后减一。aoFmpValue = 1、fmpOutMaxFrq = 1、
     * aoFmpMax = 1000 时 tmp = 6e10, 远超 65535。
     * 这时外层 `((MAX_UINT16-1) != period) && period` 为假 -> 走 else 支,
     * ref 的 `!period` 为假 -> HDO_CMP = 0; 变异 `if (period)` 为真 ->
     * HDO_CMP = PULSE_OUT_CTR_PERIOD + 1。PO6 则让外层 if 变成真, 整条
     * HDO_PRD/HDO_CMP/HDO_CTR 的写值都不一样。 */
    {
        reset_case();
        g_inE.fc.code.aoFunc[AOFMP_FMP] = AO_FMP_FUNC_FRQ_SET;
        g_inE.fc.code.fmpOutMaxFrq = 1;
        g_inE.maxFrq  = 1000;
        g_inE.frqDisp = 1;
        g_inE.frqAim  = 500;   /* UF1 会把 aoFmpValue 换成它, 留 0 就除零 */
        g_inE.epwm5.TBPRD = 0;
        g_inE.epwm5.TBCTR = 10;
        run1("PulseOutCalc/p65534", K_PulseOutCalc, 0, 0);
    }

    (void)k;
}

/* ------------------------------------------------------------------------- */
/* phase: LineCalc / LpfCalc                                                 */
/* ------------------------------------------------------------------------- */

static void phase_linecalc(void)
{
    static const int32 xs[] = { -100, 0, 1, 50, 100, 3000, 5000 };
    unsigned m, a, b, c;

    for (m = 0; m < 3; m++)
    {
        for (a = 0; a < 7; a++)
        {
            for (b = 0; b < 7; b++)
            {
                for (c = 0; c < 3; c++)
                {
                    reset_case();

                    g_line.calc = (void (*)(void *))ref_LineCalc;
                    g_line.mode = (int16)m;
                    g_line.x1 = xs[a];
                    g_line.y1 = (int32)(a * 100 - 200);
                    /* x2 必须 != x1: LineCalc 的 tmp 在**所有分支之前**无条件算,
                     * `(p->x2 - p->x1)` 是除数。 */
                    g_line.x2 = (int32)(xs[b] + (xs[b] == xs[a] ? 1 : 0));
                    g_line.y2 = (int32)(b * 300 - 100);
                    g_line.x  = (int32)(c * 1000 - 500);
                    g_line.y  = 0;

                    run1("LineCalc", K_LineCalc, 0, 0);
                }
            }
        }
    }

    /* 边界: x 正好等于 x1 / x2, 以及 mode==2 且 x < x1 的那一支。 */
    {
        static const int32 pts[] = { -1, 0, 1, 99, 100, 101, 2999, 3000, 3001 };
        unsigned p;

        for (p = 0; p < 9; p++)
        {
            reset_case();
            g_line.calc = (void (*)(void *))ref_LineCalc;
            g_line.mode = 2;
            g_line.x1 = 100;
            g_line.y1 = 10;
            g_line.x2 = 3000;
            g_line.y2 = 90;
            g_line.x  = pts[p];
            g_line.y  = 0;
            run1("LineCalc/edge", K_LineCalc, 0, 0);

            g_line.mode = 1;
            run1("LineCalc/edge1", K_LineCalc, 0, 0);

            g_line.mode = 0;
            run1("LineCalc/edge0", K_LineCalc, 0, 0);
        }
    }

    /* LC1 的 `p->x <= p->x1` -> `<`: 只有当 x 正好等于 x1 **并且 x1 > x2** 时
     * 两支才落到不同结果 —— x1 > x2 时 `x < x1` 为假, 判据继续往下走,
     * `x >= x2` 立刻成立, 于是取 y2 而不是 y1。
     * 现有扫描里 x 只取 -500/500/1500, 永远碰不到 x1 = 3000 这种端点。 */
    {
        reset_case();
        g_line.calc = (void (*)(void *))ref_LineCalc;
        g_line.mode = 0;
        g_line.x1 = 3000;   g_line.y1 = 10;
        g_line.x2 = 100;    g_line.y2 = 90;
        g_line.x  = 3000;   g_line.y  = 0;
        run1("LineCalc/x1gtx2", K_LineCalc, 0, 0);

        g_line.mode = 2;
        run1("LineCalc/x1gtx2m2", K_LineCalc, 0, 0);
    }

    /* LC2 的 `p->x >= p->x2` -> `>`: 折线在结点处连续, 所以 x == x2 时
     * tmp 恰好等于 y2 —— **除非** `(x - x1) << 15` 溢出 int32。
     * x2 - x1 = 80000 时 80000 << 15 已经越过 INT32_MAX, 商被算错,
     * 于是 `>` 那支取到的 tmp 与 y2 不同。y1/y2 故意取不对称,
     * 否则回绕后的误差会被 (y2 - y1) 放大回同一个整数 (试过, 会碰巧相等)。 */
    {
        reset_case();
        g_line.calc = (void (*)(void *))ref_LineCalc;
        g_line.mode = 0;
        g_line.x1 = -40000;  g_line.y1 = 0;
        g_line.x2 = 40000;   g_line.y2 = 12345;
        g_line.x  = 40000;   g_line.y  = 0;
        run1("LineCalc/ovf", K_LineCalc, 0, 0);
    }
}

static void phase_lpfcalc(void)
{
    static const int32 ts[]   = { -10, -1, 0, 1, 2, 5, 100 };
    static const int32 ins[]  = { -1000, 0, 1, 500, 30000 };
    static const int32 outs[] = { -500, 0, 1, 400, 20000 };
    static const int32 rems[] = { -3, 0, 1, 7 };
    unsigned a, b, c, d;

    for (a = 0; a < 7; a++)
    {
        for (b = 0; b < 5; b++)
        {
            for (c = 0; c < 5; c++)
            {
                for (d = 0; d < 4; d++)
                {
                    reset_case();

                    g_lpf.calc      = (void (*)(void *))ref_LpfCalc;
                    g_lpf.t         = ts[a];
                    g_lpf.in        = ins[b];
                    g_lpf.out       = outs[c];
                    g_lpf.outOld    = outs[(c + 1) % 5];
                    g_lpf.remainder = rems[d];

                    run1("LpfCalc", K_LpfCalc, 0, 0);
                }
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase: Init* (十个 GPIO / EPWM / ECAP 初始化入口)                          */
/* ------------------------------------------------------------------------- */

static void phase_init(void)
{
    static const int kinds[] =
    {
        K_InitSetEPWM4, K_InitSetEPWM5, K_InitSetEPWM6, K_InitSetEcap1,
        K_InitDIGpio, K_InitDOGpio, K_InitEPwm4Gpio, K_InitEPwm5Gpio,
        K_InitEPwm6Gpio, K_InitECap1Gpio, K_InitECap2Gpio, K_InitECap3Gpio,
        K_InitECap4Gpio
    };
    unsigned i, k;

    for (i = 0; i < sizeof kinds / sizeof kinds[0]; i++)
    {
        /* 寄存器预填两种: 全 0 与全 1。全 1 那一遍能抓出"把位域写反"
         * (例如 TB_CTR_ZERO 写成 TB_IMMEDIATE) —— 全 0 时两者都写 0。 */
        for (k = 0; k < 2; k++)
        {
            reset_case();
            if (k)
            {
                memset(&g_inE.ecap1,    0xFF, sizeof g_inE.ecap1);
                memset(&g_inE.epwm4,    0xFF, sizeof g_inE.epwm4);
                memset(&g_inE.epwm5,    0xFF, sizeof g_inE.epwm5);
                memset(&g_inE.epwm6,    0xFF, sizeof g_inE.epwm6);
                memset(&g_inE.gpioCtrl, 0xFF, sizeof g_inE.gpioCtrl);
                memset(&g_inE.gpioData, 0xFF, sizeof g_inE.gpioData);
            }
            run1("Init", kinds[i], 0, 0);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase: 温度                                                               */
/* ------------------------------------------------------------------------- */

static void phase_temp(void)
{
    static const Uint16 modes[] = { FUNCCODE_tempSenorType_NONE,
                                    FUNCCODE_tempSenorType_PTC100,
                                    FUNCCODE_tempSenorType_PTC1000,
                                    FUNCCODE_tempSenorType_NTC };
    static const Uint16 temps[] = { 0, 49, 50, 59, 60, 79, 80, 100 };
    static const Uint16 errs[]  = { 0, ERROR_MOTOR_OT, 0x0001 };
    unsigned m, t, e, k;

    for (m = 0; m < 4; m++)
    {
        for (t = 0; t < 8; t++)
        {
            for (e = 0; e < 3; e++)
            {
                reset_case();

                g_inE.fc.code.motorOtMode = modes[m];
                g_inM.temperature  = temps[t];
                g_inE.errOther     = errs[e];
                g_inE.fc.code.motorOtCoef    = 60;
                g_inE.fc.code.motorOtProtect = 80;

                g_inM.aiDeal[2].sample  = (Uint16)(t * 300);
                g_inM.tempSampleSum     = (Uint32)(t * 3000);
                /* 这里**不能**让 tickerTempDeal 涨到 50。
                 *
                 * TemperatureSensorDeal 是 `tickerTempDeal++` 之后才判
                 * `>= TEMPERATURE_CALC_PERIOD / TEMPERATURE_CALL_PERIOD` (50),
                 * 一旦跨过去就会调 GetTemperature; 而 GetTemperature 在
                 * motorOtMode 既不是 PTC100 也不是 PTC1000 时, 局部变量 p / size
                 * **根本没被赋值**, 直接拿去当指针和长度用。第一版写的是 `t * 7`
                 * (最大 49, 自增后正好 50), 于是 motorOtMode = NTC 那一档立刻
                 * segfault, 现场停在 "[phase] temp"。
                 *
                 * 这不是测试台的 bug, 是模块自己的潜在缺陷 (NTC 档下读野指针);
                 * 但它属于**可达性之外**的输入, 差分测试没有资格去踩, 所以这里
                 * 把 ticker 压在 50 以下, NTC 只覆盖"不累加"的那一段。跨 50 的
                 * 那一支单独由下面 PTC100 / PTC1000 的循环覆盖。 */
                g_inM.tickerTempDeal    = (Uint16)(t * 6);
                g_inM.temperatureVoltage = (Uint16)(1650 + t * 100);

                /* TemperatureDeal 在 motorOtMode == 0 时清零并直接返回;
                 * 否则走 SensorDeal + AfterDeal。两支都要覆盖。 */
                run1("TemperatureDeal", K_TemperatureDeal, 0, 0);
                run1("TemperatureAfterDeal", K_TemperatureAfterDeal, 0, 0);
            }
        }
    }

    /* TemperatureSensorDeal 的除数: k = (4095<<4)*tickerTempDeal / 3300,
     * 只有 tickerTempDeal >= 50 才会进到除法, 而 50 正是门槛
     * (TEMPERATURE_CALC_PERIOD / TEMPERATURE_CALL_PERIOD = 100/2)。
     * 连跑让 tickerTempDeal 自己涨上去, 覆盖 49 / 50 两侧。 */
    for (m = 1; m <= 2; m++)
    {
        reset_case();
        g_inE.fc.code.motorOtMode = modes[m];
        g_inM.tickerTempDeal = 0;
        g_inM.tempSampleSum  = 0;
        g_inM.aiDeal[2].sample = 1500;
        for (k = 0; k < 52; k++)
        {
            run1("TemperatureSensorDeal", K_TemperatureSensorDeal, 0, 0);
        }
    }

    /* GetTemperature: motorOtMode 只能是 PTC100 / PTC1000 —— 其它取值时
     * 模块里的 p / size 两个局部量根本没被赋值, 是本模块**自己**的定义域外
     * 输入, 驱动不给。 */
    for (m = 1; m <= 2; m++)
    {
        static const Uint16 volt[] =
        {
            0, 1600, 1650, 1800, 2000, 2500, 2837, 3000, 4000, 0xFFFF
        };
        unsigned v;

        for (v = 0; v < sizeof volt / sizeof volt[0]; v++)
        {
            reset_case();
            g_inE.fc.code.motorOtMode = modes[m];
            g_inM.temperatureVoltage  = volt[v];
            run1("GetTemperature", K_GetTemperature, 0, 0);
        }
    }

    /* GetTemperatureCalc: 直接给表和长度。
     * len 必须 >= 2 —— len == 1 时 `i = len - 1 = 0`, 接着读 p[-1]。 */
    {
        static const Uint16 volt[] = { 0, 1, 99, 100, 101, 149, 150, 250, 251, 1000 };
        unsigned v, n;

        for (n = 0; n < 8; n++)
        {
            g_tbl[n] = (Uint16)(100 + n * 50);
        }

        for (v = 0; v < sizeof volt / sizeof volt[0]; v++)
        {
            for (n = 2; n <= 8; n += 3)
            {
                reset_case();
                run1("GetTemperatureCalc", K_GetTemperatureCalc, volt[v], (Uint16)n);
            }
        }

        /* `voltage < *(p + i)` 与 `<=` 在**严格递增**的表上可证等价 ——
         * 折线在结点处连续, i 差一格时 x1/y1 与 x2/y2 正好互换, 两端输出
         * 都等于那个结点的 y。所以上面对照永远抓不到它。
         *
         * 但表里有**重复项**时 y1 = (i - 1) * 4 会跟着 i 变, 两个 i 给出的
         * 输出就不再相等: flat[1] == flat[2] == 150, voltage = 150 时
         *   ref (`<`) 走到 i = 3 -> x1 = flat[2] = 150, y1 = 8  -> y = 8
         *   变异 (`<=`) 走到 i = 1 -> x2 = flat[1] = 150, y2 = 4  -> y = 4
         * 这是模块的合法输入 (表由调用方给), 不是定义域外。 */
        {
            static const Uint16 flat[] = { 100, 150, 150, 250, 250, 250, 400, 500 };
            unsigned f;

            for (f = 0; f < 8; f++)
            {
                g_tbl[f] = flat[f];
            }
            for (f = 0; f < 8; f++)
            {
                reset_case();
                run1("GetTemperatureCalc/flat", K_GetTemperatureCalc, flat[f], 8);
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase: Di2Bin / Do2Bin / getDiHwStatus / setDOStatus / UpdateFmpAoValue    */
/* ------------------------------------------------------------------------- */

static void phase_small(void)
{
    static const Uint16 decs[] = { 0, 1, 9, 10, 11, 99, 100, 1234, 11111, 99999 };
    unsigned a, b, c;

    /* Di2Bin / Do2Bin: 十进制编码 -> 二进制掩码。字段变化才会重算 (四个 Old
     * 都是 static), 所以每个取值连跑两次: 第一次"变", 第二次"没变"。 */
    for (a = 0; a < 10; a++)
    {
        for (b = 0; b < 10; b += 3)
        {
            reset_case();
            g_inE.fc.code.diLogic[0]     = decs[a];
            g_inE.fc.code.diLogic[1]     = decs[b];
            g_inE.fc.code.diLogicAiAsDi  = decs[(a + b) % 10];
            g_inE.fc.code.vdiSrc         = decs[(a + 1) % 10];
            g_inE.fc.code.vdiFcSet       = decs[(b + 2) % 10];
            g_inE.fc.code.doLogic        = decs[a];
            g_inE.fc.code.vdoLogic       = decs[b];

            run1("Di2Bin", K_Di2Bin, 0, 0);
            run1("Di2Bin/2", K_Di2Bin, 0, 0);
            run1("Do2Bin", K_Do2Bin, 0, 0);
            run1("Do2Bin/2", K_Do2Bin, 0, 0);
        }
    }

    /* getDiHwStatus: 十个 GPIO 引脚的每一种组合都要能把对应位置起来。
     * 单点 + 全 0 + 全 1 + 交替。 */
    for (a = 0; a < 12; a++)
    {
        reset_case();
        switch (a)
        {
        case 0:  set_di_pins(0x000); break;
        case 1:  set_di_pins(0x3FF); break;
        case 2:  set_di_pins(0x155); break;
        case 3:  set_di_pins(0x2AA); break;
        default: set_di_pins((Uint16)(1U << (a - 4))); break;
        }
        run1("getDiHwStatus", K_getDiHwStatus, 0, 0);
    }

    /* setDOStatus: doHwStatus 的五个位各自决定 GPxSET 还是 GPxCLEAR。 */
    for (a = 0; a < 32; a++)
    {
        reset_case();
        g_inM.doHwStatus.all = (Uint16)a;
        run1("setDOStatus", K_setDOStatus, 0, 0);
    }

    /* UpdateFmpAoValue: func 0..17 (含两个未定义的 17 与 default 支),
     * aoOrFmp 覆盖 0 / 1 / 2 —— 只有 AO_FMP_FUNC_COMM 用得到它。 */
    for (a = 0; a < 18; a++)
    {
        for (b = 0; b < 3; b++)
        {
            reset_case();

            g_inE.frqDisp         = (Uint16)(100 + a * 10);
            g_inE.frqAim          = (int32)(a * 100 - 500);
            g_inE.outCurrentDisp  = (Uint16)(a * 100);
            g_inE.itDisp          = (Uint16)(a * 300);
            g_inE.outPower        = (int16)(a * 100 - 800);
            g_inE.outVoltage      = (Uint16)(a * 100);
            g_inM.pulseInFrq      = (Uint32)(a * 4000);
            g_inM.aiDeal[0].voltage = (int16)(a * 50);
            g_inM.aiDeal[1].voltage = (int16)(a * 60);
            g_inM.aiDeal[2].voltage = (int16)(a * 70);
            g_inE.fc.code.lengthCurrent = (Uint16)(a * 50);
            g_inE.fc.code.counterTicker = (Uint16)(a * 50);
            g_inE.ao[b]           = (Uint16)(a * 2000);
            g_inE.frqRun          = (int32)(a * 50);
            g_inE.generatrixVoltage = (Uint16)(a * 500);

            run1("UpdateFmpAoValue", K_UpdateFmpAoValue, (Uint16)a, (Uint16)b);
        }
    }

    /* 负值与越界钳位: aoFmpValue 与 aoFmpMax 最后都会被 ABS_INT32 再比大小。
     * 这里让 frqRun 取负, 覆盖 `aoFmpValue = frqRun` 那一支的符号。 */
    for (c = 0; c < 4; c++)
    {
        reset_case();
        g_inE.frqRun = (int32)(-(int32)(c * 1000));
        g_inE.maxFrq = (Uint16)(c * 500);
        run1("UpdateFmpAoValue/speed", K_UpdateFmpAoValue,
             AO_FMP_FUNC_SPEED, 0);
    }
}

/* ------------------------------------------------------------------------- */
/* 随机相                                                                    */
/* ------------------------------------------------------------------------- */

static unsigned long g_rnd = 0x2545F491UL;

static Uint32 rnd32(void)
{
    g_rnd ^= (g_rnd << 13) & 0xFFFFFFFFUL;
    g_rnd ^= (g_rnd >> 17);
    g_rnd ^= (g_rnd << 5) & 0xFFFFFFFFUL;
    return (Uint32)g_rnd;
}

static Uint16 rnd16(void)
{
    return (Uint16)rnd32();
}

/* 随机相只补"字段组合", 不补"定义域外的值": 每条钳位都对应文件头那张表里
 * 的一条前提, 去掉任何一条都会让某一类变异变成崩溃而不是差异。 */
static void rand_safe(void)
{
    int i;

    g_inE.run.all = rnd16();
    g_inE.errCode = (Uint16)(rnd32() & 0x0101);
    g_inE.errOther = (Uint16)(rnd32() & 0x0003);
    g_inE.errDealStatus = (enum ERROR_DEAL_STATUS)(rnd32() % 4);
    g_inE.errAttr.all = rnd16();
    g_inE.errAutoRstNum = (Uint16)(rnd32() % 8);
    g_inE.dsp.all = rnd16();

    g_inE.frq    = (int32)(rnd32() % 2000) - 1000;
    g_inE.frqAim = (int32)(rnd32() % 2000) - 1000;
    g_inE.frqRun = (int32)(rnd32() % 2000) - 1000;
    g_inE.frqDisp = (Uint16)(1 + rnd32() % 3000);
    g_inE.maxFrq = (Uint16)(1 + rnd32() % 3000);          /* 除数, 非零 */
    g_inE.upperFrq = (Uint16)(rnd32() % 3000);
    g_inE.lowerFrq = (Uint16)(rnd32() % 3000);
    g_inE.swingFrqLimit = (Uint16)(rnd32() & 1);
    g_inE.tuneCmd = (Uint16)(rnd32() & 1);

    g_inE.outCurrentDisp = (Uint16)(rnd32() % 4000);
    g_inE.outPower = (int16)(rnd32() % 4000);
    g_inE.outVoltage = (Uint16)(rnd32() % 4000);
    g_inE.generatrixVoltage = (Uint16)(rnd32() % 8000);
    g_inE.itDisp = (Uint16)(rnd32() % 4000);

    g_inE.ratingCurrent = (Uint16)(1 + rnd32() % 4000);   /* 除数, 非零 */
    g_inE.ratingPower   = (Uint16)(1 + rnd32() % 4000);

    for (i = 0; i < AO_N; i++)
    {
        g_inE.ao[i] = rnd16();
    }
    g_inE.doComm.all = rnd16();

    g_inE.bPlcEndOneLoop = (Uint16)(rnd32() & 1);
    g_inE.bUv = (Uint16)(rnd32() & 1);
    g_inE.curTime.runTime = (Uint16)(rnd32() % 5000);
    g_inE.setRunLostTime = (Uint16)(rnd32() & 1);
    g_inE.setRunTimeAim = (Uint16)(rnd32() % 5000);
    g_inE.mainLoopTicker = (Uint16)(rnd32() % 3);
    g_inE.torqueRet = (Uint16)(rnd32() & 1);
    g_inE.diHw.all = rnd32();

    /* funcCode: 只动本模块读的那些字段, 且每个都夹在定义域内。 */
    g_inE.fc.code.diFilterTime = rnd16();
    for (i = 0; i < DI_NUMBER_PHSIC; i++)
    {
        g_inE.fc.code.diFunc[i] = (Uint16)(rnd32() % DI_FUNC_NUMBER);
    }
    for (i = 0; i < DI_NUMBER_V; i++)
    {
        g_inE.fc.code.vdiFunc[i] = (Uint16)(rnd32() % DI_FUNC_NUMBER);
    }
    for (i = 0; i < DI_NUMBER_AI_AS_DI; i++)
    {
        g_inE.fc.code.aiAsDiFunc[i] = (Uint16)(rnd32() % DI_FUNC_NUMBER);
    }
    for (i = 0; i < 3; i++)
    {
        g_inE.fc.code.diDelayTime[i] = (Uint16)(rnd32() % 20);
    }
    for (i = 0; i < DO_NUMBER_PHSIC; i++)
    {
        g_inE.fc.code.doFunc[i] = (Uint16)(rnd32() % DO_FUNC_NUMBER);
        g_inE.fc.code.doDelayTime[i] = (Uint16)(rnd32() % 20);
    }
    for (i = 0; i < DO_NUMBER_V; i++)
    {
        g_inE.fc.code.vdoFunc[i] = (Uint16)(rnd32() % DO_FUNC_NUMBER);
        g_inE.fc.code.vdoDelayTime[i] = (Uint16)(rnd32() % 20);
    }

    g_inE.fc.code.motorOtMode = (Uint16)(rnd32() % 4);
    /* aiCurveSrc 的四个半字节必须逐个夹在 1..5 —— 不能靠 `0x1111 + rnd % 0x4444`
     * 这种"整体加法", 那会造出带 0 半字节的值 (见 phase_aicalc 里的注释)。 */
    g_inE.fc.code.aiCurveSrc = 0;
    for (i = 0; i < 4; i++)
    {
        g_inE.fc.code.aiCurveSrc |=
            (Uint16)((1 + rnd32() % 5) << (i * 4));
    }
    g_inE.fc.code.aiLimitSrc  = rnd16();

    for (i = 0; i < AI_NUMBER; i++)
    {
        g_inE.fc.code.aiCalibrateCurve[i].before1 = rnd16();
        g_inE.fc.code.aiCalibrateCurve[i].after1  = rnd16();
        g_inE.fc.code.aiCalibrateCurve[i].before2 = (Uint16)(rnd16() | 1);
        if (g_inE.fc.code.aiCalibrateCurve[i].before2 ==
            g_inE.fc.code.aiCalibrateCurve[i].before1)
        {
            g_inE.fc.code.aiCalibrateCurve[i].before2++;
        }
        g_inE.fc.code.aiCalibrateCurve[i].after2 = rnd16();
    }

    for (i = 0; i < AO_NUMBER; i++)
    {
        g_inE.fc.code.aoCalibrateCurve[i].before1 = rnd16();
        g_inE.fc.code.aoCalibrateCurve[i].after1  = rnd16();
        g_inE.fc.code.aoCalibrateCurve[i].before2 = (Uint16)(rnd16() | 1);
        if (g_inE.fc.code.aoCalibrateCurve[i].before2 ==
            g_inE.fc.code.aoCalibrateCurve[i].before1)
        {
            g_inE.fc.code.aoCalibrateCurve[i].before2++;
        }
        g_inE.fc.code.aoCalibrateCurve[i].after2 = rnd16();
        g_inE.fc.code.aoPara[i].gain   = rnd16();
        g_inE.fc.code.aoPara[i].offset = rnd16();
    }

    /* 曲线组: 每条曲线的 x1 与 x2 必须不同 (LineCalc 的除数)。 */
    for (i = 0; i < 4; i++)
    {
        g_inE.fc.code.curveSet2P1[i] = rnd16();
        g_inE.fc.code.curveSet2P2[i] = rnd16();
        g_inE.fc.code.curveSet2P3[i] = rnd16();
        g_inE.fc.code.curveSet2P4[i] = rnd16();
    }
    if (g_inE.fc.code.curveSet2P1[2] == g_inE.fc.code.curveSet2P1[0])
        g_inE.fc.code.curveSet2P1[2]++;
    if (g_inE.fc.code.curveSet2P2[2] == g_inE.fc.code.curveSet2P2[0])
        g_inE.fc.code.curveSet2P2[2]++;
    if (g_inE.fc.code.curveSet2P3[2] == g_inE.fc.code.curveSet2P3[0])
        g_inE.fc.code.curveSet2P3[2]++;
    if (g_inE.fc.code.curveSet2P4[2] == g_inE.fc.code.curveSet2P4[0])
        g_inE.fc.code.curveSet2P4[2]++;
    for (i = 0; i < 8; i++)
    {
        g_inE.fc.code.curveSet4P1[i] = rnd16();
        g_inE.fc.code.curveSet4P2[i] = rnd16();
    }
    /* AiCalc 在 aiNum >= 4 时会 `p += i * 2`, i 取 2 / 1 / 0, 于是 LineCalc 拿到的
     * x1 / x2 分别是 (p[0], p[2]) / (p[2], p[4]) / (p[4], p[6]) —— 三对都必须不同,
     * 只保证相邻两个不同是不够的。 */
    for (i = 0; i < 6; i += 2)
    {
        if (g_inE.fc.code.curveSet4P1[i + 2] == g_inE.fc.code.curveSet4P1[i])
            g_inE.fc.code.curveSet4P1[i + 2]++;
        if (g_inE.fc.code.curveSet4P2[i + 2] == g_inE.fc.code.curveSet4P2[i])
            g_inE.fc.code.curveSet4P2[i + 2]++;
    }

    for (i = 0; i < AI_NUMBER; i++)
    {
        g_inE.fc.code.aiJumpSet[i].point   = rnd16();
        g_inE.fc.code.aiJumpSet[i].arrange = rnd16();
    }

    g_inE.fc.code.aoFunc[AOFMP_FMP] = (Uint16)(rnd32() % 18);
    g_inE.fc.code.aoFunc[AOFMP_AO1] = (Uint16)(rnd32() % 18);
    g_inE.fc.code.aoFunc[AOFMP_AO2] = (Uint16)(rnd32() % 18);
    g_inE.fc.code.ao1Value = rnd16();
    g_inE.fc.code.fmOutSelect = (Uint16)(rnd32() % 3);
    g_inE.fc.code.fmpValue = (Uint16)(1 + rnd32() % 2000);
    g_inE.fc.code.fmpOutMaxFrq = (Uint16)(1 + rnd32() % 5000);  /* 除数, 非零 */

    g_inE.fc.code.ai1FilterTime = (Uint16)(rnd32() % 30);
    g_inE.fc.code.ai2FilterTime = (Uint16)(rnd32() % 30);
    g_inE.fc.code.ai3FilterTime = (Uint16)(rnd32() % 30);
    g_inE.fc.code.ai1VoltageUpper = rnd16();
    g_inE.fc.code.ai1VoltageLimit = rnd16();

    g_inE.fc.code.counterTicker = rnd16();
    /* counterSet / lengthSet 是 UpdateFmpAoValue 的 aoFmpMax, 而 AoCalcChannel 立刻
     * 拿 aoFmpMax 当除数 —— 0 就是除零。见文件头"输入必须留在定义域内"。 */
    g_inE.fc.code.counterSet = (Uint16)(1 + rnd32() % 4000);
    g_inE.fc.code.counterPoint = rnd16();
    g_inE.fc.code.lengthCurrent = rnd16();
    g_inE.fc.code.lengthSet = (Uint16)(1 + rnd32() % 4000);
    g_inE.fc.code.lengthPulsePerMeter = (Uint16)(1 + rnd32() % 2000);  /* 除数 */
    g_inE.fc.code.lengthTickerRemainder = rnd16();

    g_inE.fc.code.runTimeAddup = rnd16();
    g_inE.fc.code.runTimeArriveSet = rnd16();
    g_inE.fc.code.powerUpTimeAddup = rnd16();
    g_inE.fc.code.powerUpTimeArriveSet = rnd16();

    g_inE.fc.code.frqFdtValue  = (Uint16)(rnd32() % 2000);
    g_inE.fc.code.frqFdt1Value = (Uint16)(rnd32() % 2000);
    g_inE.fc.code.frqFdtLag    = (Uint16)(rnd32() % 1001);
    g_inE.fc.code.frqFdt1Lag   = (Uint16)(rnd32() % 1001);
    g_inE.fc.code.frqArriveRange  = (Uint16)(rnd32() % 1001);
    g_inE.fc.code.frqArriveValue1 = rnd16();
    g_inE.fc.code.frqArriveRange1 = (Uint16)(rnd32() % 1001);
    g_inE.fc.code.frqArriveValue2 = rnd16();
    g_inE.fc.code.frqArriveRange2 = (Uint16)(rnd32() % 1001);

    g_inE.fc.code.currentArriveValue1 = rnd16();
    g_inE.fc.code.currentArriveRange1 = rnd16();
    g_inE.fc.code.currentArriveValue2 = rnd16();
    g_inE.fc.code.currentArriveRange2 = rnd16();

    g_inE.fc.code.oCurrentChkValue = rnd16();
    g_inE.fc.code.oCurrentChkTime  = (Uint16)(rnd32() % 20);
    g_inE.fc.code.radiatorTemp = rnd16();
    g_inE.fc.code.temperatureArrive = rnd16();
    g_inE.fc.code.setTimeArrive = rnd16();
    g_inE.fc.code.motorOtCoef = rnd16();
    g_inE.fc.code.motorOtProtect = rnd16();

    g_inE.fc.code.doLogic = rnd16();
    g_inE.fc.code.vdoLogic = rnd16();
    g_inE.fc.code.diLogic[0] = rnd16();
    g_inE.fc.code.diLogic[1] = rnd16();
    g_inE.fc.code.diLogicAiAsDi = rnd16();
    g_inE.fc.code.vdiSrc = rnd16();
    g_inE.fc.code.vdiFcSet = rnd16();

    g_inE.fc.code.errAutoRstRelayAct = (Uint16)(rnd32() & 1);
    g_inE.fc.code.errAutoRstNumMax = (Uint16)(rnd32() % 10);

    /* 模块状态: 预填随机值, 让"改写漏写某字段"可见。 */
    g_inM.diFunc.f1.all = rnd32();
    g_inM.diFunc.f2.all = rnd32();
    g_inM.diSelectFunc.f1.all = rnd32();
    g_inM.diSelectFunc.f2.all = rnd32();
    g_inM.diStatus.a.all = rnd32();
    g_inM.diStatus.b.all = rnd32();
    g_inM.diStatus.c.all = rnd32();
    g_inM.doFunc.f1.all = rnd32();
    g_inM.doFunc.f2.all = rnd32();
    g_inM.doStatus.a.all = rnd16();
    g_inM.doStatus.b.all = rnd16();
    g_inM.doStatus.c.all = rnd16();
    g_inM.doHwStatus.all = rnd16();
    g_inM.oCurrentCheckFlag = (Uint16)(rnd32() & 1);
    g_inM.oCurrentChkTicker = rnd32() % 100;
    g_inM.softOcDoFlag = (Uint16)(rnd32() & 1);
    g_inM.motorForeOT = (Uint16)(rnd32() & 1);
    g_inM.motorOT = (Uint16)(rnd32() & 1);
    g_inM.temperature = rnd16();
    g_inM.tickerTempDeal = (Uint16)(rnd32() % 60);
    /* tickerTempDeal 与 motorOtMode 是**耦合**输入, 不能各自独立随机:
     * TemperatureSensorDeal 是 `tickerTempDeal++` **之后**才判
     * `>= TEMPERATURE_CALC_PERIOD / TEMPERATURE_CALL_PERIOD` (= 50),
     * 所以预置 49 也会跨过去并调 GetTemperature; 而 GetTemperature 在
     * motorOtMode 既非 PTC100 也非 PTC1000 时局部量 p / size 从未赋值 ——
     * 目标端读到的是栈上垃圾 (不崩, 但值不可复现), 宿主端就是野指针。
     * 因此 NTC 档只能配 tickerTempDeal <= 48, 跨门槛那一支只配 PTC100/PTC1000。 */
    if (((Uint32)g_inM.tickerTempDeal + 1 >= 50) &&
        (g_inE.fc.code.motorOtMode > FUNCCODE_tempSenorType_PTC1000))
    {
        g_inE.fc.code.motorOtMode = (Uint16)(1 + rnd32() % 2);
    }
    g_inM.tempSampleSum = rnd32();
    g_inM.temperatureVoltage = rnd16();
    g_inM.temperatureVoltageOrigin = rnd16();
    g_inM.diLogic = rnd32();
    g_inM.vdiSrc = rnd16();
    g_inM.vdiFcSet = rnd16();
    g_inM.doLogic = rnd16();
    g_inM.aoFmpValue = (int32)(rnd32() % 4000);
    g_inM.aoFmpMax = (int32)(1 + rnd32() % 4000);   /* 除数, 非零 */
    g_inM.pulseInSet = (int16)rnd16();
    g_inM.pulseInFrq = rnd32();
    g_inM.pulseInFrq1 = rnd32();
    g_inM.lineSpeed = rnd16();
    g_inM.capturePeriodSum = (Uint32)(1 + rnd32() % 60000);  /* 除数, 非零 */
    g_inM.captureTicker = (Uint16)(rnd32() % 8);
    g_inM.noCaptureTicker = (Uint16)(rnd32() % 8);
    g_inM.bOverFlowCounterTicker = (Uint16)(rnd32() & 1);
    g_inM.bOverFlowLengthCurrent = (Uint16)(rnd32() & 1);

    for (i = 0; i < AI_N; i++)
    {
        g_inM.aiLpf[i].t = (int32)(rnd32() % 200);
        g_inM.aiLpf[i].in = (int32)(rnd32() % 4000);
        g_inM.aiLpf[i].out = (int32)(rnd32() % 4000);
        g_inM.aiLpf[i].outOld = (int32)(rnd32() % 4000);
        g_inM.aiLpf[i].remainder = (int32)(rnd32() % 100);

        g_inM.aiDeal[i].sample = rnd16();
        g_inM.aiDeal[i].voltageOrigin = (int16)rnd16();
        g_inM.aiDeal[i].voltage = (int16)(rnd32() % 1200);
        g_inM.aiDeal[i].set = (int16)rnd16();
    }

    g_inM.pulseInLpf.t = (int32)(rnd32() % 200);
    g_inM.pulseInLpf.in = (int32)(rnd32() % 4000);
    g_inM.pulseInLpf.out = (int32)(rnd32() % 4000);
    g_inM.pulseInLpf.outOld = (int32)(rnd32() % 4000);
    g_inM.pulseInLpf.remainder = (int32)(rnd32() % 100);

    for (i = 0; i < DO_N; i++)
    {
        g_inM.doDelayTicker.all[i].high = rnd32() % 400;
        g_inM.doDelayTicker.all[i].low  = rnd32() % 400;
    }
    for (i = 0; i < 3; i++)
    {
        g_inM.diDelayTicker.all[i].high = rnd32() % 400;
        g_inM.diDelayTicker.all[i].low  = rnd32() % 400;
    }

    /* 寄存器: 全 0 / 全 1 / 随机三种轮换。 */
    switch (rnd32() % 3)
    {
    case 0:
        memset(&g_inE.ecap1,    0x00, sizeof g_inE.ecap1);
        memset(&g_inE.epwm4,    0x00, sizeof g_inE.epwm4);
        memset(&g_inE.epwm5,    0x00, sizeof g_inE.epwm5);
        memset(&g_inE.epwm6,    0x00, sizeof g_inE.epwm6);
        memset(&g_inE.gpioCtrl, 0x00, sizeof g_inE.gpioCtrl);
        memset(&g_inE.gpioData, 0x00, sizeof g_inE.gpioData);
        break;
    case 1:
        memset(&g_inE.ecap1,    0xFF, sizeof g_inE.ecap1);
        memset(&g_inE.epwm4,    0xFF, sizeof g_inE.epwm4);
        memset(&g_inE.epwm5,    0xFF, sizeof g_inE.epwm5);
        memset(&g_inE.epwm6,    0xFF, sizeof g_inE.epwm6);
        memset(&g_inE.gpioCtrl, 0xFF, sizeof g_inE.gpioCtrl);
        memset(&g_inE.gpioData, 0xFF, sizeof g_inE.gpioData);
        break;
    default:
        for (i = 0; i < (int)sizeof g_inE.ecap1; i++)
        {
            ((unsigned char *)&g_inE.ecap1)[i] = (unsigned char)rnd32();
        }
        for (i = 0; i < (int)sizeof g_inE.epwm4; i++)
        {
            ((unsigned char *)&g_inE.epwm4)[i] = (unsigned char)rnd32();
        }
        for (i = 0; i < (int)sizeof g_inE.epwm5; i++)
        {
            ((unsigned char *)&g_inE.epwm5)[i] = (unsigned char)rnd32();
        }
        for (i = 0; i < (int)sizeof g_inE.epwm6; i++)
        {
            ((unsigned char *)&g_inE.epwm6)[i] = (unsigned char)rnd32();
        }
        for (i = 0; i < (int)sizeof g_inE.gpioCtrl; i++)
        {
            ((unsigned char *)&g_inE.gpioCtrl)[i] = (unsigned char)rnd32();
        }
        for (i = 0; i < (int)sizeof g_inE.gpioData; i++)
        {
            ((unsigned char *)&g_inE.gpioData)[i] = (unsigned char)rnd32();
        }
        break;
    }
}

static void phase_random(long n)
{
    long i;
    /* 崩溃定位用: 设 IO_DBG_R=1 就逐例打印 (i, kind), 最后一行即崩溃点。
     * 平时关闭, 不影响台账。 */
    static int dbg = -1;

    if (dbg < 0)
    {
        dbg = (NULL != getenv("IO_DBG_R")) ? 1 : 0;
    }

    for (i = 0; i < n; i++)
    {
        int kind = (int)(rnd32() % K_NUM);
        Uint16 a0 = 0;
        Uint16 a1 = 0;

        reset_case();
        rand_safe();

        switch (kind)
        {
        case K_Dout:
            a0 = (Uint16)(rnd32() % DO_FUNC_NUMBER);
            break;
        case K_AoCalc:
            a0 = (Uint16)(1 + rnd32() % AO_NUMBER);
            break;
        case K_UpdateFmpAoValue:
            a0 = (Uint16)(rnd32() % 18);
            a1 = (Uint16)(rnd32() % 3);
            break;
        case K_GetTemperatureCalc:
            for (a1 = 0; a1 < 8; a1++)
            {
                g_tbl[a1] = (Uint16)(100 + a1 * 50);
            }
            a0 = rnd16();
            a1 = (Uint16)(2 + rnd32() % 7);     /* len >= 2 */
            break;
        case K_LineCalc:
            g_line.calc = (void (*)(void *))ref_LineCalc;
            g_line.mode = (int16)(rnd32() % 3);
            g_line.x1 = (int32)(rnd32() % 8000) - 4000;
            g_line.x2 = (int32)(rnd32() % 8000) - 4000;
            if (g_line.x2 == g_line.x1)
            {
                g_line.x2++;
            }
            g_line.y1 = (int32)(rnd32() % 8000) - 4000;
            g_line.y2 = (int32)(rnd32() % 8000) - 4000;
            g_line.x  = (int32)(rnd32() % 8000) - 4000;
            g_line.y  = 0;
            break;
        case K_LpfCalc:
            g_lpf.calc = (void (*)(void *))ref_LpfCalc;
            g_lpf.t = (int32)(rnd32() % 400);
            g_lpf.in = (int32)(rnd32() % 8000) - 4000;
            g_lpf.out = (int32)(rnd32() % 8000) - 4000;
            g_lpf.outOld = (int32)(rnd32() % 8000) - 4000;
            g_lpf.remainder = (int32)(rnd32() % 400) - 200;
            break;
        case K_GetTemperature:
            /* 只有 PTC100 / PTC1000 是 GetTemperature 的定义域。 */
            g_inE.fc.code.motorOtMode = (Uint16)(1 + rnd32() % 2);
            break;
        default:
            break;
        }

        /* PulseOutCalc 的分母是 `aoFmpValue * funcCode.code.fmpOutMaxFrq`, 而
         * 检查 `!aoFmpValue` 的那句写在除法**之后** —— aoFmpValue 为 0 时宿主端
         * 除零崩溃 (rc = 0xC0000094); 目标端 C28x 的除零不陷入, 只是置 DIV 溢出
         * 位并给出饱和结果, 所以这是**只存在于宿主**的差异, 不是被测行为。
         *
         * aoFmpValue 由 UpdateFmpAoValue 从十几个全局量之一算出 (frqDisp /
         * frqAim / outCurrentDisp / itDisp / outPower / outVoltage / pulseInFrq /
         * aiDeal[].voltage / lengthCurrent / counterTicker / aoComm[] / frqRun /
         * generatrixVoltage), 逐个钉非零会把"取值为 0"这个**真实边界**一起钉掉。
         * 所以只对**会走到 PulseOutCalc 的那两个 kind** 把 aoFunc 钉在 FRQ_SET,
         * 其余 kind 的输入维度保持全随机; aoFunc 的取值遍历由 K_UpdateFmpAoValue
         * 单独承担 (UpdateFmpAoValue 里没有除法, 可以全量扫 0..17)。 */
        if ((K_PulseOutCalc == kind) || (K_FMPDeal == kind))
        {
            g_inE.fc.code.aoFunc[AOFMP_FMP] = AO_FMP_FUNC_FRQ_SET;
            g_inE.frqDisp      = (Uint16)(1 + rnd32() % 3000);
            g_inE.maxFrq       = (Uint16)(1 + rnd32() % 3000);
            g_inE.fc.code.fmpOutMaxFrq = (Uint16)(1 + rnd32() % 5000);
            /* FRQ_SET 这一支只读 frqDisp, 但"设定频率取成频率给定"这条变异
             * (UF1) 会把它换成 frqAim —— frqAim 随机到 0 时
             * `... / (aoFmpValue * fmpOutMaxFrq)` 立刻除零, 宿主上就是
             * 0xC0000094, 总账还会把它记成 CAUGHT。所以两个候选一起钉非零。 */
            g_inE.frqAim       = (int32)(1 + rnd32() % 3000);
        }

        if (dbg)
        {
            printf("[R] i=%ld kind=%d a0=%u a1=%u mode=%u tick=%u\n",
                   i, kind, (unsigned)a0, (unsigned)a1,
                   (unsigned)g_inE.fc.code.motorOtMode,
                   (unsigned)g_inM.tickerTempDeal);
            fflush(stdout);
        }

        run1("random", kind, a0, a1);
    }
}

/* ------------------------------------------------------------------------- */
/* main                                                                      */
/* ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long nrand = 2000;
    int i;
    int uncov = 0;

    if (argc > 1)
    {
        nrand = atol(argv[1]);
        if (nrand < 0)
        {
            nrand = 0;
        }
    }

    printf("[phase] dicalc     cases=%ld\n", g_cases);
    fflush(stdout);
    phase_dicalc();
    printf("[phase] dout       cases=%ld\n", g_cases);
    fflush(stdout);
    phase_dout();
    printf("[phase] docalc     cases=%ld\n", g_cases);
    fflush(stdout);
    phase_docalc();
    printf("[phase] aicalc     cases=%ld\n", g_cases);
    fflush(stdout);
    phase_aicalc();
    printf("[phase] aocalc     cases=%ld\n", g_cases);
    fflush(stdout);
    phase_aocalc();
    printf("[phase] pulsein    cases=%ld\n", g_cases);
    fflush(stdout);
    phase_pulsein();
    printf("[phase] fmp        cases=%ld\n", g_cases);
    fflush(stdout);
    phase_fmp();
    printf("[phase] line/lpf   cases=%ld\n", g_cases);
    fflush(stdout);
    phase_linecalc();
    phase_lpfcalc();
    printf("[phase] init       cases=%ld\n", g_cases);
    fflush(stdout);
    phase_init();
    printf("[phase] temp       cases=%ld\n", g_cases);
    fflush(stdout);
    phase_temp();
    printf("[phase] small      cases=%ld\n", g_cases);
    fflush(stdout);
    phase_small();
    printf("[phase] random     cases=%ld\n", g_cases);
    fflush(stdout);
    phase_random(nrand);

    printf("\n---- coverage (每个入口被驱动了多少例) ----\n");
    for (i = 0; i < K_NUM; i++)
    {
        printf("  %-22s %ld\n", KIND_NAME[i], g_kindN[i]);
        if (0 == g_kindN[i])
        {
            printf("  !! 未覆盖: %s\n", KIND_NAME[i]);
            uncov++;
        }
    }

    printf("\ncases=%ld mismatches=%d\n", g_cases, g_fails);
    if (g_fails >= FAIL_LIMIT)
    {
        printf("(mismatch 输出已截断于 %d 条)\n", FAIL_LIMIT);
    }
    if (uncov)
    {
        printf("未覆盖入口 %d 个 -> 覆盖门失败\n", uncov);
    }
    return (g_fails || uncov) ? 1 : 0;
}
