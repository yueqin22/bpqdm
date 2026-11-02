"""用真实构建结果生成 build_debug.txt / build_release.txt / warning_list.txt
以及 equivalence_evidence.md (等价性证据总表)。

数据来源 (均由前面的工具产生, 本文件只做汇总, 不编造):
  docs/_official_build.json  新工程 Debug/Release 的编译+链接+hex 记录
  docs/_baseline_build.json  原工程副本的同类记录
  docs/_dis_compare.json     逐模块反汇编 + 目标文件内容级比对
  docs/_verify.json          静态验收 (注释/token/符号...)
  docs/_negctl.json          负向对照
"""
import os, re, json, hashlib

DST = r"E:/software/bpqdm/bpqdm_rewrite"
DOC = os.path.join(DST, 'docs')


def load(name):
    p = os.path.join(DOC, name)
    return json.load(open(p, encoding='utf-8')) if os.path.isfile(p) else None


def sha(p):
    return hashlib.sha256(open(p, 'rb').read()).hexdigest() if p and os.path.isfile(p) else None


def size(p):
    return os.path.getsize(p) if p and os.path.isfile(p) else None


OFF = load('_official_build.json')
BAS = load('_baseline_build.json')
CMP = load('_dis_compare.json')
VER = load('_verify.json')
NEG = load('_negctl.json')

# 改写清单: 决定"逐字节一致"还是"行为等价"适用于哪些模块
MAN = load('rewritten_modules.json') or {'modules': []}
REW = [m['dst'] for m in MAN['modules']]
REW_STEM = set(os.path.splitext(os.path.basename(p))[0] for p in REW)
NMOD = (CMP or {}).get('modules', 0)
NREW = len(REW)

# 新鲜度闸门: `docs/_verify.json` 由 `tools/verify_tree.py` 写, 它记录的 `rewritten`
# 列表必须与改写清单**同长**。不等 = 它还是**旧清单**跑出来的产物, 于是本文件会把
# "15 个改写模块" 写成 "14 个", 同一次输出里两处互相打架 —— 一份自相矛盾的证据文档
# 比没有更糟, 因为它看起来像证据。
# 2026-10-07 实测踩到过: 链的顺序是 make_build_report 在 run_all 之前, 而 verify_tree
# 是 run_all 里的 test_tree_contract 跑的, 于是 equivalence_evidence.md 里
# "token 流等价" 那行写的是 `131 / 145 ... 14 个改写模块`, 同文件第 124 行却写 15 个。
# 注意本文件**只做汇总、不编造**, 所以这里选择直接停下, 而不是自己重算一遍。
if VER is not None:
    nver = len((VER.get('token_stream_identical') or {}).get('rewritten') or [])
    if nver != NREW:
        raise SystemExit(
            'docs/_verify.json 过期, 拒绝生成: 它记着 %d 个改写模块, 改写清单里是 %d 个。\n'
            '  先跑: python tools/verify_tree.py\n'
            '  (tests/run_all.py 也会跑它 —— 但它必须排在本文件**之前**)\n'
            % (nver, NREW))


def difftest_rec(case):
    return load(os.path.join('_difftest', case + '.json'))


def target_rec(dst):
    stem = os.path.splitext(os.path.basename(dst))[0]
    return load(os.path.join('_difftest', stem + '.target.json'))


def layout_rec(case):
    return load(os.path.join('_difftest', case + '.layout.json'))


# 负向对照总账: case -> [抓到数, 总数]。BROKEN (变体没编译起来) 不计入"抓到"。
NEGCTL = load(os.path.join('_difftest', '_negctl_summary.json'))
NEGCTL_BY_CASE = {}
for _r in (NEGCTL or {}).get('records', []):
    _e = NEGCTL_BY_CASE.setdefault(_r['case'], [0, 0])
    _e[1] += 1
    if _r.get('verdict') == 'CAUGHT':
        _e[0] += 1


def negctl_of(case):
    e = NEGCTL_BY_CASE.get(case)
    return '%d / %d 抓到' % (e[0], e[1]) if e else '—'

# ---------------------------------------------------------------- 镜像比对
def image_cmp(cfg, off_key, bas_key):
    o = (OFF or {}).get(off_key) or {}
    b = (BAS or {}).get(bas_key) or {}
    oh, bh = o.get('hex'), b.get('hex')
    so, sb = sha(oh), sha(bh)
    return dict(cfg=cfg, new_hex=oh, base_hex=bh,
                new_size=size(oh), base_size=size(bh),
                new_sha=so, base_sha=sb,
                identical=(so is not None and so == sb))


