/* Host-side replacement for f_io.h, reduced to what f_motorFc.c touches.
 *
 * f_motorFc.c reads `diSelectFunc.f2.bit.motorSnDi` and `diFunc.f2.bit.motorSnDi`,
 * so the bitfield layout of struct DI_FUNC matters: the driver writes the input
 * through f2.all and the module must observe it through the named field.
 *
 * The bitfield structs below are copied VERBATIM from include/function/f_io.h
 * (lines 24-96). They are the authoritative layout; nothing here is invented.
 * Both BITS structs total exactly 32 bits, so each union is 4 bytes and
 * struct DI_FUNC is 8 bytes on any compiler that does not straddle a bitfield
 * across its 16-bit allocation unit -- which is also how C28x lays them out.
 * driver.c re-checks the resulting bit position at run time.
 */
#ifndef __F_IO_H__
#define __F_IO_H__

#include "motorfc_common.h"

struct DI_FUNC1_BITS
{
    Uint16 noFunc:1;
    Uint16 fwd:1;
    Uint16 rev:1;
    Uint16 tripleLineCtrl:1;
    Uint16 fwdJog:1;
    Uint16 revJog:1;
    Uint16 up:1;
    Uint16 down:1;

    Uint16 closePwm:1;
    Uint16 errorReset:1;
    Uint16 runPause:1;
    Uint16 externalErrOpenIn:1;
    Uint16 multiSet:4;

    Uint16 accDecTimeSrc:2;
    Uint16 frqSrcSwitch:1;
    Uint16 clearUpDownFrq:1;
    Uint16 localOrRemote:1;
    Uint16 forbidAccDecSpd:1;
    Uint16 pidPause:1;
    Uint16 resetPLC:1;

    Uint16 swingPause:1;
    Uint16 counterTickerIn:1;
    Uint16 resetCounter:1;
    Uint16 lengthTickerIn:1;
    Uint16 resetLengthCounter:1;
    Uint16 forbidTorqueCtrl:1;
    Uint16 pulseIn:1;
    Uint16 motorCtrlMode2Fvc:1;
};
union DI_FUNC1
{
    Uint32                  all;
    struct DI_FUNC1_BITS    bit;
};

struct DI_FUNC2_BITS
{
    Uint16 brake:1;
    Uint16 externalErrCloseIn:1;
    Uint16 frqOk:1;
    Uint16 pidDirRev:1;
    Uint16 stopPanel:1;
    Uint16 diOrComm:1;
    Uint16 pidPauseI:1;
    Uint16 frqXSrc2Preset:1;
    Uint16 frqYSrc2Preset:1;
    Uint16 motorSnDi:2;
    Uint16 pidChg:1;
    Uint16 userError1:1;
    Uint16 userError2:1;
    Uint16 SpdTorqSwitch:1;
    Uint16 emergencyStop:1;
    Uint16 stop4dec:1;
    Uint16 decBrake:1;
    Uint16 clearSetRunTime:1;
    Uint16 rsvd2:13;
};
union DI_FUNC2
{
    Uint32                  all;
    struct DI_FUNC2_BITS    bit;
};

struct DI_FUNC
{
    union DI_FUNC1  f1;
    union DI_FUNC2  f2;
};

extern struct DI_FUNC diFunc;
extern struct DI_FUNC diSelectFunc;

#endif
