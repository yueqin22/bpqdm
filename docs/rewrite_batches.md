# 实现改写的分批方案

本文件由 `tools/difftest/classify_modules.py` 生成, 数据来自**实测产物**:

- 活/死与代码规模: `build/Debug/jt380_C.map` 里各 `.obj` 的贡献
- 硬件耦合: `nm2000` 的未定义符号 ∩ `DSP2803x_GlobalVariableDefs.obj` 导出的寄存器集合
- 汇编数学依赖: 未定义符号里是否出现 `_qsin` / `_qatan` / `_qsqrt`
- 时序敏感: 源码里是否有 `interrupt void` 定义

| 批 | 含义 | 模块数 | 代码字数 | 证明难度 |
|---|---|---:|---:|---|
| **DONE** | 已改写 | 43 | 33,983 | — |
| **NONE** | 无实现可改 | 4 | 0 | — |
| **T1** | 纯逻辑 | 0 | 0 | 低 |
| **T2** | 硬件耦合 | 1 | 3,852 | 中 |
| **T3** | 依赖汇编数学 | 4 | 6,914 | 高 |
| **T4** | 时序敏感 | 3 | 4,283 | 最高 |

**建议按 T1 → T2 → T3 → T4 推进**, 每一批跑完再进下一批。

## DONE — 已改写

已完成, 走差分测试 + 目标端检查路径

| 模块 | 代码字数 | 寄存器 | 汇编数学 | 中断 |
|---|---:|---:|---:|---:|
| `src/function/f_menu.c` | 3914 | 0 | 0 | 0 |
| `src/function/f_runSrc.c` | 2873 | 0 | 0 | 0 |
| `src/motor/MotorPmsmParEst.c` | 2146 | 5 | 0 | 0 |
| `src/function/f_frqSrc.c` | 2072 | 0 | 0 | 0 |
| `src/motor/MotorInvProtect.c` | 1400 | 5 | 0 | 0 |
| `src/motor/MotorMain.c` | 1392 | 1 | 0 | 0 |
| `src/function/f_frqSrc_pid.c` | 1297 | 0 | 0 | 0 |
| `src/motor/MotorEncoder.c` | 1205 | 5 | 0 | 0 |
| `src/function/f_eeprom.c` | 1154 | 3 | 0 | 0 |
| `src/function/f_interface.c` | 1143 | 1 | 0 | 0 |
| `src/motor/MotorPmsmMain.c` | 1084 | 3 | 2 | 1 |
| `src/function/f_error.c` | 1054 | 0 | 0 | 0 |
| `src/motor/MotorDataExchange.c` | 977 | 0 | 0 | 0 |
| `src/function/f_main.c` | 957 | 2 | 0 | 0 |
| `src/function/f_ui.c` | 903 | 3 | 0 | 0 |
| `src/motor/MotorCarrier.c` | 856 | 1 | 0 | 0 |
| `src/motor/MotorSVC.c` | 833 | 1 | 2 | 0 |
| `src/function/f_invPara.c` | 711 | 0 | 0 | 0 |
| `src/motor/MotorSpeedCheck.c` | 697 | 1 | 0 | 0 |
| `src/motor/MotorPublicCal.c` | 672 | 5 | 0 | 0 |
| `src/function/f_fcDeal.c` | 648 | 0 | 0 | 0 |
| `src/function/f_profibus.c` | 589 | 0 | 0 | 0 |
| `src/function/f_canlink.c` | 570 | 0 | 0 | 0 |
| `src/motor/MotorPWM.c` | 521 | 3 | 1 | 0 |
| `src/function/f_comm_DP.c` | 507 | 4 | 0 | 4 |
| `src/function/f_dspcan.c` | 482 | 4 | 0 | 0 |
| `src/subprogram/SubPrg.c` | 428 | 0 | 1 | 0 |
| `src/function/f_plc.c` | 417 | 0 | 0 | 0 |
| `src/system/mainInit_2803x.c` | 407 | 8 | 0 | 3 |
| `src/motor/MotorParEst.c` | 320 | 3 | 0 | 0 |
| `src/motor/MotorInfoCollect.c` | 274 | 1 | 0 | 0 |
| `src/function/f_torqueCtrl.c` | 250 | 0 | 0 | 0 |
| `src/system/main.c` | 232 | 5 | 0 | 3 |
| `src/function/f_modbus.c` | 225 | 0 | 0 | 0 |
| `src/motor/MotorCurrentTransform.c` | 212 | 0 | 2 | 0 |
| `src/function/f_vf.c` | 176 | 0 | 0 | 0 |
| `src/function/f_p2p.c` | 130 | 0 | 0 | 0 |
| `src/function/f_motorFc.c` | 121 | 0 | 0 | 0 |
| `src/function/f_common.c` | 70 | 0 | 0 | 0 |
| `src/system/DSP2803x_CpuTimers.c` | 64 | 3 | 0 | 0 |
| `src/motor/MotorDebug.c` | 0 | 0 | 0 | 0 |
| `src/motor/ZeroPosCtl.c` | 0 | 2 | 0 | 0 |
| `src/function/f_debug.c` | 0 | 0 | 0 | 0 |

