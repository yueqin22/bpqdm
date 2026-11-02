/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define DebugSaveDeal    ref_DebugSaveDeal
#define ResetDebugBuffer ref_ResetDebugBuffer
#define SaveDebugData16  ref_SaveDebugData16
#define SaveDebugData32  ref_SaveDebugData32
#define SaveTmCnt        ref_SaveTmCnt
#define gDebugBuffer     ref_gDebugBuffer
#define gDebugFlag       ref_gDebugFlag
#define gDebugPoint      ref_gDebugPoint
#define pVD1             ref_pVD1
#define pVD2             ref_pVD2
#define pVD3             ref_pVD3
#define pVD4             ref_pVD4
#define saveCurveNum     ref_saveCurveNum
#define saveIndexFunc    ref_saveIndexFunc
#define saveIndexP       ref_saveIndexP
#define saveRstMem       ref_saveRstMem
#define startSave        ref_startSave

#include "MotorDebug_ref.c"
