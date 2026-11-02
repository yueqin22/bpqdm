#include "MotorPwmInclude.h"

OUT_VOLT_STRUCT             gOutVolt;
PWM_OUT_STRUCT              gPWM;
Uint                        gRatio;

Uint                        EPWMFlagU;
Uint                        EPWMFlagV;
Uint                        EPWMFlagW;

Uint    iTrig1_Cpwm[12] = {0,0,1,1,1,1,2,2,2,2,0,0};
Uint    iTrig1_Dpwm[12] = {0,2,2,1,1,0,0,2,2,1,1,0};
Uint    iTrig2_Pwm[12]  = {1,1,0,0,2,2,1,1,0,0,2,2};
Uint    iTrig3_Cpwm[12] = {2,2,2,2,0,0,0,0,1,1,1,1};
Uint    iTrig3_Dpwm[12] = {2,0,1,2,0,1,2,0,1,2,0,1};

#ifdef TMS320F2808
#define C_TZ_SEL            0x010E
#else
#define C_TZ_SEL            0x0102
#endif

static void SetPwmTimerBase(volatile struct EPWM_REGS *pReg, Uint tbCtl)
{
    pReg->TBPRD = C_INIT_PRD;
    pReg->TBPHS.all = 0;
    pReg->TBCTL.all = tbCtl;

    pReg->TBCTL.bit.HSPCLKDIV = PWM_CLK_DIV;

    pReg->CMPCTL.all = 0x0100;

    pReg->CMPA.half.CMPA = C_INIT_PRD/2;

    pReg->AQCTLA.all = 0x0090;

    pReg->AQSFRC.all  = 0x0;
    pReg->AQCSFRC.all = 0x0;
    pReg->DBCTL.all = 0x0007;

    pReg->DBFED = C_MAX_DB;
    pReg->DBRED = C_MAX_DB;
}

static void SetPwmTripZone(volatile struct EPWM_REGS *pReg)
{
    pReg->TZSEL.all = C_TZ_SEL;
    pReg->TZCTL.all = 0x0005;
#ifndef TMS320F2808
    pReg->TZEINT.all = 0x0004;
#endif
}

void InitSetPWM(void)
{
    EALLOW;

    SetPwmTimerBase(&EPwm1Regs, 0xE012);
    EPwm1Regs.CMPB = EPwm1Regs.TBPRD - gADC.DelayApply;

    EPwm1Regs.TZCLR.all = 0x07;
    SetPwmTripZone(&EPwm1Regs);
    EPwm1Regs.TZEINT.all = 0x0004;

    EPwm1Regs.ETCLR.bit.INT = 1;
    EPwm1Regs.ETSEL.all = 0x0F09;

    EPwm1Regs.ETPS.all = 0x0101;

    SetPwmTimerBase(&EPwm2Regs, 0xE006);
    SetPwmTripZone(&EPwm2Regs);

    EPwm2Regs.ETSEL.all = 0;
    EPwm2Regs.ETPS.all  = 0;

    SetPwmTimerBase(&EPwm3Regs, 0xE036);
    SetPwmTripZone(&EPwm3Regs);

    EDIS;
}

static void OutputOnePhase(long *pOut, int phase, int comp3, int ratio)
{
    int m_Sin;

    m_Sin = qsin(16384 - phase) + comp3;
    m_Sin = 4096 - (((long)ratio * (long)m_Sin) >> 15);

    *pOut = ((long)m_Sin * (long)gPWM.gPWMPrdApply) >> 13;
}

void OutPutPWMVC(void)
{
    int     phase;
    int     phase3;
    int     Comp3;
    int     m_Ratio;

    phase = gPhase.OutPhase;
    phase3 = 16384 - phase;
    phase3 = phase3 + (phase3 << 1);
    Comp3 = qsin(phase3) >> 2;

    m_Ratio = ((long)gRatio * 4710) >> 12;

    OutputOnePhase(&gPWM.U, phase, Comp3, m_Ratio);
    OutputOnePhase(&gPWM.V, phase - 21845, Comp3, m_Ratio);
    OutputOnePhase(&gPWM.W, phase - 21845 - 21845, Comp3, m_Ratio);

    DeadBandComp();
    SendPWM();
}

