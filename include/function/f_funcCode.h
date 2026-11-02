
#ifndef __F_FUNCCODE_H__
#define __F_FUNCCODE_H__

#include "DSP28x_Project.h"

#include "f_debug.h"
#include "f_interface.h"
#include "f_p2p.h"

#define EEPROM_CHECK1       (0x0009)
#define EEPROM_CHECK2       (0xFFFF - EEPROM_CHECK1)

#define USER_MENU_GROUP     FUNCCODE_GROUP_FE

#define GetCodeIndex(code)    ((Uint16)((&(code)) - (&(funcCode.all[0]))))
#define GetGradeIndex(group, grade)  (funcCodeGradeSum[group] + (grade))

#define ATTRIBUTE_UNIT_HZ_BIT           0
#define ATTRIBUTE_UNIT_A_BIT            1
#define ATTRIBUTE_UNIT_V_BIT            2

#define ATTRIBUTE_READ_AND_WRITE        0
#define ATTRIBUTE_READ_ONLY_WHEN_RUN    1
#define ATTRIBUTE_READ_ONLY_ANYTIME     2

#define ATTRIBUTE_MULTI_LIMIT_SINGLE    0
#define ATTRIBUTE_MULTI_LIMIT_DEC       1
#define ATTRIBUTE_MULTI_LIMIT_HEX       2

struct  FUNC_ATTRIBUTE_BITS
{
    Uint16 point:3;

    Uint16 unit:3;

    Uint16 displayBits:3;
    Uint16 upperLimit:1;
    Uint16 lowerLimit:1;
    Uint16 writable:2;
    Uint16 signal:1;
    Uint16 multiLimit:2;

};

union FUNC_ATTRIBUTE
{
   Uint16                      all;
   struct FUNC_ATTRIBUTE_BITS  bit;
};

typedef struct FUNCCODE_ATTRIBUTE_STRUCT
{
    Uint16                  lower;
    Uint16                  upper;
    Uint16                  init;
    union FUNC_ATTRIBUTE    attribute;

    Uint16                  eepromIndex;
} FUNCCODE_ATTRIBUTE;

extern const FUNCCODE_ATTRIBUTE funcCodeAttribute[];

struct PLC_STRUCT
{
    Uint16 runTime;
    Uint16 accDecTimeSet;
};

struct AI_SET_CURVE
{
    Uint16 minIn;
    Uint16 minInSet;
    Uint16 maxIn;
    Uint16 maxInSet;
};

struct AI_JUMP
{
    Uint16 point;
    Uint16 arrange;
};

struct ANALOG_CALIBRATE_CURVE
{
    Uint16 before1;
    Uint16 after1;

    Uint16 before2;
    Uint16 after2;
};

typedef struct AO_PARA_STRUCT
{
    Uint16 offset;
    Uint16 gain;
} AO_PARA;

struct FC_GROUP_HIDE_STRUCT
{
    Uint16 password;

    Uint16 f;
    Uint16 a;
    Uint16 b;
    Uint16 c;
};
union FC_GROUP_HIDE
{
    Uint16 all[5];
    struct FC_GROUP_HIDE_STRUCT elem;
};

struct ERROR_SCENE_STRUCT
{
    Uint16 errorFrq;
    Uint16 errorCurrent;
    Uint16 errorGeneratrixVoltage;
    Uint16 errorDiStatus;
    Uint16 errorDoStatus;

    Uint16 errorInverterStatus;
    Uint16 errorTimeFromPowerUp;
    Uint16 errorTimeFromRun;
};

union ERROR_SCENE
{
    Uint16 all[sizeof(struct ERROR_SCENE_STRUCT)];

    struct ERROR_SCENE_STRUCT elem;
};

#define APTP_NUM    32
typedef struct
{
    Uint16 low;
    Uint16 high;
} LENGTH_SET;

#define MOTOR_TYPE_ACI_GENERAL  0
#define MOTOR_TYPE_ACI_INV      1
#define MOTOR_TYPE_PMSM         2
struct MOTOR_PARA_STRUCT
{

    Uint16 motorType;
    Uint16 ratingPower;
    Uint16 ratingVoltage;
    Uint16 ratingCurrent;
    Uint16 ratingFrq;
    Uint16 ratingSpeed;

    Uint16 statorResistance;
    Uint16 rotorResistance;
    Uint16 leakInductance;
    Uint16 mutualInductance;
    Uint16 zeroLoadCurrent;
    Uint16 rsvdF11[5];

    Uint16 pmsmRs;
    Uint16 pmsmLd;
    Uint16 pmsmLq;
    Uint16 pmsmRsLdUnit;
    Uint16 pmsmCoeff;
    Uint16 pmsmCheckTime;
    Uint16 rsvdF12[5];

};

struct PG_PARA_STRUCT
{

    Uint16 encoderPulse;
    Uint16 pgType;
    Uint16 fvcPgSrc;
    Uint16 enCoderDir;
    Uint16 enCoderAngle;
    Uint16 uvwSignDir;
    Uint16 uvwSignAngle;
    Uint16 enCoderPole;
    Uint16 rsvdF11;
    Uint16 fvcPgLoseTime;
};

union MOTOR_PARA
{
    Uint16 all[sizeof(struct MOTOR_PARA_STRUCT)];

    struct MOTOR_PARA_STRUCT elem;
};

union PG_PARA
{
    Uint16 all[sizeof(struct PG_PARA_STRUCT)];

    struct PG_PARA_STRUCT elem;
};

enum MOTOR_SN
{
    MOTOR_SN_1,
    MOTOR_SN_2,
    MOTOR_SN_3,
    MOTOR_SN_4
};

struct VC_PARA
{
    Uint16 vcSpdLoopKp1;
    Uint16 vcSpdLoopTi1;
    Uint16 vcSpdLoopChgFrq1;
    Uint16 vcSpdLoopKp2;
    Uint16 vcSpdLoopTi2;

    Uint16 vcSpdLoopChgFrq2;
    Uint16 vcSlipCompCoef;
    Uint16 vcSpdLoopFilterTime;
    Uint16 vcOverMagGain;
    Uint16 spdCtrlDriveTorqueLimitSrc;

    Uint16 spdCtrlDriveTorqueLimit;
    Uint16 spdCtrlBrakeTorqueLimitSrc;
    Uint16 spdCtrlBrakeTorqueLimit;
    Uint16 mAcrKp;
    Uint16 mAcrKi;

    Uint16 tAcrKp;
    Uint16 tAcrKi;
    Uint16 spdLoopI;

    Uint16 weakFlusMode;
    Uint16 weakFlusCoef;
    Uint16 weakFlusCurMax;
    Uint16 weakFlusAutoCoef;
    Uint16 weakFlusIntegrMul;
};

struct MOTOR_FC
{
    union MOTOR_PARA motorPara;
    union PG_PARA    pgPara;
    Uint16 tuneCmd;
    struct VC_PARA vcPara;

    Uint16 motorCtrlMode;
    Uint16 accDecTimeMotor;
    Uint16 torqueBoost;
    Uint16 rsvdA21;
    Uint16 antiVibrateGain;
};

#define AI_NUMBER               3

#define AO_NUMBER               2

#define HDI_NUMBER              1

#define HDO_NUMBER              1

#define DI_NUMBER_PHSIC         10
#define DI_NUMBER_V             5
#define DI_NUMBER_AI_AS_DI      AI_NUMBER

#define DO_NUMBER_PHSIC         5
#define DO_NUMBER_V             5

#define PLC_STEP_MAX            16

#define EEPROM_INDEX_USE_LENGTH     994
#define EEPROM_INDEX_USE_INDEX        4

#define FUNCCODE_RSVD4ALL_INDEX     GetCodeIndex(funcCode.code.rsvd4All)

#define REM_P_OFF_MOTOR     5

#define FUNCCODE_GROUP_NUM  83

