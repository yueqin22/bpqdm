
#include "MotorInclude.h"

IPM_ZERO_POS_STRUCT     gIPMZero;
IPM_INITPOS_PULSE_STR   gIPMInitPos;
PMSM_EST_BEMF           gEstBemf;
PMSM_EST_PARAM_DATA     gPmParEst;

void InitSetPosTune(void);
void DetectZeroPosOnce(void);
void SynInitPosDetSetTs(void);
void SynCalLabAndLbc(void);
void SynInitPosDetCal(void);

static Uint QepCountFromPos(Uint Pos)
{
    Uint m_Pos, m_Cnt;

    m_Pos = Pos;
    m_Cnt = ((Ulong)(m_Pos/gMotorExtInfo.Poles) *(Ulong)gPGData.PulseNum + (2<<13))>>14;

    return m_Cnt;
}

void InitSetPosTune(void)
{
    ResetParForVC();

    gPGDir.CDDirCnt = 0;
    gPGDir.ABDirCnt = 0;
    gPGDir.UVWDirCnt = 0;

    gPGDir.ABAngleBak  = gIPMZero.FeedPos;
    gPGDir.UVWAngleBak = gUVWPG.UVWAngle;

    gIPMZero.DetectCnt = 0;
    gIPMZero.TotalErr  = 0;
    gIPMZero.Flag      = 0;

    gMainStatus.PrgStatus.all = 0;
}

void SynTuneInitPos(void)
{
    if(gGetParVarable.IdSubStep == 1)
    {
        gIPMInitPos.Waite = 0;

        gIPMInitPos.InitPWMTs = (50 * DSP_CLOCK);
        gGetParVarable.IdSubStep++;
    }
    else if(gGetParVarable.IdSubStep == 2)
    {
        gIPMInitPos.Waite ++;
        if(gIPMInitPos.Waite > 10)
        {
            gIPMInitPos.Waite = 0;
            gIPMInitPos.Step = 1;
            gMainStatus.PrgStatus.bit.PWMDisable = 1;
            gGetParVarable.IdSubStep++;
        }
    }
    else if(gGetParVarable.IdSubStep == 3)
    {
        if(gIPMInitPos.Step == 0)
        {
            SetIPMPos((Uint)gIPMPos.InitPos);
            SetIPMPos_ABZRef((Uint)gIPMPos.InitPos);

            gGetParVarable.IdSubStep++;
        }
    }
    else if(gGetParVarable.IdSubStep == 4)
    {
        DisableDrive();
        SynCalLdAndLq(gIPMPos.RotorPos);
        IPMCalAcrPIDCoff();
        gGetParVarable.IdSubStep++;
        gIPMInitPos.Waite = 0;
    }
    else if(gGetParVarable.IdSubStep == 5)
    {
        if(gIPMInitPos.Waite < 500)
        {
            gIPMInitPos.Waite ++;
        }
        else
        {
            InitSetPWM();
            gGetParVarable.IdSubStep = 1;
            gGetParVarable.ParEstMstep++;

            gMainStatus.PrgStatus.all = 0;
        }
    }
}

