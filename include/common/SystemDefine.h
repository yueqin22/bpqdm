
#ifndef MAIN_DEFINE_28035_H
#define MAIN_DEFINE_28035_H

#ifdef __cplusplus
extern "C" {
#endif
#include "DSP2803x_Device.h"

#define SOFT_VERSION		72

#define      TMS320F28035   3

#define    DSP_CLOCK60			60

#ifdef		 DSP_CLOCK100
	#define    	DSP_CLOCK			100
	#define		PWM_CLK_DIV			0
	#define     PWM_CLOCK           100
	#define     DBTIME_1140V         50
    #define     DCTIME_1140V         14
#endif
#ifdef      DSP_CLOCK80
	#define    	DSP_CLOCK			80
	#define		PWM_CLK_DIV			0
	#define     PWM_CLOCK           80
#endif
#ifdef      DSP_CLOCK60
	#define    	DSP_CLOCK			60
	#define		PWM_CLK_DIV			0
	#define     PWM_CLOCK           60
    #define     DBTIME_1140V         30
    #define     DCTIME_1140V         8
#endif

#define     PWM_CLOCK_HALF      (PWM_CLOCK/2)
#define  	C_TIME_05MS         (DSP_CLOCK*500L)
#define  	C_TIME_20MS         (DSP_CLOCK*2000L)
#define     C_TIME_045MS        (DSP_CLOCK*450L)

#define  READ_RAND_FLASH_WAITE		3
#define  READ_PAGE_FLASH_WAITE		3
#define  READ_OTP_WAITE				8

#define ADC_GND              (AdcResult.ADCRESULT0<<4)
#define	ADC_IU				 (AdcResult.ADCRESULT1<<4)
#define ADC_IW               (AdcResult.ADCRESULT2<<4)
#define	ADC_UDC				 (AdcResult.ADCRESULT3<<4)

#define	ADC_TEMP			 (AdcResult.ADCRESULT4<<4)
#define	ADC_IV               (AdcResult.ADCRESULT5<<4)
#define ADC_AI1              (AdcResult.ADCRESULT6<<4)
#define ADC_VREFLO           (AdcResult.ADCRESULT6<<4)
#define	ADC_AI2				 (AdcResult.ADCRESULT7<<4)

#define ADC_AI3              (AdcResult.ADCRESULT8<<4)
#define	ADC_UU4              (AdcResult.ADCRESULT9<<4)
#define ADC_UU5              (AdcResult.ADCRESULT10<<4)
#define ADC_UU6              (AdcResult.ADCRESULT11<<4)
#define ADC_UU7              (AdcResult.ADCRESULT12<<4)
#define	PL_VOE_PROTECT		 (AdcResult.ADCRESULT13<<4)

#define UVW_PG_U                (AdcResult.ADCRESULT9 <<4)
#define UVW_PG_V                (AdcResult.ADCRESULT10<<4)
#define UVW_PG_W                (AdcResult.ADCRESULT11<<4)

#define PIE_VECTTABLE_ADCINT    PieVectTable.ADCINT1
#define ADC_CLEAR_INT_FLAG      AdcRegs.ADCINTFLGCLR.bit.ADCINT1 = 1
#define ADC_RESET_SEQUENCE      AdcRegs.SOCPRICTL.bit.RRPOINTER = 0x20
#define ADC_START_CONVERSION   	AdcRegs.ADCSOCFRC1.all = 0xFFFF
#define ADC_END_CONVERSIN       AdcRegs.ADCINTFLG.bit.ADCINT1

#define  ADC_VOLTAGE_08         ( 8L*65535/33)
#define  ADC_VOLTAGE_10         (10L*65535/33)
#define  ADC_VOLTAGE_20         (20L*65535/33)
#define  ADC_VOLTAGE_25         (25L*65535/33)

#define PL_INPUT_HIGH           (PL_VOE_PROTECT < ADC_VOLTAGE_08)

#define DisConnectRelay()   	GpioDataRegs.GPASET.bit.GPIO11   = 1
#define ConnectRelay()    	    GpioDataRegs.GPACLEAR.bit.GPIO11 = 1

#define EnableDrive()    	    GpioDataRegs.GPACLEAR.bit.GPIO7  = 1
#define DisableDrive()     	GpioDataRegs.GPASET.bit.GPIO7  = 1
#define GetOverUdcFlag()      GpioDataRegs.AIODAT.bit.AIO2 == 0

#define RT_SAMPLE_START         (GpioDataRegs.AIOCLEAR.bit.AIO10 = 1)
#define RT_SAMPLE_END           (GpioDataRegs.AIOSET.bit.AIO10 = 1)

#define ROTOR_TRANS_RDVEL	    GpioDataRegs.GPADAT.bit.GPIO20
#define ROTOR_TRANS_SO		    GpioDataRegs.GPADAT.bit.GPIO21
#define ROTOR_TRANS_RD	        GpioDataRegs.GPADAT.bit.GPIO23
#define ROTOR_TRANS_SCLK	    GpioDataRegs.AIODAT.bit.AIO12

#ifdef __cplusplus
}
#endif
#endif