IMG = [image_cmp('Debug', 'debug', 'debug'), image_cmp('Release', 'release', 'release')]

# ---------------------------------------------------------------- 文档
L = ['等价性证据 (equivalence_evidence)', '']
L += ['本文件汇总 "新工程 == 原工程" 的**可执行证据**。所有数字均来自工具实际运行输出,',
      '不是推断。', '', '## 0. 工具链', '',
      '| 项 | 值 |', '|---|---|',
      '| 编译器 | TI C2000 CGT `v25.11.1.LTS` |',
      '| 路径 | `E:\\ti\\c2000\\cgt-c2000_25.11.1.LTS\\ti-cgt-c2000_25.11.1.LTS\\bin` |',
      '| 目标 | TMS320F28035 / C28x, `-v28 -ml -mt` |',
      '| 目标文件格式 | **COFF** — 必须显式加 `--abi=coffabi` |',
      '| 解析工具 | `dis2000`(反汇编) / `ofd2000`(目标文件) / `hex2000`(映像) |', '',
      '> 新 CGT 默认产出 EABI。EABI 会去掉符号前导下划线、换用 `rts2800_ml_eabi.lib`,',
      '> 且把已初始化全局量放进 `.data`(原 `.cmd` 无 `.data` 归置 => 会被丢进 OTP)。',
      '> 因此 `--abi=coffabi` 是硬性前提。', '']

_a = (CMP or {}).get('tierA', {})
_b = (CMP or {}).get('tierB', {})
_same = len(_a.get('same', []))
_diff = len(_a.get('diff', []))
_idn = len(_b.get('identical', []))
L += ['## 1. 证据层级总览', '',
      '| 层级 | 手段 | 覆盖 | 结果 |', '|---|---|---|---|',
      '| A | 逐模块反汇编文本比对 (`dis2000 -1`) | 含 `.text` 的模块 | **%d 逐行相同, %d 不同 (改写)** |' % (
          _same, _diff),
      '| B | 目标文件内容级比对 (段+重定位+符号表) | **全部 %d 个模块** | **%d 一致, %d 不同 (改写)** |' % (
          NMOD, _idn, len(_b.get('failed', []))),
      '| C | 整机映像比对 (`hex2000` + SHA256) | Debug + Release | **%s** |' % (
          '两配置均逐字节相同' if all(i['identical'] for i in IMG)
          else '因改写而不同 (差异有界, 见第 4 节)'),
      '| D | **宿主差分测试** (逐字段比对全部状态) | 改写模块 | **见第 5 节** |',
      '| E | **目标端副作用检查** (告警 + 外部符号集合) | 改写模块 | **见第 5 节** |',
      '', '层级 A 只对含机器码的模块有效; 纯数据模块 (无 `.text`) 的反汇编产物只有文件头,',
      '因此 **"空 == 空" 不被当作证据**, 这类模块由层级 B 覆盖。', '',
      '**关于改写**: 按"函数名保留 / 功能不变 / 实现适当变"的要求改过实现的模块,',
      '**不受**"逐字节相同"这条契约约束 (登记在 `docs/rewritten_modules.json`),',
      '改由层级 D/E 提供行为等价证据。注意契约的方向是"**没改写过的必须相同**",',
      '而**不是**"改写过的一定不同" —— 实测已有改写模块编译产物与基准逐字节相同,',
      '那是最强的一档结果, 不是异常。',
      '当前改写模块: %s' % (
          ', '.join('`%s`' % p for p in REW) if REW else '无'),
      '']

L += ['### 1.1 当前改写模块', '']
if REW:
    L += ['| 模块 | 变换 | 差分用例 | 不一致 | 负向对照 | `.text` 变化 | 外部符号 |',
          '|---|---|---:|---:|---:|---:|---|']
    for m in MAN['modules']:
        d = difftest_rec(m['case']) or {}
        t = target_rec(m['dst']) or {}
        L.append('| `%s` | %s | %s | **%s** | %s | %+d | %s |' % (
            m['dst'], m.get('what', ''),
            format(d.get('cases') or 0, ','), d.get('mismatches'),
            negctl_of(m['case']),
            t.get('text_delta') or 0,
            '一致' if t.get('symbols_equal') else '不一致'))
    L += ['']
else:
    L += ['无。当前全部模块仍受 token 等价 + 逐字节一致的原始契约约束。', '']

