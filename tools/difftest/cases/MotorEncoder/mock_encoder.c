/* Shared mock for the MotorEncoder differential test.
 *
 * One instance of everything the module touches but does NOT define. Both
 * translation units see the same memory, so a rewrite that stops writing a
 * register, or writes a different value, changes what is compared.
 *
 * Five groups:
 *
 *   1. peripheral registers   AdcResult / GpioCtrlRegs / GpioDataRegs /
 *      SysCtrlRegs / the CPU timer. The structs come from the real device
 *      headers (see real.txt), so field order and bitfield layout are the
 *      chip's, not an imitation. This module reaches the hardware through
 *      MACROS, not calls:
 *          UVW_PG_U/V/W      AdcResult.ADCRESULT9/10/11 << 4
 *          ROTOR_TRANS_SO    GpioDataRegs.GPADAT.bit.GPIO21
 *                            (host build: redirected to gHostSoBits[], see 3b)
 *          ROTOR_TRANS_RD    GpioDataRegs.GPADAT.bit.GPIO23
 *          ROTOR_TRANS_SCLK  GpioDataRegs.AIODAT.bit.AIO12
 *          GetTime()         CpuTimer1.RegsAddr->TIM.all
 *      There is no call to observe, so these have to be real objects and are
 *      compared whole. `CpuTimer1.RegsAddr` is pointed at gHostCpuTimer1Regs
 *      by the driver before every call, so the driver can seed the countdown.
 *
 *   2. two QEP blocks         `EQepRegs` is a pointer DEFINED by the module
 *      (`struct EQEP_REGS *EQepRegs;`), so it is not in the mock's symbol
 *      inventory. But the block it points at has to exist, and it must exist
 *      TWICE: one per side. A single shared block would let the ref side's
 *      QCLR/QEPSTS writes leak into the new side's input image and mask a
 *      rewrite that stopped clearing a latch. The driver assigns
 *      ref_EQepRegs / new_EQepRegs at these two blocks.
 *
 *   3. CPU control sequence   the shim turns EINT/DINT/EALLOW/EDIS into
 *      increments of gHostCpuCtl[]. They are real C28x instructions with no
 *      memory operand, so without this "the rewrite dropped EDIS" would
 *      produce identical state on both sides. This module pairs EALLOW/EDIS
 *      around every QEP register write and DINT/EINT around the speed
 *      critical sections.
 *
 *   4. call log               BurrFilter() is the one real callee. It is
 *      stubbed so the harness stays self-contained -- src/subprogram/SubPrg.c
 *      is a different module with its own rewrite status. The stub is a
 *      verbatim copy of the frozen baseline algorithm, NOT a synthetic mix,
 *      because RotorTransCalVel hands it `&gSpeedFilter` and the whole point of
 *      comparing gSpeedFilter afterwards is that the real filter ran. A call
 *      counter makes "was it called at all" observable independently of what
 *      it computed.
 *
 *      Width note: `gSpeedFilter` is declared `BURR_FILTER_STRUCT` (five
 *      `long`s), so the `(BURR_FILTER_STRUCT *)` cast at the call site is a
 *      no-op. On C28x COFF a `long` is 2 words, so the struct is 10 words and
 *      fits in the same space the delivered code reserves for it. The shim
 *      pins the host to the same widths.
 *
 *  3b. host-only serial input   the ROTOR_TRANS_SO backing store. See there.
 *
 *  3c. C28x runtime-library shims   `abs`, whose host CRT version is 32 bits
 *      and therefore a silent no-op for negative 16-bit arguments. See there.
 *
 *   5. globals owned by other modules.
 *
 * The undefined-symbol list this was built from is `nm2000` output on
 * build/Debug/MotorEncoder.obj, recorded in mocks.txt. The C28x runtime
 * helpers in that list (L$$DIV / LL$$MPY / ULL$$CMP / ULL$$DIV) are emitted by
 * the target compiler for the 32- and 64-bit multiplies and divides; the host
 * compiler emits calls into its own CRT instead, so they need no stub here.
 */

