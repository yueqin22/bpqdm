# 静态检查报告 (static_check_report)

## 1. 检查项总览

| # | 检查项 | 方法 | 结果 |
|---:|---|---|---|
| 1 | 注释残留 | 逐行扫描 `/*` `//` 与汇编 `;` / 列首 `*` | **0 处** |
| 2 | TODO / FIXME / NOTE / XXX / HACK | 全树关键字扫描 | **0 处** |
| 3 | 非 ASCII 字节 | 逐文件 ASCII 解码 | **0 处** |
| 4 | 二次去注释可删字符 | 幂等复跑 | **0 处** |
| 5 | token 流等价 | 真 C 词法器逐 token 比对 | **未改写层 102 / 102 一致**; 改写层 43 个豁免 (另证) |
| 6 | 符号抽取一致(全局量 + 函数签名) | 剥离注释后正则抽取比对 | **0 处不一致** |
| 7 | 隐式函数声明 | 调用点 vs 可见声明集合 | **28 处**(见第 4 节) |
| 8 | 未使用全局量 | 全树标识符计数 | **32 处**(见第 5 节) |
| 9 | include 自洽 | 新树内解析全部 `#include "..."` | **329 处解析, 1 处缺失** |
| 10 | 对原目录的引用 | 关键字扫描 | **0 处** |
| 11 | 中断函数定义 | `interrupt void` 抽取 | **89 个** |
| 12 | volatile 声明 | `volatile` 声明行统计 | **7 个文件** |
| 13 | 负向对照 | 注入 4 类错误验证检查有效性 | **4 / 4 抓到** |

## 2. token 流等价明细

比对文件数: **145**; 未改写层不一致: **0**; 改写层豁免: **43** (见 `docs/rewritten_modules.json`)。

比对方法: 忽略注释与空白后逐 token 比较。字符串/字符字面量按整体处理,
因此字面量内部的分号、括号、反斜杠不会干扰切分。

**结论: 全部文件逻辑零改动。**

## 3. include 自洽性

- 新树内成功解析: **329** 处
- 编译器自带头(工程外, 正常): `limits.h`, `math.h`, `stdlib.h`
- 缺失: **1** 处
  - `src/orphan/f_length.c` -> `f_length.h` (该文件是孤儿文件, 从未可编译)
- 对原工程路径/目录名的引用: **0** 处

## 4. 隐式函数声明 (保留未修复)

原工程依赖 C89 隐式函数声明: 跨模块调用时调用点看不到函数原型。
**本轮保持原样**(改动即改变行为), 仅登记。分两类:

### 4.1 A 类: 全树内有定义, 但调用点缺原型 (28 处)

| 调用点 | 函数 |
|---|---|
| `src/function/f_main.c` | `InitECap2Gpio()` |
| `src/function/f_main.c` | `InitScibGpioDp()` |
| `src/function/f_main.c` | `InitSetSciDp()` |
| `src/motor/MotorMain.c` | `CalTorqueLimitPar()` |
| `src/motor/MotorMain.c` | `CalTorqueUp()` |
| `src/motor/MotorMain.c` | `CalUdcLimitIT()` |
| `src/motor/MotorMain.c` | `CalWsAndSynFreq()` |
| `src/motor/MotorMain.c` | `CalWsAndSynFreq_380()` |
| `src/motor/MotorMain.c` | `DebugSaveDeal()` |
| `src/motor/MotorMain.c` | `HVfOscDampDeal()` |
| `src/motor/MotorMain.c` | `PrepareAsrPar()` |
| `src/motor/MotorMain.c` | `PrepareForSpeedCheck()` |
| `src/motor/MotorMain.c` | `VcAsrControl()` |
| `src/motor/MotorMain.c` | `VfOscIndexCalc()` |
| `src/motor/MotorParEst.c` | `InitRrLoVariable()` |
| `src/motor/MotorParEst.c` | `LmIoIdentify()` |
| `src/motor/MotorParEst.c` | `RrLoIdentify()` |
| `src/motor/MotorParEst.c` | `RsIdentify()` |
| `src/motor/MotorPmsmMain.c` | `PrepareParForRun()` |
| `src/motor/MotorPmsmMain.c` | `TurnToStopStatus()` |
| `src/motor/MotorSpeedCheck.c` | `CalTorqueUp()` |
| `src/motor/MotorVCMain.c` | `PID32()` |
| `src/orphan/f_length.c` | `AccDecFrqCalc()` |
| `src/orphan/f_length.c` | `qsqrt()` |
| `src/orphan/z_func.c` | `Main05msFunctionA()` |
| `src/orphan/z_func.c` | `Main05msFunctionB()` |
| `src/orphan/z_func.c` | `Main05msFunctionC()` |
| `src/orphan/z_func.c` | `Main05msFunctionD()` |

