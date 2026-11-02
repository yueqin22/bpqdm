
#include "MotorInclude.h"

void SystemParChg2Ms()
{
    long	m_Long;
	Ulong	m_ULong;
    Uint    minType, maxType;
    Uint    *pInvVolt, *pInvCur;
    Uint 	m_UData;

	if(100 > gInvInfo.InvTypeSet)
	{
        pInvVolt  = (Uint *)gInvVoltageInfo380T;
        pInvCur = (Uint *)gInvCurrentTable380T;
        gInvInfo.InvType  = gInvInfo.InvTypeSet;
        gInvInfo.InvVoltageType = INV_VOLTAGE_380V;
        gInvInfo.GPType = gInvInfo.GpTypeSet;
	}
	else if(200 > gInvInfo.InvTypeSet)
	{
	    pInvVolt  = (Uint *)gInvVoltageInfo220T;

        pInvCur   = (Uint *)gInvCurrentTable220T;
        gInvInfo.InvType  = gInvInfo.InvTypeSet - 100;

        gInvInfo.InvVoltageType = INV_VOLTAGE_220V;
        gInvInfo.GPType   = 1;
	}
    else if(300 > gInvInfo.InvTypeSet)
    {
        pInvVolt  = (Uint *)gInvVoltageInfo220S;
        pInvCur = (Uint *)gInvCurrentTable220S;
        gInvInfo.InvType  = gInvInfo.InvTypeSet - 200;
        gInvInfo.InvVoltageType = INV_VOLTAGE_220V;
        gInvInfo.GPType = gInvInfo.GpTypeSet;
    }
    else if(400 > gInvInfo.InvTypeSet)
    {
        pInvVolt  = (Uint *)gInvVoltageInfo480T;
        pInvCur = (Uint *)gInvCurrentTable380T;
        gInvInfo.InvType  = gInvInfo.InvTypeSet - 300;
        gInvInfo.InvVoltageType = INV_VOLTAGE_480V;
        gInvInfo.GPType = gInvInfo.GpTypeSet;
    }
    else if(500 > gInvInfo.InvTypeSet)
    {
        pInvVolt  = (Uint *)gInvVoltageInfo690T;
        pInvCur = (Uint *)gInvCurrentTable690T;
        gInvInfo.InvType  = gInvInfo.InvTypeSet - 400;
        gInvInfo.InvVoltageType = INV_VOLTAGE_690V;
        gInvInfo.GPType = gInvInfo.GpTypeSet;
    }
    else
    {
        pInvVolt  = (Uint *)gInvVoltageInfo1140T;
        pInvCur = (Uint *)gInvCurrentTable1140T;
        gInvInfo.InvType  = gInvInfo.InvTypeSet - 500;
        gInvInfo.InvVoltageType = INV_VOLTAGE_1140V;
        gInvInfo.GPType = gInvInfo.GpTypeSet;
    }

	gInvInfo.InvVolt     = *(pInvVolt + 0);
	gInvInfo.InvLowUdcStad = *(pInvVolt + 1);
	gInvInfo.InvUpUDC    = *(pInvVolt + 2);
	gInvInfo.BaseUdc     = *(pInvVolt + 3);
    minType              = *(pInvVolt + 4);
    maxType              = *(pInvVolt + 5);
    gInLose.ForeInvType  = *(pInvVolt + 6);
    gUDC.uDCADCoff       = *(pInvVolt + 7);
    gVFPar.ovPointCoff   = *(pInvVolt + 8);

	gInvInfo.InvType = (gInvInfo.InvType > maxType) ? maxType : gInvInfo.InvType;
	if(gInvInfo.InvType <= minType)
    {
        gInvInfo.GPType = 1;
        gInvInfo.InvType = minType;
	}

    if(gInvInfo.GPType == 1)    gInvInfo.InvTypeApply = gInvInfo.InvType;
    else                        gInvInfo.InvTypeApply = gInvInfo.InvType - 1;

	gInvInfo.InvCurrent  = *(pInvCur + gInvInfo.InvTypeApply - minType);
    gInvInfo.InvCurrentOvload = *(pInvCur + gInvInfo.InvType - minType);
    if(gInvInfo.InvVoltageType == INV_VOLTAGE_220V)
    {
        gMotorExtInfo.UnitCoff = (gInvInfo.InvType > 19) ? 10 : 1;
    }
    else
    {
        gMotorExtInfo.UnitCoff = (gInvInfo.InvType > 21) ? 10 : 1;
    }

	if((1 != gInvInfo.GPType) && (22 == gInvInfo.InvType))
	{

        gInvInfo.InvCurrent = (Ulong)gInvInfo.InvCurrent * 3264 >> 15;
    }
    gInvInfo.InvCurrForP = *(pInvCur + gInvInfo.InvType - minType);

	m_UData = (gInvInfo.InvTypeApply > 24) ? 24 : gInvInfo.InvTypeApply;
	m_UData = (m_UData < 12) ? 12 : m_UData;
    if(500 <= gInvInfo.InvTypeSet)
    {
        gDeadBand.DeadBand = DBTIME_1140V*gDeadBand.DeadTimeSet/10;
        gDeadBand.Comp     = DCTIME_1140V*gDeadBand.DeadTimeSet/10;
    }
    else if(400 <= gInvInfo.InvTypeSet)
    {
	    gDeadBand.DeadBand = gDeadBandTable[13];
	    gDeadBand.Comp     = gDeadCompTable[13];
    }
    else
    {
	    gDeadBand.DeadBand = gDeadBandTable[m_UData - 12];
	    gDeadBand.Comp     = gDeadCompTable[m_UData - 12];
    }
	EALLOW;
	EPwm1Regs.DBFED = gDeadBand.DeadBand;
	EPwm1Regs.DBRED = gDeadBand.DeadBand;
	EPwm2Regs.DBFED = gDeadBand.DeadBand;
	EPwm2Regs.DBRED = gDeadBand.DeadBand;
	EPwm3Regs.DBFED = gDeadBand.DeadBand;
	EPwm3Regs.DBRED = gDeadBand.DeadBand;
	EDIS;

    #ifdef		DSP_CLOCK100
	gADC.DelayApply = gADC.DelaySet * 10;
    #else
	gADC.DelayApply = gADC.DelaySet * 6;
    #endif

	m_UData = gInvInfo.InvCurrent>>2;
	DINT;
	gMotorInfo.Current = (gMotorInfo.CurrentGet < m_UData) ? m_UData : gMotorInfo.CurrentGet;
	m_ULong = (((Ulong)gMotorInfo.Current)<<8) / gMotorInfo.CurrentGet;
	gMotorInfo.CurBaseCoff = (m_ULong > 32767) ? 32767 : m_ULong;
	EINT;

    if( 0 == gExtendCmd.bit.FreqUint )
    {
        gMainCmd.si2puCoeff = 1;
        gMainCmd.pu2siCoeff = 100;
    }
    else if(1 == gExtendCmd.bit.FreqUint)
    {
        gMainCmd.si2puCoeff = 10;
        gMainCmd.pu2siCoeff = 10;
    }
    else
    {
        gMainCmd.si2puCoeff = 100;
        gMainCmd.pu2siCoeff = 1;
    }
	gBasePar.FullFreq01 = (Ulong)gBasePar.MaxFreq * (Ulong)gMainCmd.pu2siCoeff + 2000;
	gBasePar.FullFreq =   gBasePar.MaxFreq + 20 * gMainCmd.si2puCoeff;
	gMotorInfo.FreqPer =  ((Ulong)gMotorInfo.Frequency <<15) / gBasePar.FullFreq;

    gMotorInfo.Motor_HFreq = ((Ulong)gMotorInfo.Frequency * 410) >>10;
    gMotorInfo.Motor_LFreq = ((Ulong)gMotorInfo.Frequency * 205) >>10;

    gRotorTrans.PolesRatio = ((Ulong)gMotorExtInfo.Poles << 8) / gRotorTrans.Poles;
}

