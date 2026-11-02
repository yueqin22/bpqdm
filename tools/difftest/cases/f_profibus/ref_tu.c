/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define DPDataNum                ref_DPDataNum
#define DPRcvTest                ref_DPRcvTest
#define ProfibusCommErrCheck     ref_ProfibusCommErrCheck
#define ProfibusRcvDataDeal      ref_ProfibusRcvDataDeal
#define ProfibusSendDataDeal     ref_ProfibusSendDataDeal
#define ProfibusStartDeal        ref_ProfibusStartDeal
#define UpdateProfibusCommFormat ref_UpdateProfibusCommFormat
#define dpControlCmd             ref_dpControlCmd
#define dpControlCmdBak          ref_dpControlCmdBak
#define dpFrqAim                 ref_dpFrqAim
#define dpStatus                 ref_dpStatus
#define getDPControlCmd          ref_getDPControlCmd
#define rcvRigthFlag             ref_rcvRigthFlag
#define saveEepromFlag           ref_saveEepromFlag
#define saveEepromIndex          ref_saveEepromIndex
#define setDPStatusInfo          ref_setDPStatusInfo

#include "f_profibus_ref.c"