static void SetPhaseLength(Uint sel, Uint len)
{
    long m_Len;

    m_Len = (long)len;

    switch (sel)
    {
        case ZERO_VECTOR_U:
            gPWM.U = m_Len;
            break;
        case ZERO_VECTOR_V:
            gPWM.V = m_Len;
            break;
        case ZERO_VECTOR_W:
            gPWM.W = m_Len;
            break;
        default:
            break;
    }
}

void OutPutPWMVF(void)
{
    Uint    m_iSec30;
    Uint    m_iSec60;
    Uint    m_iRamainPhase;
    Uint    m_FlagDPWMInverse;

    Uint    SinAlfa;
    Uint    SinAlfa60;
    Uint    m_iT1Length;
    Uint    m_iT2Length;
    Uint    m_iZeroLength;
    Uint    m_TotalLength;
    Uint    m_UVWSel1;
    Uint    m_UVWSel2;
    Uint    phase;

    phase = gPhase.OutPhase;
    if (phase > 65531)
    {
        phase = 65531;
    }

    m_iSec30 = phase/5461;
    if (m_iSec30 > 11)
    {
        m_iSec30 = 0;
    }
    m_iSec60 = m_iSec30 >> 1;

    m_iRamainPhase = phase - (m_iSec30 * 5461);
    if ((m_iSec30 & 0x01) == 1)
    {
        if (gPWM.PWMModle == MODLE_CPWM)
        {
            m_iRamainPhase = 5461 + m_iRamainPhase;
        }
        else
        {
            m_iRamainPhase = 5461 - m_iRamainPhase;
        }
    }

    m_FlagDPWMInverse = 0;
    if (gPWM.PWMModle == MODLE_DPWM)
    {
        if ((((m_iSec30+1)>>1) & 0x01) == 1)
        {
            m_FlagDPWMInverse = 1;
        }
    }
    else
    {
        if ((m_iSec60 & 0x01) == 1)
        {
            m_iRamainPhase = 10922 - m_iRamainPhase;
        }
    }

    SinAlfa60 = qsin(10922 - m_iRamainPhase);
    SinAlfa = qsin(m_iRamainPhase);

    m_iT1Length = ((long)SinAlfa60 * (long)gRatio)>>15;
    m_iT1Length = ((long)m_iT1Length * (long)gPWM.gPWMPrdApply)>>12;

    m_iT2Length = ((long)SinAlfa * (long)gRatio)>>15;
    m_iT2Length = ((long)m_iT2Length * (long)gPWM.gPWMPrdApply)>>12;

    m_TotalLength = m_iT1Length + m_iT2Length;
    if (m_TotalLength > gPWM.gPWMPrdApply)
    {
        m_iT1Length = ((Ulong)m_iT1Length * (Ulong)gPWM.gPWMPrdApply)/m_TotalLength;
        m_iT2Length = gPWM.gPWMPrdApply - m_iT1Length;
        m_iZeroLength = 0;
    }
    gPWM.gZeroLengthPhase = ZERO_VECTOR_NONE;
    if (gPWM.PWMModle == MODLE_CPWM)
    {
        m_iZeroLength = (Uint)((gPWM.gPWMPrdApply - m_iT1Length - m_iT2Length)>>1);
        m_UVWSel1 = iTrig1_Cpwm[m_iSec30];
        m_UVWSel2 = iTrig3_Cpwm[m_iSec30];
    }
    else
    {
        m_iZeroLength = 0;
        m_UVWSel1 = iTrig1_Dpwm[m_iSec30];
        gPWM.gZeroLengthPhase = (ZERO_LENGTH_PHASE_SELECT_ENUM)m_UVWSel1;
        m_UVWSel2 = iTrig3_Dpwm[m_iSec30];
    }

    m_iT1Length = m_iZeroLength + m_iT1Length;
    m_iT2Length = m_iT1Length + m_iT2Length;

    if (m_FlagDPWMInverse == 1)
    {
        m_iZeroLength = gPWM.gPWMPrdApply - m_iZeroLength;
        m_iT1Length   = gPWM.gPWMPrdApply - m_iT1Length;
        m_iT2Length   = gPWM.gPWMPrdApply - m_iT2Length;
    }

    SetPhaseLength(m_UVWSel1, m_iZeroLength);
    SetPhaseLength(iTrig2_Pwm[m_iSec30], m_iT1Length);
    SetPhaseLength(m_UVWSel2, m_iT2Length);

    DeadBandComp();
    SendPWM();
}

