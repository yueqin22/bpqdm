/* ref translation unit: rename the module's file-scope symbols, then include the
 * FROZEN baseline.
 *
 * Three symbols need renaming, not one:
 *   UpdateMotorPara            the exported function
 *   polePair                   a file-scope global (C: external linkage)
 *   motorSnIndexNIncToMotor2   file-scope layout table (also external linkage)
 *
 * The two `UpdateMotorPara` definitions in the source sit in mutually exclusive
 * preprocessor branches (#if DEBUG_F_MANY_MOTOR / #elif 1), so only one is ever
 * compiled -- no redefinition.
 *
 * The include target is the HOST-PATCHED copy, not ref/f_motorFc_ref.c directly.
 * See cases/f_motorFc/host_patch.json: MSVC's C front end cannot fold the layout
 * table's `&a.m1 - &a.m2` initializer (error C2099), so the host copy drops the
 * initializer and driver.c fills it at run time. The patch is asserted to change
 * exactly one hunk, and run_case.py fails loudly if the source no longer matches.
 */

#define UpdateMotorPara             ref_UpdateMotorPara
#define polePair                    ref_polePair
#define motorSnIndexNIncToMotor2    ref_motorSnIndexNIncToMotor2

#include "host_ref_f_motorFc.c"
