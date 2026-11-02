
#include "MotorInclude.h"
#include "MotorEncoder.h"
#include "MotorPwmInclude.h"

FC_CAL_STRUCT            gFcCal;
SYN_PWM_STRUCT           gSynPWM;
ANGLE_STRUCT             gPhase;
DEAD_BAND_STRUCT         gDeadBand;

Uint const gSynFreqTable[10] =
{
    75, 113, 169, 250, 355,
    535, 854, 1281, 1922, 65535
};

Uint const gSynNcTable[10] =
{
    80, 60, 42, 30, 20,
    12, 9, 6, 3, 1
};

Ulong const gSynStepAngleTable[10] =
{
    53687091, 71582788, 102261126, 143165576, 214748365,
    357913941, 477218588, 715827883, 1431655765, 4294967295
};

void AsynPWMAngleCal(Ulong);
void SynPWMAngleCal(void);

void CalCarrierWaveFreq(void)
{
    Uint    m_TempLim;
    Uint    m_MaxFc;
    Uint    m_MinFc;

    gFcCal.Time++;

    gFcCal.Cnt += gBasePar.FcSetApply;
    if(gFcCal.Cnt < 100)
    {
        return;
    }
    gFcCal.Cnt = 0;

    m_TempLim = 70;
    if(gInvInfo.InvTypeApply >= 19)
    {
        m_TempLim = 70;
    }

    if((gSubCommand.bit.VarFcByTem != 0) &&
       (gMainStatus.RunStep != STATUS_GET_PAR) &&
       (gTemperature.Temp > (m_TempLim-5)) &&
       (10 < gBasePar.FcSet))
    {
        if(gFcCal.Time >= 10000)
        {
            gFcCal.Time = 0;
            if(gTemperature.Temp <= m_TempLim)
            {
                gFcCal.FcBak += 5;
            }
            else if(gTemperature.Temp >= (m_TempLim+5))
            {
                gFcCal.FcBak -= 5;
                if(gFcCal.FcBak < 10)
                {
                    gFcCal.FcBak = 10;
                }
            }
        }
    }
    else
    {
        gFcCal.Time = 0;
        gFcCal.FcBak = gBasePar.FcSet;
    }

    if(gFcCal.FcBak > gBasePar.FcSet)
    {
        gFcCal.FcBak = gBasePar.FcSet;
    }

    if(gOverLoad.InvTotal.half.MSW < 16000)
    {
        gFcCal.FcLimitOvLoad = gBasePar.FcSet*10;
    }
    else if(gOverLoad.InvTotal.half.MSW > 18000)
    {
        gFcCal.FcLimitOvLoad--;
        if(gFcCal.FcLimitOvLoad < 400)
        {
            gFcCal.FcLimitOvLoad = 400;
        }
    }

    if(gFcCal.FcBak > (gFcCal.FcLimitOvLoad/10))
    {
        gFcCal.FcBak = (gFcCal.FcLimitOvLoad/10);
    }

    m_MinFc = 8;
    m_MaxFc = 120;

    if(gInvInfo.InvTypeApply > 15)
    {
        m_MaxFc = 80;
    }
    else if(gInvInfo.InvTypeApply > 12)
    {
        m_MaxFc = 100;
    }

    if(gMainCmd.Command.bit.ControlMode != IDC_VF_CTL)
    {
        if(m_MaxFc > 80)
        {
            m_MaxFc = 80;
        }
        m_MinFc = 20;
    }

    if(gMainCmd.FreqReal < 800)
    {
        if(m_MaxFc > 50)
        {
            m_MaxFc = 50;
        }
    }

    if(INV_VOLTAGE_1140V == gInvInfo.InvVoltageType)
    {
        if(m_MaxFc > 10)
        {
            m_MaxFc = 10;
        }
        m_MinFc = 8;
    }
    else if(INV_VOLTAGE_690V == gInvInfo.InvVoltageType)
    {
        if(m_MaxFc > 40)
        {
            m_MaxFc = 40;
        }
        m_MinFc = 8;
    }

    gFcCal.FcBak = __IQsat(gFcCal.FcBak,m_MaxFc,m_MinFc);

    if(gFcCal.FcBak > gBasePar.FcSetApply)
    {
        gBasePar.FcSetApply++;
    }
    else if(gFcCal.FcBak < gBasePar.FcSetApply)
    {
        gBasePar.FcSetApply--;
    }

    gSynPWM.AbsFreq = gMainCmd.FreqReal/100;

    if((IDC_VF_CTL != gMainCmd.Command.bit.ControlMode) ||
       (gExtendCmd.bit.ModulateType == 0) ||
       (gSynPWM.AbsFreq < gSynFreqTable[0]-10))
    {
        gSynPWM.ModuleApply = 0;
    }
    else
    {
        if((gSynPWM.ModuleApply == 0) && (gSynPWM.AbsFreq > gSynFreqTable[0]+10))
        {
            gSynPWM.ModuleApply = 1;
            gSynPWM.Index       = 0;
            gSynPWM.Flag        = 0;
        }
        else if((gSynPWM.ModuleApply >= 1) && (gSynPWM.AbsFreq < gSynFreqTable[0]-10))
        {
            gSynPWM.ModuleApply = 0;
        }
    }

    if(gSynPWM.ModuleApply == 0)
    {
        gSynPWM.FcApply = ((Ulong)gBasePar.FcSetApply)<<9;
    }

    if(gSynPWM.ModuleApply != 1)
    {
        AsynPWMAngleCal(gSynPWM.FcApply);
    }
    else
    {
        SynPWMAngleCal();
    }

    if((gBasePar.FcSetApply <= 15) || ( 1 == gExtendCmd.bit.ModulateType) ||(gCtrMotorType != ASYNC_VF)  )
    {
        gPWM.PWMModle = MODLE_CPWM;
    }
    else if( gMainCmd.FreqReal > gPWM.PwmModeSwitchLF + 300)
    {
        gPWM.PWMModle = MODLE_DPWM;
    }
    else if( gMainCmd.FreqReal < gPWM.PwmModeSwitchLF)
    {
        gPWM.PWMModle = MODLE_CPWM;
    }

}

