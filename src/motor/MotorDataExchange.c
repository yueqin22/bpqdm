
#include "MotorInclude.h"

#define     F2M_2MS_MAIN_WORDS      75
#define     F2M_2MS_DEBUG_WORDS     40
#define     F2M_2MS_DEBUG_BASE      F2M_2MS_MAIN_WORDS

#define     M2F_2MS_MAIN_WORDS      30
#define     M2F_2MS_EXT_WORDS       30
#define     M2F_2MS_EXT_BASE        M2F_2MS_MAIN_WORDS

#define     F2M_05MS_WORDS          14
#define     M2F_05MS_WORDS          5
#define     M2F_TUNE_WORDS          20

int  gSendToMotor05MsDataBuff[F2M_05MS_WORDS];
int  gSendToMotor2MsDataBuff[F2M_2MS_MAIN_WORDS + F2M_2MS_DEBUG_WORDS];
int  gSendToFunctionDataBuff[M2F_2MS_MAIN_WORDS + M2F_2MS_EXT_WORDS];
int  gRealTimeToFunctionDataBuff[M2F_05MS_WORDS];
int  gParaIdToFunctionDataBuff[M2F_TUNE_WORDS];

Uint uReservedData;
Uint gSoftVersion = SOFT_VERSION;

extern MT_STRUCT_Q24           gIMTQ12_obs;
extern ALPHABETA_STRUCT        gABFluxS_LowFilter;
extern ALPHABETA_STRUCT        gABFluxS_comp;

void ParSend05Ms(void)
{

}

void ParGet05Ms(void)
{
    gMainCmd.Command.all            =    gSendToMotor05MsDataBuff[0]    ;
    gExtendCmd.all                  =    gSendToMotor05MsDataBuff[1]    ;
    gGetParVarable.TuneType         =    (TUNE_FLOW_ENUM)gSendToMotor05MsDataBuff[2]    ;
    gMotorInfo.MotorType            =    gSendToMotor05MsDataBuff[3]    ;
    gMainCmd.FreqSet                =    gSendToMotor05MsDataBuff[4]    ;
    gOutVolt.vfSplit                =    gSendToMotor05MsDataBuff[5]    ;
    gMotorInfo.Votage               =    gSendToMotor05MsDataBuff[6]    ;
    gMotorInfo.CurrentGet           =    gSendToMotor05MsDataBuff[7]    ;
    gMotorInfo.Frequency            =    gSendToMotor05MsDataBuff[8]    ;
    gVFPar.VFLineType               =    gSendToMotor05MsDataBuff[9]    ;

    gVFPar.ovGain                   =    gSendToMotor05MsDataBuff[10]    ;
    gVFPar.ovPoint                  =    gSendToMotor05MsDataBuff[11]    ;
    gVFPar.ocGain                   =    gSendToMotor05MsDataBuff[12]    ;
    gVFPar.ocPoint                  =    gSendToMotor05MsDataBuff[13]    ;
}

