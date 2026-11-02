
#define DISP_FRQ_RUN            0
#define DISP_FRQ_AIM            1
#define DISP_OUT_CURRENT        4
#define DISP_OUT_POWER          5
#define DISP_FRQ_RUN_FDB        19
#define DISP_FRQ_COMM           28
#define DISP_P2P_COMM_SEND      63
#define DISP_P2P_COMM_REV       64
#define DISP_FRQ_FDB            29
#define DISP_FRQ_X              30
#define DISP_FRQ_Y              31
#define DISP_LOAD_SPEED         14

#define DISP_DI_STATUS_SPECIAL1 41
#define DISP_DO_STATUS_SPECIAL1 42
#define DISP_DI_FUNC_SPECIAL1   43
#define DISP_DI_FUNC_SPECIAL2   44

extern Uint16 ticker05msFuncEachDisp[4];
extern Uint16 ticker05msFuncInternalDisp[4];
extern Uint16 vfSeparateVol;
extern Uint16 vfSeprateVolAim;
extern Uint16 temperature;
extern Uint16 ZCount;
extern Uint16 antiVibrateCoeff;
#define UF_VIEW_ATTRIBUTE  0x3140

void UpdateU0Data(void)
{
    funcCode.group.u0[0]  = frqDisp;
    funcCode.group.u0[1]  = frqAimDisp;
    funcCode.group.u0[2]  = generatrixVoltage;
    funcCode.group.u0[3]  = outVoltageDisp;
    funcCode.group.u0[4]  = outCurrentDisp;
    funcCode.group.u0[5]  = outPower;
    funcCode.group.u0[6]  = itDisp;
    funcCode.group.u0[7]  = diStatus.a.all&0x7FFF;
    funcCode.group.u0[8]  = doStatus.a.all;
    funcCode.group.u0[9]  = aiDeal[0].voltage;
    funcCode.group.u0[10] = aiDeal[1].voltage;
    funcCode.group.u0[11] = aiDeal[2].voltage;
    funcCode.group.u0[12] = funcCode.code.counterTicker;
    funcCode.group.u0[13] = funcCode.code.lengthCurrent;
    funcCode.group.u0[14] = loadSpeedDisp;
    funcCode.group.u0[15] = pidFuncRefDisp;
    funcCode.group.u0[16] = pidFuncFdbDisp;
    funcCode.group.u0[17] = plcStep;
    funcCode.group.u0[18] = pulseInFrqDisp;
    funcCode.group.u0[19] = frqRunDisp;
    funcCode.group.u0[20] = setRunLostTime;
    funcCode.group.u0[21] = aiDeal[0].voltageOrigin;
    funcCode.group.u0[22] = aiDeal[1].voltageOrigin;
    funcCode.group.u0[23] = aiDeal[2].voltageOrigin;
    funcCode.group.u0[24] = lineSpeed;
    funcCode.group.u0[25] = curTime.powerOnTime;
    funcCode.group.u0[26] = curTime.runTime;
    funcCode.group.u0[27] = pulseInFrq;
    funcCode.group.u0[28] = funcCode.code.frqComm;
    funcCode.group.u0[29] = frqFdb;
    funcCode.group.u0[30] = frqXDisp;
    funcCode.group.u0[31] = frqYDisp;
    funcCode.group.u0[32] = memoryValue;
    funcCode.group.u0[33] = pmsmRotorPos;
    funcCode.group.u0[34] = temperature;
    funcCode.group.u0[35] = torqueAim;
    funcCode.group.u0[36] = enCoderPosition;
    funcCode.group.u0[37] = gPhiRtDisp;
    funcCode.group.u0[38] = ABZPos;
    funcCode.group.u0[39] = vfSeprateVolAim;
    funcCode.group.u0[40] = vfSeparateVol;

    funcCode.group.u0[45] = errorInfo;

    funcCode.group.u0[50] = ticker05msFuncInternalDisp[0];
    funcCode.group.u0[51] = ticker05msFuncInternalDisp[1];
    funcCode.group.u0[52] = ticker05msFuncInternalDisp[2];
    funcCode.group.u0[53] = ticker05msFuncInternalDisp[3];
    funcCode.group.u0[54] = ticker05msFuncEachDisp[0];
    funcCode.group.u0[55] = ticker05msFuncEachDisp[1];
    funcCode.group.u0[56] = ticker05msFuncEachDisp[2];
    funcCode.group.u0[57] = ticker05msFuncEachDisp[3];
    funcCode.group.u0[58] = ZCount;
    funcCode.group.u0[59] = frqAimPLCDisp;
    funcCode.group.u0[60] = frqPLCDisp;
    funcCode.group.u0[61] = invtStatus.all;

    funcCode.group.u0[63] = p2pData.P2PSendData;
    funcCode.group.u0[64] = p2pData.P2PRevData;
 #if 0
    funcCode.group.u0[62] = 0;
    funcCode.group.u0[63] = 0;
    funcCode.group.u0[64] = 0;
    funcCode.group.u0[65] = 0;
    funcCode.group.u0[66] = 0;
    funcCode.group.u0[67] = 0;
    funcCode.group.u0[68] = 0;
    funcCode.group.u0[69] = 0;
    funcCode.group.u0[70] = 0;
    funcCode.group.u0[71] = 0;
    funcCode.group.u0[72] = 0;
    funcCode.group.u0[73] = 0;
    funcCode.group.u0[74] = 0;
    funcCode.group.u0[75] = 0;
    funcCode.group.u0[76] = 0;
    funcCode.group.u0[77] = 0;
    funcCode.group.u0[78] = 0;
    funcCode.group.u0[79] = 0;
    funcCode.group.u0[80] = 0;
    funcCode.group.u0[81] = 0;
    funcCode.group.u0[82] = 0;
    funcCode.group.u0[83] = 0;
    funcCode.group.u0[84] = 0;
    funcCode.group.u0[85] = 0;
    funcCode.group.u0[86] = 0;
    funcCode.group.u0[87] = 0;
    funcCode.group.u0[88] = 0;
    funcCode.group.u0[89] = 0;
    funcCode.group.u0[90] = 0;
    funcCode.group.u0[91] = 0;
    funcCode.group.u0[92] = 0;
    funcCode.group.u0[93] = 0;
    funcCode.group.u0[94] = 0;
    funcCode.group.u0[95] = 0;
    funcCode.group.u0[96] = 0;
    funcCode.group.u0[97] = 0;
    funcCode.group.u0[98] = 0;
    funcCode.group.u0[99] = 0;
#endif
}

