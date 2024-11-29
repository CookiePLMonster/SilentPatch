// This API is Win7 only, so make sure we use dynamic imports
#define WIN32_LEAN_AND_MEAN

#define NOMINMAX
#define WINVER 0x0602
#define _WIN32_WINNT 0x0602

#include "FriendlyMonitorNames.h"

#include <windows.h>

#include <memory>

std::map<std::string, std::string, std::less<>> FriendlyMonitorNames::GetNamesForDevicePaths()
{
    std::map<std::string, std::string, std::less<>> monitorNames;

    HMODULE user32Lib = LoadLibrary(TEXT("user32"));
    if (user32Lib != nullptr)
    {
        auto getDisplayConfigBufferSizes = (decltype(GetDisplayConfigBufferSizes)*)GetProcAddress(user32Lib, "GetDisplayConfigBufferSizes");
        auto queryDisplayConfig = (decltype(QueryDisplayConfig)*)GetProcAddress(user32Lib, "QueryDisplayConfig");
        auto displayConfigGetDeviceInfo = (decltype(DisplayConfigGetDeviceInfo)*)GetProcAddress(user32Lib, "DisplayConfigGetDeviceInfo");
        if (getDisplayConfigBufferSizes != nullptr && queryDisplayConfig != nullptr && displayConfigGetDeviceInfo != nullptr)
        {
            UINT32 pathCount, modeCount;
            std::unique_ptr<DISPLAYCONFIG_PATH_INFO[]> paths;
            std::unique_ptr<DISPLAYCONFIG_MODE_INFO[]> modes;

            LONG result = ERROR_SUCCESS;
            do
            {
                result = getDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount);
                if (result != ERROR_SUCCESS)
                {
                    break;
                }
                paths = std::make_unique<DISPLAYCONFIG_PATH_INFO[]>(pathCount);
                modes = std::make_unique<DISPLAYCONFIG_MODE_INFO[]>(modeCount);
                result = queryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.get(), &modeCount, modes.get(), nullptr);
            }
            while (result == ERROR_INSUFFICIENT_BUFFER);

            if (result == ERROR_SUCCESS)
            {
                for (size_t i = 0; i < pathCount; i++)
                {
                    DISPLAYCONFIG_TARGET_DEVICE_NAME targetName = {};
                    targetName.header.adapterId = paths[i].targetInfo.adapterId;
                    targetName.header.id = paths[i].targetInfo.id;
                    targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
                    targetName.header.size = sizeof(targetName);
                    const LONG targetNameResult = DisplayConfigGetDeviceInfo(&targetName.header);

                    DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName = {};
                    sourceName.header.adapterId = paths[i].sourceInfo.adapterId;
                    sourceName.header.id = paths[i].sourceInfo.id;
                    sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
                    sourceName.header.size = sizeof(sourceName);
                    const LONG sourceNameResult = DisplayConfigGetDeviceInfo(&sourceName.header);
                    if (targetNameResult == ERROR_SUCCESS && sourceNameResult == ERROR_SUCCESS && targetName.monitorFriendlyDeviceName[0] != '\0')
                    {
                        char gdiDeviceName[std::size(sourceName.viewGdiDeviceName)];
                        char monitorFriendlyDeviceName[std::size(targetName.monitorFriendlyDeviceName)];
                        WideCharToMultiByte(CP_ACP, 0, sourceName.viewGdiDeviceName, -1, gdiDeviceName, static_cast<int>(std::size(gdiDeviceName)), nullptr, nullptr);
                        WideCharToMultiByte(CP_ACP, 0, targetName.monitorFriendlyDeviceName, -1, monitorFriendlyDeviceName, static_cast<int>(std::size(monitorFriendlyDeviceName)), nullptr, nullptr);

                        monitorNames.try_emplace(gdiDeviceName, monitorFriendlyDeviceName);
                    }
                }
            }
        }
        FreeLibrary(user32Lib);
    }

    return monitorNames;
}
