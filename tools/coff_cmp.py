"""TI-COFF 目标文件内容级比对 (路径无关)。

为什么需要它:
  * dis2000 只能比对含 .text 的模块。纯数据模块 (DSP2803x_GlobalVariableDefs,
    MotorVar) 没有指令, 反汇编产物只有文件头, 无法用 "空 == 空" 充当证据。
  * 直接 cmp .obj 会被两处与语义无关的差异污染:
      1) COFF 文件头 4 字节时间戳 (编译时刻不同);
      2) 字符串表/调试段里内嵌的源文件绝对路径。新工程换了目录, 路径长度必然
         不同, 会让 symptr、字符串表偏移、长符号名的 string-table 偏移整体平移,
         进而让 naive 的 cmp 报出上万条 "差异"。

做法: 段布局交给厂商解析器 ofd2000 -v, 内容用原始字节做 SHA256。
  * 逐个段 (跳过 .debug_*): 名字 / 大小 / 是否装载 / 标志 / 重定位条数
    + 段内容 SHA256 + 重定位区 SHA256
  * 符号表 (18 字节/项, 跳过 aux, 长名走字符串表): 名字 / value / scnum / type / sclass
  * 跳过 .file 符号 (其 aux 里存的就是源文件路径, 与语义无关)
忽略: 时间戳、文件内偏移、字符串表本身。

判据: canonical 摘要完全相同 => 该模块对最终映像的贡献逐字节一致。

经验数据 (TI-COFF v2, C28x):
  * 符号表项 = 18 字节;  重定位项 = 12 字节
  * 字符串表紧跟在 符号表起点 + nsyms*18 之后
  * ofd2000 -v 里 "Loaded Onto Device: Yes" 的段才进映像
"""
import os, re, sys, json, hashlib, subprocess

CGT = 'E:/ti/c2000/cgt-c2000_25.11.1.LTS/ti-cgt-c2000_25.11.1.LTS/bin'
SYM_ENT = 18
REL_ENT = 12
SKIP_SEC = ('.debug', '.TI.')
SKIP_SYM = ('.file', 'file')


SEC_HDR = re.compile(r'\s*<\d+>\s+"(.*)"\s*$')
BLOCK_END = re.compile(r'\s*(Relocation Table|Symbol Table|String Table|Build Attributes)\b')


def _fld(line, label):
    """取 ofd2000 输出里 '<label>:' 后面的值。

    字段可能落在行首, 也可能落在**第二列**, 所以标签前面允许是行首或两个以上空格。
    这个 "两个以上空格" 很关键: `Address Unit Size:` 里的 `Size:` 只隔一个空格,
    因此不会被当成 `Size:` 命中。
    """
    m = re.search(r'(?:^|\s{2,})' + re.escape(label) + r':\s*(\S+)', line)
    return m.group(1) if m else None


def ofd_sections(path):
    """用厂商解析器取段清单 (名字/大小/地址单元/装载/偏移/重定位)。

    两个必须记住的格式事实:
      * 段清单是**两列**排布, 一行两个字段:
            Loaded Onto Device:  No          Address Unit Size:  8 bits
        所以每行都要把所有字段都取出来, 不能在第一个字段命中后就 continue ——
        早期版本正是这样漏掉了 Address Unit Size 和 # Relocs 的。
      * 段清单在 " Relocation Table" 处结束; 之后的符号表里也有 `# Relocs:` 行,
        若不切断就会把符号表的数字写到最后一个段的头上。
    """
    d = os.path.dirname(os.path.abspath(path))
    # 必须带 timeout。ofd2000 曾经出现过"只占 0.09 s CPU 就再也不返回"的挂死
    # (既不读盘也不占 CPU, 卡在一个句柄上), 而不带 timeout 的 subprocess.run
    # 会让整个构建比对永久停在这一行, 外面看起来只是"没有输出"。
    # 超时抛异常, 由调用方记成 ERROR, 至少能定位到是哪个模块。
    try:
        r = subprocess.run([os.path.join(CGT, 'ofd2000.exe'), '-v',
                            os.path.basename(path)],
                           cwd=d, capture_output=True, text=True,
                           errors='replace', timeout=120)
    except subprocess.TimeoutExpired:
        raise RuntimeError('ofd2000 超时 (>120s): %s' % path)
    txt = (r.stdout or '') + (r.stderr or '')
    if not txt.strip():
        raise RuntimeError('ofd2000 输出为空: %s (empty == empty 不算证据)' % path)
    secs = []
    cur = None
    for line in txt.split('\n'):
        # 段清单结束后必须**彻底停手**, 不能只把 cur 置空: 符号表里的
        # `    <0> "f_common.c"` 同样符合段头形状, 会把 cur 重新点着, 于是符号表的
        # `Size: 25bits` 被当成段大小。注意 "Object File Information" 块里也有
        # `Symbol Table File Offset:` 这种行, 所以只有已经读到过段之后才认这个终止标记。
        if secs and BLOCK_END.match(line):
            break
        m = SEC_HDR.match(line)
        if m:
            cur = dict(name=m.group(1), size=0, unit=0, loaded=False, off=0,
                       nreloc=0, reloff=0, flags=0)
            secs.append(cur)
            continue
        if cur is None:
            continue
        v = _fld(line, 'Size')
        if v:
            cur['size'] = int(v, 0)
        v = _fld(line, 'Address Unit Size')
        if v:
            cur['unit'] = int(v, 0)
        v = _fld(line, 'Loaded Onto Device')
        if v:
            cur['loaded'] = v.lower() == 'yes'
        v = _fld(line, 'File Offset')
        if v:
            cur['off'] = int(v, 0)
        v = _fld(line, 'Reloc File Offset')
        if v:
            cur['reloff'] = int(v, 0)
        v = _fld(line, '# Relocs')
        if v:
            cur['nreloc'] = int(v)
        v = _fld(line, 'TI-COFF s_flags')
        if v:
            cur['flags'] = int(v, 0)
    return txt, secs


