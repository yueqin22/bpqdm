#include "MotorInclude.h"
#include "MotorDataExchange.h"
#include "SystemDefine.h"
#include "MotorDefine.h"

void RunCaseDeal05Ms(void);
void RunCaseDeal2Ms(void);
void RunCaseRun05Ms(void);
void RunCaseRun2Ms(void);
void RunCaseStop(void);
void SendDataPrepare(void);

void InitForMotorApp(void)
{
    const Uint m_DefaultFreqHz = 50;
    const int  m_SpeedMaxErr = 1500;
    const Uint m_UdcDivValue = 4096;

    DisableDrive();
    TurnOffFan();

    gMainStatus.RunStep = STATUS_LOW_POWER;
    gMainStatus.SubStep = 1;
    gMainStatus.ParaCalTimes = 0;
    gError.LastErrorCode = gError.ErrorCode.all;

    gCBCProtect.EnableFlag = 1;
    gADC.ZeroCnt = 0;
    gADC.DelaySet = 100;
    gADC.DelayApply = 600;
    gFcCal.FcBak = m_DefaultFreqHz;
    gBasePar.FcSetApply = m_DefaultFreqHz;
    gUVCoff.UDivV = m_UdcDivValue;
    gPWM.gPWMPrd = C_INIT_PRD;

    gMainStatus.StatusWord.all = 0;

    gRotorTrans.Flag = 0;
    gFVCSpeed.MTCnt = 0;
    gFVCSpeed.MTLimitTime = 0;
    gFVCSpeed.MSpeedSmooth.LastSpeed = 0;
    gFVCSpeed.MSpeedSmooth.SpeedMaxErr = m_SpeedMaxErr;
    gFVCSpeed.MTSpeedSmooth.LastSpeed = 0;
    gFVCSpeed.MTSpeedSmooth.SpeedMaxErr = m_SpeedMaxErr;
    gFVCSpeed.TransRatio = 1000;
    gPGData.QEPIndex = QEP_SELECT_NONE;
    gPGData.PGType = PG_TYPE_NULL;
    gPWM.gZeroLengthPhase = ZERO_VECTOR_NONE;

    gMotorInfo.MotorType = MOTOR_TYPE_IM;

    gMotorInfo.LastMotorType = MOTOR_NONE;

    gPGData.PGMode = 0;
    gFVCSpeed.MDetaPosBak = 0;

    gIPMPos.ZErrCnt = 0;
    gIPMPosCheck.UvwRevCnt = 0;
    gIPMInitPos.Flag = 0;

    gIPMInitPos.InitPWMTs = (50 * DSP_CLOCK);
    gPWM.PWMModle = MODLE_CPWM;

    gIPMPos.Zcounter = 0;

    ParSend2Ms();
    ParSend05Ms();
}

void Main0msMotor(void)
{
    if (gPGData.PGMode != 0)
    {
        return;
    }

    GetMDetaPos();
    GetMTTimeNum();
}

void Main05msMotor(void)
{
    if (ASYNC_SVC == gCtrMotorType)
    {
        VCGetFeedBackSpeed();
    }
    else
    {
        VCGetFeedBackSpeed();
    }

    #ifdef MOTOR_SYSTEM_DEBUG
    DebugSaveDeal(3);
    #endif
}

void Main2msMotorA(void)
{
    ParGet2Ms();
    ParGet05Ms();

    if (STATUS_GET_PAR == gMainStatus.RunStep)
    {
        ChgParForEst();
    }

    if (0 == gMainCmd.Command.bit.Start)
    {
        SystemParChg2Ms();
        SystemParChg05Ms();

        ChangeMotorPar();
    }
    RunStateParChg2Ms();
}

