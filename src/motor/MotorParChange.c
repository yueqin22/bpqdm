#include "MotorInclude.h"

#define INV_BAND_CNT        6
#define FREQ_COEFF_CNT      3
#define OV_LIMIT_CNT        5
#define DEAD_IDX_MAX       24
#define DEAD_IDX_MIN       12
#define DEAD_IDX_1140      13
#define CUR_COFF_MAX    32767
#define POS_STEP        18641L
#define POS_SH             10
#define DEG_STEP          180L
#define DEG_SH             15
#define RES_STEP        18597L
#define DEAD_SET_1140     500
#define DEAD_SET_400      400
#define COFF_LIMIT_220     19
#define COFF_LIMIT_OTH     21
#define COFF_UNIT         10
#define COFF_UNIT_1        1
#define TYPE_22            22
#define UDC_CAL_WIN       200
#define UDC_TO_VOLT      3251L
#define UDC_TO_VOLT_SH     13
#define VOLT_TO_OUT       710L
#define UDIV_V_GET       4160L
#define AMP_DC_STEP      5792L
#define CUR_SAMP_28      12745L
#define CUR_SAMP_28_8    11586L
#define UDC_COFF_28         36
#define UDC_COFF_28_8       33
#define CUR_SAMP_SH        15
#define FREQ_REAL_HALF   (1 << 14)
#define FREQ_REAL_SH       15

typedef struct
{
    Uint        m_Limit;
    Uint        m_Base;
    Uint        m_VoltType;
    Uint        m_GpForce;
    const Uint *m_pVolt;
    const Uint *m_pCur;
} INV_BAND;

typedef struct
{
    Uint m_Set;
    Uint m_Limit;
} OV_BAND;

static const INV_BAND s_InvBand[INV_BAND_CNT] =
{
    { 100,   0, INV_VOLTAGE_380V,  0, gInvVoltageInfo380T,  gInvCurrentTable380T  },
    { 200, 100, INV_VOLTAGE_220V,  1, gInvVoltageInfo220T,  gInvCurrentTable220T  },
    { 300, 200, INV_VOLTAGE_220V,  0, gInvVoltageInfo220S,  gInvCurrentTable220S  },
    { 400, 300, INV_VOLTAGE_480V,  0, gInvVoltageInfo480T,  gInvCurrentTable380T  },
    { 500, 400, INV_VOLTAGE_690V,  0, gInvVoltageInfo690T,  gInvCurrentTable690T  },
    {   0, 500, INV_VOLTAGE_1140V, 0, gInvVoltageInfo1140T, gInvCurrentTable1140T }
};

static const Uint s_FreqCoeff[FREQ_COEFF_CNT][2] =
{
    {   1, 100 },
    {  10,  10 },
    { 100,   1 }
};

static const OV_BAND s_OvBand[OV_LIMIT_CNT] =
{
    { 100,  7500 },
    { 300,  3700 },
    { 400,  8300 },
    { 500, 13000 },
    {   0, 19000 }
};

static Ulong s_FreqToReal(Uint m_Val)
{
    return (Ulong)((((Ullong)m_Val * gBasePar.FullFreq01)
                    + FREQ_REAL_HALF) >> FREQ_REAL_SH);
}

static Ulong s_PosScale(Uint m_Val)
{
    return (Ulong)(((long)m_Val * POS_STEP) >> POS_SH);
}

static int s_DegScale(int m_Val)
{
    return (int)(((long)m_Val * DEG_STEP) >> DEG_SH);
}

static Uint s_ResScale(Uint m_Res, Uint m_Cur, Uint m_Volt)
{
    Uint m_Tmp;

    m_Tmp = (Uint)(((Ulong)m_Res * (Ulong)m_Cur) / m_Volt);
    return (Uint)(((Ulong)m_Tmp * RES_STEP) >> 14);
}

static Ulong s_IndScale(Uint m_L, Uint m_BaseL)
{
    Ulong m_Tmp;

    m_Tmp = (((Ulong)m_L << 11) + m_BaseL) >> 1;
    return m_Tmp / m_BaseL;
}

