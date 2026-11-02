/* Differential test driver for f_interface.

 * f_interface is the function-side <-> core-side data pump: 10 externally visible
 * functions, all of which either MOVE a block of words between two buffers or
 * COMPUTE one derived quantity from a handful of globals.

 *   copyDataFunc2Motor05ms   func -> motor,  14 words, one per source field
 *   copyDataFunc2Motor2ms    func -> motor,  76 words (5 memcpy blocks + 62
 *                            field-by-field writes)
 *   copyDataFunc2CF2ms       func -> motor,  memcpy of funcCode.group.cf[0..39]
 *   copyDataCore2Func2ms     motor -> func,  30 words into 25 named globals
 *   copyDataCore2Uf2ms       motor -> func,  memcpy into funcCode.group.uf[0..29]
 *   copyDataCore2Func05ms    empty (kept as a call point)
 *   checkSpeedFdbDir         a 4-state machine (NO/START/END/QUITE) that decides
 *                            funcCode.code.speedFdbDir
 *   UpdateDataCore2Func      the big one: frqRun / frqVFRun / frqVFRunRemainder,
 *                            an LPF, the frqFdb clamp, ABZPos, the display
 *                            quantities, the soft-overcurrent ticker, powerAddup
 *   UpdateDataFunc2Core0p5ms droop + frq2Core
 *   UpdateDataFunc2Core2ms   packs funcCode.code.* / motorFc.* into the three
 *                            DSP command bitfield unions

 * ---- the two-worlds split -------------------------------------------------

 * The module owns 37 objects (frq2Core, outVoltage, ABZPos, softOCTicker, ...).
 * All have external linkage and are therefore renamed ref_ / new_, so each side
 * has its own copy: the driver installs both from one input image and snapshots
 * both back.

 * Everything else is SHARED and must look identical to both sides on every call:

 *   funcCode, motorFc, runFlag, dspMainCmd/1, dspSubCmd, dspStatus, invtStatus,
 *   aiDeal[], invPara, the three send buffers, frq / frqTmp / frqDroop /
 *   frqCurAim / frqCurAimFrac / frqPuQ15, the display quantities, bUv,
 *   softOcDoFlag, motorCtrlTuneStatus, EQep1Regs.QPOSCNT

 * ... which is why the world is REINSTALLED before each side, not just once:
 * the module writes a large part of it.

 * ---- what is compared -----------------------------------------------------

 *   per-side : all 37 module-owned objects
 *   shared   : the whole WORLD image, i.e. everything the module can write --
 *              funcCode (code.* and group.uf[]), dspMainCmd / dspSubCmd /
 *              dspMainCmd1, invtStatus, frqDroop, outCurrentDisp, currentOcDisp,
 *              outVoltageDisp, frqRunDisp, itDisp, softOcDoFlag,
 *              motorCtrlTuneStatus, aiDeal[].sample, the three send buffers

 * ---- statics: the reason several phases are SEQUENCES ----------------------

 * Four pieces of state are function-scope statics and cannot be installed:

 *   checkSpeedFdbDir          checkFlag / speedDir / speedDirBak /
 *                             speedFdbDirTcnt
 *   UpdateDataCore2Func       outCurrentDispOldTmp (display hysteresis),
 *                             frqFdbLpf (a LowPassFilter whose calc is a
 *                             function POINTER into f_io.c)
 *   UpdateDataFunc2Core0p5ms  frq2CoreTmp (assigned then read in the same call,
 *                             but it is `static Uint16`, so the 16-bit
 *                             truncation happens BEFORE the comparison)

 * The first two are therefore stepped in LOCKSTEP: ref and new each get the
 * identical call sequence and the comparison happens after every single call, so
 * a divergence surfaces on the call that caused it.

 * ---- host-unrepresentable / deliberately unreached regions -----------------

 *   (1) memcpy. C28x `char` is 16 bits, so the RTS memcpy(dst, src, n) moves n
 *       WORDS; the host moves n BYTES. All seven calls in this module pass a
 *       WORD count (proved by their `#if 1 / #else` twin, which lists exactly
 *       that many fields). host_patch.json multiplies each count by
 *       sizeof(Uint16). Without it the host would copy half the words and
 *       "expand the memcpy into per-field assignments" -- a perfectly legal
 *       rewrite -- would show up as a FALSE difference. The driver's preflight
 *       asserts the patch took effect.

 *   (2) DIVISION BY ZERO is a precondition of the module, not a case to test.
 *       `frq2CoreTmp = (...) / ((int32)frqPuQ15)` and
 *       `droopValue = (...) / motorFc.motorPara.elem.ratingCurrent` both divide
 *       by a value that comes from outside. The bench keeps frqPuQ15 != 0 and
 *       ratingCurrent != 0 in every phase. A zero would fault on the host and
 *       fault identically on the target, so it says nothing about the rewrite.

 *   (3) `#if F_DEBUG_RAM` (f_debug.h: F_DEBUG_RAM is 0) is dead throughout --
 *       including the whole `errorCodeFromMotor = ERROR_NONE; dspStatus.bit.run
 *       = 1;` block. So is `#if DEBUG_F_POSITION_CTRL` (0), i.e. pcOriginDisp /
 *       bPcErrorOk / bPcErrorNear / currentInLpf. The bench makes no claim about
 *       those arms. The live arms are DEBUG_F_INTERFACE=1, DEBUG_F_POWER_ADD=1,
 *       DEBUG_F_MOTOR_FUNCCODE=1, DSP_2803X=1.

 *   (4) checkSpeedFdbDir reaches CHECK_SPEED_DIR_QUITE and never leaves: QUITE is
 *       terminal by construction (`if (checkFlag == QUITE) return;`). So the
 *       state-machine phase runs FIRST and drives the full cycle once; after
 *       that the function is inert for the rest of the process. That is fine --
 *       every statement in it, including the `tcnt > 2500` boundary, is exercised
 *       exactly once, by that phase.

 *   (5) FUNCCODE_ALL / struct MOTOR_FC are compared with memcmp. They are unions
 *       of a Uint16[] and a bitfield struct, so the comparison is exact -- but
 *       padding bytes, if any, are host layout, and this bench says nothing about
 *       target padding.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DSP28x_Project.h"
#include "mock_interface.h"

#define FAIL_LIMIT 20

/* ---- function index ------------------------------------------------------ */

#define FN_M05   0   /* copyDataFunc2Motor05ms   */
#define FN_M2    1   /* copyDataFunc2Motor2ms    */
#define FN_CF2   2   /* copyDataFunc2CF2ms       */
#define FN_C2F2  3   /* copyDataCore2Func2ms     */
#define FN_C2U2  4   /* copyDataCore2Uf2ms       */
#define FN_C2F05 5   /* copyDataCore2Func05ms    */
#define FN_SDIR  6   /* checkSpeedFdbDir         */
#define FN_UPDC  7   /* UpdateDataCore2Func      */
#define FN_UP05  8   /* UpdateDataFunc2Core0p5ms */
#define FN_UP2   9   /* UpdateDataFunc2Core2ms   */
#define FN_N    10

/* ---- ref side ------------------------------------------------------------ */

extern void ref_copyDataFunc2Motor05ms(void);
extern void ref_copyDataFunc2Motor2ms(void);
extern void ref_copyDataFunc2CF2ms(void);
extern void ref_copyDataCore2Func2ms(void);
extern void ref_copyDataCore2Uf2ms(void);
extern void ref_copyDataCore2Func05ms(void);
extern void ref_checkSpeedFdbDir(int32 frqFdb);
extern void ref_UpdateDataCore2Func(void);
extern void ref_UpdateDataFunc2Core0p5ms(void);
extern void ref_UpdateDataFunc2Core2ms(void);

extern Uint16 ref_frq2Core;
extern Uint16 ref_frqCurAim2Core;
extern Uint16 ref_rsvdData;
extern Uint16 ref_driveCoeffFrqFdb;
extern Uint16 ref_vcSpdLoopKp1;
extern Uint16 ref_vcSpdLoopKp2;
extern Uint16 ref_jerkFf;
extern Uint16 ref_speedMotor;
extern Uint16 ref_motorRun;
extern int32  ref_frqRun;
extern int32  ref_frqVFRun;
extern int32  ref_frqVFRunRemainder;
extern Uint16 ref_outVoltage;
extern Uint16 ref_outCurrent;
extern Uint16 ref_generatrixVoltage;
extern int16  ref_outPower;
extern Uint16 ref_errorCodeFromMotor;
extern Uint16 ref_errorsCodeFromMotor[2];
extern Uint16 ref_motorErrInfor[5];
extern Uint16 ref_outCurrentDispOld;
extern int16  ref_gPhiRt;
extern int16  ref_gPhiRtDisp;
extern Uint16 ref_pmsmRotorPos;
extern Uint16 ref_frqFdb;
extern Uint16 ref_frqFdbDisp;
extern int32  ref_frqFdbTmp;
extern Uint16 ref_frqFdbFlag;
extern Uint16 ref_torqueCurrent;
extern Uint16 ref_enCoderPosition;
extern Uint16 ref_ABZPos;
extern Uint16 ref_ZCount;
extern Uint16 ref_antiVibrateCoeff;
extern Uint16 ref_currentOc;
extern Uint16 ref_currentPu;
extern Uint16 ref_spdLoopOut;
extern Uint32 ref_softOCTicker;
extern Uint16 ref_powerAddDec;

/* ---- new side ------------------------------------------------------------ */

extern void new_copyDataFunc2Motor05ms(void);
extern void new_copyDataFunc2Motor2ms(void);
extern void new_copyDataFunc2CF2ms(void);
extern void new_copyDataCore2Func2ms(void);
extern void new_copyDataCore2Uf2ms(void);
extern void new_copyDataCore2Func05ms(void);
extern void new_checkSpeedFdbDir(int32 frqFdb);
extern void new_UpdateDataCore2Func(void);
extern void new_UpdateDataFunc2Core0p5ms(void);
extern void new_UpdateDataFunc2Core2ms(void);

