"""交付树契约: 文件数 / 编译单元数 / 注释 0 / 非 ASCII 0 / 无 TODO / 目录结构。"""
import os, sys, re, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools'))
from _common import ROOT, Checker, load
import cstrip as C

c = Checker('test_tree_contract')

MAP = load('_map.json')
c.check(len(MAP) == 145, '源码/头文件映射 145 个 (实际 %d)' % len(MAP))

build = [m for m in MAP if not m['dst'].startswith('src/orphan/')
         and m['kind'] in ('c', 'asm')]
c.check(len(build) == 59, '参与构建的编译单元 59 个 (实际 %d)' % len(build))

orphan = [m for m in MAP if m['dst'].startswith('src/orphan/')]
c.check(len(orphan) == 4, '孤儿文件 4 个 (实际 %d)' % len(orphan))

for d in ('src/system', 'src/function', 'src/motor', 'src/subprogram', 'src/orphan',
          'include/device', 'include/common', 'include/function', 'include/motor',
          'asm', 'linker', 'linker/lib', 'project', 'project/gel', 'tests', 'tools',
          'docs', 'build', 'build/Debug', 'build/Release'):
    c.check(os.path.isdir(os.path.join(ROOT, d)), '目录存在: %s' % d)

c.check(os.path.isfile(os.path.join(ROOT, 'src/orphan/README.md')), 'src/orphan/README.md 存在')

# 全树扫描
SRC_EXT = ('.c', '.h', '.asm', '.cmd', '.pjt', '.lkf', '.gel')
KEY = re.compile(r'\b(TODO|FIXME|NOTE|XXX|HACK)\b')
files, code_files, cmt, nonascii, todo = 0, 0, [], [], []
# 只扫交付源码/工程/文档; build/ 是生成物目录 (含原工程只读副本 baseline_tree),
# 里面的源码带注释、含中文, 拿它来判定 "注释 0 / 非 ASCII 0" 是错的。
for rel in C.walk_delivered(ROOT):
    rel_root = rel.split('/')[0]
    if rel_root in ('docs', 'tools', 'tests'):
        continue
    fn = os.path.basename(rel)
    if not fn.endswith(SRC_EXT):
        continue
    files += 1
    if fn.endswith(('.c', '.h', '.asm')):
        code_files += 1
    p = os.path.join(ROOT, rel)
    raw = open(p, 'rb').read()
    try:
        raw.decode('ascii')
    except UnicodeDecodeError:
        nonascii.append(rel)
    t = raw.decode('ascii', 'replace')
    for i, l in enumerate(t.split('\n'), 1):
        if fn.endswith('.asm'):
            if l.strip().startswith('*') or ';' in l:
                cmt.append('%s:%d' % (rel, i))
        elif fn.endswith('.pjt'):
            if l.startswith(';') and 'do not modify' not in l:
                cmt.append('%s:%d' % (rel, i))
        elif '/*' in l or '//' in l:
            cmt.append('%s:%d' % (rel, i))
        if KEY.search(l):
            todo.append('%s:%d: %s' % (rel, i, l.strip()[:60]))

c.check(code_files == 145, '交付源码 (.c/.h/.asm) 145 (实际 %d)' % code_files)
c.check(files == 153, '交付源码+工程/链接/GEL 文件 153 (实际 %d)' % files)
c.check(not cmt, '注释残留 0 (实际 %d) %s' % (len(cmt), cmt[:5]))
c.check(not nonascii, '非 ASCII 0 (实际 %d) %s' % (len(nonascii), nonascii[:5]))
c.check(not todo, 'TODO/FIXME/NOTE 0 (实际 %d) %s' % (len(todo), todo[:3]))

sys.exit(0 if c.done() else 1)
