/* Host-side replacement for DSP2803x_Device.h -- f_comm_DP case private.
 *
 * The real DSP2803x_Device.h cannot be forwarded, for three reasons:
 *   - `extern cregister volatile unsigned int IER;` -- `cregister` is a C28x
 *     keyword MSVC does not know, and `IER` is a real operand of this module
 *     (`IER |= M_INT9;` in InitSciaGpioDp / InitScibGpioDp);
 *   - EINT / DINT / ERTM / DRTM / EALLOW / EDIS / ESTOP0 are `asm(" ...")`
 *     macros;
 *   - `interrupt` is a C28x storage-class keyword and this module's two entry
 *     points are declared `interrupt void SCI_DP_RXD_isr(void)`.
 *
 * Each of the three is answered explicitly rather than papered over:
 *
 *   IER          a plain `extern volatile Uint16`, instantiated by the case
 *                mock.  It is compared like any other output, so "the rewrite
 *                stopped enabling the PIE group" is a visible difference.
 *   EALLOW/EDIS  turned into increments of a shared counter table.  Blanking
 *                them out would make "the rewrite moved a register write
 *                outside the EALLOW window" invisible; counting them makes the
 *                presence and the count of the CPU-control sequence observable
 *                state.  This does not *execute* the instruction -- it records
 *                that it appeared, and target-side semantics stay out of the
 *                host test's reach.
 *   interrupt    `#define interrupt` empty, exactly as the mainInit_2803x case
 *                does.  `typedef interrupt void(*PINT)(void);` in the real
 *                DSP2803x_PieVect.h then folds to `typedef void(*PINT)(void);`,
 *                which is the type PieVectTable.SCIRXINTA actually has once the
 *                keyword is dropped.  No host_patch entry is needed for
 *                DSP2803x_PieVect.h because of this.
 *
 * The peripheral headers are pulled in verbatim (pure struct + extern
 * declarations, MSVC accepts them).  This module drives real peripheral memory:
 * InitSetSciDp writes nine SCI registers, UpdateSciDpFormat reads SCIRXST and
 * rewrites SCICCR, InitSciaGpioDp writes four GpioCtrlRegs fields plus two PIE
 * enables and two vector slots.  The SCI_REGS / GPIO_CTRL_REGS bitfield layouts
 * therefore have to come from the real headers, not from hand-written
 * stand-ins -- otherwise the diff would compare two layouts that are both
 * wrong.  The case mock instantiates real register objects.
 *
 * No `#define int short` fold here, unlike MotorPWM / MotorSVC.  This module
 * contains no bare `int`, `Uint`, `long` or `char` at all (checked: every
 * declaration is `Uint16` / `int16` / `const Uint16`), so there is no storage
 * width to pin and no promotion to freeze.  The real peripheral headers are
 * included before this point anyway, so their widths are never affected.
 */
#ifndef DSP2803x_DEVICE_H
#define DSP2803x_DEVICE_H

#include <stdint.h>

#ifndef DSP28_DATA_TYPES
#define DSP28_DATA_TYPES
typedef int16_t             int16;
typedef int32_t             int32;
typedef int64_t             int64;
typedef uint16_t            Uint16;
typedef uint32_t            Uint32;
typedef uint64_t            Uint64;
typedef float               float32;
typedef long double         float64;
#endif

#define   TARGET   1

#define   DSP28_28030PAG   0
#define   DSP28_28030PN    0
#define   DSP28_28031PAG   0
#define   DSP28_28031PN    0
#define   DSP28_28032PAG   0
#define   DSP28_28032PN    0
#define   DSP28_28033PAG   0
#define   DSP28_28033PN    0
#define   DSP28_28034PAG   0
#define   DSP28_28034PN    0
#define   DSP28_28035PAG   0
#define   DSP28_28035PN    TARGET

#define BIT0    0x0001
#define BIT1    0x0002
#define BIT2    0x0004
#define BIT3    0x0008
#define BIT4    0x0010
#define BIT5    0x0020
#define BIT6    0x0040
#define BIT7    0x0080
#define BIT8    0x0100
#define BIT9    0x0200
#define BIT10   0x0400
#define BIT11   0x0800
#define BIT12   0x1000
#define BIT13   0x2000
#define BIT14   0x4000
#define BIT15   0x8000

/* Must precede the chip headers: DSP2803x_PieVect.h line 9 is
 * `typedef interrupt void(*PINT)(void);`. */
#define interrupt

#include "DSP2803x_Adc.h"
#include "DSP2803x_BootVars.h"
#include "DSP2803x_Cla.h"
#include "DSP2803x_Comp.h"
#include "DSP2803x_CpuTimers.h"
#include "DSP2803x_DevEmu.h"
#include "DSP2803x_ECan.h"
#include "DSP2803x_ECap.h"
#include "DSP2803x_EPwm.h"
#include "DSP2803x_EQep.h"
#include "DSP2803x_Gpio.h"
#include "DSP2803x_I2c.h"
#include "DSP2803x_Lin.h"
#include "DSP2803x_NmiIntrupt.h"
#include "DSP2803x_PieCtrl.h"
#include "DSP2803x_PieVect.h"
#include "DSP2803x_Spi.h"
#include "DSP2803x_Sci.h"
#include "DSP2803x_SysCtrl.h"
#include "DSP2803x_XIntrupt.h"

/* CPU control sequence -> shared counter table, one slot per real macro. */
extern volatile Uint16 gHostCpuCtl[7];

#define  EINT    (gHostCpuCtl[0]++)
#define  DINT    (gHostCpuCtl[1]++)
#define  ERTM    (gHostCpuCtl[2]++)
#define  DRTM    (gHostCpuCtl[3]++)
#define  EALLOW  (gHostCpuCtl[4]++)
#define  EDIS    (gHostCpuCtl[5]++)
#define  ESTOP0  (gHostCpuCtl[6]++)

/* `extern cregister volatile unsigned int IER;` -- the cregister part is
 * C28x-only, the storage itself is ordinary memory as far as this test is
 * concerned.  M_INT9 keeps the real value from the real device header. */
extern volatile Uint16 IER;
#define M_INT9  0x0100

/* Aligned with the real `#if (DSP28_28035PN||...)` branch (this board is
 * 28035PN, so that branch is the live one).  Nothing in src/ reads these; they
 * are kept so the shim is the same shape as the real device header. */
#define DSP28_COMP1 1
#define DSP28_COMP2 1
#define DSP28_COMP3 1
#define DSP28_EPWM1 1
#define DSP28_EPWM2 1
#define DSP28_EPWM3 1
#define DSP28_EPWM4 1
#define DSP28_EPWM5 1
#define DSP28_EPWM6 1
#define DSP28_EPWM7 1
#define DSP28_ECAP1 1
#define DSP28_EQEP1 1
#define DSP28_ECANA 1
#define DSP28_SPIA  1
#define DSP28_SPIB  1
#define DSP28_SCIA  1
#define DSP28_I2CA  1
#define DSP28_LINA  1

#endif
