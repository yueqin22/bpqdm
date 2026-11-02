/* Differential-test driver for MotorImParEst.
 *
 * 冻结基准 cases/MotorImParEst/ref/MotorImParEst_ref.c 与交付源码
 * src/motor/MotorImParEst.c 编进两个符号命名空间 (ref_* / new_*), 同一份输入
 * 喂给两侧, 每次调用后把**全部**可观测量逐字段比一遍。
 *
 * ---------------------------------------------------------------------------
 * 模块形状
 * ---------------------------------------------------------------------------
 * 15 个入口, 全都没有参数, 全部状态在全局量里:
 *
 *   RsIdentify            IdSubStep 1..7 (case 3 与 case 5 共用一支)
 *   RrLoIdentify          IdSubStep 1..4
 *   SetPwmForRrLoId       纯寄存器写
 *   InitRrLoVariable      纯 gRrLoIdentify 写
 *   EnableTimer           EPwm + EINT + EnableDrive
 *   RrLoInPeriodAndCmpInt 中断体, 直接调
 *   RrWaitAddGetUdc       ADC 握手 + GetUDCInfo
 *   RrGetVoltReBegin      EPwm + DisableDrive
 *   RrLoCalVaule          纯计算, 三个除法
 *   ImCalKpKi             纯计算, 两个除法
 *   LmIoIdentify          IdSubStep 1..5 (case 1 **贯穿**到 case 2)
 *   InitLmIoVaribale      纯 gLmIoIdentify 写
 *   LmIoCalValue          纯计算, 五个除法
 *   LmIoInPeriodInt       中断体, 直接调; 每 200 次进 LmIoCalValue
 *   ChgParForEst          PM / 非 PM 两支, 各 3 段 InvTypeSet
 *
 * ---------------------------------------------------------------------------
 * 状态怎么比
 * ---------------------------------------------------------------------------
 * 本模块**不定义**任何全局量 (nm2000 的未定义符号表里有 23 个), 所以 ref 与 new
 * 两侧共用同一块内存 —— 与 MotorParEst 那种"每侧一份改名副本"的模块不同。做法是
 * 整幅快照:
 *
 *   install(g_in)      把**整幅输入映像**写进所有全局量与外设寄存器
 *   ref 侧调用
 *   capture(&g_a)
 *   install(g_in)      把状态恢复成同一幅输入映像
 *   new 侧调用
 *   capture(&g_b)
 *   compare(g_a, g_b)
 *
 * install 是整结构 memcpy, 所以不存在"某个字段忘了重置"的漏洞; 也正因为如此,
 * **每一个字段都是输入**: 想测"改写后不再写某字段", 只要让输入映像里那个字段
 * 与正确写入值不同就行 (baseline() 把整幅映像先填 0xA5, 再覆盖掉会让除数为 0 的
 * 那些字段)。
 *
 * 唯一不能按原样 memcmp 的是 PieVectTable 的两个中断向量: ref 侧写进去的是
 * &ref_RrLoInPeriodAndCmpInt, new 侧写的是 &new_RrLoInPeriodAndCmpInt, 天然不等。
 * 所以 capture 把它们归一成 (0=空 / 1=本模块 ISR / 2=EPWM1_zero_isr / 3=dummy /
 * 9=其它) 的标签再比, 结构体里那两个字段本身置空。
 *
 * ---------------------------------------------------------------------------
 * 除零: 输入空间的硬约束, 不是风格问题
 * ---------------------------------------------------------------------------
 * 本模块有 11 处除法的除数是变量。除数一旦为 0 不是"报一处 MISMATCH", 而是
 * **进程直接死掉**, 整份台账静默作废。更糟的是 RsIdentify case 6 的
 *
 *     while((m_ULong2>>16) != 0) { m_ULong1 >>= 1; m_ULong2 >>= 1; }
 *
 * —— m_ULong2 是 long, 一旦 (Uint)((TotalI-TotalIL)>>7) * Current 的乘积超过
 * 2^31 赋给 long 就是负数, 算术右移永远到不了 0, **死循环**。所以 baseline() 与
 * pin_*() 不是可选的礼貌, 是能不能跑完的前提:
 *
 *   gMotorInfo.Current        [100, 30000]       case 3/5、case 6 的除数
 *   gInvInfo.InvCurrent       [100, 30000]       case 6、RrLoIdentify case 1
 *   gMotorInfo.Votage         >= 100             ImCalKpKi
 *   gUVCoff.Rs_PRD            >= 200             case 6 的 Rs_PRD/2
 *   gUVCoff.TotalV            >= 256             case 6 的 TotalV>>8
 *   gUVCoff.TotalI > TotalIL  >= 16384           且乘积 < 2^31 (见上)
 *   gRrLoIdentify.PwmCompareValue [100, 2000]    RrLoCalVaule
 *   gRrLoIdentify.IsSampleValue[]  |x| <= 3000   防 m_Value1 溢出 32 位
 *   m_IsZeroVoltDiff != 0                        见 pin_rrlo()
 *   m_Value2 != 0                                见 pin_rrlo()
 *   tempL1 = L0/10 + LM != 0                     ImCalKpKi
 *   m_ComLmb = (lIsAccValue/100)^2 >> 13 != 0    LmIoCalValue
 *   gMainCmd.FreqReal != 0                       LmIoCalValue
 *
 * pin_rrlo() 把 RrLoCalVaule 的除数**照原样算一遍**, 算出 0 就调输入再算, 并记
 * DIV_GUARD; 基准上 DIV_GUARD 必须恒为 0 —— 它不是装饰, 是"我把除数都钉住了"这句
 * 话的证据。
 *
 * ---------------------------------------------------------------------------
 * 覆盖不到的地方
 * ---------------------------------------------------------------------------
 *   - EALLOW/EDIS/EINT/DINT 只是被**记录**, 没有执行; 中断屏蔽语义不在宿主上;
 *   - `int` 已经用 shim 的 `#define int short` 折到 16 位, 但**提升规则**去不掉:
 *     `Uint - Uint` 在目标端是 16 位无符号回绕, 宿主上是 32 位有符号。两侧都是宿主
 *     代码, 所以差分有效, 绝对值不可移植;
 *   - `__IQsat` / `abs` / `qsin` / `qsqrt` 是宿主替身, 见 shim 与 hostlib_mock.c;
 *   - 被调者 (InitSetPWM / InitSetAdc / SetInterruptEnable / GetUDCInfo /
 *     CBCLimitCurPrepare) 是桩, 只能观察"有没有调、按什么顺序调";
 *   - ADC 转换完成标志由 driver 直接摆, 不模拟转换时序。
 */

#include <stdio.h>
#include <string.h>

#include "MotorParaIDinclude.h"

/* 刻意**不** include <stdlib.h>: 它声明 `int abs(int)` (32 位), 而本用例的 shim
 * 把 `int` 折叠成 `short`, 于是 SubPrgInclude.h 的 `extern int abs(int);` 变成
 * `extern short abs(short);` —— 两条声明撞在同一个 TU 里, MSVC 报 C2371
 * "abs: 重定义; 不同的基类型"。MotorPWM / MotorPmsmParEst 的 driver 也是只
 * include <stdio.h> / <string.h>, 原因相同。
 * 代价: 不能用 atol, 参数自己解析 (见 main)。 */

#define FAIL_LIMIT 20
#define LOG_LEN    64
#define SAMPLE_N   14
#define SAMPLE_ALL 20

/* ---- ref 侧 ------------------------------------------------------------- */

extern void ref_RsIdentify(void);
extern void ref_RrLoIdentify(void);
extern void ref_SetPwmForRrLoId(void);
extern void ref_InitRrLoVariable(void);
extern void ref_EnableTimer(void);
extern void ref_RrLoInPeriodAndCmpInt(void);
extern void ref_RrWaitAddGetUdc(void);
extern void ref_RrGetVoltReBegin(void);
extern void ref_RrLoCalVaule(void);
extern void ref_ImCalKpKi(void);
extern void ref_LmIoIdentify(void);
extern void ref_InitLmIoVaribale(void);
extern void ref_LmIoCalValue(void);
extern void ref_LmIoInPeriodInt(void);
extern void ref_ChgParForEst(void);

/* ---- new 侧 ------------------------------------------------------------- */

extern void new_RsIdentify(void);
extern void new_RrLoIdentify(void);
extern void new_SetPwmForRrLoId(void);
extern void new_InitRrLoVariable(void);
extern void new_EnableTimer(void);
extern void new_RrLoInPeriodAndCmpInt(void);
extern void new_RrWaitAddGetUdc(void);
extern void new_RrGetVoltReBegin(void);
extern void new_RrLoCalVaule(void);
extern void new_ImCalKpKi(void);
extern void new_LmIoIdentify(void);
extern void new_InitLmIoVaribale(void);
extern void new_LmIoCalValue(void);
extern void new_LmIoInPeriodInt(void);
extern void new_ChgParForEst(void);

/* ---- mock 提供的仪表 ----------------------------------------------------- */

extern Uint16 gCallLog[LOG_LEN];
extern Uint16 gCallCount;
extern volatile Uint16 gHostCpuCtl[7];
extern Uint gMockUdcFilter;

/* ADC 侧的两个计数。RrWaitAddGetUdc 里 `ADC_CLEAR_INT_FLAG` 与
 * `while(ADC_END_CONVERSIN == 0)` 都是**写一次 / 读一次就没有内存痕迹**的动作:
 *   - ADCINTFLGCLR 在 492 与 508 各写一次, 两次都写同一个位, 于是"少清一次"
 *     在最终寄存器值上看不出来 (两次写的是同一个 1);
 *   - 超时循环的圈数只影响"读了几次完成标志", 一个字段都不改。
 * 这两类变异此前全靠总账记 CAUGHT 混过去 —— 实际是 MISSED。把"清了几次 / 读了
 * 几次"搬进比较镜像, 它们才成为可观测量。 */
extern Uint16 gAdcClrCnt;
extern Uint16 gAdcReadCnt;

/* ---- 状态映像 ----------------------------------------------------------- */

struct IMG
{
    struct EPWM_REGS        epwm1;
    struct EPWM_REGS        epwm2;
    struct EPWM_REGS        epwm3;
    struct ADC_REGS         adc;
    struct ADC_RESULT_REGS  adcres;
    struct PIE_CTRL_REGS    piec;
    struct PIE_VECT_TABLE   piev;
    struct GPIO_DATA_REGS   gpio;
    Uint16                  cpu[7];
    Uint16                  log[LOG_LEN];
    Uint16                  logCount;
    Uint16                  adcClr;
    Uint16                  adcRead;
    int                     vecTag[2];

    UV_BIAS_COFF_STRUCT        uvc;
    IDENTIFY_RRLO_VARIABLE     rrlo;
    IDENTIFY_LMIO_VARIABLE     lmio;
    MOTOR_PARA_EST             par;
    MOTOR_EXTERN_STRUCT        ext;
    PMSM_EST_PARAM_DATA        pmsm;
    PG_DATA_STRUCT             pg;
    VF_INFO_STRUCT             vf;
    BASE_PAR_STRUCT            base;
    CONTROL_MOTOR_TYPE_ENUM    ctrType;
    PWM_OUT_STRUCT             pwm;
    struct FAULT_CODE_INFOR_STRUCT_DEF err;

    ADC_STRUCT                 adcst;
    UDC_STRUCT                 udc;
    UVW_STRUCT                 iuvw;
    AMPTHETA_STRUCT            amp;
    CUR_EXCURSION_STRUCT       exc;
    ANGLE_STRUCT               phase;
    OUT_VOLT_STRUCT            outv;
    FVC_SPEED_STRUCT           fvcs;
    INV_STRUCT                 inv;
    MOTOR_STRUCT               mot;
    BASE_COMMAND_STRUCT        cmd;
};

static struct IMG g_in;
static struct IMG g_a;
static struct IMG g_b;

static long g_cases = 0;
static int  g_fails = 0;

/* ---- 中断向量归一 -------------------------------------------------------- */

static void dummy_isr(void) { }

static int vec_tag(PINT p)
{
    if (p == 0)
    {
        return 0;
    }
    if (p == (PINT)ref_RrLoInPeriodAndCmpInt)
    {
        return 1;
    }
    if (p == (PINT)new_RrLoInPeriodAndCmpInt)
    {
        return 1;           /* 两侧的"本模块 ISR"是同一个逻辑值 */
    }
    if (p == (PINT)EPWM1_zero_isr)
    {
        return 2;
    }
    if (p == (PINT)dummy_isr)
    {
        return 3;
    }
    return 9;
}

/* ---- install / capture --------------------------------------------------- */

#define CPY_IN(dst, src)  memcpy((void *)&(dst), (const void *)&(src), sizeof(dst))
#define CPY_OUT(dst, src) memcpy((void *)&(dst), (const void *)&(src), sizeof(dst))