void AsynPWMAngleCal(Ulong FcApply)
{
    Uint  m_HalfTc,m_AbsFreq,m_Fc;

    Ulong m_LData = (Ulong)PWM_CLOCK_HALF * 10000L * 512L;

    if(FcApply <= 61440)
    {
        m_Fc = FcApply;
    }
    else
    {
        m_Fc = FcApply>>1;
        m_LData = m_LData>>1;
    }
    m_HalfTc = (Uint)(m_LData/m_Fc);

    m_AbsFreq  = abs(gMainCmd.FreqSyn);
    m_LData = ((Ullong)m_AbsFreq * (Ullong)gBasePar.FullFreq01 * 439804651L / m_Fc)>>16;

    DINT;
    if(gMainCmd.FreqSyn >= 0)
    {
        gPhase.StepPhase = m_LData;
        gPhase.CompPhase = (m_LData>>15) + COM_PHASE_DEADTIME;
    }
    else
    {
        gPhase.StepPhase = -m_LData;
        gPhase.CompPhase = -(m_LData>>15) - COM_PHASE_DEADTIME;
    }

    gPWM.gPWMPrd = m_HalfTc;
    EINT;
}

void SynPWMAngleCal(void)
{
    Uint  m_AbsFreq,m_HalfTc,m_Index;
    Ulong m_Fc,m_Long,m_LData;

    m_Index = 0;
    while((m_Index < 10) && (gSynPWM.AbsFreq >= gSynFreqTable[m_Index]))
    {
        m_Index++;
    }
    m_Index--;

    if((m_Index < gSynPWM.Index) &&
       (gSynPWM.AbsFreq <= gSynFreqTable[gSynPWM.Index]-10))
    {
        gSynPWM.Flag  = 1;
        gSynPWM.Index = m_Index;
    }
    else if((m_Index > gSynPWM.Index) &&
            (gSynPWM.AbsFreq > gSynFreqTable[gSynPWM.Index+1]+10))
    {
        gSynPWM.Flag  = 2;
        gSynPWM.Index = m_Index;
    }

    m_AbsFreq  = abs(gMainCmd.FreqSyn);
    m_Long = ((Ullong)gBasePar.FullFreq01 * (Ullong)m_AbsFreq)>>7;
    m_Long = ((Ullong)m_Long * (Ullong)gSynNcTable[gSynPWM.Index] / 100)>>8;

    m_Fc = (((Ullong)m_Long<<9))/100;

    if(gSynPWM.Flag == 1)
    {
        gSynPWM.FcApply += 100;
        if(gSynPWM.FcApply > m_Fc)
        {
            gSynPWM.Flag = 0;
        }
    }
    else if(gSynPWM.Flag == 2)
    {
        gSynPWM.FcApply -= 100;
        if(gSynPWM.FcApply < m_Fc)
        {
            gSynPWM.Flag = 0;
        }
    }
    else
    {
        gSynPWM.FcApply = m_Fc;
    }

    if(gSynPWM.Flag != 0)
    {
        AsynPWMAngleCal(gSynPWM.FcApply);
        return;
    }

    m_LData = (Ulong)PWM_CLOCK_HALF * 1000000L;
    m_HalfTc = m_LData/m_Long;

    DINT;
    if(gMainCmd.FreqSyn >= 0)
    {
        gPhase.StepPhase = gSynStepAngleTable[gSynPWM.Index];
        gPhase.CompPhase = (gSynStepAngleTable[gSynPWM.Index]>>15) + COM_PHASE_DEADTIME;
    }
    else
    {
        gPhase.StepPhase = -gSynStepAngleTable[gSynPWM.Index];
        gPhase.CompPhase = -(gSynStepAngleTable[gSynPWM.Index]>>15) - COM_PHASE_DEADTIME;
    }

    gPWM.gPWMPrd = m_HalfTc;
    EINT;
}

