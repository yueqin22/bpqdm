/* f_error 差分测试驱动。
 *
 * 被测模块是"故障的产生 / 定级 / 记录 / 复位"这一层。13 个函数:
 *
 *   ErrorDeal()                 故障处理主入口 = underVoltageDeal + UpdateError
 *                               + ErrorSceneSave
 *   underVoltageDeal()          欠压状态机 (bUv + uvDealStatus 三态)
 *   UpdateError()               新旧故障对比 + 按等级回退
 *   GetNewError()               本模块最大的一块: 电机故障位图 -> 故障码,
 *                               外部/用户/EEPROM 输入, 偏差检查 (devChk),
 *                               超速检查 (osChk), PLC 故障, 最后定码
 *   getMotorError()             32 位电机故障位图 -> errorCodeFromMotor
 *   GetNewErrorAttribute()      errorCode -> errorAttribute.bit.level
 *   ErrorSceneSave()            把当前运行态录进 funcCode.code.errorScene3
 *   ErrorReset()                复位状态机 (4 态) + 自动复位判断
 *   GetErrorResetDeal()         面板/按键复位源 -> runCmd.bit.errorReset
 *   AutoResetErrorDeal()        自动复位总入口 (下面两个的或)
 *   AutoResetErrorTrueDeal()    按故障本身的性质自动复位 (欠压/EEPROM/运行级)
 *   AutoResetErrorFcDeal()      按功能码 errAutoRstNumMax / errAutoRstSpaceTime
 *                               自动复位 (带一个 Uint16 计数斜坡)
 *   GetErrorAttribute(errCode)  故障码 -> 动作等级, 查 funcCode.code.errorAction[]
 *
 * ---- 比对什么 ----
 *
 *  返回值                    9 个非 void 函数的返回值 (4 个 void 靠状态比)
 *  errorCode / errorCodeOld / errorOther / errorOtherOld / errorInfo /
 *  errorInfoOld / errorInfoFromMotor / errorEeprom / errAutoRstNum / bUv
 *  errorAttribute.all / errorAttributeOld.all
 *  curTime                   故障现场用的运行时刻
 *  errorDealStatus / uvDealStatus        两个状态机
 *  errAutoRstClrWaitTicker / errorSpaceTicker   两个计数器 (Uint32 / Uint16)
 *  errorCodeFromMotorList[]  模块自己的常量表 (19 项)
 *  funcCode                  整个联合体, 逐字节 —— ErrorSceneSave 写进去的
 *                            errorLatest1/3 + errorScene3 的 8 个字段 +
 *                            errorFrqUnit, GetNewError 清掉的 plcErrorCode
 *  runFlag / runCmd / dspMainCmd / dspMainCmd1 / dspSubCmd / dspStatus / tuneCmd
 *                            ErrorReset 会清 runFlag.bit.run、dspMainCmd 的 6 个位、
 *                            dspMainCmd1.bit.speedTrack, 置 dspSubCmd.bit.errorDealing
 *  funcCodeRwMode / funcCodeRwModeTmp / startIndexWriteSeries / endIndexWriteSeries
 *  funcCodeOneWriteIndex[] / funcCodeOneWriteNum
 *  errorCodeFromMotor         getMotorError 的产物之一
 *  GetNumberDigit1 的调用次数与末次实参, PowerOffRemDeal 的调用次数
 *
 * ---- 为什么这些量都要比 ----
 *
 * f_error 的绝大多数函数是 void, 全部作用都落在**共享状态**上。少写一个字段、
 * 写错一个字段、把 errorOther 与 errorOtherOld 写反 —— 这些错法只有把 18 个模块
 * 导出 + 7 个命令/状态字 + 整个 funcCode 一起比才看得见。
 *
 * 两个**状态机** (errorDealStatus / uvDealStatus) 和四个**文件级静态量**
 * (GetNewError 里的 devTcnt / osTcnt / motorSnOld, GetErrorResetDeal 里的
 * diFuncErrRstOld) 是序列相关的: 单次调用看不出差别, 必须按**轨迹**比。所以本
 * 驱动有意把同一个输入连打若干次 (见 phase_new_error_dev / _os / _plc_switch、
 * phase_reset_deal、phase_error_deal), 让"计数器斜坡的拐点"变成可观测量。
 * 静态量本身读不到 (它们在 nm2000 里是小写, 是 TU 局部的), 但它们的**后果**
 * 能读到 —— 这正是"非平凡输入"要求的落点。
 *
 * ---- 注意 errorAttribute 的位序 ----
 *
 * struct ERROR_ATTRIBUTE_BITS 是 { reset:1; level:3; } —— level 在**第 1~3 位**,
 * 不是第 0~2 位。所以驱动一律用 .bit.level 赋值, 绝不写成
 * `errorAttribute.all = 3` 那种 (那等于 level = 1)。level 只有 3 位, 而
 * GetErrorAttribute 能返回 10 (digit 9 + 1), 赋值时**会被截断** —— 这条截断也是
 * 被测行为, 见 phase_new_error_attribute 里 errorAction = 9999 那一组。
 *
 * ---- 共享状态 ----
 *
 * funcCode / runFlag / diFunc / motorFc / 故障位图 等全部由 mock_error.c 定义,
 * 两侧**共用**。每轮用例前 install(side) 从同一份输入快照恢复, 跑完
 * snapshot(side) 取回结果。
 * 模块自己的 18 个导出是 ref_tu.c / new_tu.c 各一份, 由 X[2] 那组指针按侧索引。
 *
 * ---- 两个替身 ----
 *
 * GetNumberDigit1() 的真实现在 f_main.c:855 (已单独做过差分测试), 这里逐字照抄,
 * 因为 GetErrorAttribute 的返回值就是它的 digit[...] + 1 —— 拆位规则换了,
 * 测的就不是同一张表。
 * PowerOffRemDeal() 的真实现在 f_fcDeal.c (已单独做过差分测试), 返回 void,
 * 所以观测量只能是调用次数。
 *
 * ---- 负向对照要注意的: 死代码 ----
 *
 * f_error.c 里有四块死代码: `#if F_DEBUG_RAM` 的 0 号分支 (F_DEBUG_RAM = 0)、
 * `#if 0` 的 getMotorErrorInfo()、getMotorError() 里的 `#if 0` else 分支、
 * GetErrorAttribute() 里 `#if 0` 的 ERROR_SPEED_DETECT / ERROR_PROGRAM_LOGIC
 * 两条, 以及文件末尾 `#elif 1` 的空 ErrorDeal()。死代码在行为上不可观测, 所以
 * negative_control.py 的 CASES 条目用 `live_span: None` + 逐条核对行号 ——
 * 与 f_fcDeal / f_frqSrc 同一套做法。
 *
 * ---- 有意不测的既有缺陷 ----
 *
 * 1. getMotorError() 里 errorInfo 是**未初始化**的局部量: 当
 *    `errorsCodeFromMotor[0] || errorsCodeFromMotor[1]` 为真、而所有置位的故障
 *    等级都 >= GetErrorAttribute(ERROR_NONE) (= 4) 时, 循环一次都不赋值,
 *    随后 `errorInfoFromMotor = errorInfo` 读的是垃圾。目标端同样是未定义行为。
 *    本驱动**只要置任何位就把第 18 位 (ERROR_UVW_FDB, 等级恒为 1) 一起置上**
 *    (见 set_err_bit), 于是循环至少会在 i = 18 处赋值一次, 那条路径永远走不到。
 *    见 README。
 * 2. AutoResetErrorFcDeal() 的 `++errorSpaceTicker` 是 Uint16, 阈值
 *    `errAutoRstSpaceTime * 50` 却可能超过 65535。超过时计数回绕, 判据**永远
 *    不成立** —— 真固件里这条路径就是死的。驱动两个方向都测 (1310 可达 /
 *    1311 不可达), 见 README。
 * 3. ErrorReset() 里 `endIndexWriteSeries = GetCodeIndex(errorScene1.all[
 *    sizeof(struct ERROR_SCENE_STRUCT) - 1])` 用到 sizeof, 而 union ERROR_SCENE
 *    的数组长度也写成同一个 sizeof —— 目标端取 all[7]、宿主取 all[15], 两边都是
 *    "数组最后一个元素", 语义一致。**刻意不改写成字面量 8**, 那样宿主侧才会真的
 *    错位。见 host_patch.json 的 why。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 与 src/function/f_error.c 相同的 include 顺序。 */
#include "f_error.h"
#include "f_main.h"
#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_eeprom.h"
#include "f_io.h"
#include "f_menu.h"
#include "f_ui.h"
#include "f_comm.h"
#include "f_posCtrl.h"
#include "f_invPara.h"

#include "mock_error.h"

/* ---- 驱动自己的常量 ---- */

#define FAIL_LIMIT      20
#define Q_SNAP          FERR_WRITE_QUEUE_LEN

/* devChk / osChk 计数斜坡的重复次数。
 * 阈值最大 3 * (100/ERROR_DEAL_PERIOD) = 150, 加上 DEV_CHK_TIME_MIN = 5 的余量,
 * 计到 156 才会触发; 取 165 保证走完全程并观察到拐点两侧。 */
#define FERR_RAMP_MAX   165

/* f_error.c 私有的 DEV_CHK_TIME_MIN (源码里在 getMotorError 之后、GetNewError 之前
 * 那一行 `#define DEV_CHK_TIME_MIN 5`), 头文件里没有, 只能镜像。
 * 它只用来给 devChk 的计数上界加一点余量, 所以镜像错了也不会让测试失去意义 ——
 * 但 check_invariants() 会断言 FERR_RAMP_MAX 足够大, 免得重复次数小于阈值。 */
#define FERR_DEV_CHK_TIME_MIN   5

/* errorCodeFromMotorList[] 的项数。
 * 依据: 源码里是 19 个初始值设定项, 目标端 sizeof 返回 19 (16 位字数);
 * 链接映射里 .econst:_errorCodeFromMotorList 的长度是 0x13 = 19 字。
 * 驱动里必须写成**完整数组类型**才能对两侧做 memcmp —— 写成 `[]` 的话
 * sizeof 在 MSVC 上是 0 (warning C4034), memcmp 就变成空操作。 */
#define FERR_LIST_N     19

/* ---- f_error.c 内部的 enum UV_DEAL_STATUS 没有导出到头文件, 这里镜像一份。
 * 值必须逐个对应 (0/1/2); 宿主上两者都是 int。 ---- */
enum FERR_UV_STATUS
{
    FERR_UV_READY_FOR_WRITE_EEPROM,
    FERR_UV_WAIT_WRITE_EEPROM,
    FERR_UV_WRITE_EEPROM_OK
};

/* ---- 被测函数 ---- */

extern void   ref_ErrorDeal(void);
extern void   new_ErrorDeal(void);
extern void   ref_underVoltageDeal(void);
extern void   new_underVoltageDeal(void);
extern void   ref_UpdateError(void);
extern void   new_UpdateError(void);
extern void   ref_GetNewError(void);
extern void   new_GetNewError(void);
extern void   ref_getMotorError(void);
extern void   new_getMotorError(void);
extern void   ref_GetNewErrorAttribute(void);
extern void   new_GetNewErrorAttribute(void);
extern void   ref_ErrorSceneSave(void);
extern void   new_ErrorSceneSave(void);
extern void   ref_ErrorReset(void);
extern void   new_ErrorReset(void);
extern void   ref_GetErrorResetDeal(void);
extern void   new_GetErrorResetDeal(void);
extern Uint16 ref_AutoResetErrorDeal(void);
extern Uint16 new_AutoResetErrorDeal(void);
extern Uint16 ref_AutoResetErrorTrueDeal(void);
extern Uint16 new_AutoResetErrorTrueDeal(void);
extern Uint16 ref_AutoResetErrorFcDeal(void);
extern Uint16 new_AutoResetErrorFcDeal(void);
extern Uint16 ref_GetErrorAttribute(Uint16 errCode);
extern Uint16 new_GetErrorAttribute(Uint16 errCode);

/* ---- 模块自己的 18 个导出 (每侧一份) ---- */

extern Uint16 ref_errorEeprom, new_errorEeprom;
extern Uint16 ref_errorOther, new_errorOther;
extern Uint16 ref_errorOtherOld, new_errorOtherOld;
extern Uint16 ref_errorCode, new_errorCode;
extern Uint16 ref_errorInfo, new_errorInfo;
extern union ERROR_ATTRIBUTE ref_errorAttribute, new_errorAttribute;
extern union ERROR_ATTRIBUTE ref_errorAttributeOld, new_errorAttributeOld;
extern Uint16 ref_errorCodeOld, new_errorCodeOld;
extern Uint16 ref_errorInfoOld, new_errorInfoOld;
extern Uint16 ref_errorInfoFromMotor, new_errorInfoFromMotor;
extern Uint16 ref_errAutoRstNum, new_errAutoRstNum;
extern Uint16 ref_bUv, new_bUv;
extern struct CUR_TIME ref_curTime, new_curTime;
extern enum ERROR_DEAL_STATUS ref_errorDealStatus, new_errorDealStatus;
extern enum FERR_UV_STATUS ref_uvDealStatus, new_uvDealStatus;
extern Uint32 ref_errAutoRstClrWaitTicker, new_errAutoRstClrWaitTicker;
extern Uint16 ref_errorSpaceTicker, new_errorSpaceTicker;
extern const Uint16 ref_errorCodeFromMotorList[FERR_LIST_N];
extern const Uint16 new_errorCodeFromMotorList[FERR_LIST_N];

