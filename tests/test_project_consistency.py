"""工程一致性: .pjt 源文件清单 == .lkf 目标文件清单; 链接脚本/包含路径存在。"""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _common import ROOT, Checker

c = Checker('test_project_consistency')
PJT = os.path.join(ROOT, 'project/jt380.pjt')
LKFD = os.path.join(ROOT, 'project/Debug.lkf')
LKFR = os.path.join(ROOT, 'project/Release.lkf')

c.check(os.path.isfile(PJT), 'project/jt380.pjt 存在')
c.check(os.path.isfile(LKFD), 'project/Debug.lkf 存在')
c.check(os.path.isfile(LKFR), 'project/Release.lkf 存在')

pjt = open(PJT, encoding='ascii').read()
srcs = re.findall(r'^Source="(.+?)"', pjt, re.M)
code = [s for s in srcs if s.lower().endswith(('.c', '.asm'))]
cmds = [s for s in srcs if s.lower().endswith('.cmd')]
c.check(len(code) == 59, '.pjt 编译单元 59 (实际 %d)' % len(code))
c.check(len(cmds) == 2, '.pjt 链接脚本 2 (实际 %d)' % len(cmds))
c.check('Config="Debug"' in pjt and 'Config="Release"' in pjt, '.pjt 含 Debug/Release 两配置')
for k in ('["Compiler" Settings: "Debug"]', '["Compiler" Settings: "Release"]',
          '["Linker" Settings: "Debug"]', '["Linker" Settings: "Release"]'):
    c.check(k in pjt, '.pjt 含 %s' % k)

for p, name in ((LKFD, 'Debug'), (LKFR, 'Release')):
    t = open(p, encoding='ascii').read().splitlines()
    objs = [l for l in t if l.strip().lower().endswith('.obj"')]
    cmdl = [l for l in t if l.strip().lower().endswith('.cmd"')]
    c.check(len(objs) == 59, '%s.lkf obj 数 59 (实际 %d)' % (name, len(objs)))
    c.check(len(cmdl) == 2, '%s.lkf cmd 数 2 (实际 %d)' % (name, len(cmdl)))
    c.check('rts2800_ml.lib' in t[0], '%s.lkf 链接 rts2800_ml.lib' % name)
    c.check('-ecode_start' in t[0], '%s.lkf 入口为 code_start' % name)

# .pjt 源文件与 .lkf obj 名一一对应
pjt_stems = sorted(os.path.splitext(os.path.basename(s))[0] for s in code)
lkf_stems = sorted(os.path.splitext(os.path.basename(
    l.strip().strip('"')))[0] for l in open(LKFD, encoding='ascii').read().splitlines()
    if l.strip().lower().endswith('.obj"'))
c.check(pjt_stems == lkf_stems, '.pjt 源文件与 Debug.lkf obj 名一一对应')

# 链接脚本与包含目录存在
for f in ('linker/F28035.cmd', 'linker/DSP28035Peripheral.cmd', 'linker/lib/rts2800_ml.lib'):
    c.check(os.path.isfile(os.path.join(ROOT, f)), '文件存在: %s' % f)
for d in ('include/device', 'include/common', 'include/function', 'include/motor'):
    c.check(os.path.isdir(os.path.join(ROOT, d)), '包含路径存在: %s' % d)

# .pjt 的 -i 路径覆盖上述四个目录
for d in ('device', 'common', 'function', 'motor'):
    c.check('include\\%s' % d in pjt, '.pjt -i 含 include\\%s' % d)

sys.exit(0 if c.done() else 1)
