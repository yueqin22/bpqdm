/* Minimal host-side MotorInclude.h: supplies only what the module under test uses.
 *
 * The real MotorInclude.h pulls in 17 headers plus the whole DSP peripheral
 * register model, which cannot compile on the host. This replacement declares
 * only the referenced symbols and preserves the original widths and semantics:
 *   - int fields become s16 (on C28x, int is 16 bits)
 *   - hardware registers become plain memory, instantiated once in hw_mock.c
 *
 * The hardware mock is SHARED (it is not part of the symbol renaming), so both
 * the ref and the new side observe the same register state. Only the symbols
 * exported by the module itself are duplicated per side.
 */
#ifndef MOTOR_INCLUDE_H
#define MOTOR_INCLUDE_H

#include "DataTypeDef.h"

struct EQEP_REGS {
    u32 QPOSCNT;
};
extern struct EQEP_REGS *EQepRegs;

struct CPUTIMER_TIM_REGS {
    u32 all;
};
struct CPUTIMER_REGS {
    struct CPUTIMER_TIM_REGS TIM;
};
struct CPUTIMER_VARS {
    struct CPUTIMER_REGS *RegsAddr;
};
extern struct CPUTIMER_VARS CpuTimer1;

#define GetTime() (CpuTimer1.RegsAddr->TIM.all)

typedef struct BASE_COMMAND_STRUCT_DEF {
    int Command;
    s16 FreqSet;
} BASE_COMMAND_STRUCT;

extern BASE_COMMAND_STRUCT gMainCmd;

#endif
