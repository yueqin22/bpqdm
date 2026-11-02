# 中断清单 (interrupt_list)

## 1. 中断向量表 (来自 system/DSP2803x_CodeStartBranch.asm 与 PIE 初始化)

入口段: `codestart` -> `BEGIN (0x3F7FF6)`; PIE 向量表位于 `PieVectTable` 结构。

## 2. 出现的 ISR 符号 (按文件)

| 文件 | ISR 符号 |
|---|---|
| `function/f_comm.c` | `Lina_Level0_ISR`<br>`Lina_Level1_ISR` |
| `public/28035_header/DSP2803x_DefaultISR.h` | `ADCINT1_ISR`<br>`ADCINT2_ISR`<br>`ADCINT3_ISR`<br>`ADCINT4_ISR`<br>`ADCINT5_ISR`<br>`ADCINT6_ISR`<br>`ADCINT7_ISR`<br>`ADCINT8_ISR`<br>`ADCINT9_ISR`<br>`CLA1_INT1_ISR`<br>`CLA1_INT2_ISR`<br>`CLA1_INT3_ISR`<br>`CLA1_INT4_ISR`<br>`CLA1_INT5_ISR`<br>`CLA1_INT6_ISR`<br>`CLA1_INT7_ISR`<br>`CLA1_INT8_ISR`<br>`DATALOG_ISR`<br>`ECAN0INTA_ISR`<br>`ECAN1INTA_ISR`<br>`ECAP1_INT_ISR`<br>`EMUINT_ISR`<br>`EPWM1_INT_ISR`<br>`EPWM1_TZINT_ISR`<br>`EPWM2_INT_ISR`<br>`EPWM2_TZINT_ISR`<br>`EPWM3_INT_ISR`<br>`EPWM3_TZINT_ISR`<br>`EPWM4_INT_ISR`<br>`EPWM4_TZINT_ISR`<br>`EPWM5_INT_ISR`<br>`EPWM5_TZINT_ISR`<br>`EPWM6_INT_ISR`<br>`EPWM6_TZINT_ISR`<br>`EPWM7_INT_ISR`<br>`EPWM7_TZINT_ISR`<br>`EQEP1_INT_ISR`<br>`I2CINT1A_ISR`<br>`I2CINT2A_ISR`<br>`ILLEGAL_ISR`<br>`INT13_ISR`<br>`INT14_ISR`<br>`INT_NOTUSED_ISR`<br>`LIN0INTA_ISR`<br>`LIN1INTA_ISR`<br>`LUF_ISR`<br>`LVF_ISR`<br>`NMI_ISR`<br>`RTOSINT_ISR`<br>`SCIRXINTA_ISR`<br>`SCITXINTA_ISR`<br>`SPIRXINTA_ISR`<br>`SPIRXINTB_ISR`<br>`SPITXINTA_ISR`<br>`SPITXINTB_ISR`<br>`TINT0_ISR`<br>`USER10_ISR`<br>`USER11_ISR`<br>`USER12_ISR`<br>`USER1_ISR`<br>`USER2_ISR`<br>`USER3_ISR`<br>`USER4_ISR`<br>`USER5_ISR`<br>`USER6_ISR`<br>`USER7_ISR`<br>`USER8_ISR`<br>`USER9_ISR`<br>`WAKEINT_ISR`<br>`XINT1_ISR`<br>`XINT2_ISR`<br>`XINT3_ISR`<br>`rsvd_ISR` |
| `system/mainInit_2803x.c` | `rsvd_ISR` |

## 3. ISR 函数定义