void SynInitPosDetect(void)
{
    int  m_Cur;
    int  m_Index;

    if(gIPMInitPos.Step == 1)
    {
        gIPMInitPos.PeriodCnt = 0;
        gIPMInitPos.Section   = 0;
        gIPMInitPos.PWMTs     = gIPMInitPos.InitPWMTs;
        gIPMInitPos.CurFirst  = 0;

        gIPMInitPos.CurLimit = 5792;
        if(gInvInfo.InvCurrent < gMotorInfo.Current)
        {
            Ulong temp;
            temp = (Ulong)gIPMInitPos.CurLimit * gInvInfo.InvCurrent;
            gIPMInitPos.CurLimit = temp / gMotorInfo.Current;
        }

        SynInitPosDetSetTs();
        SynInitPosDetSetPwm(7);

        gIPMInitPos.Step ++;
    }
    else if(gIPMInitPos.Step == 2)
    {
        EnableDrive();
        gIPMInitPos.PeriodCnt++;
        if(gIPMInitPos.PeriodCnt >= 6)
        {
            gIPMInitPos.PeriodCnt = 0;
        }

        if(gIPMInitPos.PeriodCnt == 0)
        {
            SynInitPosDetSetPwm(7);
            gIPMInitPos.Section = (gIPMInitPos.Section + 1) & 0x01;

            m_Cur = abs(gIUVWQ24.U >> 12);
            if(m_Cur < abs(gIPMInitPos.CurFirst))
            {
                m_Cur = abs(gIPMInitPos.CurFirst);
            }

                    #if 0
                    if(gIPMInitPos.PWMTs >= 60000)
                    {
                        if(m_Cur < (gIPMInitPos.CurLimit>>2))
                        {

                            gError.ErrorCode.all |= ERROR_OUTPUT_LACK_PHASE;
                            gError.ErrorInfo[1].bit.Fault1 = 10;
                        }
                        else
                        {
                            gError.ErrorCode.all |= ERROR_INIT_POS;
                            gError.ErrorInfo[3].bit.Fault3 = 5;
                        }
                    }
                    #else
                    if((gIPMInitPos.PWMTs >= 65000) && (m_Cur < (gIPMInitPos.CurLimit>>2)))
                    {
                        gError.ErrorCode.all |= ERROR_INIT_POS;
                        gError.ErrorInfo[3].bit.Fault3 = 1;
                    }
                    #endif

            if((m_Cur > gIPMInitPos.CurLimit) || (gIPMInitPos.PWMTs >= 60000))
            {
                gIPMInitPos.Section     = 0;
                gIPMInitPos.PeriodCnt   = 0;
                gIPMInitPos.InitPWMTs = gIPMInitPos.PWMTs;
                gIPMInitPos.Step = 3;
            }
            else
            {
                if(m_Cur > (gIPMInitPos.CurLimit >> 1))
                {
                    gIPMInitPos.PWMTs += (2 * DSP_CLOCK);
                }
                else
                {
                    gIPMInitPos.PWMTs += (gMotorExtInfo.Poles *DSP_CLOCK *2);
                }

                SynInitPosDetSetTs();
            }
        }
        else if(gIPMInitPos.PeriodCnt == 3)
        {
            SynInitPosDetSetPwm(7);
            gIPMInitPos.Section = (gIPMInitPos.Section + 1) & 0x01;

            gIPMInitPos.CurFirst = gIUVWQ24.U >> 12;
        }
        else if((gIPMInitPos.PeriodCnt == 1) || (gIPMInitPos.PeriodCnt == 4))
        {
            SynInitPosDetSetPwm(gIPMInitPos.Section);
        }
    }
    else if((gIPMInitPos.Step == 3) || (gIPMInitPos.Step == 4))
    {
        gIPMInitPos.PeriodCnt++;
        if(gIPMInitPos.PeriodCnt >= 3)
        {
            gIPMInitPos.PeriodCnt = 0;
        }

        if(gIPMInitPos.Section <= 1)
        {
            m_Cur  = gIUVWQ24.U >> 12;
        }
        else if(gIPMInitPos.Section <= 3)
        {
            m_Cur  = gIUVWQ24.V >> 12;
        }
        else
        {
            m_Cur  = gIUVWQ24.W >> 12;
        }

        if(gIPMInitPos.PeriodCnt == 0)
        {
            SynInitPosDetSetPwm(7);
            m_Cur = m_Cur - gIPMInitPos.CurFirst;
            m_Index = (gIPMInitPos.Step - 3) * 6 + gIPMInitPos.Section;
            gIPMInitPos.Cur[m_Index] = abs(m_Cur);

            gIPMInitPos.Section++;
            if(gIPMInitPos.Section == 6)
            {
                gIPMInitPos.Section = 0;
                gIPMInitPos.Step++;
            }
        }
        else if(gIPMInitPos.PeriodCnt == 1)
        {
            SynInitPosDetSetPwm(gIPMInitPos.Section);
        }
        else if(gIPMInitPos.PeriodCnt == 2)
        {
            gIPMInitPos.CurFirst = m_Cur;
        }
    }
    else if(gIPMInitPos.Step == 5)
    {
        DisableDrive();
        SynInitPosDetSetPwm(6);

        SynCalLabAndLbc();
        SynInitPosDetCal();

        if(gMainStatus.RunStep == STATUS_GET_PAR)
        {
            gIPMInitPos.Step ++;
        }
        else
        {
            gIPMInitPos.Step = 0;
            DisableDrive();
        }
    }
    else if(gIPMInitPos.Step == 6)
    {
        SynInitPosDetSetTs();
        SynInitPosDetSetPwm(7);
        EnableDrive();

        gIPMInitPos.Step ++;
        gIPMInitPos.PhsChkStep = 0;
    }
    else if(gIPMInitPos.Step == 7)
    {
        if(gIPMInitPos.PhsChkStep == 0)
        {
            SynInitPosDetSetPwm(0);
            gIPMInitPos.PhsChkStep ++;
        }
        else if((gIPMInitPos.PhsChkStep == 1) || (gIPMInitPos.PhsChkStep == 4))
        {
            gIPMInitPos.PhsChkStep ++;
        }
        else if(gIPMInitPos.PhsChkStep == 2)
        {
            SynInitPosDetSetPwm(7);
            if(abs(gIUVWQ24.U>>12) < (gIPMInitPos.CurLimit>>3))
            {
                DisableDrive();
                gError.ErrorCode.all |= ERROR_OUTPUT_LACK_PHASE;
                gError.ErrorInfo[1].bit.Fault1 = 10;
                gIPMInitPos.Step ++;
            }
            if(abs(gIUVWQ24.V>>12) < (gIPMInitPos.CurLimit>>3))
            {
                DisableDrive();
                gError.ErrorCode.all |= ERROR_OUTPUT_LACK_PHASE;
                gError.ErrorInfo[1].bit.Fault1 = 11;
                gIPMInitPos.Step ++;
            }
            gIPMInitPos.PhsChkStep ++;
        }
        else if(gIPMInitPos.PhsChkStep == 3)
        {
            SynInitPosDetSetPwm(2);
            gIPMInitPos.PhsChkStep ++;
        }
        else if(gIPMInitPos.PhsChkStep == 5)
        {
            SynInitPosDetSetPwm(7);
            if(abs(gIUVWQ24.W>>12) < (gIPMInitPos.CurLimit>>3))
            {
                DisableDrive();
                gError.ErrorCode.all |= ERROR_OUTPUT_LACK_PHASE;
                gError.ErrorInfo[1].bit.Fault1 = 12;
                gIPMInitPos.Step ++;
            }
            if(abs(gIUVWQ24.V>>12) < (gIPMInitPos.CurLimit>>3))
            {
                DisableDrive();
                gError.ErrorCode.all |= ERROR_OUTPUT_LACK_PHASE;
                gError.ErrorInfo[1].bit.Fault1 = 11;
                gIPMInitPos.Step ++;
            }
            gIPMInitPos.PhsChkStep ++;
        }
        else
        {
            DisableDrive();
            SynInitPosDetSetPwm(6);
            gIPMInitPos.Step ++;
        }
    }
    else
    {
        gIPMInitPos.Step = 0;
        DisableDrive();
    }
}

