#include "MotorDefine.h"
#include "MotorInclude.h"

#define Device_cal (void   (*)(void))0x3D7C80

#pragma CODE_SECTION(InitFlash, "ramfuncs");
extern COPY_TABLE prginRAM;

void 	copy_prg(COPY_TABLE *tp);
void   	DisableDog(void);
void 	InitPll(Uint val);
void	InitFlash(void);
void	InitPeripheralClocks(void);

void 	InitPieCtrl(void);
void 	InitPieVectTable(void);

void	InitSetGpio(void);
extern interrupt void SCI_RXD_isr(void);
extern interrupt void SCI_TXD_isr(void);

void InitSysCtrl()
{
    DisableDog();

        EALLOW;

        EDIS;

    InitPll(DSP_CLOCK/10);

    copy_prg(&prginRAM);
    InitFlash();

    InitPeripheralClocks();
}

void InitInterrupt()
{
   DINT;
   InitPieCtrl();
   IFR = 0x0000;
   IER = 0x0000;

   InitPieVectTable();

   EALLOW;

   PieVectTable.SCITXINTA 	= &SCI_TXD_isr;
   PieVectTable.SCIRXINTA 	= &SCI_RXD_isr;
   PieVectTable.EPWM1_TZINT = &EPWM1_TZ_isr;
   PieVectTable.EPWM1_INT 	= &EPWM1_zero_isr;
   PieVectTable.ADCINT1     = &ADC_Over_isr;

   EDIS;
}

void SetInterruptEnable()
{
   PieCtrlRegs.PIEIER1.bit.INTx1 = 1;
   PieCtrlRegs.PIEIER3.bit.INTx1 = 1;
   PieCtrlRegs.PIEIER2.bit.INTx1 = 1;
   IER |= (M_INT1 | M_INT2 | M_INT3 | M_INT5);
}

void InitPeripherals(void)
{
   	InitSetGpio();
   	InitSetPWM();
   	InitSetAdc();

   	InitCpuTimers();

   	StartCpuTimer1();
}

void copy_prg(COPY_TABLE *tp)
{
	Uint size;
	COPY_RECORD *crp = &tp->recs[0];

	size = (Uint)(crp->size - 1);

	MovePrgFrFlashToRam(crp->src_addr, crp->dst_addr, size);
}

void InitFlash(void)
{
   EALLOW;

   FlashRegs.FOTPWAIT.bit.OTPWAIT = READ_OTP_WAITE;

   FlashRegs.FBANKWAIT.bit.PAGEWAIT = READ_PAGE_FLASH_WAITE;

   FlashRegs.FOPT.all = 0x01;

   FlashRegs.FBANKWAIT.bit.RANDWAIT = READ_RAND_FLASH_WAITE;

   EDIS;
   asm(" RPT #7 || NOP");
}

void KickDog(void)
{
    EALLOW;
    SysCtrlRegs.WDKEY = 0x0055;
    SysCtrlRegs.WDKEY = 0x00AA;
    EDIS;
}

void DisableDog(void)
{
    EALLOW;
    SysCtrlRegs.WDCR= 0x0068;
    EDIS;
}

void EnableDog(void)
{
    EALLOW;
    SysCtrlRegs.WDCR= 0x002A;
    EDIS;
}

void InitPll(Uint16 val)
{

    EALLOW;
    SysCtrlRegs.CLKCTL.bit.INTOSC1OFF = 1;
    SysCtrlRegs.CLKCTL.bit.INTOSC2OFF = 1;
    SysCtrlRegs.CLKCTL.bit.WDCLKSRCSEL = 1;
    SysCtrlRegs.CLKCTL.bit.OSCCLKSRCSEL = 1;
    SysCtrlRegs.CLKCTL.bit.OSCCLKSRC2SEL = 0;
    SysCtrlRegs.CLKCTL.bit.XCLKINOFF = 1;
    SysCtrlRegs.CLKCTL.bit.XTALOSCOFF = 0;
    EDIS;

    if (SysCtrlRegs.PLLSTS.bit.MCLKSTS != 1)
    {

        if (SysCtrlRegs.PLLSTS.bit.DIVSEL != 0)
        {
            EALLOW;
            SysCtrlRegs.PLLSTS.bit.DIVSEL = 0;
            EDIS;
        }

        EALLOW;

        SysCtrlRegs.PLLCR.bit.DIV = val;
        SysCtrlRegs.PLLSTS.bit.MCLKOFF = 1;
        EDIS;

        DisableDog();

        while(SysCtrlRegs.PLLSTS.bit.PLLLOCKS != 1);

        EALLOW;
        SysCtrlRegs.PLLSTS.bit.MCLKOFF = 0;
        EDIS;

        EALLOW;
        SysCtrlRegs.PLLSTS.bit.DIVSEL = 2;
        EDIS;
    }
}

