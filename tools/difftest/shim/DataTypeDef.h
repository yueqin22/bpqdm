/* Host-side exact-width model, replacing the project's DataTypeDef.h.
 *
 * Original target: int = 16 bits, long = 32 bits (C28x).
 * Host x86:        int = 32 bits.
 * Reusing the original header would silently widen s16/u16 to 32 bits and move
 * every overflow/truncation point, which would make the differential test
 * meaningless. The four types are therefore pinned to exact widths here.
 */
#ifndef DATA_TYPE_DEF_INCLUDE_H
#define DATA_TYPE_DEF_INCLUDE_H

#include <stdint.h>

typedef int16_t   s16;
typedef uint16_t  u16;
typedef int32_t   s32;
typedef uint32_t  u32;

#endif
