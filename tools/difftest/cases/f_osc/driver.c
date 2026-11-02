/* f_osc differential bench -- driver.
 *
 * 被测模块: src/function/f_osc.c (4 个外部入口 + 11 个 static 内部函数)
 *
 *   OscSciFunction      主循环每拍调用 (f_main.c:714,720)
 *   OscControlRead      只由 SciDataRxDeal 的 FC_READ_FC_DATA 分支调用
 *   OscControlWriter    只由 SciDataRxDeal 的 FC_WRITE_FC_DATA 分支调用
 *   sciaRxFifoIsr       static interrupt, 地址写进 PieVectTable.SCIRXINTA
 *   sciaTxFifoIsr       static interrupt, 地址写进 PieVectTable.SCITXINTA
 *
 * 其余 9 个 (InitOsc / InitSciOSC / SciDataTX / SciDataRx / SciDataRxDeal /
 * OscDataQcq / OscDataTxDeal / GetOscData / OscSciIoInit / ClearOscBufEnd)
 * 都是 static, driver 叫不到名字, 只能沿真实调用路径走进去。
 *
 * -------------------------------------------------------------------------
 * 比较面
 * -------------------------------------------------------------------------
 * 三块, 缺一不可:
 *
 *   1. 外设寄存器 (SCI 13 个字段 / PIE 4 个 / 两个中断向量 / GPIO 整块 /
 *      SysCtrl 整块) + CPU 控制计数器 gHostCpuCtl[7] + IER。
 *      把 EALLOW/EDIS 计数放进比较面, "少一个 EDIS"、"两对合并成一对" 才看得见 ——
 *      这类错只比结果永远看不出来。
 *
 *   2. 模块的文件级 static 对象 (OscCongData / OscDataBufA / OscDataBufB /
 *      OscConFrameBuf / BaudCfg / SciTxBufStr / SciRxBufStr)。
 *      它们没有外部链接, driver 拿不到名字; ref_tu.c / new_tu.c 在 include 完
 *      源码之后各挂一个快照函数把它们拷出来 (见 shim/osc_state.h)。源码一个字
 *      都没改。
 *
 *   3. 对外写的三个全局量 (sciFlag / commRcvData / commReadData) 与两个入口的
 *      返回值。
 *
 * 函数内 static (OscSciFunction 的 count/countRx/OscCfg、SciDataRx 的
 * count/timeOut/countOut、OscDataQcq 的 count/bufSel、OscDataTxDeal 的
 * frame/bufSel) 拿不到, 靠"两侧从同一个零态出发、跑同一条调用序列"保持同步 ——
 * 与本项目其它用例的做法一致。
 *
 * -------------------------------------------------------------------------
 * 每个 step 的形状
 * -------------------------------------------------------------------------
 *   apply_common(in)      把**所有**共享输入铺成 in 描述的样子 (幂等)
 *   preset_*(...)         需要的话把模块 static 预置成某个可达前置状态
 *   hw_save(&pre)         存下"调用前"的共享状态
 *   call_side(0)          ref 侧调一次
 *   hw_save(&hw[0]); state_get(0)      快照
 *   hw_load(&pre)         把共享状态**原样还原**
 *   call_side(1)          new 侧用**同一份输入**调一次
 *   hw_save(&hw[1]); state_get(1)
 *   比对
 *
 * 还原这一步是整件事的关键: 外设寄存器是共享的, 不还原的话 new 侧看到的是 ref
 * 侧写过的寄存器, 两侧的输入就不一样了。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DSP2803x_Device.h"
#include "osc_state.h"
#include "f_osc.h"
#include "f_comm.h"

/* mock_osc.c 里两个可调的返回值 —— 输入, 两侧共用。 */
extern Uint16 g_commReadRet;
extern Uint16 g_commWriteRet;

/* ------------------------------------------------------------------------- */
/* 被测模块的入口 (ref_tu.c / new_tu.c 里改过名)                              */
/* ------------------------------------------------------------------------- */

void   ref_OscSciFunction(void);
void   new_OscSciFunction(void);
Uint16 ref_OscControlWriter(Uint16 addr, Uint16 data);
Uint16 new_OscControlWriter(Uint16 addr, Uint16 data);
Uint16 ref_OscControlRead(Uint16 addr, Uint16 *result);
Uint16 new_OscControlRead(Uint16 addr, Uint16 *result);

Uint16 CrcValueByteCalc(const Uint16 *data, Uint16 length);

/* ------------------------------------------------------------------------- */
/* 统计                                                                       */
/* ------------------------------------------------------------------------- */

static long g_cases;
static long g_mism;
static long g_mismShown;

#define MAX_SHOW 60

static void report(const char *field, const char *detail)
{
    g_mism++;
    if (g_mismShown < MAX_SHOW)
    {
        g_mismShown++;
        printf("MISMATCH %s: %s\n", field, detail);
    }
}

/* "哪一步失配"这行也要受 MAX_SHOW 约束。
 *
 * 一个强变异 (比如采集基址整体挪一个字) 会让 29089 个用例全失配, 每条都打一行
 * "^ in ..." 就是 21 万行日志 —— 跑一次负向对照要写几百 MB, 而前 60 行已经足够
 * 定位问题。MISMATCH 明细本来就有 MAX_SHOW 上限, 这行漏掉了, 属于同一类遗漏。 */
static void note_where(const char *what)
{
    if (g_mismShown < MAX_SHOW)
    {
        printf("  ^ in %s\n", what);
    }
}

