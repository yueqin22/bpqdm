/* new translation unit: same renaming table, including the REFORMULATED
 * delivered source at src/motor/ZeroPosCtl.c.
 *
 * The rename table must match ref_tu.c entry for entry -- the symbol names
 * differ but the types and layouts must be identical, otherwise the field by
 * field comparison in the driver means nothing.
 */

#define gZeroPos        new_gZeroPos
#define GetPGPos        new_GetPGPos
#define ZeroPosJudge    new_ZeroPosJudge
#define ZeroPosControl  new_ZeroPosControl

#include "ZeroPosCtl.c"
