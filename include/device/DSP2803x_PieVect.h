
#ifndef DSP2803x_PIE_VECT_H
#define DSP2803x_PIE_VECT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef interrupt void(*PINT)(void);

struct PIE_VECT_TABLE {

      PINT     PIE1_RESERVED;
      PINT     PIE2_RESERVED;
      PINT     PIE3_RESERVED;
      PINT     PIE4_RESERVED;
      PINT     PIE5_RESERVED;
      PINT     PIE6_RESERVED;
      PINT     PIE7_RESERVED;
      PINT     PIE8_RESERVED;
      PINT     PIE9_RESERVED;
      PINT     PIE10_RESERVED;
      PINT     PIE11_RESERVED;
      PINT     PIE12_RESERVED;
      PINT     PIE13_RESERVED;

      PINT     TINT1;
      PINT     TINT2;
      PINT     DATALOG;
      PINT     RTOSINT;
      PINT     EMUINT;
      PINT     NMI;
      PINT     ILLEGAL;
      PINT     USER1;
      PINT     USER2;
      PINT     USER3;
      PINT     USER4;
      PINT     USER5;
      PINT     USER6;
      PINT     USER7;
      PINT     USER8;
      PINT     USER9;
      PINT     USER10;
      PINT     USER11;
      PINT     USER12;

      PINT     ADCINT1;
      PINT     ADCINT2;
      PINT     rsvd1_3;
      PINT     XINT1;
      PINT     XINT2;
      PINT     ADCINT9;
      PINT     TINT0;
      PINT     WAKEINT;

      PINT     EPWM1_TZINT;
      PINT     EPWM2_TZINT;
      PINT     EPWM3_TZINT;
      PINT     EPWM4_TZINT;
      PINT     EPWM5_TZINT;
      PINT     EPWM6_TZINT;
      PINT     EPWM7_TZINT;
      PINT     rsvd2_8;

      PINT     EPWM1_INT;
      PINT     EPWM2_INT;
      PINT     EPWM3_INT;
      PINT     EPWM4_INT;
      PINT     EPWM5_INT;
      PINT     EPWM6_INT;
      PINT     EPWM7_INT;
      PINT     rsvd3_8;

      PINT     ECAP1_INT;
      PINT     rsvd4_2;
      PINT     rsvd4_3;
      PINT     rsvd4_4;
      PINT     rsvd4_5;
      PINT     rsvd4_6;
      PINT     rsvd4_7;
      PINT     rsvd4_8;

      PINT     EQEP1_INT;
      PINT     rsvd5_2;
      PINT     rsvd5_3;
      PINT     rsvd5_4;
      PINT     rsvd5_5;
      PINT     rsvd5_6;
      PINT     rsvd5_7;
      PINT     rsvd5_8;

      PINT     SPIRXINTA;
      PINT     SPITXINTA;
      PINT     SPIRXINTB;
      PINT     SPITXINTB;
      PINT     rsvd6_5;
      PINT     rsvd6_6;
      PINT     rsvd6_7;
      PINT     rsvd6_8;

      PINT     rsvd7_1;
      PINT     rsvd7_2;
      PINT     rsvd7_3;
      PINT     rsvd7_4;
      PINT     rsvd7_5;
      PINT     rsvd7_6;
      PINT     rsvd7_7;
      PINT     rsvd7_8;

      PINT     I2CINT1A;
      PINT     I2CINT2A;
      PINT     rsvd8_3;
      PINT     rsvd8_4;
      PINT     rsvd8_5;
      PINT     rsvd8_6;
      PINT     rsvd8_7;
      PINT     rsvd8_8;

      PINT     SCIRXINTA;
      PINT     SCITXINTA;
      PINT     LIN0INTA;
      PINT     LIN1INTA;
      PINT     ECAN0INTA;
      PINT     ECAN1INTA;
      PINT     rsvd9_7;
      PINT     rsvd9_8;

      PINT     rsvd10_1;
      PINT     rsvd10_2;
      PINT     ADCINT3;
      PINT     ADCINT4;
      PINT     ADCINT5;
      PINT     ADCINT6;
      PINT     ADCINT7;
      PINT     ADCINT8;

      PINT     CLA1_INT1;
      PINT     CLA1_INT2;
      PINT     CLA1_INT3;
      PINT     CLA1_INT4;
      PINT     CLA1_INT5;
      PINT     CLA1_INT6;
      PINT     CLA1_INT7;
      PINT     CLA1_INT8;

      PINT     XINT3;
      PINT     rsvd12_2;
      PINT     rsvd12_3;
      PINT     rsvd12_4;
      PINT     rsvd12_5;
      PINT     rsvd12_6;
      PINT     LVF;
      PINT     LUF;
};

extern struct PIE_VECT_TABLE PieVectTable;

#ifdef __cplusplus
}
#endif

#endif
