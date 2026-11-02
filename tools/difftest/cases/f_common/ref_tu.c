/* ref translation unit: prefix the module's exported symbols and include the
 * FROZEN baseline implementation verbatim.
 *
 * Every file-scope symbol the module defines must be renamed here, otherwise
 * the two translation units collide at link time. For f_common that is the
 * three functions plus the const table (in C, file-scope `const` still has
 * external linkage).
 */

#define CrcValueByteCalc   ref_CrcValueByteCalc
#define Reverse4Bit        ref_Reverse4Bit
#define Reverse8Bit        ref_Reverse8Bit
#define crc16Table         ref_crc16Table

#include "f_common_ref.c"
