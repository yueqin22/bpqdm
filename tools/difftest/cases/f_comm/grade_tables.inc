/* 由 tools/difftest/gen_plc_grade_tables.py 生成, 不要手改。
 * 来源: src/function/f_funcCode.c 的 funcCodeGradeAll / funcCodeGradeSum,
 *       以及 include/function/f_funcCode.h 的 struct FUNCCODE_GROUP。
 * 重新生成: python tools/difftest/gen_plc_grade_tables.py
 * 过期校验: python tools/difftest/gen_plc_grade_tables.py --check
 *   (注意: 没有任何自动测试会跑 --check —— 它是手动工具。改过
 *    f_funcCode.c 的两张表或 f_funcCode.h 的成员表之后要自己跑一次。)
 */

/* GRADE_SUM_LIST 的每一项是 funcCodeGradeSum[] 里那个 GetCodeIndex() 的**实参**
 * 去掉 `funcCode.` 前缀之后的成员指示符。
 *
 * 只写这一份, 两处共用:
 *   driver.c   用 GetCodeIndex(funcCode.m) 展开成**运行期值**, 与下面这张
 *              表逐项比对 —— 于是"sizeof 累加 == GetCodeIndex"是被**测过**的,
 *              不是被假设的, 而 f_plc 的访问控制恰恰就读这张表。
 *  (mock_plc.c 不再需要它 —— 它改用下面的 GS_* 与 GRADE_SUM_INIT。) */
#define GRADE_SUM_LIST(X)                     \
    X(group.f0[0]                       )  \
    X(group.f1[0]                       )  \
    X(group.f2[0]                       )  \
    X(group.f3[0]                       )  \
    X(group.f4[0]                       )  \
    X(group.f5[0]                       )  \
    X(group.f6[0]                       )  \
    X(group.f7[0]                       )  \
    X(group.f8[0]                       )  \
    X(group.f9[0]                       )  \
    X(group.fa[0]                       )  \
    X(group.fb[0]                       )  \
    X(group.fc[0]                       )  \
    X(group.fd[0]                       )  \
    X(group.fe[0]                       )  \
    X(group.ff[0]                       )  \
    X(group.fp[0]                       )  \
    X(group.a0[0]                       )  \
    X(group.a1[0]                       )  \
    X(group.a2[0]                       )  \
    X(group.a3[0]                       )  \
    X(group.a4[0]                       )  \
    X(group.a5[0]                       )  \
    X(group.a6[0]                       )  \
    X(group.a7[0]                       )  \
    X(group.a8[0]                       )  \
    X(group.a9[0]                       )  \
    X(group.aa[0]                       )  \
    X(group.ab[0]                       )  \
    X(group.ac[0]                       )  \
    X(group.ad[0]                       )  \
    X(group.ae[0]                       )  \
    X(group.af[0]                       )  \
    X(group.b0[0]                       )  \
    X(group.b1[0]                       )  \
    X(group.b2[0]                       )  \
    X(group.b3[0]                       )  \
    X(group.b4[0]                       )  \
    X(group.b5[0]                       )  \
    X(group.b6[0]                       )  \
    X(group.b7[0]                       )  \
    X(group.b8[0]                       )  \
    X(group.b9[0]                       )  \
    X(group.ba[0]                       )  \
    X(group.bb[0]                       )  \
    X(group.bc[0]                       )  \
    X(group.bd[0]                       )  \
    X(group.be[0]                       )  \
    X(group.bf[0]                       )  \
    X(group.c0[0]                       )  \
    X(group.c1[0]                       )  \
    X(group.c2[0]                       )  \
    X(group.c3[0]                       )  \
    X(group.c4[0]                       )  \
    X(group.c5[0]                       )  \
    X(group.c6[0]                       )  \
    X(group.c7[0]                       )  \
    X(group.c8[0]                       )  \
    X(group.c9[0]                       )  \
    X(group.ca[0]                       )  \
    X(group.cb[0]                       )  \
    X(group.cc[0]                       )  \
    X(group.cd[0]                       )  \
    X(group.ce[0]                       )  \
    X(group.cf[0]                       )  \
    X(group.fChk[0]                     )  \
    X(group.remember[0]                 )  \
    X(group.u0[0]                       )  \
    X(group.u1[0]                       )  \
    X(group.u2[0]                       )  \
    X(group.u3[0]                       )  \
    X(group.u4[0]                       )  \
    X(group.u5[0]                       )  \
    X(group.u6[0]                       )  \
    X(group.u7[0]                       )  \
    X(group.u8[0]                       )  \
    X(group.u9[0]                       )  \
    X(group.ua[0]                       )  \
    X(group.ub[0]                       )  \
    X(group.uc[0]                       )  \
    X(group.ud[0]                       )  \
    X(group.ue[0]                       )  \
    X(group.uf[0]                       )

/* 一个成员占多少个 Uint16 字。sizeof 是整型常量表达式, MSVC 折叠;
 * 而 offsetof 折出来的地址常量 MSVC 不让参与运算 (见脚本 docstring)。 */
#define PLC_WORDS(m)        ((Uint16)(sizeof(funcCode.group.m) / sizeof(Uint16)))

