
#include "f_funcCode.h"
#include "f_ui.h"
#include "f_io.h"
#include "f_eeprom.h"
#include "f_comm.h"
#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_main.h"

#define RAM_START_ADDRESS 0x008000
#if F_DEBUG_RAM
#define RAM_SIZE          0x0600

#elif 1
#define RAM_SIZE          0x1F80
#endif

typedef struct
{
    Uint16 ms;
    Uint16 s;
    Uint16 min;
    Uint16 hour;
}TIME;

#if F_DEBUG_RAM

#endif

#if !F_DEBUG_RAM
int16 gSendToMotor05MsDataBuff[FUNC_TO_MOTOR_05MS_DATA_NUM];
int16 gSendToMotor2MsDataBuff[FUNC_TO_MOTOR_2MS_DATA_NUM + FUNC_TO_CORE_DEBUG_DATA_NUM];
#endif
int16 gSendToFunctionDataBuff[MOTOR_TO_Func_2MS_DATA_NUM + CORE_TO_FUNC_DISP_DATA_NUM];
int16 gParaIdToFunctionDataBuff[TUNE_DATA_NUM];

int16 gRealTimeToFunctionDataBuff[1];

LOCALF TIME time;
LOCALF Uint16 timerTicker;
LOCALF Uint16 bTimerHalfMs;
LOCALF Uint16 timerTickerHalfMsOld;
LOCALF Uint16 bTimer1ms;
LOCALF Uint16 timerTicker1msOld;
LOCALF Uint16 bTimer2ms;
LOCALF Uint16 timerTicker2msOld;

extern void InitECap1Gpio(void);
extern void InitECap2Gpio(void);
extern void InitSciaGpio(void);
extern void InitSetAdc(void);
extern interrupt void SCI_RXD_isr(void);
extern interrupt void SCI_TXD_isr(void);
LOCALD void TimerDeal(void);
void ramInit(void);
LOCALD void dspInit(void);
LOCALD void functionPeripheralInit(void);
#define EEPROM_INDEX_USE_LENGTH     994
extern Uint16 sizeOfTable_eeprom2Fc;
void main(void)
{
    Uint32 baseTime;
    static Uint16 ticker;
    dspInit();
	sizeOfTable_eeprom2Fc = EEPROM_INDEX_USE_LENGTH;
    InitForFunctionApp();

    baseTime = GetTime();

    for (;;)
    {
        Main0msFunction();

        if(baseTime - GetTime() >= TIME_100US)
        {
            static Uint16 k;

            baseTime -= TIME_100US;

            TimerDeal();

            if (++k >= 1)
            {
                k = 0;

            }
        }

        if (bTimerHalfMs)
        {
            bTimerHalfMs = 0;
             if (0 == ticker)
            {
                Main05msFunctionA();
            }
            else if (1 == ticker)
            {
                Main05msFunctionB();
            }
            else if (2 == ticker)
            {
                Main05msFunctionC();
            }
            else if (3 == ticker)
            {
                Main05msFunctionD();
            }
            ticker++;
            if (ticker >= 4)
            {
                ticker = 0;
            }

        }

        if (bTimer1ms)
        {
            bTimer1ms = 0;
        }

        if (bTimer2ms)
        {
            bTimer2ms = 0;
            Main2msFunction();
        }
    }
}

LOCALF void TimerDeal()
{
    timerTicker++;

    if ((timerTicker - timerTicker1msOld) >= 1 * 10)
    {
        timerTicker1msOld = timerTicker;
        bTimer1ms = 1;

        if (++time.ms >= 1000)
        {
            time.ms = 0;
            if (++time.s >= 60)
            {
                time.s = 0;
                if (++time.min >= 60)
                {
                    time.min = 0;
                    time.hour++;
                }
            }
        }
    }

    if ((timerTicker - timerTicker2msOld) >= 2 * 10)
    {
        timerTicker2msOld = timerTicker;
        bTimer2ms = 1;
    }

    if ((timerTicker - timerTickerHalfMsOld) >= 5)
    {
        timerTickerHalfMsOld = timerTicker;
        bTimerHalfMs = 1;
    }

#if F_DEBUG_RAM
    AdcRegs.ADCTRL2.bit.RST_SEQ1 = 1;
    AdcRegs.ADCTRL2.bit.SOC_SEQ1 = 1;
#endif
}

