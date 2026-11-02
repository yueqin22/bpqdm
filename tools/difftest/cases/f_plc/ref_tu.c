/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define FuncCodeRwAtrrib ref_FuncCodeRwAtrrib
#define InvRecFastTab    ref_InvRecFastTab
#define InvRecSlowTab    ref_InvRecSlowTab
#define InvRecTabCfg     ref_InvRecTabCfg
#define InvTrFastIndex   ref_InvTrFastIndex
#define InvTrFastTab     ref_InvTrFastTab
#define InvTrSlowIndex   ref_InvTrSlowIndex
#define InvTrSlowTab     ref_InvTrSlowTab
#define InvTranTabCfg    ref_InvTranTabCfg
#define PlcDataFramDeal  ref_PlcDataFramDeal

#include "f_plc_ref.c"