/* struct FUNCCODE_GROUP 各成员的起始**字**偏移 = 前面所有成员长度之和。
 * 结构体是一串扁平 Uint16 数组, 无嵌套无填充, 所以成员偏移就是前缀长度和。 */
#define GS_f0        0
#define GS_f1        (GS_f0 + PLC_WORDS(f0))
#define GS_f2        (GS_f1 + PLC_WORDS(f1))
#define GS_f3        (GS_f2 + PLC_WORDS(f2))
#define GS_f4        (GS_f3 + PLC_WORDS(f3))
#define GS_f5        (GS_f4 + PLC_WORDS(f4))
#define GS_f6        (GS_f5 + PLC_WORDS(f5))
#define GS_f7        (GS_f6 + PLC_WORDS(f6))
#define GS_f8        (GS_f7 + PLC_WORDS(f7))
#define GS_f9        (GS_f8 + PLC_WORDS(f8))
#define GS_fa        (GS_f9 + PLC_WORDS(f9))
#define GS_fb        (GS_fa + PLC_WORDS(fa))
#define GS_fc        (GS_fb + PLC_WORDS(fb))
#define GS_fd        (GS_fc + PLC_WORDS(fc))
#define GS_fe        (GS_fd + PLC_WORDS(fd))
#define GS_ff        (GS_fe + PLC_WORDS(fe))
#define GS_fp        (GS_ff + PLC_WORDS(ff))
#define GS_a0        (GS_fp + PLC_WORDS(fp))
#define GS_a1        (GS_a0 + PLC_WORDS(a0))
#define GS_a2        (GS_a1 + PLC_WORDS(a1))
#define GS_a3        (GS_a2 + PLC_WORDS(a2))
#define GS_a4        (GS_a3 + PLC_WORDS(a3))
#define GS_a5        (GS_a4 + PLC_WORDS(a4))
#define GS_a6        (GS_a5 + PLC_WORDS(a5))
#define GS_a7        (GS_a6 + PLC_WORDS(a6))
#define GS_a8        (GS_a7 + PLC_WORDS(a7))
#define GS_a9        (GS_a8 + PLC_WORDS(a8))
#define GS_aa        (GS_a9 + PLC_WORDS(a9))
#define GS_ab        (GS_aa + PLC_WORDS(aa))
#define GS_ac        (GS_ab + PLC_WORDS(ab))
#define GS_ad        (GS_ac + PLC_WORDS(ac))
#define GS_ae        (GS_ad + PLC_WORDS(ad))
#define GS_af        (GS_ae + PLC_WORDS(ae))
#define GS_b0        (GS_af + PLC_WORDS(af))
#define GS_b1        (GS_b0 + PLC_WORDS(b0))
#define GS_b2        (GS_b1 + PLC_WORDS(b1))
#define GS_b3        (GS_b2 + PLC_WORDS(b2))
#define GS_b4        (GS_b3 + PLC_WORDS(b3))
#define GS_b5        (GS_b4 + PLC_WORDS(b4))
#define GS_b6        (GS_b5 + PLC_WORDS(b5))
#define GS_b7        (GS_b6 + PLC_WORDS(b6))
#define GS_b8        (GS_b7 + PLC_WORDS(b7))
#define GS_b9        (GS_b8 + PLC_WORDS(b8))
#define GS_ba        (GS_b9 + PLC_WORDS(b9))
#define GS_bb        (GS_ba + PLC_WORDS(ba))
#define GS_bc        (GS_bb + PLC_WORDS(bb))
#define GS_bd        (GS_bc + PLC_WORDS(bc))
#define GS_be        (GS_bd + PLC_WORDS(bd))
#define GS_bf        (GS_be + PLC_WORDS(be))
#define GS_c0        (GS_bf + PLC_WORDS(bf))
#define GS_c1        (GS_c0 + PLC_WORDS(c0))
#define GS_c2        (GS_c1 + PLC_WORDS(c1))
#define GS_c3        (GS_c2 + PLC_WORDS(c2))
#define GS_c4        (GS_c3 + PLC_WORDS(c3))
#define GS_c5        (GS_c4 + PLC_WORDS(c4))
#define GS_c6        (GS_c5 + PLC_WORDS(c5))
#define GS_c7        (GS_c6 + PLC_WORDS(c6))
#define GS_c8        (GS_c7 + PLC_WORDS(c7))
#define GS_c9        (GS_c8 + PLC_WORDS(c8))
#define GS_ca        (GS_c9 + PLC_WORDS(c9))
#define GS_cb        (GS_ca + PLC_WORDS(ca))
#define GS_cc        (GS_cb + PLC_WORDS(cb))
#define GS_cd        (GS_cc + PLC_WORDS(cc))
#define GS_ce        (GS_cd + PLC_WORDS(cd))
#define GS_cf        (GS_ce + PLC_WORDS(ce))
#define GS_fChk      (GS_cf + PLC_WORDS(cf))
#define GS_remember  (GS_fChk + PLC_WORDS(fChk))
#define GS_u0        (GS_remember + PLC_WORDS(remember))
#define GS_u1        (GS_u0 + PLC_WORDS(u0))
#define GS_u2        (GS_u1 + PLC_WORDS(u1))
#define GS_u3        (GS_u2 + PLC_WORDS(u2))
#define GS_u4        (GS_u3 + PLC_WORDS(u3))
#define GS_u5        (GS_u4 + PLC_WORDS(u4))
#define GS_u6        (GS_u5 + PLC_WORDS(u5))
#define GS_u7        (GS_u6 + PLC_WORDS(u6))
#define GS_u8        (GS_u7 + PLC_WORDS(u7))
#define GS_u9        (GS_u8 + PLC_WORDS(u8))
#define GS_ua        (GS_u9 + PLC_WORDS(u9))
#define GS_ub        (GS_ua + PLC_WORDS(ua))
#define GS_uc        (GS_ub + PLC_WORDS(ub))
#define GS_ud        (GS_uc + PLC_WORDS(uc))
#define GS_ue        (GS_ud + PLC_WORDS(ud))
#define GS_uf        (GS_ue + PLC_WORDS(ue))

