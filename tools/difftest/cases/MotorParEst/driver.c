/* Differential test driver for MotorParEst.
 *
 * Three entry points, three different shapes:
 *
 *   RunCaseGetPar()      a dispatcher. Reads gGetParVarable.ParEstContent[..]
 *                        and calls one of seven identification steps. Also has
 *                        a "give up" prefix that fires when the drive is not
 *                        started or a fault is latched.
 *   PrepareParForTune()  a bulk copy: 20-odd assignments from half a dozen
 *                        source structs into gMotorExtReg / gPmParEst /
 *                        gPGData / gUVCoff / gGetParVarable, then a switch that
 *                        picks one of six const tables and copies it in.
 *   EndOfParIdentify()   a two-branch state machine on QtEstDelay that either
 *                        tears the drive down (disable, re-point three PIE
 *                        vectors, re-init PWM/ADC, re-enable interrupts) or
 *                        latches an encoder fault.
 *
 * What is compared, and why each thing is there:
 *
 *   per-side    gUVCoff / gMotorExtReg / gGetParVarable / gRrLoIdentify /
 *               gLmIoIdentify -- DEFINED by this module, so each side owns a
 *               renamed copy. Whole-struct memcmp.
 *   shared      everything else the module writes: PieVectTable, PieCtrlRegs,
 *               GpioDataRegs, gMainStatus, gError, gPGData, gPmParEst,
 *               gEstBemf, gIPMZero, plus the two instrumentation arrays.
 *
 * ---- the call log -------------------------------------------------------
 *
 * The dispatcher's entire behaviour is "which callee ran". Empty stubs would
 * make `case IDENTIFY_RR_LO: LmIoIdentify();` byte-identical to the original,
 * so every callee appends its id to a shared log instead. The log is compared
 * as a sequence, not a set -- order and repetition both matter.
 *
 * ---- the CPU control sequence -------------------------------------------
 *
 * DINT / EALLOW / EDIS / EINT / ERTM are real instructions with no memory
 * operand. The shim rewrites them to increments of gHostCpuCtl[], so "the
 * rewrite dropped DINT" is a visible difference rather than nothing at all.
 * This records that the sequence happened; it does not execute it. Interrupt
 * masking itself is not covered on the host.
 *
 * ---- non-trivial initial state ------------------------------------------
 *
 * Every input is filled with a value that DIFFERS from what the correct code
 * writes into that location. That is the only reason a "stopped writing this
 * field" mutation is detectable: ref-vs-new can only see a change on the new
 * side, so if both sides leave the pre-existing value alone they agree.
 * Concretely: PieVectTable's three target vectors are pre-loaded with dummy
 * handlers, PieCtrlRegs.PIEIER3.bit.INTx2 starts at 1, GpioDataRegs.GPASET's
 * GPIO7 starts at 0, and gPmParEst / gPGData / gEstBemf / gIPMZero / gUVCoff
 * all start at values distinct from their sources in gVCPar / gIPMPos /
 * gUVWPG / gMotorExtInfo / gUVCoff.UDivVGet.
 *
 * ---- what is NOT covered -------------------------------------------------
 *
 *   - DINT/EALLOW/EDIS/EINT/ERTM as instructions (see above);
 *   - the 16-bit wrap points of the target: `int` and `Uint` are 32 bits on the
 *     host and 16 on C28x, so `8 + gPGData.PGErrorFlag` truncates differently.
 *     Both sides are host code, so the comparison stays valid; the absolute
 *     values are not transferable;
 *   - the six const tables' PLACEMENT (`.econst`), only their contents as seen
 *     through ParEstContent[];
 *   - what the callees actually do -- they are stubs, so only the call itself
 *     is observed;
 *   - float behaviour: this module has none.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MotorParaIDinclude.h"

#define FAIL_LIMIT 20

/* ---- ref side ------------------------------------------------------------ */