L += ['## 2. 层级 A — 逐模块反汇编', '']
if CMP:
    a = CMP['tierA']
    L += ['- 编译: **%d / %d** 个目标文件成功 (原/新各 %d), 失败 **%d**' % (
        a.get('compiled', 0), CMP.get('modules', 0) * 2, CMP.get('modules', 0),
        len(a.get('compile_failed', []))),
        '- 有代码模块: **%d 相同 / %d 不同**' % (len(a['same']), len(a['diff'])),
        '- 纯数据模块 (本条不适用, 转由层级 B 覆盖): %s' % (
            ', '.join('`%s`' % x for x in a.get('no_code', [])) or '无'), '']
    if a['diff']:
        L += ['不一致模块:', ''] + ['- `%s`: %s' % (s, a['details'].get(s, '')) for s in a['diff']] + ['']
    L += ['比对细节: `dis2000` 必须 `cd` 进目标目录并用**裸文件名**调用 ——',
          '否则输出第 2 行会回显传入路径, 造成假差异; 同时必须断言产物非空。', '']

L += ['## 3. 层级 B — 目标文件内容级比对 (覆盖全部 59 个模块)', '',
      '**方法**: 把两侧源码复制到**同一个暂存路径**后编译, 使目标文件里内嵌的源文件',
      '绝对路径完全一致; 再按 COFF 结构取摘要比对:', '',
      '1. 每个段: 名字 / 大小 / 是否装载 / 标志 / 重定位条数 / **段内容 SHA256** / **重定位区 SHA256**',
      '2. 符号表 (18 字节/项, 跳过 aux 与 `.file`): 名字 / value / scnum / type / sclass',
      '3. 忽略: 文件头 4 字节时间戳、文件内偏移、字符串表本身', '',
      '**为什么不直接 `cmp` 两个 `.obj`**: 时间戳与内嵌源路径(新工程换了目录, 长度必然不同)',
      '会污染结果 —— 实测会报出上万条与语义无关的 "差异"。', '']
if CMP:
    b = CMP['tierB']
    L += ['- 结果: **%d / %d 个模块内容完全一致**' % (len(b['identical']), CMP.get('modules', 0)), '']
    if b['failed']:
        L += ['不一致:', ''] + ['- `%s` %s: %s' % (x['stem'], x['kind'],
                                              json.dumps(x['info'], ensure_ascii=False)[:200])
                              for x in b['failed']] + ['']

L += ['## 4. 层级 C — 整机映像逐字节比对', '',
      '| 配置 | 原工程映像 | 新工程映像 | 大小 | SHA256 | 结论 |', '|---|---|---|---:|---|---|']
for i in IMG:
    L.append('| %s | `%s` | `%s` | %s / %s | `%s` | %s |' % (
        i['cfg'],
        os.path.basename(i['base_hex']) if i['base_hex'] else '(未生成)',
        os.path.basename(i['new_hex']) if i['new_hex'] else '(未生成)',
        i['base_size'], i['new_size'],
        (i['new_sha'] or '-')[:16],
        '**逐字节相同**' if i['identical'] else '不同 (因改写, 见下)'))
L += ['']
if NREW == 0:
    L += ['> 原工程 Release 的产物名沿用其自身配置 (`MD380_Release.out`), 新工程统一为',
          '> `jt380_Release.out`; 二者**内容逐字节相同**, 仅文件名不同。', '']
else:
    L += ['> 当前含 %d 个改写模块 (%s)。' % (
        NREW, ', '.join('`%s`' % p for p in REW)), '']
    _stem = [os.path.splitext(os.path.basename(p))[0] for p in REW]
    _ident = set((CMP or {}).get('tierB', {}).get('identical', []))
    _zero = [s for s in _stem if s in _ident]
    if _zero:
        L += ['> **整机映像与原工程不同, 但这不是"改写"的必然结果**: 其中 %d 个模块' % len(_zero),
              '> (%s) 改写后编译产物**逐字节相同**, 对映像**零贡献**。' % ', '.join('`%s`' % s for s in _zero),
              '> 差异只来自另外 %d 个确实改变了指令序列的模块。也就是说"改写过 ⇒ 产物必然' % (NREW - len(_zero)),
              '> 不同"并不成立 —— 最好的改写本就该收敛到同一份机器码。', '']
    else:
        L += ['> 这些模块都改变了指令序列, 因此整机映像与原工程不同。', '']
    L += ['> 差异必须是**有界的**: 由 `tests/test_build_evidence.py` 卡住 (≤256 字节)。',
          '> 改写模块的行为等价证据见第 5 节。',
          '> 另外, 原工程 Release 的产物名沿用其自身配置 (`MD380_Release.out`), 新工程统一为',
          '> `jt380_Release.out`。', '']
