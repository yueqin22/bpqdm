/* f_comm 差分测试驱动。
 *
 * 被测模块是串行通讯层: SCI/LIN 寄存器编程 + 报文分发 + funcCode 的远程读写。
 * 它的"能做什么"分两半:
 *
 *   1. 写模块自己的状态 (commStatus / commRcvData / commSendData / sendFrame /
 *      rcvFrame / commReadData / sciFlag / aoComm / doComm / commRunCmd /
 *      三个密码通过标志 / commTicker)。
 *   2. 写外设寄存器 (LinaRegs 二十多个位域、GpioCtrlRegs、GpioDataRegs、
 *      PieCtrlRegs、PieVectTable、IER) 和 EALLOW/EDIS 的 CPU 控制序列。
 *
 * 两半都必须进比较镜像。只比状态会漏掉"寄存器写错了/写丢了", 只比寄存器会漏掉
 * 报文层。所以每个用例都做完整快照:
 *
 *   模块状态  逐字段 (结构体整体 memcpy)
 *   LinaRegs / GpioCtrlRegs / GpioDataRegs / PieCtrlRegs / IER / gHostCpuCtl
 *             整体按字节比 —— 位域布局来自真器件头, 所以"写对了值写错了寄存器"
 *             和"少写一个 EDIS" 都看得见。
 *   PieVectTable
 *             不按字节比: 两侧装进去的是**各自**的 ISR 地址, 字节必然不同。
 *             折算成编码 0=空 / 1=装的是本侧该装的那个 / 2=装了别的。
 *   mock 调用日志 + 参数化回填 (StartDeal 的 tmp、UpdateFormat 的 baudRate、
 *   SendDataDeal 的 err、ModifyFuncCode 的 index/data、SaveOneFuncCode 排进
 *   EEPROM 写队列的 index) —— 这些是"模块把什么交给了下一层", 不在任何状态
 *   字段里, 只能靠日志观测。
 *
 * ---------------------------------------------------------------------------
 * 每个输入都先填成非平凡值
 *
 * 改写如果不再写某个字段, 该字段应当**立刻**表现为差异, 而不是"恰好两边都还是
 * 0"。所以 install() 之前一律先把 commRcvData 的 14 个字段、commSendData 的 6 个
 * 字段、sendFrame/rcvFrame/commReadData 全填上非零值, 而不是只填这一相要用的
 * 那几个。
 *
 * ---------------------------------------------------------------------------
 * 两个必须钉住的边界 (钉不住就是未定义行为, 不是"少测一点")
 *
 *   commDispIndex[i + low]   CommRead 的 0x1000 支用 `low + data > COMM_PARA_NUM`
 *                            当越界判据, 但循环体读的是 commDispIndex[i + low],
 *                            i 从 0 到 data-1。所以 `low + data <= COMM_PARA_NUM`
 *                            是**必要不充分**的: low=32、data=1 时 i+low=32 落在
 *                            表内 (33 项), 但 low=33、data=1 时
 *                            `33 + 1 > 33` 已经拦住了。驱动把 low 限在 0..32、
 *                            data 限在 0..32 且 low+data<=33 —— 正好是模块自己
 *                            的判据允许的最大集合。
 *
 *   dspBaudRegData[tmp]      UpdateSciFormat 用
 *                            tmp = digit[commType] (+9, 当协议是 PROFIBUS)
 *                            去索引 13 项的 dspBaudRegData[]。digit[] 是个位/十位,
 *                            所以 PROFIBUS 时 tmp = 十位+9, 要 <= 12 就得让
 *                            commBaudRate 的十位 <= 3。驱动的取值集合是
 *                            {0..39}, 刚好把 tmp 限制在 9..12。
 *
 * ---------------------------------------------------------------------------
 * 为什么不比 funcCode 的"只读性"
 *
 * CommRwFuncCode 的 U3 组会写 funcCode.all[index], 所以 funcCode 是**输出**,
 * 必须整体比对。它同时又是输入 (密码字段、paraInitMode、saveUserParaFlag 决定
 * 走哪一支), 所以每例都整体重新灌一遍。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "f_comm.h"

#define FRAME_N       35
#define READ_N        RTU_READ_DATA_NUM_MAX
#define AO_N          (AO_NUMBER + 1)
#define LOG_N         64
#define FAIL_LIMIT    20

/* f_comm.c 私有的通讯地址映射。这些 #define 在 **.c 文件里** (f_comm.c:145-154),
 * 头文件看不到, 所以驱动只能照抄一遍数值。
 *
 * 照抄的代价要说清: 如果哪天模块改了某个地址, 这里的副本不会跟着变, 于是驱动
 * 开始测一个"模块已经不认的地址"。这**不会**造成假通过 (两侧拿到同一个 addr,
 * 走的还是同一条分支, 差异仍然可比), 只会让那一支的覆盖悄悄降级。所以负向对照
 * 里特意给每个地址常量都留了变异 (把 `COMM_DO_ADDR` 改成别的值), 一旦副本失配,
 * 那几条会先 MISSED 出来。 */
#define COMM_SET_VALUE_ADDR     0x1000
#define COMM_CMD1_ADDR          0x2000
#define COMM_STATUS_ADDR        0x3000
#define COMM_DO_ADDR            0x2001
#define COMM_HDO_ADDR           0x2004
#define COMM_AO1_ADDR           0x2002
#define COMM_AO2_ADDR           0x2003
#define COMM_INV_ERROR          0x8000
#define COMM_KEYBORD_TEST       0xFFFF

/* 读写模式: 同样是 f_comm.c:124-125 的私有 #define, 不是头文件里的。 */
#define COMM_READ_FC    0
#define COMM_WRITE_FC   1

/* ------------------------------------------------------------------------- */
/* 两侧的符号                                                                */
/* ------------------------------------------------------------------------- */

extern enum COMM_STATUS ref_commStatus;
extern enum COMM_STATUS new_commStatus;
extern COMM_SEND_DATA   ref_commSendData;
extern COMM_SEND_DATA   new_commSendData;
extern COMM_RCV_DATA    ref_commRcvData;
extern COMM_RCV_DATA    new_commRcvData;
extern Uint16           ref_commType;
extern Uint16           new_commType;
extern Uint16           ref_commProtocol;
extern Uint16           new_commProtocol;
extern Uint32           ref_commTicker;
extern Uint32           new_commTicker;
extern Uint32           ref_canLinkTicker;
extern Uint32           new_canLinkTicker;
extern Uint16           ref_commRunCmd;
extern Uint16           new_commRunCmd;
extern Uint16           ref_aoComm[AO_N];
extern Uint16           new_aoComm[AO_N];
extern union DO_SCI     ref_doComm;
extern union DO_SCI     new_doComm;
extern Uint16           ref_userPwdPass4Comm;
extern Uint16           new_userPwdPass4Comm;
extern Uint16           ref_companyPwdPass4Comm;
extern Uint16           new_companyPwdPass4Comm;
extern Uint16           ref_groupHidePassComm;
extern Uint16           new_groupHidePassComm;
extern Uint16           ref_sendFrame[FRAME_N];
extern Uint16           new_sendFrame[FRAME_N];
extern Uint16           ref_rcvFrame[FRAME_N];
extern Uint16           new_rcvFrame[FRAME_N];
extern Uint16           ref_commReadData[READ_N];
extern Uint16           new_commReadData[READ_N];
extern union SCI_FLAG   ref_sciFlag;
extern union SCI_FLAG   new_sciFlag;

extern void   ref_SciDeal(void);
extern void   ref_SCI_RXD_isr(void);
extern void   ref_SCI_TXD_isr(void);
extern void   ref_Lina_Level0_ISR(void);
extern void   ref_Lina_Level1_ISR(void);
extern void   ref_CommRcvDataDeal(void);
extern Uint16 ref_SciErrCheck(void);
extern void   ref_CommSendDataDeal(void);
extern Uint16 ref_CommWrite(Uint16 addr, Uint16 data);
extern Uint16 ref_CommRead(Uint16 addr, Uint16 data);
extern Uint16 ref_CommRwFuncCode(Uint16 addr, Uint16 data, Uint16 rwMode);
extern void   ref_CommDataReRcv(Uint16 tmp);
extern void   ref_CommDataSend(void);
extern void   ref_InitSetScia(void);
extern void   ref_UpdateSciFormat(void);
extern void   ref_InitSciaGpio(void);
extern void   ref_commErrorDeal(void);
extern void   ref_commStatusDeal(void);
extern void   ref_resetLinSci(void);
extern void   ref_closeRTX(void);
extern void   ref_setRxConfig(void);
extern void   ref_setTxConfig(void);
extern void   ref_CommStartSend(void);

extern void   new_SciDeal(void);
extern void   new_SCI_RXD_isr(void);
extern void   new_SCI_TXD_isr(void);
extern void   new_Lina_Level0_ISR(void);
extern void   new_Lina_Level1_ISR(void);
extern void   new_CommRcvDataDeal(void);
extern Uint16 new_SciErrCheck(void);
extern void   new_CommSendDataDeal(void);
extern Uint16 new_CommWrite(Uint16 addr, Uint16 data);
extern Uint16 new_CommRead(Uint16 addr, Uint16 data);
extern Uint16 new_CommRwFuncCode(Uint16 addr, Uint16 data, Uint16 rwMode);
extern void   new_CommDataReRcv(Uint16 tmp);
extern void   new_CommDataSend(void);
extern void   new_InitSetScia(void);
extern void   new_UpdateSciFormat(void);
extern void   new_InitSciaGpio(void);
extern void   new_commErrorDeal(void);
extern void   new_commStatusDeal(void);
extern void   new_resetLinSci(void);
extern void   new_closeRTX(void);
extern void   new_setRxConfig(void);
extern void   new_setTxConfig(void);
extern void   new_CommStartSend(void);

/* mock 提供的参数化开关与日志 */
extern Uint16 gMockLog[LOG_N];
extern Uint16 gMockLogNum;
extern Uint16 gMockStartRet[2];
extern Uint16 gMockErrCheckRet[2];
extern Uint16 gMockUpDownRet;
extern Uint16 gMockUpDownData;
extern Uint16 gMockEnterRet;
extern Uint16 gMockFormatBaud[2];
extern Uint16 gMockSendErr[2];
extern Uint16 gMockStartArg[2];
extern void   MockResetLog(void);

extern Uint16 funcCodeOneWriteIndex[];
extern Uint16 funcCodeOneWriteNum;

/* f_ui.h 里的两个键盘量。本模块只读 keyBordValue、只写 keyBordTestFlag, 而
 * f_comm.h 的包含链够不到 f_ui.h (它只到 f_funcCode.h / f_p2p.h), 所以在这里
 * 单独声明。定义在 mock_comm.c。 */
extern Uint16 keyBordTestFlag;
extern Uint16 keyBordValue;

/* ------------------------------------------------------------------------- */
/* 一相的状态镜像                                                            */
/* ------------------------------------------------------------------------- */

struct MSTATE
{
    Uint16         commStatus;
    COMM_SEND_DATA snd;
    COMM_RCV_DATA  rcv;
    Uint16         commType;
    Uint16         commProtocol;
    Uint32         commTicker;
    Uint32         canLinkTicker;
    Uint16         commRunCmd;
    Uint16         ao[AO_N];
    Uint16         doAll;
    Uint16         pwdU;
    Uint16         pwdC;
    Uint16         pwdG;
    Uint16         send[FRAME_N];
    Uint16         rcvF[FRAME_N];
    Uint16         read[READ_N];
    Uint16         sciAll;
};

struct ENV
{
    /* 共享的工程全局量 */
    FUNCCODE_ALL   fc;
    Uint16         runBitAll;
    Uint16         errCode;
    Uint16         errOther;
    Uint16         errInfo;
    Uint16         kbFlag;
    Uint16         kbVal;
    Uint16         p2pRev;
    Uint16         rwMode;
    Uint16         rwModeTmp;
    Uint16         oneIdx[8];
    Uint16         oneNum;
    /* 外设输入 */
    struct LIN_REGS       lin;
    struct GPIO_CTRL_REGS gpioCtrl;
    struct GPIO_DATA_REGS gpioData;
    struct PIE_CTRL_REGS  pie;
    Uint16                ier;
    Uint16                ctl[7];
    /* 替身开关 */
    Uint16         startRet[2];
    Uint16         errCheckRet[2];
    Uint16         upDownRet;
    Uint16         upDownData;
    Uint16         enterRet;
};

