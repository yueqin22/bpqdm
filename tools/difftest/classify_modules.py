"""给剩余模块分批: 按"改写后有多难证明"排序。

判据全部来自实测产物, 不靠读代码猜:
  - 活/死代码      <- build/Debug/jt380_C.map 里该 .obj 的 code/data 贡献
  - 代码规模       <- 同一张表
  - 硬件耦合度     <- nm2000 的未定义符号里有多少是外设寄存器
  - 数学库依赖     <- 是否引用汇编实现的 qsin / qatan / qsqrt (宿主无法直接编)
  - 跨模块调用数   <- 未定义符号里非寄存器、非运行时的项目内函数
  - 时序敏感       <- 源码里是否有 interrupt 定义

分批逻辑:
  T1 纯逻辑    : 无寄存器、无汇编数学 -> 差分测试最容易, 直接可做
  T2 弱耦合    : 有寄存器访问但无汇编数学 -> 需要寄存器 mock 覆盖读写序列
  T3 汇编数学  : 引用 qsin/qatan/qsqrt -> 还要先做宿主侧位精确复刻
  T4 时序敏感  : 定义了中断函数 -> 除行为外还要重测周期数 (风险最高)
"""

import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
DOC = os.path.join(ROOT, 'docs')
CGT = r'E:/ti/c2000/cgt-c2000_25.11.1.LTS/ti-cgt-c2000_25.11.1.LTS/bin'
OBJDIR = os.path.join(ROOT, 'build', 'Debug')
MAP = os.path.join(OBJDIR, 'jt380_C.map')

# 汇编实现的数学库 (asm/ASMSubPrg.asm)
ASM_MATH = ('_qsin', '_qatan', '_qsqrt', '_abs', '_absl', '_swap')

# 运行时支持库, 不算项目内依赖
RT_PREFIX = ('L$$', 'FS$$', 'I$$', '__', '_memcpy', '_memset', '_str')


def load_map():
    """从链接映射取每个目标文件的 code/data/ebss 贡献。

    表头那行就是 '..\\build\\Debug\\', 表尾是 '+--+---...' 分隔线;
    按行定位比写正则稳 (反斜杠在正则里容易数错)。
    """
    lines = open(MAP, encoding='utf-8', errors='replace').read().split('\n')
    start = None
    for i, ln in enumerate(lines):
        # 表头形如 "    ..\build\Debug\" (结尾带反斜杠), 末尾反斜杠先剥掉再比
        if ln.strip().rstrip('\\').lower() == r'..\build\debug':
            start = i + 1
            break
    if start is None:
        raise SystemExit('没能在 %s 里找到对象文件贡献表的表头' % MAP)

    rows = {}
    for ln in lines[start:]:
        if ln.lstrip().startswith('+--+'):
            break
        m = re.match(r'\s+(\S+\.obj)\s+(\d+)\s+(\d+)\s+(\d+)\s*$', ln)
        if m:
            rows[m.group(1)] = (int(m.group(2)), int(m.group(3)), int(m.group(4)))
    if not rows:
        raise SystemExit('对象文件贡献表解析为空')
    return rows


def undefined_syms(obj):
    r = subprocess.run([os.path.join(CGT, 'nm2000.exe'), obj],
                       capture_output=True, text=True, errors='replace')
    out = []
    for ln in ((r.stdout or '') + (r.stderr or '')).split('\n'):
        if ' U ' in ln:
            out.append(ln.split(' U ', 1)[1].strip())
    return out


def defined_syms(obj):
    r = subprocess.run([os.path.join(CGT, 'nm2000.exe'), obj],
                       capture_output=True, text=True, errors='replace')
    out = []
    for ln in ((r.stdout or '') + (r.stderr or '')).split('\n'):
        parts = ln.split()
        if len(parts) == 3 and parts[1] in 'BbDdCcGg' and parts[2].startswith('_'):
            out.append(parts[2])
    return out


