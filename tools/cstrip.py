"""词法级注释清理 + token 流等价校验 (C / C28x 汇编 / 链接脚本)。

设计要点 (见 docs/rewrite_decisions.md):
  * 逐文件探测编码, 混用编码用 utf-8 + surrogateescape 严格往返。
  * 只删注释与空白, 不改动任何 token。
  * `/*...*/` 替换为一个空格 + 注释内原有换行 (避免 token 粘连)。
  * `//...` 删到行尾, 保留换行; 行尾反斜杠触发 C 行拼接, 注释吞掉下一行。
  * C28x 汇编: `;` 行注释 与 列首 `*` 注释。
"""
import os

ENCODINGS = ('utf-8', 'gbk', 'gb18030')


def decode_any(raw):
    for enc in ENCODINGS:
        try:
            return raw.decode(enc), enc
        except UnicodeDecodeError:
            pass
    return raw.decode('utf-8', 'surrogateescape'), 'utf-8+surrogateescape'


def encode_back(text, enc):
    if enc == 'utf-8+surrogateescape':
        return text.encode('utf-8', 'surrogateescape')
    return text.encode(enc)


def eol_of(text):
    crlf = text.count('\r\n')
    lf = text.count('\n') - crlf
    if crlf and lf:
        return 'mixed'
    return 'CRLF' if crlf else 'LF'


# --------------------------------------------------------------------- C
def strip_c(text):
    """删除 C 注释。返回 (新文本, 删除的注释条数)。"""
    out = []
    i, n = 0, len(text)
    removed = 0
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            q = c
            out.append(c)
            i += 1
            while i < n:
                if text[i] == '\\' and i + 1 < n:
                    out.append(text[i:i + 2])
                    i += 2
                    continue
                out.append(text[i])
                if text[i] == q:
                    i += 1
                    break
                i += 1
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            j = text.find('*/', i + 2)
            if j < 0:
                j = n - 2
            body = text[i:j + 2]
            out.append(' ')
            out.append('\n' * body.count('\n'))
            removed += 1
            i = j + 2
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            j = i + 2
            while j < n:
                if text[j] == '\\' and j + 1 < n and text[j + 1] == '\n':
                    j += 2
                    continue
                if text[j] == '\\' and j + 2 < n and text[j + 1] == '\r' and text[j + 2] == '\n':
                    j += 3
                    continue
                if text[j] == '\n':
                    break
                j += 1
            # 保留行终止符 (\r 若存在则属于终止符)
            k = j
            while k > i + 2 and text[k - 1] == '\r':
                k -= 1
            out.append(text[k:j])
            removed += 1
            i = j
            continue
        out.append(c)
        i += 1
    return ''.join(out), removed


# ---------------------------------------------------------------- C28x asm
def strip_asm(text):
    out = []
    removed = 0
    for line in text.splitlines(keepends=True):
        body = line.rstrip('\r\n')
        term = line[len(body):]
        stripped = body.lstrip()
        if stripped.startswith('*'):
            out.append(term)
            removed += 1
            continue
        # `;` 行注释 (注意: 汇编中 `;` 后可能紧跟 `;`)
        idx = body.find(';')
        if idx >= 0:
            # 双引号字符串保护 (本工程汇编未使用, 保守处理)
            inq = False
            for p in range(idx):
                if body[p] == '"':
                    inq = not inq
            if not inq:
                body = body[:idx]
                removed += 1
        out.append(body.rstrip() + term)
    return ''.join(out), removed


# ---------------------------------------------------------------- linker cmd
def strip_cmd(text):
    return strip_c(text)


# ---------------------------------------------------------------- tokenizer
def c_tokens(text):
    """真 C 词法器: 字符串/字符字面量算一个 token, 忽略注释与空白。"""
    toks = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c in ' \t\r\n\f\v':
            i += 1
            continue
        if c == '/' and i + 1 < n and text[i + 1] in '*/':
            if text[i + 1] == '*':
                j = text.find('*/', i + 2)
                i = (n if j < 0 else j + 2)
            else:
                j = i + 2
                while j < n and text[j] != '\n':
                    j += 1
                i = j
            continue
        if c == '"' or c == "'":
            q = c
            j = i + 1
            buf = [c]
            while j < n:
                if text[j] == '\\' and j + 1 < n:
                    buf.append(text[j:j + 2])
                    j += 2
                    continue
                buf.append(text[j])
                if text[j] == q:
                    j += 1
                    break
                j += 1
            toks.append(''.join(buf))
            i = j
            continue
        if c.isalpha() or c == '_':
            j = i
            while j < n and (text[j].isalnum() or text[j] == '_'):
                j += 1
            toks.append(text[i:j])
            i = j
            continue
        if c.isdigit() or (c == '.' and i + 1 < n and text[i + 1].isdigit()):
            j = i
            while j < n and (text[j].isalnum() or text[j] in '._'):
                j += 1
            toks.append(text[i:j])
            i = j
            continue
        toks.append(c)
        i += 1
    return toks


def tidy(text, eol='LF'):
    """去掉行尾空白, 连续空行折叠为 1, 末尾保留一个换行; 按 eol 还原行尾。

    eol 取 'LF' / 'CRLF' (与 eol_of 的返回值一致)。
    """
    sep = '\r\n' if eol == 'CRLF' else '\n'
    lines = text.replace('\r\n', '\n').split('\n')
    res = []
    blank = 0
    for ln in lines:
        s = ln.rstrip()
        if s == '':
            blank += 1
            if blank > 1:
                continue
        else:
            blank = 0
        res.append(s)
    while res and res[-1] == '':
        res.pop()
    return sep.join(res) + sep


# ---------------------------------------------------------------- 交付树扫描范围
# build/ 是生成物目录 (里面有 .obj/.out/.hex, 还有 tools/build_baseline.py 放进去的
# 原工程只读副本 baseline_tree)。它**绝不能**参与 "注释零残留 / 纯 ASCII" 这类
# 针对交付源码的验收扫描, 否则会拿原工程的带注释、含中文的源码去判定失败。
SKIP_TOP_DIRS = {'build', '.workbuddy-ai', '.git', '__pycache__'}
# 交付源码所在的一级目录
SRC_TOP_DIRS = ('src', 'include', 'asm', 'linker', 'project', 'tests', 'tools', 'docs')


def walk_delivered(root):
    """遍历交付树中属于 "源码/工程/文档" 的文件, 跳过 build/ 等生成物目录。

    返回相对 root 的 posix 风格路径列表。
    """
    out = []
    for base in SRC_TOP_DIRS:
        top = os.path.join(root, base)
        if not os.path.isdir(top):
            continue
        for r, dirs, fs in os.walk(top):
            dirs[:] = [d for d in dirs if d not in SKIP_TOP_DIRS]
            for fn in fs:
                p = os.path.join(r, fn)
                out.append(os.path.relpath(p, root).replace('\\', '/'))
    # 根目录下的散文件 (README.md 等)
    for fn in os.listdir(root):
        p = os.path.join(root, fn)
        if os.path.isfile(p):
            out.append(fn)
    return sorted(out)