void SystemParChg2Ms()
{
    Ulong m_ULong;
    Uint  minType, maxType;
    const Uint *pInvVolt, *pInvCur;
    Uint  m_UData;
    Uint  m_Band;
    Uint  m_DeadIdx;
    Uint  m_CoffLimit;

    m_Band = 0;
    while ((m_Band < (INV_BAND_CNT - 1))
           && (gInvInfo.InvTypeSet >= s_InvBand[m_Band].m_Limit))
    {
        m_Band++;
    }

    pInvVolt = s_InvBand[m_Band].m_pVolt;
    pInvCur  = s_InvBand[m_Band].m_pCur;

    gInvInfo.InvType = (Uint)(gInvInfo.InvTypeSet - s_InvBand[m_Band].m_Base);
    gInvInfo.InvVoltageType = s_InvBand[m_Band].m_VoltType;
    gInvInfo.GPType = s_InvBand[m_Band].m_GpForce ? 1 : gInvInfo.GpTypeSet;

    gInvInfo.InvVolt       = pInvVolt[0];
    gInvInfo.InvLowUdcStad = pInvVolt[1];
    gInvInfo.InvUpUDC      = pInvVolt[2];
    gInvInfo.BaseUdc       = pInvVolt[3];
    minType                = pInvVolt[4];
    maxType                = pInvVolt[5];
    gInLose.ForeInvType    = pInvVolt[6];
    gUDC.uDCADCoff         = pInvVolt[7];
    gVFPar.ovPointCoff     = pInvVolt[8];

    if (gInvInfo.InvType > maxType)
    {
        gInvInfo.InvType = maxType;
    }
    if (gInvInfo.InvType <= minType)
    {
        gInvInfo.GPType  = 1;
        gInvInfo.InvType = minType;
    }

    if (gInvInfo.GPType == 1)
    {
        gInvInfo.InvTypeApply = gInvInfo.InvType;
    }
    else
    {
        gInvInfo.InvTypeApply = (Uint)(gInvInfo.InvType - 1);
    }

    gInvInfo.InvCurrent       = pInvCur[gInvInfo.InvTypeApply - minType];
    gInvInfo.InvCurrentOvload = pInvCur[gInvInfo.InvType - minType];

    m_CoffLimit = (gInvInfo.InvVoltageType == INV_VOLTAGE_220V)
                  ? COFF_LIMIT_220 : COFF_LIMIT_OTH;
    gMotorExtInfo.UnitCoff = (gInvInfo.InvType > m_CoffLimit)
                             ? COFF_UNIT : COFF_UNIT_1;

    if ((1 != gInvInfo.GPType) && (TYPE_22 == gInvInfo.InvType))
    {
        gInvInfo.InvCurrent = (Ulong)gInvInfo.InvCurrent * 3264 >> 15;
    }
    gInvInfo.InvCurrForP = pInvCur[gInvInfo.InvType - minType];

    m_UData = gInvInfo.InvTypeApply;
    if (m_UData > DEAD_IDX_MAX)
    {
        m_UData = DEAD_IDX_MAX;
    }
    if (m_UData < DEAD_IDX_MIN)
    {
        m_UData = DEAD_IDX_MIN;
    }
    m_DeadIdx = (Uint)(m_UData - DEAD_IDX_MIN);

    if (gInvInfo.InvTypeSet >= DEAD_SET_1140)
    {
        gDeadBand.DeadBand = DBTIME_1140V * gDeadBand.DeadTimeSet / 10;
        gDeadBand.Comp     = DCTIME_1140V * gDeadBand.DeadTimeSet / 10;
    }
    else
    {
        if (gInvInfo.InvTypeSet >= DEAD_SET_400)
        {
            m_DeadIdx = DEAD_IDX_1140;
        }
        gDeadBand.DeadBand = gDeadBandTable[m_DeadIdx];
        gDeadBand.Comp     = gDeadCompTable[m_DeadIdx];
    }

    EALLOW;
    EPwm1Regs.DBFED = gDeadBand.DeadBand;
    EPwm1Regs.DBRED = gDeadBand.DeadBand;
    EPwm2Regs.DBFED = gDeadBand.DeadBand;
    EPwm2Regs.DBRED = gDeadBand.DeadBand;
    EPwm3Regs.DBFED = gDeadBand.DeadBand;
    EPwm3Regs.DBRED = gDeadBand.DeadBand;
    EDIS;

#ifdef DSP_CLOCK100
    gADC.DelayApply = gADC.DelaySet * 10;
#else
    gADC.DelayApply = gADC.DelaySet * 6;
#endif

    m_UData = gInvInfo.InvCurrent >> 2;
    DINT;
    if (gMotorInfo.CurrentGet < m_UData)
    {
        gMotorInfo.Current = m_UData;
    }
    else
    {
        gMotorInfo.Current = gMotorInfo.CurrentGet;
    }
    m_ULong = (((Ulong)gMotorInfo.Current) << 8) / gMotorInfo.CurrentGet;
    gMotorInfo.CurBaseCoff = (m_ULong > CUR_COFF_MAX) ? CUR_COFF_MAX : m_ULong;
    EINT;

    m_UData = gExtendCmd.bit.FreqUint;
    if (m_UData >= FREQ_COEFF_CNT)
    {
        m_UData = FREQ_COEFF_CNT - 1;
    }
    gMainCmd.si2puCoeff = s_FreqCoeff[m_UData][0];
    gMainCmd.pu2siCoeff = s_FreqCoeff[m_UData][1];

    gBasePar.FullFreq01 = (Ulong)gBasePar.MaxFreq
                          * (Ulong)gMainCmd.pu2siCoeff + 2000;
    gBasePar.FullFreq   = gBasePar.MaxFreq + 20 * gMainCmd.si2puCoeff;
    gMotorInfo.FreqPer  = ((Ulong)gMotorInfo.Frequency << 15) / gBasePar.FullFreq;

    gMotorInfo.Motor_HFreq = ((Ulong)gMotorInfo.Frequency * 410) >> 10;
    gMotorInfo.Motor_LFreq = ((Ulong)gMotorInfo.Frequency * 205) >> 10;

    gRotorTrans.PolesRatio = ((Ulong)gMotorExtInfo.Poles << 8) / gRotorTrans.Poles;
}

