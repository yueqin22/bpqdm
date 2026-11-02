
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
	gFcCal.FcBak = 50;
	gBasePar.FcSetApply = 50;
	gUVCoff.UDivV = 4096;
    gPWM.gPWMPrd = C_INIT_PRD;

    gMainStatus.StatusWord.all = 0;

    gRotorTrans.Flag = 0;
    gFVCSpeed.MTCnt = 0;
    gFVCSpeed.MTLimitTime = 0;
    gFVCSpeed.MSpeedSmooth.LastSpeed = 0;
    gFVCSpeed.MSpeedSmooth.SpeedMaxErr = 1500;
    gFVCSpeed.MTSpeedSmooth.LastSpeed = 0;
    gFVCSpeed.MTSpeedSmooth.SpeedMaxErr = 1500;
    gFVCSpeed.TransRatio = 1000;
    gPGData.QEPIndex = QEP_SELECT_NONE;
    gPGData.PGType = PG_TYPE_NULL;
    gPWM.gZeroLengthPhase = ZERO_VECTOR_NONE;

    gMotorInfo.MotorType = MOTOR_TYPE_IM;

    gMotorInfo.LastMotorType = MOTOR_NONE;

    gPGData.PGMode = 0;
    gFVCSpeed.MDetaPosBak = 0;

    gIPMPos.ZErrCnt = 0	;
    gIPMPosCheck.UvwRevCnt = 0	;
    gIPMInitPos.Flag = 0;

    gIPMInitPos.InitPWMTs = (50 * DSP_CLOCK);
    gPWM.PWMModle = MODLE_CPWM;

    gIPMPos.Zcounter  = 0;

    ParSend2Ms();
    ParSend05Ms();
}

void Main0msMotor(void)
{
    if(gPGData.PGMode == 0)
    {
    GetMDetaPos();
    GetMTTimeNum();
    }
}

void Main05msMotor(void)
{
  if(ASYNC_SVC == gCtrMotorType)
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

    if(STATUS_GET_PAR == gMainStatus.RunStep)
    {
        ChgParForEst();
    }

    if(gMainCmd.Command.bit.Start == 0)
    {
        SystemParChg2Ms();
        SystemParChg05Ms();

        ChangeMotorPar();
    }
    RunStateParChg2Ms();

}