void RunStateParChg2Ms()
{

    Uint    m_UData, tempU;
    Ulong   m_Long;
    Uint    m_AbsFreq;

     Ulong   tmpAmp;
	gIUVWQ12.U = (int)(gIUVWQ24.U>>12);
	gIUVWQ12.V = (int)(gIUVWQ24.V>>12);
	gIUVWQ12.W = (int)(gIUVWQ24.W>>12);

    gIMTSetQ12.M = (int)(gIMTSetApply.M >> 12);
    gIMTSetQ12.T = (int)(gIMTSetApply.T >> 12);

	m_AbsFreq = abs(gMainCmd.FreqSyn);
    tempU = ((Ulong)gMotorExtPer.I0 * gMotorInfo.FreqPer) / m_AbsFreq;
    gMotorExtPer.IoVsFreq = (m_AbsFreq < gMotorInfo.FreqPer) ? gMotorExtPer.I0 : tempU;

    gIAmpTheta.CurTmpM = abs(gIMTQ12.M);
    gIAmpTheta.CurTmpT = abs(gIMTQ12.T);
    tmpAmp = (long)gIAmpTheta.CurTmpM * gIAmpTheta.CurTmpM;
    tmpAmp += (long)gIAmpTheta.CurTmpT * gIAmpTheta.CurTmpT;
    gIAmpTheta.Amp = (Uint)qsqrt((Ulong)tmpAmp);

    if((gMainCmd.Command.bit.StartDC == 1) ||
       (gMainCmd.Command.bit.StopDC == 1))
    {
        gIAmpTheta.Amp = ((Ulong)gIAmpTheta.Amp * 5792)>>12;
    }
	gLineCur.CurPer = Filter2(gIAmpTheta.Amp, gLineCur.CurPer);
	gLineCur.CurPerFilter += gLineCur.CurPer - (gLineCur.CurPerFilter>>7);

	m_Long = (Ulong)gLineCur.CurPer * gMotorInfo.Current;
	gLineCur.CurBaseInv = m_Long/gInvInfo.InvCurrentOvload;

    gDeadBand.InvCurFilter = Filter2(gLineCur.CurBaseInv, gDeadBand.InvCurFilter);
	m_UData = abs(gMainCmd.FreqSyn);
	gMainCmd.FreqReal = ((Ullong)m_UData * gBasePar.FullFreq01 + (1<<14))>>15;
    gMainCmd.FreqRealFilt += (gMainCmd.FreqReal>>4) - (gMainCmd.FreqRealFilt>>4);
    m_UData = abs(gMainCmd.FreqDesired);
    gMainCmd.FreqDesiredReal = ((Ullong)m_UData * gBasePar.FullFreq01 + (1<<14))>>15;
    m_UData = abs(gMainCmd.FreqSet);
    gMainCmd.FreqSetReal = ((Ullong)m_UData * gBasePar.FullFreq01 + (1<<14))>>15;
    gMainCmd.FreqSetBak = gMainCmd.FreqSet;

	gInvInfo.InvLowUDC = (long)gInvInfo.InvLowUdcStad * gInvInfo.LowUdcCoff / 1000;

    if(gMainStatus.RunStep == STATUS_GET_PAR)
	{
		gUVCoff.UDivV = 4096;
	}
	else
	{

        gUVCoff.UDivV = (Ulong)gUVCoff.UDivVGet * 4160L >> 10;
	}

    if((abs(gUDC.uDC - gUDC.uDcCalMax) > 200)
       || (gMainStatus.RunStep == STATUS_STOP))
    {
        gUDC.uDcCalMax = gUDC.uDC;
        m_UData = ((long)gUDC.uDC * 3251L)>>13;
        gOutVolt.MaxOutVolt = ((long)m_UData * 710L) / gMotorInfo.Votage;
    }

    gVarAvr.CoffApply = gVFPar.VFOverExc;

	if(MOTOR_TYPE_PM == gMotorInfo.MotorType)
	{

        gIPMPos.RotorZero = (Ulong)gIPMPos.RotorZeroGet * 18641L >> 10;

        gUVWPG.UvwZeroPos = gUVWPG.UvwZeroPos_deg * 18641L >> 10;

        gUVWPG.UvwZIntErr_deg = (long)gUVWPG.UvwZIntErr * 180L >>15;
        gIPMPosCheck.UvwStopErr_deg = (long)gIPMPosCheck.UvwStopErr *180L >>15;
        gIPMPos.AbzErrPos_deg = (long)gIPMPos.AbzErrPos * 180L >>15;
        #if 0
        tempU = gUVWPG.UVWAngle + gUVWPG.UvwZeroPos;
        gUVWPG.UvwRealErr_deg = (int)(gIPMPos.RotorPos - tempU) * 180L >>15;
        #endif

        if(gPGData.PGMode == 0 && gMainStatus.ParaCalTimes == 1 &&
            gMainStatus.RunStep != STATUS_GET_PAR &&
            gIPMPos.ZeroPosLast != gIPMPos.RotorZero)
        {
            tempU = gIPMPos.RotorZero - gIPMPos.ZeroPosLast;

            SetIPMPos(gIPMPos.RotorPos + tempU);
            SetIPMPos_ABZRef(gIPMPos.RotorPos + tempU);
        }
        gIPMPos.ZeroPosLast = gIPMPos.RotorZero;
        gPmDecoup.EnableDcp = 0;
	}

    if(gMainStatus.RunStep != STATUS_GET_PAR)
    {
        EQepRegs->QDECCTL.bit.SWAP = gPGData.SpeedDir;
    }

#ifdef TMS320F2808
    m_Long = ((long)gInvInfo.InvCurrent * 11586L)/gMotorInfo.Current;
#else
	m_Long = ((long)gInvInfo.InvCurrent * 12745L)/gMotorInfo.Current;
#endif

	gCurSamp.Coff = (m_Long * (Ulong)gInvInfo.CurrentCoff) * 33 >>15;
#ifdef TMS320F2808

    gUDC.Coff = (Ulong)gUDC.uDCADCoff * gInvInfo.UDCCoff * 33 >>15;
#else
    gUDC.Coff = (Ulong)gUDC.uDCADCoff * gInvInfo.UDCCoff * 36 >>15;
#endif
    gOvUdc.Limit = gVFPar.ovPoint * gVFPar.ovPointCoff;
    if     (100 > gInvInfo.InvTypeSet)
        {
          gOvUdc.Limit = (gOvUdc.Limit>7500)?7500:gOvUdc.Limit;
        }
    else if(300 > gInvInfo.InvTypeSet)
        {
          gOvUdc.Limit = (gOvUdc.Limit>3700)?3700:gOvUdc.Limit;
        }
    else if(400 > gInvInfo.InvTypeSet)
        {
          gOvUdc.Limit = (gOvUdc.Limit>8300)?8300:gOvUdc.Limit;
        }
    else if(500 > gInvInfo.InvTypeSet)
        {
          gOvUdc.Limit = (gOvUdc.Limit>13000)?13000:gOvUdc.Limit;
        }
    else
        {
         gOvUdc.Limit = (gOvUdc.Limit>19000)?19000:gOvUdc.Limit;
        }
}