static void install(const struct IMG *s)
{
    int i;

    CPY_IN(EPwm1Regs,    s->epwm1);
    CPY_IN(EPwm2Regs,    s->epwm2);
    CPY_IN(EPwm3Regs,    s->epwm3);
    CPY_IN(AdcRegs,      s->adc);
    CPY_IN(AdcResult,    s->adcres);
    CPY_IN(PieCtrlRegs,  s->piec);
    CPY_IN(PieVectTable, s->piev);
    CPY_IN(GpioDataRegs, s->gpio);

    /* 这两个向量每次都从 dummy 起步: 它们只能由被测代码写。 */
    PieVectTable.EPWM1_INT = dummy_isr;
    PieVectTable.EPWM2_INT = dummy_isr;

    for (i = 0; i < 7; i++)
    {
        gHostCpuCtl[i] = s->cpu[i];
    }
    memcpy(gCallLog, s->log, sizeof gCallLog);
    gCallCount = s->logCount;
    gAdcClrCnt = s->adcClr;
    gAdcReadCnt = s->adcRead;

    CPY_IN(gUVCoff,        s->uvc);
    CPY_IN(gRrLoIdentify,  s->rrlo);
    CPY_IN(gLmIoIdentify,  s->lmio);
    CPY_IN(gGetParVarable, s->par);
    CPY_IN(gMotorExtReg,   s->ext);
    CPY_IN(gPmParEst,      s->pmsm);
    CPY_IN(gPGData,        s->pg);
    CPY_IN(gVFPar,         s->vf);
    CPY_IN(gBasePar,       s->base);
    gCtrMotorType = s->ctrType;
    CPY_IN(gPWM,           s->pwm);
    CPY_IN(gError,         s->err);

    CPY_IN(gADC,           s->adcst);
    CPY_IN(gUDC,           s->udc);
    CPY_IN(gIUVWQ12,       s->iuvw);
    CPY_IN(gIAmpTheta,     s->amp);
    CPY_IN(gExcursionInfo, s->exc);
    CPY_IN(gPhase,         s->phase);
    CPY_IN(gOutVolt,       s->outv);
    CPY_IN(gFVCSpeed,      s->fvcs);
    CPY_IN(gInvInfo,       s->inv);
    CPY_IN(gMotorInfo,     s->mot);
    CPY_IN(gMainCmd,       s->cmd);
}

static void capture(struct IMG *s)
{
    int i;

    CPY_OUT(s->epwm1, EPwm1Regs);
    CPY_OUT(s->epwm2, EPwm2Regs);
    CPY_OUT(s->epwm3, EPwm3Regs);
    CPY_OUT(s->adc,   AdcRegs);
    CPY_OUT(s->adcres, AdcResult);
    CPY_OUT(s->piec,  PieCtrlRegs);
    CPY_OUT(s->piev,  PieVectTable);
    CPY_OUT(s->gpio,  GpioDataRegs);

    s->vecTag[0] = vec_tag(PieVectTable.EPWM1_INT);
    s->vecTag[1] = vec_tag(PieVectTable.EPWM2_INT);
    s->piev.EPWM1_INT = 0;
    s->piev.EPWM2_INT = 0;

    for (i = 0; i < 7; i++)
    {
        s->cpu[i] = gHostCpuCtl[i];
    }
    memcpy(s->log, gCallLog, sizeof s->log);
    s->logCount = gCallCount;
    s->adcClr = gAdcClrCnt;
    s->adcRead = gAdcReadCnt;

    CPY_OUT(s->uvc,  gUVCoff);
    CPY_OUT(s->rrlo, gRrLoIdentify);
    CPY_OUT(s->lmio, gLmIoIdentify);
    CPY_OUT(s->par,  gGetParVarable);
    CPY_OUT(s->ext,  gMotorExtReg);
    CPY_OUT(s->pmsm, gPmParEst);
    CPY_OUT(s->pg,   gPGData);
    CPY_OUT(s->vf,   gVFPar);
    CPY_OUT(s->base, gBasePar);
    s->ctrType = gCtrMotorType;
    CPY_OUT(s->pwm,  gPWM);
    CPY_OUT(s->err,  gError);

    CPY_OUT(s->adcst, gADC);
    CPY_OUT(s->udc,   gUDC);
    CPY_OUT(s->iuvw,  gIUVWQ12);
    CPY_OUT(s->amp,   gIAmpTheta);
    CPY_OUT(s->exc,   gExcursionInfo);
    CPY_OUT(s->phase, gPhase);
    CPY_OUT(s->outv,  gOutVolt);
    CPY_OUT(s->fvcs,  gFVCSpeed);
    CPY_OUT(s->inv,   gInvInfo);
    CPY_OUT(s->mot,   gMotorInfo);
    CPY_OUT(s->cmd,   gMainCmd);
}

/* ---- 报告与比对 ---------------------------------------------------------- */

static const char *g_fn = "?";

static void report(const char *what)
{
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s: %s\n", g_fn, what);
    printf("  IdSubStep=%u ParEstMstep=%u MotorType=%u InvType=%u InvTypeApply=%u\n",
           (unsigned)g_in.par.IdSubStep, (unsigned)g_in.par.ParEstMstep,
           (unsigned)g_in.mot.MotorType, (unsigned)g_in.inv.InvType,
           (unsigned)g_in.inv.InvTypeApply);
    printf("  Current=%u CurrentGet=%u InvCurrent=%u Votage=%u\n",
           (unsigned)g_in.mot.Current, (unsigned)g_in.mot.CurrentGet,
           (unsigned)g_in.inv.InvCurrent, (unsigned)g_in.mot.Votage);
    printf("  RrWait=%d RrCmp=%u RrSig=%d RrCycle=%d RrMax=%d\n",
           (int)g_in.rrlo.WaitCnt, (unsigned)g_in.rrlo.PwmCompareValue,
           (int)g_in.rrlo.RrL07PulseOverSig, (int)g_in.rrlo.RrL0CycleCnt,
           (int)g_in.rrlo.CurrentMax);
    printf("  LmWait=%d LmDest=%d LmSaved=%d FreqSyn=%d FreqReal=%ld\n",
           (int)g_in.lmio.WaitCnt, (int)g_in.lmio.DestinationFreq,
           (int)g_in.lmio.DataSavedNum, (int)g_in.cmd.FreqSyn,
           (long)g_in.cmd.FreqReal);
    printf("  ref calls=%u new calls=%u\n",
           (unsigned)g_a.logCount, (unsigned)g_b.logCount);
}

static void cmp_m(const char *what, const void *a, const void *b, size_t n)
{
    if (memcmp(a, b, n) != 0)
    {
        report(what);
    }
}

static void compare(void)
{
    cmp_m("EPwm1Regs",   &g_a.epwm1,  &g_b.epwm1,  sizeof g_a.epwm1);
    cmp_m("EPwm2Regs",   &g_a.epwm2,  &g_b.epwm2,  sizeof g_a.epwm2);
    cmp_m("EPwm3Regs",   &g_a.epwm3,  &g_b.epwm3,  sizeof g_a.epwm3);
    cmp_m("AdcRegs",     &g_a.adc,    &g_b.adc,    sizeof g_a.adc);
    cmp_m("AdcResult",   &g_a.adcres, &g_b.adcres, sizeof g_a.adcres);
    cmp_m("PieCtrlRegs", &g_a.piec,   &g_b.piec,   sizeof g_a.piec);
    cmp_m("PieVectTable",&g_a.piev,   &g_b.piev,   sizeof g_a.piev);
    cmp_m("vecTag",      g_a.vecTag,  g_b.vecTag,  sizeof g_a.vecTag);
    cmp_m("GpioDataRegs",&g_a.gpio,   &g_b.gpio,   sizeof g_a.gpio);
    cmp_m("cpuCtl",      g_a.cpu,     g_b.cpu,     sizeof g_a.cpu);
    cmp_m("callLog",     g_a.log,     g_b.log,     sizeof g_a.log);
    cmp_m("callCount",   &g_a.logCount, &g_b.logCount, sizeof g_a.logCount);
    cmp_m("adcClrCnt",   &g_a.adcClr, &g_b.adcClr, sizeof g_a.adcClr);
    cmp_m("adcReadCnt",  &g_a.adcRead, &g_b.adcRead, sizeof g_a.adcRead);

    cmp_m("gUVCoff",        &g_a.uvc,  &g_b.uvc,  sizeof g_a.uvc);
    cmp_m("gRrLoIdentify",  &g_a.rrlo, &g_b.rrlo, sizeof g_a.rrlo);
    cmp_m("gLmIoIdentify",  &g_a.lmio, &g_b.lmio, sizeof g_a.lmio);
    cmp_m("gGetParVarable", &g_a.par,  &g_b.par,  sizeof g_a.par);
    cmp_m("gMotorExtReg",   &g_a.ext,  &g_b.ext,  sizeof g_a.ext);
    cmp_m("gPmParEst",      &g_a.pmsm, &g_b.pmsm, sizeof g_a.pmsm);
    cmp_m("gPGData",        &g_a.pg,   &g_b.pg,   sizeof g_a.pg);
    cmp_m("gVFPar",         &g_a.vf,   &g_b.vf,   sizeof g_a.vf);
    cmp_m("gBasePar",       &g_a.base, &g_b.base, sizeof g_a.base);
    cmp_m("gCtrMotorType",  &g_a.ctrType, &g_b.ctrType, sizeof g_a.ctrType);
    cmp_m("gPWM",           &g_a.pwm,  &g_b.pwm,  sizeof g_a.pwm);
    cmp_m("gError",         &g_a.err,  &g_b.err,  sizeof g_a.err);
    cmp_m("gADC",           &g_a.adcst, &g_b.adcst, sizeof g_a.adcst);
    cmp_m("gUDC",           &g_a.udc,  &g_b.udc,  sizeof g_a.udc);
    cmp_m("gIUVWQ12",       &g_a.iuvw, &g_b.iuvw, sizeof g_a.iuvw);
    cmp_m("gIAmpTheta",     &g_a.amp,  &g_b.amp,  sizeof g_a.amp);
    cmp_m("gExcursionInfo", &g_a.exc,  &g_b.exc,  sizeof g_a.exc);
    cmp_m("gPhase",         &g_a.phase,&g_b.phase,sizeof g_a.phase);
    cmp_m("gOutVolt",       &g_a.outv, &g_b.outv, sizeof g_a.outv);
    cmp_m("gFVCSpeed",      &g_a.fvcs, &g_b.fvcs, sizeof g_a.fvcs);
    cmp_m("gInvInfo",       &g_a.inv,  &g_b.inv,  sizeof g_a.inv);
    cmp_m("gMotorInfo",     &g_a.mot,  &g_b.mot,  sizeof g_a.mot);
    cmp_m("gMainCmd",       &g_a.cmd,  &g_b.cmd,  sizeof g_a.cmd);
}

/* ---- 分支覆盖计数器 ------------------------------------------------------ */

/* 计数**不是**在调用点"声明我打算测哪一支" —— 那是记录意图, 不是记录事实。
 * 每个计数器都从 (输入, ref 侧输出) 这一对**状态差**推出来, 所以"我以为我驱动了
 * 这一支"和"这一支真的走到了"是两件事, 而这里只认后者。
 *
 * 用途很具体: 负向对照里一条变异 MISSED 时, 先看它所在的分支计数器是不是 0。
 * 是 0 = 测试台没覆盖 (要加输入); 不是 0 = 覆盖了但没抓到, 才需要去证明语义等价。
 * 最后 main 会断言 44 个计数器全部非零, 任一为 0 直接判 BENCH-BUG。 */
enum
{
    COV_RS_STEP1 = 0,
    COV_RS_DELAY,
    COV_RS_ARMED,
    COV_RS_C4_ACC,
    COV_RS_C4_DONE,
    COV_RS_C35_UP,
    COV_RS_C35_BUMP,
    COV_RS_C35_LACK,
    COV_RS_C6_ACC,
    COV_RS_C6_FIRST,
    COV_RS_C6_PM,
    COV_RS_C6_IM,
    COV_RS_END,

    COV_RR1_WAIT,
    COV_RR1_START,
    COV_RR2_DOWN,
    COV_RR2_UP50,
    COV_RR2_UP100,
    COV_RR2_STEP,
    COV_RR3_CALC,
    COV_RR3_RETRY,
    COV_RR_END,

    COV_LM2_WAIT,
    COV_LM2_ARM,
    COV_LM3_SHOCK,
    COV_LM3_WAIT,
    COV_LM3_DONE,
    COV_LM4_NEXT,
    COV_LM4_DEC,
    COV_LM5_END,

    COV_ISR_SAMPLE,
    COV_ISR_OVER,
    COV_WAIT_OK,
    COV_WAIT_TIMEOUT,
    COV_CALV_INIT,
    COV_CALV_MINMAX,
    COV_KPKI,
    COV_LMCAL,
    COV_PERIOD_EARLY,
    COV_PERIOD_RUN,
    COV_PERIOD_SAVE,
    COV_CHG_IM,
    COV_CHG_PM10,
    COV_CHG_PM20,
    COV_CHG_PMKEEP,

    COV_COUNT
};