void ParSend2Ms(void)
{
    int *ext = &gSendToFunctionDataBuff[M2F_2MS_EXT_BASE];

    gSendToFunctionDataBuff[0]    =    gMainStatus.StatusWord.all    ;
    gSendToFunctionDataBuff[1]    =    gGetParVarable.StatusWord    ;
    gSendToFunctionDataBuff[2]    =    gError.ErrorCode.ErrorCodeStruct.ErrorCode1    ;
    gSendToFunctionDataBuff[3]    =    gError.ErrorCode.ErrorCodeStruct.ErrorCode2    ;
    gSendToFunctionDataBuff[4]    =    gError.ErrorInfo[0].all    ;
    gSendToFunctionDataBuff[5]    =    gError.ErrorInfo[1].all    ;
    gSendToFunctionDataBuff[6]    =    gError.ErrorInfo[2].all    ;
    gSendToFunctionDataBuff[7]    =    gError.ErrorInfo[3].all    ;
    gSendToFunctionDataBuff[8]    =    gError.ErrorInfo[4].all    ;
    gSendToFunctionDataBuff[9]    =    gLineCur.ErrorShow    ;
    gSendToFunctionDataBuff[10]    =    gMainCmd.FreqToFunc    ;
    gSendToFunctionDataBuff[11]    =    gOutVolt.VoltDisplay    ;
    gSendToFunctionDataBuff[12]    =    gTemperature.Temp    ;
    gSendToFunctionDataBuff[13]    =    gMotorInfo.Current    ;
    gSendToFunctionDataBuff[14]    =    gSoftVersion    ;
    gSendToFunctionDataBuff[15]    =    gAI.gAI1    ;
    gSendToFunctionDataBuff[16]    =    gAI.gAI2    ;
    gSendToFunctionDataBuff[17]    =    gAI.gAI3    ;
    gSendToFunctionDataBuff[18]    =    gUDC.uDCFilter    ;
    gSendToFunctionDataBuff[19]    =    gLineCur.CurTorque    ;
    gSendToFunctionDataBuff[20]    =    gLineCur.CurPerShow    ;
    gSendToFunctionDataBuff[21]    =    gIPMPos.RealPos    ;
    gSendToFunctionDataBuff[22]    =    uReservedData    ;
    gSendToFunctionDataBuff[23]    =    gFVCSpeed.SpeedEncoder;
    gSendToFunctionDataBuff[24] =   gPowerTrq.InvPower_si;
    gSendToFunctionDataBuff[25] =   gPowerTrq.TrqOut_pu;
    gSendToFunctionDataBuff[26] =   gRotorTrans.AbsRotPos;
    gSendToFunctionDataBuff[27] =   gPowerTrq.anglePF;
    gSendToFunctionDataBuff[28] =   gIPMPos.Zcounter;
    gSendToFunctionDataBuff[29] =   gHVfOscIndex.oscIndex;

    ext[0] =   gIMTSetQ12.M;
    ext[1] =   gIMTQ12.M;
    ext[2] =   gIMTSetQ12.T;
    ext[3] =   gIMTQ12.T;
    ext[4] =   gIAmpTheta.PowerAngle;

    ext[5] =   gMainCmd.FreqFeed;
    ext[6] =   gFVCSpeed.MTTime;
    ext[7]    =    gFVCSpeed.MTPulseNum        ;
    ext[8]    =    gIAmpTheta.Amp     ;
    ext[9]    =    gLineCur.CurBaseInv    ;

    ext[10]    =    gMainCmd.FreqSet    ;
    ext[11] =   gFVCSpeed.SpeedEncoder;
    ext[12]    =    gIMTQ12_obs.M     ;
    ext[13]    =    gIMTQ12_obs.T     ;
    ext[14]    =    gRatio    ;

    ext[15]    =    gUMTSet.M    ;
    ext[16]    =    gUMTSet.T     ;
    ext[17]    =    gUVWPG.UVWAngle ;
    ext[18]    =    gOutVolt.VoltPhaseApply;
    ext[19]    =    gIPMPos.RotorPos    ;

    ext[20]    =   gABFluxS_LowFilter.Alph;
    ext[21]    =   gABFluxS_LowFilter.Beta;
    ext[22]    =   gPhase.IMPhase>>16;
    ext[23]    =   gFluxR.Theta ;
    ext[24]    =    gFluxR.Amp    ;

    ext[25]    =    gIPMPosCheck.UvwRevCnt    ;
    ext[26]    =    gIUVWQ12.U;
    ext[27]    =    gIPMPos.ABZ_QepTotal    ;
    ext[28]    =    gCpuTime.CpuCoff0Ms    ;
    ext[29]    =    gCpuTime.CpuBusyCoff    ;
}

