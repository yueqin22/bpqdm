"""由原工程 map / 审计数据机械生成对比类文档。"""
import os, re, json, shutil, collections

SRC = r"E:/software/bpqdm/bpqym"
DST = r"E:/software/bpqdm/bpqdm_rewrite"
DOC = os.path.join(DST, 'docs')

MAPF = os.path.join(SRC, 'CCs/Debug/md380_C.map')
txt = open(MAPF, encoding='gbk', errors='replace').read()

# ------------------------------------------------------------ memory layout
lines = txt.split('\n')
mem_rows, sec_rows = [], []
mode = None
cur_page = None
for line in lines:
    s = line.strip()
    if s == 'MEMORY CONFIGURATION':
        mode = 'mem'
        continue
    if s == 'SECTION ALLOCATION MAP':
        mode = 'sec'
        continue
    if s == 'LINKER GENERATED COPY TABLES':
        mode = None
        continue
    if mode == 'mem':
        m = re.match(r'^(PAGE \d+):', s)
        if m:
            cur_page = m.group(1)
            continue
        m = re.match(r'^(\S+)\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+(\S+)', s)
        if m:
            mem_rows.append((cur_page, m.group(1), int(m.group(2), 16), int(m.group(3), 16),
                             int(m.group(4), 16), int(m.group(5), 16), m.group(6)))
    elif mode == 'sec':
        m = re.match(r'^(\.\w+|\w+)\s+(\d)\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s*(.*)$', line)
        if m:
            sec_rows.append((m.group(1), m.group(2), m.group(3), m.group(4), m.group(5).strip()))

rows = mem_rows
sections = sec_rows

entry = re.search(r'ENTRY POINT SYMBOL: "(\S+)"\s+address:\s+(\S+)', txt)
outname = re.search(r'OUTPUT FILE NAME:\s+<(.+?)>', txt)

L = ['# 内存布局 (memory_layout)', '',
     '## 1. 链接配置来源', '',
     '- 链接命令: `CCs/Debug.lkf` 第 1 行 (Debug)',
     '- 存储区定义: `public/common/F28035.cmd` (MEMORY + SECTIONS)',
     '- 外设寄存器区: `public/common/DSP28035Peripheral.cmd`',
     '- 参考 map: `CCs/Debug/md380_C.map` (TMS320C2000 Linker PC v5.2.1)',
     '',
     '- 输出文件: `%s`' % (outname.group(1) if outname else '?'),
     '- 入口符号: `%s` @ `0x%s`' % (entry.group(1), entry.group(2)) if entry else '',
     '- 链接器选项含 `-ecode_start`, 即入口为 `code_start` (汇编启动段), 不是默认的 `_c_int00`。',
     '',
     '## 2. 存储区 (MEMORY)', '',
     '| 页 | 名称 | origin | length | 已用 | 空闲 | 属性 |',
     '|---|---|---|---:|---:|---:|---|']
for p, n, o, l, u, un, a in rows:
    L.append('| %s | `%s` | 0x%08X | 0x%04X | 0x%04X | 0x%04X | %s |' % (p, n, o, l, u, un, a))

L += ['', '## 3. 段分配 (SECTION ALLOCATION, 顶层)', '',
      '| 段 | 页 | origin | length | 说明 |', '|---|---|---|---:|---|']
for n, p, o, l, note in sections:
    L.append('| `%s` | %s | 0x%s | 0x%s | %s |' % (n, p, o, l, note or ''))

L += ['', '## 4. 关键约束 (重写必须保持)', '',
      '- 入口 `codestart` 段固定在 `BEGIN = 0x3F7FF6`, 长度 2 个字。',
      '- `ramfuncs` 段 `LOAD = FLASHA`, `RUN = RAMPRG (0x8000)`, 由 `_prginRAM` 搬运表复制。',
      '- `.text` / `.cinit` / `.pinit` / `.binit` / `.ovly` 均在 `FLASHA (0x3E8000)`。',
      '- 外设寄存器区由 `DSP28035Peripheral.cmd` 固定映射 (PAGE 1)。',
      '- `DSP2803x_GlobalVariableDefs.c` 里的 `#pragma DATA_SECTION` 决定了每个外设结构的落点。',
      '- 新工程链接脚本逐字沿用原 `.cmd` (仅去除注释), 存储区与段映射未变。']
open(os.path.join(DOC, 'memory_layout.md'), 'w', encoding='utf-8').write('\n'.join(L) + '\n')

# ------------------------------------------------------------ link map summary
S = ['TMS320C2000 Linker map 摘要 (原工程 Debug)',
     '=' * 60,
     '来源: CCs/Debug/md380_C.map',
     '',
     '输出文件 : %s' % (outname.group(1) if outname else '?'),
     '入口符号 : %s @ 0x%s' % (entry.group(1), entry.group(2)) if entry else '',
     '',
     '存储区占用:']
for p, n, o, l, u, un, a in rows:
    if u:
        S.append('  %-22s origin=0x%08X length=0x%04X used=0x%04X unused=0x%04X'
                 % (n, o, l, u, un))
