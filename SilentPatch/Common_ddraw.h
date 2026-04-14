#pragma once

#include <cstdint>

extern char** ppUserFilesDir;

namespace Common
{
	namespace Patches
	{
		bool FixRwcseg_Patterns();

		void DDraw_III_10( uint32_t width, uint32_t height, const char* desktopText );
		void DDraw_III_11( uint32_t width, uint32_t height, const char* desktopText );
		void DDraw_III_Steam( uint32_t width, uint32_t height, const char* desktopText );

		void DDraw_VC_10( uint32_t width, uint32_t height, const char* desktopText );
		void DDraw_VC_11( uint32_t width, uint32_t height, const char* desktopText );
		void DDraw_VC_Steam( uint32_t width, uint32_t height, const char* desktopText );

		void DDraw_Common();
	}
};
