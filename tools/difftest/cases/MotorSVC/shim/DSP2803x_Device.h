/* Host-side replacement for DSP2803x_Device.h -- MotorSVC case private.
 *
 * The real DSP2803x_Device.h cannot be forwarded:
 *   - `extern cregister volatile unsigned int IFR;` -- `cregister` is a C28x
 *     keyword MSVC does not know;
 *   - EINT / DINT / ERTM / DRTM / EALLOW / EDIS / ESTOP0 are `asm(" ...")`
 *     macros.
 *
 * MotorSVC really executes DINT/EINT: SVCCalRotorSpeed() brackets the
 * DetaPhase snapshot with them (source lines 193/198).  Blanking them out
 * would make "the rewrite dropped the critical section" invisible, so they are
 * turned into increments of a shared counter table instead -- the presence and
 * order of the CPU-control sequence become observable state.  As with the
 * register mock this does not *execute* the instruction, it records that it
 * appeared; target-side semantics stay out of the host test's reach.
 *
 * The peripheral headers are pulled in verbatim (pure struct + extern
 * declarations, MSVC accepts them).  MotorSVC touches no peripheral register
 * directly -- its only hardware-read is GetTime(), i.e.
 * CpuTimer1.RegsAddr->TIM.all, which the case mock instantiates as real
 * CPUTIMER_REGS memory (cases/MotorSVC/mock_svc.c).  The peripheral structs are
 * still needed because the real project headers referenced below declare them.
 *
 * Type widths must be pinned: the real header makes Uint16 `unsigned int`
 * (16 bits on the C28x, 32 on the host).
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

/* ---------------------------------------------------------------------------
 * `__IQsat` is a C28x compiler builtin (nothing in include/ or src/ defines it,
 * yet the target build is warning-free).  Measured semantics:
 *     __IQsat(A, Pos, Neg) == min(max(A, Neg), Pos), evaluated on the 32-bit
 * signed ACC.
 * MotorMain has exactly one call site:
 *     SendDataPrepare: mRatio = __IQsat(gRatio, 4096, 0);   (Uint operand)
 * A static function is used rather than a macro because a macro would evaluate
 * the argument twice while the builtin evaluates it once.
 * ------------------------------------------------------------------------- */
static long __IQsat(long A, long Pos, long Neg)
{
    if (A > Pos)
    {
        return Pos;
    }
    if (A < Neg)
    {
        return Neg;
    }
    return A;
}

/* CPU control sequence -> shared counter table, one slot per real macro. */
extern volatile Uint16 gHostCpuCtl[7];

#define  EINT    (gHostCpuCtl[0]++)
#define  DINT    (gHostCpuCtl[1]++)
#define  ERTM    (gHostCpuCtl[2]++)
#define  DRTM    (gHostCpuCtl[3]++)
#define  EALLOW  (gHostCpuCtl[4]++)
#define  EDIS    (gHostCpuCtl[5]++)
#define  ESTOP0  (gHostCpuCtl[6]++)

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

/* ---------------------------------------------------------------------------
 * Fold `int` down to the target's 16 bits -- the same discipline SubPrg /
 * MotorInvProtect / MotorEncoder use.
 *
 * `MotorDefine.h` (line 13) is `typedef unsigned int Uint;`.  On the C28x that
 * is a 16-bit storage unit, on the host 32 bits.  It is not a promotion-rule
 * difference (that one cannot be removed, README 3.2) but a storage width, the
 * class README 3.1 says must be pinned.  MotorSVC truncates into 16-bit
 * storage in several places that matter:
 *     int  m_DetaTimer;  m_DetaTimer = (((Timer - GetTime()) & 0xFFFFFF)>>3);
 *                        (SVCCalFlux_380 -- the mask is 24 bits, the destination
 *                         is 16, so the fold decides what survives)
 *     gSVCSpeed.DetaPhase = gFluxR.Theta - gSVCSpeed.SvcLastFluxPos;
 *                        (Uint - int difference: wraps at 16 bits, not 32)
 *     (Uint)((long)gPhase.IMPhase >> 16)   (ResetSVC -> gFluxR.Theta)
 * Without the fold those assignments keep 32 bits on the host, so a mutation
 * whose only visible effect is the 16-bit truncation would hide.
 *
 * Cost: SubPrgInclude.h's `long int` prototypes become `long short` (MSVC
 * C2632), which host_patch.json repairs without changing the width.
 *
 * The real peripheral headers above are included *before* this point, so their
 * widths are not affected.
 * ------------------------------------------------------------------------- */
#define int   short

#endif
