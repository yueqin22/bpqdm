/* Shared mock for the MotorVF differential test.
 *
 * src/motor/MotorVF.c DEFINES fifteen data objects of its own (gVFPar,
 * gVFAutoVar, gWsComp, gVarAvr, gVfOsc, gOscAmp, gVfOverUdc, gOvCur2,
 * gVfFreq2, gOvCur, gOvUdc, gHVfOscDamp, gHVfOscIndex, gHVfDeadBandCompOpt,
 * gHVfCur).  ref_tu.c / new_tu.c rename those per side, so each translation
 * unit owns a full set and they are NOT declared here.
 *
 * Everything else the module references but does not define is in this file,
 * one instance each, shared by both sides:
 *
 *   1. the 19 globals owned by other translation units.  The list is exactly
 *      the DATA entries of
 *          nm2000 -u build/Debug/MotorVF.obj
 *      (19 `_g...` lines; the remaining undefined entries are the five library
 *      helpers and the four integer-divide runtimes, see section 3).  Their
 *      types come from the real project headers -- no hand-written stand-ins --
 *      so field widths and layout are the firmware's.  That matters because the
 *      case shim folds `int` to the target's 16 bits (see
 *      shim/DSP2803x_Device.h): `Uint` storage and `int` storage must both
 *      match the C28x or a truncation-only mutation would hide.
 *
 *   2. gExponentVf.  `extern Uint const gExponentVf[][129];` is declared in
 *      MotorVFInclude.h and DEFINED in src/motor/MotorConstant.c, so MotorVF
 *      only reads it.  The initializer below is copied verbatim out of
 *      MotorConstant.c -- it is *data*, not code under test, and the four
 *      curves are what CalOutVotInVFStatus interpolates for VFLineType 3..8.
 *      Reproducing it is the whole point: a hand-written "plausible" curve would
 *      let a wrong `ptV` mapping look correct.
 *
 *   3. faithful ports of the two callees, PID() and AlphBetaToDQ().  Both live
 *      in other translation units (src/subprogram/SubPrg.c and
 *      src/motor/MotorCurrentTransform.c) and both are on a *feedback* path:
 *        - PID() mutates gVFAutoVar.AutoTorquePID.Total/.Out, and
 *          CalTorqueUp reads `AutoTorquePID.Out >> 16`.  A no-op mock would
 *          freeze that input at 0 and make every mutation downstream of it
 *          unobservable.
 *        - AlphBetaToDQ() produces the m_NewMT that becomes
 *          gWsComp.WsCompMT.T/.M, which is module state that gets compared.
 *      So they are ported rather than stubbed.  See the notes above each port
 *      for the one thing that could not be carried over (the C28x OVM flag).
 *
 * The library helpers (abs / qsin / qsqrt / atan) are NOT here: they come from
 * the shared hostlib_mock.c / atan_mock.c.  abs is inlined on the target, so it
 * does not even appear in the undefined set; qsin / qsqrt / atan do.
 */

#include "MotorInclude.h"

/* ---- 0. CPU control sequence --------------------------------------------- *
 * The case shim turns EINT / DINT / EALLOW / EDIS ... into increments of this
 * table.  MotorVF.c uses none of them today (verified: no EALLOW, DINT, EINT or
 * EDIS anywhere in the file), so nothing increments it.  It is defined anyway
 * because the shim declares it `extern` and a future edit that adds a critical
 * section should not fail to LINK -- that failure would be reported as "the
 * mock is incomplete" when the real finding is "the module started bracketing
 * something".  The driver does not compare it, for the same reason.
 */
volatile Uint16 gHostCpuCtl[7];

/* ---- 1. globals owned by other translation units ------------------------- */

BASE_PAR_STRUCT              gBasePar;
CONTROL_MOTOR_TYPE_ENUM      gCtrMotorType;
DEAD_BAND_STRUCT             gDeadBand;
MAIN_COMMAND_EXTEND_UNION    gExtendCmd;
ALPHABETA_STRUCT             gIAlphBeta;
AMPTHETA_STRUCT              gIAmpTheta;
MT_STRUCT                    gIMTQ12;
MT_STRUCT_Q24                gIMTQ24;
INV_STRUCT                   gInvInfo;
LINE_CURRENT_STRUCT          gLineCur;
BASE_COMMAND_STRUCT          gMainCmd;
RUN_STATUS_STRUCT            gMainStatus;
MOTOR_EXTERN_STRUCT          gMotorExtInfo;
MOTOR_EXTERN_STRUCT          gMotorExtPer;
MOTOR_STRUCT                 gMotorInfo;
OUT_VOLT_STRUCT              gOutVolt;
ANGLE_STRUCT                 gPhase;
UDC_STRUCT                   gUDC;

