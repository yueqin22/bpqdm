/* Differential test driver for f_comm_DP.
 *
 * ---------------------------------------------------------------------------
 * What is compared
 *
 * The module is a UART protocol handler for a "DP" (profibus-DP-style) slave
 * link.  It has three kinds of observable state, and all three are diffed:
 *
 *   (1) its own struct  struct SCI_DATA_DP sciM380DpData  -- 40-byte TX and
 *       RX shift buffers, the frame state machine (frameFlagDp,
 *       rcvDataJuageFlag, rcvRigthFlag, commDpRcvNumber), the TX cursor
 *       (commDpSendNum / commDpSendNumMax), the CRC error counter, two frame
 *       header unions and four enums.  Compared field by field, and the two
 *       40-word arrays element by element so a wrong index is visible.
 *
 *   (2) its file-scope globals  rcvData_DP[] / sendData_DP[] / dataByteNum /
 *       dpParameter / dpSciCommRcvFlag / dpSciCommSendFlag.
 *
 *   (3) the peripherals it drives.  SciaRegs, PieCtrlRegs, GpioCtrlRegs and
 *       IER are memcmp'd whole, not summarised.  InitSetSciDp writes nine SCI
 *       registers and InitSciaGpioDp writes four GPIO fields plus two PIE
 *       enables; "wrote the right value to the wrong register" is exactly the
 *       mistake a summary hides.  The two PieVectTable slots are compared as
 *       normalised codes instead of raw pointers, because each side's ISR has
 *       a different host address by construction (see vect_code below).
 *
 * ---------------------------------------------------------------------------
 * How both sides see the same input
 *
 * The peripherals are shared; the module's own globals are not (they are
 * renamed per translation unit).  So each case does: load() -> run the ref
 * side -> capture() -> load() again -> run the new side -> capture() ->
 * compare.  load() writes the ref_* and new_* globals with the same values and
 * re-dirties the shared peripherals, so neither side can see the other's
 * leftovers.
 *
 * ---------------------------------------------------------------------------
 * Dirty prefills
 *
 * Every peripheral and every module field is filled with a position-dependent
 * byte ramp before the declared inputs are applied.  That matters twice: it
 * makes "the code failed to write a field" visible (the ramp value survives and
 * differs from what a write would produce), and it makes IER |= M_INT9 a real
 * test of OR-versus-assignment (the ramp leaves bits set that an assignment
 * would clear).
 *
 * ---------------------------------------------------------------------------
 * What is deliberately NOT driven
 *
 *   dpParameter.dpDataFormat outside 1..5.  GetDataByteNum indexes
 *   `dataByteNums[dpDataFormat - 1]` with no bounds check, so format 0 reads
 *   index -1 (65535 as a Uint16) and format 6 reads index 5 of a 5-element
 *   array.  Both are out-of-bounds reads on both sides -- undefined behaviour
 *   that would abort the run rather than reveal anything.  SciDpDeal hard-codes
 *   the format to 5 anyway, so the reachable range is 1..5.
 *
 *   NO_FIFO == 0 and BUG_SCI_BACK_DATA == 1.  Both are 0/1 in f_comm_DP.h, and
 *   that header is forwarded real (see host_patch.json), so the case tracks the
 *   firmware's configuration by construction.  The `#define FIFO_NUM 16;` with
 *   its trailing semicolon only compiles because NO_FIFO == 1 keeps it out of
 *   the live arm -- recorded as a latent trap, not fixed (fixing it would change
 *   what a NO_FIFO == 0 build does, and that build does not exist).
 */

#include <stdio.h>
#include <string.h>

#include "f_comm_DP.h"

/* The module's own symbols, renamed per side by ref_tu.c / new_tu.c. */
extern struct SCI_DATA_DP ref_sciM380DpData;
extern struct SCI_DATA_DP new_sciM380DpData;
extern Uint16 ref_rcvData_DP[RCV_DATA_NUMBER];
extern Uint16 new_rcvData_DP[RCV_DATA_NUMBER];
extern Uint16 ref_sendData_DP[SEND_DATA_NUMBER];
extern Uint16 new_sendData_DP[SEND_DATA_NUMBER];
extern Uint16 ref_dataByteNum;
extern Uint16 new_dataByteNum;
extern DP_PARAMETER ref_dpParameter;
extern DP_PARAMETER new_dpParameter;
extern enum DP_SCI_COMM_RCV_FLAG  ref_dpSciCommRcvFlag;
extern enum DP_SCI_COMM_RCV_FLAG  new_dpSciCommRcvFlag;
extern enum DP_SCI_COMM_SEND_FLAG ref_dpSciCommSendFlag;
extern enum DP_SCI_COMM_SEND_FLAG new_dpSciCommSendFlag;

extern void ref_SCI_DP_RXD_isr(void);
extern void ref_SCI_DP_TXD_isr(void);
extern void ref_CommDpStartSend(struct SCI_DATA_DP *p);
extern void ref_CommDpRcvDataDeal(struct SCI_DATA_DP *p);
extern void ref_CommDpSendDataDeal(struct SCI_DATA_DP *p);
extern void ref_SciDpDeal(struct SCI_DATA_DP *p);
extern void ref_ErrAgainSend(struct SCI_DATA_DP *p);
extern void ref_InitSetSciDp(struct SCI_DATA_DP *p);
extern void ref_UpdateSciDpFormat(struct SCI_DATA_DP *p);
extern void ref_InitSciaGpioDp(void);
extern void ref_InitScibGpioDp(void);
extern void ref_InitSciDpBaudRate(struct SCI_DATA_DP *p);
extern void ref_GetDataByteNum(void);
extern void ref_DpDataDeal(void);

extern void new_SCI_DP_RXD_isr(void);
extern void new_SCI_DP_TXD_isr(void);
extern void new_CommDpStartSend(struct SCI_DATA_DP *p);
extern void new_CommDpRcvDataDeal(struct SCI_DATA_DP *p);
extern void new_CommDpSendDataDeal(struct SCI_DATA_DP *p);
extern void new_SciDpDeal(struct SCI_DATA_DP *p);
extern void new_ErrAgainSend(struct SCI_DATA_DP *p);
extern void new_InitSetSciDp(struct SCI_DATA_DP *p);
extern void new_UpdateSciDpFormat(struct SCI_DATA_DP *p);
extern void new_InitSciaGpioDp(void);
extern void new_InitScibGpioDp(void);
extern void new_InitSciDpBaudRate(struct SCI_DATA_DP *p);
extern void new_GetDataByteNum(void);
extern void new_DpDataDeal(void);

/* Supplied by cases/f_comm_DP/mock_commdp.c -- one copy, shared by both sides. */
extern Uint16 CrcValueByteCalc(const Uint16 *data, Uint16 length);

/* Declared by hand rather than pulled in with <stdlib.h>: the case shim does
 * not fold `int` down to 16 bits here (this module has no bare `int`), but
 * stdlib.h's `int abs(int)` is a needless hazard to leave lying around and the
 * only thing wanted from that header is atol. */
extern long atol(const char *);

/* ------------------------------------------------------------------------- */
/* bookkeeping                                                               */
/* ------------------------------------------------------------------------- */

static long g_cases = 0;
static long g_fail  = 0;
static int  g_stop  = 0;
static int  g_quiet = 0;    /* set while the self-check plants a difference */

static unsigned long long g_rng = 0x243F6A8885A308D3ULL;

static Uint16 rnd16(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return (Uint16)(g_rng >> 17);
}

/* Branch-reach counters.  Every one of them is asserted non-zero at the end:
 * a differential test that never reaches a branch proves nothing about it, and
 * a silently-zero counter would look exactly like a clean run. */
