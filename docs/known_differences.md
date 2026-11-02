# 已知差异列表 (known_differences)

本节列出新工程相对原工程**全部有意的差异**。除本表所列之外, 所有源码文件的
**token 流逐项相同**(见 `docs/symbol_comparison.md`), 即逻辑零改动。

## 1. 目录结构 (全部差异之根源)

| 原位置 | 新位置 | 说明 |
|---|---|---|
| `system/` | `src/system/` | 系统启动、主循环、初始化 |
| `function/` | `src/function/` | 功能、参数、通信、UI |
| `motor/` | `src/motor/` | 电机控制、采样、PWM、保护 |
| `subprogram/` | `src/subprogram/` | 辅助程序 |
| `public/28035_header/` (TI 设备头) | `include/device/` | 28 个 `DSP2803x_*.h` + `DSP28x_Project.h` + `IQmathLib.h` + `SFO_V6.h` |
| `public/28035_header/SystemDefine.h` 等 | `include/common/` | 公共类型/宏/结构定义 |
| `public/function_header/` | `include/function/` | 功能层接口 (含被 `#include` 的表文件) |
| `public/motor_header/` | `include/motor/` | 电机层接口 |
| `public/common/*.cmd` | `linker/` | 链接脚本 |
| `public/common/*.lib` | `linker/lib/` | 运行库 / IQmath 库 |
| `CCs/md380.pjt` | `project/jt380.pjt` | 工程文件 |
| `CCs/Debug.lkf` | `project/Debug.lkf` | 重新生成 |
| (无) | `project/Release.lkf` | 新增, 对齐 Debug 结构 |
| `gel/` | `project/gel/` | GEL 调试脚本 |
| `function/DSP2803x_usDelay.asm` 等 4 个 | `asm/` | 汇编单独成目录 |
| (无) | `src/orphan/` | 不参与构建的孤儿文件 |
| (无) | `tests/`、`tools/`、`docs/`、`build/` | 新增 |

## 2. 丢弃的文件 (41 个, 全部有依据)

### 2.1 `function/include/` 下的 30 个陈旧副本

`f_canlink.h` `f_comm.h` `f_comm_DP.h` `f_common.h` `f_debug.h` `f_dspcan.h`
`f_eeprom.h` `f_error.h` `f_fcDeal.h` `f_frqSrc.h` `f_funcCode.h` `f_interface.h`
`f_invPara.h` `f_io.h` `f_main.h` `f_menu.h` `f_osc.h` `f_p2p.h` `f_plc.h`
`f_posCtrl.h` `f_runSrc.h` `f_ui.h` `f_funcCode_disp.c`
`f_table_eeprom2Fc.c` `f_table_eeprom2Fc1.c` `f_table_eeprom2Fc2.c`
`f_table_eeprom2Fc3.c` `f_table_eeprom2Fc4.c` `f_table_eeprom2Fc5.c`
`f_table_eeprom2Fc6---.c`

**依据**: `function/include/` **不在 `md380.pjt` 的 `-i` 搜索路径上**,
这些文件不参与任何编译。权威版本是 `public/function_header/` 下的同名文件。
决定性证据: `function/include/f_funcCode.h` 引用了全工程不存在的
`DSP280x_Device.h`, 若它参与编译原工程必然失败。

### 2.2 `system/` 下的 3 个非构建副本

`system/main.c`(与 `function/main.c` **内容完全相同**),
`system/DSP2803x_CpuTimers.c`、`system/DSP2803x_GlobalVariableDefs.c`
(构建用的是 `function/` 版本)。

### 2.3 `function/` 下的 2 个陈旧汇编副本

`function/DSP2803x_CodeStartBranch.asm`、`function/DSP2803x_CSMPasswords.asm`
(构建用的是 `system/` 版本)。

### 2.4 `public/motor_header/` 下的 2 个陈旧头文件

`DSP2803x_Device.h`(2009-12 版, 落后于 `28035_header` 的 2012-12 版)、
`MotorVCInclude.h`(缺 `SVC0_STRUCT`、`gIMAcr`、`gITAcr`)。

### 2.5 未丢弃但改位置的 4 个孤儿文件

`DSP2803x_PieCtrl.c`、`DSP2803x_SysCtrl.c`、`f_length.c`、`z_func.c`
→ `src/orphan/`, **不参与构建**, 见 `src/orphan/README.md`。

## 3. 注释清除

- 删除注释 **26,886 处**(C 单/多行、汇编 `;` 与列首 `*`、链接脚本、工程文件、GEL)。
- 去注释后 **145 / 145 文件为纯 ASCII**(中文只存在于注释中, 全部消失)。
- 行尾空白已去除, 连续空行折叠为 1。
- **保留** `.pjt` 首行的 CCS 格式必需行(见 `rewrite_decisions.md` D6)。

## 4. 工程配置改动 (仅 4 处)

| 项 | 原值 | 新值 | 原因 |
|---|---|---|---|
| Debug 包含路径 | `public/28035_header`, `public/motor_header`, `public/function_header` | `include/device`, `include/common`, `include/function`, `include/motor` | 目录重组 |
| Debug/Release 输出目录 | 源码目录旁 / `CCs/Release` | `build/Debug` / `build/Release` | 计划要求 build 只放生成物 |
| 链接器库搜索路径 | `C:/CCStudio_v3.3PLA/C2000/cgtools/lib` | `..\linker\lib`(工程自带 `rts2800_ml.lib`) | 消除对原 CCS 安装路径的硬依赖 |
| Release 产物名 | `MD380_Release.out` | `jt380_Release.out` | 命名统一(仅文件名不同) |