/* ---- 2. the V/f exponent curves ----------------------------------------- *
 * Copied verbatim from src/motor/MotorConstant.c.  Read-only (`Uint const`), so
 * it is shared by both sides without any risk of one side writing to it.
 * 4 rows x 129 entries.  Row index is ptV, derived from VFLineType in
 * CalOutVotInVFStatus:
 *      VFLineType 3 -> 0,  4 -> 1,  5 -> 2,  6 -> 2,
 *      VFLineType 7 -> 3,  8 -> 3
 * (that is what the `if(ptV == 3) ptV = 2; if(ptV == 5) ptV = 3;` remap does
 * after `ptV = VFLineType - 3`).
 */
Uint const gExponentVf[4][129] =
{

    {
        0   ,
        12  ,
        28  ,
        45  ,
        64  ,
        84  ,
        104 ,
        125 ,
        147 ,
        169 ,
        192 ,
        215 ,
        239 ,
        263 ,
        288 ,
        313 ,
        338 ,
        363 ,
        389 ,
        415 ,
        442 ,
        468 ,
        495 ,
        522 ,
        549 ,
        577 ,
        605 ,
        633 ,
        661 ,
        690 ,
        718 ,
        747 ,
        776 ,
        805 ,
        835 ,
        864 ,
        894 ,
        924 ,
        954 ,
        984 ,
        1014,
        1045,
        1075,
        1106,
        1137,
        1168,
        1200,
        1231,
        1262,
        1294,
        1326,
        1358,
        1390,
        1422,
        1454,
        1486,
        1519,
        1552,
        1584,
        1617,
        1650,
        1683,
        1716,
        1750,
        1783,
        1816,
        1850,
        1884,
        1917,
        1951,
        1985,
        2019,
        2054,
        2088,
        2122,
        2157,
        2191,
        2226,
        2261,
        2295,
        2330,
        2365,
        2400,
        2436,
        2471,
        2506,
        2542,
        2577,
        2613,
        2648,
        2684,
        2720,
        2756,
        2792,
        2828,
        2864,
        2900,
        2937,
        2973,
        3009,
        3046,
        3082,
        3119,
        3156,
        3193,
        3229,
        3266,
        3303,
        3341,
        3378,
        3415,
        3452,
        3490,
        3527,
        3564,
        3602,
        3640,
        3677,
        3715,
        3753,
        3791,
        3829,
        3867,
        3905,
        3943,
        3981,
        4019,
        4058,
        4096,
    },

    {
        0   ,
        5   ,
        12  ,
        21  ,
        32  ,
        44  ,
        56  ,
        70  ,
        84  ,
        100 ,
        115 ,
        132 ,
        149 ,
        167 ,
        185 ,
        204 ,
        223 ,
        243 ,
        263 ,
        283 ,
        305 ,
        326 ,
        348 ,
        370 ,
        393 ,
        416 ,
        440 ,
        464 ,
        488 ,
        512 ,
        537 ,
        563 ,
        588 ,
        614 ,
        640 ,
        667 ,
        694 ,
        721 ,
        748 ,
        776 ,
        804 ,
        832 ,
        861 ,
        889 ,
        919 ,
        948 ,
        978 ,
        1007,
        1038,
        1068,
        1099,
        1129,
        1161,
        1192,
        1224,
        1255,
        1287,
        1320,
        1352,
        1385,
        1418,
        1451,
        1485,
        1518,
        1552,
        1586,
        1620,
        1655,
        1690,
        1724,
        1760,
        1795,
        1830,
        1866,
        1902,
        1938,
        1974,
        2011,
        2047,
        2084,
        2121,
        2158,
        2196,
        2233,
        2271,
        2309,
        2347,
        2386,
        2424,
        2463,
        2502,
        2541,
        2580,
        2619,
        2659,
        2698,
        2738,
        2778,
        2818,
        2859,
        2899,
        2940,
        2981,
        3022,
        3063,
        3104,
        3146,
        3187,
        3229,
        3271,
        3313,
        3355,
        3398,
        3440,
        3483,
        3526,
        3569,
        3612,
        3655,
        3699,
        3742,
        3786,
        3830,
        3874,
        3918,
        3962,
        4007,
        4051,
        4096,
    },

    {
        0   ,
        2   ,
        5   ,
        10  ,
        16  ,
        23  ,
        31  ,
        39  ,
        49  ,
        59  ,
        69  ,
        81  ,
        93  ,
        105 ,
        119 ,
        133 ,
        147 ,
        162 ,
        178 ,
        194 ,
        210 ,
        227 ,
        245 ,
        263 ,
        281 ,
        300 ,
        320 ,
        340 ,
        360 ,
        381 ,
        402 ,
        424 ,
        446 ,
        468 ,
        491 ,
        514 ,
        538 ,
        562 ,
        587 ,
        612 ,
        637 ,
        663 ,
        689 ,
        715 ,
        742 ,
        769 ,
        797 ,
        824 ,
        853 ,
        881 ,
        910 ,
        940 ,
        969 ,
        999 ,
        1030,
        1060,
        1091,
        1123,
        1154,
        1186,
        1219,
        1251,
        1284,
        1318,
        1351,
        1385,
        1419,
        1454,
        1489,
        1524,
        1559,
        1595,
        1631,
        1668,
        1704,
        1741,
        1779,
        1816,
        1854,
        1892,
        1931,
        1970,
        2009,
        2048,
        2088,
        2128,
        2168,
        2208,
        2249,
        2290,
        2331,
        2373,
        2415,
        2457,
        2499,
        2542,
        2585,
        2628,
        2672,
        2715,
        2759,
        2804,
        2848,
        2893,
        2938,
        2984,
        3029,
        3075,
        3121,
        3167,
        3214,
        3261,
        3308,
        3355,
        3403,
        3451,
        3499,
        3547,
        3596,
        3645,
        3694,
        3744,
        3793,
        3843,
        3893,
        3943,
        3994,
        4045,
        4096,
    },

    {
        0   ,
        1   ,
        2   ,
        5   ,
        8   ,
        12  ,
        17  ,
        22  ,
        28  ,
        34  ,
        42  ,
        49  ,
        58  ,
        67  ,
        76  ,
        86  ,
        97  ,
        108 ,
        120 ,
        132 ,
        145 ,
        158 ,
        172 ,
        186 ,
        201 ,
        217 ,
        232 ,
        249 ,
        266 ,
        283 ,
        301 ,
        319 ,
        338 ,
        357 ,
        377 ,
        397 ,
        418 ,
        439 ,
        460 ,
        482 ,
        505 ,
        528 ,
        551 ,
        575 ,
        599 ,
        624 ,
        649 ,
        675 ,
        701 ,
        727 ,
        754 ,
        782 ,
        809 ,
        838 ,
        866 ,
        895 ,
        925 ,
        955 ,
        985 ,
        1016,
        1047,
        1079,
        1111,
        1143,
        1176,
        1210,
        1243,
        1277,
        1312,
        1347,
        1382,
        1418,
        1454,
        1491,
        1528,
        1565,
        1603,
        1641,
        1679,
        1718,
        1758,
        1797,
        1838,
        1878,
        1919,
        1960,
        2002,
        2044,
        2087,
        2130,
        2173,
        2216,
        2260,
        2305,
        2350,
        2395,
        2440,
        2486,
        2533,
        2579,
        2627,
        2674,
        2722,
        2770,
        2819,
        2868,
        2917,
        2967,
        3017,
        3067,
        3118,
        3169,
        3221,
        3273,
        3325,
        3378,
        3431,
        3484,
        3538,
        3592,
        3647,
        3702,
        3757,
        3813,
        3868,
        3925,
        3982,
        4039,
        4096,
    }
};

