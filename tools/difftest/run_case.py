"""差分测试运行器: 编译 ref/new 两侧 + 驱动, 跑起来比对。

用法:  python tools/difftest/run_case.py <case> [随机用例数]

约定 (每个用例目录都要满足):
  cases/<name>/ref/<Mod>_ref.c   冻结的基准实现 (当前这份"与原工程字节等价"的版本)
  cases/<name>/ref_tu.c          改名后包含基准实现
  cases/<name>/new_tu.c          同样改名, 包含 src/... 下的交付源码
  cases/<name>/driver.c          枚举输入 + 逐字段比对

可选:
  cases/<name>/shim/             用例私有 shim, 优先于公共 shim/ 被搜索到
  cases/<name>/mocks.txt         本用例要编译的 mock 源文件清单 (相对 tools/difftest/),
                                 每行一个。不给则用默认的 hw_mock.c + hw_mock_p2p.c。
  cases/<name>/new/              负向对照用: 覆盖 srcdir.txt 指向的交付源码
  cases/<name>/srcdir.txt        交付源码所在目录 (默认 src/motor)
  cases/<name>/host_patch.json   宿主构建专用补丁 (见下), 目标端不使用

私有 shim 与 mocks.txt 是给"依赖项目真实结构体"的用例准备的: 这类用例要用真的
f_funcCode.h, 而公共 shim 里的 p2p_common.h 也声明了 funcCode, 两者会撞车。

host_patch.json 是给"宿主编译器编不过、但目标编译器编得过"的构造准备的。目前只有
一种: MSVC 的 C 前端拒绝 `&a.m1 - &a.m2` 当文件作用域静态初始化式 (error C2099),
而 C28x 的 cl2000 正常折叠。补丁把被影响的源文件改写成宿主可编译的副本, 放在
build/_difftest/gen_<case>/ 并优先于原目录被 include。补丁由 (old, new, count) 描述,
出现次数不符就直接报错停下 —— 源码一改, 补丁立刻暴露, 不会悄悄失配。
"""

import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, HERE)

import ccenv

SHIM = os.path.join(HERE, 'shim')
DEFAULT_MOCKS = ['hw_mock.c', 'hw_mock_p2p.c']


def case_defines(case):
    """cases/<case>/defines.txt -> 宿主侧要复刻的目标端宏列表。

    目标端用 -d_DEBUG -dDSP2803X -dLARGE_MODEL (见 tools/build_and_compare.py 的
    FLAGS)。不写进来的话, 宿主会按"宏未定义"去选 #if 分支 —— 那可能是目标端根本
    不会走的分支, 测了个寂寞。抽成函数是因为宿主探针 (target_layout.py) 也要用。
    """
    df = os.path.join(case, 'defines.txt')
    if not os.path.isfile(df):
        return []
    return [l.strip() for l in open(df, encoding='utf-8')
            if l.strip() and not l.startswith('#')]


