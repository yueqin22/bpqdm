
#include "f_menu.h"
#include "f_eeprom.h"
#include "f_comm.h"
#include "f_invPara.h"

#if F_DEBUG_RAM
#define DEBUG_F_TUNE                0
#elif 1
#define DEBUG_F_TUNE                1
#endif

struct INV_PARA invPara;

const Uint16 ovVoltageInitValue[INV_TYPE_VOLTAGE_NUM] = {8100,4000,4000,8900,13000,20000};

const struct INV_TYPE_LIMIT invTypeLimitTable[INV_TYPE_VOLTAGE_NUM] =
{
    {380,  INV_TYPE_LOWER_LIMIT_T380, INV_TYPE_UPPER_LIMIT_T380},
    {220,  INV_TYPE_LOWER_LIMIT_T220, INV_TYPE_UPPER_LIMIT_T220},
    {220,  INV_TYPE_LOWER_LIMIT_S220, INV_TYPE_UPPER_LIMIT_S220},
    {480,  INV_TYPE_LOWER_LIMIT_T480, INV_TYPE_UPPER_LIMIT_T480},
    {690,  INV_TYPE_LOWER_LIMIT_T690, INV_TYPE_UPPER_LIMIT_T690},
    {1140,  INV_TYPE_LOWER_LIMIT_T1140, INV_TYPE_UPPER_LIMIT_T1140},
};

const Uint16 motor1ParaIndex[] =
{

    GetCodeIndex(funcCode.code.motorParaM1.elem.ratingPower),
    GetCodeIndex(funcCode.code.motorParaM1.elem.ratingCurrent),

    GetCodeIndex(funcCode.code.motorParaM1.elem.statorResistance),
    GetCodeIndex(funcCode.code.motorParaM1.elem.rotorResistance),
    GetCodeIndex(funcCode.code.motorParaM1.elem.leakInductance),
    GetCodeIndex(funcCode.code.motorParaM1.elem.mutualInductance),
    GetCodeIndex(funcCode.code.motorParaM1.elem.zeroLoadCurrent),

    GetCodeIndex(funcCode.code.motorParaM1.elem.pmsmRs),
    GetCodeIndex(funcCode.code.motorParaM1.elem.pmsmLd),
    GetCodeIndex(funcCode.code.motorParaM1.elem.pmsmLq),
    GetCodeIndex(funcCode.code.motorParaM1.elem.pmsmRsLdUnit),
    GetCodeIndex(funcCode.code.motorParaM1.elem.pmsmCoeff),

    GetCodeIndex(funcCode.code.vcParaM1.mAcrKp),
    GetCodeIndex(funcCode.code.vcParaM1.mAcrKi),
    GetCodeIndex(funcCode.code.vcParaM1.tAcrKp),
    GetCodeIndex(funcCode.code.vcParaM1.tAcrKi),
    GetCodeIndex(funcCode.code.motorParaM1.elem.pmsmCheckTime),

    GetCodeIndex(funcCode.code.pgParaM1.elem.enCoderDir),
    GetCodeIndex(funcCode.code.pgParaM1.elem.enCoderAngle),
    GetCodeIndex(funcCode.code.uvGainWarp),
    GetCodeIndex(funcCode.code.pgParaM1.elem.uvwSignDir),
    GetCodeIndex(funcCode.code.pgParaM1.elem.uvwSignAngle),
};

#if (DEBUG_F_MOTOR_POWER_RELATE || DEBUG_F_INV_TYPE_RELATE)