#include "MotorEncoder.h"
#include "MotorPmsmMain.h"
#include "MotorPmsmParEst.h"
#include "MotorVCInclude.h"
#include "SubPrgInclude.h"

/* ---- 1. peripheral registers -------------------------------------------- */

volatile struct ADC_RESULT_REGS AdcResult;
volatile struct GPIO_CTRL_REGS  GpioCtrlRegs;
volatile struct GPIO_DATA_REGS  GpioDataRegs;
volatile struct SYS_CTRL_REGS   SysCtrlRegs;

/* GetTime() is `CpuTimer1.RegsAddr->TIM.all`, so this has to be a real
 * CPUTIMER_REGS instance and CpuTimer1.RegsAddr has to point at it. The driver
 * sets the pointer and seeds TIM before every call.
 *
 * TIM is a DOWN counter on the chip: the project always writes the difference
 * as "the value read earlier minus the value read later" (MotorEncoder.c:692,
 * f_main.c:314/592). The driver seeds it accordingly -- see the header of
 * driver.c. Getting that direction wrong makes every "every N ms" gate
 * permanently true and quietly turns real mutations into MISSED. */
volatile struct CPUTIMER_REGS gHostCpuTimer1Regs;
struct CPUTIMER_VARS          CpuTimer1;

/* ---- 2. two QEP blocks --------------------------------------------------- */

struct EQEP_REGS gHostEqepRef;
struct EQEP_REGS gHostEqepNew;

/* ---- 3. CPU control sequence -------------------------------------------- */

volatile Uint16 gHostCpuCtl[7];

/* ---- 3b. host-only serial input ----------------------------------------- */

/* Backing store for the ROTOR_TRANS_SO rewrite in host_patch.json.
 *
 * On the target the pin is a fixed memory bit
 * (SystemDefine.h: #define ROTOR_TRANS_SO GpioDataRegs.GPADAT.bit.GPIO21),
 * so from the harness it can only be set once per call -- and
 * GetRotorTransPos reads it twelve times inside one call. With a constant
 * pin all twelve reads are identical, mData can only be 0x0000 or 0xFFF0,
 * and the twelve shift amounts are mutually indistinguishable: `<< 13`
 * rewritten as `<< 12` produces the same result. The pin does move on real
 * hardware (the resolver chip shifts out one bit per clock), so that state
 * is reachable and the rewrite must be tested on it.
 *
 * The patch therefore turns the constant into an input: the macro expands
 * to gHostSoBits[gHostSoIdx++], and the driver resets gHostSoIdx and loads a
 * 12-bit pattern before each side's call. Entries 12..4095 stay 0, so a
 * rewrite that reads the pin more than twelve times still cannot run off the
 * end; it just sees zeros where the pattern ran out, which is itself a
 * difference the comparison will catch. Only 0/1 values are ever loaded,
 * because that is the whole range of a one-bit field on the chip -- feeding
 * anything else would test behaviour the target cannot exhibit.
 *
 * The array is shared, not per-side, and is deliberately NOT snapshotted:
 * it is an input, and both sides are handed the same sequence. What is
 * compared is the module's reaction to it (RtorBuffer, RTPos, AbsRotPos,
 * SimuZBack, ...), not the sequence itself.
 *
 * HOST_SO_LEN comes from the injection itself (MotorEncoder.h, via
 * host_patch.json), so the extern declaration there and the definition here
 * cannot drift apart -- C does not cross-check array sizes between
 * translation units, so two literals would be a silent aliasing hazard. */

Uint32 gHostSoBits[HOST_SO_LEN];
Uint16 gHostSoIdx;

/* ---- 3c. C28x runtime-library shims -------------------------------------- */