#define FUNCCODE_GROUP_F0   0
#define FUNCCODE_GROUP_F1   1
#define FUNCCODE_GROUP_FE   14
#define FUNCCODE_GROUP_FF   15
#define FUNCCODE_GROUP_FP   16
#define FUNCCODE_GROUP_A0   17
#define FUNCCODE_GROUP_A5   22
#define FUNCCODE_GROUP_AA   27
#define FUNCCODE_GROUP_AB   28
#define FUNCCODE_GROUP_AE   31
#define FUNCCODE_GROUP_AF   32
#define FUNCCODE_GROUP_B0   33
#define FUNCCODE_GROUP_BF   48
#define FUNCCODE_GROUP_C0   49

#define FUNCCODE_GROUP_CF   64
#define FUNCCODE_GROUP_U0   67
#define FUNCCODE_GROUP_U3   70
#define FUNCCODE_GROUP_UF   (FUNCCODE_GROUP_NUM - 1)

#define F0NUM           (28+ 1  )
#define F1NUM           (38+ 0  )
#define F2NUM           (23+ 0  )
#define F3NUM           (16+ 0  )

#define F4NUM           (40+ 0  )
#define F5NUM           (23+ 0  )
#define F6NUM           (16+ 0  )
#define F7NUM           (15+ 0  )

#define F8NUM           (54+ 0  )
#define F9NUM           (71+ 0  )
#define FANUM           (29+ 0  )
#define FBNUM           (10+ 0  )

#define FCNUM           (52+ 0  )
#define FDNUM           ( 8+ 0  )
#define FENUM           (32+ 0  )
#define FFNUM           (13+ 0  )

#define FPNUM           ( 6+ 0  )

#define A0NUM           ( 9+ 0  )
#define A1NUM           (22+0   )
#define A2NUM           (F1NUM+2+F2NUM+3)
#define A3NUM           (A2NUM)
#define A4NUM           (A2NUM)
#define A5NUM           (10+ 0  )
#define A6NUM           (30+0   )
#define A7NUM           (9+3    )
#define A8NUM           (8+0    )
#define A9NUM           (30     )
#define AANUM           (1+ 0  )
#define ABNUM           (1+ 0   )
#define ACNUM           (20     )
#define ADNUM           (1+0    )
#define AENUM           (ACNUM  )
#define AFNUM           ( 1+0   )

#define B0NUM           ( 0+1   )
#define B1NUM           ( 0+1   )
#define B2NUM           ( 0+1   )
#define B3NUM           ( 0+1   )

#define B4NUM           ( 0+1   )
#define B5NUM           ( 0+1   )
#define B6NUM           ( 0+1   )
#define B7NUM           ( 0+1   )

#define B8NUM           ( 0+1   )
#define B9NUM           ( 0+1   )
#define BANUM           ( 0+1   )
#define BBNUM           ( 0+1   )

#define BCNUM           ( 0+1   )
#define BDNUM           ( 0+1   )
#define BENUM           ( 0+1   )
#define BFNUM           ( 0+1   )

#define C0NUM           ( 0+1   )
#define C1NUM           ( 0+1   )
#define C2NUM           ( 0+1)
#define C3NUM           ( 0+1)

#define C4NUM           ( 0+1)
#define C5NUM           ( 0+1)
#define C6NUM           ( 0+1)
#define C7NUM           ( 0+1)

#define C8NUM           ( 0+1)
#define C9NUM           ( 0+1)
#define CANUM           ( 0+1)
#define CBNUM           ( 0+1)

#define CCNUM           ( 1+0)
#define CDNUM           ( 0+1)
#define CENUM           ( 0+1)
#if DEBUG_F_MOTOR_FUNCCODE
#define CFNUM           FUNC_TO_CORE_DEBUG_DATA_NUM
#elif 1
#define CFNUM           ( 0+1)
#endif

#define CHK_NUM   (4 +0  )
#define REM_NUM   (48)

#define U0NUM     (100+0)
#define U1NUM     ( 0+1)
#define U2NUM     ( 0+1)
#define U3NUM     (10+2)

#define U4NUM     ( 0+1)
#define U5NUM     ( 0+1)
#define U6NUM     ( 0+1)
#define U7NUM     ( 0+1)

#define U8NUM     ( 0+1)
#define U9NUM     ( 0+1)
#define UANUM     ( 0+1)
#define UBNUM     ( 0+1)

#define UCNUM     ( 0+1)
#define UDNUM     ( 0+1)
#define UENUM     ( 0+1)
#if DEBUG_F_MOTOR_FUNCCODE
#define UFNUM     CORE_TO_FUNC_DISP_DATA_NUM
#elif 1
#define UFNUM     ( 0+1)
#endif

#define FNUM_PARA      (F0NUM + F1NUM + F2NUM + F3NUM +     \
                        F4NUM + F5NUM + F6NUM + F7NUM +     \
                        F8NUM + F9NUM + FANUM + FBNUM +     \
                        FCNUM + FDNUM + FENUM + FFNUM +     \
                                                            \
                        FPNUM +                             \
                                                            \
                        A0NUM + A1NUM + A2NUM + A3NUM +     \
                        A4NUM + A5NUM + A6NUM + A7NUM +     \
                        A8NUM + A9NUM + AANUM + ABNUM +     \
                        ACNUM + ADNUM + AENUM + AFNUM +     \
                                                            \
                        B0NUM + B1NUM + B2NUM + B3NUM +     \
                        B4NUM + B5NUM + B6NUM + B7NUM +     \
                        B8NUM + B9NUM + BANUM + BBNUM +     \
                        BCNUM + BDNUM + BENUM + BFNUM +     \
                                                            \
                        C0NUM + C1NUM + C2NUM + C3NUM +     \
                        C4NUM + C5NUM + C6NUM + C7NUM +     \
                        C8NUM + C9NUM + CANUM + CBNUM +     \
                        CCNUM + CDNUM + CENUM + CFNUM       \
                        )
#define FNUM_EEPROM    (FNUM_PARA + CHK_NUM + REM_NUM)
#define FNUM_ALL       (FNUM_EEPROM +                       \
                        U0NUM + U1NUM + U2NUM + U3NUM +     \
                        U4NUM + U5NUM + U6NUM + U7NUM +     \
                        U8NUM + U9NUM + UANUM + UBNUM +     \
                        UCNUM + UDNUM + UENUM + UFNUM       \
                       )

struct FUNCCODE_GROUP
{

    Uint16 f0[F0NUM];
    Uint16 f1[F1NUM];
    Uint16 f2[F2NUM];
    Uint16 f3[F3NUM];

    Uint16 f4[F4NUM];
    Uint16 f5[F5NUM];
    Uint16 f6[F6NUM];
    Uint16 f7[F7NUM];

    Uint16 f8[F8NUM];
    Uint16 f9[F9NUM];
    Uint16 fa[FANUM];
    Uint16 fb[FBNUM];

    Uint16 fc[FCNUM];
    Uint16 fd[FDNUM];
    Uint16 fe[FENUM];
    Uint16 ff[FFNUM];

    Uint16 fp[FPNUM];

    Uint16 a0[A0NUM];
    Uint16 a1[A1NUM];
    Uint16 a2[A2NUM];
    Uint16 a3[A3NUM];

    Uint16 a4[A4NUM];
    Uint16 a5[A5NUM];
    Uint16 a6[A6NUM];
    Uint16 a7[A7NUM];

    Uint16 a8[A8NUM];
    Uint16 a9[A9NUM];
    Uint16 aa[AANUM];
    Uint16 ab[ABNUM];

    Uint16 ac[ACNUM];
    Uint16 ad[ADNUM];
    Uint16 ae[AENUM];
    Uint16 af[AFNUM];

    Uint16 b0[B0NUM];
    Uint16 b1[B1NUM];
    Uint16 b2[B2NUM];
    Uint16 b3[B3NUM];

    Uint16 b4[B4NUM];
    Uint16 b5[B5NUM];
    Uint16 b6[B6NUM];
    Uint16 b7[B7NUM];

    Uint16 b8[B8NUM];
    Uint16 b9[B9NUM];
    Uint16 ba[BANUM];
    Uint16 bb[BBNUM];

