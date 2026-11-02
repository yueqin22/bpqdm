#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_main.h"
#include "f_menu.h"
#include "f_debug.h"

#if F_DEBUG_RAM

#define DEBUG_F_S_CURVE_A         0
#define DEBUG_F_S_CURVE_B         0

#elif 1

#define DEBUG_F_S_CURVE_A         1

#if !DEBUG_F_POSITION_CTRL
#define DEBUG_F_S_CURVE_B         1
#elif 1
#define DEBUG_F_S_CURVE_B         1
#endif

#endif

#define ACC_DEC_DEBUG 1
enum S_CURVE_A_STATUS
{
    SA_STEP_START_S,
    SA_STEP_LINE,
    SA_STEP_END_S,
    SA_STEP_OK
};
enum S_CURVE_B_STATUS
{
    SB_STEP_START_S,
    SB_STEP_LINE,
    SB_STEP_END_S,
    SB_STEP_OK
};

#define LINE_FRQ_CHANGE_STRTUCT_DEFALUTS   \
{                                          \
	(void (*)(void *))LineChangeCalc       \
}

LINE_CHANGE_STRUCT frqLine = LINE_FRQ_CHANGE_STRTUCT_DEFALUTS;

int32 frqCurAimOld;
int32 accTimeOld;
int32 decTimeOld;

Uint16 bFrqCurAimChg;

LOCALD int32 SCurveOridinaryCalc(int32 frq0, int32 accDecTime);
LOCALD int32 SCurveSpecialCalc(int32 frq0, int32 accDecTime);

static int s_AccDecUseRun(void)
{
    if (!((funcCode.code.ovGain) || (funcCode.code.ocGain)))
    {
        return 0;
    }
    if (motorFc.motorCtrlMode != FUNCCODE_motorCtrlMode_VF)
    {
        return 0;
    }
    if (motorFc.motorPara.elem.motorType == MOTOR_TYPE_PMSM)
    {
        return 0;
    }
    if (funcCode.code.droopCtrl)
    {
        return 0;
    }
    if (runStatus == RUN_STATUS_TUNE)
    {
        return 0;
    }
    if ((frqCurAim == 0) && (ABS_INT32(frqRun) <= 50))
    {
        return 0;
    }
    return 1;
}

static int s_AccDecIsAcc(int32 frq0)
{
    if ((frq0 >= 0) && (frqCurAim > frq0))
    {
        return 1;
    }
    if ((frq0 <= 0) && (frqCurAim < frq0))
    {
        return 1;
    }
    return 0;
}

static void s_BenchOverride(void)
{
    switch (funcCode.code.accDecBenchmark)
    {
        case 0:
            benchFrq = maxFrq;
            break;

        case 2:
            benchFrq = 100 * decNumber[funcCode.code.frqPoint];
            break;

        default:
            break;
    }
}

#if DEBUG_F_S_CURVE_A || DEBUG_F_S_CURVE_A
static Uint16 s_CurveDowngrade(Uint16 mode, int32 accDecTime)
{
    if ((FUNCCODE_accDecSpdCurve_S_CURVE_B == mode) &&
        ((accDecTime > (int32)100 * TIME_UNIT_MS_PER_SEC / timeBench)
        || (ABS_INT32(frqCurAim) > 6 * motorFc.motorPara.elem.ratingFrq)))
    {
        return FUNCCODE_accDecSpdCurve_LINE;
    }
    return mode;
}
#endif

