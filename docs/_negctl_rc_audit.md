# 负向对照总账的 `rc` 复核

生成方式: `python build/_negctl_rc_audit.py` (读 `docs/_difftest/_negctl_summary.json` 与逐条现场 json)。

## 为什么要有这份清单

`negative_control.py` 的判定是 `elif rc != 0: verdict = "CAUGHT"` —— 任何非零退出码都算抓到。于是一份 `caught == total, pass: true` 的台账里, 可能混着**进程崩在半路**的条目: 它后面的相根本没跑, 却写着"抓到"。

审台账的第一件事是 **`rc` 直方图**, 不是看 verdict。

## 汇总

| 项 | 条数 |
|---|---|
| 总账记录 | 6339 |
| `rc == 1` 干净的不一致 | **6263** |
| `rc != 1` 崩在半路 / 不终止 | **76** |
| 其中 **崩之前一条 `MISMATCH` 都没打出来** (假 CAUGHT) | **18** |
| 其中已有字段级差异、只是覆盖被截断 | 58 |

`rc` 直方图 (全部 6339 条):

| rc | 含义 | 条数 |
|---|---|---|
| `1` | 干净的不一致 (跑完 + 至少一处字段级差异) | 6263 |
| `3221225620` | 0xC0000094 STATUS_INTEGER_DIVIDE_BY_ZERO | 45 |
| `3221225477` | 0xC0000005 STATUS_ACCESS_VIOLATION | 23 |
| `2` | 测试台自身的守卫主动中止 (打了 FATAL / SELFCHECK FAIL, 不是崩溃) | 3 |
| `3221225725` | 0xC00000FD STATUS_STACK_OVERFLOW | 2 |
| `-2` | run_case 约定: 变体不终止 (超时) | 2 |
| `3221226505` | 0xC0000409 STATUS_STACK_BUFFER_OVERRUN | 1 |

`rc == 2` 是测试台**自己**的守卫 (不是被测模块崩溃): `MotorSVC` 的 `SELFCHECK FAIL` 与 `f_menu` 的 `FATAL: GetFuncCodeInit 收到越界下标`。它们打了诊断信息才 `return 2`, 所以既不是静默崩溃, 也不能直接当"抓到"用 —— 要单独看它前面有没有 `MISMATCH`。

## 逐条清单 (`rc != 1`)

`MISMATCH` 列 = 崩之前已经打出的差异条数; `-1` = 现场 json 缺失。

