/* ref translation unit -- 基准侧。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

/* ------------------------------------------------------------------------- *
 * char -> short: 复刻 C28x 的 16 位 char
 *
 * f_osc.h 里 `typedef unsigned char uint8;`。C28x 上 char 是 16 位, 所以
 * `uint8` 是 16 位无符号量, `uint8 OscConFrameBuf[10]` 占 10 个字。
 * 模块自己依赖这个事实:
 *
 *     CrcValueByteCalc((const Uint16 *)OscConFrameBuf, FC_FRAME_LEN-2)
 *
 * 把字节数组当字数组重解释, 读 6 个元素。目标端读的是这 10 个字里的前 6 个;
 * 宿主若用 8 位 char, 同样 6 个元素会读 12 个字节, 越过 10 字节的数组尾部 2
 * 字节 —— CRC 于是取决于数组后面恰好放了什么, 而 ref / new 两个 TU 里"后面
 * 恰好放什么"没有理由相同。那会造出假的失配, 更糟的是掩盖真的失配。
 *
 * 把 char 定义成 short 之后 `uint8` 变成 16 位, 与目标端逐位一致 (包括赋值
 * 不截断这一点)。<stdint.h> / <stddef.h> 必须在替换之前包含完 —— 否则
 * glibc 的 `typedef signed char int8_t;` 会被改成 signed short, 把 int8_t
 * 也一起弄坏。
 *
 * driver.c 不做这个替换 (它要用真正的 char 跑 printf), 所以它读状态时一律
 * 用 shim/osc_state.h 里的显式 `unsigned short` 字段。
 * ------------------------------------------------------------------------- */
#include <stdint.h>
#include <stddef.h>

#define char short

#define OscControlRead   ref_OscControlRead
#define OscControlWriter ref_OscControlWriter
#define OscSciFunction   ref_OscSciFunction

#include "f_osc_ref.c"

/* 静态量在 include 之后就在本 TU 的作用域里, 于是可以取地址、可以快照。
 * 源码一个字都没改 —— 没有把 static 抹掉, 也没有重排声明。 */
#include "osc_state.h"

#define OSC_SIDE ref
#include "osc_state_body.h"
#undef OSC_SIDE