const INV_TYPE_RELATED_PARA invTypeParaTableT380[INV_TYPE_LENGTH_T380] =
{

    { 60,   7,     210,    9134,   4281,   3799,  4077,  155,   0,          },
    { 60,   15,    380,    4421,   2813,   2065,  3457,  187,   0,          },
    { 60,   22,    510,    2706,   1543,   1011,  2450,  267,   0,          },
    { 60,   37,    900,    1204,   908,    528,   1586,  424,   0,          },
    { 60,   55,    1300,   804,    708,    475,   1206,  518,   0,          },
    { 60,   75,    1700,   610,    584,    430,   1069,  630,   0,          },
    { 60,   110,   2500,   410,    220,    260,   776,   890,   0,          },
    { 40,   150,   3200,   273,    200,    220,   578,   1160,  0,          },
    { 40,   185,   3700,   200,    130,    150,   537,   1280,  0,          },
    { 40,   220,   4500,   152,    138,    115,   342,   1965,  0,          },
    { 40,   300,   6000,   110,    98,     100,   322,   2070,  0,          },
    { 40,   370,   7500,   69,     74,     60,    308,   2190,  20,         },
    { 40,   450,   9000,   61,     66,     52,    223,   3020,  20,         },
    { 30,   550,   11000,  50,     60,     41,    183,   3690,  30,         },

    { 20,   750,   1520,   300,    460,    290,   1640,  420 ,  30,         },
    { 20,   900,   1760,   220,    380,    200,   1400,  490 ,  40,         },
    { 20,   1100,  2100,   120,    320,    150,   1200,  580 ,  40,         },
    { 20,   1320,  2530,   100,    270,    130,   1060,  680 ,  40,         },
    { 20,   1600,  3040,   70,     240,    110,   800,   815 ,  50,         },
    { 20,   2000,  3800,   40,     210,    90,    600,   950 ,  50,         },
    { 20,   2200,  4260,   28,     190,    75,    450,   1050,  50,         },
    { 20,   2500,  4650,   20,     170,    55,    350,   1200,  50,         },
    { 20,   2800,  5200,   14,     150,    45,    280,   1300,  50,         },
    { 20,   3150,  5850,   10,     120,    30,    200,   1500,  50,         },
    { 20,   3550,  6500,   8,      100,    22,    140,   1640,  50,         },
    { 20,   4000,  7250,   6,      85,     17,    100,   1750,  50,         },
    { 20,   4500,  8200,   4,      63,     13,    70,    1900,  50,         },

    { 20,   5000,  7250,   4,      63,     13,    70,    1900,  50,         },
    { 20,   5500,  8200,   4,      63,     13,    70,    1900,  50,         },
    { 20,   6300,  9360,   4,      63,     13,    70,    1900,  50,         },

};

const INV_TYPE_RELATED_PARA invTypeParaTableT220[INV_TYPE_LENGTH_T220] =
{

    { 60,   4,     210,    9134,   4281,   3799,  4077,  155,   0,          },
    { 60,   7,     380,    4421,   2813,   2065,  3457,  187,   0,          },
    { 60,   11,    510,    2706,   1543,   1011,  2450,  267,   0,          },
    { 60,   22,    900,    1204,   908,    528,   1586,  424,   0,          },
    { 60,   37,    1300,   804,    708,    475,   1206,  518,   0,          },
    { 60,   55,    2500,   410,    220,    260,   776,   890,   0,          },
    { 40,   75,    3200,   273,    200,    220,   578,   1160,  0,          },
    { 40,   110,   4500,   152,    138,    115,   342,   1965,  0,          },
    { 40,   150,   6000,   110,    98,     100,   322,   2070,  0,          },
    { 40,   185,   7500,   69,     74,     60,    308,   2190,  20,         },
    { 40,   220,   9000,   61,     66,     52,    223,   3020,  20,         },
    { 30,   300,   11000,  50,     60,     41,    183,   3690,  30,         },

    { 20,   370,   1520,   300,    460,    290,   1640,  420 ,  30,         },
    { 20,   450,   1760,   220,    380,    200,   1400,  490 ,  40,         },
    { 20,   550,   2100,   120,    320,    150,   1200,  580 ,  40,         },
    { 20,   750,   3040,   70,     240,    110,   800,   815 ,  50,         },

};