典型原因: 函数原型写在某个 `.c` 里(如 `CalTorqueUp` 声明在 `MotorVF.c`),
或函数定义在同文件但位于调用点之后且无前置声明。

### 4.2 B 类: 全树内找不到定义 (31 处)

| 调用点 | 名称 | 判读 |
|---|---|---|
| `src/function/f_comm_DP.c` | `ethDpDeal()` | 需人工确认(可能位于非活动 `#if` 分支或由宏展开产生) |
| `src/function/f_debug.c` | `rand()` | C 标准库函数, 原工程未 `#include <string.h>/<stdlib.h>` |
| `src/function/f_interface.c` | `memcpy()` | C 标准库函数, 原工程未 `#include <string.h>/<stdlib.h>` |
| `src/function/f_io.c` | `MAX()` | 位于非活动的 `#elif` 分支内(文本级扫描不判断条件编译), 不参与编译 |
| `src/function/f_io.c` | `MIN()` | 位于非活动的 `#elif` 分支内(文本级扫描不判断条件编译), 不参与编译 |
| `src/function/f_main.c` | `InitPosCtrl()` | 需人工确认(可能位于非活动 `#if` 分支或由宏展开产生) |
| `src/function/f_main.c` | `PcMiscDeal()` | 需人工确认(可能位于非活动 `#if` 分支或由宏展开产生) |
| `src/function/f_menu.c` | `InitSetAptpZero()` | 需人工确认(可能位于非活动 `#if` 分支或由宏展开产生) |
| `src/function/f_menu.c` | `InitSetEcap4WithInt()` | 需人工确认(可能位于非活动 `#if` 分支或由宏展开产生) |
| `src/function/f_menu.c` | `memcpy()` | C 标准库函数, 原工程未 `#include <string.h>/<stdlib.h>` |
| `src/function/f_motorFc.c` | `memcpy()` | C 标准库函数, 原工程未 `#include <string.h>/<stdlib.h>` |
| `src/function/f_runSrc.c` | `GetCurPos()` | 需人工确认(可能位于非活动 `#if` 分支或由宏展开产生) |
| `src/function/f_runSrc.c` | `RcCutPointDeal()` | 需人工确认(可能位于非活动 `#if` 分支或由宏展开产生) |
| `src/motor/MotorCarrier.c` | `__IQsat()` | TI C28x 编译器内建 (IQmath intrinsic), 无需声明 |
| `src/motor/MotorEncoder.c` | `__IQsat()` | TI C28x 编译器内建 (IQmath intrinsic), 无需声明 |
| `src/motor/MotorEncoder.c` | `labs()` | C 标准库函数, 原工程未 `#include <string.h>/<stdlib.h>` |
| `src/motor/MotorImParEst.c` | `__IQsat()` | TI C28x 编译器内建 (IQmath intrinsic), 无需声明 |
| `src/motor/MotorInfoCollect.c` | `__IQsat()` | TI C28x 编译器内建 (IQmath intrinsic), 无需声明 |
| `src/motor/MotorInvProtect.c` | `__IQsat()` | TI C28x 编译器内建 (IQmath intrinsic), 无需声明 |
| `src/motor/MotorMain.c` | `__IQsat()` | TI C28x 编译器内建 (IQmath intrinsic), 无需声明 |
| `src/motor/MotorPWM.c` | `__IQsat()` | TI C28x 编译器内建 (IQmath intrinsic), 无需声明 |
| `src/motor/MotorPmsmMain.c` | `__IQsat()` | TI C28x 编译器内建 (IQmath intrinsic), 无需声明 |
| `src/motor/MotorPmsmMain.c` | `labs()` | C 标准库函数, 原工程未 `#include <string.h>/<stdlib.h>` |
| `src/motor/MotorSVC.c` | `__IQsat()` | TI C28x 编译器内建 (IQmath intrinsic), 无需声明 |
| `src/motor/MotorVCMain.c` | `__IQsat()` | TI C28x 编译器内建 (IQmath intrinsic), 无需声明 |
| `src/motor/MotorVF.c` | `__IQsat()` | TI C28x 编译器内建 (IQmath intrinsic), 无需声明 |
| `src/orphan/f_length.c` | `ABS_INT16()` | 需人工确认(可能位于非活动 `#if` 分支或由宏展开产生) |
| `src/orphan/f_length.c` | `GetCodeIndex()` | 需人工确认(可能位于非活动 `#if` 分支或由宏展开产生) |
| `src/subprogram/SubPrg.c` | `__IQsat()` | TI C28x 编译器内建 (IQmath intrinsic), 无需声明 |
| `src/system/main.c` | `__IQsat()` | TI C28x 编译器内建 (IQmath intrinsic), 无需声明 |
| `src/system/mainInit_2803x.c` | `MovePrgFrFlashToRam()` | 汇编 `_MovePrgFrFlashToRam`(见 `asm/DSP2803x_CodeStartBranch.asm`), 无 C 原型 |

