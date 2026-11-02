/* ref translation unit: prefix the module's exported symbols with ref_ and then
 * include the FROZEN baseline implementation verbatim.
 *
 * The point is that the real delivered source is compiled, not a transcribed
 * copy -- a copy would introduce "is the thing under test really the source?"
 * as a brand new uncertainty.
 *
 * Only symbols EXPORTED by the module are renamed. The hardware mock
 * (EQepRegs / CpuTimer1 / gMainCmd) is left alone so both sides see the same
 * register state.
 */

#define gZeroPos        ref_gZeroPos
#define GetPGPos        ref_GetPGPos
#define ZeroPosJudge    ref_ZeroPosJudge
#define ZeroPosControl  ref_ZeroPosControl

#include "ZeroPosCtl_ref.c"
