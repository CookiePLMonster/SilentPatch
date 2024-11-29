#define WIN32_LEAN_AND_MEAN

#define WINVER 0x0501
#define _WIN32_WINNT 0x0501
#define NOMINMAX

#include "Common_ddraw.h"

#include <windows.h>

#include <shlwapi.h>
#include <shlobj.h>
#include "Utils/MemoryMgr.h"
#include "Utils/Patterns.h"

#pragma comment(lib, "shlwapi.lib")

namespace Common {
	char* GetMyDocumentsPath()
	{
		static char	cUserFilesPath[MAX_PATH];

		if ( cUserFilesPath[0] == '\0' )
		{	
			if ( SHGetFolderPathA(nullptr, CSIDL_MYDOCUMENTS, nullptr, SHGFP_TYPE_CURRENT, cUserFilesPath) == S_OK )
			{
				PathAppendA(cUserFilesPath, *ppUserFilesDir);
				CreateDirectoryA(cUserFilesPath, nullptr);
			}
			else
			{
				strcpy_s(cUserFilesPath, "data");
			}
		}
		return cUserFilesPath;
	}

	namespace Patches {

		bool FixRwcseg_Patterns() try
		{
			using namespace hook::txn;

			// _rwcseg can be placed far after the code section, and the default pattern heuristics currently break with it
			// (it's only using SizeOfCode instead of scanning all code sections).
			// To fix this, explicitly scan the entire module
			const uintptr_t module = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));
			PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(module);
			PIMAGE_NT_HEADERS ntHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(module + dosHeader->e_lfanew);

			const uintptr_t sizeOfHeaders = ntHeader->OptionalHeader.SizeOfHeaders;
			const uintptr_t moduleBegin = module + sizeOfHeaders;
			const uintptr_t moduleEnd = module + (ntHeader->OptionalHeader.SizeOfImage - sizeOfHeaders);

			auto begin = make_range_pattern(moduleBegin, moduleEnd, "55 8B EC 50 53 51 52 8B 5D 14 8B 4D 10 8B 45 0C 8B 55 08").get_first<void>();
			auto end = make_range_pattern(moduleBegin, moduleEnd, "9B D9 3D ? ? ? ? 81 25 ? ? ? ? FF FC FF FF 83 0D ? ? ? ? 3F").get_first<void>(31);

