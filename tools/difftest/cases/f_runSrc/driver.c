/* Differential test driver for f_runSrc.
 *
 * The module is the run-command / run-state machine of the drive. It is the
 * largest module in the T1 tier (2852 words of .text in the linked map) and it
 * owns the whole "what should the drive do next" decision chain:
 *
 *   UpdateRunCmd()          turn panel keys, DI terminals and the serial/PLC
 *                           run command into runCmd.bit.* -- and clear the
 *                           transient sources it just consumed.
 *   RunSrcDeal()            the periodic task: UpdateRunCmd() then
 *                           RunSrcUpdateFrq().
 *   RunSrcUpdateFrq()       pick runStatus from the run command, the DI
 *                           brakes, lose-load and jog; run the acc/dec chain;
 *                           dispatch to the per-status controllers; publish
 *                           frq = frqTmp, frqFrac = frqTmpFrac.
 *   StartRunCtrl()          START_RUN_STATUS_* state machine (speed track,
 *                           start brake, pre-flux, hold start frequency).
 *   NormalRunCtrl()         the running state: lower-frequency rule, swing,
 *                           and the acc/dec call.
 *   StopRunCtrl()           STOP_RUN_STATUS_* state machine (dec stop, wait
 *                           brake, brake).
 *   JogRunCtrl()            jog run, including jog-when-running.
 *   TuneRunCtrl()           motor auto-tune state machine.
 *   LoseLoadRunCtrl()       lose-load (belt break) handling.
 *   ShutDownRunCtrl()       power-off sequence and PLC step bookkeeping.
 *   UpdateSwingPara()       swing amplitude/period -> swingAccTime/DecTime.
 *   SwingDeal()             swing up/down state machine.
 *   LowerThanLowerFrqDeal() the lower-frequency rule.
 *   PowerOffNoStopDeal()    power-off ride-through.
 *   AccDecTimeCalc()        choose accFrqTime/decFrqTime from the 4 sources.
 *   DormantDeal()           the sleep/wake detector.
 *   UpdateBenchTime()       accDecTimeUnit -> timeBench.
 *   setTimeRun()            the timed-run countdown.
 *   runTimeCal()            the running-hours / power-on-hours counters.
 *
 * Nothing here touches a peripheral register. The module's whole hardware
 * boundary is four functions it calls, which the shared mock implements:
 * AccDecFrqCalc (f_runSrc_accDecFrq.c), UpdateFrqAim (f_frqSrc.c),
 * GetErrorAttribute (f_error.c) and SaveTuneData (f_invPara.c).
 * See mock_runsrc.c for why each stub does as little as it can.
 *
 * ---- what is compared ----
 *
 * After every call the driver compares, field by field:
 *
 *   * all 36 scalar module globals and the 9 16-bit unions, via the FIELDS()
 *     list below;
 *   * the 26 external globals the module *writes*, snapshotted per side
 *     because both sides write the same object (SH below);
 *   * the mock call trace (which stub, which arguments, in which order).
 *
 * ---- state the module carries, and how the driver reaches it ----
 *
 * File-scope state is per-side and therefore both an input and an output: the
 * driver installs it before the call and reads it back after. That is
 * deliberate -- a rewrite that stops updating one of them shows up immediately.
 *
 * Function-local statics are NOT reachable from the driver. They are:
 *
 *   UpdateRunCmd      bMfkSwitch, fwdOld, revOld, fJogOld, rJogOld,
 *                     startProtectSrcOld, otherStopSrcOld,
 *                     otherStopLowerSrcOld, bJog, bRun, jogCmdPulse, runSrcBak
 *   RunSrcUpdateFrq   errorLevelOld, runOldFrqFlag, frqOld
 *   TuneRunCtrl       stopFlag
 *   SwingDeal         frqAimOld, frqDeltaOldMax
 *   AccDecTimeCalc    stop4DecFlag
 *   DormantDeal       stopTicker, startTicker
 *
 * They are not a hole in the test: both sides run the *same* call sequence from
 * the same start state, so their statics evolve identically as long as the two
 * implementations are equivalent. What the driver must never do is give the two
 * sides different call sequences -- run_case() below is written so that it
 * cannot. The consequence for the *reader* is that the phases are order
 * dependent: the first case that touches UpdateRunCmd sees the statics cold,
 * every later one sees them warm. That is why the widest sweeps run first.
 *
 * ---- pre-existing gaps that are NOT tested ----
 *
 * Listed rather than quietly dropped. None is reachable from a valid
 * function-code table, and "fixing" one would be a behaviour change, not a
 * rewrite:
 *
 *   a. `UpdateSwingPara` divides by `swingDoubleAw - swingJumpFrq`, which is 0
 *      when swingAmplitude is 0 (swingAw becomes 0) and swingJumpRange is 0.
 *      The driver keeps every swing configuration strictly positive -- see
 *      swing_safe() -- rather than "fixing" the module.
 *   b. `UpdateSwingPara` also divides by `timeBench`, which is 0 until
 *      AccDecTimeCalc has run once. The driver keeps timeBench in {10,100,1000},
 *      the three values UpdateBenchTime can produce.
 *   c. `TuneRunCtrl` divides by `motorFc.motorPara.elem.ratingSpeed`, guarded by
 *      `ratingSpeed > PMSM_TUNE_SPEED`. The driver never violates the guard.
 *   d. `frqLine.calc` is left NULL in the mock. The module only ever writes
 *      `frqLine.remainder` and never calls through the pointer; the real
 *      initialiser lives in f_runSrc_accDecFrq.c, which this case does not link.
 *      Documented in mock_runsrc.c.
 *   e. `runStatusOld4POffNoStop` and friends are plain enums, so an out-of-range
 *      value installed by the driver simply round-trips. The driver only uses
 *      values that exist in the enum.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

/* Same include order as src/function/f_runSrc.c. */
#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_io.h"
#include "f_comm.h"
#include "f_ui.h"
#include "f_main.h"
#include "f_menu.h"
#include "f_eeprom.h"
#include "f_posCtrl.h"

/* MOTOR_TYPE_IM / MOTOR_TYPE_VARFREQ live in include/common/MotorDefine.h, but
 * that header pulls in SystemDefine.h -> DSP2803x_Device.h, which is not on this
 * case's include path (the device headers are only reached through the real
 * translation units). The two values are firmware constants that the module
 * compares against; duplicated here with the source of truth named, and pinned
 * by the differential test itself -- if they ever moved, the motor-type sweep
 * would stop exercising the IM branch and the negative control would notice. */
#define MOTOR_TYPE_IM       0
#define MOTOR_TYPE_VARFREQ  1

#include "mock_runsrc.h"
#include "runsrc_sides.h"

#define NEL(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* ---------------------------------------------------------------------------
 * The two sides
 * ------------------------------------------------------------------------ */

#define PS(n)   (side ? &new_##n : &ref_##n)

/* Scalar module globals.  (type, name) -- order is the comparison order. */
#define SFIELDS(X)                                   \
    X(Uint16, startProtectSrc)                       \
    X(Uint16, otherStopSrc)                          \
    X(Uint16, otherStopLowerSrc)                     \
    X(enum RUN_STATUS, runStatus)                    \
    X(enum START_RUN_STATUS, startRunStatus)         \
    X(enum STOP_RUN_STATUS, stopRunStatus)           \
    X(enum RUN_STATUS, runStatusOld4LoseLoad)        \
    X(enum RUN_STATUS, runStatusOld4Jog)             \
    X(enum RUN_STATUS, runStatusOld4POffNoStop)      \
    X(Uint16, runSrc)                                \
    X(Uint32, accFrqTime)                            \
    X(Uint32, decFrqTime)                            \
    X(enum SWING_STATUS, swingStatus)                \
    X(Uint32, swingDoubleAw)                         \
    X(int32, swingMaxFrq)                            \
    X(int32, swingMinFrq)                            \
    X(int32, swingJumpFrq)                           \
    X(Uint32, swingAccTime)                          \
    X(Uint32, swingDecTime)                          \
    X(Uint16, swingFrqLimit)                         \
    X(Uint16, setRunLostTime)                        \
    X(Uint16, setRunTimeAim)                         \
    X(Uint32, runTimeTicker)                         \
    X(Uint32, lowerDelayTicker)                      \
    X(Uint32, shuntTicker)                           \
    X(Uint16, runTimeAddupTicker)                    \
    X(Uint16, powerUpTimeAddupTicker)                \
    X(Uint16, shuntFlag)                             \
    X(Uint16, accDecTimeSrcPlc)                      \
    X(Uint16, stopRemFlag)                           \
    X(Uint16, runDirPanelOld)                        \
    X(Uint16, bRunJog)                               \
    X(Uint16, tuneCmd)                               \
    X(Uint16, saveTuneDataFlag)                      \
    X(enum DORMANT_STATUS, dormantStatus)            \
    X(Uint16, motorCtrlTuneStatus)

/* The 9 unions.  Compared with memcmp, not != -- they have no operator.
 * Each is installed through its `all` member, so every padding bit is
 * deterministic and memcmp cannot produce a false positive. */
#define UFIELDS(X)                                   \
    X(union RUN_CMD, runCmd)                         \
    X(union RUN_FLAG, runFlag)                       \
    X(union RUN_STATUS_FIRST_STEP, runStatus1Step)   \
    X(union DSP_MAIN_COMMAND, dspMainCmd)            \
    X(union DSP_MAIN_COMMAND1, dspMainCmd1)          \
    X(union DSP_SUB_COMMAND, dspSubCmd)              \
    X(union DSP_STATUS, dspStatus)                   \
    X(union DSP_SUB_STATUS, dspSubStatus)            \
    X(union INVT_STATUS, invtStatus)

struct ST
{
#define DECL(t, n) t n;
    SFIELDS(DECL)
    UFIELDS(DECL)
#undef DECL
};

static void capture(int side, struct ST *s)
{
#define CAP(t, n) s->n = *PS(n);
    SFIELDS(CAP)
    UFIELDS(CAP)
#undef CAP
}

static void install(int side, const struct ST *s)
{
#define INS(t, n) *PS(n) = s->n;
    SFIELDS(INS)
    UFIELDS(INS)
#undef INS
}

/* ---------------------------------------------------------------------------
 * Shared globals the module writes
 *
 * These 26 objects live in *other* translation units. Both sides write the same
 * instance, so each side needs its own copy restored before the call. The
 * driver's apply_inputs() runs first and establishes every read-only input;
 * sh_write() then overwrites the 26 written ones (including the five fields
 * inside funcCode), and sh_read() snapshots them afterwards.
 * ------------------------------------------------------------------------ */

#define SHFIELDS(X)                                  \
    X(int32,  accTimeOld)                            \
    X(int32,  decTimeOld)                            \
    X(int32,  frqCurAimOld)                          \
    X(Uint16, bFrqCurAimChg)                         \
    X(int32,  frq)                                   \
    X(int32,  frqTmp)                                \
    X(int32,  frqFrac)                               \
    X(int32,  frqTmpFrac)                            \
    X(int32,  frqCurAim)                             \
    X(int32,  frqAimTmp0)                            \
    X(int32,  frqAimOld4Dir)                         \
    X(int32,  upDownFrq)                             \
    X(Uint16, plcStep)                               \
    X(Uint32, plcTime)                               \
    X(Uint16, plcStepRemOld)                         \
    X(Uint32, plcTimeRemOld)                         \
    X(Uint16, bStopPlc)                              \
    X(Uint16, timeBench)                             \
    X(Uint16, keyFunc)                               \
    X(Uint16, commRunCmd)                            \
    X(Uint16, errorOther)

struct SH
{
#define DECL(t, n) t n;
    SHFIELDS(DECL)
#undef DECL
    union FRQ_FLAG frqFlag;
    struct CUR_TIME curTime;
    enum MENU0_DISP_STATUS menu0DispStatus;
    LINE_CHANGE_STRUCT frqLine;

    /* The five fields the module writes inside the shared funcCode. They are
     * kept out of SHFIELDS because their struct member names differ from the
     * funcCode paths they map onto, so sh_read/sh_write handle them explicitly. */
    Uint16 fcPlcCmd;
    Uint16 fcPowerUpTimeAddup;
    Uint16 fcPowerUpTimeAddupSec;
    Uint16 fcRunTimeAddup;
    Uint16 fcRunTimeAddupSec;
};

static void sh_read(struct SH *s)
{
#define RD(t, n) s->n = n;
    SHFIELDS(RD)
#undef RD
    s->frqFlag          = frqFlag;
    s->curTime          = curTime;
    s->menu0DispStatus  = menu0DispStatus;
    s->frqLine          = frqLine;

    s->fcPlcCmd               = funcCode.code.plcCmd;
    s->fcPowerUpTimeAddup     = funcCode.code.powerUpTimeAddup;
    s->fcPowerUpTimeAddupSec  = funcCode.code.powerUpTimeAddupSec;
    s->fcRunTimeAddup         = funcCode.code.runTimeAddup;
    s->fcRunTimeAddupSec      = funcCode.code.runTimeAddupSec;
}

static void sh_write(const struct SH *s)
{
#define WR(t, n) n = s->n;
    SHFIELDS(WR)
#undef WR
    frqFlag         = s->frqFlag;
    curTime         = s->curTime;
    menu0DispStatus = s->menu0DispStatus;
    frqLine         = s->frqLine;

    funcCode.code.plcCmd               = s->fcPlcCmd;
    funcCode.code.powerUpTimeAddup     = s->fcPowerUpTimeAddup;
    funcCode.code.powerUpTimeAddupSec  = s->fcPowerUpTimeAddupSec;
    funcCode.code.runTimeAddup         = s->fcRunTimeAddup;
    funcCode.code.runTimeAddupSec      = s->fcRunTimeAddupSec;
}

/* ---------------------------------------------------------------------------
 * Inputs
 * ------------------------------------------------------------------------ */

struct IN
{
    /* carried state installed before each call */
    struct ST st;
    struct SH sh0;

    /* function code */
    Uint16 accDecSpdCurve, accDecTimeUnit;
    Uint16 accTime1, accTime2, accTime3, accTime4;
    Uint16 decTime1, decTime2, decTime3, decTime4;
    Uint16 accTimefrqChgValue, decTimefrqChgValue;
    Uint16 diControlMode, errorRunFrqSrc, errorSecondFrq, frqRemMode;
    Uint16 jogAccTime, jogDecTime, jogWhenRun, loseLoadProtectMode, lowerDeal;
    Uint16 fcMaxFrq, mfkKeyFunc, ocGain, ovGain;
    Uint16 pOffThresholdVol, pOffTransitoryFrqDecSlope, pOffTransitoryNoStop;
    Uint16 pOffVolBackTime;
    Uint16 pidCalcMode, plcEnable, plcCmd, plcPowerOffRemMode;
    Uint16 runDir, fcRunSrc;
    Uint16 setTimeArrive, setTimeMode, setTimeSource, setTimeValue;
    Uint16 startBrakeTime, startFrq, startFrqTime, startMode, startProtect;
    Uint16 stopBrakeFrq, stopBrakeTime, stopBrakeWaitTime, stopKeyFunc, stopMode;
    Uint16 swingAmplitude, swingBaseMode, swingJumpRange, swingPeriod;
    Uint16 swingRiseTimeCoeff;
    Uint16 wakeUpFrq, powerUpTimeArriveSet, runTimeArriveSet;
    Uint16 f9_31, f9_32, f9_33, f9_34;

    /* other modules */
    Uint16 motorCtrlMode, accDecTimeMotor, motorType, ratingFrq, ratingSpeed;
    int16  aiSet[AI_NUMBER];
    int16  ai1Voltage;
    Uint16 ratingVoltage;
    Uint32 diF1, diF2, diSelF1;
    Uint16 errorLevel, errorCode;
    int32  frqAim, frqAimTmp, frqFdbTmp, frqRun;
    Uint16 generatrixVoltage, lowerFrq, maxFrq, upperFrq, runMode, bAntiReverseRun;
    Uint16 funcCodeRwMode, mainLoopTicker, motorSn, powerOnStatus;

    /* mock driving values */
    Uint16 mockErrorAttrLevel;
};

/* ---- field offsets, so one sweep helper covers every field ---- */

#define ST_OFF(f) (offsetof(struct IN, st) + offsetof(struct ST, f))
#define SH_OFF(f) (offsetof(struct IN, sh0) + offsetof(struct SH, f))

static void put16(void *p, size_t off, Uint16 v)  { memcpy((char *)p + off, &v, sizeof v); }
static void put32(void *p, size_t off, Uint32 v)  { memcpy((char *)p + off, &v, sizeof v); }
static void puti32(void *p, size_t off, int32 v)  { memcpy((char *)p + off, &v, sizeof v); }
static void putenum(void *p, size_t off, Uint32 v)
{
    /* Host enums are 4 bytes (MSVC uses int). Writing all four bytes keeps the
     * high half at 0 instead of leaving whatever the template had. */
    put32(p, off, v);
}