void SetIPMPos(Uint Pos)
{
    Uint m_Cnt;

    m_Cnt = QepCountFromPos(Pos);

    gIPMPos.RotorPos = Pos;
    DINT;
    gIPMPos.QepTotal = m_Cnt;
    gIPMPos.QepBak   = GetQepCnt();
    EINT;
}

void SetIPMPos_ABZRef(Uint Pos)
{
    Uint m_Cnt;

    m_Cnt = QepCountFromPos(Pos);

    gIPMPos.ABZ_RotorPos_Ref = Pos;
    DINT;
    gIPMPos.ABZ_QepTotal = m_Cnt;
    gIPMPos.ABZ_QepBak   = GetQepCnt();
    EINT;
}

void SynCalLabAndLbc(void)
{
    Uint  m_Index,m_Sel;
    Uint  m_DetaI,m_Cur1,m_Cur2,m_UDC;
    Ulong m_LTemp;

    for(m_Index = 0;m_Index < 3;m_Index++)
    {
        m_Sel = m_Index<<1;

        if(gIPMInitPos.Cur[m_Sel] <= gIPMInitPos.Cur[m_Sel+1])
        {
            m_Cur1 = gIPMInitPos.Cur[m_Sel];
        }
        else
        {
            m_Cur1 = gIPMInitPos.Cur[m_Sel+1];
        }

        if(gIPMInitPos.Cur[m_Sel+6] <= gIPMInitPos.Cur[m_Sel+7])
        {
            m_Cur2 = gIPMInitPos.Cur[m_Sel+6];
        }
        else
        {
            m_Cur2 = gIPMInitPos.Cur[m_Sel+7];
        }

        m_DetaI = (m_Cur1>>1) + (m_Cur2>>1);

        m_DetaI = ((Ulong)m_DetaI * (Ulong)gMotorInfo.Current) >> 12;
        m_UDC   = gUDC.uDCFilter - (Ulong)m_DetaI * (Ulong)gMotorExtReg.RsPm / 10000;
        m_LTemp = (Ulong)gIPMInitPos.PWMTs*2L * 100L/DSP_CLOCK;
        m_LTemp = (Ulong)m_UDC * (Ulong)m_LTemp;

        gIPMInitPos.LPhase[m_Index] = (m_LTemp)/((Ulong)m_DetaI * 100);
    }
}

void SynInitPosDetCal(void)
{
    static int  m_X,m_Y;
    Uint  m_Index;

    for(m_Index = 0;m_Index<12; m_Index = m_Index + 2)
    {
        gIPMInitPos.Cur[m_Index] = gIPMInitPos.Cur[m_Index+1] - gIPMInitPos.Cur[m_Index];
    }

    for(m_Index = 0;m_Index<6; m_Index = m_Index + 2)
    {
        gIPMInitPos.Cur[m_Index] = (gIPMInitPos.Cur[m_Index] + gIPMInitPos.Cur[m_Index+6]);
    }

    m_X = gIPMInitPos.Cur[0] - (gIPMInitPos.Cur[2]>>1) - (gIPMInitPos.Cur[4]>>1);
    m_Y = ((long)(gIPMInitPos.Cur[2] - gIPMInitPos.Cur[4]) * 28378L)>>15;
    if(abs(m_X) + abs(m_Y) < (4096/40))
    {
        gError.ErrorCode.all |= ERROR_INIT_POS;
        gError.ErrorInfo[3].bit.Fault3 = 2;
    }

    gIPMPos.InitPos = (Uint)atan(m_X, m_Y) - 5461;
    gIPMPos.InitAngle_deg = (Ulong)gIPMPos.InitPos * 3600 >> 16;
}

