#include "StdAfx.h"

#include "Maths.h"
#include "Timer.h"
#include "Common.h"
#include "Common_ddraw.h"
#include "Desktop.h"
#include "VehicleIII.h"
#include "ModelInfoIII.h"
#include "TheFLAUtils.h"
#include "SVF.h"

#include <array>
#include <memory>
#include <shlwapi.h>

#include "Utils/ModuleList.hpp"
#include "Utils/Patterns.h"
#include "Utils/ScopedUnprotect.hpp"
#include "Utils/HookEach.hpp"

#include "debugmenu_public.h"

#pragma comment(lib, "shlwapi.lib")

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

// ============= Mod compatibility stuff =============

namespace ModCompat
{
	// If an old version of III Aircraft is installed, register Skimmer for the "Sit in boat" feature.
	// If III Aircraft is ever updated, it will start exporting GetBuildNumber and will be expected to register
	// Skimmer with SilentPatch by itself (in case an update un-hardcodes its ID). For the current builds, do it ourselves.
	bool IIIAircraftNeedsSkimmerFallback(HMODULE module)
	{
		if (module == nullptr) return false; // III Aircraft not installed

		bool bOldModVersion = true;

		auto func = (uint32_t(*)())GetProcAddress(module, "GetBuildNumber");
		if (func != nullptr)
		{
			bOldModVersion = func() <= 0x100;
		}
		return bOldModVersion;
	}

	namespace Utils
	{
		template<typename AT>
		HMODULE GetModuleHandleFromAddress( AT address )
		{
			HMODULE result = nullptr;
			GetModuleHandleEx( GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, LPCTSTR(address), &result );
			return result;
		}
	}
}

struct PsGlobalType
{
	HWND	window;
	DWORD	instance;
	DWORD	fullscreen;
	DWORD	lastMousePos_X;
	DWORD	lastMousePos_Y;
	DWORD	unk;
	DWORD	diInterface;
	DWORD	diMouse;
	void*	diDevice1;
	void*	diDevice2;
};

struct RsGlobalType
{
	const char*		AppName;
	unsigned int	unkWidth, unkHeight;
	signed int		MaximumWidth;
	signed int		MaximumHeight;
	unsigned int	frameLimit;
	BOOL			quit;
	PsGlobalType*	ps;
	void*			keyboard;
	void*			mouse;
	void*			pad;
};

DebugMenuAPI gDebugMenuAPI;

static void (*DrawRect)(const CRect&,const CRGBA&);
static int*				InstantHitsFiredByPlayer;
static const void*		HeadlightsFix_JumpBack;

static bool*			bWantsToDrawHud;
static bool*			bCamCheck;
static RsGlobalType*	RsGlobal;
static const void*		SubtitlesShadowFix_JumpBack;

auto 					WorldRemove = reinterpret_cast<void(*)(void*)>(hook::get_pattern("8A 43 50 56 24 07", -5));

float GetWidthMult()
{
	static float** ResolutionWidthMult = hook::get_pattern<float*>("D8 0D ? ? ? ? 89 54 24 0C", 2);
	return **ResolutionWidthMult;
}

float GetHeightMult()
{
	static float** ResolutionHeightMult = hook::get_pattern<float*>("D8 0D ? ? ? ? 89 44 24 04 50 D8 0D", 2);
	return **ResolutionHeightMult;
}

void ShowRadarTrace(float fX, float fY, unsigned int nScale, BYTE r, BYTE g, BYTE b, BYTE a)
{
	if ( *bWantsToDrawHud == true && !*bCamCheck )
	{
		float	fWidthMult = GetWidthMult();
		float	fHeightMult = GetHeightMult();

		DrawRect(CRect(	fX - ((nScale+1.0f) * fWidthMult * RsGlobal->MaximumWidth),
						fY + ((nScale+1.0f) * fHeightMult * RsGlobal->MaximumHeight),
						fX + ((nScale+1.0f) * fWidthMult * RsGlobal->MaximumWidth),
						fY - ((nScale+1.0f) * fHeightMult * RsGlobal->MaximumHeight)),
				 CRGBA(0, 0, 0, a));

		DrawRect(CRect(	fX - (nScale * fWidthMult * RsGlobal->MaximumWidth),
						fY + (nScale * fHeightMult * RsGlobal->MaximumHeight),
						fX + (nScale * fWidthMult * RsGlobal->MaximumWidth),
						fY - (nScale * fHeightMult * RsGlobal->MaximumHeight)),
				 CRGBA(r, g, b, a));
	}
}

namespace ScalingFixes
{
	template<std::size_t Index>
	static void (*orgSetScale)(float fX, float fY);

	template<std::size_t Index>
	static void SetScale_Fix(float fX, float fY)
	{
		orgSetScale<Index>(fX * GetWidthMult() * RsGlobal->MaximumWidth, fY * GetHeightMult() * RsGlobal->MaximumHeight);
	}

	HOOK_EACH_INIT(SetScale, orgSetScale, SetScale_Fix);
}

class CGang
{
public:
	int32_t m_vehicleModel;
	int8_t m_gangModelOverride;
	int32_t m_gangWeapons[2];
};

static_assert(sizeof(CGang) == 0x10, "Wrong size: CGang");

static CGang* const Gangs = *hook::get_pattern<CGang*>( "0F BF 4C 24 04 8B 44 24 08 C1 E1 04 89 81", -0x60 + 2 );
void PurpleNinesGlitchFix()
{
	for ( size_t i = 0; i < 9; ++i )
		Gangs[i].m_gangModelOverride = -1;
}

static bool bGameInFocus = true;

static LRESULT (CALLBACK **OldWndProc)(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK CustomWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch ( uMsg )
	{
	case WM_KILLFOCUS:
		bGameInFocus = false;
		break;
	case WM_SETFOCUS:
		bGameInFocus = true;
		break;
	}

	return (*OldWndProc)(hwnd, uMsg, wParam, lParam);
}
static auto* const pCustomWndProc = CustomWndProc;

static void (* const RsMouseSetPos)(RwV2d*) = AddressByVersion<void(*)(RwV2d*)>(0x580D20, 0x581070, 0x580F70);
static void (*orgConstructRenderList)();
void ResetMousePos()
{
	if ( bGameInFocus )
	{
		RwV2d	vecPos = { RsGlobal->MaximumWidth * 0.5f, RsGlobal->MaximumHeight * 0.5f };
		RsMouseSetPos(&vecPos);
	}
	orgConstructRenderList();
}

__declspec(naked) void M16StatsFix()
{
#ifdef _MSC_VER
	_asm
	{
		add		eax, 0x34
		add		ebx, 0x34
		mov		ecx, InstantHitsFiredByPlayer
		inc		dword ptr [ecx]
		ret
	}
#else
	__asm__ volatile
	(
		"add 	eax, 0x34\n"
		"add 	ebx, 0x34\n"
		"mov 	ecx, %[InstantHitsFiredByPlayer]\n"
		"inc 	dword ptr [ecx]\n"
		"ret"
		:: [InstantHitsFiredByPlayer] "m" (InstantHitsFiredByPlayer)
	);
#endif
}

static const float fMinusOne = -1.0f;
__declspec(naked) void HeadlightsFix()
{
#ifdef _MSC_VER
	_asm
	{
		fld		dword ptr [esp+0x708-0x690]
		fcomp	fMinusOne
		fnstsw	ax
		and		ah, 5
		cmp		ah, 1
		jnz		HeadlightsFix_DontLimit
		fld		fMinusOne
		fstp	dword ptr [esp+0x708-0x690]

	HeadlightsFix_DontLimit:
		fld		dword ptr [esp+0x708-0x690]
		fabs
		fld		st
		jmp		HeadlightsFix_JumpBack
	}
#else
	__asm__ volatile
	(
		"fld	dword ptr [esp+0x708-0x690]\n"
		"fcomp	%[fMinusOne]\n"
		"fnstsw	ax\n"
		"and	ah, 5\n"
		"cmp	ah, 1\n"
		"jnz	HeadlightsFix_DontLimit\n"
		"fld	%[fMinusOne]\n"
		"fstp	dword ptr [esp+0x708-0x690]\n"

"HeadlightsFix_DontLimit:\n"
		"fld	dword ptr [esp+0x708-0x690]\n"
		"fabs\n"
		"fld	st\n"
		"jmp	%[HeadlightsFix_JumpBack]"
		:: [fMinusOne] "f" (fMinusOne),
		[HeadlightsFix_JumpBack] "m" (HeadlightsFix_JumpBack)
	);
#endif
}

static float fShadowXSize, fShadowYSize;

void __stdcall Recalculate(signed int nShadow)
{
	fShadowXSize = nShadow * GetWidthMult() * RsGlobal->MaximumWidth;
	fShadowYSize = nShadow * GetHeightMult() * RsGlobal->MaximumHeight;
}

namespace PrintStringShadows
{
	template<uintptr_t addr>
	static const float** margin = reinterpret_cast<const float**>(Memory::DynBaseAddress(addr));

	static void PrintString_Internal(void (*printFn)(float,float,const wchar_t*), float fX, float fY, float fMarginX, float fMarginY, const wchar_t* pText)
	{
		printFn(fX - fMarginX + (fMarginX * GetWidthMult() * RsGlobal->MaximumWidth), fY - fMarginY + (fMarginY * GetHeightMult() * RsGlobal->MaximumHeight), pText);
	}

	template<uintptr_t pFltX, uintptr_t pFltY>
	struct XY
	{
		static inline void (*orgPrintString)(float,float,const wchar_t*);
		static void PrintString(float fX, float fY, const wchar_t* pText)
		{
			PrintString_Internal(orgPrintString, fX, fY, **margin<pFltX>, **margin<pFltY>, pText);
		}

		static void Hook(uintptr_t addr)
		{
			Memory::DynBase::InterceptCall(addr, orgPrintString, PrintString);
		}
	};

	template<uintptr_t pFltX, uintptr_t pFltY>
	struct XYMinus
	{
		static inline void (*orgPrintString)(float,float,const wchar_t*);
		static void PrintString(float fX, float fY, const wchar_t* pText)
		{
			PrintString_Internal(orgPrintString, fX, fY, -(**margin<pFltX>), -(**margin<pFltY>), pText);
		}

		static void Hook(uintptr_t addr)
		{
			Memory::DynBase::InterceptCall(addr, orgPrintString, PrintString);
		}
	};

	template<uintptr_t pFltX>
	struct X
	{
		static inline void (*orgPrintString)(float,float,const wchar_t*);
		static void PrintString(float fX, float fY, const wchar_t* pText)
		{
			PrintString_Internal(orgPrintString, fX, fY, **margin<pFltX>, 0.0f, pText);
		}

		static void Hook(uintptr_t addr)
		{
			Memory::DynBase::InterceptCall(addr, orgPrintString, PrintString);
		}
	};

	template<uintptr_t pFltY>
	struct Y
	{
		static inline void (*orgPrintString)(float,float,const wchar_t*);
		static void PrintString(float fX, float fY, const wchar_t* pText)
		{
			PrintString_Internal(orgPrintString, fX, fY, 0.0f, **margin<pFltY>, pText);
		}

		static void Hook(uintptr_t addr)
		{
			Memory::DynBase::InterceptCall(addr, orgPrintString, PrintString);
		}
	};
}

float FixedRefValue()
{
	return 1.0f;
}

__declspec(naked) void SubtitlesShadowFix()
{
#ifdef _MSC_VER
	_asm
	{
		push	eax
		call	Recalculate
		fadd	dword ptr [esp+0x50+8]
		fadd	fShadowYSize
		jmp		SubtitlesShadowFix_JumpBack
	}
#else
	__asm__ volatile
	(
		"push	eax\n"
		"call	%[Recalculate]\n"
		"fadd	dword ptr [esp+0x50+8]\n"
		"fadd	%[fShadowYSize]\n"
		"jmp 	%[SubtitlesShadowFix_JumpBack]"
		:: [Recalculate] "i" (Recalculate),
		[fShadowYSize] "m" (fShadowYSize),
		[SubtitlesShadowFix_JumpBack] "m" (SubtitlesShadowFix_JumpBack)
	);
#endif
}

__declspec(naked) void III_SensResetFix()
{
#ifdef _MSC_VER
	_asm
	{
		mov     ecx, 0x3A76
		mov     edi, ebp
		fld     dword ptr [ebp+0x194]
		fld     dword ptr [ebp+0x198]
		rep		stosd
		fstp	dword ptr [ebp+0x198]
		fstp	dword ptr [ebp+0x194]
		ret
	}
#else
	__asm__ volatile
	(
		"mov	ecx, 0x3A76\n"
		"mov	edi, ebp\n"
		"fld	dword ptr [ebp+0x194]\n"
		"fld	dword ptr [ebp+0x198]\n"
		"rep	stosd\n"
		"fstp	dword ptr [ebp+0x198]\n"
		"fstp	dword ptr [ebp+0x194]\n"
		"ret"
	);
#endif
}

static void* RadarBoundsCheckCoordBlip_JumpBack = AddressByVersion<void*>(0x4A55B8, 0x4A56A8, 0x4A5638);
static void* RadarBoundsCheckCoordBlip_Count = AddressByVersion<void*>(0x4A55AF, 0x4A569F, 0x4A562F);
__declspec(naked) void RadarBoundsCheckCoordBlip()
{
#ifdef _MSC_VER
	_asm
	{
		mov		edx, RadarBoundsCheckCoordBlip_Count
		cmp		cl, byte ptr [edx]
		jnb		OutOfBounds
		mov     edx, ecx
		mov     eax, [esp+4]
		jmp		RadarBoundsCheckCoordBlip_JumpBack

	OutOfBounds:
		or		eax, -1
		fcompp
		ret
	}
#else
	__asm__ volatile
	(
		"mov	edx, %[RadarBoundsCheckCoordBlip_Count]\n"
		"cmp	cl, byte ptr [edx]\n"
		"jnb	OutOfBounds\n"
		"mov    edx, ecx\n"
		"mov    eax, [esp+4]\n"
		"jmp	%[RadarBoundsCheckCoordBlip_JumpBack]\n"

"OutOfBounds:\n"
		"or		eax, -1\n"
		"fcompp\n"
		"ret"
		:: [RadarBoundsCheckCoordBlip_Count] "m" (RadarBoundsCheckCoordBlip_Count),
		[RadarBoundsCheckCoordBlip_JumpBack] "m" (RadarBoundsCheckCoordBlip_JumpBack)
	);
#endif
}