static struct {
    long rxd_preamble;      /* tmp == 0xAA                                     */
    long rxd_second55;      /* rcvRigthFlag == 1 and tmp == 0x55                */
    long rxd_secondnot;     /* rcvRigthFlag == 1 and tmp != 0x55                */
    long rxd_reset;         /* the else arm -> UpdateSciDpFormat               */
    long rxd_guard;         /* rcvDataJuageFlag != 0, first block skipped      */
    long rxd_store;         /* frameFlagDp == 2, room left, stored             */
    long rxd_storefull;     /* frameFlagDp == 2, last byte, frame complete     */
    long rxd_storeskip;     /* frameFlagDp == 2, cursor already at the limit   */
    long rxd_flagother;     /* frameFlagDp not 2, second block skipped         */
    long txd_send;          /* commDpSendNum < commDpSendNumMax                */
    long txd_skip;          /* commDpSendNum >= commDpSendNumMax               */
    long rcv_nosignal;      /* sciRcvFlag != SCI_RCV_YES                       */
    long rcv_flagother;     /* frameFlagDp != 2                                */
    long rcv_crcok;         /* CRC accepted                                    */
    long rcv_crcbad;        /* CRC rejected                                    */
    long rcv_reset;         /* rcvCrcErrCounter > RCV_CRC_ERR_NUMBER           */
    long rcv_norereset;     /* ... and not                                  */
    long send_yes;          /* dpSciCommSendFlag == YES                        */
    long send_no;           /* dpSciCommSendFlag == NO                         */
    long dpdeal_early;      /* DpDataDeal early return                         */
    long dpdeal_pass;       /* DpDataDeal ran to the end                       */
    long upd_err;           /* UpdateSciDpFormat saw RXERROR                   */
    long upd_noerr;         /* ... and did not                              */
    long fmt[6];            /* GetDataByteNum per dpDataFormat 1..5            */
    long start;             /* CommDpStartSend                                 */
    long errsend;           /* ErrAgainSend                                    */
    long gpiodea;           /* InitSciaGpioDp                                  */
    long gpiodeb;           /* InitScibGpioDp                                  */
    long setdp;             /* InitSetSciDp                                    */
    long baud;              /* InitSciDpBaudRate                               */
} cov;

/* ------------------------------------------------------------------------- */
/* state                                                                     */
/* ------------------------------------------------------------------------- */

#define NSCI SEND_DATA_SCI_NUMBER   /* 40 */

typedef struct {
    /* --- inputs the test declares ------------------------------------- */
    Uint16 sendSci[NSCI];
    Uint16 rcvSci[NSCI];
    Uint16 commDpRcvNumber;
    Uint16 commDpSendNum;
    Uint16 commDpSendNumMax;
    Uint16 frameFlagDp;
    Uint16 rcvTickDp;
    Uint16 rcvDataJuageFlag;
    Uint16 rcvCrcErrCounter;
    Uint16 rcvRigthFlag;
    Uint16 frameStartAll;
    Uint16 frameSendStartAll;
    int    rcvTickFlag;
    int    dpReadFlag;
    int    sciRcvFlag;
    int    commDpStatus;

    Uint16 rcvDP[RCV_DATA_NUMBER];
    Uint16 sendDP[SEND_DATA_NUMBER];
    Uint16 dataByteNum;
    Uint16 dpAddress;
    Uint16 dpDataFormat;
    int    dpRcvFlag;
    int    dpSendFlag;

    Uint16 rxBuf;       /* SciaRegs.SCIRXBUF.all       */
    Uint16 rxError;     /* SciaRegs.SCIRXST.bit.RXERROR */
    Uint16 ier;
    Uint16 ctl[7];
} FIELDS;

typedef struct {
    FIELDS f;
    struct SCI_REGS       sci;
    struct PIE_CTRL_REGS  pie;
    struct GPIO_CTRL_REGS gpio;
    struct PIE_VECT_TABLE vect;
    int vx;     /* normalised SCIRXINTA */
    int vt;     /* normalised SCITXINTA */
    Uint16 txOut[NSCI];  /* bytes actually handed to SCITXBUF by SCI_DP_TXD_isr */
    Uint16 txLen;
} STATE;

/* A shared sentinel: both vector slots are pointed at it before every call, so
 * "untouched" is distinguishable from "set to some other address". */
static void sentinel(void)
{
}

static void dirty(void *p, size_t n)
{
    unsigned char *b = (unsigned char *)p;
    size_t i;

    for (i = 0; i < n; i++)
    {
        b[i] = (unsigned char)((i * 37u + 11u) & 0xFFu);
    }
}

static void setup_defaults(FIELDS *f)
{
    Uint16 i;

    memset(f, 0, sizeof *f);

    for (i = 0; i < NSCI; i++)
    {
        f->sendSci[i] = (Uint16)(0x1000u + i);
        f->rcvSci[i]  = (Uint16)(0x2000u + i);
    }
    for (i = 0; i < RCV_DATA_NUMBER; i++)
    {
        f->rcvDP[i] = (Uint16)(0x3000u + i);
    }
    for (i = 0; i < SEND_DATA_NUMBER; i++)
    {
        f->sendDP[i] = (Uint16)(0x4000u + i);
    }

    f->commDpSendNumMax = 8;
    f->dataByteNum      = 12;
    f->dpAddress        = 3;
    f->dpDataFormat     = 1;
    f->sciRcvFlag       = SCI_RCV_NO;
    f->commDpStatus     = SCI_CONNECT;
    f->rxError          = 0;
}

/* Write the case's inputs to BOTH sides and re-dirty the shared peripherals. */
static void load(const FIELDS *f)
{
    int i;

    dirty((void *)&SciaRegs, sizeof SciaRegs);
    SciaRegs.SCIRXBUF.all       = f->rxBuf;
    SciaRegs.SCIRXST.bit.RXERROR = f->rxError;

    dirty((void *)&PieCtrlRegs, sizeof PieCtrlRegs);
    dirty((void *)&GpioCtrlRegs, sizeof GpioCtrlRegs);

    PieVectTable.SCIRXINTA = sentinel;
    PieVectTable.SCITXINTA = sentinel;

    IER = f->ier;
    for (i = 0; i < 7; i++)
    {
        gHostCpuCtl[i] = f->ctl[i];
    }

    for (i = 0; i < 2; i++)
    {
        struct SCI_DATA_DP *p = (i == 0) ? &ref_sciM380DpData : &new_sciM380DpData;

        p->pSciRegs = (volatile struct SCI_REGS *)&SciaRegs;

        memcpy(p->sendData_SCI, f->sendSci, sizeof f->sendSci);
        memcpy(p->rcvData_SCI,  f->rcvSci,  sizeof f->rcvSci);
        p->commDpRcvNumber   = f->commDpRcvNumber;
        p->commDpSendNum     = f->commDpSendNum;
        p->commDpSendNumMax  = f->commDpSendNumMax;
        p->frameFlagDp       = f->frameFlagDp;
        p->rcvTickDp         = f->rcvTickDp;
        p->rcvDataJuageFlag  = f->rcvDataJuageFlag;
        p->rcvCrcErrCounter  = f->rcvCrcErrCounter;
        p->rcvRigthFlag      = f->rcvRigthFlag;
        p->frameStart.all      = f->frameStartAll;
        p->frameSendStart.all  = f->frameSendStartAll;
        p->rcvTickFlag       = (enum SCI_RCV_TICK_FLAG_State)f->rcvTickFlag;
        p->dpReadFlag        = (enum DP_READ_FLAG_State)f->dpReadFlag;
        p->sciRcvFlag        = (enum SCI_RCV_FLAG_State)f->sciRcvFlag;
        p->commDpStatus      = (enum COMM_DP_STATUS)f->commDpStatus;
    }

    memcpy(ref_rcvData_DP,  f->rcvDP,  sizeof f->rcvDP);
    memcpy(new_rcvData_DP,  f->rcvDP,  sizeof f->rcvDP);
    memcpy(ref_sendData_DP, f->sendDP, sizeof f->sendDP);
    memcpy(new_sendData_DP, f->sendDP, sizeof f->sendDP);

    ref_dataByteNum = f->dataByteNum;
    new_dataByteNum = f->dataByteNum;

    ref_dpParameter.dpAddress    = f->dpAddress;
    ref_dpParameter.dpDataFormat = f->dpDataFormat;
    new_dpParameter.dpAddress    = f->dpAddress;
    new_dpParameter.dpDataFormat = f->dpDataFormat;

    ref_dpSciCommRcvFlag  = (enum DP_SCI_COMM_RCV_FLAG)f->dpRcvFlag;
    new_dpSciCommRcvFlag  = (enum DP_SCI_COMM_RCV_FLAG)f->dpRcvFlag;
    ref_dpSciCommSendFlag = (enum DP_SCI_COMM_SEND_FLAG)f->dpSendFlag;
    new_dpSciCommSendFlag = (enum DP_SCI_COMM_SEND_FLAG)f->dpSendFlag;
}

