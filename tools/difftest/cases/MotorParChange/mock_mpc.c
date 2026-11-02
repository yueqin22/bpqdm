/* Everything MotorParChange.c touches but does not define.
 *
 * The module is a "parameter recalculation" pass: it reads operator settings
 * and other modules' results, and writes derived scalings. So almost every
 * symbol here is BOTH an input and an output, and the driver keeps one input
 * image of all of them and restores it before each side runs.
 *
 * Groups:
 *   1. peripheral registers -- three real chip structs (EPwm1/2/3, EQep1) plus
 *      PieVectTable / PieCtrlRegs / SysCtrlRegs, forwarded verbatim from
 *      include/device so the bitfield layout is the chip's. The module writes
 *      EPwm1..3 DBFED/DBRED, EQep QDECCTL.SWAP and QEINT.all, the PIE vector
 *      for EQEP1, PIEIER5.INTx1 and PCLKCR1.EQEP1ENCLK -- "which register, which
 *      field, in what order" is exactly what has to stay comparable.
 *   2. EQepRegs is a *pointer* (MotorEncoder.h:170). It is REASSIGNED by
 *      SystemParChg05Ms (`EQepRegs = (struct EQEP_REGS *)&EQep1Regs;`), so the
 *      pointer itself is an observable. Both the pointee (a real EQEP_REGS
 *      instance) and the pointer go into the compared image.
 *   3. constant tables -- defined here with the same element counts as
 *      MotorConstant.c, because the module indexes them with values that come
 *      from the table itself (`*(pInvVolt+5)` is maxType, then
 *      `*(pInvCur + InvType - minType)`). A different length would change what
 *      an out-of-range index reads.
 *   4. callee stubs -- SetIPMPos / SetIPMPos_ABZRef / ReInitForPG / InitSetQEP
 *      record the call and its argument. PG_Zero_isr only has its address
 *      taken (stored into PieVectTable.EQEP1_INT), so it is an empty body whose
 *      address is still distinct from every other function.
 *   5. abs / qsin / qsqrt come from hostlib_mock.c (mocks.txt).
 */

#include "MotorInclude.h"
#include "mock_calls.h"

/* ---- 0. CPU control sequence -------------------------------------------- */

/* The shim turns EINT/DINT/EALLOW/EDIS into increments of this array, so the
 * module's four critical sections become observable. */
volatile Uint16 gHostCpuCtl[7];

/* ---- 1. peripherals ------------------------------------------------------ */

volatile struct EPWM_REGS      EPwm1Regs;
volatile struct EPWM_REGS      EPwm2Regs;
volatile struct EPWM_REGS      EPwm3Regs;
volatile struct EQEP_REGS      EQep1Regs;
struct PIE_VECT_TABLE          PieVectTable;
volatile struct PIE_CTRL_REGS  PieCtrlRegs;
volatile struct SYS_CTRL_REGS  SysCtrlRegs;

/* ---- 2. the EQEP pointer and its pointee -------------------------------- */

struct EQEP_REGS   gHostEQep;
struct EQEP_REGS  *EQepRegs = &gHostEQep;

/* ---- 2b. the 33 globals owned by other modules -------------------------- */

/* This module owns nothing, so every one of these has to be defined here.
 * The types are the real ones from the real headers -- a hand-written
 * substitute would change the field offsets and with them what "the same
 * write" means. */
ADC_STRUCT               gADC;
BASE_PAR_STRUCT          gBasePar;
IUVW_SAMPLING_STRUCT     gCurSamp;
DEAD_BAND_STRUCT         gDeadBand;
MAIN_COMMAND_EXTEND_UNION gExtendCmd;
AMPTHETA_STRUCT          gIAmpTheta;
MT_STRUCT                gIMTQ12;
MT_STRUCT_Q24            gIMTSetApply;
MT_STRUCT                gIMTSetQ12;
IPM_INITPOS_PULSE_STR    gIPMInitPos;
IPM_POSITION_STRUCT      gIPMPos;
IPM_POS_CHECK_STRUCT     gIPMPosCheck;
UVW_STRUCT               gIUVWQ12;
UVW_STRUCT_Q24           gIUVWQ24;
INPUT_LOSE_STRUCT        gInLose;
INV_STRUCT               gInvInfo;
LINE_CURRENT_STRUCT      gLineCur;
BASE_COMMAND_STRUCT      gMainCmd;
RUN_STATUS_STRUCT        gMainStatus;
MOTOR_EXTERN_STRUCT      gMotorExtInfo;
MOTOR_EXTERN_STRUCT      gMotorExtPer;
MOTOR_STRUCT             gMotorInfo;
OUT_VOLT_STRUCT          gOutVolt;
OVER_UDC_CTL_STRUCT      gOvUdc;
PG_DATA_STRUCT           gPGData;
PM_DECOUPLE              gPmDecoup;
MOTOR_POWER_TORQUE       gPowerTrq;
ROTOR_TRANS_STRUCT       gRotorTrans;
UDC_STRUCT               gUDC;
UV_BIAS_COFF_STRUCT      gUVCoff;
IPM_UVW_PG_STRUCT        gUVWPG;
VF_INFO_STRUCT           gVFPar;
VAR_AVR_STRUCT           gVarAvr;

