/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define GetCurExcursion ref_GetCurExcursion
#define GetCurrentInfo  ref_GetCurrentInfo
#define GetUDCInfo      ref_GetUDCInfo
#define gADC            ref_gADC
#define gAI             ref_gAI
#define gCurSamp        ref_gCurSamp
#define gExcursionInfo  ref_gExcursionInfo
#define gIAlphBeta      ref_gIAlphBeta
#define gIAlphBetaQ12   ref_gIAlphBetaQ12
#define gIAmpTheta      ref_gIAmpTheta
#define gIMTQ12         ref_gIMTQ12
#define gIMTQ24         ref_gIMTQ24
#define gIMTSetQ12      ref_gIMTSetQ12
#define gIUVWQ12        ref_gIUVWQ12
#define gIUVWQ24        ref_gIUVWQ24
#define gLineCur        ref_gLineCur
#define gTemperature    ref_gTemperature
#define gUDC            ref_gUDC

#include "MotorInfoCollect_ref.c"
