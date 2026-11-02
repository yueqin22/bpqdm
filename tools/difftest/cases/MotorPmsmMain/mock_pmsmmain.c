/* Shared mock for the MotorPmsmMain differential test.
 *
 * src/motor/MotorPmsmMain.c owns five data objects of its own
 * (gIPMPos / gFluxWeak / gPMInitPos / gIPMPosCheck / gPmDecoup -- they show up
 * as `B` symbols in build/Debug/MotorPmsmMain.obj) and ref_tu.c / new_tu.c
 * rename those per side.  Everything else it touches is an extern owned by
 * another translation unit, and that is what this file supplies:
 *
 *   3 peripheral register objects  EQepRegs / PieCtrlRegs / GpioDataRegs
 *   1 CPU-control counter table    gHostCpuCtl[]  (DINT / EINT / EALLOW / EDIS)
 *  21 globals                      read and/or written by the module
 *   9 callee stubs                  recorded in gMockCall[] / gMockArg[]
 *
 * The list is the module object file's undefined-symbol set read out with
 * `nm2000 build/Debug/MotorPmsmMain.obj` (37 entries) minus the two runtime
 * helpers (L$$DIV / L$$MOD) minus the nine functions minus the three
 * registers.  It is not hand-guessed.
 *
 * Types come from the real project headers -- mock_pmsmmain.c includes
 * MotorInclude.h, which real.txt forwards verbatim.  No hand-written stand-in
 * structs: a field offset that disagreed with the firmware would make the
 * whole comparison compare the wrong words.
 *
 * ---------------------------------------------------------------------------
 * Why the callee stubs record instead of doing something
 * ---------------------------------------------------------------------------
 * Every callee below lives in another module and is not under test.  Giving it
 * a body would be wrong twice over: it would test the wrong module, and any
 * state it wrote would be written identically on both sides, i.e. invisible.
 * So each one increments its slot in gMockCall[] and, where the argument
 * carries information the module computed, stores it in gMockArg[].  That is
 * what makes `SetIPMPos(gIPMPos.RotorZero)` observable at all: the module's
 * answer leaves the module through an argument, and dropping or changing the
 * argument is exactly the kind of rewrite error this bench has to catch.
 *
 * The interesting values the real callees would PRODUCE are instead promoted
 * to driver inputs: GetUvwPhase() would write gUVWPG.UVWAngle, so the driver
 * pokes gUVWPG.UVWAngle directly before each case.  That is strictly more
 * coverage than driving it through the callee could give -- the callee's own
 * arithmetic is not under test here and would only add noise.
 *
 * What this case therefore does NOT cover, stated plainly: the bodies of the
 * nine callees, the target-side effect of DINT/EINT/EALLOW/EDIS, and the real
 * behaviour of EQepRegs.QPOSCNT (it is a plain word here).
 */

#include "MotorInclude.h"
#include "mock_calls.h"

/* ---- call ledger --------------------------------------------------------- */

unsigned short gMockCall[MC_COUNT];
unsigned long  gMockArg[MC_COUNT];

/* ---- 1. peripheral registers -------------------------------------------- */
/* EQepRegs is a POINTER, not an object: MotorEncoder.h line 170 declares
 *     extern struct EQEP_REGS *EQepRegs;
 * and SubPrgInclude.h line 22 makes GetQepCnt() `(*EQepRegs).QPOSCNT`.  So the
 * mock owns a real EQEP_REGS instance and points the module at it -- which is
 * also what makes "the rewrite stopped using GetQepCnt()" observable, because
 * the only way to reach QPOSCNT is through that pointer.
 *
 * PG_Zero_isr writes QCLR.all = 0x0401; SynCalRealPos reads QPOSCNT twice.
 *
 * PieCtrlRegs: PG_Zero_isr writes PIEACK.all = PIEACK_GROUP5.
 * GpioDataRegs: EnableDrive()/DisableDrive() are macros on GPACLEAR/GPASET bit
 * GPIO7 (SystemDefine.h 83/84), used by RunCaseIpmInitPos. */

