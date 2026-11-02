/* f_fcDeal 差分测试驱动。
 *
 * 被测模块是"功能码的初值 / 清除 / 掉电保持 / 恢复出厂 / 互限"这一层。六个函数:
 *
 *   GetFuncCodeInit(index, type)   功能码初值: 一大串例外区间 + 逆变器类型相关项
 *   GetFuncCodeInitOriginal(index) 初值兜底: 查 funcCodeAttribute[index].init
 *   ClearRecordDeal()              清记录: 串行清一段 + 逐个清 6 项
 *   PowerOffRemDeal()              掉电保持: 把运行态写进 funcCode 的 remember 组
 *   RestoreCompanyParaOtherDeal()  恢复出厂: 两项清零 + MenuModeDeal + 互限
 *   LimitOtherCodeDeal(index)      互限: 按属性表把参数夹到上下限之间
 *
 * ---- 比对什么 ----
 *
 *  返回值                      六个函数的返回值 (四个是 void, 靠状态比)
 *  funcCode                    整个联合体, 逐字节
 *  funcCodeOneWriteIndex[]     写队列内容 (SaveOneFuncCode 的副作用)
 *  funcCodeOneWriteNum         写队列游标
 *  funcCodeRwModeTmp           串行写模式
 *  startIndexWriteSeries       串行写起点
 *  endIndexWriteSeries         串行写终点
 *  limitedByOtherCodeIndex[]   互限清单 (模块自己的导出)
 *  limitedByOtherCodeIndexNum  互限清单长度
 *  limitOtherCodeIndex[]       13 个可互限的功能码下标 (模块自己的导出)
 *  upDownFrq / frqFlag / runCmd / runDirPanelOld   恢复出厂要清的四样
 *  TorqueBoostDeal / MenuModeDeal 的调用次数与参数
 *
 * 写队列为什么要单独比: SaveOneFuncCode() 是模块里唯一"对外部世界产生副作用"的
 * 东西 (`funcCodeOneWriteIndex[funcCodeOneWriteNum++] = index`)。少写一次、写错
 * 下标、写错顺序, funcCode 本身可能完全看不出来 (比如同一个值被夹了两次), 但
 * 队列序列会不一样。它是这个模块最灵敏的观测量。
 *
 * ---- 共享状态 ----
 *
 * funcCode / 写队列 / invPara / 池子 等全部由 mock_fcdeal.c 定义, 两侧**共用**。
 * 每轮用例前 install(side) 从同一份输入快照恢复, 跑完 snapshot(side) 取回结果。
 * 模块自己的导出 (limitedByOtherCodeIndex 等) 是 ref_tu.c / new_tu.c 各一份,
 * 由 g_lim[] / g_limTable[] 按侧索引。
 *
 * ---- 有意不测的既有缺陷 (与 f_plc 同样的处理方式) ----
 *
 * 1. SaveOneFuncCode() 没有边界检查, 写队列只有 200 格。LimitOtherCodeDeal()
 *    最多连写 4 * limitedByOtherCodeIndexNum 次, 而 f_main.c 允许
 *    limitedByOtherCodeIndexNum 到 909 (LIMITED_BY_OTHER_CODE_NUM_MAX = 150 是
 *    数组容量, 不是写入上限)。真固件里这条路径能把队列写穿。
 *    本驱动把 limitedByOtherCodeIndexNum 压在 LIM_N_MAX = 16 以内
 *    (4 * 16 = 64 < 200), 并在 check_invariants() 里断言。
 *
 * 2. funcCodeAttribute[] 正好 FNUM_PARA = 909 项, LimitOtherCodeDeal() 读它
 *    **没有边界检查**。真固件里 limitedByOtherCodeIndex[] 由 f_main.c 填,
 *    填进去的 i 一律 < FNUM_PARA, 所以在表内。本驱动同样保证所有下标 < FNUM_PARA。
 *
 * 3. GetInvParaPointer(invType) 没有边界检查 (invType < lower 时按 Uint16 回绕)。
 *    本驱动只在 [lower, upper] 内取值。
 *
 * 三条都是"目标板上的真实越界, 宿主无法复现相邻内存里恰好是什么", 所以用例
 * 把它们列出来而不是悄悄绕开 —— 见 README 的「有意不测的既有缺陷」。
 *
 * ---- 负向对照要注意的: 死代码 ----
 *
 * f_fcDeal.c 里有两块死代码: `#if DEBUG_F_POSITION_CTRL` 的 `#pragma DATA_SECTION`
 * (DEBUG_F_POSITION_CTRL = 0) 和 GetFuncCodeInitOriginal() 里的 `#if 0` 块。
 * 死代码在行为上不可观测, 所以 negative_control.py 的 CASES 条目用
 * `live_span: None` + 逐条核对行号 —— 与 f_frqSrc 同一套做法。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 与 src/function/f_fcDeal.c 相同的 include 顺序。 */
#include "f_menu.h"
#include "f_main.h"
#include "f_eeprom.h"
#include "f_invPara.h"
#include "f_fcDeal.h"
#include "f_runSrc.h"
#include "f_frqSrc.h"

#include "mock_fcdeal.h"

/* ---- 驱动自己的上限 ----
 *
 * LIM_N_MAX 是**驱动**给 limitedByOtherCodeIndexNum 设的上限, 不是模块的。
 * 取 16 是为了 4 * 16 = 64 次写队列调用稳稳落在 200 格以内。
 * check_invariants() 会断言 FCD_WRITE_QUEUE_LEN >= 4 * LIM_N_MAX。 */
#define LIM_N_MAX        16
#define FAIL_LIMIT       20
#define Q_SNAP           FCD_WRITE_QUEUE_LEN

/* ---- 被测函数 ---- */

extern Uint16 ref_GetFuncCodeInit(Uint16 index, Uint16 type);
extern Uint16 new_GetFuncCodeInit(Uint16 index, Uint16 type);
extern Uint16 ref_GetFuncCodeInitOriginal(Uint16 index);
extern Uint16 new_GetFuncCodeInitOriginal(Uint16 index);
extern void   ref_ClearRecordDeal(void);
extern void   new_ClearRecordDeal(void);
extern void   ref_PowerOffRemDeal(void);
extern void   new_PowerOffRemDeal(void);
extern void   ref_RestoreCompanyParaOtherDeal(void);
extern void   new_RestoreCompanyParaOtherDeal(void);
extern void   ref_LimitOtherCodeDeal(Uint16 index);
extern void   new_LimitOtherCodeDeal(Uint16 index);

/* ---- 模块自己的导出 (每侧一份) ---- */

extern Uint16 ref_limitedByOtherCodeIndex[FCD_LIMITED_INDEX_MAX];
extern Uint16 new_limitedByOtherCodeIndex[FCD_LIMITED_INDEX_MAX];
extern Uint16 ref_limitedByOtherCodeIndexNum;
extern Uint16 new_limitedByOtherCodeIndexNum;
extern Uint16 ref_limitOtherCodeIndex[];
extern Uint16 new_limitOtherCodeIndex[];

/* ---- 用例种类与侧指针 ---- */

enum FCD_KIND
{
    K_INIT, K_ORIG, K_CLEAR, K_POWEROFF, K_RESTORE, K_LIMIT
};

