
#include "f_funcCode.h"
#include "f_runSrc.h"
#include "f_main.h"
#include "f_ui.h"
#include "f_debug.h"

#define SCOPE_NUM_KK    1

#if 0
#define SCOPE_NUM0  1
#define SCOPE_NUM1  1
#define SCOPE_NUM2  1
#define SCOPE_NUM3  1
#define SCOPE_NUM4  1
#define SCOPE_NUM5  1
#elif 1
#define SCOPE_NUM0  SCOPE_NUM_KK
#define SCOPE_NUM1  SCOPE_NUM_KK
#define SCOPE_NUM2  SCOPE_NUM_KK
#define SCOPE_NUM3  SCOPE_NUM_KK
#define SCOPE_NUM4  1
#define SCOPE_NUM5  1
#elif 1
#define SCOPE_NUM0  SCOPE_NUM_KK
#define SCOPE_NUM1  SCOPE_NUM_KK
#define SCOPE_NUM2  SCOPE_NUM_KK
#define SCOPE_NUM3  SCOPE_NUM_KK
#define SCOPE_NUM4  SCOPE_NUM_KK
#define SCOPE_NUM5  SCOPE_NUM_KK
#endif

#define SCOPE_START_RUN_CLEAR       1
#define SCOPE_START_RUN_ARRAY_0     1
#define SCOPE_SAVE_ONLY_RUN         1
#define SCOPE_ARRAY_USER_OVER_LOOP  1

#define SCOPE_NUMBER    6

Uint16 scope0[SCOPE_NUM0];
Uint16 scope1[SCOPE_NUM1];
Uint16 scope2[SCOPE_NUM2];
Uint16 scope3[SCOPE_NUM3];
Uint16 scope4[SCOPE_NUM4];
Uint16 scope5[SCOPE_NUM5];
Uint16 scopeIndex[SCOPE_NUMBER];
Uint16 scopeInterval[SCOPE_NUMBER] =
#if 0
{1, 1, 1, 1, 1, 1};
#elif 1
{2, 2, 2, 2, 2, 2};
#elif 0
{4, 4, 4, 4, 4, 4};
#elif 1
{8, 8, 8, 8, 8, 8};
#elif 1
{12, 12, 12, 12, 12, 12};
#elif 1
{16, 16, 16, 16, 16, 16};
#endif

extern int32 frq;
extern int32 frqRun;
extern int32 frqTmp;
extern int32 pcRef;
extern Uint16 pcErrorDisp;
Uint16 matlabDebugRsvd;

extern int32 mPosAct;
extern int32 mVelAct;
extern int32 sPosAct;
extern int32 sVelAct;

extern Uint16 pcRefDisp;
extern Uint16 pcFdbDisp;
extern Uint16 mPosActDisp;
extern Uint16 sPosActDisp;
Uint16 *matlabDebug[SCOPE_NUMBER] =
{
#if 1
    (Uint16 *)&frq,
    (Uint16 *)&frqRun,
    (Uint16 *)&frqTmp,
    (Uint16 *)&matlabDebugRsvd,
    (Uint16 *)&matlabDebugRsvd,
    (Uint16 *)&matlabDebugRsvd,
#elif 0
    (Uint16 *)&gSendToMotor05MsDataBuff[4],
    (Uint16 *)&gSendToFunctionDataBuff[5],
    (Uint16 *)&frq2Core,
    (Uint16 *)&matlabDebugRsvd,
    (Uint16 *)&matlabDebugRsvd,
    (Uint16 *)&matlabDebugRsvd,
#elif 1
    (Uint16 *)&frq,
    (Uint16 *)&frqRun,
    (Uint16 *)&frqTmp,
    (Uint16 *)&pcErrorDisp,
    (Uint16 *)&mVelAct,
    (Uint16 *)&pcRefDisp,

#elif 1
    (Uint16 *)&frqTmp,
    (Uint16 *)&frqRun,
    (Uint16 *)&frq,
    (Uint16 *)&mVelAct,
    (Uint16 *)&pcRefDisp,
    (Uint16 *)&pcErrorDisp,

#elif 1
    (Uint16 *)&mPosActDisp,
    (Uint16 *)&mVelAct,
    (Uint16 *)&sPosActDisp,
    (Uint16 *)&sVelAct,
    (Uint16 *)&pcErrorDisp,
    (Uint16 *)&frqRun,
#endif
};

