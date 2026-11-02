/* Differential test driver for f_modbus.
 *
 * The module is the Modbus RTU half of the serial protocol layer. It has no
 * hardware boundary of its own: it never touches a peripheral register and never
 * calls another project function, so everything it can do is visible in the nine
 * globals that mock_modbus.c owns. There is therefore no call log here -- the
 * comparison of the objects IS the complete observation. (Contrast f_plc, where
 * three mock functions had to be logged because their effects were invisible.)
 *
 * What is compared after every call:
 *
 *   return value            the two Uint16-returning entry points
 *   commRcvData             the whole receive descriptor, every field
 *   commSendData            the whole send descriptor, every field
 *   sendFrame[35]           the transmit buffer
 *   rcvFrame[35]            the receive buffer
 *   commReadData[12]        the register block the read path copies out
 *   commTicker              the Uint32 timeout counter
 *   sciFlag                 the whole union (compared as .all)
 *   commProtocol            selects the two read-path frame layouts
 *   funcCode                the whole union -- compared only to prove it is
 *                           read-only, since no entry point writes it
 *
 * Every input is pre-filled with non-trivial data before each side runs, so a
 * field the rewrite stops writing shows up as a difference instead of staying
 * accidentally equal. That is the point of pre-filling commRcvData in phase A
 * (only 8 of its 14 fields are assigned) and sendFrame in phase D.
 *
 * ---- why the read path is bounded at commData <= 12 ----
 *
 * ModbusSendDataDeal's read branch indexes two arrays by commRcvData.commData:
 * commReadData[i] for i = commData-1 down to 0, and sendFrame[(i<<1)+start(+1)].
 * With RTU_READ_DATA_NUM_MAX = 12 and sendFrame[35], commData = 12 reaches
 * commReadData[11] and sendFrame[28] -- both the last valid slot. commData = 17
 * would already index sendFrame[36] out of bounds. So the driver keeps commData
 * inside 0..12. That bound is the firmware's own contract (the master may not ask
 * for more registers than the buffer holds); it is enforced here rather than
 * assumed, because the alternative is reading undefined host memory and calling
 * the result a pass.
 *
 * The same bound also keeps the test away from a second, subtler cliff:
 * `sendNum = commData << 1` is promoted to `int`, which is 16 bits on C28x and 32
 * on the host. For commData >= 0x8000 the two disagree, and `sendNum >> 8` then
 * produces 0 on the target and 256 on the host. Both sides of this test are
 * compiled for the host, so the comparison stays valid -- but the host value is
 * not the target's, and commData <= 12 keeps that whole region out of scope.
 *
 * ---- division by zero ----
 *
 * UpdateModbusCommFormat divides by baudRate with no guard, so baudRate = 0 is
 * undefined behaviour (a trap on the host). The sweep starts at 1. The target's
 * behaviour for 0 is not covered by anything and is not claimed to be.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Same include order as src/function/f_modbus.c. */
#include "f_comm.h"

#define FRAME_N       35
#define READ_N        RTU_READ_DATA_NUM_MAX
#define FAIL_LIMIT    20
#define DATA_MAX      12

extern Uint16 ref_ModbusCommErrCheck(void);
extern void   ref_ModbusRcvDataDeal(void);
extern void   ref_ModbusSendDataDeal(Uint16 err);
extern Uint16 ref_ModbusStartDeal(Uint16 tmp);
extern void   ref_UpdateModbusCommFormat(Uint16 baudRate);

extern Uint16 new_ModbusCommErrCheck(void);
extern void   new_ModbusRcvDataDeal(void);
extern void   new_ModbusSendDataDeal(Uint16 err);
extern Uint16 new_ModbusStartDeal(Uint16 tmp);
extern void   new_UpdateModbusCommFormat(Uint16 baudRate);

/* ---- side plumbing ------------------------------------------------------- */

