
#ifndef DSP2803x_SYS_CTRL_H
#define DSP2803x_SYS_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct copy_record
{
   unsigned int		 src_pgid;
   unsigned int		 dst_pgid;
   unsigned long	 src_addr;
   unsigned long	 dst_addr;
   unsigned long	 size;
} COPY_RECORD;

typedef struct copy_table
{
   unsigned int		 rec_size;
   unsigned int		 num_recs;
   COPY_RECORD		 recs[1];
} COPY_TABLE;

struct XCLK_BITS   {
   Uint16 XCLKOUTDIV:2;
   Uint16 rsvd1:4;
   Uint16 XCLKINSEL:1;
   Uint16 rsvd3:9;
};

union XCLK_REG {
   Uint16              all;
   struct XCLK_BITS    bit;
};

struct PLLSTS_BITS   {
   Uint16 PLLLOCKS:1;
   Uint16 rsvd1:1;
   Uint16 PLLOFF:1;
   Uint16 MCLKSTS:1;
   Uint16 MCLKCLR:1;
   Uint16 OSCOFF:1;
   Uint16 MCLKOFF:1;
   Uint16 DIVSEL:2;
   Uint16 rsvd2:6;
   Uint16 NORMRDYE:1;
};

union PLLSTS_REG {
   Uint16              all;
   struct PLLSTS_BITS  bit;
};

struct CLKCTL_BITS   {
   Uint16 OSCCLKSRCSEL:1;
   Uint16 OSCCLKSRC2SEL:1;
   Uint16 WDCLKSRCSEL:1;
   Uint16 TMR2CLKSRCSEL:2;
   Uint16 TMR2CLKPRESCALE:3;
   Uint16 INTOSC1OFF:1;
   Uint16 INTOSC1HALTI:1;
   Uint16 INTOSC2OFF:1;
   Uint16 INTOSC2HALTI:1;
   Uint16 WDHALTI:1;
   Uint16 XCLKINOFF:1;
   Uint16 XTALOSCOFF:1;
   Uint16 NMIRESETSEL:1;
};

union CLKCTL_REG {
   Uint16              all;
   struct CLKCTL_BITS  bit;
};

struct INTOSC1TRIM_BITS   {
   Uint16 COARSETRIM:8;
   Uint16 rsvd1:1;
   Uint16 FINETRIM:6;
   Uint16 rsvd2:1;
};

union INTOSC1TRIM_REG {
   Uint16                     all;
   struct INTOSC1TRIM_BITS    bit;
};

struct INTOSC2TRIM_BITS   {
   Uint16 COARSETRIM:8;
   Uint16 rsvd1:1;
   Uint16 FINETRIM:6;
   Uint16 rsvd2:1;
};

union INTOSC2TRIM_REG {
   Uint16                     all;
   struct INTOSC2TRIM_BITS    bit;
};

struct LOSPCP_BITS  {
   Uint16 LSPCLK:3;
   Uint16 rsvd1:13;
};

union LOSPCP_REG {
   Uint16              all;
   struct LOSPCP_BITS  bit;
};

struct PCLKCR0_BITS  {
   Uint16 HRPWMENCLK:1;
   Uint16 LINAENCLK:1;
   Uint16 TBCLKSYNC:1;
   Uint16 ADCENCLK:1;
   Uint16 I2CAENCLK:1;
   Uint16 rsvd1:3;
   Uint16 SPIAENCLK:1;
   Uint16 SPIBENCLK:1;
   Uint16 SCIAENCLK:1;
   Uint16 rsvd3:3;
   Uint16 ECANAENCLK:1;
   Uint16 rsvd5:1;
};

union PCLKCR0_REG {
   Uint16              all;
   struct PCLKCR0_BITS bit;
};

struct PCLKCR1_BITS  {
   Uint16 EPWM1ENCLK:1;
   Uint16 EPWM2ENCLK:1;
   Uint16 EPWM3ENCLK:1;
   Uint16 EPWM4ENCLK:1;
   Uint16 EPWM5ENCLK:1;
   Uint16 EPWM6ENCLK:1;
   Uint16 EPWM7ENCLK:1;
   Uint16 rsvd1:1;
   Uint16 ECAP1ENCLK:1;
   Uint16 rsvd2:5;
   Uint16 EQEP1ENCLK:1;
   Uint16 rsvd3:1;
};

union PCLKCR1_REG {
   Uint16              all;
   struct PCLKCR1_BITS bit;
};

struct PCLKCR3_BITS  {
   Uint16 COMP1ENCLK:1;
   Uint16 COMP2ENCLK:1;
   Uint16 COMP3ENCLK:1;
   Uint16 rsvd1:5;
   Uint16 CPUTIMER0ENCLK:1;
   Uint16 CPUTIMER1ENCLK:1;
   Uint16 CPUTIMER2ENCLK:1;
   Uint16 rsvd2:2;
   Uint16 GPIOINENCLK:1;
   Uint16 CLA1ENCLK:1;
   Uint16 rsvd3:1;
};

union PCLKCR3_REG {
   Uint16              all;
   struct PCLKCR3_BITS bit;
};

struct PLLCR_BITS {
   Uint16 DIV:4;
   Uint16 rsvd1:12;
};

union PLLCR_REG {
   Uint16             all;
   struct PLLCR_BITS  bit;
};

struct LPMCR0_BITS {
   Uint16 LPM:2;
   Uint16 QUALSTDBY:6;
   Uint16 rsvd1:7;
   Uint16 WDINTE:1;
};

