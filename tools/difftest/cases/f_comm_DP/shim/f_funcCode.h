/* Host-side replacement for f_funcCode.h -- f_comm_DP case private.
 *
 * The real f_funcCode.h is a hub: it pulls DSP28x_Project.h, f_debug.h,
 * f_interface.h and f_p2p.h, and those reach most of the project.  Forwarding
 * it would drag in the whole tree for a module that needs almost nothing from
 * it.
 *
 * What f_comm_DP.c actually takes from this header is exactly two things, and
 * neither of them is a type of its own:
 *
 *   1. `Uint16` and the peripheral register model.  The module's own header
 *      (f_comm_DP.h, which IS forwarded -- see host_patch.json) declares
 *      `volatile struct SCI_REGS *pSciRegs;` and `Uint16` fields, and it
 *      reaches both only through this include.  So the chip header is included
 *      here, and the module's storage widths come from the real struct.
 *
 *   2. `funcCode`.  The module touches `funcCode.code.dpDataBuffer[1..3]` in
 *      three statements -- all of them inside `#if BUG_SCI_BACK_DATA`, which
 *      f_comm_DP.h defines as 0.  The preprocessor drops them, so no
 *      declaration is needed and none is invented: a stand-in `funcCode` here
 *      would be a fake the module never reads, and worse, it would make the
 *      bench look like it covered a branch it does not.
 *
 * Nothing else.  If the module ever grows a live dependency on this header,
 * this stub will fail to compile rather than quietly test the wrong thing.
 */
#ifndef F_FUNCCODE_H
#define F_FUNCCODE_H

#include "DSP2803x_Device.h"

/* CrcValueByteCalc is declared in include/common/f_common.h line 6, which the
 * real header chain reaches (f_funcCode.h -> ... -> f_common.h).  The prototype
 * is repeated here with the SAME signature, because without it the module's
 * three call sites would be implicitly declared as returning `int` (MSVC C4013)
 * and the host build would no longer match the target build's type information.
 * The definition lives in cases/f_comm_DP/mock_commdp.c. */
Uint16 CrcValueByteCalc(const Uint16 *data, Uint16 length);

#endif
