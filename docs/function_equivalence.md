# 逐函数功能比对: 改写版 vs 原工程

> 生成脚本 `build/_func_equiv.py` + `build/_func_equiv_report.py`; 原始数据 `docs/function_equivalence.json`。
> 比对对象是**目标文件**, 不是源码文本 —— 源码怎么排版不影响结论。

## 0. 结论

**函数功能一致, 没有丢函数, 也没有多出对外函数。**

| 检查项 | 结果 |
|---|---|
| 构建单元 | **59** 个 (工程 `.lkf` 里的全部 C/汇编编译单元) |
| 函数总数 (原工程) | **582** |
| 函数集合 | **一个不少、一个不多** —— 外部符号 (`C_EXT` + `.text`) 在 **59/59** 个单元里完全相同 |
| 逐指令完全相同 | **245** 个 (42%) |
| 指令不同 (已改写) | **337** 个 (58%) |
| 改写函数的验证覆盖 | **337/337** 从差分测试入口可达, 覆盖缺口 **0** |
| 新增 `static` 辅助函数 | **65** 个, 全部是 `C_LABEL` (无外部链接), 不进对外符号表 |
| 函数级变异灵敏度 | **331/337** 个改写函数至少被 1 条变异打到; 6 个没有 (见第 5 节) |
| 差分测试 | **51 个用例 / 25,892,604 例 / 0 不一致** |
| 负向对照 | **6339 条变异全部判 CAUGHT**; 其中 6263 条是干净的不一致 (`rc=1`), 76 条进程崩在半路 (逐条清单见 `docs/_negctl_rc_audit.md`) |

"逐指令相同"和"已改写"是两类都合格的结果: 前者是**最强**的等价证据(一个字节都没动), 后者的证据是差分测试 + 负向对照。

## 1. 判定口径

三层, 一层比一层强; 只有三层都成立的函数才算"已验证"。

| 层 | 判据 | 为什么是这个判据 |
|---|---|---|
| **L1 契约层** | 两个目标文件的**外部符号集合**完全相同 (`C_EXT` 且定义在 `.text`) | 外部符号就是链接器看到的一切。少一个 = 调用方链接失败或功能丢失; 多一个 = 改了对外契约 |
| **L2 实现层** | 每个函数按**指令字**逐条比 (操作码 + 操作数文本) | 比源码文本没意义 (排版/变量名都变了); 比指令才是比"机器实际做什么" |
| **L3 行为层** | 指令不同的函数, 必须能从差分测试的某个入口**可达** | 改了实现就必须有行为证据; 不可达的改写等于没验证 |

L3 的"可达"是沿**调用图**算的, 不是按名字猜的:

* 同段内调用: 反汇编里的 `LCR 0xADDR` 直接对应函数起始地址;
* 跨段调用: `.text:retain` 里的中断服务程序调 `.text` 里的函数, 操作数是重定位处的**就地加数**, 段名从该指令地址的重定位项里取;
* 间接调用: 代码里 `MOVL XARn, #func` 把地址装进寄存器 (随后写进 `PieVectTable` 或函数指针表) —— 这条边静态调用图上没有, 靠"该指令地址上有指向 `.text*` 段的重定位, 而指令本身不是 `LCR`/`B`"识别;
* 驱动侧: 用例的 `K_xxx` 入口编号和 `new_xxx` 函数指针表项都算种子。

## 2. 逐模块总表

