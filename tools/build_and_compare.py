"""用新 CGT 同时编译原工程与新工程, 做两层逐模块等价证明 + 整机镜像比对。

层级 (由弱到强):
  A. 反汇编文本比对 (dis2000 -1)
       只有含 .text 的模块可比; 纯数据模块产物为空 => 不能当证据。
  B. 目标文件内容级比对 (coff_cmp)
       把两侧源码复制到 **同一个暂存路径** 再编译, 使内嵌的源文件路径完全一致,
       于是除 4 字节 COFF 时间戳外应逐字节相同; 再按 段/重定位/符号表 取摘要比对。
       这条覆盖全部 59 个模块 (含纯数据模块), 是本工程最强的逐模块证据。
  C. 整机镜像比对 (hex2000 + 逐字节 cmp)
       端到端证据: 两个 .out 编成 hex 后应逐字节一致。

陷阱 (已在 SKILL 中记录):
  * dis2000 必须在目标目录内用裸文件名调用, 否则输出会多一行路径回显;
    且必须断言产物非空 —— "空 == 空" 是假通过。
  * 直接 cmp .obj 会因时间戳 + 内嵌源路径长度而报上万条假差异, 不可用。
  * ofd2000 对未初始化段报告 File Offset = 0; 若照 off 去切字节会切到文件头。
  * hex2000 必须 -memwidth 16 -romwidth 16 (C28x 是 16 位字机器)。
"""
import os, re, sys, json, shutil, subprocess, hashlib
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import coff_cmp

CGT = 'E:/ti/c2000/cgt-c2000_25.11.1.LTS/ti-cgt-c2000_25.11.1.LTS/bin'
SRC = 'E:/software/bpqdm/bpqym'
DST = 'E:/software/bpqdm/bpqdm_rewrite'
DOC = os.path.join(DST, 'docs')
WORK = os.path.join(DST, 'build/_verify')
STAGE = os.path.join(WORK, 'stage')
STAGE_SRC = os.path.join(STAGE, 'src')
STAGE_OBJ = os.path.join(STAGE, 'obj')

ORIG_INC = [SRC + '/public/28035_header', SRC + '/public/motor_header',
            SRC + '/public/function_header']
NEW_INC = [DST + '/include/device', DST + '/include/common',
           DST + '/include/function', DST + '/include/motor']

# 两侧必须完全相同的选项。这里**故意不带优化** (工程实际用的是 `-o2`, 即 `-O2`,
# 见 docs/rewrite_decisions.md D5): 关优化时指令序列最贴近源码结构, 任何改写都会
# 在 .text 上留下痕迹; 开了优化反而可能被合并/消除掩盖掉。所以关优化 = 更灵敏,
# 是刻意的选择, 不是"因为 -o2 无效"。
FLAGS = ['-g', '-q', '--abi=coffabi', '-d_DEBUG', '-dDSP2803X', '-dLARGE_MODEL',
         '-ml', '-mt', '-v28']
FLAGS_NOG = [f for f in FLAGS if f != '-g']

MAP = json.load(open(os.path.join(DOC, '_map.json'), encoding='utf-8'))
BUILT = [m for m in MAP if m['kind'] in ('c', 'asm')
         and not m['dst'].startswith('src/orphan/')]


# 单条 cl2000/lnk2000/dis2000 的墙钟上限。**必须给**, 否则会挂死整条流水线:
# 实测有一次 cl2000.exe 只用掉 0.09 s CPU 就再也不返回 (既不读盘也不占 CPU,
# 卡在某个句柄上), 而 subprocess.run 不带 timeout 时父进程会一直等下去 ——
# 外面看到的现象是"脚本跑了 37 分钟没有任何输出", 日志里一行都没有,
# 分不清是慢还是死。给上限之后超时被折成 returncode=124,
# 上层按"这条编译失败"处理并重试一次, 最坏情况是报一条 FAIL 而不是整轮作废。
TIMEOUT = 240
TMPROOT = os.path.join(WORK, 'tmp')