#define NUM_L_DEBUG   1
LOCALF Uint32 vDL[NUM_L_DEBUG];

LOCALD void InitDebug(void);

#define SPI     1
#define IOSIM   0
#define keyValueDebugIndexMax 1

LOCALF int16 keyValueDebug[keyValueDebugIndexMax];
LOCALF int16 keyValueDebugIndex;
LOCALD void InitKeyDebug(void);
LOCALD void DealKeyDebug(void);

#define I2CDebugNum 1
#define I2C_IO      0
#define I2C_DSP     1

LOCALF Uint16 I2CWrongTimer;
LOCALF Uint16 sendDataI2C[I2CDebugNum];
LOCALF Uint16 rcvDataI2C[I2CDebugNum];
LOCALD void InitI2cDebug(void);
LOCALD void TestI2cDebug(void);
#if 0
LOCALD void pass(void);
LOCALD void fail(void);
#endif

#if 0
void BitAssign(Uint32 source, Uint32 *dest, int16 bit)
{
    if ((source) & (0x1UL << (bit)))
        (*dest) |= (0x1UL << (bit));
    else
        (*dest) &= (~(0x1UL << (bit)));
}
#endif

#if 1

#if 0
#define ATTRIBUTE_READ_AND_WRITE1        0
#define ATTRIBUTE_READ_ONLY_WHEN_RUN1    1
#define ATTRIBUTE_READ_ONLY_ANYTIME1     2
struct  FUNC_ATTRIBUTE_BITS1
{
   Uint16 point:3;

   Uint16 unit:3;

   Uint16 displayBits:3;
   Uint16 upperLimit:1;
   Uint16 lowerLimit:1;
   Uint16 writable:2;
   Uint16 signal:1;
   Uint16 rsvd:2;
};
union FUNC_ATTRIBUTE1
{
   Uint16                       all;
   struct FUNC_ATTRIBUTE_BITS1  bit;
};
#elif 1
#define ATTRIBUTE_READ_AND_WRITE        0
#define ATTRIBUTE_READ_ONLY_WHEN_RUN    1
#define ATTRIBUTE_READ_ONLY_ANYTIME     2
struct  FUNC_ATTRIBUTE_BITS1
{
   Uint16 point:3;

   Uint16 unit:3;

   Uint16 displayBits:3;
   Uint16 upperLimit:1;
   Uint16 lowerLimit:1;
   Uint16 writable:2;
   Uint16 signal:1;
   Uint16 rsvd:2;
};

union FUNC_ATTRIBUTE1
{
   Uint16                      all;
   struct FUNC_ATTRIBUTE_BITS1  bit;
};
#endif

int16 i;

LOCALD void ChgAttr(void);
#endif

#define SCOPE_START_RUN_FLAG        (runFlag.bit.run)

void UniversalDebug(Uint16 sn)
{
    static Uint16 runOld[SCOPE_NUMBER];
    static Uint16 cnt[SCOPE_NUMBER];
    int16 j;

    Uint16 *pScope = scope0;
    Uint16 scopeNum = SCOPE_NUM0;

    if (sn >= SCOPE_NUMBER)
    {
        return;
    }

    switch (sn)
    {
        case 1:
            pScope = scope1;
            scopeNum = SCOPE_NUM1;
            break;

        case 2:
            pScope = scope2;
            scopeNum = SCOPE_NUM2;
            break;

        case 3:
            pScope = scope3;
            scopeNum = SCOPE_NUM3;
            break;

        case 4:
            pScope = scope4;
            scopeNum = SCOPE_NUM4;
            break;

        case 5:
            pScope = scope5;
            scopeNum = SCOPE_NUM5;
            break;

        default:
            break;
    }

    if ((!runOld[sn]) && (SCOPE_START_RUN_FLAG))
    {
        cnt[sn] = 0;

#if SCOPE_START_RUN_ARRAY_0
        scopeIndex[sn] = 0;
#endif

#if SCOPE_START_RUN_CLEAR
        for (j = scopeNum - 1; j >= 0; j--)
        {
            *(pScope + j) = 0;
        }
#endif
    }
    runOld[sn] = SCOPE_START_RUN_FLAG;

#if SCOPE_SAVE_ONLY_RUN
    if (!SCOPE_START_RUN_FLAG)
    {
        return;
    }
#endif

    if (++cnt[sn] >= scopeInterval[sn])
    {
        cnt[sn] = 0;

        if (scopeIndex[sn] < scopeNum)
        {
            *(pScope + scopeIndex[sn]) = *matlabDebug[sn];

            if (++scopeIndex[sn] >= scopeNum)
            {
#if SCOPE_ARRAY_USER_OVER_LOOP
                scopeIndex[sn] = 0;
#elif 1
                scopeIndex[sn] = scopeNum-1;
#endif
            }
        }
    }
}