extern void ref_RunCaseGetPar(void);
extern void ref_PrepareParForTune(void);
extern void ref_EndOfParIdentify(void);
extern UV_BIAS_COFF_STRUCT     ref_gUVCoff;
extern MOTOR_EXTERN_STRUCT     ref_gMotorExtReg;
extern MOTOR_PARA_EST          ref_gGetParVarable;
extern IDENTIFY_RRLO_VARIABLE  ref_gRrLoIdentify;
extern IDENTIFY_LMIO_VARIABLE  ref_gLmIoIdentify;

/* ---- new side ------------------------------------------------------------ */

extern void new_RunCaseGetPar(void);
extern void new_PrepareParForTune(void);
extern void new_EndOfParIdentify(void);
extern UV_BIAS_COFF_STRUCT     new_gUVCoff;
extern MOTOR_EXTERN_STRUCT     new_gMotorExtReg;
extern MOTOR_PARA_EST          new_gGetParVarable;
extern IDENTIFY_RRLO_VARIABLE  new_gRrLoIdentify;
extern IDENTIFY_LMIO_VARIABLE  new_gLmIoIdentify;

/* ---- instrumentation provided by the mock -------------------------------- */

#define CALL_LOG_LEN 64
extern Uint16 gCallLog[CALL_LOG_LEN];
extern Uint16 gCallCount;
extern volatile Uint16 gHostCpuCtl[7];

/* ---- per-side snapshot --------------------------------------------------- */

struct SIDE
{
    UV_BIAS_COFF_STRUCT    uvc;
    MOTOR_EXTERN_STRUCT    ext;
    MOTOR_PARA_EST         par;
    IDENTIFY_RRLO_VARIABLE rrlo;
    IDENTIFY_LMIO_VARIABLE lmio;
};

static struct SIDE S[2];

/* ---- input image --------------------------------------------------------- */

static UV_BIAS_COFF_STRUCT    inUvc;
static MOTOR_EXTERN_STRUCT    inExt;
static MOTOR_PARA_EST         inPar;
static IDENTIFY_RRLO_VARIABLE inRrlo;
static IDENTIFY_LMIO_VARIABLE inLmio;

static RUN_STATUS_STRUCT                  inSt;
static BASE_COMMAND_STRUCT                inCmd;
static struct FAULT_CODE_INFOR_STRUCT_DEF inErr;
static MOTOR_EXTERN_STRUCT                inExtInfo;
static PMSM_EST_BEMF                      inBemf;
static PMSM_EST_PARAM_DATA                inPm;
static IPM_ZERO_POS_STRUCT                inZero;
static IPM_POSITION_STRUCT                inPos;
static IPM_UVW_PG_STRUCT                  inUvwpg;
static PG_DATA_STRUCT                     inPg;
static VC_INFO_STRUCT                     inVc;

/* 预置到 PieVectTable 里的假中断入口: 让"有没有改写过这三个向量"可见。 */
static void vecDummyA(void) { }
static void vecDummyB(void) { }
static void vecDummyC(void) { }

static void install_side(int side)
{
    if (side == 0)
    {
        memcpy(&ref_gUVCoff,        &inUvc,  sizeof inUvc);
        memcpy(&ref_gMotorExtReg,   &inExt,  sizeof inExt);
        memcpy(&ref_gGetParVarable, &inPar,  sizeof inPar);
        memcpy(&ref_gRrLoIdentify,  &inRrlo, sizeof inRrlo);
        memcpy(&ref_gLmIoIdentify,  &inLmio, sizeof inLmio);
    }
    else
    {
        memcpy(&new_gUVCoff,        &inUvc,  sizeof inUvc);
        memcpy(&new_gMotorExtReg,   &inExt,  sizeof inExt);
        memcpy(&new_gGetParVarable, &inPar,  sizeof inPar);
        memcpy(&new_gRrLoIdentify,  &inRrlo, sizeof inRrlo);
        memcpy(&new_gLmIoIdentify,  &inLmio, sizeof inLmio);
    }
}

