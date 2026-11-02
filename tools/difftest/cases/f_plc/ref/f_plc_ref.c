
#include "f_funcCode.h"
#include "f_dspcan.h"
#include "f_canlink.h"
#include "f_comm.h"
#include "f_plc.h"

#define DEBUG_F_PLC              1

#if DEBUG_F_PLC

Uint16 InvTrFastIndex = 1,
	   InvTrSlowIndex = 1;

Uint16 InvTrFastTab[INV_TRAN_FAST_NUM+1] = {0, 0x703d};
Uint16 InvTrSlowTab[INV_TRAN_SLOW_NUM+1] = {0, 0};
Uint16 InvRecFastTab[INV_REC_FAST_NUM+1] = {0, 0};
Uint16 InvRecSlowTab[INV_REC_SLOW_NUM+1] = {0, 0};

Uint16 FuncCodeRwAtrrib(Uint16 addr,Uint16 rwMode);

#define COMM_NO_W_FC_0  GetCodeIndex(funcCode.code.tuneCmd)
#define COMM_NO_W_FC_1  GetCodeIndex(funcCode.code.menuMode)
#define COMM_NO_W_FC_2  GetCodeIndex(funcCode.code.motorFcM2.tuneCmd)
#define COMM_NO_W_FC_3  GetCodeIndex(funcCode.code.motorFcM3.tuneCmd)
#define COMM_NO_W_FC_4  GetCodeIndex(funcCode.code.motorFcM4.tuneCmd)
#define COMM_NO_W_FC_5  GetCodeIndex(funcCode.code.funcParaView)

#define COMM_NO_R_FC_0  GetCodeIndex(funcCode.code.userPassword)

#define COMM_NO_RW_FC_0 GetCodeIndex(funcCode.code.userPasswordReadOnly)
#define COMM_NO_RW_FC_2  GetCodeIndex(funcCode.code.plcEnable)
#define COMM_READ_CURRENT_FC GetCodeIndex(funcCode.group.u0[4])

#define COMM_READ_FC    0
#define COMM_WRITE_FC   1

Uint16 InvTranTabCfg(struct CANLINK_DATA_BUF *dataPi)
{
	Uint16 err;
	err = FuncCodeRwAtrrib(dataPi->mdl.data.datal, COMM_READ_FC);
	if (err)
		return (COMM_ERR_PARA);

	err = COMM_ERR_ADDR;
	if (dataPi->mdl.data.datah >= INV_SLOW_TAB_ADDR)
	{
		if ( (dataPi->mdl.data.datah - INV_SLOW_TAB_ADDR) <= INV_TRAN_SLOW_NUM )
		{
			InvTrSlowTab[dataPi->mdl.data.datah - INV_SLOW_TAB_ADDR] = dataPi->mdl.data.datal;
			err = COMM_ERR_NONE;
		}
	}
	else if (dataPi->mdl.data.datah != 1)
	{
		if (dataPi->mdl.data.datah <= INV_TRAN_FAST_NUM)
		{
			InvTrFastTab[dataPi->mdl.data.datah] = dataPi->mdl.data.datal;
			err = COMM_ERR_NONE;
		}
	}

	return (err);
}

Uint16 InvRecTabCfg(struct CANLINK_DATA_BUF *dataPi)
{
	Uint16 err;
	err = FuncCodeRwAtrrib(dataPi->mdl.data.datal, COMM_WRITE_FC);
	if (err)
		return (err);

	err = COMM_ERR_PARA;
	if (dataPi->mdl.data.datah >= INV_SLOW_TAB_ADDR)
	{
		if ( (dataPi->mdl.data.datah - INV_SLOW_TAB_ADDR) <= INV_TRAN_SLOW_NUM )
		{
			InvRecSlowTab[dataPi->mdl.data.datah - INV_SLOW_TAB_ADDR] = dataPi->mdl.data.datal;
			err = COMM_ERR_NONE;
		}
	}
	else
	{
		if (dataPi->mdl.data.datah <= INV_TRAN_FAST_NUM)
		{
			InvRecFastTab[dataPi->mdl.data.datah] = dataPi->mdl.data.datal;
			err = COMM_ERR_NONE;
		}
	}
	return (err);
}