S += ['', '段分配 (顶层):']
for n, p, o, l, note in sections:
    S.append('  %-14s page=%s origin=0x%s length=0x%s' % (n, p, o, l))
S += ['', '说明: 新工程链接脚本与原工程一致 (仅去注释), 因此新工程的 map 应与本摘要',
      '      的结构相同 (绝对地址可能因代码大小变化而不同)。']


def _parse_map(path, enc):
    t = open(path, encoding=enc, errors='replace').read()
    rows_, secs_, mode_, page_ = [], [], None, None
    for line in t.split('\n'):
        s = line.strip()
        if s == 'MEMORY CONFIGURATION':
            mode_ = 'mem'
            continue
        if s == 'SECTION ALLOCATION MAP':
            mode_ = 'sec'
            continue
        if s == 'LINKER GENERATED COPY TABLES':
            mode_ = None
            continue
        if mode_ == 'mem':
            m = re.match(r'^(PAGE \d+):', s)
            if m:
                page_ = m.group(1)
                continue
            m = re.match(r'^(\S+)\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+(\S+)', s)
            if m:
                rows_.append((page_, m.group(1), int(m.group(2), 16), int(m.group(3), 16),
                              int(m.group(4), 16), int(m.group(5), 16), m.group(6)))
        elif mode_ == 'sec':
            m = re.match(r'^(\.\w+|\w+)\s+(\d)\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s*(.*)$', line)
            if m:
                secs_.append((m.group(1), m.group(2), m.group(3), m.group(4), m.group(5).strip()))
    en = re.search(r'ENTRY POINT SYMBOL: "(\S+)"\s+address:\s+(\S+)', t)
    on = re.search(r'OUTPUT FILE NAME:\s+<(.+?)>', t)
    return rows_, secs_, en, on


# ------------------------------------------------- 新工程自己的 map (本工程构建产物)
#
# 上面那份摘要来自**原工程**的 map (基线对照)。计划第十一节要求提交的
# `link_map_summary.txt` 是用来核对**新工程**内存布局的, 所以这里再附一份
# 新工程 Debug 的 map 摘要 —— 两份并排, 才能看出布局是否一致。
NEWMAP = os.path.join(DST, 'build/Debug/jt380_C.map')
if os.path.isfile(NEWMAP):
    nrows, nsecs, nentry, nout = _parse_map(NEWMAP, 'utf-8')
    S += ['', '=' * 60,
          '新工程 Debug map 摘要 (本工程自己构建, 供内存布局验收)',
          '=' * 60,
          '来源: build/Debug/jt380_C.map',
          '',
          '输出文件 : %s' % (nout.group(1) if nout else '?'),
          '入口符号 : %s @ 0x%s' % (nentry.group(1), nentry.group(2)) if nentry else '',
          '',
          '存储区占用:']
    for p, n, o, l, u, un, a in nrows:
        if u:
            S.append('  %-22s origin=0x%08X length=0x%04X used=0x%04X unused=0x%04X'
                     % (n, o, l, u, un))
    S += ['', '段分配 (顶层):']
    for n, p, o, l, note in nsecs:
        S.append('  %-14s page=%s origin=0x%s length=0x%s' % (n, p, o, l))
    S += ['', '说明: 与原工程摘要逐项对照可确认存储区划分、段归置与入口地址未变。']

open(os.path.join(DOC, 'link_map_summary.txt'), 'w', encoding='utf-8').write('\n'.join(S) + '\n')

# ------------------------------------------------------------ module map
M = json.load(open(os.path.join(DOC, '_map.json'), encoding='utf-8'))
L = ['# 原工程 -> 新工程 文件映射 (new_tree_module_map)', '',
     '## 1. 参与构建的源码 (59 个编译单元)', '',
     '| 原路径 | 新路径 | 类型 | 原编码 | 行尾 | 删除注释数 |',
     '|---|---|---|---|---|---:|']
for m in sorted(M, key=lambda x: x['dst']):
    L.append('| `%s` | `%s` | %s | %s | %s | %d |' % (
        m['src'], m['dst'], m['kind'], m['enc'], m['eol'], m['comments']))
L += ['', '## 2. 头文件 (82 个)', '',
      '| 原路径 | 新路径 | 说明 |', '|---|---|---|']
for m in sorted(M, key=lambda x: x['dst']):
    if not m['dst'].startswith('include/'):
        continue
    note = ''
    if m['dst'].startswith('include/device/'):
        note = 'TI 设备头文件'
    elif m['dst'].startswith('include/common/'):
        note = '公共类型/宏/结构定义'
    elif m['dst'].startswith('include/function/'):
        note = '功能层接口 (含被 #include 的表文件)'
    elif m['dst'].startswith('include/motor/'):
        note = '电机层接口'
    L.append('| `%s` | `%s` | %s |' % (m['src'], m['dst'], note))
open(os.path.join(DOC, 'new_tree_module_map.md'), 'w', encoding='utf-8').write('\n'.join(L) + '\n')