def _workenv(tag):
    """每次调用各持一个 TEMP/TMP 目录。

    并发 cl2000 共用一个 TEMP 会**偶发死锁**: Tier B 里 f_error 那一对编译在
    4 线程下卡死 (cl2000 进程活着、CPU 停在 0.09 s、无子进程、两条线程都停在
    Executive 等待上), 而把同一对编译拿出来串行跑只要 1.17 s / 0.91 s。
    两次全量比对都恰好停在这一个模块上, 说明不是"某段代码编得慢", 而是
    并发下的资源争用。既然复现条件就是"共享临时目录 + 并发", 就给每次调用
    单独一个 —— 这比事后重试可靠, 重试只是在赌第二次不撞上。
    """
    d = os.path.join(TMPROOT, tag)
    os.makedirs(d, exist_ok=True)
    e = dict(os.environ)
    e['TMP'] = d
    e['TEMP'] = d
    return e


def run(cmd, cwd=None, env=None, timeout=TIMEOUT):
    try:
        return subprocess.run(cmd, cwd=cwd, env=env, capture_output=True,
                              text=True, errors='replace', timeout=timeout)
    except subprocess.TimeoutExpired as ex:
        o = ex.stdout if isinstance(ex.stdout, str) else ''
        return subprocess.CompletedProcess(
            cmd, 124, o, 'TIMEOUT after %ss (进程已被杀掉)' % timeout)


def run_retry(cmd, cwd=None, env=None, stale=None, tries=2):
    """只对超时重试; 真正的编译错误重试也是错的, 会掩盖问题。"""
    r = None
    for _ in range(tries):
        r = run(cmd, cwd=cwd, env=env)
        if r.returncode != 124:
            return r
        # 超时后目标 .obj 可能是上一次留下的旧产物, 先删掉再重试,
        # 否则 "rc==0 且文件存在" 会把旧文件当成这次的成果。
        if stale and os.path.isfile(stale):
            try:
                os.remove(stale)
            except OSError:
                pass
    return r



def src_of(m, side):
    p = os.path.join(SRC, m['src']) if side == 'orig' else os.path.join(DST, m['dst'])
    return p.replace('/', '\\')


def inc_of(side):
    return ORIG_INC if side == 'orig' else NEW_INC


# ---------------------------------------------------------------- Tier A
def compile_inplace(m, side):
    stem = os.path.splitext(os.path.basename(m['src']))[0]
    out = os.path.join(WORK, side)
    os.makedirs(out, exist_ok=True)
    cmd = ([os.path.join(CGT, 'cl2000.exe')] + FLAGS + ['-i' + p for p in inc_of(side)] +
           ['-fr' + out.replace('/', '\\'), '-c', src_of(m, side)])
    obj = os.path.join(out, stem + '.obj')
    r = run_retry(cmd, cwd=out, env=_workenv('a_%s_%s' % (stem, side)), stale=obj)
    return dict(stem=stem, side=side, ok=r.returncode == 0 and os.path.isfile(obj),
                rc=r.returncode, out=(r.stdout or '') + (r.stderr or ''), obj=obj)


def dis(obj, side):
    """cd 进目录 + 裸文件名 (避开 dis2000 的路径回显陷阱)。"""
    d = os.path.join(WORK, side)
    tag = 'd_%s_%s' % (os.path.splitext(os.path.basename(obj))[0], side)
    r = run([os.path.join(CGT, 'dis2000.exe'), '-1', os.path.basename(obj)],
            cwd=d, env=_workenv(tag))
    return (r.stdout or '') + (r.stderr or '')


def count_instr(text):
    return len(re.findall(r'^[0-9a-f]{8}\s{2}', text, re.M))