| 文件 | 定义 |
|---|---|
| `function/f_comm.c` | `Lina_Level0_ISR`<br>`Lina_Level1_ISR`<br>`SCI_RXD_isr`<br>`SCI_TXD_isr`<br>`SCI_RXD_isr`<br>`SCI_TXD_isr`<br>`Lina_Level0_ISR`<br>`Lina_Level1_ISR`<br>`SCI_RXD_isr`<br>`SCI_TXD_isr` |
| `function/f_comm_DP.c` | `SCI_DP_RXD_isr`<br>`SCI_DP_TXD_isr`<br>`SCI_DP_RXD_isr`<br>`SCI_DP_TXD_isr` |
| `function/f_osc.c` | `sciaTxFifoIsr`<br>`sciaRxFifoIsr` |
| `function/main.c` | `cpu_timer0_isr`<br>`ADC_Over_isr`<br>`EPWM1_zero_isr`<br>`EPWM1_TZ_isr` |
| `function/z_func.c` | `cpu_timer0_isr` |
| `motor/MotorImParEst.c` | `RrLoInPeriodAndCmpInt`<br>`RrLoInPeriodAndCmpInt` |
| `motor/MotorPmsmMain.c` | `PG_Zero_isr` |
| `system/main.c` | `cpu_timer0_isr`<br>`ADC_Over_isr`<br>`EPWM1_zero_isr`<br>`EPWM1_TZ_isr` |
| `system/mainInit_2803x.c` | `rsvd_ISR` |

## 4. PIE 向量赋值 (PieVectTable.X = &Func)

| 文件 | 赋值 |
|---|---|
| `function/f_comm.c` | `PieVectTable.LIN0INTA = &Lina_Level0_ISR;`<br>`PieVectTable.LIN1INTA = &Lina_Level1_ISR;`<br>`PieVectTable.SCIRXINTA = SCI_RXD_isr;`<br>`PieVectTable.SCITXINTA = SCI_TXD_isr;` |
| `function/f_comm_DP.c` | `PieVectTable.SCIRXINTA = SCI_DP_RXD_isr;`<br>`PieVectTable.SCITXINTA = SCI_DP_TXD_isr;`<br>`PieVectTable.SCIRXINTA = SCI_DP_RXD_isr;`<br>`PieVectTable.SCITXINTA = SCI_DP_TXD_isr;` |
| `function/f_osc.c` | `PieVectTable.SCIRXINTA = &sciaRxFifoIsr;`<br>`PieVectTable.SCITXINTA = &sciaTxFifoIsr;`<br>`PieVectTable.SCIRXINTB = &sciaRxFifoIsr;`<br>`PieVectTable.SCITXINTB = &sciaTxFifoIsr;` |
| `function/z_func.c` | `PieVectTable.TINT0 = cpu_timer0_isr;`<br>`PieVectTable.SCIRXINTA 		= SCI_RXD_isr;`<br>`PieVectTable.SCITXINTA 		= SCI_TXD_isr;` |
| `motor/MotorImParEst.c` | `PieVectTable.EPWM1_INT 	= &EPWM1_zero_isr;`<br>`PieVectTable.EPWM1_INT = &RrLoInPeriodAndCmpInt;`<br>`PieVectTable.EPWM2_INT = &RrLoInPeriodAndCmpInt;` |
| `motor/MotorParChange.c` | `PieVectTable.EQEP1_INT = &PG_Zero_isr;`<br>`PieVectTable.EQEP2_INT = &PG_Zero_isr;` |
| `motor/MotorParEst.c` | `PieVectTable.EPWM1_TZINT = &EPWM1_TZ_isr;`<br>`PieVectTable.EPWM1_INT 	= &EPWM1_zero_isr;` |
| `system/mainInit_2803x.c` | `PieVectTable.TINT0 		= &cpu_timer0_isr;`<br>`PieVectTable.ADCINT1     = &ADC_Over_isr;`<br>`PieVectTable.EPWM1_TZINT = &EPWM1_TZ_isr;`<br>`PieVectTable.EPWM1_INT 	= &EPWM1_zero_isr;`<br>`PieVectTable.SCIRXINTA 	= &SCI_RXD_isr;`<br>`PieVectTable.SCITXINTA 	= &SCI_TXD_isr;` |
