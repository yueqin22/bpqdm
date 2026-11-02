
#ifndef DSP2803x_DEFAULT_ISR_H
#define DSP2803x_DEFAULT_ISR_H

#ifdef __cplusplus
extern "C" {
#endif

interrupt void INT13_ISR(void);
interrupt void INT14_ISR(void);
interrupt void DATALOG_ISR(void);
interrupt void RTOSINT_ISR(void);
interrupt void EMUINT_ISR(void);
interrupt void NMI_ISR(void);
interrupt void ILLEGAL_ISR(void);
interrupt void USER1_ISR(void);
interrupt void USER2_ISR(void);
interrupt void USER3_ISR(void);
interrupt void USER4_ISR(void);
interrupt void USER5_ISR(void);
interrupt void USER6_ISR(void);
interrupt void USER7_ISR(void);
interrupt void USER8_ISR(void);
interrupt void USER9_ISR(void);
interrupt void USER10_ISR(void);
interrupt void USER11_ISR(void);
interrupt void USER12_ISR(void);

interrupt void  ADCINT1_ISR(void);
interrupt void  ADCINT2_ISR(void);
interrupt void  XINT1_ISR(void);
interrupt void  XINT2_ISR(void);
interrupt void  ADCINT9_ISR(void);
interrupt void  TINT0_ISR(void);
interrupt void  WAKEINT_ISR(void);

interrupt void EPWM1_TZINT_ISR(void);
interrupt void EPWM2_TZINT_ISR(void);
interrupt void EPWM3_TZINT_ISR(void);
interrupt void EPWM4_TZINT_ISR(void);
interrupt void EPWM5_TZINT_ISR(void);
interrupt void EPWM6_TZINT_ISR(void);
interrupt void EPWM7_TZINT_ISR(void);

interrupt void EPWM1_INT_ISR(void);
interrupt void EPWM2_INT_ISR(void);
interrupt void EPWM3_INT_ISR(void);
interrupt void EPWM4_INT_ISR(void);
interrupt void EPWM5_INT_ISR(void);
interrupt void EPWM6_INT_ISR(void);
interrupt void EPWM7_INT_ISR(void);

interrupt void ECAP1_INT_ISR(void);

interrupt void EQEP1_INT_ISR(void);

interrupt void SPIRXINTA_ISR(void);
interrupt void SPITXINTA_ISR(void);
interrupt void SPIRXINTB_ISR(void);
interrupt void SPITXINTB_ISR(void);

interrupt void I2CINT1A_ISR(void);
interrupt void I2CINT2A_ISR(void);

interrupt void SCIRXINTA_ISR(void);
interrupt void SCITXINTA_ISR(void);
interrupt void LIN0INTA_ISR (void);
interrupt void LIN1INTA_ISR (void);
interrupt void ECAN0INTA_ISR(void);
interrupt void ECAN1INTA_ISR(void);

interrupt void ADCINT3_ISR(void);
interrupt void ADCINT4_ISR(void);
interrupt void ADCINT5_ISR(void);
interrupt void ADCINT6_ISR(void);
interrupt void ADCINT7_ISR(void);
interrupt void ADCINT8_ISR(void);

interrupt void CLA1_INT1_ISR(void);
interrupt void CLA1_INT2_ISR(void);
interrupt void CLA1_INT3_ISR(void);
interrupt void CLA1_INT4_ISR(void);
interrupt void CLA1_INT5_ISR(void);
interrupt void CLA1_INT6_ISR(void);
interrupt void CLA1_INT7_ISR(void);
interrupt void CLA1_INT8_ISR(void);

interrupt void XINT3_ISR (void);
interrupt void LVF_ISR(void);
interrupt void LUF_ISR(void);

interrupt void PIE_RESERVED(void);
interrupt void rsvd_ISR(void);
interrupt void INT_NOTUSED_ISR(void);

#ifdef __cplusplus
}
#endif

#endif