void InitPeripheralClocks(void)
{
   EALLOW;

   SysCtrlRegs.PCLKCR1.all = 0x4307;

   SysCtrlRegs.PCLKCR0.all = 0x051C;

   SysCtrlRegs.XCLK.all = 0x02;

   SysCtrlRegs.LOSPCP.all = 0x0002;

   EDIS;
}

void InitPieCtrl(void)
{
	Uint * m_Point1,* m_Point2;
	Uint   m_Index;

    DINT;

    PieCtrlRegs.PIECTRL.bit.ENPIE = 0;

	m_Point1 = (Uint *)&PieCtrlRegs.PIEIER1.all;
	m_Point2 = (Uint *)&PieCtrlRegs.PIEIFR1.all;
	for(m_Index = 0;m_Index<12;m_Index++)
	{
		m_Point1[m_Index] = 0;
		m_Point2[m_Index] = 0;
	}
}

int gErrorIntCnt;
interrupt void rsvd_ISR(void)
{
	gErrorIntCnt++;
}

void InitPieVectTable(void)
{
	int16	i;
	PINT *pPieTable = (void *) &PieVectTable;

	EALLOW;
	for(i=0; i < 128; i++)
	{
		pPieTable[i] = rsvd_ISR;
	}
	EDIS;

	PieCtrlRegs.PIECTRL.bit.ENPIE = 1;

}

void InitSetGpio(void)
{
   EALLOW;

   GpioCtrlRegs.GPAQSEL2.all = 0x0300803FL;

   GpioCtrlRegs.AIODIR.bit.AIO2 = 0;

   GpioCtrlRegs.GPADIR.all = 0x00000980L;

   GpioCtrlRegs.AIOMUX1.bit.AIO2 = 0;

   GpioDataRegs.GPBDAT.all = 0x00000000L;

   GpioCtrlRegs.GPBQSEL1.all = 0x0000000FL;

   GpioCtrlRegs.GPBDIR.all = 0x00;

   GpioDataRegs.GPADAT.all = 0x00000980L;

   GpioCtrlRegs.GPAQSEL1.all = 0x57000000L;

   GpioCtrlRegs.GPAMUX1.all = 0x05000555L;

   GpioCtrlRegs.GPBPUD.all = 0x0;

   GpioCtrlRegs.GPAMUX2.all = 0x00004500L;

   GpioCtrlRegs.GPAPUD.all = 0x00000640;

    EDIS;
}

void InitSetAdc(void)
{
    int waite;

    EALLOW;
        SysCtrlRegs.PCLKCR0.bit.ADCENCLK = 1;
        (*Device_cal)();
    EDIS;

    for(waite = 0;waite<5000;waite++){}

    AdcRegs.ADCCTL1.bit.RESET     = 0x1;
    asm(" RPT #28 || NOP");
    for(waite = 0;waite<5000;waite++){}

    EALLOW;
        AdcRegs.ADCCTL1.bit.ADCREFSEL = 0;
        AdcRegs.ADCCTL1.bit.ADCENABLE = 1;
        AdcRegs.ADCCTL1.bit.ADCPWDN   = 1;
        AdcRegs.ADCCTL1.bit.ADCREFPWD = 1;
        AdcRegs.ADCCTL1.bit.ADCBGPWD  = 1;
	EDIS;

    for(waite = 0;waite<25000;waite++){}

    EALLOW;
	AdcRegs.ADCCTL1.bit.INTPULSEPOS	= 1;
    AdcRegs.ADCSAMPLEMODE.all = 0;
    AdcRegs.INTSEL1N2.all = 0x002C;
	AdcRegs.ADCOFFTRIM.all = 0;

    AdcRegs.ADCSOC13CTL.all = 0x2AC8;
    AdcRegs.ADCSOC0CTL.all  = 0x2BC8;
    AdcRegs.ADCSOC11CTL.all = 0x2B08;
    AdcRegs.ADCSOC7CTL.all  = 0x29C8;
    AdcRegs.ADCSOC4CTL.all  = 0x2A48;
    AdcRegs.ADCSOC10CTL.all = 0x2A88;
    AdcRegs.ADCSOC1CTL.all  = 0x2808;
    AdcRegs.ADCSOC8CTL.all  = 0x2848;
    AdcRegs.ADCSOC5CTL.all  = 0x2A08;
    AdcRegs.ADCSOC12CTL.all = 0x2B88;
    AdcRegs.ADCSOC2CTL.all  = 0x28C8;
    AdcRegs.ADCSOC9CTL.all  = 0x2988;
    AdcRegs.ADCSOC6CTL.all  = 0x2B48;
    AdcRegs.ADCSOC3CTL.all  = 0x2948;

	ADC_CLEAR_INT_FLAG;
    EDIS;
}