void SoftPWMProcess(void)
{
    Uint  m_Coff;

    if((IDC_VF_CTL != gMainCmd.Command.bit.ControlMode) ||
        (gPWM.SoftPWMTune == 0) || (gSynPWM.ModuleApply >= 1) ||
       (gBasePar.FcSetApply < 10) || (gBasePar.FcSetApply > 60))
    {
        gPWM.gPWMPrdApply = gPWM.gPWMPrd;
        gPhase.StepPhaseApply = gPhase.StepPhase;
        return;
    }

    m_Coff = (30 < gBasePar.FcSetApply) ? 1966 : 3277;

    m_Coff = (gPWM.SoftPWMTune * m_Coff)>>4;
    gPWM.SoftPWMCoff += 29517 * GetTime();
    m_Coff = 4096 + (((long)gPWM.SoftPWMCoff * (long)m_Coff)>>15);

    gPWM.gPWMPrdApply = ((Ulong)gPWM.gPWMPrd * (Ulong)m_Coff)>>12;
    gPhase.StepPhaseApply = (Ulong)(gPhase.StepPhase>>12) * (Ulong)m_Coff;
}

void CalOutputPhase(void)
{

    if(MOTOR_TYPE_PM == gMotorInfo.MotorType)
    {
        if(0 == gPGData.PGMode)
        {
            SynCalRealPos();

            if((gMainStatus.RunStep != STATUS_GET_PAR) ||
               (gMainStatus.RunStep == STATUS_GET_PAR && TUNE_STEP_NOW != PM_EST_NO_LOAD)
               )
            {
                if(STATUS_IPM_INIT_POS != gMainStatus.RunStep)
                {
                    gIPMPos.RotorPos = gIPMZero.FeedPos;
                    gIPMPos.ABZ_RotorPos_Ref = gIPMZero.FeedABZPos;
                }
            }
        }
        else if(gPGData.PGType == PG_TYPE_RESOLVER)
        {
            if(gMainStatus.RunStep != STATUS_GET_PAR)
            {
                gIPMPos.RotorPos = gRotorTrans.RTPos + gIPMPos.RotorZero + gRotorTrans.PosComp;
            }
            else if(gMainStatus.RunStep == STATUS_GET_PAR && TUNE_STEP_NOW != PM_EST_NO_LOAD)
            {
                gIPMPos.RotorPos = gRotorTrans.RTPos + gPmParEst.EstZero;
            }
        }

        if(TUNE_STEP_NOW == PM_EST_BEMF)
        {
            gPhase.IMPhase += gPhase.StepPhaseApply;
        }
        else
        {
            gPhase.IMPhase  = ((long)gIPMPos.RotorPos) << 16;
        }
    }

    if((gMotorInfo.MotorType != MOTOR_TYPE_PM) ||
        (gCtrMotorType == SYNC_VF || gCtrMotorType == SYNC_SVC))
    {
        gPhase.IMPhase += gPhase.StepPhaseApply;
    }

    gPhase.OutPhase = (int)(gPhase.IMPhase>>16) + gOutVolt.VoltPhaseApply;
}

