
#include "MotorEncoder.h"
#include "MotorPmsmMain.h"
#include "MotorPmsmParEst.h"
#include "MotorVCInclude.h"
#include "SubPrgInclude.h"

struct EQEP_REGS       *EQepRegs;

PG_DATA_STRUCT			gPGData;
IPM_UVW_PG_STRUCT       gUVWPG;
IPM_PG_DIR_STRUCT       gPGDir;
FVC_SPEED_STRUCT		gFVCSpeed;
ROTOR_TRANS_STRUCT		gRotorTrans;
BURR_FILTER_STRUCT		gSpeedFilter;
CUR_LINE_STRUCT_DEF		gSpeedLine;

Uint const gUVWAngleTable[6] = {

    (330*65536L/360), (210*65536L/360),	(270*65536L/360),

    (90 *65536L/360), (30 *65536L/360), (150*65536L/360)
};

void GetSpeedMMethod(void);
void GetSpeedMTMethod(void);
void SpeedSmoothDeal( ROTOR_SPEED_SMOOTH * pSpeedError,int iSpeed );
void RotorTransCalVel_old(void);

void InitRtInterface(void)
{
    EALLOW;
#ifdef TMS320F2808

	SysCtrlRegs.PCLKCR0.bit.SPICENCLK = 1;

    GpioCtrlRegs.GPAPUD.bit.GPIO21  = 0;
    GpioCtrlRegs.GPAMUX2.bit.GPIO21 = 2;
    GpioCtrlRegs.GPAQSEL2.bit.GPIO21= 3;
    GpioCtrlRegs.GPAPUD.bit.GPIO22  = 0;
    GpioCtrlRegs.GPAMUX2.bit.GPIO22 = 2;
    GpioCtrlRegs.GPAQSEL2.bit.GPIO22= 3;
    GpioCtrlRegs.GPAPUD.bit.GPIO23  = 0;
    GpioCtrlRegs.GPAMUX2.bit.GPIO23 = 2;
    GpioCtrlRegs.GPAQSEL2.bit.GPIO23= 3;

    SpicRegs.SPIFFTX.all=0xE040;
    SpicRegs.SPIFFRX.all=0x405f;
    SpicRegs.SPIFFRX.all=0x205f;
    SpicRegs.SPIFFCT.all=0x0000;

    SpicRegs.SPICCR.all =0x000F;

    #if 1
    SpicRegs.SPICCR.bit.CLKPOLARITY = 1;
    SpicRegs.SPICTL.all =0x000E;

    #else
	SpicRegs.SPICCR.bit.CLKPOLARITY = 0;
    SpicRegs.SPICTL.all =0x0006;
    #endif

    SpicRegs.SPIBRR = 48;

    SpicRegs.SPICCR.bit.SPISWRESET = 1;
    SpicRegs.SPIPRI.bit.FREE = 1;

    GpioCtrlRegs.GPBPUD.bit.GPIO34 = 0;
    GpioCtrlRegs.GPBMUX1.bit.GPIO34= 0;
    GpioDataRegs.GPBSET.bit.GPIO34 = 1;
    GpioCtrlRegs.GPBDIR.bit.GPIO34 = 1;

#else

    GpioCtrlRegs.GPAMUX2.bit.GPIO20 = 0;
    GpioCtrlRegs.GPAMUX2.bit.GPIO21 = 0;
    GpioCtrlRegs.GPAMUX2.bit.GPIO23 = 0;
    GpioCtrlRegs.AIOMUX1.bit.AIO10 = 0;
    GpioCtrlRegs.AIOMUX1.bit.AIO12 = 0;

    GpioCtrlRegs.GPADIR.bit.GPIO20 = 1;
    GpioCtrlRegs.GPADIR.bit.GPIO21 = 0;
    GpioCtrlRegs.GPADIR.bit.GPIO23 = 1;
    GpioCtrlRegs.AIODIR.bit.AIO10 = 1;
    GpioCtrlRegs.AIODIR.bit.AIO12 = 1;

    ROTOR_TRANS_RDVEL = 1;
#endif
    EDIS;
}