| 构建单元 | 函数 | 逐指令相同 | 已改写 | 外部符号 | 差分例数 | 不一致 |
|---|---:|---:|---:|---|---:|---:|
| `ASMSubPrg` | 6 | 6 | 0 | 一致 | — | — |
| `DSP2803x_CSMPasswords` | 0 | 0 | 0 | 一致 | — | — |
| `DSP2803x_CodeStartBranch` | 3 | 3 | 0 | 一致 | — | — |
| `DSP2803x_CpuTimers` | 2 | 0 | 2 | 一致 | 20312 | 0 |
| `DSP2803x_GlobalVariableDefs` | 0 | 0 | 0 | 一致 | — | — |
| `DSP2803x_usDelay` | 1 | 1 | 0 | 一致 | — | — |
| `MotorCarrier` | 6 | 1 | 5 | 一致 | 32441 | 0 |
| `MotorConstant` | 0 | 0 | 0 | 一致 | — | — |
| `MotorCurrentTransform` | 4 | 0 | 4 | 一致 | 70932 | 0 |
| `MotorDataExchange` | 5 | 3 | 2 | 一致 | 25233 | 0 |
| `MotorDebug` | 4 | 2 | 2 | 一致 | 758330 | 0 |
| `MotorEncoder` | 16 | 8 | 8 | 一致 | 76063 | 0 |
| `MotorImParEst` | 15 | 8 | 7 | 一致 | 24299 | 0 |
| `MotorInfoCollect` | 3 | 2 | 1 | 一致 | 515204 | 0 |
| `MotorInvProtect` | 14 | 0 | 14 | 一致 | 61694 | 0 |
| `MotorMain` | 15 | 5 | 10 | 一致 | 22517 | 0 |
| `MotorPWM` | 5 | 0 | 5 | 一致 | 217585 | 0 |
| `MotorParChange` | 5 | 0 | 5 | 一致 | 20207 | 0 |
| `MotorParEst` | 3 | 0 | 3 | 一致 | 20786 | 0 |
| `MotorPmsmMain` | 10 | 5 | 5 | 一致 | 113392 | 0 |
| `MotorPmsmParEst` | 13 | 1 | 12 | 一致 | 20145 | 0 |
| `MotorPublicCal` | 8 | 4 | 4 | 一致 | 217924 | 0 |
| `MotorSVC` | 4 | 0 | 4 | 一致 | 502 | 0 |
| `MotorSpeedCheck` | 6 | 3 | 3 | 一致 | 33909 | 0 |
| `MotorVCMain` | 16 | 4 | 12 | 一致 | 86945 | 0 |
| `MotorVF` | 16 | 3 | 13 | 一致 | 32503 | 0 |
| `MotorVar` | 0 | 0 | 0 | 一致 | — | — |
| `SubPrg` | 6 | 1 | 5 | 一致 | 517311 | 0 |
| `ZeroPosCtl` | 3 | 0 | 3 | 一致 | 243472 | 0 |
| `f_canlink` | 12 | 8 | 4 | 一致 | 20175 | 0 |
| `f_comm` | 23 | 6 | 17 | 一致 | 24766 | 0 |
| `f_comm_DP` | 14 | 4 | 10 | 一致 | 20991 | 0 |
| `f_common` | 3 | 0 | 3 | 一致 | 17039736 | 0 |
| `f_debug` | 9 | 7 | 2 | 一致 | 150226 | 0 |
| `f_dspcan` | 5 | 0 | 5 | 一致 | 29356 | 0 |
| `f_eeprom` | 16 | 10 | 6 | 一致 | 103745 | 0 |
| `f_error` | 13 | 2 | 11 | 一致 | 33762 | 0 |
| `f_fcDeal` | 6 | 2 | 4 | 一致 | 38923 | 0 |
| `f_frqSrc` | 17 | 13 | 4 | 一致 | 47949 | 0 |
| `f_frqSrc_pid` | 4 | 2 | 2 | 一致 | 21411 | 0 |
| `f_funcCode` | 0 | 0 | 0 | 一致 | — | — |
| `f_interface` | 10 | 10 | 0 | 一致 | 205651 | 0 |
| `f_invPara` | 8 | 1 | 7 | 一致 | 68300 | 0 |
| `f_io` | 74 | 40 | 34 | 一致 | 56213 | 0 |
| `f_main` | 18 | 15 | 3 | 一致 | 77173 | 0 |
| `f_menu` | 70 | 31 | 39 | 一致 | 107013 | 0 |
| `f_modbus` | 5 | 0 | 5 | 一致 | 414979 | 0 |
| `f_motorFc` | 1 | 0 | 1 | 一致 | 260000 | 0 |
| `f_osc` | 15 | 1 | 14 | 一致 | 29123 | 0 |
| `f_p2p` | 1 | 0 | 1 | 一致 | 2541160 | 0 |
| `f_plc` | 4 | 0 | 4 | 一致 | 325966 | 0 |
| `f_profibus` | 7 | 6 | 1 | 一致 | 118120 | 0 |
| `f_runSrc` | 19 | 10 | 9 | 一致 | 49144 | 0 |
| `f_runSrc_accDecFrq` | 4 | 0 | 4 | 一致 | 539530 | 0 |
| `f_torqueCtrl` | 4 | 1 | 3 | 一致 | 76663 | 0 |
| `f_ui` | 10 | 6 | 4 | 一致 | 84331 | 0 |
| `f_vf` | 1 | 1 | 0 | 一致 | 56089 | 0 |
| `main` | 4 | 3 | 1 | 一致 | 200120 | 0 |
| `mainInit_2803x` | 16 | 6 | 10 | 一致 | 20283 | 0 |
| **合计** | **582** | **245** | **337** | **59/59 一致** | **25,892,604** | **0** |