static int vect_code(PINT v, PINT rx, PINT tx)
{
    if (v == rx)       return 1;
    if (v == tx)       return 2;
    if (v == sentinel) return 0;
    return 3;
}

static void capture_side(STATE *s, int isref)
{
    const struct SCI_DATA_DP *p = isref ? &ref_sciM380DpData : &new_sciM380DpData;
    int i;

    memcpy(s->f.sendSci, p->sendData_SCI, sizeof s->f.sendSci);
    memcpy(s->f.rcvSci,  p->rcvData_SCI,  sizeof s->f.rcvSci);
    s->f.commDpRcvNumber  = p->commDpRcvNumber;
    s->f.commDpSendNum    = p->commDpSendNum;
    s->f.commDpSendNumMax = p->commDpSendNumMax;
    s->f.frameFlagDp      = p->frameFlagDp;
    s->f.rcvTickDp        = p->rcvTickDp;
    s->f.rcvDataJuageFlag = p->rcvDataJuageFlag;
    s->f.rcvCrcErrCounter = p->rcvCrcErrCounter;
    s->f.rcvRigthFlag     = p->rcvRigthFlag;
    s->f.frameStartAll    = p->frameStart.all;
    s->f.frameSendStartAll = p->frameSendStart.all;
    s->f.rcvTickFlag      = (int)p->rcvTickFlag;
    s->f.dpReadFlag       = (int)p->dpReadFlag;
    s->f.sciRcvFlag       = (int)p->sciRcvFlag;
    s->f.commDpStatus     = (int)p->commDpStatus;

    if (isref)
    {
        memcpy(s->f.rcvDP,  ref_rcvData_DP,  sizeof s->f.rcvDP);
        memcpy(s->f.sendDP, ref_sendData_DP, sizeof s->f.sendDP);
        s->f.dataByteNum = ref_dataByteNum;
        s->f.dpAddress   = ref_dpParameter.dpAddress;
        s->f.dpDataFormat = ref_dpParameter.dpDataFormat;
        s->f.dpRcvFlag   = (int)ref_dpSciCommRcvFlag;
        s->f.dpSendFlag  = (int)ref_dpSciCommSendFlag;
    }
    else
    {
        memcpy(s->f.rcvDP,  new_rcvData_DP,  sizeof s->f.rcvDP);
        memcpy(s->f.sendDP, new_sendData_DP, sizeof s->f.sendDP);
        s->f.dataByteNum = new_dataByteNum;
        s->f.dpAddress   = new_dpParameter.dpAddress;
        s->f.dpDataFormat = new_dpParameter.dpDataFormat;
        s->f.dpRcvFlag   = (int)new_dpSciCommRcvFlag;
        s->f.dpSendFlag  = (int)new_dpSciCommSendFlag;
    }

    s->sci  = SciaRegs;
    s->pie  = PieCtrlRegs;
    s->gpio = GpioCtrlRegs;

    s->vect = PieVectTable;
    s->vx = vect_code(s->vect.SCIRXINTA,
                      isref ? (PINT)ref_SCI_DP_RXD_isr : (PINT)new_SCI_DP_RXD_isr,
                      isref ? (PINT)ref_SCI_DP_TXD_isr : (PINT)new_SCI_DP_TXD_isr);
    s->vt = vect_code(s->vect.SCITXINTA,
                      isref ? (PINT)ref_SCI_DP_RXD_isr : (PINT)new_SCI_DP_RXD_isr,
                      isref ? (PINT)ref_SCI_DP_TXD_isr : (PINT)new_SCI_DP_TXD_isr);
    s->vect.SCIRXINTA = (PINT)0;
    s->vect.SCITXINTA = (PINT)0;

    s->f.ier = IER;
    for (i = 0; i < 7; i++)
    {
        s->f.ctl[i] = gHostCpuCtl[i];
    }
}

static int cmp_arr(const Uint16 *a, const Uint16 *b, int n,
                   const char *what, const char *name)
{
    int i;

    for (i = 0; i < n; i++)
    {
        if (a[i] != b[i])
        {
            if (!g_quiet)
            {
                printf("MISMATCH %s: %s[%d] ref=%04X new=%04X\n",
                       what, name, i, a[i], b[i]);
            }
            return 1;
        }
    }
    return 0;
}