static void* RadarBoundsCheckEntityBlip_JumpBack = AddressByVersion<void*>(0x4A565E, 0x4A574E, 0x4A56DE);
__declspec(naked) void RadarBoundsCheckEntityBlip()
{
#ifdef _MSC_VER
	_asm
	{
		mov		edx, RadarBoundsCheckCoordBlip_Count
		cmp		cl, byte ptr [edx]
		jnb		OutOfBounds
		mov     edx, ecx
		mov     eax, [esp+4]
		jmp		RadarBoundsCheckEntityBlip_JumpBack

	OutOfBounds:
		or		eax, -1
		ret
	}
#else
	__asm__ volatile
	(
		"mov	edx, %[RadarBoundsCheckCoordBlip_Count]\n"
		"cmp	cl, byte ptr [edx]\n"
		"jnb	OutOfBounds2\n"
		"mov    edx, ecx\n"
		"mov    eax, [esp+4]\n"
		"jmp	%[RadarBoundsCheckEntityBlip_JumpBack]\n"

		"OutOfBounds2:\n"
		"or		eax, -1\n"
		"ret"
		:: [RadarBoundsCheckCoordBlip_Count] "m" (RadarBoundsCheckCoordBlip_Count),
		[RadarBoundsCheckEntityBlip_JumpBack] "m" (RadarBoundsCheckEntityBlip_JumpBack)
	);
#endif
}

char** ppUserFilesDir = AddressByVersion<char**>(0x580C16, 0x580F66, 0x580E66);

static LARGE_INTEGER	FrameTime;
NOBUFFERCHECKS int32_t GetTimeSinceLastFrame()
{
	LARGE_INTEGER	curTime;
	QueryPerformanceCounter(&curTime);
	return int32_t(curTime.QuadPart - FrameTime.QuadPart);
}

static int (*RsEventHandler)(int, void*);
int NewFrameRender(int nEvent, void* pParam)
{
	QueryPerformanceCounter(&FrameTime);
	return RsEventHandler(nEvent, pParam);
}

static void (*orgPickNextNodeToChaseCar)(void*, float, float, void*);
static float PickNextNodeToChaseCarZ = 0.0f;
static void PickNextNodeToChaseCarXYZ( void* vehicle, const CVector& vec, void* chaseTarget )
{
	PickNextNodeToChaseCarZ = vec.z;
	orgPickNextNodeToChaseCar( vehicle, vec.x, vec.y, chaseTarget );
	PickNextNodeToChaseCarZ = 0.0f;
}


static char		aNoDesktopMode[64];

unsigned int __cdecl AutoPilotTimerCalculation_III(unsigned int nTimer, int nScaleFactor, float fScaleCoef)
{
	return nTimer - static_cast<unsigned int>(nScaleFactor * fScaleCoef);
}

__declspec(naked) void AutoPilotTimerFix_III()
{
#ifdef _MSC_VER
	_asm
	{
		push    dword ptr [esp + 0x4]
		push    dword ptr [ebx + 0x10]
		push    eax
		call    AutoPilotTimerCalculation_III
		add     esp, 0xC
		mov     [ebx + 0xC], eax
		add     esp, 0x28
		pop     ebp
		pop     esi
		pop     ebx
		ret     4
	}
#else
	__asm__ volatile
	(
		"push	dword ptr [esp + 0x4]\n"
		"push	dword ptr [ebx + 0x10]\n"
		"push	eax\n"
		"call	%[AutoPilotTimerCalculation_III]\n"
		"add	esp, 0xC\n"
		"mov	[ebx + 0xC], eax\n"
		"add	esp, 0x28\n"
		"pop	ebp\n"
		"pop	esi\n"
		"pop	ebx\n"
		"ret	4"
		:: [AutoPilotTimerCalculation_III] "i" (AutoPilotTimerCalculation_III)
	);
#endif
}

namespace ZeroAmmoFix
{

template<std::size_t Index>
static void (__fastcall *orgGiveWeapon)(void* ped, void*, unsigned int weapon, unsigned int ammo);

template<std::size_t Index>
static void __fastcall GiveWeapon_SP(void* ped, void*, unsigned int weapon, unsigned int ammo)
{
	orgGiveWeapon<Index>(ped, nullptr, weapon, std::max(1u, ammo));
}
HOOK_EACH_INIT(GiveWeapon, orgGiveWeapon, GiveWeapon_SP);

}


// ============= Credits! =============
namespace Credits
{
	static void (*PrintCreditText)(float scaleX, float scaleY, const wchar_t* text, unsigned int& pos, float timeOffset);
	static void (*PrintCreditText_Hooked)(float scaleX, float scaleY, const wchar_t* text, unsigned int& pos, float timeOffset);

	static void PrintCreditSpace( float scale, unsigned int& pos )
	{
		pos += static_cast<unsigned int>( scale * 25.0f );
	}

	constexpr wchar_t xvChar(const wchar_t ch)
	{
		constexpr uint8_t xv = SILENTPATCH_REVISION_ID;
		return ch ^ xv;
	}

	constexpr wchar_t operator "" _xv(const char ch)
	{
		return xvChar(ch);
	}

	static void PrintSPCredits( float scaleX, float scaleY, const wchar_t* text, unsigned int& pos, float timeOffset )
	{
		// Original text we intercepted
		PrintCreditText_Hooked( scaleX, scaleY, text, pos, timeOffset );
		PrintCreditSpace( 2.0f, pos );

		{
			wchar_t spText[] = { 'A'_xv, 'N'_xv, 'D'_xv, '\0'_xv };

			for ( auto& ch : spText ) ch = xvChar(ch);
			PrintCreditText( 1.7f, 1.0f, spText, pos, timeOffset );
		}

		PrintCreditSpace( 2.0f, pos );

		{
			wchar_t spText[] = { 'A'_xv, 'D'_xv, 'R'_xv, 'I'_xv, 'A'_xv, 'N'_xv, ' '_xv, '\''_xv, 'S'_xv, 'I'_xv, 'L'_xv, 'E'_xv, 'N'_xv, 'T'_xv, '\''_xv, ' '_xv,
				'Z'_xv, 'D'_xv, 'A'_xv, 'N'_xv, 'O'_xv, 'W'_xv, 'I'_xv, 'C'_xv, 'Z'_xv, '\0'_xv };

			for ( auto& ch : spText ) ch = xvChar(ch);
			PrintCreditText( 1.7f, 1.7f, spText, pos, timeOffset );
		}
	}
}

// ============= Keyboard latency input fix =============
namespace KeyboardInputFix
{
	static void* NewKeyState;
	static void* OldKeyState;
	static void* TempKeyState;
	static constexpr size_t objSize = 0x270;
	static void (__fastcall *orgClearSimButtonPressCheckers)(void*);
	void __fastcall ClearSimButtonPressCheckers(void* pThis)
	{
		memcpy( OldKeyState, NewKeyState, objSize );
		memcpy( NewKeyState, TempKeyState, objSize );

		orgClearSimButtonPressCheckers(pThis);
	}
}

namespace Localization
{
	static int8_t forcedUnits = -1; // 0 - metric, 1 - imperial

	bool IsMetric_LocaleBased()
	{
		if ( forcedUnits != -1 ) return forcedUnits == 0;

		unsigned int LCData;
		if ( GetLocaleInfo( LOCALE_USER_DEFAULT, LOCALE_IMEASURE|LOCALE_RETURN_NUMBER, reinterpret_cast<LPTSTR>(&LCData), sizeof(LCData) / sizeof(TCHAR) ) != 0 )
		{
			return LCData == 0;
		}

		// If fails, default to metric. Hopefully never fails though
		return true;
	}

	static void (__thiscall* orgUpdateCompareFlag_IsMetric)(void* pThis, uint8_t flag);
	void __fastcall UpdateCompareFlag_IsMetric(void* pThis, void*, uint8_t)
	{
		std::invoke( orgUpdateCompareFlag_IsMetric, pThis, IsMetric_LocaleBased() );
	}

	uint32_t PrefsLanguage_IsMetric()
	{
		return IsMetric_LocaleBased();
	}
}


// ============= Call cDMAudio::IsAudioInitialised before adding one shot sounds, like in VC =============
namespace AudioInitializedFix
{
	auto IsAudioInitialised = reinterpret_cast<bool(*)()>(Memory::ReadCallFrom( hook::get_pattern( "E8 ? ? ? ? 84 C0 74 ? 0F B7 47 10" ) ));
	void* (*operatorNew)(size_t size);

	void* operatorNew_InitializedCheck( size_t size )
	{
		return IsAudioInitialised() ? operatorNew( size ) : nullptr;
	}

	void (*orgLoadAllAudioScriptObjects)(uint8_t*, uint32_t);
	void LoadAllAudioScriptObjects_InitializedCheck( uint8_t* buffer, uint32_t a2 )
	{
		if ( IsAudioInitialised() )
		{
			orgLoadAllAudioScriptObjects( buffer, a2 );
		}
	}
};


// ============= Corrected FBI Car secondary siren sound =============
namespace SirenSwitchingFix
{
	static bool (__thiscall *orgUsesSirenSwitching)(void* pThis, unsigned int index);
	static bool __fastcall UsesSirenSwitching_FbiCar( void* pThis, void*, unsigned int index )
	{
		// index 17 = FBICAR
		return index == 17 || orgUsesSirenSwitching( pThis, index );
	}
};


// ============= Fixed vehicles exploding twice if the driver leaves the car while it's exploding =============
namespace RemoveDriverStatusFix
{
	__declspec(naked) static void RemoveDriver_SetStatus()
	{
		// if (m_nStatus != STATUS_WRECKED)
		//   m_nStatus = STATUS_ABANDONED;
#ifdef _MSC_VER
		_asm
		{
			mov		ah, [ecx+0x50]
			mov		al, ah
			and		ah, 0xF8
			cmp		ah, 0x28
			je		DontSetStatus
			and     al, 7
			or      al, 0x20

		DontSetStatus:
			ret
		}
#else
		__asm__ volatile
		(
			"mov	ah, [ecx+0x50]\n"
			"mov	al, ah\n"
			"and	ah, 0xF8\n"
			"cmp	ah, 0x28\n"
			"je		DontSetStatus\n"
			"and    al, 7\n"
			"or     al, 0x20\n"

		"DontSetStatus:\n"
			"ret"
		);
#endif
	}
}


// ============= Apply bilinear filtering on the player skin =============
namespace SkinTextureFilter
{
	static RwTexture* (*orgRwTextureCreate)(RwRaster* raster);
	static RwTexture* RwTextureCreate_SetLinearFilter(RwRaster* raster)
	{
		RwTexture* texture = orgRwTextureCreate(raster);
		RwTextureSetFilterMode(texture, rwFILTERLINEAR);
		return texture;
	}
}


// ============= Fix the evasive dive miscalculating the angle, resulting in peds diving towards the vehicle =============
namespace EvasiveDiveFix
{
	static float CalculateAngle(float x, float y)
	{
		float angle = static_cast<float>(CGeneral::GetRadianAngleBetweenPoints(x, y, 0.0f, 0.0f) - M_PI_2);
		if ((rand() & 1) != 0)
		{
			angle += static_cast<float>(M_PI);
		}
		return CGeneral::LimitRadianAngle(angle);
	}

	__declspec(naked) static void CalculateAngle_Hook()
	{
#ifdef _MSC_VER
		_asm
		{
			push    dword ptr [esi+0x7C]
			push	dword ptr [esi+0x78]
			call	CalculateAngle
			add		esp, 8

			mov     ecx, ebp
			ret
		}
#else
		__asm__ volatile
		(
			"push	dword ptr [esi+0x7C]\n"
			"push	dword ptr [esi+0x78]\n"
			"call	%[CalculateAngle]\n"
			"add	esp, 8\n"

			"mov	ecx, ebp\n"
			"ret"
			:: [CalculateAngle] "i" (CalculateAngle)
		);
#endif
	}
}


// ============= Null terminate read lines in CPlane::LoadPath and CTrain::ReadAndInterpretTrackFile =============
namespace NullTerminatedLines
{
	static char* gString;

	static void* orgSscanf_LoadPath;
	__declspec(naked) static void sscanf1_LoadPath_Terminate()
	{
#ifdef _MSC_VER
		_asm
		{
			mov		eax, [esp+4]
			mov		byte ptr [eax+ecx], 0
			jmp		orgSscanf_LoadPath
		}
#else
		__asm__ volatile
		(
			"mov	eax, [esp+4]\n"
			"mov	byte ptr [eax+ecx], 0\n"
			"jmp	%[orgSscanf_LoadPath]"
			:: [orgSscanf_LoadPath] "m" (orgSscanf_LoadPath)
		);
#endif
	}

	static void* orgSscanf1;
	__declspec(naked) static void sscanf1_Terminate()
	{
#ifdef _MSC_VER
		_asm
		{
			mov		eax, [esp+4]
			mov		byte ptr [eax+ecx], 0
			jmp		orgSscanf1
		}
#else
		__asm__ volatile
		(
			"mov	eax, [esp+4]\n"
			"mov	byte ptr [eax+ecx], 0\n"
			"jmp	%[orgSscanf1]"
			:: [orgSscanf1] "m" (orgSscanf1)
		);
#endif
	}

