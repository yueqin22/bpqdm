import os, re, json, collections, hashlib

SRC = r"E:/software/bpqdm/bpqym"
OUT = r"E:/software/bpqdm/bpqdm_rewrite/docs"

A = json.load(open(os.path.join(OUT, '_audit.json'), encoding='utf-8'))
rows = A['rows']
built = set(A['built'])

# include search order as configured in jt380.pjt (Debug & Release identical)
INCPATH = [
    'public/28035_header',
    'public/motor_header',
    'public/function_header',
]
# implicit: directory of the including file; plus CGT default include dirs (unavailable)

SRC_FILES = {r['rel'] for r in rows if r['ext'] in ('.c', '.h', '.asm')}
by_rel = {r['rel']: r for r in rows}
by_name = collections.defaultdict(list)
for r in rows:
    by_name[r['name']].append(r['rel'])


def resolve(cur_rel, inc):
    """Resolve an #include to a project-relative path, mimicking CCS 3.3."""
    cands = []
    if inc.startswith('"'):
        pass
    curdir = os.path.dirname(cur_rel)
    cands.append(os.path.normpath(os.path.join(curdir, inc)).replace('\\', '/'))
    for p in INCPATH:
        cands.append(os.path.normpath(os.path.join(p, inc)).replace('\\', '/'))
    for c in cands:
        if c in SRC_FILES:
            return c
    return None


INC_RE = re.compile(r'^\s*#\s*include\s+([<"])([^">]+)[">]', re.M)


def read(rel):
    raw = open(os.path.join(SRC, rel), 'rb').read()
    for enc in ('utf-8', 'gbk', 'gb18030'):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            pass
    return raw.decode('utf-8', 'surrogateescape')


# ---------------------------------------------------------------- dependency
inc_map = {}
for r in rows:
    if r['ext'] not in ('.c', '.h', '.asm'):
        continue
    txt = read(r['rel'])
    lst = []
    for m in INC_RE.finditer(txt):
        quote, inc = m.group(1), m.group(2)
        if quote == '<' and (inc.startswith('DSP') or inc.startswith('IQ') or inc.startswith('SFO')):
            pass
        res = resolve(r['rel'], inc)
        lst.append((inc, res))
    inc_map[r['rel']] = lst

unresolved = collections.Counter()
for rel, lst in inc_map.items():
    for inc, res in lst:
        if res is None:
            unresolved[inc] += 1

rev = collections.defaultdict(set)
for rel, lst in inc_map.items():
    for inc, res in lst:
        if res:
            rev[res].add(rel)

L = ['# 模块依赖 (module_dependency)', '',
     '## 1. include 搜索顺序 (来自 jt380.pjt)', '']
for i, p in enumerate(INCPATH, 1):
    L.append('%d. `%s`' % (i, p))
L += ['', '此外, `#include "..."` 会先搜索**包含者所在目录**。', '',
      '## 2. 每个源文件的直接依赖 (已按上述顺序解析到实际文件)', '',
      '| 文件 | 包含的头文件 (解析结果) |', '|---|---|']
for rel in sorted(inc_map):
    items = []
    for inc, res in inc_map[rel]:
        if res:
            items.append('`%s`' % res)
        else:
            items.append('`%s` *(未解析: 编译器自带/库头)*' % inc)
    L.append('| `%s` | %s |' % (rel, '<br>'.join(items) if items else '-'))

L += ['', '## 3. 头文件被谁包含 (反向依赖)', '', '| 头文件 | 被包含次数 | 包含者 |', '|---|---:|---|']
for h in sorted(rev, key=lambda x: -len(rev[x])):
    L.append('| `%s` | %d | %s |' % (h, len(rev[h]), '<br>'.join(sorted(rev[h]))))

L += ['', '## 4. 未解析的 include (编译器自带头 / 库头)', '', '| 头文件 | 出现次数 |', '|---|---:|']
for inc, c in unresolved.most_common():
    L.append('| `%s` | %d |' % (inc, c))

open(os.path.join(OUT, 'module_dependency.md'), 'w', encoding='utf-8').write('\n'.join(L) + '\n')

# ---------------------------------------------------------------- symbols
DECL_RE = re.compile(
    r'^(?P<pre>(?:extern\s+|static\s+|volatile\s+|const\s+|far\s+|interrupt\s+|near\s+)*)'
    r'(?P<type>[A-Za-z_]\w*(?:\s*\*)?)\s+'
    r'(?P<name>[A-Za-z_]\w*)\s*(?P<arr>\[[^\]]*\])?\s*(?P<tail>[=;,])', re.M)

KEYWORDS = {'if', 'for', 'while', 'switch', 'return', 'else', 'do', 'case',
            'sizeof', 'typedef', 'struct', 'union', 'enum', 'goto', 'break',
            'continue', 'defined'}

FUNC_RE = re.compile(
    r'^\s*(?P<pre>(?:static\s+|interrupt\s+|volatile\s+|inline\s+|void\s+)*)'
    r'(?P<ret>[A-Za-z_]\w*(?:\s*\*)?)\s+'
    r'(?P<name>[A-Za-z_]\w*)\s*\((?P<args>[^;{)]*(?:\([^)]*\)[^;{)]*)*)\)\s*(?:\{|$)', re.M)

globals_by_file = collections.defaultdict(list)
funcs_by_file = collections.defaultdict(list)