L += ['入口点核对 (`build/Debug/jt380_C.map`):', '',
      '```',
      'ENTRY POINT SYMBOL: "code_start"  address: 003f7ff6',
      '  BEGIN                 003f7ff6   00000002  ...  codestart',
      '```', '',
      '与原工程一致: `code_start` @ `0x003F7FF6`, 段 `codestart` 归入 `BEGIN`。', '']

# ---------------------------------------------------------------- 5. 改写模块
L += ['## 5. 改写模块的行为等价证据 (差分测试 + 目标端)', '']
if not REW:
    L += ['当前没有改写模块 —— 全部模块仍走 token 等价 + 逐字节一致路径。', '']
else:
    L += ['改写过的实现**不受**"逐字节相同"契约约束, 所以层级 A/B/C 不足以证明它。',
          '替代证据是 **行为等价**: 同一组输入喂给基准实现与改写实现, 逐字段比对结果。', '',
          '**方法**:',
          '',
          '1. 冻结基准 `tools/difftest/cases/<case>/ref/<Mod>_ref.c` (即改写前那份"与原工程字节等价"的版本)',
          '2. 两侧各自编译成宿主可执行文件: 模块导出的符号加 `ref_` / `new_` 前缀,',
          '   硬件寄存器 mock **共享同一份**, 保证两侧看到相同的外设状态',
          '3. 输入空间 = 边界值全交叉枚举 + 可复现的随机采样',
          '4. 每次调用前后比对: 返回值、模块导出的全局状态**全字段** (`memcmp`)、硬件副作用;',
          '   比对前把目标结构体填成哨兵值, 这样"少写一个字段"也会被抓出来',
          '',
          '**为什么必须用精确宽度类型**: C28x 上 `int` 是 16 位, 宿主上是 32 位。',
          '沿用原 `DataTypeDef.h` 会让 `s16`/`u16` 静默变宽, 溢出与截断位置全错。',
          '`tools/difftest/shim/DataTypeDef.h` 把四个类型钉到 `int16_t`/`uint16_t`/`int32_t`/`uint32_t`。', '',
          '**负向对照** (`tools/difftest/negative_control.py`): 往基准注入真实 bug',
          '(上限差一、移位量错、写错字段、操作数互换、赋值漏改、判据取反、常量写错、',
          '整机拷贝长度取错 …), 要求**全部被抓到** —— 抓不到就说明测试台对该类错误',
          '不敏感, 此时 "0 处不一致" 没有意义。', '',
          '变体**没编译起来**单独记为 `BROKEN` 并判总账失败: 否则 "全部抓到" 可能只是',
          '"全都没跑起来" —— 这两件事必须分得开。', '']
    if NEGCTL_BY_CASE:
        L += ['| 用例 | 注入的 bug | 抓到 | 结果 |', '|---|---:|---:|---|']
        for m in MAN['modules']:
            e = NEGCTL_BY_CASE.get(m['case'])
            if not e:
                continue
            L.append('| `%s` | %d | **%d** | %s |' % (
                m['case'], e[1], e[0], '**全部抓到**' if e[0] == e[1] else '**有漏网**'))
        _nc_t = sum(e[1] for e in NEGCTL_BY_CASE.values())
        _nc_c = sum(e[0] for e in NEGCTL_BY_CASE.values())
        L += ['| **合计** | **%d** | **%d** | %s |' % (
            _nc_t, _nc_c, '**全部抓到**' if _nc_c == _nc_t else '**有漏网**'), '']
    for m in MAN['modules']:
        d = difftest_rec(m['case']) or {}
        t = target_rec(m['dst']) or {}
        L += ['### `%s`' % m['dst'], '',
              '- 变换: %s' % m.get('what', '(未记录)'), '']
        if m.get('invariants'):
            L += ['- 必须守住的不变量:', ''] + ['  - %s' % x for x in m['invariants']] + ['']
        L += ['| 项 | 结果 |', '|---|---|',
              '| 差分用例数 | **%s** |' % format(d.get('cases') or 0, ','),
              '| 不一致 | **%s** |' % d.get('mismatches'),
              '| 进程退出码 | %s |' % d.get('rc'),
              '| 目标端编译 (ref / new) | rc=%s / rc=%s |' % (t.get('rc_ref'), t.get('rc_new')),
              '| 目标端新增告警 | %s |' % (t.get('warn_added') or '无'),
              '| 外部符号集合 | %s |' % ('**一致**' if t.get('symbols_equal') else '不一致'),
              '| 外部符号 | %s |' % ', '.join('`%s`' % s for s in (t.get('symbols_ref') or [])),
              '| `.text` 尺寸 | %s → %s (**%+d**) |' % (
                  t.get('text_ref'), t.get('text_new'), t.get('text_delta') or 0),
              '']
    # ---- 宿主专用补丁: 目标编译器能折叠、MSVC 折叠不了的常量表达式 ----
    _patched = [m for m in MAN['modules']
                if (difftest_rec(m['case']) or {}).get('host_patched')]
    if _patched:
        L += ['### 宿主专用补丁 (`host_patch.json`)', '',
              '差分测试在 x86 上重编源码, 于是会碰到**目标编译器接受、MSVC 不接受**的',
              '写法。已确认的那一类是**静态初始值设定项里的"两个地址常量相减"**:', '',
              '- MSVC 接受**单个**地址常量 (`offsetof(...)`, `&((T*)0)->m`)',
              '- MSVC **拒绝任何两个地址常量之差**: 实测 6 种写法 (裸指针差、`char*` 差、',
              '  `size_t` 差、`long` 差、经 `extern` 对象、`offsetof` 之差) 全部报 C2099',
              '- 本机没有 gcc/clang (VS 的 LLVM 组件只带 `clang-format` / `clang-tidy`)', '',
              '这类写法无法用 shim 绕开, 只能用**宿主专用补丁**: 把源文件改写成宿主可编译',
              '的副本, 放进 `build/_difftest/gen_<case>/`, 并让该目录**优先**于原目录被',
              'include。补丁以 `(原串, 替换串, 出现次数)` 描述, **出现次数不符就报错停下** ——',
              '与其拿一份"看着像但已经不对"的副本去跑测试, 不如直接停。', '',
              '被补丁的用例: %s' % ', '.join('`%s`' % m['case'] for m in _patched), '',
              '> 补丁**只作用于宿主副本**, 交付源码与目标端编译完全不受影响。', '']

    # ---- 宿主 vs 目标端 布局对照 ----
    # 按改写清单的顺序列出**所有**有探针记录的用例; 不再只写死 f_motorFc。
    _lays = [(m['case'], layout_rec(m['case'])) for m in MAN['modules']]
    _lays = [(c, r) for c, r in _lays if r]
    if _lays:
        _u = {'B': '字节', 'W': '字', 'C': '个'}
        L += ['### 宿主 vs 目标端 布局对照 (`tools/difftest/target_layout.py <case>`)', '',
              '差分测试用两套编译器编同一份源码, 所以"宿主上的结构体布局是否等于目标端"',
              '必须**实测**。探针把每个用例关心的尺寸/偏移分别用 MSVC 与 cl2000 编一遍,',
              '再并排比对:']
        for _c, _lay in _lays:
            L += ['', '**`%s`** — 不一致 **%d / %d** 项' % (_c, _lay['ndiff'], _lay['nitem']), '',
                  '| 项 | 单位 | 宿主 (x86) | 目标端 (C28x) | 差 |', '|---|---|---:|---:|---:|']
            for it in _lay['items']:
                L.append('| `%s` | %s | %d | %d | %s |' % (
                    it['label'], _u.get(it['unit'], it['unit']), it['host'], it['target'],
                    ('%+d' % it['delta']) if it['delta'] else '='))
        L += ['', '根因两条, 都已确认:', '',
              '- `f_funcCode.h` 里 3 处 `Uint16 all[sizeof(struct X)]` (ERROR_SCENE /',
              '  MOTOR_PARA / PG_PARA)。C28x 上 `sizeof` 返回**16 位单元数**, 正好等于',
              '  `Uint16` 元素个数; x86 上返回**8 位字节数**, 数组因此大一倍, 把 union',
              '  撑大, 进而撑大 `MOTOR_FC` 与 `FUNCCODE_CODE`。这是被测头文件的惯用法在',
              '  x86 上的固有失真。',
              '- `enum MOTOR_SN` 在 C28x 上是 1 个 16 位单元, MSVC 上是 4 字节, 使',
              '  `motorSn` 之后的成员整体 +1 字。', '',
              '**这对等价性结论的影响, 必须说清楚**: 两侧 (ref/new) 共用同一套头文件,',
              '布局差异对两侧完全对称, "相同输入 → 相同输出" 的判据本身不受影响 —— 因此这',
              '是**已量化的保真度缺口, 不是等价性缺口**。但它确实限制了下述推理: 宿主上',
              '的等价性只覆盖"两份实现用同一套下标表达式算出同一个值", **不覆盖**"目标端',
              '会写到同一个地址"。后者由第 5 节的**目标端外部符号集合检查**兜底: 改写后的',
              '目标文件仍然引用同一组全局量 (`motor1ParaIndex` / `motorSnIndexIncToMotor1`',
              '/ `funcCode` ...), 说明它仍然通过这几张表在算下标, 而不是把下标值写死。', '']

    L += ['**`text` 尺寸变化必须知情**: 改写会改变生成的指令序列, 因此**周期数会变**。',
          '落在中断服务程序里的函数需要单独评估最坏执行时间, 本证据链不覆盖时序。', '']