extern Uint16 new_frq2Core;
extern Uint16 new_frqCurAim2Core;
extern Uint16 new_rsvdData;
extern Uint16 new_driveCoeffFrqFdb;
extern Uint16 new_vcSpdLoopKp1;
extern Uint16 new_vcSpdLoopKp2;
extern Uint16 new_jerkFf;
extern Uint16 new_speedMotor;
extern Uint16 new_motorRun;
extern int32  new_frqRun;
extern int32  new_frqVFRun;
extern int32  new_frqVFRunRemainder;
extern Uint16 new_outVoltage;
extern Uint16 new_outCurrent;
extern Uint16 new_generatrixVoltage;
extern int16  new_outPower;
extern Uint16 new_errorCodeFromMotor;
extern Uint16 new_errorsCodeFromMotor[2];
extern Uint16 new_motorErrInfor[5];
extern Uint16 new_outCurrentDispOld;
extern int16  new_gPhiRt;
extern int16  new_gPhiRtDisp;
extern Uint16 new_pmsmRotorPos;
extern Uint16 new_frqFdb;
extern Uint16 new_frqFdbDisp;
extern int32  new_frqFdbTmp;
extern Uint16 new_frqFdbFlag;
extern Uint16 new_torqueCurrent;
extern Uint16 new_enCoderPosition;
extern Uint16 new_ABZPos;
extern Uint16 new_ZCount;
extern Uint16 new_antiVibrateCoeff;
extern Uint16 new_currentOc;
extern Uint16 new_currentPu;
extern Uint16 new_spdLoopOut;
extern Uint32 new_softOCTicker;
extern Uint16 new_powerAddDec;

/* ---- UpdateDataCore2Func 的输入通道 -------------------------------------- */
/*
 * UpdateDataCore2Func 的第一件事就是 copyDataCore2Func2ms(), 而后者把下面这些
 * 全局量**全部**从 gSendToFunctionDataBuff[] 重新赋值:
 *
 *   currentOc [9]  speedMotor [10]  outVoltage [11]  currentPu [13]
 *   generatrixVoltage [18]  torqueCurrent [19]  outCurrent [20]
 *   pmsmRotorPos [21]  motorRun [23]  outPower [24]  itDisp [25]
 *   enCoderPosition [26]  gPhiRt [27]  ZCount [28]  antiVibrateCoeff [29]
 *
 * 所以 `install_side()` 装进去的 inSide.<这些字段> 会被**原地覆盖**, 对
 * UpdateDataCore2Func 一点用都没有 —— 真正决定它们的是函数缓冲区。
 *
 * 踩过一次: phase_hysteresis / phase_soft_oc / phase_power_add / phase_frq 都
 * 用 `inSide.outCurrent = ...` / `inSide.speedMotor = ...` 喂输入, 于是
 * `> 15` / `>= 15` / `outPower > 0` / `powerAddDec > 5000` / 32000 钳位这些
 * 边界全都没被打到, 负向对照里 11 条变异 MISSED。输入路径收敛到本 helper,
 * 并且 preflight (3) 直接断言"缓冲区确实驱动了这些量"。
 */
#define IFACE_FI_CURRENT_OC    9
#define IFACE_FI_SPEED_MOTOR  10
#define IFACE_FI_OUT_VOLTAGE  11
#define IFACE_FI_CURRENT_PU   13
#define IFACE_FI_GEN_VOLTAGE  18
#define IFACE_FI_TORQUE_CUR   19
#define IFACE_FI_OUT_CURRENT  20
#define IFACE_FI_MOTOR_RUN    23
#define IFACE_FI_OUT_POWER    24
#define IFACE_FI_G_PHI_RT     27

/* powerAddupInt 也是**从缓冲区来的**, 而且藏得很深 —— 别踩第二次。
 *
 * `FUNCCODE_ALL` 是 union: `all[FNUM_ALL=1116]` / `group` (1116 字) / `code`
 * (1290 字)。`group.uf[]` 是 group 的**最后 30 字** (字 1086..1115), 而
 * `code.powerAddupInt` 在**字 1098** —— 正好落在 uf[12] 里。于是
 * `copyDataCore2Func2ms()` 的
 *
 *     memcpy(&funcCode.group.uf[0], &gSendToFunctionDataBuff[30], 30 字)
 *
 * 会在每次调用开头把 `powerAddupInt` **原地覆盖**成
 * `gSendToFunctionDataBuff[30 + 12] = [42]`。
 *
 * 也就是说 `inW.funcCode.code.powerAddupInt = pi[i]` 是**死写**: 装进去,
 * 函数第一个动作就把它冲掉。必须走缓冲区。offsetof 实测:
 *     offsetof(GROUP, uf)      = 2172 字节 = 字 1086
 *     offsetof(CODE, powerAddupInt) = 2196 字节 = 字 1098
 *     => uf[12] => gSendToFunctionDataBuff[30 + 12] = [42]
 */
#define IFACE_FI_POWER_ADDUP_INT   42

/* ---- per-side snapshot --------------------------------------------------- */

struct SIDE
{
    Uint16 frq2Core;
    Uint16 frqCurAim2Core;
    Uint16 rsvdData;
    Uint16 driveCoeffFrqFdb;
    Uint16 vcSpdLoopKp1;
    Uint16 vcSpdLoopKp2;
    Uint16 jerkFf;
    Uint16 speedMotor;
    Uint16 motorRun;
    int32  frqRun;
    int32  frqVFRun;
    int32  frqVFRunRemainder;
    Uint16 outVoltage;
    Uint16 outCurrent;
    Uint16 generatrixVoltage;
    int16  outPower;
    Uint16 errorCodeFromMotor;
    Uint16 errorsCodeFromMotor[2];
    Uint16 motorErrInfor[5];
    Uint16 outCurrentDispOld;
    int16  gPhiRt;
    int16  gPhiRtDisp;
    Uint16 pmsmRotorPos;
    Uint16 frqFdb;
    Uint16 frqFdbDisp;
    int32  frqFdbTmp;
    Uint16 frqFdbFlag;
    Uint16 torqueCurrent;
    Uint16 enCoderPosition;
    Uint16 ABZPos;
    Uint16 ZCount;
    Uint16 antiVibrateCoeff;
    Uint16 currentOc;
    Uint16 currentPu;
    Uint16 spdLoopOut;
    Uint32 softOCTicker;
    Uint16 powerAddDec;
};

static struct SIDE inSide;
static struct SIDE S[2];

#define SIDE_INSTALL(P)                                                       \
    do {                                                                      \
        P##frq2Core           = inSide.frq2Core;                              \
        P##frqCurAim2Core     = inSide.frqCurAim2Core;                        \
        P##rsvdData           = inSide.rsvdData;                              \
        P##driveCoeffFrqFdb   = inSide.driveCoeffFrqFdb;                      \
        P##vcSpdLoopKp1       = inSide.vcSpdLoopKp1;                          \
        P##vcSpdLoopKp2       = inSide.vcSpdLoopKp2;                          \
        P##jerkFf             = inSide.jerkFf;                                \
        P##speedMotor         = inSide.speedMotor;                            \
        P##motorRun           = inSide.motorRun;                              \
        P##frqRun             = inSide.frqRun;                                \
        P##frqVFRun           = inSide.frqVFRun;                              \
        P##frqVFRunRemainder  = inSide.frqVFRunRemainder;                     \
        P##outVoltage         = inSide.outVoltage;                            \
        P##outCurrent         = inSide.outCurrent;                            \
        P##generatrixVoltage  = inSide.generatrixVoltage;                     \
        P##outPower           = inSide.outPower;                              \
        P##errorCodeFromMotor = inSide.errorCodeFromMotor;                    \
        memcpy(P##errorsCodeFromMotor, inSide.errorsCodeFromMotor,            \
               sizeof inSide.errorsCodeFromMotor);                            \
        memcpy(P##motorErrInfor, inSide.motorErrInfor,                        \
               sizeof inSide.motorErrInfor);                                  \
        P##outCurrentDispOld  = inSide.outCurrentDispOld;                     \
        P##gPhiRt             = inSide.gPhiRt;                                \
        P##gPhiRtDisp         = inSide.gPhiRtDisp;                            \
        P##pmsmRotorPos       = inSide.pmsmRotorPos;                          \
        P##frqFdb             = inSide.frqFdb;                                \
        P##frqFdbDisp         = inSide.frqFdbDisp;                            \
        P##frqFdbTmp          = inSide.frqFdbTmp;                             \
        P##frqFdbFlag         = inSide.frqFdbFlag;                            \
        P##torqueCurrent      = inSide.torqueCurrent;                         \
        P##enCoderPosition    = inSide.enCoderPosition;                       \
        P##ABZPos             = inSide.ABZPos;                                \
        P##ZCount             = inSide.ZCount;                                \
        P##antiVibrateCoeff   = inSide.antiVibrateCoeff;                      \
        P##currentOc          = inSide.currentOc;                             \
        P##currentPu          = inSide.currentPu;                             \
        P##spdLoopOut         = inSide.spdLoopOut;                            \
        P##softOCTicker       = inSide.softOCTicker;                          \
        P##powerAddDec        = inSide.powerAddDec;                           \
    } while (0)