	__declspec(naked) static void ReadTrackFile_Terminate()
	{
#ifdef _MSC_VER
		_asm
		{
			mov		ecx, gString
			mov		byte ptr [ecx+edx], 0
			mov     ecx, [esi]
			inc     ebp
			add     ecx, [esp+0xAC-0x98]
			ret
		}
#else
		__asm__ volatile
		(
			"mov	ecx, %[gString]\n"
			"mov	byte ptr [ecx+edx], 0\n"
			"mov	ecx, [esi]\n"
			"inc	ebp\n"
			"add	ecx, [esp+0xAC-0x98]\n"
			"ret"
			:: [gString] "m" (gString)
		);
#endif
	}
}


// ============= Backport 1.1 Stats menu font fix to 1.0 =============
namespace StatsMenuFont
{
	static void (*orgSetFontStyle)(short);

	static int (__thiscall *orgConstructStatLine)(void* obj, int);
	static int __fastcall ConstructStatLine_SetFontStyle(void* obj, void*, int index)
	{
		const int result = orgConstructStatLine(obj, index);
		orgSetFontStyle(0);
		return result;
	}
}

// ============= Enable Dodo keyboard controls for all cars when the flying cars cheat is enabled =============
namespace DodoKeyboardControls
{
	static bool* bAllDodosCheat;

	static void* (*orgFindPlayerVehicle)();
	__declspec(naked) static void FindPlayerVehicle_DodoCheck()
	{
#ifdef _MSC_VER
		_asm
		{
			call	orgFindPlayerVehicle
			mov		ecx, bAllDodosCheat
			cmp		byte ptr [ecx], 0
			je		CheatDisabled
			mov		byte ptr [esp+0x1C-0x14], 1

		CheatDisabled:
			ret
		}
#else
		__asm__ volatile
		(
			"call	%[orgFindPlayerVehicle]\n"
			"mov	ecx, %[bAllDodosCheat]\n"
			"cmp	byte ptr [ecx], 0\n"
			"je		CheatDisabled\n"
			"mov	byte ptr [esp+0x1C-0x14], 1\n"

		"CheatDisabled:\n"
			"ret"
			:: [orgFindPlayerVehicle] "m" (orgFindPlayerVehicle),
			[bAllDodosCheat] "m" (bAllDodosCheat)
		);
#endif
	}
}


// ============= Resetting stats and variables on New Game =============
namespace VariableResets
{
	static void (*TimerInitialise)();

	using VarVariant = std::variant< bool*, int* >;
	std::vector<VarVariant> GameVariablesToReset;

	static void ReInitOurVariables()
	{
		for ( const auto& var : GameVariablesToReset )
		{
			std::visit( []( auto&& v ) {
				*v = {};
				}, var );
		}

		PurpleNinesGlitchFix();
	}

	template<std::size_t Index>
	static void (*orgReInitGameObjectVariables)();

	template<std::size_t Index>
	void ReInitGameObjectVariables()
	{
		// First reinit "our" variables in case stock ones rely on those during resetting
		ReInitOurVariables();
		orgReInitGameObjectVariables<Index>();
	}
	HOOK_EACH_INIT(ReInitGameObjectVariables, orgReInitGameObjectVariables, ReInitGameObjectVariables);

	static void (*orgGameInitialise)(const char*);
	void GameInitialise(const char* path)
	{
		ReInitOurVariables();
		TimerInitialise();
		orgGameInitialise(path);
	}

	static void (__fastcall* DestroyAllGameCreatedEntities)(void* DMAudio);

	template<std::size_t Index>
	static void (__fastcall* orgService)(void* DMAudio);

	template<std::size_t Index>
	static void __fastcall Service_AndDestroyEntities(void* DMAudio)
	{
		DestroyAllGameCreatedEntities(DMAudio);
		orgService<Index>(DMAudio);
	}
	HOOK_EACH_INIT(Service, orgService, Service_AndDestroyEntities);
}


// ============= Clean up the pickup object when reusing a temporary slot =============
// This has been fixed in VC/SA
namespace GenerateNewPickup_ReuseObjectFix
{
	static void* pPickupObject;
	static void (*orgGiveUsAPickUpObject)(int);

	__declspec(naked) static void GiveUsAPickUpObject_CleanUpObject()
	{
#ifdef _MSC_VER
		_asm
		{
			mov		eax, pPickupObject
			add		eax, ebp
			mov		eax, [eax]
			test	eax, eax
			jz		NoPickup
			push	edi
			mov		edi, eax

			push	edi
			call	WorldRemove
			add		esp, 4

			// Call dtor
			mov		ecx, edi
			mov		eax, [edi]
			push	1
			call	dword ptr [eax]

			pop		edi

		NoPickup:
			jmp		orgGiveUsAPickUpObject
		}
#else
		__asm__ volatile
		(
			"mov	eax, %[pPickupObject]\n"
			"add	eax, ebp\n"
			"mov	eax, [eax]\n"
			"test	eax, eax\n"
			"jz		NoPickup\n"
			"push	edi\n"
			"mov	edi, eax\n"

			"push	edi\n"
			"call	offset %[WorldRemove]\n"
			"add	esp, 4\n"

			// Call dtor
			"mov	ecx, edi\n"
			"mov	eax, [edi]\n"
			"push	1\n"
			"call	dword ptr [eax]\n"

			"pop	edi\n"

		"NoPickup:\n"
			"jmp	%[orgGiveUsAPickUpObject]"
			:: [pPickupObject] "m" (pPickupObject),
			[WorldRemove] "m" (WorldRemove),
			[orgGiveUsAPickUpObject] "m" (orgGiveUsAPickUpObject)
		);
#endif
	}
}


// ============= Sitting in boat (Speeder), implemented as a special vehicle feature =============
// Based off SitInBoat from Fire_Head
namespace SitInBoat
{
	// We only need a single flag...
	struct Ped
	{
		std::byte gap[348];
		bool unused1 : 1;
		bool unused2 : 1;
		bool bVehExitWillBeInstant : 1;
	};

	static bool bSitInBoat = false;

	template<std::size_t Index>
	static void (__fastcall *orgRegisterReference)(CVehicle* pThis, void*, CVehicle** pReference);

	template<std::size_t Index>
	static void __fastcall RegisterReference_CheckSitInBoat(CVehicle* pThis, void*, CVehicle** pReference)
	{
		bSitInBoat = SVF::ModelHasFeature(pThis->GetModelIndex(), SVF::Feature::SIT_IN_BOAT);
		orgRegisterReference<Index>(pThis, nullptr, pReference);
	}

	HOOK_EACH_INIT(CheckSitInBoat, orgRegisterReference, RegisterReference_CheckSitInBoat);

	template<std::size_t Index>
	static void* (*orgBlendAnimation)(void*, unsigned int, unsigned int, float);

	template<std::size_t Index>
	static void* BlendAnimation_SitInBoat(void* clump, unsigned int groupId, unsigned int animationId, float factor)
	{
		if (bSitInBoat)
		{
			animationId = 0x6F; // ANIMATION_CAR_SIT
		}
		return orgBlendAnimation<Index>(clump, groupId, animationId, factor);
	}

	HOOK_EACH_INIT(BlendAnimation, orgBlendAnimation, BlendAnimation_SitInBoat);

	using FinishCB = void(*)(void*, void*);
	static void __fastcall FinishCallback_CallImmediately(void*, void*, FinishCB cb, Ped* ped)
	{
		cb(nullptr, ped);
		ped->bVehExitWillBeInstant = true;
	}
}


// ============= Fixed Brightness saving (fixed in VC) =============
namespace FixedBrightnessSaving
{
	// Heuristics we use for determining whether the value is stock or from SilentPatch
	// 0 - must have been the default brightness of 256 = stock
	// >= 31 - must have been the default brightness of 256 + x = stock
	// Otherwise, this must be the "value index" from SilentPatch
	int (*orgRead)(int, char*, int);
	int Read_Brightness(int file, int* brightness, int size)
	{
		// This should never happen
		if (size != 1)
		{
			return orgRead(file, reinterpret_cast<char*>(brightness), size);
		}

		uint8_t tmp;
		int result = orgRead(file, reinterpret_cast<char*>(&tmp), sizeof(tmp));
		if (result != sizeof(tmp))
		{
			return result;
		}
		
		if (tmp == 0 || tmp >= 31)
		{
			*brightness = 256 + tmp;
		}
		else
		{
			*brightness = (tmp - 1) << 5;
		}

		return result;
	}

	int (*orgWrite)(int, const char*, int);
	int Write_Brightness(int file, const int* brightness, int size)
	{
		// This should never happen
		if (size != 1)
		{
			return orgWrite(file, reinterpret_cast<const char*>(brightness), size);
		}

		const uint32_t brightnessRounded = (*brightness + 31u) & -32;
		const uint8_t brightnessPacked = static_cast<uint8_t>(brightnessRounded >> 5) + 1;
		return orgWrite(file, reinterpret_cast<const char*>(&brightnessPacked), sizeof(brightnessPacked));
	}
}


// ============= Radar position and radardisc scaling =============
namespace RadardiscFixes
{
	template<std::size_t Index>
	static const float* orgRadarXPos;

	template<std::size_t Index>
	static float RadarXPos_Recalculated;

	template<std::size_t... I>
	static void RecalculateXPositions(std::index_sequence<I...>)
	{
		const float multiplier = GetWidthMult() * RsGlobal->MaximumWidth;
		((RadarXPos_Recalculated<I> = *orgRadarXPos<I> * multiplier), ...);
	}

	template<std::size_t Index>
	static const float* orgRadarYPos;

	template<std::size_t Index>
	static float RadarYPos_Recalculated;

	template<std::size_t... I>
	static void RecalculateYPositions(std::index_sequence<I...>)
	{
		const float multiplier = GetHeightMult() * RsGlobal->MaximumHeight;
		((RadarYPos_Recalculated<I> = *orgRadarYPos<I> * multiplier), ...);
	}

	static void (*orgDrawMap)();
	template<std::size_t NumXPos, std::size_t NumYPos>
	static void DrawMap_RecalculatePositions()
	{
		RecalculateXPositions(std::make_index_sequence<NumXPos>{});
		RecalculateYPositions(std::make_index_sequence<NumYPos>{});
		orgDrawMap();
	}

	HOOK_EACH_INIT(CalculateRadarXPos, orgRadarXPos, RadarXPos_Recalculated);
	HOOK_EACH_INIT(CalculateRadarYPos, orgRadarYPos, RadarYPos_Recalculated);
}

// ============= Fix the onscreen counter bar X placement not scaling to resolution =============
namespace OnscreenCounterBarFixes
{
	template<std::size_t Index>
	static const float* orgXPos;

	template<std::size_t Index>
	static float XPos_Recalculated;

	template<std::size_t Index>
	static const float* orgYPos;

	template<std::size_t Index>
	static float YPos_Recalculated;

	template<std::size_t... I>
	static void RecalculateXPositions(std::index_sequence<I...>)
	{
		const float multiplier = GetWidthMult() * RsGlobal->MaximumWidth;
		((XPos_Recalculated<I> = *orgXPos<I> * multiplier), ...);
	}

	template<std::size_t... I>
	static void RecalculateYPositions(std::index_sequence<I...>)
	{
		const float multiplier = GetHeightMult() * RsGlobal->MaximumHeight;
		((YPos_Recalculated<I> = *orgYPos<I> * multiplier), ...);
	}

	static int (*orgAtoi)(const char* str);

	template<std::size_t NumXPos, std::size_t NumYPos>
	static int atoi_RecalculatePositions(const char* str)
	{
		RecalculateXPositions(std::make_index_sequence<NumXPos>{});
		RecalculateYPositions(std::make_index_sequence<NumYPos>{});

		return orgAtoi(str);
	}

	HOOK_EACH_INIT(XPos, orgXPos, XPos_Recalculated);
	HOOK_EACH_INIT(YPos, orgYPos, YPos_Recalculated);
}

// ============= Fix credits not scaling to resolution =============
namespace CreditsScalingFixes
{
	static const unsigned int FIXED_RES_HEIGHT_SCALE = 448;

	template<std::size_t Index>
	static void (*orgPrintString)(float,float,const wchar_t*);

	template<std::size_t Index>
	static void PrintString_ScaleY(float fX, float fY, const wchar_t* pText)
	{
		orgPrintString<Index>(fX, fY * GetHeightMult() * RsGlobal->MaximumHeight, pText);
	}

	static void (*orgSetScale)(float X, float Y);
	static void SetScale_ScaleToRes(float X, float Y)
	{
		orgSetScale(X * GetWidthMult() * RsGlobal->MaximumWidth, Y * GetHeightMult() * RsGlobal->MaximumHeight);
	}

	HOOK_EACH_INIT(PrintString, orgPrintString, PrintString_ScaleY);
}


// ============= Fix some big messages staying on screen longer at high resolutions due to a cut sliding text feature =============
namespace SlidingTextsScalingFixes
{
	static const unsigned int FIXED_RES_WIDTH_SCALE = 640;

	static std::array<float, 6>* pBigMessageX;
	static float* pOddJob2XOffset;

	template<std::size_t BigMessageIndex>
	struct BigMessageSlider
	{
		static inline bool bSlidingEnabled = false;

		static inline float** pHorShadowValue;

		template<std::size_t Index>
		STATIC_INLINE void (*orgPrintString)(float,float,const wchar_t*);