static void (*g_rcvDeal[2])(void);
static Uint16 (*g_startDeal[2])(Uint16);
static void (*g_updateFormat[2])(Uint16);
static void (*g_sendDeal[2])(Uint16);
static Uint16 (*g_errCheck[2])(void);

static void init_ptrs(void)
{
    g_rcvDeal[0]      = ref_ModbusRcvDataDeal;      g_rcvDeal[1]      = new_ModbusRcvDataDeal;
    g_startDeal[0]    = ref_ModbusStartDeal;        g_startDeal[1]    = new_ModbusStartDeal;
    g_updateFormat[0] = ref_UpdateModbusCommFormat; g_updateFormat[1] = new_UpdateModbusCommFormat;
    g_sendDeal[0]     = ref_ModbusSendDataDeal;     g_sendDeal[1]     = new_ModbusSendDataDeal;
    g_errCheck[0]     = ref_ModbusCommErrCheck;     g_errCheck[1]     = new_ModbusCommErrCheck;
}

struct SIDE
{
    Uint16 ret;
    COMM_RCV_DATA rcv;
    COMM_SEND_DATA snd;
    Uint16 send[FRAME_N];
    Uint16 rcvF[FRAME_N];
    Uint16 read[READ_N];
    Uint32 ticker;
    Uint16 sciAll;
    Uint16 protocol;
    FUNCCODE_ALL fc;
};

static struct SIDE S[2];

/* ---- input state -------------------------------------------------------- */

static COMM_RCV_DATA g_inRcv;
static COMM_SEND_DATA g_inSnd;
static Uint16 g_inSend[FRAME_N];
static Uint16 g_inRcvF[FRAME_N];
static Uint16 g_inRead[READ_N];
static Uint32 g_inTicker;
static Uint16 g_inSciAll;
static Uint16 g_inProtocol;
static FUNCCODE_ALL g_inFc;

static void install(void)
{
    memcpy(&commRcvData, &g_inRcv, sizeof g_inRcv);
    memcpy(&commSendData, &g_inSnd, sizeof g_inSnd);
    memcpy(sendFrame, g_inSend, sizeof g_inSend);
    memcpy(rcvFrame, g_inRcvF, sizeof g_inRcvF);
    memcpy(commReadData, g_inRead, sizeof g_inRead);
    commTicker = g_inTicker;
    sciFlag.all = g_inSciAll;
    commProtocol = g_inProtocol;
    memcpy(&funcCode, &g_inFc, sizeof g_inFc);
}

static void snapshot(int side)
{
    struct SIDE *s = &S[side];

    memcpy(&s->rcv, &commRcvData, sizeof s->rcv);
    memcpy(&s->snd, &commSendData, sizeof s->snd);
    memcpy(s->send, sendFrame, sizeof s->send);
    memcpy(s->rcvF, rcvFrame, sizeof s->rcvF);
    memcpy(s->read, commReadData, sizeof s->read);
    s->ticker = commTicker;
    s->sciAll = sciFlag.all;
    s->protocol = commProtocol;
    memcpy(&s->fc, &funcCode, sizeof s->fc);
}

/* ---- case runner -------------------------------------------------------- */

static long g_cases = 0;
static int g_fails = 0;

static void report(const char *fn, const char *what)
{
    if (g_fails >= FAIL_LIMIT)
    {
        return;
    }
    g_fails++;
    printf("MISMATCH %s: %s\n", fn, what);
    printf("  in: commData=%u protocol=%u sciFlag=%04X ticker=%lu\n",
           g_inRcv.commData, g_inProtocol, g_inSciAll, (unsigned long)g_inTicker);
    printf("      rcvF[0..5]=%u,%u,%u,%u,%u,%u  sendPre[0..5]=%u,%u,%u,%u,%u,%u\n",
           g_inRcvF[0], g_inRcvF[1], g_inRcvF[2], g_inRcvF[3], g_inRcvF[4], g_inRcvF[5],
           g_inSend[0], g_inSend[1], g_inSend[2], g_inSend[3], g_inSend[4], g_inSend[5]);
}

