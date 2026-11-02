/* Shared mock for the f_debug case.
 *
 * These four objects are the module's only external data.  They are deliberately
 * NOT renamed per side: f_debug.c declares them `extern` and ref_tu.c/new_tu.c
 * only rename symbols the module DEFINES, so both translation units bind to the
 * single copy below.  That is exactly right for a differential test -- the two
 * sides must read the same input, otherwise a difference could come from the
 * input rather than from the code.
 *
 *   runFlag  -- read three times per UniversalDebug call, through the macro
 *               SCOPE_START_RUN_FLAG == (runFlag.bit.run).  The union lives in
 *               cases/f_debug/shim/f_runSrc.h.
 *
 *   frq, frqRun, frqTmp
 *            -- the first three entries of the module's matlabDebug[] pointer
 *               table, cast to Uint16* by the module itself:
 *                   (Uint16 *)&frq
 *               so the module reads only the LOW 16-bit word of a 32-bit object.
 *               On the C28x a `long` is two words stored low word first; on the
 *               host an int32 is little-endian, so the low word is also at
 *               offset 0.  The bench does not take that on faith: the driver's
 *               bench_selfcheck() writes 0x12345678 and asserts that
 *               *matlabDebug[0] reads back 0x5678.  If the premise were ever
 *               wrong, the check fires before any comparison runs.
 *
 * The other three matlabDebug[] entries point at `matlabDebugRsvd`, which the
 * module DEFINES, so those are renamed per side (ref_/new_) and do not appear
 * here.
 */
#include "f_runSrc.h"

union RUN_FLAG runFlag;

int32 frq;
int32 frqRun;
int32 frqTmp;