static Uint16 (*g_init[2])(Uint16, Uint16);
static Uint16 (*g_initOrig[2])(Uint16);
static void   (*g_clear[2])(void);
static void   (*g_powerOff[2])(void);
static void   (*g_restore[2])(void);
static void   (*g_limit[2])(Uint16);

static Uint16 *g_lim[2];
static Uint16 *g_limNum[2];
static Uint16 *g_limTable[2];

static void init_ptrs(void)
{
    g_init[0]     = ref_GetFuncCodeInit;           g_init[1]     = new_GetFuncCodeInit;
    g_initOrig[0] = ref_GetFuncCodeInitOriginal;   g_initOrig[1] = new_GetFuncCodeInitOriginal;
    g_clear[0]    = ref_ClearRecordDeal;           g_clear[1]    = new_ClearRecordDeal;
    g_powerOff[0] = ref_PowerOffRemDeal;           g_powerOff[1] = new_PowerOffRemDeal;
    g_restore[0]  = ref_RestoreCompanyParaOtherDeal;
    g_restore[1]  = new_RestoreCompanyParaOtherDeal;
    g_limit[0]    = ref_LimitOtherCodeDeal;        g_limit[1]    = new_LimitOtherCodeDeal;

    g_lim[0]      = ref_limitedByOtherCodeIndex;   g_lim[1]      = new_limitedByOtherCodeIndex;
    g_limNum[0]   = &ref_limitedByOtherCodeIndexNum;
    g_limNum[1]   = &new_limitedByOtherCodeIndexNum;
    g_limTable[0] = ref_limitOtherCodeIndex;
    g_limTable[1] = new_limitOtherCodeIndex;
}

/* ---- 运行期重建 limitOtherCodeIndex[] ----
 *
 * 模块里这张表是:
 *
 *     LOCALF Uint16 const limitOtherCodeIndex[LIMIT_OTHER_CODE_NUM] =
 *     { PULSE_IN_MAX_INDEX, PULSE_IN_MIN_INDEX, ... };
 *
 * 而 PULSE_IN_MAX_INDEX 这些宏全都展开成 GetCodeIndex(funcCode.code.X), 也就是
 * **两个地址常量相减**。C28x 的 cl2000 折叠它, MSVC 的 C 前端拒绝 (error C2099,
 * 实测 6 种写法全被拒 —— 见 f_invPara / f_motorFc 的 host_patch.json 说明)。
 *
 * 所以宿主副本里这张表**没有初始值设定项** (host_patch.json 的 count=1 保证
 * 源码一改就失配报错), 由这里在跑之前填回去。填的是**同一批宏**, 不是另写一份
 * 公式 —— 宏名与源码里那张表逐个对应。
 *
 * 副作用要说清: 表**内容**的等价由目标端证明 —— build_and_compare.py 的 Tier B
 * 逐段比对 f_fcDeal.obj 的 .econst 内容, 那里的表是真编译器折叠出来的。宿主侧
 * 只保证"喂给两侧的是同一张表"。
 *
 * 顺序无关性: 模块只做 `if (limitOtherCodeIndex[j] == index) { ...; break; }`,
 * 而 13 个值互不相同 (下面断言), 命中是唯一的, 所以表的**排列**在行为上不可观测。
 * 这一点写进 README。 */
static void fill_limit_table(Uint16 *t)
{
    t[0]  = PULSE_IN_MAX_INDEX;
    t[1]  = PULSE_IN_MIN_INDEX;
    t[2]  = CURVE2_MAX_INDEX;
    t[3]  = CURVE2_MIN_INDEX;
    t[4]  = CURVE1_MAX_INDEX;
    t[5]  = CURVE1_MIN_INDEX;
    t[6]  = VF_FRQ3_INDEX;
    t[7]  = VF_FRQ2_INDEX;
    t[8]  = VF_FRQ1_INDEX;
    t[9]  = RATING_FRQ_INDEX;
    t[10] = LOWER_FRQ_INDEX;
    t[11] = UPPER_FRQ_INDEX;
    t[12] = MAX_FRQ_INDEX;
}

static void check_limit_table(void)
{
    Uint16 i, j;

    for (i = 0; i < FCD_LIMIT_OTHER_CODE_NUM; i++)
    {
        if (g_limTable[0][i] >= FNUM_PARA)
        {
            printf("FATAL: limitOtherCodeIndex[%u] = %u 超出参数区 FNUM_PARA=%d\n",
                   i, g_limTable[0][i], (int)FNUM_PARA);
            exit(2);
        }
        for (j = (Uint16)(i + 1); j < FCD_LIMIT_OTHER_CODE_NUM; j++)
        {
            if (g_limTable[0][i] == g_limTable[0][j])
            {
                printf("FATAL: limitOtherCodeIndex 里 %u 出现两次 (位置 %u 与 %u) —— "
                       "重复项会让「命中即 break」的语义不再与顺序无关\n",
                       g_limTable[0][i], i, j);
                exit(2);
            }
        }
    }
}

/* ---- 输入快照 / 结果快照 ----
 *
 * 输入里既有共享的 mock 状态 (funcCode / 队列 / invPara / 池子 ...), 也有模块
 * 自己的导出 (limitedByOtherCodeIndex 及其长度)。install() 两者都恢复, 于是两侧
 * 的起点逐字节相同。
 *
 * 池子 (6 * 64 项) 也一起进出: GetInvParaPointer() 会解引用它, 而"读错成员"
 * 这类错法只有池子里的值互不相同时才看得见 (全零池: 0 == 0)。 */

struct FCD_IN
{
    FUNCCODE_ALL fc;
    Uint16 q[Q_SNAP];
    Uint16 qNum;
    Uint16 rwMode, startSer, endSer;
    Uint16 lim[FCD_LIMITED_INDEX_MAX];
    Uint16 limNum;

    struct INV_PARA inv;
    union FRQ_FLAG frq;
    union RUN_CMD cmd;
    int32  upDownFrq;
    Uint16 plcStep;
    Uint32 plcTime;
    Uint16 pmsmRotorPos;
    Uint16 saveEepromIndex;
    Uint16 runDirPanelOld;

    int16 f2m[FCD_F2M_LEN];
    INV_TYPE_RELATED_PARA pool[INV_TYPE_VOLTAGE_NUM][FCD_INVPOOL_STRIDE];

    /* 替身控制: 这是"外部世界"的输入, 不是观测量。 */
    Uint16 tbBase;
};

struct FCD_OUT
{
    Uint16 ret;

    FUNCCODE_ALL fc;
    Uint16 q[Q_SNAP];
    Uint16 qNum;
    Uint16 rwMode, startSer, endSer;
    Uint16 lim[FCD_LIMITED_INDEX_MAX];
    Uint16 limNum;
    const Uint16 *limTable;

    int32  upDownFrq;
    Uint16 frqAll;
    Uint16 cmdAll;
    Uint16 runDirPanelOld;

    Uint16 tbCalls, tbLastArg, menuCalls;
};

static struct FCD_IN  g_in;
static struct FCD_OUT S[2];

/* ---- 当前用例描述 (报差异时打印, 便于复现) ---- */

static const char *g_fn;
static enum FCD_KIND g_kind;
static Uint16 g_arg0, g_arg1;
static Uint16 g_index, g_type;

