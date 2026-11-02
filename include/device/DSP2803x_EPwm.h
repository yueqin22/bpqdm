
#ifndef DSP2803x_EPWM_H
#define DSP2803x_EPWM_H

#ifdef __cplusplus
extern "C" {
#endif

struct TBCTL_BITS {
   Uint16 CTRMODE:2;
   Uint16 PHSEN:1;
   Uint16 PRDLD:1;
   Uint16 SYNCOSEL:2;
   Uint16 SWFSYNC:1;
   Uint16 HSPCLKDIV:3;
   Uint16 CLKDIV:3;
   Uint16 PHSDIR:1;
   Uint16 FREE_SOFT:2;
};

union TBCTL_REG {
   Uint16              all;
   struct TBCTL_BITS   bit;
};

struct TBSTS_BITS {
   Uint16 CTRDIR:1;
   Uint16 SYNCI:1;
   Uint16 CTRMAX:1;
   Uint16 rsvd1:13;
};

union TBSTS_REG {
   Uint16              all;
   struct TBSTS_BITS   bit;
};

struct CMPCTL_BITS {
   Uint16 LOADAMODE:2;
   Uint16 LOADBMODE:2;
   Uint16 SHDWAMODE:1;
   Uint16 rsvd1:1;
   Uint16 SHDWBMODE:1;
   Uint16 rsvd2:1;
   Uint16 SHDWAFULL:1;
   Uint16 SHDWBFULL:1;
   Uint16 rsvd3:6;
};

union CMPCTL_REG {
   Uint16                all;
   struct CMPCTL_BITS    bit;
};

struct AQCTL_BITS {
   Uint16 ZRO:2;
   Uint16 PRD:2;
   Uint16 CAU:2;
   Uint16 CAD:2;
   Uint16 CBU:2;
   Uint16 CBD:2;
   Uint16 rsvd:4;
};

union AQCTL_REG {
   Uint16                all;
   struct AQCTL_BITS     bit;
};

struct AQSFRC_BITS {
   Uint16 ACTSFA:2;
   Uint16 OTSFA:1;
   Uint16 ACTSFB:2;
   Uint16 OTSFB:1;
   Uint16 RLDCSF:2;
   Uint16 rsvd1:8;
};

union AQSFRC_REG {
   Uint16                 all;
   struct AQSFRC_BITS     bit;
};

struct AQCSFRC_BITS {
   Uint16 CSFA:2;
   Uint16 CSFB:2;
   Uint16 rsvd1:12;
};

union AQCSFRC_REG {
   Uint16                  all;
   struct AQCSFRC_BITS     bit;
};

struct DBCTL_BITS {
   Uint16 OUT_MODE:2;
   Uint16 POLSEL:2;
   Uint16 IN_MODE:2;
   Uint16 rsvd1:9;
   Uint16 HALFCYCLE:1;
};

union DBCTL_REG {
   Uint16                  all;
   struct DBCTL_BITS       bit;
};

struct TZSEL_BITS {
   Uint16  CBC1:1;
   Uint16  CBC2:1;
   Uint16  CBC3:1;
   Uint16  CBC4:1;
   Uint16  CBC5:1;
   Uint16  CBC6:1;
   Uint16  DCAEVT2:1;
   Uint16  DCBEVT2:1;
   Uint16  OSHT1:1;
   Uint16  OSHT2:1;
   Uint16  OSHT3:1;
   Uint16  OSHT4:1;
   Uint16  OSHT5:1;
   Uint16  OSHT6:1;
   Uint16  DCAEVT1:1;
   Uint16  DCBEVT1:1;
};

union TZSEL_REG {
   Uint16                  all;
   struct TZSEL_BITS       bit;
};

struct TZDCSEL_BITS {
   Uint16  DCAEVT1:3;
   Uint16  DCAEVT2:3;
   Uint16  DCBEVT1:3;
   Uint16  DCBEVT2:3;
};

union TZDCSEL_REG {
   Uint16                  all;
   struct TZDCSEL_BITS     bit;
};

struct TZCTL_BITS {
   Uint16 TZA:2;
   Uint16 TZB:2;
   Uint16 DCAEVT1:2;
   Uint16 DCAEVT2:2;
   Uint16 DCBEVT1:2;
   Uint16 DCBEVT2:2;
   Uint16 rsvd:4;
};

union TZCTL_REG {
   Uint16                  all;
   struct TZCTL_BITS       bit;
};

struct TZEINT_BITS {
   Uint16  rsvd1:1;
   Uint16  CBC:1;
   Uint16  OST:1;
   Uint16  DCAEVT1:1;
   Uint16  DCAEVT2:1;
   Uint16  DCBEVT1:1;
   Uint16  DCBEVT2:1;
   Uint16  rsvd2:9;
};

union TZEINT_REG {
   Uint16                  all;
   struct TZEINT_BITS      bit;
};

struct TZFLG_BITS {
   Uint16  INT:1;
   Uint16  CBC:1;
   Uint16  OST:1;
   Uint16  DCAEVT1:1;
   Uint16  DCAEVT2:1;
   Uint16  DCBEVT1:1;
   Uint16  DCBEVT2:1;
   Uint16  rsvd2:9;
};

union TZFLG_REG {
   Uint16                  all;
   struct TZFLG_BITS       bit;
};

struct TZCLR_BITS {
   Uint16  INT:1;
   Uint16  CBC:1;
   Uint16  OST:1;
   Uint16  DCAEVT1:1;
   Uint16  DCAEVT2:1;
   Uint16  DCBEVT1:1;
   Uint16  DCBEVT2:1;
   Uint16  rsvd2:9;
};

union TZCLR_REG {
   Uint16                  all;
   struct TZCLR_BITS       bit;
};

struct TZFRC_BITS {
   Uint16  rsvd1:1;
   Uint16  CBC:1;
   Uint16  OST:1;
   Uint16  DCAEVT1:1;
   Uint16  DCAEVT2:1;
   Uint16  DCBEVT1:1;
   Uint16  DCBEVT2:1;
   Uint16  rsvd2:9;
};

union TZFRC_REG {
   Uint16                  all;
   struct TZFRC_BITS       bit;
};

struct ETSEL_BITS {
   Uint16  INTSEL:3;
   Uint16  INTEN:1;
   Uint16  rsvd1:4;
   Uint16  SOCASEL:3;
   Uint16  SOCAEN:1;
   Uint16  SOCBSEL:3;
   Uint16  SOCBEN:1;
};

union ETSEL_REG {
   Uint16                  all;
   struct ETSEL_BITS       bit;
};

struct ETPS_BITS {
   Uint16  INTPRD:2;
   Uint16  INTCNT:2;
   Uint16  rsvd1:4;
   Uint16  SOCAPRD:2;
   Uint16  SOCACNT:2;
   Uint16  SOCBPRD:2;
   Uint16  SOCBCNT:2;
};

union ETPS_REG {
   Uint16                  all;
   struct ETPS_BITS        bit;
};

struct ETFLG_BITS {
   Uint16  INT:1;
   Uint16  rsvd1:1;
   Uint16  SOCA:1;
   Uint16  SOCB:1;
   Uint16  rsvd2:12;
};

union ETFLG_REG {
   Uint16                   all;
   struct ETFLG_BITS        bit;
};

struct ETCLR_BITS {
   Uint16  INT:1;
   Uint16  rsvd1:1;
   Uint16  SOCA:1;
   Uint16  SOCB:1;
   Uint16  rsvd2:12;
};

union ETCLR_REG {
   Uint16                   all;
   struct ETCLR_BITS        bit;
};

struct ETFRC_BITS {
   Uint16  INT:1;
   Uint16  rsvd1:1;
   Uint16  SOCA:1;
   Uint16  SOCB:1;
   Uint16  rsvd2:12;
};

union ETFRC_REG {
   Uint16                  all;
   struct ETFRC_BITS        bit;
};

struct PCCTL_BITS {
   Uint16  CHPEN:1;
   Uint16  OSHTWTH:4;
   Uint16  CHPFREQ:3;
   Uint16  CHPDUTY:3;
   Uint16  rsvd1:5;
};

union PCCTL_REG {
   Uint16                  all;
   struct PCCTL_BITS       bit;
};

struct DCTRIPSEL_BITS {
   Uint16 DCAHCOMPSEL:4;
   Uint16 DCALCOMPSEL:4;
   Uint16 DCBHCOMPSEL:4;
   Uint16 DCBLCOMPSEL:4;
};

union DCTRIPSEL_REG {
   Uint16                  all;
   struct DCTRIPSEL_BITS   bit;
};

struct DCCTL_BITS {
   Uint16 EVT1SRCSEL:1;
   Uint16 EVT1FRCSYNCSEL:1;
   Uint16 EVT1SOCE:1;
   Uint16 EVT1SYNCE:1;
   Uint16 rsvd1:4;
   Uint16 EVT2SRCSEL:1;
   Uint16 EVT2FRCSYNCSEL:1;
   Uint16 rsvd2:6;
};

union DCCTL_REG {
   Uint16                  all;
   struct DCCTL_BITS       bit;
};

struct DCCAPCTL_BITS {
   Uint16 CAPE:1;
   Uint16 SHDWMODE:1;
   Uint16 rsvd:14;
};

union DCCAPCTL_REG {
   Uint16                  all;
   struct DCCAPCTL_BITS    bit;
};

struct DCFCTL_BITS {
   Uint16 SRCSEL:2;
   Uint16 BLANKE:1;
   Uint16 BLANKINV:1;
   Uint16 PULSESEL:2;
   Uint16 rsvd:10;
};

union DCFCTL_REG {
   Uint16                  all;
   struct DCFCTL_BITS      bit;
};

struct HRPCTL_BITS {
   Uint16  HRPE:1;
   Uint16  rsvd1:1;
   Uint16  TBPHSHRLOADE:1;
   Uint16  rsvd2:13;
};

union HRPCTL_REG {
   Uint16                  	all;
   struct HRPCTL_BITS       bit;
};

struct HRCNFG_BITS {
   Uint16  EDGMODE:2;
   Uint16  CTLMODE:1;
   Uint16  HRLOAD:2;
   Uint16  SELOUTB:1;
   Uint16  AUTOCONV:1;
   Uint16  SWAPAB:1;
   Uint16  rsvd1:8;
};

union HRCNFG_REG {
   Uint16                  	all;
   struct HRCNFG_BITS       bit;
};

struct HRPWR_BITS {
   Uint16  rsvd1:6;
   Uint16  MEPOFF:4;
   Uint16  rsvd2:6;
};

union HRPWR_REG {
   Uint16                  	all;
   struct HRPWR_BITS        bit;
};

struct TBPHS_HRPWM_REG {
   Uint16  TBPHSHR;
   Uint16  TBPHS;
};

union TBPHS_HRPWM_GROUP {
   Uint32                  all;
   struct TBPHS_HRPWM_REG  half;
};

struct CMPA_HRPWM_REG {
   Uint16  CMPAHR;
   Uint16  CMPA;
};

union CMPA_HRPWM_GROUP {
   Uint32                 all;
   struct CMPA_HRPWM_REG  half;
};

struct TBPRD_HRPWM_REG {
   Uint16  TBPRDHR;
   Uint16  TBPRD;
};

union TBPRD_HRPWM_GROUP {
   Uint32                  all;
   struct TBPRD_HRPWM_REG  half;
};

struct EPWM_REGS {
   union  TBCTL_REG           TBCTL;
   union  TBSTS_REG           TBSTS;
   union  TBPHS_HRPWM_GROUP   TBPHS;
   Uint16                     TBCTR;
   Uint16                     TBPRD;
   Uint16                     TBPRDHR;
   union  CMPCTL_REG          CMPCTL;
   union  CMPA_HRPWM_GROUP    CMPA;
   Uint16                     CMPB;
   union  AQCTL_REG           AQCTLA;
   union  AQCTL_REG           AQCTLB;
   union  AQSFRC_REG          AQSFRC;
   union  AQCSFRC_REG         AQCSFRC;
   union  DBCTL_REG           DBCTL;
   Uint16                     DBRED;
   Uint16                     DBFED;
   union  TZSEL_REG           TZSEL;
   union  TZDCSEL_REG         TZDCSEL;
   union  TZCTL_REG           TZCTL;
   union  TZEINT_REG          TZEINT;
   union  TZFLG_REG           TZFLG;
   union  TZCLR_REG           TZCLR;
   union  TZFRC_REG    	      TZFRC;
   union  ETSEL_REG           ETSEL;
   union  ETPS_REG            ETPS;
   union  ETFLG_REG           ETFLG;
   union  ETCLR_REG           ETCLR;
   union  ETFRC_REG           ETFRC;
   union  PCCTL_REG           PCCTL;
   Uint16                     rsvd3;
   union  HRCNFG_REG          HRCNFG;
   union  HRPWR_REG			  HRPWR;
   Uint16                     rsvd4[4];
   Uint16                     HRMSTEP;
   Uint16                     rsvd5;
   union  HRPCTL_REG          HRPCTL;
   Uint16                     rsvd6;
   union  TBPRD_HRPWM_GROUP   TBPRDM;
   union  CMPA_HRPWM_GROUP    CMPAM;
   Uint16                     rsvd7[2];
   union  DCTRIPSEL_REG       DCTRIPSEL;
   union  DCCTL_REG           DCACTL;
   union  DCCTL_REG           DCBCTL;
   union  DCFCTL_REG          DCFCTL;
   union  DCCAPCTL_REG        DCCAPCTL;
   Uint16                     DCFOFFSET;
   Uint16                     DCFOFFSETCNT;
   Uint16                     DCFWINDOW;
   Uint16                     DCFWINDOWCNT;
   Uint16                     DCCAP;
};

extern volatile struct EPWM_REGS EPwm1Regs;
extern volatile struct EPWM_REGS EPwm2Regs;
extern volatile struct EPWM_REGS EPwm3Regs;
extern volatile struct EPWM_REGS EPwm4Regs;
extern volatile struct EPWM_REGS EPwm5Regs;
extern volatile struct EPWM_REGS EPwm6Regs;
extern volatile struct EPWM_REGS EPwm7Regs;

#ifdef __cplusplus
}
#endif

#endif
