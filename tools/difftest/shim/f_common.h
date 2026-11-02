/* Host-side shim for the f_common differential test.
 *
 * f_common.c includes "f_common.h", which in the project pulls in f_funcCode.h
 * and with it most of the firmware. The module itself only needs Uint16/int16
 * and three prototypes, so the header is replaced by this minimal stand-in.
 *
 * Widths are pinned to exact-width types rather than reused from the project's
 * DataTypeDef.h, where `typedef unsigned int Uint16` is 16 bits on C28x but 32
 * bits on the host. Pinning uint16_t keeps the integer promotions of every
 * expression in this module identical on both sides:
 *   - `crcValue ^ *data++`  : C28x promotes 16-bit unsigned int to itself;
 *                             host promotes uint16_t to 32-bit int. Both are
 *                             unsigned-valued and fit, so the XOR matches.
 *   - `... >> 8`            : logical shift on both sides (the operand is
 *                             unsigned-valued either way).
 *   - `(tmp & 0x00FF) << 8` : <= 65280, representable in both.
 * No intermediate exceeds 65535, so there is no truncation point to diverge on.
 */
#ifndef F_COMMON_H
#define F_COMMON_H

#include "DataTypeDef.h"

typedef uint16_t Uint16;
typedef int16_t  int16;

/* f_common.c gates its body on F_DEBUG_RAM. On the target f_debug.h:10-14 sets
 * it to 0 because FUNC_DEBUG_RAM is not defined, so the CRC/bit-reverse branch
 * is the live one (link map: f_common.obj contributes 67 code words). It is
 * pinned here so the host build cannot silently pick the empty branch.
 */
#ifndef F_DEBUG_RAM
#define F_DEBUG_RAM 0
#endif

Uint16 CrcValueByteCalc(const Uint16 *data, Uint16 length);
Uint16 Reverse4Bit(Uint16 value);
Uint16 Reverse8Bit(Uint16 value);

#endif