void InitSetQEP(void)
{
   	EALLOW;
	(*EQepRegs).QEPCTL.bit.QPEN = 0;
	(*EQepRegs).QDECCTL.all = 0;
	(*EQepRegs).QEPCTL.bit.PCRM = 1;
	(*EQepRegs).QEPCTL.bit.QPEN = 1;
	(*EQepRegs).QEPCTL.bit.QCLM = 1;
	(*EQepRegs).QEPCTL.bit.UTE = 1;
	(*EQepRegs).QEPCTL.bit.WDE = 0;

	(*EQepRegs).QPOSCTL.all = 0;
	(*EQepRegs).QCAPCTL.bit.CEN = 0;
	(*EQepRegs).QCAPCTL.bit.CCPS = 2;
	(*EQepRegs).QCAPCTL.bit.UPPS = 2;
	(*EQepRegs).QCAPCTL.bit.CEN = 1;

	(*EQepRegs).QPOSCNT = 0;
	(*EQepRegs).QPOSINIT = 0;
	(*EQepRegs).QPOSMAX = 0xFFFFFFFF;
	(*EQepRegs).QPOSCMP = 0;
	(*EQepRegs).QUTMR = 0;
	(*EQepRegs).QWDTMR = 0;
	(*EQepRegs).QUPRD = C_TIME_05MS;
	(*EQepRegs).QCLR.all = 0x0FFF;

	(*EQepRegs).QCTMR = 0;
	(*EQepRegs).QCPRD = 0;

    (*EQepRegs).QEPCTL.bit.IEL = 1;

	EDIS;
}

void ReInitForPG(void)
{
    switch(gPGData.PGType)
    {
        case PG_TYPE_UVW:
            gPGData.PGMode = 0;
            EALLOW;
            GpioCtrlRegs.GPAMUX2.bit.GPIO20 = 1;
            GpioCtrlRegs.GPAMUX2.bit.GPIO21 = 1;
            GpioCtrlRegs.GPAMUX2.bit.GPIO23 = 1;
        #ifdef TMS320F2808
            GpioCtrlRegs.GPAMUX2.bit.GPIO24 = 2;
            GpioCtrlRegs.GPAMUX2.bit.GPIO25 = 2;
            GpioCtrlRegs.GPAMUX2.bit.GPIO26 = 2;
        #endif
        #ifdef TMS320F28035
            GpioCtrlRegs.AIOMUX1.bit.AIO6  = 2;
            GpioCtrlRegs.AIOMUX1.bit.AIO10 = 2;
            GpioCtrlRegs.AIOMUX1.bit.AIO12 = 2;
        #endif
            EDIS;
            break;

        case PG_TYPE_SPECIAL_UVW:
            gPGData.PGMode = 0;
            break;

        case PG_TYPE_RESOLVER:
            gPGData.PGMode = 1;
            EALLOW;
            SysCtrlRegs.PCLKCR1.bit.EQEP1ENCLK = 0;
        #ifdef TMS320F2808
            SysCtrlRegs.PCLKCR1.bit.EQEP2ENCLK = 0;
        #endif
            EDIS;

            InitRtInterface();
            break;

        case PG_TYPE_SC:
            gPGData.PGMode = 0;
            break;

        default:
            gPGData.PGMode = 0;
            EALLOW;
            GpioCtrlRegs.GPAMUX2.bit.GPIO20 = 1;
            GpioCtrlRegs.GPAMUX2.bit.GPIO21 = 1;
            GpioCtrlRegs.GPAMUX2.bit.GPIO23 = 1;

        #ifdef TMS320F2808
            GpioCtrlRegs.GPAMUX2.bit.GPIO24 = 2;
            GpioCtrlRegs.GPAMUX2.bit.GPIO25 = 2;
            GpioCtrlRegs.GPAMUX2.bit.GPIO26 = 2;
        #endif
            EDIS;
            break;
    }

    if(gPGData.PGMode == 0)
    {
        gPGData.QEPIndex = QEP_SELECT_NONE;
    }
    else
    {
        gPGData.QEPIndex = (QEP_INDEX_ENUM_STRUCT)gExtendCmd.bit.QepIndex;
    }
}

