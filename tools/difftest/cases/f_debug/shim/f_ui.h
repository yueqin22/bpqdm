/* Host-side replacement for f_ui.h -- f_debug case private.
 *
 * The real f_ui.h carries the display and keypad model: DISPLAY_* / KEY_* /
 * LED_* code tables, DISPLAY_8LED_NUM, the `keyValue` and `displayBuffer`
 * externs.  f_debug.c's live code reads none of them.  The only keypad-ish
 * thing in the module is DealKeyDebug(), whose entire body is inside `#if 0`
 * (source lines 339-346), so `keyValue` is never touched.
 *
 * Empty stub for the same reason as f_main.h: f_debug.c includes it by name and
 * the case shim must shadow the real header so the include tree stays out.
 */
#ifndef F_UI_SHIM_H
#define F_UI_SHIM_H

#include "DSP28x_Project.h"

#endif
