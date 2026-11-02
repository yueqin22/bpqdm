/* f_eeprom 差分测试的共享声明。

 * 放在 shim/ 是因为 shim/ 是唯一排在 include 路径上的"用例私有头文件"位置
 * (用例目录本身不在路径上, 只有 case/ref 与交付源码目录在)。它**不做任何头文件
 * 替换**, 只是让 mock_eeprom.c 与 driver.c 共用同一份声明, 免得两边各抄一份然后漂移。

 * include 链与 src/function/f_eeprom.c 完全一致, 于是这里用到的都是**真类型**。
 * 这对本模块是硬要求: 本模块几乎全部行为都建立在"每页 4 字节 = 2 个功能码"
 * 之上 —— FUNCCODE_RW.data[2]、I2cMsg.buffer[4]、WriteFuncCode 的
 * `(index & 0xFFF8) + 0x0008` 页对齐、ReadFuncCode 的 `buffer[i<<1] << 8` 拼接 ——
 * 宽度或数组长度错一点, 整个测试就失去意义。

 * 注意这个头文件会被 build_variant 跟着 shim/ 一起复制到负向对照的变体目录 ——
 * 漏掉它的后果和漏掉 real.txt 一样: 变体编不过, 被误记成 BROKEN。
 */
#ifndef MOCK_EEPROM_H
#define MOCK_EEPROM_H

#include "DSP28x_Project.h"

#include "f_funcCode.h"
#include "f_eeprom.h"
#include "f_main.h"
#include "f_menu.h"
#include "DSP2803x_I2C_defines.h"

/* ---- 本模块引用但不拥有的外部量 -----------------------------------------
 *
 * 清单来自 `nm2000 --undefined-only build/Debug/f_eeprom.obj` 读出的 12 个外部
 * 引用 (见 README 的符号核对一节), 逐个对照, 一个不多一个不少:
 *
 *   _funcCode                     FUNCCODE_ALL      —— f_funcCode.c
 *   _funcCodeAttribute            FUNCCODE_ATTRIBUTE[] —— f_funcCode.c (替身)
 *   _fcNoAttri2Eeprom             const Uint16[]    —— f_main.c (替身)
 *   _errorEeprom                  Uint16            —— f_error.c
 *   _powerOnStatus                enum POWER_ON_STATUS —— f_main.c
 *   _aiaoChckReadOK               Uint16            —— f_main.c
 *   _GetFuncCodeInit              Uint16(Uint16,Uint16) —— f_fcDeal.c
 *   _GetFuncCodeInitOriginal      Uint16(Uint16)    —— f_fcDeal.c
 *   _RestoreCompanyParaOtherDeal  void(void)        —— f_fcDeal.c
 *   _I2caRegs / _GpioCtrlRegs / _GpioDataRegs       —— 外设
 *
 * 前三个函数 (GetFuncCodeInit / GetFuncCodeInitOriginal /
 * RestoreCompanyParaOtherDeal) 由 mock 提供**记录型替身**而不是固定返回值 ——
 * 见 mock_eeprom.c 的说明。
 */
extern volatile struct I2C_REGS         I2caRegs;
extern volatile struct GPIO_CTRL_REGS   GpioCtrlRegs;
extern volatile struct GPIO_DATA_REGS   GpioDataRegs;

extern FUNCCODE_ALL funcCode;
extern const FUNCCODE_ATTRIBUTE funcCodeAttribute[];
extern const Uint16 fcNoAttri2Eeprom[];

extern Uint16 errorEeprom;
extern enum POWER_ON_STATUS powerOnStatus;
extern Uint16 aiaoChckReadOK;

/* ---- CPU 控制序列计数表 (DSP2803x_Device.h shim 声明, mock 定义) ---------- */
extern volatile Uint16 gHostCpuCtl[7];

/* ---- DSP28x_usDelay 计数 (DSP2803x_Examples.h shim 声明, mock 定义) --------
 *
 * mock 侧把每次调用的 **LoopCount 累加** 进这张表, 于是"改写把某处 DELAY_US 删了
 * 或改了参数"会体现为对不上。只记次数不够: DELAY_US(5) 与 DELAY_US(20000) 的
 * LoopCount 分别是 58 与 239994, 差着三个数量级。
 */
extern Uint32 gHostDelaySum;
extern Uint32 gHostDelayCalls;

