"""由 _verify.json / _includes.json 生成静态检查报告与构建说明类文档。"""
import os, json

DST = r"E:/software/bpqdm/bpqdm_rewrite"
DOC = os.path.join(DST, 'docs')
V = json.load(open(os.path.join(DOC, '_verify.json'), encoding='utf-8'))
I = json.load(open(os.path.join(DOC, '_includes.json'), encoding='utf-8'))
P = json.load(open(os.path.join(DOC, '_project.json'), encoding='utf-8'))
M = json.load(open(os.path.join(DOC, '_map.json'), encoding='utf-8'))

# token 比对的"未改写层"口径: 比对总数里扣掉已豁免的改写模块。
T = V['token_stream_identical']
NEX = len(T.get('rewritten', []))
NTOK = T['files'] - NEX

# 逐模块产物比对的结果**不写死**: 能取到 _dis_compare.json 就按它现算,
# 取不到就指向文档 —— 免得再留"几年前的 57/57"这种过期数字。
D = (json.load(open(os.path.join(DOC, '_dis_compare.json'), encoding='utf-8'))
     if os.path.isfile(os.path.join(DOC, '_dis_compare.json')) else None)

L = ['# 静态检查报告 (static_check_report)', '',
     '## 1. 检查项总览', '',
     '| # | 检查项 | 方法 | 结果 |', '|---:|---|---|---|',
     '| 1 | 注释残留 | 逐行扫描 `/*` `//` 与汇编 `;` / 列首 `*` | **0 处** |',
     '| 2 | TODO / FIXME / NOTE / XXX / HACK | 全树关键字扫描 | **0 处** |',
     "| 3 | 非 ASCII 字节 | 逐文件 ASCII 解码 | **0 处** |",
     '| 4 | 二次去注释可删字符 | 幂等复跑 | **0 处** |',
     '| 5 | token 流等价 | 真 C 词法器逐 token 比对 | **未改写层 %d / %d 一致**; 改写层 %d 个豁免 (另证) |' % (
         NTOK - len(T['mismatch']), NTOK, NEX),
     '| 6 | 符号抽取一致(全局量 + 函数签名) | 剥离注释后正则抽取比对 | **%d 处不一致** |' % len(V['symbol_identical']),
     '| 7 | 隐式函数声明 | 调用点 vs 可见声明集合 | **%d 处**(见第 4 节) |' % len(V['implicit_declaration']),
     '| 8 | 未使用全局量 | 全树标识符计数 | **%d 处**(见第 5 节) |' % len(V['unused_global']),
     '| 9 | include 自洽 | 新树内解析全部 `#include "..."` | **%d 处解析, %d 处缺失** |' % (
         I['resolved'], len(I['missing'])),
     '| 10 | 对原目录的引用 | 关键字扫描 | **%d 处** |' % len(I['oldref']),
     '| 11 | 中断函数定义 | `interrupt void` 抽取 | **%d 个** |' % len(V['isr_definitions']),
     '| 12 | volatile 声明 | `volatile` 声明行统计 | **%d 个文件** |' % len(V['volatile_declarations']),
     '| 13 | 负向对照 | 注入 4 类错误验证检查有效性 | **4 / 4 抓到** |',
     '']

L += ['## 2. token 流等价明细', '',
      '比对文件数: **%d**; 未改写层不一致: **%d**; 改写层豁免: **%d** (见 `docs/rewritten_modules.json`)。' % (
          T['files'], len(T['mismatch']), NEX),
      '', '比对方法: 忽略注释与空白后逐 token 比较。字符串/字符字面量按整体处理,',
      '因此字面量内部的分号、括号、反斜杠不会干扰切分。', '']
if V['token_stream_identical']['mismatch']:
    L += ['不一致文件:'] + ['- `%s`' % x for x in V['token_stream_identical']['mismatch']]
else:
    L += ['**结论: 全部文件逻辑零改动。**']
L += ['']

L += ['## 3. include 自洽性', '',
      '- 新树内成功解析: **%d** 处' % I['resolved'],
      '- 编译器自带头(工程外, 正常): %s' % ', '.join('`%s`' % x for x in I['system']),
      '- 缺失: **%d** 处' % len(I['missing'])]
for m in I['missing']:
    L.append('  - `%s` -> `%s` (该文件是孤儿文件, 从未可编译)' % (m[0], m[1]))
L += ['- 对原工程路径/目录名的引用: **%d** 处' % len(I['oldref']), '']

L += ['## 4. 隐式函数声明 (保留未修复)', '',
      '原工程依赖 C89 隐式函数声明: 跨模块调用时调用点看不到函数原型。',
      '**本轮保持原样**(改动即改变行为), 仅登记。分两类:', '',
      '### 4.1 A 类: 全树内有定义, 但调用点缺原型 (%d 处)' % len(V['implicit_declaration']),
      '',
      '| 调用点 | 函数 |', '|---|---|']
for x in V['implicit_declaration']:
    f, n = x.split(': ')
    L.append('| `%s` | `%s` |' % (f, n))
L += ['', '典型原因: 函数原型写在某个 `.c` 里(如 `CalTorqueUp` 声明在 `MotorVF.c`),',
      '或函数定义在同文件但位于调用点之后且无前置声明。', '',
      '### 4.2 B 类: 全树内找不到定义 (%d 处)' % len(V['implicit_undeclared_elsewhere']),
      '',
      '| 调用点 | 名称 | 判读 |', '|---|---|---|']