void CalDeadBandComp(void)
{
    int   phase,m_Com;

    if(gMainCmd.FreqReal > 40000)
    {
        m_Com = (int)(((long)gDeadBand.Comp * (long)(gMainCmd.FreqReal - 40000))>>15);
    }
    else
    {
        m_Com = gDeadBand.Comp;
    }

    if((gMotorInfo.MotorType == MOTOR_TYPE_PM) && (gDeadBand.InvCurFilter < 512))
    {
        m_Com = ((long)m_Com * gDeadBand.InvCurFilter)>>9;
    }

    if((gMainCmd.Command.bit.StartDC == 1) || (gMainCmd.Command.bit.StopDC == 1))
    {
        m_Com = 0;
    }

    gIAmpTheta.ThetaFilter = gIAmpTheta.Theta;

    phase = (int)(gPhase.IMPhase>>16) + gIAmpTheta.ThetaFilter + gPhase.CompPhase + 16384;
 #if 1
    gDeadBand.CompU = (phase <= 0) ? m_Com : (-m_Com);

    phase -= 21845;
    gDeadBand.CompV = (phase <= 0) ? m_Com : (-m_Com);

    phase -= 21845;
    gDeadBand.CompW = (phase <= 0) ? m_Com : (-m_Com);

#else
    if(phase <= 0)
    {
        if((phase >= -32504)&&(phase <= -264))
        {
            gDeadBand.CompU = m_Com;
        }
        else
        {
            gDeadBand.CompU = 0;
        }
    }
    else
    {
        if((phase <= 32504)&&(phase >= 264))
        {
            gDeadBand.CompU = (-m_Com);
        }
        else
        {
            gDeadBand.CompU = 0;
        }
    }

    phase -= 21845;

    if(phase <= 0)
    {
        if((phase >= -32504)&&(phase <= -264))
        {
            gDeadBand.CompV = m_Com;
        }
        else
        {
            gDeadBand.CompV = 0;
        }
    }
    else
    {
        if((phase <= 32504)&&(phase >= 264))
        {
            gDeadBand.CompV = (-m_Com);
        }
        else
        {
            gDeadBand.CompV = 0;
        }
    }

    phase -= 21845;

    if(phase <= 0)
    {
        if((phase >= -32504)&&(phase <= -264))
        {
            gDeadBand.CompW = m_Com;
        }
        else
        {
            gDeadBand.CompW = 0;
        }
    }
    else
    {
        if((phase <= 32504)&&(phase >= 264))
        {
            gDeadBand.CompW = (-m_Com);
        }
        else
        {
            gDeadBand.CompW = 0;
        }
    }
 #endif
}
#if 0

void ImSvcDeadBandComp()
{
    ;
}

void ImFvcDeadBandComp()
{
    ;
}

void PmFvcDeadBandComp()
{
    ;
}
#endif
