/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define CanControlRead    new_CanControlRead
#define CanControlWriter  new_CanControlWriter
#define CanHostTx         new_CanHostTx
#define CanLinkChara      new_CanLinkChara
#define CanLinkConDeal    new_CanLinkConDeal
#define CanLinkRemoteDeal new_CanLinkRemoteDeal
#define CanLlinkDataDeal  new_CanLlinkDataDeal
#define CanRxTxCon        new_CanRxTxCon
#define CanlinkDataRec    new_CanlinkDataRec
#define CanlinkDataTran   new_CanlinkDataTran
#define CanlinkFun        new_CanlinkFun
#define CanlinkRecBuf     new_CanlinkRecBuf
#define InitCanlink       new_InitCanlink
#define InitCanlinkRec    new_InitCanlinkRec
#define InitCanlinkTran   new_InitCanlinkTran

#include "f_canlink.c"