static const char *g_covName[COV_COUNT] =
{
    "RS_STEP1", "RS_DELAY", "RS_ARMED", "RS_C4_ACC", "RS_C4_DONE",
    "RS_C35_UP", "RS_C35_BUMP", "RS_C35_LACK",
    "RS_C6_ACC", "RS_C6_FIRST", "RS_C6_PM", "RS_C6_IM", "RS_END",
    "RR1_WAIT", "RR1_START", "RR2_DOWN", "RR2_UP50", "RR2_UP100",
    "RR2_STEP", "RR3_CALC", "RR3_RETRY", "RR_END",
    "LM2_WAIT", "LM2_ARM", "LM3_SHOCK", "LM3_WAIT", "LM3_DONE",
    "LM4_NEXT", "LM4_DEC", "LM5_END",
    "ISR_SAMPLE", "ISR_OVER", "WAIT_OK", "WAIT_TIMEOUT",
    "CALV_INIT", "CALV_MINMAX", "KPKI", "LMCAL",
    "PERIOD_EARLY", "PERIOD_RUN", "PERIOD_SAVE",
    "CHG_IM", "CHG_PM10", "CHG_PM20", "CHG_PMKEEP"
};

static long g_cov[COV_COUNT];

#define COV(i)  (g_cov[(i)]++)

/* 每个入口一个 kind。用显式枚举而不是从函数名字符串里猜前缀 —— 猜前缀那套在
 * "RrLoIdentify" / "RrLoInPeriodAndCmpInt" / "RrLoCalVaule" / "RrWaitAddGetUdc"
 * 之间必须数到第 5 个字符才分得开, 加一个入口就会静默错配, 而错配的表现是
 * "某个计数器一直是 0", 看着像覆盖不足。 */
enum
{
    K_RS = 0,
    K_RR,
    K_RR_ISR,
    K_RR_WAIT,
    K_RR_CALV,
    K_KPKI,
    K_LM,
    K_LM_CAL,
    K_PERIOD,
    K_CHG,
    K_NONE
};

static int g_kind = K_NONE;

static void cov_observe(void)
{
    Uint16 s = g_in.par.IdSubStep;

    switch (g_kind)
    {
    case K_RS:
        if (s == 1 && g_a.par.IdSubStep == 2)
        {
            COV(COV_RS_STEP1);
        }
        if (s == 2)
        {
            if (g_in.uvc.IdRsDelay != 0)
            {
                if (g_a.uvc.IdRsDelay != g_in.uvc.IdRsDelay)
                {
                    COV(COV_RS_DELAY);
                }
            }
            else if (g_a.par.IdSubStep == 3)
            {
                COV(COV_RS_ARMED);
            }
        }
        if (s == 4)
        {
            if (g_a.uvc.TotalVoltL != g_in.uvc.TotalVoltL)
            {
                COV(COV_RS_C4_ACC);
            }
            if (g_a.par.IdSubStep == 5)
            {
                COV(COV_RS_C4_DONE);
            }
        }
        if (s == 3 || s == 5)
        {
            if (g_a.par.IdSubStep != s)
            {
                COV(COV_RS_C35_UP);
            }
            if (g_a.uvc.Comper != g_in.uvc.Comper)
            {
                COV(COV_RS_C35_BUMP);
            }
            if ((g_a.err.ErrorCode.all & (Uint32)ERROR_OUTPUT_LACK_PHASE)
                != (g_in.err.ErrorCode.all & (Uint32)ERROR_OUTPUT_LACK_PHASE))
            {
                COV(COV_RS_C35_LACK);
            }
        }
        if (s == 6)
        {
            if (g_a.uvc.TotalVolt != g_in.uvc.TotalVolt)
            {
                COV(COV_RS_C6_ACC);
            }
            if (g_in.uvc.IdRsCnt == 0 && g_a.par.IdSubStep == 1)
            {
                COV(COV_RS_C6_FIRST);
            }
            if (g_in.mot.MotorType == MOTOR_TYPE_PM
                && g_a.ext.RsPm != g_in.ext.RsPm)
            {
                COV(COV_RS_C6_PM);
            }
            if (g_in.mot.MotorType != MOTOR_TYPE_PM
                && g_a.ext.R1 != g_in.ext.R1)
            {
                COV(COV_RS_C6_IM);
            }
        }
        if (g_a.par.ParEstMstep != g_in.par.ParEstMstep)
        {
            COV(COV_RS_END);
        }
        break;

    case K_RR:
        if (s == 1)
        {
            if (g_a.par.IdSubStep == 2)
            {
                COV(COV_RR1_START);
            }
            else if (g_a.rrlo.WaitCnt != g_in.rrlo.WaitCnt)
            {
                COV(COV_RR1_WAIT);
            }
        }
        if (s == 2)
        {
            if (g_a.par.IdSubStep == 3)
            {
                COV(COV_RR2_STEP);
            }
            if ((int)g_a.rrlo.PwmCompareValue
                - (int)g_in.rrlo.PwmCompareValue == -20)
            {
                COV(COV_RR2_DOWN);
            }
            if ((int)g_a.rrlo.PwmCompareValue
                - (int)g_in.rrlo.PwmCompareValue == 50)
            {
                COV(COV_RR2_UP50);
            }
            if ((int)g_a.rrlo.PwmCompareValue
                - (int)g_in.rrlo.PwmCompareValue == 100)
            {
                COV(COV_RR2_UP100);
            }
        }
        if (s == 3)
        {
            if (g_a.rrlo.RrL0CycleCnt != g_in.rrlo.RrL0CycleCnt)
            {
                COV(COV_RR3_CALC);
            }
            if (g_a.rrlo.WaitCnt == 20 && g_in.rrlo.WaitCnt == 0)
            {
                COV(COV_RR3_RETRY);
            }
        }
        if (g_a.par.ParEstMstep != g_in.par.ParEstMstep)
        {
            COV(COV_RR_END);
        }
        break;

    case K_LM:
        if (s == 1 || s == 2)
        {
            if (g_a.par.IdSubStep == 3)
            {
                COV(COV_LM2_ARM);
            }
            else if (g_a.lmio.WaitCnt != g_in.lmio.WaitCnt)
            {
                COV(COV_LM2_WAIT);
            }
        }
        if (s == 3)
        {
            if (g_a.lmio.VFOvShock < g_in.lmio.VFOvShock)
            {
                COV(COV_LM3_SHOCK);
            }
            if (g_a.lmio.WaitCnt != g_in.lmio.WaitCnt)
            {
                COV(COV_LM3_WAIT);
            }
            if (g_a.par.IdSubStep == 4)
            {
                COV(COV_LM3_DONE);
            }
        }
        if (s == 4)
        {
            if (g_a.par.IdSubStep == 5)
            {
                COV(COV_LM4_NEXT);
            }
            if (g_a.ext.LM != g_in.ext.LM)
            {
                COV(COV_LM4_DEC);
            }
        }
        if (s == 5 && g_a.par.ParEstMstep != g_in.par.ParEstMstep)
        {
            COV(COV_LM5_END);
        }
        break;

    case K_CHG:
        if (g_a.ctrType == ASYNC_VF)
        {
            COV(COV_CHG_IM);
        }
        else if (g_a.base.FcSet == 10)
        {
            COV(COV_CHG_PM10);
        }
        else if (g_a.base.FcSet == 20)
        {
            COV(COV_CHG_PM20);
        }
        else
        {
            COV(COV_CHG_PMKEEP);
        }
        break;

    case K_RR_ISR:
        if (g_a.rrlo.RrL07PulseOverSig != g_in.rrlo.RrL07PulseOverSig)
        {
            COV(COV_ISR_OVER);
        }
        if (g_a.rrlo.SampleTimes != g_in.rrlo.SampleTimes)
        {
            COV(COV_ISR_SAMPLE);
        }
        break;

    case K_RR_WAIT:
        if ((g_a.err.ErrorCode.all & (Uint32)ERROR_TUNE_FAIL)
            != (g_in.err.ErrorCode.all & (Uint32)ERROR_TUNE_FAIL))
        {
            COV(COV_WAIT_TIMEOUT);
        }
        else
        {
            COV(COV_WAIT_OK);
        }
        break;

    case K_RR_CALV:
        if (g_in.rrlo.RrL0CycleCnt == 0)
        {
            COV(COV_CALV_INIT);
        }
        else
        {
            COV(COV_CALV_MINMAX);
        }
        break;

    case K_KPKI:
        if (g_a.pmsm.IdKp != g_in.pmsm.IdKp)
        {
            COV(COV_KPKI);
        }
        break;

    case K_LM_CAL:
        if (g_a.lmio.WaitCnt != g_in.lmio.WaitCnt)
        {
            COV(COV_LMCAL);
        }
        break;

    case K_PERIOD:
        if (g_a.lmio.DataSavedNum == 0 && g_in.lmio.DataSavedNum != 0)
        {
            COV(COV_PERIOD_SAVE);
        }
        else if (g_a.lmio.DataSavedNum != g_in.lmio.DataSavedNum)
        {
            COV(COV_PERIOD_RUN);
        }
        else
        {
            COV(COV_PERIOD_EARLY);
        }
        break;

    default:
        break;
    }
}

/* ---- 跑一次 -------------------------------------------------------------- */

typedef void (*entry_t)(void);

static void run(entry_t rf, entry_t nf, const char *what, int kind)
{
    g_fn = what;
    g_kind = kind;

    install(&g_in);
    rf();
    capture(&g_a);

    install(&g_in);
    nf();
    capture(&g_b);

    g_cases++;
    cov_observe();
    compare();
}

/* ---- 输入基线 ------------------------------------------------------------ */

/* 整幅映像先填 0xA5, 再覆盖掉"不能是 0 / 不能越界"的那些字段。
 *
 * 0xA5 有两个作用:
 *   1. 让每个**被写**的位置一开始就与正确写入值不同 —— 这是"改写后不再写某字段"
 *      唯一能被看见的理由;
 *   2. 让每个**被读**的位置也是非平凡值, 于是判据不会恰好落在 0 上。
 * 之后那一批显式赋值是**除数约束** (见文件头), 它们把可达集从"全空间"缩到
 * "不会把进程弄死的子空间" —— 缩掉的部分在 README §5 里如实登记。 */