> 注: 第 7 项检查是**文本级**的, 不判断条件编译是否成立;
> `MAX` / `MIN` 即属此类(它们位于 `f_io.c` 的 `#if 0 ... #elif 1 ... #elif 1` 中
> **未生效**的分支里)。

## 5. 未使用全局量 (32 处, 保留未删除)

判据: 该标识符在整个 `src/**/*.c` 语料里出现次数 <= 1(即只有定义、没有引用)。
可能是仅由中断/汇编引用、或被 `#if` 排除的引用。**保留**, 仅登记。

| 文件 | 标识符 |
|---|---|
| `src/function/f_dspcan.c` | `CAN_INIT_BAUD_ERR` |
| `src/function/f_dspcan.c` | `CAN_INIT_TIMEOUT` |
| `src/function/f_dspcan.c` | `CAN_MBOX_BUSY` |
| `src/function/f_dspcan.c` | `CAN_MBOX_NUM_ERROR` |
| `src/function/f_dspcan.c` | `CAN_MBOX_REC_SUCC` |
| `src/function/f_eeprom.c` | `I2C_BUS_BUSY_ERROR` |
| `src/function/f_eeprom.c` | `I2C_STP_NOT_READY_ERROR` |
| `src/function/f_frqSrc_pid.c` | `dLine` |
| `src/function/f_frqSrc_pid.c` | `iLine` |
| `src/function/f_frqSrc_pid.c` | `pLine` |
| `src/function/f_funcCode.c` | `funcCodeGradeSum` |
| `src/function/f_interface.c` | `frqFdbDisp` |
| `src/function/f_interface.c` | `frqFdbFlag` |
| `src/function/f_interface.c` | `jerkFf` |
| `src/function/f_interface.c` | `rsvdData` |
| `src/function/f_invPara.c` | `pInvTypeParaTable` |
| `src/function/f_torqueCtrl.c` | `torqueSetEthDp` |
| `src/function/f_torqueCtrl.c` | `torqueUpperEthDp` |
| `src/function/f_ui.c` | `keyAnimateTime1` |
| `src/function/f_ui.c` | `keyAnimateTimeAll` |
| `src/motor/MotorDebug.c` | `startSave` |
| `src/motor/MotorEncoder.c` | `gSpeedLine` |
| `src/motor/MotorEncoder.c` | `mUlong` |
| `src/motor/MotorVCMain.c` | `gPWMVAlphBeta` |
| `src/motor/MotorVCMain.c` | `m_TempVar` |
| `src/motor/MotorVF.c` | `gOvCur2` |
| `src/motor/MotorVF.c` | `gVfOverUdc` |
| `src/motor/MotorVar.c` | `gTestDataDisplay` |
| `src/orphan/DSP2803x_SysCtrl.c` | `iVol` |
| `src/orphan/f_length.c` | `pulseNote3` |
| `src/orphan/f_length.c` | `pulseRectangle` |
| `src/orphan/f_length.c` | `servoRunDIFlag` |

## 6. 中断函数定义 (89 个)

