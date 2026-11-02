
#ifndef DSP2803x_EXAMPLES_H
#define DSP2803x_EXAMPLES_H

#ifdef __cplusplus
extern "C" {
#endif

#define    	DSP_CLOCK			60

#define     CPU_TYPE            28034

#define DSP28_DIVSEL   2

#define DSP28_PLLCR   12

#define CPU_RATE   16.667L

#define Device_cal (void   (*)(void))0x3D7C80

#include "DSP2803x_GlobalPrototypes.h"

#include "DSP2803x_EPwm_defines.h"
#include "DSP2803x_I2C_defines.h"
#include "DSP2803x_Cla_Defines.h"

#define   PARTNO_28030PAG   0xAA
#define   PARTNO_28030PN    0xAB

#define   PARTNO_28031PAG   0xAE
#define   PARTNO_28031PN    0xAF

#define   PARTNO_28032PAG   0xB2
#define   PARTNO_28032PN    0xB3

#define   PARTNO_28033PAG   0xB6
#define   PARTNO_28033PN    0xB7

#define   PARTNO_28034PAG   0xBA
#define   PARTNO_28034PN    0xBB

#define   PARTNO_28035PAG   0xBE
#define   PARTNO_28035PN    0xBF

#ifndef DSP28_BIOS
#include "DSP2803x_DefaultISR.h"
#endif

#define DELAY_US(A)  DSP28x_usDelay(((((long double) A * 1000.0L) / (long double)CPU_RATE) - 9.0L) / 5.0L)

#ifdef __cplusplus
}
#endif

#endif