struct SIDE
{
    Uint16         ret;
    struct MSTATE  m;
    struct ENV     e;
    Uint16         vec[2];      /* 0=空 1=本侧该装的 2=别的 */
    Uint16         log[LOG_N];
    Uint16         logNum;
    Uint16         startArg[2];
    Uint16         fmtBaud[2];
    Uint16         sendErr[2];
};

static struct SIDE S[2];

static struct MSTATE g_inM;
static struct ENV    g_inE;

/* 每侧自己的两个 ISR 地址, 用来给 PieVectTable 的槽位编码 */
static void *g_lin0[2];
static void *g_lin1[2];

/* ------------------------------------------------------------------------- */
/* 指针表                                                                    */
/* ------------------------------------------------------------------------- */

static enum COMM_STATUS *P_status[2];
static COMM_SEND_DATA   *P_snd[2];
static COMM_RCV_DATA    *P_rcv[2];
static Uint16           *P_commType[2];
static Uint16           *P_commProtocol[2];
static Uint32           *P_ticker[2];
static Uint32           *P_canTicker[2];
static Uint16           *P_runCmd[2];
static Uint16           *P_ao[2];
static union DO_SCI     *P_do[2];
static Uint16           *P_pwdU[2];
static Uint16           *P_pwdC[2];
static Uint16           *P_pwdG[2];
static Uint16           *P_send[2];
static Uint16           *P_rcvF[2];
static Uint16           *P_read[2];
static union SCI_FLAG   *P_sci[2];

static void init_ptrs(void)
{
    P_status[0]       = &ref_commStatus;         P_status[1]       = &new_commStatus;
    P_snd[0]          = &ref_commSendData;       P_snd[1]          = &new_commSendData;
    P_rcv[0]          = &ref_commRcvData;        P_rcv[1]          = &new_commRcvData;
    P_commType[0]     = &ref_commType;           P_commType[1]     = &new_commType;
    P_commProtocol[0] = &ref_commProtocol;       P_commProtocol[1] = &new_commProtocol;
    P_ticker[0]       = &ref_commTicker;         P_ticker[1]       = &new_commTicker;
    P_canTicker[0]    = &ref_canLinkTicker;      P_canTicker[1]    = &new_canLinkTicker;
    P_runCmd[0]       = &ref_commRunCmd;         P_runCmd[1]       = &new_commRunCmd;
    P_ao[0]           = ref_aoComm;              P_ao[1]           = new_aoComm;
    P_do[0]           = &ref_doComm;             P_do[1]           = &new_doComm;
    P_pwdU[0]         = &ref_userPwdPass4Comm;   P_pwdU[1]         = &new_userPwdPass4Comm;
    P_pwdC[0]         = &ref_companyPwdPass4Comm; P_pwdC[1]        = &new_companyPwdPass4Comm;
    P_pwdG[0]         = &ref_groupHidePassComm;  P_pwdG[1]         = &new_groupHidePassComm;
    P_send[0]         = ref_sendFrame;           P_send[1]         = new_sendFrame;
    P_rcvF[0]         = ref_rcvFrame;            P_rcvF[1]         = new_rcvFrame;
    P_read[0]         = ref_commReadData;        P_read[1]         = new_commReadData;
    P_sci[0]          = &ref_sciFlag;            P_sci[1]          = &new_sciFlag;

    g_lin0[0] = (void *)ref_Lina_Level0_ISR;
    g_lin0[1] = (void *)new_Lina_Level0_ISR;
    g_lin1[0] = (void *)ref_Lina_Level1_ISR;
    g_lin1[1] = (void *)new_Lina_Level1_ISR;
}

/* ------------------------------------------------------------------------- */
/* 装 / 取                                                                   */
/* ------------------------------------------------------------------------- */

static void install(int side)
{
    const struct MSTATE *m = &g_inM;
    const struct ENV    *e = &g_inE;
    int i;

    *P_status[side]       = (enum COMM_STATUS)m->commStatus;
    memcpy(P_snd[side],    &m->snd,    sizeof m->snd);
    memcpy(P_rcv[side],    &m->rcv,    sizeof m->rcv);
    *P_commType[side]     = m->commType;
    *P_commProtocol[side] = m->commProtocol;
    *P_ticker[side]       = m->commTicker;
    *P_canTicker[side]    = m->canLinkTicker;
    *P_runCmd[side]       = m->commRunCmd;
    for (i = 0; i < AO_N; i++)
    {
        P_ao[side][i] = m->ao[i];
    }
    P_do[side]->all    = m->doAll;
    *P_pwdU[side]      = m->pwdU;
    *P_pwdC[side]      = m->pwdC;
    *P_pwdG[side]      = m->pwdG;
    memcpy(P_send[side], m->send, sizeof m->send);
    memcpy(P_rcvF[side], m->rcvF, sizeof m->rcvF);
    memcpy(P_read[side], m->read, sizeof m->read);
    P_sci[side]->all   = m->sciAll;

    memcpy(&funcCode, &e->fc, sizeof funcCode);
    runFlag.all = e->runBitAll;
    errorCode   = e->errCode;
    errorOther  = e->errOther;
    errorInfo   = e->errInfo;
    keyBordTestFlag = e->kbFlag;
    keyBordValue    = e->kbVal;
    p2pData.P2PRevData = e->p2pRev;
    funcCodeRwMode    = e->rwMode;
    funcCodeRwModeTmp = e->rwModeTmp;
    for (i = 0; i < 8; i++)
    {
        funcCodeOneWriteIndex[i] = e->oneIdx[i];
    }
    funcCodeOneWriteNum = e->oneNum;

    gMockStartRet[0] = e->startRet[0];
    gMockStartRet[1] = e->startRet[1];
    gMockErrCheckRet[0] = e->errCheckRet[0];
    gMockErrCheckRet[1] = e->errCheckRet[1];
    gMockUpDownRet  = e->upDownRet;
    gMockUpDownData = e->upDownData;
    gMockEnterRet   = e->enterRet;

    /* 外设: 先整体灌入输入镜像, 再把两个向量槽清空。
     * 清空而不是灌入, 是因为两侧要装的是**不同**的函数地址; 灌进去再比字节
     * 会得到必然不同的两个值, 那不是差异而是噪音。装进去什么由 snapshot 折算。 */
    memcpy((void *)&LinaRegs,     &e->lin,      sizeof e->lin);
    memcpy((void *)&GpioCtrlRegs, &e->gpioCtrl, sizeof e->gpioCtrl);
    memcpy((void *)&GpioDataRegs, &e->gpioData, sizeof e->gpioData);
    memcpy((void *)&PieCtrlRegs,  &e->pie,      sizeof e->pie);
    IER = e->ier;
    for (i = 0; i < 7; i++)
    {
        gHostCpuCtl[i] = e->ctl[i];
    }
    PieVectTable.LIN0INTA = 0;
    PieVectTable.LIN1INTA = 0;

    MockResetLog();
    gMockStartArg[0] = 0;
    gMockStartArg[1] = 0;
    gMockFormatBaud[0] = 0;
    gMockFormatBaud[1] = 0;
    gMockSendErr[0] = 0;
    gMockSendErr[1] = 0;
}

static void snapshot(int side)
{
    struct SIDE *s = &S[side];
    struct MSTATE *m = &s->m;
    struct ENV *e = &s->e;
    int i;

    m->commStatus     = (Uint16)*P_status[side];
    memcpy(&m->snd,     P_snd[side], sizeof m->snd);
    memcpy(&m->rcv,     P_rcv[side], sizeof m->rcv);
    m->commType       = *P_commType[side];
    m->commProtocol   = *P_commProtocol[side];
    m->commTicker     = *P_ticker[side];
    m->canLinkTicker  = *P_canTicker[side];
    m->commRunCmd     = *P_runCmd[side];
    for (i = 0; i < AO_N; i++)
    {
        m->ao[i] = P_ao[side][i];
    }
    m->doAll  = P_do[side]->all;
    m->pwdU   = *P_pwdU[side];
    m->pwdC   = *P_pwdC[side];
    m->pwdG   = *P_pwdG[side];
    memcpy(m->send, P_send[side], sizeof m->send);
    memcpy(m->rcvF, P_rcvF[side], sizeof m->rcvF);
    memcpy(m->read, P_read[side], sizeof m->read);
    m->sciAll = P_sci[side]->all;

    memcpy(&e->fc, &funcCode, sizeof funcCode);
    e->runBitAll = runFlag.all;
    e->errCode   = errorCode;
    e->errOther  = errorOther;
    e->errInfo   = errorInfo;
    e->kbFlag    = keyBordTestFlag;
    e->kbVal     = keyBordValue;
    e->p2pRev    = p2pData.P2PRevData;
    e->rwMode    = funcCodeRwMode;
    e->rwModeTmp = funcCodeRwModeTmp;
    for (i = 0; i < 8; i++)
    {
        e->oneIdx[i] = funcCodeOneWriteIndex[i];
    }
    e->oneNum    = funcCodeOneWriteNum;

    memcpy(&e->lin,      (const void *)&LinaRegs,     sizeof e->lin);
    memcpy(&e->gpioCtrl, (const void *)&GpioCtrlRegs, sizeof e->gpioCtrl);
    memcpy(&e->gpioData, (const void *)&GpioDataRegs, sizeof e->gpioData);
    memcpy(&e->pie,      (const void *)&PieCtrlRegs,  sizeof e->pie);
    e->ier = IER;
    for (i = 0; i < 7; i++)
    {
        e->ctl[i] = gHostCpuCtl[i];
    }

    s->vec[0] = (PieVectTable.LIN0INTA == 0) ? 0
              : ((void *)PieVectTable.LIN0INTA == g_lin0[side]) ? 1 : 2;
    s->vec[1] = (PieVectTable.LIN1INTA == 0) ? 0
              : ((void *)PieVectTable.LIN1INTA == g_lin1[side]) ? 1 : 2;

    s->logNum = gMockLogNum;
    for (i = 0; i < LOG_N; i++)
    {
        s->log[i] = gMockLog[i];
    }
    s->startArg[0] = gMockStartArg[0];
    s->startArg[1] = gMockStartArg[1];
    s->fmtBaud[0]  = gMockFormatBaud[0];
    s->fmtBaud[1]  = gMockFormatBaud[1];
    s->sendErr[0]  = gMockSendErr[0];
    s->sendErr[1]  = gMockSendErr[1];
}

/* ------------------------------------------------------------------------- */
/* 比较                                                                      */
/* ------------------------------------------------------------------------- */

static long g_cases = 0;
static int  g_fails = 0;

/* 当前这一例的入参, 供 dump_in 打印 */
static Uint16 g_curAddr;
static Uint16 g_curData;
static Uint16 g_curRw;

static void dump_in(void)
{
    printf("  in: addr=%04X data=%04X rw=%u status=%u ticker=%lu\n",
           (unsigned)g_curAddr, (unsigned)g_curData, (unsigned)g_curRw,
           (unsigned)g_inM.commStatus, (unsigned long)g_inM.commTicker);
    printf("      rcv: slave=%u cmd=%04X a=%04X d=%04X crc=%04X sz=%u n=%u/%u\n",
           (unsigned)g_inM.rcv.slaveAddr, (unsigned)g_inM.rcv.commCmd,
           (unsigned)g_inM.rcv.commAddr, (unsigned)g_inM.rcv.commData,
           (unsigned)g_inM.rcv.crcRcv, (unsigned)g_inM.rcv.crcSize,
           (unsigned)g_inM.rcv.rcvNum, (unsigned)g_inM.rcv.rcvNumMax);
    printf("      sci=%04X commType=%u proto=%u pw=%u/%u/%u\n",
           (unsigned)g_inM.sciAll, (unsigned)g_inM.commType,
           (unsigned)g_inM.commProtocol, (unsigned)g_inM.pwdU,
           (unsigned)g_inM.pwdC, (unsigned)g_inM.pwdG);
    printf("      fc: pwd=%u/%u init=%u flag=%u/%u baud=%u proto=%u sec=%u par=%u\n",
           (unsigned)funcCode.code.userPassword, (unsigned)funcCode.code.factoryPassword,
           (unsigned)funcCode.code.paraInitMode,
           (unsigned)funcCode.code.saveUserParaFlag1, (unsigned)funcCode.code.saveUserParaFlag2,
           (unsigned)funcCode.code.commBaudRate, (unsigned)funcCode.code.commProtocol,
           (unsigned)funcCode.code.commProtocolSec, (unsigned)funcCode.code.commParity);
}