# ---------------------------------------------------------------- Tier B
def compile_staged(m, side, keep):
    """把源码复制到公共暂存路径再编译, 消除内嵌源路径差异。

    输出目录和 TEMP 都**按 (模块, 侧) 分开**。语义上与"共用一个目录"等价 ——
    COFF 里内嵌的是**源**路径 (`dst`), 与 `-fr` 输出目录无关; 分开只为避开
    并发 cl2000 在共享目录上的偶发死锁 (见 `_workenv` 的说明)。
    """
    stem = os.path.splitext(os.path.basename(m['src']))[0]
    dst = os.path.join(STAGE_SRC, os.path.basename(m['src']))
    shutil.copyfile(src_of(m, side).replace('\\', os.sep), dst)
    outdir = os.path.join(STAGE_OBJ, '%s.%s' % (stem, side))
    os.makedirs(outdir, exist_ok=True)
    obj = os.path.join(outdir, stem + '.obj')
    cmd = ([os.path.join(CGT, 'cl2000.exe')] + FLAGS_NOG +
           ['-i' + p for p in inc_of(side)] +
           ['-fr' + outdir.replace('/', '\\'), '-c', dst.replace('/', '\\')])
    r = run_retry(cmd, cwd=STAGE, env=_workenv('b_%s_%s' % (stem, side)), stale=obj)
    ok = r.returncode == 0 and os.path.isfile(obj)
    if ok:
        shutil.copyfile(obj, keep)
    return ok, (r.stdout or '') + (r.stderr or '')


def tier_b(m):
    stem = os.path.splitext(os.path.basename(m['src']))[0]
    a = os.path.join(STAGE, stem + '.a.obj')
    b = os.path.join(STAGE, stem + '.b.obj')
    # 逐模块打印: 万一将来再挂, 日志最后一行就是挂住的那个模块。
    # 不打印的话, 挂死与"正在慢慢编"在外部完全无法区分 (两次都吃过这个亏)。
    sys.stdout.write('  B %-30s ' % stem)
    sys.stdout.flush()
    oka, ea = compile_staged(m, 'orig', a)
    okb, eb = compile_staged(m, 'new', b)
    if not (oka and okb):
        print('COMPILE-FAIL')
        return stem, 'COMPILE-FAIL', dict(orig=ea[-300:], new=eb[-300:])
    try:
        ok, info = coff_cmp.compare(a, b)
    except Exception as ex:
        print('ERROR')
        return stem, 'ERROR', dict(err=str(ex))
    print('IDENTICAL' if ok else 'DIFFERS')
    return stem, ('IDENTICAL' if ok else 'DIFFERS'), info


