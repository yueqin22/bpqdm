"""在临时副本里构建 "原工程" (bpqym), 产出与工程配置一致的 .out / .hex。

原工程 CCs/Debug.lkf 里写的是原开发机上的绝对路径
(E:\\XDS\\md380_C_org11\\...), 在本机不可用; 且原目录严禁修改。
因此把 bpqym 整树复制到 build/_verify/baseline_tree 再就地构建,
用 md380.pjt 里的选项字符串 (展开 $(Proj_dir)) 驱动编译与链接,
并另生成一份指向副本的 .lkf。

产物: build/_verify/baseline/{Debug,Release}/md380*.out|.hex|.map
"""
import os, re, sys, json, shutil, subprocess
from concurrent.futures import ThreadPoolExecutor

CGT = 'E:/ti/c2000/cgt-c2000_25.11.1.LTS/ti-cgt-c2000_25.11.1.LTS/bin'
SRC = 'E:/software/bpqdm/bpqym'
DST = 'E:/software/bpqdm/bpqdm_rewrite'
DOC = os.path.join(DST, 'docs')
WORK = os.path.join(DST, 'build/_verify')
BASE = os.path.join(WORK, 'baseline_tree')
OUT = os.path.join(WORK, 'baseline')

P = json.load(open(os.path.join(DOC, '_project.json'), encoding='utf-8'))
MAP = json.load(open(os.path.join(DOC, '_map.json'), encoding='utf-8'))
BUILT = [m for m in MAP if m['kind'] in ('c', 'asm')
         and not m['dst'].startswith('src/orphan/')]

PROJ_DIR = os.path.join(BASE, 'CCs').replace('/', '\\')
ORIG_PJT = os.path.join(SRC, 'CCs', 'md380.pjt')


def pjt_options(config):
    """从原工程 md380.pjt 取该配置的编译选项原文 (原样, 不借用新工程的模板)。"""
    txt = open(ORIG_PJT, encoding='gbk', errors='replace').read()
    m = re.search(r'\["Compiler" Settings: "%s"\]\s*\r?\nOptions=([^\r\n]*)' % config, txt)
    if not m:
        raise SystemExit('未能在 %s 中找到 Compiler/%s 选项' % (ORIG_PJT, config))
    return m.group(1)


def sh(cmd, cwd=None):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, errors='replace')
    return r.returncode, (r.stdout or '') + (r.stderr or '')


def stage_tree():
    # 不做整树删除 (批量删除会触发安全确认); 直接覆盖式复制即可 ——
    # bpqym 本身只读不变, 副本里多出来的只有上一轮的 .obj/.out/.map。
    shutil.copytree(SRC, BASE, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns('*.obj', '*.out', '*.map'))


def expand(s):
    return s.replace('$(Proj_dir)', PROJ_DIR)


def build(cfg, opts_key, lkf_name, outname):
    outdir = os.path.join(BASE, cfg)
    os.makedirs(outdir, exist_ok=True)
    opts = expand(pjt_options(cfg)).replace('"', '').split()
    if '--abi=coffabi' not in opts:
        opts.insert(0, '--abi=coffabi')
    opts = [o for o in opts if not o.startswith('-fr')]
    opts.append('-fr' + outdir.replace('/', '\\'))

    def one(m):
        rel = '..\\' + m['src'].replace('/', '\\')
        rc, out = sh([os.path.join(CGT, 'cl2000.exe')] + opts + ['-c', rel], cwd=PROJ_DIR)
        stem = os.path.splitext(os.path.basename(m['src']))[0]
        return stem, rc, out, os.path.isfile(os.path.join(outdir, stem + '.obj'))

    with ThreadPoolExecutor(max_workers=6) as ex:
        res = list(ex.map(one, BUILT))
    nfail = sum(1 for _, rc, _, has in res if rc or not has)
    warn_lines = ['[%s] %s' % (stem, l.strip())
                  for stem, _, o, _ in res for l in o.split('\n')
                  if 'warning' in l.lower() or 'error' in l.lower() or 'remark' in l.lower()]
    print('[%s] 编译 %d 个, 失败 %d, 警告/错误行 %d' % (cfg, len(res), nfail, len(warn_lines)))
    for stem, rc, o, has in res:
        if rc or not has:
            print('   FAIL %s rc=%d %s' % (stem, rc, o.strip()[:200]))

    # ---- 生成指向副本的 .lkf ----
    cmd_files = [os.path.join(BASE, 'public/common/DSP28035Peripheral.cmd'),
                 os.path.join(BASE, 'public/common/F28035.cmd')]
    objs = [os.path.join(outdir, os.path.splitext(os.path.basename(m['src']))[0] + '.obj')
            for m in BUILT]
    mapf = os.path.join(outdir, outname + '.map')
    outf = os.path.join(outdir, outname + '.out')
    libdir = os.path.join(BASE, 'public/common')
    first = ('-z -q -c -ecode_start -m"%s" -o"%s" -stack0x400 -w -x '
             '-i"%s" -l"rts2800_ml.lib"' % (mapf.replace('/', '\\'),
                                            outf.replace('/', '\\'),
                                            libdir.replace('/', '\\')))
    lkf = os.path.join(BASE, 'CCs', lkf_name)
    with open(lkf, 'w', encoding='ascii') as f:
        f.write(first + '\n')
        for p in cmd_files + objs:
            f.write('"%s"\n' % p.replace('/', '\\'))

    rc, out = sh([os.path.join(CGT, 'cl2000.exe'), '-@' + lkf])
    print('[%s] 链接 rc=%d' % (cfg, rc))
    if out.strip():
        print('   ' + out.strip()[:800].replace('\n', '\n   '))
    if not os.path.isfile(outf):
        return dict(cfg=cfg, nfail=nfail, linked=False, warns=warn_lines, out=None, hex=None)

    hexf = outf[:-4] + '.hex'
    rc2, out2 = sh([os.path.join(CGT, 'hex2000.exe'), '-i', '-memwidth', '16',
                    '-romwidth', '16', outf.replace('/', '\\'), '-o', hexf.replace('/', '\\')])
    print('[%s] hex2000 rc=%d -> %s' % (cfg, rc2, os.path.basename(hexf)))
    return dict(cfg=cfg, nfail=nfail, linked=True, warns=warn_lines,
                out=outf, hex=hexf if os.path.isfile(hexf) else None, log=out)


def main():
    os.makedirs(OUT, exist_ok=True)
    print('复制 bpqym -> %s (原目录只读, 不改动)' % BASE)
    stage_tree()
    d = build('Debug', 'cdebug', 'Debug.baseline.lkf', 'md380_C')
    r = build('Release', 'crelease', 'Release.baseline.lkf', 'MD380_Release')
    json.dump(dict(debug=d, release=r),
              open(os.path.join(DOC, '_baseline_build.json'), 'w', encoding='utf-8'),
              ensure_ascii=False, indent=1)
    return 0 if (d['linked'] and r['linked'] and d['nfail'] == 0 and r['nfail'] == 0) else 1


sys.exit(main())
