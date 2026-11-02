/* Shared mock for the MotorDataExchange differential test.
 *
 * ONE instance of every global the module touches, read by both sides. The
 * module's own exports are NOT here -- ref_tu.c / new_tu.c give each side its own
 * copy of those (see shim/mock_mde.h).
 *
 * This file is only definitions; it has no behaviour. All the work is in
 * driver.c, which seeds these globals, calls the module and compares what came
 * back out.
 */
#include "mock_mde.h"

#define DEFINE_GLOBAL(name, type) type name;

MDE_EXTERN_GLOBALS(DEFINE_GLOBAL)
