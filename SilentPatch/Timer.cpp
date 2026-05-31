#include "StdAfx.h"
#include "Timer.h"

ExternalRef<int>	CTimer::m_snTimeInMilliseconds("83 E4 F8 89 44 24 08 C7 44 24 0C 00 00 00 00 DF 6C 24 08", -20 + 1);

#if _GTA_III

ExternalRef<float>	CTimer::ms_fTimeScale("83 E4 F8 89 44 24 08 C7 44 24 0C 00 00 00 00 DF 6C 24 08", 0x66 + 2);
ExternalRef<float>	CTimer::ms_fTimeStep("83 E4 F8 89 44 24 08 C7 44 24 0C 00 00 00 00 DF 6C 24 08", 0xE1 + 2);
ExternalRef<bool>	CTimer::m_UserPause("83 E4 F8 89 44 24 08 C7 44 24 0C 00 00 00 00 DF 6C 24 08", 0xBE + 2);
ExternalRef<bool>	CTimer::m_CodePause("83 E4 F8 89 44 24 08 C7 44 24 0C 00 00 00 00 DF 6C 24 08", 0xD8 + 2);
ExternalRef<int>	CTimer::m_snTimeInMillisecondsNonClipped("83 E4 F8 89 44 24 08 C7 44 24 0C 00 00 00 00 DF 6C 24 08", 0x129 + 1);
ExternalRef<int>	CTimer::m_snTimeInMillisecondsPauseMode("83 E4 F8 89 44 24 08 C7 44 24 0C 00 00 00 00 DF 6C 24 08", 0x8E + 1);

#elif _GTA_VC

ExternalRef<float>	CTimer::ms_fTimeScale("83 E4 F8 89 44 24 08 C7 44 24 0C 00 00 00 00 DF 6C 24 08", 0x70 + 2);
ExternalRef<float>	CTimer::ms_fTimeStep("83 E4 F8 89 44 24 08 C7 44 24 0C 00 00 00 00 DF 6C 24 08", 0xF3 + 2);
ExternalRef<bool>	CTimer::m_UserPause("83 E4 F8 89 44 24 08 C7 44 24 0C 00 00 00 00 DF 6C 24 08", 0x4A + 2);
ExternalRef<bool>	CTimer::m_CodePause("83 E4 F8 89 44 24 08 C7 44 24 0C 00 00 00 00 DF 6C 24 08", 0x67 + 2);
ExternalRef<int>	CTimer::m_snTimeInMillisecondsNonClipped("83 E4 F8 89 44 24 08 C7 44 24 0C 00 00 00 00 DF 6C 24 08", 0x13B + 1);
ExternalRef<int>	CTimer::m_snTimeInMillisecondsPauseMode("83 E4 F8 89 44 24 08 C7 44 24 0C 00 00 00 00 DF 6C 24 08", 0x9C + 1);

#endif

static ExternalRef<uint32_t> timerFrequency("83 E4 F8 89 44 24 08 C7 44 24 0C 00 00 00 00 DF 6C 24 08", -8 + 1);
static ExternalRef<LARGE_INTEGER> prevTimer("83 E4 F8 89 44 24 08 C7 44 24 0C 00 00 00 00 DF 6C 24 08", 62 + 2);

bool CTimer::HasGameBindings()
{
	return EnsureBindings(m_snTimeInMilliseconds, ms_fTimeScale, ms_fTimeStep, m_UserPause, m_CodePause, m_snTimeInMillisecondsNonClipped, m_snTimeInMillisecondsPauseMode,
							timerFrequency, prevTimer);
}

void CTimer::Update_SilentPatch()
{
	LARGE_INTEGER perfCount;
	QueryPerformanceCounter( &perfCount );

	double diff = double(perfCount.QuadPart - prevTimer.Get().QuadPart);
#if _GTA_VC
	if ( !m_UserPause.Get() && !m_CodePause.Get() )
#endif
	{
		diff *= ms_fTimeScale.Get();
	}

	prevTimer.Get() = perfCount;

	static double DeltaRemainder = 0.0;
	const double delta = diff / timerFrequency.Get();
	double deltaIntegral;
	DeltaRemainder = modf( delta + DeltaRemainder, &deltaIntegral );

	const int deltaInteger = int(deltaIntegral);
	m_snTimeInMillisecondsPauseMode.Get() += deltaInteger;
	if ( !m_UserPause.Get() && !m_CodePause.Get() )
	{
		m_snTimeInMillisecondsNonClipped.Get() += deltaInteger;
		m_snTimeInMilliseconds.Get() += deltaInteger;
		ms_fTimeStep.Get() = float(delta * 0.05);
	}
	else
	{
		ms_fTimeStep.Get() = 0.0f;
	}
}