Uint JudgeABDir(void)
{
    int m_Deta;

    m_Deta = gIPMZero.FeedPos - gPGDir.ABAngleBak;
    if(m_Deta > 1820)
    {
        if(gPGDir.ABDirCnt < 32767)
        {
            gPGDir.ABDirCnt ++;
        }
        gPGDir.ABAngleBak = gIPMZero.FeedPos;
    }
    else if(m_Deta < -1820)
    {
        if(gPGDir.ABDirCnt > -32767)
        {
            gPGDir.ABDirCnt --;
        }
        gPGDir.ABAngleBak = gIPMZero.FeedPos;
    }

    if(gPGDir.ABDirCnt > 2)
    {
        return (DIR_FORWARD);
    }
    else if(gPGDir.ABDirCnt < -2)
    {
        return (DIR_BACKWARD);
    }
	else
    {
        return (DIR_ERROR);
    }
}

Uint JudgeUVWDir(void)
{
    int m_Deta;

    m_Deta = (int)gUVWPG.UVWAngle - (int)gPGDir.UVWAngleBak;
    if(m_Deta > 1820)
    {
        if(gPGDir.UVWDirCnt < 32767)
        {
            gPGDir.UVWDirCnt ++;
        }
        gPGDir.UVWAngleBak = gUVWPG.UVWAngle;
    }
    else if(m_Deta < -1820)
    {
        if(gPGDir.UVWDirCnt > -32767)
        {
            gPGDir.UVWDirCnt --;
        }
        gPGDir.UVWAngleBak = gUVWPG.UVWAngle;
    }

    if(gPGDir.UVWDirCnt > 2)
    {
        if(gMainCmd.FreqSet >= 0)
        {
             return (DIR_FORWARD);
        }
        else
        {
            return (DIR_BACKWARD);
        }
    }
    else if(gPGDir.UVWDirCnt < -2)
    {
        if(gMainCmd.FreqSet >= 0)
        {
            return (DIR_BACKWARD);
        }
        else
        {
            return (DIR_FORWARD);
        }
    }
	else
    {
        return (DIR_ERROR);
    }
}

Uint JudgeRTDir()
{
    int m_Deta;

    m_Deta = (int)gRotorTrans.RTPos - gPGDir.RtPhaseBak;

    if(m_Deta > 1820)
    {
        if(gPGDir.RtDirCnt < 32767)
        {
            gPGDir.RtDirCnt ++;
        }
        gPGDir.RtPhaseBak = (int)gRotorTrans.RTPos;
    }
    else if(m_Deta < -1820)
    {
        if(gPGDir.RtDirCnt > -32767)
        {
            gPGDir.RtDirCnt --;
        }
        gPGDir.RtPhaseBak = (int)gRotorTrans.RTPos;
    }

    if(gPGDir.RtDirCnt > 2)
    {
        return (DIR_FORWARD);
    }
    else if(gPGDir.RtDirCnt < -2)
    {
        return (DIR_BACKWARD);
    }
	else
    {
        return (DIR_ERROR);
    }
}

void GetUvwPhase()
{
    if((MOTOR_TYPE_PM != gMotorInfo.MotorType) ||
        (gPGData.PGType != PG_TYPE_UVW)   ||
        (gMainCmd.Command.bit.ControlMode != IDC_FVC_CTL))
    {
        return;
    }

    if((Uint)UVW_PG_U > (Uint)0xA500)
    {
        gUVWPG.LogicU = ACTIVE_HARDWARE_LOGICAL_U;
    }
    else
    {
        gUVWPG.LogicU = ! ACTIVE_HARDWARE_LOGICAL_U;
    }
    if((Uint)UVW_PG_V > (Uint)0xA500)
    {
        gUVWPG.LogicV = ACTIVE_HARDWARE_LOGICAL_V;
    }
    else
    {
        gUVWPG.LogicV = ! ACTIVE_HARDWARE_LOGICAL_V;
    }

    if((Uint)UVW_PG_W > (Uint)0xA500)
    {
        gUVWPG.LogicW = ACTIVE_HARDWARE_LOGICAL_W;
    }
    else
    {
        gUVWPG.LogicW = ! ACTIVE_HARDWARE_LOGICAL_W;
    }

    if((gPmParEst.UvwDir == 0 && gMainStatus.RunStep == STATUS_GET_PAR) ||
        (gUVWPG.UvwDir == 0  && gMainStatus.RunStep != STATUS_GET_PAR))
    {
        gUVWPG.UVWData = (gUVWPG.LogicU<<2) + (gUVWPG.LogicV<<1) + gUVWPG.LogicW;
    }
    else
    {
        gUVWPG.UVWData = (gUVWPG.LogicU<<2) + (gUVWPG.LogicW<<1) + gUVWPG.LogicV;
    }

    if((gUVWPG.UVWData > 0) && (gUVWPG.UVWData < 7))
    {
        gUVWPG.UVWAngle =  gUVWAngleTable[gUVWPG.UVWData - 1];
    }
    else if(gMainCmd.Command.bit.Start)
    {
        gError.ErrorCode.all |= ERROR_ENCODER;
        if(gUVWPG.UVWData == 7) gError.ErrorInfo[4].bit.Fault1 = 11;
        else                    gError.ErrorInfo[4].bit.Fault1 = 12;
    }

}