void SystemParChg05Ms()
{

    if(gMotorInfo.LastMotorType != gMotorInfo.MotorType)
    {
        gMotorInfo.LastMotorType = gMotorInfo.MotorType;

        gPGData.PGType = PG_TYPE_NULL;
    }

    if(gPGData.PGType != (PG_TYPE_ENUM_STRUCT)gPGData.PGTypeGetFromFun)
    {
        gPGData.PGType = (PG_TYPE_ENUM_STRUCT)gPGData.PGTypeGetFromFun;
        ReInitForPG();

        gIPMInitPos.Flag = 0;
    }

	if(gPGData.QEPIndex != (QEP_INDEX_ENUM_STRUCT)gExtendCmd.bit.QepIndex)
	{
        gPGData.QEPIndex = (QEP_INDEX_ENUM_STRUCT)gExtendCmd.bit.QepIndex;

        if(gPGData.QEPIndex == QEP_SELECT_1)
        {
            EQepRegs = (struct EQEP_REGS *)&EQep1Regs;
            EALLOW;
            PieVectTable.EQEP1_INT = &PG_Zero_isr;
            PieCtrlRegs.PIEIER5.bit.INTx1 = 1;
            SysCtrlRegs.PCLKCR1.bit.EQEP1ENCLK = 1;
            EDIS;
        }
        #ifdef TMS320F2808
        if(gPGData.QEPIndex == QEP_SELECT_2)
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

    if(MOTOR_TYPE_PM != gMotorInfo.MotorType ||
        gPGData.PGMode != 0)
    {
        EALLOW;
        (*EQepRegs).QEINT.all = 0x0;
        EDIS;
    }
    else
    {
        EALLOW;
        (*EQepRegs).QEINT.all = 0x0400;
        EDIS;
    }
}

void ChangeMotorPar(void)
{
	Uint m_UData,m_BaseL;
	Ulong m_Ulong;

	m_BaseL = ((Ulong)gMotorInfo.Votage * 3678)/gMotorInfo.Current;
	m_BaseL = ((Ulong)m_BaseL * 5000)/gBasePar.FullFreq01;

    if(MOTOR_TYPE_PM != gMotorInfo.MotorType)
    {

    	m_UData = ((Ulong)gMotorExtInfo.R1 * (Ulong)gMotorInfo.Current)/gMotorInfo.Votage;
    	gMotorExtPer.R1 = ((Ulong)m_UData * 18597)>>14;

    	m_UData = ((Ulong)gMotorExtInfo.R2 * (Ulong)gMotorInfo.Current)/gMotorInfo.Votage;
    	gMotorExtPer.R2 = ((Ulong)m_UData * 18597)>>14;

    	gMotorExtInfo.L1 = 	gMotorExtInfo.LM + ((Ulong)gMotorExtInfo.L0 * 102 >> 10);
    	gMotorExtInfo.L2 = gMotorExtInfo.L1;

    	m_Ulong = (((Ulong)gMotorExtInfo.L1<<11) + m_BaseL)>>1;
    	gMotorExtPer.L1 = m_Ulong/m_BaseL;
    	gMotorExtPer.L2 = gMotorExtPer.L1;
    	m_Ulong = (((Ulong)gMotorExtInfo.LM<<11) + m_BaseL)>>1;
    	gMotorExtPer.LM = m_Ulong/m_BaseL;
    	gMotorExtPer.L0 = (gMotorExtPer.L1 - gMotorExtPer.LM)<<5;

        gMotorExtPer.I0 = (((Ulong)gMotorExtInfo.I0)<<12)/gMotorInfo.Current;

        m_Ulong = 4096L * 4096L - (long)gMotorExtPer.I0 * gMotorExtPer.I0;
        gMotorExtPer.ItRated = qsqrt(m_Ulong);
        gPowerTrq.rpItRated = (1000L<<12) / gMotorExtPer.ItRated;
    }
    else
    {
        m_UData = ((Ulong)gMotorExtInfo.RsPm * (Ulong)gMotorInfo.Current)/gMotorInfo.Votage;
        gMotorExtPer.Rpm = ((Ulong)m_UData * 18597)>>14;

        m_Ulong = (((Ulong)gMotorExtInfo.LD <<11) + m_BaseL) >>1;
        gMotorExtPer.LD = m_Ulong / m_BaseL / 10;
        m_Ulong = (((Ulong)gMotorExtInfo.LQ <<11) + m_BaseL) >>1;
        gMotorExtPer.LQ = m_Ulong / m_BaseL / 10;

        m_Ulong = ((long)gMotorExtInfo.BemfVolt <<12) / (gMotorInfo.Votage *10);
        gMotorExtPer.FluxRotor = (m_Ulong << 15) / gMotorInfo.FreqPer;

        gPowerTrq.rpItRated = 1000;
    }

	m_Ulong = (((Ullong)gMotorInfo.Frequency * (Ullong)gMainCmd.pu2siCoeff * 19661L)>>15);
	gMotorExtInfo.Poles = (m_Ulong + (gMotorExtInfo.Rpm>>1)) / gMotorExtInfo.Rpm;

    m_Ulong = gMotorExtInfo.Rpm * gMotorExtInfo.Poles * 6830L >> 12;
    gMotorExtInfo.RatedComp = (Ulong)gMotorInfo.Frequency * gMainCmd.pu2siCoeff - m_Ulong;

    gMotorExtPer.RatedComp = ((long)gMotorExtInfo.RatedComp << 15)/gBasePar.FullFreq01;
}

void InvCalcPower(void)
{
    int  temp;

    long m_PowerN;

    gPowerTrq.anglePF = Filter16(abs(gIAmpTheta.PowerAngle), gPowerTrq.anglePF);
    gPowerTrq.anglePF = (gMainStatus.StatusWord.bit.StartStop) ? gPowerTrq.anglePF : 0;
    gPowerTrq.Cur_Ft4 = Filter16(gLineCur.CurPer, gPowerTrq.Cur_Ft4);

    gPowerTrq.InvPowerPU = (1732L * (long)gOutVolt.VoltApply /1000L) * gPowerTrq.Cur_Ft4 >> 12;
    gPowerTrq.InvPowerPU = (long)gPowerTrq.InvPowerPU * qsin(16384 - gPowerTrq.anglePF) >> 15;

    m_PowerN = ((long)gMotorInfo.Current * gMotorInfo.Votage) >> 10;
    m_PowerN = (gInvInfo.InvTypeApply >= 22) ? m_PowerN : (m_PowerN*409L>>12);
	gPowerTrq.InvPower_si= ((long)gPowerTrq.InvPowerPU * m_PowerN) >> 12;

    gPowerTrq.InvPower_si = (gMainStatus.StatusWord.bit.StartStop) ? gPowerTrq.InvPower_si : 0;

    gPowerTrq.TrqOut_pu = (long)gLineCur.CurTorque * gPowerTrq.rpItRated >> 13;
    gPowerTrq.TrqOut_pu = (gMainStatus.StatusWord.bit.StartStop) ? gPowerTrq.TrqOut_pu : 0;
}
