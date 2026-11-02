/* Shared declarations for the mainInit_2803x differential test bench.
 *
 * Lives in the case's private shim/ so that both the mock (which DEFINES these
 * objects) and the driver (which inspects them) see the same declarations, and
 * so that the negative-control variants -- which copy the whole case directory,
 * shim/ included -- keep working.
 *
 * Two layers of observability, because this module has almost no data flow:
 *
 *   1. the register objects themselves (shared memory, refilled before every
 *      side) -- catches "wrote the wrong constant", "stopped writing a field",
 *      "wrote it twice";
 *   2. an EVENT LOG: every EALLOW/EDIS/DINT/EINT and every call into a stub
 *      appends one record = { event id, 32-bit fingerprint of all registers }.
 *      Without layer 2 the pair "EALLOW; write reg; EDIS" and
 *      "write reg; EALLOW; EDIS" produce identical final state and identical
 *      counter totals. The fingerprint at each event pins the register writes
 *      down to the event they precede.
 */
#ifndef MOCK_MAININIT_H
#define MOCK_MAININIT_H

#include "DSP2803x_Device.h"

/* ---- event ids ---------------------------------------------------------- */
/* 0..6 mirror the seven CPU-control macros, one per gHostCpuCtl slot. */
#define HOST_EV_EINT    0
#define HOST_EV_DINT    1
#define HOST_EV_ERTM    2
#define HOST_EV_DRTM    3
#define HOST_EV_EALLOW  4
#define HOST_EV_EDIS    5
#define HOST_EV_ESTOP0  6
/* 20..: calls out of the module into the mock. */
#define HOST_EV_SETPWM      20
#define HOST_EV_CPUTIMERS   21
#define HOST_EV_STARTT1     22
#define HOST_EV_MOVEPRG     23
#define HOST_EV_DEVCAL      24

#define HOST_LOG_MAX    8192
#define HOST_MOVE_MAX   16

extern volatile Uint16 gHostCpuCtl[7];
extern volatile Uint16 gHostIER;
extern volatile Uint16 gHostIFR;

extern int  gHostLogN;
extern int  gHostLogId[HOST_LOG_MAX];
extern unsigned long gHostLogFp[HOST_LOG_MAX];

extern int  gHostMoveN;
extern unsigned long gHostMoveSrc[HOST_MOVE_MAX];
extern unsigned long gHostMoveDst[HOST_MOVE_MAX];
extern unsigned long gHostMoveSize[HOST_MOVE_MAX];
extern unsigned long gHostMoveIx[HOST_MOVE_MAX];

extern int gHostDevCalN;

void gHostCtlEvent(int idx);
void gHostLogCall(int id);
void gHostResetLog(void);
void gHostDeviceCal(void);

/* The copy table the module declares `extern`; the mock owns the storage and
 * the driver fills the fields before every case, so src/dst/size are inputs
 * rather than constants. Linkage matches the device header exactly (non-volatile
 * on both sides) so the declaration and this definition do not fight. */
extern COPY_TABLE prginRAM;

/* The five interrupt entry points mainInit_2803x.c takes the address of. They
 * are shared (defined by the mock, not renamed), so both sides write the SAME
 * pointer value into PieVectTable and the table compares directly. */
extern void ADC_Over_isr(void);
extern void EPWM1_TZ_isr(void);
extern void EPWM1_zero_isr(void);
extern void SCI_RXD_isr(void);
extern void SCI_TXD_isr(void);

/* Callees of InitPeripherals / copy_prg. */
extern void InitSetPWM(void);
extern void InitCpuTimers(void);
extern void MovePrgFrFlashToRam(unsigned long src, unsigned long dst,
                                unsigned long size);

/* NOTE: there is deliberately no StartCpuTimer1 here. The real
 * DSP2803x_CpuTimers.h defines `#define StartCpuTimer1() CpuTimer1Regs.TCR.bit.TSS = 0`,
 * so `InitPeripherals()`'s call is a REGISTER WRITE, not a call -- and the write
 * lands in CpuTimer1Regs, which the driver snapshots. Stubbing it would hide
 * that write behind a mock counter and measure the wrong thing. */

#endif