static void baseline(void)
{
    int i;

    memset(&g_in, 0xA5, sizeof g_in);

    /* 外设: 输入阶段一律清零, 让"没写"与"写了 0"可区分地留到比较里。 */
    memset(&g_in.epwm1,  0, sizeof g_in.epwm1);
    memset(&g_in.epwm2,  0, sizeof g_in.epwm2);
    memset(&g_in.epwm3,  0, sizeof g_in.epwm3);
    memset(&g_in.adc,    0, sizeof g_in.adc);
    memset(&g_in.adcres, 0, sizeof g_in.adcres);
    memset(&g_in.piec,   0, sizeof g_in.piec);
    memset(&g_in.piev,   0, sizeof g_in.piev);
    memset(&g_in.gpio,   0, sizeof g_in.gpio);
    memset(g_in.cpu, 0, sizeof g_in.cpu);
    memset(g_in.log, 0, sizeof g_in.log);
    g_in.logCount = 0;
    g_in.adcClr  = 0;
    g_in.adcRead = 0;

    /* ADC: 转换完成标志默认置 1 -> RrWaitAddGetUdc 走快支。
     * ADCRESULT1 限制在 0x0FFF 以内: `ADC_IU` 是 `ADCRESULT1<<4`, 目标端在 16 位
     * 上算, 超过 0x0FFF 就会回绕, 而宿主不会 —— 那是宽度差不是被测代码差, 见
     * README §5。 */
    g_in.adc.ADCINTFLG.bit.ADCINT1 = 1;
    g_in.adcres.ADCRESULT1 = 2048;

    /* 逆变器 / 电机: 除数都在这里钉住 */
    g_in.inv.InvCurrent     = 500;
    g_in.inv.InvType        = 20;
    g_in.inv.InvTypeApply   = 17;
    g_in.inv.InvTypeSet     = 450;
    g_in.inv.InvVoltageType = 0;
    g_in.inv.CurrentCoff    = 1000;

    g_in.mot.MotorType  = MOTOR_TYPE_IM;
    g_in.mot.Current    = 1000;
    g_in.mot.CurrentGet = 500;
    g_in.mot.Votage     = 400;
    g_in.mot.FreqPer    = 50;

    g_in.udc.uDCFilter = 3000;
    gMockUdcFilter = 3000;

    g_in.iuvw.U = 1000;
    g_in.iuvw.V = 1000;
    g_in.iuvw.W = 1000;

    g_in.exc.ErrIu   = 0;
    g_in.exc.IuValue = 500;
    g_in.exc.IvValue = 500;

    g_in.amp.Theta  = 0;
    g_in.phase.IMPhase = 0;
    g_in.outv.Volt  = 3000;

    g_in.cmd.FreqSyn  = 40;
    g_in.cmd.FreqReal = 40000L;

    g_in.fvcs.SpeedEncoder = 1000;

    g_in.adcst.DelayApply = 10;

    /* gUVCoff: RsIdentify 的累加器 + case 6 的除数 */
    g_in.uvc.Rs_PRD      = 1000;
    g_in.uvc.Comper      = 500;
    g_in.uvc.ComperL     = 500;
    g_in.uvc.Number      = 0;
    g_in.uvc.IdRsCnt     = 0;
    g_in.uvc.IdRsDelay   = 0;
    g_in.uvc.IdRsBak     = 0;
    g_in.uvc.TotalU      = 1000;
    g_in.uvc.TotalV      = 1000;      /* >= 256, 否则 TotalV>>8 == 0 */
    g_in.uvc.TotalVoltL  = 1000;
    g_in.uvc.TotalVolt   = 1000;
    g_in.uvc.TotalI      = 100000L;
    g_in.uvc.TotalIL     = 60000L;    /* 差 40000 -> >>7 = 312, 乘积远小于 2^31 */
    g_in.uvc.UDivVSave   = 0;

    /* gRrLoIdentify */
    g_in.rrlo.WaitCnt        = 0;
    g_in.rrlo.CurrentMax     = 0;
    g_in.rrlo.CurrentRatio   = 500;
    g_in.rrlo.RrL07PulseOverSig = 0;
    g_in.rrlo.RrL0CycleCnt   = 0;
    g_in.rrlo.SampleTimes    = 0;
    g_in.rrlo.PwmCompareValue= 500;
    g_in.rrlo.UdcVoltage     = 3000;
    g_in.rrlo.RrAndRsMax     = 0;
    g_in.rrlo.RrAndRsMin     = 0;
    g_in.rrlo.LoMax          = 0;
    g_in.rrlo.LoMin          = 0;
    g_in.rrlo.RrAndRsAccValue= 0;
    g_in.rrlo.LoAccValue     = 0;
    /* 采样窗口: 每对差都在 +-100 内, 第 0 对固定 1000, 于是
     * m_IsZeroVoltDiff 落在 [600,1400], 恒不为 0 (那是 RrLoCalVaule 的除数)。 */
    for (i = 0; i < SAMPLE_ALL; i++)
    {
        g_in.rrlo.IsSampleValue[i] = (i * 37) % 201 - 100;
    }
    g_in.rrlo.IsSampleValue[4] = 0;
    g_in.rrlo.IsSampleValue[5] = 1000;

    /* gLmIoIdentify */
    g_in.lmio.WaitCnt       = 0;
    g_in.lmio.DestinationFreq = 40;
    g_in.lmio.DataSavedNum  = 0;
    g_in.lmio.VFOvShock     = 0;
    g_in.lmio.lIsValue      = 5000;
    g_in.lmio.LmAccValue    = 0;
    g_in.lmio.IoAccValue    = 0;
    g_in.lmio.lImAccValue   = 50000;
    g_in.lmio.lIsAccValue   = 100000;   /* LmIoCalValue 的除数 (见 pin_calc) */

    /* gGetParVarable */
    g_in.par.IdSubStep    = 1;
    g_in.par.ParEstMstep  = 0;
    g_in.par.StatusWord   = TUNE_NULL;
    g_in.par.TuneType     = TUNE_NULL;
    g_in.par.QtEstDelay   = 0;
    for (i = 0; i < IDENTIFY_PROGRESS_LENGTH; i++)
    {
        g_in.par.ParEstContent[i] = IDENTIFY_RS;
    }

    /* gMotorExtReg: tempL1 = L0/10 + LM 是 ImCalKpKi 的除数 */
    g_in.ext.R1   = 50;
    g_in.ext.R2   = 50;
    g_in.ext.L0   = 100;
    g_in.ext.LM   = 100;
    g_in.ext.I0   = 10;
    g_in.ext.RsPm = 50;

    memset(&g_in.pmsm, 0, sizeof g_in.pmsm);
    memset(&g_in.err,  0, sizeof g_in.err);
    memset(&g_in.pg,   0, sizeof g_in.pg);
    memset(&g_in.vf,   0, sizeof g_in.vf);
    memset(&g_in.base, 0, sizeof g_in.base);
    g_in.ctrType = ASYNC_SVC;
    memset(&g_in.pwm,  0, sizeof g_in.pwm);
}

/* ---- 除数钉死 ------------------------------------------------------------ */

/* RrLoCalVaule 的三个除数里有两个是"算出来的", 靠 baseline 的常量保证不了。
 * 这里把模块里那几行照抄一遍, 算出 0 就调输入再算 —— 调的是**输入**, 不是结果。
 *
 * g_divPin 记的是"输入被归一化了几次", 不是"发现了几次异常": 随机相本来就会把
 * 字段撒到全空间, 归一化是**设计的一部分**。真正要盯的是
 *   (a) 进程没死 —— 归一化失败会立刻表现为崩溃, 不是一行 MISMATCH;
 *   (b) pin_rrlo 六次都救不回来时打 BENCH-BUG, 不静默。 */
static long g_divPin = 0;

static long iabs32(long v)
{
    return (v < 0) ? -v : v;
}

static void pin_rrlo(void)
{
    long usum, zsum, udiff, zdiff, zeroTime, den, v1, v2, v3, v4;
    int  i, tries;

    for (i = 0; i < SAMPLE_ALL; i++)
    {
        if (g_in.rrlo.IsSampleValue[i] > 3000)
        {
            g_in.rrlo.IsSampleValue[i] = 3000;
            g_divPin++;
        }
        if (g_in.rrlo.IsSampleValue[i] < -3000)
        {
            g_in.rrlo.IsSampleValue[i] = -3000;
            g_divPin++;
        }
    }
    if ((g_in.rrlo.PwmCompareValue < 100) || (g_in.rrlo.PwmCompareValue > 2000))
    {
        g_in.rrlo.PwmCompareValue = 500;
        g_divPin++;
    }

    for (tries = 0; tries < 6; tries++)
    {
        udiff = 0;
        for (i = 2; i < 7; i++)
        {
            udiff += (long)g_in.rrlo.IsSampleValue[2 * i]
                   - (long)g_in.rrlo.IsSampleValue[2 * i - 1];
        }
        udiff = iabs32(udiff);

        zdiff = 0;
        for (i = 2; i < 7; i++)
        {
            zdiff += (long)g_in.rrlo.IsSampleValue[2 * i + 1]
                   - (long)g_in.rrlo.IsSampleValue[2 * i];
        }
        zdiff = iabs32(zdiff);

        usum = 0;
        for (i = 3; i < 13; i++)
        {
            usum += iabs32(g_in.rrlo.IsSampleValue[i]);
        }
        usum >>= 1;

        zsum = 0;
        for (i = 4; i < 14; i++)
        {
            zsum += iabs32(g_in.rrlo.IsSampleValue[i]);
        }
        zsum >>= 1;

        if (zdiff == 0)
        {
            g_in.rrlo.IsSampleValue[5] = (int)(g_in.rrlo.IsSampleValue[4] + 1000);
            g_divPin++;
            continue;
        }

        zeroTime = 12000L - (long)g_in.rrlo.PwmCompareValue;
        den = zdiff * (long)g_in.rrlo.PwmCompareValue;
        v1 = usum + (long)(((long long)zsum * (long long)udiff
                            * (long long)zeroTime) / (long long)den);

        v2 = (long)(((long long)v1 * (long long)g_in.inv.InvCurrent) >> 14);
        v3 = (long)(((long long)v2 * 6372L) >> 11);
        v4 = v3 * (long)g_in.inv.CurrentCoff / 1000;

        if (v4 != 0)
        {
            return;
        }
        /* m_Value2 为 0 -> 把被乘数抬上去 (改的是输入, 不是中间量)。 */
        for (i = 3; i < 13; i++)
        {
            g_in.rrlo.IsSampleValue[i] = 2000;
        }
        g_in.inv.CurrentCoff = 1000;
        g_divPin++;
    }

    /* 六次都没救回来, 说明 pin 的算术与模块脱节了 —— 记进 BENCH-BUG, 不要静默。 */
    printf("BENCH-BUG pin_rrlo: m_Value2 still 0 after 6 tries\n");
}

/* RsIdentify case 6 的除数: TotalV>>8、Rs_PRD/2、以及那个会死循环的 m_ULong2。 */
static void pin_rs(void)
{
    long diff;

    if (g_in.uvc.TotalV < 256)
    {
        g_in.uvc.TotalV = 1000;
        g_divPin++;
    }
    if (g_in.uvc.Rs_PRD < 200)
    {
        g_in.uvc.Rs_PRD = 1000;
        g_divPin++;
    }
    if (g_in.mot.Current < 100)
    {
        g_in.mot.Current = 1000;
        g_divPin++;
    }
    if (g_in.inv.InvCurrent < 100)
    {
        g_in.inv.InvCurrent = 500;
        g_divPin++;
    }
    if (g_in.uvc.TotalI <= g_in.uvc.TotalIL)
    {
        g_in.uvc.TotalI = g_in.uvc.TotalIL + 65536L;
        g_divPin++;
    }
    diff = (long)(g_in.uvc.TotalI - g_in.uvc.TotalIL);
    if (diff < 16384L)
    {
        g_in.uvc.TotalI = g_in.uvc.TotalIL + 65536L;
        g_divPin++;
    }
    /* m_ULong2 = ((Uint)(diff>>7) * Current) >> 10, 且 while 循环要求它非负:
     * 乘积必须 < 2^31, 否则赋给 long 变成负数 -> 算术右移永远到不了 0 -> 死循环。 */
    {
        long m2 = (long)(Uint)((Ulong)diff >> 7);
        while (m2 * (long)g_in.mot.Current >= 2147483647L)
        {
            g_in.mot.Current >>= 1;
            g_divPin++;
        }
        if (m2 == 0)
        {
            g_in.uvc.TotalI = g_in.uvc.TotalIL + 65536L;
            g_divPin++;
        }
    }
}

/* ImCalKpKi / LmIoCalValue 的除数。 */
static void pin_calc(void)
{
    if ((long)(g_in.ext.L0 / 10) + (long)g_in.ext.LM == 0)
    {
        g_in.ext.LM = 100;
        g_divPin++;
    }
    if (g_in.mot.Votage < 100)
    {
        g_in.mot.Votage = 400;
        g_divPin++;
    }
    if (g_in.cmd.FreqReal == 0)
    {
        g_in.cmd.FreqReal = 40000L;
        g_divPin++;
    }
    /* m_ComLmb = (lIsAccValue/100)^2 >> 13, 需要 |lIsAccValue| >= 100*91 */
    if (iabs32(g_in.lmio.lIsAccValue) < 100000L)
    {
        g_in.lmio.lIsAccValue = (g_in.lmio.lIsAccValue < 0) ? -100000L : 100000L;
        g_divPin++;
    }
}

static void pin_all(void)
{
    pin_rs();
    pin_rrlo();
    pin_calc();
}

/* ---- 相位: ChgParForEst -------------------------------------------------- */

static void phase_chg(void)
{
    static const Uint   types[] = { 0, 2, 3, 100 };
    static const Uint   sets[]  = { 0, 399, 400, 401, 499, 500, 501, 600 };
    static const Uint   fcs[]   = { 0, 29, 30, 31, 65535 };
    unsigned ti, si, fi;

    for (ti = 0; ti < sizeof types / sizeof types[0]; ti++)
    {
        for (si = 0; si < sizeof sets / sizeof sets[0]; si++)
        {
            for (fi = 0; fi < sizeof fcs / sizeof fcs[0]; fi++)
            {
                baseline();
                g_in.mot.MotorType = types[ti];
                g_in.inv.InvTypeSet = sets[si];
                g_in.base.FcSet = fcs[fi];
                /* 非 PM 支会无条件写这五个, PM 支一个都不写 —— 先给非平凡值 */
                g_in.vf.VFWsComp = 0x1111;
                g_in.vf.VFLineType = 0x2222;
                g_in.vf.VFTorqueUp = 0x3333;
                g_in.pwm.SoftPWMTune = 0x4444;
                g_in.ctrType = SYNC_FVC;
                run(ref_ChgParForEst, new_ChgParForEst, "ChgParForEst", K_CHG);
            }
        }
    }
}

/* ---- 相位: RsIdentify ---------------------------------------------------- */

