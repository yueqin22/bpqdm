#include "MotorParaIDinclude.h"
#include "MotorPmsmParEst.h"
#include "MotorEncoder.h"

UV_BIAS_COFF_STRUCT      gUVCoff;
MOTOR_EXTERN_STRUCT      gMotorExtReg;
MOTOR_PARA_EST           gGetParVarable;
IDENTIFY_RRLO_VARIABLE   gRrLoIdentify;
IDENTIFY_LMIO_VARIABLE   gLmIoIdentify;

PAR_EST_MAIN_STEP const AsynParEstStatic[IDENTIFY_PROGRESS_LENGTH] =
{
    IDENTIFY_RS,
    IDENTIFY_RR_LO,
    IDENTIFY_END,
    IDENTIFY_END,
    IDENTIFY_END
};

PAR_EST_MAIN_STEP const AsynParEstRotor[IDENTIFY_PROGRESS_LENGTH] =
{
    IDENTIFY_RS,
    IDENTIFY_RR_LO,
    IDENTIFY_LM_IO,
    IDENTIFY_END,
    IDENTIFY_END
};

PAR_EST_MAIN_STEP const pmTuneProgNoLoad[IDENTIFY_PROGRESS_LENGTH] =
{
    IDENTIFY_RS,
    PM_EST_POLSE_POS,
    PM_EST_NO_LOAD,
    PM_EST_BEMF,

    IDENTIFY_END
};

PAR_EST_MAIN_STEP const pmTuneProgLoad[IDENTIFY_PROGRESS_LENGTH] =
{
    IDENTIFY_RS,
    PM_EST_POLSE_POS,
    PM_EST_WITH_LOAD,

    IDENTIFY_END,
    IDENTIFY_END
};

PAR_EST_MAIN_STEP const noTuneProgress[IDENTIFY_PROGRESS_LENGTH] =
{
    IDENTIFY_END,
    IDENTIFY_END,
    IDENTIFY_END,
    IDENTIFY_END,
    IDENTIFY_END
};

PAR_EST_MAIN_STEP const debugTuneProcess[IDENTIFY_PROGRESS_LENGTH] =
{
    PM_EST_BEMF,
    IDENTIFY_END,
    IDENTIFY_END,
    IDENTIFY_END,
    IDENTIFY_END
};

void EndOfParIdentify(void);

void RunCaseGetPar(void)
{
    PAR_EST_MAIN_STEP m_stepNow;

    if ((gError.ErrorCode.all != 0) || (gMainCmd.Command.bit.Start == 0))
    {
        DisableDrive();
        DINT;

        gGetParVarable.ParEstContent[gGetParVarable.ParEstMstep] = IDENTIFY_END;
        gGetParVarable.StatusWord = TUNE_FINISH;
    }

    m_stepNow = gGetParVarable.ParEstContent[gGetParVarable.ParEstMstep];

    switch (m_stepNow)
    {
        case IDENTIFY_RS:
            RsIdentify();
            break;

        case IDENTIFY_RR_LO:
            RrLoIdentify();
            break;

        case IDENTIFY_LM_IO:
            LmIoIdentify();
            break;

        case PM_EST_POLSE_POS:
            SynTuneInitPos();
            break;

        case PM_EST_NO_LOAD:
            SynTunePGZero_No_Load();
            break;

        case PM_EST_WITH_LOAD:
            SynTunePGZero_Load();
            break;

        case PM_EST_BEMF:
            SynTuneBemf();
            break;

        default:
            EndOfParIdentify();
            break;
    }

    ParSendTune();
}