/* ---- 用例种类 ---- */

enum FERR_KIND
{
    K_ATTR, K_MOTORERR, K_NEWERR, K_NEWATTR, K_UPDATE, K_SCENE,
    K_RESET, K_RESETDEAL, K_UV, K_ERRDEAL, K_AUTOD, K_AUTOT, K_AUTOF
};

static enum FERR_KIND g_kind;

static void   (*g_errorDeal[2])(void);
static void   (*g_uvDeal[2])(void);
static void   (*g_update[2])(void);
static void   (*g_newError[2])(void);
static void   (*g_motorError[2])(void);
static void   (*g_newAttr[2])(void);
static void   (*g_sceneSave[2])(void);
static void   (*g_reset[2])(void);
static void   (*g_resetDeal[2])(void);
static Uint16 (*g_autoDeal[2])(void);
static Uint16 (*g_autoTrue[2])(void);
static Uint16 (*g_autoFc[2])(void);
static Uint16 (*g_attr[2])(Uint16);

/* 模块导出的按侧索引。 */
struct FERR_EXP
{
    Uint16 *eeprom, *other, *otherOld, *code, *info;
    union ERROR_ATTRIBUTE *attr, *attrOld;
    Uint16 *codeOld, *infoOld, *infoFromMotor, *rstNum, *bUv;
    struct CUR_TIME *cur;
    enum ERROR_DEAL_STATUS *dealStatus;
    enum FERR_UV_STATUS *uvStatus;
    Uint32 *rstClrTicker;
    Uint16 *spaceTicker;
    const Uint16 *list;
};

static struct FERR_EXP X[2];

static void init_ptrs(void)
{
    g_errorDeal[0] = ref_ErrorDeal;              g_errorDeal[1] = new_ErrorDeal;
    g_uvDeal[0]    = ref_underVoltageDeal;       g_uvDeal[1]    = new_underVoltageDeal;
    g_update[0]    = ref_UpdateError;            g_update[1]    = new_UpdateError;
    g_newError[0]  = ref_GetNewError;            g_newError[1]  = new_GetNewError;
    g_motorError[0] = ref_getMotorError;         g_motorError[1] = new_getMotorError;
    g_newAttr[0]   = ref_GetNewErrorAttribute;   g_newAttr[1]   = new_GetNewErrorAttribute;
    g_sceneSave[0] = ref_ErrorSceneSave;         g_sceneSave[1] = new_ErrorSceneSave;
    g_reset[0]     = ref_ErrorReset;             g_reset[1]     = new_ErrorReset;
    g_resetDeal[0] = ref_GetErrorResetDeal;      g_resetDeal[1] = new_GetErrorResetDeal;
    g_autoDeal[0]  = ref_AutoResetErrorDeal;     g_autoDeal[1]  = new_AutoResetErrorDeal;
    g_autoTrue[0]  = ref_AutoResetErrorTrueDeal; g_autoTrue[1]  = new_AutoResetErrorTrueDeal;
    g_autoFc[0]    = ref_AutoResetErrorFcDeal;   g_autoFc[1]    = new_AutoResetErrorFcDeal;
    g_attr[0]      = ref_GetErrorAttribute;      g_attr[1]      = new_GetErrorAttribute;

    X[0].eeprom = &ref_errorEeprom;      X[1].eeprom = &new_errorEeprom;
    X[0].other = &ref_errorOther;        X[1].other = &new_errorOther;
    X[0].otherOld = &ref_errorOtherOld;  X[1].otherOld = &new_errorOtherOld;
    X[0].code = &ref_errorCode;          X[1].code = &new_errorCode;
    X[0].info = &ref_errorInfo;          X[1].info = &new_errorInfo;
    X[0].attr = &ref_errorAttribute;     X[1].attr = &new_errorAttribute;
    X[0].attrOld = &ref_errorAttributeOld;
    X[1].attrOld = &new_errorAttributeOld;
    X[0].codeOld = &ref_errorCodeOld;    X[1].codeOld = &new_errorCodeOld;
    X[0].infoOld = &ref_errorInfoOld;    X[1].infoOld = &new_errorInfoOld;
    X[0].infoFromMotor = &ref_errorInfoFromMotor;
    X[1].infoFromMotor = &new_errorInfoFromMotor;
    X[0].rstNum = &ref_errAutoRstNum;    X[1].rstNum = &new_errAutoRstNum;
    X[0].bUv = &ref_bUv;                 X[1].bUv = &new_bUv;
    X[0].cur = &ref_curTime;             X[1].cur = &new_curTime;
    X[0].dealStatus = &ref_errorDealStatus;
    X[1].dealStatus = &new_errorDealStatus;
    X[0].uvStatus = &ref_uvDealStatus;   X[1].uvStatus = &new_uvDealStatus;
    X[0].rstClrTicker = &ref_errAutoRstClrWaitTicker;
    X[1].rstClrTicker = &new_errAutoRstClrWaitTicker;
    X[0].spaceTicker = &ref_errorSpaceTicker;
    X[1].spaceTicker = &new_errorSpaceTicker;
    X[0].list = ref_errorCodeFromMotorList;
    X[1].list = new_errorCodeFromMotorList;
}

/* ---- 输入快照 / 结果快照 ----
 *
 * 输入里既有共享的 mock 状态 (funcCode / 命令字 / 数字量 / 故障位图 ...), 也有
 * 模块自己的 18 个导出 —— 对本模块来说它们是**输入也是输出** (errorCode 既是
 * GetNewError 的初值, 也是它的结果; errorAttribute 既是 UpdateError 回退的依据,
 * 也是它的结果)。install() 两者都恢复, 于是两侧的起点逐字节相同。 */

struct FERR_IN
{
    /* 模块自己的导出 (既是输入也是输出) */
    Uint16 eeprom, other, otherOld, code, info;
    union ERROR_ATTRIBUTE attr, attrOld;
    Uint16 codeOld, infoOld, infoFromMotor, rstNum, bUv;
    struct CUR_TIME cur;
    enum ERROR_DEAL_STATUS dealStatus;
    enum FERR_UV_STATUS uvStatus;
    Uint32 rstClrTicker;
    Uint16 spaceTicker;

    /* 共享的 mock 状态 */
    FUNCCODE_ALL fc;
    union RUN_FLAG runFlag;
    Uint16 runMode;
    union RUN_CMD runCmd;
    union DSP_MAIN_COMMAND mainCmd;
    union DSP_MAIN_COMMAND1 mainCmd1;
    union DSP_SUB_COMMAND subCmd;
    union DSP_STATUS status;
    Uint16 tuneCmd;

    Uint16 rwMode, rwModeTmp, startSer, endSer;
    Uint16 q[Q_SNAP];
    Uint16 qNum;

    int32 frq, frqRun, frqFdbTmp;
    Uint16 maxFrq, genVolt;

    struct MOTOR_FC motorFc;
    struct DI_FUNC diFunc, diSelectFunc;
    struct DI_STATUS_ALL diStatus;
    struct DO_STATUS_ALL doStatus;
    Uint16 outCurOld, curOcDisp;

    Uint16 codeFromMotor;
    Uint16 errBits[2];
    Uint16 errInfor[5];

    Uint16 keyFunc;
    enum POWER_ON_STATUS powerOn;
    enum MOTOR_SN motorSn;

    /* GetErrorAttribute 的入参 */
    Uint16 arg;
};

struct FERR_OUT
{
    Uint16 ret;

    /* 模块自己的导出 */
    Uint16 eeprom, other, otherOld, code, info;
    Uint16 attrAll, attrOldAll;
    Uint16 codeOld, infoOld, infoFromMotor, rstNum, bUv;
    struct CUR_TIME cur;
    enum ERROR_DEAL_STATUS dealStatus;
    enum FERR_UV_STATUS uvStatus;
    Uint32 rstClrTicker;
    Uint16 spaceTicker;
    Uint16 list[FERR_LIST_N];

    /* 共享状态里会被改写的部分 */
    FUNCCODE_ALL fc;
    Uint16 runFlagAll, runCmdAll, mainCmdAll, mainCmd1All, subCmdAll, statusAll;
    Uint16 tuneCmd;
    Uint16 rwMode, rwModeTmp, startSer, endSer;
    Uint16 q[Q_SNAP];
    Uint16 qNum;

    Uint16 codeFromMotor;

    Uint16 digitCalls, digitLastArg, powerOffCalls;
};

static struct FERR_IN  g_in;
static struct FERR_OUT S[2];

/* ---- 当前用例描述 (报差异时打印, 便于复现) ---- */

static const char *g_fn;
static Uint16 g_arg0, g_arg1;

static void set_kind(enum FERR_KIND k, Uint16 a0, Uint16 a1)
{
    g_kind = k;
    g_arg0 = a0;
    g_arg1 = a1;

    switch (k)
    {
    case K_ATTR:      g_fn = "GetErrorAttribute";      break;
    case K_MOTORERR:  g_fn = "getMotorError";          break;
    case K_NEWERR:    g_fn = "GetNewError";            break;
    case K_NEWATTR:   g_fn = "GetNewErrorAttribute";   break;
    case K_UPDATE:    g_fn = "UpdateError";            break;
    case K_SCENE:     g_fn = "ErrorSceneSave";         break;
    case K_RESET:     g_fn = "ErrorReset";             break;
    case K_RESETDEAL: g_fn = "GetErrorResetDeal";      break;
    case K_UV:        g_fn = "underVoltageDeal";       break;
    case K_ERRDEAL:   g_fn = "ErrorDeal";              break;
    case K_AUTOD:     g_fn = "AutoResetErrorDeal";     break;
    case K_AUTOT:     g_fn = "AutoResetErrorTrueDeal"; break;
    default:          g_fn = "AutoResetErrorFcDeal";   break;
    }
}

static void install(int side)
{
    *X[side].eeprom = g_in.eeprom;
    *X[side].other = g_in.other;
    *X[side].otherOld = g_in.otherOld;
    *X[side].code = g_in.code;
    *X[side].info = g_in.info;
    X[side].attr->all = g_in.attr.all;
    X[side].attrOld->all = g_in.attrOld.all;
    *X[side].codeOld = g_in.codeOld;
    *X[side].infoOld = g_in.infoOld;
    *X[side].infoFromMotor = g_in.infoFromMotor;
    *X[side].rstNum = g_in.rstNum;
    *X[side].bUv = g_in.bUv;
    *X[side].cur = g_in.cur;
    *X[side].dealStatus = g_in.dealStatus;
    *X[side].uvStatus = g_in.uvStatus;
    *X[side].rstClrTicker = g_in.rstClrTicker;
    *X[side].spaceTicker = g_in.spaceTicker;

    memcpy(&funcCode, &g_in.fc, sizeof g_in.fc);
    runFlag = g_in.runFlag;
    runMode = g_in.runMode;
    runCmd = g_in.runCmd;
    dspMainCmd = g_in.mainCmd;
    dspMainCmd1 = g_in.mainCmd1;
    dspSubCmd = g_in.subCmd;
    dspStatus = g_in.status;
    tuneCmd = g_in.tuneCmd;

    funcCodeRwMode = g_in.rwMode;
    funcCodeRwModeTmp = g_in.rwModeTmp;
    startIndexWriteSeries = g_in.startSer;
    endIndexWriteSeries = g_in.endSer;
    memcpy(funcCodeOneWriteIndex, g_in.q, sizeof g_in.q);
    funcCodeOneWriteNum = g_in.qNum;

    frq = g_in.frq;
    frqRun = g_in.frqRun;
    frqFdbTmp = g_in.frqFdbTmp;
    maxFrq = g_in.maxFrq;
    generatrixVoltage = g_in.genVolt;

    motorFc = g_in.motorFc;
    diFunc = g_in.diFunc;
    diSelectFunc = g_in.diSelectFunc;
    diStatus = g_in.diStatus;
    doStatus = g_in.doStatus;
    outCurrentDispOld = g_in.outCurOld;
    currentOcDisp = g_in.curOcDisp;

    errorCodeFromMotor = g_in.codeFromMotor;
    errorsCodeFromMotor[0] = g_in.errBits[0];
    errorsCodeFromMotor[1] = g_in.errBits[1];
    memcpy(motorErrInfor, g_in.errInfor, sizeof g_in.errInfor);

    keyFunc = g_in.keyFunc;
    powerOnStatus = g_in.powerOn;
    motorSn = g_in.motorSn;

    mockDigitCalls = 0;
    mockDigitLastArg = 0;
    mockPowerOffCalls = 0;
}