/* ---- GPIO "寄存器写语义" 的外部电平输入 -----------------------------------
 *
 * GPBSET / GPBCLEAR 是**写 1 生效**的寄存器: 写 1 到 GPBSET 的第 n 位会置
 * GPBDAT 的第 n 位, 写 1 到 GPBCLEAR 的第 n 位会清它。这是芯片行为,
 * 但普通内存变量不会自动实现, 所以 mock 侧在每次读 GPBDAT **之前**做一次
 * "把 GPBSET/GPBCLEAR 的待处理位应用到 GPBDAT" 的搬运 —— 见
 * mock_eeprom.c 的 gpio_apply() 一节。
 *
 * 搬运要区分方向: GPBDIR 的第 n 位为 1 (输出) 时, GPBDAT 读回输出锁存, 于是
 * GPBCLEAR 真的把它清 0; 为 0 (输入) 时读回的是**引脚外部电平**, 不受
 * GPBCLEAR 影响。gHostPinExtLevel32 就是"GPIO32 这根本脚的外部电平"。
 *
 * f_eeprom 里唯一需要它的地方是 I2cDealBeforeInit: 它先 SdaIoAsOutput(),
 * I2cDataHigh(), SdaIoAsInput(), 读一次 SDA; 若为 1 则 SdaIoAsOutput(),
 * I2cDataLow(), SdaIoAsInput(), 再读一次。第二次读之前 GPBDIR 被设回**输入**,
 * 所以读回的是外部电平 —— 这正是"从机是否拉低 SDA"这一句原始语义的模型。
 */
extern Uint16 gHostPinExtLevel32;

/* ---- 本模块拥有的对象 ----------------------------------------------------
 * 它们都是 extern 链接 (LOCALF/LOCALD 在本工程展开成空), 因此被 ref_tu.c /
 * new_tu.c 改名成 ref_/new_, 每侧各持一份。这里把**两侧的**名字都声明出来,
 * 好让 driver 能安装/快照。清单与 ref_tu.c 的 #define 表一一对应。
 */
extern Uint16 ref_funcCodeOneWriteNum;
extern Uint16 ref_funcCodeOneWriteIndex[];
extern Uint16 ref_funcCodeRwMode;
extern Uint16 ref_funcCodeRwModeTmp;
extern Uint16 ref_funcCodeRwStatus;
extern struct I2C_MSG ref_I2cMsg;
extern Uint16 ref_startIndexWriteSeries;
extern Uint16 ref_endIndexWriteSeries;
extern Uint16 ref_endIndexRwFuncCode;
extern FUNCCODE_RW ref_funcCodeRead;
extern FUNCCODE_RW ref_funcCodeWrite;
extern Uint16 ref_rwFuncCodeRepeatDelayTicker;
extern struct EEPROM_OPERATE_TIME ref_eepromOperateTime;

extern Uint16 new_funcCodeOneWriteNum;
extern Uint16 new_funcCodeOneWriteIndex[];
extern Uint16 new_funcCodeRwMode;
extern Uint16 new_funcCodeRwModeTmp;
extern Uint16 new_funcCodeRwStatus;
extern struct I2C_MSG new_I2cMsg;
extern Uint16 new_startIndexWriteSeries;
extern Uint16 new_endIndexWriteSeries;
extern Uint16 new_endIndexRwFuncCode;
extern FUNCCODE_RW new_funcCodeRead;
extern FUNCCODE_RW new_funcCodeWrite;
extern Uint16 new_rwFuncCodeRepeatDelayTicker;
extern struct EEPROM_OPERATE_TIME new_eepromOperateTime;

/* ---- mock 调用轨迹 -------------------------------------------------------
 *
 * 三个替身函数把每次调用的**参数与次数**记在这几个变量里 (见 mock_eeprom.c),
 * driver 在每一步之后比对两侧。固定返回值会让"参数传错了"完全不可见。
 */
extern Uint16 gMockGfciCalls;
extern Uint16 gMockGfciLastIndex;
extern Uint16 gMockGfciLastType;
extern Uint32 gMockGfciArgSum;

extern Uint16 gMockGfcioCalls;
extern Uint16 gMockGfcioLastIndex;

extern Uint16 gMockRcpoCalls;


/* ---- 用例私有的替身表尺寸 -------------------------------------------------
 *
 * `funcCodeAttribute` 与 `fcNoAttri2Eeprom` 在真工程里是两张**巨大**的静态表
 * (f_funcCode.c 与 f_main.c), 链进来会带一大坨无关符号。这里用替身:
 *
 *   funcCodeAttribute[]   下标是 fcIndex, 长度必须 >= FNUM_PARA = 909
 *                         (GetEepromIndexFromFcIndex 的 else 分支按 fcIndex 索引)
 *   fcNoAttri2Eeprom[]    下标是 (fcIndex - 909), 长度必须 >= 4
 *                         (909..912 是 fChk 那 4 个)
 *
 * 两张表由 mock 生成, 生成规律写在 mock_eeprom.c 里。**不是**真表的拷贝 ——
 * 差分测试只要求两侧看到同一份, 不要求它等于真值; 表的真值由目标端链接保证。
 */
#define EEP_ATTR_N   909
#define EEP_NOATTR_N 8

#endif
