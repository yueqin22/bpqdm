/* Hardware register mock: one shared instance, read by both ref and new side. */

#include "MotorInclude.h"

static struct EQEP_REGS        g_eqep_regs;
static struct CPUTIMER_REGS    g_cputimer_regs;

struct EQEP_REGS        *EQepRegs   = &g_eqep_regs;
struct CPUTIMER_VARS     CpuTimer1  = { &g_cputimer_regs };
BASE_COMMAND_STRUCT      gMainCmd;
