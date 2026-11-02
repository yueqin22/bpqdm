/* Differential test driver for f_main.

 * f_main is the function-side top level: 18 externally visible functions, of which
 * thirteen are SEQUENCERS -- their whole meaning is "which other module gets
 * called, in what order, with what arguments". The remaining five compute:

 *   GetNumberDigit / 1 / 2   digit decomposition (one by /10 via the 52429
 *                            multiply-shift, one by >>4)
 *   SoftVersionDeal          picks the larger of two version fields
 *   CodeRunMonitor           a self-adjusting display refresh ticker
 *   getMomeryValue           reads one 16-bit word through a pointer taken from
 *                            funcCode.code.memoryAddr

 * ---- the two-worlds split -------------------------------------------------

 * The module owns 18 objects (funcCodeInitStatus, the ticker arrays, motorPara /
 * motorFc / motorSn, memoryValue, ...). All of them have external linkage and are
 * therefore renamed ref_ / new_, so each side has its own copy: the driver
 * installs both from one input image and snapshots both back.

 * Everything else is SHARED and must look identical to both sides on every call:

 *   SysCtrlRegs, CpuTimer1Regs     the module reads, writes and never allocates
 *                                  them, so the driver plays the chip
 *   funcCode, funcCodeRead,        borrowed from f_funcCode / f_eeprom. funcCode
 *   errorEeprom, dspStatus, ...    is WRITTEN by InitFuncCode and FuncVariableInit,
 *                                  funcCodeRead by ReadFuncCode's caller -- so the
 *                                  shared image has to be REINSTALLED before each
 *                                  side, not just once
 *   the 58 stub callees            plus their call log

 * ---- what is compared -----------------------------------------------------

 *   per-side : CODE_RUN_DISPLAY_UPDATE_TIME, aiaoChckReadOK, driveCoeff,
 *              funcCodeInitStatus, mainLoopTicker, memoryValue, motorPara,
 *              motorFc, motorSn, powerOnStatus, the six ticker arrays/scalars,
 *              timeBase, timeBase05msOld
 *   shared   : SysCtrlRegs + CpuTimer1Regs, cpuCtl[7] (EALLOW/EDIS),
 *              funcCode, funcCodeRead, funcCodeRwModeTmp, funcCodeOneWriteNum,
 *              errorCode, errorEeprom, canLinkTicker, commTicker, dspMainCmd1,
 *              dspStatus, frqAimTmp0, frqCalcSrcOld, gSendToFunctionDataBuff,
 *              limitedByOtherCodeIndexNum, plcStep/RemOld, plcTime/RemOld,
 *              pmsmRotorPos, scanStep, spiData, upDownFrqInit
 *   outputs  : the call log (as a SEQUENCE), funcCodeOneWriteIndex[0..num),
 *              limitedByOtherCodeIndex[0..num)

 * ---- why the phases are mostly SEQUENCES ----------------------------------

 * Five pieces of state are function-scope statics and cannot be installed from
 * outside:

 *   Main05msFunctionEachStart   waitPowerOnTicker -- only reaches
 *                               WAIT_POWER_ON_TIME (10000) after 10000 calls
 *   Main05msFunctionEachEnd     ticker 0..3 wrap
 *   CodeRunMonitor              ticker, threshold = CODE_RUN_DISPLAY_UPDATE_TIME
 *                               / FUNC_DEAL_PERIOD
 *   InitFuncCode                the `static Uint16 flag` of the AI/AO calibration
 *                               read loop, which adds 4 to readIndex every 4th
 *                               successful read

 * All four are therefore stepped in LOCKSTEP: ref and new each get the identical
 * call sequence and the comparison happens after every single call, so a
 * divergence surfaces on the call that caused it.

 * ---- host-unrepresentable / deliberately unreached regions ----------------

 *   (1) `getMomeryValue` dereferences a 16-bit ADDRESS. host_patch.json rewrites
 *       `(Uint16*)MEMORY_ADDRESS` to `(Uint16*)(gHostMemory + MEMORY_ADDRESS)`,
 *       a 64K-word table, so the read is defined and the index is still the
 *       address. memoryAddr is swept over a set that includes both ends.

 *   (2) `asm(" nop")` / `asm(" ESTOP0")` are C28x-only; host_patch.json replaces
 *       them with null statements. They produce no observable state.

 *   (3) The `else if (errorEeprom)` arm of InitFuncCode ends in `for (;;)`. It is
 *       reachable only by setting errorEeprom != 0, and once entered it never
 *       returns -- so the bench keeps errorEeprom == 0 throughout and does NOT
 *       claim any coverage of that arm. It is not a hole in the bench that could
 *       be plugged: the arm is non-terminating by construction. A mutation
 *       anchored there would come out as HUNG, not MISSED, and is therefore
 *       excluded from the catalogue.

 *   (4) `FUNCCODE_ALL`, `union MOTOR_PARA`, `struct MOTOR_FC` are compared with
 *       memcmp. They are unions of a `Uint16[]` and a bitfield struct, so the
 *       comparison is exact -- but padding bytes, if any, are host layout, and
 *       this bench says nothing about target padding.

 *   (5) funcCodeOneWriteIndex / limitedByOtherCodeIndex are sized 2048 here
 *       against 200 / 150 in the deliverable; see mock_main.h. The counts
 *       (funcCodeOneWriteNum / limitedByOtherCodeIndexNum) are compared as-is.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DSP28x_Project.h"
#include "mock_main.h"

#define FAIL_LIMIT 20

/* ---- the file-private enum of f_main.c -----------------------------------
 *
 * `enum FUNC_CODE_INIT_STATUS` is declared inside f_main.c and in no header, so
 * it is re-declared here with the same enumerators. C says an enumerated type is
 * compatible with its underlying integer type, and MSVC picks `int` for both
 * sides, so the objects agree in size and linkage.
 */
enum FUNC_CODE_INIT_STATUS
{
    FUNCCODE_INIT_STATUS_VERIFY_EEPROM,
    FUNCCODE_INIT_STATUS_NEW_EEPROM,
    FUNCCODE_INIT_STATUS_OLD_EEPROM,
    FUNCCODE_INIT_STATUS_OVER
};

/* ---- ref side ------------------------------------------------------------ */

extern void   ref_InitForFunctionApp(void);
extern void   ref_InitDspFunc(void);
extern void   ref_InitFuncCode(void);
extern void   ref_FuncVariableInit(void);
extern void   ref_Main2msFunction(void);
extern void   ref_Main0msFunction(void);
extern void   ref_Main05msFunctionEachStart(void);
extern void   ref_Main05msFunctionEachEnd(void);
extern void   ref_Main05msFunctionA(void);
extern void   ref_Main05msFunctionB(void);
extern void   ref_Main05msFunctionC(void);
extern void   ref_Main05msFunctionD(void);
extern void   ref_CodeRunMonitor(void);
extern Uint16 ref_GetNumberDigit(Uint16 digit[5], Uint16 number, Uint16 mode);
extern void   ref_GetNumberDigit1(Uint16 digit[5], Uint16 number);
extern void   ref_GetNumberDigit2(Uint16 digit[5], Uint16 number);
extern void   ref_SoftVersionDeal(void);
extern void   ref_getMomeryValue(void);

extern Uint16 ref_CODE_RUN_DISPLAY_UPDATE_TIME;
extern Uint16 ref_aiaoChckReadOK;
extern Uint16 ref_driveCoeff[2][4];
extern enum FUNC_CODE_INIT_STATUS ref_funcCodeInitStatus;
extern Uint16 ref_mainLoopTicker;
extern Uint16 ref_memoryValue;
extern union MOTOR_PARA ref_motorPara;
extern struct MOTOR_FC  ref_motorFc;
extern enum MOTOR_SN    ref_motorSn;
extern enum POWER_ON_STATUS ref_powerOnStatus;
extern Uint32 ref_ticker05msFuncEa[4];
extern Uint32 ref_ticker05msFuncEach;
extern Uint16 ref_ticker05msFuncEachDisp[4];
extern Uint32 ref_ticker05msFuncIn[4];
extern Uint32 ref_ticker05msFuncInternal;
extern Uint16 ref_ticker05msFuncInternalDisp[4];
extern Uint32 ref_timeBase;
extern Uint32 ref_timeBase05msOld;

/* ---- new side ------------------------------------------------------------ */

