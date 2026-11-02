/* Host library-compatibility mock.
 *
 * The target resolves `abs` to asm/ASMSubPrg.asm's `_abs`:
 *
 *     _abs:  MOV ACC,AL<<16
 *            ABS ACC          ; 32-bit abs
 *            MOV AL,AH        ; take the high word back
 *            LRETR
 *
 * i.e. `_abs(x)` returns |x| truncated to 16 bits.  Because C28x `int` is
 * 16 bits, that is exactly `abs(x)`.
 *
 * WITHOUT this file the host links the MSVC 32-bit `abs`, which for a 16-bit
 * negative argument is a silent no-op: the caller zero-extends the short into
 * the 32-bit parameter, `abs` sees a positive number and returns it unchanged,
 * and truncating back to short restores the negative value.  Measured:
 * abs(-1) == -1, abs(-5) == -5.  See MotorEncoder/mock_encoder.c section 3c.
 * A variant that defines `int abs(int)` in an object file overrides the static
 * library copy, which is why this works.
 *
 * `labs` is the 32-bit sibling of ASMSubPrg's `_absl` (plain 32-bit ABS).
 *
 * `qsin` / `qatan` / `qsqrt` are the fixed-point helpers from ASMSubPrg.asm.
 * The assembly is a minimax polynomial whose bit pattern is NOT reproduced
 * here; what IS reproduced is the documented I/O convention, which is pinned
 * by the call sites and by the asm's own sign reconstruction:
 *
 *   * qsin uses 32768 == pi (period 65536).  Pinned by MotorCurrentTransform:
 *         m_cos = qsin(16384 - angle);   // must equal cos(angle)
 *         => qsin(u) = sin(u * pi / 32768)
 *     which is exactly what the asm computes: it folds the sign out with
 *     `ABS` (setting TC), runs the polynomial on |x|, then `NEGTC` re-applies
 *     the sign at the end.
 *   * qatan returns the same convention: qatan(0) == 0 and qatan(+inf) ==
 *     16384 == pi/2.  Self-consistent with SubPrg.c's atan():
 *         atan(x, y): x == 0 -> +-16384;  x < 0 -> result += 32768
 *     so the argument of qatan is a Q16 tangent (SubPrg passes (y<<16)/x).
 *   * qsqrt is an integer square root of a 32-bit magnitude.  The asm folds a
 *     Q15 scale (SQRT2 == 32768/sqrt(2)) in and out around a Newton step; the
 *     plain integer root is the I/O contract MotorCurrentTransform relies on
 *     (AmpTheta->Amp is an amplitude).
 *
 * CONSEQUENCE OF THE APPROXIMATION: the numbers this mock produces are not
 * bit-identical to the target's.  That is acceptable for a differential test
 * because BOTH the ref side and the new side call this same file, so any
 * behaviour change in the module under test still shows up as a mismatch.
 * What an approximate mock can cost is *coverage*: an input-insensitive mock
 * would let mutations on the path that consumes the result go unnoticed.  All
 * three functions below are strictly input-sensitive over their whole domain
 * (qsin is non-constant on every interval of length 64, qatan and qsqrt are
 * strictly monotone), so that failure mode does not apply.  If absolute values
 * ever matter (golden vectors), port the polynomials from asm/ASMSubPrg.asm
 * first.
 */

/* NOTE ON THE INCLUDE: MotorInclude.h resolves to the common shim's minimal
 * version, which only pulls in the shim's DataTypeDef.h (s16/u16/s32/u32) and
 * deliberately does NOT define Uint / Ulong.  MotorDefine.h is the real common
 * header that does, so it is included explicitly.
 *
 * This file references NO libm symbol at all -- see the notes on h_atan and
 * h_sin for why that is a hard requirement, not a style choice. */

#include "MotorDefine.h"

#define HOST_PI 3.14159265358979323846

int abs(int mValue)
{
    return (mValue < 0) ? -mValue : mValue;
}

long labs(long mValue)
{
    return (mValue < 0) ? -mValue : mValue;
}