    Uint16 bc[BCNUM];
    Uint16 bd[BDNUM];
    Uint16 be[BENUM];
    Uint16 bf[BFNUM];

    Uint16 c0[C0NUM];
    Uint16 c1[C1NUM];
    Uint16 c2[C2NUM];
    Uint16 c3[C3NUM];

    Uint16 c4[C4NUM];
    Uint16 c5[C5NUM];
    Uint16 c6[C6NUM];
    Uint16 c7[C7NUM];

    Uint16 c8[C8NUM];
    Uint16 c9[C9NUM];
    Uint16 ca[CANUM];
    Uint16 cb[CBNUM];

    Uint16 cc[CCNUM];
    Uint16 cd[CDNUM];
    Uint16 ce[CENUM];
    Uint16 cf[CFNUM];

    Uint16 fChk[CHK_NUM];

    Uint16 remember[REM_NUM];

    Uint16 u0[U0NUM];
    Uint16 u1[U1NUM];
    Uint16 u2[U2NUM];
    Uint16 u3[U3NUM];

    Uint16 u4[U4NUM];
    Uint16 u5[U5NUM];
    Uint16 u6[U6NUM];
    Uint16 u7[U7NUM];

    Uint16 u8[U8NUM];
    Uint16 u9[U9NUM];
    Uint16 ua[UANUM];
    Uint16 ub[UBNUM];

    Uint16 uc[UCNUM];
    Uint16 ud[UDNUM];
    Uint16 ue[UENUM];
    Uint16 uf[UFNUM];

};

struct FUNCCODE_CODE
{

    Uint16 inverterGpTypeDisp;
    Uint16 motorCtrlMode;
    Uint16 runSrc;
    Uint16 frqXSrc;
    Uint16 frqYSrc;
    Uint16 frqYRangeBase;
    Uint16 frqYRange;
    Uint16 frqCalcSrc;
    Uint16 presetFrq;
    Uint16 runDir;
    Uint16 maxFrq;
    Uint16 upperFrqSrc;
    Uint16 upperFrq;
    Uint16 upperFrqOffset;
    Uint16 lowerFrq;
    Uint16 carrierFrq;
    Uint16 varFcByTem;
    Uint16 accTime1;
    Uint16 decTime1;
    Uint16 accDecTimeUnit;
    Uint16 frqYOffsetSrc;
    Uint16 frqYOffsetFc;
    Uint16 frqPoint;
    Uint16 frqRemMode;
    enum MOTOR_SN motorSn;
    Uint16 accDecBenchmark;
    Uint16 updnBenchmark;
    Uint16 frqRunCmdBind;
    Uint16 commProtocolSec;

    union MOTOR_PARA motorParaM1;
    union PG_PARA    pgParaM1;
    Uint16 tuneCmd;

    struct VC_PARA vcParaM1;

    Uint16 vfCurve;
    Uint16 torqueBoost;
    Uint16 boostCloseFrq;
    Uint16 vfFrq1;
    Uint16 vfVol1;
    Uint16 vfFrq2;
    Uint16 vfVol2;
    Uint16 vfFrq3;
    Uint16 vfVol3;
    Uint16 slipCompCoef;
    Uint16 vfOverMagGain;
    Uint16 antiVibrateGain;

    Uint16 rsvdF31;
    Uint16 vfVoltageSrc;
    Uint16 vfVoltageDigtalSet;
    Uint16 vfVoltageAccTime;

    Uint16 diFunc[DI_NUMBER_PHSIC];

    Uint16 diFilterTime;
    Uint16 diControlMode;
    Uint16 diUpDownSlope;

    Uint16 curveSet2P1[4];
    Uint16 ai1FilterTime;

    Uint16 curveSet2P2[4];
    Uint16 ai2FilterTime;

    Uint16 curveSet2P3[4];
    Uint16 ai3FilterTime;

    Uint16 curveSet2P4[4];
    Uint16 pulseInFilterTime;

    Uint16 aiCurveSrc;

    Uint16 aiLimitSrc;

    Uint16 diDelayTime[3];
    Uint16 diLogic[2];

    Uint16 fmOutSelect;
    Uint16 doFunc[DO_NUMBER_PHSIC];

    Uint16 aoFunc[AO_NUMBER+HDO_NUMBER];

    Uint16 fmpOutMaxFrq;

    AO_PARA aoPara[AO_NUMBER];

    Uint16 aoLpfTime[AO_NUMBER+HDO_NUMBER];

    Uint16 doDelayTime[DO_NUMBER_PHSIC];

    Uint16 doLogic;

    Uint16 startMode;
    Uint16 speedTrackMode;
    Uint16 speedTrackVelocity;
    Uint16 startFrq;
    Uint16 startFrqTime;
    Uint16 startBrakeCurrent;
    Uint16 startBrakeTime;
    Uint16 accDecSpdCurve;
    Uint16 sCurveStartPhaseTime;
    Uint16 sCurveEndPhaseTime;
    Uint16 stopMode;
    Uint16 stopBrakeFrq;
    Uint16 stopBrakeWaitTime;
    Uint16 stopBrakeCurrent;
    Uint16 stopBrakeTime;
    Uint16 brakeDutyRatio;

    Uint16 rsvdF71;
    Uint16 mfkKeyFunc;
    Uint16 stopKeyFunc;
    Uint16 ledDispParaRun1;
    Uint16 ledDispParaRun2;

    Uint16 ledDispParaStop;
    Uint16 speedDispCoeff;
    Uint16 radiatorTemp;
    Uint16 temp2;
    Uint16 runTimeAddup;

    Uint16 productVersion;
    Uint16 softVersion;
    Uint16 speedDispPointPos;
    Uint16 powerUpTimeAddup;
    Uint16 powerAddup;

    Uint16 jogFrq;
    Uint16 jogAccTime;
    Uint16 jogDecTime;
    Uint16 accTime2;
    Uint16 decTime2;
    Uint16 accTime3;
    Uint16 decTime3;
    Uint16 accTime4;
    Uint16 decTime4;
    Uint16 jumpFrq1;
    Uint16 jumpFrq2;
    Uint16 jumpFrqRange;
    Uint16 zeroSpeedDeadTime;
    Uint16 antiReverseRun;
    Uint16 lowerDeal;
    Uint16 droopCtrl;
    Uint16 powerUpTimeArriveSet;
    Uint16 runTimeArriveSet;
    Uint16 startProtect;
    Uint16 frqFdtValue;
    Uint16 frqFdtLag;
    Uint16 frqArriveRange;
    Uint16 jumpFrqMode;
    Uint16 runTimeOverAct;

    Uint16 powerUpTimeOverAct;
    Uint16 accTimefrqChgValue;
    Uint16 decTimefrqChgValue;
    Uint16 jogWhenRun;
    Uint16 frqFdt1Value;
    Uint16 frqFdt1Lag;
    Uint16 frqArriveValue1;
    Uint16 frqArriveRange1;
    Uint16 frqArriveValue2;
    Uint16 frqArriveRange2;

    Uint16 oCurrentChkValue;
    Uint16 oCurrentChkTime;
    Uint16 softOCValue;
    Uint16 softOCDelay;

    Uint16 currentArriveValue1;
    Uint16 currentArriveRange1;
    Uint16 currentArriveValue2;
    Uint16 currentArriveRange2;

    Uint16 setTimeMode;
    Uint16 setTimeSource;
    Uint16 setTimeValue;

    Uint16 ai1VoltageLimit;
    Uint16 ai1VoltageUpper;

    Uint16 temperatureArrive;
    Uint16 fanControl;
    Uint16 wakeUpFrq;
    Uint16 wakeUpTime;
    Uint16 dormantFrq;
    Uint16 dormantTime;
    Uint16 setTimeArrive;

    Uint16 overloadMode;
    Uint16 overloadGain;
    Uint16 foreOverloadCoef;
    Uint16 ovGain;
    Uint16 ovPoint;

    Uint16 ocGain;
    Uint16 ocPoint;
    Uint16 shortCheckMode;
    Uint16 rsvdF91;
    Uint16 errAutoRstNumMax;