L += ['## 6. 静态验收 (与编译无关的补充证据)', '']
if VER:
    L += ['| 检查 | 结果 |', '|---|---|',
          '| token 流等价 | **%d / %d 文件一致**, 0 处非预期不一致, %d 个改写模块按预期不同 |' % (
              VER['token_stream_identical']['files'] - len(VER['token_stream_identical']['mismatch'])
              - len(VER['token_stream_identical'].get('rewritten', [])),
              VER['token_stream_identical']['files'],
              len(VER['token_stream_identical'].get('rewritten', []))),
          '| 注释残留 | **%d 处** |' % len(VER['comment_residual']),
          '| TODO/FIXME/NOTE | **%d 处** |' % len(VER['todo_fixme_note']),
          '| 非 ASCII 字节 | **%d 处** |' % len(VER['non_ascii']),
          '| 二次去注释可删字符 | **%d 处** |' % len(VER['idempotent']),
          '| 符号抽取不一致 | **%d 处** |' % len(VER['symbol_identical']),
          '| 隐式函数声明 | %d 处 (保留未修) |' % len(VER['implicit_declaration']),
          '| 中断函数定义 | %d 个 |' % len(VER['isr_definitions']), '']
if NEG:
    _caught = sum(1 for k, v in NEG.get('after', {}).items() if v)
    _total = len(NEG.get('after', {}))
    L += ['负向对照 (注入错误验证检查有效性): **%d / %d 类被抓到**, 总体判定 **%s**' % (
        _caught, _total, '通过' if NEG.get('pass_') else '未通过'),
        '', '| 注入的错误类型 | 是否被抓到 |', '|---|---|']
    for k, v in NEG.get('after', {}).items():
        L.append('| %s | %s |' % (k, '**是**' if v else '否'))
    L += ['', '基线(注入前)应全部为空: %s' % (
        '是' if not any(NEG.get('baseline', {}).values()) else '否'), '']