union FUNC_ATTRIBUTE const dispAttributeU0[U0NUM] =
{
    0x094A & ~(1U<<11) | (1U<<12),
    0x094A & ~(1U<<11) | (1U<<12),
    0x1161 & ~(1U<<11) | (1U<<12),
    0x08E0 & ~(1U<<11) | (1U<<12),
    0x0952 & ~(1U<<11) | (1U<<12),

    0x2941 & ~(1U<<11) | (1U<<12),
    0x2131 & ~(1U<<11) | (1U<<12),
    0x9D00 & ~(1U<<11) | (1U<<12),
    0x9D00 & ~(1U<<11) | (1U<<12),
    0x2122 & ~(1U<<11) | (1U<<12),

    0x2122 & ~(1U<<11) | (1U<<12),
    0x2122 & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),

    0x0170 & ~(1U<<11) | (1U<<12),
    0x0170 & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),
    0x1142 & ~(1U<<11) | (1U<<12),
    0x294A & ~(1U<<11) | (1U<<12),
    0x0141 & ~(1U<<11) | (1U<<12),

    0x2163 & ~(1U<<11) | (1U<<12),
    0x2163 & ~(1U<<11) | (1U<<12),
    0x2163 & ~(1U<<11) | (1U<<12),

    0x1D00 & ~(1U<<11) | (1U<<12),

    0x0140 & ~(1U<<11) | (1U<<12),
    0x0141 & ~(1U<<11) | (1U<<12),
    0x0148 & ~(1U<<11) | (1U<<12),
    0x2972 & ~(1U<<11) | (1U<<12),
    0x294A & ~(1U<<11) | (1U<<12),

    0x094A & ~(1U<<11) | (1U<<12),
    0x094A & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),
    0x0101 & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),

    0x2131 & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),
    0x2101 & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),

    0x0140 & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),

    0x0140 & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),

    0x1001,
    0x1001,
    0x1001,
    0x1001,
    0x1001,
    0x1001,
    0x1001,
    0x1001,
    0x0140 & ~(1U<<11) | (1U<<12),

    0x3172 & ~(1U<<11) | (1U<<12),
    0x3172 & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),
    0x2972 & ~(1U<<11) | (1U<<12),
    0x2972 & ~(1U<<11) | (1U<<12),
    0x0140 & ~(1U<<11) | (1U<<12),
	0x0140 & ~(1U<<11) | (1U<<12),
};

const Uint16 stopDispIndex[STOP_DISPLAY_NUM] =
{
    1,
    2,
    7,
    8,
    9,

    10,
    11,
    12,
    13,
    17,

    14,
    15,
    18,
};

Uint16 const commDispIndex[COMM_PARA_NUM] =
{
    28,
    0,
    2,
    3,
    4,
    5,
    6,
    19,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,

    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
};
