/* f_menu 差分测试的共享状态。
 *
 * 放在 shim/ 是因为 shim/ 是唯一排在 include 路径上的"用例私有头文件"位置
 * (用例目录本身不在路径上, 只有 case/ref 与交付源码目录在)。它**不做任何头文件
 * 替换**, 只是让 mock_menu.c 与 driver.c 共用同一份声明, 免得两边各抄一份然后
 * 慢慢漂移。
 *
 * include 链与 src/function/f_menu.c 一致 (外加 f_error.h —— 模块用的
 * errorCode / errorAttribute / errorInfo / curTime 在那一份里), 于是这里用到的
 * 都是**真类型**。这对本模块是硬要求:
 *
 *   - GetCodeIndex() 是 `(&code) - (&funcCode.all[0])` 的指针减法, 观测量里
 *     有一大半是它算出来的下标; 替身类型会给出替身偏移。
 *   - funcCodeAttribute[] 的 909 项、funcCodeGradeAll/Sum 的 83 项是**真表**
 *     (由 build/_gen_fmenu_tables.py 从交付源码逐字抄出), 模块的整套限值/分级
 *     语义都挂在这三张表上。
 *
 * 注意这个头文件会被 build_variant 跟着 shim/ 一起复制到负向对照的变体目录 ——
 * 漏掉它的后果和漏掉 real.txt 一样: 变体编不过, 被误记成 BROKEN。
 */
#ifndef MOCK_MENU_H
#define MOCK_MENU_H

#include "f_menu.h"
#include "f_main.h"
#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_io.h"
#include "f_ui.h"
#include "f_eeprom.h"
#include "f_comm.h"
#include "f_posCtrl.h"
#include "f_invPara.h"
#include "f_fcDeal.h"
#include "f_p2p.h"
#include "f_error.h"

/* ---- 镜像的模块私有量 (头文件里没有) ----
 *
 * f_menu.c 自己定义的两个宏, 头文件里没有:
 *   DECIMAL_DISPLAY_UPDATE_TIME = 6, UP_DOWN_DEAL_DONE_TIME = 800
 * 它们只参与显示刷新计数, driver 用不到数值, 但要断言"表没被抄错"时才有意义,
 * 所以这里只镜像真正会用到的两个尺寸。
 */
#define FMENU_UP_DOWN_DEAL_DONE_TIME    800

/* 写队列容量 = 工程真值 (src/function/f_eeprom.c:49 的
 * FUNCCODE_ONE_WRITE_NUMBER_MAX)。f_menu.c 自己**不写**队列, 它只经
 * ModifyFunccodeEnter -> SaveOneFuncCode 那条路间接碰它; 队列仍然带进快照,
 * 是为了确认模块没有顺手写坏别的。 */
#define FMENU_WRITE_QUEUE_LEN           200

/* funcCodeAttribute[] 的项数 (= FNUM_PARA, 链接映射里 0x11c1/5 = 909)。 */
#define FMENU_ATTR_N                    909

/* funcCodeGradeAll[] 的项数 = FUNCCODE_GROUP_NUM。 */
#define FMENU_GRADE_N                   FUNCCODE_GROUP_NUM

/* ---- 模块引用的 57 个全局量: 全部由 mock_menu.c 定义, 两侧**共用** ---- */

extern FUNCCODE_ALL funcCode;

extern union RUN_FLAG runFlag;
extern Uint16 runMode;
extern Uint16 runSrc;
extern Uint16 tuneCmd;
extern Uint16 keyFunc;
extern Uint16 upDownDelta;
extern union INVT_STATUS invtStatus;
extern Uint16 setRunLostTime;
extern int16  torqueAim;

extern struct DI_FUNC diFunc;
extern struct DI_STATUS_ALL diStatus;
extern struct DO_STATUS_ALL doStatus;
extern struct AI_DEAL aiDeal[AI_NUMBER];

