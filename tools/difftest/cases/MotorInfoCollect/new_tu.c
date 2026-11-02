/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define GetCurExcursion new_GetCurExcursion
#define GetCurrentInfo  new_GetCurrentInfo
#define GetUDCInfo      new_GetUDCInfo
#define gADC            new_gADC
#define gAI             new_gAI
#define gCurSamp        new_gCurSamp
#define gExcursionInfo  new_gExcursionInfo
#define gIAlphBeta      new_gIAlphBeta
#define gIAlphBetaQ12   new_gIAlphBetaQ12
#define gIAmpTheta      new_gIAmpTheta
#define gIMTQ12         new_gIMTQ12
#define gIMTQ24         new_gIMTQ24
#define gIMTSetQ12      new_gIMTSetQ12
#define gIUVWQ12        new_gIUVWQ12
#define gIUVWQ24        new_gIUVWQ24
#define gLineCur        new_gLineCur
#define gTemperature    new_gTemperature
#define gUDC            new_gUDC

#include "MotorInfoCollect.c"
