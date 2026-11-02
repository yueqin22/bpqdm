"""构建证据契约: 逐模块比对与整机映像的结果必须与"改写清单"一致。

分两层:
  - 未列入 docs/rewritten_modules.json 的模块: 必须仍然逐字节一致 (原契约不变);
  - 列入的模块: **允许**不一致, 但必须确实是清单里那一个, 且必须有差分测试覆盖
    (见 test_difftest.py)。

注意这里用的是**子集**而不是相等: "改写过" 并不蕴含 "产物必然不同"。
实测 `f_vf` 的实现被改写 (4 个具名宏、局部量改名、门控条件规范化), 但在工程选项
(-o2/-o3) 下生成的 `.text` 与符号表与改写前**逐字节相同**。最好的改写本来就该
收敛到同一份机器码, 所以契约只能是"**没改写过的必须相同**", 不能反过来要求
"改写过的必须不同" —— 后者会把一个更强的结果误判成失败。

本测试**不需要编译器** —— 它校验的是 `tools/build_and_compare.py` /
`tools/build_official.py` / `tools/build_baseline.py` 已经产生的记录与产物。
若这些产物不存在 (例如 `build/` 被清理过), 则明确报告 SKIP 而不是假装通过。
"""
import os, sys, hashlib, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _common import ROOT, DOC, Checker, load

c = Checker('test_build_evidence')

NEEDED = ['_dis_compare.json', '_official_build.json', '_baseline_build.json']
missing = [n for n in NEEDED if not os.path.isfile(os.path.join(DOC, n))]
if missing:
    print('  [SKIP] 缺少构建记录 %s' % missing)
    print('         先运行: python tools/build_and_compare.py / '
          'build_official.py / build_baseline.py')
    print('test_build_evidence: 0 项, 失败 0 (已跳过)')
    sys.exit(0)

CMP = load('_dis_compare.json')
OFF = load('_official_build.json')
BAS = load('_baseline_build.json')

MAN = json.load(open(os.path.join(DOC, 'rewritten_modules.json'), encoding='utf-8'))
WANT = set(os.path.splitext(os.path.basename(m['dst']))[0] for m in MAN['modules'])
NMOD = 59
NREW = len(WANT)

# ---- 1. 逐模块比对 ----
c.check(CMP.get('modules') == NMOD, '比对模块数 %d (实际 %s)' % (NMOD, CMP.get('modules')))
a = CMP['tierA']
c.check(not a['compile_failed'], '编译失败 0 (实际 %d)' % len(a['compile_failed']))
c.check(set(a['diff']) <= WANT,
        '反汇编不同的模块都在改写清单内 (越界 %s)' % sorted(set(a['diff']) - WANT))
c.check(len(a['same']) == NMOD - len(a['diff']) - 2,
        '反汇编逐行相同 %d (实际 %d)' % (NMOD - len(a['diff']) - 2, len(a['same'])))
c.check(len(a['no_code']) == 2, '纯数据模块 2 (实际 %d)' % len(a['no_code']))
c.check(set(a['no_code']) == {'DSP2803x_GlobalVariableDefs', 'MotorVar'},
        '纯数据模块为 DSP2803x_GlobalVariableDefs / MotorVar')

b = CMP['tierB']
bstems = set(x['stem'] for x in b['failed'])
c.check(bstems <= WANT,
        '目标文件内容级不同的模块都在改写清单内 (越界 %s)' % sorted(bstems - WANT))
c.check(len(b['identical']) == NMOD - len(bstems),
        '目标文件内容级一致 %d (实际 %d)' % (NMOD - len(bstems), len(b['identical'])))


# ---- 2. 构建记录 ----
def sha(p):
    return hashlib.sha256(open(p, 'rb').read()).hexdigest() if p and os.path.isfile(p) else None


for side, tag in ((OFF, '新工程'), (BAS, '原工程副本')):
    for cfg in ('debug', 'release'):
        r = side.get(cfg) or {}
        c.check(r.get('nfail') == 0, '%s %s 编译失败 0 (实际 %s)' % (tag, cfg, r.get('nfail')))
        c.check(r.get('linked') is True, '%s %s 链接成功' % (tag, cfg))
        c.check(bool(r.get('hex')) and os.path.isfile(r['hex']),
                '%s %s 生成 hex' % (tag, cfg))

# ---- 3. 整机映像 ----
# 清单为空 => 必须逐字节相同; 清单非空 => 必然不同, 此时要求差异是**有界**的,
# 不允许悄悄放过一个巨大的偏移。
#
# 注意**不要**拿 `os.path.getsize(hex)` 当"映像尺寸"。Intel HEX 是 ASCII 文本,
# 其长度由**记录条数与分段**决定: 一条 16 字节数据记录占 43 个字符, 数据挪动
# 几个字节就可能让某段多/少一条记录, 于是文件长度动几十字节而映像一个字节没变。
# 实测新工程 Debug 比原工程小 430 字节, 但真实映像内容只小了 176 字节, 其余
# 254 字节纯粹是记录分帧的账 —— 用它做阈值只会随改写模块数累积漂移。
#
# 所以这里解析 HEX 记录本身, 改用三条**有意义**的不变式:
#   (1) 数据字节数之差 <= 逐对象 `.text` 净变化之和 + 256 字节兜底 ——
#       "整幅偏移必须能被逐个对象的偏移解释", 见下面的 text_word_delta();
#   (2) 地址跨度 (lo..hi) 逐位相同 —— 链接器把任何东西放错地方都会被抓到;
#   (3) 触及的 64K 页集合相同 —— 整段丢失/新增会立刻暴露。
# 其中 (2)(3) 是旧写法完全没有的检查, 所以这不是放宽, 是换了个更严的口径。
ABS_SLACK = 256          # 字节, 兜底下限
REL_SLACK = 0.01         # 相对上限: 映像内容的 1% (仅在拿不到逐对象字数时兜底)