void Main2msMotorB(void)
{
    int       m_Hz50Pu;
    int       m20HzPu;
    Uint      m_FcSetApply;
    const int m_KpScaleDiv = 80;
    const int m_HalfQ15Value = 16384;

    m20HzPu = (200L<<15) / gBasePar.FullFreq01;

    if (gIPMZero.zFilterCnt)
    {
        gIPMZero.zFilterCnt--;
    }
    gMainCmd.FreqSetApply = (long)gMainCmd.FreqSet;

    if (gCtrMotorType == ASYNC_SVC)
    {
        if (gMainStatus.RunStep != STATUS_SPEED_CHECK)
        {
            if (0 == gVCPar.SvcMode)
            {
                m_Hz50Pu = (50L<<15) / gBasePar.FullFreq01;
                if ((abs(gMainCmd.FreqSet)+3) < m_Hz50Pu)
                {
                    gMainCmd.FreqSet = 0;
                    gMainCmd.FreqSetApply = 0;
                }
                SVCCalRotorSpeed();
                VcAsrControl();
                CalWsAndSynFreq();
            }
            else
            {
                if ((1 == gMainCmd.Command.bit.TorqueCtl)||(1 == gTestDataReceive.TestData1))
                {
                }
                else
                {
                    m_Hz50Pu = (50L<<15) / gBasePar.FullFreq01;
                    if ((abs(gMainCmd.FreqSet)+3) < m_Hz50Pu)
                    {
                        gMainCmd.FreqSet = 0;
                        gMainCmd.FreqSetApply = 0;
                    }
                }
                SVCCalRotorSpeed_380();
                VcAsrControl();
                CalWsAndSynFreq_380();
            }
            gMainCmd.FreqToFunc = gMainCmd.FreqFeed;
        }
        else
        {
            gMainCmd.FreqSyn = gFeisu.SpeedCheck;
            gMainCmd.FreqToFunc = gFeisu.SpeedCheck;

            gIMTSet.T = 0L <<12;
            gOutVolt.VoltApply = (long)gOutVolt.VoltApply * gFeisu.VoltCheck >>12;
        }
    }
    else if (gCtrMotorType == ASYNC_FVC)
    {
        VcAsrControl();
        CalWsAndSynFreq();
        gMainCmd.FreqToFunc = gMainCmd.FreqFeed;
    }
    else if ((gCtrMotorType == SYNC_SVC) || (gCtrMotorType == SYNC_VF) || (gCtrMotorType == ASYNC_VF))
    {
        if (gMainStatus.RunStep == STATUS_SPEED_CHECK)
        {
            gOutVolt.Volt           = gFeisu.VoltCheck;
            gMainCmd.FreqSyn      = gFeisu.SpeedCheck;
            gMainCmd.FreqToFunc     = gMainCmd.FreqSyn;
            gVFPar.FreqApply        = gMainCmd.FreqSyn;
            if (gFeisu.SpeedLast > 0)
            {
                gOutVolt.VoltPhaseApply = m_HalfQ15Value;
            }
            else
            {
                gOutVolt.VoltPhaseApply = -m_HalfQ15Value;
            }
        }
    }
    else if (gCtrMotorType == SYNC_FVC)
    {
        VcAsrControl();
        gMainCmd.FreqSyn = gMainCmd.FreqFeed;
        gMainCmd.FreqToFunc = gMainCmd.FreqFeed;
    }
    else if (gCtrMotorType == DC_CONTROL)
    {
        gMainCmd.FreqSyn = 0;
        RunCaseDcBrake();
        gOutVolt.VoltPhaseApply = 0;

        gMainCmd.FreqToFunc = 0;
    }
    else if (gCtrMotorType == RUN_SYNC_TUNE)
    {
        m_FcSetApply = gBasePar.FcSetApply;

        gImAcrQ24.KP = (long)gPmParEst.IdKp * m_FcSetApply / m_KpScaleDiv;
        gItAcrQ24.KP = (long)gPmParEst.IqKp * m_FcSetApply / m_KpScaleDiv;
        gImAcrQ24.KI = gPmParEst.IdKi;
        gItAcrQ24.KI = gPmParEst.IqKi;

        if ((TUNE_STEP_NOW == PM_EST_NO_LOAD) ||
            (TUNE_STEP_NOW == PM_EST_BEMF))
        {
            gMainCmd.FreqSyn = 0;
            gIMTSet.T = 0;

            if (TUNE_STEP_NOW == PM_EST_BEMF)
            {
                gMainCmd.FreqSyn = gEstBemf.TuneFreqSet;
                gEstBemf.IdSetFilt = Filter4(gEstBemf.IdSet, gEstBemf.IdSetFilt);
                gIMTSet.M = (long)gEstBemf.IdSetFilt << 12;
            }
            gMainCmd.FreqToFunc = gMainCmd.FreqSyn;
        }

        if (TUNE_STEP_NOW == PM_EST_WITH_LOAD)
        {
            PrepareAsrPar();
            CalTorqueLimitPar();
            CalUdcLimitIT();
            VcAsrControl();

            gIMTSet.M = 0;
            gMainCmd.FreqSyn = gMainCmd.FreqFeed;
            gMainCmd.FreqToFunc = gMainCmd.FreqFeed;
        }
    }
    else
    {
        gMainCmd.FreqSyn = 0;
        gMainCmd.FreqToFunc = 0;
    }

    CalCarrierWaveFreq();

    if (MOTOR_TYPE_PM != gMotorInfo.MotorType)
    {
        if (gMainStatus.RunStep == STATUS_GET_PAR)
        {
            gCtrMotorType = ASYNC_VF;
        }
        else
        {
            gCtrMotorType = (CONTROL_MOTOR_TYPE_ENUM)gMainCmd.Command.bit.ControlMode;
        }
    }
    else if (MOTOR_TYPE_PM == gMotorInfo.MotorType)
    {
        gCtrMotorType = (CONTROL_MOTOR_TYPE_ENUM)(gMainCmd.Command.bit.ControlMode + 10);
    }

    if ((1 == gMainCmd.Command.bit.StartDC) || (1 == gMainCmd.Command.bit.StopDC))
    {
        gCtrMotorType = DC_CONTROL;
    }

    if (gMainStatus.RunStep == STATUS_RUN)
    {
        RunCaseRun2Ms();
    }
    else if (gMainStatus.RunStep == STATUS_STOP)
    {
        RunCaseStop();
    }
    else if (gMainStatus.RunStep == STATUS_IPM_INIT_POS)
    {
        RunCaseIpmInitPos();
    }
    else if (gMainStatus.RunStep == STATUS_SPEED_CHECK)
    {
        if (gComPar.SpdSearchMethod == 3)
        {
            RunCaseSpeedCheck();
        }
        else
        {
            RunCaseSpeedCheck();
        }
    }
    else if (gMainStatus.RunStep == STATUS_GET_PAR)
    {
        RunCaseGetPar();

        if (TUNE_STEP_NOW == IDENTIFY_LM_IO)
        {
            VfOverCurDeal();
            VfOverUdcDeal();
            VfFreqDeal();

            gMainCmd.FreqToFunc = gVFPar.FreqApply;
            gMainCmd.FreqSetApply = gVFPar.FreqApply;

            gWsComp.CompFreq = 0;
            VFSpeedControl();
            CalTorqueUp();
            HVfOscDampDeal();
            gOutVolt.VoltPhaseApply = gHVfOscDamp.VoltPhase;
            gOutVolt.Volt = gHVfOscDamp.VoltAmp;
            VFOverMagneticControl();
        }
    }
    else if (gMainStatus.RunStep == STATUS_LOW_POWER)
    {
        RunCaseLowPower();
    }
    else if (gMainStatus.RunStep == STATUS_SHORT_GND)
    {
        RunCaseShortGnd();
    }
    else
    {
        gMainStatus.RunStep = STATUS_STOP;
    }
}