static void snapshot(int side)
{
    struct FERR_OUT *s = &S[side];

    s->eeprom = *X[side].eeprom;
    s->other = *X[side].other;
    s->otherOld = *X[side].otherOld;
    s->code = *X[side].code;
    s->info = *X[side].info;
    s->attrAll = X[side].attr->all;
    s->attrOldAll = X[side].attrOld->all;
    s->codeOld = *X[side].codeOld;
    s->infoOld = *X[side].infoOld;
    s->infoFromMotor = *X[side].infoFromMotor;
    s->rstNum = *X[side].rstNum;
    s->bUv = *X[side].bUv;
    s->cur = *X[side].cur;
    s->dealStatus = *X[side].dealStatus;
    s->uvStatus = *X[side].uvStatus;
    s->rstClrTicker = *X[side].rstClrTicker;
    s->spaceTicker = *X[side].spaceTicker;
    memcpy(s->list, X[side].list, sizeof s->list);

    memcpy(&s->fc, &funcCode, sizeof s->fc);
    s->runFlagAll = runFlag.all;
    s->runCmdAll = runCmd.all;
    s->mainCmdAll = dspMainCmd.all;
    s->mainCmd1All = dspMainCmd1.all;
    s->subCmdAll = dspSubCmd.all;
    s->statusAll = dspStatus.all;
    s->tuneCmd = tuneCmd;

    s->rwMode = funcCodeRwMode;
    s->rwModeTmp = funcCodeRwModeTmp;
    s->startSer = startIndexWriteSeries;
    s->endSer = endIndexWriteSeries;
    memcpy(s->q, funcCodeOneWriteIndex, sizeof s->q);
    s->qNum = funcCodeOneWriteNum;

    s->codeFromMotor = errorCodeFromMotor;

    s->digitCalls = mockDigitCalls;
    s->digitLastArg = mockDigitLastArg;
    s->powerOffCalls = mockPowerOffCalls;
}

/* ---- 差异上报 ---- */

static long g_cases = 0;
static int  g_fails = 0;

static void report(const char *what)
{
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s: %s\n", g_fn, what);
    printf("  arg0=%u arg1=%u\n", g_arg0, g_arg1);
    printf("  code=%u/%u codeOld=%u/%u other=%u/%u otherOld=%u/%u info=%u/%u\n",
           S[0].code, S[1].code, S[0].codeOld, S[1].codeOld,
           S[0].other, S[1].other, S[0].otherOld, S[1].otherOld,
           S[0].info, S[1].info);
    printf("  attr=0x%04X/0x%04X attrOld=0x%04X/0x%04X eeprom=%u/%u bUv=%u/%u\n",
           S[0].attrAll, S[1].attrAll, S[0].attrOldAll, S[1].attrOldAll,
           S[0].eeprom, S[1].eeprom, S[0].bUv, S[1].bUv);
    printf("  dealStatus=%d/%d uvStatus=%d/%d ticker=%lu/%lu space=%u/%u rstNum=%u/%u\n",
           (int)S[0].dealStatus, (int)S[1].dealStatus,
           (int)S[0].uvStatus, (int)S[1].uvStatus,
           (unsigned long)S[0].rstClrTicker, (unsigned long)S[1].rstClrTicker,
           S[0].spaceTicker, S[1].spaceTicker, S[0].rstNum, S[1].rstNum);
    printf("  codeFromMotor=%u/%u infoFromMotor=%u/%u\n",
           S[0].codeFromMotor, S[1].codeFromMotor,
           S[0].infoFromMotor, S[1].infoFromMotor);
    printf("  runFlag=0x%04X/0x%04X runCmd=0x%04X/0x%04X subCmd=0x%04X/0x%04X\n",
           S[0].runFlagAll, S[1].runFlagAll, S[0].runCmdAll, S[1].runCmdAll,
           S[0].subCmdAll, S[1].subCmdAll);
    printf("  mainCmd=0x%04X/0x%04X mainCmd1=0x%04X/0x%04X status=0x%04X/0x%04X\n",
           S[0].mainCmdAll, S[1].mainCmdAll, S[0].mainCmd1All, S[1].mainCmd1All,
           S[0].statusAll, S[1].statusAll);
    printf("  rwMode=%u/%u rwModeTmp=%u/%u ser=%u/%u..%u/%u qNum=%u/%u\n",
           S[0].rwMode, S[1].rwMode, S[0].rwModeTmp, S[1].rwModeTmp,
           S[0].startSer, S[1].startSer, S[0].endSer, S[1].endSer,
           S[0].qNum, S[1].qNum);
    printf("  digits calls=%u/%u last=%u/%u powerOff=%u/%u\n",
           S[0].digitCalls, S[1].digitCalls, S[0].digitLastArg, S[1].digitLastArg,
           S[0].powerOffCalls, S[1].powerOffCalls);
    printf("  errBits=0x%04X%04X errInfor=%u,%u,%u,%u,%u\n",
           g_in.errBits[1], g_in.errBits[0],
           g_in.errInfor[0], g_in.errInfor[1], g_in.errInfor[2],
           g_in.errInfor[3], g_in.errInfor[4]);
    printf("  frq=%ld frqRun=%ld frqFdbTmp=%ld maxFrq=%u genVolt=%u\n",
           (long)g_in.frq, (long)g_in.frqRun, (long)g_in.frqFdbTmp,
           g_in.maxFrq, g_in.genVolt);
    printf("  plcEnable=%u plcErrCode=%u devChkValue=%u devChkTime=%u "
           "osChkValue=%u osChkTime=%u\n",
           g_in.fc.code.plcEnable, g_in.fc.code.plcErrorCode,
           g_in.fc.code.devChkValue, g_in.fc.code.devChkTime,
           g_in.fc.code.osChkValue, g_in.fc.code.osChkTime);
}

/* ---- 跑一轮并比对 ----
 *
 * 字段一个一比而不是整块 memcmp: 报出来的是"哪个观测量不一样", 不是"第几个字节
 * 不一样"。 */
static void run_case(void)
{
    int side;

    g_cases++;

    for (side = 0; side < 2; side++)
    {
        install(side);
        S[side].ret = 0;

        switch (g_kind)
        {
        case K_ATTR:      S[side].ret = g_attr[side](g_arg0);   break;
        case K_MOTORERR:  g_motorError[side]();                 break;
        case K_NEWERR:    g_newError[side]();                   break;
        case K_NEWATTR:   g_newAttr[side]();                    break;
        case K_UPDATE:    g_update[side]();                     break;
        case K_SCENE:     g_sceneSave[side]();                  break;
        case K_RESET:     g_reset[side]();                      break;
        case K_RESETDEAL: g_resetDeal[side]();                  break;
        case K_UV:        g_uvDeal[side]();                     break;
        case K_ERRDEAL:   g_errorDeal[side]();                  break;
        case K_AUTOD:     S[side].ret = g_autoDeal[side]();     break;
        case K_AUTOT:     S[side].ret = g_autoTrue[side]();     break;
        default:          S[side].ret = g_autoFc[side]();       break;
        }

        snapshot(side);
    }

    if (S[0].ret != S[1].ret) { report("return value"); return; }
    if (S[0].eeprom != S[1].eeprom) { report("errorEeprom"); return; }
    if (S[0].other != S[1].other) { report("errorOther"); return; }
    if (S[0].otherOld != S[1].otherOld) { report("errorOtherOld"); return; }
    if (S[0].code != S[1].code) { report("errorCode"); return; }
    if (S[0].info != S[1].info) { report("errorInfo"); return; }
    if (S[0].attrAll != S[1].attrAll) { report("errorAttribute.all"); return; }
    if (S[0].attrOldAll != S[1].attrOldAll) { report("errorAttributeOld.all"); return; }
    if (S[0].codeOld != S[1].codeOld) { report("errorCodeOld"); return; }
    if (S[0].infoOld != S[1].infoOld) { report("errorInfoOld"); return; }
    if (S[0].infoFromMotor != S[1].infoFromMotor) { report("errorInfoFromMotor"); return; }
    if (S[0].rstNum != S[1].rstNum) { report("errAutoRstNum"); return; }
    if (S[0].bUv != S[1].bUv) { report("bUv"); return; }
    if (memcmp(&S[0].cur, &S[1].cur, sizeof S[0].cur) != 0) { report("curTime"); return; }
    if (S[0].dealStatus != S[1].dealStatus) { report("errorDealStatus"); return; }
    if (S[0].uvStatus != S[1].uvStatus) { report("uvDealStatus"); return; }
    if (S[0].rstClrTicker != S[1].rstClrTicker) { report("errAutoRstClrWaitTicker"); return; }
    if (S[0].spaceTicker != S[1].spaceTicker) { report("errorSpaceTicker"); return; }
    if (memcmp(S[0].list, S[1].list, sizeof S[0].list) != 0)
    {
        report("errorCodeFromMotorList"); return;
    }
    if (memcmp(&S[0].fc, &S[1].fc, sizeof S[0].fc) != 0) { report("funcCode"); return; }
    if (S[0].runFlagAll != S[1].runFlagAll) { report("runFlag.all"); return; }
    if (S[0].runCmdAll != S[1].runCmdAll) { report("runCmd.all"); return; }
    if (S[0].mainCmdAll != S[1].mainCmdAll) { report("dspMainCmd.all"); return; }
    if (S[0].mainCmd1All != S[1].mainCmd1All) { report("dspMainCmd1.all"); return; }
    if (S[0].subCmdAll != S[1].subCmdAll) { report("dspSubCmd.all"); return; }
    if (S[0].statusAll != S[1].statusAll) { report("dspStatus.all"); return; }
    if (S[0].tuneCmd != S[1].tuneCmd) { report("tuneCmd"); return; }
    if (S[0].rwMode != S[1].rwMode) { report("funcCodeRwMode"); return; }
    if (S[0].rwModeTmp != S[1].rwModeTmp) { report("funcCodeRwModeTmp"); return; }
    if (S[0].startSer != S[1].startSer) { report("startIndexWriteSeries"); return; }
    if (S[0].endSer != S[1].endSer) { report("endIndexWriteSeries"); return; }
    if (S[0].qNum != S[1].qNum) { report("funcCodeOneWriteNum"); return; }
    if (memcmp(S[0].q, S[1].q, sizeof S[0].q) != 0)
    {
        report("funcCodeOneWriteIndex"); return;
    }
    if (S[0].codeFromMotor != S[1].codeFromMotor) { report("errorCodeFromMotor"); return; }
    if (S[0].digitCalls != S[1].digitCalls) { report("GetNumberDigit1 调用次数"); return; }
    if (S[0].digitLastArg != S[1].digitLastArg) { report("GetNumberDigit1 末次实参"); return; }
    if (S[0].powerOffCalls != S[1].powerOffCalls) { report("PowerOffRemDeal 调用次数"); return; }
}

/* ---- 随机数 ----
 *
 * 自带 LCG, 不用 rand(): 一是各平台 rand() 序列不同, 二是这里要的只是"同一份输入
 * 喂给两侧", 与随机性质量无关。 */
static Uint32 g_seed = 0x2468ACE0UL;

static Uint16 rnd(void)
{
    g_seed = g_seed * 1103515245UL + 12345UL;
    return (Uint16)((g_seed >> 16) & 0xFFFFUL);
}

static Uint16 rnd_n(Uint16 n)
{
    return (Uint16)(rnd() % n);
}

/* ---- 不变量检查 (跑之前做一次, 不满足就直接停下) ----
 *
 * 这些断言的意义: 用例必须"要么真的测到了, 要么大声失败", 不能因为替身摆得不对
 * 而悄悄少测一条分支。 */