def ofd_nsyms(txt):
    m = re.search(r'Number of Symbols:\s*(\d+)', txt)
    if m:
        return int(m.group(1))
    return None


def _name_from(b, strtab):
    if b[:4] == b'\x00\x00\x00\x00':
        import struct
        o = struct.unpack_from('<I', b, 4)[0]
        return strtab[o - 4:].split(b'\x00', 1)[0].decode('latin-1') if o >= 4 else ''
    return b.split(b'\x00', 1)[0].decode('latin-1')


def canonical(path):
    raw = open(path, 'rb').read()
    txt, secs = ofd_sections(path)
    if not secs:
        raise RuntimeError('未能解析段清单: %s' % path)

    out_secs = []
    for s in secs:
        if s['name'].startswith(SKIP_SEC):
            continue
        if not s['unit']:
            raise RuntimeError('段 %s 缺 Address Unit Size, 无法确定字节长度' % s['name'])
        nbytes = s['size'] * s['unit'] // 8
        # off == 0 表示该段未初始化 (.bss/.ebss/寄存器文件等), 文件里没有数据;
        # 此时内容按 nbytes 个 0 字节对待。绝不能拿 raw[0:nbytes] (那是文件头)。
        if s['size'] == 0:
            data = b''
        elif s['off'] == 0:
            data = b'\x00' * nbytes
        else:
            data = raw[s['off']: s['off'] + nbytes]
            if len(data) != nbytes:
                raise RuntimeError('段 %s 越界 (off=%d nbytes=%d)' % (s['name'], s['off'], nbytes))
        rel = b''
        if s['nreloc']:
            rel = raw[s['reloff']: s['reloff'] + s['nreloc'] * REL_ENT]
            if len(rel) != s['nreloc'] * REL_ENT:
                raise RuntimeError('段 %s 重定位区越界' % s['name'])
        out_secs.append(dict(
            name=s['name'], size=s['size'], nbytes=nbytes, unit=s['unit'],
            loaded=s['loaded'], flags=s['flags'],
            init=(s['size'] > 0 and s['off'] != 0),
            nreloc=s['nreloc'],
            data=hashlib.sha256(data).hexdigest()[:32],
            reloc=hashlib.sha256(rel).hexdigest()[:32] if rel else '-'))

    # ---- 符号表 ----
    import struct
    magic, nscns = struct.unpack_from('<HH', raw, 0)
    timdat, symptr, nsyms, opthdr, flags = struct.unpack_from('<IIIHH', raw, 4)
    strtab = b''
    if nsyms:
        st = symptr + nsyms * SYM_ENT
        ln = struct.unpack_from('<I', raw, st)[0]
        strtab = raw[st + 4: st + ln]

    syms = []
    i = 0
    while i < nsyms:
        b = raw[symptr + i * SYM_ENT: symptr + (i + 1) * SYM_ENT]
        if len(b) < SYM_ENT:
            break
        nm = _name_from(b[0:8], strtab)
        value, scnum, typ, sclass, naux = struct.unpack_from('<IhHBB', b, 8)
        if sclass != 0 and nm and not nm.startswith(SKIP_SYM):
            syms.append((nm, value, scnum, typ, sclass))
        i += 1 + naux

    return dict(sections=out_secs, symbols=syms,
                nsyms_raw=nsyms, nscns=nscns, magic=magic)


def compare(pa, pb):
    ca, cb = canonical(pa), canonical(pb)
    if ca == cb:
        return True, dict(nsec=len(ca['sections']), nsym=len(ca['symbols']),
                          loaded=sum(1 for s in ca['sections'] if s['loaded']),
                          bytes=sum(s['nbytes'] for s in ca['sections'] if s['loaded']))
    msgs = []
    sa, sb = ca['sections'], cb['sections']
    if len(sa) != len(sb):
        msgs.append('段数 %d vs %d' % (len(sa), len(sb)))
    for x, y in zip(sa, sb):
        if x != y:
            msgs.append('段 %s: size=%s/%s data=%s/%s reloc=%s/%s nreloc=%s/%s' % (
                x['name'], x['size'], y['size'], x['data'][:8], y['data'][:8],
                x['reloc'][:8], y['reloc'][:8], x['nreloc'], y['nreloc']))
    ta, tb = ca['symbols'], cb['symbols']
    if ta != tb:
        oa = [s for s in ta if s not in tb][:4]
        ob = [s for s in tb if s not in ta][:4]
        msgs.append('符号表差异: 仅A=%r 仅B=%r (共 %d/%d)' % (oa, ob, len(ta), len(tb)))
    return False, dict(msgs=msgs[:8])


if __name__ == '__main__':
    ok, info = compare(sys.argv[1], sys.argv[2])
    print(('IDENTICAL ' if ok else 'DIFFERS   ') + json.dumps(info, ensure_ascii=False))
    sys.exit(0 if ok else 1)
