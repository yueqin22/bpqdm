/* Shared mock for the MotorMain differential test.
 *
 * src/motor/MotorMain.c DEFINES NO DATA OF ITS OWN -- its object file has only
 * `T` symbols (the 15 entry points).  Everything it touches is an extern owned
 * by another translation unit.  Two consequences:
 *
 *   1. There is no per-side private state at all, unlike MotorInvProtect.  The
 *      driver keeps ONE input image of every global below, hands it to the ref
 *      side, restores it, hands it to the new side, and diffs the two results.
 *   2. The mock below is the whole outside world: 56 globals, 73 callees, the
 *      CPU-control counter table, `asm`, and the 16-bit `abs`.
 *
 * Both ref_tu.c and new_tu.c link against this single copy, so both sides see
 * the same memory.  A rewrite that stops writing a field, writes another value,
 * or drops/reorders a call changes what the driver compares.
 *
 * ---------------------------------------------------------------------------
 * Why the callee stubs only count
 * ---------------------------------------------------------------------------
 * Every callee (VcAsrControl, GetUDCInfo, SoftPWMProcess, ...) lives in another
 * module and is not under test here.  Reproducing their bodies would be wrong in
 * two ways: it would test the wrong module, and any state they write would be
 * written identically on both sides, i.e. invisible.  So each is a no-op that
 * only increments its slot in gMockCall[].  The interesting inputs that the real
 * callees would produce are instead promoted to *driver inputs* (the driver
 * pokes the global the module reads downstream of the call), which is strictly
 * more coverage than driving them through the callee could be.
 *
 * What this case therefore does NOT cover, stated plainly: the bodies of all 73
 * callees, and the target-side effect of DINT/EINT.  Those belong to their own
 * modules / to the target check.
 *
 * `asm(" NOP")` (PWMZeroInterrupt) is a function here: on the C28x it is inline
 * assembly that leaves no symbol, on the host it parses as a call, and counting
 * it is better than deleting it from the source.
 */

#include "MotorInclude.h"
#include "mock_calls.h"

/* ---- call ledger --------------------------------------------------------- */

unsigned short gMockCall[MC_COUNT];
unsigned short gAsmCount;

/* ---- 1. peripheral registers -------------------------------------------- */
/* The only peripheral MotorMain reaches is GpioDataRegs, through the
 * DisableDrive()/EnableDrive()/GetOverUdcFlag()/RT_SAMPLE_*/
/* ROTOR_TRANS_* macros of SystemDefine.h and the TurnOn/OffFan macros of
 * MotorDefine.h.  It is the real struct (real.txt forwards DSP2803x_Gpio.h),
 * so the bit positions are the chip's -- including GPADAT.GPIO23 and
 * AIODAT.AIO12, which ADCOverInterrupt writes when PGType == PG_TYPE_RESOLVER
 * (TMS320F28035 is defined by SystemDefine.h, so that branch is live). */

volatile struct GPIO_DATA_REGS GpioDataRegs;

/* ---- 2. CPU control sequence -------------------------------------------- */
/* DINT/EINT in SendDataPrepare -> increments, shared by both sides. */

volatile Uint16 gHostCpuCtl[7];

/* ---- 3. the module's global inputs and outputs -------------------------- */
/* One flat list; the driver mirrors it in its IMAGE struct, with the same
 * order and the same types.  Types come from the real project headers (no
 * hand-written stand-ins).  The set is the module object file's undefined
 * DATA symbols read out with coff_cmp -- 56 entries -- PLUS gComPar.
 *
 * Why gComPar is here and is not in that 56: in Main2msMotorB the module reads
 * `gComPar.SpdSearchMethod` in an if/else whose two arms call the SAME function
 * (`RunCaseSpeedCheck()` in both).  cl2000 folded the comparison away, so the
 * target object file has no gComPar reference at all.  MSVC /Od keeps the load,
 * so the host link needs the symbol; defining it (and putting it in the input
 * image) keeps the two sides symmetric and costs nothing. */

/* command / status / parameters */
INV_STRUCT                       gInvInfo;
CONTROL_MOTOR_TYPE_ENUM          gCtrMotorType;
MOTOR_STRUCT                     gMotorInfo;
RUN_STATUS_STRUCT                gMainStatus;
BASE_COMMAND_STRUCT              gMainCmd;
MAIN_COMMAND_EXTEND_UNION        gExtendCmd;
BASE_PAR_STRUCT                  gBasePar;
COM_PAR_INFO_STRUCT              gComPar;
CPU_TIME_STRUCT                  gCpuTime;
MOTOR_DEBUG_DATA_RECEIVE_STRUCT  gTestDataReceive;