static void phase_rs(void)
{
    static const Uint vtypes[] = { 0, INV_VOLTAGE_690V, INV_VOLTAGE_1140V, 9 };
    static const Uint applies[] = { 0, 16, 17, 20 };
    static const Uint numbers[] = { 0, 511, 512, 513, 1023, 1024, 1025 };
    static const Uint currents[] = { 100, 1000, 30000 };
    static const Uint getvals[] = { 0, 1, 60, 600, 30000 };
    static const int  us[] = { 0, 1, -1, 4000, -4000, 32767, -32768 };
    unsigned vi, ai, ni, ci, gi, ui, s;

    /* case 1 / 2: 电压等级、IdRsDelay */
    for (vi = 0; vi < sizeof vtypes / sizeof vtypes[0]; vi++)
    {
        for (s = 0; s < 3; s++)
        {
            baseline();
            g_in.par.IdSubStep = (Uint16)s;
            g_in.inv.InvVoltageType = vtypes[vi];
            g_in.adcst.DelayApply = (Uint)(10 + vi);
            run(ref_RsIdentify, new_RsIdentify, "RsIdentify", K_RS);
        }
    }
    for (s = 0; s < 3; s++)
    {
        unsigned d;
        for (d = 0; d < 3; d++)
        {
            baseline();
            g_in.par.IdSubStep = 2;
            g_in.uvc.IdRsDelay = (Uint)(d == 0 ? 0 : (d == 1 ? 1 : 1000));
            run(ref_RsIdentify, new_RsIdentify, "RsIdentify", K_RS);
        }
    }

    /* case 3 / case 5: 门限比较 + Comper 爬升 + 缺相 */
    for (s = 3; s <= 5; s += 2)
    {
        for (ai = 0; ai < sizeof applies / sizeof applies[0]; ai++)
        {
            for (ci = 0; ci < sizeof currents / sizeof currents[0]; ci++)
            {
                for (gi = 0; gi < sizeof getvals / sizeof getvals[0]; gi++)
                {
                    for (ui = 0; ui < sizeof us / sizeof us[0]; ui++)
                    {
                        baseline();
                        g_in.par.IdSubStep = (Uint16)s;
                        g_in.inv.InvTypeApply = applies[ai];
                        g_in.mot.Current = currents[ci];
                        g_in.mot.CurrentGet = getvals[gi];
                        g_in.iuvw.U = (int)us[ui];
                        g_in.uvc.Comper = 500;
                        g_in.uvc.Rs_PRD = 1000;
                        pin_all();
                        run(ref_RsIdentify, new_RsIdentify, "RsIdentify", K_RS);
                    }
                }
            }
        }
    }
    /* Comper 爬到 Rs_PRD 之上 -> 缺相告警。
     * 要进 else 支必须 `abs(gIUVWQ12.U) < m_UData1`, 而 m_UData1 是
     * `(1024*CurrentGet)/Current` 再按 m_MaxCur 缩一次 —— 所以 CurrentGet 不能是 0
     * (那样 m_UData1 = 0, 而 abs(0) >= 0 恒真, 走的是 UP 支, 一条 LACK 都出不来)。
     * 第一版就是 CurrentGet=0, RS_C35_LACK 计数器直接报 0, 覆盖门把它拦下了。 */
    for (s = 3; s <= 5; s += 2)
    {
        unsigned k;
        for (k = 0; k < 4; k++)
        {
            baseline();
            g_in.par.IdSubStep = (Uint16)s;
            g_in.uvc.Rs_PRD = 1000;
            g_in.uvc.Comper = (Uint)(k == 0 ? 996 : (k == 1 ? 1000
                               : (k == 2 ? 1200 : 400)));
            g_in.iuvw.U = 0;
            g_in.mot.CurrentGet = 500;
            g_in.mot.Current = 1000;
            /* InvTypeApply 决定每次 +4 还是 +1 */
            g_in.inv.InvTypeApply = (Uint)((k & 1) ? 20 : 10);
            run(ref_RsIdentify, new_RsIdentify, "RsIdentify", K_RS);
        }
    }

    /* case 4: Number 跨 512 / 1024 */
    for (ni = 0; ni < sizeof numbers / sizeof numbers[0]; ni++)
    {
        baseline();
        g_in.par.IdSubStep = 4;
        g_in.uvc.Number = numbers[ni];
        g_in.uvc.Comper = 400;
        g_in.iuvw.U = 1234;
        run(ref_RsIdentify, new_RsIdentify, "RsIdentify", K_RS);
    }

    /* case 6: 累加 + 大计算 */
    for (ni = 0; ni < sizeof numbers / sizeof numbers[0]; ni++)
    {
        unsigned cnt, mt;
        for (cnt = 0; cnt < 2; cnt++)
        {
            for (mt = 0; mt < 2; mt++)
            {
                baseline();
                g_in.par.IdSubStep = 6;
                g_in.uvc.Number = numbers[ni];
                g_in.uvc.IdRsCnt = (Uint)cnt;
                g_in.mot.MotorType = mt ? MOTOR_TYPE_PM : MOTOR_TYPE_IM;
                g_in.iuvw.U = 1500;
                g_in.iuvw.V = 800;
                g_in.uvc.ComperL = 300;
                g_in.uvc.Comper = 700;
                pin_all();
                run(ref_RsIdentify, new_RsIdentify, "RsIdentify", K_RS);
            }
        }
    }
    /* case 6 的 InvType 折算三支 (18<x<23 / x>23 / 其它) */
    {
        static const Uint itypes[] = { 18, 19, 22, 23, 24, 30 };
        unsigned ii;
        for (ii = 0; ii < sizeof itypes / sizeof itypes[0]; ii++)
        {
            baseline();
            g_in.par.IdSubStep = 6;
            g_in.uvc.Number = 1024;
            g_in.uvc.IdRsCnt = 1;
            g_in.inv.InvType = itypes[ii];
            g_in.iuvw.U = 1500;
            g_in.iuvw.V = 800;
            pin_all();
            run(ref_RsIdentify, new_RsIdentify, "RsIdentify", K_RS);
        }
    }
    /* case 6 的 16 位截断: CurrentGet/Current 比值很大 */
    for (ci = 0; ci < sizeof currents / sizeof currents[0]; ci++)
    {
        baseline();
        g_in.par.IdSubStep = 6;
        g_in.uvc.Number = 1024;
        g_in.uvc.IdRsCnt = 1;
        g_in.mot.Current = currents[ci];
        g_in.mot.CurrentGet = 65535;
        g_in.iuvw.U = 1500;
        g_in.iuvw.V = 800;
        pin_all();
        run(ref_RsIdentify, new_RsIdentify, "RsIdentify", K_RS);
    }

    /* case 7 / default / 越界步号 */
    {
        static const Uint steps[] = { 7, 8, 100, 65535 };
        unsigned k;
        for (k = 0; k < sizeof steps / sizeof steps[0]; k++)
        {
            baseline();
            g_in.par.IdSubStep = steps[k];
            g_in.par.ParEstMstep = 3;
            run(ref_RsIdentify, new_RsIdentify, "RsIdentify", K_RS);
        }
    }
}

/* ---- 相位: RrLoIdentify -------------------------------------------------- */

static void phase_rr(void)
{
    static const int waits[] = { 0, 1, 2, 19, 20, 999, 1000, 1001 };
    static const int cmax[]  = { 0, 100, 500, 3201, 3701, 8000, 20000 };
    static const int ratio[] = { 0, 1, 100, 8992, 9000, 32000 };
    static const int rrcnt[] = { 0, 1, 5, 6, 7 };
    static const int stimes[]= { 0, 13, 14, 15 };
    static const int ovsig[] = { 0, 1, 2 };
    unsigned wi, ci, ri, ki, si, oi, s;

    for (s = 1; s <= 4; s++)
    {
        for (wi = 0; wi < sizeof waits / sizeof waits[0]; wi++)
        {
            baseline();
            g_in.par.IdSubStep = (Uint16)s;
            g_in.rrlo.WaitCnt = waits[wi];
            pin_all();
            run(ref_RrLoIdentify, new_RrLoIdentify, "RrLoIdentify", K_RR);
        }
    }

    /* case 1: CurrentRatio 夹到 8992 */
    for (ri = 0; ri < sizeof ratio / sizeof ratio[0]; ri++)
    {
        unsigned cc;
        for (cc = 0; cc < 3; cc++)
        {
            baseline();
            g_in.par.IdSubStep = 1;
            g_in.rrlo.WaitCnt = 1000;
            g_in.mot.Current = (Uint)(cc == 0 ? 100 : (cc == 1 ? 1000 : 30000));
            g_in.inv.InvCurrent = (Uint)(cc == 2 ? 100 : 500);
            g_in.mot.CurrentGet = 30000;
            pin_all();
            run(ref_RrLoIdentify, new_RrLoIdentify, "RrLoIdentify", K_RR);
        }
    }

    /* case 2: CurrentMax / CurrentRatio / RrL07PulseOverSig 的三层判据 */
    for (ci = 0; ci < sizeof cmax / sizeof cmax[0]; ci++)
    {
        for (ri = 0; ri < sizeof ratio / sizeof ratio[0]; ri++)
        {
            for (oi = 0; oi < sizeof ovsig / sizeof ovsig[0]; oi++)
            {
                baseline();
                g_in.par.IdSubStep = 2;
                g_in.rrlo.WaitCnt = 0;
                g_in.rrlo.CurrentMax = cmax[ci];
                g_in.rrlo.CurrentRatio = ratio[ri];
                g_in.rrlo.RrL07PulseOverSig = ovsig[oi];
                pin_all();
                run(ref_RrLoIdentify, new_RrLoIdentify, "RrLoIdentify", K_RR);
            }
        }
    }

    /* case 3: SampleTimes==14 + RrL0CycleCnt 门槛 + InvType 折算 */
    for (si = 0; si < sizeof stimes / sizeof stimes[0]; si++)
    {
        for (ki = 0; ki < sizeof rrcnt / sizeof rrcnt[0]; ki++)
        {
            unsigned it;
            for (it = 0; it < 4; it++)
            {
                baseline();
                g_in.par.IdSubStep = 3;
                g_in.rrlo.WaitCnt = 0;
                g_in.rrlo.RrL07PulseOverSig = 1;
                g_in.rrlo.SampleTimes = stimes[si];
                g_in.rrlo.RrL0CycleCnt = rrcnt[ki];
                g_in.inv.InvType = (Uint)(it == 0 ? 18 : (it == 1 ? 19
                                   : (it == 2 ? 23 : 24)));
                g_in.rrlo.RrAndRsAccValue = 4000;
                g_in.rrlo.RrAndRsMax = 500;
                g_in.rrlo.RrAndRsMin = 400;
                g_in.rrlo.LoAccValue = 2000;
                g_in.rrlo.LoMax = 300;
                g_in.rrlo.LoMin = 200;
                pin_all();
                run(ref_RrLoIdentify, new_RrLoIdentify, "RrLoIdentify", K_RR);
            }
        }
    }
    /* case 3: RrL07PulseOverSig == 0 -> 什么都不做 (与 case 2 的同一判据对称) */
    baseline();
    g_in.par.IdSubStep = 3;
    g_in.rrlo.WaitCnt = 0;
    g_in.rrlo.RrL07PulseOverSig = 0;
    g_in.rrlo.SampleTimes = 14;
    run(ref_RrLoIdentify, new_RrLoIdentify, "RrLoIdentify", K_RR);
}

/* ---- 相位: 小函数直调 ---------------------------------------------------- */

static void phase_small(void)
{
    unsigned k;

    for (k = 0; k < 4; k++)
    {
        baseline();
        g_in.uvc.Number = (Uint)(k * 100);
        run(ref_InitRrLoVariable, new_InitRrLoVariable, "InitRrLoVariable", K_NONE);
    }

    for (k = 0; k < 4; k++)
    {
        baseline();
        g_in.epwm1.AQCTLA.all = (Uint16)(0x1111 + k);
        g_in.epwm1.AQCSFRC.all = (Uint16)(0x2222 + k);
        g_in.epwm2.TBCTR = (Uint16)(0x3333 + k);
        g_in.gpio.GPASET.all = 0;
        g_in.gpio.GPACLEAR.all = 0;
        run(ref_EnableTimer, new_EnableTimer, "EnableTimer", K_NONE);
    }

    for (k = 0; k < 6; k++)
    {
        baseline();
        g_in.rrlo.PwmCompareValue = (Uint)(k == 0 ? 0 : (k == 1 ? 1
                                   : (k == 2 ? 2000 : (k == 3 ? 2001 : 40000))));
        g_in.epwm1.CMPA.all = 0x1111;
        g_in.epwm2.CMPA.all = 0x2222;
        g_in.gpio.GPASET.all = 0;
        run(ref_RrGetVoltReBegin, new_RrGetVoltReBegin, "RrGetVoltReBegin", K_NONE);
    }

    for (k = 0; k < 3; k++)
    {
        baseline();
        g_in.epwm1.ETSEL.all = 0x1111;
        g_in.epwm2.ETSEL.all = 0x2222;
        g_in.epwm3.ETSEL.all = 0x3333;
        g_in.piec.PIEIER3.all = (Uint16)(k * 0x1111);
        g_in.piev.EPWM1_INT = dummy_isr;
        run(ref_SetPwmForRrLoId, new_SetPwmForRrLoId, "SetPwmForRrLoId", K_NONE);
    }

    for (k = 0; k < 4; k++)
    {
        baseline();
        g_in.lmio.WaitCnt = (int)(k * 7);
        g_in.lmio.LmAccValue = (long)(k * 13);
        g_in.lmio.IoAccValue = (long)(k * 17);
        g_in.mot.FreqPer = (Uint)(40 + k * 10);
        run(ref_InitLmIoVaribale, new_InitLmIoVaribale, "InitLmIoVaribale", K_NONE);
    }
}

