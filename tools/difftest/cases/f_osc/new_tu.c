/* new translation unit -- 交付源码侧。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

/* char -> short 的理由与 ref_tu.c 里那段注释相同 (C28x 的 char 是 16 位,
 * uint8 因此是 16 位, `(const Uint16 *)OscConFrameBuf` 那次重解释读的是 6 个
 * 字而不是 12 个字节)。两侧必须用同一个宽度模型, 否则比较的就不是同一件事。 */
#include <stdint.h>
#include <stddef.h>

#define char short

#define OscControlRead   new_OscControlRead
#define OscControlWriter new_OscControlWriter
#define OscSciFunction   new_OscSciFunction

#include "f_osc.c"

#include "osc_state.h"

#define OSC_SIDE new
#include "osc_state_body.h"
#undef OSC_SIDE
