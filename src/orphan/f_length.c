#include "f_length.h"

#define SERVO_ADJUST_MULTIPLY                   8
#define SERVO_ENABLE_AIM_FREQ                   4000

Uint16 servoRunStatus;
SERVO_RUN_PARA_STRUCT servoRunControl;

LOCALF Uint16 runTickerServo;
LOCALF Uint32 qepPulseTemp;
Uint32 qepPulseCalALl;
Uint32 qepPulsePLC;
LOCALF Uint16 servoTicker;

unsigned int intervalTime;
unsigned int rangePer;
unsigned int bobofrq;
unsigned int boboreal;
unsigned int boboerr;
Uint16 servoRunStarFlag;
Uint16 servoControlEnable;
Uint32 pulseNote1;
Uint32 pulseNote2;
Uint32 pulseNote3;
Uint32 pulseTriangleLeft;
Uint16 timeLeft1;
Uint16 timeLeft2;
Uint16 frqSaveDis;
Uint32 motorPulseAll;
Uint16 servoRunResetFlag;
Uint16 servoCalFlag;
Uint16 servoRunDIFlag;
Uint32 servoPulseRunToPLC;
Uint16 servoOverFlag;
Uint16 servoEndTime;

LOCALF void servoParaCal(void);
LOCALF int32 ServoTrackPulseCal(int32 f0,int32 f1,int32 t);

