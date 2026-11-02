/* Host-side mock for MotorDebug.
 *
 * The module reads two globals it does not define: gTestDataReceive (filled by
 * MotorDataExchange from the debug port) and gMainCmd (filled by the command
 * layer).  Both are defined here as a single instance each, shared by the ref
 * and new translation units -- the driver installs a byte-identical snapshot
 * before each call, exactly like install_side() in the other cases.
 *
 * MotorInclude.h is included FIRST so the MOTOR_INCLUDE_H guard is set: the
 * module's own quoted include of the sibling real MotorInclude.h would
 * otherwise pull in the whole 17-header chain (and the `extern interrupt`
 * prototypes MSVC rejects).
 */

#include "MotorInclude.h"

MOTOR_DEBUG_DATA_RECEIVE_STRUCT     gTestDataReceive;
BASE_COMMAND_STRUCT                 gMainCmd;
