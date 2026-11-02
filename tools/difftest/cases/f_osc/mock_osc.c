/* Shared mock state for the f_osc differential bench.
 *
 * One instance of each object, read by BOTH sides, so ref and new observe the
 * same inputs.  Only the module's own externally-linked symbols are duplicated
 * (that happens in ref_tu.c / new_tu.c); everything here is input surface.
 *
 * Two groups:
 *
 *   1. Peripheral register objects.  f_osc.c drives real hardware: InitSciOSC
 *      writes nine SCI registers, four PIE fields and two vector slots;
 *      SciDataTX and both ISRs write SCITXBUF; sciaRxFifoIsr READS
 *      SCIRXST.bit.RXERROR and SCIRXBUF.bit.RXDT; OscSciIoInit writes six
 *      GpioCtrlRegs fields plus SysCtrlRegs.PCLKCR0.  The register STRUCTS come
 *      from the real device headers (pulled in through the case-private
 *      DSP2803x_Device.h) so the bitfield layouts under test are the target
 *      ones; only the storage is instantiated here.
 *
 *   2. The four external functions and three globals the module calls out to
 *      (nm2000 -u on f_osc.obj: CrcValueByteCalc / CommRead / CommWrite and
 *      funcCode / sciFlag / commRcvData / commReadData).
 *
 * SCIRXBUF is ordinary memory here, not a FIFO.  sciaRxFifoIsr reads it four
 * times in a row, so on this bench the four bytes it stores are always equal.
 * That is a modelling limit, not a bug in the module -- see
 * shim/osc_state.h for how the frame dispatcher is driven instead.
 */

#include "f_funcCode.h"
#include "f_comm.h"

/* ------------------------------------------------------------------------- */
/* 1. 外设寄存器实例                                                          */
/* ------------------------------------------------------------------------- */

volatile struct SCI_REGS        SciaRegs;
volatile struct PIE_CTRL_REGS   PieCtrlRegs;
struct          PIE_VECT_TABLE  PieVectTable;
volatile struct GPIO_CTRL_REGS  GpioCtrlRegs;
volatile struct GPIO_DATA_REGS  GpioDataRegs;
volatile struct SYS_CTRL_REGS   SysCtrlRegs;

/* CPU control sequence counters (see the case-private DSP2803x_Device.h:
 * EALLOW / EDIS / DINT / EINT / ERTM / DRTM / ESTOP0 all bump a slot here).
 * Keeping the counts in the compared image is what makes "the rewrite dropped
 * an EDIS" or "the rewrite merged two EALLOW/EDIS pairs" visible -- comparing
 * only results never shows it. */
volatile Uint16 gHostCpuCtl[7];

/* `extern cregister volatile unsigned int IER;` on the target.  InitSciOSC does
 * `IER |= M_INT9`, so it is an output: the driver preloads a value and the
 * post-call value is compared. */
volatile Uint16 IER;

/* ------------------------------------------------------------------------- */
/* 2. 模块的对外依赖                                                          */
/* ------------------------------------------------------------------------- */

FUNCCODE_ALL      funcCode;
union SCI_FLAG    sciFlag;
COMM_RCV_DATA     commRcvData;
Uint16            commReadData[RTU_READ_DATA_NUM_MAX];

/* Driver-controlled return values.  Both sides see the same value, so they are
 * inputs, not per-side state. */
Uint16 g_commReadRet;
Uint16 g_commWriteRet;

/* Real CRC-16/Modbus, copied verbatim from src/function/f_common.c (the live
 * `#if (NEW_CRC == 1)` branch) so the driver can build frames whose CRC is
 * accepted and, by flipping one byte, frames whose CRC is rejected.  A stand-in
 * algorithm would work too, but then the bench would be exercising a CRC that
 * the firmware never computes. */
static const Uint16 crc16Table[16] =
{
    0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
    0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400,
};

Uint16 CrcValueByteCalc(const Uint16 *data, Uint16 length)
{
    Uint16 crcValue = 0xFFFF;
    Uint16 tmp;
    const Uint16 *pData;

    if (length == 0)
    {
        return (crcValue);
    }

    pData = data;

    do
    {
        tmp = crc16Table[(crcValue ^ *pData++) & 0x00FF];
        crcValue = ((tmp & 0x00FF) << 8) + ((tmp ^ crcValue) >> 8);
    }
    while (--length);

    return (crcValue);
}

/* CommRead / CommWrite are stubs on purpose: f_comm.c is not the module under
 * test.  Their return values are folded through addr / data / the call-time
 * value of commRcvData.commCmdSaveEeprom, so a rewrite that passes the wrong
 * address, the wrong data, or forgets to raise the "save to EEPROM" flag is
 * visible in OscControlWriter's return value even before the state comparison
 * runs. */
Uint16 CommRead(Uint16 addr, Uint16 data)
{
    /* `data` 必须参与结果。第一版写的是 (void)data, 于是
     * `oscReturn = CommRead(addr, 1)` 里的 1 改成 0 完全不可观测 —— CR1 就这么
     * MISSED 了。第二个实参是调用方的输入, 不是可以丢掉的装饰。 */
    commReadData[0] = (Uint16)(g_commReadRet + addr + (Uint16)(data * 13u));
    return (Uint16)(g_commReadRet ^ (Uint16)(addr * 7u));
}

Uint16 CommWrite(Uint16 addr, Uint16 data)
{
    Uint16 v;

    v  = (Uint16)(g_commWriteRet ^ (Uint16)(addr * 31u));
    v ^= (Uint16)(data * 17u);
    v ^= (Uint16)(commRcvData.commCmdSaveEeprom << 12);

    return v;
}
