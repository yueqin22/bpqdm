
#include "f_frqSrc.h"
#include "f_main.h"
#include "f_runSrc.h"
#include "f_io.h"
#include "f_menu.h"
#include "f_ui.h"

#if F_DEBUG_RAM
#define DEBUG_F_PID             0
#elif 1

#if !DEBUG_F_POSITION_CTRL
#define DEBUG_F_PID             1
#elif 1
#define DEBUG_F_PID             0
#endif

#endif

#define PID_ERROR_DEAD          20

#define PID_Q15_ONE             0x7fff
#define PID_Q15_SHIFT           15
#define PID_Q15_ROUND           (1 << 14)
#define PID_Q16_SHIFT           16
#define PID_QP_INIT             24
#define PID_QI_INIT             26
#define PID_QD_INIT             12
#define PID_KP_NORM_LIMIT       (1L << 15)
#define PID_KI_NORM_LIMIT       (1L << 16)
#define PID_QP2_ALIGN           16
#define PID_KI2_ALIGN           20
#define PID_KI2_ALIGN_MID       14
#define PID_PERMILLE            1000
#define PID_PERCENT             100
#define PID_PER_TEN_THOUSAND    10000
#define PID_DIFF_TD_UNIT        25
#define PID_DIFF_ERR_COEFF      3
#define PID_FC_SET_SCALE        2097
#define PID_FC_SET_SHIFT        6
#define PID_COMM_SCALE          13421
#define PID_COMM_SHIFT          12
#define PID_DELTA_SCALE         53687
#define PID_DELTA_SHIFT         14

PID_FUNC pidFunc = PID_FUNC_DEFAULTS;

#if DEBUG_F_PID
LINE_CHANGE_STRUCT pidFuncRef = LINE_CHANGE_STRTUCT_DEFALUTS;

#if 0

struct PID_PARA_LINE
{
    LINE_STRUCT pLine;
    LINE_STRUCT iLine;
    LINE_STRUCT dLine;
};
struct PID_PARA_LINE pidParaLine = {LINE_STRTUCT_DEFALUTS, LINE_STRTUCT_DEFALUTS, LINE_STRTUCT_DEFALUTS};
#else
    LINE_STRUCT pidParaLine = LINE_STRTUCT_DEFALUTS;
#endif

#define PID_LPF_FDB 0
#define PID_LPF_OUT 1
LowPassFilter pidLpf[2] = {LPF_DEFALUTS, LPF_DEFALUTS};

Uint32 pidInitTicker;
enum PID_CALC_FLAG
{
    PID_CALC_FLAG_NO_CALC,
    PID_CALC_FLAG_CALC,
    PID_CALC_FLAG_PID_INIT_HOLD,
    PID_CALC_FLAG_CALC_TRUE
};
enum PID_CALC_FLAG pidCalcFlag;

Uint32 pidFdbLoseTicker;
int32 pidError;
Uint16 bPidFdbLose;

void UpdatePidFuncPara(PID_FUNC *pid);
void UpdatePidFuncRefFdb(PID_FUNC *pid);

