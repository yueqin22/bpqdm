/* Per-side extern declarations for the f_frqSrc_pid differential driver.
 *
 * ref_tu.c / new_tu.c rename every external-linkage symbol of f_frqSrc_pid.c
 * with a ref_ / new_ prefix so both copies can live in one binary.  The driver
 * needs to reach both copies, which means declaring each symbol twice, with the
 * right type.  The symbol list comes from ref_tu.c (itself generated from
 * nm2000, so it cannot drift); the types come from the module's own definition
 * line in the frozen baseline.
 *
 * Four functions + nine data objects = the thirteen symbols.  The two
 * function-local statics (pidCalcFlagOld inside FrqPidSetDeal, pidChgOld inside
 * PidFuncCalc) are deliberately absent: cl2000 mangles file-local statics with a
 * $N suffix, so they are local symbols and the driver cannot reach them.  See
 * the header comment of driver.c for why that is not a hole.
 *
 * pidCalcFlag is declared `int` rather than `enum PID_CALC_FLAG`.  The enum is
 * defined *inside* f_frqSrc_pid.c, so it is not visible in this translation
 * unit -- but C does not check types across translation units, and both the
 * host compiler and cl2000 give that enum the width of int.  Declaring it as
 * the underlying type is the honest description of what the linker sees.
 */

extern int32  ref_FrqPidSetDeal(void);
extern int32  new_FrqPidSetDeal(void);
extern void   ref_PidFuncCalc(PID_FUNC *p);
extern void   new_PidFuncCalc(PID_FUNC *p);
extern void   ref_UpdatePidFuncPara(PID_FUNC *p);
extern void   new_UpdatePidFuncPara(PID_FUNC *p);
extern void   ref_UpdatePidFuncRefFdb(PID_FUNC *p);
extern void   new_UpdatePidFuncRefFdb(PID_FUNC *p);

extern PID_FUNC            ref_pidFunc;
extern PID_FUNC            new_pidFunc;
extern LINE_CHANGE_STRUCT  ref_pidFuncRef;
extern LINE_CHANGE_STRUCT  new_pidFuncRef;
extern LINE_STRUCT         ref_pidParaLine;
extern LINE_STRUCT         new_pidParaLine;
extern LowPassFilter       ref_pidLpf[2];
extern LowPassFilter       new_pidLpf[2];
extern Uint32              ref_pidInitTicker;
extern Uint32              new_pidInitTicker;
extern int                 ref_pidCalcFlag;
extern int                 new_pidCalcFlag;
extern Uint32              ref_pidFdbLoseTicker;
extern Uint32              new_pidFdbLoseTicker;
extern int32               ref_pidError;
extern int32               new_pidError;
extern Uint16              ref_bPidFdbLose;
extern Uint16              new_bPidFdbLose;