const INV_TYPE_RELATED_PARA invTypeParaTableS220[INV_TYPE_LENGTH_S220] =
{

    { 60,   2,     170,    15330,  10440,  4700,  3720,  90,    0,          },
    { 60,   5,     230,    4007,   2462,   1358,  1821,  202,   0,          },
    { 60,   7,     400,    2963,   1697,   1008,  1472,  255,   0,          },
    { 60,   15,    700,    1534,   1043,   628,   1308,  292,   0,          },
    { 60,   22,    960,    1025,   680,    340,   885,   450,   0,          },
    { 60,   37,    1700,   735,    450,    268,   598,   637,   0,          },
    { 60,   55,    2500,   435,    350,    180,   350,   1100,  0,          },
};

const INV_TYPE_RELATED_PARA invTypeParaTableT480[INV_TYPE_LENGTH_T480] =
{

    { 60,   7,     210,    9134,   4281,   3799,  4077,  155,   0,          },
    { 60,   15,    380,    4421,   2813,   2065,  3457,  187,   0,          },
    { 60,   22,    510,    2706,   1543,   1011,  2450,  267,   0,          },
    { 60,   37,    900,    1204,   908,    528,   1586,  424,   0,          },
    { 60,   55,    1300,   804,    708,    475,   1206,  518,   0,          },
    { 60,   75,    1700,   610,    584,    430,   1069,  630,   0,          },
    { 60,   110,   2500,   410,    220,    260,   776,   890,   0,          },
    { 40,   150,   3200,   273,    200,    220,   578,   1160,  0,          },
    { 40,   185,   3700,   200,    130,    150,   537,   1280,  0,          },
    { 40,   220,   4500,   152,    138,    115,   342,   1965,  0,          },
    { 40,   300,   6000,   110,    98,     100,   322,   2070,  0,          },
    { 40,   370,   7500,   69,     74,     60,    308,   2190,  20,         },
    { 40,   450,   9000,   61,     66,     52,    223,   3020,  20,         },
    { 30,   550,   11000,  50,     60,     41,    183,   3690,  30,         },

    { 20,   750,   1520,   300,    460,    290,   1640,  420 ,  30,         },
    { 20,   900,   1760,   220,    380,    200,   1400,  490 ,  40,         },
    { 20,   1100,  2100,   120,    320,    150,   1200,  580 ,  40,         },
    { 20,   1320,  2530,   100,    270,    130,   1060,  680 ,  40,         },
    { 20,   1600,  3040,   70,     240,    110,   800,   815 ,  50,         },
    { 20,   2000,  3800,   40,     210,    90,    600,   950 ,  50,         },
    { 20,   2200,  4260,   28,     190,    75,    450,   1050,  50,         },
    { 20,   2500,  4650,   20,     170,    55,    350,   1200,  50,         },
    { 20,   2800,  5200,   14,     150,    45,    280,   1300,  50,         },
    { 20,   3150,  5850,   10,     120,    30,    200,   1500,  50,         },
    { 20,   3550,  6500,   8,      100,    22,    140,   1640,  50,         },
    { 20,   4000,  7250,   6,      85,     17,    100,   1750,  50,         },
    { 20,   4500,  8200,   4,      63,     13,    70,    1900,  50,         },
#if 0
    { 20,   5000,  7250,   4,      63,     13,    70,    1900,  50,         },
    { 20,   5500,  8200,   4,      63,     13,    70,    1900,  50,         },
#endif
};

