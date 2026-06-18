#ifndef __TIMERSA
#define __TIMERSA

#include "ExternalBindings.hpp"

class CTimer
{
public:
	static ExternalRef<int>	m_snTimeInMilliseconds;
	static ExternalRef<float> m_fTimeStep;

	static bool				HasGameBindings();
};

#endif