def apply_host_patch(case_dir, newdir, gen_dir):
    """应用 cases/<name>/host_patch.json, 返回 {'made': [...], 'skipped': [...]}。

    没补丁时返回 None。

    每个 (old, new) 都要求 old 在源文件里出现恰好 count 次。不符就抛错: 与其
    拿一份"看着像但已经不对"的宿主副本去跑差分测试, 不如直接停下。

    **例外**是带 `lenient: true` 的规则 (见下面的长注释): 它允许"这次没命中",
    跳过并把跳过登记到 skipped 里。它存在的唯一理由是负向对照 —— 有些变异必然
    会改掉补丁锚点自身的文本, 而那条补丁只影响宿主侧的搬移宽度, 不影响两侧的
    对称性。跳过必须**可见** (进结果 JSON), 不许静默。

    每条替换可以带 `side` 字段, 取 `ref` / `new` / `both` (默认 both)。
    差分测试的两侧**故意**是不同文本: ref 侧是冻结的改写前版本, new 侧是改写
    后的版本。所以按文本匹配的补丁天然只能命中一侧 —— 比如 C2099 那条补丁要
    匹配"基准那张表的初始值设定项", 改写后文字变了就得另写一条 new 侧的。
    反过来, 纯宿主保真补丁 (例如 sizeof 的词/字节差) 两侧都要打, 用 both。
    没有这个字段时, 旧行为是两侧都打, 保留不变。
    """
    spec_path = os.path.join(case_dir, 'host_patch.json')
    if not os.path.isfile(spec_path):
        return None
    with open(spec_path, encoding='utf-8') as f:
        spec = json.load(f)

    os.makedirs(gen_dir, exist_ok=True)

    def _resolve(base, rel):
        """按 base 解析 rel; base 侧找不到就退回工程根。

        为什么需要这个退回: `ref_src` 相对**用例目录**解析, `new_base` 相对
        **new 侧源码目录**解析, 而后者在两种跑法下不是同一个地方 ——
        正常差分测试时 newdir = src/<模块目录> (由 srcdir.txt 给出), 负向对照
        时 newdir = <变体目录>/new (里面只有被测模块那一个 .c)。一个相对路径
        没法同时对上这两个基。

        退回只把"文件不存在 -> 直接报错"变成"再试一次工程根"。它**不会**改变
        任何当前能跑通的用例: base 侧命中时行为与以前逐字相同, 只有以前必然
        抛 SystemExit 的情况才会走到退回分支。负向对照的 new 侧编的是**冻结
        基准 + 一处变异**, 它 include 的头文件就是工程里那一份, 所以按工程根
        解析正是想要的。
        """
        p = os.path.normpath(os.path.join(base, rel))
        if os.path.isfile(p):
            return p
        q = os.path.normpath(os.path.join(ROOT, rel))
        return q if os.path.isfile(q) else p

    made = []
    skipped = []
    for mod in spec['modules']:
        for side, src_path, out_name in (
                ('ref', _resolve(case_dir, mod['ref_src']), mod['ref_out']),
                ('new', _resolve(newdir, mod['new_base']), mod['new_out'])):
            if not os.path.isfile(src_path):
                raise SystemExit('宿主补丁: 源文件不存在 %s' % src_path)
            with open(src_path, encoding='utf-8') as f:
                txt = f.read()
            for h in mod['replace']:
                if h.get('side', 'both') not in ('both', side):
                    continue
                n = txt.count(h['old'])
                if n != h['count']:
                    # `lenient: true` 的规则允许"这次没命中"。
                    #
                    # 存在的理由: 负向对照的 new 侧编的是"冻结基准 + 一处变异",
                    # 而某些变异**必然会**改掉补丁锚点本身的文本 —— 最典型的是
                    # 改 memcpy 的长度 / 源 / 目的 (那正是被测的行为)。这类补丁
                    # 的作用是**宿主保真**(把 C28x 的'字'折成宿主的'字节'), 它
                    # 失效只影响这一条语句在宿主上的搬移宽度, 不会让两侧产生
                    # 非对称的假差异: 同一处语句在 ref 侧用的是**同一个** n (变异
                    # 只打在 new 侧), 所以两侧仍然搬一样多的字节, 比对依旧公平。
                    #
                    # 因此这里的选择是"跳过并把跳过**记下来**", 而不是中止。
                    # 关键在"记下来": 调用方拿到 skipped 列表会写进结果 JSON,
                    # 所以"补丁退化了"永远可见, 不会变成静默降级。
                    if not h.get('lenient'):
                        raise SystemExit(
                            '宿主补丁失配: %s (%s 侧) 里 %r 出现 %d 次, 期望 %d 次 —— '
                            '源码改了, host_patch.json 必须重新核对'
                            % (src_path, side, h['old'][:60], n, h['count']))
                    skipped.append({'file': os.path.basename(src_path),
                                    'side': side,
                                    'anchor': h['old'][:80],
                                    'got': n, 'want': h['count']})
                    continue
                txt = txt.replace(h['old'], h['new'])
            with open(os.path.join(gen_dir, out_name), 'w', encoding='utf-8') as f:
                f.write(txt)
            made.append(out_name)
    return {'made': made, 'skipped': skipped}


def _real_header(name):
    """在 include/{motor,common,function,device} 里找 name 对应的真头文件。

    `device/` 是 T2 用例才需要的: 那些模块摸外设寄存器, 结构体定义在
    `include/device/DSP2803x_*.h` 里。转发它们不是为了把整个外设模型拉进宿主
    (`DSP2803x_Device.h` 有 `cregister` 这种宿主编不过的关键字, 见用例私有的
    `shim/DSP2803x_Device.h`), 而是为了让**被测模块真正引用的那一个**寄存器
    结构体保持真身 —— 位域布局对了, 差分测试才在测真东西。
    """
    for sub in ('motor', 'common', 'function', 'device'):
        p = os.path.join(ROOT, 'include', sub, name)
        if os.path.isfile(p):
            return p
    return None


# 这两个 shim 头的职责是**钉宽度**, 必须继续生效, 绝不能被真头文件顶掉。
# 尤其 DataTypeDef.h: 真文件把 u16 定义成 `unsigned int`, 宿主上是 32 位,
# 一转发进去整个测试台的宽度前提就没了。
PINNED_SHIMS = ('DataTypeDef.h',)