Uint16 FuncCodeRwAtrrib(Uint16 addr,Uint16 rwMode)
{
	Uint16 index, group, grade, highH;
    Uint16 funcCodeGradeComm[FUNCCODE_GROUP_NUM];

	if (addr == 0)
		return COMM_ERR_NONE;

    highH = (addr & 0xF000);

	if (

        (highH == 0xF000) ||
        (highH == 0xA000) ||
        (highH == 0xB000) ||
        (highH == 0xC000) ||
		((highH == 0x7000) && (rwMode == COMM_READ_FC) ) ||
		( ((addr & 0xFF00) == 0x7300) && (rwMode == COMM_WRITE_FC) )

		)
    {

    }
    else
    {
        return COMM_ERR_ADDR;
    }

    group = (addr >> 8) & 0x0F;
    grade = addr & 0xFF;

    if (0xA000 == highH)
    {
        group += FUNCCODE_GROUP_A0;
    }
    else if (0xB000 == highH)
    {
        group += FUNCCODE_GROUP_B0;
    }
    else if (0xC000 == highH)
    {
        group += FUNCCODE_GROUP_C0;
    }

    else if (0x7000 == highH)
    {
        group += FUNCCODE_GROUP_U0;
    }

	if (group == FC_GROUP_FACTORY)
		return COMM_ERR_ADDR;

    UpdataFuncCodeGrade(funcCodeGradeComm);

    if (grade >= funcCodeGradeComm[group])
    {
        return COMM_ERR_ADDR;
    }

    index = GetGradeIndex(group, grade);
    if (
			(COMM_NO_RW_FC_0 == index) ||
			(COMM_NO_RW_FC_2  == index)||
			(	(COMM_WRITE_FC == rwMode) &&
				(	(COMM_NO_W_FC_0 == index) ||
					(COMM_NO_W_FC_1 == index) ||
					(COMM_NO_W_FC_2 == index) ||
					(COMM_NO_W_FC_3 == index) ||
					(COMM_NO_W_FC_4 == index) ||
					(COMM_NO_W_FC_5 == index)
				)
			) ||
			((COMM_READ_FC == rwMode) && (COMM_NO_R_FC_0 == index))

		)
    {
        return COMM_ERR_ADDR;
    }

	return COMM_ERR_NONE;
}

void PlcDataFramDeal(struct CANLINK_DATA_BUF *dataPi)
{
	Uint16 addr, *InvRecTab, *InvTrTab;
	Uint16 result, invTrIndex, indexReset = 1;
	Uint16 len = 0;

	addr = dataPi->mdl.data.datal;

	if ( (addr > 0) && (addr <= INV_REC_FAST_NUM) )
	{
		InvRecTab = InvRecFastTab;

		InvTrTab = InvTrFastTab;
		invTrIndex = InvTrFastIndex;
	}
	else if ( (addr > INV_SLOW_TAB_ADDR) && (addr <= (INV_SLOW_TAB_ADDR + INV_REC_SLOW_NUM) ) )
	{
        addr -= INV_SLOW_TAB_ADDR;
        InvRecTab = InvRecSlowTab;

		InvTrTab = InvTrSlowTab;
		invTrIndex = InvTrSlowIndex;
	}
	else
	{
        if (funcCode.code.u3[11] < 65535)
            funcCode.code.u3[11]++;

        return;
	}

	if (InvRecTab[addr])
	{
		CanControlWriter(InvRecTab[addr], dataPi->mdl.data.datah, 0);
		if (InvRecTab[addr+1])
		{
			CanControlWriter(InvRecTab[addr+1], dataPi->mdh.data.datal, 0);
			if (InvRecTab[addr+2])
				CanControlWriter(InvRecTab[addr+2], dataPi->mdh.data.datah, 0);
		}
	}

	if (InvTrTab[invTrIndex])
	{
        if (InvTrTab == InvTrFastTab)
            dataPi->mdl.data.datal = invTrIndex;
        else
            dataPi->mdl.data.datal = invTrIndex + INV_SLOW_TAB_ADDR;

        CanControlRead(InvTrTab[invTrIndex], &result);
		dataPi->mdl.data.datah = result;
		len += 4;
		if (InvTrTab[invTrIndex+1])
		{
			CanControlRead(InvTrTab[invTrIndex+1], &result);
			dataPi->mdh.data.datal = result;
			len += 2;
			if (InvTrTab[invTrIndex+2])
			{
				CanControlRead(InvTrTab[invTrIndex+2], &result);
				dataPi->mdh.data.datah = result;
				len += 2;
				if (InvTrTab[invTrIndex+3])
					indexReset = 0;
			}
		}
	}
	dataPi->msgid.bit.destSta = PLC_CARD_ID;
	dataPi->msgid.bit.srcSta = INV_PLC_ID;
	CanlinkDataTran((Uint32*)(dataPi), len, 1000);

	if (InvTrTab == InvTrFastTab)
	{
		if ( (InvTrFastIndex > (INV_TRAN_FAST_NUM - 3) )
			|| (0 != indexReset) )
			InvTrFastIndex = 1;
		else
			InvTrFastIndex += 3;
	}
	else
	{
		if ( (InvTrSlowIndex > (INV_TRAN_SLOW_NUM - 3) )
			|| (0 != indexReset) )
			InvTrSlowIndex = 1;
		else
			InvTrSlowIndex += 3;
	}
}

#else

Uint16 InvTranTabCfg(struct CANLINK_DATA_BUF *dataPi)
{
    dataPi = dataPi;
    return 0;
}
Uint16 InvRecTabCfg(struct CANLINK_DATA_BUF *dataPi)
{
    dataPi = dataPi;
    return 0;
}
void PlcDataFramDeal(struct CANLINK_DATA_BUF *dataPi)
{
    dataPi = dataPi;
}

#endif