## 3. 逐函数明细

`docs/function_equivalence.json` 里每个函数一条记录:

```json
{"f": "_DiCalc", "state": "changed", "n_o": 210, "n_n": 205,
 "ext": true, "cov": "direct"}
```

* `state`: `same` = 逐指令相同; `changed` = 已改写; `ONLY-NEW` = 新增的内部辅助函数
* `n_o` / `n_n`: 原工程 / 改写版的指令条数
* `ext`: 是否属于对外符号
* `cov`: `identical` / `direct` (直接可达) / `table` (经函数指针或中断向量间接)

## 4. 新增的 65 个 `static` 辅助函数

改写时把重复片段抽成了内部函数。它们**不是新功能**:

* 全部是 `static` —— 在目标文件里是 `C_LABEL`, 不在 `C_EXT` 符号表里, 链接器与调用方都看不到;
* 每个都被至少一个"已改写且已验证"的函数调用 (见上面的调用图), 不存在只定义不调用的死函数;
* 不引入新的全局状态 —— 参数全在寄存器/栈上, 副作用只有原函数本来就有的那些。

| 构建单元 | 新增内部函数 |
|---|---|
| `DSP2803x_CpuTimers` | `_InitOneTimer` |
| `MotorEncoder` | `_UvwLogicLevel` |
| `MotorImParEst` | `_RrLoWaitTick` |
| `MotorPWM` | `_DeadBandCompOne`, `_OutputOnePhase`, `_SendOneChannel`, `_SetPhaseLength`, `_SetPwmTimerBase`, `_SetPwmTripZone` |
| `MotorParChange` | `_s_DegScale`, `_s_FreqToReal`, `_s_IndScale`, `_s_PosScale`, `_s_ResScale` |
| `MotorPmsmMain` | `_FluxWeakIdClamp`, `_QepTotalWrap`, `_ZeroErrCount` |
| `MotorPmsmParEst` | `_QepCountFromPos` |
| `MotorVCMain` | `_CalAsrKi`, `_ClearVoltWhenNotRun`, `_LimitVoltAmp`, `_PrepareTorqueLimitPid`, `_PrepareUdcPid`, `_ResetAsrTotals`, `_ResetVoltOut`, `_SetVoltPhase`, `_SnapshotImTs`, `_VcLowFreqHold` |
| `f_comm` | `_commSendPrepare`, `_commSetAo`, `_setLinParity` |
| `f_comm_DP` | `_CompleteDpFrame`, `_ResetDpChannel`, `_StoreCrc` |
| `f_error` | `_ClassifySpeedFault`, `_ConsiderError`, `_GetActionLevel`, `_RollbackError`, `_SaveErrorScene` |
| `f_fcDeal` | `_GetCalibrateCurveInit`, `_IsAccDecTimeCode`, `_IsCalibrateCurveCode`, `_IsInitExceptSeries`, `_IsInitExceptSingle`, `_LimitOneCode` |
| `f_io` | `_ioDoFuncFlag`, `_ioErrorOutputActive`, `_ioFrqFdtArrive`, `_ioHdoCompareTrack`, `_ioPulseSourceConflict`, `_ioSetOrClear` |
| `f_menu` | `_HandleQuickMenuSwitch`, `_SetWinkFlag` |
| `f_profibus` | `_DpLatchEepromIndex` |
| `f_runSrc_accDecFrq` | `_s_AccDecIsAcc`, `_s_AccDecUseRun`, `_s_BenchOverride`, `_s_CurveDowngrade`, `_s_Fit18`, `_s_Q15Rescale`, `_s_SbClamp`, `_s_SbLine`, `_s_SbStep`, `_s_TickDown`, `_s_TickUp` |

