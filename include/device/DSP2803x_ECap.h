
#ifndef DSP2803x_ECAP_H
#define DSP2803x_ECAP_H

#ifdef __cplusplus
extern "C" {
#endif

struct ECCTL1_BITS {
   Uint16 CAP1POL:1;
   Uint16 CTRRST1:1;
   Uint16 CAP2POL:1;
   Uint16 CTRRST2:1;
   Uint16 CAP3POL:1;
   Uint16 CTRRST3:1;
   Uint16 CAP4POL:1;
   Uint16 CTRRST4:1;
   Uint16 CAPLDEN:1;
   Uint16 PRESCALE:5;
   Uint16 FREE_SOFT:2;
};

union ECCTL1_REG {
   Uint16              all;
   struct ECCTL1_BITS  bit;
};

struct ECCTL2_BITS {
   Uint16 CONT_ONESHT:1;
   Uint16 STOP_WRAP:2;
   Uint16 REARM:1;
   Uint16 TSCTRSTOP:1;
   Uint16 SYNCI_EN:1;
   Uint16 SYNCO_SEL:2;
   Uint16 SWSYNC:1;
   Uint16 CAP_APWM:1;
   Uint16 APWMPOL:1;
   Uint16 rsvd1:5;
};

union ECCTL2_REG {
   Uint16              all;
   struct ECCTL2_BITS  bit;
};

struct ECEINT_BITS {
   Uint16 rsvd1:1;
   Uint16 CEVT1:1;
   Uint16 CEVT2:1;
   Uint16 CEVT3:1;
   Uint16 CEVT4:1;
   Uint16 CTROVF:1;
   Uint16 CTR_EQ_PRD:1;
   Uint16 CTR_EQ_CMP:1;
   Uint16 rsvd2:8;
};

union ECEINT_REG {
   Uint16              all;
   struct ECEINT_BITS  bit;
};

struct ECFLG_BITS {
   Uint16 INT:1;
   Uint16 CEVT1:1;
   Uint16 CEVT2:1;
   Uint16 CEVT3:1;
   Uint16 CEVT4:1;
   Uint16 CTROVF:1;
   Uint16 CTR_EQ_PRD:1;
   Uint16 CTR_EQ_CMP:1;
   Uint16 rsvd2:8;
};

union ECFLG_REG {
   Uint16              all;
   struct ECFLG_BITS   bit;
};

struct ECAP_REGS {
   Uint32              TSCTR;
   Uint32              CTRPHS;
   Uint32              CAP1;
   Uint32              CAP2;
   Uint32              CAP3;
   Uint32              CAP4;
   Uint16              rsvd1[8];
   union   ECCTL1_REG  ECCTL1;
   union   ECCTL2_REG  ECCTL2;
   union   ECEINT_REG  ECEINT;
   union   ECFLG_REG   ECFLG;
   union   ECFLG_REG   ECCLR;
   union   ECEINT_REG  ECFRC;
   Uint16              rsvd2[6];
};

extern volatile struct ECAP_REGS ECap1Regs;

#ifdef __cplusplus
}
#endif

#endif
