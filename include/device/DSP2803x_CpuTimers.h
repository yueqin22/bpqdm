
#ifndef DSP2803x_CPU_TIMERS_H
#define DSP2803x_CPU_TIMERS_H

#ifdef __cplusplus
extern "C" {
#endif

struct  TCR_BITS {
   Uint16    rsvd1:4;
   Uint16    TSS:1;
   Uint16    TRB:1;
   Uint16    rsvd2:4;
   Uint16    SOFT:1;
   Uint16    FREE:1;
   Uint16    rsvd3:2;
   Uint16    TIE:1;
   Uint16    TIF:1;
};

union TCR_REG {
   Uint16           all;
   struct TCR_BITS  bit;
};

struct  TPR_BITS {
   Uint16     TDDR:8;
   Uint16     PSC:8;
};

union TPR_REG {
   Uint16           all;
   struct TPR_BITS  bit;
};

struct  TPRH_BITS {
   Uint16     TDDRH:8;
   Uint16     PSCH:8;
};

union TPRH_REG {
   Uint16           all;
   struct TPRH_BITS bit;
};

struct TIM_REG {
   Uint16  LSW;
   Uint16  MSW;
};

union TIM_GROUP {
   Uint32          all;
   struct TIM_REG  half;
};

struct PRD_REG {
   Uint16  LSW;
   Uint16  MSW;
};

union PRD_GROUP {
   Uint32          all;
   struct PRD_REG  half;
};

struct CPUTIMER_REGS {
   union TIM_GROUP TIM;
   union PRD_GROUP PRD;
   union TCR_REG   TCR;
   Uint16          rsvd1;
   union TPR_REG   TPR;
   union TPRH_REG  TPRH;
};

struct CPUTIMER_VARS {
   volatile struct  CPUTIMER_REGS  *RegsAddr;
   Uint32    InterruptCount;
   float   CPUFreqInMHz;
   float   PeriodInUSec;
};

void InitCpuTimers(void);
void ConfigCpuTimer(struct CPUTIMER_VARS *Timer, float Freq, float Period);

extern volatile struct CPUTIMER_REGS CpuTimer0Regs;
extern struct CPUTIMER_VARS CpuTimer0;

extern volatile struct CPUTIMER_REGS CpuTimer1Regs;
extern volatile struct CPUTIMER_REGS CpuTimer2Regs;

extern struct CPUTIMER_VARS CpuTimer1;
extern struct CPUTIMER_VARS CpuTimer2;

#define StartCpuTimer0()   CpuTimer0Regs.TCR.bit.TSS = 0

#define StopCpuTimer0()   CpuTimer0Regs.TCR.bit.TSS = 1

#define ReloadCpuTimer0() CpuTimer0Regs.TCR.bit.TRB = 1

#define ReadCpuTimer0Counter() CpuTimer0Regs.TIM.all

#define ReadCpuTimer0Period() CpuTimer0Regs.PRD.all

#define StartCpuTimer1()   CpuTimer1Regs.TCR.bit.TSS = 0
#define StartCpuTimer2()   CpuTimer2Regs.TCR.bit.TSS = 0

#define StopCpuTimer1()   CpuTimer1Regs.TCR.bit.TSS = 1
#define StopCpuTimer2()   CpuTimer2Regs.TCR.bit.TSS = 1

#define ReloadCpuTimer1() CpuTimer1Regs.TCR.bit.TRB = 1
#define ReloadCpuTimer2() CpuTimer2Regs.TCR.bit.TRB = 1

#define ReadCpuTimer1Counter() CpuTimer1Regs.TIM.all
#define ReadCpuTimer2Counter() CpuTimer2Regs.TIM.all

#define ReadCpuTimer1Period() CpuTimer1Regs.PRD.all
#define ReadCpuTimer2Period() CpuTimer2Regs.PRD.all

#ifdef __cplusplus
}
#endif

#endif
