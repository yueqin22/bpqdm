/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define ResetSVC             new_ResetSVC
#define SVCCalFlux_380       new_SVCCalFlux_380
#define SVCCalRotorSpeed     new_SVCCalRotorSpeed
#define SVCCalRotorSpeed_380 new_SVCCalRotorSpeed_380
#define gABFluxS_LowFilter   new_gABFluxS_LowFilter
#define gABFluxS_comp        new_gABFluxS_comp
#define gCalFlux             new_gCalFlux
#define gFluxR               new_gFluxR
#define gFluxS               new_gFluxS
#define gIMTQ12_obs          new_gIMTQ12_obs
#define gIMTQ24_obs          new_gIMTQ24_obs
#define gSVCSpeed            new_gSVCSpeed

#include "MotorSVC.c"