union LPMCR0_REG {
   Uint16              all;
   struct LPMCR0_BITS  bit;
};

struct SYS_CTRL_REGS {
   union   XCLK_REG        XCLK;
   union   PLLSTS_REG      PLLSTS;
   union   CLKCTL_REG      CLKCTL;
   Uint16                  PLLLOCKPRD;
   union   INTOSC1TRIM_REG INTOSC1TRIM;
   Uint16                  rsvd1;
   union   INTOSC2TRIM_REG INTOSC2TRIM;
   Uint16                  rsvd2[4];
   union   LOSPCP_REG      LOSPCP;
   union   PCLKCR0_REG     PCLKCR0;
   union   PCLKCR1_REG     PCLKCR1;
   union   LPMCR0_REG      LPMCR0;
   Uint16                  rsvd3;
   union   PCLKCR3_REG     PCLKCR3;
   union   PLLCR_REG       PLLCR;

   Uint16                  SCSR;
   Uint16                  WDCNTR;
   Uint16                  rsvd4;
   Uint16                  WDKEY;
   Uint16                  rsvd5[3];

   Uint16                  WDCR;
   Uint16                  rsvd6[6];
};

struct BORCFG_BITS {
   Uint16 BORENZ:1;
   Uint16 rsvd1:15;
};

union BORCFG_REG {
   Uint16              all;
   struct BORCFG_BITS  bit;
};

struct SYS_PWR_CTRL_REGS {
    union    BORCFG_REG   BORCFG;
    Uint16   rsvd1[2];
};

struct  CSMSCR_BITS {
   Uint16     SECURE:1;
   Uint16     rsvd1:14;
   Uint16     FORCESEC:1;

};

union CSMSCR_REG {
   Uint16             all;
   struct CSMSCR_BITS bit;
};

struct  CSM_REGS {
   Uint16           KEY0;
   Uint16           KEY1;
   Uint16           KEY2;
   Uint16           KEY3;
   Uint16           KEY4;
   Uint16           KEY5;
   Uint16           KEY6;
   Uint16           KEY7;
   Uint16           rsvd1;
   Uint16           rsvd2;
   Uint16           rsvd3;
   Uint16           rsvd4;
   Uint16           rsvd5;
   Uint16           rsvd6;
   Uint16           rsvd7;
   union CSMSCR_REG CSMSCR;
};

struct  CSM_PWL {
   Uint16   PSWD0;
   Uint16   PSWD1;
   Uint16   PSWD2;
   Uint16   PSWD3;
   Uint16   PSWD4;
   Uint16   PSWD5;
   Uint16   PSWD6;
   Uint16   PSWD7;
};

#define FLASH_SLEEP   0x0000;
#define FLASH_STANDBY 0x0001;
#define FLASH_ACTIVE  0x0003;

struct  FOPT_BITS {
   Uint16     ENPIPE:1;
   Uint16     rsvd:15;
};

union FOPT_REG {
   Uint16           all;
   struct FOPT_BITS bit;
};

struct  FPWR_BITS {
   Uint16     PWR:2;
   Uint16     rsvd:14;
};

union FPWR_REG {
   Uint16           all;
   struct FPWR_BITS bit;
};

struct  FSTATUS_BITS {
   Uint16     PWRS:2;
   Uint16     STDBYWAITS:1;
   Uint16     ACTIVEWAITS:1;
   Uint16     rsvd1:4;
   Uint16     V3STAT:1;
   Uint16     rsvd2:7;
};

union FSTATUS_REG {
   Uint16              all;
   struct FSTATUS_BITS bit;
};

struct  FSTDBYWAIT_BITS {
   Uint16     STDBYWAIT:9;
   Uint16     rsvd:7;
};

union FSTDBYWAIT_REG {
   Uint16                 all;
   struct FSTDBYWAIT_BITS bit;
};

struct  FACTIVEWAIT_BITS {
   Uint16     ACTIVEWAIT:9;
   Uint16     rsvd:7;
};

union FACTIVEWAIT_REG {
   Uint16                  all;
   struct FACTIVEWAIT_BITS bit;
};

struct  FBANKWAIT_BITS {
   Uint16     RANDWAIT:4;
   Uint16     rsvd1:4;
   Uint16     PAGEWAIT:4;
   Uint16     rsvd2:4;
};

union FBANKWAIT_REG {
   Uint16                all;
   struct FBANKWAIT_BITS bit;
};

struct  FOTPWAIT_BITS {
   Uint16     OTPWAIT:5;
   Uint16     rsvd:11;
};

union FOTPWAIT_REG {
   Uint16               all;
   struct FOTPWAIT_BITS bit;
};

struct FLASH_REGS {
   union FOPT_REG        FOPT;
   Uint16                rsvd1;
   union FPWR_REG        FPWR;
   union FSTATUS_REG     FSTATUS;
   union FSTDBYWAIT_REG  FSTDBYWAIT;
   union FACTIVEWAIT_REG FACTIVEWAIT;
   union FBANKWAIT_REG   FBANKWAIT;
   union FOTPWAIT_REG    FOTPWAIT;
};

extern volatile struct SYS_CTRL_REGS SysCtrlRegs;
extern volatile struct SYS_PWR_CTRL_REGS SysPwrCtrlRegs;
extern volatile struct CSM_REGS CsmRegs;
extern volatile struct CSM_PWL CsmPwl;
extern volatile struct FLASH_REGS FlashRegs;

#ifdef __cplusplus
}
#endif

#endif
