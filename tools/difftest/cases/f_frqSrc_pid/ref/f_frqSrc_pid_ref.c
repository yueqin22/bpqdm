
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

void UpdatePidFuncPara(PID_FUNC *p);
void UpdatePidFuncRefFdb(PID_FUNC *p);

int32 FrqPidSetDeal(void)
{
    static Uint16 pidCalcFlagOld;
    int32 frqPid;
    int32 pidOut = pidFunc.out;

    runFlag.bit.pid = 1;

    UpdatePidFuncRefFdb(&pidFunc);
    UpdatePidFuncPara(&pidFunc);

    if ((FUNCCODE_pidCalcMode_YES == funcCode.code.pidCalcMode)
            || (runCmd.bit.common))

    {
        if (PID_CALC_FLAG_NO_CALC == pidCalcFlag)
        {
            pidCalcFlag = PID_CALC_FLAG_CALC;
        }
    }
    else
    {
        pidCalcFlag = PID_CALC_FLAG_NO_CALC;
    }

    if ((PID_CALC_FLAG_CALC == pidCalcFlag)
        && (!pidCalcFlagOld)
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

        pidFunc.out = (int32)funcCode.code.pidInit * 0x7fff / 1000;
        pidOut = pidFunc.out;
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
        pidOut = pidLpf[PID_LPF_OUT].out;
    }

    pidFunc.error2 = pidFunc.error1;
    pidFunc.error1 = pidFunc.error;
    pidFunc.error = pidError;

    if ((!pidCalcFlag)
        || (frqCalcSrcOld != frqCalcSrc)
        || ((plcStepOld) && (!plcStep))
        )
    {
        pidFunc.deltaPRem = 0;
        pidFunc.deltaIRem = 0;
        pidFunc.deltaDRem = 0;
        pidFunc.deltaRemainder = 0;
        pidFunc.out = 0;
		pidLpf[PID_LPF_OUT].out = 0;
		pidLpf[PID_LPF_OUT].remainder = 0;
        pidOut = 0;
    }

    frqPid = (pidOut * maxFrq + (1 << 14)) >> 15;

    pidCalcFlagOld = pidCalcFlag;

    return frqPid;
}

void PidFuncCalc(PID_FUNC *p)
{
    static Uint16 pidChgOld;
    int32 Kp, Ki, Kd;
    int32 deltaP, deltaI, deltaD;

    Kp = p->Kp;
    Ki = p->Ki;
    Kd = p->Kd;

    if (FUNCCODE_pidParaChgCondition_DI  == funcCode.code.pidParaChgCondition)
    {
        if (diFunc.f2.bit.pidChg)
        {
            Kp = p->Kp2;
            Kd = p->Kd2;
            Ki = p->Ki2;
        }

        if(diFunc.f2.bit.pidChg != pidChgOld)
        {
            int32 tmp1,tmp2;

            tmp1 = (Kp * (pidError - p->error));
            tmp2 = (Kd * (pidError - p->error2 + 3 * (p->error - p->error1)));

            if(p->Qi >= p->Qp)
            {
                tmp1 = tmp1<<(p->Qi - p->Qp);
            }
            else
            {
                tmp1 = tmp1>>(p->Qp - p->Qi);
            }

            if(p->Qi >= p->Qd)
            {
                tmp2 = tmp2<<(p->Qd - p->Qi);
            }
            else
            {
                tmp2 = tmp2>>(p->Qd - p->Qi);
            }

            Ki = (p->delta<<p->Qi - tmp1 - tmp2 )/((pidError + p->error) >> 1);
        }

        pidChgOld = diFunc.f2.bit.pidChg;
    }
    else if (FUNCCODE_pidParaChgCondition_PID_ERROR == funcCode.code.pidParaChgCondition)
    {
        pidParaLine.x1 = p->errorSmall;
        pidParaLine.x2 = p->errorBig;

        pidParaLine.y1 = p->Kp;
        pidParaLine.y2 = p->Kp2;
        pidParaLine.x = ABS_INT32(pidError);
        pidParaLine.calc(&pidParaLine);
        Kp = pidParaLine.y;

        pidParaLine.y1 = p->Ki;
        pidParaLine.y2 = p->Ki2;
        pidParaLine.x = ABS_INT32(pidError);
        pidParaLine.calc(&pidParaLine);
        Ki = pidParaLine.y;

        pidParaLine.y1 = p->Kd;
        pidParaLine.y2 = p->Kd2;
        pidParaLine.x = ABS_INT32(pidError);
        pidParaLine.calc(&pidParaLine);
        Kd = pidParaLine.y;
    }

    p->deltaPRem += (int64)Kp * (pidError - p->error);
    deltaP = p->deltaPRem >> p->Qp;
    p->deltaPRem = p->deltaPRem - (deltaP << p->Qp);

    if (( (((p->out >= p->outMax) && (pidError > 0))
        || ((p->out < p->outMin) && (pidError < 0))
        || (frqFlag.bit.frqSetLimit))
         && (funcCode.code.pidIAttribute/10))
        || ((diFunc.f2.bit.pidPauseI) && (funcCode.code.pidIAttribute%10)))
    {
        deltaI = 0;
        p->deltaIRem = 0;
    }
    else
    {
        p->deltaIRem += ((int64)Ki * ((pidError + p->error) >> 1));
        deltaI = p->deltaIRem >> p->Qi;
        p->deltaIRem = p->deltaIRem - (deltaI << p->Qi);
    }

    p->deltaDRem += (int64)Kd * (pidError - p->error2 + 3 * (p->error - p->error1));
    deltaD = p->deltaDRem >> p->Qd;

    if((deltaD > 0) && (deltaD > p->pidDLimit))
    {
        deltaD = p->pidDLimit;
    }
    else if ((deltaD < 0) && (deltaD < (-p->pidDLimit)))
    {
        deltaD = (-p->pidDLimit);
    }
    p->deltaDRem = p->deltaDRem - (deltaD << p->Qd);

    p->delta = deltaP + deltaI + deltaD;

    p->delta += p->deltaRemainder;
    if (p->delta > p->deltaMax)
    {
        p->deltaRemainder = p->delta - p->deltaMax;
        p->delta = p->deltaMax;
    }
    else if (p->delta < p->deltaMin)
    {
        p->deltaRemainder = p->delta - p->deltaMin;
        p->delta = p->deltaMin;
    }
    else
    {
        p->deltaRemainder = 0;
    }

    if (frqFlag.bit.frqSetLimit)
    {
        if (((p->out > 0) && (p->delta < 0))
            || ((p->out < 0) && (p->delta > 0))
            )
        {
            p->out += p->delta;
        }
        else
        {
            p->deltaRemainder = 0;
            p->deltaPRem = 0;
            p->deltaDRem = 0;
        }
    }
    else
    {
        p->out += p->delta;
    }

    if (p->out > p->outMax)
    {
        p->out = p->outMax;
        p->deltaRemainder = 0;
        p->deltaPRem = 0;
        p->deltaDRem = 0;
    }
    else if (p->out < p->outMin)
    {
        p->out = p->outMin;
        p->deltaRemainder = 0;
        p->deltaPRem = 0;
        p->deltaDRem = 0;
    }

}