const INV_TYPE_RELATED_PARA invTypeParaTableT690[INV_TYPE_LENGTH_T690] =
{

#if 0
    { 60,   7,     80,     9134,   4281,   3799,  4077,  155,   0,          },
    { 60,   15,    160,    4000,   2653,   2130,  3622,  187,   0,          },
    { 60,   22,    240,    2706,   1543,   1011,  2450,  267,   0,          },
    { 60,   37,    400,    1050,   1000,   750,   1870,  380,   0,          },
    { 60,   55,    600,    680,    640,    600,   1250,  518,   0,          },
    { 60,   75,    800,    610,    584,    430,   1069,  630,   0,          },
    { 40,   110,   1200,   410,    220,    260,   776,   890,   0,          },
    { 40,   150,   1600,   273,    200,    220,   578,   1160,  0,          },
    { 40,   185,   2000,   200,    130,    150,   537,   1280,  0,          },
    { 40,   220,   2400,   152,    138,    115,   342,   1965,  0,          },
    { 40,   300,   3300,   110,    98,     100,   322,   2070,  0,          },
    { 40,   370,   4100,   69,     74,     60,    308,   2190,  20,         },
    { 40,   450,   5000,   61,     66,     52,    223,   3020,  20,         },
#endif
    { 20,   550,   6200,   50,     60,     41,    183,   3690,  30,         },

    { 20,   750,   850,    300,    460,    290,   1640,  420,   30,         },
    { 20,   900,   1020,   220,    380,    200,   1400,  490,   40,         },
    { 20,   1100,  1250,   553,    862,    620,   4106,  302,   40,         },
    { 20,   1320,  1500,   100,    270,    130,   1060,  680,   40,         },
    { 20,   1600,  1750,   70,     240,    110,   800,   815,   50,         },
    { 20,   2000,  2150,   40,     210,    90,    600,   950,   50,         },
    { 20,   2200,  2450,   28,     190,    75,    450,   1050,  50,         },
    { 20,   2500,  2600,   20,     170,    55,    350,   1200,  50,         },
    { 20,   2800,  2990,   14,     150,    45,    280,   1300,  50,         },
    { 20,   3150,  3300,   10,     120,    30,    200,   1500,  50,         },
    { 20,   3550,  3740,   8,      100,    22,    140,   1640,  50,         },
    { 20,   4000,  4100,   6,      85,     17,    100,   1750,  50,         },
    { 20,   4500,  4650,   4,      63,     13,    70,    1900,  50,         },
    { 20,   5000,  5500,   4,      63,     13,    70,    1900,  50,         },
    { 20,   5500,  5900,   4,      63,     13,    70,    1900,  50,         },
};

const INV_TYPE_RELATED_PARA invTypeParaTableT1140[INV_TYPE_LENGTH_T1140] =
{

    { 10,  370,  2500,    69,     74,     60,    308,   2190,  20,         },
    { 10,   450,  3000,    61,     66,     52,    223,   3020,  20,         },
    { 10,   550,  3700,    50,     60,     41,    183,   3690,  30,         },

    { 10,   750,   500,    300,    460,    290,   1640,  420,   30,         },
    { 10,   900,   590,    220,    380,    200,   1400,  490,   40,         },
    { 10,   1100,  700,    553,    862,    620,   4106,  302,   40,         },
    { 10,   1320,  910,    100,    270,    130,   1060,  680,   40,         },
    { 10,   1600,  1060,   70,     240,    110,   800,   815,   50,         },
    { 10,   2000,  1300,   40,     210,    90,    600,   950,   50,         },
    { 10,   2200,  1480,   28,     190,    75,    450,   1050,  50,         },
    { 10,   2500,  1570,   20,     170,    55,    350,   1200,  50,         },
    { 10,   2800,  1810,   14,     150,    45,    280,   1300,  50,         },
    { 10,   3150,  2000,   10,     120,    30,    200,   1500,  50,         },
    { 10,   3550,  2260,   8,      100,    22,    140,   1640,  50,         },
    { 10,   4000,  2480,   6,      85,     17,    100,   1750,  50,         },
    { 10,   4500,  2810,   4,      63,     13,    70,    1900,  50,         },
    { 10,   5000,  3330,   4,      63,     13,    70,    1900,  50,         },
    { 10,   5600,  3480,   4,      63,     13,    70,    1900,  50,         },
    { 10,   6300,  4000,   4,      63,     13,    70,    1900,  50,         },
};