static void apply_inputs(const struct IN *in)
{
    int i;

    memset(&funcCode, 0, sizeof(funcCode));
    funcCode.code.accDecSpdCurve         = in->accDecSpdCurve;
    funcCode.code.accDecTimeUnit         = in->accDecTimeUnit;
    funcCode.code.accTime1               = in->accTime1;
    funcCode.code.accTime2               = in->accTime2;
    funcCode.code.accTime3               = in->accTime3;
    funcCode.code.accTime4               = in->accTime4;
    funcCode.code.decTime1               = in->decTime1;
    funcCode.code.decTime2               = in->decTime2;
    funcCode.code.decTime3               = in->decTime3;
    funcCode.code.decTime4               = in->decTime4;
    funcCode.code.accTimefrqChgValue     = in->accTimefrqChgValue;
    funcCode.code.decTimefrqChgValue     = in->decTimefrqChgValue;
    funcCode.code.diControlMode          = in->diControlMode;
    funcCode.code.errorRunFrqSrc         = in->errorRunFrqSrc;
    funcCode.code.errorSecondFrq         = in->errorSecondFrq;
    funcCode.code.frqRemMode             = in->frqRemMode;
    funcCode.code.jogAccTime             = in->jogAccTime;
    funcCode.code.jogDecTime             = in->jogDecTime;
    funcCode.code.jogWhenRun             = in->jogWhenRun;
    funcCode.code.loseLoadProtectMode    = in->loseLoadProtectMode;
    funcCode.code.lowerDeal              = in->lowerDeal;
    funcCode.code.maxFrq                 = in->fcMaxFrq;
    funcCode.code.mfkKeyFunc             = in->mfkKeyFunc;
    funcCode.code.ocGain                 = in->ocGain;
    funcCode.code.ovGain                 = in->ovGain;
    funcCode.code.pOffThresholdVol       = in->pOffThresholdVol;
    funcCode.code.pOffTransitoryFrqDecSlope = in->pOffTransitoryFrqDecSlope;
    funcCode.code.pOffTransitoryNoStop   = in->pOffTransitoryNoStop;
    funcCode.code.pOffVolBackTime        = in->pOffVolBackTime;
    funcCode.code.pidCalcMode            = in->pidCalcMode;
    funcCode.code.plcEnable              = in->plcEnable;
    funcCode.code.plcCmd                 = in->plcCmd;
    funcCode.code.plcPowerOffRemMode     = in->plcPowerOffRemMode;
    funcCode.code.runDir                 = in->runDir;
    funcCode.code.runSrc                 = in->fcRunSrc;
    funcCode.code.setTimeArrive          = in->setTimeArrive;
    funcCode.code.setTimeMode            = in->setTimeMode;
    funcCode.code.setTimeSource          = in->setTimeSource;
    funcCode.code.setTimeValue           = in->setTimeValue;
    funcCode.code.startBrakeTime         = in->startBrakeTime;
    funcCode.code.startFrq               = in->startFrq;
    funcCode.code.startFrqTime           = in->startFrqTime;
    funcCode.code.startMode              = in->startMode;
    funcCode.code.startProtect           = in->startProtect;
    funcCode.code.stopBrakeFrq           = in->stopBrakeFrq;
    funcCode.code.stopBrakeTime          = in->stopBrakeTime;
    funcCode.code.stopBrakeWaitTime      = in->stopBrakeWaitTime;
    funcCode.code.stopKeyFunc            = in->stopKeyFunc;
    funcCode.code.stopMode               = in->stopMode;
    funcCode.code.swingAmplitude         = in->swingAmplitude;
    funcCode.code.swingBaseMode          = in->swingBaseMode;
    funcCode.code.swingJumpRange         = in->swingJumpRange;
    funcCode.code.swingPeriod            = in->swingPeriod;
    funcCode.code.swingRiseTimeCoeff     = in->swingRiseTimeCoeff;
    funcCode.code.wakeUpFrq              = in->wakeUpFrq;
    funcCode.code.powerUpTimeArriveSet   = in->powerUpTimeArriveSet;
    funcCode.code.runTimeArriveSet       = in->runTimeArriveSet;
    funcCode.group.f9[31]                = in->f9_31;
    funcCode.group.f9[32]                = in->f9_32;
    funcCode.group.f9[33]                = in->f9_33;
    funcCode.group.f9[34]                = in->f9_34;

    motorFc.motorCtrlMode               = in->motorCtrlMode;
    motorFc.accDecTimeMotor             = in->accDecTimeMotor;
    motorFc.motorPara.elem.motorType    = in->motorType;
    motorFc.motorPara.elem.ratingFrq    = in->ratingFrq;
    motorFc.motorPara.elem.ratingSpeed  = in->ratingSpeed;

    for (i = 0; i < AI_NUMBER; i++)
    {
        aiDeal[i].set = in->aiSet[i];
    }
    aiDeal[0].voltage = in->ai1Voltage;

    invPara.ratingVoltage = in->ratingVoltage;

    memset(&diFunc, 0, sizeof(diFunc));
    memset(&diSelectFunc, 0, sizeof(diSelectFunc));
    diFunc.f1.all       = in->diF1;
    diFunc.f2.all       = in->diF2;
    diSelectFunc.f1.all = in->diSelF1;

    /* errorAttribute is a union: bit 0 is `reset`, bits 1-3 are `level`
     * (struct ERROR_ATTRIBUTE_BITS in f_error.h). The driver's `errorLevel`
     * carries the *level*, so it has to be written into the bitfield. Writing it
     * into `.all` shifted every value right by one -- ERROR_LEVEL_RUN (3) landed
     * on level 1 -- which made the whole `switch (errorRunFrqSrc)` under
     * ERROR_LEVEL_RUN, and the STOP/RUN arms of UpdateRunCmd's error handling,
     * unreachable. */
    errorAttribute.all = 0;
    errorAttribute.bit.level = (Uint16)(in->errorLevel & 0x7u);
    errorCode          = in->errorCode;

    frqAim      = in->frqAim;
    frqAimTmp   = in->frqAimTmp;
    frqFdbTmp   = in->frqFdbTmp;
    frqRun      = in->frqRun;
    generatrixVoltage = in->generatrixVoltage;
    lowerFrq    = in->lowerFrq;
    maxFrq      = in->maxFrq;
    upperFrq    = in->upperFrq;
    runMode     = in->runMode;
    bAntiReverseRun = in->bAntiReverseRun;

    funcCodeRwMode  = in->funcCodeRwMode;
    mainLoopTicker  = in->mainLoopTicker;
    motorSn         = (enum MOTOR_SN) in->motorSn;
    powerOnStatus   = (enum POWER_ON_STATUS) in->powerOnStatus;

    mockErrorAttrLevel = in->mockErrorAttrLevel;
}

/* ---------------------------------------------------------------------------
 * Running one case
 * ------------------------------------------------------------------------ */

#define E_UPDATE_RUN_CMD     0
#define E_SET_TIME_RUN       1
#define E_RUN_TIME_CAL       2
#define E_RUN_SRC_DEAL       3
#define E_UPDATE_BENCH_TIME  4
#define E_RUN_SRC_UPD_FRQ    5
#define E_START_RUN_CTRL     6
#define E_NORMAL_RUN_CTRL    7
#define E_STOP_RUN_CTRL      8
#define E_JOG_RUN_CTRL       9
#define E_TUNE_RUN_CTRL      10
#define E_LOSE_LOAD_CTRL     11
#define E_SHUT_DOWN_CTRL     12
#define E_UPDATE_SWING_PARA  13
#define E_SWING_DEAL         14
#define E_LOWER_FRQ          15
#define E_POWER_OFF_NO_STOP  16
#define E_ACC_DEC_TIME_CALC  17
#define E_DORMANT_DEAL       18

static void invoke(int side, int entry)
{
    switch (entry)
    {
        case E_UPDATE_RUN_CMD:
            if (side) new_UpdateRunCmd();     else ref_UpdateRunCmd();     break;
        case E_SET_TIME_RUN:
            if (side) new_setTimeRun();       else ref_setTimeRun();       break;
        case E_RUN_TIME_CAL:
            if (side) new_runTimeCal();       else ref_runTimeCal();       break;
        case E_RUN_SRC_DEAL:
            if (side) new_RunSrcDeal();       else ref_RunSrcDeal();       break;
        case E_UPDATE_BENCH_TIME:
            if (side) new_UpdateBenchTime();  else ref_UpdateBenchTime();  break;
        case E_RUN_SRC_UPD_FRQ:
            if (side) new_RunSrcUpdateFrq();  else ref_RunSrcUpdateFrq();  break;
        case E_START_RUN_CTRL:
            if (side) new_StartRunCtrl();     else ref_StartRunCtrl();     break;
        case E_NORMAL_RUN_CTRL:
            if (side) new_NormalRunCtrl();    else ref_NormalRunCtrl();    break;
        case E_STOP_RUN_CTRL:
            if (side) new_StopRunCtrl();      else ref_StopRunCtrl();      break;
        case E_JOG_RUN_CTRL:
            if (side) new_JogRunCtrl();       else ref_JogRunCtrl();       break;
        case E_TUNE_RUN_CTRL:
            if (side) new_TuneRunCtrl();      else ref_TuneRunCtrl();      break;
        case E_LOSE_LOAD_CTRL:
            if (side) new_LoseLoadRunCtrl();  else ref_LoseLoadRunCtrl();  break;
        case E_SHUT_DOWN_CTRL:
            if (side) new_ShutDownRunCtrl();  else ref_ShutDownRunCtrl();  break;
        case E_UPDATE_SWING_PARA:
            if (side) new_UpdateSwingPara();  else ref_UpdateSwingPara();  break;
        case E_SWING_DEAL:
            if (side) new_SwingDeal();        else ref_SwingDeal();        break;
        case E_LOWER_FRQ:
            if (side) new_LowerThanLowerFrqDeal(); else ref_LowerThanLowerFrqDeal(); break;
        case E_POWER_OFF_NO_STOP:
            if (side) new_PowerOffNoStopDeal(); else ref_PowerOffNoStopDeal(); break;
        case E_ACC_DEC_TIME_CALC:
            if (side) new_AccDecTimeCalc();   else ref_AccDecTimeCalc();   break;
        case E_DORMANT_DEAL:
            if (side) new_DormantDeal();      else ref_DormantDeal();      break;
        default:
            break;
    }
}

static long g_cases;
static int  g_fails;
static int  g_trace_on;
static const char *g_phase = "";

static void report(const char *field, const char *detail)
{
    if (g_fails < 40)
    {
        printf("MISMATCH %s: %s %s\n", g_phase, field, detail);
    }
    g_fails++;
}

/* Compare the 26 written externals. Field by field, not memcmp: frqLine holds
 * a function pointer, so its host layout depends on the pointer size and
 * memcmp would compare padding. */
