/* Host-side mock for the main case: replaces every extern main.c calls.
 *
 * Two jobs:
 *   1. give every called symbol a definition whose side effects are *visible*
 *      (an ordered event log + per-id counters), so the differential test can
 *      compare ref vs new side-effect sequences, not just final memory;
 *   2. model GetTime() as a virtual down-counter and give main()'s while(1)
 *      loop a termination oracle (call budget -> longjmp), because on the
 *      target that loop never exits.
 *
 * The mock is SHARED between the ref and the new translation unit (it is not
 * part of the per-side renaming), exactly like gTestDataReceive in the
 * MotorDebug case.  The driver therefore has to reset the shared state
 * (gHostResetState) before each side runs, then snapshot and compare.
 */
#include <setjmp.h>
#include <string.h>

#include "mock_main.h"

jmp_buf gMockAbort;

volatile Uint16 gHostCpuCtl[7];
Uint16 gHostLog[HOST_LOG_MAX];
Uint   gHostLogLen;
Uint32 gHostCnt[HOST_CNT_MAX];
Uint16 gHostRegTrace[HOST_TRACE_MAX][HOST_TRACE_WORDS];
Uint   gHostTraceLen;

CPU_TIME_STRUCT gCpuTime;

struct ADC_REGS AdcRegs;
struct EPWM_REGS EPwm1Regs;
struct PIE_CTRL_REGS PieCtrlRegs;
struct GPIO_DATA_REGS GpioDataRegs;

Uint32 gHostTime;
Uint32 gHostTimeStep;
Uint32 gHostTimeCalls;
Uint32 gHostBudget;
int    gMockAbortArmed;

void gHostLogCall(Uint16 id)
{
    if (id < HOST_CNT_MAX)
    {
        gHostCnt[id]++;
    }
    if (gHostLogLen < HOST_LOG_MAX)
    {
        gHostLog[gHostLogLen] = id;
    }
    if (gHostTraceLen < HOST_TRACE_MAX)
    {
        gHostRegTrace[gHostTraceLen][0] = (Uint16)AdcRegs.ADCINTFLGCLR.all;
        gHostRegTrace[gHostTraceLen][1] = (Uint16)AdcRegs.SOCPRICTL.all;
        gHostRegTrace[gHostTraceLen][2] = (Uint16)EPwm1Regs.ETCLR.all;
        gHostRegTrace[gHostTraceLen][3] = (Uint16)PieCtrlRegs.PIEACK.all;
        gHostRegTrace[gHostTraceLen][4] = (Uint16)GpioDataRegs.GPASET.all;
        gHostTraceLen++;
    }
    if (gHostLogLen < HOST_LOG_MAX)
    {
        gHostLogLen++;
    }
}

void gHostCtlEvent(int idx)
{
    if (idx >= 0 && idx < 7)
    {
        gHostCpuCtl[idx]++;
    }
    gHostLogCall((Uint16)idx);
}

void gHostResetState(void)
{
    memset((void *)gHostCpuCtl, 0, sizeof gHostCpuCtl);
    memset(gHostLog, 0, sizeof gHostLog);
    memset(gHostCnt, 0, sizeof gHostCnt);
    gHostLogLen = 0;
    memset(gHostRegTrace, 0, sizeof gHostRegTrace);
    gHostTraceLen = 0;
    memset(&gCpuTime, 0, sizeof gCpuTime);
    memset(&AdcRegs, 0, sizeof AdcRegs);
    memset(&EPwm1Regs, 0, sizeof EPwm1Regs);
    memset(&PieCtrlRegs, 0, sizeof PieCtrlRegs);
    gHostTimeCalls = 0;
}

Ulong gHostGetTime(void)
{
    gHostTimeCalls++;
    if (gMockAbortArmed && gHostBudget && (gHostTimeCalls > gHostBudget))
    {
        longjmp(gMockAbort, 1);
    }
    gHostTime -= gHostTimeStep;
    gHostLogCall(HOST_EV_GETTIME);
    return gHostTime;
}

/* The real SystemDefine.h line 123 defines DisableDrive() as a macro that
 * writes GpioDataRegs.GPASET.bit.GPIO7.  The bench keeps the write identical
 * and only adds the event -- otherwise "the TZ ISR calls it before/after
 * HardWareErrorDeal" is invisible (MN25). */
void gHostDisableDrive(void)
{
    GpioDataRegs.GPASET.bit.GPIO7 = 1;
    gHostLogCall(HOST_EV_DISDRV);
}

void gHostAdcClearIntFlag(void)
{
    AdcRegs.ADCINTFLGCLR.bit.ADCINT1 = 1;
    gHostLogCall(HOST_EV_ADCCLR);
}

void gHostAdcResetSequence(void)
{
    AdcRegs.SOCPRICTL.bit.RRPOINTER = 0x20;
    gHostLogCall(HOST_EV_ADCRST);
}

#define TASK(id, name)                                                    \
    void name(void)                                                       \
    {                                                                     \
        gHostLogCall((Uint16)(id));                                       \
    }

TASK(10, InitSysCtrl)
TASK(11, InitInterrupt)
TASK(12, InitPeripherals)
TASK(13, InitForMotorApp)
TASK(14, InitForFunctionApp)
TASK(15, EnableDog)
TASK(16, SetInterruptEnable)
TASK(17, KickDog)
/* DisableDrive is NOT a function: the real SystemDefine.h line 123 defines it
 * as `#define DisableDrive() GpioDataRegs.GPASET.bit.GPIO7 = 1`, so
 * EPWM1_TZ_isr's call is a GPIO write, observed through the GpioDataRegs
 * snapshot instead of the event log. */

TASK(30, Main05msMotor)
TASK(31, Main05msFunctionA)
TASK(32, Main05msFunctionB)
TASK(33, Main05msFunctionC)
TASK(34, Main05msFunctionD)
TASK(35, Main0msFunction)
TASK(36, Main0msMotor)
TASK(37, Main2msMotorA)
TASK(38, Main2msMotorB)
TASK(39, Main2msMotorC)
TASK(40, Main2msMotorD)

TASK(50, ADCOverInterrupt)
TASK(51, PWMZeroInterrupt)
TASK(52, HardWareErrorDeal)