## 5. 函数级变异灵敏度

差分测试的 "0 不一致" 只证明 **ref 与 new 跑出了同样的结果**, 不证明"这段代码出错会被发现"。后者靠负向对照: 把 ref 的某一行改错, 看差分测试能不能抓出来。所以"每个函数至少有一条变异"是个独立的证据维度 —— 一条变异都没有的函数, 在差分测试里"通过"的说服力要弱得多。

把 6339 条变异按锚点在**冻结基准**里的位置归到函数上 (`build/_func_mutmap.py`, 定位规则与 `negative_control.py` 完全一致: 从 ref 文件里 `find` 第一处命中):

| 构建单元 | 改写函数 | 有变异 | 无变异 |
|---|---:|---:|---:|
| `DSP2803x_CpuTimers` | 2 | 2 | 0 |
| `MotorCarrier` | 5 | 5 | 0 |
| `MotorCurrentTransform` | 4 | 4 | 0 |
| `MotorDataExchange` | 2 | 2 | 0 |
| `MotorDebug` | 2 | 2 | 0 |
| `MotorEncoder` | 8 | 8 | 0 |
| `MotorImParEst` | 7 | 7 | 0 |
| `MotorInfoCollect` | 1 | 1 | 0 |
| `MotorInvProtect` | 14 | 13 | 1 |
| `MotorMain` | 10 | 10 | 0 |
| `MotorPWM` | 5 | 5 | 0 |
| `MotorParChange` | 5 | 5 | 0 |
| `MotorParEst` | 3 | 3 | 0 |
| `MotorPmsmMain` | 5 | 5 | 0 |
| `MotorPmsmParEst` | 12 | 12 | 0 |
| `MotorPublicCal` | 4 | 4 | 0 |
| `MotorSVC` | 4 | 4 | 0 |
| `MotorSpeedCheck` | 3 | 3 | 0 |
| `MotorVCMain` | 12 | 12 | 0 |
| `MotorVF` | 13 | 13 | 0 |
| `SubPrg` | 5 | 5 | 0 |
| `ZeroPosCtl` | 3 | 2 | 1 |
| `f_canlink` | 4 | 4 | 0 |
| `f_comm` | 17 | 15 | 2 |
| `f_comm_DP` | 10 | 10 | 0 |
| `f_common` | 3 | 3 | 0 |
| `f_debug` | 2 | 1 | 1 |
| `f_dspcan` | 5 | 5 | 0 |
| `f_eeprom` | 6 | 6 | 0 |
| `f_error` | 11 | 11 | 0 |
| `f_fcDeal` | 4 | 4 | 0 |
| `f_frqSrc` | 4 | 4 | 0 |
| `f_frqSrc_pid` | 2 | 2 | 0 |
| `f_invPara` | 7 | 7 | 0 |
| `f_io` | 34 | 34 | 0 |
| `f_main` | 3 | 3 | 0 |
| `f_menu` | 39 | 39 | 0 |
| `f_modbus` | 5 | 5 | 0 |
| `f_motorFc` | 1 | 1 | 0 |
| `f_osc` | 14 | 14 | 0 |
| `f_p2p` | 1 | 1 | 0 |
| `f_plc` | 4 | 4 | 0 |
| `f_profibus` | 1 | 1 | 0 |
| `f_runSrc` | 9 | 9 | 0 |
| `f_runSrc_accDecFrq` | 4 | 4 | 0 |
| `f_torqueCtrl` | 3 | 2 | 1 |
| `f_ui` | 4 | 4 | 0 |
| `main` | 1 | 1 | 0 |
| `mainInit_2803x` | 10 | 10 | 0 |
| **合计** | **337** | **331** | **6** |

**没有变异打到的 6 个改写函数** (如实列出, 不并入上面的"已证明"):

| 构建单元 | 函数 | 函数体是什么 |
|---|---|---|
| `MotorInvProtect` | `_OutputLoseReset` | 清标志位的复位函数 |
| `ZeroPosCtl` | `_GetPGPos` | 一行 `return ((*EQepRegs).QPOSCNT);` —— 只读一个寄存器 |
| `f_comm` | `_SCI_RXD_isr` | SCI 接收中断服务程序 |
| `f_comm` | `_SCI_TXD_isr` | SCI 发送中断服务程序 |
| `f_debug` | `_InitDebug` | 初始化: 置默认值, 无分支 |
| `f_torqueCtrl` | `_TorqueCalc` | 两行调用 (`UpdateTorqueAim()` + `AccDecTorqueCalc()`), 自身无运算 |