const INV_TYPE_RELATED_PARA * pInvTypeParaTable[INV_TYPE_VOLTAGE_NUM] =
{
    invTypeParaTableT380,
    invTypeParaTableT220,
    invTypeParaTableS220,
    invTypeParaTableT480,
    invTypeParaTableT690,
    invTypeParaTableT1140,
};

const Uint16 INV_TYPE_POINT_LIMIT_TABLE[INV_TYPE_VOLTAGE_NUM] =
{
    INV_TYPE_POINT_LIMIT_T380,
    INV_TYPE_POINT_LIMIT_T220,
    INV_TYPE_POINT_LIMIT_T380,
    INV_TYPE_POINT_LIMIT_T380,
    INV_TYPE_POINT_LIMIT_T380,
    INV_TYPE_POINT_LIMIT_T380,
};

const Uint16 INV_TYPE_BIG_ACC_DEC_START_TABLE[INV_TYPE_VOLTAGE_NUM] =
{
    INV_TYPE_BIG_ACC_DEC_START_T380,
    INV_TYPE_BIG_ACC_DEC_START_T220,
    INV_TYPE_BIG_ACC_DEC_START_T380,
    INV_TYPE_BIG_ACC_DEC_START_T380,
    INV_TYPE_BIG_ACC_DEC_START_T380,
    INV_TYPE_BIG_ACC_DEC_START_T380,
};

#endif

#define MOTOR2_INDEX_INC_TO_MOTOR1                          \
(GetCodeIndex(funcCode.code.motorFcM2.motorPara.all[0]) -    \
GetCodeIndex(funcCode.code.motorParaM1.all[0]))              \

#define MOTOR3_INDEX_INC_TO_MOTOR1                          \
(GetCodeIndex(funcCode.code.motorFcM3.motorPara.all[0]) -    \
GetCodeIndex(funcCode.code.motorParaM1.all[0]))              \

#define MOTOR4_INDEX_INC_TO_MOTOR1                          \
(GetCodeIndex(funcCode.code.motorFcM4.motorPara.all[0]) -    \
GetCodeIndex(funcCode.code.motorParaM1.all[0]))              \

const Uint16 motorSnIndexIncToMotor1[4] =
{
    0,
    MOTOR2_INDEX_INC_TO_MOTOR1,
    MOTOR3_INDEX_INC_TO_MOTOR1,
    MOTOR4_INDEX_INC_TO_MOTOR1,
};

