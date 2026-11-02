/* Shared mock for the MotorSVC differential test.
 *
 * src/motor/MotorSVC.c defines 8 data symbols of its own (gFluxR, gFluxS,
 * gCalFlux, gSVCSpeed, gABFluxS_LowFilter, gABFluxS_comp, gIMTQ12_obs,
 * gIMTQ24_obs) -- those are renamed per side by ref_tu.c / new_tu.c, exactly
 * like the four entry points, so they are NOT in this file.  Everything the
 * module references but does not define lives here, one instance each, shared
 * by both translation units:
 *
 *   1. the CPU timer.  GetTime() is the macro `CpuTimer1.RegsAddr->TIM.all`
 *      (MotorInclude.h), so the timer has to be real CPUTIMER_REGS memory and
 *      CpuTimer1.RegsAddr has to point at it.  SVCCalFlux_380 reads it to form
 *      `(gSVCSpeed.Timer - m_Timer) & 0xFFFFFF`, i.e. the timer IS module
 *      input, and the driver sets TIM.all before every call.
 *   2. the CPU-control counter table the case shim turns EINT/DINT into.
 *      SVCCalRotorSpeed brackets the DetaPhase snapshot with DINT/EINT, so the
 *      pair's occurrence is observable state.
 *   3. the 14 globals owned by other translation units in the firmware
 *      (MotorVar / MotorInfoCollect / MotorVCMain / MotorPWM ...).  Their types
 *      come from the real project headers -- no hand-written stand-ins -- so
 *      field widths and layout are the firmware's, which matters because the
 *      case shim folds `int` to the target's 16 bits: Uint storage and int
 *      storage must match the C28x, see shim/DSP2803x_Device.h.
 *      (gIMTQ24_obs / gIMTQ12_obs are NOT here: MotorSVC.c defines them, so
 *      ref_tu.c / new_tu.c rename them per side, and these four functions
 *      never touch them.)
 *
 * The library helpers the module calls (abs, qsin, qatan, qsqrt, atan) are NOT
 * here: they come from the shared hostlib_mock.c / atan_mock.c, and __IQsat is
 * the static model in the case shim.
 */

#include "MotorInclude.h"

/* ---- 1. CPU timer -------------------------------------------------------- */

volatile struct CPUTIMER_REGS gHostCpuTimer1Regs;
struct CPUTIMER_VARS          CpuTimer1;

/* ---- 2. CPU control sequence --------------------------------------------- */

volatile Uint16 gHostCpuCtl[7];

/* ---- 3. globals owned by other translation units ------------------------- */
/* The set is MotorSVC.c's object file's undefined DATA symbols, read out with
 * coff_cmp (14 entries), with types taken from the headers that declare them. */

BASE_COMMAND_STRUCT                gMainCmd;
ANGLE_STRUCT                       gPhase;
BASE_PAR_STRUCT                    gBasePar;
VC_INFO_STRUCT                     gVCPar;
MOTOR_EXTERN_STRUCT                gMotorExtPer;
ALPHABETA_STRUCT                   gIAlphBetaQ12;
MT_STRUCT_Q24                      gIMTSetApply;
MT_STRUCT_Q24                      gIMTSet;
MT_STRUCT                          gIMTQ12;
MOTOR_DEBUG_DATA_RECEIVE_STRUCT    gTestDataReceive;
INV_STRUCT                         gInvInfo;
Uint                               gRatio;
MOTOR_STRUCT                       gMotorInfo;
ALPHABETA_STRUCT                   gABVoltSet;
