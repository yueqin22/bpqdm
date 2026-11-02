/* Interface of the f_canlink differential mock.
 *
 * mock_canlink.c supplies everything the module reads from outside itself: the
 * eight external data objects and the ten external functions.  See that file
 * for why the data is defined here with its real type.
 *
 * ---- the call trace ----
 *
 * f_canlink's hardware boundary is five functions that return a status the
 * module branches on (InitdspECan, eCanDataTran, eCanDataRec) or that configure
 * a mailbox and return nothing (InitTranMbox, InitRecMbox).  A rewrite can
 * change *which* mailbox it configures, or the msgid it writes, without moving
 * any global the driver can see -- so the calls themselves are part of the
 * observation set.  The same is true of the five project functions
 * (CommWrite, CommRead, PlcDataFramDeal, InvTranTabCfg, InvRecTabCfg).
 *
 * One entry per call, five words: which, then four arguments.  The four slots
 * carry what actually matters per call:
 *
 *   InitdspECan   (baud)
 *   InitTranMbox  (mbox, msgid, data[0], data[1])
 *   InitRecMbox   (mbox, msgid, lam)
 *   eCanDataTran  (mbox, len, msgid, data[0])
 *   eCanDataRec   (mbox)
 *   CommWrite     (addr, data, commCmdSaveEeprom, sciFlag.all)
 *   CommRead      (addr, len,  commCmdSaveEeprom, sciFlag.all)
 *   PlcDataFramDeal / InvTranTabCfg / InvRecTabCfg  (msgid.all, mdl.all)
 *
 * eCanDataTran is called from three places with three different payloads
 * (CanlinkDataTran, CanLinkRemoteDeal and CanLinkConDeal/CanHostTx), so
 * capturing msgid and the first payload word is what tells them apart.
 *
 * The two Comm* stubs carry two extra words because the latch globals they read
 * are the *only* outward effect of CanControlWriter's `eeprom` argument: the
 * flag is overwritten on the next line, so a caller cannot see it afterwards,
 * but the callee can -- and in the real f_comm.c it does (CommWrite ->
 * CommRwFuncCode -> SaveOneFuncCode, gated on commCmdSaveEeprom). See
 * mock_canlink.c.
 *
 * A single CanlinkFun() call can reach the stub set up to 4 + 4 + 4 + 4 = 16
 * times (InitTranMbox x4 and InitRecMbox x4 on a re-init, then eCanDataRec x4
 * and one eCanDataTran per received frame).  A fixed array would either
 * overflow silently or need a size that only one call sequence happens to fit.
 * So the driver compares three things:
 *
 *   * cantTraceN    -- total number of stub calls, unbounded;
 *   * cantTraceHash -- FNV-1a over every entry, in order;
 *   * cantTrace[]   -- the first CANT_N entries, for diagnostics only.
 *
 * Comparing only the terminal state would miss "a stub call was deleted" or
 * "two calls swapped", which is exactly what a trace is for.
 */
#ifndef MOCK_CANLINK_H
#define MOCK_CANLINK_H

#include "DataTypeDef.h"

#define CANT_N 24

/* `which` values. */
#define CANT_INITECAN   0
#define CANT_INITTRAN   1
#define CANT_INITREC    2
#define CANT_ECTRAN     3
#define CANT_ECREC      4
#define CANT_COMMWRITE  5
#define CANT_COMMREAD   6
#define CANT_PLCDATA    7
#define CANT_INVTTRAN   8
#define CANT_INVREC     9

struct CANT_ENT
{
    Uint16 which;
    Uint32 a0, a1, a2, a3;
};

extern struct CANT_ENT cantTrace[CANT_N];
extern Uint32 cantTraceN;
extern Uint32 cantTraceHash;

/* Return-value knobs.  Each is the answer of one stub; the driver sets them as
 * input and the module branches on the result, so they are not constants. */
extern Uint16 mockInitECanRc;    /* InitdspECan                        */
extern Uint16 mockTranSuccOn;    /* eCanDataTran: SUCC on the Nth call (1-based); 0 = never */
extern Uint16 mockRecOverOn;     /* eCanDataRec:  REC_OVER on the Nth call; 0 = never */
extern Uint16 mockRecSuccFrom;   /* eCanDataRec:  REC_SUCC for call >= this; 0 = never */
extern Uint16 mockCommWriteRc;   /* CommWrite                          */
extern Uint16 mockCommReadRc;    /* CommRead                           */
extern Uint16 mockInvTranRc;     /* InvTranTabCfg                      */
extern Uint16 mockInvRecRc;      /* InvRecTabCfg                       */

/* eCanDataRec writes mockRecWord[(n-1) & 3] into *dataPi on every non-empty
 * call, so the four receive mailboxes can carry four different frames -- that
 * is what feeds CanLlinkDataDeal's frame-type switch on the next tick. */
extern Uint32 mockRecWord[4];

/* Zero the trace, the hash and the per-kind call counters.  The driver calls
 * this before each side runs so the two sides start from the same place. */
void mockReset(void);

#endif