def hw_symbols():
    """外设寄存器符号集合 —— 取权威来源, 不靠命名猜。

    DSP2803x_GlobalVariableDefs.c 就是外设寄存器结构体的定义处;
    它导出的 '_' 前缀变量即全部寄存器文件。再补上定时器与 eQEP 指针。
    """
    s = set(defined_syms(os.path.join(OBJDIR, 'DSP2803x_GlobalVariableDefs.obj')))
    s |= set(defined_syms(os.path.join(OBJDIR, 'DSP2803x_CpuTimers.obj')))
    s |= {'_EQepRegs', '_pEQepRegsFvc', '_pEQepRegsPc'}
    s.discard('_gZeroPos')
    return s


def main():
    man = json.load(open(os.path.join(DOC, 'rewritten_modules.json'), encoding='utf-8'))
    done = set(os.path.splitext(os.path.basename(m['dst']))[0] for m in man['modules'])

    mp = json.load(open(os.path.join(DOC, '_map.json'), encoding='utf-8'))
    stem2dst = {}
    for e in mp:
        if e.get('kind') == 'c':
            stem2dst[os.path.splitext(os.path.basename(e['dst']))[0]] = e['dst']

    contrib = load_map()
    HW = hw_symbols()

    rows = []
    for obj, (code, data, ebss) in contrib.items():
        stem = obj[:-4]
        if stem not in stem2dst:
            continue
        dst = stem2dst[stem]
        src = os.path.join(ROOT, dst)
        txt = open(src, encoding='utf-8', errors='replace').read() if os.path.isfile(src) else ''
        isr = len(re.findall(r'\binterrupt\s+void\b', txt))

        syms = undefined_syms(os.path.join(OBJDIR, obj))
        hw = [s for s in syms if s in HW]
        asmm = [s for s in syms if s in ASM_MATH]
        proj = [s for s in syms if s not in HW and s not in ASM_MATH
                and not s.startswith(RT_PREFIX)]

        # 0 字代码 = 没有实现可改 (纯数据/表文件, 或链接期被丢弃的死代码)。
        # 这两类都不能靠改写推进, 单独归为一档。
        if stem in done:
            tier = 'DONE'
        elif code == 0:
            tier = 'NONE'
        elif isr:
            tier = 'T4'
        elif asmm:
            tier = 'T3'
        elif hw:
            tier = 'T2'
        else:
            tier = 'T1'
        rows.append(dict(stem=stem, dst=dst, code=code, data=data, ebss=ebss,
                         hw=hw, asm=asmm, proj=proj, isr=isr, tier=tier,
                         size=os.path.getsize(src) if os.path.isfile(src) else 0))

    order = {'DONE': 0, 'NONE': 1, 'T1': 2, 'T2': 3, 'T3': 4, 'T4': 5}
    rows.sort(key=lambda r: (order[r['tier']], -r['code'], r['stem']))

    print('剩余模块分批 (判据全部来自 map + nm2000 实测)')
    print('=' * 92)
    print('%-4s %-28s %6s %6s %5s %5s %5s  %s' % (
        '批', '模块', '代码', '字节', '寄存器', '汇编', '中断', '项目内调用'))
    print('-' * 92)
    for r in rows:
        print('%-4s %-28s %6d %6d %5d %5d %5d  %s' % (
            r['tier'], r['stem'], r['code'], r['size'],
            len(r['hw']), len(r['asm']), r['isr'],
            ','.join(x.lstrip('_') for x in r['proj'][:4]) +
            ('...' if len(r['proj']) > 4 else '')))
    print('=' * 92)

    from collections import Counter
    cnt = Counter(r['tier'] for r in rows)
    tot = Counter()
    for r in rows:
        tot[r['tier']] += r['code']
    for t in ('DONE', 'NONE', 'T1', 'T2', 'T3', 'T4'):
        print('  %-5s %2d 个模块, 共 %5d 字代码' % (t, cnt[t], tot[t]))

    out = os.path.join(DOC, '_triage.json')
    json.dump(rows, open(out, 'w', encoding='utf-8'), ensure_ascii=False, indent=2)
    print('\n明细 -> docs/_triage.json')

    write_doc(rows, cnt, tot)
    print('分批方案 -> docs/rewrite_batches.md')


