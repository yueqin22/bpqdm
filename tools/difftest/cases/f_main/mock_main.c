/* f_main 差分测试的共享 mock。

 * 这里只定义**两侧必须看到同一份**的东西:
 *
 *   1. 外设: SysCtrlRegs (InitDspFunc 写它的时钟使能位) 与 CpuTimer1Regs
 *      (GetTime() 读它的 TIM.all)。模块读它们、写它们, 从不分配它们, 所以由
 *      driver 扮演芯片: 每次调用前装一份完整映像, 调用后整块快照。
 *   2. 模块引用但不拥有的 27 个全局量 (funcCode / funcCodeRead / errorEeprom /
 *      dspStatus / commTicker / scanStep / spiData ...)。
 *      其中 funcCode 与 funcCodeRead 是**被写的**, 所以 driver 必须在每一侧调用
 *      **之前**把它们装回同一份输入映像, 否则第二侧看到的是第一侧写过的值。
 *   3. 58 个被调函数的替身与它们的**序列**日志。
 *
 * ---- 三个替身不是空壳 --------------------------------------------------------
 *
 *   LimitDeal()                    逐字抄自 src/function/f_menu.c:1037。f_main 拿它
 *                                  的返回值写回 funcCode.all[], 换成"返回 data"
 *                                  的空壳, 夹限值那条路径就测不到了。
 *   EepromDeal()                   必须把 funcCodeRwModeTmp 清成
 *                                  FUNCCODE_RW_MODE_NO_OPERATION。InitFuncCode 的
 *                                  状态机正是靠 `if (!funcCodeRwModeTmp)` 判断
 *                                  EEPROM 往返结束 —— 不清就永远退不出那个 while。
 *   ReadFuncCode() / GetEeprom*()  是状态机的握手与地址映射, 由 driver 预置内容。
 *
 * ---- 时间 ---------------------------------------------------------------------
 *
 * 每个替身被调用时把 CpuTimer1Regs.TIM.all 推进 gTimeStep 拍。于是一次调用内部
 * 多次 GetTime() 的差 = "这一趟调了几个替身" x gTimeStep, 两侧完全一致, 而且
 * InitFuncCode 那个每 2 ms 一次的分支可以被 gTimeStep 直接开关。
 */
#include <stddef.h>

#include "DSP28x_Project.h"
#include "mock_main.h"

/* ---- 外设 ---------------------------------------------------------------- */

volatile struct SYS_CTRL_REGS   SysCtrlRegs;
volatile struct CPUTIMER_REGS   CpuTimer1Regs;

volatile Uint16 gHostCpuCtl[7];
Uint16          gHostMemory[HOST_MEMORY_WORDS];

struct CPUTIMER_VARS CpuTimer1;

/* ---- 共享全局量 ---------------------------------------------------------- */

Uint32 canLinkTicker;
Uint32 commTicker;
const int16 decNumber[5] = { 1, 10, 100, 1000, 10000 };

Uint16 displayBuffer[DISPLAY_8LED_NUM + 1];

union DSP_MAIN_COMMAND1 dspMainCmd1;
union DSP_STATUS        dspStatus;

Uint16 errorCode;
Uint16 errorEeprom;

int32  frqAimTmp0;
Uint16 frqCalcSrcOld;

FUNCCODE_ALL funcCode;

const FUNCCODE_ATTRIBUTE funcCodeAttribute[FNUM_PARA] =
#include "tbl_attribute.inc"

Uint16 funcCodeOneWriteIndex[FMAIN_ONE_WRITE_MAX];
Uint16 funcCodeOneWriteNum;

FUNCCODE_RW funcCodeRead;
Uint16      funcCodeRwModeTmp;

int16 gSendToFunctionDataBuff[64];

Uint16 limitedByOtherCodeIndex[FMAIN_LIMITED_MAX];
Uint16 limitedByOtherCodeIndexNum;

Uint16 plcStep;
Uint16 plcStepRemOld;
Uint32 plcTime;
Uint32 plcTimeRemOld;

Uint16 pmsmRotorPos;
Uint16 scanStep;

struct SPI_IN_OUT spiData;

Uint16 upDownFrqInit;

/* ---- 时间轴 / EEPROM 映像 ------------------------------------------------ */

Uint32 gTimeStep;
Uint16 gEeprom[FMAIN_EEPROM_MAX];
Uint16 gReadFuncCodeRet;

