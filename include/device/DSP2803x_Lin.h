
#ifndef DSP2803x_LIN_H
#define DSP2803x_LIN_H

#ifdef __cplusplus
extern "C" {
#endif

struct SCIGCR0_BITS {
    Uint16  RESET:1;
    Uint16  rsvd1:15;
	Uint16  rsvd2:16;
};

union SCIGCR0_REG {
   Uint32               all;
   struct SCIGCR0_BITS  bit;
};

struct SCIGCR1_BITS {
    Uint16  COMMMODE:1;
    Uint16  TIMINGMODE:1;
    Uint16  PARITYENA:1;
    Uint16  PARITY:1;
    Uint16  STOP:1;
    Uint16  CLK_MASTER:1;
    Uint16  LINMODE:1;
    Uint16  SWnRST:1;
    Uint16  SLEEP:1;
    Uint16  ADAPT:1;
    Uint16  MBUFMODE:1;
    Uint16  CTYPE:1;
    Uint16  HGENCTRL:1;
    Uint16  STOPEXTFRAME:1;
    Uint16  rsvd2:2;
    Uint16  LOOPBACK:1;
    Uint16  CONT:1;
    Uint16  rsvd3:6;
    Uint16  RXENA:1;
    Uint16  TXENA:1;
    Uint16  rsvd4:6;
};

union SCIGCR1_REG {
   Uint32               all;
   struct SCIGCR1_BITS  bit;
};

struct SCIGCR2_BITS {
    Uint16  POWERDOWN:1;
    Uint16  rsvd1:7;
    Uint16  GENWU:1;
    Uint16  rsvd2:7;
    Uint16  SC:1;
    Uint16  CC:1;
    Uint16  rsvd3:14;
};

union SCIGCR2_REG {
   Uint32               all;
   struct SCIGCR2_BITS  bit;
};

struct SCISETINT_BITS {
    Uint16  SETBRKDTINT:1;
    Uint16  SETWAKEUPINT:1;
    Uint16  rsvd1:2;
    Uint16  SETTIMEOUTINT:1;
    Uint16  rsvd2:1;
    Uint16  SETTOAWUSINT:1;
    Uint16  SETTOA3WUSINT:1;
    Uint16  SETTXINT:1;
    Uint16  SETRXINT:1;
    Uint16  rsvd3:3;
    Uint16  SETIDINT:1;
    Uint16  rsvd4:2;
	Uint16  rsvd5:2;
    Uint16  rsvd6:1;
    Uint16  rsvd7:5;
    Uint16  SETPEINT:1;
    Uint16  SETOEINT:1;
    Uint16  SETFEINT:1;
    Uint16  SETNREINT:1;
    Uint16  SETISFEINT:1;
    Uint16  SETCEINT:1;
    Uint16  SETPBEINT:1;
    Uint16  SETBEINT:1;
};

union SCISETINT_REG {
   Uint32                 all;
   struct SCISETINT_BITS  bit;
};

struct SCICLEARINT_BITS {
    Uint16  CLRBRKDTINT:1;
    Uint16  CLRWAKEUPINT:1;
    Uint16  rsvd1:2;
    Uint16  CLRTIMEOUTINT:1;
    Uint16  rsvd2:1;
    Uint16  CLRTOAWUSINT:1;
    Uint16  CLRTOA3WUSINT:1;
    Uint16  CLRTXINT:1;
    Uint16  CLRRXINT:1;
    Uint16  rsvd3:3;
    Uint16  CLRIDINT:1;
    Uint16  rsvd4:2;
	Uint16  rsvd5:2;
    Uint16  rsvd6:1;
    Uint16  rsvd7:5;
    Uint16  CLRPEINT:1;
    Uint16  CLROEINT:1;
    Uint16  CLRFEINT:1;
    Uint16  CLRNREINT:1;
    Uint16  CLRISFEINT:1;
    Uint16  CLRCEINT:1;
    Uint16  CLRPBEINT:1;
    Uint16  CLRBEINT:1;
};

union SCICLEARINT_REG {
   Uint32                   all;
   struct SCICLEARINT_BITS  bit;
};

struct SCISETINTLVL_BITS {
    Uint16  SETBRKDTINTLVL:1;
    Uint16  SETWAKEUPINTLVL:1;
    Uint16  rsvd1:2;
    Uint16  SETTIMEOUTINTLVL:1;
    Uint16  rsvd2:1;
    Uint16  SETTOAWUSINTLVL:1;
    Uint16  SETTOA3WUSINTLVL:1;
    Uint16  SETTXINTLVL:1;
    Uint16  SETRXINTOVO:1;
    Uint16  rsvd3:3;
    Uint16  SETIDINTLVL:1;
    Uint16  rsvd4:2;
	Uint16  rsvd5:2;
    Uint16  rsvd6:1;
    Uint16  rsvd7:5;
    Uint16  SETPEINTLVL:1;
    Uint16  SETOEINTLVL:1;
    Uint16  SETFEINTLVL:1;
    Uint16  SETNREINTLVL:1;
    Uint16  SETISFEINTLVL:1;
    Uint16  SETCEINTLVL:1;
    Uint16  SETPBEINTLVL:1;
    Uint16  SETBEINTLVL:1;
};

union SCISETINTLVL_REG {
   Uint32                    all;
   struct SCISETINTLVL_BITS  bit;
};

struct SCICLEARINTLVL_BITS {
    Uint16  CLRBRKDTINTLVL:1;
    Uint16  CLRWAKEUPINTLVL:1;
    Uint16  rsvd1:2;
    Uint16  CLRTIMEOUTINTLVL:1;
    Uint16  rsvd2:1;
    Uint16  CLRTOAWUSINTLVL:1;
    Uint16  CLRTOA3WUSINTLVL:1;
    Uint16  CLRTXINTLVL:1;
    Uint16  CLRRXINTLVL:1;
    Uint16  rsvd3:3;
    Uint16  CLRIDINTLVL:1;
    Uint16  rsvd4:2;
	Uint16  rsvd5:2;
    Uint16  rsvd6:1;
    Uint16  rsvd7:5;
    Uint16  CLRPEINTLVL:1;
    Uint16  CLROEINTLVL:1;
    Uint16  CLRFEINTLVL:1;
    Uint16  CLRNREINTLVL:1;
    Uint16  CLRISFEINTLVL:1;
    Uint16  CLRCEINTLVL:1;
    Uint16  CLRPBEINTLVL:1;
    Uint16  CLRBEINTLVL:1;
};

union SCICLEARINTLVL_REG {
   Uint32                      all;
   struct SCICLEARINTLVL_BITS  bit;
};

struct SCIFLR_BITS {
    Uint16  BRKDT:1;
    Uint16  WAKEUP:1;
    Uint16  IDLE:1;
    Uint16  BUSY:1;
    Uint16  TIMEOUT:1;
    Uint16  rsvd2:1;
    Uint16  TOAWUS:1;
    Uint16  TOA3WUS:1;
    Uint16  TXRDY:1;
    Uint16  RXRDY:1;
    Uint16  TXWAKE:1;
    Uint16  TXEMPTY:1;
    Uint16  RXWAKE:1;
    Uint16  IDTXFLAG:1;
    Uint16  IDRXFLAG:1;
	Uint16	rsvd3:1;
    Uint16  rsvd4:8;
    Uint16  PE:1;
    Uint16  OE:1;
    Uint16  FE:1;
    Uint16  NRE:1;
    Uint16  ISFE:1;
    Uint16  CE:1;
    Uint16  PBE:1;
    Uint16  BE:1;
};

union SCIFLR_REG {
   Uint32             all;
   struct SCIFLR_BITS  bit;
};

struct SCIINTVECT0_BITS {
    Uint16  INTVECT0:5;
    Uint16  rsvd1:11;
	Uint16  rsvd2:16;
};

union SCIINTVECT0_REG {
   Uint32               all;
   struct SCIINTVECT0_BITS  bit;
};

struct SCIINTVECT1_BITS {
    Uint16  INTVECT1:5;
    Uint16  rsvd1:11;
	Uint16  rsvd2:16;
};

union SCIINTVECT1_REG {
   Uint32               all;
   struct SCIINTVECT1_BITS  bit;
};

struct SCIFORMAT_BITS {
    Uint16  CHAR:3;
    Uint16  rsvd1:13;
    Uint16  LENGTH:3;
    Uint16  rsvd2:13;
};

union SCIFORMAT_REG {
   Uint32                 all;
   struct SCIFORMAT_BITS  bit;
};

struct BRSR_BITS {
	Uint16  SCI_LIN_PSL :16;
	Uint16  SCI_LIN_PSH :8;
    Uint16  M:4;
    Uint16  rsvd1:4;
};

union BRSR_REG {
   Uint32               all;
   struct BRSR_BITS     bit;
};

struct SCIPIO2_BITS {
    Uint16  rsvd1:1;
    Uint16  RXIN:1;
    Uint16  TXIN:1;
    Uint16  rsvd2:13;
	Uint16  rsvd3:16;
};

union SCIPIO2_REG {
   Uint32               all;
   struct SCIPIO2_BITS  bit;
};

struct LINCOMP_BITS {
    Uint16  SBREAK:3;
    Uint16  rsvd1:5;
    Uint16  SDEL:2;
	Uint16  rsvd2:6;
	Uint16  rsvd3:16;
};

union LINCOMP_REG {
   Uint32               all;
   struct LINCOMP_BITS  bit;
};

struct LINRD0_BITS {
    Uint16  RD3:8;
    Uint16  RD2:8;
    Uint16  RD1:8;
    Uint16  RD0:8;
};

union LINRD0_REG {
   Uint32              all;
   struct LINRD0_BITS  bit;
};

struct LINRD1_BITS {
    Uint16  RD7:8;
    Uint16  RD6:8;
    Uint16  RD5:8;
    Uint16  RD4:8;
};

union LINRD1_REG {
   Uint32              all;
   struct LINRD1_BITS  bit;
};

struct LINMASK_BITS {
    Uint16  TXIDMASK:8;
    Uint16  rsvd1:8;
    Uint16  RXIDMASK:8;
    Uint16  rsvd2:8;
};

union LINMASK_REG {
   Uint32               all;
   struct LINMASK_BITS  bit;
};

struct LINID_BITS {
    Uint16  IDBYTE:8;
    Uint16  IDSLAVETASKBYTE:8;
    Uint16  RECEIVEDID:8;
    Uint16  rsvd1:8;
};

union LINID_REG {
   Uint32             all;
   struct LINID_BITS  bit;
};

struct LINTD0_BITS {
    Uint16  TD3:8;
    Uint16  TD2:8;
    Uint16  TD1:8;
    Uint16  TD0:8;
};

union LINTD0_REG {
   Uint32              all;
   struct LINTD0_BITS  bit;
};

struct LINTD1_BITS {
    Uint16  TD7:8;
    Uint16  TD6:8;
    Uint16  TD5:8;
    Uint16  TD4:8;
};

union LINTD1_REG {
   Uint32               all;
   struct LINTD1_BITS   bit;
};

struct IODFTCTRL_BITS {
    Uint16  RXPENA:1;
    Uint16  LPBENA:1;
    Uint16  rsvd1:6;
    Uint16  IODFTENA:4;
    Uint16  rsvd2:4;
    Uint16  TXSHIFT:3;
    Uint16  PINSAMPLEMASK:2;
    Uint16  rsvd3:3;
    Uint16  BRKDTERRENA:1;
    Uint16  PERRENA:1;
    Uint16  FERRENA:1;
    Uint16  rsvd:1;
    Uint16  ISFERRENA:1;
    Uint16  CERRENA:1;
    Uint16  PBERRENA:1;
    Uint16  BERRENA:1;
};

union IODFTCTRL_REG {
   Uint32                 all;
   struct IODFTCTRL_BITS  bit;
};

struct LIN_REGS {
    union   SCIGCR0_REG         SCIGCR0;
	union	SCIGCR1_REG		    SCIGCR1;
	union	SCIGCR2_REG		    SCIGCR2;
	union	SCISETINT_REG		SCISETINT;
	union	SCICLEARINT_REG		SCICLEARINT;
	union	SCISETINTLVL_REG	SCISETINTLVL;
	union	SCICLEARINTLVL_REG	SCICLEARINTLVL;
	union	SCIFLR_REG		    SCIFLR;
	union	SCIINTVECT0_REG		SCIINTVECT0;
	union	SCIINTVECT1_REG	 	SCIINTVECT1;
	union	SCIFORMAT_REG		SCIFORMAT;
	union	BRSR_REG			BRSR;
	Uint32          			SCIED;
	Uint32          			SCIRD;
	Uint32          			SCITD;
	Uint32                      rsvd1[2];
	union	SCIPIO2_REG			SCIPIO2;
	Uint32                      rsvd2[6];
	union	LINCOMP_REG			LINCOMP;
	union	LINRD0_REG			LINRD0;
	union	LINRD1_REG			LINRD1;
	union	LINMASK_REG			LINMASK;
	union	LINID_REG			LINID;
	union	LINTD0_REG			LINTD0;
	union	LINTD1_REG			LINTD1;
	Uint32			            MBRSR;
	Uint32						rsvd3[4];
	union   IODFTCTRL_REG		IODFTCTRL;
};

extern volatile struct LIN_REGS LinaRegs;

#ifdef __cplusplus
}
#endif

#endif
