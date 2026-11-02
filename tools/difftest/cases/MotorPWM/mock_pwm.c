/* Shared mock for the MotorPWM differential test.
 *
 * src/motor/MotorPWM.c defines 10 data symbols of its own (gOutVolt, gPWM,
 * gRatio, EPWMFlagU/V/W and the five iTrig* tables) -- those are renamed per
 * side by ref_tu.c / new_tu.c, exactly like the five entry points, so they are
 * NOT in this file.  Everything the module references but does not define lives
 * here, one instance each, shared by both translation units:
 *
 *   1. the three enhanced-PWM register blocks.  MotorPWM is the only module so
 *      far that writes a peripheral field by field, and it also READS one back:
 *      SendPWM() branches on `EPwm1Regs.CMPA.half.CMPA != 0` / `== 0` before
 *      writing it (source lines 397/405, 415/423, 433/441).  That read-back
 *      makes the previous register content part of the module's input, so these
 *      have to be real `struct EPWM_REGS` objects -- taken from the real
 *      include/device/DSP2803x_EPwm.h via real.txt, not hand-written -- or the
 *      diff would compare two layouts that are both wrong.
 *
 *   2. the CPU-control counter table the case shim turns EALLOW/EDIS into.
 *      InitSetPWM() and SendPWM() each bracket their register writes with the
 *      pair (lines 20/131 and 369/457), so the count is observable state: a
 *      rewrite that moves a write outside the window changes it.
 *
 *   3. the three globals owned by other translation units in the firmware
 *      (gADC / gDeadBand / gPhase -- MotorInfoCollect / MotorPWM's own
 *      CalDeadBandComp / CalOutputPhase).  Their types come from the real
 *      project headers, so field widths and layout are the firmware's, which
 *      matters because the case shim folds `int` to the target's 16 bits:
 *      Uint storage and int storage must match the C28x, see
 *      shim/DSP2803x_Device.h.
 *
 * The library helper the module calls (qsin) is NOT here: it comes from the
 * shared hostlib_mock.c, and __IQsat is the static model in the case shim.
 */

#include "MotorPwmInclude.h"

/* ---- 1. peripheral registers -------------------------------------------- */

volatile struct EPWM_REGS EPwm1Regs;
volatile struct EPWM_REGS EPwm2Regs;
volatile struct EPWM_REGS EPwm3Regs;

/* ---- 2. CPU control sequence -------------------------------------------- */

volatile Uint16 gHostCpuCtl[7];

/* ---- 3. globals owned by other translation units ------------------------ */
/* The set is MotorPWM.c's object file's undefined DATA symbols, read out with
 * coff_cmp (3 entries), with types taken from the headers that declare them. */

ADC_STRUCT                         gADC;
DEAD_BAND_STRUCT                   gDeadBand;
ANGLE_STRUCT                       gPhase;