for r in rows:
    if r['ext'] not in ('.c', '.h'):
        continue
    txt = read(r['rel'])
    txt_nc = re.sub(r'/\*.*?\*/', ' ', txt, flags=re.S)
    txt_nc = re.sub(r'//[^\n]*', ' ', txt_nc)
    for m in DECL_RE.finditer(txt_nc):
        name = m.group('name')
        pre = m.group('pre')
        typ = m.group('type')
        if name in KEYWORDS or typ in KEYWORDS:
            continue
        if m.group('tail') == ',':
            continue
        if m.start() > 0 and txt_nc[m.start() - 1] not in '\n;}{':
            continue
        ext = 'extern' in pre
        globals_by_file[r['rel']].append((name, typ, 'extern' if ext else 'def',
                                          'static' if 'static' in pre else ''))
    for m in FUNC_RE.finditer(txt_nc):
        nm = m.group('name')
        if nm in KEYWORDS:
            continue
        funcs_by_file[r['rel']].append((nm, m.group('ret'), 'static' if 'static' in m.group('pre') else ''))

G = ['# 全局符号清单 (global_symbol_list)', '',
     '说明: 由源码静态扫描得到 (已剥离注释)。`def` = 有定义的全局量, `extern` = 仅声明。', '',
     '## 1. 全局变量定义 (按文件)', '',
     '| 文件 | 符号 | 类型 | 属性 |', '|---|---|---|---|']
defs = collections.defaultdict(list)
for rel in sorted(globals_by_file):
    for name, typ, kind, st in globals_by_file[rel]:
        defs[name].append(rel)
        G.append('| `%s` | `%s` | `%s` | %s |' % (rel, name, typ, (kind + (' ' + st if st else '')).strip()))

dupdef = {k: v for k, v in defs.items() if len(set(v)) > 1}
G += ['', '## 2. 同名符号跨文件出现', '', '| 符号 | 出现文件 |', '|---|---|']
for k, v in sorted(dupdef.items()):
    G.append('| `%s` | %s |' % (k, '<br>'.join(sorted(set(v)))))
if not dupdef:
    G.append('| - | 无 |')

G += ['', '## 3. 函数清单 (按文件)', '', '| 文件 | 函数 | 返回类型 | 属性 |', '|---|---|---|---|']
for rel in sorted(funcs_by_file):
    for nm, ret, st in funcs_by_file[rel]:
        G.append('| `%s` | `%s` | `%s` | %s |' % (rel, nm, ret, st or '-'))
open(os.path.join(OUT, 'global_symbol_list.md'), 'w', encoding='utf-8').write('\n'.join(G) + '\n')

# ---------------------------------------------------------------- interrupts
ISR_NAME = re.compile(r'\b([A-Za-z_]\w*ISR)\s*\(')
PIE_REF = re.compile(r'\b(PieVectTable\.[A-Za-z_]\w*|EALLOW|DINT|ERTM|EDIS)\b')
isr_files = collections.defaultdict(set)
isr_defs = collections.defaultdict(list)
for r in rows:
    if r['ext'] not in ('.c', '.h', '.asm'):
        continue
    txt = read(r['rel'])
    for m in ISR_NAME.finditer(txt):
        isr_files[r['rel']].add(m.group(1))
    if r['ext'] == '.c':
        for m in re.finditer(r'^\s*interrupt\s+void\s+([A-Za-z_]\w*)', txt, re.M):
            isr_defs[r['rel']].append(m.group(1))
    if r['ext'] == '.asm':
        for m in re.finditer(r'^(_?[A-Za-z_]\w*ISR)\s*:', txt, re.M):
            isr_defs[r['rel']].append(m.group(1))

I = ['# 中断清单 (interrupt_list)', '',
     '## 1. 中断向量表 (来自 system/DSP2803x_CodeStartBranch.asm 与 PIE 初始化)', '',
     '入口段: `codestart` -> `BEGIN (0x3F7FF6)`; PIE 向量表位于 `PieVectTable` 结构。', '',
     '## 2. 出现的 ISR 符号 (按文件)', '', '| 文件 | ISR 符号 |', '|---|---|']
for rel in sorted(isr_files):
    I.append('| `%s` | %s |' % (rel, '<br>'.join('`%s`' % s for s in sorted(isr_files[rel]))))
I += ['', '## 3. ISR 函数定义', '', '| 文件 | 定义 |', '|---|---|']
for rel in sorted(isr_defs):
    I.append('| `%s` | %s |' % (rel, '<br>'.join('`%s`' % s for s in isr_defs[rel])))

I += ['', '## 4. PIE 向量赋值 (PieVectTable.X = &Func)', '', '| 文件 | 赋值 |', '|---|---|']
for r in rows:
    if r['ext'] != '.c':
        continue
    txt = read(r['rel'])
    hits = re.findall(r'(PieVectTable\s*\.\s*\w+\s*=\s*[^;]+;)', txt)
    if hits:
        I.append('| `%s` | %s |' % (r['rel'], '<br>'.join('`%s`' % h.strip() for h in hits)))
open(os.path.join(OUT, 'interrupt_list.md'), 'w', encoding='utf-8').write('\n'.join(I) + '\n')

print('dependency written; unresolved includes:', len(unresolved))
print('isr files:', len(isr_files))
