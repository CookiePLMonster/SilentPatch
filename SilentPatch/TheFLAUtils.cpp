#include "TheFLAUtils.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "Utils/ModuleList.hpp"

int32_t (*FLAUtils::GetExtendedID8Func)(const uint8_t* ptr) = FLAUtils::GetExtendedID8_Stock;
int32_t (*FLAUtils::GetExtendedID16Func)(const uint16_t* ptr) = FLAUtils::GetExtendedID16_Stock;
void (*FLAUtils::SetCdStreamWakeFunc)(CdStreamWakeFunc func) = nullptr;

static HMODULE flaModule = nullptr;

void FLAUtils::Init( const ModuleList& moduleList )
{
	flaModule = moduleList.Get( L"$fastman92limitAdjuster" );
	if ( flaModule != nullptr )
	{
		const auto function8 = reinterpret_cast<decltype(GetExtendedID8Func)>(GetProcAddress( flaModule, "GetExtendedIDfrom8bitBefore" ));
		if ( function8 != nullptr )
		{
			GetExtendedID8Func = function8;
		}

		const auto function16 = reinterpret_cast<decltype(GetExtendedID16Func)>(GetProcAddress( flaModule, "GetExtendedIDfrom16bitBefore" ));
		if ( function16 != nullptr )
		{
			GetExtendedID16Func = function16;
		}

		SetCdStreamWakeFunc = reinterpret_cast<decltype(SetCdStreamWakeFunc)>(GetProcAddress( flaModule, "SetCdStreamReleaseChannelfUsedByTheFLA" ));
	}
}

bool FLAUtils::UsesEnhancedIMGs()
{
	if ( flaModule == nullptr ) return false;

	const auto func = reinterpret_cast<bool(*)()>(GetProcAddress( flaModule, "IsHandlingOfEnhancedIMGarchivesEnabled" ));
	if ( func == nullptr ) return false;
	return func();
}