int32 FrqPidSetDeal(void)
{
    static Uint16 s_pidCalcFlagLast;
    Uint16 bPidCalcEnable;
    Uint16 bPidReset;
    int32 frqPidResult;
    int32 pidOutput = pidFunc.out;

    runFlag.bit.pid = 1;

    UpdatePidFuncRefFdb(&pidFunc);
    UpdatePidFuncPara(&pidFunc);

    bPidCalcEnable = (FUNCCODE_pidCalcMode_YES == funcCode.code.pidCalcMode)
                     || (runCmd.bit.common);

    if (!bPidCalcEnable)
    {
        pidCalcFlag = PID_CALC_FLAG_NO_CALC;
    }
    else if (PID_CALC_FLAG_NO_CALC == pidCalcFlag)
    {
        pidCalcFlag = PID_CALC_FLAG_CALC;
    }

    if ((PID_CALC_FLAG_CALC == pidCalcFlag)
        && (!s_pidCalcFlagLast)
        )
    {
        pidCalcFlag = PID_CALC_FLAG_PID_INIT_HOLD;

        pidInitTicker = 0;
    }

    if (PID_CALC_FLAG_PID_INIT_HOLD == pidCalcFlag)
    {
        if (++pidInitTicker >= (int32)funcCode.code.pidInitTime * (TIME_UNIT_PID_INIT / PID_CALC_PERIOD))
        {
            pidCalcFlag = PID_CALC_FLAG_CALC_TRUE;
        }

        pidFunc.out = (int32)funcCode.code.pidInit * PID_Q15_ONE / PID_PERMILLE;
        pidOutput = pidFunc.out;
    }

    pidError = pidFunc.ref - pidFunc.fdb;
    if (funcCode.code.pidDir^diFunc.f2.bit.pidDirRev)
    {
        pidError = -pidError;
    }

    if ((PID_CALC_FLAG_CALC_TRUE == pidCalcFlag)
        && (!diFunc.f1.bit.pidPause)
        && (!runFlag.bit.jog)
        && ((ABS_INT32(pidError) > pidFunc.errorDead) )
        )
    {

        pidFunc.calc(&pidFunc);

        pidLpf[PID_LPF_OUT].t = (int32)funcCode.code.pidOutLpfTime * (TIME_UNIT_PID_FILTER / PID_CALC_PERIOD);
        pidLpf[PID_LPF_OUT].in = pidFunc.out;
        pidLpf[PID_LPF_OUT].calc(&pidLpf[PID_LPF_OUT]);
        pidOutput = pidLpf[PID_LPF_OUT].out;
    }

    pidFunc.error2 = pidFunc.error1;
    pidFunc.error1 = pidFunc.error;
    pidFunc.error = pidError;

    bPidReset = (!pidCalcFlag)
                || (frqCalcSrcOld != frqCalcSrc)
                || ((plcStepOld) && (!plcStep));

    if (bPidReset)
    {
        pidFunc.deltaPRem = 0;
        pidFunc.deltaIRem = 0;
        pidFunc.deltaDRem = 0;
        pidFunc.deltaRemainder = 0;
        pidFunc.out = 0;
        pidLpf[PID_LPF_OUT].out = 0;
        pidLpf[PID_LPF_OUT].remainder = 0;
        pidOutput = 0;
    }

    frqPidResult = (pidOutput * maxFrq + PID_Q15_ROUND) >> PID_Q15_SHIFT;

    s_pidCalcFlagLast = pidCalcFlag;

    return frqPidResult;
}

