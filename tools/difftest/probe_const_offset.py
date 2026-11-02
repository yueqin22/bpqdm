"""探针: MSVC 的 C 前端到底接受哪些"成员偏移"的**常量初始化式**写法。

背景: 目标端 f_funcCode.c 里 funcCodeGradeSum[] 的静态初始化式是
`GetCodeIndex(funcCode.group.fX[0])`, 展开成两个地址常量之差, MSVC 报 C2099。
差分测试要在宿主上重建这张表, 就得找到一种 MSVC 认、且与 GetCodeIndex 等价的
**常量**写法。

第一版探针把 `enum { E = offsetof(...) }` 放进了公共前缀, 于是 9 个形态**全部**
在那一行失败 —— 得到的信息是"offsetof 连 enum 都不认", 而不是各形态的区别。
这一版每个形态给一份完整源文件, 互不干扰。

用法: python tools/difftest/probe_const_offset.py
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, HERE)
import ccenv

PRELUDE = '''#include <stddef.h>
struct INNER { unsigned short a; unsigned short b[3]; };
struct S { unsigned short x; struct INNER in; unsigned short tail; };
typedef unsigned short U16;
'''

# (编号, 说明, 文件正文)
FORMS = [
    ('F1', 'enum { E = offsetof(...) }',
     'enum { E = offsetof(struct S, in.b[0]) };\n'
     'unsigned short tbl[1] = { (U16)E };\n'),

    ('F2', 'offsetof 直接当初始化式',
     'unsigned short tbl[1] = { (U16)offsetof(struct S, in.b[0]) };\n'),

    ('F3', 'offsetof / sizeof(U16)',
     'unsigned short tbl[1] = { (U16)(offsetof(struct S, in.b[0]) / sizeof(U16)) };\n'),

    ('F4', '&((S*)0)->m 转整型',
     'unsigned short tbl[1] = { (U16)(size_t)&((struct S *)0)->in.b[0] };\n'),

    ('F5', '字符指针相减 (&m - 0)',
     'unsigned short tbl[1] = { (U16)((char *)&((struct S *)0)->in.b[0] - (char *)0) };\n'),

    ('F6', 'U16 指针相减 (&m - 0)',
     'unsigned short tbl[1] = { (U16)((U16 *)&((struct S *)0)->in.b[0] - (U16 *)0) };\n'),

    ('F7', '先算字符偏移再除 sizeof',
     'unsigned short tbl[1] = '
     '{ (U16)(((char *)&((struct S *)0)->in.b[0] - (char *)0) / sizeof(U16)) };\n'),

    ('F8', '两个成员地址之差 (对照, 预期失败)',
     'unsigned short tbl[1] = '
     '{ (U16)((U16 *)&((struct S *)0)->in.b[0] - (U16 *)&((struct S *)0)->x) };\n'),

    ('F9', '运行期赋值 (对照, 预期成功)',
     'unsigned short tbl[1];\n'
     'void f(void) { tbl[0] = (U16)offsetof(struct S, in.b[0]); }\n'),

    ('F10', '运行期指针相减 (对照, 预期成功)',
     'unsigned short tbl[1];\n'
     'void f(void) '
     '{ tbl[0] = (U16)((U16 *)&((struct S *)0)->in.b[0] - (U16 *)0); }\n'),

    ('F11', '运行期 GetCodeIndex 式 (对照, 预期成功)',
     'static struct S s;\n'
     'unsigned short tbl[1];\n'
     'void f(void) '
     '{ tbl[0] = (U16)((U16 *)&s.in.b[0] - (U16 *)&s.x); }\n'),
]


def main():
    out = os.path.join(ROOT, 'build', '_probe_const_offset')
    os.makedirs(out, exist_ok=True)
    print('MSVC C 前端 / 常量初始化式 探针')
    print('=' * 68)
    ok, fail = [], []
    for mid, note, body in FORMS:
        src = os.path.join(out, 'p_%s.c' % mid)
        with open(src, 'w', encoding='utf-8') as f:
            f.write(PRELUDE + body + 'int main(void) { f(); return tbl[0]; }\n')
        exe = os.path.join(out, 'p_%s.exe' % mid)
        rc, log = ccenv.compile_exe([src], exe, workdir=out)
        (ok if rc == 0 else fail).append(mid)
        print('  [%s] %-4s %s' % ('OK  ' if rc == 0 else 'FAIL', mid, note))
        if rc != 0:
            for line in log.splitlines():
                low = line.lower()
                if 'error' in low:
                    print('         %s' % line.strip()[-105:])
    print('=' * 68)
    print('可用: %s' % (', '.join(ok) if ok else '(无)'))
    print('不可用: %s' % (', '.join(fail) if fail else '(无)'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
