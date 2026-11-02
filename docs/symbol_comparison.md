# 原工程 / 新工程 关键符号对比 (symbol_comparison)

## 1. 两层证明口径

交付源码共 145 个文件, 分两层验收 —— **两层用的是两条不同的证据链**,
不能混着读:

- **未改写层 (94 个)**: 只去掉了注释, 做**词法级 token 流比对**。把注释与空白全部忽略后, 原文件与新文件的 token 序列逐项比较; token 流完全相同=> 函数名、参数、返回值、全局变量、结构体布局、宏、字符串字面量、数值常量、运算顺序、移位位数全部未变。
- **改写层 (51 个)**: 实现被**有意重写**, token 流本来就应该不同, 因此**豁免** token 等价 (见 `docs/rewritten_modules.json`)。它们的等价性由**宿主差分测试 + 目标端副作用检查**承担 —— 差分测试覆盖状态空间并做负向对照,目标端比对符号集合、告警与段尺寸 (见 `docs/_difftest/`、`docs/equivalence_evidence.md`)。

> 读下表时注意: "token 流不一致" 只统计**未改写层**。改写层既不计入"不一致",
> 也不计入"一致" —— 它的结论来自另一条证据链, 不在这张表里。

| 项目 | 结果 |
|---|---|
| 交付源码文件数 | 145 |
| 其中 未改写层 (要求 token 等价) | 94 |
| 其中 改写层 (豁免 token 等价, 另证) | 51 |
| 未改写层 token 流不一致 | 0 |
| 符号抽取不一致 | 0 |
| 注释残留 | 0 |
| 非 ASCII 残留 | 0 |
| TODO/FIXME/NOTE | 0 |
| 二次去注释可删字符 | 0 |

## 2. 关键对比项结论

| 对比项 | 未改写层 | 改写层 |
|---|---|---|
| 函数名称 / 参数 / 返回值 | 未变 (token 流等价) | 未变 (名字与签名逐字保留) |
| 全局变量与定义位置 | 未变 (逐文件 token 等价) | 未变 (同一符号, 同一落点) |
| 结构体 / 位域 / union 布局 | 未变 (逐 token 相同) | 未变 (声明未动) |
| 宏与常量 | 未变 | 未变 |
| 中断入口与 PIE 向量赋值 | 未变 | 未变 (ISR 名与向量赋值未动) |
| 参数地址 / 通信地址 / 错误码 | 未变 (均为字面量) | 未变 (均为字面量) |
| 任务周期 (0.5ms/2ms/0ms) | 未变 | 未变 |
| 条件编译分支 | 原样保留, 未解析 | 原样保留, 未解析 |
| **函数体实现语句** | 逐 token 相同 | **有意重写** (差分测试证明等价) |

### 改写层逐模块状态

