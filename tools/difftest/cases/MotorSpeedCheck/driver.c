/* Differential-test driver for MotorSpeedCheck.
 *
 * The frozen baseline (cases/MotorSpeedCheck/ref/MotorSpeedCheck_ref.c) and the
 * delivered source (src/motor/MotorSpeedCheck.c) are compiled into two symbol
 * namespaces, ref_* and new_*. Both are handed identical inputs and every
 * observable is compared after every call.
 *
 * ---------------------------------------------------------------------------
 * Why this one is sequence-driven, not per-function
 * ---------------------------------------------------------------------------
 * RunCaseSpeedCheck is a state machine. gFeisu carries CycleCnt, UdcRiseCnt,
 * UdcOvCnt, GuoduCnt, VoltCNT, AlmostCNT, CsrWtOver across calls, and which
 * branch runs on call N depends on what calls 1..N-1 left behind. Feeding each
 * function one isolated tuple would therefore only ever reach the entry
 * branches. So the driver runs lockstep sequences: ref and new are stepped one
 * call at a time and compared after *every* call, which also catches a
 * divergence at the exact step it appears instead of 300 steps later.
 *
 * ---------------------------------------------------------------------------
 * Turning constants into inputs
 * ---------------------------------------------------------------------------
 * Several branches are gated on a flag that the module itself sets deep inside
 * a long search, which a bounded sequence may never reach. Those flags are
 * *inputs* to the function under test -- the module only reads them at the top
 * of the step -- so the driver pokes them directly and the branch is entered
 * deterministically:
 *
 *     Case4Sig1.bit.CHECK_GET    -> the "speed found, equalise voltage" branch
 *     Case4Sig2.bit.DelayOver    -> the VoltCNT > 100 branch
 *     Case4Sig2.bit.AlmostOver   -> the AlmostCNT > 1000 branch
 *     Case4Sig1.bit.UdcHigh      -> the "bus too high" branch
 *     Case4Sig2.bit.TwoCyclesOver-> the two-cycle completion test
 *     GuoduCnt / VoltCNT / AlmostCNT / UdcOvCnt -> boundary values around the
 *                                   constants they are compared against
 *                                   (>400, >100, >1000, >1, >=1000)
 *
 * Poking a flag is not the same as making the branch reachable: the *other*
 * input the branch reads (VoltCheck, VoltCheckAim, SpeedCheck, ...) is still
 * enumerated, so the branch is genuinely exercised rather than short-circuited.
 *
 * ---------------------------------------------------------------------------
 * dirty_* discipline
 * ---------------------------------------------------------------------------
 * The comparison only sees what the NEW side leaves behind. A mutation that
 * makes new *stop writing* a field is invisible if the pre-existing value
 * already equals what correct code writes. So every private and shared word is
 * pre-filled with a non-zero, input-dependent pattern (dirty_*()) before each
 * case instead of being left at zero.
 *
 * ---------------------------------------------------------------------------
 * Stub fidelity
 * ---------------------------------------------------------------------------
 * CalOutVotInVFStatus / CalTorqueUp / PID are stubs (see mock_speed_check.c).
 * They are deterministic and depend on their arguments, which is all the
 * comparison needs -- both sides call the same stub, so the only thing that can
 * differ is *what the module passes in* and *what it does with the result*.
 * The stubs are NOT the real arithmetic; see the ledger entry for that caveat.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MotorSpeedCheceInclude.h"

#define VF_LOG_LEN 32

/* ---- renamed entry points ------------------------------------------------ */

extern void ref_PrepareForSpeedCheck(void);
extern void ref_RunCaseSpeedCheck(void);
extern void ref_SpeedCheckCycleOver(void);
extern void ref_SpeedCheckCycleSUB2(void);
extern void ref_SpeedCheckSUB1(void);
extern void ref_SpeedCheckUdcRiseDeal(void);

extern void new_PrepareForSpeedCheck(void);
extern void new_RunCaseSpeedCheck(void);
extern void new_SpeedCheckCycleOver(void);
extern void new_SpeedCheckCycleSUB2(void);
extern void new_SpeedCheckSUB1(void);
extern void new_SpeedCheckUdcRiseDeal(void);

/* ---- private per-side state ---------------------------------------------- */

extern FEISU_STRUCT     ref_gFeisu;
extern FEISU_STRUCT_NEW ref_gFeisuNew;
extern PID_STRUCT       ref_gSpeedCheckPID;
extern PID_STRUCT       ref_gSpeedCheckPID1;

extern FEISU_STRUCT     new_gFeisu;
extern FEISU_STRUCT_NEW new_gFeisuNew;
extern PID_STRUCT       new_gSpeedCheckPID;
extern PID_STRUCT       new_gSpeedCheckPID1;

/* ---- mock-owned ---------------------------------------------------------- */

extern volatile Uint16 gHostCpuCtl[7];
extern int         gCalOutVfLog[VF_LOG_LEN];
extern Uint16      gCalOutVfCount;
extern Uint16      gCalTorqueUpCount;
extern Uint16      gPidCount;
extern PID_STRUCT  gPidLog[VF_LOG_LEN];

/* Set to (value << 16) for one step to pin gFeisu.VoltCheck to `value`
 * (see the comment on gPidOutForce in mock_speed_check.c). */
extern long        gPidOutForce;

/* ---- counters ------------------------------------------------------------ */

static long g_cases;
static long g_mism;
static long g_mismShown;

/* ------------------------------------------------------------------------- */
/* shared state (owned by other modules, read and written by the module)      */
/* ------------------------------------------------------------------------- */

struct SHARED {
    BASE_COMMAND_STRUCT                mainCmd;
    RUN_STATUS_STRUCT                  mainStatus;
    OUT_VOLT_STRUCT                    outVolt;
    LINE_CURRENT_STRUCT                lineCur;
    struct FAULT_CODE_INFOR_STRUCT_DEF error;
    COM_PAR_INFO_STRUCT                comPar;
    UDC_STRUCT                         udc;
    INV_STRUCT                         invInfo;
    MOTOR_EXTERN_STRUCT                motorExtPer;
    BASE_PAR_STRUCT                    basePar;
    MT_STRUCT_Q24                      imtSetApply;
    CONTROL_MOTOR_TYPE_ENUM            ctrMotorType;
    MOTOR_STRUCT                       motorInfo;
    struct GPIO_DATA_REGS              gpio;
    Uint16                             cpu[7];
};

static struct SHARED sRef;
static struct SHARED sNew;

static void shared_save(struct SHARED *s)
{
    memset(s, 0, sizeof(*s));
    s->mainCmd      = gMainCmd;
    s->mainStatus   = gMainStatus;
    s->outVolt      = gOutVolt;
    s->lineCur      = gLineCur;
    s->error        = gError;
    s->comPar       = gComPar;
    s->udc          = gUDC;
    s->invInfo      = gInvInfo;
    s->motorExtPer  = gMotorExtPer;
    s->basePar      = gBasePar;
    s->imtSetApply  = gIMTSetApply;
    s->ctrMotorType = gCtrMotorType;
    s->motorInfo    = gMotorInfo;
    memcpy(&s->gpio, (const void *)&GpioDataRegs, sizeof(s->gpio));
    memcpy(s->cpu, (const void *)gHostCpuCtl, sizeof(s->cpu));
}

static void shared_load(const struct SHARED *s)
{
    gMainCmd      = s->mainCmd;
    gMainStatus   = s->mainStatus;
    gOutVolt      = s->outVolt;
    gLineCur      = s->lineCur;
    gError        = s->error;
    gComPar       = s->comPar;
    gUDC          = s->udc;
    gInvInfo      = s->invInfo;
    gMotorExtPer  = s->motorExtPer;
    gBasePar      = s->basePar;
    gIMTSetApply  = s->imtSetApply;
    gCtrMotorType = s->ctrMotorType;
    gMotorInfo    = s->motorInfo;
    memcpy((void *)&GpioDataRegs, &s->gpio, sizeof(s->gpio));
    memcpy((void *)gHostCpuCtl, s->cpu, sizeof(s->cpu));
}

/* ------------------------------------------------------------------------- */
/* private state                                                            */
/* ------------------------------------------------------------------------- */

struct PRIV {
    FEISU_STRUCT     feisu;
    FEISU_STRUCT_NEW feisuNew;
    PID_STRUCT       pid0;
    PID_STRUCT       pid1;
};

static struct PRIV pRef;
static struct PRIV pNew;

static void priv_snap(struct PRIV *p, int isref)
{
    memset(p, 0, sizeof(*p));
    if (isref)
    {
        p->feisu    = ref_gFeisu;
        p->feisuNew = ref_gFeisuNew;
        p->pid0     = ref_gSpeedCheckPID;
        p->pid1     = ref_gSpeedCheckPID1;
    }
    else
    {
        p->feisu    = new_gFeisu;
        p->feisuNew = new_gFeisuNew;
        p->pid0     = new_gSpeedCheckPID;
        p->pid1     = new_gSpeedCheckPID1;
    }
}

/* ------------------------------------------------------------------------- */
/* call log                                                                 */
/* ------------------------------------------------------------------------- */

struct LOGS {
    int        vf[VF_LOG_LEN];
    Uint16     vfN;
    Uint16     torqueN;
    PID_STRUCT pidl[VF_LOG_LEN];
    Uint16     pidN;
};

static struct LOGS lRef;
static struct LOGS lNew;

static void logs_reset(void)
{
    memset(gCalOutVfLog, 0, sizeof(gCalOutVfLog));
    gCalOutVfCount = 0;
    gCalTorqueUpCount = 0;
    gPidCount = 0;
    memset(gPidLog, 0, sizeof(gPidLog));
}