static void report(const char *fn, const char *what)
{
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s: %s\n", fn, what);
    dump_in();
}

#define CMP_F(field, name)                                            \
    if (A->m.field != B->m.field)                                     \
    {                                                                 \
        report(fn, name);                                             \
        printf("  ref=%ld new=%ld\n", (long)A->m.field, (long)B->m.field); \
    }

#define CMP_U(field, name)                                            \
    if (A->m.field != B->m.field)                                     \
    {                                                                 \
        report(fn, name);                                             \
        printf("  ref=%u new=%u\n", (unsigned)A->m.field, (unsigned)B->m.field); \
    }

#define CMP_E(field, name)                                            \
    if (A->e.field != B->e.field)                                     \
    {                                                                 \
        report(fn, name);                                             \
        printf("  ref=%ld new=%ld\n", (long)A->e.field, (long)B->e.field); \
    }

#define CMP_S(field, name)                                            \
    if (A->field != B->field)                                         \
    {                                                                 \
        report(fn, name);                                             \
        printf("  ref=%u new=%u\n", (unsigned)A->field, (unsigned)B->field); \
    }

#define CMP_MEM(a, b, n, name)                                        \
    if (memcmp((a), (b), (n)) != 0)                                   \
    {                                                                 \
        report(fn, name);                                             \
    }

static void compare(const char *fn)
{
    const struct SIDE *A = &S[0];
    const struct SIDE *B = &S[1];

    CMP_S(ret, "return value");

    CMP_U(commStatus, "commStatus");
    CMP_MEM(&A->m.snd, &B->m.snd, sizeof A->m.snd, "commSendData");
    CMP_MEM(&A->m.rcv, &B->m.rcv, sizeof A->m.rcv, "commRcvData");
    CMP_U(commType, "commType");
    CMP_U(commProtocol, "commProtocol");
    CMP_U(commTicker, "commTicker");
    CMP_U(canLinkTicker, "canLinkTicker");
    CMP_U(commRunCmd, "commRunCmd");
    CMP_MEM(A->m.ao, B->m.ao, sizeof A->m.ao, "aoComm");
    CMP_U(doAll, "doComm");
    CMP_U(pwdU, "userPwdPass4Comm");
    CMP_U(pwdC, "companyPwdPass4Comm");
    CMP_U(pwdG, "groupHidePassComm");
    CMP_MEM(A->m.send, B->m.send, sizeof A->m.send, "sendFrame");
    CMP_MEM(A->m.rcvF, B->m.rcvF, sizeof A->m.rcvF, "rcvFrame");
    CMP_MEM(A->m.read, B->m.read, sizeof A->m.read, "commReadData");
    CMP_U(sciAll, "sciFlag");

    CMP_MEM(&A->e.fc, &B->e.fc, sizeof A->e.fc, "funcCode");
    CMP_E(runBitAll, "runFlag");
    CMP_E(errCode, "errorCode");
    CMP_E(errOther, "errorOther");
    CMP_E(errInfo, "errorInfo");
    CMP_E(kbFlag, "keyBordTestFlag");
    CMP_E(kbVal, "keyBordValue");
    CMP_E(p2pRev, "p2pData.P2PRevData");
    CMP_E(rwMode, "funcCodeRwMode");
    CMP_E(rwModeTmp, "funcCodeRwModeTmp");
    CMP_MEM(A->e.oneIdx, B->e.oneIdx, sizeof A->e.oneIdx, "funcCodeOneWriteIndex");
    CMP_E(oneNum, "funcCodeOneWriteNum");

    CMP_MEM(&A->e.lin, &B->e.lin, sizeof A->e.lin, "LinaRegs");
    CMP_MEM(&A->e.gpioCtrl, &B->e.gpioCtrl, sizeof A->e.gpioCtrl, "GpioCtrlRegs");
    CMP_MEM(&A->e.gpioData, &B->e.gpioData, sizeof A->e.gpioData, "GpioDataRegs");
    CMP_MEM(&A->e.pie, &B->e.pie, sizeof A->e.pie, "PieCtrlRegs");
    CMP_E(ier, "IER");
    CMP_MEM(A->e.ctl, B->e.ctl, sizeof A->e.ctl, "gHostCpuCtl (EALLOW/EDIS)");

    CMP_S(vec[0], "PieVectTable.LIN0INTA");
    CMP_S(vec[1], "PieVectTable.LIN1INTA");

    CMP_S(logNum, "mock call log length");
    CMP_MEM(A->log, B->log, sizeof A->log, "mock call log");
    CMP_MEM(A->startArg, B->startArg, sizeof A->startArg, "StartDeal tmp");
    CMP_MEM(A->fmtBaud, B->fmtBaud, sizeof A->fmtBaud, "UpdateFormat baudRate");
    CMP_MEM(A->sendErr, B->sendErr, sizeof A->sendErr, "SendDataDeal err");
}

/* ------------------------------------------------------------------------- */
/* 一例                                                                      */
/* ------------------------------------------------------------------------- */

enum
{
    K_SciDeal = 0, K_RXD, K_TXD, K_L0, K_L1,
    K_RcvDeal, K_ErrCheck, K_SendDeal, K_Write, K_Read, K_RwFc,
    K_ReRcv, K_DataSend, K_InitScia, K_UpdFormat, K_InitGpio,
    K_ErrDeal, K_StatusDeal, K_Reset, K_Close, K_SetRx, K_SetTx, K_StartSend
};

/* 覆盖台账: 每个入口被驱动了多少例。
 *
 * 这里数的是"入口被驱动"而不是"分支被执行" —— 分支是否真的走到, 由负向对照
 * 来证 (每条变异都锚在某个分支里, CAUGHT 就说明那一支确实被喂到过)。这样做是
 * 有意的: 本模块的分支判据大多依赖 funcCodeGradeSum[] 与 GetCodeIndex() 的
 * 联合布局, 在驱动里**重算**一遍等于把被测逻辑抄一遍, 抄错时会谎报命中
 * (MotorVF 的 CU7/CU8/CU9 就是覆盖计数器在错误的宽度上算, 结果谎报 PID 臂
 * 命中, 而模块实际值被截断成 0)。宁可少说, 不可错说。
 */
static long g_kindN[23];

static void run1(const char *fn, int kind, Uint16 a0, Uint16 a1)
{
    int side;

    g_cases++;
    if ((kind >= 0) && (kind < 23))
    {
        g_kindN[kind]++;
    }
    g_curAddr = a0;
    g_curData = a1;
    /* 这里**不能**写 `g_curRw = 0`。
     *
     * 第一版就是那么写的, 后果是 runRwFc() 设好 rw 再调 run1(), run1 立刻把它
     * 清成 0 —— 于是 phase_rwfc 的 2205 个"写"例全部以 rwMode = COMM_READ_FC
     * 调用, CommRwFuncCode 的**整个写分支从来没被执行过**。
     *
     * 它不会表现为"测试台坏了", 而是表现为 7 条只存在于写分支里的变异集体 MISSED
     * (RF9/RF11/RF14/RF17/RF21/RF22/RF23), 看起来像"这些逻辑等价"。定位手法:
     * 在 dump_in 里看 rw 字段 —— 标着 CommRwFuncCode/write 的例打印出 rw=0。
     * 更早一步的线索是 pwd-ok 返回 COMM_ERR_ADDR(4): 写模式 gradeAdd=0, 而读模式
     * gradeAdd=data-1, 0x1234-1 会让 `grade + gradeAdd >= 组内条数` 立刻成立。
     *
     * rw 由 runRwFc() 管: 进前置位, 出后复位。 */

    for (side = 0; side < 2; side++)
    {
        install(side);
        S[side].ret = 0;

        switch (kind)
        {
        case K_SciDeal:     if (side) new_SciDeal();          else ref_SciDeal();          break;
        case K_RXD:         if (side) new_SCI_RXD_isr();      else ref_SCI_RXD_isr();      break;
        case K_TXD:         if (side) new_SCI_TXD_isr();      else ref_SCI_TXD_isr();      break;
        case K_L0:          if (side) new_Lina_Level0_ISR();  else ref_Lina_Level0_ISR();  break;
        case K_L1:          if (side) new_Lina_Level1_ISR();  else ref_Lina_Level1_ISR();  break;
        case K_RcvDeal:     if (side) new_CommRcvDataDeal();  else ref_CommRcvDataDeal();  break;
        case K_ErrCheck:    S[side].ret = side ? new_SciErrCheck() : ref_SciErrCheck();   break;
        case K_SendDeal:    if (side) new_CommSendDataDeal(); else ref_CommSendDataDeal(); break;
        case K_Write:       S[side].ret = side ? new_CommWrite(a0, a1) : ref_CommWrite(a0, a1); break;
        case K_Read:        S[side].ret = side ? new_CommRead(a0, a1) : ref_CommRead(a0, a1); break;
        case K_RwFc:        S[side].ret = side ? new_CommRwFuncCode(a0, a1, g_curRw)
                                               : ref_CommRwFuncCode(a0, a1, g_curRw); break;
        case K_ReRcv:       if (side) new_CommDataReRcv(a0);  else ref_CommDataReRcv(a0);  break;
        case K_DataSend:    if (side) new_CommDataSend();     else ref_CommDataSend();     break;
        case K_InitScia:    if (side) new_InitSetScia();      else ref_InitSetScia();      break;
        case K_UpdFormat:   if (side) new_UpdateSciFormat();  else ref_UpdateSciFormat();  break;
        case K_InitGpio:    if (side) new_InitSciaGpio();     else ref_InitSciaGpio();     break;
        case K_ErrDeal:     if (side) new_commErrorDeal();    else ref_commErrorDeal();    break;
        case K_StatusDeal:  if (side) new_commStatusDeal();   else ref_commStatusDeal();   break;
        case K_Reset:       if (side) new_resetLinSci();      else ref_resetLinSci();      break;
        case K_Close:       if (side) new_closeRTX();         else ref_closeRTX();         break;
        case K_SetRx:       if (side) new_setRxConfig();      else ref_setRxConfig();      break;
        case K_SetTx:       if (side) new_setTxConfig();      else ref_setTxConfig();      break;
        case K_StartSend:   if (side) new_CommStartSend();    else ref_CommStartSend();    break;
        default:            break;
        }

        snapshot(side);
    }

    compare(fn);
}

static void runRwFc(const char *fn, Uint16 addr, Uint16 data, Uint16 rw)
{
    g_curRw = rw;
    run1(fn, K_RwFc, addr, data);
    g_curRw = 0;
}

/* ------------------------------------------------------------------------- */
/* 输入构造助手                                                              */
/* ------------------------------------------------------------------------- */

static void zero_env(void)
{
    memset(&g_inM, 0, sizeof g_inM);
    memset(&g_inE, 0, sizeof g_inE);
}

/* 把模块状态填成"非平凡": 每个字段都有值, 且互不相同。
 * 改写如果漏写某个字段, 它会保留这里的值, 与另一侧写过的值不同 -> 立刻可见。 */