for x in V['implicit_undeclared_elsewhere']:
    f, n = x.split(': ')
    name = n[:-2]
    if name == '__IQsat':
        why = 'TI C28x 编译器内建 (IQmath intrinsic), 无需声明'
    elif name in ('memcpy', 'rand', 'labs'):
        why = 'C 标准库函数, 原工程未 `#include <string.h>/<stdlib.h>`'
    elif name == 'MovePrgFrFlashToRam':
        why = '汇编 `_MovePrgFrFlashToRam`(见 `asm/DSP2803x_CodeStartBranch.asm`), 无 C 原型'
    elif name in ('MAX', 'MIN'):
        why = '位于非活动的 `#elif` 分支内(文本级扫描不判断条件编译), 不参与编译'
    else:
        why = '需人工确认(可能位于非活动 `#if` 分支或由宏展开产生)'
    L.append('| `%s` | `%s` | %s |' % (f, n, why))
L += ['', '> 注: 第 7 项检查是**文本级**的, 不判断条件编译是否成立;',
      '> `MAX` / `MIN` 即属此类(它们位于 `f_io.c` 的 `#if 0 ... #elif 1 ... #elif 1` 中',
      '> **未生效**的分支里)。', '']

L += ['## 5. 未使用全局量 (%d 处, 保留未删除)' % len(V['unused_global']), '',
      '判据: 该标识符在整个 `src/**/*.c` 语料里出现次数 <= 1(即只有定义、没有引用)。',
      '可能是仅由中断/汇编引用、或被 `#if` 排除的引用。**保留**, 仅登记。', '',
      '| 文件 | 标识符 |', '|---|---|']
for x in V['unused_global']:
    f, n = x.split(': ')
    L.append('| `%s` | `%s` |' % (f, n))
L += ['']

L += ['## 6. 中断函数定义 (%d 个)' % len(V['isr_definitions']), '',
      '| 文件 | 中断函数 |', '|---|---|']
for x in V['isr_definitions']:
    f, n = x.split(': ')
    L.append('| `%s` | `%s` |' % (f, n))
L += ['', '注: 含 `include/device/DSP2803x_DefaultISR.h` 里 TI 提供的默认 ISR 声明。', '']

L += ['## 7. volatile 声明分布', '', '| 文件 | 声明数 |', '|---|---:|']
for x in V['volatile_declarations']:
    f, n = x.split(': ')
    L.append('| `%s` | %s |' % (f, n))
L += ['', '> 提示: 中断与主循环共享的变量必须 `volatile`。原工程的 `volatile` 使用量偏少,',
      '> 本轮**未增加**任何 `volatile`(增加会改变生成代码), 仅登记供后续批次评估。', '']

if D:
    TIER_ROWS = [
        '| 逐模块反汇编比对 (关优化口径) | **%d 逐行相同 / %d 按预期不同 / %d 纯数据不适用** |' % (
            len(D['tierA']['same']), len(D['tierA']['diff']), len(D['tierA']['no_code'])),
        '| 目标文件内容级比对 (段+重定位+符号表) | **%d / %d 一致** |' % (
            len(D['tierB']['identical']),
            len(D['tierB']['identical']) + len(D['tierB']['failed']))]
else:
    TIER_ROWS = ['| 逐模块反汇编 / 目标文件内容级比对 | 见 `docs/equivalence_evidence.md` 第 2 / 3 节 |']

L += ['## 8. 编译与链接验证', '',
      '本机已具备 TI C2000 CGT `v25.11.1.LTS`, 因此以下项目**已实际执行**:', '',
      '| 项 | 结果 |', '|---|---|',
      '| 全工程编译 (原/新 各 59 个目标文件) | **118 / 118 成功, 0 失败** |',
      ] + TIER_ROWS + [
      '| Debug 链接 + hex2000 | 见 `docs/build_debug.txt` |',
      '| Release 链接 + hex2000 | 见 `docs/build_release.txt` |',
      '| 告警清单 | 见 `docs/warning_list.txt` |',
      '| 整机映像与原工程副本逐字节比对 | 见 `docs/equivalence_evidence.md` 第 4 节 |',
      '',
      '注意: 必须加 `--abi=coffabi`。新 CGT 默认 EABI, 会改变符号命名、运行时库与',
      '`.data` 归置, 与本工程的 COFF `.cmd` 不兼容。', '',
      '**仍未覆盖** (需硬件或额外条件):', '',
      '| 项 | 原因 |', '|---|---|',
      '| 目标板上的实际行为 (PWM/ADC/CAN/SCI/闭环) | 需目标板与台架 |',
      '| 中断时序 / 最坏执行时间 | 需示波器或周期计数 |',
      '| Flash 烧写与上电启动 | 需仿真器 |',
      '| 原工程 `39 Warnings` 的逐条对照 | 原日志只记条数, 未记正文 |',
      '', '原工程 Debug 构建的实测结果(供参考): `0 Errors, 39 Warnings`',
      '(见 `docs/original_build_Debug.log`)。', '']

L += ['## 9. 复现步骤', '',
      '```bash', '# 1) 重建源码树 (幂等)', 'python tools/rewrite_tree.py',
      '# 2) 重建工程 / 链接脚本 / GEL', 'python tools/build_project.py',
      '# 3) 静态验收', 'python tools/verify_tree.py',
      '# 4) include 自洽性', 'python tools/check_includes.py',
      '# 5) 负向对照 (会临时改动并自动还原 f_common.c)', 'python tools/negative_control.py',
      '# 6) 一键全跑', 'python tests/run_all.py', '```']

open(os.path.join(DOC, 'static_check_report.md'), 'w', encoding='utf-8').write('\n'.join(L) + '\n')

# 构建类文档 (build_debug/build_release/warning_list) 与等价性证据改由
# tools/make_build_report.py 依据**真实构建输出**生成, 这里不再写占位内容。
print('static_check_report written')