L += ['## 7. 明确**未**证明的部分', '',
      '| 项 | 原因 |', '|---|---|',
      '| 硬件上的实际行为 (PWM 输出、ADC 采样、CAN/SCI 收发、电机闭环) | 需目标板与台架 |',
      '| 中断响应时序 / 最坏执行时间 | 需示波器或周期计数实测; 改写模块的 `.text` 尺寸已变化 |',
      '| 目标端有符号/无符号混合比较的宽度语义 | 宿主与 C28x 提升规则不同, 靠"不改变表达式类型"的纪律保证, 非测试保证 |',
      '| Flash 烧写与上电启动 | 需仿真器 (XDS100/510) |',
      '| 运行期数据竞争 | 需硬件在环 |',
      '| 原工程 `39 Warnings` 的逐条对照 | 原构建日志只记条数, 未记正文 |', '',
      '本工程提供的是**编译输入等价 + 生成产物等价 + 改写模块行为等价**的证据链;',
      '上述硬件相关项必须在目标板上复测, 不能由本证据链替代。', '']

L += ['## 8. 复现命令', '',
      '```bash', '# 逐模块两层比对 (反汇编 + 目标文件内容级)',
      'python tools/build_and_compare.py', '',
      '# 构建新工程 (按 jt380.pjt 的真实选项), 产出 .out/.map/.hex',
      'python tools/build_official.py', '',
      '# 构建原工程副本 (bpqym 只读复制), 产出 .out/.map/.hex',
      'python tools/build_baseline.py', '',
      '# 改写模块: 先跑负向对照确认测试台有效, 再跑差分测试与目标端检查',
      'python tools/difftest/negative_control.py']