void RunStateParChg2Ms()
{
    Uint  m_UData, tempU;
    Ulong m_Long;
    Uint  m_AbsFreq;
    Ulong tmpAmp;
    Uint  m_Band;

    gIUVWQ12.U = (int)(gIUVWQ24.U >> 12);
    gIUVWQ12.V = (int)(gIUVWQ24.V >> 12);
    gIUVWQ12.W = (int)(gIUVWQ24.W >> 12);

    gIMTSetQ12.M = (int)(gIMTSetApply.M >> 12);
    gIMTSetQ12.T = (int)(gIMTSetApply.T >> 12);

    m_AbsFreq = abs(gMainCmd.FreqSyn);
    tempU = (Uint)(((Ulong)gMotorExtPer.I0 * gMotorInfo.FreqPer) / m_AbsFreq);
    if (m_AbsFreq < gMotorInfo.FreqPer)
    {
        gMotorExtPer.IoVsFreq = gMotorExtPer.I0;
    }
    else
    {
        gMotorExtPer.IoVsFreq = tempU;
    }

    gIAmpTheta.CurTmpM = abs(gIMTQ12.M);
    gIAmpTheta.CurTmpT = abs(gIMTQ12.T);
    tmpAmp  = (Ulong)((long)gIAmpTheta.CurTmpM * gIAmpTheta.CurTmpM);
    tmpAmp += (Ulong)((long)gIAmpTheta.CurTmpT * gIAmpTheta.CurTmpT);
    gIAmpTheta.Amp = (Uint)qsqrt(tmpAmp);

    if ((gMainCmd.Command.bit.StartDC == 1)
        || (gMainCmd.Command.bit.StopDC == 1))
    {
        gIAmpTheta.Amp = ((Ulong)gIAmpTheta.Amp * AMP_DC_STEP) >> 12;
    }

    gLineCur.CurPer = Filter2(gIAmpTheta.Amp, gLineCur.CurPer);
    gLineCur.CurPerFilter += gLineCur.CurPer - (gLineCur.CurPerFilter >> 7);

    m_Long = (Ulong)gLineCur.CurPer * gMotorInfo.Current;
    gLineCur.CurBaseInv = m_Long / gInvInfo.InvCurrentOvload;

    gDeadBand.InvCurFilter = Filter2(gLineCur.CurBaseInv, gDeadBand.InvCurFilter);

    m_UData = abs(gMainCmd.FreqSyn);
    gMainCmd.FreqReal = (long)s_FreqToReal(m_UData);
    gMainCmd.FreqRealFilt += (gMainCmd.FreqReal >> 4)
                             - (gMainCmd.FreqRealFilt >> 4);
    m_UData = abs(gMainCmd.FreqDesired);
    gMainCmd.FreqDesiredReal = (long)s_FreqToReal(m_UData);
    m_UData = abs(gMainCmd.FreqSet);
    gMainCmd.FreqSetReal = (long)s_FreqToReal(m_UData);
    gMainCmd.FreqSetBak = gMainCmd.FreqSet;

    gInvInfo.InvLowUDC = (long)gInvInfo.InvLowUdcStad
                         * gInvInfo.LowUdcCoff / 1000;

    if (gMainStatus.RunStep == STATUS_GET_PAR)
    {
        gUVCoff.UDivV = 4096;
    }
    else
    {
        gUVCoff.UDivV = (Ulong)gUVCoff.UDivVGet * UDIV_V_GET >> 10;
    }

    if ((abs(gUDC.uDC - gUDC.uDcCalMax) > UDC_CAL_WIN)
        || (gMainStatus.RunStep == STATUS_STOP))
    {
        gUDC.uDcCalMax = gUDC.uDC;
        m_UData = (Uint)(((long)gUDC.uDC * UDC_TO_VOLT) >> UDC_TO_VOLT_SH);
        gOutVolt.MaxOutVolt = ((long)m_UData * VOLT_TO_OUT) / gMotorInfo.Votage;
    }

    gVarAvr.CoffApply = gVFPar.VFOverExc;

    if (MOTOR_TYPE_PM == gMotorInfo.MotorType)
    {
        gIPMPos.RotorZero = (Uint)s_PosScale(gIPMPos.RotorZeroGet);
        gUVWPG.UvwZeroPos = (Uint)s_PosScale(gUVWPG.UvwZeroPos_deg);

        gUVWPG.UvwZIntErr_deg       = s_DegScale(gUVWPG.UvwZIntErr);
        gIPMPosCheck.UvwStopErr_deg = s_DegScale(gIPMPosCheck.UvwStopErr);
        gIPMPos.AbzErrPos_deg       = s_DegScale(gIPMPos.AbzErrPos);

        if ((gPGData.PGMode == 0)
            && (gMainStatus.ParaCalTimes == 1)
            && (gMainStatus.RunStep != STATUS_GET_PAR)
            && (gIPMPos.ZeroPosLast != gIPMPos.RotorZero))
        {
            tempU = (Uint)(gIPMPos.RotorZero - gIPMPos.ZeroPosLast);

            SetIPMPos((Uint)(gIPMPos.RotorPos + tempU));
            SetIPMPos_ABZRef((Uint)(gIPMPos.RotorPos + tempU));
        }
        gIPMPos.ZeroPosLast = gIPMPos.RotorZero;
        gPmDecoup.EnableDcp = 0;
    }

    if (gMainStatus.RunStep != STATUS_GET_PAR)
    {
        EQepRegs->QDECCTL.bit.SWAP = gPGData.SpeedDir;
    }

#ifdef TMS320F2808
    m_Long = ((long)gInvInfo.InvCurrent * CUR_SAMP_28_8) / gMotorInfo.Current;
#else
    m_Long = ((long)gInvInfo.InvCurrent * CUR_SAMP_28) / gMotorInfo.Current;
#endif

    gCurSamp.Coff = (m_Long * (Ulong)gInvInfo.CurrentCoff) * 33 >> CUR_SAMP_SH;

#ifdef TMS320F2808
    gUDC.Coff = (Ulong)gUDC.uDCADCoff * gInvInfo.UDCCoff * UDC_COFF_28_8
                >> CUR_SAMP_SH;
#else
    gUDC.Coff = (Ulong)gUDC.uDCADCoff * gInvInfo.UDCCoff * UDC_COFF_28
                >> CUR_SAMP_SH;
#endif

    gOvUdc.Limit = gVFPar.ovPoint * gVFPar.ovPointCoff;

    m_Band = 0;
    while ((m_Band < (OV_LIMIT_CNT - 1))
           && (gInvInfo.InvTypeSet >= s_OvBand[m_Band].m_Set))
    {
        m_Band++;
    }
    if (gOvUdc.Limit > (int)s_OvBand[m_Band].m_Limit)
    {
        gOvUdc.Limit = (int)s_OvBand[m_Band].m_Limit;
    }
}

