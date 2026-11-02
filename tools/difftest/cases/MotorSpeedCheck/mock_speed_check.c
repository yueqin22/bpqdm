/* Shared mock for the MotorSpeedCheck differential test.
 *
 * One instance of everything the module touches but does NOT define. Both
 * translation units see the same memory, so a rewrite that stops writing a
 * field, or writes a different value, changes what is compared.
 *
 * Four groups:
 *
 *   1. peripheral registers   GpioDataRegs only. DisableDrive() is a MACRO --
 *          DisableDrive()   GpioDataRegs.GPASET.bit.GPIO7 = 1
 *      -- so there is no call to observe, only the register write; the struct
 *      has to be the real GPIO_DATA_REGS for the bitfield to be the chip's.
 *
 *   2. CPU control sequence   the shim turns EINT/DINT/ERTM/DRTM/EALLOW/EDIS/
 *      ESTOP0 into increments of gHostCpuCtl[]. This module does not issue any
 *      of them, so the counters are here only to prove that (they stay 0 on
 *      both sides).
 *
 *   3. call log               the three real callees. Each is stubbed so the
 *      harness stays self-contained, and each stub appends what it was handed
 *      to a shared log -- so "was it called, and with which argument" is
 *      observable rather than invisible. This matters: `CalOutVotInVFStatus`
 *      is called three times in this module with three *different* arguments
 *      (gFeisu.SpeedCheck, gMainCmd.FreqSyn, gFeisu.SpeedCheck again), and a
 *      rewrite that passed the wrong one would otherwise be undetectable.
 *
 *   4. globals owned by other modules.
 */

#include "MotorSpeedCheceInclude.h"

/* CalTorqueUp is declared only inside MotorVF.c in the project, so the
 * prototype has to be repeated here. */
void CalTorqueUp(void);

/* ---- 1. peripheral registers -------------------------------------------- */

volatile struct GPIO_DATA_REGS GpioDataRegs;

/* ---- 2. CPU control sequence -------------------------------------------- */

volatile Uint16 gHostCpuCtl[7];

/* ---- 3. call log --------------------------------------------------------- */

#define VF_LOG_LEN 32

int    gCalOutVfLog[VF_LOG_LEN];
Uint16 gCalOutVfCount;

Uint16 gCalTorqueUpCount;
Uint16 gPidCount;
PID_STRUCT gPidLog[VF_LOG_LEN];

/* The module reads exactly one thing out of the PID result:
 *
 *     gFeisu.VoltCheck = (gSpeedCheckPID.Out >> 16);
 *
 * (baseline line 149). VoltCheck then drives two comparisons -- the AlmostOver
 * test at line 167 and the m_UData test at line 179 -- so it is a *boundary
 * input* to the module under test, not an internal detail. Hashing the inputs
 * (pid_mix) makes Out deterministic but NOT settable, and a boundary can only
 * be tested by landing on it exactly.
 *
 * So the driver may force Out for one step: gPidOutForce = value << 16 pins
 * gFeisu.VoltCheck to `value`. -1 means "no override, use the hash".
 *
 * This does not weaken anything. Out is produced by a dependency the module
 * does not own; both sides still receive the same value, the PID *arguments*
 * are still compared through gPidLog, and gSpeedCheckPID (Out included) is
 * still compared field by field. Overriding only turns a hashed constant into
 * a controllable input. */
long gPidOutForce = -1;

/* Deterministic, and a genuine function of the argument, so "which value was
 * passed" flows through into gFeisu.VoltTemp / VoltCheckAim and is compared.
 * It is NOT the real VF voltage curve -- see the driver header. */
int CalOutVotInVFStatus(int freq)
{
    if (gCalOutVfCount < VF_LOG_LEN)
    {
        gCalOutVfLog[gCalOutVfCount] = freq;
    }
    gCalOutVfCount++;
    return (int)((((long)freq * 3L) + 7L) & 0xFFFFL);
}

void CalTorqueUp(void)
{
    gCalTorqueUpCount++;
    gOutVolt.detVfVoltUp = (int16)(gCalTorqueUpCount * 11);
}

/* Deterministic mix of the inputs, so the value handed back through
 * `gFeisu.VoltCheck = (gSpeedCheckPID.Out>>16)` is compared too. */
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
    if (gPidCount < VF_LOG_LEN)
    {
        gPidLog[gPidCount] = *pid;
    }
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

/* ---- 4. globals owned by other modules ----------------------------------- */

UDC_STRUCT                         gUDC;
COM_PAR_INFO_STRUCT                gComPar;
BASE_PAR_STRUCT                    gBasePar;
MT_STRUCT_Q24                      gIMTSetApply;
BASE_COMMAND_STRUCT                gMainCmd;
CONTROL_MOTOR_TYPE_ENUM            gCtrMotorType;
struct FAULT_CODE_INFOR_STRUCT_DEF gError;
RUN_STATUS_STRUCT                  gMainStatus;
INV_STRUCT                         gInvInfo;
MOTOR_EXTERN_STRUCT                gMotorExtPer;
OUT_VOLT_STRUCT                    gOutVolt;
LINE_CURRENT_STRUCT                gLineCur;
MOTOR_STRUCT                       gMotorInfo;

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
