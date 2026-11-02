/* f_comm case mock: 外设对象 + 工程全局量 + 被调函数替身, 两侧共享一份。
 *
 * 这里的东西都**不按侧重命名** —— 它们是模块的输入面与输出面, 不是任一侧的
 * 实现。任何一处按侧复制, 都会让"差异来自输入"和"差异来自代码"分不开。
 *
 * 注意: f_comm.c **自己定义**了它那十几个状态量 (commStatus / commRcvData /
 * commSendData / sendFrame / rcvFrame / commReadData / sciFlag / commTicker /
 * commType / commProtocol / aoComm / doComm / 三个密码通过标志 / commRunCmd /
 * canLinkTicker)。那些量经 ref_tu.c / new_tu.c 改名成 ref_* / new_*, 由
 * driver.c 用 extern 分别访问 —— 本文件**不能**定义它们, 否则链接期撞名。
 *
 * ---------------------------------------------------------------------------
 * 1. 外设对象 (真寄存器结构体, 实例化而不是造替身)
 *
 *   LinaRegs       resetLinSci / closeRTX / setRxConfig / setTxConfig /
 *                  InitSetScia / UpdateSciFormat 一共写二十多个位域;
 *                  UpdateSciFormat 还**读** SCIFLR 的 BRKDT/PE/OE/FE 来决定
 *                  要不要整块重新初始化。所以 LINA_REGS 的位域布局既是输入面
 *                  也是输出面, 必须用真头里的结构, 替身会把布局差异抹平。
 *   GpioCtrlRegs   InitSciaGpio 写 GPAPUD/GPAPUD/GPBMUX1/GPBDIR/GPAQSEL1
 *   GpioDataRegs   RTS 宏 = GPBDAT.bit.GPIO39, 既写 (RS485_T) 也读
 *   PieCtrlRegs    PIEACK.bit.ACK9 (两个 ISR) / PIEIER9.bit.INTx3,INTx4
 *   PieVectTable   LIN0INTA / LIN1INTA 两个向量槽
 *   IER            `IER |= M_INT9;` —— 是 |=, 所以预置值有意义
 *
 *   EALLOW / EDIS 做成 gHostCpuCtl[] 的计数器而不是空宏。"改写把寄存器写挪到
 *   EALLOW 窗口外面" 只比结果永远看不出来, 比计数才看得见。代价是改写时不能
 *   为了好看把两对 EALLOW/EDIS 合成一对。
 *
 * ---------------------------------------------------------------------------
 * 2. 工程全局量
 *
 *   funcCode       f_funcCode.c 定义。那份文件不能链: funcCodeGradeSum 的静态
 *                  初始化式是"两个地址常量相减", MSVC 报 C2099 (见
 *                  tools/difftest/gen_plc_grade_tables.py 的长注释)。
 *   runFlag / errorCode / errorOther / errorInfo / keyBordTestFlag /
 *   keyBordValue / p2pData / funcCodeRwMode / funcCodeRwModeTmp
 *                  分别来自 f_runSrc.c / f_error.c / f_ui.c / f_p2p.c /
 *                  f_eeprom.c。本模块只读它们的标量字段, 由 driver 供值。
 *   funcCodeGradeSum[] / funcCodeGradeAll[]
 *                  由 grade_tables.inc 的宏算术给出, 数值与固件一致 (生成器
 *                  从头文件的成员表推 sizeof 累加, 并断言 83 项一一同序)。
 *   funcCodeOneWriteIndex[] / funcCodeOneWriteNum
 *                  SaveOneFuncCode 是 f_eeprom.h 里的宏, 直接写这两个数组 ——
 *                  于是"到底把哪个 index 排进了 EEPROM 写队列"是**可观测**的,
 *                  driver 把它当输出比对。
 *
 * ---------------------------------------------------------------------------
 * 3. 被调函数替身
 *
 * 每个替身都**可被 driver 参数化**, 并且把调用记进日志。固定返回值会把分支
 * 判据整片抹平: 例如让 ModifyFunccodeUpDown 恒返回 COMM_ERR_NONE, 则
 * "COMM_ERR_PARA 支" 与 "COMM_ERR_READ_ONLY 支" 永远走不到, 负向对照会成片
 * MISSED 而看起来像"等价"。
 *
 * CrcValueByteCalc 与 GetNumberDigit1 是**逐字抄自冻结基准**的真实现, 不是常量:
 *   CrcValueByteCalc  f_comm 拿它的返回值当分支条件 (帧校验), 也把它写进出帧。
 *                     换成常量会让"数据错"和"CRC 错"塌成同一个结果。
 *   GetNumberDigit1  UpdateSciFormat 用它把 commBaudRate / commProtocol 拆成
 *                     十进制位, 再用 digit[commType] 去索引 dspBaudRegData[]。
 *                     换成常量等于把整条波特率选择路径钉死。
 */