		template<std::size_t Index>
		static void PrintString_Slide(float fX, float fY, const wchar_t* pText)
		{
			if (bSlidingEnabled)
			{
				// We divide by a constant 640.0, because the X position is meant to slide across the entire screen
				fX = (*pBigMessageX)[BigMessageIndex] * RsGlobal->MaximumWidth / 640.0f;
				// The first draws are shadows, add the shadow offset manually. We know this function is called BEFORE the one fixing the shadow scale,
				// so we're fine with this approach.
				if constexpr (Index == 0)
				{
					fX += **pHorShadowValue;
				}
			}
			orgPrintString<Index>(fX, fY, pText);
		}

		HOOK_EACH_INIT(PrintString, orgPrintString, PrintString_Slide);
	};

	struct OddJob2Slider
	{
		static inline bool bSlidingEnabled = false;

		template<std::size_t Index>
		STATIC_INLINE void (*orgPrintString)(float,float,const wchar_t*);

		template<std::size_t Index>
		static void PrintString_Slide(float fX, float fY, const wchar_t* pText)
		{
			// We divide by a constant 640.0, because the X position is meant to slide across the entire screen
			if (bSlidingEnabled)
			{
				fX -= *pOddJob2XOffset * RsGlobal->MaximumWidth / 640.0f;
			}
			orgPrintString<Index>(fX, fY, pText);
		}

		HOOK_EACH_INIT(PrintString, orgPrintString, PrintString_Slide);
	};
}

// ============= Rsstore the 'brakelights' dummy as it's vanished from the III PC code =============
namespace BrakelightsDummy
{
	struct RwObjectNameIdAssocation
	{
		const char*	pName;
		int32_t		hierId;
		uint32_t	flags;
	};

	// This vector is "null terminated" with an empty entry
	std::vector<RwObjectNameIdAssocation> carIds;

	static CVector* (__thiscall* orgVectorCtor)(CVector* pThis, const CVector* vec);
	static CVector* __fastcall VectorCtor_Brakelights(CVector* pThis, void*, const char* vec)
	{
		// We are given a pointer to CAR_POS_TAILLIGHTS (ID 1), but we want CAR_POS_BRAKELIGHTS (ID 4), synthesise one
		const CVector* brakeLights = reinterpret_cast<const CVector*>(vec + 3*sizeof(CVector));
		
		// If all 3 components are 0.0, assume it's a modded vehicle and fall back to the stock dummy
		if (brakeLights->x == 0.0f && brakeLights->y == 0.0f && brakeLights->z == 0.0f)
		{
			brakeLights = reinterpret_cast<const CVector*>(vec);
		}
		return orgVectorCtor(pThis, brakeLights);
	}
}


// ============= Fix text background padding not scaling to resolution =============
namespace TextRectPaddingScalingFixes
{
	template<std::size_t Index>
	static const float* orgPaddingXSize;

	template<std::size_t Index>
	static float PaddingXSize_Recalculated;

	template<std::size_t... I>
	static void RecalculateXSize(std::index_sequence<I...>)
	{
		const float multiplier = GetWidthMult() * RsGlobal->MaximumWidth;
		((PaddingXSize_Recalculated<I> = *orgPaddingXSize<I> * multiplier), ...);
	}

	template<std::size_t Index>
	static const float* orgPaddingYSize;

	template<std::size_t Index>
	static float PaddingYSize_Recalculated;

	template<std::size_t... I>
	static void RecalculateYSize(std::index_sequence<I...>)
	{
		const float multiplier = GetHeightMult() * RsGlobal->MaximumHeight;
		((PaddingYSize_Recalculated<I> = *orgPaddingYSize<I> * multiplier), ...);
	}

	static void (*orgGetTextRect)(CRect*, float, float, void*);
	template<std::size_t NumXPadding, std::size_t NumYPadding>
	static void GetTextRect_Recalculate(CRect* a1, float a2, float a3, void* a4)
	{
		RecalculateXSize(std::make_index_sequence<NumXPadding>{});
		RecalculateYSize(std::make_index_sequence<NumYPadding>{});
		orgGetTextRect(a1, a2, a3, a4);
	}

	HOOK_EACH_INIT(PaddingXSize, orgPaddingXSize, PaddingXSize_Recalculated);
	HOOK_EACH_INIT(PaddingYSize, orgPaddingYSize, PaddingYSize_Recalculated);

	template<std::size_t Index>
	static const float* orgWrapX;

	template<std::size_t Index>
	static float WrapX_Recalculated;

	template<std::size_t... I>
	static void RecalculateWrapX(std::index_sequence<I...>)
	{
		const float multiplier = GetWidthMult() * RsGlobal->MaximumWidth;
		((WrapX_Recalculated<I> = *orgWrapX<I> * multiplier), ...);
	}

	static void (*orgSetJustifyOff)();
	template<std::size_t NumXWrap>
	static void SetJustifyOff_Recalculate()
	{
		RecalculateWrapX(std::make_index_sequence<NumXWrap>{});
		orgSetJustifyOff();
	}

	HOOK_EACH_INIT(WrapX, orgWrapX, WrapX_Recalculated);
}


// ============= Fix menu texts not scaling to resolution =============
namespace MenuManagerScalingFixes
{
	static void (*orgPrintString)(float,float,const wchar_t*);
	static void PrintString_Scale(float fX, float fY, const wchar_t* pText)
	{
		orgPrintString(fX * GetWidthMult() * RsGlobal->MaximumWidth, fY * GetHeightMult() * RsGlobal->MaximumHeight, pText);
	}
}


namespace ModelIndicesReadyHook
{
	static void (*orgInitialiseObjectData)(const char*);
	static void InitialiseObjectData_ReadySVF(const char* path)
	{
		orgInitialiseObjectData(path);
		SVF::MarkModelNamesReady();
	}
}


