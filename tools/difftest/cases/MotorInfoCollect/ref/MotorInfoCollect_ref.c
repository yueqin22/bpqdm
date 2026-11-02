
#include "MotorInfoCollectInclude.h"

ADC_STRUCT				gADC;
UDC_STRUCT				gUDC;
IUVW_SAMPLING_STRUCT	gCurSamp;

UVW_STRUCT				gIUVWQ12;
UVW_STRUCT_Q24          gIUVWQ24;
MT_STRUCT				gIMTQ12;
MT_STRUCT_Q24           gIMTQ24;
MT_STRUCT               gIMTSetQ12;
ALPHABETA_STRUCT		gIAlphBeta;
ALPHABETA_STRUCT		gIAlphBetaQ12;
AMPTHETA_STRUCT			gIAmpTheta;
LINE_CURRENT_STRUCT		gLineCur;
CUR_EXCURSION_STRUCT	gExcursionInfo;
TEMPLETURE_STRUCT		gTemperature;
AI_STRUCT				gAI;

void GetCurExcursion(void)
{
	int m_ErrIu,m_ErrIv;

	if((gMainStatus.RunStep != STATUS_LOW_POWER) &&
	   (gMainStatus.RunStep != STATUS_STOP))
	{
		gExcursionInfo.EnableCount = 0;
		return;
	}

	gExcursionInfo.EnableCount++;
	gExcursionInfo.EnableCount = (gExcursionInfo.EnableCount>200)?200:gExcursionInfo.EnableCount;
	if((gExcursionInfo.EnableCount < 200))
	{
		gExcursionInfo.TotalIu = 0;
		gExcursionInfo.TotalIv = 0;
		gExcursionInfo.Count = 0;
		return;
	}
	gExcursionInfo.TotalIu += gExcursionInfo.Iu;
	gExcursionInfo.TotalIv += gExcursionInfo.Iv;
	gExcursionInfo.Count++;

	if(gExcursionInfo.Count >= 32)
	{
		m_ErrIu = gExcursionInfo.TotalIu >> 5;
		m_ErrIv = gExcursionInfo.TotalIv >> 5;
        if(-32768 == m_ErrIu)
            m_ErrIu = -32767;
        if(-32768 == m_ErrIv)
            m_ErrIv = -32767;
		gExcursionInfo.TotalIu = 0;
		gExcursionInfo.TotalIv = 0;
		gExcursionInfo.Count = 0;

		gMainStatus.StatusWord.bit.RunEnable = 1;
		if( (abs(m_ErrIu) < 5120) && (abs(m_ErrIv) < 5120) )
		{
			gExcursionInfo.ErrIu = m_ErrIu;
			gExcursionInfo.ErrIv = m_ErrIv;
			gExcursionInfo.ErrCnt = 0;
		}
		else if((gExcursionInfo.ErrCnt++) > 5)
		{
			gError.ErrorCode.all |= ERROR_CURRENT_CHECK;
			gExcursionInfo.ErrCnt = 0;
			gExcursionInfo.EnableCount = 0;
		}
	}
}

void GetUDCInfo(void)
{
	Uint m_uDC;

   	m_uDC = ((Uint32)ADC_UDC * gUDC.Coff)>>16;
   	gUDC.uDC = (gUDC.uDC + m_uDC)>>1;
   	gUDC.uDCFilter = gUDC.uDCFilter - (gUDC.uDCFilter>>3) + (gUDC.uDC>>3);

	gUDC.uDCBigFilter = Filter32(gUDC.uDC,gUDC.uDCBigFilter);
}

void GetCurrentInfo(void)
{
	long  m_Iu,m_Iv;

	gExcursionInfo.Iu = (int)(ADC_IU - (Uint)32768);
	m_Iu = (long)gExcursionInfo.Iu - (long)gExcursionInfo.ErrIu;
    gExcursionInfo.IuValue = m_Iu;
	gShortGnd.ShortCur = Filter32(m_Iu, gShortGnd.ShortCur);
	m_Iu = (m_Iu * gCurSamp.Coff) >> 3;
	m_Iu = __IQsat(m_Iu, C_MAX_PER, -C_MAX_PER);

	gExcursionInfo.Iv = (int)(ADC_IV - (Uint)32768);
	m_Iv = (long)gExcursionInfo.Iv - (long)gExcursionInfo.ErrIv;
    gExcursionInfo.IvValue = m_Iv;
	m_Iv = (m_Iv * gUVCoff.UDivV) >> 12;
	m_Iv = (m_Iv * gCurSamp.Coff) >> 3;
	m_Iv = __IQsat(m_Iv, C_MAX_PER, -C_MAX_PER);

    gIUVWQ24.U = m_Iu;
    gIUVWQ24.V = m_Iv;

	gIUVWQ24.W = - (gIUVWQ24.U + gIUVWQ24.V);
	gIUVWQ24.W = __IQsat(gIUVWQ24.W,C_MAX_PER,-C_MAX_PER);

    gTemperature.TempAD = Filter16((ADC_TEMP &0xFFF0), gTemperature.TempAD);
    gAI.ai1Total += (ADC_AI1);
    gAI.ai2Total += (ADC_AI2);
    gAI.ai3Total += (ADC_AI3);
    gAI.aiCounter ++;
}