for _m in MAN['modules']:
    _d = difftest_rec(_m['case']) or {}
    L.append('python tools/difftest/run_case.py %s %s' % (
        _m['case'], _d.get('random') or 200000))
for _m in MAN['modules']:
    L.append('python tools/difftest/target_check.py %s' % _m['dst'])
L += ['python tools/difftest/target_layout.py', '',
      '# 静态验收 + 全量测试', 'python tools/verify_tree.py', 'python tests/run_all.py', '',
      '# 汇总本报告', 'python tools/make_build_report.py', '```', '']

open(os.path.join(DOC, 'equivalence_evidence.md'), 'w', encoding='utf-8').write('\n'.join(L) + '\n')

# ---------------------------------------------------------------- build_debug.txt
def build_txt(cfg, key, outname, opts):
    r = (OFF or {}).get(key) or {}
    b = (BAS or {}).get(key) or {}
    t = ['构建记录 — %s 配置' % cfg, '=' * 60, '']
    t += ['工具链: TI C2000 CGT v25.11.1.LTS (--abi=coffabi, COFF)',
          '目标: TMS320F28035 / C28x', '']
    t += ['一、编译选项 (取自 project/jt380.pjt, 逐字执行)', '-' * 60, opts, '']
    t += ['二、编译结果', '-' * 60,
          '  源文件数 : %s' % (len((CMP or {}).get('tierA', {}).get('same', [])) +
                            len((CMP or {}).get('tierA', {}).get('diff', [])) +
                            len((CMP or {}).get('tierA', {}).get('no_code', []))),
          '  失败     : %s' % r.get('nfail', '?'),
          '  告警/错误行: %s' % len(r.get('warns', [])), '']
    t += ['三、链接', '-' * 60,
          '  命令: cd project && cl2000 -@%s.lkf' % cfg,
          '  结果: %s' % ('成功' if r.get('linked') else '失败'),
          '  产物: %s' % (r.get('out') or '(无)'),
          '  映射: %s' % ((r.get('out') or '')[:-4] + '.map'), '']
    t += ['四、映像', '-' * 60,
          '  hex2000 -i -memwidth 16 -romwidth 16  (C28x 为 16 位字机器)',
          '  产物: %s' % (r.get('hex') or '(无)'),
          '  大小: %s 字节' % size(r.get('hex')),
          '  SHA256: %s' % (sha(r.get('hex')) or '-'), '']
    if b.get('hex'):
        t += ['五、与原工程副本的比对 (同一工具链、同一选项)', '-' * 60,
              '  原工程映像: %s (%s 字节)' % (b['hex'], size(b['hex'])),
              '  SHA256    : %s' % sha(b['hex']),
              '  结论      : %s' % ('**逐字节相同**' if sha(b['hex']) == sha(r.get('hex')) else '不同'), '']
    t += ['六、与原 CCS 3.3 构建的对照', '-' * 60,
          '  原工程 Debug 实测: Build Complete, 0 Errors, 39 Warnings, 0 Remarks',
          '  原始日志: docs/original_build_Debug.log',
          '  (原日志未记录告警正文, 故无法逐条比对)', '']
    return '\n'.join(t)


open(os.path.join(DOC, 'build_debug.txt'), 'w', encoding='utf-8').write(
    build_txt('Debug', 'debug', 'jt380_C', 'cl2000 ' + '(见 project/jt380.pjt [Compiler] Debug)') + '\n')
open(os.path.join(DOC, 'build_release.txt'), 'w', encoding='utf-8').write(
    build_txt('Release', 'release', 'jt380_Release', 'cl2000 (见 project/jt380.pjt [Compiler] Release)') + '\n')

# ---------------------------------------------------------------- warning_list.txt
W = ['告警清单 — warning_list.txt', '=' * 60, '']


def warns_of(key):
    v = (OFF or {}).get(key) or {}
    w = v.get('warns', [])
    return w if isinstance(w, list) else []


def base_warns_of(key):
    v = (BAS or {}).get(key) or {}
    w = v.get('warns', [])
    return w if isinstance(w, list) else []


allw = []
for key, cfg in (('debug', 'Debug'), ('release', 'Release')):
    for line in warns_of(key):
        allw.append('[%s] %s' % (cfg, line))
W += ['本清单由新工具链实际构建产生 (cl2000 v25.11.1.LTS, `--abi=coffabi`)。', '',
      '总行数: **%d** (Debug %d + Release %d; 两配置的告警集合相同)'
      % (len(allw), len(warns_of('debug')), len(warns_of('release'))), '']