LOCALF void UpdatePidFuncPara(PID_FUNC *p)
{
    Uint32 Kp = (((Uint32)funcCode.code.pidKp << 16) / 1000) << 8;
    Uint32 Ki = (((((((Uint32)funcCode.code.pidKp * PID_CALC_PERIOD) << 14) / 100) << 6) / 100) << 6)
            / funcCode.code.pidTi;

    Uint16 Qp, Qi, Qd;
    int32 outMax;
    int32 outMin;

    Qp = 24;
    while (Kp >= 1L << 15)
    {
        Kp >>= 1;
        Qp--;
    }

    if (p->Qp > Qp)
    {
        p->deltaPRem = 0;
    }
    p->Qp = Qp;
    p->Kp = Kp;

    Qi = 26;
    while (Ki >= 1L << 16)
    {
        Ki >>= 1;
        Qi--;
    }

    if (p->Qi > Qi)
    {
        p->deltaIRem = 0;
    }
    p->Qi = Qi;
    p->Ki = Ki;

    Qd = 12;
    p->Kd = (((((Uint32)funcCode.code.pidKp * funcCode.code.pidTd) << 6) / 5) << 3) / (25*PID_CALC_PERIOD);
    p->Qd = Qd;

    p->errorDead = ((Uint32)funcCode.code.pidErrMin << 15) / 1000;

    if (funcCode.code.pidFdbSrc <= FUNCCODE_pidFdbSrc_AI1_SUB_AI2)
    {
        p->errorDead += PID_ERROR_DEAD;
    }

    outMax = 0x7FFF;
    if (upperFrq < maxFrq)
    {
        outMax = ((int32)upperFrq << 15) / maxFrq;
    }

    if ((funcCode.code.frqYSrc == FUNCCODE_frqXySrc_PID)
       && (upperFrq > frqAiPu))
    {
        outMax = ((int32)frqAiPu << 15) / maxFrq;
    }

    p->outMax = outMax;

    outMin = lowerFrq;
    if (0 == funcCode.code.reverseCutOffFrq)
    {
        ;
    }

    else
    {
        if (outMin < funcCode.code.reverseCutOffFrq)
        {
            outMin = funcCode.code.reverseCutOffFrq;
        }

        if ((funcCode.code.frqYSrc == FUNCCODE_frqXySrc_PID)
            && (outMin > frqAiPu))
        {
            outMin = frqAiPu;
        }

        outMin = -outMin;
    }
    p->outMin = (outMin << 15) / maxFrq;

    p->pidDLimit =  (Uint32)funcCode.code.pidDLimit * 53687>>14;

    p->deltaMax = (Uint32)funcCode.code.pidOutDeltaMax * 53687>>14;
    p->deltaMin = -(int32)((Uint32)funcCode.code.pidOutDeltaMin * 53687>>14);

    if (Qp >= 16)
    {
        p->Kp2 = (((Uint32)funcCode.code.pidKp2 << 16) / 1000) << (Qp - 16);
    }
    else
    {
         p->Kp2 = (((Uint32)funcCode.code.pidKp2 << Qp) / 1000);
    }

    if (Qi >= 20)
    {
        p->Ki2 = (((((((Uint32)funcCode.code.pidKp2 * PID_CALC_PERIOD) << 14) / 100) << 6) / 100) << (Qi - 20)) / funcCode.code.pidTi2;
    }
    else if(Qi >= 14)
    {
        p->Ki2 = ((((((Uint32)funcCode.code.pidKp2 * PID_CALC_PERIOD) << 14) / 100) << (Qi - 14)) / 100) / funcCode.code.pidTi2;
    }
    else
    {
       p->Ki2 = ((((Uint32)funcCode.code.pidKp2 * PID_CALC_PERIOD) << Qi) / 10000) / funcCode.code.pidTi2;
    }

    p->Kd2 = (((((Uint32)funcCode.code.pidKp2 * funcCode.code.pidTd2) << 6) / 5) << 3) / (25*PID_CALC_PERIOD);

	p->errorSmall = (int32)funcCode.code.pidParaChgDelta1 * 0x7FFF / 1000;
    p->errorBig   = (int32)funcCode.code.pidParaChgDelta2 * 0x7FFF / 1000;

}