void SynInitPosDetSetTs(void)
{
    EALLOW;
    EPwm1Regs.TBPRD = gIPMInitPos.PWMTs;
    EPwm1Regs.CMPB  = gIPMInitPos.PWMTs;
    EPwm2Regs.TBPRD = gIPMInitPos.PWMTs;
    EPwm2Regs.CMPB  = gADC.DelayApply;
    EPwm3Regs.TBPRD = gIPMInitPos.PWMTs;
    EDIS;
}

void SynInitPosDetSetPwm(Uint Section)
{
    EALLOW;
    if(Section == 0)
    {
        EPwm1Regs.AQCSFRC.all = 0x08;
        EPwm2Regs.AQCSFRC.all = 0x02;
        EPwm3Regs.AQCSFRC.all = 0x0A;
    }
    else if(Section == 1)
    {
        EPwm1Regs.AQCSFRC.all = 0x02;
        EPwm2Regs.AQCSFRC.all = 0x08;
        EPwm3Regs.AQCSFRC.all = 0x0A;
    }
    else if(Section == 2)
    {
        EPwm1Regs.AQCSFRC.all = 0x0A;
        EPwm2Regs.AQCSFRC.all = 0x08;
        EPwm3Regs.AQCSFRC.all = 0x02;
    }
    else if(Section == 3)
    {
        EPwm1Regs.AQCSFRC.all = 0x0A;
        EPwm2Regs.AQCSFRC.all = 0x02;
        EPwm3Regs.AQCSFRC.all = 0x08;
    }
    else if(Section == 4)
    {
        EPwm1Regs.AQCSFRC.all = 0x02;
        EPwm2Regs.AQCSFRC.all = 0x0A;
        EPwm3Regs.AQCSFRC.all = 0x08;
    }
    else if(Section == 5)
    {
        EPwm1Regs.AQCSFRC.all = 0x08;
        EPwm2Regs.AQCSFRC.all = 0x0A;
        EPwm3Regs.AQCSFRC.all = 0x02;
    }
    else if(Section == 6)
    {
        EPwm1Regs.DBCTL.all   = 0x0007;
        EPwm1Regs.AQCTLA.all  = 0x0090;
        EPwm1Regs.AQCTLB.all  = 0x00;
        EPwm1Regs.AQCSFRC.all = 0x00;

        EPwm2Regs.DBCTL.all   = 0x0007;
        EPwm2Regs.AQCTLA.all  = 0x0090;
        EPwm2Regs.AQCTLB.all  = 0x00;
        EPwm2Regs.AQCSFRC.all = 0x00;

        EPwm3Regs.DBCTL.all   = 0x0007;
        EPwm3Regs.AQCTLA.all  = 0x0090;
        EPwm3Regs.AQCTLB.all  = 0x00;
        EPwm3Regs.AQCSFRC.all = 0x00;
    }
    else
    {
        EPwm1Regs.AQCSFRC.all = 0x0A;
        EPwm1Regs.DBCTL.all   = 0;
        EPwm1Regs.AQCTLA.all  = 0x000C;
        EPwm1Regs.AQCTLB.all  = 0x000C;

        EPwm2Regs.AQCSFRC.all = 0x0A;
        EPwm2Regs.DBCTL.all   = 0;
        EPwm2Regs.AQCTLA.all  = 0x000C;
        EPwm2Regs.AQCTLB.all  = 0x000C;

        EPwm3Regs.AQCSFRC.all = 0x0A;
        EPwm3Regs.DBCTL.all   = 0;
        EPwm3Regs.AQCTLA.all  = 0x000C;
        EPwm3Regs.AQCTLB.all  = 0x000C;
    }
    EDIS;
}

void DetectZeroPosOnce(void)
{
    Uint m_Data;

    if(gPGData.PGMode == 0)
    {
        m_Data = gIPMPos.RotorPos - gIPMZero.FeedPos;
    }
    else
    {
        m_Data = gIPMPos.RotorPos - gRotorTrans.RTPos;
    }

    if(gIPMZero.DetectCnt != 2000)
    {
        gIPMZero.TotalErr += ((long)m_Data - (long)gIPMZero.FirstPos);
    }
    else
    {
        gIPMZero.FirstPos = m_Data;
    }
}

