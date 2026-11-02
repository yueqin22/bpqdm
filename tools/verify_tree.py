"""静态验收: token 流等价 / 注释零残留 / 非 ASCII 零残留 / 幂等 / 符号一致性 /
   隐式声明 / 未使用全局量 / 中断与 volatile 检查。"""
import os, re, sys, json, collections, hashlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cstrip as C

SRC = r"E:/software/bpqdm/bpqym"
DST = r"E:/software/bpqdm/bpqdm_rewrite"
DOC = os.path.join(DST, 'docs')

MAP = json.load(open(os.path.join(DOC, '_map.json'), encoding='utf-8'))
# 只扫交付源码/工程/文档; build/ 是生成物目录 (含原工程只读副本), 必须排除。
NEWFILES = C.walk_delivered(DST)

# 固件源码的一级目录。"注释零残留 / 纯 ASCII" 这类**交付契约**只约束固件源码,
# 不约束 tools/ 与 tests/ —— 那些是工程脚手架, 里面的 .c 是**宿主测试台**,
# 写中文注释完全正当。把两者混在一起查, 会把"测试台里有中文"报成
# "交付源码有非 ASCII 字节", 数字看着像缺陷, 其实是查错了对象。
FW_TOP_DIRS = ('src', 'include', 'asm', 'linker', 'project')


def is_fw(rel):
    return rel.split('/', 1)[0] in FW_TOP_DIRS

# 「实现被改写过的模块」清单。未列入的模块必须与原工程 token 流一致;
# 列入的模块豁免 token 等价, 改由差分测试 + 目标端副作用检查覆盖。
REWRITTEN = set()
_rw = os.path.join(DOC, 'rewritten_modules.json')
if os.path.isfile(_rw):
    REWRITTEN = set(m['dst'] for m in
                    json.load(open(_rw, encoding='utf-8'))['modules'])

SRC_TEXT = {m['dst']: m['src'] for m in MAP}
report = collections.OrderedDict()


_RCACHE = {}


def read_text(path):
    if path in _RCACHE:
        return _RCACHE[path]
    raw = open(path, 'rb').read()
    t, enc = C.decode_any(raw)
    _RCACHE[path] = (t, enc)
    return t, enc


# ---------------------------------------------------------- 1. token 等价
eq_fail = []
rewritten_seen = []
for m in MAP:
    a, _ = read_text(os.path.join(SRC, m['src']))
    b, _ = read_text(os.path.join(DST, m['dst']))
    if m['kind'] in ('c', 'h'):
        same = C.c_tokens(a) == C.c_tokens(b)
    else:
        ka = [re.sub(r';.*$', '', l).strip() for l in a.replace('\r\n', '\n').split('\n')]
        ka = [l for l in ka if l and not l.startswith('*')]
        kb = [l.strip() for l in b.replace('\r\n', '\n').split('\n') if l.strip()]
        same = ka == kb
    if m['dst'] in REWRITTEN:
        # 改写过的模块本来就应该不一样; 若反而相同, 说明这轮改写是空转, 要暴露出来
        rewritten_seen.append({'dst': m['dst'], 'token_same': same})
    elif not same:
        eq_fail.append(m['dst'])
report['token_stream_identical'] = {'files': len(MAP), 'mismatch': eq_fail,
                                    'rewritten': rewritten_seen}

# ---------------------------------------------------------- 2. 注释残留
COMMENT_MARK = re.compile(r'/\*|//|(^|\n)\s*\*[^*\n]')
residual = []
for m in MAP:
    p = os.path.join(DST, m['dst'])
    t, _ = read_text(p)
    if m['kind'] == 'asm':
        bad = [l for l in t.split('\n') if ';' in l or l.startswith('*')]
    else:
        bad = [l for l in t.split('\n') if '/*' in l or '//' in l]
    if bad:
        residual.append((m['dst'], bad[:2]))
report['comment_residual'] = residual

# ---------------------------------------------------------- 3. TODO / FIXME
KEY = re.compile(r'\b(TODO|FIXME|NOTE|XXX|HACK)\b')
kw = []
for rel in NEWFILES:
    if not is_fw(rel) or not rel.endswith(('.c', '.h', '.asm', '.cmd', '.pjt', '.lkf', '.gel')):
        continue
    t, _ = read_text(os.path.join(DST, rel))
    for i, l in enumerate(t.split('\n'), 1):
        if KEY.search(l):
            kw.append('%s:%d: %s' % (rel, i, l.strip()[:80]))
report['todo_fixme_note'] = kw

# ---------------------------------------------------------- 4. 非 ASCII
# 按**字节**判定: 逐个找出 > 0x7F 的字节并报出位置与个数。原来用
# raw.decode('ascii') 捕异常, 一是把"整文件是 UTF-8 中文"和"混进了个别非 ASCII
# 字节"报成同一条消息, 二是异常文本只给第一个字节的偏移, 看不出到底有几处。
nonascii = []
for rel in NEWFILES:
    if not is_fw(rel) or not rel.endswith(('.c', '.h', '.asm', '.cmd', '.pjt', '.lkf', '.gel')):
        continue
    raw = open(os.path.join(DST, rel), 'rb').read()
    bad = [i for i, b in enumerate(raw) if b > 0x7F]
    if bad:
        nonascii.append('%s: %d 处非 ASCII 字节, 首个 @0x%X = 0x%02X'
                        % (rel, len(bad), bad[0], raw[bad[0]]))
