/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define ADC_Over_isr   new_ADC_Over_isr
#define EPWM1_TZ_isr   new_EPWM1_TZ_isr
#define EPWM1_zero_isr new_EPWM1_zero_isr
#define main           new_main

#include "main.c"