void VCGetFeedBackSpeed(void)
{
    long tempLong;

    switch(gPGData.PGType)
    {
        case PG_TYPE_UVW:
        case PG_TYPE_SPECIAL_UVW:
            GetUvwPhase();
            ;
        case PG_TYPE_ABZ:
	         if((*EQepRegs).QFLG.bit.WTO == 1)
	         {
		        (*EQepRegs).QCLR.all = 0x0010;
	         }

	         GetSpeedMMethod();
	         GetSpeedMTMethod();
             gFVCSpeed.SpeedTemp = gFVCSpeed.MTSpeed;

             break;

        case PG_TYPE_RESOLVER:
            RotorTransCalVel();
            gFVCSpeed.SpeedTemp = gRotorTrans.FreqFeed;
            break;

        case PG_TYPE_SC:
            ;
            break;
        default:
             break;
    }

    tempLong = (long)gFVCSpeed.SpeedTemp * gFVCSpeed.TransRatio;
    gFVCSpeed.SpeedEncoder = (llong)tempLong * 16777 >> 24;

    gMainCmd.FreqFeed = gFVCSpeed.SpeedEncoder;

}

void GetSpeedMMethod(void)
{
	Ullong m_Llong,m_Long;
	Uint   m_UData;
	int	   m_Speed;

	m_UData = abs(gFVCSpeed.MDetaPos);
	m_Long  = (Ulong)gMotorExtInfo.Poles * (Ulong)m_UData;
	m_Llong = ((Ullong)m_Long * 50000)<<15;
	m_Long  = (Ullong)gPGData.PulseNum * (Ullong)gBasePar.FullFreq01;

	m_Speed = __IQsat((m_Llong / m_Long), 32767, -32767);
	if(gFVCSpeed.MDetaPosBak < 0)
	{
		m_Speed = -m_Speed;
	}
    SpeedSmoothDeal( &gFVCSpeed.MSpeedSmooth, m_Speed);

    gFVCSpeed.MSpeed = Filter4(gFVCSpeed.MSpeedSmooth.LastSpeed, gFVCSpeed.MSpeed);

    if( gFVCSpeed.MSpeedSmooth.LastSpeed > gFVCSpeed.MSpeed )
    {
        gFVCSpeed.MSpeed++;
    }
    else if(gFVCSpeed.MSpeedSmooth.LastSpeed < gFVCSpeed.MSpeed )
    {
        gFVCSpeed.MSpeed--;
    }
    if(abs(gFVCSpeed.MSpeed - gFVCSpeed.MSpeedSmooth.LastSpeed ) < 8 )
    {
        gFVCSpeed.MSpeed = gFVCSpeed.MSpeedSmooth.LastSpeed;
    }
}

