
#ifndef DSP2803x_ADC_H
#define DSP2803x_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

struct ADCCTL1_BITS {
    Uint16  TEMPCONV:1;
    Uint16  VREFLOCONV:1;
    Uint16  INTPULSEPOS:1;
    Uint16  ADCREFSEL:1;
    Uint16  rsvd1:1;
    Uint16  ADCREFPWD:1;
    Uint16  ADCBGPWD:1;
    Uint16  ADCPWDN:1;
    Uint16  ADCBSYCHN:5;
    Uint16  ADCBSY:1;
    Uint16  ADCENABLE:1;
    Uint16  RESET:1;
};

union ADCCTL1_REG {
   Uint16                all;
   struct ADCCTL1_BITS   bit;
};

struct ADCCTL2_BITS {
    Uint16  CLKDIV2EN:1;
    Uint16  ADCNONOVERLAP:1;
    Uint16  rsvd1:14;
};

union ADCCTL2_REG {
   Uint16                all;
   struct ADCCTL2_BITS   bit;
};

struct ADCINT_BITS {
    Uint16  ADCINT1:1;
    Uint16  ADCINT2:1;
    Uint16  ADCINT3:1;
    Uint16  ADCINT4:1;
    Uint16  ADCINT5:1;
    Uint16  ADCINT6:1;
    Uint16  ADCINT7:1;
    Uint16  ADCINT8:1;
    Uint16  ADCINT9:1;
    Uint16  rsvd1:7;
};

union ADCINT_REG {
   Uint16                 all;
   struct ADCINT_BITS     bit;
};

struct INTSEL1N2_BITS {
    Uint16  INT1SEL:5;
    Uint16  INT1E:1;
    Uint16  INT1CONT:1;
    Uint16  rsvd1:1;
    Uint16  INT2SEL:5;
    Uint16  INT2E:1;
    Uint16  INT2CONT:1;
    Uint16  rsvd2:1;
};

union INTSEL1N2_REG {
    Uint16                all;
    struct INTSEL1N2_BITS bit;
};

struct INTSEL3N4_BITS {
    Uint16  INT3SEL:5;
    Uint16  INT3E:1;
    Uint16  INT3CONT:1;
    Uint16  rsvd1:1;
    Uint16  INT4SEL:5;
    Uint16  INT4E:1;
    Uint16  INT4CONT:1;
    Uint16  rsvd2:1;
};

union INTSEL3N4_REG {
    Uint16                all;
    struct INTSEL3N4_BITS bit;
};

struct INTSEL5N6_BITS {
    Uint16  INT5SEL:5;
    Uint16  INT5E:1;
    Uint16  INT5CONT:1;
    Uint16  rsvd1:1;
    Uint16  INT6SEL:5;
    Uint16  INT6E:1;
    Uint16  INT6CONT:1;
    Uint16  rsvd2:1;
};

union INTSEL5N6_REG {
    Uint16                all;
    struct INTSEL5N6_BITS bit;
};

struct INTSEL7N8_BITS {
    Uint16  INT7SEL:5;
    Uint16  INT7E:1;
    Uint16  INT7CONT:1;
    Uint16  rsvd1:1;
    Uint16  INT8SEL:5;
    Uint16  INT8E:1;
    Uint16  INT8CONT:1;
    Uint16  rsvd2:1;
};

union INTSEL7N8_REG {
    Uint16                all;
    struct INTSEL7N8_BITS bit;
};

struct INTSEL9N10_BITS {
    Uint16  INT9SEL:5;
    Uint16  INT9E:1;
    Uint16  INT9CONT:1;
    Uint16  rsvd1:1;
    Uint16  INT10SEL:5;
    Uint16  INT10E:1;
    Uint16  INT10CONT:1;
    Uint16  rsvd2:1;
};

union INTSEL9N10_REG {
    Uint16                all;
    struct INTSEL9N10_BITS bit;
};

struct SOCPRICTL_BITS {
    Uint16  SOCPRIORITY:5;
    Uint16  RRPOINTER:6;
    Uint16  rsvd1:4;
    Uint16  ONESHOT:1;
};

union SOCPRICTL_REG {
    Uint16                 all;
    struct SOCPRICTL_BITS  bit;
};

struct ADCSAMPLEMODE_BITS {
    Uint16 SIMULEN0:1;
    Uint16 SIMULEN2:1;
    Uint16 SIMULEN4:1;
    Uint16 SIMULEN6:1;
    Uint16 SIMULEN8:1;
    Uint16 SIMULEN10:1;
    Uint16 SIMULEN12:1;
    Uint16 SIMULEN14:1;
    Uint16 rsvd1:8;
};

union ADCSAMPLEMODE_REG {
    Uint16                      all;
    struct ADCSAMPLEMODE_BITS   bit;
};

struct ADCINTSOCSEL1_BITS {
    Uint16 SOC0:2;
    Uint16 SOC1:2;
    Uint16 SOC2:2;
    Uint16 SOC3:2;
    Uint16 SOC4:2;
    Uint16 SOC5:2;
    Uint16 SOC6:2;
    Uint16 SOC7:2;
};

union ADCINTSOCSEL1_REG {
    Uint16                    all;
    struct ADCINTSOCSEL1_BITS bit;
};

struct ADCINTSOCSEL2_BITS {
    Uint16 SOC8:2;
    Uint16 SOC9:2;
    Uint16 SOC10:2;
    Uint16 SOC11:2;
    Uint16 SOC12:2;
    Uint16 SOC13:2;
    Uint16 SOC14:2;
    Uint16 SOC15:2;
};

union ADCINTSOCSEL2_REG {
    Uint16                    all;
    struct ADCINTSOCSEL2_BITS bit;
};