static void snapshot(int side)
{
    struct SIDE *s = &S[side];
    if (side == 0)
    {
        memcpy(&s->uvc,  &ref_gUVCoff,        sizeof s->uvc);
        memcpy(&s->ext,  &ref_gMotorExtReg,   sizeof s->ext);
        memcpy(&s->par,  &ref_gGetParVarable, sizeof s->par);
        memcpy(&s->rrlo, &ref_gRrLoIdentify,  sizeof s->rrlo);
        memcpy(&s->lmio, &ref_gLmIoIdentify,  sizeof s->lmio);
    }
    else
    {
        memcpy(&s->uvc,  &new_gUVCoff,        sizeof s->uvc);
        memcpy(&s->ext,  &new_gMotorExtReg,   sizeof s->ext);
        memcpy(&s->par,  &new_gGetParVarable, sizeof s->par);
        memcpy(&s->rrlo, &new_gRrLoIdentify,  sizeof s->rrlo);
        memcpy(&s->lmio, &new_gLmIoIdentify,  sizeof s->lmio);
    }
}

/* ---- shared snapshot ----------------------------------------------------- */

struct SHARED
{
    struct PIE_VECT_TABLE             pie;
    volatile struct PIE_CTRL_REGS     piec;
    volatile struct GPIO_DATA_REGS    gpio;
    Uint16 cpu[7];
    Uint16 callLog[CALL_LOG_LEN];
    Uint16 callCount;
    RUN_STATUS_STRUCT                 st;
    BASE_COMMAND_STRUCT               cmd;
    struct FAULT_CODE_INFOR_STRUCT_DEF err;
    MOTOR_EXTERN_STRUCT               extInfo;
    PMSM_EST_BEMF                     bemf;
    PMSM_EST_PARAM_DATA               pm;
    IPM_ZERO_POS_STRUCT               zero;
    IPM_POSITION_STRUCT               pos;
    IPM_UVW_PG_STRUCT                 uvwpg;
    PG_DATA_STRUCT                    pg;
    VC_INFO_STRUCT                    vc;
};

static struct SHARED SH[2];

static void install_shared(void)
{
    int i;

    memset(&PieVectTable, 0, sizeof PieVectTable);
    PieVectTable.ADCINT1     = vecDummyA;
    PieVectTable.EPWM1_TZINT = vecDummyB;
    PieVectTable.EPWM1_INT   = vecDummyC;

    memset(&PieCtrlRegs, 0, sizeof PieCtrlRegs);
    PieCtrlRegs.PIEIER3.bit.INTx2 = 1;

    memset(&GpioDataRegs, 0, sizeof GpioDataRegs);

    for (i = 0; i < 7; i++)
    {
        gHostCpuCtl[i] = 0;
    }
    memset(gCallLog, 0, sizeof gCallLog);
    gCallCount = 0;

    memcpy(&gMainStatus,   &inSt,      sizeof inSt);
    memcpy(&gMainCmd,      &inCmd,     sizeof inCmd);
    memcpy(&gError,        &inErr,     sizeof inErr);
    memcpy(&gMotorExtInfo, &inExtInfo, sizeof inExtInfo);
    memcpy(&gEstBemf,      &inBemf,    sizeof inBemf);
    memcpy(&gPmParEst,     &inPm,      sizeof inPm);
    memcpy(&gIPMZero,      &inZero,    sizeof inZero);
    memcpy(&gIPMPos,       &inPos,     sizeof inPos);
    memcpy(&gUVWPG,        &inUvwpg,   sizeof inUvwpg);
    memcpy(&gPGData,       &inPg,      sizeof inPg);
    memcpy(&gVCPar,        &inVc,      sizeof inVc);
}

static void snap_shared(int side)
{
    struct SHARED *s = &SH[side];
    int i;

    memcpy(&s->pie,  &PieVectTable, sizeof s->pie);
    memcpy(&s->piec, &PieCtrlRegs,  sizeof s->piec);
    memcpy(&s->gpio, &GpioDataRegs, sizeof s->gpio);
    for (i = 0; i < 7; i++)
    {
        s->cpu[i] = gHostCpuCtl[i];
    }
    memcpy(s->callLog, gCallLog, sizeof s->callLog);
    s->callCount = gCallCount;

    memcpy(&s->st,      &gMainStatus,   sizeof s->st);
    memcpy(&s->cmd,     &gMainCmd,      sizeof s->cmd);
    memcpy(&s->err,     &gError,        sizeof s->err);
    memcpy(&s->extInfo, &gMotorExtInfo, sizeof s->extInfo);
    memcpy(&s->bemf,    &gEstBemf,      sizeof s->bemf);
    memcpy(&s->pm,      &gPmParEst,     sizeof s->pm);
    memcpy(&s->zero,    &gIPMZero,      sizeof s->zero);
    memcpy(&s->pos,     &gIPMPos,       sizeof s->pos);
    memcpy(&s->uvwpg,   &gUVWPG,        sizeof s->uvwpg);
    memcpy(&s->pg,      &gPGData,       sizeof s->pg);
    memcpy(&s->vc,      &gVCPar,        sizeof s->vc);
}

