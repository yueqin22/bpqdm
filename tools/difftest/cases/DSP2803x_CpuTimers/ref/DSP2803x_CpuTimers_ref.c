
#include "DSP2803x_Device.h"
#include "DSP2803x_Examples.h"

struct CPUTIMER_VARS CpuTimer0;
struct CPUTIMER_VARS CpuTimer1;
struct CPUTIMER_VARS CpuTimer2;

void InitCpuTimers(void)
{

	CpuTimer0.RegsAddr = &CpuTimer0Regs;

	CpuTimer0Regs.PRD.all  = 0xFFFFFFFF;

	CpuTimer0Regs.TPR.all  = 0;
	CpuTimer0Regs.TPRH.all = 0;

	CpuTimer0Regs.TCR.bit.TSS = 1;

	CpuTimer0Regs.TCR.bit.TRB = 1;

	CpuTimer0.InterruptCount = 0;

	CpuTimer1.RegsAddr = &CpuTimer1Regs;
	CpuTimer2.RegsAddr = &CpuTimer2Regs;

	CpuTimer1Regs.PRD.all  = 0xFFFFFFFF;
	CpuTimer2Regs.PRD.all  = 0xFFFFFFFF;

	CpuTimer1Regs.TPR.all  = 0;
	CpuTimer1Regs.TPRH.all = 0;
	CpuTimer2Regs.TPR.all  = 0;
	CpuTimer2Regs.TPRH.all = 0;

	CpuTimer1Regs.TCR.bit.TSS = 1;
	CpuTimer2Regs.TCR.bit.TSS = 1;

	CpuTimer1Regs.TCR.bit.TRB = 1;
	CpuTimer2Regs.TCR.bit.TRB = 1;

	CpuTimer1.InterruptCount = 0;
	CpuTimer2.InterruptCount = 0;

}

void ConfigCpuTimer(struct CPUTIMER_VARS *Timer, float Freq, float Period)
{
	Uint32 	temp;

	Timer->CPUFreqInMHz = Freq;
	Timer->PeriodInUSec = Period;
	temp = (long) (Freq * Period);
	Timer->RegsAddr->PRD.all = temp;

	Timer->RegsAddr->TPR.all  = 0;
	Timer->RegsAddr->TPRH.all  = 0;

	Timer->RegsAddr->TCR.bit.TSS = 1;
	Timer->RegsAddr->TCR.bit.TRB = 1;
	Timer->RegsAddr->TCR.bit.SOFT = 0;
	Timer->RegsAddr->TCR.bit.FREE = 0;
	Timer->RegsAddr->TCR.bit.TIE = 1;

	Timer->InterruptCount = 0;
}
