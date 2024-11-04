#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0501
#define _WIN32_WINNT 0x0501

#include <windows.h>
#include <stdio.h>
#include <shlwapi.h>
#include <shlobj.h>
#include "Utils/MemoryMgr.h"
#include "Utils/Patterns.h"
#include "Utils/ScopedUnprotect.hpp"

#include "Common_ddraw.h"
#include "Desktop.h"

#pragma comment(lib, "shlwapi.lib")
#pragma comment(linker, "/EXPORT:DirectDrawCreateEx=_DirectDrawCreateEx@16")

extern "C" HRESULT WINAPI DirectDrawCreateEx(GUID FAR *lpGUID, LPVOID *lplpDD, REFIID iid, IUnknown FAR *pUnkOuter)
{
	static HRESULT	(WINAPI *pDirectDrawCreateEx)(GUID FAR*, LPVOID*, REFIID, IUnknown FAR*);
	if ( pDirectDrawCreateEx == nullptr )
	{
		wchar_t		wcSystemPath[MAX_PATH];
		GetSystemDirectoryW(wcSystemPath, MAX_PATH);
		PathAppendW(wcSystemPath, L"ddraw.dll");

		HMODULE		hLib = LoadLibraryW(wcSystemPath);
		pDirectDrawCreateEx = (HRESULT(WINAPI*)(GUID FAR*, LPVOID*, REFIID, IUnknown FAR*))GetProcAddress(hLib, "DirectDrawCreateEx");
	}
	return pDirectDrawCreateEx(lpGUID, lplpDD, iid, pUnkOuter);
}

char** ppUserFilesDir;

void InjectHooks()
{
	static char		aNoDesktopMode[64];

	const auto [width, height] = GetDesktopResolution();
	sprintf_s(aNoDesktopMode, "Cannot find %ux%ux32 video mode", width, height);

	std::unique_ptr<ScopedUnprotect::Unprotect> Protect = ScopedUnprotect::UnprotectSectionOrFullModule( GetModuleHandle( nullptr ), ".text" );

	if (*(DWORD*)Memory::DynBaseAddress(0x5C1E75) == 0xB85548EC)
	{
		// III 1.0
		ppUserFilesDir = (char**)Memory::DynBaseAddress(0x580C16);
		Common::Patches::DDraw_III_10( width, height, aNoDesktopMode );
	}
	else if (*(DWORD*)Memory::DynBaseAddress(0x5C2135) == 0xB85548EC)
	{
		// III 1.1
		ppUserFilesDir = (char**)Memory::DynBaseAddress(0x580F66);
		Common::Patches::DDraw_III_11( width, height, aNoDesktopMode );
	}
	else if (*(DWORD*)Memory::DynBaseAddress(0x5C6FD5) == 0xB85548EC)
	{
		// III Steam
		ppUserFilesDir = (char**)Memory::DynBaseAddress(0x580E66);
		Common::Patches::DDraw_III_Steam( width, height, aNoDesktopMode );
	}

	else if (*(DWORD*)Memory::DynBaseAddress(0x667BF5) == 0xB85548EC)
	{
		// VC 1.0
		ppUserFilesDir = (char**)Memory::DynBaseAddress(0x6022AA);
		Common::Patches::DDraw_VC_10( width, height, aNoDesktopMode );
	}
	else if (*(DWORD*)Memory::DynBaseAddress(0x667C45) == 0xB85548EC)
	{
		// VC 1.1
		ppUserFilesDir = (char**)Memory::DynBaseAddress(0x60228A);
		Common::Patches::DDraw_VC_11( width, height, aNoDesktopMode );
	}
	else if (*(DWORD*)Memory::DynBaseAddress(0x666BA5) == 0xB85548EC)
	{
		// VC Steam
		ppUserFilesDir = (char**)Memory::DynBaseAddress(0x601ECA);
		Common::Patches::DDraw_VC_Steam( width, height, aNoDesktopMode );
	}

	Common::Patches::DDraw_Common();
}

static bool rwcsegUnprotected = false;

static void ProcHook()
{
	static bool		bPatched = false;
	if ( !bPatched )
	{
		bPatched = true;

		InjectHooks();

		if ( !rwcsegUnprotected )
		{
			rwcsegUnprotected = Common::Patches::FixRwcseg_Patterns();
		}
	}
}

static VOID (WINAPI* pOrgGetStartupInfoA)(LPSTARTUPINFOA);
VOID WINAPI GetStartupInfoA_Hook(LPSTARTUPINFOA lpStartupInfo)
{
	ProcHook();
	pOrgGetStartupInfoA(lpStartupInfo);
}

static uint8_t orgCode[5];
static decltype(SystemParametersInfoA)* pOrgSystemParametersInfoA;
BOOL WINAPI SystemParametersInfoA_OverwritingHook( UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni )
{
	ProcHook();
	Memory::VP::Patch( pOrgSystemParametersInfoA, { orgCode[0], orgCode[1], orgCode[2], orgCode[3], orgCode[4] } );
	return pOrgSystemParametersInfoA( uiAction, uiParam, pvParam, fWinIni );
}