void InverterTypeRelatedParaDeal(void)
{
#if DEBUG_F_INV_TYPE_RELATE
    Uint16 ratingFrq;
    Uint16 invType;
    int16 i;

    UpdateInvType();
    invType = invPara.type;

    for (i = 6; i >= 0; i--)
    {
        Uint16 index1;
        Uint16 index2;
        Uint16 index3;
        Uint16 index4;
        Uint16 tmp;

        index1 = motor1ParaIndex[i];
        index2 = motor1ParaIndex[i] + MOTOR2_INDEX_INC_TO_MOTOR1;
        index3 = motor1ParaIndex[i] + MOTOR3_INDEX_INC_TO_MOTOR1;
        index4 = motor1ParaIndex[i] + MOTOR4_INDEX_INC_TO_MOTOR1;

        tmp = GetInvParaPointer(invType)->all[i+1];

        funcCode.all[index1] = tmp;
        funcCode.all[index2] = tmp;
        funcCode.all[index3] = tmp;
        funcCode.all[index4] = tmp;

        SaveOneFuncCode(index1);
        SaveOneFuncCode(index2);
        SaveOneFuncCode(index3);
        SaveOneFuncCode(index4);
    }

    ratingFrq = RATING_FRQ_INIT_0 * decNumber[funcCode.code.frqPoint];

    funcCode.code.motorParaM1.elem.ratingVoltage = invPara.ratingVoltage;
    funcCode.code.motorParaM1.elem.ratingFrq = ratingFrq;
    funcCode.code.motorParaM1.elem.ratingSpeed = RATING_SPEED_RPM;
    SaveOneFuncCode(GetCodeIndex(funcCode.code.motorParaM1.elem.ratingVoltage));
    SaveOneFuncCode(GetCodeIndex(funcCode.code.motorParaM1.elem.ratingFrq));
    SaveOneFuncCode(GetCodeIndex(funcCode.code.motorParaM1.elem.ratingSpeed));

    funcCode.code.motorFcM2.motorPara.elem.ratingVoltage = invPara.ratingVoltage;
    funcCode.code.motorFcM2.motorPara.elem.ratingFrq = ratingFrq;
    funcCode.code.motorFcM2.motorPara.elem.ratingSpeed = RATING_SPEED_RPM;
    SaveOneFuncCode(GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.ratingVoltage));
    SaveOneFuncCode(GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.ratingFrq));
    SaveOneFuncCode(GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.ratingSpeed));

    funcCode.code.motorFcM3.motorPara.elem.ratingVoltage = invPara.ratingVoltage;
    funcCode.code.motorFcM3.motorPara.elem.ratingFrq = ratingFrq;
    funcCode.code.motorFcM3.motorPara.elem.ratingSpeed = RATING_SPEED_RPM;
    SaveOneFuncCode(GetCodeIndex(funcCode.code.motorFcM3.motorPara.elem.ratingVoltage));
    SaveOneFuncCode(GetCodeIndex(funcCode.code.motorFcM3.motorPara.elem.ratingFrq));
    SaveOneFuncCode(GetCodeIndex(funcCode.code.motorFcM3.motorPara.elem.ratingSpeed));

    funcCode.code.motorFcM4.motorPara.elem.ratingVoltage = invPara.ratingVoltage;
    funcCode.code.motorFcM4.motorPara.elem.ratingFrq = ratingFrq;
    funcCode.code.motorFcM4.motorPara.elem.ratingSpeed = RATING_SPEED_RPM;
    SaveOneFuncCode(GetCodeIndex(funcCode.code.motorFcM4.motorPara.elem.ratingVoltage));
    SaveOneFuncCode(GetCodeIndex(funcCode.code.motorFcM4.motorPara.elem.ratingFrq));
    SaveOneFuncCode(GetCodeIndex(funcCode.code.motorFcM4.motorPara.elem.ratingSpeed));

    funcCode.code.inverterPower = funcCode.code.motorParaM1.elem.ratingPower;
    SaveOneFuncCode(GetCodeIndex(funcCode.code.inverterPower));

    InverterTypeRelatedNoMotorParaDeal(invType);

    InverterTypeRelatedNoMotorParaSaveDeal();
#endif
}

LOCALF void InverterTypeRelatedNoMotorParaDeal(Uint16 invType)
{
#if DEBUG_F_INV_TYPE_RELATE
    Uint16 accDecInit;
    if (invType < invPara.bitAccDecStart)
    {
        accDecInit = ACC_DEC_T_INIT1;
    }
    else
    {
        accDecInit = ACC_DEC_T_INIT2;
    }

    funcCode.all[ACC_TIME1_INDEX] = accDecInit;
    funcCode.all[DEC_TIME1_INDEX] = accDecInit;
    funcCode.all[ACC_TIME2_INDEX] = accDecInit;
    funcCode.all[DEC_TIME2_INDEX] = accDecInit;
    funcCode.all[ACC_TIME3_INDEX] = accDecInit;
    funcCode.all[DEC_TIME3_INDEX] = accDecInit;
    funcCode.all[ACC_TIME4_INDEX] = accDecInit;
    funcCode.all[DEC_TIME4_INDEX] = accDecInit;

    funcCode.all[CARRIER_FRQ_INDEX] = GetInvParaPointer(invType)->elem.carrierFrq;

    funcCode.all[ANTI_VIBRATE_GAIN_INDEX] = GetInvParaPointer(invType)->elem.antiVibrateGain;
    funcCode.code.motorFcM2.antiVibrateGain = funcCode.code.antiVibrateGain;
    funcCode.code.motorFcM3.antiVibrateGain = funcCode.code.antiVibrateGain;
    funcCode.code.motorFcM4.antiVibrateGain = funcCode.code.antiVibrateGain;

    funcCode.all[TORQUE_BOOST_INDEX] = TorqueBoostDeal(funcCode.code.motorParaM1.elem.ratingPower);
    funcCode.code.motorFcM2.torqueBoost = funcCode.code.torqueBoost;
    funcCode.code.motorFcM3.torqueBoost = funcCode.code.torqueBoost;
    funcCode.code.motorFcM4.torqueBoost = funcCode.code.torqueBoost;

    funcCode.code.ovPointSet = ovVoltageInitValue[invPara.volLevel];
#endif
}

