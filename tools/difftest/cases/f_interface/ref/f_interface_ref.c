
#include "f_funcCode.h"
#include "f_runSrc.h"
#include "f_main.h"
#include "f_frqSrc.h"
#include "f_io.h"
#include "f_menu.h"
#include "f_posCtrl.h"

#if F_DEBUG_RAM
#define DEBUG_F_INTERFACE                   0
#define DEBUG_F_POWER_ADD                   0
#elif 1
#define DEBUG_F_INTERFACE                   1
#define DEBUG_F_POWER_ADD                   1
#endif

#define DROOP_VOLTAGE_LEVEL 6500

Uint16 frq2Core;
Uint16 frqCurAim2Core;
Uint16 rsvdData;
Uint16 driveCoeffFrqFdb;
Uint16 vcSpdLoopKp1;
Uint16 vcSpdLoopKp2;
Uint16 jerkFf;

extern Uint16 vfSeparateVol;

Uint16 speedMotor;
Uint16 motorRun;

int32 frqRun;
int32 frqVFRun;
int32 frqVFRunRemainder;
Uint16 outVoltage;
Uint16 outCurrent;
Uint16 generatrixVoltage;
int16 outPower;
Uint16 errorCodeFromMotor;

Uint16 errorsCodeFromMotor[2];
Uint16 motorErrInfor[5];

Uint16 outCurrentDispOld;

int16 gPhiRt;
int16 gPhiRtDisp;
Uint16 pmsmRotorPos;
Uint16 frqFdb;
Uint16 frqFdbDisp;
int32  frqFdbTmp;
Uint16 frqFdbFlag;
Uint16 torqueCurrent;
Uint16 enCoderPosition;
Uint16 ABZPos;
Uint16 ZCount;
Uint16 antiVibrateCoeff;

#if DEBUG_F_INTERFACE
void copyDataFunc2Motor2ms(void);

Uint16 currentOc;
Uint16 currentPu;
Uint16 spdLoopOut;

LOCALF Uint32 softOCTicker;
extern Uint16 softOcDoFlag;

void copyDataFunc2Motor05ms(void)
{
    gSendToMotor05MsDataBuff[0] = dspMainCmd.all;
    gSendToMotor05MsDataBuff[1] = dspMainCmd1.all;
    gSendToMotor05MsDataBuff[2] = tuneCmd;
    gSendToMotor05MsDataBuff[3] = motorFc.motorPara.elem.motorType;
    gSendToMotor05MsDataBuff[4] = frq2Core;
    gSendToMotor05MsDataBuff[5] = vfSeparateVol;
    gSendToMotor05MsDataBuff[6] = motorFc.motorPara.elem.ratingVoltage;
    gSendToMotor05MsDataBuff[7] = motorFc.motorPara.elem.ratingCurrent;
    gSendToMotor05MsDataBuff[8] = motorFc.motorPara.elem.ratingFrq;
    gSendToMotor05MsDataBuff[9] = funcCode.code.vfCurve;
    gSendToMotor05MsDataBuff[10] = funcCode.code.ovGain;
    gSendToMotor05MsDataBuff[11] = funcCode.code.ovPoint;
    gSendToMotor05MsDataBuff[12] = funcCode.code.ocGain;
    gSendToMotor05MsDataBuff[13] = funcCode.code.ocPoint;
}

