
#ifndef DSP2803x_PIE_CTRL_H
#define DSP2803x_PIE_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

struct PIECTRL_BITS {
   Uint16  ENPIE:1;
   Uint16  PIEVECT:15;
};

union PIECTRL_REG {
   Uint16                 all;
   struct PIECTRL_BITS  bit;
};

struct PIEIER_BITS {
   Uint16 INTx1:1;
   Uint16 INTx2:1;
   Uint16 INTx3:1;
   Uint16 INTx4:1;
   Uint16 INTx5:1;
   Uint16 INTx6:1;
   Uint16 INTx7:1;
   Uint16 INTx8:1;
   Uint16 rsvd:8;
};

union PIEIER_REG {
   Uint16              all;
   struct PIEIER_BITS  bit;
};

struct PIEIFR_BITS {
   Uint16 INTx1:1;
   Uint16 INTx2:1;
   Uint16 INTx3:1;
   Uint16 INTx4:1;
   Uint16 INTx5:1;
   Uint16 INTx6:1;
   Uint16 INTx7:1;
   Uint16 INTx8:1;
   Uint16 rsvd:8;
};

union PIEIFR_REG {
   Uint16              all;
   struct PIEIFR_BITS  bit;
};

struct PIEACK_BITS {
   Uint16 ACK1:1;
   Uint16 ACK2:1;
   Uint16 ACK3:1;
   Uint16 ACK4:1;
   Uint16 ACK5:1;
   Uint16 ACK6:1;
   Uint16 ACK7:1;
   Uint16 ACK8:1;
   Uint16 ACK9:1;
   Uint16 ACK10:1;
   Uint16 ACK11:1;
   Uint16 ACK12:1;
   Uint16 rsvd:4;
};

union PIEACK_REG {
   Uint16              all;
   struct PIEACK_BITS  bit;
};

struct PIE_CTRL_REGS {
   union PIECTRL_REG PIECTRL;
   union PIEACK_REG  PIEACK;
   union PIEIER_REG  PIEIER1;
   union PIEIFR_REG  PIEIFR1;
   union PIEIER_REG  PIEIER2;
   union PIEIFR_REG  PIEIFR2;
   union PIEIER_REG  PIEIER3;
   union PIEIFR_REG  PIEIFR3;
   union PIEIER_REG  PIEIER4;
   union PIEIFR_REG  PIEIFR4;
   union PIEIER_REG  PIEIER5;
   union PIEIFR_REG  PIEIFR5;
   union PIEIER_REG  PIEIER6;
   union PIEIFR_REG  PIEIFR6;
   union PIEIER_REG  PIEIER7;
   union PIEIFR_REG  PIEIFR7;
   union PIEIER_REG  PIEIER8;
   union PIEIFR_REG  PIEIFR8;
   union PIEIER_REG  PIEIER9;
   union PIEIFR_REG  PIEIFR9;
   union PIEIER_REG  PIEIER10;
   union PIEIFR_REG  PIEIFR10;
   union PIEIER_REG  PIEIER11;
   union PIEIFR_REG  PIEIFR11;
   union PIEIER_REG  PIEIER12;
   union PIEIFR_REG  PIEIFR12;
};

#define PIEACK_GROUP1   0x0001
#define PIEACK_GROUP2   0x0002
#define PIEACK_GROUP3   0x0004
#define PIEACK_GROUP4   0x0008
#define PIEACK_GROUP5   0x0010
#define PIEACK_GROUP6   0x0020
#define PIEACK_GROUP7   0x0040
#define PIEACK_GROUP8   0x0080
#define PIEACK_GROUP9   0x0100
#define PIEACK_GROUP10  0x0200
#define PIEACK_GROUP11  0x0400
#define PIEACK_GROUP12  0x0800

extern volatile struct PIE_CTRL_REGS PieCtrlRegs;

#ifdef __cplusplus
}
#endif

#endif
