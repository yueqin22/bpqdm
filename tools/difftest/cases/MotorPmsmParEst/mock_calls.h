/* Shared call ledger for the MotorPmsmParEst differential test.
 *
 * The module's object file has eight undefined FUNCTION symbols (read out of
 * build/Debug/MotorPmsmParEst.obj with coff_cmp, not counted by eye):
 *
 *     GetUvwPhase   IPMCalAcrPIDCoff   InitSetPWM   ResetParForVC
 *     SynCalLdAndLq(Uint)   JudgeABDir   JudgeUVWDir   JudgeRTDir
 *
 * (the ninth undefined function, `atan`, is a math helper supplied by
 * tools/difftest/atan_mock.c, which reaches `qatan` in hostlib_mock.c; it is
 * not a module-level callee and is not counted here -- the same rule
 * MotorInvProtect applies to `abs`).
 *
 * Every callee lives in another translation unit and is not under test. A
 * rewrite that drops, duplicates or moves a call would otherwise pass silently
 * whenever the dropped callee happened to leave no shared state behind, so each
 * call bumps its own slot. `unsigned short` is used for the counters because it
 * is 16 bits on the host exactly as Uint16 is on the C28x; the ledger is
 * deliberately independent of the module's include chain.
 *
 * The three Judge* callees also *return* the direction, so their slots are
 * bumped inside their (separately defined, value-returning) stubs; the five
 * void callees are generated from the same list.
 */
#ifndef MOCK_CALLS_H
#define MOCK_CALLS_H

#define MOCK_CALL_LIST(X)        \
    X(GetUvwPhase, (void))       \
    X(IPMCalAcrPIDCoff, (void))  \
    X(InitSetPWM, (void))        \
    X(ResetParForVC, (void))     \
    X(SynCalLdAndLq, (Uint m_Pos)) \
    X(JudgeABDir, (void))        \
    X(JudgeUVWDir, (void))       \
    X(JudgeRTDir, (void))

/* MC_<callee> names the counter slot; MC_COUNT is the array length. */
#define MOCK_CALL_ENUM(name, params) MC_##name,
enum { MOCK_CALL_LIST(MOCK_CALL_ENUM) MC_COUNT };
#undef MOCK_CALL_ENUM

extern unsigned short gMockCall[MC_COUNT];

/* The direction the three Judge* stubs return. The real values are computed
 * from the encoder / UVW / resolver signals by MotorEncoder.c; since that
 * module is not under test, the direction is promoted to a *driver input*: the
 * driver pokes these three globals so every DIR_ERROR / DIR_FORWARD /
 * DIR_BACKWARD combination becomes reachable directly (strictly more coverage
 * than driving them through the real bodies could be). */
extern unsigned short gMockAbDir;
extern unsigned short gMockUvwDir;
extern unsigned short gMockRtDir;

#endif