static void set_kind(enum FCD_KIND k, Uint16 a0, Uint16 a1)
{
    g_kind = k;
    g_arg0 = a0;
    g_arg1 = a1;
    g_index = a0;
    g_type = a1;

    switch (k)
    {
    case K_INIT:     g_fn = "GetFuncCodeInit";            break;
    case K_ORIG:     g_fn = "GetFuncCodeInitOriginal";    g_type = 0; break;
    case K_CLEAR:    g_fn = "ClearRecordDeal";            g_index = 0; g_type = 0; break;
    case K_POWEROFF: g_fn = "PowerOffRemDeal";            g_index = 0; g_type = 0; break;
    case K_RESTORE:  g_fn = "RestoreCompanyParaOtherDeal"; g_index = 0; g_type = 0; break;
    default:         g_fn = "LimitOtherCodeDeal";         g_type = 0; break;
    }
}

static void install(int side)
{
    memcpy(&funcCode, &g_in.fc, sizeof g_in.fc);
    memcpy(funcCodeOneWriteIndex, g_in.q, sizeof g_in.q);
    funcCodeOneWriteNum = g_in.qNum;
    funcCodeRwModeTmp = g_in.rwMode;
    startIndexWriteSeries = g_in.startSer;
    endIndexWriteSeries = g_in.endSer;

    memcpy(g_lim[side], g_in.lim, sizeof g_in.lim);
    *g_limNum[side] = g_in.limNum;

    invPara = g_in.inv;
    frqFlag = g_in.frq;
    runCmd = g_in.cmd;
    upDownFrq = g_in.upDownFrq;
    plcStep = g_in.plcStep;
    plcTime = g_in.plcTime;
    pmsmRotorPos = g_in.pmsmRotorPos;
    saveEepromIndex = g_in.saveEepromIndex;
    runDirPanelOld = g_in.runDirPanelOld;

    memcpy(gSendToFunctionDataBuff, g_in.f2m, sizeof g_in.f2m);
    memcpy(fcdInvParaPool, g_in.pool, sizeof g_in.pool);

    mockTorqueBoostDealBase = g_in.tbBase;
    mockTorqueBoostDealCalls = 0;
    mockTorqueBoostDealLastArg = 0;
    mockMenuModeDealCalls = 0;
}

static void snapshot(int side)
{
    struct FCD_OUT *s = &S[side];

    memcpy(&s->fc, &funcCode, sizeof s->fc);
    memcpy(s->q, funcCodeOneWriteIndex, sizeof s->q);
    s->qNum = funcCodeOneWriteNum;
    s->rwMode = funcCodeRwModeTmp;
    s->startSer = startIndexWriteSeries;
    s->endSer = endIndexWriteSeries;

    memcpy(s->lim, g_lim[side], sizeof s->lim);
    s->limNum = *g_limNum[side];
    s->limTable = g_limTable[side];

    s->upDownFrq = upDownFrq;
    s->frqAll = frqFlag.all;
    s->cmdAll = runCmd.all;
    s->runDirPanelOld = runDirPanelOld;

    s->tbCalls = mockTorqueBoostDealCalls;
    s->tbLastArg = mockTorqueBoostDealLastArg;
    s->menuCalls = mockMenuModeDealCalls;
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
    printf("  index=%u type=%u limNum=%u qNum=%u tbBase=%u\n",
           g_index, g_type, g_in.limNum, g_in.qNum, g_in.tbBase);
    printf("  invPara type=%u ratingVoltage=%u volLevel=%u pointLimit=%u bitAccDecStart=%u\n",
           g_in.inv.type, g_in.inv.ratingVoltage, g_in.inv.volLevel,
           g_in.inv.pointLimit, g_in.inv.bitAccDecStart);
    printf("  upDownFrq=%ld plcStep=%u plcTime=0x%08lX pmsmRotorPos=%u saveEeprom=%u\n",
           (long)g_in.upDownFrq, g_in.plcStep,
           (unsigned long)g_in.plcTime, g_in.pmsmRotorPos, g_in.saveEepromIndex);
}

/* ---- 跑一轮并比对 ----
 *
 * 字段一个一比而不是整块 memcmp: 报出来的是"哪个观测量不一样", 不是"第几个字节
 * 不一样"。而且结构体里有指针 (limTable), 整块 memcmp 会把指针值算进去 ——
 * 两侧的表地址本来就不同, 那会变成假阳性。 */
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
        case K_INIT:     S[side].ret = g_init[side](g_arg0, g_arg1); break;
        case K_ORIG:     S[side].ret = g_initOrig[side](g_arg0);     break;
        case K_CLEAR:    g_clear[side]();                            break;
        case K_POWEROFF: g_powerOff[side]();                         break;
        case K_RESTORE:  g_restore[side]();                          break;
        default:         g_limit[side](g_arg0);                      break;
        }

        snapshot(side);
    }

    if (S[0].ret != S[1].ret) { report("return value"); return; }
    if (memcmp(&S[0].fc, &S[1].fc, sizeof S[0].fc) != 0) { report("funcCode"); return; }
    if (S[0].qNum != S[1].qNum) { report("funcCodeOneWriteNum"); return; }
    if (memcmp(S[0].q, S[1].q, sizeof S[0].q) != 0) { report("funcCodeOneWriteIndex"); return; }
    if (S[0].rwMode != S[1].rwMode) { report("funcCodeRwModeTmp"); return; }
    if (S[0].startSer != S[1].startSer) { report("startIndexWriteSeries"); return; }
    if (S[0].endSer != S[1].endSer) { report("endIndexWriteSeries"); return; }
    if (S[0].limNum != S[1].limNum) { report("limitedByOtherCodeIndexNum"); return; }
    if (memcmp(S[0].lim, S[1].lim, sizeof S[0].lim) != 0) { report("limitedByOtherCodeIndex"); return; }
    if (memcmp(S[0].limTable, S[1].limTable,
               FCD_LIMIT_OTHER_CODE_NUM * sizeof(Uint16)) != 0)
    {
        report("limitOtherCodeIndex"); return;
    }
    if (S[0].upDownFrq != S[1].upDownFrq) { report("upDownFrq"); return; }
    if (S[0].frqAll != S[1].frqAll) { report("frqFlag.all"); return; }
    if (S[0].cmdAll != S[1].cmdAll) { report("runCmd.all"); return; }
    if (S[0].runDirPanelOld != S[1].runDirPanelOld) { report("runDirPanelOld"); return; }
    if (S[0].tbCalls != S[1].tbCalls) { report("TorqueBoostDeal 调用次数"); return; }
    if (S[0].tbLastArg != S[1].tbLastArg) { report("TorqueBoostDeal 参数"); return; }
    if (S[0].menuCalls != S[1].menuCalls) { report("MenuModeDeal 调用次数"); return; }
}

/* ---- 随机数 ----
 *
 * 自带 LCG, 不用 rand(): 一是各平台 rand() 序列不同, 二是这里要的只是"同一份输入
 * 喂给两侧", 与随机性质量无关。 */
static Uint32 g_seed = 0x12345678UL;

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
 * 这些断言的意义: 用例必须"要么真的测到了, 要么大声失败", 不能因为替身表摆得
 * 不对而悄悄少测一条分支。 */
