
#ifndef DSP2803x_NMIINTRUPT_H
#define DSP2803x_NMIINTRUPT_H

#ifdef __cplusplus
extern "C" {
#endif

struct NMICFG_BITS {
    Uint16   rsvd1:1;
    Uint16   CLOCKFAIL:1;
    Uint16   rsvd2:14;
};

union NMICFG_REG  {
    Uint16                         all;
    struct NMICFG_BITS             bit;
};

struct NMIFLG_BITS {
    Uint16   NMIINT:1;
    Uint16   CLOCKFAIL:1;
    Uint16   rsvd1:14;
};

union NMIFLG_REG  {
    Uint16                         all;
    struct NMIFLG_BITS             bit;
};

struct NMIFLGCLR_BITS {
    Uint16   NMIINT:1;
    Uint16   CLOCKFAIL:1;
    Uint16   rsvd1:14;
};

union NMIFLGCLR_REG  {
    Uint16                         all;
    struct NMIFLGCLR_BITS          bit;
};

struct NMIFLGFRC_BITS {
    Uint16   rsvd1:1;
    Uint16   CLOCKFAIL:1;
    Uint16   rsvd2:14;
};

union NMIFLGFRC_REG  {
    Uint16                         all;
    struct NMIFLGFRC_BITS          bit;
};

struct NMIINTRUPT_REGS {
    union  NMICFG_REG          NMICFG;
    union  NMIFLG_REG          NMIFLG;
    union  NMIFLGCLR_REG       NMIFLGCLR;
    union  NMIFLGFRC_REG       NMIFLGFRC;
    Uint16                     NMIWDCNT;
    Uint16                     NMIWDPRD;
    Uint16                     rsvd1[10];
};

extern volatile struct NMIINTRUPT_REGS NmiIntruptRegs;

#ifdef __cplusplus
}
#endif

#endif
