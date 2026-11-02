"""按 project/jt380.pjt 的真实配置构建 Debug 与 Release, 生成映像与日志。

把 CCS 变量 $(Proj_dir) 展开为实际路径后逐字执行工程里的选项字符串,
因此这是对工程配置的忠实复现 (含原工程遗留的 -o2 / -o3)。
"""
import os, re, sys, json, subprocess, shutil, threading, itertools
from concurrent.futures import ThreadPoolExecutor

CGT = 'E:/ti/c2000/cgt-c2000_25.11.1.LTS/ti-cgt-c2000_25.11.1.LTS/bin'
DST = 'E:/software/bpqdm/bpqdm_rewrite'
PROJ = os.path.join(DST, 'project')
DOC = os.path.join(DST, 'docs')
P = json.load(open(os.path.join(DOC, '_project.json'), encoding='utf-8'))
PROJ_DIR = PROJ.replace('/', '\\')

TMPROOT = os.path.join(DST, 'build/_tmp')
_seq = itertools.count(1)
_seq_lock = threading.Lock()


def expand(s):
    return s.replace('$(Proj_dir)', PROJ_DIR)


def sh(cmd, cwd=None, timeout=300):
    """跑一条工具链命令。

    两处防护都是踩出来的:
      * **timeout**: cl2000 曾出现只占 0.09 s CPU 就再也不返回的挂死,
        不带 timeout 时整个构建永久停住, 日志里一行都不多, 无法与"编得慢"区分。
      * **每次调用独立的 TEMP/TMP**: 6 线程并发编译共用一个临时目录时会偶发死锁
        (build_and_compare.py 里 f_error 连续两轮都停在同一处, 串行却 1 s 编完)。
    """
    with _seq_lock:
        n = next(_seq)
    d = os.path.join(TMPROOT, 'c%04d' % n)
    os.makedirs(d, exist_ok=True)
    env = dict(os.environ)
    env['TMP'] = d
    env['TEMP'] = d
    try:
        r = subprocess.run(cmd, cwd=cwd, env=env, capture_output=True, text=True,
                           errors='replace', timeout=timeout)
    except subprocess.TimeoutExpired as ex:
        o = ex.stdout if isinstance(ex.stdout, str) else ''
        return 124, o + '\nTIMEOUT after %ss' % timeout
    return r.returncode, (r.stdout or '') + (r.stderr or '')


def build(cfg, opts, lkf, outdir):
    print('=' * 60)
    print('== %s 构建' % cfg)
    print('=' * 60)
    os.makedirs(outdir, exist_ok=True)
    opts = expand(opts).replace('"', '').split()
    # 新 CGT 默认 EABI; 本工程是 COFF (老 .cmd / 老 rts2800_ml.lib), 必须显式切换。
    # 该开关是**新工具链的兼容性要求**, 不是对工程语义的改动 ——
    # CCS 3.3 的 CGT 不认 --abi, 所以不写进 jt380.pjt (见 rewrite_decisions.md D8)。
    if '--abi=coffabi' not in opts:
        opts.insert(0, '--abi=coffabi')
    # -fr 指向 build/<cfg>
    opts = [o for o in opts if not o.startswith('-fr')]
    opts.append('-fr' + outdir.replace('/', '\\'))

    log = []
    warn_lines = []

    def one(src):
        # .pjt 里的源码路径是相对 project/ 的 (即 ..\src\...), CCS 也在该目录下
        # 调用编译器; 这里保持一致, 否则相对路径解析不到源文件。
        rel = '..\\' + src.replace('/', '\\')
        cmd = [os.path.join(CGT, 'cl2000.exe')] + opts + ['-c', rel]
        rc, out = sh(cmd, cwd=PROJ)
        return src, rc, out, cmd

    with ThreadPoolExecutor(max_workers=6) as ex:
        res = list(ex.map(one, P['ordered']))

    nfail = 0
    shown = 0
    for src, rc, out, cmd in res:
        stem = os.path.splitext(os.path.basename(src))[0]
        obj = os.path.join(outdir, stem + '.obj')
        ok = rc == 0 and os.path.isfile(obj)
        if not ok:
            nfail += 1
            if shown < 3:
                shown += 1
                print('  FAIL %s rc=%d' % (stem, rc))
                print('       ' + ' '.join(cmd)[:300])
                print('       ' + out.strip()[:600].replace('\n', '\n       '))
        for l in out.split('\n'):
            if 'warning' in l.lower() or 'error' in l.lower() or 'remark' in l.lower():
                warn_lines.append('[%s] %s' % (stem, l.strip()))
        log.append('[%s] %s' % (stem, 'OK' if ok else 'FAILED(rc=%d)' % rc))
        log.append('    ' + ' '.join(cmd))
        if out.strip():
            log.append('    ' + out.strip().replace('\n', '\n    '))

    print('编译: %d 个, 失败 %d' % (len(res), nfail))
    print('警告/错误行: %d' % len(warn_lines))

    # ---- 链接 ----
    print('链接 (cd project && cl2000 -@%s) ...' % lkf)
    rc, out = sh([os.path.join(CGT, 'cl2000.exe'), '-@' + lkf], cwd=PROJ)
    print('链接 rc=%d' % rc)
    if out.strip():
        print(out.strip()[:1500])
    log.append('')
    log.append('[Linking] cl2000 -@%s' % lkf)
    log.append(out.strip())

    outfile = os.path.join(outdir, 'jt380_C.out' if cfg == 'Debug' else 'jt380_Release.out')
    if not os.path.isfile(outfile):
        print('!! 未生成 %s' % outfile)
        return dict(cfg=cfg, nfail=nfail, linked=False, log=log,
                    warns=warn_lines, hex=None, out=None)

    # ---- hex ----
    hexf = outfile[:-4] + '.hex'
    rc2, out2 = sh([os.path.join(CGT, 'hex2000.exe'), '-i', '-memwidth', '16',
                    '-romwidth', '16', outfile.replace('/', '\\'), '-o', hexf.replace('/', '\\')])
    print('hex2000 rc=%d -> %s' % (rc2, os.path.basename(hexf)))
    log.append('[hex2000] rc=%d' % rc2)
    log.append(out2.strip())
    return dict(cfg=cfg, nfail=nfail, linked=True, log=log, warns=warn_lines,
                hex=hexf if os.path.isfile(hexf) else None, out=outfile)


def main():
    d = build('Debug', P['cdebug'], 'Debug.lkf', os.path.join(DST, 'build/Debug'))
    r = build('Release', P['crelease'], 'Release.lkf', os.path.join(DST, 'build/Release'))
    json.dump(dict(debug=d, release=r),
              open(os.path.join(DOC, '_official_build.json'), 'w', encoding='utf-8'),
              ensure_ascii=False, indent=1)
    return 0 if (d['nfail'] == 0 and d['linked'] and r['nfail'] == 0 and r['linked']) else 1


sys.exit(main())
