/* Host-side replacement for DSP28x_Project.h.

 * f_funcCode.h includes it on its line 5 -- before anything else in the project
 * is parsed -- so this is the natural place to pin the integer widths. The
 * project's DataTypeDef.h maps Uint16 to `unsigned int`: 16 bits on C28x, 32 on
 * the host. Reusing it would silently widen every u16 and move every overflow
 * and truncation point, which is exactly what the differential test is supposed
 * to be checking. Pinning here means every header parsed afterwards sees the
 * C28x widths.
 *
 * This matters more than usual for f_frqSrc_pid: PID_FUNC holds `int64
 * deltaIRem`, and PidFuncCalc does `(int64)Kp * (...)`, `>> p->Qp` and
 * `- (deltaP << p->Qp)` on it. If int64 were 32 bits on the host the
 * accumulator would wrap at a different point than on the target.
 *
 * The real DSP28x_Project.h pulls in the whole peripheral model
 * (DSP2803x_Device.h and the DSP2803x_*.h register files), which the host has no
 * use for. Nothing in this module reads a peripheral register, so the
 * peripheral half is dropped -- but the *type* half is kept, because project
 * headers reach for int64/Uint64/float32 as well (f_frqSrc.h uses int64), and
 * on the target those come from DSP2803x_Device.h.
 *
 * The typedef block mirrors include/device/DSP2803x_Device.h:74-84, which is
 * guarded by DSP28_DATA_TYPES; the same guard is used here so that including
 * both is harmless.
 */
#ifndef DSP28X_PROJECT_H
#define DSP28X_PROJECT_H

#include "DataTypeDef.h"

/* The real DSP28x_Project.h also pulls DSP2803x_Examples.h, whose line 9 is
 * `#define DSP_CLOCK 60`. f_debug.h keys TIME_10US/TIME_1MS &c. off DSP_CLOCK,
 * and an undefined DSP_CLOCK makes those comparisons silently false, leaving
 * the macros undefined. The project headers below use them, so the value has to
 * be present. 60 MHz is the real board clock, not a guess. */
#define DSP_CLOCK   60

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

/* The device header's BITn defines; project headers use them freely. */
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

#endif
