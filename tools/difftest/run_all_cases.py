# -*- coding: utf-8 -*-
"""重跑 docs/rewritten_modules.json 里每个模块的差分测试与目标端检查。

用法:
  python tools/difftest/run_all_cases.py            # 全部模块
  python tools/difftest/run_all_cases.py f_plc ...  # 只跑指定用例

为什么要它:
  `docs/_difftest/<case>.json` 是"某次跑过"的证据, 不是"现在也对"的证据。源文件
  只要被碰过一下 (哪怕只是重排空行), 那份记录就过期了 —— 而 test_difftest.py 只读
  记录、不看时间戳, 过期记录照样能过。本脚本把"记录是否还对得上现在的源码"这件事
  变成一次显式重跑。

  采样规模 (`random`) 从旧记录里读回来, 所以重跑是**复现**而不是另抽一次:
  用例总数与旧记录不同就说明驱动/源码变了, 会被如实打印出来。

退出码: 0 = 全部 0 不一致且目标端检查通过; 否则 1。
"""
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
PY = sys.executable
DD = os.path.join(ROOT, 'docs', '_difftest')


def old_record(case):
    p = os.path.join(DD, case + '.json')
    if not os.path.isfile(p):
        return {}
    with open(p, encoding='utf-8') as f:
        return json.load(f)


def run(cmd):
    t0 = time.time()
    r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    return r.returncode, (r.stdout or '') + (r.stderr or ''), time.time() - t0


def main():
    only = [a for a in sys.argv[1:] if not a.startswith('-')]
    man = json.load(open(os.path.join(ROOT, 'docs', 'rewritten_modules.json'),
                         encoding='utf-8'))
    mods = man['modules']
    if only:
        mods = [m for m in mods if m['case'] in only]

    rows = []
    bad = 0
    for m in mods:
        case = m['case']
        stem = os.path.splitext(os.path.basename(m['dst']))[0]
        prev = old_record(case)
        nrand = prev.get('random') or 20000

        print('=' * 72)
        print('== %s  (%s)   random=%d' % (case, m['dst'], nrand))
        print('=' * 72)
        sys.stdout.flush()

        rc1, log1, t1 = run([PY, os.path.join(HERE, 'run_case.py'),
                             case, str(nrand)])
        tail = [l for l in log1.splitlines() if 'cases=' in l or 'MISMATCH' in l]
        print('\n'.join(tail[-6:]) or log1[-500:])
        print('  [difftest] rc=%d  %.1fs' % (rc1, t1))

        rc2, log2, t2 = run([PY, os.path.join(HERE, 'target_check.py'),
                             m['dst']])
        keep = [l for l in log2.splitlines()
                if '符号集合' in l or '.text' in l or '结论' in l
                or '新增告警' in l or 'FAIL' in l]
        print('\n'.join(keep) or log2[-500:])
        print('  [target]   rc=%d  %.1fs' % (rc2, t2))
        sys.stdout.flush()

        now = old_record(case)
        cases = now.get('cases')
        mm = now.get('mismatches')
        if rc1 != 0 or mm != 0 or rc2 != 0:
            bad += 1
        rows.append({'case': case, 'dst': m['dst'], 'random': nrand,
                     'cases': cases, 'mismatches': mm,
                     'difftest_rc': rc1, 'target_rc': rc2,
                     'drift': (cases != prev.get('cases')),
                     'seconds': round(t1 + t2, 1)})

    print()
    print('=' * 72)
    print('%-24s %10s %6s %6s %6s %8s' %
          ('case', 'cases', 'mism', 'dt_rc', 'tg_rc', 'sec'))
    for r in rows:
        print('%-24s %10s %6s %6s %6s %8s' %
              (r['case'], r['cases'], r['mismatches'], r['difftest_rc'],
               r['target_rc'], r['seconds']))
    tot = sum(r['cases'] or 0 for r in rows)
    print('合计 %d 个模块, %d 条用例, 失败 %d' % (len(rows), tot, bad))

    out = os.path.join(DD, '_sweep.json')
    with open(out, 'w', encoding='utf-8') as f:
        json.dump({'rows': rows, 'total_cases': tot, 'failed': bad},
                  f, ensure_ascii=False, indent=2)
    print('已写出 %s' % out)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
