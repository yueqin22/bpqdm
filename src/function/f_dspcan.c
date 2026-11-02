
#include "DSP28x_Project.h"

#include "f_funcCode.h"
#include "f_dspcan.h"

#define DEBUG_F_CAN              1

#if DEBUG_F_CAN

#if (DSP_CLOCK == 100)
#define     DSPCAN_CLK      100000
#else
#define     DSPCAN_CLK      30000
#endif

const CAN_BAUD eCanBaud[CAN_BAUD_SUM] = {
    {(DSPCAN_CLK / 20 / 20) - 1, 3, 14},
    {(DSPCAN_CLK / 20 / 50) - 1, 3, 14},
    {(DSPCAN_CLK / 20 / 100) - 1, 3, 14},
    {(DSPCAN_CLK / 20 / 125) - 1, 3, 14},
    {(DSPCAN_CLK / 20 / 250) - 1, 3, 14},
    {(DSPCAN_CLK / 20 / 500) - 1, 3, 14},
    {(DSPCAN_CLK / 10 / 1000) - 1, 1, 6}
};

Uint32 eCanTranEnFlag;
Uint32 eCanReEnFlag;

#define     IINIT_CAN_TIME      3

Uint16 InitdspECan(Uint16 baud)
{
    struct ECAN_REGS ECanaShadow;
    Uint32 *MsgCtrlPi;
    Uint16 i;
    static Uint16 con = 0;
    static Uint16 count = 0;

    if (baud >= CAN_BAUD_SUM)
    {
        return CAN_INIT_BAUD_ERR;
    }
    if (count > IINIT_CAN_TIME)
    {
        return CAN_INIT_TIMEOUT;
    }

    EALLOW;

    if (con == 0)
    {
        GpioCtrlRegs.GPAPUD.bit.GPIO30 = 0;
        GpioCtrlRegs.GPAPUD.bit.GPIO31 = 0;

        GpioCtrlRegs.GPAQSEL2.bit.GPIO30 = 3;

        GpioCtrlRegs.GPAMUX2.bit.GPIO30 = 1;
        GpioCtrlRegs.GPAMUX2.bit.GPIO31 = 1;

        ECANREGS.CANTIOC.bit.TXFUNC = 1;
        ECANREGS.CANRIOC.bit.RXFUNC = 1;

        ECanaShadow.CANMC.all = 0;
        ECanaShadow.CANMC.bit.SRES = 1;
        ECANREGS.CANMC.all = ECanaShadow.CANMC.all;

        ECanaShadow.CANMC.all = ECANREGS.CANMC.all;
        ECanaShadow.CANMC.bit.SCB = 1;
        ECanaShadow.CANMC.bit.SUSP = 1;
        ECanaShadow.CANMC.bit.CCR = 1;
        ECanaShadow.CANMC.bit.ABO = 1;
        ECANREGS.CANMC.all = ECanaShadow.CANMC.all;

        MsgCtrlPi = (Uint32 *)(&ECANMBOXES.MBOX0.MSGCTRL);
        for (i = 0; i < 32; i++)
        {
            *MsgCtrlPi = 0x00000000;
            MsgCtrlPi += 4;
        }

        MsgCtrlPi = (Uint32 *)(&ECANLAMS.LAM0);
        for (i = 0; i < 32; i++)
        {
            MsgCtrlPi[i] = 0x00000000;
        }

        ECANREGS.CANTRR.all = 0xFFFFFFFF;
        ECANREGS.CANTA.all = 0xFFFFFFFF;
        ECANREGS.CANRMP.all = 0xFFFFFFFF;
        ECANREGS.CANGIF0.all = 0xFFFFFFFF;
        ECANREGS.CANGIF1.all = 0xFFFFFFFF;
        ECANREGS.CANOPC.all = 0;

        con = 1;
    }

    if (con == 1)
    {
        ECanaShadow.CANES.all = ECANREGS.CANES.all;
        if (ECanaShadow.CANES.bit.CCE == 0)
        {
            count++;
            EDIS;
            return CAN_INIT_TIME;
        }
        else
        {
            con = 2;
        }
    }

    if (con == 2)
    {
        ECanaShadow.CANBTC.all = 0;
        ECanaShadow.CANBTC.bit.BRPREG = eCanBaud[baud].BRPREG;
        ECanaShadow.CANBTC.bit.TSEG2REG = eCanBaud[baud].TSEG2REG;
        ECanaShadow.CANBTC.bit.TSEG1REG = eCanBaud[baud].TSEG1REG;
        ECanaShadow.CANBTC.bit.SAM = 0;
        ECANREGS.CANBTC.all = ECanaShadow.CANBTC.all;

        ECanaShadow.CANMC.all = ECANREGS.CANMC.all;
        ECanaShadow.CANMC.bit.CCR = 0;
        ECANREGS.CANMC.all = ECanaShadow.CANMC.all;

        con = 3;
    }

    if (con == 3)
    {
        ECanaShadow.CANES.all = ECANREGS.CANES.all;
        if (ECanaShadow.CANES.bit.CCE == 1)
        {
            count++;
            EDIS;
            return CAN_INIT_TIME;
        }
    }

    con = 0;
    count = 0;
    ECANREGS.CANME.all = 0;

    EDIS;

    eCanTranEnFlag = 0;
    eCanReEnFlag = 0;

    return CAN_INIT_SUCC;
}

