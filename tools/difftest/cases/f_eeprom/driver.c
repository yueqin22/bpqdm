/* Differential test driver for f_eeprom.

 * f_eeprom is the parameter-storage engine: it owns the 24LC32 EEPROM and moves
 * FUNCCODE words between the parameter image (funcCode.all[]) and the chip, one
 * I2C transaction at a time, without ever blocking the 10 kHz control loop.

 *   RwI2cBus            the register-level transaction starter. Three modes:
 *                       WRITE (fill I2CDXR with high/low addr + payload, fire
 *                       I2CMDR=0x6E20), READ-after-RESTART (I2CMDR=0x6C20), and
 *                       the ACK/pointer-set case (I2CMDR=0x6620).
 *   I2cIntDeal          the ISR body. Reads I2CISRC, then advances the state
 *                       machine in I2cMsg.status: WRITE_BUSY -> ACK, SEND_NOSTOP
 *                       -> IDLE, READ_BUSY -> drain I2CDRR into I2cMsg.buffer[]
 *                       and go RW_OK; the ARDY branch handles NACK by setting
 *                       STP and clearing the NACK flag.
 *   RwEeprom            one step of the message-level machine driven by
 *                       I2cMsg.status: IDLE -> start, RESTART -> go read,
 *                       RW_OK -> done (returns 1) and back to IDLE.
 *   ReadFuncCode        a 3-attempt verify loop. First read stores and returns
 *                       READING; subsequent reads compare and only after
 *                       readRightTime consecutive agreements (2 after a write,
 *                       3 otherwise) does it return OK.
 *   WriteFuncCode       a 3-state machine (WRITE / WRITE_REMAINDER /
 *                       READ_TO_VERIFY_WRITE). A full page is written in one
 *                       shot; a 2-word-odd payload needs a second single-byte
 *                       transaction; then it reads back and verifies.
 *   EepromDeal          the scheduler. Picks a mode from funcCodeRwModeTmp /
 *                       funcCodeOneWriteNum, walks index4EepromDeal up to
 *                       endIndexRwFuncCode a page at a time, and per fc index
 *                       does one of: restore company defaults, restore all
 *                       defaults, copy EEPROM->RAM, copy RAM->EEPROM (with the
 *                       AI/AO calibration and user-para exceptions), or nothing.
 *   UpdateIndexForEepromDeal  advance index, finish the mode, and write the
 *                       user-para save flags on the SAVE_USER_PARA path.
 *   EepromOperateTimeDeal     three timeouts: read, write, whole-transaction.
 *   InitI2CGpio / InitSetI2ca / I2cDealBeforeInit / I2CNoACK / I2CStop /
 *   I2CRcvByte                setup + the bit-banged recovery sequence.
 *   GetEepromIndexFromFcIndex / GetFcIndexFromEepromIndex   the two lookup
 *                       directions, over funcCodeAttribute[] / fcNoAttri2Eeprom[].

 * ---- the two-worlds split -------------------------------------------------

 * The module owns 12 renamed objects (I2cMsg, funcCodeRead, funcCodeWrite,
 * funcCodeOneWriteIndex[], funcCodeRwMode, funcCodeRwModeTmp,
 * funcCodeRwStatus, funcCodeOneWriteNum, rwFuncCodeRepeatDelayTicker,
 * startIndexWriteSeries, endIndexWriteSeries, endIndexRwFuncCode,
 * eepromOperateTime). All have external linkage, so ref_/new_ each get a copy:
 * the driver installs both from one input image and snapshots both back.

 * Everything else is SHARED and must look identical to both sides on every call:

 *   funcCode, errorEeprom, powerOnStatus, aiaoChckReadOK, funcCodeAttribute[],
 *   fcNoAttri2Eeprom[], I2caRegs, GpioCtrlRegs, GpioDataRegs, and the three
 *   mock call-trace counters.

 * ... which is why the world is REINSTALLED before each side, not once: the
 * module writes a large part of it.

 * ---- what is compared -----------------------------------------------------

 *   per-side : 13 module-owned objects (all of I2cMsg, both FUNCCODE_RW, the
 *              index/counter words, eepromOperateTime)
 *   shared   : the whole WORLD image -- funcCode, errorEeprom, powerOnStatus,
 *              aiaoChckReadOK, every I2C register, every GPIO register (control
 *              and data), gHostCpuCtl[], the DELAY_US accumulation, and the
 *              mock call trace

 * ---- hardware side effects ARE the test -----------------------------------

 * This is the one module where "the registers are the output". A rewrite that
 * reorders two I2CDXR writes, drops an I2CCNT store, flips an STP bit, or
 * moves a DELAY_US changes nothing in RAM but breaks the chip. So the WORLD
 * image includes every register the module can touch and the snapshot is exact
 * (memcpy of the whole struct), not "the interesting fields". gHostCpuCtl[]
 * and the DELAY_US sum make the non-register side effects (EALLOW/EDIS pairs,
 * bit-bang timing) equally observable.

 * ---- statics: the reason most phases are SEQUENCES -------------------------

 * Five pieces of state are function-scope statics and cannot be installed:

 *   ReadFuncCode     readTime / readRightTime / bVerifyWrong
 *   WriteFuncCode    writeStatus / bVerifyWrong / number
 *   EepromDeal       (none -- all its latches are module globals)
 *   I2cIntDeal       (none)

 * `bVerifyWrong` in BOTH ReadFuncCode and WriteFuncCode is a separate static,
 * and it gates an early return (`if (bVerifyWrong && rwFuncCodeRepeatDelayTicker
 * < 100) return ...READING/WRITING;`) that is otherwise unreachable from a
 * cold start. So those two functions must be stepped in LOCKSTEP sequences and
 * compared after every single call -- a divergence then surfaces on the call
 * that caused it, not on some later step.

 * ---- host-unrepresentable / deliberately unreached regions -----------------

 *   (1) The real EEPROM does not exist. `RwI2cBus` only STARTS a transaction;
 *       `I2cIntDeal` is what completes it, and on hardware it is called from the
 *       I2C interrupt. The bench therefore drives I2cIntDeal BY HAND and feeds
 *       `I2caRegs.I2CISRC.all` / `I2caRegs.I2CSTR.bit.NACK` / `I2caRegs.I2CDRR`
 *       as inputs. That is faithful to the split -- the module itself never
 *       assumes the ISR runs -- but it means "does the ISR actually fire on the
 *       target" is NOT tested here.

 *   (2) `EepromDeal`'s `#if F_DEBUG_RAM` block (and the `#if (F_DEBUG_RAM &&
 *       DEBUG_F_EEPROM)` FUNCCODE_RW_STATUS_DEBUG arm) is dead: f_debug.h has
 *       F_DEBUG_RAM 0. The bench makes no claim about them. The live arms are
 *       DEBUG_F_EEPROM=1, DEBUG_F_TABLE_FC2EEPROM_CONST=1, PARA_INIT_MODE_SAVE_
 *       ALL_CODE=1, EEPROM_TYPE=EEPROM_24LC32, DSP_2803X=1.

 *   (3) `GetFcIndexFromEepromIndex` has two arms under
 *       DEBUG_F_TABLE_FC2EEPROM_CONST; the live one is the LINEAR SCAN
 *       (`#elif 1`), not the `eeprom2Fc[a]` table read. So `eeprom2Fc[]` is
 *       dead data in this build -- which is why the bench does not provide it
 *       and only exercises the scan. Both sides see the same dead arm.

 *   (4) `DELAY_US` magnitude. On the target it burns real microseconds; here it
 *       only accumulates a number. The bench asserts the NUMBER, which is what
 *       a rewrite can plausibly get wrong (delete a call, change an argument).
 *       It does not assert elapsed time.

 *   (5) `RwEeprom`'s retry loops are bounded at 100 tries by construction. The
 *       bench never feeds a permanently-failing bus, because a variant that
 *       removed the bound would then hang rather than differ. Termination
 *       changes are reported as such by run_case.py (rc=-2), not as MISSED.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DSP28x_Project.h"
#include "mock_eeprom.h"

#define FAIL_LIMIT 20

/* 函数码读写状态机的四个状态。这组常量定义在 src/function/f_eeprom.c 里
 * (模块私有, 没进 f_eeprom.h), 所以宿主编不过去只能在本文件照抄一份。
 * 值与模块逐一对应: PREPARE_DATA_FOR_EAD=0 / READ_CURRENT_DATA=1 /
 * PREPARE_DATA_FOR_WRITE=2 / WRITE_DATA=3, FUNC_CODE_RW_STATUS_INIT 就是 0。
 * driver 要装 inSide.funcCodeRwStatus, 没有这组名字就只能写魔数。 */
#define DRV_RW_STATUS_PREPARE_DATA_FOR_EAD  0
#define DRV_RW_STATUS_READ_CURRENT_DATA     1
#define DRV_RW_STATUS_PREPARE_DATA_FOR_WRITE 2
#define DRV_RW_STATUS_WRITE_DATA            3

/* ---- function index ------------------------------------------------------ */

#define FN_RWI2C    0   /* RwI2cBus            */
#define FN_INT      1   /* I2cIntDeal          */
#define FN_RWEE     2   /* RwEeprom            */
#define FN_READ     3   /* ReadFuncCode        */
#define FN_WRITE    4   /* WriteFuncCode       */
#define FN_DEAL     5   /* EepromDeal          */
#define FN_TIME     6   /* EepromOperateTimeDeal */
#define FN_INITGPIO 7   /* InitI2CGpio         */
#define FN_INITSET  8   /* InitSetI2ca         */
#define FN_BEFORE   9   /* I2cDealBeforeInit   */
#define FN_NOACK   10   /* I2CNoACK            */
#define FN_STOP    11   /* I2CStop             */
#define FN_RCV     12   /* I2CRcvByte          */
#define FN_GEFI    13   /* GetEepromIndexFromFcIndex */
#define FN_GFEI    14   /* GetFcIndexFromEepromIndex */
#define FN_UPDIDX  15   /* UpdateIndexForEepromDeal */
#define FN_N       16

/* ---- ref side ------------------------------------------------------------ */

extern Uint16   ref_RwI2cBus(Uint16 mode);
extern void     ref_I2cIntDeal(void);
extern Uint16   ref_RwEeprom(Uint16 mode);
extern Uint16   ref_ReadFuncCode(FUNCCODE_RW *p);
extern Uint16   ref_WriteFuncCode(FUNCCODE_RW *p);
extern void     ref_EepromDeal(void);
extern void     ref_EepromOperateTimeDeal(void);
extern void     ref_InitI2CGpio(void);
extern void     ref_InitSetI2ca(void);
extern void     ref_I2cDealBeforeInit(void);
extern void     ref_I2CNoACK(void);
extern void     ref_I2CStop(void);
extern void     ref_I2CRcvByte(void);
extern Uint16   ref_GetEepromIndexFromFcIndex(Uint16 index);
extern Uint16   ref_GetFcIndexFromEepromIndex(Uint16 a);
extern void     ref_UpdateIndexForEepromDeal(void);

extern Uint16 ref_funcCodeOneWriteNum;
extern Uint16 ref_funcCodeOneWriteIndex[];
extern Uint16 ref_funcCodeRwMode;
extern Uint16 ref_funcCodeRwModeTmp;
extern Uint16 ref_funcCodeRwStatus;
extern struct I2C_MSG ref_I2cMsg;
extern Uint16 ref_startIndexWriteSeries;
extern Uint16 ref_endIndexWriteSeries;
extern Uint16 ref_endIndexRwFuncCode;
extern FUNCCODE_RW ref_funcCodeRead;
extern FUNCCODE_RW ref_funcCodeWrite;
extern Uint16 ref_rwFuncCodeRepeatDelayTicker;
extern struct EEPROM_OPERATE_TIME ref_eepromOperateTime;

/* ---- new side ------------------------------------------------------------ */

extern Uint16   new_RwI2cBus(Uint16 mode);
extern void     new_I2cIntDeal(void);
extern Uint16   new_RwEeprom(Uint16 mode);
extern Uint16   new_ReadFuncCode(FUNCCODE_RW *p);
extern Uint16   new_WriteFuncCode(FUNCCODE_RW *p);
extern void     new_EepromDeal(void);
extern void     new_EepromOperateTimeDeal(void);
extern void     new_InitI2CGpio(void);
extern void     new_InitSetI2ca(void);
extern void     new_I2cDealBeforeInit(void);
extern void     new_I2CNoACK(void);
extern void     new_I2CStop(void);
extern void     new_I2CRcvByte(void);
extern Uint16   new_GetEepromIndexFromFcIndex(Uint16 index);
extern Uint16   new_GetFcIndexFromEepromIndex(Uint16 a);
extern void     new_UpdateIndexForEepromDeal(void);

extern Uint16 new_funcCodeOneWriteNum;
extern Uint16 new_funcCodeOneWriteIndex[];
extern Uint16 new_funcCodeRwMode;
extern Uint16 new_funcCodeRwModeTmp;
extern Uint16 new_funcCodeRwStatus;
extern struct I2C_MSG new_I2cMsg;
extern Uint16 new_startIndexWriteSeries;
extern Uint16 new_endIndexWriteSeries;
extern Uint16 new_endIndexRwFuncCode;
extern FUNCCODE_RW new_funcCodeRead;
extern FUNCCODE_RW new_funcCodeWrite;
extern Uint16 new_rwFuncCodeRepeatDelayTicker;
extern struct EEPROM_OPERATE_TIME new_eepromOperateTime;

/* ---- per-side snapshot --------------------------------------------------- */

struct SIDE
{
    Uint16 funcCodeOneWriteNum;
    Uint16 funcCodeOneWriteIndex[200];      /* FUNCCODE_ONE_WRITE_NUMBER_MAX */
    Uint16 funcCodeRwMode;
    Uint16 funcCodeRwModeTmp;
    Uint16 funcCodeRwStatus;
    struct I2C_MSG I2cMsg;
    Uint16 startIndexWriteSeries;
    Uint16 endIndexWriteSeries;
    Uint16 endIndexRwFuncCode;
    FUNCCODE_RW funcCodeRead;
    FUNCCODE_RW funcCodeWrite;
    Uint16 rwFuncCodeRepeatDelayTicker;
    struct EEPROM_OPERATE_TIME eepromOperateTime;
};

static struct SIDE inSide;
static struct SIDE S[2];