/* `abs` -- the host CRT's version has the WRONG WIDTH, and the wrong width
 * makes it a silent no-op.
 *
 * The project declares, in SubPrgInclude.h:
 *     extern int abs(int);
 * On C28x `int` is 16 bits, so that is a 16-bit absolute value. On the host
 * the shim's `#define int short` turns the declaration into
 * `extern short abs(short)`, but the definition that gets LINKED is still the
 * CRT's `int abs(int)`: same symbol name, 32-bit parameter. So the caller
 * zero-extends the 16-bit argument (`-1` -> 0x0000FFFF), the 32-bit `abs`
 * sees a positive number and returns it unchanged, and truncating back to
 * `short` restores the original negative value. Measured with the project's
 * own declaration (build/_me_absprobe.py):
 *
 *     abs(-1) = -1     abs(-5) = -5     abs(-100) = -100
 *
 * i.e. abs(x) == x for every negative x -- the call does nothing. That is not
 * a cosmetic difference. It made six negative-control mutations invisible:
 * deleting `abs` from `m_UData = abs(gFVCSpeed.MDetaPos)` produced identical
 * state on both sides, and the `< 8` collapse tests in GetSpeedMMethod /
 * GetSpeedMTMethod were comparing a signed difference instead of an absolute
 * one. Both sides were wrong in the same way, so the differential test still
 * reported zero mismatches -- the failure mode is lost coverage, not a false
 * positive, which is exactly why it went unnoticed until a mutation refused
 * to be caught.
 *
 * Defining it here overrides the library version: an object-file definition
 * wins over the same symbol in a static library. The body is the C28x RTS
 * semantics with 16-bit wrap -- `-(-32768)` is 32768, which does not fit in a
 * 16-bit int, so it wraps back to -32768. The host computes `-mValue` in 32
 * bits and truncates on return, landing on the same value.
 *
 * `labs` needs no shim: the shim header declares `extern long labs(long)`, and
 * the host's `long` is 32 bits, which is what C28x uses too (measured, see
 * DSP2803x_Device.h). */
int abs(int mValue)
{
    return (mValue < 0) ? -mValue : mValue;
}

/* ---- 4. call log --------------------------------------------------------- */

Uint16 gBurrCount;
BURR_FILTER_STRUCT gBurrLog;

/* Verbatim from the frozen baseline (src/subprogram/SubPrg.c, BurrFilter).
 * Both translation units call this same object, so the comparison is about
 * what the module did to gSpeedFilter, not about two copies of the filter. */
void BurrFilter(BURR_FILTER_STRUCT * filter)
{
    int m_Deta;

    m_Deta = abs((filter->Input) - (filter->Output));
    if (m_Deta > filter->Err)
    {
        filter->Err = filter->Err << 1;
    }
    else
    {
        filter->Output = filter->Input;
        if (m_Deta < (filter->Err >> 1))
        {
            filter->Err = filter->Err >> 1;
        }
    }
    filter->Err = (filter->Err > filter->Max) ? filter->Max : filter->Err;
    filter->Err = (filter->Err < filter->Min) ? filter->Min : filter->Err;

    gBurrLog = *filter;
    gBurrCount++;
}

/* ---- 5. globals owned by other modules ----------------------------------- */

MOTOR_STRUCT                       gMotorInfo;
MOTOR_EXTERN_STRUCT                gMotorExtInfo;
RUN_STATUS_STRUCT                  gMainStatus;
BASE_COMMAND_STRUCT                gMainCmd;
MAIN_COMMAND_EXTEND_UNION          gExtendCmd;
BASE_PAR_STRUCT                    gBasePar;
CPU_TIME_STRUCT                    gCpuTime;
struct FAULT_CODE_INFOR_STRUCT_DEF gError;
IPM_POSITION_STRUCT                gIPMPos;
IPM_ZERO_POS_STRUCT                gIPMZero;
PWM_OUT_STRUCT                     gPWM;
PMSM_EST_PARAM_DATA                gPmParEst;
VC_INFO_STRUCT                     gVCPar;