void InitTranMbox(Uint16 mbox, Uint32 msgid, Uint32 *dataPi)
{
    Uint16 id;
    Uint32 ECanaShadow;
    Uint32 *msgIdPi;

    id = mbox & 0x1f;
    eCanTranEnFlag |= 1ul << mbox;

    msgIdPi = (Uint32 *)(&ECANMBOXES.MBOX0.MSGID);
    msgIdPi[id << 2] = msgid;
    msgIdPi[(id << 2) + 1] = 8;

    ECanaShadow = ECANREGS.CANMD.all;
    ECanaShadow &= ~(1ul << id);
    ECANREGS.CANMD.all = ECanaShadow;

    ECanaShadow = ECANREGS.CANME.all;
    ECanaShadow |= 1ul << id;
    ECANREGS.CANME.all = ECanaShadow;

    msgIdPi[(id << 2) + 2] = dataPi[0];
    msgIdPi[(id << 2) + 3] = dataPi[1];
}

void InitRecMbox(Uint16 mbox, Uint32 msgid, Uint32 lam)
{
    Uint16 id;
    Uint32 ECanaShadow;
    Uint32 *pi;

    id = mbox & 0x1f;
    eCanReEnFlag |= 1ul << id;

    pi = (Uint32 *)(&ECANMBOXES.MBOX0.MSGID);
    pi[id << 2] = msgid;

    if (mbox & 0x80)
    {
        pi[(id << 2) + 1] = (1 << 4) | 8;
    }
    else
    {
        pi[(id << 2) + 1] = 8;
    }

    ECanaShadow = ECANREGS.CANOPC.all;
    if (mbox & 0x40)
    {
        ECanaShadow |= 1ul << id;
    }
    else
    {
        ECanaShadow &= ~(1ul << id);
    }
    ECANREGS.CANOPC.all = ECanaShadow;

    ECanaShadow = ECANREGS.CANMD.all;
    ECanaShadow |= 1ul << id;
    ECANREGS.CANMD.all = ECanaShadow;

    ECanaShadow = ECANREGS.CANME.all;
    ECanaShadow |= 1ul << id;
    ECANREGS.CANME.all = ECanaShadow;

    pi = (Uint32 *)(&ECANLAMS.LAM0);
    pi[id] = lam;
}

Uint16 eCanDataTran(Uint16 mbox, Uint16 len, Uint32 msgid, Uint32 *dataPi)
{
    Uint32 ECanaShadow;
    Uint32 *pi;

    mbox &= 0x1f;

    if (!(eCanTranEnFlag & (1ul << mbox)))
    {
        return CAN_MBOX_NUM_ERROR;
    }

    if (ECANREGS.CANTRS.all & (1ul << mbox))
    {
        return CAN_MBOX_BUSY;
    }

    ECANREGS.CANTA.all = 1ul << mbox;

    pi = (Uint32 *)(&ECANMBOXES.MBOX0.MSGID);

    msgid &= ~(0x7ul << 29);
    msgid |= pi[mbox << 2] & (0x7ul << 29);

    ECanaShadow = ECANREGS.CANME.all;
    ECanaShadow &= ~(1ul << mbox);
    ECANREGS.CANME.all = ECanaShadow;

    pi[mbox << 2] = msgid;
    pi[(mbox << 2) + 1] = len;
    pi[(mbox << 2) + 2] = dataPi[0];
    pi[(mbox << 2) + 3] = dataPi[1];

    ECanaShadow |= 1ul << mbox;
    ECANREGS.CANME.all = ECanaShadow;

    ECANREGS.CANTRS.all = 1ul << mbox;

    return CAN_MBOX_TRAN_SUCC;
}

Uint16 eCanDataRec(Uint16 mbox, Uint32 *dataPi)
{
    Uint32 *pi;

    mbox &= 0x1f;

    if (!(ECANREGS.CANRMP.all & (1ul << mbox)))
    {
        return CAN_MBOX_EMPTY;
    }

    pi = (Uint32 *)(&ECANMBOXES.MBOX0.MSGID);
    dataPi[0] = pi[mbox << 2];
    dataPi[1] = pi[(mbox << 2) + 2];
    dataPi[2] = pi[(mbox << 2) + 3];
    dataPi[3] = pi[(mbox << 2) + 1] & 0xf;

    ECANREGS.CANRMP.all = 1ul << mbox;

    if (ECANREGS.CANRML.all & (1ul << mbox))
    {
        return CAN_MBOX_REC_OVER;
    }

    return CAN_MBOX_REC_SUCC;
}

#elif 1

#endif
