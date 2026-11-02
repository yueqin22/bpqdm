/* f_interface 差分测试的共享声明。

 * 放在 shim/ 是因为 shim/ 是唯一排在 include 路径上的"用例私有头文件"位置
 * (用例目录本身不在路径上, 只有 case/ref 与交付源码目录在)。它**不做任何头文件
 * 替换**, 只是让 mock_interface.c 与 driver.c 共用同一份声明, 免得两边各抄一份然后漂移。

 * include 链与 src/function/f_interface.c 完全一致, 于是这里用到的都是**真类型**。
 * 这对本模块是硬要求: 观测量里有一大半是 `FUNCCODE_ALL` / `struct MOTOR_FC` /
 * `union RUN_FLAG` / `union DSP_MAIN_COMMAND` 的**具体位域布局** ——
 *   * UpdateDataFunc2Core2ms 把十几个 `funcCode.code.*` 逐位塞进
 *     `dspMainCmd.bit.*` / `dspSubCmd.bit.*` / `dspMainCmd1.bit.*`;
 *   * copyDataFunc2Motor2ms 用 `memcpy(&buff[5], &motorFc.motorPara.elem.ratingSpeed, 6)`
 *     这种"按字段连续排列"的假设一次搬 6 个字;
 *   * UpdateDataCore2Func 把 `funcCode.code.powerAddupInt` 当 16 位计数器自增。
 * 替身类型会让位域位置与字段偏移整体错位, 测试就失去意义。

 * 注意这个头文件会被 build_variant 跟着 shim/ 一起复制到负向对照的变体目录 ——
 * 漏掉它的后果和漏掉 real.txt 一样: 变体编不过, 被误记成 BROKEN。
 */
#ifndef MOCK_INTERFACE_H
#define MOCK_INTERFACE_H

#include "f_funcCode.h"
#include "f_runSrc.h"
#include "f_main.h"
#include "f_frqSrc.h"
#include "f_io.h"
#include "f_menu.h"
#include "f_posCtrl.h"
#include "f_interface.h"
#include "f_p2p.h"

/* ---- 两个只在 .c 里 extern 声明、任何头文件都没有的共享量 -------------------
 *
 * f_interface.c 第 68 行 `extern Uint16 softOcDoFlag;`、第 210-211 行
 * `extern Uint16 errorInfoFromMotor; extern Uint16 motorCtrlTuneStatus;`。
 * 它们定义在别的模块里 (交付源码的 f_io.c / f_comm.c 一带), 不随本模块一起编译,
 * 所以由 mock 提供。声明放这里, 好让 driver 也能读。
 *
 * errorInfoFromMotor 虽然被声明, 但本模块**从未引用** —— 它不在目标文件的未定义
 * 符号表里 (见 tools/difftest/cases/f_interface/README.md 的符号核对一节)。
 * 仍然定义一份, 免得将来改写引入引用时无声地缺符号。
 */
extern Uint16 softOcDoFlag;
extern Uint16 errorInfoFromMotor;
extern Uint16 motorCtrlTuneStatus;

/* ---- 缓冲区真实容量 --------------------------------------------------------
 *
 * 交付源码里这三张表的定义在 src/orphan/z_func.c:32-35:
 *     int16 gSendToMotor05MsDataBuff[FUNC_TO_MOTOR_05MS_DATA_NUM];           = 14
 *     int16 gSendToMotor2MsDataBuff[FUNC_TO_MOTOR_2MS_DATA_NUM
 *                                   + FUNC_TO_CORE_DEBUG_DATA_NUM];          = 75+40 = 115
 *     int16 gSendToFunctionDataBuff[MOTOR_TO_Func_2MS_DATA_NUM
 *                                   + CORE_TO_FUNC_DISP_DATA_NUM];           = 30+30 = 60
 * 这里**原样照抄** (不放大): 本模块对它们的写入点全部落在界内, 且
 * copyDataFunc2CF2ms 的 `memcpy(&buff[75], &funcCode.group.cf[0], 40)` 正好写到
 * 第 114 项 —— 容量本身就是被测语义的一部分, 放大反而会把越界掩盖掉。
 */
#define IFACE_MOTOR_05MS_NUM   (FUNC_TO_MOTOR_05MS_DATA_NUM)
#define IFACE_MOTOR_2MS_NUM    (FUNC_TO_MOTOR_2MS_DATA_NUM + FUNC_TO_CORE_DEBUG_DATA_NUM)
#define IFACE_FUNC_2MS_NUM     (MOTOR_TO_Func_2MS_DATA_NUM + CORE_TO_FUNC_DISP_DATA_NUM)

#endif