| 用例 | 变异 | rc | MISMATCH | 崩在哪个相 | 备注 |
|---|---|---|---|---|---|
| `MotorPmsmMain` | `PM143` | `3221225620` | **0** | `prep` | 频率上限 32767 -> 32766 |
| `MotorPublicCal` | `P80` | `3221225620` | **0** | `-` | 制动占空判据 100 改 101 |
| `MotorPublicCal` | `P82` | `3221225620` | **0** | `-` | 制动占空判据 0 改 1 |
| `MotorPublicCal` | `P86` | `3221225620` | 18 | `-` | 占空换算分母 100 改 101 |
| `MotorSVC` | `MS110` | `3221225620` | **0** | `-` | 转速分母 FullFreq01 -> FcSetApply |
| `MotorSVC` | `MS123` | `3221225620` | **0** | `-` | 滑差除以 LM 而不是 L1 |
| `MotorSVC` | `MS142` | `3221225620` | 40 | `-` | FreqFeed 取 FreqSyn 而不是 SvcRotorSpeed |
| `MotorSVC` | `MS164` | `3221225620` | 40 | `-` | M>>12 改成 M>>13 (_380) |
| `MotorSVC` | `MS18` | `2` | 2 | `-` | ResetSVC: Theta = IMPhase>>16 右移 17 位 |
| `MotorSVC` | `MS19` | `2` | 2 | `-` | ResetSVC: Theta = IMPhase>>16 右移 15 位 |
| `MotorSVC` | `MS26` | `3221225620` | 35 | `-` | m_Timer 不再读硬件定时器 (置 0) |
| `SubPrg` | `AN1` | `3221225620` | **0** | `-` | atan 的 x==0 判据取反 |
| `f_comm_DP` | `DP31` | `3221225477` | 18 | `random` | 发送游标比较反向 |
| `f_debug` | `DBG27` | `3221225477` | 16 | `-` | 边界判据 < -> > |
| `f_eeprom` | `EP084` | `3221225477` | 20 | `-` | Deal 空闲判据取反 (== -> !=) |
| `f_eeprom` | `EP103` | `3221225477` | 20 | `-` | Deal number 计算方向反了 (index - tmp) |
| `f_eeprom` | `EP112` | `3221225477` | 20 | `-` | Deal 命中时落到准备写而不是读当前数据 |
| `f_eeprom` | `EP114` | `3221225477` | 20 | `-` | Deal 准备写态判据换成读当前数据态 |
| `f_invPara` | `I17` | `3221225477` | **0** | `-` | MotorPowerRelatedParaDeal 数据点格式首项 100 -> 101 |
| `f_invPara` | `I20` | `3221225477` | **0** | `-` | SaveTuneData 循环上界少 1 |
| `f_invPara` | `I21` | `3221225477` | **0** | `-` | SaveTuneData 特殊项位置错 3 -> 4 |
| `f_invPara` | `I22` | `3221225477` | **0** | `-` | SaveTuneData 索引整体偏移 1 |
| `f_invPara` | `I8` | `3221225477` | **0** | `-` | UpdateInvType 加减速起点取错表 |
| `f_main` | `FM19` | `-2` | 19 | `-` | 循环退出条件 与 -> 或 [变体不终止, 但挂之前已跑出差异: MISMATCH 19 处 (打印上限), 首条 'M |
| `f_menu` | `M020` | `3221225725` | 12 | `-` | MenuModeSwitch: menuModeStatus 清 NONE 改 ON_QUICK |
| `f_menu` | `M038` | `3221225725` | **0** | `-` | Menu1OnEnter: ON_QUICK 判定 == 改 != |
| `f_menu` | `M134` | `-2` | **缺失** | `NOJSON` | NoSameDeal: 外层终止条件 data == lower 改 != lower [变体不终止, 但挂之前已跑出差 |
| `f_menu` | `M438` | `3221225477` | 15 | `-` | 上位链走错方向 (upper 换成 lower) |
| `f_menu` | `M447` | `3221225477` | 1 | `-` | F 组分界由 <= 改成 < |
| `f_menu` | `M463` | `2` | 20 | `-` | 限值表命中判据取反 |
| `f_motorFc` | `F9` | `3221225620` | **0** | `-` | 极对数用错转速字段 |
| `f_profibus` | `P017` | `3221225477` | 20 | `-` | 帧长 DPDataNum + 4 改成 + 3 |
| `f_profibus` | `P024` | `3221225477` | 8 | `-` | 连通测试置 DPRcvTest 1 -> 0 |
| `f_profibus` | `P045` | `3221225477` | 20 | `-` | 发送侧连通测试判据取反 |
| `f_profibus` | `P072` | `3221225477` | 20 | `-` | PPO3 判据取反 |
| `f_profibus` | `P094` | `3221225477` | 20 | `-` | 读索引的两个实参对调 |
| `f_runSrc` | `M108` | `3221225620` | 40 | `-` | 摆动基准取错 (最大频率取成上限) |
| `f_runSrc_accDecFrq` | `AD1` | `3221225620` | 60 | `random` | guard: ovGain//ocGain 改成 && |
| `f_runSrc_accDecFrq` | `AD13` | `3221225620` | 60 | `phase_mode` | 匀速判据 == 改成 != |
| `f_runSrc_accDecFrq` | `AD18` | `3221225477` | 60 | `phase_closed_loop` | ACC/DEC 两个半条件用 && 连 |
| `f_runSrc_accDecFrq` | `AD19` | `3221225477` | 50 | `random` | ACC 分支: 时间取 decTime |
| `f_runSrc_accDecFrq` | `AD2` | `3221225620` | 60 | `random` | guard: motorCtrlMode == VF 改成 != |
| `f_runSrc_accDecFrq` | `AD20` | `3221225620` | 60 | `random` | ACC 分支: benchFrq 取 maxFrq |
| `f_runSrc_accDecFrq` | `AD22` | `3221225620` | 60 | `random` | DEC 分支: 时间取 accTime |
| `f_runSrc_accDecFrq` | `AD25` | `3221225620` | 60 | `phase_closed_loop` | benchmark: == 0 改成 == 1 |
| `f_runSrc_accDecFrq` | `AD26` | `3221225620` | 60 | `phase_closed_loop` | benchmark: maxFrq 换成 frqCurAim |
| `f_runSrc_accDecFrq` | `AD27` | `3221225620` | 60 | `phase_closed_loop` | benchmark: == 2 改成 == 1 |
| `f_runSrc_accDecFrq` | `AD28` | `3221225620` | 60 | `random` | benchmark: 100*decNumber 改成 50* |
| `f_runSrc_accDecFrq` | `AD3` | `3221225620` | 60 | `random` | guard: motorType != PMSM 改成 == |
| `f_runSrc_accDecFrq` | `AD30` | `3221225477` | 60 | `random` | 降级: 曲线判据取反 |
| `f_runSrc_accDecFrq` | `AD31` | `3221225620` | 60 | `random` | 降级: accDecTime 判据 > 改成 < |
| `f_runSrc_accDecFrq` | `AD32` | `3221225620` | 60 | `random` | 降级: 时间基准不除 timeBench |
| `f_runSrc_accDecFrq` | `AD33` | `3221225477` | 60 | `random` | 降级: 6 倍额定频率改成 5 倍 |
| `f_runSrc_accDecFrq` | `AD34` | `3221225620` | 60 | `random` | 降级: 两条判据用 && 连 |
| `f_runSrc_accDecFrq` | `AD35` | `3221225620` | 60 | `random` | 降级: 降成 S_CURVE_A 而不是 LINE |
| `f_runSrc_accDecFrq` | `AD36` | `3221225620` | 60 | `phase_closed_loop` | 时间换算: 漏掉 / RUN_CTRL_PERIOD |
| `f_runSrc_accDecFrq` | `AD37` | `3221225620` | 60 | `phase_mode` | 时间换算: 乘除互换 |
| `f_runSrc_accDecFrq` | `AD38` | `3221225477` | 60 | `phase_closed_loop` | 派发: LINE 判据改成 S_CURVE_A |
| `f_runSrc_accDecFrq` | `AD4` | `3221225620` | 60 | `random` | guard: 去掉 droopCtrl 取反 |
| `f_runSrc_accDecFrq` | `AD45` | `3221225620` | 60 | `random` | 派发: S_CURVE_A 判据改成 S_CURVE_B |
| `f_runSrc_accDecFrq` | `AD46` | `3221225620` | 60 | `random` | S_CURVE_A: 两个实参互换 |
| `f_runSrc_accDecFrq` | `AD47` | `3221225620` | 60 | `random` | S_CURVE_B: 两个实参互换 |
| `f_runSrc_accDecFrq` | `AD5` | `3221225620` | 60 | `random` | guard: runStatus != TUNE 改成 == |
| `f_runSrc_accDecFrq` | `AD7` | `3221225620` | 60 | `random` | guard 成立分支: frq0 取 frqTmp 而不是 frqRun |
| `f_runSrc_accDecFrq` | `AD8` | `3221225620` | 60 | `phase_closed_loop` | guard 不成立分支: frq0 取 frqRun 而不是 frqTmp |
| `f_runSrc_accDecFrq` | `LC2` | `3221225620` | **0** | `phase_line` | line: tickerAll 判据取反 |
| `f_runSrc_accDecFrq` | `SA1` | `3221225620` | 60 | `phase_scurve_a` | 入口: accDecTime 判据取反 |
| `f_runSrc_accDecFrq` | `SA11` | `3221225620` | **0** | `-` | 初始化: tmp3 的左移改右移 |
| `f_runSrc_accDecFrq` | `SA20` | `3221225620` | 60 | `phase_scurve_a` | 初始化: a[1] 的除数乘改除 |
| `f_runSrc_accDecFrq` | `SA8` | `3221225620` | **0** | `-` | 初始化: lineTime 的 2000 改成 1000 |
| `f_runSrc_accDecFrq` | `SB10` | `3221225620` | **0** | `-` | 拐点: frqCurAim > 0 改成 < |
| `f_runSrc_accDecFrq` | `SB15` | `3221225620` | 60 | `phase_scurve_b` | tickerAll2: 用 /frqCurAim/ 而不是 /inflexionFrq/ |
| `f_runSrc_accDecFrq` | `SB17` | `3221225620` | 60 | `phase_scurve_b` | tickerAll1: 右移 14 |
| `f_runSrc_accDecFrq` | `SB50` | `3221225620` | 60 | `phase_closed_loop` | START_S: 减速侧判据 > 0 改成 >= 0 |
| `f_runSrc_accDecFrq` | `SB6` | `3221225620` | 60 | `phase_scurve_b` | 入口: accDecTime 判据取反 |
| `f_ui` | `FU18` | `3221226505` | 18 | `-` | 位段范围 < -> <= |

## 结论

1. **`rc == 1` 的 6263 条是硬证据** —— 每一条都是"跑完 + 至少一处字段级不一致"。
2. **18 条 `MISMATCH` 为 0 的记录必须重做**。它们不是"抓到", 是"进程死了"。典型成因: 变异把一个除法/取模的除数变成 0, 或把一个下标推出数组边界 —— 目标端 C28x 上除零不陷入 (只置 DIV 溢出位并给饱和值)、越界只踩相邻内存, 宿主上却是立即终止。
3. 其余 58 条**已有字段级差异**, 证据不是假的, 但覆盖被截断: 崩溃点之后的相没有跑。这类要如实标注"覆盖到第 N 相为止", 不能与跑满全场的条目混在一起算。
4. `rc == 2` 的三条是测试台守卫主动中止, 打的是 `FATAL` / `SELFCHECK FAIL` —— 属于**可读的诊断**, 不是崩溃; 但仍应改成"喂一个良定义的输入"而不是"让台子拒绝继续"。
5. 修法不是"把那个输入从定义域里删掉" —— `p->t == 0`、`lengthCurrent == 0`、`accDecTime == 0` 都是模块的**合法输入**, 驱动必须喂。要么把变异改成良定义的形式 (保底非零 / 换判据 / 两臂互换), 要么证明它在可达状态空间里处处等价并从清单删除, 并在用例 `README.md` 里写清理由。