struct EQEP_REGS                gEqepShadow;
struct EQEP_REGS               *EQepRegs = &gEqepShadow;

volatile struct PIE_CTRL_REGS   PieCtrlRegs;
volatile struct GPIO_DATA_REGS  GpioDataRegs;

/* ---- 2. CPU control sequence -------------------------------------------- */
/* DINT / EINT in SynCalRealPos bracket the two QEP snapshots; EALLOW / EDIS in
 * PG_Zero_isr bracket the QCLR write.  Counting them keeps "the rewrite dropped
 * the critical section" observable.  Index order is the case's own:
 *   0 DINT  1 EINT  2 ERTM  3 DRTM  4 EALLOW  5 EDIS  6 ESTOP0
 * (same table as cases/MotorSVC and cases/MotorMain, so the numbers mean the
 * same thing across cases). */

volatile Uint16 gHostCpuCtl[7];

/* ---- 3. the outside world's globals ------------------------------------- */

MOTOR_STRUCT                    gMotorInfo;
MOTOR_EXTERN_STRUCT             gMotorExtInfo;
MOTOR_EXTERN_STRUCT             gMotorExtPer;
MOTOR_EXTERN_STRUCT             gMotorExtReg;
RUN_STATUS_STRUCT               gMainStatus;
BASE_COMMAND_STRUCT             gMainCmd;
BASE_PAR_STRUCT                 gBasePar;
struct FAULT_CODE_INFOR_STRUCT_DEF gError;
FVC_SPEED_STRUCT                gFVCSpeed;
MOTOR_PARA_EST                  gGetParVarable;
MT_STRUCT_Q24                   gIMTQ24;
MT_STRUCT_Q24                   gIMTSet;
IPM_INITPOS_PULSE_STR           gIPMInitPos;
IPM_ZERO_POS_STRUCT             gIPMZero;
PID32_STRUCT                    gImAcrQ24;
PID32_STRUCT                    gItAcrQ24;
OUT_VOLT_STRUCT                 gOutVolt;
PG_DATA_STRUCT                  gPGData;
PMSM_EST_PARAM_DATA             gPmParEst;
IPM_UVW_PG_STRUCT               gUVWPG;
VC_INFO_STRUCT                  gVCPar;

/* ---- 4. callees --------------------------------------------------------- */

void GetUvwPhase(void)              { gMockCall[MC_GETUVWPHASE]++; }
void InitSetAdc(void)               { gMockCall[MC_INITSETADC]++; }
void InitSetPWM(void)               { gMockCall[MC_INITSETPWM]++; }
void SetInterruptEnable(void)       { gMockCall[MC_SETINTENABLE]++; }
void PrepareParForRun(void)         { gMockCall[MC_PREPPARFORRUN]++; }
void TurnToStopStatus(void)         { gMockCall[MC_TURNTOSTOP]++; }

void SynInitPosDetSetPwm(Uint Section)
{
    gMockCall[MC_SYNINITPOS]++ ;
    gMockArg[MC_SYNINITPOS] = (unsigned long)Section;
}

void SetIPMPos(Uint Pos)
{
    gMockCall[MC_SETIPMPOS]++ ;
    gMockArg[MC_SETIPMPOS] = (unsigned long)Pos;
}

void SetIPMPos_ABZRef(Uint Pos)
{
    gMockCall[MC_SETIPMPOSABZ]++ ;
    gMockArg[MC_SETIPMPOSABZ] = (unsigned long)Pos;
}

/* PrepareParForRun / TurnToStopStatus are DEFINED in MotorMain.c but declared
 * in no header -- the firmware relies on C89 implicit declaration, so the
 * caller inside MotorPmsmMain.c sees `int f()` and the definition is
 * `void f(void)`.  That mismatch is in the original project (it is one of the
 * 28 `implicit_declaration` findings in docs/static_check_report.md) and is
 * reproduced here on purpose: "fixing" it in the mock would mean the host
 * build had a different prototype than the target build. */
