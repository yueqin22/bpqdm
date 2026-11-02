
#include "f_main.h"
#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_io.h"

#if F_DEBUG_RAM

#define DEBUG_F_VF          0

#elif 1

#if !DEBUG_F_POSITION_CTRL
#define DEBUG_F_VF          1
#elif 1
#define DEBUG_F_VF          0
#endif

#endif

Uint16 vfSeprateVolAim;
Uint16 vfSeparateVol;

#if DEBUG_F_VF

#define VF_AI_SCALE_SHIFT       15
#define VF_COMM_SCALE_BASE      10000
#define VF_HALF_SEPARATE_NUM    2
#define VF_ACC_TICKS            (TIME_UNIT_VF_VOL_ACC_TIME / VF_CALC_PERIOD)

LINE_CHANGE_STRUCT vfSeprateVolAccLine = LINE_CHANGE_STRTUCT_DEFALUTS;
extern int32 FrqPlcSetDeal(void);

void vfSeparateDeal(void)
{
    int16 srcValue;

    if (((funcCode.code.vfCurve != FUNCCODE_vfCurve_ALL_SEPARATE)
        && (funcCode.code.vfCurve != FUNCCODE_vfCurve_HALF_SEPARATE))
        || (motorFc.motorCtrlMode != FUNCCODE_motorCtrlMode_VF))
    {
        vfSeparateVol = 0;
        vfSeprateVolAim = 0;
        return;
    }

    switch (funcCode.code.vfVoltageSrc)
    {
        case FUNCCODE_vfVoltageSrc_FC:
            vfSeprateVolAim = funcCode.code.vfVoltageDigtalSet;
            break;

        case FUNCCODE_vfVoltageSrc_AI1:
        case FUNCCODE_vfVoltageSrc_AI2:
        case FUNCCODE_vfVoltageSrc_AI3:
            srcValue = aiDeal[funcCode.code.vfVoltageSrc - FUNCCODE_vfVoltageSrc_AI1].set;
            vfSeprateVolAim = (int32)srcValue * funcCode.code.motorParaM1.elem.ratingVoltage
                >> VF_AI_SCALE_SHIFT;
            break;

        case FUNCCODE_vfVoltageSrc_PULSE:
            vfSeprateVolAim = (int32)pulseInSet * funcCode.code.motorParaM1.elem.ratingVoltage
                >> VF_AI_SCALE_SHIFT;
            break;

        case FUNCCODE_vfVoltageSrc_MULTI_SET:
            vfSeprateVolAim = (int32)UpdateMultiSetFrq(diFunc.f1.bit.multiSet)
                * funcCode.code.motorParaM1.elem.ratingVoltage / maxFrq;
            break;

        case FUNCCODE_vfVoltageSrc_PLC:
            vfSeprateVolAim = (int32)FrqPlcSetDeal()
                * funcCode.code.motorParaM1.elem.ratingVoltage / maxFrq;
            break;

        case FUNCCODE_vfVoltageSrc_PID:
            vfSeprateVolAim = (int32)FrqPidSetDeal()
                * funcCode.code.motorParaM1.elem.ratingVoltage / maxFrq;
            break;

        case FUNCCODE_vfVoltageSrc_COMM:
            vfSeprateVolAim = (int32)(int16)funcCode.code.frqComm
                * funcCode.code.motorParaM1.elem.ratingVoltage / VF_COMM_SCALE_BASE;
            break;

        default:
            break;
    }

    vfSeprateVolAim = ABS_INT16((int16)vfSeprateVolAim);

    if (FUNCCODE_vfCurve_HALF_SEPARATE == funcCode.code.vfCurve)
    {
        vfSeprateVolAim = ((Uint32)VF_HALF_SEPARATE_NUM * ABS_INT32(frq) * vfSeprateVolAim)
            / motorFc.motorPara.elem.ratingFrq;
    }

    if (!runFlag.bit.run)
    {
        vfSeparateVol = 0;
        return;
    }

    if (vfSeprateVolAim > motorFc.motorPara.elem.ratingVoltage)
    {
        vfSeprateVolAim = motorFc.motorPara.elem.ratingVoltage;
    }

    vfSeprateVolAccLine.aimValue = vfSeprateVolAim;
    vfSeprateVolAccLine.tickerAll = (Uint32)funcCode.code.vfVoltageAccTime * VF_ACC_TICKS;
    vfSeprateVolAccLine.maxValue = motorFc.motorPara.elem.ratingVoltage;
    vfSeprateVolAccLine.curValue = vfSeparateVol;
    vfSeprateVolAccLine.calc(&vfSeprateVolAccLine);
    vfSeparateVol = vfSeprateVolAccLine.curValue;
}

#elif 1

void vfSeparateDeal(void){}

#endif
