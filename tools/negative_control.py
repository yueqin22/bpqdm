"""负向对照: 故意注入错误, 确认静态检查真的会报错。
不报错的检查等于没检查。
"""
import os, sys, runpy, json

DST = r"E:/software/bpqdm/bpqdm_rewrite"
DOC = os.path.join(DST, 'docs')


def pick_target():
    """挑一个**未改写**的 src/*.c 模块当注入对象。

    改写过的模块按设计豁免 token 等价检查 (它的不一致记在 `rewritten` 里, 不进
    `mismatch`)。这里原先硬编码 `src/function/f_common.c`, 而它后来被登记成改写
    模块 —— 于是 token 检查再也抓不到注入, 负向对照自己失效了, 却看着像"检查有
    bug"。所以改成从清单里现挑, 并保证挑出来的确实没被改写: 下一个模块被改写时
    本文件自动换目标, 不会悄悄失效。
    """
    rw = set()
    p = os.path.join(DOC, 'rewritten_modules.json')
    if os.path.isfile(p):
        rw = set(m['dst'] for m in json.load(open(p, encoding='utf-8'))['modules'])
    man = json.load(open(os.path.join(DOC, '_map.json'), encoding='utf-8'))
    for m in man:
        if m['kind'] == 'c' and m['dst'].startswith('src/') and m['dst'] not in rw:
            return m['dst']
    raise SystemExit('找不到未改写的 src/*.c 模块, 无法做 token 负向对照')


TARGET_REL = pick_target()
TARGET = os.path.join(DST, TARGET_REL)

INJECT = ('\nvoid _negctl_probe(void)\n{\n    _negctl_undeclared_fn();\n}\n'
          '/* NEGCTL_COMMENT */\n'
          'int _negctl_cn = 1;\n')


def run_verify():
    g = runpy.run_path(os.path.join(DST, 'tools/verify_tree.py'))
    return g['report']


def summarize(r):
    return dict(
        token_mismatch=r['token_stream_identical']['mismatch'],
        comment_residual=[x[0] for x in r['comment_residual']],
        non_ascii=[x.split(':')[0] for x in r['non_ascii']],
        implicit=[x for x in (r['implicit_declaration'] +
                              r['implicit_undeclared_elsewhere']) if '_negctl' in x],
    )


backup = open(TARGET, 'rb').read()
try:
    base = run_verify()
    print('注入目标      :', TARGET_REL)
    print('BASELINE      :', summarize(base))

    with open(TARGET, 'ab') as f:
        f.write(INJECT.encode('ascii'))
    # 再注入一段非 ASCII 字节 (GBK 中文), 用于触发 non_ascii 检查
    with open(TARGET, 'ab') as f:
        f.write(b'\nint _negctl_cn2 = 1;\n/* \xd6\xd0\xce\xc4 */\n')

    bad = run_verify()
    s = summarize(bad)
    print('AFTER INJECT  :', s)

    ok = True
    if not s['token_mismatch']:
        print('FAIL: token 检查没抓到注入'); ok = False
    if not s['comment_residual']:
        print('FAIL: 注释检查没抓到注入'); ok = False
    if not s['implicit']:
        print('FAIL: 隐式声明检查没抓到注入'); ok = False
    if not s['non_ascii']:
        print('FAIL: 非 ASCII 检查没抓到注入'); ok = False
    print('NEGATIVE CONTROL:', 'PASS' if ok else 'FAIL')
    json.dump(dict(pass_=ok, target=TARGET_REL, after=s, baseline=summarize(base)),
              open(os.path.join(DST, 'docs', '_negctl.json'), 'w', encoding='utf-8'),
              ensure_ascii=False, indent=1)
finally:
    with open(TARGET, 'wb') as f:
        f.write(backup)
    r = run_verify()
    print('RESTORED      :', summarize(r))
    print('restore clean :',
          not r['token_stream_identical']['mismatch'] and not r['comment_residual'])
