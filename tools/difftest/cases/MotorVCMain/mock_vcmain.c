/* Shared mock for the MotorVCMain differential test.
 *
 * src/motor/MotorVCMain.c owns sixteen data objects of its own (they show up as
 * `B`/`D` symbols in build/Debug/MotorVCMain.obj) and ref_tu.c / new_tu.c
 * rename those per side:
 *     gVCPar gIMTSet gIMTSetApply gUMTSet gUAmpTheta gImAcrQ24 gItAcrQ24
 *     gAsr gVcCsrPara gIMSetAcr gWspid gPWMVAlphBeta gUdcLimitIt gSvcVoltAdj
 *     gABVoltSet SVC0Signal
 * Everything else it touches is an extern owned by another translation unit,
 * and that is what this file supplies:
 *
 *   1 CPU-control counter table   gHostCpuCtl[]  (DINT / EINT)
 *  21 globals                     read and/or written by the module
 *   3 callee stubs                 PID / PID32 / ResetSVC
 *
 * The list is the module object file's undefined-symbol set read out with
 * `nm2000 -u build/Debug/MotorVCMain.obj` (29 entries) minus the two runtime
 * helpers (L$$DIV / LL$$MPY) minus the six asm/real-callees that
 * tools/difftest/hostlib_mock.c and tools/difftest/atan_mock.c supply
 * (qsin / qsqrt / atan) minus the 21 globals.  It is not hand-guessed.
 *
 * Types come from the real project headers -- this file includes
 * MotorInclude.h, which real.txt forwards verbatim.  No hand-written stand-in
 * structs: a field offset that disagreed with the firmware would make the whole
 * comparison compare the wrong words.
 *
 * ---------------------------------------------------------------------------
 * Why PID / PID32 hash their inputs instead of doing nothing
 * ---------------------------------------------------------------------------
 * Both are real functions in SubPrg.c and neither is under test here.  A stub
 * that left `Out` untouched would make `gAsr.TorqueLimit = ...Pid.Out>>16`
 * a constant on both sides, i.e. every mutation of that shift, that field or
 * that assignment would be unobservable.  A stub that invented a plausible
 * controller would be testing a controller nobody wrote.
 *
 * So: `Out` is a deterministic hash of every field the module just filled in,
 * which makes "which Deta/Max/Min/KP/KI got passed" flow through into the
 * compared result, and `gPidOutForce` / `gPid32OutForce` let the driver pin a
 * boundary value exactly (the module compares `Out>>16` against constants, and
 * a boundary can only be tested by landing on it).  The argument struct itself
 * is also logged, so the assignment sequence is compared directly and not only
 * through its effect.  Same scheme as cases/MotorSpeedCheck.
 *
 * ---------------------------------------------------------------------------
 * What this case does NOT cover
 * ---------------------------------------------------------------------------
 * The bodies of PID / PID32 / ResetSVC, the target-side effect of DINT/EINT,
 * and qsin/qsqrt/atan beyond "both sides call the same approximation".
 */

#include "MotorInclude.h"

/* ---- 1. CPU control sequence -------------------------------------------- */

/* VCCsrControl and VCCsrControl_380 bracket the two gIMTSetApply assignments
 * with DINT/EINT.  Counting them keeps "the rewrite dropped the critical
 * section" (or moved one assignment outside it) observable -- that class of
 * error leaves every data field identical.
 *   0 EINT  1 DINT  2 ERTM  3 DRTM  4 EALLOW  5 EDIS  6 ESTOP0
 * (same table as cases/MotorSVC, cases/MotorMain, cases/MotorPmsmMain). */

volatile Uint16 gHostCpuCtl[7];

/* ---- 2. the outside world's globals ------------------------------------- */

