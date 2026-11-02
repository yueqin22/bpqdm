/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define ResetSVC             ref_ResetSVC
#define SVCCalFlux_380       ref_SVCCalFlux_380
#define SVCCalRotorSpeed     ref_SVCCalRotorSpeed
#define SVCCalRotorSpeed_380 ref_SVCCalRotorSpeed_380
#define gABFluxS_LowFilter   ref_gABFluxS_LowFilter
#define gABFluxS_comp        ref_gABFluxS_comp
#define gCalFlux             ref_gCalFlux
#define gFluxR               ref_gFluxR
#define gFluxS               ref_gFluxS
#define gIMTQ12_obs          ref_gIMTQ12_obs
#define gIMTQ24_obs          ref_gIMTQ24_obs
#define gSVCSpeed            ref_gSVCSpeed

#include "MotorSVC_ref.c"