| 模块 | token 流是否恰好相同 |
|---|---|
| `src/system/DSP2803x_CpuTimers.c` | 否 (实现已重写, 属预期) |
| `src/function/f_canlink.c` | 否 (实现已重写, 属预期) |
| `src/function/f_comm.c` | 否 (实现已重写, 属预期) |
| `src/function/f_comm_DP.c` | 否 (实现已重写, 属预期) |
| `src/function/f_common.c` | 否 (实现已重写, 属预期) |
| `src/function/f_debug.c` | 否 (实现已重写, 属预期) |
| `src/function/f_dspcan.c` | 否 (实现已重写, 属预期) |
| `src/function/f_eeprom.c` | 否 (实现已重写, 属预期) |
| `src/function/f_error.c` | 否 (实现已重写, 属预期) |
| `src/function/f_fcDeal.c` | 否 (实现已重写, 属预期) |
| `src/function/f_frqSrc.c` | 否 (实现已重写, 属预期) |
| `src/function/f_frqSrc_pid.c` | 否 (实现已重写, 属预期) |
| `src/function/f_interface.c` | 否 (实现已重写, 属预期) |
| `src/function/f_invPara.c` | 否 (实现已重写, 属预期) |
| `src/function/f_io.c` | 否 (实现已重写, 属预期) |
| `src/function/f_main.c` | 否 (实现已重写, 属预期) |
| `src/function/f_menu.c` | 否 (实现已重写, 属预期) |
| `src/function/f_modbus.c` | 否 (实现已重写, 属预期) |
| `src/function/f_motorFc.c` | 否 (实现已重写, 属预期) |
| `src/function/f_osc.c` | 否 (实现已重写, 属预期) |
| `src/function/f_p2p.c` | 否 (实现已重写, 属预期) |
| `src/function/f_plc.c` | 否 (实现已重写, 属预期) |
| `src/function/f_profibus.c` | 否 (实现已重写, 属预期) |
| `src/function/f_runSrc.c` | 否 (实现已重写, 属预期) |
| `src/function/f_runSrc_accDecFrq.c` | 否 (实现已重写, 属预期) |
| `src/function/f_torqueCtrl.c` | 否 (实现已重写, 属预期) |
| `src/function/f_ui.c` | 否 (实现已重写, 属预期) |
| `src/function/f_vf.c` | 否 (实现已重写, 属预期) |
| `src/system/main.c` | 否 (实现已重写, 属预期) |
| `src/motor/MotorCarrier.c` | 否 (实现已重写, 属预期) |
| `src/motor/MotorCurrentTransform.c` | 否 (实现已重写, 属预期) |
| `src/motor/MotorDataExchange.c` | 否 (实现已重写, 属预期) |
| `src/motor/MotorDebug.c` | 否 (实现已重写, 属预期) |
| `src/motor/MotorEncoder.c` | 否 (实现已重写, 属预期) |
| `src/motor/MotorImParEst.c` | 否 (实现已重写, 属预期) |
| `src/motor/MotorInfoCollect.c` | 否 (实现已重写, 属预期) |
| `src/motor/MotorInvProtect.c` | 否 (实现已重写, 属预期) |
| `src/motor/MotorMain.c` | 否 (实现已重写, 属预期) |
| `src/motor/MotorParChange.c` | 否 (实现已重写, 属预期) |
| `src/motor/MotorParEst.c` | 否 (实现已重写, 属预期) |
| `src/motor/MotorPmsmMain.c` | 否 (实现已重写, 属预期) |
| `src/motor/MotorPmsmParEst.c` | 否 (实现已重写, 属预期) |
| `src/motor/MotorPublicCal.c` | 否 (实现已重写, 属预期) |
| `src/motor/MotorPWM.c` | 否 (实现已重写, 属预期) |
| `src/motor/MotorSpeedCheck.c` | 否 (实现已重写, 属预期) |
| `src/motor/MotorSVC.c` | 否 (实现已重写, 属预期) |
| `src/motor/MotorVCMain.c` | 否 (实现已重写, 属预期) |
| `src/motor/MotorVF.c` | 否 (实现已重写, 属预期) |
| `src/motor/ZeroPosCtl.c` | 否 (实现已重写, 属预期) |
| `src/subprogram/SubPrg.c` | 否 (实现已重写, 属预期) |
| `src/system/mainInit_2803x.c` | 否 (实现已重写, 属预期) |

说明: 改写层里若出现 `token_same = 是`, 说明该轮改写是**空转** (没真正改到实现),
应当暴露出来而不是当成通过 —— 这正是 `tests/test_token_equivalence.py` 盯着的一点。

## 3. 中断入口 (新树)

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

## 4. volatile 声明分布

| 文件 | 数量 |
|---|---:|
| `src/system/DSP2803x_CpuTimers.c` | 1 |
| `src/system/DSP2803x_GlobalVariableDefs.c` | 41 |
| `src/function/f_comm_DP.c` | 2 |
| `src/function/f_io.c` | 2 |
| `include/device/DSP2803x_CpuTimers.h` | 1 |
| `include/function/f_comm_DP.h` | 1 |
| `src/orphan/DSP2803x_SysCtrl.c` | 2 |
