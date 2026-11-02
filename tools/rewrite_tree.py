"""把 bpqym 重写为独立的新工程 bpqdm_rewrite。

流程: 权威构建清单 -> 映射到新目录结构 -> 词法级去注释 -> token 流等价校验 -> 回写。
条件编译 (#if/#ifdef) 原样保留, 不解析 (见 docs/rewrite_decisions.md)。
"""
import os, re, sys, json, hashlib, shutil, collections

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cstrip as C

SRC = r"E:/software/bpqdm/bpqym"
DST = r"E:/software/bpqdm/bpqdm_rewrite"
DOC = os.path.join(DST, 'docs')

# ------------------------------------------------------------------ 映射规则
def map_built(d, stem, ext):
    """d = 原目录 (function/motor/subprogram/system); ext = .c/.asm"""
    if d == 'function':
        if stem == 'main':
            return 'src/system/main.c'
        if stem in ('DSP2803x_CpuTimers', 'DSP2803x_GlobalVariableDefs'):
            return 'src/system/%s.c' % stem
        if stem == 'DSP2803x_usDelay':
            return 'asm/%s.asm' % stem
        return 'src/function/%s.c' % stem
    if d == 'motor':
        return 'src/motor/%s.c' % stem
    if d == 'subprogram':
        if stem == 'ASMSubPrg':
            return 'asm/%s.asm' % stem
        return 'src/subprogram/%s.c' % stem
    if d == 'system':
        if stem in ('DSP2803x_CodeStartBranch', 'DSP2803x_CSMPasswords'):
            return 'asm/%s.asm' % stem
        return 'src/system/%s.c' % stem
    raise SystemExit('unmapped built source: %s/%s%s' % (d, stem, ext))


def lkf_entries():
    """从 Debug.lkf 取权威构建清单: 保留原始目录, 避免同名文件歧义。"""
    txt = open(os.path.join(SRC, 'CCs', 'Debug.lkf'), encoding='gbk', errors='replace').read()
    res = []
    for line in txt.splitlines():
        m = re.match(r'^\s*"(.+?\.obj)"\s*$', line, re.I)
        if not m:
            continue
        p = m.group(1).replace('\\', '/')
        parts = p.split('/')
        res.append((parts[-2], parts[-1][:-4]))
    return res


DEV_H = [h for h in sorted(os.listdir(os.path.join(SRC, 'public/28035_header')))
         if h.startswith('DSP2803x_') and h.endswith('.h')]
DEV_H += ['DSP28x_Project.h', 'IQmathLib.h', 'SFO_V6.h']

COMMON_H = ['public/28035_header/SystemDefine.h',
            'public/motor_header/DataTypeDef.h',
            'public/motor_header/MotorDefine.h',
            'public/motor_header/MotorStructDefine.h',
            'public/function_header/f_common.h']

MOTOR_H_EXTRA = ['public/28035_header/MotorVCInclude.h']
MOTOR_H_DROP = {'DSP2803x_Device.h', 'DataTypeDef.h', 'MotorDefine.h',
                'MotorStructDefine.h', 'MotorVCInclude.h'}

FUNC_H = [h for h in sorted(os.listdir(os.path.join(SRC, 'public/function_header')))
          if (h.endswith('.h') or h.endswith('.c')) and h != 'f_common.h']

# 目录中存在但不在 Debug.lkf 链接清单里的源码 (孤儿文件)
ORPHANS = ['function/DSP2803x_PieCtrl.c',
           'function/DSP2803x_SysCtrl.c',
           'function/f_length.c',
           'function/z_func.c']


def build_map():
    m = []
    for d, stem in lkf_entries():
        for ext in ('.c', '.asm'):
            cand = '%s/%s%s' % (d, stem, ext)
            if os.path.exists(os.path.join(SRC, cand)):
                m.append((cand, map_built(d, stem, ext),
                          'asm' if ext == '.asm' else 'c'))
                break
        else:
            raise SystemExit('cannot locate source for %s/%s' % (d, stem))
    for h in DEV_H:
        m.append(('public/28035_header/' + h, 'include/device/' + h, 'h'))
    for h in COMMON_H:
        m.append((h, 'include/common/' + os.path.basename(h), 'h'))
    for h in FUNC_H:
        m.append(('public/function_header/' + h, 'include/function/' + h, 'h'))
    for h in MOTOR_H_EXTRA:
        m.append((h, 'include/motor/' + os.path.basename(h), 'h'))
    for h in sorted(os.listdir(os.path.join(SRC, 'public/motor_header'))):
        if not h.endswith('.h') or h in MOTOR_H_DROP:
            continue
        m.append(('public/motor_header/' + h, 'include/motor/' + h, 'h'))
    # 孤儿文件: 目录中存在但不参与链接。仍然交付干净版本, 单独存放并标注。
    for o in ORPHANS:
        m.append((o, 'src/orphan/' + os.path.basename(o),
                  'asm' if o.endswith('.asm') else 'c'))
    return m


