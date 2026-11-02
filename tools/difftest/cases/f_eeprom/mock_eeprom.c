/* f_eeprom 差分测试的共享 mock —— 两侧 (ref / new) 看到同一份状态。

 * 这里定义的是 f_eeprom.c **引用但不拥有**的东西: 它们由别的模块定义
 * (f_funcCode.c / f_main.c / f_fcDeal.c / f_error.c ...), 在本用例里不参与编译,
 * 所以由本文件提供唯一一份。两侧共享同一份, 于是"改写有没有多读一个字段 /
 * 少写一个字段"是可观测的。

 * 清单不是人眼数的, 是 `nm2000 --undefined-only build/Debug/f_eeprom.obj` 读出来
 * 的 12 个外部引用 (见 README 的符号核对一节), 逐个对照本文件。
 */
#include <string.h>

#include "mock_eeprom.h"

/* 本文件定义 GpioDataRegs 本体, 所以必须**关掉** shim/DSP2803x_Gpio.h 里那个
 * "每次访问都先跑 gpio_apply()" 的宏 —— 否则 gpio_apply() 自己写
 * `GpioDataRegs.GPBSET.all = 0` 会被展开成又一次 gpio_data_access() 调用,
 * 无限递归 (MSVC C4717 只是警告, 跑起来会爆栈)。
 *
 * 交付源码 (ref_tu.c / new_tu.c 那一边) 仍然看到宏 —— 那正是我们要的:
 * 被测模块的每一次 GPBDAT 读都会先应用待处理的 set/clear。 */
#ifdef GpioDataRegs
#undef GpioDataRegs
#endif

/* ---- 外设寄存器 -----------------------------------------------------------
 *
 * 外设头由 real.txt 转发 (DSP2803x_I2c.h / DSP2803x_Gpio.h), 所以下面这几个
 * 结构体是**芯片真身** —— I2C_REGS 的字段顺序与宽度、GPB*_REG 的位域位置全都
 * 与目标端一致。这一点对本模块是硬要求, 因为:
 *
 *   * ReadFuncCode 把 I2cMsg.buffer[] 的两个字节拼成一个 Uint16
 *     (`(buffer[i<<1] << 8) + buffer[(i<<1)+1]`), 而 buffer[] 由 I2cIntDeal 从
 *     `I2caRegs.I2CDRR` 填充; I2CDRR 的宽度错一位, 拼接结果就整体错。
 *   * I2caRegs.I2CMDR.bit.STP / I2CSTR.bit.BB / I2CSTR.bit.NACK 都是**位域读**。
 *     RwI2cBus 的头两条 if 就是读它们, 位域位置不对则分支走向不对。
 *   * I2cDealBeforeInit 读写 GpioDataRegs.GPBDAT/GPBSET/GPBCLEAR 与
 *     GpioCtrlRegs.GPBDIR 的 GPIO32/GPIO33 位。这些是**真正的可观测副作用**:
 *     位操作序列 (SDA/SCL 的翻转次序) 就是软件模拟 I2C 协议本身。
 *
 * 一句话: 这里不 mock 外设的"行为", 只提供可以被读写、可以被快照的真实布局。
 * 谁是 (side) effect 由 driver 逐位比较。
 */
volatile struct I2C_REGS        I2caRegs;
volatile struct GPIO_CTRL_REGS  GpioCtrlRegs;
volatile struct GPIO_DATA_REGS  GpioDataRegs;

/* ---- GPIO 寄存器写语义 ----------------------------------------------------
 *
 * GPBSET / GPBCLEAR 是"写 1 生效"的侧写寄存器。真芯片上, 写 1 到
 * GPBSET.GPIO32 之后读 GPBDAT.GPIO32 会看到 1; 写 GPBCLEAR 则看到 0。
 * 普通内存变量不自动实现, 所以 shim/DSP2803x_Gpio.h 把 `GpioDataRegs` 重定义
 * 成一个函数调用, 每次访问都先执行下面的 gpio_apply()。
 *
 * 应用规则 (逐位独立):
 *   对 GPBDAT 的每一位 n:
 *     if (GPBSET.bit_n)   { GPBDAT.bit_n = 1; GPBSET.bit_n = 0; }
 *     else if (GPBCLEAR.bit_n) { GPBDAT.bit_n = 0; GPBCLEAR.bit_n = 0; }
 *   只有 GPIO32 / GPIO33 两位在本模块里被 set/clear, 但循环按整字做, 免得
 *   将来别的位被用到时这里悄悄漏掉。
 *
 * "输入方向" 的特殊处理 (这**不是**随便规定的):
 *   芯片上 GPBDAT 读回的是引脚电平。当 GPBDIR.bit_n == 0 (输入) 时, 引脚电平
 *   由外部决定, GPBCLEAR 清的是**输出锁存**, 改变不了引脚 —— 于是 GPBCLEAR
 *   写 1 之后读回的值仍是外部电平。gHostPinExtLevel32 就是 GPIO32 的外部电平。
 *   当 GPBDIR.bit_n == 1 (输出) 时, 引脚电平 == 输出锁存, 所以 GPBCLEAR 生效。
 *
 * 这条规则是 I2cDealBeforeInit 的两次 `SDA` 读可分辨的全部依据:
 *   第一次读在 SdaIoAsInput() 之后 -> 输入方向, 读外部电平。
 *   第二次读也在 SdaIoAsInput() 之后 -> 同样是外部电平。两次读之间隔了一次
 *   SdaIoAsOutput() + I2cDataLow() + SdaIoAsInput(), 于是"若第一次为 1,
 *   则把 SDA 拉低再放开" 这个握手动作的**结果**由外部电平决定。
 */
