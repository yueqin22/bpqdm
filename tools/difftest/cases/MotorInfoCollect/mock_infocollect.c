/* mock_infocollect.c -- MotorInfoCollect 用例的外部状态 mock。

 * 本模块自己不定义这些量, 只读写它们 (目标端分别由别的 .c 定义):
 *
 *   AdcResult    外设寄存器块, 目标端由 DSP2803x_GlobalVariableDefs.c 用
 *                `#pragma DATA_SECTION` 钉在固定地址。宿主上给一块普通内存即可 ——
 *                差分测试要的是"同一串操作后寄存器内容一不一样", 不是地址对不对。
 *                SystemDefine.h 的 ADC_IU / ADC_UDC / ADC_TEMP / ADC_AI1..3 全是
 *                这个结构体的字段左移 4 位, 所以这里是本模块**唯一**的硬件入口。
 *
 *   gMainStatus  模块读 RunStep (判低功耗/停机) 并写 StatusWord.bit.RunEnable
 *   gError       模块只读改 ErrorCode.all (置 ERROR_CURRENT_CHECK)
 *   gShortGnd    Filter32 的累加器, 模块读写 ShortCur
 *   gUVCoff      模块只读 UDivV (V 相偏置系数)
 *
 * 结构体全部取自真头文件 (MotorStructDefine.h / MotorInvProtectInclude.h /
 * MotorParaIDinclude.h / MotorPublicCalInclude.h), 不手抄 —— 位域与字段顺序
 * 必须与目标端一致, 否则差分测试测的是另一套布局。
 *
 * 两侧共用这一份: ref 与 new 通过同一块内存写读, 所以任何"少写一个字段"
 * 都会在逐字段比对里现形。
 */
#include "MotorInfoCollectInclude.h"

volatile struct ADC_RESULT_REGS AdcResult;

RUN_STATUS_STRUCT gMainStatus;
struct FAULT_CODE_INFOR_STRUCT_DEF gError;
SHORT_GND_STRUCT gShortGnd;
UV_BIAS_COFF_STRUCT gUVCoff;

/* ---- C28x runtime-library shim: abs -------------------------------------- */

/* The host CRT's `abs` has the WRONG WIDTH, and the wrong width turns it into
 * a silent no-op.
 *
 * The project declares, in SubPrgInclude.h:
 *     extern int abs(int);
 * On C28x `int` is 16 bits, so that is a 16-bit absolute value. On the host the
 * shim's `#define int short` turns the declaration into `extern short
 * abs(short)`, but the definition that gets LINKED is still the CRT's
 * `int abs(int)`: same symbol name, 32-bit parameter. The caller zero-extends
 * the 16-bit argument (`-1` -> 0x0000FFFF), the 32-bit `abs` sees a positive
 * number and returns it unchanged, and truncating back to `short` restores the
 * original negative value. Measured with the project's own declaration
 * (build/_me_absprobe.py): abs(-1) = -1, abs(-5) = -5, abs(-100) = -100.
 *
 * i.e. abs(x) == x for every negative x. Both sides of the differential test
 * were wrong in the same way, so it reported zero mismatches -- the damage is
 * lost coverage, not a false positive. It was found on MotorEncoder, where it
 * made six negative-control mutations invisible (deleting `abs` from
 * `m_UData = abs(gFVCSpeed.MDetaPos)` produced identical state on both sides).
 *
 * This module's abs-bearing comparisons were re-checked after the fix: all of
 * them were already CAUGHT, because each one changes a constant or an operand
 * *around* the abs call rather than removing the call, so a no-op abs could not
 * hide it. The shim is added anyway -- the bench has to model the target, and a
 * no-op abs is the one defect class it would hide.
 *
 * Defining it here overrides the library version: an object-file definition
 * wins over the same symbol in a static library. The body is the C28x RTS
 * semantics with 16-bit wrap -- `-(-32768)` is 32768, which does not fit in a
 * 16-bit int, so it wraps back to -32768. The host computes `-mValue` in 32 bits
 * and truncates on return, landing on the same value. */
int abs(int mValue)
{
    return (mValue < 0) ? -mValue : mValue;
}