DESC = {
    'DONE': ('已改写', '已完成, 走差分测试 + 目标端检查路径'),
    'NONE': ('无实现可改', '0 字代码: 纯数据/表文件, 或链接期被丢弃的死代码。'
                           '改写无从下手, 也不需要'),
    'T1':   ('纯逻辑', '不碰外设寄存器、不依赖汇编数学库。'
                       '差分测试最容易做, 风险最低'),
    'T2':   ('硬件耦合', '读写外设寄存器。差分测试要先补寄存器 mock, '
                         '并保证两侧看到同一份寄存器状态'),
    'T3':   ('依赖汇编数学', '引用 qsin / qatan / qsqrt (在 asm/ASMSubPrg.asm 里)。'
                             '宿主测试要先做位精确的 C 复刻, 复刻本身又是新的不确定源'),
    'T4':   ('时序敏感', '定义了中断服务程序。除行为等价外, 还必须重测最坏执行时间 —— '
                         '而改写必然改变指令序列'),
}


def write_doc(rows, cnt, tot):
    L = ['# 实现改写的分批方案', '',
         '本文件由 `tools/difftest/classify_modules.py` 生成, 数据来自**实测产物**:',
         '',
         '- 活/死与代码规模: `build/Debug/jt380_C.map` 里各 `.obj` 的贡献',
         '- 硬件耦合: `nm2000` 的未定义符号 ∩ `DSP2803x_GlobalVariableDefs.obj` 导出的寄存器集合',
         '- 汇编数学依赖: 未定义符号里是否出现 `_qsin` / `_qatan` / `_qsqrt`',
         '- 时序敏感: 源码里是否有 `interrupt void` 定义',
         '',
         '| 批 | 含义 | 模块数 | 代码字数 | 证明难度 |', '|---|---|---:|---:|---|']
    for t in ('DONE', 'NONE', 'T1', 'T2', 'T3', 'T4'):
        name, desc = DESC[t]
        L.append('| **%s** | %s | %d | %s | %s |' % (
            t, name, cnt[t], format(tot[t], ','),
            {'DONE': '—', 'NONE': '—', 'T1': '低', 'T2': '中',
             'T3': '高', 'T4': '最高'}[t]))
    L += ['', '**建议按 T1 → T2 → T3 → T4 推进**, 每一批跑完再进下一批。', '']

    for t in ('DONE', 'NONE', 'T1', 'T2', 'T3', 'T4'):
        name, desc = DESC[t]
        L += ['## %s — %s' % (t, name), '', desc, '']
        sub = [r for r in rows if r['tier'] == t]
        if not sub:
            L += ['(无)', '']
            continue
        L += ['| 模块 | 代码字数 | 寄存器 | 汇编数学 | 中断 |', '|---|---:|---:|---:|---:|']
        for r in sub:
            L.append('| `%s` | %d | %d | %d | %d |' % (
                r['dst'], r['code'], len(r['hw']), len(r['asm']), r['isr']))
        L += ['']

    L += ['## 每个模块的固定动作', '',
          '见 `tools/difftest/README.md`「怎么加一个模块」—— 12 步, 顺序不能换。',
          '其中两条是硬性的:', '',
          '1. **动代码前先跑一次差分测试**, 确认 0 不一致 (此时两侧还是同一份代码)。',
          '    跳过这步, 后面的"0 不一致"无法区分"改写正确"和"测试台不工作"。',
          '2. **负向对照必须全部抓到**。抓不到就说明这个输入空间对该类错误不敏感,',
          '    要补用例而不是继续。', '',
          '## 这一批不能证明的东西', '',
          '- **周期数**: 改写会改变指令序列 (`f_p2p` `.text` 154→159 字)。',
          '  T4 的 7 个模块全部定义了中断, 必须重测最坏执行时间。',
          '- **目标端数值**: 宿主 `int` 是 32 位, C28x 是 16 位,',
          '  `Uint16 * 50` 这类表达式两边不等值。比较仍然有效, 但绝对值不可迁移。',
          '- **硬件行为**: 上板验证不在本方案范围内。', '']

    open(os.path.join(DOC, 'rewrite_batches.md'), 'w',
         encoding='utf-8').write('\n'.join(L) + '\n')


if __name__ == '__main__':
    sys.exit(main())
