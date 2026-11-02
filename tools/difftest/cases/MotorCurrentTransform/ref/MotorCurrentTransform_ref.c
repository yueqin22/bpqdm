
#include "MotorCurrentTransform.h"
#include "MotorInclude.h"
#include "MotorEncoder.h"

void inline UVWToAlphBetaAxes(UVW_STRUCT_Q24 * uvw, ALPHABETA_STRUCT * AlphBeta)
{
	AlphBeta->Alph = ((llong)uvw->U * 23170L)>>15;

	AlphBeta->Beta = ((llong)((long)uvw->V - (long)uvw->W) * 13377L)>>15;
}

void AlphBetaToDQ(ALPHABETA_STRUCT * AlphBeta, int angle, MT_STRUCT_Q24 * MT)
{
	int m_sin,m_cos;

	m_sin  = qsin(angle);
	m_cos  = qsin(16384 - angle);
	MT->M = ( ((llong)m_cos * (llong)(AlphBeta->Alph)) +
	          ((llong)m_sin * (llong)(AlphBeta->Beta)) )>>15;
	MT->T = (-((llong)m_sin * (llong)(AlphBeta->Alph)) +
	          ((llong)m_cos * (llong)(AlphBeta->Beta)) )>>15;
}

void DQToAmpTheta(MT_STRUCT * MT,AMPTHETA_STRUCT * AmpTheta)
{
	long m_Input;

	m_Input = (((long)MT->M * (long)MT->M) + ((long)MT->T * (long)MT->T));
	AmpTheta->Amp = (Uint)qsqrt(m_Input);

	AmpTheta->Theta = atan(MT->M,MT->T);
}

void ChangeCurrent(void)
{

    int temp;

	UVWToAlphBetaAxes((UVW_STRUCT_Q24*)&gIUVWQ24,(ALPHABETA_STRUCT*)&gIAlphBeta);
    gIAlphBetaQ12.Alph = gIAlphBeta.Alph>>12;
    gIAlphBetaQ12.Beta = gIAlphBeta.Beta>>12;

	AlphBetaToDQ((ALPHABETA_STRUCT*)&gIAlphBeta,(gPhase.IMPhase>>16), &gIMTQ24);

	gIMTQ12.M = Filter2((gIMTQ24.M>>12), gIMTQ12.M);
	gIMTQ12.T = Filter2((gIMTQ24.T>>12), gIMTQ12.T);

    gIAmpTheta.Theta = atan(gIMTQ12.M, gIMTQ12.T);

    temp = gOutVolt.VoltPhaseApply - gIAmpTheta.Theta;
    gIAmpTheta.PowerAngle = Filter8(temp, gIAmpTheta.PowerAngle);
}