#define SIDE_INSTALL(P)                                                       \
    do {                                                                      \
        P##funcCodeOneWriteNum       = inSide.funcCodeOneWriteNum;            \
        memcpy(P##funcCodeOneWriteIndex, inSide.funcCodeOneWriteIndex,        \
               sizeof inSide.funcCodeOneWriteIndex);                          \
        P##funcCodeRwMode            = inSide.funcCodeRwMode;                 \
        P##funcCodeRwModeTmp         = inSide.funcCodeRwModeTmp;              \
        /* funcCodeRwStatus 是 EepromDeal 那棵大 if-else 的**总开关**:         \
         * 整个 PREPARE_DATA_FOR_EAD 块 (页对齐夹取、RSVD 扫描、               \
         * UpdateIndexForEepromDeal 的调用点) 都在 `if (PREPARE_DATA_FOR_EAD  \
         * == funcCodeRwStatus)` 里面。原来这里**漏了它** —— driver 里明明声明 \
         * 了 inSide.funcCodeRwStatus、side_begin() 也每例随机填它, 但从来没   \
         * 装进模块, 于是:                                                     \
         *   (1) 模块里的状态从程序启动起只由模块自己推进, 跨用例/跨相位残留;    \
         *   (2) SIDE_SNAPSHOT 也没抄它, 而 driver 又比它 —— 两边比的是同一个   \
         *       静态缓冲里同一份永不更新的 0, 这条比较**恒真**, 是空转;        \
         *   (3) 各 deal 相位注释里写的"直接设成 0 (PREPARE)"全都落了空,        \
         *       EAD 块实际命中率只剩"模块状态正好走到 0"的概率。                \
         * build/_probe_ep_v5.py 实测 20000 例 EAD 块只进 33 次,               \
         * phase_deal_allrsvd 的 WRITE_SERIES 一次都没进,                      \
         * EP102/EP108/EP109/EP111/EP115/EP116/EP130 因此全 MISSED。           \
         * 装上之后: 状态是每序列一次的**输入**, 相位里钉 0 才真的起作用,        \
         * 而 snapshot 侧的比较也第一次变成真比较。 */                           \
        P##funcCodeRwStatus          = inSide.funcCodeRwStatus;               \
        memcpy(&P##I2cMsg, &inSide.I2cMsg, sizeof inSide.I2cMsg);             \
        P##startIndexWriteSeries     = inSide.startIndexWriteSeries;          \
        P##endIndexWriteSeries       = inSide.endIndexWriteSeries;            \
        P##endIndexRwFuncCode        = inSide.endIndexRwFuncCode;             \
        memcpy(&P##funcCodeRead, &inSide.funcCodeRead, sizeof inSide.funcCodeRead); \
        memcpy(&P##funcCodeWrite, &inSide.funcCodeWrite, sizeof inSide.funcCodeWrite); \
        P##rwFuncCodeRepeatDelayTicker = inSide.rwFuncCodeRepeatDelayTicker;  \
        memcpy(&P##eepromOperateTime, &inSide.eepromOperateTime,              \
               sizeof inSide.eepromOperateTime);                              \
    } while (0)

#define SIDE_SNAPSHOT(P, s)                                                   \
    do {                                                                      \
        (s)->funcCodeOneWriteNum       = P##funcCodeOneWriteNum;              \
        memcpy((s)->funcCodeOneWriteIndex, P##funcCodeOneWriteIndex,          \
               sizeof (s)->funcCodeOneWriteIndex);                            \
        (s)->funcCodeRwMode            = P##funcCodeRwMode;                   \
        (s)->funcCodeRwModeTmp         = P##funcCodeRwModeTmp;                \
        /* 与 SIDE_INSTALL 成对: 见上面关于 funcCodeRwStatus 的说明。少了这一条, \
         * cmp("funcCodeRwStatus", ...) 比的是静态缓冲里那份永不更新的 0。 */    \
        (s)->funcCodeRwStatus          = P##funcCodeRwStatus;                 \
        memcpy(&(s)->I2cMsg, &P##I2cMsg, sizeof (s)->I2cMsg);                 \
        (s)->startIndexWriteSeries     = P##startIndexWriteSeries;            \
        (s)->endIndexWriteSeries       = P##endIndexWriteSeries;              \
        (s)->endIndexRwFuncCode        = P##endIndexRwFuncCode;               \
        memcpy(&(s)->funcCodeRead, &P##funcCodeRead, sizeof (s)->funcCodeRead); \
        memcpy(&(s)->funcCodeWrite, &P##funcCodeWrite, sizeof (s)->funcCodeWrite); \
        (s)->rwFuncCodeRepeatDelayTicker = P##rwFuncCodeRepeatDelayTicker;    \
        memcpy(&(s)->eepromOperateTime, &P##eepromOperateTime,                \
               sizeof (s)->eepromOperateTime);                                \
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
    FUNCCODE_ALL funcCode;

    Uint16 errorEeprom;
    Uint16 powerOnStatus;
    Uint16 aiaoChckReadOK;

    /* 外设: 整结构体快照, 不是"挑几个字段"。见文件头 "hardware side effects
     * ARE the test"。 */
    struct I2C_REGS         i2c;
    struct GPIO_CTRL_REGS   gpioCtrl;
    struct GPIO_DATA_REGS   gpioData;

    Uint16 cpuCtl[7];

    Uint32 delaySum;
    Uint32 delayCalls;

    Uint16 pinExtLevel32;

    /* mock 调用轨迹 */
    Uint16 gfciCalls;
    Uint16 gfciLastIndex;
    Uint16 gfciLastType;
    Uint32 gfciArgSum;
    Uint16 gfcioCalls;
    Uint16 gfcioLastIndex;
    Uint16 rcpoCalls;
};

static struct WORLD inW;
static struct WORLD SH[2];

static void install_world(void)
{
    memcpy(&funcCode, &inW.funcCode, sizeof inW.funcCode);

    errorEeprom     = inW.errorEeprom;
    powerOnStatus   = (enum POWER_ON_STATUS)inW.powerOnStatus;
    aiaoChckReadOK  = inW.aiaoChckReadOK;

    memcpy((void *)&I2caRegs,       &inW.i2c,      sizeof inW.i2c);
    memcpy((void *)&GpioCtrlRegs,   &inW.gpioCtrl, sizeof inW.gpioCtrl);
    memcpy((void *)&GpioDataRegs,   &inW.gpioData, sizeof inW.gpioData);

    memcpy((void *)gHostCpuCtl, inW.cpuCtl, sizeof inW.cpuCtl);

    gHostDelaySum   = inW.delaySum;
    gHostDelayCalls = inW.delayCalls;

    gHostPinExtLevel32 = inW.pinExtLevel32;

    gMockGfciCalls     = inW.gfciCalls;
    gMockGfciLastIndex = inW.gfciLastIndex;
    gMockGfciLastType  = inW.gfciLastType;
    gMockGfciArgSum    = inW.gfciArgSum;
    gMockGfcioCalls    = inW.gfcioCalls;
    gMockGfcioLastIndex = inW.gfcioLastIndex;
    gMockRcpoCalls     = inW.rcpoCalls;
}

static void snap_world(int side)
{
    memcpy(&SH[side].funcCode, &funcCode, sizeof funcCode);

    SH[side].errorEeprom    = errorEeprom;
    SH[side].powerOnStatus  = (Uint16)powerOnStatus;
    SH[side].aiaoChckReadOK = aiaoChckReadOK;

    memcpy(&SH[side].i2c,      (const void *)&I2caRegs,     sizeof SH[side].i2c);
    memcpy(&SH[side].gpioCtrl, (const void *)&GpioCtrlRegs, sizeof SH[side].gpioCtrl);
    memcpy(&SH[side].gpioData, (const void *)&GpioDataRegs, sizeof SH[side].gpioData);

    memcpy(SH[side].cpuCtl, (const void *)gHostCpuCtl, sizeof SH[side].cpuCtl);

    SH[side].delaySum   = gHostDelaySum;
    SH[side].delayCalls = gHostDelayCalls;

    SH[side].pinExtLevel32 = gHostPinExtLevel32;

    SH[side].gfciCalls      = gMockGfciCalls;
    SH[side].gfciLastIndex  = gMockGfciLastIndex;
    SH[side].gfciLastType   = gMockGfciLastType;
    SH[side].gfciArgSum     = gMockGfciArgSum;
    SH[side].gfcioCalls     = gMockGfcioCalls;
    SH[side].gfcioLastIndex = gMockGfcioLastIndex;
    SH[side].rcpoCalls      = gMockRcpoCalls;
}

/* ---- reporting ----------------------------------------------------------- */

static long g_cases = 0;
static int  g_fails = 0;
static const char *g_cur = "";

/* 两侧函数返回值。它们不能进 SIDE (返回值不是模块状态), 但要参与比较, 而且
 * 差异报告里要看得见 —— 所以单独存。 */
static Uint16 g_ret[2];
static Uint16 g_retSet[2];

static Uint16 g_argIndex;   /* forward: report()'s gfei diagnostic reads it */

static void report(const char *what)
{
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s: %s\n", g_cur, what);
    printf("  ret %s %u/%u  rwMode %u/%u rwModeTmp %u/%u oneNum %u/%u\n",
           g_retSet[0] || g_retSet[1] ? "set" : "n/a",
           (unsigned)g_ret[0], (unsigned)g_ret[1],
           (unsigned)S[0].funcCodeRwMode, (unsigned)S[1].funcCodeRwMode,
           (unsigned)S[0].funcCodeRwModeTmp, (unsigned)S[1].funcCodeRwModeTmp,
           (unsigned)S[0].funcCodeOneWriteNum, (unsigned)S[1].funcCodeOneWriteNum);
    printf("  idx4 %u/%u num4 %u/%u endIdx %u/%u startSer %u/%u endSer %u/%u\n",
           (unsigned)S[0].funcCodeRead.index, (unsigned)S[1].funcCodeRead.index,
           (unsigned)S[0].funcCodeRead.number, (unsigned)S[1].funcCodeRead.number,
           (unsigned)S[0].endIndexRwFuncCode, (unsigned)S[1].endIndexRwFuncCode,
           (unsigned)S[0].startIndexWriteSeries, (unsigned)S[1].startIndexWriteSeries,
           (unsigned)S[0].endIndexWriteSeries, (unsigned)S[1].endIndexWriteSeries);
    printf("  I2cMsg.status %d/%d bytes %u/%u hi %u/%u lo %u/%u buf %u,%u,%u,%u / %u,%u,%u,%u\n",
           (int)S[0].I2cMsg.status, (int)S[1].I2cMsg.status,
           (unsigned)S[0].I2cMsg.bytes, (unsigned)S[1].I2cMsg.bytes,
           (unsigned)S[0].I2cMsg.highAddr, (unsigned)S[1].I2cMsg.highAddr,
           (unsigned)S[0].I2cMsg.lowAddr, (unsigned)S[1].I2cMsg.lowAddr,
           (unsigned)S[0].I2cMsg.buffer[0], (unsigned)S[0].I2cMsg.buffer[1],
           (unsigned)S[0].I2cMsg.buffer[2], (unsigned)S[0].I2cMsg.buffer[3],
           (unsigned)S[1].I2cMsg.buffer[0], (unsigned)S[1].I2cMsg.buffer[1],
           (unsigned)S[1].I2cMsg.buffer[2], (unsigned)S[1].I2cMsg.buffer[3]);
    printf("  I2CMDR %04X/%04X I2CCNT %u/%u I2CSAR %u/%u I2CDXR %u/%u "
           "I2CSTR %04X/%04X I2CISRC %u/%u\n",
           (unsigned)SH[0].i2c.I2CMDR.all, (unsigned)SH[1].i2c.I2CMDR.all,
           (unsigned)SH[0].i2c.I2CCNT, (unsigned)SH[1].i2c.I2CCNT,
           (unsigned)SH[0].i2c.I2CSAR, (unsigned)SH[1].i2c.I2CSAR,
           (unsigned)SH[0].i2c.I2CDXR, (unsigned)SH[1].i2c.I2CDXR,
           (unsigned)SH[0].i2c.I2CSTR.all, (unsigned)SH[1].i2c.I2CSTR.all,
           (unsigned)SH[0].i2c.I2CISRC.all, (unsigned)SH[1].i2c.I2CISRC.all);
    printf("  ctl EALLOW/EDIS %u,%u / %u,%u  delay calls %lu/%lu sum %lu/%lu\n",
           (unsigned)SH[0].cpuCtl[4], (unsigned)SH[0].cpuCtl[5],
           (unsigned)SH[1].cpuCtl[4], (unsigned)SH[1].cpuCtl[5],
           (unsigned long)SH[0].delayCalls, (unsigned long)SH[1].delayCalls,
           (unsigned long)SH[0].delaySum, (unsigned long)SH[1].delaySum);
    printf("  mock gfci %u/%u (last %u,%u / %u,%u) gfcio %u/%u rcpo %u/%u\n",
           (unsigned)SH[0].gfciCalls, (unsigned)SH[1].gfciCalls,
           (unsigned)SH[0].gfciLastIndex, (unsigned)SH[0].gfciLastType,
           (unsigned)SH[1].gfciLastIndex, (unsigned)SH[1].gfciLastType,
           (unsigned)SH[0].gfcioCalls, (unsigned)SH[1].gfcioCalls,
           (unsigned)SH[0].rcpoCalls, (unsigned)SH[1].rcpoCalls);
    printf("  errorEeprom %u/%u powerOn %u/%u aiaoChk %u/%u rwTicker %u/%u\n",
           (unsigned)SH[0].errorEeprom, (unsigned)SH[1].errorEeprom,
           (unsigned)SH[0].powerOnStatus, (unsigned)SH[1].powerOnStatus,
           (unsigned)SH[0].aiaoChckReadOK, (unsigned)SH[1].aiaoChckReadOK,
           (unsigned)S[0].rwFuncCodeRepeatDelayTicker,
           (unsigned)S[1].rwFuncCodeRepeatDelayTicker);
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
    /* ---- 返回值 ---- */
    if (g_retSet[0] || g_retSet[1])
    {
        cmp("ret", &g_ret[0], &g_ret[1], sizeof g_ret[0]);
    }

    /* ---- per-side module-owned objects ---- */
    cmp("funcCodeOneWriteNum", &S[0].funcCodeOneWriteNum, &S[1].funcCodeOneWriteNum,
        sizeof S[0].funcCodeOneWriteNum);
    cmp("funcCodeOneWriteIndex", S[0].funcCodeOneWriteIndex,
        S[1].funcCodeOneWriteIndex, sizeof S[0].funcCodeOneWriteIndex);
    cmp("funcCodeRwMode", &S[0].funcCodeRwMode, &S[1].funcCodeRwMode,
        sizeof S[0].funcCodeRwMode);
    cmp("funcCodeRwModeTmp", &S[0].funcCodeRwModeTmp, &S[1].funcCodeRwModeTmp,
        sizeof S[0].funcCodeRwModeTmp);
    cmp("funcCodeRwStatus", &S[0].funcCodeRwStatus, &S[1].funcCodeRwStatus,
        sizeof S[0].funcCodeRwStatus);
    cmp("I2cMsg", &S[0].I2cMsg, &S[1].I2cMsg, sizeof S[0].I2cMsg);
    cmp("startIndexWriteSeries", &S[0].startIndexWriteSeries,
        &S[1].startIndexWriteSeries, sizeof S[0].startIndexWriteSeries);
    cmp("endIndexWriteSeries", &S[0].endIndexWriteSeries,
        &S[1].endIndexWriteSeries, sizeof S[0].endIndexWriteSeries);
    cmp("endIndexRwFuncCode", &S[0].endIndexRwFuncCode, &S[1].endIndexRwFuncCode,
        sizeof S[0].endIndexRwFuncCode);
    cmp("funcCodeRead", &S[0].funcCodeRead, &S[1].funcCodeRead,
        sizeof S[0].funcCodeRead);
    cmp("funcCodeWrite", &S[0].funcCodeWrite, &S[1].funcCodeWrite,
        sizeof S[0].funcCodeWrite);
    cmp("rwFuncCodeRepeatDelayTicker", &S[0].rwFuncCodeRepeatDelayTicker,
        &S[1].rwFuncCodeRepeatDelayTicker, sizeof S[0].rwFuncCodeRepeatDelayTicker);
    cmp("eepromOperateTime", &S[0].eepromOperateTime, &S[1].eepromOperateTime,
        sizeof S[0].eepromOperateTime);

    /* ---- shared state the module writes (incl. hardware) ---- */
    cmp("funcCode", &SH[0].funcCode, &SH[1].funcCode, sizeof SH[0].funcCode);
    cmp("errorEeprom", &SH[0].errorEeprom, &SH[1].errorEeprom,
        sizeof SH[0].errorEeprom);
    cmp("powerOnStatus", &SH[0].powerOnStatus, &SH[1].powerOnStatus,
        sizeof SH[0].powerOnStatus);
    cmp("aiaoChckReadOK", &SH[0].aiaoChckReadOK, &SH[1].aiaoChckReadOK,
        sizeof SH[0].aiaoChckReadOK);

    cmp("I2caRegs", &SH[0].i2c, &SH[1].i2c, sizeof SH[0].i2c);
    cmp("GpioCtrlRegs", &SH[0].gpioCtrl, &SH[1].gpioCtrl, sizeof SH[0].gpioCtrl);
    cmp("GpioDataRegs", &SH[0].gpioData, &SH[1].gpioData, sizeof SH[0].gpioData);

    cmp("cpuCtl", SH[0].cpuCtl, SH[1].cpuCtl, sizeof SH[0].cpuCtl);
    cmp("delaySum", &SH[0].delaySum, &SH[1].delaySum, sizeof SH[0].delaySum);
    cmp("delayCalls", &SH[0].delayCalls, &SH[1].delayCalls, sizeof SH[0].delayCalls);
    cmp("pinExtLevel32", &SH[0].pinExtLevel32, &SH[1].pinExtLevel32,
        sizeof SH[0].pinExtLevel32);

    cmp("mockGfci", &SH[0].gfciCalls, &SH[1].gfciCalls, sizeof SH[0].gfciCalls);
    cmp("mockGfciLast", &SH[0].gfciLastIndex, &SH[1].gfciLastIndex,
        sizeof SH[0].gfciLastIndex);
    cmp("mockGfciType", &SH[0].gfciLastType, &SH[1].gfciLastType,
        sizeof SH[0].gfciLastType);
    cmp("mockGfciArgSum", &SH[0].gfciArgSum, &SH[1].gfciArgSum,
        sizeof SH[0].gfciArgSum);
    cmp("mockGfcio", &SH[0].gfcioCalls, &SH[1].gfcioCalls, sizeof SH[0].gfcioCalls);
    cmp("mockGfcioLast", &SH[0].gfcioLastIndex, &SH[1].gfcioLastIndex,
        sizeof SH[0].gfcioLastIndex);
    cmp("mockRcpo", &SH[0].rcpoCalls, &SH[1].rcpoCalls, sizeof SH[0].rcpoCalls);
}

/* ---- lockstep step ------------------------------------------------------- */

typedef void (*void_fn)(void);

static void_fn g_refFn[FN_N];
static void_fn g_newFn[FN_N];
static const char *g_fnName[FN_N] = {
    "RwI2cBus", "I2cIntDeal", "RwEeprom", "ReadFuncCode", "WriteFuncCode",
    "EepromDeal", "EepromOperateTimeDeal", "InitI2CGpio", "InitSetI2ca",
    "I2cDealBeforeInit", "I2CNoACK", "I2CStop", "I2CRcvByte",
    "GetEepromIndexFromFcIndex", "GetFcIndexFromEepromIndex",
    "UpdateIndexForEepromDeal"
};

/* 带参数的函数用全局量传递。 */
static Uint16 g_argMode;
static FUNCCODE_RW *g_argRw;      /* ref 侧指针 */
static FUNCCODE_RW *g_argRwNew;   /* new 侧指针 */
/* g_argIndex 已在 report() 之前前向声明 */

/* RwI2cBus 有返回值 (Uint16), 但基准里调用它的地方全部丢弃返回值; 仍然捕获,
 * 好让"改写把 return 改了"可见。 */
static void ref_call_rwi2c(void)     { g_ret[0] = ref_RwI2cBus(g_argMode); g_retSet[0] = 1; }
static void new_call_rwi2c(void)     { g_ret[1] = new_RwI2cBus(g_argMode); g_retSet[1] = 1; }
static void ref_call_rweep(void)     { g_ret[0] = ref_RwEeprom(g_argMode); g_retSet[0] = 1; }
static void new_call_rweep(void)     { g_ret[1] = new_RwEeprom(g_argMode); g_retSet[1] = 1; }
static void ref_call_read(void)      { g_ret[0] = ref_ReadFuncCode(g_argRw); g_retSet[0] = 1; }
static void new_call_read(void)      { g_ret[1] = new_ReadFuncCode(g_argRwNew); g_retSet[1] = 1; }
static void ref_call_write(void)     { g_ret[0] = ref_WriteFuncCode(g_argRw); g_retSet[0] = 1; }
static void new_call_write(void)     { g_ret[1] = new_WriteFuncCode(g_argRwNew); g_retSet[1] = 1; }
static void ref_call_gefi(void)      { g_ret[0] = ref_GetEepromIndexFromFcIndex(g_argIndex); g_retSet[0] = 1; }
static void new_call_gefi(void)      { g_ret[1] = new_GetEepromIndexFromFcIndex(g_argIndex); g_retSet[1] = 1; }
static void ref_call_gfei(void)      { g_ret[0] = ref_GetFcIndexFromEepromIndex(g_argIndex); g_retSet[0] = 1; }
static void new_call_gfei(void)      { g_ret[1] = new_GetFcIndexFromEepromIndex(g_argIndex); g_retSet[1] = 1; }

static void init_ptrs(void)
{
    g_refFn[FN_RWI2C]  = ref_call_rwi2c;
    g_refFn[FN_INT]    = ref_I2cIntDeal;
    g_refFn[FN_RWEE]   = ref_call_rweep;
    g_refFn[FN_READ]   = ref_call_read;
    g_refFn[FN_WRITE]  = ref_call_write;
    g_refFn[FN_DEAL]   = ref_EepromDeal;
    g_refFn[FN_TIME]   = ref_EepromOperateTimeDeal;
    g_refFn[FN_INITGPIO] = ref_InitI2CGpio;
    g_refFn[FN_INITSET]  = ref_InitSetI2ca;
    g_refFn[FN_BEFORE]   = ref_I2cDealBeforeInit;
    g_refFn[FN_NOACK]    = ref_I2CNoACK;
    g_refFn[FN_STOP]     = ref_I2CStop;
    g_refFn[FN_RCV]      = ref_I2CRcvByte;
    g_refFn[FN_GEFI]     = ref_call_gefi;
    g_refFn[FN_GFEI]     = ref_call_gfei;
    g_refFn[FN_UPDIDX]   = ref_UpdateIndexForEepromDeal;

    g_newFn[FN_RWI2C]  = new_call_rwi2c;
    g_newFn[FN_INT]    = new_I2cIntDeal;
    g_newFn[FN_RWEE]   = new_call_rweep;
    g_newFn[FN_READ]   = new_call_read;
    g_newFn[FN_WRITE]  = new_call_write;
    g_newFn[FN_DEAL]   = new_EepromDeal;
    g_newFn[FN_TIME]   = new_EepromOperateTimeDeal;
    g_newFn[FN_INITGPIO] = new_InitI2CGpio;
    g_newFn[FN_INITSET]  = new_InitSetI2ca;
    g_newFn[FN_BEFORE]   = new_I2cDealBeforeInit;
    g_newFn[FN_NOACK]    = new_I2CNoACK;
    g_newFn[FN_STOP]     = new_I2CStop;
    g_newFn[FN_RCV]      = new_I2CRcvByte;
    g_newFn[FN_GEFI]     = new_call_gefi;
    g_newFn[FN_GFEI]     = new_call_gfei;
    g_newFn[FN_UPDIDX]   = new_UpdateIndexForEepromDeal;
}

/* One lockstep step: install the world + side state, run ref, snapshot; install
 * the SAME world + side state again, run new, snapshot; compare.
 *
 * 注意每步都重装 side 状态 —— 与 f_interface 不同。本模块的多数函数把状态留在
 * **模块全局**里 (funcCodeRwMode / I2cMsg / funcCodeRead...), 这些字段是
 * install_side() 管的; 只有 ReadFuncCode/WriteFuncCode 的函数级 static 不在其中,
 * 而那两个函数的相位是**故意**用连续 step() 来推进那两个 static 的。
 */
static void step(int which)
{
    install_side(0);
    install_world();
    g_retSet[0] = 0;
    g_refFn[which]();
    snapshot(0);
    snap_world(0);

    install_side(1);
    install_world();
    g_retSet[1] = 0;
    g_newFn[which]();
    snapshot(1);
    snap_world(1);

    g_cases++;
    compare();

    g_retSet[0] = 0;
    g_retSet[1] = 0;
}

/* 不断言 side 状态的 step: 给 ReadFuncCode/WriteFuncCode 的 static 推进用。
 * 它**不**重装 inSide, 于是两侧各自的 static 与各自的模块全局一起往前走。 */
static void step_seq(int which)
{
    install_world();
    g_retSet[0] = 0;
    g_refFn[which]();
    snapshot(0);
    snap_world(0);

    install_world();
    g_retSet[1] = 0;
    g_newFn[which]();
    snapshot(1);
    snap_world(1);

    g_cases++;
    compare();

    g_retSet[0] = 0;
    g_retSet[1] = 0;
}

/* ---- RwEeprom 的"成功"握手模拟 -------------------------------------------
 *
 * RwEeprom 的四条出口 (见 ref/f_eeprom_ref.c 第 229..276 行):
 *
 *   I2cMsg.status == IDLE       -> 真的去写寄存器 (WRITE 支或"设指针"支),
 *                                  然后 status 变成 WRITE_BUSY / SEND_NOSTOP_BUSY,
 *                                  **return 0**
 *   I2cMsg.status == RESTART    -> 真的去写寄存器 ("读+重启"支),
 *                                  status 变成 READ_BUSY, **return 0**
 *   I2cMsg.status == RW_OK      -> status 变回 IDLE, **return 1**
 *   其它 (WRITE_BUSY / SEND_NOSTOP_BUSY / READ_BUSY) -> **return 0**, 什么都不动
 *
 * 调用它的上层 (ReadFuncCode / WriteFuncCode / EepromDeal) 一律写成
 * `if (1 == RwEeprom(...))`, 也就是只有**走到 RW_OK 那一态**才算成功。
 * 而 RW_OK 是由中断服务 I2cIntDeal 在"硬件做完了"时设上的。
 *
 * 测试台必须把这条链走全, 否则会漏掉一大片:
 *   * 如果每步之前都强设 status = RW_OK (本用例初版就是这么写的), RwEeprom
 *     **一次寄存器都不写**, 直接 return 1。于是 RwI2cBus 的三个支路、IDLE/RESTART
 *     两态的状态迁移全部不可达 —— ReadFuncCode/WriteFuncCode 里凡是靠
 *     "写出去的内容"或"status 迁移后走到哪一支"区分的变异, 一律 MISSED。
 *     实测: 初版漏了 24 条。
 *
 * 所以这里做**两步握手**: 先按当前 status 调一次 (让 RwEeprom 干它该干的活),
 * 若它没返 1, 就按 I2cIntDeal 的行为把 status 推到 RW_OK, 再调一次拿"成功"。
 * 每一步都记一账 (g_cases), 两侧比的是"两次调用各自把世界改成了什么样"。
 *
 * 注意: 这两步必须是**同一台步进器**里的连续调用, 且两侧完全对称 ——
 * 所以用 step_seq() (不重装 SIDE), 靠 inSide 的当前值继续往下走。
 */

/* 调试计数: 序列里两侧各返过几次"成功" (ret==1)。只看数字不看代码。 */
static unsigned g_hs_ok_ref;
static unsigned g_hs_ok_new;

static void rweeprom_step(int which, Uint16 mode)
{
    unsigned guard;

    for (guard = 0; guard < 3; guard++)
    {
        g_argMode = mode;
        step_seq(which);

        if (g_retSet[0] && g_ret[0] == 1) { g_hs_ok_ref++; }
        if (g_retSet[1] && g_ret[1] == 1) { g_hs_ok_new++; }

        if (g_retSet[0] && g_ret[0] == 1 && g_retSet[1] && g_ret[1] == 1)
        {
            return;                 /* 两侧都走到 RW_OK 那一态, 握手完成 */
        }

        /* 按 I2cIntDeal 的做法把"硬件做完了"补上, 让下一次调用落到 RW_OK 支。
         * 两侧分别设自己的 I2cMsg —— 它们是各自的 SIDE, 本来就分开。 */
        ref_I2cMsg.status = I2C_MSG_STATUS_RW_OK;
        new_I2cMsg.status = I2C_MSG_STATUS_RW_OK;
    }
}

/* 把 inSide 装进两侧, 然后开始序列。
 *
 * `step_seq()` **不重装** inSide (那是它存在的理由: 让函数内 static 一路往前
 * 走)。所以序列的**第一步**必须先把 inSide 落进两侧的模块全局 —— 否则第一步
 * 读的是上一个相位留下的随机值。实测: 少了这一步, WriteFuncCode 第一步就撞
 * 参数闸返回 PARA_ERROR (ret=4), 后面 29 步全在同一个错状态上打转,
 * 于是"补了覆盖"看着跑了 442 例, 实际一次都没进 verify 支。
 */
static void seq_begin(void)
{
    install_side(0);
    install_world();
    install_side(1);
    install_world();
}

/* ---- deterministic input fill -------------------------------------------- */

static unsigned long g_rng = 0x20261011UL;

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

/* Non-trivial prefill for every module-owned input: ref-vs-new only ever sees a
 * change on the NEW side, so "the rewrite stopped writing this field" is visible
 * only if the position already held something else. */
static void side_begin(unsigned long seed)
{
    g_rng = seed ? seed : 1UL;

    {
        unsigned q;
        inSide.funcCodeOneWriteNum = (Uint16)(rnd() % 101);  /* <= ONE_WRITE_NUMBER_MAX(100) */
        for (q = 0; q < 200; q++)
        {
            /* 数组元素是 **fcIndex** (`GetEepromIndexFromFcIndex(funcCodeOneWriteIndex[n])`),
             * 所以必须落在替身正向表的有效域 0..916, 否则同一类表外读。 */
            inSide.funcCodeOneWriteIndex[q] = (Uint16)(rnd() % 917);
        }
    }
    inSide.funcCodeRwMode    = (Uint16)rnd();
    inSide.funcCodeRwModeTmp = (Uint16)rnd();
    inSide.funcCodeRwStatus  = (Uint16)(rnd() % 4);

    rnd_fill(&inSide.I2cMsg, sizeof inSide.I2cMsg);
    /* status 是 enum, 取值必须落在 I2C_MSG_STATUS_IDLE..RW_OK; 随机填会不会
     * 命中任何分支 (which is fine), 但为了覆盖, 默认给一个合法的。 */
    inSide.I2cMsg.status = (enum I2C_MSG_STATUS)(rnd() % 6);
    /* bytes 是**字节数**, 模块契约: bytes = number << 1, 而
     * number <= EEPROM_PAGE_NUM_FUNC_CODE = 2, 所以 bytes <= 4, 正好等于
     * I2C_MSG_BUFFER_SIZE (DSP_2803X 支)。I2cIntDeal 的 READ_BUSY 分支
     *
     *     for (i = 0; i < I2cMsg.bytes; i++) I2cMsg.buffer[i] = I2caRegs.I2CDRR;
     *
     * 没有上界夹取 (这是既有缺陷, 见 README), 喂 >4 会**踩爆 buffer[4]** ——
     * 宿主上直接段错误。所以按契约遍历 0..4。 */
    inSide.I2cMsg.bytes  = (Uint16)(rnd() % 5);   /* 0..4 */
    /* highAddr/lowAddr 是 EEPROM 地址高/低字节, 任意 16 位都合法。 */
    inSide.I2cMsg.highAddr = (Uint16)rnd();
    inSide.I2cMsg.lowAddr  = (Uint16)rnd();

    /* ---- eepromIndex 域必须落在模块契约内 ---------------------------------
     *
     * `EepromDeal` 在 READ 分支里直接做
     *     GetFcIndexFromEepromIndex(index4EepromDeal + i)
     * 而活的 GetFcIndexFromEepromIndex 是 **`eeprom2Fc[a]` 查表**, 表长
     * EEPROM_INDEX_USE_LENGTH + 1 = 995。真实调用方把
     *     index4EepromDeal < endIndexRwFuncCode <= EEPROM_INDEX_USE_LENGTH = 994
     * 钉死; 喂一个 random Uint16 (可到 65535) 会**表外读**, 宿主上直接段错误。
     * 那不是"更全的覆盖", 而是把不可达状态喂进测试台 —— 覆盖看着更宽,
     * 结论其实更弱 (见 MEMORY.md 负向对照纪律里 MISSED 分类 (b))。
     *
     * 所以剩下的 eepromIndex 类字段 (funcCodeRead.index / funcCodeWrite.index)
     * 按契约随机 0..994 —— 它们只进 I2cMsg.highAddr, 不参与查表, 不会表外读。
     * startIndexWriteSeries / endIndexWriteSeries 是 fcIndex 域,
     * 上界 = FNUM_ALL-1 由替身表决定, 取 0..916。 */
    /* endIndexRwFuncCode **不能随机**, 钉在模块自己的默认值
     * EEPROM_INDEX_USE_LENGTH(=994) 上。原因是一条**无符号回绕**:
     * EAD 块里 `number4EepromDeal = tmp - index4EepromDeal`, 而
     * index4EepromDeal 是函数级 static, driver 装不进去 —— 于是
     * "endIndex < index" 这个组合 driver 无法躲开, 真固件也到不了
     * (index 只朝 endIndex 长, 越过就结束本 mode), 但宿主上
     * number 回绕成 ~65000 会让 `eeprom2Fc[index+i]` 一路读到表外,
     * 直接段错误。
     * 实测 2026-10-12: funcCodeRwStatus 补进 SIDE_INSTALL 之后随机相位
     * 立刻 rc=139, 就是这条回绕 (之前状态装不进去, EAD 块很少进, 侥幸没炸)。
     * "把小 endIndex 装进去"的覆盖改由 phase_deal_edges (ii) 用**两步序列**
     * 给: 先走一遍"设模式"把 index 落到 0, 第二步才装小 endIndex ——
     * 那时 index == 0, number = endIndex - 0 不会回绕, 夹取支照样测得到。 */
    inSide.endIndexRwFuncCode    = EEPROM_INDEX_USE_LENGTH;         /* 994 */
    inSide.startIndexWriteSeries = (Uint16)(rnd() % 917);            /* fcIndex */
    inSide.endIndexWriteSeries   = (Uint16)(rnd() % 917);            /* fcIndex */

    inSide.funcCodeRead.index  = (Uint16)(rnd() % 995);
    inSide.funcCodeRead.number = (Uint16)(1 + rnd() % 8);
    rnd_fill(inSide.funcCodeRead.data, sizeof inSide.funcCodeRead.data);

    inSide.funcCodeWrite.index  = (Uint16)(rnd() % 995);
    inSide.funcCodeWrite.number = (Uint16)(1 + rnd() % 8);
    rnd_fill(inSide.funcCodeWrite.data, sizeof inSide.funcCodeWrite.data);

    inSide.rwFuncCodeRepeatDelayTicker = (Uint16)rnd();

    inSide.eepromOperateTime.readFlag    = (Uint16)(rnd() & 1);
    inSide.eepromOperateTime.readTicker  = (Uint16)rnd();
    inSide.eepromOperateTime.writeFlag   = (Uint16)(rnd() & 1);
    inSide.eepromOperateTime.writeTicker = (Uint16)rnd();
    inSide.eepromOperateTime.rwFlag      = (Uint16)(rnd() & 1);
    inSide.eepromOperateTime.rwTicker    = (Uint16)rnd();
}

static void world_begin(unsigned long seed)
{
    g_rng = seed ? seed : 1UL;

    rnd_fill(&inW.funcCode, sizeof inW.funcCode);

    inW.errorEeprom    = (Uint16)rnd();
    inW.powerOnStatus  = (Uint16)(rnd() % 6);
    inW.aiaoChckReadOK = (Uint16)(rnd() & 1);

    /* 外设: 整结构体随机, 于是"没写"与"写成 0"可区分。 */
    rnd_fill(&inW.i2c,      sizeof inW.i2c);
    rnd_fill(&inW.gpioCtrl, sizeof inW.gpioCtrl);
    rnd_fill(&inW.gpioData, sizeof inW.gpioData);

    rnd_fill(inW.cpuCtl, sizeof inW.cpuCtl);

    inW.delaySum   = (Uint32)rnd();
    inW.delayCalls = (Uint32)rnd();

    inW.gfciCalls     = (Uint16)rnd();
    inW.gfciLastIndex = (Uint16)rnd();
    inW.gfciLastType  = (Uint16)rnd();
    inW.gfciArgSum    = (Uint32)rnd();
    inW.gfcioCalls    = (Uint16)rnd();
    inW.gfcioLastIndex = (Uint16)rnd();
    inW.rcpoCalls     = (Uint16)rnd();
}

static void case_begin(unsigned long seed)
{
    side_begin(seed);
    world_begin(seed ^ 0x5A5A5A5AUL);
}

/* ---- phases -------------------------------------------------------------- */

/* 0. 两个纯查表函数。
 *
 * ---- 活分支: 反查是**查表**, 不是线性扫描 ----------------------------------
 *
 * `GetFcIndexFromEepromIndex` 里有一对 `#if`:
 *
 *     #if F_DEBUG_RAM
 *     #define DEBUG_F_TABLE_FC2EEPROM_CONST   0
 *     #elif 1
 *     #define DEBUG_F_TABLE_FC2EEPROM_CONST   1
 *     #endif
 *
 * 宿主机上 F_DEBUG_RAM 未定义, 所以展开成 **1** —— 活代码是
 *
 *     i = eeprom2Fc[a];      -- 反查表, 995 项, 在 include/function/ 下
 *
 * 而 `for (i=0; i<EEPROM_INDEX_USE_LENGTH; i++) ... FUNCCODE_RSVD4ALL_INDEX`
 * 那一支是 **死代码** (不可达), 覆盖不到也不算漏 —— 见 README。
 *
 * 这直接决定了输入域: eeprom2Fc[] 长度 = EEPROM_INDEX_USE_LENGTH + 1 = 995,
 * 合法下标 **0..994**。喂 995 及以上就是表外读, 两侧各自读到不同的相邻
 * 内存, 会报出**假差异** (实测 a=1000 时 ref=909 / new=4, 全是垃圾, 因为
 * ref_eeprom2Fc 与 new_eeprom2Fc 后面跟的字节不同)。调用方 EepromDeal 把
 * index4EepromDeal+i 钳在 endIndexRwFuncCode (<= EEPROM_INDEX_USE_LENGTH
 * = 994) 之内, 所以 0..994 就是模块契约。
 *
 * ---- 正向表的两支 ----------------------------------------------------------
 *
 * `GetEepromIndexFromFcIndex` 走的是 (同样由 DEBUG_F_TABLE_FC2EEPROM_CONST
 * 选出的) 属性表 / 无属性表两支:
 *
 *     if (index >= B)  return fcNoAttri2Eeprom[index - B];   B = GetCodeIndex(fChk[0])
 *     else             return funcCodeAttribute[index].eepromIndex;
 *
 * B 是 `GetCodeIndex(funcCode.group.fChk[0])` = **fChk[0] 的地址减
 * funcCode.all[0] 的地址**, 与 fChk[0] 的**值**无关 —— 它是编译期常量
 * 909 (fChk 在 funcCode.all[] 里的字偏移)。所以随机 `funcCode` 不会移动这条
 * 边界; 边界只由 `fcIdx[]` 里的取值决定。替身属性表 909 项 (fcIndex 0..908),
 * 替身无属性表 8 项 (fcIndex 909..916), 所以 fcIndex 域取 0..916。
 *
 * 覆盖点:
 *   fcIndex    0, 1, 2, 3, 4..8, 300, 807..809, 907, 908 | 909..916
 *              —— 908/909 是 B 的两侧; 4..7 与 909..912 在替身表里撞同一个
 *                 eepromIndex (见 build/_gen_eep_attr.py 文件头), 于是
 *                 "正向两个 fcIndex 映到同一格" 与 "反向取先命中者" 都可见。
 *   eepromIndex 0, 1, 4..8, 100, 500, 807..809, 908 | 909, 910, 912, 950,
 *              992, 993, 994
 *              —— 909..993 在替身表里是**空洞**; 994 是最后一个合法下标。
 */
static void phase_lookup(void)
{
    static const Uint16 fcIdx[] = {
        /* B = 909 的两侧 + 替身无属性表的偏移端点 (909..916) */
        0, 1, 2, 3, 4, 5, 6, 7, 8, 300, 807, 808, 809,
        907, 908, 909, 910, 911, 912, 913, 914, 915, 916
    };
    static const Uint16 eeIdx[] = {
        /* 合法域 0..994; 909 起是空洞; 994 是上界。**不要**喂 995+ (表外读) */
        0, 1, 4, 5, 6, 7, 8, 100, 500, 807, 808, 809,
        908, 909, 910, 912, 950, 992, 993, 994
    };
    unsigned i;

    case_begin(0x3001);

    for (i = 0; i < sizeof fcIdx / sizeof fcIdx[0]; i++)
    {
        g_argIndex = fcIdx[i];
        g_cur = "gefi";
        step(FN_GEFI);
    }

    for (i = 0; i < sizeof eeIdx / sizeof eeIdx[0]; i++)
    {
        g_argIndex = eeIdx[i];
        g_cur = "gfei";
        step(FN_GFEI);
    }
}

/* 1. RwI2cBus 的三个 mode 与它前面的两道闸。
 *
 * 闸 1: `if (I2caRegs.I2CMDR.bit.STP == 1) return I2C_STP_NOT_READY_ERROR;`
 * 闸 2: `if ((I2caRegs.I2CSTR.bit.BB == 1) && (I2cMsg.status != RESTART))
 *        return I2C_BUS_BUSY_ERROR;`
 *
 * 闸 2 的两个条件是 AND, 所以三个格都要走:
 *   BB=0                 -> 通过 (status 无关)
 *   BB=1, status=RESTART -> 通过
 *   BB=1, status!=RESTART-> 返回 BUSY
 *
 * 通过两道闸之后按 mode 分三支:
 *   WRITE             -> I2CCNT = bytes+2, DXR = hi, lo, 然后 bytes 个 payload,
 *                        I2CMDR = 0x6E20
 *   READ 且 status==RESTART -> I2CCNT = bytes, I2CMDR = 0x6C20
 *   其余 (含 READ 且 status!=RESTART) -> I2CCNT = 2, DXR = hi, lo,
 *                        I2CMDR = 0x6620   (这是"设指针"那一支)
 *
 * I2cMsg.bytes 决定 payload 循环长度, 取 0..4 (buffer 只有 4 项; 真模块里
 * bytes 最大 = number<<1 <= EEPROM_PAGE_NUM_FUNC_CODE<<1 = 4)。喂 >4 会越界读
 * buffer, 那是既有缺陷, 不在本用例覆盖范围内 —— 见 README。
 */
static void phase_rwi2cbus(void)
{
    static const Uint16 modes[] = { 0, 1, 2, 3, 7 };  /* WRITE/READ/ACK/越界mode */
    static const Uint16 cnts[]  = { 0, 1, 2, 3, 4 };
    unsigned i, j;
    unsigned s;

    case_begin(0x3011);

    for (s = 0; s < 6; s++)
    {
        for (i = 0; i < sizeof modes / sizeof modes[0]; i++)
        {
            for (j = 0; j < sizeof cnts / sizeof cnts[0]; j++)
            {
                case_begin(0x3011 + s * 512 + i * 8 + j);
                /* 两道闸的各种组合: STP / BB / status。 */
                inW.i2c.I2CMDR.bit.STP = (s & 1) ? 1 : 0;
                inW.i2c.I2CSTR.bit.BB  = (s & 2) ? 1 : 0;
                inSide.I2cMsg.status   = (enum I2C_MSG_STATUS)
                                         ((s & 4) ? I2C_MSG_STATUS_RESTART
                                                  : (rnd() % 6));
                inSide.I2cMsg.bytes    = cnts[j];
                inSide.I2cMsg.highAddr = (Uint16)rnd();
                inSide.I2cMsg.lowAddr  = (Uint16)rnd();
                rnd_fill(inSide.I2cMsg.buffer, sizeof inSide.I2cMsg.buffer);

                g_argMode = modes[i];
                g_cur = "rwi2cbus";
                step(FN_RWI2C);
            }
        }
    }
}

/* 2. I2cIntDeal 的四个分支。
 *
 *   I2CISRC == I2C_NO_ISRC (0)              -> 早退, 什么都不动
 *   I2CISRC == I2C_SCD_ISRC (6):
 *        status == WRITE_BUSY       -> 调 RwI2cBus(ACK) 至多 100 次
 *        status == SEND_NOSTOP_BUSY -> status = IDLE
 *        status == READ_BUSY        -> bytes 次 buffer[i] = I2CDRR, status = RW_OK
 *   I2CISRC == I2C_ARDY_ISRC (3):
 *        I2CSTR.NACK == 1        -> I2CMDR.STP = 1; I2CSTR = CLR_NACK
 *        status == SEND_NOSTOP   -> status = RESTART
 *        status == WRITE_BUSY    -> status = RW_OK; InitSetI2ca()   <-- **副作用**
 *   其它 I2CISRC (任意其它值)              -> 什么都不做
 *
 * 三处细节都要照顾到:
 *   (a) SCD 分支的 while 循环: 循环体是 `if (++tmp >= 100) break;`, 而循环条件
 *       是 RwI2cBus(ACK) != SUCCESS。RwI2cBus(ACK) 走"设指针"那一支, 只要
 *       STP==0 且 (BB==0 || status==RESTART) 就返回 SUCCESS -> 循环 0 次;
 *       否则返回 0x5555/0x1000 -> 循环 100 次后 break。两个极端都要走到。
 *   (b) ARDY + WRITE_BUSY 分支会调 InitSetI2ca(), 那是一次**寄存器大写入** ——
 *       正是要观测的东西。所以 I2caRegs 必须整结构体快照。
 *   (c) READ_BUSY 分支的 bytes 次循环。bytes 取 0..4。
 *
 * 还有一个**必须驱到**的边界: 上面 (a) 的 while 循环里, `RwI2cBus(ACK)` 在
 * STP==1 时立刻返回 0x5555, 于是 tmp 数到 100 才 break。tmp 从 0 起,
 * `++tmp >= 100` 在 tmp 到 100 时成立 —— 所以恰好 100 次。若改写把它写成 99
 * 或 101, "100 次 RwI2cBus 的寄存器副作用"会差一次, I2CCNT 之类的最终值一样
 * (每次都写同样的值), 但**调用次数**不同 —— 而模块里没有调用计数器。
 * 所以这一格靠的是"最后一次 I2CMDR 的值"加上延后几拍: 见下面的哨兵。
 */
static void phase_intdeal(void)
{
    static const Uint16 srcs[] = { 0, 3, 6, 7, 1, 4, 5 };
    static const enum I2C_MSG_STATUS sts[] = {
        I2C_MSG_STATUS_IDLE, I2C_MSG_STATUS_WRITE_BUSY,
        I2C_MSG_STATUS_SEND_NOSTOP_BUSY, I2C_MSG_STATUS_RESTART,
        I2C_MSG_STATUS_READ_BUSY, I2C_MSG_STATUS_RW_OK
    };
    unsigned i, j, k;
    unsigned n;

    case_begin(0x3021);

    for (i = 0; i < sizeof srcs / sizeof srcs[0]; i++)
    {
        for (j = 0; j < sizeof sts / sizeof sts[0]; j++)
        {
            for (k = 0; k < 6; k++)
            {
                case_begin(0x3021 + i * 4096 + j * 256 + k * 16);
                inW.i2c.I2CISRC.all     = srcs[i];
                inW.i2c.I2CSTR.bit.NACK = (k & 1) ? 1 : 0;
                inW.i2c.I2CMDR.bit.STP  = (k & 2) ? 1 : 0;
                inW.i2c.I2CSTR.bit.BB   = (k & 4) ? 1 : 0;
                inW.i2c.I2CDRR          = (Uint16)(0xA000u + k * 0x111u + i);

                inSide.I2cMsg.status = sts[j];
                inSide.I2cMsg.bytes  = (Uint16)(k % 5);
                inSide.I2cMsg.highAddr = (Uint16)(0x1000u + k);
                inSide.I2cMsg.lowAddr  = (Uint16)(0x2000u + k);
                rnd_fill(inSide.I2cMsg.buffer, sizeof inSide.I2cMsg.buffer);

                g_cur = "i2cint";
                step(FN_INT);
            }
        }
    }

    /* (a2) READ_BUSY 的 payload 循环长度边界: bytes = 0,1,2,3,4 —— 上面已覆盖;
     *      再补一个 bytes = 5..7 的越界读 (既有缺陷, 记录不测, 见 README)。 */
    n = 0;
    for (i = 0; i < 3; i++)
    {
        case_begin(0x3021 + 0xF000 + i);
        inW.i2c.I2CISRC.all = I2C_SCD_ISRC;
        inW.i2c.I2CDRR = (Uint16)(0x5000u + i);
        inSide.I2cMsg.status = I2C_MSG_STATUS_READ_BUSY;
        inSide.I2cMsg.bytes  = (Uint16)(5 + i);
        g_cur = "i2cint-bigbytes";
        step(FN_INT);
        n++;
    }
    (void)n;
}

/* 3. RwEeprom 的三支状态机。
 *
 *   status == IDLE:
 *        mode == WRITE -> RwI2cBus(WRITE); 成功则 status = WRITE_BUSY
 *        mode != WRITE -> 循环 RwI2cBus(READ) 至多 100 次, 然后 status = SEND_NOSTOP_BUSY
 *        两种情况都 return 0
 *   status == RESTART:
 *        循环 RwI2cBus(READ) 至多 100 次, 然后 status = READ_BUSY, return 0
 *   status == RW_OK:
 *        status = IDLE, return 1
 *   其它 (WRITE_BUSY / SEND_NOSTOP_BUSY / READ_BUSY):
 *        return 0
 *
 * 返回值的 1 只出现在 RW_OK 那一格, 而 WriteFuncCode / ReadFuncCode 正是靠它
 * 推进状态机的 —— 所以这一格必须精确覆盖。
 */
static void phase_rweeprom(void)
{
    static const enum I2C_MSG_STATUS sts[] = {
        I2C_MSG_STATUS_IDLE, I2C_MSG_STATUS_WRITE_BUSY,
        I2C_MSG_STATUS_SEND_NOSTOP_BUSY, I2C_MSG_STATUS_RESTART,
        I2C_MSG_STATUS_READ_BUSY, I2C_MSG_STATUS_RW_OK
    };
    unsigned i, j, k;

    case_begin(0x3031);

    for (i = 0; i < sizeof sts / sizeof sts[0]; i++)
    {
        for (j = 0; j < 5; j++)            /* mode = WRITE(0) / READ(1) / 其它 */
        {
            for (k = 0; k < 4; k++)        /* STP / BB 组合 */
            {
                case_begin(0x3031 + i * 1024 + j * 32 + k * 4);
                inSide.I2cMsg.status = sts[i];
                inSide.I2cMsg.bytes  = (Uint16)(k % 5);
                inW.i2c.I2CMDR.bit.STP = (k & 1) ? 1 : 0;
                inW.i2c.I2CSTR.bit.BB  = (k & 2) ? 1 : 0;

                g_argMode = (Uint16)j;
                g_cur = "rweeprom";
                step(FN_RWEE);
            }
        }
    }
}

/* 4. ReadFuncCode 的验证计数逻辑 —— 这是本模块最要紧的一片。
 *
 * 结构 (活分支):
 *     eepromOperateTime.readFlag = 1;                       <-- 无条件副作用
 *     if (!number || number > EEPROM_PAGE_NUM_FUNC_CODE(=2)) return PARA_ERROR;
 *     if (bVerifyWrong && rwFuncCodeRepeatDelayTicker < 100)  return READING;
 *     rwFuncCodeRepeatDelayTicker = 0;  bVerifyWrong = 0;
 *     I2cMsg.highAddr/lowAddr = (index + 16) << 1 的 hi/lo;
 *     I2cMsg.bytes = number << 1;
 *     if (RwEeprom(READ) == 1) {
 *         readTime++;
 *         for (i = number-1; i >= 0; i--) {
 *             dataRead[i] = (buffer[i<<1] << 8) + buffer[(i<<1)+1];
 *             if (readTime == 1)  funcCodeRw->data[i] = dataRead[i];
 *             else                if (data[i] != dataRead[i]) break;
 *         }
 *         if (readTime == 1)  { readRightTime = 1; return READING; }
 *         if (i < 0) {                       <-- 全部一致
 *             readRightTimeMax = 3; (WRITE_DATA 状态时 = 2)
 *             if (++readRightTime >= readRightTimeMax) {
 *                 readTime = 0; readFlag = 0; readTicker = 0;
 *                 return OK;
 *             }
 *         } else {                           <-- 有子项不一致
 *             readTime = 0; bVerifyWrong = 1;
 *             return READING;
 *         }
 *     }
 *     return READING;
 *
 * 可观测的状态: funcCodeRw->data[] (第一次读时写入)、eepromOperateTime 三个字段、
 * 返回码。**readTime / readRightTime / bVerifyWrong 是 static, 不可安装** ——
 * 所以必须按"步"推进, 每一步都比一次。
 *
 * 覆盖的关键几步:
 *   step A  第一次成功读 -> readTime 0->1, 写入 data[], readRightTime=1, READING
 *   step B  再读, 一致   -> readRightTime 1->2, 仍 < 3, 返回 READING (落空)
 *   step C  再读, 一致   -> readRightTime 2->3, >= 3, 返回 OK 并清零 readTime
 *   step D  再读, 不一致 -> readTime=0, bVerifyWrong=1, 返回 READING
 *   step E  重试被挡     -> 因为 bVerifyWrong 且 rwFuncCodeRepeatDelayTicker<100
 *                           (前一步把它清 0, 需先 EepromOperateTimeDeal 加)
 *   step F  重试放行     -> ticker 加到 >=100 后再读, 一次成功 -> readTime=1
 *
 * 三处最容易漏的边界:
 *   (i)   readRightTimeMax 的 2 / 3 之差, 只由 funcCodeRwStatus 决定
 *         (FUNCCODE_RW_STATUS_WRITE_DATA == 3)。两侧的 funcCodeRwStatus 都是
 *         隐藏 static, 但 install_side 装不了它 —— 它在 new/ref 各自 TU 里。
 *         所以这一路只能在 WriteFuncCode 的相位里间接走到 (见 phase_write)。
 *         本相位把 readRightTimeMax 固定在 3, 把 2 那一格交给 phase_write。
 *   (ii)  循环下标 i 的边界: number = 1 与 2 都走, 且第一次读 (readTime==1)
 *         与后续读的分支不同 —— 必须至少两次成功读才走到验证比较。
 *   (iii) `dataRead[i] = (buffer[i<<1] << 8) + buffer[(i<<1)+1]` 的字节拼接。
 *         把某个 buffer 字节改成 0xFF/0x00, 让拼接的高低位都可辨。
 */
static void phase_read(void)
{
    unsigned k;

    /* (1) 第一个成功读 -> 写 data[]; 然后连续几次一致 -> OK; 再一次不一致。 */
    for (k = 0; k < 3; k++)
    {
        case_begin(0x3041 + k * 64);
        inSide.I2cMsg.status = I2C_MSG_STATUS_RW_OK;   /* RwEeprom 直接返 1 */
        inSide.I2cMsg.bytes  = (Uint16)(2 + k);        /* 1..3 */

        inSide.funcCodeRead.index  = (Uint16)(0x100 + k);
        inSide.funcCodeRead.number = (Uint16)(1 + (k % 2));   /* 1 与 2 */
        /* 预置 data[]: 第一次读会覆盖它, 后续读要与它比。 */
        inSide.funcCodeRead.data[0] = 0x1111 ^ (Uint16)k;
        inSide.funcCodeRead.data[1] = 0x2222 ^ (Uint16)k;

        /* buffer 决定读回来的值; 让高低字节都非平凡。 */
        inSide.I2cMsg.buffer[0] = 0xAB;
        inSide.I2cMsg.buffer[1] = 0xCD;
        inSide.I2cMsg.buffer[2] = 0xEF;
        inSide.I2cMsg.buffer[3] = 0x01;

        g_argRw    = &ref_funcCodeRead;
        g_argRwNew = &new_funcCodeRead;

        g_cur = "read-first";
        step(FN_READ);          /* readTime 0->1, 写 data[], READINg */

        /* 让 buffer 与第一次写入的 data[] 一致 -> 后续读应逐步 OK。
         * dataRead[0] 是从 buffer[0..1] 拼的, dataRead[1] 是从 buffer[2..3] 拼的。
         * 因为 readTime==1 时 data[i] 已被覆盖成 dataRead[i], 所以**不动 buffer**
         * 就自然一致 —— 这正是原版的行为。 */
        g_cur = "read-verify2";
        step(FN_READ);          /* readRightTime 1->2, 返回 READING */
        g_cur = "read-verify3";
        step(FN_READ);          /* readRightTime 2->3, 返回 OK, readTime 清 0 */
        g_cur = "read-ok-then-again";
        step(FN_READ);          /* readTime 0->1 再来一遍 */
    }

    /* (2) 不一致 -> bVerifyWrong 置位, readTime 清零。 */
    for (k = 0; k < 2; k++)
    {
        case_begin(0x3041 + 0x200 + k * 64);
        inSide.I2cMsg.status = I2C_MSG_STATUS_RW_OK;
        inSide.funcCodeRead.number = 2;
        inSide.funcCodeRead.data[0] = 0xAAAA;
        inSide.funcCodeRead.data[1] = 0xBBBB;
        inSide.I2cMsg.buffer[0] = 0xAA;
        inSide.I2cMsg.buffer[1] = 0xAA;   /* -> dataRead[0] = 0xAAAA (一致) */
        inSide.I2cMsg.buffer[2] = 0x00;   /* -> dataRead[1] = 0x00xx (不一致) */
        inSide.I2cMsg.buffer[3] = 0x00;

        g_argRw    = &ref_funcCodeRead;
        g_argRwNew = &new_funcCodeRead;

        g_cur = "read-mismatch-first";
        step(FN_READ);          /* readTime 0->1, 写入 data[] */
        /* 第二次: data[1] 被第一次覆盖成 0x0000, 但 buffer 里有 0xAAAA / 0xBBBB?
         * 不 —— 第一次把 data[i] 全写成 dataRead[i], 所以第二次一定一致。
         * 要造不一致, 必须在两次之间**改 buffer**。 */
        inSide.I2cMsg.buffer[2] = 0xFF;
        inSide.I2cMsg.buffer[3] = 0xFF;
        g_cur = "read-mismatch";
        step(FN_READ);          /* 比较在 i=1 处不一致 -> break -> bVerifyWrong=1 */
    }

    /* (3) `bVerifyWrong && ticker < 100` 的**早退**。
     *
     * 上一步留下 bVerifyWrong=1 且 ticker=0 (函数里写 0)。现在:
     *   连续步进不推进 ticker -> 每次都早退 (返回 READING, 且**不写 readFlag**?
     *   注意 readFlag=1 在早退**之前**执行, 所以早退步骤里 readFlag 仍被置 1)。
     * 然后把 ticker 用 EepromOperateTimeDeal 推过 100, 早退放行。 */
    {
        case_begin(0x3041 + 0x400);
        inSide.I2cMsg.status = I2C_MSG_STATUS_RW_OK;
        inSide.funcCodeRead.number = 2;
        g_argRw    = &ref_funcCodeRead;
        g_argRwNew = &new_funcCodeRead;

        /* 造 bVerifyWrong=1: 先成功读一次, 改 buffer, 再读一次不一致。 */
        inSide.I2cMsg.buffer[0] = 0x11; inSide.I2cMsg.buffer[1] = 0x11;
        inSide.I2cMsg.buffer[2] = 0x22; inSide.I2cMsg.buffer[3] = 0x22;
        g_cur = "rvi-first";
        step(FN_READ);
        inSide.I2cMsg.buffer[2] = 0x99; inSide.I2cMsg.buffer[3] = 0x99;
        g_cur = "rvi-bad";
        step(FN_READ);          /* bVerifyWrong = 1, ticker = 0 */

        /* ticker = 0 -> 早退。连走 3 步, 保证每步的"早退"都被比。 */
        g_cur = "rvi-blocked";
        step(FN_READ);
        step(FN_READ);
        step(FN_READ);

        /* 推 ticker 到 99, 99, 100 三次 —— 100 是 `>=` 的边界。 */
        {
            unsigned t;
            for (t = 0; t < 99; t++)
            {
                g_cur = "rvi-tick";
                step(FN_TIME);
            }
            /* ticker 现在是 99: 仍 < 100 -> 早退 */
            g_cur = "rvi-at99";
            step(FN_READ);
            g_cur = "rvi-tick100";
            step(FN_TIME);      /* 100 */
            /* 现在 ticker = 100, `100 < 100` 假 -> 放行 */
            g_cur = "rvi-pass";
            step(FN_READ);
        }
    }

    /* (4) RwEeprom 返 0 (未完成) 的那一支: 直接落到底部 return READING,
     *     且 readTime 不加 —— 所以"读两次才 OK"的计数不会被这次污染。 */
    {
        static const enum I2C_MSG_STATUS sts[] = {
            I2C_MSG_STATUS_IDLE, I2C_MSG_STATUS_WRITE_BUSY,
            I2C_MSG_STATUS_SEND_NOSTOP_BUSY, I2C_MSG_STATUS_RESTART,
            I2C_MSG_STATUS_READ_BUSY
        };
        unsigned s;
        for (s = 0; s < sizeof sts / sizeof sts[0]; s++)
        {
            case_begin(0x3041 + 0x600 + s * 32);
            inSide.I2cMsg.status = sts[s];
            inSide.funcCodeRead.number = 2;
            inSide.funcCodeRead.data[0] = 0x0102;
            inSide.funcCodeRead.data[1] = 0x0304;
            g_argRw    = &ref_funcCodeRead;
            g_argRwNew = &new_funcCodeRead;
            g_cur = "read-notdone";
            step(FN_READ);
        }
    }

    /* (5) number 的边界: 0 / 1 / 2 / 3 (3 > PAGE_NUM=2 -> PARA_ERROR)。 */
    {
        static const Uint16 nums[] = { 0, 1, 2, 3, 65535 };
        unsigned n2;
        for (n2 = 0; n2 < sizeof nums / sizeof nums[0]; n2++)
        {
            case_begin(0x3041 + 0x800 + n2 * 16);
            inSide.I2cMsg.status = I2C_MSG_STATUS_RW_OK;
            inSide.funcCodeRead.index  = (Uint16)(n2 * 100);
            inSide.funcCodeRead.number = nums[n2];
            g_argRw    = &ref_funcCodeRead;
            g_argRwNew = &new_funcCodeRead;
            g_cur = "read-number";
            step(FN_READ);
        }
    }
}

/* 5. WriteFuncCode 的三态机 + 它的页对齐判据。
 *
 * 第一段是**参数校验**:
 *     if ((index + number > (index & 0xFFF8) + 0x0008)
 *         || (0 == number) || (number > PAGE_NUM(=2)))  return PARA_ERROR;
 *
 * 这一条是"一次不能跨页写"的判据。页是 8 个 FUNCCODE (index & 0xFFF8),
 * 所以合法组合不少; 必须把**恰好等于边界**的几格都喂到:
 *     index = 8k+0..8k+7, number = 1..2 -> 求 (index&0xFFF8)+8 - index >= number
 *   例: index=7 (页基 0), number=1  -> 7+1=8 <= 0+8  OK
 *       index=7, number=2          -> 7+2=9 >  0+8  PARA_ERROR
 *       index=0, number=2          -> 2 <= 8      OK
 *       index=6, number=2          -> 8 <= 8      OK   (恰好落在边界)
 *       index=8, number=2          -> 10 <= 16    OK   (换页了, 但仍在页内)
 *
 * 第二段是三态机:
 *     status == WRITE:
 *         number = (PAGE_NUM == funcCodeRw->number) ? PAGE_NUM-1 : funcCodeRw->number;
 *         填 buffer[i<<1] / buffer[(i<<1)+1]  ← data[i] 的高/低字节
 *         highAddr/lowAddr = (index + 16) << 1
 *         bytes = number << 1
 *         RwEeprom(WRITE) == 1 -> status = (number == number_in) ? READ_TO_VERIFY
 *                                                             : WRITE_REMAINDER
 *     status == WRITE_REMAINDER:
 *         只写 data[PAGE_NUM-1] 一个 (buffer[0..1]), 地址 += PAGE_NUM-1, bytes = 2
 *         RwEeprom(WRITE) == 1 -> status = READ_TO_VERIFY
 *     status == READ_TO_VERIFY:
 *         funcCodeRead.index/number 从 funcCodeWrite 抄
 *         ReadFuncCode(&funcCodeRead) == OK -> 逐项比"读回的"与"要写的"
 *             全一致 -> writeFlag=0, writeTicker=0, status = WRITE, return OK
 *             有不一致 -> bVerifyWrong = 1, status = WRITE, return WRITING
 *         否则 (ReadFuncCode 未完成) -> return WRITING
 *
 * 关键: WRITE_REMAINDER 只在 `funcCodeRw->number == PAGE_NUM == 2` 且
 * `writeStatus == WRITE` 且 `RwEeprom(WRITE) == 1` 时设置, 因为此时
 * `number` 被改成 `PAGE_NUM - 1 = 1`, 于是 `number(1) == funcCodeRw->number(2)`
 * 为**假** -> WRITE_REMAINDER。所以"跨页的两字写"这一整条路必须走到。
 */
static void phase_write(void)
{
    unsigned k;

    /* (1) 参数校验的边界扫描。 */
    {
        static const Uint16 idxs[] = { 0, 1, 6, 7, 8, 15, 16, 0x7FE, 0xFFF8, 0xFFFF };
        static const Uint16 nums[] = { 0, 1, 2, 3 };
        unsigned a, b;
        for (a = 0; a < sizeof idxs / sizeof idxs[0]; a++)
        {
            for (b = 0; b < sizeof nums / sizeof nums[0]; b++)
            {
                case_begin(0x3051 + a * 64 + b * 4);
                inSide.I2cMsg.status = I2C_MSG_STATUS_IDLE;
                inSide.funcCodeWrite.index  = idxs[a];
                inSide.funcCodeWrite.number = nums[b];
                rnd_fill(inSide.funcCodeWrite.data, sizeof inSide.funcCodeWrite.data);
                g_argRw    = &ref_funcCodeWrite;
                g_argRwNew = &new_funcCodeWrite;
                g_cur = "write-guard";
                step(FN_WRITE);
            }
        }
    }

    /* (2) 三态机的推进。从一个干净状态开始, 连续步进。
     *
     * 每步都让 RwEeprom 立刻返回 1 (status = RW_OK), 于是状态机一步一态:
     *   step1: WRITE         填 buffer, bytes=2, status -> READ_TO_VERIFY (number==2)
     *          但我们先走"number==1"那支, 让它直接 READ_TO_VERIFY。
     *   然后 ReadFuncCode 要走 3 次才 OK —— 那 3 次都要在同一序列里走完。
     */
    for (k = 0; k < 4; k++)
    {
        unsigned t;
        case_begin(0x3051 + 0x1000 + k * 2129);

        inSide.funcCodeWrite.index  = (Uint16)(16 + k * 8);   /* 页内 */
        inSide.funcCodeWrite.number = (Uint16)(1 + (k % 2));  /* 1 或 2 */
        inSide.funcCodeWrite.data[0] = (Uint16)(0xA000u + k);
        inSide.funcCodeWrite.data[1] = (Uint16)(0xB000u + k);

        g_argRw    = &ref_funcCodeWrite;
        g_argRwNew = &new_funcCodeWrite;

        /* 让 I2cMsg.status 保持 RW_OK: RwEeprom 每次都直接 return 1。
         * 但 WriteFuncCode 里也会写 I2cMsg 的地址/字节字段, 所以每步前重装
         * I2cMsg.status = RW_OK。step_seq 不重装 side, 所以这里手工设。 */
        inSide.I2cMsg.status = I2C_MSG_STATUS_RW_OK;

        for (t = 0; t < 10; t++)
        {
            inSide.I2cMsg.status = I2C_MSG_STATUS_RW_OK;
            g_cur = "write-seq";
            step_seq(FN_WRITE);
        }
    }

    /* (3) number == PAGE_NUM 的 WRITE_REMAINDER 分支。
     *
     * number = 2 且 writeStatus 初始为 WRITE -> 内部 number 变 1 -> 写完一步后
     * 因为 `1 != 2` 而进 WRITE_REMAINDER。这条路只有在**同一个序列**里从
     * WRITE 起步才会走到。上面 (2) 的 k=1/3 已经走了; 这里再单独钉一格,
     * 校验 buffer 里第 0/1 项来自 data[PAGE_NUM-1] = data[1] (而不是 data[0])。 */
    {
        unsigned t;
        case_begin(0x3051 + 0x2000);
        inSide.funcCodeWrite.index  = 100;
        inSide.funcCodeWrite.number = 2;          /* == PAGE_NUM -> 触发 REMAINDER */
        inSide.funcCodeWrite.data[0] = 0xDEAD;
        inSide.funcCodeWrite.data[1] = 0xBEEF;
        inSide.I2cMsg.status = I2C_MSG_STATUS_RW_OK;
        g_argRw    = &ref_funcCodeWrite;
        g_argRwNew = &new_funcCodeWrite;
        for (t = 0; t < 12; t++)
        {
            inSide.I2cMsg.status = I2C_MSG_STATUS_RW_OK;
            g_cur = "write-remainder";
            step_seq(FN_WRITE);
        }
    }

    /* (4) 验证失败 -> bVerifyWrong = 1 -> 下一次调用被 `ticker < 100` 挡住。 */
    {
        unsigned t;
        case_begin(0x3051 + 0x3000);
        inSide.funcCodeWrite.index  = 200;
        inSide.funcCodeWrite.number = 1;
        inSide.funcCodeWrite.data[0] = 0x1234;
        inSide.I2cMsg.status = I2C_MSG_STATUS_RW_OK;
        g_argRw    = &ref_funcCodeWrite;
        g_argRwNew = &new_funcCodeWrite;
        /* 走 W 态 -> READ_TO_VERIFY; 然后反复 ReadFuncCode 直到 OK (3 次)。
         * ReadFuncCode 读回来的值由 I2cMsg.buffer 决定, 而 WriteFuncCode 在
         * WRITE 态已经把 data[0] 的高低字节写进了 buffer[0..1] —— 所以
         * "读回来"必然与 data[0] 一致, 验证成功。这条是**成功**路径。 */
        for (t = 0; t < 14; t++)
        {
            inSide.I2cMsg.status = I2C_MSG_STATUS_RW_OK;
            g_cur = "write-verify-ok";
            step_seq(FN_WRITE);
        }
    }
}

/* 6. EepromDeal —— 调度器。这是本模块最长的一段, 也是分支最多的一段。
 *
 * 它的结构是"每个调用推进一小步"的协程式调度:
 *
 *   1) funcCodeOneWriteNum >= 100 -> 夹回 100, 非 POWER_ON_WAIT 时 errorEeprom = 4
 *   2) I2cIntDeal()                       <-- 每个调用都跑
 *   3) funcCodeRwMode == NO_OPERATION 时:
 *        funcCodeRwModeTmp 非零 -> funcCodeRwMode = Tmp
 *            READ_ALL  -> index4EepromDeal = 0
 *            其它      -> index4EepromDeal = EEPROM_INDEX_USE_INDEX(=4)
 *            endIndexRwFuncCode = EEPROM_INDEX_USE_LENGTH(=994)
 *            WRITE_SERIES -> index4EepromDeal = GetEepromIndexFromFcIndex(start)
 *                            endIndexRwFuncCode = GetEepromIndexFromFcIndex(end)+1
 *        funcCodeOneWriteNum 非零 -> funcCodeRwMode = WRITE_ONE
 *            funcCodeOneWriteNum--
 *            index4EepromDeal = GetEepromIndexFromFcIndex(oneWriteIndex[--后的值])
 *        都为零 -> return
 *   4) funcCodeRwStatus == PREPARE_DATA_FOR_EAD:
 *        WRITE_ONE -> number4EepromDeal = 1
 *        其它 -> tmp = (index4EepromDeal & 0xFFF8) + PAGE_NUM(=2)
 *                if (tmp > endIndex) tmp = endIndex
 *                number4EepromDeal = tmp - index4EepromDeal
 *        funcCodeRead.index/number = index4EepromDeal/number4EepromDeal
 *        WRITE_ALL -> status = PREPARE_DATA_FOR_WRITE
 *        其它:
 *            SAVE_USER_PARA -> funcCodeRead.index += USER_PARA_START_INDEX(=2064)
 *            扫 i: fcIndex = GetFcIndexFromEepromIndex(index4EepromDeal + i)
 *                  首个 != FUNCCODE_RSVD4ALL_INDEX 就 break
 *            i == number4EepromDeal -> UpdateIndexForEepromDeal()
 *            否则                   -> status = READ_CURRENT_DATA
 *   5) READ_CURRENT_DATA: ReadFuncCode(&funcCodeRead) == OK -> status = PREPARE_DATA_FOR_WRITE
 *   6) PREPARE_DATA_FOR_WRITE:
 *        SAVE_USER_PARA / RESTORE_USER_PARA:
 *            funcCodeWrite.index = index4EepromDeal (+2064 若 SAVE)
 *            funcCodeWrite.number = number4EepromDeal
 *            ReadFuncCode(&funcCodeWrite) != OK -> return
 *        for i: fcIndex = GetFcIndexFromEepromIndex(index4EepromDeal + i)
 *            RESTORE_COMPANY_PARA      -> funcCode.all[fcIndex] = GetFuncCodeInit(fcIndex, 0)
 *            RESTORE_COMPANY_PARA_ALL  -> funcCode.all[fcIndex] = GetFuncCodeInit(fcIndex, 1)
 *            READ_ALL                  -> funcCode.all[fcIndex] = funcCodeRead.data[i]
 *            WRITE_ALL                 -> AI/AO 标定区 或 SAVE_USER_PARA_PARA1/2
 *                                         时不动, 否则
 *                                         funcCode.all[fcIndex] = GetFuncCodeInitOriginal(fcIndex)
 *                                         status = WRITE_DATA
 *            WRITE_ONE / WRITE_SERIES  -> 什么都不做
 *            RESTORE_USER_PARA         -> funcCode.all[fcIndex] = funcCodeWrite.data[i]
 *            funcCodeWrite.data[i] = funcCode.all[fcIndex]
 *            if (funcCodeWrite.data[i] != funcCodeRead.data[i]) status = WRITE_DATA
 *        status != WRITE_DATA -> UpdateIndexForEepromDeal()
 *        否则 -> funcCodeWrite.index/number = ..., (+2064 若 SAVE_USER_PARA)
 *   7) WRITE_DATA: WriteFuncCode(&funcCodeWrite) == OK -> UpdateIndexForEepromDeal()
 *
 * 这一段是全模块覆盖面最广的: 每个 mode 一支, 每个 mode 又有"数据变/不变"两个
 * 后果 (决定 UpdateIndexForEepromDeal 与否)。下面按 mode 逐个跑序列。
 */
static void deal_step(void)
{
    g_cur = "deal";
    step(FN_DEAL);
}

/* ---- WriteFuncCode 三态机的**完整**握手 (补缺口) -------------------------
 *
 * 为什么会缺: 别的相位为了让"被测函数自己的逻辑"暴露得干净, 每步之前一律把
 * I2cMsg.status 强设成 RW_OK。于是 RwEeprom 每次都走 `RW_OK` 那一支 ——
 * 把 status 改回 IDLE 然后 return 1, **从不写寄存器, 也从不做状态迁移**。
 *
 * 后果: WriteFuncCode 里这些支路一次都走不到
 *   * `IDLE` 支: 填 buffer + I2cMsg, status -> WRITE_BUSY
 *   * 首片成功后 writeStatus 从 WRITE -> READ_TO_VERIFY_WRITE / WRITE_REMAINDER
 *   * 第二片 (number==PAGE_NUM) 的 WRITE_REMAINDER 分支
 *   * 读回校验循环 `funcCodeRead.data[i] != funcCodeRw->data[i]`
 *   * 校验通过 -> writeStatus 回 WRITE、writeFlag/writeTicker 清 0、返回 OK
 * 实测因此漏掉 24 条变异 (EP030..EP078 里的一片)。
 *
 * 这个相位用 rweeprom_step() 走**两步握手**, 让 RwEeprom 真的干活, 把上面
 * 每一条都驱到。参数按 ReadFuncCode/WriteFuncCode 的契约域取:
 * index 是 eepromIndex (0..994), number <= 2 (PAGE_NUM), number==2 才触发
 * REMAINDER 支。
 */
static void chip_fill(struct I2C_MSG *m, Uint16 c0, Uint16 c1)
{
    m->buffer[0] = (Uint16)((c0 >> 8) & 0xFF);
    m->buffer[1] = (Uint16)(c0 & 0xFF);
    m->buffer[2] = (Uint16)((c1 >> 8) & 0xFF);
    m->buffer[3] = (Uint16)(c1 & 0xFF);
}

/* 把"EEPROM 片内容"回填进两侧 I2cMsg.buffer。
 * wr/wn 是两侧各自正要写进 EEPROM 的两个字; bad != 0 时片内容与要写的值
 * 故意错开 (写后比对必然失败), bad == 0 时片内容就是要写的值 (比对通过)。
 * 两侧各按自己那一份算, 所以谁先把内容写错, 回填立刻把它放大成差异。 */
static void chip_backfill(const Uint16 *wr, const Uint16 *wn, unsigned bad)
{
    Uint16 c0, c1;

    c0 = wr[0]; c1 = wr[1];
    if (bad) { c0 = (Uint16)(c0 ^ 0x0055u); c1 = (Uint16)(c1 ^ 0x00AAu); }
    chip_fill(&ref_I2cMsg, c0, c1);

    c0 = wn[0]; c1 = wn[1];
    if (bad) { c0 = (Uint16)(c0 ^ 0x0055u); c1 = (Uint16)(c1 ^ 0x00AAu); }
    chip_fill(&new_I2cMsg, c0, c1);
}

static void phase_write_handshake(void)
{
    unsigned c;

    for (c = 0; c < 8; c++)
    {
        unsigned t;
        Uint16 c0;
        Uint16 c1;
        unsigned bad = c & 1u;

        case_begin(0x30A1 + c * 4099);

        /* 让 WriteFuncCode 从"干净初态"起步: status 空闲、状态机停在 WRITE。
         *
         * 跨页判据的**活**支是 `#elif 1` 那条 (EEPROM_TYPE==24LC32 -> 上面
         * `#if` 走 0xFFFE/+0x0002 的是**死**支 —— 见 README 1.2 与
         * build/_eeprom_dead_audit.py):
         *     index + number <= (index & 0xFFF8) + 0x0008
         *
         * number 只能取 1 或 2。EEPROM_PAGE_NUM_FUNC_CODE == EEPROM_PAGE_BYTE
         * / FUNC_CODE_BYTE == 4 / 2 == 2 (DSP2803X 那一支, 见
         * include/function/f_eeprom.h 第 9..17 行), 所以 WriteFuncCode 的参数
         * 闸 `number > EEPROM_PAGE_NUM_FUNC_CODE -> FUNCCODE_RW_RET_PARA_ERROR`
         * 一超过 2 就当场返回。初版这里写 `1 + (c % 8)` 正是这个问题: c >= 2
         * 的六个序列每步都在参数闸上返回, 一步都没进状态机 —— 而那六个序列
         * 恰好是后来发现 EP071..EP074 全 MISSED 的直接原因。
         * number == 2 时才走 WRITE_REMAINDER (number 被内部改成 1), 所以 1/2
         * 两格都要扫。 */
        inSide.I2cMsg.status = I2C_MSG_STATUS_IDLE;
        inSide.funcCodeWrite.index  = (Uint16)(16 + c * 8);   /* 页首 */
        inSide.funcCodeWrite.number = (Uint16)(1 + (c % 2));  /* 1 或 2 */
        inSide.funcCodeWrite.data[0] = (Uint16)(0xA000u + c * 7u);
        inSide.funcCodeWrite.data[1] = (Uint16)(0xB000u + c * 11u);

        /* ---- 片内容: 整个相位都在和"校验读回来的是什么"打交道 ----------
         *
         * 偶数 c: 片内容 == funcCodeWrite.data -> 写后比对**通过**, 进到
         *          EP071..EP074 那一格 (writeFlag/writeTicker 清 0,
         *          writeStatus 回 WRITE, 返回 OK)。
         * 奇数 c: 片内容与 funcCodeWrite.data 故意错开 -> 比对**失败**,
         *          bVerifyWrong = 1, 返回 WRITING (EP075 / EP077 那一格)。
         * 两格都必须有确定驱动: 只靠 case_begin 的随机残值时,"比对通过"出现
         * 过几次完全取决于之前那些相位把 ReadFuncCode 的 static 推到哪儿
         * (build/_probe_wfc_verify.py 实测 20000 例: W.succ 0 次, W.fail 7 次)。
         *
         * funcCodeRead.data 也必须装成片内容。ReadFuncCode 只在 readTime == 1
         * 时落盘, readTime >= 2 时是拿 funcCodeRead.data[] 和片内容比 —— 装的
         * 值和片内容不一致就 readTime = 0 + bVerifyWrong = 1, 永远读不到 OK,
         * 后面全在白跑。装成同值之后 readTime 的推进就成了唯一变量, 30 步够。 */
        c0 = inSide.funcCodeWrite.data[0];
        c1 = inSide.funcCodeWrite.data[1];
        if (bad) { c0 = (Uint16)(c0 ^ 0x0055u); c1 = (Uint16)(c1 ^ 0x00AAu); }
        inSide.funcCodeRead.data[0] = c0;
        inSide.funcCodeRead.data[1] = c1;
        chip_fill(&inSide.I2cMsg, c0, c1);

        /* 回退门: ReadFuncCode / WriteFuncCode 各有一个 bVerifyWrong 静态,
         * 跨 case_begin() 保留。前面任何相位只要比对失败过一次, 这两个静态就
         * 停在 1, 于是 `bVerifyWrong && ticker < 100` 让**每一次**调用都立刻
         * 返回 WRITING, verify 支整段不可达。ticker 是模块全局, 序列第一步装
         * 得进去 —— 钉成 > 100 就是把"重试延时已过"这个真实状态喂进去, 不是
         * 短路判据 (变异清单里没有以这个常量为变异点的条目)。 */
        inSide.rwFuncCodeRepeatDelayTicker = 0xFFFF;

        g_argRw    = &ref_funcCodeWrite;
        g_argRwNew = &new_funcCodeWrite;

        seq_begin();     /* 序列第一步: 把 inSide 落进两侧 (step_seq 不装) */

        /* 连推 30 步。30 是算出来的, 不是拍的:
         *   * 状态机三态 WRITE -> READ_TO_VERIFY_WRITE -> (连写时) WRITE_REMAINDER
         *     -> 回到 WRITE, 每态都要 RwEeprom 的一次"两步握手";
         *   * 进到 READ_TO_VERIFY_WRITE 之后, 它内部调 ReadFuncCode, 而
         *     ReadFuncCode 自己要 readTime 1->2->3 三次成功读才返回 OK
         *     (见 ref 第 336..356 行)。每次成功读又是一次 RwEeprom 两步握手。
         * 8 步不够 —— 实测那时一次都没进过 verify 支。30 步留足余量, 且
         * 状态机自身是幂等的, 多推几步不引入新状态。
         *
         * 关键: **不要在每步之前强设 status = RW_OK**。那正是本相位要修的
         * 缺口 —— 强设之后 RwEeprom 永远走 RW_OK 支, 既不写寄存器也不迁移
         * 状态, WriteFuncCode 的 verify 支一次都到不了。 */
        for (t = 0; t < 30; t++)
        {
            /* 真 EEPROM 不在场 —— 每个状态 (WRITE / WRITE_REMAINDER) 都会把
             * I2cMsg.buffer 改写成"要写出去的内容", 而片子里在写完之前一直是
             * 旧值, 所以每步都要把片内容回填回去。回填规则见 chip_backfill()。 */
            if (t > 0)
            {
                chip_backfill(ref_funcCodeWrite.data, new_funcCodeWrite.data,
                              bad);
            }

            rweeprom_step(FN_WRITE, 0 /* RW_EEPROM_WRITE */);
        }
    }

    /* 同一件事, 但换成 ReadFuncCode: 让它的 readTime / readRightTime 计数
     * 在**真的读成功**的前提下走完 1 -> 2 -> 3 -> OK 的全程。 */
    for (c = 0; c < 6; c++)
    {
        unsigned t;

        case_begin(0x30B1 + c * 4099);

        inSide.I2cMsg.status = I2C_MSG_STATUS_IDLE;
        inSide.funcCodeRead.index  = (Uint16)(16 + c * 13);
        inSide.funcCodeRead.number = (Uint16)(1 + (c % 2));
        inSide.funcCodeRead.data[0] = (Uint16)(0xC000u + c * 5u);
        inSide.funcCodeRead.data[1] = (Uint16)(0xD000u + c * 3u);
        /* buffer 是"从 EEPROM 读回来的字节"; 与 data[] 一致才走"全对"那条。
         * 注意 buffer 是 Uint16[] (每项一个字节), 不是 unsigned char[]。 */
        inSide.I2cMsg.buffer[0] = (Uint16)((inSide.funcCodeRead.data[0] >> 8) & 0xFF);
        inSide.I2cMsg.buffer[1] = (Uint16)(inSide.funcCodeRead.data[0] & 0xFF);
        inSide.I2cMsg.buffer[2] = (Uint16)((inSide.funcCodeRead.data[1] >> 8) & 0xFF);
        inSide.I2cMsg.buffer[3] = (Uint16)(inSide.funcCodeRead.data[1] & 0xFF);
        /* 同一个回退门, 见上面第一个循环的说明。 */
        inSide.rwFuncCodeRepeatDelayTicker = 0xFFFF;

        g_argRw    = &ref_funcCodeRead;
        g_argRwNew = &new_funcCodeRead;

        seq_begin();     /* 同上 */

        for (t = 0; t < 12; t++)
        {
            /* buffer 在整条读会话里不会被改写 (ReadFuncCode 只读不写 buffer,
             * 而本相位不驱动 I2cIntDeal), 所以片内容装一次就够, 不用逐步回填。 */
            rweeprom_step(FN_READ, 1 /* RW_EEPROM_READ */);
        }
    }
}

/* 跑 n 步 EepromDeal。
 *
 * `st` 只在**第一步之前**生效 (给状态机一个好起点); 之后每步用
 * rweeprom_step() 走真实握手, **不再强设 RW_OK**。
 *
 * 为什么改: 初版每步之前强设 `I2cMsg.status = RW_OK`, 于是 RwEeprom 永远
 * 走 `RW_OK` 那一支 —— 寄存器不写、状态不迁移。EepromDeal 的状态机因此
 * 停在第一个状态上出不去, 它内部对 ReadFuncCode / WriteFuncCode 的调用
 * 也全都拿不到"成功", 于是 EP087..EP135 那一片 (30 条) 一律 MISSED。
 * 实测: 强设 RW_OK 时 59 条 MISSED, 其中 30 条集中在这一片。
 *
 * 同时, 每步之后把 I2cMsg.buffer 回填成"刚写出去的内容", 模拟真 EEPROM
 * 把数据还回来 —— 否则读回校验必然失败, 状态机卡在错误分支。
 */
/* EepromDeal 序列的"片内容"回填。
 *
 * 片内容要分两种状态, 这是 whole bench 里最容易搞错的一处:
 *
 *   (a) 还没决定写 (funcCodeRwStatus != WRITE_DATA) —— 片里是"当前参数"。
 *       ReadFuncCode 把它读进 funcCodeRead.data[], EepromDeal 拿它和
 *       funcCode.all[] 比, 决定要不要写。回填 funcCodeRead.data 就是
 *       "读回来的和上次一样", READ_CURRENT_DATA 那条链才能走下去。
 *
 *   (b) 已经决定写 (funcCodeRwStatus == WRITE_DATA) —— 这一页已经被
 *       WriteFuncCode 的 WRITE / WRITE_REMAINDER 两个状态写出去了 (它们都会
 *       把要写的字节填进 I2cMsg.buffer), 所以片内容必须回填成
 *       **funcCodeWrite.data**。
 *
 * 原来这里一律回填 funcCodeRead.data, 于是写后校验读回来的永远是**写之前**的
 * 旧值: `funcCodeRead.data[i] != funcCodeRw->data[i]` 恒成立 ->
 * bVerifyWrong = 1 -> WriteFuncCode 永远返回 WRITING ->
 * WRITE_DATA 成功支 (ref 第 716 行那个 UpdateIndexForEepromDeal 调用点,
 * EP135 的变异点) 一次都到不了。
 * build/_probe_wfc_verify.py 实测 20000 例: D.wdata 进 9376 次, D.wok 0 次。
 *
 * 两侧各按自己那一份回填, 所以谁先把内容写错, 回填立刻把它放大成差异。
 */
static void deal_backfill(void)
{
    const Uint16 *rr = (ref_funcCodeRwStatus == DRV_RW_STATUS_WRITE_DATA)
                       ? ref_funcCodeWrite.data : ref_funcCodeRead.data;
    const Uint16 *nn = (new_funcCodeRwStatus == DRV_RW_STATUS_WRITE_DATA)
                       ? new_funcCodeWrite.data : new_funcCodeRead.data;

    chip_fill(&ref_I2cMsg, rr[0], rr[1]);
    chip_fill(&new_I2cMsg, nn[0], nn[1]);
}

static void deal_run(int n, enum I2C_MSG_STATUS st)
{
    int t;
    for (t = 0; t < n; t++)
    {
        if (t == 0)
        {
            inSide.I2cMsg.status = st;
            /* 回退门: bVerifyWrong 是函数级 static, 跨 case_begin() 保留。
             * 前面任何相位比对失败过一次, 它就停在 1, 之后每次调用都在
             * `bVerifyWrong && ticker < 100` 上弹回, 写后校验整段不可达。 */
            inSide.rwFuncCodeRepeatDelayTicker = 0xFFFF;
            seq_begin();        /* 第一步: 把 inSide 落进两侧 */
        }
        rweeprom_step(FN_DEAL, 0 /* 参数被 EepromDeal 忽略 */);

        /* 真 EEPROM 不在场: 按 deal_backfill() 的规则回填片内容, 让读回校验
         * 该过的时候能过。 */
        deal_backfill();
    }
}

/* 6a-pre. 把 ReadFuncCode 的函数级 static **重新推回可复现起点**。
 *
 * `ReadFuncCode` 的 static readTime / readRightTime 跨 case_begin() 保留,
 * 能不能读到 FUNCCODE_READ_RET_OK 取决于"之前所有例"把它推到哪儿 ——
 * 这是相位顺序的隐藏耦合。phase_deal_parainit 里四个 mode 挨着跑, 第一个
 * (SAVE_USER_PARA) 吃掉了刚被 phase_read 推好的静态, 后面三个
 * (RESTORE_USER_PARA / RESTORE_COMPANY_PARA / _ALL) 就再也读不到 OK,
 * 于是 PREPARE_DATA_FOR_WRITE 只被 mode 4 进过 —— EP115 / EP116 / EP130
 * 因此全 MISSED (build/_probe_deal_states.py 实测: 进入 183 次, mode 恒为 4)。
 *
 * 这里在每个 mode 之前先用 READ_ALL 空转若干步: READ_ALL 一路走
 *   PREPARE_EAD -> 扫描 break -> READ_CURRENT_DATA -> ReadFuncCode
 * 是最短的重置路径, 它自己就会把 readTime 从 0 推到 1 再推 readRightTime 到 3。
 * 只推进 index, 不动被测判据, 所以不会把任何一格"喂"成恒真。
 */
static void deal_prime_read(int n)
{
    int t;
    for (t = 0; t < n; t++)
    {
        if (t == 0)
        {
            inSide.funcCodeRwMode      = FUNCCODE_RW_MODE_NO_OPERATION;
            inSide.funcCodeRwModeTmp   = FUNCCODE_RW_MODE_READ_ALL;
            inSide.funcCodeOneWriteNum = 0;
            inSide.endIndexRwFuncCode  = 994;
            /* 入口状态必须钉死。funcCodeRwStatus 是 side_begin() 里
             * `rnd() % 4` 随机出来的, 而 EepromDeal 的整个
             * PREPARE_DATA_FOR_EAD 块由 `if (PREPARE_DATA_FOR_EAD ==
             * funcCodeRwStatus)` 把关 —— 不钉就等于 3/4 的概率整块跳过,
             * "重置 ReadFuncCode 静态"这件事只在 1/4 的例子上发生,
             * 后面每个 mode 能不能读到 OK 全看运气。
             * build/_probe_ep_v5.py 实测: 20000 例里 EAD 块只进了 33 次,
             * 且 mode 只出现 {13, 4} —— phase_deal_allrsvd 的 WRITE_SERIES
             * 一次都没进去, EP108/EP109/EP111 因此全 MISSED。
             * funcCodeRwStatus 是模块全局, driver 本来就装得进去
             * (install_side 每次序列第一步把它落进两侧), 钉 0 不是短路判据,
             * 是让这条相位真的走到它注释里写的那条路上。 */
            inSide.funcCodeRwStatus   = DRV_RW_STATUS_PREPARE_DATA_FOR_EAD;
            inW.powerOnStatus          = 2;
            inSide.I2cMsg.status       = I2C_MSG_STATUS_RW_OK;
            inSide.rwFuncCodeRepeatDelayTicker = 0xFFFF;
            seq_begin();
        }
        rweeprom_step(FN_DEAL, 0);
        deal_backfill();
    }
}

/* 6a. EepromDeal: paraInit 四种 mode 的**首次覆盖**, 必须跑在其它 deal 相位之前。
 *
 * 为什么单独一个相位、为什么必须靠前:
 *
 *   `ReadFuncCode` 内部有函数级 static
 *
 *       static Uint16 readTime;
 *       static Uint16 readRightTime;
 *
 *   它要连续读回**一致**的数据 `readRightTime` 次 (阈值 3; 写后校验是 2) 才返
 *   FUNCCODE_READ_RET_OK, 而 `readRightTime` 只在 `readTime == 1` 那一次被置回 1,
 *   其余时候只增不减。这些状态**跨 case_begin() 保留**, 所以"这一例能不能读到
 *   OK"取决于**之前所有例**把它推到了哪儿。
 *
 *   实测 (build/_probe_deal_path.py, 对 ref 侧插 fprintf 数命中):
 *
 *       PREPARE_DATA_FOR_WRITE 进入 182 次, 但 mode 只出现过 {12,13,10,11,2};
 *       READ_CURRENT_DATA 的进出门只出现在 mode {12,13,2}。
 *       mode 1/3/4/501 **一次都没有走到 READ_CURRENT_DATA** ——
 *       因为它们在相位顺序上排在 WRITE_ONE / READ_ALL 之后, 轮到它们时
 *       ReadFuncCode 的 static 已经处在"再也读不到 OK"的状态上。
 *
 *   直接后果: EP115..EP122 / EP124..EP127 / EP129 / EP130 全 MISSED。
 *
 * 所以把这四段**提前**到 phase_read 之后立刻跑 (此时 ReadFuncCode 的 static 刚
 * 被 phase_read 的序列推到一个可复现的起点), 每例给足步数。
 */
static void phase_deal_parainit(void)
{
    static const Uint16 umodes[] = {
        FUNCCODE_paraInitMode_SAVE_USER_PARA,          /* 4   */
        FUNCCODE_paraInitMode_RESTORE_USER_PARA,       /* 501 */
        FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA,    /* 1   */
        FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA_ALL /* 3   */
    };
    /* index 起步与扫描窗口: index = EEPROM_INDEX_USE_INDEX(4),
     * number = (4 & 0xFFFE) + 8 - 4 = 8, 扫 eepromIndex 4..11 —— 全非 RSVD,
     * 于是必然 break 在 i=0 -> READ_CURRENT_DATA -> (读 OK) -> PREPARE_WRITE。 */
    static const Uint16 ends[] = { 4, 12, 20, 994 };
    unsigned k, e;

    for (k = 0; k < sizeof umodes / sizeof umodes[0]; k++)
    {
        for (e = 0; e < sizeof ends / sizeof ends[0]; e++)
        {
            case_begin(0x30A1 + k * 256 + e * 16);
            deal_prime_read(28);
            inSide.funcCodeRwMode    = FUNCCODE_RW_MODE_NO_OPERATION;
            inSide.funcCodeRwModeTmp = umodes[k];
            inSide.funcCodeOneWriteNum = 0;
            inSide.endIndexRwFuncCode  = ends[e];
            /* 同上: 入口状态钉在 PREPARE_DATA_FOR_EAD。modes 里 501
             * (RESTORE_USER_PARA) 要一路走
             *   EAD -> 扫描 break -> READ_CURRENT_DATA -> ReadFuncCode OK
             *   -> PREPARE_DATA_FOR_WRITE -> 用户参数块
             * 才够得着 EP115 / EP116 / EP130。入口状态是随机值时这段路
             * 只有 1/4 的概率起头。 */
            inSide.funcCodeRwStatus = DRV_RW_STATUS_PREPARE_DATA_FOR_EAD;
            inW.aiaoChckReadOK = (Uint16)((k + e) & 1);
            inW.powerOnStatus  = 2;
            deal_run(60, I2C_MSG_STATUS_RW_OK);
        }
    }
}

/* 6a'. EepromDeal: 让 RSVD 扫描**走完全程** (i == number4EepromDeal)。
 *
 *   `GetFcIndexFromEepromIndex` 的反查表 (本用例走真头文件
 *   f_table_eeprom2Fc.c 的 eeprom2Fc[]) 里 FUNCCODE_RSVD4ALL_INDEX(=909) 的
 *   第一段 **8 个连续** 落在 eepromIndex 564..571。
 *
 *   扫描的窗口由 PREPARE_DATA_FOR_EAD 的页对齐给出:
 *
 *       number4EepromDeal = ((index & 0xFFFE) + 8) - index
 *
 *   index = 564 (偶数) => number = 8, 正好 564..571 全是 RSVD, 于是循环跑满
 *   `i == number4EepromDeal`, 走 UpdateIndexForEepromDeal()。
 *   endIndexRwFuncCode 只要 >= 572 就不会把 number 夹小。
 *
 *   eepromIndex 564 由 **fcIndex 600** 映射而来 —— 注意这里要按**本用例的替身
 *   属性表** funcCodeAttribute[] 算 (eeprom_tables.inc 的生成布局), 不能照
 *   真头文件的 eeprom2Fc[] 反推, 两张表的下标对应关系不同:
 *   真表 eeprom2Fc[840]=563, 而替身表里得到 eepromIndex 563 的是 fcIndex 470。
 *   替身表里 fcIndex 600 -> eepromIndex 564 (见 eeprom_tables.inc 第 600 行)。
 *
 *   不覆盖这一格时 EP107 / EP108 / EP109 / EP111 全 MISSED
 *   (旧相位只扫到"第一格非 RSVD 就 break", 扫描体从没跑完过)。
 */
static void phase_deal_allrsvd(void)
{
    /* WRITE_SERIES 把一串"非默认"的 start 送进 EAD 块: start 既是 fcIndex
     * 也是 eepromIndex (fcIndex < 909 时正向映射恒等 —— 见 phase_deal_edges
     * (iii) 对替身表第 5 列才是 eepromIndex 的说明)。
     *
     * 名字里的 "allrsvd" 是历史包袱: 这一相位原本想让 RSVD 扫描走完全程
     * (i == number), 但那条支路在**替身表上不可达** —— eeprom2Fc[] 里没有
     * 一个 1082 (= FUNCCODE_RSVD4ALL_INDEX)。实测 1456 次扫描全部 i=0 就
     * break。"i == number" 现在靠 number4EepromDeal == 0 到达 (环体一次都
     * 不进), 那一格在 phase_deal_edges (ii) 的 endIndex = 0 里, EP111 就是
     * 在那儿被抓到的。这里保留它, 因为它仍然提供**别的**相位不给的组合:
     * 奇/偶 start、end 紧随其后、40 步长跑 —— 让 EAD 块的 number 计算与
     * UpdateIndexForEepromDeal 的"越过 endIndex 就结束 mode"分支真的被走到。
     */
    static const Uint16 starts[] = { 600, 598, 599, 601 };
    unsigned k;

    for (k = 0; k < sizeof starts / sizeof starts[0]; k++)
    {
        case_begin(0x30B1 + k * 128);
        inSide.funcCodeRwMode    = FUNCCODE_RW_MODE_NO_OPERATION;
        inSide.funcCodeRwModeTmp = FUNCCODE_RW_MODE_WRITE_SERIES;
        inSide.funcCodeOneWriteNum = 0;
        inSide.startIndexWriteSeries = starts[k];
        inSide.endIndexWriteSeries   = (Uint16)(starts[k] + 40);
        inSide.endIndexRwFuncCode    = 994;
        /* 入口状态钉在 PREPARE_DATA_FOR_EAD —— 否则整个 RSVD 扫描块
         * 3/4 的概率被跳过, 这个相位就等于没跑 (见 deal_prime_read 的说明:
         * 实测 EAD 块 20000 例只进 33 次, WRITE_SERIES 一次都没进)。 */
        inSide.funcCodeRwStatus   = DRV_RW_STATUS_PREPARE_DATA_FOR_EAD;
        inW.powerOnStatus = 2;
        deal_run(40, I2C_MSG_STATUS_RW_OK);
    }
}

static void phase_deal_modes(void)
{
    /* 模式清单: 连同"各种 index/endIndex 组合"一起跑。 */
    static const Uint16 modes[] = {
        FUNCCODE_RW_MODE_WRITE_ALL,                    /* 10 */
        FUNCCODE_RW_MODE_WRITE_SERIES,                 /* 11 */
        FUNCCODE_RW_MODE_WRITE_ONE,                    /* 12 */
        FUNCCODE_RW_MODE_READ_ALL,                     /* 13 */
        FUNCCODE_RW_MODE_READ_SERIES,                  /* 14 */
        FUNCCODE_RW_MODE_READ_ONE,                     /* 15 */
        FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA,       /* 1 */
        FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA_ALL,   /* 3 */
        FUNCCODE_paraInitMode_SAVE_USER_PARA,             /* 4 */
        FUNCCODE_paraInitMode_RESTORE_USER_PARA,          /* 501 */
        2,                                             /* CLEAR_RECORD */
        9999                                           /* 未知 mode */
    };
    unsigned m;

    for (m = 0; m < sizeof modes / sizeof modes[0]; m++)
    {
        unsigned k;

        /* (a) 从 funcCodeRwModeTmp 起: 走"设模式"那一支。 */
        for (k = 0; k < 3; k++)
        {
            case_begin(0x3061 + m * 512 + k * 32);
            inSide.funcCodeRwMode    = FUNCCODE_RW_MODE_NO_OPERATION;
            inSide.funcCodeRwModeTmp = modes[m];
            inSide.funcCodeOneWriteNum = 0;
            inSide.startIndexWriteSeries = (Uint16)(k * 100);
            inSide.endIndexWriteSeries   = (Uint16)(k * 100 + 3);
            inSide.endIndexRwFuncCode    = (Uint16)(k * 50 + 8);
            /* 入口状态钉 PREPARE_DATA_FOR_EAD: 12 个 mode 都要从
             * "设模式 -> EAD -> 扫描 -> 读 -> 备写 -> 写" 这条主干上过一遍,
             * 入口状态随机时 3/4 的 mode 组合连 EAD 块都进不去
             * (build/_probe_ep_v5.py 实测: 20000 例只进 33 次)。 */
            inSide.funcCodeRwStatus = DRV_RW_STATUS_PREPARE_DATA_FOR_EAD;
            inW.aiaoChckReadOK = (Uint16)(k & 1);
            inW.powerOnStatus  = (Uint16)(k == 0 ? 0 /* POWER_ON_WAIT */ : 2);
            deal_run(24, I2C_MSG_STATUS_RW_OK);
        }

        /* (b) 从"写队列"起: funcCodeOneWriteNum 非零那一支。 */
        for (k = 0; k < 3; k++)
        {
            unsigned q;
            case_begin(0x3061 + m * 512 + 0x100 + k * 32);
            inSide.funcCodeRwMode    = FUNCCODE_RW_MODE_NO_OPERATION;
            inSide.funcCodeRwModeTmp = 0;
            inSide.funcCodeOneWriteNum = (Uint16)(1 + k * 2);
            for (q = 0; q < 200; q++)
            {
                inSide.funcCodeOneWriteIndex[q] = (Uint16)(q * 4 + k);
            }
            /* 写队列支**不碰** endIndexRwFuncCode (模块只在"设模式"支里赋它),
             * 所以这里装的就是模块这一路看到的那个值。index 由
             * GetEepromIndexFromFcIndex 映射来, 最大 ~916 —— endIndex 必须
             * 比它大, 否则 number = tmp - index 回绕 (见 side_begin 的说明)。
             * 钉 994 是模块默认值, 也是真固件的取值。 */
            inSide.endIndexRwFuncCode  = EEPROM_INDEX_USE_LENGTH;
            /* 同 (a): 钉住入口状态, 让 WRITE_ONE 也走完整条主干。 */
            inSide.funcCodeRwStatus = DRV_RW_STATUS_PREPARE_DATA_FOR_EAD;
            deal_run(24, I2C_MSG_STATUS_RW_OK);
        }
    }
}

/* 6b. EepromDeal 的几处**边界**单独钉。
 *
 *   (i)   funcCodeOneWriteNum >= (200 >> 1) = 100 的夹取, 以及它是否写 errorEeprom
 *         —— 两格: powerOnStatus == POWER_ON_WAIT(0) 与 != 0。
 *   (ii)  PREPARE_DATA_FOR_EAD 里 `tmp = (index & 0xFFF8) + 2; if (tmp > endIndex)
 *         tmp = endIndex; number = tmp - index;` 的两侧:
 *         不夹 (tmp <= endIndex) / 夹 (tmp > endIndex)。index 取页边界值。
 *   (iii) 扫描循环 `for (i = 0; i < number4EepromDeal; i++)` 的全命中 / 全不命中:
 *         GetFcIndexFromEepromIndex 对 eepromIndex >= 909 返回 FUNCCODE_RSVD4ALL_INDEX
 *         (= GetCodeIndex(funcCode.code.rsvd4All))。所以把 index4EepromDeal 设在
 *         909..993 之间就能让整段扫出 RSVD -> 走 UpdateIndexForEepromDeal。
 *   (iv)  WRITE_ALL 的 AI/AO 标定例外 (aiaoChckReadOK == AIAO_CHK_READ_OK=1)
 *         与 SAVE_USER_PARA_PARA1/PARA2 例外。
 *   (v)   `funcCodeWrite.data[i] != funcCodeRead.data[i]` 决定 status 的走向 ——
 *         两种后果各来一次。
 */
static void phase_deal_edges(void)
{
    unsigned k;

    /* (i) 写队列夹取 */
    for (k = 0; k < 4; k++)
    {
        case_begin(0x3071 + k * 64);
        inSide.funcCodeRwMode    = FUNCCODE_RW_MODE_NO_OPERATION;
        inSide.funcCodeRwModeTmp = 0;
        inSide.funcCodeOneWriteNum = (Uint16)(98 + k * 2);  /* 98,100,102,104 */
        inW.powerOnStatus = (Uint16)(k & 1 ? 0 : 2);        /* WAIT / 非 WAIT */
        inSide.I2cMsg.status = I2C_MSG_STATUS_RW_OK;
        g_cur = "deal-onewrite-clamp";
        step(FN_DEAL);
    }

    /* (ii) 页对齐夹取: 两步序列。
     *
     *      第一步 (deal-pagealign-primer): mode = NO_OPERATION + modeTmp =
     *      READ_ALL, 让模块自己走"设模式"那一支 —— 它会把 index4EepromDeal
     *      落到 **0** (READ_ALL 支), 同时把 endIndexRwFuncCode 覆写成
     *      EEPROM_INDEX_USE_LENGTH。到这一步为止, 模块的 index 是一个**已知
     *      的小值**, 不再是上一个相位留下的残留。
     *
     *      第二步 (deal-pagealign): 装小 endIndexRwFuncCode 再走一次。
     *      此时 index == 0, `number4EepromDeal = tmp - index` 不会回绕,
     *      而 `tmp = (0 & 0xFFF8) + 8 = 8` 与 endIndex = 0/1/2/3 一比就
     *      落进夹取支 —— EP102 要的就是那一格。
     *
     *      为什么必须两步: index4EepromDeal 是函数级 static, driver 装不进
     *      去, 所以"装一个小 endIndex"与"index 还是上一步的大值"这两个事实
     *      会撞上, number 回绕成 ~65000 -> eeprom2Fc[] 表外读 -> 段错误
     *      (2026-10-12 funcCodeRwStatus 补装进 SIDE_INSTALL 后实测 rc=139)。
     *      funcCodeRwStatus 是模块全局, 装得进去 —— 直接设成 0 (PREPARE)。 */
    {
        static const Uint16 ends[] = { 0, 1, 2, 3, 8, 16, 993, 994 };
        unsigned b;
        for (b = 0; b < sizeof ends / sizeof ends[0]; b++)
        {
            /* 第一步: 让模块自己把 index 落到 0 */
            case_begin(0x3071 + 0x100 + b * 32);
            inSide.funcCodeRwMode      = FUNCCODE_RW_MODE_NO_OPERATION;
            inSide.funcCodeRwModeTmp   = FUNCCODE_RW_MODE_READ_ALL;
            inSide.funcCodeOneWriteNum = 0;
            inSide.endIndexRwFuncCode  = EEPROM_INDEX_USE_LENGTH;
            inSide.funcCodeRwStatus    = DRV_RW_STATUS_PREPARE_DATA_FOR_EAD;
            inSide.I2cMsg.status = I2C_MSG_STATUS_RW_OK;
            g_cur = "deal-pagealign-primer";
            step(FN_DEAL);

            /* 第二步: index 已是 0, 装小 endIndex 测夹取支 */
            inSide.funcCodeRwMode      = FUNCCODE_RW_MODE_READ_ALL;
            inSide.funcCodeRwModeTmp   = 0;
            inSide.funcCodeOneWriteNum = 0;
            inSide.endIndexRwFuncCode  = ends[b];
            inSide.funcCodeRwStatus    = DRV_RW_STATUS_PREPARE_DATA_FOR_EAD;
            g_cur = "deal-pagealign";
            step(FN_DEAL);
        }
    }

    /* (iii) RSVD 扫描环: 让 index4EepromDeal 遍历一串**奇偶混合**的值。
     *
     *      原来这里写的是"用 startIndexWriteSeries = 900+k*4 把 index 送进
     *      909..993 的空洞, 好让整段扫出 RSVD"。那个前提是错的, 而且错在
     *      两处:
     *
     *      (1) 替身属性表的结构体是
     *              { lower, upper, init, attribute, eepromIndex }
     *          **eepromIndex 是第 5 列**, 不是第 2 列 (第 2 列是 upper)。
     *          所以 fcIndex 0..908 的正向映射是**恒等**的:
     *              GetEepromIndexFromFcIndex(i) == i          (i < 909)
     *              fcNoAttri2Eeprom[i - 909] == 4..11          (909..916)
     *          (build/_probe_ep_map.py 实测: FWD 0..8 -> 0..8,
     *           600 -> 600, 640 -> 640, 899 -> 899, 909 -> 4, 916 -> 11)。
     *          它只在 909 那一跳上不单调 —— 而旧版正是踩在这一跳上:
     *          start=908 给出 index=908, 而 end=910 走无属性表给出 5,
     *          endIndex = 6 < index -> number = 6 - 908 **无符号回绕**成
     *          65034, eeprom2Fc[908+i] 一路读到表外。两侧是两张不同的表,
     *          表外读到的是不同垃圾 -> 可能报**假差异** (见函数头 a >= 995
     *          的警告)。这就是本相位要修掉的东西。
     *      (2) "扫出 RSVD" 从来不会发生。FUNCCODE_RSVD4ALL_INDEX
     *          = GetCodeIndex(funcCode.code.rsvd4All) = **1082** (编译期
     *          常量, 只取决于 funcCode 联合体布局; build/_probe_ep_scan.py
     *          实测打出 rsvd=1082), 而 eeprom2Fc[] 994 项里**没有一个 1082**
     *          (与原工程 bpqym 的表逐项相同, 已对账)。实测 20000 例:
     *          1456 次扫描全部 i=0 就 break。所以 EP108/EP109 是**语义等价**
     *          变异而不是覆盖缺口 (证词见 negative_control.py)。
     *          "i == number4EepromDeal" 那一支仍然可达, 入口是
     *          **number4EepromDeal == 0** (环体一次都不进, i 停在 0):
     *          number = tmp - index 在 tmp == index 时为 0 —— 即
     *          phase_deal_edges (ii) 里 endIndex = 0 那一格。两格合起来
     *          才算覆盖全 (EP111 走的就是那条)。
     *
     *      于是这一相位真正该干的事变成: 把 index4EepromDeal 遍历到一串
     *      奇偶不同的值上 (奇数 -> number = 1, 偶数 -> number = 2), 让
     *      `tmp = (index & 0xFFFE) + 2` 与 `number = tmp - index` 两侧都有
     *      实例, 并且换着 fcIndex 走 GetFcIndexFromEepromIndex(index + i)。
     *
     *      不变量: index4EepromDeal < endIndexRwFuncCode。取 end = 899
     *      (正向恒等, fwd(899) = 899) -> endIndexRwFuncCode = 900, 于是任何
     *      start <= 899 都合法, number 不会回绕。逐行核算:
     *                    start   index   tmp   number
     *                      0        0      2       2
     *                      1        1      2       1
     *                      2        2      4       2
     *                      3        3      4       1
     *                      4        4      6       2
     *                      5        5      6       1
     *                      6        6      8       2
     *                      7        7      8       1
     *                      8        8     10       2
     *                    600      600    602       2
     *                    640      640    642       2
     *                    899      899    900       1
     */
    {
        static const Uint16 starts[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 600, 640, 899 };
        unsigned s;

        for (s = 0; s < sizeof starts / sizeof starts[0]; s++)
        {
            case_begin(0x3071 + 0x400 + s * 64);
            inSide.funcCodeRwMode    = FUNCCODE_RW_MODE_NO_OPERATION;
            inSide.funcCodeRwModeTmp = FUNCCODE_RW_MODE_WRITE_SERIES;
            inSide.funcCodeOneWriteNum = 0;
            inSide.startIndexWriteSeries = starts[s];
            /* end = 899: fcIndex < 909 时正向映射是**恒等**的, 所以
             * endIndexRwFuncCode = 900。任何 start <= 899 都满足
             * index4EepromDeal (= start) < 900, number 不回绕。 */
            inSide.endIndexWriteSeries   = 899;
            inSide.endIndexRwFuncCode    = EEPROM_INDEX_USE_LENGTH;
            /* 入口状态钉 PREPARE_DATA_FOR_EAD, 否则扫描块 3/4 概率整块跳过。 */
            inSide.funcCodeRwStatus      = DRV_RW_STATUS_PREPARE_DATA_FOR_EAD;
            deal_run(20, I2C_MSG_STATUS_RW_OK);
        }
    }

    /* (iv) WRITE_ALL 的 AI/AO 标定与用户参数例外。aiaoChckReadOK = 1 时,
     *      AI1..AO1 区间内的 fcIndex **不**被写; 为 0 时才写。 */
    for (k = 0; k < 2; k++)
    {
        case_begin(0x3071 + 0x600 + k * 64);
        inSide.funcCodeRwMode    = FUNCCODE_RW_MODE_NO_OPERATION;
        inSide.funcCodeRwModeTmp = FUNCCODE_RW_MODE_WRITE_ALL;
        inSide.funcCodeOneWriteNum = 0;
        inSide.funcCodeRwStatus  = DRV_RW_STATUS_PREPARE_DATA_FOR_EAD;
        inW.aiaoChckReadOK = (Uint16)k;
        deal_run(20, I2C_MSG_STATUS_RW_OK);
    }

    /* (v) funcCodeWrite.data[i] != funcCodeRead.data[i] 两种后果。
     *     READ_ALL 会把 funcCode.all[fcIndex] 设成 funcCodeRead.data[i], 于是
     *     funcCodeWrite.data[i] 与 funcCodeRead.data[i] 必然相等 (因为紧接着
     *     `funcCodeWrite.data[i] = funcCode.all[fcIndex]`), -> 不置 WRITE_DATA。
     *     要让它们不等, 得用 RESTORE_COMPANY_PARA 那一支 (它把 all 设成
     *     GetFuncCodeInit 的值)。两条都跑。 */
    for (k = 0; k < 2; k++)
    {
        case_begin(0x3071 + 0x800 + k * 64);
        inSide.funcCodeRwMode    = FUNCCODE_RW_MODE_NO_OPERATION;
        inSide.funcCodeRwModeTmp = (Uint16)(k ? FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA
                                              : FUNCCODE_RW_MODE_READ_ALL);
        inSide.funcCodeOneWriteNum = 0;
        inSide.funcCodeRwStatus  = DRV_RW_STATUS_PREPARE_DATA_FOR_EAD;
        deal_run(24, I2C_MSG_STATUS_RW_OK);
    }

    /* (vi) PREPARE_DATA_FOR_WRITE 里 SAVE_USER_PARA / RESTORE_USER_PARA /
     *      RESTORE_COMPANY_PARA(_ALL) 四段。
     *
     *      这一段**必须**在相位最前面跑 —— 见 phase_deal_parainit() 的说明。
     *      留在这里的只是"再来一遍"的保险, 不再承担首次覆盖。 */
}

/* 6c. PREPARE_DATA_FOR_WRITE 里 RESTORE_USER_PARA 臂的**取数源** (EP130)。
 *
 *      那一臂是
 *
 *          else if (RESTORE_USER_PARA == funcCodeRwMode)
 *              funcCode.all[fcIndex] = funcCodeWrite.data[i];      <-- EP130
 *          funcCodeWrite.data[i] = funcCode.all[fcIndex];
 *          if (funcCodeWrite.data[i] != funcCodeRead.data[i])
 *              funcCodeRwStatus = FUNCCODE_RW_STATUS_WRITE_DATA;
 *
 *      变异把它改成 `funcCodeRead.data[i]`。要抓到, 必须让两个 data[] 在
 *      这一拍**不相等** —— 相等时第 690 行那个 `!=` 恒假, 两侧连 funcCodeRwStatus
 *      都一模一样。
 *
 *      为什么 deal_run() 永远喂出相等: 它每一步都把 buffer 回填成
 *      encode(funcCodeRead.data[]), 于是"用户参数块的读会话"读回来的
 *      funcCodeWrite.data 恒等于 funcCodeRead.data。而且这两件事发生在
 *      **同一次** EepromDeal 调用里 (第 609 行把 status 置成 PREPARE,
 *      第 615 行的块紧跟着就进, 中间 I2cIntDeal 只在函数开头跑过一次),
 *      所以在"READ_CURRENT_DATA -> PREPARE"这条正常链上, 相等是被
 *      **结构性保证**的 —— 不是覆盖不够, 是那条路上不可能不等。
 *
 *      破法是换一条同样真实的路: **直接以 PREPARE_DATA_FOR_WRITE 状态进
 *      EepromDeal**。这个入口状态是模块自己写出来的 (第 611 行), 真固件
 *      里"上一个周期刚把读会话收尾"就是这样; 而此时 I2cMsg.buffer 已经是
 *      中断后来灌进来的**新**内容, 与 funcCodeRead.data 里那份旧值不同。
 *
 *      实现: 第一趟走正常链 (把 mode / index / number 和读会话的 static 都
 *      摆好), 之后反复"装状态 + 三步握手"。两个细节不能省:
 *
 *        (a) readTime 是 ReadFuncCode 的函数级 static, 第一趟收尾时停在 1
 *            (用户参数块刚做过 0->1 的填充)。第二趟第一次调用因此**不会**
 *            再填充, 而是拿旧值与新 buffer 比对 -> 不等 -> readTime 归 0、
 *            bVerifyWrong 置 1, 直接 return, 第 635 行的环压根没进。
 *            所以要多来几轮, 让 readTime 重新 0->1 填上一次。
 *        (b) bVerifyWrong 置 1 之后, ReadFuncCode 开头
 *                if (bVerifyWrong && (rwFuncCodeRepeatDelayTicker < 100))
 *                    return FUNCCODE_READ_RET_READING;
 *            会一直早退 (rwFuncCodeRepeatDelayTicker 由 EepromOperateTimeDeal
 *            递增, 这里不调它)。所以每一轮都把它装成 >= 100, 把这条退避闸
 *            绕开 —— 轮次之间本来就是独立的 install_side, 装多少都合法。
 */
static void phase_deal_userpara(void)
{
    unsigned k;
    unsigned round;

    for (k = 0; k < 2; k++)
    {
        case_begin(0x30B1 + 0x700 + k * 16);

        /* ---- 全部钉死, 不靠 rnd(): 这个相位的全部意义就是"两侧看到同一份
         *      buffer, 而 buffer 在序列中间换过一次"。任何随机化都会把
         *      换没换这件事变成碰运气。 */
        inSide.funcCodeRwMode      = FUNCCODE_RW_MODE_NO_OPERATION;
        inSide.funcCodeRwModeTmp   = FUNCCODE_paraInitMode_RESTORE_USER_PARA;
        inSide.funcCodeRwStatus    = DRV_RW_STATUS_PREPARE_DATA_FOR_EAD;
        inSide.funcCodeOneWriteNum = 0;
        inSide.endIndexRwFuncCode  = EEPROM_INDEX_USE_LENGTH;
        inSide.rwFuncCodeRepeatDelayTicker = 0xFFFF;   /* >= DELAY_TIME_MAX(100) */

        inSide.I2cMsg.status = I2C_MSG_STATUS_RW_OK;
        inSide.I2cMsg.bytes  = 4;
        inSide.I2cMsg.highAddr = (Uint16)(0x1000u + k);
        inSide.I2cMsg.lowAddr  = (Uint16)(0x2000u + k);
        /* buffer "第一份": 0x0102 / 0x0304 */
        inSide.I2cMsg.buffer[0] = 0x01;
        inSide.I2cMsg.buffer[1] = 0x02;
        inSide.I2cMsg.buffer[2] = 0x03;
        inSide.I2cMsg.buffer[3] = 0x04;

        inSide.funcCodeRead.index  = 4;
        inSide.funcCodeRead.number = 2;
        inSide.funcCodeRead.data[0] = 0x0102;
        inSide.funcCodeRead.data[1] = 0x0304;

        inSide.funcCodeWrite.index  = 4;
        inSide.funcCodeWrite.number = 2;
        inSide.funcCodeWrite.data[0] = 0x0102;
        inSide.funcCodeWrite.data[1] = 0x0304;

        /* I2C 寄存器: BB 必须为 0, 否则 RwI2cBus 一路返 BUS_BUSY,
         * 状态机每步都在兜圈子, 读会话永远收不了尾。 */
        inW.i2c.I2CSTR.bit.BB   = 0;
        inW.i2c.I2CMDR.bit.STP  = 0;
        inW.i2c.I2CISRC.all     = 6;             /* I2C_SCD_ISRC */
        inW.i2c.I2CDRR          = (Uint16)(0xABCDu - k);

        /* ---- 第一趟: 走"设模式 -> EAD -> RSVD 扫描 -> 读当前数据 -> 备写"
         *      这条正常链。它把 mode / index4EepromDeal / number4EepromDeal
         *      摆好, 并把读会话的 readTime / readRightTime 推到 1。
         *      这一趟里用户参数块也会执行一次, 但 buffer 还是第一份,
         *      两个 data[] 必然相等 —— 那正是"无差异"那一格, EP130 观测不到。 */
        g_cur = "deal-userpara-first";
        seq_begin();
        rweeprom_step(FN_DEAL, 0);

        /* ---- 第二趟起: buffer 换成"中断后来灌进来的第二份", 状态直接装成
         *      PREPARE_DATA_FOR_WRITE, 反复握手直到用户参数块的那个读会话
         *      自己走完 (readTime 0->1 填一次, 再 1->2、2->3 各比对一次)。
         *      6 轮是余量: 实测 3 轮就够 (第 1 轮撞 bVerifyWrong 早退,
         *      第 2 轮填充, 第 3 轮收尾进第 635 行的环)。 */
        inSide.I2cMsg.buffer[0] = 0xAB;          /* 第二份: 0xABCD / 0xABCD */
        inSide.I2cMsg.buffer[1] = 0xCD;
        inSide.I2cMsg.buffer[2] = 0xAB;
        inSide.I2cMsg.buffer[3] = 0xCD;

        for (round = 0; round < 6; round++)
        {
            inSide.funcCodeRwMode    = FUNCCODE_paraInitMode_RESTORE_USER_PARA;
            inSide.funcCodeRwModeTmp = 0;
            inSide.funcCodeRwStatus  = DRV_RW_STATUS_PREPARE_DATA_FOR_WRITE;
            inSide.I2cMsg.status     = I2C_MSG_STATUS_RW_OK;
            inSide.rwFuncCodeRepeatDelayTicker = 0xFFFF;
            inSide.funcCodeRead.data[0] = 0x0102;
            inSide.funcCodeRead.data[1] = 0x0304;
            install_side(0);
            install_side(1);
            g_cur = "deal-userpara-swap";
            rweeprom_step(FN_DEAL, 0);
        }
    }
}


/* 7. UpdateIndexForEepromDeal —— 直接驱动。
 *
 * 它是 LOCALF, 不导出, 但 ref_tu.c 的符号表里有它 (nm2000 读到
 * `_UpdateIndexForEepromDeal` 是 T), 所以两侧都能链到。driver 直接调它,
 * 把"推进 index / 结束模式 / 写 saveUserParaFlag"这三件事单独钉死。
 *
 *   index4EepromDeal += number4EepromDeal
 *   if (index4EepromDeal >= endIndexRwFuncCode) {
 *       RESTORE_COMPANY_PARA / _ALL -> RestoreCompanyParaOtherDeal()
 *       SAVE_USER_PARA -> funcCode.code.saveUserParaFlag1 = FLAG1;
 *                         funcCode.code.saveUserParaFlag2 = FLAG2;
 *       funcCodeRwMode = NO_OPERATION; funcCodeRwModeTmp = NO_OPERATION;
 *   }
 *   funcCodeRwStatus = FUNC_CODE_RW_STATUS_INIT
 *   若 funcCodeRwMode == WRITE_ONE -> funcCodeRwMode = NO_OPERATION (不碰 index!)
 *
 * 注意最后的 `if WRITE_ONE` 分支在**最前**执行 (函数体顺序: status 先置 INIT,
 * 然后判断 WRITE_ONE 早退, 否则才 += 与结束判定)。这个顺序弄反了就是一个典型
 * 的"看起来对"的改写 —— 所以三种 mode 各跑一遍。
 */
static void phase_update_index(void)
{
    static const Uint16 modes[] = {
        FUNCCODE_RW_MODE_WRITE_ONE,
        FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA,
        FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA_ALL,
        FUNCCODE_paraInitMode_SAVE_USER_PARA,
        FUNCCODE_RW_MODE_READ_ALL,
        FUNCCODE_RW_MODE_WRITE_ALL
    };
    static const Uint16 idxs[]  = { 0, 1, 992, 993, 994, 1000 };
    static const Uint16 nums[]  = { 0, 1, 2, 8 };
    static const Uint16 ends[]  = { 0, 1, 994, 995, 2000 };
    unsigned m, a, b, e;

    for (m = 0; m < sizeof modes / sizeof modes[0]; m++)
    {
        for (a = 0; a < sizeof idxs / sizeof idxs[0]; a++)
        {
            for (b = 0; b < sizeof nums / sizeof nums[0]; b++)
            {
                for (e = 0; e < sizeof ends / sizeof ends[0]; e++)
                {
                    case_begin(0x3081 + m * 4096 + a * 256 + b * 64 + e * 4);
                    inSide.funcCodeRwMode      = modes[m];
                    inSide.funcCodeRwModeTmp   = (Uint16)(rnd() & 1);
                    inSide.funcCodeRwStatus    = (Uint16)(rnd() % 4);
                    inSide.funcCodeRead.index  = idxs[a];
                    inSide.funcCodeRead.number = nums[b];
                    inSide.endIndexRwFuncCode  = ends[e];
                    inSide.startIndexWriteSeries = (Uint16)rnd();
                    inSide.endIndexWriteSeries   = (Uint16)rnd();

                    g_cur = "updindex";
                    step(FN_UPDIDX);
                }
            }
        }
    }
}

/* 8. EepromOperateTimeDeal —— 三个独立的超时。每个都是 `++x > MAX` 的边界,
 *    其中 rwTicker 用的是 `>=` 而另外两个是 `>` —— 这正是最容易改错的一处。
 *
 *   rwFuncCodeRepeatDelayTicker++            (无界)
 *   readFlag  -> if (++readTicker  > 250)  { readTicker = 0; errorEeprom = 2; }
 *   writeFlag -> if (++writeTicker > 500)  { writeTicker = 0; errorEeprom = 1; }
 *   mode != NO_OPERATION -> if (++rwTicker >= 5000) { rwTicker = 0; errorEeprom = 3; }
 *   否则 rwTicker = 0
 *
 * 注意 rwTicker 那一支: mode != NO_OPERATION 才自增, 否则**清零**。所以
 * "mode 与 rwTicker 的联动"是一个可观测的行为对, 两边都要跑。
 */
static void phase_operate_time(void)
{
    static const Uint16 flags[] = { 0, 1 };
    static const Uint16 maxes[] = { 249, 250, 251, 499, 500, 501, 4999, 5000, 5001 };
    unsigned f, w;
    unsigned mi;

    /* (a) readTicker / writeTicker 的边界 (两个 flag 的所有组合)。 */
    for (f = 0; f < 2; f++)
    {
        for (w = 0; w < 2; w++)
        {
            unsigned t;
            for (t = 0; t < sizeof maxes / sizeof maxes[0]; t++)
            {
                case_begin(0x3091 + f * 4096 + w * 2048 + t * 64);
                inSide.eepromOperateTime.readFlag  = (Uint16)f;
                inSide.eepromOperateTime.writeFlag = (Uint16)w;
                inSide.eepromOperateTime.readTicker  = maxes[t];
                inSide.eepromOperateTime.writeTicker = maxes[t];
                inSide.funcCodeRwMode = (Uint16)rnd();
                inSide.rwFuncCodeRepeatDelayTicker = (Uint16)rnd();
                g_cur = "optime";
                step(FN_TIME);
            }
        }
    }

    /* (b) rwTicker 的 >= 5000 与 mode 联动。 */
    for (mi = 0; mi < 3; mi++)
    {
        static const Uint16 modes[] = {
            FUNCCODE_RW_MODE_NO_OPERATION, FUNCCODE_RW_MODE_WRITE_ALL, 0xFFFF
        };
        unsigned t;
        for (t = 0; t < sizeof maxes / sizeof maxes[0]; t++)
        {
            case_begin(0x3091 + 0x4000 + mi * 512 + t * 16);
            inSide.funcCodeRwMode = modes[mi];
            inSide.eepromOperateTime.rwTicker = maxes[t];
            inSide.eepromOperateTime.readFlag = 0;
            inSide.eepromOperateTime.writeFlag = 0;
            g_cur = "optime-rw";
            step(FN_TIME);
        }
    }
}

/* 9. 位操作 / 初始化函数: InitI2CGpio / InitSetI2ca / I2CNoACK / I2CStop /
 *    I2CRcvByte。它们没有输入, 输出全在寄存器与 DELAY_US 计数上 ——
 *    正是"硬件副作用即输出"这一类的典型。
 *
 *    InitI2CGpio:   写 GPBPUD/GPBQSEL1/GPBMUX1 的 GPIO32/33, 一对 EALLOW/EDIS。
 *    InitSetI2ca:   I2CMDR=0x4000, I2CPSC(按 DSP_CLOCK=60 -> 5),
 *                   I2CCLKL/H = 25/25, I2CIER=0x0024, I2CMDR=0x4020,
 *                   I2CFFTX=0x6040, I2CFFRX=0x2040。
 *                   **注意 I2CFFTX/FFRX 不在 I2C_REGS 的前段, 而在尾部**
 *                   (rsvd2[19] 之后), 所以整结构体快照才能把它们看见。
 *    I2CNoACK:      DELAY_US(5) x2 + 两个时钟位, DELAY_US(5) x2。
 *    I2CStop:       三个位操作, 中间两次 DELAY_US(5) 与一次 DELAY_US(5)。
 *    I2CRcvByte:    SdaIoAsInput, 800 次 {ClkLow, DELAY_US(5), ClkHigh,
 *                   DELAY_US(5)}, 然后 ClkLow + DELAY_US(5) + SdaIoAsOutput
 *                   + DELAY_US(5)。**800 这个数**是唯一的循环边界。
 *
 *    每个都从随机寄存器状态起步, 于是"少写一个位""多写一次 DELAY"都可见。
 */
static void phase_bits(void)
{
    static const unsigned fns[] = { FN_INITGPIO, FN_INITSET, FN_NOACK, FN_STOP, FN_RCV };
    static const char *names[] = {
        "initi2cgpio", "initseti2ca", "i2cnoack", "i2cstop", "i2crcvbyte"
    };
    unsigned i, k;

    for (i = 0; i < sizeof fns / sizeof fns[0]; i++)
    {
        for (k = 0; k < 4; k++)
        {
            case_begin(0x30A1 + i * 256 + k * 8);
            /* 让 GPIO 与 I2C 的起始状态每次都不同 —— 否则"没写"与"写成 0"
             * 在某些位上不可区分。case_begin 已随机化, 这里额外把 SDA/SCL
             * 与 STP/BB/NACK 钉成确定的组合以覆盖分支。 */
            inW.gpioData.GPBDAT.bit.GPIO32 = (Uint16)(k & 1);
            inW.gpioData.GPBDAT.bit.GPIO33 = (Uint16)((k >> 1) & 1);
            inW.gpioCtrl.GPBDIR.bit.GPIO32 = (Uint16)(k & 1);
            inW.gpioCtrl.GPBDIR.bit.GPIO33 = (Uint16)((k >> 1) & 1);
            inW.i2c.I2CMDR.bit.STP = (Uint16)(k & 1);
            inW.i2c.I2CSTR.bit.BB  = (Uint16)((k >> 1) & 1);
            inW.i2c.I2CSTR.bit.NACK = (Uint16)((k >> 2) & 1);

            g_cur = names[i];
            step(fns[i]);
        }
    }
}

/* 10. I2cDealBeforeInit —— 位操作 + 两次**读** GPIO 的分支。
 *
 *      EALLOW; 写一堆 GPIO 控制寄存器; EDIS;
 *      DELAY_US(20000);
 *      I2CStop();
 *      SdaIoAsOutput(); I2cDataHigh(); SdaIoAsInput();
 *      if (!SDA)  a = 1;
 *      else {
 *          SdaIoAsOutput(); I2cDataLow(); SdaIoAsInput();
 *          if (SDA) a = 1;
 *      }
 *      if (a == 1) { I2CRcvByte(); I2CNoACK(); I2CStop(); }
 *
 *    这是本模块唯一**读输入引脚**的地方, 而且读的是 SDA 在"刚写过一次高/低"之后
 *    的值。两次读的值由 driver 在调用**之前**摆好:
 *
 *      第一次读 SDA: 由 driver 摆 GPBDAT.bit.GPIO32 决定。
 *      第二次读 SDA: 由 I2cDataLow() (即写 GPBCLEAR.GPIO32=1) 决定 ——
 *                    真实外设里"写 1 到 GPBCLEAR 的某一位"会把 GPBDAT 的对应位
 *                    清 0。这个**寄存器写语义**由 mock 实现 (见 mock_eeprom.c 的
 *                    gpio_write_semantics 一节), 不再是可关的旋钮。
 *
 *    这样一来 "a" 的取值路径完全由输入决定, 不再依赖 driver 事后插手:
 *
 *      路径 A (a = 1, 走第一次读): GPBDAT.bit.GPIO32 起始为 0。
 *      路径 B (a = 1, 走第二次读): 起始为 1, 而 I2cDataLow() 之后**仍然读回 1**
 *                                  —— 这需要 GPBDIR.bit.GPIO32 在该处为**输入**
 *                                  (SdaIoAsInput), 输入引脚的电平由外部上拉决定。
 *                                  本用例的模型是: "输入"方向上 GPBDAT 的位**不**
 *                                  被 GPBCLEAR 影响 (外部电平), 由 driver 通过
 *                                  gHostPinExtLevel32 指定。
 *      路径 C (a = 0, 两次读都非零): 第一次 1, 第二次 (输出方向) 被清成 0 后
 *                                  仍然读回 1 —— 模型里让"输入方向的外部电平"
 *                                  在第一次读之后就被观察到, 于是 a 停在 0。
 *
 *    这三种路径覆盖了 `a == 1` 的两条入口与 `a == 0` 的出口 —— 也就是原版
 *    `if (!SDA) ... else { ... if (SDA) ... }` 的全部可达分支。
 *
 *    "输入方向上 GPBDAT 的位不被 GPBCLEAR 影响" 不是凭空规定: C28x 的 GPBDAT
 *    读回的是**引脚当前电平** (输入) 或**输出锁存** (输出), 而 GPBCLEAR 清的是
 *    输出锁存。所以"输入方向读回外部电平"正是芯片行为。
 */
static void phase_before_init(void)
{
    /* (k&1): 起始电平 (第一次读)
     * (k&2): 输入方向的外部电平 inW.pinExtLevel32
     * (k&4): GPBDIR.bit.GPIO32 起始方向 (0=输入, 1=输出)
     *
     * 走一遍:
     *   A: 起始电平 0                     -> 第一次读 0 -> a = 1
     *   B: 起始电平 1, 外部电平 1, 输入方向 -> 第二次读 1 -> a = 1
     *   C: 起始电平 1, 外部电平 1, 输出方向 -> 第二次读 0 (被清) -> a = 0
     *
     * **外部电平必须写进 inW.pinExtLevel32, 不能直接写 gHostPinExtLevel32**:
     * `step()` 内部先 install_side()/install_world(), 而 install_world() 会把
     * gHostPinExtLevel32 重新赋成 inW.pinExtLevel32 —— 直接写全局等于在
     * step() 之前就被覆盖掉, 于是"外部电平"这一维从来没被真正驱动过。
     * 实测: 直接写全局时, 8 例里 `if (!SDA) a = 1;` 与 `if (SDA) a = 1;` 一次
     * 都没进 (a 恒 0), 于是分支尾部的 I2CStop() 永不执行, EP184 永远 MISSED。
     * 同一类陷阱见 tools/difftest/README 的"install_side() 要写在 inSide 钉死之后"。
     */
    unsigned k;

    for (k = 0; k < 8; k++)
    {
        case_begin(0x30B1 + k * 16);
        inW.gpioData.GPBDAT.bit.GPIO32   = (Uint16)(k & 1);
        inW.gpioData.GPBDAT.bit.GPIO33   = (Uint16)((k >> 3) & 1);
        inW.gpioCtrl.GPBDIR.bit.GPIO32   = (Uint16)((k >> 2) & 1);
        inW.gpioData.GPBSET.all   = 0;
        inW.gpioData.GPBCLEAR.all = 0;
        inW.pinExtLevel32                = (Uint16)((k >> 1) & 1);

        g_cur = "beforeinit";
        step(FN_BEFORE);
    }
}

/* 11. 随机长序列: 把 EepromDeal / I2cIntDeal / EepromOperateTimeDeal 交叉驱动,
 *     模拟"主循环 + I2C 中断"的真实交替。
 *
 *     这一步是做**状态空间覆盖**的: 前面各相位是分立的边界点, 这里是它们的
 *     随机组合 —— 尤其能抓到"状态机在某两条支路之间跳转时才出现的差异"。
 */
static void phase_random(long n)
{
    long i;
    unsigned long seed = 0x30C1;

    for (i = 0; i < n; i++)
    {
        unsigned pick;
        case_begin(seed + (unsigned long)i * 7919UL);

        /* 三到六步的混合序列 */
        {
            unsigned t;
            unsigned steps = 3 + (unsigned)(rnd() % 4);
            for (t = 0; t < steps; t++)
            {
                pick = (unsigned)(rnd() % 5);
                inSide.I2cMsg.status = (enum I2C_MSG_STATUS)(rnd() % 6);
                inW.i2c.I2CISRC.all  = (Uint16)(rnd() % 8);
                inW.i2c.I2CDRR       = (Uint16)rnd();

                switch (pick)
                {
                case 0: g_cur = "rand-deal";  step(FN_DEAL); break;
                case 1: g_cur = "rand-int";   step(FN_INT);  break;
                case 2: g_cur = "rand-time";  step(FN_TIME); break;
                case 3: g_cur = "rand-rw";    g_argMode = (Uint16)(rnd() % 3);
                                              step(FN_RWEE); break;
                default:
                    g_argRw    = &ref_funcCodeRead;
                    g_argRwNew = &new_funcCodeRead;
                    g_cur = "rand-read"; step(FN_READ);
                    break;
                }
            }
        }
    }
}

/* ---- main ---------------------------------------------------------------- */

static void preflight(void)
{
    int ok = 1;

    /* (1) 替身表长度对账 (编译期已断言一次, 这里再运行时确认表内容可用):
     *     fcIndex 909 必须走 fcNoAttri2Eeprom 分支并返回 4 (表首)。 */
    {
        Uint16 r;
        g_argIndex = 909;
        g_cur = "preflight-gefi";
        step(FN_GEFI);
        r = g_ret[1];
        printf("preflight: GetEepromIndexFromFcIndex(909) = %u (期望 4)%s\n",
               (unsigned)r, r == 4 ? "" : "  **FAIL**");
        if (r != 4) ok = 0;
    }

    /* (2) 反查是**查表**: `GetFcIndexFromEepromIndex(a) = eeprom2Fc[a]`。
     *     这里断言两条:
     *       (a) a = 950 的结果与表文件里的值**逐字对上** (= 751) —— 证明活分支
     *           确实是查表, 不是那个 (已死的) 线性扫描;
     *       (b) a = 0 与表首一致 (= 909)。
     *     如果哪天 F_DEBUG_RAM 被打开, 活分支会翻到线性扫描, 那时 (a) 会失败
     *     —— 这正是我们要的告警。 */
    {
        Uint16 r;
        g_argIndex = 950;
        g_cur = "preflight-gfei";
        step(FN_GFEI);
        r = g_ret[1];
        printf("preflight: GetFcIndexFromEepromIndex(950) = %u (查表期望 751)%s\n",
               (unsigned)r, r == 751 ? "" : "  **FAIL**");
        if (r != 751) ok = 0;

        g_argIndex = 0;
        g_cur = "preflight-gfei";
        step(FN_GFEI);
        r = g_ret[1];
        printf("preflight: GetFcIndexFromEepromIndex(0) = %u (查表期望 909)%s\n",
               (unsigned)r, r == 909 ? "" : "  **FAIL**");
        if (r != 909) ok = 0;
    }

    /* (3) DELAY_US 真的被记账: I2CRcvByte 的 800 次循环必须产生 800*2+2 次调用。
     *     参数是 DELAY_US(5) -> LoopCount = (5*1000/16.667 - 9)/5 = 58。 */
    {
        Uint32 d0, d1;
        case_begin(0x30D1);
        memset((void *)&GpioDataRegs, 0, sizeof GpioDataRegs);
        d0 = 0;
        gHostDelaySum = 0;
        gHostDelayCalls = 0;
        ref_I2CRcvByte();
        d1 = gHostDelaySum;
        printf("preflight: I2CRcvByte DELAY calls = %lu sum = %lu "
               "(期望 calls=1602 sum=%lu)%s\n",
               (unsigned long)gHostDelayCalls, (unsigned long)d1,
               (unsigned long)(1602UL * 58UL),
               (gHostDelayCalls == 1602UL) ? "" : "  **FAIL**");
        if (gHostDelayCalls != 1602UL) ok = 0;
        if (d1 == 0) { printf("preflight: **FAIL** DELAY_US 没有记账\n"); ok = 0; }
        (void)d0;
    }

    /* (4) 外设整结构体快照真的覆盖到 I2CFFTX/FFRX (结构体尾部)。 */
    {
        Uint16 before;
        case_begin(0x30D2);
        memset((void *)&I2caRegs, 0, sizeof I2caRegs);
        ref_InitSetI2ca();
        before = I2caRegs.I2CFFTX.all;
        printf("preflight: InitSetI2ca 之后 I2CFFTX = 0x%04X (期望 0x6040)%s\n",
               (unsigned)before, before == 0x6040 ? "" : "  **FAIL**");
        if (before != 0x6040) ok = 0;
    }

    if (!ok)
    {
        g_fails++;
    }
}

/* 相位门 (调试用, 平时全跑): FEEP_ONLY=<子串> 只跑名字里含该子串的相位。
 * 保留它是因为 f_eeprom 的相位之间有**模块全局**的耦合, 定位一次段错误
 * (或一次 MISMATCH) 需要能把相位单独拎出来跑 —— 见 README「怎么定位」。
 * 注意: **只有非空 FEEP_ONLY 时才打印 [phase] 行**, 免得污染落盘证据里
 * 的 `cases=...` 头行。 */
#define RUN_PH(name)                                                          \
    do {                                                                      \
        const char *gate = getenv("FEEP_ONLY");                               \
        if (gate == NULL || gate[0] == 0)                                     \
        {                                                                     \
            name();                                                           \
        }                                                                     \
        else if (strstr(gate, #name) != NULL)                                 \
        {                                                                     \
            fprintf(stderr, "[phase] %s ...\n", #name);                       \
            fflush(stderr);                                                   \
            name();                                                           \
            fprintf(stderr, "[phase] %s ok (cases=%ld fails=%d)\n", #name,    \
                    g_cases, g_fails);                                        \
            fflush(stderr);                                                   \
            printf("cases=%ld mismatches=%d\n", g_cases, g_fails);            \
        }                                                                     \
    } while (0)

int main(int argc, char **argv)
{
    long nrand = argc > 1 ? atol(argv[1]) : 20000;

    /* stdout 是**管道**时 (run_case.py 用 capture_output 抓) 默认块缓冲, 子进程
     * 退出时的最后一次 flush 在 Windows 上会与管道拆除竞争, 结果落盘证据里
     * 丢掉 `cases=...` 那一行 (实测: 直接重定向到文件时在, 走管道时丢)。
     * 关掉缓冲, 让头行一定到得了抓取端。 */
    setvbuf(stdout, NULL, _IONBF, 0);

    init_ptrs();

    RUN_PH(phase_lookup);        /* 纯查表, 无状态 */
    RUN_PH(phase_bits);          /* 无输入, 无 static */
    RUN_PH(phase_before_init);
    RUN_PH(phase_rwi2cbus);
    RUN_PH(phase_intdeal);
    RUN_PH(phase_rweeprom);
    RUN_PH(phase_read);          /* ReadFuncCode 的 static 序列 */
    RUN_PH(phase_write);         /* WriteFuncCode 的 static 序列 */
    RUN_PH(phase_write_handshake); /* RwEeprom 的两步握手 (补 24 条 MISSED 的缺口) */
    RUN_PH(phase_deal_parainit); /* paraInit 四种 mode 的首次覆盖: 必须早于其它 deal */
    RUN_PH(phase_deal_allrsvd);  /* RSVD 扫描走完全程 (i == number) */
    RUN_PH(phase_update_index);
    RUN_PH(phase_deal_modes);
    RUN_PH(phase_deal_edges);
    RUN_PH(phase_deal_userpara);  /* 取用户参数臂的数据源 (EP130): 第二次 buffer */
    RUN_PH(phase_operate_time);
    RUN_PH(preflight);
    if (nrand > 0)
    {
        phase_random(nrand);
    }

    printf("cases=%ld mismatches=%d\n", g_cases, g_fails);
    fflush(stdout);
    return g_fails != 0;
}