#define SIDE_SNAPSHOT(P, s)                                                   \
    do {                                                                      \
        (s)->frq2Core           = P##frq2Core;                                \
        (s)->frqCurAim2Core     = P##frqCurAim2Core;                          \
        (s)->rsvdData           = P##rsvdData;                                \
        (s)->driveCoeffFrqFdb   = P##driveCoeffFrqFdb;                        \
        (s)->vcSpdLoopKp1       = P##vcSpdLoopKp1;                            \
        (s)->vcSpdLoopKp2       = P##vcSpdLoopKp2;                            \
        (s)->jerkFf             = P##jerkFf;                                  \
        (s)->speedMotor         = P##speedMotor;                              \
        (s)->motorRun           = P##motorRun;                                \
        (s)->frqRun             = P##frqRun;                                  \
        (s)->frqVFRun           = P##frqVFRun;                                \
        (s)->frqVFRunRemainder  = P##frqVFRunRemainder;                       \
        (s)->outVoltage         = P##outVoltage;                              \
        (s)->outCurrent         = P##outCurrent;                              \
        (s)->generatrixVoltage  = P##generatrixVoltage;                       \
        (s)->outPower           = P##outPower;                                \
        (s)->errorCodeFromMotor = P##errorCodeFromMotor;                      \
        memcpy((s)->errorsCodeFromMotor, P##errorsCodeFromMotor,              \
               sizeof (s)->errorsCodeFromMotor);                              \
        memcpy((s)->motorErrInfor, P##motorErrInfor,                          \
               sizeof (s)->motorErrInfor);                                    \
        (s)->outCurrentDispOld  = P##outCurrentDispOld;                       \
        (s)->gPhiRt             = P##gPhiRt;                                  \
        (s)->gPhiRtDisp         = P##gPhiRtDisp;                              \
        (s)->pmsmRotorPos       = P##pmsmRotorPos;                            \
        (s)->frqFdb             = P##frqFdb;                                  \
        (s)->frqFdbDisp         = P##frqFdbDisp;                              \
        (s)->frqFdbTmp          = P##frqFdbTmp;                               \
        (s)->frqFdbFlag         = P##frqFdbFlag;                              \
        (s)->torqueCurrent      = P##torqueCurrent;                           \
        (s)->enCoderPosition    = P##enCoderPosition;                         \
        (s)->ABZPos             = P##ABZPos;                                  \
        (s)->ZCount             = P##ZCount;                                  \
        (s)->antiVibrateCoeff   = P##antiVibrateCoeff;                        \
        (s)->currentOc          = P##currentOc;                               \
        (s)->currentPu          = P##currentPu;                               \
        (s)->spdLoopOut         = P##spdLoopOut;                              \
        (s)->softOCTicker       = P##softOCTicker;                            \
        (s)->powerAddDec        = P##powerAddDec;                             \
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
    volatile struct EQEP_REGS eqep;

    struct AI_DEAL  ai[AI_NUMBER];
    Uint16          bUv;
    Uint16          currentOcDisp;

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

    int16  motor05[IFACE_MOTOR_05MS_NUM];
    int16  motor2ms[IFACE_MOTOR_2MS_NUM];
    int16  func2ms[IFACE_FUNC_2MS_NUM];

    struct INV_PARA invPara;
    union INVT_STATUS invtStatus;
    Uint16 itDisp;

    Uint16 motorCtrlTuneStatus;
    struct MOTOR_FC motorFc;

    Uint16 outCurrentDisp;
    Uint16 outVoltageDisp;

    union RUN_FLAG  runFlag;
    enum RUN_STATUS runStatus;

    Uint16 softOcDoFlag;
    Uint32 torqueCurrentAct;
    Uint16 tuneCmd;
    int16  upperTorque;
    Uint16 vfSeparateVol;
};

static struct WORLD inW;
static struct WORLD SH[2];

static void install_world(void)
{
    memcpy((void *)&EQep1Regs, (const void *)&inW.eqep, sizeof inW.eqep);

    memcpy(aiDeal, inW.ai, sizeof inW.ai);
    bUv          = inW.bUv;
    currentOcDisp = inW.currentOcDisp;

    memcpy(&dspMainCmd,  &inW.dspMainCmd,  sizeof inW.dspMainCmd);
    memcpy(&dspMainCmd1, &inW.dspMainCmd1, sizeof inW.dspMainCmd1);
    memcpy(&dspStatus,   &inW.dspStatus,   sizeof inW.dspStatus);
    memcpy(&dspSubCmd,   &inW.dspSubCmd,   sizeof inW.dspSubCmd);

    frq           = inW.frq;
    frqCurAim     = inW.frqCurAim;
    frqCurAimFrac = inW.frqCurAimFrac;
    frqDroop      = inW.frqDroop;
    frqPuQ15      = inW.frqPuQ15;
    frqRunDisp    = inW.frqRunDisp;
    frqTmp        = inW.frqTmp;

    memcpy(&funcCode, &inW.funcCode, sizeof inW.funcCode);

    memcpy(gSendToMotor05MsDataBuff, inW.motor05, sizeof inW.motor05);
    memcpy(gSendToMotor2MsDataBuff,  inW.motor2ms, sizeof inW.motor2ms);
    memcpy(gSendToFunctionDataBuff,  inW.func2ms, sizeof inW.func2ms);

    memcpy(&invPara,    &inW.invPara,    sizeof inW.invPara);
    memcpy(&invtStatus, &inW.invtStatus, sizeof inW.invtStatus);
    itDisp = inW.itDisp;

    motorCtrlTuneStatus = inW.motorCtrlTuneStatus;
    memcpy(&motorFc, &inW.motorFc, sizeof inW.motorFc);

    outCurrentDisp = inW.outCurrentDisp;
    outVoltageDisp = inW.outVoltageDisp;

    memcpy(&runFlag, &inW.runFlag, sizeof inW.runFlag);
    runStatus = inW.runStatus;

    softOcDoFlag      = inW.softOcDoFlag;
    torqueCurrentAct  = inW.torqueCurrentAct;
    tuneCmd           = inW.tuneCmd;
    upperTorque       = inW.upperTorque;
    vfSeparateVol     = inW.vfSeparateVol;
}

/* 给 UpdateDataCore2Func 喂它真正看得见的输入 (见 IFACE_FI_* 注释)。
 *
 * `powerAddupInt` 也在这里 —— 它同样只能从函数缓冲区进 (见
 * IFACE_FI_POWER_ADDUP_INT 的长注释)。 */
static void updc_in(Uint16 currentPu, Uint16 outCurrent, int16 outPower)
{
    inW.func2ms[IFACE_FI_CURRENT_PU]  = (int16)currentPu;
    inW.func2ms[IFACE_FI_OUT_CURRENT] = (int16)outCurrent;
    inW.func2ms[IFACE_FI_OUT_POWER]   = outPower;
}

/* 只改 powerAddupInt 这一个输入, 其余不动。 */
static void updc_pai(Uint16 val)
{
    inW.func2ms[IFACE_FI_POWER_ADDUP_INT] = (int16)val;
}

static void snap_world(int side)
{
    memcpy((void *)&SH[side].eqep, (const void *)&EQep1Regs, sizeof EQep1Regs);

    /* **不要**写 `sizeof aiDeal`: f_io.h 里它是 `extern struct AI_DEAL aiDeal[]`
     * —— 一个**不完全数组类型**, `sizeof` 求值为 0 (MSVC 只给一条 C4034 警告,
     * 于是 memcpy 一个字节都不搬, 而且看起来"跑得好好的")。用完整类型取长度。 */
    memcpy(SH[side].ai, aiDeal, sizeof SH[side].ai);
    SH[side].bUv           = bUv;
    SH[side].currentOcDisp = currentOcDisp;

    memcpy(&SH[side].dspMainCmd,  &dspMainCmd,  sizeof dspMainCmd);
    memcpy(&SH[side].dspMainCmd1, &dspMainCmd1, sizeof dspMainCmd1);
    memcpy(&SH[side].dspStatus,   &dspStatus,   sizeof dspStatus);
    memcpy(&SH[side].dspSubCmd,   &dspSubCmd,   sizeof dspSubCmd);

    SH[side].frq           = frq;
    SH[side].frqCurAim     = frqCurAim;
    SH[side].frqCurAimFrac = frqCurAimFrac;
    SH[side].frqDroop      = frqDroop;
    SH[side].frqPuQ15      = frqPuQ15;
    SH[side].frqRunDisp    = frqRunDisp;
    SH[side].frqTmp        = frqTmp;

    memcpy(&SH[side].funcCode, &funcCode, sizeof funcCode);

    memcpy(SH[side].motor05, gSendToMotor05MsDataBuff, sizeof SH[side].motor05);
    memcpy(SH[side].motor2ms, gSendToMotor2MsDataBuff, sizeof SH[side].motor2ms);
    memcpy(SH[side].func2ms, gSendToFunctionDataBuff, sizeof SH[side].func2ms);

    memcpy(&SH[side].invPara,    &invPara,    sizeof invPara);
    memcpy(&SH[side].invtStatus, &invtStatus, sizeof invtStatus);
    SH[side].itDisp = itDisp;

    SH[side].motorCtrlTuneStatus = motorCtrlTuneStatus;
    memcpy(&SH[side].motorFc, &motorFc, sizeof motorFc);

    SH[side].outCurrentDisp = outCurrentDisp;
    SH[side].outVoltageDisp = outVoltageDisp;

    memcpy(&SH[side].runFlag, &runFlag, sizeof runFlag);
    SH[side].runStatus = runStatus;

    SH[side].softOcDoFlag     = softOcDoFlag;
    SH[side].torqueCurrentAct = torqueCurrentAct;
    SH[side].tuneCmd          = tuneCmd;
    SH[side].upperTorque      = upperTorque;
    SH[side].vfSeparateVol    = vfSeparateVol;
}

/* ---- reporting ----------------------------------------------------------- */

static long g_cases = 0;
static int  g_fails = 0;
static const char *g_cur = "";

/* checkSpeedFdbDir 走到 QUITE 那一刻写下的 speedFdbDir, 由 phase_speed_dir 记录。
 * 不能事后去读 funcCode: `install_world()` 每次调用前都会把 funcCode 从 inW 重置,
 * 所以跑完那一步之后现场早就被下一次 install 抹掉了。 */
static Uint16 g_sdFinal = 0xFFFF;