/* ---- 3. faithful port of PID() ------------------------------------------ *
 * From src/subprogram/SubPrg.c:
 *
 *     m_KpSat = vMax >> (4 + pid->QP);
 *     m_KiSat = vMax >> pid->QI;
 *     m_OutKp = KP * Deta;  __IQsat(+-m_KpSat);  << (4 + QP)
 *     m_OutKi = KI * Deta;  __IQsat(+-m_KiSat);  << QI
 *     anti-windup when (m_OutKp > m_Max && Total > 0) or
 *                         (m_OutKp < m_Min && Total < 0)
 *     Total += m_OutKi;  Out = __IQsat(Total + m_OutKp, m_Max, m_Min);
 *     Total = __IQsat(Total, m_Max, m_Min);
 *
 * THE ONE DEVIATION: the original brackets the body with SETOVM / CLROVM, i.e.
 * `asm(" SETC OVM")` / `asm(" CLRC OVM")`.  With OVM set, the C28x accumulator
 * SATURATES on shift overflow instead of wrapping.  MSVC has no such mode, and
 * the port therefore wraps.
 *
 * That is exact for every call MotorVF makes, and the reason is measurable
 * rather than hopeful: VFAutoTorqueBoost sets QP = QI = 0, Max = 1228,
 * Min = m_LowerLimit >= 0, KP = 56, KI = 49, and
 *     Deta = VfReverseVolt >> 5   (InvTypeApply > 19), or
 *     Deta = (VfReverseVolt >> 3) & 0xFFFC, or
 *     Deta = (VfReverseVolt >> 1) & 0xFFF0,
 * where VfReverseVolt is itself the output of Filter32 over a value clamped by
 * __IQsat to +-32767, so |Deta| <= 32767.
 *     |m_OutKp| <= 56 * 32767            = 1 835 012  <  2^31 >> 4
 *     m_OutKp << 4 <= 29 360 192         -- four bits of headroom left
 *     |m_OutKi| <= 49 * 32767            = 1 605 583,  << 0
 *     |Total| <= Max<<16 = 1228<<16      = 80 478 208, and it is re-clamped
 *                                          to that range at the end of each call
 * so no shift in this path can overflow.  The saturation mode is unreachable
 * from MotorVF, and the deviation costs nothing.  If PID() is ever ported into
 * another case with QP > 0 or larger KP, this note stops being true -- recheck
 * the bound before reusing it.
 *
 * `__IQsat` is the static model in the case shim (C28x builtin, nothing in
 * include/ or src/ defines it).
 *
 * The ledger below is not needed for correctness -- Total and Out live inside
 * gVFAutoVar.AutoTorquePID, which is module-owned and compared byte-wise, so
 * "the rewrite stopped calling PID" already shows up there.  It exists so the
 * failure message says WHICH callee and with what arguments, instead of a byte
 * offset inside a 40-byte struct.
 */
