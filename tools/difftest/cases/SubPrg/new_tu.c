/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define BurrFilter      new_BurrFilter
#define PID             new_PID
#define PID32           new_PID32
#define PIDLongRegulate new_PIDLongRegulate
#define SlipFilter      new_SlipFilter
#define atan            new_atan

#include "SubPrg.c"
