/* new translation unit: identical renaming table, but including the HOST-PATCHED
 * copy of the delivered source at src/function/f_motorFc.c (see ref_tu.c and
 * cases/f_motorFc/host_patch.json for why the patch exists).
 *
 * Keep this renaming list byte-for-byte in step with ref_tu.c: a symbol renamed
 * on one side but not the other either fails to link (missing symbol) or
 * silently aliases the two implementations together, which would make the whole
 * differential test compare a thing against itself.
 */

#define UpdateMotorPara             new_UpdateMotorPara
#define polePair                    new_polePair
#define motorSnIndexNIncToMotor2    new_motorSnIndexNIncToMotor2

#include "host_new_f_motorFc.c"
