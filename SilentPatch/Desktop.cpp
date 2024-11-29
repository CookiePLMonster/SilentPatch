#define WIN32_LEAN_AND_MEAN

#define WINVER 0x0501
#define _WIN32_WINNT 0x0501
#define NOMINMAX

#include "Desktop.h"

#include <windows.h>

std::pair<uint32_t, uint32_t> GetDesktopResolution()
{
	std::pair<uint32_t, uint32_t> result {};

	DEVMODEW displaySettings;
	displaySettings.dmSize = sizeof(displaySettings);
	displaySettings.dmDriverExtra = 0;
	if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &displaySettings) != FALSE)
	{
		result.first = displaySettings.dmPelsWidth;
		result.second = displaySettings.dmPelsHeight;
	}
	return result;
}