#include "f_comm.h"

/* ------------------------------------------------------------------------- */
/* 1. 外设对象                                                                */
/* ------------------------------------------------------------------------- */

volatile struct LIN_REGS          LinaRegs;
volatile struct GPIO_CTRL_REGS    GpioCtrlRegs;
volatile struct GPIO_DATA_REGS    GpioDataRegs;
volatile struct PIE_CTRL_REGS     PieCtrlRegs;
struct PIE_VECT_TABLE             PieVectTable;
volatile Uint16                   IER;
volatile Uint16                   gHostCpuCtl[7];

/* ------------------------------------------------------------------------- */
/* 2. 工程全局量                                                              */
/* ------------------------------------------------------------------------- */

#include "grade_tables.inc"

FUNCCODE_ALL funcCode;

const Uint16 funcCodeGradeSum[FUNCCODE_GROUP_NUM] = { GRADE_SUM_INIT };
const Uint16 funcCodeGradeAll[FUNCCODE_GROUP_NUM] = { GRADE_ALL_INIT };

Uint16 funcCodeOneWriteIndex[8];
Uint16 funcCodeOneWriteNum;

union RUN_FLAG runFlag;

Uint16 errorCode;
Uint16 errorOther;
Uint16 errorInfo;

Uint16 keyBordTestFlag;
Uint16 keyBordValue;

P2P_DATA_STRUCT p2pData;

Uint16 funcCodeRwMode;
Uint16 funcCodeRwModeTmp;

/* commDispIndex[] 的真身住在 include/function/f_funcCode_disp.c 里。那个文件
 * **不在 .lkf 的编译清单上** —— 它是被 f_menu.c 用 `#include "f_funcCode_disp.c"`
 * 拉进同一个翻译单元的, 所以链接期才有定义。
 *
 * 整份 include 进来不行: 那个文件除了这张表还有一整个 FuncCodeDisp() 的函数体,
 * 引用 diStatus / doStatus / aiDeal / lineSpeed / pulseInFrq 等一堆 f_ui /
 * f_runSrc 的全局量, 编不过 (实测 C2065 x 十几条)。所以这里只**冻结那张表**。
 *
 * 逐字抄的正当性: 它是一张 33 项的**常量数据表**, 不是逻辑; f_comm 只读它
 * (f_comm.c:551 `commDispIndex[i + low]`), 从不写它, 所以两侧看到同一份。抄的
 * 唯一风险是"源文件改了这里没跟" —— 但 f_comm 的差分测试对这张表的**取值**不
 * 敏感 (两侧同一个表), 只对**下标怎么算**敏感; 表里 33 项互不相同的程度足以让
 * `[i+low]` 与 `[i]` 这类错法分得开。改 f_funcCode_disp.c 时这份要跟着重新冻结。
 *
 * 不用 driver 填表: f_menu.h 把它声明成 `Uint16 const`, 驱动改不了。
 */