def text_word_delta():
    """从 tierA.details 里读每个改写对象两侧的 `.text` 字数, 返回净变化 (字)。

    `first diff line 3: 'TEXT Section .text (Little Endian), 0x8e7 words ...'
    vs '... 0x896 words ...'` —— 左边是原工程, 右边是新工程。
    纯数据模块 (NO-CODE) 与 `.text` 字数没变的模块不计。
    解析不出任何一对时返回 None。
    """
    import re
    total = 0
    seen = 0
    for blob in (CMP['tierA'].get('details') or {}).values():
        w = re.findall(r'\.text \(Little Endian\), 0x([0-9a-f]+) words', blob or '')
        if len(w) != 2:
            continue
        total += int(w[1], 16) - int(w[0], 16)
        seen += 1
    return total if seen else None


def hex_image(p):
    """解析 Intel HEX, 返回 (数据字节数, 扩展地址页集合, 最低地址, 最高地址)。"""
    nbytes, pages, lo, hi, base = 0, set(), None, None, 0
    for line in open(p, encoding='ascii', errors='replace'):
        line = line.strip()
        if not line.startswith(':'):
            continue
        b = bytes.fromhex(line[1:])
        n, off, typ = b[0], (b[1] << 8) | b[2], b[3]
        if typ == 0x04:                       # 扩展线性地址记录
            base = ((b[4] << 8) | b[5]) << 16
            pages.add(base)
        elif typ == 0x00:                     # 数据记录
            nbytes += n
            a = base + off
            lo = a if lo is None else min(lo, a)
            hi = a + n if hi is None else max(hi, a + n)
    return nbytes, pages, lo, hi


for cfg in ('debug', 'release'):
    oh = (OFF.get(cfg) or {}).get('hex')
    bh = (BAS.get(cfg) or {}).get('hex')
    so, sb = sha(oh), sha(bh)
    if NREW == 0:
        c.check(so is not None and so == sb,
                '%s 映像逐字节相同 (new %s / base %s)' % (cfg, (so or '-')[:12], (sb or '-')[:12]))
    else:
        c.check(so is not None and sb is not None and so != sb,
                '%s 映像因改写而不同 (new %s / base %s)' % (cfg, (so or '-')[:12], (sb or '-')[:12]))
        no, po, lo_o, hi_o = hex_image(oh)
        nb, pb, lo_b, hi_b = hex_image(bh)
        c.check(no > 0 and nb > 0, '%s 映像解析出数据 (%d / %d 字节)' % (cfg, no, nb))
        c.check((lo_o, hi_o) == (lo_b, hi_b),
                '%s 映像地址跨度未变 (new %s..%s / base %s..%s)'
                % (cfg, hex(lo_o), hex(hi_o), hex(lo_b), hex(hi_b)))
        c.check(po == pb, '%s 映像覆盖的 64K 页未变 (new %s / base %s)'
                % (cfg, sorted(hex(x) for x in po), sorted(hex(x) for x in pb)))
        # 上限不用"映像的固定百分比"。那个百分比是"整幅偏移能被逐对象差异
        # 解释"的**代理**, 而代理会随改写模块数漂移 —— 实测 48 个模块改写后
        # 净缩小 1078 字节 (逐对象 `.text` 之和), 映像缩小 1224 字节,
        # 差 146 字节来自 .cinit/.const 的重新排布, 而 1% 的固定上限只有 1192,
        # 于是"完全正常的累积缩小"把这条判据顶掉了。
        # 所以直接比它代理的那个量: 每个改写对象各自 `.text` 净变化之和
        # (docs/_dis_compare.json 的 tierA.details 里有两侧字数), 再加兜底。
        # 这个上限**随改写模块数自动长大**, 不需要每次手工调; 拿不到字数时
        # 才退回 1%。
        dw = text_word_delta()
        if dw is None:
            lim = max(ABS_SLACK, int(REL_SLACK * nb))
        else:
            lim = ABS_SLACK + 2 * abs(dw)
        c.check(abs(no - nb) <= lim,
                '%s 映像内容尺寸差有界 (new %d / base %d, 差 %+d, 限 %d%s)'
                % (cfg, no, nb, no - nb, lim,
                   '' if dw is None else ', 逐对象 .text 净 %+d 字' % dw))

# ---- 4. 证据文档存在且含关键结论 ----
ev = os.path.join(DOC, 'equivalence_evidence.md')
c.check(os.path.isfile(ev), 'docs/equivalence_evidence.md 存在')
if os.path.isfile(ev):
    t = open(ev, encoding='utf-8').read()
    c.check('%d / %d' % (NMOD - len(bstems), NMOD) in t,
            '证据文档记录了 %d/%d 目标文件一致' % (NMOD - len(bstems), NMOD))
    c.check('003f7ff6' in t, '证据文档记录了入口点 code_start @ 003f7ff6')
    for m in MAN['modules']:
        c.check(os.path.basename(m['dst']) in t,
                '证据文档点名了改写模块 %s' % os.path.basename(m['dst']))

# ---- 5. 原工程未被改动 ----
c.check(os.path.isdir(r'E:/software/bpqdm/bpqym'),
        '原工程目录仍存在 (未被删除)')
c.check(not os.path.isfile(os.path.join(ROOT, 'bpqym')),
        '新工程内没有指向原工程的副本/链接')

sys.exit(0 if c.done() else 1)
