import os, re, sys, hashlib, json, collections

SRC = r"E:/software/bpqdm/bpqym"
OUT = r"E:/software/bpqdm/bpqdm_rewrite/docs"
os.makedirs(OUT, exist_ok=True)

CODE_EXT = {'.c', '.h', '.asm', '.cmd', '.pjt', '.lkf', '.gel', '.m', '.txt'}


def read_bytes(p):
    with open(p, 'rb') as f:
        return f.read()


def detect_encoding(raw):
    for enc in ('ascii', 'utf-8', 'gbk', 'gb18030'):
        try:
            raw.decode(enc)
            return enc
        except UnicodeDecodeError:
            pass
    try:
        raw.decode('utf-8', 'surrogateescape')
        return 'mixed(utf-8+surrogateescape)'
    except Exception:
        return 'unknown'


def eol_of(raw):
    crlf = raw.count(b'\r\n')
    lf = raw.count(b'\n') - crlf
    if crlf and lf:
        return 'mixed'
    if crlf:
        return 'CRLF'
    return 'LF'


def collect():
    rows = []
    for root, dirs, files in os.walk(SRC):
        for fn in files:
            p = os.path.join(root, fn)
            rel = os.path.relpath(p, SRC).replace('\\', '/')
            ext = os.path.splitext(fn)[1].lower()
            raw = read_bytes(p)
            rows.append(dict(
                rel=rel,
                name=fn,
                dir=os.path.dirname(rel).replace('\\', '/'),
                ext=ext,
                size=len(raw),
                md5=hashlib.md5(raw).hexdigest(),
                lines=raw.count(b'\n') + (0 if raw.endswith(b'\n') else 1),
                enc=detect_encoding(raw) if ext in CODE_EXT else '-',
                eol=eol_of(raw) if ext in CODE_EXT else '-',
            ))
    return rows


# ---- build list from Debug.lkf -------------------------------------------
def parse_lkf():
    txt = open(os.path.join(SRC, 'CCs', 'Debug.lkf'), encoding='gbk', errors='replace').read()
    objs, cmds, cmdline = [], [], None
    for line in txt.splitlines():
        line = line.strip()
        if line.startswith('-'):
            cmdline = line
            continue
        m = re.match(r'^"(.+?)"\s*$', line)
        if not m:
            continue
        v = m.group(1)
        if v.lower().endswith('.obj'):
            objs.append(v)
        elif v.lower().endswith('.cmd'):
            cmds.append(v)
    built = [os.path.basename(o)[:-4] for o in objs]
    return cmdline, cmds, built, objs


def parse_pjt():
    txt = open(os.path.join(SRC, 'CCs', 'md380.pjt'), encoding='gbk', errors='replace').read()
    srcs = re.findall(r'^Source="(.+?)"', txt, re.M)
    opts = re.findall(r'^\["(.+?)" Settings: "(Debug|Release)"\]\nOptions=(.*)$', txt, re.M)
    excl = re.findall(r'^\["(.+?)" Settings: "(Debug|Release)"\]\nExcludeFromBuild=true', txt, re.M)
    order = re.findall(r'^\["(.+?)" Settings: "(Debug|Release)"\]\nLinkOrder=(\d+)', txt, re.M)
    return srcs, opts, excl, order


