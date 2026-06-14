#include "StdAfxSA.h"
#include "ScriptSA.h"

#include "ExternalBindings.hpp"

ExternalRef StatTypesInt(AddressByVersion<int (**)[]>(0x55C0D8, 0x55C578, 0x574F24));

extern ExternalRef<uint8_t> nGameClockDays;
extern ExternalRef<uint8_t> nGameClockMonths;

bool HasGameBindings_GymGlitch()
{
	return EnsureBindings(StatTypesInt, nGameClockMonths, nGameClockDays);
}

int32_t* CRunningScript::GetDay_GymGlitch()
{
	static int32_t	Out[2];

	if ( strcmp(Name, "gymbike") == 0 || strcmp(Name, "gymbenc") == 0 || strcmp(Name, "gymtrea") == 0 || strcmp(Name, "gymdumb") == 0 )
	{
		Out[0] = -1;
		Out[1] = StatTypesInt.Get()[134-120] + 1;
	}
	else
	{
		Out[0] = nGameClockMonths.Get();
		Out[1] = nGameClockDays.Get();
	}

	return Out;
}