    Uint16 errAutoRstRelayAct;
    Uint16 errAutoRstSpaceTime;
    Uint16 inPhaseLossProtect;
    Uint16 outPhaseLossProtect;
    Uint16 errorLatest1;

    Uint16 errorLatest2;
    Uint16 errorLatest3;

    union ERROR_SCENE errorScene3;

    Uint16 rsvdF92[2];

    union ERROR_SCENE errorScene2;
    Uint16 rsvdF921[2];

    union ERROR_SCENE errorScene1;
    Uint16 rsvdF922[2];

    Uint16 errorAction[5];
    Uint16 errorShow[2];
    Uint16 errorRunFrqSrc;
    Uint16 errorSecondFrq;

    Uint16 motorOtMode;
    Uint16 motorOtProtect;
    Uint16 motorOtCoef;

    Uint16 pOffTransitoryNoStop;
    Uint16 pOffTransitoryFrqDecSlope;
    Uint16 pOffVolBackTime;

    Uint16 pOffThresholdVol;
    Uint16 loseLoadProtectMode;
    Uint16 loseLoadLevel;
    Uint16 loseLoadTime;
    Uint16 rsvdF923;

    Uint16 osChkValue;
    Uint16 osChkTime;
    Uint16 devChkValue;
    Uint16 devChkTime;

#if 0
    Uint16 losePowerStopEnable;
    Uint16 losePowerStopSel;
    Uint16 losePowerLowerFrq;
    Uint16 losePowerLowerTime;
    Uint16 losePowerDectime;
    Uint16 losePowerP;
    Uint16 losePowerI;

#endif

    Uint16 pidSetSrc;
    Uint16 pidSet;
    Uint16 pidFdbSrc;
    Uint16 pidDir;
    Uint16 pidDisp;

    Uint16 pidKp;
    Uint16 pidTi;
    Uint16 pidTd;
    Uint16 reverseCutOffFrq;
    Uint16 pidErrMin;
    Uint16 pidDLimit;
    Uint16 pidSetChangeTime;
    Uint16 pidFdbLpfTime;
    Uint16 pidOutLpfTime;
    Uint16 pidSampleTime;
    Uint16 pidKp2;
    Uint16 pidTi2;
    Uint16 pidTd2;
    Uint16 pidParaChgCondition;
    Uint16 pidParaChgDelta1;
    Uint16 pidParaChgDelta2;
    Uint16 pidInit;
    Uint16 pidInitTime;
    Uint16 pidOutDeltaMax;
    Uint16 pidOutDeltaMin;
    Uint16 pidIAttribute;
    Uint16 pidFdbLoseDetect;
    Uint16 pidFdbLoseDetectTime;
    Uint16 pidCalcMode;

    Uint16 swingBaseMode;
    Uint16 swingAmplitude;
    Uint16 swingJumpRange;
    Uint16 swingPeriod;
    Uint16 swingRiseTimeCoeff;
    Uint16 lengthSet;
    Uint16 lengthCurrent;
    Uint16 lengthPulsePerMeter;
    Uint16 counterSet;
    Uint16 counterPoint;

    Uint16 plcFrq[PLC_STEP_MAX];
    Uint16 plcRunMode;
    Uint16 plcPowerOffRemMode;
    struct PLC_STRUCT plcAttribute[PLC_STEP_MAX];
    Uint16 plcTimeUnit;
    Uint16 plcFrq0Src;

    Uint16 commBaudRate;
    Uint16 commParity;
    Uint16 commSlaveAddress;
    Uint16 commDelay;
    Uint16 commOverTime;
    Uint16 commProtocol;
    Uint16 commReadCurrentPoint;
    Uint16 commMaster;

    Uint16 userCustom[FENUM];

    Uint16 factoryPassword;
    Uint16 inverterType;
    Uint16 inverterGpType;
    Uint16 inverterPower;
    Uint16 tempCurve;

    Uint16 uvGainWarp;
    Uint16 funcSoftVersion;
    Uint16 motorSoftVersion;
    Uint16 volJudgeCoeff;
    Uint16 curJudgeCoeff;

    Uint16 motorDebugFc;
    Uint16 aiaoCalibrateDisp;
    Uint16 memoryAddr;

    Uint16 userPassword;
    Uint16 paraInitMode;
    Uint16 funcParaView;
    Uint16 menuMode;

    Uint16 userPasswordReadOnly;
    Uint16 rsvdFp;

    Uint16 torqueCtrl;
    Uint16 driveUpperTorqueSrc;
    Uint16 brakeUpperTorqueSrc;
    Uint16 driveUpperTorque;
    Uint16 torqueFilter;
    Uint16 torqueCtrlFwdMaxFrq;
    Uint16 torqueCtrlRevMaxFrq;
    Uint16 torqueCtrlAccTime;
    Uint16 torqueCtrlDecTime;

    Uint16 vdiFunc[5];
    Uint16 vdiSrc;
    Uint16 vdiFcSet;
    Uint16 aiAsDiFunc[3];
    Uint16 diLogicAiAsDi;
    Uint16 vdoFunc[5];
    Uint16 vdoDelayTime[5];
    Uint16 vdoLogic;

    struct MOTOR_FC motorFcM2;

    struct MOTOR_FC motorFcM3;

    struct MOTOR_FC motorFcM4;

    Uint16 pwmMode;
    Uint16 modulationMode;
    Uint16 deadCompMode;
    Uint16 softPwm;
    Uint16 cbcEnable;
    Uint16 curSampleDelayComp;
    Uint16 uvPoint;
    Uint16 svcMode;
    Uint16 deadTimeSet;
    Uint16 ovPointSet;

    Uint16 curveSet4P1[8];
    Uint16 curveSet4P2[8];
    Uint16 rsvdA41[8];

    struct AI_JUMP aiJumpSet[AI_NUMBER];

    Uint16 plcEnable;
    Uint16 outPortControl;
    Uint16 plcAI3Cfg;
    Uint16 fmpValue;
    Uint16 ao1Value;
    Uint16 inPortOut;
    Uint16 plcFrqSet;
    Uint16 plcTorqueSet;
    Uint16 plcCmd;
    Uint16 plcErrorCode;
    Uint16 rsvdA7[2];

    Uint16 p2pEnable;
    Uint16 p2pTypeSel;
    Uint16 p2pSendDataSel;
    Uint16 p2pRevDataSel;
    Uint16 p2pRevOffset;
    Uint16 p2pRevGain;
    Uint16 p2pOverTime;
    Uint16 p2pSendPeriod;

    Uint16 A9[A9NUM];

    Uint16 AA[AANUM];
    #if 0
    Uint16 motorCtrlM1;
    Uint16 motorCtrlM2;
    Uint16 motorCtrlM3;
    Uint16 motorCtrlM4;
    Uint16 motorCtrlM5;
    Uint16 motorCtrlM6;
    Uint16 motorCtrlM7;
    Uint16 motorCtrlM8;
    Uint16 motorCtrlM9;
    Uint16 motorCtrlM10;
    Uint16 motorCtrlM11;
    Uint16 motorCtrlM12;
    Uint16 motorCtrlM13;
    Uint16 motorCtrlM14;
    Uint16 motorCtrlM15;
    Uint16 motorCtrlM16;
    #endif

    Uint16 AB[ABNUM];
    #if 0
    Uint16 vfCtrlM2;
    Uint16 vfCtrlM3;
    Uint16 vfCtrlM4;
    Uint16 vfCtrlM5;
    Uint16 vfCtrlM6;
    Uint16 vfCtrlM7;
    Uint16 vfCtrlM8;
    #endif

    struct ANALOG_CALIBRATE_CURVE aiCalibrateCurve[AI_NUMBER];
    struct ANALOG_CALIBRATE_CURVE aoCalibrateCurve[AO_NUMBER];

    Uint16 AD[ADNUM];