report['non_ascii'] = nonascii

# ---------------------------------------------------------- 5. 幂等
idem = []
for m in MAP:
    p = os.path.join(DST, m['dst'])
    t, _ = read_text(p)
    if m['kind'] == 'asm':
        s, n = C.strip_asm(t)
    else:
        s, n = C.strip_c(t)
    if n != 0:
        idem.append((m['dst'], n))
report['idempotent'] = idem

# ---------------------------------------------------------- 6. 符号一致性
DECL_RE = re.compile(
    r'^(?P<pre>(?:extern\s+|static\s+|volatile\s+|const\s+|far\s+|interrupt\s+)*)'
    r'(?P<type>[A-Za-z_]\w*(?:\s*\*)?)\s+(?P<name>[A-Za-z_]\w*)\s*'
    r'(?P<arr>\[[^\]]*\])?\s*(?P<tail>[=;,])', re.M)
FUNC_RE = re.compile(
    r'^\s*(?P<pre>(?:static\s+|interrupt\s+|volatile\s+|inline\s+|void\s+|'
    r'LOCALD\s+|LOCALF\s+|const\s+|extern\s+)*)'
    r'(?P<ret>[A-Za-z_]\w*(?:\s*\*)?)\s+(?P<name>[A-Za-z_]\w*)\s*'
    r'\((?P<args>[^;{)]*(?:\([^)]*\)[^;{)]*)*)\)\s*(?:\{|$)', re.M)
KW = {'if', 'for', 'while', 'switch', 'return', 'else', 'do', 'case', 'sizeof',
      'typedef', 'struct', 'union', 'enum', 'goto', 'break', 'continue', 'defined'}


def syms(t):
    """剥离注释并折叠空白后再抽取符号, 保证原/新两侧的判据完全一致。"""
    t = re.sub(r'/\*.*?\*/', ' ', t, flags=re.S)
    t = re.sub(r'//[^\n]*', ' ', t)
    t = re.sub(r'"(?:[^"\\]|\\.)*"', '""', t)
    t = re.sub(r'\s+', ' ', t)
    g, f = [], []
    for m in DECL_RE.finditer(t):
        if m.group('name') in KW or m.group('type') in KW or m.group('tail') == ',':
            continue
        if m.start() > 0 and t[m.start() - 1] not in ' ;}{':
            continue
        g.append(m.group('name'))
    for m in FUNC_RE.finditer(t):
        if m.group('name') in KW:
            continue
        f.append((m.group('name'), m.group('args')))
    return set(g), f


sym_fail = []
for m in MAP:
    if m['kind'] == 'asm':
        continue
    a, _ = read_text(os.path.join(SRC, m['src']))
    b, _ = read_text(os.path.join(DST, m['dst']))
    ga, fa = syms(a)
    gb, fb = syms(b)
    if ga != gb:
        sym_fail.append((m['dst'], 'globals', sorted(ga ^ gb)[:6]))
    if [x[0] for x in fa] != [x[0] for x in fb]:
        sym_fail.append((m['dst'], 'functions', sorted(set(x[0] for x in fa) ^ set(x[0] for x in fb))[:6]))
report['symbol_identical'] = sym_fail

# ---------------------------------------------------------- 7. 隐式声明
# 收集每个 .c 可见的声明集合 (自身 + 传递包含的头文件)
INC = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)
INC_DIRS = ['include/device', 'include/common', 'include/function', 'include/motor']


def resolve(rel, inc):
    d = os.path.dirname(rel)
    for c in [os.path.normpath(os.path.join(d, inc)).replace('\\', '/')] + \
             [os.path.normpath(os.path.join(i, inc)).replace('\\', '/') for i in INC_DIRS]:
        if os.path.exists(os.path.join(DST, c)):
            return c
    return None


_VIS = {}


def visible(rel, seen=None):
    if seen is None:
        if rel in _VIS:
            return _VIS[rel]
        seen = set()
        visible(rel, seen)
        _VIS[rel] = seen
        return seen
    if rel in seen:
        return seen
    seen.add(rel)
    t, _ = read_text(os.path.join(DST, rel))
    for m in INC.finditer(t):
        r = resolve(rel, m.group(1))
        if r:
            visible(r, seen)
    return seen


CALL = re.compile(r'(?<![.>])\b([A-Za-z_]\w*)\s*\(')
PROTO = re.compile(
    r'^\s*(?:[A-Za-z_]\w*[ \t]+)+(?:\*[ \t]*)?([A-Za-z_]\w*)\s*\(', re.M)
MEMBER_FP = re.compile(r'\(\s*\*\s*([A-Za-z_]\w*)\s*\)')
MEMBER_V = re.compile(r'^\s+([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*;', re.M)
PPDIR = re.compile(r'^\s*#.*$', re.M)
BUILTIN = {'if', 'for', 'while', 'switch', 'return', 'sizeof', 'defined',
           'asm', 'va_arg', 'va_start', 'va_end'}