void SynTunePGZero_No_Load(void)
{
    int  m_Dir1, m_Dir2, m_Dir3;

    gCtrMotorType = RUN_SYNC_TUNE;

    if(gGetParVarable.IdSubStep == 1)
    {
        if(gIPMZero.DetectCnt == 0)
        {
            InitSetPosTune();

            gPGData.PGDir = 0;
            gPmParEst.UvwDir = 0;
            EALLOW;
            EQepRegs->QDECCTL.all = 0;
            EDIS;

            gIPMZero.DetectCnt ++;
            gIMTSet.M = 0L <<12;
            gIMTSet.T = 0;
            gIPMZero.CurLimit = 4096UL * (Ulong)gMotorInfo.CurrentGet / gMotorInfo.Current;
            if(gInvInfo.InvCurrent < gMotorInfo.Current)
            {
                Ulong temp;
                temp = (Ulong)gIPMZero.CurLimit * gInvInfo.InvCurrent;
                gIPMZero.CurLimit = temp / gMotorInfo.Current;
            }
        }
        else
        {
            if(gIPMZero.DetectCnt == 1)
            {
                gIPMZero.DetectCnt ++;
                EnableDrive();
            }

            if(gIMTSet.M < ((long)gIPMZero.CurLimit <<12))
            {
                gIMTSet.M += (50L << 12);
            }
            else
            {
                gIMTSet.M = (long)gIPMZero.CurLimit << 12;
                gIPMZero.DetectCnt = 0;

                gPGDir.ABDirCnt = 0;
                gPGDir.UVWDirCnt = 0;
                gPGDir.RtDirCnt = 0;

                gGetParVarable.IdSubStep++;
            }
        }
    }
    else if(gGetParVarable.IdSubStep == 2)
    {
        gIPMPos.RotorPos += (gMotorExtInfo.Poles * 11);
        gIPMZero.DetectCnt++;

        m_Dir1 = JudgeABDir();
        m_Dir2 = JudgeUVWDir();
        m_Dir3 = JudgeRTDir();

        if(gIPMZero.DetectCnt > 6000)
        {
            if(gPGData.PGMode == 0)
            {
                if(m_Dir1 == DIR_ERROR)
                {
                    gError.ErrorCode.all |= ERROR_ENCODER;
                    gError.ErrorInfo[4].bit.Fault1 = 1;
                }
                else if(gPGData.PGType == PG_TYPE_UVW)
                {
                    if(m_Dir2 == DIR_ERROR)
                    {
                        gError.ErrorCode.all |= ERROR_ENCODER;
                        gError.ErrorInfo[4].bit.Fault1 = 2;
                    }
                    if(m_Dir2 == DIR_BACKWARD)
                    {
                        gPmParEst.UvwDir = ((gPmParEst.UvwDir+1)&0x01) ;
                    }
                }

                if(m_Dir1 == DIR_BACKWARD)
                {
                    EALLOW;
                    gPGData.PGDir = 1;
                    EQepRegs->QDECCTL.all = 0x0400;
                    EDIS;
                }
                gGetParVarable.IdSubStep = 3;
            }
            else
            {
                if(m_Dir3 == DIR_ERROR)
                {
                    gError.ErrorCode.all |= ERROR_ENCODER;
                    gError.ErrorInfo[4].bit.Fault1 = 3;
                }
                else if(m_Dir3 == DIR_BACKWARD)
                {
                    gPGData.PGDir = 1;
                }
                gGetParVarable.IdSubStep = 4;
            }

            gIPMZero.DetectCnt = 0;
            gIPMZero.Flag = 0;
        }
    }
    else if(gGetParVarable.IdSubStep == 3)
    {
        gIPMPos.RotorPos += (gMotorExtInfo.Poles * 22);
        gIPMZero.DetectCnt++;
        if(gIPMZero.DetectCnt > 6000)
        {
            gError.ErrorCode.all |= ERROR_ENCODER;
            gError.ErrorInfo[4].bit.Fault1 = 4;
        }
        if(1 == (gIPMZero.Flag & 0x01))
        {
            gUVWPG.TotalErr = 0;
            gIPMZero.DetectCnt = 0;
            gGetParVarable.IdSubStep++;
            gUVWPG.TuneFlag = 0;
        }
    }
    else if(gGetParVarable.IdSubStep == 4)
    {
        gIPMPos.RotorPos += (gMotorExtInfo.Poles * 6);

        gIPMZero.DetectCnt++;
        if(gIPMZero.DetectCnt >= 2000)
        {
            DetectZeroPosOnce();
        }

        if(gPGData.PGType == PG_TYPE_UVW && gIPMZero.DetectCnt < 5461)
        {
            gUVWPG.lastAgl = gUVWPG.NewAgl;
            gUVWPG.NewAgl = gUVWPG.UVWAngle;
            if(((gUVWPG.lastAgl >= 60072)&&(gUVWPG.lastAgl <= 60074))
                &&(( gUVWPG.NewAgl >=5460)&&(gUVWPG.NewAgl <=5462))
               )
            {
                gUVWPG.ErrAgl = gIPMPos.RotorPos;
                gUVWPG.TuneFlag = 1;
            }
        }
        else if(gIPMZero.DetectCnt == 5461)
        {
            gPmParEst.UvwZeroAng = gUVWPG.ErrAgl;
            gPmParEst.UvwZeroAng_deg = ((Ulong)gPmParEst.UvwZeroAng * 3600L +10) >>16;
        }

        if(gIPMZero.DetectCnt > 6096)
        {
            gIPMPos.RotorZero = gIPMZero.FirstPos + (int)(gIPMZero.TotalErr>>12);
            gPmParEst.CoderPos_deg = ((Ulong)gIPMPos.RotorZero * 3600L + 10)>>16;
            gIPMPos.RotorZeroGet = gPmParEst.CoderPos_deg;

            gIPMZero.DetectCnt = 0;
            gGetParVarable.IdSubStep ++;
        }
    }
    else if(gGetParVarable.IdSubStep == 5)
    {
        if(gIPMZero.DetectCnt < 500)
        {
            gIPMZero.DetectCnt++;
        }
        else
        {
            if(gPGData.PGMode == 0)
            {
                SetIPMPos(gIPMPos.RotorPos);
                SetIPMPos_ABZRef(gIPMPos.RotorPos);
            }
            gIPMZero.DetectCnt = 0;
            gGetParVarable.IdSubStep ++;
        }
    }
    else if(gGetParVarable.IdSubStep == 6)
    {
        DisableDrive();
        if(gIPMZero.DetectCnt < 500)
        {
            gIPMZero.DetectCnt ++;
        }
        else
        {
            gIPMInitPos.Flag = 0;
            gMainStatus.PrgStatus.all = 0;
            gGetParVarable.IdSubStep ++;
        }
    }
    else
    {
        gGetParVarable.IdSubStep = 1;
        gGetParVarable.ParEstMstep++;
    }
}

