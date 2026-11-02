"""差分测试的负向对照总账。

tools/difftest/negative_control.py 往冻结基准里注入 bug, 逐条确认差分测试台
抓得到。没有这一步, "0 处不一致" 就无法和 "测试台根本没在工作" 区分开。

本测试不需要编译器, 只校验已产生的总账 docs/_difftest/_negctl_summary.json:
  * 每条变异都必须被 CAUGHT;
  * 变体**编译不过**必须单独计数为 broken 并让总账失败 —— 一个跑不起来的变体
    不构成任何证据, 早期版本把 broken 也算成 "抓到", 于是 "14/14 全抓到"
    可能只是 "14 个变体全都编不过"。这条断言就是为了钉死这个假象。
  * 变体**不终止且挂之前一处差异都没有**单独计数为 hung, 同样让总账失败。
    它和 broken 不是一回事: broken 是"一行都没执行", hung 是"执行了但不终止"。
    终止性本身就是语义, 只是差分测试台没法用比字段的方式表达 —— 所以既不能
    算 CAUGHT, 也不能混进 BROKEN。
  * 清单里的每个用例都要有记录, 防止悄悄少测。
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _common import ROOT, DOC, Checker

c = Checker('test_difftest_negctl')

P = os.path.join(DOC, '_difftest', '_negctl_summary.json')
if not os.path.isfile(P):
    print('  [SKIP] 还没有负向对照总账 %s' % os.path.relpath(P, ROOT))
    print('         先运行: python tools/difftest/negative_control.py')
    print('test_difftest_negctl: 0 项, 失败 0 (已跳过)')
    sys.exit(0)

r = json.load(open(P, encoding='utf-8'))
recs = r.get('records') or []

# 清单来源: 负向对照脚本自己的 CASES 表。直接导入它, 避免在测试里另抄一份
# 用例列表 —— 抄一份就会漂移, 漂移了这条断言就形同虚设。
sys.path.insert(0, os.path.join(ROOT, 'tools', 'difftest'))
import negative_control

want = set()
for case, spec in negative_control.CASES.items():
    for mid, _note, _old, _new in spec['muts']:
        want.add((case, mid))
got = set((x['case'], x['mut']) for x in recs)

c.check(r.get('subset') is None,
        '总账是全量跑出来的 (subset=%s)' % r.get('subset'))
c.check(not (want - got), '每条变异都有记录 (缺 %s)' % sorted(want - got))
c.check(not (got - want), '没有多余的变异记录 (多 %s)' % sorted(got - want))
c.check(len(recs) > 0, '记录数 > 0 (实际 %d)' % len(recs))

bad = [x for x in recs if x.get('verdict') == 'MISSED']
broken = [x for x in recs if x.get('verdict') == 'BROKEN']
hung = [x for x in recs if x.get('verdict') == 'HUNG']
c.check(not bad, '没有漏掉的变异 (MISSED=%s)' % [x['mut'] for x in bad])
c.check(not broken, '没有跑不起来的变体 (BROKEN=%s)' % [x['mut'] for x in broken])
c.check(r.get('broken') == 0, '总账 broken 计数为 0 (实际 %s)' % r.get('broken'))
# HUNG 与 BROKEN 必须分开数。理由: 两者都是"没拿到字段级证据", 但成因完全不同 ——
#   * BROKEN: 变体压根没执行, 一行证据都没有;
#   * HUNG  : 变体执行了但**不终止**, 终止性本身就是语义差异, 只是差分测试台
#             表达不了。把它记成 BROKEN 会把"挂住了"和"编不过"混成一堆。
# 两者都必须让总账失败: 它们都不是 CAUGHT。混在一起数会让后人以为前者也是编译问题。
c.check(not hung,
        '没有"不终止且现场无差异"的变体 (HUNG=%s)' % [x['mut'] for x in hung])
c.check(r.get('hung', 0) == 0,
        '总账 hung 计数为 0 (实际 %s)' % r.get('hung'))
c.check(r.get('caught') == r.get('total'),
        '抓到 %s / %s' % (r.get('caught'), r.get('total')))

# CAUGHT 的 rc 必须干净。negative_control.py 把**任何** rc != 0 都记 CAUGHT,
# 所以只看 verdict 会把"进程崩在半路"读成"抓到" —— 它后面的用例根本没跑完,
# 一次字段级比较都没发生。允许的两个 rc:
#   1  = 干净的不一致 (唯一"正常"的 CAUGHT);
#   -2 = 超时, 但挂之前已跑出差异 (negative_control 只在有现场证据时这么记)。
# 历史欠账登记在 _negctl_rc_debt.json (由 build/_make_rc_debt.py 生成): 只许减少。
# 这条断言的作用不是"清零", 而是让欠账**可见且不许增长** —— 一份写着
# "5522/5522 全抓到"的总账, 读者有权知道其中有 74 条其实是崩溃。
DEBT = os.path.join(DOC, '_difftest', '_negctl_rc_debt.json')
dirty = set((x['case'], x['mut']) for x in recs
            if x.get('verdict') == 'CAUGHT' and x.get('rc') not in (1, -2))
debt = set()
if os.path.isfile(DEBT):
    for e in json.load(open(DEBT, encoding='utf-8')).get('entries') or []:
        debt.add((e['case'], e['mut']))
c.check(not (dirty - debt),
        '崩溃记录都已登记欠账 (未登记 %s)' % sorted(dirty - debt)[:8])
c.check(not (debt - dirty),
        '欠账清单没有过期条目 (过期 %s)' % sorted(debt - dirty)[:8])
print('  [INFO] 崩溃欠账 %d 条: rc 不干净的 CAUGHT, 不构成字段级证据' % len(debt))

c.check(r.get('pass') is True, '总账结论为通过')

sys.exit(0 if c.done() else 1)
