# 构建与验证说明 (build_and_verify)

## 1. 工程概况

| 项 | 值 |
|---|---|
| 目标芯片 | TMS320F28035 (C28x), 60 MHz (`SystemDefine.h` 里 `DSP_CLOCK60` 生效) |
| 输出格式 | COFF (老 CCS 3.3 时代工具链) |
| 原工具链 | Code Composer Studio v3.3 (`C:\CCStudio_v3.3PLA`), 链接器 `v5.2.1` |
| 工程文件 | `project/jt380.pjt` (CCS 3.3 格式) |
| 编译单元 | 59 个 (`src/system` 4 + `src/function` 28 + `src/motor` 22 + `src/subprogram` 1 + `asm` 4) |
| 链接脚本 | `linker/F28035.cmd`, `linker/DSP28035Peripheral.cmd` |
| 运行库 | `linker/lib/rts2800_ml.lib` |
| 入口 | `code_start` @ `0x003F7FF6` (段 `codestart`, 来自 `asm/DSP2803x_CodeStartBranch.asm`) |

## 2. 目录结构

```
bpqdm_rewrite/
├─ src/system/       系统启动、主循环、初始化 (4 个 .c)
├─ src/function/     功能、参数、通信、UI (28 个 .c)
├─ src/motor/        电机控制、采样、PWM、保护 (22 个 .c)
├─ src/subprogram/   辅助程序 (1 个 .c)
├─ src/orphan/       不参与构建的孤儿文件 (4 个 .c, 见该目录 README)
├─ include/device/   TI 设备头文件 (31 个)
├─ include/common/   公共类型 / 宏 / 结构定义 (5 个)
├─ include/function/ 功能层接口 (29 个, 含被 #include 的表文件)
├─ include/motor/    电机层接口 (17 个)
├─ asm/              必须保留的汇编 (4 个)
├─ linker/           链接脚本 + lib/
├─ project/          CCS 工程 + lkf + gel/
├─ tests/            主机侧测试与静态检查
├─ tools/            构建与扫描工具
├─ docs/             设计与测试报告
└─ build/            编译生成物 (Debug/ Release/)
```

## 3. 用 CCS 3.3 构建 (与原工程一致)

1. 打开 CCS 3.3 → `Project` → `Open` → 选择 `project/jt380.pjt`。
2. 选择 `Debug` 或 `Release` 配置。
3. `Project` → `Rebuild All`。
4. 产物: `build/Debug/jt380_C.out` + `build/Debug/jt380_C.map`
   (Release: `build/Release/jt380_Release.out` + `.map`)。

## 4. 用新版 CGT 构建 (推荐, 便于自动化)

新 CGT **默认 EABI**, 原工程是 **COFF**, 必须加 `--abi=coffabi`, 否则必然链接失败
(符号前导下划线约定与运行库都不同)。**注意: 这个开关不能写进 `jt380.pjt`**
—— CCS 3.3 的 CGT 不认它, 会让老 IDE 打不开工程。详见 `rewrite_decisions.md` D8。

本机实测可用的完整命令 (`E:\ti\c2000\cgt-c2000_25.11.1.LTS`):

```bash
cd <工程根>/project
CGT="E:/ti/c2000/cgt-c2000_25.11.1.LTS/ti-cgt-c2000_25.11.1.LTS/bin"
INC='-i"$(Proj_dir)\..\include\device" -i"$(Proj_dir)\..\include\common" \
     -i"$(Proj_dir)\..\include\function" -i"$(Proj_dir)\..\include\motor"'
FLAGS="-g -q -o2 --abi=coffabi -d_DEBUG -dDSP2803X -dLARGE_MODEL -ml -mt -v28"

# 编译 59 个单元 (源文件路径是相对 project/ 的, 即 ..\src\...)
for f in ../src/system/*.c ../src/function/*.c ../src/motor/*.c \
         ../src/subprogram/*.c ../asm/*.asm; do
    "$CGT/cl2000" $FLAGS -i../include/device -i../include/common \
        -i../include/function -i../include/motor \
        -fr../build/Debug -c "$f"
done

# 链接 (Debug.lkf 第 1 行的 -z 触发链接模式; 其中路径也是相对 project/ 的)
"$CGT/cl2000" -@Debug.lkf

# 生成烧写镜像 (C28x 是 16 位字机器, 宽度参数不能漏)
"$CGT/hex2000" -i -memwidth 16 -romwidth 16 \
    ../build/Debug/jt380_C.out -o ../build/Debug/jt380_C.hex
```

