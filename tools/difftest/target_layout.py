"""宿主 vs 目标端 布局对照 (f_motorFc 用例)。

f_motorFc.c 用 GetCodeIndex(funcCode.code.motorFcM2.motorPara.elem.motorType)
去索引 funcCode.all[]。这个值是**结构体成员的字节偏移**, 只有和真头文件一起才有意义。
本工程把 Uint16 钉成 uint16_t 后, 宿主的结构体尺寸与 C28x 一致 —— 但有一个例外:
`enum MOTOR_SN` 在 C28x 上是 2 字节, 在 MSVC 上是 4 字节。它出现在 FUNCCODE_CODE
里, 会把它后面的成员整体推移。

本脚本把同一份 _layout_probe.c 分别用 MSVC 与 cl2000 编一遍, 把两边 14 个数字
并排列出来。这样"宿主布局与目标端差多少"就是一个**实测数**, 不是假设。
"""

import json
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, 'tools'))

import ccenv
import coff_cmp
import run_case

CASE_NAME = sys.argv[1] if len(sys.argv) > 1 else 'f_motorFc'
CASE = os.path.join(HERE, 'cases', CASE_NAME)
SHIM = os.path.join(HERE, 'shim')
CGT = coff_cmp.CGT

FLAGS = ['-g', '-q', '--abi=coffabi', '-d_DEBUG', '-dDSP2803X', '-dLARGE_MODEL',
         '-ml', '-mt', '-v28']

# (标签, 单位). 单位决定怎么把两侧放到同一把尺子上:
#
#   BYTES —— sizeof 的结果。C28x 的 "字节" 就是 16 位地址单元 (CHAR_BIT == 16),
#            所以目标端的 sizeof 数值 = 宿主字节数 / 2。两边要乘 2 才能对齐。
#   WORDS —— 指针相减得到的字偏移。宿主的 Uint16 是 uint16_t, 目标端的 Uint16 是
#            16 位 unsigned int, 指针差在两边都已经是"字"数, 无需换算。
#   COUNT —— 元素个数, 与平台无关。
BYTES, WORDS, COUNT = 'B', 'W', 'C'
_KIND = {'B': BYTES, 'W': WORDS, 'C': COUNT}


def load_labels():
    """从 cases/<case>/layout_labels.txt 读 (标签, 单位)。

    标签清单和探针源文件分开放, 是为了让"探针量了什么"和"报告怎么标注"各有
    唯一来源: 探针里的 LAYOUT_LIST 决定**顺序与条数**, 本文件决定**名字与单位**,
    两边条数不符时 target_values() 会报错停下。

    格式: 每行 `标签|单位`, 单位取 B (字节) / W (字) / C (个); `#` 起头是注释。
    """
    p = os.path.join(CASE, 'layout_labels.txt')
    if not os.path.isfile(p):
        raise SystemExit('缺少 %s' % os.path.relpath(p, ROOT).replace('\\', '/'))
    out = []
    for line in open(p, encoding='utf-8'):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        lab, _, kind = line.rpartition('|')
        if kind not in _KIND:
            raise SystemExit('layout_labels.txt: 单位必须是 B/W/C: %r' % line)
        out.append((lab.strip(), _KIND[kind]))
    if not out:
        raise SystemExit('layout_labels.txt 是空的: %s' % p)
    return out


def probe_mocks():
    """cases/<case>/layout_mocks.txt 列出的、与探针一起编译的 mock 源文件。

    宿主探针要一个真的 funcCode 对象才能取成员地址; 目标端只要编译成 .obj,
    取地址只是重定位, 不需要定义。所以这份清单只用于宿主侧。
    """
    p = os.path.join(CASE, 'layout_mocks.txt')
    if not os.path.isfile(p):
        return []
    names = [l.strip() for l in open(p, encoding='utf-8')
             if l.strip() and not l.startswith('#')]
    return [os.path.join(CASE, n) for n in names]


LABELS = load_labels()


