"""生成新工程的链接脚本 / 工程配置 / GEL, 并创建 tests 与 build 骨架。"""
import os, re, sys, json, shutil

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cstrip as C

SRC = r"E:/software/bpqdm/bpqym"
DST = r"E:/software/bpqdm/bpqdm_rewrite"
DOC = os.path.join(DST, 'docs')

A = json.load(open(os.path.join(DOC, '_audit.json'), encoding='utf-8'))
MAP = json.load(open(os.path.join(DOC, '_map.json'), encoding='utf-8'))

# ------------------------------------------------------------------ 链接脚本
def strip_and_copy(src_rel, dst_rel, kind='c'):
    sp = os.path.join(SRC, src_rel)
    dp = os.path.join(DST, dst_rel)
    os.makedirs(os.path.dirname(dp), exist_ok=True)
    raw = open(sp, 'rb').read()
    text, enc = C.decode_any(raw)
    eol = C.eol_of(text)
    if eol == 'mixed':
        eol = 'CRLF' if text.count('\r\n') >= (text.count('\n') - text.count('\r\n')) else 'LF'
    if kind == 'c':
        s, n = C.strip_c(text)
    else:
        s, n = C.strip_asm(text)
    final = C.tidy(s, eol)
    if kind == 'c':
        assert C.c_tokens(text) == C.c_tokens(final), 'token mismatch ' + src_rel
    with open(dp, 'wb') as f:
        f.write(C.encode_back(final, enc))
    return n, enc


for f in ('F28035.cmd', 'DSP28035Peripheral.cmd'):
    n, enc = strip_and_copy('public/common/' + f, 'linker/' + f)
    print('linker/%-24s comments=%-4d enc=%s' % (f, n, enc))

os.makedirs(os.path.join(DST, 'linker/lib'), exist_ok=True)
for f in ('rts2800_ml.lib', 'IQmath.lib'):
    shutil.copyfile(os.path.join(SRC, 'public/common/' + f),
                    os.path.join(DST, 'linker/lib', f))
print('linker/lib copied')

for f in os.listdir(os.path.join(SRC, 'gel')):
    n, enc = strip_and_copy('gel/' + f, 'project/gel/' + f)
    print('project/gel/%-14s comments=%-4d enc=%s' % (f, n, enc))

# ------------------------------------------------------------------ 工程文件
sources = [m['dst'] for m in MAP
           if m['kind'] in ('c', 'asm') and not m['dst'].startswith('src/orphan/')]
sysrc = sorted(s for s in sources if s.startswith('src/system/'))
fsrc = sorted(s for s in sources if s.startswith('src/function/'))
msrc = sorted(s for s in sources if s.startswith('src/motor/'))
ssrc = sorted(s for s in sources if s.startswith('src/subprogram/'))
asrc = sorted(s for s in sources if s.startswith('asm/'))
ordered = sysrc + fsrc + msrc + ssrc + asrc
assert len(ordered) == 59, len(ordered)

INC = ('-i"$(Proj_dir)\\..\\include\\device" '
       '-i"$(Proj_dir)\\..\\include\\common" '
       '-i"$(Proj_dir)\\..\\include\\function" '
       '-i"$(Proj_dir)\\..\\include\\motor"')

CDEBUG = ('-g -q -o2 ' + INC + ' -fr"$(Proj_dir)\\..\\build\\Debug" '
          '-d"_DEBUG" -d"DSP2803X" -d"LARGE_MODEL" -ml -mt -v28')
CRELEASE = ('-pdsw225 -o3 ' + INC + ' -fr"$(Proj_dir)\\..\\build\\Release" '
            '-d"DSP2803X" -d"LARGE_MODEL" -ml -v28')

LD = ('-z -q -c -ecode_start -m"..\\build\\Debug\\jt380_C.map" -o"..\\build\\Debug\\jt380_C.out" '
      '-stack0x400 -w -x -i"..\\linker\\lib" -l"rts2800_ml.lib"')