extern void   new_InitForFunctionApp(void);
extern void   new_InitDspFunc(void);
extern void   new_InitFuncCode(void);
extern void   new_FuncVariableInit(void);
extern void   new_Main2msFunction(void);
extern void   new_Main0msFunction(void);
extern void   new_Main05msFunctionEachStart(void);
extern void   new_Main05msFunctionEachEnd(void);
extern void   new_Main05msFunctionA(void);
extern void   new_Main05msFunctionB(void);
extern void   new_Main05msFunctionC(void);
extern void   new_Main05msFunctionD(void);
extern void   new_CodeRunMonitor(void);
extern Uint16 new_GetNumberDigit(Uint16 digit[5], Uint16 number, Uint16 mode);
extern void   new_GetNumberDigit1(Uint16 digit[5], Uint16 number);
extern void   new_GetNumberDigit2(Uint16 digit[5], Uint16 number);
extern void   new_SoftVersionDeal(void);
extern void   new_getMomeryValue(void);

extern Uint16 new_CODE_RUN_DISPLAY_UPDATE_TIME;
extern Uint16 new_aiaoChckReadOK;
extern Uint16 new_driveCoeff[2][4];
extern enum FUNC_CODE_INIT_STATUS new_funcCodeInitStatus;
extern Uint16 new_mainLoopTicker;
extern Uint16 new_memoryValue;
extern union MOTOR_PARA new_motorPara;
extern struct MOTOR_FC  new_motorFc;
extern enum MOTOR_SN    new_motorSn;
extern enum POWER_ON_STATUS new_powerOnStatus;
extern Uint32 new_ticker05msFuncEa[4];
extern Uint32 new_ticker05msFuncEach;
extern Uint16 new_ticker05msFuncEachDisp[4];
extern Uint32 new_ticker05msFuncIn[4];
extern Uint32 new_ticker05msFuncInternal;
extern Uint16 new_ticker05msFuncInternalDisp[4];
extern Uint32 new_timeBase;
extern Uint32 new_timeBase05msOld;

/* ---- per-side snapshot --------------------------------------------------- */

struct SIDE
{
    Uint16  aborted;            /* 1 = 没自己退出, 被停机预算强停 (见 mock_main.h) */
    Uint16  crdut;
    Uint16  aiaoChckReadOK;
    Uint16  driveCoeff[2][4];
    enum FUNC_CODE_INIT_STATUS funcCodeInitStatus;
    Uint16  mainLoopTicker;
    Uint16  memoryValue;
    union MOTOR_PARA motorPara;
    struct MOTOR_FC  motorFc;
    enum MOTOR_SN    motorSn;
    enum POWER_ON_STATUS powerOnStatus;
    Uint32  ticker05msFuncEa[4];
    Uint32  ticker05msFuncEach;
    Uint16  ticker05msFuncEachDisp[4];
    Uint32  ticker05msFuncIn[4];
    Uint32  ticker05msFuncInternal;
    Uint16  ticker05msFuncInternalDisp[4];
    Uint32  timeBase;
    Uint32  timeBase05msOld;
};

static struct SIDE inSide;
static struct SIDE S[2];

