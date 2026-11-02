
#ifndef DSP2803x_EQEP_H
#define DSP2803x_EQEP_H

#ifdef __cplusplus
extern "C" {
#endif

struct QDECCTL_BITS {
   Uint16 rsvd1:5;
   Uint16 QSP:1;
   Uint16 QIP:1;
   Uint16 QBP:1;
   Uint16 QAP:1;
   Uint16 IGATE:1;
   Uint16 SWAP:1;
   Uint16 XCR:1;
   Uint16 SPSEL:1;
   Uint16 SOEN:1;
   Uint16 QSRC:2;
};

union QDECCTL_REG {
   Uint16              all;
   struct QDECCTL_BITS   bit;
};

struct QEPCTL_BITS {
   Uint16 WDE:1;
   Uint16 UTE:1;
   Uint16 QCLM:1;
   Uint16 QPEN:1;
   Uint16 IEL:2;
   Uint16 SEL:1;
   Uint16 SWI:1;
   Uint16 IEI:2;
   Uint16 SEI:2;
   Uint16 PCRM:2;
   Uint16 FREE_SOFT:2;
};

union QEPCTL_REG {
   Uint16               all;
   struct QEPCTL_BITS   bit;
};

struct QCAPCTL_BITS {
   Uint16 UPPS:4;
   Uint16 CCPS:3;
   Uint16 rsvd1:8;
   Uint16 CEN:1;
};

union QCAPCTL_REG {
   Uint16               all;
   struct QCAPCTL_BITS  bit;
};

struct QPOSCTL_BITS {
   Uint16 PCSPW:12;
   Uint16 PCE:1;
   Uint16 PCPOL:1;
   Uint16 PCLOAD:1;
   Uint16 PCSHDW:1;
};

union QPOSCTL_REG {
   Uint16               all;
   struct QPOSCTL_BITS  bit;
};

struct QEINT_BITS {
   Uint16 rsvd1:1;
   Uint16 PCE:1;
   Uint16 QPE:1;
   Uint16 QDC:1;
   Uint16 WTO:1;
   Uint16 PCU:1;
   Uint16 PCO:1;
   Uint16 PCR:1;
   Uint16 PCM:1;
   Uint16 SEL:1;
   Uint16 IEL:1;
   Uint16 UTO:1;
   Uint16 rsvd2:4;
};

union QEINT_REG {
   Uint16               all;
   struct QEINT_BITS    bit;
};

struct QFLG_BITS {
   Uint16 INT:1;
   Uint16 PCE:1;
   Uint16 PHE:1;
   Uint16 QDC:1;
   Uint16 WTO:1;
   Uint16 PCU:1;
   Uint16 PCO:1;
   Uint16 PCR:1;
   Uint16 PCM:1;
   Uint16 SEL:1;
   Uint16 IEL:1;
   Uint16 UTO:1;
   Uint16 rsvd2:4;
};

union QFLG_REG {
   Uint16               all;
   struct QFLG_BITS     bit;
};

struct QFRC_BITS {
   Uint16 reserved:1;
   Uint16 PCE:1;
   Uint16 PHE:1;
   Uint16 QDC:1;
   Uint16 WTO:1;
   Uint16 PCU:1;
   Uint16 PCO:1;
   Uint16 PCR:1;
   Uint16 PCM:1;
   Uint16 SEL:1;
   Uint16 IEL:1;
   Uint16 UTO:1;
   Uint16 rsvd2:4;
};

union QFRC_REG {
   Uint16               all;
   struct QFRC_BITS     bit;
};

struct QEPSTS_BITS {
   Uint16 PCEF:1;
   Uint16 FIMF:1;
   Uint16 CDEF:1;
   Uint16 COEF:1;
   Uint16 QDLF:1;
   Uint16 QDF:1;
   Uint16 FIDF:1;
   Uint16 UPEVNT:1;
   Uint16 rsvd1:8;
};

union QEPSTS_REG {
   Uint16               all;
   struct QEPSTS_BITS   bit;
};

struct EQEP_REGS {
   Uint32              QPOSCNT;
   Uint32              QPOSINIT;
   Uint32              QPOSMAX;
   Uint32              QPOSCMP;
   Uint32              QPOSILAT;
   Uint32              QPOSSLAT;
   Uint32              QPOSLAT;
   Uint32              QUTMR;
   Uint32              QUPRD;
   Uint16              QWDTMR;
   Uint16              QWDPRD;
   union  QDECCTL_REG  QDECCTL;
   union  QEPCTL_REG   QEPCTL;
   union  QCAPCTL_REG  QCAPCTL;
   union  QPOSCTL_REG  QPOSCTL;
   union  QEINT_REG    QEINT;
   union  QFLG_REG     QFLG;
   union  QFLG_REG     QCLR;
   union  QFRC_REG     QFRC;
   union  QEPSTS_REG   QEPSTS;
   Uint16              QCTMR;
   Uint16              QCPRD;
   Uint16              QCTMRLAT;
   Uint16              QCPRDLAT;
   Uint16              rsvd1[30];
};

extern volatile struct EQEP_REGS EQep1Regs;

#ifdef __cplusplus
}
#endif

#endif