static bool FixRwcseg_Header()
{
	HINSTANCE					hInstance = GetModuleHandle(nullptr);
	PIMAGE_NT_HEADERS			ntHeader = (PIMAGE_NT_HEADERS)((DWORD_PTR)hInstance + ((PIMAGE_DOS_HEADER)hInstance)->e_lfanew);

	// Give _rwcseg proper access rights
	PIMAGE_SECTION_HEADER	pSection = IMAGE_FIRST_SECTION(ntHeader);

	for ( SIZE_T i = 0, j = ntHeader->FileHeader.NumberOfSections; i < j; i++, pSection++ )
	{
		if ( *(uint64_t*)(pSection->Name) == 0x006765736377725F )	// _rwcseg
		{
			DWORD	dwProtect;
			VirtualProtect((LPVOID)((DWORD_PTR)hInstance + pSection->VirtualAddress), pSection->Misc.VirtualSize, PAGE_EXECUTE_READ, &dwProtect);

			DWORD Characteristics = pSection->Characteristics;
			if ( (Characteristics & IMAGE_SCN_CNT_CODE) == 0 )
			{
				Characteristics |= IMAGE_SCN_CNT_CODE;
				Memory::VP::Patch( &ntHeader->OptionalHeader.SizeOfCode, ntHeader->OptionalHeader.SizeOfCode + pSection->Misc.VirtualSize );
			}
			if ( (Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA) != 0 )
			{
				Characteristics &= ~(IMAGE_SCN_CNT_INITIALIZED_DATA);
				Memory::VP::Patch( &ntHeader->OptionalHeader.SizeOfInitializedData, ntHeader->OptionalHeader.SizeOfInitializedData - pSection->Misc.VirtualSize );
			}
			if ( (Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA) != 0 )
			{
				Characteristics &= ~(IMAGE_SCN_CNT_UNINITIALIZED_DATA);
				Memory::VP::Patch( &ntHeader->OptionalHeader.SizeOfUninitializedData, ntHeader->OptionalHeader.SizeOfUninitializedData - pSection->Misc.VirtualSize );
			}
			Memory::VP::Patch( &pSection->Characteristics, Characteristics );
			return true;
		}
	}
	return false;
}

static bool PatchIAT()
{
	HINSTANCE					hInstance = GetModuleHandle(nullptr);
	PIMAGE_NT_HEADERS			ntHeader = (PIMAGE_NT_HEADERS)((DWORD_PTR)hInstance + ((PIMAGE_DOS_HEADER)hInstance)->e_lfanew);

	// Find IAT	
	PIMAGE_IMPORT_DESCRIPTOR	pImports = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD_PTR)hInstance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	// Find kernel32.dll
	for ( ; pImports->Name != 0; pImports++ )
	{
		if ( !_stricmp((const char*)((DWORD_PTR)hInstance + pImports->Name), "KERNEL32.DLL") )
		{
			if ( pImports->OriginalFirstThunk != 0 )
			{
				PIMAGE_IMPORT_BY_NAME*		pFunctions = (PIMAGE_IMPORT_BY_NAME*)((DWORD_PTR)hInstance + pImports->OriginalFirstThunk);

				// kernel32.dll found, find GetStartupInfoA
				for ( ptrdiff_t j = 0; pFunctions[j] != nullptr; j++ )
				{
					if ( !strcmp((const char*)((DWORD_PTR)hInstance + pFunctions[j]->Name), "GetStartupInfoA") )
					{
						// Overwrite the address with the address to a custom GetStartupInfoA
						DWORD			dwProtect[2];
						DWORD_PTR*		pAddress = &((DWORD_PTR*)((DWORD_PTR)hInstance + pImports->FirstThunk))[j];

						VirtualProtect(pAddress, sizeof(DWORD_PTR), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
						pOrgGetStartupInfoA = **(VOID(WINAPI**)(LPSTARTUPINFOA))pAddress;
						*pAddress = (DWORD_PTR)GetStartupInfoA_Hook;
						VirtualProtect(pAddress, sizeof(DWORD_PTR), dwProtect[0], &dwProtect[1]);

						return true;
					}
				}
			}
		}
	}
	return false;
}

static bool PatchIAT_ByPointers()
{
	using namespace Memory::VP;

	pOrgSystemParametersInfoA = SystemParametersInfoA;
	memcpy( orgCode, reinterpret_cast<void*>(pOrgSystemParametersInfoA), sizeof(orgCode) );
	InjectHook( pOrgSystemParametersInfoA, SystemParametersInfoA_OverwritingHook, HookType::Jump );
	return true;
}

static void ApplyDDrawHooks()
{
	rwcsegUnprotected = FixRwcseg_Header();

	bool getStartupInfoHooked = PatchIAT();
	if ( !getStartupInfoHooked )
	{
		PatchIAT_ByPointers();
	}
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	UNREFERENCED_PARAMETER(hinstDLL);
	UNREFERENCED_PARAMETER(lpvReserved);

	if ( fdwReason == DLL_PROCESS_ATTACH )
	{
		ApplyDDrawHooks();
	}

	return TRUE;
}

extern "C" __declspec(dllexport)
uint32_t GetBuildNumber()
{
	return (SILENTPATCH_REVISION_ID << 8) | SILENTPATCH_BUILD_ID;
}
