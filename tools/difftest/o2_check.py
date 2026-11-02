# -*- coding: utf-8 -*-
"""在**工程实际优化选项**下比对改写模块与基准的反汇编。

为什么需要这一步 —— `build_and_compare.py` 是**故意关优化**编译的 (它的 `FLAGS`
里没有 `-o2`): 关优化时指令序列最贴近源码结构, 任何改写都会在 `.text` 上留下痕迹,
灵敏度最高。但工程真正交付的映像是按 `-o2` (Debug) / `-o3` (Release) 编译的。

于是出现一个 `build_and_compare.py` 看不见的问题: 某个改写可能只在**关优化**时
产生不同的指令, 而按真实选项编译后**收敛回同一份机器码**。那是比"允许不同"
更强的结果, 不记录下来就浪费了。

实测: `f_vf.c` 与 `f_frqSrc.c` 都属这一类 —— 关优化下 `.text` 有差异,
`-o2`/`-o3` 下反汇编**逐行相同**, 整机 hex 也因此逐字节未变。

用法:
    python tools/difftest/o2_check.py            # 清单里全部模块
    python tools/difftest/o2_check.py f_frqSrc   # 只查一个

结果落盘到 docs/_difftest/<case>.o2.json, 供人工复核与报告引用。

陷阱 (与 build_and_compare.py 同一套):
  * `dis2000` 必须在目标目录内用**裸文件名**调用, 否则输出会多一行路径回显,
    两侧的行数/内容就对不齐了;
  * 必须断言反汇编**非空** —— "空 == 空" 是假通过 (纯数据模块会这样);
  * Release 配置的基准目录里, 可执行文件是大写 (MD380_Release.out), 但 `.obj`
    仍是小写, 别按可执行文件的名字去推。
"""

import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
DOC = os.path.join(ROOT, 'docs')
CGT = 'E:/ti/c2000/cgt-c2000_25.11.1.LTS/ti-cgt-c2000_25.11.1.LTS/bin'
WORK = os.path.join(ROOT, 'build', '_o2check')

CONFIGS = ['Debug', 'Release']


def load_manifest():
    p = os.path.join(DOC, 'rewritten_modules.json')
    return json.load(open(p, encoding='utf-8'))['modules']


def obj_paths(stem):
    """返回 {配置: (new_obj, ref_obj)}; 缺文件的配置直接略去。"""
    out = {}
    for cfg in CONFIGS:
        new_p = os.path.join(ROOT, 'build', cfg, stem + '.obj')
        ref_p = os.path.join(ROOT, 'build', '_verify', 'baseline_tree', cfg,
                             stem + '.obj')
        if os.path.isfile(new_p) and os.path.isfile(ref_p):
            out[cfg] = (new_p, ref_p)
    return out


def dis(path, tag):
    """在私有目录里用裸文件名反汇编。返回行列表; 空产物返回 None。"""
    # 这里**不做 rmtree 清目录**: 沙箱的批量删除保护会把"一次删 >50 个文件"拦下来
    # (实测第一模块就报 `[SAFE_DELETE_BULK_CONFIRM_REQUIRED] count=90`), 整个 o2 检查
    # 当场中止 —— 而中止的原因是环境护栏, 不是被测代码, 很容易被误读成"收敛性变差了"。
    # 目录本来也不需要清: dis2000 只读 `m.obj`, 覆盖它即可, 残留文件不参与反汇编。
    d = os.path.join(WORK, tag)
    os.makedirs(d, exist_ok=True)
    shutil.copy2(path, os.path.join(d, 'm.obj'))
    r = subprocess.run([os.path.join(CGT, 'dis2000'), '-1', 'm.obj'],
                       cwd=d, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit('dis2000 rc=%d (%s): %s' % (r.returncode, path, r.stderr))
    txt = r.stdout
    if not txt.strip():
        return None
    return txt.split('\n')


def compare_pair(new_p, ref_p, tag):
    a = dis(new_p, tag + '_new')
    b = dis(ref_p, tag + '_ref')
    if a is None or b is None:
        return {'applicable': False,
                'why': '反汇编产物为空 (纯数据模块?)', 'diffs': None}
    n = max(len(a), len(b))
    diffs = []
    for i in range(n):
        x = a[i] if i < len(a) else '<无>'
        y = b[i] if i < len(b) else '<无>'
        if x != y:
            diffs.append({'line': i + 1, 'new': x, 'ref': y})
    return {'applicable': True, 'lines_new': len(a), 'lines_ref': len(b),
            'ndiff': len(diffs), 'diffs': diffs[:20]}


def main():
    argv = [a for a in sys.argv[1:] if not a.startswith('--')]
    man = load_manifest()
    if argv:
        man = [m for m in man if m['case'] in argv]
        if not man:
            raise SystemExit('清单里没有这些用例: %s' % argv)
    os.makedirs(WORK, exist_ok=True)
    outdir = os.path.join(DOC, '_difftest')
    os.makedirs(outdir, exist_ok=True)

    overall = True
    for m in man:
        stem = os.path.splitext(os.path.basename(m['dst']))[0]
        case = m['case']
        paths = obj_paths(stem)
        print('=' * 64)
        print('%s  (%s)' % (stem, case))
        print('=' * 64)
        if not paths:
            print('  跳过: build/<配置>/%s.obj 或基准产物不存在' % stem)
            print('       先跑 tools/build_official.py 与 tools/build_baseline.py')
            overall = False
            continue
        rec = {'case': case, 'module': m['dst'], 'configs': {}}
        for cfg, (new_p, ref_p) in paths.items():
            r = compare_pair(new_p, ref_p, '%s_%s' % (case, cfg))
            rec['configs'][cfg] = r
            if not r['applicable']:
                print('  %-8s 不适用: %s' % (cfg, r['why']))
            elif r['ndiff'] == 0:
                print('  %-8s 反汇编逐行相同 (%d 行) —— 改写收敛到同一份机器码'
                      % (cfg, r['lines_new']))
            else:
                print('  %-8s 不同: %d 行 (new %d 行 / ref %d 行)'
                      % (cfg, r['ndiff'], r['lines_new'], r['lines_ref']))
                for d in r['diffs'][:5]:
                    print('     第%d行\n       new: %s\n       ref: %s'
                          % (d['line'], d['new'], d['ref']))
        rec['identical_all'] = all(
            (not r['applicable']) or r['ndiff'] == 0 for r in rec['configs'].values())
        json.dump(rec, open(os.path.join(outdir, case + '.o2.json'), 'w',
                            encoding='utf-8'), ensure_ascii=False, indent=2)
    print('=' * 64)
    print('记录 -> docs/_difftest/<case>.o2.json')
    # 有差异不算失败: 改写本来就允许改变指令序列。这里只负责把事实记下来。
    return 0 if overall else 1


if __name__ == '__main__':
    sys.exit(main())
