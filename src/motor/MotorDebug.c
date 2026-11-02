#include "MotorDefine.h"
#include "SubPrgInclude.h"
#include "MotorInclude.h"

#pragma DATA_SECTION(gDebugBuffer, "debug_buffer");

#define MOTOR_SYSTEM_DEBUG
#define CPU_TIME_DEBUG

#ifdef MOTOR_SYSTEM_DEBUG
#define C_DEBUG_BUFF_SIZE       1000

Uint gDebugBuffer[C_DEBUG_BUFF_SIZE];
Uint gDebugPoint = 0;
int  gDebugFlag;

Ulong   saveIndexFunc;
Ulong   saveIndexP;
Uint    saveCurveNum;
Uint    saveRstMem;
Uint    SaveTmCnt;
int     startSave;

int *   pVD1;
int *   pVD2;
int *   pVD3;
int *   pVD4;

void DebugSaveDeal(Uint savePrgPos)
{
    Uint    save;
    Uint    m_HalfPeriod;

    SaveTmCnt ++;
    if(savePrgPos == 2 &&
        gTestDataReceive.TestData17 > 3)
    {
        m_HalfPeriod = (Uint)(gTestDataReceive.TestData17/2);
        if(SaveTmCnt >= m_HalfPeriod)
        {
            SaveTmCnt = 0;
        }
        else
        {
            return;
        }
    }
    else if(gTestDataReceive.TestData17 != savePrgPos)
    {
        return;
    }

    if(gTestDataReceive.TestData12 == 1)
    {
        save = 1;
    }
    else if(gTestDataReceive.TestData12 == 2)
    {
        save = (gMainCmd.Command.bit.Start) ? 1 : 0;
    }
    else
    {
        save = 0;
    }

    if(gDebugPoint >= C_DEBUG_BUFF_SIZE)
	{

        gDebugPoint = (gTestDataReceive.TestData14) ? 0 : C_DEBUG_BUFF_SIZE;
	}

    if(save)
    {
        Uint i = 0;
        Uint totalN = 0;
        int * pVDTable[4];

        pVDTable[0] = pVD1;
        pVDTable[1] = pVD2;
        pVDTable[2] = pVD3;
        pVDTable[3] = pVD4;

        if(gTestDataReceive.TestData13)
        {
            if(saveIndexP >= 1 && saveIndexP <= 4)
            {
                for(i = 0; i < (Uint)saveIndexP; i ++)
                {
                    SaveDebugData16(*pVDTable[i]);
                    totalN ++;
                }
            }
        }
        else
        {
            Ulong m_Mask = saveIndexFunc;

            while(m_Mask)
            {
                totalN += (Uint)(m_Mask & 1L);
                m_Mask >>= 1;
            }
        }
        saveCurveNum = totalN;
    }

    if(gTestDataReceive.TestData15 && saveRstMem == 0)
    {
        ResetDebugBuffer();
        saveRstMem ++;
    }
    if(saveRstMem && gTestDataReceive.TestData15 == 0)
    {
        saveRstMem = 0;
    }
}

void SaveDebugData16(Uint data)
{
    if(gDebugPoint >= C_DEBUG_BUFF_SIZE)
	{

	    gDebugPoint = C_DEBUG_BUFF_SIZE-1;
	}

	gDebugBuffer[gDebugPoint++] = data;
}

void SaveDebugData32(Ulong data)
{
	Ulong * p;
	Uint    m_Half;
	gDebugFlag = 1;

	p = (Ulong *)(&gDebugBuffer);
	m_Half = (Uint)(C_DEBUG_BUFF_SIZE/2);
	if(gDebugFlag)
	{
		if(gDebugPoint >= m_Half)	gDebugPoint = m_Half - 2;
		p[gDebugPoint++] = data;
	}
}

void ResetDebugBuffer(void)
{
    int m_index;

    for(m_index=0;m_index<(C_DEBUG_BUFF_SIZE-1);m_index++)
    {
        gDebugBuffer[m_index] = 0;
    }
	gDebugPoint = 0;
}
#endif