def main():
    rows = collect()
    cmdline, cmds, built, objs = parse_lkf()
    srcs, opts, excl, order = parse_pjt()

    built_set = set(built)
    code_rows = [r for r in rows if r['ext'] in ('.c', '.h', '.asm')]

    # ---- inventory ------------------------------------------------------
    L = []
    L.append('# 文件清单 (file_inventory)')
    L.append('')
    L.append('来源工程: `bpqym`  (TMS320F28035 / C28x, CCS 3.3, COFF)')
    L.append('')
    L.append('统计: 源码/头文件/汇编共 %d 个; .c %d, .h %d, .asm %d' % (
        len(code_rows),
        sum(1 for r in code_rows if r['ext'] == '.c'),
        sum(1 for r in code_rows if r['ext'] == '.h'),
        sum(1 for r in code_rows if r['ext'] == '.asm')))
    L.append('')
    L.append('| 文件 | 目录 | 行数 | 字节 | 编码 | 行尾 | 参与构建 |')
    L.append('|---|---|---:|---:|---|---|---|')
    for r in sorted(code_rows, key=lambda x: (x['dir'], x['name'])):
        stem = os.path.splitext(r['name'])[0]
        inb = '是' if stem in built_set else '否'
        L.append('| %s | %s | %d | %d | %s | %s | %s |' % (
            r['name'], r['dir'], r['lines'], r['size'], r['enc'], r['eol'], inb))
    open(os.path.join(OUT, 'file_inventory.md'), 'w', encoding='utf-8').write('\n'.join(L) + '\n')

    # ---- build configuration -------------------------------------------
    B = []
    B.append('# 构建配置 (build_configuration)')
    B.append('')
    B.append('## CCS 工程文件')
    B.append('')
    B.append('- 工程: `CCs/md380.pjt` (Code Composer Project File Version 2.0)')
    B.append('- CPU 家族: `TMS320C28XX`; 工程类型: `Executable`')
    B.append('- 配置: `Debug`, `Release`')
    B.append('- 输出格式: COFF (CCS 3.3 时代工具链)')
    B.append('')
    B.append('## 编译器选项')
    B.append('')
    for who, cfg, o in opts:
        B.append('- **%s** `%s`: `%s`' % (cfg, who, o))
    B.append('')
    B.append('## 链接器选项')
    B.append('')
    L2 = open(os.path.join(SRC, 'CCs', 'Debug.lkf'), encoding='gbk', errors='replace').read().splitlines()
    B.append('- **Debug** (`CCs/Debug.lkf` 第 1 行):')
    B.append('  `%s`' % cmdline)
    B.append('')
    B.append('- **Release** (`md380.pjt` Linker Settings):')
    for who, cfg, o in opts:
        if cfg == 'Release' and who == 'Linker':
            B.append('  `%s`' % o)
    B.append('')
    B.append('## 头文件搜索路径')
    B.append('')
    B.append('1. `$(Proj_dir)\\..\\public\\28035_header`  (DSP 设备头文件)')
    B.append('2. `$(Proj_dir)\\..\\public\\motor_header`  (电机层头文件)')
    B.append('3. `$(Proj_dir)\\..\\public\\function_header` (功能层头文件)')
    B.append('')
    B.append('## 预定义宏')
    B.append('')
    B.append('- Debug: `_DEBUG`, `DSP2803X`, `LARGE_MODEL`')
    B.append('- Release: `DSP2803X`, `LARGE_MODEL`')
    B.append('')
    B.append('## 语言 / 模型选项')
    B.append('')
    B.append('- `-v28` 目标 C28x (Debug/Release)')
    B.append('- `-ml` 大内存模型 (Debug/Release)')
    B.append('- `-mt` 仅 Debug 出现 (老工具链选项)')
    B.append('- `-pdsw225` 仅 Release 出现 (抑制诊断 225)')
    B.append('')
    B.append('## 链接输入清单 (权威, 共 %d 个 .obj)' % len(built))
    B.append('')
    for i, b in enumerate(built, 1):
        B.append('%d. `%s.obj`' % (i, b))
    B.append('')
    B.append('## 参与链接的 .cmd')
    B.append('')
    for c in cmds:
        B.append('- `%s`' % c)
    B.append('')
    B.append('## 已知工程配置异常 (原样记录, 未修改)')
    B.append('')
    B.append('- Debug 编译选项含 `-o2`, Release 含 `-o3`。这两个**是优化级别**:')
    B.append('  在 `cl2000` 里 `-o` 是 `-O` / `--opt_level` 的别名 (实测 `-oN` 与 `-ON`')
    B.append('  目标文件 canonical 摘要完全相同; `-o=<路径>` 报 `bad argument to option -O`)。')
    B.append('  因此原工程 Debug 按 O2、Release 按 O3 优化, 优化**是生效的**。')
    B.append('  (早期文档误判为"`-o` 是输出文件名, 故未开启优化", 见 `docs/rewrite_decisions.md` D5。)')
    B.append('- `-mt` 只出现在 Debug, `-pdsw225` 只出现在 Release, 两者配置不对称。')
    B.append('- Release 链接器 `-o".\\Release\\MD380_Release.out"`, 而目录中产物名为')
    B.append('  `MD380_Release_20130110_Chint.out`, 说明 Release 产物曾被改名归档。')
    open(os.path.join(OUT, 'build_configuration.md'), 'w', encoding='utf-8').write('\n'.join(B) + '\n')

    # ---- duplicates -----------------------------------------------------
    by_name = collections.defaultdict(list)
    by_md5 = collections.defaultdict(list)
    for r in code_rows:
        by_name[r['name']].append(r)
    for r in rows:
        by_md5[r['md5']].append(r)
    D = []
    D.append('# 重复文件清单 (duplicate_file_list)')
    D.append('')
    D.append('## 1. 同名文件出现在多个目录')
    D.append('')
    D.append('| 文件名 | 出现位置 | 内容是否完全相同 |')
    D.append('|---|---|---|')
    for name, rs in sorted(by_name.items()):
        if len(rs) < 2:
            continue
        same = len(set(r['md5'] for r in rs)) == 1
        D.append('| `%s` | %s | %s |' % (
            name, '<br>'.join(r['dir'] for r in rs), '是' if same else '**否**'))
    D.append('')
    D.append('## 2. 内容完全相同的文件组 (任意文件名)')
    D.append('')
    groups = [v for v in by_md5.values() if len(v) > 1]
    groups.sort(key=lambda v: -len(v))
    D.append('| # | 文件组 | 副本数 |')
    D.append('|---:|---|---:|')
    for i, g in enumerate(groups, 1):
        D.append('| %d | %s | %d |' % (i, '<br>'.join(r['rel'] for r in g), len(g)))
    D.append('')
    D.append('## 3. 目录内但不在链接清单中的源码 (孤儿文件)')
    D.append('')
    D.append('| 文件 | 说明 |')
    D.append('|---|---|')
    allc = [r for r in code_rows if r['ext'] in ('.c', '.asm')]
    orphan = [r for r in allc if os.path.splitext(r['name'])[0] not in built_set]
    for r in sorted(orphan, key=lambda x: x['rel']):
        D.append('| `%s` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |' % r['rel'])
    D.append('')
    open(os.path.join(OUT, 'duplicate_file_list.md'), 'w', encoding='utf-8').write('\n'.join(D) + '\n')

    json.dump(dict(rows=rows, built=built, cmds=cmds, cmdline=cmdline,
                   srcs=srcs, opts=opts, excl=excl, order=order),
              open(os.path.join(OUT, '_audit.json'), 'w', encoding='utf-8'),
              ensure_ascii=False, indent=1)
    print('built objects :', len(built))
    print('source files  :', len(code_rows))
    print('orphans       :', len(orphan))
    print('dup groups    :', len(groups))


main()
