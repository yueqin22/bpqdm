/* MotorImParEst 用例的共享 mock。
 *
 * 这个文件提供模块**用到但不定义**的一切, 两侧翻译单元看到的是同一块内存 ——
 * 于是一个"改写后不再写某个寄存器 / 不再写某个字段"的 bug, 会在 new 侧留下与
 * ref 侧不同的残值, 被 driver 逐字段比出来。
 *
 * 五组东西:
 *
 *   1. 外设寄存器   EPwm1/2/3Regs / AdcRegs / AdcResult / PieCtrlRegs /
 *      PieVectTable / GpioDataRegs。结构体来自真设备头 (见 real.txt), 字段顺序
 *      与位域布局是芯片的, 不是仿制品。本模块会写其中四类:
 *        EPwm1/2/3Regs   ETSEL / ETPS / TBPRD / TBCTL / TBCTR / CMPA / CMPCTL /
 *                        AQSFRC / AQCSFRC / AQCTLA / DBCTL / ETCLR
 *        AdcRegs         SOCPRICTL / ADCSOCFRC1 / ADCINTFLGCLR / ADCINTFLG
 *        PieCtrlRegs     PIEIER1 / PIEIER3 / PIEACK
 *        PieVectTable    EPWM1_INT / EPWM2_INT
 *        GpioDataRegs    GPASET / GPACLEAR 的 GPIO7 (EnableDrive / DisableDrive)
 *
 *   2. CPU 控制序列 EINT / DINT / ERTM / DRTM / EALLOW / EDIS / ESTOP0 在 shim
 *      里被改成 gHostCpuCtl[] 的自增。它们是**没有内存操作数**的真指令, 不这么做
 *      的话"改写少了一次 EDIS""临界区套错位置"在宿主上完全不可见。
 *
 *   3. 调用日志     模块派发到的每个被调者往 gCallLog 里追加自己的 id。本模块
 *      真正"决定做什么"的地方之一就是这些调用 (InitSetPWM / InitSetAdc /
 *      SetInterruptEnable / GetUDCInfo / CBCLimitCurPrepare), 空桩会让
 *      "case 4 里漏掉 InitSetAdc" 与原文逐字节相同。
 *
 *   4. 别的翻译单元拥有的全局量。本模块不定义它们 (nm2000 的未定义符号表里有
 *      23 个), 所以 ref/new 两侧共用同一份 —— driver 用"快照 -> 跑 ref -> 记结果
 *      -> 恢复快照 -> 跑 new -> 比结果"的办法保证两侧起点相同。
 *
 *   5. GetUDCInfo 不是空桩: 它把 gMockUdcFilter 写进 gUDC.uDCFilter。真实现
 *      (MotorInfoCollect.c) 就是干这个的, 而 RrLoIdentify / RrLoInPeriodAndCmpInt
 *      都拿 uDCFilter 当输入。空桩会让"改写漏调 GetUDCInfo" 与"uDCFilter 恰好没变"
 *      无法区分。
 */

#include "MotorParaIDinclude.h"

/* ---- 1. 外设寄存器 ------------------------------------------------------- */

volatile struct EPWM_REGS      EPwm1Regs;
volatile struct EPWM_REGS      EPwm2Regs;
volatile struct EPWM_REGS      EPwm3Regs;
volatile struct ADC_REGS       AdcRegs;
volatile struct ADC_RESULT_REGS AdcResult;
volatile struct PIE_CTRL_REGS  PieCtrlRegs;
struct PIE_VECT_TABLE          PieVectTable;
volatile struct GPIO_DATA_REGS GpioDataRegs;

/* ---- 2. CPU 控制序列 ----------------------------------------------------- */

volatile Uint16 gHostCpuCtl[7];

/* ---- 3. 调用日志 --------------------------------------------------------- */

#define CALL_LOG_LEN 64

Uint16 gCallLog[CALL_LOG_LEN];
Uint16 gCallCount;

static void log_call(Uint16 id)
{
    if (gCallCount < CALL_LOG_LEN)
    {
        gCallLog[gCallCount] = id;
    }
    gCallCount++;
}

