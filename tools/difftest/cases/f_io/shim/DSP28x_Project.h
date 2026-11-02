/* Host-side replacement for DSP28x_Project.h -- f_io case private.
 *
 * f_funcCode.h includes this on its first line, before anything else in the
 * project is parsed, so this is where the C28x integer widths get pinned and
 * the (host-safe) device model gets pulled in.
 *
 * The real DSP28x_Project.h is two halves:
 *   - DSP2803x_Device.h  (type widths, BITn, the peripheral register structs,
 *                         and the C28x-only `cregister` / `asm(...)` macros)
 *   - DSP2803x_Examples.h (DSP_CLOCK, and a pile of `__interrupt`/`asm()`
 *                         examples this module never needs)
 * Both are unreachable on the host, so a case-private device header replaces
 * the first and DSP_CLOCK is set here; the examples half is dropped. The
 * peripheral register structs stay the chip's own (see DSP2803x_Device.h) --
 * f_io.c really does write six of them.
 *
 * DSP_CLOCK == 60 is the real board clock (the same value DSP2803x_Examples.h
 * defines). f_debug.h keys TIME_10US/TIME_1MS &c. off it, and an undefined
 * DSP_CLOCK would leave those macros undefined and silently turn every
 * comparison in the module false.
 */
#ifndef DSP28X_PROJECT_H
#define DSP28X_PROJECT_H

#include "DataTypeDef.h"

#define DSP_CLOCK   60

#include "DSP2803x_Device.h"

#endif
