/* Shared mock for the MotorCarrier differential test.
 *
 * Everything the module touches but does NOT define lives here. Both translation
 * units see the same memory, so a rewrite that stops writing a field, writes a
 * different value, or skips a call changes what gets compared.
 *
 * Four groups:
 *
 *   1. the one peripheral register block. GetTime() is a MACRO --
 *          #define GetTime()  (CpuTimer1.RegsAddr->TIM.all)
 *      -- so there is no call to observe, only the read of CpuTimer1Regs.TIM.
 *      CpuTimer1 is CPUTIMER_VARS (RegsAddr + interrupt counters); on the chip
 *      RegsAddr points at CpuTimer1Regs, and the mock wires it the same way.
 *      The struct is the real CPUTIMER_REGS, so the bitfield layout is the
 *      chip's, not a replica.
 *
 *   2. CPU control sequence. The shim turns EINT/DINT/ERTM/DRTM/EALLOW/EDIS/
 *      ESTOP0 into increments of gHostCpuCtl[]. AsynPWMAngleCal and
 *      SynPWMAngleCal both wrap their phase writes in DINT ... EINT (they are
 *      called from the carrier interrupt while the 0.5 ms task may be reading
 *      gPhase / gPWM.gPWMPrd). Losing that pair is a real defect that would be
 *      invisible if the macros were blanked, so it is counted instead.
 *
 *   3. call log. SynCalRealPos() is the only real callee. It is stubbed so the
 *      harness stays self-contained, and the stub records that it ran and with
 *      which side. CalOutputPhase calls it in exactly one branch (PGMode == 0
 *      with a PM motor); a rewrite that moved or dropped the call would
 *      otherwise be undetectable because the stub has no visible result.
 *
 *   4. globals owned by other modules. These are the module's *inputs* as well
 *      as its outputs, so the driver must restore every one of them before each
 *      side runs -- otherwise the ref side's writes leak into the new side.
 *
 * The module itself DEFINES gFcCal / gSynPWM / gPhase / gDeadBand, so those are
 * deliberately absent here: they are renamed per side by ref_tu.c / new_tu.c and
 * the driver compares the two instances field by field.
 */

#include "MotorInclude.h"

/* ---- 1. peripheral registers -------------------------------------------- */

volatile struct CPUTIMER_REGS CpuTimer1Regs;
struct CPUTIMER_VARS          CpuTimer1;

/* ---- 2. CPU control sequence -------------------------------------------- */

volatile Uint16 gHostCpuCtl[7];

/* ---- 3. call log --------------------------------------------------------- */

#define CARRIER_LOG_LEN 64

int    gSynCalPosLog[CARRIER_LOG_LEN];
Uint16 gSynCalPosCount;

void SynCalRealPos(void)
{
    if (gSynCalPosCount < CARRIER_LOG_LEN)
    {
        gSynCalPosLog[gSynCalPosCount] = 1;
    }
    gSynCalPosCount++;
}

/* ---- 4. globals owned by other modules ---------------------------------- */

BASE_PAR_STRUCT         gBasePar;
BASE_COMMAND_STRUCT     gMainCmd;
RUN_STATUS_STRUCT       gMainStatus;
MOTOR_STRUCT            gMotorInfo;
MOTOR_PARA_EST          gGetParVarable;
CONTROL_MOTOR_TYPE_ENUM gCtrMotorType;
MAIN_COMMAND_EXTEND_UNION gExtendCmd;
SUB_COMMAND_UNION       gSubCommand;
PWM_OUT_STRUCT          gPWM;
OUT_VOLT_STRUCT         gOutVolt;
AMPTHETA_STRUCT         gIAmpTheta;
TEMPLETURE_STRUCT       gTemperature;
OVER_LOAD_PROTECT       gOverLoad;
PG_DATA_STRUCT          gPGData;
ROTOR_TRANS_STRUCT      gRotorTrans;
IPM_POSITION_STRUCT     gIPMPos;
IPM_ZERO_POS_STRUCT     gIPMZero;
PMSM_EST_PARAM_DATA     gPmParEst;
INV_STRUCT              gInvInfo;

/* ---- C28x runtime-library shim: abs -------------------------------------- */

/* The host CRT's `abs` has the WRONG WIDTH, and the wrong width turns it into
 * a silent no-op.
 *
 * The project declares, in SubPrgInclude.h:
 *     extern int abs(int);
 * On C28x `int` is 16 bits, so that is a 16-bit absolute value. On the host the
 * shim's `#define int short` turns the declaration into `extern short
 * abs(short)`, but the definition that gets LINKED is still the CRT's
 * `int abs(int)`: same symbol name, 32-bit parameter. The caller zero-extends
 * the 16-bit argument (`-1` -> 0x0000FFFF), the 32-bit `abs` sees a positive
 * number and returns it unchanged, and truncating back to `short` restores the
 * original negative value. Measured with the project's own declaration
 * (build/_me_absprobe.py): abs(-1) = -1, abs(-5) = -5, abs(-100) = -100.
 *
 * i.e. abs(x) == x for every negative x. Both sides of the differential test
 * were wrong in the same way, so it reported zero mismatches -- the damage is
 * lost coverage, not a false positive. It was found on MotorEncoder, where it
 * made six negative-control mutations invisible (deleting `abs` from
 * `m_UData = abs(gFVCSpeed.MDetaPos)` produced identical state on both sides).
 *
 * This module's abs-bearing comparisons were re-checked after the fix: all of
 * them were already CAUGHT, because each one changes a constant or an operand
 * *around* the abs call rather than removing the call, so a no-op abs could not
 * hide it. The shim is added anyway -- the bench has to model the target, and a
 * no-op abs is the one defect class it would hide.
 *
 * Defining it here overrides the library version: an object-file definition
 * wins over the same symbol in a static library. The body is the C28x RTS
 * semantics with 16-bit wrap -- `-(-32768)` is 32768, which does not fit in a
 * 16-bit int, so it wraps back to -32768. The host computes `-mValue` in 32 bits
 * and truncates on return, landing on the same value. */
int abs(int mValue)
{
    return (mValue < 0) ? -mValue : mValue;
}