Uint16 const commDispIndex[COMM_PARA_NUM] =
{
    28,  0,  2,  3,  4,  5,  6, 19,
     7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22,
    23, 24, 25, 26, 27, 28, 29, 30,
    31,
};

/* ------------------------------------------------------------------------- */
/* 3. 调用日志 (driver 清空 / 读取)                                           */
/* ------------------------------------------------------------------------- */

#define LOG_N 64

Uint16 gMockLog[LOG_N];
Uint16 gMockLogNum;

/* 参数化开关, driver 每例重设。 */
Uint16 gMockStartRet[2];        /* Modbus / Profibus 的 StartDeal 返回值   */
Uint16 gMockErrCheckRet[2];     /* Modbus / Profibus 的 CommErrCheck 返回值 */
Uint16 gMockUpDownRet;          /* ModifyFunccodeUpDown 返回值             */
Uint16 gMockUpDownData;         /* ModifyFunccodeUpDown 写回的 *data       */
Uint16 gMockEnterRet;           /* ModifyFunccodeEnter 返回值              */
Uint16 gMockFormatBaud[2];      /* Update*CommFormat 收到的 baudRate       */
Uint16 gMockSendErr[2];         /* *SendDataDeal 收到的 err                */
Uint16 gMockStartArg[2];        /* *StartDeal 收到的 tmp                   */

static void log_push(Uint16 tag, Uint16 arg)
{
    if (gMockLogNum + 2 <= LOG_N)
    {
        gMockLog[gMockLogNum++] = tag;
        gMockLog[gMockLogNum++] = arg;
    }
}

void MockResetLog(void)
{
    Uint16 i;

    gMockLogNum = 0;
    for (i = 0; i < LOG_N; i++)
    {
        gMockLog[i] = 0;
    }
}

/* ------------------------------------------------------------------------- */
/* 3a. 协议入口替身 (protocolFunc[] 的十个槽)                                 */
/* ------------------------------------------------------------------------- */

void ModbusRcvDataDeal(void)                        { log_push(0x0100, 0); }
void ProfibusRcvDataDeal(void)                      { log_push(0x0101, 0); }

Uint16 ModbusStartDeal(Uint16 tmp)
{
    gMockStartArg[0] = tmp;
    log_push(0x0200, tmp);
    return gMockStartRet[0];
}

Uint16 ProfibusStartDeal(Uint16 tmp)
{
    gMockStartArg[1] = tmp;
    log_push(0x0201, tmp);
    return gMockStartRet[1];
}

void UpdateModbusCommFormat(Uint16 baudRate)
{
    gMockFormatBaud[0] = baudRate;
    log_push(0x0300, baudRate);
}

void UpdateProfibusCommFormat(Uint16 baudRate)
{
    gMockFormatBaud[1] = baudRate;
    log_push(0x0301, baudRate);
}

void ModbusSendDataDeal(Uint16 err)
{
    gMockSendErr[0] = err;
    log_push(0x0400, err);
}

void ProfibusSendDataDeal(Uint16 err)
{
    gMockSendErr[1] = err;
    log_push(0x0401, err);
}

Uint16 ModbusCommErrCheck(void)
{
    log_push(0x0500, gMockErrCheckRet[0]);
    return gMockErrCheckRet[0];
}

Uint16 ProfibusCommErrCheck(void)
{
    log_push(0x0501, gMockErrCheckRet[1]);
    return gMockErrCheckRet[1];
}

/* ------------------------------------------------------------------------- */
/* 3b. funcCode / 菜单层的替身                                                */
/* ------------------------------------------------------------------------- */