MOTOR_STRUCT                        gMotorInfo;
MOTOR_EXTERN_STRUCT                 gMotorExtPer;
RUN_STATUS_STRUCT                   gMainStatus;
BASE_COMMAND_STRUCT                 gMainCmd;
BASE_PAR_STRUCT                     gBasePar;
COM_PAR_INFO_STRUCT                 gComPar;
CONTROL_MOTOR_TYPE_ENUM             gCtrMotorType;
INV_STRUCT                          gInvInfo;
VF_INFO_STRUCT                      gVFPar;
UDC_STRUCT                          gUDC;
OVER_UDC_CTL_STRUCT                 gOvUdc;
ANGLE_STRUCT                        gPhase;
PM_DECOUPLE                         gPmDecoup;
PM_FLUX_WEAK                        gFluxWeak;
MOTOR_DEBUG_DATA_RECEIVE_STRUCT     gTestDataReceive;
OUT_VOLT_STRUCT                     gOutVolt;
MT_STRUCT                           gIMTQ12;
MT_STRUCT                           gIMTSetQ12;
MT_STRUCT_Q24                       gIMTQ24;
MT_STRUCT_Q24                       gIMTQ12_obs;
MT_STRUCT_Q24                       gIMTQ24_obs;

/* ---- 3. callees --------------------------------------------------------- */

/* Call ledger.  Two slots per callee: count and a hash of the argument, so
 * "PID was called with the fields the module just set" is compared even when
 * the hash's effect on the output happens to be identical on both sides. */

unsigned short gPidCount;
unsigned short gPid32Count;
unsigned short gResetSvcCount;

PID_STRUCT     gPidLog;         /* last argument seen by PID   */
PID32_STRUCT   gPid32Log;       /* last argument seen by PID32 */

/* -1 == "no override, use the hash".  Otherwise the value the driver pinned. */
long gPidOutForce   = -1;
long gPid32OutForce = -1;
/* Pins ONLY the second PID32 call.  VCCsrControl runs PID32 twice, once
 * for the M axis and once for T; a single force can only make the two
 * axes equal, so "Amp sits exactly on the limit" and "gUMTSet.T is
 * exactly 0" were both unreachable. */
long gPid32Force2   = -1;

static long pid_mix(const PID_STRUCT *p)
{
    long h = (long)p->Total;

    h = h * 31L + (long)p->Deta;
    h = h * 31L + (long)p->KP;
    h = h * 31L + (long)p->KI;
    h = h * 31L + (long)p->KD;
    h = h * 31L + (long)p->Max;
    h = h * 31L + (long)p->Min;
    h = h * 31L + (long)p->QP;
    h = h * 31L + (long)p->QI;
    h = h * 31L + (long)p->QD;
    return h;
}

static long pid32_mix(const PID32_STRUCT *p)
{
    long h = (long)p->Total;

    h = h * 31L + (long)p->Deta;
    h = h * 31L + (long)p->KP;
    h = h * 31L + (long)p->KI;
    h = h * 31L + (long)p->Max;
    h = h * 31L + (long)p->Min;
    return h;
}

void PID(PID_STRUCT *pid)
{
    gPidLog = *pid;
    gPidCount++;
    if (gPidOutForce >= 0)
    {
        pid->Out = gPidOutForce;
    }
    else
    {
        pid->Out = pid_mix(pid) | ((long)gPidCount << 24);
    }
}

void PID32(PID32_STRUCT *pid)
{
    gPid32Log = *pid;
    gPid32Count++;
    if ((2 == gPid32Count) && (gPid32Force2 >= 0))
    {
        pid->Out = gPid32Force2;
    }
    else if (gPid32OutForce >= 0)
    {
        pid->Out = gPid32OutForce;
    }
    else
    {
        pid->Out = pid32_mix(pid) | ((long)gPid32Count << 24);
    }
}

/* ResetParForVC calls ResetSVC when the control mode is IDC_SVC_CTL.  The real
 * one lives in MotorSVC.c and resets the SVC observer; that state is not part
 * of this module's comparison, so the counter is the whole point: "the rewrite
 * dropped the call" or "called it in the wrong mode" must not be invisible. */
void ResetSVC(void)
{
    gResetSvcCount++;
}
