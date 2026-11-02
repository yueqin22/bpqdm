
#ifndef DSP2803x_COMP_H
#define DSP2803x_COMP_H

#ifdef __cplusplus
extern "C" {
#endif

struct COMPCTL_BITS {
    Uint16   COMPDACEN:1;
    Uint16   COMPSOURCE:1;
    Uint16   CMPINV:1;
    Uint16   QUALSEL:5;
    Uint16   SYNCSEL:1;
    Uint16   rsvd1:7;
};

union COMPCTL_REG  {
    Uint16                         all;
    struct COMPCTL_BITS            bit;
};

struct COMPSTS_BITS {
    Uint16   COMPSTS:1;
    Uint16   rsvd1:15;
};

union COMPSTS_REG  {
    Uint16                         all;
    struct COMPSTS_BITS            bit;
};

struct DACVAL_BITS {
    Uint16   DACVAL:10;
    Uint16   rsvd1:6;
};

union DACVAL_REG  {
    Uint16                         all;
    struct DACVAL_BITS             bit;
};

struct COMP_REGS {
    union  COMPCTL_REG         COMPCTL;
    Uint16                     rsvd1;
    union  COMPSTS_REG         COMPSTS;
    Uint16                     rsvd2[3];
    union  DACVAL_REG          DACVAL;
    Uint16                     rsvd4[10];
};

extern volatile struct COMP_REGS Comp1Regs;
extern volatile struct COMP_REGS Comp2Regs;
extern volatile struct COMP_REGS Comp3Regs;

#ifdef __cplusplus
}
#endif

#endif