			const ptrdiff_t size = reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(begin);
			if ( size > 0 )
			{
				DWORD dwProtect;
				VirtualProtect( begin, size, PAGE_EXECUTE_READ, &dwProtect );
				return true;
			}
			return false;
		}
		catch (const hook::txn_exception&)
		{
			return false;
		}

		// ================= III =================
		void DDraw_III_10( uint32_t width, uint32_t height, const char* desktopText )
		{
			using namespace Memory::DynBase;

			InjectHook(0x580BB0, GetMyDocumentsPath, HookType::Jump);

			if (width != 0 && height != 0)
			{
				Patch<DWORD>(0x581E5E, width);
				Patch<DWORD>(0x581E68, height);
				Patch<const char*>(0x581EA8, desktopText);
			}
			Patch<BYTE>(0x581E72, 32);

			// No 12mb vram check
			Patch<BYTE>(0x581411, 0xEB);

			// No DirectPlay dependency
			Patch<BYTE>(0x5812D6, 0xB8);
			Patch<DWORD>(0x5812D7, 0x900);
		}

		void DDraw_III_11( uint32_t width, uint32_t height, const char* desktopText )
		{
			using namespace Memory::DynBase;

			InjectHook(0x580F00, GetMyDocumentsPath, HookType::Jump);

			if (width != 0 && height != 0)
			{
				Patch<DWORD>(0x58219E, width);
				Patch<DWORD>(0x5821A8, height);
				Patch<const char*>(0x5821E8, desktopText);
			}
			Patch<BYTE>(0x5821B2, 32);

			// No 12mb vram check
			Patch<BYTE>(0x581753, 0xEB);

			// No DirectPlay dependency
			Patch<BYTE>(0x581620, 0xB8);
			Patch<DWORD>(0x581621, 0x900);
		}

		void DDraw_III_Steam( uint32_t width, uint32_t height, const char* desktopText )
		{
			using namespace Memory::DynBase;

			InjectHook(0x580E00, GetMyDocumentsPath, HookType::Jump);

			if (width != 0 && height != 0)
			{
				Patch<DWORD>(0x58208E, width);
				Patch<DWORD>(0x582098, height);
				Patch<const char*>(0x5820D8, desktopText);
			}
			Patch<BYTE>(0x5820A2, 32);

			// No 12mb vram check
			Patch<BYTE>(0x581653, 0xEB);

			// No DirectPlay dependency
			Patch<BYTE>(0x581520, 0xB8);
			Patch<DWORD>(0x581521, 0x900);
		}

		// ================= VC =================
		void DDraw_VC_10( uint32_t width, uint32_t height, const char* desktopText )
		{
			using namespace Memory::DynBase;

			InjectHook(0x602240, GetMyDocumentsPath, HookType::Jump);

			InjectHook(0x601A40, GetMyDocumentsPath, HookType::Call);
			InjectHook(0x601A45, DynBaseAddress(0x601B2F), HookType::Jump);

			if (width != 0 && height != 0)
			{
				Patch<DWORD>(0x600E7E, width);
				Patch<DWORD>(0x600E88, height);
				Patch<const char*>(0x600EC8, desktopText);
			}
			Patch<BYTE>(0x600E92, 32);

			// No 12mb vram check
			Patch<BYTE>(0x601E26, 0xEB);

			// No DirectPlay dependency
			Patch<BYTE>(0x601CA0, 0xB8);
			Patch<DWORD>(0x601CA1, 0x900);
		}

		void DDraw_VC_11( uint32_t width, uint32_t height, const char* desktopText )
		{
			using namespace Memory::DynBase;

			InjectHook(0x602220, GetMyDocumentsPath, HookType::Jump);

			InjectHook(0x601A70, GetMyDocumentsPath, HookType::Call);
			InjectHook(0x601A75, DynBaseAddress(0x601B5F), HookType::Jump);

			if (width != 0 && height != 0)
			{
				Patch<DWORD>(0x600E9E, width);
				Patch<DWORD>(0x600EA8, height);
				Patch<const char*>(0x600EE8, desktopText);
			}
			Patch<BYTE>(0x600EB2, 32);

			// No 12mb vram check
			Patch<BYTE>(0x601E56, 0xEB);

			// No DirectPlay dependency
			Patch<BYTE>(0x601CD0, 0xB8);
			Patch<DWORD>(0x601CD1, 0x900);
		}


		void DDraw_VC_Steam( uint32_t width, uint32_t height, const char* desktopText )
		{
			using namespace Memory::DynBase;

			InjectHook(0x601E60, GetMyDocumentsPath, HookType::Jump);

			InjectHook(0x6016B0, GetMyDocumentsPath, HookType::Call);
			InjectHook(0x6016B5, DynBaseAddress(0x60179F), HookType::Jump);

			if (width != 0 && height != 0)
			{
				Patch<DWORD>(0x600ADE, width);
				Patch<DWORD>(0x600AE8, height);
				Patch<const char*>(0x600B28, desktopText);
			}
			Patch<BYTE>(0x600AF2, 32);

			// No 12mb vram check
			Patch<BYTE>(0x601A96, 0xEB);

			// No DirectPlay dependency
			Patch<BYTE>(0x601910, 0xB8);
			Patch<DWORD>(0x601911, 0x900);
		}

		// ================= COMMON =================
		void DDraw_Common()
		{
			using namespace Memory;
			using namespace hook::txn;

			// Remove FILE_FLAG_NO_BUFFERING from CdStreams
			try
			{
				auto mem = get_pattern("81 7C 24 04 00 08 00 00", 0x12);
				Patch<uint8_t>( mem, 0xEB );
			}
			TXN_CATCH();

			// III: Patch the icon handle to fix missing window icon
			// (This is fixed since VC)
			try
			{
				auto addr = get_pattern("c7 44 24 1c 00 00 00 00 c7 44 24 08 00 20 00", 0x4);
				HICON wndIconIII = LoadIconA(GetModuleHandleA(nullptr), MAKEINTRESOURCEA(0x412)); // Group icon ID

				Patch<HICON>(addr, wndIconIII);
			}
			TXN_CATCH();

			// No censorships
			try
			{
				auto addr = get_pattern( "83 FB 07 74 0A 83 FD 07 74 05 83 FE 07 75 15" );
				Patch( addr, { 0xEB, 0x5E } );	
			}
			TXN_CATCH();

			// unnamed CdStream semaphore
			try
			{
				auto mem = pattern( "8D 04 85 00 00 00 00 50 6A 40 FF 15" ).get_one();

				Patch( mem.get<void>( 0x25 ), { 0x6A, 0x00 } ); // push 0 \ nop
				Nop( mem.get<void>( 0x25 + 2 ), 3 );			
			}
			TXN_CATCH();
		}
	}
}