void SystemParChg05Ms()
{
    Uint m_Qeint;

    if (gMotorInfo.LastMotorType != gMotorInfo.MotorType)
    {
        gMotorInfo.LastMotorType = gMotorInfo.MotorType;

        gPGData.PGType = PG_TYPE_NULL;
    }

    if (gPGData.PGType != (PG_TYPE_ENUM_STRUCT)gPGData.PGTypeGetFromFun)
    {
        gPGData.PGType = (PG_TYPE_ENUM_STRUCT)gPGData.PGTypeGetFromFun;
        ReInitForPG();

        gIPMInitPos.Flag = 0;
    }

    if (gPGData.QEPIndex != (QEP_INDEX_ENUM_STRUCT)gExtendCmd.bit.QepIndex)
    {
        gPGData.QEPIndex = (QEP_INDEX_ENUM_STRUCT)gExtendCmd.bit.QepIndex;

        if (gPGData.QEPIndex == QEP_SELECT_1)
        {
            EQepRegs = (struct EQEP_REGS *)&EQep1Regs;
            EALLOW;
            PieVectTable.EQEP1_INT = &PG_Zero_isr;
            PieCtrlRegs.PIEIER5.bit.INTx1 = 1;
            SysCtrlRegs.PCLKCR1.bit.EQEP1ENCLK = 1;
            EDIS;
        }
#ifdef TMS320F2808
        if (gPGData.QEPIndex == QEP_SELECT_2)
        {
            EQepRegs = (struct EQEP_REGS *)&EQep2Regs;
            EALLOW;
            PieVectTable.EQEP2_INT = &PG_Zero_isr;
            PieCtrlRegs.PIEIER5.bit.INTx2 = 1;
            SysCtrlRegs.PCLKCR1.bit.EQEP2ENCLK = 1;
            EDIS;
        }
#endif
        InitSetQEP();
    }

    m_Qeint = 0x0;
    if ((MOTOR_TYPE_PM == gMotorInfo.MotorType) && (gPGData.PGMode == 0))
    {
        m_Qeint = 0x0400;
    }

    EALLOW;
    (*EQepRegs).QEINT.all = m_Qeint;
    EDIS;
}