void ServoRunCtrl(void)
{
        Uint32 mAdjustTmp;
        Uint32 i;

        if (!runCmd.bit.common)
        {
                swingStatus = SWING_NONE;
                runStatus = RUN_STATUS_STOP;
                stopRunStatus = STOP_RUN_STATUS_INIT;

                frqTmp = frqRun;
                runFlag.bit.servo = 0;
                return;
        }

        runFlag.bit.run = 1;
        runFlag.bit.common = 1;
        runFlag.bit.servo = 1;
        dspMainCmd.bit.run = 1;

        if(0)
        {
            servoRunResetFlag =1;
            servoControlEnable = 1;

            qepPulsePLC = EQep1Regs.QPOSCNT;
            servoPulseRunToPLC = 0;
            servoOverFlag = 0;
            servoEndTime = 0;

            servoRunControl.motorRealRunPulse = 0;
            qepPulseTemp = EQep1Regs.QPOSCNT;

            servoRunControl.servoLeftPulse = funcCode.code.servoPulseCheck ;

            pulseTriangleLeft = ServoTrackPulseCal(0,frqTmp,servoRunControl.decTime);

            if( servoRunControl.servoLeftPulse <= pulseTriangleLeft)
            {
                servoRunControl.pulseSet = servoRunControl.servoLeftPulse;

                timeLeft1 = (Uint64)servoRunControl.servoLeftPulse * servoRunControl.motorPolePairs * 2 *100000
                           /((Uint32)frqTmp * servoRunControl.encoderPulse);

                timeLeft2 = (Uint32) maxFrq * timeLeft1 / frqTmp;
                servoRunControl.decTime = timeLeft2 / 10;
                servoRunControl.constTime =0;
                servoRunStatus = DEC_RUN_STATUS;
            }
            else
            {
                    servoParaCal();
                    servoRunStatus = ACC_RUN_STATUS;
            }
        }

        if((servoRunControl.aimFreq > SERVO_ENABLE_AIM_FREQ) && (!servoRunResetFlag))
        {
                if((ABS_INT16(frqTmp) <= SERVO_ENABLE_AIM_FREQ) && (DEC_RUN_STATUS == servoRunStatus))
                {
                        servoControlEnable = 1;

                        if(!servoCalFlag)
                        {
                                frqSaveDis = frqTmp;
                                servoRunStarFlag = 1;
                                servoCalFlag = 1;
                                servoOverFlag = 0;
                                servoEndTime = 0;

                                servoRunControl.motorRealRunPulse += (int32)(EQep1Regs.QPOSCNT - qepPulseTemp);
                                pulseNote2 = (servoRunControl.motorRealRunPulse >> 2);
                                qepPulseTemp = EQep1Regs.QPOSCNT;
                                servoRunControl.servoLeftPulse = servoRunControl.pulseSet - (servoRunControl.motorRealRunPulse >> 2);
                                servoRunControl.motorRealRunPulse = 0;

                                pulseTriangleLeft = ServoTrackPulseCal(0,frqTmp,servoRunControl.decTime);

                                pulseNote1 = servoRunControl.calRunPulse;

                                if( servoRunControl.servoLeftPulse >= pulseTriangleLeft)
                                {
                                        servoRunControl.constTime = (int64)servoRunControl.motorPolePairs* (servoRunControl.servoLeftPulse - pulseTriangleLeft)
                                                                  * FRQ_UINT * (TIME_UNIT_MS_PER_SEC/RUN_CTRL_PERIOD)
                                                                  / ((int64)frqTmp  * servoRunControl.encoderPulse);
                                        servoRunStatus = CONST_RUN_STATUS;
                                        servoRunControl.constTicker = 1;
                                        servoRunControl.accCoursePulse =0;
                                }

                                else
                                {
                                        timeLeft1 = (Uint64)servoRunControl.servoLeftPulse * servoRunControl.motorPolePairs * 2 *100000
                                                   /((Uint32)frqTmp * servoRunControl.encoderPulse);

                                        timeLeft2 = (Uint32) maxFrq * timeLeft1 / frqTmp;
                                        servoRunControl.decTime = timeLeft2 / 10;
                                        servoRunControl.constTime =0;
                                }
                        }
                }
                else
                {
                        mAdjustTmp = 0;

                }
        }

        switch (servoRunStatus)
        {
                case ZERO_PREPARE_PARA_CAL:
                {
                        servoParaCal();
                        servoControlEnable = 0;
                        if(servoRunControl.aimFreq <= SERVO_ENABLE_AIM_FREQ)
                        {
                            servoControlEnable = 1;
                        }
                        servoRunStatus = ACC_RUN_STATUS;
                        servoRunStarFlag = 0;
                        motorPulseAll = 0;
                        servoPulseRunToPLC = 0;
                        qepPulsePLC = EQep1Regs.QPOSCNT;
                        servoOverFlag = 0;
                        servoEndTime = 0;
                        break;
                }
                case ACC_RUN_STATUS:
                {
                        frqCurAim = servoRunControl.aimFreq;
                        AccDecFrqCalc(servoRunControl.accTime, servoRunControl.decTime, 0);

                        if(servoControlEnable)
                        {
                            servoRunControl.accCoursePulse = ServoTrackPulseCal(servoRunControl.runStarFreq,frqTmp,servoRunControl.accTime);
                            servoRunControl.calRunPulse = servoRunControl.accCoursePulse;
                        }

                        if (frqCurAim == frqTmp)
                        {
                                runTickerServo = 0;
                                servoRunStatus = CONST_RUN_STATUS;
                                if(servoRunControl.constTime)
                                {
                                        servoRunControl.constTicker++;
                                }
                                else
                                {
                                    servoRunStatus = DEC_RUN_STATUS;
                                }
                        }
                        break;
                }
                case CONST_RUN_STATUS:
                {

                        if(servoCalFlag)
                        {
                            servoRunControl.aimFreq = frqTmp;
                        }
                        if(servoControlEnable)
                        {
                            servoRunControl.constCoursePulse = ((int64)servoRunControl.aimFreq * servoRunControl.encoderPulse * servoRunControl.constTicker * 2)
                                                                / ((int32)servoRunControl.motorPolePairs * TIME_UNIT_MS_PER_SEC * FRQ_UINT);
                            servoRunControl.calRunPulse = servoRunControl.accCoursePulse + servoRunControl.constCoursePulse;
                        }
                        if (servoRunControl.constTicker++ >= (servoRunControl.constTime-1) )
                        {
                                servoRunStatus = DEC_RUN_STATUS;
                        }
                        break;
                }
                case DEC_RUN_STATUS:
                {
                        frqCurAim = 0;
                        AccDecFrqCalc(servoRunControl.accTime, servoRunControl.decTime, 0);

                        if(!servoControlEnable)
                        {
                            break;
                        }
                        servoRunControl.decCoursePulse = ServoTrackPulseCal(0,frqTmp,servoRunControl.decTime);

                        if(servoRunStarFlag)
                        {
                                servoRunControl.calRunPulse = servoRunControl.servoLeftPulse - servoRunControl.decCoursePulse;

                        }
                        else
                        {
                                servoRunControl.calRunPulse = servoRunControl.pulseSet - servoRunControl.decCoursePulse;
                        }
                        break;
                }
                default:
                {
                        break;
                }
        }

        servoPulseRunToPLC += (int32)(EQep1Regs.QPOSCNT - qepPulsePLC);
        qepPulsePLC = EQep1Regs.QPOSCNT;

        if(!servoControlEnable)
        {
            servoRunControl.servoDspFreq = frqTmp;
            return;
        }

        mAdjustTmp = (Uint32)funcCode.code.servoAdjustRange * maxFrq / 1000;

        motorPulseAll += (int32)(EQep1Regs.QPOSCNT - qepPulseCalALl);
        qepPulseCalALl = EQep1Regs.QPOSCNT;

        servoRunControl.motorRealRunPulse += (int32)(EQep1Regs.QPOSCNT - qepPulseTemp);
        qepPulseTemp = EQep1Regs.QPOSCNT;

        servoRunControl.errorPulse =(int32)(servoRunControl.calRunPulse - (servoRunControl.motorRealRunPulse >> 2));
        boboerr = servoRunControl.errorPulse;

        if (ABS_INT16(servoRunControl.errorPulse) < funcCode.code.servoSwitchPulseErr)
        {
                Uint16 upper;

                vcSpdLoopKp1 = funcCode.code.servoVcSpdLoopKp1 << 1;
                upper = funcCodeAttribute[GetCodeIndex(funcCode.code.servoVcSpdLoopKp1)].upper;
                if (vcSpdLoopKp1 > upper)
                {
                        vcSpdLoopKp1 = upper;
                }
                vcSpdLoopKp2 = funcCode.code.servoVcSpdLoopKp2 << 1;
                upper = funcCodeAttribute[GetCodeIndex(funcCode.code.servoVcSpdLoopKp2)].upper;
                if (vcSpdLoopKp2 > upper)
                {
                        vcSpdLoopKp2 = upper;
                }
        }
        servoRunControl.servoKp  = funcCode.code.servoLoopKp1;

        if(ABS_INT16(servoRunControl.errorPulse) >= funcCode.code.servoSwitchPulseErr)
        {
                servoRunControl.servoKp = funcCode.code.servoLoopKp2;
        }

        servoRunControl.adjustFreq = (((int32)servoRunControl.errorPulse *  servoRunControl.servoKp) >> SERVO_ADJUST_MULTIPLY);

        if(ABS_INT16(servoRunControl.adjustFreq) > mAdjustTmp)
        {
                if(servoRunControl.adjustFreq < 0)
                {
                        servoRunControl.adjustFreq = - mAdjustTmp;
                }
                else
                {
                        servoRunControl.adjustFreq =  mAdjustTmp;
                }
        }

        servoRunControl.servoDspFreq = frqTmp + servoRunControl.adjustFreq;
}