void GetSpeedMTMethod(void)
{
	Ullong m_Llong,m_Long;
	Ulong  m_Time;
	Uint   m_UData,m_DetaPos;
	int	   m_Speed;
    long   tempL;
	if((*EQepRegs).QEPSTS.bit.COEF == 1)
	{
		gFVCSpeed.Flag = 1;
		(*EQepRegs).QEPSTS.all = 0x0008;
	}
    DINT;
	if((gFVCSpeed.MTPulseNum == 0) ||
	   (gFVCSpeed.MTTime > 65535) ||
	   (gFVCSpeed.Flag != 0))
	{
        if( 0 < gFVCSpeed.MTPulseNum )    gFVCSpeed.Flag = 0;
		gFVCSpeed.MTPulseNum  = 0;
		gFVCSpeed.MTTime      = 0;
		gFVCSpeed.MTLimitTime = 0;
	EINT;
		gFVCSpeed.MTSpeed = gFVCSpeed.MSpeed;
        return;
	}

    DINT;
	m_DetaPos = gFVCSpeed.MTPulseNum;
	m_Time    = gFVCSpeed.MTTime;
	gFVCSpeed.MTPulseNum = 0;
	gFVCSpeed.MTTime     = 0;
	EINT;
	gFVCSpeed.MTLimitTime = (Uint)m_Time/m_DetaPos;

	m_UData = (Ulong)gMotorExtInfo.Poles * (Ulong)m_DetaPos;
    m_Long = ((Ullong)DSP_CLOCK * 100L * 1000000L) / (16 * (long)m_Time);
	m_Llong = ((Ullong)m_Long * (Ullong)m_UData)<<15;
	m_Long  = (Ullong)gPGData.PulseNum * (Ullong)gBasePar.FullFreq01;
	while((m_Long>>16) != 0)
	{
		m_Long  = m_Long>>1;
		m_Llong = m_Llong>>1;
	}
	if((m_Llong>>15) >= m_Long)
	{
		m_Speed = 32767;
	}
	else
	{
		m_UData = m_Long>>1;
		m_Long  = m_Llong>>1;
		m_Speed = (int)(m_Long/m_UData);
	}

	if(gFVCSpeed.MDetaPosBak < 0)
	{
		m_Speed = -m_Speed;
	}
    if(gVCPar.VCSpeedFilter <= 1)
     {
       gFVCSpeed.MTSpeed = Filter4(m_Speed, gFVCSpeed.MTSpeed);
     }
    else
     {
       tempL =  (long)gFVCSpeed.MTSpeed * (gVCPar.VCSpeedFilter-1L) + 2L * m_Speed;
       gFVCSpeed.MTSpeed = tempL / (gVCPar.VCSpeedFilter + 1L);
     }
    if(gFVCSpeed.MTSpeed < m_Speed)   gFVCSpeed.MTSpeed ++;
    else                                gFVCSpeed.MTSpeed --;
    if(abs(gFVCSpeed.MTSpeed - m_Speed) < 8)
    {
        gFVCSpeed.MTSpeed  = m_Speed;
    }
}

void SpeedSmoothDeal( ROTOR_SPEED_SMOOTH * pSpeedError, int iSpeed )
{
    if( abs( pSpeedError->LastSpeed - iSpeed ) < pSpeedError->SpeedMaxErr )
    {
        pSpeedError->LastSpeed = iSpeed;
        pSpeedError->SpeedMaxErr = pSpeedError->SpeedMaxErr>>1;
        if( 1000 > pSpeedError->SpeedMaxErr )
            pSpeedError->SpeedMaxErr = 1000;
    }
    else
    {
        if( 16383 > pSpeedError->SpeedMaxErr )
            pSpeedError->SpeedMaxErr = pSpeedError->SpeedMaxErr<<1;
        else
            pSpeedError->SpeedMaxErr = 32767;
    }
}

void GetMDetaPos(void)
{
	DINT;
	if((*EQepRegs).QFLG.bit.UTO == 0)
	{
	EINT;
		return;
	}
    EALLOW;
	(*EQepRegs).QCLR.all = 0x0800;
    EDIS;

	gFVCSpeed.MDetaPos = (int)((long)(*EQepRegs).QPOSLAT - gFVCSpeed.MLastPos);
    if(0 != gFVCSpeed.MDetaPos)
    {
        gFVCSpeed.MDetaPosBak = gFVCSpeed.MDetaPos;
    }
	gFVCSpeed.MLastPos = (*EQepRegs).QPOSLAT;
	EINT;
}

void GetMTTimeNum(void)
{
    EALLOW;
	if((*EQepRegs).QEPSTS.bit.UPEVNT == 0)
	{
		return;
	}
	if((Uint)(*EQepRegs).QCPRD < (Uint)gFVCSpeed.MTLimitTime)
	{
	    (*EQepRegs).QEPSTS.all = 0x0080;
		return;
	}
    DINT;
	gFVCSpeed.MTPulseNum += 4;
	gFVCSpeed.MTTime += (*EQepRegs).QCPRD;
	EINT;

	(*EQepRegs).QEPSTS.all = 0x0080;
    EDIS;
}

