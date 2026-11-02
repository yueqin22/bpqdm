#ifndef __F_DISPLAY_KEY_H__
#define __F_DISPLAY_KEY_H__

#include "f_funcCode.h"

#define DISPLAY_8LED_NUM    5
#define DISPLAY_LED_NUM     7

#define DISPLAY_8LED_CODE_NUM       38

#define DISPLAY_0       0
#define DISPLAY_1       1
#define DISPLAY_2       2
#define DISPLAY_3       3
#define DISPLAY_4       4

#define DISPLAY_5       5
#define DISPLAY_6       6
#define DISPLAY_7       7
#define DISPLAY_8       8
#define DISPLAY_9       9

#define DISPLAY_A       10
#define DISPLAY_B       11
#define DISPLAY_C       12
#define DISPLAY_D       13
#define DISPLAY_E       14

#define DISPLAY_F       15
#define DISPLAY_P       16
#define DISPLAY_H       17
#define DISPLAY_J       18
#define DISPLAY_L       19

#define DISPLAY_n       20
#define DISPLAY_r       21
#define DISPLAY_U       22
#define DISPLAY_y       23
#define DISPLAY_G       24

#define DISPLAY_h       25
#define DISPLAY_I       26
#define DISPLAY_N       27
#define DISPLAY_o       28
#define DISPLAY_q       29

#define DISPLAY_S       5
#define DISPLAY_d       13
#define DISPLAY_b       11

#define DISPLAY_T       30
#define DISPLAY_t       31
#define DISPLAY_u       32
#define DISPLAY_NULL    33
#define DISPLAY_LINE    34

#define DISPLAY_DOT     35
#define DISPLAY_ALL     36

#define DISPLAY_c      (DISPLAY_8LED_CODE_NUM-1)

#define DISPLAY_LED_CODE_NUM    11

#define LED_RUN      0
#define LED_LOCAL    1
#define LED_DIR      2
#define LED_TUNE     3
#define LED_V        4
#define LED_A        5
#define LED_HZ       6
#define LED_RPM      7
#define LED_PERCENT  8
#define LED_ALL      9
#define LED_NULL     (DISPLAY_LED_CODE_NUM-1)

#define KEY_NUM     9
#define KEY_PRG     (0+1)
#define KEY_QUICK   (1+1)
#define KEY_RUN     (2+1)
#define KEY_MFK     (3+1)
#define KEY_STOP    (4+1)
#define KEY_DOWN    (5+1)
#define KEY_SHIFT   (6+1)
#define KEY_ENTER   (7+1)
#define KEY_UP      (8+1)

#define KEY_FWD_JOG (0x93)
#define KEY_REV_JOG (0x94)
#define KEY_REV     (0x91)
#define KEY_SWITCH  (0x92)
#define KEY_NO_KEY  0

#if DSP_2803X
#define RCK (GpioDataRegs.GPADAT.bit.GPIO22)
#define KEYSEL (GpioDataRegs.AIODAT.bit.AIO4)
#else
#define RCK (GpioDataRegs.GPADAT.bit.GPIO19)
#endif

extern Uint16 keyBordTestFlag;
extern Uint16 keyBordValue;

extern const Uint16 DISPLAY_CODE[DISPLAY_8LED_CODE_NUM];
extern const Uint16 LED_CODE[DISPLAY_LED_CODE_NUM];
extern Uint16 scanStep;
extern Uint16 displayBuffer[];

#define SPI_BYTE    3

extern Uint16 upDownDelta;

extern Uint16 factoryPwd;

extern Uint16 keyFunc;
extern Uint16 keyPreCtrl;

void InitSetSpiForFunc(void);
void UpdateDisplayBuffer(void);
void WinkDeal(void);
void DisplayScanPrepare(void);
void DisplayScan(void);
void KeyScan(void);
void KeyProcess(void);
extern Uint16 keySelStatus;

#if DSP_2803X
#define SPI_LENGTH  1
#else
#define SPI_LENGTH  2
#endif

struct SPI_OUT_A_BITS
{
#if DSP_2803X
    Uint16 dispCode:8;
    Uint16 dispSect:8;
#else
    Uint16 doData:8;
    Uint16 dispCode:8;
#endif
};
union SPI_OUT_A
{
    Uint16              all;
    struct SPI_OUT_A_BITS bit;
};

struct SPI_OUT_B_BITS
{
    Uint16 dispSect:8;
    Uint16 rsvd:8;
};
union SPI_OUT_B
{
    Uint16              all;
    struct SPI_OUT_B_BITS bit;
};

struct SPI_OUT
{
    union SPI_OUT_A a;
    union SPI_OUT_B b;
};

union SPI_IN
{
    Uint16 all[SPI_LENGTH];

#if DSP_2803X
    Uint16 key;
#else
    struct SPI_IN_STRUCT
    {
        Uint16 di;
        Uint16 key;
    } c;
#endif
};

struct SPI_IN_OUT
{
    union SPI_IN in;
    struct SPI_OUT out;
};

extern struct SPI_IN_OUT spiData;

#endif
