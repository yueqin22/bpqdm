/* Shared mock for the MotorInvProtect differential test.
 *
 * Everything the module touches but does NOT define lives here. Both translation
 * units see the same memory, so a rewrite that stops writing a field, writes a
 * different value, or skips a call changes what gets compared.
 *
 * Five groups:
 *
 *   1. peripheral registers. Three of them are real structures from
 *      include/device (forwarded by real.txt), so the bitfield layouts are the
 *      chip's:
 *          AdcResult    read by PL_INPUT_HIGH -> AdcResult.ADCRESULT13
 *          GpioDataRegs written by the DisableDrive()/DisConnectRelay() MACROS
 *                       and read by GetOverUdcFlag() -- all three are macros in
 *                       SystemDefine.h, so there is no call to observe, only the
 *                       register traffic
 *          EPwm1/2/3Regs  SoftWareErrorDeal clears TZCLR.bit.OST/INT, and
 *                       SetCBCEnable/SetCBCDisable write TZSEL.bit.CBC2
 *
 *   2. CPU control sequence. The shim turns EINT/DINT/ERTM/DRTM/EALLOW/EDIS/
 *      ESTOP0 into increments of gHostCpuCtl[]. This module issues DINT/EINT
 *      around the over-current shutdown and EALLOW/EDIS around the trip-zone
 *      register writes, so losing one is a real defect that must not be
 *      invisible.
 *
 *   3. call log. CBCLimitCurPrepare() is the one real callee (it lives in
 *      MotorPublicCal.c). It is stubbed so the harness stays self-contained and
 *      the call is *counted*. Note what the stub deliberately does NOT do.
 *
 *      The real function reads EPwm1/2/3Regs.TZFLG.bit.CBC and sets
 *      gCBCProtect.Flag.bit.CBC_U/V/W accordingly. It cannot be reproduced
 *      here: gCBCProtect is *module-owned*, so each side owns its own copy and
 *      a single shared stub can only ever write one of them. Modelling it would
 *      make the two sides observe different memory, i.e. it would manufacture a
 *      mismatch. Instead the CBC flags are promoted to *inputs* -- the driver
 *      pokes gCBCProtect.Flag directly (see the driver header) -- which is
 *      strictly more coverage than driving them through TZFLG could be: every
 *      combination of the three flag bits becomes reachable.
 *
 *      The consequence has to be stated: the TZFLG->Flag transfer inside
 *      MotorPublicCal is NOT covered by this case. It is that module's job, not
 *      this one's.
 *
 *   4. globals owned by other modules. These are the module's *inputs* as well
 *      as its outputs, so the driver must restore every one of them before each
 *      side runs -- otherwise the ref side's writes leak into the new side.
 *
 *   5. the C28x runtime `abs`. See the long note at the bottom.
 *
 * The module itself DEFINES gOverLoad / gPhaseLose / gInLose / gLoadLose /
 * gFanCtrl / gBuffResCnt / gCBCProtect / gError, so those are deliberately
 * absent here: they are renamed per side by ref_tu.c / new_tu.c and the driver
 * compares the two instances field by field.
 */

#include "MotorInvProtectInclude.h"

/* ---- 1. peripheral registers -------------------------------------------- */

volatile struct ADC_RESULT_REGS AdcResult;
volatile struct GPIO_DATA_REGS  GpioDataRegs;
volatile struct EPWM_REGS       EPwm1Regs;
volatile struct EPWM_REGS       EPwm2Regs;
volatile struct EPWM_REGS       EPwm3Regs;

/* ---- 2. CPU control sequence -------------------------------------------- */

volatile Uint16 gHostCpuCtl[7];

/* ---- 3. call log --------------------------------------------------------- */

Uint16 gCbcPrepCount;

void CBCLimitCurPrepare(void)
{
    gCbcPrepCount++;
}

/* ---- 4. globals owned by other modules ----------------------------------- */

UDC_STRUCT               gUDC;
COM_PAR_INFO_STRUCT      gComPar;
BASE_COMMAND_STRUCT      gMainCmd;
RUN_STATUS_STRUCT        gMainStatus;
INV_STRUCT               gInvInfo;
LINE_CURRENT_STRUCT      gLineCur;
MOTOR_STRUCT             gMotorInfo;
TEMPLETURE_STRUCT        gTemperature;
PWM_OUT_STRUCT           gPWM;
ANGLE_STRUCT             gPhase;
UVW_STRUCT               gIUVWQ12;
UVW_STRUCT_Q24           gIUVWQ24;
SUB_COMMAND_UNION        gSubCommand;

/* ---- 5. C28x runtime-library shim: abs ----------------------------------- */

/* `abs` must be the C28x runtime-library one, and its WIDTH is what makes the
 * difference.
 *
 * The project declares, in SubPrgInclude.h:
 *     extern int abs(int);
 * On C28x `int` is 16 bits, so that is a 16-bit absolute value with 16-bit
 * wrap: abs(-32768) is -32768. The host CRT's `int abs(int)` is 32 bits, and a
 * 16-bit call site would zero-extend its argument (`-1` -> 0x0000FFFF), so the
 * CRT's version sees a positive number, returns it unchanged, and truncating
 * back to 16 bits restores the original negative value -- abs(x) == x for every
 * negative x. Both sides of the differential test would be wrong in the same
 * way and still report zero mismatches; the damage is lost coverage, not a
 * false positive. That is not a hypothesis: it was measured with the project's
 * own declaration (build/_me_absprobe.py) -- abs(-1) = -1, abs(-5) = -5,
 * abs(-100) = -100.
 *
 * Two things keep that out of this case:
 *   * the shim's tail `#define int short` makes the project's prototype and this
 *     definition both 16-bit, so the call site and the callee agree;
 *   * <stdlib.h> is NOT included anywhere in the case (driver, mock, TUs) --
 *     stdlib declares the 32-bit prototype and would collide with the folded
 *     one. SubPrg's case documents the same trap.
 *
 * Defining it here overrides the library version: an object-file definition wins
 * over the same symbol in a static library. The body is the C28x semantics with
 * 16-bit wrap -- `-(-32768)` is 32768, which does not fit in a 16-bit int, so it
 * wraps back to -32768. The host computes `-mValue` in 32 bits and truncates on
 * return, landing on the same value.
 *
 * This module calls abs() on the negative-capable quantities gIUVWQ24.U/V/W
 * (OutputLoseAdd), gIUVWQ12.U/V/W (CBCLimitCurProtect, LoadLoseDetect) and
 * m_CurU/V/W. A no-op abs would hide exactly those mutations that delete the
 * call or move it across a sign change, so the shim is required here, not
 * optional. */
int abs(int mValue)
{
    return (mValue < 0) ? -mValue : mValue;
}