void UniversalDebug32(Uint32 a)
{
#if 0
    static Uint16 i;

#if 1
    if (!SCOPE_START_RUN_FLAG)
        return;
#endif

    vDL[i] = a;
    if (++i >= NUM_L_DEBUG)
        i = 0;
#endif
}

LOCALF void InitDebug(void)
{
    InitKeyDebug();
    InitI2cDebug();

    ChgAttr();
}

LOCALF void InitKeyDebug(void)
{
#if 0
    for (keyValueDebugIndex = 0;
         keyValueDebugIndex < keyValueDebugIndexMax;
         keyValueDebugIndex++)
        keyValueDebug[keyValueDebugIndex] = 0xffaa;

    keyValueDebugIndex = 0;
#endif
}

LOCALF void DealKeyDebug(void)
{
#if 0
    keyValueDebug[keyValueDebugIndex++] = keyValue;
    if (keyValueDebugIndex >= keyValueDebugIndexMax)
        keyValueDebugIndex = 0;
#endif
}

LOCALF void InitI2cDebug(void)
{
#if 0
    int16 i;

#if 0
    int16 k[I2CDebugNum];

    for (i = 0; i < I2CDebugNum; i++)
        k[i] = rand();
#endif
    for (i = 0; i < I2CDebugNum; i++)
    {

        sendDataI2C[i] = (sendDataI2C[i] + 7) & 0xff;
        rcvDataI2C[i] = 0;
    }
#endif
}

LOCALF void TestI2cDebug(void)
{
#if 0
    int16 i;
    Uint32 j;

    for (i = 0; i < I2CDebugNum; i++)
    {
        if (rcvDataI2C[i] != sendDataI2C[i])
        {
            I2CWrongTimer++;

            for (j = 0; j < 400; j++)
            {
                DELAY_US(25000);
            }
            asm(" nop");
        }
    }
#endif
}

#if 0
LOCALF void pass()
{

    passTimer++;
}

LOCALF void fail()
{

    failTimer++;
}
#endif

void PowerOnDebug(Uint16 value)
{
#if 0
    static Uint16 a,b;

    if (!a)
    {
        a = 1;
        b = value;
    }

    funcCode.group.ff[0]= b;
#endif
}

#if 0
#define DA_SYNC (GpioDataRegs.GPADAT.bit.GPIO23)
#if 0
#define DA_SYNC_DEAL()  \
{                       \
    DA_SYNC = 1;        \
    asm(" nop ");       \
    asm(" nop ");       \
    asm(" nop ");       \
    asm(" nop ");       \
    DA_SYNC = 0;        \
    asm(" nop ");       \
}
#elif 0
#define DA_SYNC_DEAL()  \
{                       \
    DA_SYNC = 1;        \
    DA_SYNC = 0;        \
}
#elif 1
#define DA_SYNC_DEAL()  \
{                       \
    DA_SYNC = 0;        \
    asm(" nop ");       \
    asm(" nop ");       \
    DA_SYNC = 1;        \
    asm(" nop ");       \
    asm(" nop ");       \
    asm(" nop ");       \
    asm(" nop ");       \
    DA_SYNC = 0;        \
    asm(" nop ");       \
    asm(" nop ");       \
}
#endif

