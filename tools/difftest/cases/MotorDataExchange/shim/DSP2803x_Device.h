/* Host-side replacement for include/device/DSP2803x_Device.h.
 *
 * The real header ends with 19 #include lines that pull in the whole DSP
 * peripheral register model (Adc.h, EPwm.h, Gpio.h, ...). None of that can
 * compile on the host, and MotorDataExchange.c never dereferences a peripheral
 * register -- its entire hardware boundary is "read a global struct field, write
 * a global struct field".
 *
 * What the real header DOES have to supply, and is reproduced here:
 *
 *   1. the BITn macros -- used by the real motor headers' struct/flag unions;
 *   2. the base integer typedefs (DSP28_DATA_TYPES block, real line 77-88).
 *
 * The typedefs are PINNED to <stdint.h> exact widths rather than copied
 * verbatim. On C28x `unsigned int` is 16 bits, so the original `typedef unsigned
 * int Uint16` is a 16-bit type there; copying it literally would make it 32-bit
 * on the host and move every truncation point in the structs under test. Same
 * reasoning as tools/difftest/shim/DataTypeDef.h -- see its header comment.
 *
 * NOT reproduced: the 19 peripheral headers, and the peripheral register structs
 * (AdcResult / AdcRegs / GpioDataRegs / PieVectTable / CpuTimer1). Those names
 * appear only inside #define bodies in SystemDefine.h / MotorDefine.h, and the
 * preprocessor never evaluates a macro body it does not expand -- MotorDataExchange
 * expands none of them. If that ever changes, the compile fails loudly here
 * rather than silently testing a different program.
 */
#ifndef DSP2803X_DEVICE_H
#define DSP2803X_DEVICE_H

#include "DataTypeDef.h"
#include <stdint.h>

#define BIT0    0x0001
#define BIT1    0x0002
#define BIT2    0x0004
#define BIT3    0x0008
#define BIT4    0x0010
#define BIT5    0x0020
#define BIT6    0x0040
#define BIT7    0x0080
#define BIT8    0x0100
#define BIT9    0x0200
#define BIT10   0x0400
#define BIT11   0x0800
#define BIT12   0x1000
#define BIT13   0x2000
#define BIT14   0x4000
#define BIT15   0x8000

#ifndef DSP28_DATA_TYPES
#define DSP28_DATA_TYPES
typedef int16_t             int16;
typedef int32_t             int32;
typedef int64_t             int64;
typedef uint16_t            Uint16;
typedef uint32_t            Uint32;
typedef uint64_t            Uint64;
typedef float               float32;
typedef long double         float64;
#endif

#endif