void PidFuncCalc(PID_FUNC *pid)
{
    static Uint16 s_pidParaChgDiLast;
    int32 kpGain, kiGain, kdGain;
    int32 deltaProportional, deltaIntegral, deltaDerivative;

    kpGain = pid->Kp;
    kiGain = pid->Ki;
    kdGain = pid->Kd;

    if (FUNCCODE_pidParaChgCondition_DI  == funcCode.code.pidParaChgCondition)
    {
        if (diFunc.f2.bit.pidChg)
        {
            kpGain = pid->Kp2;
            kdGain = pid->Kd2;
            kiGain = pid->Ki2;
        }

        if(diFunc.f2.bit.pidChg != s_pidParaChgDiLast)
        {
            int32 paraScaleP,paraScaleD;

            paraScaleP = (kpGain * (pidError - pid->error));
            paraScaleD = (kdGain * (pidError - pid->error2 + PID_DIFF_ERR_COEFF * (pid->error - pid->error1)));

            if(pid->Qi >= pid->Qp)
            {
                paraScaleP = paraScaleP<<(pid->Qi - pid->Qp);
            }
            else
            {
                paraScaleP = paraScaleP>>(pid->Qp - pid->Qi);
            }

            if(pid->Qi >= pid->Qd)
            {
                paraScaleD = paraScaleD<<(pid->Qd - pid->Qi);
            }
            else
            {
                paraScaleD = paraScaleD>>(pid->Qd - pid->Qi);
            }

            kiGain = (pid->delta<<pid->Qi - paraScaleP - paraScaleD )/((pidError + pid->error) >> 1);
        }

        s_pidParaChgDiLast = diFunc.f2.bit.pidChg;
    }
    else if (FUNCCODE_pidParaChgCondition_PID_ERROR == funcCode.code.pidParaChgCondition)
    {
        pidParaLine.x1 = pid->errorSmall;
        pidParaLine.x2 = pid->errorBig;

        pidParaLine.y1 = pid->Kp;
        pidParaLine.y2 = pid->Kp2;
        pidParaLine.x = ABS_INT32(pidError);
        pidParaLine.calc(&pidParaLine);
        kpGain = pidParaLine.y;

        pidParaLine.y1 = pid->Ki;
        pidParaLine.y2 = pid->Ki2;
        pidParaLine.x = ABS_INT32(pidError);
        pidParaLine.calc(&pidParaLine);
        kiGain = pidParaLine.y;

        pidParaLine.y1 = pid->Kd;
        pidParaLine.y2 = pid->Kd2;
        pidParaLine.x = ABS_INT32(pidError);
        pidParaLine.calc(&pidParaLine);
        kdGain = pidParaLine.y;
    }

    pid->deltaPRem += (int64)kpGain * (pidError - pid->error);
    deltaProportional = pid->deltaPRem >> pid->Qp;
    pid->deltaPRem = pid->deltaPRem - (deltaProportional << pid->Qp);

    if (( (((pid->out >= pid->outMax) && (pidError > 0))
        || ((pid->out < pid->outMin) && (pidError < 0))
        || (frqFlag.bit.frqSetLimit))
         && (funcCode.code.pidIAttribute/10))
        || ((diFunc.f2.bit.pidPauseI) && (funcCode.code.pidIAttribute%10)))
    {
        deltaIntegral = 0;
        pid->deltaIRem = 0;
    }
    else
    {
        pid->deltaIRem += ((int64)kiGain * ((pidError + pid->error) >> 1));
        deltaIntegral = pid->deltaIRem >> pid->Qi;
        pid->deltaIRem = pid->deltaIRem - (deltaIntegral << pid->Qi);
    }

    pid->deltaDRem += (int64)kdGain * (pidError - pid->error2 + PID_DIFF_ERR_COEFF * (pid->error - pid->error1));
    deltaDerivative = pid->deltaDRem >> pid->Qd;

    if((deltaDerivative > 0) && (deltaDerivative > pid->pidDLimit))
    {
        deltaDerivative = pid->pidDLimit;
    }
    else if ((deltaDerivative < 0) && (deltaDerivative < (-pid->pidDLimit)))
    {
        deltaDerivative = (-pid->pidDLimit);
    }
    pid->deltaDRem = pid->deltaDRem - (deltaDerivative << pid->Qd);

    pid->delta = deltaProportional + deltaIntegral + deltaDerivative;

    pid->delta += pid->deltaRemainder;
    if (pid->delta > pid->deltaMax)
    {
        pid->deltaRemainder = pid->delta - pid->deltaMax;
        pid->delta = pid->deltaMax;
    }
    else if (pid->delta < pid->deltaMin)
    {
        pid->deltaRemainder = pid->delta - pid->deltaMin;
        pid->delta = pid->deltaMin;
    }
    else
    {
        pid->deltaRemainder = 0;
    }

    if (frqFlag.bit.frqSetLimit)
    {
        if (((pid->out > 0) && (pid->delta < 0))
            || ((pid->out < 0) && (pid->delta > 0))
            )
        {
            pid->out += pid->delta;
        }
        else
        {
            pid->deltaRemainder = 0;
            pid->deltaPRem = 0;
            pid->deltaDRem = 0;
        }
    }
    else
    {
        pid->out += pid->delta;
    }

    if (pid->out > pid->outMax)
    {
        pid->out = pid->outMax;
        pid->deltaRemainder = 0;
        pid->deltaPRem = 0;
        pid->deltaDRem = 0;
    }
    else if (pid->out < pid->outMin)
    {
        pid->out = pid->outMin;
        pid->deltaRemainder = 0;
        pid->deltaPRem = 0;
        pid->deltaDRem = 0;
    }

}

