
#ifndef DSP2803x_GPIO_H
#define DSP2803x_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

struct GPACTRL_BITS {
   Uint16 QUALPRD0:8;
   Uint16 QUALPRD1:8;
   Uint16 QUALPRD2:8;
   Uint16 QUALPRD3:8;
};

union GPACTRL_REG {
   Uint32              all;
   struct GPACTRL_BITS bit;
};

struct GPBCTRL_BITS {
   Uint16 QUALPRD0:8;
   Uint16 rsvd1:8;
   Uint16 rsvd2:16;
};

union GPBCTRL_REG {
   Uint32              all;
   struct GPBCTRL_BITS bit;
};

struct GPA1_BITS {
   Uint16 GPIO0:2;
   Uint16 GPIO1:2;
   Uint16 GPIO2:2;
   Uint16 GPIO3:2;
   Uint16 GPIO4:2;
   Uint16 GPIO5:2;
   Uint16 GPIO6:2;
   Uint16 GPIO7:2;
   Uint16 GPIO8:2;
   Uint16 GPIO9:2;
   Uint16 GPIO10:2;
   Uint16 GPIO11:2;
   Uint16 GPIO12:2;
   Uint16 GPIO13:2;
   Uint16 GPIO14:2;
   Uint16 GPIO15:2;
};

struct GPA2_BITS {
   Uint16 GPIO16:2;
   Uint16 GPIO17:2;
   Uint16 GPIO18:2;
   Uint16 GPIO19:2;
   Uint16 GPIO20:2;
   Uint16 GPIO21:2;
   Uint16 GPIO22:2;
   Uint16 GPIO23:2;
   Uint16 GPIO24:2;
   Uint16 GPIO25:2;
   Uint16 GPIO26:2;
   Uint16 GPIO27:2;
   Uint16 GPIO28:2;
   Uint16 GPIO29:2;
   Uint16 GPIO30:2;
   Uint16 GPIO31:2;
};

struct GPB1_BITS {
   Uint16 GPIO32:2;
   Uint16 GPIO33:2;
   Uint16 GPIO34:2;
   Uint16 GPIO35:2;
   Uint16 GPIO36:2;
   Uint16 GPIO37:2;
   Uint16 GPIO38:2;
   Uint16 GPIO39:2;
   Uint16 GPIO40:2;
   Uint16 GPIO41:2;
   Uint16 GPIO42:2;
   Uint16 GPIO43:2;
   Uint16 GPIO44:2;
   Uint16 rsvd1:6;
};

struct AIO_BITS {
   Uint16 rsvd1:2;
   Uint16 rsvd2:2;
   Uint16 AIO2:2;
   Uint16 rsvd3:2;
   Uint16 AIO4:2;
   Uint16 rsvd4:2;
   Uint16 AIO6:2;
   Uint16 rsvd5:2;
   Uint16 rsvd6:2;
   Uint16 rsvd7:2;
   Uint16 AIO10:2;
   Uint16 rsvd8:2;
   Uint16 AIO12:2;
   Uint16 rsvd9:2;
   Uint16 AIO14:2;
   Uint16 rsvd10:2;
};

union GPA1_REG {
   Uint32              all;
   struct GPA1_BITS    bit;
};

union GPA2_REG {
   Uint32              all;
   struct GPA2_BITS    bit;
};

union GPB1_REG {
   Uint32              all;
   struct GPB1_BITS    bit;
};

union AIO_REG {
   Uint32              all;
   struct AIO_BITS     bit;
};

struct GPADAT_BITS {
   Uint16 GPIO0:1;
   Uint16 GPIO1:1;
   Uint16 GPIO2:1;
   Uint16 GPIO3:1;
   Uint16 GPIO4:1;
   Uint16 GPIO5:1;
   Uint16 GPIO6:1;
   Uint16 GPIO7:1;
   Uint16 GPIO8:1;
   Uint16 GPIO9:1;
   Uint16 GPIO10:1;
   Uint16 GPIO11:1;
   Uint16 GPIO12:1;
   Uint16 GPIO13:1;
   Uint16 GPIO14:1;
   Uint16 GPIO15:1;
   Uint16 GPIO16:1;
   Uint16 GPIO17:1;
   Uint16 GPIO18:1;
   Uint16 GPIO19:1;
   Uint16 GPIO20:1;
   Uint16 GPIO21:1;
   Uint16 GPIO22:1;
   Uint16 GPIO23:1;
   Uint16 GPIO24:1;
   Uint16 GPIO25:1;
   Uint16 GPIO26:1;
   Uint16 GPIO27:1;
   Uint16 GPIO28:1;
   Uint16 GPIO29:1;
   Uint16 GPIO30:1;
   Uint16 GPIO31:1;
};

