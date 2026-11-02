/* Per-side extern declarations for the f_canlink differential driver.
 *
 * ref_tu.c / new_tu.c rename every external-linkage symbol of f_canlink.c with a
 * ref_ / new_ prefix so both copies can live in one binary.  The driver needs to
 * reach both copies, which means declaring each symbol twice, with the right
 * type.  The symbol list comes from ref_tu.c (itself generated from nm2000, so
 * it cannot drift); the types come from the module's own definition lines in
 * the frozen baseline.
 *
 * Twelve functions + three data objects = the fifteen symbols.  The four
 * function-local statics (count in CanlinkDataTran, timeOutErr and initFlag in
 * CanlinkFun, and the two file-scope statics sBaud / sSourID) are deliberately
 * absent: cl2000 mangles file-local statics with a $N suffix, so they are local
 * symbols and the driver cannot reach them.  See the header comment of driver.c
 * for why that is not a hole.
 *
 * Note which globals are NOT here: funcCode, p2pData, sciFlag, commRcvData,
 * commReadData, curTime, errorOther and errorInfo are defined by the shared mock
 * (mock_canlink.c), not by the module, so both sides read and write the *same*
 * object.  The driver installs that object before each side runs and captures it
 * after -- see driver.c.
 */
#ifndef CANLINK_SIDES_H
#define CANLINK_SIDES_H

#include "f_canlink.h"

extern void   ref_InitCanlinkTran(Uint16 addr, Uint32 *dataPi);
extern void   new_InitCanlinkTran(Uint16 addr, Uint32 *dataPi);
extern Uint16 ref_CanlinkDataTran(Uint32 *dataPi, Uint16 len, Uint16 timeOut);
extern Uint16 new_CanlinkDataTran(Uint32 *dataPi, Uint16 len, Uint16 timeOut);
extern void   ref_InitCanlinkRec(Uint16 addr);
extern void   new_InitCanlinkRec(Uint16 addr);
extern Uint16 ref_CanlinkDataRec(struct CANLINK_REC_BUF *dataPi);
extern Uint16 new_CanlinkDataRec(struct CANLINK_REC_BUF *dataPi);
extern Uint16 ref_InitCanlink(Uint16 addr, Uint16 baud, Uint32 *dataPi);
extern Uint16 new_InitCanlink(Uint16 addr, Uint16 baud, Uint32 *dataPi);
extern void   ref_CanlinkFun(void);
extern void   new_CanlinkFun(void);
extern void   ref_CanLlinkDataDeal(struct CANLINK_DATA_BUF *dataPi);
extern void   new_CanLlinkDataDeal(struct CANLINK_DATA_BUF *dataPi);
extern void   ref_CanLinkRemoteDeal(struct CANLINK_DATA_BUF *dataPi);
extern void   new_CanLinkRemoteDeal(struct CANLINK_DATA_BUF *dataPi);
extern Uint16 ref_CanControlWriter(Uint16 addr, Uint16 data, Uint16 eeprom);
extern Uint16 new_CanControlWriter(Uint16 addr, Uint16 data, Uint16 eeprom);
extern Uint16 ref_CanControlRead(Uint16 addr, Uint16 *result);
extern Uint16 new_CanControlRead(Uint16 addr, Uint16 *result);
extern void   ref_CanLinkConDeal(struct CANLINK_DATA_BUF *dataPi);
extern void   new_CanLinkConDeal(struct CANLINK_DATA_BUF *dataPi);
extern void   ref_CanHostTx(Uint16 addr, Uint16 data);
extern void   new_CanHostTx(Uint16 addr, Uint16 data);

extern Uint16                 ref_CanRxTxCon;
extern Uint16                 new_CanRxTxCon;
extern struct CANLINK_REC_BUF ref_CanlinkRecBuf;
extern struct CANLINK_REC_BUF new_CanlinkRecBuf;
extern Uint32                 ref_CanLinkChara[2];
extern Uint32                 new_CanLinkChara[2];

#endif
