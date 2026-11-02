/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define DisableDog           ref_DisableDog
#define EnableDog            ref_EnableDog
#define InitFlash            ref_InitFlash
#define InitInterrupt        ref_InitInterrupt
#define InitPeripheralClocks ref_InitPeripheralClocks
#define InitPeripherals      ref_InitPeripherals
#define InitPieCtrl          ref_InitPieCtrl
#define InitPieVectTable     ref_InitPieVectTable
#define InitPll              ref_InitPll
#define InitSetAdc           ref_InitSetAdc
#define InitSetGpio          ref_InitSetGpio
#define InitSysCtrl          ref_InitSysCtrl
#define KickDog              ref_KickDog
#define SetInterruptEnable   ref_SetInterruptEnable
#define copy_prg             ref_copy_prg
#define gErrorIntCnt         ref_gErrorIntCnt
#define rsvd_ISR             ref_rsvd_ISR

#include "mainInit_2803x_ref.c"
