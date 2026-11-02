
#ifndef DSP2803x_SCI_H
#define DSP2803x_SCI_H

#ifdef __cplusplus
extern "C" {
#endif

struct  SCICCR_BITS {
   Uint16 SCICHAR:3;
   Uint16 ADDRIDLE_MODE:1;
   Uint16 LOOPBKENA:1;
   Uint16 PARITYENA:1;
   Uint16 PARITY:1;
   Uint16 STOPBITS:1;
   Uint16 rsvd1:8;
};

union SCICCR_REG {
   Uint16              all;
   struct SCICCR_BITS  bit;
};

struct  SCICTL1_BITS {
   Uint16 RXENA:1;
   Uint16 TXENA:1;
   Uint16 SLEEP:1;
   Uint16 TXWAKE:1;
   Uint16 rsvd:1;
   Uint16 SWRESET:1;
   Uint16 RXERRINTENA:1;
   Uint16 rsvd1:9;

};

union SCICTL1_REG {
   Uint16               all;
   struct SCICTL1_BITS  bit;
};

struct  SCICTL2_BITS {
   Uint16 TXINTENA:1;
   Uint16 RXBKINTENA:1;
   Uint16 rsvd:4;
   Uint16 TXEMPTY:1;
   Uint16 TXRDY:1;
   Uint16 rsvd1:8;

};

union SCICTL2_REG {
   Uint16               all;
   struct SCICTL2_BITS  bit;
};

struct  SCIRXST_BITS {
   Uint16 rsvd:1;
   Uint16 RXWAKE:1;
   Uint16 PE:1;
   Uint16 OE:1;
   Uint16 FE:1;
   Uint16 BRKDT:1;
   Uint16 RXRDY:1;
   Uint16 RXERROR:1;

};

union SCIRXST_REG {
   Uint16               all;
   struct SCIRXST_BITS  bit;
};

struct  SCIRXBUF_BITS {
   Uint16 RXDT:8;
   Uint16 rsvd:6;
   Uint16 SCIFFPE:1;
   Uint16 SCIFFFE:1;
};

union SCIRXBUF_REG {
   Uint16                all;
   struct SCIRXBUF_BITS  bit;
};

struct  SCIPRI_BITS {
   Uint16 rsvd:3;
   Uint16 FREE:1;
   Uint16 SOFT:1;
   Uint16 rsvd1:3;
};

union SCIPRI_REG {
   Uint16              all;
   struct SCIPRI_BITS  bit;
};

struct  SCIFFTX_BITS {
   Uint16 TXFFIL:5;
   Uint16 TXFFIENA:1;
   Uint16 TXFFINTCLR:1;
   Uint16 TXFFINT:1;
   Uint16 TXFFST:5;
   Uint16 TXFIFOXRESET:1;
   Uint16 SCIFFENA:1;
   Uint16 SCIRST:1;

};

union SCIFFTX_REG {
   Uint16               all;
   struct SCIFFTX_BITS  bit;
};

struct  SCIFFRX_BITS {
   Uint16 RXFFIL:5;
   Uint16 RXFFIENA:1;
   Uint16 RXFFINTCLR:1;
   Uint16 RXFFINT:1;
   Uint16 RXFFST:5;
   Uint16 RXFIFORESET:1;
   Uint16 RXFFOVRCLR:1;
   Uint16 RXFFOVF:1;

};

union SCIFFRX_REG {
   Uint16               all;
   struct SCIFFRX_BITS  bit;
};

struct  SCIFFCT_BITS {
   Uint16 FFTXDLY:8;
   Uint16 rsvd:5;
   Uint16 CDC:1;
   Uint16 ABDCLR:1;
   Uint16 ABD:1;
};

union SCIFFCT_REG {
   Uint16               all;
   struct SCIFFCT_BITS  bit;
};

struct  SCI_REGS {
   union SCICCR_REG     SCICCR;
   union SCICTL1_REG    SCICTL1;
   Uint16               SCIHBAUD;
   Uint16               SCILBAUD;
   union SCICTL2_REG    SCICTL2;
   union SCIRXST_REG    SCIRXST;
   Uint16               SCIRXEMU;
   union SCIRXBUF_REG   SCIRXBUF;
   Uint16               rsvd1;
   Uint16               SCITXBUF;
   union SCIFFTX_REG    SCIFFTX;
   union SCIFFRX_REG    SCIFFRX;
   union SCIFFCT_REG    SCIFFCT;
   Uint16               rsvd2;
   Uint16               rsvd3;
   union SCIPRI_REG     SCIPRI;
};

extern volatile struct SCI_REGS SciaRegs;

#ifdef __cplusplus
}
#endif

#endif
