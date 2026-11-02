/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define DPDataNum                new_DPDataNum
#define DPRcvTest                new_DPRcvTest
#define ProfibusCommErrCheck     new_ProfibusCommErrCheck
#define ProfibusRcvDataDeal      new_ProfibusRcvDataDeal
#define ProfibusSendDataDeal     new_ProfibusSendDataDeal
#define ProfibusStartDeal        new_ProfibusStartDeal
#define UpdateProfibusCommFormat new_UpdateProfibusCommFormat
#define dpControlCmd             new_dpControlCmd
#define dpControlCmdBak          new_dpControlCmdBak
#define dpFrqAim                 new_dpFrqAim
#define dpStatus                 new_dpStatus
#define getDPControlCmd          new_getDPControlCmd
#define rcvRigthFlag             new_rcvRigthFlag
#define saveEepromFlag           new_saveEepromFlag
#define saveEepromIndex          new_saveEepromIndex
#define setDPStatusInfo          new_setDPStatusInfo

#include "f_profibus.c"