/* ---- 相位: RrWaitAddGetUdc ----------------------------------------------- */

static void phase_wait(void)
{
    unsigned k;

    for (k = 0; k < 3; k++)
    {
        baseline();
        /* k=0 快支; k=1/2 走 51 次空转 -> ERROR_TUNE_FAIL */
        g_in.adc.ADCINTFLG.bit.ADCINT1 = (k == 0) ? 1 : 0;
        g_in.adc.SOCPRICTL.all = 0x1111;
        g_in.adc.ADCSOCFRC1.all = 0x2222;
        g_in.adc.ADCINTFLGCLR.all = 0x3333;
        g_in.err.ErrorCode.all = (Uint32)(k == 2 ? 0xFFFFFFFFUL : 0);
        gMockUdcFilter = (Uint)(1000 + k * 777);
        run(ref_RrWaitAddGetUdc, new_RrWaitAddGetUdc, "RrWaitAddGetUdc", K_RR_WAIT);
    }
}

/* ---- 相位: RrLoCalVaule -------------------------------------------------- */

static void phase_calv(void)
{
    static const int cycles[] = { 0, 1, 6 };
    unsigned ki, vi;

    for (ki = 0; ki < sizeof cycles / sizeof cycles[0]; ki++)
    {
        for (vi = 0; vi < 4; vi++)
        {
            baseline();
            g_in.rrlo.RrL0CycleCnt = cycles[ki];
            g_in.rrlo.UdcVoltage = (int)(vi == 0 ? 100 : (vi == 1 ? 3000
                                   : (vi == 2 ? 30000 : 32767)));
            g_in.rrlo.LoMax = 100;
            g_in.rrlo.LoMin = 50;
            g_in.rrlo.RrAndRsMax = 900;
            g_in.rrlo.RrAndRsMin = 700;
            pin_all();
            run(ref_RrLoCalVaule, new_RrLoCalVaule, "RrLoCalVaule", K_RR_CALV);
        }
    }
    /* 负数采样: abs 的两个分支 + 差值为负 */
    {
        int s;
        for (s = 0; s < 4; s++)
        {
            int i;
            baseline();
            for (i = 0; i < SAMPLE_ALL; i++)
            {
                g_in.rrlo.IsSampleValue[i] = (int)((i % 2 == 0) ? (s * 500 - 800)
                                                                : (900 - s * 300));
            }
            g_in.rrlo.IsSampleValue[4] = 0;
            g_in.rrlo.IsSampleValue[5] = 1000;
            pin_all();
            run(ref_RrLoCalVaule, new_RrLoCalVaule, "RrLoCalVaule", K_RR_CALV);
        }
    }
}

/* ---- 相位: ImCalKpKi ----------------------------------------------------- */

static void phase_kpki(void)
{
    static const Uint l0s[] = { 0, 9, 10, 100, 1000, 65535 };
    static const Uint lms[] = { 0, 1, 100, 1000, 65535 };
    static const Uint r1s[] = { 0, 50, 5000, 65535 };
    unsigned a, b, c;

    for (a = 0; a < sizeof l0s / sizeof l0s[0]; a++)
    {
        for (b = 0; b < sizeof lms / sizeof lms[0]; b++)
        {
            for (c = 0; c < sizeof r1s / sizeof r1s[0]; c++)
            {
                baseline();
                g_in.ext.L0 = l0s[a];
                g_in.ext.LM = lms[b];
                g_in.ext.R1 = r1s[c];
                g_in.pmsm.IdKp = 0x1111;
                g_in.pmsm.IqKp = 0x2222;
                g_in.pmsm.IdKi = 0x3333;
                g_in.pmsm.IqKi = 0x4444;
                pin_all();
                run(ref_ImCalKpKi, new_ImCalKpKi, "ImCalKpKi", K_KPKI);
            }
        }
    }
}

/* ---- 相位: LmIoIdentify -------------------------------------------------- */

static void phase_lm(void)
{
    static const int waits[] = { 0, 1, 499, 500, 799, 800, 801 };
    static const int freq[]  = { 0, 7, 8, 20, 29, 30, 40, 65535 };
    static const int dest[]  = { 0, 20, 40 };
    static const int shock[] = { 0, 1, 3 };
    static const int ticks[] = { 0, 127, 128, 129 };
    static const int adders[]= { -100, -51, -50, -49, 0, 49, 50, 51, 100 };
    static const int enc[]   = { -3000, -1, 0, 1, 3000 };
    unsigned wi, fi, di, si, ti, ai, ei, s;

    for (s = 1; s <= 5; s++)
    {
        for (wi = 0; wi < sizeof waits / sizeof waits[0]; wi++)
        {
            baseline();
            g_in.par.IdSubStep = (Uint16)s;
            g_in.lmio.WaitCnt = waits[wi];
            pin_all();
            run(ref_LmIoIdentify, new_LmIoIdentify, "LmIoIdentify", K_LM);
        }
    }

    /* case 3: 五层判据的交叉 */
    for (fi = 0; fi < sizeof freq / sizeof freq[0]; fi++)
    {
        for (di = 0; di < sizeof dest / sizeof dest[0]; di++)
        {
            for (si = 0; si < sizeof shock / sizeof shock[0]; si++)
            {
                for (wi = 0; wi < 3; wi++)
                {
                    baseline();
                    g_in.par.IdSubStep = 3;
                    g_in.cmd.FreqSyn = freq[fi];
                    g_in.lmio.DestinationFreq = dest[di];
                    g_in.lmio.VFOvShock = (Uint)shock[si];
                    g_in.lmio.WaitCnt = (int)(wi == 0 ? 0 : (wi == 1 ? 499 : 500));
                    g_in.vf.VFOvShock = 0x5555;
                    pin_all();
                    run(ref_LmIoIdentify, new_LmIoIdentify, "LmIoIdentify", K_LM);
                }
            }
        }
    }

    /* case 3: imPgEstTick 越过 128 之后的四层判据 */
    for (ti = 0; ti < sizeof ticks / sizeof ticks[0]; ti++)
    {
        for (ai = 0; ai < sizeof adders / sizeof adders[0]; ai++)
        {
            for (ei = 0; ei < sizeof enc / sizeof enc[0]; ei++)
            {
                baseline();
                g_in.par.IdSubStep = 3;
                g_in.cmd.FreqSyn = 40;
                g_in.lmio.DestinationFreq = 40;
                g_in.lmio.WaitCnt = 500;
                g_in.pg.imPgEstTick = (Uint)ticks[ti];
                g_in.pg.imDirAdder = adders[ai];
                g_in.pg.imFreqErr = 0x10000;
                g_in.pg.imFrqEncoder = 0x10000;
                g_in.pg.SpeedDir = 1;
                g_in.pg.PGDir = 0;
                g_in.pg.PGErrorFlag = 0x77;
                g_in.fvcs.SpeedEncoder = enc[ei];
                pin_all();
                run(ref_LmIoIdentify, new_LmIoIdentify, "LmIoIdentify", K_LM);
            }
        }
    }
    /* case 3: imFreqErr / imFrqEncoder 与 maxFrqErr 的三条比较 */
    {
        static const long ferrs[] = { 0, 100, 1000, 4000, 40000, 400000 };
        unsigned a, b;
        for (a = 0; a < sizeof ferrs / sizeof ferrs[0]; a++)
        {
            for (b = 0; b < sizeof ferrs / sizeof ferrs[0]; b++)
            {
                baseline();
                g_in.par.IdSubStep = 3;
                g_in.cmd.FreqSyn = 40;
                g_in.lmio.DestinationFreq = 40;
                g_in.lmio.WaitCnt = 500;
                g_in.pg.imPgEstTick = 129;
                g_in.pg.imDirAdder = 0;
                g_in.pg.imFreqErr = ferrs[a];
                g_in.pg.imFrqEncoder = ferrs[b];
                g_in.pg.PGErrorFlag = 0;
                g_in.fvcs.SpeedEncoder = 1000;
                pin_all();
                run(ref_LmIoIdentify, new_LmIoIdentify, "LmIoIdentify", K_LM);
            }
        }
    }

    /* case 4: FreqSyn 门槛 + WaitCnt 门槛 */
    for (fi = 0; fi < sizeof freq / sizeof freq[0]; fi++)
    {
        for (wi = 0; wi < sizeof waits / sizeof waits[0]; wi++)
        {
            baseline();
            g_in.par.IdSubStep = 4;
            g_in.cmd.FreqSyn = freq[fi];
            g_in.lmio.DestinationFreq = 40;
            g_in.lmio.WaitCnt = waits[wi];
            g_in.lmio.LmAccValue = 20000;
            g_in.lmio.IoAccValue = 3000;
            g_in.ext.L0 = 100;
            pin_all();
            run(ref_LmIoIdentify, new_LmIoIdentify, "LmIoIdentify", K_LM);
        }
    }
    /* case 4 的 TMS320F28035 折算: LM/I0 两条乘法 */
    {
        static const long lms[] = { 0, 1, 1000, 100000, 10000000 };
        unsigned k;
        for (k = 0; k < sizeof lms / sizeof lms[0]; k++)
        {
            baseline();
            g_in.par.IdSubStep = 4;
            g_in.cmd.FreqSyn = 10;
            g_in.lmio.DestinationFreq = 40;
            g_in.lmio.WaitCnt = 120;
            g_in.lmio.LmAccValue = lms[k];
            g_in.lmio.IoAccValue = lms[k] / 3;
            g_in.ext.L0 = 100;
            pin_all();
            run(ref_LmIoIdentify, new_LmIoIdentify, "LmIoIdentify", K_LM);
        }
    }

    /* case 5 */
    for (fi = 0; fi < sizeof freq / sizeof freq[0]; fi++)
    {
        baseline();
        g_in.par.IdSubStep = 5;
        g_in.cmd.FreqSyn = freq[fi];
        g_in.par.ParEstMstep = 1;
        g_in.par.StatusWord = TUNE_ACC;
        pin_all();
        run(ref_LmIoIdentify, new_LmIoIdentify, "LmIoIdentify", K_LM);
    }

    /* 越界步号: switch 没有 default, 应当什么都不做 */
    {
        static const Uint steps[] = { 0, 6, 100, 65535 };
        unsigned k;
        for (k = 0; k < sizeof steps / sizeof steps[0]; k++)
        {
            baseline();
            g_in.par.IdSubStep = steps[k];
            run(ref_LmIoIdentify, new_LmIoIdentify, "LmIoIdentify", K_LM);
        }
    }
}

/* ---- 相位: LmIoCalValue -------------------------------------------------- */

static void phase_lmcal(void)
{
    static const long iss[]  = { -1000000L, -100000L, 0, 100000L, 1000000L };
    static const long ims[]  = { -1000000L, 0, 100000L, 1000000L };
    static const long fr[]   = { 0, 1, 40000, 1000000L };
    static const int  vols[] = { 0, 3000, 32767, -32768 };
    unsigned a, b, c, d;

    for (a = 0; a < sizeof iss / sizeof iss[0]; a++)
    {
        for (b = 0; b < sizeof ims / sizeof ims[0]; b++)
        {
            for (c = 0; c < sizeof fr / sizeof fr[0]; c++)
            {
                for (d = 0; d < sizeof vols / sizeof vols[0]; d++)
                {
                    baseline();
                    g_in.lmio.lIsAccValue = iss[a];
                    g_in.lmio.lImAccValue = ims[b];
                    g_in.cmd.FreqReal = fr[c];
                    g_in.outv.Volt = vols[d];
                    g_in.lmio.WaitCnt = 3;
                    pin_all();
                    run(ref_LmIoCalValue, new_LmIoCalValue, "LmIoCalValue", K_LM_CAL);
                }
            }
        }
    }
}

/* ---- 相位: RrLoInPeriodAndCmpInt ----------------------------------------- */

static void phase_isr(void)
{
    static const Uint16 adc[] = { 0, 1, 2048, 32767, 4095 };
    static const int    err[] = { 0, 1, -1, 100, -100, 32767, -32768 };
    static const int    cmax[] = { 0, 1, 1000, 32767 };
    unsigned ai, ei, ci, st;

    for (ai = 0; ai < sizeof adc / sizeof adc[0]; ai++)
    {
        for (ei = 0; ei < sizeof err / sizeof err[0]; ei++)
        {
            for (ci = 0; ci < sizeof cmax / sizeof cmax[0]; ci++)
            {
                for (st = 0; st < 4; st++)
                {
                    baseline();
                    g_in.adcres.ADCRESULT1 = adc[ai];
                    g_in.exc.ErrIu = err[ei];
                    g_in.rrlo.CurrentMax = cmax[ci];
                    g_in.rrlo.SampleTimes = (int)(st == 0 ? 0 : (st == 1 ? 13
                                            : (st == 2 ? 14 : 15)));
                    g_in.rrlo.UdcVoltage = 3000;
                    g_in.udc.uDCFilter = 4000;
                    g_in.epwm1.ETCLR.all = 0;
                    g_in.epwm2.ETCLR.all = 0;
                    g_in.piec.PIEACK.all = 0;
                    run(ref_RrLoInPeriodAndCmpInt, new_RrLoInPeriodAndCmpInt,
                        "RrLoInPeriodAndCmpInt", K_RR_ISR);
                }
            }
        }
    }
}

