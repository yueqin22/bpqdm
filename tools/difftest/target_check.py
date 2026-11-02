"""目标端 (C28x) 检查: 改写后的模块还能不能编, 以及副作用集合有没有变。

差分测试证明的是"宿主上算出来的数一样"; 这里补的是**目标端**证据:
  1. 用真实 TI CGT 编译 ref / new, 告警必须不增加;
  2. 目标文件里引用到的外部符号 (读写的全局量、调用的函数) 必须完全一致 ——
     这条能抓出"少写了一个全局量""忘了调用某个函数"这类宿主测试看不见的错;
  3. 代码尺寸变化必须是有界的 (记录, 不做强制阈值)。
"""

import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

import coff_cmp

CGT = coff_cmp.CGT
OUT = os.path.join(ROOT, 'build', '_targetcheck')

# 与 tools/build_and_compare.py 保持一致: 本工程是 COFF, 必须显式 --abi=coffabi
FLAGS = ['-g', '-q', '--abi=coffabi', '-d_DEBUG', '-dDSP2803X', '-dLARGE_MODEL',
         '-ml', '-mt', '-v28']


def inc_dirs():
    return [os.path.join(ROOT, 'include', d)
            for d in ('device', 'common', 'function', 'motor')]


def find_case(module):
    """按模块名去 cases/*/ref/<stem>_ref.c 找它的冻结基准。"""
    stem = os.path.splitext(os.path.basename(module))[0]
    cases = os.path.join(HERE, 'cases')
    for d in sorted(os.listdir(cases)):
        p = os.path.join(cases, d, 'ref', stem + '_ref.c')
        if os.path.isfile(p):
            return d, p
    raise SystemExit('找不到 %s 的冻结基准 (cases/*/ref/%s_ref.c)' % (module, stem))


def compile_one(src, tag):
    os.makedirs(OUT, exist_ok=True)
    # -fr 只给输出目录, 目标文件名由源文件基名决定
    obj = os.path.join(OUT, os.path.basename(src).rsplit('.', 1)[0] + '.obj')
    cmd = ([os.path.join(CGT, 'cl2000.exe')] + FLAGS +
           ['-i' + p for p in inc_dirs()] +
           ['-fr' + OUT.replace('/', '\\'), '-c', os.path.abspath(src)])
    r = subprocess.run(cmd, cwd=os.path.dirname(os.path.abspath(src)),
                       capture_output=True, text=True, errors='replace')
    log = (r.stdout or '') + (r.stderr or '')
    return r.returncode, log, obj


def warnings(log):
    out = set()
    for ln in log.split('\n'):
        m = re.search(r'(warning|error|remark):[ \t]*(.*)', ln)
        if m:
            out.add(m.group(2).strip())
    return out


def main(module='src/motor/ZeroPosCtl.c'):
    case, ref_src = find_case(module)
    new_src = os.path.join(ROOT, module)

    rc1, log1, obj1 = compile_one(ref_src, 'ref')
    rc2, log2, obj2 = compile_one(new_src, 'new')

    print('目标端 (C28x) 检查: %s  (case=%s)' % (module, case))
    print('=' * 64)
    print('ref 编译 rc=%d   new 编译 rc=%d' % (rc1, rc2))
    if rc1 != 0:
        print(log1)
    if rc2 != 0:
        print(log2)
    if rc1 != 0 or rc2 != 0:
        return 1

    w1, w2 = warnings(log1), warnings(log2)
    print('ref 告警 %d 条 / new 告警 %d 条' % (len(w1), len(w2)))
    added = w2 - w1
    gone = w1 - w2
    print('  新增告警: %s' % (sorted(added) if added else '无'))
    print('  消失告警: %s' % (sorted(gone) if gone else '无'))

    a = coff_cmp.canonical(obj1)
    b = coff_cmp.canonical(obj2)

    def real_syms(d):
        """只保留**对外可见**的符号: COFF storage class == 2 (C_EXT)。

        为什么按 storage class 判, 而不是按名字形状判:
          * 段符号 (`.text`/`.bss`/...)、内嵌源文件名符号、编译器局部标号
            (`$C$L1`/`$C$IR_1`) 都不是 C 语言层面的外部符号, 其 storage class
            分别是 3 / 103 / 3 / 6;
          * **文件内 static 与函数内 static 在目标文件里都是局部符号** —— 别的
            编译单元引用不到, 不构成"对外副作用"。改写给它们改名是**预期**结果,
            拿它判失败只会把正确改写挡在门外。

        早期版本用"名字里带 `$` 就排除"当启发式, 这**只挡住了函数内 static**:
        TI CGT 给函数内 static 加序号去重 (`_plcFlag$6`、`_count$1`), 但
        **文件内 static 不加序号** (`_sBaud`、`_sSourID`)。f_canlink 是第一个
        改名文件内 static 的模块 (`sBaud` -> `s_canBaudIndex`), 该启发式当场
        假报"符号集合不同"。改用 storage class 后两类都正确排除, 且**判据更严**:
        若改写误把 static 改成外部链接, 符号会以 C_EXT 出现 -> 照样被抓到。

        未定义符号 (scnum == 0) 同样是 C_EXT, 表示"本模块引用了别处的量/函数",
        属于对外副作用, 必须计入。
        """
        return set(s[0] for s in d['symbols']
                   if s[0].startswith('_') and s[4] == 2)

    sa, sb = real_syms(a), real_syms(b)
    print()
    print('ref 目标文件外部符号: %s' % sorted(sa))
    print('new 目标文件外部符号: %s' % sorted(sb))
    print('  符号集合相同: %s' % (sa == sb))
    if sa != sb:
        print('    仅 ref 有: %s' % sorted(sa - sb))
        print('    仅 new 有: %s' % sorted(sb - sa))

    def text_size(d):
        for s in d['sections']:
            if s['name'] == '.text':
                return s['size']
        return 0
    t1, t2 = text_size(a), text_size(b)
    print()
    print('.text 尺寸: ref=%d  new=%d  差=%+d' % (t1, t2, t2 - t1))

    ok = (rc2 == 0) and (not added) and (sa == sb)

    # 落盘, 供 test_difftest.py 引用
    docdir = os.path.join(ROOT, 'docs', '_difftest')
    os.makedirs(docdir, exist_ok=True)
    stem = os.path.splitext(os.path.basename(module))[0]
    rec = {
        'module': module,
        'rc_ref': rc1, 'rc_new': rc2,
        'warn_ref': sorted(w1), 'warn_new': sorted(w2),
        'warn_added': sorted(added), 'warn_gone': sorted(gone),
        'symbols_ref': sorted(sa), 'symbols_new': sorted(sb),
        'symbols_equal': sa == sb,
        'text_ref': t1, 'text_new': t2, 'text_delta': t2 - t1,
        'pass': ok,
    }
    with open(os.path.join(docdir, stem + '.target.json'), 'w', encoding='utf-8') as f:
        json.dump(rec, f, ensure_ascii=False, indent=2)

    print()
    print('结论: %s' % ('通过' if ok else '不通过'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(*sys.argv[1:]))