void copyDataFunc2Motor2ms(void)
{
#if  DEBUG_F_INTERFACE
    gSendToMotor2MsDataBuff[0] = dspSubCmd.all;
    gSendToMotor2MsDataBuff[1] = frqCurAim2Core;
    gSendToMotor2MsDataBuff[2] = funcCode.code.maxFrq;
    gSendToMotor2MsDataBuff[3] = funcCode.code.carrierFrq;
    gSendToMotor2MsDataBuff[4] = motorFc.motorPara.elem.ratingPower;

#if 1
    memcpy(&gSendToMotor2MsDataBuff[5], &motorFc.motorPara.elem.ratingSpeed, 6);
#else
    gSendToMotor2MsDataBuff[5] = motorFc.motorPara.elem.ratingSpeed;
    gSendToMotor2MsDataBuff[6] = motorFc.motorPara.elem.statorResistance;
    gSendToMotor2MsDataBuff[7] = motorFc.motorPara.elem.rotorResistance;
    gSendToMotor2MsDataBuff[8] = motorFc.motorPara.elem.leakInductance;
    gSendToMotor2MsDataBuff[9] = motorFc.motorPara.elem.mutualInductance;
    gSendToMotor2MsDataBuff[10] = motorFc.motorPara.elem.zeroLoadCurrent ;
#endif

#if 1
    memcpy(&gSendToMotor2MsDataBuff[11], &motorFc.motorPara.elem.pmsmRs, 3);
#else
    gSendToMotor2MsDataBuff[11] = motorFc.motorPara.elem.pmsmRs;
    gSendToMotor2MsDataBuff[12] = motorFc.motorPara.elem.pmsmLd;
    gSendToMotor2MsDataBuff[13] = motorFc.motorPara.elem.pmsmLq;
#endif

    gSendToMotor2MsDataBuff[14] = motorFc.vcPara.vcSpdLoopKp1;
    gSendToMotor2MsDataBuff[15] = motorFc.vcPara.vcSpdLoopTi1;
    gSendToMotor2MsDataBuff[16] = motorFc.vcPara.vcSpdLoopKp2;
    gSendToMotor2MsDataBuff[17] = motorFc.vcPara.vcSpdLoopTi2;
    gSendToMotor2MsDataBuff[18] = motorFc.vcPara.vcSpdLoopChgFrq1;
    gSendToMotor2MsDataBuff[19] = motorFc.vcPara.vcSpdLoopChgFrq2;
    gSendToMotor2MsDataBuff[20] = motorFc.vcPara.vcSlipCompCoef;
    gSendToMotor2MsDataBuff[21] = motorFc.vcPara.vcSpdLoopFilterTime;
    gSendToMotor2MsDataBuff[22] = motorFc.pgPara.elem.encoderPulse;
    gSendToMotor2MsDataBuff[23] = motorFc.vcPara.vcOverMagGain;
    gSendToMotor2MsDataBuff[24] = funcCode.code.loseLoadLevel;
    gSendToMotor2MsDataBuff[25] = funcCode.code.loseLoadTime;
    gSendToMotor2MsDataBuff[26] = upperTorque;
    gSendToMotor2MsDataBuff[27] = motorFc.torqueBoost;

#if 1
    memcpy(&gSendToMotor2MsDataBuff[28], &funcCode.code.boostCloseFrq, 9);
#else
    gSendToMotor2MsDataBuff[28] = funcCode.code.boostCloseFrq;
    gSendToMotor2MsDataBuff[29] = funcCode.code.vfFrq1;
    gSendToMotor2MsDataBuff[30] = funcCode.code.vfVol1;
    gSendToMotor2MsDataBuff[31] = funcCode.code.vfFrq2;
    gSendToMotor2MsDataBuff[32] = funcCode.code.vfVol2;
    gSendToMotor2MsDataBuff[33] = funcCode.code.vfFrq3;
    gSendToMotor2MsDataBuff[34] = funcCode.code.vfVol3;
    gSendToMotor2MsDataBuff[35] = funcCode.code.slipCompCoef;
    gSendToMotor2MsDataBuff[36] = funcCode.code.vfOverMagGain;
#endif

    gSendToMotor2MsDataBuff[37] = motorFc.antiVibrateGain;
    gSendToMotor2MsDataBuff[38] = funcCode.code.startBrakeCurrent;
    gSendToMotor2MsDataBuff[39] = funcCode.code.stopBrakeCurrent;
    gSendToMotor2MsDataBuff[40] = funcCode.code.brakeDutyRatio;
    gSendToMotor2MsDataBuff[41] = funcCode.code.overloadGain;
    gSendToMotor2MsDataBuff[42] = funcCode.code.foreOverloadCoef;
    gSendToMotor2MsDataBuff[43] = funcCode.code.softPwm;
    gSendToMotor2MsDataBuff[44] = funcCode.code.curSampleDelayComp;
    gSendToMotor2MsDataBuff[45] = funcCode.code.uvPoint;
    gSendToMotor2MsDataBuff[46] = motorFc.pgPara.elem.pgType;
    gSendToMotor2MsDataBuff[47] = driveCoeffFrqFdb;
    gSendToMotor2MsDataBuff[48] = funcCode.code.inverterType;
    gSendToMotor2MsDataBuff[49] = funcCode.code.inverterGpType;
    gSendToMotor2MsDataBuff[50] = funcCode.code.tempCurve;
    gSendToMotor2MsDataBuff[51] = funcCode.code.volJudgeCoeff;
    gSendToMotor2MsDataBuff[52] = funcCode.code.curJudgeCoeff;
    gSendToMotor2MsDataBuff[53] = funcCode.code.uvGainWarp;
    gSendToMotor2MsDataBuff[54] = funcCode.code.svcMode;
    gSendToMotor2MsDataBuff[55] = funcCode.code.deadTimeSet;
    gSendToMotor2MsDataBuff[56] = funcCode.code.speedTrackMode;
    gSendToMotor2MsDataBuff[57] = funcCode.code.speedTrackVelocity;
    gSendToMotor2MsDataBuff[58] = funcCode.code.pmsmRotorPos;
    gSendToMotor2MsDataBuff[59] = motorFc.pgPara.elem.enCoderPole;
    gSendToMotor2MsDataBuff[60] = motorFc.motorPara.elem.pmsmCoeff;

#if 1
    memcpy(&gSendToMotor2MsDataBuff[61], &motorFc.vcPara.mAcrKp, 4);
#else
    gSendToMotor2MsDataBuff[61] = motorFc.vcPara.mAcrKp;
    gSendToMotor2MsDataBuff[62] = motorFc.vcPara.mAcrKi;
    gSendToMotor2MsDataBuff[63] = motorFc.vcPara.tAcrKp;
    gSendToMotor2MsDataBuff[64] = motorFc.vcPara.tAcrKi;
#endif

    gSendToMotor2MsDataBuff[65] = motorFc.pgPara.elem.enCoderDir;
    gSendToMotor2MsDataBuff[66] = motorFc.pgPara.elem.enCoderAngle;
    gSendToMotor2MsDataBuff[67] = funcCode.code.pwmMode;
    gSendToMotor2MsDataBuff[68] = motorFc.pgPara.elem.uvwSignDir;
    gSendToMotor2MsDataBuff[69] = motorFc.pgPara.elem.uvwSignAngle;

#if 1
    memcpy(&gSendToMotor2MsDataBuff[70], &motorFc.vcPara.weakFlusMode, 5);
#else
    gSendToMotor2MsDataBuff[71] = motorFc.vcPara.weakFlusMode;
    gSendToMotor2MsDataBuff[72] = motorFc.vcPara.weakFlusCoef;
    gSendToMotor2MsDataBuff[73] = motorFc.vcPara.weakFlusCurMax;
    gSendToMotor2MsDataBuff[74] = motorFc.vcPara.weakFlusAutoCoef;
    gSendToMotor2MsDataBuff[75] = motorFc.vcPara.weakFlusIntegrMul;
#endif

#endif
}

