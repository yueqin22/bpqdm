/* Shared call ledger for the MotorPmsmMain differential test.
 *
 * The module owns five data objects, so most of its output IS comparable by
 * value.  But four of its ten entry points only ever express their answer by
 * CALLING another module with a computed argument:
 *
 *     PG_Zero_isr        -> SetIPMPos(gIPMPos.RotorZero)
 *     IPMCheckInitPos    -> SetIPMPos(uvwPos) / SetIPMPos_ABZRef(uvwPos)
 *     RunCaseIpmInitPos  -> SynInitPosDetSetPwm(6)
 *     RunCaseIpmInitPos  -> DisableDrive() / EnableDrive()   (macros, GPACLR/SET)
 *
 * A stub that ignored the argument would make "the module computed the wrong
 * position" completely invisible: the callee is a no-op on both sides, so the
 * state comparison would agree.  Hence two slots per callee -- a count and a
 * last-argument word -- and the driver compares both.
 *
 * The list is the module object file's undefined FUNCTION set read out with
 * `nm2000 build/Debug/MotorPmsmMain.obj`: 9 entries (qsin / qsqrt come from the
 * shared hostlib_mock.c and are not part of this ledger).
 *
 * `unsigned short` is used for the counters because it is 16 bits on the host
 * exactly as Uint16 is on the C28x.  The argument words are `unsigned long`
 * (32 bits host, 32 bits target) because SetIPMPos takes a Uint that the module
 * may hand a 16-bit wrap of a wider expression.
 */
#ifndef MOCK_CALLS_H
#define MOCK_CALLS_H

enum MOCK_CALL_SLOT {
    MC_GETUVWPHASE = 0,
    MC_INITSETADC,
    MC_INITSETPWM,
    MC_SETINTENABLE,
    MC_PREPPARFORRUN,
    MC_TURNTOSTOP,
    MC_SYNINITPOS,
    MC_SETIPMPOS,
    MC_SETIPMPOSABZ,
    MC_COUNT
};

#endif