void AccDecFrqCalc(int32 accTime, int32 decTime, Uint16 mode)
{
    int32 accDecTime;
    int32 frq0;

    if (s_AccDecUseRun())
    {
        frq0 = frqRun;
    }
    else
    {
        frq0 = frqTmp;
        frqVFRunRemainder = 0;
    }

    if (frqCurAimOld != frqCurAim)
    {
        frqCurAimOld = frqCurAim;
        bFrqCurAimChg = 1;
    }

    if (frqCurAim == frq0)
    {
        runFlag.bit.accDecStatus = CONST_SPEED;
        frqTmpFrac = frqCurAimFrac;
    }
    else
    {
        if (s_AccDecIsAcc(frq0))
        {
            runFlag.bit.accDecStatus = ACC_SPEED;
            accDecTime = accTime;
            benchFrq = ABS_INT32(frqCurAim);
            if (accTimeOld != accTime)
            {
                bFrqCurAimChg = 1;
            }
        }
        else
        {
            runFlag.bit.accDecStatus = DEC_SPEED;
            accDecTime = decTime;
            if (decTimeOld != decTime)
            {
                bFrqCurAimChg = 1;
            }
        }

        s_BenchOverride();

#if DEBUG_F_S_CURVE_A || DEBUG_F_S_CURVE_A
        mode = s_CurveDowngrade(mode, accDecTime);
#endif

        accDecTime = accDecTime * (timeBench / RUN_CTRL_PERIOD);

        switch (mode)
        {
            case FUNCCODE_accDecSpdCurve_LINE:
                frqLine.aimValue = frqCurAim;
                frqLine.tickerAll = accDecTime;
                frqLine.maxValue = benchFrq;
                frqLine.curValue = frq0;
                frqLine.calc(&frqLine);
                frq0 = frqLine.curValue;
                break;

#if DEBUG_F_S_CURVE_A
            case FUNCCODE_accDecSpdCurve_S_CURVE_A:
                frq0 = SCurveOridinaryCalc(frq0, accDecTime);
                break;
#endif

#if DEBUG_F_S_CURVE_B
            default:
                frq0 = SCurveSpecialCalc(frq0, accDecTime);
                break;
#endif
        }
    }

    bFrqCurAimChg = 0;
    decTimeOld = decTime;
    accTimeOld = accTime;
    frqTmp = frq0;
}

#if DEBUG_F_S_CURVE_A
static int32 s_Fit18(int64 v, Uint16 *qp)
{
    while (v >= 1L << 18)
    {
        v >>= 1;
        (*qp)--;
    }
    return (int32)v;
}

static int32 s_Q15Rescale(int64 v, Uint16 qq)
{
    if (qq >= 15)
    {
        return (int32)(v >> (qq - 15));
    }
    return (int32)(v << (15 - qq));
}
#endif

LOCALF int32 SCurveOridinaryCalc(int32 frq0, int32 accDecTime)
{
#if DEBUG_F_S_CURVE_A
    static enum S_CURVE_A_STATUS sCurveAStatus;
    static int32 ticker;
    static int32 t[3];
    static int32 a[3];
    static Uint16 q[3];
    static int64 remainder;
    int32 delta;
    int64 tmp;
    int32 tmp2;

    if (!accDecTime)
    {
        frq0 = frqCurAim;
        frqTmpFrac = 0;
        return frq0;
    }

    if (bFrqCurAimChg)
    {
        int32 tickerAll;
        Uint16 startTime;
        Uint16 lineTime;
        Uint16 endTime;
        int32 frqAimDelta;
        int64 tmp1;
        int64 tmp3;

        sCurveAStatus = SA_STEP_START_S;
        ticker = 0;
        remainder = 0;

        startTime = funcCode.code.sCurveStartPhaseTime;
        endTime = funcCode.code.sCurveEndPhaseTime;
        lineTime = 2000 - startTime - endTime;
        frqAimDelta = frqCurAim - frq0;

        tmp3 = ((int64)accDecTime * ABS_INT32(frqAimDelta) << 16) / (int32)benchFrq;
        tickerAll = tmp3 >> 16;
        t[0] = ((int64)tickerAll * startTime + 0) / 1000;
        t[2] = ((int64)tickerAll * endTime + 0) / 1000;
        t[1] = tickerAll - (t[0] + t[2]);

        q[0] = 20;
        q[1] = 35;

        a[1] = s_Fit18(((int64)benchFrq * 2000 << q[1]) / ((int64)accDecTime * lineTime), &q[1]);

        tmp1 = ((int64)a[1] * 500 << (q[0] + 16)) / tmp3;

        q[0] += q[1];
        q[2] = q[0];

        a[0] = s_Fit18(tmp1 / startTime, &q[0]);
        a[2] = s_Fit18(tmp1 / endTime, &q[2]);
    }

    if (sCurveAStatus <= SA_STEP_END_S)
    {
        if (ticker >= t[sCurveAStatus])
        {
            ticker = 0;
            sCurveAStatus++;

            while ((t[sCurveAStatus] == 0) && (SA_STEP_OK != sCurveAStatus))
            {
                sCurveAStatus++;
            }

            remainder = 0;
        }

        ticker++;

        switch (sCurveAStatus)
        {
            case SA_STEP_START_S:
                tmp2 = 2 * ticker - 1;
                break;

            case SA_STEP_LINE:
                tmp2 = 1;
                break;

            default:
                tmp2 = 2 * t[2] - 2 * ticker + 1;
                break;
        }

        tmp = (int64)tmp2 * a[sCurveAStatus] + remainder;
        delta = tmp >> q[sCurveAStatus];
        remainder = tmp - ((int64)delta << q[sCurveAStatus]);

        frqTmpFrac = s_Q15Rescale(remainder, q[sCurveAStatus]);

        if (frq0 < frqCurAim)
        {
            frq0 += delta;
            if (frq0 > frqCurAim)
            {
                frq0 = frqCurAim;
            }
        }
        else
        {
            frq0 -= delta;
            frqTmpFrac = -frqTmpFrac;

            if (frq0 < frqCurAim)
            {
                frq0 = frqCurAim;
            }
        }
    }

    if (SA_STEP_OK == sCurveAStatus)
    {
        frq0 = frqCurAim;
    }
#endif
    return frq0;
}