static void logs_snap(struct LOGS *l)
{
    memset(l, 0, sizeof(*l));
    memcpy(l->vf, gCalOutVfLog, sizeof(l->vf));
    l->vfN = gCalOutVfCount;
    l->torqueN = gCalTorqueUpCount;
    memcpy(l->pidl, gPidLog, sizeof(l->pidl));
    l->pidN = gPidCount;
}

/* ------------------------------------------------------------------------- */
/* the six entry points                                                     */
/* ------------------------------------------------------------------------- */

enum { F_PREP = 0, F_RUN, F_SUB1, F_UDCRISE, F_CYCLEOVER, F_CYCLESUB2, F_COUNT };

static void call_ref(int fn)
{
    switch (fn)
    {
        case F_PREP:      ref_PrepareForSpeedCheck(); break;
        case F_RUN:       ref_RunCaseSpeedCheck();    break;
        case F_SUB1:      ref_SpeedCheckSUB1();       break;
        case F_UDCRISE:   ref_SpeedCheckUdcRiseDeal();break;
        case F_CYCLEOVER: ref_SpeedCheckCycleOver();  break;
        default:          ref_SpeedCheckCycleSUB2();  break;
    }
}

static void call_new(int fn)
{
    switch (fn)
    {
        case F_PREP:      new_PrepareForSpeedCheck(); break;
        case F_RUN:       new_RunCaseSpeedCheck();    break;
        case F_SUB1:      new_SpeedCheckSUB1();       break;
        case F_UDCRISE:   new_SpeedCheckUdcRiseDeal();break;
        case F_CYCLEOVER: new_SpeedCheckCycleOver();  break;
        default:          new_SpeedCheckCycleSUB2();  break;
    }
}

/* ------------------------------------------------------------------------- */
/* comparison                                                               */
/* ------------------------------------------------------------------------- */

static void dump_first_diff(const char *tag, const char *what,
                            const void *a, const void *b, size_t n)
{
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    size_t i;

    for (i = 0; i < n; i++)
    {
        if (pa[i] != pb[i])
        {
            printf("        %s: %s byte %u  ref=0x%02X new=0x%02X\n",
                   tag, what, (unsigned)i, pa[i], pb[i]);
            return;
        }
    }
}

#define CMPM(a, b, what)                                                   \
    do {                                                                   \
        if (memcmp(&(a), &(b), sizeof(a)) != 0)                            \
        {                                                                  \
            g_mism++;                                                      \
            if (g_mismShown < 40)                                          \
            {                                                              \
                g_mismShown++;                                             \
                printf("MISMATCH %s: %s\n", tag, what);                    \
                dump_first_diff(tag, what, &(a), &(b), sizeof(a));         \
            }                                                              \
        }                                                                  \
    } while (0)

static void compare(const char *tag)
{
    CMPM(sRef.mainCmd,      sNew.mainCmd,      "gMainCmd");
    CMPM(sRef.mainStatus,   sNew.mainStatus,   "gMainStatus");
    CMPM(sRef.outVolt,      sNew.outVolt,      "gOutVolt");
    CMPM(sRef.lineCur,      sNew.lineCur,      "gLineCur");
    CMPM(sRef.error,        sNew.error,        "gError");
    CMPM(sRef.comPar,       sNew.comPar,       "gComPar");
    CMPM(sRef.udc,          sNew.udc,          "gUDC");
    CMPM(sRef.invInfo,      sNew.invInfo,      "gInvInfo");
    CMPM(sRef.motorExtPer,  sNew.motorExtPer,  "gMotorExtPer");
    CMPM(sRef.basePar,      sNew.basePar,      "gBasePar");
    CMPM(sRef.imtSetApply,  sNew.imtSetApply,  "gIMTSetApply");
    CMPM(sRef.ctrMotorType, sNew.ctrMotorType, "gCtrMotorType");
    CMPM(sRef.motorInfo,    sNew.motorInfo,    "gMotorInfo");
    CMPM(sRef.gpio,         sNew.gpio,         "GpioDataRegs");
    CMPM(sRef.cpu,          sNew.cpu,          "cpuCtl");

    CMPM(pRef.feisu,    pNew.feisu,    "gFeisu");
    CMPM(pRef.feisuNew, pNew.feisuNew, "gFeisuNew");
    CMPM(pRef.pid0,     pNew.pid0,     "gSpeedCheckPID");
    CMPM(pRef.pid1,     pNew.pid1,     "gSpeedCheckPID1");

    CMPM(lRef.vf,      lNew.vf,      "CalOutVotInVFStatus args");
    CMPM(lRef.vfN,     lNew.vfN,     "CalOutVotInVFStatus count");
    CMPM(lRef.torqueN, lNew.torqueN, "CalTorqueUp count");
    CMPM(lRef.pidl,    lNew.pidl,    "PID args");
    CMPM(lRef.pidN,    lNew.pidN,    "PID count");
}

/* ------------------------------------------------------------------------- */
/* one lockstep step                                                        */
/* ------------------------------------------------------------------------- */

static struct SHARED sIn;

/* 覆盖自检: RunCaseSpeedCheck 的两条主干 (基准 104 行 CHECK_GET == 1 提前返回,
 * 与 129 行之后的整段) 必须都走到。
 *
 * 这个计数器是负向对照逼出来的。之前 rnd() 的低位退化 (见 rnd() 的说明), 让
 * 每一次 F_RUN 都以 CHECK_GET == 1 进入, 基准 129~345 行在差分测试里成了死
 * 代码 —— 而 "0 处不一致" 看上去依然完美, 61 条变异全被记成 MISSED。
 * 把可达性变成**会失败的断言**, 比写在注释里可靠得多。 */
static long g_get0 = 0, g_get1 = 0;

/* 第二条覆盖自检: 基准第 191 行 `(gLineCur.CurPer + 1000) < gFeisu.CurDelta` 的
 * 分道点。第 145 行把 CurDelta 无条件钉成 5125 (只有 `#if 0` 那一支才按电流反算),
 * 所以分道点**只可能**是 CurPer == 4124 —— B5 因此扫的是 CurPer 而不是 CurDelta。
 * 这里把两支的落点断言下来: 一旦有人改了第 145 行 (例如把 `#if 0` 打开),
 * 或者 CurDelta 的常量被改动, 断点立刻失败, 而不是让 S64 悄悄退回 MISSED。 */
static long g_ger_ok = 0, g_ger_bad = 0;

static void do_step(int fn, const char *tag)
{
    g_cases++;

    if (fn == F_RUN)
    {
        if (ref_gFeisu.Case4Sig1.bit.CHECK_GET == 0) g_get0++;
        else                                         g_get1++;
    }

    shared_save(&sIn);

    logs_reset();
    call_ref(fn);
    shared_save(&sRef);
    priv_snap(&pRef, 1);
    logs_snap(&lRef);

    shared_load(&sIn);
    logs_reset();
    call_new(fn);
    shared_save(&sNew);
    priv_snap(&pNew, 0);
    logs_snap(&lNew);

    compare(tag);

    /* The PID-output override is a per-step input, never a sticky one: leaving
     * it armed would silently pin gFeisu.VoltCheck in every later scenario. */
    gPidOutForce = -1;
}

/* ------------------------------------------------------------------------- */
/* input helpers                                                            */
/* ------------------------------------------------------------------------- */

static unsigned long g_rng;

/* 32 位 LCG + 一次位混合。
 *
 * 原来写成 `g_rng * 6364136223846793005UL + ...` 再 `>> 33`, 有两个坑, 合起来
 * 让随机相"看着随机、其实不随机":
 *
 *   1. 宿主上 `unsigned long` 是 **4 字节** (实测 sizeof == 4)。移位计数 33 越过
 *      宽度是未定义行为; MSVC 发出 `shr eax, cl`, 而 x86 把 cl 掩到 5 位, 于是
 *      `>> 33` 实际等价于 `>> 1`。取到的正好是 LCG **最低**的那几位。
 *   2. LCG 第 k 位的周期只有 2^(k+1), 所以 `rnd() % 65536` 只在低 16 位里打转。
 *      更糟的是 phase_random 的循环体每次恰好消耗 40 次 rnd() (40 ≡ 0 mod 8),
 *      于是同一个调用点每次都落在同一相位上 —— 拿到的值恒等 mod 4。
 *
 * 后果是实打实的: `Case4Sig1.bit.CHECK_GET` 就是 bit 1, 恒等 mod 4 意味着它在
 * 随机相里**永远是 1**, RunCaseSpeedCheck 每次都从"已找到转速"那一支 (基准第
 * 104~126 行) 提前 return, 基准第 129~345 行在差分测试里成了死代码。负向对照
 * S41~S101 共 61 条变异因此全部 MISSED —— 不是它们语义等价, 是测试台没走到。
 *
 * 混合后低位与高位同等可用; 再取低 31 位, 保证 rnd_int 的 (hi-lo+1) 取模范围。 */
static unsigned long rnd(void)
{
    unsigned long x;

    g_rng = g_rng * 1664525UL + 1013904223UL;
    x = g_rng;
    x ^= x >> 16;
    x *= 0x7FEB352DUL;
    x ^= x >> 15;
    x *= 0x846CA68BUL;
    x ^= x >> 16;
    return x & 0x7FFFFFFFUL;
}

static int rnd_int(int lo, int hi)
{
    if (hi <= lo)
    {
        return lo;
    }
    return lo + (int)(rnd() % (unsigned long)(hi - lo + 1));
}

/* Non-zero, input-dependent pre-fill. See the dirty_* note in the header.
 *
 * Every field of FEISU_STRUCT is written here, not just the handful the
 * systematic phases care about. Two reasons:
 *   - the comparison sees the whole struct, so an untouched field is still an
 *     observable; leaving one at zero would let "the rewrite stopped writing
 *     it" hide behind a matching zero image;
 *   - several fields (LowFreqCNT, UdcDelta, UdcOld, ...) are only ever written
 *     by the module and never read back, so without a non-zero pre-fill a
 *     dropped write would be completely invisible.
 */