/* ---- case runner --------------------------------------------------------- */

static long g_cases = 0;
static int  g_fails = 0;

static void report(const char *fn, const char *what)
{
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s: %s\n", fn, what);
    printf("  ParEstMstep=%u TuneType=%d QtEstDelay=%d StatusWord=%d\n",
           (unsigned)inPar.ParEstMstep, (int)inPar.TuneType,
           inPar.QtEstDelay, (int)inPar.StatusWord);
    printf("  content=%d,%d,%d,%d,%d  Start=%u CtrlMode=%u ErrCode=0x%04lX\n",
           (int)inPar.ParEstContent[0], (int)inPar.ParEstContent[1],
           (int)inPar.ParEstContent[2], (int)inPar.ParEstContent[3],
           (int)inPar.ParEstContent[4],
           (unsigned)inCmd.Command.bit.Start,
           (unsigned)inCmd.Command.bit.ControlMode,
           (unsigned long)inErr.ErrorCode.all);
    printf("  PGErrFlag=%u SpeedDir=%u PGDir=%u\n",
           (unsigned)inPg.PGErrorFlag, (unsigned)inPg.SpeedDir,
           (unsigned)inPg.PGDir);
    printf("  ref calls=%u new calls=%u\n", SH[0].callCount, SH[1].callCount);
}

static void cmp(const char *fn, const char *what, const void *a, const void *b,
                size_t n)
{
    if (memcmp(a, b, n) != 0)
    {
        report(fn, what);
    }
}

static void compare(const char *fn)
{
    cmp(fn, "gUVCoff",        &S[0].uvc,  &S[1].uvc,  sizeof S[0].uvc);
    cmp(fn, "gMotorExtReg",   &S[0].ext,  &S[1].ext,  sizeof S[0].ext);
    cmp(fn, "gGetParVarable", &S[0].par,  &S[1].par,  sizeof S[0].par);
    cmp(fn, "gRrLoIdentify",  &S[0].rrlo, &S[1].rrlo, sizeof S[0].rrlo);
    cmp(fn, "gLmIoIdentify",  &S[0].lmio, &S[1].lmio, sizeof S[0].lmio);

    cmp(fn, "PieVectTable",  &SH[0].pie,   &SH[1].pie,   sizeof SH[0].pie);
    cmp(fn, "PieCtrlRegs",   &SH[0].piec,  &SH[1].piec,  sizeof SH[0].piec);
    cmp(fn, "GpioDataRegs",  &SH[0].gpio,  &SH[1].gpio,  sizeof SH[0].gpio);
    cmp(fn, "cpuCtl",        SH[0].cpu,    SH[1].cpu,    sizeof SH[0].cpu);
    cmp(fn, "callLog",       SH[0].callLog, SH[1].callLog, sizeof SH[0].callLog);
    cmp(fn, "callCount",     &SH[0].callCount, &SH[1].callCount, sizeof SH[0].callCount);
    cmp(fn, "gMainStatus",   &SH[0].st,    &SH[1].st,    sizeof SH[0].st);
    cmp(fn, "gMainCmd",      &SH[0].cmd,   &SH[1].cmd,   sizeof SH[0].cmd);
    cmp(fn, "gError",        &SH[0].err,   &SH[1].err,   sizeof SH[0].err);
    cmp(fn, "gMotorExtInfo", &SH[0].extInfo, &SH[1].extInfo, sizeof SH[0].extInfo);
    cmp(fn, "gEstBemf",      &SH[0].bemf,  &SH[1].bemf,  sizeof SH[0].bemf);
    cmp(fn, "gPmParEst",     &SH[0].pm,    &SH[1].pm,    sizeof SH[0].pm);
    cmp(fn, "gIPMZero",      &SH[0].zero,  &SH[1].zero,  sizeof SH[0].zero);
    cmp(fn, "gIPMPos",       &SH[0].pos,   &SH[1].pos,   sizeof SH[0].pos);
    cmp(fn, "gUVWPG",        &SH[0].uvwpg, &SH[1].uvwpg, sizeof SH[0].uvwpg);
    cmp(fn, "gPGData",       &SH[0].pg,    &SH[1].pg,    sizeof SH[0].pg);
    cmp(fn, "gVCPar",        &SH[0].vc,    &SH[1].vc,    sizeof SH[0].vc);
}