/* ---- 停机预算 (见 mock_main.h) ------------------------------------------- */

jmp_buf       gMockAbort;
int           gMockAbortArmed;
unsigned long gMockTicks;

/* ---- 调用日志 ------------------------------------------------------------ */

struct MAIN_LOG_ENT gMainLog[MAIN_LOG_LEN];
Uint16              gMainLogN;
Uint16              gMainLogOverflow;

static void log_put(Uint16 op, Uint16 a, Uint16 b)
{
    if (gMainLogN < MAIN_LOG_LEN)
    {
        gMainLog[gMainLogN].op = op;
        gMainLog[gMainLogN].a  = a;
        gMainLog[gMainLogN].b  = b;
        gMainLogN++;
    }
    else
    {
        gMainLogOverflow++;
    }
}

static void tick(void)
{
    /* **减**, 不是加 —— C28x 的 CPU 定时器 TIM 是**下计数器** (从 PRD 减到 0
     * 再重装), 所以 GetTime() 随时间**变小**, 而 `baseTime - GetTime()` 从 0
     * **变大**。这不是风格问题: 写成 `+=` 会让 GetTime() 越来越大, 于是
     * `baseTime - GetTime()` 恒为负 -> 按 Uint32 回绕成 ~4.29e9 -> 第 314 行
     * 那个"每 2 ms 一次"的门恒为真, 整块显示/采样体每次都跑。后果是实测出来的:
     * 门内的 FM45 (`& 0x00ff` -> `& 0xff00`) 和门本身的 FM43 (`>=` -> `>`)
     * 双双报 MISSED, 看着像"等价变异", 其实是测试台把门短路了。
     *
     * 方向的依据不靠猜: 全工程的时间差都是"先减后" —— f_main.c 第 314/592 行的
     * `baseTime - GetTime()` / `timeBase - GetTime()`, 以及
     * MotorEncoder.c 第 692 行 `gCpuTime.tmpBase - GetTime()`。先读的数减后读
     * 的数还是正的, 只可能是下计数。
     *
     * 回绕无害: 令 TIM0 为初值, 则 GetTime() = TIM0 - delta (mod 2^32),
     * baseTime - GetTime() = delta (mod 2^32), 与初值无关。 */
    CpuTimer1Regs.TIM.all -= gTimeStep;

    /* 停机预算; 见 mock_main.h。gMockAbortArmed 为 0 时 (例如 driver 直接调
     * GetNumberDigit* 那些不碰替身的纯函数) 这里只是白加一次计数。 */
    gMockTicks++;
    if (gMockAbortArmed && (gMockTicks > MOCK_TICK_BUDGET))
    {
        gMockAbortArmed = 0;
        longjmp(gMockAbort, 1);
    }
}

/* 无参替身的统一形状: 记一条日志, 推一拍时间。 */
#define VOID_STUB(fn, op)                                                     \
    void fn(void)                                                             \
    {                                                                         \
        log_put(op, 0, 0);                                                    \
        tick();                                                               \
    }

/* ---- 58 个被调函数的替身 -------------------------------------------------
 *
 * 顺序与 mock_main.h 里 enum MAIN_OP 一致, 清单来自
 * `nm2000 -u build/Debug/f_main.obj` (87 个未定义符号里除 18 个模块自有符号、
 * 2 个外设对象、27 个共享全局量之外的全部)。
 */