LOCALF void servoParaCal(void)
{
        Uint32 mFrqCal;
        Uint32 pulseTriangle;
        Uint32 pulseRectangle;

        qepPulseTemp = EQep1Regs.QPOSCNT;
        qepPulseCalALl = EQep1Regs.QPOSCNT;
        servoRunControl.motorRealRunPulse =0;
        servoRunControl.accCoursePulse = 0;
        servoRunControl.constCoursePulse = 0;
        servoRunControl.decCoursePulse = 0;
        servoTicker = 0;
        servoRunControl.constTicker = 0;
        servoRunControl.calRunPulse = 0;
        servoCalFlag =0;

        servoRunControl.runStarFreq = frqTmp;
        servoRunControl.pulseSet = (Uint32)funcCode.code.servoPulseHigh *65535 + funcCode.code.servoPulseLower;
        servoRunControl.accTime = funcCode.code.servoCtrlAccTime;
        servoRunControl.decTime = funcCode.code.servoCtrlDecTime;
        servoRunControl.encoderPulse = funcCode.code.servoEncoderPulse;

        servoRunControl.motorPolePairs = 60UL * funcCode.code.servoRatingFrq / ((int32)FRQ_UINT * funcCode.code.servoRatingSpeed);

        servoRunControl.pulseSet = (Uint64)servoRunControl.pulseSet * funcCode.code.servoElectroGearB / funcCode.code.servoElectroGearA;

        if(servoRunResetFlag)
        {
                servoRunControl.pulseSet = servoRunControl.servoLeftPulse;
        }

        mFrqCal = ((Uint64)servoRunControl.motorPolePairs * 2 * maxFrq * servoRunControl.pulseSet * FRQ_UINT
                * (TIME_UNIT_MS_PER_SEC / SERVO_ADD_DEC_TIME_UINT) + (Uint64)servoRunControl.encoderPulse
                * servoRunControl.accTime * frqTmp * frqTmp)
                / ((Uint32)servoRunControl.encoderPulse  * (servoRunControl.accTime + servoRunControl.decTime));

        servoRunControl.aimFreq = qsqrt(mFrqCal);

        if (servoRunControl.aimFreq > maxFrq)
        {
                servoRunControl.aimFreq = maxFrq;

                pulseTriangle = ServoTrackPulseCal(frqTmp,servoRunControl.aimFreq, servoRunControl.accTime)
                              + ServoTrackPulseCal( 0,servoRunControl.aimFreq, servoRunControl.decTime);

                servoRunControl.constTime = (int64)servoRunControl.motorPolePairs* (servoRunControl.pulseSet - pulseTriangle)
                                          * FRQ_UINT *  (TIME_UNIT_MS_PER_SEC/RUN_CTRL_PERIOD)
                                          / ((int64)servoRunControl.aimFreq  * servoRunControl.encoderPulse);
        }
        else
        {
                servoRunControl.constTime = 0;
        }
}

LOCALF int32 ServoTrackPulseCal(int32 f0,int32 f1,int32 t)
{
        int32 mPulse;

        mPulse = ((int64)servoRunControl.encoderPulse * ((int64) f1 * f1 - f0 * f0) * t)
                 /((int32)servoRunControl.motorPolePairs * 2* maxFrq * FRQ_UINT * (TIME_UNIT_MS_PER_SEC / SERVO_ADD_DEC_TIME_UINT));

        return mPulse;
}
