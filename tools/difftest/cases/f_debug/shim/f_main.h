/* Host-side replacement for f_main.h -- f_debug case private.
 *
 * The real f_main.h includes f_funcCode.h, f_error.h, f_common.h and
 * f_canlink.h, then declares the main-loop state (mainCmd, runStatus,
 * invtStatus, errorOther, ...).  f_debug.c's live code reads none of it:
 *
 *   - `mainCmd` / `invtStatus` / `runStatus*`  -- not referenced at all;
 *   - the only "main" concept the module touches is `runFlag`, and that lives
 *     in f_runSrc.h, not here.
 *
 * An empty translation-safe stub is therefore faithful.  It is kept as a file
 * rather than deleted because f_debug.c names it in an #include, and the case
 * shim directory has to answer for every header on that list -- otherwise the
 * real header would be reached through include/ and drag the tree back in.
 */
#ifndef F_MAIN_SHIM_H
#define F_MAIN_SHIM_H

#include "DSP28x_Project.h"

#endif