static void report(const char *what)
{
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s: %s\n", g_cur, what);
    printf("  in : frqPuQ15=%u frq=%ld frqTmp=%ld frqDroop=%ld runStatus=%d "
           "runFlag=0x%04X\n",
           (unsigned)inW.frqPuQ15, (long)inW.frq, (long)inW.frqTmp,
           (long)inW.frqDroop, (int)inW.runStatus,
           (unsigned)inW.runFlag.all);
    printf("  in : outCurrent=%u currentPu=%u outPower=%d powerAddDec=%u "
           "softOCTicker=%lu softOCValue=%u softOCDelay=%u\n",
           (unsigned)inSide.outCurrent, (unsigned)inSide.currentPu,
           (int)inSide.outPower, (unsigned)inSide.powerAddDec,
           (unsigned long)inSide.softOCTicker,
           (unsigned)funcCode.code.softOCValue,
           (unsigned)funcCode.code.softOCDelay);
    printf("  in : genVolt=%u speedMotor=%u frqVFRunRem=%ld ovGain=%u ocGain=%u "
           "droopCtrl=%u\n",
           (unsigned)inSide.generatrixVoltage, (unsigned)inSide.speedMotor,
           (long)inSide.frqVFRunRemainder, (unsigned)funcCode.code.ovGain,
           (unsigned)funcCode.code.ocGain, (unsigned)funcCode.code.droopCtrl);
    printf("  in : motorCtrlMode=%u motorType=%u ratingCurrent=%u "
           "accDecStatus=%u inPhaseLoss=%u QPOSCNT=%lu\n",
           (unsigned)motorFc.motorCtrlMode,
           (unsigned)motorFc.motorPara.elem.motorType,
           (unsigned)motorFc.motorPara.elem.ratingCurrent,
           (unsigned)runFlag.bit.accDecStatus,
           (unsigned)funcCode.code.inPhaseLossProtect,
           (unsigned long)EQep1Regs.QPOSCNT);
    printf("  frqFdb %u/%u  frqFdbTmp %ld/%ld  frq2Core %u/%u  frqRun %ld/%ld\n",
           (unsigned)S[0].frqFdb, (unsigned)S[1].frqFdb,
           (long)S[0].frqFdbTmp, (long)S[1].frqFdbTmp,
           (unsigned)S[0].frq2Core, (unsigned)S[1].frq2Core,
           (long)S[0].frqRun, (long)S[1].frqRun);
    printf("  ABZPos %u/%u  softOCTicker %lu/%lu  powerAddDec %u/%u\n",
           (unsigned)S[0].ABZPos, (unsigned)S[1].ABZPos,
           (unsigned long)S[0].softOCTicker, (unsigned long)S[1].softOCTicker,
           (unsigned)S[0].powerAddDec, (unsigned)S[1].powerAddDec);
    printf("  outCurrentDisp %u/%u  currentOcDisp %u/%u  outVoltageDisp %u/%u\n",
           (unsigned)SH[0].outCurrentDisp, (unsigned)SH[1].outCurrentDisp,
           (unsigned)SH[0].currentOcDisp, (unsigned)SH[1].currentOcDisp,
           (unsigned)SH[0].outVoltageDisp, (unsigned)SH[1].outVoltageDisp);
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
    /* ---- per-side module-owned objects ---- */
    cmp("frq2Core",           &S[0].frq2Core, &S[1].frq2Core, sizeof S[0].frq2Core);
    cmp("frqCurAim2Core",     &S[0].frqCurAim2Core, &S[1].frqCurAim2Core, sizeof S[0].frqCurAim2Core);
    cmp("rsvdData",           &S[0].rsvdData, &S[1].rsvdData, sizeof S[0].rsvdData);
    cmp("driveCoeffFrqFdb",   &S[0].driveCoeffFrqFdb, &S[1].driveCoeffFrqFdb, sizeof S[0].driveCoeffFrqFdb);
    cmp("vcSpdLoopKp1",       &S[0].vcSpdLoopKp1, &S[1].vcSpdLoopKp1, sizeof S[0].vcSpdLoopKp1);
    cmp("vcSpdLoopKp2",       &S[0].vcSpdLoopKp2, &S[1].vcSpdLoopKp2, sizeof S[0].vcSpdLoopKp2);
    cmp("jerkFf",             &S[0].jerkFf, &S[1].jerkFf, sizeof S[0].jerkFf);
    cmp("speedMotor",         &S[0].speedMotor, &S[1].speedMotor, sizeof S[0].speedMotor);
    cmp("motorRun",           &S[0].motorRun, &S[1].motorRun, sizeof S[0].motorRun);
    cmp("frqRun",             &S[0].frqRun, &S[1].frqRun, sizeof S[0].frqRun);
    cmp("frqVFRun",           &S[0].frqVFRun, &S[1].frqVFRun, sizeof S[0].frqVFRun);
    cmp("frqVFRunRemainder",  &S[0].frqVFRunRemainder, &S[1].frqVFRunRemainder, sizeof S[0].frqVFRunRemainder);
    cmp("outVoltage",         &S[0].outVoltage, &S[1].outVoltage, sizeof S[0].outVoltage);
    cmp("outCurrent",         &S[0].outCurrent, &S[1].outCurrent, sizeof S[0].outCurrent);
    cmp("generatrixVoltage",  &S[0].generatrixVoltage, &S[1].generatrixVoltage, sizeof S[0].generatrixVoltage);
    cmp("outPower",           &S[0].outPower, &S[1].outPower, sizeof S[0].outPower);
    cmp("errorCodeFromMotor", &S[0].errorCodeFromMotor, &S[1].errorCodeFromMotor, sizeof S[0].errorCodeFromMotor);
    cmp("errorsCodeFromMotor", S[0].errorsCodeFromMotor, S[1].errorsCodeFromMotor, sizeof S[0].errorsCodeFromMotor);
    cmp("motorErrInfor",      S[0].motorErrInfor, S[1].motorErrInfor, sizeof S[0].motorErrInfor);
    cmp("outCurrentDispOld",  &S[0].outCurrentDispOld, &S[1].outCurrentDispOld, sizeof S[0].outCurrentDispOld);
    cmp("gPhiRt",             &S[0].gPhiRt, &S[1].gPhiRt, sizeof S[0].gPhiRt);
    cmp("gPhiRtDisp",         &S[0].gPhiRtDisp, &S[1].gPhiRtDisp, sizeof S[0].gPhiRtDisp);
    cmp("pmsmRotorPos",       &S[0].pmsmRotorPos, &S[1].pmsmRotorPos, sizeof S[0].pmsmRotorPos);
    cmp("frqFdb",             &S[0].frqFdb, &S[1].frqFdb, sizeof S[0].frqFdb);
    cmp("frqFdbDisp",         &S[0].frqFdbDisp, &S[1].frqFdbDisp, sizeof S[0].frqFdbDisp);
    cmp("frqFdbTmp",          &S[0].frqFdbTmp, &S[1].frqFdbTmp, sizeof S[0].frqFdbTmp);
    cmp("frqFdbFlag",         &S[0].frqFdbFlag, &S[1].frqFdbFlag, sizeof S[0].frqFdbFlag);
    cmp("torqueCurrent",      &S[0].torqueCurrent, &S[1].torqueCurrent, sizeof S[0].torqueCurrent);
    cmp("enCoderPosition",    &S[0].enCoderPosition, &S[1].enCoderPosition, sizeof S[0].enCoderPosition);
    cmp("ABZPos",             &S[0].ABZPos, &S[1].ABZPos, sizeof S[0].ABZPos);
    cmp("ZCount",             &S[0].ZCount, &S[1].ZCount, sizeof S[0].ZCount);
    cmp("antiVibrateCoeff",   &S[0].antiVibrateCoeff, &S[1].antiVibrateCoeff, sizeof S[0].antiVibrateCoeff);
    cmp("currentOc",          &S[0].currentOc, &S[1].currentOc, sizeof S[0].currentOc);
    cmp("currentPu",          &S[0].currentPu, &S[1].currentPu, sizeof S[0].currentPu);
    cmp("spdLoopOut",         &S[0].spdLoopOut, &S[1].spdLoopOut, sizeof S[0].spdLoopOut);
    cmp("softOCTicker",       &S[0].softOCTicker, &S[1].softOCTicker, sizeof S[0].softOCTicker);
    cmp("powerAddDec",        &S[0].powerAddDec, &S[1].powerAddDec, sizeof S[0].powerAddDec);

    /* ---- shared state the module writes ---- */
    cmp("funcCode",        &SH[0].funcCode,    &SH[1].funcCode,    sizeof SH[0].funcCode);
    cmp("dspMainCmd",      &SH[0].dspMainCmd,  &SH[1].dspMainCmd,  sizeof SH[0].dspMainCmd);
    cmp("dspMainCmd1",     &SH[0].dspMainCmd1, &SH[1].dspMainCmd1, sizeof SH[0].dspMainCmd1);
    cmp("dspSubCmd",       &SH[0].dspSubCmd,   &SH[1].dspSubCmd,   sizeof SH[0].dspSubCmd);
    cmp("dspStatus",       &SH[0].dspStatus,   &SH[1].dspStatus,   sizeof SH[0].dspStatus);
    cmp("invtStatus",      &SH[0].invtStatus,  &SH[1].invtStatus,  sizeof SH[0].invtStatus);
    cmp("frqDroop",        &SH[0].frqDroop,    &SH[1].frqDroop,    sizeof SH[0].frqDroop);
    cmp("outCurrentDisp",  &SH[0].outCurrentDisp, &SH[1].outCurrentDisp, sizeof SH[0].outCurrentDisp);
    cmp("currentOcDisp",   &SH[0].currentOcDisp,  &SH[1].currentOcDisp,  sizeof SH[0].currentOcDisp);
    cmp("outVoltageDisp",  &SH[0].outVoltageDisp, &SH[1].outVoltageDisp, sizeof SH[0].outVoltageDisp);
    cmp("frqRunDisp",      &SH[0].frqRunDisp,  &SH[1].frqRunDisp,  sizeof SH[0].frqRunDisp);
    cmp("itDisp",          &SH[0].itDisp,      &SH[1].itDisp,      sizeof SH[0].itDisp);
    cmp("softOcDoFlag",    &SH[0].softOcDoFlag, &SH[1].softOcDoFlag, sizeof SH[0].softOcDoFlag);
    cmp("motorCtrlTuneStatus", &SH[0].motorCtrlTuneStatus, &SH[1].motorCtrlTuneStatus,
        sizeof SH[0].motorCtrlTuneStatus);
    cmp("aiDeal",          SH[0].ai,           SH[1].ai,           sizeof SH[0].ai);
    cmp("motor05buff",     SH[0].motor05,      SH[1].motor05,      sizeof SH[0].motor05);
    cmp("motor2msbuff",    SH[0].motor2ms,     SH[1].motor2ms,     sizeof SH[0].motor2ms);
    cmp("func2msbuff",     SH[0].func2ms,      SH[1].func2ms,      sizeof SH[0].func2ms);
    /* motorFc 的 memcpy 源字段被读但不被写; 仍然比一次, 万一改写引入了写入 */
    cmp("motorFc",         &SH[0].motorFc,     &SH[1].motorFc,     sizeof SH[0].motorFc);
}

/* ---- lockstep step ------------------------------------------------------- */

typedef void (*void_fn)(void);