LOCALF void UpdatePidFuncPara(PID_FUNC *pid)
{
    Uint32 kpGain = (((Uint32)funcCode.code.pidKp << PID_Q16_SHIFT) / PID_PERMILLE) << 8;
    Uint32 kiGain = (((((((Uint32)funcCode.code.pidKp * PID_CALC_PERIOD) << 14) / PID_PERCENT) << 6) / PID_PERCENT) << 6)
            / funcCode.code.pidTi;

    Uint16 qFormatP, qFormatI, qFormatD;
    int32 outputLimitMax;
    int32 outputLimitMin;

    qFormatP = PID_QP_INIT;
    while (kpGain >= PID_KP_NORM_LIMIT)
    {
        kpGain >>= 1;
        qFormatP--;
    }

    if (pid->Qp > qFormatP)
    {
        pid->deltaPRem = 0;
    }
    pid->Qp = qFormatP;
    pid->Kp = kpGain;

    qFormatI = PID_QI_INIT;
    while (kiGain >= PID_KI_NORM_LIMIT)
    {
        kiGain >>= 1;
        qFormatI--;
    }

    if (pid->Qi > qFormatI)
    {
        pid->deltaIRem = 0;
    }
    pid->Qi = qFormatI;
    pid->Ki = kiGain;

    qFormatD = PID_QD_INIT;
    pid->Kd = (((((Uint32)funcCode.code.pidKp * funcCode.code.pidTd) << 6) / 5) << 3) / (PID_DIFF_TD_UNIT*PID_CALC_PERIOD);
    pid->Qd = qFormatD;

    pid->errorDead = ((Uint32)funcCode.code.pidErrMin << PID_Q15_SHIFT) / PID_PERMILLE;

    if (funcCode.code.pidFdbSrc <= FUNCCODE_pidFdbSrc_AI1_SUB_AI2)
    {
        pid->errorDead += PID_ERROR_DEAD;
    }

    outputLimitMax = PID_Q15_ONE;
    if (upperFrq < maxFrq)
    {
        outputLimitMax = ((int32)upperFrq << PID_Q15_SHIFT) / maxFrq;
    }

    if ((funcCode.code.frqYSrc == FUNCCODE_frqXySrc_PID)
       && (upperFrq > frqAiPu))
    {
        outputLimitMax = ((int32)frqAiPu << PID_Q15_SHIFT) / maxFrq;
    }

    pid->outMax = outputLimitMax;

    outputLimitMin = lowerFrq;
    if (0 == funcCode.code.reverseCutOffFrq)
    {
        ;
    }

    else
    {
        if (outputLimitMin < funcCode.code.reverseCutOffFrq)
        {
            outputLimitMin = funcCode.code.reverseCutOffFrq;
        }

        if ((funcCode.code.frqYSrc == FUNCCODE_frqXySrc_PID)
            && (outputLimitMin > frqAiPu))
        {
            outputLimitMin = frqAiPu;
        }

        outputLimitMin = -outputLimitMin;
    }
    pid->outMin = (outputLimitMin << PID_Q15_SHIFT) / maxFrq;

    pid->pidDLimit =  (Uint32)funcCode.code.pidDLimit * PID_DELTA_SCALE >> PID_DELTA_SHIFT;

    pid->deltaMax = (Uint32)funcCode.code.pidOutDeltaMax * PID_DELTA_SCALE >> PID_DELTA_SHIFT;
    pid->deltaMin = -(int32)((Uint32)funcCode.code.pidOutDeltaMin * PID_DELTA_SCALE >> PID_DELTA_SHIFT);

    if (qFormatP >= PID_QP2_ALIGN)
    {
        pid->Kp2 = (((Uint32)funcCode.code.pidKp2 << PID_Q16_SHIFT) / PID_PERMILLE) << (qFormatP - PID_QP2_ALIGN);
    }
    else
    {
         pid->Kp2 = (((Uint32)funcCode.code.pidKp2 << qFormatP) / PID_PERMILLE);
    }

    if (qFormatI >= PID_KI2_ALIGN)
    {
        pid->Ki2 = (((((((Uint32)funcCode.code.pidKp2 * PID_CALC_PERIOD) << 14) / PID_PERCENT) << 6) / PID_PERCENT) << (qFormatI - PID_KI2_ALIGN)) / funcCode.code.pidTi2;
    }
    else if(qFormatI >= PID_KI2_ALIGN_MID)
    {
        pid->Ki2 = ((((((Uint32)funcCode.code.pidKp2 * PID_CALC_PERIOD) << 14) / PID_PERCENT) << (qFormatI - PID_KI2_ALIGN_MID)) / PID_PERCENT) / funcCode.code.pidTi2;
    }
    else
    {
       pid->Ki2 = ((((Uint32)funcCode.code.pidKp2 * PID_CALC_PERIOD) << qFormatI) / PID_PER_TEN_THOUSAND) / funcCode.code.pidTi2;
    }

    pid->Kd2 = (((((Uint32)funcCode.code.pidKp2 * funcCode.code.pidTd2) << 6) / 5) << 3) / (PID_DIFF_TD_UNIT*PID_CALC_PERIOD);

    pid->errorSmall = (int32)funcCode.code.pidParaChgDelta1 * PID_Q15_ONE / PID_PERMILLE;
    pid->errorBig   = (int32)funcCode.code.pidParaChgDelta2 * PID_Q15_ONE / PID_PERMILLE;

}

