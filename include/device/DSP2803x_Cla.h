
#ifndef DSP2803X_CLA_H
#define DSP2803X_CLA_H

#ifdef __cplusplus
extern "C" {
#endif

struct MCTL_BITS {
    Uint16   HARDRESET:1;
    Uint16   SOFTRESET:1;
    Uint16   IACKE:1;
    Uint16   rsvd1:13;
};

union MCTL_REG {
   Uint16                   all;
   struct MCTL_BITS         bit;
};

struct MMEMCFG_BITS {
    Uint16   PROGE:1;
    Uint16   rsvd1:3;
    Uint16   RAM0E:1;
    Uint16   RAM1E:1;
    Uint16   rsvd2:10;
};

union MMEMCFG_REG {
   Uint16                   all;
   struct MMEMCFG_BITS      bit;
};

struct MPISRCSEL1_BITS {
    Uint16   PERINT1SEL:4;
    Uint16   PERINT2SEL:4;
    Uint16   PERINT3SEL:4;
    Uint16   PERINT4SEL:4;
    Uint16   PERINT5SEL:4;
    Uint16   PERINT6SEL:4;
    Uint16   PERINT7SEL:4;
    Uint16   PERINT8SEL:4;
};

union MPISRCSEL1_REG {
   Uint32                   all;
   struct MPISRCSEL1_BITS   bit;
};

struct MIFR_BITS {
    Uint16   INT1:1;
    Uint16   INT2:1;
    Uint16   INT3:1;
    Uint16   INT4:1;
    Uint16   INT5:1;
    Uint16   INT6:1;
    Uint16   INT7:1;
    Uint16   INT8:1;
    Uint16   rsvd:8;
};

union MIFR_REG {
   Uint16              all;
   struct MIFR_BITS    bit;
};

union MIOVF_REG {
   Uint16              all;
   struct MIFR_BITS    bit;
};

union MIFRC_REG {
   Uint16              all;
   struct MIFR_BITS    bit;
};

union MICLR_REG {
   Uint16              all;
   struct MIFR_BITS    bit;
};

union MICLROVF_REG {
   Uint16              all;
   struct MIFR_BITS    bit;
};

union MIER_REG {
   Uint16              all;
   struct MIFR_BITS    bit;
};

union MIRUN_REG {
   Uint16              all;
   struct MIFR_BITS    bit;
};

struct MSTF_BITS {
    Uint16   LVF:1;
    Uint16   LUF:1;
    Uint16   NF:1;
    Uint16   ZF:1;
    Uint16   rsvd1:2;
    Uint16   TF:1;
    Uint16   rsvd2:2;
    Uint16   RNDF32:1;
    Uint16   rsvd3:1;
    Uint16   MEALLOW:1;
    Uint16   RPCL:4;
    Uint16   RPCH:8;
    Uint16   rsvd4:8;
};

union MSTF_REG {
   Uint32              all;
   struct MSTF_BITS    bit;
};

union MR_REG {
   Uint32              i32;
   float               f32;
};

struct CLA_REGS {
    Uint16  MVECT1;
    Uint16  MVECT2;
    Uint16  MVECT3;
    Uint16  MVECT4;
    Uint16  MVECT5;
    Uint16  MVECT6;
    Uint16  MVECT7;
    Uint16  MVECT8;
    Uint16  rsvd1[8];
    union   MCTL_REG        MCTL;
    union   MMEMCFG_REG     MMEMCFG;
    Uint16  rsvd2[2];
    union   MPISRCSEL1_REG  MPISRCSEL1;
    Uint16  rsvd3[10];
    union   MIFR_REG        MIFR;
    union   MIOVF_REG       MIOVF;
    union   MIFRC_REG       MIFRC;
    union   MICLR_REG       MICLR;
    union   MICLROVF_REG    MICLROVF;
    union   MIER_REG        MIER;
    union   MIRUN_REG       MIRUN;
    Uint16  rsvd10;

    Uint16  _MPC;
    Uint16  rsvd4;
    Uint16  _MAR0;
    Uint16  _MAR1;
    Uint16  rsvd5[2];
    union   MSTF_REG        _MSTF;
    union   MR_REG          _MR0;
    Uint32  rsvd6;
    union   MR_REG          _MR1;
    Uint32  rsvd7;
    union   MR_REG          _MR2;
    Uint32  rsvd8;
    union   MR_REG          _MR3;
    Uint32  rsvd9;
};

extern volatile struct CLA_REGS Cla1Regs;

#ifdef __cplusplus
}
#endif

#endif