def rewritten_set():
    """docs/rewritten_modules.json 里列出的模块: 实现已被改写, 不能被本脚本覆盖。

    本脚本是"从 bpqym 重新生成一遍"的幂等入口。如果不豁免这些模块, 重跑一次就会把
    改写过的实现悄悄换回原样 —— 而且不会报错, 因为下面的 token 断言比的是
    "bpqym 原文 vs 它的去注释结果", 压根没看交付文件。这类静默回退比报错更危险。
    """
    p = os.path.join(DOC, 'rewritten_modules.json')
    if not os.path.isfile(p):
        return set()
    data = json.load(open(p, encoding='utf-8'))
    return {m['dst'] for m in data.get('modules', [])}


def main():
    M = build_map()
    KEEP = rewritten_set()
    # 断言: 目标路径唯一
    seen = collections.Counter(d for _, d, _ in M)
    dup = [k for k, v in seen.items() if v > 1]
    if dup:
        raise SystemExit('destination collision: %s' % dup)
    # 断言: 各 include 目录内 basename 不冲突
    byname = collections.defaultdict(set)
    for _, d, _ in M:
        if d.startswith('include/'):
            byname[os.path.basename(d)].add(os.path.dirname(d))
    clash = {k: v for k, v in byname.items() if len(v) > 1}
    if clash:
        raise SystemExit('header basename clash: %s' % clash)

    report = []
    stats = dict(files=0, c=0, asm=0, h=0, cmd=0, comments_removed=0,
                 ascii_after=0, nonascii_after=0, mixed_eol=0, preserved=0)
    if KEEP:
        print('rewritten (preserved, not regenerated): %s' % sorted(KEEP))

    # 幂等: 清掉源码树里不在本次映射内的残留文件 (只动源码目录, 不碰 docs/tools/project/linker)
    # 只删源码扩展名: 该目录下还有手写文档 (如 src/orphan/README.md), 它不是生成物,
    # 误删会造成不可恢复的丢失 (本工程不是 git 仓库)。宁可不删, 也不删错。
    STALE_EXT = ('.c', '.h', '.asm')
    want = {d for _, d, _ in M}
    for root in ('src', 'asm', 'include'):
        base = os.path.join(DST, root)
        if not os.path.isdir(base):
            continue
        for r, _dirs, files in os.walk(base):
            for fn in files:
                rel = os.path.relpath(os.path.join(r, fn), DST).replace('\\', '/')
                if rel not in want and fn.lower().endswith(STALE_EXT):
                    os.remove(os.path.join(r, fn))
                    print('removed stale:', rel)

    for srel, drel, kind in M:
        sp = os.path.join(SRC, srel)
        dp = os.path.join(DST, drel)
        os.makedirs(os.path.dirname(dp), exist_ok=True)
        raw = open(sp, 'rb').read()
        text, enc = C.decode_any(raw)
        eol = C.eol_of(text)
        if eol == 'mixed':
            stats['mixed_eol'] += 1
            eol = 'CRLF' if text.count('\r\n') >= (text.count('\n') - text.count('\r\n')) else 'LF'

        if kind == 'asm':
            stripped, nrem = C.strip_asm(text)
        else:
            stripped, nrem = C.strip_c(text)
        final = C.tidy(stripped, eol)

        # 往返一致性
        back = C.encode_back(final, enc)
        assert C.decode_any(back)[0] == final, 'roundtrip failed: ' + srel

        # token 流等价 (C/头文件/汇编行级)
        if kind in ('c', 'h'):
            a = C.c_tokens(text)
            b = C.c_tokens(final)
            if a != b:
                raise SystemExit('TOKEN MISMATCH: %s' % srel)
        else:
            ka = [l.strip() for l in text.replace('\r\n', '\n').split('\n') if l.strip()]
            kb = [l.strip() for l in final.replace('\r\n', '\n').split('\n') if l.strip()]
            ka = [l for l in ka if not l.startswith('*')]
            ka = [re.sub(r';.*$', '', l).strip() for l in ka]
            ka = [l for l in ka if l]
            if ka != kb:
                raise SystemExit('ASM MISMATCH: %s\n%r\n%r' % (srel, ka[:5], kb[:5]))

        if drel in KEEP:
            # 实现已改写。上面的 token 断言比的是 "bpqym 原文 vs 它的去注释结果",
            # 跟交付文件无关, 所以这里必须显式跳过写入 —— 否则重跑一次就把改写
            # 悄悄换回原样, 而且不报任何错。元信息仍然登记, _map.json 结构不变。
            if not os.path.isfile(dp):
                raise SystemExit('rewritten module missing: %s' % drel)
            stats['preserved'] += 1
        else:
            with open(dp, 'wb') as f:
                f.write(back)

        try:
            final.encode('ascii')
            stats['ascii_after'] += 1
        except UnicodeEncodeError:
            stats['nonascii_after'] += 1
        stats['files'] += 1
        stats[kind if kind in ('c', 'asm', 'h') else 'c'] += 1
        stats['comments_removed'] += nrem
        report.append(dict(src=srel, dst=drel, kind=kind, enc=enc, eol=eol,
                           comments=nrem, md5_src=hashlib.md5(raw).hexdigest()))

    json.dump(report, open(os.path.join(DOC, '_map.json'), 'w', encoding='utf-8'),
              ensure_ascii=False, indent=1)
    print(json.dumps(stats, indent=1))
    print('mapped files:', len(M))


main()
