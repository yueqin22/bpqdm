/* Host-side replacement for f_runSrc.h -- f_debug case private.
 *
 * f_debug.c takes exactly one thing from f_runSrc.h: `runFlag`, via
 *
 *     #define SCOPE_START_RUN_FLAG  (runFlag.bit.run)
 *
 * The real f_runSrc.h includes f_funcCode.h (the whole header tree) and then
 * declares ~40 more unions (INVT_STATUS, RUN_STATUS_FIRST_STEP, DSP_MAIN_COMMAND
 * ...) plus a large pile of externs, none of which this module reads.  So the
 * RUN_FLAG definition below is the only part reproduced.
 *
 * RUN_FLAG_BITS is copied VERBATIM from include/function/f_runSrc.h lines 61-90,
 * not trimmed to the single `run` field.  Two reasons:
 *
 *   1. the bit allocation of `run` depends on the fields that precede it, so a
 *      trimmed struct would only be correct by accident;
 *   2. negative_control mutates the module's source, and a mutation that swaps
 *      `.bit.run` for `.bit.jog` has to COMPILE to be a meaningful test.  With
 *      the full struct it compiles and gets caught; with a trimmed one it would
 *      come out BROKEN, which is not evidence of anything.
 *
 * Bit position is measured, not assumed.  The driver's bench_selfcheck() sets
 * `runFlag.all = 0; runFlag.bit.run = 1;` and records which single bit lit up,
 * then drives `runFlag.all` through that bit for the rest of the run.  The C28x
 * and MSVC need not agree on whether bit-fields are allocated from the LSB or
 * the MSB -- and it does not matter here, because the module only ever tests
 * `runFlag.bit.run` for zero/non-zero, so all that has to be faithful is the
 * mapping from "the driver's notion of running" to that one bit.
 */
#ifndef F_RUNSRC_SHIM_H
#define F_RUNSRC_SHIM_H

#include "DSP28x_Project.h"

struct RUN_FLAG_BITS
{
    Uint16 run:1;

    Uint16 common:1;
    Uint16 jog:1;
    Uint16 tune:1;

    Uint16 jogWhenRun:1;

    Uint16 accDecStatus:2;

    Uint16 plc:1;
    Uint16 pid:1;
    Uint16 torque:1;

    Uint16 dir:1;
    Uint16 curDir:1;
    Uint16 dirReversing:1;
    Uint16 dirFinal:1;

    Uint16 servo:1;
    Uint16 rsvd:1;
};

union RUN_FLAG
{
    Uint16               all;
    struct RUN_FLAG_BITS bit;
};

extern union RUN_FLAG runFlag;

#endif
