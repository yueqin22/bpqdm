/* Shared stand-ins for the f_modbus differential test.
 *
 * f_modbus.c touches no peripheral register and calls no other project function:
 * every effect it has is a write to one of nine globals, and every input it
 * reads is one of the same nine. So the whole hardware boundary is "these
 * objects exist", and the test needs no call log -- comparing the objects after
 * each call observes everything the module can do.
 *
 * The nine objects are declared extern in include/function/f_comm.h and defined
 * in src/function/f_comm.c. That file cannot be linked here: it is 900+ lines of
 * SCIA register programming and pulls in the rest of the communication stack.
 * It is also not needed -- the objects it defines are plain data with no
 * initialisers, so a definition here is byte-identical to what the target
 * starts with (all zero; the target's .bss is zeroed by the boot code).
 *
 * Two of them are easy to get wrong, so they are called out:
 *
 *   sendFrame / rcvFrame / commReadData / sciFlag are written as `LOCALF`, which
 *     f_debug.h resolves to *empty* (its `#if 1` branch), not to `static`. They
 *     are therefore real global definitions, and f_modbus.c's references to them
 *     resolve across translation units -- which is exactly why they have to be
 *     defined here rather than being private to f_comm.c.
 *
 *   funcCode is normally defined in src/function/f_funcCode.c. That file also
 *     defines funcCodeGradeSum[]/funcCodeGradeAll[], whose static initialisers
 *     are differences of two address constants -- MSVC rejects those (C2099),
 *     which is why the f_plc case had to rebuild them. f_modbus reads only
 *     scalar fields of funcCode.code, so the simplest correct thing is to own
 *     the union here and let the driver supply the fields as input. No grade
 *     table is reachable from this module, so none is needed.
 */

#include "f_comm.h"

COMM_SEND_DATA commSendData;
COMM_RCV_DATA  commRcvData;

Uint16 commProtocol;
Uint32 commTicker;

Uint16 sendFrame[35];
Uint16 rcvFrame[35];
Uint16 commReadData[RTU_READ_DATA_NUM_MAX];

union SCI_FLAG sciFlag;

FUNCCODE_ALL funcCode;
