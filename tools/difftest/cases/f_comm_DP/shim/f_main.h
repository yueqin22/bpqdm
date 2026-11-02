/* Host-side replacement for f_main.h -- f_comm_DP case private.
 *
 * f_comm_DP.c line 3 includes it.  The live code takes nothing from it: the
 * module's only reference to anything f_main.h provides is `funcCode`, and that
 * appears exclusively inside `#if BUG_SCI_BACK_DATA` (f_comm_DP.h sets it to 0).
 *
 * The real f_main.h is a hub -- f_funcCode.h / f_error.h / f_common.h /
 * f_canlink.h -- so forwarding it would drag in the whole tree to supply
 * nothing.  Empty is the honest answer, and it is self-checking: if a live
 * reference ever appears, the case stops compiling instead of testing a
 * different module.
 */
#ifndef F_MAIN_H
#define F_MAIN_H

#endif
