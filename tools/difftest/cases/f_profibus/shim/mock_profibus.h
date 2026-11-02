/* Interface of the f_profibus differential mock.
 *
 * mock_profibus.c supplies everything the module reads from outside itself: 21
 * external data objects and 2 external functions.  See that file for why each
 * object is defined here with its real type.
 *
 * ---- the call trace ----
 *
 * f_profibus.c reaches outside itself through exactly two calls, both from the
 * PZD loop at the bottom of ProfibusSendDataDeal:
 *
 *     if (ModifyFunccodeUpDown(indexWrite, &pzdSaveData, 0) == COMM_ERR_NONE)
 *         ModifyFunccodeEnter(indexWrite, pzdSaveData);
 *
 * Neither call moves a global the driver can see, and the `indexWrite` /
 * `indexRead` they are built from are *locals* -- `indexWrite` exists nowhere
 * else, and `indexRead` survives only as two bytes pushed into sendFrame.  So
 * the calls themselves have to be part of the observation set, and the trace is
 * the only place the computed index is visible at all.
 *
 * One entry per call, five words: which, then four arguments.
 *
 *   ModifyFunccodeUpDown  (index, value_on_entry, delta, return_value)
 *   ModifyFunccodeEnter   (index, value)
 *
 * `value_on_entry` is read *before* the stub perturbs *data (see
 * mock_profibus.c), so the pair (UpDown.value_on_entry, Enter.value) is what
 * proves the module forwarded the post-call value rather than a stale one.
 *
 * A single ProfibusSendDataDeal call can reach the stubs 2 * pzdNum times, and
 * pzdNum is derived from the frame length the driver supplies.  A fixed array
 * would either overflow silently or need a size only one call sequence happens
 * to fit, so the driver compares three things:
 *
 *   * protTraceN    -- total number of stub calls, unbounded;
 *   * protTraceHash -- FNV-1a over every entry, in order;
 *   * protTrace[]   -- the first PROT_N entries, for diagnostics only.
 *
 * Comparing only the terminal state would miss "a call was deleted" or "two
 * calls swapped", which is exactly what a trace is for.
 */
#ifndef MOCK_PROFIBUS_H
#define MOCK_PROFIBUS_H

#include "DataTypeDef.h"

/* Frame buffers.
 *
 * The target declares `LOCALF Uint16 sendFrame[35]` / `rcvFrame[35]`
 * (src/function/f_comm.c:27-28) and the module's own reachable frame lengths are
 * at most 36 words (PPO5: DPDataNum[3] + 4).  That makes
 * `rcvFrame[commRcvData.rcvNumMax - 1]` in ProfibusRcvDataDeal read rcvFrame[35]
 * -- one word past the end of the real array -- for PPO5.  See
 * cases/f_profibus/README.md section 7(a): the original firmware really does
 * read out of bounds there.
 *
 * FRAME_N is 40 rather than 35 so that index 35 (and the tail the PZD loop
 * reaches) lands inside the mock instead of in whatever .bss happens to follow.
 * Both sides share this one definition, so the widening is symmetric and the
 * comparison stays valid; what it buys is that the PPO5 chain is *defined*
 * rather than reading undefined host memory and calling the result a pass.
 * The module never uses sizeof on these arrays, so the widening cannot change
 * what it does. */
#define FRAME_N 40

/* Trace capacity.  A single call logs at most 2 * pzdNum <= 2 * 13 entries; 32
 * is comfortably above that, and anything beyond it still lands in the count
 * and the hash. */
#define PROT_N 32

/* `which` values. */
#define PROT_UPDOWN 1
#define PROT_ENTER  2

struct PROT_ENT
{
    Uint16 which;
    Uint16 a0, a1, a2, a3;
};

extern struct PROT_ENT protTrace[PROT_N];
extern Uint32 protTraceN;
extern Uint32 protTraceHash;

/* Return-value knob: the answer of ModifyFunccodeUpDown.  The module branches
 * on it (`== COMM_ERR_NONE`), so it selects whether ModifyFunccodeEnter runs at
 * all -- a constant would leave half the loop unreachable. */
extern Uint16 mockUpDownRc;

/* Zero the trace, the hash and the call counters.  The driver calls this before
 * each side runs so the two sides start from the same place. */
void mockReset(void);

#endif