def _expand_star():
    """`*` 展开成"公共 shim 里所有有真身可用的头文件"。

    用例写 `*` 的场合: 模块经真头文件链 (f_comm.h -> f_main.h -> ...) 摸到好几个
    公共 shim 桩。这时**必须一次全转发** —— 只转发其中一个, 链条下游仍会撞上
    桩里的 `extern struct FUNCCODE_MOCK funcCode`, 和真 `FUNCCODE_ALL funcCode`
    重定义冲突。所以与其让人一个个数, 不如给一个"全都要"的写法。

    展开结果会记进记录的 `real_forwarded` 字段, 所以"到底转发了哪些"仍然可查,
    不是黑箱。
    """
    out = []
    for name in sorted(os.listdir(SHIM)):
        if not name.endswith('.h') or name in PINNED_SHIMS:
            continue
        if _real_header(name):
            out.append(name)
    return out


def make_real_passthroughs(case_dir, gen_dir):
    """把 `cases/<case>/real.txt` 里列出的头文件转发到**真头文件**。

    想用真头文件时, 光靠 include 顺序解决不了: 用例私有 shim 和公共 shim 都排在
    `include/` 前面, 同名头文件一定被它们挡住。所以这里生成一层转发头, 并把生成
    目录放在 include 路径**最前面**。转发头用**绝对路径** include 真文件, 不依赖
    相对层级 —— 用例目录以后挪位置也不会断。

    `real.txt` 每行一个头文件名; 单独一行 `*` 表示 _expand_star() 那一整批。
    """
    spec = os.path.join(case_dir, 'real.txt')
    if not os.path.isfile(spec):
        return []
    os.makedirs(gen_dir, exist_ok=True)

    names = []
    for line in open(spec, encoding='utf-8'):
        name = line.strip()
        if not name or name.startswith('#'):
            continue
        if name == '*':
            names.extend(_expand_star())
        else:
            names.append(name)

    made = []
    for name in names:
        if name in made:
            continue
        real = _real_header(name)
        if real is None:
            raise SystemExit('real.txt: include/ 下找不到 %s' % name)
        with open(os.path.join(gen_dir, name), 'w', encoding='utf-8') as f:
            f.write('/* 由 run_case.py 生成: 转发到真头文件, 绕开公共 shim。 */\n'
                    '#include %s\n' % json.dumps(real.replace('\\', '/')))
        made.append(name)
    return made


