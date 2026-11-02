#ifndef __F_DEBUG_H__
#define __F_DEBUG_H__

#if defined(DSP2803X)
#define DSP_2803X  1
#elif 1
#define DSP_2803X  0
#endif

#if defined(FUNC_DEBUG_RAM)
#define F_DEBUG_RAM             1
#elif 1
#define F_DEBUG_RAM             0
#endif

#if !defined(ZJ_MD380M)
#define DEBUG_F_POSITION_CTRL   0
#elif 1
#define DEBUG_F_POSITION_CTRL   1
#endif

#define DEBUG_F_MOTOR_FUNCCODE    1

#define DEBUG_F_PLC_CTRL          1
#define DEBUG_F_P2P_CTRL          1

#if 1
#define LOCALF
#define LOCALD extern
#else
#define LOCALF static
#define LOCALD static
#endif

#if (DSP_CLOCK == 100)
#define TIME_10US       1000UL
#define TIME_50US       5000UL
#define TIME_100US      10000UL
#define TIME_1MS        100000UL
#elif (DSP_CLOCK == 60)
#define TIME_10US       600UL
#define TIME_50US       3000UL
#define TIME_100US      6000UL
#define TIME_1MS        60000UL
#endif

#define BitAssign(source, bit1, dest, bit2)     \
{                                               \
    if ((source) & (0x1UL << (bit1)))           \
        (dest) |= (0x1UL << (bit2));            \
    else                                        \
        (dest) &= (~(0x1UL << (bit2)));         \
}

Uint16 qsqrt(Uint32 data);

void UniversalDebug(Uint16 sn);

#if F_DEBUG_RAM
void UniversalDebug32(Uint32 a);
void PowerOnDebug(Uint16 value);

extern Uint16 daValue[];
void InitDa(void);
void UpdateDaBuffer(Uint16 port);
void DaOut(Uint16 port);
#endif

#endif