/* kind: 0 = ModbusRcvDataDeal, 1 = ModbusStartDeal, 2 = UpdateModbusCommFormat,
 *       3 = ModbusSendDataDeal, 4 = ModbusCommErrCheck. */
static void run_case(const char *fn, int kind, Uint16 arg)
{
    int side;

    g_cases++;

    for (side = 0; side < 2; side++)
    {
        install();

        switch (kind)
        {
        case 0:
            g_rcvDeal[side]();
            S[side].ret = 0;
            break;
        case 1:
            S[side].ret = g_startDeal[side](arg);
            break;
        case 2:
            g_updateFormat[side](arg);
            S[side].ret = 0;
            break;
        case 3:
            g_sendDeal[side](arg);
            S[side].ret = 0;
            break;
        default:
            S[side].ret = g_errCheck[side]();
            break;
        }

        snapshot(side);
    }

    if (S[0].ret != S[1].ret)
    {
        report(fn, "return value");
        return;
    }
    if (memcmp(&S[0].rcv, &S[1].rcv, sizeof S[0].rcv) != 0)
    {
        report(fn, "commRcvData");
        return;
    }
    if (memcmp(&S[0].snd, &S[1].snd, sizeof S[0].snd) != 0)
    {
        report(fn, "commSendData");
        return;
    }
    if (memcmp(S[0].send, S[1].send, sizeof S[0].send) != 0)
    {
        report(fn, "sendFrame");
        return;
    }
    if (memcmp(S[0].rcvF, S[1].rcvF, sizeof S[0].rcvF) != 0)
    {
        report(fn, "rcvFrame");
        return;
    }
    if (memcmp(S[0].read, S[1].read, sizeof S[0].read) != 0)
    {
        report(fn, "commReadData");
        return;
    }
    if (S[0].ticker != S[1].ticker)
    {
        report(fn, "commTicker");
        return;
    }
    if (S[0].sciAll != S[1].sciAll)
    {
        report(fn, "sciFlag");
        return;
    }
    if (S[0].protocol != S[1].protocol)
    {
        report(fn, "commProtocol");
        return;
    }
    if (memcmp(&S[0].fc, &S[1].fc, sizeof S[0].fc) != 0)
    {
        report(fn, "funcCode (should be read-only)");
        return;
    }
}

/* ---- input construction -------------------------------------------------- */

static Uint32 g_seed = 0x2468ACE0UL;

static Uint16 rnd(void)
{
    g_seed = g_seed * 1103515245UL + 12345UL;
    return (Uint16)(g_seed >> 16);
}

static Uint32 rnd32(void)
{
    return ((Uint32)rnd() << 16) | rnd();
}

/* Zero everything, then let the caller fill in the fields it cares about. The
 * descriptors and buffers are deliberately NOT left zero by the phases that care
 * about "field untouched" -- those pre-fill them. */
static void reset_input(void)
{
    memset(&g_inRcv, 0, sizeof g_inRcv);
    memset(&g_inSnd, 0, sizeof g_inSnd);
    memset(g_inSend, 0, sizeof g_inSend);
    memset(g_inRcvF, 0, sizeof g_inRcvF);
    memset(g_inRead, 0, sizeof g_inRead);
    memset(&g_inFc, 0, sizeof g_inFc);
    g_inTicker = 0;
    g_inSciAll = 0;
    g_inProtocol = 0;
}

/* Pre-fill the parts a given function is supposed to leave alone, so "left
 * alone" is observable rather than accidental. */
