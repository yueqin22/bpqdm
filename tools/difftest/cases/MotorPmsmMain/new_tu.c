/* new translation unit -- 由 tools/difftest/new_case.py 生成, 可手改。
 *
 * 每个外部链接的文件级符号都必须在这里改名, 否则两个翻译单元在链接期
 * 撞名。清单来自 nm2000 读出的符号表, 不是人眼数的。
 *
 * new 侧 include 的是交付源码, 路径由 srcdir.txt 给出。
 */

#define IPMCalAcrPIDCoff  new_IPMCalAcrPIDCoff
#define IPMCheckInitPos   new_IPMCheckInitPos
#define PG_Zero_isr       new_PG_Zero_isr
#define PmChkInitPosRest  new_PmChkInitPosRest
#define PmDecoupleDeal    new_PmDecoupleDeal
#define PmFluxWeakDeal    new_PmFluxWeakDeal
#define PrepPmsmCsrPrar   new_PrepPmsmCsrPrar
#define RunCaseIpmInitPos new_RunCaseIpmInitPos
#define SynCalLdAndLq     new_SynCalLdAndLq
#define SynCalRealPos     new_SynCalRealPos
#define gFluxWeak         new_gFluxWeak
#define gIPMPos           new_gIPMPos
#define gIPMPosCheck      new_gIPMPosCheck
#define gPMInitPos        new_gPMInitPos
#define gPmDecoup         new_gPmDecoup

#include "MotorPmsmMain.c"