static void check_invariants(void)
{
    Uint16 combos[8];
    Uint16 i;

    /* 1. 写队列够不够 4 * LIM_N_MAX 次连写 */
    if (FCD_WRITE_QUEUE_LEN < 4 * LIM_N_MAX)
    {
        printf("FATAL: 写队列 %d 格, 装不下 4 * LIM_N_MAX = %d 次\n",
               FCD_WRITE_QUEUE_LEN, 4 * LIM_N_MAX);
        exit(2);
    }

    /* 2. 替身表长度必须够 FNUM_PARA 项。编译期已断言 ATTR_TABLE_N == FCD_ATTR_N,
     *    这里再确认 FCD_ATTR_N 确实 >= FNUM_PARA —— 将来若把 FCD_ATTR_N 改成
     *    别的式子, 这一条仍然成立。 */
    if (FCD_ATTR_N < FNUM_PARA)
    {
        printf("FATAL: 替身属性表 %d 项 < FNUM_PARA %d\n", FCD_ATTR_N, (int)FNUM_PARA);
        exit(2);
    }

    /* 3. 8 种 (upperLimit, lowerLimit, signal) 组合都要在表里出现, 否则
     *    LimitOtherCodeDeal() 的某个分支永远走不到。生成器负责摆位, 这里负责
     *    用 `.bit` 读回来数一遍 —— 与生成器里的位序假设解耦。 */
    for (i = 0; i < 8; i++)
    {
        combos[i] = 0;
    }
    for (i = 0; i < FCD_ATTR_N; i++)
    {
        Uint16 v = (Uint16)(funcCodeAttribute[i].attribute.bit.upperLimit
                          | (funcCodeAttribute[i].attribute.bit.lowerLimit << 1)
                          | (funcCodeAttribute[i].attribute.bit.signal << 2));

        combos[v]++;
    }
    for (i = 0; i < 8; i++)
    {
        if (combos[i] == 0)
        {
            printf("FATAL: 属性表里没有 (upperLimit,lowerLimit,signal) = (%u,%u,%u) 的组合\n",
                   i & 1, (i >> 1) & 1, (i >> 2) & 1);
            exit(2);
        }
    }
    printf("属性表 8 种组合的项数: %u %u %u %u %u %u %u %u\n",
           combos[0], combos[1], combos[2], combos[3],
           combos[4], combos[5], combos[6], combos[7]);

    /* 4. 表里每个 lower / upper 都必须是**合法功能码下标** —— 模块会拿它去读
     *    funcCode.all[pAttribute->upper]。越界就是宿主复现不了的 UB。 */
    for (i = 0; i < FCD_ATTR_N; i++)
    {
        if (funcCodeAttribute[i].lower >= FNUM_PARA
            || funcCodeAttribute[i].upper >= FNUM_PARA)
        {
            printf("FATAL: 属性表第 %u 项 lower=%u upper=%u 超出 FNUM_PARA=%d\n",
                   i, funcCodeAttribute[i].lower, funcCodeAttribute[i].upper,
                   (int)FNUM_PARA);
            exit(2);
        }
    }
}

/* ---- 输入构造工具 ---- */

static void zero_in(void)
{
    memset(&g_in, 0, sizeof g_in);
}

/* 把池子填成确定值。刻意让 carrierFrq 与 antiVibrateGain 取不同数量级 ——
 * 读错成员时 (比如该读 carrierFrq 却读了 antiVibrateGain) 结果就不一样。 */
static void fill_pool(void)
{
    Uint16 v, k;

    for (v = 0; v < INV_TYPE_VOLTAGE_NUM; v++)
    {
        for (k = 0; k < FCD_INVPOOL_STRIDE; k++)
        {
            g_in.pool[v][k].elem.carrierFrq = (Uint16)(1000 + v * 100 + k);
            g_in.pool[v][k].elem.antiVibrateGain = (Uint16)(7 + v * 3 + k);
            g_in.pool[v][k].elem.ratingPower = (Uint16)(1000 * (k + 1));
        }
    }
}

/* 把 (volLevel, invType) 设成一对**合法**组合: invType 落在该电压等级自己的
 * [lower, upper] 内, 这样 GetInvParaPointer() 不会跑出池子。 */
static void set_inv_pair(Uint16 volLevel, Uint16 invType)
{
    g_in.inv.volLevel = volLevel;
    g_in.inv.type = invType;
    g_in.inv.ratingVoltage = invTypeLimitTable[volLevel].vol;
}

/* 写队列游标: 给一个起始值, 但保证后面最多 nwrite 次 SaveOneFuncCode 不越界。 */
static void set_qnum(Uint16 start, Uint16 nwrite)
{
    if (start > (Uint16)(FCD_WRITE_QUEUE_LEN - nwrite))
    {
        start = (Uint16)(FCD_WRITE_QUEUE_LEN - nwrite);
    }
    g_in.qNum = start;
}

/* ---- 阶段 1: GetFuncCodeInitOriginal(index) ----
 *
 * 只有四条出路: index < FNUM_PARA 取表里的 init; 两个 SAVE_USER_PARA_* ;
 * 其余给 0。所以下标扫满 [0, FNUM_PARA] 再补几个远大于它的值就够。
 * 两个 saveUserParaFlag 也要变化 —— 它们是被返回的初值来源。 */
static void phase_orig(void)
{
    static const Uint16 EXTRA[] =
        { 0, 1, 908, 909, 910, 960, 961, 1100, 1101, 1102, 1103, 1115, 1116, 65535 };
    Uint16 i;
    Uint16 f;

    for (i = 0; i <= FNUM_PARA; i++)
    {
        for (f = 0; f < 3; f++)
        {
            zero_in();
            fill_pool();
            g_in.fc.code.saveUserParaFlag1 = (f == 1) ? 0x1234 : 0;
            g_in.fc.code.saveUserParaFlag2 = (f == 2) ? 0xABCD : 0;
            set_kind(K_ORIG, i, 0);
            run_case();
        }
    }

    for (i = 0; i < (Uint16)(sizeof EXTRA / sizeof EXTRA[0]); i++)
    {
        zero_in();
        fill_pool();
        g_in.fc.code.saveUserParaFlag1 = 0x5555;
        g_in.fc.code.saveUserParaFlag2 = 0xAAAA;
        set_kind(K_ORIG, EXTRA[i], 0);
        run_case();
    }
}

/* ---- 阶段 2: GetFuncCodeInit(index, type) 全下标扫描 ----
 *
 * 下标 0..FNUM_ALL-1 一个不落, type 取 0 与非 0 各一次。初值分支里那些
 * `INIT_EXCEPT_*` 宏全都是 GetCodeIndex(funcCode.code.X) 或某个组的首末下标,
 * 所以"扫满下标"就等于把每个区间端点、区间内部、区间之间都走到了。
 *
 * invPara 固定成一组**合法**组合 (volLevel 0 = T380, type = lower = 8),
 * 于是 CARRIER_FRQ_INDEX / ANTI_VIBRATE_GAIN_* 这两条也走得到。
 */
static void phase_init_sweep(void)
{
    Uint16 index;
    Uint16 t;

    for (index = 0; index < FNUM_ALL; index++)
    {
        for (t = 0; t < 2; t++)
        {
            zero_in();
            fill_pool();
            set_inv_pair(0, INV_TYPE_LOWER_LIMIT_T380);
            g_in.inv.bitAccDecStart = 12;
            g_in.tbBase = 0x0100;
            /* 让每个下标上的值都不一样 —— 例外区间里 `init = funcCode.all[index]`
             * 这类赋值才有意义。 */
            g_in.fc.all[index] = (Uint16)(index * 3 + 7);
            set_kind(K_INIT, index, t);
            run_case();
        }
    }
}