static void check_invariants(void)
{
    /* errorCodeFromMotorList[] 的**期望内容** —— 与 src/function/f_error.c 里
     * 那张表的 19 个初始值设定项逐个对应。这不是"抄一遍", 而是把
     * "位图下标 -> 故障码" 这张映射钉死: getMotorError() 的每一条择优路径都
     * 依赖它, 表一改 (少一项 / 换顺序 / 改码值) 这里立刻报错。 */
    static const Uint16 expect[FERR_LIST_N] =
    {
        ERROR_OC_ACC_SPEED, ERROR_OV_ACC_SPEED, ERROR_BUFFER_RES, ERROR_UV,
        ERROR_OL_INVERTER, ERROR_OL_MOTOR, ERROR_LOSE_PHASE_INPUT,
        ERROR_LOSE_PHASE_OUTPUT, ERROR_OT_IGBT, ERROR_CONTACTOR,
        ERROR_CURRENT_SAMPLE, ERROR_MOTOR_SHORT_TO_GND, ERROR_LOSE_LOAD,
        ERROR_CBC, ERROR_INIT_POSITION, ERROR_RSVD, ERROR_RSVD,
        ERROR_TUNE, ERROR_UVW_FDB
    };
    Uint16 i;

    for (i = 0; i < FERR_LIST_N; i++)
    {
        if (ref_errorCodeFromMotorList[i] != expect[i]
            || new_errorCodeFromMotorList[i] != expect[i])
        {
            printf("FATAL: errorCodeFromMotorList[%u] = %u/%u, 期望 %u\n",
                   i, ref_errorCodeFromMotorList[i],
                   new_errorCodeFromMotorList[i], expect[i]);
            exit(2);
        }
    }

    /* 表尾之后紧跟的一项由 nm2000 核对: 链接映射里
     * .econst:_errorCodeFromMotorList 的长度是 0x13 = 19 字。 */
    printf("errorCodeFromMotorList: %d 项, 内容已逐项核对\n", FERR_LIST_N);

    /* union ERROR_SCENE 的数组长度与 ERROR_SCENE_STRUCT 的成员数必须对得上。
     * 源码把数组长度写成 sizeof(struct ERROR_SCENE_STRUCT), 所以这两条恒等;
     * 断言它是为了把"sizeof 的词/字节差"这件事钉在记录里。 */
    if ((Uint16)(sizeof(struct ERROR_SCENE_STRUCT) / sizeof(Uint16))
        != FERR_SCENE_ELEM_NUM)
    {
        printf("FATAL: ERROR_SCENE_STRUCT 成员数 %u != %d\n",
               (unsigned)(sizeof(struct ERROR_SCENE_STRUCT) / sizeof(Uint16)),
               FERR_SCENE_ELEM_NUM);
        exit(2);
    }

    /* 故障等级枚举必须真的分档, 否则 GetErrorAttribute 的分支退化成一条。 */
    if (ERROR_LEVEL_NO_ERROR == ERROR_LEVEL_FREE_STOP
        || ERROR_LEVEL_FREE_STOP == ERROR_LEVEL_RUN)
    {
        printf("FATAL: 故障等级枚举退化\n");
        exit(2);
    }

    /* errorAttribute 的位序: level 必须在第 1~3 位。
     * 驱动全程用 .bit.level 赋值, 这里把"位序"这件事显式钉住 —— 万一头文件
     * 改了位序, 用例立刻报错而不是悄悄测错。 */
    {
        union ERROR_ATTRIBUTE probe;

        probe.all = 0;
        probe.bit.level = 5;
        if (probe.all != (Uint16)(5 << 1))
        {
            printf("FATAL: ERROR_ATTRIBUTE.level 不在第 1~3 位 (all=0x%04X)\n",
                   probe.all);
            exit(2);
        }
    }

    /* ERROR_DEALING 必须落在 dspSubCmd 的第 0 位 —— 模块靠它把"正在处理
     * 故障"告诉别的模块, 位序错了整条链路都测不到。 */
    dspSubCmd.all = 0;
    ERROR_DEALING = 1;
    if (dspSubCmd.all != 1)
    {
        printf("FATAL: ERROR_DEALING 不是 dspSubCmd 的第 0 位 (all=0x%04X)\n",
               dspSubCmd.all);
        exit(2);
    }
    dspSubCmd.all = 0;

    /* 写队列容量 (本模块不写它, 但快照带它进出一趟, 尺寸要够)。 */
    if (FERR_WRITE_QUEUE_LEN < 8)
    {
        printf("FATAL: 写队列快照太短\n");
        exit(2);
    }

    /* 计数斜坡的重复次数必须够跑完全程: 阈值最大 3 * 50 = 150, 上界再加
     * DEV_CHK_TIME_MIN = 5 的余量。FERR_DEV_CHK_TIME_MIN 是**镜像**的模块私有
     * 宏 (源码里那一行 `#define DEV_CHK_TIME_MIN 5`), 所以这条断言是"镜像与
     * 重复次数自洽", 不是"与模块自洽" —— 模块那一行若改了, 这里要跟着核。 */
    if (FERR_RAMP_MAX
        <= 3 * (100 / ERROR_DEAL_PERIOD) + FERR_DEV_CHK_TIME_MIN)
    {
        printf("FATAL: FERR_RAMP_MAX=%d 不足以跑完 devChk 斜坡 (需要 > %d)\n",
               FERR_RAMP_MAX, 3 * (100 / ERROR_DEAL_PERIOD) + FERR_DEV_CHK_TIME_MIN);
        exit(2);
    }

    /* 几个"斜坡阈值"打出来, 让记录里有据可查。 */
    printf("devChk 阈值 = devChkTime * %u, 计数上限 = 65535, DEV_CHK_TIME_MIN = %d\n",
           (unsigned)(100 / ERROR_DEAL_PERIOD), FERR_DEV_CHK_TIME_MIN);
    printf("osChk  阈值 = osChkTime  * %u\n",
           (unsigned)(100 / ERROR_DEAL_PERIOD));
    printf("自动复位延时 = errAutoRstSpaceTime * %u (errorSpaceTicker 是 Uint16)\n",
           (unsigned)(TIME_UNIT_ERR_AUTO_RST_DELAY / RUN_CTRL_PERIOD));
    printf("自动复位清等待 = %lu\n",
           (unsigned long)(10UL * TIME_UNIT_ERR_AUTO_RST_CLR / ERROR_DEAL_PERIOD));
}

/* ---- 输入构造工具 ---- */

static void zero_in(void)
{
    memset(&g_in, 0, sizeof g_in);
}

static void set_attr(union ERROR_ATTRIBUTE *a, Uint16 level, Uint16 reset)
{
    a->all = 0;
    a->bit.level = level;
    a->bit.reset = reset;
}

/* 把全部输入填成**非平凡**的基线值。
 *
 * 每一条都刻意避开"恰好落在判据边界上"或"恰好落在掩码盲区里"的取值:
 *   - errorAction[] 的 4 个十进制数让 digit[0..3] 两两不同, 于是
 *     GetErrorAttribute 里 "读错 digit 下标" 立刻可见;
 *   - errorFrqUnit 的每个半字节都非零, 否则 `& 0x0FFF` 的差异会被抹平;
 *   - frq 与 frqRun 的差、frqFdbTmp 的绝对值都不为 0;
 *   - 故障位图与 motorErrInfor 全非零, 否则 errorInfo 的抽取
 *     (`>> (i%4)*4 & 0xF`) 不可分辨。 */
static void base_state(void)
{
    zero_in();

    /* 模块自己的导出 */
    g_in.eeprom = 0;
    g_in.other = ERROR_NONE;
    g_in.otherOld = ERROR_OL_MOTOR;
    g_in.code = ERROR_NONE;
    g_in.info = 0;
    set_attr(&g_in.attr, ERROR_LEVEL_NO_ERROR, 0);
    set_attr(&g_in.attrOld, ERROR_LEVEL_NO_ERROR, 1);
    g_in.codeOld = ERROR_NONE;
    g_in.infoOld = 0;
    g_in.infoFromMotor = 0;
    g_in.rstNum = 0;
    g_in.bUv = 0;
    g_in.cur.runTimeM = 7;
    g_in.cur.runTimeSec = 31;
    g_in.cur.powerOnTimeM = 3;
    g_in.cur.powerOnTimeSec = 59;
    g_in.cur.runTime = 451;
    g_in.cur.powerOnTime = 239;
    g_in.dealStatus = ERROR_DEAL_PREPARE_FOR_WRITE_EEPROM;
    g_in.uvStatus = FERR_UV_READY_FOR_WRITE_EEPROM;
    g_in.rstClrTicker = 0;
    g_in.spaceTicker = 0;

    /* funcCode 里本模块用到的成员 */
    g_in.fc.code.errorAction[0] = 4321;   /* d0=1 d1=2 d2=3 d3=4 */
    g_in.fc.code.errorAction[1] = 8765;   /* d0=5 d1=6 d2=7 d3=8 */
    g_in.fc.code.errorAction[2] = 2345;   /* d0=5 d1=4 d2=3 d3=2 */
    g_in.fc.code.errorAction[3] = 9876;   /* d0=6 d1=7 d2=8 d3=9 */
    g_in.fc.code.ovPointSet = 4000;
    g_in.fc.code.devChkValue = 1000;
    g_in.fc.code.devChkTime = 0;
    g_in.fc.code.osChkValue = 1000;
    g_in.fc.code.osChkTime = 0;
    g_in.fc.code.plcEnable = 0;
    g_in.fc.code.plcErrorCode = 0;
    g_in.fc.code.errAutoRstNumMax = 3;
    g_in.fc.code.errAutoRstSpaceTime = 10;
    g_in.fc.code.errorLatest1 = 11;
    g_in.fc.code.errorLatest3 = 22;
    g_in.fc.code.errorFrqUnit = 0xABCD;
    g_in.fc.code.frqPoint = 2;

    /* 命令 / 状态 */
    g_in.runFlag.all = 0;
    g_in.runMode = 0;
    g_in.runCmd.all = 0;
    g_in.mainCmd.all = 0;
    g_in.mainCmd1.all = 0;
    g_in.subCmd.all = 0;
    g_in.status.all = 0;
    g_in.tuneCmd = 0;

    /* EEPROM 握手 */
    g_in.rwMode = 0;
    g_in.rwModeTmp = 0;
    g_in.startSer = 5;
    g_in.endSer = 9;
    g_in.qNum = 0;

    /* 频率 / 电压 */
    g_in.frq = 300;
    g_in.frqRun = 300;
    g_in.frqFdbTmp = 300;
    g_in.maxFrq = 500;
    g_in.genVolt = 3999;

    g_in.motorFc.motorCtrlMode = 0;
    g_in.outCurOld = 77;
    g_in.curOcDisp = 88;

    g_in.codeFromMotor = 0;
    g_in.errBits[0] = 0;
    g_in.errBits[1] = 0;
    g_in.errInfor[0] = 0x1357;
    g_in.errInfor[1] = 0x2468;
    g_in.errInfor[2] = 0x9ABC;
    g_in.errInfor[3] = 0xDEF0;
    g_in.errInfor[4] = 0x1234;

    g_in.keyFunc = 0;
    g_in.powerOn = POWER_ON_CORE_OK;
    g_in.motorSn = MOTOR_SN_1;
}

/* 置位故障位图里的第 i 位 (0..18 是有意义的项)。
 * 约定: **只要置任何位, 就一起置第 18 位** (ERROR_UVW_FDB) —— 它的等级恒为 1,
 * 保证 getMotorError() 的循环至少赋值一次 errorInfo, 避开那个未初始化读
 * (见文件头「有意不测的既有缺陷」第 1 条)。 */
static void set_err_bit(Uint16 i)
{
    if (i < 16)
    {
        g_in.errBits[0] |= (Uint16)(1u << i);
    }
    else
    {
        g_in.errBits[1] |= (Uint16)(1u << (i - 16));
    }
    g_in.errBits[1] |= (Uint16)(1u << 2);   /* 第 18 位 */
}

static void clr_err_bits(void)
{
    g_in.errBits[0] = 0;
    g_in.errBits[1] = 0;
}

static void set_action(Uint16 a0, Uint16 a1, Uint16 a2, Uint16 a3)
{
    g_in.fc.code.errorAction[0] = a0;
    g_in.fc.code.errorAction[1] = a1;
    g_in.fc.code.errorAction[2] = a2;
    g_in.fc.code.errorAction[3] = a3;
}

/* ---- 阶段 1: GetErrorAttribute(errCode) 全码扫描 ----
 *
 * 判据链是 5 组 + ERROR_NONE 特例 + 兜底, 每组从 errorAction[k] 里取一位十进制
 * 数字当等级。所以下标要**扫满 0..60** (覆盖 1..53 与 40..45 两段空洞),
 * 再加 PLC 段 (80..89)、ERROR_RSVD (9999) 与几个越界值; errorAction 的 4 个值
 * 轮换 8 组, 让每个 digit 位置都取到 0..9 的不同值 (含 >= 3 的高位, 那会把等级
 * 顶到 4 以上)。 */
static void phase_attribute(void)
{
    static const Uint16 actions[8] =
    {
        4321, 8765, 2345, 9876, 1234, 0, 9, 65535
    };
    static const Uint16 codes[62] =
    {
        0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,
        21,22,23,24,25,26,27,28,29,30,31,32,33,39,40,41,42,43,44,45,
        46,50,51,52,53,54,60,79,80,81,85,89,90,99,100,255,1000,
        9998,9999,10000,65535
    };
    Uint16 a, c;

    for (a = 0; a < 8; a++)
    {
        set_action(actions[a], actions[(Uint16)((a + 3) & 7)],
                   actions[(Uint16)((a + 5) & 7)],
                   actions[(Uint16)((a + 6) & 7)]);
        for (c = 0; c < 62; c++)
        {
            set_kind(K_ATTR, codes[c], 0);
            run_case();
        }
    }
}

/* ---- 阶段 2: getMotorError() 位图扫描 ----
 *
 * 循环 19 次, 前 16 次从 errorsCodeFromMotor[0] 取位、后 3 次从 [1] 取。
 * 三条通路要都走到:
 *   (a) 两位图全 0 -> else 分支, errorInfo = 0;
 *   (b) 单个位 -> 该位对应的故障码胜出, errorInfo 从 motorErrInfor[i/4] 抽半字节;
 *   (c) 多位同时 -> "等级严格更小才换" 的择优逻辑。
 * motorErrInfor[5] 每个半字节都不同, 于是 "读错 motorErrInfor 下标" 或 "移位量
 * 算错" 立刻可见。 */
