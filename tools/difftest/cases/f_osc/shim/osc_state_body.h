/* Accessor bodies for the f_osc bench -- included once by ref_tu.c and once by
 * new_tu.c, with OSC_SIDE defined to `ref` or `new` beforehand.
 *
 * The bodies are identical because the objects they name are identical: only
 * the three externally-linked functions are renamed in the two translation
 * units (OscSciFunction / OscControlRead / OscControlWriter), and those are not
 * touched here.  Writing the body twice by hand would let the two copies drift,
 * and a drifted accessor reports a mismatch that is really a typo in the
 * harness -- the worst kind of false positive, because it looks like a real
 * behavioural difference.
 *
 * Precondition: the module source has already been included, so OscCongData /
 * OscDataBufA / OscDataBufB / OscConFrameBuf / BaudCfg / SciTxBufStr /
 * SciRxBufStr and the type `uint8` are all in scope.
 */

#define OSC_PASTE2(a, b) a##b
#define OSC_PASTE(a, b)  OSC_PASTE2(a, b)

#define OSC_FN_GET OSC_PASTE(OSC_SIDE, _state_get)
#define OSC_FN_SETF OSC_PASTE(OSC_SIDE, _set_frame)
#define OSC_FN_SETD OSC_PASTE(OSC_SIDE, _set_rx_done)
#define OSC_FN_NORM OSC_PASTE(OSC_SIDE, _norm_buf)
#define OSC_FN_RXDIR OSC_PASTE(OSC_SIDE, _call_scidatarx)

/* `buf` is a uint8* into one of three objects, or NULL, or something else.
 * Compare it as (kind, offset) instead of as an address.  The comparisons go
 * through unsigned long long so no pointer-vs-pointer ordering is relied on. */
static void OSC_FN_NORM(const uint8 *p, unsigned short *kind, long long *off)
{
    unsigned long long v = (unsigned long long)p;
    unsigned long long b;

    *kind = 0;
    *off  = 0;
    if (p == 0)
    {
        return;
    }

    /* 偏移以**元素**为单位, 不是字节。宿主上 uint8 是 2 字节 (char->short),
     * 用字节数会让"前进 4 个通道"看起来是 8, 把正确的实现判成 BENCH-BUG。 */
    b = (unsigned long long)(const uint8 *)&OscDataBufA;
    if ((v >= b) && (v < b + sizeof(OscDataBufA)))
    {
        *kind = 1;
        *off  = (long long)((v - b) / sizeof(uint8));
        return;
    }

    b = (unsigned long long)(const uint8 *)&OscDataBufB;
    if ((v >= b) && (v < b + sizeof(OscDataBufB)))
    {
        *kind = 2;
        *off  = (long long)((v - b) / sizeof(uint8));
        return;
    }

    b = (unsigned long long)(const uint8 *)&OscConFrameBuf;
    if ((v >= b) && (v < b + sizeof(OscConFrameBuf)))
    {
        *kind = 3;
        *off  = (long long)((v - b) / sizeof(uint8));
        return;
    }

    *kind = 4;
    *off  = 0;
}

void OSC_FN_GET(struct OSC_STATE *s)
{
    const unsigned short *p;
    int i;

    p = (const unsigned short *)&OscCongData;
    for (i = 0; i < OSC_CON_N; i++)
    {
        s->con[i] = p[i];
    }

    p = (const unsigned short *)&OscDataBufA;
    for (i = 0; i < OSC_BUF_N; i++)
    {
        s->bufA[i] = p[i];
    }

    p = (const unsigned short *)&OscDataBufB;
    for (i = 0; i < OSC_BUF_N; i++)
    {
        s->bufB[i] = p[i];
    }

    p = (const unsigned short *)&OscConFrameBuf[0];
    for (i = 0; i < OSC_FRAME_N; i++)
    {
        s->frame[i] = p[i];
    }

    s->baudCfg = (unsigned short)BaudCfg;

    s->txBusy = (unsigned short)SciTxBufStr.busy;
    s->txLen  = (unsigned short)SciTxBufStr.len;
    s->txFlag = (unsigned short)SciTxBufStr.rxflag;
    OSC_FN_NORM(SciTxBufStr.buf, &s->txKind, &s->txOff);

    s->rxBusy = (unsigned short)SciRxBufStr.busy;
    s->rxLen  = (unsigned short)SciRxBufStr.len;
    s->rxFlag = (unsigned short)SciRxBufStr.rxflag;
    OSC_FN_NORM(SciRxBufStr.buf, &s->rxKind, &s->rxOff);
}

void OSC_FN_SETF(const unsigned short *f, int n)
{
    int i;

    for (i = 0; (i < n) && (i < OSC_FRAME_N); i++)
    {
        OscConFrameBuf[i] = (uint8)f[i];
    }
}

/* Exactly what two sciaRxFifoIsr calls leave behind when the frame length is 8:
 * eight bytes written, `len` decremented to 0, `busy` cleared, `rxflag` set to
 * 0x01 (the ISR writes 0x01 on the non-error path).  `buf` ends up eight
 * elements past the start of the frame buffer. */
void OSC_FN_SETD(void)
{
    SciRxBufStr.len    = 0;
    SciRxBufStr.busy   = 0;
    SciRxBufStr.rxflag = 0x01;
    SciRxBufStr.buf    = &OscConFrameBuf[8];
}

/* SciDataRx 直接驱动。
 *
 * 生产代码里它只有一个调用点, 而且 timeout 传的是常量 0 —— 于是
 * `(count > timeOut) && (timeOut != 0)` 那个超时分支在整机上是**不可达**的,
 * 里面四条语句(清 busy / 关 RXENA / 写 SCIFFRX / 返回 FALSE)一条都跑不到,
 * 针对它们的变异只能全部 MISSED, 看起来像"测试台没覆盖", 实际是"常量把分支
 * 焊死了"。
 *
 * 处置照 MEMORY 里的老办法: **把常量变成输入**。SciDataRx 是 static, 驱动叫不到,
 * 但本 TU include 了源码, 所以在这里开一个转发口, 用非零 timeout 直接调它。
 * 这不算伪造状态 —— timeout 本来就是形参, 非零值是它契约里明写的用法, 只是这
 * 一份固件恰好只用了 0。
 *
 * 注意: 这个口子会改写 SciDataRx 里的 count / timeOut 两个函数内 static, 所以
 * 驱动把它安排在**最后一个相位**, 不让它污染前面那些"自然轨迹"的用例。 */
unsigned short OSC_FN_RXDIR(unsigned short len, unsigned short timeout)
{
    return (unsigned short)SciDataRx((uint8 *)(&OscConFrameBuf), (uint8)len,
                                     (uint8)timeout);
}

#undef OSC_FN_GET
#undef OSC_FN_SETF
#undef OSC_FN_SETD
#undef OSC_FN_NORM
#undef OSC_FN_RXDIR
#undef OSC_PASTE
#undef OSC_PASTE2