VOID_STUB(AccDecTimeCalc,            MOP_ACC_DEC_TIME_CALC)
VOID_STUB(AiCalc,                    MOP_AI_CALC)
VOID_STUB(CanlinkFun,                MOP_CANLINK_FUN)
VOID_STUB(DiCalc,                    MOP_DI_CALC)
VOID_STUB(DisplayScan,               MOP_DISPLAY_SCAN)
VOID_STUB(DisplayScanPrepare,        MOP_DISPLAY_SCAN_PREPARE)
VOID_STUB(DoCalc,                    MOP_DO_CALC)
VOID_STUB(EepromOperateTimeDeal,     MOP_EEPROM_OPERATE_TIME_DEAL)
VOID_STUB(ErrorDeal,                 MOP_ERROR_DEAL)
VOID_STUB(ErrorReset,                MOP_ERROR_RESET)
VOID_STUB(FMPDeal,                   MOP_FMP_DEAL)
VOID_STUB(FrqSrcDeal,                MOP_FRQ_SRC_DEAL)
VOID_STUB(I2cDealBeforeInit,         MOP_I2C_DEAL_BEFORE_INIT)
VOID_STUB(InitDIGpio,                MOP_INIT_DI_GPIO)
VOID_STUB(InitDOGpio,                MOP_INIT_DO_GPIO)
VOID_STUB(InitECap1Gpio,             MOP_INIT_ECAP1_GPIO)
VOID_STUB(InitEPwm4Gpio,             MOP_INIT_EPWM4_GPIO)
VOID_STUB(InitEPwm5Gpio,             MOP_INIT_EPWM5_GPIO)
VOID_STUB(InitEPwm6Gpio,             MOP_INIT_EPWM6_GPIO)
VOID_STUB(InitI2CGpio,               MOP_INIT_I2C_GPIO)
VOID_STUB(InitSciaGpio,              MOP_INIT_SCIA_GPIO)
VOID_STUB(InitSetEPWM4,              MOP_INIT_SET_EPWM4)
VOID_STUB(InitSetEPWM5,              MOP_INIT_SET_EPWM5)
VOID_STUB(InitSetEPWM6,              MOP_INIT_SET_EPWM6)
VOID_STUB(InitSetEcap1,              MOP_INIT_SET_ECAP1)
VOID_STUB(InitSetI2ca,               MOP_INIT_SET_I2CA)
VOID_STUB(InitSetScia,               MOP_INIT_SET_SCIA)
VOID_STUB(InitSetSpiForFunc,         MOP_INIT_SET_SPI_FOR_FUNC)
VOID_STUB(InitSpiaGpio,              MOP_INIT_SPIA_GPIO)
VOID_STUB(KeyProcess,                MOP_KEY_PROCESS)
VOID_STUB(KeyScan,                   MOP_KEY_SCAN)
VOID_STUB(MenuInit,                  MOP_MENU_INIT)
VOID_STUB(OscSciFunction,            MOP_OSC_SCI_FUNCTION)
VOID_STUB(P2PDataDeal,               MOP_P2P_DATA_DEAL)
VOID_STUB(PulseInCalc,               MOP_PULSE_IN_CALC)
VOID_STUB(PulseInSample,             MOP_PULSE_IN_SAMPLE)
VOID_STUB(RunSrcDeal,                MOP_RUN_SRC_DEAL)
VOID_STUB(SciDeal,                   MOP_SCI_DEAL)
VOID_STUB(TemperatureDeal,           MOP_TEMPERATURE_DEAL)
VOID_STUB(TorqueCalc,                MOP_TORQUE_CALC)
VOID_STUB(UpdateDataCore2Func,       MOP_UPDATE_DATA_CORE2FUNC)
VOID_STUB(UpdateDataFunc2Core0p5ms,  MOP_UPDATE_DATA_FUNC2CORE_05MS)
VOID_STUB(UpdateDataFunc2Core2ms,    MOP_UPDATE_DATA_FUNC2CORE_2MS)
VOID_STUB(UpdateDisplayBuffer,       MOP_UPDATE_DISPLAY_BUFFER)
VOID_STUB(UpdateErrorDisplayBuffer,  MOP_UPDATE_ERROR_DISPLAY_BUFFER)
VOID_STUB(UpdateInvType,             MOP_UPDATE_INV_TYPE)
VOID_STUB(UpdateMotorPara,           MOP_UPDATE_MOTOR_PARA)
VOID_STUB(UpdateU0Data,              MOP_UPDATE_U0_DATA)
VOID_STUB(WinkDeal,                  MOP_WINK_DEAL)
VOID_STUB(runTimeCal,                MOP_RUN_TIME_CAL)
VOID_STUB(setTimeRun,                MOP_SET_TIME_RUN)
VOID_STUB(vfSeparateDeal,            MOP_VF_SEPARATE_DEAL)

/* ---- 带参数 / 有返回值的替身 --------------------------------------------- */

/* `AoCalcChannel(AOFMP_AO1)` / `AoCalcChannel(AOFMP_AO2)` —— 通道号是实参,
 * "写死成 AO1" 这种错法只有把实参记下来才看得见。 */
void AoCalcChannel(Uint16 channel)
{
    log_put(MOP_AO_CALC_CHANNEL, channel, 0);
    tick();
}