#if DEBUG_F_S_CURVE_B
static void s_TickUp(int32 *ticker, int32 limit, int16 *flag)
{
    (*ticker)++;
    if (*ticker < limit)
    {
        *flag = 1;
    }
}

static void s_TickDown(int32 *ticker, int32 limit, int16 *flag)
{
    (*ticker)--;
    if (*ticker > limit)
    {
        *flag = 1;
    }
}

static void s_SbStep(int32 *delta, int64 *rem, int32 a, int32 b, int32 tAll2,
                     int32 dAim, int16 A, int16 B, int16 Sden)
{
    int64 num;
    int64 den;

    num = ((int64)A * a + tAll2 * B * b) * dAim + *rem;
    den = (int64)tAll2 * tAll2 * Sden;
    *delta = (int32)(num / den);
    *rem = num % den;
}

static void s_SbLine(int32 *delta, int64 *rem, int32 dAim, int32 span)
{
    int64 num;

    num = (int64)dAim + *rem;
    *delta = (int32)(num / span);
    *rem = num % span;
}

static int32 s_SbClamp(int32 frq0, int32 old)
{
    if (((frqCurAim > old) && (frq0 > frqCurAim))
        || ((frqCurAim <= old) && (frq0 < frqCurAim)))
    {
        return frqCurAim;
    }
    return frq0;
}
#endif