void InverterTypeRelatedNoMotorParaSaveDeal(void)
{
#if DEBUG_F_INV_TYPE_RELATE
    SaveOneFuncCode(ACC_TIME1_INDEX);
    SaveOneFuncCode(DEC_TIME1_INDEX);
    SaveOneFuncCode(ACC_TIME2_INDEX);
    SaveOneFuncCode(DEC_TIME2_INDEX);
    SaveOneFuncCode(ACC_TIME3_INDEX);
    SaveOneFuncCode(DEC_TIME3_INDEX);
    SaveOneFuncCode(ACC_TIME4_INDEX);
    SaveOneFuncCode(DEC_TIME4_INDEX);

    SaveOneFuncCode(CARRIER_FRQ_INDEX);

    SaveOneFuncCode(ANTI_VIBRATE_GAIN_INDEX);
    SaveOneFuncCode(GetCodeIndex(funcCode.code.motorFcM2.antiVibrateGain));
    SaveOneFuncCode(GetCodeIndex(funcCode.code.motorFcM3.antiVibrateGain));
    SaveOneFuncCode(GetCodeIndex(funcCode.code.motorFcM4.antiVibrateGain));

    SaveOneFuncCode(TORQUE_BOOST_INDEX);
    SaveOneFuncCode(GetCodeIndex(funcCode.code.motorFcM2.torqueBoost));
    SaveOneFuncCode(GetCodeIndex(funcCode.code.motorFcM3.torqueBoost));
    SaveOneFuncCode(GetCodeIndex(funcCode.code.motorFcM4.torqueBoost));
    SaveOneFuncCode(GetCodeIndex(funcCode.code.ovPointSet));
#endif
}