/* current / voltage sampling */
ADC_STRUCT                       gADC;
UDC_STRUCT                       gUDC;
IUVW_SAMPLING_STRUCT             gCurSamp;
UVW_STRUCT                       gIUVWQ12;
UVW_STRUCT_Q24                   gIUVWQ24;
ALPHABETA_STRUCT                 gIAlphBeta;
MT_STRUCT                        gIMTQ12;
MT_STRUCT_Q24                    gIMTQ24;
MT_STRUCT_Q24                    gIMTQ12_obs;
MT_STRUCT_Q24                    gIMTQ24_obs;
MT_STRUCT_Q24                    gIMTSet;
AMPTHETA_STRUCT                  gIAmpTheta;
LINE_CURRENT_STRUCT              gLineCur;

/* output voltage / PWM */
OUT_VOLT_STRUCT                  gOutVolt;
FC_CAL_STRUCT                    gFcCal;
PWM_OUT_STRUCT                   gPWM;
DEAD_BAND_STRUCT                 gDeadBand;
Uint                             gRatio;

/* protection */
CBC_PROTECT_STRUCT               gCBCProtect;
struct FAULT_CODE_INFOR_STRUCT_DEF gError;
DC_BRAKE_STRUCT                  gDCBrake;

/* information collection / identification */
AI_STRUCT                        gAI;
UV_BIAS_COFF_STRUCT              gUVCoff;
MOTOR_PARA_EST                   gGetParVarable;

/* VF / VC / SVC control blocks */
VF_INFO_STRUCT                   gVFPar;
VF_WS_COMP_STRUCT                gWsComp;
VAR_AVR_STRUCT                   gVarAvr;
HVF_OSC_DAMP_STRUCT              gHVfOscDamp;
VC_INFO_STRUCT                   gVCPar;
PID32_STRUCT                     gImAcrQ24;
PID32_STRUCT                     gItAcrQ24;
FLUX_STRUCT                      gFluxR;

/* speed check / PM */
FEISU_STRUCT                     gFeisu;
FEISU_STRUCT_NEW                 gFeisuNew;
IPM_POSITION_STRUCT              gIPMPos;
PM_FLUX_WEAK                     gFluxWeak;
IPM_POS_CHECK_STRUCT             gIPMPosCheck;
IPM_ZERO_POS_STRUCT              gIPMZero;
IPM_INITPOS_PULSE_STR            gIPMInitPos;
PMSM_EST_PARAM_DATA              gPmParEst;
PMSM_EST_BEMF                    gEstBemf;
IPM_UVW_PG_STRUCT                gUVWPG;

/* encoder / rotor */
PG_DATA_STRUCT                   gPGData;
BURR_FILTER_STRUCT               gSpeedFilter;
ROTOR_TRANS_STRUCT               gRotorTrans;
FVC_SPEED_STRUCT                 gFVCSpeed;

/* ---- 4. callee stubs ---------------------------------------------------- */

#define MOCK_STUB(name, params) void name params { gMockCall[MC_##name]++; }

MOCK_CALL_LIST(MOCK_STUB)

#undef MOCK_STUB

/* ---- 5. `asm(" NOP")` and the 16-bit C28x `abs` -------------------------- */

void asm(const char *mCode)
{
    (void)mCode;
    gAsmCount++;
}

/* 16-bit `abs`, not the host CRT's 32-bit one.
 *
 * `nm2000 build/Debug/MotorMain.obj` shows no `_abs` undefined symbol: cl2000
 * resolves abs() to the runtime-library `_abs` (present in the map at
 * 0x003f3d1a) and inlines it.  MotorMain calls abs() on three quantities:
 * gMainCmd.FreqSet, gVFPar.tpLst (Main2msMotorB, RunCaseRun2Ms) and gIMTQ12.T
 * (SendDataPrepare via Filter32) -- all signed and all able to be negative, so
 * a no-op abs would swallow exactly the mutations that move or delete the call.
 *
 * Width matters: with the shim's `#define int short` this definition and the
 * project's own `extern int abs(int)` prototype (SubPrgInclude.h:75) are both
 * 16-bit, so the call site and the callee agree.  <stdlib.h> is deliberately
 * not included anywhere in this case -- its 32-bit `int abs(int)` would collide
 * with the folded prototype.  abs(-32768) wraps back to -32768, which is what
 * the C28x runtime does.
 *
 * (Note: MotorMain's include chain does NOT reach ZeroPosCtl.h, so there is no
 * `#define abs(x) ((x>0)?(x):-(x))` macro in play here -- that macro is only
 * visible to modules that include ZeroPosCtl.h, e.g. ZeroPosCtl.c itself.) */
int abs(int mValue)
{
    return (mValue < 0) ? -mValue : mValue;
}
