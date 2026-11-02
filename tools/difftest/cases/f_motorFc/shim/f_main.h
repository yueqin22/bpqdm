/* Host-side replacement for f_main.h.
 *
 * The real f_main.h defines GetTime() / OpenDrive() / Nop() against
 * CpuTimer1.RegsAddr and GpioDataRegs, so it needs the peripheral model.
 * f_motorFc.c needs from it only the two globals declared at its lines 69-70.
 *
 * f_funcCode.h is included here (not in motorfc_common.h) so that the pinned
 * widths are in scope before it is parsed, and so that motorFc / motorSn can be
 * declared with their real types.
 */
#ifndef __F_MAIN_H__
#define __F_MAIN_H__

#include "motorfc_common.h"
#include "f_funcCode.h"

extern struct MOTOR_FC motorFc;
extern enum MOTOR_SN   motorSn;

#endif