void copyDataFunc2CF2ms(void)
{
#if 1
    memcpy(&gSendToMotor2MsDataBuff[FUNC_TO_MOTOR_2MS_DATA_NUM], &funcCode.group.cf[0], 40);
#elif 1
    gSendToMotor2MsDataBuff[98] = funcCode.group.cf[0];
    ...
    gSendToMotor2MsDataBuff[137] = funcCode.group.cf[39];

#endif
}

extern Uint16 errorInfoFromMotor;
extern Uint16 motorCtrlTuneStatus;

void copyDataCore2Func2ms(void)
{
    dspStatus.all = gSendToFunctionDataBuff[0];
    motorCtrlTuneStatus = gSendToFunctionDataBuff[1];
    errorsCodeFromMotor[0] = gSendToFunctionDataBuff[2];
    errorsCodeFromMotor[1] = gSendToFunctionDataBuff[3];
    motorErrInfor[0] = gSendToFunctionDataBuff[4];
    motorErrInfor[1] = gSendToFunctionDataBuff[5];
    motorErrInfor[2] = gSendToFunctionDataBuff[6];
    motorErrInfor[3] = gSendToFunctionDataBuff[7];
    motorErrInfor[4] = gSendToFunctionDataBuff[8];
    currentOc = gSendToFunctionDataBuff[9];
    speedMotor = gSendToFunctionDataBuff[10];
    outVoltage = gSendToFunctionDataBuff[11];
    funcCode.code.radiatorTemp = gSendToFunctionDataBuff[12];
    currentPu = gSendToFunctionDataBuff[13];
    funcCode.code.motorSoftVersion = gSendToFunctionDataBuff[14];
    aiDeal[0].sample = gSendToFunctionDataBuff[15];
    aiDeal[1].sample = gSendToFunctionDataBuff[16];
    aiDeal[2].sample = gSendToFunctionDataBuff[17];
    generatrixVoltage = gSendToFunctionDataBuff[18];
    torqueCurrent = gSendToFunctionDataBuff[19];
    outCurrent = gSendToFunctionDataBuff[20];
    pmsmRotorPos = gSendToFunctionDataBuff[21];

    motorRun = gSendToFunctionDataBuff[23];
    outPower = gSendToFunctionDataBuff[24];
    itDisp   = gSendToFunctionDataBuff[25];
    enCoderPosition = gSendToFunctionDataBuff[26];
    gPhiRt = gSendToFunctionDataBuff[27];
    ZCount = gSendToFunctionDataBuff[28];
    antiVibrateCoeff = gSendToFunctionDataBuff[29];
}