LOCALF int32 SCurveSpecialCalc(int32 frq0, int32 accDecTime)
{
#if DEBUG_F_S_CURVE_B
    static enum S_CURVE_B_STATUS sCurveBStatus;
    const int16 m1 = 13848;
    const int16 fm = 11483;

    const int16 A = 21;
    const int16 B = 16;
    const int16 Sden = 30;

    static int32 ticker;
    static int32 tickerAll1;
    static int32 tickerAll2;
    static int32 tickerAll3;
    static int32 inflexionFrq;
    static int32 pointFrq;
    static int64 rem1;
    static int64 remainder;
    static int32 delta;
    static int32 deltaFrqAim;
    Uint16 tmp0;
    int16 flag = 0;
    int16 s;
    int32 tmp;
    int32 frqTmpOld = frq0;
    int32 a;
    int32 b;
    int acc;

    if (!accDecTime)
    {
        frq0 = frqCurAim;
        return frq0;
    }

    acc = (runFlag.bit.accDecStatus == ACC_SPEED);

    if (bFrqCurAimChg)
    {
        int32 src;
        int32 sel;

        src = (frq0 != 0) ? frq0 : frqCurAim;
        if (src > 0)
        {
            inflexionFrq = motorFc.motorPara.elem.ratingFrq;
        }
        else if (src < 0)
        {
            inflexionFrq = -(int32)motorFc.motorPara.elem.ratingFrq;
        }

        pointFrq = ((inflexionFrq * fm) >> 15);

        tickerAll2 = (int32)accDecTime * ABS_INT32(inflexionFrq) / (int32)benchFrq;
        tickerAll1 = (tickerAll2 * m1) >> 15;

        sel = 0;
        if (acc)
        {
            if (ABS_INT32(frqCurAim) > ABS_INT32(inflexionFrq))
            {
                sel = frqCurAim;
            }
        }
        else
        {
            if (ABS_INT32(frq0) > ABS_INT32(inflexionFrq))
            {
                sel = frq0;
            }
        }

        if (sel)
        {
            tmp = ((sel << 13) / inflexionFrq);
            tickerAll3 = ((((tmp * tmp) >> 11) + (5UL << 13)) * tickerAll2 / 9 + (1UL << 12)) >> 13;
        }
        else
        {
            tickerAll3 = tickerAll2;
        }

        if (ABS_INT32(frq0) <= ABS_INT32(pointFrq))
        {
            Uint32 u;

            u = (((((Uint64)B*B) << 30) / (Sden*Sden))
                 + (4*((((Uint64)A) << 30) * frq0) / (Sden * inflexionFrq)));
            tmp0 = qsqrt(u);
            ticker = (((((int32)tmp0 + 3)*Sden - ((int32)B<<15)) * tickerAll2 / A + (1UL << 15)) >> 16);

            remainder = 0;
            deltaFrqAim = inflexionFrq;
            sCurveBStatus = SB_STEP_START_S;
        }
        else if (ABS_INT32(frq0) <= ABS_INT32(inflexionFrq))
        {
            ticker = ((((((frq0 << 15) / inflexionFrq - fm) * 8 + (int32)m1 * 9)) * tickerAll2 / 9
                      + (1UL<<(15-1))) >> (15));

            deltaFrqAim = inflexionFrq - pointFrq;

            sCurveBStatus = SB_STEP_LINE;
        }
        else
        {
            ticker = tickerAll3;
            sCurveBStatus = SB_STEP_END_S;
        }
    }

    switch (sCurveBStatus)
    {
        case SB_STEP_START_S:
            if (acc)
            {
                s = 1;
                s_TickUp(&ticker, tickerAll1, &flag);
            }
            else
            {
                s = -1;
                s_TickDown(&ticker, 0, &flag);
            }

            if (flag)
            {
                a = 2 * ticker - 1;
                b = 1;
                s_SbStep(&delta, &remainder, a, b, tickerAll2, deltaFrqAim, A, B, Sden);
                frq0 += s*delta;
            }
            else
            {
                if (acc)
                {
                    frq0 += s*delta;
                    deltaFrqAim = inflexionFrq - frq0;
                    rem1 = 0;
                    sCurveBStatus = SB_STEP_LINE;
                }
                else
                {
                    sCurveBStatus = SB_STEP_OK;
                }
            }

            break;

        case SB_STEP_LINE:
            if (acc)
            {
                s = 1;
                s_TickUp(&ticker, tickerAll2, &flag);
            }
            else
            {
                s = -1;
                s_TickDown(&ticker, tickerAll1 + 1, &flag);
            }

            if (flag)
            {
                s_SbLine(&delta, &rem1, deltaFrqAim, tickerAll2 - tickerAll1);
                frq0 += s*delta;
            }
            else
            {
                if (acc)
                {
                    frq0 += s*delta;
                    sCurveBStatus = SB_STEP_END_S;
                }
                else
                {
                    frq0 += s*delta;
                    remainder = 0;
                    deltaFrqAim = inflexionFrq;
                    sCurveBStatus = SB_STEP_START_S;

                    ticker = tickerAll1 + 1;
                }
            }

            break;

        case SB_STEP_END_S:
            if (acc)
            {
                s_TickUp(&ticker, tickerAll3, &flag);
            }
            else
            {
                s_TickDown(&ticker, tickerAll2 + 0, &flag);
            }

            if (flag)
            {
                Uint32 u;

                u = ((((int64)9 * ticker - 5 * tickerAll2) << 28) / (9 * tickerAll2));
                tmp0 = qsqrt(u);
                frq0 = (((int32)tmp0 + 3) * inflexionFrq * 3 ) >> (15);
            }
            else
            {
                if (acc)
                {
                    sCurveBStatus = SB_STEP_OK;
                }
                else
                {
                    frq0 = inflexionFrq;
                    deltaFrqAim = inflexionFrq - pointFrq;
                    rem1 = 0;
                    sCurveBStatus = SB_STEP_LINE;

                    ticker = tickerAll2 + 1;
                }
            }
            break;

        default:
            break;
    }

    frq0 = s_SbClamp(frq0, frqTmpOld);

    if (sCurveBStatus == SB_STEP_OK)
    {
        frq0 = frqCurAim;
    }
#endif
    return frq0;
}

void LineChangeCalc(LINE_CHANGE_STRUCT *p)
{
    int32 delta;
    int32 step;

    if (p->curValue == p->aimValue)
    {
        return;
    }

    if (!p->tickerAll)
    {
        p->curValue = p->aimValue;
        return;
    }

    step = (int32)p->maxValue + p->remainder;
    delta = step / p->tickerAll;
    p->remainder = step % p->tickerAll;

    if (p->aimValue > p->curValue)
    {
        p->curValue += delta;
        if (p->curValue > p->aimValue)
        {
            p->curValue = p->aimValue;
        }
    }
    else
    {
        p->curValue -= delta;
        if (p->curValue < p->aimValue)
        {
            p->curValue = p->aimValue;
        }
    }
}
