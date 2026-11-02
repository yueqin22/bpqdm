"""主机侧测试公共工具: 加载 tools/ 下的脚本并取其产物。"""
import os, sys, json, runpy

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOC = os.path.join(ROOT, 'docs')
TOOLS = os.path.join(ROOT, 'tools')
sys.path.insert(0, TOOLS)


def load(name):
    return json.load(open(os.path.join(DOC, name), encoding='utf-8'))


def run_tool(name):
    return runpy.run_path(os.path.join(TOOLS, name))


class Checker:
    def __init__(self, title):
        self.title = title
        self.fails = []
        self.n = 0

    def check(self, ok, desc):
        self.n += 1
        print('  [%s] %s' % ('PASS' if ok else 'FAIL', desc))
        if not ok:
            self.fails.append(desc)
        return ok

    def done(self):
        print('%s: %d 项, 失败 %d' % (self.title, self.n, len(self.fails)))
        return not self.fails