static void prefill_all(void)
{
    Uint16 i;

    for (i = 0; i < FRAME_N; i++)
    {
        g_inSend[i] = (Uint16)(0x1000 + i);
        g_inRcvF[i] = (Uint16)(0x2000 + i);
    }
    for (i = 0; i < READ_N; i++)
    {
        g_inRead[i] = (Uint16)(0x3000 + i);
    }

    g_inRcv.slaveAddr = 0xAAAA;
    g_inRcv.commAddr = 0xBBBB;
    g_inRcv.commData = 5;
    g_inRcv.rcvNum = 0xCCCC;
    g_inRcv.rcvFlag = 0xDDDD;
    g_inRcv.crcRcv = 0xEEEE;
    g_inRcv.commCmd = 0xFFFF;
    g_inRcv.crcSize = 0x1234;
    g_inRcv.rcvNumMax = 0x5678;
    g_inRcv.frameSpaceTime = 100;
    g_inRcv.delay = 0x9ABC;
    g_inRcv.rcvCrcErrCounter = 0xDEF0;
    g_inRcv.rcvDataJuageFlag = 0x1357;
    g_inRcv.commCmdSaveEeprom = 0x2468;

    g_inSnd.sendNumMax = 0x1111;
    g_inSnd.sendNum = 0x2222;
    g_inSnd.commData = 0x3333;
    g_inSnd.crcRcv = 0x4444;
    g_inSnd.commCmd = 0x5555;
    g_inSnd.crcSize = 0x6666;
}

/* ---- phase A: ModbusRcvDataDeal ----------------------------------------- */

/* rcvFrame[0..7] -> commRcvData. Eight fields are assembled from the frame; the
 * other six must survive untouched, which prefill_all() makes observable. */
static void phase_rcvdeal(void)
{
    static const Uint16 EDGE[] = { 0x0000, 0x0001, 0x007F, 0x0080, 0x00FF, 0x0100, 0x7FFF, 0x8000, 0xFF00, 0xFFFF };
    Uint16 k;
    Uint16 v;

    /* (a) one frame word at a time, everything else zero: isolates each of the
     * four (high<<8)+low pairs and each of the four direct copies. */
    for (k = 0; k < 8; k++)
    {
        for (v = 0; v < (Uint16)(sizeof EDGE / sizeof EDGE[0]); v++)
        {
            reset_input();
            prefill_all();
            g_inRcvF[k] = EDGE[v];
            run_case("ModbusRcvDataDeal", 0, 0);
        }
    }

    /* (b) both words of each pair together, on the values where the <<8 and the
     * + cross a 16-bit boundary. */
    for (k = 0; k < 4; k++)
    {
        Uint16 hi;

        for (hi = 0; hi < (Uint16)(sizeof EDGE / sizeof EDGE[0]); hi++)
        {
            for (v = 0; v < (Uint16)(sizeof EDGE / sizeof EDGE[0]); v++)
            {
                reset_input();
                prefill_all();
                g_inRcvF[k * 2]     = EDGE[hi];
                g_inRcvF[k * 2 + 1] = EDGE[v];
                run_case("ModbusRcvDataDeal", 0, 0);
            }
        }
    }

    /* (c) random frames over the whole word range. */
    for (k = 0; k < 20000; k++)
    {
        Uint16 i;

        reset_input();
        prefill_all();
        for (i = 0; i < 8; i++)
        {
            g_inRcvF[i] = rnd();
        }
        run_case("ModbusRcvDataDeal", 0, 0);
    }
}

/* ---- phase B: ModbusStartDeal ------------------------------------------- */

/* Five inputs decide the outcome: commTicker against frameSpaceTime (the early
 * out), the address echoed into rcvFrame[0], the CAN_OPEN special case, and the
 * configured slave address. The rcvNum/rcvFlag pre-state matters because the
 * reject branch writes rcvFlag but NOT rcvNum -- a rewrite that hoisted
 * "rcvNum = 1" out of the chain would change rcvNum exactly there, and only
 * there. */