static void_fn g_refFn[FN_N];
static void_fn g_newFn[FN_N];
static const char *g_fnName[FN_N] = {
    "copyDataFunc2Motor05ms",
    "copyDataFunc2Motor2ms",
    "copyDataFunc2CF2ms",
    "copyDataCore2Func2ms",
    "copyDataCore2Uf2ms",
    "copyDataCore2Func05ms",
    "checkSpeedFdbDir",
    "UpdateDataCore2Func",
    "UpdateDataFunc2Core0p5ms",
    "UpdateDataFunc2Core2ms"
};

/* checkSpeedFdbDir 带一个 int32 参数, 用全局量传递。 */
static int32 g_arg;

static void ref_call_sdir(void) { ref_checkSpeedFdbDir(g_arg); }
static void new_call_sdir(void) { new_checkSpeedFdbDir(g_arg); }

static void init_ptrs(void)
{
    g_refFn[FN_M05]  = ref_copyDataFunc2Motor05ms;
    g_refFn[FN_M2]   = ref_copyDataFunc2Motor2ms;
    g_refFn[FN_CF2]  = ref_copyDataFunc2CF2ms;
    g_refFn[FN_C2F2] = ref_copyDataCore2Func2ms;
    g_refFn[FN_C2U2] = ref_copyDataCore2Uf2ms;
    g_refFn[FN_C2F05]= ref_copyDataCore2Func05ms;
    g_refFn[FN_SDIR] = ref_call_sdir;
    g_refFn[FN_UPDC] = ref_UpdateDataCore2Func;
    g_refFn[FN_UP05] = ref_UpdateDataFunc2Core0p5ms;
    g_refFn[FN_UP2]  = ref_UpdateDataFunc2Core2ms;

    g_newFn[FN_M05]  = new_copyDataFunc2Motor05ms;
    g_newFn[FN_M2]   = new_copyDataFunc2Motor2ms;
    g_newFn[FN_CF2]  = new_copyDataFunc2CF2ms;
    g_newFn[FN_C2F2] = new_copyDataCore2Func2ms;
    g_newFn[FN_C2U2] = new_copyDataCore2Uf2ms;
    g_newFn[FN_C2F05]= new_copyDataCore2Func05ms;
    g_newFn[FN_SDIR] = new_call_sdir;
    g_newFn[FN_UPDC] = new_UpdateDataCore2Func;
    g_newFn[FN_UP05] = new_UpdateDataFunc2Core0p5ms;
    g_newFn[FN_UP2]  = new_UpdateDataFunc2Core2ms;
}

/* One lockstep step: install the world, run ref, snapshot; install the SAME
 * world again, run new, snapshot; compare. Module-private state is NOT
 * reinstalled, so a sequence of steps carries it forward on both sides at once.
 */
static void step(int which)
{
    install_world();
    g_refFn[which]();
    snapshot(0);
    snap_world(0);

    install_world();
    g_newFn[which]();
    snapshot(1);
    snap_world(1);

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

    inSide.frq2Core           = (Uint16)rnd();
    inSide.frqCurAim2Core     = (Uint16)rnd();
    inSide.rsvdData           = (Uint16)rnd();
    inSide.driveCoeffFrqFdb   = (Uint16)rnd();
    inSide.vcSpdLoopKp1       = (Uint16)rnd();
    inSide.vcSpdLoopKp2       = (Uint16)rnd();
    inSide.jerkFf             = (Uint16)rnd();
    inSide.speedMotor         = (Uint16)rnd();
    inSide.motorRun           = (Uint16)rnd();
    inSide.frqRun             = (int32)rnd();
    inSide.frqVFRun           = (int32)rnd();
    inSide.frqVFRunRemainder  = (int32)rnd();
    inSide.outVoltage         = (Uint16)rnd();
    inSide.outCurrent         = (Uint16)rnd();
    inSide.generatrixVoltage  = (Uint16)rnd();
    inSide.outPower           = (int16)rnd();
    inSide.errorCodeFromMotor = (Uint16)rnd();
    rnd_fill(inSide.errorsCodeFromMotor, sizeof inSide.errorsCodeFromMotor);
    rnd_fill(inSide.motorErrInfor, sizeof inSide.motorErrInfor);
    inSide.outCurrentDispOld  = (Uint16)rnd();
    inSide.gPhiRt             = (int16)rnd();
    inSide.gPhiRtDisp         = (int16)rnd();
    inSide.pmsmRotorPos       = (Uint16)rnd();
    inSide.frqFdb             = (Uint16)rnd();
    inSide.frqFdbDisp         = (Uint16)rnd();
    inSide.frqFdbTmp          = (int32)rnd();
    inSide.frqFdbFlag         = (Uint16)rnd();
    inSide.torqueCurrent      = (Uint16)rnd();
    inSide.enCoderPosition    = (Uint16)rnd();
    inSide.ABZPos             = (Uint16)rnd();
    inSide.ZCount             = (Uint16)rnd();
    inSide.antiVibrateCoeff   = (Uint16)rnd();
    inSide.currentOc          = (Uint16)rnd();
    inSide.currentPu          = (Uint16)rnd();
    inSide.spdLoopOut         = (Uint16)rnd();
    inSide.softOCTicker       = (Uint32)rnd();
    inSide.powerAddDec        = (Uint16)rnd();
}

static void world_begin(unsigned long seed)
{
    g_rng = seed ? seed : 1UL;

    rnd_fill((void *)&inW.eqep, sizeof inW.eqep);

    rnd_fill(inW.ai, sizeof inW.ai);
    inW.bUv            = (Uint16)rnd();
    inW.currentOcDisp  = (Uint16)rnd();

    rnd_fill(&inW.dspMainCmd,  sizeof inW.dspMainCmd);
    rnd_fill(&inW.dspMainCmd1, sizeof inW.dspMainCmd1);
    rnd_fill(&inW.dspStatus,   sizeof inW.dspStatus);
    rnd_fill(&inW.dspSubCmd,   sizeof inW.dspSubCmd);

    inW.frq           = (int32)rnd() - 0x800000L;
    inW.frqCurAim     = (int32)rnd() - 0x800000L;
    inW.frqCurAimFrac = (int32)rnd() - 0x800000L;
    inW.frqDroop      = (int32)rnd() - 0x800000L;
    /* frqPuQ15 是除数, 见文件头 (2): 绝不为 0。 */
    inW.frqPuQ15      = (Uint16)(1 + rnd() % 65535);
    inW.frqRunDisp    = (Uint16)rnd();
    inW.frqTmp        = (int32)rnd() - 0x800000L;

    rnd_fill(&inW.funcCode, sizeof inW.funcCode);

    rnd_fill(inW.motor05, sizeof inW.motor05);
    rnd_fill(inW.motor2ms, sizeof inW.motor2ms);
    rnd_fill(inW.func2ms, sizeof inW.func2ms);

    rnd_fill(&inW.invPara,    sizeof inW.invPara);
    rnd_fill(&inW.invtStatus, sizeof inW.invtStatus);
    inW.itDisp = (Uint16)rnd();

    inW.motorCtrlTuneStatus = (Uint16)rnd();
    rnd_fill(&inW.motorFc, sizeof inW.motorFc);
    /* ratingCurrent 是除数, 见文件头 (2): 绝不为 0。 */
    inW.motorFc.motorPara.elem.ratingCurrent =
        (Uint16)(1 + rnd() % 65535);

    inW.outCurrentDisp = (Uint16)rnd();
    inW.outVoltageDisp = (Uint16)rnd();

    rnd_fill(&inW.runFlag, sizeof inW.runFlag);
    inW.runStatus = (enum RUN_STATUS)(rnd() % 11);

    inW.softOcDoFlag     = (Uint16)rnd();
    inW.torqueCurrentAct = (Uint32)rnd();
    inW.tuneCmd          = (Uint16)rnd();
    inW.upperTorque      = (int16)rnd();
    inW.vfSeparateVol    = (Uint16)rnd();
}

static void case_begin(unsigned long seed)
{
    side_begin(seed);
    world_begin(seed ^ 0x5A5A5A5AUL);
}

/* ---- phases -------------------------------------------------------------- */

/* 0. checkSpeedFdbDir 的状态机 —— **必须第一个跑**。
 *
 * QUITE 是终态 (`if (checkFlag == CHECK_SPEED_DIR_QUITE) return;`), 一旦到达就
 * 永远出不来。所以整条 NO -> START -> END -> QUITE 一个进程只可能走一次, 步序列
 * 必须一次设计到位, 没有"再来一轮"的机会。
 *
 * 可观测的东西有两样, 缺一不可:
 *
 *   (a) **哪一步**写下 funcCode.code.speedFdbDir。compare() 每步都比一次, 而
 *       install_world() 每步都把 funcCode 从 inW 重置 —— 于是"写入时刻"本身
 *       就是可观测量。只看最终值会漏掉一大堆变异 (IF96 / IF102 / IF103 之类
 *       只让 tcnt 多转或少转一次, 最终值往往一样)。
 *   (b) 写下的**值**, 也就是那一刻的 speedDir。
 *
 * 步序列由 build/_sd_sim.py 的状态机仿真逐条挑出来: 它对 18 条变异 (IF91..IF108)
 * 全部可分辨, 且原版最后一次写下的值是 1 (preflight 依赖这一点)。核心约束是
 * speedDir 只在 prod>0 / prod<0 处变, 而**换向会把 tcnt 清零** —— 所以一个
 * "让某变异多转一次" 的窗口只要被后面的换向碰到就白费。每段窗口后面都必须紧跟
 * 同向 (prod==0) 的尾巴, 把差距放大到能改变 END 时刻。
 *
 * 注意乘积用的是**全局** frqTmp 与**参数** frqFdb, 不是同一个东西。
 */
static void sd_step(int run, int32 frqTmp, int32 fdb)
{
    inW.runFlag.bit.run = (Uint16)(run ? 1 : 0);
    inW.frqTmp = frqTmp;
    g_arg = fdb;
    step(FN_SDIR);
}