Uint16 gHostPinExtLevel32;

static void gpio_apply(void)
{
    Uint32 set;
    Uint32 clr;
    Uint32 d;
    unsigned b;

    set = GpioDataRegs.GPBSET.all;
    clr = GpioDataRegs.GPBCLEAR.all;
    d   = GpioDataRegs.GPBDAT.all;

    /* GPIO32 = bit 0, GPIO33 = bit 1 (见 struct GPBDAT_BITS)。
     * 只处理这两位 —— 其余位在本模块里从不被 set/clear, 且 GPBDAT 的 rsvd
     * 位在真芯片上是只读的。 */
    for (b = 0; b < 2; b++)
    {
        Uint32 m = 1UL << b;
        unsigned isOut = (b == 0)
            ? (unsigned)GpioCtrlRegs.GPBDIR.bit.GPIO32
            : (unsigned)GpioCtrlRegs.GPBDIR.bit.GPIO33;
        unsigned ext = (b == 0) ? (unsigned)gHostPinExtLevel32 : 0u;

        if (set & m)
        {
            if (isOut)
            {
                d |= m;                       /* 输出方向: 锁存置 1 */
            }
            /* 输入方向: GPBSET 只影响内部上拉, 不改变引脚电平 */
        }
        else if (clr & m)
        {
            if (isOut)
            {
                d &= ~m;                      /* 输出方向: 锁存清 0 */
            }
            else
            {
                d = (d & ~m) | (ext ? m : 0u); /* 输入方向: 读回外部电平 */
            }
        }
        else
        {
            /* 没有待处理动作: 输入方向仍然读回外部电平。 */
            if (!isOut)
            {
                d = (d & ~m) | (ext ? m : 0u);
            }
        }
    }

    GpioDataRegs.GPBDAT.all   = d;
    GpioDataRegs.GPBSET.all   = 0;
    GpioDataRegs.GPBCLEAR.all = 0;
}

volatile struct GPIO_DATA_REGS *gpio_data_access(void)
{
    gpio_apply();
    return &GpioDataRegs;
}

/* ---- CPU 控制序列表 (DSP2803x_Device.h shim 把 EALLOW/EDIS 映到这上面) ----- */
volatile Uint16 gHostCpuCtl[7];

/* ---- DELAY_US 的调用记录 --------------------------------------------------
 *
 * 每次 DSP28x_usDelay(N) 累加 gHostDelaySum += N, 并 ++gHostDelayCalls。
 * 只记次数不够: DELAY_US(5) 与 DELAY_US(20000) 的 N 分别是 58 与 239994,
 * 差三个数量级, 记和才能分辨"参数被改了"与"调用被挪了位置"。
 *
 * N 的类型是 long (与真 DSP2803x_usDelay.asm 的 C 原型一致)。累加进 Uint32,
 * 溢出是**故意的**: 单次 I2CRcvByte 里 800 次 DELAY_US(5) 就有 46400,
 * 整个用例跑下来会绕几圈 —— 绕圈对两侧同样发生, 所以不影响比较。
 */
Uint32 gHostDelaySum;
Uint32 gHostDelayCalls;

void DSP28x_usDelay(long LoopCount)
{
    gHostDelaySum += (Uint32)LoopCount;
    gHostDelayCalls++;
}

/* ---- 被测模块引用的全局量 ------------------------------------------------- */

FUNCCODE_ALL funcCode;

Uint16 errorEeprom;
enum POWER_ON_STATUS powerOnStatus;
Uint16 aiaoChckReadOK;