void InjectDelayedPatches_III_Common( bool bHasDebugMenu, const wchar_t* wcModulePath )
{
	using namespace Memory;
	using namespace hook::txn;

	const ModuleList moduleList;

	const HMODULE hGameModule = GetModuleHandle(nullptr);

	const HMODULE skygfxModule = moduleList.Get(L"skygfx");
	const HMODULE iiiAircraftModule = moduleList.Get(L"IIIAircraft");
	if (skygfxModule != nullptr)
	{
		auto attachCarPipe = reinterpret_cast<void(*)(RwObject*)>(GetProcAddress(skygfxModule, "AttachCarPipeToRwObject"));
		if (attachCarPipe != nullptr)
		{
			CVehicleModelInfo::AttachCarPipeToRwObject = attachCarPipe;
		}
	}

	if (ModCompat::IIIAircraftNeedsSkimmerFallback(iiiAircraftModule))
	{
		SVF::RegisterFeature(156, SVF::Feature::SIT_IN_BOAT);
	}

	auto PatchFloat = [](float** address, const float*& org, float& replaced)
		{
			org = *address;
			Patch(address, &replaced);
		};

	// Locale based metric/imperial system INI/debug menu
	{
		using namespace Localization;

		forcedUnits = static_cast<int8_t>(GetPrivateProfileIntW(L"SilentPatch", L"Units", -1, wcModulePath));
		if ( bHasDebugMenu )
		{
			static const char * const str[] = { "Default", "Metric", "Imperial" };
			DebugMenuEntry *e = DebugMenuAddVar( "SilentPatch", "Forced units", &forcedUnits, nullptr, 1, -1, 1, str );
			DebugMenuEntrySetWrap(e, true);
		}			
	}

	// Make crane be unable to lift Coach instead of Blista
	try
	{
		// There is a possible incompatibility with limit adjusters, so patch it in a delayed hook point and gracefully handle failure
		auto canPickBlista = get_pattern("83 FA 66 74", 2);
		Patch<int8_t>( canPickBlista, 127 ); // coach
	}
	TXN_CATCH();


	// Corrected siren corona placement for emergency vehicles
	if ( GetPrivateProfileIntW(L"SilentPatch", L"EnableVehicleCoronaFixes", -1, wcModulePath) == 1 )
	{
		// Other mods might be touching it, so only patch specific vehicles if their code has not been touched at all
		try
		{
			auto firetruckX1 = get_pattern("C7 84 24 9C 05 00 00 CD CC 8C 3F", 7);
			auto firetruckY1 = get_pattern("C7 84 24 A4 05 00 00 9A 99 D9 3F", 7);
			auto firetruckZ1 = get_pattern("C7 84 24 A8 05 00 00 00 00 00 40", 7);

			auto firetruckX2 = get_pattern("C7 84 24 A8 05 00 00 CD CC 8C BF", 7);
			auto firetruckY2 = get_pattern("C7 84 24 B0 05 00 00 9A 99 D9 3F", 7);
			auto firetruckZ2 = get_pattern("C7 84 24 B4 05 00 00 00 00 00 40", 7);

			constexpr CVector FIRETRUCK_SIREN_POS(0.95f, 3.2f, 1.4f);

			Patch<float>( firetruckX1, FIRETRUCK_SIREN_POS.x );
			Patch<float>( firetruckY1, FIRETRUCK_SIREN_POS.y );
			Patch<float>( firetruckZ1, FIRETRUCK_SIREN_POS.z );

			Patch<float>( firetruckX2, -FIRETRUCK_SIREN_POS.x );
			Patch<float>( firetruckY2, FIRETRUCK_SIREN_POS.y );
			Patch<float>( firetruckZ2, FIRETRUCK_SIREN_POS.z );
		}
		TXN_CATCH();

		try
		{
			auto ambulanceX1 = get_pattern("C7 84 24 84 05 00 00 CD CC 8C 3F", 7);
			auto ambulanceY1 = get_pattern("C7 84 24 8C 05 00 00 66 66 66 3F", 7);
			auto ambulanceZ1 = get_pattern("C7 84 24 90 05 00 00 CD CC CC 3F", 7);

			auto ambulanceX2 = get_pattern("C7 84 24 90 05 00 00 CD CC 8C BF", 7);
			auto ambulanceY2 = get_pattern("C7 84 24 98 05 00 00 66 66 66 3F", 7);
			auto ambulanceZ2 = get_pattern("C7 84 24 9C 05 00 00 CD CC CC 3F", 7);

			constexpr CVector AMBULANCE_SIREN_POS(0.7f, 0.65f, 1.55f);

			Patch<float>( ambulanceX1, AMBULANCE_SIREN_POS.x );
			Patch<float>( ambulanceY1, AMBULANCE_SIREN_POS.y );
			Patch<float>( ambulanceZ1, AMBULANCE_SIREN_POS.z );

			Patch<float>( ambulanceX2, -AMBULANCE_SIREN_POS.x );
			Patch<float>( ambulanceY2, AMBULANCE_SIREN_POS.y );
			Patch<float>( ambulanceZ2, AMBULANCE_SIREN_POS.z );
		}
		TXN_CATCH();

		try
		{
			auto enforcerX1 = get_pattern("C7 84 24 6C 05 00 00 CD CC 8C 3F", 7);
			auto enforcerY1 = get_pattern("C7 84 24 74 05 00 00 CD CC 4C 3F", 7);
			auto enforcerZ1 = get_pattern("C7 84 24 78 05 00 00 9A 99 99 3F", 7);

			auto enforcerX2 = get_pattern("C7 84 24 78 05 00 00 CD CC 8C BF", 7);
			auto enforcerY2 = get_pattern("C7 84 24 80 05 00 00 CD CC 4C 3F", 7);
			auto enforcerZ2 = get_pattern("C7 84 24 84 05 00 00 9A 99 99 3F", 7);

			constexpr CVector ENFORCER_SIREN_POS(0.6f, 1.05f, 1.4f);

			Patch<float>( enforcerX1, ENFORCER_SIREN_POS.x );
			Patch<float>( enforcerY1, ENFORCER_SIREN_POS.y );
			Patch<float>( enforcerZ1, ENFORCER_SIREN_POS.z );

			Patch<float>( enforcerX2, -ENFORCER_SIREN_POS.x );
			Patch<float>( enforcerY2, ENFORCER_SIREN_POS.y );
			Patch<float>( enforcerZ2, ENFORCER_SIREN_POS.z );
		}
		TXN_CATCH();

		try
		{
			auto chopper1 = pattern("C7 44 24 44 00 00 E0 40 50 C7 44 24 4C 00 00 00 00").get_one();	// Front light

			constexpr CVector CHOPPER_SEARCH_LIGHT_POS(0.0f, 3.0f, -1.25f);

			Patch( chopper1.get<float>( 4 ), CHOPPER_SEARCH_LIGHT_POS.y );
			Patch( chopper1.get<float>( 9 + 4 ), CHOPPER_SEARCH_LIGHT_POS.z );
		}
		TXN_CATCH();
	}


	// Corrected FBI Car secondary siren sound
	try
	{
		using namespace SirenSwitchingFix;

		// Other mods might be touching it, so only patch specific vehicles if their code has not been touched at all
		auto usesSirenSwitching = pattern("E8 ? ? ? ? 84 C0 74 12 83 C4 08").get_one();

		InterceptCall(usesSirenSwitching.get<void>(), orgUsesSirenSwitching, UsesSirenSwitching_FbiCar);
	}
	TXN_CATCH();


	// Corrected CSimpleModelInfo::SetupBigBuilding minimum draw distance for big buildings without a matching model
	// Fixes cranes in Portland and bright windows in the city
	// By aap
	try
	{
		auto setupMinDist = pattern("C7 43 44 00 00 C8 42").get_one();

		// mov ecx, ebx
		// call CSimpleModelInfo::SetNearDistanceForLOD
		Patch( setupMinDist.get<void>(), { 0x89, 0xD9 } );
		InjectHook( setupMinDist.get<void>( 2 ), &CSimpleModelInfo::SetNearDistanceForLOD_SilentPatch, HookType::Call );
	}
	TXN_CATCH();


	// Register CBaseModelInfo::GetModelInfo for SVF so we can resolve model names
	try
	{
		using namespace ModelIndicesReadyHook;

		auto initialiseObjectData = get_pattern("E8 ? ? ? ? B3 01 59 8D 6D 04");
		auto getModelInfo = (void*(*)(const char*, int*))get_pattern("31 FF 8D 84 20 00 00 00 00 8B 04 BD", -7);

		InterceptCall(initialiseObjectData, orgInitialiseObjectData, InitialiseObjectData_ReadySVF);
		SVF::RegisterGetModelInfoCB(getModelInfo);
	}
	TXN_CATCH();


	// Fix the radar disc shadow scaling and radar X position
	try
	{
		using namespace RadardiscFixes;
		// We use this overkill pattern so we can get all those constants safely in one sweep. One big scan is better than several smaller ones.
		auto drawRadardisc = pattern("D8 05 ? ? ? ? D9 1C 24 DB 05 ? ? ? ? 50 D8 0D ? ? ? ? D8 0D ? ? ? ? D8 05 ? ? ? ? D8 05 ? ? ? ? D9 1C 24 D9 C0 D8 25 ? ? ? ? 50 D9 1C 24 8D 8C 24 ? ? ? ? FF 35")
								.get_one();
		auto drawRadarMap = pattern("D8 05 ? ? ? ? D9 1C 24 50 D9 14 24 8D 4C 24 24 FF 35").get_one();

		std::array<float**, 6> radarXPos = {
			drawRadardisc.get<float*>(28 + 2),
			drawRadardisc.get<float*>(34 + 2),
			drawRadardisc.get<float*>(62 + 2),

			get_pattern<float*>("D8 05 ? ? ? ? DE C1 D9 19", 2),
			drawRadarMap.get<float*>(2),
			drawRadarMap.get<float*>(17 + 2),
		};

		std::array<float**, 2> radarYPos = {
			drawRadardisc.get<float*>(2),
			drawRadardisc.get<float*>(45 + 2),
		};

		auto drawMap = get_pattern("0F 84 ? ? ? ? E8 ? ? ? ? 8D 8C 24", 6);

		// Undo the damage caused by IVRadarScaling from the widescreen fix moving the radar way too far to the right
		// It's moved from 40.0f to 71.0f, which is way too much now that we're scaling the horizontal placement correctly!
		// This is removed from the most up-to-date widescreen fix, but keep it so we don't break with older builds.
		try
		{
			// Use exactly the same patterns as widescreen fix
			float* radarPos = *get_pattern<float*>("D8 05 ? ? ? ? DE C1 D9 19 8B 15 ? ? ? ? 89 14 24", 2);
			auto radarDisc1 = get_pattern("FF 35 ? ? ? ? DD D8 E8 ? ? ? ? B9 ? ? ? ? 50", 2);
			auto radarDisc2 = get_pattern("D8 05 ? ? ? ? D8 05 ? ? ? ? D9 1C 24 D9 C0 D8 25 ? ? ? ? 50", 2);

			if (hGameModule == ModCompat::Utils::GetModuleHandleFromAddress(radarPos) && *radarPos == (40.0f + 31.0f))
			{
				*radarPos = 40.0f;

				static float STOCK_RADAR_POS = 40.0f;
				static float STOCK_RADARDISC_POS = STOCK_RADAR_POS - 4.0f;
				Patch(radarDisc1, &STOCK_RADARDISC_POS);
				Patch(radarDisc2, &STOCK_RADAR_POS);
			}
		}
		TXN_CATCH();

		HookEach_CalculateRadarXPos(radarXPos, PatchFloat);
		HookEach_CalculateRadarYPos(radarYPos, PatchFloat);
		InterceptCall(drawMap, orgDrawMap, DrawMap_RecalculatePositions<radarXPos.size(), radarYPos.size()>);
	}
	TXN_CATCH();


	// Fix the onscreen counter bar X placement not scaling to resolution
	try
	{
		using namespace OnscreenCounterBarFixes;

		auto atoiWrap = get_pattern("E8 ? ? ? ? 59 8D 8C 24 ? ? ? ? 6A 50");

		std::array<float**, 4> XPositions = {
			get_pattern<float*>("D9 05 ? ? ? ? D8 C1 D9 1C 24", 2),
			get_pattern<float*>("D8 E9 D8 05 ? ? ? ? D9 1C 24", 2 + 2),
			get_pattern<float*>("D8 C2 D8 05 ? ? ? ? D9 1C 24", 2 + 2),
			get_pattern<float*>("D9 05 ? ? ? ? D8 C2 D9 1C 24", 2),
		};

		HookEach_XPos(XPositions, PatchFloat);

		InterceptCall(atoiWrap, orgAtoi, atoi_RecalculatePositions<XPositions.size(), 0>);
	}
	TXN_CATCH();


	// Fix credits not scaling to resolution
	try
	{
		using namespace CreditsScalingFixes;

		std::array<void*, 1> creditPrintString = {
			get_pattern("D9 1C 24 E8 ? ? ? ? 83 C4 0C 8B 03", 3),
		};

		auto setScale = get_pattern("E8 ? ? ? ? 8B 44 24 28 59 59");

		// Credits are timed for 640x480, but the PC version scales to 640x448 internally - speed credits up a bit,
		// especially since ours are longer and even the PS2 version cuts them a bit late.
		float** creditsSpeed = get_pattern<float*>("D8 0D ? ? ? ? D9 5D E8", 2);

		// As we now scale everything on PrintString time, the resolution height checks need to be unscaled.
		void* resHeightScales[] = {
			get_pattern("DF 6C 24 08 DB 05 ? ? ? ? D8 05", 4 + 2),
			get_pattern("8B 35 ? ? ? ? 03 75 F4", 2),
		};

		HookEach_PrintString(creditPrintString, InterceptCall);
		InterceptCall(setScale, orgSetScale, SetScale_ScaleToRes);

		static float newSpeed = **creditsSpeed * (480.0f/448.0f);
		Patch(creditsSpeed, &newSpeed);

		for (void* addr : resHeightScales)
		{
			Patch(addr, &FIXED_RES_HEIGHT_SCALE);
		}
	}
	TXN_CATCH();


	// Fix some big messages staying on screen longer at high resolutions due to a cut sliding text feature
	// Also since we're touching it, optionally allow to re-enable this feature.
	try
	{
		using namespace SlidingTextsScalingFixes;

		// "Unscale" text sliding thresholds, so texts don't stay on screen longer at high resolutions
		auto scalingThreshold = pattern("A1 ? ? ? ? 59 83 C0 EC").count(2);

		scalingThreshold.for_each_result([](pattern_match match)
			{
				Patch(match.get<void>(1), &FIXED_RES_WIDTH_SCALE);
			});

		// Optional sliding texts
		if (const int INIoption = GetPrivateProfileIntW(L"SilentPatch", L"SlidingMissionTitleText", -1, wcModulePath); INIoption != -1) try
		{
			// We need to manually add the shadow offset in this case
			pBigMessageX = *get_pattern<std::array<float, 6>*>("DB 44 24 20 D8 1D ? ? ? ? DF E0", 4 + 2);

			auto bigMessage1ShadowPrint = pattern("D8 05 ? ? ? ? D9 1C 24 DD D8 E8 ? ? ? ? D9 05 ? ? ? ? D9 7C 24 0C").get_one();

			std::array<void*, 2> slidingMessage1 = {
				bigMessage1ShadowPrint.get<void>(0xB),
				get_pattern("E8 ? ? ? ? 83 C4 0C EB 0A C7 05 ? ? ? ? ? ? ? ? 83 C4 68"),
			};

			BigMessageSlider<1>::pHorShadowValue = bigMessage1ShadowPrint.get<float*>(2);
			BigMessageSlider<1>::bSlidingEnabled = INIoption != 0;
			BigMessageSlider<1>::HookEach_PrintString(slidingMessage1, InterceptCall);

			if (bHasDebugMenu)
			{
				DebugMenuAddVar("SilentPatch", "Sliding mission title text", &BigMessageSlider<1>::bSlidingEnabled, nullptr);
			}
		}
		TXN_CATCH();

		if (const int INIoption = GetPrivateProfileIntW(L"SilentPatch", L"SlidingOddJobText", -1, wcModulePath); INIoption != -1) try
		{
			pOddJob2XOffset = *get_pattern<float*>("D9 05 ? ? ? ? D8 E1 D9 1D ? ? ? ? E9", 2);

			std::array<void*, 2> slidingOddJob2 = {
				get_pattern("E8 ? ? ? ? 83 C4 0C 8D 4C 24 5C"),
				get_pattern("E8 ? ? ? ? 83 C4 0C 66 83 3D ? ? ? ? ? 0F 84"),
			};

			OddJob2Slider::bSlidingEnabled = INIoption != 0;
			OddJob2Slider::HookEach_PrintString(slidingOddJob2, InterceptCall);

			if (bHasDebugMenu)
			{
				DebugMenuAddVar("SilentPatch", "Sliding odd job text", &OddJob2Slider::bSlidingEnabled, nullptr);
			}
		}
		TXN_CATCH();
	}
	TXN_CATCH();


	// Rsstore the 'brakelights' dummy as it's vanished from the III PC code
	try
	{
		using namespace BrakelightsDummy;

		auto preRenderBrakeLightsPos = get_pattern("50 E8 ? ? ? ? 8D 84 24 ? ? ? ? 8D 55 04", 1);

		// Only patch the hierarchy if no other mod does it
		RwObjectNameIdAssocation** vehicleDescs = *get_pattern<RwObjectNameIdAssocation**>("89 D9 8B 04 95 ? ? ? ? 50 E8 ? ? ? ? 89 D9", 2 + 3);
		if (ModCompat::Utils::GetModuleHandleFromAddress(vehicleDescs) == hGameModule)
		{
			RwObjectNameIdAssocation* orgCarIds = vehicleDescs[0];
			if (ModCompat::Utils::GetModuleHandleFromAddress(orgCarIds) == hGameModule)
			{
				// Copy the original hierarchy, then add ours
				while (orgCarIds->pName != nullptr)
				{
					carIds.push_back(*orgCarIds++);
				}

				// Add ours, then null terminate
				carIds.push_back({ "brakelights", 4, 9 });
				carIds.push_back({});

				Patch(&vehicleDescs[0], carIds.data());

				InterceptCall(preRenderBrakeLightsPos, orgVectorCtor, VectorCtor_Brakelights);
			}
		}

	}
	TXN_CATCH();


	// Fix text background padding not scaling to resolution
	try
	{
		using namespace TextRectPaddingScalingFixes;

		auto getTextRect = get_pattern("FF 74 24 54 FF 74 24 54 50 E8 ? ? ? ? 83 C4 10", 9);
		auto rectWidth1 = pattern("D8 25 ? ? ? ? D9 18 89 54 24 18 8B 44 24 34 DB 44 24 18 D8 44 24 38 D8 05").get_one();
		auto rectWidth2 = pattern("D8 25 ? ? ? ? D9 18 D9 05 ? ? ? ? D8 0D ? ? ? ? 8B 44 24 34 D8 44 24 38 D8 05").get_one();
		auto rectWidth3 = get_pattern<float*>("D8 25 ? ? ? ? 8B 44 24 34 D9 18", 2);

		auto rectHeight1 = pattern("D8 05 ? ? ? ? D9 58 04 D9 44 24 3C D8 25").count(2);
		auto rectHeight2 = pattern("D9 05 ? ? ? ? D8 44 24 3C DE C1 D8 05").get_one();

		// SetWrapx on the help boxes includes an unscaled -4.0f probably to work together with this padding,
		// so treat it as part of the same fix
		auto setJustifyOff_helpBox = get_pattern("59 E8 ? ? ? ? D9 EE", 1);

		std::array<float**, 5> paddingXSizes = {
			rectWidth1.get<float*>(2),
			rectWidth1.get<float*>(0x18 + 2),
			rectWidth2.get<float*>(2),
			rectWidth2.get<float*>(0x1C + 2),
			rectWidth3
		};

		std::array<float**, 6> paddingYSizes = {
			rectHeight1.get(0).get<float*>(2),
			rectHeight1.get(0).get<float*>(0xD + 2),
			rectHeight1.get(1).get<float*>(2),
			rectHeight1.get(1).get<float*>(0xD + 2),
			rectHeight2.get<float*>(2),
			rectHeight2.get<float*>(0xC + 2),
		};

		std::array<float**, 1> wrapxWidth = {
			get_pattern<float*>("D8 25 ? ? ? ? D9 1C 24 DD D8", 2),
		};

		HookEach_PaddingXSize(paddingXSizes, PatchFloat);
		HookEach_PaddingYSize(paddingYSizes, PatchFloat);
		InterceptCall(getTextRect, orgGetTextRect, GetTextRect_Recalculate<paddingXSizes.size(), paddingYSizes.size()>);

		HookEach_WrapX(wrapxWidth, PatchFloat);
		InterceptCall(setJustifyOff_helpBox, orgSetJustifyOff, SetJustifyOff_Recalculate<wrapxWidth.size()>);
	}
	TXN_CATCH();


	// ============= Fix menu texts not scaling to resolution =============
	try
	{
		using namespace MenuManagerScalingFixes;

		auto printStringMenuText = get_pattern("E8 ? ? ? ? 83 C4 0C DB 05 ? ? ? ? 50");
		InterceptCall(printStringMenuText, orgPrintString, PrintString_Scale);
	}
	TXN_CATCH();

	FLAUtils::Init(moduleList);
}

void InjectDelayedPatches_III_Common()
{
	std::unique_ptr<ScopedUnprotect::Unprotect> Protect = ScopedUnprotect::UnprotectSectionOrFullModule( GetModuleHandle( nullptr ), ".text" );

	// Obtain a path to the ASI
	wchar_t			wcModulePath[MAX_PATH];
	GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), wcModulePath, _countof(wcModulePath) - 3); // Minus max required space for extension
	PathRenameExtensionW(wcModulePath, L".ini");

	const bool hasDebugMenu = DebugMenuLoad();

	InjectDelayedPatches_III_Common( hasDebugMenu, wcModulePath );

	Common::Patches::III_VC_DelayedCommon( hasDebugMenu, wcModulePath );
}


