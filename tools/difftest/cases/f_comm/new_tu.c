/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define COMM_ERR_INDEX      new_COMM_ERR_INDEX
#define CommDataReRcv       new_CommDataReRcv
#define CommDataSend        new_CommDataSend
#define CommRcvDataDeal     new_CommRcvDataDeal
#define CommStartSend       new_CommStartSend
#define CommRead            new_CommRead
#define CommRwFuncCode      new_CommRwFuncCode
#define CommSendDataDeal    new_CommSendDataDeal
#define CommWrite           new_CommWrite
#define InitSciaGpio        new_InitSciaGpio
#define InitSetScia         new_InitSetScia
#define Lina_Level0_ISR     new_Lina_Level0_ISR
#define Lina_Level1_ISR     new_Lina_Level1_ISR
#define SCI_RXD_isr         new_SCI_RXD_isr
#define SCI_TXD_isr         new_SCI_TXD_isr
#define SciDeal             new_SciDeal
#define SciErrCheck         new_SciErrCheck
#define UpdateSciFormat     new_UpdateSciFormat
#define aoComm              new_aoComm
#define canLinkTicker       new_canLinkTicker
#define closeRTX            new_closeRTX
#define commErrorDeal       new_commErrorDeal
#define commParitys         new_commParitys
#define commProtocol        new_commProtocol
#define commRcvData         new_commRcvData
#define commReadData        new_commReadData
#define commRunCmd          new_commRunCmd
#define commSendData        new_commSendData
#define commStatus          new_commStatus
#define commStatusDeal      new_commStatusDeal
#define commTicker          new_commTicker
#define commType            new_commType
#define companyPwdPass4Comm new_companyPwdPass4Comm
#define doComm              new_doComm
#define dspBaudRegData      new_dspBaudRegData
#define groupHidePassComm   new_groupHidePassComm
#define protocolFunc        new_protocolFunc
#define rcvFrame            new_rcvFrame
#define resetLinSci         new_resetLinSci
#define sciFlag             new_sciFlag
#define sendFrame           new_sendFrame
#define setRxConfig         new_setRxConfig
#define setTxConfig         new_setTxConfig
#define userPwdPass4Comm    new_userPwdPass4Comm

#include "f_comm.c"