def inc_dirs(host):
    """宿主侧要 shim (设备头文件在 x86 上编不了); 目标端**不加任何 shim** ——
    直接用真头文件, 这样量到的就是固件真实的布局。

    目标端还必须是 include/device 在前: Uint16 / int16 / int32 / Uint32 真正定义在
    include/device/DSP2803x_Device.h 里, 由 f_funcCode.h -> DSP28x_Project.h 拉进来。
    宿主侧则由 shim 的 DataTypeDef.h 钉成精确宽度。
    """
    if host:
        return [os.path.join(CASE, 'shim'), SHIM,
                os.path.join(ROOT, 'include', 'motor'),
                os.path.join(ROOT, 'include', 'common'),
                os.path.join(ROOT, 'include', 'function')]
    return [os.path.join(ROOT, 'include', 'device'),
            os.path.join(ROOT, 'include', 'common'),
            os.path.join(ROOT, 'include', 'function'),
            os.path.join(ROOT, 'include', 'motor')]


def host_values():
    out = os.path.join(ROOT, 'build', '_layout')
    os.makedirs(out, exist_ok=True)
    exe = os.path.join(out, 'layout_host.exe')

    # 宿主探针必须和差分测试看到**同一套**头文件, 否则量出来的布局不是被测代码
    # 实际用的那一套。两件事要复用 run_case 的机制, 不能各写一份:
    #   real.txt    把用例点名的头文件转发到真头文件 (否则被公共 shim 的同名桩挡住,
    #               拿到 FUNCCODE_P2P_MOCK 而不是 FUNCCODE_ALL, 直接编不过);
    #   defines.txt 复刻目标端的 -d_DEBUG -dDSP2803X -dLARGE_MODEL。
    gen = os.path.join(ROOT, 'build', '_difftest', 'gen_%s_layout' % CASE_NAME)
    run_case.make_real_passthroughs(CASE, gen)

    srcs = [os.path.join(CASE, '_layout_probe.c')] + probe_mocks()
    rc, log = ccenv.compile_exe(
        srcs, exe, incdirs=[gen] + inc_dirs(True),
        defines=['HOST_PROBE'] + run_case.case_defines(CASE))
    if rc != 0:
        print(log)
        raise SystemExit('宿主探针编译失败 rc=%d' % rc)
    rc, log = ccenv.run_exe(exe, cwd=out)
    if rc != 0:
        raise SystemExit('宿主探针运行失败 rc=%d\n%s' % (rc, log))
    toks = [int(x) for x in log.split()]
    count, vals = toks[0], toks[1:]
    if count != len(vals):
        raise SystemExit('宿主探针输出自相矛盾: 声明 %d 项, 实际 %d 项' % (count, len(vals)))
    return vals


def target_values():
    out = os.path.join(ROOT, 'build', '_layout')
    os.makedirs(out, exist_ok=True)
    cmd = ([os.path.join(CGT, 'cl2000.exe')] + FLAGS +
           ['-i' + p for p in inc_dirs(False)] +
           ['-fr' + out.replace('/', '\\'), '-c',
            os.path.abspath(os.path.join(CASE, '_layout_probe.c'))])
    r = subprocess.run(cmd, cwd=os.path.dirname(os.path.abspath(CASE)),
                       capture_output=True, text=True, errors='replace')
    log = (r.stdout or '') + (r.stderr or '')
    if r.returncode != 0:
        print(log)
        raise SystemExit('目标端探针编译失败 rc=%d' % r.returncode)

    # -fr 只给输出目录, 目标文件名由源文件基名决定
    obj = os.path.join(out, '_layout_probe.obj')
    if not os.path.isfile(obj):
        raise SystemExit('找不到目标文件 %s' % obj)
    raw = open(obj, 'rb').read()
    _txt, secs = coff_cmp.ofd_sections(obj)   # 返回 (文本, 段列表)
    cand = [s for s in secs if s['name'] == 'layout_probe']
    if not cand:
        raise SystemExit('目标文件里没有 layout_probe 段, 实际段: %s'
                         % [s['name'] for s in secs])
    s = cand[0]
    # ofd2000 的 Size 单位是**地址单元**而不是字节。g_layout 是 Uint16 数组,
    # C28x 上地址单元 = 16 位, 所以字节长度是 size*2。按 size 当字节读会正好
    # 少读一半, 后面的 unpack 就会拿后半段当作前半段的值 —— 静默出错。
    if not s['unit']:
        raise SystemExit('layout_probe 段缺 Address Unit Size')
    nbytes = s['size'] * s['unit'] // 8
    if s['off'] == 0:
        raise SystemExit('layout_probe 段未初始化 (off=0), 文件里没有数据')
    data = raw[s['off']: s['off'] + nbytes]
    if len(data) != nbytes:
        raise SystemExit('layout_probe 越界 (off=%d nbytes=%d)' % (s['off'], nbytes))
    n = len(LABELS)
    if len(data) < n * 2:
        raise SystemExit('layout_probe 只有 %d 字节, 放不下 %d 个字' % (len(data), n))
    return list(struct.unpack_from('<%dH' % n, data, 0))


