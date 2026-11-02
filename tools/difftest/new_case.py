"""为一个模块搭出差分测试用例的脚手架。

只做**机械部分** —— 能从编译产物里推出来的那些:

  1. 冻结基准: 把当前 `src/<dir>/<Mod>.c` 复制成 `cases/<case>/ref/<Mod>_ref.c`。
     必须在改写**之前**跑, 否则冻结的就不是基准了。
  2. 生成 `ref_tu.c` / `new_tu.c`: 把模块所有**外部链接**的符号加 `ref_` / `new_`
     前缀, 再 include 各自的实现。符号表从 `nm2000` 读, 不靠人眼数 ——
     漏一个就在链接期撞名, 多写一个就编译不过, 两种都会立刻暴露。
  3. 写 `srcdir.txt`。

**不**做语义部分: `shim/`、`mocks.txt`、`driver.c` 决定"测什么", 必须人来写。
本脚本跑完会把还缺什么打出来。

用法:
    python tools/difftest/new_case.py f_vf
    python tools/difftest/new_case.py src/function/f_vf.c
    python tools/difftest/new_case.py --list          # 列出还没有用例的模块
"""

import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, 'tools'))

import coff_cmp

DOC = os.path.join(ROOT, 'docs')
CASES = os.path.join(HERE, 'cases')
NM = os.path.join(coff_cmp.CGT, 'nm2000' + ('.exe' if os.name == 'nt' else ''))

# nm2000 里大写类型字母 = 外部链接 (T=text, D=已初始化数据, B=bss, C=common,
# R=只读)。小写 = 文件内局部, 不需要改名; '?' 是调试段, 也不是符号。
GLOBAL_TYPES = set('TDBRC')


def load_map():
    return json.load(open(os.path.join(DOC, '_map.json'), encoding='utf-8'))


def rewritten():
    p = os.path.join(DOC, 'rewritten_modules.json')
    if not os.path.isfile(p):
        return set()
    return set(m['dst'] for m in json.load(open(p, encoding='utf-8'))['modules'])


def find_module(arg):
    """把 f_vf / src/function/f_vf.c / function/f_vf.c 都认成同一个模块。"""
    man = load_map()
    stem = os.path.splitext(os.path.basename(arg))[0]
    hits = [m for m in man
            if m['kind'] == 'c' and os.path.splitext(os.path.basename(m['dst']))[0] == stem]
    if not hits:
        raise SystemExit('在 docs/_map.json 里找不到模块: %s' % arg)
    if len(hits) > 1:
        raise SystemExit('模块名不唯一 (%s), 请用完整路径: %s'
                         % (stem, ', '.join(m['dst'] for m in hits)))
    return hits[0]


def global_symbols(stem):
    """从 build/Debug/<stem>.obj 读出该模块定义的外部符号 (去掉前导下划线)。"""
    obj = os.path.join(ROOT, 'build', 'Debug', stem + '.obj')
    if not os.path.isfile(obj):
        raise SystemExit('找不到 %s —— 先跑 python tools/build_and_compare.py'
                         % os.path.relpath(obj, ROOT).replace('\\', '/'))
    r = subprocess.run([NM, obj], capture_output=True, text=True)
    log = (r.stdout or '') + (r.stderr or '')
    if r.returncode != 0:
        raise SystemExit('nm2000 失败:\n%s' % log)

    syms, seen = [], set()
    for line in log.split('\n'):
        m = re.match(r'^[0-9a-fA-F]+\s+([A-Z])\s+_([A-Za-z_]\w*)\s*$', line.strip())
        if not m or m.group(1) not in GLOBAL_TYPES:
            continue
        name = m.group(2)
        if name in seen:
            continue
        seen.add(name)
        syms.append(name)
    return sorted(syms)


def tu_text(prefix, syms, include, why):
    w = max((len(s) for s in syms), default=8)
    lines = ['/* %s translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。'
             % prefix,
             ' *',
             ' * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期',
             ' * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。',
             ' *',
             ' * %s' % why,
             ' */',
             '']
    for s in syms:
        lines.append('#define %-*s %s_%s' % (w, s, prefix, s))
    lines += ['', '#include "%s"' % include, '']
    return '\n'.join(lines)


def main():
    args = [a for a in sys.argv[1:] if a != '--list']
    if '--list' in sys.argv[1:]:
        done = set(os.listdir(CASES))
        rw = rewritten()
        rows = []
        for m in load_map():
            if m['kind'] != 'c':
                continue
            stem = os.path.splitext(os.path.basename(m['dst']))[0]
            if stem in done:
                continue
            rows.append((stem, m['dst'], '已改写' if m['dst'] in rw else ''))
        print('还没有差分用例的 C 模块 (%d 个):' % len(rows))
        for stem, dst, tag in rows:
            print('  %-28s %s %s' % (stem, dst, tag))
        return 0

    if len(args) != 1:
        raise SystemExit(__doc__)

    m = find_module(args[0])
    stem = os.path.splitext(os.path.basename(m['dst']))[0]
    case = os.path.join(CASES, stem)
    mod = os.path.splitext(os.path.basename(m['dst']))[0]

    if m['dst'] in rewritten():
        raise SystemExit('%s 已在改写清单里 —— 基准必须在改写前冻结, 现在冻结只会'
                         '拿到改写后的代码。' % m['dst'])

    syms = global_symbols(stem)
    if not syms:
        raise SystemExit('%s 没有定义任何外部符号 (纯数据模块?), 不适用差分测试。' % stem)

    os.makedirs(os.path.join(case, 'ref'), exist_ok=True)

    src = os.path.join(ROOT, m['dst'])
    base = os.path.join(case, 'ref', mod + '_ref.c')
    with open(src, 'rb') as f:
        raw = f.read()
    with open(base, 'wb') as f:
        f.write(raw)

    with open(os.path.join(case, 'ref_tu.c'), 'w', encoding='utf-8') as f:
        f.write(tu_text('ref', syms, mod + '_ref.c',
                        '基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。'))
    with open(os.path.join(case, 'new_tu.c'), 'w', encoding='utf-8') as f:
        f.write(tu_text('new', syms, mod + '.c',
                        'new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。'))

    srcdir = os.path.dirname(m['dst'])
    with open(os.path.join(case, 'srcdir.txt'), 'w', encoding='utf-8') as f:
        f.write(srcdir)

    print('已生成脚手架: cases/%s/' % stem)
    print('  冻结基准      : ref/%s_ref.c  (%d 字节, 取自 %s)'
          % (mod, len(raw), m['dst']))
    print('  改名符号 %d 个: %s' % (len(syms), ', '.join(syms)))
    print('  srcdir.txt    : %s' % srcdir)
    print()
    print('还缺 (必须人写, 决定"测什么"):')
    print('  cases/%s/shim/     该模块依赖的头文件替换 (必须和真头文件语义一致)'
          % stem)
    print('  cases/%s/mocks.txt 要链接的 mock 清单 (默认 shim/hw_mock.c)'
          % stem)
    print('  cases/%s/driver.c  输入枚举 + 逐字段比对' % stem)
    print()
    print('写完再跑:')
    print('  python tools/difftest/run_case.py %s 20000' % stem)
    print('  python tools/difftest/target_check.py %s' % m['dst'])
    return 0


if __name__ == '__main__':
    sys.exit(main())