LOCALF void UpdatePidFuncRefFdb(PID_FUNC *p)
{
    int32 aimValue;
    int32 fdb;
    int16 i;

    switch (funcCode.code.pidSetSrc)
    {

        case FUNCCODE_pidSetSrc_FC:
            aimValue = (int32)(int16)funcCode.code.pidSet * 2097 >> 6;
            break;

        case FUNCCODE_pidSetSrc_AI1:
        case FUNCCODE_pidSetSrc_AI2:
        case FUNCCODE_pidSetSrc_AI3:
            i = funcCode.code.pidSetSrc - FUNCCODE_pidSetSrc_AI1;
            aimValue = aiDeal[i].set;
            break;

        case FUNCCODE_pidSetSrc_PULSE:
            aimValue = pulseInSet;
            break;

        case FUNCCODE_pidSetSrc_COMM:
            aimValue = (int32)(int16)funcCode.code.frqComm * 13421 >> 12;
            break;

        case FUNCCODE_pidSetSrc_MULTI_SET:
            aimValue = UpdateMultiSetFrq(diFunc.f1.bit.multiSet);
            aimValue = aimValue * 0x7fff / (int32)maxFrq;
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

    pidFuncRef.aimValue = aimValue;
    pidFuncRef.tickerAll = (Uint32)funcCode.code.pidSetChangeTime * (TIME_UNIT_PID_SET_CHANGE / PID_CALC_PERIOD);
    pidFuncRef.maxValue = 0x7fff;
    pidFuncRef.curValue = p->ref;
    pidFuncRef.calc(&pidFuncRef);
    p->ref = pidFuncRef.curValue;

    switch (funcCode.code.pidFdbSrc)
    {

        case FUNCCODE_pidFdbSrc_AI1:
        case FUNCCODE_pidFdbSrc_AI2:
        case FUNCCODE_pidFdbSrc_AI3:
            fdb = aiDeal[funcCode.code.pidFdbSrc - FUNCCODE_pidFdbSrc_AI1].set;
            break;

        case FUNCCODE_pidFdbSrc_AI1_SUB_AI2:
            fdb = (int32)aiDeal[0].set - aiDeal[1].set;
            break;

        case FUNCCODE_pidFdbSrc_PULSE:
            fdb = pulseInSet;
            break;

        case FUNCCODE_pidFdbSrc_COMM:
            fdb = (int32)(int16)funcCode.code.frqComm * 13421 >> 12;
            break;

        case FUNCCODE_pidFdbSrc_AI1_ADD_AI2:
            fdb = (int32)aiDeal[0].set + aiDeal[1].set;
            break;

        case FUNCCODE_pidFdbSrc_MAX_AI:
            fdb = GetMax(ABS_INT32(aiDeal[0].set), ABS_INT32(aiDeal[1].set));
            break;

        case FUNCCODE_pidFdbSrc_MIN_AI:
            fdb = GetMin(ABS_INT32(aiDeal[0].set), ABS_INT32(aiDeal[1].set));
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

    if (fdb > 0x7fff)
    {
        fdb = 0x7fff;
    }

    if (PID_CALC_FLAG_CALC_TRUE == pidCalcFlag)
    {

        if ((funcCode.code.pidFdbLoseDetect) && (runFlag.bit.run) && (fdb < (int32)funcCode.code.pidFdbLoseDetect * 0x7fff / 1000))
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
    pidLpf[PID_LPF_FDB].in = fdb;
    pidLpf[PID_LPF_FDB].calc(&pidLpf[PID_LPF_FDB]);
    p->fdb = pidLpf[PID_LPF_FDB].out;
}

#elif 1

int32 FrqPidSetDeal(void){}
void PidFuncCalc(PID_FUNC *p){}

#endif
