"""负向对照: 证明静态检查真的会报错 (不报错的检查等于没检查)。"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _common import ROOT, Checker, run_tool, load

c = Checker('test_negative_control')
print('  (会临时改动一个 src/*.c 模块并在结束时自动还原, 目标由工具自己挑)')
run_tool('negative_control.py')
r = load('_negctl.json')

tgt = r.get('target')
c.check(tgt, '负向对照记录了注入目标: %s' % tgt)

# 注入目标必须是**未改写**的模块。改写模块按设计豁免 token 等价检查 (它的不一致
# 记在 rewritten 里, 不进 mismatch), 往它身上注入的话 token 检查永远不会响 ——
# 负向对照会静默退化成"四项里一项空转"。这里卡住它, 免得下次改写又踩同一个坑。
man = load('rewritten_modules.json') or {'modules': []}
rw = set(m['dst'] for m in man['modules'])
c.check(tgt and tgt not in rw,
        '注入目标未被改写 (否则 token 检查抓不到): %s' % tgt)

a = r['after']
c.check(a['token_mismatch'], 'token 流检查抓到注入: %s' % a['token_mismatch'])
c.check(a['comment_residual'], '注释检查抓到注入: %s' % a['comment_residual'])
c.check(a['non_ascii'], '非 ASCII 检查抓到注入: %s' % a['non_ascii'])
c.check(a['implicit'], '隐式声明检查抓到注入: %s' % a['implicit'])
c.check(r['pass_'], '负向对照总体通过')

# 还原后必须干净
v = load('_verify.json')
c.check(not v['token_stream_identical']['mismatch'], '还原后 token 流干净')
c.check(not v['comment_residual'], '还原后无注释残留')

sys.exit(0 if c.done() else 1)
