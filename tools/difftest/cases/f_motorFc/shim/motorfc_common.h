/* Host-side shim for the f_motorFc differential test.
 *
 * f_motorFc.c reads a large slice of the project's global state, and the index
 * it builds into funcCode.all[] is a *word offset of a struct member*:
 *
 *     GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.motorType)
 *         == (Uint16)(&(that member) - &(funcCode.all[0]))
 *
 * That offset is only meaningful against the REAL struct definitions, so
 * f_funcCode.h -- and what it pulls in, f_debug.h / f_interface.h / f_p2p.h --
 * is used verbatim. Only the headers that drag in the DSP peripheral model are
 * replaced. This is the same technique as the f_p2p case, taken further.
 *
 * Widths are pinned to exact-width types for the same reason as the other
 * cases: the project's DataTypeDef.h maps Uint16 to `unsigned int`, which is 16
 * bits on C28x but 32 on the host. Pinning also makes struct sizes match the
 * target, because C28x has int = 16 and long = 32.
 */
#ifndef MOTORFC_COMMON_H
#define MOTORFC_COMMON_H

#include "DataTypeDef.h"

/* f_motorFc.c calls memcpy without including <string.h>; on the target it gets
 * the prototype transitively through the device headers, which are replaced
 * here. Include it explicitly so the host call uses the real prototype rather
 * than MSVC's implicit-int fallback. */
#include <string.h>

typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int16_t  int16;
typedef int32_t  int32;

/* f_p2p.h (real) gates P2P_DATA_STRUCT behind this. */
#define DEBUG_F_P2P_CTRL 1

/* f_error.h: only these two are reachable from f_motorFc.c. ERROR_ENCODER is
 * the plain value 20 (include/function/f_error.h:27) -- not the motor-side
 * bitmask of the same name in MotorInvProtectInclude.h. */
#define ERROR_ENCODER 20
extern Uint16 errorCode;

/* f_menu.h: only decNumber is reachable. */
extern const int16 decNumber[];

#endif