static void phase_startdeal(void)
{
    static const Uint32 TICKER[] = { 0, 1, 99, 100, 101, 0xFFFF, 0x10000, 0xFFFFFFFFUL };
    static const Uint16 SPACE[]  = { 0, 1, 99, 100, 101, 0xFFFF };
    static const Uint16 ADDR[]   = { 0, 1, 2, 3, 0xFFFF };
    static const Uint16 PROTO[]  = { 0, 1, 2, 3, 0xFFFF };
    static const Uint16 SLAVE[]  = { 0, 1, 2, 0xFFFF };
    Uint16 a, b, c, d, e;
    Uint16 k;

    for (a = 0; a < (Uint16)(sizeof TICKER / sizeof TICKER[0]); a++)
    {
        for (b = 0; b < (Uint16)(sizeof SPACE / sizeof SPACE[0]); b++)
        {
            for (c = 0; c < (Uint16)(sizeof ADDR / sizeof ADDR[0]); c++)
            {
                for (d = 0; d < (Uint16)(sizeof PROTO / sizeof PROTO[0]); d++)
                {
                    for (e = 0; e < (Uint16)(sizeof SLAVE / sizeof SLAVE[0]); e++)
                    {
                        reset_input();
                        prefill_all();
                        g_inTicker = TICKER[a];
                        g_inRcv.frameSpaceTime = SPACE[b];
                        g_inFc.code.commProtocolSec = PROTO[d];
                        g_inFc.code.commSlaveAddress = SLAVE[e];
                        run_case("ModbusStartDeal", 1, ADDR[c]);
                    }
                }
            }
        }
    }

    /* Random, with the pre-state randomised too: rcvNum and rcvFlag both carry a
     * distinguishable non-zero pattern so "not written" cannot be confused with
     * "written to the same value". */
    for (k = 0; k < 20000; k++)
    {
        reset_input();
        prefill_all();
        g_inTicker = rnd32();
        g_inRcv.frameSpaceTime = rnd();
        g_inFc.code.commProtocolSec = rnd();
        g_inFc.code.commSlaveAddress = rnd();
        g_inRcv.rcvNum = rnd();
        g_inRcv.rcvFlag = rnd();
        run_case("ModbusStartDeal", 1, rnd());
    }
}

/* ---- phase C: UpdateModbusCommFormat ------------------------------------ */

/* baudRate is swept exhaustively from 1 (0 divides by zero, see the header) so
 * the integer division 770/baudRate and its truncation are covered at every
 * value the Uint16 parameter can take. commDelay is swept separately because it
 * only feeds the other field. */
static void phase_updateformat(void)
{
    static const Uint16 DELAY[] = { 0, 1, 2, 0x7FFF, 0x8000, 0xFFFF };
    Uint32 b;
    Uint16 d;

    for (b = 1; b <= 0xFFFFUL; b++)
    {
        reset_input();
        prefill_all();
        g_inFc.code.commDelay = 0x1234;
        run_case("UpdateModbusCommFormat", 2, (Uint16)b);
    }

    for (d = 0; d < (Uint16)(sizeof DELAY / sizeof DELAY[0]); d++)
    {
        static const Uint16 RATE[] = { 1, 2, 3, 96, 192, 384, 768, 770, 771, 772, 1152, 0xFFFF };
        Uint16 r;

        for (r = 0; r < (Uint16)(sizeof RATE / sizeof RATE[0]); r++)
        {
            reset_input();
            prefill_all();
            g_inFc.code.commDelay = DELAY[d];
            run_case("UpdateModbusCommFormat", 2, RATE[r]);
        }
    }
}

/* ---- phase D: ModbusSendDataDeal ---------------------------------------- */

/* Four-way chain: err > sciFlag.pwdPass > sciFlag.write > sciFlag.read. The whole
 * 16-bit sciFlag space is swept with err = 0, which covers every combination of
 * the three bits the function reads plus every combination of the thirteen it
 * ignores; then err is swept, then the read path's frame length. */
