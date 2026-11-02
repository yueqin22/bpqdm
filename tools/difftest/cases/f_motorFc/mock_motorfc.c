/* Shared mock state for the f_motorFc differential test.
 *
 * One instance, read by both the ref and the new side, so both observe exactly
 * the same inputs. Only the symbols the module under test consumes are defined
 * here; the module's own exports (UpdateMotorPara, polePair, motorSnIndexNIncToMotor2)
 * are duplicated per side by ref_tu.c / new_tu.c.
 *
 * funcCode and motorFc use their REAL types from f_funcCode.h, so the word
 * offsets that GetCodeIndex() computes are the ones the project actually uses.
 */

#include "f_main.h"
#include "f_io.h"

FUNCCODE_ALL     funcCode;
struct MOTOR_FC  motorFc;
enum MOTOR_SN    motorSn;
Uint16           errorCode;
struct DI_FUNC   diFunc;
struct DI_FUNC   diSelectFunc;

/* Verbatim from src/function/f_menu.c:45. The real table, not a stand-in: its
 * length is what bounds funcCode.code.frqPoint. */
const int16 decNumber[5] = {1, 10, 100, 1000, 10000};
