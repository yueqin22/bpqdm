/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define CommDpRcvDataDeal  ref_CommDpRcvDataDeal
#define CommDpSendDataDeal ref_CommDpSendDataDeal
#define DpDataDeal         ref_DpDataDeal
#define ErrAgainSend       ref_ErrAgainSend
#define GetDataByteNum     ref_GetDataByteNum
#define InitSciDpBaudRate  ref_InitSciDpBaudRate
#define InitSciaGpioDp     ref_InitSciaGpioDp
#define InitScibGpioDp     ref_InitScibGpioDp
#define InitSetSciDp       ref_InitSetSciDp
#define SCI_DP_RXD_isr     ref_SCI_DP_RXD_isr
#define SCI_DP_TXD_isr     ref_SCI_DP_TXD_isr
#define SciDpDeal          ref_SciDpDeal
#define UpdateSciDpFormat  ref_UpdateSciDpFormat
#define dataByteNum        ref_dataByteNum
#define dataByteNums       ref_dataByteNums
#define dpParameter        ref_dpParameter
#define dpSciCommRcvFlag   ref_dpSciCommRcvFlag
#define dpSciCommSendFlag  ref_dpSciCommSendFlag
#define dpTest             ref_dpTest
#define rcvData_DP         ref_rcvData_DP
#define sciM380DpData      ref_sciM380DpData
#define sendData_DP        ref_sendData_DP

/* 手工补的一条, nm2000 读不出来。
 *
 * f_comm_DP.c 第 152 行是 `void inline CommDpStartSend(...)`。C11 的 inline
 * 定义在目标端**不发外部符号** (nm2000 的 f_comm_DP.obj 里既没有 T 也没有 U,
 * 已核), 所以 new_case.py 从 nm2000 推出来的 22 个名字里没有它。
 *
 * 但宿主 MSVC 会把它发成 linkonce/COMDAT: ref_tu.c 和 new_tu.c 各有一份同名
 * 定义, 链接器**静默折叠成一个**, 于是其中一侧的函数体整个是死代码。现象是
 * 这个函数体内的变异全部 MISSED、函数外的全部 CAUGHT —— 看着像"这块逻辑测
 * 不出", 其实是测试台把待测代码短路了。MotorCurrentTransform 的 MC1~MC6 就是
 * 这么漏的 (最小复现 build/_me_repro_inline.py)。
 *
 * 改名之后两侧各有独立符号, driver 还要**直接驱动**它, 覆盖不能只靠外层传递。 */
#define CommDpStartSend    ref_CommDpStartSend

#include "f_comm_DP_ref.c"
