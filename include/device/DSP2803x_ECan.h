
#ifndef DSP2803x_ECAN_H
#define DSP2803x_ECAN_H

#ifdef __cplusplus
extern "C" {
#endif

struct  CANME_BITS {
   Uint16      ME0:1;
   Uint16      ME1:1;
   Uint16      ME2:1;
   Uint16      ME3:1;
   Uint16      ME4:1;
   Uint16      ME5:1;
   Uint16      ME6:1;
   Uint16      ME7:1;
   Uint16      ME8:1;
   Uint16      ME9:1;
   Uint16      ME10:1;
   Uint16      ME11:1;
   Uint16      ME12:1;
   Uint16      ME13:1;
   Uint16      ME14:1;
   Uint16      ME15:1;
   Uint16      ME16:1;
   Uint16      ME17:1;
   Uint16      ME18:1;
   Uint16      ME19:1;
   Uint16      ME20:1;
   Uint16      ME21:1;
   Uint16      ME22:1;
   Uint16      ME23:1;
   Uint16      ME24:1;
   Uint16      ME25:1;
   Uint16      ME26:1;
   Uint16      ME27:1;
   Uint16      ME28:1;
   Uint16      ME29:1;
   Uint16      ME30:1;
   Uint16      ME31:1;

};

union CANME_REG {
   Uint32             all;
   struct CANME_BITS  bit;
};

struct  CANMD_BITS {
   Uint16      MD0:1;
   Uint16      MD1:1;
   Uint16      MD2:1;
   Uint16      MD3:1;
   Uint16      MD4:1;
   Uint16      MD5:1;
   Uint16      MD6:1;
   Uint16      MD7:1;
   Uint16      MD8:1;
   Uint16      MD9:1;
   Uint16      MD10:1;
   Uint16      MD11:1;
   Uint16      MD12:1;
   Uint16      MD13:1;
   Uint16      MD14:1;
   Uint16      MD15:1;
   Uint16      MD16:1;
   Uint16      MD17:1;
   Uint16      MD18:1;
   Uint16      MD19:1;
   Uint16      MD20:1;
   Uint16      MD21:1;
   Uint16      MD22:1;
   Uint16      MD23:1;
   Uint16      MD24:1;
   Uint16      MD25:1;
   Uint16      MD26:1;
   Uint16      MD27:1;
   Uint16      MD28:1;
   Uint16      MD29:1;
   Uint16      MD30:1;
   Uint16      MD31:1;

};

union CANMD_REG {
   Uint32             all;
   struct CANMD_BITS  bit;
};

struct  CANTRS_BITS {
   Uint16      TRS0:1;
   Uint16      TRS1:1;
   Uint16      TRS2:1;
   Uint16      TRS3:1;
   Uint16      TRS4:1;
   Uint16      TRS5:1;
   Uint16      TRS6:1;
   Uint16      TRS7:1;
   Uint16      TRS8:1;
   Uint16      TRS9:1;
   Uint16      TRS10:1;
   Uint16      TRS11:1;
   Uint16      TRS12:1;
   Uint16      TRS13:1;
   Uint16      TRS14:1;
   Uint16      TRS15:1;
   Uint16      TRS16:1;
   Uint16      TRS17:1;
   Uint16      TRS18:1;
   Uint16      TRS19:1;
   Uint16      TRS20:1;
   Uint16      TRS21:1;
   Uint16      TRS22:1;
   Uint16      TRS23:1;
   Uint16      TRS24:1;
   Uint16      TRS25:1;
   Uint16      TRS26:1;
   Uint16      TRS27:1;
   Uint16      TRS28:1;
   Uint16      TRS29:1;
   Uint16      TRS30:1;
   Uint16      TRS31:1;

};

union CANTRS_REG {
   Uint32              all;
   struct CANTRS_BITS  bit;
};

struct  CANTRR_BITS {
   Uint16      TRR0:1;
   Uint16      TRR1:1;
   Uint16      TRR2:1;
   Uint16      TRR3:1;
   Uint16      TRR4:1;
   Uint16      TRR5:1;
   Uint16      TRR6:1;
   Uint16      TRR7:1;
   Uint16      TRR8:1;
   Uint16      TRR9:1;
   Uint16      TRR10:1;
   Uint16      TRR11:1;
   Uint16      TRR12:1;
   Uint16      TRR13:1;
   Uint16      TRR14:1;
   Uint16      TRR15:1;
   Uint16      TRR16:1;
   Uint16      TRR17:1;
   Uint16      TRR18:1;
   Uint16      TRR19:1;
   Uint16      TRR20:1;
   Uint16      TRR21:1;
   Uint16      TRR22:1;
   Uint16      TRR23:1;
   Uint16      TRR24:1;
   Uint16      TRR25:1;
   Uint16      TRR26:1;
   Uint16      TRR27:1;
   Uint16      TRR28:1;
   Uint16      TRR29:1;
   Uint16      TRR30:1;
   Uint16      TRR31:1;

};

union CANTRR_REG {
   Uint32              all;
   struct CANTRR_BITS  bit;
};

struct  CANTA_BITS {
   Uint16      TA0:1;
   Uint16      TA1:1;
   Uint16      TA2:1;
   Uint16      TA3:1;
   Uint16      TA4:1;
   Uint16      TA5:1;
   Uint16      TA6:1;
   Uint16      TA7:1;
   Uint16      TA8:1;
   Uint16      TA9:1;
   Uint16      TA10:1;
   Uint16      TA11:1;
   Uint16      TA12:1;
   Uint16      TA13:1;
   Uint16      TA14:1;
   Uint16      TA15:1;
   Uint16      TA16:1;
   Uint16      TA17:1;
   Uint16      TA18:1;
   Uint16      TA19:1;
   Uint16      TA20:1;
   Uint16      TA21:1;
   Uint16      TA22:1;
   Uint16      TA23:1;
   Uint16      TA24:1;
   Uint16      TA25:1;
   Uint16      TA26:1;
   Uint16      TA27:1;
   Uint16      TA28:1;
   Uint16      TA29:1;
   Uint16      TA30:1;
   Uint16      TA31:1;

};

union CANTA_REG {
   Uint32             all;
   struct CANTA_BITS  bit;
};

struct  CANAA_BITS {
   Uint16      AA0:1;
   Uint16      AA1:1;
   Uint16      AA2:1;
   Uint16      AA3:1;
   Uint16      AA4:1;
   Uint16      AA5:1;
   Uint16      AA6:1;
   Uint16      AA7:1;
   Uint16      AA8:1;
   Uint16      AA9:1;
   Uint16      AA10:1;
   Uint16      AA11:1;
   Uint16      AA12:1;
   Uint16      AA13:1;
   Uint16      AA14:1;
   Uint16      AA15:1;
   Uint16      AA16:1;
   Uint16      AA17:1;
   Uint16      AA18:1;
   Uint16      AA19:1;
   Uint16      AA20:1;
   Uint16      AA21:1;
   Uint16      AA22:1;
   Uint16      AA23:1;
   Uint16      AA24:1;
   Uint16      AA25:1;
   Uint16      AA26:1;
   Uint16      AA27:1;
   Uint16      AA28:1;
   Uint16      AA29:1;
   Uint16      AA30:1;
   Uint16      AA31:1;

};

union CANAA_REG {
   Uint32             all;
   struct CANAA_BITS  bit;
};

struct  CANRMP_BITS {
   Uint16      RMP0:1;
   Uint16      RMP1:1;
   Uint16      RMP2:1;
   Uint16      RMP3:1;
   Uint16      RMP4:1;
   Uint16      RMP5:1;
   Uint16      RMP6:1;
   Uint16      RMP7:1;
   Uint16      RMP8:1;
   Uint16      RMP9:1;
   Uint16      RMP10:1;
   Uint16      RMP11:1;
   Uint16      RMP12:1;
   Uint16      RMP13:1;
   Uint16      RMP14:1;
   Uint16      RMP15:1;
   Uint16      RMP16:1;
   Uint16      RMP17:1;
   Uint16      RMP18:1;
   Uint16      RMP19:1;
   Uint16      RMP20:1;
   Uint16      RMP21:1;
   Uint16      RMP22:1;
   Uint16      RMP23:1;
   Uint16      RMP24:1;
   Uint16      RMP25:1;
   Uint16      RMP26:1;
   Uint16      RMP27:1;
   Uint16      RMP28:1;
   Uint16      RMP29:1;
   Uint16      RMP30:1;
   Uint16      RMP31:1;

};

union CANRMP_REG {
   Uint32              all;
   struct CANRMP_BITS  bit;
};

struct  CANRML_BITS {
   Uint16      RML0:1;
   Uint16      RML1:1;
   Uint16      RML2:1;
   Uint16      RML3:1;
   Uint16      RML4:1;
   Uint16      RML5:1;
   Uint16      RML6:1;
   Uint16      RML7:1;
   Uint16      RML8:1;
   Uint16      RML9:1;
   Uint16      RML10:1;
   Uint16      RML11:1;
   Uint16      RML12:1;
   Uint16      RML13:1;
   Uint16      RML14:1;
   Uint16      RML15:1;
   Uint16      RML16:1;
   Uint16      RML17:1;
   Uint16      RML18:1;
   Uint16      RML19:1;
   Uint16      RML20:1;
   Uint16      RML21:1;
   Uint16      RML22:1;
   Uint16      RML23:1;
   Uint16      RML24:1;
   Uint16      RML25:1;
   Uint16      RML26:1;
   Uint16      RML27:1;
   Uint16      RML28:1;
   Uint16      RML29:1;
   Uint16      RML30:1;
   Uint16      RML31:1;

};

union CANRML_REG {
   Uint32              all;
   struct CANRML_BITS  bit;
};

struct  CANRFP_BITS {
   Uint16      RFP0:1;
   Uint16      RFP1:1;
   Uint16      RFP2:1;
   Uint16      RFP3:1;
   Uint16      RFP4:1;
   Uint16      RFP5:1;
   Uint16      RFP6:1;
   Uint16      RFP7:1;
   Uint16      RFP8:1;
   Uint16      RFP9:1;
   Uint16      RFP10:1;
   Uint16      RFP11:1;
   Uint16      RFP12:1;
   Uint16      RFP13:1;
   Uint16      RFP14:1;
   Uint16      RFP15:1;
   Uint16      RFP16:1;
   Uint16      RFP17:1;
   Uint16      RFP18:1;
   Uint16      RFP19:1;
   Uint16      RFP20:1;
   Uint16      RFP21:1;
   Uint16      RFP22:1;
   Uint16      RFP23:1;
   Uint16      RFP24:1;
   Uint16      RFP25:1;
   Uint16      RFP26:1;
   Uint16      RFP27:1;
   Uint16      RFP28:1;
   Uint16      RFP29:1;
   Uint16      RFP30:1;
   Uint16      RFP31:1;

};

union CANRFP_REG {
   Uint32              all;
   struct CANRFP_BITS  bit;
};

struct  CANGAM_BITS {
   Uint16 GAM150:16;
   Uint16 GAM2816:13;
   Uint16 rsvd:2;
   Uint16 AMI:1;
};

union CANGAM_REG {
   Uint32              all;
   struct CANGAM_BITS  bit;
};

struct  CANMC_BITS {
   Uint16      MBNR:5;
   Uint16      SRES:1;
   Uint16      STM:1;
   Uint16      ABO:1;
   Uint16      CDR:1;
   Uint16      WUBA:1;
   Uint16      DBO:1;
   Uint16      PDR:1;
   Uint16      CCR:1;
   Uint16      SCB:1;
   Uint16      TCC:1;
   Uint16      MBCC:1;
   Uint16      SUSP:1;
   Uint16      rsvd:15;
};

union CANMC_REG {
   Uint32             all;
   struct CANMC_BITS  bit;
};

struct  CANBTC_BITS {
   Uint16  TSEG2REG:3;
   Uint16  TSEG1REG:4;
   Uint16  SAM:1;
   Uint16  SJWREG:2;
   Uint16  rsvd1:6;
   Uint16  BRPREG:8;
   Uint16  rsvd2:8;
};

union CANBTC_REG {
   Uint32              all;
   struct CANBTC_BITS  bit;
};

struct  CANES_BITS {
   Uint16   TM:1;
   Uint16   RM:1;
   Uint16   rsvd1:1;
   Uint16   PDA:1;
   Uint16   CCE:1;
   Uint16   SMA:1;
   Uint16   rsvd2:10;
   Uint16   EW:1;
   Uint16   EP:1;
   Uint16   BO:1;
   Uint16   ACKE:1;
   Uint16   SE:1;
   Uint16   CRCE:1;
   Uint16   SA1:1;
   Uint16   BE:1;
   Uint16   FE:1;
   Uint16   rsvd3:7;
};

union CANES_REG {
   Uint32             all;
   struct CANES_BITS  bit;
};

struct  CANTEC_BITS {
   Uint16 TEC:8;
   Uint16 rsvd1:8;
   Uint16 rsvd2:16;
};

union CANTEC_REG {
   Uint32              all;
   struct CANTEC_BITS  bit;
};

struct  CANREC_BITS {
   Uint16 REC:8;
   Uint16 rsvd1:8;
   Uint16 rsvd2:16;
};

union CANREC_REG {
   Uint32              all;
   struct CANREC_BITS  bit;
};

struct  CANGIF0_BITS {
   Uint16   MIV0:5;
   Uint16   rsvd1:3;
   Uint16   WLIF0:1;
   Uint16   EPIF0:1;
   Uint16   BOIF0:1;
   Uint16   RMLIF0:1;
   Uint16   WUIF0:1;
   Uint16   WDIF0:1;
   Uint16   AAIF0:1;
   Uint16   GMIF0:1;
   Uint16   TCOF0:1;
   Uint16   MTOF0:1;
   Uint16   rsvd2:14;
};

union CANGIF0_REG {
   Uint32               all;
   struct CANGIF0_BITS  bit;
};

struct  CANGIM_BITS {
   Uint16  I0EN:1;
   Uint16  I1EN:1;
   Uint16  GIL:1;
   Uint16  rsvd1:5;
   Uint16  WLIM:1;
   Uint16  EPIM:1;
   Uint16  BOIM:1;
   Uint16  RMLIM:1;
   Uint16  WUIM:1;
   Uint16  WDIM:1;
   Uint16  AAIM:1;
   Uint16  rsvd2:1;
   Uint16  TCOM:1;
   Uint16  MTOM:1;
   Uint16  rsvd3:14;
};

union CANGIM_REG {
   Uint32              all;
   struct CANGIM_BITS  bit;
};

struct  CANGIF1_BITS {
   Uint16      MIV1:5;
   Uint16      rsvd1:3;
   Uint16      WLIF1:1;
   Uint16      EPIF1:1;
   Uint16      BOIF1:1;
   Uint16      RMLIF1:1;
   Uint16      WUIF1:1;
   Uint16      WDIF1:1;
   Uint16      AAIF1:1;
   Uint16      GMIF1:1;
   Uint16      TCOF1:1;
   Uint16      MTOF1:1;
   Uint16      rsvd2:14;
};

union CANGIF1_REG {
   Uint32               all;
   struct CANGIF1_BITS  bit;
};

struct  CANMIM_BITS {
   Uint16      MIM0:1;
   Uint16      MIM1:1;
   Uint16      MIM2:1;
   Uint16      MIM3:1;
   Uint16      MIM4:1;
   Uint16      MIM5:1;
   Uint16      MIM6:1;
   Uint16      MIM7:1;
   Uint16      MIM8:1;
   Uint16      MIM9:1;
   Uint16      MIM10:1;
   Uint16      MIM11:1;
   Uint16      MIM12:1;
   Uint16      MIM13:1;
   Uint16      MIM14:1;
   Uint16      MIM15:1;
   Uint16      MIM16:1;
   Uint16      MIM17:1;
   Uint16      MIM18:1;
   Uint16      MIM19:1;
   Uint16      MIM20:1;
   Uint16      MIM21:1;
   Uint16      MIM22:1;
   Uint16      MIM23:1;
   Uint16      MIM24:1;
   Uint16      MIM25:1;
   Uint16      MIM26:1;
   Uint16      MIM27:1;
   Uint16      MIM28:1;
   Uint16      MIM29:1;
   Uint16      MIM30:1;
   Uint16      MIM31:1;

};

union CANMIM_REG {
   Uint32              all;
   struct CANMIM_BITS  bit;
};

struct  CANMIL_BITS {
   Uint16      MIL0:1;
   Uint16      MIL1:1;
   Uint16      MIL2:1;
   Uint16      MIL3:1;
   Uint16      MIL4:1;
   Uint16      MIL5:1;
   Uint16      MIL6:1;
   Uint16      MIL7:1;
   Uint16      MIL8:1;
   Uint16      MIL9:1;
   Uint16      MIL10:1;
   Uint16      MIL11:1;
   Uint16      MIL12:1;
   Uint16      MIL13:1;
   Uint16      MIL14:1;
   Uint16      MIL15:1;
   Uint16      MIL16:1;
   Uint16      MIL17:1;
   Uint16      MIL18:1;
   Uint16      MIL19:1;
   Uint16      MIL20:1;
   Uint16      MIL21:1;
   Uint16      MIL22:1;
   Uint16      MIL23:1;
   Uint16      MIL24:1;
   Uint16      MIL25:1;
   Uint16      MIL26:1;
   Uint16      MIL27:1;
   Uint16      MIL28:1;
   Uint16      MIL29:1;
   Uint16      MIL30:1;
   Uint16      MIL31:1;

};

union CANMIL_REG {
   Uint32              all;
   struct CANMIL_BITS  bit;
};

struct  CANOPC_BITS {
   Uint16      OPC0:1;
   Uint16      OPC1:1;
   Uint16      OPC2:1;
   Uint16      OPC3:1;
   Uint16      OPC4:1;
   Uint16      OPC5:1;
   Uint16      OPC6:1;
   Uint16      OPC7:1;
   Uint16      OPC8:1;
   Uint16      OPC9:1;
   Uint16      OPC10:1;
   Uint16      OPC11:1;
   Uint16      OPC12:1;
   Uint16      OPC13:1;
   Uint16      OPC14:1;
   Uint16      OPC15:1;
   Uint16      OPC16:1;
   Uint16      OPC17:1;
   Uint16      OPC18:1;
   Uint16      OPC19:1;
   Uint16      OPC20:1;
   Uint16      OPC21:1;
   Uint16      OPC22:1;
   Uint16      OPC23:1;
   Uint16      OPC24:1;
   Uint16      OPC25:1;
   Uint16      OPC26:1;
   Uint16      OPC27:1;
   Uint16      OPC28:1;
   Uint16      OPC29:1;
   Uint16      OPC30:1;
   Uint16      OPC31:1;

};

union CANOPC_REG {
   Uint32              all;
   struct CANOPC_BITS  bit;
};

struct  CANTIOC_BITS {
   Uint16  rsvd1:3;
   Uint16  TXFUNC:1;
   Uint16  rsvd2:12;
   Uint16  rsvd3:16;
};

union CANTIOC_REG {
   Uint32               all;
   struct CANTIOC_BITS  bit;
};

struct  CANRIOC_BITS {
   Uint16  rsvd1:3;
   Uint16  RXFUNC:1;
   Uint16  rsvd2:12;
   Uint16  rsvd3:16;
};

union CANRIOC_REG {
   Uint32               all;
   struct CANRIOC_BITS  bit;
};

struct  CANTOC_BITS {
   Uint16      TOC0:1;
   Uint16      TOC1:1;
   Uint16      TOC2:1;
   Uint16      TOC3:1;
   Uint16      TOC4:1;
   Uint16      TOC5:1;
   Uint16      TOC6:1;
   Uint16      TOC7:1;
   Uint16      TOC8:1;
   Uint16      TOC9:1;
   Uint16      TOC10:1;
   Uint16      TOC11:1;
   Uint16      TOC12:1;
   Uint16      TOC13:1;
   Uint16      TOC14:1;
   Uint16      TOC15:1;
   Uint16      TOC16:1;
   Uint16      TOC17:1;
   Uint16      TOC18:1;
   Uint16      TOC19:1;
   Uint16      TOC20:1;
   Uint16      TOC21:1;
   Uint16      TOC22:1;
   Uint16      TOC23:1;
   Uint16      TOC24:1;
   Uint16      TOC25:1;
   Uint16      TOC26:1;
   Uint16      TOC27:1;
   Uint16      TOC28:1;
   Uint16      TOC29:1;
   Uint16      TOC30:1;
   Uint16      TOC31:1;

};

union CANTOC_REG {
   Uint32              all;
   struct CANTOC_BITS  bit;
};

struct  CANTOS_BITS {
   Uint16      TOS0:1;
   Uint16      TOS1:1;
   Uint16      TOS2:1;
   Uint16      TOS3:1;
   Uint16      TOS4:1;
   Uint16      TOS5:1;
   Uint16      TOS6:1;
   Uint16      TOS7:1;
   Uint16      TOS8:1;
   Uint16      TOS9:1;
   Uint16      TOS10:1;
   Uint16      TOS11:1;
   Uint16      TOS12:1;
   Uint16      TOS13:1;
   Uint16      TOS14:1;
   Uint16      TOS15:1;
   Uint16      TOS16:1;
   Uint16      TOS17:1;
   Uint16      TOS18:1;
   Uint16      TOS19:1;
   Uint16      TOS20:1;
   Uint16      TOS21:1;
   Uint16      TOS22:1;
   Uint16      TOS23:1;
   Uint16      TOS24:1;
   Uint16      TOS25:1;
   Uint16      TOS26:1;
   Uint16      TOS27:1;
   Uint16      TOS28:1;
   Uint16      TOS29:1;
   Uint16      TOS30:1;
   Uint16      TOS31:1;

};

union CANTOS_REG {
   Uint32              all;
   struct CANTOS_BITS  bit;
};

struct ECAN_REGS {
   union CANME_REG   CANME;
   union CANMD_REG   CANMD;
   union CANTRS_REG  CANTRS;
   union CANTRR_REG  CANTRR;
   union CANTA_REG   CANTA;
   union CANAA_REG   CANAA;
   union CANRMP_REG  CANRMP;
   union CANRML_REG  CANRML;
   union CANRFP_REG  CANRFP;
   union CANGAM_REG  CANGAM;
   union CANMC_REG   CANMC;
   union CANBTC_REG  CANBTC;
   union CANES_REG   CANES;
   union CANTEC_REG  CANTEC;
   union CANREC_REG  CANREC;
   union CANGIF0_REG CANGIF0;
   union CANGIM_REG  CANGIM;
   union CANGIF1_REG CANGIF1;
   union CANMIM_REG  CANMIM;
   union CANMIL_REG  CANMIL;
   union CANOPC_REG  CANOPC;
   union CANTIOC_REG CANTIOC;
   union CANRIOC_REG CANRIOC;
   Uint32            CANTSC;
   union CANTOC_REG  CANTOC;
   union CANTOS_REG  CANTOS;

};

struct  CANMSGID_BITS {
   Uint16      EXTMSGID_L:16;
   Uint16      EXTMSGID_H:2;
   Uint16      STDMSGID:11;
   Uint16      AAM:1;
   Uint16      AME:1;
   Uint16      IDE:1;

};

union CANMSGID_REG {
   Uint32                all;
   struct CANMSGID_BITS  bit;
};

struct  CANMSGCTRL_BITS {
   Uint16 DLC:4;
   Uint16 RTR:1;
   Uint16 rsvd1:3;
   Uint16 TPL:5;
   Uint16 rsvd2:3;
   Uint16 rsvd3:16;
};

union CANMSGCTRL_REG {
   Uint32                  all;
   struct CANMSGCTRL_BITS  bit;
};

struct  CANMDL_WORDS {
   Uint16      LOW_WORD:16;
   Uint16      HI_WORD:16;
};

struct  CANMDL_BYTES {
   Uint16      BYTE3:8;
   Uint16      BYTE2:8;
   Uint16      BYTE1:8;
   Uint16      BYTE0:8;
};

union CANMDL_REG {
   Uint32                all;
   struct CANMDL_WORDS   word;
   struct CANMDL_BYTES   byte;
};

struct  CANMDH_WORDS {
   Uint16      LOW_WORD:16;
   Uint16      HI_WORD:16;
};

struct  CANMDH_BYTES {
   Uint16      BYTE7:8;
   Uint16      BYTE6:8;
   Uint16      BYTE5:8;
   Uint16      BYTE4:8;
};

union CANMDH_REG {
   Uint32                  all;
   struct CANMDH_WORDS     word;
   struct CANMDH_BYTES     byte;
};

struct MBOX {
   union CANMSGID_REG     MSGID;
   union CANMSGCTRL_REG   MSGCTRL;
   union CANMDL_REG       MDL;
   union CANMDH_REG       MDH;
};

struct ECAN_MBOXES {
   struct MBOX MBOX0;
   struct MBOX MBOX1;
   struct MBOX MBOX2;
   struct MBOX MBOX3;
   struct MBOX MBOX4;
   struct MBOX MBOX5;
   struct MBOX MBOX6;
   struct MBOX MBOX7;
   struct MBOX MBOX8;
   struct MBOX MBOX9;
   struct MBOX MBOX10;
   struct MBOX MBOX11;
   struct MBOX MBOX12;
   struct MBOX MBOX13;
   struct MBOX MBOX14;
   struct MBOX MBOX15;
   struct MBOX MBOX16;
   struct MBOX MBOX17;
   struct MBOX MBOX18;
   struct MBOX MBOX19;
   struct MBOX MBOX20;
   struct MBOX MBOX21;
   struct MBOX MBOX22;
   struct MBOX MBOX23;
   struct MBOX MBOX24;
   struct MBOX MBOX25;
   struct MBOX MBOX26;
   struct MBOX MBOX27;
   struct MBOX MBOX28;
   struct MBOX MBOX29;
   struct MBOX MBOX30;
   struct MBOX MBOX31;
};

struct  CANLAM_BITS {
   Uint16      LAM_L:16;
   Uint16      LAM_H:13;
   Uint16 rsvd1:2;
   Uint16      LAMI:1;
};

union CANLAM_REG {
   Uint32        all;
   struct CANLAM_BITS  bit;
};

struct LAM_REGS {
   union CANLAM_REG LAM0;
   union CANLAM_REG LAM1;
   union CANLAM_REG LAM2;
   union CANLAM_REG LAM3;
   union CANLAM_REG LAM4;
   union CANLAM_REG LAM5;
   union CANLAM_REG LAM6;
   union CANLAM_REG LAM7;
   union CANLAM_REG LAM8;
   union CANLAM_REG LAM9;
   union CANLAM_REG LAM10;
   union CANLAM_REG LAM11;
   union CANLAM_REG LAM12;
   union CANLAM_REG LAM13;
   union CANLAM_REG LAM14;
   union CANLAM_REG LAM15;
   union CANLAM_REG LAM16;
   union CANLAM_REG LAM17;
   union CANLAM_REG LAM18;
   union CANLAM_REG LAM19;
   union CANLAM_REG LAM20;
   union CANLAM_REG LAM21;
   union CANLAM_REG LAM22;
   union CANLAM_REG LAM23;
   union CANLAM_REG LAM24;
   union CANLAM_REG LAM25;
   union CANLAM_REG LAM26;
   union CANLAM_REG LAM27;
   union CANLAM_REG LAM28;
   union CANLAM_REG LAM29;
   union CANLAM_REG LAM30;
   union CANLAM_REG LAM31;
};

struct MOTS_REGS {
   Uint32 MOTS0;
   Uint32 MOTS1;
   Uint32 MOTS2;
   Uint32 MOTS3;
   Uint32 MOTS4;
   Uint32 MOTS5;
   Uint32 MOTS6;
   Uint32 MOTS7;
   Uint32 MOTS8;
   Uint32 MOTS9;
   Uint32 MOTS10;
   Uint32 MOTS11;
   Uint32 MOTS12;
   Uint32 MOTS13;
   Uint32 MOTS14;
   Uint32 MOTS15;
   Uint32 MOTS16;
   Uint32 MOTS17;
   Uint32 MOTS18;
   Uint32 MOTS19;
   Uint32 MOTS20;
   Uint32 MOTS21;
   Uint32 MOTS22;
   Uint32 MOTS23;
   Uint32 MOTS24;
   Uint32 MOTS25;
   Uint32 MOTS26;
   Uint32 MOTS27;
   Uint32 MOTS28;
   Uint32 MOTS29;
   Uint32 MOTS30;
   Uint32 MOTS31;
};

struct MOTO_REGS {
   Uint32 MOTO0;
   Uint32 MOTO1;
   Uint32 MOTO2;
   Uint32 MOTO3;
   Uint32 MOTO4;
   Uint32 MOTO5;
   Uint32 MOTO6;
   Uint32 MOTO7;
   Uint32 MOTO8;
   Uint32 MOTO9;
   Uint32 MOTO10;
   Uint32 MOTO11;
   Uint32 MOTO12;
   Uint32 MOTO13;
   Uint32 MOTO14;
   Uint32 MOTO15;
   Uint32 MOTO16;
   Uint32 MOTO17;
   Uint32 MOTO18;
   Uint32 MOTO19;
   Uint32 MOTO20;
   Uint32 MOTO21;
   Uint32 MOTO22;
   Uint32 MOTO23;
   Uint32 MOTO24;
   Uint32 MOTO25;
   Uint32 MOTO26;
   Uint32 MOTO27;
   Uint32 MOTO28;
   Uint32 MOTO29;
   Uint32 MOTO30;
   Uint32 MOTO31;
};

extern volatile struct ECAN_REGS ECanaRegs;
extern volatile struct ECAN_MBOXES ECanaMboxes;
extern volatile struct LAM_REGS ECanaLAMRegs;
extern volatile struct MOTO_REGS ECanaMOTORegs;
extern volatile struct MOTS_REGS ECanaMOTSRegs;

#ifdef __cplusplus
}
#endif

#endif
