
#ifndef DSP2803x_XINTRUPT_H
#define DSP2803x_XINTRUPT_H

#ifdef __cplusplus
extern "C" {
#endif

struct XINTCR_BITS {
    Uint16   ENABLE:1;
    Uint16   rsvd1:1;
    Uint16   POLARITY:2;
    Uint16   rsvd2:12;
};

union XINTCR_REG {
   Uint16               all;
   struct XINTCR_BITS   bit;
};

struct XINTRUPT_REGS {
   union XINTCR_REG XINT1CR;
   union XINTCR_REG XINT2CR;
   union XINTCR_REG XINT3CR;
   Uint16           rsvd1[5];
   Uint16           XINT1CTR;
   Uint16           XINT2CTR;
   Uint16           XINT3CTR;
   Uint16           rsvd[5];
};

extern volatile struct XINTRUPT_REGS XIntruptRegs;

#ifdef __cplusplus
}
#endif

#endif