/* ---- 3. constant tables (element counts match MotorConstant.c) ---------- */

Uint const gDeadBandTable[14] =
{
    192, 192, 192, 192, 240, 240, 240, 288, 288, 288, 288, 288, 288, 288
};

Uint const gDeadCompTable[14] =
{
    78, 87, 87, 87, 117, 117, 117, 129, 129, 129, 135, 135, 120, 120
};

Uint const gInvCurrentTable220S[8] =
{
    230, 230, 230, 400, 700, 960, 1700, 2500
};

Uint const gInvCurrentTable220T[16] =
{
    210, 380, 510, 900, 1300, 2500, 3200, 4500,
    6000, 7500, 9000, 11000, 1520, 1760, 2100, 3040
};

Uint const gInvCurrentTable380T[27] =
{
    210, 380, 510, 900, 1300,
    1700, 2500, 3200, 3700, 4500,
    6000, 7500, 9000, 11000,
    1520, 1760, 2100, 2530, 3040,
    3770, 4260, 4650, 5200, 5850,
    6500, 7250, 8200
};

Uint const gInvCurrentTable690T[33] =
{
    80, 160, 240, 400, 600,
    800, 1200, 1600, 2000, 2400,
    3300, 4100, 5000, 6200,
    850, 1020, 1250, 1500, 1750,
    2150, 2450, 2600, 2990, 3300,
    3740, 4100, 4650, 5500, 5900,
    6000, 7000, 8000, 9000
};

Uint const gInvCurrentTable1140T[19] =
{
    2500, 3000, 3700, 5000, 590,
    700, 910, 1170, 1300, 1480,
    1570, 1810, 2000, 2260,
    2480, 2810, 3330, 3480, 4000
};

Uint const gInvVoltageInfo220S[9]  = { 220, 2000, 4000,  3111,  0,  7, 99,  4655,  29 };
Uint const gInvVoltageInfo220T[9]  = { 220, 2000, 4000,  3111,  7, 22, 14,  4655,  29 };
Uint const gInvVoltageInfo380T[9]  = { 380, 3500, 8100,  5374,  8, 34, 16,  9309,  53 };
Uint const gInvVoltageInfo480T[9]  = { 480, 3500, 8900,  6500,  8, 34, 16, 10472,  62 };
Uint const gInvVoltageInfo690T[9]  = { 690, 6500, 13500, 9757,  8, 40,  5, 16812,  88 };
Uint const gInvVoltageInfo1140T[9] = { 1140, 13500, 20000, 16120, 19, 38, 19, 26420, 138 };

/* ---- 4. the module's callees -------------------------------------------- */

unsigned short gMockCall[MC_COUNT];
unsigned long  gMockArg[MC_COUNT];

static void bump(int slot, unsigned long arg)
{
    gMockCall[slot] = (unsigned short)(gMockCall[slot] + 1);
    gMockArg[slot]  = arg;
}

void SetIPMPos(Uint Pos)
{
    bump(MC_SETIPMPOS, (unsigned long)Pos);
}

void SetIPMPos_ABZRef(Uint Pos)
{
    bump(MC_SETIPMPOSABZ, (unsigned long)Pos);
}

void ReInitForPG(void)
{
    bump(MC_REINITFORPG, 0UL);
}

void InitSetQEP(void)
{
    bump(MC_INITSETQEP, 0UL);
}

/* Only the ADDRESS of this function is observable (it is stored into
 * PieVectTable.EQEP1_INT). An empty body is enough: a rewrite that drops or
 * changes the assignment changes which address lands in the vector table. */
void PG_Zero_isr(void)
{
    bump(MC_PGZEROISR, 0UL);
}