static void phase_motor_error(void)
{
    Uint16 i, k;
    Uint16 pat;

    /* (a) 全 0 */
    clr_err_bits();
    set_kind(K_MOTORERR, 0, 0);
    run_case();

    /* (b) 单一位: 第 i 位 + 第 18 位 */
    for (i = 0; i < 19; i++)
    {
        clr_err_bits();
        set_err_bit(i);
        set_kind(K_MOTORERR, i, 0);
        run_case();
    }

    /* (b2) 单一位, 但把 errorAction 换成高位数字 (等级 6/7/8/9/10) —— 高等级
     *      位不会顶掉低等级位, 择优逻辑的方向在这里被钉住。 */
    for (i = 0; i < 19; i++)
    {
        clr_err_bits();
        set_err_bit(i);
        set_action(0, 9, 88, 777);
        set_kind(K_MOTORERR, i, 1);
        run_case();
    }
    set_action(4321, 8765, 2345, 9876);

    /* (c) 全 19 位 */
    clr_err_bits();
    for (i = 0; i < 19; i++)
    {
        set_err_bit(i);
    }
    set_kind(K_MOTORERR, 19, 0);
    run_case();

    /* (d) 随机多位 */
    for (k = 0; k < 600; k++)
    {
        clr_err_bits();
        pat = rnd();
        for (i = 0; i < 19; i++)
        {
            if (pat & (Uint16)(1u << (i % 16)))
            {
                set_err_bit(i);
            }
        }
        set_err_bit((Uint16)rnd_n(19));
        set_action(rnd_n(10000), rnd_n(10000), rnd_n(10000), rnd_n(10000));
        set_kind(K_MOTORERR, k, 0);
        run_case();
    }
    set_action(4321, 8765, 2345, 9876);

    /* (e) 等级**相等**时的择优方向: 原实现是严格更小才换 (`<`), 改成 `<=` 就会
     *      换成**后出现**的那一个。构造: 只置第 6 位与第 12 位
     *      (ERROR_LOSE_PHASE_INPUT 与 ERROR_LOSE_LOAD)。base_state() 的
     *      errorAction[0] = 4321 (digit1 = 2) 与 errorAction[2] = 2345
     *      (digit3 = 2) 让两者的等级**都是 3** —— 相等, 且都不是 FREE_STOP,
     *      所以循环不会提前 break, 相等关系真的走到了判据上。
     *
     *      **这里不能走 set_err_bit()**: 那个助手总是顺手置第 18 位, 而
     *      ERROR_UVW_FDB 的等级是 1 —— 它会在 i = 18 处把前两者一起顶掉并 break,
     *      相等关系就再也看不到 (这正是 E038 曾经 MISSED 的原因)。只置这两位
     *      仍然保证至少赋值一次 (i = 6 先命中, 等级 3 < 初始 4), 所以不会踩到
     *      `errorInfo` 未初始化那条既有 UB。 */
    clr_err_bits();
    g_in.errBits[0] = (Uint16)((1u << 6) | (1u << 12));
    g_in.errBits[1] = 0;
    set_kind(K_MOTORERR, 20, 0);
    run_case();

    /* (e2) 同一对位, 但把两条 errorAction 的数字位都压到 1, 让等级同为 2
     *      —— 换一个数值再钉一遍, 免得"恰好只有一个值能造出相等"。 */
    clr_err_bits();
    g_in.errBits[0] = (Uint16)((1u << 6) | (1u << 12));
    g_in.errBits[1] = 0;
    set_action(4111, 8765, 1111, 9876);
    set_kind(K_MOTORERR, 21, 0);
    run_case();
    set_action(4321, 8765, 2345, 9876);
}

/* ---- 阶段 3: GetNewError() —— 故障码择优 ----
 *
 * 只留电机故障位图, 扫 accDecStatus (决定 OC/OV 报加速/减速/恒速) 与
 * generatrixVoltage >= ovPointSet 的边界。 */
static void phase_new_error_motor(void)
{
    static const Uint16 ov[3] = { 3999, 4000, 4001 };
    static const Uint16 on[3] = { 3999, 4000, 4001 };
    static const Uint16 others[6] =
    {
        ERROR_NONE, ERROR_UV, ERROR_OV_ACC_SPEED, ERROR_OL_MOTOR,
        ERROR_EXTERNAL, ERROR_EEPROM
    };
    Uint16 i, acc, a, b, k;

    for (i = 0; i < 19; i++)
    {
        for (acc = 0; acc < 4; acc++)
        {
            clr_err_bits();
            set_err_bit(i);
            g_in.runFlag.bit.accDecStatus = acc;
            g_in.runFlag.bit.run = 1;
            g_in.motorFc.motorCtrlMode = FUNCCODE_motorCtrlMode_FVC;
            set_kind(K_NEWERR, i, acc);
            run_case();
        }
    }

    /* ovPointSet 边界: 小于 / 等于 / 大于。等于也算过压 (>=)。 */
    for (a = 0; a < 3; a++)
    {
        for (b = 0; b < 3; b++)
        {
            clr_err_bits();
            set_err_bit(1);                /* ERROR_OV_ACC_SPEED */
            g_in.genVolt = ov[a];
            g_in.fc.code.ovPointSet = on[b];
            g_in.powerOn = POWER_ON_CORE_OK;
            set_kind(K_NEWERR, ov[a], on[b]);
            run_case();

            /* powerOnStatus 不是 CORE_OK 时那条判据整条不成立 */
            g_in.powerOn = POWER_ON_WAIT;
            set_kind(K_NEWERR, ov[a], (Uint16)(on[b] | 0x0100));
            run_case();
        }
    }
    g_in.genVolt = 3999;
    g_in.fc.code.ovPointSet = 4000;
    g_in.powerOn = POWER_ON_CORE_OK;

    /* errorOther 本身就是输入: `errorOther == ERROR_OV_ACC_SPEED` 会顶掉
     * errorCodeFromMotor 的分支。 */
    for (k = 0; k < 6; k++)
    {
        clr_err_bits();
        set_err_bit(1);
        g_in.other = others[k];
        set_kind(K_NEWERR, others[k], 0x0200);
        run_case();
    }
    g_in.other = ERROR_NONE;
}

/* ---- 阶段 4: GetNewError() —— 外部输入那一串 if ----
 *
 * 5 个布尔输入 (外部常开/常闭、用户 1/2、EEPROM) 的 32 种组合全枚举;
 * errorLevel 的初值由 errorOther 的等级决定, 所以 errorOther 也要扫 ——
 * 判据是**严格小于**, 两边都要走到。 */
static void phase_new_error_external(void)
{
    static const Uint16 others[8] =
    {
        ERROR_NONE, ERROR_OC_ACC_SPEED, ERROR_OL_MOTOR, ERROR_EXTERNAL,
        ERROR_USER_1, ERROR_EEPROM, ERROR_DEV, ERROR_OS
    };
    Uint16 m, o;

    for (o = 0; o < 8; o++)
    {
        for (m = 0; m < 32; m++)
        {
            clr_err_bits();
            g_in.other = others[o];
            g_in.diFunc.f1.bit.externalErrOpenIn = (Uint32)(m & 1);
            g_in.diSelectFunc.f2.bit.externalErrCloseIn = (Uint32)((m >> 1) & 1);
            g_in.diFunc.f2.bit.externalErrCloseIn = (Uint32)((m >> 2) & 1);
            g_in.diFunc.f2.bit.userError1 = (Uint32)((m >> 3) & 1);
            g_in.diFunc.f2.bit.userError2 = (Uint32)((m >> 4) & 1);
            g_in.eeprom = (Uint16)((m & 1) ? ERROR_EEPROM : 0);
            set_kind(K_NEWERR, others[o], m);
            run_case();
        }
    }
    g_in.other = ERROR_NONE;
    g_in.eeprom = 0;
    g_in.diFunc.f1.all = 0;
    g_in.diFunc.f2.all = 0;
    g_in.diSelectFunc.f2.all = 0;
}

/* ---- 阶段 5: GetNewError() —— devChk 计数斜坡 ----
 *
 * devTcnt 是文件级 static, 单次调用看不出差别, 必须连打看拐点:
 *   条件成立 -> devTcnt++ (上界 devChkTime*(100/ERROR_DEAL_PERIOD) + DEV_CHK_TIME_MIN)
 *   条件不成立 -> devTcnt = 0
 *   拐点: devTcnt > devChkTime*(100/ERROR_DEAL_PERIOD) 时才报 ERROR_DEV
 * 所以每个 (devChkValue, devChkTime, 频差) 组合都连打 FERR_RAMP_MAX 次,
 * 轨迹逐次比对。频差在阈值两侧各取一次 (阈值 = maxFrq*devChkValue/1000)。 */
static void phase_new_error_dev(void)
{
    static const Uint16 times[3] = { 1, 2, 3 };
    static const Uint16 values[3] = { 1000, 500, 0 };
    Uint16 t, v, d, n;
    Uint16 thr;

    /* 触发分支的判据是 `attr(ERROR_DEV) < errorLevel`, 而 errorLevel 从
     * GetErrorAttribute(errorOther = ERROR_NONE) = 4 起步。base_state() 给的
     * errorAction[3] = 9876 让 attr(ERROR_DEV) = 7、attr(ERROR_OS) = 8 —— 两个
     * 都比 4 差, 于是 devChk / osChk 的触发分支**永远进不去**。负向对照里
     * E084 / E089 / E094 因此 MISSED: 不是测试台坏了, 是这一路根本没被走到。
     * 换成 21 (digit0 = 1, digit1 = 2) 之后 attr(ERROR_DEV) = 2、
     * attr(ERROR_OS) = 3, 都 < 4。
     *
     * 再把 errorAction[2] 换成 11、把残留的 errorCode 钉在 ERROR_USER_1
     * (它的等级也变成 2): 这样 `if (errorLevel < GetErrorAttribute(errorCode))`
     * 会在 ref (errorLevel = attr(ERROR_DEV) = 2 -> 判据假) 与
     * 『errorLevel 被压成 ERROR_LEVEL_FREE_STOP = 1』的变异 (判据真) 之间劈开,
     * 否则 E089 仍然不可观测。 */
    g_in.fc.code.errorAction[2] = 11;
    g_in.fc.code.errorAction[3] = 21;
    g_in.code = ERROR_USER_1;

    for (t = 0; t < 3; t++)
    {
        for (v = 0; v < 3; v++)
        {
            thr = (Uint16)(((Uint32)g_in.maxFrq * values[v]) / 1000u);
            for (d = 0; d < 3; d++)
            {
                int32 diff;

                if (d == 0)      { diff = (int32)thr; }
                else if (d == 1) { diff = (int32)thr + 1; }
                else             { diff = (int32)thr - 1; }

                clr_err_bits();
                g_in.other = ERROR_NONE;
                g_in.fc.code.devChkValue = values[v];
                g_in.fc.code.devChkTime = times[t];
                g_in.runMode = 0;
                g_in.runFlag.bit.run = 1;
                g_in.runFlag.bit.tune = 0;
                g_in.motorFc.motorCtrlMode = FUNCCODE_motorCtrlMode_FVC;
                g_in.frq = 1000;
                g_in.frqRun = (int32)(1000 + diff);

                for (n = 0; n < FERR_RAMP_MAX; n++)
                {
                    set_kind(K_NEWERR, (Uint16)(t * 9 + v * 3 + d),
                             (Uint16)(0x4000 | n));
                    run_case();
                }
            }
        }
    }

    /* 条件成立但被 runMode / tune / motorCtrlMode / run 各自否决一遍 */
    {
        static const Uint16 modes[3] = { 0, 1, 2 };
        Uint16 k;

        clr_err_bits();
        g_in.fc.code.devChkValue = 1000;
        g_in.fc.code.devChkTime = 1;
        g_in.frq = 1000;
        g_in.frqRun = 1600;
        g_in.runFlag.bit.run = 1;
        g_in.runFlag.bit.tune = 0;
        g_in.motorFc.motorCtrlMode = FUNCCODE_motorCtrlMode_FVC;

        for (k = 0; k < 3; k++)
        {
            g_in.runMode = modes[k];
            set_kind(K_NEWERR, 0x0500, k);
            run_case();
        }

        g_in.runMode = 0;
        g_in.runFlag.bit.tune = 1;
        set_kind(K_NEWERR, 0x0501, 0);
        run_case();
        g_in.runFlag.bit.tune = 0;

        g_in.motorFc.motorCtrlMode = 0;
        set_kind(K_NEWERR, 0x0502, 0);
        run_case();
        g_in.motorFc.motorCtrlMode = FUNCCODE_motorCtrlMode_FVC;

        g_in.runFlag.bit.run = 0;
        set_kind(K_NEWERR, 0x0503, 0);
        run_case();
    }

    g_in.fc.code.devChkTime = 0;
    g_in.frq = 300;
    g_in.frqRun = 300;
    g_in.runFlag.bit.run = 0;
}

/* ---- 阶段 6: GetNewError() —— osChk 计数斜坡 ----
 *
 * 与 devChk 同构, 判据是 |frqFdbTmp| > maxFrq*(1000+osChkValue)/1000。
 * 负的 frqFdbTmp 也来一遍 —— ABS_INT32 在负数上的行为是独立的一条。 */