其余编译/链接选项**逐字保留**, 包括 `-o2` / `-o3`。
这两个是 `-O2` / `-O3` 的**同义写法**(在 `cl2000` 里 `-o` 即 `--opt_level` 的别名),
所以原工程的 Debug/Release **确实开了优化**, 逐字保留即忠实复现该配置。
(早期文档曾误判为"`-o` 是输出文件名, 故未开启优化", 已更正 ——
实测 `-oN` 与 `-ON` 目标文件 canonical 摘要完全相同, 见 `docs/rewrite_decisions.md` D5。)

> **不在上表内的一个"隐性"要求**: 用新版 CGT 构建时必须额外传 `--abi=coffabi`。
> 它**没有**写进 `project/jt380.pjt`(CCS 3.3 的 CGT 不认这个开关, 写进去会让老 IDE
> 打不开工程), 而是由 `tools/build_official.py` / `tools/build_baseline.py` 在调用
> `cl2000` 时注入。详见 `docs/rewrite_decisions.md` D8。

## 5. 未做改动的项 (刻意保持)

> 本节说的是**未列入 `docs/rewritten_modules.json` 的模块**。已改写的模块按设计
> 豁免"实现逐字一致", 但豁免范围仅限**结构性**变换 (分支极性、提取同类型局部量、
> 把宏展开成显式语句、重排互不相关的语句); 函数名、表达式类型宽度、硬件读写序、
> 魔数与定点标度仍然不动 —— 逐条约束写在清单的 `invariants` 里, 由差分测试与
> 目标端外部符号集合检查兜底。

- 条件编译 `#if` / `#ifdef` / `#if 0` 分支: 全部原样保留。
- 所有函数名、参数、返回值、全局变量名: 未改。
- 所有魔数、定点标度、移位位数、运算顺序、括号: 未改 (已改写模块的结构性变换除外, 见上)。
- 所有结构体 / 位域 / union 布局: 未改。
- 所有隐式函数声明、未使用全局量: **保留**, 只登记(见静态检查报告)。
- 原工程目录 `bpqym` 未被修改(只读使用; 实测近 3 小时内 0 个文件被改动)。
- **`f_plc.c` 的既有越界读: 刻意不修。** `PlcDataFramDeal()` 的四张表都只声明了
  `[NUM+1]` 个元素, 而可达下标能超出去 (`InvRecFastTab`/`InvRecSlowTab` 经 `addr+2`,
  `InvTrFastTab`/`InvTrSlowTab` 经 `invTrIndex+3`)。这是**原工程既有行为**, 不是改写
  引入的; 本工程的契约是"功能不变", 改写它等于改行为。且越界读是 UB, 宿主上读到的
  相邻内存本来就不等于目标端读到的, 补 padding 只是把"读到未知值"换成"读到宿主特有
  的值", 给不了目标端任何保证。
  代价要说清: `f_plc` 的差分测试通过控制表内容**避开**了这些状态, 所以它证明的是
  "表尾为零的配置下 ref 与 new 等价"; "表尾非零导致越界读"那一档属于本证据链**不覆盖**
  的范围。细节与四张表的可达下标表见 `tools/difftest/cases/f_plc/README.md`。

## 6. 差异之外: 等价性证据

**本节不再抄数字。** 之前这里有一张手打的汇总表, 和 `docs/equivalence_evidence.md`
里的数字重复 —— 而那份报告是 `tools/make_build_report.py` **生成**的, 数字来自
`docs/_difftest/*.json` 与 `docs/_dis_compare.json`。两处同源数字必然漂移, 之前就
出过一次: 报告里的用例数在工具输出里**根本不存在**, 因为生成器没跟上改写进度。
所以现在只有一处权威数字, 就是那份生成的报告。

```bash
python tools/build_and_compare.py     # 层级 A/B/C: 反汇编、目标文件内容级、整机映像
python tools/make_build_report.py    # 生成 docs/equivalence_evidence.md
python tests/run_all.py              # 断言上面这些数字没有退化
```

定性结论 (这些不随数字变化):

- **未改写模块**: 仍受"token 流逐文件一致 + 目标文件内容级一致"的原始契约约束,
  逐字节证据对它们完全适用。
- **已改写模块**: 编译产物必然不同, 因此 token 流 / 反汇编 / 整机映像这三条逐字节
  证据对它**不适用**。改由**宿主差分测试** (层级 D) + **目标端副作用检查** (层级 E)
  覆盖。清单见 `docs/rewritten_modules.json`。
- **整机映像**: 因改写而不同, 但差异**有界** (只有改写模块的 `.text` 变化)。
- **告警集合**: 与原工程副本归一化后完全一致, 改写未引入新告警。
- **本证据链不覆盖的**: 中断时序 / 最坏执行时间、目标端宽度语义、宿主与目标端的
  绝对值差异、宿主与目标端的结构体布局差异、硬件实际行为 (PWM/ADC/CAN 波形、
  保护动作、上电引导)。逐条说明见 `tools/difftest/README.md` 的
  "这套证据**不能**证明什么"。