/* 逐字抄自 src/function/f_menu.c:1037 的 LimitDeal()。
 *
 * 为什么抄一份而不是做成空壳: f_main 的 InitFuncCode 拿它的**返回值**写回
 * funcCode.all[] —— `if (funcCode.all[i] != tmp) { funcCode.all[i] = tmp; ... }`。
 * 空壳会让那条"值被改动就登记待写"的路径彻底消失。
 *
 * 这是**改写前**的冻结副本, 两侧共用。所以"f_menu 改写把 LimitDeal 改错了"这件事
 * 本测试台看不见 —— 那是 f_menu 用例的职责, 不是这里的漏洞。 */
Uint16 LimitDeal(Uint16 signal, Uint16 data, Uint16 upper, Uint16 lower, int16 delta)
{
    int32 dataSigned;
    int32 upperSigned;
    int32 lowerSigned;

    log_put(MOP_LIMIT_DEAL, data, upper);

    if (signal)
    {
        dataSigned = (int32)(int16)data;
        upperSigned = (int32)(int16)upper;
        lowerSigned = (int32)(int16)lower;
    }
    else
    {
        dataSigned = (int32)data;
        upperSigned = (int32)upper;
        lowerSigned = (int32)lower;
    }

    dataSigned += delta;

    if (dataSigned > upperSigned)
    {
        dataSigned = upperSigned;
    }

    if (dataSigned < lowerSigned)
    {
        dataSigned = lowerSigned;
    }

    tick();

    return (Uint16)dataSigned;
}

/* EEPROM 地址映射。交付源码里这是一张表 (fcNoAttri2Eeprom / funcCodeAttribute 的
 * eepromIndex 反查), 本测试台用恒等映射: 下标就是 funcCode 的下标。
 * 这样 InitFuncCode 里 `funcCode.all[fcIndex] = data[0]` 的 fcIndex 一定落在
 * funcCode 自己的范围内, 而 START..STOP 之间的循环长度由真实字段偏移决定。 */
Uint16 GetEepromIndexFromFcIndex(Uint16 index)
{
    log_put(MOP_GET_EEPROM_INDEX_FROM_FC_INDEX, index, 0);
    tick();
    return index;
}

Uint16 GetFcIndexFromEepromIndex(Uint16 a)
{
    log_put(MOP_GET_FC_INDEX_FROM_EEPROM_INDEX, a, 0);
    tick();
    return a;
}

/* 从 gEeprom[] 按 index 取 number 个字填进 p->data[]。
 * 返回 FUNCCODE_READ_RET_READING(0) 会让 InitFuncCode 的循环原地打转 ——
 * 这是**刻意**保留的可控性: driver 只喂 OK。 */
Uint16 ReadFuncCode(FUNCCODE_RW *funcCodeRw)
{
    Uint16 i;

    log_put(MOP_READ_FUNC_CODE, funcCodeRw->index, funcCodeRw->number);

    for (i = 0; i < funcCodeRw->number; i++)
    {
        Uint16 idx = (Uint16)(funcCodeRw->index + i);

        if (idx < FMAIN_EEPROM_MAX)
        {
            funcCodeRw->data[i] = gEeprom[idx];
        }
        else
        {
            funcCodeRw->data[i] = 0;
        }
    }

    tick();
    return gReadFuncCodeRet;
}

/* 见文件头: 不清 funcCodeRwModeTmp, InitFuncCode 就永远退不出 while。 */
void EepromDeal(void)
{
    log_put(MOP_EEPROM_DEAL, funcCodeRwModeTmp, funcCodeOneWriteNum);
    funcCodeRwModeTmp = FUNCCODE_RW_MODE_NO_OPERATION;
    tick();
}

/* ---- 世界复位 ------------------------------------------------------------ */

void main_world_reset(void)
{
    Uint16 i;

    for (i = 0; i < MAIN_LOG_LEN; i++)
    {
        gMainLog[i].op = 0;
        gMainLog[i].a  = 0;
        gMainLog[i].b  = 0;
    }
    gMainLogN        = 0;
    gMainLogOverflow = 0;

    for (i = 0; i < 7; i++)
    {
        gHostCpuCtl[i] = 0;
    }

    gMockTicks      = 0;
    gMockAbortArmed = 0;
}