extern Uint16 displayBuffer[DISPLAY_8LED_NUM + 1];
extern const Uint16 DISPLAY_CODE[DISPLAY_8LED_CODE_NUM];
extern const Uint16 LED_CODE[DISPLAY_LED_CODE_NUM];

extern struct CUR_TIME curTime;
extern union ERROR_ATTRIBUTE errorAttribute;
extern Uint16 errorCode;
extern Uint16 errorInfo;

extern int32  frq;
extern int32  frqAim;
extern int32  frqDroop;
extern int32  frqRun;
extern Uint16 frqFdb;
extern Uint16 frqKeyUpDownDelta;
extern Uint16 bFrqDigital;
extern Uint16 maxFrq;
extern Uint16 plcStep;
extern PID_FUNC pidFunc;

extern Uint16 generatrixVoltage;
extern Uint16 currentPu;
extern Uint16 enCoderPosition;
extern Uint16 ABZPos;
extern int16  gPhiRtDisp;
extern int16  outPower;
extern Uint32 pulseInFrq;
extern Uint16 lineSpeed;
extern Uint16 memoryValue;
extern Uint16 torqueCurrent;
extern Uint16 pmsmRotorPos;

extern struct MOTOR_FC motorFc;
extern enum MOTOR_SN motorSn;

extern Uint16 temperature;
extern Uint16 ZCount;
extern Uint16 vfSeparateVol;
extern Uint16 vfSeprateVolAim;
extern Uint16 ticker05msFuncEachDisp[4];
extern Uint16 ticker05msFuncInternalDisp[4];

extern P2P_DATA_STRUCT p2pData;

extern struct INV_PARA invPara;
extern const struct INV_TYPE_LIMIT invTypeLimitTable[INV_TYPE_VOLTAGE_NUM];

extern Uint16 funcCodeRwMode;
extern Uint16 funcCodeRwModeTmp;
extern Uint16 funcCodeOneWriteIndex[FMENU_WRITE_QUEUE_LEN];
extern Uint16 funcCodeOneWriteNum;

extern const FUNCCODE_ATTRIBUTE funcCodeAttribute[FMENU_ATTR_N];
extern const Uint16 funcCodeGradeAll[FMENU_GRADE_N];
extern const Uint16 funcCodeGradeSum[FMENU_GRADE_N];
extern Uint16 funcCodeGradeCurMenuMode[FMENU_GRADE_N];

/* ---- 被调函数: 模块的**输入**而不是被测逻辑, 所以用替身 ----
 *
 * 每一个都逐字照抄真实现 (见 mock_menu.c 的注释), 并单独记调用次数与末次实参 ——
 * "读错下标 / 传错参数"这种错法只有把实参也记下来才看得见。
 */
extern Uint16 mockDigitCalls, mockDigitLastArg, mockDigitLastMode;
extern Uint16 mockGetInitCalls, mockGetInitLastIndex, mockGetInitLastType;
extern Uint16 mockLimitOtherCalls, mockLimitOtherLastIndex;
extern Uint16 mockClearRecordCalls;
extern Uint16 mockMotorPowerCalls, mockMotorPowerLastPower, mockMotorPowerLastSn;
extern Uint16 mockInvTypeRelatedCalls;
extern Uint16 mockValidateInvTypeCalls, mockValidateInvTypeRet;
extern Uint16 mockResetUpDownCalls;
extern Uint16 mockLpfCalls, mockLpfLastT, mockLpfLastIn;

Uint16 GetNumberDigit(Uint16 digit[5], Uint16 number, Uint16 mode);
void   GetNumberDigit1(Uint16 digit[5], Uint16 number);
Uint16 GetFuncCodeInit(Uint16 index, Uint16 type);
void   LimitOtherCodeDeal(Uint16 index);
void   ClearRecordDeal(void);
void   MotorPowerRelatedParaDeal(Uint16 power, Uint16 motorSn4Power);
void   InverterTypeRelatedParaDeal(void);
Uint16 ValidateInvType(void);
void   ResetUpDownFrq(void);
void   LpfCalc(LowPassFilter *p);

#endif