/* ---- 阶段 3: 逆变器类型相关的那几条分支 ----
 *
 * CARRIER_FRQ_INDEX / ANTI_VIBRATE_GAIN_* 走 GetInvParaPointer(invPara.type),
 * OV_POINT_SET_INDEX 走 ovVoltageInitValue[invPara.volLevel],
 * SVC_MODE_INDX 走 `invPara.ratingVoltage == 1140`,
 * TORQUE_BOOST_* 走 TorqueBoostDeal(某个电机的 ratingPower)。
 *
 * (volLevel, type) 只取**合法**对: 每个电压等级的 lower / lower+1 / 中间 / upper-1 / upper。
 * 真表六段长度不同 (T380 30 / T220 16 / S220 7 / T480 27 / T690 16 / T1140 19),
 * 取 lower 与 upper 就把每段的两个端点都覆盖了。
 */
static void phase_inv_type(void)
{
    /* 不能加 `static`: CARRIER_FRQ_INDEX 这些宏展开成 GetCodeIndex(...), 也就是
     * **两个地址常量相减**, 不是 C 的常量表达式, MSVC 在文件作用域/静态存储期的
     * 初始化式里直接报 C2099。自动存储期就没这个限制。 */
    const Uint16 IDX[] =
    {
        CARRIER_FRQ_INDEX,
        ANTI_VIBRATE_GAIN_INDEX,
        ANTI_VIBRATE_GAIN_MOTOR2_INDEX,
        ANTI_VIBRATE_GAIN_MOTOR3_INDEX,
        ANTI_VIBRATE_GAIN_MOTOR4_INDEX,
        TORQUE_BOOST_INDEX,
        TORQUE_BOOST_MOTOR2_INDEX,
        TORQUE_BOOST_MOTOR3_INDEX,
        TORQUE_BOOST_MOTOR4_INDEX,
        SVC_MODE_INDX,
        OV_POINT_SET_INDEX,
    };
    Uint16 v;

    for (v = 0; v < INV_TYPE_VOLTAGE_NUM; v++)
    {
        Uint16 lo = invTypeLimitTable[v].lower;
        Uint16 hi = invTypeLimitTable[v].upper;
        Uint16 cand[5];
        Uint16 c;

        cand[0] = lo;
        cand[1] = (Uint16)(lo + 1);
        cand[2] = (Uint16)(lo + (Uint16)((hi - lo) / 2));
        cand[3] = (Uint16)(hi - 1);
        cand[4] = hi;

        for (c = 0; c < 5; c++)
        {
            Uint16 t;

            for (t = 0; t < (Uint16)(sizeof IDX / sizeof IDX[0]); t++)
            {
                Uint16 ty;

                for (ty = 0; ty < 2; ty++)
                {
                    zero_in();
                    fill_pool();
                    set_inv_pair(v, cand[c]);
                    g_in.tbBase = 0x0300;
                    g_in.fc.code.motorParaM1.elem.ratingPower = 0x1111;
                    g_in.fc.code.motorFcM2.motorPara.elem.ratingPower = 0x2222;
                    g_in.fc.code.motorFcM3.motorPara.elem.ratingPower = 0x3333;
                    g_in.fc.code.motorFcM4.motorPara.elem.ratingPower = 0x4444;
                    set_kind(K_INIT, IDX[t], ty);
                    run_case();
                }
            }
        }
    }

    /* ratingVoltage 的 1140 判据: 用 SVC_MODE_INDX 打一遍边界。
     * 真表里 1140 只出现在 T1140 那一档 (invTypeLimitTable[5].vol)。 */
    {
        static const Uint16 RV[] = { 0, 1, 379, 380, 1139, 1140, 1141, 65535 };
        Uint16 r;

        for (r = 0; r < (Uint16)(sizeof RV / sizeof RV[0]); r++)
        {
            zero_in();
            fill_pool();
            set_inv_pair(5, INV_TYPE_LOWER_LIMIT_T1140);
            g_in.inv.ratingVoltage = RV[r];
            set_kind(K_INIT, SVC_MODE_INDX, 0);
            run_case();
        }
    }
}

/* ---- 阶段 4: 加/减速时间那 8 个下标 ----
 *
 * `invPara.type < invPara.bitAccDecStart` 是唯一判据, 两个方向都要走到。
 * 注意这里**不能**用 set_inv_pair(): invPara.type 在这里只是个被比较的数,
 * 不参与 GetInvParaPointer, 所以可以自由取值。 */
static void phase_accdec(void)
{
    /* 同上: 这些宏不是常量表达式, 不能放静态存储期。 */
    const Uint16 IDX[] =
    {
        ACC_TIME1_INDEX, DEC_TIME1_INDEX,
        ACC_TIME2_INDEX, DEC_TIME2_INDEX,
        ACC_TIME3_INDEX, DEC_TIME3_INDEX,
        ACC_TIME4_INDEX, DEC_TIME4_INDEX,
    };
    static const Uint16 START[] = { 0, 1, 8, 20, 65535 };
    static const Uint16 TYPE[]  = { 0, 1, 7 };
    Uint16 t;
    Uint16 s;

    for (t = 0; t < (Uint16)(sizeof IDX / sizeof IDX[0]); t++)
    {
        for (s = 0; s < (Uint16)(sizeof START / sizeof START[0]); s++)
        {
            Uint16 ty;

            for (ty = 0; ty < (Uint16)(sizeof TYPE / sizeof TYPE[0]); ty++)
            {
                zero_in();
                fill_pool();
                g_in.inv.volLevel = 0;
                g_in.inv.type = TYPE[ty];
                g_in.inv.bitAccDecStart = START[s];
                set_kind(K_INIT, IDX[t], 0);
                run_case();
            }
        }
    }
}

/* ---- 阶段 5: ClearRecordDeal() ----
 *
 * 副作用: 把 errorLatest1..LAST_ERROR_RECORD_INDEX 一段清零, 再逐个清 6 项,
 * 每次都 SaveOneFuncCode。队列序列是主要观测量。
 * 输入里 funcCode 的整段内容随机 —— "清零"这件事只有在原来非零时才看得出来。 */
static void phase_clear(void)
{
    static const Uint16 QN[] = { 0, 1, 5, 20, 100, 194 };
    Uint16 qi;
    Uint16 k;

    for (qi = 0; qi < (Uint16)(sizeof QN / sizeof QN[0]); qi++)
    {
        for (k = 0; k < 8; k++)
        {
            Uint16 i;

            zero_in();
            fill_pool();
            for (i = 0; i < FNUM_ALL; i++)
            {
                g_in.fc.all[i] = (Uint16)(0xA000 + i * 5 + k);
            }
            set_qnum(QN[qi], 6);
            set_kind(K_CLEAR, 0, 0);
            run_case();
        }
    }
}

/* ---- 阶段 6: PowerOffRemDeal() ----
 *
 * 掉电保持。关键点:
 *   plcStep >= PLC_STEP_MAX (16) 时 plcStepRem 归零 —— 边界两侧都要有。
 *   plcTime 拆成高低两个 16 位字 —— 取全 0 / 全 1 / 只低半 / 只高半。
 *   saveEepromIndex 非零时多写一次队列 —— 0 与非 0 都要有。
 *   upDownFrq 是 int32, 存进 funcCode 的哪个成员由模块决定, 所以正负、越界值都要给。
 *   gSendToFunctionDataBuff[30..34] 被读走 5 个字 (REM_P_OFF_MOTOR)。
 */
