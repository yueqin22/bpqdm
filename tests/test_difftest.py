"""改写模块的行为等价契约。

对 docs/rewritten_modules.json 里的每个模块, 要求同时具备:
  1. 宿主差分测试记录 (docs/_difftest/<case>.json): 用例数 > 0 且 0 处不一致;
  2. 目标端检查记录 (docs/_difftest/<Mod>.target.json): 编译通过、告警不增加、
     外部符号集合 (读写的全局量 / 调用的函数) 完全一致。

本测试不需要编译器, 只校验已产生的记录; 记录缺失时明确 SKIP。
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _common import ROOT, DOC, Checker

c = Checker('test_difftest')

MANP = os.path.join(DOC, 'rewritten_modules.json')
if not os.path.isfile(MANP):
    print('  [SKIP] 没有改写清单, 无需校验')
    print('test_difftest: 0 项, 失败 0 (已跳过)')
    sys.exit(0)

MAN = json.load(open(MANP, encoding='utf-8'))
mods = MAN['modules']
if not mods:
    print('  [PASS] 改写清单为空 —— 当前全部模块仍走 token/字节等价路径')
    print('test_difftest: 1 项, 失败 0')
    sys.exit(0)

DD = os.path.join(DOC, '_difftest')
missing = []
for m in mods:
    for n in (m['case'] + '.json',
              os.path.splitext(os.path.basename(m['dst']))[0] + '.target.json'):
        if not os.path.isfile(os.path.join(DD, n)):
            missing.append(n)
if missing:
    print('  [SKIP] 缺少差分/目标端记录 %s' % missing)
    print('         先运行: python tools/difftest/run_case.py <case>')
    print('                 python tools/difftest/target_check.py')
    print('test_difftest: 0 项, 失败 0 (已跳过)')
    sys.exit(0)

c.check(len(mods) > 0, '改写模块数 %d' % len(mods))
for m in mods:
    stem = os.path.splitext(os.path.basename(m['dst']))[0]

    d = json.load(open(os.path.join(DD, m['case'] + '.json'), encoding='utf-8'))
    cases = d.get('cases') or 0
    mm = d.get('mismatches')
    c.check(cases > 0, '%s 差分用例数 > 0 (实际 %s)' % (stem, cases))
    c.check(mm == 0, '%s 差分不一致 0 (实际 %s)' % (stem, mm))
    c.check(d.get('rc') == 0, '%s 差分测试进程退出码 0 (实际 %s)' % (stem, d.get('rc')))

    t = json.load(open(os.path.join(DD, stem + '.target.json'), encoding='utf-8'))
    c.check(t.get('rc_new') == 0, '%s 目标端编译通过' % stem)
    c.check(not t.get('warn_added'), '%s 目标端无新增告警 (实际 %s)'
            % (stem, t.get('warn_added')))
    c.check(t.get('symbols_equal') is True,
            '%s 目标端外部符号集合一致 (ref %s / new %s)'
            % (stem, len(t.get('symbols_ref') or []), len(t.get('symbols_new') or [])))
    c.check(t.get('pass') is True, '%s 目标端检查结论为通过' % stem)

sys.exit(0 if c.done() else 1)
