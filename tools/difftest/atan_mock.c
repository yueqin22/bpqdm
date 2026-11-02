/* Host mock for SubPrg.c's atan().
 *
 * Only cases whose module-under-test is NOT SubPrg.c link this file, because
 * SubPrg.c defines `atan` itself and a second definition would collide at link
 * time.  Keep it a separate file (rather than a #if inside hostlib_mock.c) so
 * that mocks.txt alone decides which one is in play.
 *
 * Ported in behaviour from SubPrg.c:
 *
 *     if (x == 0) return (y < 0) ? -16384 : 16384;
 *     m_Input = ((long)y << 16) / x;
 *     result  = qatan(m_Input);
 *     if (x < 0) result += 32768;
 *
 * The only difference is that `qatan` comes from hostlib_mock.c and is an
 * approximation of the assembly polynomial (see the long note in
 * hostlib_mock.c).  Both differential-test sides use the same one.
 *
 * ---------------------------------------------------------------------------
 * ONE DELIBERATE DEVIATION -- the 64-bit intermediate
 * ---------------------------------------------------------------------------
 * `((long)y << 16) / x` overflows 32-bit signed arithmetic for exactly one
 * input pair: x == -1 with y == -32768, because -32768 << 16 == INT32_MIN and
 * INT32_MIN / -1 is not representable.  This UB exists in the original
 * firmware as well, but the two sides do not agree on what happens next:
 *   - C28x: the 32-bit divide helper wraps silently and returns some value;
 *   - MSVC x64: `idiv` raises #DE and the process dies (measured: the bench
 *     aborted at exactly M=-1, T=-32768 before this fix).
 * Because the corner is UB in the module under test, there is no "correct"
 * value to preserve, and no differential claim can be made about it.  The
 * intermediate is therefore computed in 64 bits here so the bench stays alive
 * and deterministic.  Consequence, stated plainly: for that single input pair
 * the number this mock returns is invented by the bench, NOT taken from the
 * target.  Do not cite any result from this region as evidence about the
 * firmware.  Everything outside that pair is bit-identical to the 32-bit
 * expression.
 */

#include "MotorInclude.h"

extern int qatan(long);

int atan(int x, int y)
{
    int    result;
    long long m_Input;

    if (x == 0) {
        if (y < 0) {
            return (-16384);
        } else {
            return (16384);
        }
    }
    m_Input = (((long long)y) << 16) / x;
    result  = qatan((long)m_Input);
    if (x < 0) {
        result += 32768;
    }
    return result;
}
