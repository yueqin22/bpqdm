"""COFF 段长度回归测试。

ofd2000 -v 报告的段大小以**地址单元**计, 不是字节。C28x 的 .text / .econst 是
16 位单元, 所以 `Size: 0x43` 是 67 个字 = 134 字节。coff_cmp.canonical() 早先
直接按 `size` 字节切片, 每个段都只读了前一半 —— 段后半段的任何差异都看不见,
而 "逐模块目标文件内容级比对" 依旧报"一致"。这是一个会**静默削弱证据**的缺陷,
所以单独立一个测试把它钉住:

  1. 每个段都解析出 Address Unit Size, 且只取 8 / 16;
  2. 16 位段的字节长度必须是 size 的两倍 (这就是早先漏掉的那一半);
  3. 未初始化段 (off == 0) 的内容按 nbytes 个 0 字节计 —— 连字节数也要对;
  4. 相邻段在文件里不重叠, 即不会读到别的段里去;
  5. 在**每个非调试段的第二个四分之一**翻一个字节, canonical 必须发现差异。

第 5 条是核心: 它直接检验 "段的后半段也在证据范围内"。
"""
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from _common import ROOT, Checker

sys.path.insert(0, os.path.join(ROOT, 'tools'))
import coff_cmp

c = Checker('test_coff_section_bytes')

BUILD = os.path.join(ROOT, 'build', 'Debug')
# 覆盖三种形态: 16 位代码+常量 (f_common)、纯数据 (MotorVar)、汇编 (ASMSubPrg)
TARGETS = ['f_common.obj', 'MotorVar.obj', 'ASMSubPrg.obj']

avail = [t for t in TARGETS if os.path.isfile(os.path.join(BUILD, t))]
if not avail:
    print('  [SKIP] build/Debug 下没有目标文件, 先跑 tools/build_official.py')
    print('test_coff_section_bytes: 0 项, 失败 0 (已跳过)')
    sys.exit(0)

TMP = os.path.join(ROOT, 'build', '_coffsec')
os.makedirs(TMP, exist_ok=True)

sixteen_total = 0

for name in avail:
    p = os.path.join(BUILD, name)
    _txt, secs = coff_cmp.ofd_sections(p)
    withsize = [s for s in secs if s['size'] > 0]
    loaded = [s for s in withsize if s['off'] != 0]

    # ---- 1. 地址单元
    bad = [s['name'] for s in withsize if s['unit'] not in (8, 16)]
    c.check(bool(withsize) and not bad,
            '%s 每个段都有 8/16 位的 Address Unit Size (异常: %s)' % (name, bad))

    # ---- 2. 16 位段字节长度
    sixteen = [s for s in withsize if s['unit'] == 16]
    sixteen_total += len(sixteen)
    c.check(all(s['size'] * s['unit'] // 8 == 2 * s['size'] for s in sixteen),
            '%s 的 %d 个 16 位段字节长度 == 2 * size' % (name, len(sixteen)))

    # ---- 3. 未初始化段: 内容 == nbytes 个 0 字节
    uninit = [s for s in withsize if s['off'] == 0]
    canon = {s['name']: s for s in coff_cmp.canonical(p)['sections']}
    bad_uninit = []
    for s in uninit:
        nb = s['size'] * s['unit'] // 8
        want = hashlib.sha256(b'\x00' * nb).hexdigest()[:32]
        got = canon.get(s['name'], {}).get('data')
        if got != want:
            bad_uninit.append('%s(size=%d unit=%d nbytes=%d)' % (s['name'], s['size'], s['unit'], nb))
    c.check(not bad_uninit,
            '%s 的 %d 个未初始化段内容 == nbytes 个 0 字节 (异常: %s)'
            % (name, len(uninit), bad_uninit))

    # ---- 4. 相邻段不重叠
    ordered = sorted(loaded, key=lambda s: s['off'])
    over = []
    for a, b in zip(ordered, ordered[1:]):
        nb = a['size'] * a['unit'] // 8
        if a['off'] + nb > b['off']:
            over.append('%s(off=%d nbytes=%d) -> %s(off=%d)'
                        % (a['name'], a['off'], nb, b['name'], b['off']))
    c.check(not over, '%s 相邻段不重叠 (越界: %s)' % (name, over))

    # ---- 5. 段后半段的改动必须被发现
    raw = open(p, 'rb').read()
    base_h = {s['name']: s['data'] for s in coff_cmp.canonical(p)['sections']}
    tested = 0
    miss = []
    for s in secs:
        if s['name'].startswith(coff_cmp.SKIP_SEC):
            continue                      # canonical 有意不比调试段
        if s['size'] == 0 or s['off'] == 0:
            continue
        nb = s['size'] * s['unit'] // 8
        if nb < 4:
            continue                      # 太短, 没有明确的"后半段"
        idx = s['off'] + nb // 2 + nb // 4      # 第二个四分之一
        if idx >= len(raw):
            continue
        mod = bytearray(raw)
        mod[idx] ^= 0xFF
        mp = os.path.join(TMP, 'mut_%s' % name)
        open(mp, 'wb').write(bytes(mod))
        tested += 1
        try:
            mh = {x['name']: x['data'] for x in coff_cmp.canonical(mp)['sections']}
        except Exception as e:
            miss.append('%s(canonical 抛错 %s)' % (s['name'], e))
            continue
        if base_h.get(s['name']) == mh.get(s['name']):
            miss.append('%s(off=%d size=%d unit=%d)' % (s['name'], s['off'], s['size'], s['unit']))
    c.check(tested > 0, '%s 有可测的非调试段 (%d 个)' % (name, tested))
    c.check(not miss, '%s 段后半段改动全部被发现 (漏 %d: %s)' % (name, len(miss), miss))

# ---- 全局: 必须真的覆盖到 16 位段, 否则上面第 2 条是空转
c.check(sixteen_total > 0, '被测目标文件里存在 16 位地址单元段 (%d 个)' % sixteen_total)

sys.exit(0 if c.done() else 1)