implicit = []
for m in MAP:
    if m['kind'] != 'c' or not m['dst'].startswith('src/'):
        continue
    rel = m['dst']
    t, _ = read_text(os.path.join(DST, rel))
    files = visible(rel)
    declared = set()
    macroish = set()
    for f in files:
        ft, _ = read_text(os.path.join(DST, f))
        fc = re.sub(r'/\*.*?\*/', ' ', ft, flags=re.S)
        fc = re.sub(r'//[^\n]*', ' ', fc)
        for mm in FUNC_RE.finditer(fc):
            declared.add(mm.group('name'))
        for mm in PROTO.finditer(fc):
            declared.add(mm.group(1))
        for mm in DECL_RE.finditer(fc):
            declared.add(mm.group('name'))
        for mm in MEMBER_FP.finditer(fc):
            declared.add(mm.group(1))
        for mm in MEMBER_V.finditer(fc):
            declared.add(mm.group(1))
        for mm in re.finditer(r'^\s*#\s*define\s+([A-Za-z_]\w*)', ft, re.M):
            macroish.add(mm.group(1))
        for mm in re.finditer(r'typedef[^;]*?([A-Za-z_]\w*)\s*;', fc):
            declared.add(mm.group(1))
    tc = re.sub(r'/\*.*?\*/', ' ', t, flags=re.S)
    tc = re.sub(r'//[^\n]*', ' ', tc)
    tc = re.sub(r'"(?:[^"\\]|\\.)*"', '""', tc)
    tc = PPDIR.sub('', tc)                     # 预处理器指令不参与调用分析
    for mm in CALL.finditer(tc):
        n = mm.group(1)
        if n in BUILTIN or n in declared or n in macroish or n in KW:
            continue
        implicit.append('%s: %s()' % (rel, n))
implicit = sorted(set(implicit))

# 分类: A = 全树内有定义但调用点看不到原型 (真实的隐式声明);
#       B = 全树内找不到定义 (多为非活动 #if 分支 / 编译器内建 / 库函数)
defined_anywhere = set()
for f in sorted(NEWFILES):
    if f.endswith('.c') and f.startswith('src/'):
        ft, _ = read_text(os.path.join(DST, f))
        fc = re.sub(r'/\*.*?\*/', ' ', ft, flags=re.S)
        fc = re.sub(r'//[^\n]*', ' ', fc)
        for mm in FUNC_RE.finditer(fc):
            defined_anywhere.add(mm.group('name'))
A, B = [], []
for x in implicit:
    n = x.split(': ')[1][:-2]
    (A if n in defined_anywhere else B).append(x)
report['implicit_declaration'] = A
report['implicit_undeclared_elsewhere'] = B

# ---------------------------------------------------------- 8. 未使用全局量
DEF = re.compile(r'^\s*(?:volatile\s+|const\s+|static\s+)*[A-Za-z_]\w*(?:\s*\*)?\s+'
                 r'([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*(?:=[^;]*)?;', re.M)
unused = []
_ALLSRC = '\n'.join(read_text(os.path.join(DST, f))[0]
                     for f in sorted(NEWFILES) if f.endswith('.c') and f.startswith('src/'))
_IDCOUNT = collections.Counter(re.findall(r'[A-Za-z_]\w*', _ALLSRC))
for m in MAP:
    if m['kind'] != 'c' or not m['dst'].startswith('src/'):
        continue
    rel = m['dst']
    t, _ = read_text(os.path.join(DST, rel))
    names = set()
    for mm in DEF.finditer(t):
        names.add(mm.group(1))
    for n in sorted(names):
        if _IDCOUNT[n] <= 1:
            unused.append('%s: %s' % (rel, n))
report['unused_global'] = sorted(set(unused))

# ---------------------------------------------------------- 9. 中断 / volatile
isr_def = []
isr_no_volatile = []
for m in MAP:
    if m['kind'] not in ('c', 'h'):
        continue
    t, _ = read_text(os.path.join(DST, m['dst']))
    for mm in re.finditer(r'^\s*interrupt\s+void\s+([A-Za-z_]\w*)', t, re.M):
        isr_def.append('%s: %s' % (m['dst'], mm.group(1)))
report['isr_definitions'] = sorted(set(isr_def))

VOL_DECL = re.compile(r'^\s*volatile\s+.*?;', re.M)
vol = []
for m in MAP:
    if m['kind'] not in ('c', 'h'):
        continue
    t, _ = read_text(os.path.join(DST, m['dst']))
    n = len(VOL_DECL.findall(t))
    if n:
        vol.append('%s: %d' % (m['dst'], n))
report['volatile_declarations'] = vol

json.dump(report, open(os.path.join(DOC, '_verify.json'), 'w', encoding='utf-8'),
          ensure_ascii=False, indent=1)
for k, v in report.items():
    print('%-26s %s' % (k, (len(v) if isinstance(v, (list, str)) else v)))
