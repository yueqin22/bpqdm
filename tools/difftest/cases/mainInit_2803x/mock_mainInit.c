/* Shared mock for the mainInit_2803x differential test.
 *
 * One instance of everything the module touches but does NOT define. Both
 * translation units see the same memory, so a rewrite that stops writing a
 * register, writes a different constant, or drops a callee call, changes what
 * the driver compares.
 *
 * Six groups:
 *
 *   1. peripheral registers   SysCtrlRegs / FlashRegs / AdcRegs / PieCtrlRegs /
 *      PieVectTable / GpioCtrlRegs / GpioDataRegs / CpuTimer1Regs. The structs
 *      come from the real device headers (see real.txt), so bitfield layout is
 *      the chip's, not an imitation.
 *   2. IER / IFR              plain volatile words standing in for the two
 *      cregister objects (see shim/DSP2803x_Device.h).
 *   3. the CPU-control counter table + event log   see mock_mainInit.h.
 *   4. callees                InitSetPWM / InitCpuTimers log an
 *      event; MovePrgFrFlashToRam additionally records its three arguments, so
 *      the copy_prg arithmetic (size - 1, src/dst passthrough) is observable.
 *   5. the copy table         prginRAM, whose fields the driver fills per case.
 *   6. Device_cal             the module calls it through a function pointer at
 *      0x3D7C80; the host patch redirects it here, so the "call the calibration
 *      routine" step stays observable instead of jumping into no man's land.
 */
#include <string.h>

#include "DSP2803x_Device.h"
#include "mock_mainInit.h"

/* ---- 1. peripheral registers ------------------------------------------- */
volatile struct SYS_CTRL_REGS   SysCtrlRegs;
volatile struct FLASH_REGS      FlashRegs;
volatile struct ADC_REGS        AdcRegs;
volatile struct PIE_CTRL_REGS   PieCtrlRegs;
struct PIE_VECT_TABLE  PieVectTable;
volatile struct GPIO_CTRL_REGS  GpioCtrlRegs;
volatile struct GPIO_DATA_REGS  GpioDataRegs;
volatile struct CPUTIMER_REGS   CpuTimer1Regs;

/* ---- 2. IER / IFR ------------------------------------------------------- */
volatile Uint16 gHostIER;
volatile Uint16 gHostIFR;

/* ---- 3. control counters, event log, fingerprint ------------------------ */
volatile Uint16 gHostCpuCtl[7];

int  gHostLogN;
int  gHostLogId[HOST_LOG_MAX];
unsigned long gHostLogFp[HOST_LOG_MAX];

int  gHostMoveN;
unsigned long gHostMoveSrc[HOST_MOVE_MAX];
unsigned long gHostMoveDst[HOST_MOVE_MAX];
unsigned long gHostMoveSize[HOST_MOVE_MAX];
unsigned long gHostMoveIx[HOST_MOVE_MAX];

int gHostDevCalN;

/* FNV-1a over a byte range. */
static unsigned long fp_mix(unsigned long h, const void *p, unsigned n)
{
    const unsigned char *b = (const unsigned char *)p;
    unsigned i;
    for (i = 0; i < n; i++)
    {
        h ^= (unsigned long)b[i];
        h *= 16777619UL;
    }
    return h;
}

/* Classify one vector-table slot without looking at which side is running.
 *
 * PieVectTable holds function pointers, and 123 of them are filled with
 * rsvd_ISR -- which is DEFINED BY THE MODULE, hence renamed per side, hence a
 * different address on each side. Hashing the raw table would make the two
 * sides differ on every single event even when they behave identically.
 *
 * So each slot is reduced to: which of the five shared stubs it holds (1..5),
 * or "holds the module's own handler" (9). A slot holding anything else (the
 * driver's prefill junk) also maps to 9 -- and the driver compares those raw,
 * so a rewrite that leaves a slot untouched still shows up there.
 */
static unsigned long fp_vect(void)
{
    unsigned char cls[128];
    unsigned long h = 2166136261UL;
    int i;

    for (i = 0; i < 128; i++)
    {
        void *p = *(void **)((char *)&PieVectTable + (unsigned)i * sizeof(void *));
        if (p == (void *)ADC_Over_isr)        cls[i] = 1;
        else if (p == (void *)EPWM1_TZ_isr)   cls[i] = 2;
        else if (p == (void *)EPWM1_zero_isr) cls[i] = 3;
        else if (p == (void *)SCI_RXD_isr)    cls[i] = 4;
        else if (p == (void *)SCI_TXD_isr)    cls[i] = 5;
        else                                  cls[i] = 9;
    }
    return fp_mix(h, cls, sizeof cls);
}

