/* f_error 差分测试的共享状态。
 *
 * 放在 shim/ 是因为 shim/ 是唯一排在 include 路径上的"用例私有头文件"位置
 * (用例目录本身不在路径上, 只有 case/ref 与交付源码目录在)。它**不做任何头文件
 * 替换**, 只是让 mock_error.c 与 driver.c 共用同一份声明, 免得两边各抄一份然后
 * 慢慢漂移。
 *
 * include 链与 src/function/f_error.c 逐行一致, 于是这里用到的都是**真类型**。
 * 这对本模块是硬要求: 观测量里有 funcCode.code.errorScene3.elem 的 8 个 Uint16、
 * 以及 GetCodeIndex() 算出来的两个下标, 替身类型会给出替身偏移。
 *
 * 注意这个头文件会被 build_variant 跟着 shim/ 一起复制到负向对照的变体目录 ——
 * 漏掉它的后果和漏掉 real.txt 一样: 变体编不过, 被误记成 BROKEN。
 */
#ifndef MOCK_ERROR_H
#define MOCK_ERROR_H

#include "f_error.h"
#include "f_main.h"
#include "f_runSrc.h"
#include "f_frqSrc.h"
#include "f_eeprom.h"
#include "f_io.h"
#include "f_menu.h"
#include "f_ui.h"
#include "f_comm.h"
#include "f_posCtrl.h"
#include "f_invPara.h"

/* ---- f_error.c 私有的量, 头文件里没有, 只能镜像 ----
 *
 * ERROR_SCENE_STRUCT 有 8 个 Uint16, 所以 union ERROR_SCENE 的 all[] 也是 8 项。
 * driver 用它来断言"下标扫满".
 */
#define FERR_SCENE_ELEM_NUM     8

/* 写队列容量 = 工程真值 (src/function/f_eeprom.c 的 FUNCCODE_ONE_WRITE_NUMBER_MAX)。
 * f_error.c 自己**不写**队列, 它只设 funcCodeRwModeTmp / startIndexWriteSeries /
 * endIndexWriteSeries 三个游标。队列仍然带进快照, 是因为 ErrorReset() 改的是
 * 那一组"EEPROM 串行写"的握手变量, 把队列一起比可以确认它没有顺手写坏别的。 */
#define FERR_WRITE_QUEUE_LEN    200

/* ---- 模块引用的 33 个外部符号: 31 个全局量 + 2 个被调函数 ----
 *
 * 全部由 mock_error.c 定义, ref 侧与 new 侧**共用**。 */
extern FUNCCODE_ALL funcCode;
extern union RUN_FLAG runFlag;
extern Uint16 runMode;
extern union RUN_CMD runCmd;
extern union DSP_MAIN_COMMAND dspMainCmd;
extern union DSP_MAIN_COMMAND1 dspMainCmd1;
extern union DSP_SUB_COMMAND dspSubCmd;
extern union DSP_STATUS dspStatus;
extern Uint16 tuneCmd;

extern Uint16 funcCodeRwMode;
extern Uint16 funcCodeRwModeTmp;
extern Uint16 startIndexWriteSeries;
extern Uint16 endIndexWriteSeries;
extern Uint16 funcCodeOneWriteIndex[FERR_WRITE_QUEUE_LEN];
extern Uint16 funcCodeOneWriteNum;

extern int32  frq;
extern int32  frqRun;
extern int32  frqFdbTmp;
extern Uint16 maxFrq;
extern Uint16 generatrixVoltage;

extern struct MOTOR_FC motorFc;
extern struct DI_FUNC diFunc;
extern struct DI_FUNC diSelectFunc;
extern struct DI_STATUS_ALL diStatus;
extern struct DO_STATUS_ALL doStatus;
extern Uint16 outCurrentDispOld;
extern Uint16 currentOcDisp;

extern Uint16 errorCodeFromMotor;
extern Uint16 errorsCodeFromMotor[2];
extern Uint16 motorErrInfor[5];

extern Uint16 keyFunc;
extern enum POWER_ON_STATUS powerOnStatus;
extern enum MOTOR_SN motorSn;

/* ---- 两个被调函数是模块的**输入**而不是被测逻辑, 所以用替身 ----
 *
 * GetNumberDigit1() 的真实现是 f_main.c:855 (把一个 Uint16 拆成 5 位十进制数字,
 * 低位在前)。这里**逐字照抄**: GetErrorAttribute() 的返回值就是 digit[...] + 1,
 * 所以替身的拆位规则必须和真的一样, 否则测的就不是同一张表。
 * 参数与调用次数单独记一份 —— "读错 errorAction 下标" 这种错法只有把实参也记下来
 * 才看得见 (换一个下标, 数字可能仍然拆出同样的某一位)。
 *
 * PowerOffRemDeal() 的真实现在 f_fcDeal.c 里 (已单独做过差分测试), 返回 void,
 * 所以它的观测量只能是**调用次数**。 */
extern Uint16 mockDigitCalls;
extern Uint16 mockDigitLastArg;
extern Uint16 mockPowerOffCalls;

void GetNumberDigit1(Uint16 digit[5], Uint16 number);
void PowerOffRemDeal(void);

#endif
