#include "StdAfx.h"

#include "Maths.h"
#include "Timer.h"
#include "Common.h"
#include "Common_ddraw.h"
#include "Desktop.h"
#include "EntityVC.h"
#include "ModelInfoVC.h"
#include "VehicleVC.h"
#include "SVF.h"
#include "RWUtils.hpp"
#include "TheFLAUtils.h"
#include "ParseUtils.hpp"
#include "Random.h"

#include <array>
#include <memory>
#include <shlwapi.h>
#include <time.h>

#include "Utils/ModuleList.hpp"
#include "Utils/Patterns.h"
#include "Utils/ScopedUnprotect.hpp"
#include "Utils/HookEach.hpp"
#include "Utils/DelimStringReader.h"

#include "debugmenu_public.h"

#pragma comment(lib, "shlwapi.lib")

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

// ============= Mod compatibility stuff =============

namespace ModCompat
{
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

struct RsGlobalType
{
	const char*		AppName;
	unsigned int	unkWidth, unkHeight;
	signed int		MaximumWidth;
	signed int		MaximumHeight;
	unsigned int	frameLimit;
	BOOL			quit;
	void*			ps;
	void*			keyboard;
	void*			mouse;
	void*			pad;
};

DebugMenuAPI gDebugMenuAPI;

static RsGlobalType*	RsGlobal;
static const void*		SubtitlesShadowFix_JumpBack;

void* (*GetModelInfo)(const char*, int*);

// This is actually CBaseModelInfo, but we currently don't have it defined
CVehicleModelInfo**& ms_modelInfoPtrs = *hook::get_pattern<CVehicleModelInfo**>("8B 15 ? ? ? ? 8D 04 24", 2);
int32_t& numModelInfos = *hook::get_pattern<int32_t>("81 FD ? ? ? ? 7C B7", 2);

float GetWidthMult()
{
	static float**	ResolutionWidthMult = hook::get_pattern<float*>("D8 0D ? ? ? ? D1 F8 52", 2);
	return **ResolutionWidthMult;
}

float GetHeightMult()
{
	static float**	ResolutionHeightMult = hook::get_pattern<float*>("D8 0D ? ? ? ? D1 F8 C7 44 24 ? 00 00 00 00 89 04 24", 2);
	return **ResolutionHeightMult;
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

static void (* const RsMouseSetPos)(RwV2d*) = AddressByVersion<void(*)(RwV2d*)>(0x6030C0, 0x6030A0, 0x602CE0);
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

void __stdcall Recalculate(float& fX, float& fY, signed int nShadow)
{
	fX = nShadow * GetWidthMult() * RsGlobal->MaximumWidth;
	fY = nShadow * GetHeightMult() * RsGlobal->MaximumHeight;
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

	template<uintptr_t pFltX>
	struct XMinus
	{
		static inline void (*orgPrintString)(float,float,const wchar_t*);
		static void PrintString(float fX, float fY, const wchar_t* pText)
		{
			PrintString_Internal(orgPrintString, fX, fY, -(**margin<pFltX>), 0.0f, pText);
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

// ============= Radar position and radardisc shadow =============
namespace RadardiscFixes
{
	static const float RADARDISC_SHRINK = 2.0f; // We are shrinking the radardisc by that

	static float orgRadarXPosVal;
	static float* orgRadarXPosPtr;

	template<std::size_t Index>
	static const float* orgRadarXPos_RadardiscShrink;

	template<std::size_t Index>
	static float RadarXPos_Recalculated_RadardiscShrink;

	template<std::size_t... I>
	static void RecalculateXPositions_RadardiscShrink(std::index_sequence<I...>)
	{
		const float multiplier = GetWidthMult() * RsGlobal->MaximumWidth;
		((RadarXPos_Recalculated_RadardiscShrink<I> = (*orgRadarXPos_RadardiscShrink<I> - RADARDISC_SHRINK) * multiplier), ...);
	}

	template<std::size_t Index>
	static const float* orgRadarYPos_RadardiscShrink;

	template<std::size_t Index>
	static float RadarYPos_Recalculated_RadardiscShrink;

	template<std::size_t... I>
	static void RecalculateYPositions_RadardiscShrink(std::index_sequence<I...>)
	{
		const float multiplier = GetHeightMult() * RsGlobal->MaximumHeight;
		((RadarYPos_Recalculated_RadardiscShrink<I> = (*orgRadarYPos_RadardiscShrink<I> - RADARDISC_SHRINK) * multiplier), ...);
	}

	static void (*orgDrawMap)();
	template<std::size_t NumXPosRadardisc, std::size_t NumYPosRadardisc>
	static void DrawMap_RecalculatePositions()
	{
		*orgRadarXPosPtr = orgRadarXPosVal * GetWidthMult() * RsGlobal->MaximumWidth;
		RecalculateXPositions_RadardiscShrink(std::make_index_sequence<NumXPosRadardisc>{});
		RecalculateYPositions_RadardiscShrink(std::make_index_sequence<NumYPosRadardisc>{});
		orgDrawMap();
	}

	HOOK_EACH_INIT(CalculateRadarXPos_RadardiscShrink, orgRadarXPos_RadardiscShrink, RadarXPos_Recalculated_RadardiscShrink);
	HOOK_EACH_INIT(CalculateRadarYPos_RadardiscShrink, orgRadarYPos_RadardiscShrink, RadarYPos_Recalculated_RadardiscShrink);

	static CRect ScaleWidthRect(CRect rect)
	{
		// Also account for a smaller radardisc
		rect.x1 = (rect.x1 + RADARDISC_SHRINK) * GetWidthMult() * RsGlobal->MaximumWidth;
		return rect;
	}

	template<std::size_t Index>
	static void (__fastcall* orgDrawSprite)(void* obj, void*, const CRect& rect, const CRGBA& col1, const CRGBA& col2, const CRGBA& col3, const CRGBA& col4);
	
	template<std::size_t Index>
	static void __fastcall DrawSprite_Scale(void* obj, void*, const CRect& rect, const CRGBA& col1, const CRGBA& col2, const CRGBA& col3, const CRGBA& col4)
	{
		orgDrawSprite<Index>(obj, nullptr, ScaleWidthRect(rect), col1, col2, col3, col4);
	}

	HOOK_EACH_INIT(DrawRadarDisc, orgDrawSprite, DrawSprite_Scale);
}

// ============= Fix the onscreen counter bar placement and shadow not scaling to resolution =============
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

// ============= Fix the radar trace blip outline not scaling to resolution =============
namespace RadarTraceOutlineFixes
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

	template<std::size_t NumXPos, std::size_t NumYPos>
	struct PositionRecalculator
	{
		static void RecalculatePositions()
		{
			RecalculateXPositions(std::make_index_sequence<NumXPos>{});
			RecalculateYPositions(std::make_index_sequence<NumYPos>{});
		}

		template<std::size_t Index>
		STATIC_INLINE void (*orgShowRadarTraceWithHeight)(float, float, unsigned int, unsigned char, unsigned char, unsigned char, unsigned char, unsigned char);

		template<std::size_t Index>
		static void ShowRadarTraceWithHeight_RecalculatePositions(float a1, float a2, unsigned int a3, unsigned char a4, unsigned char a5, unsigned char a6, unsigned char a7, unsigned char a8)
		{
			RecalculatePositions();
			orgShowRadarTraceWithHeight<Index>(a1, a2, a3, a4, a5, a6, a7, a8);
		}

		HOOK_EACH_INIT(ShowRadarTraceWithHeight, orgShowRadarTraceWithHeight, ShowRadarTraceWithHeight_RecalculatePositions);
	};

	HOOK_EACH_INIT(XPos, orgXPos, XPos_Recalculated);
	HOOK_EACH_INIT(YPos, orgYPos, YPos_Recalculated);
}


// ============= Fix the loading bar outline not scaling to resolution =============
namespace LoadingBarOutlineFixes
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

	static CRGBA* (__fastcall* orgRGBACtor)(CRGBA* obj, void*, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
	
	template<std::size_t NumXPos, std::size_t NumYPos>
	static CRGBA* __fastcall RGBACtor_RecalculatePositions(CRGBA* obj, void*, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
	{
		RecalculateXPositions(std::make_index_sequence<NumXPos>{});
		RecalculateYPositions(std::make_index_sequence<NumYPos>{});

		return orgRGBACtor(obj, nullptr, r, g, b, a);
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

		template<std::size_t Index>
		STATIC_INLINE void (*orgPrintString)(float,float,const wchar_t*);

		template<std::size_t Index>
		static void PrintString_Slide(float fX, float fY, const wchar_t* pText)
		{
			// We divide by a constant 640.0, because the X position is meant to slide across the entire screen
			orgPrintString<Index>(bSlidingEnabled ? (*pBigMessageX)[BigMessageIndex] * RsGlobal->MaximumWidth / 640.0f : fX, fY, pText);
		}

		template<std::size_t Index>
		STATIC_INLINE void (*orgSetRightJustifyWrap)(float wrap);

		template<std::size_t Index>
		static void SetRightJustifyWrap_Slide(float wrap)
		{
			orgSetRightJustifyWrap<Index>(bSlidingEnabled ? -500.0f * GetWidthMult() * RsGlobal->MaximumWidth : wrap);
		}

		HOOK_EACH_INIT(PrintString, orgPrintString, PrintString_Slide);
		HOOK_EACH_INIT(RightJustifyWrap, orgSetRightJustifyWrap, SetRightJustifyWrap_Slide);
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


// ============= Minimal HUD changes =============
namespace MinimalHUD
{
	// Wanted level stars
	static void (*orgSetDropShadowPosition)(short);
	static void (*orgSetDropColor)(const CRGBA&);

	static int8_t* pDropShadowSize;
	static int8_t* pDropShadowR;
	static int8_t* pDropShadowG;
	static int8_t* pDropShadowB;

	static void (*orgSetColor)(const CRGBA&);
	static void SetColor_SetShadow(const CRGBA& color)
	{
		orgSetDropShadowPosition(*pDropShadowSize);
		orgSetDropColor(CRGBA(*pDropShadowR, *pDropShadowG, *pDropShadowB, color.a));
		orgSetColor(color);
	}


	// Show the energy values when losing armor
	static uint8_t* pPlayerInFocus;
	static uint32_t* pTimeLastArmorLoss;

	static uint32_t LastTimeArmorLost;

	static float (*orgDrawFadeState)(int state, int force);
	static float DrawFadeState_CheckArmor(int state, int force)
	{
		// This is a bit hacky, but we don't necessarily need better right now
		if (pTimeLastArmorLoss[92 * *pPlayerInFocus] != LastTimeArmorLost)
		{
			force = 1;
			LastTimeArmorLost = pTimeLastArmorLoss[92 * *pPlayerInFocus];
		}
		return orgDrawFadeState(state, force);
	}


	// Fade the weapon icon - fix the render state and pass the alpha parameter
	static void (*orgRenderOneXLUSprite)(float, float, float, float, float, uint8_t, uint8_t, uint8_t, int16_t, float, uint8_t);
	static void RenderOneXLUSprite_FloatAlpha(float arg1, float arg2, float arg3, float arg4, float arg5, uint8_t red, uint8_t green, uint8_t blue, int16_t mult, float arg10, float alpha)
	{
		RwScopedRenderState<rwRENDERSTATEVERTEXALPHAENABLE> vertexAlpha;
		RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)TRUE);

		orgRenderOneXLUSprite(arg1, arg2, arg3, arg4, arg5, red, green, blue, mult, arg10, static_cast<uint8_t>(alpha));
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


// ============= Fix ammunation text (big message type 3) Y position offset not scaling to resolution =============
namespace BigMessage3ScalingFixes
{
	template<std::size_t Index>
	static const float* orgOffsetY;

	template<std::size_t Index>
	static float OffsetY_Recalculated;

	template<std::size_t... I>
	static void RecalculateYOffset(std::index_sequence<I...>)
	{
		const float multiplier = GetHeightMult() * RsGlobal->MaximumHeight;
		((OffsetY_Recalculated<I> = *orgOffsetY<I> * multiplier), ...);
	}

	static void (*orgSetDropColor)(const CRGBA&);

	template<std::size_t NumXOffsets>
	static void SetDropColor_Scale(const CRGBA& color)
	{
		RecalculateYOffset(std::make_index_sequence<NumXOffsets>{});
		orgSetDropColor(color);
	}

	HOOK_EACH_INIT(MessageYOffset, orgOffsetY, OffsetY_Recalculated);
}


// ============= Fix an incorrect vertex setup for the outline of a destination blip in the Map Legend =============
namespace LegendBlipFix
{
	static void (*orgDraw2DPolygon)(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, const CRGBA& color);
	static void Draw2DPolygon_FixVertices(float x1, float y1, float x2, float y2, float /*x3*/, float y3, float x4, float y4, const CRGBA& color)
	{
		// In this setup, x3 is incorrect - it should have been (X + scaleX(14.0f)) but is (X + scaleX(2.0f)), same as x4
		// We can recover the correct dimensions from x1 (bottom center) and x4 (top left):
		// x3 = x1 + (x1 - x4)
		// Write it out in full like this (without simplifying), so we know (x1 - x4) is done at a correct floating point precision.
		const float x3 = x1 + (x1 - x4);
		orgDraw2DPolygon(x1, y1, x2, y2, x3, y3, x4, y4, color);
	}
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
		mov		[esp], eax
		fild	dword ptr [esp]
		push	eax
		lea		eax, [esp+20h-18h]
		push	eax
		lea		eax, [esp+24h-14h]
		push	eax
		call	Recalculate
		jmp		SubtitlesShadowFix_JumpBack
	}
#else
	__asm__ volatile
	(
		"mov	[esp], eax\n"
		"fild	dword ptr [esp]\n"
		"push	eax\n"
		"lea	eax, [esp+0x20-0x18]\n"
		"push	eax\n"
		"lea	eax, [esp+0x24-0x14]\n"
		"push	eax\n"
		"call	%[Recalculate]\n"
		"jmp	%[SubtitlesShadowFix_JumpBack]"
		:: [Recalculate] "i" (Recalculate),
		[SubtitlesShadowFix_JumpBack] "m" (SubtitlesShadowFix_JumpBack)
	);
#endif
}

// C-style wrapper for calling a C++ function in GCC-style inline assembly (the function signature is really important here)
RwFrame* __thiscall CVehicleModelInfo_GetExtrasFrame(CVehicleModelInfo* modelInfo, RpClump* clump)
{
	return modelInfo->GetExtrasFrame(clump);
}

__declspec(naked) void CreateInstance_BikeFix()
{
#ifdef _MSC_VER
	_asm
	{
		push	eax
		mov		ecx, ebp
		call	CVehicleModelInfo::GetExtrasFrame
		ret
	}
#else
	__asm__ volatile
	(
		"push	eax\n"
		"mov	ecx, ebp\n"
		"call	%[CVehicleModelInfo_GetExtrasFrame]\n"
		"ret"
		:: [CVehicleModelInfo_GetExtrasFrame] "i" (CVehicleModelInfo_GetExtrasFrame)
	);
#endif
}

char** ppUserFilesDir = AddressByVersion<char**>(0x6022AA, 0x60228A, 0x601ECA);

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

unsigned int __cdecl AutoPilotTimerCalculation_VC(unsigned int nTimer, int nScaleFactor, float fScaleCoef)
{
	return nTimer - static_cast<unsigned int>(nScaleFactor * fScaleCoef);
}

__declspec(naked) void AutoPilotTimerFix_VC()
{
#ifdef _MSC_VER
	_asm
	{
		push	dword ptr [esp + 0xC]
		push	dword ptr [ebx + 0x10]
		push	eax
		call	AutoPilotTimerCalculation_VC
		add 	esp, 0xC
		mov 	[ebx + 0xC], eax
		add     esp, 0x30
		pop     ebp
		pop     ebx
		ret     4
	}
#else
	__asm__ volatile
	(
		"push	dword ptr [esp + 0xC]\n"
		"push	dword ptr [ebx + 0x10]\n"
		"push	eax\n"
		"call	%[AutoPilotTimerCalculation_VC]\n"
		"add	esp, 0xC\n"
		"mov	[ebx + 0xC], eax\n"
		"add	esp, 0x30\n"
		"pop	ebp\n"
		"pop	ebx\n"
		"ret	4"
		:: [AutoPilotTimerCalculation_VC] "i" (AutoPilotTimerCalculation_VC)
	);
#endif
}


namespace ZeroAmmoFix
{

template<std::size_t Index>
static void (__fastcall *orgGiveWeapon)(void* ped, void*, unsigned int weapon, unsigned int ammo, bool flag);

template<std::size_t Index>
static void __fastcall GiveWeapon_SP(void* ped, void*, unsigned int weapon, unsigned int ammo, bool flag)
{
	orgGiveWeapon<Index>(ped, nullptr, weapon, std::max(1u, ammo), flag);
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
		PrintCreditSpace( 1.5f, pos );

		{
			wchar_t spText[] = { 'A'_xv, 'N'_xv, 'D'_xv, '\0'_xv };

			for ( auto& ch : spText ) ch = xvChar(ch);
			PrintCreditText( 1.1f, 0.8f, spText, pos, timeOffset );
		}

		PrintCreditSpace( 1.5f, pos );

		{
			wchar_t spText[] = { 'A'_xv, 'D'_xv, 'R'_xv, 'I'_xv, 'A'_xv, 'N'_xv, ' '_xv, '\''_xv, 'S'_xv, 'I'_xv, 'L'_xv, 'E'_xv, 'N'_xv, 'T'_xv, '\''_xv, ' '_xv,
				'Z'_xv, 'D'_xv, 'A'_xv, 'N'_xv, 'O'_xv, 'W'_xv, 'I'_xv, 'C'_xv, 'Z'_xv, '\0'_xv };

			for ( auto& ch : spText ) ch = xvChar(ch);
			PrintCreditText( 1.1f, 1.1f, spText, pos, timeOffset );
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


// ============= Corrected FBI Washington sirens sound =============
namespace SirenSwitchingFix
{
	__declspec(naked) static void IsFBIRanchOrFBICar()
	{
#ifdef _MSC_VER
		_asm
		{
			mov     dword ptr [esi+0x1C], 0x1C

			// al = 0 - high pitched siren
			// al = 1 - normal siren
			cmp     dword ptr [ebp+0x14], 90	// fbiranch
			je      IsFBIRanchOrFBICar_HighPitchSiren
			cmp     dword ptr [ebp+0x14], 17	// fbicar
			setne   al
			ret

		IsFBIRanchOrFBICar_HighPitchSiren:
			xor     al, al
			ret
		}
#else
		__asm__ volatile
		(
			"mov    dword ptr [esi+0x1C], 0x1C\n"

			// al = 0 - high pitched siren
			// al = 1 - normal siren
			"cmp    dword ptr [ebp+0x14], 90\n"	// fbiranch
			"je     IsFBIRanchOrFBICar_HighPitchSiren\n"
			"cmp    dword ptr [ebp+0x14], 17\n"	// fbicar
			"setne  al\n"
			"ret\n"

		"IsFBIRanchOrFBICar_HighPitchSiren:\n"
			"xor    al, al\n"
			"ret"
		);
#endif
	}
}


// ============= Corrected siren corona placement for FBI cars and Vice Cheetah =============
namespace FBISirenCoronaFix
{
	bool overridePosition;
	CVector vecOverridePosition;

	// True - don't display siren
	// False - display siren
	bool SetUpFBISiren( const CVehicle* vehicle )
	{
		SVF::Feature foundFeature = SVF::Feature::NO_FEATURE;
		SVF::ForAllModelFeatures( vehicle->GetModelIndex(), [&]( SVF::Feature f ) {
			if ( f >= SVF::Feature::FBI_RANCHER_SIREN && f <= SVF::Feature::VICE_CHEETAH_SIREN )
			{
				foundFeature = f;
				return false;
			}
			return true;
		} );

		if ( foundFeature != SVF::Feature::NO_FEATURE )
		{
			if ( foundFeature != SVF::Feature::VICE_CHEETAH_SIREN )
			{
				constexpr CVector FBICAR_SIREN_POS(0.4f, 0.8f, 0.25f);
				constexpr CVector FBIRANCH_SIREN_POS(0.5f, 1.12f, 0.5f);

				overridePosition = true;
				vecOverridePosition = foundFeature == SVF::Feature::FBI_WASHINGTON_SIREN ? FBICAR_SIREN_POS : FBIRANCH_SIREN_POS;
			}
			else
			{
				overridePosition = false;
			}

			return false;
		}

		return true;
	}

	CVector& __fastcall SetUpVector( CVector& out, void*, float X, float Y, float Z )
	{
		if ( overridePosition )
		{
			out = vecOverridePosition;
		}
		else
		{
			out = CVector(X, Y, Z);
		}

		return out;
	}
}


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
			mov		cl, [ebx+0x50]
			mov		al, cl
			and		cl, 0xF8
			cmp		cl, 0x28
			je		DontSetStatus
			and     al, 7
			or      al, 0x20

		DontSetStatus:
			ret
		}
#else
		__asm__ volatile
		(
			"mov	cl, [ebx+0x50]\n"
			"mov	al, cl\n"
			"and	cl, 0xF8\n"
			"cmp	cl, 0x28\n"
			"je		DontSetStatus\n"
			"and    al, 7\n"
			"or     al, 0x20\n"

		"DontSetStatus:\n"
			"ret"
		);
#endif
	}
}


// ============= Apply the environment mapping on extra components =============
namespace EnvMapsOnExtras
{
	static RpMaterial* (*RpMatFXMaterialSetEnvMapCoefficient)(RpMaterial* material, RwReal coef);
	static int (*RpMatFXMaterialGetEffects)(const RpMaterial* material);

	static void RemoveSpecularityFromAtomic(RpAtomic* atomic)
	{
		RpGeometry* geometry = RpAtomicGetGeometry(atomic);
		if (geometry != nullptr)
		{
			RpGeometryForAllMaterials(geometry, [](RpMaterial* material)
				{
					bool bRemoveSpecularity = false;

					// Only remove specularity from the body materials, keep glass intact.
					// This is only done on a best-effort basis, as mods can fine-tune it better
					// and just remove the model from the exceptions list
					RwTexture* texture = RpMaterialGetTexture(material);
					if (texture != nullptr)
					{
						if (strstr(RwTextureGetName(texture), "glass") == nullptr && strstr(RwTextureGetMaskName(texture), "glass") == nullptr)
						{
							bRemoveSpecularity = true;
						}
					}

					if (bRemoveSpecularity)
					{
						if (RpMatFXMaterialGetEffects(material) == 2) // rpMATFXEFFECTENVMAP
						{
							RpMatFXMaterialSetEnvMapCoefficient(material, 0.0f);
						}
						RpMaterialGetSurfaceProperties(material)->specular = 0.0f;
					}
					return material;
				});
		}
	}

	static RpClump* (*orgRpClumpForAllAtomics)(RpClump* clump, RpAtomicCallBack callback, void* data);
	static RpClump* RpClumpForAllAtomics_ExtraComps(CVehicleModelInfo* modelInfo, RpAtomicCallBack callback, void* data)
	{
		RpClump* result = orgRpClumpForAllAtomics(modelInfo->m_clump, callback, data);

		const int32_t modelID = std::distance(ms_modelInfoPtrs, std::find(ms_modelInfoPtrs, ms_modelInfoPtrs+numModelInfos, modelInfo));
		const bool bRemoveSpecularity = ExtraCompSpecularity::SpecularityExcluded(modelID);
		for (int32_t i = 0; i < modelInfo->m_numComps; i++)
		{
			if (bRemoveSpecularity)
			{
				RemoveSpecularityFromAtomic(modelInfo->m_comps[i]);
			}

			callback(modelInfo->m_comps[i], data);
			CVehicleModelInfo::AttachCarPipeToRwObject(reinterpret_cast<RwObject*>(modelInfo->m_comps[i]));
		}
		return result;
	}
}


// ============= Null terminate read lines in CPlane::LoadPath =============
namespace NullTerminatedLines
{
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
}


// ============= Don't reset mouse sensitivity on New Game =============
namespace MouseSensNewGame
{
	static float DefaultHorizontalAccel;
	static float* fMouseAccelHorzntl;

	static void (*orgSetDirMyDocuments)();
	static void SetDirMyDocuments_ResetMouse()
	{
		orgSetDirMyDocuments();
		*fMouseAccelHorzntl = DefaultHorizontalAccel;
	}
}


// ============= Fixed pickup effects colors =============
namespace PickupEffectsFixes
{
	__declspec(naked) static void PickUpEffects_BigDollarColor()
	{
#ifdef _MSC_VER
		_asm
		{
			mov		byte ptr [esp+0x184-0x170], 0
			mov		dword ptr [esp+0x184-0x174], 37
			ret
		}
#else
		__asm__ volatile
		(
			"mov	byte ptr [esp+0x184-0x170], 0\n"
			"mov	dword ptr [esp+0x184-0x174], 37\n"
			"ret"
		);
#endif
	}

	__declspec(naked) static void PickUpEffects_Minigun2Glow()
	{
#ifdef _MSC_VER
		_asm
		{
			cmp		ecx, 294	// minigun2
			jnz		NotMinigun2
			mov		byte ptr [esp+0x184-0x170], 0
			xor		eax, eax
			jmp		Return

		NotMinigun2:
			lea     eax, [ecx+1]

		Return:
			mov     ebx, ecx
			ret
		}
#else
		__asm__ volatile
		(
			"cmp	ecx, 294\n"	// minigun2
			"jnz	NotMinigun2\n"
			"mov	byte ptr [esp+0x184-0x170], 0\n"
			"xor	eax, eax\n"
			"jmp	Return\n"

		"NotMinigun2:\n"
			"lea    eax, [ecx+1]\n"

		"Return:\n"
			"mov    ebx, ecx\n"
			"ret"
		);
#endif
	}
}


// ============= Fixed IS_PLAYER_TARGETTING_CHAR incorrectly detecting targetting in Classic controls =============
// ============= when the player is not aiming =============
// ============= By Wesser =============
namespace IsPlayerTargettingCharFix
{
	static bool* bUseMouse3rdPerson;
	static void* TheCamera;
	static bool (__fastcall* Using1stPersonWeaponMode)();

	__declspec(naked) static void IsPlayerTargettingChar_ExtraChecks()
	{
		// After this extra block of code, there is a jz Return, so set ZF to 0 here if this path is to be entered
#ifdef _MSC_VER
		_asm
		{
			test	bl, bl
			jnz		ReturnToUpdateCompareFlag
			mov		eax, bUseMouse3rdPerson
			cmp		byte ptr [eax], 0
			jne		CmpAndReturn
			mov		ecx, TheCamera
			call	Using1stPersonWeaponMode
			test	al, al
			jz		ReturnToUpdateCompareFlag

		CmpAndReturn:
			cmp     byte ptr [esp+0x11C-0x10C], 0
			ret

		ReturnToUpdateCompareFlag:
			xor		al, al
			ret
		}
#else
		__asm__ volatile
		(
			"test	bl, bl\n"
			"jnz	ReturnToUpdateCompareFlag\n"
			"mov	eax, %[bUseMouse3rdPerson]\n"
			"cmp	byte ptr [eax], 0\n"
			"jne	CmpAndReturn\n"
			"mov	ecx, %[TheCamera]\n"
			"call	%[Using1stPersonWeaponMode]\n"
			"test	al, al\n"
			"jz		ReturnToUpdateCompareFlag\n"

		"CmpAndReturn:\n"
			"cmp    byte ptr [esp+0x11C-0x10C], 0\n"
			"ret\n"

		"ReturnToUpdateCompareFlag:\n"
			"xor	al, al\n"
			"ret"
			:: [bUseMouse3rdPerson] "m" (bUseMouse3rdPerson),
			[TheCamera] "m" (TheCamera),
			[Using1stPersonWeaponMode] "m" (Using1stPersonWeaponMode)
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
}


// ============= Disabled backface culling on detached car parts, peds and specific models =============
namespace SelectableBackfaceCulling
{
	void ReadDrawBackfacesExclusions(const wchar_t* pPath)
	{
		constexpr size_t SCRATCH_PAD_SIZE = 32767;
		WideDelimStringReader reader(SCRATCH_PAD_SIZE);

		GetPrivateProfileSectionW(L"DrawBackfaces", reader.GetBuffer(), reader.GetSize(), pPath);
		while (const wchar_t* str = reader.GetString())
		{
			auto modelID = ParseUtils::TryParseInt(str);
			if (modelID)
				SVF::RegisterFeature(*modelID, SVF::Feature::DRAW_BACKFACES);
			else
				SVF::RegisterFeature(ParseUtils::ParseString(str), SVF::Feature::DRAW_BACKFACES);
		}
	}

	// Only the parts of CObject we need
	struct Object
	{
		std::byte		__pad[364];
		uint8_t			m_objectCreatedBy;
		bool			bObjectFlag0 : 1;
		bool			bObjectFlag1 : 1;
		bool			bObjectFlag2 : 1;
		bool			bObjectFlag3 : 1;
		bool			bObjectFlag4 : 1;
		bool			bObjectFlag5 : 1;
		bool			m_bIsExploded : 1;
		bool			bUseVehicleColours : 1;
		std::byte		__pad2[22];
		FLAUtils::int16	m_wCarPartModelIndex;
	};

	static void* EntityRender_Prologue_JumpBack;
	__declspec(naked) static void __fastcall EntityRender_Original(CEntity*)
	{
#ifdef _MSC_VER
		_asm
		{
			push    ebx
			mov     ebx, ecx
			cmp     dword ptr [ebx+0x4C], 0
			jmp		EntityRender_Prologue_JumpBack
		}
#else
		__asm__ volatile
		(
			"push   ebx\n"
			"mov    ebx, ecx\n"
			"cmp    dword ptr [ebx+0x4C], 0\n"
			"jmp	%[EntityRender_Prologue_JumpBack]"
			:: [EntityRender_Prologue_JumpBack] "m" (EntityRender_Prologue_JumpBack)
		);
#endif
	}

	static bool ShouldDisableBackfaceCulling(const CEntity* entity)
	{
		const uint8_t entityType = entity->m_nType;

		// Vehicles disable BFC elsewhere already
		if (entityType == 2)
		{
			return false;
		}

		// Always disable BFC on peds
		if (entityType == 3)
		{
			return true;
		}

		// For objects, do extra checks
		if (entityType == 4)
		{
			const Object* object = reinterpret_cast<const Object*>(entity);
			if (object->m_wCarPartModelIndex.Get() != -1 && object->m_objectCreatedBy == TEMP_OBJECT && object->bUseVehicleColours)
			{
				return true;
			}
		}

		// For everything else, check the exclusion list
		return SVF::ModelHasFeature(entity->m_modelIndex.Get(), SVF::Feature::DRAW_BACKFACES);
	}

	// If CEntity::Render is re-routed by another mod, we overwrite this later
	static void (__fastcall *orgEntityRender)(CEntity* obj) = &EntityRender_Original;

	static void __fastcall EntityRender_BackfaceCulling(CEntity* obj)
	{
		RwScopedRenderState<rwRENDERSTATECULLMODE> cullState;

		if (ShouldDisableBackfaceCulling(obj))
		{
			RwRenderStateSet(rwRENDERSTATECULLMODE, reinterpret_cast<void*>(rwCULLMODECULLNONE));
		}

		orgEntityRender(obj);
	}
}


// ============= Fix the construction site LOD losing its HQ model and showing at all times =============
namespace ConstructionSiteLODFix
{
	static bool bActivateConstructionSiteFix = false;

	static int32_t MI_BLDNGST2MESH, MI_BLDNGST2MESHDAM;
	static CSimpleModelInfo* Bldngst2mesh_ModelInfo;
	static CSimpleModelInfo* Bldngst2meshDam_ModelInfo;
	static CSimpleModelInfo* LODngst2mesh_ModelInfo;
	void MatchModelIndices()
	{
		CSimpleModelInfo* Bldngst2mesh = reinterpret_cast<CSimpleModelInfo*>(GetModelInfo("bldngst2mesh", &MI_BLDNGST2MESH));
		CSimpleModelInfo* Bldngst2meshDam = reinterpret_cast<CSimpleModelInfo*>(GetModelInfo("bldngst2meshdam", &MI_BLDNGST2MESHDAM));
		CSimpleModelInfo* LODngst2mesh = reinterpret_cast<CSimpleModelInfo*>(GetModelInfo("LODngst2mesh", nullptr));
		CSimpleModelInfo* LODngst2meshDam = reinterpret_cast<CSimpleModelInfo*>(GetModelInfo("LODngst2meshdam", nullptr));

		const bool bHasBldngst2mesh = Bldngst2mesh != nullptr;
		const bool bHasBldngst2meshDam = Bldngst2meshDam != nullptr;
		const bool bHasLODngst2mesh = LODngst2mesh != nullptr;
		const bool bHasLODngst2meshDam = LODngst2meshDam != nullptr;

		// LODngst2meshdam doesn't exist in the vanilla game, so if it exists - a mod to fix this issue via
		// the map modifications has been installed.
		bActivateConstructionSiteFix = bHasBldngst2mesh && bHasBldngst2meshDam && bHasLODngst2mesh && !bHasLODngst2meshDam;

		Bldngst2mesh_ModelInfo = Bldngst2mesh;
		Bldngst2meshDam_ModelInfo = Bldngst2meshDam;
		LODngst2mesh_ModelInfo = LODngst2mesh;
	}

	static void FixConstructionSiteModel(int oldModelID, int newModelID)
	{
		if (!bActivateConstructionSiteFix)
		{
			return;
		}

		if (oldModelID == MI_BLDNGST2MESH && newModelID == MI_BLDNGST2MESHDAM)
		{
			LODngst2mesh_ModelInfo->m_atomics[2] = Bldngst2meshDam_ModelInfo;
		}
		else if (oldModelID == MI_BLDNGST2MESHDAM && newModelID == MI_BLDNGST2MESH)
		{
			LODngst2mesh_ModelInfo->m_atomics[2] = Bldngst2mesh_ModelInfo;
		}
	}

	template<std::size_t Index>
	static void (__fastcall *orgReplaceWithNewModel)(CEntity* building, void*, int newModelID);

	template<std::size_t Index>
	static void __fastcall ReplaceWithNewModel_ConstructionSiteFix(CEntity* building, void*, int newModelID)
	{
		const int oldModelID = building->m_modelIndex.Get();
		orgReplaceWithNewModel<Index>(building, nullptr, newModelID);
		FixConstructionSiteModel(oldModelID, newModelID);
	}

	HOOK_EACH_INIT(ReplaceWithNewModel, orgReplaceWithNewModel, ReplaceWithNewModel_ConstructionSiteFix);
}


namespace ModelIndicesReadyHook
{
	static void (*orgInitialiseObjectData)(const char*);
	static void InitialiseObjectData_ReadySVF(const char* path)
	{
		orgInitialiseObjectData(path);
		SVF::MarkModelNamesReady();
		ConstructionSiteLODFix::MatchModelIndices();

		// This is a bit dirty, but whatever
		// Tooled Up in North Point Mall needs a "draw last" flag, or else our BFC changes break it very badly
		// AmmuNation and other stores already have that flag, this one does not
		void* model = GetModelInfo("mall_hardware", nullptr);
		if (model != nullptr)
		{
			uint16_t* flags = reinterpret_cast<uint16_t*>(static_cast<char*>(model) + 0x42);
			*flags |= 0x40;
		}
	}
}


// ============= Fix the outro splash flickering for a frame when fading in =============
namespace OutroSplashFix
{
	struct RGBA
	{
		uint8_t r, g, b, a;
	};

	static RGBA* (__thiscall *orgRGBASet)(RGBA*, uint8_t, uint8_t, uint8_t, uint8_t);
	static RGBA* __fastcall RGBASet_Clamp(RGBA* rgba, void*, int r, int g, int b, int a)
	{	
		return orgRGBASet(rgba, static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), static_cast<uint8_t>(std::clamp(a, 0, 255)));
	}
}


// ============= Fix Tommy not shaking his fists with brass knuckles (in all cases) and most post-GTA III weapons (when cars slow down for him) =============
namespace TommyFistShakeWithWeapons
{
	struct WeaponInfo
	{
		std::byte	__pad[0x60];
		uint32_t	m_weaponSlot;
	};
	static WeaponInfo* (*GetWeaponInfo)(uint32_t weaponID);

	constexpr uint32_t WEAPON_CHAINSAW = 11;
	static WeaponInfo DUMMY_INFO = [] {
		WeaponInfo dummy;
		dummy.m_weaponSlot = 99;
		return dummy;
	}();

	static bool WeaponProhibitsFistShake(uint32_t weaponID)
	{
		const uint32_t weaponSlot = GetWeaponInfo(weaponID)->m_weaponSlot;
		const bool bWeaponAllowsFistShake = (weaponSlot == 0 || weaponSlot == 1 || weaponSlot == 3 || weaponSlot == 5) && weaponID != WEAPON_CHAINSAW;
		return !bWeaponAllowsFistShake;
	}

	__declspec(naked) static void CheckWeaponGroupHook()
	{
#ifdef _MSC_VER
		_asm
		{
			push	dword ptr [eax]
			call	WeaponProhibitsFistShake
			add		esp, 4
			test	al, al
			ret
		}
#else
		__asm__ volatile
		(
			"push	dword ptr [eax]\n"
			"call	%[WeaponProhibitsFistShake]\n"
			"add	esp, 4\n"
			"test	al, al\n"
			"ret"
			:: [WeaponProhibitsFistShake] "i" (WeaponProhibitsFistShake)
		);
#endif
	}

	template<std::size_t Index>
	static WeaponInfo* (*orgGetWeaponInfo)(uint32_t weaponID);

	template<std::size_t Index>
	static WeaponInfo* gGetWeaponInfo_ExcludeChainsaw(uint32_t weaponID)
	{
		if (weaponID == WEAPON_CHAINSAW)
		{
			return &DUMMY_INFO;
		}
		return orgGetWeaponInfo<Index>(weaponID);
	}

	HOOK_EACH_INIT(ExcludeChainsaw, orgGetWeaponInfo, gGetWeaponInfo_ExcludeChainsaw);
}


void InjectDelayedPatches_VC_Common( bool bHasDebugMenu, const wchar_t* wcModulePath )
{
	using namespace Memory;
	using namespace hook::txn;

	const ModuleList moduleList;

	const HMODULE hGameModule = GetModuleHandle(nullptr);

	const HMODULE skygfxModule = moduleList.Get(L"skygfx");
	if (skygfxModule != nullptr)
	{
		auto attachCarPipe = reinterpret_cast<void(*)(RwObject*)>(GetProcAddress(skygfxModule, "AttachCarPipeToRwObject"));
		if (attachCarPipe != nullptr)
		{
			CVehicleModelInfo::AttachCarPipeToRwObject = attachCarPipe;
		}
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

	
	// Corrected siren corona placement for emergency vehicles
	if ( GetPrivateProfileIntW(L"SilentPatch", L"EnableVehicleCoronaFixes", -1, wcModulePath) == 1 )
	{
		// Other mods might be touching it, so only patch specific vehicles if their code has not been touched at all
		try
		{
			auto firetruck1 = pattern("8D 8C 24 24 09 00 00 FF 35 ? ? ? ? FF 35 ? ? ? ? FF 35").get_one();
			auto firetruck2 = pattern("8D 8C 24 30 09 00 00 FF 35 ? ? ? ? FF 35 ? ? ? ? FF 35").get_one();

			static const CVector FIRETRUCK_SIREN_POS(0.95f, 3.2f, 1.4f);
			static const float FIRETRUCK_SIREN_MINUS_X = -FIRETRUCK_SIREN_POS.x;

			Patch( firetruck1.get<float*>( 7 + 2 ), &FIRETRUCK_SIREN_POS.z );
			Patch( firetruck1.get<float*>( 7 + 2 + (6*1) ), &FIRETRUCK_SIREN_POS.y );
			Patch( firetruck1.get<float*>( 7 + 2 + (6*2) ), &FIRETRUCK_SIREN_POS.x );

			Patch( firetruck2.get<float*>( 7 + 2 ), &FIRETRUCK_SIREN_POS.z );
			Patch( firetruck2.get<float*>( 7 + 2 + (6*1) ), &FIRETRUCK_SIREN_POS.y );
			Patch( firetruck2.get<float*>( 7 + 2 + (6*2) ), &FIRETRUCK_SIREN_MINUS_X );
		}
		TXN_CATCH();

		try
		{
			auto ambulan1 = pattern("8D 8C 24 0C 09 00 00 FF 35 ? ? ? ? FF 35 ? ? ? ? FF 35").get_one();
			auto ambulan2 = pattern("8D 8C 24 18 09 00 00 FF 35 ? ? ? ? FF 35 ? ? ? ? FF 35").get_one();

			static const CVector AMBULANCE_SIREN_POS(0.7f, 0.65f, 1.55f);
			static const float AMBULANCE_SIREN_MINUS_X = -AMBULANCE_SIREN_POS.x;

			Patch( ambulan1.get<float*>( 7 + 2 ), &AMBULANCE_SIREN_POS.z );
			Patch( ambulan1.get<float*>( 7 + 2 + (6*1) ), &AMBULANCE_SIREN_POS.y );
			Patch( ambulan1.get<float*>( 7 + 2 + (6*2) ), &AMBULANCE_SIREN_POS.x );

			Patch( ambulan2.get<float*>( 7 + 2 ), &AMBULANCE_SIREN_POS.z );
			Patch( ambulan2.get<float*>( 7 + 2 + (6*1) ), &AMBULANCE_SIREN_POS.y );
			Patch( ambulan2.get<float*>( 7 + 2 + (6*2) ), &AMBULANCE_SIREN_MINUS_X );
		}
		TXN_CATCH();

		try
		{
			auto police1 = pattern("8D 8C 24 DC 08 00 00 FF 35 ? ? ? ? FF 35 ? ? ? ? FF 35").get_one();
			auto police2 = pattern("8D 8C 24 E8 08 00 00 FF 35 ? ? ? ? FF 35 ? ? ? ? FF 35").get_one();

			static const CVector POLICE_SIREN_POS(0.55f, -0.4f, 0.95f);
			static const float POLICE_SIREN_MINUS_X = -POLICE_SIREN_POS.x;

			Patch( police1.get<float*>( 7 + 2 ), &POLICE_SIREN_POS.z );
			Patch( police1.get<float*>( 7 + 2 + (6*1) ), &POLICE_SIREN_POS.y );
			Patch( police1.get<float*>( 7 + 2 + (6*2) ), &POLICE_SIREN_POS.x );

			Patch( police2.get<float*>( 7 + 2 ), &POLICE_SIREN_POS.z );
			Patch( police2.get<float*>( 7 + 2 + (6*1) ), &POLICE_SIREN_POS.y );
			Patch( police2.get<float*>( 7 + 2 + (6*2) ), &POLICE_SIREN_MINUS_X );
		}
		TXN_CATCH();

		try
		{
			auto enforcer1 = pattern("8D 8C 24 F4 08 00 00 FF 35 ? ? ? ? FF 35 ? ? ? ? FF 35").get_one();
			auto enforcer2 = pattern("8D 8C 24 00 09 00 00 FF 35 ? ? ? ? FF 35 ? ? ? ? FF 35").get_one();

			static const CVector ENFORCER_SIREN_POS(0.6f, 1.05f, 1.4f);
			static const float ENFORCER_SIREN_MINUS_X = -ENFORCER_SIREN_POS.x;

			Patch( enforcer1.get<float*>( 7 + 2 ), &ENFORCER_SIREN_POS.z );
			Patch( enforcer1.get<float*>( 7 + 2 + (6*1) ), &ENFORCER_SIREN_POS.y );
			Patch( enforcer1.get<float*>( 7 + 2 + (6*2) ), &ENFORCER_SIREN_POS.x );

			Patch( enforcer2.get<float*>( 7 + 2 ), &ENFORCER_SIREN_POS.z );
			Patch( enforcer2.get<float*>( 7 + 2 + (6*1) ), &ENFORCER_SIREN_POS.y );
			Patch( enforcer2.get<float*>( 7 + 2 + (6*2) ), &ENFORCER_SIREN_MINUS_X );	
		}
		TXN_CATCH();

		{
			try
			{
				auto chopper1 = pattern("C7 44 24 44 00 00 E0 40 50 C7 44 24 4C 00 00 00 00").get_one();	// Front light

				constexpr CVector CHOPPER_SEARCH_LIGHT_POS(0.0f, 3.0f, -1.0f);	// Same as in III Aircraft (not implemented there yet!)

				Patch( chopper1.get<float>( 4 ), CHOPPER_SEARCH_LIGHT_POS.y );
				Patch( chopper1.get<float>( 9 + 4 ), CHOPPER_SEARCH_LIGHT_POS.z );
			}
			TXN_CATCH();

			try
			{
				auto chopper2 = pattern("C7 44 24 6C 00 00 10 C1 8D 44 24 5C C7 44 24 70 00 00 00 00").get_one();	// Tail light

				constexpr CVector CHOPPER_RED_LIGHT_POS(0.0f, -7.5f, 2.5f);	// Same as in III Aircraft

				Patch( chopper2.get<float>( 4 ), CHOPPER_RED_LIGHT_POS.y );
				Patch( chopper2.get<float>( 12 + 4 ), CHOPPER_RED_LIGHT_POS.z );
			}
			TXN_CATCH();
		}

		try
		{
			using namespace FBISirenCoronaFix;

			auto viceCheetah = pattern("8D 8C 24 CC 09 00 00 FF 35 ? ? ? ? FF 35 ? ? ? ? FF 35 ? ? ? ? E8").get_one(); // Siren pos

			try
			{
				auto hasFBISiren = pattern("83 E9 04 0F 84 87 0A 00 00 83 E9 10").get_one(); // Predicate for showing FBI/Vice Squad siren

				Patch<uint8_t>( hasFBISiren.get<void>(), 0x55 ); // push ebp
				InjectHook( hasFBISiren.get<void>( 1 ), SetUpFBISiren, HookType::Call );
				Patch( hasFBISiren.get<void>( 1 + 5 ), { 0x83, 0xC4, 0x04, 0x84, 0xC0, 0x90 } ); // add esp, 4 / test al, al / nop

				InjectHook( viceCheetah.get<void>( 0x19 ), SetUpVector );
			}
			TXN_CATCH();

			static const float VICE_CHEETAH_SIREN_POS_Z = 0.25f;
			Patch( viceCheetah.get<float*>( 7 + 2 ), &VICE_CHEETAH_SIREN_POS_Z );
		}
		TXN_CATCH();
	}


	bool HasModelInfo = false;
	// Register CBaseModelInfo::GetModelInfo for SVF so we can resolve model names
	try
	{
		using namespace ModelIndicesReadyHook;

		auto initialiseObjectData = get_pattern("E8 ? ? ? ? 59 E8 ? ? ? ? E8 ? ? ? ? 31 DB");
		auto getModelInfo = (void*(*)(const char*, int*))get_pattern("57 31 FF 55 8B 6C 24 14", -6);

		GetModelInfo = getModelInfo;
		InterceptCall(initialiseObjectData, orgInitialiseObjectData, InitialiseObjectData_ReadySVF);
		SVF::RegisterGetModelInfoCB(getModelInfo);

		HasModelInfo = true;
	}
	TXN_CATCH();


	// Fix the construction site LOD losing its HQ model and showing at all times
	if (HasModelInfo) try
	{
		using namespace ConstructionSiteLODFix;

		std::array<void*, 3> replaceWithNewModel = {
			get_pattern("E8 ? ? ? ? C7 85 ? ? ? ? 00 00 00 00 83 8D ? ? ? ? FF"),
			get_pattern("DD D8 E8 ? ? ? ? 56", 2),
			get_pattern("E8 ? ? ? ? FF 44 24 0C 83 C5 0C"),
		};
		
		HookEach_ReplaceWithNewModel(replaceWithNewModel, InterceptCall);
	}
	TXN_CATCH();


	// Fix the radar disc shadow scaling and radar X position
	try
	{
		using namespace RadardiscFixes;

		auto draw_radar_disc1 = pattern("D8 25 ? ? ? ? DD DB D9 C2 D9 9C 24 ? ? ? ? DB 05 ? ? ? ? D8 0D ? ? ? ? D8 0D ? ? ? ? D8 05 ? ? ? ? D8 05").count(2);
		auto draw_radar_disc2 = pattern("D8 C1 D8 05 ? ? ? ? D9 9C 24 ? ? ? ? DE D9 DD D8").count(2);

		std::array<float**, 4> radarXPos_RadardiscShrink = {
			draw_radar_disc1.get(0).get<float*>(35 + 2),
			draw_radar_disc1.get(0).get<float*>(35 + 6 + 2),
			draw_radar_disc1.get(1).get<float*>(35 + 2),
			draw_radar_disc1.get(1).get<float*>(35 + 6 + 2),
		};

		std::array<float**, 4> radarYPos_RadardiscShrink = {
			draw_radar_disc1.get(0).get<float*>(2),
			draw_radar_disc1.get(1).get<float*>(2),
			draw_radar_disc2.get(0).get<float*>(2 + 2),
			draw_radar_disc2.get(1).get<float*>(2 + 2),
		};

		auto drawMap = get_pattern("59 E8 ? ? ? ? 83 3D ? ? ? ? ? 0F 84", 1);

		auto drawRadarDiscSprite = pattern("D8 05 ? ? ? ? D9 9C 24 ? ? ? ? DE D9 DD D8 E8").count(2);
		std::array<void*, 2> spriteDraw = {
			drawRadarDiscSprite.get(0).get<void>(17),
			drawRadarDiscSprite.get(1).get<void>(17),
		};

		// Use exactly the same patterns as widescreen fix
		float* radarPos = *get_pattern<float*>("D8 05 ? ? ? ? DE C1 D9 5C 24 28", 2);
		std::array<float**, 2> youAreHereSize = {
			get_pattern<float*>("DD D9 D9 05 ? ? ? ? D8 C9 D9 7C 24 04", 2 + 2),
			get_pattern<float*>("8B 5C 24 18 D8 0D ? ? ? ? D8 0D ? ? ? ? D9 7C 24 04", 10 + 2),
		};

		// Undo the damage caused by IVRadarScaling from the widescreen fix moving the radar way too far to the right
		// It's moved from 40.0f to 71.0f, which is way too much now that we're scaling the horizontal placement correctly!
		// This is removed from the most up-to-date widescreen fix, but keep it so we don't break with older builds.
		try
		{
			// No need to undo CRadar::DrawYouAreHereSprite, as wsfix keeps it as 40.0f

			// This hardcodes a patched constant inside so the pattern will fail to match without IV radar scaling
			auto radarRing1 = pattern("C7 84 24 ? ? ? ? 00 00 82 42").count(2);
			auto radarRing2 = pattern("D8 05 ? ? ? ? D8 05 ? ? ? ? D9 9C 24").count(2);

			// This + radarRing1 succeeding is enough proof that IVRadarScaling is in use
			if (hGameModule == ModCompat::Utils::GetModuleHandleFromAddress(radarPos) && *radarPos == (40.0f + 31.0f))
			{
				*radarPos = 40.0f;
				radarRing1.for_each_result([](pattern_match match)
					{
						Patch<float>(match.get<void>(7), 34.0f);
					});
				radarRing2.for_each_result([](pattern_match match)
					{
						static float STOCK_RADAR_POS = 40.0f;
						Patch(match.get<void>(2), &STOCK_RADAR_POS);
					});
			}
		}
		TXN_CATCH();

		// Normally we would "wrap" the global variable 40.0f used as a X radar position, but that causes issues with plugin-sdk.
		// Vice City inlined CRadar::TransformRadarPointToScreenSpace and plugin-sdk reimplements it using this global directly, so we need to patch it.
		// Therefore, we instead do the following:
		// 1. Patch the float directly, reading the original value once and rescaling as usual in-place.
		// 2. If CRadar::DrawYouAreHereSprite still points at the same global variable, give it a dedicated one.
		//    Otherwise do nothing, as some other mod (like the wsfix above) may have already done it.
		// 3. If we can't safely change the radar position because it was relocated, bail out of the fix entirely, just to be safe.
		//    Missing out on a fix is better than breaking something.
		if (hGameModule == ModCompat::Utils::GetModuleHandleFromAddress(radarPos))
		{
			static float fYouAreHereSize;
			orgRadarXPosVal = fYouAreHereSize = *radarPos;
			orgRadarXPosPtr = radarPos;

			for (float** val : youAreHereSize)
			{
				if (*val == radarPos)
				{
					Patch(val, &fYouAreHereSize);
				}
			}

			HookEach_CalculateRadarXPos_RadardiscShrink(radarXPos_RadardiscShrink, PatchFloat);
			HookEach_CalculateRadarYPos_RadardiscShrink(radarYPos_RadardiscShrink, PatchFloat);
			HookEach_DrawRadarDisc(spriteDraw, InterceptCall);
			InterceptCall(drawMap, orgDrawMap, DrawMap_RecalculatePositions<radarXPos_RadardiscShrink.size(), radarYPos_RadardiscShrink.size()>);
		}
	}
	TXN_CATCH();

	
	// Fix the onscreen counter bar placement and shadow not scaling to resolution
	try
	{
		using namespace OnscreenCounterBarFixes;

		auto atoiWrap = get_pattern("E8 ? ? ? ? D9 EE DB 05 ? ? ? ? 89 C7");
		auto shadow1 = pattern("D8 05 ? ? ? ? D9 9C 24 ? ? ? ? D9 05 ? ? ? ? D8 44 24 50").get_one();
		auto shadow2 = pattern("D9 05 ? ? ? ? D8 C1 D9 9C 24 ? ? ? ? D9 05").get_one();
		auto fill1 = pattern("D8 05 ? ? ? ? D9 9C 24 ? ? ? ? D9 05 ? ? ? ? 8D 84 24").get_one();

		std::array<float**, 6> XPositions = {
			shadow1.get<float*>(2),
			shadow2.get<float*>(2),
			fill1.get<float*>(2),
			fill1.get<float*>(13 + 2),
			get_pattern<float*>("D9 05 ? ? ? ? D8 C1 D9 9C 24 ? ? ? ? DB 44 24 6C", 2),
			get_pattern<float*>("D8 05 ? ? ? ? D9 9C 24 ? ? ? ? DE D9 E8 ? ? ? ? 59", 2),
		};

		std::array<float**, 2> YPositions = {
			shadow1.get<float*>(13 + 2),
			shadow2.get<float*>(15 + 2),
		};

		HookEach_XPos(XPositions, PatchFloat);
		HookEach_YPos(YPositions, PatchFloat);

		InterceptCall(atoiWrap, orgAtoi, atoi_RecalculatePositions<XPositions.size(), YPositions.size()>);
	}
	TXN_CATCH();


	// Fix the radar trace blip shadow not scaling to resolution
	try
	{
		using namespace RadarTraceOutlineFixes;

		auto triangle1 = pattern("D8 05 ? ? ? ? DD D9 D9 44 24 68").count(2);
		auto triangle2 = pattern("D8 05 ? ? ? ? DD DA D9 44 24 58").count(2);

		auto showRadarTraceWithHeight = pattern("E8 ? ? ? ? 83 C4 20 80 3D ? ? ? ? ? 0F 84 ? ? ? ? 30 D2").count(2);

		std::array<float**, 5> XPositions = {
			triangle1.get(0).get<float*>(2),
			triangle1.get(1).get<float*>(2),

			triangle2.get(0).get<float*>(2),
			triangle2.get(1).get<float*>(2),

			get_pattern<float*>("D8 05 ? ? ? ? DD D9 D9 44 24 50", 2),
		};

		std::array<float**, 6> YPositions = {
			get_pattern<float*>("D9 05 ? ? ? ? D8 C1 D8 6C 24 58 DD DA", 2),
			get_pattern<float*>("D9 05 ? ? ? ? D8 C1 D8 44 24 60", 2),
			get_pattern<float*>("D8 05 ? ? ? ? DD D9 D9 44 24 58", 2),

			get_pattern<float*>("D9 05 ? ? ? ? D8 C1 D8 6C 24 58 89 D0", 2),
			get_pattern<float*>("D9 05 ? ? ? ? D8 C1 D8 44 24 64", 2),
			get_pattern<float*>("D8 05 ? ? ? ? 89 D8", 2),
		};

		std::array<void*, 2> showRadarTraceWithHeight_Patches = {
			showRadarTraceWithHeight.get(0).get<void>(),
			showRadarTraceWithHeight.get(1).get<void>(),
		};

		HookEach_XPos(XPositions, PatchFloat);
		HookEach_YPos(YPositions, PatchFloat);
		PositionRecalculator<XPositions.size(), YPositions.size()>::HookEach_ShowRadarTraceWithHeight(showRadarTraceWithHeight_Patches, InterceptCall);
	}
	TXN_CATCH();


	// Fix the loading bar outline not scaling to resolution
	try
	{
		using namespace LoadingBarOutlineFixes;

		auto rgbaCtor = get_pattern("6A 00 E8 ? ? ? ? 8B 0D ? ? ? ? A1", 2);

		std::array<float**, 2> XPositions = {
			get_pattern<float*>("D9 C1 D8 25 ? ? ? ? DD DC", 2 + 2),
			get_pattern<float*>(" D8 C2 D8 05 ? ? ? ? D9 5C 24 38", 2 + 2),
		};

		std::array<float**, 2> YPositions = {
			get_pattern<float*>("DD D1 D8 25 ? ? ? ? D9 5C 24 3C", 2 + 2),
			get_pattern<float*>("D8 05 ? ? ? ? D9 5C 24 34 DE D9", 2),
		};

		HookEach_XPos(XPositions, PatchFloat);
		HookEach_YPos(YPositions, PatchFloat);
		InterceptCall(rgbaCtor, orgRGBACtor, RGBACtor_RecalculatePositions<XPositions.size(), YPositions.size()>);
	}
	TXN_CATCH();


	// Fix credits not scaling to resolution
	try
	{
		using namespace CreditsScalingFixes;

		std::array<void*, 2> creditPrintString = {
			get_pattern("E8 ? ? ? ? 83 C4 0C 8D 4C 24 14"),
			get_pattern("E8 ? ? ? ? 83 C4 0C 8B 03"),
		};

		auto setScale = get_pattern("E8 ? ? ? ? 59 59 8D 4C 24 10");

		// Fix the credits cutting off on the bottom early, they don't do that in III
		// but it regressed in VC and SA
		auto positionOffset = pattern("D8 1D ? ? ? ? DF E0 F6 C4 45 0F 85 ? ? ? ? 89 4C 24 08 DB 44 24 08 D8 25").get_one();

		// As we now scale everything on PrintString time, the resolution height checks need to be unscaled.
		void* resHeightScales[] = {
			get_pattern("8B 0D ? ? ? ? 8B 03", 2),
			get_pattern("8B 0D ? ? ? ? C7 44 24 ? ? ? ? ? 03 4C 24 14 ", 2),
		};

		static const float floatStorage[2] = { 1.0f, -(**positionOffset.get<float*>(0x19 + 2)) };
		Patch(positionOffset.get<void>(2), &floatStorage[0]);
		Patch(positionOffset.get<void>(0x19 + 2), &floatStorage[0]);

		HookEach_PrintString(creditPrintString, InterceptCall);
		InterceptCall(setScale, orgSetScale, SetScale_ScaleToRes);

		for (void* addr : resHeightScales)
		{
			Patch(addr, &FIXED_RES_HEIGHT_SCALE);
		}
	}
	TXN_CATCH();


	// Minimal HUD
	if (const int INIoption = GetPrivateProfileIntW(L"SilentPatch", L"MinimalHUD", -1, wcModulePath); INIoption != -1) try
	{
		using namespace MinimalHUD;

		// Fix original bugs

		// Wanted level stars losing their shadow if health/armour counters are off
		auto setColor_WantedStars = get_pattern("E8 ? ? ? ? DB 05 ? ? ? ? 8D 84 24 ? ? ? ? 59 50 50 D8 0D ? ? ? ? D8 0D ? ? ? ? D9 1C 24 FF 74 24 24");

		// Get SetDropShadowPosition and SetDropColor with their corresponding parameters straight from the armor counter,
		// to preserve the current behaviour
		auto setDropShadow = pattern("6A ? E8 ? ? ? ? 59 8D 8C 24 ? ? ? ? 68 ? ? ? ? 6A ? 6A ? 6A ? E8 ? ? ? ? 8D 84 24 ? ? ? ? 50 E8").get_one();


		// Show the energy values when losing armor
		auto drawFadeState_Energy = get_pattern("6A 01 E8 ? ? ? ? DD D8", 2);

		auto timeLastArmorLoss = pattern("0F B6 15 ? ? ? ? A1 ? ? ? ? 6B D2 2E 89 04 D5 ? ? ? ? D9 83").get_one();


		// Fade the weapon icon - fix the render state and pass the alpha parameter
		auto drawWeaponIconAlphaPush = get_pattern("68 FF 00 00 00 D8 0D ? ? ? ? FF 35");
		auto renderOneXLUSprite = get_pattern("E8 ? ? ? ? 83 C4 2C 6A 00 6A 08");


		// Stuff to let us (re)initialize
		static void (*HUDReInitialise)() = reinterpret_cast<decltype(HUDReInitialise)>(get_pattern("31 C0 53 0F EF C0 C6 05"));

		// This pattern has 5 hits - first 2 are in Reinitialise, the rest is in Initialise
		auto reinitialise1 = pattern("C7 05 ? ? ? ? 05 00 00 00 66 C7 05 ? ? ? ? 00 00 C7 05 ? ? ? ? 00 00 00 00").count(5);

		// This one covers the rest of Reinitialise
		auto reinitialise2 = pattern("C7 05 ? ? ? ? 05 00 00 00 C6 05 ? ? ? ? 00 C7 05 ? ? ? ? 00 00 00 00").count(2);

		// This one we touch only once, no need for static
		const std::array<uint32_t*, 4> hudInitialiseVariables = {
			reinitialise1.get(2).get<uint32_t>(6),
			reinitialise1.get(3).get<uint32_t>(6),
			reinitialise1.get(4).get<uint32_t>(6),

			get_pattern<uint32_t>("8B 83 ? ? ? ? C7 05 ? ? ? ? 05 00 00 00", 6 + 6),
		};

		static const std::array<uint32_t*, 4> hudReinitialiseVariables = {
			reinitialise1.get(0).get<uint32_t>(6),
			reinitialise1.get(1).get<uint32_t>(6),
			
			reinitialise2.get(0).get<uint32_t>(6),
			reinitialise2.get(1).get<uint32_t>(6),
		};


		ReadCall(setDropShadow.get<void>(2), orgSetDropShadowPosition);
		ReadCall(setDropShadow.get<void>(39), orgSetDropColor);
		pDropShadowSize = setDropShadow.get<int8_t>(1);
		pDropShadowB = setDropShadow.get<int8_t>(20 + 1);
		pDropShadowG = setDropShadow.get<int8_t>(22 + 1);
		pDropShadowR = setDropShadow.get<int8_t>(24 + 1);
		InterceptCall(setColor_WantedStars, orgSetColor, SetColor_SetShadow);


		pPlayerInFocus = *timeLastArmorLoss.get<uint8_t*>(3);
		pTimeLastArmorLoss = *timeLastArmorLoss.get<uint32_t*>(0xF + 3);
		InterceptCall(drawFadeState_Energy, orgDrawFadeState, DrawFadeState_CheckArmor);


		// push 0FFh -> push dword ptr [esp+520h+var_4E0]
		Patch(drawWeaponIconAlphaPush, { 0x90, 0xFF, 0x74, 0x24, 0x40 });
		InterceptCall(renderOneXLUSprite, orgRenderOneXLUSprite, RenderOneXLUSprite_FloatAlpha);

		if (INIoption != 0)
		{
			for (uint32_t* var : hudInitialiseVariables)
			{
				Patch<uint32_t>(var, 0);
			}
			for (uint32_t* var : hudReinitialiseVariables)
			{
				Patch<uint32_t>(var, 0);
			}
		}

		if (bHasDebugMenu)
		{
			static bool bMinimalHUDEnabled = INIoption != 0;
			DebugMenuAddVar("SilentPatch", "Minimal HUD", &bMinimalHUDEnabled, [] {
				if (bMinimalHUDEnabled)
				{
					for (uint32_t* var : hudReinitialiseVariables)
					{
						Memory::VP::Patch<uint32_t>(var, 0);
					}
				}
				else
				{
					for (uint32_t* var : hudReinitialiseVariables)
					{
						Memory::VP::Patch<uint32_t>(var, 5);
					}
				}

				// Call CHud::ReInitialise
				HUDReInitialise();
			});
		}
	}
	TXN_CATCH();


	// Fix some big messages staying on screen longer at high resolutions due to a cut sliding text feature
	// Also since we're touching it, optionally allow to re-enable this feature.
	try
	{
		using namespace SlidingTextsScalingFixes;

		// "Unscale" text sliding thresholds, so texts don't stay on screen longer at high resolutions
		void* scalingThreshold[] = {
			get_pattern("A1 ? ? ? ? 59 83 C0 EC", 1),
			get_pattern("59 A1 ? ? ? ? 83 C0 EC", 1 + 1)
		};

		for (void* addr : scalingThreshold)
		{
			Patch(addr, &FIXED_RES_WIDTH_SCALE);
		}

		// Optional sliding texts
		if (const int INIoption = GetPrivateProfileIntW(L"SilentPatch", L"SlidingMissionTitleText", -1, wcModulePath); INIoption != -1) try
		{
			pBigMessageX = *get_pattern<std::array<float, 6>*>("DB 44 24 68 D8 1D ? ? ? ? DF E0", 4 + 2);

			std::array<void*, 1> slidingMessage1 = {
				get_pattern("E8 ? ? ? ? 83 C4 0C EB 0A C7 05 ? ? ? ? ? ? ? ? 83 C4 60"),
			};

			std::array<void*, 1> textWrapFix = {
				get_pattern("E8 ? ? ? ? 59 E8 ? ? ? ? 6A 00 E8 ? ? ? ? 59 8B 0D"),
			};

			BigMessageSlider<1>::bSlidingEnabled = INIoption != 0;
			BigMessageSlider<1>::HookEach_PrintString(slidingMessage1, InterceptCall);
			BigMessageSlider<1>::HookEach_RightJustifyWrap(textWrapFix, InterceptCall);

			if (bHasDebugMenu)
			{
				DebugMenuAddVar("SilentPatch", "Sliding mission title text", &BigMessageSlider<1>::bSlidingEnabled, nullptr);
			}
		}
		TXN_CATCH();

		if (const int INIoption = GetPrivateProfileIntW(L"SilentPatch", L"SlidingOddJobText", -1, wcModulePath); INIoption != -1) try
		{
			pOddJob2XOffset = *get_pattern<float*>("D9 05 ? ? ? ? D8 E1 D9 1D ? ? ? ? E9", 2);

			std::array<void*, 1> slidingOddJob2 = {
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


	// Fix text background padding not scaling to resolution
	try
	{
		using namespace TextRectPaddingScalingFixes;

		auto getTextRect = get_pattern("FF 74 24 54 FF 74 24 54 50 E8 ? ? ? ? 83 C4 10", 9);
		auto rectWidth1 = pattern("D8 25 ? ? ? ? D9 1E D9 44 24 38 D8 05 ? ? ? ? D8 05").get_one();
		auto rectWidth2 = pattern("D8 25 ? ? ? ? D9 1E D9 05 ? ? ? ? D8 0D ? ? ? ? D8 44 24 38 D8 05").get_one();
		auto rectWidth3 = get_pattern<float*>("D8 25 ? ? ? ? 0F BF C5 D9 1E", 2);

		auto rectHeight1 = pattern("D8 05 ? ? ? ? D9 5E 04 D9 44 24 3C D8 25").count(2);
		auto rectHeight2 = pattern("D9 05 ? ? ? ? D8 44 24 3C DE C1 D8 05").get_one();

		// SetWrapx on the help boxes includes an unscaled -4.0f probably to work together with this padding,
		// so treat it as part of the same fix
		auto setJustifyOff_helpBox = get_pattern("59 E8 ? ? ? ? D9 EE", 1);

		std::array<float**, 5> paddingXSizes = {
			rectWidth1.get<float*>(2),
			rectWidth1.get<float*>(0x12 + 2),
			rectWidth2.get<float*>(2),
			rectWidth2.get<float*>(0x18 + 2),
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


	// Fix ammunation text (big message type 3) Y position offset not scaling to resolution
	try
	{
		using namespace BigMessage3ScalingFixes;

		auto setDropColor = get_pattern("E8 ? ? ? ? 59 8D 4C 24 40");
		std::array<float**, 1> YOffset = {
			get_pattern<float*>("D8 25 ? ? ? ? D9 1C 24 A1", 2),
		};

		HookEach_MessageYOffset(YOffset, PatchFloat);
		InterceptCall(setDropColor, orgSetDropColor, SetDropColor_Scale<YOffset.size()>);
	}
	TXN_CATCH();

	FLAUtils::Init(moduleList);
}

void InjectDelayedPatches_VC_Common()
{
	std::unique_ptr<ScopedUnprotect::Unprotect> Protect = ScopedUnprotect::UnprotectSectionOrFullModule( GetModuleHandle( nullptr ), ".text" );

	// Obtain a path to the ASI
	wchar_t			wcModulePath[MAX_PATH];
	GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), wcModulePath, _countof(wcModulePath) - 3); // Minus max required space for extension
	PathRenameExtensionW(wcModulePath, L".ini");

	const bool hasDebugMenu = DebugMenuLoad();

	SelectableBackfaceCulling::ReadDrawBackfacesExclusions(wcModulePath);

	InjectDelayedPatches_VC_Common( hasDebugMenu, wcModulePath );

	Common::Patches::III_VC_DelayedCommon( hasDebugMenu, wcModulePath );
}

void Patch_VC_10(uint32_t width, uint32_t height)
{
	using namespace Memory::DynBase;

	RsGlobal = *(RsGlobalType**)DynBaseAddress(0x602D32);
	SubtitlesShadowFix_JumpBack = (void*)DynBaseAddress(0x551701);

	InjectHook(0x5433BD, FixedRefValue);

	InjectHook(0x5516FC, SubtitlesShadowFix, HookType::Jump);
	Patch<BYTE>(0x5517C4, 0xD9);
	Patch<BYTE>(0x5517DF, 0xD9);
	Patch<BYTE>(0x551832, 0xD9);
	Patch<BYTE>(0x551848, 0xD9);
	Patch<BYTE>(0x5517E2, 0x34-0x14);
	Patch<BYTE>(0x55184B, 0x34-0x14);
	Patch<BYTE>(0x5517C7, 0x28-0x18);
	Patch<BYTE>(0x551835, 0x24-0x18);
	Patch<BYTE>(0x5516FB, 0x90);

	{
		using namespace PrintStringShadows;

		XY<0x5FA1F6, 0x5FA1D5>::Hook(0x5FA1FD);
		XMinus<0x544727/*, 0x544727*/>::Hook(0x54474D); // Don't patch Y as we're doing it in the credits scale fix
	}

	// Mouse fucking fix!
	Patch<DWORD>(0x601740, 0xC3C030);

	// (Hopefully) more precise frame limiter
	ReadCall( 0x6004A2, RsEventHandler );
	InjectHook(0x6004A2, NewFrameRender);
	InjectHook(0x600449, GetTimeSinceLastFrame);


	// RsMouseSetPos call (SA style fix)
	ReadCall( 0x4A5E45, orgConstructRenderList );
	InjectHook(0x4A5E45, ResetMousePos);

	// New wndproc
	OldWndProc = *(LRESULT (CALLBACK***)(HWND, UINT, WPARAM, LPARAM))DynBaseAddress(0x601727);
	Patch(0x601727, &pCustomWndProc);

	// Y axis sensitivity fix
	// By ThirteenAG
	float* sens = *(float**)DynBaseAddress(0x4796E5);
	Patch<const void*>(0x479410 + 0x2E0 + 0x2, sens);
	Patch<const void*>(0x47A20E + 0x27D + 0x2, sens);
	Patch<const void*>(0x47AE27 + 0x1CC + 0x2, sens);
	Patch<const void*>(0x47BE8F + 0x22E + 0x2, sens);
	Patch<const void*>(0x481AB3 + 0x4FE + 0x2, sens);

	// Don't lock mouse Y axis during fadeins
	Patch<BYTE>(0x47C11E, 0xEB);
	Patch<BYTE>(0x47CD94, 0xEB);
	Nop(0x47C15A, 2);

	// Scan for A/B drives looking for audio files
	Patch<DWORD>(0x5D7941, 'A');
	Patch<DWORD>(0x5D7B04, 'A');


	// ~x~ as cyan blip
	// Shared with GInput
	Patch<BYTE>(0x550481, 0);
	Patch<BYTE>(0x550488, 255);
	Patch<BYTE>(0x55048F, 255);

	Patch<BYTE>(0x5505FF, 0);
	Patch<BYTE>(0x550603, 255);
	Patch<BYTE>(0x550607, 255);

	
	// Corrected crime codes
	Patch<DWORD>(0x5FDDDB, 0xC5);


	// Fixed ammo for melee weapons in cheats
	Patch<BYTE>(0x4AED14+1, 1); // katana
	Patch<BYTE>(0x4AEB74+1, 1); // chainsaw

	// Fixed crash related to autopilot timing calculations
	InjectHook(0x418FAE, AutoPilotTimerFix_VC, HookType::Jump);

	Common::Patches::DDraw_VC_10( width, height, aNoDesktopMode );
}

void Patch_VC_11(uint32_t width, uint32_t height)
{
	using namespace Memory::DynBase;

	RsGlobal = *(RsGlobalType**)DynBaseAddress(0x602D12);
	SubtitlesShadowFix_JumpBack = (void*)DynBaseAddress(0x551721);

	InjectHook(0x5433DD, FixedRefValue);

	InjectHook(0x55171C, SubtitlesShadowFix, HookType::Jump);
	Patch<BYTE>(0x5517E4, 0xD9);
	Patch<BYTE>(0x5517FF, 0xD9);
	Patch<BYTE>(0x551852, 0xD9);
	Patch<BYTE>(0x551868, 0xD9);
	Patch<BYTE>(0x551802, 0x34-0x14);
	Patch<BYTE>(0x55186B, 0x34-0x14);
	Patch<BYTE>(0x5517E7, 0x28-0x18);
	Patch<BYTE>(0x551855, 0x24-0x18);
	Patch<BYTE>(0x55171B, 0x90);

	{
		using namespace PrintStringShadows;

		XY<0x5FA216, 0x5FA1F5>::Hook(0x5FA21D);
		XMinus<0x544747/*, 0x544747*/>::Hook(0x54476D); // Don't patch Y as we're doing it in the credits scale fix
	}

	// Mouse fucking fix!
	Patch<DWORD>(0x601770, 0xC3C030);

	// (Hopefully) more precise frame limiter
	ReadCall( 0x6004C2, RsEventHandler );
	InjectHook(0x6004C2, NewFrameRender);
	InjectHook(0x600469, GetTimeSinceLastFrame);

	// RsMouseSetPos call (SA style fix)
	ReadCall( 0x4A5E65, orgConstructRenderList );
	InjectHook(0x4A5E65, ResetMousePos);

	// New wndproc
	OldWndProc = *(LRESULT (CALLBACK***)(HWND, UINT, WPARAM, LPARAM))DynBaseAddress(0x601757);
	Patch(0x601757, &pCustomWndProc);

	// Y axis sensitivity fix
	// By ThirteenAG
	float* sens = *(float**)DynBaseAddress(0x4796E5);
	Patch<const void*>(0x479410 + 0x2E0 + 0x2, sens);
	Patch<const void*>(0x47A20E + 0x27D + 0x2, sens);
	Patch<const void*>(0x47AE27 + 0x1CC + 0x2, sens);
	Patch<const void*>(0x47BE8F + 0x22E + 0x2, sens);
	Patch<const void*>(0x481AB3 + 0x4FE + 0x2, sens);

	// Don't lock mouse Y axis during fadeins
	Patch<BYTE>(0x47C11E, 0xEB);
	Patch<BYTE>(0x47CD94, 0xEB);
	Nop(0x47C15A, 2);

	// Scan for A/B drives looking for audio files
	Patch<DWORD>(0x5D7961, 'A');
	Patch<DWORD>(0x5D7B24, 'A');


	// ~x~ as cyan blip
	// Shared with GInput
	Patch<BYTE>(0x5504A1, 0);
	Patch<BYTE>(0x5504A8, 255);
	Patch<BYTE>(0x5504AF, 255);

	Patch<BYTE>(0x55061F, 0);
	Patch<BYTE>(0x550623, 255);
	Patch<BYTE>(0x550627, 255);


	// Corrected crime codes
	Patch<DWORD>(0x5FDDFB, 0xC5);


	// Fixed ammo for melee weapons in cheats
	Patch<BYTE>(0x4AED34+1, 1); // katana
	Patch<BYTE>(0x4AEB94+1, 1); // chainsaw

	// Fixed crash related to autopilot timing calculations
	InjectHook(0x418FAE, AutoPilotTimerFix_VC, HookType::Jump);

	Common::Patches::DDraw_VC_11( width, height, aNoDesktopMode );
}

void Patch_VC_Steam(uint32_t width, uint32_t height)
{
	using namespace Memory::DynBase;

	RsGlobal = *(RsGlobalType**)DynBaseAddress(0x602952);
	SubtitlesShadowFix_JumpBack = (void*)DynBaseAddress(0x5515F1);

	InjectHook(0x5432AD, FixedRefValue);

	InjectHook(0x5515EC, SubtitlesShadowFix, HookType::Jump);
	Patch<BYTE>(0x5516B4, 0xD9);
	Patch<BYTE>(0x5516CF, 0xD9);
	Patch<BYTE>(0x551722, 0xD9);
	Patch<BYTE>(0x551738, 0xD9);
	Patch<BYTE>(0x5516D2, 0x34-0x14);
	Patch<BYTE>(0x55173B, 0x34-0x14);
	Patch<BYTE>(0x5516B7, 0x28-0x18);
	Patch<BYTE>(0x551725, 0x24-0x18);
	Patch<BYTE>(0x5515EB, 0x90);

	{
		using namespace PrintStringShadows;

		XY<0x5F9E56, 0x5F9E35>::Hook(0x5F9E5D);
		XMinus<0x544617/*, 0x544617*/>::Hook(0x54463D); // Don't patch Y as we're doing it in the credits scale fix
	}

	// Mouse fucking fix!
	Patch<DWORD>(0x6013B0, 0xC3C030);

	// (Hopefully) more precise frame limiter
	ReadCall( 0x600102, RsEventHandler );
	InjectHook(0x600102, NewFrameRender);
	InjectHook(0x6000A9, GetTimeSinceLastFrame);

	// RsMouseSetPos call (SA style fix)
	ReadCall( 0x4A5D15, orgConstructRenderList );
	InjectHook(0x4A5D15, ResetMousePos);

	// New wndproc
	OldWndProc = *(LRESULT (CALLBACK***)(HWND, UINT, WPARAM, LPARAM))DynBaseAddress(0x601397);
	Patch(0x601397, &pCustomWndProc);

	// Y axis sensitivity fix
	// By ThirteenAG
	float* sens = *(float**)DynBaseAddress(0x4795C5);
	Patch<const void*>(0x4792F0 + 0x2E0 + 0x2, sens);
	Patch<const void*>(0x47A0EE + 0x27D + 0x2, sens);
	Patch<const void*>(0x47AD07 + 0x1CC + 0x2, sens);
	Patch<const void*>(0x47BD6F + 0x22E + 0x2, sens);
	Patch<const void*>(0x481993 + 0x4FE + 0x2, sens);

	// Don't lock mouse Y axis during fadeins
	Patch<BYTE>(0x47BFFE, 0xEB);
	Patch<BYTE>(0x47CC74, 0xEB);
	Nop(0x47C03A, 2);

	// Scan for A/B drives looking for audio files
	Patch<DWORD>(0x5D7764, 'A');


	// ~x~ as cyan blip
	// Shared with GInput
	Patch<BYTE>(0x550371, 0);
	Patch<BYTE>(0x550378, 255);
	Patch<BYTE>(0x55037F, 255);

	Patch<BYTE>(0x5504EF, 0);
	Patch<BYTE>(0x5504F3, 255);
	Patch<BYTE>(0x5504F7, 255);


	// Corrected crime codes
	Patch<DWORD>(0x5FDA3B, 0xC5);


	// Fixed ammo for melee weapons in cheats
	Patch<BYTE>(0x4AEA44+1, 1); // katana
	Patch<BYTE>(0x4AEBE4+1, 1); // chainsaw

	// Fixed crash related to autopilot timing calculations
	InjectHook(0x418FAE, AutoPilotTimerFix_VC, HookType::Jump);

	Common::Patches::DDraw_VC_Steam( width, height, aNoDesktopMode );
}

void Patch_VC_JP()
{
	using namespace Memory::DynBase;

	// Y axis sensitivity fix
	// By ThirteenAG
	Patch<DWORD>(0x4797E7 + 0x2E0 + 0x2, 0x94ABD8);
	Patch<DWORD>(0x47A5E5 + 0x27D + 0x2, 0x94ABD8);
	Patch<DWORD>(0x47B1FE + 0x1CC + 0x2, 0x94ABD8);
	Patch<DWORD>(0x47C266 + 0x22E + 0x2, 0x94ABD8);
	Patch<DWORD>(0x481E8A + 0x4FE + 0x2, 0x94ABD8);
}

void Patch_VC_Common()
{
	using namespace Memory;
	using namespace hook::txn;

	const HMODULE hGameModule = GetModuleHandle(nullptr);

	// New timers fix
	try
	{
		auto hookPoint = pattern( "83 E4 F8 89 44 24 08 C7 44 24 0C 00 00 00 00 DF 6C 24 08" ).get_one();
		auto jmpPoint = get_pattern( "DD D8 E9 31 FF FF FF" );

		InjectHook( hookPoint.get<void>( 0x21 ), CTimer::Update_SilentPatch, HookType::Call );
		InjectHook( hookPoint.get<void>( 0x21 + 5 ), jmpPoint, HookType::Jump );
	}
	TXN_CATCH();


	// Alt+F4
	try
	{
		auto addr = pattern( "59 59 31 C0 83 C4 70 5D 5F 5E 5B C2 10 00" ).count(2);
		auto dest = get_pattern( "53 55 56 FF B4 24 90 00 00 00 FF 15" );

		addr.for_each_result( [&]( pattern_match match ) {
			InjectHook( match.get<void>( 2 ), dest, HookType::Jump );
		});
	}
	TXN_CATCH();


	// Proper panels damage
	try
	{
		auto addr = pattern( "C6 41 09 03 C6 41 0A 03 C6 41 0B 03" ).get_one();

		// or dword ptr[ecx+14], 3300000h
		// nop
		Patch( addr.get<void>( 0x18 ), { 0x81, 0x49, 0x14, 0x00, 0x00, 0x30, 0x03 }  );
		Nop( addr.get<void>( 0x18 + 7 ), 13 );

		Nop( addr.get<void>( 0x33 ), 7 );
	}
	TXN_CATCH();


	// Proper metric-imperial conversion constants
	try
	{
		static const float METERS_TO_MILES = 0.0006213711922f;
		auto addr = pattern( "75 ? D9 05 ? ? ? ? D8 0D ? ? ? ? 6A 00 6A 00 D9 9C 24" ).count(6);
		auto sum = get_pattern( "D9 9C 24 A8 00 00 00 8D 84 24 A8 00 00 00 50", -6 + 2 );

		addr.for_each_result( [&]( pattern_match match ) {
			Patch<const void*>( match.get<void>( 0x8 + 2 ), &METERS_TO_MILES );
		});

		Patch<const void*>( sum, &METERS_TO_MILES );
	}
	TXN_CATCH();


	// Improved pathfinding in PickNextNodeAccordingStrategy - PickNextNodeToChaseCar with XYZ coords
	try
	{
		auto addr = pattern( "E8 ? ? ? ? 50 8D 44 24 10 50 E8" ).get_one();
		ReadCall( addr.get<void>( 0x25 ), orgPickNextNodeToChaseCar );

		const uintptr_t funcAddr = (uintptr_t)get_pattern( "8B 9C 24 BC 00 00 00 66 8B B3 A6 01 00 00 66 85 F6", -0xA );
		InjectHook( funcAddr - 5, PickNextNodeToChaseCarXYZ, HookType::Jump ); // For plugin-sdk

		// push PickNextNodeToChaseCarZ instead of 0.0f
		// mov ecx, [PickNextNodeToChaseCarZ]
		// mov [esp+0B8h+var_2C], ecx
		Patch( funcAddr + 0x5D, { 0x8B, 0x0D } );
		Patch<const void*>( funcAddr + 0x5D + 2, &PickNextNodeToChaseCarZ );
		Patch( funcAddr + 0x5D + 6, { 0x89, 0x8C, 0x24, 0x8C, 0x00, 0x00, 0x00 } );

		// lea eax, [ecx+edx*4] -> lea eax, [edx+edx*4]
		Patch<uint8_t>( funcAddr + 0x6E + 2, 0x92 );


		// lea eax, [esp+20h+var_10]
		// push eax
		// nop...
		Patch( addr.get<void>( 0x10 ), { 0x83, 0xC4, 0x04, 0x8D, 0x44, 0x24, 0x10, 0x50, 0xEB, 0x0A } );
		InjectHook( addr.get<void>( 0x25 ), PickNextNodeToChaseCarXYZ );
		Patch<uint8_t>( addr.get<void>( 0x2A + 2 ), 0xC );

		// push edx
		// nop...
		Patch<uint8_t>( addr.get<void>( 0x3E ), 0x52 );
		Nop( addr.get<void>( 0x3E + 1 ), 6 );
		InjectHook( addr.get<void>( 0x46 ), PickNextNodeToChaseCarXYZ );
		Patch<uint8_t>( addr.get<void>( 0x4B + 2 ), 0xC );
	}
	TXN_CATCH();


	// No censorships
	try
	{
		auto addr = get_pattern( "8B 43 50 85 C0 8B 53 50 74 2B 83 E8 01" );
		Patch( addr, { 0x83, 0xC4, 0x08, 0x5B, 0xC3 } );	// add     esp, 8 \ pop ebx \ retn
	}
	TXN_CATCH();


	// 014C cargen counter fix (by spaceeinstein)
	try
	{
		auto do_processing = pattern( "0F B7 43 28 83 F8 FF 7D 04 66 FF 4B 28" ).get_one();

		Patch<uint8_t>( do_processing.get<uint8_t*>(1), 0xBF ); // movzx   eax, word ptr [ebx+28h] -> movsx   eax, word ptr [ebx+28h]
		Patch<uint8_t>( do_processing.get<uint8_t*>(7), 0x74 ); // jge -> jz
	}
	TXN_CATCH();


	// Fixed ammo from SCM
	try
	{
		using namespace ZeroAmmoFix;

		std::array<void*, 2> give_weapon = {
			get_pattern( "6B C0 2E 6A 01 56 8B 3C", 0x15 ),
			get_pattern( "89 F9 6A 01 55 50 E8", 6 ),
		};
		HookEach_GiveWeapon(give_weapon, InterceptCall);
	}
	TXN_CATCH();


	// Extras working correctly on bikes
	try
	{
		auto createInstance = get_pattern( "89 C1 8B 41 04" );
		InjectHook( createInstance, CreateInstance_BikeFix, HookType::Call );
	}
	TXN_CATCH();


	// Credits =)
	try
	{
		auto renderCredits = pattern( "8D 44 24 28 83 C4 14 50 FF 35 ? ? ? ? E8 ? ? ? ? 8D 44 24 1C 59 59 50 FF 35 ? ? ? ? E8 ? ? ? ? 59 59" ).get_one();

		ReadCall( renderCredits.get<void>( -50 ), Credits::PrintCreditText );
		ReadCall( renderCredits.get<void>( -5 ), Credits::PrintCreditText_Hooked );
		InjectHook( renderCredits.get<void>( -5 ), Credits::PrintSPCredits );
	}
	TXN_CATCH();


	// Decreased keyboard input latency
	try
	{
		using namespace KeyboardInputFix;

		auto updatePads = pattern( "66 8B 42 1A" ).get_one();
		void* jmpDest = get_pattern( "66 A3 ? ? ? ? 5F", 6 );
		void* simButtonCheckers = get_pattern( "56 57 B3 01", 0x16 );

		NewKeyState = *updatePads.get<void*>( 0x27 + 1 );
		OldKeyState = *updatePads.get<void*>( 4 + 1 );
		TempKeyState = *updatePads.get<void*>( 0x270 + 1 );

		ReadCall( simButtonCheckers, orgClearSimButtonPressCheckers );
		InjectHook( simButtonCheckers, ClearSimButtonPressCheckers );
		InjectHook( updatePads.get<void>( 9 ), jmpDest, HookType::Jump );
	}
	TXN_CATCH();


	// Locale based metric/imperial system
	try
	{
		using namespace Localization;

		void* updateCompareFlag = get_pattern( "89 D9 6A 00 E8 ? ? ? ? 30 C0 83 C4 70 5D 5F 5E 5B C2 04 00", 4 );
		auto constructStatLine = pattern( "85 C0 74 11 83 E8 01 83 F8 03" ).get_one();

		ReadCall( updateCompareFlag, orgUpdateCompareFlag_IsMetric );
		InjectHook( updateCompareFlag, UpdateCompareFlag_IsMetric );

		// Stats
		Nop( constructStatLine.get<void>( -11 ), 1 );
		InjectHook( constructStatLine.get<void>( -11 + 1 ), PrefsLanguage_IsMetric, HookType::Call );
		Nop( constructStatLine.get<void>( -2 ), 2 );
	}
	TXN_CATCH();


	// Corrected FBI Washington sirens sound
	// Primary siren lower pitched like in FBI Rancher and secondary siren higher pitched
	try
	{
		using namespace SirenSwitchingFix;

		// Other mods might be touching it, so only patch specific vehicles if their code has not been touched at all
		auto sirenPitch = pattern( "83 F8 17 74 32" ).get_one();

		InjectHook( sirenPitch.get<void>( 5 ), IsFBIRanchOrFBICar, HookType::Call );
		Patch( sirenPitch.get<void>( 5 + 5 ), { 0x84, 0xC0 } ); // test al, al
		Nop( sirenPitch.get<void>( 5 + 5 + 2 ), 4 );

		// Pitch shift FBI Washington primary siren
		try
		{
			struct tVehicleSampleData {
				int m_nAccelerationSampleIndex;
				char m_bEngineSoundType;
				int m_nHornSample;
				int m_nHornFrequency;
				char m_nSirenOrAlarmSample;
				int m_nSirenOrAlarmFrequency;
				char m_bDoorType;
			};

			tVehicleSampleData* dataTable = *get_pattern<tVehicleSampleData*>( "8B 04 95 ? ? ? ? 89 43 1C", 3 );
			// Only pitch shift if table hasn't been relocated elsewhere
			if ( hGameModule == ModCompat::Utils::GetModuleHandleFromAddress(dataTable) )
			{
				// fbicar frequency = fbiranch frequency
				dataTable[17].m_nSirenOrAlarmFrequency = dataTable[90].m_nSirenOrAlarmFrequency;
			}
		}
		TXN_CATCH();
	}
	TXN_CATCH();


	// Allow extra6 to be picked with component rule 4 (any)
	try
	{
		void* extraMult6 = get_pattern( "D8 0D ? ? ? ? D9 7C 24 04 8B 44 24 04 80 4C 24 05 0C D9 6C 24 04 89 44 24 04 DB 5C 24 08 D9 6C 24 04 8B 44 24 08 83 C4 10 5D", 2 );

		static const float MULT_6 = 6.0f;
		Patch( extraMult6, &MULT_6 );
	}
	TXN_CATCH();

	
	// Make drive-by one shot sounds owned by the driver instead of the car
	// Fixes incorrect weapon sound being used for drive-by
	try
	{
		auto getDriverOneShot = pattern( "FF 35 ? ? ? ? 6A 37 50 E8 ? ? ? ? 83 7E 08 00" ).get_one();

		// nop
		// mov ecx, ebx
		// call CVehicle::GetOneShotOwnerID
		Patch( getDriverOneShot.get<void>( -8 ), { 0x90, 0x89, 0xD9 } );
		InjectHook( getDriverOneShot.get<void>( -5 ), &CVehicle::GetOneShotOwnerID_SilentPatch, HookType::Call );
	}
	TXN_CATCH();


	// Fixed vehicles exploding twice if the driver leaves the car while it's exploding
	try
	{
		using namespace RemoveDriverStatusFix;

		auto removeDriver = pattern("8A 43 50 24 07 0C 20 88 43 50 E8").get_one();
		auto processCommands1 = get_pattern("88 42 50 8B 33");
		auto processCommands2 = get_pattern("88 42 50 8B AE");
		auto removeThisPed = get_pattern("88 42 50 8B 85");
		auto pedSetOutCar = get_pattern("0C 20 88 47 50 8B 85", 2);

		Nop(removeDriver.get<void>(), 2);
		InjectHook(removeDriver.get<void>(2), RemoveDriver_SetStatus, HookType::Call);

		// CVehicle::RemoveDriver already sets the status to STATUS_ABANDONED, these are redundant
		Nop(processCommands1, 3);
		Nop(processCommands2, 3);
		Nop(removeThisPed, 3);
		Nop(pedSetOutCar, 3);
	}
	TXN_CATCH();


	// Apply the environment mapping on extra components
	try
	{
		using namespace EnvMapsOnExtras;

		auto forAllAtomics = pattern("50 E8 ? ? ? ? 66 8B 4B 44").get_one();
		auto setEnvMapCoefficient = reinterpret_cast<decltype(RpMatFXMaterialSetEnvMapCoefficient)>(get_pattern("8B 44 24 14 81 E2 FF 00 00 00 8D 14 52 8D 0C D6 89 41 08", -0x48));
		auto getEffects = reinterpret_cast<decltype(RpMatFXMaterialGetEffects)>(get_pattern("8B 04 01 85 C0 75 01", -0xA));

		// push eax -> push ebx
		Patch<uint8_t>(forAllAtomics.get<void>(), 0x53);
		InterceptCall(forAllAtomics.get<void>(1), orgRpClumpForAllAtomics, RpClumpForAllAtomics_ExtraComps);

		RpMatFXMaterialSetEnvMapCoefficient = setEnvMapCoefficient;
		RpMatFXMaterialGetEffects = getEffects;
	}
	TXN_CATCH();


	// Fix probabilities in CVehicle::InflictDamage incorrectly assuming a random range from 0 to 100.000
	try
	{
		auto probability = get_pattern("66 81 7B 5A ? ? 73 50", 4);

		Patch<uint16_t>(probability, 35000u / 2u);
	}
	TXN_CATCH();


	// Null terminate read lines in CPlane::LoadPath
	try
	{
		using namespace NullTerminatedLines;

		auto loadPath = get_pattern("DD D8 45 E8", 3);

		InterceptCall(loadPath, orgSscanf_LoadPath, sscanf1_LoadPath_Terminate);
	}
	TXN_CATCH();


	// Don't reset mouse sensitivity on New Game
	try
	{
		using namespace MouseSensNewGame;

		auto cameraInit = pattern("C7 85 14 09 00 00 00 00 00 00 C7 05 ? ? ? ? ? ? ? ? C7 05").get_one();
		auto setDirMyDocuments = get_pattern("89 CD E8 ? ? ? ? 68", 2);

		DefaultHorizontalAccel = *cameraInit.get<float>(20 + 2 + 4);
		fMouseAccelHorzntl = *cameraInit.get<float*>(20 + 2);

		Nop(cameraInit.get<void>(20), 10);
		InterceptCall(setDirMyDocuments, orgSetDirMyDocuments, SetDirMyDocuments_ResetMouse);
	}
	TXN_CATCH();


	// Fixed pickup effects
	try
	{
		using namespace PickupEffectsFixes;

		// Give money pickups color ID 37, like most other "generic" pickups
		// Coincidentally, it's also the most likely color to be "randomly" assigned to them now
		auto bigDollarColor = get_pattern("C6 44 24 ? 00 E9 ? ? ? ? 8D 80 00 00 00 00 0F B7 1D ? ? ? ? 39 CB 75 0C");

		// Remove the glow from minigun2
		auto minigun2Glow = get_pattern("8D 41 01 89 CB");

		InjectHook(bigDollarColor, &PickUpEffects_BigDollarColor, HookType::Call);
		InjectHook(minigun2Glow, &PickUpEffects_Minigun2Glow, HookType::Call);
	}
	TXN_CATCH();


	// Fixed the muzzle flash facing the wrong direction
	// By Wesser
	try
	{
		auto fireInstantHit = pattern("D9 44 24 50 D8 44 24 44").get_one();

		// Replace fld [esp].vecSource with fldz, as vecEnd is already absolute
		Patch(fireInstantHit.get<void>(), { 0xD9, 0xEE, 0x90, 0x90 });
		Patch(fireInstantHit.get<void>(15), { 0xD9, 0xEE, 0x90, 0x90 });
		Patch(fireInstantHit.get<void>(30), { 0xD9, 0xEE, 0x90, 0x90 });
	}
	TXN_CATCH();


	// Fixed IS_PLAYER_TARGETTING_CHAR incorrectly detecting targetting in Classic controls
	// when the player is not aiming
	// By Wesser
	try
	{
		using namespace IsPlayerTargettingCharFix;

		auto isPlayerTargettingChar = pattern("83 7C 24 ? ? A3 ? ? ? ? 0F 84").get_one();
		auto using1stPersonWeaponMode = reinterpret_cast<decltype(Using1stPersonWeaponMode)>(get_pattern("66 83 F8 07 74 18", -7));
		bool* useMouse3rdPerson = *get_pattern<bool*>("80 3D ? ? ? ? ? 75 09 66 C7 05 ? ? ? ? ? ? 8B 35", 2);
		void* theCamera = *get_pattern<void*>("B9 ? ? ? ? 31 DB E8", 1);

		Using1stPersonWeaponMode = using1stPersonWeaponMode;
		bUseMouse3rdPerson = useMouse3rdPerson;
		TheCamera = theCamera;

		// Move mov ds:dword_784030, eax one instruction earlier so we don't need
		// to include it in the patched routine
		memmove(isPlayerTargettingChar.get<void>(), isPlayerTargettingChar.get<void>(5), 5);
		InjectHook(isPlayerTargettingChar.get<void>(5), IsPlayerTargettingChar_ExtraChecks, HookType::Call);
	}
	TXN_CATCH();


	// Use PS2 randomness for Rosenberg audio to hopefully bring the odds closer to PS2
	// The functionality was never broken on PC - but the random distribution seemingly made it looks as if it was
	try
	{
		using namespace ConsoleRandomness;

		auto busted_audio_rand = get_pattern("80 BB 48 01 00 00 00 0F 85 ? ? ? ? E8 ? ? ? ? 25 FF FF 00 00", 13);
		InjectHook(busted_audio_rand, rand15);
	}
	TXN_CATCH();


	// Reset variables on New Game
	try
	{
		using namespace VariableResets;

		auto game_initialise = get_pattern("6A 00 E8 ? ? ? ? 83 C4 0C 68 ? ? ? ? E8 ? ? ? ? 59 C3", 15);
		std::array<void*, 2> reinit_game_object_variables = {
			get_pattern("74 05 E8 ? ? ? ? E8 ? ? ? ? 80 3D", 7),
			get_pattern("C6 05 ? ? ? ? ? E8 ? ? ? ? C7 05", 7)
		};

		TimerInitialise = reinterpret_cast<decltype(TimerInitialise)>(get_pattern("83 E4 F8 68 ? ? ? ? E8", -6));

		InterceptCall(game_initialise, orgGameInitialise, GameInitialise);
		HookEach_ReInitGameObjectVariables(reinit_game_object_variables, InterceptCall);

		// Variables to reset
		GameVariablesToReset.emplace_back(*get_pattern<bool*>("7D 09 80 3D ? ? ? ? ? 74 32", 2 + 2)); // Free resprays
		GameVariablesToReset.emplace_back(*get_pattern<int*>("7D 78 A1 ? ? ? ? 05", 2 + 1)); // LastTimeAmbulanceCreated
		GameVariablesToReset.emplace_back(*get_pattern<int*>("A1 ? ? ? ? 05 ? ? ? ? 39 05 ? ? ? ? 0F 86 ? ? ? ? 8B 15", 1)); // LastTimeFireTruckCreated
		GameVariablesToReset.emplace_back(*get_pattern<int*>("FF 0D ? ? ? ? EB 15 90", 2)); // CWeather::StreamAfterRainTimer
	}
	TXN_CATCH();


	// Ped speech fix
	// Based off Sergeanur's fix
	try
	{
		// Remove the artificial 6s delay between any ped speech samples
		auto delay_check = get_pattern("80 BE ? ? ? ? ? 0F 85 ? ? ? ? B9", 7);
		auto comment_delay_id1 = get_pattern("0F B7 C2 DD D8 C1 E0 04");
		auto comment_delay_id2 = pattern("0F B7 95 DA 05 00 00 D9 6C 24 04").get_one();

		Nop(delay_check, 6);

		// movzx eax, dx -> movzx eax, bx
		Patch(comment_delay_id1, { 0x0F, 0xB7, 0xC3 });

		// movzx edx, word ptr [ebp+5DAh] -> movzx edx, bx \ nop
		Patch(comment_delay_id2.get<void>(), { 0x0F, 0xB7, 0xD3 });
		Nop(comment_delay_id2.get<void>(3), 4);
	}
	TXN_CATCH();


	// Disabled backface culling on detached car parts, peds and specific models
	try
	{
		using namespace SelectableBackfaceCulling;

		auto entity_render = pattern("56 75 06 5E 5B C3").get_one();

		EntityRender_Prologue_JumpBack = entity_render.get<void>();
		
		// Check if CEntity::Render is already re-routed by something else
		if (*entity_render.get<uint8_t>(-7) == 0xE9)
		{
			ReadCall(entity_render.get<void>(-7), orgEntityRender);
		}

		InjectHook(entity_render.get<void>(-7), EntityRender_BackfaceCulling, HookType::Jump);
	}
	TXN_CATCH();


	// Correct the duration of the outro splash to 2.5 seconds
	// The outro splash displays for 150 ticks from the moment it fully fades in, with the tick cpimt supposedly incrementing every 10ms
	// However, since the game is locked to 30FPS, the tick count actually increments every 33.3ms, so the splash takes around 5s
	// Correct the "tick rate" to 33ms and the "tick count" to 75, so it is more or less 2.5s at 30FPS,
	// and similarly long when running at 60FPS or uncapped. The original code hints at 1.5s,
	// but that makes it hard to read the splash before it vanishes
	//
	// Also fix the splash flickering for a frame when fading in
	try
	{
		using namespace OutroSplashFix;

		auto outro_tick_rate = get_pattern("83 F8 0A 76 10", 2);
		auto outro_tick_count = get_pattern("81 3D ? ? ? ? 96 00 00 00", 6);
		auto splash_rgba = get_pattern("E8 ? ? ? ? DB 05 ? ? ? ? 8D 54 24 2C");
		auto alpha_clamp = get_pattern("8A 83 ? ? ? ? 8D 4C 24 2C");

		// Ideally, we want (time - lastTime) >= 33, but we can express the same with > 32
		Patch<uint8_t>(outro_tick_rate, 32);
		Patch<uint32_t>(outro_tick_count, 75);

		InterceptCall(splash_rgba, orgRGBASet, RGBASet_Clamp);

		// al -> eax
		Patch<uint8_t>(alpha_clamp, 0x8B);
	}
	TXN_CATCH();


	// Fix Tommy not shaking his fists with brass knuckles (in all cases)
	// and most post-GTA III weapons (when cars slow down for him)
	try
	{
		using namespace TommyFistShakeWithWeapons;

		auto weapon_group_1a = pattern("8B 40 60 59 83 F8 01 75 43").get_one();
		auto weapon_group_1b = pattern("8B 40 60 59 83 F8 01 0F 85 ? ? ? ? 8A 85").get_one();
		auto slow_car_down_for_peds = pattern("89 C7 8B 17 85 D2 74 19").get_one();
		auto else_jump = get_pattern("0F 85 ? ? ? ? 8A 83 ? ? ? ? 24 FE");

		std::array<void*, 2> exclude_chainsaw = {
			weapon_group_1a.get<void>(-5),
			weapon_group_1b.get<void>(-5),
		};

		ReadCall(weapon_group_1a.get<void>(-5), GetWeaponInfo);

		// jnz -> ja
		Patch<uint8_t>(weapon_group_1a.get<void>(7), 0x77);
		Patch<uint8_t>(weapon_group_1b.get<void>(7 + 1), 0x87);

		Nop(slow_car_down_for_peds.get<void>(), 1);
		InjectHook(slow_car_down_for_peds.get<void>(1), &CheckWeaponGroupHook, HookType::Call);
		InjectHook(slow_car_down_for_peds.get<void>(8), else_jump, HookType::Jump);

		HookEach_ExcludeChainsaw(exclude_chainsaw, InterceptCall);
	}
	TXN_CATCH();


	// Fix the screwdriver not making sounds on impact
	try
	{
		void** pedAttackJumpTable = *get_pattern<void**>("83 F8 05 77 77 FF 24 85", 5 + 3);
		// Only make changes if the table hasn't been relocated
		if (hGameModule == ModCompat::Utils::GetModuleHandleFromAddress(pedAttackJumpTable))
		{
			// Give ASSOCGRP_SCREWDRIVER the same case as ASSOCGRP_KNIFE and others
			pedAttackJumpTable[1] = pedAttackJumpTable[2];
		}
	}
	TXN_CATCH();


	// Allow the tear gas to damage anyone (including the player), like on PS2
	try
	{
		auto set_peds_choking = get_pattern("0F 84 ? ? ? ? 8D 4B 34 D9 41 08");
		Nop(set_peds_choking, 6);
	}
	TXN_CATCH();


	// Fix an incorrect vertex setup for the outline of a destination blip in the Map Legend
	try
	{
		using namespace LegendBlipFix;

		auto draw2dPolygon = get_pattern("E8 ? ? ? ? D9 EE D9 EE D9 EE DB 05 ? ? ? ? 89 5C 24 24");
		InterceptCall(draw2dPolygon, orgDraw2DPolygon, Draw2DPolygon_FixVertices);
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
			if ( version == 0 ) Patch_VC_10(width, height);
			else if ( version == 1 ) Patch_VC_11(width, height);
			else if ( version == 2 ) Patch_VC_Steam(width, height);

			// Y axis sensitivity only
			else if (*(DWORD*)Memory::DynBaseAddress(0x601048) == 0x5E5F5D60) Patch_VC_JP();

			Patch_VC_Common();
			Common::Patches::III_VC_Common();
			Common::Patches::DDraw_Common();

			Common::Patches::III_VC_SetDelayedPatchesFunc( InjectDelayedPatches_VC_Common );
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