typedef void (*entry_t)(void);

static entry_t g_refFn[3];
static entry_t g_newFn[3];
static const char *g_fnName[3] = { "RunCaseGetPar", "PrepareParForTune",
                                   "EndOfParIdentify" };

static void init_ptrs(void)
{
    g_refFn[0] = ref_RunCaseGetPar;      g_newFn[0] = new_RunCaseGetPar;
    g_refFn[1] = ref_PrepareParForTune;  g_newFn[1] = new_PrepareParForTune;
    g_refFn[2] = ref_EndOfParIdentify;   g_newFn[2] = new_EndOfParIdentify;
}

static void run_case(int which)
{
    install_shared();
    install_side(0);
    g_refFn[which]();
    snapshot(0);
    snap_shared(0);

    install_shared();
    install_side(1);
    g_newFn[which]();
    snapshot(1);
    snap_shared(1);

    g_cases++;
    compare(g_fnName[which]);
}

/* ---- input helpers ------------------------------------------------------- */

static void clear_inputs(void)
{
    memset(&inUvc, 0, sizeof inUvc);
    memset(&inExt, 0, sizeof inExt);
    memset(&inPar, 0, sizeof inPar);
    memset(&inRrlo, 0, sizeof inRrlo);
    memset(&inLmio, 0, sizeof inLmio);
    memset(&inSt, 0, sizeof inSt);
    memset(&inCmd, 0, sizeof inCmd);
    memset(&inErr, 0, sizeof inErr);
    memset(&inExtInfo, 0, sizeof inExtInfo);
    memset(&inBemf, 0, sizeof inBemf);
    memset(&inPm, 0, sizeof inPm);
    memset(&inZero, 0, sizeof inZero);
    memset(&inPos, 0, sizeof inPos);
    memset(&inUvwpg, 0, sizeof inUvwpg);
    memset(&inPg, 0, sizeof inPg);
    memset(&inVc, 0, sizeof inVc);
}

/* 所有"被写"的位置都预置成**与正确写入值不同**的非平凡值。
 * 这是"改写不再写某字段"能被看见的唯一理由: ref-vs-new 只看得到 new 侧的变化,
 * 两侧都保留原值的话它们当然相等。 */
static void dirty_all(void)
{
    memset(&inSt, 0xFF, sizeof inSt);
    memset(&inErr, 0xFF, sizeof inErr);
    memset(&inPm, 0xFF, sizeof inPm);
    memset(&inZero, 0xFF, sizeof inZero);
    memset(&inPg, 0xFF, sizeof inPg);
    memset(&inBemf, 0xFF, sizeof inBemf);
    memset(&inExt, 0xFF, sizeof inExt);
    memset(&inUvc, 0xFF, sizeof inUvc);
    memset(&inExtInfo, 0x5A, sizeof inExtInfo);
    memset(&inPos, 0x5A, sizeof inPos);
    memset(&inUvwpg, 0x5A, sizeof inUvwpg);
    memset(&inVc, 0x5A, sizeof inVc);
}

/* ---- phases -------------------------------------------------------------- */

