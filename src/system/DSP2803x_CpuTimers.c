#include "DSP2803x_Device.h"
#include "DSP2803x_Examples.h"

struct CPUTIMER_VARS CpuTimer0;
struct CPUTIMER_VARS CpuTimer1;
struct CPUTIMER_VARS CpuTimer2;

#define CPU_TIMER_PRD_MAX   0xFFFFFFFFU

static void InitOneTimer(struct CPUTIMER_VARS *Timer,
                         volatile struct CPUTIMER_REGS *Regs)
{
    Timer->RegsAddr = Regs;

    Regs->PRD.all  = CPU_TIMER_PRD_MAX;

    Regs->TPR.all  = 0;
    Regs->TPRH.all = 0;

    Regs->TCR.bit.TSS = 1;
    Regs->TCR.bit.TRB = 1;

    Timer->InterruptCount = 0;
}

void InitCpuTimers(void)
{
    InitOneTimer(&CpuTimer0, &CpuTimer0Regs);
    InitOneTimer(&CpuTimer1, &CpuTimer1Regs);
    InitOneTimer(&CpuTimer2, &CpuTimer2Regs);
}

void ConfigCpuTimer(struct CPUTIMER_VARS *Timer, float Freq, float Period)
{
    Uint32 temp;
    volatile struct CPUTIMER_REGS *Regs = Timer->RegsAddr;

    Timer->CPUFreqInMHz = Freq;
    Timer->PeriodInUSec = Period;
    temp = (long) (Freq * Period);
    Regs->PRD.all = temp;

    Regs->TPR.all  = 0;
    Regs->TPRH.all  = 0;

    Regs->TCR.bit.TSS = 1;
    Regs->TCR.bit.TRB = 1;
    Regs->TCR.bit.SOFT = 0;
    Regs->TCR.bit.FREE = 0;
    Regs->TCR.bit.TIE = 1;

    Timer->InterruptCount = 0;
}