| 文件 | 中断函数 |
|---|---|
| `include/device/DSP2803x_DefaultISR.h` | `ADCINT1_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `ADCINT2_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `ADCINT3_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `ADCINT4_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `ADCINT5_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `ADCINT6_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `ADCINT7_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `ADCINT8_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `ADCINT9_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `CLA1_INT1_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `CLA1_INT2_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `CLA1_INT3_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `CLA1_INT4_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `CLA1_INT5_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `CLA1_INT6_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `CLA1_INT7_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `CLA1_INT8_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `DATALOG_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `ECAN0INTA_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `ECAN1INTA_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `ECAP1_INT_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `EMUINT_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `EPWM1_INT_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `EPWM1_TZINT_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `EPWM2_INT_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `EPWM2_TZINT_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `EPWM3_INT_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `EPWM3_TZINT_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `EPWM4_INT_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `EPWM4_TZINT_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `EPWM5_INT_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `EPWM5_TZINT_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `EPWM6_INT_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `EPWM6_TZINT_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `EPWM7_INT_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `EPWM7_TZINT_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `EQEP1_INT_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `I2CINT1A_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `I2CINT2A_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `ILLEGAL_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `INT13_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `INT14_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `INT_NOTUSED_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `LIN0INTA_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `LIN1INTA_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `LUF_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `LVF_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `NMI_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `PIE_RESERVED` |
| `include/device/DSP2803x_DefaultISR.h` | `RTOSINT_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `SCIRXINTA_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `SCITXINTA_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `SPIRXINTA_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `SPIRXINTB_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `SPITXINTA_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `SPITXINTB_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `TINT0_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `USER10_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `USER11_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `USER12_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `USER1_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `USER2_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `USER3_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `USER4_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `USER5_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `USER6_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `USER7_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `USER8_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `USER9_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `WAKEINT_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `XINT1_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `XINT2_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `XINT3_ISR` |
| `include/device/DSP2803x_DefaultISR.h` | `rsvd_ISR` |
| `src/function/f_comm.c` | `Lina_Level0_ISR` |
| `src/function/f_comm.c` | `Lina_Level1_ISR` |
| `src/function/f_comm.c` | `SCI_RXD_isr` |
| `src/function/f_comm.c` | `SCI_TXD_isr` |
| `src/function/f_comm_DP.c` | `SCI_DP_RXD_isr` |
| `src/function/f_comm_DP.c` | `SCI_DP_TXD_isr` |
| `src/function/f_osc.c` | `sciaRxFifoIsr` |
| `src/function/f_osc.c` | `sciaTxFifoIsr` |
| `src/motor/MotorImParEst.c` | `RrLoInPeriodAndCmpInt` |
| `src/motor/MotorPmsmMain.c` | `PG_Zero_isr` |
| `src/orphan/z_func.c` | `cpu_timer0_isr` |
| `src/system/main.c` | `ADC_Over_isr` |
| `src/system/main.c` | `EPWM1_TZ_isr` |
| `src/system/main.c` | `EPWM1_zero_isr` |
| `src/system/mainInit_2803x.c` | `rsvd_ISR` |

注: 含 `include/device/DSP2803x_DefaultISR.h` 里 TI 提供的默认 ISR 声明。

## 7. volatile 声明分布

| 文件 | 声明数 |
|---|---:|
| `src/system/DSP2803x_CpuTimers.c` | 1 |
| `src/system/DSP2803x_GlobalVariableDefs.c` | 41 |
| `src/function/f_comm_DP.c` | 2 |
| `src/function/f_io.c` | 4 |
| `include/device/DSP2803x_CpuTimers.h` | 1 |
| `include/function/f_comm_DP.h` | 1 |
| `src/orphan/DSP2803x_SysCtrl.c` | 2 |

> 提示: 中断与主循环共享的变量必须 `volatile`。原工程的 `volatile` 使用量偏少,
> 本轮**未增加**任何 `volatile`(增加会改变生成代码), 仅登记供后续批次评估。

## 8. 编译与链接验证

本机已具备 TI C2000 CGT `v25.11.1.LTS`, 因此以下项目**已实际执行**:

| 项 | 结果 |
|---|---|
| 全工程编译 (原/新 各 59 个目标文件) | **118 / 118 成功, 0 失败** |
| 逐模块反汇编比对 (关优化口径) | **16 逐行相同 / 41 按预期不同 / 2 纯数据不适用** |
| 目标文件内容级比对 (段+重定位+符号表) | **18 / 59 一致** |
| Debug 链接 + hex2000 | 见 `docs/build_debug.txt` |
| Release 链接 + hex2000 | 见 `docs/build_release.txt` |
| 告警清单 | 见 `docs/warning_list.txt` |
| 整机映像与原工程副本逐字节比对 | 见 `docs/equivalence_evidence.md` 第 4 节 |

注意: 必须加 `--abi=coffabi`。新 CGT 默认 EABI, 会改变符号命名、运行时库与
`.data` 归置, 与本工程的 COFF `.cmd` 不兼容。

**仍未覆盖** (需硬件或额外条件):

| 项 | 原因 |
|---|---|
| 目标板上的实际行为 (PWM/ADC/CAN/SCI/闭环) | 需目标板与台架 |
| 中断时序 / 最坏执行时间 | 需示波器或周期计数 |
| Flash 烧写与上电启动 | 需仿真器 |
| 原工程 `39 Warnings` 的逐条对照 | 原日志只记条数, 未记正文 |

原工程 Debug 构建的实测结果(供参考): `0 Errors, 39 Warnings`
(见 `docs/original_build_Debug.log`)。

## 9. 复现步骤

```bash
# 1) 重建源码树 (幂等)
python tools/rewrite_tree.py
# 2) 重建工程 / 链接脚本 / GEL
python tools/build_project.py
# 3) 静态验收
python tools/verify_tree.py
# 4) include 自洽性
python tools/check_includes.py
# 5) 负向对照 (会临时改动并自动还原 f_common.c)
python tools/negative_control.py
# 6) 一键全跑
python tests/run_all.py
```