/* RunCaseGetPar: the "give up" prefix (fault latched OR not started) and the
 * 8-way dispatch on ParEstContent[ParEstMstep]. Every reachable content value is
 * placed at every reachable step, because the failure branch WRITES
 * ParEstContent[ParEstMstep] and so the two interact. */
static void phase_run_case(void)
{
    static const int steps[] = {
        IDENTIFY_RS, IDENTIFY_RR_LO, IDENTIFY_LM_IO, PM_EST_POLSE_POS,
        PM_EST_NO_LOAD, PM_EST_WITH_LOAD, PM_EST_BEMF, IDENTIFY_END, 99, -1
    };
    static const unsigned long errCode[] = { 0UL, 0x400UL, 0xFFFFFFFFUL };
    unsigned mstep, v, e, s, k;

    for (mstep = 0; mstep < 5; mstep++)
    {
        for (v = 0; v < sizeof steps / sizeof steps[0]; v++)
        {
            for (e = 0; e < sizeof errCode / sizeof errCode[0]; e++)
            {
                for (s = 0; s < 2; s++)
                {
                    for (k = 0; k < 2; k++)
                    {
                        clear_inputs();
                        dirty_all();
                        inPar.ParEstMstep = (Uint16)mstep;
                        inPar.ParEstContent[0] = IDENTIFY_RS;
                        inPar.ParEstContent[1] = IDENTIFY_RR_LO;
                        inPar.ParEstContent[2] = IDENTIFY_LM_IO;
                        inPar.ParEstContent[3] = PM_EST_POLSE_POS;
                        inPar.ParEstContent[4] = PM_EST_BEMF;
                        inPar.ParEstContent[mstep] = (PAR_EST_MAIN_STEP)steps[v];
                        inErr.ErrorCode.all = errCode[e];
                        inCmd.Command.bit.Start = (Uint)s;
                        inPar.StatusWord = k ? TUNE_FINISH : TUNE_INITIAL;
                        run_case(0);
                    }
                }
            }
        }
    }
}

/* PrepareParForTune: 20-odd field copies plus the six-way table selection.
 * TuneType values are the ones the enum declares, plus two that fall to
 * `default`. */
static void phase_prepare(void)
{
    static const int tune[] = {
        TUNE_NULL, TUNE_IM_STATIC, TUNE_IM_ROTOR, TUNE_PM_COMP_LOAD,
        TUNE_PM_COMP_NO_LOAD, TUNE_PM_PARA_temp, TUNE_INERTIA, 7, -3
    };
    unsigned a, b;

    for (a = 0; a < sizeof tune / sizeof tune[0]; a++)
    {
        for (b = 0; b < 2; b++)
        {
            clear_inputs();
            dirty_all();
            inPar.TuneType = (TUNE_FLOW_ENUM)tune[a];
            inPar.ParEstMstep = b ? 4 : 0;
            inUvc.UDivVGet = b ? 0x1234u : 0u;
            inVc.AcrImKp = b ? 11 : 0;
            inVc.AcrImKi = b ? 22 : 0;
            inVc.AcrItKp = b ? 33 : 0;
            inVc.AcrItKi = b ? 44 : 0;
            inExtInfo.R1 = b ? 101 : 0;
            inExtInfo.R2 = b ? 102 : 0;
            inExtInfo.L0 = b ? 103 : 0;
            inExtInfo.LM = b ? 104 : 0;
            inExtInfo.I0 = b ? 105 : 0;
            inExtInfo.RsPm = b ? 106 : 0;
            inExtInfo.LD = b ? 107 : 0;
            inExtInfo.LQ = b ? 108 : 0;
            inExtInfo.BemfVolt = b ? 109 : 0;
            inPos.RotorZeroGet = b ? 201 : 0;
            inUvwpg.UvwDir = b ? 202 : 0;
            inUvwpg.UvwZeroPos_deg = b ? 203 : 0;
            inPg.SpeedDir = b ? 204 : 0;
            run_case(1);
        }
    }
}

/* EndOfParIdentify: the QtEstDelay == 0 teardown, the >= 5 fault latch, and the
 * 1..4 / negative gap in between where only the counter moves. */
