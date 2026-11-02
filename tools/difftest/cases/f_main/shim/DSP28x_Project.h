/* Host-side replacement for DSP28x_Project.h -- f_main 用例私有。

 * f_main.c 第 2 行 `#include "f_main.h"`, 真 f_main.h 第 4 行再
 * `#include "f_funcCode.h"`, 后者第 5 行就是 `#include "DSP28x_Project.h"` ——
 * 所以这里既要把类型宽度钉死, 也要把本模块真正用到的外设模型挂上。

 * ---- 为什么钉宽度 ------------------------------------------------------------

 * 工程 DataTypeDef.h 把 `Uint16` 定义成 `unsigned int`: C28x 上 16 位, 宿主上 32 位。
 * 沿用真文件会让每个 u16 悄悄变宽, 溢出与截断点整体位移 —— 而本模块的
 * `ticker05msFuncEa[ticker] = ticker05msFuncEach * 3413 >> 11;` 正是靠 32 位算术
 * 再截到 16 位存回去, 宽度一变结论就没了。这里钉成 uint16_t。

 * ---- 为什么挂 SysCtrl / CpuTimers -------------------------------------------

 * InitDspFunc 写 SysCtrlRegs.PCLKCR0/PCLKCR1 的六七个时钟使能位;
 * GetTime() 展开成 `CpuTimer1.RegsAddr->TIM.all`。两者的位域布局与偏移必须是
 * 芯片的真布局。这两个头实测 MSVC 直接编得过 (纯结构体 + extern 声明 + 宏),
 * 由 real.txt 生成转发头。

 * 有意**不**挂 DSP2803x_Device.h (`extern cregister volatile unsigned int IFR;`) 与
 * DSP2803x_Examples.h (拉进 DSP2803x_DefaultISR.h 的 `__interrupt` 原型)。
 * 本模块用到的只有 DSP_CLOCK、BITn、EALLOW/EDIS 三样, 都在下面自己给。

 * ---- EALLOW / EDIS ----------------------------------------------------------

 * 真 Device.h 里它们是 `asm(" EALLOW")` / `asm(" EDIS")`, MSVC 不认。
 * InitDspFunc 成对调用它们把 SysCtrlRegs 的写保护打开再关上, 所以这里改成对一张
 * 共享计数表自增 —— 这样"改写把 EDIS 弄丢了"是可观测的, 而不是完全看不见。
 * 计数表由 mock_main.c 定义, 两侧共用; 快照后逐格比对。

 * ---- gHostMemory -----------------------------------------------------------

 * `getMomeryValue()` 的整个正文是
 *     a = (Uint16*)MEMORY_ADDRESS;   memoryValue = *a;
 * 其中 MEMORY_ADDRESS 是 `funcCode.code.memoryAddr` (一个 16 位**地址**)。
 * 目标端那条地址指向真实 RAM, 宿主上直接解引用 16 位整数当指针是未定义行为。
 * host_patch.json 把它改写成 `(Uint16*)(gHostMemory + MEMORY_ADDRESS)` ——
 * 一张 64K 项的 16 位表, 下标就是那个 16 位地址。语义(按下标取一个字)保住了,
 * 越界也变成可复现的确定行为。表本身由 driver 灌内容。
 */
#ifndef DSP28X_PROJECT_H
#define DSP28X_PROJECT_H

#include "DataTypeDef.h"

/* 真 DSP28x_Project.h 会拉 DSP2803x_Examples.h, 那里第 9 行是 `#define DSP_CLOCK 60`。
 * f_debug.h 的 TIME_10US / TIME_1MS 按它选值, 而 f_main.c 的 InitFuncCode 用
 * `baseTime - GetTime() >= (TIME_1MS << 1)` 决定要不要走每 2 ms 一次的那块。
 * 少了它 DSP_CLOCK 按 0 求值 -> TIME_1MS 未定义 -> 编不过。
 * 60 MHz 是这块板子的真实主频。 */
#define DSP_CLOCK   60

#ifndef DSP28_DATA_TYPES
#define DSP28_DATA_TYPES
typedef int16_t             int16;
typedef int32_t             int32;
typedef int64_t             int64;
typedef uint16_t            Uint16;
typedef uint32_t            Uint32;
typedef uint64_t            Uint64;
typedef float               float32;
typedef long double         float64;
#endif

/* Device.h 里的 BITn; 工程头文件到处在用。 */
#define BIT0    0x0001
#define BIT1    0x0002
#define BIT2    0x0004
#define BIT3    0x0008
#define BIT4    0x0010
#define BIT5    0x0020
#define BIT6    0x0040
#define BIT7    0x0080
#define BIT8    0x0100
#define BIT9    0x0200
#define BIT10   0x0400
#define BIT11   0x0800
#define BIT12   0x1000
#define BIT13   0x2000
#define BIT14   0x4000
#define BIT15   0x8000

#include "DSP2803x_SysCtrl.h"
#include "DSP2803x_CpuTimers.h"

/* CPU 控制序列 -> 共享计数表 (下标与真 Device.h 里那 7 个宏一一对应)。 */
extern volatile Uint16 gHostCpuCtl[7];

#define  EINT    (gHostCpuCtl[0]++)
#define  DINT    (gHostCpuCtl[1]++)
#define  ERTM    (gHostCpuCtl[2]++)
#define  DRTM    (gHostCpuCtl[3]++)
#define  EALLOW  (gHostCpuCtl[4]++)
#define  EDIS    (gHostCpuCtl[5]++)
#define  ESTOP0  (gHostCpuCtl[6]++)

/* getMomeryValue 的宿主地址空间; 见文件头。 */
#define HOST_MEMORY_WORDS   65536U
extern Uint16 gHostMemory[HOST_MEMORY_WORDS];

#endif