void ParGet2Ms(void)
{
    int *dbg = &gSendToMotor2MsDataBuff[F2M_2MS_DEBUG_BASE];

    gSubCommand.all              =    gSendToMotor2MsDataBuff[0]    ;
    gMainCmd.FreqDesired         =    gSendToMotor2MsDataBuff[1]    ;
    gBasePar.MaxFreq             =    gSendToMotor2MsDataBuff[2]    ;
    gBasePar.FcSet               =    gSendToMotor2MsDataBuff[3]    ;
    gMotorInfo.Power             =    gSendToMotor2MsDataBuff[4]    ;
    gMotorExtInfo.Rpm            =    gSendToMotor2MsDataBuff[5]    ;
    gMotorExtInfo.R1             =    gSendToMotor2MsDataBuff[6]    ;
    gMotorExtInfo.R2             =    gSendToMotor2MsDataBuff[7]    ;
    gMotorExtInfo.L0             =    gSendToMotor2MsDataBuff[8]    ;
    gMotorExtInfo.LM             =    gSendToMotor2MsDataBuff[9]    ;
    gMotorExtInfo.I0             =    gSendToMotor2MsDataBuff[10]    ;
    gMotorExtInfo.RsPm           =    gSendToMotor2MsDataBuff[11]    ;
    gMotorExtInfo.LD             =    gSendToMotor2MsDataBuff[12]    ;
    gMotorExtInfo.LQ             =    gSendToMotor2MsDataBuff[13]    ;
    gVCPar.ASRKpLow              =    gSendToMotor2MsDataBuff[14]    ;
    gVCPar.ASRTILow              =    gSendToMotor2MsDataBuff[15]    ;
    gVCPar.ASRKpHigh             =    gSendToMotor2MsDataBuff[16]    ;
    gVCPar.ASRTIHigh             =    gSendToMotor2MsDataBuff[17]    ;
    gVCPar.ASRSwitchLow          =    gSendToMotor2MsDataBuff[18]    ;
    gVCPar.ASRSwitchHigh         =    gSendToMotor2MsDataBuff[19]    ;
    gVCPar.VCWsCoff              =    gSendToMotor2MsDataBuff[20]    ;
    gVCPar.VCSpeedFilter         =    gSendToMotor2MsDataBuff[21]    ;
    gPGData.PulseNum             =    gSendToMotor2MsDataBuff[22]    ;
    gVCPar.VcOverExc             =    gSendToMotor2MsDataBuff[23]    ;
    gLoadLose.ChkLevel           =  gSendToMotor2MsDataBuff[24]    ;
    gLoadLose.ChkTime            =  gSendToMotor2MsDataBuff[25]    ;
    gVCPar.VCTorqueLim           =    gSendToMotor2MsDataBuff[26]    ;
    gVFPar.VFTorqueUp            =    gSendToMotor2MsDataBuff[27]    ;
    gVFPar.VFTorqueUpLim         =    gSendToMotor2MsDataBuff[28]    ;
    gVFPar.VFLineFreq1           =    gSendToMotor2MsDataBuff[29]    ;
    gVFPar.VFLineVolt1           =    gSendToMotor2MsDataBuff[30]    ;
    gVFPar.VFLineFreq2           =    gSendToMotor2MsDataBuff[31]    ;
    gVFPar.VFLineVolt2           =    gSendToMotor2MsDataBuff[32]    ;
    gVFPar.VFLineFreq3           =    gSendToMotor2MsDataBuff[33]    ;
    gVFPar.VFLineVolt3           =    gSendToMotor2MsDataBuff[34]    ;
    gVFPar.VFWsComp              =    gSendToMotor2MsDataBuff[35]    ;
    gVFPar.VFOverExc             =    gSendToMotor2MsDataBuff[36]    ;
    gVFPar.VFOvShock             =    gSendToMotor2MsDataBuff[37]    ;
    gComPar.StartDCBrakeCur      =    gSendToMotor2MsDataBuff[38]    ;
    gComPar.StopDCBrakeCur       =    gSendToMotor2MsDataBuff[39]    ;
    gComPar.BrakeCoff            =    gSendToMotor2MsDataBuff[40]    ;
    gComPar.MotorOvLoad          =    gSendToMotor2MsDataBuff[41]    ;
    gComPar.PerMotorOvLoad       =    gSendToMotor2MsDataBuff[42]    ;
    gPWM.SoftPWMTune             =    gSendToMotor2MsDataBuff[43]    ;
    gADC.DelaySet                =    gSendToMotor2MsDataBuff[44]    ;
    gInvInfo.LowUdcCoff          =    gSendToMotor2MsDataBuff[45]    ;
    gPGData.PGTypeGetFromFun     =    gSendToMotor2MsDataBuff[46]    ;
    gFVCSpeed.TransRatio       =    gSendToMotor2MsDataBuff[47]    ;
    gInvInfo.InvTypeSet          =    gSendToMotor2MsDataBuff[48]    ;
    gInvInfo.GpTypeSet           =    gSendToMotor2MsDataBuff[49]    ;
    gInvInfo.TempType            =    gSendToMotor2MsDataBuff[50]    ;
    gInvInfo.UDCCoff             =    gSendToMotor2MsDataBuff[51]    ;
    gInvInfo.CurrentCoff         =    gSendToMotor2MsDataBuff[52]    ;
    gUVCoff.UDivVGet             =    gSendToMotor2MsDataBuff[53]    ;
    gVCPar.SvcMode               =    gSendToMotor2MsDataBuff[54]    ;
    gDeadBand.DeadTimeSet        =    gSendToMotor2MsDataBuff[55]    ;
    gComPar.SpdSearchMethod      =    gSendToMotor2MsDataBuff[56]    ;
    gComPar.SpdSearchTimeSet     =    gSendToMotor2MsDataBuff[57]    ;
    gIPMPos.PowerOffPos          =    gSendToMotor2MsDataBuff[58]    ;
    gRotorTrans.Poles            =    gSendToMotor2MsDataBuff[59]    ;
    gMotorExtInfo.BemfVolt       =    gSendToMotor2MsDataBuff[60]    ;
    gVCPar.AcrImKp               =    gSendToMotor2MsDataBuff[61]    ;
    gVCPar.AcrImKi               =    gSendToMotor2MsDataBuff[62]    ;
    gVCPar.AcrItKp               =    gSendToMotor2MsDataBuff[63]    ;
    gVCPar.AcrItKi               =    gSendToMotor2MsDataBuff[64]    ;
    gPGData.SpeedDir             =    gSendToMotor2MsDataBuff[65]    ;
    gIPMPos.RotorZeroGet         =    gSendToMotor2MsDataBuff[66]    ;
    gPWM.PwmModeSwitchLF       =    gSendToMotor2MsDataBuff[67] ;
    gUVWPG.UvwDir                           =    gSendToMotor2MsDataBuff[68]    ;
    gUVWPG.UvwZeroPos_deg                   =    gSendToMotor2MsDataBuff[69]    ;
    gFluxWeak.Mode                          =    gSendToMotor2MsDataBuff[70]    ;
    gFluxWeak.CoefFlux                      =    gSendToMotor2MsDataBuff[71]    ;
    gFluxWeak.IdMax                         =    gSendToMotor2MsDataBuff[72]    ;
    gFluxWeak.CoefAdj                       =    gSendToMotor2MsDataBuff[73]    ;
    gFluxWeak.CoefKI                        =    gSendToMotor2MsDataBuff[74]    ;

    gTestDataReceive.TestData0       =    dbg[0]    ;
    gTestDataReceive.TestData1       =    dbg[1]    ;
    gTestDataReceive.TestData2       =    dbg[2]    ;
    gTestDataReceive.TestData3       =    dbg[3]    ;
    gTestDataReceive.TestData4       =    dbg[4]    ;
    gTestDataReceive.TestData5       =    dbg[5]    ;
    gVFPar.SVCTorqueUp               =    dbg[6]    ;
    gVFPar.SVCTorqueUpLim            =    dbg[7]    ;

    gTestDataReceive.TestData8       =    dbg[8]    ;
    gTestDataReceive.TestData9       =    dbg[9]    ;
    gTestDataReceive.TestData10      =    dbg[10]    ;
    gMotorExtPer.FluxLeakCurveGain   =    dbg[11]    ;

    gTestDataReceive.TestData12      =    dbg[12]    ;
    gTestDataReceive.TestData13      =    dbg[13]    ;
    gTestDataReceive.TestData14      =    dbg[14]    ;
    gTestDataReceive.TestData15      =    dbg[15]    ;
    gTestDataReceive.TestData16      =    dbg[16]    ;
    gTestDataReceive.TestData17      =    dbg[17]    ;
    gTestDataReceive.TestData18      =    dbg[18]    ;
    gTestDataReceive.TestData19      =    dbg[19]    ;
    gTestDataReceive.TestData20      =    dbg[20]    ;
    gTestDataReceive.TestData21      =    dbg[21]    ;
    gTestDataReceive.TestData22      =    dbg[22]    ;
    gTestDataReceive.TestData23      =    dbg[23]    ;
    gTestDataReceive.TestData24      =    dbg[24]    ;
    gTestDataReceive.TestData25      =    dbg[25]    ;
    gTestDataReceive.TestData26      =    dbg[26]    ;
    gTestDataReceive.TestData27      =    dbg[27]    ;
    gTestDataReceive.TestData28      =    dbg[28]    ;
    gTestDataReceive.TestData29      =    dbg[29]    ;
    gTestDataReceive.TestData30      =    dbg[30]    ;
    gTestDataReceive.TestData31      =    dbg[31]    ;
    gTestDataReceive.TestData32      =    dbg[32]    ;
    gTestDataReceive.TestData33      =    dbg[33]    ;
    gTestDataReceive.TestData34      =    dbg[34]    ;
    gTestDataReceive.TestData35      =    dbg[35]    ;
    gTestDataReceive.TestData36      =    dbg[36]    ;
    gTestDataReceive.TestData37      =    dbg[37]    ;
    gTestDataReceive.TestData38      =    dbg[38]    ;
    gTestDataReceive.TestData39      =    dbg[39]    ;

}

