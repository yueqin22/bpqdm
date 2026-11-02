"""token 流等价: 145 个文件逐一与原工程比对。

未列入 docs/rewritten_modules.json 的模块必须逐 token 一致;
列入的模块本来就该不一致, 但要求它们**确实变了** (否则这轮改写是空转)。
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _common import ROOT, DOC, Checker, load, run_tool

c = Checker('test_token_equivalence')
r = run_tool('verify_tree.py')['report']

mm = r['token_stream_identical']['mismatch']
rw = r['token_stream_identical'].get('rewritten', [])
man = json.load(open(os.path.join(DOC, 'rewritten_modules.json'), encoding='utf-8'))
want = set(m['dst'] for m in man['modules'])

c.check(r['token_stream_identical']['files'] == 145,
        '比对文件数 145 (实际 %d)' % r['token_stream_identical']['files'])
c.check(not mm, '未改写模块 token 流不一致 0 处 (实际 %d) %s' % (len(mm), mm[:5]))
c.check(set(x['dst'] for x in rw) == want,
        '改写清单与实际一致 (%d 个)' % len(want))
stale = [x['dst'] for x in rw if x['token_same']]
c.check(not stale, '清单内模块确实被改过 (空转的: %s)' % stale)
c.check(not r['symbol_identical'],
        '符号抽取不一致 0 处 (实际 %d)' % len(r['symbol_identical']))
c.check(not r['comment_residual'],
        '注释残留 0 (实际 %d)' % len(r['comment_residual']))
c.check(not r['non_ascii'], '非 ASCII 0 (实际 %d)' % len(r['non_ascii']))
c.check(not r['idempotent'], '二次去注释零可删 (实际 %d)' % len(r['idempotent']))
c.check(not r['todo_fixme_note'], 'TODO/FIXME/NOTE 0 (实际 %d)' % len(r['todo_fixme_note']))

sys.exit(0 if c.done() else 1)
