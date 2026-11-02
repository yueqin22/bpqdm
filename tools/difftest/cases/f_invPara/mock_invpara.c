/* Shared mock state for the f_invPara differential test.
 *
 * One instance, read by both the ref and the new side, so both observe exactly
 * the same inputs. Only the symbols the module under test *consumes* live here;
 * the module's own exports (invPara, the parameter tables, ...) are duplicated
 * per side by ref_tu.c / new_tu.c.
 *
 * funcCode uses its REAL type from f_funcCode.h. That matters more here than in
 * the other cases: motor1ParaIndex[] and the MOTORn_INDEX_INC_TO_MOTOR1 macros
 * are built out of GetCodeIndex(funcCode.<member>), i.e. word offsets of real
 * struct members, and GetInvParaPointer() indexes through
 * invTypeLimitTable[]/pInvTypeParaTable[] by invPara.volLevel. Stand-in types
 * would produce stand-in offsets and the test would be checking nothing.
 *
 * decNumber is copied verbatim from src/function/f_menu.c:45 -- the real table,
 * not a stand-in: its length is what bounds funcCode.code.frqPoint, and the
 * module multiplies by decNumber[frqPoint].
 */
#include "f_menu.h"
#include "f_eeprom.h"
#include "f_invPara.h"

FUNCCODE_ALL  funcCode;
enum MOTOR_SN motorSn;

const int16 decNumber[5] = {1, 10, 100, 1000, 10000};

/* SaveOneFuncCode(index) is `funcCodeOneWriteIndex[funcCodeOneWriteNum++] = index`.
 * The real array lives in f_eeprom.c; its length is not visible from the header,
 * so this is a deliberately generous stand-in. driver.c asserts the write count
 * never approaches the end, which keeps the stand-in honest: an overflow would
 * otherwise silently corrupt adjacent mock state instead of failing.
 */
#define MOCK_WRITE_INDEX_N 512
Uint16 funcCodeOneWriteIndex[MOCK_WRITE_INDEX_N];
Uint16 funcCodeOneWriteNum;

/* f_interface.h: TUNE_DATA_NUM == 20. SaveTuneData() indexes this with
 * (i - 2) for i up to sizeof(motor1ParaIndex) - 1, i.e. 19. */
int16 gParaIdToFunctionDataBuff[TUNE_DATA_NUM];
