/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define DetectZeroPosOnce     ref_DetectZeroPosOnce
#define InitSetPosTune        ref_InitSetPosTune
#define SetIPMPos             ref_SetIPMPos
#define SetIPMPos_ABZRef      ref_SetIPMPos_ABZRef
#define SynCalLabAndLbc       ref_SynCalLabAndLbc
#define SynInitPosDetCal      ref_SynInitPosDetCal
#define SynInitPosDetSetPwm   ref_SynInitPosDetSetPwm
#define SynInitPosDetSetTs    ref_SynInitPosDetSetTs
#define SynInitPosDetect      ref_SynInitPosDetect
#define SynTuneBemf           ref_SynTuneBemf
#define SynTuneInitPos        ref_SynTuneInitPos
#define SynTunePGZero_Load    ref_SynTunePGZero_Load
#define SynTunePGZero_No_Load ref_SynTunePGZero_No_Load
#define gEstBemf              ref_gEstBemf
#define gIPMInitPos           ref_gIPMInitPos
#define gIPMZero              ref_gIPMZero
#define gPmParEst             ref_gPmParEst

#include "MotorPmsmParEst_ref.c"