LOCALF void dspInit()
{

    InitSysCtrl();

#if 0
    InitSpiaGpio();
    InitI2CGpio();
    InitSciaGpio();
#endif

    DINT;

    InitPieCtrl();

    IER = 0x0000;
    IFR = 0x0000;

    PieCtrlRegs.PIECTRL.bit.ENPIE = 1;

#if 0
    EALLOW;
    PieVectTable.TINT0 = cpu_timer0_isr;
    EDIS;
#endif

    functionPeripheralInit();

    InitCpuTimers();

#if 0
#if (DSP_CLOCK == 100)
    ConfigCpuTimer(&CpuTimer0, 100.0, 100.0);
#elif (DSP_CLOCK == 60)
    ConfigCpuTimer(&CpuTimer0, 60.0, 6.0);

#endif

#endif

    StartCpuTimer1();

   MemCopy(&RamfuncsLoadStart, &RamfuncsLoadEnd, &RamfuncsRunStart);

   InitFlash();

#if 1
    EALLOW;
    PieVectTable.SCIRXINTA 		= SCI_RXD_isr;
	PieVectTable.SCITXINTA 		= SCI_TXD_isr;
    EDIS;
	IER |= M_INT9;
	PieCtrlRegs.PIEIER9.bit.INTx1 = 1;
	PieCtrlRegs.PIEIER9.bit.INTx2 = 1;
#endif

    EINT;
    ERTM;
}

#if 0
void ramInit()
{
    Uint16 *p = (Uint16 *)RAM_START_ADDRESS;

    for (; p < (Uint16 *)(RAM_START_ADDRESS + RAM_SIZE); p++)
        *p = 0;
}
#else
void ramInit()
{
    int16 i;
    Uint16 *p = (Uint16 *)RAM_START_ADDRESS;

    for (i = 0; i < RAM_SIZE; i++)
        *(p++) = 0;
}
#endif

void MemCopy(Uint16 *SourceAddr, Uint16* SourceEndAddr, Uint16* DestAddr)
{
    while(SourceAddr < SourceEndAddr)
    {
       *DestAddr++ = *SourceAddr++;
    }
    return;
}

LOCALF void functionPeripheralInit()
{
    InitSetAdc();
}

#if 0
interrupt void cpu_timer0_isr(void)
{

    CPUPerformanceBase++;

    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}
#endif

#if 1
void InitSetAdc(void)
{
#if DEBUG_F_AI
    int16 waite;

    AdcRegs.ADCTRL3.all = 0x00E0;

    for (waite = 0;waite < 5000; waite++)
    {
        ;
    }

    AdcRegs.ADCTRL3.bit.ADCCLKPS = 1;
    AdcRegs.ADCTRL1.bit.CPS = 1;

    AdcRegs.ADCTRL1.bit.ACQ_PS = 1;
    AdcRegs.ADCTRL1.bit.CONT_RUN = 0;
    AdcRegs.ADCTRL1.bit.SEQ_CASC = 1;
    AdcRegs.ADCTRL3.bit.SMODE_SEL = 1;
    AdcRegs.ADCTRL2.bit.EPWM_SOCA_SEQ1 = 1;
    AdcRegs.ADCMAXCONV.all = 0x3;

    AdcRegs.ADCCHSELSEQ1.bit.CONV00 = 0;
    AdcRegs.ADCCHSELSEQ1.bit.CONV01 = 1;
    AdcRegs.ADCCHSELSEQ1.bit.CONV02 = 2;
    AdcRegs.ADCCHSELSEQ1.bit.CONV03 = 3;

    AdcRegs.ADCCHSELSEQ2.bit.CONV04 = 4;
    AdcRegs.ADCCHSELSEQ2.bit.CONV05 = 5;
    AdcRegs.ADCCHSELSEQ2.bit.CONV06 = 6;
    AdcRegs.ADCCHSELSEQ2.bit.CONV07 = 7;
#endif
}
#endif

#if 1

Uint16 qsqrt(Uint32 data)
{
	Uint32 b = 0;
	Uint32 tmp;

    if (data)
    {
        tmp = data / 2;
		for (;;)
		{
			b = data / tmp;
			if (ABS_INT32((int32)(tmp - b)) > 1)
                tmp = (tmp + b) / 2;
			else
                break;
		}
    }

    return b;
}
#endif

#if 0
Uint32 sqrt64(const Uint64 x)
{
    int16 i;
    Uint64 rc = 0;
    Uint64 t;

    for (i = 31; i >= 0; i--)
    {
        t = rc + (1UL << i);
        if ((Uint64)t * t <= x)
            rc = t;
    }

    return rc;
}
#endif