void Main2msMotorC(void)
{
    InvCalcPower();
    VfOscIndexCalc();

    InvDeviceControl();
}

void Main2msMotorD(void)
{
    GetCurExcursion();

    SendDataPrepare();

    ParSend2Ms();
    ParSend05Ms();

    gCpuTime.CpuBusyCoff = (Ulong)gCpuTime.Det05msClk * 655 >> 16;
    gCpuTime.CpuCoff0Ms = gCpuTime.tmp0Ms;
    gCpuTime.tmp0Ms = 0;

    #ifdef MOTOR_SYSTEM_DEBUG
    DebugSaveDeal(2);
    #endif
}

void SendDataPrepare(void)
{
    Uint  m_PosSum;
    int   m_SampleCnt;
    Ulong m_Sum1;
    Ulong m_Sum2;
    Ulong m_Sum3;
    Uint  m_VoltRatio;

    if (!((gMainStatus.RunStep == STATUS_LOW_POWER) ||
          (gMainStatus.RunStep == STATUS_STOP) ||
          (gLineCur.CurBaseInv < (4096/50))    ||
          (1 == gMainStatus.StatusWord.bit.OutOff)))
    {
        gLineCur.CurPerShow = gLineCur.CurPerFilter >> 7;
        gLineCur.CurTorque  = Filter32(abs(gIMTQ12.T), gLineCur.CurTorque);
    }
    else
    {
        gLineCur.CurPerShow = 0;
        gLineCur.CurTorque  = 0;
    }

    m_PosSum = (Uint)((int)gRotorTrans.RTPos + gRotorTrans.PosComp);
    gRotorTrans.RtRealPos = ((Ulong)m_PosSum * 3600L + 10) >> 16;
    if (gMotorInfo.MotorType == MOTOR_TYPE_PM)
    {
        gIPMPos.RealPos = ((Ulong)gIPMPos.RotorPos * 3600L + 10) >> 16;
    }

    DINT;
    m_Sum1 = gAI.ai1Total;
    m_Sum2 = gAI.ai2Total;
    m_Sum3 = gAI.ai3Total;
    m_SampleCnt = gAI.aiCounter;

    gAI.ai1Total = 0;
    gAI.ai2Total = 0;
    gAI.ai3Total = 0;
    gAI.aiCounter = 0;
    EINT;

    gAI.gAI1 = m_Sum1 / m_SampleCnt;
    gAI.gAI2 = m_Sum2 / m_SampleCnt;
    gAI.gAI3 = m_Sum3 / m_SampleCnt;

    m_VoltRatio = __IQsat(gRatio, 4096, 0);
    m_VoltRatio = (Ulong)m_VoltRatio * gUDC.uDC / gInvInfo.BaseUdc;
    gOutVolt.VoltDisplay = Filter4(m_VoltRatio, gOutVolt.VoltDisplay);
}