LOCALF void UpdatePidFuncRefFdb(PID_FUNC *pid)
{
    int32 setpointAim;
    int32 feedback;
    int16 aiIndex;

    switch (funcCode.code.pidSetSrc)
    {

        case FUNCCODE_pidSetSrc_FC:
            setpointAim = (int32)(int16)funcCode.code.pidSet * PID_FC_SET_SCALE >> PID_FC_SET_SHIFT;
            break;

        case FUNCCODE_pidSetSrc_AI1:
        case FUNCCODE_pidSetSrc_AI2:
        case FUNCCODE_pidSetSrc_AI3:
            aiIndex = funcCode.code.pidSetSrc - FUNCCODE_pidSetSrc_AI1;
            setpointAim = aiDeal[aiIndex].set;
            break;

        case FUNCCODE_pidSetSrc_PULSE:
            setpointAim = pulseInSet;
            break;

        case FUNCCODE_pidSetSrc_COMM:
            setpointAim = (int32)(int16)funcCode.code.frqComm * PID_COMM_SCALE >> PID_COMM_SHIFT;
            break;

        case FUNCCODE_pidSetSrc_MULTI_SET:
            setpointAim = UpdateMultiSetFrq(diFunc.f1.bit.multiSet);
            setpointAim = setpointAim * PID_Q15_ONE / (int32)maxFrq;
            break;

        default:
            break;
    }

#if 0

    if (aimValue < 0)
    {
        aimValue = 0;
    }
#endif

    pidFuncRef.aimValue = setpointAim;
    pidFuncRef.tickerAll = (Uint32)funcCode.code.pidSetChangeTime * (TIME_UNIT_PID_SET_CHANGE / PID_CALC_PERIOD);
    pidFuncRef.maxValue = PID_Q15_ONE;
    pidFuncRef.curValue = pid->ref;
    pidFuncRef.calc(&pidFuncRef);
    pid->ref = pidFuncRef.curValue;

    switch (funcCode.code.pidFdbSrc)
    {

        case FUNCCODE_pidFdbSrc_AI1:
        case FUNCCODE_pidFdbSrc_AI2:
        case FUNCCODE_pidFdbSrc_AI3:
            feedback = aiDeal[funcCode.code.pidFdbSrc - FUNCCODE_pidFdbSrc_AI1].set;
            break;

        case FUNCCODE_pidFdbSrc_AI1_SUB_AI2:
            feedback = (int32)aiDeal[0].set - aiDeal[1].set;
            break;

        case FUNCCODE_pidFdbSrc_PULSE:
            feedback = pulseInSet;
            break;

        case FUNCCODE_pidFdbSrc_COMM:
            feedback = (int32)(int16)funcCode.code.frqComm * PID_COMM_SCALE >> PID_COMM_SHIFT;
            break;

        case FUNCCODE_pidFdbSrc_AI1_ADD_AI2:
            feedback = (int32)aiDeal[0].set + aiDeal[1].set;
            break;

        case FUNCCODE_pidFdbSrc_MAX_AI:
            feedback = GetMax(ABS_INT32(aiDeal[0].set), ABS_INT32(aiDeal[1].set));
            break;

        case FUNCCODE_pidFdbSrc_MIN_AI:
            feedback = GetMin(ABS_INT32(aiDeal[0].set), ABS_INT32(aiDeal[1].set));
            break;

        default:
            break;
    }

#if 0

    if (fdb < 0)
    {
        fdb = 0;
    }
#endif

    feedback = GetMin(feedback, PID_Q15_ONE);

    if (PID_CALC_FLAG_CALC_TRUE == pidCalcFlag)
    {

        if ((funcCode.code.pidFdbLoseDetect) && (runFlag.bit.run) && (feedback < (int32)funcCode.code.pidFdbLoseDetect * PID_Q15_ONE / PID_PERMILLE))
        {
            if (pidFdbLoseTicker < (int32)funcCode.code.pidFdbLoseDetectTime * (TIME_UNIT_PID_FDB_LOSE / PID_CALC_PERIOD))
            {
                pidFdbLoseTicker++;
            }
            else
            {
                bPidFdbLose = 1;

                errorOther = ERROR_FDB_LOSE;
            }
        }
        else
        {
            bPidFdbLose = 0;
            pidFdbLoseTicker = 0;
        }
    }
    else
    {
        pidFdbLoseTicker = 0;
    }

    pidLpf[PID_LPF_FDB].t = (int32)funcCode.code.pidFdbLpfTime * (TIME_UNIT_PID_FILTER / PID_CALC_PERIOD);
    pidLpf[PID_LPF_FDB].in = feedback;
    pidLpf[PID_LPF_FDB].calc(&pidLpf[PID_LPF_FDB]);
    pid->fdb = pidLpf[PID_LPF_FDB].out;
}

#elif 1

int32 FrqPidSetDeal(void){}
void PidFuncCalc(PID_FUNC *p){}

#endif
