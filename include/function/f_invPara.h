
#ifndef __F_INV_PARA_H__
#define __F_INV_PARA_H__

#if F_DEBUG_RAM
#define DEBUG_F_MOTOR_POWER_RELATE      0
#define DEBUG_F_INV_TYPE_RELATE         0
#elif 1
#define DEBUG_F_MOTOR_POWER_RELATE      1
#define DEBUG_F_INV_TYPE_RELATE         1
#endif

#define INV_TYPE_VOLTAGE_NUM    6

#define INV_TYPE_VOLTAGE_T380   0
#define INV_TYPE_VOLTAGE_T220   1
#define INV_TYPE_VOLTAGE_S220   2
#define INV_TYPE_VOLTAGE_T480   3
#define INV_TYPE_VOLTAGE_T690   4
#define INV_TYPE_VOLTAGE_T1140  5

#define INV_TYPE_THREE_PHASE_START          8
#define INV_TYPE_BIG_ACC_DEC_START_T380     21
#define INV_TYPE_BIG_ACC_DEC_START_T220     18
#define INV_TYPE_POINT_LIMIT_T380           21
#define INV_TYPE_POINT_LIMIT_T220           18
#define MOTOR_PARA_POINT_POWER_LIMIT        750
#define RATING_SPEED_RPM                    1460

#define GetInvParaPointer(invType)            \
(pInvTypeParaTable[invPara.volLevel] + ((invType) - invTypeLimitTable[invPara.volLevel].lower)) \

#define INV_TYPE_LOWER_LIMIT_T380  8
#define INV_TYPE_UPPER_LIMIT_T380  37
#define INV_TYPE_LENGTH_T380                                    \
(INV_TYPE_UPPER_LIMIT_T380 - INV_TYPE_LOWER_LIMIT_T380 + 1)     \

#define INV_TYPE_LOWER_LIMIT_T220  7
#define INV_TYPE_UPPER_LIMIT_T220  22
#define INV_TYPE_LENGTH_T220                                    \
(INV_TYPE_UPPER_LIMIT_T220 - INV_TYPE_LOWER_LIMIT_T220 + 1)     \

#define INV_TYPE_LOWER_LIMIT_S220  1
#define INV_TYPE_UPPER_LIMIT_S220  7
#define INV_TYPE_LENGTH_S220                                    \
(INV_TYPE_UPPER_LIMIT_S220 - INV_TYPE_LOWER_LIMIT_S220 + 1)     \

#define INV_TYPE_LOWER_LIMIT_T480  8
#define INV_TYPE_UPPER_LIMIT_T480  34
#define INV_TYPE_LENGTH_T480                                    \
(INV_TYPE_UPPER_LIMIT_T480 - INV_TYPE_LOWER_LIMIT_T480 + 1)     \

#define INV_TYPE_LOWER_LIMIT_T690  21
#define INV_TYPE_UPPER_LIMIT_T690  36
#define INV_TYPE_LENGTH_T690                                    \
(INV_TYPE_UPPER_LIMIT_T690 - INV_TYPE_LOWER_LIMIT_T690 + 1)     \

#define INV_TYPE_LOWER_LIMIT_T1140  19
#define INV_TYPE_UPPER_LIMIT_T1140  37
#define INV_TYPE_LENGTH_T1140                                   \
(INV_TYPE_UPPER_LIMIT_T1140 - INV_TYPE_LOWER_LIMIT_T1140 + 1)   \

struct INV_TYPE_RELATED_PARA_STRUCT
{
    Uint16 carrierFrq;

    Uint16 ratingPower;
    Uint16 ratingCurrent;
    Uint16 statorResistance;
    Uint16 rotorResistance;
    Uint16 leakInductance;
    Uint16 mutualInductance;
    Uint16 zeroLoadCurrent;

    Uint16 antiVibrateGain;
};
typedef union
{
    Uint16 all[sizeof(struct INV_TYPE_RELATED_PARA_STRUCT)];

    struct INV_TYPE_RELATED_PARA_STRUCT elem;
} INV_TYPE_RELATED_PARA;

struct INV_TYPE_RELATED_PARA_ALL
{
    INV_TYPE_RELATED_PARA t380[INV_TYPE_LENGTH_T380];
    INV_TYPE_RELATED_PARA t220[INV_TYPE_LENGTH_T220];
    INV_TYPE_RELATED_PARA s220[INV_TYPE_LENGTH_S220];
    INV_TYPE_RELATED_PARA t480[INV_TYPE_LENGTH_T480];
    INV_TYPE_RELATED_PARA t690[INV_TYPE_LENGTH_T690];
    INV_TYPE_RELATED_PARA t1140[INV_TYPE_LENGTH_T1140];
};

struct INV_TYPE_LIMIT
{
    Uint16 vol;

    Uint16 lower;
    Uint16 upper;
};
extern const struct INV_TYPE_LIMIT invTypeLimitTable[];

extern const INV_TYPE_RELATED_PARA * pInvTypeParaTable[];

LOCALD void InverterTypeRelatedNoMotorParaDeal(Uint16 invType);
Uint16 ValidateInvType(void);
void MotorPowerRelatedParaDeal(Uint16 power, Uint16 motorSn4Power);
void InverterTypeRelatedParaDeal(void);
void InverterTypeRelatedNoMotorParaSaveDeal(void);

#endif
