/* Module-state snapshot for the f_osc differential bench.
 *
 * Why this exists
 * ---------------
 * f_osc.c keeps ALL of its working state in file-scope `static` objects:
 *
 *     OscCongData, OscDataBufA, OscDataBufB, OscConFrameBuf, BaudCfg,
 *     SciTxBufStr, SciRxBufStr
 *
 * None of them has external linkage, so the driver cannot name them.  Comparing
 * only the peripheral registers would leave the two 68-byte oscilloscope
 * buffers, the receive cursor and the configuration block entirely
 * unobserved -- and those are exactly where a rewrite goes wrong.
 *
 * The names ARE visible inside the translation unit that includes the source,
 * so ref_tu.c / new_tu.c each append an accessor that copies the objects out.
 * The source itself is untouched: no `static` is stripped, no declaration is
 * reordered.  Function-local statics stay private and are handled the usual
 * way -- both sides start from zero and run the identical call sequence, so
 * they evolve in lockstep while the implementations are equivalent.
 *
 * Why the fields are `unsigned short`
 * -----------------------------------
 * On C28x a `char` is 16 bits, so `uint8` (typedef'd as `unsigned char` in
 * f_osc.h) is a 16-bit unsigned quantity and `uint8 x[10]` occupies ten words.
 * The module depends on that: `CrcValueByteCalc((const Uint16 *)OscConFrameBuf,
 * FC_FRAME_LEN-2)` reinterprets the byte array as words and reads six of them.
 * On a host with 8-bit char the same cast would read twelve bytes from a
 * ten-byte object -- two bytes past the end -- and the CRC would then depend on
 * whatever happened to follow it, which is not the same thing in the two
 * translation units.
 *
 * ref_tu.c / new_tu.c therefore pin `char` to `short` before including the
 * source (see the comment there), which makes `uint8` 16-bit and reproduces the
 * target layout exactly.  This header is included by the driver too, which does
 * NOT do that substitution, so every field below is spelled as an explicit
 * `unsigned short` rather than `uint8`.  Both sides then agree on the layout
 * without the driver having to pretend it is a C28x.
 */
#ifndef OSC_STATE_H
#define OSC_STATE_H

/* DSP_OSC_CON_DATA: status baudRate interval runContinue ch1..ch4 chSum */
#define OSC_CON_N        9

/* OSC_DATA_TYPE: frameHead1 frameHead2 frameNum oscDataBuf[64] check rwPI full */
#define OSC_BUF_N        70

/* uint8 OscConFrameBuf[10] */
#define OSC_FRAME_N      10

struct OSC_STATE
{
    unsigned short con[OSC_CON_N];
    unsigned short bufA[OSC_BUF_N];
    unsigned short bufB[OSC_BUF_N];
    unsigned short frame[OSC_FRAME_N];
    unsigned short baudCfg;

    /* SCI_RT_CON_DATA { buf, len, busy, rxflag } -- `buf` is a pointer, so it
     * is emitted as (which object, offset inside it).  A raw address would
     * differ between the two sides by construction. */
    unsigned short txBusy, txLen, txFlag, txKind;
    long long      txOff;
    unsigned short rxBusy, rxLen, rxFlag, rxKind;
    long long      rxOff;
};

void ref_state_get(struct OSC_STATE *s);
void new_state_get(struct OSC_STATE *s);

/* Pre-loading the receive frame, and pre-loading "a frame has just been
 * received", are the only two write-back operations the driver gets.
 *
 * Rationale: on the target the frame bytes are produced by sciaRxFifoIsr, one
 * SCIRXBUF read per byte.  This bench models SCIRXBUF as ordinary memory (see
 * mock_osc.c), so the ISR reads the SAME value four times in a row -- the frame
 * it leaves behind always has bytes 0..3 equal and bytes 4..7 equal.  That is
 * enough to prove the ISR's own bookkeeping, but it cannot produce an arbitrary
 * control frame, and without an arbitrary frame the whole command dispatcher in
 * SciDataRxDeal is unreachable.
 *
 * So the dispatcher is driven the way the hardware drives it -- "the ISR has
 * already filled the buffer, len has reached 0" -- by setting those exact
 * fields to the exact values two ISR calls leave behind.  That is a reachable
 * state, not a fabricated one; what it does NOT prove is the byte-by-byte
 * handoff inside the ISR, which is covered separately by calling the ISR
 * through the PIE vector.  Both halves are needed and both are present. */
void ref_set_frame(const unsigned short *f, int n);
void new_set_frame(const unsigned short *f, int n);
void ref_set_rx_done(void);
void new_set_rx_done(void);

/* SciDataRx 直接驱动。
 *
 * 生产代码只有一处调用 `SciDataRx(buf, 8, 0)` —— timeout 是常量 0, 所以
 * `(count > timeOut) && (timeOut != 0)` 的超时分支在整机上不可达。里面四条
 * 语句(清 busy / 关 RXENA / 写 SCIFFRX / 返回 FALSE)不驱动就永远测不到。
 * 把常量变成输入: 用非零 timeout 直接调它。它会改写该函数的 count/timeOut
 * 两个函数内 static, 所以驱动把它放在最后一个相位。 */
unsigned short ref_call_scidatarx(unsigned short len, unsigned short timeout);
unsigned short new_call_scidatarx(unsigned short len, unsigned short timeout);

#endif