static void sh_cmp(const struct SH *a, const struct SH *b)
{
    int bad = 0;
    char buf[256];

#define CMPF(t, n)                                                       \
    if (a->n != b->n) {                                                  \
        sprintf(buf, "ref=%lld new=%lld", (long long)a->n, (long long)b->n); \
        report(#n, buf); bad = 1;                                        \
    }
    SHFIELDS(CMPF)
#undef CMPF

    if (a->frqFlag.all != b->frqFlag.all)
    {
        sprintf(buf, "ref=%04x new=%04x", a->frqFlag.all, b->frqFlag.all);
        report("frqFlag", buf); bad = 1;
    }
    if (a->curTime.runTimeM      != b->curTime.runTimeM      ||
        a->curTime.runTimeSec    != b->curTime.runTimeSec    ||
        a->curTime.powerOnTimeM  != b->curTime.powerOnTimeM  ||
        a->curTime.powerOnTimeSec!= b->curTime.powerOnTimeSec||
        a->curTime.runTime       != b->curTime.runTime       ||
        a->curTime.powerOnTime   != b->curTime.powerOnTime)
    {
        report("curTime", "struct differs"); bad = 1;
    }
    if (a->menu0DispStatus != b->menu0DispStatus)
    {
        sprintf(buf, "ref=%d new=%d", (int)a->menu0DispStatus,
                (int)b->menu0DispStatus);
        report("menu0DispStatus", buf); bad = 1;
    }
    if (a->frqLine.maxValue  != b->frqLine.maxValue  ||
        a->frqLine.aimValue  != b->frqLine.aimValue  ||
        a->frqLine.curValue  != b->frqLine.curValue  ||
        a->frqLine.tickerAll != b->frqLine.tickerAll ||
        a->frqLine.remainder != b->frqLine.remainder||
        a->frqLine.calc      != b->frqLine.calc)
    {
        report("frqLine", "struct differs"); bad = 1;
    }

    /* The five funcCode fields the module writes. They are not in SHFIELDS
     * (their struct member names differ from the funcCode paths), so they need
     * an explicit comparison -- reading them in sh_read() without comparing
     * them here made UpdateRunCmd's `funcCode.code.plcCmd = 0;` invisible. */
    if (a->fcPlcCmd != b->fcPlcCmd)
    {
        sprintf(buf, "ref=%u new=%u", a->fcPlcCmd, b->fcPlcCmd);
        report("funcCode.code.plcCmd", buf); bad = 1;
    }
    if (a->fcPowerUpTimeAddup    != b->fcPowerUpTimeAddup ||
        a->fcPowerUpTimeAddupSec != b->fcPowerUpTimeAddupSec)
    {
        sprintf(buf, "ref=(%u,%u) new=(%u,%u)",
                a->fcPowerUpTimeAddup, a->fcPowerUpTimeAddupSec,
                b->fcPowerUpTimeAddup, b->fcPowerUpTimeAddupSec);
        report("funcCode.code.powerUpTimeAddup", buf); bad = 1;
    }
    if (a->fcRunTimeAddup    != b->fcRunTimeAddup ||
        a->fcRunTimeAddupSec != b->fcRunTimeAddupSec)
    {
        sprintf(buf, "ref=(%u,%u) new=(%u,%u)",
                a->fcRunTimeAddup, a->fcRunTimeAddupSec,
                b->fcRunTimeAddup, b->fcRunTimeAddupSec);
        report("funcCode.code.runTimeAddup", buf); bad = 1;
    }
    (void)bad;
}

static void run_case_out(const struct IN *in, int entry,
                         struct ST *ost, struct SH *osh)
{
    struct ST st[2];
    struct SH sh[2];
    struct RUNTRACE tr[2][RUNTRACE_N];
    Uint16 trn[2];
    int side;
    char buf[256];

    g_cases++;

    /* Set RUNTRACE=1 in the environment to get one line per case on stderr.
     * The driver is long-running and the sandbox kills slow commands, so when it
     * dies part-way through this is how the last executed case is identified. */
    if (g_trace_on)
    {
        fprintf(stderr, "case %ld phase=%s entry=%d\n", g_cases, g_phase, entry);
    }

    for (side = 0; side < 2; side++)
    {
        apply_inputs(in);
        sh_write(&in->sh0);
        install(side, &in->st);
        runTraceN = 0;

        invoke(side, entry);

        capture(side, &st[side]);
        sh_read(&sh[side]);
        memcpy(tr[side], runTrace, sizeof(tr[side]));
        trn[side] = runTraceN;
    }

    /* ---- compare ---- */
    {
#define CMPF(t, n)                                                       \
        if (st[0].n != st[1].n) {                                        \
            sprintf(buf, "ref=%lld new=%lld", (long long)st[0].n,        \
                    (long long)st[1].n);                                 \
            report(#n, buf);                                             \
        }
        SFIELDS(CMPF)
#undef CMPF

#define CMPU(t, n)                                                       \
        if (memcmp(&st[0].n, &st[1].n, sizeof(st[0].n))) {               \
            report(#n, "aggregate differs");                             \
        }
        UFIELDS(CMPU)
#undef CMPU

        sh_cmp(&sh[0], &sh[1]);

        if (trn[0] != trn[1] ||
            memcmp(tr[0], tr[1],
                   sizeof(struct RUNTRACE) * (trn[0] < trn[1] ? trn[0] : trn[1])))
        {
            unsigned k = (trn[0] < trn[1]) ? trn[0] : trn[1];
            unsigned d = 0;
            while (d < k && tr[0][d].which == tr[1][d].which &&
                   tr[0][d].a == tr[1][d].a && tr[0][d].b == tr[1][d].b &&
                   tr[0][d].c == tr[1][d].c)
            {
                d++;
            }
            sprintf(buf, "n ref=%u new=%u first-diff idx=%u "
                         "ref=(%u,%ld,%ld,%ld) new=(%u,%ld,%ld,%ld)",
                    trn[0], trn[1], d,
                    d < RUNTRACE_N ? tr[0][d].which : 999u,
                    d < RUNTRACE_N ? (long)tr[0][d].a : -1L,
                    d < RUNTRACE_N ? (long)tr[0][d].b : -1L,
                    d < RUNTRACE_N ? (long)tr[0][d].c : -1L,
                    d < RUNTRACE_N ? tr[1][d].which : 999u,
                    d < RUNTRACE_N ? (long)tr[1][d].a : -1L,
                    d < RUNTRACE_N ? (long)tr[1][d].b : -1L,
                    d < RUNTRACE_N ? (long)tr[1][d].c : -1L);
            report("stub call trace", buf);
        }
    }

    if (ost) { *ost = st[0]; }
    if (osh) { *osh = sh[0]; }
}

static void run_case(const struct IN *in, int entry)
{
    run_case_out(in, entry, NULL, NULL);
}

/* ---------------------------------------------------------------------------
 * Sweep helpers: one factor at a time from a context.
 *
 * Why one-factor-at-a-time plus a handful of hand-picked contexts, instead of
 * the full cross product: the input space is ~60 dimensions. A full product is
 * astronomical, and most combinations are not reachable. What actually finds
 * rewrites is (a) every value of every dimension, from a state where it matters,
 * and (b) long stateful sequences. So: contexts x single-factor sweeps, plus
 * phase_sequence() below.
 * ------------------------------------------------------------------------ */

/* The template every sweep starts from, and what init_base() produced. Declared
 * here because the sweep helpers below read them. */
static struct IN g_base;
static struct IN g_pristine;

static void sw16(size_t off, const Uint16 *v, int n, int entry)
{
    int i;
    struct IN w;
    for (i = 0; i < n; i++) { w = g_base; put16(&w, off, v[i]); run_case(&w, entry); }
}

static void sw32(size_t off, const Uint32 *v, int n, int entry)
{
    int i;
    struct IN w;
    for (i = 0; i < n; i++) { w = g_base; put32(&w, off, v[i]); run_case(&w, entry); }
}

static void swi32(size_t off, const int32 *v, int n, int entry)
{
    int i;
    struct IN w;
    for (i = 0; i < n; i++) { w = g_base; puti32(&w, off, v[i]); run_case(&w, entry); }
}

/* Enum fields are 4 bytes wide on the host (MSVC uses int for enums), so they
 * need a four-byte write. The values are all small and non-negative, so
 * zero-extending a 16-bit constant is exactly the value. */
static void swenum(size_t off, const Uint16 *v, int n, int entry)
{
    int i;
    struct IN w;
    for (i = 0; i < n; i++)
    {
        w = g_base;
        putenum(&w, off, (Uint32)v[i]);
        run_case(&w, entry);
    }
}

/* ---------------------------------------------------------------------------
 * Swing safety
 *
 * `UpdateSwingPara` divides by `swingDoubleAw - swingJumpFrq`, which is 0 when
 * swingAw is 0 -- i.e. when swingAmplitude is 0, or when the amplitude base
 * (maxFrq, or |frqAim|) is 0. It also divides by timeBench, which is 0 until
 * AccDecTimeCalc has run once. Both are pre-existing gaps (a) and (b) in the
 * file header: not reachable from a valid function-code table, and "fixing"
 * either would be a behaviour change rather than a rewrite.
 *
 * Any phase that can reach UpdateSwingPara (directly, or through
 * NormalRunCtrl -> SwingDeal) must therefore filter its cases through
 * swing_safe(). The guarded sweep variants below do exactly that.
 * ------------------------------------------------------------------------ */

static int swing_safe(const struct IN *w)
{
    int32 base;
    int32 aw;
    int32 jump;

    if (w->sh0.timeBench == 0) { return 0; }
    base = (w->swingBaseMode == FUNCCODE_swingBaseMode_AGAIN_MAXFRQ)
               ? (int32)w->maxFrq : (w->frqAim >= 0 ? w->frqAim : -w->frqAim);
    aw   = (base * (int32)w->swingAmplitude) / 1000;
    jump = (aw * (int32)w->swingJumpRange) / 1000;
    if (aw <= 0) { return 0; }
    if ((aw << 1) - jump == 0) { return 0; }
    return 1;
}

/* Guarded variants of sw16 / sw32 / swi32. */
static void sw16s(size_t off, const Uint16 *v, int n, int entry)
{
    int i;
    struct IN w;
    for (i = 0; i < n; i++)
    {
        w = g_base;
        put16(&w, off, v[i]);
        if (swing_safe(&w)) { run_case(&w, entry); }
    }
}

static void swi32s(size_t off, const int32 *v, int n, int entry)
{
    int i;
    struct IN w;
    for (i = 0; i < n; i++)
    {
        w = g_base;
        puti32(&w, off, v[i]);
        if (swing_safe(&w)) { run_case(&w, entry); }
    }
}

static void sw32s(size_t off, const Uint32 *v, int n, int entry)
{
    int i;
    struct IN w;
    for (i = 0; i < n; i++)
    {
        w = g_base;
        put32(&w, off, v[i]);
        if (swing_safe(&w)) { run_case(&w, entry); }
    }
}

/* ---------------------------------------------------------------------------
 * Input defaults.
 *
 * Every field zeroed, then the handful that must not be zero.  The module has
 * three division points that a zero would crash on; they are listed in the file
 * header as gaps (a)(b)(c) and kept safe here rather than "fixed".
 * ------------------------------------------------------------------------ */

static void init_base(void)
{
    int i;

    memset(&g_base, 0, sizeof(g_base));

    /* Function-code values that must be non-zero for the module to behave like
     * a configured drive at all. */
    g_base.fcMaxFrq     = 5000;   /* 50.00 Hz */
    g_base.upperFrq     = 5000;
    g_base.lowerFrq     = 0;
    g_base.maxFrq       = 5000;
    g_base.startFrq     = 100;
    g_base.stopBrakeFrq = 50;
    g_base.accDecSpdCurve = FUNCCODE_accDecSpdCurve_LINE;
    g_base.accDecTimeUnit = FUNCCODE_accDecTimeUnit_0POINT;
    g_base.accTime1 = 500;  g_base.decTime1 = 500;
    g_base.accTime2 = 100;  g_base.decTime2 = 100;
    g_base.accTime3 = 50;   g_base.decTime3 = 50;
    g_base.accTime4 = 10;   g_base.decTime4 = 10;
    g_base.jogAccTime = 100; g_base.jogDecTime = 100;
    g_base.accTimefrqChgValue = 100;
    g_base.decTimefrqChgValue = 100;
    g_base.motorCtrlMode = FUNCCODE_motorCtrlMode_VF;
    g_base.motorType     = MOTOR_TYPE_IM;
    g_base.ratingFrq     = 5000;
    g_base.ratingSpeed   = 1440;
    g_base.motorSn       = MOTOR_SN_1;
    g_base.powerOnStatus = POWER_ON_CORE_OK;
    g_base.runMode       = RUN_MODE_SPEED_CTRL;
    g_base.ratingVoltage = 380;
    g_base.errorLevel    = ERROR_LEVEL_NO_ERROR;
    g_base.mockErrorAttrLevel = ERROR_LEVEL_NO_ERROR;

    /* Swing: keep the divisor strictly positive.  See gap (a). */
    g_base.swingBaseMode      = FUNCCODE_swingBaseMode_AGAIN_MAXFRQ;
    g_base.swingAmplitude     = 1000;
    g_base.swingJumpRange     = 100;
    g_base.swingPeriod        = 100;
    g_base.swingRiseTimeCoeff = 500;

    /* Carried state that must not be zero. */
    g_base.sh0.timeBench  = 1000;   /* gap (b) */
    g_base.sh0.fcPlcCmd   = 0;
    g_base.st.runStatus   = RUN_STATUS_WAIT;
    g_base.st.startRunStatus = START_RUN_STATUS_INIT;
    g_base.st.stopRunStatus  = STOP_RUN_STATUS_INIT;
    g_base.st.dormantStatus  = DORMANT_RESPOND;
    g_base.st.swingStatus    = SWING_NONE;

    /* Fault-injection-free function codes for the non-zero ones that matter. */
    g_base.mfkKeyFunc  = 0;
    g_base.stopKeyFunc = 0;

    for (i = 0; i < AI_NUMBER; i++)
    {
        g_base.aiSet[i] = 16384;   /* 50% of full scale */
    }

    g_pristine = g_base;
}

static void ctx_reset(void) { g_base = g_pristine; }

/* ===========================================================================
 * Phases
 * ======================================================================== */

/* ---- UpdateRunCmd ------------------------------------------------------- */

static const Uint16 U_RUNSRC[]  = { FUNCCODE_runSrc_PANEL, FUNCCODE_runSrc_DI,
                                    FUNCCODE_runSrc_COMM, 3, 4 };
static const Uint16 U_DIMODE[]  = { FUNCCODE_diControlMode_2LINE1,
                                    FUNCCODE_diControlMode_2LINE2,
                                    FUNCCODE_diControlMode_3LINE1,
                                    FUNCCODE_diControlMode_3LINE2, 9 };
static const Uint16 U_KEY[]     = { KEY_NO_KEY, KEY_RUN, KEY_STOP, KEY_REV,
                                    KEY_SWITCH, KEY_UP, KEY_FWD_JOG, KEY_REV_JOG,
                                    KEY_MFK, KEY_PRG, KEY_ENTER };
/* DI_FUNC1 bits: fwd=1 rev=2 tripleLineCtrl=3 fwdJog=4 revJog=5 closePwm=8
 * runPause=10 accDecTimeSrc=16..17 localOrRemote=20 forbidAccDecSpd=21
 * swingPause=24.  Uint32, so the high bits really are reachable. */
static const Uint32 U_DI1[] = {
    0,
    (1UL << 1),                    /* fwd */
    (1UL << 2),                    /* rev */
    (1UL << 1) | (1UL << 2),       /* fwd + rev */
    (1UL << 3),                    /* tripleLineCtrl */
    (1UL << 4),                    /* fwdJog */
    (1UL << 5),                    /* revJog */
    (1UL << 1) | (1UL << 4),       /* fwd + fwdJog */
    (1UL << 2) | (1UL << 5),       /* rev + revJog */
    (1UL << 8),                    /* closePwm */
    (1UL << 10),                   /* runPause */
    (1UL << 16),                   /* accDecTimeSrc = 1 */
    (2UL << 16),                   /* accDecTimeSrc = 2 */
    (3UL << 16),                   /* accDecTimeSrc = 3 */
    (1UL << 20),                   /* localOrRemote */
    (1UL << 21),                   /* forbidAccDecSpd */
    (1UL << 24),                   /* swingPause */
    (1UL << 3) | (1UL << 1) | (1UL << 2)
};
/* DI_FUNC2 bits: brake=0 frqOk=2 stopPanel=4 diOrComm=5 emergencyStop=15
 * stop4dec=16 decBrake=17 clearSetRunTime=18. */
static const Uint32 U_DI2[] = {
    0,
    (1UL << 0),                    /* brake */
    (1UL << 2),                    /* frqOk */
    (1UL << 4),                    /* stopPanel */
    (1UL << 5),                    /* diOrComm */
    (1UL << 15),                   /* emergencyStop */
    (1UL << 16),                   /* stop4dec */
    (1UL << 17),                   /* decBrake */
    (1UL << 18),                   /* clearSetRunTime */
    (1UL << 0) | (1UL << 17),      /* brake + decBrake */
    (1UL << 15) | (1UL << 16)      /* emergencyStop + stop4dec */
};
static const Uint16 U_ERRLEV[] = { ERROR_LEVEL_NO_ERROR, ERROR_LEVEL_FREE_STOP,
                                   ERROR_LEVEL_STOP, ERROR_LEVEL_RUN };
static const Uint16 U_ERRCODE[] = { ERROR_NONE, ERROR_OC_ACC_SPEED, ERROR_TUNE,
                                    ERROR_LOSE_LOAD, 99 };
static const Uint16 U_POWERON[] = { POWER_ON_WAIT, POWER_ON_CORE_OK,
                                    POWER_ON_FUNC_WAIT_OT };
static const Uint16 U_BOOL[]    = { 0, 1 };
static const Uint16 U_BOOL2[]   = { 0, 1, 2 };
static const Uint16 U_RW_MODE[] = { FUNCCODE_paraInitMode_NO_OPERATION,
                                    FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA,
                                    FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA_ALL,
                                    FUNCCODE_paraInitMode_SAVE_USER_PARA };
static const Uint16 U_TICKER[]  = { 0, 1, 2, 3, 10 };
static const Uint16 U_RUNSTATUS[] = {
    RUN_STATUS_WAIT, RUN_STATUS_ZERO, RUN_STATUS_START, RUN_STATUS_NORMAL,
    RUN_STATUS_STOP, RUN_STATUS_JOG, RUN_STATUS_POS_CTRL, RUN_STATUS_TUNE,
    RUN_STATUS_DI_BRAKE_DEC, RUN_STATUS_DI_BRAKE, RUN_STATUS_LOSE_LOAD,
    RUN_STATUS_SHUT_DOWN
};
/* runCmd bit masks. */
static const Uint16 U_RUNCMD[] = {
    0,
    0x0001,                 /* common0 */
    0x0002,                 /* common */
    0x0004,                 /* jog = 1 */
    0x0008,                 /* jog = 2 */
    0x000C,                 /* jog = 3 */
    0x0040,                 /* dir = REVERSE */
    0x0080,                 /* pause */
    0x0100,                 /* freeStop */
    0x0200,                 /* hurryStop */
    0x0400,                 /* otherStop */
    0x0800,                 /* startProtect */
    0x1000,                 /* errorReset */
    0x0001 | 0x0002,        /* common0 + common */
    0x0002 | 0x0004,        /* common + jog */
    0x0001 | 0x0080,        /* common0 + pause */
    0x0002 | 0x0100         /* common + freeStop */
};
/* runFlag bit masks. */
static const Uint16 U_RUNFLAG[] = {
    0,
    0x0001,                 /* run */
    0x0002,                 /* common */
    0x0004,                 /* jog */
    0x0008,                 /* tune */
    0x0010,                 /* jogWhenRun */
    0x0020,                 /* accDecStatus = ACC_SPEED */
    0x0040,                 /* accDecStatus = DEC_SPEED */
    0x0080,                 /* plc */
    0x0100,                 /* pid */
    0x0200,                 /* torque */
    0x1000,                 /* dirReversing */
    0x2000,                 /* dirFinal = REVERSE */
    0x4000,                 /* servo */
    0x0001 | 0x0002,        /* run + common */
    0x0001 | 0x0004,        /* run + jog */
    0x0002 | 0x0010,        /* common + jogWhenRun */
    0x0001 | 0x1000         /* run + dirReversing */
};
/* dspMainCmd bit masks. */
static const Uint16 U_MAINCMD[] = {
    0,
    0x0001,                 /* run */
    0x0002,                 /* startBrake */
    0x0004,                 /* stopBrake */
    0x0020,                 /* startFlux */
    0x0002 | 0x0004,        /* startBrake + stopBrake */
    0x0001 | 0x0020         /* run + startFlux */
};
/* dspMainCmd1 bit masks: speedTrack = bits 10..11. */
static const Uint16 U_MAINCMD1[] = { 0, 0x0400, 0x0800 };
/* dspStatus bit masks: speedTrackEnd = bit 4, outAirSwitchOff = bit 8. */
static const Uint16 U_DSPSTATUS[] = { 0, 0x0010, 0x0100, 0x0110 };
static const Uint16 U_COMMRUNCMD[] = {
    SCI_RUN_CMD_NONE, SCI_RUN_CMD_FWD_RUN, SCI_RUN_CMD_REV_RUN,
    SCI_RUN_CMD_FWD_JOG, SCI_RUN_CMD_REV_JOG, SCI_RUN_CMD_FREE_STOP,
    SCI_RUN_CMD_STOP, SCI_RUN_CMD_RESET_ERROR, 99
};
static const Uint16 U_LOWERDEAL[] = { FUNCCODE_lowerDeal_RUN_LOWER,
                                      FUNCCODE_lowerDeal_DELAY_STOP,
                                      FUNCCODE_lowerDeal_RUN_ZERO, 3 };
static const Uint16 U_STARTMODE[] = { FUNCCODE_startMode_DIRECT_START,
                                      FUNCCODE_startMode_SPEED_TRACK,
                                      FUNCCODE_startMode_FORE_MAG, 9 };
static const Uint16 U_MOTORMODE[] = { FUNCCODE_motorCtrlMode_SVC,
                                      FUNCCODE_motorCtrlMode_FVC,
                                      FUNCCODE_motorCtrlMode_VF, 9 };
static const Uint16 U_MOTORTYPE[] = { MOTOR_TYPE_IM, MOTOR_TYPE_VARFREQ,
                                      MOTOR_TYPE_PMSM, 7 };
static const Uint16 U_STOPMODE[] = { 0, FUNCCODE_stopMode_FREESTOP };
static const Uint16 U_ACCUNIT[] = { FUNCCODE_accDecTimeUnit_0POINT,
                                    FUNCCODE_accDecTimeUnit_1POINT,
                                    FUNCCODE_accDecTimeUnit_2POINT, 7 };
/* The three values UpdateBenchTime can write into timeBench, and the only ones
 * that are safe to run through UpdateSwingPara's `... / timeBench` (gap (b)). */
static const Uint16 U_BENCH[] = { 10, 100, 1000 };
static const Uint16 U_CURVE[] = { FUNCCODE_accDecSpdCurve_LINE,
                                  FUNCCODE_accDecSpdCurve_S_CURVE_A,
                                  FUNCCODE_accDecSpdCurve_S_CURVE_B, 7 };
static const Uint16 U_RUNMODE[] = { RUN_MODE_SPEED_CTRL, RUN_MODE_TORQUE_CTRL,
                                    RUN_MODE_POSITION_CTRL, 7 };
static const Uint16 U_TIMESRC[] = { 0, FUNCCODE_plcFrq0Src_AI1,
                                    FUNCCODE_plcFrq0Src_AI2,
                                    FUNCCODE_plcFrq0Src_AI3, 9 };
static const Uint16 U_ERRSRC[] = { ERR_RUN_FRQ_RUN, ERR_RUN_FRQ_AIM,
                                   ERR_RUN_FRQ_UPPER, ERR_RUN_FRQ_LOWER,
                                   ERR_RUN_FRQ_SECOND, 99 };
/* `motorCtrlTuneStatus` is compared against the file-local TUNE_STEP_* macros of
 * f_runSrc.c (lines 1687-1690): WAIT=0, ACC=50, DEC=51, END=100. Those names are
 * not visible here, so the numbers are spelled out and must be kept in sync.
 *
 * This used to be the enum TUNE_RUN_STATUS_* (WAIT/ACC/DEC/END = 0/1/2/3). That
 * enum is declared in f_runSrc.c and then never used anywhere -- the module only
 * ever compares against TUNE_STEP_*. So every sweep of this field landed on
 * TUNE_STEP_WAIT, the switch's ACC / DEC / END cases were never entered, and the
 * negative control silently missed five mutations there (M85-M89). 49/52/99 keep
 * the default case covered. */
#define DRV_TUNE_STEP_WAIT  0
#define DRV_TUNE_STEP_ACC   50
#define DRV_TUNE_STEP_DEC   51
#define DRV_TUNE_STEP_END   100
static const Uint16 U_TUNESTEP[] = { DRV_TUNE_STEP_WAIT, DRV_TUNE_STEP_ACC,
                                     DRV_TUNE_STEP_DEC, DRV_TUNE_STEP_END,
                                     49, 52, 99 };
static const Uint16 U_FRQ_WIDE[] = { 0, 1, 49, 50, 51, 100, 500, 1000, 5000 };
static const Uint32 U32_WIDE[]   = { 0, 1, 49, 50, 51, 100, 500, 1000, 5000,
                                     65535, 65536, 100000, 4294967295u };
static const Uint32 U32_TICK[]   = { 0, 1, 49, 50, 51, 499, 500, 501,
                                     60000, 100000, 4294967295u };
static const int32  I_FRQ_WIDE[] = { 0, 1, 49, 50, 51, 100, 500, 1000, 5000,
                                     -1, -50, -100, -5000 };
static const Uint16 U_SWING_STATUS[] = { SWING_NONE, SWING_UP, SWING_DOWN };

static void sweep_urc(void)
{
    sw16(offsetof(struct IN, fcRunSrc),   U_RUNSRC, NEL(U_RUNSRC), E_UPDATE_RUN_CMD);
    sw16(offsetof(struct IN, diControlMode), U_DIMODE, NEL(U_DIMODE), E_UPDATE_RUN_CMD);
    sw16(SH_OFF(keyFunc),                 U_KEY,    NEL(U_KEY),    E_UPDATE_RUN_CMD);
    sw32(offsetof(struct IN, diF1),       U_DI1,    NEL(U_DI1),    E_UPDATE_RUN_CMD);
    sw32(offsetof(struct IN, diF2),       U_DI2,    NEL(U_DI2),    E_UPDATE_RUN_CMD);
    sw32(offsetof(struct IN, diSelF1),    U_DI1,    NEL(U_DI1),    E_UPDATE_RUN_CMD);
    sw16(offsetof(struct IN, errorLevel), U_ERRLEV, NEL(U_ERRLEV), E_UPDATE_RUN_CMD);
    sw16(offsetof(struct IN, errorCode),  U_ERRCODE,NEL(U_ERRCODE),E_UPDATE_RUN_CMD);
    sw16(offsetof(struct IN, powerOnStatus), U_POWERON, NEL(U_POWERON), E_UPDATE_RUN_CMD);
    sw16(SH_OFF(bStopPlc),                U_BOOL,   NEL(U_BOOL),   E_UPDATE_RUN_CMD);
    sw16(SH_OFF(commRunCmd),              U_COMMRUNCMD, NEL(U_COMMRUNCMD), E_UPDATE_RUN_CMD);
    sw16(SH_OFF(timeBench),               U_BOOL,   NEL(U_BOOL),   E_UPDATE_RUN_CMD);
    sw16(ST_OFF(startProtectSrc),         U_BOOL2,  NEL(U_BOOL2),  E_UPDATE_RUN_CMD);
    sw16(ST_OFF(otherStopSrc),            U_BOOL2,  NEL(U_BOOL2),  E_UPDATE_RUN_CMD);
    sw16(ST_OFF(otherStopLowerSrc),       U_BOOL2,  NEL(U_BOOL2),  E_UPDATE_RUN_CMD);
    sw16(ST_OFF(runCmd),                  U_RUNCMD, NEL(U_RUNCMD), E_UPDATE_RUN_CMD);
    sw16(ST_OFF(runFlag),                 U_RUNFLAG,NEL(U_RUNFLAG),E_UPDATE_RUN_CMD);
    sw16(ST_OFF(dspMainCmd),              U_MAINCMD,NEL(U_MAINCMD),E_UPDATE_RUN_CMD);
    sw16(ST_OFF(dspMainCmd1),             U_MAINCMD1,NEL(U_MAINCMD1),E_UPDATE_RUN_CMD);
    sw16(ST_OFF(dspStatus),               U_DSPSTATUS,NEL(U_DSPSTATUS),E_UPDATE_RUN_CMD);
    sw16(ST_OFF(dspSubCmd),               U_BOOL,   NEL(U_BOOL),   E_UPDATE_RUN_CMD);
    sw16(ST_OFF(runStatus1Step),          U_BOOL2,  NEL(U_BOOL2),  E_UPDATE_RUN_CMD);
    swenum(ST_OFF(runStatus),             U_RUNSTATUS, NEL(U_RUNSTATUS), E_UPDATE_RUN_CMD);
    swenum(ST_OFF(dormantStatus),         U_BOOL2,  NEL(U_BOOL2),  E_UPDATE_RUN_CMD);
    sw16(ST_OFF(stopRemFlag),             U_BOOL2,  NEL(U_BOOL2),  E_UPDATE_RUN_CMD);
    sw16(ST_OFF(bRunJog),                 U_BOOL,   NEL(U_BOOL),   E_UPDATE_RUN_CMD);
    sw16(ST_OFF(runDirPanelOld),          U_BOOL,   NEL(U_BOOL),   E_UPDATE_RUN_CMD);
    sw16(ST_OFF(tuneCmd),                 U_BOOL,   NEL(U_BOOL),   E_UPDATE_RUN_CMD);
    sw16(ST_OFF(shuntFlag),               U_BOOL,   NEL(U_BOOL),   E_UPDATE_RUN_CMD);
    sw16(offsetof(struct IN, funcCodeRwMode), U_RW_MODE, NEL(U_RW_MODE), E_UPDATE_RUN_CMD);
    sw16(offsetof(struct IN, mainLoopTicker), U_TICKER, NEL(U_TICKER), E_UPDATE_RUN_CMD);
    sw16(offsetof(struct IN, mfkKeyFunc), U_BOOL2,  NEL(U_BOOL2),  E_UPDATE_RUN_CMD);
    sw16(offsetof(struct IN, stopKeyFunc),U_BOOL,   NEL(U_BOOL),   E_UPDATE_RUN_CMD);
    sw16(offsetof(struct IN, startProtect),U_BOOL,  NEL(U_BOOL),   E_UPDATE_RUN_CMD);
    sw16(offsetof(struct IN, pidCalcMode),U_BOOL,   NEL(U_BOOL),   E_UPDATE_RUN_CMD);
    sw16(offsetof(struct IN, jogWhenRun), U_BOOL,   NEL(U_BOOL),   E_UPDATE_RUN_CMD);
    sw16(offsetof(struct IN, stopMode),   U_STOPMODE, NEL(U_STOPMODE), E_UPDATE_RUN_CMD);
    sw16(offsetof(struct IN, startFrq),   U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_UPDATE_RUN_CMD);
    sw16(offsetof(struct IN, lowerDeal),  U_LOWERDEAL, NEL(U_LOWERDEAL), E_UPDATE_RUN_CMD);
    sw16(offsetof(struct IN, lowerFrq),   U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_UPDATE_RUN_CMD);
    sw16(offsetof(struct IN, frqAimTmp),  U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_UPDATE_RUN_CMD);
    swi32(SH_OFF(frqAimTmp0),             I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_UPDATE_RUN_CMD);
    sw16(offsetof(struct IN, plcEnable),  U_BOOL,   NEL(U_BOOL),   E_UPDATE_RUN_CMD);
    sw16(offsetof(struct IN, plcCmd),     U_COMMRUNCMD, NEL(U_COMMRUNCMD), E_UPDATE_RUN_CMD);
}

static void ctx_panel(void)
{
    ctx_reset();
    g_base.fcRunSrc  = FUNCCODE_runSrc_PANEL;
    g_base.st.runStatus = RUN_STATUS_WAIT;
    put16(&g_base, SH_OFF(keyFunc), KEY_NO_KEY);
    put16(&g_base, ST_OFF(runDirPanelOld), FORWARD_DIR);
}

static void ctx_di(void)
{
    ctx_reset();
    g_base.fcRunSrc     = FUNCCODE_runSrc_DI;
    g_base.diControlMode = FUNCCODE_diControlMode_2LINE1;
    put32(&g_base, offsetof(struct IN, diF1), 1UL << 1);
    put16(&g_base, ST_OFF(runFlag), 0x0001);
    put16(&g_base, ST_OFF(runCmd), 0x0001);
    putenum(&g_base, ST_OFF(runStatus), RUN_STATUS_NORMAL);
}

static void ctx_comm(void)
{
    ctx_reset();
    g_base.fcRunSrc = FUNCCODE_runSrc_COMM;
    g_base.plcEnable = 1;
    put16(&g_base, SH_OFF(commRunCmd), SCI_RUN_CMD_FWD_RUN);
    put16(&g_base, ST_OFF(runFlag), 0x0001);
    putenum(&g_base, ST_OFF(runStatus), RUN_STATUS_NORMAL);
}

static void ctx_error(void)
{
    ctx_reset();
    g_base.errorLevel = ERROR_LEVEL_FREE_STOP;
    g_base.errorCode  = ERROR_OC_ACC_SPEED;
    put16(&g_base, ST_OFF(runFlag), 0x0003);
    put16(&g_base, ST_OFF(runCmd), 0x0003);
    putenum(&g_base, ST_OFF(runStatus), RUN_STATUS_NORMAL);
}

static void ctx_jog(void)
{
    ctx_reset();
    g_base.fcRunSrc = FUNCCODE_runSrc_PANEL;
    put16(&g_base, ST_OFF(runFlag), 0x0005);
    put16(&g_base, ST_OFF(runCmd), 0x000E);
    putenum(&g_base, ST_OFF(runStatus), RUN_STATUS_JOG);
    put16(&g_base, SH_OFF(keyFunc), KEY_FWD_JOG);
}

static void ctx_dormant(void)
{
    ctx_reset();
    put16(&g_base, ST_OFF(runFlag), 0x0002);
    putenum(&g_base, ST_OFF(dormantStatus), DORMANT_NO_RESPOND);
    put16(&g_base, ST_OFF(runCmd), 0x0001);
    puti32(&g_base, SH_OFF(frqAimTmp0), 0);
}

static void ctx_shutdown(void)
{
    ctx_reset();
    put16(&g_base, ST_OFF(runFlag), 0x0003);
    put16(&g_base, ST_OFF(runCmd), 0x0002);
    putenum(&g_base, ST_OFF(runStatus), RUN_STATUS_SHUT_DOWN);
    put16(&g_base, ST_OFF(stopRemFlag), 2);
}

static void phase_update_run_cmd(void)
{
    ctx_panel();    sweep_urc();
    ctx_di();       sweep_urc();
    ctx_comm();     sweep_urc();
    ctx_error();    sweep_urc();
    ctx_jog();      sweep_urc();
    ctx_dormant();  sweep_urc();
    ctx_shutdown(); sweep_urc();
}

/* ---- setTimeRun --------------------------------------------------------- */

static void phase_set_time_run(void)
{
    static const Uint16 mode[]  = { 0, 1 };
    static const int16  sets[]  = { 0, 1, 3276, 16384, -16384, 32767, -32768 };
    int i;

    ctx_reset();
    sw16(offsetof(struct IN, setTimeMode), mode, NEL(mode), E_SET_TIME_RUN);
    sw16(offsetof(struct IN, setTimeSource), U_TIMESRC, NEL(U_TIMESRC), E_SET_TIME_RUN);
    sw16(offsetof(struct IN, setTimeValue), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_SET_TIME_RUN);
    sw16(ST_OFF(runCmd), U_RUNCMD, NEL(U_RUNCMD), E_SET_TIME_RUN);
    sw16(ST_OFF(otherStopSrc), U_BOOL2, NEL(U_BOOL2), E_SET_TIME_RUN);
    sw16(SH_OFF(curTime) + offsetof(struct CUR_TIME, runTime), U_FRQ_WIDE,
         NEL(U_FRQ_WIDE), E_SET_TIME_RUN);

    /* setTimeSource selects aiDeal[i]; sweep the AI values with mode on. */
    for (i = 0; i < (int)NEL(sets); i++)
    {
        struct IN w = g_base;
        w.setTimeMode = 1;
        w.setTimeSource = FUNCCODE_plcFrq0Src_AI1;
        w.setTimeValue = 100;
        w.aiSet[0] = sets[i];
        put16(&w, SH_OFF(curTime) + offsetof(struct CUR_TIME, runTime), 50);
        run_case(&w, E_SET_TIME_RUN);

        w.setTimeSource = FUNCCODE_plcFrq0Src_AI3;
        w.aiSet[2] = sets[i];
        run_case(&w, E_SET_TIME_RUN);
    }
}

/* ---- runTimeCal --------------------------------------------------------- */

static void phase_run_time_cal(void)
{
    static const Uint16 tick[] = { 0, 1, 499, 500, 501, 65535 };
    static const Uint16 sec[]  = { 0, 1, 59, 60, 61, 65535 };
    static const Uint16 mins[] = { 0, 1, 59, 60, 65535 };
    static const Uint16 add[]  = { 0, 1, 3599, 3600, 3601, 65535 };
    size_t off;

    ctx_reset();
    put16(&g_base, ST_OFF(runFlag), 0x0001);

    off = SH_OFF(curTime);
    sw16(off + offsetof(struct CUR_TIME, runTimeSec),    sec, NEL(sec), E_RUN_TIME_CAL);
    sw16(off + offsetof(struct CUR_TIME, runTimeM),      mins, NEL(mins), E_RUN_TIME_CAL);
    sw16(off + offsetof(struct CUR_TIME, powerOnTimeSec),sec, NEL(sec), E_RUN_TIME_CAL);
    sw16(off + offsetof(struct CUR_TIME, powerOnTimeM),  mins, NEL(mins), E_RUN_TIME_CAL);
    sw16(ST_OFF(runTimeAddupTicker),      tick, NEL(tick), E_RUN_TIME_CAL);
    sw16(ST_OFF(powerUpTimeAddupTicker),  tick, NEL(tick), E_RUN_TIME_CAL);
    sw16(SH_OFF(fcRunTimeAddupSec),       add, NEL(add), E_RUN_TIME_CAL);
    sw16(SH_OFF(fcPowerUpTimeAddupSec),   add, NEL(add), E_RUN_TIME_CAL);
    sw16(SH_OFF(fcRunTimeAddup),          add, NEL(add), E_RUN_TIME_CAL);
    sw16(SH_OFF(fcPowerUpTimeAddup),      add, NEL(add), E_RUN_TIME_CAL);
    sw16(offsetof(struct IN, runTimeArriveSet),     add, NEL(add), E_RUN_TIME_CAL);
    sw16(offsetof(struct IN, powerUpTimeArriveSet), add, NEL(add), E_RUN_TIME_CAL);
    sw16(offsetof(struct IN, setTimeArrive),        add, NEL(add), E_RUN_TIME_CAL);
    sw16(offsetof(struct IN, aiSet), U_BOOL, NEL(U_BOOL), E_RUN_TIME_CAL);
    sw16(SH_OFF(errorOther), add, NEL(add), E_RUN_TIME_CAL);

    /* runFlag.run x clearSetRunTime x setTimeArrive: the three-way gate on the
     * running-hours branch. */
    {
        static const Uint16 flags[] = { 0, 0x0001, 0x0002, 0x0003 };
        static const Uint32 di2[]   = { 0, (1UL << 18) };
        int i, j, k;
        struct IN w;
        for (i = 0; i < (int)NEL(flags); i++)
        {
            for (j = 0; j < (int)NEL(di2); j++)
            {
                for (k = 0; k < 3; k++)
                {
                    w = g_base;
                    put16(&w, ST_OFF(runFlag), flags[i]);
                    put32(&w, offsetof(struct IN, diF2), di2[j]);
                    w.setTimeArrive = (k == 0) ? 0 : (k == 1) ? 100 : 1;
                    put32(&w, SH_OFF(curTime) + offsetof(struct CUR_TIME, runTime), 100);
                    put16(&w, ST_OFF(runTimeAddupTicker), 499);
                    run_case(&w, E_RUN_TIME_CAL);
                }
            }
        }
    }
}

/* ---- UpdateBenchTime ---------------------------------------------------- */

static void phase_update_bench_time(void)
{
    ctx_reset();
    sw16(offsetof(struct IN, accDecTimeUnit), U_ACCUNIT, NEL(U_ACCUNIT), E_UPDATE_BENCH_TIME);
}

/* ---- RunSrcUpdateFrq ---------------------------------------------------- */

static void sweep_rsuf(void)
{
    sw16(ST_OFF(runCmd),    U_RUNCMD, NEL(U_RUNCMD), E_RUN_SRC_UPD_FRQ);
    swenum(ST_OFF(runStatus), U_RUNSTATUS, NEL(U_RUNSTATUS), E_RUN_SRC_UPD_FRQ);
    sw16(ST_OFF(runFlag),   U_RUNFLAG, NEL(U_RUNFLAG), E_RUN_SRC_UPD_FRQ);
    sw32(offsetof(struct IN, diF2), U_DI2, NEL(U_DI2), E_RUN_SRC_UPD_FRQ);
    sw32(offsetof(struct IN, diF1), U_DI1, NEL(U_DI1), E_RUN_SRC_UPD_FRQ);
    sw16(offsetof(struct IN, errorLevel), U_ERRLEV, NEL(U_ERRLEV), E_RUN_SRC_UPD_FRQ);
    sw16(offsetof(struct IN, errorCode), U_ERRCODE, NEL(U_ERRCODE), E_RUN_SRC_UPD_FRQ);
    sw16(offsetof(struct IN, errorRunFrqSrc), U_ERRSRC, NEL(U_ERRSRC), E_RUN_SRC_UPD_FRQ);
    sw16(offsetof(struct IN, errorSecondFrq), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_RUN_SRC_UPD_FRQ);
    sw16(offsetof(struct IN, fcMaxFrq), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_RUN_SRC_UPD_FRQ);
    swi32(offsetof(struct IN, frqAim), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_RUN_SRC_UPD_FRQ);
    swi32(offsetof(struct IN, frqRun), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_RUN_SRC_UPD_FRQ);
    swi32(SH_OFF(frqTmp),  I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_RUN_SRC_UPD_FRQ);
    swi32(SH_OFF(frqTmpFrac), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_RUN_SRC_UPD_FRQ);
    swi32(SH_OFF(frqCurAim), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_RUN_SRC_UPD_FRQ);
    swi32(SH_OFF(frqAimOld4Dir), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_RUN_SRC_UPD_FRQ);
    sw16(ST_OFF(shuntFlag), U_BOOL, NEL(U_BOOL), E_RUN_SRC_UPD_FRQ);
    sw16(ST_OFF(stopRemFlag), U_BOOL2, NEL(U_BOOL2), E_RUN_SRC_UPD_FRQ);
    sw16(offsetof(struct IN, frqRemMode), U_BOOL, NEL(U_BOOL), E_RUN_SRC_UPD_FRQ);
    swenum(ST_OFF(dormantStatus), U_BOOL2, NEL(U_BOOL2), E_RUN_SRC_UPD_FRQ);
    sw16(SH_OFF(bStopPlc), U_BOOL, NEL(U_BOOL), E_RUN_SRC_UPD_FRQ);
    sw16(ST_OFF(tuneCmd), U_BOOL, NEL(U_BOOL), E_RUN_SRC_UPD_FRQ);
    sw16(ST_OFF(saveTuneDataFlag), U_BOOL, NEL(U_BOOL), E_RUN_SRC_UPD_FRQ);
    sw16(ST_OFF(motorCtrlTuneStatus), U_TUNESTEP, NEL(U_TUNESTEP), E_RUN_SRC_UPD_FRQ);
    sw16(offsetof(struct IN, bAntiReverseRun), U_BOOL, NEL(U_BOOL), E_RUN_SRC_UPD_FRQ);
    sw16(offsetof(struct IN, runMode), U_RUNMODE, NEL(U_RUNMODE), E_RUN_SRC_UPD_FRQ);
    sw16(offsetof(struct IN, stopBrakeFrq), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_RUN_SRC_UPD_FRQ);
    sw32(ST_OFF(accFrqTime), U32_WIDE, NEL(U32_WIDE), E_RUN_SRC_UPD_FRQ);
    sw32(ST_OFF(decFrqTime), U32_WIDE, NEL(U32_WIDE), E_RUN_SRC_UPD_FRQ);
    sw16(offsetof(struct IN, accDecSpdCurve), U_CURVE, NEL(U_CURVE), E_RUN_SRC_UPD_FRQ);
    sw16(offsetof(struct IN, motorCtrlMode), U_MOTORMODE, NEL(U_MOTORMODE), E_RUN_SRC_UPD_FRQ);
    sw16(offsetof(struct IN, motorType), U_MOTORTYPE, NEL(U_MOTORTYPE), E_RUN_SRC_UPD_FRQ);
    sw16(offsetof(struct IN, ratingFrq), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_RUN_SRC_UPD_FRQ);
    sw16(offsetof(struct IN, ratingSpeed), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_RUN_SRC_UPD_FRQ);
    sw16(offsetof(struct IN, runDir), U_BOOL, NEL(U_BOOL), E_RUN_SRC_UPD_FRQ);
    sw16(ST_OFF(dspStatus), U_DSPSTATUS, NEL(U_DSPSTATUS), E_RUN_SRC_UPD_FRQ);
    sw16(offsetof(struct IN, loseLoadProtectMode), U_BOOL, NEL(U_BOOL), E_RUN_SRC_UPD_FRQ);
    sw32(ST_OFF(shuntTicker), U32_WIDE, NEL(U32_WIDE), E_RUN_SRC_UPD_FRQ);
    sw16(offsetof(struct IN, pOffTransitoryNoStop), U_BOOL2, NEL(U_BOOL2), E_RUN_SRC_UPD_FRQ);
    /* timeBench only in the three values UpdateBenchTime can produce: the module
     * divides by it in UpdateSwingPara, reachable from here through
     * RunSrcUpdateFrq -> NormalRunCtrl -> SwingDeal.  See gap (b). */
    sw16(SH_OFF(timeBench), U_BENCH, NEL(U_BENCH), E_RUN_SRC_UPD_FRQ);
}

static void phase_run_src_update_frq(void)
{
    ctx_reset();
    g_base.fcRunSrc = FUNCCODE_runSrc_PANEL;
    put16(&g_base, ST_OFF(runFlag), 0x0003);
    put16(&g_base, ST_OFF(runCmd), 0x0003);
    putenum(&g_base, ST_OFF(runStatus), RUN_STATUS_NORMAL);
    sweep_rsuf();

    ctx_reset();
    put16(&g_base, ST_OFF(runFlag), 0x0003);
    put16(&g_base, ST_OFF(runCmd), 0x0003);
    putenum(&g_base, ST_OFF(runStatus), RUN_STATUS_START);
    sweep_rsuf();

    ctx_reset();
    put16(&g_base, ST_OFF(runFlag), 0x0003);
    put16(&g_base, ST_OFF(runCmd), 0x0002);
    putenum(&g_base, ST_OFF(runStatus), RUN_STATUS_STOP);
    sweep_rsuf();

    ctx_reset();
    put16(&g_base, ST_OFF(runFlag), 0x0001);
    put16(&g_base, ST_OFF(runCmd), 0x0000);
    putenum(&g_base, ST_OFF(runStatus), RUN_STATUS_SHUT_DOWN);
    sweep_rsuf();

    ctx_reset();
    put16(&g_base, ST_OFF(runFlag), 0x0001);
    put16(&g_base, ST_OFF(runCmd), 0x0002);
    putenum(&g_base, ST_OFF(runStatus), RUN_STATUS_DI_BRAKE_DEC);
    sweep_rsuf();

    ctx_reset();
    put16(&g_base, ST_OFF(runFlag), 0x0001);
    putenum(&g_base, ST_OFF(runStatus), RUN_STATUS_LOSE_LOAD);
    sweep_rsuf();

    ctx_reset();
    put16(&g_base, ST_OFF(runFlag), 0x0003);
    put16(&g_base, ST_OFF(runCmd), 0x0003);
    putenum(&g_base, ST_OFF(runStatus), RUN_STATUS_TUNE);
    put16(&g_base, ST_OFF(tuneCmd), 1);
    sweep_rsuf();

    /* The `switch (funcCode.code.errorRunFrqSrc)` under ERROR_LEVEL_RUN needs
     * four things at once: the error level, the source selector, and the two
     * frequencies it picks between. The one-factor sweeps above always leave one
     * of them at its template value -- in particular maxFrq and errorSecondFrq
     * are both 0 there, so `errorSecondFrq * maxFrq` is 0 whichever one is
     * swept, and the UPPER/LOWER/SECOND arms were never selected.
     *
     * Two constraints on the surrounding state, both learned the hard way:
     *   - maxFrq must stay non-zero. It also feeds UpdateSwingPara's
     *     `swingDoubleAw - swingJumpFrq` divisor through NormalRunCtrl, and 0
     *     there is the pre-existing gap (a) -- a crash, not a signal.
     *   - frqAim must stay >= lowerFrq, otherwise the NormalRunCtrl call that
     *     follows the switch runs LowerThanLowerFrqDeal, which overwrites
     *     frqCurAim and hides whatever the switch just wrote. */
    {
        static const Uint16 srcs[] = { ERR_RUN_FRQ_RUN, ERR_RUN_FRQ_AIM,
                                       ERR_RUN_FRQ_UPPER, ERR_RUN_FRQ_LOWER,
                                       ERR_RUN_FRQ_SECOND, 99 };
        static const Uint16 frqs[] = { 1, 500, 1000, 5000 };
        int i, j, k;
        struct IN w;

        for (i = 0; i < (int)NEL(srcs); i++)
        {
            for (j = 0; j < (int)NEL(frqs); j++)
            {
                for (k = 0; k < (int)NEL(frqs); k++)
                {
                    w = g_base;
                    w.errorLevel = ERROR_LEVEL_RUN;
                    w.errorCode = ERROR_NONE;
                    w.errorRunFrqSrc = srcs[i];
                    w.upperFrq = frqs[j];
                    w.lowerFrq = frqs[k];
                    w.errorSecondFrq = frqs[j];
                    w.maxFrq = frqs[k];
                    puti32(&w, offsetof(struct IN, frqAim), 5000);
                    puti32(&w, SH_OFF(frqTmp), 0);
                    put16(&w, ST_OFF(runFlag), 0x0003);
                    put16(&w, ST_OFF(runCmd), 0x0003);
                    putenum(&w, ST_OFF(runStatus), RUN_STATUS_NORMAL);
                    run_case(&w, E_RUN_SRC_UPD_FRQ);
                }
            }
        }
    }
}

/* ---- StartRunCtrl ------------------------------------------------------- */

static void phase_start_run_ctrl(void)
{
    static const Uint16 srs[] = { START_RUN_STATUS_SPEED_TRACK,
                                  START_RUN_STATUS_BRAKE,
                                  START_RUN_STATUS_PRE_FLUX,
                                  START_RUN_STATUS_HOLD_START_FRQ, 9 };

    ctx_reset();
    put16(&g_base, ST_OFF(runCmd), 0x0001);
    put16(&g_base, ST_OFF(runFlag), 0x0000);
    swenum(ST_OFF(startRunStatus), srs, NEL(srs), E_START_RUN_CTRL);
    sw16(offsetof(struct IN, startMode), U_STARTMODE, NEL(U_STARTMODE), E_START_RUN_CTRL);
    sw16(offsetof(struct IN, motorCtrlMode), U_MOTORMODE, NEL(U_MOTORMODE), E_START_RUN_CTRL);
    sw16(offsetof(struct IN, motorType), U_MOTORTYPE, NEL(U_MOTORTYPE), E_START_RUN_CTRL);
    sw16(offsetof(struct IN, bAntiReverseRun), U_BOOL, NEL(U_BOOL), E_START_RUN_CTRL);
    sw16(ST_OFF(runCmd), U_RUNCMD, NEL(U_RUNCMD), E_START_RUN_CTRL);
    sw16(ST_OFF(runFlag), U_RUNFLAG, NEL(U_RUNFLAG), E_START_RUN_CTRL);
    sw32(ST_OFF(runTimeTicker), U32_TICK, NEL(U32_TICK), E_START_RUN_CTRL);
    sw16(ST_OFF(bRunJog), U_BOOL, NEL(U_BOOL), E_START_RUN_CTRL);
    sw16(ST_OFF(dspStatus), U_DSPSTATUS, NEL(U_DSPSTATUS), E_START_RUN_CTRL);
    sw16(offsetof(struct IN, startBrakeTime), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_START_RUN_CTRL);
    sw16(offsetof(struct IN, startFrqTime), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_START_RUN_CTRL);
    sw16(offsetof(struct IN, startFrq), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_START_RUN_CTRL);
    swi32(offsetof(struct IN, frqFdbTmp), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_START_RUN_CTRL);
    swi32(offsetof(struct IN, frqRun), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_START_RUN_CTRL);
    swi32(SH_OFF(frqTmp), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_START_RUN_CTRL);
    swi32(SH_OFF(frqCurAim), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_START_RUN_CTRL);
    swi32(SH_OFF(frqCurAimOld), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_START_RUN_CTRL);
    sw16(SH_OFF(bFrqCurAimChg), U_BOOL, NEL(U_BOOL), E_START_RUN_CTRL);
    swenum(ST_OFF(runStatus), U_RUNSTATUS, NEL(U_RUNSTATUS), E_START_RUN_CTRL);

    /* The START_RUN_STATUS_* fall-through chain only advances when the ticker
     * crosses a threshold, so cross ticker with the start mode. The
     * HOLD_START_FRQ step also branches on motorCtrlMode (FVC re-seeds frqTmp
     * from the feedback), so the mode and the two frequency sources are crossed
     * in as well -- one-factor sweeps cannot have FVC and a non-zero
     * frqFdbTmp - frqRun at the same time.
     *
     * runCmd.bit.common must be set: StartRunCtrl returns at the top when it is
     * clear, so a block that leaves the template's 0x0001 never reaches the
     * switch at all. RUN_CMD_BITS: common0 = bit 0, common = bit 1. */
    {
        static const Uint16 modes[] = { FUNCCODE_startMode_SPEED_TRACK,
                                        FUNCCODE_startMode_DIRECT_START,
                                        FUNCCODE_startMode_FORE_MAG };
        static const Uint16 ctrlModes[] = { FUNCCODE_motorCtrlMode_VF,
                                            FUNCCODE_motorCtrlMode_SVC,
                                            FUNCCODE_motorCtrlMode_FVC };
        static const int32  fdb[] = { 0, 1000, -1000 };
        static const int32  run[] = { 0, 500, -500 };
        int i, j, m, n;
        struct IN w;
        for (i = 0; i < 4; i++)
        {
            for (j = 0; j < (int)NEL(modes); j++)
            {
                w = g_base;
                putenum(&w, ST_OFF(startRunStatus), (Uint32)srs[i]);
                put16(&w, ST_OFF(runCmd), 0x0003);
                w.startMode = modes[j];
                put32(&w, ST_OFF(runTimeTicker), 49);
                w.startBrakeTime = 1;
                w.startFrqTime = 1;
                run_case(&w, E_START_RUN_CTRL);

                put32(&w, ST_OFF(runTimeTicker), 51);
                run_case(&w, E_START_RUN_CTRL);
            }

            for (m = 0; m < (int)NEL(ctrlModes); m++)
            {
                for (n = 0; n < (int)NEL(fdb); n++)
                {
                    w = g_base;
                    putenum(&w, ST_OFF(startRunStatus),
                            START_RUN_STATUS_HOLD_START_FRQ);
                    put16(&w, ST_OFF(runCmd), 0x0003);
                    w.startMode = FUNCCODE_startMode_SPEED_TRACK;
                    w.motorCtrlMode = ctrlModes[m];
                    put16(&w, ST_OFF(bRunJog), 0);
                    put32(&w, ST_OFF(runTimeTicker), 49);
                    w.startFrqTime = 1;
                    puti32(&w, offsetof(struct IN, frqFdbTmp), fdb[n]);
                    puti32(&w, offsetof(struct IN, frqRun), run[n]);
                    run_case(&w, E_START_RUN_CTRL);

                    put32(&w, ST_OFF(runTimeTicker), 51);
                    run_case(&w, E_START_RUN_CTRL);
                }
            }
        }
    }
}

/* ---- NormalRunCtrl ------------------------------------------------------ */

static void phase_normal_run_ctrl(void)
{
    static const Uint16 runmodes[] = { RUN_MODE_SPEED_CTRL, RUN_MODE_TORQUE_CTRL,
                                       RUN_MODE_POSITION_CTRL };
    static const int32  pairs[][2] = {
        { 0, 0 }, { 100, 100 }, { 100, 200 }, { 0, 100 }, { 5000, 5000 }
    };
    int i, j;

    ctx_reset();
    put16(&g_base, ST_OFF(runCmd), 0x0002);
    put16(&g_base, ST_OFF(runFlag), 0x0001);
    sw16(ST_OFF(runCmd), U_RUNCMD, NEL(U_RUNCMD), E_NORMAL_RUN_CTRL);
    sw16(ST_OFF(runFlag), U_RUNFLAG, NEL(U_RUNFLAG), E_NORMAL_RUN_CTRL);
    sw16(offsetof(struct IN, runMode), U_RUNMODE, NEL(U_RUNMODE), E_NORMAL_RUN_CTRL);
    sw16(offsetof(struct IN, bAntiReverseRun), U_BOOL, NEL(U_BOOL), E_NORMAL_RUN_CTRL);
    swenum(ST_OFF(swingStatus), U_SWING_STATUS, NEL(U_SWING_STATUS), E_NORMAL_RUN_CTRL);
    sw32(offsetof(struct IN, diF1), U_DI1, NEL(U_DI1), E_NORMAL_RUN_CTRL);
    sw16(offsetof(struct IN, lowerFrq), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_NORMAL_RUN_CTRL);
    sw16(offsetof(struct IN, lowerDeal), U_LOWERDEAL, NEL(U_LOWERDEAL), E_NORMAL_RUN_CTRL);
    sw16(offsetof(struct IN, accDecSpdCurve), U_CURVE, NEL(U_CURVE), E_NORMAL_RUN_CTRL);
    sw32(ST_OFF(swingAccTime), U32_WIDE, NEL(U32_WIDE), E_NORMAL_RUN_CTRL);
    sw32(ST_OFF(swingDecTime), U32_WIDE, NEL(U32_WIDE), E_NORMAL_RUN_CTRL);
    sw32(ST_OFF(accFrqTime), U32_WIDE, NEL(U32_WIDE), E_NORMAL_RUN_CTRL);
    sw32(ST_OFF(decFrqTime), U32_WIDE, NEL(U32_WIDE), E_NORMAL_RUN_CTRL);
    sw16(SH_OFF(timeBench), U_BENCH, NEL(U_BENCH), E_NORMAL_RUN_CTRL);

    /* frq == frqAim vs != : the gate on SwingDeal. */
    for (i = 0; i < (int)NEL(pairs); i++)
    {
        for (j = 0; j < 3; j++)
        {
            struct IN w = g_base;
            puti32(&w, SH_OFF(frq), pairs[i][0]);
            puti32(&w, offsetof(struct IN, frqAim), pairs[i][1]);
            w.runMode = runmodes[j];
            putenum(&w, ST_OFF(swingStatus), (Uint32)SWING_NONE);
            run_case(&w, E_NORMAL_RUN_CTRL);
            putenum(&w, ST_OFF(swingStatus), (Uint32)SWING_UP);
            run_case(&w, E_NORMAL_RUN_CTRL);
            putenum(&w, ST_OFF(swingStatus), (Uint32)SWING_DOWN);
            run_case(&w, E_NORMAL_RUN_CTRL);
        }
    }
}

/* ---- StopRunCtrl -------------------------------------------------------- */

static void phase_stop_run_ctrl(void)
{
    static const Uint16 srs[] = { STOP_RUN_STATUS_DEC_STOP,
                                  STOP_RUN_STATUS_WAIT_BRAKE,
                                  STOP_RUN_STATUS_BRAKE, 9 };

    ctx_reset();
    put16(&g_base, ST_OFF(runCmd), 0x0000);
    put16(&g_base, ST_OFF(runFlag), 0x0001);
    swenum(ST_OFF(stopRunStatus), srs, NEL(srs), E_STOP_RUN_CTRL);
    sw16(ST_OFF(runCmd), U_RUNCMD, NEL(U_RUNCMD), E_STOP_RUN_CTRL);
    sw16(ST_OFF(runFlag), U_RUNFLAG, NEL(U_RUNFLAG), E_STOP_RUN_CTRL);
    sw16(offsetof(struct IN, ovGain), U_BOOL, NEL(U_BOOL), E_STOP_RUN_CTRL);
    sw16(offsetof(struct IN, ocGain), U_BOOL, NEL(U_BOOL), E_STOP_RUN_CTRL);
    sw16(offsetof(struct IN, motorCtrlMode), U_MOTORMODE, NEL(U_MOTORMODE), E_STOP_RUN_CTRL);
    sw16(offsetof(struct IN, stopBrakeFrq), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_STOP_RUN_CTRL);
    sw16(offsetof(struct IN, stopBrakeWaitTime), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_STOP_RUN_CTRL);
    sw16(offsetof(struct IN, stopBrakeTime), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_STOP_RUN_CTRL);
    sw16(offsetof(struct IN, accDecSpdCurve), U_CURVE, NEL(U_CURVE), E_STOP_RUN_CTRL);
    sw32(ST_OFF(accFrqTime), U32_WIDE, NEL(U32_WIDE), E_STOP_RUN_CTRL);
    sw32(ST_OFF(decFrqTime), U32_WIDE, NEL(U32_WIDE), E_STOP_RUN_CTRL);
    sw32(ST_OFF(runTimeTicker), U32_TICK, NEL(U32_TICK), E_STOP_RUN_CTRL);
    swi32(SH_OFF(frqTmp), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_STOP_RUN_CTRL);
    swi32(offsetof(struct IN, frqRun), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_STOP_RUN_CTRL);
    swi32(SH_OFF(frqCurAim), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_STOP_RUN_CTRL);
    swenum(ST_OFF(runStatus), U_RUNSTATUS, NEL(U_RUNSTATUS), E_STOP_RUN_CTRL);
}

/* ---- JogRunCtrl --------------------------------------------------------- */

static void phase_jog_run_ctrl(void)
{
    ctx_reset();
    put16(&g_base, ST_OFF(runFlag), 0x0005);
    put16(&g_base, ST_OFF(runCmd), 0x0004);
    putenum(&g_base, ST_OFF(runStatus), RUN_STATUS_JOG);
    sw16(ST_OFF(runCmd), U_RUNCMD, NEL(U_RUNCMD), E_JOG_RUN_CTRL);
    sw16(ST_OFF(runFlag), U_RUNFLAG, NEL(U_RUNFLAG), E_JOG_RUN_CTRL);
    swenum(ST_OFF(runStatusOld4Jog), U_RUNSTATUS, NEL(U_RUNSTATUS), E_JOG_RUN_CTRL);
    swi32(SH_OFF(frqTmp), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_JOG_RUN_CTRL);
    swi32(SH_OFF(frqCurAim), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_JOG_RUN_CTRL);
    sw32(offsetof(struct IN, diF1), U_DI1, NEL(U_DI1), E_JOG_RUN_CTRL);
    sw16(offsetof(struct IN, jogAccTime), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_JOG_RUN_CTRL);
    sw16(offsetof(struct IN, jogDecTime), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_JOG_RUN_CTRL);
    sw16(offsetof(struct IN, runDir), U_BOOL, NEL(U_BOOL), E_JOG_RUN_CTRL);
}

/* ---- TuneRunCtrl -------------------------------------------------------- */

static void phase_tune_run_ctrl(void)
{
    static const Uint32 ticker[] = { 0, 1, 89999, 90000, 90001, 100000 };

    ctx_reset();
    put16(&g_base, ST_OFF(runFlag), 0x0009);
    put16(&g_base, ST_OFF(runCmd), 0x0002);
    putenum(&g_base, ST_OFF(runStatus), RUN_STATUS_TUNE);
    sw16(ST_OFF(runCmd), U_RUNCMD, NEL(U_RUNCMD), E_TUNE_RUN_CTRL);
    sw16(ST_OFF(runFlag), U_RUNFLAG, NEL(U_RUNFLAG), E_TUNE_RUN_CTRL);
    sw16(ST_OFF(motorCtrlTuneStatus), U_TUNESTEP, NEL(U_TUNESTEP), E_TUNE_RUN_CTRL);
    sw16(ST_OFF(saveTuneDataFlag), U_BOOL, NEL(U_BOOL), E_TUNE_RUN_CTRL);
    sw32(ST_OFF(runTimeTicker), ticker, NEL(ticker), E_TUNE_RUN_CTRL);
    sw16(offsetof(struct IN, motorType), U_MOTORTYPE, NEL(U_MOTORTYPE), E_TUNE_RUN_CTRL);
    sw16(offsetof(struct IN, ratingFrq), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_TUNE_RUN_CTRL);
    sw16(offsetof(struct IN, ratingSpeed), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_TUNE_RUN_CTRL);
    sw16(offsetof(struct IN, runDir), U_BOOL, NEL(U_BOOL), E_TUNE_RUN_CTRL);
    sw16(offsetof(struct IN, accDecSpdCurve), U_CURVE, NEL(U_CURVE), E_TUNE_RUN_CTRL);
    sw32(ST_OFF(accFrqTime), U32_WIDE, NEL(U32_WIDE), E_TUNE_RUN_CTRL);
    sw32(ST_OFF(decFrqTime), U32_WIDE, NEL(U32_WIDE), E_TUNE_RUN_CTRL);
    swi32(SH_OFF(frqTmp), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_TUNE_RUN_CTRL);
    swi32(SH_OFF(frqCurAim), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_TUNE_RUN_CTRL);
    sw16(SH_OFF(errorOther), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_TUNE_RUN_CTRL);
    swenum(ST_OFF(runStatus), U_RUNSTATUS, NEL(U_RUNSTATUS), E_TUNE_RUN_CTRL);

    /* TUNE_STEP_ACC: the two motor-type branches and the reverse-direction
     * negation. Reachable only now that motorCtrlTuneStatus is swept with the
     * real TUNE_STEP_* values -- see the note at U_TUNESTEP.
     *
     * The PMSM branch divides by ratingSpeed, so it stays on the safe side of
     * the guard (ratingSpeed > PMSM_TUNE_SPEED == 10); see gap (c). The else
     * branch does not divide, so any ratingFrq is fine there. */
    {
        static const Uint16 pmsmSpeed[] = { 0, 1, 9, 10, 11, 1440 };
        static const Uint16 anyFrq[]    = { 0, 1, 5, 5000, 65535 };
        static const Uint16 types[]     = { MOTOR_TYPE_IM, MOTOR_TYPE_VARFREQ,
                                            MOTOR_TYPE_PMSM };
        int i, j;
        struct IN w;

        /* PMSM side of the guard: ratingSpeed around PMSM_TUNE_SPEED. */
        for (i = 0; i < (int)NEL(pmsmSpeed); i++)
        {
            w = g_base;
            w.motorType = MOTOR_TYPE_PMSM;
            w.ratingFrq = 5000;
            w.ratingSpeed = pmsmSpeed[i];
            w.runDir = FUNCCODE_runDir_NO_REVERSE;
            put16(&w, ST_OFF(motorCtrlTuneStatus), DRV_TUNE_STEP_ACC);
            put32(&w, ST_OFF(runTimeTicker), 0);
            run_case(&w, E_TUNE_RUN_CTRL);
        }

        /* Both branches x both run directions: motorType x ratingFrq x runDir.
         * Non-PMSM is what makes `ratingFrq * 4 / 5` differ from `* 5 / 4`, and
         * runDir=REVERSE with a non-PMSM type is what tells the `&&` in the
         * negation guard apart from an `||`. */
        for (i = 0; i < (int)NEL(types); i++)
        {
            for (j = 0; j < (int)NEL(anyFrq); j++)
            {
                w = g_base;
                w.motorType = types[i];
                w.ratingFrq = anyFrq[j];
                w.ratingSpeed = 1440;
                w.runDir = FUNCCODE_runDir_NO_REVERSE;
                put16(&w, ST_OFF(motorCtrlTuneStatus), DRV_TUNE_STEP_ACC);
                put32(&w, ST_OFF(runTimeTicker), 0);
                run_case(&w, E_TUNE_RUN_CTRL);

                w.runDir = FUNCCODE_runDir_REVERSE;
                run_case(&w, E_TUNE_RUN_CTRL);
            }
        }
    }

    /* TUNE_STEP_DEC / TUNE_STEP_END: the dec-arrival test `ABS_INT32(frqTmp) < 50`
     * (and the END step's saveTuneDataFlag latch) need two factors at once --
     * frqTmp exactly at the 50 boundary AND a tune step that reaches the DEC
     * case -- so neither one-factor sweep above can hit them.
     *
     * How the DEC case is reached: `motorCtrlTuneStatusTmp` is forced to
     * TUNE_STEP_DEC when saveTuneDataFlag is set (line 1718). The inner
     * `TUNE_STEP_DEC != motorCtrlTuneStatus` guard then still holds because the
     * global is left at TUNE_STEP_ACC -- which is exactly the state the real
     * firmware is in when it decides the ramp has arrived.
     *
     * frqTmp is not recomputed here: AccDecFrqCalc is a stub that only records
     * its arguments (see mock_runsrc.c), so the value the driver installs is the
     * value the module tests. */
    {
        static const int32 decFrq[] = { 0, 1, 49, 50, 51, -49, -50, -51, 100 };
        static const Uint16 steps[] = { DRV_TUNE_STEP_ACC, DRV_TUNE_STEP_DEC,
                                        DRV_TUNE_STEP_END, 49, 52 };
        int i, j;
        struct IN w;

        for (i = 0; i < (int)NEL(decFrq); i++)
        {
            for (j = 0; j < (int)NEL(steps); j++)
            {
                w = g_base;
                put16(&w, ST_OFF(runCmd), 0x0003);   /* common0 = 1, common = 1 */
                put16(&w, ST_OFF(runFlag), 0x0009);
                putenum(&w, ST_OFF(runStatus), RUN_STATUS_TUNE);
                put16(&w, ST_OFF(motorCtrlTuneStatus), steps[j]);
                put16(&w, ST_OFF(saveTuneDataFlag), 1);
                put32(&w, ST_OFF(runTimeTicker), 0);
                puti32(&w, SH_OFF(frqTmp), decFrq[i]);
                puti32(&w, SH_OFF(frqCurAim), 0);
                run_case(&w, E_TUNE_RUN_CTRL);

                /* Also with the latch clear, so the END case is entered directly
                 * from motorCtrlTuneStatus rather than through the latch. */
                put16(&w, ST_OFF(saveTuneDataFlag), 0);
                run_case(&w, E_TUNE_RUN_CTRL);
            }
        }
    }
}

/* ---- LoseLoadRunCtrl ---------------------------------------------------- */

static void phase_lose_load_ctrl(void)
{
    static const Uint16 attrs[] = { ERROR_LEVEL_NO_ERROR, ERROR_LEVEL_FREE_STOP,
                                    ERROR_LEVEL_STOP, ERROR_LEVEL_RUN };

    ctx_reset();
    putenum(&g_base, ST_OFF(runStatus), RUN_STATUS_LOSE_LOAD);
    put16(&g_base, ST_OFF(runFlag), 0x0001);
    put16(&g_base, ST_OFF(runCmd), 0x0002);
    sw16(ST_OFF(runCmd), U_RUNCMD, NEL(U_RUNCMD), E_LOSE_LOAD_CTRL);
    sw16(ST_OFF(runFlag), U_RUNFLAG, NEL(U_RUNFLAG), E_LOSE_LOAD_CTRL);
    sw16(ST_OFF(dspStatus), U_DSPSTATUS, NEL(U_DSPSTATUS), E_LOSE_LOAD_CTRL);
    swenum(ST_OFF(runStatusOld4LoseLoad), U_RUNSTATUS, NEL(U_RUNSTATUS), E_LOSE_LOAD_CTRL);
    swi32(offsetof(struct IN, frqAim), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_LOSE_LOAD_CTRL);
    swi32(SH_OFF(frqTmp), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_LOSE_LOAD_CTRL);
    sw16(offsetof(struct IN, ratingFrq), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_LOSE_LOAD_CTRL);
    sw16(offsetof(struct IN, mockErrorAttrLevel), attrs, NEL(attrs), E_LOSE_LOAD_CTRL);
    swenum(ST_OFF(runStatus), U_RUNSTATUS, NEL(U_RUNSTATUS), E_LOSE_LOAD_CTRL);
}

/* ---- ShutDownRunCtrl ---------------------------------------------------- */

static void phase_shut_down_ctrl(void)
{
    static const Uint16 plcRem[] = { 0, 9, 10, 11, 19, 20 };

    ctx_reset();
    put16(&g_base, ST_OFF(runFlag), 0x0003);
    put16(&g_base, ST_OFF(runCmd), 0x0002);
    putenum(&g_base, ST_OFF(runStatus), RUN_STATUS_SHUT_DOWN);
    sw16(ST_OFF(runFlag), U_RUNFLAG, NEL(U_RUNFLAG), E_SHUT_DOWN_CTRL);
    sw16(ST_OFF(runCmd), U_RUNCMD, NEL(U_RUNCMD), E_SHUT_DOWN_CTRL);
    sw16(offsetof(struct IN, errorCode), U_ERRCODE, NEL(U_ERRCODE), E_SHUT_DOWN_CTRL);
    sw16(offsetof(struct IN, plcPowerOffRemMode), plcRem, NEL(plcRem), E_SHUT_DOWN_CTRL);
    sw16(SH_OFF(plcStep), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_SHUT_DOWN_CTRL);
    sw32(SH_OFF(plcTime), U32_WIDE, NEL(U32_WIDE), E_SHUT_DOWN_CTRL);
    sw16(SH_OFF(plcStepRemOld), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_SHUT_DOWN_CTRL);
    sw32(SH_OFF(plcTimeRemOld), U32_WIDE, NEL(U32_WIDE), E_SHUT_DOWN_CTRL);
    sw16(ST_OFF(stopRemFlag), U_BOOL2, NEL(U_BOOL2), E_SHUT_DOWN_CTRL);
    sw32(ST_OFF(runTimeTicker), U32_WIDE, NEL(U32_WIDE), E_SHUT_DOWN_CTRL);
    sw32(ST_OFF(lowerDelayTicker), U32_WIDE, NEL(U32_WIDE), E_SHUT_DOWN_CTRL);
    sw32(ST_OFF(shuntTicker), U32_WIDE, NEL(U32_WIDE), E_SHUT_DOWN_CTRL);
    sw16(ST_OFF(dspMainCmd), U_MAINCMD, NEL(U_MAINCMD), E_SHUT_DOWN_CTRL);
    sw16(ST_OFF(dspMainCmd1), U_MAINCMD1, NEL(U_MAINCMD1), E_SHUT_DOWN_CTRL);
    sw16(ST_OFF(tuneCmd), U_BOOL, NEL(U_BOOL), E_SHUT_DOWN_CTRL);
    swi32(SH_OFF(frqTmp), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_SHUT_DOWN_CTRL);
    swi32(SH_OFF(frqTmpFrac), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_SHUT_DOWN_CTRL);
    swi32(SH_OFF(frqCurAimOld), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_SHUT_DOWN_CTRL);
    swi32(SH_OFF(accTimeOld), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_SHUT_DOWN_CTRL);
    swi32(SH_OFF(decTimeOld), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_SHUT_DOWN_CTRL);
    swenum(ST_OFF(swingStatus), U_SWING_STATUS, NEL(U_SWING_STATUS), E_SHUT_DOWN_CTRL);
    swenum(ST_OFF(runStatus), U_RUNSTATUS, NEL(U_RUNSTATUS), E_SHUT_DOWN_CTRL);
}

/* ---- UpdateSwingPara / SwingDeal ---------------------------------------- */

/* swing_safe() is defined with the sweep helpers above, because the guarded
 * sweep variants live there too. */

static void phase_update_swing_para(void)
{
    static const Uint16 base_mode[] = { FUNCCODE_swingBaseMode_AGAIN_MAXFRQ, 0 };
    static const Uint16 amp[]  = { 100, 250, 500, 1000, 2000 };
    static const Uint16 jump[] = { 0, 100, 500, 1000, 2000 };
    static const Uint16 period[] = { 0, 1, 100, 1000, 10000, 65535 };
    static const Uint16 coeff[]  = { 0, 1, 500, 1000, 1500, 2000 };
    static const Uint16 bench[]  = { 10, 100, 1000 };
    static const int32  aims[] = { 0, 100, 1000, 5000, -100, -1000, -5000 };
    static const Uint16 frqs[] = { 0, 100, 1000, 5000 };
    int i, j;

    /* One-factor sweeps, each guarded by swing_safe(). */
    {
        struct IN w;
        int k;
        const Uint16 *lists[6];
        int           lens[6];
        size_t        offs[6];

        lists[0] = base_mode; lens[0] = NEL(base_mode); offs[0] = offsetof(struct IN, swingBaseMode);
        lists[1] = amp;       lens[1] = NEL(amp);       offs[1] = offsetof(struct IN, swingAmplitude);
        lists[2] = jump;      lens[2] = NEL(jump);      offs[2] = offsetof(struct IN, swingJumpRange);
        lists[3] = period;    lens[3] = NEL(period);    offs[3] = offsetof(struct IN, swingPeriod);
        lists[4] = coeff;     lens[4] = NEL(coeff);     offs[4] = offsetof(struct IN, swingRiseTimeCoeff);
        lists[5] = bench;     lens[5] = NEL(bench);     offs[5] = SH_OFF(timeBench);

        ctx_reset();
        put16(&g_base, offsetof(struct IN, maxFrq), 5000);
        puti32(&g_base, offsetof(struct IN, frqAim), 1000);
        for (k = 0; k < 6; k++)
        {
            for (i = 0; i < lens[k]; i++)
            {
                w = g_base;
                put16(&w, offs[k], lists[k][i]);
                if (swing_safe(&w)) { run_case(&w, E_UPDATE_SWING_PARA); }
            }
        }
    }

    /* frqAim x maxFrq x swingBaseMode, and upperFrq/lowerFrq clamping.
     *
     * swingBaseMode matters: with AGAIN_MAXFRQ the base is maxFrq, and the
     * `swingAwBase = ABS_INT32(frqAim)` arm is dead. Only AGAIN_FRQSETAIM with a
     * negative frqAim tells that arm apart from a bare `= frqAim`. The template
     * sets AGAIN_MAXFRQ (to keep the divisor positive, gap (a)), so this has to
     * be crossed in explicitly. */
    for (i = 0; i < (int)NEL(aims); i++)
    {
        for (j = 0; j < (int)NEL(frqs); j++)
        {
            int m;
            for (m = 0; m < 2; m++)
            {
                struct IN w = g_base;
                puti32(&w, offsetof(struct IN, frqAim), aims[i]);
                put16(&w, offsetof(struct IN, maxFrq), frqs[j]);
                put16(&w, offsetof(struct IN, swingBaseMode),
                      m ? FUNCCODE_swingBaseMode_AGAIN_MAXFRQ
                        : FUNCCODE_swingBaseMode_AGAIN_FRQSETAIM);
                if (!swing_safe(&w)) { continue; }
                run_case(&w, E_UPDATE_SWING_PARA);

                w.upperFrq = 100;
                w.lowerFrq = 0;
                if (swing_safe(&w)) { run_case(&w, E_UPDATE_SWING_PARA); }

                w.upperFrq = 5000;
                w.lowerFrq = 2000;
                if (swing_safe(&w)) { run_case(&w, E_UPDATE_SWING_PARA); }

                put16(&w, ST_OFF(runFlag), 0x2000);   /* dirFinal = REVERSE */
                if (swing_safe(&w)) { run_case(&w, E_UPDATE_SWING_PARA); }
            }
        }
    }
}

static void phase_swing_deal(void)
{
    static const Uint16 period[] = { 0, 1, 100, 1000 };
    static const Uint16 amp[]    = { 0, 100, 1000 };
    static const int32  aims[]   = { 0, 100, 1000, 5000 };
    static const int32  tmps[]   = { 0, 100, 1000, 5000, -100, -1000 };
    static const int32  swmax[]  = { 0, 500, 1000, -500, -1000 };
    static const int32  swmin[]  = { 0, -500, -1000, 500, 1000 };
    static const int32  swjmp[]  = { 0, 100, 500, -100 };
    static const Uint32 swdaw[]  = { 0, 100, 1000, 2000 };
    int i;

    ctx_reset();
    putenum(&g_base, ST_OFF(swingStatus), SWING_UP);
    sw16(ST_OFF(swingStatus), U_SWING_STATUS, NEL(U_SWING_STATUS), E_SWING_DEAL);
    sw16(offsetof(struct IN, swingPeriod), period, NEL(period), E_SWING_DEAL);
    /* swingAmplitude == 0 makes the divisor 0 (gap (a)), so this one is guarded.
     * The amplitude-0 branch -- `swingStatus = SWING_NONE; return;` -- sits
     * *after* UpdateSwingPara, so it cannot be reached through SwingDeal at all;
     * it is covered through phase_update_swing_para's guarded sweep instead. */
    sw16s(offsetof(struct IN, swingAmplitude), amp, NEL(amp), E_SWING_DEAL);
    sw32(offsetof(struct IN, diF1), U_DI1, NEL(U_DI1), E_SWING_DEAL);
    swi32s(offsetof(struct IN, frqAim), aims, NEL(aims), E_SWING_DEAL);
    swi32(SH_OFF(frqTmp), tmps, NEL(tmps), E_SWING_DEAL);
    swi32(SH_OFF(frqCurAim), aims, NEL(aims), E_SWING_DEAL);
    swi32(ST_OFF(swingMaxFrq), swmax, NEL(swmax), E_SWING_DEAL);
    swi32(ST_OFF(swingMinFrq), swmin, NEL(swmin), E_SWING_DEAL);
    swi32(ST_OFF(swingJumpFrq), swjmp, NEL(swjmp), E_SWING_DEAL);
    sw32(ST_OFF(swingDoubleAw), swdaw, NEL(swdaw), E_SWING_DEAL);

    /* frqTmp exactly on the swing bound is what advances the state machine.
     * Unguarded on purpose: every value here comes from g_pristine, whose swing
     * configuration init_base() pins to a safe one. If that ever stopped being
     * true this would crash rather than silently skip, which is the right
     * failure mode for a test rig. */
    for (i = 0; i < (int)NEL(swmax); i++)
    {
        struct IN w = g_base;
        puti32(&w, ST_OFF(swingMaxFrq), swmax[i]);
        puti32(&w, ST_OFF(swingMinFrq), swmin[i % NEL(swmin)]);
        puti32(&w, ST_OFF(swingJumpFrq), swjmp[i % NEL(swjmp)]);
        put32(&w, ST_OFF(swingDoubleAw), swdaw[i % NEL(swdaw)]);

        puti32(&w, SH_OFF(frqTmp), swmax[i]);
        putenum(&w, ST_OFF(swingStatus), SWING_UP);
        run_case(&w, E_SWING_DEAL);

        puti32(&w, SH_OFF(frqTmp), swmin[i % NEL(swmin)]);
        putenum(&w, ST_OFF(swingStatus), SWING_DOWN);
        run_case(&w, E_SWING_DEAL);
    }
}

/* ---- LowerThanLowerFrqDeal --------------------------------------------- */

static void phase_lower_frq(void)
{
    static const Uint16 lows[] = { 0, 1, 50, 100, 500, 5000 };

    ctx_reset();
    putenum(&g_base, ST_OFF(runStatus), RUN_STATUS_NORMAL);
    swi32(offsetof(struct IN, frqAim), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_LOWER_FRQ);
    sw16(offsetof(struct IN, lowerFrq), lows, NEL(lows), E_LOWER_FRQ);
    sw16(offsetof(struct IN, bAntiReverseRun), U_BOOL, NEL(U_BOOL), E_LOWER_FRQ);
    sw16(offsetof(struct IN, lowerDeal), U_LOWERDEAL, NEL(U_LOWERDEAL), E_LOWER_FRQ);
    swi32(SH_OFF(frq), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_LOWER_FRQ);
    swi32(SH_OFF(frqCurAim), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_LOWER_FRQ);
    sw16(ST_OFF(runFlag), U_RUNFLAG, NEL(U_RUNFLAG), E_LOWER_FRQ);
    swenum(ST_OFF(runStatus), U_RUNSTATUS, NEL(U_RUNSTATUS), E_LOWER_FRQ);
    swenum(ST_OFF(stopRunStatus), U_BOOL2, NEL(U_BOOL2), E_LOWER_FRQ);
    sw16(ST_OFF(otherStopLowerSrc), U_BOOL2, NEL(U_BOOL2), E_LOWER_FRQ);
    sw32(ST_OFF(lowerDelayTicker), U32_WIDE, NEL(U32_WIDE), E_LOWER_FRQ);

    /* frqAim and frq on the same side of lowerFrq: the DELAY_STOP rule reads
     * both, and only the "frq exactly equal" case is distinct. */
    {
        static const int32 pairs[][2] = {
            { 0, 0 }, { 99, 99 }, { 100, 100 }, { 101, 101 },
            { 100, 0 }, { 0, 100 }, { 100, 101 }, { 101, 100 }
        };
        int i;
        struct IN w;
        for (i = 0; i < (int)NEL(pairs); i++)
        {
            w = g_base;
            w.lowerFrq = 100;
            w.lowerDeal = FUNCCODE_lowerDeal_DELAY_STOP;
            puti32(&w, offsetof(struct IN, frqAim), pairs[i][0]);
            puti32(&w, SH_OFF(frq), pairs[i][1]);
            run_case(&w, E_LOWER_FRQ);

            w.lowerDeal = FUNCCODE_lowerDeal_RUN_ZERO;
            run_case(&w, E_LOWER_FRQ);

            w.lowerDeal = FUNCCODE_lowerDeal_RUN_LOWER;
            put16(&w, ST_OFF(runFlag), 0x2000);
            run_case(&w, E_LOWER_FRQ);
        }
    }
}

/* ---- PowerOffNoStopDeal ------------------------------------------------- */

static void phase_power_off_no_stop(void)
{
    static const Uint16 udc[] = { 0, 1, 100, 380, 1000, 5000 };
    static const Uint16 thres[] = { 0, 1, 100, 300, 1000, 5000 };
    static const Uint16 genv[] = { 0, 1, 100, 500, 1000, 5000, 65535 };
    static const Uint16 back[] = { 0, 1, 10, 100 };
    static const Uint16 slope[] = { 0, 1, 100, 1000, 5000 };

    ctx_reset();
    g_base.pOffTransitoryNoStop = 1;
    put16(&g_base, ST_OFF(runCmd), 0x0001);
    put16(&g_base, ST_OFF(runFlag), 0x0001);
    sw16(offsetof(struct IN, pOffTransitoryNoStop), U_BOOL2, NEL(U_BOOL2), E_POWER_OFF_NO_STOP);
    sw16(ST_OFF(runCmd), U_RUNCMD, NEL(U_RUNCMD), E_POWER_OFF_NO_STOP);
    sw16(ST_OFF(runFlag), U_RUNFLAG, NEL(U_RUNFLAG), E_POWER_OFF_NO_STOP);
    sw16(offsetof(struct IN, ratingVoltage), udc, NEL(udc), E_POWER_OFF_NO_STOP);
    sw16(offsetof(struct IN, pOffThresholdVol), thres, NEL(thres), E_POWER_OFF_NO_STOP);
    sw16(offsetof(struct IN, generatrixVoltage), genv, NEL(genv), E_POWER_OFF_NO_STOP);
    sw16(offsetof(struct IN, pOffVolBackTime), back, NEL(back), E_POWER_OFF_NO_STOP);
    sw16(offsetof(struct IN, pOffTransitoryFrqDecSlope), slope, NEL(slope), E_POWER_OFF_NO_STOP);
    sw16(ST_OFF(shuntFlag), U_BOOL, NEL(U_BOOL), E_POWER_OFF_NO_STOP);
    sw32(ST_OFF(shuntTicker), U32_WIDE, NEL(U32_WIDE), E_POWER_OFF_NO_STOP);
    swenum(ST_OFF(runStatusOld4POffNoStop), U_RUNSTATUS, NEL(U_RUNSTATUS), E_POWER_OFF_NO_STOP);
    swenum(ST_OFF(runStatus), U_RUNSTATUS, NEL(U_RUNSTATUS), E_POWER_OFF_NO_STOP);
    swi32(offsetof(struct IN, frqRun), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_POWER_OFF_NO_STOP);
    sw32(ST_OFF(accFrqTime), U32_WIDE, NEL(U32_WIDE), E_POWER_OFF_NO_STOP);
    sw16(offsetof(struct IN, decTime4), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_POWER_OFF_NO_STOP);
    sw16(offsetof(struct IN, accDecSpdCurve), U_CURVE, NEL(U_CURVE), E_POWER_OFF_NO_STOP);

    /* The ride-through only leaves the shunt state when the ticker crosses
     * pOffVolBackTime * (TIME_UNIT_P_OFF_VOL_BACK / RUN_CTRL_PERIOD), i.e.
     * back * 5. Two things this block has to get right, both of which were
     * wrong before:
     *
     *   - the body is only reached when runCmd.bit.common or runCmd.bit.jog is
     *     set (otherwise the function returns at the top). The template has
     *     runCmd = 0x0001, which has neither, so the whole ride-through below
     *     was unreachable here.
     *   - the boundary is `++shuntTicker >= back * 5`, so the ticker has to be
     *     placed exactly at back * 5 - 1 to tell `>=` from `>`. A fixed list of
     *     ticker values cannot do that for every `back`, so it is computed.
     *
     * RUN_CMD_BITS: common0 = bit 0, common = bit 1, jog = bits 2-3. */
    {
        int i, d;
        struct IN w;
        for (i = 0; i < (int)NEL(back); i++)
        {
            for (d = -1; d <= 1; d++)
            {
                Uint32 x = (Uint32)back[i] * 5;
                int k;
                for (k = 0; k < 2; k++)
                {
                    w = g_base;
                    w.pOffTransitoryNoStop = 1;
                    w.pOffVolBackTime = back[i];
                    w.generatrixVoltage = 5000;  /* above threshold: back branch */
                    w.ratingVoltage = 380;
                    w.pOffThresholdVol = 1;
                    put16(&w, ST_OFF(runCmd), k ? 0x0004 : 0x0002);
                    put16(&w, ST_OFF(shuntFlag), 1);
                    put32(&w, ST_OFF(shuntTicker), x + (Uint32)d);
                    putenum(&w, ST_OFF(runStatus), RUN_STATUS_NORMAL);
                    putenum(&w, ST_OFF(runStatusOld4POffNoStop), RUN_STATUS_JOG);
                    run_case(&w, E_POWER_OFF_NO_STOP);
                }
            }
        }
    }
}

/* ---- AccDecTimeCalc ---------------------------------------------------- */

static void phase_acc_dec_time_calc(void)
{
    static const Uint16 srcs[] = { 0, 1, 2, 3, 7 };
    static const Uint16 mtrs[] = { 0, 1, 2, 3, 4, 5, 255 };
    static const Uint16 accstat[] = { 0, ACC_SPEED, DEC_SPEED, 3 };

    ctx_reset();
    sw16(offsetof(struct IN, runMode), U_RUNMODE, NEL(U_RUNMODE), E_ACC_DEC_TIME_CALC);
    sw16(ST_OFF(runFlag), U_RUNFLAG, NEL(U_RUNFLAG), E_ACC_DEC_TIME_CALC);
    sw32(offsetof(struct IN, diSelF1), U_DI1, NEL(U_DI1), E_ACC_DEC_TIME_CALC);
    sw32(offsetof(struct IN, diF1), U_DI1, NEL(U_DI1), E_ACC_DEC_TIME_CALC);
    sw32(offsetof(struct IN, diF2), U_DI2, NEL(U_DI2), E_ACC_DEC_TIME_CALC);
    swi32(SH_OFF(frq), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_ACC_DEC_TIME_CALC);
    sw16(offsetof(struct IN, accTimefrqChgValue), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_ACC_DEC_TIME_CALC);
    sw16(offsetof(struct IN, decTimefrqChgValue), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_ACC_DEC_TIME_CALC);
    sw16(offsetof(struct IN, motorSn), U_BOOL2, NEL(U_BOOL2), E_ACC_DEC_TIME_CALC);
    sw16(offsetof(struct IN, accDecTimeMotor), mtrs, NEL(mtrs), E_ACC_DEC_TIME_CALC);
    sw16(ST_OFF(accDecTimeSrcPlc), srcs, NEL(srcs), E_ACC_DEC_TIME_CALC);
    sw16(offsetof(struct IN, accDecTimeUnit), U_ACCUNIT, NEL(U_ACCUNIT), E_ACC_DEC_TIME_CALC);
    sw16(offsetof(struct IN, accTime1), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_ACC_DEC_TIME_CALC);
    sw16(offsetof(struct IN, decTime4), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_ACC_DEC_TIME_CALC);

    /* runFlag.accDecStatus selects between accTimefrqChgValue and
     * decTimefrqChgValue; cross them with |frq|. */
    {
        static const int32 frqs[] = { 0, 1, 49, 50, 51, 5000, -100 };
        int i, j;
        struct IN w;
        for (i = 0; i < (int)NEL(accstat); i++)
        {
            for (j = 0; j < (int)NEL(frqs); j++)
            {
                w = g_base;
                put16(&w, ST_OFF(runFlag), (Uint16)(0x0001 | ((Uint16)accstat[i] << 5)));
                puti32(&w, SH_OFF(frq), frqs[j]);
                w.accTimefrqChgValue = 50;
                w.decTimefrqChgValue = 50;
                put16(&w, offsetof(struct IN, motorSn), MOTOR_SN_1);
                run_case(&w, E_ACC_DEC_TIME_CALC);
            }
        }
    }

    /* stop4DecFlag is a sticky static: diFunc.f2.stop4dec sets it, !runFlag.run
     * clears it. Drive both in order. */
    {
        struct IN w = g_base;
        put16(&w, ST_OFF(runFlag), 0x0001);
        put32(&w, offsetof(struct IN, diF2), 1UL << 16);   /* stop4dec */
        w.decTime4 = 7;
        run_case(&w, E_ACC_DEC_TIME_CALC);

        put16(&w, ST_OFF(runFlag), 0x0000);
        put32(&w, offsetof(struct IN, diF2), 0);
        w.decTime4 = 9;
        run_case(&w, E_ACC_DEC_TIME_CALC);

        put32(&w, offsetof(struct IN, diF2), 1UL << 15);   /* emergencyStop */
        w.decTime4 = 11;
        run_case(&w, E_ACC_DEC_TIME_CALC);
    }
}

/* ---- DormantDeal ------------------------------------------------------- */

static void phase_dormant_deal(void)
{
    static const int16 volt[] = { -32768, -1, 0, 1, 100, 500, 1000, 32767 };
    static const Uint16 thr[] = { 0, 1, 100, 500, 1000, 65535 };
    static const Uint16 tset[] = { 0, 1, 10, 100, 1000 };

    ctx_reset();
    put16(&g_base, ST_OFF(runFlag), 0x0001);
    putenum(&g_base, ST_OFF(dormantStatus), DORMANT_RESPOND);
    sw16(offsetof(struct IN, f9_31), thr, NEL(thr), E_DORMANT_DEAL);
    sw16(offsetof(struct IN, f9_32), thr, NEL(thr), E_DORMANT_DEAL);
    sw16(offsetof(struct IN, f9_33), tset, NEL(tset), E_DORMANT_DEAL);
    sw16(offsetof(struct IN, f9_34), tset, NEL(tset), E_DORMANT_DEAL);
    sw16(offsetof(struct IN, wakeUpFrq), U_FRQ_WIDE, NEL(U_FRQ_WIDE), E_DORMANT_DEAL);
    sw16(ST_OFF(runFlag), U_RUNFLAG, NEL(U_RUNFLAG), E_DORMANT_DEAL);
    swenum(ST_OFF(dormantStatus), U_BOOL2, NEL(U_BOOL2), E_DORMANT_DEAL);
    swi32(SH_OFF(frqAimTmp0), I_FRQ_WIDE, NEL(I_FRQ_WIDE), E_DORMANT_DEAL);

    {
        int i;
        struct IN w;
        for (i = 0; i < (int)NEL(volt); i++)
        {
            w = g_base;
            w.ai1Voltage = volt[i];
            w.f9_31 = 500;
            w.f9_32 = 100;
            put16(&w, ST_OFF(runFlag), 0x0001);
            run_case(&w, E_DORMANT_DEAL);

            put16(&w, ST_OFF(runFlag), 0x0000);
            putenum(&w, ST_OFF(dormantStatus), DORMANT_NO_RESPOND);
            run_case(&w, E_DORMANT_DEAL);
        }
    }

    /* The two timers are function-local statics (stopTicker / startTicker), so
     * their `++ticker >= tickSet` boundaries cannot be reached by installing an
     * input value -- the counter has to be walked up by calling the module
     * repeatedly in one phase. The tick set is
     * f9_3x * TIME_UNIT_WAKE_UP / RUN_CTRL_PERIOD = f9_3x * 100 / 2 = f9_3x * 50,
     * so with f9_33 = f9_34 = 1 the thresholds are 50; 60 consecutive calls
     * cross them whatever the statics held on entry. Both the stop side
     * (runFlag.bit.run, voltage above the high threshold) and the wake side
     * (not running, below the low threshold) are walked. */
    {
        static const Uint16 tset[] = { 1, 2, 10 };
        int i, k;
        struct IN w;

        for (i = 0; i < (int)NEL(tset); i++)
        {
            w = g_base;
            w.ai1Voltage = 1000;
            w.f9_31 = 500;
            w.f9_32 = 100;
            w.f9_33 = tset[i];
            w.f9_34 = tset[i];
            put16(&w, ST_OFF(runFlag), 0x0001);
            putenum(&w, ST_OFF(dormantStatus), DORMANT_RESPOND);
            for (k = 0; k < 60; k++)
            {
                run_case(&w, E_DORMANT_DEAL);
            }

            w.ai1Voltage = 0;
            put16(&w, ST_OFF(runFlag), 0x0000);
            putenum(&w, ST_OFF(dormantStatus), DORMANT_NO_RESPOND);
            puti32(&w, SH_OFF(frqAimTmp0), 0);
            for (k = 0; k < 60; k++)
            {
                run_case(&w, E_DORMANT_DEAL);
            }
        }
    }
}

/* ---- RunSrcDeal (the whole periodic task) ------------------------------ */

static void phase_deal(void)
{
    static const Uint16 statuses[] = { RUN_STATUS_WAIT, RUN_STATUS_START,
                                       RUN_STATUS_NORMAL, RUN_STATUS_STOP,
                                       RUN_STATUS_JOG, RUN_STATUS_TUNE,
                                       RUN_STATUS_DI_BRAKE_DEC, RUN_STATUS_DI_BRAKE,
                                       RUN_STATUS_LOSE_LOAD, RUN_STATUS_SHUT_DOWN };
    static const Uint16 cmds[] = { 0x0001, 0x0002, 0x0003, 0x0004, 0x000C,
                                   0x0002 | 0x0004, 0x0002 | 0x0001, 0x0000 };
    static const Uint32 di2[]  = { 0, (1UL << 15), (1UL << 16), (1UL << 17),
                                   (1UL << 0), (1UL << 18) };
    int i, j, k, m;

    for (i = 0; i < (int)NEL(statuses); i++)
    {
        for (j = 0; j < (int)NEL(cmds); j++)
        {
            for (k = 0; k < (int)NEL(di2); k++)
            {
                struct IN w;
                w = g_pristine;
                w.fcRunSrc = FUNCCODE_runSrc_PANEL;
                put16(&w, ST_OFF(runFlag), 0x0003);
                putenum(&w, ST_OFF(runStatus), (Uint32)statuses[i]);
                put16(&w, ST_OFF(runCmd), cmds[j]);
                put32(&w, offsetof(struct IN, diF2), di2[k]);
                run_case(&w, E_RUN_SRC_DEAL);

                for (m = 0; m < 4; m++)
                {
                    w.errorLevel = U_ERRLEV[m];
                    w.errorCode  = (m == 0) ? ERROR_NONE : ERROR_OV_ACC_SPEED;
                    w.errorRunFrqSrc = U_ERRSRC[m];
                    run_case(&w, E_RUN_SRC_DEAL);
                }
            }
        }
    }
}

/* ---- Random and stateful sequence ------------------------------------- */

static unsigned long rnd_state = 20261003UL;

static unsigned long rnd(void)
{
    rnd_state = rnd_state * 1103515245UL + 12345UL;
    return (rnd_state >> 16) & 0x7FFFUL;
}

static Uint16 pick16(const Uint16 *v, int n) { return v[rnd() % (unsigned)n]; }
static Uint32 pick32(const Uint32 *v, int n) { return v[rnd() % (unsigned)n]; }

static void randomize(struct IN *w)
{
    *w = g_pristine;

    w->fcRunSrc     = pick16(U_RUNSRC, NEL(U_RUNSRC));
    w->diControlMode= pick16(U_DIMODE, NEL(U_DIMODE));
    w->diF1         = pick32(U_DI1, NEL(U_DI1));
    w->diF2         = pick32(U_DI2, NEL(U_DI2));
    w->diSelF1      = pick32(U_DI1, NEL(U_DI1));
    w->errorLevel   = pick16(U_ERRLEV, NEL(U_ERRLEV));
    w->errorCode    = pick16(U_ERRCODE, NEL(U_ERRCODE));
    w->powerOnStatus= pick16(U_POWERON, NEL(U_POWERON));
    w->funcCodeRwMode = pick16(U_RW_MODE, NEL(U_RW_MODE));
    w->mainLoopTicker = pick16(U_TICKER, NEL(U_TICKER));
    w->runMode      = pick16(U_RUNMODE, NEL(U_RUNMODE));
    w->accDecSpdCurve = pick16(U_CURVE, NEL(U_CURVE));
    w->accDecTimeUnit = pick16(U_ACCUNIT, NEL(U_ACCUNIT));
    w->lowerDeal    = pick16(U_LOWERDEAL, NEL(U_LOWERDEAL));
    w->startMode    = pick16(U_STARTMODE, NEL(U_STARTMODE));
    w->motorCtrlMode= pick16(U_MOTORMODE, NEL(U_MOTORMODE));
    w->motorType    = pick16(U_MOTORTYPE, NEL(U_MOTORTYPE));
    w->stopMode     = pick16(U_STOPMODE, NEL(U_STOPMODE));
    w->errorRunFrqSrc = pick16(U_ERRSRC, NEL(U_ERRSRC));
    w->setTimeSource  = pick16(U_TIMESRC, NEL(U_TIMESRC));
    w->runDir       = pick16(U_BOOL, NEL(U_BOOL));
    w->bAntiReverseRun = pick16(U_BOOL, NEL(U_BOOL));
    w->plcEnable    = pick16(U_BOOL, NEL(U_BOOL));
    w->plcCmd       = pick16(U_COMMRUNCMD, NEL(U_COMMRUNCMD));
    w->ovGain       = pick16(U_BOOL, NEL(U_BOOL));
    w->ocGain       = pick16(U_BOOL, NEL(U_BOOL));
    w->startProtect = pick16(U_BOOL, NEL(U_BOOL));
    w->pidCalcMode  = pick16(U_BOOL, NEL(U_BOOL));
    w->jogWhenRun   = pick16(U_BOOL, NEL(U_BOOL));
    w->mfkKeyFunc   = pick16(U_BOOL2, NEL(U_BOOL2));
    w->stopKeyFunc  = pick16(U_BOOL, NEL(U_BOOL));
    w->loseLoadProtectMode = pick16(U_BOOL, NEL(U_BOOL));
    w->frqRemMode   = pick16(U_BOOL, NEL(U_BOOL));
    w->pOffTransitoryNoStop = pick16(U_BOOL2, NEL(U_BOOL2));
    w->setTimeMode  = pick16(U_BOOL, NEL(U_BOOL));
    w->motorSn      = pick16(U_BOOL2, NEL(U_BOOL2));
    w->accDecTimeMotor = pick16(U_BOOL2, NEL(U_BOOL2));
    w->plcPowerOffRemMode = (Uint16)(pick16(U_BOOL2, NEL(U_BOOL2)) * 10);
    w->mockErrorAttrLevel = pick16(U_ERRLEV, NEL(U_ERRLEV));

    w->upperFrq  = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->lowerFrq  = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->fcMaxFrq  = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->startFrq  = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->stopBrakeFrq = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->startBrakeTime = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->startFrqTime   = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->stopBrakeTime  = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->stopBrakeWaitTime = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->jogAccTime = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->jogDecTime = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->accTimefrqChgValue = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->decTimefrqChgValue = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->wakeUpFrq  = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->errorSecondFrq = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->ratingFrq  = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->ratingSpeed= pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->ratingVoltage = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->pOffThresholdVol = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->pOffVolBackTime  = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->pOffTransitoryFrqDecSlope = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->generatrixVoltage = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->setTimeValue = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->setTimeArrive = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->runTimeArriveSet = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->powerUpTimeArriveSet = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->f9_31 = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->f9_32 = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->f9_33 = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->f9_34 = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->aiSet[0] = (int16)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->aiSet[1] = (int16)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->aiSet[2] = (int16)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->ai1Voltage = (int16)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));

    /* Swing: only safe configurations (gaps a and b). */
    w->swingBaseMode      = pick16(U_BOOL, NEL(U_BOOL));
    w->swingAmplitude     = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->swingJumpRange     = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->swingPeriod        = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->swingRiseTimeCoeff = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->maxFrq = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    if (!swing_safe(w))
    {
        w->swingBaseMode  = FUNCCODE_swingBaseMode_AGAIN_MAXFRQ;
        w->swingAmplitude = 1000;
        w->swingJumpRange = 100;
        w->maxFrq         = 5000;
        w->frqAim         = 1000;
        if (w->sh0.timeBench == 0) { w->sh0.timeBench = 1000; }
    }

    w->frqAim     = (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)) - 2500;
    w->frqAimTmp  = (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)) - 2500;
    w->frqFdbTmp  = (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)) - 2500;
    w->frqRun     = (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)) - 2500;

    put16(w, ST_OFF(runCmd), pick16(U_RUNCMD, NEL(U_RUNCMD)));
    put16(w, ST_OFF(runFlag), pick16(U_RUNFLAG, NEL(U_RUNFLAG)));
    put16(w, ST_OFF(dspMainCmd), pick16(U_MAINCMD, NEL(U_MAINCMD)));
    put16(w, ST_OFF(dspMainCmd1), pick16(U_MAINCMD1, NEL(U_MAINCMD1)));
    put16(w, ST_OFF(dspStatus), pick16(U_DSPSTATUS, NEL(U_DSPSTATUS)));
    put16(w, ST_OFF(dspSubCmd), pick16(U_BOOL, NEL(U_BOOL)));
    put16(w, ST_OFF(runStatus1Step), pick16(U_BOOL2, NEL(U_BOOL2)));
    putenum(w, ST_OFF(runStatus), pick16(U_RUNSTATUS, NEL(U_RUNSTATUS)));
    putenum(w, ST_OFF(startRunStatus), pick16(U_BOOL2, NEL(U_BOOL2)));
    putenum(w, ST_OFF(stopRunStatus), pick16(U_BOOL2, NEL(U_BOOL2)));
    putenum(w, ST_OFF(runStatusOld4LoseLoad), pick16(U_RUNSTATUS, NEL(U_RUNSTATUS)));
    putenum(w, ST_OFF(runStatusOld4Jog), pick16(U_RUNSTATUS, NEL(U_RUNSTATUS)));
    putenum(w, ST_OFF(runStatusOld4POffNoStop), pick16(U_RUNSTATUS, NEL(U_RUNSTATUS)));
    putenum(w, ST_OFF(swingStatus), pick16(U_SWING_STATUS, NEL(U_SWING_STATUS)));
    putenum(w, ST_OFF(dormantStatus), pick16(U_BOOL2, NEL(U_BOOL2)));
    put16(w, ST_OFF(runSrc), pick16(U_RUNSRC, NEL(U_RUNSRC)));
    put16(w, ST_OFF(startProtectSrc), pick16(U_BOOL2, NEL(U_BOOL2)));
    put16(w, ST_OFF(otherStopSrc), pick16(U_BOOL2, NEL(U_BOOL2)));
    put16(w, ST_OFF(otherStopLowerSrc), pick16(U_BOOL2, NEL(U_BOOL2)));
    put16(w, ST_OFF(swingFrqLimit), pick16(U_BOOL, NEL(U_BOOL)));
    put16(w, ST_OFF(setRunLostTime), pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)));
    put16(w, ST_OFF(setRunTimeAim), pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)));
    put16(w, ST_OFF(runDirPanelOld), pick16(U_BOOL, NEL(U_BOOL)));
    put16(w, ST_OFF(shuntFlag), pick16(U_BOOL, NEL(U_BOOL)));
    put16(w, ST_OFF(accDecTimeSrcPlc), pick16(U_BOOL2, NEL(U_BOOL2)));
    put16(w, ST_OFF(stopRemFlag), pick16(U_BOOL2, NEL(U_BOOL2)));
    put16(w, ST_OFF(bRunJog), pick16(U_BOOL, NEL(U_BOOL)));
    put16(w, ST_OFF(tuneCmd), pick16(U_BOOL, NEL(U_BOOL)));
    put16(w, ST_OFF(saveTuneDataFlag), pick16(U_BOOL, NEL(U_BOOL)));
    put16(w, ST_OFF(motorCtrlTuneStatus), pick16(U_TUNESTEP, NEL(U_TUNESTEP)));
    put32(w, ST_OFF(accFrqTime), pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)));
    put32(w, ST_OFF(decFrqTime), pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)));
    put32(w, ST_OFF(swingDoubleAw), pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)));
    put32(w, ST_OFF(swingAccTime), pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)));
    put32(w, ST_OFF(swingDecTime), pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)));
    put32(w, ST_OFF(runTimeTicker), pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)));
    put32(w, ST_OFF(lowerDelayTicker), pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)));
    put32(w, ST_OFF(shuntTicker), pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)));
    puti32(w, ST_OFF(swingMaxFrq), (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)) - 2500);
    puti32(w, ST_OFF(swingMinFrq), (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)) - 2500);
    puti32(w, ST_OFF(swingJumpFrq), (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)) - 2500);

    put16(w, SH_OFF(keyFunc), pick16(U_KEY, NEL(U_KEY)));
    put16(w, SH_OFF(commRunCmd), pick16(U_COMMRUNCMD, NEL(U_COMMRUNCMD)));
    put16(w, SH_OFF(bStopPlc), pick16(U_BOOL, NEL(U_BOOL)));
    put16(w, SH_OFF(errorOther), pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)));
    puti32(w, SH_OFF(frq), (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)) - 2500);
    puti32(w, SH_OFF(frqTmp), (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)) - 2500);
    puti32(w, SH_OFF(frqFrac), (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)) - 2500);
    puti32(w, SH_OFF(frqTmpFrac), (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)) - 2500);
    puti32(w, SH_OFF(frqCurAim), (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)) - 2500);
    puti32(w, SH_OFF(frqAimTmp0), (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)) - 2500);
    puti32(w, SH_OFF(frqAimOld4Dir), (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)) - 2500);
    puti32(w, SH_OFF(frqCurAimOld), (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)) - 2500);
    puti32(w, SH_OFF(accTimeOld), (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)) - 2500);
    puti32(w, SH_OFF(decTimeOld), (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)) - 2500);
    puti32(w, SH_OFF(upDownFrq), (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)) - 2500);
    put16(w, SH_OFF(plcStep), pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)));
    put32(w, SH_OFF(plcTime), pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)));
    put16(w, SH_OFF(plcStepRemOld), pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)));
    put32(w, SH_OFF(plcTimeRemOld), pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)));
    put16(w, SH_OFF(bFrqCurAimChg), pick16(U_BOOL, NEL(U_BOOL)));
    put16(w, SH_OFF(fcPlcCmd), pick16(U_COMMRUNCMD, NEL(U_COMMRUNCMD)));
    put16(w, SH_OFF(fcPowerUpTimeAddup), pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)));
    put16(w, SH_OFF(fcPowerUpTimeAddupSec), pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)));
    put16(w, SH_OFF(fcRunTimeAddup), pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)));
    put16(w, SH_OFF(fcRunTimeAddupSec), pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)));
    w->sh0.menu0DispStatus = (enum MENU0_DISP_STATUS)pick16(U_BOOL2, NEL(U_BOOL2));
    {
        Uint16 f = pick16(U_BOOL, NEL(U_BOOL));
        w->sh0.frqFlag.all = f ? 0x0040u : 0u;
    }
    w->sh0.curTime.runTimeM = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->sh0.curTime.runTimeSec = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->sh0.curTime.powerOnTimeM = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->sh0.curTime.powerOnTimeSec = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->sh0.curTime.runTime = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->sh0.curTime.powerOnTime = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->sh0.frqLine.maxValue = (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->sh0.frqLine.aimValue = (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->sh0.frqLine.curValue = (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->sh0.frqLine.tickerAll = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));
    w->sh0.frqLine.remainder = (int32)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE));

    /* timeBench must stay in {10,100,1000} (gap (b)). */
    w->sh0.timeBench = pick16(U_ACCUNIT, NEL(U_ACCUNIT));
    w->sh0.timeBench = (w->sh0.timeBench == FUNCCODE_accDecTimeUnit_0POINT) ? 1000
                     : (w->sh0.timeBench == FUNCCODE_accDecTimeUnit_1POINT) ? 100 : 10;
}

