
#include "f_main.h"
#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_io.h"
#include "f_menu.h"
#include "f_invPara.h"

#if F_DEBUG_RAM
#define DEBUG_F_MANY_MOTOR     0
#elif 1
#define DEBUG_F_MANY_MOTOR     1
#endif

Uint16 polePair;

#if DEBUG_F_MANY_MOTOR

#define MOTOR3_INDEX_N_INC_TO_MOTOR2                        \
(GetCodeIndex(funcCode.code.motorFcM3.motorCtrlMode) -      \
GetCodeIndex(funcCode.code.motorFcM2.motorCtrlMode))        \

#define MOTOR4_INDEX_N_INC_TO_MOTOR2                        \
(GetCodeIndex(funcCode.code.motorFcM4.motorCtrlMode) -      \
GetCodeIndex(funcCode.code.motorFcM2.motorCtrlMode))        \

const Uint16 motorSnIndexNIncToMotor2[3] =
{
    0,
    MOTOR3_INDEX_N_INC_TO_MOTOR2,
    MOTOR4_INDEX_N_INC_TO_MOTOR2,
};

extern const int16 decNumber[];
void UpdateMotorPara(void)
{
    Uint16 tmp;

    if (diSelectFunc.f2.bit.motorSnDi)
    {
        motorSn = (enum MOTOR_SN)diFunc.f2.bit.motorSnDi;
    }
    else
    {
        motorSn = funcCode.code.motorSn;
    }

    if (MOTOR_SN_1 == motorSn)
    {
        memcpy(&motorFc.motorPara, &funcCode.code.motorParaM1, sizeof(struct MOTOR_PARA_STRUCT));
        memcpy(&motorFc.pgPara,    &funcCode.code.pgParaM1, sizeof(struct PG_PARA_STRUCT));
        memcpy(&motorFc.vcPara, &funcCode.code.vcParaM1, sizeof(struct VC_PARA));
        motorFc.motorCtrlMode = funcCode.code.motorCtrlMode;
        motorFc.accDecTimeMotor = 0;
        motorFc.torqueBoost = funcCode.code.torqueBoost;

        motorFc.antiVibrateGain = funcCode.code.antiVibrateGain;
    }
    else
    {
		 memcpy(&motorFc, &funcCode.all[GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.motorType) + motorSnIndexNIncToMotor2[motorSn - MOTOR_SN_2]], sizeof(struct MOTOR_FC));
    }

    if (errorCode == ERROR_ENCODER)
    {
        motorFc.motorCtrlMode = FUNCCODE_motorCtrlMode_VF;
    }

#if 1

    tmp = motorFc.motorPara.elem.ratingSpeed;
    polePair = (60UL * motorFc.motorPara.elem.ratingFrq / decNumber[funcCode.code.frqPoint] + tmp / 2) / tmp;
#endif
}

#elif 1

void UpdateMotorPara(void)
{
    motorFc.motorCtrlMode = funcCode.code.motorCtrlMode;
}

#endif