static void prefill_state(void)
{
    int i;

    g_inM.commStatus  = 0;
    g_inM.commType    = 0;
    g_inM.commProtocol = 0;
    g_inM.commTicker  = 0;
    g_inM.canLinkTicker = 0x12345678UL;
    g_inM.commRunCmd  = 0x5A5A;
    g_inM.doAll       = 0x0FFF;
    g_inM.pwdU        = 1;
    g_inM.pwdC        = 1;
    g_inM.pwdG        = 1;
    g_inM.sciAll      = 0;

    g_inM.rcv.slaveAddr        = 1;
    g_inM.rcv.commAddr         = 0x2000;
    g_inM.rcv.commData         = 1;
    g_inM.rcv.rcvNum           = 0;
    g_inM.rcv.rcvFlag          = 0;
    g_inM.rcv.crcRcv           = 0x1234;
    g_inM.rcv.commCmd          = SCI_CMD_READ;
    g_inM.rcv.crcSize          = 6;
    g_inM.rcv.rcvNumMax        = 8;
    g_inM.rcv.frameSpaceTime   = 100;
    g_inM.rcv.delay            = 50;
    g_inM.rcv.rcvCrcErrCounter = 0x0101;
    g_inM.rcv.rcvDataJuageFlag = 0x0202;
    g_inM.rcv.commCmdSaveEeprom = SCI_WRITE_WITH_EEPROM;

    g_inM.snd.sendNumMax = 8;
    g_inM.snd.sendNum    = 1;
    g_inM.snd.commData   = 0x0303;
    g_inM.snd.crcRcv     = 0x0404;
    g_inM.snd.commCmd    = 0x0505;
    g_inM.snd.crcSize    = 0x0606;

    for (i = 0; i < AO_N; i++)
    {
        g_inM.ao[i] = (Uint16)(0x1111 * (i + 1));
    }
    for (i = 0; i < FRAME_N; i++)
    {
        g_inM.send[i] = (Uint16)(0x1000 + i);
        g_inM.rcvF[i] = (Uint16)(0x2000 + i);
    }
    for (i = 0; i < READ_N; i++)
    {
        g_inM.read[i] = (Uint16)(0x3000 + i);
    }
}

static void prefill_env(void)
{
    int i;

    /* funcCode 整体: 每个字节都非零, 于是"某个字段忘了读" 不会被 0 掩盖。 */
    for (i = 0; i < (int)(sizeof funcCode); i++)
    {
        ((unsigned char *)&g_inE.fc)[i] = (unsigned char)(i * 7 + 1);
    }
    g_inE.fc.code.userPassword      = 1111;
    g_inE.fc.code.factoryPassword   = 2222;
    g_inE.fc.code.paraInitMode      = FUNCCODE_paraInitMode_NO_OPERATION;
    g_inE.fc.code.saveUserParaFlag1 = USER_PARA_SAVE_FLAG1;
    g_inE.fc.code.saveUserParaFlag2 = USER_PARA_SAVE_FLAG2;
    g_inE.fc.code.commReadCurrentPoint = 1;
    g_inE.fc.code.commBaudRate      = 3;
    g_inE.fc.code.commProtocol      = 3;
    g_inE.fc.code.commProtocolSec   = MODBUS;
    g_inE.fc.code.commParity        = 0;

    g_inE.runBitAll = 0;
    g_inE.errCode   = 0x00FF;
    g_inE.errOther  = 0x0F0F;
    g_inE.errInfo   = 0xF0F0;
    g_inE.kbFlag    = 0x1111;
    g_inE.kbVal     = 0x2222;
    g_inE.p2pRev    = 0x3333;
    g_inE.rwMode    = FUNCCODE_RW_MODE_NO_OPERATION;
    g_inE.rwModeTmp = 0x4444;
    for (i = 0; i < 8; i++)
    {
        g_inE.oneIdx[i] = (Uint16)(0x5000 + i);
    }
    g_inE.oneNum = 0;

    g_inE.startRet[0] = 1;
    g_inE.startRet[1] = 1;
    g_inE.errCheckRet[0] = 0;
    g_inE.errCheckRet[1] = 0;
    g_inE.upDownRet = COMM_ERR_NONE;
    g_inE.upDownData = 0x6666;
    g_inE.enterRet = COMM_ERR_NONE;

    /* 外设: 全零起步 (目标端 .bss 也是零), 具体相里再按需点值。 */
    memset(&g_inE.lin, 0, sizeof g_inE.lin);
    memset(&g_inE.gpioCtrl, 0, sizeof g_inE.gpioCtrl);
    memset(&g_inE.gpioData, 0, sizeof g_inE.gpioData);
    memset(&g_inE.pie, 0, sizeof g_inE.pie);
    g_inE.ier = 0;
    for (i = 0; i < 7; i++)
    {
        g_inE.ctl[i] = 0;
    }
}

static void reset_case(void)
{
    zero_env();
    prefill_state();
    prefill_env();
}

/* ------------------------------------------------------------------------- */
/* 地址构造: 从"想要的 (group, grade)" 反推一个能落到那里的 addr             */
/* ------------------------------------------------------------------------- */

/* CommRwFuncCode 的解码是
 *     group = (addr >> 8) & 0x0F;
 *     grade = addr & 0xFF;
 * 然后按 high nibble 给 group 加基址。所以同一个 group 可能有多条路走得到,
 * 这里固定选一条 (最直观的那条), 保证 addr -> group 是单值的。
 *
 * 反过来说: **group 65 / 66 (fChk / remember) 从任何 addr 都到不了** ——
 * 解码只产出 0..15 / 17..32 / 33..48 / 49..64 / 67..82 这几个区间, 16 由
 * 0x1F00 特判得到。这是固件地址空间的形状, 不是测试台的洞 (README 里登记)。 */
static Uint16 addr_for_group(Uint16 group, Uint16 grade)
{
    if (group <= 15)
    {
        return (Uint16)((group << 8) | (grade & 0xFF));
    }
    if (FUNCCODE_GROUP_FP == group)
    {
        return (Uint16)(0x1F00 | (grade & 0xFF));
    }
    if ((group >= FUNCCODE_GROUP_A0) && (group <= FUNCCODE_GROUP_A0 + 15))
    {
        return (Uint16)(0xA000 | ((group - FUNCCODE_GROUP_A0) << 8) | (grade & 0xFF));
    }
    if ((group >= FUNCCODE_GROUP_B0) && (group <= FUNCCODE_GROUP_B0 + 15))
    {
        return (Uint16)(0xB000 | ((group - FUNCCODE_GROUP_B0) << 8) | (grade & 0xFF));
    }
    if ((group >= FUNCCODE_GROUP_C0) && (group <= FUNCCODE_GROUP_C0 + 15))
    {
        return (Uint16)(0xC000 | ((group - FUNCCODE_GROUP_C0) << 8) | (grade & 0xFF));
    }
    if ((group >= FUNCCODE_GROUP_U0) && (group <= FUNCCODE_GROUP_U0 + 15))
    {
        return (Uint16)(0x7000 | ((group - FUNCCODE_GROUP_U0) << 8) | (grade & 0xFF));
    }
    return 0xFFFF;      /* 该 group 不可达 */
}

/* 给定一个 funcCode 全局下标, 反推出能把它选中的 addr。
 * 用模块自己的 funcCodeGradeSum[] / funcCodeGradeAll[] 算, 不硬编码任何偏移 ——
 * 硬编码的守卫一旦和表脱节就会静默指到别的组上 (f_modbus / f_plc 都踩过)。 */
static Uint16 addr_for_index(Uint16 idx, Uint16 *ok)
{
    Uint16 g;

    for (g = 0; g < FUNCCODE_GROUP_NUM; g++)
    {
        Uint16 grade;
        Uint16 a;

        if (idx < funcCodeGradeSum[g])
        {
            break;
        }
        grade = (Uint16)(idx - funcCodeGradeSum[g]);
        if (grade >= funcCodeGradeAll[g])
        {
            continue;
        }
        a = addr_for_group(g, grade);
        if (0xFFFF != a)
        {
            if (ok)
            {
                *ok = 1;
            }
            return a;
        }
    }
    if (ok)
    {
        *ok = 0;
    }
    return 0x9000;      /* 兜底: 落到 COMM_ERR_ADDR, 不会读越界 */
}

/* ------------------------------------------------------------------------- */
/* 相 1: 单发入口 (只写寄存器, 无分支或分支极少)                            */
/* ------------------------------------------------------------------------- */

static void phase_single(void)
{
    reset_case();
    run1("InitSciaGpio", K_InitGpio, 0, 0);

    /* 再跑一遍, 这次把被写的寄存器**先填成全 1**。
     *
     * 必要性: prefill_env() 把 gpioCtrl / gpioData / pie / ier 都清成 0, 于是
     * "少写一位" 与 "照写一位 0" 结果相同 —— IG1 (GPAPUD.GPIO14 写成 GPIO15)
     * 在零初值下恒等, 首轮就是这么 MISSED 的。全 1 之后, ref 把 GPAPUD 的
     * bit14 清成 0, 变异体则一位不动, 差异立刻可见。
     *
     * 为什么不干脆把 prefill 全改全 1: PIEIER9 的 INTx3/INTx4 也在这条路径上,
     * 全 1 会让 IG7 (INTx3 写成 INTx4) 变成"两位本来都是 1"而 MISSED。零初值
     * 与全 1 各覆盖一半, 两个都要跑。 */
    reset_case();
    memset(&g_inE.gpioCtrl, 0xFF, sizeof g_inE.gpioCtrl);
    memset(&g_inE.gpioData, 0xFF, sizeof g_inE.gpioData);
    memset(&g_inE.pie,      0xFF, sizeof g_inE.pie);
    g_inE.ier = 0xFFFF;
    run1("InitSciaGpio/ones", K_InitGpio, 0, 0);

    reset_case();
    run1("resetLinSci", K_Reset, 0, 0);

    reset_case();
    run1("closeRTX", K_Close, 0, 0);

    reset_case();
    run1("setRxConfig", K_SetRx, 0, 0);

    reset_case();
    run1("setTxConfig", K_SetTx, 0, 0);

    /* CommStartSend 单独驱动: 它只被 commStatusDeal 的 SCI_SEND_DATA_PREPARE 支
     * 调用一次, 覆盖别靠外层传递 —— 外层一旦被改短路, 这条就静默失去意义。 */
    reset_case();
    g_inM.send[0] = 0xBEEF;
    run1("CommStartSend", K_StartSend, 0, 0);

    reset_case();
    g_inM.commStatus = SCI_SEND_OK;
    run1("commErrorDeal", K_ErrDeal, 0, 0);
}

/* ------------------------------------------------------------------------- */
/* 相 2: InitSetScia                                                         */
/* ------------------------------------------------------------------------- */