static void phase_end(void)
{
    static const int delay[] = { 0, 1, 4, 5, 6, 7, -1 };
    static const unsigned flag[] = { 0, 1, 2, 255 };
    static const unsigned ctl[] = { IDC_FVC_CTL, 0, 2 };
    unsigned a, b, c, d;

    for (a = 0; a < sizeof delay / sizeof delay[0]; a++)
    {
        for (b = 0; b < sizeof flag / sizeof flag[0]; b++)
        {
            for (c = 0; c < sizeof ctl / sizeof ctl[0]; c++)
            {
                for (d = 0; d < 2; d++)
                {
                    clear_inputs();
                    dirty_all();
                    inPar.QtEstDelay = delay[a];
                    inPg.PGErrorFlag = flag[b];
                    inCmd.Command.bit.ControlMode = (Uint)ctl[c];
                    inPar.StatusWord = d ? TUNE_FINISH : TUNE_SUCCESS;
                    run_case(2);
                }
            }
        }
    }
}

/* ---- random sweep -------------------------------------------------------- */

static unsigned long g_rng = 0x20261009UL;

static unsigned long rnd(void)
{
    g_rng = g_rng * 1103515245UL + 12345UL;
    return (g_rng >> 8) & 0xFFFFFFUL;
}

static void phase_random(long n)
{
    long k;

    for (k = 0; k < n; k++)
    {
        int which = (int)(rnd() % 3);
        unsigned i;

        clear_inputs();
        dirty_all();

        inPar.ParEstMstep = (Uint16)(rnd() % 5);
        for (i = 0; i < IDENTIFY_PROGRESS_LENGTH; i++)
        {
            inPar.ParEstContent[i] = (PAR_EST_MAIN_STEP)(rnd() % 10);
        }
        inPar.TuneType = (TUNE_FLOW_ENUM)(rnd() % 24);
        inPar.QtEstDelay = (int)(rnd() % 12) - 2;
        inPar.StatusWord = (TUNE_TO_FUNCTION_WORLD_ENUM)(rnd() % 1001);

        inErr.ErrorCode.all = (unsigned long)rnd();
        inCmd.Command.bit.Start = (Uint)(rnd() & 1u);
        inCmd.Command.bit.ControlMode = (Uint)(rnd() % 4);
        inSt.PrgStatus.all = (Uint)rnd();
        inSt.RunStep = (Uint)(rnd() % 8);

        inPg.PGErrorFlag = (Uint)(rnd() % 256);
        inPg.SpeedDir = (Uint)(rnd() % 4);
        inPg.PGDir = (Uint)(rnd() % 4);

        inUvc.UDivVGet = (Uint)(rnd() % 65536);
        inVc.AcrImKp = (int)rnd();
        inVc.AcrImKi = (int)rnd();
        inVc.AcrItKp = (int)rnd();
        inVc.AcrItKi = (int)rnd();
        inPos.RotorZeroGet = (int)rnd();
        inUvwpg.UvwDir = (Uint)(rnd() % 4);
        inUvwpg.UvwZeroPos_deg = (int)rnd();

        inExtInfo.R1 = (int)rnd();
        inExtInfo.R2 = (int)rnd();
        inExtInfo.L0 = (int)rnd();
        inExtInfo.LM = (int)rnd();
        inExtInfo.I0 = (int)rnd();
        inExtInfo.RsPm = (int)rnd();
        inExtInfo.LD = (int)rnd();
        inExtInfo.LQ = (int)rnd();
        inExtInfo.BemfVolt = (int)rnd();

        run_case(which);
    }
}

/* ---- main ---------------------------------------------------------------- */

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 20000;
    long base;

    init_ptrs();

    phase_run_case();
    printf("  RunCaseGetPar                : %ld\n", g_cases);

    base = g_cases;
    phase_prepare();
    printf("  PrepareParForTune            : %ld\n", g_cases - base);

    base = g_cases;
    phase_end();
    printf("  EndOfParIdentify             : %ld\n", g_cases - base);

    base = g_cases;
    phase_random(n);
    printf("  random mixed                 : %ld\n", g_cases - base);

    printf("MotorParEst difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    return g_fails ? 1 : 0;
}