void SynTunePGZero_Load(void)
{
    int  m_Dir1,m_Dir2, m_Dir3;

    gCtrMotorType = RUN_SYNC_TUNE;

    if(gGetParVarable.IdSubStep == 1)
    {
        InitSetPosTune();

        if(gPGData.PGMode != 0)
        {
            gPmParEst.EstZero = gIPMPos.InitPos - gRotorTrans.RTPos;
            gPmParEst.CoderPos_deg = ((Ulong)gPmParEst.EstZero * 3600L + 10)>>16;
        }

        gGetParVarable.IdSubStep ++;
    }
    else if(gGetParVarable.IdSubStep == 2)
    {
        gGetParVarable.StatusWord = TUNE_ACC;
        gIPMZero.DetectCnt = 0;

        EnableDrive();
        gGetParVarable.IdSubStep++;
        gIPMZero.time = 0 ;
    }
    else if(gGetParVarable.IdSubStep == 3)
    {
        gIPMZero.time ++ ;
        if(gIPMZero.time >= 6000)
        {
            gError.ErrorCode.all |= ERROR_ENCODER;
            gError.ErrorInfo[4].bit.Fault1 = 5;
        }
        if(speed_CON && 1 == (gIPMZero.Flag & 0x01))
        {
            gIPMZero.DetectCnt++;
            if(gIPMZero.DetectCnt > 500)
            {
                gIPMZero.DetectCnt = 0;
                gPGDir.ABDirCnt = 0;
                gPGDir.UVWDirCnt = 0;
                gPGDir.RtDirCnt = 0;

                gUVWPG.TotalErr = 0;
                gUVWPG.UvwCnt = 0;
                gUVWPG.UvwEstStep = 0;

                gIPMZero.Flag &= 0xFFFE;
                gGetParVarable.IdSubStep++;
            }
        }
    }
    else if(gGetParVarable.IdSubStep == 4)
    {
        if(gPGData.PGMode == 0)
        {
            m_Dir1 = JudgeABDir();
            m_Dir2 = JudgeUVWDir();

            if(gPGData.PGType == PG_TYPE_UVW && abs(gPGDir.UVWDirCnt) > 2)
            {
                if(m_Dir2 == DIR_BACKWARD)
                {
                    gPmParEst.UvwDir = (gPmParEst.UvwDir+1)&0x01;
                }
                else if(m_Dir2 == DIR_ERROR)
                {
                    gError.ErrorCode.all |= ERROR_ENCODER;
                    gError.ErrorInfo[4].bit.Fault1 = 6;
                }
            }

            if(abs(gPGDir.ABDirCnt) > 3)
            {
                if((gPGData.PGType != PG_TYPE_UVW) ||
                   (abs(gPGDir.UVWDirCnt)> 3))
                {
                    gGetParVarable.IdSubStep++;
                }
            }
            gUVWPG.lastAgl = 0;
            gUVWPG.NewAgl = 0;
            gUVWPG.TuneFlag = 0;
        }
        else if(gPGData.PGMode == 1)
        {
            m_Dir3 = JudgeRTDir();
            if(abs(gPGDir.RtDirCnt) > 5)
            {
                gIPMZero.DetectCnt = 0;
                gGetParVarable.IdSubStep++;
            }
        }
    }
    else if(gGetParVarable.IdSubStep == 5)
    {
        if(gPGData.PGType == PG_TYPE_UVW)
        {
            if(gUVWPG.UvwEstStep == 0)
            {
                gUVWPG.UvwEstStep ++;
            }
            else if(gUVWPG.UvwEstStep == 2)
            {
                GetUvwPhase();
                gUVWPG.lastAgl = gUVWPG.NewAgl;
                gUVWPG.NewAgl = gUVWPG.UVWAngle;
                if(((gUVWPG.lastAgl >= 60072)&&(gUVWPG.lastAgl <= 60074))
                    &&(( gUVWPG.NewAgl >=5460)&&(gUVWPG.NewAgl <=5462))
                   )
                {
                    gUVWPG.ErrAgl = gIPMPos.RotorPos;
                    gUVWPG.TuneFlag = 1;
                }
                else if(((gUVWPG.lastAgl >= 5460)&&(gUVWPG.lastAgl <= 5462))
                          &&(( gUVWPG.NewAgl >=60072)&&(gUVWPG.NewAgl <=60074))
                    )
                {
                    gUVWPG.ErrAgl = gIPMPos.RotorPos;
                    gUVWPG.TuneFlag = 1;
                }
            }
            else if(gUVWPG.UvwEstStep == 3)
            {
                gPmParEst.UvwZeroAng = gUVWPG.ErrAgl;
                gPmParEst.UvwZeroAng_deg = ((Ulong)gPmParEst.UvwZeroAng * 3600L +10) >>16;
                gUVWPG.UvwEstStep ++;
            }
        }

        if(1 == (gIPMZero.Flag & 0x01))
        {
            gIPMZero.Flag &= 0xFFFE;
            if(gIPMZero.DetectCnt == 0)
            {
                gIPMZero.FirstPos = gIPMPos.PosInZInfo;
                gIPMZero.TotalErr = 0;
            }
            else if(gIPMZero.DetectCnt < 5)
            {
                gIPMZero.TotalErr += (int)(gIPMPos.PosInZInfo - gIPMZero.FirstPos);
            }
            else if(gIPMZero.DetectCnt == 5)
            {
                gIPMPos.RotorZero = gIPMZero.FirstPos + (int)(gIPMZero.TotalErr >>4);
                gPmParEst.CoderPos_deg = ((Ulong)gIPMPos.RotorZero * 3600L + 10)>>16;
                gIPMPos.RotorZeroGet = gPmParEst.CoderPos_deg;
            }
            gIPMZero.DetectCnt++;
        }

        if(gIPMZero.DetectCnt > 5)
        {
            if((gPGData.PGType != PG_TYPE_UVW) ||
                (gPGData.PGType == PG_TYPE_UVW && gUVWPG.UvwEstStep >=4))
            {
                gGetParVarable.StatusWord = TUNE_DEC;
                gGetParVarable.IdSubStep++;
            }
        }
    }
    else if(gGetParVarable.IdSubStep == 6)
    {
        if(abs(gMainCmd.FreqSet) <= 10)
        {
            gIPMZero.DetectCnt++;
            if(gIPMZero.DetectCnt > 250)
            {
                gIPMZero.DetectCnt = 0;
                DisableDrive();

                gGetParVarable.IdSubStep++;
            }
        }
    }
    else if(gGetParVarable.IdSubStep == 7)
    {
        if(gIPMZero.DetectCnt < 500)
        {
            gIPMZero.DetectCnt ++;
        }
        else
        {
            gIPMInitPos.Flag = 0;
            SetIPMPos(gIPMPos.RotorPos);
            SetIPMPos_ABZRef(gIPMPos.RotorPos);

            gMainStatus.PrgStatus.all = 0;
            gGetParVarable.ParEstMstep++;
            gGetParVarable.IdSubStep = 1;
        }
    }
}

