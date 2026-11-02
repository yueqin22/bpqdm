/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define DetectZeroPosOnce     new_DetectZeroPosOnce
#define InitSetPosTune        new_InitSetPosTune
#define SetIPMPos             new_SetIPMPos
#define SetIPMPos_ABZRef      new_SetIPMPos_ABZRef
#define SynCalLabAndLbc       new_SynCalLabAndLbc
#define SynInitPosDetCal      new_SynInitPosDetCal
#define SynInitPosDetSetPwm   new_SynInitPosDetSetPwm
#define SynInitPosDetSetTs    new_SynInitPosDetSetTs
#define SynInitPosDetect      new_SynInitPosDetect
#define SynTuneBemf           new_SynTuneBemf
#define SynTuneInitPos        new_SynTuneInitPos
#define SynTunePGZero_Load    new_SynTunePGZero_Load
#define SynTunePGZero_No_Load new_SynTunePGZero_No_Load
#define gEstBemf              new_gEstBemf
#define gIPMInitPos           new_gIPMInitPos
#define gIPMZero              new_gIPMZero
#define gPmParEst             new_gPmParEst

#include "MotorPmsmParEst.c"