void ParSendTune(void)
{
    gParaIdToFunctionDataBuff[0]    =    gMotorExtReg.R1    ;
    gParaIdToFunctionDataBuff[1]    =    gMotorExtReg.R2    ;
    gParaIdToFunctionDataBuff[2]    =    gMotorExtReg.L0    ;
    gParaIdToFunctionDataBuff[3]    =    gMotorExtReg.LM    ;
    gParaIdToFunctionDataBuff[4]    =    gMotorExtReg.I0    ;

    gParaIdToFunctionDataBuff[5]    =    gMotorExtReg.RsPm ;
    gParaIdToFunctionDataBuff[6]    =    gMotorExtReg.LD    ;
    gParaIdToFunctionDataBuff[7]    =    gMotorExtReg.LQ    ;
    gParaIdToFunctionDataBuff[8]    =    uReservedData    ;
    gParaIdToFunctionDataBuff[9]    =    gEstBemf.BemfVolt    ;

    gParaIdToFunctionDataBuff[10]    =    gPmParEst.IdKp    ;
    gParaIdToFunctionDataBuff[11]    =    gPmParEst.IdKi    ;
    gParaIdToFunctionDataBuff[12]    =    gPmParEst.IqKp    ;
    gParaIdToFunctionDataBuff[13]    =    gPmParEst.IqKi    ;
    gParaIdToFunctionDataBuff[14]    =    gPMInitPos.PWMTSet    ;

    gParaIdToFunctionDataBuff[15]    =    gPGData.PGDir    ;
    gParaIdToFunctionDataBuff[16]    =    gPmParEst.CoderPos_deg    ;
    gParaIdToFunctionDataBuff[17]    =    gUVCoff.UDivVSave    ;
    gParaIdToFunctionDataBuff[18]    =    gPmParEst.UvwDir    ;
    gParaIdToFunctionDataBuff[19]    =    gPmParEst.UvwZeroAng_deg    ;
}