void Main2msMotorB(void)
{
  int     m05HzPu;
  int     m20HzPu;
    m20HzPu = (200L<<15) / gBasePar.FullFreq01;

	if(gIPMZero.zFilterCnt)
	{
		gIPMZero.zFilterCnt--;
	}
    gMainCmd.FreqSetApply = (long)gMainCmd.FreqSet;

    switch(gCtrMotorType)
    {
        case ASYNC_SVC:
            if(gMainStatus.RunStep != STATUS_SPEED_CHECK)
            {
             	if(0 == gVCPar.SvcMode)
                {
                    m05HzPu = (50L<<15) / gBasePar.FullFreq01;
                    if((abs(gMainCmd.FreqSet)+3) < m05HzPu)
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
                    if((1 == gMainCmd.Command.bit.TorqueCtl)||(1 == gTestDataReceive.TestData1))
					{}
					else
					{
                        m05HzPu = (50L<<15) / gBasePar.FullFreq01;
                        if((abs(gMainCmd.FreqSet)+3) < m05HzPu)
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
            break;

        case ASYNC_FVC:
            VcAsrControl();
            CalWsAndSynFreq();
            gMainCmd.FreqToFunc = gMainCmd.FreqFeed;
            break;

        case SYNC_SVC:
        case SYNC_VF:
        case ASYNC_VF:
            if(gMainStatus.RunStep == STATUS_SPEED_CHECK)
            {
                gOutVolt.Volt           = gFeisu.VoltCheck;
                gMainCmd.FreqSyn      = gFeisu.SpeedCheck;
                gMainCmd.FreqToFunc     = gMainCmd.FreqSyn;
                gVFPar.FreqApply        = gMainCmd.FreqSyn;
                gOutVolt.VoltPhaseApply = (gFeisu.SpeedLast > 0) ? 16384 : -16384;
            }
            break;

        case SYNC_FVC:

            VcAsrControl();
            gMainCmd.FreqSyn = gMainCmd.FreqFeed;
            gMainCmd.FreqToFunc = gMainCmd.FreqFeed;
            break;

        case DC_CONTROL:
            gMainCmd.FreqSyn = 0;
            RunCaseDcBrake();
            gOutVolt.VoltPhaseApply = 0;

            gMainCmd.FreqToFunc = 0;
            break;

        case RUN_SYNC_TUNE:
            ;

            gImAcrQ24.KP = (long)gPmParEst.IdKp * gBasePar.FcSetApply / 80;
            gItAcrQ24.KP = (long)gPmParEst.IqKp * gBasePar.FcSetApply / 80;
            gImAcrQ24.KI = gPmParEst.IdKi;
            gItAcrQ24.KI = gPmParEst.IqKi;

            if((TUNE_STEP_NOW == PM_EST_NO_LOAD) ||
                (TUNE_STEP_NOW == PM_EST_BEMF))
            {
                gMainCmd.FreqSyn = 0;
                gIMTSet.T = 0;

                if(TUNE_STEP_NOW == PM_EST_BEMF)
                {
                    gMainCmd.FreqSyn = gEstBemf.TuneFreqSet;
                    gEstBemf.IdSetFilt = Filter4(gEstBemf.IdSet, gEstBemf.IdSetFilt);
                    gIMTSet.M = (long)gEstBemf.IdSetFilt << 12;
                }
                gMainCmd.FreqToFunc = gMainCmd.FreqSyn;
            }

            if(TUNE_STEP_NOW == PM_EST_WITH_LOAD)
            {
                PrepareAsrPar();
                CalTorqueLimitPar();
                CalUdcLimitIT();
                VcAsrControl();

                gIMTSet.M = 0;
                gMainCmd.FreqSyn = gMainCmd.FreqFeed;
                gMainCmd.FreqToFunc = gMainCmd.FreqFeed;
            }

            break;

        default:
            gMainCmd.FreqSyn = 0;
            gMainCmd.FreqToFunc = 0;
            break;
    }

	CalCarrierWaveFreq();

	if(MOTOR_TYPE_PM != gMotorInfo.MotorType)
    {
        if(gMainStatus.RunStep != STATUS_GET_PAR)
        {
            gCtrMotorType = (CONTROL_MOTOR_TYPE_ENUM)gMainCmd.Command.bit.ControlMode;
        }
        else
        {
            gCtrMotorType = ASYNC_VF;
        }
    }
    else if(MOTOR_TYPE_PM ==gMotorInfo.MotorType)
    {
        gCtrMotorType = (CONTROL_MOTOR_TYPE_ENUM)(gMainCmd.Command.bit.ControlMode + 10);
    }

    if((1 == gMainCmd.Command.bit.StartDC) || (1 == gMainCmd.Command.bit.StopDC))
    {
        gCtrMotorType = DC_CONTROL;
    }

	switch(gMainStatus.RunStep)
	{
		case STATUS_RUN:
			RunCaseRun2Ms();
			break;

        case STATUS_STOP:
            RunCaseStop();
            break;

        case STATUS_IPM_INIT_POS:
			RunCaseIpmInitPos();
            break;

		case STATUS_SPEED_CHECK:

			if(gComPar.SpdSearchMethod == 3)    RunCaseSpeedCheck();
            else                                RunCaseSpeedCheck();
			break;

		case STATUS_GET_PAR:
			RunCaseGetPar();

            if(TUNE_STEP_NOW == IDENTIFY_LM_IO)
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
			break;

		case STATUS_LOW_POWER:
			RunCaseLowPower();
			break;

		case STATUS_SHORT_GND:
			RunCaseShortGnd();
			break;

		default:
            gMainStatus.RunStep = STATUS_STOP;
			break;
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
    Uint tempU;
    int   mAiCounter;
    Ulong mTotal1;
    Ulong mTotal2;
    Ulong mTotal3;
    Uint   mRatio;

	if((gMainStatus.RunStep == STATUS_LOW_POWER) ||
	   (gMainStatus.RunStep == STATUS_STOP) ||
	   (gLineCur.CurBaseInv < (4096/50))    ||
	   (1 == gMainStatus.StatusWord.bit.OutOff ))
	{
		gLineCur.CurPerShow = 0;
        gLineCur.CurTorque  = 0;
	}
	else
	{
		gLineCur.CurPerShow = gLineCur.CurPerFilter >> 7;
        gLineCur.CurTorque  = Filter32(abs(gIMTQ12.T), gLineCur.CurTorque);
	}

	tempU = (Uint)((int)gRotorTrans.RTPos + gRotorTrans.PosComp);
    gRotorTrans.RtRealPos = ((Ulong)tempU * 3600L + 10) >> 16;
	if(gMotorInfo.MotorType == MOTOR_TYPE_PM)
    {
	    gIPMPos.RealPos = ((Ulong)gIPMPos.RotorPos * 3600L + 10) >> 16;
    }

    DINT;
    mTotal1 = gAI.ai1Total;
    mTotal2 = gAI.ai2Total;
    mTotal3 = gAI.ai3Total;
    mAiCounter = gAI.aiCounter;

    gAI.ai1Total = 0;
    gAI.ai2Total = 0;
    gAI.ai3Total = 0;
    gAI.aiCounter = 0;
    EINT;

    gAI.gAI1 = mTotal1 / mAiCounter;
    gAI.gAI2 = mTotal2 / mAiCounter;
    gAI.gAI3 = mTotal3 / mAiCounter;

    mRatio = __IQsat(gRatio, 4096, 0);
    mRatio= (Ulong)mRatio * gUDC.uDC / gInvInfo.BaseUdc;
    gOutVolt.VoltDisplay = Filter4(mRatio, gOutVolt.VoltDisplay);
}

void RunCaseRun2Ms(void)
{

    if(gMainCmd.Command.bit.Start == 0)
    {
        gMainStatus.RunStep = STATUS_STOP;
        RunCaseStop();
        return;
    }

    gMainStatus.StatusWord.bit.StartStop = 1;

    gFeisu.SpeedLast = (gMainCmd.FreqSyn) ? gMainCmd.FreqSyn : gFeisu.SpeedLast;

    switch(gCtrMotorType)
    {
        case ASYNC_SVC:
        case ASYNC_FVC:
            CalTorqueLimitPar();
            PrepareAsrPar();
            PrepImCsrPara();
            CalIMSet();
            CalUdcLimitIT();
            break;

        case ASYNC_VF:
        case SYNC_VF:
            VFWsTorqueBoostComm();
            VFWSCompControl();
            VFAutoTorqueBoost();
            #if 1
            if(speed_DEC &&(abs(gMainCmd.FreqSet) > abs(gVFPar.tpLst)))
            {
              gMainCmd.FreqSet = gVFPar.tpLst;
            }
            gVFPar.tpLst = gMainCmd.FreqSet;
            #endif
    		VfOverCurDeal();
    		VfOverUdcDeal();
    		VfFreqDeal();

            gMainCmd.FreqToFunc = gVFPar.FreqApply;
            gMainCmd.FreqSetApply = gVFPar.FreqApply;

            VFSpeedControl();
            CalTorqueUp();

            HVfOscDampDeal();
            gOutVolt.VoltPhaseApply = gHVfOscDamp.VoltPhase;
            gOutVolt.Volt = gHVfOscDamp.VoltAmp;

            VFOverMagneticControl();
            break;

        case SYNC_SVC:
            break;

        case SYNC_FVC:
            CalTorqueLimitPar();
            CalUdcLimitIT();
            PrepareAsrPar();

            PrepPmsmCsrPrar();

            PmFluxWeakDeal();
            break;

        case DC_CONTROL:
            ;
            break;

        default:
            break;
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

    if(gMainCmd.Command.bit.ControlMode != IDC_VF_CTL)
    {
  	    ResetParForVC();
    }

	gFluxWeak.AdjustId = 0;

	gFeisuNew.gWs_out = 0;
	gFeisuNew.t_DetaTime = 0;
	gFeisuNew.stop_time = 0;
	gFeisuNew.inh_mag = 0;
	gFeisuNew.ang_amu =0;
	gFeisuNew.jicicg  =0;
	gFeisuNew.jicics=0;
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

	if(gMainStatus.StatusWord.bit.RunEnable != 1)
    {
        return;
    }

	if((1 == gExtendCmd.bit.ShortGnd) && (gMainStatus.StatusWord.bit.ShortGndOver == 0))
	{
		gMainStatus.RunStep = STATUS_SHORT_GND;
		gMainStatus.SubStep = 1;
		return;
	}
	else
	{
		gMainStatus.StatusWord.bit.ShortGndOver = 1;
	}

    if(gMotorInfo.MotorType == MOTOR_TYPE_PM)
	{
		IPMCheckInitPos();
	}

	if(gMainCmd.Command.bit.Start == 1)
	{
    #ifdef MOTOR_SYSTEM_DEBUG
        if(gTestDataReceive.TestData16)
        {
            ResetDebugBuffer();
        }
    #endif

        PmChkInitPosRest();

	    if(TUNE_NULL != gGetParVarable.TuneType)
	    {
		    gMainStatus.RunStep = STATUS_GET_PAR;
            PrepareParForTune();
		    return;
	    }

		if(0 != gExtendCmd.bit.SpeedSearch)
		{
			gMainStatus.RunStep = STATUS_SPEED_CHECK;
			gMainStatus.SubStep = 1;
			PrepareForSpeedCheck();
			EnableDrive();
            return;
		}

    	if((gIPMInitPos.Flag == 0) &&
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
	Uint	m_Ratio;

    if( 1 == gMainStatus.StatusWord.bit.OutOff )
    {
	    gOutVolt.VoltApply = 287;
    }

	m_Ratio = ((Ulong)gOutVolt.VoltApply * (Ulong)gMotorInfo.Votage)/gInvInfo.InvVolt;

	m_Ratio = ((Ulong)m_Ratio<<2) * gInvInfo.BaseUdc / gUDC.uDC >> 2;
	gRatio = (m_Ratio > 8192) ? 8192 : m_Ratio;
}

void ADCOverInterrupt()
{

    if(gPGData.PGType == PG_TYPE_RESOLVER)
	{

		RT_SAMPLE_START;

	}

    if(GetOverUdcFlag())
    {
       HardWareOverUDCDeal();
    }

	GetUDCInfo();
	GetCurrentInfo();

	ChangeCurrent();
	OutputLoseAdd();

    switch(gCtrMotorType)
    {
        case ASYNC_SVC:
            CalcABVolt();
            SVCCalFlux_380();

	    	if(0 == gVCPar.SvcMode)
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
            break;

        case ASYNC_FVC:
            CalcABVolt();
            SVCCalFlux_380();
		    VCCsrControl();
            break;

        case ASYNC_VF:
        case SYNC_VF:

            break;

        case SYNC_SVC:
            ;
            break;

        case SYNC_FVC:
            PmDecoupleDeal();
            VCCsrControl();
            break;

        case RUN_SYNC_TUNE:
            ;
            if(TUNE_STEP_NOW == PM_EST_NO_LOAD ||
                TUNE_STEP_NOW == PM_EST_BEMF ||
                TUNE_STEP_NOW == PM_EST_WITH_LOAD)
            {
                PmDecoupleDeal();
                VCCsrControl();
            }
            break;

        default:
            break;
    }
   if(DEADBAND_COMP_280 == gExtendCmd.bit.DeadCompMode)
    {
         CalDeadBandComp();
    }
   else if(DEADBAND_COMP_380== gExtendCmd.bit.DeadCompMode)
    {
         HVfDeadBandComp();
    }
   else
    {
     gDeadBand.CompU = 0;
     gDeadBand.CompV = 0;
     gDeadBand.CompW = 0;
    }

    if((STATUS_GET_PAR ==gMainStatus.RunStep || gMainStatus.RunStep ==STATUS_IPM_INIT_POS)
        && (MOTOR_TYPE_PM == gMotorInfo.MotorType)
        && (gIPMInitPos.Step != 0))
	{
		SynInitPosDetect();
	}

    else if((STATUS_GET_PAR == gMainStatus.RunStep) &&
             (TUNE_STEP_NOW == PM_EST_WITH_LOAD) &&
             (gUVWPG.UvwEstStep == 2))
    {
        GetUvwPhase();
        gUVWPG.TotalErr += (long)(gIPMPos.RotorPos - gUVWPG.UVWAngle);
        gUVWPG.UvwCnt ++;
    }

	else if((STATUS_GET_PAR == gMainStatus.RunStep) &&
             (MOTOR_TYPE_PM != gMotorInfo.MotorType))
	{
		LmIoInPeriodInt();
	}
#ifndef MOTOR_SYSTEM_DEBUG

#endif

    if(gPGData.PGMode == 0)
    {
        GetMDetaPos();
       GetMTTimeNum();
    }
    else if(gPGData.PGType == PG_TYPE_RESOLVER)
	{

		RT_SAMPLE_END;

        #ifdef TMS320F28035
        ROTOR_TRANS_RD = 1;
        ROTOR_TRANS_SCLK   = 1;
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

    if(gMainStatus.PrgStatus.bit.PWMDisable)
    {
        asm(" NOP");
    }
	else
	{
	    OutPutPWMVF();

	}

    if(gPGData.PGMode == 0)
    {
        GetMDetaPos();
        GetMTTimeNum();
    }
    else if(gPGData.PGType == PG_TYPE_RESOLVER)
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