static void dirty_private(unsigned long seed)
{
    unsigned long s = seed * 2654435761UL + 1UL;

    /* 0xA5 rather than 0: any byte the module fails to write stays 0xA5 and
     * therefore differs from the side that did write it. */
    memset(&ref_gFeisu, 0xA5, sizeof(ref_gFeisu));
    memset(&new_gFeisu, 0xA5, sizeof(new_gFeisu));
    memset(&ref_gFeisuNew, 0xA5, sizeof(ref_gFeisuNew));
    memset(&new_gFeisuNew, 0xA5, sizeof(new_gFeisuNew));
    memset(&ref_gSpeedCheckPID, 0xA5, sizeof(ref_gSpeedCheckPID));
    memset(&new_gSpeedCheckPID, 0xA5, sizeof(new_gSpeedCheckPID));
    memset(&ref_gSpeedCheckPID1, 0xA5, sizeof(ref_gSpeedCheckPID1));
    memset(&new_gSpeedCheckPID1, 0xA5, sizeof(new_gSpeedCheckPID1));

    ref_gFeisu.Case4Sig1.all = (Uint16)(s & 0xFFFFUL);
    ref_gFeisu.Case4Sig2.all = (Uint16)((s >> 16) & 0xFFFFUL);
    ref_gFeisu.CycleCnt   = (Uint)((s >> 1) % 3UL) * 0x1111U;
    ref_gFeisu.UdcRiseCnt = (Uint)((s >> 2) % 1200UL);
    ref_gFeisu.UdcOvCnt   = (Uint)((s >> 4) % 6UL);
    ref_gFeisu.GuoduCnt   = (Uint)((s >> 6) % 500UL);
    ref_gFeisu.VoltCNT    = (Uint)((s >> 8) % 150UL);
    ref_gFeisu.AlmostCNT  = (Uint)((s >> 10) % 1200UL);
    ref_gFeisu.LowFreqCNT = (Uint)((s >> 12) % 700UL) + 1U;
    ref_gFeisu.UdcBak     = (Uint)((s >> 14) % 4000UL) + 1U;
    ref_gFeisu.CheckMode  = (Uint)((s >> 16) % 3UL);
    ref_gFeisu.SpeedLast  = (int)(s % 40001UL) - 20000;
    ref_gFeisu.Speed5hz   = (Uint)((s >> 18) % 32767UL) + 1U;
    ref_gFeisu.SpeedCheck = (int)((s >> 3) % 40001UL) - 20000;
    ref_gFeisu.SpeedMaxPu = (int)((s >> 20) % 32767UL) + 1;
    ref_gFeisu.VoltCheck  = (Uint)((s >> 5) % 8193UL);
    ref_gFeisu.VoltCheckAim = (Uint)((s >> 22) % 8193UL);
    ref_gFeisu.VoltTemp   = (Uint)((s >> 24) % 8193UL) + 1U;
    ref_gFeisu.CurDelta   = (int)((s >> 26) % 6001UL);
    ref_gFeisu.Ger4A      = (int)((s >> 28) % 1001UL) - 500;
    ref_gFeisu.CsrWtOver  = (int)((s >> 7) % 2UL);
    ref_gFeisu.UdcDelta   = (int)((s >> 9) % 2001UL) - 1000;
    ref_gFeisu.UdcOld     = (Uint)((s >> 11) % 4000UL) + 1U;

    new_gFeisu.Case4Sig1.all = ref_gFeisu.Case4Sig1.all;
    new_gFeisu.Case4Sig2.all = ref_gFeisu.Case4Sig2.all;
    new_gFeisu.CycleCnt   = ref_gFeisu.CycleCnt;
    new_gFeisu.UdcRiseCnt = ref_gFeisu.UdcRiseCnt;
    new_gFeisu.UdcOvCnt   = ref_gFeisu.UdcOvCnt;
    new_gFeisu.GuoduCnt   = ref_gFeisu.GuoduCnt;
    new_gFeisu.VoltCNT    = ref_gFeisu.VoltCNT;
    new_gFeisu.AlmostCNT  = ref_gFeisu.AlmostCNT;
    new_gFeisu.LowFreqCNT = ref_gFeisu.LowFreqCNT;
    new_gFeisu.UdcBak     = ref_gFeisu.UdcBak;
    new_gFeisu.CheckMode  = ref_gFeisu.CheckMode;
    new_gFeisu.SpeedLast  = ref_gFeisu.SpeedLast;
    new_gFeisu.Speed5hz   = ref_gFeisu.Speed5hz;
    new_gFeisu.SpeedCheck = ref_gFeisu.SpeedCheck;
    new_gFeisu.SpeedMaxPu = ref_gFeisu.SpeedMaxPu;
    new_gFeisu.VoltCheck  = ref_gFeisu.VoltCheck;
    new_gFeisu.VoltCheckAim = ref_gFeisu.VoltCheckAim;
    new_gFeisu.VoltTemp   = ref_gFeisu.VoltTemp;
    new_gFeisu.CurDelta   = ref_gFeisu.CurDelta;
    new_gFeisu.Ger4A      = ref_gFeisu.Ger4A;
    new_gFeisu.CsrWtOver  = ref_gFeisu.CsrWtOver;
    new_gFeisu.UdcDelta   = ref_gFeisu.UdcDelta;
    new_gFeisu.UdcOld     = ref_gFeisu.UdcOld;

    ref_gFeisuNew.TimeTotal  = (Uint)(s & 0xFFFFUL) + 1U;
    ref_gFeisuNew.VoltageDec = (Uint)((s >> 5) & 0xFFFFUL) + 1U;
    new_gFeisuNew.TimeTotal  = ref_gFeisuNew.TimeTotal;
    new_gFeisuNew.VoltageDec = ref_gFeisuNew.VoltageDec;

    ref_gSpeedCheckPID.Total = (long)(s & 0xFFFFFFUL) + 1L;
    ref_gSpeedCheckPID.Out   = (long)((s >> 2) & 0xFFFFFFUL) + 1L;
    ref_gSpeedCheckPID.KP    = (int)((s >> 4) % 4097UL) + 1;
    ref_gSpeedCheckPID.KI    = (int)((s >> 6) % 4097UL) + 1;
    ref_gSpeedCheckPID.KD    = (int)((s >> 8) % 4097UL);
    ref_gSpeedCheckPID.Max   = (int)((s >> 10) % 4097UL) + 1;
    ref_gSpeedCheckPID.Min   = (int)((s >> 12) % 4097UL);
    ref_gSpeedCheckPID.Deta  = (int)((s >> 14) % 20001UL) - 10000;
    ref_gSpeedCheckPID.QP    = (Uint)((s >> 16) % 4097UL);
    ref_gSpeedCheckPID.QI    = (Uint)((s >> 18) % 4097UL);
    ref_gSpeedCheckPID.QD    = (Uint)((s >> 20) % 4097UL);
    new_gSpeedCheckPID = ref_gSpeedCheckPID;

    ref_gSpeedCheckPID1.Total = (long)((s >> 7) & 0xFFFFFFUL) + 1L;
    new_gSpeedCheckPID1.Total = ref_gSpeedCheckPID1.Total;
}

/* Pre-fill every shared word the module may read, so "stopped writing" is
 * observable rather than masked by a zero image. */
static void dirty_shared(unsigned long seed)
{
    unsigned long s = seed * 2246822519UL + 7UL;

    memset(&gMainCmd, 0xA5, sizeof(gMainCmd));
    memset(&gMainStatus, 0xA5, sizeof(gMainStatus));
    memset(&gOutVolt, 0xA5, sizeof(gOutVolt));
    memset(&gLineCur, 0xA5, sizeof(gLineCur));
    memset(&gError, 0xA5, sizeof(gError));
    memset(&gComPar, 0xA5, sizeof(gComPar));
    memset(&gUDC, 0xA5, sizeof(gUDC));
    memset(&gInvInfo, 0xA5, sizeof(gInvInfo));
    memset(&gMotorExtPer, 0xA5, sizeof(gMotorExtPer));
    memset(&gBasePar, 0xA5, sizeof(gBasePar));
    memset(&gIMTSetApply, 0xA5, sizeof(gIMTSetApply));
    memset(&gMotorInfo, 0xA5, sizeof(gMotorInfo));
    memset((void *)&GpioDataRegs, 0xA5, sizeof(GpioDataRegs));
    memset((void *)gHostCpuCtl, 0, sizeof(gHostCpuCtl));

    gLineCur.CurPer        = (int)(s % 6000UL);
    gUDC.uDC               = (Uint)(s % 2000UL);
    gUDC.uDCFilter         = (Uint)((s >> 4) % 2000UL);
    gOutVolt.detVfVoltUp   = (int16)(s % 501UL);
    gOutVolt.Volt          = (int)((s >> 8) % 4097UL);
    gInvInfo.InvTypeApply  = (Uint)((s >> 2) % 21UL);
    gInvInfo.InvLowUDC     = (Uint)((s >> 6) % 1000UL);
    gMotorExtPer.R1        = (Uint)((s >> 9) % 10000UL);
    gBasePar.FullFreq      = (Uint)((s >> 11) % 400UL) + 1U;
    gBasePar.FullFreq01    = (Ulong)((s >> 13) % 400UL) + 1UL;
    gBasePar.MaxFreq       = (Uint)((s >> 15) % 400UL);
    gComPar.SpdSearchTimeSet = (Uint)((s >> 17) % 200UL);
    gComPar.SpdSearchTime    = (Uint)((s >> 19) % 200UL);
    gComPar.SpdSearchMethod  = (Uint)((s >> 21) % 3UL);
    gMainCmd.FreqReal      = (int)((s >> 23) % 4001UL);
    gMainCmd.FreqDesired   = (int)((s >> 25) % 4001UL) - 2000;
    gMainCmd.FreqSetBak    = (int)((s >> 27) % 4001UL) - 2000;
}