void UpdataFuncCodeGrade(Uint16 funcCodeGrade[])
{
    Uint16 i;

    /* 真实现 (f_menu.c:2499) 的第一句就是这个 memcpy, 后面按
     * funcCode.code.funcParaView 的十进制位逐组收窄。那段收窄是 f_menu 的
     * 逻辑, 不是 f_comm 的, 复制过来只会让本用例的失败指向错误的模块;
     * 所以这里只保留"取全量"这一半, 并让 driver 把 funcParaView 钉成
     * 不收窄的取值 —— 两侧看到同一张表, 判据仍然可分辨。 */
    for (i = 0; i < FUNCCODE_GROUP_NUM; i++)
    {
        funcCodeGrade[i] = funcCodeGradeAll[i];
    }

    log_push(0x0600, 0);
}

Uint16 ModifyFunccodeUpDown(Uint16 index, Uint16 *data, int16 delta)
{
    /* delta 必须记进日志。
     *
     * 第一版写的是 `(void)delta;` —— 于是 "f_comm 传给上下限函数的第三参数是 0
     * 还是 1" 在宿主上**完全不可观测**, RF17 首轮 MISSED, 看起来像等价变异。
     * 但真实现 (f_menu.c:1755) 里 delta 是一路传到 LimitDeal 的加减量:
     *
     *     dataSigned += delta;   // LimitDeal
     *
     * 所以它是**真参数**, 不是摆设; 替身把它丢掉等于把判据抹平。 */
    log_push(0x0700, index);
    log_push(0x0702, (Uint16)delta);
    *data = gMockUpDownData;
    return gMockUpDownRet;
}

Uint16 ModifyFunccodeEnter(Uint16 index, Uint16 data)
{
    log_push(0x0800, index);
    log_push(0x0801, data);
    return gMockEnterRet;
}

void ClearRecordDeal(void)
{
    log_push(0x0900, 0);
}

/* ------------------------------------------------------------------------- */
/* 3c. 逐字抄自冻结基准的两个依赖                                             */
/* ------------------------------------------------------------------------- */