void copyDataCore2Uf2ms(void)
{
#if 1
    memcpy(&funcCode.group.uf[0], &gSendToFunctionDataBuff[MOTOR_TO_Func_2MS_DATA_NUM], 30);
#elif 1
    funcCode.group.uf[0]  = gSendToFunctionDataBuff[26];
...
    funcCode.group.uf[29] = gSendToFunctionDataBuff[55];

#endif
}

void copyDataCore2Func05ms(void)
{
}

#elif 1

Uint16 currentPu;

#endif

void UpdateDataCore2Func(void);
void UpdateDataFunc2Core0p5ms(void);
void UpdateDataFunc2Core2ms(void);

#define CHECK_SPEED_DIR_NO     0
#define CHECK_SPEED_DIR_START  1
#define CHECK_SPEED_DIR_END    2
#define CHECK_SPEED_DIR_QUITE  3

void checkSpeedFdbDir(int32 frqFdb)
{

    static Uint16 checkFlag = CHECK_SPEED_DIR_NO;
    static Uint16 speedDir;
    static Uint16 speedDirBak = 0;
    static Uint16 speedFdbDirTcnt = 0;

    if (checkFlag == CHECK_SPEED_DIR_QUITE)
    {
        return;
    }

    if ((!runFlag.bit.run)
        && (!frqFdb)
        && (checkFlag == CHECK_SPEED_DIR_NO)
        )
    {
        checkFlag = CHECK_SPEED_DIR_START;
    }

    if (runFlag.bit.run
        &&(checkFlag == CHECK_SPEED_DIR_START)
        )
    {
        speedDirBak = speedDir;

        if ((int64)frqTmp*((int32)frqFdb) > 0)
        {
            speedDir = 0;
        }

        else if ((int64)frqTmp*((int32)frqFdb) < 0)
        {
            speedDir = 1;
        }

        if (speedDir == speedDirBak)
        {
            speedFdbDirTcnt++;
        }
        else
        {
            speedFdbDirTcnt = 0;
        }

        if (speedFdbDirTcnt > 2500)
        {
            checkFlag = CHECK_SPEED_DIR_END;
        }
    }

    if ((!runFlag.bit.run)
        &&(checkFlag == CHECK_SPEED_DIR_END)
        )
    {
        funcCode.code.speedFdbDir = speedDir;
        checkFlag = CHECK_SPEED_DIR_QUITE;
    }
}

Uint16 powerAddDec;