def run(name, nrand=200000, recname=None):
    # 用例目录默认在 tools/difftest/cases/ 下。负向对照的变体是**生成物**,
    # 放在 build/_difftest/negctl/ 里: 一是生成物不该混进源码树 (verify_tree.py
    # 会把变体副本当成交付源码去查编码/注释, 报一堆假错), 二是沙箱的批量删除
    # 守卫只放行 build/ 下的删除, 变体重建时的 rmtree 放外面会被直接拦下。
    # 所以 name 允许写成相对 ROOT 的路径。
    #
    # recname 是**落盘名**, 与用例目录名解耦: 变体的目录名是
    # build/_difftest/negctl/<case>_<编号>, 直接拿来当文件名会得到
    # build__difftest_negctl_xxx.json 这种又长又难认的名字。
    case = os.path.join(HERE, 'cases', name)
    if not os.path.isdir(case):
        case = os.path.join(ROOT, name)
    if not os.path.isdir(case):
        raise SystemExit('用例目录不存在: %s' % name)

    recname = recname or name.replace('/', '_')
    out = os.path.join(ROOT, 'build', '_difftest')
    os.makedirs(out, exist_ok=True)
    exe = os.path.join(out, recname + '.exe')

    # new 侧源码目录: 用例目录下有 new/ 则优先 (负向对照用);
    # 否则读 srcdir.txt (默认 src/motor), 指向交付源码。
    newdir = os.path.join(case, 'new')
    if not os.path.isdir(newdir):
        sd = os.path.join(case, 'srcdir.txt')
        rel = open(sd).read().strip() if os.path.isfile(sd) else 'src/motor'
        newdir = os.path.join(ROOT, rel)

    gen_dir = os.path.join(out, 'gen_' + name.replace('/', '_'))
    patch_res = apply_host_patch(case, newdir, gen_dir)
    patched = patch_res['made'] if patch_res else None
    patch_skipped = patch_res['skipped'] if patch_res else []
    realfwd = make_real_passthroughs(case, gen_dir)

    # 宿主补丁跳过了就必须喊出来 —— 静默降级比失配更危险。
    for sk in patch_skipped:
        print('[warn] 宿主补丁跳过 (%s 侧, %s): %r 命中 %d 次, 期望 %d 次'
              % (sk['side'], sk['file'], sk['anchor'][:60],
                 sk['got'], sk['want']))

    # 生成目录排最前: 里面是宿主补丁副本和"真头文件"转发头, 都必须压过所有 shim。
    inc = [gen_dir]
    case_shim = os.path.join(case, 'shim')
    if os.path.isdir(case_shim):
        inc.append(case_shim)                   # 用例私有 shim 优先
    inc += [
        SHIM,                                   # 精确宽度类型 + 公共 shim 头
        os.path.join(ROOT, 'include', 'motor'),
        os.path.join(ROOT, 'include', 'common'),
        os.path.join(ROOT, 'include', 'function'),
        os.path.join(case, 'ref'),              # <Mod>_ref.c
        newdir,
    ]

    mf = os.path.join(case, 'mocks.txt')
    if os.path.isfile(mf):
        mocks = [l.strip() for l in open(mf, encoding='utf-8')
                 if l.strip() and not l.startswith('#')]
    else:
        mocks = DEFAULT_MOCKS

    # cases/<case>/defines.txt: 宿主侧要复刻的目标端宏。见 case_defines()。
    defines = case_defines(case)

    srcs = [
        os.path.join(case, 'ref_tu.c'),
        os.path.join(case, 'new_tu.c'),
        os.path.join(case, 'driver.c'),
    ] + [os.path.join(HERE, m) for m in mocks]

    rc, log = ccenv.compile_exe(srcs, exe, incdirs=inc, defines=defines)
    if rc != 0:
        print(log)
        raise SystemExit('编译失败 rc=%d' % rc)

    # 超时是**第三种结局**, 必须与"编不过"分开:
    #
    #   rc =  0  跑完且 0 处不一致        -> 变体没被抓到 (MISSED)
    #   rc != 0  跑完且有 N 处不一致      -> 抓到 (CAUGHT)
    #   rc = -2  跑起来了但**没跑完**     -> 变体不终止
    #   rc = -1  压根没编译/没启动        -> 不构成证据 (BROKEN)
    #
    # -2 这一格里, "挂之前已经跑出差异"是**强证据** (差异 + 不终止), 而"一处差异
    # 都没有就挂了"只说明终止性变了。两种都由调用方按现场文本判定, 所以现场必须
    # 完整落盘 —— 这正是 ExeTimeout 存在的理由。
    timed_out = False
    try:
        rc, log = ccenv.run_exe(exe, cwd=out, args=[str(nrand)])
    except ccenv.ExeTimeout as e:
        rc = -2
        timed_out = True
        log = (e.stdout or '') + (e.stderr or '')
        print(e)
        print('---- 超时前已产出的 stdout (%d 字节, 尾部 4000) ----' % len(log))
        print(log.rstrip()[-4000:])
    else:
        print(log.rstrip())

    # 把结果落盘, 供 test_difftest.py 与证据文档引用 (不靠人肉转述数字)
    #
    # stdout 要截断保存: 差异明细是按条打印的, 一个错得很彻底的变体能刷出
    # 几十兆 (实测 36 MB), 会把 docs/_difftest 撑爆。只留头部, 并把原始长度
    # 记下来 —— 截断这件事本身必须是可见的, 不能悄悄丢。
    MAX_STDOUT = 200000
    if len(log) > MAX_STDOUT:
        stored = log[:MAX_STDOUT] + '\n... [stdout 已截断, 原始 %d 字节]\n' % len(log)
    else:
        stored = log
    rec = {'case': recname, 'random': nrand, 'rc': rc, 'stdout': stored.strip(),
           'stdout_bytes': len(log), 'timed_out': timed_out,
           'host_patched': sorted(patched) if patched else [],
           'host_patch_skipped': patch_skipped,
           'real_forwarded': sorted(realfwd) if realfwd else [],
           'defines': sorted(defines) if defines else []}
    m = re.search(r'cases=(\d+) mismatches=(\d+)', log)
    if m:
        rec['cases'] = int(m.group(1))
        rec['mismatches'] = int(m.group(2))
    docdir = os.path.join(ROOT, 'docs', '_difftest')
    os.makedirs(docdir, exist_ok=True)
    with open(os.path.join(docdir, recname + '.json'), 'w',
              encoding='utf-8') as f:
        json.dump(rec, f, ensure_ascii=False, indent=2)
    return rc


if __name__ == '__main__':
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 200000
    sys.exit(run(sys.argv[1], n))