这 6 个都是**胶水函数**: 函数体只有调用、寄存器读或赋值, 没有可改的判据。它们的等价性证据只有差分测试的 "0 不一致" —— 而它们被调用的**上游**函数是有变异覆盖的, 上游一旦算错, 这些函数拿到的输入就会变, 输出自然跟着变。要给它们补变异, 得改变异集并重跑对应用例 (纪律: 改了变异集就不许 `--resume`)。

## 6. 没有差分用例的单元, 和纯数据单元

| 单元 | 函数 | 状态 | 说明 |
|---|---:|---|---|
| `ASMSubPrg` | 6 | 6 个逐指令相同 | 手写汇编, 未改写, 身份即证据 |
| `DSP2803x_CodeStartBranch` | 3 | 3 个逐指令相同 | 启动跳转, 未改写 |
| `DSP2803x_usDelay` | 1 | 1 个逐指令相同 | `ramfuncs` 段里的延时循环, 未改写 |

这三个单元的结论不依赖差分测试: 目标文件逐字节相同, 不需要"跑一遍看结果对不对"。

另有 **5 个纯数据单元**, 函数数为 0, 载荷全在 `.econst` / `.ebss` (标定表、变量定义、外设寄存器):

| 单元 | 内容 |
|---|---|
| `MotorConstant` | 电机常数表 |
| `MotorVar` | 全局变量定义 |
| `f_funcCode` | 参数规格表 `funcCodeAttribute[]` 等 |
| `DSP2803x_CSMPasswords` | CSM 密码区 |
| `DSP2803x_GlobalVariableDefs` | 外设寄存器结构体定义 |

这 5 个的等价性证据是 Tier B: 两个目标文件**逐字节相同**。

顺带一提: `f_interface` 与 `f_vf` 虽然**改写过了**, 目标文件也仍然**逐字节相同** —— 改写只动了源码表达, 没动编译器吐出来的东西。这是比"差分通过"更强的结果, 不是"没改够"。

## 7. 这套比对**不能**证明什么

把边界写清楚, 比多报几个"通过"有用:

1. **"逐指令相同"不等于"原代码正确"** —— 它证明的是"和原工程一样", 原工程本身的 bug 会原样保留。这是本项目的契约 ("功能不要变"), 不是缺陷。
2. **"可达"不等于"每条分支都跑到"** —— 差分用例的分支覆盖不是 100%。没跑到的分支靠**负向对照**兜底: 每一条变异都必须被差分测试抓出来, 抓不出来就说明那条路径没被驱动到。6339 条变异全部 CAUGHT, 说明变异所在的分支都被驱动过; 但"没有变异落在某条分支上"这件事, 本报告证明不了。
3. **宿主指令相同 ≠ 目标端时序相同** —— 宿主编译器是 MSVC, 目标端是 `cl2000`。目标端的证据是另一条链: `build_and_compare.py` 的 Tier A (反汇编) / Tier B (目标文件逐字节) + `target_check.py` (符号集合、`.text` 大小、告警)。
4. **硬件相关行为未验证** —— ADC 采样、EEPROM 写时序、CAN/SCI 收发、PWM 死区这些在宿主上是替身。差分测试证明的是"同样的输入产生同样的寄存器写入序列", 不是"真实器件会怎么响应"。
5. **优化口径** —— 上面的逐指令比对是**关优化** (`/Od` / `-o0`) 下的结论。工程实际用 `-o2`/`-o3`, 那一档由 `build_official.py` 单独编译验证, 允许生成不同的码 (表驱动改写会让优化器换策略), 行为等价性仍由差分测试保证。

## 8. 怎么复现

```bash
python -u build/_func_equiv.py          # 反汇编 + 符号表 + 调用图, 产出 json
python -u build/_func_mutmap.py         # 变异 -> 函数, 产出 json
python -u build/_func_equiv_report.py   # 渲染本文档
```

前置: `tools/build_and_compare.py` 跑过一次, `build/_verify/stage/obj/<单元>.{orig,new}/` 里有目标文件。