unsigned short gVfPidCount;
PID_STRUCT     gVfPidArg;      /* *pid as it was handed in, before mutation */

void PID(PID_STRUCT * pid)
{
    long m_Max, m_Min, m_Out, m_OutKp, m_OutKi, m_KpSat, m_KiSat;
    long vMax = 0x7FFFFFFFL;

    gVfPidCount++;
    gVfPidArg = *pid;

    m_KpSat = (vMax >> (4 + pid->QP));
    m_KiSat = (vMax >> pid->QI);
    m_Max   = ((long)pid->Max) << 16;
    m_Min   = ((long)pid->Min) << 16;

    m_OutKp = (long)pid->KP * (long)pid->Deta;
    m_OutKp = __IQsat(m_OutKp, m_KpSat, -(m_KpSat));
    m_OutKp = m_OutKp << (4 + pid->QP);

    m_OutKi = (long)pid->KI * (long)pid->Deta;
    m_OutKi = __IQsat(m_OutKi, m_KiSat, -(m_KiSat));
    m_OutKi = m_OutKi << pid->QI;

    if ((m_OutKp > m_Max) && (pid->Total > 0))
    {
        pid->Total -= (pid->Total >> 8) + 1;
        m_OutKi = 0;
    }
    else if ((m_OutKp < m_Min) && (pid->Total < 0))
    {
        pid->Total -= (pid->Total >> 8) - 1;
        m_OutKi = 0;
    }

    pid->Total += m_OutKi;
    m_Out       = pid->Total + m_OutKp;
    pid->Out    = __IQsat(m_Out, m_Max, m_Min);
    pid->Total  = __IQsat(pid->Total, m_Max, m_Min);
}

/* ---- 4. faithful port of AlphBetaToDQ() --------------------------------- *
 * From src/motor/MotorCurrentTransform.c:
 *
 *     m_sin = qsin(angle);  m_cos = qsin(16384 - angle);
 *     m_M = m_cos * Alph + m_sin * Beta;
 *     m_T = m_cos * Beta - m_sin * Alph;
 *     MT->M = m_M >> 15;  MT->T = m_T >> 15;
 *
 * Ported without deviation: the body is plain 64-bit (`llong`) arithmetic with
 * no asm, no saturation and no library call beyond qsin -- which both sides get
 * from the same hostlib_mock.c.  The `>> 15` lands in a 32-bit `long`, so the
 * narrowing is identical on both targets.
 *
 * Note the argument is a 16-bit `int` (the shim folds it): VFWsTorqueBoostComm
 * passes `(int)(gPhase.IMPhase>>16) +- VfReverseAngle`, and the truncation to 16
 * bits is part of the behaviour under test, not an accident.
 */
unsigned short gVfAbDqCount;
int            gVfAbDqAngle;
long           gVfAbDqAlph;
long           gVfAbDqBeta;

void AlphBetaToDQ(ALPHABETA_STRUCT * AlphBeta, int angle, MT_STRUCT_Q24 * MT)
{
    int   m_sin, m_cos;
    llong m_M, m_T;

    gVfAbDqCount++;
    gVfAbDqAngle = angle;
    gVfAbDqAlph  = AlphBeta->Alph;
    gVfAbDqBeta  = AlphBeta->Beta;

    m_sin = qsin(angle);
    m_cos = qsin(16384 - angle);

    m_M = ((llong)m_cos * (AlphBeta->Alph)) + ((llong)m_sin * (AlphBeta->Beta));
    m_T = ((llong)m_cos * (AlphBeta->Beta)) - ((llong)m_sin * (AlphBeta->Alph));

    MT->M = m_M >> 15;
    MT->T = m_T >> 15;
}