void GetRotorTransPos()
{
#ifdef TMS320F28035
	Uint  mData,mBit;

    mData = 0;
	DINT;
	ROTOR_TRANS_RD=0;

    gCpuTime.tmpBase = GetTime();

	ROTOR_TRANS_SCLK  = 0;

	mBit   = (ROTOR_TRANS_SO) << 15;
	mData  = mData | mBit;
	ROTOR_TRANS_SCLK = 1;
    asm(" RPT #5 || NOP ");

    ROTOR_TRANS_SCLK  = 0;

	mBit   = (ROTOR_TRANS_SO) << 14;
	mData  = mData | mBit;
	ROTOR_TRANS_SCLK = 1;
    asm(" RPT #5 || NOP ");

    ROTOR_TRANS_SCLK  = 0;

	mBit   = (ROTOR_TRANS_SO) << 13;
	mData  = mData | mBit;
	ROTOR_TRANS_SCLK = 1;
    asm(" RPT #5 || NOP ");

    ROTOR_TRANS_SCLK  = 0;

	mBit   = (ROTOR_TRANS_SO) << 12;
	mData  = mData | mBit;
	ROTOR_TRANS_SCLK = 1;
    asm(" RPT #5 || NOP ");

    ROTOR_TRANS_SCLK  = 0;

	mBit   = (ROTOR_TRANS_SO) << 11;
	mData  = mData | mBit;
	ROTOR_TRANS_SCLK = 1;
    asm(" RPT #5 || NOP ");

    ROTOR_TRANS_SCLK  = 0;

	mBit   = (ROTOR_TRANS_SO) << 10;
	mData  = mData | mBit;
	ROTOR_TRANS_SCLK = 1;
    asm(" RPT #5 || NOP ");

    ROTOR_TRANS_SCLK  = 0;

	mBit   = (ROTOR_TRANS_SO) << 9;
	mData  = mData | mBit;
	ROTOR_TRANS_SCLK = 1;
    asm(" RPT #5 || NOP ");

    ROTOR_TRANS_SCLK  = 0;

	mBit   = (ROTOR_TRANS_SO) << 8;
	mData  = mData | mBit;
	ROTOR_TRANS_SCLK = 1;
    asm(" RPT #5 || NOP ");

    ROTOR_TRANS_SCLK  = 0;

	mBit   = (ROTOR_TRANS_SO) << 7;
	mData  = mData | mBit;
	ROTOR_TRANS_SCLK = 1;
    asm(" RPT #5 || NOP ");

    ROTOR_TRANS_SCLK  = 0;

	mBit   = (ROTOR_TRANS_SO) << 6;
	mData  = mData | mBit;
	ROTOR_TRANS_SCLK = 1;
    asm(" RPT #5 || NOP ");

    ROTOR_TRANS_SCLK  = 0;

	mBit   = (ROTOR_TRANS_SO) << 5;
	mData  = mData | mBit;
	ROTOR_TRANS_SCLK = 1;
    asm(" RPT #5 || NOP ");

    ROTOR_TRANS_SCLK  = 0;

	mBit   = (ROTOR_TRANS_SO) << 4;
	mData  = mData | mBit;
	ROTOR_TRANS_SCLK = 1;

    EINT;

    if((gPGData.SpeedDir && gMainStatus.RunStep != STATUS_GET_PAR) ||
        (gPGData.PGDir && gMainStatus.RunStep == STATUS_GET_PAR))
    {
	    gRotorTrans.RtorBuffer = 65535 - (Uint)mData;
    }
    else
    {
        gRotorTrans.RtorBuffer = (Uint)mData;
    }

    gCpuTime.tmpTime = gCpuTime.tmpBase- GetTime();

#else

    if((gPGData.SpeedDir && gMainStatus.RunStep != STATUS_GET_PAR) ||
        (gPGData.PGDir && gMainStatus.RunStep == STATUS_GET_PAR))
    {
        gRotorTrans.RtorBuffer = 65535 - (Uint)SpicRegs.SPIRXBUF;
    }
    else
    {
        gRotorTrans.RtorBuffer = SpicRegs.SPIRXBUF;
    }
#endif

    gRotorTrans.AbsRotPos = gRotorTrans.RtorBuffer >> 4;

    if(gMainStatus.RunStep == STATUS_GET_PAR)
    {
        if(((int)gRotorTrans.RtorBuffer > 0) &&
            ((int)gRotorTrans.SimuZBack <= 0) &&
            ((int)gRotorTrans.SimuZBack2 <= 0) &&
            (gIPMZero.zFilterCnt == 0))
        {
            gIPMZero.Flag = 1;
            gIPMZero.zFilterCnt = 8;
            gIPMPos.PosInZInfo = gIPMPos.RotorPos;
        }
        gRotorTrans.SimuZBack2 = gRotorTrans.SimuZBack;
        gRotorTrans.SimuZBack = gRotorTrans.RtorBuffer;
    }

    gRotorTrans.RTPos = (Ulong)gRotorTrans.RtorBuffer * gRotorTrans.PolesRatio >>8;

#ifdef TMS320F2808

    SpicRegs.SPITXBUF = 0xFFFF;
#endif
}

