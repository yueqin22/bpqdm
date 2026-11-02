/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define DisableDog           new_DisableDog
#define EnableDog            new_EnableDog
#define InitFlash            new_InitFlash
#define InitInterrupt        new_InitInterrupt
#define InitPeripheralClocks new_InitPeripheralClocks
#define InitPeripherals      new_InitPeripherals
#define InitPieCtrl          new_InitPieCtrl
#define InitPieVectTable     new_InitPieVectTable
#define InitPll              new_InitPll
#define InitSetAdc           new_InitSetAdc
#define InitSetGpio          new_InitSetGpio
#define InitSysCtrl          new_InitSysCtrl
#define KickDog              new_KickDog
#define SetInterruptEnable   new_SetInterruptEnable
#define copy_prg             new_copy_prg
#define gErrorIntCnt         new_gErrorIntCnt
#define rsvd_ISR             new_rsvd_ISR

#include "mainInit_2803x.c"
