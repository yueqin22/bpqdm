/* Call ledger slots for the MotorParChange bench.
 *
 * Every callee the module has is a stub in mock_mpc.c; it bumps one of these
 * counters and records its argument. "Did the rewrite still call it, how many
 * times, with what argument" is therefore comparable.
 */

#ifndef MOCK_CALLS_H
#define MOCK_CALLS_H

enum MOCK_CALL_SLOT {
    MC_SETIPMPOS = 0,
    MC_SETIPMPOSABZ,
    MC_REINITFORPG,
    MC_INITSETQEP,
    MC_PGZEROISR,
    MC_COUNT
};

#endif