/* Fingerprint of state the driver does not compare per field: the six register
 * blocks plus IER/IFR plus the vector-table classification. Cheap enough to run
 * per event (a few hundred bytes), and it is what makes the ORDER of a register
 * write relative to an EALLOW/EDIS observable. */
static unsigned long fp_all(void)
{
    unsigned long h = 2166136261UL;
    h = fp_mix(h, (const void *)&SysCtrlRegs,   sizeof(struct SYS_CTRL_REGS));
    h = fp_mix(h, (const void *)&FlashRegs,     sizeof(struct FLASH_REGS));
    h = fp_mix(h, (const void *)&AdcRegs,       sizeof(struct ADC_REGS));
    h = fp_mix(h, (const void *)&PieCtrlRegs,   sizeof(struct PIE_CTRL_REGS));
    h = fp_mix(h, (const void *)&GpioCtrlRegs,  sizeof(struct GPIO_CTRL_REGS));
    h = fp_mix(h, (const void *)&GpioDataRegs,  sizeof(struct GPIO_DATA_REGS));
    h = fp_mix(h, (const void *)&CpuTimer1Regs, sizeof(struct CPUTIMER_REGS));
    h = fp_mix(h, (const void *)&gHostIER,      sizeof gHostIER);
    h = fp_mix(h, (const void *)&gHostIFR,      sizeof gHostIFR);
    h = h * 31UL + fp_vect();
    return h;
}

/* Every EALLOW/EDIS/DINT/EINT goes through here: bump the counter AND append an
 * event with the current register fingerprint. Both halves matter -- the counter
 * alone cannot tell "EALLOW ... EDIS" from "EDIS ... EALLOW", and the events
 * alone cannot tell how many times each ran. */
void gHostCtlEvent(int idx)
{
    if (idx >= 0 && idx < 7)
    {
        gHostCpuCtl[idx] = (Uint16)(gHostCpuCtl[idx] + 1);
    }
    gHostLogCall(HOST_EV_EINT + idx);
}

void gHostLogCall(int id)
{
    if (gHostLogN < HOST_LOG_MAX)
    {
        gHostLogId[gHostLogN] = id;
        gHostLogFp[gHostLogN] = fp_all();
        gHostLogN++;
    }
}

void gHostResetLog(void)
{
    gHostLogN = 0;
    gHostMoveN = 0;
    gHostDevCalN = 0;
    memset((void *)gHostCpuCtl, 0, sizeof gHostCpuCtl);
}

/* ---- 6. Device_cal ------------------------------------------------------ */
void gHostDeviceCal(void)
{
    gHostDevCalN++;
    gHostLogCall(HOST_EV_DEVCAL);
}

/* ---- 4. callees of InitPeripherals -------------------------------------- */
void InitSetPWM(void)
{
    gHostLogCall(HOST_EV_SETPWM);
}

void InitCpuTimers(void)
{
    gHostLogCall(HOST_EV_CPUTIMERS);
}

/* The real one lives in system/DSP2803x_CodeStartBranch.asm and copies the
 * ramfuncs overlay out of flash. Only its arguments matter here. */
void MovePrgFrFlashToRam(unsigned long src, unsigned long dst,
                         unsigned long size)
{
    if (gHostMoveN < HOST_MOVE_MAX)
    {
        gHostMoveSrc[gHostMoveN]  = src;
        gHostMoveDst[gHostMoveN]  = dst;
        gHostMoveSize[gHostMoveN] = size;
        gHostMoveIx[gHostMoveN]   = (unsigned long)gHostMoveN;
        gHostMoveN++;
    }
    gHostLogCall(HOST_EV_MOVEPRG);
}

/* ---- 5. the copy table -------------------------------------------------- */
/* Storage for the extern the module declares. The linker-generated original is
 * filled in by the .cmd file at load time; here the driver owns the contents so
 * that size / src / dst are inputs, not constants. */
COPY_TABLE prginRAM;

/* ---- the five interrupt vectors the module installs --------------------- */
void ADC_Over_isr(void)
{
    gHostLogCall(30);
}

void EPWM1_TZ_isr(void)
{
    gHostLogCall(31);
}

void EPWM1_zero_isr(void)
{
    gHostLogCall(32);
}

void SCI_RXD_isr(void)
{
    gHostLogCall(33);
}

void SCI_TXD_isr(void)
{
    gHostLogCall(34);
}