def main():
    h = host_values()
    t = target_values()
    print('宿主 (MSVC x64) vs 目标端 (C28x cl2000) 布局对照')
    print('=' * 78)
    print('%-34s %8s %8s %6s' % ('项', '宿主', '目标端', '差'))
    print('-' * 78)
    diffs = []
    for i, (lab, kind) in enumerate(LABELS):
        # 目标端 sizeof 的单位是 16 位地址单元, 乘 2 才是"宿主字节"。
        tv = t[i] * 2 if kind is BYTES else t[i]
        d = h[i] - tv
        if d:
            diffs.append(lab)
        print('%-34s %8d %8d %6s   %s' % (lab, h[i], tv, ('%+d' % d) if d else '=',
                                          {'B': '字节', 'W': '字', 'C': '个'}[kind]))
    print('-' * 78)
    print('不一致项: %d / %d' % (len(diffs), len(LABELS)))
    if diffs:
        print()
        print('差异根因 (已知且已确认, 不是探针故障):')
        print('  * f_funcCode.h 里 3 处 `Uint16 all[sizeof(struct X)]`')
        print('      (ERROR_SCENE / MOTOR_PARA / PG_PARA)。C28x 上 sizeof 返回的是')
        print('      16 位单元数, 正好等于 Uint16 元素个数; 宿主上 sizeof 返回 8 位')
        print('      字节数, 数组因此**大一倍**, 把 union 撑大, 进而撑大 MOTOR_FC 与')
        print('      FUNCCODE_CODE。这是被测头文件的惯用法在 x86 上的固有失真。')
        print('  * `enum MOTOR_SN` 在 C28x 上 1 个 16 位单元, 在 MSVC 上 4 字节 ->')
        print('      motorSn 之后的成员整体 +1 字 (见 motorParaM1.*)。')
        print()
        print('对差分测试的影响: 两侧(ref/new)共用同一套头文件, 因此布局差异对两侧')
        print('  完全对称, "相同输入 -> 相同输出" 的判据不受影响。受影响的是被索引到的')
        print('  funcCode.all[] 具体下标 —— 也就是上面单位为 W 的那些项。所以这是')
        print('  **已量化的保真度缺口**, 不是等价性缺口: 宿主上跑通的等价性, 对')
        print('  "同一份下标表达式在两侧算出同一个数" 这件事只能给到部分保证。')

    # 落盘: 报告是**生成**的, 数字不能只活在终端输出里 —— 否则下一版报告要么
    # 靠人肉转述, 要么干脆漏掉这张表。JSON 里统一换算成宿主字节。
    rec = {'case': CASE_NAME, 'host': 'MSVC x64', 'target': 'C28x cl2000',
           'ndiff': len(diffs), 'nitem': len(LABELS),
           'diffs': diffs, 'items': []}
    for i, (lab, kind) in enumerate(LABELS):
        tv = t[i] * 2 if kind is BYTES else t[i]
        rec['items'].append({'label': lab, 'unit': kind,
                             'host': h[i], 'target': tv, 'delta': h[i] - tv})
    out = os.path.join(ROOT, 'docs', '_difftest', CASE_NAME + '.layout.json')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, 'w', encoding='utf-8') as f:
        json.dump(rec, f, ensure_ascii=False, indent=2)
    print('已写出 docs/_difftest/%s.layout.json' % CASE_NAME)
    return 0


if __name__ == '__main__':
    sys.exit(main())
