/* Host-side replacement for DSP28x_Project.h -- f_debug case private.
 *
 * f_debug.c's live code touches no peripheral at all: InitDa / UpdateDaBuffer /
 * DaOut / DA_SYNC_DEAL sit inside a `#if 0` block (source lines 419-520) and
 * are not compiled, so the SPI and GPIO models are not needed here.  What the
 * live code needs is exactly three things:
 *
 *   - exact-width integer types (Uint16 / Uint32 / int16 / int32);
 *   - `runFlag` with `bit.run` at bit 0 (supplied by shim/f_runSrc.h);
 *   - the macros in the REAL f_debug.h (LOCALF / LOCALD /
 *     DEBUG_F_POSITION_CTRL / F_DEBUG_RAM), which real.txt forwards rather than
 *     re-typing -- the whole point of that header is to carry the build
 *     configuration, and a hand copy could silently disagree with it.
 *
 * Widths are pinned, not typedef'd from the original header.  The original
 * makes Uint16 `unsigned int`, which is 16 bits on the C28x and 32 on the host;
 * reusing it would widen every Uint16 in this module and move the truncation
 * point in `*(pScope + scopeIndex[sn]) = *matlabDebug[sn]` -- the very
 * expression the differential test exists to check.
 *
 * `DSP_CLOCK` is deliberately left undefined.  The real f_debug.h gates TIME_*
 * on `#if (DSP_CLOCK == 100) / #elif (DSP_CLOCK == 60)`, so with it undefined
 * neither arm is taken and TIME_* stay undefined -- which is harmless because
 * f_debug.c never uses them.  Defining it would invent a value the build does
 * not have.
 */
#ifndef DSP28X_PROJECT_H
#define DSP28X_PROJECT_H

#include <stdint.h>

typedef int16_t             int16;
typedef int32_t             int32;
typedef uint16_t            Uint16;
typedef uint32_t            Uint32;

#endif