/* ---- 两张替身表 ----------------------------------------------------------
 *
 * 由 build/_gen_eep_attr.py 生成到 eeprom_tables.inc。下面的编译期断言把
 * "生成器写的表长" 与 "真头文件算出的 FNUM_PARA / 无属性尾巴长度" 对账:
 * 任一侧改动而另一侧没跟上, 编译当场失败, 不用等人去数。
 *
 * 为什么表长必须是 909 而不是"随便给大点": GetEepromIndexFromFcIndex 的
 * `else` 分支按 fcIndex 直接索引这张表, **没有边界检查**。真表正好
 * FNUM_PARA 项, 给大就把"表到底多长"糊掉了, 用例会变成"用一张比真的更宽松的
 * 表去证明等价"。这里取真值, 越界由 driver 守住。
 *
 * fcNoAttri2Eeprom[] 真身也在 f_main.c, 索引是 (fcIndex - 909), 覆盖 fcIndex
 * 909.. 那一批无属性功能码。表长 8 = 真表里被本模块可达的那一段 (909..916);
 * 真表更长, 但 917 以上在本用例的输入空间之外 (driver 只喂 fcIndex < 917)。
 * 长度取真表**前缀**而不是随手 8 —— 生成器文件头记了为什么 4..7 要被二次占用。
 */
const FUNCCODE_ATTRIBUTE funcCodeAttribute[EEP_ATTR_N] =
{
#include "eeprom_tables.inc"
};

const Uint16 fcNoAttri2Eeprom[EEP_NOATTR_N] =
{
#include "eeprom_noattr.inc"
};

/* 编译期对账 (负长度数组 = 唯一不需要任何扩展的 C89 手法)。 */
typedef char eep_attr_len_must_equal_FNUM_PARA[
    (EEP_ATTR_N == 909) ? 1 : -1];
typedef char eep_noattr_len_must_be_8[
    (EEP_NOATTR_N == 8) ? 1 : -1];

/* ---- 三个"被调用"的外部函数 —— 记录型替身, 不是固定返回值 ----------------
 *
 * f_eeprom.c 里对它们的调用点是:
 *
 *   GetFuncCodeInit(fcIndex, 0)           EepromDeal 的 RESTORE_COMPANY_PARA 臂
 *   GetFuncCodeInit(fcIndex, 1)                       RESTORE_COMPANY_PARA_ALL 臂
 *   GetFuncCodeInitOriginal(fcIndex)                  WRITE_ALL 臂的 else
 *   RestoreCompanyParaOtherDeal()         UpdateIndexForEepromDeal 的收尾
 *
 * 固定返回值会让"参数传错了"完全不可见 —— 例如 `GetFuncCodeInit(fcIndex, 0)`
 * 被改写成 `(fcIndex, 1)`, 返回值一样, 差分测试一声不响。所以替身把
 * **参数与调用次数记下来**, 由 driver 在每一步之后比对两侧的调用轨迹。
 *
 * 返回值本身也做成参数的函数 (而不是常数), 于是"写回 funcCode.all[] 的值"
 * 同样带信息。
 */
Uint16 gMockGfciCalls;
Uint16 gMockGfciLastIndex;
Uint16 gMockGfciLastType;
Uint32 gMockGfciArgSum;

Uint16 gMockGfcioCalls;
Uint16 gMockGfcioLastIndex;

Uint16 gMockRcpoCalls;

void mock_calls_reset(void)
{
    gMockGfciCalls = 0;
    gMockGfciLastIndex = 0;
    gMockGfciLastType = 0;
    gMockGfciArgSum = 0;
    gMockGfcioCalls = 0;
    gMockGfcioLastIndex = 0;
    gMockRcpoCalls = 0;
}

Uint16 GetFuncCodeInit(Uint16 index, Uint16 type)
{
    gMockGfciCalls++;
    gMockGfciLastIndex = index;
    gMockGfciLastType = type;
    gMockGfciArgSum += (Uint32)index * 3u + (Uint32)type * 100003u;
    /* 返回值是参数的函数, 且与 type 有关 —— 见上面说明。 */
    return (Uint16)((index * 37u + type * 4099u + 0x5A5Au) & 0xFFFFu);
}

Uint16 GetFuncCodeInitOriginal(Uint16 index)
{
    gMockGfcioCalls++;
    gMockGfcioLastIndex = index;
    return (Uint16)((index * 101u + 0x1234u) & 0xFFFFu);
}

void RestoreCompanyParaOtherDeal(void)
{
    gMockRcpoCalls++;
}
