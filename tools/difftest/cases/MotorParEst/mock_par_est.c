/* Shared mock for the MotorParEst differential test.
 *
 * One instance of everything the module touches but does NOT define. Both
 * translation units see the same memory, so a rewrite that stops writing a
 * register, or writes a different vector, changes what is compared.
 *
 * Three groups:
 *
 *   1. peripheral registers   PieVectTable / PieCtrlRegs / GpioDataRegs.
 *      The structs come from the real device headers (see real.txt), so field
 *      order and bitfield layout are the chip's, not an imitation.
 *      PieVectTable is the interesting one: EndOfParIdentify re-points
 *      ADCINT1 / EPWM1_TZINT / EPWM1_INT at three ISR entry points.
 *
 *   2. CPU control sequence   the shim turns EINT/DINT/ERTM/DRTM/EALLOW/EDIS/
 *      ESTOP0 into increments of gHostCpuCtl[]. They are real C28x instructions
 *      with no memory operand, so without this they would be invisible: "the
 *      rewrite dropped DINT" would produce identical state on both sides.
 *
 *   3. call log               every callee the module dispatches to appends its
 *      id to gCallLog. RunCaseGetPar's switch is the module's whole job, so
 *      "wrong case body" (IDENTIFY_RR_LO -> LmIoIdentify) has to be observable;
 *      with plain empty stubs it would not be.
 *
 * The ISR bodies are needed because the module takes their ADDRESS
 * (&ADC_Over_isr). Both sides take the same address, so they compare equal.
 */

#include "MotorParaIDinclude.h"

/* ---- 1. peripheral registers -------------------------------------------- */

struct PIE_VECT_TABLE          PieVectTable;
volatile struct PIE_CTRL_REGS  PieCtrlRegs;
volatile struct GPIO_DATA_REGS GpioDataRegs;

/* ---- 2. CPU control sequence -------------------------------------------- */

volatile Uint16 gHostCpuCtl[7];

/* ---- 3. call log --------------------------------------------------------- */

#define CALL_LOG_LEN 64

Uint16 gCallLog[CALL_LOG_LEN];
Uint16 gCallCount;

static void log_call(Uint16 id)
{
    if (gCallCount < CALL_LOG_LEN)
    {
        gCallLog[gCallCount] = id;
    }
    gCallCount++;
}

/* 调用者清单来自目标端 nm2000 的未定义符号表, 不是人眼数的。 */
void RsIdentify(void)            { log_call(1);  }
void RrLoIdentify(void)          { log_call(2);  }
void LmIoIdentify(void)          { log_call(3);  }
void SynTuneInitPos(void)        { log_call(4);  }
void SynTunePGZero_No_Load(void) { log_call(5);  }
void SynTunePGZero_Load(void)    { log_call(6);  }
void SynTuneBemf(void)           { log_call(7);  }
void ParSendTune(void)           { log_call(8);  }
void InitRrLoVariable(void)      { log_call(9);  }
void InitSetPWM(void)            { log_call(10); }
void InitSetAdc(void)            { log_call(11); }
void SetInterruptEnable(void)    { log_call(12); }

/* ---- ISR entry points ---------------------------------------------------- */

void ADC_Over_isr(void)   { }
void EPWM1_TZ_isr(void)   { }
void EPWM1_zero_isr(void) { }
void PG_Zero_isr(void)    { }

/* ---- globals owned by other modules -------------------------------------- */

RUN_STATUS_STRUCT                  gMainStatus;
BASE_COMMAND_STRUCT                gMainCmd;
struct FAULT_CODE_INFOR_STRUCT_DEF gError;
MOTOR_EXTERN_STRUCT                gMotorExtInfo;
PMSM_EST_BEMF                      gEstBemf;
PMSM_EST_PARAM_DATA                gPmParEst;
IPM_ZERO_POS_STRUCT                gIPMZero;
IPM_POSITION_STRUCT                gIPMPos;
IPM_UVW_PG_STRUCT                  gUVWPG;
PG_DATA_STRUCT                     gPGData;
VC_INFO_STRUCT                     gVCPar;
