/* Case-private host-side MotorInclude.h -- MotorCurrentTransform 用例。
 *
 * Why not the common shim
 * -----------------------
 * The common shim's MotorInclude.h is a minimal stub (DataTypes + EQEP/CPU
 * timer + gMainCmd).  This module needs more than that: it reads
 * gPhase.IMPhase and gOutVolt.VoltPhaseApply and writes six current-transform
 * globals, so the real header chain that declares those has to be visible.
 *
 * Why not the real MotorInclude.h
 * --------------------------------
 * The real one pulls in 17 headers plus the whole peripheral register model,
 * and it declares `extern interrupt void ..._isr(void)` prototypes, which
 * MSVC rejects (see MotorEncoder/host_patch.json).
 *
 * What this file does
 * -------------------
 * It replays the subset of the real chain, in dependency order, and stops
 * there.  `MotorInclude.h` is itself part of that chain (MotorPwmInclude.h and
 * MotorInfoCollectInclude.h both include it), so the MOTOR_INCLUDE_H guard is
 * what keeps it from recursing: by the time those two are pulled in the guard
 * is already set and their `#include "MotorInclude.h"` is a no-op, while they
 * still define ANGLE_STRUCT / OUT_VOLT_STRUCT themselves.
 *
 * Nothing here touches a hardware register -- that is a property of this
 * module (pure fixed-point arithmetic on structs), not of the chain.
 */

#ifndef MOTOR_INCLUDE_H
#define MOTOR_INCLUDE_H

/* Must come first: s16/u16/s32/u32 live here (the common shim's DataTypeDef.h,
 * which is what the include path resolves to), and the real chain below uses
 * them without including it directly. */
#include "DataTypeDef.h"

#include "SystemDefine.h"
#include "MotorDefine.h"
#include "MotorCurrentTransform.h"
#include "SubPrgInclude.h"
#include "MotorPwmInclude.h"
#include "MotorInfoCollectInclude.h"

/* This private shim replaces the common shim, so the three objects that
 * hw_mock.c defines have to be declared here as well.  Copied verbatim from
 * tools/difftest/shim/MotorInclude.h -- the widths and the GetTime() spelling
 * must stay identical to what the rest of the bench expects. */
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