void Patch_III_10(uint32_t width, uint32_t height)
{
	using namespace Memory::DynBase;

	DrawRect = (void(*)(const CRect&,const CRGBA&))DynBaseAddress(0x51F970);

	InstantHitsFiredByPlayer = *(int**)DynBaseAddress(0x482C8F);
	bWantsToDrawHud = *(bool**)DynBaseAddress(0x4A5877);
	bCamCheck = *(bool**)DynBaseAddress(0x4A588C);
	RsGlobal = *(RsGlobalType**)DynBaseAddress(0x584C42);
	HeadlightsFix_JumpBack = (void*)DynBaseAddress(0x5382F2);
	SubtitlesShadowFix_JumpBack = (void*)DynBaseAddress(0x500D32);

	Patch<BYTE>(0x490F83, 1);

	Patch<WORD>(0x5382BF, 0x0EEB);
	InjectHook(0x5382EC, HeadlightsFix, HookType::Jump);

	InjectHook(0x4A5870, ShowRadarTrace, HookType::Jump);
	{
		using namespace ScalingFixes;

		std::array<uintptr_t, 6> set_scale {
			0x4209A7, 0x420A1F, 0x420AC1, 0x420D9E, 0x426342, 0x4326B8
		};

		HookEach_SetScale(set_scale, InterceptCall);
	}

	InjectHook(0x4F9E4D, FixedRefValue);

	InjectHook(0x500D27, SubtitlesShadowFix, HookType::Jump);
	Patch<WORD>(0x500D4C, 0x05D8);
	Patch<WORD>(0x500D5F, 0x05D9);
	Patch<WORD>(0x500D6E, 0x05D9);
	Patch<void*>(0x500D4E, &fShadowXSize);
	Patch<void*>(0x500D70, &fShadowXSize);
	Patch<void*>(0x500D61, &fShadowYSize);
	Patch<DWORD>(0x500D53, 0x0000441F);
	Patch<BYTE>(0x500D52, 0x0F);
	Patch<BYTE>(0x500D65, 0x90);
	Patch<BYTE>(0x500D6D, 0x50);
	Patch<WORD>(0x500D74, 0x9066);

	Patch<BYTE>(0x5623B5, 0x90);
	InjectHook(0x5623B6, M16StatsFix, HookType::Call);

	{
		using namespace PrintStringShadows;

		XY<0x505F7B, 0x505F50>::Hook(0x505F82);
		XY<0x5065D3, 0x5065A8>::Hook(0x5065DA);
		XY<0x50668E, 0x506670>::Hook(0x50669B);
		XY<0x506944, 0x506919>::Hook(0x50694B);
		XY<0x5069FF, 0x5069E1>::Hook(0x506A0C);
		XY<0x506C2B, 0x506C22>::Hook(0x506C37);
		XY<0x5070F3, 0x5070C8>::Hook(0x5070FA);
		XY<0x507591, 0x507566>::Hook(0x507598);
		XY<0x50774D, 0x507722>::Hook(0x507754);
		XY<0x50793D, 0x507912>::Hook(0x507944);
		Y<0x507A8B>::Hook(0x507AC8);
		XY<0x507CE9, 0x507CBE>::Hook(0x507CF0);
		Y<0x507FB4>::Hook(0x507FF1);
		XY<0x508C67, 0x508C46>::Hook(0x508C6E);
		X<0x508F02>::Hook(0x508F09);
		XY<0x42643F, 0x426418>::Hook(0x426446);
		XY<0x42657D, 0x426556>::Hook(0x426584);
		XYMinus<0x426658, 0x426637>::Hook(0x42665F);
		XY<0x509A5E, 0x509A3D>::Hook(0x5098D6);
		XYMinus<0x509A5E, 0x509A3D>::Hook(0x509A65);
		X<0x50A139>::Hook(0x50A142);
		XY<0x57E9EE, 0x57E9CD>::Hook(0x57E9F5);
	}

	// RsMouseSetPos call (SA style fix)
	ReadCall( 0x48E539, orgConstructRenderList );
	InjectHook(0x48E539, ResetMousePos);

	// New wndproc
	OldWndProc = *(LRESULT (CALLBACK***)(HWND, UINT, WPARAM, LPARAM))DynBaseAddress(0x581C74);
	Patch(0x581C74, &pCustomWndProc);

	// Armour cheat as TORTOISE - like in 1.1 and Steam
	Patch<const char*>(0x4925FB, "ESIOTROT");
	
	// BOOOOORING fixed
	Patch<BYTE>(0x4925D7, 10);

	// 1.1 mouse sensitivity not resetting fix
	Patch<WORD>(0x46BE81, 0x12EB);
	Nop(0x46BAD6, 4);
	InjectHook(0x46BADA, III_SensResetFix, HookType::Call);

	// (Hopefully) more precise frame limiter
	ReadCall( 0x582EFD, RsEventHandler );
	InjectHook(0x582EFD, NewFrameRender);
	InjectHook(0x582EA4, GetTimeSinceLastFrame);


	// Radar blips bounds check
	InjectHook(0x4A55B2, RadarBoundsCheckCoordBlip, HookType::Jump);
	InjectHook(0x4A5658, RadarBoundsCheckEntityBlip, HookType::Jump);


	// No-CD fix (from CLEO)
	Patch<DWORD>(0x566A15, 0);
	Nop(0x566A56, 6);
	Nop(0x581C44, 2);
	Nop(0x581C52, 2);
	Patch<const char*>(0x566A3D, "");

	// Fixed crash related to autopilot timing calculations
	InjectHook(0x4139B2, AutoPilotTimerFix_III, HookType::Jump);

	Common::Patches::DDraw_III_10( width, height, aNoDesktopMode );
}

void Patch_III_11(uint32_t width, uint32_t height)
{
	using namespace Memory::DynBase;

	DrawRect = (void(*)(const CRect&,const CRGBA&))DynBaseAddress(0x51FBA0);

	InstantHitsFiredByPlayer = *(int**)DynBaseAddress(0x482D5F);
	bWantsToDrawHud = *(bool**)DynBaseAddress(0x4A5967);
	bCamCheck = *(bool**)DynBaseAddress(0x4A597C);
	RsGlobal = *(RsGlobalType**)DynBaseAddress(0x584F82);
	HeadlightsFix_JumpBack = (void*)DynBaseAddress(0x538532);
	SubtitlesShadowFix_JumpBack = (void*)DynBaseAddress(0x500E12);

	Patch<BYTE>(0x491043, 1);

	Patch<WORD>(0x5384FF, 0x0EEB);
	InjectHook(0x53852C, HeadlightsFix, HookType::Jump);

	InjectHook(0x4A5960, ShowRadarTrace, HookType::Jump);
	{
		using namespace ScalingFixes;

		std::array<uintptr_t, 6> set_scale {
			0x4209A7, 0x420A1F, 0x420AC1, 0x420D9E, 0x426342, 0x4326B8
		};

		HookEach_SetScale(set_scale, InterceptCall);
	}

	InjectHook(0x4F9F2D, FixedRefValue);

	InjectHook(0x500E07, SubtitlesShadowFix, HookType::Jump);
	Patch<WORD>(0x500E2C, 0x05D8);
	Patch<WORD>(0x500E3F, 0x05D9);
	Patch<WORD>(0x500E4E, 0x05D9);
	Patch<void*>(0x500E2E, &fShadowXSize);
	Patch<void*>(0x500E50, &fShadowXSize);
	Patch<void*>(0x500E41, &fShadowYSize);
	Patch<DWORD>(0x500E33, 0x0000441F);
	Patch<BYTE>(0x500E32, 0x0F);
	Patch<BYTE>(0x500E45, 0x90);
	Patch<BYTE>(0x500E4D, 0x50);
	Patch<WORD>(0x500E54, 0x9066);

	Patch<BYTE>(0x5624E5, 0x90);
	InjectHook(0x5624E6, M16StatsFix, HookType::Call);

	{
		using namespace PrintStringShadows;

		XY<0x50605B, 0x506030>::Hook(0x506062);
		XY<0x5066B3, 0x506688>::Hook(0x5066BA);
		XY<0x50676E, 0x506750>::Hook(0x50677B);
		XY<0x506A24, 0x5069F9>::Hook(0x506A2B);
		XY<0x506ADF, 0x506AC1>::Hook(0x506AEC);
		XY<0x506D0B, 0x506D02>::Hook(0x506D17);
		XY<0x5071D3, 0x5071A8>::Hook(0x5071DA);
		XY<0x507671, 0x507646>::Hook(0x507678);
		XY<0x50782D, 0x507802>::Hook(0x507834);
		XY<0x507A1D, 0x5079F2>::Hook(0x507A24);
		Y<0x507B6B>::Hook(0x507BA8);
		XY<0x507DC9, 0x507D9E>::Hook(0x507DD0);
		Y<0x508094>::Hook(0x5080D1);
		XY<0x508D47, 0x508D26>::Hook(0x508D4E);
		X<0x508FE2>::Hook(0x508FE9);
		XY<0x42643F, 0x426418>::Hook(0x426446);
		XY<0x42657D, 0x426556>::Hook(0x426584);
		XYMinus<0x426658, 0x426637>::Hook(0x42665F);
		XY<0x509B3E, 0x509B1D>::Hook(0x5099B6);
		XYMinus<0x509B3E, 0x509B1D>::Hook(0x509B45);
		X<0x50A219>::Hook(0x50A222);
		XY<0x57ED3E, 0x57ED1D>::Hook(0x57ED45);
	}

	// RsMouseSetPos call (SA style fix)
	ReadCall( 0x48E5F9, orgConstructRenderList );
	InjectHook(0x48E5F9, ResetMousePos);

	// New wndproc
	OldWndProc = *(LRESULT (CALLBACK***)(HWND, UINT, WPARAM, LPARAM))DynBaseAddress(0x581FB4);
	Patch(0x581FB4, &pCustomWndProc);

	// (Hopefully) more precise frame limiter
	ReadCall( 0x58323D, RsEventHandler );
	InjectHook(0x58323D, NewFrameRender);
	InjectHook(0x5831E4, GetTimeSinceLastFrame);


	// Radar blips bounds check
	InjectHook(0x4A56A2, RadarBoundsCheckCoordBlip, HookType::Jump);
	InjectHook(0x4A5748, RadarBoundsCheckEntityBlip, HookType::Jump);


	// No-CD fix (from CLEO)
	Patch<DWORD>(0x566B55, 0);
	Nop(0x566B96, 6);
	Nop(0x581F84, 2);
	Nop(0x581F92, 2);
	Patch<const char*>(0x566B7D, "");

	// Fixed crash related to autopilot timing calculations
	InjectHook(0x4139B2, AutoPilotTimerFix_III, HookType::Jump);

	Common::Patches::DDraw_III_11( width, height, aNoDesktopMode );
}

void Patch_III_Steam(uint32_t width, uint32_t height)
{
	using namespace Memory::DynBase;

	DrawRect = (void(*)(const CRect&,const CRGBA&))DynBaseAddress(0x51FB30);

	InstantHitsFiredByPlayer = *(int**)DynBaseAddress(0x482D5F);
	bWantsToDrawHud = *(bool**)DynBaseAddress(0x4A58F7);
	bCamCheck = *(bool**)DynBaseAddress(0x4A590C);
	RsGlobal = *(RsGlobalType**)DynBaseAddress(0x584E72);
	SubtitlesShadowFix_JumpBack = (void*)DynBaseAddress(0x500DA2);

	Patch<BYTE>(0x490FD3, 1);

	InjectHook(0x4A58F0, ShowRadarTrace, HookType::Jump);
	{
		using namespace ScalingFixes;

		std::array<uintptr_t, 6> set_scale {
			0x4209A7, 0x420A1F, 0x420AC1, 0x420D9E, 0x426342, 0x4326B8
		};

		HookEach_SetScale(set_scale, InterceptCall);
	}

	InjectHook(0x4F9EBD, FixedRefValue);

	InjectHook(0x500D97, SubtitlesShadowFix, HookType::Jump);
	Patch<WORD>(0x500DBC, 0x05D8);
	Patch<WORD>(0x500DCF, 0x05D9);
	Patch<WORD>(0x500DDE, 0x05D9);
	Patch<void*>(0x500DBE, &fShadowXSize);
	Patch<void*>(0x500DE0, &fShadowXSize);
	Patch<void*>(0x500DD1, &fShadowYSize);
	Patch<DWORD>(0x500DC3, 0x0000441F);
	Patch<BYTE>(0x500DC2, 0x0F);
	Patch<BYTE>(0x500DD5, 0x90);
	Patch<BYTE>(0x500DDD, 0x50);
	Patch<WORD>(0x500DE4, 0x9066);

	Patch<BYTE>(0x562495, 0x90);
	InjectHook(0x562496, M16StatsFix, HookType::Call);

	{
		using namespace PrintStringShadows;

		XY<0x505FEB, 0x505FC0>::Hook(0x505FF2);
		XY<0x506643, 0x506618>::Hook(0x50664A);
		XY<0x5066FE, 0x5066E0>::Hook(0x50670B);
		XY<0x5069B4, 0x506989>::Hook(0x5069BB);
		XY<0x506A6F, 0x506A51>::Hook(0x506A7C);
		XY<0x506C9B, 0x506C92>::Hook(0x506CA7);
		XY<0x507163, 0x507138>::Hook(0x50716A);
		XY<0x507601, 0x5075D6>::Hook(0x507608);
		XY<0x5077BD, 0x507792>::Hook(0x5077C4);
		XY<0x5079AD, 0x507982>::Hook(0x5079B4);
		Y<0x507AFB>::Hook(0x507B38);
		XY<0x507D59, 0x507D2E>::Hook(0x507D60);
		Y<0x508024>::Hook(0x508061);
		XY<0x508CD7, 0x508CB6>::Hook(0x508CDE);
		X<0x508F72>::Hook(0x508F79);
		XY<0x42643F, 0x426418>::Hook(0x426446);
		XY<0x42657D, 0x426556>::Hook(0x426584);
		XYMinus<0x426658, 0x426637>::Hook(0x42665F);
		XY<0x509ACE, 0x509AAD>::Hook(0x509946);
		XYMinus<0x509ACE, 0x509AAD>::Hook(0x509AD5);
		X<0x50A1A9>::Hook(0x50A1B2);
		XY<0x57EC3E, 0x57EC1D>::Hook(0x57EC45);
	}

	// RsMouseSetPos call (SA style fix)
	ReadCall( 0x48E589, orgConstructRenderList );
	InjectHook(0x48E589, ResetMousePos);

	// New wndproc
	OldWndProc = *(LRESULT (CALLBACK***)(HWND, UINT, WPARAM, LPARAM))DynBaseAddress(0x581EA4);
	Patch(0x581EA4, &pCustomWndProc);

	// (Hopefully) more precise frame limiter
	ReadCall( 0x58312D, RsEventHandler );
	InjectHook(0x58312D, NewFrameRender);
	InjectHook(0x5830D4, GetTimeSinceLastFrame);


	// Radar blips bounds check
	InjectHook(0x4A5632, RadarBoundsCheckCoordBlip, HookType::Jump);
	InjectHook(0x4A56D8, RadarBoundsCheckEntityBlip, HookType::Jump);

	// Fixed crash related to autopilot timing calculations
	InjectHook(0x4139B2, AutoPilotTimerFix_III, HookType::Jump);

	Common::Patches::DDraw_III_Steam( width, height, aNoDesktopMode );
}