void RunCaseRun2Ms(void)
{
    int m_FreqApply;

    if (gMainCmd.Command.bit.Start == 0)
    {
        gMainStatus.RunStep = STATUS_STOP;
        RunCaseStop();
        return;
    }

    gMainStatus.StatusWord.bit.StartStop = 1;

    if (gMainCmd.FreqSyn)
    {
        m_FreqApply = gMainCmd.FreqSyn;
    }
    else
    {
        m_FreqApply = gFeisu.SpeedLast;
    }
    gFeisu.SpeedLast = m_FreqApply;

    if ((gCtrMotorType == ASYNC_SVC) || (gCtrMotorType == ASYNC_FVC))
    {
        CalTorqueLimitPar();
        PrepareAsrPar();
        PrepImCsrPara();
        CalIMSet();
        CalUdcLimitIT();
    }
    else if ((gCtrMotorType == ASYNC_VF) || (gCtrMotorType == SYNC_VF))
    {
        VFWsTorqueBoostComm();
        VFWSCompControl();
        VFAutoTorqueBoost();
        #if 1
        if (speed_DEC &&(abs(gMainCmd.FreqSet) > abs(gVFPar.tpLst)))
        {
          gMainCmd.FreqSet = gVFPar.tpLst;
        }
        gVFPar.tpLst = gMainCmd.FreqSet;
        #endif
        VfOverCurDeal();
        VfOverUdcDeal();
        VfFreqDeal();

        m_FreqApply = gVFPar.FreqApply;
        gMainCmd.FreqToFunc = m_FreqApply;
        gMainCmd.FreqSetApply = m_FreqApply;

        VFSpeedControl();
        CalTorqueUp();

        HVfOscDampDeal();
        gOutVolt.VoltPhaseApply = gHVfOscDamp.VoltPhase;
        gOutVolt.Volt = gHVfOscDamp.VoltAmp;

        VFOverMagneticControl();
    }
    else if (gCtrMotorType == SYNC_SVC)
    {
    }
    else if (gCtrMotorType == SYNC_FVC)
    {
        CalTorqueLimitPar();
        CalUdcLimitIT();
        PrepareAsrPar();

        PrepPmsmCsrPrar();

        PmFluxWeakDeal();
    }
    else if (gCtrMotorType == DC_CONTROL)
    {
    }
}

