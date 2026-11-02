/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define COMM_ERR_INDEX      ref_COMM_ERR_INDEX
#define CommDataReRcv       ref_CommDataReRcv
#define CommDataSend        ref_CommDataSend
#define CommRcvDataDeal     ref_CommRcvDataDeal
#define CommStartSend       ref_CommStartSend
#define CommRead            ref_CommRead
#define CommRwFuncCode      ref_CommRwFuncCode
#define CommSendDataDeal    ref_CommSendDataDeal
/* CommStartSend 是**手加**的, 不在 nm2000 的符号表里: 它被声明成
 * `LOCALF void inline`, 而 TI cl2000 对 C11 inline 定义**不发外部符号**,
 * 所以 new_case.py 按符号表生成时看不到它。
 *
 * 但宿主侧必须改名。MSVC 把 inline 定义发成 linkonce/COMDAT, 两个翻译单元
 * 各有一份**同名**定义时链接器会**静默折叠成一个** (不报 LNK2005), 另一侧
 * 的函数体整个变成死代码。现象非常典型: 该函数体内的变异全 MISSED, 函数外
 * 的全 CAUGHT —— 看着像"这块逻辑宿主测不出", 其实是测试台把待测代码短路了
 * (MotorCurrentTransform 的 MC1~MC6 就是这么漏的, 见 cases/MotorCurrentTransform
 * /README.md)。改名之后两个 COMDAT 组名字不同, 折叠不再发生。 */
#define CommWrite           ref_CommWrite
#define InitSciaGpio        ref_InitSciaGpio
#define InitSetScia         ref_InitSetScia
#define Lina_Level0_ISR     ref_Lina_Level0_ISR
#define Lina_Level1_ISR     ref_Lina_Level1_ISR
#define SCI_RXD_isr         ref_SCI_RXD_isr
#define SCI_TXD_isr         ref_SCI_TXD_isr
#define SciDeal             ref_SciDeal
#define SciErrCheck         ref_SciErrCheck
#define UpdateSciFormat     ref_UpdateSciFormat
#define aoComm              ref_aoComm
#define canLinkTicker       ref_canLinkTicker
#define closeRTX            ref_closeRTX
#define commErrorDeal       ref_commErrorDeal
#define commParitys         ref_commParitys
#define commProtocol        ref_commProtocol
#define commRcvData         ref_commRcvData
#define commReadData        ref_commReadData
#define commRunCmd          ref_commRunCmd
#define commSendData        ref_commSendData
#define commStatus          ref_commStatus
#define commStatusDeal      ref_commStatusDeal
#define commTicker          ref_commTicker
#define commType            ref_commType
#define companyPwdPass4Comm ref_companyPwdPass4Comm
#define doComm              ref_doComm
#define dspBaudRegData      ref_dspBaudRegData
#define groupHidePassComm   ref_groupHidePassComm
#define protocolFunc        ref_protocolFunc
#define rcvFrame            ref_rcvFrame
#define resetLinSci         ref_resetLinSci
#define sciFlag             ref_sciFlag
#define sendFrame           ref_sendFrame
#define setRxConfig         ref_setRxConfig
#define setTxConfig         ref_setTxConfig
#define userPwdPass4Comm    ref_userPwdPass4Comm

#include "f_comm_ref.c"