/* Self-contained sine.  `qsin` is the last libm dependency in this file and it
 * is removed for the same reason `atan` was: a module-provided definition would
 * silently win at link time (object files beat libraries, C has no mangling).
 * No module defines `sin` today, so this is defence in depth rather than a
 * live bug -- but the remaining modules that use qsin include MotorSVC,
 * MotorVF and f_runSrc_accDecFrq, so the guarantee is worth having.
 *
 * Method: fold the argument into [0, pi/2] using the pi and pi/2 symmetries
 * (exact, no repeated subtraction of 2*pi), then an eight-term Taylor series.
 * At x = pi/2 the truncation error is ~6e-12 rad, i.e. under 1e-6 of one Q15
 * unit.  Measured against libm over the whole 16-bit input range in
 * build/_me_check_atan.py -- see the recorded max deviation there. */

static double h_sin(double x)
{
    double t = x;
    double x2, p;

    if (t < 0.0) {
        return -h_sin(-t);
    }
    if (t > HOST_PI) {
        t -= 2.0 * HOST_PI;
        if (t < 0.0) {
            return -h_sin(-t);
        }
    }
    if (t > HOST_PI / 2.0) {
        t = HOST_PI - t;
    }
    x2 = t * t;
    p = 1.0 + x2 * (-1.0 / 6.0 + x2 * (1.0 / 120.0 + x2 * (-1.0 / 5040.0
        + x2 * (1.0 / 362880.0 + x2 * (-1.0 / 39916800.0
        + x2 * (1.0 / 6227020800.0 + x2 * (-1.0 / 1307674368000.0)))))));
    return t * p;
}

int qsin(int angle)
{
    double r = h_sin((double)angle * HOST_PI / 32768.0) * 32767.0;
    long v;

    if (r >= 0.0) {
        v = (long)(r + 0.5);
    } else {
        v = (long)(r - 0.5);
    }
    if (v > 32767L) {
        v = 32767L;
    }
    if (v < -32768L) {
        v = -32768L;
    }
    return (int)v;
}

/* Self-contained arctangent.  This file must NOT reference the symbol `atan`,
 * because SubPrg.c defines `int atan(int x, int y)` and an object-file
 * definition takes precedence over the C library at link time.  C has no name
 * mangling, so a call to libm's `double atan(double)` from here silently binds
 * to SubPrg's two-argument version, which then calls back into qatan: an
 * infinite recursion that dies as 0xC00000FD (STATUS_STACK_OVERFLOW).  That is
 * not a theory -- it is what this bench did before the fix, and the crash
 * looked like a data-dependent divide fault, which is why it took a while.
 *
 * Method: atan(x) = pi/2 - atan(1/x) for |x| > 1, then five applications of
 * tan(a/2) = u / (1 + sqrt(1+u^2)) to shrink the argument below 2^-5, then a
 * four-term Taylor series.  Error below 1e-13 rad, i.e. under 1e-9 of one Q15
 * unit -- see build/_me_check_atan.py for the measurement against libm. */

static double h_sqrt_1px2(double x2)
{
    double v = 1.0 + x2;
    double g = 1.0;
    int i;

    for (i = 0; i < 12; i++) {
        g = 0.5 * (g + v / g);
    }
    return g;
}

static double h_atan_unit(double x)
{
    double u = x;
    double u3;
    int k;

    for (k = 0; k < 5; k++) {
        u = u / (1.0 + h_sqrt_1px2(u * u));
    }
    u3 = u * u * u;
    /* five halvings were applied, so the reduced angle has to be scaled back
     * by 2^5.  build/_me_check_atan.c caught the version that forgot this:
     * it was off by 7935 output units and 0.76 rad. */
    return 32.0 * (u - u3 / 3.0 + u3 * u * u / 5.0 - u3 * u3 * u / 7.0);
}

static double h_atan(double x)
{
    int neg = 0;
    double a;

    if (x < 0.0) {
        neg = 1;
        x = -x;
    }
    if (x > 1.0) {
        a = HOST_PI / 2.0 - h_atan_unit(1.0 / x);
    } else {
        a = h_atan_unit(x);
    }
    return neg ? -a : a;
}

int qatan(long t)
{
    /* t is a Q16 tangent; result is an angle in the 32768 == pi convention */
    double r = h_atan((double)t / 65536.0) * (32768.0 / HOST_PI);
    long v = (r >= 0.0) ? (long)(r + 0.5) : (long)(r - 0.5);

    if (v > 16384L) {
        v = 16384L;
    }
    if (v < -16384L) {
        v = -16384L;
    }
    return (int)v;
}

Uint qsqrt(Ulong data)
{
    Ulong r = 0;
    Ulong bit = 1UL << 30;

    if (data == 0) {
        return 0;
    }
    while (bit > data) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (data >= r + bit) {
            data -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return (Uint)r;
}