    struct ANALOG_CALIBRATE_CURVE aiFactoryCalibrateCurve[AI_NUMBER];
    struct ANALOG_CALIBRATE_CURVE aoFactoryCalibrateCurve[AO_NUMBER];

    Uint16 AF[AFNUM];

    Uint16 b0[B0NUM];
    Uint16 b1[B1NUM];
    Uint16 b2[B2NUM];
    Uint16 b3[B3NUM];

    Uint16 b4[B4NUM];
    Uint16 b5[B5NUM];
    Uint16 b6[B6NUM];
    Uint16 b7[B7NUM];

    Uint16 b8[B8NUM];
    Uint16 b9[B9NUM];
    Uint16 ba[BANUM];
    Uint16 bb[BBNUM];

    Uint16 bc[BCNUM];
    Uint16 bd[BDNUM];
    Uint16 be[BENUM];
    Uint16 bf[BFNUM];

    Uint16 c0[C0NUM];
    Uint16 c1[C1NUM];
    Uint16 c2[C2NUM];
    Uint16 c3[C3NUM];

    Uint16 c4[C4NUM];
    Uint16 c5[C5NUM];
    Uint16 c6[C6NUM];
    Uint16 c7[C7NUM];

    Uint16 c8[C8NUM];
    Uint16 c9[C9NUM];
    Uint16 ca[CANUM];
    Uint16 cb[CBNUM];

    Uint16 cc[CCNUM];
    Uint16 cd[CDNUM];
    Uint16 ce[CENUM];
    Uint16 cf[CFNUM];

    Uint16 rsvd4All;
    Uint16 eepromCheckWord1;
    Uint16 eepromCheckWord2;
    Uint16 aiaoChkWord;

    Uint16 extendType;
    Uint16 plcStepRem;
    Uint16 plcTimeHighRem;
    Uint16 plcTimeLowRem;
    Uint16 dispParaRunBit;
    Uint16 dispParaStopBit;
    Uint16 runTimeAddupSec;
    Uint16 counterTicker;
    Uint16 lengthTickerRemainder;
    Uint16 frqComm;
    Uint16 upDownFrqRem;
    Uint16 pmsmRotorPos;
    Uint16 powerAddupInt;
    Uint16 powerUpTimeAddupSec;
    Uint16 errorFrqUnit;
    Uint16 saveUserParaFlag1;
    Uint16 saveUserParaFlag2;
    Uint16 speedFdbDir;
    Uint16 rsvdRem[2];
    Uint16 rsvdRem1[23];
    Uint16 remPOffMotorCtrl[REM_P_OFF_MOTOR];

    Uint16 u0[U0NUM];
    Uint16 u1[U1NUM];
    Uint16 u2[U2NUM];
    Uint16 u3[U3NUM];

    Uint16 u4[U4NUM];
    Uint16 u5[U5NUM];
    Uint16 u6[U6NUM];
    Uint16 u7[U7NUM];

    Uint16 u8[U8NUM];
    Uint16 u9[U9NUM];
    Uint16 ua[UANUM];
    Uint16 ub[UBNUM];

    Uint16 uc[UCNUM];
    Uint16 ud[UDNUM];
    Uint16 ue[UENUM];
    Uint16 uf[UFNUM];

};

typedef union FUNCCODE_ALL_UNION
{
    Uint16 all[FNUM_ALL];

    struct FUNCCODE_GROUP group;

    struct FUNCCODE_CODE code;
} FUNCCODE_ALL;

#define FUNCCODE_frqXySrc_FC                0
#define FUNCCODE_frqXySrc_FC_P_OFF_REM      1
#define FUNCCODE_frqXySrc_AI1               2
#define FUNCCODE_frqXySrc_AI2               3
#define FUNCCODE_frqXySrc_AI3               4
#define FUNCCODE_frqXySrc_PULSE             5
#define FUNCCODE_frqXySrc_MULTI_SET         6
#define FUNCCODE_frqXySrc_PLC               7
#define FUNCCODE_frqXySrc_PID               8
#define FUNCCODE_frqXySrc_COMM              9

#define FUNCCODE_frqYRangeBase_MAX_FRQ      0
#define FUNCCODE_frqYRangeBase_FRQ_X        1

#define FUNCCODE_frqCalcSrc_X               0
#define FUNCCODE_frqCalcSrc_COMPOSE         1
#define FUNCCODE_frqCalcSrc_X_OR_Y          2
#define FUNCCODE_frqCalcSrc_X_OR_COMPOSE    3
#define FUNCCODE_frqCalcSrc_Y_OR_COMPOSE    4

#define FUNCCODE_frqCalcSrc_ADD             0
#define FUNCCODE_frqCalcSrc_SUBTRATION      1
#define FUNCCODE_frqCalcSrc_MAX             2
#define FUNCCODE_frqCalcSrc_MIN             3
#define FUNCCODE_frqCalcSrc_4               4
#define FUNCCODE_frqCalcSrc_5               5

#define FUNCCODE_upperFrqSrc_FC         0
#define FUNCCODE_upperFrqSrc_AI1        1
#define FUNCCODE_upperFrqSrc_AI2        2
#define FUNCCODE_upperFrqSrc_AI3        3
#define FUNCCODE_upperFrqSrc_PULSE      4
#define FUNCCODE_upperFrqSrc_COMM       5

#define FUNCCODE_frqPoint_1             0
#define FUNCCODE_frqPoint_0_1           1
#define FUNCCODE_frqPoint_0_01          2

#define FUNCCODE_accDecTimeUnit_0POINT  0
#define FUNCCODE_accDecTimeUnit_1POINT  1
#define FUNCCODE_accDecTimeUnit_2POINT  2

#define FUNCCODE_frqRemMode_POWEROFF_NO     0
#define FUNCCODE_frqRemMode_POWEROFF_YES    1
#define FUNCCODE_frqRemMode_STOP_NO         0
#define FUNCCODE_frqRemMode_STOP_YES        1

#define FUNCCODE_accDecSpdCurve_LINE        0
#define FUNCCODE_accDecSpdCurve_S_CURVE_A   1
#define FUNCCODE_accDecSpdCurve_S_CURVE_B   2
#define ACC_DEC_LINE    FUNCCODE_accDecSpdCurve_LINE
#define ACC_DEC_SA      FUNCCODE_accDecSpdCurve_S_CURVE_A
#define ACC_DEC_SB      FUNCCODE_accDecSpdCurve_S_CURVE_B

#define FUNCCODE_upperTorqueSrc_FC      0
#define FUNCCODE_upperTorqueSrc_AI1     1
#define FUNCCODE_upperTorqueSrc_AI2     2
#define FUNCCODE_upperTorqueSrc_AI3     3
#define FUNCCODE_upperTorqueSrc_PULSE   4
#define FUNCCODE_upperTorqueSrc_COMM    5
#define FUNCCODE_upperTorqueSrc_MIN_AI1_AI2 6
#define FUNCCODE_upperTorqueSrc_MAX_AI1_AI2 7

#define FUNCCODE_fvcPgSrc_QEP1          0
#define FUNCCODE_fvcPgSrc_QEP2          1

#define TIME_UNIT_ACC_DEC_SPEED         100

#define FUNCCODE_vfCurve_Line               0
#define FUNCCODE_vfCurve_DOT                1
#define FUNCCODE_vfCurve_SQUARE             2
#define FUNCCODE_vfCurve_ALL_SEPARATE       10
#define FUNCCODE_vfCurve_HALF_SEPARATE      11

#define FUNCCODE_vfVoltageSrc_FC            0
#define FUNCCODE_vfVoltageSrc_AI1           1
#define FUNCCODE_vfVoltageSrc_AI2           2
#define FUNCCODE_vfVoltageSrc_AI3           3
#define FUNCCODE_vfVoltageSrc_PULSE         4
#define FUNCCODE_vfVoltageSrc_MULTI_SET     5
#define FUNCCODE_vfVoltageSrc_PLC           6
#define FUNCCODE_vfVoltageSrc_PID           7
#define FUNCCODE_vfVoltageSrc_COMM          8