static void phase_speed_dir(void)
{
    int i;

    /* 预置成一个不可能的值, 好在末尾判定"写"确实发生了。 */
    inW.funcCode.code.speedFdbDir = 0x5A5A;
    inW.runStatus = RUN_STATUS_NORMAL;
    inW.runFlag.all = 0;

    /* (1) NO -> START。第一个调用给 fdb==0, 原版立刻进 START; 把 `(!frqFdb)`
     *     取反 (IF107) 或把 `(!runFlag.bit.run)` 取反 (IF108) 的版本在这里
     *     进不去, 于是整条轨迹都缺掉后面那 2500 次计数。 */
    sd_step(0, 12345, 0);

    /* (2) 两个方向分支 + 换向清零 / 同向累加。中间那次 prod==0 是 IF96 的窗口
     *     (speedDir 为 1 时把 `>0` 写成 `>=0` 会落到 0)。 */
    sd_step(1, 100, 7);      /* prod > 0  -> speedDir = 0, 同向 tcnt 0->1  */
    sd_step(1, -100, 7);     /* prod < 0  -> speedDir = 1, 换向 tcnt -> 0  */
    sd_step(1, 0, 7);        /* prod == 0 -> speedDir 保持 1, 同向 tcnt++  */
    sd_step(1, 100, 7);      /* prod > 0  -> speedDir = 0, 换向 tcnt -> 0  */

    /* (3) prod==0 且 speedDir==0: 这是 IF97 (`<0` 改成 `<=0`) 唯一的窗口。
     *     紧接着再走一次 prod<0 把 speedDir 拉回 1 —— 此刻原版算"换向"(tcnt
     *     清零), IF97 算"同向"(tcnt 累加), 差距换了个符号但留下来了。 */
    sd_step(1, 0, 7);
    sd_step(1, -100, 7);

    /* (4) 尾巴: 一直喂 prod==0, 让 tcnt 单调爬到 2501。此时 speedDir == 1,
     *     于是 IF96 的落点 (speedDir 变 0) 也在这里 —— 它之后每一步都比原版
     *     少 1, 于是 END 晚一步到。下面三个 run=0 探针分别卡在原版 / IF97 /
     *     IF96 三者的 END 时刻之间: 只要谁的 END 时刻偏了, 就会有一步对不上。 */
    for (i = 0; i < 2500; i++)
    {
        sd_step(1, 0, 7);
    }
    sd_step(0, 0, 7);        /* IF97 (tcnt 早一步到) 在这里写下 speedFdbDir */
    sd_step(1, 0, 7);        /* 原版 tcnt 2500 -> 2501 -> END              */
    sd_step(0, 0, 7);        /* 原版写 speedFdbDir = speedDir = 1, 转 QUITE */
    g_sdFinal = (Uint16)SH[1].funcCode.code.speedFdbDir;

    /* (5) QUITE 是终态: 后面无论怎么喂都不许再动。 */
    sd_step(1, 100, 7);
    sd_step(0, 0, 0);
    sd_step(1, -1, 1);
    sd_step(0, 0, 0);
}

/* 1. 显示滞回 outCurrentDispOldTmp 的 `> 15` 边界。
 *
 *   tmp = ((int32)outCurrent * currentPu) >> 8;
 *   if (ABS_INT32(outCurrentDispOldTmp - tmp) > 15) { outCurrentDisp = (tmp+8)>>4;
 *                                                     outCurrentDispOldTmp = tmp; }
 *
 * currentPu 与 outCurrent **必须从函数缓冲区喂** (见 IFACE_FI_* 的注释): 这个
 * 函数一进来就调 copyDataCore2Func2ms(), 会把 outCurrent / currentPu 一起覆盖掉,
 * 用 `inSide.` 设的那两个值根本到不了这里。currentPu = 256 时 tmp == outCurrent,
 * 于是差值可以精确构造。
 *
 * 先把跨调用静态量 outCurrentDispOldTmp 钉到已知的 0: 喂 1000 一定触发
 * (|0-1000| > 15), 再喂 0 一定触发 (|1000-0| > 15)。
 *
 * 之后按 15 / 16 / 31 / 32 走, 这四个点把三条边界全钉住:
 *   tmp=15  |0-15|  == 15  -> `> 15` 不触发, `>= 15` 触发            (IF137)
 *   tmp=16  |0-16|  == 16  -> `> 15` 触发,   `> 16` 不触发            (IF136)
 *   tmp=31  |16-31| == 15  -> 两边都不触发
 *   tmp=32  |16-32| == 16  -> 原版触发; `outCurrentDispOldTmp = tmp + 1`
 *                             的版本算成 15, 不触发                      (IF140)
 */
static void phase_hysteresis(void)
{
    int i;

    case_begin(0x2101);
    inW.runFlag.all = 0;                 /* 关掉 softOC, 免得干扰 */
    inW.funcCode.code.softOCValue = 0;

    updc_in(256, 1000, 0);               /* 强制触发一次, old 钉到 1000 */
    fresh(FN_UPDC);
    updc_in(256, 0, 0);                  /* 再触发一次, old = 0         */
    fresh(FN_UPDC);

    {
        static const Uint16 oc[9] = { 15, 16, 31, 32, 14, 17, 47, 48, 1 };
        for (i = 0; i < 9; i++)
        {
            updc_in(256, oc[i], 0);
            fresh(FN_UPDC);
        }
    }
}

/* 2. softOC: 阈值比较 `>=` 与 ticker 上限。
 *
 *   threshold = softOCValue * ratingCurrent / 1000
 *   if (run && softOCValue && outCurrentDisp >= threshold)
 *       if (softOCTicker < softOCDelay*TIME_UNIT_CURRENT_CHK/CORE_FUNC_PERIOD)
 *           softOCTicker++;
 *       else softOcDoFlag = 1;
 *   else softOCTicker = 0;
 */
static void phase_soft_oc(void)
{
    int i;

    case_begin(0x2102);
    inW.runFlag.all = 0;
    inW.runFlag.bit.run = 1;
    inW.motorFc.motorPara.elem.ratingCurrent = 1000;
    inW.funcCode.code.softOCValue = 100;      /* threshold = 100 */
    inW.funcCode.code.softOCDelay = 4;        /* 上限 = 4*10/2 = 20 */

    /* 归零滞回 (currentPu / outCurrent 走缓冲区, 见 IFACE_FI_* 注释),
     * 然后精确打到 outCurrentDisp = 99 / 100 / 101 */
    updc_in(256, 0, 0);
    fresh(FN_UPDC);

    /* (tmp+8)>>4 == 99  -> tmp in [1576, 1591] */
    updc_in(256, 1584, 0);
    fresh(FN_UPDC);

    /* == 100 -> tmp in [1592, 1607]。这一步打的是 `>=` 的**等号**边界。 */
    updc_in(256, 1600, 0);
    fresh(FN_UPDC);

    /* == 101 */
    updc_in(256, 1616, 0);
    fresh(FN_UPDC);

    /* ticker 上限: 19 -> ++ ; 20 -> flag */
    {
        static const Uint32 tv[6] = { 0, 1, 18, 19, 20, 21 };
        for (i = 0; i < 6; i++)
        {
            inSide.softOCTicker = tv[i];
            updc_in(256, 1600, 0);            /* outCurrentDisp = 100 */
            fresh(FN_UPDC);
        }
    }

    /* softOCValue == 0 短路; runFlag.run == 0 短路 */
    inW.funcCode.code.softOCValue = 0;
    inSide.softOCTicker = 5;
    updc_in(256, 1600, 0);
    fresh(FN_UPDC);
    inW.funcCode.code.softOCValue = 100;
    inW.runFlag.bit.run = 0;
    inSide.softOCTicker = 5;
    updc_in(256, 1600, 0);
    fresh(FN_UPDC);
    inW.runFlag.bit.run = 1;

    /* 阈值随 ratingCurrent 变。rc = 1001 时 threshold 恰好等于 100, 也就是
     * outCurrentDisp 的值 —— 这是 `>=` 与 `>` 唯一分道扬镳的地方。 */
    {
        static const Uint16 rc[5] = { 1, 999, 1000, 1001, 65535 };
        for (i = 0; i < 5; i++)
        {
            inW.motorFc.motorPara.elem.ratingCurrent = rc[i];
            updc_in(256, 1600, 0);
            inSide.softOCTicker = 3;
            fresh(FN_UPDC);
        }
    }
    inW.motorFc.motorPara.elem.ratingCurrent = 1000;
}

/* 3. powerAddup: `outPower > 0`、`powerAddDec > 5000`、`++powerAddupInt >= 3600`
 *    三处边界。
 *
 * 三个输入**全都只能从函数缓冲区喂**: outPower (IFACE_FI_OUT_POWER) 和
 * powerAddupInt (IFACE_FI_POWER_ADDUP_INT, 藏在 group.uf[12] 里, 见那里的长
 * 注释)。写 `inW.funcCode.code.powerAddupInt` 是**死写** —— 函数头一句
 * copyDataCore2Func2ms() 就把它从 gSendToFunctionDataBuff[42] 冲掉了。
 * 这一条当初就是这么漏掉的: 边界全摆对了, 只有 powerAddupInt 没真的送到。
 *
 * 三处都要求输入**恰好**落在边界上, 随机输入永远碰不到:
 *   outPower == 0 且 powerAddDec 已越过 5000 -> `> 0` 与 `>= 0` 分开   (IF154)
 *   powerAddDec + outPower == 5000           -> `> 5000` 与 `>= 5000`  (IF156)
 *   powerAddupInt == 3599 (++ 之后 == 3600)  -> `>= 3600` 与 `> 3600`  (IF158)
 */
static void phase_power_add(void)
{
    int i;

    case_begin(0x2103);
    inW.runFlag.all = 0;
    inW.funcCode.code.softOCValue = 0;

    updc_in(256, 0, 0);
    fresh(FN_UPDC);

    /* outPower 的三态 */
    {
        static const int16 op[5] = { -1, 0, 1, 100, 32767 };
        for (i = 0; i < 5; i++)
        {
            updc_in(256, 0, op[i]);
            inSide.powerAddDec = 0;
            fresh(FN_UPDC);
        }
    }

    /* outPower == 0: `> 0` 整块不进去; `>= 0` 会先 `+= 0` 再判 5000。
     * 只有 powerAddDec 已经越过 5000 时两者才分得开 —— 原版留着 5001,
     * `>= 0` 那一版会把它清成 0 并顺带推进 powerAddupInt。 */
    updc_pai(0);
    inW.funcCode.code.powerAddup = 0x1234;
    updc_in(256, 0, 0);
    inSide.powerAddDec = 5001;
    fresh(FN_UPDC);

    /* powerAddDec + outPower 恰好 5000: `> 5000` 不触发, `>= 5000` 触发 */
    updc_pai(0);
    updc_in(256, 0, 1);
    inSide.powerAddDec = 4999;
    fresh(FN_UPDC);

    /* 真正触发的那一格, 作为对照 */
    updc_pai(0);
    updc_in(256, 0, 1);
    inSide.powerAddDec = 5000;
    fresh(FN_UPDC);

    /* ++powerAddupInt 恰好 == 3600: `>= 3600` 触发, `> 3600` 不触发。
     * powerAddupInt 走缓冲区 (IFACE_FI_POWER_ADDUP_INT), 不能写
     * inW.funcCode.code.powerAddupInt —— 那是死写。 */
    {
        static const Uint16 pi[5] = { 0, 3598, 3599, 3600, 65535 };
        for (i = 0; i < 5; i++)
        {
            updc_pai(pi[i]);
            inW.funcCode.code.powerAddup = 0x1234;
            updc_in(256, 0, 1);
            inSide.powerAddDec = 5000;
            fresh(FN_UPDC);
        }
    }
}