void PrepareParForRun(void)
{
    gMainStatus.StatusWord.bit.StartStop = 0;
    gMainStatus.StatusWord.bit.SpeedSearchOver = 0;

    gMainStatus.PrgStatus.all = 0;
    gMainStatus.PrgStatus.bit.ACRDisable = 1;
    gGetParVarable.StatusWord = TUNE_INITIAL;
    gVarAvr.UDCFilter = gInvInfo.BaseUdc;
    gMainCmd.FreqSyn = 0;
    gMainCmd.FreqReal = 0;
    gOutVolt.Volt = 0;
    gOutVolt.VoltApply = 0;
    gRatio = 0;
    gCurSamp.U = 0;
    gCurSamp.V = 0;
    gCurSamp.W = 0;
    gCurSamp.UErr = 600L<<12;
    gCurSamp.VErr = 600L<<12;
    gIUVWQ24.U = 0;
    gIUVWQ24.V = 0;
    gIUVWQ24.W = 0;
    gIUVWQ12.U = 0;
    gIUVWQ12.V = 0;
    gIUVWQ12.W = 0;
    gLineCur.CurPerShow = 0;
    gLineCur.CurTorque  = 0;
    gLineCur.CurBaseInv = 0;
    gLineCur.CurPer = 0;
    gLineCur.CurPerFilter = 0;
    gIMTQ12.M = 0;
    gIMTQ12.T = 0;
    gIMTQ24.M = 0;
    gIMTQ24.T = 0;
    gDCBrake.Time = 0;
    gPWM.gZeroLengthPhase = ZERO_VECTOR_NONE;
    gIAmpTheta.ThetaFilter = gIAmpTheta.Theta;

    VfVarInitiate();

    gSpeedFilter.Max = 32767;
    gSpeedFilter.Min = 3277;
    gSpeedFilter.Output = 0;

    if (IDC_VF_CTL != gMainCmd.Command.bit.ControlMode)
    {
        ResetParForVC();
    }

    gFluxWeak.AdjustId = 0;

    gFeisuNew.gWs_out = 0;
    gFeisuNew.t_DetaTime = 0;
    gFeisuNew.stop_time = 0;
    gFeisuNew.inh_mag = 0;
    gFeisuNew.ang_amu = 0;
    gFeisuNew.jicicg = 0;
    gFeisuNew.jicics = 0;
}

void TurnToStopStatus(void)
{
    DisableDrive();
    gMainStatus.RunStep = STATUS_STOP;
    gMainStatus.SubStep = 1;
}

void RunCaseStop(void)
{
    DisableDrive();
    PrepareParForRun();
    gMainCmd.FreqToFunc = 0;

    if (gMainStatus.StatusWord.bit.RunEnable != 1)
    {
        return;
    }

    if ((1 == gExtendCmd.bit.ShortGnd) && (gMainStatus.StatusWord.bit.ShortGndOver == 0))
    {
        gMainStatus.RunStep = STATUS_SHORT_GND;
        gMainStatus.SubStep = 1;
        return;
    }

    gMainStatus.StatusWord.bit.ShortGndOver = 1;

    if (gMotorInfo.MotorType == MOTOR_TYPE_PM)
    {
        IPMCheckInitPos();
    }

    if (gMainCmd.Command.bit.Start == 1)
    {
    #ifdef MOTOR_SYSTEM_DEBUG
        if (gTestDataReceive.TestData16)
        {
            ResetDebugBuffer();
        }
    #endif

        PmChkInitPosRest();

        if (TUNE_NULL != gGetParVarable.TuneType)
        {
            gMainStatus.RunStep = STATUS_GET_PAR;
            PrepareParForTune();
            return;
        }

        if (0 != gExtendCmd.bit.SpeedSearch)
        {
            gMainStatus.RunStep = STATUS_SPEED_CHECK;
            gMainStatus.SubStep = 1;
            PrepareForSpeedCheck();
            EnableDrive();
            return;
        }

        if ((gIPMInitPos.Flag == 0) &&
            (gMotorInfo.MotorType == MOTOR_TYPE_PM) &&
            (gPGData.PGType == PG_TYPE_ABZ))
        {
            gMainStatus.RunStep = STATUS_IPM_INIT_POS;
            gMainStatus.SubStep = 1;
            gIPMInitPos.Step = 0;

            return;
        }

        gMainStatus.RunStep = STATUS_RUN;
        gMainStatus.PrgStatus.all = 0;
        gMainStatus.SubStep = 1;

        EnableDrive();

        RunCaseRun2Ms();
    }
}

void CalRatioFromVot(void)
{
    Uint m_RatioTmp;
    const Uint m_MaxRatioValue = 8192;
    const int  m_OffVoltApply = 287;

    if (1 == gMainStatus.StatusWord.bit.OutOff)
    {
        gOutVolt.VoltApply = m_OffVoltApply;
    }

    m_RatioTmp = ((Ulong)gOutVolt.VoltApply * (Ulong)gMotorInfo.Votage)/gInvInfo.InvVolt;

    m_RatioTmp = ((Ulong)m_RatioTmp<<2) * gInvInfo.BaseUdc / gUDC.uDC >> 2;
    if (m_RatioTmp > m_MaxRatioValue)
    {
        gRatio = m_MaxRatioValue;
    }
    else
    {
        gRatio = m_RatioTmp;
    }
}

