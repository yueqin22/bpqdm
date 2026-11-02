/* ref translation unit: prefix the module's exported symbols and include the
 * FROZEN baseline implementation verbatim.
 *
 * Only symbols exported by the module are renamed. The shared mock state
 * (funcCode / itDisp / frqRun / ...) is left alone so both sides read the same
 * inputs.
 */

#define p2pData       ref_p2pData
#define P2PDataDeal   ref_P2PDataDeal

#include "f_p2p_ref.c"