/* 4. frqRun / frqVFRun / frqVFRunRemainder。
 *
 *   frqRun  = ((int32)(int16)speedMotor * frqPuQ15 + (1<<14)) >> 15
 *   VF 时 frqVFRun = (product) >> 15  (不带四舍五入)
 *   两个分支由 `frqVFRun < frqRun` 选, 由于 frqRun 带舍入, 等号可达。
 */
static void phase_frq(void)
{
    int i;

    case_begin(0x2104);
    inW.runFlag.all = 0;
    inW.funcCode.code.softOCValue = 0;
    inSide.currentPu = 256;
    inSide.outCurrent = 0;
    inSide.outPower = 0;
    fresh(FN_UPDC);

    /* VF 模式: 两个分支都要走到 */
    inW.motorFc.motorCtrlMode = FUNCCODE_motorCtrlMode_VF;
    {
        static const Uint16 pu[6] = { 1, 32767, 32768, 32769, 65535, 16384 };
        static const Uint16 sm[6] = { 0, 1, 2, 0x7FFF, 0x8000, 0xFFFF };
        for (i = 0; i < 6; i++)
        {
            inW.frqPuQ15 = pu[i];
            inSide.speedMotor = sm[i];
            inSide.motorRun = sm[(i + 3) % 6];
            fresh(FN_UPDC);
        }
    }

    /* 非 VF: frqVFRunRemainder = 0 */
    inW.motorFc.motorCtrlMode = 0;
    inW.frqPuQ15 = 32768;
    inSide.speedMotor = 1000;
    inSide.motorRun = 1000;
    fresh(FN_UPDC);

    /* 极端乘积: 制造 frqVFRun == frqRun 与 != 的各种情形 */
    {
        static const Uint16 pu[5] = { 1, 2, 3, 100, 65535 };
        static const Uint16 sm[5] = { 0x7FFF, 0x4000, 0x0001, 0x8001, 0x7FFE };
        for (i = 0; i < 5; i++)
        {
            inW.frqPuQ15 = pu[i];
            inSide.speedMotor = sm[i];
            inW.motorFc.motorCtrlMode = FUNCCODE_motorCtrlMode_VF;
            fresh(FN_UPDC);
        }
    }

    /* frqFdb 钳位: |frqFdbDispTmp| > 32000 的三态。
     *
     * 两件事必须同时做对, 少一件这条边界就永远打不到:
     *
     *  (1) motorRun **只能从函数缓冲区喂** (见 IFACE_FI_*)。写
     *      `inSide.motorRun` 是死写 —— copyDataCore2Func2ms() 会把它冲掉。
     *      frqFdbTmp = (motorRun * frqPuQ15 + (1<<14)) >> 15, 取 frqPuQ15=32768
     *      时它恰好等于 motorRun, 所以直接把 motorRun 取到边界值 32000/32001
     *      即可, 不用反解。
     *
     *  (2) frqFdbDispTmp 是 **一阶低通 (t=30, AI_CALC_PERIOD)** 的输出。只调一次
     *      只会得到"从 0 起步的一小步", 永远够不到钳位门槛。必须让输入稳定在
     *      同一个值上迭代到收敛 (out == in, 残差为 0)。收敛是**精确**的:
     *      in 不变时 tmp = (in-outOld+rem)*PERIOD, 当 outOld==in 且 rem==0 时
     *      out 恒等于 in。所以迭代 128 次即可, 留足余量。
     *
     * frqFdbLpf 是函数内 static, 一旦跑过 phase_random 就带上噪声 —— 所以本
     * 相位必须**自建收敛**: 每次 fresh() 不重置 static, 我们就用同一目标值连着
     * 打 128 次, 让低通自己爬到目标。 */
    {
        static const int32 mr[5] = { 32000, 32001, -32000, -32001, 32767 };
        int k;
        for (i = 0; i < 5; i++)
        {
            inW.frqPuQ15 = 32768;
            inW.funcCode.code.runDir = 0;
            inW.funcCode.code.speedFdbDir = 0;
            inW.runFlag.all = 0;
            for (k = 0; k < 160; k++)
            {
                inW.func2ms[IFACE_FI_MOTOR_RUN] = (int16)mr[i];
                inSide.frq2Core = 0x5A5A;
                fresh(FN_UPDC);
            }
        }
    }

    /* ABZPos: QPOSCNT 的低位必须变, 否则 & 0x0000FFFF 测不出东西 */
    {
        static const Uint32 qp[8] = { 0, 1, 2, 0x0000FFFF, 0x00010000,
                                      0x0001FFFF, 0xFFFF0000, 0x12345678 };
        for (i = 0; i < 8; i++)
        {
            inW.eqep.QPOSCNT = qp[i];
            fresh(FN_UPDC);
        }
    }

    /* gPhiRt -> gPhiRtDisp 的符号与饱和 */
    {
        static const int16 gp[7] = { 0, 1, -1, 32767, -32768, 1800, -1800 };
        for (i = 0; i < 7; i++)
        {
            inSide.gPhiRt = gp[i];
            fresh(FN_UPDC);
        }
    }

    /* `speedMotor == frq2Core` -> frqRun = frqDroop  (IF131), 以及紧接着的
     * `|frqRun| > 32000` 钳位 (IF132)。
     *
     * 两件必须做对:
     *   (a) speedMotor **从缓冲区来** (IFACE_FI_SPEED_MOTOR), 写 inSide 是死写;
     *       frq2Core 反过来是**模块全局**, 只能写 inSide。
     *   (b) 判据是**相等**, 随机值永远碰不到。这里直接把两边钉成同一个值,
     *       同时把 frqDroop 摆到 32000/32001 两侧, 让下面的钳位也有边界可打。
     *
     * frqDroop 由 UpdateDataFunc2Core0p5ms 写, 而这里只调 UpdateDataCore2Func
     * —— 所以它整段由 install_side 决定, 可以直接摆。 */
    {
        static const int32 eq[4] = { 0, 1, 32000, 32001 };
        for (i = 0; i < 4; i++)
        {
            inW.func2ms[IFACE_FI_SPEED_MOTOR] = (int16)eq[i];
            inSide.frq2Core = (Uint16)eq[i];
            inW.frqDroop = eq[i] + 1;      /* 与 frqRun 的另一分支区分 */
            fresh(FN_UPDC);

            /* 不等: 走不到 frqRun = frqDroop, 作为负向对照 */
            inW.func2ms[IFACE_FI_SPEED_MOTOR] = (int16)(eq[i] + 1);
            inSide.frq2Core = (Uint16)eq[i];
            inW.frqDroop = eq[i] + 1;
            fresh(FN_UPDC);
        }

        /* 钳位的三态: frqDroop 分别落在 -32001 / -32000 / 0 / 32000 / 32001 */
        {
            static const int32 fd[5] = { -32001, -32000, 0, 32000, 32001 };
            for (i = 0; i < 5; i++)
            {
                inW.func2ms[IFACE_FI_SPEED_MOTOR] = 1000;
                inSide.frq2Core = 1000;
                inW.frqDroop = fd[i];
                fresh(FN_UPDC);
            }
        }
    }
}

