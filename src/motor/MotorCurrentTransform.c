
#include "MotorCurrentTransform.h"
#include "MotorInclude.h"
#include "MotorEncoder.h"

void inline UVWToAlphBetaAxes(UVW_STRUCT_Q24 * uvw, ALPHABETA_STRUCT * AlphBeta)
{
	llong m_AlphBeta[2];

	m_AlphBeta[0] = ((llong)uvw->U * 23170L) * 2L;
	m_AlphBeta[1] = ((llong)((long)uvw->V - (long)uvw->W) * 13377L) * 2L;

	AlphBeta->Alph = m_AlphBeta[0] >> 16;
	AlphBeta->Beta = m_AlphBeta[1] >> 16;
}

void AlphBetaToDQ(ALPHABETA_STRUCT * AlphBeta, int angle, MT_STRUCT_Q24 * MT)
{
	int m_sin,m_cos;
	llong m_M,m_T;

	m_sin  = qsin(angle);
	m_cos  = qsin(16384 - angle);

	m_M = ((llong)m_cos * (AlphBeta->Alph)) + ((llong)m_sin * (AlphBeta->Beta));
	m_T = ((llong)m_cos * (AlphBeta->Beta)) - ((llong)m_sin * (AlphBeta->Alph));

	MT->M = m_M >> 15;
	MT->T = m_T >> 15;
}

void DQToAmpTheta(MT_STRUCT * MT,AMPTHETA_STRUCT * AmpTheta)
{
	long m_M,m_T,m_Input;

	m_M = (long)MT->M;
	m_T = (long)MT->T;

	m_Input = m_M * m_M + m_T * m_T;
	AmpTheta->Amp = (Uint)qsqrt(m_Input);

	AmpTheta->Theta = atan(MT->M,MT->T);
}

void ChangeCurrent(void)
{

    int m_Angle,temp;

	m_Angle = (int)(gPhase.IMPhase>>16);

	UVWToAlphBetaAxes((UVW_STRUCT_Q24*)&gIUVWQ24,(ALPHABETA_STRUCT*)&gIAlphBeta);

    gIAlphBetaQ12.Alph = gIAlphBeta.Alph>>12;
    gIAlphBetaQ12.Beta = gIAlphBeta.Beta>>12;

	AlphBetaToDQ((ALPHABETA_STRUCT*)&gIAlphBeta,m_Angle, &gIMTQ24);

	gIMTQ12.M = Filter2((gIMTQ24.M>>12), gIMTQ12.M);
	gIMTQ12.T = Filter2((gIMTQ24.T>>12), gIMTQ12.T);

    gIAmpTheta.Theta = atan(gIMTQ12.M, gIMTQ12.T);

    temp = gOutVolt.VoltPhaseApply - gIAmpTheta.Theta;
    gIAmpTheta.PowerAngle = Filter8(temp, gIAmpTheta.PowerAngle);
}