void SynTuneBemf()
{
    Ulong temp1, temp2, temp3;
    Ulong fluxRotor;

    gCtrMotorType = RUN_SYNC_TUNE;

    if(gGetParVarable.IdSubStep == 1)
    {
        gEstBemf.TotalVq2 = 0;
        gEstBemf.TotalVq1 = 0;
        gEstBemf.TotalId2 = 0;
        gEstBemf.TotalId1 = 0;

        gEstBemf.IdSet = 1500;
        gEstBemf.IqSet = 0;
        gEstBemf.IdSetFilt = 0;
        if(gInvInfo.InvCurrent < gMotorInfo.Current)
        {
            temp1 = (Ulong)gEstBemf.IdSet * gInvInfo.InvCurrent;
            gEstBemf.IdSet = temp1 / gMotorInfo.Current;
        }
        ResetParForVC();

        gEstBemf.TuneFreqAim = (long)gMotorInfo.FreqPer * 2L / 5L;
        gEstBemf.FreqRem = 0;
        gEstBemf.AccDecTick = (gInvInfo.InvTypeApply <= 20) ? (30L*1000L/2) : (60L*1000L/2);

        gEstBemf.TuneFreqSet = 0;

        gGetParVarable.IdSubStep ++;
    }
    else if(gGetParVarable.IdSubStep == 2)
    {
        EnableDrive();

        gEstBemf.FreqStep = ((long)gMotorInfo.FreqPer + gEstBemf.FreqRem) / gEstBemf.AccDecTick;
        gEstBemf.FreqRem = ((long)gMotorInfo.FreqPer + gEstBemf.FreqRem) % gEstBemf.AccDecTick;

        if(gEstBemf.TuneFreqSet < gEstBemf.TuneFreqAim)
        {
            gEstBemf.TuneFreqSet += gEstBemf.FreqStep;
        }
        else
        {
            gEstBemf.TuneFreqSet = gEstBemf.TuneFreqAim;
            gEstBemf.Cnt ++;
        }
        if(gEstBemf.Cnt > 500)
        {
            gEstBemf.Cnt = 0;
            gGetParVarable.IdSubStep ++;
        }
    }
    else if(gGetParVarable.IdSubStep == 3)
    {
        gEstBemf.Cnt ++;
        if(gEstBemf.Cnt <= 2048)
        {
            gEstBemf.TotalId1 += gLineCur.CurPer;
            gEstBemf.TotalVq1 += gOutVolt.VoltApply;
        }
        else
        {
            gEstBemf.TotalId1 = gEstBemf.TotalId1 >> 11;
            gEstBemf.TotalVq1 = gEstBemf.TotalVq1 >> 11;

            gEstBemf.TuneFreq = abs(gMainCmd.FreqSyn);
            gEstBemf.Cnt = 0;
            gGetParVarable.IdSubStep ++;
        }
    }
    else if(gGetParVarable.IdSubStep == 4)
    {
        gEstBemf.IdSet = 3000;
        if(gInvInfo.InvCurrent < gMotorInfo.Current)
        {
            temp1 = (Ulong)gEstBemf.IdSet * gInvInfo.InvCurrent;
            gEstBemf.IdSet = temp1 / gMotorInfo.Current;
        }
        gEstBemf.Cnt ++;
        if(gEstBemf.Cnt > 500)
        {
            gEstBemf.Cnt = 0;
            gGetParVarable.IdSubStep ++;
        }
    }
    else if(gGetParVarable.IdSubStep == 5)
    {
        gEstBemf.Cnt ++;
        if(gEstBemf.Cnt <= 2048)
        {
            gEstBemf.TotalId2 += gLineCur.CurPer;
            gEstBemf.TotalVq2 += gOutVolt.VoltApply;
        }
        else
        {
            gEstBemf.TotalId2 = gEstBemf.TotalId2 >> 11;
            gEstBemf.TotalVq2 = gEstBemf.TotalVq2 >> 11;

            gEstBemf.Cnt = 0;

            gEstBemf.TuneFreqAim = 0;
            gEstBemf.FreqRem = 0;
            gGetParVarable.IdSubStep ++;
        }
    }
    else if(gGetParVarable.IdSubStep == 6)
    {
        gEstBemf.FreqStep = ((long)gMotorInfo.FreqPer + gEstBemf.FreqRem) / gEstBemf.AccDecTick;
        gEstBemf.FreqRem = ((long)gMotorInfo.FreqPer + gEstBemf.FreqRem) % gEstBemf.AccDecTick;

        if(gEstBemf.TuneFreqSet > gEstBemf.TuneFreqAim)
        {
            gEstBemf.TuneFreqSet -= gEstBemf.FreqStep;
        }
        else
        {
            gEstBemf.TuneFreqSet = 0;
            DisableDrive();
            gEstBemf.Cnt  = 0;
            gGetParVarable.IdSubStep ++;
        }
    }
    else if(gGetParVarable.IdSubStep == 7)
    {
        if(gEstBemf.Cnt < 500)
        {
            gEstBemf.Cnt ++;
        }
        else
        {
            gEstBemf.Cnt = 0;
            gGetParVarable.IdSubStep ++;
        }
    }
    else if(gGetParVarable.IdSubStep == 8)
    {
        temp1 = (Ulong)gEstBemf.TotalVq2 * gEstBemf.TotalId1;
        temp2 = (Ulong)gEstBemf.TotalVq1 * gEstBemf.TotalId2;
        temp3 = (gEstBemf.TotalId2 - gEstBemf.TotalId1) * gEstBemf.TuneFreq;
        fluxRotor = (((Ullong)temp2 - (Ullong)temp1) <<15) /temp3;

        temp3 = fluxRotor * abs(gMotorInfo.FreqPer) >> 12;
        gEstBemf.BemfVolt = temp3 * gMotorInfo.Votage * 10L >> 15;

        gMainStatus.PrgStatus.all = 0;
        gGetParVarable.IdSubStep = 1;
        gGetParVarable.ParEstMstep ++;

        SetIPMPos(gIPMPos.RotorPos);
        SetIPMPos_ABZRef(gIPMPos.RotorPos);
    }
}