static void phase_random(long n)
{
    static const Uint16 entries[] = {
        E_UPDATE_RUN_CMD, E_SET_TIME_RUN, E_RUN_TIME_CAL, E_RUN_SRC_DEAL,
        E_UPDATE_BENCH_TIME, E_RUN_SRC_UPD_FRQ, E_START_RUN_CTRL,
        E_NORMAL_RUN_CTRL, E_STOP_RUN_CTRL, E_JOG_RUN_CTRL, E_TUNE_RUN_CTRL,
        E_LOSE_LOAD_CTRL, E_SHUT_DOWN_CTRL, E_UPDATE_SWING_PARA, E_SWING_DEAL,
        E_LOWER_FRQ, E_POWER_OFF_NO_STOP, E_ACC_DEC_TIME_CALC, E_DORMANT_DEAL
    };
    long i;
    struct IN w;

    for (i = 0; i < n; i++)
    {
        randomize(&w);
        if (rnd() & 1)
        {
            run_case(&w, E_RUN_SRC_DEAL);
        }
        else
        {
            run_case(&w, (int)pick16(entries, NEL(entries)));
        }
    }
}

/* The stateful sequence: carry each call's outputs into the next call's inputs.
 *
 * This is what exercises the function-local statics. Both sides see the same
 * inputs and therefore evolve the same statics, so side 0's outputs are a
 * well-defined starting point for the next tick. Without this phase the
 * tickers, the START_RUN_STATUS_* chain and the PLC step machine would never
 * advance past their first transition. */