或直接用仓库内脚本 (已封装上述全部步骤, 含原工程副本对照):

```bash
python tools/build_official.py    # 构建本工程 Debug + Release
python tools/build_baseline.py    # 只读复制 bpqym 后构建, 做对照
python tools/build_and_compare.py # 逐模块两层比对
python tools/verify_tree.py       # 刷新 docs/_verify.json (静态验收: token/注释/符号)
python tools/make_build_report.py # 汇总 docs/equivalence_evidence.md 等
```

> `verify_tree.py` **必须排在 `make_build_report.py` 前面**。后者是从
> `docs/_verify.json` 读"有几个改写模块、几个文件 token 流一致"的; 而 `_verify.json`
> 由前者写。顺序颠倒时它读到的是**旧清单**跑出来的产物, 生成的
> `equivalence_evidence.md` 就会自相矛盾 (同一次输出里两处报出**不同**的改写模块数,
> 一边还是旧清单的数字)。现在 `make_build_report.py` 里加了新鲜度闸门: 两边条数对不上就直接
> 拒绝生成并提示先跑 `verify_tree.py`。`tests/run_all.py` 也会跑 `verify_tree.py`,
> 但它同样必须排在本文件之前。

### 4.1 实测结果 (2026-10-19, 含 43 个改写模块)

| 项 | Debug | Release |
|---|---|---|
| 编译 | **59 / 59 成功, 0 失败** | **59 / 59 成功, 0 失败** |
| 链接 | rc=0 | rc=0 |
| 映像 | `build/Debug/jt380_C.hex` (285,587 B) | `build/Release/jt380_Release.hex` (285,303 B) |
| SHA256 | `eac00aebd8c3114c…` | `74981c8529e12a29…` |
| 与原工程副本比对 | **不同, 映像数据字节数 -1,767 B (因改写)** | **不同, -1,784 B (因改写)** |

入口点核对: `ENTRY POINT SYMBOL: "code_start" address: 003f7ff6` ✓

逐模块证据: **16 个模块反汇编逐行相同**、**18 / 59 个模块目标文件内容级一致**
(段 + 重定位 + 符号表); 41 个改写模块按预期不同 (清单见 `docs/rewritten_modules.json`,
汇总表见 `docs/equivalence_evidence.md`), 由**宿主差分测试 + 目标端副作用检查**覆盖。

> 注意 `f_vf.c` 与 `f_interface.c` **虽然也在改写清单里, 产物却与改写前相同** ——
> 它们落在上面那 16 / 18 里, 不在"不同"的 41 个之中。所以这一节的判据是
> "**没改写过的必须相同**", 不是"改写过的一定不同": 最好的改写本来就应该收敛到
> 同一份机器码。
>
> 这里的两层比对**刻意关优化**编译 (灵敏度最高, 任何改写都会在 `.text` 上留痕)。
> 工程实际交付的映像按 `-o2`/`-o3` 编, 口径不同: 43 个改写模块里 **6 个在两个配置下
> 都收敛**到同一份机器码 (`f_vf` / `f_frqSrc` / `f_interface` / `f_eeprom` / `f_main` /
> `MotorDataExchange` —— 例如 `f_frqSrc` 关优化下 `.text` 差 2 字, 到 `-o2`/`-o3`
> 就逐行相同), **1 个只在 Release 收敛** (`f_profibus`; Debug 差 478 行),
> 另外 36 个两个配置都仍不同 (最极端的是 `f_menu`, Debug 差 4140 行 / Release 差 4129 行)。
> 所以整机 hex 与改写前**不同但差异有界** (Debug -1,767 B / Release -1,784 B, 见上表) ——
> 既不是"改写过就一定不同", 也不是"一定相同"。逐模块收敛情况见
> `tools/difftest/o2_check.py` 落盘的 `docs/_difftest/<case>.o2.json`。
> 详见 `docs/equivalence_evidence.md` 与 `docs/rewrite_decisions.md` D10 / D11 / D13。
>
> `MotorDataExchange` 是最新加入的一个, 也是**第一个结构性改写后仍完全收敛**的模块:
> 关优化下它的 `.text` 从 1259 字降到 1218 字 (少了 41 字), 但 `-o2`/`-o3` 下优化器
> 把新增的局部指针折回同一套寻址, 反汇编逐行相同 —— 因此**它在交付配置下对整机映像
> 尺寸没有贡献** (它的 `-o2`/`-o3` 目标文件与改写前逐字节相同)。