void ChangeMotorPar(void)
{
    Uint  m_BaseL;
    Ulong m_Ulong;

    m_BaseL = (Uint)(((Ulong)gMotorInfo.Votage * 3678) / gMotorInfo.Current);
    m_BaseL = (Uint)(((Ulong)m_BaseL * 5000) / gBasePar.FullFreq01);

    if (MOTOR_TYPE_PM != gMotorInfo.MotorType)
    {
        gMotorExtPer.R1 = s_ResScale(gMotorExtInfo.R1,
                                     gMotorInfo.Current, gMotorInfo.Votage);
        gMotorExtPer.R2 = s_ResScale(gMotorExtInfo.R2,
                                     gMotorInfo.Current, gMotorInfo.Votage);

        gMotorExtInfo.L1 = (Uint)(gMotorExtInfo.LM
                                  + ((Ulong)gMotorExtInfo.L0 * 102 >> 10));
        gMotorExtInfo.L2 = gMotorExtInfo.L1;

        gMotorExtPer.L1 = (Uint)s_IndScale(gMotorExtInfo.L1, m_BaseL);
        gMotorExtPer.L2 = gMotorExtPer.L1;
        gMotorExtPer.LM = (Uint)s_IndScale(gMotorExtInfo.LM, m_BaseL);
        gMotorExtPer.L0 = (Uint)((gMotorExtPer.L1 - gMotorExtPer.LM) << 5);

        gMotorExtPer.I0 = (Uint)((((Ulong)gMotorExtInfo.I0) << 12)
                                 / gMotorInfo.Current);

        m_Ulong = 4096L * 4096L - (long)gMotorExtPer.I0 * gMotorExtPer.I0;
        gMotorExtPer.ItRated = (Uint)qsqrt(m_Ulong);
        gPowerTrq.rpItRated = (Uint)((1000L << 12) / gMotorExtPer.ItRated);
    }
    else
    {
        gMotorExtPer.Rpm = s_ResScale(gMotorExtInfo.RsPm,
                                      gMotorInfo.Current, gMotorInfo.Votage);

        gMotorExtPer.LD = (Uint)(s_IndScale(gMotorExtInfo.LD, m_BaseL) / 10);
        gMotorExtPer.LQ = (Uint)(s_IndScale(gMotorExtInfo.LQ, m_BaseL) / 10);

        m_Ulong = ((long)gMotorExtInfo.BemfVolt << 12) / (gMotorInfo.Votage * 10);
        gMotorExtPer.FluxRotor = (Uint)((m_Ulong << 15) / gMotorInfo.FreqPer);

        gPowerTrq.rpItRated = 1000;
    }

    m_Ulong = (((Ullong)gMotorInfo.Frequency * (Ullong)gMainCmd.pu2siCoeff
                * 19661L) >> 15);
    gMotorExtInfo.Poles = (Uint)((m_Ulong + (gMotorExtInfo.Rpm >> 1))
                                 / gMotorExtInfo.Rpm);

    m_Ulong = gMotorExtInfo.Rpm * gMotorExtInfo.Poles * 6830L >> 12;
    gMotorExtInfo.RatedComp = (Uint)((Ulong)gMotorInfo.Frequency
                                     * gMainCmd.pu2siCoeff - m_Ulong);

    gMotorExtPer.RatedComp = (Uint)(((long)gMotorExtInfo.RatedComp << 15)
                                    / gBasePar.FullFreq01);
}