static void phase_sequence(long n)
{
    static const Uint16 entries[] = {
        E_RUN_SRC_DEAL, E_RUN_SRC_DEAL, E_RUN_SRC_DEAL, E_RUN_SRC_DEAL,
        E_UPDATE_RUN_CMD, E_RUN_TIME_CAL, E_ACC_DEC_TIME_CALC,
        E_SET_TIME_RUN, E_UPDATE_SWING_PARA, E_SWING_DEAL
    };
    long i;
    struct IN w;
    struct ST st;
    struct SH sh;

    randomize(&w);
    w.fcRunSrc = FUNCCODE_runSrc_PANEL;
    w.sh0.timeBench = 1000;

    for (i = 0; i < n; i++)
    {
        int e = (int)pick16(entries, NEL(entries));

        /* Perturb a few inputs each tick, the way a real drive's inputs move. */
        if ((rnd() % 7) == 0) { w.fcRunSrc = pick16(U_RUNSRC, NEL(U_RUNSRC)); }
        if ((rnd() % 5) == 0) { w.diF1 = pick32(U_DI1, NEL(U_DI1)); }
        if ((rnd() % 5) == 0) { w.diF2 = pick32(U_DI2, NEL(U_DI2)); }
        if ((rnd() % 11) == 0) { w.errorLevel = pick16(U_ERRLEV, NEL(U_ERRLEV)); }
        if ((rnd() % 11) == 0) { w.errorCode = pick16(U_ERRCODE, NEL(U_ERRCODE)); }
        if ((rnd() % 13) == 0) { w.powerOnStatus = pick16(U_POWERON, NEL(U_POWERON)); }
        if ((rnd() % 17) == 0) { put16(&w, SH_OFF(keyFunc), pick16(U_KEY, NEL(U_KEY))); }
        if ((rnd() % 17) == 0) { put16(&w, SH_OFF(commRunCmd), pick16(U_COMMRUNCMD, NEL(U_COMMRUNCMD))); }
        if ((rnd() % 19) == 0) { put16(&w, SH_OFF(bStopPlc), pick16(U_BOOL, NEL(U_BOOL))); }
        if ((rnd() % 23) == 0) { w.generatrixVoltage = pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)); }
        if ((rnd() % 23) == 0) { w.ai1Voltage = (int16)pick16(U_FRQ_WIDE, NEL(U_FRQ_WIDE)); }

        run_case_out(&w, e, &st, &sh);
        w.st  = st;
        w.sh0 = sh;

        /* Keep the two hard constraints alive across the carried state. */
        if (w.sh0.timeBench == 0) { w.sh0.timeBench = 1000; }
        if (!swing_safe(&w))
        {
            w.swingBaseMode  = FUNCCODE_swingBaseMode_AGAIN_MAXFRQ;
            w.swingAmplitude = 1000;
            w.swingJumpRange = 100;
            w.maxFrq         = 5000;
        }
    }
}

