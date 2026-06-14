#ifndef __TIMERSA
#define __TIMERSA

#include "ExternalBindings.hpp"

class CTimer
{
public:
	static int&				m_snTimeInMilliseconds;
	static float&			m_fTimeStep;

	static bool				HasGameBindings();
};

#endif