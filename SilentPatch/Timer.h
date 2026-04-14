#ifndef __TIMER
#define __TIMER

#include <cmath>

class CTimer
{
public:
	static float&			ms_fTimeScale;
	static float&			ms_fTimeStep;
	static bool&			m_UserPause;
	static bool&			m_CodePause;
	static int&				m_snTimeInMilliseconds;
	static int&				m_snTimeInMillisecondsNonClipped;
	static int&				m_snTimeInMillisecondsPauseMode;

public:
	static void				Update_SilentPatch();
};

#endif
