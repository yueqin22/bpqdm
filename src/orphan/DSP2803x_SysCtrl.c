
#include "DSP2803x_Device.h"
#include "DSP2803x_Examples.h"

#pragma CODE_SECTION(InitFlash, "ramfuncs");

void InitSysCtrl(void)
{

   DisableDog();

        EALLOW;
        SysCtrlRegs.PCLKCR0.bit.ADCENCLK = 1;
        (*Device_cal)();
        SysCtrlRegs.PCLKCR0.bit.ADCENCLK = 0;
        EDIS;

   IntOsc1Sel();

   InitPll(DSP28_PLLCR,DSP28_DIVSEL);

   InitPeripheralClocks();
}

void InitFlash(void)
{
   EALLOW;

   FlashRegs.FOPT.bit.ENPIPE = 1;

   FlashRegs.FBANKWAIT.bit.PAGEWAIT = 2;

   FlashRegs.FBANKWAIT.bit.RANDWAIT = 2;

   FlashRegs.FOTPWAIT.bit.OTPWAIT = 2;

   FlashRegs.FSTDBYWAIT.bit.STDBYWAIT = 0x01FF;
   FlashRegs.FACTIVEWAIT.bit.ACTIVEWAIT = 0x01FF;
   EDIS;

   asm(" RPT #7 || NOP");
}

void ServiceDog(void)
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

void InitPll(Uint16 val, Uint16 divsel)
{
   volatile Uint16 iVol;

   if (SysCtrlRegs.PLLSTS.bit.MCLKSTS != 0)
   {
      EALLOW;

      SysCtrlRegs.PLLSTS.bit.MCLKCLR = 1;
      EDIS;

      asm("        ESTOP0");
   }

   if (SysCtrlRegs.PLLSTS.bit.DIVSEL != 0)
   {
       EALLOW;
       SysCtrlRegs.PLLSTS.bit.DIVSEL = 0;
       EDIS;
   }

   if (SysCtrlRegs.PLLCR.bit.DIV != val)
   {

      EALLOW;

      SysCtrlRegs.PLLSTS.bit.MCLKOFF = 1;
      SysCtrlRegs.PLLCR.bit.DIV = val;
      EDIS;

      DisableDog();

      while(SysCtrlRegs.PLLSTS.bit.PLLLOCKS != 1)
      {

      }

      EALLOW;
      SysCtrlRegs.PLLSTS.bit.MCLKOFF = 0;
      EDIS;
    }

    if((divsel == 1)||(divsel == 2))
    {
        EALLOW;
        SysCtrlRegs.PLLSTS.bit.DIVSEL = divsel;
        EDIS;
    }

    if(divsel == 3)
    {
        EALLOW;
        SysCtrlRegs.PLLSTS.bit.DIVSEL = 2;
        DELAY_US(50L);
        SysCtrlRegs.PLLSTS.bit.DIVSEL = 3;
        EDIS;
    }
}

void InitPeripheralClocks(void)
{
   EALLOW;

   GpioCtrlRegs.GPAMUX2.bit.GPIO18 = 3;
   SysCtrlRegs.LOSPCP.all = 0x0002;

   SysCtrlRegs.XCLK.bit.XCLKOUTDIV=2;

   SysCtrlRegs.PCLKCR0.bit.ADCENCLK = 1;
   SysCtrlRegs.PCLKCR3.bit.COMP1ENCLK = 1;
   SysCtrlRegs.PCLKCR3.bit.COMP2ENCLK = 1;
   SysCtrlRegs.PCLKCR3.bit.COMP3ENCLK = 1;

   SysCtrlRegs.PCLKCR0.bit.ECANAENCLK=1;
   SysCtrlRegs.PCLKCR1.bit.EQEP1ENCLK = 1;
   SysCtrlRegs.PCLKCR1.bit.EPWM1ENCLK = 1;
   SysCtrlRegs.PCLKCR1.bit.EPWM2ENCLK = 1;
   SysCtrlRegs.PCLKCR1.bit.EPWM3ENCLK = 1;

   SysCtrlRegs.PCLKCR0.bit.HRPWMENCLK = 1;
   SysCtrlRegs.PCLKCR0.bit.I2CAENCLK = 1;
   SysCtrlRegs.PCLKCR0.bit.LINAENCLK = 1;
   SysCtrlRegs.PCLKCR3.bit.CLA1ENCLK = 1;
   SysCtrlRegs.PCLKCR0.bit.SCIAENCLK = 1;
   SysCtrlRegs.PCLKCR0.bit.SPIAENCLK = 1;
   SysCtrlRegs.PCLKCR0.bit.SPIBENCLK = 1;

   SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1;

   EDIS;
}

