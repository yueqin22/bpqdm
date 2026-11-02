/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define DebugSaveDeal    new_DebugSaveDeal
#define ResetDebugBuffer new_ResetDebugBuffer
#define SaveDebugData16  new_SaveDebugData16
#define SaveDebugData32  new_SaveDebugData32
#define SaveTmCnt        new_SaveTmCnt
#define gDebugBuffer     new_gDebugBuffer
#define gDebugFlag       new_gDebugFlag
#define gDebugPoint      new_gDebugPoint
#define pVD1             new_pVD1
#define pVD2             new_pVD2
#define pVD3             new_pVD3
#define pVD4             new_pVD4
#define saveCurveNum     new_saveCurveNum
#define saveIndexFunc    new_saveIndexFunc
#define saveIndexP       new_saveIndexP
#define saveRstMem       new_saveRstMem
#define startSave        new_startSave

#include "MotorDebug.c"
