/* Shared mock state for the f_p2p differential test.
 *
 * One instance, read by both the ref and the new side, so both observe the same
 * inputs. Only the symbols exported by the module under test are duplicated.
 */

#include "p2p_common.h"

struct FUNCCODE_MOCK funcCode;
Uint16 itDisp;
int32  frqRun;
int32  frqAimTmp;
Uint16 frqFdb;
Uint16 maxFrq;
Uint16 errorOther;
Uint16 errorInfo;
