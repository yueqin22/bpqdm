
#ifndef __f_plc__
#define	__f_plc__

#define		PLC_CARD_ID      			1
#define		INV_PLC_ID      			2

#define		INV_TRAN_FAST_NUM			6
#define		INV_TRAN_SLOW_NUM			30
#define		INV_REC_FAST_NUM			6
#define		INV_REC_SLOW_NUM			30

#define		INV_FAST_TAB_ADDR			0
#define		INV_SLOW_TAB_ADDR			0x100

#define		PLC_DISP_U3NUM				10
#define		PLC_FUNC_C0NUM				15

typedef struct C0_ATTRIBUTE_STRUCT
{
    Uint16                  lower;
    Uint16                  upper;
    Uint16                  init;
    union FUNC_ATTRIBUTE    attribute;
} C0_FUNCCODE_ATTRIBUTE;

extern Uint16 InvTranTabCfg(struct CANLINK_DATA_BUF *dataPi);
extern Uint16 InvRecTabCfg(struct CANLINK_DATA_BUF *dataPi);
extern void PlcDataFramDeal(struct CANLINK_DATA_BUF *dataPi);

#endif