#define FUNCCODE_posCtrl_NONE               0
#define FUNCCODE_posCtrl_POSITION_CTRL      1
#define FUNCCODE_posCtrl_SWITCH_TO_PC       2
#define FUNCCODE_posCtrl_SWITCH_FROM_PC     3

#define FUNCCODE_pcMode_PCMD            0
#define FUNCCODE_pcMode_APTP            1
#define FUNCCODE_pcMode_SWITCH_TO_APTP  2

#define FUNCCODE_pcPulseType_PULSE_AND_DIR  0
#define FUNCCODE_pcPulseType_QUADRATURE     1
#define FUNCCODE_pcPulseType_CW_AND_CCW     2

#define FUNCCODE_aptpMode_RELATIVE      0
#define FUNCCODE_aptpMode_ABSOLUTE      1
#define FUNCCODE_aptpMode_INDEX         2

#define FUNCCODE_pcZeroSelect_ENCODER   0
#define FUNCCODE_pcZeroSelect_DI        1

#define FUNCCODE_pgLocation_MOTOR       0
#define FUNCCODE_pgLocation_AXIS        1

#define FUNCCODE_motorCtrlMode_SVC  0
#define FUNCCODE_motorCtrlMode_FVC  1
#define FUNCCODE_motorCtrlMode_VF   2

#define FUNCCODE_runSrc_PANEL       0
#define FUNCCODE_runSrc_DI          1
#define FUNCCODE_runSrc_COMM        2
#define FUNCCODE_runSrc_AUTO_RUN    3

#define FUNCCODE_runDir_NO_REVERSE      0
#define FUNCCODE_runDir_REVERSE         1

#define FUNCCODE_tuneCmd_NONE           0
#define FUNCCODE_tuneCmd_ACI_STATIC     1
#define FUNCCODE_tuneCmd_ACI_WHOLE      2
#define FUNCCODE_tuneCmd_PMSM_11        11
#define FUNCCODE_tuneCmd_PMSM_12        12
#define FUNCCODE_tuneCmd_PMSM_13        13

#define FUNCCODE_diControlMode_2LINE1   0
#define FUNCCODE_diControlMode_2LINE2   1
#define FUNCCODE_diControlMode_3LINE1   2
#define FUNCCODE_diControlMode_3LINE2   3

#define FUNCCODE_fmOutSelect_PULSE      0
#define FUNCCODE_fmOutSelect_DO         1
#define FUNCCODE_fmOutSelect_AO         2

#define FUNCCODE_startMode_DIRECT_START 0
#define FUNCCODE_startMode_SPEED_TRACK  1
#define FUNCCODE_startMode_FORE_MAG     2

#define FUNCCODE_stopMode_DEC_STOP      0
#define FUNCCODE_stopMode_FREESTOP      1
#define FUNCCODE_stopMode_HURRY_STOP    2

#define FUNCCODE_lowerDeal_RUN_LOWER    0
#define FUNCCODE_lowerDeal_DELAY_STOP   1
#define FUNCCODE_lowerDeal_RUN_ZERO     2

#define FUNCCODE_runTimeOverAct_RUN     0
#define FUNCCODE_runTimeOverAct_STOP    1

#define FUNCCODE_powerUpTimeOverAct_RUN     0
#define FUNCCODE_powerUpTimeOverAct_STOP    1

#define FUNCCODE_pidSetSrc_FC               0
#define FUNCCODE_pidSetSrc_AI1              1
#define FUNCCODE_pidSetSrc_AI2              2
#define FUNCCODE_pidSetSrc_AI3              3
#define FUNCCODE_pidSetSrc_PULSE            4
#define FUNCCODE_pidSetSrc_COMM             5
#define FUNCCODE_pidSetSrc_MULTI_SET        6

#define FUNCCODE_pidFdbSrc_AI1              0
#define FUNCCODE_pidFdbSrc_AI2              1
#define FUNCCODE_pidFdbSrc_AI3              2
#define FUNCCODE_pidFdbSrc_AI1_SUB_AI2      3
#define FUNCCODE_pidFdbSrc_PULSE            4
#define FUNCCODE_pidFdbSrc_COMM             5
#define FUNCCODE_pidFdbSrc_AI1_ADD_AI2      6
#define FUNCCODE_pidFdbSrc_MAX_AI           7
#define FUNCCODE_pidFdbSrc_MIN_AI           8

#define FUNCCODE_pidParaChgCondition_NO         0
#define FUNCCODE_pidParaChgCondition_DI         1
#define FUNCCODE_pidParaChgCondition_PID_ERROR  2

#define FUNCCODE_pidCalcMode_NO             0
#define FUNCCODE_pidCalcMode_YES            1

#define FUNCCODE_swingBaseMode_AGAIN_FRQSETAIM  0
#define FUNCCODE_swingBaseMode_AGAIN_MAXFRQ     1

#define FUNCCODE_mfkKeyFunc_NONE        0
#define FUNCCODE_mfkKeyFunc_SWITCH      1
#define FUNCCODE_mfkKeyFunc_REVERSE     2
#define FUNCCODE_mfkKeyFunc_FWD_JOG     3
#define FUNCCODE_mfkKeyFunc_REV_JOG     4

#define FUNCCODE_stopKeyFunc_KEYBOARD   0
#define FUNCCODE_stopKeyFunc_ALL        1

#define FUNCCODE_plcFrq0Src_FC          0
#define FUNCCODE_plcFrq0Src_AI1         1
#define FUNCCODE_plcFrq0Src_AI2         2
#define FUNCCODE_plcFrq0Src_AI3         3
#define FUNCCODE_plcFrq0Src_PULSE       4
#define FUNCCODE_plcFrq0Src_PID         5
#define FUNCCODE_plcFrq0Src_PRESET_FRQ  6

#define FUNCCODE_plcRunMode_ONCE_STOP   0
#define FUNCCODE_plcRunMode_ONCE_RUN    1
#define FUNCCODE_plcRunMode_REPEAT      2

#define FUNCCODE_plcPowerOffRemMode_NO_REM  0
#define FUNCCODE_plcPowerOffRemMode_REM     1

#define FUNCCODE_plcStopRemMode_NO_REM  0
#define FUNCCODE_plcStopRemMode_REM     1

#define FUNCCODE_plcTimeUnit_S      0
#define FUNCCODE_plcTimeUnit_H      1

#define FUNCCODE_tempSenorType_NONE         0
#define FUNCCODE_tempSenorType_PTC100       1
#define FUNCCODE_tempSenorType_PTC1000      2
#define FUNCCODE_tempSenorType_NTC          3

#define FUNCCODE_sciParity_NONE     0
#define FUNCCODE_sciParity_EVEN     1
#define FUNCCODE_sciParity_ODD      2
#define FUNCCODE_sciParity_NONE1    3

#define TIME_UNIT_SEC_PER_HOUR          3600
#define TIME_UNIT_MIN_PER_HOUR          60
#define TIME_UNIT_SEC_PER_MIN           60
#define TIME_UNIT_MS_PER_SEC            1000

#define TIME_UNIT_VF_VOL_ACC_TIME       100

#define TIME_UNIT_AI_PULSE_IN_FILTER    10
#define TIME_UNIT_DI_DELAY              100
#define TIME_UNIT_DO_DELAY              100
#define TIME_UNIT_START_FRQ_WAIT        100
#define TIME_UNIT_START_BRAKE           100
#define TIME_UNIT_WAIT_STOP_BRAKE       100
#define TIME_UNIT_STOP_BRAKE            100
#define TIME_UNIT_ZERO_SPEED_DEAD       100
#define TIME_UNIT_LOWER_STOP_DELAY      100
#define TIME_UNIT_PID_SET_CHANGE        10
#define TIME_UNIT_PID_FILTER            10
#define TIME_UNIT_PID_INIT              10
#define TIME_UNIT_PID_FDB_LOSE          100
#define TIME_UNIT_SWING_PERIOD          100
#define TIME_UNIT_sciCommOverTime       100
#define TIME_UNIT_ERR_AUTO_RST_DELAY    100
#define TIME_UNIT_ERR_AUTO_RST_CLR      (TIME_UNIT_SEC_PER_HOUR*100UL)
#define TIME_UNIT_P_OFF_VOL_BACK        10
#define TIME_UNIT_PLC                   100

