
#ifndef DSP2803x_I2C_H
#define DSP2803x_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

struct I2CISRC_BITS {
   Uint16 INTCODE:3;
   Uint16 rsvd1:13;
};

union I2CISRC_REG {
   Uint16                 all;
   struct I2CISRC_BITS    bit;
};

struct I2CIER_BITS {
   Uint16 ARBL:1;
   Uint16 NACK:1;
   Uint16 ARDY:1;
   Uint16 RRDY:1;
   Uint16 XRDY:1;
   Uint16 SCD:1;
   Uint16 AAS:1;
   Uint16 rsvd:9;
};

union I2CIER_REG {
   Uint16                 all;
   struct I2CIER_BITS     bit;
};

struct I2CSTR_BITS {
   Uint16 ARBL:1;
   Uint16 NACK:1;
   Uint16 ARDY:1;
   Uint16 RRDY:1;
   Uint16 XRDY:1;
   Uint16 SCD:1;
   Uint16 rsvd1:2;
   Uint16 AD0:1;
   Uint16 AAS:1;
   Uint16 XSMT:1;
   Uint16 RSFULL:1;
   Uint16 BB:1;
   Uint16 NACKSNT:1;
   Uint16 SDIR:1;
   Uint16 rsvd2:1;
};

union I2CSTR_REG {
   Uint16                 all;
   struct I2CSTR_BITS     bit;
};

struct I2CMDR_BITS {
   Uint16 BC:3;
   Uint16 FDF:1;
   Uint16 STB:1;
   Uint16 IRS:1;
   Uint16 DLB:1;
   Uint16 RM:1;
   Uint16 XA:1;
   Uint16 TRX:1;
   Uint16 MST:1;
   Uint16 STP:1;
   Uint16 rsvd1:1;
   Uint16 STT:1;
   Uint16 FREE:1;
   Uint16 NACKMOD:1;
};

union I2CMDR_REG {
   Uint16                 all;
   struct I2CMDR_BITS     bit;
};

struct I2CPSC_BITS {
   Uint16 IPSC:8;
   Uint16 rsvd1:8;
};

union I2CPSC_REG {
   Uint16                 all;
   struct I2CPSC_BITS     bit;
};

struct I2CFFTX_BITS {
   Uint16 TXFFIL:5;
   Uint16 TXFFIENA:1;
   Uint16 TXFFINTCLR:1;
   Uint16 TXFFINT:1;
   Uint16 TXFFST:5;
   Uint16 TXFFRST:1;
   Uint16 I2CFFEN:1;
   Uint16 rsvd1:1;

};

union I2CFFTX_REG {
   Uint16                 all;
   struct I2CFFTX_BITS    bit;
};

struct I2CFFRX_BITS {
   Uint16 RXFFIL:5;
   Uint16 RXFFIENA:1;
   Uint16 RXFFINTCLR:1;
   Uint16 RXFFINT:1;
   Uint16 RXFFST:5;
   Uint16 RXFFRST:1;
   Uint16 rsvd1:2;
};

union I2CFFRX_REG {
   Uint16                 all;
   struct I2CFFRX_BITS    bit;
};

struct I2C_REGS {
   Uint16              I2COAR;
   union  I2CIER_REG   I2CIER;
   union  I2CSTR_REG   I2CSTR;
   Uint16              I2CCLKL;
   Uint16              I2CCLKH;
   Uint16              I2CCNT;
   Uint16              I2CDRR;
   Uint16              I2CSAR;
   Uint16              I2CDXR;
   union  I2CMDR_REG   I2CMDR;
   union  I2CISRC_REG  I2CISRC;
   Uint16              rsvd1;
   union  I2CPSC_REG   I2CPSC;
   Uint16              rsvd2[19];
   union  I2CFFTX_REG  I2CFFTX;
   union  I2CFFRX_REG  I2CFFRX;
};

extern volatile struct I2C_REGS I2caRegs;

#ifdef __cplusplus
}
#endif

#endif
