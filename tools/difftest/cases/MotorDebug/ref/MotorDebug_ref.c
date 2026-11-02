#include "MotorDefine.h"
#include "SubPrgInclude.h"
#include "MotorInclude.h"

#pragma DATA_SECTION(gDebugBuffer, "debug_buffer");

#define MOTOR_SYSTEM_DEBUG
#define CPU_TIME_DEBUG

#ifdef MOTOR_SYSTEM_DEBUG
#ifdef TMS320F2808
#define  C_DEBUG_BUFF_SIZE	    5000
#else
#define C_DEBUG_BUFF_SIZE       1000
#endif
#else

#define C_DEBUG_BUFF_SIZE   1

#endif

Uint gDebugBuffer[C_DEBUG_BUFF_SIZE];
Uint gDebugPoint = 0;
int  gDebugFlag;

#ifdef MOTOR_SYSTEM_DEBUG
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

    SaveTmCnt ++;
    if(savePrgPos == 2 &&
        gTestDataReceive.TestData17 > 3)
    {
        if(SaveTmCnt >= (gTestDataReceive.TestData17/2))
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

    switch (gTestDataReceive.TestData12)
    {
        case 0:
            save = 0;
            break;
        case 1:
            save = 1;
            break;
        case 2:
            save = (gMainCmd.Command.bit.Start) ? 1 : 0;
            break;
        default:
            save = 0;
            break;
    }

    if(gDebugPoint >= C_DEBUG_BUFF_SIZE)
	{

        gDebugPoint = (gTestDataReceive.TestData14) ? 0 : C_DEBUG_BUFF_SIZE;
	}

    if(save)
    {
        Uint i = 0;
        Uint totalN = 0;

        if(gTestDataReceive.TestData13)
        {
            totalN = 0;
            if(saveIndexP == 1)
            {
                SaveDebugData16(*pVD1);
                totalN = 1;
            }
            else if(saveIndexP == 2)
            {
                SaveDebugData16(*pVD1);
                SaveDebugData16(*pVD2);
                totalN = 2;
            }
            else if(saveIndexP == 3)
            {
                SaveDebugData16(*pVD1);
                SaveDebugData16(*pVD2);
                SaveDebugData16(*pVD3);
                totalN = 3;
            }
            else if(saveIndexP == 4)
            {
                SaveDebugData16(*pVD1);
                SaveDebugData16(*pVD2);
                SaveDebugData16(*pVD3);
                SaveDebugData16(*pVD4);
                totalN = 4;
            }

        }
        else
        {
            for(i = 0; i < 32; i++)
            {

                if(saveIndexFunc & (1L<<i))
                {

                    totalN ++;
                }
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
	gDebugFlag = 1;

	p = (Ulong *)(&gDebugBuffer);
	if(gDebugFlag)
	{
		if(gDebugPoint >= C_DEBUG_BUFF_SIZE/2)	gDebugPoint = C_DEBUG_BUFF_SIZE/2 - 2;
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