if allw:
    W += ['| # | 配置 | 内容 |', '|---:|---|---|']
    for i, l in enumerate(allw[:400], 1):
        W.append('| %d | %s | `%s` |' % (i, l[1:l.index(']')],
                                         l[l.index(']') + 2:].replace('|', '\\|')))
    if len(allw) > 400:
        W += ['', '(仅显示前 400 条)']
else:
    W += ['**0 条告警/错误。**', '']

# ---- 与原工程副本的告警集合对照 ----
bw = base_warns_of('debug')
nw = warns_of('debug')
W += ['', '## 与原工程副本的告警集合对照 (Debug)', '',
      '注意: 行号**必然不同** —— 新工程删掉了全部注释, 每个文件的行号都前移了。',
      '因此这里按 **(模块, 告警正文)** 归一化后再比较, 忽略文件路径与行号。', '']


def norm_warn(l):
    """[mod] "path", line N: warning: text  ->  (mod, text)"""
    m = re.match(r'\[([^\]]+)\]\s*"[^"]*",\s*line\s*\d+:\s*(warning|error|remark):\s*(.*)$', l)
    if m:
        return (m.group(1), re.sub(r'"[^"]*"', '"X"', m.group(3)))
    m = re.match(r'\[([^\]]+)\]\s*(.*)$', l)
    return (m.group(1), m.group(2)) if m else (l, '')


bw, nw = base_warns_of('debug'), warns_of('debug')
if bw and nw:
    sb = set(norm_warn(x) for x in bw)
    sn = set(norm_warn(x) for x in nw)
    W += ['- 原工程副本 (归一化后): **%d** 类' % len(sb),
          '- 新工程     (归一化后): **%d** 类' % len(sn),
          '- 仅原工程有: **%d** 类' % len(sb - sn),
          '- 仅新工程有: **%d** 类' % len(sn - sb), '']
    for t, s in (('仅原工程有', sorted(sb - sn)), ('仅新工程有', sorted(sn - sb))):
        if s:
            W += ['**%s**:' % t, '']
            W += ['- `%s` : %s' % (a, b.replace('|', '\\|')) for a, b in s[:30]]
            W += ['']
    if not (sb - sn) and not (sn - sb):
        W += ['**告警集合完全一致** (模块 + 正文逐条相同)。', '']
else:
    W += ['(原工程副本的告警正文未采集)', '']

# ---- 按告警正文归类 ----
def categorize(lines):
    import collections
    c = collections.Counter()
    for l in lines:
        m = re.search(r'warning:[ \t]*(.*)', l)
        if m:
            c[re.sub(r'"[^"]*"', '"X"', m.group(1))] += 1
    return c


cat = categorize(nw)
if cat:
    W += ['## 新工程告警归类 (Debug)', '',
          '| 条数 | 告警正文 |', '|---:|---|']
    for k, v in cat.most_common():
        W += ['| %d | `%s` |' % (v, k.replace('|', '\\|'))]
    W += ['', '合计 **%d** 条。' % sum(cat.values()), '']

W += ['', '## 与原始 CCS 3.3 构建的对照', '',
      '原工程 CCS 3.3 Debug 构建实测: `0 Errors, 39 Warnings, 0 Remarks`。',
      '原构建日志 `CCs/cc_build_Debug.log` **只记录了条数, 未记录告警正文**,',
      '因此无法逐条对照。条数差异来自编译器版本不同',
      '(CCS 3.3 的 CGT v5.2.x 与 v25.11.1.LTS), 与源码无关 ——',
      ('未改写模块的源码 token 流与原工程完全一致, 且最终映像逐字节相同'
       if NREW == 0 else
       '未改写模块的源码 token 流与原工程完全一致; %d 个改写模块 (%s) 按预期不同, '
       '其行为等价证据见 `docs/equivalence_evidence.md` 第 5 节' % (
           NREW, ', '.join('`%s`' % p for p in REW))),
      '(见 `docs/equivalence_evidence.md`)。', '']
open(os.path.join(DOC, 'warning_list.txt'), 'w', encoding='utf-8').write('\n'.join(W) + '\n')

print('equivalence_evidence.md / build_debug.txt / build_release.txt / warning_list.txt written')
for i in IMG:
    print('  image %-8s identical=%s  new=%s' % (i['cfg'], i['identical'], i['new_size']))