const Uint16 crc16Table[256] =
{
0x0000, 0xC1C0, 0x81C1, 0x4001, 0x01C3, 0xC003, 0x8002, 0x41C2, 0x01C6, 0xC006, 0x8007, 0x41C7,
0x0005, 0xC1C5, 0x81C4, 0x4004, 0x01CC, 0xC00C, 0x800D, 0x41CD, 0x000F, 0xC1CF, 0x81CE, 0x400E,
0x000A, 0xC1CA, 0x81CB, 0x400B, 0x01C9, 0xC009, 0x8008, 0x41C8, 0x01D8, 0xC018, 0x8019, 0x41D9,
0x001B, 0xC1DB, 0x81DA, 0x401A, 0x001E, 0xC1DE, 0x81DF, 0x401F, 0x01DD, 0xC01D, 0x801C, 0x41DC,
0x0014, 0xC1D4, 0x81D5, 0x4015, 0x01D7, 0xC017, 0x8016, 0x41D6, 0x01D2, 0xC012, 0x8013, 0x41D3,
0x0011, 0xC1D1, 0x81D0, 0x4010, 0x01F0, 0xC030, 0x8031, 0x41F1, 0x0033, 0xC1F3, 0x81F2, 0x4032,
0x0036, 0xC1F6, 0x81F7, 0x4037, 0x01F5, 0xC035, 0x8034, 0x41F4, 0x003C, 0xC1FC, 0x81FD, 0x403D,
0x01FF, 0xC03F, 0x803E, 0x41FE, 0x01FA, 0xC03A, 0x803B, 0x41FB, 0x0039, 0xC1F9, 0x81F8, 0x4038,
0x0028, 0xC1E8, 0x81E9, 0x4029, 0x01EB, 0xC02B, 0x802A, 0x41EA, 0x01EE, 0xC02E, 0x802F, 0x41EF,
0x002D, 0xC1ED, 0x81EC, 0x402C, 0x01E4, 0xC024, 0x8025, 0x41E5, 0x0027, 0xC1E7, 0x81E6, 0x4026,
0x0022, 0xC1E2, 0x81E3, 0x4023, 0x01E1, 0xC021, 0x8020, 0x41E0, 0x01A0, 0xC060, 0x8061, 0x41A1,
0x0063, 0xC1A3, 0x81A2, 0x4062, 0x0066, 0xC1A6, 0x81A7, 0x4067, 0x01A5, 0xC065, 0x8064, 0x41A4,
0x006C, 0xC1AC, 0x81AD, 0x406D, 0x01AF, 0xC06F, 0x806E, 0x41AE, 0x01AA, 0xC06A, 0x806B, 0x41AB,
0x0069, 0xC1A9, 0x81A8, 0x4068, 0x0078, 0xC1B8, 0x81B9, 0x4079, 0x01BB, 0xC07B, 0x807A, 0x41BA,
0x01BE, 0xC07E, 0x807F, 0x41BF, 0x007D, 0xC1BD, 0x81BC, 0x407C, 0x01B4, 0xC074, 0x8075, 0x41B5,
0x0077, 0xC1B7, 0x81B6, 0x4076, 0x0072, 0xC1B2, 0x81B3, 0x4073, 0x01B1, 0xC071, 0x8070, 0x41B0,
0x0050, 0xC190, 0x8191, 0x4051, 0x0193, 0xC053, 0x8052, 0x4192, 0x0196, 0xC056, 0x8057, 0x4197,
0x0055, 0xC195, 0x8194, 0x4054, 0x019C, 0xC05C, 0x805D, 0x419D, 0x005F, 0xC19F, 0x819E, 0x405E,
0x005A, 0xC19A, 0x819B, 0x405B, 0x0199, 0xC059, 0x8058, 0x4198, 0x0188, 0xC048, 0x8049, 0x4189,
0x004B, 0xC18B, 0x818A, 0x404A, 0x004E, 0xC18E, 0x818F, 0x404F, 0x018D, 0xC04D, 0x804C, 0x418C,
0x0044, 0xC184, 0x8185, 0x4045, 0x0187, 0xC047, 0x8046, 0x4186, 0x0182, 0xC042, 0x8043, 0x4183,
0x0041, 0xC181, 0x8180, 0x4040,
};

Uint16 CrcValueByteCalc(const Uint16 *data, Uint16 length)
{
    Uint16 crcValue = 0xFFFF;
    Uint16 tmp;

    if (length == 0)
    {
        return (crcValue);
    }

    do
    {
        tmp = crc16Table[(crcValue ^ *data++) & 0x00FF];
        crcValue = ((tmp & 0x00FF) << 8) + ((tmp ^ crcValue) >> 8);
    }
    while (--length);

    return (crcValue);
}

/* 逐字抄自 src/function/f_main.c:854。注意 digit[0] 是**个位**, 不是最高位 ——
 * UpdateSciFormat 用 digit[commType] 索引 dspBaudRegData[], commType 是
 * MODBUS(0) / PROFIBUS(1), 所以它取的是个位 / 十位。抄错方向会让整条波特率
 * 选择路径落到别的行上, 而两侧仍然一致 (同一个错替身), 测试台看不出问题。 */
void GetNumberDigit1(Uint16 digit[5], Uint16 number)
{
    Uint16 tmp;

    tmp = (Uint16)((Uint32)number * 52429 >> 19);
    digit[0] = number - tmp * 10;
    number = tmp;

    tmp = (Uint16)((Uint32)number * 52429 >> 19);
    digit[1] = number - tmp * 10;
    number = tmp;

    tmp = (Uint16)((Uint32)number * 52429 >> 19);
    digit[2] = number - tmp * 10;
    number = tmp;

    tmp = (Uint16)((Uint32)number * 52429 >> 19);
    digit[3] = number - tmp * 10;
    number = tmp;

    tmp = (Uint16)((Uint32)number * 52429 >> 19);
    digit[4] = number - tmp * 10;
}
