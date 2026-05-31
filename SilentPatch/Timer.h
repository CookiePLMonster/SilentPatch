#pragma once

#include "ExternalBindings.hpp"

class CTimer
{
public:
	static ExternalRef<float>	ms_fTimeScale;
	static ExternalRef<float>	ms_fTimeStep;
	static ExternalRef<bool>	m_UserPause;
	static ExternalRef<bool>	m_CodePause;
	static ExternalRef<int>		m_snTimeInMilliseconds;
	static ExternalRef<int>		m_snTimeInMillisecondsNonClipped;
	static ExternalRef<int>		m_snTimeInMillisecondsPauseMode;

public:
	static void				Update_SilentPatch();
	static bool				HasGameBindings();
};