#define POWER_ADD_COUNT_DEC    5000
#define POWER_ADD_COUNT_INT    3600
void UpdateDataCore2Func(void)
{
    int32 tmp;
    int32 frqFdbDispTmp;
    static int32 outCurrentDispOldTmp;
    static LowPassFilter frqFdbLpf = LPF_DEFALUTS;

#if DEBUG_F_INTERFACE
    copyDataCore2Func2ms();
    copyDataCore2Func05ms();

#endif

#if F_DEBUG_RAM
    if (ERROR_DEALING)
    {
        errorCodeFromMotor = ERROR_NONE;
    }

    if (dspMainCmd.bit.run && (!errorCodeFromMotor))
    {
        dspStatus.bit.run = 1;
    }
    else
    {
        dspStatus.bit.run = 1;
    }

    speedMotor = frq2Core;

    {
        static int16 k;

        if (++k >= (Uint16)(200/RUN_CTRL_PERIOD))
        {
            dspStatus.bit.runEnable = 1;
        }
    }

    currentPu = funcCode.code.motorParaM1.elem.ratingCurrent;

    speedMotor = frq2Core;

    generatrixVoltage = 12345;
    outCurrent = 2048;
    outVoltage = 1<<12;

#endif

#if DEBUG_F_MOTOR_FUNCCODE
#if !F_DEBUG_RAM

    copyDataCore2Uf2ms();
#endif
#endif

    frqRun = ((int32)(int16)speedMotor * (int32)frqPuQ15 + (1 << 14)) >> 15;

    if (motorFc.motorCtrlMode == FUNCCODE_motorCtrlMode_VF)
    {

    	frqVFRun = ((int32)(int16)speedMotor * (int32)frqPuQ15) >> 15;

    	if (frqVFRun < frqRun)
    	{
    		frqVFRunRemainder = ((int32)(int16)speedMotor * (int32)frqPuQ15) - (frqRun<<15);
    	}
    	else
    	{

    	    frqVFRunRemainder = ((int32)(int16)speedMotor * (int32)frqPuQ15) - (frqVFRun<<15);
    	}
    }
    else

    {
        frqVFRunRemainder = 0;
    }

    frqFdbTmp = ((int32)(int16)motorRun * (int32)frqPuQ15 + (1 << 14)) >> 15;

#define    OUT_FDB_FRQ_DISP_FILTER_TIME   30
    frqFdbLpf.t = OUT_FDB_FRQ_DISP_FILTER_TIME;
    frqFdbLpf.in = (frqFdbTmp);
    frqFdbLpf.calc(&frqFdbLpf);

    frqFdbDispTmp = frqFdbLpf.out;

    checkSpeedFdbDir(frqFdbDispTmp);

    if (funcCode.code.speedFdbDir != funcCode.code.runDir)
    {
        frqFdbDispTmp = -frqFdbDispTmp;
    }

    if(ABS_INT32(frqFdbDispTmp) > (Uint32)32000)
    {
        frqFdb = (frqFdbDispTmp > 0) ? (int16)32000 : (int16)-32000;
    }
    else
    {
        frqFdb = frqFdbDispTmp;
    }

    gPhiRtDisp = (int32)gPhiRt*1800 >> 15;

#if DSP_2803X
    ABZPos = EQep1Regs.QPOSCNT & 0x0000FFFF;
#else
    if (motorFc.pgPara.elem.fvcPgSrc == FUNCCODE_fvcPgSrc_QEP2)
    {
        ABZPos = EQep2Regs.QPOSCNT & 0x0000FFFF;
    }
    else
    {
        ABZPos = EQep1Regs.QPOSCNT & 0x0000FFFF;
    }
#endif

#if DEBUG_F_INTERFACE
    if (speedMotor == frq2Core)
    {
        frqRun = frqDroop;
    }

    if(ABS_INT32(frqRun) > (Uint32)32000)
    {
        frqRunDisp = (frqRun > 0) ? (int16)32000 : (int16)-32000;
    }
    else
    {
        frqRunDisp = frqRun;
    }

#if DEBUG_F_POSITION_CTRL
    pcOriginDisp = pcOrigin / 4;
#endif

    outCurrentDispOld = outCurrentDisp;

    tmp = ((int32)outCurrent * currentPu) >> 8;

#if 1
    if (ABS_INT32(outCurrentDispOldTmp - tmp) > 15)
    {
        outCurrentDisp = (tmp + (1 << 3)) >> 4;
        outCurrentDispOldTmp = tmp;
    }
#else

    currentInLpf.in = (tmp + (1 << 3)) >> 4;
    currentInLpf.calc(&currentInLpf);
    outCurrentDisp = currentInLpf.out;
#endif
    currentOcDisp = ((int32)currentOc * currentPu + (1 << 11)) >> 12;

    outVoltageDisp = ((int32)outVoltage * invPara.ratingVoltage + (1 << 11)) >> 12;

    softOcDoFlag = 0;
    if(runFlag.bit.run &&(funcCode.code.softOCValue) && (outCurrentDisp >= ((Uint32)funcCode.code.softOCValue*motorFc.motorPara.elem.ratingCurrent/1000)))
    {

        if(softOCTicker < ((Uint32)funcCode.code.softOCDelay*TIME_UNIT_CURRENT_CHK / CORE_FUNC_PERIOD))
        {
            softOCTicker++;
        }
        else
        {
            softOcDoFlag = 1;

        }
    }
    else
    {
        softOCTicker = 0;
    }

#if DEBUG_F_POWER_ADD

    if (outPower > 0)
    {
        powerAddDec += outPower;

        if (powerAddDec > POWER_ADD_COUNT_DEC)
        {

            powerAddDec = 0;

            if (++funcCode.code.powerAddupInt >= POWER_ADD_COUNT_INT)
            {

                funcCode.code.powerAddupInt = 0;

                funcCode.code.powerAddup++;
            }
        }
    }
#endif

    vcSpdLoopKp1 = funcCode.code.vcParaM1.vcSpdLoopKp1;
    vcSpdLoopKp2 = funcCode.code.vcParaM1.vcSpdLoopKp2;
#if DEBUG_F_POSITION_CTRL
    bPcErrorOk = 0;
    bPcErrorNear = 0;
#endif

    invtStatus.bit.run = runFlag.bit.run;
    invtStatus.bit.accDecStatus = runFlag.bit.accDecStatus;
    invtStatus.bit.uv = bUv;
#endif

}

