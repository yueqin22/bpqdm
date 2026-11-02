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
	Uint  m_LoopFlag,m_Phase;

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

	for(;;)
	{
		m_DetaTime = m_BaseTime - GetTime();
		if(m_DetaTime >= C_TIME_05MS)
		{
			m_LoopFlag ++;
			m_BaseTime -= C_TIME_05MS;
			KickDog();
			Main05msMotor();

			m_Phase = m_LoopFlag & 0x03;
			switch(m_Phase)
			{
			case 0:
				Main05msFunctionA();
				Main05msFunctionB();
				break;
			case 1:
				Main05msFunctionC();
				break;
			case 2:
				Main05msFunctionD();
				Main2msMotorA();
				break;
			case 3:
				Main2msMotorB();
				Main2msMotorC();
				Main2msMotorD();
				break;
			}

			gCpuTime.Det05msClk = __IQsat(m_DetaTime, 65535, C_TIME_05MS);
		}

		Main0msFunction();
		Main0msMotor();
		gCpuTime.tmp0Ms ++;
	}
}

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