LR = ('-z -c -ecode_start -m"..\\build\\Release\\jt380_Release.map" -o"..\\build\\Release\\jt380_Release.out" '
      '-stack0x400 -w -x -i"..\\linker\\lib" -l"rts2800_ml.lib"')

P = []
P.append('; Code Composer Project File, Version 2.0 (do not modify or remove this line)')
P.append('')
P.append('[Project Settings]')
P.append('ProjectDir="%s\\project\\"' % DST.replace('/', '\\'))
P.append('ProjectType=Executable')
P.append('CPUFamily=TMS320C28XX')
P.append('Tool="Compiler"')
P.append('Tool="CustomBuilder"')
P.append('Tool="DspBiosBuilder"')
P.append('Tool="Linker"')
P.append('Config="Debug"')
P.append('Config="Release"')
P.append('')
P.append('[Source Files]')
for s in ordered:
    P.append('Source="..\\%s"' % s.replace('/', '\\'))
P.append('Source="..\\linker\\DSP28035Peripheral.cmd"')
P.append('Source="..\\linker\\F28035.cmd"')
P.append('')
P.append('["Compiler" Settings: "Debug"]')
P.append('Options=%s' % CDEBUG)
P.append('')
P.append('["Compiler" Settings: "Release"]')
P.append('Options=%s' % CRELEASE)
P.append('')
P.append('["Linker" Settings: "Debug"]')
P.append('Options=%s' % LD)
P.append('')
P.append('["Linker" Settings: "Release"]')
P.append('Options=%s' % LR)
P.append('')
P.append('["..\\linker\\DSP28035Peripheral.cmd" Settings: "Debug"]')
P.append('LinkOrder=1')
P.append('')
P.append('["..\\linker\\DSP28035Peripheral.cmd" Settings: "Release"]')
P.append('LinkOrder=1')
P.append('')
P.append('["..\\linker\\F28035.cmd" Settings: "Debug"]')
P.append('LinkOrder=2')
P.append('')
P.append('["..\\linker\\F28035.cmd" Settings: "Release"]')
P.append('LinkOrder=2')
P.append('')
open(os.path.join(DST, 'project/jt380.pjt'), 'w', encoding='ascii', newline='\r\n').write('\n'.join(P))


def lkf(cfg, linkopts, outdir):
    L = [linkopts]
    L.append('"..\\linker\\DSP28035Peripheral.cmd"')
    L.append('"..\\linker\\F28035.cmd"')
    for s in ordered:
        L.append('"..\\%s"' % (outdir + '\\' + os.path.splitext(os.path.basename(s))[0] + '.obj').replace('/', '\\'))
    return '\r\n'.join(L) + '\r\n'


open(os.path.join(DST, 'project/Debug.lkf'), 'w', encoding='ascii', newline='').write(
    lkf('Debug', LD, 'build/Debug'))
open(os.path.join(DST, 'project/Release.lkf'), 'w', encoding='ascii', newline='').write(
    lkf('Release', LR, 'build/Release'))

# ------------------------------------------------------------------ 骨架目录
for d in ('build/Debug', 'build/Release', 'tests'):
    os.makedirs(os.path.join(DST, d), exist_ok=True)
open(os.path.join(DST, 'build/README.md'), 'w', encoding='utf-8').write(
    '# build\n\n此目录只保存编译器生成的文件 (目标文件 / 映像 / map), 不作为源码提交。\n\n'
    '- `Debug/`   Debug 配置产物\n- `Release/` Release 配置产物\n')

json.dump(dict(ordered=ordered, cdebug=CDEBUG, crelease=CRELEASE, ld=LD, lr=LR),
          open(os.path.join(DOC, '_project.json'), 'w', encoding='utf-8'),
          ensure_ascii=False, indent=1)
print('project files written; sources=%d' % len(ordered))
