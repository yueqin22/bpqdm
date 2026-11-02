/* Shared mock for the f_dspcan differential test.
 *
 * The module owns no memory except two flags; everything else it touches is a
 * peripheral register block. Both translation units therefore see the SAME
 * register memory, which is what makes "the rewrite stopped writing CANME" or
 * "wrote the wrong bit into CANTRS" visible.
 *
 * The structs come from the real device headers (see real.txt), so the bitfield
 * layout is the chip's. That matters here more than usual: the module aliases
 * the mailbox block as a flat `Uint32 *` and walks it with a stride of four
 * 32-bit words per mailbox (`MsgCtrlPi[i<<2]`). If MBOX0's layout were imitated
 * rather than real, the stride would be testing the imitation.
 *
 * gHostCpuCtl is the EALLOW/EDIS instrumentation -- see shim/DSP28x_Project.h.
 */

#include "DSP28x_Project.h"
#include "f_dspcan.h"

volatile struct ECAN_REGS       ECanaRegs;
volatile struct ECAN_MBOXES     ECanaMboxes;
volatile struct LAM_REGS        ECanaLAMRegs;
volatile struct GPIO_CTRL_REGS  GpioCtrlRegs;

volatile Uint16 gHostCpuCtl[7];
