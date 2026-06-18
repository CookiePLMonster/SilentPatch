#include "StdAfxSA.h"
#include "TimerSA.h"

ExternalRef<int> CTimer::m_snTimeInMilliseconds(AddressByVersion<int**>(0x4242D1, 0x53F6A1, 0x406FA1));
ExternalRef<float> CTimer::m_fTimeStep(AddressByVersion<float**>(0x41DE4F + 2, Memory::PatternAndOffset("0F B7 86 DC 04 00 00 D9 05", 8) ));

bool CTimer::HasGameBindings()
{
	return EnsureBindings(m_snTimeInMilliseconds, m_fTimeStep);
}