/* ---- 相位: LmIoInPeriodInt ----------------------------------------------- */

static void phase_period(void)
{
    static const Uint steps[] = { 0, 3, 4, 5 };
    static const int  waits[] = { 0, 119, 120, 121 };
    static const int  saved[] = { 0, 1, 199, 200, 201 };
    static const long phasev[] = { 0, 16384, 32767, 65535 };
    unsigned si, wi, vi, pi;

    for (si = 0; si < sizeof steps / sizeof steps[0]; si++)
    {
        for (wi = 0; wi < sizeof waits / sizeof waits[0]; wi++)
        {
            for (vi = 0; vi < sizeof saved / sizeof saved[0]; vi++)
            {
                for (pi = 0; pi < sizeof phasev / sizeof phasev[0]; pi++)
                {
                    baseline();
                    g_in.par.IdSubStep = steps[si];
                    g_in.lmio.WaitCnt = waits[wi];
                    g_in.lmio.DataSavedNum = saved[vi];
                    /* IMPhase 是 **long**, 模块取的是 `IMPhase>>16` —— 也就是高 16 位。
                     * 直接写 phasev[pi] (<= 65535) 会让 m_VoltagePhase 恒为 0, 整条
                     * qsin 通路一次都没有被非零相位驱动过。必须左移 16 位, 低 16 位
                     * 再铺一点花样 (它们会被 >>16 丢掉, 顺带能抓「>>16 写成 >>15」)。 */
                    g_in.phase.IMPhase = ((long)phasev[pi] << 16) | (long)(pi * 0x1111u);
                    g_in.exc.IvValue = 3000;
                    g_in.exc.IuValue = 2000;
                    g_in.amp.Theta = 8000;
                    g_in.lmio.lIsValue = 5000;
                    pin_all();
                    run(ref_LmIoInPeriodInt, new_LmIoInPeriodInt,
                        "LmIoInPeriodInt", K_PERIOD);
                }
            }
        }
    }
}

/* ---- 相位: 边界命中 ------------------------------------------------------ */

/* 这一相只做一件事: 把"判据正好落在门槛上"的输入钉出来。
 *
 * 为什么非它不可: 随机相把字段撒到 0..65535 全空间, 于是 `Number >= 1024` 这类
 * 门槛只有输入**恰好**是 1022/1023/1024 时才区分得开 1023 与 1024 两个门槛 ——
 * 命中概率 3/65536, 20000 个随机用例的期望命中数不到 1。凡是"门槛差 1"的变异
 * 都栽在这里, 表现为 MISSED 而不是"被测代码没问题"。
 *
 * 下面每个块都是从被测代码的算式**反解**出来的区分输入, 注释里写明解的是哪个
 * 不等式。反解而不是试, 是因为随机搜索的期望命中数是小数, 试不出来。
 *
 * 还有一类要在这里一起解决: 中间量只活在**局部变量**里。例如 RsIdentify 的
 * m_UData1 只用于 `abs(gIUVWQ12.U) >= m_UData1` 这一次比较, 所以光让 m_UData1
 * 差 4 没用, 必须让 abs(U) 落在这 4 的窗口**里面** —— 下面 iuvw.U 就是按这个
 * 窗口算出来的。 */
static void phase_bound(void)
{
    static const Uint numbers[] = { 511, 512, 513, 1022, 1023, 1024, 1025 };
    static const int  cycles[]  = { 2, 3, 4, 5, 6 };
    static const int  dirs[]    = { 48, 49, 50, 51, 52, -50, -49, -48, 100 };
    unsigned i, j, k;

    /* --- RsIdentify case 3/5: 最大电流钳位 (基准 103~106 行) ---------------
     * m_MaxCur = InvCurrent*3277>>12。系数 3277->3276 只在 InvCurrent >= 4096
     * 时差 1 (差值就是 InvCurrent/4096 的整数部分)。差值还得活到结果里:
     * 钳位式 (m_MaxCur*m_UData1)/Current 的差是 m_UData1/Current, 只有
     * m_UData1 >= Current 时才 >= 1, 即 Current^2 <= 1024*CurrentGet。
     *   j=0 解 InvCurrent=4095, Current=4000, CurrentGet=65535
     *       -> m_UData1 16776, 钳位结果 13739 vs 13735, 窗口 [13735,13739)
     *   j=1 解"钳位后多算 1": 需要 (m_MaxCur*m_UData1) % Current == Current-1
     *       -> Current=1001, CurrentGet=65535, m_UData1 1504, 结果 600 vs 601
     * 注意 m_UData1 是**局部量**, 只喂给下面这一次比较, 所以 iuvw.U 必须落在
     * 两个结果之间, 否则两侧算出的 m_UData1 再不同也没人看。 */
    for (j = 0; j < 2; j++)
    {
        baseline();
        g_in.par.IdSubStep    = 3;
        g_in.inv.InvTypeApply = 17;
        if (j == 0)
        {
            g_in.inv.InvCurrent = 4095;
            g_in.mot.Current    = 4000;
            g_in.mot.CurrentGet = 65535;
            g_in.iuvw.U         = 13737;
        }
        else
        {
            g_in.inv.InvCurrent = 500;
            g_in.mot.Current    = 1001;
            g_in.mot.CurrentGet = 65535;
            g_in.iuvw.U         = 600;
        }
        run(ref_RsIdentify, new_RsIdentify, "RsIdentify", K_RS);
    }

    /* --- RsIdentify case 4 / case 6: Number 的 512 / 1024 两道门槛 --------
     * 两道门槛都是"先 ++ 再判", 所以输入 Number 取 门槛-1 才是分界点:
     * 输入 1022 -> ++ 得 1023, 1023 门槛命中而 1024 不命中。 */
    for (i = 0; i < sizeof numbers / sizeof numbers[0]; i++)
    {
        baseline();
        g_in.par.IdSubStep = 4;
        g_in.uvc.Number    = numbers[i];
        run(ref_RsIdentify, new_RsIdentify, "RsIdentify", K_RS);

        baseline();
        g_in.par.IdSubStep = 6;
        g_in.uvc.Number    = numbers[i];
        g_in.uvc.IdRsCnt   = (Uint)(i & 1);   /* 两条收尾支都要走到 */
        pin_rs();
        run(ref_RsIdentify, new_RsIdentify, "RsIdentify", K_RS);
    }

    /* --- RrLoIdentify case 3: 6 <= RrL0CycleCnt (基准 306 行) -------------
     * RrLoCalVaule 先 ++, 所以输入 4 才让比较看到 5。 */
    for (i = 0; i < sizeof cycles / sizeof cycles[0]; i++)
    {
        baseline();
        g_in.par.IdSubStep          = 3;
        g_in.rrlo.WaitCnt           = 0;
        g_in.rrlo.RrL07PulseOverSig = 1;
        g_in.rrlo.SampleTimes       = 14;
        g_in.rrlo.RrL0CycleCnt      = cycles[i];
        pin_rrlo();
        run(ref_RrLoIdentify, new_RrLoIdentify, "RrLoIdentify", K_RR);
    }

    /* --- LmIoIdentify case 3: 方向判据 + 误差判据 (基准 673~690 行) -------
     * 这一段的输入全都会被模块**先改一遍**再比较, 所以钉的是"改之前"的值:
     *   imDirAdder  --  (SpeedEncoder*FreqSyn 为正时走 else 支)
     *   imFreqErr   += (FreqSyn - abs(SpeedEncoder))
     *   imFrqEncoder+= abs(SpeedEncoder)
     * 取 SpeedEncoder = 0 让后两条的增量只剩 FreqSyn / 0, 好算。
     *
     * 第一组 (dirs): DestinationFreq=40 -> maxFrqErr=3, 取 FreqSyn=29,
     *   imFreqErr = -29 (加完正好 0, 不触发 2 号错误码), imFrqEncoder 取大值
     *   让编码器判据不触发。于是 PGErrorFlag 只由 imDirAdder 那一支决定 ——
     *   49/50 分界与 -50/-49 分界才暴露出来。
     *
     * 第二组: DestinationFreq=1024 -> maxFrqErr=100 (101 那一侧是 101)。
     *   imFreqErr = 12928 -> >>7 = 101, 正好卡在 100/101 之间 (LM31/LM32);
     *   imFrqEncoder = 12800 -> >>7 = 100, 卡在"<100 不成立 / >>8=50 成立"之间
     *   (LM34); 取 0 让编码器判据成立 (LM35 的错误码 1 vs 3)。 */
    for (i = 0; i < sizeof dirs / sizeof dirs[0]; i++)
    {
        baseline();
        g_in.par.IdSubStep          = 3;
        g_in.cmd.FreqSyn            = 29;
        g_in.fvcs.SpeedEncoder      = 0;
        g_in.lmio.DestinationFreq   = 40;
        g_in.lmio.WaitCnt           = 500;
        g_in.lmio.VFOvShock         = 0;
        g_in.pg.imPgEstTick         = 128;
        g_in.pg.imDirAdder          = dirs[i] + 1;   /* 模块会先 -- */
        g_in.pg.imFreqErr           = -29;
        g_in.pg.imFrqEncoder        = 65536;
        run(ref_LmIoIdentify, new_LmIoIdentify, "LmIoIdentify", K_LM);
    }

    for (j = 0; j < 3; j++)
    {
        baseline();
        g_in.par.IdSubStep          = 3;
        g_in.cmd.FreqSyn            = 1013;
        g_in.fvcs.SpeedEncoder      = 0;
        g_in.lmio.DestinationFreq   = 1024;
        g_in.lmio.WaitCnt           = 500;
        g_in.lmio.VFOvShock         = 0;
        g_in.pg.imPgEstTick         = 128;
        if (j == 2)
        {
            g_in.pg.imDirAdder   = 101;      /* 减完 100 -> 走 >50 支, 不动错误码 */
            g_in.pg.imFreqErr    = -1013;    /* 加完 0 */
            g_in.pg.imFrqEncoder = 0;
        }
        else
        {
            g_in.pg.imDirAdder   = 1;        /* 减完 0 -> 走 >-50 支, 置 1 */
            g_in.pg.imFreqErr    = 11915;    /* 加完 12928 -> >>7 = 101 */
            g_in.pg.imFrqEncoder = (j == 0) ? 12928 : 12800;
        }
        run(ref_LmIoIdentify, new_LmIoIdentify, "LmIoIdentify", K_LM);
    }

    /* --- LmIoInPeriodInt: qsin 的两个相位常量 (基准 775 / 776 行) ---------
     * 相位偏 1 只让 qsin 的输出差 0~3 (导数是 |cos|*32767*pi/32768), 再经过
     * `>>15` 与 qsqrt 两次量化, 所以既要取 |Iu|/|Iv| 最大, 又要取 |cos| 最大
     * 的相位 —— 21845 与 38229 附近各扫 +-64, 窗口外差值为 0, 扫了也没用。
     * Theta 与 Iu 的符号各扫两档: 量化落点对它们的符号敏感。 */
    for (k = 0; k < 2; k++)
    {
        int t;
        long p;

        for (t = 0; t < 2; t++)
        {
            for (p = -64; p <= 64; p++)
            {
                int half;

                for (half = 0; half < 2; half++)
                {
                    baseline();
                    g_in.par.IdSubStep     = 4;
                    g_in.lmio.WaitCnt      = 0;
                    g_in.lmio.DataSavedNum = 0;
                    g_in.exc.IuValue       = (k == 0) ? 30000 : -30000;
                    g_in.exc.IvValue       = 30000;
                    g_in.lmio.lIsValue     = (t == 0) ? 30000 : 30015;
                    g_in.amp.Theta         = (t == 0) ? 0 : 8000;
                    /* 同上: 模块取 `IMPhase>>16`, 所以相位必须写进高 16 位。 */
                    g_in.phase.IMPhase     = (long)(((half == 0) ? (21845L + p) : (38229L + p)) & 0xFFFFL) << 16;
                    pin_all();
                    run(ref_LmIoInPeriodInt, new_LmIoInPeriodInt,
                        "LmIoInPeriodInt", K_PERIOD);
                }
            }
        }
    }
}

/* ---- 随机相 -------------------------------------------------------------- */

static unsigned long g_seed = 0x12345678UL;

static unsigned long rnd(void)
{
    g_seed = g_seed * 1103515245UL + 12345UL;
    return (g_seed >> 8) & 0xFFFFFFUL;
}

typedef struct ENTRY_DEF
{
    entry_t     ref;
    entry_t     nw;
    const char *name;
    int         kind;
} ENTRY;

