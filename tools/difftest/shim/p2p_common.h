/* Host-side shim for the f_p2p differential test.
 *
 * Widths are pinned to exact-width types, NOT to the original typedefs.
 * Reason: the original has `typedef unsigned int Uint16`. On C28x `unsigned int`
 * is 16 bits, so `long / Uint16` converts the divisor to long and does a SIGNED
 * division. On the host `unsigned int` is 32 bits, so `long / unsigned int`
 * converts the dividend to unsigned long and does an UNSIGNED division -- a
 * different result for negative dividends. Using uint16_t here restores the
 * C28x promotion: uint16_t promotes to int, so the division stays signed.
 */
#ifndef P2P_COMMON_H
#define P2P_COMMON_H

#include "DataTypeDef.h"

typedef uint16_t       Uint16;
typedef uint32_t       Uint32;
typedef int16_t        int16;
typedef int32_t        int32;

/* f_p2p.h gates its struct behind this; must be defined before it is included. */
#define DEBUG_F_P2P_CTRL 1

#define ERROR_COMM          16
#define COMM_ERROR_MODBUS   1

struct FUNCCODE_P2P_MOCK {
    Uint16 p2pEnable;
    Uint16 p2pTypeSel;
    Uint16 p2pSendDataSel;
    Uint16 p2pRevDataSel;
    Uint16 p2pRevOffset;
    Uint16 p2pRevGain;
    Uint16 p2pOverTime;
    Uint16 p2pSendPeriod;
};
struct FUNCCODE_MOCK {
    struct FUNCCODE_P2P_MOCK code;
};
extern struct FUNCCODE_MOCK funcCode;

extern Uint16 itDisp;
extern int32  frqRun;
extern int32  frqAimTmp;
extern Uint16 frqFdb;
extern Uint16 maxFrq;
extern Uint16 errorOther;
extern Uint16 errorInfo;

#endif
