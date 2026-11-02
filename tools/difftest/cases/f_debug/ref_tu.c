/* ref translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * 基准实现 = 冻结的改写前版本 (与 bpqym 原工程 token 等价)。
 */

#define ChgAttr            ref_ChgAttr
#define DealKeyDebug       ref_DealKeyDebug
#define I2CWrongTimer      ref_I2CWrongTimer
#define InitDebug          ref_InitDebug
#define InitI2cDebug       ref_InitI2cDebug
#define InitKeyDebug       ref_InitKeyDebug
#define PowerOnDebug       ref_PowerOnDebug
#define TestI2cDebug       ref_TestI2cDebug
#define UniversalDebug     ref_UniversalDebug
#define UniversalDebug32   ref_UniversalDebug32
#define i                  ref_i
#define keyValueDebug      ref_keyValueDebug
#define keyValueDebugIndex ref_keyValueDebugIndex
#define matlabDebug        ref_matlabDebug
#define matlabDebugRsvd    ref_matlabDebugRsvd
#define rcvDataI2C         ref_rcvDataI2C
#define scope0             ref_scope0
#define scope1             ref_scope1
#define scope2             ref_scope2
#define scope3             ref_scope3
#define scope4             ref_scope4
#define scope5             ref_scope5
#define scopeIndex         ref_scopeIndex
#define scopeInterval      ref_scopeInterval
#define sendDataI2C        ref_sendDataI2C
#define vDL                ref_vDL

#include "f_debug_ref.c"