#define SIDE_INSTALL(P)                                                       \
    do {                                                                      \
        P##CODE_RUN_DISPLAY_UPDATE_TIME = inSide.crdut;                       \
        P##aiaoChckReadOK  = inSide.aiaoChckReadOK;                           \
        memcpy(P##driveCoeff, inSide.driveCoeff, sizeof inSide.driveCoeff);   \
        P##funcCodeInitStatus = inSide.funcCodeInitStatus;                    \
        P##mainLoopTicker  = inSide.mainLoopTicker;                           \
        P##memoryValue     = inSide.memoryValue;                              \
        memcpy(&P##motorPara, &inSide.motorPara, sizeof inSide.motorPara);    \
        memcpy(&P##motorFc,   &inSide.motorFc,   sizeof inSide.motorFc);      \
        P##motorSn         = inSide.motorSn;                                  \
        P##powerOnStatus   = inSide.powerOnStatus;                            \
        memcpy(P##ticker05msFuncEa, inSide.ticker05msFuncEa,                  \
               sizeof inSide.ticker05msFuncEa);                               \
        P##ticker05msFuncEach = inSide.ticker05msFuncEach;                    \
        memcpy(P##ticker05msFuncEachDisp, inSide.ticker05msFuncEachDisp,      \
               sizeof inSide.ticker05msFuncEachDisp);                         \
        memcpy(P##ticker05msFuncIn, inSide.ticker05msFuncIn,                  \
               sizeof inSide.ticker05msFuncIn);                               \
        P##ticker05msFuncInternal = inSide.ticker05msFuncInternal;            \
        memcpy(P##ticker05msFuncInternalDisp,                                 \
               inSide.ticker05msFuncInternalDisp,                             \
               sizeof inSide.ticker05msFuncInternalDisp);                     \
        P##timeBase        = inSide.timeBase;                                 \
        P##timeBase05msOld = inSide.timeBase05msOld;                          \
    } while (0)

#define SIDE_SNAPSHOT(P, s)                                                   \
    do {                                                                      \
        (s)->crdut         = P##CODE_RUN_DISPLAY_UPDATE_TIME;                 \
        (s)->aiaoChckReadOK = P##aiaoChckReadOK;                              \
        memcpy((s)->driveCoeff, P##driveCoeff, sizeof (s)->driveCoeff);       \
        (s)->funcCodeInitStatus = P##funcCodeInitStatus;                      \
        (s)->mainLoopTicker = P##mainLoopTicker;                              \
        (s)->memoryValue    = P##memoryValue;                                 \
        memcpy(&(s)->motorPara, &P##motorPara, sizeof (s)->motorPara);        \
        memcpy(&(s)->motorFc,   &P##motorFc,   sizeof (s)->motorFc);          \
        (s)->motorSn        = P##motorSn;                                     \
        (s)->powerOnStatus  = P##powerOnStatus;                               \
        memcpy((s)->ticker05msFuncEa, P##ticker05msFuncEa,                    \
               sizeof (s)->ticker05msFuncEa);                                 \
        (s)->ticker05msFuncEach = P##ticker05msFuncEach;                      \
        memcpy((s)->ticker05msFuncEachDisp, P##ticker05msFuncEachDisp,        \
               sizeof (s)->ticker05msFuncEachDisp);                           \
        memcpy((s)->ticker05msFuncIn, P##ticker05msFuncIn,                    \
               sizeof (s)->ticker05msFuncIn);                                 \
        (s)->ticker05msFuncInternal = P##ticker05msFuncInternal;              \
        memcpy((s)->ticker05msFuncInternalDisp,                               \
               P##ticker05msFuncInternalDisp,                                 \
               sizeof (s)->ticker05msFuncInternalDisp);                       \
        (s)->timeBase       = P##timeBase;                                    \
        (s)->timeBase05msOld = P##timeBase05msOld;                            \
    } while (0)

static void install_side(int side)
{
    if (side == 0)
    {
        SIDE_INSTALL(ref_);
    }
    else
    {
        SIDE_INSTALL(new_);
    }
}

static void snapshot(int side)
{
    if (side == 0)
    {
        SIDE_SNAPSHOT(ref_, &S[0]);
    }
    else
    {
        SIDE_SNAPSHOT(new_, &S[1]);
    }
}

/* ---- the shared world ---------------------------------------------------- */

struct WORLD
{
    unsigned long          ticks;    /* 这一侧这次调用推了多少拍替身 */

    struct SYS_CTRL_REGS   sysctrl;
    struct CPUTIMER_REGS   timer;
    Uint16                 cpu[7];

    FUNCCODE_ALL           funcCode;
    FUNCCODE_RW            funcCodeRead;
    Uint16                 rwModeTmp;

    Uint16                 errorCode;
    Uint16                 errorEeprom;

    Uint32                 canLinkTicker;
    Uint32                 commTicker;
    union DSP_MAIN_COMMAND1 dspMainCmd1;
    union DSP_STATUS       dspStatus;

    int32                  frqAimTmp0;
    Uint16                 frqCalcSrcOld;

    int16                  sendBuff[64];
    Uint16                 limitedNum;

    Uint16                 plcStep;
    Uint16                 plcStepRemOld;
    Uint32                 plcTime;
    Uint32                 plcTimeRemOld;

    Uint16                 pmsmRotorPos;
    Uint16                 scanStep;
    struct SPI_IN_OUT      spiData;
    Uint16                 displayBuffer[DISPLAY_8LED_NUM + 1];
    Uint16                 upDownFrqInit;

    Uint32                 timeStep;
    Uint16                 readRet;
};

static struct WORLD inW;
static struct WORLD SH[2];

static void install_world(void)
{
    int i;

    memcpy((void *)&SysCtrlRegs,   &inW.sysctrl, sizeof inW.sysctrl);
    memcpy((void *)&CpuTimer1Regs, &inW.timer,   sizeof inW.timer);

    for (i = 0; i < 7; i++)
    {
        gHostCpuCtl[i] = inW.cpu[i];
    }

    memcpy(&funcCode,       &inW.funcCode,     sizeof inW.funcCode);
    memcpy(&funcCodeRead,   &inW.funcCodeRead, sizeof inW.funcCodeRead);
    funcCodeRwModeTmp = inW.rwModeTmp;

    errorCode   = inW.errorCode;
    errorEeprom = inW.errorEeprom;

    canLinkTicker = inW.canLinkTicker;
    commTicker    = inW.commTicker;
    memcpy(&dspMainCmd1, &inW.dspMainCmd1, sizeof inW.dspMainCmd1);
    memcpy(&dspStatus,   &inW.dspStatus,   sizeof inW.dspStatus);

    frqAimTmp0    = inW.frqAimTmp0;
    frqCalcSrcOld = inW.frqCalcSrcOld;

    memcpy(gSendToFunctionDataBuff, inW.sendBuff, sizeof inW.sendBuff);
    limitedByOtherCodeIndexNum = inW.limitedNum;

    plcStep       = inW.plcStep;
    plcStepRemOld = inW.plcStepRemOld;
    plcTime       = inW.plcTime;
    plcTimeRemOld = inW.plcTimeRemOld;

    pmsmRotorPos = inW.pmsmRotorPos;
    scanStep     = inW.scanStep;
    memcpy(&spiData, &inW.spiData, sizeof inW.spiData);

    /* displayBuffer 是 f_main 的**读**输入 (第 316 行
     * `spiData.out.a.bit.dispCode = (displayBuffer[scanStep] & 0x00ff);`), 正常由
     * 替身 UpdateDisplayBuffer() 填 —— 但替身不填, 所以必须由 driver 自己当输入喂。
     * 首轮漏了这一步, 数组恒为全 0, 于是 `& 0x00ff` 和 `& 0xff00` 都得 0,
     * FM45 报 MISSED 而看着像等价变异。 */
    memcpy(displayBuffer, inW.displayBuffer, sizeof inW.displayBuffer);

    upDownFrqInit = inW.upDownFrqInit;

    gTimeStep        = inW.timeStep;
    gReadFuncCodeRet = inW.readRet;

    /* Outputs. funcCodeOneWriteNum / limitedByOtherCodeIndexNum are zeroed so
     * "how many writes did this call ask for" is measured per call rather than
     * accumulated. The arrays themselves are not cleared: only the first `num`
     * entries are ever looked at (the snapshot copies exactly that prefix). */
    funcCodeOneWriteNum        = 0;
    limitedByOtherCodeIndexNum = 0;

    main_world_reset();
}

static void snap_world(int side)
{
    struct WORLD *s = &SH[side];
    int i;

    s->ticks = gMockTicks;

    memcpy(&s->sysctrl, (const void *)&SysCtrlRegs,   sizeof s->sysctrl);
    memcpy(&s->timer,   (const void *)&CpuTimer1Regs, sizeof s->timer);

    for (i = 0; i < 7; i++)
    {
        s->cpu[i] = gHostCpuCtl[i];
    }

    memcpy(&s->funcCode,     &funcCode,     sizeof s->funcCode);
    memcpy(&s->funcCodeRead, &funcCodeRead, sizeof s->funcCodeRead);
    s->rwModeTmp = funcCodeRwModeTmp;

    s->errorCode   = errorCode;
    s->errorEeprom = errorEeprom;

    s->canLinkTicker = canLinkTicker;
    s->commTicker    = commTicker;
    memcpy(&s->dspMainCmd1, &dspMainCmd1, sizeof s->dspMainCmd1);
    memcpy(&s->dspStatus,   &dspStatus,   sizeof s->dspStatus);

    s->frqAimTmp0    = frqAimTmp0;
    s->frqCalcSrcOld = frqCalcSrcOld;

    memcpy(s->sendBuff, gSendToFunctionDataBuff, sizeof s->sendBuff);
    s->limitedNum = limitedByOtherCodeIndexNum;

    s->plcStep       = plcStep;
    s->plcStepRemOld = plcStepRemOld;
    s->plcTime       = plcTime;
    s->plcTimeRemOld = plcTimeRemOld;

    s->pmsmRotorPos = pmsmRotorPos;
    s->scanStep     = scanStep;
    memcpy(&s->spiData, &spiData, sizeof s->spiData);
    s->upDownFrqInit = upDownFrqInit;

    s->timeStep = gTimeStep;
    s->readRet  = gReadFuncCodeRet;
}

/* ---- outputs (call log + the two write registries) ----------------------- */

struct OUTS
{
    Uint16              logN;
    Uint16              logOvf;
    struct MAIN_LOG_ENT log[MAIN_LOG_LEN];

    Uint16              oneNum;
    Uint16              oneIdx[FMAIN_ONE_WRITE_MAX];

    Uint16              limNum;
    Uint16              limIdx[FMAIN_LIMITED_MAX];
};

static struct OUTS O[2];

static void snap_outs(int side)
{
    struct OUTS *o = &O[side];

    o->logN   = gMainLogN;
    o->logOvf = gMainLogOverflow;
    if (o->logN)
    {
        memcpy(o->log, gMainLog, o->logN * sizeof(struct MAIN_LOG_ENT));
    }

    o->oneNum = funcCodeOneWriteNum;
    if (o->oneNum)
    {
        Uint16 n = o->oneNum < FMAIN_ONE_WRITE_MAX ? o->oneNum
                                                   : FMAIN_ONE_WRITE_MAX;
        memcpy(o->oneIdx, funcCodeOneWriteIndex, n * sizeof(Uint16));
    }

    o->limNum = limitedByOtherCodeIndexNum;
    if (o->limNum)
    {
        Uint16 n = o->limNum < FMAIN_LIMITED_MAX ? o->limNum : FMAIN_LIMITED_MAX;
        memcpy(o->limIdx, limitedByOtherCodeIndex, n * sizeof(Uint16));
    }
}

/* ---- reporting ----------------------------------------------------------- */

static long g_cases = 0;
static int  g_fails = 0;
static const char *g_cur = "";

static void report(const char *what)
{
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s: %s\n", g_cur, what);
    printf("  funcCodeInitStatus=%d powerOnStatus=%d errorEeprom=%u "
           "readRet=%u timeStep=%lu\n",
           (int)inSide.funcCodeInitStatus, (int)inSide.powerOnStatus,
           (unsigned)inW.errorEeprom, (unsigned)inW.readRet,
           (unsigned long)inW.timeStep);
    printf("  frqPoint=%u frqXSrc=%u frqYSrc=%u pmsmRotorPos=%u presetFrq=%u\n",
           (unsigned)funcCode.code.frqPoint, (unsigned)funcCode.code.frqXSrc,
           (unsigned)funcCode.code.frqYSrc,
           (unsigned)funcCode.code.pmsmRotorPos,
           (unsigned)funcCode.code.presetFrq);
    printf("  memoryAddr=%u scanStep=%u rwModeTmp=%u oneWriteNum=%u/%u "
           "limNum=%u/%u\n",
           (unsigned)funcCode.code.memoryAddr, (unsigned)inW.scanStep,
           (unsigned)inW.rwModeTmp,
           (unsigned)O[0].oneNum, (unsigned)O[1].oneNum,
           (unsigned)O[0].limNum, (unsigned)O[1].limNum);
    printf("  dspStatus=0x%04X err=%u/%u ticker=%lu/%lu timeBase=%lu/%lu\n",
           (unsigned)inW.dspStatus.all,
           (unsigned)inW.errorCode, (unsigned)inW.errorEeprom,
           (unsigned long)S[0].timeBase, (unsigned long)S[1].timeBase,
           (unsigned long)S[0].timeBase05msOld,
           (unsigned long)S[1].timeBase05msOld);
    printf("  logN %u/%u ovf %u/%u cpuEALLOW %u/%u\n",
           (unsigned)O[0].logN, (unsigned)O[1].logN,
           (unsigned)O[0].logOvf, (unsigned)O[1].logOvf,
           (unsigned)SH[0].cpu[4], (unsigned)SH[1].cpu[4]);
    printf("  aborted %u/%u ticks %lu/%lu (budget %lu)\n",
           (unsigned)S[0].aborted, (unsigned)S[1].aborted,
           SH[0].ticks, SH[1].ticks, (unsigned long)MOCK_TICK_BUDGET);
}

static void cmp(const char *what, const void *a, const void *b, size_t n)
{
    if (memcmp(a, b, n) != 0)
    {
        report(what);
    }
}

static void compare(void)
{
    Uint16 n;

    /* 终止性: aborted 是"这一侧没自己退出, 被停机预算强停", ticks 是"这一趟一共
     * 推了多少拍替身"。两条都放在最前面 —— 它们一不一样, 决定后面那些字段值
     * 是"算出来的"还是"卡在半路上留下的"。 */
    cmp("aborted", &S[0].aborted, &S[1].aborted, sizeof S[0].aborted);
    cmp("ticks", &SH[0].ticks, &SH[1].ticks, sizeof SH[0].ticks);

    cmp("CODE_RUN_DISPLAY_UPDATE_TIME", &S[0].crdut, &S[1].crdut,
        sizeof S[0].crdut);
    cmp("aiaoChckReadOK", &S[0].aiaoChckReadOK, &S[1].aiaoChckReadOK,
        sizeof S[0].aiaoChckReadOK);
    cmp("driveCoeff", S[0].driveCoeff, S[1].driveCoeff, sizeof S[0].driveCoeff);
    cmp("funcCodeInitStatus", &S[0].funcCodeInitStatus, &S[1].funcCodeInitStatus,
        sizeof S[0].funcCodeInitStatus);
    cmp("mainLoopTicker", &S[0].mainLoopTicker, &S[1].mainLoopTicker,
        sizeof S[0].mainLoopTicker);
    cmp("memoryValue", &S[0].memoryValue, &S[1].memoryValue,
        sizeof S[0].memoryValue);
    cmp("motorPara", &S[0].motorPara, &S[1].motorPara, sizeof S[0].motorPara);
    cmp("motorFc",   &S[0].motorFc,   &S[1].motorFc,   sizeof S[0].motorFc);
    cmp("motorSn",   &S[0].motorSn,   &S[1].motorSn,   sizeof S[0].motorSn);
    cmp("powerOnStatus", &S[0].powerOnStatus, &S[1].powerOnStatus,
        sizeof S[0].powerOnStatus);
    cmp("ticker05msFuncEa", S[0].ticker05msFuncEa, S[1].ticker05msFuncEa,
        sizeof S[0].ticker05msFuncEa);
    cmp("ticker05msFuncEach", &S[0].ticker05msFuncEach, &S[1].ticker05msFuncEach,
        sizeof S[0].ticker05msFuncEach);
    cmp("ticker05msFuncEachDisp", S[0].ticker05msFuncEachDisp,
        S[1].ticker05msFuncEachDisp, sizeof S[0].ticker05msFuncEachDisp);
    cmp("ticker05msFuncIn", S[0].ticker05msFuncIn, S[1].ticker05msFuncIn,
        sizeof S[0].ticker05msFuncIn);
    cmp("ticker05msFuncInternal", &S[0].ticker05msFuncInternal,
        &S[1].ticker05msFuncInternal, sizeof S[0].ticker05msFuncInternal);
    cmp("ticker05msFuncInternalDisp", S[0].ticker05msFuncInternalDisp,
        S[1].ticker05msFuncInternalDisp, sizeof S[0].ticker05msFuncInternalDisp);
    cmp("timeBase", &S[0].timeBase, &S[1].timeBase, sizeof S[0].timeBase);
    cmp("timeBase05msOld", &S[0].timeBase05msOld, &S[1].timeBase05msOld,
        sizeof S[0].timeBase05msOld);

    cmp("SysCtrlRegs",  &SH[0].sysctrl, &SH[1].sysctrl, sizeof SH[0].sysctrl);
    cmp("CpuTimer1Regs", &SH[0].timer,  &SH[1].timer,   sizeof SH[0].timer);
    cmp("cpuCtl",       SH[0].cpu, SH[1].cpu, sizeof SH[0].cpu);

    cmp("funcCode",     &SH[0].funcCode,     &SH[1].funcCode,
        sizeof SH[0].funcCode);
    cmp("funcCodeRead", &SH[0].funcCodeRead, &SH[1].funcCodeRead,
        sizeof SH[0].funcCodeRead);
    cmp("funcCodeRwModeTmp", &SH[0].rwModeTmp, &SH[1].rwModeTmp,
        sizeof SH[0].rwModeTmp);
    cmp("errorCode",   &SH[0].errorCode,   &SH[1].errorCode,
        sizeof SH[0].errorCode);
    cmp("errorEeprom", &SH[0].errorEeprom, &SH[1].errorEeprom,
        sizeof SH[0].errorEeprom);
    cmp("canLinkTicker", &SH[0].canLinkTicker, &SH[1].canLinkTicker,
        sizeof SH[0].canLinkTicker);
    cmp("commTicker", &SH[0].commTicker, &SH[1].commTicker,
        sizeof SH[0].commTicker);
    cmp("dspMainCmd1", &SH[0].dspMainCmd1, &SH[1].dspMainCmd1,
        sizeof SH[0].dspMainCmd1);
    cmp("dspStatus", &SH[0].dspStatus, &SH[1].dspStatus,
        sizeof SH[0].dspStatus);
    cmp("frqAimTmp0", &SH[0].frqAimTmp0, &SH[1].frqAimTmp0,
        sizeof SH[0].frqAimTmp0);
    cmp("frqCalcSrcOld", &SH[0].frqCalcSrcOld, &SH[1].frqCalcSrcOld,
        sizeof SH[0].frqCalcSrcOld);
    cmp("gSendToFunctionDataBuff", SH[0].sendBuff, SH[1].sendBuff,
        sizeof SH[0].sendBuff);
    cmp("plcStep", &SH[0].plcStep, &SH[1].plcStep, sizeof SH[0].plcStep);
    cmp("plcStepRemOld", &SH[0].plcStepRemOld, &SH[1].plcStepRemOld,
        sizeof SH[0].plcStepRemOld);
    cmp("plcTime", &SH[0].plcTime, &SH[1].plcTime, sizeof SH[0].plcTime);
    cmp("plcTimeRemOld", &SH[0].plcTimeRemOld, &SH[1].plcTimeRemOld,
        sizeof SH[0].plcTimeRemOld);
    cmp("pmsmRotorPos", &SH[0].pmsmRotorPos, &SH[1].pmsmRotorPos,
        sizeof SH[0].pmsmRotorPos);
    cmp("scanStep", &SH[0].scanStep, &SH[1].scanStep, sizeof SH[0].scanStep);
    cmp("spiData", &SH[0].spiData, &SH[1].spiData, sizeof SH[0].spiData);
    cmp("upDownFrqInit", &SH[0].upDownFrqInit, &SH[1].upDownFrqInit,
        sizeof SH[0].upDownFrqInit);

    /* The call log as a SEQUENCE. The two prefixes are compared up to the
     * shorter one; the counts are compared separately, so "same length,
     * different order" and "different length" are both caught. */
    n = O[0].logN < O[1].logN ? O[0].logN : O[1].logN;
    if (n)
    {
        cmp("mainLog", O[0].log, O[1].log,
            (size_t)n * sizeof(struct MAIN_LOG_ENT));
    }
    cmp("mainLogN", &O[0].logN, &O[1].logN, sizeof O[0].logN);
    cmp("mainLogOverflow", &O[0].logOvf, &O[1].logOvf, sizeof O[0].logOvf);

    cmp("funcCodeOneWriteNum", &O[0].oneNum, &O[1].oneNum, sizeof O[0].oneNum);
    n = O[0].oneNum < O[1].oneNum ? O[0].oneNum : O[1].oneNum;
    if (n)
    {
        cmp("funcCodeOneWriteIndex", O[0].oneIdx, O[1].oneIdx,
            (size_t)n * sizeof(Uint16));
    }

    cmp("limitedByOtherCodeIndexNum", &O[0].limNum, &O[1].limNum,
        sizeof O[0].limNum);
    n = O[0].limNum < O[1].limNum ? O[0].limNum : O[1].limNum;
    if (n)
    {
        cmp("limitedByOtherCodeIndex", O[0].limIdx, O[1].limIdx,
            (size_t)n * sizeof(Uint16));
    }
}

/* ---- runner -------------------------------------------------------------- */

typedef void (*void_fn)(void);

#define FN_N 15

static void_fn g_refFn[FN_N];
static void_fn g_newFn[FN_N];
static const char *g_fnName[FN_N] = {
    "InitForFunctionApp", "InitDspFunc", "InitFuncCode", "FuncVariableInit",
    "Main05msFunctionEachStart", "Main05msFunctionEachEnd",
    "Main05msFunctionA", "Main05msFunctionB", "Main05msFunctionC",
    "Main05msFunctionD", "CodeRunMonitor", "Main0msFunction",
    "Main2msFunction", "SoftVersionDeal", "getMomeryValue"
};

static void init_ptrs(void)
{
    g_refFn[0]  = ref_InitForFunctionApp;       g_newFn[0]  = new_InitForFunctionApp;
    g_refFn[1]  = ref_InitDspFunc;              g_newFn[1]  = new_InitDspFunc;
    g_refFn[2]  = ref_InitFuncCode;             g_newFn[2]  = new_InitFuncCode;
    g_refFn[3]  = ref_FuncVariableInit;         g_newFn[3]  = new_FuncVariableInit;
    g_refFn[4]  = ref_Main05msFunctionEachStart; g_newFn[4] = new_Main05msFunctionEachStart;
    g_refFn[5]  = ref_Main05msFunctionEachEnd;  g_newFn[5]  = new_Main05msFunctionEachEnd;
    g_refFn[6]  = ref_Main05msFunctionA;        g_newFn[6]  = new_Main05msFunctionA;
    g_refFn[7]  = ref_Main05msFunctionB;        g_newFn[7]  = new_Main05msFunctionB;
    g_refFn[8]  = ref_Main05msFunctionC;        g_newFn[8]  = new_Main05msFunctionC;
    g_refFn[9]  = ref_Main05msFunctionD;        g_newFn[9]  = new_Main05msFunctionD;
    g_refFn[10] = ref_CodeRunMonitor;           g_newFn[10] = new_CodeRunMonitor;
    g_refFn[11] = ref_Main0msFunction;          g_newFn[11] = new_Main0msFunction;
    g_refFn[12] = ref_Main2msFunction;          g_newFn[12] = new_Main2msFunction;
    g_refFn[13] = ref_SoftVersionDeal;          g_newFn[13] = new_SoftVersionDeal;
    g_refFn[14] = ref_getMomeryValue;           g_newFn[14] = new_getMomeryValue;
}

/* 一次调用的最大实测拍数, 末尾打印。用来把"预算远高于实际用量"变成落盘数字 ——
 * 否则"预算够大"只是一句声明。 */
static unsigned long g_maxTicks = 0;

/* One lockstep step: install the world, run ref, snapshot; install the SAME
 * world again, run new, snapshot; compare. Module-private state is NOT
 * reinstalled, so a sequence of steps carries it forward on both sides at once.
 *
 * The setjmp/longjmp pair around each side is the termination oracle: if the
 * module enters a handshake loop that cannot exit, the mock's tick budget fires
 * and lands back here, so the case is still compared field by field instead of
 * timing out. See mock_main.h.
 */
static void step(int which)
{
    install_world();
    gMockAbortArmed = 1;
    if (setjmp(gMockAbort) == 0)
    {
        g_refFn[which]();
        S[0].aborted = 0;
    }
    else
    {
        S[0].aborted = 1;
    }
    gMockAbortArmed = 0;
    snapshot(0);
    snap_world(0);
    snap_outs(0);
    if (SH[0].ticks > g_maxTicks)
    {
        g_maxTicks = SH[0].ticks;
    }

    install_world();
    gMockAbortArmed = 1;
    if (setjmp(gMockAbort) == 0)
    {
        g_newFn[which]();
        S[1].aborted = 0;
    }
    else
    {
        S[1].aborted = 1;
    }
    gMockAbortArmed = 0;
    snapshot(1);
    snap_world(1);
    snap_outs(1);
    if (SH[1].ticks > g_maxTicks)
    {
        g_maxTicks = SH[1].ticks;
    }

    g_cases++;
    compare();
}

/* A one-shot call: module state is reinstalled from the input image first. */
static void fresh(int which)
{
    install_side(0);
    install_side(1);
    step(which);
}

/* ---- deterministic input fill -------------------------------------------- */

static unsigned long g_rng = 0x20261010UL;

static unsigned long rnd(void)
{
    g_rng = g_rng * 1103515245UL + 12345UL;
    g_rng ^= g_rng >> 7;
    g_rng *= 2654435761UL;
    return (g_rng >> 8) & 0xFFFFFFUL;
}

static void rnd_fill(void *p, size_t n)
{
    unsigned char *b = (unsigned char *)p;
    size_t i;

    for (i = 0; i < n; i++)
    {
        b[i] = (unsigned char)(rnd() >> 4);
    }
}

/* Non-trivial prefill for every module-owned input. The reason is the one
 * stated in the bench README: ref-vs-new only ever sees a change on the NEW
 * side, so "the rewrite stopped writing this field" is visible only if the
 * position already held something other than what correct code writes.
 */
static void side_begin(unsigned long seed)
{
    g_rng = seed ? seed : 1UL;

    inSide.crdut         = (Uint16)(1 + rnd() % 400);
    inSide.aiaoChckReadOK = (Uint16)(rnd() % 2);
    rnd_fill(inSide.driveCoeff, sizeof inSide.driveCoeff);
    inSide.funcCodeInitStatus =
        (enum FUNC_CODE_INIT_STATUS)(rnd() % 4);
    inSide.mainLoopTicker = (Uint16)(rnd() % 2);
    inSide.memoryValue    = (Uint16)rnd();

    rnd_fill(&inSide.motorPara, sizeof inSide.motorPara);
    rnd_fill(&inSide.motorFc,   sizeof inSide.motorFc);
    inSide.motorSn = (enum MOTOR_SN)(rnd() % 4);

    inSide.powerOnStatus = (enum POWER_ON_STATUS)(rnd() % 3);

    rnd_fill(inSide.ticker05msFuncEa, sizeof inSide.ticker05msFuncEa);
    inSide.ticker05msFuncEach = (Uint32)rnd();
    rnd_fill(inSide.ticker05msFuncEachDisp,
             sizeof inSide.ticker05msFuncEachDisp);
    rnd_fill(inSide.ticker05msFuncIn, sizeof inSide.ticker05msFuncIn);
    inSide.ticker05msFuncInternal = (Uint32)rnd();
    rnd_fill(inSide.ticker05msFuncInternalDisp,
             sizeof inSide.ticker05msFuncInternalDisp);

    inSide.timeBase        = (Uint32)rnd();
    inSide.timeBase05msOld = (Uint32)rnd();
}

static void world_begin(unsigned long seed)
{
    int i;

    g_rng = seed ? seed : 1UL;

    rnd_fill(&inW.sysctrl, sizeof inW.sysctrl);
    rnd_fill(&inW.timer,   sizeof inW.timer);
    for (i = 0; i < 7; i++)
    {
        inW.cpu[i] = (Uint16)(rnd() % 8);
    }

    rnd_fill(&inW.funcCode,     sizeof inW.funcCode);
    rnd_fill(&inW.funcCodeRead, sizeof inW.funcCodeRead);
    inW.rwModeTmp = (Uint16)(rnd() % 16);

    inW.errorCode   = 0;                    /* see host-unrepresentable (3) */
    inW.errorEeprom = 0;

    inW.canLinkTicker = (Uint32)rnd();
    inW.commTicker    = (Uint32)rnd();
    rnd_fill(&inW.dspMainCmd1, sizeof inW.dspMainCmd1);
    rnd_fill(&inW.dspStatus,   sizeof inW.dspStatus);

    inW.frqAimTmp0    = (int32)rnd();
    inW.frqCalcSrcOld = (Uint16)(rnd() % 8);

    rnd_fill(inW.sendBuff, sizeof inW.sendBuff);
    inW.limitedNum = 0;

    inW.plcStep       = (Uint16)rnd();
    inW.plcStepRemOld = (Uint16)rnd();
    inW.plcTime       = (Uint32)rnd();
    inW.plcTimeRemOld = (Uint32)rnd();

    inW.pmsmRotorPos = (Uint16)(rnd() % 8);
    inW.scanStep     = (Uint16)(rnd() % (DISPLAY_8LED_NUM + 1));
    rnd_fill(&inW.spiData, sizeof inW.spiData);
    rnd_fill(&inW.displayBuffer, sizeof inW.displayBuffer);
    inW.upDownFrqInit = (Uint16)rnd();

    inW.timeStep = 0;
    inW.readRet  = FUNCCODE_READ_RET_OK;
}

static void case_begin(unsigned long seed)
{
    side_begin(seed);
    world_begin(seed ^ 0x5A5A5A5AUL);
}

/* ---- the EEPROM image ----------------------------------------------------
 *
 * ReadFuncCode() copies `number` words from gEeprom[index]. The driver controls
 * three positions, and it has to control them SEPARATELY -- that is the whole
 * point of the two selectors:
 *
 *   check_mode  what gEeprom holds at the two EEPROM_CHECK_INDEX1/2 positions,
 *               i.e. the two words InitFuncCode compares against
 *               EEPROM_CHECK1 / EEPROM_CHECK2:
 *                 0  neither matches  -> the `&&` is false, the `||` is false
 *                 1  both match       -> the `&&` is true,  the `||` is true
 *                 2  only word 1 matches -> `&&` false, `||` TRUE
 *                 3  only word 2 matches -> `&&` false, `||` TRUE
 *               Modes 2 and 3 exist because of a MEASURED bench hole: with
 *               only 0 and 1 available, `(EEPROM_CHECK1 == data[0]) &&
 *               (EEPROM_CHECK2 == data[1])` and the same expression with `||`
 *               are EQUIVALENT on every reachable input, so the `&& -> ||`
 *               mutation came out MISSED. That was not "the mutation is
 *               pointless" -- it was the bench feeding only the two rows of the
 *               truth table where the two operators agree.
 *
 *   aiao        1 puts AIAO_CHK_WORD into the AI_AO_CHK_FLAG position, which is
 *               the gate into the AI/AO factory-calibration read loop. Without
 *               it that loop is entered only when 16 random bits happen to
 *               equal 0xA5, so every mutation inside the loop was unreachable.
 *               This is the "turn a constant into an input" rule: AIAO_CHK_WORD
 *               is a literal in the module and has to become a driver input.
 *
 * Everything else in the image stays random, so the loop still sees varied
 * data[0] / data[1] (FM29/FM30 -- "write the wrong word" -- need them different).
 */
static void eeprom_begin(unsigned long seed, int check_mode, int aiao)
{
    Uint16 i;

    g_rng = seed ? seed : 1UL;
    for (i = 0; i < FMAIN_EEPROM_MAX; i++)
    {
        gEeprom[i] = (Uint16)(rnd() & 0xFFFFU);
    }

    /* `+1` cannot collide with the other check word: the two differ by more
     * than one (0x0009 / 0xFFF6). */
    switch (check_mode & 3)
    {
        case 0:
            gEeprom[GetEepromIndexFromFcIndex(EEPROM_CHECK_INDEX1)] =
                (Uint16)(EEPROM_CHECK1 ^ 0x1234U);
            gEeprom[GetEepromIndexFromFcIndex(EEPROM_CHECK_INDEX2)] = 0;
            break;

        case 1:
            gEeprom[GetEepromIndexFromFcIndex(EEPROM_CHECK_INDEX1)] =
                EEPROM_CHECK1;
            gEeprom[GetEepromIndexFromFcIndex(EEPROM_CHECK_INDEX2)] =
                EEPROM_CHECK2;
            break;

        case 2:
            gEeprom[GetEepromIndexFromFcIndex(EEPROM_CHECK_INDEX1)] =
                EEPROM_CHECK1;
            gEeprom[GetEepromIndexFromFcIndex(EEPROM_CHECK_INDEX2)] =
                (Uint16)(EEPROM_CHECK2 + 1);
            break;

        default:
            gEeprom[GetEepromIndexFromFcIndex(EEPROM_CHECK_INDEX1)] =
                (Uint16)(EEPROM_CHECK1 + 1);
            gEeprom[GetEepromIndexFromFcIndex(EEPROM_CHECK_INDEX2)] =
                EEPROM_CHECK2;
            break;
    }

    if (aiao)
    {
        gEeprom[GetEepromIndexFromFcIndex(AI_AO_CHK_FLAG)] = AIAO_CHK_WORD;
    }
}

/* ---- 0. GetNumberDigit / 1 / 2 -------------------------------------------
 *
 * These are the only three functions here that take arguments and return a
 * value, so they are compared directly instead of through the world snapshot:
 * the output buffer belongs to the caller.
 *
 * GetNumberDigit1 is `/10` done as `(Uint32)number * 52429 >> 19`; the boundary
 * cases are where the shift truncates (x9 / x0 / x1 around each power of ten)
 * plus the top of the 16-bit range, where `number * 52429` is closest to
 * overflowing 32 bits.
 */
static void phase_number_digit(void)
{
    static const Uint16 nums[] = {
        0, 1, 2, 9, 10, 11, 99, 100, 101, 999, 1000, 1001,
        9999, 10000, 10001, 32767, 32768, 52429, 52430,
        65534, 65535
    };
    const int N = (int)(sizeof nums / sizeof nums[0]);
    Uint16 dr[5], dn[5];
    Uint16 rr, rn;
    int i, mode;

    g_cur = "GetNumberDigit";

    for (mode = 0; mode < 2; mode++)
    for (i = 0; i < N; i++)
    {
        memset(dr, 0xAA, sizeof dr);
        memset(dn, 0xAA, sizeof dn);
        rr = ref_GetNumberDigit(dr, nums[i], (Uint16)mode);
        rn = new_GetNumberDigit(dn, nums[i], (Uint16)mode);
        g_cases++;
        if (rr != rn)
        {
            report("GetNumberDigit:ret");
        }
        if (memcmp(dr, dn, sizeof dr) != 0)
        {
            report("GetNumberDigit:digit");
        }
    }

    for (i = 0; i < N; i++)
    {
        memset(dr, 0xAA, sizeof dr);
        memset(dn, 0xAA, sizeof dn);
        ref_GetNumberDigit1(dr, nums[i]);
        new_GetNumberDigit1(dn, nums[i]);
        g_cases++;
        if (memcmp(dr, dn, sizeof dr) != 0)
        {
            report("GetNumberDigit1");
        }

        memset(dr, 0xAA, sizeof dr);
        memset(dn, 0xAA, sizeof dn);
        ref_GetNumberDigit2(dr, nums[i]);
        new_GetNumberDigit2(dn, nums[i]);
        g_cases++;
        if (memcmp(dr, dn, sizeof dr) != 0)
        {
            report("GetNumberDigit2");
        }
    }

    /* Random, plus every value around a digit-boundary. */
    for (i = 0; i < 4000; i++)
    {
        Uint16 v = (Uint16)rnd();

        memset(dr, 0xAA, sizeof dr);
        memset(dn, 0xAA, sizeof dn);
        rr = ref_GetNumberDigit(dr, v, (Uint16)(i & 1));
        rn = new_GetNumberDigit(dn, v, (Uint16)(i & 1));
        g_cases++;
        if (rr != rn || memcmp(dr, dn, sizeof dr) != 0)
        {
            report("GetNumberDigit:random");
        }
    }
}

/* ---- 1. SoftVersionDeal --------------------------------------------------
 *
 * `funcSoftVersion < motorSoftVersion ? motorSoftVersion : funcSoftVersion`,
 * plus the two unconditional stores. The interesting inputs are the ones either
 * side of equality, and the initial values already in the three fields (which
 * the rewrite must overwrite unconditionally).
 */
static void phase_soft_version(void)
{
    static const Uint16 vers[6] = { 0, 1, 71, 72, 73, 65535 };
    int a, b, c;

    g_cur = g_fnName[13];

    for (a = 0; a < 6; a++)
    for (b = 0; b < 6; b++)
    for (c = 0; c < 2; c++)
    {
        case_begin(0x1000UL + (unsigned long)g_cases);
        funcCode.code.funcSoftVersion  = vers[a];
        funcCode.code.motorSoftVersion = vers[b];
        if (c)
        {
            funcCode.code.softVersion = (Uint16)rnd();
        }
        fresh(13);
    }
}

/* ---- 2. CodeRunMonitor ---------------------------------------------------
 *
 * A sequence, because of the function-scope static `ticker`: the body only runs
 * when `++ticker >= CODE_RUN_DISPLAY_UPDATE_TIME / FUNC_DEAL_PERIOD`, and that
 * threshold moves as CODE_RUN_DISPLAY_UPDATE_TIME is bumped by 2 and wrapped
 * back to 200 at 300.
 *
 * The initial CODE_RUN_DISPLAY_UPDATE_TIME is swept across the threshold and
 * across the 300 wrap, so both the `>= 300` test and the division are covered.
 */
static void phase_code_run_monitor(void)
{
    static const Uint16 crd[12] = {
        0, 1, 2, 99, 100, 101, 199, 200, 298, 299, 300, 301
    };
    int ci, i;

    g_cur = g_fnName[10];

    for (ci = 0; ci < 12; ci++)
    {
        case_begin(0x2000UL + (unsigned long)ci);

        inSide.crdut = crd[ci];
        rnd_fill(inSide.ticker05msFuncEa, sizeof inSide.ticker05msFuncEa);
        rnd_fill(inSide.ticker05msFuncIn, sizeof inSide.ticker05msFuncIn);

        /* Order matters: case_begin() re-randomises inSide, so install_side()
         * has to run AFTER the inputs are pinned. */
        install_side(0);
        install_side(1);

        for (i = 0; i < 400; i++)
        {
            step(10);
        }
    }
}

/* ---- 3. Main05msFunctionEachStart ---------------------------------------
 *
 * `waitPowerOnTicker` is a function-scope static that has to reach
 * WAIT_POWER_ON_TIME (10000). Once past it the inner test runs on EVERY call,
 * so the post-crossing logic can be swept freely; the crossing itself is
 * observable exactly once, at call 10000, and that is enough to separate
 * `>=` from `>`.
 */
static void phase_each_start(void)
{
    int i, po, ds;

    g_cur = g_fnName[4];

    /* The crossing: dspStatus == 0 so the `runEnable && uv` arm stays out of
     * the way and powerOnStatus is still POWER_ON_WAIT at call 10000. */
    case_begin(0x3000UL);
    inW.dspStatus.all = 0;
    inSide.powerOnStatus = POWER_ON_WAIT;
    install_side(0);
    install_side(1);
    for (i = 0; i < 10005; i++)
    {
        step(4);
    }

    /* Past the crossing: every call now evaluates
     * `if (POWER_ON_WAIT == powerOnStatus) powerOnStatus = POWER_ON_FUNC_WAIT_OT;`
     * so the three states x the dspStatus arm are all reachable. */
    for (po = 0; po < 3; po++)
    for (ds = 0; ds < 4; ds++)
    {
        case_begin(0x3100UL + (unsigned long)(po * 4 + ds));
        inSide.powerOnStatus = (enum POWER_ON_STATUS)po;
        inW.dspStatus.all = 0;
        inW.dspStatus.bit.runEnable = (Uint16)(ds & 1);
        inW.dspStatus.bit.uv        = (Uint16)((ds >> 1) & 1);
        fresh(4);
    }
}

/* ---- 4. Main05msFunctionEachEnd -----------------------------------------
 *
 * The static `ticker` walks 0..3, and the two stores are
 * `timeBase - GetTime()` scaled by 3413 >> 11. timeBase and timeBase05msOld are
 * swept around the GetTime() value the stubs leave behind, so the subtraction
 * is exercised on both sides of zero (it is Uint32, so "negative" wraps).
 */
static void phase_each_end(void)
{
    static const Uint32 bases[6] = { 0, 1, 60000, 120000, 4294967295UL,
                                     2147483648UL };
    int bi, i;

    g_cur = g_fnName[5];

    for (bi = 0; bi < 6; bi++)
    {
        case_begin(0x4000UL + (unsigned long)bi);
        inSide.timeBase        = bases[bi];
        inSide.timeBase05msOld = bases[5 - bi];
        inW.timeStep           = (Uint32)(bi * 60000UL);
        install_side(0);
        install_side(1);

        for (i = 0; i < 12; i++)
        {
            step(5);
        }
    }
}

/* ---- 5. Main05msFunctionA / B / C / D ------------------------------------
 *
 * Pure sequencers: the evidence is the call log plus the module-owned state they
 * touch. The inputs that steer them are the guards inside the callees (stubbed
 * away) and the shared globals they hand over -- so the sweep is over funcCode
 * fields read by InitFuncCode-adjacent code, dspStatus, errorCode, scanStep and
 * the EALLOC/EDIS counters.
 */
static void phase_main_5ms_dispatch(void)
{
    int which, i, a, b, c, d;

    for (which = 6; which <= 9; which++)
    {
        g_cur = g_fnName[which];

        for (a = 0; a < 2; a++)
        for (b = 0; b < 2; b++)
        for (c = 0; c < 2; c++)
        for (d = 0; d < 2; d++)
        {
            case_begin(0x5000UL + (unsigned long)(which * 100 + g_cases % 97));

            inW.dspStatus.all = 0;
            inW.dspStatus.bit.runEnable = (Uint16)a;
            inW.dspStatus.bit.uv        = (Uint16)b;
            inW.dspStatus.bit.run       = (Uint16)c;
            inW.errorCode   = (Uint16)d;
            inW.scanStep    = (Uint16)(rnd() % (DISPLAY_8LED_NUM + 1));
            inW.timeStep    = (Uint32)(rnd() % 3) * 60000UL;
            inSide.powerOnStatus = (enum POWER_ON_STATUS)(rnd() % 3);

            fresh(which);
        }

        for (i = 0; i < 300; i++)
        {
            case_begin(0x5100UL + (unsigned long)i);
            fresh(which);
        }
    }
}

/* ---- 6. InitDspFunc ------------------------------------------------------
 *
 * A straight line of register writes plus fifteen init stubs. The register
 * image is random, so every clock-enable bit is exercised both ways -- a rewrite
 * that drops a write, or writes a different bit, shows up in the SysCtrlRegs
 * snapshot.
 */
static void phase_init_dsp_func(void)
{
    int i;

    g_cur = g_fnName[1];

    for (i = 0; i < 64; i++)
    {
        case_begin(0x6000UL + (unsigned long)i);
        fresh(1);
    }

    /* Both arms of the DSP_2803X branch, with an all-zero and an all-ones
     * register image so "did it write at all" is unambiguous. */
    for (i = 0; i < 4; i++)
    {
        case_begin(0x6100UL + (unsigned long)i);
        memset(&inW.sysctrl, i & 1 ? 0x00 : 0xFF, sizeof inW.sysctrl);
        fresh(1);
    }
}

/* ---- 7. FuncVariableInit -------------------------------------------------
 *
 * Everything it does is a function of funcCode fields and motorFc. The sweeps
 * below are on the quantities the code actually branches on:
 *   frqXSrc / frqYSrc == FUNCCODE_frqXySrc_FC_P_OFF_REM   -> upDownFrqInit
 *   plcPowerOffRemMode % 10 == FUNCCODE_plcPowerOffRemMode_REM -> the plc block
 * and on the REM_P_OFF_MOTOR-element copy out of funcCode.code.remPOffMotorCtrl.
 */
static void phase_func_variable_init(void)
{
    static const Uint16 xysrc[4] = {
        FUNCCODE_frqXySrc_FC, FUNCCODE_frqXySrc_FC_P_OFF_REM,
        FUNCCODE_frqXySrc_AI1, FUNCCODE_frqXySrc_AI2
    };
    static const Uint16 plcmode[6] = { 0, 1, 10, 11, 20, 21 };
    int xs, ys, pm, c, i;

    g_cur = g_fnName[3];

    for (xs = 0; xs < 4; xs++)
    for (ys = 0; ys < 4; ys++)
    for (pm = 0; pm < 6; pm++)
    for (c = 0; c < 2; c++)
    {
        case_begin(0x7000UL + (unsigned long)g_cases);

        funcCode.code.frqXSrc = xysrc[xs];
        funcCode.code.frqYSrc = xysrc[ys];
        funcCode.code.upDownFrqRem = (Uint16)rnd();
        funcCode.code.plcPowerOffRemMode = plcmode[pm];
        funcCode.code.plcStepRem    = (Uint16)rnd();
        funcCode.code.plcTimeHighRem = (Uint16)rnd();
        funcCode.code.plcTimeLowRem  = (Uint16)rnd();
        funcCode.code.pmsmRotorPos  = (Uint16)rnd();
        funcCode.code.presetFrq     = (Uint16)rnd();
        funcCode.code.inverterGpType = (Uint16)(c ? rnd() : 0);
        funcCode.code.frqCalcSrc    = (Uint16)(rnd() % 8);
        funcCode.code.shortCheckMode = (Uint16)(rnd() % 2);
        /* motorFc is MODULE-OWNED (renamed ref_motorFc / new_motorFc), so it has
         * to be driven through the per-side input image, not the bare global. */
        inSide.motorFc.pgPara.elem.fvcPgSrc = (Uint16)rnd();

        for (i = 0; i < REM_P_OFF_MOTOR; i++)
        {
            funcCode.code.remPOffMotorCtrl[i] = (Uint16)rnd();
        }
        for (i = 0; i < 64; i++)
        {
            inW.sendBuff[i] = (int16)rnd();
        }

        fresh(3);
    }

    for (i = 0; i < 1500; i++)
    {
        case_begin(0x7100UL + (unsigned long)i);
        fresh(3);
    }
}

/* ---- 8. InitFuncCode -----------------------------------------------------
 *
 * The big one. Its state machine is driven entirely by the EEPROM handshake:
 *
 *   VERIFY_EEPROM   ReadFuncCode() -> check words match? OLD : NEW
 *   NEW_EEPROM      AI_AO_CHK_FLAG word == AIAO_CHK_WORD? START : END
 *   NEW_EEPROM/START  read pairs, write funcCode.all[], until readIndex passes
 *                     AI_AO_CALIB_STOP -> READ_OK, END
 *   .../END         funcCodeRwModeTmp = WRITE_ALL, EepromDeal() clears it,
 *                   status = OVER, three SaveOneFuncCode()
 *   OLD_EEPROM      funcCodeRwModeTmp = READ_ALL, EepromDeal() clears it,
 *                   status = OVER
 *
 * then, once status == OVER, the DEBUG_F_FC_INIT_OVER block sweeps all 909
 * attribute entries and clamps funcCode.all[] through LimitDeal().
 *
 * The `static Uint16 flag` inside the START arm advances 4 readIndex steps for
 * every 4 successful reads, so the phase is stepped as a short sequence per
 * case rather than sampled once.
 *
 * gTimeStep is swept over the *exact divisors of the 2 ms threshold* as well as
 * non-divisors. The 2 ms gate is `baseTime - GetTime() >= (TIME_1MS << 1)` =
 * `>= 120000`, and the delta is `gTimeStep x (stub edges since baseTime)`. If a
 * loop body walks k stub edges, then gTimeStep == 120000 / k makes the delta land
 * EXACTLY on the threshold -- the only place `>=` and `>` disagree (FM43). With
 * only 60000 in the sweep, k == 2 would have to hold; it does not, which is why
 * FM43 was MISSED on the first pass.
 */
static void phase_init_func_code(void)
{
    static const Uint32 steps[12] = { 0, 1, 59999, 60000, 120000, 200000,
                                      40000, 30000, 24000, 20000, 15000, 12000 };
    static const Uint16 frqpts[5] = { 0, 1, 2, 3, 4 };
    int cm, ai, si, fi, pr, rep, i;

    g_cur = g_fnName[2];

    for (cm = 0; cm < 4; cm++)
    for (ai = 0; ai < 2; ai++)
    for (si = 0; si < 12; si++)
    for (fi = 0; fi < 5; fi++)
    for (pr = 0; pr < 3; pr++)
    for (rep = 0; rep < 3; rep++)
    {
        case_begin(0x8000UL + (unsigned long)g_cases);

        eeprom_begin(0x8000UL + (unsigned long)g_cases, cm, ai);

        funcCode.code.frqPoint = frqpts[fi];
        inW.timeStep = steps[si];
        inW.readRet  = FUNCCODE_READ_RET_OK;

        /* pr picks how funcCode.all[] is prefilled: 0 = from the attribute
         * table's own init values (so most entries are already in range and
         * few writes happen), 1 = random (so most are out of range and the
         * SaveOneFuncCode path runs hard), 2 = all ones. */
        if (pr == 0)
        {
            for (i = 0; i < FNUM_PARA; i++)
            {
                funcCode.all[i] = funcCodeAttribute[i].init;
            }
        }
        else if (pr == 1)
        {
            for (i = 0; i < FNUM_ALL; i++)
            {
                funcCode.all[i] = (Uint16)rnd();
            }
        }
        else
        {
            for (i = 0; i < FNUM_ALL; i++)
            {
                funcCode.all[i] = 0xFFFFU;
            }
        }

        inSide.funcCodeInitStatus =
            (enum FUNC_CODE_INIT_STATUS)rep;      /* 0 = start from VERIFY */
        inSide.aiaoChckReadOK = (Uint16)(rep & 1);

        install_side(0);
        install_side(1);

        /* A short lockstep sequence: the state machine plus the static `flag`. */
        for (i = 0; i < 8; i++)
        {
            step(2);
        }
    }

    /* Random, and the all-zero register/parameter image. */
    for (i = 0; i < 200; i++)
    {
        case_begin(0x8100UL + (unsigned long)i);
        eeprom_begin(0x8100UL + (unsigned long)i, (int)(i & 3),
                      (int)((i >> 2) & 1));
        inW.timeStep = steps[i % 12];
        fresh(2);
    }
}

/* ---- 9. InitForFunctionApp ----------------------------------------------
 *
 * The integration entry point: InitDspFunc + InitFuncCode + FuncVariableInit in
 * one call. Kept as its own phase because it is what the real firmware calls.
 */
static void phase_init_for_function_app(void)
{
    int i;

    g_cur = g_fnName[0];

    for (i = 0; i < 64; i++)
    {
        case_begin(0x9000UL + (unsigned long)i);
        eeprom_begin(0x9000UL + (unsigned long)i, (int)(i & 3),
                      (int)((i >> 2) & 1));
        inW.timeStep = (Uint32)(i % 3) * 60000UL;
        fresh(0);
    }
}

/* ---- 10. getMomeryValue -------------------------------------------------
 *
 * Reads the 16-bit word at funcCode.code.memoryAddr out of the host-side
 * gHostMemory table (see host_patch.json). memoryAddr is swept over both ends of
 * the 16-bit range plus the values that index the ends of the table.
 */
static void phase_get_momery_value(void)
{
    static const Uint16 addrs[8] = { 0, 1, 0x00FF, 0x0100, 0x7FFF, 0xFFFE,
                                     0xFFFF, 0x1234 };
    int a, i;

    g_cur = g_fnName[14];

    for (a = 0; a < 8; a++)
    {
        case_begin(0xA000UL + (unsigned long)a);
        funcCode.code.memoryAddr = addrs[a];
        inSide.memoryValue = (Uint16)rnd();
        fresh(14);
    }

    for (i = 0; i < 256; i++)
    {
        case_begin(0xA100UL + (unsigned long)i);
        funcCode.code.memoryAddr = (Uint16)rnd();
        fresh(14);
    }
}

/* ---- 11. Main0msFunction / Main2msFunction ------------------------------
 *
 * Main2msFunction is empty; Main0msFunction is one call. Both are kept so that
 * "the rewrite emptied a function that used to do something" is caught.
 */
static void phase_trivial(void)
{
    int i;

    g_cur = g_fnName[12];
    for (i = 0; i < 8; i++)
    {
        case_begin(0xB000UL + (unsigned long)i);
        fresh(12);
    }

    g_cur = g_fnName[11];
    for (i = 0; i < 8; i++)
    {
        case_begin(0xB100UL + (unsigned long)i);
        fresh(11);
    }
}

/* ---- random sweep over everything --------------------------------------- */
static void phase_random(long n)
{
    long i;

    for (i = 0; i < n; i++)
    {
        case_begin(0xC0000000UL + (unsigned long)i);
        eeprom_begin(0xC0000000UL + (unsigned long)i, (int)(i & 3),
                      (int)((i >> 2) & 1));
        fresh((int)(i % FN_N));
    }
}

/* ---- main ---------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long nrand = 20000;
    int i;

    if (argc > 1)
    {
        nrand = strtol(argv[1], NULL, 10);
        if (nrand < 0)
        {
            nrand = 0;
        }
    }

    /* GetTime() is CpuTimer1.RegsAddr->TIM.all; point it at the mock block. */
    CpuTimer1.RegsAddr = &CpuTimer1Regs;

    /* A fixed, non-trivial image for the address space getMomeryValue reads. */
    g_rng = 0x5EED0001UL;
    for (i = 0; i < HOST_MEMORY_WORDS; i++)
    {
        gHostMemory[i] = (Uint16)rnd();
    }

    init_ptrs();

    /* The AI/AO calibration window is a pair of COMPILE-TIME field offsets, so
     * the driver cannot move it. Printing it is the only way to tell whether
     * `readIndex > STOP` and `readIndex >= STOP` can ever disagree: readIndex
     * advances by 2 words per successful read (plus 4 once per 4th read), so
     * the two operators differ only if STOP - START is EVEN.
     *
     * BOTH spaces are printed, because they are NOT the same and the first
     * version of this print was misleading: AI_AO_CALIB_* are FcIndex (word
     * offsets into funcCode.all[]), while `readIndex` and the comparison at
     * line 274 live in EEPROM-index space via GetEepromIndexFromFcIndex(). The
     * stub mapping is identity so the two happen to agree here, but on the real
     * firmware the mapping is the `fcNoAttri2Eeprom` table -- which is exactly
     * why the parity has to be argued for both (see the FM34 exclusion note in
     * tools/difftest/negative_control.py). */
    printf("  AIAO calib window (FcIndex):    START=%u STOP=%u delta=%d\n",
           (unsigned)(AI_AO_CALIB_START), (unsigned)(AI_AO_CALIB_STOP),
           (int)(AI_AO_CALIB_STOP) - (int)(AI_AO_CALIB_START));
    printf("  AIAO calib window (EEPROM idx): START=%u STOP=%u delta=%d\n",
           (unsigned)GetEepromIndexFromFcIndex(AI_AO_CALIB_START),
           (unsigned)GetEepromIndexFromFcIndex(AI_AO_CALIB_STOP),
           (int)GetEepromIndexFromFcIndex(AI_AO_CALIB_STOP)
             - (int)GetEepromIndexFromFcIndex(AI_AO_CALIB_START));

    phase_number_digit();
    phase_soft_version();
    phase_code_run_monitor();
    phase_each_start();
    phase_each_end();
    phase_main_5ms_dispatch();
    phase_init_dsp_func();
    phase_func_variable_init();
    phase_init_func_code();
    phase_init_for_function_app();
    phase_get_momery_value();
    phase_trivial();
    phase_random(nrand);

    printf("f_main difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    printf("  max ticks in one call: %lu (budget %lu, headroom x%lu)\n",
           g_maxTicks, (unsigned long)MOCK_TICK_BUDGET,
           (unsigned long)MOCK_TICK_BUDGET / (g_maxTicks ? g_maxTicks : 1UL));
    return g_fails ? 1 : 0;
}