void RotorTransSamplePos(void)
{
	Ulong	mTime;
	Ulong 	mDetaTime;

	mTime = GetTime();
	mDetaTime = labs((long)(gRotorTrans.TimeBak - mTime));
	gRotorTrans.Flag = 0;
	if(mDetaTime >= C_TIME_045MS)
	{
		gRotorTrans.Flag = 1;
		gRotorTrans.DetaTime = mDetaTime;
		gRotorTrans.TimeBak = mTime;
	}
}

void RotorTransCalVel(void)
{
	Ulong  mUlong;
	Ulong  m_05ms;
	Uint   m_DetaPos;
	int	   m_Speed;
    long   tempL;

#if 0
    DINT;
    gRotorTrans.DetaTime = gRotorTrans.TimeBak - GetTime();
    gRotorTrans.TimeBak = GetTime();
    gRotorTrans.DetaPos = (int)((Uint)gRotorTrans.RTPos - (Uint)gRotorTrans.PosBak);
	gRotorTrans.PosBak  = gRotorTrans.RTPos;
    EINT;

    m_DetaPos = abs(gRotorTrans.DetaPos);
    tempL = gRotorTrans.DetaTime >> 1;
    mUlong = (50000L * (Ulong)m_DetaPos + tempL) / gRotorTrans.DetaTime;
    tempL = gBasePar.FullFreq01 >> 1;
    tempL = (mUlong * (1000L * DSP_CLOCK) + tempL) / gBasePar.FullFreq01;
    m_Speed = __IQsat(tempL, 32767, -32767);

    if(gRotorTrans.DetaPos < 0)
	{
		m_Speed = -m_Speed;
	}
#else
    DINT;
    gRotorTrans.IntNum = gRotorTrans.IntCnt;
    gRotorTrans.IntCnt = 0;
    gRotorTrans.DetaPos = (int)((Uint)gRotorTrans.RTPos - (Uint)gRotorTrans.PosBak);
	gRotorTrans.PosBak  = gRotorTrans.RTPos;
    EINT;

    if(gRotorTrans.IntNum == 0)
    {
        return;
    }

    m_DetaPos = abs(gRotorTrans.DetaPos);
    m_05ms = gRotorTrans.IntNum << 2;
    tempL = (Ulong)m_DetaPos * 100000L / (Ulong)gPWM.gPWMPrdApply;
    tempL = tempL * DSP_CLOCK / m_05ms;
    m_Speed = tempL * 1000L / gBasePar.FullFreq01;

    if(gRotorTrans.DetaPos < 0)
	{
		m_Speed = -m_Speed;
	}
#endif

    gSpeedFilter.Input = m_Speed;
    BurrFilter((BURR_FILTER_STRUCT * )&gSpeedFilter);

    if(gVCPar.VCSpeedFilter <= 1)
	{
        tempL =  (long)gRotorTrans.FreqFeed * 29L + 2L * m_Speed;
        gRotorTrans.FreqFeed = tempL / 31L;
	}
	else
	{
        tempL =  (long)gRotorTrans.FreqFeed * (gVCPar.VCSpeedFilter-1L) + 2L * m_Speed;
        gRotorTrans.FreqFeed = tempL / (gVCPar.VCSpeedFilter + 1L);
	}

    gRotorTrans.RealTimeSpeed = gRotorTrans.FreqFeed;
    tempL = ((long)gRotorTrans.RealTimeSpeed * 500L /gBasePar.FcSetApply)>>3;
	gRotorTrans.PosComp = tempL * gBasePar.FullFreq>>16;
	return;
}
