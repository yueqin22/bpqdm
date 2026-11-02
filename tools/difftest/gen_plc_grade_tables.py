"""从 include/function/f_funcCode.h 抽出等级表的形状, 生成各用例的 grade_tables.inc。

输出给**每个**需要真 `funcCodeGradeSum[]` 的用例, 内容完全相同 (这份表是模块的
共享输入, 与用它的模块无关):
  * cases/f_plc/grade_tables.inc       —— f_plc 的访问控制读它
  * cases/f_profibus/grade_tables.inc  —— f_profibus 的 GetGradeIndex 读它

脚本名里的 `plc` 是历史遗留 (f_plc 是第一个需要的用例); 现在它是通用的,
输出清单见下面的 OUTS。

为什么是生成而不是直接链接 f_funcCode.c:

  1. f_funcCode.c 同时定义 funcCode, 而我们已经在 mock 里定义它 (而且是**共享**状态,
     两侧共用一份) —— 链接整份会撞名。
  2. 更要紧的是它编不过: funcCodeGradeSum 的静态初始化式是
     `GetCodeIndex(funcCode.group.fX[0])`, 展开后是**两个地址常量之差**, MSVC 报
     C2099 (funcCodeAttribute 里也有若干处)。这正是 host_patch.json 处理的那一类,
     但补丁机制只覆盖"被测模块的 ref/new 两份副本", 覆盖不到一个第三方数据模块。

解法是换一种等价的**常量**写法。这里有两个已经实测掉的坑:

  * `offsetof(FUNCCODE_ALL, x) / sizeof(Uint16)` —— 不行。MSVC 的 C 前端会把
    `offsetof` / `&((T*)0)->m` 折叠成一个**地址常量**, 但拒绝让地址常量参与运算:
    实测 `(U16)offsetof(...)`、`(U16)(size_t)&((T*)0)->m`、
    `(U16)((char*)&((T*)0)->m - (char*)0)` 都能编过, 而只要出现 `/`、`>>` 或
    两个地址常量相减就是 C2099。详见 tools/difftest/probe_const_offset.py。

  * `funcCodeGradeSum[i] == 前 i 项 funcCodeGradeAll[] 之和` —— 不行。
    funcCodeGradeAll[] 数的是**该组有多少个可用等级项** (FENUM-2、U9NUM-15 这种),
    而不是组的**数组长度**; 两者不相等。

真正可行的是 **sizeof 算术**:

    funcCodeGradeSum[k] == 前 k 个成员的 sizeof 之和 / sizeof(Uint16)

因为 struct FUNCCODE_GROUP 是一串**扁平的 Uint16 数组** (无嵌套、无填充),
成员偏移就是前面各成员长度之和; 而 sizeof 是整型常量表达式, MSVC 正常折叠。

  * 这一条不引入任何"结构体长什么样"的重复知识 —— 成员表是从头文件里读出来的,
    不是抄的。
  * 83 个 funcCodeGradeSum 项与 struct FUNCCODE_GROUP 的 83 个成员**一一对应且同序**
    (生成时断言, 头文件一旦重排就报错而不是悄悄生成一张错的表)。
  * 等价性仍然在运行期被**测**一遍: driver.c 用模块自己的 GetCodeIndex 重算 83 项
    逐项比对 (GRADE_SUM_LIST 这个 X 宏只写一份, 两处共用)。而 f_plc 的访问控制
    (FuncCodeRwAtrrib 里的 GetGradeIndex) 恰恰就读这张表。

用法:
    python tools/difftest/gen_plc_grade_tables.py          # 生成
    python tools/difftest/gen_plc_grade_tables.py --check  # 只校验是否过期
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))

SRC = os.path.join(ROOT, 'src', 'function', 'f_funcCode.c')
HDR = os.path.join(ROOT, 'include', 'function', 'f_funcCode.h')
# 需要这份表的用例, 全部写同一份内容。加用例就往这里加一行。
OUTS = [os.path.join(HERE, 'cases', c, 'grade_tables.inc')
        for c in ('f_plc', 'f_profibus', 'f_comm')]

GROUP_NUM = 83


def body_of(text, name):
    m = re.search(r'const\s+Uint16\s+' + name + r'\s*\[[^\]]*\]\s*=\s*\{(.*?)\n\};',
                  text, re.S)
    if not m:
        raise SystemExit('在 %s 里找不到 %s 的定义' % (SRC, name))
    return m.group(1)


def split_entries(body):
    # 源码里这些项的空白是手工对齐的 (有一项还夹了个制表符), 折成单空格让生成结果
    # 稳定、可 diff。这些都是纯算术式, 空白不参与语义。
    return [' '.join(e.split()) for e in body.split(',') if e.strip()]


def group_members():
    """struct FUNCCODE_GROUP 的成员名, 按声明顺序。"""
    h = open(HDR, encoding='utf-8').read()
    m = re.search(r'struct\s+FUNCCODE_GROUP\s*\{(.*?)\n\};', h, re.S)
    if not m:
        raise SystemExit('在 %s 里找不到 struct FUNCCODE_GROUP' % HDR)
    return re.findall(r'Uint16\s+(\w+)\s*\[([^\]]+)\]\s*;', m.group(1))


def build():
    text = open(SRC, encoding='utf-8').read()

    allv = split_entries(body_of(text, 'funcCodeGradeAll'))
    sumv = split_entries(body_of(text, 'funcCodeGradeSum'))
    members = group_members()

    if len(allv) != GROUP_NUM:
        raise SystemExit('funcCodeGradeAll 有 %d 项, 期望 %d' % (len(allv), GROUP_NUM))
    if len(sumv) != GROUP_NUM:
        raise SystemExit('funcCodeGradeSum 有 %d 项, 期望 %d' % (len(sumv), GROUP_NUM))
    if len(members) != GROUP_NUM:
        raise SystemExit('struct FUNCCODE_GROUP 有 %d 个成员, 期望 %d'
                         % (len(members), GROUP_NUM))

    names = []
    for e in sumv:
        m = re.match(r'^GetCodeIndex\((.+)\)$', e)
        if not m:
            raise SystemExit('funcCodeGradeSum 的项不是 GetCodeIndex(...): %r' % e)
        arg = m.group(1).strip()
        # GetCodeIndex(code) 展开成 (&(code) - &(funcCode.all[0])), 所以它的实参是
        # `funcCode.group.f0[0]` —— 带 `funcCode.` 前缀。这个前缀是 driver 侧
        # "用模块自己的 GetCodeIndex 重算"所必需的, 所以只剥不丢:
        # driver 用 `funcCode.m` 把它拼回去 (见 GRADE_SUM_LIST 的用法)。
        if not arg.startswith('funcCode.group.'):
            raise SystemExit('funcCodeGradeSum 的实参不是 funcCode.group.* 形式: %r' % arg)
        names.append(arg[len('funcCode.group.'):].split('[')[0])

    # 一一对应且同序 —— 这是"sizeof 累加 == GetCodeIndex"成立的前提。头文件一旦
    # 重排成员或改动某一组的可用项数, 这里立刻停下, 而不是生成一张看似合理的错表。
    if names != [n for n, _ in members]:
        for i, (a, b) in enumerate(zip(names, [n for n, _ in members])):
            if a != b:
                raise SystemExit(
                    'funcCodeGradeSum[%d] 是 %s, 但 struct FUNCCODE_GROUP 的第 %d 个'
                    '成员是 %s —— 两者必须一一对应且同序' % (i, a, i, b))
        raise SystemExit('funcCodeGradeSum 与 struct FUNCCODE_GROUP 成员数不一致')

    L = []
    L.append('/* 由 tools/difftest/gen_plc_grade_tables.py 生成, 不要手改。')
    L.append(' * 来源: src/function/f_funcCode.c 的 funcCodeGradeAll / funcCodeGradeSum,')
    L.append(' *       以及 include/function/f_funcCode.h 的 struct FUNCCODE_GROUP。')
    L.append(' * 重新生成: python tools/difftest/gen_plc_grade_tables.py')
    L.append(' * 过期校验: python tools/difftest/gen_plc_grade_tables.py --check')
    L.append(' *   (注意: 没有任何自动测试会跑 --check —— 它是手动工具。改过')
    L.append(' *    f_funcCode.c 的两张表或 f_funcCode.h 的成员表之后要自己跑一次。)')
    L.append(' */')
    L.append('')
    L.append('/* GRADE_SUM_LIST 的每一项是 funcCodeGradeSum[] 里那个 GetCodeIndex() 的**实参**')
    L.append(' * 去掉 `funcCode.` 前缀之后的成员指示符。')
    L.append(' *')
    L.append(' * 只写这一份, 两处共用:')
    L.append(' *   driver.c   用 GetCodeIndex(funcCode.m) 展开成**运行期值**, 与下面这张')
    L.append(' *              表逐项比对 —— 于是"sizeof 累加 == GetCodeIndex"是被**测过**的,')
    L.append(' *              不是被假设的, 而 f_plc 的访问控制恰恰就读这张表。')
    L.append(' *  (mock_plc.c 不再需要它 —— 它改用下面的 GS_* 与 GRADE_SUM_INIT。) */')
    L.append('#define GRADE_SUM_LIST(X)                     \\')
    for i, m in enumerate(names):
        tail = '  \\' if i + 1 < len(names) else ''
        L.append('    X(%-34s)%s' % ('group.%s[0]' % m, tail))
    L.append('')
    L.append('/* 一个成员占多少个 Uint16 字。sizeof 是整型常量表达式, MSVC 折叠;')
    L.append(' * 而 offsetof 折出来的地址常量 MSVC 不让参与运算 (见脚本 docstring)。 */')
    L.append('#define PLC_WORDS(m)        ((Uint16)(sizeof(funcCode.group.m) / sizeof(Uint16)))')
    L.append('')
    L.append('/* struct FUNCCODE_GROUP 各成员的起始**字**偏移 = 前面所有成员长度之和。')
    L.append(' * 结构体是一串扁平 Uint16 数组, 无嵌套无填充, 所以成员偏移就是前缀长度和。 */')
    for i, (name, _sizemacro) in enumerate(members):
        if i == 0:
            L.append('#define GS_%-9s 0' % name)
        else:
            prev = members[i - 1][0]
            L.append('#define GS_%-9s (GS_%s + PLC_WORDS(%s))' % (name, prev, prev))
    L.append('')
    L.append('/* funcCodeGradeSum[] 的初始值, 与 GRADE_SUM_LIST 同序。 */')
    L.append('#define GRADE_SUM_INIT                        \\')
    for i in range(0, GROUP_NUM, 4):
        chunk = ', '.join('GS_' + n for n, _ in members[i:i + 4])
        tail = '  \\' if i + 4 < GROUP_NUM else ''
        L.append('    %-52s%s' % (chunk + ',', tail))
    L.append('')
    L.append('/* funcCodeGradeAll: 纯宏算术, 原样照搬 (只是折成单空格)。')
    L.append(' * 用作 funcCodeGradeAll[] 的初始值, 所以给的是初始值列表而不是完整数组 ——')
    L.append(' * 真符号要由 mock_plc.c 定义, 数组维度才对得上头文件里的 extern 声明。 */')
    L.append('#define GRADE_ALL_INIT                        \\')
    for i in range(0, GROUP_NUM, 4):
        chunk = ', '.join(allv[i:i + 4])
        tail = '  \\' if i + 4 < GROUP_NUM else ''
        L.append('    %-46s%s' % (chunk + ',', tail))
    L.append('')
    return '\n'.join(L)


def main():
    want = build()
    check = '--check' in sys.argv

    stale = []
    for out in OUTS:
        cur = open(out, encoding='utf-8').read() if os.path.isfile(out) else None
        if cur != want:
            stale.append(out)

    rel = lambda p: os.path.relpath(p, ROOT).replace('\\', '/')

    if check:
        if stale:
            print('grade_tables.inc 已过期: %s' % ', '.join(rel(p) for p in stale))
            print('请重新生成: python tools/difftest/gen_plc_grade_tables.py')
            return 1
        print('grade_tables.inc 是最新的 (%d 份)' % len(OUTS))
        return 0

    for out in OUTS:
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, 'w', encoding='utf-8', newline='\n') as f:
            f.write(want)
        print('已写出 %s' % rel(out))
    return 0


if __name__ == '__main__':
    sys.exit(main())