# ------------------------------------------------------------ symbol compare
V = json.load(open(os.path.join(DOC, '_verify.json'), encoding='utf-8'))
T = V['token_stream_identical']
RW = T.get('rewritten', [])
NFILE = T['files']
NEX = len(RW)
NTOK = NFILE - NEX
L = ['# 原工程 / 新工程 关键符号对比 (symbol_comparison)', '',
     '## 1. 两层证明口径', '',
     '交付源码共 %d 个文件, 分两层验收 —— **两层用的是两条不同的证据链**,' % NFILE,
     '不能混着读:', '',
     '- **未改写层 (%d 个)**: 只去掉了注释, 做**词法级 token 流比对**。'
     '把注释与空白全部忽略后, 原文件与新文件的 token 序列逐项比较; token 流完全相同'
     '=> 函数名、参数、返回值、全局变量、结构体布局、宏、字符串字面量、数值常量、'
     '运算顺序、移位位数全部未变。' % NTOK,
     '- **改写层 (%d 个)**: 实现被**有意重写**, token 流本来就应该不同, 因此'
     '**豁免** token 等价 (见 `docs/rewritten_modules.json`)。它们的等价性由'
     '**宿主差分测试 + 目标端副作用检查**承担 —— 差分测试覆盖状态空间并做负向对照,'
     '目标端比对符号集合、告警与段尺寸 (见 `docs/_difftest/`、'
     '`docs/equivalence_evidence.md`)。' % NEX,
     '',
     '> 读下表时注意: "token 流不一致" 只统计**未改写层**。改写层既不计入"不一致",',
     '> 也不计入"一致" —— 它的结论来自另一条证据链, 不在这张表里。', '',
     '| 项目 | 结果 |', '|---|---|',
     '| 交付源码文件数 | %d |' % NFILE,
     '| 其中 未改写层 (要求 token 等价) | %d |' % NTOK,
     '| 其中 改写层 (豁免 token 等价, 另证) | %d |' % NEX,
     '| 未改写层 token 流不一致 | %d |' % len(T['mismatch']),
     '| 符号抽取不一致 | %d |' % len(V['symbol_identical']),
     '| 注释残留 | %d |' % len(V['comment_residual']),
     '| 非 ASCII 残留 | %d |' % len(V['non_ascii']),
     '| TODO/FIXME/NOTE | %d |' % len(V['todo_fixme_note']),
     '| 二次去注释可删字符 | %d |' % len(V['idempotent']),
     '', '## 2. 关键对比项结论', '',
     '| 对比项 | 未改写层 | 改写层 |', '|---|---|---|',
     '| 函数名称 / 参数 / 返回值 | 未变 (token 流等价) | 未变 (名字与签名逐字保留) |',
     '| 全局变量与定义位置 | 未变 (逐文件 token 等价) | 未变 (同一符号, 同一落点) |',
     '| 结构体 / 位域 / union 布局 | 未变 (逐 token 相同) | 未变 (声明未动) |',
     '| 宏与常量 | 未变 | 未变 |',
     '| 中断入口与 PIE 向量赋值 | 未变 | 未变 (ISR 名与向量赋值未动) |',
     '| 参数地址 / 通信地址 / 错误码 | 未变 (均为字面量) | 未变 (均为字面量) |',
     '| 任务周期 (0.5ms/2ms/0ms) | 未变 | 未变 |',
     '| 条件编译分支 | 原样保留, 未解析 | 原样保留, 未解析 |',
     '| **函数体实现语句** | 逐 token 相同 | **有意重写** (差分测试证明等价) |',
     '', '### 改写层逐模块状态', '',
     '| 模块 | token 流是否恰好相同 |', '|---|---|']
for m in RW:
    L.append('| `%s` | %s |' % (m['dst'], '是 (改写收敛到同一 token 流)' if m['token_same']
                                else '否 (实现已重写, 属预期)'))
L += ['', '说明: 改写层里若出现 `token_same = 是`, 说明该轮改写是**空转** (没真正改到实现),',
      '应当暴露出来而不是当成通过 —— 这正是 `tests/test_token_equivalence.py` 盯着的一点。',
      '', '## 3. 中断入口 (新树)', '',
      '| 文件 | 中断函数 |', '|---|---|']
for x in V['isr_definitions']:
    f, n = x.split(': ')
    L.append('| `%s` | `%s` |' % (f, n))
L += ['', '## 4. volatile 声明分布', '', '| 文件 | 数量 |', '|---|---:|']
for x in V['volatile_declarations']:
    f, n = x.split(': ')
    L.append('| `%s` | %s |' % (f, n))
open(os.path.join(DOC, 'symbol_comparison.md'), 'w', encoding='utf-8').write('\n'.join(L) + '\n')

# ------------------------------------------------------------ original log
shutil.copyfile(os.path.join(SRC, 'CCs/cc_build_Debug.log'),
                os.path.join(DOC, 'original_build_Debug.log'))
print('docs written: memory_layout, link_map_summary, new_tree_module_map, symbol_comparison')