/* ===========================================================================
 * main
 * ======================================================================== */

int main(int argc, char **argv)
{
    long nrand = (argc > 1) ? atol(argv[1]) : 20000;
    long base_cases;

    /* Unbuffered: this driver is long-running, and a crash part-way through
     * must not swallow the progress it has already made (redirected stdout is
     * block-buffered, so a crash loses everything not yet flushed). */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    g_trace_on = (getenv("RUNTRACE") != NULL);

    init_base();

    /* Widest sweeps first: they see UpdateRunCmd's statics coldest, which is
     * where a mutation to the "previous input" comparisons is most visible. */
    g_phase = "update_run_cmd";
    base_cases = g_cases;
    phase_update_run_cmd();
    printf("  UpdateRunCmd                 : %ld\n", g_cases - base_cases);

    g_phase = "run_src_update_frq";
    base_cases = g_cases;
    phase_run_src_update_frq();
    printf("  RunSrcUpdateFrq              : %ld\n", g_cases - base_cases);

    g_phase = "deal";
    base_cases = g_cases;
    phase_deal();
    printf("  RunSrcDeal matrix            : %ld\n", g_cases - base_cases);

    g_phase = "start_run_ctrl";
    base_cases = g_cases;
    phase_start_run_ctrl();
    printf("  StartRunCtrl                 : %ld\n", g_cases - base_cases);

    g_phase = "stop_run_ctrl";
    base_cases = g_cases;
    phase_stop_run_ctrl();
    printf("  StopRunCtrl                  : %ld\n", g_cases - base_cases);

    g_phase = "normal_run_ctrl";
    base_cases = g_cases;
    phase_normal_run_ctrl();
    printf("  NormalRunCtrl                : %ld\n", g_cases - base_cases);

    g_phase = "jog_run_ctrl";
    base_cases = g_cases;
    phase_jog_run_ctrl();
    printf("  JogRunCtrl                   : %ld\n", g_cases - base_cases);

    g_phase = "tune_run_ctrl";
    base_cases = g_cases;
    phase_tune_run_ctrl();
    printf("  TuneRunCtrl                  : %ld\n", g_cases - base_cases);

    g_phase = "lose_load_ctrl";
    base_cases = g_cases;
    phase_lose_load_ctrl();
    printf("  LoseLoadRunCtrl              : %ld\n", g_cases - base_cases);

    g_phase = "shut_down_ctrl";
    base_cases = g_cases;
    phase_shut_down_ctrl();
    printf("  ShutDownRunCtrl              : %ld\n", g_cases - base_cases);

    g_phase = "swing";
    base_cases = g_cases;
    phase_update_swing_para();
    printf("  UpdateSwingPara              : %ld\n", g_cases - base_cases);

    g_phase = "swing_deal";
    base_cases = g_cases;
    phase_swing_deal();
    printf("  SwingDeal                    : %ld\n", g_cases - base_cases);

    g_phase = "lower_frq";
    base_cases = g_cases;
    phase_lower_frq();
    printf("  LowerThanLowerFrqDeal        : %ld\n", g_cases - base_cases);

    g_phase = "power_off_no_stop";
    base_cases = g_cases;
    phase_power_off_no_stop();
    printf("  PowerOffNoStopDeal           : %ld\n", g_cases - base_cases);

    g_phase = "acc_dec_time_calc";
    base_cases = g_cases;
    phase_acc_dec_time_calc();
    printf("  AccDecTimeCalc               : %ld\n", g_cases - base_cases);

    g_phase = "dormant_deal";
    base_cases = g_cases;
    phase_dormant_deal();
    printf("  DormantDeal                  : %ld\n", g_cases - base_cases);

    g_phase = "set_time_run";
    base_cases = g_cases;
    phase_set_time_run();
    printf("  setTimeRun                   : %ld\n", g_cases - base_cases);

    g_phase = "run_time_cal";
    base_cases = g_cases;
    phase_run_time_cal();
    printf("  runTimeCal                   : %ld\n", g_cases - base_cases);

    g_phase = "bench_time";
    base_cases = g_cases;
    phase_update_bench_time();
    printf("  UpdateBenchTime              : %ld\n", g_cases - base_cases);

    g_phase = "random";
    base_cases = g_cases;
    phase_random(nrand);
    printf("  random                       : %ld\n", g_cases - base_cases);

    g_phase = "sequence";
    base_cases = g_cases;
    phase_sequence(nrand);
    printf("  stateful sequence            : %ld\n", g_cases - base_cases);

    printf("f_runSrc difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    return g_fails ? 1 : 0;
}
