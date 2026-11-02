/* Shared mock for the MotorPmsmParEst differential test.
 *
 * Everything the module touches but does NOT define lives here. Both
 * translation units see the same memory, so a rewrite that stops writing a
 * field, writes a different value, or skips a call changes what the driver
 * compares.
 *
 * Six groups:
 *
 *   1. peripheral registers. Three of them are real structures from
 *      include/device (forwarded by real.txt), so the bitfield layouts are the
 *      chip's:
 *          GpioDataRegs   written by the DisableDrive()/EnableDrive() MACROS
 *                         of SystemDefine.h -- there is no call to observe,
 *                         only the register traffic (GPASET/GPACLEAR.GPIO7)
 *          EPwm1/2/3Regs  SynInitPosDetSetTs writes TBPRD / CMPB;
 *                         SynInitPosDetSetPwm writes AQCSFRC / DBCTL / AQCTLA /
 *                         AQCTLB across its eight arms
 *      EQepRegs is a *pointer* (MotorEncoder.h: `extern struct EQEP_REGS
 *      *EQepRegs;`), so a real `struct EQEP_REGS` instance is defined here and
 *      EQepRegs points at it. The module reads QPOSCNT through GetQepCnt() and
 *      writes QDECCTL. The *pointee* -- not the pointer, which is never
 *      written -- is what the driver puts in the compared image.
 *
 *   2. CPU control sequence. The shim turns EINT/DINT/ERTM/DRTM/EALLOW/EDIS/
 *      ESTOP0 into increments of gHostCpuCtl[]. This module issues DINT/EINT
 *      around the QEP snapshot (SetIPMPos / SetIPMPos_ABZRef) and EALLOW/EDIS
 *      around the EPwm writes (SynInitPosDetSetTs / SynInitPosDetSetPwm /
 *      SynTunePGZero_No_Load case 1), so losing or moving one is a real defect
 *      that must not be invisible.
 *
 *   3. call ledger. The eight external callees are stubs (see the long note
 *      below). Five are no-ops that only bump gMockCall[]; the three Judge*
 *      callees return a driver-poked direction.
 *
 *   4. globals owned by other modules. These are the module's *inputs* as well
 *      as some of its outputs, so the driver keeps one input image of every
 *      one of them and restores it before each side runs -- otherwise the ref
 *      side's writes would leak into the new side.
 *
 *   5. the direction inputs the Judge* stubs return (gMockAbDir / gMockUvwDir /
 *      gMockRtDir), poked by the driver.
 *
 *   6. `abs` / `atan` come from the shared mock files in mocks.txt
 *      (hostlib_mock.c, atan_mock.c); they are deliberately absent here.
 *
 * ---------------------------------------------------------------------------
 * Why the callee stubs only count (or echo an input)
 * ---------------------------------------------------------------------------
 * GetUvwPhase / IPMCalAcrPIDCoff / InitSetPWM / ResetParForVC /
 * SynCalLdAndLq live in MotorEncoder.c / MotorPwmInclude.h's module /
 * MotorPmsmMain.c and are not under test here. Reproducing their bodies would
 * test the wrong module, and any state they wrote into a *module-owned* global
 * (gIPMInitPos.Ld/Lq via SynCalLdAndLq, gPmParEst.IdKp via IPMCalAcrPIDCoff)
 * could not be modelled anyway: each side owns its own copy, so a single
 * shared stub could only ever write one of them -- it would manufacture a
 * mismatch. Instead the values those callees would produce are promoted to
 * *driver inputs*: the driver pokes the module-owned fields directly before
 * each scenario (and sync_priv copies the ref side's copy to the new side), so
 * every value is reachable. What this case therefore does NOT cover, stated
 * plainly: the bodies of all eight callees. Those belong to their own modules.
 */

#include "MotorInclude.h"
#include "mock_calls.h"

/* ---- 1. peripheral registers -------------------------------------------- */

volatile struct GPIO_DATA_REGS  GpioDataRegs;
volatile struct EPWM_REGS       EPwm1Regs;
volatile struct EPWM_REGS       EPwm2Regs;
volatile struct EPWM_REGS       EPwm3Regs;

struct EQEP_REGS                gHostEQep;
struct EQEP_REGS               *EQepRegs = &gHostEQep;

/* ---- 2. CPU control sequence -------------------------------------------- */

volatile Uint16 gHostCpuCtl[7];

/* ---- 3. call ledger and direction inputs -------------------------------- */

unsigned short gMockCall[MC_COUNT];
unsigned short gMockAbDir;
unsigned short gMockUvwDir;
unsigned short gMockRtDir;

void GetUvwPhase(void)      { gMockCall[MC_GetUvwPhase]++; }
void IPMCalAcrPIDCoff(void) { gMockCall[MC_IPMCalAcrPIDCoff]++; }
void InitSetPWM(void)       { gMockCall[MC_InitSetPWM]++; }
void ResetParForVC(void)    { gMockCall[MC_ResetParForVC]++; }

void SynCalLdAndLq(Uint m_Pos)
{
    (void)m_Pos;
    gMockCall[MC_SynCalLdAndLq]++;
}

Uint JudgeABDir(void)  { gMockCall[MC_JudgeABDir]++;  return gMockAbDir;  }
Uint JudgeUVWDir(void) { gMockCall[MC_JudgeUVWDir]++; return gMockUvwDir; }
Uint JudgeRTDir(void)  { gMockCall[MC_JudgeRTDir]++;  return gMockRtDir;  }

/* ---- 4. globals owned by other modules ----------------------------------- */

IPM_PG_DIR_STRUCT                gPGDir;
IPM_UVW_PG_STRUCT                gUVWPG;
RUN_STATUS_STRUCT                gMainStatus;
MOTOR_PARA_EST                   gGetParVarable;
IPM_POSITION_STRUCT              gIPMPos;
MT_STRUCT_Q24                    gIMTSet;
MOTOR_STRUCT                     gMotorInfo;
INV_STRUCT                       gInvInfo;
MOTOR_EXTERN_STRUCT              gMotorExtInfo;
MOTOR_EXTERN_STRUCT              gMotorExtReg;
UDC_STRUCT                       gUDC;
ADC_STRUCT                       gADC;
BASE_COMMAND_STRUCT              gMainCmd;
LINE_CURRENT_STRUCT              gLineCur;
OUT_VOLT_STRUCT                  gOutVolt;
ROTOR_TRANS_STRUCT               gRotorTrans;
PG_DATA_STRUCT                   gPGData;
struct FAULT_CODE_INFOR_STRUCT_DEF gError;
UVW_STRUCT_Q24                   gIUVWQ24;
CONTROL_MOTOR_TYPE_ENUM          gCtrMotorType;