void PrepareParForTune(void)
{
    int m_index;
    PAR_EST_MAIN_STEP *m_PIdentifyFlow;

    gUVCoff.UDivVSave = gUVCoff.UDivVGet;

    gMotorExtReg.R1 = gMotorExtInfo.R1;
    gMotorExtReg.R2 = gMotorExtInfo.R2;
    gMotorExtReg.L0 = gMotorExtInfo.L0;
    gMotorExtReg.LM = gMotorExtInfo.LM;
    gMotorExtReg.I0 = gMotorExtInfo.I0;

    gMotorExtReg.RsPm = gMotorExtInfo.RsPm;
    gMotorExtReg.LD = gMotorExtInfo.LD;
    gMotorExtReg.LQ = gMotorExtInfo.LQ;
    gEstBemf.BemfVolt = gMotorExtInfo.BemfVolt;

    gPGData.PGDir = gPGData.SpeedDir;
    gPGData.PGErrorFlag = 0;
    gPmParEst.CoderPos_deg = gIPMPos.RotorZeroGet;
    gPmParEst.UvwDir = gUVWPG.UvwDir;
    gPmParEst.UvwZeroAng_deg = gUVWPG.UvwZeroPos_deg;

    gPmParEst.IdKp = gVCPar.AcrImKp;
    gPmParEst.IdKi = gVCPar.AcrImKi;
    gPmParEst.IqKp = gVCPar.AcrItKp;
    gPmParEst.IqKi = gVCPar.AcrItKi;

    gGetParVarable.ParEstMstep = 0;
    gGetParVarable.StatusWord = TUNE_INITIAL;
    gGetParVarable.IdSubStep = 1;
    gUVCoff.IdRsCnt = 0;
    gUVCoff.IdRsDelay = 0;

    gIPMZero.DetectCnt = 0;
    gGetParVarable.QtEstDelay = 0;

    switch (gGetParVarable.TuneType)
    {
        case TUNE_IM_STATIC:
            m_PIdentifyFlow = (PAR_EST_MAIN_STEP *)AsynParEstStatic;
            break;

        case TUNE_IM_ROTOR:
            m_PIdentifyFlow = (PAR_EST_MAIN_STEP *)AsynParEstRotor;
            break;

        case TUNE_PM_COMP_LOAD:
            m_PIdentifyFlow = (PAR_EST_MAIN_STEP *)pmTuneProgLoad;
            break;

        case TUNE_PM_COMP_NO_LOAD:
            m_PIdentifyFlow = (PAR_EST_MAIN_STEP *)pmTuneProgNoLoad;
            break;

        case TUNE_PM_PARA_temp:
            m_PIdentifyFlow = (PAR_EST_MAIN_STEP *)debugTuneProcess;
            break;

        default:
            m_PIdentifyFlow = (PAR_EST_MAIN_STEP *)noTuneProgress;
            break;
    }

    {
        PAR_EST_MAIN_STEP *m_pDst = gGetParVarable.ParEstContent;
        PAR_EST_MAIN_STEP const *m_pSrc = m_PIdentifyFlow;

        for (m_index = 0; m_index < IDENTIFY_PROGRESS_LENGTH; m_index++)
        {
            *m_pDst = *m_pSrc;
            m_pDst++;
            m_pSrc++;
        }
    }

    InitRrLoVariable();
}

void EndOfParIdentify(void)
{
    int m_delay;

    m_delay = gGetParVarable.QtEstDelay;

    if (m_delay == 0)
    {
        gGetParVarable.ParEstMstep = 0;
        gMainStatus.PrgStatus.all = 0;
        if (TUNE_FINISH != gGetParVarable.StatusWord)
        {
            gGetParVarable.StatusWord = TUNE_SUCCESS;
        }
        DisableDrive();

        EALLOW;
        PIE_VECTTABLE_ADCINT = &ADC_Over_isr;
        PieVectTable.EPWM1_TZINT = &EPWM1_TZ_isr;
        PieVectTable.EPWM1_INT = &EPWM1_zero_isr;
        PieCtrlRegs.PIEIER3.bit.INTx2 = 0;
        EDIS;

        InitSetPWM();
        InitSetAdc();
        SetInterruptEnable();
        EINT;
        ERTM;
    }
    else if (m_delay >= 5)
    {
        if (gPGData.PGErrorFlag && (IDC_FVC_CTL == gMainCmd.Command.bit.ControlMode))
        {
            gError.ErrorCode.all |= ERROR_ENCODER;
            gError.ErrorInfo[4].bit.Fault1 = 8 + gPGData.PGErrorFlag;

            gPGData.PGErrorFlag = 0;
        }

        gMainStatus.RunStep = STATUS_STOP;
        gGetParVarable.IdSubStep = 1;
    }

    gGetParVarable.QtEstDelay++;
}