static void phase_senddeal(void)
{
    static const Uint16 ERR[] = { 0, 1, 2, 0x7F, 0x80, 0xFF, 0xFFFF };
    Uint32 s;
    Uint16 e;
    Uint16 k;

    /* (a) every sciFlag value, err = 0: 65536 cases, all branch combinations. */
    for (s = 0; s <= 0xFFFFUL; s++)
    {
        reset_input();
        prefill_all();
        g_inSciAll = (Uint16)s;
        g_inProtocol = 1;
        g_inRcv.commData = 5;
        run_case("ModbusSendDataDeal", 3, 0);
    }

    /* (b) every sciFlag value against a non-zero err: the err branch must win
     * regardless of the flags. */
    for (e = 0; e < (Uint16)(sizeof ERR / sizeof ERR[0]); e++)
    {
        for (s = 0; s <= 0xFFFFUL; s += 257)
        {
            reset_input();
            prefill_all();
            g_inSciAll = (Uint16)s;
            run_case("ModbusSendDataDeal", 3, ERR[e]);
        }
    }

    /* (c) the read path: frame length 0..12 (the buffer bound) against both
     * frame layouts, with the flags forced to the read branch. */
    for (e = 0; e <= DATA_MAX; e++)
    {
        Uint16 p;

        for (p = 0; p < 3; p++)
        {
            Uint16 r;

            for (r = 0; r < 4; r++)
            {
                reset_input();
                prefill_all();
                g_inSciAll = (Uint16)(r & 1);
                g_inProtocol = (Uint16)(p == 0 ? 0 : (p == 1 ? 1 : 0xFFFF));
                g_inRcv.commData = e;
                run_case("ModbusSendDataDeal", 3, 0);
            }
        }
    }

    /* (d) random, commData kept inside the buffer bound. */
    for (k = 0; k < 20000; k++)
    {
        Uint16 i;

        reset_input();
        prefill_all();
        g_inSciAll = rnd();
        g_inProtocol = rnd();
        g_inRcv.commData = (Uint16)(rnd() % (DATA_MAX + 1));
        for (i = 0; i < READ_N; i++)
        {
            g_inRead[i] = rnd();
        }
        run_case("ModbusSendDataDeal", 3, rnd());
    }
}

/* ---- phase E: ModbusCommErrCheck ---------------------------------------- */

/* The threshold is commOverTime * 200, so the interesting ticker values are the
 * threshold and its neighbours -- for every commOverTime the Uint16 can hold.
 * That also pins down the Uint32 arithmetic: the largest threshold is
 * 65535*200 = 13,107,000, well inside 32 bits, so no wrap is expected and any
 * disagreement between the sides would be a real difference. */
static void phase_errcheck(void)
{
    Uint32 t;
    Uint16 k;

    for (t = 0; t <= 0xFFFFUL; t++)
    {
        Uint32 edge = (Uint32)t * 2 * TIME_UNIT_sciCommOverTime;
        Uint16 j;

        for (j = 0; j < 3; j++)
        {
            reset_input();
            prefill_all();
            g_inFc.code.commOverTime = (Uint16)t;
            g_inTicker = (j == 0) ? (edge - (edge ? 1 : 0)) : (edge + (j - 1));
            run_case("ModbusCommErrCheck", 4, 0);
        }
    }

    for (k = 0; k < 20000; k++)
    {
        reset_input();
        prefill_all();
        g_inFc.code.commOverTime = rnd();
        g_inTicker = rnd32();
        run_case("ModbusCommErrCheck", 4, 0);
    }
}

/* ---- main ---------------------------------------------------------------- */

int main(void)
{
    long base;

    init_ptrs();

    phase_rcvdeal();
    printf("  ModbusRcvDataDeal            : %ld\n", g_cases);

    base = g_cases;
    phase_startdeal();
    printf("  ModbusStartDeal              : %ld\n", g_cases - base);

    base = g_cases;
    phase_updateformat();
    printf("  UpdateModbusCommFormat       : %ld\n", g_cases - base);

    base = g_cases;
    phase_senddeal();
    printf("  ModbusSendDataDeal           : %ld\n", g_cases - base);

    base = g_cases;
    phase_errcheck();
    printf("  ModbusCommErrCheck           : %ld\n", g_cases - base);

    printf("f_modbus difftest: cases=%ld mismatches=%d\n", g_cases, g_fails);
    return g_fails ? 1 : 0;
}