/* ---- 4. 外部量 ----------------------------------------------------------- */

/* GetUDCInfo 的输入。driver 每相都改它, 于是 uDCFilter 不是常量。 */
Uint gMockUdcFilter;

/* 被调者清单来自目标端 nm2000 的未定义符号表, 不是人眼数的。 */
void InitSetPWM(void)         { log_call(1); }
void InitSetAdc(void)         { log_call(2); }
void SetInterruptEnable(void) { log_call(3); }
void CBCLimitCurPrepare(void) { log_call(4); }

void GetUDCInfo(void)
{
    log_call(5);
    gUDC.uDCFilter = gMockUdcFilter;
}

/* 只有地址被取用 (RrLoIdentify case 4 把 EPWM1_INT 指回它)。
 * 两侧取的是同一个外部符号, 所以地址相同、比较相等。 */
void EPWM1_zero_isr(void) { log_call(6); }

/* ---- 6. ADC 侧的两次"无痕迹"动作 ---------------------------------------- */

/* host_patch.json 把 SystemDefine.h 的两条宏改成调这两个函数:
 *     ADC_CLEAR_INT_FLAG  ->  adcClearIntFlag()
 *     ADC_END_CONVERSIN   ->  adcEndConversion()
 * 宏本身是"写一个只写寄存器"和"读一个只读位", 都不在内存里留下可比较的痕迹,
 * 于是 "少清一次中断标志" 与 "超时门槛差 1" 这两类变异在纯状态比较下恒等。
 * 改成函数调用后, 次数进了比较镜像 —— 这是本模块唯一能观测它们的地方。
 *
 * 行为本身保持与真宏一致: 清标志写 ADCINTFLGCLR 的 ADCINT1 位 (写 1 清零),
 * 读完成标志返回 ADCINTFLG 的 ADCINT1 位。 */
Uint16 gAdcClrCnt;
Uint16 gAdcReadCnt;

void adcClearIntFlag(void)
{
    gAdcClrCnt++;
    AdcRegs.ADCINTFLGCLR.bit.ADCINT1 = 1;
}

Uint16 adcEndConversion(void)
{
    gAdcReadCnt++;
    return (Uint16)AdcRegs.ADCINTFLG.bit.ADCINT1;
}

/* ---- 5. 别的翻译单元拥有的全局量 ----------------------------------------- */

ADC_STRUCT                 gADC;
UDC_STRUCT                 gUDC;
UVW_STRUCT                 gIUVWQ12;
AMPTHETA_STRUCT            gIAmpTheta;
CUR_EXCURSION_STRUCT       gExcursionInfo;
ANGLE_STRUCT               gPhase;
OUT_VOLT_STRUCT            gOutVolt;
PWM_OUT_STRUCT             gPWM;
VF_INFO_STRUCT             gVFPar;
PG_DATA_STRUCT             gPGData;
FVC_SPEED_STRUCT           gFVCSpeed;
INV_STRUCT                 gInvInfo;
CONTROL_MOTOR_TYPE_ENUM    gCtrMotorType;
MOTOR_STRUCT               gMotorInfo;
BASE_COMMAND_STRUCT        gMainCmd;
BASE_PAR_STRUCT            gBasePar;
PMSM_EST_PARAM_DATA        gPmParEst;
struct FAULT_CODE_INFOR_STRUCT_DEF gError;

/* 真工程里这五个定义在 MotorParEst.c (gUVCoff / gRrLoIdentify / gLmIoIdentify /
 * gGetParVarable / gMotorExtReg)。本用例不编 MotorParEst.c, 所以在这里给出定义;
 * 两侧共用一份, driver 用快照/恢复保证起点一致。 */
UV_BIAS_COFF_STRUCT        gUVCoff;
IDENTIFY_RRLO_VARIABLE     gRrLoIdentify;
IDENTIFY_LMIO_VARIABLE     gLmIoIdentify;
MOTOR_PARA_EST             gGetParVarable;
MOTOR_EXTERN_STRUCT        gMotorExtReg;
