"""宿主编译环境封装 (MSVC 2022 x64)。

用途: 把「原实现」和「改写后的实现」各自编译成宿主可执行文件, 用同一组输入驱动,
逐字段比对全部状态。这是"实现被改动"之后唯一还能给出的行为等价证据。

注意: 本文件里的路径与开关都是环境事实, 不是配置项。
"""

import os
import subprocess
import sys

MSVC = r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207'
SDK = r'C:\Program Files (x86)\Windows Kits\10'
SDKV = '10.0.26100.0'
CL = os.path.join(MSVC, r'bin\Hostx64\x64\cl.exe')


def host_env():
    """构造 cl.exe 需要的 PATH/INCLUDE/LIB。

    不能用 vcvars64.bat —— 本机 cmd.exe 被沙箱拦截, 只能手工拼环境变量。
    """
    env = dict(os.environ)
    env['PATH'] = ';'.join([
        os.path.join(MSVC, r'bin\Hostx64\x64'),
        os.path.join(SDK, r'bin', SDKV, r'x64'),
        env.get('PATH', ''),
    ])
    env['INCLUDE'] = ';'.join([
        os.path.join(MSVC, 'include'),
        os.path.join(SDK, r'Include', SDKV, 'ucrt'),
        os.path.join(SDK, r'Include', SDKV, 'shared'),
        os.path.join(SDK, r'Include', SDKV, 'um'),
    ])
    env['LIB'] = ';'.join([
        os.path.join(MSVC, r'lib\x64'),
        os.path.join(SDK, r'Lib', SDKV, r'ucrt', 'x64'),
        os.path.join(SDK, r'Lib', SDKV, r'um', 'x64'),
    ])
    return env


def compile_exe(sources, exe, incdirs=(), defines=(), workdir=None, extra=()):
    """编译并链接出可执行文件。返回 (rc, stdout+stderr)。

    用 /Od 关闭优化: 目标端(C28x)有符号溢出按补码回绕, 宿主端不希望优化器
    基于"有符号溢出是 UB"做任何推断, 保持两边都是朴素的补码运算。
    """
    args = [CL, '/nologo', '/TC', '/Od', '/W3', '/wd4996', '/wd4100', '/wd4189']
    for d in defines:
        args.append('/D' + d)
    for i in incdirs:
        args.append('/I' + os.path.abspath(i))
    args += list(extra)
    args += [os.path.abspath(s) for s in sources]
    args.append('/Fe:' + os.path.abspath(exe))
    args.append('/Fo:' + os.path.abspath(exe) + '.objdir\\')
    os.makedirs(os.path.abspath(exe) + '.objdir', exist_ok=True)

    r = subprocess.run(args, env=host_env(), cwd=workdir or os.getcwd(),
                       capture_output=True, text=True, errors='replace')
    return r.returncode, (r.stdout or '') + (r.stderr or '')


# 变体超时: 被测代码里有几处**可能不终止**的循环。真固件同样会挂, 但一个不终止的
# 变体会把整轮负向对照**永久**挂住 —— 实测 f_menu 的 M132
# (`NoSameDeal` 的 `data -= 1` 改成 `data -= 2`) 挂了 16 分钟以上: 跨度是奇数时
# data 每次减 2, 永远碰不到 lower, 外层循环转不出来。当时 `ps` 里只看到一个 exe,
# 很容易被误读成"机器慢"而不是"死循环"。
#
# 300 s 是实测值 (~50 s/变体) 的 6 倍余量, 4 个 worker 抢 4 个核时也不会误报。
EXE_TIMEOUT = 300.0


class ExeTimeout(Exception):
    """可执行文件在超时内没有结束。

    这里**刻意**不用 SystemExit。原因: "没跑起来"和"跑起来了但没跑完"是两回事。

      * 编译/链接失败 -> 变体一行都没执行 -> 不构成任何证据 (negative_control
        记 BROKEN, 总账必须失败);
      * 跑起来了但不终止 -> **终止性本身就是语义**。基准侧在同一个输入上能跑完、
        变异侧跑不完, 这已经是差异, 只是差分测试台没法用"比字段"的方式表达它。

    所以超时要带出**已经产出的 stdout**: 那是唯一的现场。丢掉它, 就等于把
    "变异侧挂在半路, 但挂之前已经跑出 N 处字段不一致"降级成"什么都没看到"。
    f_menu 的 M134 (`data == lower` 改 `data != lower`) 正是这种情形 ——
    它先跑出差异、之后才挂, 现场在缓冲区里, 必须冲出来才拿得到。
    """

    def __init__(self, exe, timeout, stdout, stderr):
        self.exe = exe
        self.timeout = timeout
        self.stdout = stdout
        self.stderr = stderr
        super(ExeTimeout, self).__init__(
            '%s 运行超过 %gs 仍未结束 (疑似死循环), 已终止'
            % (os.path.basename(exe), timeout))


def run_exe(exe, cwd=None, args=(), timeout=EXE_TIMEOUT):
    try:
        r = subprocess.run([os.path.abspath(exe)] + list(args), cwd=cwd,
                           capture_output=True, text=True, errors='replace',
                           timeout=timeout)
    except subprocess.TimeoutExpired as e:
        # text=True 时 e.stdout/e.stderr 一般是 str, 但 Python 在部分路径上给
        # bytes —— 两种都收, 免得现场因为一个解码细节丢掉。
        def _txt(v):
            if v is None:
                return ''
            return v.decode('utf-8', 'replace') if isinstance(v, bytes) else v
        raise ExeTimeout(exe, timeout, _txt(e.stdout), _txt(e.stderr))
    return r.returncode, (r.stdout or '') + (r.stderr or '')


if __name__ == '__main__':
    print('CL =', CL)
    print('exists =', os.path.isfile(CL))
    sys.exit(0 if os.path.isfile(CL) else 1)
