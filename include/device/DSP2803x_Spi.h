
#ifndef DSP2803x_SPI_H
#define DSP2803x_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

struct  SPIFFTX_BITS {
   Uint16 TXFFIL:5;
   Uint16 TXFFIENA:1;
   Uint16 TXFFINTCLR:1;
   Uint16 TXFFINT:1;
   Uint16 TXFFST:5;
   Uint16 TXFIFO:1;
   Uint16 SPIFFENA:1;
   Uint16 SPIRST:1;
};

union SPIFFTX_REG {
   Uint16               all;
   struct SPIFFTX_BITS  bit;
};

struct  SPIFFRX_BITS {
   Uint16 RXFFIL:5;
   Uint16 RXFFIENA:1;
   Uint16 RXFFINTCLR:1;
   Uint16 RXFFINT:1;
   Uint16 RXFFST:5;
   Uint16 RXFIFORESET:1;
   Uint16 RXFFOVFCLR:1;
   Uint16 RXFFOVF:1;

};

union SPIFFRX_REG {
   Uint16               all;
   struct SPIFFRX_BITS  bit;
};

struct  SPIFFCT_BITS {
   Uint16 TXDLY:8;
   Uint16 rsvd:8;
};

union SPIFFCT_REG {
   Uint16               all;
   struct SPIFFCT_BITS  bit;
};

struct  SPICCR_BITS {
   Uint16 SPICHAR:4;
   Uint16 SPILBK:1;
   Uint16 rsvd1:1;
   Uint16 CLKPOLARITY:1;
   Uint16 SPISWRESET:1;
   Uint16 rsvd2:8;
};

union SPICCR_REG {
   Uint16              all;
   struct SPICCR_BITS  bit;
};

struct  SPICTL_BITS {
   Uint16 SPIINTENA:1;
   Uint16 TALK:1;
   Uint16 MASTER_SLAVE:1;
   Uint16 CLK_PHASE:1;
   Uint16 OVERRUNINTENA:1;
   Uint16 rsvd:11;
};

union SPICTL_REG {
   Uint16              all;
   struct SPICTL_BITS  bit;
};

struct  SPISTS_BITS {
   Uint16 rsvd1:5;
   Uint16 BUFFULL_FLAG:1;
   Uint16 INT_FLAG:1;
   Uint16 OVERRUN_FLAG:1;
   Uint16 rsvd2:8;
};

union SPISTS_REG {
   Uint16              all;
   struct SPISTS_BITS  bit;
};

struct  SPIPRI_BITS {
   Uint16 TRIWIRE:1;
   Uint16 STEINV:1;
   Uint16 rsvd1:2;
   Uint16 FREE:1;
   Uint16 SOFT:1;
   Uint16 PRIORITY:1;
   Uint16 rsvd2:9;
};

union SPIPRI_REG {
   Uint16              all;
   struct SPIPRI_BITS  bit;
};

struct  SPI_REGS {
   union SPICCR_REG     SPICCR;
   union SPICTL_REG     SPICTL;
   union SPISTS_REG     SPISTS;
   Uint16               rsvd1;
   Uint16               SPIBRR;
   Uint16               rsvd2;
   Uint16               SPIRXEMU;
   Uint16               SPIRXBUF;
   Uint16               SPITXBUF;
   Uint16               SPIDAT;
   union SPIFFTX_REG    SPIFFTX;
   union SPIFFRX_REG    SPIFFRX;
   union SPIFFCT_REG    SPIFFCT;
   Uint16               rsvd3[2];
   union SPIPRI_REG     SPIPRI;
};

extern volatile struct SPI_REGS SpiaRegs;
extern volatile struct SPI_REGS SpibRegs;

#ifdef __cplusplus
}
#endif

#endif
