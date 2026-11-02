/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define CanControlRead    ref_CanControlRead
#define CanControlWriter  ref_CanControlWriter
#define CanHostTx         ref_CanHostTx
#define CanLinkChara      ref_CanLinkChara
#define CanLinkConDeal    ref_CanLinkConDeal
#define CanLinkRemoteDeal ref_CanLinkRemoteDeal
#define CanLlinkDataDeal  ref_CanLlinkDataDeal
#define CanRxTxCon        ref_CanRxTxCon
#define CanlinkDataRec    ref_CanlinkDataRec
#define CanlinkDataTran   ref_CanlinkDataTran
#define CanlinkFun        ref_CanlinkFun
#define CanlinkRecBuf     ref_CanlinkRecBuf
#define InitCanlink       ref_InitCanlink
#define InitCanlinkRec    ref_InitCanlinkRec
#define InitCanlinkTran   ref_InitCanlinkTran

#include "f_canlink_ref.c"
