/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define InitRecMbox    ref_InitRecMbox
#define InitTranMbox   ref_InitTranMbox
#define InitdspECan    ref_InitdspECan
#define eCanBaud       ref_eCanBaud
#define eCanDataRec    ref_eCanDataRec
#define eCanDataTran   ref_eCanDataTran
#define eCanReEnFlag   ref_eCanReEnFlag
#define eCanTranEnFlag ref_eCanTranEnFlag

#include "f_dspcan_ref.c"
