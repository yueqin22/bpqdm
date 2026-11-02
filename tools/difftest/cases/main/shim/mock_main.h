/* Shared declarations for the main case bench -- driver.c and mock_main.c.
 *
 * The module under test (main.c) calls a set of extern task functions and the
 * CPU-control macros.  On the target those either drive hardware or are
 * single assembly instructions; on the host every one of them is funnelled
 * into an ordered event log, so that the differential test compares the exact
 * *sequence* of side effects, not just final register values.
 */
#ifndef MOCK_MAIN_H
#define MOCK_MAIN_H

#include <setjmp.h>

#include "MotorInclude.h"

#define HOST_LOG_MAX   65536
#define HOST_CNT_MAX   64
#define HOST_TRACE_MAX 4096

/* register trace: at every logged event the mock also stores the four
 * register words the module can touch.  This is what makes the *order* of a
 * plain register assignment (e.g. `PieCtrlRegs.PIEACK.all = ...`) relative to
 * its neighbouring statements observable -- comparing only final register
 * values would miss a swapped pair (that is exactly how MN25/MN27 slipped
 * through the first bench). */
#define HOST_TRACE_WORDS 5

extern volatile Uint16 gHostCpuCtl[7];
extern Uint16 gHostLog[HOST_LOG_MAX];
extern Uint   gHostLogLen;
extern Uint32 gHostCnt[HOST_CNT_MAX];
extern Uint16 gHostRegTrace[HOST_TRACE_MAX][HOST_TRACE_WORDS];
extern Uint   gHostTraceLen;

void gHostCtlEvent(int idx);
void gHostLogCall(Uint16 id);
void gHostResetState(void);

void gHostDisableDrive(void);
void gHostAdcClearIntFlag(void);
void gHostAdcResetSequence(void);

#define HOST_EV_GETTIME  9
#define HOST_EV_DISDRV   53
#define HOST_EV_ADCCLR   60
#define HOST_EV_ADCRST   61

extern CPU_TIME_STRUCT gCpuTime;

extern Uint32 gHostTime;
extern Uint32 gHostTimeStep;
extern Uint32 gHostTimeCalls;
extern Uint32 gHostBudget;
extern int    gMockAbortArmed;
extern jmp_buf gMockAbort;

Ulong gHostGetTime(void);

#endif