#define TIME_UNIT_ACC_DEC_SPEED_SERVO   10
#define TIME_UNIT_WAKE_UP               100
#define TIME_UNIT_DORMANT               100
#define TIME_UNIT_CURRENT_CHK           10
#define TIME_UNIT_TORQUE_CTRL_ACC_DEC   10

#if 0
#define ACC_DEC_T_INIT1  ((Uint32)2*TIME_UNIT_MS_PER_SEC/TIME_UNIT_ACC_DEC_SPEED)
#define ACC_DEC_T_INIT2  ((Uint32)5*TIME_UNIT_MS_PER_SEC/TIME_UNIT_ACC_DEC_SPEED)
#else
#define ACC_DEC_T_INIT1  ((Uint32)10*TIME_UNIT_MS_PER_SEC/TIME_UNIT_ACC_DEC_SPEED)
#define ACC_DEC_T_INIT2  ((Uint32)30*TIME_UNIT_MS_PER_SEC/TIME_UNIT_ACC_DEC_SPEED)
#endif

#define RATING_FRQ_INIT_0   50
#define RATING_FRQ_INIT_1   500
#define RATING_FRQ_INIT_2   5000
#define BAUD_NUM_MAX        12

#define PARA_INIT_MODE_MAX  501
#define INV_TYPE_MAX   30

#define MENU_MODE_MAX       3

#define FACTORY_PWD_INDEX      (GetCodeIndex(funcCode.code.factoryPassword))
#define INV_TYPE_INDEX         (GetCodeIndex(funcCode.code.inverterType))
#define RATING_POWER_INVERTER_INDEX  (GetCodeIndex(funcCode.code.inverterPower))
#define FUNCCODE_FACTORY_START_INDEX     (GetCodeIndex(funcCode.group.ff[0]))
#define FUNCCODE_FACTORY_END_INDEX       (GetCodeIndex(funcCode.group.ff[FFNUM - 1]))

#define FC_MOTOR1_START_INDEX   (GetCodeIndex(funcCode.code.motorParaM1.all[0]))
#define FC_MOTOR1_END_INDEX     (GetCodeIndex(funcCode.code.pgParaM1.all[sizeof(struct PG_PARA_STRUCT) - 1]))

#define FC_MOTOR2_START_INDEX   (GetCodeIndex(funcCode.code.motorFcM2.motorPara.all[0]))
#define FC_MOTOR2_END_INDEX     (GetCodeIndex(funcCode.code.motorFcM2.pgPara.all[sizeof(struct PG_PARA_STRUCT) - 1]))

#define FC_MOTOR3_START_INDEX   (GetCodeIndex(funcCode.code.motorFcM3.motorPara.all[0]))
#define FC_MOTOR3_END_INDEX     (GetCodeIndex(funcCode.code.motorFcM3.pgPara.all[sizeof(struct PG_PARA_STRUCT) - 1]))

#define FC_MOTOR4_START_INDEX   (GetCodeIndex(funcCode.code.motorFcM4.motorPara.all[0]))
#define FC_MOTOR4_END_INDEX     (GetCodeIndex(funcCode.code.motorFcM4.pgPara.all[sizeof(struct PG_PARA_STRUCT) - 1]))

#define PRESET_FRQ_INDEX        (GetCodeIndex(funcCode.code.presetFrq))
#define MAX_FRQ_INDEX           (GetCodeIndex(funcCode.code.maxFrq))
#define UPPER_FRQ_INDEX         (GetCodeIndex(funcCode.code.upperFrq))
#define LOWER_FRQ_INDEX         (GetCodeIndex(funcCode.code.lowerFrq))
#define ACC_TIME1_INDEX         (GetCodeIndex(funcCode.code.accTime1))
#define DEC_TIME1_INDEX         (GetCodeIndex(funcCode.code.decTime1))
#define CARRIER_FRQ_INDEX       (GetCodeIndex(funcCode.code.carrierFrq))

#define RATING_POWER_INDEX      (GetCodeIndex(funcCode.code.motorParaM1.elem.ratingPower))
#define RATING_VOL_INDEX        (GetCodeIndex(funcCode.code.motorParaM1.elem.ratingVoltage))
#define RATING_CUR_INDEX        (GetCodeIndex(funcCode.code.motorParaM1.elem.ratingCurrent))
#define RATING_CUR_INDEX2       (GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.ratingCurrent))
#define RATING_CUR_INDEX3       (GetCodeIndex(funcCode.code.motorFcM3.motorPara.elem.ratingCurrent))
#define RATING_CUR_INDEX4       (GetCodeIndex(funcCode.code.motorFcM4.motorPara.elem.ratingCurrent))

#define RATING_FRQ_INDEX        (GetCodeIndex(funcCode.code.motorParaM1.elem.ratingFrq))

#define ZERO_LOAD_CURRENT_INDEX (GetCodeIndex(funcCode.code.motorParaM1.elem.zeroLoadCurrent))
#define STATOR_RESISTANCE_INDEX (GetCodeIndex(funcCode.code.motorParaM1.elem.statorResistance))

#define ANTI_VIBRATE_GAIN_INDEX (GetCodeIndex(funcCode.code.antiVibrateGain))
#define ANTI_VIBRATE_GAIN_MOTOR2_INDEX (GetCodeIndex(funcCode.code.motorFcM2.antiVibrateGain))
#define ANTI_VIBRATE_GAIN_MOTOR3_INDEX (GetCodeIndex(funcCode.code.motorFcM3.antiVibrateGain))
#define ANTI_VIBRATE_GAIN_MOTOR4_INDEX (GetCodeIndex(funcCode.code.motorFcM4.antiVibrateGain))

#define TUNE_CMD_INDEX_1  (GetCodeIndex(funcCode.code.tuneCmd))
#define TUNE_CMD_INDEX_2  (GetCodeIndex(funcCode.code.motorFcM2.tuneCmd))
#define TUNE_CMD_INDEX_3  (GetCodeIndex(funcCode.code.motorFcM3.tuneCmd))
#define TUNE_CMD_INDEX_4  (GetCodeIndex(funcCode.code.motorFcM4.tuneCmd))

#define VC_CHG_FRQ1_INDEX (GetCodeIndex(funcCode.code.vcParaM1.vcSpdLoopChgFrq1))
#define VC_CHG_FRQ2_INDEX (GetCodeIndex(funcCode.code.vcParaM1.vcSpdLoopChgFrq2))

#define VC_CHG_FRQ1_INDEX2 (GetCodeIndex(funcCode.code.motorFcM2.vcPara.vcSpdLoopChgFrq1))
#define VC_CHG_FRQ2_INDEX2 (GetCodeIndex(funcCode.code.motorFcM2.vcPara.vcSpdLoopChgFrq2))

#define VC_CHG_FRQ1_INDEX3 (GetCodeIndex(funcCode.code.motorFcM3.vcPara.vcSpdLoopChgFrq1))
#define VC_CHG_FRQ2_INDEX3 (GetCodeIndex(funcCode.code.motorFcM3.vcPara.vcSpdLoopChgFrq2))

#define VC_CHG_FRQ1_INDEX4 (GetCodeIndex(funcCode.code.motorFcM4.vcPara.vcSpdLoopChgFrq1))
#define VC_CHG_FRQ2_INDEX4 (GetCodeIndex(funcCode.code.motorFcM4.vcPara.vcSpdLoopChgFrq2))

#define TORQUE_BOOST_INDEX      (GetCodeIndex(funcCode.code.torqueBoost))

#define TORQUE_BOOST_MOTOR2_INDEX      (GetCodeIndex(funcCode.code.motorFcM2.torqueBoost))
#define TORQUE_BOOST_MOTOR3_INDEX      (GetCodeIndex(funcCode.code.motorFcM3.torqueBoost))
#define TORQUE_BOOST_MOTOR4_INDEX      (GetCodeIndex(funcCode.code.motorFcM4.torqueBoost))