void UpdateDataFunc2Core0p5ms(void)
{
    int32 droopValue;
	static Uint16 frq2CoreTmp;

    if (funcCode.code.droopCtrl
        && frq
        && (generatrixVoltage < DROOP_VOLTAGE_LEVEL)
        && (runStatus != RUN_STATUS_TUNE))
    {
        droopValue = ((int64)torqueCurrentAct * funcCode.code.droopCtrl) / motorFc.motorPara.elem.ratingCurrent;
        frqDroop = frq - (int64)frq * droopValue / 1000;
    }
    else
    {
        frqDroop = frq;
    }

    frq2CoreTmp = (frqVFRunRemainder + (frqDroop << 15)) / ((int32)frqPuQ15);
#if 1
    if (((funcCode.code.ovGain) || (funcCode.code.ocGain))
            && (motorFc.motorCtrlMode == FUNCCODE_motorCtrlMode_VF)
            && (motorFc.motorPara.elem.motorType != MOTOR_TYPE_PMSM)
            && (!funcCode.code.droopCtrl)
            && (runStatus != RUN_STATUS_TUNE)
            && (runFlag.bit.accDecStatus == DEC_SPEED)
			&& (ABS_INT16((int16)frq2CoreTmp) >= ABS_INT16((int16)speedMotor))
        )
    {

    }
    else
    {
        frq2Core = frq2CoreTmp;
    }
#endif

#if DEBUG_F_INTERFACE
#if !F_DEBUG_RAM
    copyDataFunc2Motor05ms();
#endif
#endif
}

extern Uint32 aptpSetOrigin;
void UpdateDataFunc2Core2ms(void)
{
#if DEBUG_F_INTERFACE
    frqCurAim2Core = ((frqCurAim << 15) + frqCurAimFrac) / ((int32)frqPuQ15);

    dspMainCmd.bit.motorCtrlMode = motorFc.motorCtrlMode;
    dspMainCmd.bit.accDecStatus = runFlag.bit.accDecStatus;
    dspMainCmd.bit.torqueCtrl = funcCode.code.torqueCtrl;
    dspMainCmd.bit.spdLoopI1 = motorFc.vcPara.spdLoopI;

    dspSubCmd.bit.outPhaseLossProtect = funcCode.code.outPhaseLossProtect;
    dspSubCmd.bit.inPhaseLossProtect = funcCode.code.inPhaseLossProtect%10;
    dspSubCmd.bit.contactorMode = funcCode.code.inPhaseLossProtect/10;
    dspSubCmd.bit.overloadMode = funcCode.code.overloadMode;
    dspSubCmd.bit.loseLoadProtectMode = funcCode.code.loseLoadProtectMode;
    dspSubCmd.bit.poffTransitoryNoStop = funcCode.code.pOffTransitoryNoStop;

    dspSubCmd.bit.cbc = funcCode.code.cbcEnable;

    dspSubCmd.bit.varFcByTem = funcCode.code.varFcByTem;

    dspMainCmd1.bit.pgLocation = motorFc.pgPara.elem.fvcPgSrc;

    dspMainCmd1.bit.modulationMode = funcCode.code.modulationMode;
    dspMainCmd1.bit.deadCompMode = funcCode.code.deadCompMode;
    dspMainCmd1.bit.frqPoint = funcCode.code.frqPoint;

    driveCoeffFrqFdb = 1000;
#if !F_DEBUG_RAM

    copyDataFunc2Motor2ms();
#else
#endif

#if DEBUG_F_MOTOR_FUNCCODE
#if !F_DEBUG_RAM

    copyDataFunc2CF2ms();
#endif
#endif
#endif
}