struct GPBDAT_BITS {
   Uint16 GPIO32:1;
   Uint16 GPIO33:1;
   Uint16 GPIO34:1;
   Uint16 GPIO35:1;
   Uint16 GPIO36:1;
   Uint16 GPIO37:1;
   Uint16 GPIO38:1;
   Uint16 GPIO39:1;
   Uint16 GPIO40:1;
   Uint16 GPIO41:1;
   Uint16 GPIO42:1;
   Uint16 GPIO43:1;
   Uint16 GPIO44:1;
   Uint16 rsvd1:3;
   Uint16 rsvd2:16;
};

struct AIODAT_BITS {
   Uint16 rsvd1:1;
   Uint16 rsvd2:1;
   Uint16 AIO2:1;
   Uint16 rsvd3:1;
   Uint16 AIO4:1;
   Uint16 rsvd4:1;
   Uint16 AIO6:1;
   Uint16 rsvd5:1;
   Uint16 rsvd6:1;
   Uint16 rsvd7:1;
   Uint16 AIO10:1;
   Uint16 rsvd8:1;
   Uint16 AIO12:1;
   Uint16 rsvd9:1;
   Uint16 AIO14:1;
   Uint16 rsvd10:1;
};

union GPADAT_REG {
   Uint32              all;
   struct GPADAT_BITS  bit;
};

union GPBDAT_REG {
   Uint32              all;
   struct GPBDAT_BITS  bit;
};

union AIODAT_REG {
   Uint16			   all;
   struct AIODAT_BITS  bit;
};

struct GPIOXINT_BITS {
    Uint16 GPIOSEL:5;
    Uint16 rsvd1:11;
};

union GPIOXINT_REG {
   Uint16                all;
   struct GPIOXINT_BITS  bit;
};

struct GPIO_CTRL_REGS {
   union  GPACTRL_REG  GPACTRL;
   union  GPA1_REG     GPAQSEL1;
   union  GPA2_REG     GPAQSEL2;
   union  GPA1_REG     GPAMUX1;
   union  GPA2_REG     GPAMUX2;
   union  GPADAT_REG   GPADIR;
   union  GPADAT_REG   GPAPUD;
   Uint32              rsvd1;
   union  GPBCTRL_REG  GPBCTRL;
   union  GPB1_REG     GPBQSEL1;
   Uint32              rsvd2;
   union  GPB1_REG     GPBMUX1;
   Uint32              rsvd3;
   union  GPBDAT_REG   GPBDIR;
   union  GPBDAT_REG   GPBPUD;
   Uint16              rsvd4[24];
   union  AIO_REG      AIOMUX1;
   Uint32              rsvd5;
   union  AIODAT_REG   AIODIR;
   Uint16              rsvd6[5];
};

struct GPIO_DATA_REGS {
   union  GPADAT_REG       GPADAT;
   union  GPADAT_REG       GPASET;
   union  GPADAT_REG       GPACLEAR;
   union  GPADAT_REG       GPATOGGLE;
   union  GPBDAT_REG       GPBDAT;
   union  GPBDAT_REG       GPBSET;
   union  GPBDAT_REG       GPBCLEAR;
   union  GPBDAT_REG       GPBTOGGLE;
   Uint16                  rsvd1[8];
   union  AIODAT_REG       AIODAT;
   Uint16				   rsvd2;
   union  AIODAT_REG       AIOSET;
   Uint16				   rsvd3;
   union  AIODAT_REG       AIOCLEAR;
   Uint16				   rsvd4;
   union  AIODAT_REG       AIOTOGGLE;
   Uint16				   rsvd5;
};

struct GPIO_INT_REGS {
   union  GPIOXINT_REG     GPIOXINT1SEL;
   union  GPIOXINT_REG     GPIOXINT2SEL;
   union  GPIOXINT_REG     GPIOXINT3SEL;
   Uint16                  rsvd1[5];
   union  GPADAT_REG       GPIOLPMSEL;
   Uint16                  rsvd2;
};

extern volatile struct GPIO_CTRL_REGS GpioCtrlRegs;
extern volatile struct GPIO_DATA_REGS GpioDataRegs;
extern volatile struct GPIO_INT_REGS GpioIntRegs;

#ifdef __cplusplus
}
#endif

#endif
