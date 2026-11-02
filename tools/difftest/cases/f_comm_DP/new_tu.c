/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define CommDpRcvDataDeal  new_CommDpRcvDataDeal
#define CommDpSendDataDeal new_CommDpSendDataDeal
#define DpDataDeal         new_DpDataDeal
#define ErrAgainSend       new_ErrAgainSend
#define GetDataByteNum     new_GetDataByteNum
#define InitSciDpBaudRate  new_InitSciDpBaudRate
#define InitSciaGpioDp     new_InitSciaGpioDp
#define InitScibGpioDp     new_InitScibGpioDp
#define InitSetSciDp       new_InitSetSciDp
#define SCI_DP_RXD_isr     new_SCI_DP_RXD_isr
#define SCI_DP_TXD_isr     new_SCI_DP_TXD_isr
#define SciDpDeal          new_SciDpDeal
#define UpdateSciDpFormat  new_UpdateSciDpFormat
#define dataByteNum        new_dataByteNum
#define dataByteNums       new_dataByteNums
#define dpParameter        new_dpParameter
#define dpSciCommRcvFlag   new_dpSciCommRcvFlag
#define dpSciCommSendFlag  new_dpSciCommSendFlag
#define dpTest             new_dpTest
#define rcvData_DP         new_rcvData_DP
#define sciM380DpData      new_sciM380DpData
#define sendData_DP        new_sendData_DP

/* see the note in ref_tu.c: `CommDpStartSend` is a C11 `inline` definition, so
 * the target emits no external symbol for it and new_case.py's nm2000-derived
 * rename list cannot see it.  Without a distinct name here the host linker
 * folds the two same-named COMDAT copies into one and this side's body never
 * runs. */
#define CommDpStartSend    new_CommDpStartSend

#include "f_comm_DP.c"