void ADCOverInterrupt()
{
    if (gPGData.PGType == PG_TYPE_RESOLVER)
    {
        RT_SAMPLE_START;
    }

    if (GetOverUdcFlag())
    {
        HardWareOverUDCDeal();
    }

    GetUDCInfo();
    GetCurrentInfo();

    ChangeCurrent();
    OutputLoseAdd();

    if (gCtrMotorType == ASYNC_SVC)
    {
        CalcABVolt();
        SVCCalFlux_380();

        if (0 == gVCPar.SvcMode)
        {
            SvcCalOutVolt();
        }
        else
        {
            AlphBetaToDQ((ALPHABETA_STRUCT*)&gIAlphBeta,gFluxR.Theta, &gIMTQ24_obs);

            gIMTQ12_obs.M = (gIMTQ24_obs.M>>12);
            gIMTQ12_obs.T = (gIMTQ24_obs.T>>12);

            SvcCalOutVolt_380();
        }
    }
    else if (gCtrMotorType == ASYNC_FVC)
    {
        CalcABVolt();
        SVCCalFlux_380();
        VCCsrControl();
    }
    else if ((gCtrMotorType == ASYNC_VF) || (gCtrMotorType == SYNC_VF))
    {
    }
    else if (gCtrMotorType == SYNC_SVC)
    {
    }
    else if (gCtrMotorType == SYNC_FVC)
    {
        PmDecoupleDeal();
        VCCsrControl();
    }
    else if (gCtrMotorType == RUN_SYNC_TUNE)
    {
        if (TUNE_STEP_NOW == PM_EST_NO_LOAD ||
            TUNE_STEP_NOW == PM_EST_BEMF ||
            TUNE_STEP_NOW == PM_EST_WITH_LOAD)
        {
            PmDecoupleDeal();
            VCCsrControl();
        }
    }

    if (DEADBAND_COMP_280 == gExtendCmd.bit.DeadCompMode)
    {
        CalDeadBandComp();
    }
    else if (DEADBAND_COMP_380 == gExtendCmd.bit.DeadCompMode)
    {
        HVfDeadBandComp();
    }
    else
    {
        gDeadBand.CompU = 0;
        gDeadBand.CompV = 0;
        gDeadBand.CompW = 0;
    }

    if ((STATUS_GET_PAR == gMainStatus.RunStep || gMainStatus.RunStep == STATUS_IPM_INIT_POS)
        && (MOTOR_TYPE_PM == gMotorInfo.MotorType)
        && (gIPMInitPos.Step != 0))
    {
        SynInitPosDetect();
    }
    else if ((STATUS_GET_PAR == gMainStatus.RunStep) &&
             (TUNE_STEP_NOW == PM_EST_WITH_LOAD) &&
             (gUVWPG.UvwEstStep == 2))
    {
        GetUvwPhase();
        gUVWPG.TotalErr += (long)(gIPMPos.RotorPos - gUVWPG.UVWAngle);
        gUVWPG.UvwCnt ++;
    }
    else if ((STATUS_GET_PAR == gMainStatus.RunStep) &&
             (MOTOR_TYPE_PM != gMotorInfo.MotorType))
    {
        LmIoInPeriodInt();
    }
#ifndef MOTOR_SYSTEM_DEBUG

#endif

    if (gPGData.PGMode == 0)
    {
        GetMDetaPos();
        GetMTTimeNum();
    }
    else if (gPGData.PGType == PG_TYPE_RESOLVER)
    {
        RT_SAMPLE_END;

        #ifdef TMS320F28035
        ROTOR_TRANS_RD = 1;
        ROTOR_TRANS_SCLK = 1;
        #endif
    }

#ifdef MOTOR_SYSTEM_DEBUG
    #ifdef TMS320F2808
    DebugSaveDeal(0);
    #endif
#endif
}

void PWMZeroInterrupt()
{
    CalRatioFromVot();
    SoftPWMProcess();
    CalOutputPhase();

    BrakeResControl();

    if (gMainStatus.PrgStatus.bit.PWMDisable)
    {
        asm(" NOP");
    }
    else
    {
        OutPutPWMVF();
    }

    if (gPGData.PGMode == 0)
    {
        GetMDetaPos();
        GetMTTimeNum();
    }
    else if (gPGData.PGType == PG_TYPE_RESOLVER)
    {
        GetRotorTransPos();

        gRotorTrans.IntCnt ++;
    }

#ifdef MOTOR_SYSTEM_DEBUG
    #ifdef TMS320F2808
    DebugSaveDeal(1);
    #endif
#endif
}
