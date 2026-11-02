/* Case-private host-side DSP2803x_Device.h -- SubPrg 用例。
 *
 * 为什么不用真的 DSP2803x_Device.h:
 *   真文件第 30~31 行有 `extern cregister volatile unsigned int IFR;`
 *   (`cregister` 是 C28x 专有关键字), 第 33~38 行把 EINT/DINT/EALLOW/... 定义成
 *   `asm(" clrc INTM")` 这类内联汇编 —— MSVC 一条都不认。
 *   本模块 (SubPrg.c) 一个外设寄存器都不碰, 但 include 链绕不开它:
 *   SystemDefine.h (真, 由 real.txt 转发) 第 8 行就 include 它。
 *
 * 因此这里只补"让链条编译过去 + 宽度钉死"所必需的东西:
 *
 *   1. DSP28_DATA_TYPES 那组 typedef (真文件第 76~83 行的同款), 宽度按 C28x 钉:
 *        Uint16 = 1 字, Uint32 = 2 字, int16/int32/int64/Uint64/float32/float64
 *      卫哨与真文件同名, 两边都包含也只生效一次。
 *
 *   2. `static int32 __IQsat(...)` —— C28x **编译器内建**, 不是头文件宏。
 *      工程里 include/ 与 src/ 全文只有调用点、没有定义, 而目标端编得过且零告警,
 *      说明它是 cl2000 内建。实测语义 (MotorEncoder / MotorCarrier / MotorInfoCollect
 *      用例里有同样的记录):
 *          MOVB XAR6, #0x64   ; Pos
 *          MOV  PL, #0xff9c   ; Neg
 *          MINL ACC, XAR6     ; ACC = min(ACC, Pos)
 *          MAXL ACC, P        ; ACC = max(ACC, Neg)
 *      即 `__IQsat(A,Pos,Neg) = min(max(A,Neg),Pos)`, 在 **32 位有符号 ACC** 上做,
 *      三个操作数都按 32 位看待。本模块 PID() 与 PID32() 各有一处调用。
 *      用 static 函数而不是宏: 宏会重复求值实参, 内建只求值一次 —— 实参带副作用时
 *      两者不同。形参写成 int32 而不是 long: 本文件末尾会把 `int` 折成 16 位,
 *      这里要的是与 ACC 等宽的 32 位有符号, 写 long 会让"恰好也是 32 位"变成隐式依赖。
 *
 *      这一份是**测试台自己引入的不确定源**, 必须说清楚: 它保证"宿主上 ref/new
 *      两侧调用同一个 __IQsat", 但**没有**证明宿主版与 C28x 内建逐位一致。
 *      目标端那一半由 target_check.py 的符号集合/告警比对覆盖, 覆盖不到饱和点取值。
 *
 *   3. 文件末尾 `#define int short` (与 MotorEncoder 用例同一手法, 理由见那里的
 *      长注释 + 实测方法 build/_widths3.c):
 *          C28x: int = 1 字 (16 位), long = 2 字 (32 位), long long = 4 字
 *          MSVC: int = 32 位,     long = 32 位,     long long = 64 位
 *      —— 只有 `int` 差一个宽度。本模块几乎全部判定都挂在 `int` 上
 *      (PID_STRUCT 的 Max/Min/Deta/KP/KI/KD、CUR_LINE_STRUCT_DEF 的
 *      FilterTime/Input/Output/Data[]、atan 的两个入参), 不钉宽度的话, 宿主上
 *      跑的就**不是**目标机那段行为: ±32767 饱和、`Data[m]>>1` 的回绕点、
 *      atan(x,y) 里 `x<0` 判据的取值范围全部位移。
 *
 *      顺带把 `unsigned int` 变成 `unsigned short` = 16 位无符号 ——
 *      `include/common/MotorDefine.h` 的 `typedef unsigned int Uint;`
 *      于是拿到目标机的宽度 (Uint 正是它)。
 *      `long` 不折: 宿主本来就是 32 位, 与目标机一致。
 *
 *      **为什么是 `short` 而不是 `int16`**: C 的语法里 `unsigned` 只肯和
 *      int/char/short/long 这几个**关键字**搭配, `unsigned <typedef 名>` 不是
 *      合法 C (MSVC 报 C2061/C2059)。`typedef unsigned int16 Uint;` 编不过,
 *      `typedef unsigned short Uint;` 可以。短与 int16_t 同为 16 位, 位宽不变。
 *
 *      这一条会把 `long int` 变成 `long short` (非法) —— 工程里只有
 *      include/motor/SubPrgInclude.h 的 `absl(long int)` / `qatan(long int)`
 *      两处, 由 host_patch.json 换成 `(long)` (语义完全相同)。
 *
 *      放在本文件**末尾**: 上面那组 typedef 已经写完, 之后被 include 的
 *      SystemDefine.h / MotorDefine.h / SubPrgInclude.h 里的 `int` 全部按 16 位
 *      展开; 而 stdint.h 之类在此**之前**就 include 完, 不会被波及。
 */
#ifndef DSP2803X_DEVICE_H
#define DSP2803X_DEVICE_H

#include <stdint.h>

#ifndef DSP28_DATA_TYPES
#define DSP28_DATA_TYPES
typedef int16_t             int16;
typedef int32_t             int32;
typedef int64_t             int64;
typedef uint16_t            Uint16;
typedef uint32_t            Uint32;
typedef uint64_t            Uint64;
typedef float               float32;
typedef double              float64;
#endif

static int32 __IQsat(int32 A, int32 Pos, int32 Neg)
{
    if (A > Pos)
    {
        return Pos;
    }
    if (A < Neg)
    {
        return Neg;
    }
    return A;
}

#define int   short

#endif
