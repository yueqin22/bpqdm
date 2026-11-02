/* Interface of the f_frqSrc_pid differential mock.
 *
 * mock_frqsrcpid.c supplies everything the module reads from outside itself:
 * the sixteen external data objects and the four external functions.  See that
 * file for why the data is declared with its real type and why three of the
 * four functions are faithful copies rather than fixed-return stubs.
 *
 * On top of that it exposes a call trace and a guard counter to the driver.
 *
 * ---- why a hash and not just an array ----
 *
 * A single FrqPidSetDeal() call can reach the stub set six times (LineChangeCalc
 * once and LpfCalc once from UpdatePidFuncRefFdb, then up to three LineCalc
 * calls and one more LpfCalc from PidFuncCalc).  A fixed array would either
 * overflow silently or need a size that only a particular call sequence
 * happens to fit.  So the driver compares three things:
 *
 *   * pidTraceN    -- total number of stub calls, unbounded;
 *   * pidTraceHash -- FNV-1a over every (which, arg) pair, in order;
 *   * pidTrace[]   -- the first PIDTRACE_N pairs, for diagnostics only.
 *
 * Comparing only the terminal state would miss "a stub call was deleted" or
 * "two stub calls swapped", which is exactly what a trace is for.
 *
 * ---- why a guard counter ----
 *
 * LineCalc divides by (x2 - x1) with no guard.  On C28x an integer divide by
 * zero yields a defined value and no trap; on the host it raises SIGFPE and
 * aborts the process.  If a negative-control mutation ever made the two sides
 * disagree about x1/x2 such that the denominator hit zero, the run would abort
 * instead of reporting a mismatch -- the mutation would still be caught, but
 * the evidence would be an exit code rather than a field diff.
 *
 * The mock therefore keeps the real arithmetic and adds a guard that counts how
 * many times it had to skip the division.  The driver resets it per side and
 * compares it like any other observable, so the same mutation surfaces as a
 * clean mismatch.  It is a harness robustness measure, not a behaviour change:
 * on every input the baseline actually produces, the counter stays 0 and the
 * arithmetic is bit-for-bit the real thing.
 */
#ifndef MOCK_FRQSRCPID_H
#define MOCK_FRQSRCPID_H

#include "DataTypeDef.h"

#define PIDTRACE_N 16

#define PIDTRACE_LINECALC    0
#define PIDTRACE_LPFCALC     1
#define PIDTRACE_LINECHANGE  2
#define PIDTRACE_MULTISET    3

struct PIDTRACE
{
    Uint16 which;
    Uint32 arg;
};

extern struct PIDTRACE pidTrace[PIDTRACE_N];
extern Uint32 pidTraceN;
extern Uint32 pidTraceHash;
extern Uint32 pidGuardN;

/* Return value of the UpdateMultiSetFrq stub.  It is the only external function
 * whose *result* the module consumes without also handing it a struct to fill,
 * so it needs a driver-set value; the other three write their answer into the
 * caller's struct. */
extern int32  mockMultiSetFrq;

#endif