void InvCalcPower(void)
{
    long m_PowerN;
    Uint m_Run;

    m_Run = gMainStatus.StatusWord.bit.StartStop;

    gPowerTrq.anglePF = Filter16(abs(gIAmpTheta.PowerAngle), gPowerTrq.anglePF);
    gPowerTrq.anglePF = m_Run ? gPowerTrq.anglePF : 0;
    gPowerTrq.Cur_Ft4 = Filter16(gLineCur.CurPer, gPowerTrq.Cur_Ft4);

    gPowerTrq.InvPowerPU = (1732L * (long)gOutVolt.VoltApply / 1000L)
                           * gPowerTrq.Cur_Ft4 >> 12;
    gPowerTrq.InvPowerPU = (long)gPowerTrq.InvPowerPU
                           * qsin(16384 - gPowerTrq.anglePF) >> 15;

    m_PowerN = ((long)gMotorInfo.Current * gMotorInfo.Votage) >> 10;
    m_PowerN = (gInvInfo.InvTypeApply >= TYPE_22) ? m_PowerN
                                                  : (m_PowerN * 409L >> 12);
    gPowerTrq.InvPower_si = ((long)gPowerTrq.InvPowerPU * m_PowerN) >> 12;
    gPowerTrq.InvPower_si = m_Run ? gPowerTrq.InvPower_si : 0;

    gPowerTrq.TrqOut_pu = (long)gLineCur.CurTorque * gPowerTrq.rpItRated >> 13;
    gPowerTrq.TrqOut_pu = m_Run ? gPowerTrq.TrqOut_pu : 0;
}
