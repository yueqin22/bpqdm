/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define DeadBandComp ref_DeadBandComp
#define EPWMFlagU    ref_EPWMFlagU
#define EPWMFlagV    ref_EPWMFlagV
#define EPWMFlagW    ref_EPWMFlagW
#define InitSetPWM   ref_InitSetPWM
#define OutPutPWMVC  ref_OutPutPWMVC
#define OutPutPWMVF  ref_OutPutPWMVF
#define SendPWM      ref_SendPWM
#define gOutVolt     ref_gOutVolt
#define gPWM         ref_gPWM
#define gRatio       ref_gRatio
#define iTrig1_Cpwm  ref_iTrig1_Cpwm
#define iTrig1_Dpwm  ref_iTrig1_Dpwm
#define iTrig2_Pwm   ref_iTrig2_Pwm
#define iTrig3_Cpwm  ref_iTrig3_Cpwm
#define iTrig3_Dpwm  ref_iTrig3_Dpwm

#include "MotorPWM_ref.c"