static void phase_poweroff(void)
{
    static const Uint16 STEP[] = { 0, 1, 15, 16, 17, 65535 };
    static const Uint32 TIME[] = { 0x00000000UL, 0xFFFFFFFFUL, 0x0000FFFFUL,
                                   0xFFFF0000UL, 0x12345678UL };
    static const int32  FRQ[]  = { 0, 1, -1, 32767, -32768, 2147483647L,
                                   (int32)0x80000000UL };
    static const Uint16 SAVE[] = { 0, 1, 0x00FF, 0xFFFF };
    Uint16 a, b, c, d;

    for (a = 0; a < (Uint16)(sizeof STEP / sizeof STEP[0]); a++)
    {
        for (b = 0; b < (Uint16)(sizeof TIME / sizeof TIME[0]); b++)
        {
            for (c = 0; c < (Uint16)(sizeof FRQ / sizeof FRQ[0]); c++)
            {
                for (d = 0; d < (Uint16)(sizeof SAVE / sizeof SAVE[0]); d++)
                {
                    Uint16 i;

                    zero_in();
                    fill_pool();
                    for (i = 0; i < FCD_F2M_LEN; i++)
                    {
                        g_in.f2m[i] = (int16)(0x4000 + i * 3);
                    }
                    g_in.plcStep = STEP[a];
                    g_in.plcTime = TIME[b];
                    g_in.upDownFrq = FRQ[c];
                    g_in.saveEepromIndex = SAVE[d];
                    g_in.pmsmRotorPos = (Uint16)(0x2000 + a * 16 + b);
                    set_qnum(0, 5);
                    set_kind(K_POWEROFF, 0, 0);
                    run_case();
                }
            }
        }
    }

    /* 队列游标接近满格时也不能越界 (PowerOffRemDeal 最多写 5 次)。 */
    {
        static const Uint16 QN[] = { 100, 195, 196 };
        Uint16 qi;

        for (qi = 0; qi < (Uint16)(sizeof QN / sizeof QN[0]); qi++)
        {
            zero_in();
            fill_pool();
            g_in.saveEepromIndex = 1;
            g_in.plcStep = 16;
            g_in.plcTime = 0xABCD1234UL;
            g_in.upDownFrq = -12345;
            set_qnum(QN[qi], 5);
            set_kind(K_POWEROFF, 0, 0);
            run_case();
        }
    }
}

/* ---- 阶段 7: RestoreCompanyParaOtherDeal() ----
 *
 * 两项清零 + 两次 SaveOneFuncCode + MenuModeDeal + LimitOtherCodeDeal(RATING_FRQ_INDEX)
 * + 四样运行态清零。RATING_FRQ_INDEX 确实在 limitOtherCodeIndex[] 里 (第 10 位),
 * 所以互限那段一定会跑 —— 它同时也是"带前序状态"的 LimitOtherCodeDeal 版本。
 *
 * limNum 从 0 扫到 LIM_N_MAX: 0 时互限内层循环空转, 非 0 时才真夹。
 */
static void phase_restore(void)
{
    Uint16 n;

    for (n = 0; n <= LIM_N_MAX; n++)
    {
        Uint16 k;

        for (k = 0; k < 4; k++)
        {
            Uint16 i;

            zero_in();
            fill_pool();
            g_in.limNum = n;
            for (i = 0; i < n; i++)
            {
                /* 用一批"一定带上下限"的下标填清单 —— 否则内层循环空转,
                 * 夹取逻辑一次都走不到。生成器里第 i*8+3 项三位全 1。 */
                g_in.lim[i] = (Uint16)((i * 8 + 3 + k * 8) % FNUM_PARA);
                g_in.fc.all[g_in.lim[i]] = (Uint16)(0x1000 + i * 37 + k);
            }
            g_in.fc.code.motorDebugFc = (Uint16)(0x111 * k);
            g_in.fc.code.aiaoCalibrateDisp = (Uint16)(0x222 * k);
            g_in.fc.all[RATING_FRQ_INDEX] = (Uint16)(0x300 + k);
            g_in.upDownFrq = (int32)(k * 1000 - 500);
            g_in.frq.all = (Uint16)(0x00F0 + k);
            g_in.cmd.all = (Uint16)(0x1234 + k);
            g_in.runDirPanelOld = (Uint16)(0x55 + k);
            set_qnum(0, (Uint16)(2 + 4 * n));
            set_kind(K_RESTORE, 0, 0);
            run_case();
        }
    }
}

/* ---- 阶段 8: LimitOtherCodeDeal(index) 单条目扫描 ----
 *
 * 模块里逻辑最密的一段。要走到的东西:
 *   index 命中 limitOtherCodeIndex[] 的 13 个成员之一 (否则整个函数空转);
 *   upperLimit / lowerLimit / signal 三位的每一种组合 (表里 8 种轮转);
 *   `fc1 > upper1` 成立与不成立;
 *   `fc1 < lower1` 成立与不成立, 且分 `tmp` 为 0 / 非 0 两支 (也就是
 *   "上夹已改过值" 与 "上夹没改" 两种前序);
 *   外层 k 循环跑两遍带来的"第二遍看到第一遍的结果";
 *   limitedByOtherCodeIndex[] 里多个条目的相互影响 (前一条把某个 lower 下标写了,
 *   后一条读的就是新值)。
 *
 * 单条目扫描: 挑一个下标 t, 令 limitedByOtherCodeIndex[0] = t, 然后把
 * funcCode.all[t] / all[attr.upper] / all[attr.lower] 三个值摆成各种相对关系。
 * 表里 attr.upper / attr.lower 恒不相等 (生成器保证了这一点)。
 */
