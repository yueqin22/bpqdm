/* Shared symbol list for the MotorDataExchange differential test.
 *
 * Both the mock (which must DEFINE these globals) and the driver (which must
 * seed, snapshot and compare them) need the same list. Keeping it in one
 * X-macro means the two can never drift apart -- a global added to the mock but
 * forgotten in the driver would otherwise silently go uncompared.
 *
 * The list is NOT hand-transcribed from the source. It is exactly the set of
 * undefined (U) symbols that
 *
 *     nm2000 build/Debug/MotorDataExchange.obj
 *
 * prints -- 49 names, all data, no functions. The module calls nothing.
 *
 * The module's OWN seven exports (ParSend05Ms / ParGet05Ms / ParSend2Ms /
 * ParGet2Ms / ParSendTune and the five ...DataBuff arrays plus uReservedData and
 * gSoftVersion) are deliberately absent: they are renamed per side by
 * ref_tu.c / new_tu.c and therefore live in the module's own translation unit,
 * not in this shared mock.
 */
#ifndef MOCK_MDE_H
#define MOCK_MDE_H

#include "MotorInclude.h"

/* MotorDataExchange.c declares this one itself, on its own line 23, rather than
 * getting it from a header:
 *
 *     extern ALPHABETA_STRUCT gABFluxS_LowFilter;
 *
 * so neither the mock nor the driver would see a declaration for it otherwise.
 * gABFluxS_comp is declared right below it in the source and is NOT listed here
 * or in MDE_EXTERN_GLOBALS: it is never referenced, generates no symbol
 * reference, and nm2000 confirms it is absent from the object's undefined set. */
extern ALPHABETA_STRUCT gABFluxS_LowFilter;

#define MDE_EXTERN_GLOBALS(F)                     \
    F(gADC,                  ADC_STRUCT)                     \
    F(gAI,                   AI_STRUCT)                      \
    F(gBasePar,              BASE_PAR_STRUCT)                \
    F(gComPar,               COM_PAR_INFO_STRUCT)            \
    F(gCpuTime,              CPU_TIME_STRUCT)                \
    F(gDeadBand,             DEAD_BAND_STRUCT)               \
    F(gError,                struct FAULT_CODE_INFOR_STRUCT_DEF) \
    F(gEstBemf,              PMSM_EST_BEMF)                  \
    F(gExtendCmd,            MAIN_COMMAND_EXTEND_UNION)      \
    F(gFVCSpeed,             FVC_SPEED_STRUCT)               \
    F(gFluxR,                FLUX_STRUCT)                    \
    F(gFluxWeak,             PM_FLUX_WEAK)                   \
    F(gGetParVarable,        MOTOR_PARA_EST)                 \
    F(gHVfOscIndex,          HVF_OSC_JUDGE_INDEX)            \
    F(gIAmpTheta,            AMPTHETA_STRUCT)                \
    F(gIMTQ12,               MT_STRUCT)                      \
    F(gIMTQ12_obs,           MT_STRUCT_Q24)                  \
    F(gIMTSetQ12,            MT_STRUCT)                      \
    F(gIPMPos,               IPM_POSITION_STRUCT)            \
    F(gIPMPosCheck,          IPM_POS_CHECK_STRUCT)           \
    F(gIUVWQ12,              UVW_STRUCT)                     \
    F(gInvInfo,              INV_STRUCT)                     \
    F(gLineCur,              LINE_CURRENT_STRUCT)            \
    F(gLoadLose,             LOAD_LOSE_STRUCT)               \
    F(gMainCmd,              BASE_COMMAND_STRUCT)            \
    F(gMainStatus,           RUN_STATUS_STRUCT)              \
    F(gMotorExtInfo,         MOTOR_EXTERN_STRUCT)            \
    F(gMotorExtPer,          MOTOR_EXTERN_STRUCT)            \
    F(gMotorExtReg,          MOTOR_EXTERN_STRUCT)            \
    F(gMotorInfo,            MOTOR_STRUCT)                   \
    F(gOutVolt,              OUT_VOLT_STRUCT)                \
    F(gPGData,               PG_DATA_STRUCT)                 \
    F(gPMInitPos,            PM_INIT_POSITION)               \
    F(gPWM,                  PWM_OUT_STRUCT)                 \
    F(gPhase,                ANGLE_STRUCT)                   \
    F(gPmParEst,             PMSM_EST_PARAM_DATA)            \
    F(gPowerTrq,             MOTOR_POWER_TORQUE)             \
    F(gRatio,                Uint)                           \
    F(gRotorTrans,           ROTOR_TRANS_STRUCT)             \
    F(gSubCommand,           SUB_COMMAND_UNION)              \
    F(gTemperature,          TEMPLETURE_STRUCT)              \
    F(gTestDataReceive,      MOTOR_DEBUG_DATA_RECEIVE_STRUCT) \
    F(gUDC,                  UDC_STRUCT)                      \
    F(gUMTSet,               MT_STRUCT)                      \
    F(gUVCoff,               UV_BIAS_COFF_STRUCT)            \
    F(gUVWPG,                IPM_UVW_PG_STRUCT)              \
    F(gVCPar,                VC_INFO_STRUCT)                 \
    F(gVFPar,                VF_INFO_STRUCT)                 \
    F(gABFluxS_LowFilter,    ALPHABETA_STRUCT)

#endif