#define SVC_MODE_INDX           (GetCodeIndex(funcCode.code.svcMode))
#define OV_POINT_SET_INDEX      (GetCodeIndex(funcCode.code.ovPointSet))

#define VF_FRQ1_INDEX           (GetCodeIndex(funcCode.code.vfFrq1))
#define VF_FRQ2_INDEX           (GetCodeIndex(funcCode.code.vfFrq2))
#define VF_FRQ3_INDEX           (GetCodeIndex(funcCode.code.vfFrq3))

#define CURVE1_MIN_INDEX        (GetCodeIndex(funcCode.code.curveSet2P1[0]))
#define CURVE1_MAX_INDEX        (GetCodeIndex(funcCode.code.curveSet2P1[2]))
#define CURVE2_MIN_INDEX        (GetCodeIndex(funcCode.code.curveSet2P2[0]))
#define CURVE2_MAX_INDEX        (GetCodeIndex(funcCode.code.curveSet2P2[2]))
#define CURVE3_MIN_INDEX        (GetCodeIndex(funcCode.code.curveSet2P3[0]))
#define CURVE3_MAX_INDEX        (GetCodeIndex(funcCode.code.curveSet2P3[2]))
#define PULSE_IN_MIN_INDEX      (GetCodeIndex(funcCode.code.curveSet2P4[0]))
#define PULSE_IN_MAX_INDEX      (GetCodeIndex(funcCode.code.curveSet2P4[2]))

#define CURVE4_MIN_INDEX        (GetCodeIndex(funcCode.code.curveSet4P1[0]))
#define CURVE4_INFLEX1_INDEX    (GetCodeIndex(funcCode.code.curveSet4P1[2]))
#define CURVE4_INFLEX2_INDEX    (GetCodeIndex(funcCode.code.curveSet4P1[4]))
#define CURVE4_MAX_INDEX        (GetCodeIndex(funcCode.code.curveSet4P1[6]))
#define CURVE5_MIN_INDEX        (GetCodeIndex(funcCode.code.curveSet4P2[0]))
#define CURVE5_INFLEX1_INDEX    (GetCodeIndex(funcCode.code.curveSet4P2[2]))
#define CURVE5_INFLEX2_INDEX    (GetCodeIndex(funcCode.code.curveSet4P2[4]))
#define CURVE5_MAX_INDEX        (GetCodeIndex(funcCode.code.curveSet4P2[6]))

#define ACC_TIME2_INDEX         (GetCodeIndex(funcCode.code.accTime2))
#define DEC_TIME2_INDEX         (GetCodeIndex(funcCode.code.decTime2))

#define ACC_TIME3_INDEX         (GetCodeIndex(funcCode.code.accTime3))
#define DEC_TIME3_INDEX         (GetCodeIndex(funcCode.code.decTime3))

#define ACC_TIME4_INDEX         (GetCodeIndex(funcCode.code.accTime4))
#define DEC_TIME4_INDEX         (GetCodeIndex(funcCode.code.decTime4))

#define RUN_TIME_ADDUP_INDEX    (GetCodeIndex(funcCode.code.runTimeAddup))
#define POWER_TIME_ADDUP_INDEX  (GetCodeIndex(funcCode.code.powerUpTimeAddup))
#define POWER_ADDUP_INDEX       (GetCodeIndex(funcCode.code.powerAddup))

#define AI1_LIMIT               (GetCodeIndex(funcCode.code.ai1VoltageLimit))
#define AI1_UPPER               (GetCodeIndex(funcCode.code.ai1VoltageUpper))

#define PID_PARA_CHG_DELTA1_MAX (GetCodeIndex(funcCode.code.pidParaChgDelta2))
#define PID_PARA_CHG_DELTA2_MIN (GetCodeIndex(funcCode.code.pidParaChgDelta1))

#define DORMANT_UPPER           (GetCodeIndex(funcCode.code.wakeUpFrq))
#define WAKE_UP_LIMIT           (GetCodeIndex(funcCode.code.dormantFrq))
#define RADIATOR_TEMP_INDEX     (GetCodeIndex(funcCode.code.radiatorTemp))
#define ERROR_LATEST1_INDEX     (GetCodeIndex(funcCode.code.errorLatest1))
#define ERROR_LATEST2_INDEX     (GetCodeIndex(funcCode.code.errorLatest2))
#define ERROR_LATEST3_INDEX     (GetCodeIndex(funcCode.code.errorLatest3))
#define ERROR_FRQ_INDEX         (GetCodeIndex(funcCode.code.errorScene3.elem.errorFrq))
#define ERROR_CURRENT_INDEX     (GetCodeIndex(funcCode.code.errorScene3.elem.errorCurrent))
#define ERROR_UDC_INDEX         (GetCodeIndex(funcCode.code.errorScene3.elem.errorGeneratrixVoltage))
#define ERROR_DI_STATUS_INDEX   (GetCodeIndex(funcCode.code.errorScene3.elem.errorDiStatus))
#define ERROR_DO_STATUS_INDEX   (GetCodeIndex(funcCode.code.errorScene3.elem.errorDoStatus))
#define LAST_ERROR_RECORD_INDEX (GetCodeIndex(funcCode.code.errorScene1.all[sizeof(struct ERROR_SCENE_STRUCT) - 1]))

#define MIN_CBC_TIME_INDEX       (GetCodeIndex(funcCode.code.cbcMinTime))
#define MAX_CBC_TIME_INDEX       (GetCodeIndex(funcCode.code.cbcMaxTime))

#define PC_LOOP_CHG_FRQ1_I      (GetCodeIndex(funcCode.code.pcLoopChgFrq1))
#define PC_LOOP_CHG_FRQ2_I      (GetCodeIndex(funcCode.code.pcLoopChgFrq2))

#define EEPROM_CHECK_INDEX      (GetCodeIndex(funcCode.code.eepromCheckWord1))

#define RUN_TIME_ADDUP_SEC_INDEX    (GetCodeIndex(funcCode.code.runTimeAddupSec))

#define EEPROM_CHECK_INDEX1     (GetCodeIndex(funcCode.code.eepromCheckWord1))
#define EEPROM_CHECK_INDEX2     (GetCodeIndex(funcCode.code.eepromCheckWord2))

#define SAVE_USER_PARA_PARA1    (GetCodeIndex(funcCode.code.saveUserParaFlag1))
#define SAVE_USER_PARA_PARA2    (GetCodeIndex(funcCode.code.saveUserParaFlag2))

#define AI_AO_CHK_FLAG          (GetCodeIndex(funcCode.code.aiaoChkWord))
#define AI_AO_CALIB_START       (GetCodeIndex(funcCode.code.aiFactoryCalibrateCurve[0].before1))
#define AI_AO_CALIB_STOP        (GetCodeIndex(funcCode.code.aoFactoryCalibrateCurve[0].after2))

#define AI1_CALB_START          (GetCodeIndex(funcCode.code.aiFactoryCalibrateCurve[0].before1))
#define AI2_CALB_STOP           (GetCodeIndex(funcCode.code.aiFactoryCalibrateCurve[1].after2))
#define AO1_CALB_START          (GetCodeIndex(funcCode.code.aoFactoryCalibrateCurve[0].before1))
#define AO1_CALB_STOP           (GetCodeIndex(funcCode.code.aoFactoryCalibrateCurve[0].after2))

#define FC_GROUP_FACTORY    FUNCCODE_GROUP_FF
#define FC_GROUP_FC_MANAGE  FUNCCODE_GROUP_FP
#define FC_START_GROUP      FUNCCODE_GROUP_F0

extern FUNCCODE_ALL funcCode;

extern const Uint16 funcCodeGradeSum[];

extern Uint16 saveEepromIndex;
extern const Uint16 ovVoltageInitValue[];
extern const Uint16 funcCodeGradeAll[];
extern Uint16 funcCodeGradeCurMenuMode[];

#endif