#define CMP_SC(field)                                                     \
    do {                                                                  \
        if (a->f.field != b->f.field)                                      \
        {                                                                 \
            if (!g_quiet) printf("MISMATCH %s: %s ref=%ld new=%ld\n", what, \
                   #field, (long)a->f.field, (long)b->f.field);            \
            return 1;                                                     \
        }                                                                 \
    } while (0)

#define CMP_MEM(field, size)                                              \
    do {                                                                  \
        if (memcmp(&a->field, &b->field, (size)) != 0)                     \
        {                                                                 \
            if (!g_quiet) printf("MISMATCH %s: %s differs\n", what, #field);\
            return 1;                                                     \
        }                                                                 \
    } while (0)

/* For fields that live on STATE rather than on STATE.f (the normalised vector
 * codes). */
#define CMP_ST(field)                                                     \
    do {                                                                  \
        if (a->field != b->field)                                          \
        {                                                                 \
            if (!g_quiet) printf("MISMATCH %s: %s ref=%ld new=%ld\n", what, \
                   #field, (long)a->field, (long)b->field);                \
            return 1;                                                     \
        }                                                                 \
    } while (0)

static int cmp_state(const STATE *a, const STATE *b, const char *what)
{
    if (cmp_arr(a->f.sendSci, b->f.sendSci, NSCI, what, "sendData_SCI")) return 1;
    if (cmp_arr(a->f.rcvSci,  b->f.rcvSci,  NSCI, what, "rcvData_SCI"))  return 1;
    if (cmp_arr(a->f.rcvDP,   b->f.rcvDP,   RCV_DATA_NUMBER,  what, "rcvData_DP"))  return 1;
    if (cmp_arr(a->f.sendDP,  b->f.sendDP,  SEND_DATA_NUMBER, what, "sendData_DP")) return 1;

    CMP_SC(commDpRcvNumber);
    CMP_SC(commDpSendNum);
    CMP_SC(commDpSendNumMax);
    CMP_SC(frameFlagDp);
    CMP_SC(rcvTickDp);
    CMP_SC(rcvDataJuageFlag);
    CMP_SC(rcvCrcErrCounter);
    CMP_SC(rcvRigthFlag);
    CMP_SC(frameStartAll);
    CMP_SC(frameSendStartAll);
    CMP_SC(rcvTickFlag);
    CMP_SC(dpReadFlag);
    CMP_SC(sciRcvFlag);
    CMP_SC(commDpStatus);

    CMP_SC(dataByteNum);
    CMP_SC(dpAddress);
    CMP_SC(dpDataFormat);
    CMP_SC(dpRcvFlag);
    CMP_SC(dpSendFlag);

    CMP_MEM(sci,  sizeof a->sci);
    CMP_MEM(pie,  sizeof a->pie);
    CMP_MEM(gpio, sizeof a->gpio);
    CMP_MEM(vect, sizeof a->vect);
    CMP_ST(vx);
    CMP_ST(vt);
    CMP_SC(ier);

    if (memcmp(a->f.ctl, b->f.ctl, sizeof a->f.ctl) != 0)
    {
        printf("MISMATCH %s: EALLOW/EDIS counters differ\n", what);
        return 1;
    }

    if (a->txLen != b->txLen)
    {
        if (!g_quiet)
        {
            printf("MISMATCH %s: TXD byte count ref=%u new=%u\n",
                   what, (unsigned)a->txLen, (unsigned)b->txLen);
        }
        return 1;
    }
    if (cmp_arr(a->txOut, b->txOut, (int)a->txLen, what, "SCITXBUF trace")) return 1;

    return 0;
}

/* ------------------------------------------------------------------------- */
/* one case                                                                  */
/* ------------------------------------------------------------------------- */

enum ENTRY {
    E_RXD, E_TXD, E_START, E_RCV, E_SEND, E_SCI, E_ERR,
    E_INITSET, E_UPDATE, E_GPIOA, E_GPIOB, E_BAUD, E_BYTENUM, E_DPDEAL
};

static void run_side(int isref, enum ENTRY e)
{
    struct SCI_DATA_DP *p = isref ? &ref_sciM380DpData : &new_sciM380DpData;

    switch (e)
    {
    case E_RXD:     if (isref) ref_SCI_DP_RXD_isr();     else new_SCI_DP_RXD_isr();     break;
    case E_TXD:     if (isref) ref_SCI_DP_TXD_isr();     else new_SCI_DP_TXD_isr();     break;
    case E_START:   if (isref) ref_CommDpStartSend(p);   else new_CommDpStartSend(p);   break;
    case E_RCV:     if (isref) ref_CommDpRcvDataDeal(p); else new_CommDpRcvDataDeal(p); break;
    case E_SEND:    if (isref) ref_CommDpSendDataDeal(p);else new_CommDpSendDataDeal(p);break;
    case E_SCI:     if (isref) ref_SciDpDeal(p);         else new_SciDpDeal(p);         break;
    case E_ERR:     if (isref) ref_ErrAgainSend(p);      else new_ErrAgainSend(p);      break;
    case E_INITSET: if (isref) ref_InitSetSciDp(p);      else new_InitSetSciDp(p);      break;
    case E_UPDATE:  if (isref) ref_UpdateSciDpFormat(p); else new_UpdateSciDpFormat(p); break;
    case E_GPIOA:   if (isref) ref_InitSciaGpioDp();     else new_InitSciaGpioDp();     break;
    case E_GPIOB:   if (isref) ref_InitScibGpioDp();     else new_InitScibGpioDp();     break;
    case E_BAUD:    if (isref) ref_InitSciDpBaudRate(p); else new_InitSciDpBaudRate(p); break;
    case E_BYTENUM: if (isref) ref_GetDataByteNum();     else new_GetDataByteNum();     break;
    case E_DPDEAL:  if (isref) ref_DpDataDeal();         else new_DpDataDeal();         break;
    default: break;
    }
}

static void one(const FIELDS *in, enum ENTRY e, const char *what)
{
    STATE a;
    STATE b;

    if (g_stop)
    {
        return;
    }

    g_cases++;

    load(in);
    run_side(1, e);
    memset(&a, 0, sizeof a);
    capture_side(&a, 1);

    load(in);
    run_side(0, e);
    memset(&b, 0, sizeof b);
    capture_side(&b, 0);

    if (cmp_state(&a, &b, what) != 0)
    {
        g_fail++;
        if (g_fail > 20)
        {
            printf("FATAL: too many mismatches, stopping\n");
            /* Not exit(1): the caller distinguishes rc 1 (mismatches) from
             * rc 2 (bench self-check) and rc 3 (a branch never reached), so
             * "gave up early" has to stay inside the mismatch class. */
            g_stop = 1;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase 1: the init functions                                               */
/* ------------------------------------------------------------------------- */

static void phase_init(void)
{
    static const Uint16 ierIn[] = { 0x0000, 0xFFFF, 0x0100, 0xA5A5 };
    static const Uint16 ctlIn[] = { 0x0000, 0xFFFF };
    FIELDS s;
    int i;

    for (i = 0; i < 4; i++)
    {
        setup_defaults(&s);
        s.ier = ierIn[i];
        one(&s, E_INITSET, "InitSetSciDp");
        cov.setdp++;

        one(&s, E_BAUD, "InitSciDpBaudRate");
        cov.baud++;
    }

    for (i = 0; i < 2; i++)
    {
        setup_defaults(&s);
        s.ctl[4] = ctlIn[i];
        one(&s, E_GPIOA, "InitSciaGpioDp");
        cov.gpiodea++;
        one(&s, E_GPIOB, "InitScibGpioDp");
        cov.gpiodeb++;
    }

    /* UpdateSciDpFormat branches on SCIRXST.bit.RXERROR, which decides whether
     * InitSetSciDp runs at all -- so both values of that one bit matter. */
    for (i = 0; i < 2; i++)
    {
        setup_defaults(&s);
        s.rxError = (Uint16)i;
        s.ier = 0x0100;
        one(&s, E_UPDATE, "UpdateSciDpFormat");
        if (i) cov.upd_err++; else cov.upd_noerr++;
    }
}

/* ------------------------------------------------------------------------- */
/* phase 2: GetDataByteNum                                                   */
/* ------------------------------------------------------------------------- */

static void phase_bytenum(void)
{
    static const Uint16 expect[5] = { 12, 20, 0, 4, 32 };
    static const Uint16 dirtyByteNum[] = { 0x0000, 0xFFFF, 0x0005, 0x1234 };
    FIELDS s;
    int f, k;
    STATE a;

    for (f = 1; f <= 5; f++)
    {
        for (k = 0; k < 4; k++)
        {
            setup_defaults(&s);
            s.dpDataFormat = (Uint16)f;
            s.dataByteNum  = dirtyByteNum[k];
            one(&s, E_BYTENUM, "GetDataByteNum");

            /* The table is the specification: assert the value, not just that
             * both sides agree.  Two implementations can agree on the wrong
             * answer, and this one is cheap to pin. */
            load(&s);
            run_side(1, E_BYTENUM);
            memset(&a, 0, sizeof a);
            capture_side(&a, 1);
            if (a.f.dataByteNum != expect[f - 1])
            {
                printf("FATAL: GetDataByteNum(format %d) = %u, expected %u\n",
                       f, (unsigned)a.f.dataByteNum, (unsigned)expect[f - 1]);
                g_fail++;
            }
            cov.fmt[f]++;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase 3: CommDpStartSend                                                  */
/* ------------------------------------------------------------------------- */

static void phase_start(void)
{
    FIELDS s;
    int k;

    for (k = 0; k < 4; k++)
    {
        setup_defaults(&s);
        s.commDpSendNum = (Uint16)(k * 1000);
        s.sendSci[0]    = (Uint16)(0xA000u + k);
        one(&s, E_START, "CommDpStartSend");
        cov.start++;
    }

    /* The function is `void inline`: on the target cl2000 emits no external
     * symbol for it, so it can only be reached through the three callers.  The
     * bench drives it directly as well, because a mutation inside it would
     * otherwise be observed only through whatever its callers happen to do with
     * the result. */
}

/* ------------------------------------------------------------------------- */
/* phase 4: SCI_DP_TXD_isr                                                   */
/* ------------------------------------------------------------------------- */

static void phase_txd(void)
{
    /* Every pair with commDpSendNum < commDpSendNumMax must keep the cursor
     * inside sendData_SCI[40]: SCI_DP_TXD_isr indexes the array with no bound
     * check, so a pair like { 0xFFFE, 0xFFFF } would read far past the end.
     * That is the module's own reachability constraint, not a bench choice --
     * in the firmware commDpSendNumMax is only ever set to dataByteNum + 4 <= 36
     * (SCI_DP_RXD_isr, line 113), so the array is never overrun.  The pairs
     * below therefore stop at 40, and the "cursor already at a huge value" case
     * is expressed as a skip (num >= max) instead of a read. */
    static const Uint16 pairs[][2] = {
        { 0, 0 }, { 0, 1 }, { 1, 1 }, { 5, 6 }, { 6, 5 },
        { 38, 39 }, { 39, 39 }, { 39, 40 }, { 40, 40 }, { 40, 41 },
        { 41, 40 }, { 0, 40 }, { 0xFFFE, 0xFFFE }, { 0xFFFF, 0xFFFF }
    };
    FIELDS s;
    int i;

    for (i = 0; i < (int)(sizeof pairs / sizeof pairs[0]); i++)
    {
        setup_defaults(&s);
        s.commDpSendNum    = pairs[i][0];
        s.commDpSendNumMax = pairs[i][1];
        one(&s, E_TXD, "SCI_DP_TXD_isr");
        if (pairs[i][0] < pairs[i][1]) cov.txd_send++;
        else                           cov.txd_skip++;
    }
}

/* ------------------------------------------------------------------------- */
/* phase 5: SCI_DP_RXD_isr, one byte at a time                               */
/* ------------------------------------------------------------------------- */

/* Classify what the ISR actually did.
 *
 * The selection mirrors the module's if-chain, and the cursor model below
 * tracks what the FIRST block does to commDpRcvNumber / frameFlagDp, because
 * the store block runs afterwards and sees the modified values.  Getting that
 * wrong is easy and it is not cosmetic: an earlier version of this function
 * predicted the cursor from the input alone and reported
 *
 *     "the skip arm moved the cursor 6 -> 2"
 *
 * on the input 0x55 with rcvRigthFlag == 1 -- the 0x55 arm resets the cursor to
 * 1 before the store block runs, so the cursor legitimately ends at 2.  The
 * module was right and the driver's model was wrong.
 *
 * Computed from the ref side only -- the two sides must agree anyway, so doing
 * it twice would just double the bookkeeping. */
static void classify_rxd(const FIELDS *before, const STATE *after)
{
    Uint16 tmp    = before->rxBuf;
    Uint16 cursor = before->commDpRcvNumber;
    Uint16 flag   = before->frameFlagDp;
    Uint16 limit  = (Uint16)(before->dataByteNum + 4);

    if (before->rcvDataJuageFlag != 0)
    {
        cov.rxd_guard++;
    }
    else if (tmp == 0xAA)
    {
        cov.rxd_preamble++;
        if (after->f.rcvRigthFlag != 1)
        {
            printf("FATAL: the 0xAA arm did not raise rcvRigthFlag\n");
            g_fail++;
        }
    }
    else if (before->rcvRigthFlag == 1)
    {
        if (tmp == 0x55)
        {
            cov.rxd_second55++;
            cursor = 1;
            flag   = 2;
            if (after->f.frameFlagDp != 2 || after->f.rcvDataJuageFlag != 1)
            {
                printf("FATAL: the 0x55 arm left flag=%u guard=%u, expected 2/1\n",
                       (unsigned)after->f.frameFlagDp,
                       (unsigned)after->f.rcvDataJuageFlag);
                g_fail++;
            }
        }
        else
        {
            cov.rxd_secondnot++;
            if (after->f.rcvRigthFlag != 0)
            {
                printf("FATAL: the non-0x55 arm did not clear rcvRigthFlag\n");
                g_fail++;
            }
        }
    }
    else
    {
        cov.rxd_reset++;
        cursor = 0;
        flag   = 0;
        if (after->f.commDpSendNum != 0 ||
            after->f.commDpSendNumMax != 0 ||
            after->f.frameFlagDp != 0 ||
            after->f.sciRcvFlag != SCI_RCV_NO)
        {
            printf("FATAL: the reset arm left num=%u max=%u flag=%u rcv=%d\n",
                   (unsigned)after->f.commDpSendNum,
                   (unsigned)after->f.commDpSendNumMax,
                   (unsigned)after->f.frameFlagDp,
                   after->f.sciRcvFlag);
            g_fail++;
        }
    }

    /* The store block, evaluated against the values the first block left. */
    if (flag == 2)
    {
        if (cursor < limit)
        {
            cov.rxd_store++;
            cursor++;
            if (cursor >= limit)
            {
                cov.rxd_storefull++;
                cursor = 0;
            }
        }
        else
        {
            cov.rxd_storeskip++;
        }
    }
    else
    {
        cov.rxd_flagother++;
    }

    if (after->f.commDpRcvNumber != cursor)
    {
        printf("FATAL: the cursor ended at %u, the branch model says %u\n",
               (unsigned)after->f.commDpRcvNumber, (unsigned)cursor);
        g_fail++;
    }
}

static void one_rxd(const FIELDS *in, const char *what)
{
    STATE a;

    one(in, E_RXD, what);

    load(in);
    run_side(1, E_RXD);
    memset(&a, 0, sizeof a);
    capture_side(&a, 1);
    classify_rxd(in, &a);
}

static void phase_rxd(void)
{
    static const Uint16 bytes[] = {
        0xAA, 0x55, 0x00, 0xFF, 0x5A, 0x01AA, 0x0155, 0xFFFF, 0x0100, 0x8000
    };
    static const Uint16 flags[]  = { 0, 1, 2, 3, 0xFFFF };
    static const Uint16 byteNums[] = { 0, 4, 12, 20, 32 };
    FIELDS s;
    int b, f, n;
    Uint16 cur;

    /* Every byte value against every frame-state combination. */
    for (b = 0; b < (int)(sizeof bytes / sizeof bytes[0]); b++)
    {
        for (f = 0; f < (int)(sizeof flags / sizeof flags[0]); f++)
        {
            for (n = 0; n < 5; n++)
            {
                setup_defaults(&s);
                s.rxBuf          = bytes[b];
                s.frameFlagDp    = flags[f];
                s.dataByteNum    = byteNums[n];
                s.rcvRigthFlag   = (Uint16)(b & 1);
                s.rcvDataJuageFlag = (Uint16)(f & 1);
                s.commDpRcvNumber  = (Uint16)(f * 3);
                one_rxd(&s, "SCI_DP_RXD_isr");
            }
        }
    }

    /* The cursor sitting exactly on the frame boundary, both sides of it. */
    for (n = 0; n < 5; n++)
    {
        cur = (Uint16)(byteNums[n] + 4);
        for (b = 0; b < 3; b++)
        {
            setup_defaults(&s);
            s.dataByteNum      = byteNums[n];
            s.frameFlagDp      = 2;
            s.rcvDataJuageFlag = 1;
            s.commDpRcvNumber  = (Uint16)(cur - 1 + b);
            s.rxBuf            = (Uint16)(0x100 + b);
            one_rxd(&s, "SCI_DP_RXD_isr boundary");
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase 6: CommDpRcvDataDeal                                                */
/* ------------------------------------------------------------------------- */

/* Build a frame directly in the shift buffer: header, payload, CRC.  `flip`
 * corrupts the CRC so the reject arm is reachable on purpose. */
static void build_rx_frame(Uint16 *buf, Uint16 n, Uint16 seed, int flip)
{
    Uint16 i;
    Uint16 crc;

    buf[0] = 0xAA;
    buf[1] = 0x55;
    for (i = 0; i < n; i++)
    {
        buf[2 + i] = (Uint16)((seed + i * 7u) & 0x00FFu);
    }
    crc = CrcValueByteCalc(buf, (Uint16)(n + 2));
    if (flip)
    {
        crc = (Uint16)(crc ^ 0x0100u);
    }
    buf[n + 2] = (Uint16)(crc & 0x00FFu);
    buf[n + 3] = (Uint16)((crc >> 8) & 0x00FFu);
}

/* Same discipline as classify_rxd: count the arm, and assert the effect that
 * arm is documented to have.  "CRC accepted" is read off the observable result
 * (dpSciCommRcvFlag latched), not off the input, because the input only says
 * which CRC was planted -- a bench could plant a good CRC and still never reach
 * the accept arm if the frame flag were wrong.
 *
 * One subtlety the assertions have to respect, and which is worth recording
 * because it is a real property of the original code:
 *
 *     the reject arm increments rcvCrcErrCounter BEFORE the trailing
 *     `if (rcvCrcErrCounter > RCV_CRC_ERR_NUMBER)` test.
 *
 * So a counter sitting at 0xFFFF wraps to 0 on the increment and the reset
 * never fires -- the error path can be entered without ever tripping the
 * "give up and re-init" branch.  A naive `before > 10` test in the driver
 * predicts a reset the module does not perform, which is exactly how this was
 * found: the bench reported "reset left cnt=0 flag=0 num=0 max=8", and max=8
 * was the prefill, proving the reset block had not run.  The counter value that
 * matters is the one at the test, i.e. after the possible increment. */
static void classify_rcv(const FIELDS *before, const STATE *after)
{
    int ran      = (before->sciRcvFlag == SCI_RCV_YES);
    int frame2   = (before->frameFlagDp == 2);
    int accepted = (after->f.dpRcvFlag == DP_SCI_COMM_RCV_YES);
    Uint16 cntAtTest = before->rcvCrcErrCounter;
    int resetFired;

    if (ran && frame2 && !accepted)
    {
        cntAtTest = (Uint16)(cntAtTest + 1);
    }
    resetFired = (cntAtTest > RCV_CRC_ERR_NUMBER);

    if (!ran)
    {
        cov.rcv_nosignal++;
        if (!resetFired && after->f.frameFlagDp != before->frameFlagDp)
        {
            printf("FATAL: a frame flag moved without the frame arm running\n");
            g_fail++;
        }
    }
    else if (!frame2)
    {
        cov.rcv_flagother++;
        if (!resetFired && after->f.frameFlagDp != before->frameFlagDp)
        {
            printf("FATAL: a non-2 frame flag was rewritten to %u\n",
                   (unsigned)after->f.frameFlagDp);
            g_fail++;
        }
    }
    else if (accepted)
    {
        cov.rcv_crcok++;
        if (after->f.frameFlagDp != 0)
        {
            printf("FATAL: an accepted frame left frameFlagDp=%u\n",
                   (unsigned)after->f.frameFlagDp);
            g_fail++;
        }
    }
    else
    {
        cov.rcv_crcbad++;
        /* Only meaningful when the trailing reset did not zero it again. */
        if (!resetFired && after->f.rcvCrcErrCounter != cntAtTest)
        {
            printf("FATAL: a rejected frame left the error counter %u, "
                   "expected %u\n",
                   (unsigned)after->f.rcvCrcErrCounter, (unsigned)cntAtTest);
            g_fail++;
        }
    }

    if (resetFired)
    {
        cov.rcv_reset++;
        if (after->f.rcvCrcErrCounter != 0 ||
            after->f.frameFlagDp != 0 ||
            after->f.commDpSendNum != 0 ||
            after->f.commDpSendNumMax != 0 ||
            after->f.sciRcvFlag != SCI_RCV_NO)
        {
            printf("FATAL: the error-counter reset left cnt=%u flag=%u "
                   "num=%u max=%u rcv=%d\n",
                   (unsigned)after->f.rcvCrcErrCounter,
                   (unsigned)after->f.frameFlagDp,
                   (unsigned)after->f.commDpSendNum,
                   (unsigned)after->f.commDpSendNumMax,
                   after->f.sciRcvFlag);
            g_fail++;
        }
    }
    else
    {
        cov.rcv_norereset++;
    }
}

static void one_rcv(const FIELDS *in, const char *what)
{
    STATE a;

    one(in, E_RCV, what);

    load(in);
    run_side(1, E_RCV);
    memset(&a, 0, sizeof a);
    capture_side(&a, 1);
    classify_rcv(in, &a);
}

static void phase_rcv(void)
{
    static const Uint16 byteNums[] = { 0, 4, 12, 20, 32 };
    static const Uint16 errCnt[]   = { 0, 9, 10, 11, 65535 };
    static const Uint16 flags[]    = { 0, 1, 2, 3 };
    FIELDS s;
    int n, e, f, g, x;

    for (n = 0; n < 5; n++)
    {
        for (f = 0; f < 4; f++)
        {
            for (g = 0; g < 2; g++)         /* CRC good / bad */
            {
                for (e = 0; e < 5; e++)
                {
                    for (x = 0; x < 2; x++)  /* sciRcvFlag */
                    {
                        setup_defaults(&s);
                        s.dataByteNum      = byteNums[n];
                        s.frameFlagDp      = flags[f];
                        s.rcvCrcErrCounter = errCnt[e];
                        s.sciRcvFlag       = x ? SCI_RCV_YES : SCI_RCV_NO;
                        build_rx_frame(s.rcvSci, byteNums[n], (Uint16)(0x40 + n), g);
                        one_rcv(&s, "CommDpRcvDataDeal");
                    }
                }
            }
        }
    }

    /* Random payloads through the same path, both CRC outcomes. */
    for (x = 0; x < 200; x++)
    {
        setup_defaults(&s);
        s.dataByteNum      = byteNums[x % 5];
        s.frameFlagDp      = 2;
        s.rcvCrcErrCounter = rnd16();
        s.sciRcvFlag       = SCI_RCV_YES;
        build_rx_frame(s.rcvSci, byteNums[x % 5], rnd16(), (x & 1));
        one_rcv(&s, "CommDpRcvDataDeal random");
    }
}

/* ------------------------------------------------------------------------- */
/* phase 7: CommDpSendDataDeal                                               */
/* ------------------------------------------------------------------------- */

static void phase_send(void)
{
    static const Uint16 byteNums[] = { 0, 4, 12, 20, 32 };
    static const Uint16 seeds[]    = { 0x00, 0x5A, 0xFF };
    FIELDS s;
    int n, k, f;

    for (n = 0; n < 5; n++)
    {
        for (k = 0; k < 3; k++)
        {
            for (f = 0; f < 2; f++)
            {
                Uint16 i;

                setup_defaults(&s);
                s.dataByteNum  = byteNums[n];
                s.dpSendFlag   = f ? DP_SCI_COMM_SEND_YES : DP_SCI_COMM_SEND_NO;
                s.commDpSendNum = 0x1234;
                s.frameSendStartAll = 0xA5A5;
                for (i = 0; i < RCV_DATA_NUMBER; i++)
                {
                    s.rcvDP[i] = (Uint16)((seeds[k] + i * 3u) & 0x00FFu);
                }
                one(&s, E_SEND, "CommDpSendDataDeal");
                if (f) cov.send_yes++; else cov.send_no++;
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase 8: ErrAgainSend                                                     */
/* ------------------------------------------------------------------------- */

static void phase_err(void)
{
    static const Uint16 byteNums[] = { 0, 1, 4, 12, 20, 32 };
    FIELDS s;
    int n, k;

    for (n = 0; n < 6; n++)
    {
        for (k = 0; k < 3; k++)
        {
            setup_defaults(&s);
            s.dataByteNum       = byteNums[n];
            s.frameSendStartAll = (Uint16)(0x3F3F + k);
            s.commDpSendNum     = (Uint16)(k * 77);
            one(&s, E_ERR, "ErrAgainSend");
            cov.errsend++;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase 9: DpDataDeal                                                       */
/* ------------------------------------------------------------------------- */

static void phase_dpdeal(void)
{
    static const int rcvFlags[] = { DP_SCI_COMM_RCV_NO, DP_SCI_COMM_RCV_YES };
    static const int sendFlags[] = { DP_SCI_COMM_SEND_NO, DP_SCI_COMM_SEND_YES };
    FIELDS s;
    int r, w;

    for (r = 0; r < 2; r++)
    {
        for (w = 0; w < 2; w++)
        {
            setup_defaults(&s);
            s.dpRcvFlag  = rcvFlags[r];
            s.dpSendFlag = sendFlags[w];
            one(&s, E_DPDEAL, "DpDataDeal");
            if (r) cov.dpdeal_pass++; else cov.dpdeal_early++;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase 10: SciDpDeal, the whole polled pipeline                            */
/* ------------------------------------------------------------------------- */

static void phase_scideal(void)
{
    static const Uint16 errCnt[] = { 0, 10, 11 };
    FIELDS s;
    int g, e, x;

    for (g = 0; g < 2; g++)
    {
        for (e = 0; e < 3; e++)
        {
            for (x = 0; x < 2; x++)
            {
                setup_defaults(&s);
                s.dpDataFormat     = 5;      /* SciDpDeal forces this anyway */
                s.dataByteNum      = 32;
                s.frameFlagDp      = 2;
                s.sciRcvFlag       = SCI_RCV_YES;
                s.rcvCrcErrCounter = errCnt[e];
                s.dpRcvFlag        = x ? DP_SCI_COMM_RCV_YES : DP_SCI_COMM_RCV_NO;
                s.dpSendFlag       = DP_SCI_COMM_SEND_NO;
                s.rxError          = (Uint16)(g & 1);
                build_rx_frame(s.rcvSci, 32, 0x11, g);
                one(&s, E_SCI, "SciDpDeal");
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase 11: full frame round trip through the ISRs                          */
/* ------------------------------------------------------------------------- */

/* Feed one complete frame byte by byte through SCI_DP_RXD_isr, then let
 * SciDpDeal consume it and CommDpSendDataDeal build the reply, then drain the
 * reply one byte at a time through SCI_DP_TXD_isr.  This is the only phase that
 * exercises the ISR/scheduler handshake as a sequence rather than as isolated
 * calls: the RX cursor and the frame flags are set by one entry point and
 * consumed by another.
 *
 * Note where the reply's first byte goes: CommDpStartSend writes sendData_SCI[0]
 * to SCITXBUF itself and leaves commDpSendNum at 1, so the drain loop observes
 * bytes 1..n-1 and the first byte is only visible in the buffer.  Both are
 * compared -- the buffer as part of the normal state diff, the SCITXBUF trace
 * through txOut. */
static void phase_frame(void)
{
    static const Uint16 byteNums[] = { 0, 4, 12, 20, 32 };
    FIELDS s;
    int n, g;
    Uint16 i;

    for (n = 0; n < 5; n++)
    {
        for (g = 0; g < 2; g++)
        {
            STATE a;
            STATE b;
            Uint16 frame[NSCI];
            Uint16 len;

            g_cases++;

            len = (Uint16)(byteNums[n] + 4);

            /* ---------- reference side ---------- */
            setup_defaults(&s);
            s.dataByteNum  = byteNums[n];
            s.dpDataFormat = (Uint16)(n + 1);

            load(&s);
            build_rx_frame(frame, byteNums[n], (Uint16)(0x60 + n), g);
            for (i = 0; i < len; i++)
            {
                SciaRegs.SCIRXBUF.all = frame[i];
                ref_SCI_DP_RXD_isr();
            }
            ref_SciDpDeal(&ref_sciM380DpData);

            memset(&a, 0, sizeof a);
            capture_side(&a, 1);

            /* Drain whatever is pending, recording each byte handed to the
             * transmit register. */
            while (ref_sciM380DpData.commDpSendNum <
                   ref_sciM380DpData.commDpSendNumMax && a.txLen < NSCI)
            {
                ref_SCI_DP_TXD_isr();
                a.txOut[a.txLen++] = SciaRegs.SCITXBUF;
            }

            /* The reply must be a well-formed DSP_TO_DP frame.  Asserted on the
             * ref side, so a bench that silently stopped producing frames is
             * caught here instead of being excused as "both sides agree". */
            if (byteNums[n] == 32 && g == 0)
            {
                if (ref_sciM380DpData.sendData_SCI[0] != 0xAA ||
                    ref_sciM380DpData.sendData_SCI[1] != 0x55)
                {
                    printf("FATAL: reply header %04X %04X, expected AA 55\n",
                           ref_sciM380DpData.sendData_SCI[0],
                           ref_sciM380DpData.sendData_SCI[1]);
                    g_fail++;
                }
                if (ref_sciM380DpData.sendData_SCI[34] !=
                    (CrcValueByteCalc(ref_sciM380DpData.sendData_SCI, 34) & 0xFF) ||
                    ref_sciM380DpData.sendData_SCI[35] !=
                    ((CrcValueByteCalc(ref_sciM380DpData.sendData_SCI, 34) >> 8) & 0xFF))
                {
                    printf("FATAL: reply CRC is not over the 34 header+payload bytes\n");
                    g_fail++;
                }
                if (ref_sciM380DpData.commDpSendNumMax != 36)
                {
                    printf("FATAL: reply length %u, expected 36\n",
                           (unsigned)ref_sciM380DpData.commDpSendNumMax);
                    g_fail++;
                }
            }

            /* ---------- new side ---------- */
            load(&s);
            for (i = 0; i < len; i++)
            {
                SciaRegs.SCIRXBUF.all = frame[i];
                new_SCI_DP_RXD_isr();
            }
            new_SciDpDeal(&new_sciM380DpData);

            memset(&b, 0, sizeof b);
            capture_side(&b, 0);

            while (new_sciM380DpData.commDpSendNum <
                   new_sciM380DpData.commDpSendNumMax && b.txLen < NSCI)
            {
                new_SCI_DP_TXD_isr();
                b.txOut[b.txLen++] = SciaRegs.SCITXBUF;
            }

            if (cmp_state(&a, &b, "full frame round trip") != 0)
            {
                g_fail++;
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* phase 12: random                                                          */
/* ------------------------------------------------------------------------- */

static void phase_random(int n)
{
    static const Uint16 byteNums[] = { 0, 4, 12, 20, 32 };
    FIELDS s;
    int k;
    Uint16 i;

    for (k = 0; k < n; k++)
    {
        setup_defaults(&s);

        for (i = 0; i < NSCI; i++)
        {
            s.sendSci[i] = rnd16();
            s.rcvSci[i]  = rnd16();
        }
        for (i = 0; i < RCV_DATA_NUMBER; i++)
        {
            s.rcvDP[i]  = rnd16();
            s.sendDP[i] = rnd16();
        }

        s.commDpRcvNumber  = rnd16();
        s.commDpSendNum    = rnd16();
        s.commDpSendNumMax = rnd16();
        /* SCI_DP_TXD_isr indexes sendData_SCI with no bound check, so a
         * "pending" cursor past the end of the 40-word array is an
         * out-of-bounds read in the module itself.  Fold those draws into the
         * equivalent skip case instead of feeding the bench a fault. */
        if (s.commDpSendNum < s.commDpSendNumMax && s.commDpSendNum >= NSCI)
        {
            s.commDpSendNum = s.commDpSendNumMax;
        }
        s.frameFlagDp      = rnd16();
        s.rcvTickDp        = rnd16();
        s.rcvDataJuageFlag = rnd16();
        s.rcvCrcErrCounter = rnd16();
        s.rcvRigthFlag     = rnd16();
        s.frameStartAll    = rnd16();
        s.frameSendStartAll = rnd16();
        s.rcvTickFlag      = (int)(rnd16() & 1);
        s.dpReadFlag       = (int)(rnd16() & 1);
        s.sciRcvFlag       = (int)(rnd16() & 1);
        s.commDpStatus     = (int)(rnd16() & 1);

        s.dataByteNum  = byteNums[rnd16() % 5];
        s.dpAddress    = rnd16();
        s.dpDataFormat = (Uint16)(1 + rnd16() % 5);
        s.dpRcvFlag    = (int)(rnd16() & 1);
        s.dpSendFlag   = (int)(rnd16() & 1);

        s.rxBuf   = rnd16();
        s.rxError = (Uint16)(rnd16() & 1);
        s.ier     = rnd16();
        for (i = 0; i < 7; i++)
        {
            s.ctl[i] = rnd16();
        }

        switch (rnd16() % 14)
        {
        case 0:  one(&s, E_RXD,     "random RXD");     break;
        case 1:  one(&s, E_TXD,     "random TXD");     break;
        case 2:  one(&s, E_START,   "random START");   break;
        case 3:  one(&s, E_RCV,     "random RCV");     break;
        case 4:  one(&s, E_SEND,    "random SEND");    break;
        case 5:  one(&s, E_SCI,     "random SCI");     break;
        case 6:  one(&s, E_ERR,     "random ERR");     break;
        case 7:  one(&s, E_INITSET, "random INITSET"); break;
        case 8:  one(&s, E_UPDATE,  "random UPDATE");  break;
        case 9:  one(&s, E_GPIOA,   "random GPIOA");   break;
        case 10: one(&s, E_GPIOB,   "random GPIOB");   break;
        case 11: one(&s, E_BAUD,    "random BAUD");    break;
        case 12: one(&s, E_BYTENUM, "random BYTENUM"); break;
        default: one(&s, E_DPDEAL,  "random DPDEAL");  break;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* self-check: the bench itself                                              */
/* ------------------------------------------------------------------------- */

static int bench_selfcheck(void)
{
    FIELDS s;
    STATE a;

    /* 1. The register image must round-trip: write a known value through the
     *    struct the module actually uses and read it back.  If this fails, the
     *    case is diffing a copy of the wrong object. */
    setup_defaults(&s);
    load(&s);
    SciaRegs.SCITXBUF = 0x1234;
    if (SciaRegs.SCITXBUF != 0x1234)
    {
        printf("SELFCHECK: SCITXBUF does not round-trip\n");
        return 0;
    }
    if (SciaRegs.SCIRXBUF.all != s.rxBuf)
    {
        printf("SELFCHECK: SCIRXBUF prefill lost (%04X, expected %04X)\n",
               SciaRegs.SCIRXBUF.all, s.rxBuf);
        return 0;
    }

    /* 2. pSciRegs must point at the mock object, or every register write lands
     *    somewhere else and the whole comparison is vacuous. */
    if (ref_sciM380DpData.pSciRegs != (volatile struct SCI_REGS *)&SciaRegs ||
        new_sciM380DpData.pSciRegs != (volatile struct SCI_REGS *)&SciaRegs)
    {
        printf("SELFCHECK: pSciRegs is not wired to the mock SciaRegs\n");
        return 0;
    }

    /* 3. The two sides must be able to produce different values at all.  If
     *    cmp_state cannot see a planted difference, it will never see a real
     *    one either. */
    load(&s);
    ref_sciM380DpData.commDpSendNum = 1;
    new_sciM380DpData.commDpSendNum = 2;
    memset(&a, 0, sizeof a);
    capture_side(&a, 1);
    {
        STATE b;
        memset(&b, 0, sizeof b);
        capture_side(&b, 0);
        g_quiet = 1;
        if (cmp_state(&a, &b, "selfcheck") == 0)
        {
            g_quiet = 0;
            printf("SELFCHECK: cmp_state did not see a planted difference\n");
            return 0;
        }
        g_quiet = 0;
    }
    /* Undo the noise from check 3 before the counters start. */
    g_cases = 0;
    g_fail = 0;
    g_stop = 0;

    /* 4. CrcValueByteCalc must be a real CRC: two different buffers must not
     *    collide, or every "wrong data" mutation would collapse into "wrong
     *    CRC, same branch". */
    {
        Uint16 x[4] = { 0xAA, 0x55, 0x01, 0x02 };
        Uint16 y[4] = { 0xAA, 0x55, 0x01, 0x03 };

        if (CrcValueByteCalc(x, 4) == CrcValueByteCalc(y, 4))
        {
            printf("SELFCHECK: CrcValueByteCalc collides on adjacent buffers\n");
            return 0;
        }
        if (CrcValueByteCalc(x, 0) != 0xFFFF)
        {
            printf("SELFCHECK: CrcValueByteCalc(len 0) is not the 0xFFFF seed\n");
            return 0;
        }
    }

    printf("SELFCHECK: ok\n");
    return 1;
}

/* ------------------------------------------------------------------------- */
/* main                                                                      */
/* ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    int nrand = 20000;

    /* Unbuffered: if a case ever faults, the last phase printed is the one that
     * faulted.  A fully-buffered stdout would lose all of it to the crash and
     * leave nothing but a return code. */
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc > 1)
    {
        long v = atol(argv[1]);

        if (v > 0)
        {
            nrand = (int)v;
        }
    }

    if (!bench_selfcheck())
    {
        return 2;
    }

    printf("phase init\n");
    phase_init();
    printf("phase bytenum\n");
    phase_bytenum();
    printf("phase start\n");
    phase_start();
    printf("phase txd\n");
    phase_txd();
    printf("phase rxd\n");
    phase_rxd();
    printf("phase rcv\n");
    phase_rcv();
    printf("phase send\n");
    phase_send();
    printf("phase err\n");
    phase_err();
    printf("phase dpdeal\n");
    phase_dpdeal();
    printf("phase scideal\n");
    phase_scideal();
    printf("phase frame\n");
    phase_frame();
    printf("phase random\n");
    phase_random(nrand);

    printf("f_comm_DP difftest: cases=%ld mismatches=%ld\n", g_cases, g_fail);
    printf("reach: rxd pre=%ld second55=%ld secondNot=%ld reset=%ld guard=%ld "
           "store=%ld full=%ld skip=%ld flagOther=%ld\n",
           cov.rxd_preamble, cov.rxd_second55, cov.rxd_secondnot,
           cov.rxd_reset, cov.rxd_guard, cov.rxd_store, cov.rxd_storefull,
           cov.rxd_storeskip, cov.rxd_flagother);
    printf("       txd send=%ld skip=%ld | rcv nosig=%ld flagOther=%ld "
           "crcOk=%ld crcBad=%ld reset=%ld noReset=%ld\n",
           cov.txd_send, cov.txd_skip, cov.rcv_nosignal, cov.rcv_flagother,
           cov.rcv_crcok, cov.rcv_crcbad, cov.rcv_reset, cov.rcv_norereset);
    printf("       send yes=%ld no=%ld | dpdeal early=%ld pass=%ld | "
           "upd err=%ld noerr=%ld\n",
           cov.send_yes, cov.send_no, cov.dpdeal_early, cov.dpdeal_pass,
           cov.upd_err, cov.upd_noerr);
    printf("       fmt 12=%ld 20=%ld 0=%ld 4=%ld 32=%ld | start=%ld err=%ld "
           "gpioA=%ld gpioB=%ld setDp=%ld baud=%ld\n",
           cov.fmt[1], cov.fmt[2], cov.fmt[3], cov.fmt[4], cov.fmt[5],
           cov.start, cov.errsend, cov.gpiodea, cov.gpiodeb,
           cov.setdp, cov.baud);

    /* Every hand-built branch must actually have been reached.  Without this,
     * "0 mismatches" could just mean "that code never ran". */
    if (!cov.rxd_preamble || !cov.rxd_second55 || !cov.rxd_secondnot ||
        !cov.rxd_reset || !cov.rxd_guard || !cov.rxd_store ||
        !cov.rxd_storefull || !cov.rxd_storeskip || !cov.rxd_flagother ||
        !cov.txd_send || !cov.txd_skip ||
        !cov.rcv_nosignal || !cov.rcv_flagother || !cov.rcv_crcok ||
        !cov.rcv_crcbad || !cov.rcv_reset || !cov.rcv_norereset ||
        !cov.send_yes || !cov.send_no ||
        !cov.dpdeal_early || !cov.dpdeal_pass ||
        !cov.upd_err || !cov.upd_noerr ||
        !cov.fmt[1] || !cov.fmt[2] || !cov.fmt[3] || !cov.fmt[4] || !cov.fmt[5] ||
        !cov.start || !cov.errsend || !cov.gpiodea || !cov.gpiodeb ||
        !cov.setdp || !cov.baud)
    {
        printf("FATAL: a hand-built branch was never reached\n");
        return 3;
    }

    return (g_fail == 0) ? 0 : 1;
}
