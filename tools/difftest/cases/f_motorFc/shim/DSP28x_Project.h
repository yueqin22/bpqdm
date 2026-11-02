/* Host-side replacement for the TI DSP28x_Project.h.
 *
 * The real one pulls in DSP2803x_Device.h + DSP2803x_Examples.h, i.e. the whole
 * peripheral register model and the C28x intrinsics. f_funcCode.h includes it
 * but needs nothing from it: the structs it defines are built from Uint16 /
 * Uint32 only. Replaced by an empty guard so f_funcCode.h can be used verbatim.
 */
#ifndef DSP28x_PROJECT_H
#define DSP28x_PROJECT_H

#endif