static void phase_new_error_os(void)
{
    static const Uint16 times[3] = { 1, 2, 3 };
    static const Uint16 values[3] = { 1000, 500, 0 };
    Uint16 t, v, d, n;
    Uint16 thr;
    int32 cand[3];

    /* 同 phase_new_error_dev: attr(ERROR_OS) 必须 < 4 才进得去触发分支
     * (E094 就是被这条挡住的)。errorCode 的残留值也一并钉住。 */
    g_in.fc.code.errorAction[2] = 11;
    g_in.fc.code.errorAction[3] = 21;
    g_in.code = ERROR_USER_1;

    for (t = 0; t < 3; t++)
    {
        for (v = 0; v < 3; v++)
        {
            thr = (Uint16)(((Uint32)g_in.maxFrq * (1000u + values[v])) / 1000u);
            cand[0] = (int32)thr;
            cand[1] = (int32)thr + 1;
            cand[2] = -((int32)thr + 1);

            for (d = 0; d < 3; d++)
            {
                clr_err_bits();
                g_in.other = ERROR_NONE;
                g_in.fc.code.osChkValue = values[v];
                g_in.fc.code.osChkTime = times[t];
                g_in.fc.code.devChkTime = 0;
                g_in.frq = 1000;
                g_in.frqRun = 1000;
                g_in.frqFdbTmp = cand[d];
                g_in.runFlag.bit.run = 1;
                g_in.runFlag.bit.tune = 0;
                g_in.motorFc.motorCtrlMode = FUNCCODE_motorCtrlMode_FVC;

                for (n = 0; n < FERR_RAMP_MAX; n++)
                {
                    set_kind(K_NEWERR, (Uint16)(t * 9 + v * 3 + d),
                             (Uint16)(0x6000 | n));
                    run_case();
                }
            }
        }
    }

    /* 三条否决条件各来一次 */
    clr_err_bits();
    g_in.fc.code.osChkValue = 1000;
    g_in.fc.code.osChkTime = 1;
    g_in.frqFdbTmp = 2000;
    g_in.runFlag.bit.run = 1;
    g_in.runFlag.bit.tune = 0;

    g_in.motorFc.motorCtrlMode = 0;
    set_kind(K_NEWERR, 0x0700, 0);
    run_case();
    g_in.motorFc.motorCtrlMode = FUNCCODE_motorCtrlMode_FVC;

    g_in.runFlag.bit.tune = 1;
    set_kind(K_NEWERR, 0x0701, 0);
    run_case();
    g_in.runFlag.bit.tune = 0;

    g_in.runFlag.bit.run = 0;
    set_kind(K_NEWERR, 0x0702, 0);
    run_case();

    g_in.fc.code.osChkTime = 0;
    g_in.frqFdbTmp = 300;
    g_in.runFlag.bit.run = 0;
}

/* ---- 阶段 7: GetNewError() —— PLC 故障与电机切换 ----
 *
 * PLC 段 (DEBUG_F_PLC_CTRL = 1, 是活代码): plcErrorCode 落在 [80, 89] 才作数,
 * 而且**无论作不作数都被清成 0** —— 清零本身是观测量。
 * 电机切换: motorSn 变化且正在运行时强报 ERROR_SWITCH_MOTOR_WHEN_RUN,
 * 这是唯一一条把 errorLevel 直接压成 1 的路径。motorSnOld 是 static, 所以
 * 必须**连打一串不同的 motorSn** 才能把"变化"这个条件造出来。 */
static void phase_new_error_plc_switch(void)
{
    static const Uint16 codes[10] =
    {
        0, 1, 79, 80, 81, 85, 89, 90, 100, 65535
    };
    static const enum MOTOR_SN sn[4] =
    {
        MOTOR_SN_1, MOTOR_SN_2, MOTOR_SN_3, MOTOR_SN_4
    };
    Uint16 k, en, i, r;

    for (k = 0; k < 10; k++)
    {
        for (en = 0; en < 2; en++)
        {
            clr_err_bits();
            g_in.fc.code.plcEnable = en;
            g_in.fc.code.plcErrorCode = codes[k];
            g_in.other = ERROR_NONE;
            set_kind(K_NEWERR, codes[k], (Uint16)(0x0800 | en));
            run_case();
        }
    }
    g_in.fc.code.plcEnable = 0;
    g_in.fc.code.plcErrorCode = 0;

    /* motorSn 序列: 每次换一个值, runFlag.bit.run 在两种状态间切 */
    for (i = 0; i < 4; i++)
    {
        for (r = 0; r < 2; r++)
        {
            clr_err_bits();
            g_in.motorSn = sn[i];
            g_in.runFlag.bit.run = r;
            g_in.other = ERROR_NONE;
            set_kind(K_NEWERR, (Uint16)i, (Uint16)(0x0900 | r));
            run_case();
        }
    }
    g_in.motorSn = MOTOR_SN_1;
    g_in.runFlag.bit.run = 0;
}

/* ---- 阶段 8: GetNewErrorAttribute() ----
 *
 * 只有两条出路: errorCode == ERROR_NONE -> 0; 其余 -> GetErrorAttribute(errorCode)。
 * errorAction 的值决定后者落在哪一档, 所以两边都要扫。
 * actions[5] = 9999 那一组会让 GetErrorAttribute 返回 10, 而
 * errorAttribute.bit.level 只有 3 位 —— **截断**本身也是被测行为。
 * 另外 errorAttribute.all 预置成非零 (level = 7), 于是"没写 level"会立刻暴露。 */
static void phase_new_error_attribute(void)
{
    static const Uint16 actions[6] =
    {
        4321, 8765, 0, 9, 9999, 65535
    };
    static const Uint16 codes[12] =
    {
        0, 2, 9, 11, 12, 13, 15, 20, 27, 42, 51, 53
    };
    Uint16 a, c;

    for (a = 0; a < 6; a++)
    {
        set_action(actions[a], actions[(Uint16)((a + 2) % 6)],
                   actions[(Uint16)((a + 4) % 6)], actions[a]);
        for (c = 0; c < 12; c++)
        {
            g_in.code = codes[c];
            set_attr(&g_in.attr, 7, 1);
            set_kind(K_NEWATTR, codes[c], a);
            run_case();
        }
    }
    set_action(4321, 8765, 2345, 9876);
}

/* ---- 阶段 9: UpdateError() ----
 *
 * 先存旧值, 再算新值, 然后按等级回退。要覆盖的组合:
 *   - errorCodeOld == errorCode            -> 整段不进
 *   - 新等级 = NO_ERROR                    -> 回退
 *   - 旧等级 = NO_ERROR                    -> 保留新值
 *   - 新等级 < 旧等级                      -> 保留新值
 *   - 旧等级 = RUN                         -> 保留新值
 *   - 其余                                 -> 回退
 * 直接喂 (errorCode, errorAttribute.level) 这两对, 比绕一大圈去构造更直接。
 * GetNewError 与 GetNewErrorAttribute 在这一相里也会被连带调用 (UpdateError 的
 * 前两步), 所以它们的输入也要摆成非平凡值。 */
static void phase_update_error(void)
{
    static const Uint16 codes[8] =
    {
        ERROR_NONE, ERROR_OC_ACC_SPEED, ERROR_UV, ERROR_OL_MOTOR,
        ERROR_EXTERNAL, ERROR_EEPROM, ERROR_DEV, ERROR_UVW_FDB
    };
    static const Uint16 levels[5] =
    {
        ERROR_LEVEL_NO_ERROR, ERROR_LEVEL_FREE_STOP, ERROR_LEVEL_STOP,
        ERROR_LEVEL_RUN, 4
    };
    Uint16 a, b, c, d;

    for (a = 0; a < 8; a++)
    {
        for (b = 0; b < 5; b++)
        {
            for (c = 0; c < 8; c++)
            {
                for (d = 0; d < 5; d++)
                {
                    clr_err_bits();
                    g_in.other = ERROR_NONE;
                    g_in.codeOld = codes[a];
                    set_attr(&g_in.attrOld, levels[b], (Uint16)(b & 1));
                    g_in.code = codes[c];
                    set_attr(&g_in.attr, levels[d], (Uint16)(d & 1));
                    g_in.info = (Uint16)(0x100 + a * 8 + b);
                    g_in.infoOld = (Uint16)(0x200 + c * 8 + d);
                    set_kind(K_UPDATE, (Uint16)(a * 5 + b), (Uint16)(c * 5 + d));
                    run_case();
                }
            }
        }
    }
}

/* ---- 阶段 10: ErrorSceneSave() ----
 *
 * 四条出路:
 *   errorCode == 0                          -> 整段不进
 *   errorCode == errorCodeOld               -> 整段不进
 *   errorCode 落在 PLC 段 [80,89]           -> 整段不进
 *   其余                                    -> 录现场 + 更新 errorLatest1/3 与 errorFrqUnit
 * 现场里 errorCurrent 有第二条来源: errorCodeFromMotor == ERROR_OC_ACC_SPEED
 * 且 currentOcDisp > outCurrentDispOld 时改用 currentOcDisp。两边都要扫。
 * errorFrqUnit 的更新是 `((v & 0x0FFF) << 4) + frqPoint` —— 掩码与移位都在,
 * 所以喂的值要让被掩的位和 frqPoint 都非零 (见 README 的掩码盲区一条)。 */
static void phase_scene_save(void)
{
    static const Uint16 codes[8] =
    {
        0, ERROR_OC_ACC_SPEED, ERROR_UV, ERROR_OL_MOTOR,
        ERROR_UVW_FDB, 80, 85, 89
    };
    static const Uint16 olds[3] = { 0, ERROR_OC_ACC_SPEED, 89 };
    static const Uint16 frqUnits[4] = { 0xABCD, 0x0FFF, 0xF000, 0x0001 };
    Uint16 a, b, k, m;

    for (a = 0; a < 8; a++)
    {
        for (b = 0; b < 3; b++)
        {
            for (k = 0; k < 4; k++)
            {
                for (m = 0; m < 2; m++)
                {
                    clr_err_bits();
                    g_in.code = codes[a];
                    g_in.codeOld = olds[b];
                    g_in.fc.code.errorFrqUnit = frqUnits[k];
                    g_in.fc.code.frqPoint = (Uint16)(m + 1);
                    g_in.frqRun = -12345;
                    g_in.genVolt = 3456;
                    g_in.diStatus.a.all = 0x0F0F;
                    g_in.doStatus.a.all = 0xF0F0;
                    g_in.runFlag.all = 0x0FF0;
                    g_in.outCurOld = 100;
                    g_in.curOcDisp = (Uint16)(m ? 300 : 50);
                    g_in.codeFromMotor = (Uint16)((a == 1) ? ERROR_OC_ACC_SPEED : 0);

                    set_kind(K_SCENE, codes[a], (Uint16)(b * 8 + k * 2 + m));
                    run_case();
                }
            }
        }
    }
}

/* ---- 阶段 11: ErrorReset() ----
 *
 * 四态状态机 + 自动复位。要覆盖:
 *   - errorCode == 0                  -> 整段不进 (但 ticker 仍然 ++)
 *   - level == FREE_STOP              -> 清 runFlag.bit.run 与 dspMainCmd 的 6 个位
 *   - 四个 errorDealStatus 状态各自的前置条件成立/不成立
 *   - errAutoRstClrWaitTicker 的阈值 1800000 两侧
 *   - ERROR_DEAL_OK 态里 errorOther / errorCodeFromMotor 与 errorCode 相等/不等
 * ERROR_DEALING 是 dspSubCmd.bit.errorDealing, 也是观测量。 */