#define CMPU(f)                                                               \
    do {                                                                      \
        if ((unsigned long)a->f != (unsigned long)b->f)                       \
        {                                                                     \
            char d[192];                                                      \
            sprintf(d, "ref=%lu new=%lu", (unsigned long)a->f,                \
                    (unsigned long)b->f);                                     \
            report(#f, d);                                                    \
        }                                                                     \
    } while (0)

/* ------------------------------------------------------------------------- */
/* 覆盖计数                                                                   */
/* ------------------------------------------------------------------------- */

enum
{
    COV_MAIN = 0,
    COV_RXISR,
    COV_TXISR,
    COV_CTRLWRITE,
    COV_CTRLREAD,

    COV_RXISR_ERR,
    COV_RXISR_OK,
    COV_RXISR_LAST,

    COV_FRM_CRC_BAD,
    COV_FRM_HEAD_BAD,
    COV_FRM_STOP,
    COV_FRM_STATUS_BUSY,
    COV_FRM_COMM_TEST,
    COV_FRM_CHANNL,
    COV_FRM_PARA,
    COV_FRM_START,
    COV_FRM_READ,
    COV_FRM_WRITE,
    COV_FRM_DEFAULT,
    COV_FRM_PWDPASS,

    COV_QCGQ_FULL_A,
    COV_QCGQ_FULL_B,
    COV_TX_SENT_A,
    COV_TX_SENT_B,

    COV_RXDIR_ARM,
    COV_RXDIR_TIMEOUT,
    COV_RXDIR_LENDONE,

    COV_BAUDCFG_REINIT,
    COV_STATUS_ON,
    COV_STATUS_OFF,

    COV_ISR_GUARD,

    COV_COUNT
};

static const char *g_covName[COV_COUNT] =
{
    "MAIN", "RXISR", "TXISR", "CTRLWRITE", "CTRLREAD",
    "RXISR_ERR", "RXISR_OK", "RXISR_LAST",
    "FRM_CRC_BAD", "FRM_HEAD_BAD", "FRM_STOP", "FRM_STATUS_BUSY",
    "FRM_COMM_TEST", "FRM_CHANNL", "FRM_PARA", "FRM_START",
    "FRM_READ", "FRM_WRITE", "FRM_DEFAULT", "FRM_PWDPASS",
    "QCGQ_FULL_A", "QCGQ_FULL_B", "TX_SENT_A", "TX_SENT_B",
    "RXDIR_ARM", "RXDIR_TIMEOUT", "RXDIR_LENDONE",
    "BAUDCFG_REINIT", "STATUS_ON", "STATUS_OFF",
    "ISR_GUARD"
};

static long g_cov[COV_COUNT];

#define COV(k) (g_cov[k]++)

/* ------------------------------------------------------------------------- */
/* 共享外设状态快照                                                           */
/* ------------------------------------------------------------------------- */

struct HW
{
    /* SCI */
    unsigned short sciccr, scictl1, scihbaud, scilbaud, scictl2;
    unsigned short scirxst, scirxemu, scirxbuf, scitxbuf;
    unsigned short scifftx, sciffrx, sciffct, scipri;

    /* PIE */
    unsigned short piectrl, pieack, pieier9, pieifr9;

    /* 中断向量: raw 用于还原, kind 用于比较 (两侧地址必然不同) */
    PINT           vecRxRaw, vecTxRaw;
    unsigned short vecRxKind, vecTxKind;

    /* GPIO / SYS: 整块字节快照, 免得漏掉某个位域 */
    unsigned char  gpioCtrl[sizeof(GpioCtrlRegs)];
    unsigned char  gpioData[sizeof(GpioDataRegs)];
    unsigned char  sysCtrl[sizeof(SysCtrlRegs)];

    /* CPU */
    unsigned short ier;
    unsigned short ctl[7];

    /* 对外写的全局量 */
    unsigned short sciFlagAll;
    unsigned short commRcv[14];
    unsigned short commRead[RTU_READ_DATA_NUM_MAX];
};

static struct HW g_hw[2];
static struct HW g_pre;

static struct OSC_STATE g_st[2];
static unsigned short   g_cr[2];

/* 中断向量的规范化。
 *   0 = NULL
 *   1 = 本侧的 RX ISR
 *   2 = 本侧的 TX ISR
 *   3 = 别的什么 (含 driver 铺进去的哨兵) */
static PINT g_rxIsr[2];
static PINT g_txIsr[2];

static void osc_other_isr(void)
{
    /* 哨兵必须是**真函数**。用假地址 (0x1234 之类) 当哨兵时, 任何一次
     * "通过向量表调用"都会立刻访问违例, 整趟跑不下来 —— 而更糟的是,
     * 真函数哨兵顺带能区分"通过指针调用"与"按名字直接调用"。 */
}

static unsigned short vec_kind(PINT p, int side)
{
    if (p == 0)
    {
        return 0;
    }
    if ((g_rxIsr[side] != 0) && (p == g_rxIsr[side]))
    {
        return 1;
    }
    if ((g_txIsr[side] != 0) && (p == g_txIsr[side]))
    {
        return 2;
    }
    return 3;
}

static void bytes_fill(volatile void *p, unsigned n, unsigned seed)
{
    volatile unsigned char *q = (volatile unsigned char *)p;
    unsigned i;

    for (i = 0; i < n; i++)
    {
        q[i] = (unsigned char)((seed + i * 7u) & 0xFFu);
    }
}

static void bytes_copy(unsigned char *dst, volatile const void *src, unsigned n)
{
    volatile const unsigned char *q = (volatile const unsigned char *)src;
    unsigned i;

    for (i = 0; i < n; i++)
    {
        dst[i] = q[i];
    }
}

static void bytes_store(volatile void *dst, const unsigned char *src, unsigned n)
{
    volatile unsigned char *q = (volatile unsigned char *)dst;
    unsigned i;

    for (i = 0; i < n; i++)
    {
        q[i] = src[i];
    }
}

static void hw_save(struct HW *h, int side)
{
    const unsigned short *p;
    int i;

    h->sciccr   = SciaRegs.SCICCR.all;
    h->scictl1  = SciaRegs.SCICTL1.all;
    h->scihbaud = SciaRegs.SCIHBAUD;
    h->scilbaud = SciaRegs.SCILBAUD;
    h->scictl2  = SciaRegs.SCICTL2.all;
    h->scirxst  = SciaRegs.SCIRXST.all;
    h->scirxemu = SciaRegs.SCIRXEMU;
    h->scirxbuf = SciaRegs.SCIRXBUF.all;
    h->scitxbuf = SciaRegs.SCITXBUF;
    h->scifftx  = SciaRegs.SCIFFTX.all;
    h->sciffrx  = SciaRegs.SCIFFRX.all;
    h->sciffct  = SciaRegs.SCIFFCT.all;
    h->scipri   = SciaRegs.SCIPRI.all;

    h->piectrl = PieCtrlRegs.PIECTRL.all;
    h->pieack  = PieCtrlRegs.PIEACK.all;
    h->pieier9 = PieCtrlRegs.PIEIER9.all;
    h->pieifr9 = PieCtrlRegs.PIEIFR9.all;

    h->vecRxRaw = PieVectTable.SCIRXINTA;
    h->vecTxRaw = PieVectTable.SCITXINTA;

    /* 第一次看到一个非哨兵非空的向量就把它记成"本侧的 ISR"。 */
    if ((g_rxIsr[side] == 0) && (h->vecRxRaw != 0) && (h->vecRxRaw != osc_other_isr))
    {
        g_rxIsr[side] = h->vecRxRaw;
    }
    if ((g_txIsr[side] == 0) && (h->vecTxRaw != 0) && (h->vecTxRaw != osc_other_isr))
    {
        g_txIsr[side] = h->vecTxRaw;
    }

    h->vecRxKind = vec_kind(h->vecRxRaw, side);
    h->vecTxKind = vec_kind(h->vecTxRaw, side);

    bytes_copy(h->gpioCtrl, &GpioCtrlRegs, sizeof(GpioCtrlRegs));
    bytes_copy(h->gpioData, &GpioDataRegs, sizeof(GpioDataRegs));
    bytes_copy(h->sysCtrl,  &SysCtrlRegs,  sizeof(SysCtrlRegs));

    h->ier = IER;
    for (i = 0; i < 7; i++)
    {
        h->ctl[i] = gHostCpuCtl[i];
    }

    h->sciFlagAll = sciFlag.all;

    p = (const unsigned short *)&commRcvData;
    for (i = 0; i < 14; i++)
    {
        h->commRcv[i] = p[i];
    }

    for (i = 0; i < RTU_READ_DATA_NUM_MAX; i++)
    {
        h->commRead[i] = commReadData[i];
    }
}

static void hw_load(const struct HW *h)
{
    const unsigned short *p;
    int i;

    SciaRegs.SCICCR.all   = (Uint16)h->sciccr;
    SciaRegs.SCICTL1.all  = (Uint16)h->scictl1;
    SciaRegs.SCIHBAUD     = (Uint16)h->scihbaud;
    SciaRegs.SCILBAUD     = (Uint16)h->scilbaud;
    SciaRegs.SCICTL2.all  = (Uint16)h->scictl2;
    SciaRegs.SCIRXST.all  = (Uint16)h->scirxst;
    SciaRegs.SCIRXEMU     = (Uint16)h->scirxemu;
    SciaRegs.SCIRXBUF.all = (Uint16)h->scirxbuf;
    SciaRegs.SCITXBUF     = (Uint16)h->scitxbuf;
    SciaRegs.SCIFFTX.all  = (Uint16)h->scifftx;
    SciaRegs.SCIFFRX.all  = (Uint16)h->sciffrx;
    SciaRegs.SCIFFCT.all  = (Uint16)h->sciffct;
    SciaRegs.SCIPRI.all   = (Uint16)h->scipri;

    PieCtrlRegs.PIECTRL.all = (Uint16)h->piectrl;
    PieCtrlRegs.PIEACK.all  = (Uint16)h->pieack;
    PieCtrlRegs.PIEIER9.all = (Uint16)h->pieier9;
    PieCtrlRegs.PIEIFR9.all = (Uint16)h->pieifr9;

    PieVectTable.SCIRXINTA = h->vecRxRaw;
    PieVectTable.SCITXINTA = h->vecTxRaw;

    bytes_store(&GpioCtrlRegs, h->gpioCtrl, sizeof(GpioCtrlRegs));
    bytes_store(&GpioDataRegs, h->gpioData, sizeof(GpioDataRegs));
    bytes_store(&SysCtrlRegs,  h->sysCtrl,  sizeof(SysCtrlRegs));

    IER = (Uint16)h->ier;
    for (i = 0; i < 7; i++)
    {
        gHostCpuCtl[i] = (Uint16)h->ctl[i];
    }

    sciFlag.all = (Uint16)h->sciFlagAll;

    p = (const unsigned short *)&commRcvData;
    {
        unsigned short *q = (unsigned short *)&commRcvData;
        (void)p;
        for (i = 0; i < 14; i++)
        {
            q[i] = h->commRcv[i];
        }
    }

    for (i = 0; i < RTU_READ_DATA_NUM_MAX; i++)
    {
        commReadData[i] = (Uint16)h->commRead[i];
    }
}

static void cmp_hw(const struct HW *a, const struct HW *b, const char *what)
{
    long before = g_mism;

    CMPU(sciccr);
    CMPU(scictl1);
    CMPU(scihbaud);
    CMPU(scilbaud);
    CMPU(scictl2);
    CMPU(scirxst);
    CMPU(scirxemu);
    CMPU(scirxbuf);
    CMPU(scitxbuf);
    CMPU(scifftx);
    CMPU(sciffrx);
    CMPU(sciffct);
    CMPU(scipri);

    CMPU(piectrl);
    CMPU(pieack);
    CMPU(pieier9);
    CMPU(pieifr9);

    CMPU(vecRxKind);
    CMPU(vecTxKind);

    if (memcmp(a->gpioCtrl, b->gpioCtrl, sizeof(GpioCtrlRegs)) != 0)
    {
        report("gpioCtrl", "raw bytes differ");
    }
    if (memcmp(a->gpioData, b->gpioData, sizeof(GpioDataRegs)) != 0)
    {
        report("gpioData", "raw bytes differ");
    }
    if (memcmp(a->sysCtrl, b->sysCtrl, sizeof(SysCtrlRegs)) != 0)
    {
        report("sysCtrl", "raw bytes differ");
    }

    CMPU(ier);
    CMPU(ctl[0]);
    CMPU(ctl[1]);
    CMPU(ctl[2]);
    CMPU(ctl[3]);
    CMPU(ctl[4]);
    CMPU(ctl[5]);
    CMPU(ctl[6]);

    CMPU(sciFlagAll);

    {
        int i;
        char d[192];
        for (i = 0; i < 14; i++)
        {
            if (a->commRcv[i] != b->commRcv[i])
            {
                sprintf(d, "[%d] ref=%u new=%u", i, a->commRcv[i], b->commRcv[i]);
                report("commRcv", d);
            }
        }
        for (i = 0; i < RTU_READ_DATA_NUM_MAX; i++)
        {
            if (a->commRead[i] != b->commRead[i])
            {
                sprintf(d, "[%d] ref=%u new=%u", i, a->commRead[i], b->commRead[i]);
                report("commRead", d);
            }
        }
    }

    if (g_mism != before)
    {
        note_where(what);
    }
}

static void cmp_state(const struct OSC_STATE *a, const struct OSC_STATE *b,
                      const char *what)
{
    long before = g_mism;
    int i;
    char d[192];

    for (i = 0; i < OSC_CON_N; i++)
    {
        if (a->con[i] != b->con[i])
        {
            sprintf(d, "[%d] ref=%u new=%u", i, a->con[i], b->con[i]);
            report("con", d);
        }
    }
    for (i = 0; i < OSC_BUF_N; i++)
    {
        if (a->bufA[i] != b->bufA[i])
        {
            sprintf(d, "A[%d] ref=%u new=%u", i, a->bufA[i], b->bufA[i]);
            report("buf", d);
        }
        if (a->bufB[i] != b->bufB[i])
        {
            sprintf(d, "B[%d] ref=%u new=%u", i, a->bufB[i], b->bufB[i]);
            report("buf", d);
        }
    }
    for (i = 0; i < OSC_FRAME_N; i++)
    {
        if (a->frame[i] != b->frame[i])
        {
            sprintf(d, "[%d] ref=%u new=%u", i, a->frame[i], b->frame[i]);
            report("frame", d);
        }
    }

    CMPU(baudCfg);
    CMPU(txBusy);
    CMPU(txLen);
    CMPU(txFlag);
    CMPU(txKind);
    CMPU(txOff);
    CMPU(rxBusy);
    CMPU(rxLen);
    CMPU(rxFlag);
    CMPU(rxKind);
    CMPU(rxOff);

    if (g_mism != before)
    {
        note_where(what);
    }
}

/* ------------------------------------------------------------------------- */
/* 输入                                                                       */
/* ------------------------------------------------------------------------- */

struct IN
{
    unsigned short pat;

    unsigned short rxerr;    /* SCIRXST.bit.RXERROR */
    unsigned short rxdt;     /* SCIRXBUF.bit.RXDT   */
    unsigned short txst;     /* SCIFFTX.bit.TXFFST  */
    unsigned short txempty;  /* SCICTL2.bit.TXEMPTY */

    unsigned short ier;
    unsigned short sciflag;
    unsigned short saveEeprom;
    unsigned short rdRet;
    unsigned short wrRet;
    unsigned short rd[RTU_READ_DATA_NUM_MAX];

    unsigned short cAddr;
    unsigned short cData;

    unsigned short rxLenIn;  /* SciDataRx 的 len 形参 (直接驱动时用) */
    unsigned short rxTo;     /* SciDataRx 的 timeout 形参 */
};

static void in_reset(struct IN *in, unsigned short pat)
{
    int i;

    memset(in, 0, sizeof(*in));
    in->pat      = pat;
    in->ier      = 0x0001;
    in->saveEeprom = SCI_WRITE_NO_EEPROM;
    in->rxLenIn  = 8;
    in->rxTo     = 0;
    for (i = 0; i < RTU_READ_DATA_NUM_MAX; i++)
    {
        in->rd[i] = (unsigned short)(0x5A00u + i);
    }
}

/* funcCode 是模块唯一的只读数据源: GET_OSC_DATA(addr) 读的是
 * OscQuqData[addr] = (&funcCode.group.u0[0])[addr-1]。
 * 每个字都填成地址的函数, "读错一个下标" 立刻显形。 */
static void fill_funccode(unsigned short seed)
{
    int i;

    for (i = 0; i < FNUM_ALL; i++)
    {
        funcCode.all[i] = (Uint16)(((unsigned)i * 2654435761u + seed * 7919u) >> 7);
    }
}

static void apply_common(const struct IN *in)
{
    unsigned short k = in->pat;
    int i;

    /* SCI: 先整块铺花样 (所以"没写"与"写了"可区分), 再放真正的输入。 */
    SciaRegs.SCICCR.all   = (Uint16)(k + 0x0101);
    SciaRegs.SCICTL1.all  = (Uint16)(k + 0x0102);
    SciaRegs.SCIHBAUD     = (Uint16)(k + 0x0103);
    SciaRegs.SCILBAUD     = (Uint16)(k + 0x0104);
    SciaRegs.SCICTL2.all  = (Uint16)(k + 0x0105);
    SciaRegs.SCIRXST.all  = (Uint16)(k + 0x0106);
    SciaRegs.SCIRXEMU     = (Uint16)(k + 0x0107);
    SciaRegs.SCIRXBUF.all = (Uint16)(k + 0x0108);
    SciaRegs.SCITXBUF     = (Uint16)(k + 0x0109);
    SciaRegs.SCIFFTX.all  = (Uint16)(k + 0x010A);
    SciaRegs.SCIFFRX.all  = (Uint16)(k + 0x010B);
    SciaRegs.SCIFFCT.all  = (Uint16)(k + 0x010C);
    SciaRegs.SCIPRI.all   = (Uint16)(k + 0x010D);

    SciaRegs.SCIRXST.bit.RXERROR = in->rxerr;
    SciaRegs.SCIRXBUF.bit.RXDT   = in->rxdt;
    SciaRegs.SCIFFTX.bit.TXFFST  = in->txst;
    SciaRegs.SCICTL2.bit.TXEMPTY = in->txempty;

    /* PIE */
    PieCtrlRegs.PIECTRL.all = (Uint16)(k + 0x0201);
    PieCtrlRegs.PIEACK.all  = (Uint16)(k + 0x0202);
    PieCtrlRegs.PIEIER9.all = (Uint16)(k + 0x0203);
    PieCtrlRegs.PIEIFR9.all = (Uint16)(k + 0x0204);

    PieVectTable.SCIRXINTA = osc_other_isr;
    PieVectTable.SCITXINTA = osc_other_isr;

    /* GPIO / SysCtrl: 整块铺花样, 再放模块会读的那几位 (本模块一位都不读,
     * 全是写; 铺花样是为了让"少写一位"可见)。 */
    bytes_fill(&GpioCtrlRegs, sizeof(GpioCtrlRegs), k);
    bytes_fill(&GpioDataRegs, sizeof(GpioDataRegs), (unsigned)(k + 3));
    bytes_fill(&SysCtrlRegs,  sizeof(SysCtrlRegs),  (unsigned)(k + 5));

    /* CPU */
    IER = (Uint16)in->ier;
    for (i = 0; i < 7; i++)
    {
        gHostCpuCtl[i] = 0;
    }

    /* 对外全局量 */
    sciFlag.all = (Uint16)in->sciflag;

    {
        unsigned short *q = (unsigned short *)&commRcvData;
        for (i = 0; i < 14; i++)
        {
            q[i] = (Uint16)(0x3000u + i);
        }
        q[13] = (Uint16)in->saveEeprom;
    }

    for (i = 0; i < RTU_READ_DATA_NUM_MAX; i++)
    {
        commReadData[i] = (Uint16)in->rd[i];
    }

    g_commReadRet  = (Uint16)in->rdRet;
    g_commWriteRet = (Uint16)in->wrRet;

    fill_funccode(in->pat);
}

/* ------------------------------------------------------------------------- */
/* 一次调用                                                                   */
/* ------------------------------------------------------------------------- */

#define K_MAIN   0
#define K_RXISR  1
#define K_TXISR  2
#define K_CWRITE 3
#define K_CREAD  4
#define K_RXDIR  5

static const char *g_kindName[6] =
{
    "MAIN", "RXISR", "TXISR", "CWRITE", "CREAD", "RXDIR"
};

/* g_ret = 入口的返回值, g_cr = OscControlRead 的 out 参数。
 * 两个都要比: 只看返回值会漏掉 "*result 写错", 只看 out 参数会漏掉
 * "CommRead 的返回值传错了"。 */
static unsigned short g_ret[2];

static void call_side(int side, int k, const struct IN *in)
{
    Uint16 r = 0;

    g_ret[side] = 0;
    g_cr[side]  = 0;

    switch (k)
    {
    case K_MAIN:
        if (side == 0) { ref_OscSciFunction(); }
        else           { new_OscSciFunction(); }
        break;

    case K_RXISR:
        if (g_rxIsr[side] != 0)
        {
            g_rxIsr[side]();
        }
        break;

    case K_TXISR:
        if (g_txIsr[side] != 0)
        {
            g_txIsr[side]();
        }
        break;

    case K_CWRITE:
        g_ret[side] = (side == 0)
                    ? ref_OscControlWriter(in->cAddr, in->cData)
                    : new_OscControlWriter(in->cAddr, in->cData);
        break;

    case K_CREAD:
        if (side == 0)
        {
            g_ret[0] = ref_OscControlRead(in->cAddr, &r);
        }
        else
        {
            g_ret[1] = new_OscControlRead(in->cAddr, &r);
        }
        g_cr[side] = r;
        break;

    case K_RXDIR:
        g_ret[side] = (side == 0)
                    ? ref_call_scidatarx(in->rxLenIn, in->rxTo)
                    : new_call_scidatarx(in->rxLenIn, in->rxTo);
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------------------- */
/* step                                                                       */
/* ------------------------------------------------------------------------- */

static int g_curKind;
static int g_curPat;

/* 测试台自检。覆盖计数器只记"我打算走哪一支", 不记"真的走了" —— 模块 45 的
 * 教训是: 计数器全绿而实际一次都没进分支, 看起来像"被测代码没问题"。
 * 所以每条关键场景后面都要**从快照里读出预期变化**, 对不上就报 BENCH-BUG。
 * BENCH-BUG 不是被测代码的错, 是测试台自己没测到东西。 */
static long g_benchBad;

static unsigned short g_prevTxBusy;
static unsigned short g_prevRwA, g_prevRwB;

static void bench_check(int cond, const char *msg)
{
    if (!cond)
    {
        g_benchBad++;
        if (g_benchBad <= 40)
        {
            printf("BENCH-BUG %s\n", msg);
        }
    }
}

static void preset_frame(const unsigned short *f, int n, int rxDone)
{
    ref_set_frame(f, n);
    new_set_frame(f, n);
    if (rxDone)
    {
        ref_set_rx_done();
        new_set_rx_done();
    }
}

/* RX/TX 中断的前置条件 —— 为什么必须显式卡住:
 *
 * 生产代码里这两个 ISR 只会在硬件把 4 字节摆进 FIFO 时被调用, 那一刻
 * SciRxBufStr.buf / SciTxBufStr.buf 一定指向一个还有 >=4 个元素余量的缓冲。
 * 测试台是**直接调 ISR**, 没有这层硬件保证: 变异只要让某一侧在
 * busy/len 上说谎 (例如 "busy 初值 0 改成 1"、"len 初值 0xff 改成 0"、
 * "交帧长度 8 改成 9"), ISR 里的 4 条 *buf++ 就会写到 OscConFrameBuf[10]
 * 之外, 覆盖紧随其后的 SciRxBufStr.buf 指针 —— 下一次解引用就是野指针,
 * 进程以 0xC0000005 直接死掉。
 *
 * 这比"抓到"更糟: 进程崩在半路, 它后面的用例一条都没跑, 总账却照样写
 * CAUGHT (见 MEMORY 里 MotorPmsmParEst 的教训)。实测第一版 201 条里有
 * 18 条 rc=3221225477, 全部是这个机制。
 *
 * 判据按**每一侧自己**的写行为算, 不按两侧是否一致:
 *   - RX ISR 只在 RXERROR==0 时写 4 字节, 所以只看 rxerr;
 *   - TX ISR 只在 len>3 时写 4 字节, len<=3 走的是"关 TXFFIENA"的收尾支,
 *     一次解引用都没有 —— 这一支必须照常驱动, 否则针对它的变异会假 MISSED。
 * 基准上这个门一次都不该关, 所以它不削弱覆盖; 关掉的次数记在 ISR_GUARD 里,
 * 一眼能看出是不是把待测代码短路了。 */
static int buf_write_ok(int kind, long long off, int n)
{
    long long cap;

    switch (kind)
    {
    case 1:  cap = OSC_BUF_N;   break;      /* OscDataBufA */
    case 2:  cap = OSC_BUF_N;   break;      /* OscDataBufB */
    case 3:  cap = OSC_FRAME_N; break;      /* OscConFrameBuf */
    default: return 0;                      /* NULL 或已经被写坏 */
    }
    return (off >= 0) && ((off + n) <= cap);
}

static int isr_safe(int k, const struct IN *in)
{
    int s;

    for (s = 0; s < 2; s++)
    {
        if (k == K_RXISR)
        {
            if (in->rxerr != 0)
            {
                continue;               /* 错误支: 一个字节都不写 */
            }
            if (!buf_write_ok(g_st[s].rxKind, g_st[s].rxOff, 4))
            {
                return 0;
            }
        }
        else
        {
            if (g_st[s].txLen < 4)
            {
                continue;               /* 收尾支: 不写 */
            }
            if (!buf_write_ok(g_st[s].txKind, g_st[s].txOff, 4))
            {
                return 0;
            }
        }
    }
    return 1;
}

static void step_p(int k, const struct IN *in, const char *what)
{
    if (((k == K_RXISR) || (k == K_TXISR)) && (!isr_safe(k, in)))
    {
        COV(COV_ISR_GUARD);
        return;
    }

    g_cases++;

    /* 覆盖按**步**计, 不按调用计 —— 每步两侧各调一次, 按调用计会一律翻倍,
     * 看着像覆盖变多了, 其实什么也没多。 */
    switch (k)
    {
    case K_MAIN:   COV(COV_MAIN);      break;
    case K_RXISR:  COV(COV_RXISR);     break;
    case K_TXISR:  COV(COV_TXISR);     break;
    case K_CWRITE: COV(COV_CTRLWRITE); break;
    case K_CREAD:  COV(COV_CTRLREAD);  break;
    case K_RXDIR:  break;
    default: break;
    }

    apply_common(in);
    hw_save(&g_pre, 0);

    call_side(0, k, in);
    hw_save(&g_hw[0], 0);
    ref_state_get(&g_st[0]);

    hw_load(&g_pre);

    call_side(1, k, in);
    hw_save(&g_hw[1], 1);
    new_state_get(&g_st[1]);

    cmp_hw(&g_hw[0], &g_hw[1], what);
    cmp_state(&g_st[0], &g_st[1], what);

    if (g_ret[0] != g_ret[1])
    {
        char d[192];
        sprintf(d, "ref=%u new=%u", g_ret[0], g_ret[1]);
        report("ret", d);
        note_where(what);
    }
    if (g_cr[0] != g_cr[1])
    {
        char d[192];
        sprintf(d, "ref=%u new=%u", g_cr[0], g_cr[1]);
        report("creadOut", d);
        note_where(what);
    }

    /* 从快照里取派生覆盖, 比"我以为走了哪一支"可靠。
     *
     * 不用 "full 位为 1" 判: OscSciFunction 里 OscDataQcq() 之后紧跟
     * OscDataTxDeal(), 填满的当拍就被送走, 步边界上几乎看不到 full=1。
     * 改用 rwPI 回绕 —— 它只会在 `rwPI > OSC_BUF_DATA_LEN - chSum*2` 那一支
     * 被清 0, 而那正是"这一块填满了"的唯一标志。 */
    if (g_st[0].bufA[68] < g_prevRwA) { COV(COV_QCGQ_FULL_A); }
    if (g_st[0].bufB[68] < g_prevRwB) { COV(COV_QCGQ_FULL_B); }
    g_prevRwA = g_st[0].bufA[68];
    g_prevRwB = g_st[0].bufB[68];
    if (g_st[0].con[0] == 1)   { COV(COV_STATUS_ON); }
    else                       { COV(COV_STATUS_OFF); }

    /* SciDataTX 起一次新发送: busy 由 0 变 1, 且游标落在 A / B 缓冲上。
     * (busy 为 1 时 SciDataTX 直接返回 FALSE, 连游标都不改, 所以这个迁移
     * 只可能是真的发起了一次新发送。) */
    if ((g_prevTxBusy == 0) && (g_st[0].txBusy != 0))
    {
        if (g_st[0].txKind == 1)      { COV(COV_TX_SENT_A); }
        else if (g_st[0].txKind == 2) { COV(COV_TX_SENT_B); }
    }
    g_prevTxBusy = g_st[0].txBusy;

    g_curKind = k;
    g_curPat  = in->pat;
}

static void step(int k, const struct IN *in)
{
    char what[96];

    sprintf(what, "%s pat=%d", g_kindName[k], (int)in->pat);
    step_p(k, in, what);
}

/* ------------------------------------------------------------------------- */
/* 控制帧                                                                     */
/* ------------------------------------------------------------------------- */

static void make_frame(unsigned short *f, unsigned short head, unsigned short fc,
                       unsigned short p1, unsigned short p2,
                       unsigned short p3, unsigned short p4)
{
    Uint16 crc;

    f[0] = head;
    f[1] = fc;
    f[2] = p1;
    f[3] = p2;
    f[4] = p3;
    f[5] = p4;

    crc = CrcValueByteCalc((const Uint16 *)f, FC_FRAME_LEN - 2);
    f[6] = (unsigned short)(crc & 0x00FFu);
    f[7] = (unsigned short)((crc >> 8) & 0x00FFu);
}

/* 跑一个完整帧: 预置"帧已收完", 然后一直跑到 SciDataRx 真的把帧交出去。
 *
 * 为什么不是固定 5 拍: SciDataRx 每 5 次 OscSciFunction 才试一次, 而 BaudCfg
 * 置起来时前三拍走早退 (BaudCfg 路径), 一次都不推进 countRx。固定 5 拍会在
 * "上一帧刚改过波特率"的场合整个错过帧, 于是帧分发一条都没跑到, 而计数器照样
 * 显示"我打算跑 4 次 PARA_CFG"。这正是模块 45 那个教训的翻版。
 *
 * 交接完成的标志: SciDataRx 在返回 TRUE 之前把 len 设成 0xff。
 * 上限 16 拍是保险, 到了就说明测试台有问题, 由 bench_check 报出来。 */
static void run_frame(const unsigned short *f, const struct IN *base,
                      const char *what)
{
    struct IN in = *base;
    int i;

    preset_frame(f, 8, 1);
    for (i = 0; i < 12; i++)
    {
        in.pat = (unsigned short)(base->pat + i);
        step_p(K_MAIN, &in, what);
    }
}

/* 和 run_frame 一样, 但 status 一变成 want 就立刻停 —— 观察的是 SciDataRxDeal
 * 返回那一刻的状态, 而不是被后面几拍里的 OscDataQcq 冲掉之后的状态。
 *
 * 为什么必须这么看: FC_STOP_OSC 那一支把 A/B 的 rwPI、full 清 0, 而紧接着的
 * 下一次 OscSciFunction 调用里, OscDataQcq 的 status!=1 分支会把同样四个字段
 * 再清一遍。于是"删掉 STOP 里那四行"在 12 拍之后看是**真的等价**, 只有在
 * STOP 当拍才看得出差别。这不是钻牛角尖: 那四行的作用正是让 STOP 立即生效,
 * 晚一拍就是另一个行为。 */
static void run_frame_until_status(const unsigned short *f, const struct IN *base,
                                   const char *what, unsigned short want)
{
    struct IN in = *base;
    int i;

    preset_frame(f, 8, 1);
    for (i = 0; i < 12; i++)
    {
        in.pat = (unsigned short)(base->pat + i);
        step_p(K_MAIN, &in, what);
        if (g_st[0].con[0] == want)
        {
            break;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* 相位                                                                       */
/* ------------------------------------------------------------------------- */

static struct IN g_in;

/* 上电: OscCfg==0 -> InitOsc -> InitSciOSC -> OscSciIoInit。
 * 之后 5 次 MAIN 让 countRx 走到 5, SciDataRx 被调用并进入接收态。 */
static void phase_boot(void)
{
    int i;

    in_reset(&g_in, 0x11);
    step(K_MAIN, &g_in);                 /* 初始化分支 */

    if ((g_rxIsr[0] == 0) || (g_txIsr[0] == 0))
    {
        printf("  [warn] no ISR captured from vector table (rx=%p tx=%p)\n",
               (void *)g_rxIsr[0], (void *)g_txIsr[0]);
    }
    if ((g_rxIsr[0] != 0) && (g_rxIsr[0] == g_rxIsr[1]))
    {
        printf("  [warn] both sides share one RX ISR address, renaming failed\n");
    }

    for (i = 0; i < 5; i++)
    {
        g_in.pat = (unsigned short)(0x11 + i + 1);
        step(K_MAIN, &g_in);
    }
}

/* RX 中断: RXERROR=1 -> BaudCfg=1; =0 -> 写 4 字节, len -= 4, 归零时关 RXENA。
 * 注意本测试台的 SCIRXBUF 是普通内存, 所以一次 ISR 里的 4 次读拿到同一个值 ——
 * 这是建模限制, 记在 README 的"测不了什么"里。 */
static void phase_isr_rx(void)
{
    struct IN in;
    int i;

    /* 先把接收态打起来: 再跑 5 次 MAIN 会重新 arm (len 从 0xff 变成 8)。 */
    for (i = 0; i < 5; i++)
    {
        in_reset(&in, (unsigned short)(0x20 + i));
        step(K_MAIN, &in);
    }

    /* 正常路径: 两次 ISR 收满 8 字节 */
    for (i = 0; i < 2; i++)
    {
        in_reset(&in, (unsigned short)(0x30 + i));
        in.rxdt = (unsigned short)(0x40 + i * 0x11);
        step(K_RXISR, &in);
        COV(COV_RXISR_OK);
    }
    if (g_st[0].rxLen == 0)
    {
        COV(COV_RXISR_LAST);
    }

    /* 错误路径: RXERROR=1 -> BaudCfg=1, 一个字节都不写 */
    for (i = 0; i < 3; i++)
    {
        in_reset(&in, (unsigned short)(0x38 + i));
        in.rxerr = 1;
        in.rxdt  = 0x5A;
        step(K_RXISR, &in);
        COV(COV_RXISR_ERR);
    }

    /* BaudCfg=1 的三拍早退, 第 3 拍重配 SCI */
    for (i = 0; i < 3; i++)
    {
        in_reset(&in, (unsigned short)(0x3C + i));
        step(K_MAIN, &in);
    }
    COV(COV_BAUDCFG_REINIT);
}

/* TX 中断: SCI_TX_INT_SPEEDUP==1 的活分支 —— len>3 时一次推 4 字节,
 * 否则 busy=0 且关 TXFFIENA。`#else` 那半边是死代码 (宏固定为 1)。 */
static void phase_isr_tx(void)
{
    struct IN in;
    unsigned short f[8];
    int i;

    /* 先造一次发送: 收一个 FC_COMM_TEST 帧, SciDataRxDeal 里会 SciDataTX */
    make_frame(f, OSC_CON_FRAME_HEAD, FC_COMM_TEST, 0, 0, 0, 0);
    in_reset(&in, 0x50);
    run_frame(f, &in, "tx-arm");

    /* 反复打 TX ISR, 每次换 TXFFST (speedup 分支不读它, 但别让它成为
     * "因为没驱动所以没差别"的假覆盖) */
    for (i = 0; i < 12; i++)
    {
        in_reset(&in, (unsigned short)(0x51 + i));
        in.txst = (unsigned short)(i & 0x1F);
        step(K_TXISR, &in);
    }

    /* 再 arm 一次并连续打, 走到 len<=3 的收尾支 */
    make_frame(f, OSC_CON_FRAME_HEAD, FC_COMM_TEST, 0, 0, 0, 0);
    in_reset(&in, 0x60);
    run_frame(f, &in, "tx-arm2");
    for (i = 0; i < 4; i++)
    {
        in_reset(&in, (unsigned short)(0x61 + i));
        step(K_TXISR, &in);
    }
}

/* 命令分发。OscCongData.status 是门: 一旦 START_OSC 过, 除 STOP 外全部
 * 走 err=0x80 分支。所以"能进 switch"的那些必须先跑。 */
static void con_copy(unsigned short *dst)
{
    int i;
    for (i = 0; i < OSC_CON_N; i++)
    {
        dst[i] = g_st[0].con[i];
    }
}

static void phase_frames(void)
{
    struct IN in;
    unsigned short f[8];
    unsigned short before[OSC_CON_N];

    /* --- 能进 switch 的: status == 0 --- */

    /* FC_COMM_TEST: 什么都不做, 但一定要走到 SciDataTX —— 否则后面所有
     * "帧被处理了"的结论都无从谈起。txBusy 0->1 且 buf 指向 OscConFrameBuf
     * 的 +4 处, 是 SciDataTX 真的跑过的证据。 */
    make_frame(f, OSC_CON_FRAME_HEAD, FC_COMM_TEST, 0, 0, 0, 0);
    in_reset(&in, 0x70);
    run_frame(f, &in, "FC_COMM_TEST");
    COV(COV_FRM_COMM_TEST);
    bench_check(g_st[0].txBusy != 0, "COMM_TEST did not reach SciDataTX");
    bench_check(g_st[0].txKind == 3, "SciDataTX cursor not inside frame buffer");
    bench_check(g_st[0].txOff == 4, "SciDataTX cursor did not advance by 4");
    bench_check(g_st[0].txLen == 4, "SciDataTX remaining len is not 8-4");

    /* FC_CHANNL_SEL: con[4..7] = P1..P4, con[8] = 非零通道数 */
    {
        static const unsigned short sel[6][4] =
        {
            { 1, 0, 0, 0 },
            { 1, 2, 0, 0 },
            { 1, 2, 3, 0 },
            { 1, 2, 3, 4 },
            { 0, 0, 0, 0 },
            { 7, 9, 11, 120 }
        };
        int i;
        int k;

        for (i = 0; i < 6; i++)
        {
            unsigned short want = 0;
            for (k = 0; k < 4; k++)
            {
                if (sel[i][k] != 0) { want++; }
            }

            make_frame(f, OSC_CON_FRAME_HEAD, FC_CHANNL_SEL,
                       sel[i][0], sel[i][1], sel[i][2], sel[i][3]);
            in_reset(&in, (unsigned short)(0x72 + i));
            run_frame(f, &in, "FC_CHANNL_SEL");
            COV(COV_FRM_CHANNL);

            bench_check(g_st[0].con[4] == sel[i][0], "CHANNL_SEL: ch1Addr not stored");
            bench_check(g_st[0].con[5] == sel[i][1], "CHANNL_SEL: ch2Addr not stored");
            bench_check(g_st[0].con[6] == sel[i][2], "CHANNL_SEL: ch3Addr not stored");
            bench_check(g_st[0].con[7] == sel[i][3], "CHANNL_SEL: ch4Addr not stored");
            bench_check(g_st[0].con[8] == want,        "CHANNL_SEL: chSum wrong");
        }
    }

    /* FC_PARA_CFG: P1 在 1..3 才改波特率, P2 在 1..100 才改间隔 */
    {
        static const unsigned short par[4][4] =
        {
            { 1, 0, 0, 0 },
            { 3, 100, 0, 0 },
            { 4, 101, 0, 0 },
            { 2, 50, 0, 0 }
        };
        int i;

        for (i = 0; i < 4; i++)
        {
            unsigned short wantBaud = g_st[0].con[1];
            unsigned short wantInt  = g_st[0].con[2];

            if ((par[i][0] > 0) && (par[i][0] < 4))   { wantBaud = par[i][0]; }
            if ((par[i][1] > 0) && (par[i][1] < 101)) { wantInt  = par[i][1]; }

            make_frame(f, OSC_CON_FRAME_HEAD, FC_PARA_CFG,
                       par[i][0], par[i][1], par[i][2], par[i][3]);
            in_reset(&in, (unsigned short)(0x80 + i));
            run_frame(f, &in, "FC_PARA_CFG");
            COV(COV_FRM_PARA);

            bench_check(g_st[0].con[1] == wantBaud, "PARA_CFG: baudRate gate wrong");
            bench_check(g_st[0].con[2] == wantInt,  "PARA_CFG: interval gate wrong");
        }
    }

    /* BaudCfg 置起来之后走三拍早退 + 重配 */
    {
        int i;
        for (i = 0; i < 3; i++)
        {
            in_reset(&in, (unsigned short)(0x85 + i));
            step(K_MAIN, &in);
        }
        COV(COV_BAUDCFG_REINIT);
    }

    /* FC_READ_FC_DATA: P3/P4 = commReadData[0] 的低/高字节。
     *
     * OscControlRead 的返回值是 CommRead(addr,1) = g_commReadRet ^ (addr*7)。
     * 要让 err == 0 走到"把读回值填进 P3/P4"那一支, 必须让这个异或恰好为 0,
     * 也就是 g_commReadRet == addr*7 —— 否则 err > 0, P3 会被覆盖成 err、
     * P4 被清 0, 那是另一条分支。第一版就是在这里把期望值算错的。 */
    {
        int i;
        for (i = 0; i < 2; i++)
        {
            unsigned short p1 = (unsigned short)(0x11 + i);
            unsigned short p2 = 0x02;
            unsigned short addr = (unsigned short)(((p2 << 8) & 0xFF00) | (p1 & 0xFF));
            unsigned short rdRet = (unsigned short)((addr * 7u) & 0xFFFFu);
            unsigned short dat;

            make_frame(f, OSC_CON_FRAME_HEAD, FC_READ_FC_DATA, p1, p2, 0, 0);
            in_reset(&in, (unsigned short)(0x88 + i));
            in.rdRet = rdRet;
            run_frame(f, &in, "FC_READ_FC_DATA");
            COV(COV_FRM_READ);

            /* mock 的 CommRead 把 data 也折进去了: OscControlRead 传的是 1 */
            dat = (unsigned short)((rdRet + addr + 13u) & 0xFFFFu);
            bench_check(g_st[0].frame[4] == (dat & 0x00FF),
                        "READ: P3 is not the low byte read back");
            bench_check(g_st[0].frame[5] == ((dat >> 8) & 0x00FF),
                        "READ: P4 is not the high byte read back");
            bench_check(g_st[0].frame[1] == FC_READ_FC_DATA,
                        "READ: FC got an error bit although err was 0");
            bench_check(g_hw[0].sciFlagAll == 0,
                        "READ: OscControlRead did not clear sciFlag");
        }

        /* err != 0: FC |= 0x80, P1=1, P2=0x80, P3=err, P4=0 */
        {
            unsigned short p1 = 0x33;
            unsigned short p2 = 0x04;
            unsigned short addr = (unsigned short)(((p2 << 8) & 0xFF00) | (p1 & 0xFF));
            unsigned short rdRet = 0x1234;
            unsigned short err = (unsigned short)((rdRet ^ (unsigned short)(addr * 7u)) & 0xFFFFu);

            make_frame(f, OSC_CON_FRAME_HEAD, FC_READ_FC_DATA, p1, p2, 0, 0);
            in_reset(&in, 0x8B);
            in.rdRet = rdRet;
            run_frame(f, &in, "FC_READ_FC_DATA/err");
            COV(COV_FRM_READ);

            bench_check(err != 0, "READ/err: err was zero, wrong case");
            bench_check(g_st[0].frame[1] == ((FC_READ_FC_DATA | 0x80) & 0xFF),
                        "READ/err: FC error bit not set");
            bench_check(g_st[0].frame[2] == 1, "READ/err: P1 is not 1");
            bench_check(g_st[0].frame[3] == 0x80, "READ/err: P2 is not 0x80");
            bench_check(g_st[0].frame[5] == 0, "READ/err: P4 is not 0");
            /* 注意: 错误分支写的是 `CONTROL_FRAME_P3 = err;` —— **不掩码**。
             * uint8 在 C28x 上是 16 位, 所以整个 err 都留在 P3 里。
             * 写成 `err & 0xFF` 会漏掉高字节, 是一条假的"已覆盖"。 */
            bench_check(g_st[0].frame[4] == err,
                        "READ/err: P3 is not the full 16-bit err");
        }
    }

    /* FC_WRITE_FC_DATA: 调 OscControlWriter, 返回后 commCmdSaveEeprom 复位 */
    {
        int i;
        for (i = 0; i < 3; i++)
        {
            make_frame(f, OSC_CON_FRAME_HEAD, FC_WRITE_FC_DATA,
                       (unsigned short)(0x21 + i), 0x03, 0x5A, 0xA5);
            in_reset(&in, (unsigned short)(0x8C + i));
            run_frame(f, &in, "FC_WRITE_FC_DATA");
            COV(COV_FRM_WRITE);

            bench_check(g_hw[0].sciFlagAll == 0, "WRITE: OscControlWriter did not clear sciFlag");
            bench_check(g_hw[0].commRcv[13] == SCI_WRITE_NO_EEPROM,
                        "WRITE: commCmdSaveEeprom not restored");
        }
    }

    /* 未知命令 -> default: err = 0x80 -> FC |= 0x80, P2=0x80, P1=1, P4=0, P3=err */
    {
        static const unsigned short bad[3] = { 0x00, 0x7F, 0xFF };
        int i;
        for (i = 0; i < 3; i++)
        {
            make_frame(f, OSC_CON_FRAME_HEAD, bad[i], 1, 2, 3, 4);
            in_reset(&in, (unsigned short)(0x90 + i));
            run_frame(f, &in, "FC_DEFAULT");
            COV(COV_FRM_DEFAULT);

            bench_check(g_st[0].frame[1] == ((bad[i] | 0x80) & 0xFF),
                        "DEFAULT: FC error bit not set");
            bench_check(g_st[0].frame[2] == 1,    "DEFAULT: P1 is not 1");
            bench_check(g_st[0].frame[3] == 0x80, "DEFAULT: P2 is not 0x80");
            bench_check(g_st[0].frame[5] == 0,    "DEFAULT: P4 is not 0");
            bench_check(g_st[0].frame[4] == 0x80, "DEFAULT: P3 is not err");
        }
    }

    /* pwdPass 分支: err==0 且 sciFlag.bit.pwdPass 置位 -> P3=P4=0x88 */
    {
        make_frame(f, OSC_CON_FRAME_HEAD, FC_COMM_TEST, 0, 0, 0, 0);
        in_reset(&in, 0x94);
        in.sciflag = 0x0800;                 /* pwdPass 在 bit 11 */
        run_frame(f, &in, "pwdPass");
        COV(COV_FRM_PWDPASS);

        bench_check(g_st[0].frame[4] == 0x88, "pwdPass: P3 is not 0x88");
        bench_check(g_st[0].frame[5] == 0x88, "pwdPass: P4 is not 0x88");
    }

    /* --- START_OSC 之后: status==1 的 err=0x80 分支 --- */
    make_frame(f, OSC_CON_FRAME_HEAD, FC_START_OSC, 0, 0, 0, 0);
    in_reset(&in, 0x96);
    run_frame(f, &in, "FC_START_OSC");
    COV(COV_FRM_START);
    bench_check(g_st[0].con[0] == 1, "START_OSC: status not set");

    con_copy(before);
    make_frame(f, OSC_CON_FRAME_HEAD, FC_COMM_TEST, 0, 0, 0, 0);
    in_reset(&in, 0x98);
    run_frame(f, &in, "status-busy");
    COV(COV_FRM_STATUS_BUSY);
    /* status != 0 这一支**什么也不写**: 它只给局部 err 赋值 0x80, 而
     * `if (err > 0) { ... }` 和 `SciDataTX(...)` 都在 else 块**里面**,
     * 也就是说这条路径既不回错误帧也不发送。原工程就是这样, 不是改写引入的。
     * 所以这里能验的只有"什么都没变" —— 反过来, 改写若把它挪进 if (err>0)
     * 里, 帧就会被改写, 这里立刻报出来。 */
    bench_check(g_st[0].con[0] == 1, "status-busy: status was modified");
    bench_check(g_st[0].frame[1] == FC_COMM_TEST,
                "status-busy: frame was modified, but this branch must write nothing");
    bench_check(g_st[0].con[4] == before[4], "status-busy: channel config was modified");

    /* --- STOP --- */
    make_frame(f, OSC_CON_FRAME_HEAD, FC_STOP_OSC, 0, 0, 0, 0);
    in_reset(&in, 0x9A);
    run_frame(f, &in, "FC_STOP_OSC");
    COV(COV_FRM_STOP);
    bench_check(g_st[0].con[0] == 0, "STOP_OSC: status not cleared");

    /* --- 帧头不对: 什么都不该发生 --- */
    {
        int i;
        for (i = 0; i < 3; i++)
        {
            con_copy(before);
            make_frame(f, (unsigned short)(0xA0 + i), FC_CHANNL_SEL, 5, 6, 7, 8);
            in_reset(&in, (unsigned short)(0x9C + i));
            run_frame(f, &in, "head-bad");
            COV(COV_FRM_HEAD_BAD);

            bench_check(g_st[0].con[4] == before[4], "head-bad: channel changed despite bad head");
            bench_check(g_st[0].con[8] == before[8], "head-bad: chSum changed despite bad head");
        }
    }

    /* --- CRC 不对: 同样什么都不该发生 --- */
    {
        int i;
        for (i = 0; i < 3; i++)
        {
            con_copy(before);
            make_frame(f, OSC_CON_FRAME_HEAD, FC_CHANNL_SEL, 9, 10, 11, 12);
            f[6] = (unsigned short)(f[6] ^ 0x01);
            in_reset(&in, (unsigned short)(0xA0 + i));
            run_frame(f, &in, "crc-bad");
            COV(COV_FRM_CRC_BAD);

            bench_check(g_st[0].con[4] == before[4], "crc-bad: channel changed despite bad CRC");
            bench_check(g_st[0].con[8] == before[8], "crc-bad: chSum changed despite bad CRC");
        }
    }
}

/* 排 TX FIFO: 每次 ISR 只推 4 个字, 68 字节的采集缓冲要 16 次才排空。
 * 只排一次的话 busy 会一直为 1, 后续 SciDataTX 全被挡掉, TxDeal 的 A/B 交替
 * 就永远只走到第一次 —— 计数器看着"有数", 其实一条分支都没重复过。 */
static void drain_tx(int maxCalls, unsigned short pat)
{
    struct IN in;
    int k;

    for (k = 0; k < maxCalls; k++)
    {
        if (g_st[0].txBusy == 0)
        {
            break;
        }
        in_reset(&in, (unsigned short)(pat + k));
        in.txst = (unsigned short)(k & 0x1F);
        step(K_TXISR, &in);
    }
}

/* 采集: START_OSC 之后连跑很多拍, 让 OscDataQcq 填满 A/B 两个 68 字缓冲,
 * OscDataTxDeal 再把它们送出去; 中间按节奏排 TX FIFO, 否则 busy 一直为 1,
 * 第二次以后的发送全被 SciDataTX 挡掉。 */
static void phase_data(void)
{
    struct IN in;
    unsigned short f[8];
    int i;

    make_frame(f, OSC_CON_FRAME_HEAD, FC_START_OSC, 0, 0, 0, 0);
    in_reset(&in, 0xB0);
    run_frame(f, &in, "start-for-data");

    /* chSum=1, interval=1: 每拍填 2 字节, 32 拍填满 64 字节。
     * 每 8 拍排一次 TX FIFO (只在 txBusy 置起来时打 ISR —— buf 还没指向任何
     * 缓冲时打进去会写坏模块自己的静态量, 那是测试台造出来的崩溃)。
     * 不排空的话 busy 永远是 1, 后续发送全被 SciDataTX 挡掉, 只能测到一次。 */
    for (i = 0; i < 200; i++)
    {
        in_reset(&in, (unsigned short)(0xB1 + i));
        step(K_MAIN, &in);
        if ((i % 8) == 7)
        {
            drain_tx(6, (unsigned short)(0xB1 + i));
        }
    }

    /* 换通道组合, 再跑一轮 */
    {
        static const unsigned short sel[4][4] =
        {
            { 1, 2, 0, 0 },
            { 1, 2, 3, 0 },
            { 1, 2, 3, 4 },
            { 5, 0, 0, 0 }
        };
        int s;

        for (s = 0; s < 4; s++)
        {
            make_frame(f, OSC_CON_FRAME_HEAD, FC_STOP_OSC, 0, 0, 0, 0);
            in_reset(&in, (unsigned short)(0xC0 + s));
            run_frame(f, &in, "stop-for-sel");

            make_frame(f, OSC_CON_FRAME_HEAD, FC_CHANNL_SEL,
                       sel[s][0], sel[s][1], sel[s][2], sel[s][3]);
            in_reset(&in, (unsigned short)(0xC4 + s));
            run_frame(f, &in, "sel-for-data");

            make_frame(f, OSC_CON_FRAME_HEAD, FC_START_OSC, 0, 0, 0, 0);
            in_reset(&in, (unsigned short)(0xC8 + s));
            run_frame(f, &in, "start-for-sel");

            for (i = 0; i < 120; i++)
            {
                in_reset(&in, (unsigned short)(0xD0 + i));
                step(K_MAIN, &in);
                if ((i % 6) == 5)
                {
                    drain_tx(6, (unsigned short)(0xD0 + i));
                }
            }
        }
    }

    /* interval 不是 1 的情形 */
    {
        static const unsigned short par[3] = { 2, 5, 100 };
        int s;

        for (s = 0; s < 3; s++)
        {
            make_frame(f, OSC_CON_FRAME_HEAD, FC_STOP_OSC, 0, 0, 0, 0);
            in_reset(&in, (unsigned short)(0xE0 + s));
            run_frame(f, &in, "stop-for-int");

            make_frame(f, OSC_CON_FRAME_HEAD, FC_PARA_CFG, 3, par[s], 0, 0);
            in_reset(&in, (unsigned short)(0xE4 + s));
            run_frame(f, &in, "para-for-int");
            for (i = 0; i < 3; i++)
            {
                in_reset(&in, (unsigned short)(0xE8 + i));
                step(K_MAIN, &in);
            }

            make_frame(f, OSC_CON_FRAME_HEAD, FC_START_OSC, 0, 0, 0, 0);
            in_reset(&in, (unsigned short)(0xEC + s));
            run_frame(f, &in, "start-for-int");

            for (i = 0; i < 150; i++)
            {
                in_reset(&in, (unsigned short)(0xF0 + i));
                step(K_MAIN, &in);
                if ((i % 6) == 5)
                {
                    drain_tx(6, (unsigned short)(0xF0 + i));
                }
            }
        }
    }

    make_frame(f, OSC_CON_FRAME_HEAD, FC_STOP_OSC, 0, 0, 0, 0);
    in_reset(&in, 0xFE);
    run_frame(f, &in, "final-stop");
}

/* OscControlRead / OscControlWriter 直接驱动 (它们在 FC_CODE_CONTROL_EN==1 下
 * 是外部链接的), 地址/数据/返回值全部扫一遍。 */
static void phase_ctrl(void)
{
    struct IN in;
    int i;

    for (i = 0; i < 24; i++)
    {
        in_reset(&in, (unsigned short)(0x100 + i));
        in.cAddr   = (unsigned short)(i * 0x1111);
        in.cData   = (unsigned short)(i * 0x0123 + 7);
        in.wrRet   = (unsigned short)(i * 0x0F0F);
        in.rdRet   = (unsigned short)(i * 0x0333);
        in.saveEeprom = SCI_WRITE_NO_EEPROM;
        step(K_CWRITE, &in);

        in_reset(&in, (unsigned short)(0x120 + i));
        in.cAddr = (unsigned short)(0xFFFF - i * 0x0101);
        in.rdRet = (unsigned short)(i * 0x2711);
        in.rd[0] = (unsigned short)(0x7000 + i);
        step(K_CREAD, &in);
    }
}

/* 两条"在零态下验证等于没验证"的分支, 单独补一个相位。
 *
 * 1) ClearOscBufEnd 把 oscDataBuf[60..63] 清 0。phase_frames 里那条 CHANNL_SEL
 *    (chSum==3) 确实调到了它, 但那时缓冲还全是 0 —— "清成 0"和"什么都没做"
 *    在快照上完全一样, 于是这个函数里的每一条变异都会 MISSED, 看起来像
 *    "不可观测", 其实是测试台没把被清的位置先弄脏。
 * 2) FC_STOP_OSC 那一支把 A/B 的 rwPI 和 full 清 0。phase_frames 里 status
 *    一直是 0, 而 OscSciFunction 每拍都走 OscDataQcq 的 status!=1 分支, 那两个
 *    字段早就是 0 了 —— 同样地, "清 0"不可观测。
 *
 * 这里先真的采一段数据 (rwPI 走完一圈、full 置起来、缓冲尾部写满), 再触发
 * 这两条分支, 并且**先断言脏态成立**再断言被清干净 —— 否则断言在空集合上
 * 恒真, 又是一条假的"已覆盖"。 */
static void phase_post(void)
{
    struct IN in;
    unsigned short f[8];
    int i;

    /* --- 1) 先把 chSum=1 的采集跑满一圈, 让 oscDataBuf[60..63] 有非零内容 --- */
    make_frame(f, OSC_CON_FRAME_HEAD, FC_STOP_OSC, 0, 0, 0, 0);
    in_reset(&in, 0x200);
    run_frame(f, &in, "post-stop");

    make_frame(f, OSC_CON_FRAME_HEAD, FC_CHANNL_SEL, 1, 0, 0, 0);
    in_reset(&in, 0x202);
    run_frame(f, &in, "post-sel1");

    make_frame(f, OSC_CON_FRAME_HEAD, FC_PARA_CFG, 3, 1, 0, 0);
    in_reset(&in, 0x204);
    run_frame(f, &in, "post-para");
    for (i = 0; i < 3; i++)
    {
        in_reset(&in, (unsigned short)(0x206 + i));
        step(K_MAIN, &in);
    }

    make_frame(f, OSC_CON_FRAME_HEAD, FC_START_OSC, 0, 0, 0, 0);
    in_reset(&in, 0x20A);
    run_frame(f, &in, "post-start");

    /* 采集只在 countRx > 4 那 1/5 的拍子里真正发生 (OscSciFunction 的节流),
     * 所以"32 个样本填满 64 字节"要 165 拍以上, 不是 32 拍。第一版按 40 拍算,
     * rwPI 只长到 16, 尾部一个字节都没写过 —— 于是 POST 的两条断言都在空集合
     * 上恒真。220 拍 = 44 个样本, 足够回绕一次并把尾部写满。 */
    for (i = 0; i < 220; i++)
    {
        in_reset(&in, (unsigned short)(0x210 + i));
        step(K_MAIN, &in);
        if ((i % 6) == 5)
        {
            drain_tx(6, (unsigned short)(0x210 + i));
        }
    }

    /* 脏态成立: 尾部四个字节至少有一个非零, 否则下面的"清 0"没意义 */
    {
        int nz = 0;
        int k;
        for (k = 60; k <= 63; k++)
        {
            if (g_st[0].bufA[3 + k] != 0) { nz++; }
            if (g_st[0].bufB[3 + k] != 0) { nz++; }
        }
        bench_check(nz != 0, "POST: tail of oscDataBuf is already zero, ClearOscBufEnd is unobservable");
    }

    /* 停下来 (STATUS 置 0 才能进 switch), 缓冲内容保持脏 */
    make_frame(f, OSC_CON_FRAME_HEAD, FC_STOP_OSC, 0, 0, 0, 0);
    in_reset(&in, 0x240);
    run_frame(f, &in, "post-stop2");

    /* chSum==3 -> ClearOscBufEnd */
    make_frame(f, OSC_CON_FRAME_HEAD, FC_CHANNL_SEL, 1, 2, 3, 0);
    in_reset(&in, 0x242);
    run_frame(f, &in, "post-clear");
    bench_check(g_st[0].con[8] == 3, "POST: chSum is not 3, ClearOscBufEnd was not reached");
    bench_check(g_st[0].bufA[3 + 63] == 0, "POST: bufA tail[0] not cleared");
    bench_check(g_st[0].bufA[3 + 62] == 0, "POST: bufA tail[1] not cleared");
    bench_check(g_st[0].bufA[3 + 61] == 0, "POST: bufA tail[2] not cleared");
    bench_check(g_st[0].bufA[3 + 60] == 0, "POST: bufA tail[3] not cleared");
    bench_check(g_st[0].bufB[3 + 63] == 0, "POST: bufB tail[0] not cleared");
    bench_check(g_st[0].bufB[3 + 62] == 0, "POST: bufB tail[1] not cleared");
    bench_check(g_st[0].bufB[3 + 61] == 0, "POST: bufB tail[2] not cleared");
    bench_check(g_st[0].bufB[3 + 60] == 0, "POST: bufB tail[3] not cleared");

    /* --- 2) 脏态下的 STOP: rwPI / full 必须真的被清掉 ---
     *
     * 这里刻意**不排 TX FIFO**: txBusy 一旦为 1 就再也不会落回去, 于是 full
     * 置起来之后 OscDataTxDeal 的 SciDataTX 一律返回 FALSE, full 留在 1 上,
     * 不会被"发送成功"顺手清掉。这样 full 才是 STOP 清掉的, 而不是被发送清掉的。
     *
     * 一个结构性限制: 同一时刻只有**一个**缓冲在被填 (bufSel 决定), 所以
     * A.rwPI 与 B.rwPI 不可能同时非零, A.full 与 B.full 也不可能同时由本相位
     * 置起。于是分两轮, 每轮让不同的两个字段脏起来, 各自用当拍观察。 */
    {
        static const struct
        {
            int steps;                 /* 采集拍数 (1/5 的拍子真正采样) */
            int wantA_rw, wantA_full;  /* 期望脏起来的字段 */
            int wantB_rw, wantB_full;
        } rnd[2] =
        {
            { 250, 0, 1, 1, 0 },       /* ~50 样本: A 已回绕, B 正在填 */
            { 400, 1, 1, 0, 1 }        /* ~80 样本: 两个都回绕过, A 又在填 */
        };
        int r;
        int k;

        for (r = 0; r < 2; r++)
        {
            /* 让 TX 忙起来: 发一帧 COMM_TEST 会调 SciDataTX */
            make_frame(f, OSC_CON_FRAME_HEAD, FC_STOP_OSC, 0, 0, 0, 0);
            in_reset(&in, (unsigned short)(0x300 + r * 0x40));
            run_frame_until_status(f, &in, "stop-pre", 0);

            make_frame(f, OSC_CON_FRAME_HEAD, FC_CHANNL_SEL, 1, 0, 0, 0);
            in_reset(&in, (unsigned short)(0x302 + r * 0x40));
            run_frame(f, &in, "sel1-for-stop");

            make_frame(f, OSC_CON_FRAME_HEAD, FC_PARA_CFG, 3, 1, 0, 0);
            in_reset(&in, (unsigned short)(0x304 + r * 0x40));
            run_frame(f, &in, "para-for-stop");
            for (k = 0; k < 3; k++)
            {
                in_reset(&in, (unsigned short)(0x306 + k));
                step(K_MAIN, &in);
            }

            /* 一帧 COMM_TEST 把 txBusy 拉起来; 之后全程不排空 */
            make_frame(f, OSC_CON_FRAME_HEAD, FC_COMM_TEST, 0, 0, 0, 0);
            in_reset(&in, (unsigned short)(0x308 + r * 0x40));
            run_frame(f, &in, "busy-tx-for-stop");
            bench_check(g_st[0].txBusy != 0, "POST: TX is not busy, full will be cleared by TX");

            make_frame(f, OSC_CON_FRAME_HEAD, FC_START_OSC, 0, 0, 0, 0);
            in_reset(&in, (unsigned short)(0x30A + r * 0x40));
            run_frame_until_status(f, &in, "start-for-stop", 1);
            bench_check(g_st[0].con[0] == 1, "POST: START_OSC did not take effect");

            for (k = 0; k < rnd[r].steps; k++)
            {
                in_reset(&in, (unsigned short)(0x310 + k));
                step(K_MAIN, &in);
            }

            /* 先断言脏态成立 —— 否则下面"被清成 0"的断言在空集合上恒真 */
            if (rnd[r].wantA_rw)
            {
                bench_check(g_st[0].bufA[68] != 0, "POST: pre-state bufA rwPI is zero, STOP write unobservable");
            }
            if (rnd[r].wantA_full)
            {
                bench_check(g_st[0].bufA[69] != 0, "POST: pre-state bufA full is zero, STOP write unobservable");
            }
            if (rnd[r].wantB_rw)
            {
                bench_check(g_st[0].bufB[68] != 0, "POST: pre-state bufB rwPI is zero, STOP write unobservable");
            }
            if (rnd[r].wantB_full)
            {
                bench_check(g_st[0].bufB[69] != 0, "POST: pre-state bufB full is zero, STOP write unobservable");
            }

            make_frame(f, OSC_CON_FRAME_HEAD, FC_STOP_OSC, 0, 0, 0, 0);
            in_reset(&in, (unsigned short)(0x380 + r * 0x40));
            run_frame_until_status(f, &in, "stop-dirty", 0);

            bench_check(g_st[0].con[0] == 0,   "POST: STOP did not clear status");
            bench_check(g_st[0].bufA[68] == 0, "POST: STOP did not clear bufA rwPI");
            bench_check(g_st[0].bufA[69] == 0, "POST: STOP did not clear bufA full");
            bench_check(g_st[0].bufB[68] == 0, "POST: STOP did not clear bufB rwPI");
            bench_check(g_st[0].bufB[69] == 0, "POST: STOP did not clear bufB full");

            /* 收尾: 把 TX 排空, 免得下一轮起手的 COMM_TEST 被 busy 挡掉 */
            drain_tx(40, (unsigned short)(0x3C0 + r * 0x40));
        }
    }
}

/* 随机混合。只调"安全"的那几种: RX/TX ISR 只在对应 busy 置起来之后才调,
 * 否则 SciTxBufStr.buf / SciRxBufStr.buf 还是 NULL, 一进去就访问违例 ——
 * 那是测试台自己造出来的崩溃, 不是被测代码的问题。 */
static unsigned long g_rng = 0x12345678UL;

static unsigned long rnd(void)
{
    g_rng = g_rng * 1103515245UL + 12345UL;
    return (g_rng >> 8) & 0xFFFFFFUL;
}

/* SciDataRx 的超时分支: 生产代码只用 timeout=0, 那个分支在整机不可达。
 * 这里用非零 timeout 直接驱动, 把常量变成输入。
 *
 * 为什么不能只调两次就完事: `count` / `timeOut` 是函数内 static, 驱动改不了,
 * 而且 `len==0` 那条支路**不复位 count**。第一版就是想当然地"先 arm 再连调
 * t+1 次", 结果 count 的初值未知, 超时分支在第 2 次调用上就提前炸了, 四条
 * bench_check 全红。可靠的做法是先把状态推到一个**确定**的现场:
 *
 *   1) 用 timeout=0 反复调, 直到 busy=1 (timeOut 为 0 时超时分支不可能触发,
 *      所以这一步一定会停在"已 arm、timeOut=0"上)
 *   2) 打两次 RX ISR 把这一帧收完 -> busy=0, len=0
 *   3) 调一次 timeout=0 -> len==0 支路: len=0xff, 返回 TRUE, 此时
 *      busy=0 / len!=0 / timeOut=0 —— 这正是 arm 支路的入口条件
 *   4) 调一次 timeout=t -> 进 arm 时 timeOut 还是 0, 超时判据不成立, 于是
 *      真的 arm, 并把 timeOut 写成 t, count 归 0
 *   5) 再连调 t+1 次 -> count 涨到 t+1 > t -> 超时分支
 *
 * 这一相位必须放在最后: 它会把 count / timeOut 留在非零值上, 放在前面会污染
 * 那些"自然轨迹"用例。 */
static void phase_rx_timeout(void)
{
    struct IN in;
    int t;
    int k;

    for (t = 1; t <= 4; t++)
    {
        /* 1) 推到 "已 arm" */
        for (k = 0; k < 6; k++)
        {
            in_reset(&in, (unsigned short)(0x500 + t * 0x20 + k));
            in.rxLenIn = 8;
            in.rxTo    = 0;
            step(K_RXDIR, &in);
            if (g_st[0].rxBusy)
            {
                break;
            }
        }
        bench_check(g_st[0].rxBusy == 1, "RXDIR: could not reach the armed state");

        /* 2) 两次 RX ISR 把这一帧收完 (len 8 -> 4 -> 0) */
        in_reset(&in, (unsigned short)(0x540 + t));
        step(K_RXISR, &in);
        in_reset(&in, (unsigned short)(0x548 + t));
        step(K_RXISR, &in);
        bench_check(g_st[0].rxBusy == 0, "RXDIR: frame did not complete (busy still set)");
        bench_check(g_st[0].rxLen == 0,  "RXDIR: frame did not complete (len not 0)");

        /* 3) len==0 支路 */
        in_reset(&in, (unsigned short)(0x550 + t));
        in.rxLenIn = 8;
        in.rxTo    = 0;
        step(K_RXDIR, &in);
        COV(COV_RXDIR_LENDONE);
        bench_check(g_ret[0] == TRUE,      "RXDIR: len==0 did not report a frame");
        bench_check(g_st[0].rxLen == 0xff, "RXDIR: len==0 did not reload 0xff");

        /* 4) arm, 并把 timeOut 写成 t */
        in_reset(&in, (unsigned short)(0x558 + t));
        in.rxLenIn = 8;
        in.rxTo    = (unsigned short)t;
        step(K_RXDIR, &in);
        COV(COV_RXDIR_ARM);
        bench_check(g_ret[0] == SCI_RT_BUSY, "RXDIR: arm did not return SCI_RT_BUSY");
        bench_check(g_st[0].rxBusy == 1,     "RXDIR: arm did not set busy");
        bench_check(g_st[0].rxLen == 8,      "RXDIR: arm did not store len");
        bench_check(g_st[0].rxFlag == 0x10,  "RXDIR: arm did not set rxflag 0x10");

        /* 5) t+1 次之后 count > timeOut */
        for (k = 0; k <= t; k++)
        {
            in_reset(&in, (unsigned short)(0x560 + k));
            in.rxLenIn = 8;
            in.rxTo    = (unsigned short)t;
            step(K_RXDIR, &in);
        }
        COV(COV_RXDIR_TIMEOUT);
        bench_check(g_st[0].rxBusy == 0, "RXDIR: timeout branch did not clear busy");
        bench_check(g_ret[0] == FALSE,   "RXDIR: timeout branch did not return FALSE");
        bench_check((g_hw[0].scictl1 & 0x0001u) == 0,
                    "RXDIR: timeout did not clear RXENA");
        bench_check(g_hw[0].sciffrx == 4,
                    "RXDIR: timeout did not write SCIFFRX=4");
    }
}

static void phase_random(long n)
{
    struct IN in;
    long i;
    int k;
    unsigned short f[8];

    for (i = 0; i < n; i++)
    {
        unsigned long r = rnd();
        unsigned long q = rnd();

        in_reset(&in, (unsigned short)(r & 0x3FF));

        in.rxerr   = (unsigned short)((q >> 0) & 1);
        in.rxdt    = (unsigned short)((q >> 3) & 0xFF);
        in.txst    = (unsigned short)((q >> 11) & 0x1F);
        in.txempty = (unsigned short)((q >> 16) & 1);
        in.ier     = (unsigned short)(r & 0xFFFF);
        in.sciflag = (unsigned short)((q >> 4) & 0xFFFF);
        in.saveEeprom = (unsigned short)((q >> 9) & 1);
        in.rdRet   = (unsigned short)(r * 3);
        in.wrRet   = (unsigned short)(q * 5);
        in.cAddr   = (unsigned short)(r ^ 0xA5A5);
        in.cData   = (unsigned short)(q ^ 0x5A5A);

        k = (int)(r % 100);

        if (k < 55)
        {
            step(K_MAIN, &in);
        }
        else if (k < 70)
        {
            step(K_TXISR, &in);
        }
        else if (k < 85)
        {
            if (g_st[0].rxBusy)
            {
                step(K_RXISR, &in);
            }
            else
            {
                step(K_MAIN, &in);
            }
        }
        else if (k < 95)
        {
            step(K_CWRITE, &in);
        }
        else
        {
            step(K_CREAD, &in);
        }

        /* 每 40 拍插一个完整帧, 免得随机相把状态推到一个再也收不到帧的地方 */
        if ((i % 40) == 39)
        {
            static const unsigned short fcs[8] =
            {
                FC_COMM_TEST, FC_CHANNL_SEL, FC_PARA_CFG, FC_START_OSC,
                FC_STOP_OSC, FC_READ_FC_DATA, FC_WRITE_FC_DATA, 0x77
            };
            unsigned short fc = fcs[(r >> 4) & 7];

            make_frame(f, OSC_CON_FRAME_HEAD, fc,
                       (unsigned short)((q >> 2) & 0x7F),
                       (unsigned short)((q >> 9) & 0x7F),
                       (unsigned short)((q >> 16) & 0xFF),
                       (unsigned short)((r >> 3) & 0xFF));
            run_frame(f, &in, "random-frame");
        }
    }
}

/* ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long nrand = 20000;

    if (argc > 1)
    {
        nrand = atol(argv[1]);
    }

    setvbuf(stdout, NULL, _IONBF, 0);

    printf("f_osc differential bench\n");

    phase_boot();

    printf("phase phase_isr_rx\n");
    phase_isr_rx();

    printf("phase phase_isr_tx\n");
    phase_isr_tx();

    printf("phase phase_frames\n");
    phase_frames();

    printf("phase phase_ctrl\n");
    phase_ctrl();

    printf("phase phase_data\n");
    phase_data();

    printf("phase phase_post\n");
    phase_post();

    printf("phase random %ld\n", nrand);
    phase_random(nrand);

    printf("phase phase_rx_timeout\n");
    phase_rx_timeout();

    printf("---- coverage ----\n");
    {
        int i;
        for (i = 0; i < COV_COUNT; i++)
        {
            printf("  %-18s %ld\n", g_covName[i], g_cov[i]);
        }
    }

    /* bench_bugs 必须为 0。它不是被测代码的问题, 是测试台没测到东西 ——
     * 一条 BENCH-BUG 就说明某个"我以为覆盖了"的场景其实一次都没进去, 那一段
     * 的"0 不一致"不算证据。
     *
     * 退出码分开: 0 干净 / 1 有失配 / 2 测试台自己坏了。混成一个的话,
     * 负向对照会把"测试台坏了"记成 CAUGHT。 */
    printf("bench_bugs=%ld\n", g_benchBad);
    printf("cases=%ld mismatches=%ld\n", g_cases, g_mism);
    if (g_mism != 0)
    {
        return 1;
    }
    return (g_benchBad == 0) ? 0 : 2;
}
