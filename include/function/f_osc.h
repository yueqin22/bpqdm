
#ifndef	 _sci_osc_h_
#define	 _sci_osc_h_

#include "f_funcCode.h"

#define			OSC_SCI_SEL						1
#define			OSC_CON_CHECK					1
#define			OSC_DATA_CHECK					1
#define			FC_CODE_CONTROL_EN				1
#define			OSC_TX_INT_EN					1
#define			SCI_INT_LOAD_RAM				0

#if DSP_2803X
#define			PERIPHERAL_CLK					15E6
#else
#define			PERIPHERAL_CLK					25E6
#endif

#define			SCI_TX_INT_SPEEDUP				1

#if DSP_2803X
    #define			RS485_ENABLE			    0
#else
    #define			RS485_ENABLE			    1
#endif

#if RS485_ENABLE == 1

    #if DSP_2803X
    #define RS485_RTS_O (GpioDataRegs.GPBDAT.bit.GPIO39)
    #else
    #define RS485_RTS_O (GpioDataRegs.GPADAT.bit.GPIO27)
    #endif
    #define RS485_R_O     0
	#define RS485_T_O     1

#endif

#if 	(1 == OSC_SCI_SEL)
	#define			SCI_OSC_REGS					SciaRegs
#else
	#define			SCI_OSC_REGS					ScibRegs
#endif

#define			OSC_CON_FRAME_HEAD				0xAC
#define			OSC_DATA_FRAME_HEAD_A			0xA5
#define			OSC_DATA_FRAME_HEAD_B			0xCD

#define			OSC_BUF_DATA_LEN				64
#define			FC_FRAME_LEN					8

#define			OSC_SCI_EN						0xCA

#define			SCI_RT_BUSY						0x03
#define			SCI_RS485_TX_BUSY				0x04

#define         FC_COMM_TEST                    0x30
#define			FC_CHANNL_SEL					0x31
#define			FC_PARA_CFG						0x32

#define			FC_START_OSC					0x33
#define			FC_STOP_OSC						0x34
#define			FC_OSC_OFF						0x35

#define			FC_READ_FC_DATA					0x41
#define			FC_WRITE_FC_DATA				0x42

#define			CONTROL_FRAME_HEAD				OscConFrameBuf[0]
#define			CONTROL_FRAME_FC				OscConFrameBuf[1]
#define			CONTROL_FRAME_P1				OscConFrameBuf[2]
#define			CONTROL_FRAME_P2				OscConFrameBuf[3]
#define			CONTROL_FRAME_P3				OscConFrameBuf[4]
#define			CONTROL_FRAME_P4				OscConFrameBuf[5]
#define			CONTROL_FRAME_CRCL				OscConFrameBuf[6]
#define			CONTROL_FRAME_CRCH				OscConFrameBuf[7]

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

typedef unsigned char  uint8;
typedef signed   char  int8;
typedef unsigned int   uint16;

typedef unsigned  long uint32;

typedef float          fp32;
typedef double         fp64;

typedef struct
{
	uint8	status;
    uint8	baudRate;
    uint8 	interval;
    uint8	runContinue;
    uint8	ch1Addr;
    uint8	ch2Addr;
    uint8	ch3Addr;
    uint8	ch4Addr;
	uint8   chSum;
} DSP_OSC_CON_DATA;

typedef	struct
{
	uint8	frameHead1;
	uint8 	frameHead2;
	uint8 	frameNum;
	uint8	oscDataBuf[OSC_BUF_DATA_LEN];
	uint8	check;
	uint8	rwPI;
	uint8	full;
} OSC_DATA_TYPE;

typedef struct
{
	uint8 *buf;
	uint8 len;
	uint8 busy;
	uint8 rxflag;

} SCI_RT_CON_DATA;

extern void OscSciFunction(void);

#endif
