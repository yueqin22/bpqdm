/* f_runSrc 差分测试: 两侧符号的声明。
 *
 * ref_tu.c / new_tu.c 各自把模块的 64 个外部符号加了 ref_ / new_ 前缀
 * (清单来自 nm2000, 不是人眼数的), 这里为 driver 声明出来。
 *
 * 拆成函数与数据两组, 只因为 driver 用它们的方式不同: 数据是"装进去 / 读回来"的
 * 观测对象, 函数是"调哪一侧"的入口。
 *
 * 注意 motorCtrlTuneStatus 定义在 f_runSrc.c:1692 (DEBUG_F_TUNE 块里), 它**是**
 * 本模块的全局量, 所以进观测集合; 而 bFrqCurAimChg 定义在 f_runSrc_accDecFrq.c:52,
 * 是**别的模块**的全局量, 由 mock 提供, 不进本文件的清单。
 */
#ifndef RUNSRC_SIDES_H
#define RUNSRC_SIDES_H

#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_menu.h"

/* ---------------------------------------------------------------------------
 * f_runSrc.c 自己定义的两个 enum 不在任何头文件里 —— 只在这个 .c 内部可见。
 * 驱动要按**真实类型**装/取 dormantStatus 与 motorCtrlTuneStatus, 所以在这里补上
 * 定义。宿主 (MSVC) 的 enum 是 4 字节, 用 Uint16 声明会只读到低半截。
 *
 * 这份头**只被 driver.c 包含** (ref_tu.c / new_tu.c 只包含 f_runSrc_ref.c 与
 * 交付源码), 所以不会和 f_runSrc.c 自己的定义在同一翻译单元里撞车。
 * ------------------------------------------------------------------------ */
enum DORMANT_STATUS
{
    DORMANT_RESPOND,
    DORMANT_NO_RESPOND,
    DORMANT_2
};

enum TUNE_RUN_STATUS
{
    TUNE_RUN_STATUS_WAIT,
    TUNE_RUN_STATUS_ACC,
    TUNE_RUN_STATUS_DEC,
    TUNE_RUN_STATUS_END
};

/* ---- 数据: 9 个聚合 + 36 个标量 = 45 ---- */

#define DATA(X)                          \
    X(union RUN_CMD,              runCmd)               \
    X(union RUN_FLAG,             runFlag)              \
    X(union RUN_STATUS_FIRST_STEP, runStatus1Step)      \
    X(union DSP_MAIN_COMMAND,     dspMainCmd)           \
    X(union DSP_MAIN_COMMAND1,    dspMainCmd1)          \
    X(union DSP_SUB_COMMAND,      dspSubCmd)            \
    X(union DSP_STATUS,           dspStatus)            \
    X(union DSP_SUB_STATUS,       dspSubStatus)         \
    X(union INVT_STATUS,          invtStatus)           \
    X(Uint16,                     startProtectSrc)      \
    X(Uint16,                     otherStopSrc)         \
    X(Uint16,                     otherStopLowerSrc)    \
    X(enum RUN_STATUS,            runStatus)            \
    X(enum START_RUN_STATUS,      startRunStatus)       \
    X(enum STOP_RUN_STATUS,       stopRunStatus)        \
    X(enum RUN_STATUS,            runStatusOld4LoseLoad) \
    X(enum RUN_STATUS,            runStatusOld4Jog)     \
    X(enum RUN_STATUS,            runStatusOld4POffNoStop) \
    X(Uint16,                     runSrc)               \
    X(Uint32,                     accFrqTime)           \
    X(Uint32,                     decFrqTime)           \
    X(enum SWING_STATUS,          swingStatus)          \
    X(Uint32,                     swingDoubleAw)        \
    X(int32,                      swingMaxFrq)          \
    X(int32,                      swingMinFrq)          \
    X(int32,                      swingJumpFrq)         \
    X(Uint32,                     swingAccTime)         \
    X(Uint32,                     swingDecTime)         \
    X(Uint16,                     swingFrqLimit)        \
    X(Uint16,                     setRunLostTime)       \
    X(Uint16,                     setRunTimeAim)        \
    X(Uint32,                     runTimeTicker)        \
    X(Uint32,                     lowerDelayTicker)     \
    X(Uint32,                     shuntTicker)          \
    X(Uint16,                     runTimeAddupTicker)   \
    X(Uint16,                     powerUpTimeAddupTicker) \
    X(Uint16,                     shuntFlag)            \
    X(Uint16,                     accDecTimeSrcPlc)     \
    X(Uint16,                     stopRemFlag)          \
    X(Uint16,                     runDirPanelOld)       \
    X(Uint16,                     bRunJog)              \
    X(Uint16,                     tuneCmd)              \
    X(Uint16,                     saveTuneDataFlag)     \
    X(enum DORMANT_STATUS,        dormantStatus)        \
    X(Uint16,                     motorCtrlTuneStatus)

#define DECL2(t, n) extern t ref_##n; extern t new_##n;
DATA(DECL2)
#undef DECL2

/* ---- 函数: 19 个 ---- */

extern void  ref_UpdateRunCmd(void);      extern void  new_UpdateRunCmd(void);
extern void  ref_setTimeRun(void);        extern void  new_setTimeRun(void);
extern void  ref_runTimeCal(void);        extern void  new_runTimeCal(void);
extern void  ref_RunSrcDeal(void);        extern void  new_RunSrcDeal(void);
extern void  ref_UpdateBenchTime(void);   extern void  new_UpdateBenchTime(void);
extern void  ref_RunSrcUpdateFrq(void);   extern void  new_RunSrcUpdateFrq(void);
extern void  ref_StartRunCtrl(void);      extern void  new_StartRunCtrl(void);
extern void  ref_NormalRunCtrl(void);     extern void  new_NormalRunCtrl(void);
extern void  ref_StopRunCtrl(void);       extern void  new_StopRunCtrl(void);
extern void  ref_JogRunCtrl(void);        extern void  new_JogRunCtrl(void);
extern void  ref_TuneRunCtrl(void);       extern void  new_TuneRunCtrl(void);
extern void  ref_LoseLoadRunCtrl(void);   extern void  new_LoseLoadRunCtrl(void);
extern void  ref_ShutDownRunCtrl(void);   extern void  new_ShutDownRunCtrl(void);
extern void  ref_UpdateSwingPara(void);   extern void  new_UpdateSwingPara(void);
extern void  ref_SwingDeal(void);         extern void  new_SwingDeal(void);
extern void  ref_LowerThanLowerFrqDeal(void); extern void new_LowerThanLowerFrqDeal(void);
extern void  ref_PowerOffNoStopDeal(void); extern void new_PowerOffNoStopDeal(void);
extern void  ref_AccDecTimeCalc(void);    extern void  new_AccDecTimeCalc(void);
extern void  ref_DormantDeal(void);       extern void  new_DormantDeal(void);

#endif