**注意事项** (踩坑清单):

| 坑 | 现象 | 解法 |
|---|---|---|
| 漏 `--abi=coffabi` | 编译全过, 链接报 `object files have incompatible formats (… = TI-COFF, … = ELF)` | 必须加 |
| 源码在中文路径 | `cl2000` **挂起**(不是报错) | 复制到纯 ASCII 路径 |
| `hex2000` 漏宽度 | hex 只有一半大, 烧进去起不来 | `-memwidth 16 -romwidth 16` |
| 把 `-o2` 当成"输出文件名" | 误判成"优化没生效" | `-o2` **就是** `-O2`: 编译器里 `-o` 是 `--opt_level` 的别名 (`-o=<路径>` 会报 `bad argument to option -O`); 输出文件名要用 `--output_file=`。见 `docs/rewrite_decisions.md` D5 |
| 链接器 `-i` 传 `/d/...` | `cannot find file "rts2800_ml.lib"` | 传 Windows 风格路径 |
| 链接器 `.lkf` 首行漏 `-z` | 当成编译选项解析, 报一堆无法识别的选项 | 首行必须有 `-z` |
| 直接 `cmp` 两个 `.obj` | 上万条"差异" | 时间戳 + 内嵌源路径不同所致; 用 `tools/coff_cmp.py` |
| 用 `ofd2000` 的 `File Offset` 切未初始化段 | 切到文件头, 报假的段内容差异 | `off == 0` 视为"无文件数据" |
| 在原目录里构建 | 会写入 `.obj`, 违反"不改原工程" | 用 `tools/build_baseline.py` 复制后再建 |

## 5. 验证清单 (给验证人员)

按计划文档第十五节, 依次执行:

| # | 检查 | 命令 / 方法 | 预期 |
|---:|---|---|---|
| 1 | 目录是否独立 | `python tools/check_includes.py` | `references to old tree: 0` |
| 2 | 是否误引用原目录 | 同上 | 0 处 |
| 3 | 源码是否完整 | `python tests/test_tree_contract.py` | 145 个文件, 59 个编译单元 |
| 4 | 注释是否全部清除 | `python tools/verify_tree.py` | `comment_residual 0` |
| 5 | 头文件与重复定义 | `python tools/check_includes.py` | 329 处解析, 仅孤儿文件缺 1 个头 |
| 6 | 入口函数与中断 | `docs/interrupt_list.md` | 90 个中断函数 |
| 7 | 全局变量与公共接口 | `docs/global_symbol_list.md` | 与原文一致 |
| 8 | Debug / Release 配置 | `project/jt380.pjt` | 两个配置齐全 |
| 9 | 编译与链接 | 见第 3/4 节 | **已实测: 59/59 编译, 链接 rc=0** |
| 10 | map 与内存布局 | `docs/memory_layout.md` | 段布局与原工程一致 |
| 11 | 关键符号与函数 | `docs/symbol_comparison.md` | token 等价 |
| 12 | 通信 / 参数 / 错误码 | 源码字面量 | token 等价, 未变 |
| 13 | 主循环与周期任务 | 源码 | 未变 |
| 14 | 电机控制关键路径 | 源码 | 未变 |
| 15 | 测试报告真实性 | `python tests/run_all.py` | **全绿** |
| 16 | 问题清单 | `docs/static_check_report.md` | 见第 4/5 节 |
| 17 | **逐模块等价** | `python tools/build_and_compare.py` | **16 个反汇编逐行相同 / 41 个不同 / 2 个纯数据; 18 / 59 目标文件内容级一致**; 41 个改写模块不同 (`f_vf` / `f_interface` 在关优化口径下产物仍逐字节相同; 6 个 (`f_vf` / `f_frqSrc` / `f_interface` / `f_eeprom` / `f_main` / `MotorDataExchange`) 在 `-o2`/`-o3` 下反汇编逐行相同, `f_profibus` 仅 Release 收敛, 其余 36 个不收敛), 由差分测试覆盖 |
| 18 | **整机映像比对** | `python tools/make_build_report.py` | 因改写而不同, **差异有界** (Debug -1,767 B / Release -1,784 B) |
| 19 | **告警集合等价** | `docs/warning_list.txt` | 与原工程副本归一化后 **21 类完全一致** |
| 20 | **改写模块行为等价** | `python tools/difftest/run_case.py` | **25,079,018 用例, 0 处不一致**; 负向对照 **4573/4573 抓到** |

## 6. 主机侧测试 (无需硬件; 编译相关项需先跑过构建脚本)

```bash
python tests/run_all.py
```

包含:

- `test_tree_contract.py` — 文件数 / 编译单元数 / 注释 0 / 非 ASCII 0 / 无 TODO
- `test_token_equivalence.py` — 145 文件 token 流等价
- `test_project_consistency.py` — `.pjt` 与 `.lkf` 的源文件清单一致(59 个)、
  链接脚本存在、include 路径存在
- `test_negative_control.py` — 负向对照(证明检查有效)
- `test_build_evidence.py` — 构建证据契约(逐模块一致 / 映像逐字节相同 /
  入口点 `code_start` @ `003f7ff6`); 若 `build/` 被清理过会明确 SKIP 而非假装通过

## 7. 必须由硬件验证的部分 (本机无法覆盖)

1. 上电、时钟、GPIO、ADC、PWM 初始状态与看门狗。
2. 电机启动 / 停止 / 正反转 / 加减速 / 低速高速 / 负载变化 / 突然停机 / 重启 / 模式切换。
3. 硬件过流、软件过流、过压、欠压、过温、编码器异常、ADC 异常、通信中断、
   参数越界、堵转、失速。
4. 通信读写、参数保存、状态读取、运行命令、频率、方向、故障读取/复位、
   CRC 错误、地址错误、超时、异常帧。
5. 默认参数、参数修改、保存、断电恢复、越界、非法参数、版本变更、恢复出厂。

> 本轮已实测证明: **未改写模块**的源码 token 流与原工程**逐一相同**, 产物逐字节一致
> (16 个未改写模块的目标文件**全部**内容级一致; 其中 14 个有代码的模块反汇编也逐行
> 相同, 另 2 个是纯数据模块 —— `DSP2803x_GlobalVariableDefs` 与 `MotorVar`,
> 反汇编不适用。见 `docs/equivalence_evidence.md`)。
> **43 个改写模块** (清单见 `docs/rewritten_modules.json`) 的实现被改过, 其中 41 个编译产物不同
> (`f_vf.c` 与 `f_interface.c` 改写后产物仍与改写前相同); 全部 43 个的
> 行为等价由 **25,079,018 用例的宿主差分测试 + 目标端副作用检查**证明,
> 负向对照 4573/4573 抓到 —— 但"行为等价"仍不能替代实机验证,
> 尤其保护逻辑必须台架确认。