static void phase_limit_single(void)
{
    /* 13 个成员 + 若干非成员, 覆盖"命中"与"不命中"。
     * 同上: MAX_FRQ_INDEX 这些宏不是常量表达式, 不能放静态存储期。 */
    const Uint16 IDX[] =
    {
        MAX_FRQ_INDEX, UPPER_FRQ_INDEX, LOWER_FRQ_INDEX, RATING_FRQ_INDEX,
        VF_FRQ1_INDEX, VF_FRQ2_INDEX, VF_FRQ3_INDEX,
        CURVE1_MIN_INDEX, CURVE1_MAX_INDEX,
        CURVE2_MIN_INDEX, CURVE2_MAX_INDEX,
        PULSE_IN_MIN_INDEX, PULSE_IN_MAX_INDEX,
        0, 9, 11, 13, 35, 173, 175, 908, 909, 65535,
    };
    /* funcCode.all[t] 的取值: 含 0x7FFF / 0x8000 两侧 —— signal 位打开时
     * `(int32)(int16)` 会把 >= 0x8000 解释成负数, 比较方向随之翻转。 */
    static const Uint16 V[] =
    {
        0, 1, 2, 0x7FFE, 0x7FFF, 0x8000, 0x8001, 0xFFFE, 0xFFFF, 0x4000, 0xC000,
    };
    static const Uint16 UL[] = { 0, 1, 0x7FFF, 0x8000, 0xFFFF };
    Uint16 a;

    for (a = 0; a < (Uint16)(sizeof IDX / sizeof IDX[0]); a++)
    {
        Uint16 t = IDX[a];
        Uint16 b, c, d, e;

        if (t >= FNUM_PARA)
        {
            /* 非成员下标只用来验"不命中就空转", 不用去查表。 */
            zero_in();
            fill_pool();
            g_in.limNum = 1;
            g_in.lim[0] = (Uint16)(t % FNUM_PARA);
            g_in.fc.all[g_in.lim[0]] = 0x5555;
            set_qnum(0, 4);
            set_kind(K_LIMIT, t, 0);
            run_case();
            continue;
        }

        for (b = 0; b < (Uint16)(sizeof V / sizeof V[0]); b++)
        {
            for (c = 0; c < (Uint16)(sizeof UL / sizeof UL[0]); c++)
            {
                for (d = 0; d < (Uint16)(sizeof UL / sizeof UL[0]); d++)
                {
                    for (e = 0; e < 2; e++)
                    {
                        zero_in();
                        fill_pool();
                        g_in.limNum = (Uint16)(1 + e);
                        g_in.lim[0] = t;
                        /* 第二条目用同一个 t 的下一个下标: 制造"两条互相看得见"
                         * 的前后依赖 (前一条写过的值, 后一条读)。 */
                        g_in.lim[1] = (Uint16)((t + 1) % FNUM_PARA);
                        g_in.fc.all[t] = V[b];
                        g_in.fc.all[funcCodeAttribute[t].upper] = UL[c];
                        g_in.fc.all[funcCodeAttribute[t].lower] = UL[d];
                        g_in.fc.all[g_in.lim[1]] = (Uint16)(V[b] ^ 0x0100);
                        set_qnum(0, (Uint16)(4 * g_in.limNum));
                        set_kind(K_LIMIT, t, 0);
                        run_case();
                    }
                }
            }
        }
    }
}

/* ---- 阶段 9: LimitOtherCodeDeal 多条目 + 随机内容 ----
 *
 * 单条目扫描把"三个值的相对关系"扫干净了, 但"条目之间怎么互相踩"只覆盖了两条。
 * 这一段用 1..LIM_N_MAX 条随机条目、随机属性下标组合来补。 */
static void phase_limit_multi(void)
{
    Uint16 n;

    for (n = 1; n <= LIM_N_MAX; n++)
    {
        Uint16 k;

        for (k = 0; k < 40; k++)
        {
            Uint16 i;

            zero_in();
            fill_pool();
            g_in.limNum = n;
            for (i = 0; i < n; i++)
            {
                Uint16 t = rnd_n(FNUM_PARA);

                g_in.lim[i] = t;
                g_in.fc.all[t] = rnd();
                g_in.fc.all[funcCodeAttribute[t].upper] = rnd();
                g_in.fc.all[funcCodeAttribute[t].lower] = rnd();
            }
            set_qnum(rnd_n(20), (Uint16)(4 * n));
            /* 从 g_limTable[0] 里挑一个成员下标 —— 两侧的表内容每轮都会被比对,
             * 所以用哪一侧的都一样; 用 [0] 只是省一次分支。 */
            set_kind(K_LIMIT, g_limTable[0][rnd_n(FCD_LIMIT_OTHER_CODE_NUM)], 0);
            run_case();
        }
    }
}

/* ---- 阶段 9b: LimitOtherCodeDeal 的"必须跑第二遍"构造 ----
 *
 * 这一段是为负向对照里两条变异专门建的 (k 循环次数 2 -> 1, k >= 0 -> k > 0)。
 * 没有它, 那两条变异**抓不到** —— 理由不是"测试台不灵", 而是随机输入下这套夹取
 * 一遍就收敛了:
 *
 *   外层 k 循环让同一批条目夹两遍。一遍就收敛的场合是"某个条目写到自己前面
 *   (下标更大、本遍已经处理过) 的槽位" —— 那一遍里没人再看它, 只有第二遍才会
 *   重新夹。要触发它, 必须让**写方在处理顺序上排在读方后面**, 而读方的槽位正是
 *   写方的下限来源。
 *
 * 具体构造 (list 长度 2, 下标 1 先处理、下标 0 后处理):
 *
 *   lim[0] = W   处理顺序最后。W 的属性同时有上夹与下夹, 且 `attr(W).lower == T`。
 *   lim[1] = T   处理顺序最先。T 有下夹。
 *
 *   funcCode.all[W]      = 0x7FF0   远大于 upper_W -> 上夹命中, tmp=1
 *   funcCode.all[attr(W).upper] = 0x0020   = upper_W, 故意小于 lower_T
 *   funcCode.all[T]      = 0x0180   同时在 T 的 [lower_T, upper_T] 内
 *   funcCode.all[attr(T).lower] = 0x0100   = lower_T
 *   funcCode.all[attr(T).upper] = 0x4000   = upper_T, 高到不会触发上夹
 *
 *   第一遍: T 先跑 -> 0x0180 在范围内, 不动。
 *           W 后跑 -> 上夹把 all[W] 压到 0x0020 (tmp=1), 下夹 0x0020 < all[T]=0x0180
 *                     成立 -> 走 tmp 那一支, 把 all[T] 写成 0x0020 (越出下限)。
 *   到此为止 all[T] 已越界, 但本遍没人再看它。
 *   第二遍: T 先跑 -> 0x0020 < lower_T=0x0100 -> 抬回 0x0100。
 *           W 后跑 -> 上夹不再命中; 下夹 0x0020 < 0x0100 -> all[W] = all[T] = 0x0100。
 *
 *   一遍的结果 (all[T]=0x0020, all[W]=0x0020) 与两遍的 (0x0100, 0x0100) 不同,
 *   写队列也不同 (2 次 vs 4 次)。所以"少跑一遍"必然被抓。
 *
 * 构造在运行时**搜**出来, 而不是把下标写死: 属性表是生成物, 下标会随生成器变。
 * 找不到就直接 exit(2) —— 用例要么真测到, 要么大声失败。 */
