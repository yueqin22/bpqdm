"""生成 f_p2p 用例的宿主侧 shim 头文件。

f_p2p.c 会 include 8 个头: f_ui.h / f_menu.h / f_frqSrc.h / f_comm.h /
f_runSrc.h / f_io.h / f_error.h / f_p2p.h。前 7 个会把整套 DSP 外设模型拉进来,
宿主上编不了; 最后一个 (f_p2p.h) 只定义 P2P_DATA_STRUCT 和几个常量, 可以**直接用真的**,
这样结构体布局就是权威的, 不是我抄的。

做法: 为前 7 个名字各生成一个只 include "p2p_common.h" 的空壳; shim 目录在
include 路径最前, 所以这 7 个被替换, f_p2p.h 仍然解析到 include/function/ 里的真货。
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
SHIM = os.path.join(HERE, 'shim')

COMMON = '''/* Host-side shim for the f_p2p differential test.
 *
 * Widths are pinned to exact-width types, NOT to the original typedefs.
 * Reason: the original has `typedef unsigned int Uint16`. On C28x `unsigned int`
 * is 16 bits, so `long / Uint16` converts the divisor to long and does a SIGNED
 * division. On the host `unsigned int` is 32 bits, so `long / unsigned int`
 * converts the dividend to unsigned long and does an UNSIGNED division -- a
 * different result for negative dividends. Using uint16_t here restores the
 * C28x promotion: uint16_t promotes to int, so the division stays signed.
 */
#ifndef P2P_COMMON_H
#define P2P_COMMON_H

#include "DataTypeDef.h"

typedef uint16_t       Uint16;
typedef uint32_t       Uint32;
typedef int16_t        int16;
typedef int32_t        int32;

/* f_p2p.h gates its struct behind this; must be defined before it is included. */
#define DEBUG_F_P2P_CTRL 1

#define ERROR_COMM          16
#define COMM_ERROR_MODBUS   1

struct FUNCCODE_P2P_MOCK {
    Uint16 p2pEnable;
    Uint16 p2pTypeSel;
    Uint16 p2pSendDataSel;
    Uint16 p2pRevDataSel;
    Uint16 p2pRevOffset;
    Uint16 p2pRevGain;
    Uint16 p2pOverTime;
    Uint16 p2pSendPeriod;
};
struct FUNCCODE_MOCK {
    struct FUNCCODE_P2P_MOCK code;
};
extern struct FUNCCODE_MOCK funcCode;

extern Uint16 itDisp;
extern int32  frqRun;
extern int32  frqAimTmp;
extern Uint16 frqFdb;
extern Uint16 maxFrq;
extern Uint16 errorOther;
extern Uint16 errorInfo;

#endif
'''

THIN = ['f_ui.h', 'f_menu.h', 'f_frqSrc.h', 'f_comm.h',
        'f_runSrc.h', 'f_io.h', 'f_error.h']

os.makedirs(SHIM, exist_ok=True)
with open(os.path.join(SHIM, 'p2p_common.h'), 'w', encoding='utf-8') as f:
    f.write(COMMON)
for n in THIN:
    with open(os.path.join(SHIM, n), 'w', encoding='utf-8') as f:
        f.write('#include "p2p_common.h"\n')
print('written: shim/p2p_common.h + %d thin headers' % len(THIN))
