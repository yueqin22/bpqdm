
#include "MotorInclude.h"

extern void ADCOverInterrupt(void);
extern void PWMZeroInterrupt(void);
extern void HardWareErrorDeal(void);

extern void Main2msMotorA(void);
extern void Main2msMotorB(void);
extern void Main2msMotorC(void);
extern void Main2msMotorD(void);

extern void Main05msMotor(void);
extern void Main0msMotor(void);

extern void Main05msFunctionA(void);
extern void Main05msFunctionB(void);
extern void Main05msFunctionC(void);
extern void Main05msFunctionD(void);
extern void Main0msFunction(void);

void main(void)
{
	Ulong m_BaseTime,m_DetaTime;
	Uint  m_LoopFlag;

   	InitSysCtrl();

   	InitInterrupt();

   	InitPeripherals();

   	InitForMotorApp();
   	InitForFunctionApp();

	EnableDog();
	SetInterruptEnable();
   	EINT;
   	ERTM;

	m_LoopFlag = 2;
	m_BaseTime = GetTime();

  	while(1)
   	{
		m_DetaTime = m_BaseTime - GetTime();
		if(m_DetaTime >= C_TIME_05MS)
        {
			m_LoopFlag ++;
			m_BaseTime -= C_TIME_05MS;
 			KickDog();

            #ifdef CPU_TIME_DEBUG
            gCpuTime.Motor05MsBase = GetTime();
            #endif
            Main05msMotor();
            #ifdef CPU_TIME_DEBUG
            gCpuTime.Motor05Ms = gCpuTime.Motor05MsBase - GetTime();
            #endif

			if((m_LoopFlag & 0x03) == 0)
			{
			    #ifdef CPU_TIME_DEBUG
				gCpuTime.MFA2msBase = GetTime();
                #endif
                Main05msFunctionA();
                Main05msFunctionB();
                #ifdef CPU_TIME_DEBUG
				gCpuTime.MFA2ms = gCpuTime.MFA2msBase - GetTime();
                #endif
			}
            else if((m_LoopFlag & 0x03) == 1)
            {
                #ifdef CPU_TIME_DEBUG
				gCpuTime.MFB2msBase = GetTime();
                #endif

                Main05msFunctionC();

                #ifdef CPU_TIME_DEBUG
				gCpuTime.MFB2ms = gCpuTime.MFB2msBase - GetTime();
                #endif
            }
			else if((m_LoopFlag & 0x03) == 2)
			{
			    #ifdef CPU_TIME_DEBUG
				gCpuTime.MFC2msBase = GetTime();
                #endif
                Main05msFunctionD();
                Main2msMotorA();

                #ifdef CPU_TIME_DEBUG
				gCpuTime.MFC2ms = gCpuTime.MFC2msBase - GetTime();
                #endif

			}
            else if((m_LoopFlag & 0x03) == 3)
            {
                #ifdef CPU_TIME_DEBUG
				gCpuTime.MFD2msBase = GetTime();
                #endif
                Main2msMotorB();
                Main2msMotorC();
				Main2msMotorD();
                #ifdef CPU_TIME_DEBUG
				gCpuTime.MFD2ms = gCpuTime.MFD2msBase - GetTime();
                #endif
            }

            gCpuTime.Det05msClk = __IQsat(m_DetaTime, 65535, C_TIME_05MS);
        }

		Main0msFunction();
		Main0msMotor();
		gCpuTime.tmp0Ms ++;
   	}
}

#if 0
interrupt void cpu_timer0_isr(void)
{
   CpuTimer0.InterruptCount++;

   PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}
#endif

interrupt void ADC_Over_isr(void)
{
    EALLOW;
    ADC_CLEAR_INT_FLAG;
    EDIS;
	EINT;
	gCpuTime.ADCIntBase = GetTime();
	ADCOverInterrupt();
	gCpuTime.ADCInt = gCpuTime.ADCIntBase - GetTime();
	DINT;
    EALLOW;
    ADC_RESET_SEQUENCE;
   	PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
    EDIS;
}

interrupt void EPWM1_zero_isr(void)
{
   	EALLOW;
   	EPwm1Regs.ETCLR.bit.INT = 1;
   	EDIS;

	EINT;
	gCpuTime.PWMIntBase = GetTime();
	PWMZeroInterrupt();
	gCpuTime.PWMInt = gCpuTime.PWMIntBase - GetTime();
	DINT;

   	PieCtrlRegs.PIEACK.all = PIEACK_GROUP3;
}

interrupt void EPWM1_TZ_isr(void)
{
	DisableDrive();
	HardWareErrorDeal();

   	PieCtrlRegs.PIEACK.all = PIEACK_GROUP2;
}