static void phase_limit_chain(void)
{
    Uint16 w;
    Uint16 found = 0;

    for (w = 0; w < FNUM_PARA && found < 8; w++)
    {
        const FUNCCODE_ATTRIBUTE *pw = &funcCodeAttribute[w];
        Uint16 t;
        const FUNCCODE_ATTRIBUTE *pt;
        Uint16 idx[5];
        Uint16 i, j;
        Uint16 clash = 0;

        if (!pw->attribute.bit.upperLimit) { continue; }
        if (!pw->attribute.bit.lowerLimit) { continue; }
        if (pw->attribute.bit.signal)      { continue; }

        t = pw->lower;
        if (t >= FNUM_PARA || t == w)      { continue; }

        pt = &funcCodeAttribute[t];
        if (!pt->attribute.bit.lowerLimit) { continue; }
        if (pt->attribute.bit.signal)      { continue; }

        /* 五个槽位必须互不相同 —— 否则后写的会盖掉先写的, 构造就不成立了。 */
        idx[0] = w;
        idx[1] = pw->upper;
        idx[2] = t;
        idx[3] = pt->upper;
        idx[4] = pt->lower;
        for (i = 0; i < 5 && !clash; i++)
        {
            for (j = (Uint16)(i + 1); j < 5; j++)
            {
                if (idx[i] == idx[j])
                {
                    clash = 1;
                    break;
                }
            }
        }
        if (clash) { continue; }

        zero_in();
        fill_pool();
        g_in.limNum = 2;
        g_in.lim[0] = w;
        g_in.lim[1] = t;

        g_in.fc.all[w] = 0x7FF0;
        g_in.fc.all[pw->upper] = 0x0020;
        g_in.fc.all[t] = 0x0180;
        g_in.fc.all[pt->lower] = 0x0100;
        g_in.fc.all[pt->upper] = 0x4000;

        set_qnum(0, 4 * 2);
        set_kind(K_LIMIT, g_limTable[0][0], 0);
        run_case();

        /* 构造是否真的"一遍不收敛": 一遍只会有 2 次写队列调用 (W 的上夹 + W 的下夹),
         * 两遍会有 4 次 (第二遍 T 的下夹 + W 的下夹)。这里断在**基准侧的结果**上,
         * 所以构造是"验过的"而不是"推的"。真要是推导错了, 用例当场大声失败,
         * 而不是悄悄退化成一条测不到东西的样本。 */
        if (S[0].qNum <= 2)
        {
            printf("FATAL: 两遍构造没生效 (w=%u t=%u, qNum=%u) —— "
                   "k 循环那两条变异会变成不可观测\n", w, t, S[0].qNum);
            exit(2);
        }
        found++;
    }

    if (found == 0)
    {
        printf("FATAL: 属性表里找不到可构造「必须跑第二遍」的 (W, T) 组合 —— "
               "k 循环那两条变异将无法被观测\n");
        exit(2);
    }
}

/* ---- 阶段 10: 随机混合 ----
 *
 * 每个随机用例随机挑一个函数来打, 输入整体随机。前九个阶段是"每个分支都要有
 * 样本", 这一段是"分支之间的组合不要有漏网"。
 *
 * 随机输入必须**受限**, 三个硬约束 (越界就是宿主复现不了的 UB):
 *   index < FNUM_ALL            —— funcCode.all[] 的长度
 *   limitedByOtherCodeIndex[] 的值 < FNUM_PARA 且条数 <= LIM_N_MAX
 *   (volLevel, invPara.type) 合法 —— GetInvParaPointer 不越池
 */
static void phase_random(long n)
{
    long k;

    for (k = 0; k < n; k++)
    {
        Uint16 which = rnd_n(6);
        Uint16 volLevel = rnd_n(INV_TYPE_VOLTAGE_NUM);
        Uint16 lo = invTypeLimitTable[volLevel].lower;
        Uint16 hi = invTypeLimitTable[volLevel].upper;
        Uint16 i;

        zero_in();
        fill_pool();

        /* 全局量: 随机但受限。 */
        g_in.inv.volLevel = volLevel;
        g_in.inv.type = (Uint16)(lo + rnd_n((Uint16)(hi - lo + 1)));
        g_in.inv.ratingVoltage = (Uint16)(rnd() % 1300);
        g_in.inv.pointLimit = rnd();
        g_in.inv.bitAccDecStart = (Uint16)(rnd() % 40);
        g_in.tbBase = rnd();
        g_in.upDownFrq = (int32)(((Uint32)rnd() << 16) | rnd());
        g_in.plcStep = rnd();
        g_in.plcTime = (((Uint32)rnd() << 16) | rnd());
        g_in.pmsmRotorPos = rnd();
        g_in.saveEepromIndex = (Uint16)((rnd() % 3) ? 0 : rnd());
        g_in.runDirPanelOld = rnd();
        g_in.frq.all = rnd();
        g_in.cmd.all = rnd();
        g_in.fc.code.saveUserParaFlag1 = rnd();
        g_in.fc.code.saveUserParaFlag2 = rnd();
        g_in.fc.code.motorParaM1.elem.ratingPower = rnd();
        g_in.fc.code.motorFcM2.motorPara.elem.ratingPower = rnd();
        g_in.fc.code.motorFcM3.motorPara.elem.ratingPower = rnd();
        g_in.fc.code.motorFcM4.motorPara.elem.ratingPower = rnd();
        for (i = 0; i < FCD_F2M_LEN; i++)
        {
            g_in.f2m[i] = (int16)rnd();
        }
        /* 参数区随机: 例外区间与互限都读它。 */
        for (i = 0; i < FNUM_PARA; i++)
        {
            g_in.fc.all[i] = rnd();
        }

        g_in.limNum = rnd_n((Uint16)(LIM_N_MAX + 1));
        for (i = 0; i < g_in.limNum; i++)
        {
            g_in.lim[i] = rnd_n(FNUM_PARA);
        }

        switch (which)
        {
        case 0:
            set_qnum(rnd_n(FCD_WRITE_QUEUE_LEN), 0);
            set_kind(K_INIT, rnd_n(FNUM_ALL), rnd_n(2));
            break;
        case 1:
            set_kind(K_ORIG, rnd(), 0);
            break;
        case 2:
            set_qnum(rnd_n(FCD_WRITE_QUEUE_LEN), 6);
            set_kind(K_CLEAR, 0, 0);
            break;
        case 3:
            set_qnum(rnd_n(FCD_WRITE_QUEUE_LEN), 5);
            set_kind(K_POWEROFF, 0, 0);
            break;
        case 4:
            set_qnum(rnd_n(FCD_WRITE_QUEUE_LEN), (Uint16)(2 + 4 * g_in.limNum));
            set_kind(K_RESTORE, 0, 0);
            break;
        default:
            set_qnum(rnd_n(FCD_WRITE_QUEUE_LEN), (Uint16)(4 * g_in.limNum));
            set_kind(K_LIMIT, rnd(), 0);
            break;
        }
        run_case();
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
    fill_limit_table(ref_limitOtherCodeIndex);
    fill_limit_table(new_limitOtherCodeIndex);
    check_limit_table();
    check_invariants();

    base = g_cases;
    phase_orig();
    printf("  GetFuncCodeInitOriginal      : %ld\n", g_cases - base);

    base = g_cases;
    phase_init_sweep();
    printf("  GetFuncCodeInit 全下标扫描   : %ld\n", g_cases - base);

    base = g_cases;
    phase_inv_type();
    printf("  GetFuncCodeInit 逆变器相关   : %ld\n", g_cases - base);

    base = g_cases;
    phase_accdec();
    printf("  GetFuncCodeInit 加减速时间   : %ld\n", g_cases - base);

    base = g_cases;
    phase_clear();
    printf("  ClearRecordDeal              : %ld\n", g_cases - base);

    base = g_cases;
    phase_poweroff();
    printf("  PowerOffRemDeal              : %ld\n", g_cases - base);

    base = g_cases;
    phase_restore();
    printf("  RestoreCompanyParaOtherDeal  : %ld\n", g_cases - base);

    base = g_cases;
    phase_limit_single();
    printf("  LimitOtherCodeDeal 单条目    : %ld\n", g_cases - base);

    base = g_cases;
    phase_limit_multi();
    printf("  LimitOtherCodeDeal 多条目    : %ld\n", g_cases - base);

    base = g_cases;
    phase_limit_chain();
    printf("  LimitOtherCodeDeal 两遍构造  : %ld\n", g_cases - base);

    base = g_cases;
    phase_random(nrand);
    printf("  随机混合                     : %ld\n", g_cases - base);

    printf("f_fcDeal difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    return g_fails ? 1 : 0;
}