static void phase_error_reset(void)
{
    static const Uint16 levels[4] =
    {
        ERROR_LEVEL_NO_ERROR, ERROR_LEVEL_FREE_STOP,
        ERROR_LEVEL_STOP, ERROR_LEVEL_RUN
    };
    static const Uint32 tickers[3] = { 0UL, 1799999UL, 1800000UL };
    static const Uint16 codes[5] =
    {
        0, ERROR_OC_ACC_SPEED, ERROR_UV, ERROR_MOTOR_SHORT_TO_GND, ERROR_UVW_FDB
    };
    static const Uint16 rw[3] = { 0, 1, 11 };
    static const Uint16 others[3] = { ERROR_UV, ERROR_NONE, ERROR_OC_ACC_SPEED };
    Uint16 c, l, s, t, k;

    for (c = 0; c < 5; c++)
    {
        for (l = 0; l < 4; l++)
        {
            for (s = 0; s < 4; s++)
            {
                for (t = 0; t < 3; t++)
                {
                    clr_err_bits();
                    g_in.code = codes[c];
                    set_attr(&g_in.attr, levels[l], 0);
                    g_in.dealStatus = (enum ERROR_DEAL_STATUS)s;
                    g_in.rstClrTicker = tickers[t];
                    g_in.rstNum = 7;
                    g_in.rwMode = 0;
                    g_in.rwModeTmp = 0;
                    g_in.subCmd.all = (Uint16)(0x0002 | (c & 1));
                    g_in.runFlag.all = 0x3FFF;
                    g_in.mainCmd.all = 0x3FFF;
                    g_in.mainCmd1.all = 0x3FFF;
                    g_in.tuneCmd = 5;
                    set_kind(K_RESET, (Uint16)(c * 16 + l * 4 + s), t);
                    run_case();
                }
            }
        }
    }

    /* 状态机每个状态的两侧: funcCodeRwMode[Tmp] 与 errorCodeFromMotor */
    for (c = 0; c < 3; c++)
    {
        Uint16 d;

        for (l = 0; l < 3; l++)
        {
            for (d = 0; d < 2; d++)
            {
                clr_err_bits();
                if (d)
                {
                    set_err_bit(0);          /* errorCodeFromMotor != 0 */
                }
                g_in.code = ERROR_UV;
                set_attr(&g_in.attr, ERROR_LEVEL_FREE_STOP, 0);
                g_in.dealStatus = ERROR_DEAL_WRITE_EEPROM_OK;
                g_in.rwMode = rw[c];
                g_in.rwModeTmp = rw[l];
                g_in.rstClrTicker = 0;
                g_in.rstNum = 0;
                g_in.subCmd.all = 0;
                set_kind(K_RESET, 0x0A00, (Uint16)(c * 6 + l * 2 + d));
                run_case();
            }
        }
    }

    /* ERROR_DEAL_OK 态: errorOther 与 errorCode 相等与否 */
    for (k = 0; k < 3; k++)
    {
        clr_err_bits();
        g_in.code = ERROR_UV;
        set_attr(&g_in.attr, ERROR_LEVEL_FREE_STOP, 0);
        g_in.other = others[k];
        g_in.otherOld = ERROR_NONE;
        g_in.dealStatus = ERROR_DEAL_OK;
        g_in.subCmd.all = 1;
        g_in.rstClrTicker = 0;
        set_kind(K_RESET, 0x0B00, k);
        run_case();
    }

    g_in.subCmd.all = 0;
    g_in.rstNum = 0;
    g_in.rstClrTicker = 0;
}

/* ---- 阶段 12: GetErrorResetDeal() ----
 *
 * 两个来源: 面板复位输入的**上升沿** (diFuncErrRstOld 是 static, 所以要连打),
 * 和"停止键 + 当前故障不是运行级 (或没在运行)"。
 * 上升沿那一条必须成对跑: 0->1 触发, 1->1 不触发。 */
static void phase_reset_deal(void)
{
    static const Uint16 keys[4] = { 0, KEY_STOP, 1, 2 };
    static const Uint16 levels[4] =
    {
        ERROR_LEVEL_NO_ERROR, ERROR_LEVEL_FREE_STOP,
        ERROR_LEVEL_STOP, ERROR_LEVEL_RUN
    };
    Uint16 k, l, r;

    for (k = 0; k < 4; k++)
    {
        for (l = 0; l < 4; l++)
        {
            for (r = 0; r < 2; r++)
            {
                g_in.keyFunc = keys[k];
                set_attr(&g_in.attr, levels[l], 0);
                g_in.runFlag.bit.run = r;
                g_in.runCmd.all = 0;

                /* 先压 0 再压 1: 造上升沿 */
                g_in.diFunc.f1.bit.errorReset = 0;
                set_kind(K_RESETDEAL, (Uint16)(k * 8 + l * 2 + r), 0);
                run_case();

                g_in.diFunc.f1.bit.errorReset = 1;
                set_kind(K_RESETDEAL, (Uint16)(k * 8 + l * 2 + r), 1);
                run_case();

                g_in.diFunc.f1.bit.errorReset = 1;
                set_kind(K_RESETDEAL, (Uint16)(k * 8 + l * 2 + r), 2);
                run_case();

                g_in.diFunc.f1.bit.errorReset = 0;
                set_kind(K_RESETDEAL, (Uint16)(k * 8 + l * 2 + r), 3);
                run_case();
            }
        }
    }
    g_in.keyFunc = 0;
    g_in.diFunc.f1.bit.errorReset = 0;
    g_in.runCmd.all = 0;
}

/* ---- 阶段 13: underVoltageDeal() ----
 *
 * bUv 的置位条件有两条 (常开: 未欠压且不在上电等待; 或 powerOnStatus 到了
 * FUNC_WAIT_OT), 加上三态状态机。三个电源状态 × 两个欠压位 × 两个运行位
 * × 三个状态 × 三个 funcCodeRwMode[Tmp] 全枚举。 */
static void phase_under_voltage(void)
{
    static const Uint16 rw[3] = { 0, 1, 11 };
    Uint16 pw, uv, run, st, a, b;

    for (pw = 0; pw < 3; pw++)
    {
        for (uv = 0; uv < 2; uv++)
        {
            for (run = 0; run < 2; run++)
            {
                for (st = 0; st < 3; st++)
                {
                    for (a = 0; a < 3; a++)
                    {
                        for (b = 0; b < 3; b++)
                        {
                            g_in.bUv = (Uint16)((pw + uv + run) & 1);
                            g_in.powerOn = (enum POWER_ON_STATUS)pw;
                            g_in.status.bit.uv = uv;
                            g_in.runFlag.bit.run = run;
                            g_in.uvStatus = (enum FERR_UV_STATUS)st;
                            g_in.rwMode = rw[a];
                            g_in.rwModeTmp = rw[b];
                            g_in.other = ERROR_NONE;
                            set_kind(K_UV,
                                     (Uint16)(pw * 100 + uv * 50 + run * 10 + st),
                                     (Uint16)(a * 3 + b));
                            run_case();
                        }
                    }
                }
            }
        }
    }

    /* bUv = 0 且置位条件成立/不成立各来一遍, 状态机三态都摆到 */
    for (st = 0; st < 3; st++)
    {
        g_in.bUv = 0;
        g_in.powerOn = POWER_ON_WAIT;
        g_in.status.bit.uv = 1;
        g_in.runFlag.bit.run = 0;
        g_in.uvStatus = (enum FERR_UV_STATUS)st;
        g_in.rwMode = 0;
        g_in.rwModeTmp = 0;
        set_kind(K_UV, 0x0C00, st);
        run_case();
    }

    g_in.bUv = 0;
    g_in.uvStatus = FERR_UV_READY_FOR_WRITE_EEPROM;
    g_in.status.all = 0;
    g_in.powerOn = POWER_ON_CORE_OK;
    g_in.runFlag.bit.run = 0;
    g_in.rwMode = 0;
    g_in.rwModeTmp = 0;
}

/* ---- 阶段 14: 三个 AutoReset* 直接调 ----
 *
 * AutoResetErrorTrueDeal: 欠压且 !bUv; EEPROM 且 errorEeprom == 0;
 *                         运行级且 errorCode 既不等于 errorOtherOld 也不等于
 *                         errorCodeFromMotor。
 * AutoResetErrorFcDeal:   两个分支 (runCmd.bit.errorReset 请求 / 计数斜坡),
 *                         斜坡阈值 errAutoRstSpaceTime*50 —— errorSpaceTicker
 *                         是 Uint16, 阈值超过 65535 时**永远到不了**
 *                         (1310*50 = 65500 可达 / 1311*50 = 65550 不可达)。 */
static void phase_auto_reset(void)
{
    static const Uint16 codes[6] =
    {
        ERROR_NONE, ERROR_UV, ERROR_EEPROM, ERROR_MOTOR_SHORT_TO_GND,
        ERROR_OC_ACC_SPEED, ERROR_UVW_FDB
    };
    static const Uint16 spaces[6] = { 0, 1, 10, 1310, 1311, 2000 };
    static const Uint16 levels[4] =
    {
        ERROR_LEVEL_NO_ERROR, ERROR_LEVEL_FREE_STOP,
        ERROR_LEVEL_STOP, ERROR_LEVEL_RUN
    };
    Uint16 c, k;

    /* TrueDeal: 扫 errorCode × bUv × errorEeprom × level × errorOtherOld */
    for (c = 0; c < 6; c++)
    {
        Uint16 buv, ee, lv, eq;

        for (buv = 0; buv < 2; buv++)
        {
            for (ee = 0; ee < 2; ee++)
            {
                for (lv = 0; lv < 4; lv++)
                {
                    for (eq = 0; eq < 3; eq++)
                    {
                        clr_err_bits();
                        if (eq == 2)
                        {
                            set_err_bit(0);      /* errorCodeFromMotor = OC_ACC */
                        }
                        g_in.code = codes[c];
                        g_in.bUv = buv;
                        g_in.eeprom = (Uint16)(ee ? 0 : ERROR_EEPROM);
                        set_attr(&g_in.attr, levels[lv], 0);
                        g_in.otherOld = (eq == 0) ? codes[c] : ERROR_NONE;
                        set_kind(K_AUTOT,
                                 (Uint16)(c * 100 + buv * 50 + ee * 25 + lv * 3 + eq),
                                 0);
                        run_case();
                    }
                }
            }
        }
    }

    /* FcDeal: runCmd.bit.errorReset 分支 */
    for (c = 0; c < 6; c++)
    {
        Uint16 req, lv, run;

        for (req = 0; req < 2; req++)
        {
            for (lv = 0; lv < 4; lv++)
            {
                for (run = 0; run < 2; run++)
                {
                    clr_err_bits();
                    g_in.code = codes[c];
                    set_attr(&g_in.attr, levels[lv], 0);
                    g_in.runCmd.all = 0;
                    g_in.runCmd.bit.errorReset = req;
                    g_in.runFlag.bit.run = run;
                    g_in.rstNum = 5;
                    set_kind(K_AUTOF, (Uint16)(c * 100 + req * 50 + lv * 2 + run), 0);
                    run_case();
                }
            }
        }
    }

    /* FcDeal: 计数斜坡 (阈值两侧 + 计数预置到阈值前一位) */
    for (k = 0; k < 6; k++)
    {
        Uint16 pre;
        Uint32 thr;

        thr = (Uint32)spaces[k]
            * (Uint16)(TIME_UNIT_ERR_AUTO_RST_DELAY / RUN_CTRL_PERIOD);

        for (pre = 0; pre < 3; pre++)
        {
            Uint16 n;

            clr_err_bits();
            g_in.code = ERROR_UVW_FDB;
            set_attr(&g_in.attr, ERROR_LEVEL_FREE_STOP, 0);
            g_in.runCmd.all = 0;
            g_in.rstNum = 0;
            g_in.fc.code.errAutoRstNumMax = 3;
            g_in.fc.code.errAutoRstSpaceTime = spaces[k];

            if (thr == 0)
            {
                g_in.spaceTicker = 0;
            }
            else if (pre == 0)
            {
                g_in.spaceTicker = (Uint16)((thr - 1) & 0xFFFF);
            }
            else if (pre == 1)
            {
                g_in.spaceTicker = (Uint16)(thr & 0xFFFF);
            }
            else
            {
                g_in.spaceTicker = 65535;
            }

            /* 连打三次: 越过阈值 -> errAutoRstNum++ -> 下一次重新计 */
            for (n = 0; n < 3; n++)
            {
                set_kind(K_AUTOF, (Uint16)(0x0D00 + k * 8 + pre * 3 + n),
                         (Uint16)(thr & 0xFFFF));
                run_case();
            }
        }
    }

    /* FcDeal 的 OC 类最小间隔夹取 (E177~E181)。
     *
     * errorCode 是 OC 三兄弟 (加速/减速/恒速) 时, errAutoRstSpaceTime 被抬到
     * 至少 5; 其余故障码原样用。夹取只在**原始值 < 5** 时才可观测, 而原来这一段
     * 只喂了 ERROR_UVW_FDB (非 OC) —— 夹取分支从来没被走到, 于是
     * E177/E178/E179 (OC 三兄弟判据互换)、E180 (比较界 5 -> 4)、
     * E181 (兜底 5 -> 4) 全部 MISSED。这是测试台的洞, 不是改写的漏洞。
     *
     * 阈值 = errAutoRstSpaceTime * (TIME_UNIT_ERR_AUTO_RST_DELAY/RUN_CTRL_PERIOD),
     * 用 Uint32 乘, 不溢出, 所以两侧的差别**只**来自夹取。把 errorSpaceTicker
     * 预置到"原始阈值"和"夹取后阈值"前后各一位, 差异就从 errAutoRstNum /
     * runCmd.bit.startProtect / 返回值三个方向同时看得见。 */
    {
        static const Uint16 ocCode[4] =
        {
            ERROR_OC_ACC_SPEED, ERROR_OC_DEC_SPEED,
            ERROR_OC_CONST_SPEED, ERROR_UVW_FDB      /* 非 OC, 作对照 */
        };
        static const Uint16 ocSpace[5] = { 0, 1, 4, 5, 6 };
        Uint16 ci, si, pi;

        for (ci = 0; ci < 4; ci++)
        {
            for (si = 0; si < 5; si++)
            {
                Uint16 raw = (Uint16)((Uint32)ocSpace[si]
                                      * (Uint16)(TIME_UNIT_ERR_AUTO_RST_DELAY
                                                 / RUN_CTRL_PERIOD));
                Uint16 clamp = (Uint16)(5u
                                        * (Uint16)(TIME_UNIT_ERR_AUTO_RST_DELAY
                                                   / RUN_CTRL_PERIOD));
                Uint16 pre[6];

                pre[0] = 0;
                pre[1] = (Uint16)(raw - 1u);
                pre[2] = raw;
                pre[3] = (Uint16)(clamp - 1u);
                pre[4] = clamp;
                pre[5] = 65535;

                for (pi = 0; pi < 6; pi++)
                {
                    clr_err_bits();
                    g_in.code = ocCode[ci];
                    set_attr(&g_in.attr, ERROR_LEVEL_FREE_STOP, 0);
                    g_in.runCmd.all = 0;
                    g_in.rstNum = 0;
                    g_in.fc.code.errAutoRstNumMax = 3;
                    g_in.fc.code.errAutoRstSpaceTime = ocSpace[si];
                    g_in.spaceTicker = pre[pi];
                    set_kind(K_AUTOF,
                             (Uint16)(0x1000 + ci * 40 + si * 8 + pi), 0);
                    run_case();
                }
            }
        }
    }

    /* errAutoRstNum 已达上限 -> 整段不进 */
    clr_err_bits();
    g_in.code = ERROR_UVW_FDB;
    g_in.rstNum = 9;
    g_in.fc.code.errAutoRstNumMax = 3;
    g_in.fc.code.errAutoRstSpaceTime = 1;
    g_in.spaceTicker = 65535;
    g_in.runCmd.all = 0;
    set_kind(K_AUTOF, 0x0E00, 0);
    run_case();

    /* AutoResetErrorDeal 是上面两个的或 */
    clr_err_bits();
    g_in.code = ERROR_UV;
    g_in.bUv = 0;
    set_attr(&g_in.attr, ERROR_LEVEL_RUN, 0);
    g_in.runCmd.all = 0;
    g_in.rstNum = 0;
    g_in.fc.code.errAutoRstNumMax = 3;
    g_in.fc.code.errAutoRstSpaceTime = 1;
    set_kind(K_AUTOD, 0x0F00, 0);
    run_case();

    clr_err_bits();
    g_in.code = ERROR_OC_ACC_SPEED;
    g_in.bUv = 1;
    g_in.runCmd.bit.errorReset = 1;
    g_in.runFlag.bit.run = 0;
    set_kind(K_AUTOD, 0x0F01, 0);
    run_case();

    g_in.runCmd.all = 0;
    g_in.rstNum = 0;
    g_in.spaceTicker = 0;
    g_in.fc.code.errAutoRstSpaceTime = 10;
    g_in.fc.code.errAutoRstNumMax = 3;
}

