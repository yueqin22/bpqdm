/* Host-side replacement for f_funcCode.h -- f_debug case private.
 *
 * The real f_funcCode.h is the root of the project's whole header tree:
 *
 *     f_funcCode.h -> DSP28x_Project.h, f_debug.h, f_interface.h, f_p2p.h
 *
 * and every other header f_debug.c includes (f_runSrc.h, f_main.h, f_ui.h)
 * just re-includes it.  Forwarding it would drag in the FUNCCODE_* enums, the
 * menu/EEPROM tables, the device register model and the cregister/asm macros
 * that MSVC rejects -- all for a module whose live code reads none of it.
 *
 * What f_debug.c's LIVE code actually uses is checked off one by one:
 *
 *   funcCode / funcCodeAttribute / attributeDebug   -- only inside the `#if 0`
 *       blocks of PowerOnDebug (line 415) and ChgAttr (lines 524-560).  The
 *       `#if 1` block at 158-210 declares the ATTRIBUTE_* structs and the
 *       global `int16 i` self-contained, so it needs nothing from here.
 *   runFlag                                        -- shim/f_runSrc.h
 *   frq / frqRun / frqTmp                          -- mock_debug.c
 *   Uint16 / Uint32 / int16 / int32                -- shim/DSP28x_Project.h
 *   LOCALF / LOCALD / DEBUG_F_POSITION_CTRL /
 *   F_DEBUG_RAM                                    -- REAL f_debug.h, forwarded
 *
 * So this stub only has to deliver the types and then the real f_debug.h.  The
 * order matters: f_debug.h spells its prototypes with Uint16/Uint32, so the
 * types have to be in place before it is read.
 *
 * DEBUG_F_POSITION_CTRL is NOT defined here.  It comes from the real f_debug.h,
 * where it is 0 because ZJ_MD380M is never defined by any build configuration.
 * Re-typing that value would make the bench and the delivered firmware disagree
 * about whether the module is reachable at all -- see the note in
 * cases/f_debug/README.md.
 */
#ifndef F_FUNCCODE_SHIM_H
#define F_FUNCCODE_SHIM_H

#include "DSP28x_Project.h"
#include "f_debug.h"

#endif