/* ------------------------------------------------------------------------- */
/* phases                                                                   */
/* ------------------------------------------------------------------------- */

static void phase_prepare(void)
{
    static const Uint modes[] = { 0U, 1U, 2U, 3U, 0xFFFFU };
    static const int  lasts[] = { -30000, -100, -1, 0, 1, 100, 30000 };
    static const Uint freqs[][3] = {
        { 50U,  50U,  50U  },
        { 100U, 100U, 100U },
        { 200U, 200U, 200U },
        { 400U, 100U, 50U  },
    };
    static const Uint udcs[] = { 0U, 500U, 1000U, 65535U };
    unsigned long k = 0;
    unsigned i, j, m, u;

    for (i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
    {
        for (j = 0; j < sizeof(lasts) / sizeof(lasts[0]); j++)
        {
            for (m = 0; m < sizeof(freqs) / sizeof(freqs[0]); m++)
            {
                for (u = 0; u < sizeof(udcs) / sizeof(udcs[0]); u++)
                {
                    k++;
                    dirty_shared(k);
                    dirty_private(k);

                    gComPar.SpdSearchMethod = modes[i];
                    gBasePar.MaxFreq   = freqs[m][0];
                    gBasePar.FullFreq  = freqs[m][1];
                    gBasePar.FullFreq01 = (Ulong)freqs[m][2];
                    gUDC.uDC           = udcs[u];
                    ref_gFeisu.SpeedLast = lasts[j];
                    new_gFeisu.SpeedLast = lasts[j];

                    do_step(F_PREP, "PrepareForSpeedCheck");
                }
            }
        }
    }
}

static void phase_sub1(void)
{
    static const Uint modes[] = { 0U, 1U, 2U, 3U, 20U };
    static const int  baks[]  = { 0, 1, -1, 100, -100, 32767, -32768 };
    static const int  spds[]  = { 0, 1, -1, 1000, -1000, 30000, -30000 };
    unsigned long k = 0;
    unsigned i, j, m;

    for (i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
    {
        for (j = 0; j < sizeof(baks) / sizeof(baks[0]); j++)
        {
            for (m = 0; m < sizeof(spds) / sizeof(spds[0]); m++)
            {
                k++;
                dirty_shared(k);
                dirty_private(k);

                gMainCmd.Command.bit.ControlMode = modes[i];
                gMainCmd.FreqSetBak = baks[j];
                ref_gFeisu.SpeedCheck = spds[m];
                new_gFeisu.SpeedCheck = spds[m];
                ref_gFeisu.VoltCheck  = (Uint)(k % 8193UL);
                new_gFeisu.VoltCheck  = ref_gFeisu.VoltCheck;

                do_step(F_SUB1, "SpeedCheckSUB1");
            }
        }
    }
}

static void phase_udc_rise(void)
{
    static const Uint sigs[] = { 0U, 0x2000U, 0x4000U, 0x8000U, 0xC000U,
                                 0x2001U, 0x6000U, 0xA000U };
    static const int  spds[] = { -30000, -1000, -1, 0, 1, 1000, 30000 };
    static const int  lasts[] = { -20000, -1, 0, 1, 20000 };
    static const int  gers[] = { 0, 1, -1, 400, -400, 32767, -32768 };
    unsigned long k = 0;
    unsigned i, j, m, g;

    for (i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++)
    {
        for (j = 0; j < sizeof(spds) / sizeof(spds[0]); j++)
        {
            for (m = 0; m < sizeof(lasts) / sizeof(lasts[0]); m++)
            {
                for (g = 0; g < sizeof(gers) / sizeof(gers[0]); g++)
                {
                    k++;
                    dirty_shared(k);
                    dirty_private(k);

                    ref_gFeisu.Case4Sig1.all = sigs[i];
                    new_gFeisu.Case4Sig1.all = sigs[i];
                    ref_gFeisu.SpeedCheck = spds[j];
                    new_gFeisu.SpeedCheck = spds[j];
                    ref_gFeisu.SpeedLast  = lasts[m];
                    new_gFeisu.SpeedLast  = lasts[m];
                    ref_gFeisu.Ger4A      = gers[g];
                    new_gFeisu.Ger4A      = gers[g];
                    ref_gFeisu.SpeedMaxPu = 16384;
                    new_gFeisu.SpeedMaxPu = 16384;

                    do_step(F_UDCRISE, "SpeedCheckUdcRiseDeal");
                }
            }
        }
    }
}

static void phase_cycle_over(void)
{
    static const int des[] = { -30000, -1, 0, 1, 30000 };
    static const Uint s5[] = { 0U, 1U, 100U, 32768U, 16384000U };
    unsigned long k = 0;
    unsigned i, j;

    for (i = 0; i < sizeof(des) / sizeof(des[0]); i++)
    {
        for (j = 0; j < sizeof(s5) / sizeof(s5[0]); j++)
        {
            k++;
            dirty_shared(k);
            dirty_private(k);

            gMainCmd.FreqDesired = des[i];
            ref_gFeisu.Speed5hz = s5[j];
            new_gFeisu.Speed5hz = s5[j];

            do_step(F_CYCLEOVER, "SpeedCheckCycleOver");
        }
    }
}

static void phase_cycle_sub2(void)
{
    unsigned long k;

    for (k = 1; k <= 32; k++)
    {
        dirty_shared(k);
        dirty_private(k);
        ref_gFeisu.VoltCheck = (Uint)(k * 1000UL);
        new_gFeisu.VoltCheck = ref_gFeisu.VoltCheck;
        do_step(F_CYCLESUB2, "SpeedCheckCycleSUB2");
    }
}

/* ------------------------------------------------------------------------- */
/* 边界扫描                                                                  */
/* ------------------------------------------------------------------------- */

/* phase_run / phase_random 都是**等距采样**: 判据差 1 只在恰好落在边界上时才
 * 分得开 —— `GuoduCnt > 400` 与 `> 401` 只在 GuoduCnt 恰好等于 400 时不同,
 * `>=` 与 `>` 只在两侧相等时不同, `<= 350` 与 `< 350` 只在等号处不同。
 * 下面把模块里每一个比较常量都变成"恰好压在边界上"的输入。
 *
 * 每个场景都先 dirty_* 再逐字段覆盖, 于是"模块少写了一个字段"仍然可见。
 *
 * 这一相是负向对照逼出来的: 没有它, S37~S101 共 65 条变异全部 MISSED。
 * 其中 61 条的直接原因是 phase_run 的 CHECK_GET 恒为 1 (见 rnd() 的说明),
 * 另外几条是纯粹的门限边界 —— 两类都不是语义等价, 是测试台没走到。
 *
 * 两条纪律, 各来自一次踩坑:
 *   1. **每个 F_RUN 场景都必须先摆好"能进主流程"的前提** (Start / 无故障 /
 *      控制方式 / 机型)。否则 RunCaseSpeedCheck 第 70~77 行直接 return, 后面
 *      所有门限一个都碰不到 —— 第一版 989 个场景就是这么全被吞掉的。
 *   2. **用 ASYNC_SVC 而不是 ASYNC_VF**。SVC 支路把 VoltCheckAim 钉成常量
 *      4096 (基准第 84 行), "电压偏高/偏低门限"的输入就完全由驱动掌握;
 *      VF 支路会按 R1 / detVfVoltUp / CalOutVotInVFStatus 重算 VoltCheckAim,
 *      门限就不可控了。VF 支路本身由 phase_run 覆盖。 */
static void bound_ready(void)
{
    gMainCmd.Command.bit.Start = 1;
    gMainCmd.Command.bit.ControlMode = IDC_VF_CTL;
    gError.ErrorCode.all = 0UL;
    gCtrMotorType = ASYNC_SVC;
    gComPar.SpdSearchTime = 0;
    gLineCur.CurPer = 0;
}

static void phase_boundary(void)
{
    unsigned long k = 0;
    unsigned i, j, m, n, o, c;
    int  sl;
    Uint s5;

    static const int  offs[]  = { -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5 };
    static const Uint guodu[] = { 399U, 400U, 401U, 402U };
    static const Uint vcnt[]  = { 99U, 100U, 101U };
    static const Uint acnt[]  = { 999U, 1000U, 1001U };
    static const Uint urise[] = { 999U, 1000U, 1001U };
    static const Uint uov[]   = { 0U, 1U, 2U };
    static const int  ubak[]  = { -1, 0, 1, 349, 350, 351, 799, 800, 801 };
    static const int  udel[]  = { -1, 0, 1 };
    static const int  freqr[] = { 999, 1000, 1001 };
    static const Uint masks[] = { 0x0000U, 0x4000U, 0x8000U, 0xC000U };
    static const int  spds[]  = { 0, 1, -1, 16383, 16384, 16385,
                                  -16383, -16384, -16385 };
    static const int  lasts[] = { -1, 0, 1 };
    static const int  curp[]  = { 0, 4123, 4124, 4125, 5125 };
    static const Uint f01[]   = { 50U, 100U, 200U, 400U };
    static const Uint cmode[] = { 0U, 1U, 2U };

    /* --- B1: CHECK_GET == 1 通道 (基准 104~126) ---
     * 电压偏高/偏低门限 4096 +- 3 (S31/S34), 过渡计数门限 400 (S37~S40)。 */
    for (i = 0; i < sizeof(offs) / sizeof(offs[0]); i++)
    {
        for (j = 0; j < sizeof(guodu) / sizeof(guodu[0]); j++)
        {
            k++;
            dirty_shared(k);
            dirty_private(k);
            bound_ready();
            ref_gFeisu.Case4Sig1.all = 0x0002U;      /* CHECK_GET = 1, UdcHigh = 0 */
            new_gFeisu.Case4Sig1.all = 0x0002U;
            ref_gFeisu.VoltCheckAim = 4096U;
            new_gFeisu.VoltCheckAim = 4096U;
            ref_gFeisu.VoltCheck = (Uint)(4096 + offs[i]);
            new_gFeisu.VoltCheck = ref_gFeisu.VoltCheck;
            ref_gFeisu.GuoduCnt = guodu[j];
            new_gFeisu.GuoduCnt = guodu[j];
            do_step(F_RUN, "bound/check-get");
        }
    }

    /* --- B2: CHECK_GET == 0, 延时结束通道 (基准 151~160) --- */
    for (i = 0; i < sizeof(vcnt) / sizeof(vcnt[0]); i++)
    {
        k++;
        dirty_shared(k);
        dirty_private(k);
        bound_ready();
        ref_gFeisu.Case4Sig1.all = 0U;
        new_gFeisu.Case4Sig1.all = 0U;
        ref_gFeisu.Case4Sig2.all = 0x0080U;          /* DelayOver = 1 */
        new_gFeisu.Case4Sig2.all = 0x0080U;
        ref_gFeisu.VoltCNT = vcnt[i];
        new_gFeisu.VoltCNT = vcnt[i];
        do_step(F_RUN, "bound/delay-over");
    }

    /* --- B3: CHECK_GET == 0, 接近结束通道 (基准 162~173) ---
     * AlmostCNT > 1000 (S53), TwoCyclesOver (S54), VoltCheck < VoltCheckAim (S55)。
     *
     * 基准第 167 行读的 VoltCheck 已经被第 149 行
     * (`gFeisu.VoltCheck = (gSpeedCheckPID.Out>>16)`) 覆写过, 直接写 gFeisu
     * 是没用的 —— 必须钉住 PID 输出。这里 4095/4096/4097 恰好把
     * `VoltCheck < VoltCheckAim` 压在两边的等号上。 */
    for (i = 0; i < sizeof(acnt) / sizeof(acnt[0]); i++)
    {
        for (m = 0; m < 2; m++)
        {
            for (n = 0; n < 3; n++)
            {
                k++;
                dirty_shared(k);
                dirty_private(k);
                bound_ready();
                ref_gFeisu.Case4Sig1.all = 0U;
                new_gFeisu.Case4Sig1.all = 0U;
                ref_gFeisu.Case4Sig2.all = 0x0040U;  /* AlmostOver = 1 */
                new_gFeisu.Case4Sig2.all = 0x0040U;
                ref_gFeisu.Case4Sig2.bit.TwoCyclesOver = (Uint16)m;
                new_gFeisu.Case4Sig2.bit.TwoCyclesOver = (Uint16)m;
                ref_gFeisu.AlmostCNT = acnt[i];
                new_gFeisu.AlmostCNT = acnt[i];
                ref_gFeisu.VoltCheckAim = 4096U;
                new_gFeisu.VoltCheckAim = 4096U;
                ref_gFeisu.VoltCheck = (Uint)(4095U + n);
                new_gFeisu.VoltCheck = ref_gFeisu.VoltCheck;
                gPidOutForce = (long)(4095U + n) << 16;
                do_step(F_RUN, "bound/almost-over");
            }
        }
    }

    /* --- B4: m_UData 与 VoltCheckAim 的比较 (基准 176~184) ---
     * 反解出让 m_UData 恰好等于 4096 的 VoltCheck, 再取它两侧 (S61)。
     *
     * m_UData = ((|SpeedCheck|>>8) + 512) * VoltCheck >> 9, 要它 == 4096 就得
     * (a+512)*V 落在 [2097152, 2097663] (= 4096<<9 .. 4097<<9 - 1)。对每个
     * SpeedCheck 取 V = ceil(2097152/(a+512)), 再用 V-1 / V / V+1 —— 上面
     * 九个 SpeedCheck 都恰好给出 4095 / 4096 / 4097。
     *
     * VoltCheck 由 PID 输出决定 (基准 149 行覆写), 所以必须走 gPidOutForce;
     * 直接写 gFeisu.VoltCheck 在这一段是无效的 —— 这正是 S61 一开始 MISSED
     * 的原因。 */
    for (i = 0; i < sizeof(spds) / sizeof(spds[0]); i++)
    {
        unsigned a  = (unsigned)((spds[i] < 0 ? -spds[i] : spds[i]) >> 8);
        unsigned d  = a + 512U;
        unsigned v0 = (2097152U + d - 1U) / d;       /* ceil(4096<<9 / d) */

        if (d * v0 > 2097663U)
        {
            continue;                                /* 这一档压不到边界上 */
        }

        for (n = 0; n < 3; n++)
        {
            Uint v = (Uint)(v0 + n - 1U);

            k++;
            dirty_shared(k);
            dirty_private(k);
            bound_ready();
            ref_gFeisu.Case4Sig1.all = 0U;
            new_gFeisu.Case4Sig1.all = 0U;
            ref_gFeisu.Case4Sig2.all = 0U;
            new_gFeisu.Case4Sig2.all = 0U;
            ref_gFeisu.SpeedCheck = spds[i];
            new_gFeisu.SpeedCheck = spds[i];
            ref_gFeisu.VoltCheck = v;
            new_gFeisu.VoltCheck = v;
            ref_gFeisu.VoltCheckAim = 4096U;
            new_gFeisu.VoltCheckAim = 4096U;
            gPidOutForce = (long)v << 16;
            do_step(F_RUN, "bound/m-udata");
        }
    }

    /* --- B5: 电流环门控 (191~200) 与方向掩码 (202~277) ---
     * CurPer + 1000 与 CurDelta 的边界 (S64), CsrWtOver (S65/S67),
     * all & 0x0C000 的四个取值 (S68/S69), SpdSig (S70/S74/S77),
     * abs(SpeedCheck) 与 SpeedMaxPu 的等号 (S71), CycleCnt (S72),
     * SpeedCheck +- Ger4A 恰好过零 (S78/S79)。
     * VoltCheck = 0 让 m_UData = 0, 保证不在这之前的第 179 行被截住。
     *
     * **要扫的是 CurPer, 不是 CurDelta。** 基准第 145 行 `gFeisu.CurDelta = 5125;`
     * 把驱动喂进去的 CurDelta 无条件覆盖掉 (只有 `#if 0` 的那一支才按电流反算),
     * 于是 CurDelta 在 RunCaseSpeedCheck 里是**只写不读的输入**, 驱动写它纯属
     * 装饰。真正参与比较的量是 `gLineCur.CurPer + 1000 < 5125`, 而变异只把
     * 常数 1000 改成 1001, 所以**分道点只有一个: CurPer == 4124**
     * (1000 + 4124 = 5124 < 5125 为真; 1001 + 4124 = 5125 < 5125 为假)。
     * 这正是 S64 一开始被记成 MISSED 的全部原因 —— 不是语义等价, 是扫错了量。
     * 教训与 MotorInfoCollect 的 `tot[e] - Iu` 同源: 要扫**被测代码真正比较的那个
     * 派生量**, 不是它上游的原始输入。 */
    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 2; j++)
        {
            for (m = 0; m < 2; m++)
            {
                for (n = 0; n < sizeof(spds) / sizeof(spds[0]); n++)
                {
                    for (c = 0; c < sizeof(curp) / sizeof(curp[0]); c++)
                    {
                        for (o = 0; o < 2; o++)
                        {
                            k++;
                            dirty_shared(k);
                            dirty_private(k);
                            bound_ready();
                            ref_gFeisu.Case4Sig1.all =
                                (Uint16)(masks[i] | (j ? 0x2000U : 0U));
                            new_gFeisu.Case4Sig1.all = ref_gFeisu.Case4Sig1.all;
                            ref_gFeisu.Case4Sig2.all = 0U;
                            new_gFeisu.Case4Sig2.all = 0U;
                            ref_gFeisu.VoltCheck = 0U;
                            new_gFeisu.VoltCheck = 0U;
                            gPidOutForce = 0;            /* 让 VoltCheck 真的是 0 */
                            ref_gFeisu.CycleCnt = m ? 0x1111U : 0U;
                            new_gFeisu.CycleCnt = ref_gFeisu.CycleCnt;
                            ref_gFeisu.SpeedMaxPu = 16384;
                            new_gFeisu.SpeedMaxPu = 16384;
                            ref_gFeisu.Speed5hz = 1024U;
                            new_gFeisu.Speed5hz = 1024U;
                            ref_gFeisu.SpeedCheck = spds[n];
                            new_gFeisu.SpeedCheck = spds[n];
                            ref_gFeisu.CsrWtOver = (int)o;
                            new_gFeisu.CsrWtOver = (int)o;
                            ref_gFeisu.CurDelta = 5125;
                            new_gFeisu.CurDelta = 5125;
                            gLineCur.CurPer = (Uint)curp[c];
                            do_step(F_RUN, "bound/mask");

                            if (o == 0 && (curp[c] == 4124 || curp[c] == 4125))
                            {
                                if ((curp[c] == 4124 && ref_gFeisu.CsrWtOver == 0) ||
                                    (curp[c] == 4125 && ref_gFeisu.CsrWtOver == 1))
                                {
                                    g_ger_ok++;
                                }
                                else
                                {
                                    g_ger_bad++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    /* --- B6: UdcHigh == 0 的母线分支 (基准 279~324) ---
     * (uDCFilter - 800) > UdcBak 的边界 (S86), FreqReal > 1000 (S87),
     * UdcDelta > 0 (S90), UdcRiseCnt >= 1000 (S92)。 */
    for (i = 0; i < sizeof(ubak) / sizeof(ubak[0]); i++)
    {
        for (j = 0; j < 2; j++)
        {
            for (m = 0; m < sizeof(freqr) / sizeof(freqr[0]); m++)
            {
                for (n = 0; n < sizeof(udel) / sizeof(udel[0]); n++)
                {
                    for (o = 0; o < sizeof(urise) / sizeof(urise[0]); o++)
                    {
                        k++;
                        dirty_shared(k);
                        dirty_private(k);
                        bound_ready();
                        ref_gFeisu.Case4Sig1.all = 0U;           /* UdcHigh = 0 */
                        new_gFeisu.Case4Sig1.all = 0U;
                        ref_gFeisu.Case4Sig2.all = 0U;
                        new_gFeisu.Case4Sig2.all = 0U;
                        ref_gFeisu.VoltCheck = 0U;
                        new_gFeisu.VoltCheck = 0U;
                        gPidOutForce = 0;                /* 让 VoltCheck 真的是 0 */
                        ref_gFeisu.SpeedCheck = 0;
                        new_gFeisu.SpeedCheck = 0;
                        ref_gFeisu.CheckMode = (Uint)(j + 1U);
                        new_gFeisu.CheckMode = ref_gFeisu.CheckMode;
                        ref_gFeisu.UdcBak = 1000U;
                        new_gFeisu.UdcBak = 1000U;
                        ref_gFeisu.UdcRiseCnt = urise[o];
                        new_gFeisu.UdcRiseCnt = urise[o];
                        gMainCmd.FreqReal = freqr[m];
                        gUDC.uDCFilter = (Uint)(1800 + ubak[i]);
                        gUDC.uDC       = gUDC.uDCFilter;
                        ref_gFeisu.UdcOld = (Uint)((int)gUDC.uDCFilter - udel[n]);
                        new_gFeisu.UdcOld = ref_gFeisu.UdcOld;
                        do_step(F_RUN, "bound/udc");
                    }
                }
            }
        }
    }

    /* --- B7: UdcHigh == 1 的母线分支 (基准 325~345) ---
     * UdcOvCnt > 1 (S98), uDCFilter - UdcBak <= 350 (S100)。 */
    for (i = 0; i < sizeof(uov) / sizeof(uov[0]); i++)
    {
        for (j = 0; j < sizeof(ubak) / sizeof(ubak[0]); j++)
        {
            for (m = 0; m < sizeof(udel) / sizeof(udel[0]); m++)
            {
                k++;
                dirty_shared(k);
                dirty_private(k);
                bound_ready();
                ref_gFeisu.Case4Sig1.all = 0x0001U;          /* UdcHigh = 1 */
                new_gFeisu.Case4Sig1.all = 0x0001U;
                ref_gFeisu.Case4Sig2.all = 0U;
                new_gFeisu.Case4Sig2.all = 0U;
                ref_gFeisu.VoltCheck = 0U;
                new_gFeisu.VoltCheck = 0U;
                gPidOutForce = 0;                /* 让 VoltCheck 真的是 0 */
                ref_gFeisu.SpeedCheck = 0;
                new_gFeisu.SpeedCheck = 0;
                ref_gFeisu.UdcOvCnt = uov[i];
                new_gFeisu.UdcOvCnt = uov[i];
                ref_gFeisu.UdcBak = 1000U;
                new_gFeisu.UdcBak = 1000U;
                gUDC.uDCFilter = (Uint)(1000 + ubak[j]);
                gUDC.uDC       = gUDC.uDCFilter;
                ref_gFeisu.UdcOld = (Uint)((int)gUDC.uDCFilter - udel[m]);
                new_gFeisu.UdcOld = ref_gFeisu.UdcOld;
                do_step(F_RUN, "bound/udc-high");
            }
        }
    }

    /* --- B8: SpeedCheckUdcRiseDeal 的四个钳位判据 (基准 396~417) ---
     * 方向掩码 == 0x4000 与 != 0x4000, SpeedCheck 与 SpeedLast 相等,
     * SpeedCheck 与 +-SpeedMaxPu 相等。
     * 直接调用, 没有前置门。
     *
     * 注意: S119/S121/S123/S124 这四条 (把四个钳位判据的 `>` 改成 `>=`,
     * `<` 改成 `<=`) **不在这条相的目标里, 因为它们处处等价, 已经从
     * negative_control.py 删掉**。理由: `>` 与 `>=` 只在两侧相等时分道,
     * 而此刻分支体干的事恰好是 `SpeedCheck = <被比较的那个值>` —— 也就是
     * 把变量赋成它自己, 一个 no-op。分支链的跳过后继也帮不上忙:
     *   - 掩码 == 0x4000 时, 第 3/4 个分支要求掩码 != 0x4000, 不可能接上;
     *     而第 1/2 个分支互斥于 SpeedLast 的符号, 同样接不上。
     *   - 掩码 != 0x4000 时, 第 3/4 个分支互为"SpeedMaxPu < 0"这一条件,
     *     而不变量 FullFreq = MaxFreq + 20*si2puCoeff > MaxFreq
     *     (MotorParChange.c) 保证 SpeedMaxPu = (MaxFreq<<15)/FullFreq < 32768,
     *     即恒 >= 0, 于是这两个分支永远同时为假。
     * 详细推导见 docs/rewritten_modules.json 里 MotorSpeedCheck 的 invariants。
     *
     * 这一相本身仍然有价值: S118/S120/S122/S125/S126 等改的是判据的**另一侧**
     * (SpeedLast 的符号、赋值目标), 在等号处会真的分道。 */
    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < sizeof(lasts) / sizeof(lasts[0]); j++)
        {
            for (m = 0; m < sizeof(spds) / sizeof(spds[0]); m++)
            {
                k++;
                dirty_shared(k);
                dirty_private(k);
                ref_gFeisu.Case4Sig1.all = masks[i];
                new_gFeisu.Case4Sig1.all = masks[i];
                ref_gFeisu.SpeedLast = lasts[j];
                new_gFeisu.SpeedLast = lasts[j];
                ref_gFeisu.SpeedCheck = spds[m];
                new_gFeisu.SpeedCheck = spds[m];
                ref_gFeisu.SpeedMaxPu = 16384;
                new_gFeisu.SpeedMaxPu = 16384;
                ref_gFeisu.Ger4A = 0;
                new_gFeisu.Ger4A = 0;
                do_step(F_UDCRISE, "bound/udcrise-clamp");
            }
        }
    }

    /* --- B9: PrepareForSpeedCheck 的低速判据 (基准 51) 与 CHECK_BIT14 (58) ---
     * 把 Speed5hz 算出来, 再让 |SpeedLast| 落在它两侧。 */
    for (i = 0; i < sizeof(cmode) / sizeof(cmode[0]); i++)
    {
        for (j = 0; j < sizeof(f01) / sizeof(f01[0]); j++)
        {
            for (m = 0; m < 5; m++)
            {
                k++;
                dirty_shared(k);
                dirty_private(k);
                gComPar.SpdSearchMethod = cmode[i];
                gBasePar.FullFreq   = 100U;
                gBasePar.FullFreq01 = (Ulong)f01[j];
                gBasePar.MaxFreq    = 100U;

                s5 = (Uint)((500L << 15) / (long)f01[j]);
                switch (m)
                {
                    case 0:  sl = (int)s5;        break;
                    case 1:  sl = (int)s5 - 1;    break;
                    case 2:  sl = (int)s5 + 1;    break;
                    case 3:  sl = -(int)s5;       break;
                    default: sl = -((int)s5 - 1); break;
                }
                ref_gFeisu.SpeedLast = sl;
                new_gFeisu.SpeedLast = sl;
                do_step(F_PREP, "bound/prepare");
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* the state-machine sequences                                              */
/* ------------------------------------------------------------------------- */

struct RUNCFG {
    const char *name;
    Uint  ctrType;       /* gCtrMotorType */
    Uint  checkMode;     /* gComPar.SpdSearchMethod */
    int   speedLast;
    Uint  startCmd;
    Ulong errCode;
    Uint  invType;
    int   pokeGet;       /* -1 = 不改; 0/1 = 每步强制 (必须显式给, 见下) */
    int   pokeDelay;
    int   pokeAlmost;
    int   pokeUdcHigh;
    int   pokeTwoCycles;
    Uint  udcMode;       /* 0 flat, 1 rising, 2 falling, 3 big jump */
    int   steps;
};

static void phase_run_cfg(const struct RUNCFG *c)
{
    int step;
    unsigned long k = 0;
    Uint udc = 800U;

    /* Every sequence starts from a clean, identical state on both sides. */
    dirty_shared(1);
    dirty_private(1);

    gCtrMotorType = c->ctrType;
    gComPar.SpdSearchMethod = c->checkMode;
    gInvInfo.InvTypeApply = c->invType;
    gBasePar.FullFreq   = 100U;
    gBasePar.FullFreq01 = 100UL;
    gBasePar.MaxFreq    = 100U;

    ref_gFeisu.SpeedLast = c->speedLast;
    new_gFeisu.SpeedLast = c->speedLast;

    /* The module reads CheckMode (not gComPar.SpdSearchMethod) on this path,
     * and it never calls PrepareForSpeedCheck, so it has to be seeded here or
     * the `CheckMode != 1` tests would run on whatever the dirty_* fill left.
     *
     * CycleCnt starts at 0 so the *first* wrap takes the SpeedCheckCycleSUB2
     * branch; every later wrap takes SpeedCheckCycleOver. Both are therefore
     * reached inside one sequence. Starting it at 0x1111 would make
     * SpeedCheckCycleSUB2 unreachable from this phase. */
    ref_gFeisu.CheckMode = c->checkMode;
    new_gFeisu.CheckMode = c->checkMode;
    ref_gFeisu.CycleCnt  = 0;
    new_gFeisu.CycleCnt  = 0;
    ref_gFeisu.UdcDelta  = 0;
    new_gFeisu.UdcDelta  = 0;

    for (step = 0; step < c->steps; step++)
    {
        k++;

        /* ---- inputs for this step ---- */
        if (c->udcMode == 1U)
        {
            udc = (Uint)(800U + (Uint)step * 3U);
        }
        else if (c->udcMode == 2U)
        {
            udc = (Uint)(3000U - (Uint)step * 4U);
        }
        else if (c->udcMode == 3U)
        {
            udc = (step % 40 == 0) ? (Uint)(5000U + (Uint)step) : 900U;
        }

        gUDC.uDCFilter = udc;
        gUDC.uDC       = udc;

        gMainCmd.Command.bit.Start = c->startCmd;
        gMainCmd.Command.bit.ControlMode = IDC_VF_CTL;
        gError.ErrorCode.all = c->errCode;
        gMainCmd.FreqReal    = (int)((step * 37) % 4000);
        gMainCmd.FreqSetBak  = (step & 1) ? 100 : -100;
        gMainCmd.FreqDesired = (step & 2) ? 500 : -500;
        gLineCur.CurPer      = (int)((step * 13) % 6000);
        gComPar.SpdSearchTime    = (Uint)((step * 7) % 200);
        gComPar.SpdSearchTimeSet = (Uint)((step * 11) % 200);
        gOutVolt.detVfVoltUp = (int16)((step * 5) % 500);
        gMotorExtPer.R1      = (Uint)(1000 + (step * 3) % 8000);
        gInvInfo.InvTypeApply = c->invType;

        /* ---- branch-enabling pokes (see the header) ---- */
        if (c->pokeGet >= 0)
        {
            ref_gFeisu.Case4Sig1.bit.CHECK_GET = (Uint16)c->pokeGet;
            new_gFeisu.Case4Sig1.bit.CHECK_GET = (Uint16)c->pokeGet;
        }
        if (c->pokeDelay >= 0)
        {
            ref_gFeisu.Case4Sig2.bit.DelayOver = (Uint16)c->pokeDelay;
            new_gFeisu.Case4Sig2.bit.DelayOver = (Uint16)c->pokeDelay;
        }
        if (c->pokeAlmost >= 0)
        {
            ref_gFeisu.Case4Sig2.bit.AlmostOver = (Uint16)c->pokeAlmost;
            new_gFeisu.Case4Sig2.bit.AlmostOver = (Uint16)c->pokeAlmost;
        }
        if (c->pokeUdcHigh >= 0)
        {
            ref_gFeisu.Case4Sig1.bit.UdcHigh = (Uint16)c->pokeUdcHigh;
            new_gFeisu.Case4Sig1.bit.UdcHigh = (Uint16)c->pokeUdcHigh;
        }
        if (c->pokeTwoCycles >= 0)
        {
            ref_gFeisu.Case4Sig2.bit.TwoCyclesOver = (Uint16)c->pokeTwoCycles;
            new_gFeisu.Case4Sig2.bit.TwoCyclesOver = (Uint16)c->pokeTwoCycles;
        }

        /* ---- boundary values around the module's own constants ---- */
        /* gFeisu.VoltCheck is not read from this field on the main path -- the
         * module overwrites it at line 149 with (gSpeedCheckPID.Out>>16). Pin
         * the PID output so the value below is what the comparisons actually
         * see; otherwise this line would be decorative. */
        ref_gFeisu.VoltCheck  = (Uint)(c->invType * 100U + (Uint)(step % 9) * 137U);
        new_gFeisu.VoltCheck  = ref_gFeisu.VoltCheck;
        gPidOutForce = (long)ref_gFeisu.VoltCheck << 16;
        ref_gFeisu.VoltCheckAim = (Uint)(500U + (Uint)(step % 7) * 211U);
        new_gFeisu.VoltCheckAim = ref_gFeisu.VoltCheckAim;
        ref_gFeisu.GuoduCnt   = (Uint)(step % 500);
        new_gFeisu.GuoduCnt   = ref_gFeisu.GuoduCnt;
        ref_gFeisu.VoltCNT    = (Uint)(step % 130);
        new_gFeisu.VoltCNT    = ref_gFeisu.VoltCNT;
        ref_gFeisu.AlmostCNT  = (Uint)(step % 1200);
        new_gFeisu.AlmostCNT  = ref_gFeisu.AlmostCNT;
        ref_gFeisu.UdcOvCnt   = (Uint)(step % 5);
        new_gFeisu.UdcOvCnt   = ref_gFeisu.UdcOvCnt;
        ref_gFeisu.UdcRiseCnt = (Uint)(step % 1100);
        new_gFeisu.UdcRiseCnt = ref_gFeisu.UdcRiseCnt;
        ref_gFeisu.SpeedCheck = (int)((step * 97) % 40001UL) - 20000;
        new_gFeisu.SpeedCheck = ref_gFeisu.SpeedCheck;
        ref_gFeisu.SpeedMaxPu = 16384;
        new_gFeisu.SpeedMaxPu = 16384;
        ref_gFeisu.Speed5hz   = 1024U;
        new_gFeisu.Speed5hz   = 1024U;
        ref_gFeisu.CsrWtOver  = (int)(step % 2);
        new_gFeisu.CsrWtOver  = ref_gFeisu.CsrWtOver;
        ref_gFeisu.UdcBak     = 800U;
        new_gFeisu.UdcBak     = 800U;
        ref_gFeisu.UdcOld     = 800U;
        new_gFeisu.UdcOld     = 800U;

        do_step(F_RUN, c->name);
    }
}

static void phase_run(void)
{
    static const struct RUNCFG cfgs[] = {
        /* name                       ctr      mode last    start err  inv  get  dly  alm  hi  two udc st */
        { "run/vf-from-0",            ASYNC_VF,  1,    100,  1U,  0UL,  5U,  0, -1, -1, -1, -1, 1U, 600 },
        { "run/vf-from-max",          ASYNC_VF,  2,  -100,  1U,  0UL,  5U,  0, -1, -1, -1, -1, 2U, 600 },
        { "run/vf-stop-freq",         ASYNC_VF,  0,   3000,  1U,  0UL,  5U,  0, -1, -1, -1, -1, 0U, 600 },
        { "run/vf-big-jump",          ASYNC_VF,  1,    100,  1U,  0UL,  5U,  0, -1, -1, -1, -1, 3U, 600 },
        { "run/svc",                  ASYNC_SVC, 1,    100,  1U,  0UL,  5U,  0, -1, -1, -1, -1, 1U, 400 },
        { "run/stopped",              ASYNC_VF,  1,    100,  0U,  0UL,  5U,  0, -1, -1, -1, -1, 0U, 60  },
        { "run/fault",                ASYNC_VF,  1,    100,  1U,  0x11UL, 5U, 0, -1, -1, -1, -1, 0U, 60  },
        { "run/invtype-13",           ASYNC_VF,  1,    100,  1U,  0UL, 13U,  0, -1, -1, -1, -1, 1U, 400 },
        { "run/invtype-14-mode1",     ASYNC_VF,  1,    100,  1U,  0UL, 14U,  0, -1, -1, -1, -1, 1U, 400 },
        { "run/invtype-14-mode2",     ASYNC_VF,  2,   -100,  1U,  0UL, 14U,  0, -1, -1, -1, -1, 2U, 400 },
        { "run/invtype-20",           ASYNC_VF,  1,    100,  1U,  0UL, 20U,  0, -1, -1, -1, -1, 1U, 400 },
        { "run/get-1",                ASYNC_VF,  1,    100,  1U,  0UL,  5U,  1, -1, -1, -1, -1, 0U, 500 },
        { "run/delay-over",           ASYNC_VF,  1,    100,  1U,  0UL,  5U,  0,  1, -1, -1, -1, 0U, 300 },
        { "run/almost-over",          ASYNC_VF,  1,    100,  1U,  0UL,  5U,  0, -1,  1, -1, -1, 0U, 1200},
        { "run/almost-two-cycles",    ASYNC_VF,  1,    100,  1U,  0UL,  5U,  0, -1,  1, -1,  1, 0U, 1200},
        { "run/udc-high",             ASYNC_VF,  1,    100,  1U,  0UL,  5U,  0, -1, -1,  1, -1, 0U, 300 },
        { "run/udc-high-falling",     ASYNC_VF,  1,    100,  1U,  0UL,  5U,  0, -1, -1,  1, -1, 2U, 300 },
        { "run/get-neg-speedlast",    ASYNC_VF,  1,   -100,  1U,  0UL,  5U,  1, -1, -1, -1, -1, 0U, 500 },
    };
    unsigned i;

    for (i = 0; i < sizeof(cfgs) / sizeof(cfgs[0]); i++)
    {
        phase_run_cfg(&cfgs[i]);
    }
}

static void phase_random(int n)
{
    int i;

    for (i = 0; i < n; i++)
    {
        unsigned long k = (unsigned long)i + 1UL;
        int fn;

        dirty_shared(k * 31UL);
        dirty_private(k * 31UL);

        /* Re-randomise the shared words the dirty_* helper pinned down, so the
         * random phase is not just a re-run of the systematic ones. */
        gComPar.SpdSearchMethod = (Uint)rnd_int(0, 3);
        gBasePar.MaxFreq    = (Uint)rnd_int(0, 400);
        /* Mirror the system invariant gBasePar.FullFreq = MaxFreq + 20*si2puCoeff
         * (MotorParChange.c). It is what keeps
         *     SpeedMaxPu = ((Ulong)MaxFreq << 15) / FullFreq
         * below 32768, i.e. non-negative once stored into the module's int16 --
         * which is exactly the premise the S123/S124 equivalence proof rests on.
         * Feeding FullFreq <= MaxFreq would exercise states the firmware cannot
         * reach, and would make the bench look stronger while proving less. */
        gBasePar.FullFreq   = (Uint)(gBasePar.MaxFreq + (Uint)rnd_int(1, 200));
        gBasePar.FullFreq01 = (Ulong)rnd_int(1, 400);
        gUDC.uDC            = (Uint)rnd_int(0, 4000);
        gUDC.uDCFilter      = (Uint)rnd_int(0, 4000);
        gInvInfo.InvTypeApply = (Uint)rnd_int(0, 21);
        gInvInfo.InvLowUDC    = (Uint)rnd_int(0, 1000);
        gMotorExtPer.R1     = (Uint)rnd_int(0, 20000);
        gLineCur.CurPer     = rnd_int(0, 6000);
        gOutVolt.detVfVoltUp = (int16)rnd_int(0, 600);
        gMainCmd.FreqReal   = rnd_int(-100, 5000);
        gMainCmd.FreqSetBak = rnd_int(-3000, 3000);
        gMainCmd.FreqDesired = rnd_int(-3000, 3000);
        gComPar.SpdSearchTime    = (Uint)rnd_int(0, 300);
        gComPar.SpdSearchTimeSet = (Uint)rnd_int(0, 300);
        gMainCmd.Command.bit.ControlMode = (Uint)rnd_int(0, 20);
        gMainCmd.Command.bit.Start = (Uint)rnd_int(0, 1);
        gError.ErrorCode.all = (rnd() & 1UL) ? 0UL : (Ulong)rnd_int(1, 0xFF);
        gCtrMotorType = (CONTROL_MOTOR_TYPE_ENUM)rnd_int(0, 20);

        ref_gFeisu.Case4Sig1.all = (Uint16)rnd_int(0, 0xFFFF);
        new_gFeisu.Case4Sig1.all = ref_gFeisu.Case4Sig1.all;
        ref_gFeisu.Case4Sig2.all = (Uint16)rnd_int(0, 0xFFFF);
        new_gFeisu.Case4Sig2.all = ref_gFeisu.Case4Sig2.all;
        ref_gFeisu.SpeedLast  = rnd_int(-40000, 40000);
        new_gFeisu.SpeedLast  = ref_gFeisu.SpeedLast;
        ref_gFeisu.SpeedCheck = rnd_int(-40000, 40000);
        new_gFeisu.SpeedCheck = ref_gFeisu.SpeedCheck;
        ref_gFeisu.SpeedMaxPu = rnd_int(0, 32767);
        new_gFeisu.SpeedMaxPu = ref_gFeisu.SpeedMaxPu;
        ref_gFeisu.Speed5hz   = (Uint)rnd_int(0, 32767);
        new_gFeisu.Speed5hz   = ref_gFeisu.Speed5hz;
        ref_gFeisu.VoltCheck  = (Uint)rnd_int(0, 8192);
        new_gFeisu.VoltCheck  = ref_gFeisu.VoltCheck;
        ref_gFeisu.VoltCheckAim = (Uint)rnd_int(0, 8192);
        new_gFeisu.VoltCheckAim = ref_gFeisu.VoltCheckAim;
        ref_gFeisu.GuoduCnt   = (Uint)rnd_int(0, 500);
        new_gFeisu.GuoduCnt   = ref_gFeisu.GuoduCnt;
        ref_gFeisu.VoltCNT    = (Uint)rnd_int(0, 150);
        new_gFeisu.VoltCNT    = ref_gFeisu.VoltCNT;
        ref_gFeisu.AlmostCNT  = (Uint)rnd_int(0, 1200);
        new_gFeisu.AlmostCNT  = ref_gFeisu.AlmostCNT;
        ref_gFeisu.UdcOvCnt   = (Uint)rnd_int(0, 6);
        new_gFeisu.UdcOvCnt   = ref_gFeisu.UdcOvCnt;
        ref_gFeisu.UdcRiseCnt = (Uint)rnd_int(0, 1200);
        new_gFeisu.UdcRiseCnt = ref_gFeisu.UdcRiseCnt;
        ref_gFeisu.Ger4A      = rnd_int(-500, 500);
        new_gFeisu.Ger4A      = ref_gFeisu.Ger4A;
        ref_gFeisu.CycleCnt   = (Uint)rnd_int(0, 2) * 0x1111U;
        new_gFeisu.CycleCnt   = ref_gFeisu.CycleCnt;
        ref_gFeisu.CsrWtOver  = rnd_int(0, 1);
        new_gFeisu.CsrWtOver  = ref_gFeisu.CsrWtOver;
        ref_gFeisu.UdcBak     = (Uint)rnd_int(0, 4000);
        new_gFeisu.UdcBak     = ref_gFeisu.UdcBak;
        ref_gFeisu.UdcOld     = (Uint)rnd_int(0, 4000);
        new_gFeisu.UdcOld     = ref_gFeisu.UdcOld;

        fn = (int)(rnd() % (unsigned long)F_COUNT);

        do_step(fn, "random");
    }
}

/* ------------------------------------------------------------------------- */
/* main                                                                     */
/* ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    int random = (argc > 1) ? atoi(argv[1]) : 20000;

    g_rng = 0x123456789ABCDEFUL;

    /* Layout assertions: the bitfields the module pokes have to land where the
     * driver assumes they do, otherwise a "poke" would silently hit the wrong
     * bit and the branch would never be entered. */
    {
        FEISU_STRUCT probe;

        memset(&probe, 0, sizeof(probe));
        probe.Case4Sig1.bit.CHECK_GET = 1;
        if (probe.Case4Sig1.all != 0x0002U)
        {
            printf("FATAL: Case4Sig1.CHECK_GET is not bit 1 (got 0x%04X)\n",
                   (unsigned)probe.Case4Sig1.all);
            return 2;
        }
        memset(&probe, 0, sizeof(probe));
        probe.Case4Sig1.bit.UdcHigh = 1;
        if (probe.Case4Sig1.all != 0x0001U)
        {
            printf("FATAL: Case4Sig1.UdcHigh is not bit 0 (got 0x%04X)\n",
                   (unsigned)probe.Case4Sig1.all);
            return 2;
        }
        memset(&probe, 0, sizeof(probe));
        probe.Case4Sig1.bit.SpdSig = 1;
        if (probe.Case4Sig1.all != 0x2000U)
        {
            printf("FATAL: Case4Sig1.SpdSig is not bit 13 (got 0x%04X)\n",
                   (unsigned)probe.Case4Sig1.all);
            return 2;
        }
        memset(&probe, 0, sizeof(probe));
        probe.Case4Sig1.bit.CHECK_BIT14 = 1;
        if (probe.Case4Sig1.all != 0x4000U)
        {
            printf("FATAL: Case4Sig1.CHECK_BIT14 is not bit 14 (got 0x%04X)\n",
                   (unsigned)probe.Case4Sig1.all);
            return 2;
        }
        memset(&probe, 0, sizeof(probe));
        probe.Case4Sig1.bit.CHECK_BIT15 = 1;
        if (probe.Case4Sig1.all != 0x8000U)
        {
            printf("FATAL: Case4Sig1.CHECK_BIT15 is not bit 15 (got 0x%04X)\n",
                   (unsigned)probe.Case4Sig1.all);
            return 2;
        }
        memset(&probe, 0, sizeof(probe));
        probe.Case4Sig2.bit.AlmostOver = 1;
        if (probe.Case4Sig2.all != 0x0040U)
        {
            printf("FATAL: Case4Sig2.AlmostOver is not bit 6 (got 0x%04X)\n",
                   (unsigned)probe.Case4Sig2.all);
            return 2;
        }
        memset(&probe, 0, sizeof(probe));
        probe.Case4Sig2.bit.DelayOver = 1;
        if (probe.Case4Sig2.all != 0x0080U)
        {
            printf("FATAL: Case4Sig2.DelayOver is not bit 7 (got 0x%04X)\n",
                   (unsigned)probe.Case4Sig2.all);
            return 2;
        }
        memset(&probe, 0, sizeof(probe));
        probe.Case4Sig2.bit.TwoCyclesOver = 1;
        if (probe.Case4Sig2.all != 0x8000U)
        {
            printf("FATAL: Case4Sig2.TwoCyclesOver is not bit 15 (got 0x%04X)\n",
                   (unsigned)probe.Case4Sig2.all);
            return 2;
        }
    }

    phase_prepare();
    phase_sub1();
    phase_udc_rise();
    phase_cycle_over();
    phase_cycle_sub2();
    phase_boundary();
    phase_run();
    phase_random(random);

    printf("MotorSpeedCheck difftest: cases=%ld mismatches=%ld\n", g_cases, g_mism);
    printf("CHECK_GET 覆盖: ==0 %ld 次, ==1 %ld 次\n", g_get0, g_get1);
    if (g_get0 == 0 || g_get1 == 0)
    {
        printf("FATAL: CHECK_GET 只走到一条路 —— RunCaseSpeedCheck 有一半没被测\n");
        return 2;
    }

    printf("CurPer 门限覆盖: 落点正确 %ld 次, 落点异常 %ld 次\n",
           g_ger_ok, g_ger_bad);
    if (g_ger_ok == 0 || g_ger_bad != 0)
    {
        printf("FATAL: 第 191 行的分道点不在 CurPer == 4124 上 —— 第 145 行的\n");
        printf("       `gFeisu.CurDelta = 5125;` 被改动过, B5 扫的量已经不是\n");
        printf("       被测代码真正比较的那个, S64 会退回 MISSED。\n");
        return 2;
    }

    return g_mism == 0 ? 0 : 1;
}