## NONE — 无实现可改

0 字代码: 纯数据/表文件, 或链接期被丢弃的死代码。改写无从下手, 也不需要

| 模块 | 代码字数 | 寄存器 | 汇编数学 | 中断 |
|---|---:|---:|---:|---:|
| `src/system/DSP2803x_GlobalVariableDefs.c` | 0 | 0 | 0 | 0 |
| `src/motor/MotorConstant.c` | 0 | 0 | 0 | 0 |
| `src/motor/MotorVar.c` | 0 | 0 | 0 | 0 |
| `src/function/f_funcCode.c` | 0 | 0 | 0 | 0 |

## T1 — 纯逻辑

不碰外设寄存器、不依赖汇编数学库。差分测试最容易做, 风险最低

(无)

## T2 — 硬件耦合

读写外设寄存器。差分测试要先补寄存器 mock, 并保证两侧看到同一份寄存器状态

| 模块 | 代码字数 | 寄存器 | 汇编数学 | 中断 |
|---|---:|---:|---:|---:|
| `src/function/f_io.c` | 3852 | 6 | 0 | 0 |

## T3 — 依赖汇编数学

引用 qsin / qatan / qsqrt (在 asm/ASMSubPrg.asm 里)。宿主测试要先做位精确的 C 复刻, 复刻本身又是新的不确定源

| 模块 | 代码字数 | 寄存器 | 汇编数学 | 中断 |
|---|---:|---:|---:|---:|
| `src/motor/MotorVF.c` | 2237 | 0 | 2 | 0 |
| `src/motor/MotorVCMain.c` | 2021 | 0 | 2 | 0 |
| `src/motor/MotorParChange.c` | 1366 | 8 | 2 | 0 |
| `src/function/f_runSrc_accDecFrq.c` | 1290 | 0 | 1 | 0 |

## T4 — 时序敏感

定义了中断服务程序。除行为等价外, 还必须重测最坏执行时间 —— 而改写必然改变指令序列

| 模块 | 代码字数 | 寄存器 | 汇编数学 | 中断 |
|---|---:|---:|---:|---:|
| `src/motor/MotorImParEst.c` | 1910 | 8 | 2 | 2 |
| `src/function/f_comm.c` | 1478 | 5 | 0 | 10 |
| `src/function/f_osc.c` | 895 | 5 | 0 | 4 |

## 每个模块的固定动作

见 `tools/difftest/README.md`「怎么加一个模块」—— 12 步, 顺序不能换。
其中两条是硬性的:

1. **动代码前先跑一次差分测试**, 确认 0 不一致 (此时两侧还是同一份代码)。
    跳过这步, 后面的"0 不一致"无法区分"改写正确"和"测试台不工作"。
2. **负向对照必须全部抓到**。抓不到就说明这个输入空间对该类错误不敏感,
    要补用例而不是继续。

## 这一批不能证明的东西

- **周期数**: 改写会改变指令序列 (`f_p2p` `.text` 154→159 字)。
  T4 的 7 个模块全部定义了中断, 必须重测最坏执行时间。
- **目标端数值**: 宿主 `int` 是 32 位, C28x 是 16 位,
  `Uint16 * 50` 这类表达式两边不等值。比较仍然有效, 但绝对值不可迁移。
- **硬件行为**: 上板验证不在本方案范围内。