void Patch_III_Common()
{
	using namespace Memory;
	using namespace hook::txn;


	// New timers fix
	try
	{
		auto hookPoint = pattern( "83 E4 F8 89 44 24 08 C7 44 24 0C 00 00 00 00 DF 6C 24 08" ).get_one();
		auto jmpPoint = get_pattern( "DD D8 E9 37 FF FF FF DD D8" );

		InjectHook( hookPoint.get<void>( 0x21 ), CTimer::Update_SilentPatch, HookType::Call );
		InjectHook( hookPoint.get<void>( 0x21 + 5 ), jmpPoint, HookType::Jump );
	}
	TXN_CATCH();


	// Alt+F4
	try
	{
		auto addr = pattern( "59 59 31 C0 83 C4 48 5D 5F 5E 5B C2 10 00" ).count(2);
		auto dest = get_pattern( "53 55 56 FF 74 24 68 FF 15" );

		addr.for_each_result( [&]( pattern_match match ) {
			InjectHook( match.get<void>( 2 ), dest, HookType::Jump );
		});
	}
	TXN_CATCH();


	// Proper panels damage
	try
	{
		auto addr = pattern( "C6 43 09 03 C6 43 0A 03 C6 43 0B 03" ).get_one();

		Patch<uint8_t>( addr.get<void>( 0x1A + 1 ), 5 );
		Patch<uint8_t>( addr.get<void>( 0x23 + 1 ), 6 );
		Nop( addr.get<void>( 0x3F ), 7 );
	}
	TXN_CATCH();


	// Proper metric-imperial conversion constants
	try
	{
		static const float METERS_TO_MILES = 0.0006213711922f;
		static const float METERS_TO_FEET = 3.280839895f;
		auto addr = pattern( "D8 0D ? ? ? ? 6A 00 6A 01 D9 9C 24" ).count(4);
		
		Patch<const void*>( addr.get(0).get<void>( 2 ), &METERS_TO_MILES );
		Patch<const void*>( addr.get(1).get<void>( 2 ), &METERS_TO_MILES );

		Patch<const void*>( addr.get(2).get<void>( 2 ), &METERS_TO_FEET );
		Patch<const void*>( addr.get(3).get<void>( 2 ), &METERS_TO_FEET );
	}
	TXN_CATCH();


	// Improved pathfinding in PickNextNodeAccordingStrategy - PickNextNodeToChaseCar with XYZ coords
	try
	{
		auto addr = pattern( "E8 ? ? ? ? 50 8D 44 24 10 50 E8" ).get_one();
		ReadCall( addr.get<void>( 0x25 ), orgPickNextNodeToChaseCar );

		const uintptr_t funcAddr = (uintptr_t)get_pattern( "8B AC 24 94 00 00 00 8B 85 2C 01 00 00", -0x7 );

		// push PickNextNodeToChaseCarZ instead of 0.0f
		Patch( funcAddr + 0x1C9, { 0xFF, 0x35 } );
		Patch<const void*>( funcAddr + 0x1C9 + 2, &PickNextNodeToChaseCarZ );
		Nop( funcAddr + 0x1C9 + 6, 1 );

		// lea eax, [esp+1Ch+var_C]
		// push eax
		// nop...
		Patch( addr.get<void>( 0x10 ), { 0x83, 0xC4, 0x04, 0x8D, 0x44, 0x24, 0x10, 0x50, 0xEB, 0x0A } );
		InjectHook( addr.get<void>( 0x25 ), PickNextNodeToChaseCarXYZ );
		Patch<uint8_t>( addr.get<void>( 0x2A + 2 ), 0xC );

		// push ecx
		// nop...
		Patch<uint8_t>( addr.get<void>( 0x3E ), 0x51 );
		Nop( addr.get<void>( 0x3E + 1 ), 6 );
		InjectHook( addr.get<void>( 0x46 ), PickNextNodeToChaseCarXYZ );
		Patch<uint8_t>( addr.get<void>( 0x4B + 2 ), 0xC );


		// For NICK007J
		// Uncomment this to get rid of "treadable hack" in CCarCtrl::PickNextNodeToChaseCar (to mirror VC behaviour)
		InjectHook( funcAddr + 0x2A, funcAddr + 0x182, HookType::Jump );
	}
	TXN_CATCH();


	// No censorships
	try
	{
		auto addr = get_pattern( "8B 15 ? ? ? ? C6 05 ? ? ? ? 00 89 D0" );
		Patch( addr, { 0x83, 0xC4, 0x08, 0xC3 } );	// add     esp, 8 \ retn
	}
	TXN_CATCH();


	// 014C cargen counter fix (by spaceeinstein)
	try
	{
		auto do_processing = pattern( "0F B7 45 28 83 F8 FF 7D 04 66 FF 4D 28" ).get_one();

		Patch<uint8_t>( do_processing.get<uint8_t*>(1), 0xBF ); // movzx   eax, word ptr [ebx+28h] -> movsx   eax, word ptr [ebx+28h]
		Patch<uint8_t>( do_processing.get<uint8_t*>(7), 0x74 ); // jge -> jz
	}
	TXN_CATCH();


	// Fixed ammo from SCM
	try
	{
		using namespace ZeroAmmoFix;

		std::array<void*, 2> give_weapon = {
			get_pattern( "6B C0 4F 51 8B 34", 0x13 ),
			get_pattern( "89 C7 A1 ? ? ? ? 55 89 F9 50", 11 ),
		};
		HookEach_GiveWeapon(give_weapon, InterceptCall);
	}
	TXN_CATCH();


	// Credits =)
	try
	{
		auto renderCredits = pattern( "83 C4 14 8D 45 F4 50 FF 35 ? ? ? ? E8 ? ? ? ? 59 59 8D 45 F4 50 FF 35 ? ? ? ? E8 ? ? ? ? 59 59 E8" ).get_one();

		ReadCall( renderCredits.get<void>( -48 ), Credits::PrintCreditText );
		ReadCall( renderCredits.get<void>( -5 ), Credits::PrintCreditText_Hooked );
		InjectHook( renderCredits.get<void>( -5 ), Credits::PrintSPCredits );
	}
	TXN_CATCH();


	// Decreased keyboard input latency
	try
	{
		using namespace KeyboardInputFix;

		auto updatePads = pattern( "BE ? ? ? ? BF ? ? ? ? A5" ).get_one();
		void* jmpDest = get_pattern( "66 A3 ? ? ? ? 5F", 6 );
		void* simButtonCheckers = get_pattern( "84 DB 74 11 6A 00", -0x24 );

		NewKeyState = *updatePads.get<void*>( 1 );
		OldKeyState = *updatePads.get<void*>( 5 + 1 );
		TempKeyState = *updatePads.get<void*>( 0x244 + 1 );

		ReadCall( simButtonCheckers, orgClearSimButtonPressCheckers );
		InjectHook( simButtonCheckers, ClearSimButtonPressCheckers );
		InjectHook( updatePads.get<void>( 10 ), jmpDest, HookType::Jump );
	}
	TXN_CATCH();


	// Locale based metric/imperial system
	try
	{
		using namespace Localization;

		void* updateCompareFlag = get_pattern( "89 E9 6A 00 E8 ? ? ? ? 30 C0 83 C4 70 5D 5E 5B C2 04 00", 4 );
		auto constructStatLine = pattern( "FF 24 9D ? ? ? ? 39 D0" ).get_one();

		ReadCall( updateCompareFlag, orgUpdateCompareFlag_IsMetric );
		InjectHook( updateCompareFlag, UpdateCompareFlag_IsMetric );

		// push eax
		// push edx
		// call IsMetric_LocaleBased
		// movzx ebx, al
		// pop edx
		// pop eax
		// nop...
		Patch( constructStatLine.get<void>( -0xF ), { 0x50, 0x52 } );
		InjectHook( constructStatLine.get<void>( -0xF + 2 ), PrefsLanguage_IsMetric, HookType::Call );
		Patch( constructStatLine.get<void>( -0xF + 7 ), { 0x0F, 0xB6, 0xD8, 0x5A, 0x58 } );
		Nop( constructStatLine.get<void>( -0xF + 12 ), 3 );
	}
	TXN_CATCH();


	// Add cDMAudio::IsAudioInitialised checks before constructing cAudioScriptObject, like in VC
	try
	{
		using namespace AudioInitializedFix;

		auto processCommands300 = pattern( "E8 ? ? ? ? 85 C0 59 74 ? 89 C1 E8 ? ? ? ? D9 05" ).get_one();
		auto processCommands300_2 = pattern( "6A 14 E8 ? ? ? ? 89 C3 59 85 DB 74" ).get_one();
		auto bulletInfoUpdate_Switch = *get_pattern<uintptr_t*>( "FF 24 85 ? ? ? ? 6A 14", 3 );
		auto playlayOneShotScriptObject = pattern( "6A 14 E8 ? ? ? ? 85 C0 59 74 ? 89 C1 E8 ? ? ? ? D9 03 D9 58 04" ).get_one();
		auto loadAllAudioScriptObjects = get_pattern( "FF B5 78 FF FF FF E8 ? ? ? ? 59 59 8B 45 C8", 6 );

		ReadCall( processCommands300.get<void>( 0 ), operatorNew );

		InjectHook( processCommands300.get<void>( 0 ), operatorNew_InitializedCheck );
		Patch<int8_t>( processCommands300.get<void>( 8 + 1 ), 0x440B62 - 0x440B24 );

		InjectHook( processCommands300_2.get<void>( 2 ), operatorNew_InitializedCheck );
		Patch<int8_t>( processCommands300_2.get<void>( 0xC + 1 ), 0x440BD7 - 0x440B8B );

		// We need to patch switch cases 0, 3, 4
		const uintptr_t bulletInfoUpdate_0 = bulletInfoUpdate_Switch[0];
		const uintptr_t bulletInfoUpdate_3 = bulletInfoUpdate_Switch[3];
		const uintptr_t bulletInfoUpdate_4 = bulletInfoUpdate_Switch[4];

		InjectHook( bulletInfoUpdate_0 + 2, operatorNew_InitializedCheck );
		Patch<int8_t>( bulletInfoUpdate_0 + 0xA + 1, 0x558B79 - 0x558B36 );

		InjectHook( bulletInfoUpdate_3 + 2, operatorNew_InitializedCheck );
		Patch<int8_t>( bulletInfoUpdate_3 + 0xA + 1, 0x558C19 - 0x558BB1 );

		InjectHook( bulletInfoUpdate_4 + 2, operatorNew_InitializedCheck );
		Patch<int8_t>( bulletInfoUpdate_4 + 0xA + 1, 0x558C19 - 0x558BE3 );

		InjectHook( playlayOneShotScriptObject.get<void>( 2 ), operatorNew_InitializedCheck );
		Patch<int8_t>( playlayOneShotScriptObject.get<void>( 0xA + 1 ), 0x57C633 - 0x57C601 );

		ReadCall( loadAllAudioScriptObjects, orgLoadAllAudioScriptObjects );
		InjectHook( loadAllAudioScriptObjects, LoadAllAudioScriptObjects_InitializedCheck );
	}
	TXN_CATCH();


	// Give chopper/escape a properly sized collision bounding box instead of using ped's
	try
	{
		auto initHelis = pattern( "C6 40 2C 00 A1" ).get_one();

		static constexpr CColModel colModelChopper( CColSphere( 8.5f, CVector(0.0f, -1.75f, 0.73f), 0, 0 ), 
						CColBox( CVector(-2.18f, -8.52f, -0.67f), CVector(-2.18f, 4.58f, 2.125f), 0, 0 ) );

		Patch( initHelis.get<void>( -7 + 3 ), &colModelChopper );
		Patch( initHelis.get<void>( 9 + 3 ), &colModelChopper );
	}
	TXN_CATCH();


	// Fixed vehicles exploding twice if the driver leaves the car while it's exploding
	try
	{
		using namespace RemoveDriverStatusFix;

		auto removeDriver = pattern("8A 41 50 24 07 0C 20 88 41 50 C7 81").get_one();
		auto processCommands1 = get_pattern("88 41 50 8B 87");
		auto processCommands2 = get_pattern("88 41 50 8B 2B");
		auto processCommands3 = get_pattern("0C 20 88 42 50", 2);
		auto processCommands4 = get_pattern("88 41 50 8B BE");
		auto pedSetOutCar = get_pattern("88 41 50 8B 85");

		Nop(removeDriver.get<void>(), 2);
		InjectHook(removeDriver.get<void>(2), RemoveDriver_SetStatus, HookType::Call);

		// CVehicle::RemoveDriver already sets the status to STATUS_ABANDONED, these are redundant
		Nop(processCommands1, 3);
		Nop(processCommands2, 3);
		Nop(processCommands3, 3);
		Nop(processCommands4, 3);
		Nop(pedSetOutCar, 3);
	}
	TXN_CATCH();


	// Fixed an inverted condition in CCarCtrl::PickNextNodeRandomly
	// leading to cars being unable to turn right from one way roads
	// By Nick007J
	try
	{
		auto pickNodeRandomly = get_pattern("3B 44 24 24 74 09", 4);
		Patch<uint8_t>(pickNodeRandomly, 0x75);
	}
	TXN_CATCH();


	// Apply bilinear filtering on the player skin
	try
	{
		using namespace SkinTextureFilter;

		auto getSkinTexture = get_pattern("E8 ? ? ? ? 89 C3 59 55");
		InterceptCall(getSkinTexture, orgRwTextureCreate, RwTextureCreate_SetLinearFilter);
	}
	TXN_CATCH();


	// Apply the environment mapping on extra components
	try
	{
		auto setEnvironmentMap = get_pattern("C7 83 D8 01 00 00 00 00 00 00 E8", 10);
	
		InterceptCall(setEnvironmentMap, CVehicleModelInfo::orgSetEnvironmentMap, &CVehicleModelInfo::SetEnvironmentMap_ExtraComps);
	}
	TXN_CATCH();


	// Fix the evasive dive miscalculating the angle, resulting in peds diving towards the vehicle
	try
	{
		using namespace EvasiveDiveFix;

		auto setEvasiveDive = pattern("D9 44 24 10 89 E9 D9 9D ? ? ? ? E8 ? ? ? ? 89 E9 E8 ? ? ? ? 89 E9 E8 ? ? ? ? C7 85").get_one();

		Nop(setEvasiveDive.get<void>(), 1);
		InjectHook(setEvasiveDive.get<void>(1), &CalculateAngle_Hook, HookType::Call);
	}
	TXN_CATCH();


	// Fix probabilities in CVehicle::InflictDamage incorrectly assuming a random range from 0 to 100.000
	try
	{
		auto probability_do_nothing = get_pattern("66 81 7E 5A ? ? 73 50", 4);
		auto probability_flee = get_pattern("0F B7 46 5A 3D ? ? ? ? 0F 8E", 4 + 1);

		Patch<uint16_t>(probability_do_nothing, 35000u * 32767u / 100000u);
		Patch<uint32_t>(probability_flee, 75000u * 32767u / 100000u);
	}
	TXN_CATCH();


	// Null terminate read lines in CPlane::LoadPath and CTrain::ReadAndInterpretTrackFile
	try
	{
		using namespace NullTerminatedLines;

		auto loadPath = get_pattern("DD D8 45 E8", 3);
		auto readTrackFile1 = pattern("E8 ? ? ? ? 0F BF 07").get_one();
		auto readTrackFile2 = pattern(" 8B 0E 45 03 4C 24 10").get_one();

		gString = *readTrackFile1.get<char*>(-5 + 1);

		InterceptCall(loadPath, orgSscanf_LoadPath, sscanf1_LoadPath_Terminate);

		Patch(readTrackFile1.get<void>(-10 + 1), "%hd");
		InterceptCall(readTrackFile1.get<void>(), orgSscanf1, sscanf1_Terminate);

		Nop(readTrackFile2.get<void>(), 2);
		InjectHook(readTrackFile2.get<void>(2), ReadTrackFile_Terminate, HookType::Call);
	}
	TXN_CATCH();


	// Backport 1.1 Stats menu font fix to 1.0
	try
	{
		using namespace StatsMenuFont;

		// This pattern fails by design on 1.1/Steam
		auto constructStatLine = pattern("E8 ? ? ? ? D9 05 ? ? ? ? DC 0D ? ? ? ? 89 C7").get_one();
		auto setFontStyle = get_pattern("6A 00 E8 ? ? ? ? 83 3D ? ? ? ? ? 59 0F 84", 2);

		ReadCall(setFontStyle, orgSetFontStyle);
		InterceptCall(constructStatLine.get<void>(), orgConstructStatLine, ConstructStatLine_SetFontStyle);
	}
	TXN_CATCH();


	// Enable Dodo keyboard controls for all cars when the flying cars cheat is enabled
	try
	{
		using namespace DodoKeyboardControls;

		auto findPlayerVehicle = get_pattern("E8 ? ? ? ? 0F BF 40 5C 83 F8 7E");
		auto allDodosCheat = *get_pattern<bool*>("80 3D ? ? ? ? ? 74 5B", 2);

		bAllDodosCheat = allDodosCheat;
		InterceptCall(findPlayerVehicle, orgFindPlayerVehicle, FindPlayerVehicle_DodoCheck);
	}
	TXN_CATCH();


	// Reset variables on New Game
	try
	{
		using namespace VariableResets;

		auto game_initialise = get_pattern("6A 00 E8 ? ? ? ? 83 C4 0C 68 ? ? ? ? E8 ? ? ? ? 59 C3", 15);
		std::array<void*, 2> reinit_game_object_variables = {
			get_pattern("E8 ? ? ? ? 80 3D ? ? ? ? ? 75 6B"),
			get_pattern("C6 05 ? ? ? ? ? E8 ? ? ? ? C7 05", 7)
		};

		TimerInitialise = reinterpret_cast<decltype(TimerInitialise)>(get_pattern("83 E4 F8 68 ? ? ? ? E8", -6));

		// In GTA III, we also need to backport one more fix from VC to avoid issues with looping audio entities:
		// CMenuManager::DoSettingsBeforeStartingAGame needs to call cDMAudio::DestroyAllGameCreatedEntities
		DestroyAllGameCreatedEntities = reinterpret_cast<decltype(DestroyAllGameCreatedEntities)>(ReadCallFrom(
			get_pattern("B9 ? ? ? ? E8 ? ? ? ? 31 DB BD ? ? ? ? 8D 40 00", 5)));

		auto audio_service = pattern("B9 ? ? ? ? E8 ? ? ? ? B9 ? ? ? ? C6 05 ? ? ? ? ? E8").count(2);
		std::array<void*, 2> audio_service_instances = {
			audio_service.get(0).get<void>(5),
			audio_service.get(1).get<void>(5),
		};

		InterceptCall(game_initialise, orgGameInitialise, GameInitialise);
		HookEach_ReInitGameObjectVariables(reinit_game_object_variables, InterceptCall);
		HookEach_Service(audio_service_instances, InterceptCall);

		// Variables to reset
		GameVariablesToReset.emplace_back(*get_pattern<bool*>("80 3D ? ? ? ? ? 74 2A", 2)); // Free resprays
		GameVariablesToReset.emplace_back(*get_pattern<int*>("7D 72 A1 ? ? ? ? 05", 2 + 1)); // LastTimeAmbulanceCreated
		GameVariablesToReset.emplace_back(*get_pattern<int*>("74 7F A1 ? ? ? ? 05", 2 + 1)); // LastTimeFireTruckCreated
	}
	TXN_CATCH();


	// Clean up the pickup object when reusing a temporary slot
	try
	{
		using namespace GenerateNewPickup_ReuseObjectFix;

		auto give_us_a_pick_up_object = pattern("6A FF E8 ? ? ? ? 89 85").get_one();

		pPickupObject = *give_us_a_pick_up_object.get<void*>(7 + 2);
		InterceptCall(give_us_a_pick_up_object.get<void>(2), orgGiveUsAPickUpObject, GiveUsAPickUpObject_CleanUpObject);
	}
	TXN_CATCH();


	// Sitting in boat (Speeder), implemented as a special vehicle feature
	// Based off SitInBoat from Fire_Head, with extra improvements
	try
	{
		using namespace SitInBoat;

		std::array<void*, 2> register_reference = {
			get_pattern("E8 ? ? ? ? 8B 83 ? ? ? ? FE 80"),
			get_pattern("E8 ? ? ? ? C7 85 ? ? ? ? ? ? ? ? 8A 45 51 24 FE 88 45 51 8A 85"),
		};
		std::array<void*, 2> blend_animation = {
			get_pattern("6A 7A 6A 00 50 DD D8 E8 ? ? ? ? 83 C4 10", 7),
			get_pattern("E8 ? ? ? ? 89 85 ? ? ? ? 83 C4 10"),
		};
		auto finish_callback = get_pattern("53 68 ? ? ? ? E8 ? ? ? ? 89 D9 E8", 6);

		HookEach_CheckSitInBoat(register_reference, InterceptCall);
		HookEach_BlendAnimation(blend_animation, InterceptCall);

		// This is intended - we don't actually need the original SetFinishCallback, only its parameters!
		InjectHook(finish_callback, FinishCallback_CallImmediately);
	}
	TXN_CATCH();


	// Copy the atomic render CB in CloneAtomicToFrameCB instead of overriding it
	// Fixes detached limbs rendering the normal and LOD atomics together
	try
	{
		auto set_render_cb = get_pattern("55 E8 ? ? ? ? 89 D8 59", 1);
		Nop(set_render_cb, 5);
	}
	TXN_CATCH();


	// Fix dark car reflections in the Steam EXE
	// Based off Sergeanur's fix
	try
	{
		// This will only pass on the Steam EXE, and if Sergeanur's standalone fix isn't present
		auto reflection = pattern("A1 ? ? ? ? 85 C0 74 34").get_one();

		// xor eax, eax \ nop
		Patch(reflection.get<void>(), { 0x31, 0xC0 });
		Nop(reflection.get<void>(2), 3);
	}
	TXN_CATCH();


	// Don't override the color of the FBI car
	try
	{
		auto spawn_one_car = get_pattern("83 7C 24 ? ? 75 0E C6 85", 5);
		Patch<uint8_t>(spawn_one_car, 0xEB);
	}
	TXN_CATCH();


	// Fixed Brightness saving (fixed in VC)
	try
	{
		using namespace FixedBrightnessSaving;

		// Read and Write calls in CMenuManager::LoadSettings and CMenuManager::SaveSettings
		// are virtually indistinguishable from each other, so we need to build raw patterns that include
		// the pointer to CMenuManager::m_PrefsBrightness
		auto prefs_brightness = *get_pattern<int*>("83 3D ? ? ? ? 00 7D 0B", 2);
		uint8_t prefs_brightness_bytes[4];
		memcpy(&prefs_brightness_bytes, &prefs_brightness, sizeof(prefs_brightness_bytes));

		const uint8_t mask[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0, 0x0, 0x0, 0x0, 0xFF, 0xFF, 0xFF };
		const uint8_t read_pattern[] { 0x6A, 0x01, 0x68,
			prefs_brightness_bytes[0], prefs_brightness_bytes[1], prefs_brightness_bytes[2], prefs_brightness_bytes[3],
			0x53, 0xE8, 0x0, 0x0, 0x0, 0x0, 0x83, 0xC4, 0x0C };
		const uint8_t write_pattern[] { 0x6A, 0x01, 0x68,
			prefs_brightness_bytes[0], prefs_brightness_bytes[1], prefs_brightness_bytes[2], prefs_brightness_bytes[3],
			0x55, 0xE8, 0x0, 0x0, 0x0, 0x0, 0x83, 0xC4, 0x0C };

		auto read = pattern({read_pattern, sizeof(read_pattern)}, {mask, sizeof(mask)}).get_first<void>(8);
		auto write = pattern({write_pattern, sizeof(write_pattern)}, {mask, sizeof(mask)}).get_first<void>(8);

		InterceptCall(read, orgRead, Read_Brightness);
		InterceptCall(write, orgWrite, Write_Brightness);
	}
	TXN_CATCH();
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	UNREFERENCED_PARAMETER(hinstDLL);
	UNREFERENCED_PARAMETER(lpvReserved);

	if ( fdwReason == DLL_PROCESS_ATTACH )
	{
		const auto [width, height] = GetDesktopResolution();
		sprintf_s(aNoDesktopMode, "Cannot find %ux%ux32 video mode", width, height);

		// This scope is mandatory so Protect goes out of scope before rwcseg gets fixed
		{
			std::unique_ptr<ScopedUnprotect::Unprotect> Protect = ScopedUnprotect::UnprotectSectionOrFullModule( GetModuleHandle( nullptr ), ".text" );

			const int8_t version = Memory::GetVersion().version;
			if ( version == 0 ) Patch_III_10(width, height);
			else if ( version == 1 ) Patch_III_11(width, height);
			else if ( version == 2 ) Patch_III_Steam(width, height);

			Patch_III_Common();
			Common::Patches::III_VC_Common();
			Common::Patches::DDraw_Common();

			Common::Patches::III_VC_SetDelayedPatchesFunc( InjectDelayedPatches_III_Common );
		}

		Common::Patches::FixRwcseg_Patterns();
	}
	return TRUE;
}

extern "C" __declspec(dllexport)
uint32_t GetBuildNumber()
{
	return (SILENTPATCH_REVISION_ID << 8) | SILENTPATCH_BUILD_ID;
}
