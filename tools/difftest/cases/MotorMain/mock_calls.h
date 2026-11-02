/* Shared call ledger for the MotorMain differential test.
 *
 * MotorMain.c defines no global data of its own, so every observable it
 * produces is either (a) a write into another module's global, or (b) a call to
 * another module.  (a) is covered by comparing the whole global image; (b) can
 * only be covered by counting the calls.  Without this ledger a rewrite that
 * drops, duplicates or moves a call would pass silently whenever the dropped
 * callee happened not to write shared state.
 *
 * One slot per external callee.  The list is the module object file's undefined
 * FUNCTION set -- read out of build/Debug/MotorMain.obj with coff_cmp, not
 * counted by eye: 73 entries.  The one callee with parameters (AlphBetaToDQ) is
 * carried in the same list, its parameter list being the second macro argument,
 * so the enum and the stub definitions are generated from one source of truth.
 *
 * `unsigned short` is used for the counters because it is 16 bits on the host
 * exactly as Uint16 is on the C28x; the folded `int` would work too, but the
 * ledger is deliberately independent of the module's include chain.
 */
#ifndef MOCK_CALLS_H
#define MOCK_CALLS_H

#define MOCK_CALL_LIST(X)                       \
    X(ChangeCurrent, (void))                    \
    X(CalWsAndSynFreq, (void))                  \
    X(InvCalcPower, (void))                     \
    X(CalOutputPhase, (void))                   \
    X(GetCurrentInfo, (void))                   \
    X(CalCarrierWaveFreq, (void))               \
    X(VfOverCurDeal, (void))                    \
    X(VcAsrControl, (void))                     \
    X(PrepPmsmCsrPrar, (void))                  \
    X(VfVarInitiate, (void))                    \
    X(VFOverMagneticControl, (void))            \
    X(PmDecoupleDeal, (void))                   \
    X(HVfOscDampDeal, (void))                   \
    X(ParSend2Ms, (void))                       \
    X(InvDeviceControl, (void))                 \
    X(SVCCalFlux_380, (void))                   \
    X(PrepareParForTune, (void))                \
    X(RunCaseGetPar, (void))                    \
    X(AlphBetaToDQ, (ALPHABETA_STRUCT *mAb, int mTheta, MT_STRUCT_Q24 *mMtQ24)) \
    X(RunCaseLowPower, (void))                  \
    X(PmFluxWeakDeal, (void))                   \
    X(BrakeResControl, (void))                  \
    X(GetUDCInfo, (void))                       \
    X(RunCaseShortGnd, (void))                  \
    X(GetMTTimeNum, (void))                     \
    X(VfFreqDeal, (void))                       \
    X(ChangeMotorPar, (void))                   \
    X(VFWSCompControl, (void))                  \
    X(VfOverUdcDeal, (void))                    \
    X(VfOscIndexCalc, (void))                   \
    X(PrepareForSpeedCheck, (void))             \
    X(SVCCalRotorSpeed, (void))                 \
    X(CalTorqueUp, (void))                      \
    X(SvcCalOutVolt, (void))                    \
    X(SvcCalOutVolt_380, (void))                \
    X(PrepImCsrPara, (void))                    \
    X(GetUvwPhase, (void))                      \
    X(SystemParChg2Ms, (void))                  \
    X(PrepareAsrPar, (void))                    \
    X(ParSend05Ms, (void))                      \
    X(CalUdcLimitIT, (void))                    \
    X(PmChkInitPosRest, (void))                 \
    X(RunStateParChg2Ms, (void))                \
    X(VFWsTorqueBoostComm, (void))              \
    X(HardWareOverUDCDeal, (void))              \
    X(RunCaseDcBrake, (void))                   \
    X(VFSpeedControl, (void))                   \
    X(VCGetFeedBackSpeed, (void))               \
    X(ResetParForVC, (void))                    \
    X(CalDeadBandComp, (void))                  \
    X(VCCsrControl, (void))                     \
    X(IPMCheckInitPos, (void))                  \
    X(GetCurExcursion, (void))                  \
    X(SynInitPosDetect, (void))                 \
    X(RunCaseIpmInitPos, (void))                \
    X(ParGet2Ms, (void))                        \
    X(SoftPWMProcess, (void))                   \
    X(LmIoInPeriodInt, (void))                  \
    X(HVfDeadBandComp, (void))                  \
    X(RunCaseSpeedCheck, (void))                \
    X(SystemParChg05Ms, (void))                 \
    X(CalTorqueLimitPar, (void))                \
    X(GetMDetaPos, (void))                      \
    X(OutputLoseAdd, (void))                    \
    X(ParGet05Ms, (void))                       \
    X(SVCCalRotorSpeed_380, (void))             \
    X(CalIMSet, (void))                         \
    X(CalcABVolt, (void))                       \
    X(GetRotorTransPos, (void))                 \
    X(VFAutoTorqueBoost, (void))                \
    X(CalWsAndSynFreq_380, (void))              \
    X(ChgParForEst, (void))                     \
    X(OutPutPWMVF, (void))

/* MC_<callee> names the counter slot; MC_COUNT is the array length. */
#define MOCK_CALL_ENUM(name, params) MC_##name,
enum { MOCK_CALL_LIST(MOCK_CALL_ENUM) MC_COUNT };
#undef MOCK_CALL_ENUM

extern unsigned short gMockCall[MC_COUNT];

/* `asm(" NOP")` in PWMZeroInterrupt (source line 795).  On the target that is
 * inline assembly and produces no symbol at all; on the host it is parsed as a
 * call to a function named `asm`, so the case mock defines one and counts it.
 * That keeps the module text byte-identical between the two sides instead of
 * patching it, and makes "the disabled-output branch still has its NOP" an
 * observable rather than a comment. */
extern unsigned short gAsmCount;

#endif
