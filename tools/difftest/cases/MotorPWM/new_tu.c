/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define DeadBandComp new_DeadBandComp
#define EPWMFlagU    new_EPWMFlagU
#define EPWMFlagV    new_EPWMFlagV
#define EPWMFlagW    new_EPWMFlagW
#define InitSetPWM   new_InitSetPWM
#define OutPutPWMVC  new_OutPutPWMVC
#define OutPutPWMVF  new_OutPutPWMVF
#define SendPWM      new_SendPWM
#define gOutVolt     new_gOutVolt
#define gPWM         new_gPWM
#define gRatio       new_gRatio
#define iTrig1_Cpwm  new_iTrig1_Cpwm
#define iTrig1_Dpwm  new_iTrig1_Dpwm
#define iTrig2_Pwm   new_iTrig2_Pwm
#define iTrig3_Cpwm  new_iTrig3_Cpwm
#define iTrig3_Dpwm  new_iTrig3_Dpwm

#include "MotorPWM.c"
