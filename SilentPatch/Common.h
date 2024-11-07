#pragma once

#include <cstdint>

// Workaround for old MSVC inline function bugs
#if defined(_MSC_VER) && _MSC_VER < 1930
#define STATIC_INLINE static
#else
#define STATIC_INLINE static inline
#endif

namespace ExtraCompSpecularity
{
	void ReadExtraCompSpecularityExceptions(const wchar_t* pPath);
	bool SpecularityExcluded(int32_t modelID);
}
namespace Common
{
	namespace Patches
	{
		void III_VC_DelayedCommon( bool hasDebugMenu, const wchar_t* iniPath );
		void III_VC_Common();
		void III_VC_SetDelayedPatchesFunc( void(*func)() );
	}
};
