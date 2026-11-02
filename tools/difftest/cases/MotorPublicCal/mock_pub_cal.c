/* Shared mock for the MotorPublicCal differential test.
 *
 * One instance of everything the module touches but does NOT define. Both
 * translation units see the same memory, so a rewrite that stops writing a
 * register, or writes a different value, changes what is compared.
 *
 * Four groups:
 *
 *   1. peripheral registers   EPwm1Regs / EPwm2Regs / EPwm3Regs / GpioDataRegs.
 *      The structs come from the real device headers (see real.txt), so field
 *      order and bitfield layout are the chip's, not an imitation. EPwm*Regs is
 *      where the module's whole job happens (TBPRD / CMPA / CMPB / AQCSFRC /
 *      DBCTL / TZCLR / TZFLG); GpioDataRegs carries the drive-enable, relay and
 *      brake pin writes, which in this project are MACROS, not functions:
 *          EnableDrive()      GpioDataRegs.GPACLEAR.bit.GPIO7  = 1
 *          DisableDrive()     GpioDataRegs.GPASET.bit.GPIO7    = 1
 *          ConnectRelay()     GpioDataRegs.GPACLEAR.bit.GPIO11 = 1
 *          DisConnectRelay()  GpioDataRegs.GPASET.bit.GPIO11   = 1
 *          TurnOnBrake()      GpioDataRegs.GPACLEAR.bit.GPIO8  = 1
 *          TurnOffBrake()     GpioDataRegs.GPASET.bit.GPIO8    = 1
 *      That is why GpioDataRegs has to be a real struct here: there is no call
 *      to observe, only the register write. And GetTime() is a macro too --
 *      `CpuTimer1.RegsAddr->TIM.all` -- so the timer has to be a real
 *      CPUTIMER_REGS instance with CpuTimer1.RegsAddr pointing at it.
 *
 *   2. CPU control sequence   the shim turns EINT/DINT/ERTM/DRTM/EALLOW/EDIS/
 *      ESTOP0 into increments of gHostCpuCtl[]. They are real C28x instructions
 *      with no memory operand, so without this "the rewrite dropped EDIS" would
 *      produce identical state on both sides. This module pairs EALLOW/EDIS
 *      around every EPwm register write.
 *
 *   3. call log               PID() is the one real callee (the other "calls"
 *      are macros). It is stubbed so the harness stays self-contained: the stub
 *      appends a snapshot of the PID_STRUCT it was handed to a shared log, so
 *      "was it called, and with which gains/target" is observable, and then
 *      writes a deterministic mix of the inputs into .Out so that the value
 *      flowing back into gOutVolt.Volt is observable too.
 *
 *   4. globals owned by other modules.
 */

#include "MotorPublicCalInclude.h"

/* ---- 1. peripheral registers -------------------------------------------- */

volatile struct EPWM_REGS      EPwm1Regs;
volatile struct EPWM_REGS      EPwm2Regs;
volatile struct EPWM_REGS      EPwm3Regs;
volatile struct GPIO_DATA_REGS GpioDataRegs;

/* GetTime() is `CpuTimer1.RegsAddr->TIM.all`, so this has to be a real
 * CPUTIMER_REGS instance and CpuTimer1.RegsAddr has to point at it. The driver
 * sets the pointer and seeds TIM before every call. */
volatile struct CPUTIMER_REGS  gHostCpuTimer1Regs;
struct CPUTIMER_VARS           CpuTimer1;

/* ---- 2. CPU control sequence -------------------------------------------- */

volatile Uint16 gHostCpuCtl[7];

/* ---- 3. call log --------------------------------------------------------- */

#define PID_LOG_LEN 32

PID_STRUCT gPidLog[PID_LOG_LEN];
Uint16     gPidCount;
Uint16     gPidOutWord;

/* A deterministic function of the inputs, so the value handed back through
 * `gOutVolt.Volt = gDCBrake.PID.Out>>16` is compared too. It is NOT the real
 * PID -- see the driver header. */
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

void PID(PID_STRUCT *pid)
{
    if (gPidCount < PID_LOG_LEN)
    {
        gPidLog[gPidCount] = *pid;
    }
    gPidCount++;
    gPidOutWord = (Uint16)gPidCount;

    pid->Out = pid_mix(pid) | ((long)gPidCount << 24);
}

/* ---- 4. globals owned by other modules ----------------------------------- */

RUN_STATUS_STRUCT                  gMainStatus;
UDC_STRUCT                         gUDC;
ADC_STRUCT                         gADC;
struct FAULT_CODE_INFOR_STRUCT_DEF gError;
INV_STRUCT                         gInvInfo;
CUR_EXCURSION_STRUCT               gExcursionInfo;
Ulong                              gBuffResCnt;
ANGLE_STRUCT                       gPhase;
CBC_PROTECT_STRUCT                 gCBCProtect;
UVW_STRUCT_Q24                     gIUVWQ24;
LINE_CURRENT_STRUCT                gLineCur;
BASE_COMMAND_STRUCT                gMainCmd;
COM_PAR_INFO_STRUCT                gComPar;
OUT_VOLT_STRUCT                    gOutVolt;
MOTOR_EXTERN_STRUCT                gMotorExtPer;
VF_INFO_STRUCT                     gVFPar;

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
