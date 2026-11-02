/* MotorCurrentTransform differential-test mock: the shared global state the
 * module references but does not define.
 *
 * These eight symbols are `extern` in the real headers and are DEFINED by
 * other translation units in the firmware (MotorVar.c / MotorInfoCollect.c /
 * MotorPWM.c).  In the host bench nobody else defines them, so this file
 * provides one instance each and both the ref side and the new side see the
 * SAME memory.  That is deliberate: if one side stops writing a field, the
 * difference becomes visible instead of being masked by two private copies.
 *
 * The driver is therefore responsible for restoring the whole snapshot before
 * each side runs, and for comparing a full struct copy taken right after each
 * side returns.
 */

/* NOTE ON THE INCLUDE ORDER: MotorInclude.h must come first.  It resolves to
 * the case-private shim, which replays the real header chain and thereby sets
 * the MOTOR_INCLUDE_H guard.  Without that, the first quoted
 * `#include "MotorInclude.h"` reached from a sibling header in include/motor/
 * pulls in the REAL file, whose `extern interrupt void ..._isr` prototypes
 * MSVC rejects. */

#include "MotorInclude.h"
#include "MotorCurrentTransform.h"
#include "MotorInfoCollectInclude.h"
#include "MotorPwmInclude.h"

UVW_STRUCT_Q24    gIUVWQ24;
ALPHABETA_STRUCT  gIAlphBeta;
ALPHABETA_STRUCT  gIAlphBetaQ12;
MT_STRUCT_Q24     gIMTQ24;
MT_STRUCT         gIMTQ12;
AMPTHETA_STRUCT   gIAmpTheta;
OUT_VOLT_STRUCT   gOutVolt;
ANGLE_STRUCT      gPhase;
