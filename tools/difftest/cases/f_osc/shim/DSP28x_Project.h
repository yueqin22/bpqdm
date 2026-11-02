/* Host-side replacement for DSP28x_Project.h -- f_osc case private.
 *
 * f_funcCode.h includes it on its line 5 -- before anything else in the
 * project is parsed -- so this is the natural place to pin the integer widths.
 * The project's DataTypeDef.h maps Uint16 to `unsigned int`: 16 bits on C28x,
 * 32 on the host.  Reusing it would silently widen every u16 and move every
 * overflow and truncation point, which is exactly what the differential test
 * is supposed to be checking.  Pinning here means every header parsed
 * afterwards sees the C28x widths.
 *
 * Unlike the f_runSrc_accDecFrq case, this module DOES touch peripheral
 * registers (SCI / PIE / GPIO), so the peripheral half is not dropped: it is
 * pulled in through the case-private DSP2803x_Device.h, which keeps the real
 * chip register structs but replaces the three host-hostile constructs
 * (`cregister`, `asm()`, `interrupt`).  See that file for the reasoning.
 *
 * Note the two headers that must NEVER be forwarded to their real
 * counterparts -- run_case.py puts gen_dir (the real-passthrough layer) at the
 * FRONT of the include path, so forwarding either one would silently delete
 * this whole width pin:
 *
 *     DSP28x_Project.h     (this file)
 *     DSP2803x_Device.h    (the case shim)
 *
 * real.txt therefore uses `*`, which only expands over the PUBLIC shim
 * directory -- neither of the two lives there.
 */
#ifndef DSP28X_PROJECT_H
#define DSP28X_PROJECT_H

#include "DataTypeDef.h"

/* The real DSP28x_Project.h also pulls DSP2803x_Examples.h, whose line 9 is
 * `#define DSP_CLOCK 60`.  f_debug.h keys TIME_10US/TIME_1MS &c. off DSP_CLOCK,
 * and an undefined DSP_CLOCK makes those comparisons silently false, leaving
 * the macros undefined.  The project headers below use them, so the value has
 * to be present.  60 MHz is the real board clock, not a guess. */
#define DSP_CLOCK   60

#include "DSP2803x_Device.h"

#endif