struct ADCSOC_BITS {
    Uint16 SOC0:1;
    Uint16 SOC1:1;
    Uint16 SOC2:1;
    Uint16 SOC3:1;
    Uint16 SOC4:1;
    Uint16 SOC5:1;
    Uint16 SOC6:1;
    Uint16 SOC7:1;
    Uint16 SOC8:1;
    Uint16 SOC9:1;
    Uint16 SOC10:1;
    Uint16 SOC11:1;
    Uint16 SOC12:1;
    Uint16 SOC13:1;
    Uint16 SOC14:1;
    Uint16 SOC15:1;
};

union ADCSOC_REG {
    Uint16                    all;
    struct ADCSOC_BITS        bit;
};

struct ADCSOCxCTL_BITS {
    Uint16 ACQPS:6;
    Uint16 CHSEL:4;
    Uint16 rsvd1:1;
    Uint16 TRIGSEL:5;
};

union ADCSOCxCTL_REG {
    Uint16                  all;
    struct ADCSOCxCTL_BITS  bit;
};

struct ADCREFTRIM_BITS{
    Uint16  BG_FINE_TRIM:4;
    Uint16  BG_COARSE_TRIM:5;
    Uint16  EXTREF_FINE_TRIM:5;
    Uint16  rsvd1:2;
};

union ADCREFTRIM_REG{
    Uint16      all;
    struct ADCREFTRIM_BITS bit;
};

struct ADCOFFTRIM_BITS{
    int16   OFFTRIM:9;
    Uint16  rsvd1:7;
};

union ADCOFFTRIM_REG{
    Uint16      all;
    struct ADCOFFTRIM_BITS bit;
};

struct ADC_REGS {
    union  ADCCTL1_REG           ADCCTL1;
    union  ADCCTL2_REG           ADCCTL2;
    Uint16                       rsvd1[2];
    union  ADCINT_REG            ADCINTFLG;
    union  ADCINT_REG            ADCINTFLGCLR;
    union  ADCINT_REG            ADCINTOVF;
    union  ADCINT_REG            ADCINTOVFCLR;
    union  INTSEL1N2_REG         INTSEL1N2;
    union  INTSEL3N4_REG         INTSEL3N4;
    union  INTSEL5N6_REG         INTSEL5N6;
    union  INTSEL7N8_REG         INTSEL7N8;
    union  INTSEL9N10_REG        INTSEL9N10;
    Uint16                       rsvd2[3];
    union  SOCPRICTL_REG         SOCPRICTL;
    Uint16                       rsvd3;
    union  ADCSAMPLEMODE_REG     ADCSAMPLEMODE;
    Uint16                       rsvd4;
    union  ADCINTSOCSEL1_REG     ADCINTSOCSEL1;
    union  ADCINTSOCSEL2_REG     ADCINTSOCSEL2;
    Uint16                       rsvd5[2];
    union  ADCSOC_REG            ADCSOCFLG1;
    Uint16                       rsvd6;
    union  ADCSOC_REG            ADCSOCFRC1;
    Uint16                       rsvd7;
    union  ADCSOC_REG            ADCSOCOVF1;
    Uint16                       rsvd8;
    union  ADCSOC_REG            ADCSOCOVFCLR1;
    Uint16                       rsvd9;
    union  ADCSOCxCTL_REG        ADCSOC0CTL;
    union  ADCSOCxCTL_REG        ADCSOC1CTL;
    union  ADCSOCxCTL_REG        ADCSOC2CTL;
    union  ADCSOCxCTL_REG        ADCSOC3CTL;
    union  ADCSOCxCTL_REG        ADCSOC4CTL;
    union  ADCSOCxCTL_REG        ADCSOC5CTL;
    union  ADCSOCxCTL_REG        ADCSOC6CTL;
    union  ADCSOCxCTL_REG        ADCSOC7CTL;
    union  ADCSOCxCTL_REG        ADCSOC8CTL;
    union  ADCSOCxCTL_REG        ADCSOC9CTL;
    union  ADCSOCxCTL_REG        ADCSOC10CTL;
    union  ADCSOCxCTL_REG        ADCSOC11CTL;
    union  ADCSOCxCTL_REG        ADCSOC12CTL;
    union  ADCSOCxCTL_REG        ADCSOC13CTL;
    union  ADCSOCxCTL_REG        ADCSOC14CTL;
    union  ADCSOCxCTL_REG        ADCSOC15CTL;
    Uint16                       rsvd10 [16];
    union  ADCREFTRIM_REG        ADCREFTRIM;
    union  ADCOFFTRIM_REG        ADCOFFTRIM;
    Uint16                       rsvd11 [14];
};

struct ADC_RESULT_REGS
{
    Uint16                 ADCRESULT0;
    Uint16                 ADCRESULT1;
    Uint16                 ADCRESULT2;
    Uint16                 ADCRESULT3;
    Uint16                 ADCRESULT4;
    Uint16                 ADCRESULT5;
    Uint16                 ADCRESULT6;
    Uint16                 ADCRESULT7;
    Uint16                 ADCRESULT8;
    Uint16                 ADCRESULT9;
    Uint16                 ADCRESULT10;
    Uint16                 ADCRESULT11;
    Uint16                 ADCRESULT12;
    Uint16                 ADCRESULT13;
    Uint16                 ADCRESULT14;
    Uint16                 ADCRESULT15;
    Uint16                 rsvd[16];
};

extern volatile struct ADC_REGS AdcRegs;
extern volatile struct ADC_RESULT_REGS AdcResult;

#ifdef __cplusplus
}
#endif

#endif
