
#ifndef DSP2803x_BOOT_VARS_H
#define DSP2803x_BOOT_VARS_H

#ifdef __cplusplus
extern "C" {
#endif

extern Uint16 EmuKey;
extern Uint16 EmuBMode;
extern Uint32 Flash_CPUScaleFactor;
extern void (*Flash_CallbackPtr) (void);

#ifdef __cplusplus
}
#endif

#endif