static void DeadBandCompOne(long *pValue, int comp,
                            ZERO_LENGTH_PHASE_SELECT_ENUM skipSel)
{
    if (gRatio != 0)
    {
        if (skipSel != gPWM.gZeroLengthPhase)
        {
            *pValue += (long)comp;
        }
    }

    *pValue = __IQsat(*pValue, gPWM.gPWMPrdApply, 0);
}

void DeadBandComp(void)
{
    long m_Max,m_Min,m_Deta;

    DeadBandCompOne(&gPWM.U, gDeadBand.CompU, ZERO_VECTOR_U);
    DeadBandCompOne(&gPWM.V, gDeadBand.CompV, ZERO_VECTOR_V);
    DeadBandCompOne(&gPWM.W, gDeadBand.CompW, ZERO_VECTOR_W);

#if 0
    if(gSubCommand.bit.CancelNarrorP == 0)
    {
        gPWM.U = __IQsat(gPWM.U, gPWM.gPWMPrdApply, 0);
        gPWM.V = __IQsat(gPWM.V, gPWM.gPWMPrdApply, 0);
        gPWM.W = __IQsat(gPWM.W, gPWM.gPWMPrdApply, 0);
    }
    else
    {
        m_Min = (long)gDeadBand.DeadBand + 1;
        m_Max = (long)(gPWM.gPWMPrdApply - m_Min);

        m_Deta = gPWM.U - m_Max;
        if(gPWM.U < (m_Min>>1))
        {
            gPWM.U = 0;
        }
        else if(gPWM.U < m_Min)
        {
            gPWM.U = m_Min;
        }
        else if(m_Deta < 0)
        {
            ;
        }
        else if(m_Deta < (m_Min>>1))
        {
            gPWM.U = m_Max;
        }
        else
        {
            gPWM.U = gPWM.gPWMPrdApply;
        }

        m_Deta = gPWM.V - m_Max;
        if(gPWM.V < (m_Min>>1))
        {
            gPWM.V = 0;
        }
        else if(gPWM.V < m_Min)
        {
            gPWM.V = m_Min;
        }
        else if(m_Deta < 0)
        {
            ;
        }
        else if(m_Deta < (m_Min>>1))
        {
            gPWM.V = m_Max;
        }
        else
        {
            gPWM.V = gPWM.gPWMPrdApply;
        }

        m_Deta = gPWM.W - m_Max;
        if(gPWM.W < (m_Min>>1))
        {
            gPWM.W = 0;
        }
        else if(gPWM.W < m_Min)
        {
            gPWM.W = m_Min;
        }
        else if(m_Deta < 0)
        {
            ;
        }
        else if(m_Deta < (m_Min>>1))
        {
            gPWM.W = m_Max;
        }
        else
        {
            gPWM.W = gPWM.gPWMPrdApply;
        }
    }
#endif

}

static void SendOneChannel(volatile struct EPWM_REGS *pReg, long value,
                           Uint *pFlag)
{
    if (*pFlag != 0)
    {
        *pFlag = 0;

        pReg->AQCTLA.all = 0x90;

        pReg->CMPCTL.all = 0;
    }

    if (value == 0)
    {
        if (pReg->CMPA.half.CMPA != 0)
        {
            pReg->AQCTLA.all = 0x91;
            *pFlag = 1;
        }
    }
    else
    {
        if (pReg->CMPA.half.CMPA == 0)
        {
            pReg->AQCTLA.all = 0x12;
            pReg->CMPCTL.all = 2;
            *pFlag = 1;
        }
    }
}

void SendPWM(void)
{
    EALLOW;

    SendOneChannel(&EPwm1Regs, gPWM.U, &EPWMFlagU);
    SendOneChannel(&EPwm2Regs, gPWM.V, &EPWMFlagV);
    SendOneChannel(&EPwm3Regs, gPWM.W, &EPWMFlagW);

    EPwm1Regs.TBPRD = gPWM.gPWMPrdApply;
    EPwm1Regs.CMPB  = EPwm1Regs.TBPRD - gADC.DelayApply;
    EPwm2Regs.TBPRD = gPWM.gPWMPrdApply;
    EPwm3Regs.TBPRD = gPWM.gPWMPrdApply;

    EPwm1Regs.CMPA.half.CMPA = gPWM.U;
    EPwm2Regs.CMPA.half.CMPA = gPWM.V;
    EPwm3Regs.CMPA.half.CMPA = gPWM.W;
    EDIS;
}
