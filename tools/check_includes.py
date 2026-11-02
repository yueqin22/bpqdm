"""自洽性检查: 新树的每个 #include 都能在新树内解析; 不引用原目录。"""
import os, re, sys, json, collections

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cstrip as C

DST = r"E:/software/bpqdm/bpqdm_rewrite"
DOC = os.path.join(DST, 'docs')
MAP = json.load(open(os.path.join(DOC, '_map.json'), encoding='utf-8'))

INC_DIRS = ['include/device', 'include/common', 'include/function', 'include/motor']
# 编译器自带 / 工具链头 (不在工程内, 属于正常)
SYSTEM_H = {
    'stdio.h', 'stdlib.h', 'string.h', 'math.h', 'ctype.h', 'assert.h',
    'stdarg.h', 'stddef.h', 'limits.h', 'time.h', 'float.h', 'setjmp.h',
    'signal.h', 'errno.h', 'locale.h', 'stdbool.h', 'stdint.h', 'inttypes.h',
}

INC = re.compile(r'^\s*#\s*include\s+([<"])([^">]+)[">]', re.M)
SRC_EXT = ('.c', '.h', '.asm')


def read(p):
    raw = open(p, 'rb').read()
    for enc in ('utf-8', 'gbk', 'gb18030'):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            pass
    return raw.decode('utf-8', 'surrogateescape')


def exists(rel):
    return os.path.exists(os.path.join(DST, rel))


def resolve(cur, inc):
    d = os.path.dirname(cur)
    for c in [os.path.normpath(os.path.join(d, inc)).replace('\\', '/')] + \
             [os.path.normpath(os.path.join(i, inc)).replace('\\', '/') for i in INC_DIRS]:
        if exists(c):
            return c
    return None


files = [m['dst'] for m in MAP]
unresolved = []
resolved_count = 0
for rel in files:
    t = read(os.path.join(DST, rel))
    for m in INC.finditer(t):
        quote, inc = m.group(1), m.group(2)
        r = resolve(rel, inc)
        if r:
            resolved_count += 1
        else:
            unresolved.append((rel, inc, 'system' if inc in SYSTEM_H else 'MISSING'))

missing = [u for u in unresolved if u[2] == 'MISSING']
system = [u for u in unresolved if u[2] == 'system']

# 旧目录引用检查
OLDREF = re.compile(r'md380_C_org11|bpqym|public/28035_header|public/motor_header|'
                    r'public/function_header|function/include', re.I)
oldref = []
for rel in C.walk_delivered(DST):
    p = os.path.join(DST, rel)
    if rel.startswith('docs/') or rel.startswith('tools/'):
        continue
    if not p.endswith(SRC_EXT) and not p.endswith(('.cmd', '.pjt', '.lkf', '.gel')):
        continue
    t = read(p)
    for i, l in enumerate(t.split('\n'), 1):
        if OLDREF.search(l):
            oldref.append('%s:%d: %s' % (rel, i, l.strip()[:100]))

print('resolved includes :', resolved_count)
print('system headers    :', len(system), sorted(set(s[1] for s in system)))
print('MISSING includes  :', len(missing))
for m in missing:
    print('   MISSING', m[0], '->', m[1])
print('references to old tree:', len(oldref))
for o in oldref:
    print('   OLDREF', o)

json.dump(dict(resolved=resolved_count, system=sorted(set(s[1] for s in system)),
               missing=missing, oldref=oldref),
          open(os.path.join(DOC, '_includes.json'), 'w', encoding='utf-8'),
          ensure_ascii=False, indent=1)