static void phase_initscia(void)
{
    static const Uint16 sec[]  = { MODBUS, PROFIBUS, CAN_OPEN, 3, 0xFFFF };
    static const Uint16 par[]  = { 0, 1, 2, 3 };
    static const Uint32 gcr1[] = { 0x00000000UL, 0xFFFFFFFFUL, 0x0000001CUL };
    unsigned i, j, k;

    for (i = 0; i < sizeof sec / sizeof sec[0]; i++)
    {
        for (j = 0; j < sizeof par / sizeof par[0]; j++)
        {
            for (k = 0; k < sizeof gcr1 / sizeof gcr1[0]; k++)
            {
                reset_case();
                g_inE.fc.code.commProtocolSec = sec[i];
                g_inE.fc.code.commParity      = par[j];
                g_inE.lin.SCIGCR1.all          = gcr1[k];
                run1("InitSetScia", K_InitScia, 0, 0);
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* 相 3: UpdateSciFormat                                                     */
/* ------------------------------------------------------------------------- */

static void phase_updformat(void)
{
    /* commBaudRate / commProtocol 都是"数字", GetNumberDigit1 取个位 (MODBUS) 或
     * 十位 (PROFIBUS)。PROFIBUS 时 tmp = 十位 + 9, 而 dspBaudRegData[] 只有 13 项,
     * 所以十位必须 <= 3 —— 驱动取 0..39, 正好是模块判据允许的整段。 */
    static const Uint16 baud[] = { 0, 1, 2, 3, 4, 9, 10, 13, 19, 20, 23, 29,
                                   30, 33, 39, 40, 99 };
    static const Uint16 proto[] = { 0, 1, 2, 3, 9, 10, 12, 19, 23, 30 };
    static const Uint16 sec[] = { MODBUS, PROFIBUS, CAN_OPEN, 3 };
    static const Uint16 flr[] = { 0x0000, 0x0001, 0x0002, 0x0004, 0x0008,
                                  0x0010, 0x0020, 0x000F, 0xFFFF };
    unsigned i, j, k, m;

    for (i = 0; i < sizeof sec / sizeof sec[0]; i++)
    {
        for (j = 0; j < sizeof baud / sizeof baud[0]; j++)
        {
            for (m = 0; m < sizeof flr / sizeof flr[0]; m++)
            {
                reset_case();
                g_inE.fc.code.commProtocolSec = sec[i];
                g_inE.fc.code.commBaudRate    = baud[j];
                g_inE.fc.code.commProtocol    = proto[j % (sizeof proto / sizeof proto[0])];
                g_inE.lin.SCIFLR.all          = flr[m];
                run1("UpdateSciFormat", K_UpdFormat, 0, 0);
            }
        }
    }

    /* commProtocol 单独再扫一遍: 它决定 commProtocol = digit[commType] 的取值。 */
    for (k = 0; k < sizeof proto / sizeof proto[0]; k++)
    {
        reset_case();
        g_inE.fc.code.commProtocolSec = MODBUS;
        g_inE.fc.code.commBaudRate    = 3;
        g_inE.fc.code.commProtocol    = proto[k];
        run1("UpdateSciFormat", K_UpdFormat, 0, 0);

        reset_case();
        g_inE.fc.code.commProtocolSec = PROFIBUS;
        g_inE.fc.code.commBaudRate    = 3;
        g_inE.fc.code.commProtocol    = proto[k];
        run1("UpdateSciFormat", K_UpdFormat, 0, 0);
    }
}

/* ------------------------------------------------------------------------- */
/* 相 4: CommDataSend                                                        */
/* ------------------------------------------------------------------------- */

static void phase_datasend(void)
{
    static const Uint16 num[] = { 0, 1, 6, 7, 8, 9, 34, 35, 0xFFFF };
    static const Uint16 mx[]  = { 0, 1, 8, 35 };
    unsigned i, j;

    for (i = 0; i < sizeof num / sizeof num[0]; i++)
    {
        for (j = 0; j < sizeof mx / sizeof mx[0]; j++)
        {
            reset_case();
            g_inM.snd.sendNum    = num[i];
            g_inM.snd.sendNumMax = mx[j];
            run1("CommDataSend", K_DataSend, 0, 0);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* 相 5: CommDataReRcv                                                       */
/* ------------------------------------------------------------------------- */

static void phase_rerrcv(void)
{
    static const Uint16 n[]   = { 0, 1, 6, 7, 8, 9, 34, 35 };
    static const Uint16 mx[]  = { 0, 1, 8, 35 };
    static const Uint16 flg[] = { 0, 1, 2, 3 };
    unsigned i, j, k;

    for (i = 0; i < sizeof n / sizeof n[0]; i++)
    {
        for (j = 0; j < sizeof mx / sizeof mx[0]; j++)
        {
            for (k = 0; k < sizeof flg / sizeof flg[0]; k++)
            {
                reset_case();
                g_inM.rcv.rcvNum    = n[i];
                g_inM.rcv.rcvNumMax = mx[j];
                g_inM.rcv.rcvFlag   = flg[k];
                g_inM.rcv.rcvDataJuageFlag = 0x0A0A;
                run1("CommDataReRcv", K_ReRcv, 0x00A5, 0);
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* 相 6: CommWrite                                                           */
/* ------------------------------------------------------------------------- */

static void phase_write(void)
{
    static const Uint16 addr[] =
    {
        COMM_KEYBORD_TEST,          /* 0xFFFF */
        COMM_SET_VALUE_ADDR,        /* 0x1000 */
        COMM_P2P_COMM_ADDRESS_DATA, /* 0x1001 */
        COMM_CMD1_ADDR,             /* 0x2000 */
        COMM_DO_ADDR,               /* 0x2001 */
        COMM_AO1_ADDR,              /* 0x2002 */
        COMM_AO2_ADDR,              /* 0x2003 */
        COMM_HDO_ADDR,              /* 0x2004 */
        COMM_STATUS_ADDR,           /* 0x3000 —— CommWrite 不认, 落到 else */
        COMM_INV_ERROR,             /* 0x8000 —— 同上 */
        0x0000, 0x00FF,             /* high 0x0000 -> 进 funcCode 分支 */
        0xF000, 0xF0FF,             /* high 0xF000 */
        0xA000, 0xA0FF,             /* high 0xA000 */
        0x4000, 0x4001,
        0xB000, 0xB0FF,
        0x5000, 0x5001,
        0xC000, 0xC0FF,
        0x6000, 0x6001,
        0x1F00, 0x1FFF,             /* (addr & 0xFF00) == 0x1F00 */
        0x7300, 0x7301,             /* (addr & 0xFF00) == 0x7300 */
        0x7000,                     /* high 0x7000 但不在允许集合 -> COMM_ERR_ADDR */
        0x9000, 0xD000, 0xE000,
    };
    static const Uint16 data[] = { 0, 1, 7, 8, 9, 0x7FFF, 0x8000, 0xFFFF,
                                   10000, 10001, (Uint16)-10000, (Uint16)-10001 };
    unsigned i, j;

    for (i = 0; i < sizeof addr / sizeof addr[0]; i++)
    {
        for (j = 0; j < sizeof data / sizeof data[0]; j++)
        {
            reset_case();
            run1("CommWrite", K_Write, addr[i], data[j]);
        }
    }

    /* COMM_CMD1_ADDR + data == 8 是**有状态**的一支: `canOpen` 是函数内 static,
     * 第一次写 8 走 `canOpen == 0` 那半边 (commRunCmd = 1, canOpen = 1), 第二次
     * 走 else 半边 (commRunCmd = 2, canOpen = 0), 之后在两值之间翻转。
     *
     * 单发一次永远只看到第一半 —— "canOpen 翻转失效" 这种变异会整个 MISSED。
     * 所以这里连发三次, 且**不**在中间 reset_case(): static 不在模块状态里,
     * 装不进去也清不掉, 只能靠调用序列把它推过拐点。 */
    reset_case();
    run1("CommWrite/cmd8-1", K_Write, COMM_CMD1_ADDR, 8);
    run1("CommWrite/cmd8-2", K_Write, COMM_CMD1_ADDR, 8);
    run1("CommWrite/cmd8-3", K_Write, COMM_CMD1_ADDR, 8);
    /* 插一个非 8 的写: 它不该影响 canOpen, 但会暴露"顺手清了状态"这类改写 */
    run1("CommWrite/cmd8-mid", K_Write, COMM_CMD1_ADDR, 3);
    run1("CommWrite/cmd8-4", K_Write, COMM_CMD1_ADDR, 8);
    run1("CommWrite/cmd8-5", K_Write, COMM_CMD1_ADDR, 8);
}

/* ------------------------------------------------------------------------- */
/* 相 7: CommRead                                                            */
/* ------------------------------------------------------------------------- */

static void phase_read(void)
{
    static const Uint16 addr[] =
    {
        0x1000, 0x1001, 0x101F, 0x1020, 0x1021, 0x10FF,   /* (addr&0xFF00)==0x1000 */
        0x1100,                                            /* 落在 0x1100 -> else */
        COMM_STATUS_ADDR,                                  /* 0x3000 */
        COMM_INV_ERROR,                                    /* 0x8000 */
        COMM_KEYBORD_TEST,                                 /* 0xFFFF */
        0xF000, 0xA000, 0xB000, 0xC000, 0x7000, 0x1F00, 0x1F01,
        0x0000, 0x4000, 0x5000, 0x6000, 0x9000, 0xD000,
    };
    /* data 上限卡在 READ_N (12): commReadData[] 只有 12 项, 而 0x1000 支的越界
     * 判据是 `low + data > COMM_PARA_NUM` (33) —— 比缓冲区宽。data = 13 就会写
     * commReadData[12]。那是固件本身的溢出 (目标端同样踩坏内存), 宿主上给不出
     * 字段级证据, 所以这里只走 data <= 12。33 这个边界由下面的 low/len 双重
     * 循环覆盖 (low=21, len=12 -> 33), 那一条正是 `>` 与 `>=` 的唯一分歧点。 */
    static const Uint16 data[] = { 0, 1, 2, 4, 11, 12, 0xFFFF };
    unsigned i, j;

    for (i = 0; i < sizeof addr / sizeof addr[0]; i++)
    {
        for (j = 0; j < sizeof data / sizeof data[0]; j++)
        {
            reset_case();
            run1("CommRead", K_Read, addr[i], data[j]);
        }
    }

    /* 0x1000 支的 low 与 data 组合: 判据是 low + data > COMM_PARA_NUM, 而循环读
     * commDispIndex[i + low] (33 项)。low 取 0..32 且 low+data<=33 是模块自己
     * 判据允许的最大集合, 超出去就是读越界 —— 那是未定义行为, 不算覆盖。 */
    {
        Uint16 low, len;

        for (low = 0; low <= 32; low++)
        {
            for (len = 0; len + low <= 33 && len <= 12; len++)
            {
                reset_case();
                run1("CommRead", K_Read, (Uint16)(0x1000 + low), len);
            }
        }
    }

    /* DISP_OUT_CURRENT 折半支: (i + low) == 4 才走 /10。 */
    {
        Uint16 low;

        for (low = 0; low <= 8; low++)
        {
            reset_case();
            g_inE.fc.code.commReadCurrentPoint = 1;
            run1("CommRead", K_Read, (Uint16)(0x1000 + low), 12);
            reset_case();
            g_inE.fc.code.commReadCurrentPoint = 0;
            run1("CommRead", K_Read, (Uint16)(0x1000 + low), 12);
        }
    }

    /* 状态字支 (COMM_STATUS_ADDR) 的三条臂: 停机 / 正转 / 反转。
     *
     * runFlag 的取值用**位域自己**算, 不手写魔数: `dir` 是第 11 位, 但把它写成
     * 0x0400 就等于把位序抄进驱动, 头文件里插一个成员就会静默指到别的位上
     * (f_ui 的 FU20/FU23 就是这么漏的)。 */
    {
        static const Uint16 dlen[] = { 0, 1, 2 };
        Uint16 run[4];
        unsigned i, j;

        runFlag.all = 0;
        run[0] = runFlag.all;                       /* 停机            */
        runFlag.all = 0;
        runFlag.bit.run = 1;
        runFlag.bit.dir = FORWARD_DIR;
        run[1] = runFlag.all;                       /* 运行 + 正转     */
        runFlag.all = 0;
        runFlag.bit.run = 1;
        runFlag.bit.dir = REVERSE_DIR;
        run[2] = runFlag.all;                       /* 运行 + 反转     */
        run[3] = 0xFFFF;

        for (i = 0; i < 4; i++)
        {
            for (j = 0; j < sizeof dlen / sizeof dlen[0]; j++)
            {
                reset_case();
                g_inE.runBitAll = run[i];
                run1("CommRead/status", K_Read, COMM_STATUS_ADDR, dlen[j]);
            }
        }
    }

    /* 键盘测试支 (COMM_KEYBORD_TEST): 判据是 `keyBordValue == 0x01FF`。
     * 只驱动 data 而不驱动 keyBordValue 的话, "0x01FF 写成 0x00FF" 这类变异
     * 两臂都取 0, 恒等 —— 常量既是"输入"也是"判据"时两边都得驱动。 */
    {
        static const Uint16 kb[] = { 0x01FF, 0x00FF, 0x0000, 0x01FE, 0xFFFF };
        static const Uint16 dlen[] = { 0, 1, 2 };
        unsigned i, j;

        for (i = 0; i < sizeof kb / sizeof kb[0]; i++)
        {
            for (j = 0; j < sizeof dlen / sizeof dlen[0]; j++)
            {
                reset_case();
                g_inE.kbVal = kb[i];
                run1("CommRead/keybord", K_Read, COMM_KEYBORD_TEST, dlen[j]);
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* 相 8: SciErrCheck                                                         */
/* ------------------------------------------------------------------------- */

static void phase_errcheck(void)
{
    unsigned bit;

    /* 一位一条: 每个错误位单独置起来, 确认优先级链上每一支都能单独命中。
     * sciFlag 是 14 个有效位 + 2 个保留位的联合体, 直接按 .all 播撒。 */
    for (bit = 0; bit < 14; bit++)
    {
        reset_case();
        g_inM.sciAll = (Uint16)(1u << bit);
        run1("SciErrCheck", K_ErrCheck, 0, 0);
    }

    /* 全部位置起来: 走优先级最高的那一支 (pwdErr)。 */
    reset_case();
    g_inM.sciAll = 0x3FFF;
    run1("SciErrCheck", K_ErrCheck, 0, 0);

    /* 读支: commData 跨过 RTU_READ_DATA_NUM_MAX 边界, 且 read 位开着。 */
    {
        static const Uint16 n[] = { 0, 1, 11, 12, 13, 0xFFFF };
        static const Uint16 a[] = { 0x1000, 0x1001, 0x1020, 0x3000, 0x8000,
                                    0xFFFF, 0xF000, 0xA000, 0x1F00, 0x1100 };
        unsigned i, j;

        for (i = 0; i < sizeof n / sizeof n[0]; i++)
        {
            for (j = 0; j < sizeof a / sizeof a[0]; j++)
            {
                reset_case();
                g_inM.sciAll = (Uint16)(1u << 0);      /* sciFlag.bit.read */
                g_inM.rcv.commData = n[i];
                g_inM.rcv.commAddr = a[j];
                run1("SciErrCheck", K_ErrCheck, 0, 0);
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* 相 9: CommSendDataDeal                                                    */
/* ------------------------------------------------------------------------- */

static void phase_senddeal(void)
{
    static const Uint16 flg[] = { 0x0000, 0x0001, 0x0002, 0x0004, 0x0008,
                                  0x0010, 0x0020, 0x0040, 0x0080, 0x0100,
                                  0x0200, 0x0400, 0x0800, 0x1000, 0x2000,
                                  0x3FFF, 0xFFFF };
    /* sendNumMax 必须 >= 2。CommSendDataDeal 结尾是
     *     sendFrame[sendNumMax - 2] = crcSend & 0x00ff;
     *     sendFrame[sendNumMax - 1] = crcSend >> 8;
     * 而 sendFrame 只有 35 项。sendNumMax = 0/1 时 `sendNumMax - 2` 在 Uint16 上
     * 回绕成 65534/65535, 直接越界写 —— 宿主上立刻段错误。这不是测试台的洞:
     * 固件在"出帧长度 < 2"时本来就会写坏内存 (目标端会踩到 sendFrame 之后的东西)。
     * 记进 README 的"证明不了的"一节; 驱动只走定义域内的一段。 */
    static const Uint16 mx[] = { 2, 3, 8, 34, 35 };
    unsigned i, j;

    for (i = 0; i < sizeof flg / sizeof flg[0]; i++)
    {
        for (j = 0; j < sizeof mx / sizeof mx[0]; j++)
        {
            reset_case();
            g_inM.sciAll = flg[i];
            g_inM.snd.sendNumMax = mx[j];
            run1("CommSendDataDeal", K_SendDeal, 0, 0);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* 相 10: commStatusDeal                                                     */
/* ------------------------------------------------------------------------- */

static void phase_statusdeal(void)
{
    static const Uint16 st[] = { 0, 1, 2, 3, 4, 5, 0xFFFF };
    static const Uint32 tk[] = { 0, 1, 49, 50, 51, 99, 100, 101, 0xFFFFFFFFUL };
    static const Uint16 tx[] = { 0, 1 };
    static const Uint16 sl[] = { 0, 1 };
    unsigned i, j, k, m;

    for (i = 0; i < sizeof st / sizeof st[0]; i++)
    {
        for (j = 0; j < sizeof tk / sizeof tk[0]; j++)
        {
            for (k = 0; k < sizeof tx / sizeof tx[0]; k++)
            {
                for (m = 0; m < sizeof sl / sizeof sl[0]; m++)
                {
                    reset_case();
                    g_inM.commStatus = st[i];
                    g_inM.commTicker = tk[j];
                    g_inM.rcv.delay  = 50;
                    g_inM.rcv.frameSpaceTime = 100;
                    g_inM.rcv.slaveAddr = sl[m];
                    g_inM.rcv.commCmd = SCI_CMD_WRITE;
                    g_inE.lin.SCIFLR.all = tx[k] ? 0x00000020UL : 0UL;
                    run1("commStatusDeal", K_StatusDeal, 0, 0);
                }
            }
        }
    }

    /* 发送准备态的门是 `(commTicker >= delay) && (commTicker > frameSpaceTime)`。
     *
     * 上面那一轮把 delay 钉成 50、frameSpaceTime 钉成 100, 于是第二项恒假,
     * 第一项的边界 (ticker == delay) **永远观察不到** —— `>=` 改成 `>` 会整个
     * MISSED。要看见它, 必须让 frameSpaceTime < delay, 再把 ticker 扫过 delay。
     * 这属于"两个判据用 && 连在一起时, 只驱动一个等于没驱动"。 */
    {
        static const Uint16 dl[] = { 50, 1, 0xFFFF };
        static const Uint16 fs[] = { 0, 1, 49, 50, 51 };
        static const Uint16 tk[] = { 0, 1, 49, 50, 51, 100, 101, 0xFFFFFFFFUL };
        unsigned i, j, k;

        for (i = 0; i < sizeof dl / sizeof dl[0]; i++)
        {
            for (j = 0; j < sizeof fs / sizeof fs[0]; j++)
            {
                for (k = 0; k < sizeof tk / sizeof tk[0]; k++)
                {
                    reset_case();
                    g_inM.commStatus = SCI_SEND_DATA_PREPARE;
                    g_inM.rcv.delay          = dl[i];
                    g_inM.rcv.frameSpaceTime = fs[j];
                    g_inM.commTicker         = tk[k];
                    g_inM.rcv.slaveAddr      = 1;
                    run1("commStatusDeal/gate", K_StatusDeal, 0, 0);
                }
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* 相 11: CommRcvDataDeal                                                    */
/* ------------------------------------------------------------------------- */

static void phase_rcvdeal(void)
{
    static const Uint16 sl[]  = { 0, 1 };
    static const Uint16 cmd[] = { SCI_CMD_READ, SCI_CMD_WRITE, SCI_CMD_WRITE_RAM,
                                  0x00, 0x04, 0xFFFF };
    static const Uint16 cs[]  = { 0, 1, 2, 6, 8, 34 };
    unsigned i, j, k, m;

    for (i = 0; i < sizeof sl / sizeof sl[0]; i++)
    {
        for (j = 0; j < sizeof cmd / sizeof cmd[0]; j++)
        {
            for (k = 0; k < sizeof cs / sizeof cs[0]; k++)
            {
                /* CRC 对 / 不对两种: 让 crcRcv 分别等于和不等于真 CRC。 */
                for (m = 0; m < 2; m++)
                {
                    reset_case();
                    g_inM.rcv.slaveAddr = sl[i];
                    g_inM.rcv.commCmd   = cmd[j];
                    g_inM.rcv.crcSize   = cs[k];
                    g_inM.rcv.commAddr  = 0x2000;
                    g_inM.rcv.commData  = 3;
                    if (m)
                    {
                        g_inM.rcv.crcRcv = CrcValueByteCalc(g_inM.rcvF, cs[k]);
                    }
                    run1("CommRcvDataDeal", K_RcvDeal, 0, 0);
                }
            }
        }
    }

    /* 写支的错误码映射: `sciFlag.all |= (0x0001 << COMM_ERR_INDEX[writeErr-1])`。
     * 这里让 CommWrite 依次返回**能返回的每一个**错误码, 覆盖整张映射表。
     *
     * 注意有两个码从这条路径**到不了**: COMM_ERR_CMD(2) 与 COMM_ERR_CRC(3)
     * 只由协议层置位 (sciFlag.bit.cmdErr / crcChkErr), CommWrite 永远不返回它们。
     * 所以 COMM_ERR_INDEX[1] 与 [2] 这两项在 CommRcvDataDeal 里不可达 —— 这是
     * 固件本身的性质, 不是测试台的洞; 记录在 README 的"覆盖缺口"一节。 */
    {
        Uint16 idx;

        /* 1 = PWD: 写 userPassword 但值不对 */
        reset_case();
        g_inM.rcv.slaveAddr = 1;
        g_inM.rcv.commCmd   = SCI_CMD_WRITE;
        g_inM.rcv.crcSize   = 6;
        g_inM.rcv.crcRcv    = CrcValueByteCalc(g_inM.rcvF, 6);
        g_inM.rcv.commAddr  = addr_for_index(GetCodeIndex(funcCode.code.userPassword), 0);
        g_inM.rcv.commData  = (Uint16)(g_inE.fc.code.userPassword + 1);
        run1("CommRcvDataDeal/PWD", K_RcvDeal, 0, 0);

        /* 4 = ADDR: 落到 else 的地址 */
        reset_case();
        g_inM.rcv.slaveAddr = 1;
        g_inM.rcv.commCmd   = SCI_CMD_WRITE;
        g_inM.rcv.crcSize   = 6;
        g_inM.rcv.crcRcv    = CrcValueByteCalc(g_inM.rcvF, 6);
        g_inM.rcv.commAddr  = 0x9000;
        g_inM.rcv.commData  = 1;
        run1("CommRcvDataDeal/ADDR", K_RcvDeal, 0, 0);

        /* 5 = PARA: 设定值越界 */
        reset_case();
        g_inM.rcv.slaveAddr = 1;
        g_inM.rcv.commCmd   = SCI_CMD_WRITE;
        g_inM.rcv.crcSize   = 6;
        g_inM.rcv.crcRcv    = CrcValueByteCalc(g_inM.rcvF, 6);
        g_inM.rcv.commAddr  = COMM_SET_VALUE_ADDR;
        g_inM.rcv.commData  = 10001;
        run1("CommRcvDataDeal/PARA", K_RcvDeal, 0, 0);

        /* 6 = READ_ONLY: ModifyFunccodeUpDown 返回只读 */
        reset_case();
        g_inM.rcv.slaveAddr = 1;
        g_inM.rcv.commCmd   = SCI_CMD_WRITE;
        g_inM.rcv.crcSize   = 6;
        g_inM.rcv.crcRcv    = CrcValueByteCalc(g_inM.rcvF, 6);
        g_inM.rcv.commAddr  = addr_for_index(1, 0);
        g_inM.rcv.commData  = 7;
        g_inE.upDownRet     = COMM_ERR_READ_ONLY;
        run1("CommRcvDataDeal/READ_ONLY", K_RcvDeal, 0, 0);

        /* 7 = SYSTEM_LOCKED: FF 组 (工厂参数) 且未通过公司密码 */
        reset_case();
        g_inM.rcv.slaveAddr = 1;
        g_inM.rcv.commCmd   = SCI_CMD_WRITE;
        g_inM.rcv.crcSize   = 6;
        g_inM.rcv.crcRcv    = CrcValueByteCalc(g_inM.rcvF, 6);
        g_inM.rcv.commAddr  = addr_for_group(FUNCCODE_GROUP_FF, 0);
        g_inM.rcv.commData  = 7;
        g_inM.pwdC          = 0;
        run1("CommRcvDataDeal/SYSTEM_LOCKED", K_RcvDeal, 0, 0);

        /* 8 = SAVE_FUNCCODE_BUSY: paraInitMode 且 funcCodeRwMode 非空闲 */
        idx = GetCodeIndex(funcCode.code.paraInitMode);
        reset_case();
        g_inM.rcv.slaveAddr = 1;
        g_inM.rcv.commCmd   = SCI_CMD_WRITE;
        g_inM.rcv.crcSize   = 6;
        g_inM.rcv.crcRcv    = CrcValueByteCalc(g_inM.rcvF, 6);
        g_inM.rcv.commAddr  = addr_for_index(idx, 0);
        g_inM.rcv.commData  = FUNCCODE_paraInitMode_CLEAR_RECORD;
        g_inM.pwdU          = 1;
        g_inE.rwMode        = 5;    /* 任何非 FUNCCODE_RW_MODE_NO_OPERATION 的值 */
        run1("CommRcvDataDeal/BUSY", K_RcvDeal, 0, 0);
    }
}

/* ------------------------------------------------------------------------- */
/* 相 12: 四个中断入口                                                       */
/* ------------------------------------------------------------------------- */

static void phase_isr(void)
{
    static const Uint16 sd[] = { 0, 1 };
    static const Uint16 rf[] = { 0, 1, 2, 3 };
    static const Uint32 vec[] = { 0, 1, 10, 11, 12, 13, 0xFFFFFFFFUL };
    unsigned i, j, k;

    for (i = 0; i < sizeof sd / sizeof sd[0]; i++)
    {
        for (j = 0; j < sizeof rf / sizeof rf[0]; j++)
        {
            reset_case();
            g_inE.startRet[0] = sd[i];
            g_inM.rcv.rcvFlag = rf[j];
            g_inM.rcv.rcvNum  = 3;
            g_inM.rcv.rcvNumMax = 8;
            run1("SCI_RXD_isr", K_RXD, 0x00C3, 0);
        }
    }

    reset_case();
    run1("SCI_TXD_isr", K_TXD, 0, 0);

    for (k = 0; k < sizeof vec / sizeof vec[0]; k++)
    {
        reset_case();
        g_inE.lin.SCIINTVECT0.all = vec[k];
        run1("Lina_Level0_ISR", K_L0, 0, 0);
    }

    reset_case();
    run1("Lina_Level1_ISR", K_L1, 0, 0);
}

/* ------------------------------------------------------------------------- */
/* 相 13: SciDeal                                                            */
/* ------------------------------------------------------------------------- */

static void phase_scideal(void)
{
    /* 30 s 的判据: commTicker >= 30 * TIME_UNIT_MS_PER_SEC * 2。
     * 门槛必须从模块自己的宏算出来, 不能硬编码 —— 硬编码的守卫一旦和宏脱节
     * 就会静默失效 (f_modbus / f_plc 都踩过)。 */
    const Uint32 lim = (Uint32)30 * TIME_UNIT_MS_PER_SEC * 2;
    Uint32 tk[8];
    static const Uint16 ec[] = { 0, 1 };
    static const Uint16 ty[] = { 0, 1 };
    unsigned i, j, k;

    tk[0] = 0;
    tk[1] = lim - 1;
    tk[2] = lim;
    tk[3] = lim + 1;
    tk[4] = lim * 2;
    tk[5] = lim / 2;
    tk[6] = lim / 3;
    tk[7] = 0xFFFFFFFFUL;

    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < sizeof ec / sizeof ec[0]; j++)
        {
            for (k = 0; k < sizeof ty / sizeof ty[0]; k++)
            {
                reset_case();
                g_inM.commTicker = tk[i];
                g_inM.commType   = ty[k];
                g_inE.errCheckRet[0] = ec[j];
                g_inE.errCheckRet[1] = ec[j];
                g_inM.rcv.slaveAddr = 1;
                g_inM.commStatus = SCI_SEND_DATA_PREPARE;
                run1("SciDeal", K_SciDeal, 0, 0);
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* 相 14: CommRwFuncCode —— 模块里唯一同时"读 funcCode 又写 funcCode" 的函数 */
/* ------------------------------------------------------------------------- */

/* 关心的下标: 每一个判据常量各一条, 外加 U3 组 / FF 组 / AF 组各一条。
 * 全部用模块自己的 GetCodeIndex() 算 —— 那是 FUNCCODE_ALL 的成员偏移, 不是
 * 手抄的常数; 头文件里成员顺序一变, 这里跟着变, 不会静默指错。 */
static Uint16 rwfc_index(int which)
{
    switch (which)
    {
    case 0:  return GetCodeIndex(funcCode.code.userPasswordReadOnly);
    case 1:  return GetCodeIndex(funcCode.code.tuneCmd);
    case 2:  return GetCodeIndex(funcCode.code.menuMode);
    case 3:  return GetCodeIndex(funcCode.code.motorFcM2.tuneCmd);
    case 4:  return GetCodeIndex(funcCode.code.motorFcM3.tuneCmd);
    case 5:  return GetCodeIndex(funcCode.code.motorFcM4.tuneCmd);
    case 6:  return GetCodeIndex(funcCode.code.funcParaView);
    case 7:  return GetCodeIndex(funcCode.code.userPassword);
    case 8:  return GetCodeIndex(funcCode.group.u0[4]);
    case 9:  return GetCodeIndex(funcCode.code.factoryPassword);
    case 10: return GetCodeIndex(funcCode.code.paraInitMode);
    case 11: return GetCodeIndex(funcCode.group.u3[0]);
    case 12: return GetCodeIndex(funcCode.group.ff[0]);
    case 13: return GetCodeIndex(funcCode.group.af[0]);
    case 14: return GetCodeIndex(funcCode.group.f0[0]);
    case 15: return GetCodeIndex(funcCode.group.fp[0]);
    case 16: return GetCodeIndex(funcCode.group.b0[0]);
    default: return 0;
    }
}

#define RWFC_N 17

static void phase_rwfc(void)
{
    static const Uint16 wdata[] = { 0, 1, 2, 3, 4, 501, 7, 1430, 1111, 0xFFFF };
    /* 读模式的 data 上限要卡死, 两个理由叠在一起:
     *   1. `for (i = 0; i < data; i++) commReadData[i] = ...` —— commReadData 只有
     *      RTU_READ_DATA_NUM_MAX (12) 项, 而越界判据 `grade + gradeAdd >=
     *      funcCodeGradeComm[group]` 允许到组内条数 (可以远大于 12)。data = 13
     *      就写 commReadData[12] 了。
     *   2. gradeAdd = data - 1 是 **Uint16 运算**。data = 0xFFFF 时 grade + 0xFFFE
     *      回绕成很小的数, 判据于是放行, 接着写 65535 次 —— 宿主直接段错误。
     * 两条都是固件在"主机一次要很多个寄存器"这种输入下**本来就会越界写**;
     * 目标端同样会踩坏内存。记录在 README 的"证明不了的"一节, 驱动只走
     * data <= 12 的那一段。 */
    static const Uint16 rdata[] = { 0, 1, 2, 4, 11, 12 };
    static const Uint16 upRet[] = { COMM_ERR_NONE, COMM_ERR_PARA, COMM_ERR_READ_ONLY, 9 };
    static const Uint16 enRet[] = { COMM_ERR_NONE, COMM_ERR_PARA };
    static const Uint16 rwm[]   = { FUNCCODE_RW_MODE_NO_OPERATION, 1, 5 };
    int which;
    unsigned i, j, k, m;

    for (which = 0; which < RWFC_N; which++)
    {
        Uint16 idx = rwfc_index(which);
        Uint16 ok = 0;
        Uint16 a = addr_for_index(idx, &ok);

        /* 读模式: gradeAdd = data - 1, 判据是 grade + data - 1 >= 组内条数。
         * data 取 0/1/2 跨过边界, 再取一个大值跨到另一侧。 */
        for (j = 0; j < sizeof rdata / sizeof rdata[0]; j++)
        {
            reset_case();
            runRwFc("CommRwFuncCode/read", a, rdata[j], COMM_READ_FC);
        }

        /* 写模式: 密码支 / 只读支 / paraInitMode 支 / U3 直写支 / EEPROM 支 */
        for (j = 0; j < sizeof wdata / sizeof wdata[0]; j++)
        {
            for (k = 0; k < sizeof upRet / sizeof upRet[0]; k++)
            {
                for (m = 0; m < sizeof rwm / sizeof rwm[0]; m++)
                {
                    reset_case();
                    g_inE.upDownRet = upRet[k];
                    g_inE.upDownData = wdata[j];
                    g_inE.enterRet = enRet[(j + k) % 2];
                    g_inE.rwMode = rwm[m];
                    g_inM.pwdU = (Uint16)(j & 1);
                    g_inM.pwdC = (Uint16)((j >> 1) & 1);
                    g_inM.pwdG = (Uint16)((j >> 2) & 1);
                    g_inM.rcv.commCmdSaveEeprom =
                        (Uint16)((j & 1) ? SCI_WRITE_WITH_EEPROM : SCI_WRITE_NO_EEPROM);
                    runRwFc("CommRwFuncCode/write", a, wdata[j], COMM_WRITE_FC);
                }
            }
        }
    }

    /* 密码支的三种结果: 值对 / 值错。上面 wdata 里已经包含 1430 (公司密码) 与
     * 1111 (用户密码), 这里再把 funcCode 里的密码改成别的值, 让"值错"一侧确定
     * 命中, 而不是靠巧合。 */
    {
        Uint16 idxU = GetCodeIndex(funcCode.code.userPassword);
        Uint16 idxC = GetCodeIndex(funcCode.code.factoryPassword);

        reset_case();
        g_inE.fc.code.userPassword = 0x1234;
        runRwFc("CommRwFuncCode/pwd-ok", addr_for_index(idxU, 0), 0x1234, COMM_WRITE_FC);

        reset_case();
        g_inE.fc.code.userPassword = 0x1234;
        runRwFc("CommRwFuncCode/pwd-bad", addr_for_index(idxU, 0), 0x1235, COMM_WRITE_FC);

        reset_case();
        runRwFc("CommRwFuncCode/cpwd-ok", addr_for_index(idxC, 0), COMPANY_PASSWORD, COMM_WRITE_FC);

        reset_case();
        runRwFc("CommRwFuncCode/cpwd-bad", addr_for_index(idxC, 0), (Uint16)(COMPANY_PASSWORD + 1),
                COMM_WRITE_FC);
    }

    /* paraInitMode 的六条支: 逐条给值。CLEAR_RECORD 走 ClearRecordDeal,
     * RESTORE_USER_PARA 还要看两个 saveUserParaFlag 是否都对。 */
    {
        static const Uint16 pim[] = { FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA,
                                      FUNCCODE_paraInitMode_CLEAR_RECORD,
                                      FUNCCODE_paraInitMode_RESTORE_COMPANY_PARA_ALL,
                                      FUNCCODE_paraInitMode_SAVE_USER_PARA,
                                      FUNCCODE_paraInitMode_RESTORE_USER_PARA,
                                      0x7777 };
        Uint16 idx = GetCodeIndex(funcCode.code.paraInitMode);
        Uint16 a = addr_for_index(idx, 0);

        for (i = 0; i < sizeof pim / sizeof pim[0]; i++)
        {
            for (j = 0; j < 2; j++)         /* 两个 saveUserParaFlag 对 / 不对 */
            {
                reset_case();
                g_inE.fc.code.saveUserParaFlag1 = USER_PARA_SAVE_FLAG1;
                g_inE.fc.code.saveUserParaFlag2 = j ? USER_PARA_SAVE_FLAG2 : 0;
                g_inM.pwdU = 1;
                g_inE.rwMode = FUNCCODE_RW_MODE_NO_OPERATION;
                /* 必须同时把 upDownData 设成 pim[i]。
                 *
                 * 理由: `ModifyFunccodeUpDown(index, &data, 0)` 会把 *data **写回**
                 * gMockUpDownData, 而 paraInitMode 的那串 `else if (... == data)`
                 * 用的是**回填之后**的 data。只把 pim[i] 当入参传进去, 函数体里
                 * 看到的却是 prefill 的 0x6666 —— 六条支一条都进不去,
                 * RF21/RF22/RF23 全部 MISSED, 而且 ret 仍是 0, 看起来像"等价"。 */
                g_inE.upDownData = pim[i];
                runRwFc("CommRwFuncCode/paraInit", a, pim[i], COMM_WRITE_FC);

                reset_case();
                g_inE.fc.code.saveUserParaFlag1 = USER_PARA_SAVE_FLAG1;
                g_inE.fc.code.saveUserParaFlag2 = j ? USER_PARA_SAVE_FLAG2 : 0;
                g_inM.pwdU = 0;             /* 未通过用户密码 -> SYSTEM_LOCKED */
                g_inE.upDownData = pim[i];
                runRwFc("CommRwFuncCode/paraInit-locked", a, pim[i], COMM_WRITE_FC);
            }
        }
    }

    /* highH >= 0xA000 与 commCmdSaveEeprom 的组合: 决定要不要 SaveOneFuncCode。
     * 0xA000 / 0xB000 / 0xC000 都满足 >= 0xA000; 0x7000 / 0x1F00 不满足。 */
    {
        static const Uint16 hi[] = { 0xA000, 0xB000, 0xC000, 0x7000, 0x1F00, 0xF000, 0x0000 };

        for (i = 0; i < sizeof hi / sizeof hi[0]; i++)
        {
            for (j = 0; j < 2; j++)
            {
                reset_case();
                g_inM.pwdG = 1;
                g_inM.pwdU = 1;
                g_inE.enterRet = COMM_ERR_NONE;
                g_inE.rwMode = FUNCCODE_RW_MODE_NO_OPERATION;
                g_inM.rcv.commCmdSaveEeprom =
                    j ? SCI_WRITE_WITH_EEPROM : SCI_WRITE_NO_EEPROM;
                runRwFc("CommRwFuncCode/eeprom", (Uint16)(hi[i] | 0x0001), 9, COMM_WRITE_FC);
            }
        }
    }

    /* AF 组的隐藏开关: 只有 groupHidePassComm 非零才放行。 */
    {
        Uint16 idx = GetCodeIndex(funcCode.group.af[0]);
        Uint16 a = addr_for_index(idx, 0);

        for (j = 0; j < 2; j++)
        {
            reset_case();
            g_inM.pwdG = (Uint16)j;
            runRwFc("CommRwFuncCode/af", a, 9, COMM_WRITE_FC);
        }
    }

    /* U3 组直写: 不看密码、不看 EEPROM, 直接写 funcCode.all[index]。 */
    {
        Uint16 idx = GetCodeIndex(funcCode.group.u3[0]);
        Uint16 a = addr_for_index(idx, 0);

        reset_case();
        runRwFc("CommRwFuncCode/u3", a, 0xA5A5, COMM_WRITE_FC);
    }

    /* rwMode 的第三值: 既不是 READ 也不是 WRITE。模块里 `if (COMM_READ_FC == rwMode)`
     * 两处都按"非读即写"处理, 所以这条会落到写路径。 */
    reset_case();
    runRwFc("CommRwFuncCode/rwmode-other", addr_for_index(GetCodeIndex(funcCode.group.f0[0]), 0),
            5, 2);
}

/* ------------------------------------------------------------------------- */
/* 相 15: 随机                                                               */
/* ------------------------------------------------------------------------- */

static unsigned long g_rnd = 0x2545F491UL;

static Uint32 rnd32(void)
{
    g_rnd ^= (g_rnd << 13) & 0xFFFFFFFFUL;
    g_rnd ^= (g_rnd >> 17);
    g_rnd ^= (g_rnd << 5) & 0xFFFFFFFFUL;
    return (Uint32)g_rnd;
}

static Uint16 rnd16(void)
{
    return (Uint16)rnd32();
}

static void phase_random(long n)
{
    long i;

    for (i = 0; i < n; i++)
    {
        int kind = (int)(rnd32() % 23);
        Uint16 a0;
        Uint16 a1;
        Uint16 rw;

        reset_case();

        /* 随机状态: 让"某一支只在某种字段组合下才走得到" 也有机会被撞上。 */
        g_inM.commStatus = (Uint16)(rnd32() % 6);
        g_inM.commType   = (Uint16)(rnd32() % 2);
        g_inM.commTicker = rnd32();
        g_inM.sciAll     = rnd16();
        g_inM.pwdU       = (Uint16)(rnd32() & 1);
        g_inM.pwdC       = (Uint16)(rnd32() & 1);
        g_inM.pwdG       = (Uint16)(rnd32() & 1);
        g_inM.rcv.slaveAddr      = (Uint16)(rnd32() & 1);
        g_inM.rcv.commCmd        = rnd16();
        g_inM.rcv.commAddr       = rnd16();
        g_inM.rcv.commData       = rnd16();
        g_inM.rcv.rcvNum         = (Uint16)(rnd32() % 36);
        g_inM.rcv.rcvNumMax      = (Uint16)(rnd32() % 36);
        g_inM.rcv.rcvFlag        = (Uint16)(rnd32() % 4);
        g_inM.rcv.delay          = rnd16();
        g_inM.rcv.frameSpaceTime = rnd16();
        g_inM.snd.sendNum        = (Uint16)(rnd32() % 36);
        /* sendNumMax 必须 >= 2, 理由同 phase_senddeal 里 mx[] 的注释:
         * CommSendDataDeal 结尾写 sendFrame[sendNumMax - 2] / [sendNumMax - 1],
         * sendNumMax = 0/1 时下标在 Uint16 上回绕成 65534/65535, 直接越界写。
         * 第一版随机相写的是 `% 36`, 于是第 705 例就撞上 sendNumMax < 2 ——
         * 进程崩在 STATUS_ACCESS_VIOLATION (rc = 3221225477), 而总账把
         * "rc != 0" 一律记成 CAUGHT, 于是 129 条里 128 条都"抓到"了。
         * 那不是证据, 是崩溃。随机相必须留在定义域内。 */
        g_inM.snd.sendNumMax     = (Uint16)(2 + rnd32() % 34);
        g_inE.fc.code.commProtocolSec = (Uint16)(rnd32() % 4);
        g_inE.fc.code.commBaudRate    = (Uint16)(rnd32() % 40);
        g_inE.fc.code.commProtocol    = (Uint16)(rnd32() % 40);
        g_inE.fc.code.commParity      = (Uint16)(rnd32() % 4);
        g_inE.fc.code.commReadCurrentPoint = (Uint16)(rnd32() & 1);
        g_inE.fc.code.paraInitMode    = (Uint16)(rnd32() % 6);
        g_inE.fc.code.userPassword    = rnd16();
        g_inE.fc.code.factoryPassword = rnd16();
        g_inE.upDownRet   = (Uint16)(rnd32() % 10);
        g_inE.upDownData  = rnd16();
        g_inE.enterRet    = (Uint16)(rnd32() % 3);
        g_inE.rwMode      = (Uint16)(rnd32() % 6);
        g_inE.startRet[0] = (Uint16)(rnd32() & 1);
        g_inE.errCheckRet[0] = (Uint16)(rnd32() & 1);
        g_inE.lin.SCIFLR.all = rnd32();
        g_inE.lin.SCIGCR1.all = rnd32();
        g_inE.lin.SCIINTVECT0.all = rnd32();
        g_inE.ier = rnd16();

        a0 = rnd16();
        a1 = rnd16();
        rw = (Uint16)(rnd32() % 2);

        /* 地址空间很宽, 纯随机几乎撞不到特殊地址; 一半的例子里挑一个特殊地址,
         * 另一半才用纯随机 —— 这样"随机"补的是字段组合, 不是地址空间。 */
        if (rnd32() & 1)
        {
            static const Uint16 sp[] =
            {
                COMM_KEYBORD_TEST, COMM_SET_VALUE_ADDR, COMM_P2P_COMM_ADDRESS_DATA,
                COMM_CMD1_ADDR, COMM_DO_ADDR, COMM_AO1_ADDR, COMM_AO2_ADDR,
                COMM_HDO_ADDR, COMM_STATUS_ADDR, COMM_INV_ERROR,
                0x1000, 0x1010, 0x1F00, 0x7300, 0x7000, 0x9000, 0xA000, 0xB000,
                0xC000, 0xF000, 0x0000, 0x4000, 0x5000, 0x6000,
            };
            a0 = sp[rnd32() % (sizeof sp / sizeof sp[0])];
        }

        /* 读路径的 data 必须同时卡两个上限:
         *   1. commReadData[] 只有 RTU_READ_DATA_NUM_MAX (12) 项, 而模块的越界
         *      判据是 `low + data > COMM_PARA_NUM` (33) —— **比缓冲区宽**。
         *      data = 13 就能让 `commReadData[12]` 被写, data = 33 能写到 [32]。
         *      这是固件本身的溢出 (目标端同样踩坏内存), 宿主上无法做字段级比较,
         *      所以驱动只走 data <= 12 这一段。
         *   2. CommRwFuncCode 读模式的 `grade + gradeAdd` 是 Uint16 运算, data 接近
         *      0xFFFF 时会回绕, 让越界判据放行。 */
        if ((K_Read == kind) || ((K_RwFc == kind) && (COMM_READ_FC == rw)))
        {
            if (a1 > READ_N)
            {
                a1 = READ_N;
            }
        }

        if (K_RwFc == kind)
        {
            runRwFc("random/CommRwFuncCode", a0, a1, rw);
        }
        else
        {
            run1("random", kind, a0, a1);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* main                                                                      */
/* ------------------------------------------------------------------------- */

static const char *KIND_NAME[23] =
{
    "SciDeal", "SCI_RXD_isr", "SCI_TXD_isr", "Lina_Level0_ISR", "Lina_Level1_ISR",
    "CommRcvDataDeal", "SciErrCheck", "CommSendDataDeal", "CommWrite", "CommRead",
    "CommRwFuncCode", "CommDataReRcv", "CommDataSend", "InitSetScia", "UpdateSciFormat",
    "InitSciaGpio", "commErrorDeal", "commStatusDeal", "resetLinSci", "closeRTX",
    "setRxConfig", "setTxConfig", "CommStartSend"
};

int main(int argc, char **argv)
{
    long nrand = 2000;
    int i;
    int uncov = 0;

    if (argc > 1)
    {
        nrand = atol(argv[1]);
        if (nrand < 0)
        {
            nrand = 0;
        }
    }

    init_ptrs();

    printf("[phase] single     cases=%ld\n", g_cases);
    fflush(stdout);
    phase_single();
    printf("[phase] initscia   cases=%ld\n", g_cases);
    fflush(stdout);
    phase_initscia();
    printf("[phase] updformat  cases=%ld\n", g_cases);
    fflush(stdout);
    phase_updformat();
    printf("[phase] datasend   cases=%ld\n", g_cases);
    fflush(stdout);
    phase_datasend();
    printf("[phase] rerrcv     cases=%ld\n", g_cases);
    fflush(stdout);
    phase_rerrcv();
    printf("[phase] write      cases=%ld\n", g_cases);
    fflush(stdout);
    phase_write();
    printf("[phase] read       cases=%ld\n", g_cases);
    fflush(stdout);
    phase_read();
    printf("[phase] errcheck   cases=%ld\n", g_cases);
    fflush(stdout);
    phase_errcheck();
    printf("[phase] senddeal   cases=%ld\n", g_cases);
    fflush(stdout);
    phase_senddeal();
    printf("[phase] statusdeal cases=%ld\n", g_cases);
    fflush(stdout);
    phase_statusdeal();
    printf("[phase] rcvdeal    cases=%ld\n", g_cases);
    fflush(stdout);
    phase_rcvdeal();
    printf("[phase] isr        cases=%ld\n", g_cases);
    fflush(stdout);
    phase_isr();
    printf("[phase] scideal    cases=%ld\n", g_cases);
    fflush(stdout);
    phase_scideal();
    printf("[phase] rwfc       cases=%ld\n", g_cases);
    fflush(stdout);
    phase_rwfc();
    printf("[phase] random     cases=%ld\n", g_cases);
    fflush(stdout);
    phase_random(nrand);

    printf("\n---- coverage (每个入口被驱动了多少例) ----\n");
    for (i = 0; i < 23; i++)
    {
        printf("  %-20s %ld\n", KIND_NAME[i], g_kindN[i]);
        if (0 == g_kindN[i])
        {
            printf("  !! 未覆盖: %s\n", KIND_NAME[i]);
            uncov++;
        }
    }

    printf("\ncases=%ld mismatches=%d\n", g_cases, g_fails);
    if (g_fails >= FAIL_LIMIT)
    {
        printf("(mismatch 输出已截断于 %d 条)\n", FAIL_LIMIT);
    }
    if (uncov)
    {
        printf("未覆盖入口 %d 个 -> 覆盖门失败\n", uncov);
    }
    return (g_fails || uncov) ? 1 : 0;
}