#define STATUS_FAIL          0
#define STATUS_SUCCESS       1

Uint16 CsmUnlock()
{
    volatile Uint16 temp;

    EALLOW;
    CsmRegs.KEY0 = 0xFFFF;
    CsmRegs.KEY1 = 0xFFFF;
    CsmRegs.KEY2 = 0xFFFF;
    CsmRegs.KEY3 = 0xFFFF;
    CsmRegs.KEY4 = 0xFFFF;
    CsmRegs.KEY5 = 0xFFFF;
    CsmRegs.KEY6 = 0xFFFF;
    CsmRegs.KEY7 = 0xFFFF;
    EDIS;

    temp = CsmPwl.PSWD0;
    temp = CsmPwl.PSWD1;
    temp = CsmPwl.PSWD2;
    temp = CsmPwl.PSWD3;
    temp = CsmPwl.PSWD4;
    temp = CsmPwl.PSWD5;
    temp = CsmPwl.PSWD6;
    temp = CsmPwl.PSWD7;

    if (CsmRegs.CSMSCR.bit.SECURE == 0) return STATUS_SUCCESS;
    else return STATUS_FAIL;

}

void IntOsc1Sel (void) {
    EALLOW;
    SysCtrlRegs.CLKCTL.bit.INTOSC1OFF = 0;
    SysCtrlRegs.CLKCTL.bit.OSCCLKSRCSEL=0;
    SysCtrlRegs.CLKCTL.bit.XCLKINOFF=1;
    SysCtrlRegs.CLKCTL.bit.XTALOSCOFF=1;
    SysCtrlRegs.CLKCTL.bit.INTOSC2OFF=1;
    EDIS;
}

void IntOsc2Sel (void) {
    EALLOW;
    SysCtrlRegs.CLKCTL.bit.INTOSC2OFF = 0;
    SysCtrlRegs.CLKCTL.bit.OSCCLKSRC2SEL = 1;
    SysCtrlRegs.CLKCTL.bit.XCLKINOFF = 1;
    SysCtrlRegs.CLKCTL.bit.XTALOSCOFF = 1;
    SysCtrlRegs.CLKCTL.bit.OSCCLKSRCSEL = 1;
    SysCtrlRegs.CLKCTL.bit.WDCLKSRCSEL = 1;
    SysCtrlRegs.CLKCTL.bit.INTOSC1OFF = 1;
    EDIS;
}

void XtalOscSel (void)  {
     EALLOW;
     SysCtrlRegs.CLKCTL.bit.XTALOSCOFF = 0;
     SysCtrlRegs.CLKCTL.bit.XCLKINOFF = 1;
     SysCtrlRegs.CLKCTL.bit.OSCCLKSRC2SEL = 0;
     SysCtrlRegs.CLKCTL.bit.OSCCLKSRCSEL = 1;
     SysCtrlRegs.CLKCTL.bit.WDCLKSRCSEL = 1;
     SysCtrlRegs.CLKCTL.bit.INTOSC2OFF = 1;
     SysCtrlRegs.CLKCTL.bit.INTOSC1OFF = 1;
     EDIS;
}

void ExtOscSel (void)  {
     EALLOW;
     SysCtrlRegs.XCLK.bit.XCLKINSEL = 1;
     SysCtrlRegs.CLKCTL.bit.XTALOSCOFF = 1;
     SysCtrlRegs.CLKCTL.bit.XCLKINOFF = 0;
     SysCtrlRegs.CLKCTL.bit.OSCCLKSRC2SEL = 0;
     SysCtrlRegs.CLKCTL.bit.OSCCLKSRCSEL = 1;
     SysCtrlRegs.CLKCTL.bit.WDCLKSRCSEL = 1;
     SysCtrlRegs.CLKCTL.bit.INTOSC2OFF = 1;
     SysCtrlRegs.CLKCTL.bit.INTOSC1OFF = 1;
     EDIS;
}