void MotorPowerRelatedParaDeal(Uint16 power, Uint16 motorSn4Power)
{
#if DEBUG_F_MOTOR_POWER_RELATE
    Uint16 i;
    Uint32 flag;
    Uint16 start;
    Uint16 end;
    Uint16 invTypeOfPower;
    int16 j;
    Uint16 dataPointFormat[6] = {1,1,1,1,1,1};

    start = invTypeLimitTable[invPara.volLevel].lower;
    end = invTypeLimitTable[invPara.volLevel].upper;

    for (i = start; i <= end; i++)
    {
        if (GetInvParaPointer(i)->elem.ratingPower >= power)
            break;
    }

    invTypeOfPower = i;
    if (end < i)
    {
        invTypeOfPower--;
    }
    else if (start < i)
    {
        if (power - GetInvParaPointer(i-1)->elem.ratingPower
            < GetInvParaPointer(i)->elem.ratingPower - power)
        {
            invTypeOfPower--;
        }
    }

    if (MOTOR_SN_1 == motorSn4Power)
    {
        funcCode.code.torqueBoost = TorqueBoostDeal(power);
        SaveOneFuncCode(GetCodeIndex(funcCode.code.torqueBoost));
    }
    else if (MOTOR_SN_2 == motorSn4Power)
    {
        funcCode.code.motorFcM2.torqueBoost = TorqueBoostDeal(power);
        SaveOneFuncCode(GetCodeIndex(funcCode.code.motorFcM2.torqueBoost));
    }
    else if (MOTOR_SN_3 == motorSn4Power)
    {
        funcCode.code.motorFcM3.torqueBoost = TorqueBoostDeal(power);
        SaveOneFuncCode(GetCodeIndex(funcCode.code.motorFcM3.torqueBoost));
    }
    else if (MOTOR_SN_4 == motorSn4Power)
    {
        funcCode.code.motorFcM4.torqueBoost = TorqueBoostDeal(power);
        SaveOneFuncCode(GetCodeIndex(funcCode.code.motorFcM4.torqueBoost));
    }

    if ((invPara.type > invPara.pointLimit)
        && (invTypeOfPower <= invPara.pointLimit))
    {
        flag = 10;
        dataPointFormat[0] = 100;
        dataPointFormat[5] = 100;

    }
    else if ((invPara.type <= invPara.pointLimit)
        && (invTypeOfPower > invPara.pointLimit))
    {
        flag = 10;
		dataPointFormat[1] = 100;
        dataPointFormat[2] = 100;
        dataPointFormat[3] = 100;
        dataPointFormat[4] = 100;
    }
    else
    {
        flag = 1;
    }

    for (j = 6; j >= 1; j--)
    {
        Uint16 index = motor1ParaIndex[j] + motorSnIndexIncToMotor1[motorSn4Power];

        funcCode.all[index] = (flag*(GetInvParaPointer(invTypeOfPower)->all[j+1]))/dataPointFormat[j-1];
        SaveOneFuncCode(index);
    }
#endif
}

Uint16 TorqueBoostDeal(Uint16 power)
{
#if (DEBUG_F_INV_TYPE_RELATE || DEBUG_F_MOTOR_POWER_RELATE)
    Uint16 torqueBoost;

    if (power <= 10)
    {
        torqueBoost = 60;
    }
    else if (power <= 40)
    {
        torqueBoost = 40;
    }
    else if (power <= 75)
    {
        torqueBoost = 30;
    }
    else if (power <= 370)
    {
        torqueBoost = 20;
    }
    else
    {
        torqueBoost = 10;
    }

    return torqueBoost;
#endif
}

Uint16 ValidateInvType(void)
{
#if DEBUG_F_INV_TYPE_RELATE
    Uint16 i;
    Uint16 upper;
    Uint16 lower;
    Uint16 ret = COMM_ERR_NONE;
    Uint16 invType = funcCode.code.inverterType;

    i = invType / 100;

    upper = invTypeLimitTable[i].upper + (i * 100);
    lower = invTypeLimitTable[i].lower + (i * 100);

    if (!((lower <= invType) && (invType <= upper)))
    {
        ret = COMM_ERR_PARA;
    }

    return ret;
#endif
}

void UpdateInvType(void)
{
    invPara.type = funcCode.code.inverterType % 100;
    invPara.volLevel = funcCode.code.inverterType / 100;
    invPara.ratingVoltage = invTypeLimitTable[invPara.volLevel].vol;
    invPara.pointLimit = INV_TYPE_POINT_LIMIT_TABLE[invPara.volLevel];
    invPara.bitAccDecStart = INV_TYPE_BIG_ACC_DEC_START_TABLE[invPara.volLevel];
}

extern enum MOTOR_SN motorSn;
void SaveTuneData(void)
{
#if DEBUG_F_TUNE
    int16 i;

    for (i = sizeof(motor1ParaIndex)-1; i >= 2; i--)
    {
        Uint16 index;
        Uint16 index4motorTune;
        Uint16 add = motorSnIndexIncToMotor1[motorSn];

        if (i == (sizeof(motor1ParaIndex)-3))
        {
            add = 0;
        }

        index = *(motor1ParaIndex + i) + add;
        index4motorTune = i - 2;

        funcCode.all[index] = gParaIdToFunctionDataBuff[index4motorTune];
        SaveOneFuncCode(index);
    }
#endif
}