/* ---- 阶段 15: ErrorDeal() 整链 ----
 *
 * 三个子函数串起来跑, 状态机与计数器都在动。每个组合连打 3 次, 把
 * uvDealStatus 的三态与两个斜坡都走一遍。 */
static void phase_error_deal(void)
{
    static const Uint16 codes[5] =
    {
        0, ERROR_UV, ERROR_OL_MOTOR, ERROR_OC_ACC_SPEED, ERROR_UVW_FDB
    };
    Uint16 c, u, d, n;

    for (c = 0; c < 5; c++)
    {
        for (u = 0; u < 3; u++)
        {
            for (d = 0; d < 4; d++)
            {
                clr_err_bits();
                set_err_bit(0);
                g_in.code = codes[c];
                g_in.codeOld = ERROR_NONE;
                g_in.uvStatus = (enum FERR_UV_STATUS)u;
                g_in.dealStatus = (enum ERROR_DEAL_STATUS)d;
                g_in.bUv = 1;
                g_in.status.bit.uv = 0;
                g_in.powerOn = POWER_ON_CORE_OK;
                g_in.runFlag.all = 0;
                g_in.runFlag.bit.run = (Uint16)(c & 1);
                g_in.rwMode = 0;
                g_in.rwModeTmp = 0;
                g_in.rstClrTicker = 0;
                g_in.rstNum = 0;
                g_in.subCmd.all = 0;
                g_in.fc.code.devChkTime = 1;
                g_in.fc.code.osChkTime = 1;
                g_in.frq = 1000;
                g_in.frqRun = 1600;
                g_in.frqFdbTmp = 2000;
                g_in.motorFc.motorCtrlMode = FUNCCODE_motorCtrlMode_FVC;

                for (n = 0; n < 3; n++)
                {
                    set_kind(K_ERRDEAL, (Uint16)(c * 12 + u * 4 + d), n);
                    run_case();
                }
            }
        }
    }

    g_in.fc.code.devChkTime = 0;
    g_in.fc.code.osChkTime = 0;
    g_in.frq = 300;
    g_in.frqRun = 300;
    g_in.frqFdbTmp = 300;
}

/* ---- 阶段 16: 随机混合 ----
 *
 * 每个输入都重新播撒, 并且**刻意让每个半字节都非零** —— 模块里有多处掩码
 * (`errorFrqUnit & 0x0FFF`、`(motorErrInfor >> (i%4)*4) & 0xF`、命令字的位域),
 * 播撒值只要有一个半字节恒为 0, 那一处的掩码差异就被抹平 (见 README 的
 * "非平凡 != 可分辨" 一条)。 */
static void phase_random(long n)
{
    long i;

    for (i = 0; i < n; i++)
    {
        Uint16 m, k;

        base_state();

        m = rnd();
        set_action((Uint16)(rnd() | 0x1111), (Uint16)(rnd() | 0x2222),
                   (Uint16)(rnd() | 0x3333), (Uint16)(rnd() | 0x4444));

        g_in.fc.code.ovPointSet = (Uint16)(rnd() | 0x0001);
        g_in.fc.code.devChkValue = (Uint16)(rnd() | 0x0101);
        g_in.fc.code.devChkTime = rnd_n(4);
        g_in.fc.code.osChkValue = (Uint16)(rnd() | 0x0101);
        g_in.fc.code.osChkTime = rnd_n(4);
        g_in.fc.code.plcEnable = (Uint16)(rnd() & 1);
        g_in.fc.code.plcErrorCode = (Uint16)(rnd() % 120);
        g_in.fc.code.errAutoRstNumMax = rnd_n(6);
        g_in.fc.code.errAutoRstSpaceTime = (Uint16)(rnd() % 1400);
        g_in.fc.code.errorLatest1 = rnd();
        g_in.fc.code.errorLatest3 = rnd();
        g_in.fc.code.errorFrqUnit = (Uint16)(rnd() | 0x0F0F);
        g_in.fc.code.frqPoint = rnd_n(3);

        g_in.eeprom = (Uint16)(rnd() & 1);
        g_in.other = (Uint16)(rnd() % 60);
        g_in.otherOld = (Uint16)(rnd() % 60);
        g_in.code = (Uint16)(rnd() % 60);
        g_in.codeOld = (Uint16)(rnd() % 60);
        g_in.info = rnd();
        g_in.infoOld = rnd();
        g_in.infoFromMotor = rnd();
        set_attr(&g_in.attr, (Uint16)(rnd() % 8), (Uint16)(rnd() & 1));
        set_attr(&g_in.attrOld, (Uint16)(rnd() % 8), (Uint16)(rnd() & 1));
        g_in.rstNum = (Uint16)(rnd() % 8);
        g_in.bUv = (Uint16)(rnd() & 1);
        g_in.dealStatus = (enum ERROR_DEAL_STATUS)rnd_n(4);
        g_in.uvStatus = (enum FERR_UV_STATUS)rnd_n(3);
        g_in.rstClrTicker = ((Uint32)rnd() << 16) | rnd();
        g_in.spaceTicker = rnd();
        g_in.cur.runTimeM = rnd();
        g_in.cur.runTimeSec = rnd();
        g_in.cur.powerOnTimeM = rnd();
        g_in.cur.powerOnTimeSec = rnd();
        g_in.cur.runTime = rnd();
        g_in.cur.powerOnTime = rnd();

        g_in.runFlag.all = rnd();
        g_in.runMode = rnd_n(4);
        g_in.runCmd.all = rnd();
        g_in.mainCmd.all = rnd();
        g_in.mainCmd1.all = rnd();
        g_in.subCmd.all = rnd();
        g_in.status.all = rnd();
        g_in.tuneCmd = rnd();

        g_in.rwMode = rnd_n(12);
        g_in.rwModeTmp = rnd_n(12);
        g_in.startSer = rnd();
        g_in.endSer = rnd();
        g_in.qNum = rnd_n(Q_SNAP);
        for (k = 0; k < 4; k++)
        {
            g_in.q[k] = rnd();
        }

        g_in.frq = (int32)((Uint32)rnd() << 8);
        g_in.frqRun = (int32)((Uint32)rnd() << 8);
        g_in.frqFdbTmp = (int32)((Uint32)rnd() << 8);
        g_in.maxFrq = (Uint16)(rnd() | 1);
        g_in.genVolt = rnd();

        g_in.motorFc.motorCtrlMode = rnd_n(3);
        g_in.outCurOld = rnd();
        g_in.curOcDisp = rnd();

        clr_err_bits();
        g_in.errBits[0] = rnd();
        g_in.errBits[1] = rnd();
        set_err_bit((Uint16)rnd_n(19));
        g_in.errInfor[0] = rnd();
        g_in.errInfor[1] = rnd();
        g_in.errInfor[2] = rnd();
        g_in.errInfor[3] = rnd();
        g_in.errInfor[4] = rnd();

        g_in.codeFromMotor = (Uint16)(rnd() % 60);

        g_in.diFunc.f1.all = ((Uint32)rnd() << 16) | rnd();
        g_in.diFunc.f2.all = ((Uint32)rnd() << 16) | rnd();
        g_in.diSelectFunc.f1.all = ((Uint32)rnd() << 16) | rnd();
        g_in.diSelectFunc.f2.all = ((Uint32)rnd() << 16) | rnd();
        g_in.diStatus.a.all = rnd();
        g_in.diStatus.b.all = rnd();
        g_in.diStatus.c.all = rnd();
        g_in.doStatus.a.all = rnd();
        g_in.doStatus.b.all = rnd();
        g_in.doStatus.c.all = rnd();

        g_in.keyFunc = rnd_n(8);
        g_in.powerOn = (enum POWER_ON_STATUS)rnd_n(3);
        g_in.motorSn = (enum MOTOR_SN)rnd_n(4);

        switch (rnd_n(13))
        {
        case 0:  set_kind(K_ATTR, rnd_n(120), 0);  run_case(); break;
        case 1:  set_kind(K_MOTORERR, 0, 0);       run_case(); break;
        case 2:  set_kind(K_NEWERR, 0, 0);         run_case(); break;
        case 3:  set_kind(K_NEWATTR, 0, 0);        run_case(); break;
        case 4:  set_kind(K_UPDATE, 0, 0);         run_case(); break;
        case 5:  set_kind(K_SCENE, 0, 0);          run_case(); break;
        case 6:  set_kind(K_RESET, 0, 0);          run_case(); break;
        case 7:  set_kind(K_RESETDEAL, 0, 0);      run_case(); break;
        case 8:  set_kind(K_UV, 0, 0);             run_case(); break;
        case 9:  set_kind(K_ERRDEAL, 0, 0);        run_case(); break;
        case 10: set_kind(K_AUTOD, 0, 0);          run_case(); break;
        case 11: set_kind(K_AUTOT, 0, 0);          run_case(); break;
        default: set_kind(K_AUTOF, 0, 0);          run_case(); break;
        }
    }
}

/* ---- main ---- */

int main(int argc, char **argv)
{
    long nrand = 200000;
    long base;

    if (argc > 1)
    {
        nrand = atol(argv[1]);
    }

    init_ptrs();
    check_invariants();

    base_state();
    base = g_cases;
    phase_attribute();
    printf("  GetErrorAttribute 全码扫描    : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_motor_error();
    printf("  getMotorError 位图扫描        : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_new_error_motor();
    printf("  GetNewError 故障码择优        : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_new_error_external();
    printf("  GetNewError 外部输入          : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_new_error_dev();
    printf("  GetNewError devChk 斜坡       : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_new_error_os();
    printf("  GetNewError osChk 斜坡        : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_new_error_plc_switch();
    printf("  GetNewError PLC / 电机切换    : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_new_error_attribute();
    printf("  GetNewErrorAttribute          : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_update_error();
    printf("  UpdateError 回退阶梯          : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_scene_save();
    printf("  ErrorSceneSave                : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_error_reset();
    printf("  ErrorReset 状态机             : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_reset_deal();
    printf("  GetErrorResetDeal             : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_under_voltage();
    printf("  underVoltageDeal              : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_auto_reset();
    printf("  AutoReset*                    : %ld\n", g_cases - base);

    base_state();
    base = g_cases;
    phase_error_deal();
    printf("  ErrorDeal 整链                : %ld\n", g_cases - base);

    base = g_cases;
    phase_random(nrand);
    printf("  随机混合                      : %ld\n", g_cases - base);

    printf("f_error difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    return g_fails ? 1 : 0;
}