/* 5. UpdateDataFunc2Core0p5ms: droop 分支与 frq2Core 更新判据。 */
static void phase_droop_frq2core(void)
{
    int i;

    case_begin(0x2105);
    inW.funcCode.code.softOCValue = 0;
    inW.motorFc.motorPara.elem.ratingCurrent = 1000;
    inSide.outPower = 0;
    inSide.powerAddDec = 0;

    /* droop 条件四要素 */
    {
        static const Uint16 dc[4] = { 0, 1, 100, 65535 };
        static const int32 fq[4]  = { 0, 1, -1, 30000 };
        static const Uint16 gv[3] = { 6499, 6500, 6501 };
        static const Uint32 ta[3] = { 0, 1, 0xFFFFFFFFUL };
        int a, b, c, d;
        for (a = 0; a < 4; a++)
        for (b = 0; b < 4; b++)
        for (c = 0; c < 3; c++)
        for (d = 0; d < 3; d++)
        {
            inW.funcCode.code.droopCtrl = dc[a];
            inW.frq = fq[b];
            inSide.generatrixVoltage = gv[c];
            inW.torqueCurrentAct = ta[d];
            inW.runStatus = RUN_STATUS_NORMAL;
            inSide.frqVFRunRemainder = 0;
            inW.frqPuQ15 = 32768;
            fresh(FN_UP05);
        }
    }

    /* runStatus == RUN_STATUS_TUNE 短路 */
    inW.funcCode.code.droopCtrl = 100;
    inW.frq = 3000;
    inSide.generatrixVoltage = 100;
    inW.torqueCurrentAct = 12345;
    inW.runStatus = RUN_STATUS_TUNE;
    fresh(FN_UP05);

    /* frq2Core 更新判据: 每一项单独翻转, 最后一项打等号边界。
     * frqPuQ15 = 32768 且 frqVFRunRemainder = 0 时 frq2CoreTmp == frqDroop == frq。
     */
    inW.runStatus = RUN_STATUS_NORMAL;
    inW.funcCode.code.droopCtrl = 0;
    inW.frqPuQ15 = 32768;
    inSide.frqVFRunRemainder = 0;
    inW.motorFc.motorCtrlMode = FUNCCODE_motorCtrlMode_VF;
    inW.motorFc.motorPara.elem.motorType = 0;         /* != PMSM */
    inW.runFlag.bit.accDecStatus = DEC_SPEED;

    {
        static const Uint16 ov[2] = { 0, 1 };
        static const Uint16 oc[2] = { 0, 1 };
        int a, b;
        for (a = 0; a < 2; a++)
        for (b = 0; b < 2; b++)
        {
            inW.funcCode.code.ovGain = ov[a];
            inW.funcCode.code.ocGain = oc[b];

            /* speedMotor = 1000, frq = 1000 -> |frq2CoreTmp| == |speedMotor|, >= 成立 */
            inSide.speedMotor = 1000;
            inW.frq = 1000;
            fresh(FN_UP05);

            /* frq = 999 -> 小于 -> >= 不成立 -> frq2Core 被赋值 */
            inW.frq = 999;
            fresh(FN_UP05);

            /* frq = 1001 -> 大于 -> >= 成立 */
            inW.frq = 1001;
            fresh(FN_UP05);
        }
    }

    /* 负值一侧的 ABS_INT16 等号 */
    inW.funcCode.code.ovGain = 1;
    inSide.speedMotor = 0xFFF6;                 /* (int16) = -10 */
    inW.frq = -10;
    fresh(FN_UP05);
    inW.frq = -9;
    fresh(FN_UP05);
    inW.frq = -11;
    fresh(FN_UP05);

    /* 逐项关掉条件, 确认 else 分支 */
    {
        static const Uint16 mcm[3] = { 0, 1, 2 };
        static const Uint16 mt[3]  = { 0, 1, 2 };
        int a, b;
        for (a = 0; a < 3; a++)
        for (b = 0; b < 3; b++)
        {
            inW.motorFc.motorCtrlMode = mcm[a];
            inW.motorFc.motorPara.elem.motorType = mt[b];
            inW.frq = 500;
            inSide.speedMotor = 500;
            fresh(FN_UP05);
        }
    }

    inW.funcCode.code.droopCtrl = 0;
    inW.runStatus = RUN_STATUS_NORMAL;
    inW.frqPuQ15 = 32768;
    inSide.frqVFRunRemainder = 0;
    inW.motorFc.motorCtrlMode = FUNCCODE_motorCtrlMode_VF;
    inW.motorFc.motorPara.elem.motorType = 0;
    inW.funcCode.code.ovGain = 1;
    inW.funcCode.code.ocGain = 0;

    /* 非零余数 + 各种 frqPuQ15, 走除法 */
    {
        static const int32 rem[5] = { 0, 1, -1, 0x7FFF, -32768 };
        static const Uint16 pu[4] = { 1, 2, 32768, 65535 };
        int a, b;
        for (a = 0; a < 5; a++)
        for (b = 0; b < 4; b++)
        {
            inSide.frqVFRunRemainder = rem[a];
            inW.frqPuQ15 = pu[b];
            inW.frq = 1234;
            inSide.speedMotor = 1234;
            fresh(FN_UP05);
        }
    }
    inW.frqPuQ15 = 32768;
}

/* 6. 搬运类函数: 每次换一组非平凡输入。 */
static void phase_copy_blocks(void)
{
    static const int fn[6] = { FN_M05, FN_M2, FN_CF2, FN_C2F2, FN_C2U2, FN_C2F05 };
    unsigned long k;
    int i;

    for (k = 0; k < 40; k++)
    {
        case_begin(0x1000 + k);
        for (i = 0; i < 6; i++)
        {
            fresh(fn[i]);
        }
    }
}

/* 7. UpdateDataFunc2Core2ms: 位域打包。 */
static void phase_bitfields(void)
{
    int i;

    case_begin(0x2107);
    inW.runFlag.all = 0;
    inW.funcCode.code.softOCValue = 0;
    inSide.currentPu = 256;
    inSide.outCurrent = 0;
    inSide.outPower = 0;
    inSide.powerAddDec = 0;
    inW.frqPuQ15 = 32768;

    /* inPhaseLossProtect 的 /10 与 %10 需要跨过每一个十进制位 */
    {
        static const Uint16 ip[12] = { 0, 1, 9, 10, 11, 19, 20, 99, 100,
                                       255, 256, 65535 };
        for (i = 0; i < 12; i++)
        {
            inW.funcCode.code.inPhaseLossProtect = ip[i];
            fresh(FN_UP2);
        }
    }

    /* 位域截断: 每一个被赋值的源都扫 0 / 边界 / 溢出值 */
    {
        static const Uint16 v[7] = { 0, 1, 2, 3, 7, 0x7FFF, 0xFFFF };
        for (i = 0; i < 7; i++)
        {
            inW.motorFc.motorCtrlMode = v[i];
            inW.motorFc.vcPara.spdLoopI = v[i];
            inW.motorFc.pgPara.elem.fvcPgSrc = v[i];
            inW.runFlag.bit.accDecStatus = v[i];
            inW.funcCode.code.torqueCtrl = v[i];
            inW.funcCode.code.outPhaseLossProtect = v[i];
            inW.funcCode.code.overloadMode = v[i];
            inW.funcCode.code.loseLoadProtectMode = v[i];
            inW.funcCode.code.pOffTransitoryNoStop = v[i];
            inW.funcCode.code.cbcEnable = v[i];
            inW.funcCode.code.varFcByTem = v[i];
            inW.funcCode.code.modulationMode = v[i];
            inW.funcCode.code.deadCompMode = v[i];
            inW.funcCode.code.frqPoint = v[i];
            fresh(FN_UP2);
        }
    }

    /* dspMainCmd.bit.motorCtrlMode 是 2 位, 源是 motorFc.motorCtrlMode */
    {
        static const Uint16 mcm[6] = { 0, 1, 2, 3, 4, 0xFFFF };
        for (i = 0; i < 6; i++)
        {
            inW.motorFc.motorCtrlMode = mcm[i];
            fresh(FN_UP2);
        }
    }
}

/* 8. 主循环次序的锁步序列: 让 2ms/0.5ms/0.05ms 三个函数交替跑, 把
 *    frqFdbLpf / outCurrentDispOldTmp 这类跨调用的静态量喂出轨迹。 */
static void phase_sequence(void)
{
    unsigned long k;
    int i;

    case_begin(0x9001);
    inW.funcCode.code.softOCValue = 100;
    inW.funcCode.code.softOCDelay = 4;
    inW.motorFc.motorPara.elem.ratingCurrent = 1000;
    inW.motorFc.motorCtrlMode = FUNCCODE_motorCtrlMode_VF;
    inW.runFlag.all = 0;
    inW.runFlag.bit.run = 1;

    for (k = 0; k < 300; k++)
    {
        /* 让滤波器的输入缓慢变化, 走出 outOld / remainder 的非平凡轨迹 */
        inSide.motorRun = (Uint16)(0x8000 + (k * 37) % 40000 - 20000);
        inW.frqPuQ15 = (Uint16)(32000 + (k % 100));
        inSide.outCurrent = (Uint16)(1600 + (k % 200));
        inSide.currentPu = 256;
        inSide.generatrixVoltage = (Uint16)(6000 + (k % 1000));
        inW.frq = (int32)(3000 + (k % 500));
        inW.torqueCurrentAct = (Uint32)(k * 7919);
        inW.funcCode.code.droopCtrl = (Uint16)(k % 3);

        fresh(FN_UP2);
        fresh(FN_UP05);
        for (i = 0; i < 3; i++)
        {
            fresh(FN_UPDC);
        }
        fresh(FN_M05);
    }
}

/* 9. 随机。 */
static void phase_random(long n)
{
    long k;
    int i;

    for (k = 0; k < n; k++)
    {
        case_begin((unsigned long)(k * 2654435761UL + 12345UL));
        for (i = 0; i < FN_N; i++)
        {
            fresh(i);
        }
    }
}

/* ---- main ---------------------------------------------------------------- */

static void preflight(void)
{
    /* (1) memcpy 保真补丁是否生效: 40 个字的块必须整段搬过去。 */
    int i;
    int ok = 1;

    for (i = 0; i < 40; i++)
    {
        funcCode.group.cf[i] = (Uint16)(0x1000 + i);
    }
    for (i = 0; i < IFACE_MOTOR_2MS_NUM; i++)
    {
        gSendToMotor2MsDataBuff[i] = -1;
    }
    ref_copyDataFunc2CF2ms();
    if ((Uint16)gSendToMotor2MsDataBuff[75] != 0x1000 ||
        (Uint16)gSendToMotor2MsDataBuff[114] != 0x1027)
    {
        ok = 0;
    }
    printf("preflight: memcpy 字数保真 %s (buff[75]=0x%04X buff[114]=0x%04X)\n",
           ok ? "OK" : "**FAIL**",
           (unsigned)(Uint16)gSendToMotor2MsDataBuff[75],
           (unsigned)(Uint16)gSendToMotor2MsDataBuff[114]);
    if (!ok)
    {
        g_fails++;
    }

    /* (2) 状态机是否真的走到了 END -> QUITE 并写下 speedFdbDir。
     *     phase_speed_dir 把它预置成 0x5A5A, 走到 QUITE 才会变成 1。 */
    if (g_sdFinal != 1)
    {
        printf("preflight: **FAIL** checkSpeedFdbDir 未走到 END->QUITE "
               "(speedFdbDir=0x%04X, 期望 0x0001)\n", (unsigned)g_sdFinal);
        g_fails++;
    }
    else
    {
        printf("preflight: checkSpeedFdbDir 走到 QUITE 并写下 speedFdbDir=1 "
               "(tcnt 2500 不转, 2501 转)\n");
    }
}

int main(int argc, char **argv)
{
    long nrand = argc > 1 ? atol(argv[1]) : 20000;

    init_ptrs();

    phase_speed_dir();          /* 必须第一: QUITE 是终态 */
    preflight();                /* 用它留下的最终状态做自检 */

    phase_hysteresis();
    phase_soft_oc();
    phase_power_add();
    phase_frq();
    phase_droop_frq2core();
    phase_copy_blocks();
    phase_bitfields();
    phase_sequence();
    phase_random(nrand);

    printf("cases=%ld mismatches=%d\n", g_cases, g_fails);
    return g_fails != 0;
}