static const ENTRY g_entries[] =
{
    { ref_RsIdentify,            new_RsIdentify,            "RsIdentify",            K_RS },
    { ref_RrLoIdentify,          new_RrLoIdentify,          "RrLoIdentify",          K_RR },
    { ref_SetPwmForRrLoId,       new_SetPwmForRrLoId,       "SetPwmForRrLoId",       K_NONE },
    { ref_InitRrLoVariable,      new_InitRrLoVariable,      "InitRrLoVariable",      K_NONE },
    { ref_EnableTimer,           new_EnableTimer,           "EnableTimer",           K_NONE },
    { ref_RrLoInPeriodAndCmpInt, new_RrLoInPeriodAndCmpInt, "RrLoInPeriodAndCmpInt", K_RR_ISR },
    { ref_RrWaitAddGetUdc,       new_RrWaitAddGetUdc,       "RrWaitAddGetUdc",       K_RR_WAIT },
    { ref_RrGetVoltReBegin,      new_RrGetVoltReBegin,      "RrGetVoltReBegin",      K_NONE },
    { ref_RrLoCalVaule,          new_RrLoCalVaule,          "RrLoCalVaule",          K_RR_CALV },
    { ref_ImCalKpKi,             new_ImCalKpKi,             "ImCalKpKi",             K_KPKI },
    { ref_LmIoIdentify,          new_LmIoIdentify,          "LmIoIdentify",          K_LM },
    { ref_InitLmIoVaribale,      new_InitLmIoVaribale,      "InitLmIoVaribale",      K_NONE },
    { ref_LmIoCalValue,          new_LmIoCalValue,          "LmIoCalValue",          K_LM_CAL },
    { ref_LmIoInPeriodInt,       new_LmIoInPeriodInt,       "LmIoInPeriodInt",       K_PERIOD },
    { ref_ChgParForEst,          new_ChgParForEst,          "ChgParForEst",          K_CHG }
};

static void phase_random(long n)
{
    long k;

    for (k = 0; k < n; k++)
    {
        const ENTRY *e = &g_entries[rnd() % (sizeof g_entries / sizeof g_entries[0])];
        int i;

        baseline();

        g_in.par.IdSubStep   = (Uint16)(rnd() % 9);
        g_in.par.ParEstMstep = (Uint16)(rnd() % 8);
        g_in.par.StatusWord  = (TUNE_TO_FUNCTION_WORLD_ENUM)(rnd() % 4);

        g_in.inv.InvCurrent     = (Uint)(rnd() % 65536);
        g_in.inv.InvType        = (Uint)(rnd() % 40);
        g_in.inv.InvTypeApply   = (Uint)(rnd() % 40);
        g_in.inv.InvTypeSet     = (Uint)(rnd() % 700);
        g_in.inv.InvVoltageType = (Uint)(rnd() % 8);
        g_in.inv.CurrentCoff    = (Uint)(rnd() % 65536);

        g_in.mot.MotorType  = (Uint)(rnd() % 4);
        g_in.mot.Current    = (Uint)(rnd() % 65536);
        g_in.mot.CurrentGet = (Uint)(rnd() % 65536);
        g_in.mot.Votage     = (Uint)(rnd() % 65536);
        g_in.mot.FreqPer    = (Uint)(rnd() % 200);

        g_in.udc.uDCFilter = (Uint)(rnd() % 65536);
        gMockUdcFilter = (Uint)(rnd() % 65536);

        g_in.iuvw.U = (int)rnd();
        g_in.iuvw.V = (int)rnd();
        g_in.iuvw.W = (int)rnd();

        g_in.exc.ErrIu   = (int)rnd();
        g_in.exc.IuValue = (int)rnd();
        g_in.exc.IvValue = (int)rnd();

        g_in.amp.Theta = (int)rnd();
        /* `gPhase.IMPhase` 是 long, 模块取 `>>16`。原来的 `(long)rnd()` 里 rnd() 只到
         * 0xFFFFFF, 于是 m_VoltagePhase 只覆盖 0..255 —— 相位通路实际上只被扫了
         * 1/256。铺满高 16 位, 低 16 位也铺, 两段都能被 `>>16`/`>>15` 这类变异检验。 */
        g_in.phase.IMPhase = ((long)(rnd() & 0xFFFFUL) << 16) | (long)(rnd() & 0xFFFFUL);
        g_in.outv.Volt = (int)rnd();

        g_in.cmd.FreqSyn  = (int)rnd();
        g_in.cmd.FreqReal = (long)rnd();

        g_in.fvcs.SpeedEncoder = (int)rnd();

        g_in.adcst.DelayApply = (Uint)(rnd() % 65536);
        g_in.adc.ADCINTFLG.bit.ADCINT1 = (Uint16)(rnd() % 2);
        g_in.adcres.ADCRESULT1 = (Uint16)(rnd() % 4096);

        g_in.uvc.Rs_PRD      = (Uint)(rnd() % 65536);
        g_in.uvc.Comper      = (Uint)(rnd() % 65536);
        g_in.uvc.ComperL     = (Uint)(rnd() % 65536);
        g_in.uvc.Number      = (Uint)(rnd() % 65536);
        g_in.uvc.IdRsCnt     = (Uint)(rnd() % 3);
        g_in.uvc.IdRsDelay   = (Uint)(rnd() % 4);
        g_in.uvc.IdRsBak     = (Uint)(rnd() % 65536);
        g_in.uvc.TotalU      = (Ulong)rnd();
        g_in.uvc.TotalV      = (Ulong)rnd();
        g_in.uvc.TotalVoltL  = (Ulong)rnd();
        g_in.uvc.TotalVolt   = (Ulong)rnd();
        g_in.uvc.TotalI      = (Ulong)rnd();
        g_in.uvc.TotalIL     = (Ulong)rnd();
        g_in.uvc.UDivVSave   = (Uint)(rnd() % 65536);

        g_in.rrlo.WaitCnt        = (int)(rnd() % 1200);
        g_in.rrlo.CurrentMax     = (int)rnd();
        g_in.rrlo.CurrentRatio   = (int)rnd();
        g_in.rrlo.RrL07PulseOverSig = (int)(rnd() % 3);
        g_in.rrlo.RrL0CycleCnt   = (int)(rnd() % 10);
        g_in.rrlo.SampleTimes    = (int)(rnd() % 16);
        g_in.rrlo.PwmCompareValue= (Uint)(100 + rnd() % 1901);
        g_in.rrlo.UdcVoltage     = (int)rnd();
        g_in.rrlo.RrAndRsMax     = (long)rnd();
        g_in.rrlo.RrAndRsMin     = (long)rnd();
        g_in.rrlo.LoMax          = (long)rnd();
        g_in.rrlo.LoMin          = (long)rnd();
        g_in.rrlo.RrAndRsAccValue= (long)rnd();
        g_in.rrlo.LoAccValue     = (long)rnd();
        for (i = 0; i < SAMPLE_ALL; i++)
        {
            g_in.rrlo.IsSampleValue[i] = (int)(rnd() % 6001) - 3000;
        }
        /* 第 0 对差固定 1000: m_IsZeroVoltDiff 是 RrLoCalVaule 的除数, 让它在
         * [1000-400, 1000+400] 里恒不为 0, 同时其余四对仍然随机 (判据的求和形状
         * 照样被测到)。 */
        g_in.rrlo.IsSampleValue[4] = 0;
        g_in.rrlo.IsSampleValue[5] = 1000;

        g_in.lmio.WaitCnt        = (int)(rnd() % 1300);
        g_in.lmio.DestinationFreq= (int)(rnd() % 100);
        g_in.lmio.DataSavedNum   = (int)(rnd() % 300);
        g_in.lmio.VFOvShock      = (Uint)(rnd() % 10);
        g_in.lmio.lIsValue       = (long)rnd();
        g_in.lmio.LmAccValue     = (long)rnd();
        g_in.lmio.IoAccValue     = (long)rnd();
        g_in.lmio.lImAccValue    = (long)rnd();
        g_in.lmio.lIsAccValue    = (long)rnd();

        g_in.ext.R1   = (Uint)(rnd() % 65536);
        g_in.ext.R2   = (Uint)(rnd() % 65536);
        g_in.ext.L0   = (Uint)(rnd() % 65536);
        g_in.ext.LM   = (Uint)(rnd() % 65536);
        g_in.ext.I0   = (Uint)(rnd() % 65536);
        g_in.ext.RsPm = (Uint)(rnd() % 65536);

        g_in.pmsm.IdKp = (Uint)rnd();
        g_in.pmsm.IqKp = (Uint)rnd();
        g_in.pmsm.IdKi = (Uint)rnd();
        g_in.pmsm.IqKi = (Uint)rnd();

        g_in.pg.imPgEstTick  = (Uint)(rnd() % 400);
        g_in.pg.imDirAdder   = (int)(rnd() % 200) - 100;
        g_in.pg.imFreqErr    = (long)rnd();
        g_in.pg.imFrqEncoder = (long)rnd();
        g_in.pg.SpeedDir     = (Uint)(rnd() % 4);
        g_in.pg.PGDir        = (Uint)(rnd() % 4);
        g_in.pg.PGErrorFlag  = (Uint)(rnd() % 256);

        g_in.vf.VFOvShock   = (Uint)(rnd() % 10);
        g_in.vf.VFWsComp    = (Uint)rnd();
        g_in.vf.VFLineType  = (Uint)rnd();
        g_in.vf.VFTorqueUp  = (Uint)rnd();
        g_in.base.FcSet     = (Uint)rnd();
        g_in.ctrType        = (CONTROL_MOTOR_TYPE_ENUM)(rnd() % 30);
        g_in.pwm.SoftPWMTune= (Uint)rnd();
        g_in.err.ErrorCode.all = (Uint32)rnd();

        g_in.piec.PIEIER1.all = (Uint16)rnd();
        g_in.piec.PIEIER3.all = (Uint16)rnd();
        g_in.piec.PIEACK.all  = (Uint16)rnd();
        g_in.gpio.GPASET.all  = (Uint32)rnd();
        g_in.gpio.GPACLEAR.all= (Uint32)rnd();
        g_in.epwm1.ETCLR.all  = (Uint16)rnd();
        g_in.epwm2.ETCLR.all  = (Uint16)rnd();

        pin_all();
        run(e->ref, e->nw, e->name, e->kind);
    }
}

/* ---- main ---------------------------------------------------------------- */

/* 没有 <stdlib.h>, 所以没有 atol —— 自己解析, 非数字就退回默认值。 */
static long parse_long(const char *s)
{
    long v = 0;

    if (s == 0 || *s == 0)
    {
        return -1;
    }
    while (*s >= '0' && *s <= '9')
    {
        v = v * 10 + (*s - '0');
        s++;
    }
    return (*s == 0) ? v : -1;
}

int main(int argc, char **argv)
{
    long n = (argc > 1) ? parse_long(argv[1]) : 20000;
    long base;

    if (n < 0)
    {
        n = 20000;
    }

    phase_chg();
    printf("  ChgParForEst                 : %ld\n", g_cases);

    base = g_cases;
    phase_rs();
    printf("  RsIdentify                   : %ld\n", g_cases - base);

    base = g_cases;
    phase_rr();
    printf("  RrLoIdentify                 : %ld\n", g_cases - base);

    base = g_cases;
    phase_small();
    printf("  small entry points           : %ld\n", g_cases - base);

    base = g_cases;
    phase_wait();
    printf("  RrWaitAddGetUdc              : %ld\n", g_cases - base);

    base = g_cases;
    phase_calv();
    printf("  RrLoCalVaule                 : %ld\n", g_cases - base);

    base = g_cases;
    phase_kpki();
    printf("  ImCalKpKi                    : %ld\n", g_cases - base);

    base = g_cases;
    phase_lm();
    printf("  LmIoIdentify                 : %ld\n", g_cases - base);

    base = g_cases;
    phase_lmcal();
    printf("  LmIoCalValue                 : %ld\n", g_cases - base);

    base = g_cases;
    phase_isr();
    printf("  RrLoInPeriodAndCmpInt        : %ld\n", g_cases - base);

    base = g_cases;
    phase_period();
    printf("  LmIoInPeriodInt              : %ld\n", g_cases - base);

    base = g_cases;
    phase_bound();
    printf("  boundary targeted            : %ld\n", g_cases - base);

    base = g_cases;
    phase_random(n);
    printf("  random mixed                 : %ld\n", g_cases - base);

    printf("  DIV_PIN (input normalised)   : %ld\n", g_divPin);

    /* 覆盖门: 44 个分支计数器必须**全部非零**。任何一个为 0 都说明测试台没走到
     * 那一支, 于是那一支上的变异会变成假 MISSED —— 这正是"我加了覆盖"这句话
     * 最需要证据的地方, 所以让它直接判失败, 而不是靠人去看。 */
    {
        int i;
        int zero = 0;

        printf("  branch coverage (all 44 must be > 0):\n");
        for (i = 0; i < COV_COUNT; i++)
        {
            printf("    %-14s %ld\n", g_covName[i], g_cov[i]);
            if (g_cov[i] == 0)
            {
                printf("BENCH-BUG coverage hole: %s never reached\n", g_covName[i]);
                zero++;
            }
        }
        g_fails += zero;
    }

    printf("MotorImParEst difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    return g_fails ? 1 : 0;
}