/* funcCodeGradeSum[] 的初始值, 与 GRADE_SUM_LIST 同序。 */
#define GRADE_SUM_INIT                        \
    GS_f0, GS_f1, GS_f2, GS_f3,                           \
    GS_f4, GS_f5, GS_f6, GS_f7,                           \
    GS_f8, GS_f9, GS_fa, GS_fb,                           \
    GS_fc, GS_fd, GS_fe, GS_ff,                           \
    GS_fp, GS_a0, GS_a1, GS_a2,                           \
    GS_a3, GS_a4, GS_a5, GS_a6,                           \
    GS_a7, GS_a8, GS_a9, GS_aa,                           \
    GS_ab, GS_ac, GS_ad, GS_ae,                           \
    GS_af, GS_b0, GS_b1, GS_b2,                           \
    GS_b3, GS_b4, GS_b5, GS_b6,                           \
    GS_b7, GS_b8, GS_b9, GS_ba,                           \
    GS_bb, GS_bc, GS_bd, GS_be,                           \
    GS_bf, GS_c0, GS_c1, GS_c2,                           \
    GS_c3, GS_c4, GS_c5, GS_c6,                           \
    GS_c7, GS_c8, GS_c9, GS_ca,                           \
    GS_cb, GS_cc, GS_cd, GS_ce,                           \
    GS_cf, GS_fChk, GS_remember, GS_u0,                   \
    GS_u1, GS_u2, GS_u3, GS_u4,                           \
    GS_u5, GS_u6, GS_u7, GS_u8,                           \
    GS_u9, GS_ua, GS_ub, GS_uc,                           \
    GS_ud, GS_ue, GS_uf,                                

/* funcCodeGradeAll: 纯宏算术, 原样照搬 (只是折成单空格)。
 * 用作 funcCodeGradeAll[] 的初始值, 所以给的是初始值列表而不是完整数组 ——
 * 真符号要由 mock_plc.c 定义, 数组维度才对得上头文件里的 extern 声明。 */
#define GRADE_ALL_INIT                        \
    F0NUM - 0, F1NUM - 0, F2NUM - 0, F3NUM - 0,     \
    F4NUM - 0, F5NUM - 0, F6NUM - 0, F7NUM - 0,     \
    F8NUM - 0, F9NUM - 0, FANUM - 0, FBNUM - 0,     \
    FCNUM - 0, FDNUM - 0, FENUM - 2, FFNUM - 0,     \
    FPNUM - 1, A0NUM - 0, A1NUM - 0, A2NUM - 0,     \
    A3NUM - 0, A4NUM - 0, A5NUM - 0, A6NUM - 0,     \
    A7NUM - 2, A8NUM - 0, A9NUM - 15, AANUM - 1,    \
    ABNUM - 1, ACNUM - 0, ADNUM - 1, 0,             \
    AFNUM - 1, B0NUM - 1, B1NUM - 1, B2NUM - 1,     \
    B3NUM - 1, B4NUM - 1, B5NUM - 1, B6NUM - 1,     \
    B7NUM - 1, B8NUM - 1, B9NUM - 1, BANUM - 1,     \
    BBNUM - 1, BCNUM - 1, BDNUM - 1, BENUM - 1,     \
    BFNUM - 1, C0NUM - 1, C1NUM - 1, C2NUM - 1,     \
    C3NUM - 1, C4NUM - 1, C5NUM - 1, C6NUM - 1,     \
    C7NUM - 1, C8NUM - 1, C9NUM - 1, CANUM - 1,     \
    CBNUM - 1, CCNUM - 1, CDNUM - 1, CENUM - 1,     \
    0, 0, 0, U0NUM -31,                             \
    U1NUM - 1, U2NUM - 1, U3NUM - 0, U4NUM - 1,     \
    U5NUM - 1, U6NUM - 1, U7NUM - 1, U8NUM - 1,     \
    U9NUM - 1, UANUM - 1, UBNUM - 1, UCNUM - 1,     \
    UDNUM - 1, UENUM - 1, 0,                      