void InitDa(void)
{

    EALLOW;

    GpioCtrlRegs.GPAPUD.bit.GPIO24 = 0;
    GpioCtrlRegs.GPAPUD.bit.GPIO26 = 0;
    GpioCtrlRegs.GPAPUD.bit.GPIO23 = 0;

    GpioCtrlRegs.GPAQSEL2.bit.GPIO24 = 3;
    GpioCtrlRegs.GPAQSEL2.bit.GPIO26 = 3;
    GpioCtrlRegs.GPAQSEL2.bit.GPIO23 = 3;

    GpioCtrlRegs.GPAMUX2.bit.GPIO24 = 3;
    GpioCtrlRegs.GPAMUX2.bit.GPIO26 = 3;
    GpioCtrlRegs.GPAMUX2.bit.GPIO23 = 0;

    GpioCtrlRegs.GPADIR.bit.GPIO23 = 1;

    EDIS;

    DA_SYNC = 0;

#if 0

    SpibRegs.SPIFFTX.all=0xE040;
    SpibRegs.SPIFFRX.all=0x405f;
    SpibRegs.SPIFFRX.all=0x205f;
    SpibRegs.SPIFFCT.all=0x0000;
#endif

    SpibRegs.SPICCR.all =0x000F;
    SpibRegs.SPICTL.all =0x0006;

    SpibRegs.SPIBRR = 0x0000;

    SpibRegs.SPICCR.all |= 0x0080;
    SpibRegs.SPIPRI.bit.FREE = 1;
}

#define DA_MAX  4094

void UpdateDaBuffer(Uint16 port)
{
    int16 data = daValue[port];
    if (data > DA_MAX)
        data = DA_MAX;
    data = ((port << 14) | 0x3000) + (data);

    DA_SYNC_DEAL();

    SpibRegs.SPITXBUF = data;
}

void DaOut(Uint16 port)
{
    int16 data = daValue[port];

    if (data > DA_MAX)
        data = DA_MAX;
    data = ((port << 14) | 0x2000) + (data);

    DA_SYNC_DEAL();
    SpibRegs.SPITXBUF = data;
}
#endif

LOCALF void ChgAttr(void)
{
#if 0
    i = sizeof(attributeDebug) - 1;
    for (; i >= 0; i--)
    {
        attributeDebug[i].bit.point = funcCodeAttribute[i].attribute.bit.point;
        attributeDebug[i].bit.unit = funcCodeAttribute[i].attribute.bit.unit;

#if 0
        if (funcCodeAttribute[i].attribute.bit.displayBits == 0)
            attributeDebug[i].bit.displayBits = 0;
        else
            attributeDebug[i].bit.displayBits = funcCodeAttribute[i].attribute.bit.displayBits - 1;
#elif 0
        attributeDebug[i].bit.displayBits = funcCodeAttribute[i].attribute.bit.displayBits;
#elif 1
        attributeDebug[i].bit.displayBits = funcCodeAttribute[i].attribute.bit.displayBits+1;
#endif

        attributeDebug[i].bit.upperLimit = funcCodeAttribute[i].attribute.bit.upperLimit;
        attributeDebug[i].bit.lowerLimit = funcCodeAttribute[i].attribute.bit.lowerLimit;

#if 1
        attributeDebug[i].bit.writable = funcCodeAttribute[i].attribute.bit.writable;
#elif 1
        if (funcCodeAttribute[i].attribute.bit.writable == ATTRIBUTE_READ_AND_WRITE)
            attributeDebug[i].bit.writable = ATTRIBUTE_READ_AND_WRITE1;
        else if (funcCodeAttribute[i].attribute.bit.writable == ATTRIBUTE_READ_ONLY_WHEN_RUN)
            attributeDebug[i].bit.writable = ATTRIBUTE_READ_ONLY_WHEN_RUN1;
        else
            attributeDebug[i].bit.writable = ATTRIBUTE_READ_ONLY_ANYTIME1;
#endif

        attributeDebug[i].bit.signal = funcCodeAttribute[i].attribute.bit.signal;

        attributeDebug[i].bit.rsvd = 0;
    }
#endif
}