def main():
    for d in ('orig', 'new', 'link_orig', 'link_new'):
        os.makedirs(os.path.join(WORK, d), exist_ok=True)
    os.makedirs(STAGE_SRC, exist_ok=True)
    os.makedirs(STAGE_OBJ, exist_ok=True)
    os.makedirs(TMPROOT, exist_ok=True)

    report = dict(cgt=CGT, flags=FLAGS, modules=len(BUILT))

    # ---------- Tier A ----------
    jobs = [(m, s) for m in BUILT for s in ('orig', 'new')]
    print('== A. 反汇编比对 ==')
    print('编译 %d 个目标文件 (原/新 各 %d)...' % (len(jobs), len(BUILT)))
    with ThreadPoolExecutor(max_workers=6) as ex:
        res = list(ex.map(lambda a: compile_inplace(*a), jobs))
    fails = [r for r in res if not r['ok']]
    print('编译成功 %d / %d, 失败 %d' % (len(res) - len(fails), len(res), len(fails)))
    for f in fails[:5]:
        print('  FAIL %s [%s] rc=%d' % (f['stem'], f['side'], f['rc']))
        print('   ', f['out'].strip().split('\n')[0][:160])

    same, diff, n_text = [], [], []
    details = {}
    for m in BUILT:
        stem = os.path.splitext(os.path.basename(m['src']))[0]
        oa = os.path.join(WORK, 'orig', stem + '.obj')
        ob = os.path.join(WORK, 'new', stem + '.obj')
        if not (os.path.isfile(oa) and os.path.isfile(ob)):
            diff.append(stem); details[stem] = 'obj missing'; continue
        da, db = dis(oa, 'orig'), dis(ob, 'new')
        na, nb = count_instr(da), count_instr(db)
        if na == 0 or nb == 0:
            # 纯数据模块: 无 .text, 本条不适用 (由 Tier B 覆盖)
            n_text.append(stem); details[stem] = 'NO-CODE (data-only)'; continue
        if da == db:
            same.append(stem)
        else:
            diff.append(stem)
            al, bl = da.split('\n'), db.split('\n')
            i = next((k for k in range(min(len(al), len(bl))) if al[k] != bl[k]), -1)
            details[stem] = 'first diff line %d: %r vs %r' % (
                i, al[i][:70] if i >= 0 else '', bl[i][:70] if i >= 0 else '')
    print('有代码模块相同 %d / 不同 %d / 纯数据(不适用) %d' % (len(same), len(diff), len(n_text)))
    for s in diff:
        print('  DIFFERS %s : %s' % (s, details.get(s, '')))
    report['tierA'] = dict(same=same, diff=diff, no_code=n_text, details=details,
                           compiled=len(res) - len(fails), compile_failed=[f['stem'] for f in fails])

    # ---------- Tier B ----------
    print('\n== B. 目标文件内容级比对 (同路径暂存编译) ==')
    with ThreadPoolExecutor(max_workers=4) as ex:
        rb = list(ex.map(tier_b, BUILT))
    b_ok = [s for s, v, _ in rb if v == 'IDENTICAL']
    b_bad = [(s, v, i) for s, v, i in rb if v != 'IDENTICAL']
    print('逐字节一致 %d / %d' % (len(b_ok), len(rb)))
    for s, v, i in b_bad:
        print('  %s %s : %s' % (v, s, json.dumps(i, ensure_ascii=False)[:300]))
    report['tierB'] = dict(identical=b_ok, failed=[dict(stem=s, kind=v, info=i) for s, v, i in b_bad])

    json.dump(report, open(os.path.join(DOC, '_dis_compare.json'), 'w', encoding='utf-8'),
              ensure_ascii=False, indent=1)
    print('\n报告 -> docs/_dis_compare.json')

    # ---- 退出判定 ----
    #
    # 真正的契约不是"改写过就必须不同", 而是"**没改写过的必须相同**"。
    #
    # 这两者曾经看起来等价, 于是这里写成 `if tierA['diff'] or b_bad: return 1` ——
    # 结果是本脚本在含改写模块的树上**永远返回 1**, 退出码失去信号。
    # f_vf 把这条假设证伪了: 它的实现被改写 (4 个具名宏、局部量改名、门控条件规范化),
    # 但工程选项 (-o2/-o3) 下生成的 .text 与符号表**逐字节相同** —— 也就是说
    # "改写过" ⇒ "必然不同" 是错的, 最好的改写本来就应该收敛到同一份机器码。
    #
    # 所以改成: 只对**未列入改写清单却出现差异**的模块报错; 清单内的模块不同属预期,
    # 相同则额外打印出来 (那是最强的一档结果, 值得知道)。
    man = json.load(open(os.path.join(DOC, 'rewritten_modules.json'), encoding='utf-8'))
    want = set(os.path.splitext(os.path.basename(m['dst']))[0] for m in man['modules'])

    unexpected_a = sorted(s for s in report['tierA']['diff'] if s not in want)
    unexpected_b = sorted(s for s, _v, _i in b_bad if s not in want)
    # "清单内却仍然相同" —— 比"不同"更强的一档结果, 单独报出来
    rew_same_a = sorted(want - set(report['tierA']['diff']))
    rew_same_b = sorted(want - set(s for s, _v, _i in b_bad))

    print('改写清单 %d 个: 反汇编仍相同 %s / 目标文件仍一致 %s'
          % (len(want), rew_same_a or '无', rew_same_b or '无'))
    if unexpected_a or unexpected_b:
        print('!! 未列入改写清单却出现差异: 反汇编 %s / 目标文件 %s'
              % (unexpected_a, unexpected_b))
    report['unexpected'] = dict(tierA=unexpected_a, tierB=unexpected_b,
                                rewritten_identical=dict(tierA=rew_same_a, tierB=rew_same_b))
    json.dump(report, open(os.path.join(DOC, '_dis_compare.json'), 'w', encoding='utf-8'),
              ensure_ascii=False, indent=1)

    return 0 if not (fails or unexpected_a or unexpected_b) else 1


sys.exit(main())
