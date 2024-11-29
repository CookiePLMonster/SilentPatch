#include "StdAfxSA.h"
#include <limits>
#include <algorithm>
#include <array>
#include <d3d9.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <cinttypes>

#include "ScriptSA.h"
#include "GeneralSA.h"
#include "ModelInfoSA.h"
#include "VehicleSA.h"
#include "PedSA.h"
#include "AudioHardwareSA.h"
#include "LinkListSA.h"
#include "PNGFile.h"
#include "PlayerInfoSA.h"
#include "FireManagerSA.h"
#include "Random.h"

#include "WaveDecoderSA.h"
#include "FLACDecoderSA.h"

#include "Utils/Patterns.h"
#include "Utils/DelimStringReader.h"
#include "Utils/ModuleList.hpp"
#include "Utils/ScopedUnprotect.hpp"
#include "Utils/HookEach.hpp"

#include "Desktop.h"
#include "FriendlyMonitorNames.h"
#include "SVF.h"

#include "debugmenu_public.h"
#include "resource.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' \
		name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
		processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

// ============= Mod compatibility stuff =============

namespace ModCompat
{
	bool SkygfxPatchesMoonphases( HMODULE module )
	{
		if ( module == nullptr ) return false; // SkyGfx not installed

		struct Config
		{
			uint32_t version;
			// The rest isn't relevant at the moment
		};

		auto func = (Config*(*)())GetProcAddress( module, "GetConfig" );
		if ( func == nullptr ) return false; // Old version?

		const Config* config = func();
		if ( config == nullptr ) return false; // Old version/error?

		constexpr uint32_t SKYGFX_VERSION_WITH_MOONPHASES = 0x360;
		return config->version >= SKYGFX_VERSION_WITH_MOONPHASES;
	}

	bool bCdStreamFallBackForOldML = false;
	bool ModloaderCdStreamRaceConditionAware( HMODULE module )
	{
		if ( module == nullptr ) return false; // modloader not installed

		HMODULE stdStreamModule;
		if ( GetModuleHandleEx( 0, TEXT("std.stream.dll"), &stdStreamModule ) == 0 ) return false; // std.data not loaded

		// ML is installed, so if it's an old version we need to fall back to a less safe implementation (no condition variables)
		bCdStreamFallBackForOldML = true;

		bool aware = false;
		const auto func = (uint32_t(*)())GetProcAddress( stdStreamModule, "CdStreamRaceConditionAware" );
		if ( func != nullptr )
		{
			aware = func() >= 1;
		}
		FreeLibrary( stdStreamModule );
		return aware;
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

		// Resolves a re-route if it comes from a no-CD executable
		uintptr_t GetFunctionAddrIfRerouted(uintptr_t address)
		{
			if (*reinterpret_cast<const uint8_t*>(address) == 0xE9)
			{
				uintptr_t jumpDestination;
				Memory::ReadCall(address, jumpDestination);
				if (GetModuleHandleFromAddress(address) == GetModuleHandleFromAddress(jumpDestination))
				{
					return jumpDestination;
				}
			}
			return address;
		}
	}
}

#pragma warning(disable:4733)

// RW wrappers
static void* varAtomicDefaultRenderCallBack = AddressByVersion<void*>(0x7491C0, 0x749AD0, 0x783180);
WRAPPER RpAtomic* AtomicDefaultRenderCallBack(RpAtomic* atomic) { WRAPARG(atomic); VARJMP(varAtomicDefaultRenderCallBack); }
static void* varRtPNGImageRead = AddressByVersion<void*>(0x7CF9B0, 0x7D02B0, 0x809970);
WRAPPER RwImage* RtPNGImageRead(const RwChar* imageName) { WRAPARG(imageName); VARJMP(varRtPNGImageRead); }
static void* varRwTextureCreate = AddressByVersion<void*>(0x7F37C0, 0x7F40C0, 0x82D780);
WRAPPER RwTexture* RwTextureCreate(RwRaster* raster) { WRAPARG(raster); VARJMP(varRwTextureCreate); }
static void* varRwRasterCreate = AddressByVersion<void*>(0x7FB230, 0x7FBB30, 0x8351F0, { "8B 0D ? ? ? ? 56 68 07 04 03 00 8B 54 01 60", -5 });
WRAPPER RwRaster* RwRasterCreate(RwInt32 width, RwInt32 height, RwInt32 depth, RwInt32 flags) { WRAPARG(width); WRAPARG(height); WRAPARG(depth); WRAPARG(flags); VARJMP(varRwRasterCreate); }
static void* varRwImageDestroy = AddressByVersion<void*>(0x802740, 0x803040, 0x83C700);
WRAPPER RwBool RwImageDestroy(RwImage* image) { WRAPARG(image); VARJMP(varRwImageDestroy); }
static void* varRpMaterialSetTexture = AddressByVersion<void*>(0x74DBC0, 0x74E4D0, 0x787B80);
WRAPPER RpMaterial* RpMaterialSetTexture(RpMaterial* material, RwTexture* texture) { VARJMP(varRpMaterialSetTexture); }
static void* varRwFrameGetLTM = AddressByVersion<void*>(0x7F0990, 0x7F1290, 0x82A950);
WRAPPER RwMatrix* RwFrameGetLTM(RwFrame* frame) { VARJMP(varRwFrameGetLTM); }
static void* varRwMatrixRotate = AddressByVersion<void*>(0x7F1FD0, 0x7F28D0, 0x82BF90);
WRAPPER RwMatrix* RwMatrixRotate(RwMatrix* matrix, const RwV3d* axis, RwReal angle, RwOpCombineType combineOp) { WRAPARG(matrix); WRAPARG(axis); WRAPARG(angle); WRAPARG(combineOp); VARJMP(varRwMatrixRotate); }
static void* varRwD3D9SetRenderState = AddressByVersion<void*>(0x7FC2D0, 0x7FCBD0, 0x836290);
WRAPPER void RwD3D9SetRenderState(RwUInt32 state, RwUInt32 value) { WRAPARG(state); WRAPARG(value); VARJMP(varRwD3D9SetRenderState); }
static void* varRwEngineSetSubSystem = AddressByVersion<void*>(0x7F2C90, { "50 6A 00 6A 00 83 C1 10 6A 10 51", -0xA });
WRAPPER RwBool RwEngineSetSubSystem(RwInt32 subSystemIndex) { WRAPARG(subSystemIndex); VARJMP(varRwEngineSetSubSystem); }

RwCamera* RwCameraBeginUpdate(RwCamera* camera)
{
	return camera->beginUpdate(camera);
}

RwCamera* RwCameraEndUpdate(RwCamera* camera)
{
	return camera->endUpdate(camera);
}

RwCamera* RwCameraClear(RwCamera* camera, RwRGBA* colour, RwInt32 clearMode)
{
	return RWSRCGLOBAL(stdFunc[rwSTANDARDCAMERACLEAR])(camera, colour, clearMode) != FALSE ? camera : NULL;
}

RwMatrix* RwMatrixTranslate(RwMatrix* matrix, const RwV3d* translation, RwOpCombineType combineOp)
{
	if ( combineOp == rwCOMBINEREPLACE )
	{
		RwMatrixSetIdentity(matrix);
		matrix->pos = *translation;
	}
	else if ( combineOp == rwCOMBINEPRECONCAT )
	{
		matrix->pos.x += matrix->at.x * translation->z + matrix->up.x * translation->y + matrix->right.x * translation->x;
		matrix->pos.y += matrix->at.y * translation->z + matrix->up.y * translation->y + matrix->right.y * translation->x;
		matrix->pos.z += matrix->at.z * translation->z + matrix->up.z * translation->y + matrix->right.z * translation->x;	
	}
	else if ( combineOp == rwCOMBINEPOSTCONCAT )
	{
		matrix->pos.x += translation->x;
		matrix->pos.y += translation->y;
		matrix->pos.z += translation->z;
	}
	rwMatrixSetFlags(matrix, rwMatrixGetFlags(matrix) & ~(rwMATRIXINTERNALIDENTITY));
	return matrix;
}

RwFrame* RwFrameForAllChildren(RwFrame* frame, RwFrameCallBack callBack, void* data)
{
	for ( RwFrame* curFrame = frame->child; curFrame != nullptr; curFrame = curFrame->next )
	{
		if ( callBack(curFrame, data) == NULL )
			break;
	}
	return frame;
}

RwFrame* RwFrameForAllObjects(RwFrame* frame, RwObjectCallBack callBack, void* data)
{
	for ( RwLLLink* link = rwLinkListGetFirstLLLink(&frame->objectList); link != rwLinkListGetTerminator(&frame->objectList); link = rwLLLinkGetNext(link) )
	{
		if ( callBack(&rwLLLinkGetData(link, RwObjectHasFrame, lFrame)->object, data) == NULL )
			break;
	}

	return frame;
}

RwFrame* RwFrameUpdateObjects(RwFrame* frame)
{
	if ( !rwObjectTestPrivateFlags(&frame->root->object, rwFRAMEPRIVATEHIERARCHYSYNCLTM|rwFRAMEPRIVATEHIERARCHYSYNCOBJ) )
		rwLinkListAddLLLink(&RWSRCGLOBAL(dirtyFrameList), &frame->root->inDirtyListLink);

	rwObjectSetPrivateFlags(&frame->root->object, rwObjectGetPrivateFlags(&frame->root->object) | (rwFRAMEPRIVATEHIERARCHYSYNCLTM|rwFRAMEPRIVATEHIERARCHYSYNCOBJ));
	rwObjectSetPrivateFlags(&frame->object, rwObjectGetPrivateFlags(&frame->object) | (rwFRAMEPRIVATESUBTREESYNCLTM|rwFRAMEPRIVATESUBTREESYNCOBJ));
	return frame;
}

RwMatrix* RwMatrixUpdate(RwMatrix* matrix)
{
	matrix->flags &= ~(rwMATRIXTYPEMASK|rwMATRIXINTERNALIDENTITY);
	return matrix;
}

RwRaster* RwRasterSetFromImage(RwRaster* raster, RwImage* image)
{
	if ( RWSRCGLOBAL(stdFunc[rwSTANDARDRASTERSETIMAGE])(raster, image, 0) != FALSE )
	{
		if ( image->flags & rwIMAGEGAMMACORRECTED )
			raster->privateFlags |= rwRASTERGAMMACORRECTED;
		return raster;
	}
	return NULL;
}

RwImage* RwImageFindRasterFormat(RwImage* ipImage, RwInt32 nRasterType, RwInt32* npWidth, RwInt32* npHeight, RwInt32* npDepth, RwInt32* npFormat)
{
	RwRaster	outRaster;
	if ( RWSRCGLOBAL(stdFunc[rwSTANDARDIMAGEFINDRASTERFORMAT])(&outRaster, ipImage, nRasterType) != FALSE )
	{
		*npFormat = RwRasterGetFormat(&outRaster) | outRaster.cType;
		*npWidth = RwRasterGetWidth(&outRaster);
		*npHeight = RwRasterGetHeight(&outRaster);
		*npDepth = RwRasterGetDepth(&outRaster);
		return ipImage;
	}
	return NULL;
}

RpClump* RpClumpForAllAtomics(RpClump* clump, RpAtomicCallBack callback, void* pData)
{
	for ( RwLLLink* link = rwLinkListGetFirstLLLink(&clump->atomicList); link != rwLinkListGetTerminator(&clump->atomicList); link = rwLLLinkGetNext(link) )
	{
		if ( callback(rwLLLinkGetData(link, RpAtomic, inClumpLink), pData) == NULL )
			break;
	}
	return clump;
}

RpClump* RpClumpRender(RpClump* clump)
{
	RpClump*	retClump = clump;

	for ( RwLLLink* link = rwLinkListGetFirstLLLink(&clump->atomicList); link != rwLinkListGetTerminator(&clump->atomicList); link = rwLLLinkGetNext(link) )
	{
		RpAtomic* curAtomic = rwLLLinkGetData(link, RpAtomic, inClumpLink);
		if ( RpAtomicGetFlags(curAtomic) & rpATOMICRENDER )
		{
			// Not sure why they need this
			RwFrameGetLTM(RpAtomicGetFrame(curAtomic));
			if ( RpAtomicRender(curAtomic) == NULL )
				retClump = NULL;
		}
	}
	return retClump;
}

RpGeometry* RpGeometryForAllMaterials(RpGeometry* geometry, RpMaterialCallBack fpCallBack, void* pData)
{
	for ( RwInt32 i = 0, j = geometry->matList.numMaterials; i < j; i++ )
	{
		if ( fpCallBack(geometry->matList.materials[i], pData) == NULL )
			break;
	}
	return geometry;
}

RwInt32 RpHAnimIDGetIndex(RpHAnimHierarchy* hierarchy, RwInt32 ID)
{
	for ( RwInt32 i = 0, j = hierarchy->numNodes; i < j; i++ )
	{
		if ( ID == hierarchy->pNodeInfo[i].nodeID )
			return i;
	}
	return -1;
}

RwMatrix* RpHAnimHierarchyGetMatrixArray(RpHAnimHierarchy* hierarchy)
{
	return hierarchy->pMatrixArray;
}

void RwD3D9DeleteVertexShader(void* shader)
{
	static_cast<IUnknown*>(shader)->Release();
}

RwBool _rpD3D9VertexDeclarationInstColor(RwUInt8* mem, const RwRGBA* color, RwInt32 numVerts, RwUInt32 stride)
{
	RwUInt8 alpha = 255;
	for ( RwInt32 i = 0; i < numVerts; i++ )
	{
		*reinterpret_cast<RwUInt32*>(mem) = (color->alpha << 24) | (color->red << 16) | (color->green << 8) | color->blue;
		alpha &= color->alpha;
		color++;
		mem += stride;
	}
	return alpha != 255;
}

// Unreachable stub
RwBool RwMatrixDestroy(RwMatrix* mpMat) { assert(!"Unreachable!"); return TRUE; }

struct AlphaObjectInfo
{
	RpAtomic*	pAtomic;
	RpAtomic*	(*callback)(RpAtomic*, float);
	float		fCompareValue;

	friend bool operator < (const AlphaObjectInfo &a, const AlphaObjectInfo &b)
	{ return a.fCompareValue < b.fCompareValue; }
};

struct PsGlobalType;

struct RsGlobalType
{
	const char*		AppName;
	signed int		MaximumWidth;
	signed int		MaximumHeight;
	unsigned int	frameLimit;
	BOOL			quit;
	PsGlobalType*	ps;
	void*			keyboard;
	void*			mouse;
	void*			pad;
};

// Other wrappers
void					(*GTAdelete)(void*) = AddressByVersion<void(*)(void*)>(0x82413F, 0x824EFF, 0x85E58C);
const char*				(*GetFrameNodeName)(RwFrame*) = AddressByVersion<const char*(*)(RwFrame*)>(0x72FB30, 0x730360, 0x769C20);
RpHAnimHierarchy*		(*GetAnimHierarchyFromSkinClump)(RpClump*) = AddressByVersion<RpHAnimHierarchy*(*)(RpClump*)>(0x734A40, 0x735270, 0x7671B0);	
auto					InitializeUtrax = AddressByVersion<void(__thiscall*)(void*)>(0x4F35B0, 0x4F3A10, 0x4FFA80);
auto					RpAnimBlendClumpGetAssociation = AddressByVersion<void*(*)(RpClump*, uint32_t)>(0x4D68B0, { "8B 0D ? ? ? ? 8B 14 01 8B 02 85 C0 74 11 8B 4D 0C", -6 });
auto					GetAnimationBlockIndex = AddressByVersion<int32_t(*)(const char* animBlock)>(0x4D3990, { "83 C4 04 85 C0 75 05", -0xC });
auto					RequestModel = AddressByVersion<void(*)(int modelID, int priority)>(0x4087E0, { "57 8D 3C 9B", -0x8 });
auto					LoadAllRequestedModels = AddressByVersion<void(*)(bool bBlock)>(0x40EA10, { "A1 ? ? ? ? 03 C0", -0x20 });

auto					IsPlayerOnAMission = AddressByVersion<bool(*)()>(0x464D50, {"85 C0 74 0C 83 B8 ? ? ? ? ? 75 03 B0 01 C3", -5});

static void				(__thiscall* SetVolume)(void*,float);	
static BOOL				(*IsAlreadyRunning)();
static void				(*TheScriptsLoad)();
static void				(*DoSunAndMoon)();

auto 					WorldRemove = AddressByVersion<void(*)(CEntity*)>(0x563280, 0, 0x57D370, { "8B 06 8B 50 0C 8B CE FF D2 8A 46 36 24 07 3C 01 76 0D", -7 });


// SA variables
void**					rwengine = *AddressByVersion<void***>(0x58FFC0, 0x53F032, 0x48C194, { "8B 48 20 53 56 57 6A 01", -5 + 1 });

RsGlobalType*			RsGlobal = *AddressByVersion<RsGlobalType**>(0x619602 + 2, { "33 C0 C7 05 ? ? ? ? ? ? ? ? C7 05", 2 + 2 });

unsigned char&			nGameClockDays = **AddressByVersion<unsigned char**>(0x4E841D, 0x4E886D, 0x4F3871);
unsigned char&			nGameClockMonths = **AddressByVersion<unsigned char**>(0x4E842D, 0x4E887D, 0x4F3861);
void*&					pUserTracksStuff = **AddressByVersion<void***>(0x4D9B7B, 0x4DA06C, 0x4E4A43);

float&					fFarClipZ = **AddressByVersion<float**>(0x70D21F, 0x70DA4F, 0x421AB2);

CZoneInfo*&				pCurrZoneInfo = **AddressByVersion<CZoneInfo***>(0x58ADB1, 0x58B581, 0x407F93);
CRGBA*					HudColour = *AddressByVersion<CRGBA**>(0x58ADF6, 0x58B5C6, 0x440648);

CLinkListSA<CPed*>&			ms_weaponPedsForPC = **AddressByVersion<CLinkListSA<CPed*>**>(0x53EACA, 0x53EF6A, 0x551101);
CLinkListSA<AlphaObjectInfo>&	m_alphaList = **AddressByVersion<CLinkListSA<AlphaObjectInfo>**>(0x733A4D, 0x73427D, 0x76DCA3);

uint32_t&				bDrawCrossHair = **AddressByVersion<uint32_t**>(0x58E7BF + 2, {"83 3D ? ? ? ? ? 74 29", 2});

DebugMenuAPI gDebugMenuAPI;

// Custom variables
static float		fSunFarClip;
static struct
{
	char			Extension[8];
	unsigned int	Codec;
} UserTrackExtensions[] = { { ".ogg", DECODER_VORBIS }, { ".mp3", DECODER_QUICKTIME },
							{ ".wav", DECODER_WAVE }, { ".wma", DECODER_WINDOWSMEDIA },
							{ ".wmv", DECODER_WINDOWSMEDIA }, { ".aac", DECODER_QUICKTIME },
							{ ".m4a", DECODER_QUICKTIME }, { ".mov", DECODER_QUICKTIME },
							{ ".fla", DECODER_FLAC }, { ".flac", DECODER_FLAC } };

static bool IgnoresWeaponPedsForPCFix();

// Regular functions
static RpAtomic* RenderAtomic(RpAtomic* pAtomic, float fComp)
{
	UNREFERENCED_PARAMETER(fComp);
	return AtomicDefaultRenderCallBack(pAtomic);
}

static RpAtomic* StaticPropellerRender(RpAtomic* pAtomic)
{
	RwScopedRenderState<rwRENDERSTATEALPHATESTFUNCTIONREF> alphaRef;

	RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, 0);
	pAtomic = AtomicDefaultRenderCallBack(pAtomic);

	return pAtomic;
}

static RpAtomic* MovingPropellerRender(RpAtomic* pAtomic)
{
	RwScopedRenderState<rwRENDERSTATEALPHATESTFUNCTIONREF> alphaRef;
	RwScopedRenderState<rwRENDERSTATEVERTEXALPHAENABLE> vertexAlpha;

	RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, 0);
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, reinterpret_cast<void*>(TRUE));
	pAtomic = AtomicDefaultRenderCallBack(pAtomic);

	return pAtomic;
}

RpAtomic* RenderBigVehicleActomic(RpAtomic* pAtomic, float)
{
	const char*		pNodeName = GetFrameNodeName(RpAtomicGetFrame(pAtomic));

	if ( strncmp(pNodeName, "moving_prop", 11) == 0 )
		return MovingPropellerRender(pAtomic);

	if ( strncmp(pNodeName, "static_prop", 11) == 0 )
		return StaticPropellerRender(pAtomic);

	return AtomicDefaultRenderCallBack(pAtomic);
}

void RenderVehicleHiDetailAlphaCB_HunterDoor(RpAtomic* pAtomic)
{
	AlphaObjectInfo		NewObject;

	NewObject.callback = RenderAtomic;
	NewObject.fCompareValue = -std::numeric_limits<float>::infinity();
	NewObject.pAtomic = pAtomic;

	m_alphaList.InsertFront(NewObject);
}

void RenderWeapon(CPed* pPed)
{
	if ( !IgnoresWeaponPedsForPCFix() )
	{
		pPed->RenderWeapon(true, false, false);
	}
	ms_weaponPedsForPC.Insert(pPed);
}

void RenderWeaponPedsForPC()
{
	RwScopedRenderState<rwRENDERSTATEVERTEXALPHAENABLE> vertexAlpha;
	RwScopedRenderState<rwRENDERSTATEZWRITEENABLE> zWrite;
	RwScopedRenderState<rwRENDERSTATEFOGENABLE> fogEnable;

	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, reinterpret_cast<void*>(TRUE));
	RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, reinterpret_cast<void*>(TRUE));
	RwRenderStateSet(rwRENDERSTATEFOGENABLE, reinterpret_cast<void*>(TRUE));

	const bool renderWeapon = IgnoresWeaponPedsForPCFix();

	for ( auto it = ms_weaponPedsForPC.Next( nullptr ); it != nullptr; it = ms_weaponPedsForPC.Next( it ) )
	{
		CPed* ped = **it;
		ped->SetupLighting();
		ped->RenderWeapon(renderWeapon, true, false);
		ped->RemoveLighting();
	}
}

static CAEFLACDecoder* __stdcall DecoderCtor(CAEDataStream* pData)
{
	return new CAEFLACDecoder(pData);
}

static CAEWaveDecoder* __stdcall CAEWaveDecoderInit(CAEDataStream* pStream)
{
	return new CAEWaveDecoder(pStream);
}

namespace ScalingInternals
{
	// 1.0 - fast math uses a scaling multiplier
	float ScaleX_Multiplier(float val)
	{
		static float** ResolutionWidthMult = (float**)(0x5733FD + 2);
		return val * RsGlobal->MaximumWidth * **ResolutionWidthMult;
	}

	float ScaleY_Multiplier(float val)
	{
		static float** ResolutionHeightMult = (float**)(0x57342D + 2);
		return val * RsGlobal->MaximumHeight * **ResolutionHeightMult;
	}

	// New binaries - precise math uses a scaling divisor
	float ScaleX_Divisor(float val)
	{
		static double** ResolutionWidthDiv = hook::get_pattern<double*>("DC 35 ? ? ? ? DC 0D ? ? ? ? DE E9 D9 5D F0", 2);
		return static_cast<float>(val * RsGlobal->MaximumWidth / **ResolutionWidthDiv);
	}

	float ScaleY_Divisor(float val)
	{
		static double** ResolutionHeightDiv = hook::get_pattern<double*>("50 DC 35 ? ? ? ? DC 0D", 1 + 2);
		return static_cast<float>(val * RsGlobal->MaximumHeight / **ResolutionHeightDiv);
	}
}

// Default to 1.0, update these pointers with a pointer to the new binaries function if needed
auto ScaleX = &ScalingInternals::ScaleX_Multiplier;
auto ScaleY = &ScalingInternals::ScaleY_Multiplier;

namespace ScriptFixes
{

static void BasketballFix(unsigned char* pBuf, int nSize)
{
	for ( int i = 0, hits = 0; i < nSize && hits < 7; i++, pBuf++ )
	{
		// Pattern check for save pickup XYZ
		if ( *(unsigned int*)pBuf == 0x449DE19A )		// Save pickup X
		{
			hits++;
			*(float*)pBuf = 1291.8f;
		}
		else if ( *(unsigned int*)pBuf == 0xC4416AE1 )		// Save pickup Y
		{
			hits++;
			*(float*)pBuf = -797.8284f;
		}
		else if ( *(unsigned int*)pBuf == 0x44886C7B )		// Save pickup Z
		{
			hits++;
			*(float*)pBuf = 1089.5f;
		}
		else if ( *(unsigned int*)pBuf == 0x449DF852 )		// Save point X
		{
			hits++;
			*(float*)pBuf = 1286.8f;
		}
		else if ( *(unsigned int*)pBuf == 0xC44225C3 )		// Save point Y
		{
			hits++;
			*(float*)pBuf = -797.69f;
		}
		else if ( *(unsigned int*)pBuf == 0x44885C7B )		// Save point Z
		{
			hits++;
			*(float*)pBuf = 1089.1f;
		}
		else if ( *(unsigned int*)pBuf == 0x43373AE1 )		// Save point A
		{
			hits++;
			*(float*)pBuf = 90.0f;
		}
	}
}

static unsigned char*	ScriptSpace;
static int*				ScriptParams;
static size_t			ScriptFileSize, ScriptMissionSize;

static void InitializeScriptGlobals()
{
	static bool		bInitScriptStuff = [] () {;
		ScriptSpace = *AddressByVersion<unsigned char**>(0x5D5380, 0x5D5B60, 0x450E34);
		ScriptParams = *AddressByVersion<int**>(0x48995B, 0x46410A, 0x46979A);
		ScriptFileSize = *AddressByVersion<size_t*>( 0x468E74+1, 0, 0x46E572+1);
		ScriptMissionSize = *AddressByVersion<size_t*>( 0x489A5A+1, 0, 0x490798+1);

		return true;
	} ();
}

static void SweetsGirlFix()
{
	// Changes @ == int to @ >= int in two places
	if ( *(uint16_t*)(ScriptSpace+ScriptFileSize+2510) == 0x0039 )
		*(uint16_t*)(ScriptSpace+ScriptFileSize+2510) = 0x0029;

	if ( *(uint16_t*)(ScriptSpace+ScriptFileSize+2680) == 0x0039 )
		*(uint16_t*)(ScriptSpace+ScriptFileSize+2680) = 0x0029;
}

static hook::pattern MakeScriptPattern(bool isMission, std::string_view bytes)
{
	uintptr_t begin, end;
	if (isMission)
	{
		begin = uintptr_t(ScriptSpace) + ScriptFileSize;
		end = begin + ScriptMissionSize;
	}
	else
	{
		begin = uintptr_t(ScriptSpace);
		end = begin + ScriptFileSize;
	}
	return hook::make_range_pattern(begin, end, bytes);
}

static void MountainCloudBoysFix()
{
	auto pattern = MakeScriptPattern(true, "D6 00 04 00 39 00 03 EF 00 04 02 4D 00 01 90 F2 FF FF D6 00 04 01");
	if ( pattern.size() == 1 ) // Faulty code lies under offset 3367 - replace it if it matches
	{
		const uint8_t bNewCode[22] = {
			0x00, 0x00, 0x00, 0x00, 0xD6, 0x00, 0x04, 0x03, 0x39, 0x00, 0x03, 0x2B,
			0x00, 0x04, 0x0B, 0x39, 0x00, 0x03, 0xEF, 0x00, 0x04, 0x02
		};
		memcpy( pattern.get(0).get<void>(), bNewCode, sizeof(bNewCode) );
	}
}

static void SupplyLinesFix( bool isBeefyBaron )
{
	auto pattern = MakeScriptPattern(true, isBeefyBaron ? "B8 9E 3A 44" : "B8 1E 2F 44");
	if ( pattern.size() == 1 ) // 700.48 -> 10.0 (teleports car with CJ under the building instead)
	{
		*pattern.get(0).get<float>() = 10.0f;
	}
}

static void DrivingSchoolConesFix()
{
	auto pattern = MakeScriptPattern(true, "04 00 02 20 03 04 00 D6 00 04 00 1A 00 04 2E 02 20 03 4D 00 01 60 75 FF FF BE 00 08 01 07 24 03 20 03 2E 80 08 00 02 20 03 04 01");
	auto coneCoilConeCount = MakeScriptPattern(true, "1A 00 04 17 02 20 03");
	auto burnAndLapConeCount = MakeScriptPattern(true, "1A 00 04 23 02 20 03");
	// Only destroy as many cones as were created, and correct trafficcone_counter for "Cone Coil" and "Burn and Lap"
	if (pattern.size() == 1 && coneCoilConeCount.size() == 1 && burnAndLapConeCount.size() == 1)
	{
		const uint8_t gotoSkipAssignment[] = { 0x02, 0x00, 0x01, 0x8B, 0x75, 0xFF, 0xFF };
		memcpy(pattern.get(0).get<void>(0), gotoSkipAssignment, sizeof(gotoSkipAssignment));

		const uint8_t cmpVal[] = { 0x18, 0x00, 0x02, 0x20, 0x03, 0x04, 0x00 }; // trafficcone_counter > 0
		memcpy(pattern.get(0).get<void>(11), cmpVal, sizeof(cmpVal));

		// trafficcone_counter-- \ DELETE_OBJECT trafficcones[trafficcone_counter]
		const uint8_t subValDeleteObject[] = { 0x0C, 0x00, 0x02, 0x20, 0x03, 0x04, 0x01, 0x08, 0x01, 0x07, 0x24, 0x03, 0x20, 0x03, 0x2E, 0x80 };
		memcpy(pattern.get(0).get<void>(0x1B), subValDeleteObject, sizeof(subValDeleteObject));

		// Also set trafficcone_counter to 0 so the first destruction doesn't happen
		int32_t* trafficcone_counter = reinterpret_cast<int32_t*>(ScriptSpace+800);
		*trafficcone_counter = 0;

		// Correct the final trafficcone_counter in Cone Coil
		// 23 -> 30
		*coneCoilConeCount.get(0).get<int8_t>(3) = 30;


		// Correct the final trafficcone_counter in Burn and Lap
		// 35 -> 42
		*burnAndLapConeCount.get(0).get<int8_t>(3) = 42;
	}
}

static void BikeSchoolConesFix()
{
	auto pattern = MakeScriptPattern(true, "04 00 02 20 03 04 00 D6 00 04 00 1A 00 04 2E 02 20 03 4D 00 01 F8 AD FF FF 08 01 07 24 03 20 03 2E 80 08 00 02 20 03 04 01");
	if (pattern.size() == 1) // Only destroy as many cones as were created
	{
		const uint8_t gotoSkipAssignment[] = { 0x02, 0x00, 0x01, 0x21, 0xAE, 0xFF, 0xFF };
		memcpy(pattern.get(0).get<void>(0), gotoSkipAssignment, sizeof(gotoSkipAssignment));

		const uint8_t cmpVal[] = { 0x18, 0x00, 0x02, 0x20, 0x03, 0x04, 0x00 }; // trafficcone_counter > 0
		memcpy(pattern.get(0).get<void>(11), cmpVal, sizeof(cmpVal));

		// trafficcone_counter-- \ DELETE_OBJECT trafficcones[trafficcone_counter]
		const uint8_t subValDeleteObject[] = { 0x0C, 0x00, 0x02, 0x20, 0x03, 0x04, 0x01, 0x08, 0x01, 0x07, 0x24, 0x03, 0x20, 0x03, 0x2E, 0x80 };
		memcpy(pattern.get(0).get<void>(0x19), subValDeleteObject, sizeof(subValDeleteObject));

		// Also set trafficcone_counter to 0 so the first destruction doesn't happen
		int32_t* trafficcone_counter = reinterpret_cast<int32_t*>(ScriptSpace+800);
		*trafficcone_counter = 0;
	}
}

static void AirRaidFix()
{
	// Give the player back their weapon from the 8th slot instead of stealing it,
	// but do it properly and load the model before giving the wepaon.
	// 1.01 PC script saves and restores the weapon, but forgets to load the model.
	auto save_weapon_gosub_place = MakeScriptPattern(true, "BD 01 03 65 01 06 00 03 48 01 04 00");
	auto free_space_after_mission = MakeScriptPattern(true, "EF 04 0E 06 43 41 53 49 4E 4F EF 04 0E 0A 4F 4E 5F 4C 4F 4F 4B 45 52 53 D8 00 51 00");
	if (save_weapon_gosub_place.size() == 1 && free_space_after_mission.size() == 1)
	{
		using namespace Memory;

		// "Assemble" the script bytecode because it's more readable this way
		uintptr_t afterMissionSpace = free_space_after_mission.get(0).get_uintptr(28);

		auto assembleCommand = [](uintptr_t& mem, std::initializer_list<uint8_t> bytes)
			{
				uint8_t* buf = reinterpret_cast<uint8_t*>(mem);
				std::copy(bytes.begin(), bytes.end(), buf);
				mem += bytes.size();
			};

		auto assembleMissionOffset = [](uintptr_t& mem, uintptr_t dest)
			{
				const int32_t offset = (uintptr_t(ScriptSpace) + ScriptFileSize) - dest;
				memcpy(reinterpret_cast<uint8_t*>(mem), &offset, sizeof(offset));
				mem += sizeof(offset);
			};

		auto assembleInt32 = [](uintptr_t& mem, int32_t val)
			{
				memcpy(reinterpret_cast<uint8_t*>(mem), &val, sizeof(val));
				mem += sizeof(val);
			};

		// Store the weapon
		{
			// GOSUB zero1_store_weapon
			uintptr_t missionInitSpace = save_weapon_gosub_place.get(0).get_uintptr(5);
			assembleCommand(missionInitSpace, { 0x50, 0x00, 0x01 });
			assembleMissionOffset(missionInitSpace, afterMissionSpace);

			// zero1_store_weapon:
			assembleCommand(afterMissionSpace, { 0x06, 0x00, 0x03, 0x48, 0x01, 0x04, 0x00 }); // index_zero1 = 0
			assembleCommand(afterMissionSpace, { 0xB8, 0x04, 0x02, 0x0C, 0x00, 0x04, 0x08, 0x03, 0x72, 0x01, 0x03, 0x73, 0x01, 0x03, 0x74, 0x01}); // GET_CHAR_WEAPON_IN_SLOT scplayer 8 weapontype_zero1 ammo_zero1 model_for_weapon_zero1
			assembleCommand(afterMissionSpace, { 0x51, 0x00 }); // RETURN
		}

		// Restore the weapon
		{
			uintptr_t missionCleanupGosub = uintptr_t(ScriptSpace) + ScriptFileSize + 0x1E;
			const int32_t originalMissionCleanup = *reinterpret_cast<int32_t*>(missionCleanupGosub);
			assembleMissionOffset(missionCleanupGosub, afterMissionSpace);

			assembleCommand(afterMissionSpace, { 0x12, 0x81 }); // NOT HAS_DEATHARREST_BEEN_EXECUTED
			assembleCommand(afterMissionSpace, { 0x4D, 0x00, 0x01 }); // GOTO_IF_FALSE originalMissionCleanup
			// We can jupm directly back to the original mission cleanup to simplify code generation
			assembleInt32(afterMissionSpace, originalMissionCleanup);

			// The original fix from 1.01/2.0 PC versions only added GIVE_WEAPON_TO_CHAR, without loading the model
			// We fix it properly by ensuring the model is loaded before giving the weapon
			assembleCommand(afterMissionSpace, { 0x47, 0x02, 0x03, 0x74, 0x01 }); // REQUEST_MODEL model_for_weapon_zero1
			assembleCommand(afterMissionSpace, { 0x8B, 0x03 }); // LOAD_ALL_MODELS_NOW
			assembleCommand(afterMissionSpace, { 0xB2, 0x01, 0x02, 0x0C, 0x00, 0x03, 0x72, 0x01, 0x03, 0x73, 0x01 });  // GIVE_WEAPON_TO_CHAR scplayer weapontype_zero1 ammo_zero1
			assembleCommand(afterMissionSpace, { 0x49, 0x02, 0x03, 0x74, 0x01 }); // MARK_MODEL_AS_NO_LONGER_NEEDED model_for_weapon_zero1
			
			assembleCommand(afterMissionSpace, { 0x02, 0x00, 0x01 }); // GOTO originalMissionCleanup
			assembleInt32(afterMissionSpace, originalMissionCleanup);
		}
	}
}

static void QuadrupleStuntBonus()
{
	// IF HEIGHT_FLOAT_HJ > 4.0 -> IF HEIGHT_INT_HJ > 4
	auto pattern = MakeScriptPattern(false, "20 00 02 60 14 06 00 00 80 40");
	if ( pattern.size() == 1 )
	{
		const uint8_t newCode[10] = {
			0x18, 0x00, 0x02, 0x30, 0x14, 0x01, 0x04, 0x00, 0x00, 0x00
		};
		memcpy( pattern.get(0).get<void>(), newCode, sizeof(newCode) );
	}
}

void TheScriptsLoad_BasketballFix()
{
	TheScriptsLoad();
	InitializeScriptGlobals();

	BasketballFix(ScriptSpace+8, *(int*)(ScriptSpace+3));
	QuadrupleStuntBonus();
}

static void StartNewMission_SCMFixes()
{
	InitializeScriptGlobals();

	const int missionID = ScriptParams[0];
	switch (missionID)
	{
	// INITIAL - Basketball fix, Quadruple Stunt Bonus
	case 0:
		BasketballFix(ScriptSpace+ScriptFileSize, ScriptMissionSize);
		QuadrupleStuntBonus();
		break;
	// HOODS5 - Sweet's Girl fix
	case 18:
		SweetsGirlFix();
		break;
	// WUZI1 - Mountain Cloud Boys fix
	case 53:
		MountainCloudBoysFix();
		break;
	// DSKOOL - Driving School cones fix
	// By Wesser
	case 71:
		DrivingSchoolConesFix();
		break;
	// BSKOOL - Bike School cones fix
	// By Wesser
	case 120:
		BikeSchoolConesFix();
		break;
	// ZERO1 - Air Raid fix
	case 72:
		AirRaidFix();
		break;
	// ZERO2 - Supply Lines fix
	case 73:
		SupplyLinesFix( false );
		break;
	// ZERO5 - Beefy Baron fix
	case 10:
		SupplyLinesFix( true );
		break;
	default:
		break;
	}
}

template<std::size_t Index>
static void (*orgWipeLocalVariableMemoryForMissionScript)();

template<std::size_t Index>
static void WipeLocalVariableMemoryForMissionScript_ApplyFixes()
{
	orgWipeLocalVariableMemoryForMissionScript<Index>();
	StartNewMission_SCMFixes();
}

HOOK_EACH_INIT(SCMFixes, orgWipeLocalVariableMemoryForMissionScript, WipeLocalVariableMemoryForMissionScript_ApplyFixes)

}

// 1.01 kinda fixed it
bool GetCurrentZoneLockedOrUnlocked(float fPosX, float fPosY)
{
	// Exploit RAII really bad
	static const float		GridXOffset = **(float**)(0x572135+2), GridYOffset = **(float**)(0x57214A+2);
	static const float		GridXSize = **(float**)(0x57213B+2), GridYSize = **(float**)(0x572153+2);
	static const int		GridXNum = static_cast<int>((2.0f*GridXOffset) * GridXSize), GridYNum = static_cast<int>((2.0f*GridYOffset) * GridYSize);

	static unsigned char* const	ZonesVisited = *(unsigned char**)(0x57216A) - (GridYNum-1);		// 1.01 fixed it!

	int		Xindex = static_cast<int>((fPosX+GridXOffset) * GridXSize);
	int		Yindex = static_cast<int>((fPosY+GridYOffset) * GridYSize);

	// "Territories fix"
	if ( (Xindex >= 0 && Xindex < GridXNum) && (Yindex >= 0 && Yindex < GridYNum) )
		return ZonesVisited[GridXNum*Xindex - Yindex + (GridYNum-1)] != 0;
	
	// Outside of map bounds
	return true;
}

bool GetCurrentZoneLockedOrUnlocked_Steam(float fPosX, float fPosY)
{
	static unsigned char* const	ZonesVisited = *(unsigned char**)(0x5870E8) - 9;

	int		Xindex = static_cast<int>((fPosX+3000.0f) / 600.0f);
	int		Yindex = static_cast<int>((fPosY+3000.0f) / 600.0f);

	// "Territories fix"
	if ( (Xindex >= 0 && Xindex < 10) && (Yindex >= 0 && Yindex < 10) )
		return ZonesVisited[10*Xindex - Yindex + 9] != 0;

	// Outside of map bounds
	return true;
}

CRGBA* __fastcall BlendGangColour(CRGBA* pThis, void*, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	const double colourIntensity = std::min( static_cast<double>(pCurrZoneInfo->ZoneColour.a) / 120.0, 1.0 );
	*pThis = CRGBA(BlendSqr( HudColour[3], CRGBA(r, g, b), colourIntensity ), a);
	return pThis;
}

static bool bColouredZoneNames;
CRGBA* __fastcall BlendGangColour_Dynamic(CRGBA* pThis, void*, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	if ( bColouredZoneNames )
	{
		return BlendGangColour(pThis, nullptr, r, g, b, a);
	}
	*pThis = CRGBA(HudColour[3], a);
	return pThis;
}

void SunAndMoonFarClip()
{
	fSunFarClip = std::min(1500.0f, fFarClipZ);
	DoSunAndMoon();
}

// STEAM ONLY
template<bool bX1, bool bY1, bool bX2, bool bY2>
void DrawRect_HalfPixel_Steam(CRect& rect, const CRGBA& rgba)
{
	if constexpr ( bX1 )
		rect.x1 -= 0.5f;

	if constexpr ( bY1 )
		rect.y1 -= 0.5f;

	if constexpr ( bX2 )
		rect.x2 -= 0.5f;

	if constexpr ( bY2 )
		rect.y2 -= 0.5f;

	// Steam CSprite2d::DrawRect
	((void(*)(const CRect&, const CRGBA&))0x75CDA0)(rect, rgba);
}

char* GetMyDocumentsPathSA()
{
	static char* const pDocumentsPath = [&] () -> char* {	
		static char	cUserFilesPath[MAX_PATH];
		char* const ppTempBufPtr = Memory::GetVersion().version == 0 ? *AddressByRegion_10<char**>(0x744FE5) : cUserFilesPath;

		if ( SHGetFolderPathA(nullptr, CSIDL_MYDOCUMENTS, nullptr, SHGFP_TYPE_CURRENT, ppTempBufPtr) == S_OK )
		{
			char** const ppUserFilesDir = AddressByVersion<char**>(0x74503F, 0x74586F, 0x77EE50, { "6A 00 68 80 00 00 02 6A 03 6A 00 6A 01 B9 07 00 00 00", 0x12 + 1 });

			PathAppendA(ppTempBufPtr, *ppUserFilesDir);
			CreateDirectoryA(ppTempBufPtr, nullptr);
		}
		else
		{
			strcpy_s(ppTempBufPtr, MAX_PATH, "data");
		}

		char cTmpPath[MAX_PATH];

		strcpy_s(cTmpPath, ppTempBufPtr);
		PathAppendA(cTmpPath, "Gallery");
		CreateDirectoryA(cTmpPath, nullptr);

		strcpy_s(cTmpPath, ppTempBufPtr);
		PathAppendA(cTmpPath, "User Tracks");
		CreateDirectoryA(cTmpPath, nullptr);

		return ppTempBufPtr;
	} ();
	return pDocumentsPath;
}

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

auto FlushSpriteBuffer = AddressByVersion<void(*)()>(0x70CF20, 0x70D750, 0x7591E0, { "85 C0 0F 8E ? ? ? ? 83 3D", -5 });
void FlushLensSwitchZ( RwRenderState rwa, void* rwb )
{
	FlushSpriteBuffer();
	RwRenderStateSet( rwa, rwb );
}

auto InitSpriteBuffer2D = AddressByVersion<void(*)()>(0x70CFD0, 0x70D800, 0x759290, { "A1 ? ? ? ? D9 80 ? ? ? ? A1" });
void InitBufferSwitchZ( RwRenderState rwa, void* rwb )
{
	RwRenderStateSet( rwa, rwb );
	InitSpriteBuffer2D();
}

static void* const g_fx = *AddressByVersion<void**>(0x4A9649, 0x4AA4EF, 0x4B2BB9, { "56 8D 4F 0C E8", 9 + 1 });
static int32_t GetFxQuality()
{
	return *(int32_t*)( (uint8_t*)g_fx + 0x54 );
}


DWORD*				msaaValues = *AddressByVersion<DWORD**>(0x4CCBC5, 0x4CCDB5, 0x4D7462, { "8B 3D ? ? ? ? 57 8B 7B 18", 2 });
// These patterns have 3 hits, but that's fine as all 3 refer to exact same variables
RwRaster*&			pMirrorBuffer = **AddressByVersion<RwRaster***>(0x723001, 0x723831, 0x754971, { "A1 ? ? ? ? 3B C6 74 0F 50 E8 ? ? ? ? 83 C4 04 89 35 ? ? ? ? 89 35 ? ? ? ? 89 35 ? ? ? ? 5E C3", -6 + 2 });
RwRaster*&			pMirrorZBuffer = **AddressByVersion<RwRaster***>(0x72301C, 0x72384C, 0x75498C, { "A1 ? ? ? ? 3B C6 74 0F 50 E8 ? ? ? ? 83 C4 04 89 35 ? ? ? ? 89 35 ? ? ? ? 89 35 ? ? ? ? 5E C3", 1 });
void CreateMirrorBuffers()
{
	if ( pMirrorBuffer == nullptr )
	{
		DWORD oldMsaa[2] = { msaaValues[0], msaaValues[1] };
		msaaValues[0] = msaaValues[1] = 0;

		int32_t quality = GetFxQuality();
		RwInt32 width, height;

		if ( quality >= 3 ) // Very High
		{
			width = 2048;
			height = 1024;
		}
		else if ( quality >= 1 ) // Medium
		{
			width = 1024;
			height = 512;
		}
		else
		{
			width = 512;
			height = 256;
		}

		pMirrorBuffer = RwRasterCreate( width, height, 0, rwRASTERTYPECAMERATEXTURE );
		pMirrorZBuffer = RwRasterCreate( width, height, 0, rwRASTERTYPEZBUFFER );

		msaaValues[0] = oldMsaa[0];
		msaaValues[1] = oldMsaa[1];
	}
}

namespace MSAAFixes
{

static RwUInt32 GetMaxMultiSamplingLevels_BitScan(RwUInt32 maxSamples)
{
	RwUInt32 option;
	_BitScanForward( (DWORD*)&option, maxSamples );
	return option + 1;
}

template<typename std::size_t Index>
static RwUInt32 (*orgGetMaxMultiSamplingLevels)();

template<typename std::size_t Index>
static RwUInt32 GetMaxMultiSamplingLevels()
{
	return GetMaxMultiSamplingLevels_BitScan(orgGetMaxMultiSamplingLevels<Index>());
}
HOOK_EACH_INIT(GetMaxMultiSamplingLevels, orgGetMaxMultiSamplingLevels, GetMaxMultiSamplingLevels);

template<typename std::size_t Index>
static void (*orgSetOrChangeMultiSamplingLevels)(RwUInt32);

template<typename std::size_t Index>
static void SetOrChangeMultiSamplingLevels(RwUInt32 level)
{
	orgSetOrChangeMultiSamplingLevels<Index>( 1 << (level - 1) );
}
HOOK_EACH_INIT(SetOrChangeMultiSamplingLevels, orgSetOrChangeMultiSamplingLevels, SetOrChangeMultiSamplingLevels);

void MSAAText( char* buffer, const char*, DWORD level )
{
	sprintf_s( buffer, 100, "%ux", 1 << level );
}

}


static RwInt32 numSavedVideoModes;
static RwInt32 (*orgGetNumVideoModes)();
RwInt32 GetNumVideoModes_Store()
{
	return numSavedVideoModes = orgGetNumVideoModes();
}

RwInt32 GetNumVideoModes_Retrieve()
{
	return numSavedVideoModes;
}

namespace UnitializedCollisionDataFix
{

static void* (*orgMemMgrMalloc)(RwUInt32, RwUInt32);
static void* CollisionData_MallocAndInit( RwUInt32 size, RwUInt32 hint )
{
	CColData*	mem = (CColData*)orgMemMgrMalloc( size, hint );

	mem->m_bFlags = 0;
	mem->m_dwNumShadowTriangles = mem->m_dwNumShadowVertices = 0;
	mem->m_pShadowVertices = mem->m_pShadowTriangles = nullptr;

	return mem;
}

template<std::size_t Index>
static void* (*orgNewAlloc)(size_t);

template<std::size_t Index>
static void* CollisionData_NewAndInit(size_t size)
{
	CColData*	mem = (CColData*)orgNewAlloc<Index>(size);

	mem->m_bFlags = 0;

	return mem;
}
HOOK_EACH_INIT(CollisionDataNew, orgNewAlloc, CollisionData_NewAndInit);

}

static void (*orgEscalatorsUpdate)();
void UpdateEscalators()
{
	if ( !CEscalator::ms_entitiesToRemove.empty() )
	{
		for ( auto it : CEscalator::ms_entitiesToRemove )
		{
			WorldRemove( it );
			delete it;
		}
		CEscalator::ms_entitiesToRemove.clear();
	}
	orgEscalatorsUpdate();
}


static char** pStencilShadowsPad = *AddressByVersion<char***>(0x70FC4F, 0, 0x75E286, { "8B 15 ? ? ? ? D8 65 A8", 2 });
void StencilShadowAlloc( )
{
	static char* pMemory = [] () {;
		char* mem = static_cast<char*>( ::operator new( 3 * 0x6000 ) );
		pStencilShadowsPad[0] = mem;
		pStencilShadowsPad[1] = mem+0x6000;
		pStencilShadowsPad[2] = mem+(2*0x6000);

		return mem;
	} ();
}

RwBool GTARtAnimInterpolatorSetCurrentAnim(RtAnimInterpolator* animI, RtAnimAnimation* anim)
{
	animI->pCurrentAnim = anim;
	animI->currentTime = 0.0f;

	const RtAnimInterpolatorInfo* info = anim->interpInfo;
	animI->currentInterpKeyFrameSize = info->interpKeyFrameSize;
	animI->currentAnimKeyFrameSize = info->animKeyFrameSize;
	animI->keyFrameApplyCB = info->keyFrameApplyCB;
	animI->keyFrameBlendCB = info->keyFrameBlendCB;
	animI->keyFrameInterpolateCB = info->keyFrameInterpolateCB;
	animI->keyFrameAddCB = info->keyFrameAddCB;

	for ( RwInt32 i = 0; i < animI->numNodes; ++i )
	{
		RtAnimKeyFrameInterpolate( animI, rtANIMGETINTERPFRAME( animI, i ),
			(RwChar*)anim->pFrames + i * animI->currentAnimKeyFrameSize,
			(RwChar*)anim->pFrames + ( i + animI->numNodes) * animI->currentAnimKeyFrameSize, 0.0f );
	}

	animI->pNextFrame = (RwChar*)anim->pFrames + 2 * animI->currentAnimKeyFrameSize * animI->numNodes;

	return TRUE;
}

DWORD WINAPI CdStreamSetFilePointer( HANDLE hFile, uint32_t distanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod )
{
	assert( lpDistanceToMoveHigh == nullptr );

	LARGE_INTEGER li;
	li.QuadPart = int64_t(distanceToMove) << 11;
	return SetFilePointer( hFile, li.LowPart, &li.HighPart, dwMoveMethod );
}
static auto* const pCdStreamSetFilePointer = CdStreamSetFilePointer;

static void (*orgDrawScriptSpritesAndRectangles)(uint8_t);
void DrawScriptSpritesAndRectangles( uint8_t arg )
{
	RwRenderStateSet( rwRENDERSTATETEXTUREFILTER, (void*)rwFILTERLINEAR );
	orgDrawScriptSpritesAndRectangles( arg );
}

// Now in VehicleSA.cpp
bool ReadDoubleRearWheels(const wchar_t* pPath);
bool __stdcall CheckDoubleRWheelsList( void* modelInfo, uint8_t* handlingData );

CVehicleModelInfo* (__thiscall *orgVehicleModelInfoInit)(CVehicleModelInfo*);
CVehicleModelInfo* __fastcall VehicleModelInfoInit(CVehicleModelInfo* me)
{
	orgVehicleModelInfoInit(me);
	
	// Hack to satisfy some null checks
	static uintptr_t DUMMY;
	me->__removedInSilentPatch = &DUMMY;

	me->m_dirtMaterials = nullptr;
	me->m_numDirtMaterials = 0;
	std::fill( std::begin( me->m_staticDirtMaterials ), std::end( me->m_staticDirtMaterials ), nullptr );
	return me;
}

static void (*RemoveFromInterestingVehicleList)(CVehicle*) = AddressByVersion<void(*)(CVehicle*)>( 0x423ED0, Memory::PatternAndOffset("39 10 75 06 C7 00 00 00 00 00 83 C0 04 49 75 F0 5D C3", -0x10) );
static void (*orgRecordVehicleDeleted)(CVehicle*);
static void RecordVehicleDeleted_AndRemoveFromVehicleList( CVehicle* vehicle )
{
	orgRecordVehicleDeleted( vehicle );
	RemoveFromInterestingVehicleList( vehicle );
}

static int currDisplayedSplash_ForLastSplash = 0;
static void DoPCScreenChange_Mod()
{
	static int& currDisplayedSplash = **AddressByVersion<int**>( 0x590B22 + 1, Memory::PatternAndOffset("8B 51 20 6A 01 6A 0C FF D2 83 C4 08 E8", 17 + 1) );

	static const int numSplashes = [] () -> int {
		RwTexture** begin = *AddressByVersion<RwTexture***>( 0x590CB4 + 1, Memory::PatternAndOffset("8D 49 00 83 3E 00 74 07 8B CE E8", -5 + 1) );
		RwTexture** end = *AddressByVersion<RwTexture***>( 0x590CCE + 2, Memory::PatternAndOffset("8D 49 00 83 3E 00 74 07 8B CE E8", 18 + 2) );
		return std::distance( begin, end );
	} () - 1;

	if ( currDisplayedSplash >= numSplashes )
	{
		currDisplayedSplash = 1;
		currDisplayedSplash_ForLastSplash = numSplashes + 1;
	}
	else
	{
		currDisplayedSplash_ForLastSplash = ++currDisplayedSplash;
	}
}

static bool bUseAaronSun;
static CVector curVecToSun;
static void (*orgSetLightsWithTimeOfDayColour)( RpWorld* );
static void SetLightsWithTimeOfDayColour_SilentPatch( RpWorld* world )
{
	static CVector* const VectorToSun = *AddressByVersion<CVector**>( 0x6FC5B7 + 3, Memory::PatternAndOffset("DC 0D ? ? ? ? 8D 04 40 8B 0C 85", 9 + 3) );
	static int& CurrentStoredValue = **AddressByVersion<int**>( 0x6FC632 + 1, Memory::PatternAndOffset("84 C0 0F 84 AB 01 00 00 A1", 8 + 1) );
	static CVector& vecDirnLightToSun = **AddressByVersion<CVector**>( 0x5BC040 + 2, Memory::PatternAndOffset("E8 ? ? ? ? D9 5D F8 D9 45 F8 D8 4D F4 D9 1D", 15 + 2) );

	curVecToSun = bUseAaronSun ? VectorToSun[CurrentStoredValue] : vecDirnLightToSun;
	orgSetLightsWithTimeOfDayColour( world );
}

// ============= CdStream data racing issue =============

struct CdStream
{
	DWORD nSectorOffset;
	DWORD nSectorsToRead;
	LPVOID lpBuffer;
	BYTE field_C;
	BYTE bLocked;
	BYTE bInUse;
	BYTE field_F;
	DWORD status;
	union Sync {
		HANDLE semaphore;
		CONDITION_VARIABLE cv;
	} sync;
	HANDLE hFile;
	OVERLAPPED overlapped;
};

static_assert(sizeof(CdStream) == 0x30, "Incorrect struct size: CdStream");

namespace CdStreamSync {

static CRITICAL_SECTION CdStreamCritSec;

// WRL-like lock object
// Balancing lock/unlock is up to the user!
class SyncLock
{
public:
	SyncLock( CRITICAL_SECTION& critSec )
		: m_critSec( critSec )
	{
		Lock();
	}

	~SyncLock()
	{
		Unlock();
	}

	void Lock() const { EnterCriticalSection( &m_critSec ); }
	void Unlock() const { LeaveCriticalSection( &m_critSec ); } 

	CRITICAL_SECTION* Get() const { return &m_critSec; }

private:
	SyncLock( const SyncLock& ) = delete;
	SyncLock( SyncLock&& ) = delete;
	SyncLock& operator=( const SyncLock& ) = delete;
	SyncLock& operator=( SyncLock&& ) = delete;

	CRITICAL_SECTION& m_critSec;
};



// Function pointers for game to use
static CdStream::Sync (__stdcall *CdStreamInitializeSyncObject)();
static DWORD (__stdcall *CdStreamSyncOnObject)( CdStream* stream );
static void (__stdcall *CdStreamThreadOnObject)( CdStream* stream );
static void (__stdcall *CdStreamCloseObject)( CdStream::Sync* sync );
static void (__stdcall *CdStreamShutdownSyncObject)( CdStream* stream );

static void __stdcall CdStreamShutdownSyncObject_Stub( CdStream* stream, size_t idx )
{
	CdStreamShutdownSyncObject( &stream[idx] );
}

// Fixed return values for GetOverlappedResult - stock code assumes "nonzero" equals 1, might not be future proof
static uint32_t WINAPI GetOverlappedResult_SilentPatch( HANDLE hFile, LPOVERLAPPED lpOverlapped, LPDWORD lpNumberOfBytesTransferred, BOOL bWait )
{
	return GetOverlappedResult( hFile, lpOverlapped, lpNumberOfBytesTransferred, bWait ) != FALSE ? 0 : 254;
}
static auto* const pGetOverlappedResult = &GetOverlappedResult_SilentPatch;


namespace Sema
{
	CdStream::Sync __stdcall InitializeSyncObject()
	{
		CdStream::Sync object;
		object.semaphore = CreateSemaphore( nullptr, 0, 2, nullptr );
		return object;
	}

	void __stdcall ShutdownSyncObject( CdStream* stream )
	{
		CloseHandle( stream->sync.semaphore );
	}

	DWORD __stdcall CdStreamSync( CdStream* stream )
	{
		auto lock = SyncLock( CdStreamCritSec );
		if ( stream->nSectorsToRead != 0 )
		{
			stream->bLocked = 1;
			lock.Unlock();
			WaitForSingleObject( stream->sync.semaphore, INFINITE );
			lock.Lock();
		}
		stream->bInUse = 0;
		return stream->status;
	}

	void __stdcall CdStreamThread( CdStream* stream )
	{
		auto lock = SyncLock( CdStreamCritSec );
		stream->nSectorsToRead = 0;
		if ( stream->bLocked != 0 )
		{
			ReleaseSemaphore( stream->sync.semaphore, 1, nullptr );
		}
		stream->bInUse = 0;
	}
}

namespace CV
{
	namespace Funcs
	{
		static decltype(InitializeConditionVariable)* pInitializeConditionVariable = nullptr;
		static decltype(SleepConditionVariableCS)* pSleepConditionVariableCS = nullptr;
		static decltype(WakeConditionVariable)* pWakeConditionVariable = nullptr;

		static bool TryInit()
		{
			const HMODULE kernelDLL = GetModuleHandle( TEXT("kernel32") );
			assert( kernelDLL != nullptr );
			pInitializeConditionVariable = (decltype(pInitializeConditionVariable))GetProcAddress( kernelDLL, "InitializeConditionVariable" );
			pSleepConditionVariableCS = (decltype(pSleepConditionVariableCS))GetProcAddress( kernelDLL, "SleepConditionVariableCS" );
			pWakeConditionVariable = (decltype(pWakeConditionVariable))GetProcAddress( kernelDLL, "WakeConditionVariable" );

			return pInitializeConditionVariable != nullptr && pSleepConditionVariableCS != nullptr && pWakeConditionVariable != nullptr;
		}


	}

	CdStream::Sync __stdcall InitializeSyncObject()
	{
		CdStream::Sync object;
		Funcs::pInitializeConditionVariable( &object.cv );
		return object;
	}

	void __stdcall ShutdownSyncObject( CdStream* stream )
	{
	}

	DWORD __stdcall CdStreamSync( CdStream* stream )
	{
		auto lock = SyncLock( CdStreamCritSec );
		while ( stream->nSectorsToRead != 0 )
		{
			Funcs::pSleepConditionVariableCS( &stream->sync.cv, lock.Get(), INFINITE );
		}
		stream->bInUse = 0;
		return stream->status;
	}

	void __stdcall CdStreamThread( CdStream* stream )
	{
		auto lock = SyncLock( CdStreamCritSec );
		stream->nSectorsToRead = 0;
		Funcs::pWakeConditionVariable( &stream->sync.cv );
		stream->bInUse = 0;
	}
}

static void (*orgCdStreamInitThread)();
static void CdStreamInitThread()
{
	if ( ModCompat::bCdStreamFallBackForOldML != true && CV::Funcs::TryInit() )
	{
		CdStreamInitializeSyncObject = CV::InitializeSyncObject;
		CdStreamShutdownSyncObject = CV::ShutdownSyncObject;
		CdStreamSyncOnObject = CV::CdStreamSync;
		CdStreamThreadOnObject = CV::CdStreamThread;
	}
	else
	{
		CdStreamInitializeSyncObject = Sema::InitializeSyncObject;
		CdStreamShutdownSyncObject = Sema::ShutdownSyncObject;
		CdStreamSyncOnObject = Sema::CdStreamSync;
		CdStreamThreadOnObject = Sema::CdStreamThread;		
	}

	InitializeCriticalSection( &CdStreamCritSec );

	FLAUtils::SetCdStreamWakeFunction( []( CdStream* pStream ) {
		CdStreamThreadOnObject( pStream );
	} );

	orgCdStreamInitThread();
}

}

// Dancing timers fix
static long UtilsVariablesInit = 0;
static LARGE_INTEGER UtilsStartTime;
static LARGE_INTEGER UtilsFrequency;
static BOOL WINAPI AudioUtilsFrequency( PLARGE_INTEGER lpFrequency )
{
	::QueryPerformanceFrequency( &UtilsFrequency );
	lpFrequency->QuadPart = UtilsFrequency.QuadPart;
	return TRUE;
}
static auto* const pAudioUtilsFrequency = &AudioUtilsFrequency;

static int64_t AudioUtilsGetStartTime()
{
	QueryPerformanceCounter( &UtilsStartTime );

	_InterlockedExchange( &UtilsVariablesInit, 1 );
	return UtilsStartTime.QuadPart;
}

static int64_t AudioUtilsGetCurrentTimeInMs()
{
	if ( _InterlockedCompareExchange( &UtilsVariablesInit, 0, 0 ) == 0 )
	{
		return 0;
	}

	LARGE_INTEGER currentTime;
	QueryPerformanceCounter( &currentTime );
	return ((currentTime.QuadPart - UtilsStartTime.QuadPart) * 1000) / UtilsFrequency.QuadPart;
}

// ============= Minimal HUD changes =============
namespace MinimalHUD
{
	static CRGBA* __fastcall SetRGBA_FloatAlpha( CRGBA* rgba, void*, uint8_t red, uint8_t green, uint8_t blue, float alpha )
	{
		rgba->r = red;
		rgba->g = green;
		rgba->b = blue;
		rgba->a = static_cast<uint8_t>(alpha);
		return rgba;
	}

	static void (*orgRenderOneXLUSprite)(float, float, float, float, float, uint8_t, uint8_t, uint8_t, int16_t, float, uint8_t, uint8_t, uint8_t);
	static void RenderXLUSprite_FloatAlpha( float arg1, float arg2, float arg3, float arg4, float arg5, uint8_t red, uint8_t green, uint8_t blue, int16_t mult, float arg10, float alpha, uint8_t arg12, uint8_t arg13 )
	{
		orgRenderOneXLUSprite( arg1, arg2, arg3, arg4, arg5, red, green, blue, mult, arg10, static_cast<uint8_t>(alpha), arg12, arg13 );
	}
}

// 6 directionals on Medium/High/Very High Visual FX
int32_t GetMaxExtraDirectionals( uint32_t areaID )
{
	return areaID != 0 || GetFxQuality() >= 1 ? 6 : 4;
}

static CVehicle* FindPlayerVehicle_RCWrap( int playerID, bool )
{
	return FindPlayerVehicle( playerID, true );
}

// ============= Credits! =============
namespace Credits
{
	static void (*PrintCreditText)(float scaleX, float scaleY, const char* text, unsigned int& pos, float timeOffset, bool isHeader);
	static void (*PrintCreditText_Hooked)(float scaleX, float scaleY, const char* text, unsigned int& pos, float timeOffset, bool isHeader);

	static void PrintCreditSpace( float scale, unsigned int& pos )
	{
		pos += static_cast<unsigned int>( scale * 25.0f );
	}

	constexpr char xvChar(const char ch)
	{
		constexpr uint8_t xv = SILENTPATCH_REVISION_ID;
		return ch ^ xv;
	}

	constexpr char operator "" _xv(const char ch)
	{
		return xvChar(ch);
	}

	static void PrintSPCredits( float scaleX, float scaleY, const char* text, unsigned int& pos, float timeOffset, bool isHeader )
	{
		// Original text we intercepted
		PrintCreditText_Hooked( scaleX, scaleY, text, pos, timeOffset, isHeader );
		PrintCreditSpace( 1.5f, pos );

		{
			char spText[] = { 'A'_xv, 'N'_xv, 'D'_xv, '\0'_xv };

			for ( auto& ch : spText ) ch = xvChar(ch);
			PrintCreditText( scaleX, scaleY, spText, pos, timeOffset, true );
		}

		PrintCreditSpace( 1.5f, pos );

		{
			char spText[] = { 'A'_xv, 'd'_xv, 'r'_xv, 'i'_xv, 'a'_xv, 'n'_xv, ' '_xv, '\"'_xv, 'S'_xv, 'i'_xv, 'l'_xv, 'e'_xv, 'n'_xv, 't'_xv, '\"'_xv, ' '_xv,
							'Z'_xv, 'd'_xv, 'a'_xv, 'n'_xv, 'o'_xv, 'w'_xv, 'i'_xv, 'c'_xv, 'z'_xv, '\0'_xv };

			for ( auto& ch : spText ) ch = xvChar(ch);
			PrintCreditText( scaleX, scaleY, spText, pos, timeOffset, false );
		}
	}
}

// ============= Bicycle fire fix =============
namespace BicycleFire
{
	CPed* GetVehicleDriver( const CVehicle* vehicle )
	{
		return vehicle->GetDriver();
	}

	void __fastcall DoStuffToGoOnFire_NullAndPlayerCheck( CPed* ped )
	{
		if ( ped != nullptr && ped->IsPlayer() )
		{
			static_cast<CPlayerPed*>(ped)->DoStuffToGoOnFire();
		}
	}
}


// ============= Keyboard latency input fix =============
namespace KeyboardInputFix
{
	static void* NewKeyState;
	static void* OldKeyState;
	static void* TempKeyState;
	static size_t objSize;
	static void (__fastcall *orgClearSimButtonPressCheckers)(void*);
	void __fastcall ClearSimButtonPressCheckers(void* pThis)
	{
		memcpy( OldKeyState, NewKeyState, objSize );
		memcpy( NewKeyState, TempKeyState, objSize );

		orgClearSimButtonPressCheckers(pThis);
	}
}

// ============= handling.cfg name matching fix =============
namespace HandlingNameLoadFix
{
	void strncpy_Fix( const char** destination, const char* source, size_t )
	{
		*destination = source;
	}

	int strncmp_Fix( const char* str1, const char** str2, size_t )
	{
		return strcmp( str1, *str2 );
	}
};


// ============= Firela ladder animation =============
namespace FirelaHook
{
	static uintptr_t UpdateMovingCollisionJmp;
	static uintptr_t HydraulicControlJmpBack;

	__declspec(naked) static void TestFirelaAndFlags()
	{
		_asm
		{
			push	ecx		// Required in 0x6B1FE4: test cl, cl
			mov		ecx, esi
			call	CVehicle::HasFirelaLadder
			pop		ecx
			test	al, al
			jnz		TestFirelaAndFlags_UpdateMovingCollision
			test	[esi].hFlagsLocal, FLAG_HYDRAULICS_INSTALLED
			jmp		HydraulicControlJmpBack

		TestFirelaAndFlags_UpdateMovingCollision:
			jmp		UpdateMovingCollisionJmp
		}
	}

	static uintptr_t FollowCarCamNoMovement;
	static uintptr_t FollowCarCamJmpBack;

	__declspec(naked) static void CamControlFirela()
	{
		_asm
		{
			mov		ecx, edi
			call	CVehicle::HasFirelaLadder
			test	al, al
			jnz		TestFirelaAndFlags_UpdateMovingCollision
			mov		eax, [edi].m_dwVehicleClass
			jmp		FollowCarCamJmpBack

		TestFirelaAndFlags_UpdateMovingCollision:
			jmp		FollowCarCamNoMovement
		}
	}
}

namespace HierarchyTypoFix
{
	// Allow wheel_lm vs wheel_lm_dummy and miscX vs misc_X typos
	// Must be sorted by second parameter
	constexpr std::pair<const char*, const char*> typosAndFixes[] = {
		{ "boat_moving_hi", "boat_moving" },
		{ "misc_a", "misca" },
		{ "misc_b", "miscb" },
		{ "taillights", "tailights" },
		{ "taillights2", "tailights2" },
		{ "transmission_f", "transmision_f" },
		{ "transmission_r", "transmision_r" },
		{ "wheel_lm_dummy", "wheel_lm" },
	};
	static int (*orgStrcasecmp)(const char*, const char*);
	int strcasecmp( const char* dataName, const char* nodeName )
	{
		/*assert( std::is_sorted(std::begin(typosAndFixes), std::end(typosAndFixes), [] (const auto& a, const auto& b) {
			return _stricmp( a.second, b.second ) < 0;
		}) );*/

		const int origComp = orgStrcasecmp( dataName, nodeName );
		if ( origComp == 0 ) return 0;

		for ( const auto& typo : typosAndFixes )
		{
			const int nodeComp = _stricmp( typo.second, nodeName );
			if ( nodeComp > 0 ) break;

			if ( nodeComp == 0 && _stricmp( typo.first, dataName ) == 0 )
			{
				return 0;
			}
		}

		return origComp;
	}

}


// Resetting stats and variables on New Game
namespace VariableResets
{
	// Custom reset values for variables
	template<typename T>
	struct TimeNextMadDriverChaseCreated_t
	{
		T m_timer;

		TimeNextMadDriverChaseCreated_t()
			: m_timer( (static_cast<float>(ConsoleRandomness::rand31()) / INT32_MAX) * 600.0f + 600.0f )
		{
		}
	};

	template<typename T, T val>
	struct ResetToValue_t
	{
		T m_value;

		ResetToValue_t()
			: m_value(val)
		{
		}
	};

	using ResetToTrue_t = ResetToValue_t<bool, true>;

	using VarVariant = std::variant< bool*, int*, TimeNextMadDriverChaseCreated_t<float>*, ResetToTrue_t* >;
	std::vector<VarVariant> GameVariablesToReset;
	std::vector<std::string> PickupDefs, CarGeneratorDefs, StuntJumpDefs;

	static void ReInitOurVariables()
	{
		for ( const auto& var : GameVariablesToReset )
		{
			std::visit( []( auto&& v ) {
				*v = {};
				}, var );
		}
	}

	static std::string_view TrimWhitespace(std::string_view line)
	{
		const size_t first = line.find_first_not_of(' ');
		if (std::string_view::npos != first)
		{
			const size_t last = line.find_last_not_of(' ');
			line = line.substr(first, (last - first + 1));
		}
		return line;
	}

	static void (*orgLoadPickup)(const char* line);
	static void LoadPickup_SaveLine(const char* line)
	{
		orgLoadPickup(line);
		PickupDefs.emplace_back(TrimWhitespace(line));
	}

	static void (*orgLoadCarGenerator)(const char* line, int originScene);
	static void LoadCarGenerator_SaveLine(const char* line, int originScene)
	{
		orgLoadCarGenerator(line, originScene);
		CarGeneratorDefs.emplace_back(TrimWhitespace(line));
	}

	static void (*orgLoadStuntJump)(const char* line);
	static void LoadStuntJump_SaveLine(const char* line)
	{
		orgLoadStuntJump(line);
		StuntJumpDefs.emplace_back(TrimWhitespace(line));
	}

	static void ReloadObjectDefinitionsAfterReinit()
	{
		for (const auto& pickup : PickupDefs)
		{
			orgLoadPickup(pickup.c_str());
		}
		for (const auto& carGenerator : CarGeneratorDefs)
		{
			orgLoadCarGenerator(carGenerator.c_str(), 0);
		}
		for (const auto& stuntJump : StuntJumpDefs)
		{
			orgLoadStuntJump(stuntJump.c_str());
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

		// Then after the normal restart, re-instate pickups, car generators and stunt jumps from text IPLs as they have been 
		ReloadObjectDefinitionsAfterReinit();
	}
	HOOK_EACH_INIT(ReInitGameObjectVariables, orgReInitGameObjectVariables, ReInitGameObjectVariables);
}

namespace LightbeamFix
{
	static bool hookedSuccessfully = false;

	static CVehicle* currentHeadLightBeamVehicle;
	void SetCurrentVehicle( CVehicle* vehicle )
	{
		currentHeadLightBeamVehicle = vehicle;
	}

	template<RwRenderState State>
	class RenderStateWrapper
	{
	private:
		static inline void* SavedState;

		static void PushState( RwRenderState state, void* value )
		{
			assert( State == state );
			RwRenderStateGet( state, &SavedState );
			RwRenderStateSet( state, value );
		}
		static inline const auto pPushState = &PushState;

		static void PopState( RwRenderState state, void* value )
		{
			assert( State == state );

			void* valueToRestore = SavedState;
			if constexpr ( State == rwRENDERSTATECULLMODE )
			{
				assert( currentHeadLightBeamVehicle != nullptr );
				if ( currentHeadLightBeamVehicle != nullptr && currentHeadLightBeamVehicle->IgnoresLightbeamFix() )
				{
					valueToRestore = value;
				}
			}

			RwRenderStateSet( state, valueToRestore );

			// Restore states R* did not restore after changing them
			if constexpr ( State == rwRENDERSTATEDESTBLEND )
			{
				RenderStateWrapper<rwRENDERSTATESHADEMODE>::PopAnotherState();
			}
			if constexpr ( State == rwRENDERSTATECULLMODE )
			{
				RenderStateWrapper<rwRENDERSTATEALPHATESTFUNCTION>::PopAnotherState();
				RenderStateWrapper<rwRENDERSTATEALPHATESTFUNCTIONREF>::PopAnotherState();
			}
		}
		static inline const auto pPopState = &PopState;

	public:
		static void PopAnotherState()
		{
			PopState( State, nullptr );
		}

		static inline const uintptr_t PushStatePPtr = reinterpret_cast<uintptr_t>(&pPushState) - 0x20;
		static inline const uintptr_t PopStatePPtr = reinterpret_cast<uintptr_t>(&pPopState) - 0x20;
	};

}

namespace TrueInvincibility
{
	static bool isEnabled = false;
	static uintptr_t WillKillJumpBack;

	__declspec(naked) static void ComputeWillKillPedHook()
	{
		_asm
		{
			cmp		dword ptr [ebp+0xC], WEAPONTYPE_LAST_WEAPONTYPE
			jl		ComputeWillKillPedHook_DoNotKill
			cmp		isEnabled, 0
			je		ComputeWillKillPedHook_Kill
			cmp		dword ptr [ebp+0xC], WEAPONTYPE_UZI_DRIVEBY
			jne		ComputeWillKillPedHook_Kill

		ComputeWillKillPedHook_DoNotKill:
			pop     esi
			pop     ebp
			pop     ebx
			ret    	0xC

		ComputeWillKillPedHook_Kill:
			jmp		WillKillJumpBack
		}
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

	// Only used in "new binary" executables
	bool* germanGame;
	bool* frenchGame;
	bool* nastyGame;
	void SetUncensoredGame()
	{
		*germanGame = false;
		*frenchGame = false;
		*nastyGame = true;
	}

	void EmptyStub()
	{
	}
}


// Reintroduced corona rotation
namespace CoronaRotationFix
{
	static void (*orgRenderOneXLUSprite_Rotate_Aspect)(float, float, float, float, float, uint8_t, uint8_t, uint8_t, short, float, float, uint8_t);
	void RenderOneXLUSprite_Rotate_Aspect_SilentPatch( float a1, float a2, float a3, float a4, float a5, uint8_t a6, uint8_t a7, uint8_t a8, short a9, float recipz, float, uint8_t a12 )
	{
		orgRenderOneXLUSprite_Rotate_Aspect( a1, a2, a3, a4, a5, a6, a7, a8, a9, recipz, 20.0f * recipz, a12 );
	}
};

// ============= Static shadow alpha fix =============
namespace StaticShadowAlphaFix
{
	static void (*orgRenderStaticShadows)();
	static void RenderStaticShadows_StateFix()
	{
		RwScopedRenderState<rwRENDERSTATEALPHATESTFUNCTION> state;

		RwRenderStateSet( rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONALWAYS );
		orgRenderStaticShadows();
	}

	static void (*orgRenderStoredShadows)();
	static void RenderStoredShadows_StateFix()
	{
		RwScopedRenderState<rwRENDERSTATEALPHATESTFUNCTION> state;

		RwRenderStateSet( rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONALWAYS );
		orgRenderStoredShadows();
	}
};


// ============= Disable building pipeline for skinned objects (like parachute) =============
namespace SkinBuildingPipelineFix
{
	static RpAtomic* (*orgCustomBuildingDNPipeline_CustomPipeAtomicSetup)(RpAtomic* atomic);
	static RpAtomic* CustomBuildingDNPipeline_CustomPipeAtomicSetup_Skinned(RpAtomic* atomic)
	{
		RxPipeline* pipeline;
		RpAtomicGetPipeline(atomic, &pipeline);
		if (pipeline != nullptr && pipeline->pluginId == rwID_SKINPLUGIN)
		{
			return atomic;
		}

		return orgCustomBuildingDNPipeline_CustomPipeAtomicSetup(atomic);
	}
};

// ============= Moonphases fix =============
namespace MoonphasesFix
{
	// TODO: Reintroduce moon phases to Steam/RGL version
	// Call to RenderOneXLUSprite provides all required data except the moon mask and CClock::ms_nGameClockDays
	static void (*orgRenderOneXLUSprite)(float, float, float, float, float, uint8_t, uint8_t, uint8_t, int16_t, float, uint8_t, uint8_t, uint8_t);

	// By aap
	static void RenderOneXLUSprite_MoonPhases( float arg1, float arg2, float arg3, float arg4, float arg5, uint8_t red, uint8_t green, uint8_t blue, int16_t mult, float arg10, uint8_t alpha, uint8_t arg12, uint8_t arg13 )
	{
		static RwTexture*	gpMoonMask = [] () {
			
			// load from file
			RwTexture* mask = CPNGFile::ReadFromFile("lunar.png");
			if (mask == nullptr)
			{
				const HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);

				// Load from memory
				HRSRC resource = FindResource(module, MAKEINTRESOURCE(IDB_LUNAR64), RT_RCDATA);
				if (resource != nullptr)
				{
					HGLOBAL loadedResource = LoadResource(module, resource);
					if (loadedResource != nullptr)
					{
						mask = CPNGFile::ReadFromMemory(LockResource(loadedResource), SizeofResource(module, resource));
					}
				}
			}
			return mask;
		} ();

		RwScopedRenderState<rwRENDERSTATEALPHATESTFUNCTION> alphaTest;

		if ( gpMoonMask != nullptr )
		{
			RwRenderStateSet( rwRENDERSTATETEXTURERASTER, RwTextureGetRaster(gpMoonMask) );
		}

		RwRenderStateSet( rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONALWAYS );
		RwRenderStateSet( rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA );
		RwRenderStateSet( rwRENDERSTATEDESTBLEND, (void*)rwBLENDZERO );
		RwD3D9SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA );

		orgRenderOneXLUSprite( arg1, arg2, arg3, arg4, arg5, red, green, blue, mult, arg10, alpha, arg12, arg13 );

		RwD3D9SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED );
	}
}

// ============= Disallow moving cam up/down with mouse when looking back/left/right in vehicle =============
namespace FollowCarMouseCamFix
{
	static uint32_t& camLookDirection = **AddressByVersion<uint32_t**>( 0x525526 + 2, Memory::PatternAndOffset("83 3D ? ? ? ? 03 74 06", 2) );
	static void* (*orgGetPad)(int);
	static bool* orgUseMouse3rdPerson;

	static bool useMouseAndLooksForwards;
	static void* getPadAndSetFlag( int padNum )
	{
		useMouseAndLooksForwards = *orgUseMouse3rdPerson && camLookDirection == 3;
		return orgGetPad( padNum );
	}
};

// ======= Tie handlebar movement to the stering animations on Quadbike, fixes odd animation interpolations at low speeds =======
namespace QuadbikeHandlebarAnims
{
	static const float POW_CONSTANT = 0.86f;
	static const float SLOW_SPEED_THRESHOLD = 0.02f;
	__declspec(naked) static void ProcessRiderAnims_FixInterp()
	{
		_asm
		{
			xor		edx, edx
			cmp     [esp+0x130-0x100], edx // Reverse animation
			jne		FuncSetToZero
			cmp     [esp+0x130-0xF8], edx // Drive-by animation
			jne		FuncSetToZero
			fld     dword ptr [esp+0x130-0x108]
			fabs
			fcomp	SLOW_SPEED_THRESHOLD
			fnstsw  ax
			test    ah, 5
			jp		FuncReturn

		FuncSetToZero:
			mov		[esp+0x130-0x118], edx

		FuncReturn:
			fld		POW_CONSTANT
			ret
		}
	}

	static uint32_t savedClumpAssociation;
	__declspec(naked) static void SaveDriveByAnim_Steam()
	{
		_asm
		{
			mov		eax, [ebp-0x14]
			mov		savedClumpAssociation, eax
			fdiv    dword ptr [ecx+0x18]
			fstp	dword ptr [ebp-0x14]
			ret
		}
	}

	__declspec(naked) static void ProcessRiderAnims_FixInterp_Steam()
	{
		_asm
		{
			xor		edx, edx
			cmp		[ebp-0x28], edx // Reverse animation
			jne		FuncSetToZero
			cmp		savedClumpAssociation, edx // Drive-by animation
			jne		FuncSetToZero
			fld     dword ptr [ebp-0x24]
			fabs
			fcomp	SLOW_SPEED_THRESHOLD
			fnstsw  ax
			test    ah, 5
			jp		FuncReturn

		FuncSetToZero:
			mov		[ebp-0x14], edx

		FuncReturn:
			fld		[POW_CONSTANT]
			ret
		}
	}

}


// ======= Disable the radio station change anim on boats where CJ stands upright =======
namespace UprightBoatRadioStationChange
{
	static void* (*orgAnimManagerBlendAnimation)(RpClump*, uint32_t, uint32_t, float);
	void* AnimManagerBlendAnimation_SkipIfBoatDrive(RpClump* clump, uint32_t assocGroupId, uint32_t animationId, float rate)
	{
		if (RpAnimBlendClumpGetAssociation(clump, 81) != nullptr) // ANIM_STD_BOAT_DRIVE
		{
			return nullptr;
		}
		return orgAnimManagerBlendAnimation(clump, assocGroupId, animationId, rate);
	}
};


// ============= Fix a memory leak when taking photos =============
namespace CameraMemoryLeakFix
{
	__declspec(naked) static void psGrabScreen_UnlockAndReleaseSurface()
	{
		_asm
		{
			// Preserve the function result so we don't need two ASM hooks
			push	eax

			mov		eax, [esp+0x34-0x2C]
			mov		edx, [eax]
			push	eax
			call	dword ptr [edx+0x38] // IDirect3DSurface9.UnlockRect

			mov		eax, [esp+0x34-0x2C]
			mov		edx, [eax]
			push	eax
			call	dword ptr [edx+0x8] // IDirect3DSurface9.Release

			pop		eax
			pop		ebp
			add		esp, 0x2C
			retn
		}
	}

	__declspec(naked) static void psGrabScreen_UnlockAndReleaseSurface_Steam()
	{
		_asm
		{
			// Preserve the function result so we don't need two ASM hooks
			push	eax

			mov		eax, [ebp-4]
			mov		edx, [eax]
			push	eax
			call	dword ptr [edx+0x38] // IDirect3DSurface9.UnlockRect

			mov		eax, [ebp-4]
			mov		edx, [eax]
			push	eax
			call	dword ptr [edx+0x8] // IDirect3DSurface9.Release

			pop		eax
			pop		esi
			mov		esp, ebp
			pop		ebp
			ret
		}
	}
}


// ============= Fix crosshair issues when sniper rifle is quipped and a photo is taken by a gang member =============
namespace CameraCrosshairFix
{
	CWeaponInfo* (*orgGetWeaponInfo)(eWeaponType, signed char);
	CWeaponInfo* GetWeaponInfo_OrCamera(eWeaponType weaponType, signed char type)
	{
		return orgGetWeaponInfo(bDrawCrossHair != 2 ? weaponType : WEAPONTYPE_CAMERA, type);
	}
}


// ============= Cancel the Drive By task of biker cops when losing the wanted level =============
namespace BikerCopsDriveByFix
{
	static void (*orgJoinCarWithRoadSystem)(CVehicle* vehicle);
	void JoinCarWithRoadSystem_AbortDriveByTask(CVehicle* vehicle)
	{
		orgJoinCarWithRoadSystem(vehicle);

		CPed* driver = vehicle->GetDriver();
		if (driver != nullptr)
		{
			CPedIntelligence* driverIntelligence = driver->GetPedIntelligencePtr();
			if (driverIntelligence != nullptr)
			{
				// If the driver has a sequence, it's the fourth one
				CTask* primaryTask = driverIntelligence->m_taskManager.m_primaryTasks[3];
				if (primaryTask != nullptr && primaryTask->GetTaskType() == 244) // TASK_COMPLEX_SEQUENCE
				{
					// If the sequence contains a TASK_SIMPLE_GANG_DRIVEBY, abort it
					CTaskComplexSequence* taskSequence = reinterpret_cast<CTaskComplexSequence*>(primaryTask);
					if (taskSequence->Contains(1022))
					{
						taskSequence->MakeAbortable(driver, 1, nullptr);
					}
				}
			}
		}
	}
}


// ============= Fix miscolored racing checkpoints if no other marker was drawn before them =============
namespace RacingCheckpointsRender
{
	static RpClump* (*orgRpClumpRender)(RpClump* clump);
	static RpClump* RpClumpRender_SetLitFlag(RpClump* clump)
	{
		RpClumpForAllAtomics(clump, [](RpAtomic* atomic)
			{
				RpGeometry* geometry = RpAtomicGetGeometry(atomic);
				RpGeometrySetFlags(geometry, RpGeometryGetFlags(geometry) | rpGEOMETRYMODULATEMATERIALCOLOR);
				return atomic;
			});
		return orgRpClumpRender(clump);
	}
}

// ============= Correct an improperly decrypted CPlayerPedData::operator= that broke gang recruiting after activating replays =============
namespace PlayerPedDataAssignment
{
	__declspec(naked) static void AssignmentOp_Hoodlum()
	{
		_asm
		{
			xor     edx, [ecx+0x34]
			and     edx, 1
			xor     [eax+0x34], edx
			mov     esi, [eax+0x34]
			mov     edx, [ecx+0x34]
			xor     edx, esi
			and     edx, 2
			xor     edx, esi
			mov     [eax+0x34], edx
			mov     esi, [ecx+0x34]
			xor     esi, edx
			and     esi, 4
			xor     esi, edx
			mov     [eax+0x34], esi
			mov     edx, [ecx+0x34]
			xor     edx, esi
			and     edx, 8
			xor     edx, esi
			mov     [eax+0x34], edx
			mov     esi, [ecx+0x34]
			xor     esi, edx
			and     esi, 0x10
			xor     esi, edx
			mov     [eax+0x34], esi
			mov     edx, [ecx+0x34]
			xor     edx, esi
			and     edx, 0x20
			xor     edx, esi
			mov     [eax+0x34], edx
			mov     esi, [ecx+0x34]
			xor     esi, edx
			and     esi, 0x40
			xor     esi, edx
			mov     [eax+0x34], esi
			mov     edx, [ecx+0x34]
			xor     edx, esi
			and     edx, 0x80
			xor     edx, esi
			mov     [eax+0x34], edx
			mov     esi, [ecx+0x34]
			xor     esi, edx
			and     esi, 0x100
			xor     esi, edx
			mov     [eax+0x34], esi
			mov     edx, [ecx+0x34]
			ret
		}
	}

	__declspec(naked) static void AssignmentOp_Compact()
	{
		_asm
		{
			call	AssignmentOp_Hoodlum
			xor     edx, esi
			and     edx, 0x200
			ret
		}
	}
}


// ============= Spawn lapdm1 (biker cop) correctly if the script requests one with PEDTYPE_COP =============
namespace GetCorrectPedModel_Lapdm1
{
	__declspec(naked) static void BikerCop_Retail()
	{
		_asm
		{
			cmp		dword ptr [esp+4], 6
			jnz		BikerCop_Return
			mov		dword ptr [eax], 1

		BikerCop_Return:
			ret		8
		}
	}

	__declspec(naked) static void BikerCop_Steam()
	{
		_asm
		{
			cmp		dword ptr [ebp+8], 6
			jnz		BikerCop_Return
			mov		dword ptr [eax], 1

		BikerCop_Return:
			pop		ebp
			ret		8
		}
	}
}


// ============= Only allow impounding cars and bikes (and their subclasses), as impounding helicopters, planes, boats makes no sense =============
namespace RestrictImpoundVehicleTypes
{
	template<std::size_t Index>
	static bool (*orgIsThisVehicleInteresting)(CVehicle* vehicle);

	template<std::size_t Index>
	static bool IsThisVehicleInteresting_AndCanBeImpounded(CVehicle* vehicle)
	{
		return vehicle->CanThisVehicleBeImpounded() && orgIsThisVehicleInteresting<Index>(vehicle);
	}

	HOOK_EACH_INIT(ShouldImpound, orgIsThisVehicleInteresting, IsThisVehicleInteresting_AndCanBeImpounded)
}


// ============= Fix PlayerPed replay crashes =============
// 1. Crash when starting a mocap cutscene after playing a replay wearing different clothes to the ones CJ has currently
// 2. Crash when playing back a replay with a different motion group anim (fat/muscular/normal) than the current one
namespace ReplayPlayerPedCrashFixes
{
	static void (*orgRestoreStuffFromMem)();
	static void RestoreStuffFromMem_RebuildPlayer()
	{
		orgRestoreStuffFromMem();
		CClothes::RebuildPlayer(FindPlayerPed(), false);
	}

	static void LoadAllMotionGroupAnims()
	{
		// FLA compatibility
		static const int32_t animGroupIDOffset = *AddressByVersion<int32_t*>(0x5A814C + 2, { "81 C7 ? ? ? ? 57 E8 ? ? ? ? 83 C4 0C", 2 });

		RequestModel(GetAnimationBlockIndex("fat") + animGroupIDOffset, 18);
		RequestModel(GetAnimationBlockIndex("muscular") + animGroupIDOffset, 18);

		LoadAllRequestedModels(true);
	}

	static void (*orgRebuildPlayer)(CPlayerPed*, bool);
	static void RebuildPlayer_LoadAllMotionGroupAnims(CPlayerPed* ped, bool bForReplay)
	{
		orgRebuildPlayer(ped, bForReplay);
		LoadAllMotionGroupAnims();
	}
}


// ============= Fix planes spawning in places where they crash easily =============
// CPlane::FindPlaneCreationCoors passes the XY position to CCollision::CheckCameraCollisionBuildings,
// while the function accepts the sector numbers
namespace FindPlaneCreationCoorsFix
{
	static std::pair<int, int> GetSectorNumbersFromPosition(int posX, int posY)
	{
		// FLA compatibility
		// This function is used only for 1.0 as new binaries use a divisor and not a multiplier,
		// so it's fine to reference addresses directly!
		const float& SectorMultX = **(float**)(0x41ACED + 2);
		const float& SectorAddX = **(float**)(0x41ACF6 + 2);
		const float& SectorMultY = **(float**)(0x41AD3C + 2);
		const float& SectorAddY = **(float**)(0x41AD42 + 2);

		const int SectorX = static_cast<int>(posX * SectorMultX + SectorAddX);
		const int SectorY = static_cast<int>(posY * SectorMultY + SectorAddY);
		return {SectorX, SectorY};
	}

	static std::pair<int, int> GetSectorNumbersFromPosition_Steam(int posX, int posY)
	{
		// FLA compatibility (in case FLA ever supports the new binaries)
		static const struct {
			const double& SectorDivX;
			const double& SectorAddX;
			const double& SectorDivY;
			const double& SectorAddY;
		} SectorRefs = []() -> decltype(SectorRefs) {
			auto sectorDatas = hook::pattern("DC 35 ? ? ? ? DC 05 ? ? ? ? 83 EC 08 D9 5D EC").get_one();

			const double* SectorDivX = *sectorDatas.get<double*>(2);
			const double* SectorAddX = *sectorDatas.get<double*>(6 + 2);
			const double* SectorDivY = *sectorDatas.get<double*>(0x2B + 2);
			const double* SectorAddY = *sectorDatas.get<double*>(0x40 + 2);
			return {*SectorDivX, *SectorAddX, *SectorDivY, *SectorAddY};
		}();

		const int SectorX = static_cast<int>(posX / SectorRefs.SectorDivX + SectorRefs.SectorAddX);
		const int SectorY = static_cast<int>(posY / SectorRefs.SectorDivY + SectorRefs.SectorAddY);
		return {SectorX, SectorY};
	}

	static bool (*orgCheckCameraCollisionBuildings)(int sectorX, int sectorY, void*, void*, void*, void*);
	static bool CheckCameraCollisionBuildings_FixParams(int posX, int posY, void* a3, void* a4, void* a5, void* a6)
	{
		auto [sectorX, sectorY] = GetSectorNumbersFromPosition(posX, posY);
		return orgCheckCameraCollisionBuildings(sectorX, sectorY, a3, a4, a5, a6);
	}

	static bool CheckCameraCollisionBuildings_FixParams_Steam(int posX, int posY, void* a3, void* a4, void* a5, void* a6)
	{
		auto [sectorX, sectorY] = GetSectorNumbersFromPosition_Steam(posX, posY);
		return orgCheckCameraCollisionBuildings(sectorX, sectorY, a3, a4, a5, a6);
	}
}


// ============= Allow hovering on the Jetpack with Keyboard + Mouse controls =============
// Does not modify any other controls, only hovering
namespace JetpackKeyboardControlsHover
{
	static void (__thiscall *orgGetLookBehindForCar)(void*);

	static void* ProcessControlInput_DontHover;
	static void* ProcessControlInput_Hover;

	__declspec(naked) static void ProcessControlInput_HoverWithKeyboard()
	{
		_asm
		{
			mov		ecx, ebp
			call	orgGetLookBehindForCar
			test	al, al
			jnz		Hovering
			mov		ecx, ebp
			mov		byte ptr [esi+0xD], 0
			jmp		ProcessControlInput_DontHover

		Hovering:
			jmp		ProcessControlInput_Hover
		}
	}

	__declspec(naked) static void ProcessControlInput_HoverWithKeyboard_Steam()
	{
		_asm
		{
			mov		ecx, ebx
			call	orgGetLookBehindForCar
			test	al, al
			jnz		Hovering
			mov		ecx, ebx
			mov		byte ptr [edi+0xD], 0
			jmp		ProcessControlInput_DontHover

		Hovering:
			jmp		ProcessControlInput_Hover
		}
	}
}


// ============= During riots, don't target the player group during missions =============
// Fixes recruited homies panicking during Los Desperados and other riot-time missions
namespace RiotDontTargetPlayerGroupDuringMissions
{
	static void* SkipTargetting;
	static void* DontSkipTargetting;

	__declspec(naked) static void CheckIfInPlayerGroupAndOnAMission()
	{
		_asm
		{
			cmp     byte ptr [ebp+0x2D0], 1
			jne		NotInGroup
			call	IsPlayerOnAMission
			test	al, al
			jz		NotOnAMission
			jmp		SkipTargetting

		NotOnAMission:
			cmp     byte ptr [ebp+0x2D0], 1

		NotInGroup:
			jmp		DontSkipTargetting
		}
	}

	__declspec(naked) static void CheckIfInPlayerGroupAndOnAMission_Steam()
	{
		_asm
		{
			cmp     byte ptr [ebx+0x2D0], 1
			jne		NotInGroup
			call	IsPlayerOnAMission
			test	al, al
			jz		NotOnAMission
			jmp		SkipTargetting

		NotOnAMission:
			cmp     byte ptr [ebx+0x2D0], 1

		NotInGroup:
			jmp		DontSkipTargetting
		}
	}
}


// ============= Fixed vehicles exploding twice if the driver leaves the car while it's exploding =============
namespace RemoveDriverStatusFix
{
	__declspec(naked) static void RemoveDriver_SetStatus()
	{
		// if (m_nStatus != STATUS_WRECKED)
		//   m_nStatus = STATUS_ABANDONED;
		_asm
		{
			mov		bl, [edi+0x36]
			mov		al, bl
			and		bl, 0xF8
			cmp		bl, 0x28
			je		DontSetStatus
			and     al, 7
			or      al, 0x20

		DontSetStatus:
			ret
		}
	}

	static void (__thiscall *orgPrepareVehicleForPedExit)(CTaskComplexCarSlowBeDraggedOut* task, CPed* ped);
	static void __fastcall PrepareVehicleForPedExit_WreckedCheck(CTaskComplexCarSlowBeDraggedOut* task, void*, CPed* ped)
	{
		if (task->m_pVehicle->GetStatus() != STATUS_WRECKED)
		{
			orgPrepareVehicleForPedExit(task, ped);
		}
	}
}


// ============= Fixed shooting stars rendering black =============
namespace ShootingStarsFix
{
	static void* (*orgRwIm3DTransform)(RwIm3DVertex* pVerts, RwUInt32 numVerts, RwMatrix* ltm, RwUInt32 flags);
	static void* RwIm3DTransform_UnsetTexture(RwIm3DVertex* pVerts, RwUInt32 numVerts, RwMatrix* ltm, RwUInt32 flags)
	{
		RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
		return orgRwIm3DTransform(pVerts, numVerts, ltm, flags);
	}
}


// ============= Enable directional lights on flying car components =============
namespace LitFlyingComponents
{
	static void (*orgWorldAdd)(CEntity*);
	static void WorldAdd_SetLightObjectFlag(CEntity* entity)
	{
		entity->bLightObject = true;
		orgWorldAdd(entity);
	}
}


// ============= Make script randomness 16-bit, like on PS2 =============
namespace Rand16bit
{
	template<std::size_t Index>
	static int (*orgRand)();

	template<std::size_t Index>
	static int rand16bit()
	{
		const int bottomBits = orgRand<Index>();
		const int topBit = (orgRand<Index>() & 1) << 15;
		return bottomBits | topBit;
	}

	HOOK_EACH_INIT(Rand, orgRand, rand16bit);
}


// ============= Improved resolution selection dialog =============
namespace NewResolutionSelectionDialog
{
	static IDirect3D9** ppRWD3D9;
	static void* FrontEndMenuManager;
	static char* (*orgGetDocumentsPath)();

	static constexpr const char* SettingsFileName = "device_remembered.set";

	static bool ShouldSkipDeviceSelection()
	{
		char cTmpPath[MAX_PATH];
		PathCombineA(cTmpPath, orgGetDocumentsPath(), SettingsFileName);

		bool bSkip = false;

		FILE* hFile = nullptr;
		if (fopen_s(&hFile, cTmpPath, "r") == 0)
		{
			unsigned int val = 0;
			bSkip = fscanf_s(hFile, "%u", &val) == 1 && val != 0;
			fclose(hFile);
		}
		return bSkip;
	}

	static void RememberDeviceSelection(bool bDoNotShowAgain)
	{
		char cTmpPath[MAX_PATH];
		PathCombineA(cTmpPath, orgGetDocumentsPath(), SettingsFileName);

		FILE* hFile = nullptr;
		if (fopen_s(&hFile, cTmpPath, "w") == 0)
		{
			fprintf_s(hFile, "%u", bDoNotShowAgain ? 1 : 0);
			fclose(hFile);
		}
	}

	static RwSubSystemInfo* (*orgRwEngineGetSubSystemInfo)(RwSubSystemInfo *subSystemInfo, RwInt32 subSystemIndex);
	static RwSubSystemInfo *RwEngineGetSubSystemInfo_GetFriendlyNames(RwSubSystemInfo *subSystemInfo, RwInt32 subSystemIndex)
	{
		// If we can't do any our work, fall back to the original game functions that may already by customized by other mods
		if (*ppRWD3D9 == nullptr)
		{
			return orgRwEngineGetSubSystemInfo(subSystemInfo, subSystemIndex);
		}

		D3DADAPTER_IDENTIFIER9 identifier;
		if (FAILED((*ppRWD3D9)->GetAdapterIdentifier(subSystemIndex, 0, &identifier)))
		{
			return orgRwEngineGetSubSystemInfo(subSystemInfo, subSystemIndex);
		}

		static const auto friendlyNames = FriendlyMonitorNames::GetNamesForDevicePaths();

		// If we can't find the friendly name, either because it doesn't exist or we're on an ancient Windows, fall back to the device name
		auto it = friendlyNames.find(identifier.DeviceName);
		if (it != friendlyNames.end())
		{
			strncpy_s(subSystemInfo->name, it->second.c_str(), _TRUNCATE);
		}
		else
		{
			strncpy_s(subSystemInfo->name, identifier.Description, _TRUNCATE);
		}

		return subSystemInfo;
	}

	static size_t MenuManagerAdapterOffset = 0xDC;
	static RwInt32 (*orgRwEngineGetCurrentSubSystem)();
	static RwInt32 RwEngineGetCurrentSubSystem_FromSettings()
	{
		// If we can't do any our work, fall back to the original game functions that may already by customized by other mods
		if (*ppRWD3D9 == nullptr)
		{
			return orgRwEngineGetCurrentSubSystem();
		}

		RwInt32 subSystem = *reinterpret_cast<RwInt32*>(static_cast<char*>(FrontEndMenuManager) + MenuManagerAdapterOffset);
		if (subSystem > 0)
		{
			// Force the device selection dialog to show again if anything is wrong
			bool bResetDisplay = false;
			if ((*ppRWD3D9)->GetAdapterCount() <= (UINT)subSystem)
			{
				subSystem = 0;
				bResetDisplay = true;
			}
			if (RwEngineSetSubSystem(subSystem) == FALSE || bResetDisplay)
			{
				RememberDeviceSelection(false);
				return 0;
			}
		}
		return subSystem;
	}

	static void CreateNewButtonTooltip(HINSTANCE hInstance, HWND hDlg)
	{
		HWND hCheckbox = GetDlgItem(hDlg, IDC_REMEMBERRESCHOICE);
		HWND hwndTip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
						hDlg, nullptr, hInstance, nullptr);
		
		if (hCheckbox == nullptr || hwndTip == nullptr)
		{
			return;
		}

		TOOLINFO toolInfo { sizeof(toolInfo) };
		toolInfo.hwnd = hDlg;
		toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
		toolInfo.uId = (UINT_PTR)hCheckbox;
		toolInfo.lpszText = (LPWSTR)TEXT("Delete 'device_remembered.set' from GTA San Andreas User Files to show this dialog again.");

		SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
	}

	struct WrappedDialocFunc
	{
		DLGPROC lpDialogFunc;
		LPARAM dwInitParam;
	};
	static INT_PTR CALLBACK CustomDlgProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (msg == WM_INITDIALOG)
		{
			const WrappedDialocFunc* data = reinterpret_cast<WrappedDialocFunc*>(lParam);
			SetWindowLongPtr(window, DWLP_USER, reinterpret_cast<LONG_PTR>(data->lpDialogFunc));

			data->lpDialogFunc(window, msg, wParam, data->dwInitParam);

			// The stock dialog func loaded the selected adapter and resolution at this point,
			// we can bail if we don't need to show the dialog
			if (ShouldSkipDeviceSelection())
			{
				// The game inits the selected resolution weirdly, and corrects it in the IDOK handler
				// so let's invoke it manually (bleh)
				data->lpDialogFunc(window, WM_COMMAND, IDOK, 0);
				return FALSE;
			}

			HMODULE hGameModule = GetModuleHandle(nullptr);
			SendMessage(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(LoadIcon(hGameModule, MAKEINTRESOURCE(100))));
			CreateNewButtonTooltip(hGameModule, window);

			// Return TRUE instead of FALSE on init, as we removed a manual SetFocus from the init function
			// and we want to rely on Windows to give us focus.
			return TRUE;
		}

		// Custom handling for IDCANCEL (IDOK is fine)
		if (msg == WM_COMMAND)
		{
			if (LOWORD(wParam) == IDCANCEL)
			{
				EndDialog(window, 0);
				return TRUE;
			}

			// Just remember the selection, let the game handle the rest
			if (LOWORD(wParam) == IDOK && IsDlgButtonChecked(window, IDC_REMEMBERRESCHOICE) == BST_CHECKED)
			{
				RememberDeviceSelection(true);
			}
		}

		DLGPROC origProc = reinterpret_cast<DLGPROC>(GetWindowLongPtr(window, DWLP_USER));
		if (origProc != nullptr)
		{
			return origProc(window, msg, wParam, lParam);
		}
		return FALSE;
	}

	static INT_PTR WINAPI DialogBoxParamA_New(HINSTANCE /*hInstance*/, LPCSTR /*lpTemplateName*/, HWND /*hWndParent*/, DLGPROC lpDialogFunc, LPARAM dwInitParam)
	{
		int32_t (WINAPI *pSetThreadDpiAwarenessContext)(int32_t dpiContext) = nullptr;
		int32_t oldDpiContext = 0;

		// Specify the dialog as DPI unaware, so Windows scales it by itself
		HMODULE user32Module = LoadLibraryW(L"user32");
		if (user32Module != nullptr)
		{
			pSetThreadDpiAwarenessContext = (decltype(pSetThreadDpiAwarenessContext))GetProcAddress(user32Module, "SetThreadDpiAwarenessContext");
		}

		if (pSetThreadDpiAwarenessContext != nullptr)
		{
			oldDpiContext = pSetThreadDpiAwarenessContext(/*DPI_AWARENESS_CONTEXT_UNAWARE*/-1);
		}

		ACTCTX actCtx { sizeof(actCtx) };
		actCtx.hModule = reinterpret_cast<HMODULE>(&__ImageBase);
		actCtx.lpResourceName = MAKEINTRESOURCE(2);
		actCtx.dwFlags = ACTCTX_FLAG_HMODULE_VALID | ACTCTX_FLAG_RESOURCE_NAME_VALID;
		
		ULONG_PTR cookie = 0;
		bool bContextActivated = false;

		HANDLE hActCtx = CreateActCtx(&actCtx);
		if (hActCtx != INVALID_HANDLE_VALUE)
		{
			bContextActivated = ActivateActCtx(hActCtx, &cookie) != FALSE;
		}

		// Include our own context to allow for custom message handling
		const WrappedDialocFunc origDlgProc { lpDialogFunc, dwInitParam };
		const INT_PTR result = DialogBoxParam(reinterpret_cast<HMODULE>(&__ImageBase), MAKEINTRESOURCE(IDD_RESSELECT), nullptr, CustomDlgProc, reinterpret_cast<LPARAM>(&origDlgProc));

		if (bContextActivated)
		{
			DeactivateActCtx(0, cookie);
		}
		if (hActCtx != INVALID_HANDLE_VALUE)
		{
			ReleaseActCtx(hActCtx);
		}

		if (pSetThreadDpiAwarenessContext != nullptr)
		{
			pSetThreadDpiAwarenessContext(oldDpiContext);
		}
		if (user32Module != nullptr)
		{
			FreeLibrary(user32Module);
		}
		return result;
	}
	static auto* const pDialogBoxParamA_New = &DialogBoxParamA_New;

	static HWND WINAPI SetFocus_NOP(HWND)
	{
		return nullptr;
	}
	static auto* const pSetFocus_NOP = &SetFocus_NOP;
}


// ============= Fix credits not scaling to resolution =============
// Also makes the shadow scale properly, as they haven't done that either...
// But since this seems to be the only place, don't pull in the fix from Vice City,
// fix it here instead
namespace CreditsScalingFixes
{
	static const unsigned int FIXED_RES_HEIGHT_SCALE = 448;

	template<std::size_t Index>
	STATIC_INLINE void (*orgPrintString)(float,float,const wchar_t*);

	template<std::size_t Index>
	static void PrintString_ScaleY(float fX, float fY, const wchar_t* pText)
	{
		if constexpr (Index == 1)
		{
			// Fix the shadow X scale - the Y scale will be fixed below
			fX = fX + 1.0f - ScaleX(1.0f);
		}
		orgPrintString<Index>(fX, ScaleY(fY), pText);
	}

	static void (*orgSetScale)(float X, float Y);
	static void SetScale_ScaleToRes(float X, float Y)
	{
		orgSetScale(ScaleX(X), ScaleY(Y));
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
			orgSetRightJustifyWrap<Index>(bSlidingEnabled ? ScaleX(-500.0f) : wrap);
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


// ============= Fix post effects not scaling correctly =============
// Heat haze not rescaling after changing resolution
// Water ripple effect having too high wave frequency at higher resolutions
namespace PostEffectsScalingFixes
{
	static int32_t* pHeatHazeFXTypeLast;

	// Instead of NOPing the call to SetupBackBufferVertex, redirect it to an empty function,
	// so other mods can chain with it fine
	static void (*orgSetupBackBufferVertex)();
	static void SetupBackBufferVertex_Nop()
	{
	}

	template<std::size_t Index>
	static void (*orgSetCurrentVideoMode)(int modeIndex);

	template<std::size_t Index>
	static void SetCurrentVideoMode_SetupPostFX(int modeIndex)
	{
		*pHeatHazeFXTypeLast = -1; // Force heat haze to reinit

		orgSetCurrentVideoMode<Index>(modeIndex);
		orgSetupBackBufferVertex();
	}

	HOOK_EACH_INIT(SetCurrentVideoMode, orgSetCurrentVideoMode, SetCurrentVideoMode_SetupPostFX);

	static void (*orgUnderWaterRipple)(RwRGBA, float, float, int, float, float);
	static void UnderWaterRipple_ScaleFrequency(RwRGBA a1, float xOffset, float yOffset, int a4, float a5, float frequency)
	{
		// Scale frequency counter-proportionally to the resolution height
		// as the function already scales the sine wave frequency to that internally.
		const float freqDivFactor = RsGlobal->MaximumHeight / 480.0f;
		orgUnderWaterRipple(a1, xOffset, yOffset, a4, a5, frequency / freqDivFactor);
	}
}


// ============= Fix heat seeking and gamepad crosshairs not scaling to resolution =============
namespace CrosshairScalingFixes
{
	template<std::size_t Index>
	static void (*orgRenderOneXLUSprite_Rotate_Aspect)(float, float, float, float, float, uint8_t, uint8_t, uint8_t, short, float, float, uint8_t);

	template<std::size_t Index>
	static void RenderOneXLUSprite_Rotate_Aspect_Scale(float a1, float a2, float a3, float width, float height, uint8_t a6, uint8_t a7, uint8_t a8, short a9, float a10, float a11, uint8_t a12)
	{
		orgRenderOneXLUSprite_Rotate_Aspect<Index>(a1, a2, a3, ScaleX(width), ScaleY(height), a6, a7, a8, a9, a10, a11, a12);
	}

	template<std::size_t Index>
	static const float* orgSize_GamepadCrosshair;

	template<std::size_t Index>
	static float Size_Recalculated_GamepadCrosshair;

	template<std::size_t... I>
	static void RecalculateSizes_GamepadCrosshair(std::index_sequence<I...>)
	{
		((Size_Recalculated_GamepadCrosshair<I> = ScaleY(*orgSize_GamepadCrosshair<I>)), ...);
	}

	template<std::size_t Index>
	static const double* orgSize_GamepadCrosshair_Double;

	template<std::size_t Index>
	static double Size_Recalculated_GamepadCrosshair_Double;

	template<std::size_t... I>
	static void RecalculateSizes_GamepadCrosshair_Double(std::index_sequence<I...>)
	{
		((Size_Recalculated_GamepadCrosshair_Double<I> = ScaleY(static_cast<float>(*orgSize_GamepadCrosshair_Double<I>))), ...);
	}

	static bool (*orgCalcScreenCoors)(const RwV3d&, RwV3d*, float*, float*, bool, bool);

	template<std::size_t NumFloats, std::size_t NumDoubles>
	static bool CalcScreenCoors_Recalculate(const RwV3d& a1, RwV3d* a2, float* a3, float* a4, bool a5, bool a6)
	{
		RecalculateSizes_GamepadCrosshair(std::make_index_sequence<NumFloats>{});
		RecalculateSizes_GamepadCrosshair_Double(std::make_index_sequence<NumDoubles>{});
		return orgCalcScreenCoors(a1, a2, a3, a4, a5, a6);
	}

	HOOK_EACH_INIT(RenderOneXLUSprite_Rotate_Aspect, orgRenderOneXLUSprite_Rotate_Aspect, RenderOneXLUSprite_Rotate_Aspect_Scale);
	HOOK_EACH_INIT(GamepadCrosshair, orgSize_GamepadCrosshair, Size_Recalculated_GamepadCrosshair);
	HOOK_EACH_INIT(GamepadCrosshair_Double, orgSize_GamepadCrosshair_Double, Size_Recalculated_GamepadCrosshair_Double);
}


// ============= Fix Map screen boundaries and the cursor not scaling to resolution =============
// Debugged by Wesser
namespace MapScreenScalingFixes
{	
	__declspec(naked) void ScaleX_NewBinaries()
	{
		_asm
		{
			push	ecx
			push	0x3F800000 // 1.0f
			call	[ScaleX]
			add		esp, 4

			fsub    st(1), st
			fxch    st(1)
			pop		ecx
			ret
		}
	}

	__declspec(naked) void ScaleY_NewBinaries()
	{
		_asm
		{
			push	ecx
			push	0x3F800000 // 1.0f
			call	[ScaleY]
			add		esp, 4

			fsub    st(1), st
			fxch    st(1)
			pop		ecx
			ret
		}
	}


	template<std::size_t Index>
	static const float* orgCursorXSize;

	template<std::size_t Index>
	static float CursorXSize_Recalculated;

	template<std::size_t... I>
	static void RecalculateXSize(std::index_sequence<I...>)
	{
		((CursorXSize_Recalculated<I> = ScaleX(*orgCursorXSize<I>)), ...);
	}

	template<std::size_t Index>
	static const float* orgCursorYSize;

	template<std::size_t Index>
	static float CursorYSize_Recalculated;

	template<std::size_t... I>
	static void RecalculateYSize(std::index_sequence<I...>)
	{
		((CursorYSize_Recalculated<I> = ScaleY(*orgCursorYSize<I>)), ...);
	}

	static void (*orgLimitToMap_Scale)(float* x, float* y);
	static void LimitToMap_Scale(float* x, float* y)
	{
		// LimitToMap assumes it's given scaled coordinates, but then the caller assumes it returns unscaled coordinates.
		// Need to scale them for the call, then unscale again, to save us from some assembly patching.
		const float XScale = ScaleX(1.0f);
		const float YScale = ScaleY(1.0f);

		*x *= XScale;
		*y *= YScale;
		orgLimitToMap_Scale(x, y);
		
		*x /= XScale;
		*y /= YScale;
	}

	static void (*orgLimitToMap_RecalculateSizes)(float* x, float* y);
	template<std::size_t NumXSize, std::size_t NumYSize>
	static void LimitToMap_RecalculateSizes(float* x, float* y)
	{
		orgLimitToMap_RecalculateSizes(x, y);
		RecalculateXSize(std::make_index_sequence<NumXSize>{});
		RecalculateYSize(std::make_index_sequence<NumYSize>{});
	}

	HOOK_EACH_INIT(CursorXSize, orgCursorXSize, CursorXSize_Recalculated);
	HOOK_EACH_INIT(CursorYSize, orgCursorYSize, CursorYSize_Recalculated);
}


// ============= Fix text background padding not scaling to resolution =============
// Debugged by Wesser
namespace TextRectPaddingScalingFixes
{
	template<std::size_t Index>
	static const float* orgPaddingXSize;

	template<std::size_t Index>
	static float PaddingXSize_Recalculated;

	template<std::size_t... I>
	static void RecalculateXSize(std::index_sequence<I...>)
	{
		((PaddingXSize_Recalculated<I> = ScaleX(*orgPaddingXSize<I>)), ...);
	}

	template<std::size_t Index>
	static const float* orgPaddingYSize;

	template<std::size_t Index>
	static float PaddingYSize_Recalculated;

	template<std::size_t... I>
	static void RecalculateYSize(std::index_sequence<I...>)
	{
		((PaddingYSize_Recalculated<I> = ScaleY(*orgPaddingYSize<I>)), ...);
	}

	template<std::size_t Index>
	static const double* orgPaddingYSize_Double;

	template<std::size_t Index>
	static double PaddingYSize_Double_Recalculated;

	template<std::size_t... I>
	static void RecalculateYSize_Double(std::index_sequence<I...>)
	{
		((PaddingYSize_Double_Recalculated<I> = ScaleY(static_cast<float>(*orgPaddingYSize_Double<I>))), ...);
	}

	static short (*orgProcessCurrentString)(uint8_t, float, float, void*);

	template<std::size_t NumXSize, std::size_t NumYSize>
	static short ProcessCurrentString_Scale(uint8_t a1, float a2, float a3, void* a4)
	{
		RecalculateXSize(std::make_index_sequence<NumXSize>{});
		RecalculateYSize(std::make_index_sequence<NumYSize>{});
		return orgProcessCurrentString(a1, a2, a3, a4);
	}

	template<std::size_t NumYSizeDouble>
	static short ProcessCurrentString_Scale_NewBinaries(uint8_t a1, float a2, float a3, void* a4)
	{
		RecalculateYSize_Double(std::make_index_sequence<NumYSizeDouble>{});
		return orgProcessCurrentString(a1, a2, a3, a4);
	}
	
	HOOK_EACH_INIT(PaddingXSize, orgPaddingXSize, PaddingXSize_Recalculated);
	HOOK_EACH_INIT(PaddingYSize, orgPaddingYSize, PaddingYSize_Recalculated);
	HOOK_EACH_INIT(PaddingYSize_Double, orgPaddingYSize_Double, PaddingYSize_Double_Recalculated);
}


// ============= Fix nitrous recharging faster when reversing the car =============
// By Wesser
namespace NitrousReverseRechargeFix
{
	__declspec(naked) static void NitrousControl_DontRechargeWhenReversing()
	{
		// x = 1.0f; \ if m_fGasPedal >= 0.0f x -= m_fGasPedal;
		_asm
		{
			fld		dword ptr [esi+0x49C]
			fldz
			fcomp   st(1)
			fnstsw  ax
			test    ah, 0x41
			jnz		BiggerOrEqual
			fstp	st
			ret

		BiggerOrEqual:
			fsubp   st(1), st
			ret
		}
	}

	__declspec(naked) static void NitrousControl_DontRechargeWhenReversing_NewBinaries()
	{
		_asm
		{
			fld		dword ptr [esi+0x49C]
			fldz
			fcomp   st(1)
			fnstsw  ax
			test    ah, 0x41
			jnz		BiggerOrEqual
			fstp	st
			fldz

		BiggerOrEqual:
			ret
		}
	}
}


// ============= Fix Hydra's jet thrusters not displaying due to an uninitialized variable in RwMatrix =============
// By B1ack_Wh1te
namespace JetThrustersFix
{
	// These are technically CMatrix, but for simplicity we use RwMatrix here
	template<std::size_t Index>
	static RwMatrix* (*orgMatrixMultiply)(RwMatrix* out, const RwMatrix* lhs, const RwMatrix* rhs);

	template<std::size_t Index>
	static RwMatrix* MatrixMultiply_ZeroFlags(RwMatrix* out, const RwMatrix* lhs, const RwMatrix* rhs)
	{
		RwMatrix* result = orgMatrixMultiply<Index>(out, lhs, rhs);

		// Technically, this should be the same as RwMatrixUpdate, but this variable is on the stack
		// and completely uninitialized, so zero it completely for consistent results.
		rwMatrixSetFlags(result, 0);

		return result;
	}

	HOOK_EACH_INIT(MatrixMultiply, orgMatrixMultiply, MatrixMultiply_ZeroFlags);
}


// ============= LS-RP Mode stuff =============
namespace LSRPMode
{
	struct IPv4
	{
		uint8_t ip[4];
		uint16_t port;

		friend bool operator == ( const IPv4& left, const IPv4& right )
		{
			return std::make_tuple( left.ip[0], left.ip[1], left.ip[2], left.ip[3] ) == std::make_tuple( right.ip[0], right.ip[1], right.ip[2], right.ip[3] ) &&
					( left.port == right.port || left.port == 0 || right.port == 0 );
		}
	};

	std::vector <IPv4> serversLSRPMode = {
		{ 149, 56, 123, 148, 7777 }, // LS-RP
		{ 198, 27, 95, 178, 7777 }, // AD:RP
	};

	bool ModeForced = false;
	void DetectPlayingOnLSRP()
	{
		IPv4 myIP = {};

		// Obtain IP and check if it's LS-RP
		int numArgs = 0;
		LPWSTR* cmdLine = CommandLineToArgvW( GetCommandLineW(), &numArgs );
		if ( cmdLine != nullptr )
		{
			for ( auto it = cmdLine + 1, end = cmdLine + numArgs; it != end; ++it )
			{
				if ( _wcsicmp( *it, L"-h" ) == 0 )
				{
					auto ipIt = std::next( it );
					if ( ipIt != end )
					{
						swscanf_s( *ipIt, L"%" SCNu8 ".%" SCNu8 ".%" SCNu8 ".%" SCNu8, &myIP.ip[0], &myIP.ip[1], &myIP.ip[2], &myIP.ip[3] );
						it = ipIt;
					}
					continue;
				}

				if ( _wcsicmp( *it, L"-p") == 0 )
				{
					auto portIt = std::next( it );
					if ( portIt != end )
					{
						swscanf_s( *portIt, L"%" SCNu16 , &myIP.port );
						it = portIt;
					}
					continue;
				}
			}

			LocalFree( cmdLine );
		}

		ModeForced = std::find( serversLSRPMode.begin(), serversLSRPMode.end(), myIP ) != serversLSRPMode.end();
	}

	void ReadServersList(const wchar_t* pPath)
	{
		constexpr size_t SCRATCH_PAD_SIZE = 32767;
		WideDelimStringReader reader( SCRATCH_PAD_SIZE );

		GetPrivateProfileSectionW( L"LSRPModeServers", reader.GetBuffer(), reader.GetSize(), pPath );
		while ( const wchar_t* str = reader.GetString() )
		{
			int ip[4] = {};
			int port = 0;
			
			// IP is mandatory, port is optional
			int argsRead = swscanf_s( str, L"%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3] );
			if ( argsRead == 4 )
			{
				swscanf_s( str, L"%*d.%*d.%*d.%*d:%d", &port );

				IPv4 myIP = {};
				bool validIP = true;

				for ( size_t i = 0; i < 4; i++ )
				{
					if ( ip[i] >= 0 && ip[i] <= UINT8_MAX )
					{
						myIP.ip[i] = static_cast<uint8_t>(ip[i]);
					}
					else
					{
						validIP = false;
						break;
					}
				}

				if ( port >= 0 && port <= UINT16_MAX )
				{
					myIP.port = static_cast<uint16_t>(port);
				}
				else
				{
					validIP = false;
				}

				if ( validIP )
				{
					serversLSRPMode.emplace_back( myIP );
				}
			}
		}
	}
}

static bool IgnoresWeaponPedsForPCFix()
{
	// TODO: Pre-emptively add INI option to save hassle in the future
	return LSRPMode::ModeForced;
}


namespace ModelIndicesReadyHook
{
	static void (*orgMatchAllModelStrings)();
	static void MatchAllModelStrings_ReadySVF()
	{
		orgMatchAllModelStrings();
		SVF::MarkModelNamesReady();
	}
}

#ifndef NDEBUG

// ============= QPC spoof for verifying high timer issues =============
namespace FakeQPC
{
	static int64_t AddedTime;
	static BOOL WINAPI FakeQueryPerformanceCounter(PLARGE_INTEGER lpPerformanceCount)
	{
		const BOOL result = ::QueryPerformanceCounter( lpPerformanceCount );
		lpPerformanceCount->QuadPart += AddedTime;
		return result;
	}
}

#endif

#if MEM_VALIDATORS

#include <intrin.h>

// Validator for static allocations
void PutStaticValidator( uintptr_t begin, uintptr_t end )
{
	uint8_t* a = (uint8_t*)begin;
	uint8_t* b = (uint8_t*)end;

	std::fill( a, b, uint8_t(0xCC) );
}

void* malloc_validator(size_t size)
{
	return _malloc_dbg( size, _NORMAL_BLOCK, "EXE", (uintptr_t)_ReturnAddress() );
}

void* realloc_validator(void* ptr, size_t size)
{
	return _realloc_dbg( ptr, size, _NORMAL_BLOCK, "EXE", (uintptr_t)_ReturnAddress() );
}

void* calloc_validator(size_t count, size_t size)
{
	return _calloc_dbg( count, size, _NORMAL_BLOCK, "EXE", (uintptr_t)_ReturnAddress() );
}

void free_validator(void* ptr)
{
	_free_dbg(ptr, _NORMAL_BLOCK);
}

size_t _msize_validator(void* ptr)
{
	return _msize_dbg(ptr, _NORMAL_BLOCK);
}

void* _new(size_t size)
{
	return _malloc_dbg( size, _NORMAL_BLOCK, "EXE", (uintptr_t)_ReturnAddress() );
}

void _delete(void* ptr)
{
	_free_dbg(ptr, _NORMAL_BLOCK);
}

class CDebugMemoryMgr
{
public:
	static void* Malloc(size_t size)
	{
		return _malloc_dbg( size, _NORMAL_BLOCK, "EXE", (uintptr_t)_ReturnAddress() );
	}

	static void Free(void* ptr)
	{
		_free_dbg(ptr, _NORMAL_BLOCK);
	}

	static void* Realloc(void* ptr, size_t size)
	{
		return _realloc_dbg( ptr, size, _NORMAL_BLOCK, "EXE", (uintptr_t)_ReturnAddress() );
	}

	static void* Calloc(size_t count, size_t size)
	{
		return _calloc_dbg( count, size, _NORMAL_BLOCK, "EXE", (uintptr_t)_ReturnAddress() );
	}

	static void* MallocAlign(size_t size, size_t align)
	{
		return _aligned_malloc_dbg( size, align, "EXE", (uintptr_t)_ReturnAddress() );
	}

	static void AlignedFree(void* ptr)
	{
		_aligned_free_dbg(ptr);
	}
};

void InstallMemValidator()
{
	using namespace Memory;

	// TEST: Validate memory
	InjectHook( AddressByRegion_10(0x824257), malloc_validator, HookType::Jump );
	InjectHook( AddressByRegion_10(0x824269), realloc_validator, HookType::Jump );
	InjectHook( AddressByRegion_10(0x824416), calloc_validator, HookType::Jump );
	InjectHook( AddressByRegion_10(0x82413F), free_validator, HookType::Jump );
	InjectHook( AddressByRegion_10(0x828C4A), _msize_validator, HookType::Jump );

	InjectHook( AddressByRegion_10(0x82119A), _new, HookType::Jump );
	InjectHook( AddressByRegion_10(0x8214BD), _delete, HookType::Jump );

	InjectHook( AddressByRegion_10(0x72F420), &CDebugMemoryMgr::Malloc, HookType::Jump );
	InjectHook( AddressByRegion_10(0x72F430), &CDebugMemoryMgr::Free, HookType::Jump );
	InjectHook( AddressByRegion_10(0x72F440), &CDebugMemoryMgr::Realloc, HookType::Jump );
	InjectHook( AddressByRegion_10(0x72F460), &CDebugMemoryMgr::Calloc, HookType::Jump );
	InjectHook( AddressByRegion_10(0x72F4C0), &CDebugMemoryMgr::MallocAlign, HookType::Jump );
	InjectHook( AddressByRegion_10(0x72F4F0), &CDebugMemoryMgr::AlignedFree, HookType::Jump );


	PutStaticValidator( AddressByRegion_10(0xAAE950), AddressByRegion_10(0xB4C310) ); // CStore
	PutStaticValidator( AddressByRegion_10(0xA9AE00), AddressByRegion_10(0xA9AE58) ); // fx_c
}

#endif


// Hooks
__declspec(naked) void LightMaterialsFix()
{
	_asm
	{
		mov     [esi], edi
		mov		ebx, [ecx]
		lea     esi, [edx+4]
		mov		[ebx+4], esi
		mov		edi, [esi]
		mov		[ebx+8], edi
		add		esi, 4
		mov		[ebx+12], esi
		mov		edi, [esi]
		mov		[ebx+16], edi
		add		ebx, 20
		mov		[ecx], ebx
		ret
	}
}

__declspec(naked) void UserTracksFix()
{
	_asm
	{
		push	[esp+4]
		call	SetVolume
		mov		ecx, [pUserTracksStuff]
		mov		byte ptr [ecx+0xD], 1
		call	InitializeUtrax
		ret		4
	}
}

__declspec(naked) void UserTracksFix_Steam()
{
	_asm
	{
		push	[esp+4]
		call	SetVolume
		mov		ecx, [pUserTracksStuff]
		mov		byte ptr [ecx+5], 1
		call	InitializeUtrax
		ret		4
	}
}

static void*	PlaneAtomicRendererSetup_JumpBack = AddressByVersion<void*>(0x4C7986, 0x4C7A06, 0x4D2275);
static void*	RenderVehicleHiDetailAlphaCB_BigVehicle = AddressByVersion<void*>(0x734370, 0x734BA0, 0x76E400);
static void*	RenderVehicleHiDetailCB_BigVehicle = AddressByVersion<void*>(0x733420, 0x733C50, 0x76D6C0);
__declspec(naked) void PlaneAtomicRendererSetup()
{
	static const char	aStaticProp[] = "static_prop";
	static const char	aMovingProp[] = "moving_prop";
	_asm
	{
		mov     eax, [esi+4]
		push	eax
		call	GetFrameNodeName
		mov		[esp+8+8], eax
		push	11
		push	offset aStaticProp
		push	eax
		call	strncmp
		add		esp, 0x10
		test	eax, eax
		jz		PlaneAtomicRendererSetup_Alpha
		push	11
		push	offset aMovingProp
		push	[esp+12+8]
		call	strncmp
		add		esp, 0xC
		test	eax, eax
		jnz		PlaneAtomicRendererSetup_NoAlpha

	PlaneAtomicRendererSetup_Alpha:
		push	RenderVehicleHiDetailAlphaCB_BigVehicle
		jmp		PlaneAtomicRendererSetup_Return

	PlaneAtomicRendererSetup_NoAlpha:
		push	RenderVehicleHiDetailCB_BigVehicle

	PlaneAtomicRendererSetup_Return:
		jmp		PlaneAtomicRendererSetup_JumpBack
	}
}

static unsigned int		nCachedCRC;

static void*	RenderVehicleHiDetailCB = AddressByVersion<void*>(0x733240, 0x733A70, 0x76D4C0);
static void*	RenderVehicleHiDetailAlphaCB = AddressByVersion<void*>(0x733F80, 0x7347B0, 0x76DFE0);
static void*	RenderHeliRotorAlphaCB = AddressByVersion<void*>(0x7340B0, 0x7348E0, 0x76E110);
static void*	RenderHeliTailRotorAlphaCB = AddressByVersion<void*>(0x734170, 0x7349A0, 0x76E1E0);
static void*	HunterTest_JumpBack = AddressByVersion<void*>(0x4C7914, 0x4C7994, 0x4D2203);

// strcmp can't be invoked from inline assembly block?
static int strcmp_wrap(const char *s1, const char *s2)
{
	return strcmp( s1, s2 );
}

__declspec(naked) void HunterTest()
{
	static const char	aDoorDummy[] = "door_lf_ok";
	static const char	aStaticRotor[] = "static_rotor";
	static const char	aStaticRotor2[] = "static_rotor2";
	static const char	aWindscreen[] = "windscreen";
	_asm
	{
		setnz	al
		movzx	di, al

		push	10
		push	offset aWindscreen
		push	ebp
		call	strncmp
		add		esp, 0xC
		test	eax, eax
		jz		HunterTest_RegularAlpha

		push	offset aStaticRotor2
		push	ebp
		call	strcmp_wrap
		add		esp, 8
		test	eax, eax
		jz		HunterTest_StaticRotor2AlphaSet

		push	offset aStaticRotor
		push	ebp
		call	strcmp_wrap
		add		esp, 8
		test	eax, eax
		jz		HunterTest_StaticRotorAlphaSet

		test	di, di
		jnz		HunterTest_DoorTest

		push	RenderVehicleHiDetailCB
		jmp		HunterTest_JumpBack

	HunterTest_DoorTest:
		cmp		nCachedCRC, 0x45D0B41C
		jnz		HunterTest_RegularAlpha
		push	offset aDoorDummy
		push	ebp
		call	strcmp_wrap
		add		esp, 8
		test	eax, eax
		jnz		HunterTest_RegularAlpha
		push	RenderVehicleHiDetailAlphaCB_HunterDoor
		jmp		HunterTest_JumpBack

	HunterTest_RegularAlpha:
		push	RenderVehicleHiDetailAlphaCB
		jmp		HunterTest_JumpBack

	HunterTest_StaticRotorAlphaSet:
		push	RenderHeliRotorAlphaCB
		jmp		HunterTest_JumpBack

	HunterTest_StaticRotor2AlphaSet:
		push	RenderHeliTailRotorAlphaCB
		jmp		HunterTest_JumpBack
	}
}
 
static void*	CacheCRC32_JumpBack = AddressByVersion<void*>(0x4C7B10, 0x4C7B90, 0x4D2400);
__declspec(naked) void CacheCRC32()
{
	_asm
	{
		mov		eax, [ecx+4]
		mov		nCachedCRC, eax
		jmp		CacheCRC32_JumpBack
	}
}

static void* const TrailerDoubleRWheelsFix_ReturnFalse = AddressByVersion<void*>(0x4C9333, 0x4C9533, 0x4D3C59);
static void* const TrailerDoubleRWheelsFix_ReturnTrue = AddressByVersion<void*>(0x4C9235, 0x4C9435, 0x4D3B59);
__declspec(naked) void TrailerDoubleRWheelsFix()
{
	_asm
	{
		cmp		[edi]CVehicleModelInfo.m_dwType, VEHICLE_TRAILER
		je		TrailerDoubleRWheelsFix_DoWheels
		cmp		eax, 2
		je		TrailerDoubleRWheelsFix_False
		cmp		eax, 5
		je		TrailerDoubleRWheelsFix_False

	TrailerDoubleRWheelsFix_DoWheels:
		jmp		TrailerDoubleRWheelsFix_ReturnTrue

	TrailerDoubleRWheelsFix_False:
		jmp		TrailerDoubleRWheelsFix_ReturnFalse
	}
}

__declspec(naked) void TrailerDoubleRWheelsFix2()
{
	_asm
	{
		add     esp, 0x18
		mov     eax, [ebx]
		mov     eax, [esi+eax+4]
		jmp		TrailerDoubleRWheelsFix
	}
}

__declspec(naked) void TrailerDoubleRWheelsFix_Steam()
{
	_asm
	{
		cmp		[esi]CVehicleModelInfo.m_dwType, VEHICLE_TRAILER
		je		TrailerDoubleRWheelsFix_DoWheels
		cmp		eax, 2
		je		TrailerDoubleRWheelsFix_False
		cmp		eax, 5
		je		TrailerDoubleRWheelsFix_False

TrailerDoubleRWheelsFix_DoWheels:
		jmp		TrailerDoubleRWheelsFix_ReturnTrue

TrailerDoubleRWheelsFix_False:
		jmp		TrailerDoubleRWheelsFix_ReturnFalse
	}
}

__declspec(naked) void TrailerDoubleRWheelsFix2_Steam()
{
	_asm
	{
		add     esp, 0x18
		mov     eax, [ebp]
		mov     eax, [ebx+eax+4]
		jmp		TrailerDoubleRWheelsFix_Steam
	}
}

static void*	LoadFLAC_JumpBack = AddressByVersion<void*>(0x4F3743, Memory::GetVersion().version == 1 ? (*(BYTE*)0x4F3A50 == 0x6A ? 0x4F3BA3 : 0x5B6B81) : 0, 0x4FFC3F);
__declspec(naked) void LoadFLAC()
{
	_asm
	{
		jz		LoadFLAC_WindowsMedia
		sub		ebp, 2
		jnz		LoadFLAC_Return
		push	esi
		call	DecoderCtor
		jmp		LoadFLAC_Success

	LoadFLAC_WindowsMedia:
		jmp		LoadFLAC_JumpBack

	LoadFLAC_Success:
		test	eax, eax
		mov		[esp+0x20+4], eax
		jnz		LoadFLAC_Return_NoDelete

	LoadFLAC_Return:
		mov		ecx, esi
		call	CAEDataStreamOld::~CAEDataStreamOld
		push	esi
		call	GTAdelete
		add     esp, 4

	LoadFLAC_Return_NoDelete:
		mov     eax, [esp+0x20+4]
		mov		ecx, [esp+0x20-0xC]
		pop		esi
		pop		ebp
		pop		edi
		pop		ebx
		mov		fs:0, ecx
		add		esp, 0x10
		ret		4
	}
}

// 1.01 securom butchered this func, might not be reliable
__declspec(naked) void LoadFLAC_11()
{
	_asm
	{
		jz		LoadFLAC_WindowsMedia
		sub		ebp, 2
		jnz		LoadFLAC_Return
		push	esi
		call	DecoderCtor
		jmp		LoadFLAC_Success

	LoadFLAC_WindowsMedia:
		jmp		LoadFLAC_JumpBack

	LoadFLAC_Success:
		test	eax, eax
		mov		[esp+0x20+4], eax
		jnz		LoadFLAC_Return_NoDelete

	LoadFLAC_Return:
		mov		ecx, esi
		call	CAEDataStreamNew::~CAEDataStreamNew
		push	esi
		call	GTAdelete
		add     esp, 4

	LoadFLAC_Return_NoDelete:
		mov     eax, [esp+0x20+4]
		mov		ecx, [esp+0x20-0xC]
		pop		esi
		pop		ebp
		pop		edi
		pop		ebx
		mov		fs:0, ecx
		add		esp, 0x10
		ret		4
	}
}


__declspec(naked) void LoadFLAC_Steam()
{
	_asm
	{
		jz		LoadFLAC_WindowsMedia
		sub		ebp, 2
		jnz		LoadFLAC_Return
		push	esi
		call	DecoderCtor
		jmp		LoadFLAC_Success

	LoadFLAC_WindowsMedia:
		jmp		LoadFLAC_JumpBack

	LoadFLAC_Success:
		test	eax, eax
		mov		[esp+0x20+4], eax
		jnz		LoadFLAC_Return_NoDelete

	LoadFLAC_Return:
		mov		ecx, esi
		call	CAEDataStreamOld::~CAEDataStreamOld
		push	esi
		call	GTAdelete
		add     esp, 4

	LoadFLAC_Return_NoDelete:
		mov     eax, [esp+0x20+4]
		mov		ecx, [esp+0x20-0xC]
		pop		ebx
		pop		esi
		pop		ebp
		pop		edi
		mov		fs:0, ecx
		add		esp, 0x10
		ret		4
	}
}

__declspec(naked) void FLACInit()
{
	_asm
	{
		mov		byte ptr [ecx+0xD], 1
		jmp		InitializeUtrax
	}
}

__declspec(naked) void FLACInit_Steam()
{
	_asm
	{
		mov		byte ptr [ecx+5], 1
		jmp		InitializeUtrax
	}
}


// 1.0 ONLY BEGINS HERE
static bool			bDarkVehicleThing;
static RpLight**	pDirect;

static void* DarkVehiclesFix1_JumpBack;
 __declspec(naked) void DarkVehiclesFix1()
{
	_asm
	{
		shr     eax, 0xE
		test	al, 1
		jz		DarkVehiclesFix1_DontApply
		mov		ecx, pDirect
		mov		ecx, [ecx]
		mov		al, [ecx+2]
		test	al, 1
		jnz		DarkVehiclesFix1_DontApply
		mov		bDarkVehicleThing, 1
		jmp		DarkVehiclesFix1_Return

	DarkVehiclesFix1_DontApply:
		mov		bDarkVehicleThing, 0

	DarkVehiclesFix1_Return:
		jmp		DarkVehiclesFix1_JumpBack
	}
}

__declspec(naked) void DarkVehiclesFix2()
{
	_asm
	{
		jz		DarkVehiclesFix2_MakeItDark
		mov		al, bDarkVehicleThing
		test	al, al
		jnz		DarkVehiclesFix2_MakeItDark
		mov		eax, 0x5D9A7A
		jmp		eax

	DarkVehiclesFix2_MakeItDark:
		mov		eax, 0x5D9B09
		jmp		eax
	}
}

__declspec(naked) void DarkVehiclesFix3()
{
	_asm
	{
		jz		DarkVehiclesFix3_MakeItDark
		mov		al, bDarkVehicleThing
		test	al, al
		jnz		DarkVehiclesFix3_MakeItDark
		mov		eax, 0x5D9B4A
		jmp		eax

	DarkVehiclesFix3_MakeItDark:
		mov		eax, 0x5D9CAC
		jmp		eax
	}
}

__declspec(naked) void DarkVehiclesFix4()
{
	_asm
	{
		jz		DarkVehiclesFix4_MakeItDark
		mov		al, bDarkVehicleThing
		test	al, al
		jnz		DarkVehiclesFix4_MakeItDark
		mov		eax, 0x5D9CB8
		jmp		eax

	DarkVehiclesFix4_MakeItDark:
		mov		eax, 0x5D9E0D
		jmp		eax
	}
}
// 1.0 ONLY ENDS HERE

NOBUFFERCHECKS static int _Timers_ftol_internal( double timer, double& remainder )
{
	double integral;
	remainder = modf( timer + remainder, &integral );
	return int(integral);
}

int __stdcall Timers_ftol_PauseMode( double timer )
{
	static double TimersRemainder = 0.0;
	return _Timers_ftol_internal( timer, TimersRemainder );
}

int __stdcall Timers_ftol_NonClipped( double timer )
{
	static double TimersRemainder = 0.0;
	return _Timers_ftol_internal( timer, TimersRemainder );
}

int __stdcall Timers_ftol( double timer )
{
	static double TimersRemainder = 0.0;
	return _Timers_ftol_internal( timer, TimersRemainder );
}

int __stdcall Timers_ftol_SCMdelta( double timer )
{
	static double TimersRemainder = 0.0;
	return _Timers_ftol_internal( timer, TimersRemainder );
}

__declspec(naked) void asmTimers_ftol_PauseMode()
{
	_asm
	{
		sub		esp, 8
		fstp	qword ptr [esp]
		call	Timers_ftol_PauseMode
		ret
	}
}

__declspec(naked) void asmTimers_ftol_NonClipped()
{
	_asm
	{
		sub		esp, 8
		fstp	qword ptr [esp]
		call	Timers_ftol_NonClipped
		ret
	}
}

__declspec(naked) void asmTimers_ftol()
{
	_asm
	{
		sub		esp, 8
		fstp	qword ptr [esp]
		call	Timers_ftol
		ret
	}
}

__declspec(naked) void asmTimers_SCMdelta()
{
	_asm
	{
		sub		esp, 8
		fstp	qword ptr [esp]
		call	Timers_ftol_SCMdelta
		ret
	}
}

__declspec(naked) void FixedCarDamage()
{
	_asm
	{
		fldz
		fcomp	dword ptr [esp+0x20+0x10]
		fnstsw  ax
		test    ah, 5
		jp		FixedCarDamage_Negative
		movzx   eax, byte ptr [edi+0x21]
		ret

	FixedCarDamage_Negative:
		movzx   eax, byte ptr [edi+0x24]
		ret
	}
}

__declspec(naked) void FixedCarDamage_Steam()
{
	_asm
	{
		fldz
		fcomp	dword ptr [esp+0x20+0x10]
		fnstsw  ax
		test    ah, 5
		jp		FixedCarDamage_Negative
		movzx   eax, byte ptr [edi+0x21]
		test	ecx, ecx
		ret

	FixedCarDamage_Negative:
		movzx   eax, byte ptr [edi+0x24]
		test	ecx, ecx
		ret
	}
}

__declspec(naked) void FixedCarDamage_Newsteam()
{
	_asm
	{
		mov		edi, [ebp+0x10]
		fldz
		fcomp	[ebp+0x14]
		fnstsw  ax
		test    ah, 5
		jp		FixedCarDamage_Negative
		movzx   eax, byte ptr [edi+0x21]
		ret

	FixedCarDamage_Negative:
		movzx   eax, byte ptr [edi+0x24]
		ret
	}
}

__declspec(naked) void CdStreamThreadHighSize()
{
	_asm
	{
		xor		edx, edx
		shld	edx, ecx, 11
		shl		ecx, 11
		mov		[esi]CdStream.overlapped.Offset, ecx // OVERLAPPED.Offset
		mov		[esi]CdStream.overlapped.OffsetHigh, edx // OVERLAPPED.OffsetHigh

		mov		edx, [esi]CdStream.nSectorsToRead
		ret
	}
}

__declspec(naked) void WeaponRangeMult_VehicleCheck()
{
	_asm
	{
		mov		eax, [edx]CPed.pedFlags
		test    ah, 1
		jz		WeaponRangeMult_VehicleCheck_NotInCar
		mov		eax, [edx]CPed.pVehicle
		ret
	
	WeaponRangeMult_VehicleCheck_NotInCar:
		xor		eax, eax
		ret
	}
}


static const float		fSteamSubtitleSizeX = 0.45f;
static const float		fSteamSubtitleSizeY = 0.9f;
static const float		fSteamRadioNamePosY = 33.0f;
static const float		fSteamRadioNameSizeX = 0.4f;
static const float		fSteamRadioNameSizeY = 0.6f;

static float* orgSubtitleSizeX;
static float* orgSubtitleSizeY;
static float* orgRadioNamePosY;
static float* orgRadioNameSizeX;
static float* orgRadioNameSizeY;

static void ToggleSteamTexts( bool enable )
{
	using namespace Memory::VP;

	if ( enable )
	{
		Patch<const void*>(0x58C387, &fSteamSubtitleSizeY);
		Patch<const void*>(0x58C40F, &fSteamSubtitleSizeY);
		Patch<const void*>(0x58C4CE, &fSteamSubtitleSizeY);

		Patch<const void*>(0x58C39D, &fSteamSubtitleSizeX);
		Patch<const void*>(0x58C425, &fSteamSubtitleSizeX);
		Patch<const void*>(0x58C4E4, &fSteamSubtitleSizeX);

		Patch<const void*>(0x4E9FD8, &fSteamRadioNamePosY);
		Patch<const void*>(0x4E9F22, &fSteamRadioNameSizeY);
		Patch<const void*>(0x4E9F38, &fSteamRadioNameSizeX);
	}
	else
	{
		assert( orgSubtitleSizeY != nullptr && orgSubtitleSizeX != nullptr && orgRadioNamePosY != nullptr && orgRadioNameSizeY != nullptr && orgRadioNameSizeX != nullptr );

		Patch<const void*>(0x58C387, orgSubtitleSizeY);
		Patch<const void*>(0x58C40F, orgSubtitleSizeY);
		Patch<const void*>(0x58C4CE, orgSubtitleSizeY);

		Patch<const void*>(0x58C39D, orgSubtitleSizeX);
		Patch<const void*>(0x58C425, orgSubtitleSizeX);
		Patch<const void*>(0x58C4E4, orgSubtitleSizeX);

		Patch<const void*>(0x4E9FD8, orgRadioNamePosY);
		Patch<const void*>(0x4E9F22, orgRadioNameSizeY);
		Patch<const void*>(0x4E9F38, orgRadioNameSizeX);
	}
}

static const double		dRetailSubtitleSizeX = 0.58;
static const double		dRetailSubtitleSizeY = 1.2;
static const double		dRetailSubtitleSizeY2 = 1.22;
static const double		dRetailRadioNamePosY = 22.0;
static const double		dRetailRadioNameSizeX = 0.6;
static const double		dRetailRadioNameSizeY = 0.9;

#pragma comment(lib, "shlwapi.lib")

BOOL InjectDelayedPatches_10()
{
	if ( !IsAlreadyRunning() )
	{
		using namespace Memory;

		const HINSTANCE hInstance = GetModuleHandle( nullptr );
		std::unique_ptr<ScopedUnprotect::Unprotect> Protect = ScopedUnprotect::UnprotectSectionOrFullModule( hInstance, ".text" );
		ScopedUnprotect::Section Protect2( hInstance, ".rdata" );

		// Obtain a path to the ASI
		wchar_t			wcModulePath[MAX_PATH];
		GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), wcModulePath, _countof(wcModulePath) - 3); // Minus max required space for extension
		PathRenameExtensionW(wcModulePath, L".ini");

		const ModuleList moduleList;

		const bool		bHasImVehFt = moduleList.Get(L"ImVehFt") != nullptr;
		const bool		bSAMP = moduleList.Get(L"samp") != nullptr;
		const bool		bSARender = moduleList.Get(L"SARender") != nullptr;
		const bool		bOutfit = moduleList.Get(L"outfit") != nullptr;
		const bool		bSAMPGraphicsRestore = moduleList.Get(L"SAMPGraphicRestore") != nullptr;

		if ( bSAMP )
		{
			LSRPMode::ReadServersList(wcModulePath);
			LSRPMode::DetectPlayingOnLSRP();
		}

		const HMODULE skygfxModule = moduleList.Get( L"skygfx" );
		const HMODULE modloaderModule = moduleList.Get( L"modloader" );

		ReadRotorFixExceptions(wcModulePath);
		ReadLightbeamFixExceptions(wcModulePath);
		const bool bHookDoubleRwheels = ReadDoubleRearWheels(wcModulePath);

		const bool bHasDebugMenu = DebugMenuLoad();

		auto PatchFloat = [](float** address, const float*& org, float& replaced)
			{
				org = *address;
				Patch(address, &replaced);
			};

		const std::initializer_list<uint8_t> fadd = { 0xD8, 0x05 };
		const std::initializer_list<uint8_t> fsub = { 0xD8, 0x25 };
		const std::initializer_list<uint8_t> fld = { 0xD9, 0x05 };

#ifdef _DEBUG
		if ( bHasDebugMenu )
		{
			DebugMenuAddVar( "SilentPatch", "Force LS-RP Mode", &LSRPMode::ModeForced, nullptr );
		}
#endif

		if ( GetPrivateProfileIntW(L"SilentPatch", L"SunSizeHack", -1, wcModulePath) == 1 )
		{
			// PS2 sun - more
			static const float		fSunMult = (1050.0f * 0.95f) / 1500.0f;
			Patch<const void*>(0x6FC5B0, &fSunMult);

			if ( !bSAMP )
			{
				ReadCall( 0x53C136, DoSunAndMoon );
				InjectHook(0x53C136, SunAndMoonFarClip);
				Patch<const void*>(0x6FC5AA, &fSunFarClip);
			}
		}
		
		if ( !bSARender )
		{
			// Twopass rendering (experimental)
			Patch<const void*>(0x7341D9, MovingPropellerRender);
			Patch<const void*>(0x734127, MovingPropellerRender);
			Patch(0x73445E, RenderBigVehicleActomic);


			// Weapons rendering
			if ( !bOutfit )
			{
				if ( bSAMP )
				{
					CPed::orgGetWeaponSkillForRenderWeaponPedsForPC = &CPed::GetWeaponSkillForRenderWeaponPedsForPC_SAMP;
				}

				InjectHook(0x5E7859, RenderWeapon);
				InjectHook(0x732F30, RenderWeaponPedsForPC, HookType::Jump);
			}
		}

		if ( GetPrivateProfileIntW(L"SilentPatch", L"EnableScriptFixes", -1, wcModulePath) == 1 )
		{
			using namespace ScriptFixes;

			// Gym glitch fix
			Patch<WORD>(0x470B03, 0xCD8B);
			Patch<DWORD>(0x470B0A, 0x8B04508B);
			Patch<WORD>(0x470B0E, 0x9000);
			Nop(0x470B10, 1);
			InjectHook(0x470B05, &CRunningScript::GetDay_GymGlitch, HookType::Call);

			// Basketball fix
			InterceptCall( 0x5D18F0, TheScriptsLoad, TheScriptsLoad_BasketballFix );
			
			std::array<uintptr_t, 2> wipeLocalVars = { 0x489A70, 0x4899F0 };
			HookEach_SCMFixes(wipeLocalVars, InterceptCall);
		}

		if ( GetPrivateProfileIntW(L"SilentPatch", L"SkipIntroSplashes", -1, wcModulePath) == 1 )
		{
			// Skip the damn intro splash
			Patch<WORD>(AddressByRegion_10<DWORD>(0x748AA8), 0x3DEB);
		}

		{
			static bool bSmallSteamTexts = false;
			if ( bHasDebugMenu )
			{
				orgSubtitleSizeX = *(float**)0x58C39D;
				orgSubtitleSizeY = *(float**)0x58C387;
				orgRadioNamePosY = *(float**)0x4E9FD8;
				orgRadioNameSizeY = *(float**)0x4E9F22;
				orgRadioNameSizeX = *(float**)0x4E9F38;

				DebugMenuAddVar( "SilentPatch", "Small Steam texts", &bSmallSteamTexts, []() {
					ToggleSteamTexts( bSmallSteamTexts );
				} );
			
			}

			if ( GetPrivateProfileIntW(L"SilentPatch", L"SmallSteamTexts", -1, wcModulePath) == 1 )
			{
				// We're on 1.0 - make texts smaller
				ToggleSteamTexts( true );

				bSmallSteamTexts = true;
			}
		}

		if ( const int INIoption = GetPrivateProfileIntW(L"SilentPatch", L"ColouredZoneNames", -1, wcModulePath); INIoption != -1 )
		{
			// Coloured zone names
			bColouredZoneNames = INIoption != 0;

			Patch<WORD>(0x58ADBE, 0x0E75);
			Patch<WORD>(0x58ADC5, 0x0775);

			InjectHook(0x58ADE4, &BlendGangColour_Dynamic);

			if ( bHasDebugMenu )
			{
				DebugMenuAddVar( "SilentPatch", "Coloured zone names", &bColouredZoneNames, nullptr );
			}
		}

		// ImVehFt conflicts
		if ( !bHasImVehFt )
		{
			// Lights
			InjectHook(0x4C830C, LightMaterialsFix, HookType::Call);

			// Flying components
			InjectHook(0x59F180, &CObject::Render_Stub, HookType::Jump);

			// Cars getting dirty
			// Only 1.0 and Steam
			InjectHook( 0x5D5DB0, RemapDirt, HookType::Jump );
			InjectHook(0x4C9648, &CVehicleModelInfo::FindEditableMaterialList, HookType::Call);
			Patch<DWORD>(0x4C964D, 0x0FEBCE8B);
		}

		if ( !bHasImVehFt && !bSAMP )
		{
			// Properly random numberplates
			DWORD*		pVMT = *(DWORD**)0x4C75FC;
			Patch(&pVMT[7], &CVehicleModelInfo::Shutdown_Stub);
			InjectHook(0x4C9660, &CVehicleModelInfo::SetCarCustomPlate);
			InjectHook(0x6D6A58, &CVehicle::CustomCarPlate_TextureCreate);
			InjectHook(0x6D651C, &CVehicle::CustomCarPlate_BeforeRenderingStart);
			InjectHook(0x6FDFE0, CCustomCarPlateMgr::SetupClumpAfterVehicleUpgrade, HookType::Jump);
			InjectHook(0x6D0E53, &CVehicle::CustomCarPlate_AfterRenderingStop);
			Nop(0x6D6517, 2);
			Nop(0x6D0E43, 2);
		}

		// SSE conflicts
		if ( moduleList.Get(L"shadows") == nullptr )
		{
			Patch<DWORD>(0x70665C, 0x52909090);
			InjectHook(0x706662, &CShadowCamera::Update);

			// Disable alpha test for stored shadows
			{
				using namespace StaticShadowAlphaFix;

				ReadCall( 0x53E0C8, orgRenderStoredShadows );
				InjectHook( 0x53E0C8, RenderStoredShadows_StateFix );
			}
		}

		// Bigger streamed entity linked lists
		// Increase only if they're not increased already
		if ( *(DWORD*)0x5B8E55 == 12000 )
		{
			Patch<DWORD>(0x5B8E55, 15000);
			Patch<DWORD>(0x5B8EB0, 15000);
		}

		// Read CCustomCarPlateMgr::GeneratePlateText from here
		// to work fine with Deji's Custom Plate Format
		ReadCall( 0x4C9484, CCustomCarPlateMgr::GeneratePlateText );


		if ( bHookDoubleRwheels )
		{
			// Double rwheels whitelist
			// push ecx
			// push edi
			// call CheckDoubleRWheelsWhitelist
			// test al, al
			Patch<uint16_t>( 0x4C9239, 0x5751 );
			InjectHook( 0x4C9239+2, CheckDoubleRWheelsList, HookType::Call );
			Patch<uint16_t>( 0x4C9239+7, 0xC084 );
			Nop( 0x4C9239+9, 1 );
		}

		if ( *(DWORD*)0x4065BB == 0x3B0BE1C1 )
		{
			// Handle IMGs bigger than 4GB
			Nop( 0x4065BB, 3 );
			Nop( 0x4065C2, 1 );
			InjectHook( 0x4065C2+1, CdStreamThreadHighSize, HookType::Call );
			Patch<const void*>( 0x406620+2, &pCdStreamSetFilePointer );
		}


		// Fix directional light position
		if ( const int INIoption = GetPrivateProfileIntW(L"SilentPatch", L"DirectionalFromSun", -1, wcModulePath); INIoption != -1 )
		{
			bUseAaronSun = INIoption != 0;

			ReadCall( 0x53E997, orgSetLightsWithTimeOfDayColour );
			InjectHook( 0x53E997, SetLightsWithTimeOfDayColour_SilentPatch );
			Patch<const void*>( 0x735618 + 2, &curVecToSun.x );
			Patch<const void*>( 0x73561E + 2, &curVecToSun.y );
			Patch<const void*>( 0x735624 + 1, &curVecToSun.z );

			if ( bHasDebugMenu )
			{
				DebugMenuAddVar( "SilentPatch", "Directional from sun", &bUseAaronSun, nullptr );

	#ifndef NDEBUG
				// Switch for fixed PC vehicle lighting
				static bool bFixedPCVehLight = true;
				DebugMenuAddVar( "SilentPatch", "Fixed PC vehicle light", &bFixedPCVehLight, []() {
					if ( bFixedPCVehLight )
					{
						Memory::VP::Patch<float>(0x5D88D1 + 6, 0);
						Memory::VP::Patch<float>(0x5D88DB + 6, 0);
						Memory::VP::Patch<float>(0x5D88E5 + 6, 0);

						Memory::VP::Patch<float>(0x5D88F9 + 6, 0);
						Memory::VP::Patch<float>(0x5D8903 + 6, 0);
						Memory::VP::Patch<float>(0x5D890D + 6, 0);
					}
					else
					{
						Memory::VP::Patch<float>(0x5D88D1 + 6, 0.25f);
						Memory::VP::Patch<float>(0x5D88DB + 6, 0.25f);
						Memory::VP::Patch<float>(0x5D88E5 + 6, 0.25f);

						Memory::VP::Patch<float>(0x5D88F9 + 6, 0.75f);
						Memory::VP::Patch<float>(0x5D8903 + 6, 0.75f);
						Memory::VP::Patch<float>(0x5D890D + 6, 0.75f);
					}
				} );
	#endif
			}
		}

		// Minimal HUD
		if ( const int INIoption = GetPrivateProfileIntW(L"SilentPatch", L"MinimalHUD", -1, wcModulePath); INIoption != -1 )
		{
			using namespace MinimalHUD;

			// Fix original bugs
			Patch( 0x58950E, { 0x90, 0xFF, 0x74, 0x24, 0x1C } );
			InjectHook( 0x58951D, &SetRGBA_FloatAlpha );

			Patch( 0x58D88A, { 0x90, 0xFF, 0x74, 0x24, 0x20 + 0x10 } );
			ReadCall( 0x58D8FD, orgRenderOneXLUSprite );
			InjectHook( 0x58D8FD, &RenderXLUSprite_FloatAlpha );
		
			// Re-enable
			if ( INIoption != 0 )
			{
				Patch<int32_t>( 0x588905 + 1, 0 );
			}
		
			if ( bHasDebugMenu )
			{
				static bool bMinimalHUDEnabled = INIoption != 0;
				DebugMenuAddVar( "SilentPatch", "Minimal HUD", &bMinimalHUDEnabled, []() {
					if ( bMinimalHUDEnabled )
					{
						Memory::VP::Patch<int32_t>( 0x588905 + 1, 0 );
					}
					else
					{
						Memory::VP::Patch<int32_t>( 0x588905 + 1, 5 );
					}

					// Call CHud::ReInitialise
					auto ReInitialise = (void(*)())0x588880;
					ReInitialise();
				} );
			}
		}

		// True invincibility - not being hurt by Police Maverick bullets anymore
		if ( !bSAMP )
		{
			int INIoption = GetPrivateProfileIntW(L"SilentPatch", L"TrueInvincibility", -1, wcModulePath);
			if (INIoption == -1)
			{
				// Minor spelling mistake, keep it for backwards compatibility with old INI files
				INIoption = GetPrivateProfileIntW(L"SilentPatch", L"TrueInvicibility", -1, wcModulePath);
			}
			if (INIoption != -1)
			{
				using namespace TrueInvincibility;

				isEnabled = INIoption != 0;
				WillKillJumpBack = 0x4B3238;
				InjectHook( 0x4B322E, ComputeWillKillPedHook, HookType::Jump );

				if ( bHasDebugMenu )
				{
					DebugMenuAddVar( "SilentPatch", "True invincibility", &isEnabled, nullptr );
				}
			}
		}

		// Moonphases
		// Not taking effect with new skygfx since aap has it too now
		if ( !bSAMP && !ModCompat::SkygfxPatchesMoonphases( skygfxModule ) )
		{
			using namespace MoonphasesFix;

			ReadCall( 0x713B74, orgRenderOneXLUSprite );
			InjectHook( 0x713C4C, RenderOneXLUSprite_MoonPhases );
		}

		FLAUtils::Init( moduleList );

		// Race condition in CdStream fixed
		// Not taking effect with modloader
		if ( !ModCompat::ModloaderCdStreamRaceConditionAware( modloaderModule ) )
		{
			// Don't patch if old FLA and enhanced IMGs are in place
			// For new FLA, we patch everything except CdStreamThread and then interop with FLA
			const bool flaBugAware = FLAUtils::CdStreamRaceConditionAware();
			const bool usesEnhancedImages = FLAUtils::UsesEnhancedIMGs();

			if ( !usesEnhancedImages || flaBugAware )
			{
				ReadCall( 0x406C78, CdStreamSync::orgCdStreamInitThread );
				InjectHook( 0x406C78, CdStreamSync::CdStreamInitThread );

				{
					const uintptr_t address = ModCompat::Utils::GetFunctionAddrIfRerouted(0x406460);

					const uintptr_t waitForSingleObject = address + 0x1D;
					const uint8_t orgCode[] = { 0x8B, 0x46, 0x04, 0x85, 0xC0, 0x74, 0x10, 0xC6, 0x46, 0x0D, 0x01 };
					if ( memcmp( orgCode, (void*)waitForSingleObject, sizeof(orgCode) ) == 0 )
					{
						VP::Patch( waitForSingleObject, { 0x56, 0xFF, 0x15 } );
						VP::Patch( waitForSingleObject + 3, &CdStreamSync::CdStreamSyncOnObject );
						VP::Patch( waitForSingleObject + 3 + 4, { 0x5E, 0xC3 } );

						{
							const uint8_t orgCode1[] = { 0xFF, 0x15 };
							const uint8_t orgCode2[] = { 0x48, 0xF7, 0xD8 };
							const uintptr_t getOverlappedResult = address + 0x5F;
							if ( memcmp( orgCode1, (void*)getOverlappedResult, sizeof(orgCode1) ) == 0 &&
								memcmp( orgCode2, (void*)(getOverlappedResult + 6), sizeof(orgCode2) ) == 0 )
							{
								VP::Patch( getOverlappedResult + 2, &CdStreamSync::pGetOverlappedResult );
								VP::Patch( getOverlappedResult + 6, { 0x5E, 0xC3 } ); // pop esi / retn
							}
						}
					}
				}

				if ( !usesEnhancedImages )
				{
					Patch( 0x406669, { 0x56, 0xFF, 0x15 } );
					Patch( 0x406669 + 3, &CdStreamSync::CdStreamThreadOnObject );
					Patch( 0x406669 + 3 + 4, { 0xEB, 0x0F } );
				}

				Patch( 0x406910, { 0xFF, 0x15 } );
				Patch( 0x406910 + 2, &CdStreamSync::CdStreamInitializeSyncObject );
				Nop( 0x406910 + 6, 4 );
				Nop( 0x406910 + 0x16, 2 );

				Patch( 0x4063B5, { 0x56, 0x50 } );
				InjectHook( 0x4063B5 + 2, CdStreamSync::CdStreamShutdownSyncObject_Stub, HookType::Call );
			}
		}

		// For imfast compatibility
		if ( MemEquals( 0x590ADE, { 0xFF, 0x05 } ) )
		{
			// Modulo over CLoadingScreen::m_currDisplayedSplash
			Nop( 0x590ADE, 1 );
			InjectHook( 0x590ADE + 1, DoPCScreenChange_Mod, HookType::Call );
			Patch<const void*>( 0x590042 + 2, &currDisplayedSplash_ForLastSplash );
		}

		// Lightbeam fix debug menu
		if ( bHasDebugMenu )
		{
			static const char * const str[] = { "Off", "Default", "On" };

			DebugMenuEntry *e = DebugMenuAddVar( "SilentPatch", "Rotors fix", &CVehicle::ms_rotorFixOverride, nullptr, 1, -1, 1, str);
			DebugMenuEntrySetWrap(e, true);

			if ( LightbeamFix::hookedSuccessfully )
			{
				e = DebugMenuAddVar( "SilentPatch", "Lightbeam fix", &CVehicle::ms_lightbeamFixOverride, nullptr, 1, -1, 1, str);
				DebugMenuEntrySetWrap(e, true);
			}
		}

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

		// Register CBaseModelInfo::GetModelInfo for SVF so we can resolve model names
		{
			using namespace ModelIndicesReadyHook;

			auto func = (void*(*)(const char*, int*))0x4C5940;

			InterceptCall(0x5B922F, orgMatchAllModelStrings, MatchAllModelStrings_ReadySVF);
			SVF::RegisterGetModelInfoCB(func);
		}


		// Disable building pipeline for skinned objects (like parachute)
		// SAMP Graphics Restore fixes the bug preventing this fix from working right
		if (!bSAMP || bSAMPGraphicsRestore)
		{
			using namespace SkinBuildingPipelineFix;

			InterceptCall(0x5D7F1E, orgCustomBuildingDNPipeline_CustomPipeAtomicSetup, CustomBuildingDNPipeline_CustomPipeAtomicSetup_Skinned);
		}


		// Fix some big messages staying on screen longer at high resolutions due to a cut sliding text feature
		// Also since we're touching it, optionally allow to re-enable this feature.
		if (const int INIoption = GetPrivateProfileIntW(L"SilentPatch", L"SlidingMissionTitleText", -1, wcModulePath); INIoption != -1)
		{
			using namespace SlidingTextsScalingFixes;

			pBigMessageX = *(std::array<float, 6>**)(0x58C919 + 2);

			std::array<uintptr_t, 1> slidingMessage1 = { 0x58D470 };

			std::array<uintptr_t, 1> textWrapFix = { 0x58D2A9 };

			BigMessageSlider<1>::bSlidingEnabled = INIoption != 0;
			BigMessageSlider<1>::HookEach_PrintString(slidingMessage1, InterceptCall);
			BigMessageSlider<1>::HookEach_RightJustifyWrap(textWrapFix, InterceptCall);

			if (bHasDebugMenu)
			{
				DebugMenuAddVar("SilentPatch", "Sliding mission title text", &BigMessageSlider<1>::bSlidingEnabled, nullptr);
			}
		}

		if (const int INIoption = GetPrivateProfileIntW(L"SilentPatch", L"SlidingOddJobText", -1, wcModulePath); INIoption != -1)
		{
			using namespace SlidingTextsScalingFixes;

			pOddJob2XOffset = *(float**)(0x58D077 + 2);

			std::array<uintptr_t, 1> slidingOddJob2 = { 0x58D21A };

			OddJob2Slider::bSlidingEnabled = INIoption != 0;
			OddJob2Slider::HookEach_PrintString(slidingOddJob2, InterceptCall);

			if (bHasDebugMenu)
			{
				DebugMenuAddVar("SilentPatch", "Sliding odd job text", &OddJob2Slider::bSlidingEnabled, nullptr);
			}
		}


		// Fix Map screen boundaries and the cursor not scaling to resolution
		// Debugged by Wesser
		// Moved here for compatibility with wshps.asi
		{
			using namespace MapScreenScalingFixes;

			// Even though those two patch the same function, treating them as separate patches makes retaining compatibility
			// with the widescreen fix easy.
			if (MemEquals(0x588251, fld) && MemEquals(0x588265, fadd) && MemEquals(0x5882A8, fld) && MemEquals(0x5882C6, fadd))
			{
				std::array<float**, 2> cursorXSizes = { (float**)(0x588251 + 2), (float**)(0x588265 + 2) };
				std::array<float**, 2> cursorYSizes = { (float**)(0x5882A8 + 2), (float**)(0x5882C6 + 2) };

				HookEach_CursorXSize(cursorXSizes, PatchFloat);
				HookEach_CursorYSize(cursorYSizes, PatchFloat);
				InterceptCall(0x58822D, orgLimitToMap_RecalculateSizes, LimitToMap_RecalculateSizes<cursorXSizes.size(), cursorYSizes.size()>);
			}

			// Only patch this function if wshps.asi hasn't changed the way it's being called
			// The expected code:
			// lea edx, [esp+70h+a1.y]
			// push edx
			// lea eax, [esp+74h+a1]
			// push eax
			if (MemEquals(0x588223, {0x8D, 0x54, 0x24, 0x4C, 0x52, 0x8D, 0x44, 0x24, 0x4C, 0x50 }))
			{
				InterceptCall(0x58822D, orgLimitToMap_Scale, LimitToMap_Scale);
			}
		}


		// Fix text background padding not scaling to resolution
		// Debugged by Wesser
		// Moved here for compatibility with wshps.asi
		{
			using namespace TextRectPaddingScalingFixes;

			// Verify that all fadd and fsub instructions are intact
			// Patterns would do it for us for free, but 1.0 does not use them...
			if (MemEquals(0x71A653, fadd) && MemEquals(0x71A66B, fadd) && MemEquals(0x71A69D, fsub) && MemEquals(0x71A6AB, fadd) &&
				MemEquals(0x71A6BF, fsub) && MemEquals(0x71A6EC, fadd))
			{
				std::array<float**, 4> paddingXSizes = {
					(float**)(0x71A653 + 2), (float**)(0x71A66B + 2),
					(float**)(0x71A69D + 2), (float**)(0x71A6AB + 2),
				};
				std::array<float**, 2> paddingYSizes = {
					(float**)(0x71A6BF + 2), (float**)(0x71A6EC + 2),
				};

				HookEach_PaddingXSize(paddingXSizes, PatchFloat);
				HookEach_PaddingYSize(paddingYSizes, PatchFloat);
				InterceptCall(0x71A631, orgProcessCurrentString, ProcessCurrentString_Scale<paddingXSizes.size(), paddingYSizes.size()>);
			}
		}


		// Fix credits not scaling to resolution
		// Moved here for compatibility with wshps.asi
		if (MemEquals(0x5A8679, {0xD8, 0xC1, 0xD8, 0x05}) && MemEquals(0x5A8679+8, {0xD8, 0x64, 0x24, 0x18, 0xD9, 0x54, 0x24, 0x14})) // Verify wshps.asi isn't already patching the credits
		{
			using namespace CreditsScalingFixes;

			std::array<uintptr_t, 2> creditPrintString = { 0x5A8707, 0x5A8785 };

			HookEach_PrintString(creditPrintString, InterceptCall);
			InterceptCall(0x5A86C0, orgSetScale, SetScale_ScaleToRes);

			// Fix the credits cutting off on the bottom early, they don't do that in III
			// but it regressed in VC and SA
			static const float topMargin = 1.0f;
			static const float bottomMargin = -(**(float**)(0x5A869A + 2));

			Patch(0x5A8689 + 2, &topMargin);
			Patch(0x5A869A + 2, &bottomMargin);

			// As we now scale everything on PrintString time, the resolution height checks need to be unscaled.
			Patch(0x5A8660 + 2, &FIXED_RES_HEIGHT_SCALE);
			Patch(0x5AF8C9 + 2, &FIXED_RES_HEIGHT_SCALE);
		}


#ifndef NDEBUG
		if ( const int QPCDays = GetPrivateProfileIntW(L"Debug", L"AddDaysToQPC", 0, wcModulePath); QPCDays != 0 )
		{
			using namespace FakeQPC;

			LARGE_INTEGER Freq;
			QueryPerformanceFrequency( &Freq );
			AddedTime = Freq.QuadPart * QPCDays * 60 * 24;

			Patch( 0x8580C8, &FakeQueryPerformanceCounter );
		}
#endif

		return FALSE;
	}
	return TRUE;
}

BOOL InjectDelayedPatches_11()
{
#ifdef NDEBUG
	const int messageResult = MessageBoxW( nullptr, L"You're using a 1.01 executable which is no longer supported by SilentPatch!\n\n"
				L"Since this EXE is used by only a few people, I recommend downgrading back to 1.0 - you gain full compatibility with mods "
				L"and any relevant fixes 1.01 brings are backported to 1.0 by SilentPatch anyway.\n\n"
				L"To downgrade to 1.0, find a 1.0 EXE online and replace your current game executable with it. Do you want to continue?\n\n"
				L"Pressing No will close the game. Press Yes to proceed to the game anyway.",
				L"SilentPatch", MB_OK | MB_ICONWARNING );
	if (messageResult == IDNO)
	{
		return TRUE;
	}
#endif

	if ( !IsAlreadyRunning() )
	{
		using namespace Memory;
		const HINSTANCE hInstance = GetModuleHandle( nullptr );
		std::unique_ptr<ScopedUnprotect::Unprotect> Protect = ScopedUnprotect::UnprotectSectionOrFullModule( hInstance, ".text" );
		ScopedUnprotect::Section Protect2( hInstance, ".rdata" );

		// Obtain a path to the ASI
		wchar_t			wcModulePath[MAX_PATH];
		GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), wcModulePath, _countof(wcModulePath) - 3); // Minus max required space for extension
		PathRenameExtensionW(wcModulePath, L".ini");

		const ModuleList moduleList;

		bool		bHasImVehFt = moduleList.Get(L"ImVehFt") != nullptr;
		bool		bSAMP = moduleList.Get(L"samp") != nullptr;
		bool		bSARender = moduleList.Get(L"SARender") != nullptr;
		const bool	bOutfit = moduleList.Get(L"outfit") != nullptr;

		if ( bSAMP )
		{
			LSRPMode::ReadServersList(wcModulePath);
			LSRPMode::DetectPlayingOnLSRP();
		}

		ReadRotorFixExceptions(wcModulePath);

		if ( GetPrivateProfileIntW(L"SilentPatch", L"SunSizeHack", -1, wcModulePath) == 1 )
		{
			// PS2 sun - more
			static const float		fSunMult = (1050.0f * 0.95f) / 1500.0f;
			Patch<const void*>(0x6FCDE0, &fSunMult);

			if ( !bSAMP )
			{
				ReadCall( 0x53C5D6, DoSunAndMoon );
				InjectHook(0x53C5D6, SunAndMoonFarClip);

				Patch<const void*>(0x6FCDDA, &fSunFarClip);
			}
		}

		if ( !bSARender )
		{
			// Twopass rendering (experimental)
			Patch<const void*>(0x734A09, MovingPropellerRender);
			Patch<const void*>(0x734957, MovingPropellerRender);
			Patch(0x734C8E, RenderBigVehicleActomic);

			// Weapons rendering
			if ( !bOutfit )
			{
				InjectHook(0x5E8079, RenderWeapon);
				InjectHook(0x733760, RenderWeaponPedsForPC, HookType::Jump);
			}
		}

		if ( GetPrivateProfileIntW(L"SilentPatch", L"EnableScriptFixes", -1, wcModulePath) == 1 )
		{
			using namespace ScriptFixes;

			// Gym glitch fix
			Patch<WORD>(0x470B83, 0xCD8B);
			Patch<DWORD>(0x470B8A, 0x8B04508B);
			Patch<WORD>(0x470B8E, 0x9000);
			Nop(0x470B90, 1);
			InjectHook(0x470B85, &CRunningScript::GetDay_GymGlitch, HookType::Call);

			// Basketball fix
			InterceptCall( 0x5D20D0, TheScriptsLoad, TheScriptsLoad_BasketballFix );

			std::array<uintptr_t, 2> wipeLocalVars = { 0x489A70, 0x489AF0 };
			HookEach_SCMFixes(wipeLocalVars, InterceptCall);
		}

		if ( GetPrivateProfileIntW(L"SilentPatch", L"SkipIntroSplashes", -1, wcModulePath) == 1 )
		{
			// Skip the damn intro splash
			Patch<WORD>(AddressByRegion_11<DWORD>(0x749388), 0x62EB);
		}

		if ( GetPrivateProfileIntW(L"SilentPatch", L"SmallSteamTexts", -1, wcModulePath) == 1 )
		{
			// We're on 1.01 - make texts smaller
			Patch<const void*>(0x58CB57, &fSteamSubtitleSizeY);
			Patch<const void*>(0x58CBDF, &fSteamSubtitleSizeY);
			Patch<const void*>(0x58CC9E, &fSteamSubtitleSizeY);

			Patch<const void*>(0x58CB6D, &fSteamSubtitleSizeX);
			Patch<const void*>(0x58CBF5, &fSteamSubtitleSizeX);
			Patch<const void*>(0x58CCB4, &fSteamSubtitleSizeX);

			Patch<const void*>(0x4EA428, &fSteamRadioNamePosY);
			Patch<const void*>(0x4EA372, &fSteamRadioNameSizeY);
			Patch<const void*>(0x4EA388, &fSteamRadioNameSizeX);
		}

		if ( int INIoption = GetPrivateProfileIntW(L"SilentPatch", L"ColouredZoneNames", -1, wcModulePath); INIoption == 1 )
		{
			// Coloured zone names
			Patch<WORD>(0x58B58E, 0x0E75);
			Patch<WORD>(0x58B595, 0x0775);

			InjectHook(0x58B5B4, &BlendGangColour);
		}
		else if ( INIoption == 0 )
		{
			Patch<BYTE>(0x58B57E, 0xEB);
		}

		// ImVehFt conflicts
		if ( !bHasImVehFt )
		{
			// Lights
			InjectHook(0x4C838C, LightMaterialsFix, HookType::Call);

			// Flying components
			InjectHook(0x59F950, &CObject::Render_Stub, HookType::Jump);
		}

		if ( !bHasImVehFt && !bSAMP )
		{
			// Properly random numberplates
			DWORD*		pVMT = *(DWORD**)0x4C767C;
			Patch(&pVMT[7], &CVehicleModelInfo::Shutdown_Stub);
			InjectHook(0x4C984D, &CVehicleModelInfo::SetCarCustomPlate);
			InjectHook(0x6D7288, &CVehicle::CustomCarPlate_TextureCreate);
			InjectHook(0x6D6D4C, &CVehicle::CustomCarPlate_BeforeRenderingStart);
			InjectHook(0x6FE810, CCustomCarPlateMgr::SetupClumpAfterVehicleUpgrade, HookType::Jump);
			Nop(0x6D6D47, 2);
		}

		// SSE conflicts
		if ( moduleList.Get(L"shadows") == nullptr )
		{
			Patch<DWORD>(0x706E8C, 0x52909090);
			InjectHook(0x706E92, &CShadowCamera::Update);
		}

		// Bigger streamed entity linked lists
		// Increase only if they're not increased already
		if ( *(DWORD*)0x5B9635 == 12000 )
		{
			Patch<DWORD>(0x5B9635, 15000);
			Patch<DWORD>(0x5B9690, 15000);
		}

		// Read CCustomCarPlateMgr::GeneratePlateText from here
		// to work fine with Deji's Custom Plate Format
		// Albeit 1.01 obfuscates this function
		CCustomCarPlateMgr::GeneratePlateText = (decltype(CCustomCarPlateMgr::GeneratePlateText))0x6FDDE0;
		
		FLAUtils::Init( moduleList );

		return FALSE;
	}
	return TRUE;
}

BOOL InjectDelayedPatches_Steam()
{
#ifdef NDEBUG
	{
		const int messageResult = MessageBoxW( nullptr, L"You're using a 3.0 executable which is no longer supported by SilentPatch!\n\n"
			L"Since this is an old Steam EXE, by now you should have either downgraded to 1.0 or started using an up to date version. It is recommended to "
			L"verify your game's cache on Steam and then downgrade it to 1.0. Do you want to continue?\n\n"
			L"Pressing No will close the game. Press Yes to proceed to the game anyway.", L"SilentPatch", MB_YESNO | MB_ICONWARNING );
		if (messageResult == IDNO)
		{
			return TRUE;
		}
	}
#endif

	if ( !IsAlreadyRunning() )
	{
		using namespace Memory;
		const HINSTANCE hInstance = GetModuleHandle( nullptr );
		std::unique_ptr<ScopedUnprotect::Unprotect> Protect = ScopedUnprotect::UnprotectSectionOrFullModule( hInstance, ".text" );
		ScopedUnprotect::Section Protect2( hInstance, ".rdata" );

		// Obtain a path to the ASI
		wchar_t			wcModulePath[MAX_PATH];
		GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), wcModulePath, _countof(wcModulePath) - 3); // Minus max required space for extension
		PathRenameExtensionW(wcModulePath, L".ini");

		const ModuleList moduleList;

		bool		bHasImVehFt = moduleList.Get(L"ImVehFt") != nullptr;
		bool		bSAMP = moduleList.Get(L"samp") != nullptr;
		bool		bSARender = moduleList.Get(L"SARender") != nullptr;
		const bool	bOutfit = moduleList.Get(L"outfit") != nullptr;

		if ( bSAMP )
		{
			LSRPMode::ReadServersList(wcModulePath);
			LSRPMode::DetectPlayingOnLSRP();
		}

		ReadRotorFixExceptions(wcModulePath);

		if ( GetPrivateProfileIntW(L"SilentPatch", L"SunSizeHack", -1, wcModulePath) == 1 )
		{
			// PS2 sun - more
			static const double		dSunMult = (1050.0 * 0.95) / 1500.0;
			Patch<const void*>(0x734DF0, &dSunMult);

			if ( !bSAMP )
			{
				ReadCall( 0x54E0B6, DoSunAndMoon );
				InjectHook(0x54E0B6, SunAndMoonFarClip);

				Patch<const void*>(0x734DEA, &fSunFarClip);
			}
		}

		if ( !bSARender )
		{
			// Twopass rendering (experimental)
			Patch<const void*>(0x76E230, MovingPropellerRender);
			Patch<const void*>(0x76E160, MovingPropellerRender);
			Patch(0x76E4F0, RenderBigVehicleActomic);


			// Weapons rendering
			if ( !bOutfit )
			{
				InjectHook(0x604DD9, RenderWeapon);
				InjectHook(0x76D170, RenderWeaponPedsForPC, HookType::Jump);
			}
		}

		if ( GetPrivateProfileIntW(L"SilentPatch", L"EnableScriptFixes", -1, wcModulePath) == 1 )
		{
			using namespace ScriptFixes;

			// Gym glitch fix
			Patch<WORD>(0x476C2A, 0xCD8B);
			Patch<DWORD>(0x476C31, 0x408B088B);
			Patch<WORD>(0x476C35, 0x9004);
			Nop(0x476C37, 1);
			InjectHook(0x476C2C, &CRunningScript::GetDay_GymGlitch, HookType::Call);

			// Basketball fix
			InterceptCall( 0x5EE017, TheScriptsLoad, TheScriptsLoad_BasketballFix );

			std::array<uintptr_t, 2> wipeLocalVars = { 0x4907AE, 0x49072E };
			HookEach_SCMFixes(wipeLocalVars, InterceptCall);
		}

		if ( GetPrivateProfileIntW(L"SilentPatch", L"SmallSteamTexts", -1, wcModulePath) == 0 )
		{
			// We're on Steam - make texts bigger
			Patch<const void*>(0x59A719, &dRetailSubtitleSizeY);
			Patch<const void*>(0x59A7B7, &dRetailSubtitleSizeY2);
			Patch<const void*>(0x59A8A1, &dRetailSubtitleSizeY2);

			Patch<const void*>(0x59A737, &dRetailSubtitleSizeX);
			Patch<const void*>(0x59A7D5, &dRetailSubtitleSizeX);
			Patch<const void*>(0x59A8BF, &dRetailSubtitleSizeX);

			Patch<const void*>(0x4F5A71, &dRetailRadioNamePosY);
			Patch<const void*>(0x4F59A1, &dRetailRadioNameSizeY);
			Patch<const void*>(0x4F59BF, &dRetailRadioNameSizeX);
		}

		if ( int INIoption = GetPrivateProfileIntW(L"SilentPatch", L"ColouredZoneNames", -1, wcModulePath); INIoption == 1 )
		{
			// Coloured zone names
			Patch<WORD>(0x598F65, 0x0C75);
			Patch<WORD>(0x598F6B, 0x0675);

			InjectHook(0x598F87, &BlendGangColour);
		}
		else if ( INIoption == 0 )
		{
			Patch<BYTE>(0x598F56, 0xEB);
		}

		// ImVehFt conflicts
		if ( !bHasImVehFt )
		{
			// Lights
			InjectHook(0x4D2C06, LightMaterialsFix, HookType::Call);

			// Flying components
			InjectHook(0x5B80E0, &CObject::Render_Stub, HookType::Jump);

			// Cars getting dirty
			// Only 1.0 and Steam
			InjectHook( 0x5F2580, RemapDirt, HookType::Jump );
			InjectHook(0x4D3F4D, &CVehicleModelInfo::FindEditableMaterialList, HookType::Call);
			Patch<DWORD>(0x4D3F52, 0x0FEBCE8B);
		}

		if ( !bHasImVehFt && !bSAMP )
		{
			// Properly random numberplates
			DWORD*		pVMT = *(DWORD**)0x4D1E9A;
			Patch(&pVMT[7], &CVehicleModelInfo::Shutdown_Stub);
			InjectHook(0x4D3F65, &CVehicleModelInfo::SetCarCustomPlate);
			InjectHook(0x711F28, &CVehicle::CustomCarPlate_TextureCreate);
			InjectHook(0x71194D, &CVehicle::CustomCarPlate_BeforeRenderingStart);
			InjectHook(0x736BD0, CCustomCarPlateMgr::SetupClumpAfterVehicleUpgrade, HookType::Jump);
			Nop(0x711948, 2);
		}

		// SSE conflicts
		if ( moduleList.Get(L"shadows") == nullptr )
		{
			Patch<DWORD>(0x74A864, 0x52909090);
			InjectHook(0x74A86A, &CShadowCamera::Update);
		}

		// Bigger streamed entity linked lists
		// Increase only if they're not increased already
		if ( *(DWORD*)0x5D5780 == 12000 )
		{
			Patch<DWORD>(0x5D5720, 1250);
			Patch<DWORD>(0x5D5780, 15000);
		}

		// Read CCustomCarPlateMgr::GeneratePlateText from here
		// to work fine with Deji's Custom Plate Format
		ReadCall( 0x4D3DA4, CCustomCarPlateMgr::GeneratePlateText );
		
		FLAUtils::Init( moduleList );

		return FALSE;
	}
	return TRUE;
}

BOOL InjectDelayedPatches_NewBinaries()
{
	if ( !IsAlreadyRunning() )
	{
		using namespace Memory;
		using namespace hook::txn;

		const HINSTANCE hInstance = GetModuleHandle( nullptr );
		std::unique_ptr<ScopedUnprotect::Unprotect> Protect = ScopedUnprotect::UnprotectSectionOrFullModule( hInstance, ".text" );
		ScopedUnprotect::Section Protect2( hInstance, ".rdata" );

		// Obtain a path to the ASI
		wchar_t			wcModulePath[MAX_PATH];
		GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), wcModulePath, _countof(wcModulePath) - 3); // Minus max required space for extension
		PathRenameExtensionW(wcModulePath, L".ini");

		const bool bHasDebugMenu = DebugMenuLoad();

		auto PatchDouble = [](double** address, const double*& org, double& replaced)
			{
				org = *address;
				Patch(address, &replaced);
			};

		// Race condition in CdStream fixed
		// Not taking effect with modloader
		//if ( !ModCompat::ModloaderCdStreamRaceConditionAware( modloaderModule ) )
		{
			// Don't patch if old FLA and enhanced IMGs are in place
			// For new FLA, we patch everything except CdStreamThread and then interop with FLA
			constexpr bool flaBugAware = false;
			constexpr bool usesEnhancedImages = false;

			if constexpr ( !usesEnhancedImages || flaBugAware ) try
			{
				void* initThread = get_pattern( "74 14 81 25 ? ? ? ? ? ? ? ? C7 05", 0x16 );
				auto cdStreamSync = pattern( "8B 0D ? ? ? ? 8D 04 40 03 C0" ).get_one(); // 0x4064E6
				auto cdStreamInitThread = pattern( "6A 00 6A 02 6A 00 6A 00 FF D3" ).get_one();
				auto cdStreamShutdown = pattern( "8B 4C 07 14" ).get_one();

				ReadCall( initThread, CdStreamSync::orgCdStreamInitThread );
				InjectHook( initThread, CdStreamSync::CdStreamInitThread );

				Patch( cdStreamSync.get<void>( 0x18 ), { 0x56, 0xFF, 0x15 } );
				Patch( cdStreamSync.get<void>( 0x18 + 3 ), &CdStreamSync::CdStreamSyncOnObject );
				Patch( cdStreamSync.get<void>( 0x18 + 3 + 4 ), { 0x5E, 0x5D, 0xC3 } ); // pop ebp / retn

				Patch( cdStreamSync.get<void>( 0x5E + 2 ), &CdStreamSync::pGetOverlappedResult );
				Patch( cdStreamSync.get<void>( 0x5E + 6 ), { 0x5E, 0x5D, 0xC3 } ); // pop esi / pop ebp / retn

				if constexpr ( !usesEnhancedImages ) try
				{
					auto cdStreamThread = pattern( "C7 46 04 00 00 00 00 8A 4E 0D" ).get_one();

					Patch( cdStreamThread.get<void>(), { 0x56, 0xFF, 0x15 } );
					Patch( cdStreamThread.get<void>( 3 ), &CdStreamSync::CdStreamThreadOnObject );
					Patch( cdStreamThread.get<void>( 3 + 4 ), { 0xEB, 0x17 } );
				}
				TXN_CATCH();

				Patch( cdStreamInitThread.get<void>(), { 0xFF, 0x15 } );
				Patch( cdStreamInitThread.get<void>( 2 ), &CdStreamSync::CdStreamInitializeSyncObject );
				Nop( cdStreamInitThread.get<void>( 6 ), 4 );
				Nop( cdStreamInitThread.get<void>( 0x16 ), 2 );

				Patch( cdStreamShutdown.get<void>(), { 0x56, 0x50 } );
				InjectHook( cdStreamShutdown.get<void>( 2 ), CdStreamSync::CdStreamShutdownSyncObject_Stub, HookType::Call );
			}
			TXN_CATCH();
		}


		// Fix some big messages staying on screen longer at high resolutions due to a cut sliding text feature
		// Also since we're touching it, optionally allow to re-enable this feature.
		if (const int INIoption = GetPrivateProfileIntW(L"SilentPatch", L"SlidingMissionTitleText", -1, wcModulePath); INIoption != -1) try
		{
			using namespace SlidingTextsScalingFixes;

			pBigMessageX = *get_pattern<std::array<float, 6>*>("8D 4E EC D9 05 ? ? ? ? 83 C4 04", 3 + 2);

			std::array<void*, 1> slidingMessage1 = {
				get_pattern("E8 ? ? ? ? 6A 00 E8 ? ? ? ? 83 C4 10 8B E5"),
			};

			std::array<void*, 1> textWrapFix = {
				get_pattern("E8 ? ? ? ? 6A 02 E8 ? ? ? ? 6A 03"),
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
			using namespace SlidingTextsScalingFixes;

			pOddJob2XOffset = *get_pattern<float*>("D9 05 ? ? ? ? DD 05 ? ? ? ? D8 F9", 2);

			std::array<void*, 1> slidingOddJob2 = {
				get_pattern("DB 45 08 D9 1C 24 E8 ? ? ? ? 83 C4 0C 8B E5 5D C3", 6),
			};

			OddJob2Slider::bSlidingEnabled = INIoption != 0;
			OddJob2Slider::HookEach_PrintString(slidingOddJob2, InterceptCall);

			if (bHasDebugMenu)
			{
				DebugMenuAddVar("SilentPatch", "Sliding odd job text", &OddJob2Slider::bSlidingEnabled, nullptr);
			}
		}
		TXN_CATCH();


		// Fix Map screen boundaries and the cursor not scaling to resolution
		// Debugged by Wesser
		// Moved here for compatibility with wshps.asi
		{
			using namespace MapScreenScalingFixes;

			// These two are entirely separate fixes
			try
			{
				// Updated the pattern to also ensure this function's arguments are not changed by other mods
				auto limitToMap = get_pattern("8D 45 F0 50 8B CA 51 E8 ? ? ? ? DB 05 ? ? ? ? 83 C4 1C", 7);
				InterceptCall(limitToMap, orgLimitToMap_Scale, LimitToMap_Scale);
			}
			TXN_CATCH();

			// Cursor X/Y scaling need to be done differently than in 1.0, as the game has one fld1 for width and one fld1 for height
			try
			{
				auto scaleX = pattern("D9 E8 DC E9 D9 C9 D9 5D 94").get_one();
				auto scaleY = pattern("D9 E8 DC E9 D9 C9 D9 5D B0").get_one();

				Nop(scaleX.get<void>(), 1);
				InjectHook(scaleX.get<void>(1), ScaleX_NewBinaries, HookType::Call);

				Nop(scaleY.get<void>(), 1);
				InjectHook(scaleY.get<void>(1), ScaleY_NewBinaries, HookType::Call);
			}
			TXN_CATCH();
		}


		// Fix text background padding not scaling to resolution
		// Debugged by Wesser
		// Moved here for compatibility with wshps.asi
		try
		{
			using namespace TextRectPaddingScalingFixes;

			auto processCurrentString = get_pattern("E8 ? ? ? ? DD 05 ? ? ? ? 8B 4D 08");

			// In new binaries, 4.0f is shared for width and height.
			// Make height determine the scale, so it works nicer in widescreen.
			auto paddingSize = pattern("DD 05 ? ? ? ? DC E9 D9 C9 D9 19").count(2);
			std::array<double**, 3> paddingYSizes = {
				get_pattern<double*>("D8 CA DD 05 ? ? ? ? DC C1", 2 + 2),
				paddingSize.get(0).get<double*>(2),
				paddingSize.get(1).get<double*>(2),
			};

			HookEach_PaddingYSize_Double(paddingYSizes, PatchDouble);
			InterceptCall(processCurrentString, orgProcessCurrentString, ProcessCurrentString_Scale_NewBinaries<paddingYSizes.size()>);
		}
		TXN_CATCH();


		// Fix credits not scaling to resolution
		// Moved here for compatibility with wshps.asi
		try
		{
			using namespace CreditsScalingFixes;

			// Verify wshps.asi isn't already patching the credits
			(void)get_pattern("DE C2 D9 45 18 DE EA", 1);

			std::array<void*, 2> creditPrintString = {
				get_pattern("E8 ? ? ? ? 83 C4 0C 80 7D 1C 00"),
				get_pattern("D9 1C 24 E8 ? ? ? ? DD 05 ? ? ? ? 83 C4 0C 5E", 3),
			};

			auto setScale = get_pattern("D9 1C 24 E8 ? ? ? ? 83 C4 08 68 FF 00 00 00 6A 00 6A 00 6A 00", 3);

			// Fix the credits cutting off on the bottom early, they don't do that in III
			// but it regressed in VC and SA
			auto positionOffset = get_pattern("DE C2 D9 45 18 DE EA D9 C9 D9 5D 14 D9 05", 12 + 2);

			// As we now scale everything on PrintString time, the resolution height checks need to be unscaled.
			void* resHeightScales[] = {
				get_pattern("DB 05 ? ? ? ? 57 8B 7D 14", 2),
				get_pattern("A1 ? ? ? ? 03 45 FC 89 45 F4", 1)
			};

			static const float topMargin = 1.0f;
			Patch(positionOffset, &topMargin);

			HookEach_PrintString(creditPrintString, InterceptCall);
			InterceptCall(setScale, orgSetScale, SetScale_ScaleToRes);

			for (void* addr : resHeightScales)
			{
				Patch(addr, &FIXED_RES_HEIGHT_SCALE);
			}
		}
		TXN_CATCH();


		return FALSE;
	}
	return TRUE;
}

static char		aNoDesktopMode[64];


void Patch_SA_10(HINSTANCE hInstance)
{
	using namespace Memory;

#if MEM_VALIDATORS
	InstallMemValidator();
#endif

	auto PatchFloat = [](float** address, const float*& org, float& replaced)
		{
			org = *address;
			Patch(address, &replaced);
		};

	// IsAlreadyRunning needs to be read relatively late - the later, the better
	{
		const uintptr_t pIsAlreadyRunning = AddressByRegion_10<uintptr_t>(0x74872D);
		ReadCall( pIsAlreadyRunning, IsAlreadyRunning );
		InjectHook(pIsAlreadyRunning, InjectDelayedPatches_10);
	}

	// Newsteam crash fix
	pDirect = *(RpLight***)0x5BA573;
	DarkVehiclesFix1_JumpBack = AddressByRegion_10<void*>(0x756D90);

	// (Hopefully) more precise frame limiter
	{
		uintptr_t pAddress = AddressByRegion_10<uintptr_t>(0x748D9B);
		ReadCall( pAddress, RsEventHandler );
		InjectHook(pAddress, NewFrameRender);
		InjectHook(AddressByRegion_10<uintptr_t>(0x748D1F), GetTimeSinceLastFrame);
	}

	// Set CAEDataStream to use an old structure
	CAEDataStream::SetStructType(false);

	//Patch<BYTE>(0x5D7265, 0xEB);

	// Heli rotors
	InjectHook(0x6CAB70, &CPlane::Render_Stub, HookType::Jump);
	InjectHook(0x6C4400, &CHeli::Render_Stub, HookType::Jump);

	// Boats
	/*Patch<BYTE>(0x4C79DF, 0x19);
	Patch<DWORD>(0x733A87, EXPAND_BOAT_ALPHA_ATOMIC_LISTS * sizeof(AlphaObjectInfo));
	Patch<DWORD>(0x733AD7, EXPAND_BOAT_ALPHA_ATOMIC_LISTS * sizeof(AlphaObjectInfo));*/

	// Fixed strafing? Hopefully
	/*static const float		fStrafeCheck = 0.1f;
	Patch<const void*>(0x61E0C2, &fStrafeCheck);
	Nop(0x61E0CA, 6);*/

	// RefFix
	static const float						fRefZVal = 1.0f;
	static const float* const				pRefFal = &fRefZVal;

	Patch<const void*>(0x6FB97A, &pRefFal);
	Patch<BYTE>(0x6FB9A0, 0);

	// Plane rotors
	InjectHook(0x4C7981, PlaneAtomicRendererSetup, HookType::Jump);

	// DOUBLE_RWHEELS
	Patch<WORD>(0x4C9290, 0xE281);
	Patch<int>(0x4C9292, ~(rwMATRIXTYPEMASK|rwMATRIXINTERNALIDENTITY));

	// A fix for DOUBLE_RWHEELS trailers
	InjectHook(0x4C9223, TrailerDoubleRWheelsFix, HookType::Jump);
	InjectHook(0x4C92F4, TrailerDoubleRWheelsFix2, HookType::Jump);

	// No framedelay
	Patch<WORD>(0x53E923, 0x43EB);
	Patch<BYTE>(0x53E99F, 0x10);
	Nop(0x53E9A5, 1);

	// Disable re-initialization of DirectInput mouse device by the game
	Patch<BYTE>(0x576CCC, 0xEB);
	Patch<BYTE>(0x576EBA, 0xEB);
	Patch<BYTE>(0x576F8A, 0xEB);

	// Make sure DirectInput mouse device is set non-exclusive (may not be needed?)
	Patch<DWORD>(AddressByRegion_10<DWORD>(0x7469A0), 0x9090C030);

	// Hunter interior & static_rotor for helis
	InjectHook(0x4C78F2, HunterTest, HookType::Jump);
	InjectHook(0x4C9618, CacheCRC32);

	// Fixed blown up car rendering
	// ONLY 1.0
	InjectHook(0x5D993F, DarkVehiclesFix1);
	InjectHook(0x5D9A74, DarkVehiclesFix2, HookType::Jump);
	InjectHook(0x5D9B44, DarkVehiclesFix3, HookType::Jump);
	InjectHook(0x5D9CB2, DarkVehiclesFix4, HookType::Jump);

	// Bindable NUM5
	// Only 1.0 and Steam
	Nop(0x57DC55, 2);


	// TEMP
	//Patch<DWORD>(0x733B05, 40);
	//Patch<DWORD>(0x733B55, 40);
	//Patch<BYTE>(0x5B3ADD, 4);

	// Lightbeam fix
	// We need to check for presence of old lightbeam fix - first validate everything old SP did
	if (	MemEquals( 0x6A2E95, { 0xFF, 0x52, 0x20 } ) && 
			MemEquals( 0x6E0F63, { 0xA1 } ) &&
			MemEquals( 0x6E0F7C, { 0x8B, 0x15 } ) &&
			MemEquals( 0x6E0F95, { 0x8B, 0x0D } ) &&
			MemEquals( 0x6E0FAF, { 0xA1 } ) &&
			MemEquals( 0x6E13D5, { 0xA1 } ) &&
			MemEquals( 0x6E13ED, { 0x8B, 0x15 } ) &&
			MemEquals( 0x6E141F, { 0xA1 } )	
		)
	{
		using namespace LightbeamFix;

		std::array<uintptr_t, 3> doHeadLightBeam = { 0x6A2EDA, 0x6A2EF2, 0x6BDE80 };
		CVehicle::HookEach_DoHeadLightBeam(doHeadLightBeam, InterceptCall);

		Patch( 0x6E0F37 + 2, &RenderStateWrapper<rwRENDERSTATEZWRITEENABLE>::PushStatePPtr );
		Patch( 0x6E0F63 + 1, &RenderStateWrapper<rwRENDERSTATEZTESTENABLE>::PushStatePPtr );
		Patch( 0x6E0F6F + 2, &RenderStateWrapper<rwRENDERSTATEVERTEXALPHAENABLE>::PushStatePPtr );
		Patch( 0x6E0F7C + 2, &RenderStateWrapper<rwRENDERSTATESRCBLEND>::PushStatePPtr );
		Patch( 0x6E0F89 + 1, &RenderStateWrapper<rwRENDERSTATEDESTBLEND>::PushStatePPtr );
		Patch( 0x6E0F95 + 2, &RenderStateWrapper<rwRENDERSTATESHADEMODE>::PushStatePPtr );
		// rwRENDERSTATETEXTURERASTER not saved
		Patch( 0x6E0FAF + 1, &RenderStateWrapper<rwRENDERSTATECULLMODE>::PushStatePPtr );
		Patch( 0x6E0FBB + 2, &RenderStateWrapper<rwRENDERSTATEALPHATESTFUNCTION>::PushStatePPtr );
		Patch( 0x6E0FCB + 2, &RenderStateWrapper<rwRENDERSTATEALPHATESTFUNCTIONREF>::PushStatePPtr );

		// rwRENDERSTATETEXTURERASTER not saved
		Patch( 0x6E13E0 + 2, &RenderStateWrapper<rwRENDERSTATEZWRITEENABLE>::PopStatePPtr );
		Patch( 0x6E13ED + 2, &RenderStateWrapper<rwRENDERSTATEZTESTENABLE>::PopStatePPtr );
		Patch( 0x6E13FA + 1, &RenderStateWrapper<rwRENDERSTATESRCBLEND>::PopStatePPtr );
		Patch( 0x6E1406 + 2, &RenderStateWrapper<rwRENDERSTATEDESTBLEND>::PopStatePPtr );
		Patch( 0x6E1413 + 2, &RenderStateWrapper<rwRENDERSTATEVERTEXALPHAENABLE>::PopStatePPtr );
		Patch( 0x6E141F + 1, &RenderStateWrapper<rwRENDERSTATECULLMODE>::PopStatePPtr );

		// Debug override registered in delayed patches
		hookedSuccessfully = true;
	}

	// PS2 SUN!!!!!!!!!!!!!!!!!
	Nop(0x6FB17C, 3);

#if defined EXPAND_ALPHA_ENTITY_LISTS
	// Bigger alpha entity lists
	Patch<DWORD>(0x733B05, EXPAND_ALPHA_ENTITY_LISTS * 20);
	Patch<DWORD>(0x733B55, EXPAND_ALPHA_ENTITY_LISTS * 20);
#endif

	// Unlocked widescreen resolutions
	//Patch<DWORD>(0x745B71, 0x9090687D);
	Patch<DWORD>(0x745B81, 0x9090587D);
	Patch<DWORD>(0x74596C, 0x9090127D);
	Nop(0x745970, 2);
	//Nop(0x745B75, 2);
	Nop(0x745B85, 2);
	Nop(0x7459E1, 2);

	// Heap corruption fix
	Nop(0x5C25D3, 5);

	// User Tracks fix
	ReadCall( 0x4D9B66, SetVolume );
	InjectHook(0x4D9B66, UserTracksFix);
	InjectHook(0x4D9BB5, 0x4F2FD0);

	// FLAC support
	InjectHook(0x4F373D, LoadFLAC, HookType::Jump);
	InjectHook(0x57BEFE, FLACInit);
	InjectHook(0x4F3787, CAEWaveDecoderInit);

	Patch<WORD>(0x4F376A, 0x18EB);
	//Patch<BYTE>(0x4F378F, sizeof(CAEWaveDecoder));
	Patch<const void*>(0x4F3210, UserTrackExtensions);
	Patch<const void*>(0x4F3241, &UserTrackExtensions->Codec);
	Patch<const void*>(0x4F35E7, &UserTrackExtensions[1].Codec);
	Patch<BYTE>(0x4F322D, sizeof(UserTrackExtensions));

	// Impound garages working correctly
	InjectHook(0x425179, 0x448990); // CGarages::IsPointWithinAnyGarage
	InjectHook(0x425369, 0x448990); // CGarages::IsPointWithinAnyGarage
	InjectHook(0x425411, 0x448990); // CGarages::IsPointWithinAnyGarage

	// Impounding after busted works
	Nop(0x443292, 5);

	// Mouse rotates an airbone car only with Steer with Mouse option enabled
	bool*	bEnableMouseSteering = *(bool**)0x6AD7AD; // CVehicle::m_bEnableMouseSteering
	Patch<bool*>(0x6B4EC0, bEnableMouseSteering);
	Patch<bool*>(0x6CE827, bEnableMouseSteering);

	// Patched CAutomobile::Fix
	// misc_x parts don't get reset (Bandito fix), Towtruck's bouncing panel is not reset
	Patch<WORD>(0x6A34C9, 0x5EEB);
	Patch<DWORD>(0x6A3555, 0x5E5FCF8B);
	Patch<DWORD>(0x6A3559, 0x448B5B5D);
	Patch<DWORD>(0x6A355D, 0x89644824);
	Patch<DWORD>(0x6A3561, 5);
	Patch<DWORD>(0x6A3565, 0x54C48300);
	InjectHook(0x6A3569, &CAutomobile::Fix_SilentPatch, HookType::Jump);

	// Patched CPlane::Fix
	// Doors don't get reset (they can't get damaged anyway), bouncing panels DO reset
	// but not on Vortex
	Patch<BYTE>(0x6CABD0, 0xEB);
	Patch<DWORD>(0x6CAC05, 0x5E5FCF8B);
	InjectHook(0x6CAC09, &CPlane::Fix_SilentPatch, HookType::Jump);

	// Weapon icon fix (crosshairs mess up rwRENDERSTATEZWRITEENABLE)
	// Only 1.0 and 1.01, Steam somehow fixed it (not the same way though)
	Nop(0x58E210, 3);
	Nop(0x58EAB7, 3);
	Nop(0x58EAE1, 3);	

	// Zones fix
	// Only 1.0 and Steam
	InjectHook(0x572130, GetCurrentZoneLockedOrUnlocked, HookType::Jump);

	// Bilinear filtering for license plates
	//Patch<BYTE>(0x6FD528, rwFILTERLINEAR);
	Patch<BYTE>(0x6FDF47, rwFILTERLINEAR);

	// -//- Roadsign maganer
	//Patch<BYTE>(0x6FE147, rwFILTERLINEAR);

	// Bilinear filtering with mipmaps for weapon icons
	Patch<BYTE>(0x58D7DA, rwFILTERMIPLINEAR);

	// Directional multiplier value from timecyc.dat properly using floats
	Patch<WORD>(0x5BBFC9, 0x14EB);

	// Directional multiplier defaults to 1.0f
	Patch( 0x5BBB04, { 0xC7, 0x84, 0x24, 0xCC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x90 } );

	// All lights get casted at vehicles
	Patch<BYTE>(0x5D9A88, 8);
	Patch<BYTE>(0x5D9A91, 8);
	Patch<BYTE>(0x5D9F1F, 8);

	// 6 extra directionals on Medium and higher
	// push eax
	// call GetMaxExtraDirectionals
	// add esp, 4
	// mov ebx, eax
	// nop
	Patch<uint8_t>( 0x735881, 0x50 );
	InjectHook( 0x735881 + 1, GetMaxExtraDirectionals, HookType::Call );
	Patch( 0x735881 + 6, { 0x83, 0xC4, 0x04, 0x8B, 0xD8 } );
	Nop( 0x735881 + 11, 3 );

	// Default resolution to native resolution
	const auto [width, height] = GetDesktopResolution();
	sprintf_s(aNoDesktopMode, "Cannot find %ux%ux32 video mode", width, height);

	if (width != 0 && height != 0)
	{
		Patch<DWORD>(0x746363, width);
		Patch<DWORD>(0x746368, height);
		Patch<const char*>(0x7463C8, aNoDesktopMode);
	}

	// Corrected Map screen 1px issue
	Patch<float>(0x575DE7, -0.5f);
	Patch<float>(0x575DA7, -0.5f);
	Patch<float>(0x575DAF, -0.5f);
	Patch<float>(0x575D5C, -0.5f);
	Patch<float>(0x575CDA, -0.5f);
	Patch<float>(0x575D0C, -0.5f);

	// Cars drive on water cheat
	Patch<DWORD>(&(*(DWORD**)0x438513)[34], 0xE5FC92C3);

	// No DirectPlay dependency
	// mov eax, 0x900
	Patch<BYTE>(AddressByRegion_10<DWORD>(0x74754A), 0xB8);
	Patch<DWORD>(AddressByRegion_10<DWORD>(0x74754B), 0x900);

	// SHGetFolderPath on User Files
	InjectHook(0x744FB0, GetMyDocumentsPathSA, HookType::Jump);

	// Fixed muzzleflash not showing from last bullet
	// nop \ test al, al \ jz
	Nop(0x61ECDC, 6);
	Patch(0x61ECE2, { 0x84, 0xC0, 0x74 });

	// Proper randomizations
	{
		using namespace ConsoleRandomness;

		InjectHook(0x44E82E, rand31); // Missing ped paths
		InjectHook(0x44ECEE, rand31); // Missing ped paths
		InjectHook(0x666EA0, rand31); // Prostitutes
	}

	// Help boxes showing with big message
	// Game seems to assume they can show together
	Nop(0x58BA8F, 6);

	// Fixed lens flare
	Patch<DWORD>(0x70F45A, 0); // TODO: Is this needed?
	Patch<BYTE>(0x6FB621, 0xC3); // nop CSprite::FlushSpriteBuffer
	// Add CSprite::FlushSpriteBuffer, jmp loc_6FB605 at the bottom of the function
	Patch<BYTE>(0x6FB600, 0x21);
	InjectHook(0x6FB622, 0x70CF20, HookType::Call);
	Patch<WORD>(0x6FB627, 0xDCEB);

	// nop / mov eax, offset FlushLensSwitchZ
	Patch<WORD>(0x6FB476, 0xB990);
	Patch(0x6FB478, &FlushLensSwitchZ);
	Patch<WORD>(0x6FB480, 0xD1FF);
	Nop(0x6FB482, 1);

	// nop / mov ecx, offset InitBufferSwitchZ
	Patch<WORD>(0x6FAF28, 0xB990);
	Patch(0x6FAF2A, &InitBufferSwitchZ);
	Patch<WORD>(0x6FAF32, 0xD1FF);
	Nop(0x6FAF34, 1);

	// Y axis sensitivity fix
	// By ThirteenAG
	float* sens = *(float**)0x50F03C;
	Patch<const void*>(0x50EB70 + 0x4D6 + 0x2, sens);
	Patch<const void*>(0x50F970 + 0x1B6 + 0x2, sens);
	Patch<const void*>(0x5105C0 + 0x666 + 0x2, sens);
	Patch<const void*>(0x511B50 + 0x2B8 + 0x2, sens);
	Patch<const void*>(0x521500 + 0xD8C + 0x2, sens);

	// Don't lock mouse Y axis during fadeins
	Patch<WORD>(0x50FBB4, 0x27EB);
	Patch<WORD>(0x510512, 0xE990);
	InjectHook(0x524071, 0x524139, HookType::Jump);

	// Fixed mirrors crash
	// TODO: Change when short jumps are supported
	// test eax, eax / je 0727203 / add esp, 4
	Patch( 0x7271CB, { 0x85, 0xC0, 0x74, 0x34, 0x83, 0xC4, 0x04 } );

	// Mirrors depth fix & bumped quality
	InjectHook(0x72701D, CreateMirrorBuffers);

	// Fixed MSAA options
	{
		using namespace MSAAFixes;

		Patch<BYTE>(0x57D126, 0xEB);
		Nop(0x57D0E8, 2);

		Patch<BYTE>(AddressByRegion_10<BYTE*>(0x7F6C9B), 0xEB);
		Patch<BYTE>(AddressByRegion_10<BYTE*>(0x7F60C6), 0xEB);
		Patch(AddressByRegion_10<BYTE*>(0x7F6683), { 0x90, 0xE9 });

		std::array<uintptr_t, 2> getMaxMultiSamplingLevels = { 0x57D136, 0x57D0EA };
		HookEach_GetMaxMultiSamplingLevels(getMaxMultiSamplingLevels, InterceptCall);

		std::array<uintptr_t, 4> setOrChangeMultiSamplingLevels = { 0x5744FD, 0x57D162, 0x57D2A6, 0x746350 };
		HookEach_SetOrChangeMultiSamplingLevels(setOrChangeMultiSamplingLevels, InterceptCall);

		Nop(0x57A0FC, 1);
		InjectHook(0x57A0FD, MSAAText, HookType::Call);
	}
	
	// Fixed car collisions - car you're hitting gets proper damage now
	InjectHook(0x5428EA, FixedCarDamage, HookType::Call);


	// Car explosion crash with multimonitor
	// Unitialized collision data breaking stencil shadows
	{
		using namespace UnitializedCollisionDataFix;

		VP::InterceptCall(ModCompat::Utils::GetFunctionAddrIfRerouted(0x40F870) + 0x63, orgMemMgrMalloc, CollisionData_MallocAndInit);

		std::array<uintptr_t, 2> newAndInit = {
			ModCompat::Utils::GetFunctionAddrIfRerouted(0x40F740) + 0xC,
			ModCompat::Utils::GetFunctionAddrIfRerouted(0x40F810) + 0xD,
		};
		HookEach_CollisionDataNew(newAndInit, InterceptCall);
	}


	// Crash when entering advanced display options on a dual monitor machine after:
	// - starting game on primary monitor in maximum resolution, exiting,
	// starting again in maximum resolution on secondary monitor.
	// Secondary monitor maximum resolution had to be greater than maximum resolution of primary monitor.
	// Not in 1.01
	ReadCall( 0x745B1E, orgGetNumVideoModes );
	InjectHook(0x745B1E, GetNumVideoModes_Store);
	InjectHook(0x745A81, GetNumVideoModes_Retrieve);


	// Fixed escalators crash
	ReadCall( 0x7185B5, orgEscalatorsUpdate );
	InjectHook(0x7185B5, UpdateEscalators);
	InjectHook(0x71791F, &CEscalator::SwitchOffNoRemove);


	// Don't allocate constant memory for stencil shadows every frame
	InjectHook(0x711DD5, StencilShadowAlloc, HookType::Call);
	Nop(0x711E0D, 3);
	Patch(0x711DDA, { 0xEB, 0x2C });
	Patch(0x711E5F, { 0x5F, 0x5D, 0xC3 });	// pop edi, pop ebp, ret


	// "Streaming memory bug" fix
	InjectHook(0x4C51A9, GTARtAnimInterpolatorSetCurrentAnim);

	
	// Fixed ammo for melee weapons in cheats
	Patch<BYTE>(0x43890B+1, 1); // knife
	Patch<BYTE>(0x4389F8+1, 1); // knife
	Patch<BYTE>(0x438B9F+1, 1); // chainsaw
	Patch<BYTE>(0x438C58+1, 1); // chainsaw
	Patch<BYTE>(0x4395C8+1, 1); // parachute

	Patch<BYTE>(0x439F1F, 0x53); // katana
	Patch<WORD>(0x439F20, 0x016A);


	// Fixed police scanner names
	char*			pScannerNames = *(char**)0x4E72D4;
	strcpy_s(pScannerNames + (8*113), 8, "WESTP");
	strcpy_s(pScannerNames + (8*134), 8, "????");


	// AI accuracy issue
	Nop(0x73B3AE, 1);
	InjectHook( 0x73B3AE + 1, WeaponRangeMult_VehicleCheck, HookType::Call );


	// New timers fix
	InjectHook( 0x561C32, asmTimers_ftol_PauseMode );
	InjectHook( 0x561902, asmTimers_ftol_NonClipped );
	InjectHook( 0x56191A, asmTimers_ftol );
	InjectHook( 0x46A036, asmTimers_SCMdelta );


	// Don't catch WM_SYSKEYDOWN and WM_SYSKEYUP (fixes Alt+F4)
	InjectHook( AddressByRegion_10<int>(0x748220), AddressByRegion_10<int>(0x748446), HookType::Jump );
	Patch<uint8_t>( AddressByRegion_10<int>(0x7481E3), 0x5C ); // esi -> ebx
	Patch<uint8_t>( AddressByRegion_10<int>(0x7481EA), 0x53 ); // esi -> ebx
	Patch<uint8_t>( AddressByRegion_10<int>(0x74820D), 0xFB ); // esi -> ebx
	Patch<int8_t>( AddressByRegion_10<int>(0x7481EF), 0x54-0x3C ); // use stack space for new lParam
	Patch<int8_t>( AddressByRegion_10<int>(0x748200), 0x4C-0x3C ); // use stack space for new lParam
	Patch<int8_t>( AddressByRegion_10<int>(0x748214), 0x4C-0x3C ); // use stack space for new lParam

	InjectHook( AddressByRegion_10<int>(0x74826A), AddressByRegion_10<int>(0x748446), HookType::Jump );
	Patch<uint8_t>( AddressByRegion_10<int>(0x74822D), 0x5C ); // esi -> ebx
	Patch<uint8_t>( AddressByRegion_10<int>(0x748234), 0x53 ); // esi -> ebx
	Patch<uint8_t>( AddressByRegion_10<int>(0x748257), 0xFB ); // esi -> ebx
	Patch<int8_t>( AddressByRegion_10<int>(0x748239), 0x54-0x3C ); // use stack space for new lParam
	Patch<int8_t>( AddressByRegion_10<int>(0x74824A), 0x4C-0x3C ); // use stack space for new lParam
	Patch<int8_t>( AddressByRegion_10<int>(0x74825E), 0x4C-0x3C ); // use stack space for new lParam


	// FuckCarCompletely not fixing panels
	Nop(0x6C268D, 3);


	// 014C cargen counter fix (by spaceeinstein)
	Patch<uint8_t>( 0x06F3E2C + 1, 0xBF ); // movzx ecx, ax -> movsx ecx, ax
	Patch<uint8_t>( 0x6F3E32, 0x74 ); // jge -> jz


	// Linear filtering on script sprites
	ReadCall( 0x58C092, orgDrawScriptSpritesAndRectangles );
	InjectHook( 0x58C092, DrawScriptSpritesAndRectangles );


	// Properly initialize all CVehicleModelInfo fields
	InterceptCall(0x4C7633, orgVehicleModelInfoInit, VehicleModelInfoInit);

	// Animated Phoenix hood scoop
	{
		auto* automobilePreRender = (*(decltype(CAutomobile::orgAutomobilePreRender<0>)**)(0x6B0AD2 + 2)) + 17;
		CAutomobile::orgAutomobilePreRender<0> = *automobilePreRender;
		Patch(automobilePreRender, &CAutomobile::PreRender_SilentPatch<0>);

		std::array<uintptr_t, 3> preRender = { 0x6C7E7A, 0x6CEAEC, 0x6CFADC };
		CAutomobile::HookEach_PreRender(preRender, InterceptCall);
	}

	// Extra animations for planes
	auto* planePreRender = (*(decltype(CPlane::orgPlanePreRender)**)(0x6C8E5A + 2)) + 17;
	CPlane::orgPlanePreRender = *planePreRender;
	Patch(planePreRender, &CPlane::PreRender_Stub);


	// Stop BF Injection/Bandito/Hotknife rotating engine components when engine is off
	Patch<const void*>(0x6AC2BE + 2, &CAutomobile::ms_engineCompSpeed);
	Patch<const void*>(0x6ACB91 + 2, &CAutomobile::ms_engineCompSpeed);


	// Make freeing temp objects more aggressive to fix vending crash
	InjectHook( 0x5A1840, CObject::TryToFreeUpTempObjects_SilentPatch, HookType::Jump );


	// Remove FILE_FLAG_NO_BUFFERING from CdStreams
	Patch<uint8_t>( 0x406BC6, 0xEB );


	// Proper metric-imperial conversion constants
	static const float METERS_TO_FEET = 3.280839895f;
	Patch<const void*>( 0x55942F + 2, &METERS_TO_FEET );
	Patch<const void*>( 0x55AA96 + 2, &METERS_TO_FEET );


	// Fixed impounding of random vehicles (because CVehicle::~CVehicle doesn't remove cars from apCarsToKeep)
	ReadCall( 0x6E2B6E, orgRecordVehicleDeleted );
	InjectHook( 0x6E2B6E, RecordVehicleDeleted_AndRemoveFromVehicleList );


	// Don't include an extra D3DLIGHT on vehicles since we fixed directional already
	// By aap
	Patch<float>(0x5D88D1 + 6, 0);
	Patch<float>(0x5D88DB + 6, 0);
	Patch<float>(0x5D88E5 + 6, 0);

	Patch<float>(0x5D88F9 + 6, 0);
	Patch<float>(0x5D8903 + 6, 0);
	Patch<float>(0x5D890D + 6, 0);


	// Fixed CAEAudioUtility timers - not typecasting to float so we're not losing precision after X days of PC uptime
	// Also fixed integer division by zero
	Patch( 0x5B9868 + 2, &pAudioUtilsFrequency );
	InjectHook( 0x5B9886, AudioUtilsGetStartTime );
	InjectHook( 0x4D9E80, AudioUtilsGetCurrentTimeInMs, HookType::Jump );


	// Car generators placed in interiors visible everywhere
	InjectHook( 0x6F3B30, &CEntity::SetPositionAndAreaCode );


	// Fixed bomb ownership/bombs saving for bikes
	{
		std::array<uintptr_t, 2> restoreCar = {
			ModCompat::Utils::GetFunctionAddrIfRerouted(0x448550) + 0x1A,
			ModCompat::Utils::GetFunctionAddrIfRerouted(0x4485C0) + 0x1B,
		};
		CStoredCar::HookEach_RestoreCar(restoreCar, VP::InterceptCall);
	}


	// unnamed CdStream semaphore
	Patch( 0x406945, { 0x6A, 0x00 } ); // push 0 \ nop
	Nop( 0x406945 + 2, 3 );


	// Correct streaming when using RC vehicles
	InjectHook( 0x55574B, FindPlayerEntityWithRC );
	InjectHook( 0x5557C3, FindPlayerVehicle_RCWrap );


	// TODO: Verify this fix, might be causing crashes atm and too risky to include
#if 0
	// Fixed CPlayerInfo assignment operator
	InjectHook( 0x45DEF0, &CPlayerInfo::operator=, HookType::Jump );
#endif


	// Fixed triangle above recruitable peds' heads
	Patch<uint8_t>( 0x60BC52 + 2, 8 ); // GANG2


	// Credits =)
	ReadCall( 0x5AF87A, Credits::PrintCreditText );
	ReadCall( 0x5AF8A4, Credits::PrintCreditText_Hooked );
	InjectHook( 0x5AF8A4, Credits::PrintSPCredits );


	// Fixed ammo from SCM
	{
		ReadCall( 0x47D335, CPed::orgGiveWeapon );
		InjectHook( 0x47D335, &CPed::GiveWeapon_SP );
	}

	// Fixed bicycle on fire - instead of CJ being set on fire, bicycle's driver is
	{
		using namespace BicycleFire;

		Patch( 0x53A984, { 0x90, 0x57 } ); // nop \ push edi
		Patch( 0x53A9A7, { 0x90, 0x57 } ); // nop \ push edi
		InjectHook( 0x53A984 + 2, GetVehicleDriver );
		InjectHook( 0x53A9A7 + 2, GetVehicleDriver );

		ReadCall( 0x53A990, CPlayerPed::orgDoStuffToGoOnFire );
		InjectHook( 0x53A990, DoStuffToGoOnFire_NullAndPlayerCheck );

		ReadCall( 0x53A9B7, CFireManager::orgStartFire );
		InjectHook( 0x53A9B7, &CFireManager::StartFire_NullEntityCheck );
	}

	
	// Decreased keyboard input latency
	{
		using namespace KeyboardInputFix;

		NewKeyState = *(void**)( 0x541E21 + 1 );
		OldKeyState = *(void**)( 0x541E26 + 1 );
		TempKeyState = *(void**)( 0x541E32 + 1 );
		objSize = *(uint32_t*)( 0x541E1C + 1 ) * 4;

		ReadCall( 0x541DEB, orgClearSimButtonPressCheckers );

		// Only hook if this call takes to somewhere in gta_sa.exe, else bail out since it's been tampered with
		if ( hInstance == ModCompat::Utils::GetModuleHandleFromAddress(orgClearSimButtonPressCheckers) )
		{
			InjectHook( 0x541DEB, ClearSimButtonPressCheckers );
			Nop( 0x541E2B, 2 );
			Nop( 0x541E3C, 2 );
		}
	}


	// Fixed handling.cfg name matching (names don't need unique prefixes anymore)
	{
		using namespace HandlingNameLoadFix;

		InjectHook( 0x6F4F58, strncpy_Fix );
		InjectHook( 0x6F4F64, strncmp_Fix );
	}


	// Firela animations
	{
		using namespace FirelaHook;
	
		UpdateMovingCollisionJmp = 0x6B200F;
		HydraulicControlJmpBack = 0x6B1FBF + 10;
		InjectHook( 0x6B1FBF, TestFirelaAndFlags, HookType::Jump );

		FollowCarCamNoMovement = 0x52551E;
		FollowCarCamJmpBack = 0x5254F6 + 6;
		InjectHook( 0x5254F6, CamControlFirela, HookType::Jump );
	}


	// Double artict3 trailer
	{
		auto* trailerTowBarPos = (*(decltype(CTrailer::orgGetTowBarPos)**)(0x6D03FD + 2)) + 60;
		CTrailer::orgGetTowBarPos = *trailerTowBarPos;
		Patch(trailerTowBarPos, &CTrailer::GetTowBarPos_Stub);
	}


	// DFT-30 wheel, Sweeper brushes and other typos in hierarchy
	{
		using namespace HierarchyTypoFix;

		InterceptCall(0x4C5311, orgStrcasecmp, strcasecmp);
	}


	// Tug tow bar (misc_b instead of misc_a
	Nop( 0x6AF2CC, 1 );
	InjectHook( 0x6AF2CC + 1, &CAutomobile::GetTowBarFrame, HookType::Call );


	// Play passenger's voice lines when killing peds with car, not only when hitting them damages player's vehicle
	InterceptCall(0x5F05CA, CEntity::orgGetColModel, &CVehicle::PlayPedHitSample_GetColModel);

	// Prevent samples from playing where they used to, so passengers don't comment on gently pushing peds
	InterceptCall(0x6A8298, CPed::orgSay, &CPed::Say_SampleBlackList<CONTEXT_GLOBAL_CAR_HIT_PED>);


	// Reset variables on New Game
	{
		using namespace VariableResets;

		std::array<uintptr_t, 2> reInitGameObjectVariables = { 0x53C6DB, 0x53C76D };
		HookEach_ReInitGameObjectVariables(reInitGameObjectVariables, InterceptCall);

		InterceptCall(0x5B89E4, orgLoadPickup, LoadPickup_SaveLine);
		InterceptCall(0x5B89EE, orgLoadCarGenerator, LoadCarGenerator_SaveLine);
		InterceptCall(0x5B89F9, orgLoadStuntJump, LoadStuntJump_SaveLine);

		// Variables to reset
		GameVariablesToReset.emplace_back( *(bool**)(0x63E8D8+1) ); // CPlayerPed::bHasDisplayedPlayerQuitEnterCarHelpText
		GameVariablesToReset.emplace_back( *(bool**)(0x44AC97+1) ); // CGarages::RespraysAreFree
		GameVariablesToReset.emplace_back( *(bool**)(0x44B49D+1) ); // CGarages::BombsAreFree

		GameVariablesToReset.emplace_back( *(int**)(0x42131F + 2) ); // CCarCtrl::LastTimeFireTruckCreated
		GameVariablesToReset.emplace_back( *(int**)(0x421319 + 2) ); // CCarCtrl::LastTimeAmbulanceCreated

		GameVariablesToReset.emplace_back( *(int**)(0x55C843 + 1) ); // CStats::m_CycleSkillCounter
		GameVariablesToReset.emplace_back( *(int**)(0x55CA39 + 1) ); // CStats::m_SwimUnderWaterCounter
		GameVariablesToReset.emplace_back( *(int**)(0x55CF3E + 2) ); // CStats::m_WeaponCounter
		GameVariablesToReset.emplace_back( *(int**)(0x55CF2A + 2) ); // CStats::m_LastWeaponTypeFired
		GameVariablesToReset.emplace_back( *(int**)(0x55CFC1 + 1) ); // CStats::m_DeathCounter
		GameVariablesToReset.emplace_back( *(int**)(0x55C5E5 + 1) ); // CStats::m_MaxHealthCounter
		GameVariablesToReset.emplace_back( *(int**)(0x55D043 + 1) ); // CStats::m_AddToHealthCounter

		// Non-zero inits still need to be done
		GameVariablesToReset.emplace_back( *(TimeNextMadDriverChaseCreated_t<float>**)(0x421369 + 2) ); // CCarCtrl::TimeNextMadDriverChaseCreated

		GameVariablesToReset.emplace_back( *(ResetToTrue_t**)(0x4758A4 + 2) ); // CGameLogic::bPenaltyForDeathApplies
		GameVariablesToReset.emplace_back( *(ResetToTrue_t**)(0x4758C4 + 1) ); // CGameLogic::bPenaltyForArrestApplies
	}

	// Don't clean the car BEFORE Pay 'n Spray doors close, as it gets cleaned later again anyway!
	Nop( 0x44ACDC, 6 );


	// Locale based metric/imperial system
	{
		using namespace Localization;

		InjectHook( 0x56D220, IsMetric_LocaleBased, HookType::Jump );
	}


	// Fix paintjobs vanishing after opening/closing garage without rendering the car first
	InjectHook( 0x6D0B70, &CVehicle::GetRemapIndex, HookType::Jump );


	// Re-introduce corona rotation on PC, like it is in III/VC/SA PS2
	{
		using namespace CoronaRotationFix;

		// Remove *= 20.0f from recipz to retrieve the original value for later
		Nop( 0x6FB277, 6 );

		ReadCall( 0x6FB2E6, orgRenderOneXLUSprite_Rotate_Aspect );
		InjectHook( 0x6FB2E6, RenderOneXLUSprite_Rotate_Aspect_SilentPatch );
	}


	// Fixed static shadows not rendering under fire and pickups
	{
		using namespace StaticShadowAlphaFix;

		ReadCall( 0x53E0C3, orgRenderStaticShadows );
		InjectHook( 0x53E0C3, RenderStaticShadows_StateFix );

		// Stored shadows conflict with SSE and are patched only when it's not installed
	}


	// Reset requested extras if created vehicle has no extras
	// Fixes eg. lightless taxis
	InjectHook( 0x4C97B1, CVehicleModelInfo::ResetCompsForNoExtras, HookType::Call );
	Nop( 0x4C97B1 + 5, 9 );


	// Allow extra6 to be picked with component rule 4 (any)
	Patch<uint32_t>( 0x4C8010 + 4, 6 );


	// Disallow moving cam up/down with mouse when looking back/left/right in vehicle
	{
		using namespace FollowCarMouseCamFix;

		orgUseMouse3rdPerson = *(bool**)(0x525615 + 1);
		Patch( 0x525615 + 1, &useMouseAndLooksForwards );

		ReadCall( 0x5245E4, orgGetPad );
		InjectHook( 0x5245E4, getPadAndSetFlag );
	}


	// Display stats in kg as floats (they pass a float and intend to display an integer)
	Patch<const char*>( 0x55A954 + 1, "%.0fkg" );
	Patch<const char*>( 0x5593E4 + 1, "%.0fkg" );


	// Add wind animations when driving a Quadbike
	// By Wesser
	InjectHook(0x5E69BC, &CVehicle::IsOpenTopCarOrQuadbike, HookType::Call);
	Nop(0x5E69BC + 5, 3);


	// Tie handlebar movement to the stering animations on Quadbike, fixes odd animation interpolations at low speeds
	// By Wesser
	Nop(0x6B7932, 1);
	InjectHook(0x6B7932+1, &QuadbikeHandlebarAnims::ProcessRiderAnims_FixInterp, HookType::Call);


	// Disable the radio station change anim on boats where CJ stands upright
	// By Wesser
	{
		using namespace UprightBoatRadioStationChange;

		InterceptCall(0x6DF4F4, orgAnimManagerBlendAnimation, AnimManagerBlendAnimation_SkipIfBoatDrive);
	}


	// Fix a memory leak when taking photos
	{
		using namespace CameraMemoryLeakFix;

		InjectHook(0x7453CE, psGrabScreen_UnlockAndReleaseSurface, HookType::Jump);
		InjectHook(0x7453D6, psGrabScreen_UnlockAndReleaseSurface, HookType::Jump);
	}


	// Fix crosshair issues when sniper rifle is quipped and a photo is taken by a gang member
	// By Wesser
	{
		using namespace CameraCrosshairFix;

		InterceptCall(0x58E842, orgGetWeaponInfo, GetWeaponInfo_OrCamera);
	}


	// Cancel the Drive By task of biker cops when losing the wanted level
	{
		using namespace BikerCopsDriveByFix;

		// ModCompat::Utils::GetFunctionAddrIfRerouted won't work here, as the decrypted function is still
		// slightly obfuscated compared to the compact EXE deobfuscation
		bool HoodlumPatched = false;
		if (*reinterpret_cast<const uint8_t*>(0x41BFA0) == 0xE9)
		{
			// Since this function differs between EU and US Hoodlum, exceptionally use patterns
			using namespace hook::txn;

			uintptr_t backToCruisingIfNoWantedLevel_Obfuscated;
			ReadCall(0x41BFA0, backToCruisingIfNoWantedLevel_Obfuscated);
			if (ModCompat::Utils::GetModuleHandleFromAddress(backToCruisingIfNoWantedLevel_Obfuscated) == hInstance) try
			{
				auto joinCarWithRoadSystem = make_range_pattern(backToCruisingIfNoWantedLevel_Obfuscated, backToCruisingIfNoWantedLevel_Obfuscated + 0x100,
					"56 E8 ? ? ? ? 8A 96 2D 04 00 00").get_first<void>(1);

				VP::InterceptCall(joinCarWithRoadSystem, orgJoinCarWithRoadSystem, JoinCarWithRoadSystem_AbortDriveByTask);
				HoodlumPatched = true;
			}
			TXN_CATCH();
		}
		if (!HoodlumPatched)
		{
			InterceptCall(0x41C00E, orgJoinCarWithRoadSystem, JoinCarWithRoadSystem_AbortDriveByTask);
		}
	}


	// Fix miscolored racing checkpoints if no other marker was drawn before them
	{
		using namespace RacingCheckpointsRender;

		InterceptCall(0x721520, orgRpClumpRender, RpClumpRender_SetLitFlag);
	}


	// Correct an improperly decrypted CPlayerPedData::operator= that broke gang recruiting after activating replays
	// Only broken in the HOODLUM EXE and the compact EXE that carried over the bug
	// By Wesser
	{
		using namespace PlayerPedDataAssignment;

		uintptr_t placeToPatch = ModCompat::Utils::GetFunctionAddrIfRerouted(0x45C4B0) + 0x5D;

		// If we're overwriting actual meaningful instructions and not NOPs, use a different wrapper
		if (MemEquals(placeToPatch, { 0x90, 0x90, 0x90, 0x90, 0x90 }))
		{
			InjectHook(placeToPatch, AssignmentOp_Hoodlum, HookType::Call);
		}
		else
		{
			InjectHook(placeToPatch, AssignmentOp_Compact, HookType::Call);
			Nop(placeToPatch + 5, 3);
		}
	}


	// Delay destroying of cigarettes/bottles held by NPCs so it does not potentially corrupt the moving list

	// CWorld::Process processes all entries in the moving list, calling ProcessControl on them.
	// CPlayerPed::ProcessControl handles the gang recruitment which in turn can result in homies dropping cigarettes or bottles.
	// When this happens, they are destroyed -immediately-. If those props are in the moving list right after the PlayerPed,
	// this corrupts a pre-cached node->next pointer and references an already freed entity.
	// To fix this, queue the entity for a delayed destruction instead of destroying immediately,
	// and let it destroy itself in CWorld::Process later.

	// or [esi+1Ch], 800h // bRemoveFromWorld
	// (The entity reference is already cleared for us, no need to do it)
	// jmp 5E03EC
	Patch(0x5E03D4, { 0x81, 0x4E, 0x1C, 0x00, 0x08, 0x00, 0x00, 0xEB, 0x0F });


	// Spawn lapdm1 (biker cop) correctly if the script requests one with PEDTYPE_COP
	// By Wesser
	{
		using namespace GetCorrectPedModel_Lapdm1;

		Patch(0x464FC8, &BikerCop_Retail);
	}


	// Only allow impounding cars and bikes (and their subclasses), as impounding helicopters, planes, boats makes no sense
	{
		using namespace RestrictImpoundVehicleTypes;
	
		std::array<uint32_t, 2> isThisVehicleInteresting = { 0x566794, 0x56A378 };
		HookEach_ShouldImpound(isThisVehicleInteresting, InterceptCall);
	}


	// Fix PlayerPed replay crashes
	// 1. Crash when starting a mocap cutscene after playing a replay wearing different clothes to the ones CJ has currently
	// 2. Crash when playing back a replay with a different motion group anim (fat/muscular/normal) than the current one
	{
		using namespace ReplayPlayerPedCrashFixes;

		InterceptCall(0x45F060, orgRestoreStuffFromMem, RestoreStuffFromMem_RebuildPlayer);

		bool HoodlumPatched = false;
		if (*reinterpret_cast<const uint8_t*>(0x45CEA0) == 0xE9)
		{
			// Since this function differs between EU and US Hoodlum, exceptionally use patterns
			using namespace hook::txn;

			uintptr_t DealWithNewPedPacket_Obfuscated;
			ReadCall(0x45CEA0, DealWithNewPedPacket_Obfuscated);
			if (ModCompat::Utils::GetModuleHandleFromAddress(DealWithNewPedPacket_Obfuscated) == hInstance) try
			{
				auto DealWithNewPedPacket = make_range_pattern(DealWithNewPedPacket_Obfuscated, DealWithNewPedPacket_Obfuscated + 0x200,
					"6A 01 56 E8 ? ? ? ? 83 C4 10").get_first<void>(3);

				VP::InterceptCall(DealWithNewPedPacket, orgRebuildPlayer, RebuildPlayer_LoadAllMotionGroupAnims);
				HoodlumPatched = true;
			}
			TXN_CATCH();
		}

		if (!HoodlumPatched)
		{
			InterceptCall(0x45CF87, orgRebuildPlayer, RebuildPlayer_LoadAllMotionGroupAnims);
		}
	}


	// Fix planes spawning in places where they crash easily
	{
		using namespace FindPlaneCreationCoorsFix;

		InterceptCall(0x6CD2B8, orgCheckCameraCollisionBuildings, CheckCameraCollisionBuildings_FixParams);
	}


	// Allow hovering on the Jetpack with Keyboard + Mouse controls
	// Does not modify any other controls, only hovering
	{
		using namespace JetpackKeyboardControlsHover;

		ProcessControlInput_DontHover = (void*)0x67ED33;
		ProcessControlInput_Hover = (void*)0x67EDAF;
	
		Nop(0x67ED2D, 1);
		InjectHook(0x67ED2D + 1, &ProcessControlInput_HoverWithKeyboard, HookType::Jump);
		ReadCall(0x67EDA6, orgGetLookBehindForCar);
	}


	// During riots, don't target the player group during missions
	// Fixes recruited homies panicking during Los Desperados and other riot-time missions
	{
		using namespace RiotDontTargetPlayerGroupDuringMissions;
	
		DontSkipTargetting = (void*)0x6CD54C;
		SkipTargetting = (void*)0x6CD7F4;
		InjectHook(0x6CD545, CheckIfInPlayerGroupAndOnAMission, HookType::Jump);
	}


	// Rescale light switching randomness in CVehicle::GetVehicleLightsStatus for PC the randomness range
	// The original randomness was 50000 out of 65535, which is impossible to hit with PC's 32767 range
	{
		static const float LightStatusRandomnessThreshold = 1.0f / 25000.0f;
		Patch<const void*>(0x6D5612 + 2, &LightStatusRandomnessThreshold);
	}


	// Fixed vehicles exploding twice if the driver leaves the car while it's exploding
	{
		using namespace RemoveDriverStatusFix;

		Nop(0x6D1955, 2);
		InjectHook(0x6D1955 + 2, RemoveDriver_SetStatus, HookType::Call);

		InterceptCall(0x64C8CE, orgPrepareVehicleForPedExit, PrepareVehicleForPedExit_WreckedCheck);

		// CVehicle::RemoveDriver already sets the status to STATUS_ABANDONED, these are redundant
		Nop(0x48628D, 3);
		Nop(0x647E21, 3);
	}


	// Fixed falling stars rendering black
	{
		using namespace ShootingStarsFix;

		InterceptCall(0x714610, orgRwIm3DTransform, RwIm3DTransform_UnsetTexture);
	}


	// Enable directional lights on flying car components
	{
		using namespace LitFlyingComponents;

		InterceptCall(0x6A8BBE, orgWorldAdd, WorldAdd_SetLightObjectFlag);
	}


	// Fix the logic behind exploding cars losing wheels
	// Right now, they lose one wheel at random according to the damage manager, but they always lose the front left wheel visually.
	// This change matches the visuals to the physics
	// Also make it possible for the rear right wheel to be randomly picked
	{
		std::array<uint32_t, 4> spawnFlyingComponent = { 0x6B38CA, 0x6B3CCB, 0x6C6EBE, 0x6CCEF9 };
		CAutomobile::HookEach_SpawnFlyingComponent(spawnFlyingComponent, InterceptCall);

		Nop(0x6B38E4, 5);
		Nop(0x6B3CF1, 5);
		Nop(0x6C6ED8, 5);
		Nop(0x6CCF17, 5);

		static const float fRandomness = -4.0f;
		Patch(0x6C25F5 + 2, &fRandomness);
	}


	// Make script randomness 16-bit, like on PS2
	{
		using namespace Rand16bit;

		std::array<uintptr_t, 2> rands = { 0x4674FE, 0x467533 };

		HookEach_Rand(rands, InterceptCall);
	}


	// Invert a CPed::IsAlive check in CTaskComplexEnterCar::CreateNextSubTask to avoid assigning
	// CTaskComplexLeaveCarAndDie to alive drivers 
	// Fixes a bug where stealing the car from the passenger side while holding throttle and/or brake would kill the driver,
	// or briefly resurrect them if they were already dead
	Patch<uint8_t>(0x63F576, 0x75);


	// Improved resolution selection dialog
	{
		using namespace NewResolutionSelectionDialog;

		ppRWD3D9 = *AddressByRegion_10<IDirect3D9***>(0x7F6312 + 1);
		FrontEndMenuManager = *(void**)(0x4054DB + 1);

		orgGetDocumentsPath = AddressByRegion_10<char*(*)()>(0x744FB0);

		Patch(AddressByRegion_10(0x746241 + 2), &pDialogBoxParamA_New);
		Patch(AddressByRegion_10(0x745DB3 + 2), &pSetFocus_NOP);

		InterceptCall(AddressByRegion_10(0x7461D8), orgRwEngineGetSubSystemInfo, RwEngineGetSubSystemInfo_GetFriendlyNames);
		InterceptCall(AddressByRegion_10(0x7461ED), orgRwEngineGetCurrentSubSystem, RwEngineGetCurrentSubSystem_FromSettings);
	}


	// Fix some big messages staying on screen longer at high resolutions due to a cut sliding text feature
	// Also since we're touching it, optionally allow to re-enable this feature.
	{
		using namespace SlidingTextsScalingFixes;

		// "Unscale" text sliding thresholds, so texts don't stay on screen longer at high resolutions
		Patch(0x58D2E9 + 1, &FIXED_RES_WIDTH_SCALE);

		// Replace dword ptr [esp+0Ch+X], eax \ dword ptr [esp+0Ch+X]
		// with a constant fld [620.0]
		static const float f620 = FIXED_RES_WIDTH_SCALE - 20.0f;
		Patch(0x58C90E, { 0x90, 0x90, 0xD9, 0x05 });
		Patch(0x58C90E + 4, &f620);
	}


	// Fix post effects not scaling correctly
	// Heat haze not rescaling after changing resolution
	// Water ripple effect having too high wave frequency at higher resolutions
	{
		using namespace PostEffectsScalingFixes;

		std::array<uintptr_t, 4> setCurrentVideoMode = { 0x574509, 0x57D096, 0x57D16E, 0x57D2B2 };

		if (*(uint8_t*)0x701450 == 0xA1)
		{
			pHeatHazeFXTypeLast = *(int32_t**)(0x701450 + 1);
		}
		else
		{
			// Someone re-routed CPostEffects::HeatHazeFXInit and we can't read the variable, fall back to the 1.0 US address
			pHeatHazeFXTypeLast = (int32_t*)0x8D50E4;
		}

		HookEach_SetCurrentVideoMode(setCurrentVideoMode, InterceptCall);
		InterceptCall(0x745C7D, orgSetupBackBufferVertex, SetupBackBufferVertex_Nop);

		InterceptCall(0x70529C, orgUnderWaterRipple, UnderWaterRipple_ScaleFrequency);
	}


	// Fix heat seeking and gamepad crosshairs not scaling to resolution
	{
		using namespace CrosshairScalingFixes;

		std::array<uintptr_t, 6> renderRotateAspect = {
			// Heat seeking missile crosshair
			0x742EAF, 0x742F45, 0x743073, 0x74311D,

			// Co-op in-car crosshair
			0x743A0A, 0x743BD4,
		};

		// Triangular gamepad crosshairs - their size needs to scale to screen *height*
		std::array<float**, 13> triangleSizes = {
			// Co-op offscreen crosshair
			(float**)(0x7436F1 + 2), (float**)(0x7436FF + 2), (float**)(0x74370D + 2), (float**)(0x74374B + 2),
			(float**)(0x743797 + 2), (float**)(0x7437D0 + 2), (float**)(0x7437FB + 2), (float**)(0x743819 + 2),
			(float**)(0x74386F + 2),

			// Regular crosshair
			(float**)(0x743212 + 2), (float**)(0x74321E + 2), (float**)(0x743259 + 2), (float**)(0x743266 + 2),
		};

		HookEach_RenderOneXLUSprite_Rotate_Aspect(renderRotateAspect, InterceptCall);

		InterceptCall(0x74318D, orgCalcScreenCoors, CalcScreenCoors_Recalculate<triangleSizes.size(), 0>);
		HookEach_GamepadCrosshair(triangleSizes, PatchFloat);
	}


	// Fix nitrous recharging faster when reversing the car
	// By Wesser
	{
		using namespace NitrousReverseRechargeFix;

		Nop(0x6A407B, 1);
		InjectHook(0x6A407B + 1, &NitrousControl_DontRechargeWhenReversing, HookType::Call);
	}


	// Fix Hydra's jet thrusters not displaying due to an uninitialized variable in RwMatrix
	// By B1ack_Wh1te
	{
		using namespace JetThrustersFix;

		std::array<uintptr_t, 4> matrixMult = { 0x6CA09F, 0x6CA122, 0x6CA1B2, 0x6CA242 };
		HookEach_MatrixMultiply(matrixMult, InterceptCall);
	}


#if FULL_PRECISION_D3D
	// Test - full precision D3D device
	Patch<uint8_t>( 0x7F672B+1, *(uint8_t*)(0x7F672B+1) | D3DCREATE_FPU_PRESERVE );
	Patch<uint8_t>( 0x7F6751+1, *(uint8_t*)(0x7F6751+1) | D3DCREATE_FPU_PRESERVE );
	Patch<uint8_t>( 0x7F6755+1, *(uint8_t*)(0x7F6755+1) | D3DCREATE_FPU_PRESERVE );
	Patch<uint8_t>( 0x7F6759+1, *(uint8_t*)(0x7F6759+1) | D3DCREATE_FPU_PRESERVE );
#endif
}

void Patch_SA_11()
{
	using namespace Memory;

	// IsAlreadyRunning needs to be read relatively late - the later, the better
	int			pIsAlreadyRunning = AddressByRegion_11<int>(0x749000);
	ReadCall( pIsAlreadyRunning, IsAlreadyRunning );
	InjectHook(pIsAlreadyRunning, InjectDelayedPatches_11);

	// (Hopefully) more precise frame limiter
	int			pAddress = AddressByRegion_11<int>(0x7496A0);
	ReadCall( pAddress, RsEventHandler );
	InjectHook(pAddress, NewFrameRender);
	InjectHook(AddressByRegion_11<int>(0x749624), GetTimeSinceLastFrame);

	// Set CAEDataStream to use a NEW structure
	CAEDataStream::SetStructType(true);

	// Heli rotors
	InjectHook(0x6CB390, &CPlane::Render_Stub, HookType::Jump);
	InjectHook(0x6C4C20, &CHeli::Render_Stub, HookType::Jump);

	// RefFix
	static const float						fRefZVal = 1.0f;
	static const float* const				pRefFal = &fRefZVal;

	Patch<const void*>(0x6FC1AA, &pRefFal);
	Patch<BYTE>(0x6FC1D0, 0);

	// Plane rotors
	InjectHook(0x4C7A01, PlaneAtomicRendererSetup, HookType::Jump);

	// DOUBLE_RWHEELS
	Patch<WORD>(0x4C9490, 0xE281);
	Patch<int>(0x4C9492, ~(rwMATRIXTYPEMASK|rwMATRIXINTERNALIDENTITY));

	// A fix for DOUBLE_RWHEELS trailers
	InjectHook(0x4C9423, TrailerDoubleRWheelsFix, HookType::Jump);
	InjectHook(0x4C94F4, TrailerDoubleRWheelsFix2, HookType::Jump);

	// No framedelay
	Patch<WORD>(0x53EDC3, 0x43EB);
	Patch<BYTE>(0x53EE3F, 0x10);
	Nop(0x53EE45, 1);

	// Disable re-initialization of DirectInput mouse device by the game
	Patch<BYTE>(0x57723C, 0xEB);
	Patch<BYTE>(0x57742A, 0xEB);
	Patch<BYTE>(0x5774FA, 0xEB);

	// Make sure DirectInput mouse device is set non-exclusive (may not be needed?)
	Patch<DWORD>(AddressByRegion_11<DWORD>(0x747270), 0x9090C030);

	// Hunter interior & static_rotor for helis
	InjectHook(0x4C7972, HunterTest, HookType::Jump);
	InjectHook(0x4C9818, CacheCRC32);

	// Lightbeam fix
	// Removed in Build 30 because the fix has been revisited
	/*
	Nop(0x6A36B5, 3);
	Patch<WORD>(0x6E1793, 0x0AEB);
	Patch<WORD>(0x6E17AC, 0x0BEB);
	Patch<WORD>(0x6E17C5, 0x0BEB);
	Patch<WORD>(0x6E17DF, 0x1AEB);

	Patch<WORD>(0x6E1C05, 0x09EB);
	Patch<WORD>(0x6E1C1D, 0x17EB);
	Patch<WORD>(0x6E1C4F, 0x0AEB);

	Patch<BYTE>(0x6E1810, 0x28);
	Patch<BYTE>(0x6E1C5D, 0x18);
	Patch<BYTE>(0x6E180B, 0xC8-0x7C);

	InjectHook(0x6A3717, ResetAlphaFuncRefAfterRender, HookType::Jump);
	*/

	// PS2 SUN!!!!!!!!!!!!!!!!!
	Nop(0x6FB9AC, 3);

	// Unlocked widescreen resolutions
	Patch<DWORD>(0x74619C, 0x9090127D);
	Nop(0x7461A0, 2);
	Nop(0x746222, 2);

	if ( *(BYTE*)0x746333 == 0xE9 )
	{
		// securom'd EXE
		// I better check if it's an address I want to patch, I don't want to break the game
		if ( *(DWORD*)0x14E7387 == 0x00E48C0F )
		{
			VP::Patch<DWORD>(0x14E7387, 0x90905D7D);
			VP::Nop(0x14E738B, 2);
		}
	}
	else
	{
		// Sadly, this func is different in 1.01 - so I don't know the original offset
	}

	// Heap corruption fix
	Patch<BYTE>(0x4A9D50, 0xC3);

	// User Tracks fix
	ReadCall( 0x4DA057, SetVolume );
	InjectHook(0x4DA057, UserTracksFix);
	InjectHook(0x4DA0A5, 0x4F3430);

	// FLAC support
	InjectHook(0x57C566, FLACInit);
	if ( *(BYTE*)0x4F3A50 == 0x6A )
	{
		InjectHook(0x4F3A50 + 0x14D, LoadFLAC_11, HookType::Jump);
		InjectHook(0x4F3A50 + 0x197, CAEWaveDecoderInit);

		Patch<WORD>(0x4F3A50 + 0x17A, 0x18EB);
		Patch<const void*>(0x4F3650 + 0x20, UserTrackExtensions);
		Patch<const void*>(0x4F3650 + 0x51, &UserTrackExtensions->Codec);
		Patch<const void*>(0x4F3A10 + 0x37, &UserTrackExtensions[1].Codec);
		Patch<BYTE>(0x4F3650 + 0x3D, sizeof(UserTrackExtensions));
	}
	else
	{
		// securom'd EXE
		InjectHook(0x5B6B7B, LoadFLAC_11, HookType::Jump);
		InjectHook(0x5B6BFB, CAEWaveDecoderInit, HookType::Jump);
		Patch<WORD>(0x5B6BCB, 0x26EB);

		if ( *(DWORD*)0x14E4954 == 0x05C70A75 )
			VP::Patch<const void*>(0x14E4958, &UserTrackExtensions[1].Codec);

		// Deobfuscating an opcode
		Patch<BYTE>(0x4EBD25, 0xBF);
		Patch<const void*>(0x4EBD26, UserTrackExtensions);
		Patch<const void*>(0x4EBDD4, &UserTrackExtensions->Codec);
		Patch<WORD>(0x4EBD2A, 0x72EB);
		Patch<BYTE>(0x4EBDC0, sizeof(UserTrackExtensions));
	}

	// Impound garages working correctly
	InjectHook(0x4251F9, 0x448A10);
	InjectHook(0x4253E9, 0x448A10);
	InjectHook(0x425491, 0x448A10);

	// Impounding after busted works
	Nop(0x443312, 5);

	// Mouse rotates an airbone car only with Steer with Mouse option enabled
	bool*	bEnableMouseSteering = *(bool**)0x6ADFCD; // CVehicle::m_bEnableMouseSteering
	Patch<bool*>(0x6B56E0, bEnableMouseSteering);
	Patch<bool*>(0x6CF047, bEnableMouseSteering);

	// Patched CAutomobile::Fix
	// misc_x parts don't get reset (Bandito fix), Towtruck's bouncing panel is not reset
	Patch<WORD>(0x6A3CE9, 0x5EEB);
	Patch<DWORD>(0x6A3D75, 0x5E5FCF8B);
	Patch<DWORD>(0x6A3D79, 0x448B5B5D);
	Patch<DWORD>(0x6A3D7D, 0x89644824);
	Patch<DWORD>(0x6A3D81, 5);
	Patch<DWORD>(0x6A3D85, 0x54C48300);
	InjectHook(0x6A3D89, &CAutomobile::Fix_SilentPatch, HookType::Jump);

	// Patched CPlane::Fix
	// Doors don't get reset (they can't get damaged anyway), bouncing panels DO reset
	// but not on Vortex
	Patch<BYTE>(0x6CB3F0, 0xEB);
	Patch<DWORD>(0x6CB425, 0x5E5FCF8B);
	InjectHook(0x6CB429, &CPlane::Fix_SilentPatch, HookType::Jump);

	// Weapon icon fix (crosshairs mess up rwRENDERSTATEZWRITEENABLE)
	// Only 1.0 and 1.01, Steam somehow fixed it (not the same way though)
	Nop(0x58E9E0, 3);
	Nop(0x58F287, 3);
	Nop(0x58F2B1, 3);

	// CGarages::RespraysAreFree resetting on new game
	Patch<WORD>(0x448C58, 0x8966);
	Patch<BYTE>(0x448C5A, 0x0D);
	Patch<bool*>(0x448C5B, *(bool**)0x44AD18);
	Patch<BYTE>(0x448C5F, 0xC3);

	// Bilinear filtering for license plates
	//Patch<BYTE>(0x6FD528, rwFILTERLINEAR);
	Patch<BYTE>(0x6FE777, rwFILTERLINEAR);

	// -//- Roadsign maganer
	//Patch<BYTE>(0x6FE147, rwFILTERLINEAR);

	// Bilinear filtering with mipmaps for weapon icons
	Patch<BYTE>(0x58DFAA, rwFILTERMIPLINEAR);

	// Illumination value from timecyc.dat properly using floats
	Patch<WORD>(0x5BC7A9, 0x14EB);

	// Illumination defaults to 1.0
	Patch<DWORD>(0x5BC2E4, 0xCC2484C7);
	Patch<DWORD>(0x5BC2E8, 0x00000000);
	Patch<DWORD>(0x5BC2EC, 0x903F8000);

	// All lights get casted at vehicles
	Patch<BYTE>(0x5DA297, 8);
	Patch<BYTE>(0x5DA2A0, 8);
	Patch<BYTE>(0x5DA73F, 8);

	// 6 extra directionals on Medium and higher
	// push eax
	// call GetMaxExtraDirectionals
	// add esp, 4
	// mov ebx, eax
	// nop
	Patch<uint8_t>( 0x7360B1, 0x50 );
	InjectHook( 0x7360B1 + 1, GetMaxExtraDirectionals, HookType::Call );
	Patch( 0x7360B1 + 6, { 0x83, 0xC4, 0x04, 0x8B, 0xD8 } );
	Nop( 0x7360B1 + 11, 3 );

	// Default resolution to native resolution
	const auto [width, height] = GetDesktopResolution();
	sprintf_s(aNoDesktopMode, "Cannot find %ux%ux32 video mode", width, height);

	if (width != 0 && height != 0)
	{
		Patch<DWORD>(0x746BE3, width);
		Patch<DWORD>(0x746BE8, height);
		Patch<const char*>(0x746C48, aNoDesktopMode);
	}

	// Corrected Map screen 1px issue
	Patch<float>(0x576357, -0.5f);
	Patch<float>(0x576317, -0.5f);
	Patch<float>(0x57631F, -0.5f);
	Patch<float>(0x5762CC, -0.5f);
	Patch<float>(0x57624A, -0.5f);
	Patch<float>(0x57627C, -0.5f);

	// Cars drive on water cheat
	Patch<DWORD>(&(*(DWORD**)0x438593)[34], 0xE5FC92C3);

	// No DirectPlay dependency
	Patch<BYTE>(AddressByRegion_11<DWORD>(0x747E1A), 0xB8);
	Patch<DWORD>(AddressByRegion_11<DWORD>(0x747E1B), 0x900);

	// SHGetFolderPath on User Files
	InjectHook(0x7457E0, GetMyDocumentsPathSA, HookType::Jump);

	// Fixed muzzleflash not showing from last bullet
	// nop \ test al, al \ jz
	Nop(0x61F4FC, 6);
	Patch(0x61F502, { 0x84, 0xC0, 0x74 });

	// Proper randomizations
	{
		using namespace ConsoleRandomness;

		InjectHook(0x44E8AE, rand31); // Missing ped paths
		InjectHook(0x44ED6E, rand31); // Missing ped paths
		InjectHook(0x6676C0, rand31); // Prostitutes
	}

	// Help boxes showing with big message
	// Game seems to assume they can show together
	Nop(0x58C25F, 6);

	// Fixed lens flare
	Patch<DWORD>(0x70FC8A, 0);
	Patch<BYTE>(0x6FBE51, 0xC3);
	Patch<BYTE>(0x6FBE30, 0x21);
	InjectHook(0x6FBE52, 0x70D750, HookType::Call);
	Patch<WORD>(0x6FBE57, 0xDCEB);

	Patch<WORD>(0x6FBCA6, 0xB990);
	Patch(0x6FBCA8, &FlushLensSwitchZ);
	Patch<WORD>(0x6FBCB0, 0xD1FF);
	Nop(0x6FBCB2, 1);

	Patch<WORD>(0x6FB758, 0xB990);
	Patch(0x6FB75A, &InitBufferSwitchZ);
	Patch<WORD>(0x6FB762, 0xD1FF);
	Nop(0x6FB764, 1);

	// Y axis sensitivity fix
	float* sens = *(float**)0x50F4DC;
	Patch<const void*>(0x50F4E6 + 0x2, sens);
	Patch<const void*>(0x50FFC6 + 0x2, sens);
	Patch<const void*>(0x5110C6 + 0x2, sens);
	Patch<const void*>(0x5122A8 + 0x2, sens);
	Patch<const void*>(0x52272C + 0x2, sens);

	// Don't lock mouse Y axis during fadeins
	Patch<WORD>(0x510054, 0x27EB);
	Patch<WORD>(0x5109B2, 0xE990);
	InjectHook(0x524511, 0x5245D9, HookType::Jump);

	// Fixed mirrors crash
	Patch( 0x7279FB, { 0x85, 0xC0, 0x74, 0x34, 0x83, 0xC4, 0x04 } );

	// Mirrors depth fix & bumped quality
	InjectHook(0x72784D, CreateMirrorBuffers);

	// Fixed MSAA options
	{
		using namespace MSAAFixes;

		Patch<BYTE>(0x57D906, 0xEB);
		Nop(0x57D8C8, 2);

		Patch<BYTE>(AddressByRegion_11<BYTE*>(0x7F759B), 0xEB);
		Patch<BYTE>(AddressByRegion_11<BYTE*>(0x7F69C6), 0xEB);
		Patch(AddressByRegion_11<BYTE*>(0x7F6F83), { 0x90, 0xE9 });

		std::array<uintptr_t, 2> getMaxMultiSamplingLevels = { 0x57D916, 0x57D8CA };
		HookEach_GetMaxMultiSamplingLevels(getMaxMultiSamplingLevels, InterceptCall);

		std::array<uintptr_t, 4> setOrChangeMultiSamplingLevels = { 0x574A6D, 0x57D942, 0x57DA86, 0x746BD0 };
		HookEach_SetOrChangeMultiSamplingLevels(setOrChangeMultiSamplingLevels, InterceptCall);

		Nop(0x57A66C, 1);
		InjectHook(0x57A66D, MSAAText, HookType::Call);
	}

	// Fixed car collisions - car you're hitting gets proper damage now
	InjectHook(0x542D8A, FixedCarDamage, HookType::Call);


	// Car explosion crash with multimonitor
	// Unitialized collision data breaking stencil shadows
	// FUCK THIS IN 1.01

	// Fixed escalators crash
	// FUCK THIS IN 1.01


	// Don't allocate constant memory for stencil shadows every frame
	// FUCK THIS IN 1.01

	// Fixed police scanner names
	char*			pScannerNames = *(char**)0x4E7714;
	strcpy_s(pScannerNames + (8*113), 8, "WESTP");
	strcpy_s(pScannerNames + (8*134), 8, "????");


	// 1.01 ONLY
	// I'm not sure what was this new audio code supposed to do, but it leaks memory
	// and due to this I have to make extra effort if I want FLAC to work on 1.01
	Patch<DWORD>(0x4E124C, 0x4DEBC78B);
}

void Patch_SA_Steam()
{
	using namespace Memory;

	// IsAlreadyRunning needs to be read relatively late - the later, the better
	ReadCall( 0x7826ED, IsAlreadyRunning );
	InjectHook(0x7826ED, InjectDelayedPatches_Steam);

	// (Hopefully) more precise frame limiter
	ReadCall( 0x782D25, RsEventHandler );
	InjectHook(0x782D25, NewFrameRender);
	InjectHook(0x782CA8, GetTimeSinceLastFrame);

	// Set CAEDataStream to use an old structure
	CAEDataStream::SetStructType(false);

	// Heli rotors
	InjectHook(0x700620, &CPlane::Render_Stub, HookType::Jump);
	InjectHook(0x6F9550, &CHeli::Render_Stub, HookType::Jump);

	// RefFix
	static const float						fRefZVal = 1.0f;
	static const float* const				pRefFal = &fRefZVal;

	Patch<const void*>(0x733FF0, &pRefFal);
	Patch<BYTE>(0x73401A, 0);

	// Plane rotors
	InjectHook(0x4D2270, PlaneAtomicRendererSetup, HookType::Jump);

	// DOUBLE_RWHEELS
	Patch<WORD>(0x4D3B9D, 0x6781);
	Patch<int>(0x4D3BA0, ~(rwMATRIXTYPEMASK|rwMATRIXINTERNALIDENTITY));

	// A fix for DOUBLE_RWHEELS trailers
	InjectHook(0x4D3B47, TrailerDoubleRWheelsFix_Steam, HookType::Jump);
	InjectHook(0x4D3C1A, TrailerDoubleRWheelsFix2_Steam, HookType::Jump);

	// No framedelay
	Patch<WORD>(0x551113, 0x46EB);
	Patch<BYTE>(0x551195, 0xC);
	Nop(0x551197, 1);

	// Disable re-initialization of DirectInput mouse device by the game
	Patch<BYTE>(0x58C0E5, 0xEB);
	Patch<BYTE>(0x58C2CF, 0xEB);
	Patch<BYTE>(0x58C3B3, 0xEB);

	// Make sure DirectInput mouse device is set non-exclusive (may not be needed?)
	Patch<DWORD>(0x7807D0, 0x9090C030);

	// Hunter interior & static_rotor for helis
	InjectHook(0x4D21E1, HunterTest, HookType::Jump);
	InjectHook(0x4D3F1D, CacheCRC32);

	// Bindable NUM5
	// Only 1.0 and Steam
	Nop(0x59363B, 2);

	// Lightbeam fix
	// Removed in Build 30 because the fix has been revisited
	/*
	Patch<WORD>(0x6CFEF9, 0x10EB);
	Nop(0x6CFF0F, 3);
	Patch<WORD>(0x71D1F5, 0x0DEB);
	Patch<WORD>(0x71D213, 0x0CEB);
	Patch<WORD>(0x71D230, 0x0DEB);
	Patch<WORD>(0x71D24D, 0x1FEB);

	Patch<WORD>(0x71D72F, 0x0BEB);
	Patch<WORD>(0x71D74B, 0x1BEB);
	Patch<WORD>(0x71D785, 0x0CEB);

	Patch<BYTE>(0x71D284, 0x28);
	Patch<BYTE>(0x71D795, 0x18);
	Patch<BYTE>(0x71D27F, 0xD0-0x9C);
	//InjectHook(0x6A2EDA, CullTest);

	InjectHook(0x6CFF69, ResetAlphaFuncRefAfterRender_Steam, HookType::Jump);
	*/

	// PS2 SUN!!!!!!!!!!!!!!!!!
	Nop(0x73362F, 2);

	// Unlocked widescreen resolutions
	//Patch<WORD>(0x77F9F0, 0x6E7D);
	Patch<WORD>(0x77F9FC, 0x627D);
	Patch<DWORD>(0x77F80B, 0x9090127D);
	Nop(0x77F80F, 2);
	Nop(0x77F880, 2);

	// Heap corruption fix
	Nop(0x5D88AE, 5);

	// User Tracks fix
	SetVolume = reinterpret_cast<decltype(SetVolume)>(0x4E2750);
	Patch<BYTE>(0x4E4A28, 0xBA);
	Patch<const void*>(0x4E4A29, UserTracksFix_Steam);
	InjectHook(0x4E4A8B, 0x4FF2B0);

	// FLAC support
	InjectHook(0x4FFC39, LoadFLAC_Steam, HookType::Jump);
	InjectHook(0x591814, FLACInit_Steam);
	InjectHook(0x4FFC83, CAEWaveDecoderInit);

	Patch<WORD>(0x4FFC66, 0x18EB);
	Patch<const void*>(0x4FF4F0, UserTrackExtensions);
	Patch<const void*>(0x4FF523, &UserTrackExtensions->Codec);
	Patch<const void*>(0x4FFAB6, &UserTrackExtensions[1].Codec);
	Patch<BYTE>(0x4FF50F, sizeof(UserTrackExtensions));

	// Impound garages working correctly
	InjectHook(0x426B48, 0x44C950);
	InjectHook(0x426D16, 0x44C950);
	InjectHook(0x426DC5, 0x44C950);

	// Impounding after busted works
	Nop(0x446F58, 5);

	// Mouse rotates an airbone car only with Steer with Mouse option enabled
	bool*	bEnableMouseSteering = *(bool**)0x6DB76D; // CVehicle::m_bEnableMouseSteering
	Patch<bool*>(0x6E3199, bEnableMouseSteering);
	Patch<bool*>(0x7046AB, bEnableMouseSteering);

	// Patched CAutomobile::Fix
	// misc_x parts don't get reset (Bandito fix), Towtruck's bouncing panel is not reset
	Patch<DWORD>(0x6D05B3, 0x6BEBED31);
	Patch<DWORD>(0x6D0649, 0x5E5FCF8B);
	Patch<DWORD>(0x6D064D, 0x448B5B5D);
	Patch<DWORD>(0x6D0651, 0x89644824);
	Patch<DWORD>(0x6D0655, 5);
	Patch<DWORD>(0x6D0659, 0x54C48300);
	InjectHook(0x6D065D, &CAutomobile::Fix_SilentPatch, HookType::Jump);

	// Patched CPlane::Fix
	// Doors don't get reset (they can't get damaged anyway), bouncing panels DO reset
	// but not on Vortex
	Patch<BYTE>(0x700681, 0xEB);
	Patch<DWORD>(0x7006B6, 0x5E5FCF8B);
	InjectHook(0x7006BA, &CPlane::Fix_SilentPatch, HookType::Jump);

	// Zones fix
	InjectHook(0x587080, GetCurrentZoneLockedOrUnlocked_Steam, HookType::Jump);

	// CGarages::RespraysAreFree resetting on new game
	Patch<WORD>(0x44CB55, 0xC766);
	Patch<BYTE>(0x44CB57, 0x05);
	Patch<bool*>(0x44CB58, *(bool**)0x44EEBA);
	Patch<WORD>(0x44CB5C, 0x0000);

	// Bilinear filtering for license plates
	//Patch<BYTE>(0x6FD528, rwFILTERLINEAR);
	Patch<BYTE>(0x736B30, rwFILTERLINEAR);

	// -//- Roadsign maganer
	//Patch<BYTE>(0x6FE147, rwFILTERLINEAR);

	// Bilinear filtering with mipmaps for weapon icons
	Patch<BYTE>(0x59BD9C, rwFILTERMIPLINEAR);

	// Illumination value from timecyc.dat properly using floats
	Patch<WORD>(0x5DAF6B, 0x2CEB);

	// Illumination defaults to 1.0
	Patch<DWORD>(0x5DA8D4, 0xD82484C7);
	Patch<DWORD>(0x5DA8D8, 0x00000000);
	Patch<DWORD>(0x5DA8DC, 0x903F8000);

	// All lights get casted at vehicles
	Patch<BYTE>(0x5F61C7, 8);
	Patch<BYTE>(0x5F61D0, 8);
	Patch<BYTE>(0x5F666D, 8);

	// 6 extra directionals on Medium and higher
	// push dword ptr [CGame::currArea]
	// call GetMaxExtraDirectionals
	// add esp, 4
	// mov ebx, eax
	// nop
	Patch( 0x768046, { 0xFF, 0x35 } );
	InjectHook( 0x768046 + 6, GetMaxExtraDirectionals, HookType::Call );
	Patch( 0x768046 + 11, { 0x83, 0xC4, 0x04, 0x8B, 0xD8 } );
	Nop( 0x768046 + 16, 1 );

	// Default resolution to native resolution
	const auto [width, height] = GetDesktopResolution();
	sprintf_s(aNoDesktopMode, "Cannot find %ux%ux32 video mode", width, height);

	if (width != 0 && height != 0)
	{
		Patch<DWORD>(0x780219, width);
		Patch<DWORD>(0x78021E, height);
		Patch<const char*>(0x78027E, aNoDesktopMode);
	}

	// Corrected Map screen 1px issue
	/*Patch<float>(0x575DE7, -5.0f);
	Patch<float>(0x575DA7, -5.0f);
	Patch<float>(0x575DAF, -5.0f);
	Patch<float>(0x575D5C, -5.0f);
	Patch<float>(0x575CDA, -5.0f);
	Patch<float>(0x575D0C, -5.0f);*/
	InjectHook(0x58B0F8, DrawRect_HalfPixel_Steam<true,false,false,true>);
	InjectHook(0x58B146, DrawRect_HalfPixel_Steam<true,false,false,false>);
	InjectHook(0x58B193, DrawRect_HalfPixel_Steam<true,false,false,true>);
	InjectHook(0x58B1E1, DrawRect_HalfPixel_Steam<false,false,false,true>);

	// Cars drive on water cheat
	Patch<DWORD>(&(*(DWORD**)0x43B793)[34], 0xE5FC92C3);

	// No DirectPlay dependency
	Patch<BYTE>(0x781456, 0xB8);
	Patch<DWORD>(0x781457, 0x900);

	// SHGetFolderPath on User Files
	InjectHook(0x77EDC0, GetMyDocumentsPathSA, HookType::Jump);

	// Fixed muzzleflash not showing from last bullet
	// REMOVED - the fix pointed at some unrelated instruction anyway? I think it never worked

	// Proper randomizations
	{
		using namespace ConsoleRandomness;

		InjectHook(0x452CCF, rand31); // Missing ped paths
		InjectHook(0x45322C, rand31); // Missing ped paths
		InjectHook(0x690263, rand31); // Prostitutes
	}

	// Help boxes showing with big message
	// Game seems to assume they can show together
	Nop(0x599CD3, 6);

	// Fixed lens flare
	Nop(0x733C65, 5);
	Patch<BYTE>(0x733C4E, 0x26);
	InjectHook(0x733C75, 0x7591E0, HookType::Call);
	Patch<WORD>(0x733C7A, 0xDBEB);

	Nop(0x733A5A, 4);
	Patch<BYTE>(0x733A5E, 0xB8);
	Patch(0x733A5F, &FlushLensSwitchZ);

	Patch<DWORD>(0x7333B0, 0xB9909090);
	Patch(0x7333B4, &InitBufferSwitchZ);

	// Y axis sensitivity fix
	float* sens = *(float**)0x51D4FA;
	Patch<const void*>(0x51D508 + 0x2, sens);
	Patch<const void*>(0x51E25A + 0x2, sens);
	Patch<const void*>(0x51F459 + 0x2, sens);
	Patch<const void*>(0x52086A + 0x2, sens);
	Patch<const void*>(0x532B9B + 0x2, sens);

	// Don't lock mouse Y axis during fadeins
	Patch<WORD>(0x51E192, 0x2BEB);
	Patch<WORD>(0x51ED38, 0xE990);
	InjectHook(0x534D3E, 0x534DF7, HookType::Jump);

	// Fixed mirrors crash
	Patch( 0x75903A, { 0x85, 0xC0, 0x74, 0x34, 0x83, 0xC4, 0x04 } );

	// Mirrors depth fix & bumped quality
	InjectHook(0x758E91, CreateMirrorBuffers);

	// Fixed MSAA options
	{
		using namespace MSAAFixes;

		Patch<BYTE>(0x592BBB, 0xEB);
		Nop(0x592B7F, 2);

		Patch<BYTE>(0x830C5B, 0xEB);
		Patch<BYTE>(0x830086, 0xEB);
		Patch(0x830643, { 0x90, 0xE9 });

		std::array<uintptr_t, 2> getMaxMultiSamplingLevels = { 0x592BCF, 0x592B81 };
		HookEach_GetMaxMultiSamplingLevels(getMaxMultiSamplingLevels, InterceptCall);

		std::array<uintptr_t, 4> setOrChangeMultiSamplingLevels = { 0x5897CD, 0x592BFB, 0x592D2E, 0x780206 };
		HookEach_SetOrChangeMultiSamplingLevels(setOrChangeMultiSamplingLevels, InterceptCall);

		Patch(0x58F88C, { 0x90, 0xBA });
		Patch(0x58F88E, MSAAText);
	}

	// Fixed car collisions - car you're hitting gets proper damage now
	Nop(0x555AB8, 2);
	InjectHook(0x555AC0, FixedCarDamage_Steam, HookType::Call);


	// Car explosion crash with multimonitor
	// Unitialized collision data breaking stencil shadows
	{
		using namespace UnitializedCollisionDataFix;

		InterceptCall(0x41A216, orgMemMgrMalloc, CollisionData_MallocAndInit);

		std::array<uintptr_t, 2> newAndInit = { 0x41A07C, 0x41A159 };
		HookEach_CollisionDataNew(newAndInit, InterceptCall);
	}


	// Crash when entering advanced display options on a dual monitor machine after:
	// - starting game on primary monitor in maximum resolution, exiting,
	// starting again in maximum resolution on secondary monitor.
	// Secondary monitor maximum resolution had to be greater than maximum resolution of primary monitor.
	// Not in 1.01
	ReadCall( 0x77F99E, orgGetNumVideoModes );
	InjectHook(0x77F99E, GetNumVideoModes_Store);
	InjectHook(0x77F901, GetNumVideoModes_Retrieve);


	// Fixed escalators crash
	ReadCall( 0x739975, orgEscalatorsUpdate );
	InjectHook(0x739975, UpdateEscalators);
	InjectHook(0x738BBD, &CEscalator::SwitchOffNoRemove);


	// Don't allocate constant memory for stencil shadows every frame
	InjectHook(0x760795, StencilShadowAlloc, HookType::Call);
	Nop(0x7607CD, 3);
	Patch(0x76079A, { 0xEB, 0x2C });
	Patch(0x76082C, { 0x5F, 0x5D, 0xC3 });	// pop edi, pop ebp, ret


	// "Streaming memory bug" fix
	InjectHook(0x4CF9E8, GTARtAnimInterpolatorSetCurrentAnim);


	// Fixed ammo for melee weapons in cheats
	Patch<BYTE>(0x43BB8B+1, 1); // knife
	Patch<BYTE>(0x43BC78+1, 1); // knife
	Patch<BYTE>(0x43BE1F+1, 1); // chainsaw
	Patch<BYTE>(0x43BED8+1, 1); // chainsaw
	Patch<BYTE>(0x43C868+1, 1); // parachute

	Patch<BYTE>(0x43D24C, 0x53); // katana
	Patch<WORD>(0x43D24D, 0x016A);


	// AI accuracy issue
	Nop(0x7738F5, 1);
	InjectHook( 0x7738F5+1, WeaponRangeMult_VehicleCheck, HookType::Call );


	// Don't catch WM_SYSKEYDOWN and WM_SYSKEYUP (fixes Alt+F4)
	InjectHook( 0x7821E5, 0x7823FE, HookType::Jump );
	Patch<uint8_t>( 0x7821A7 + 1, 0x5C ); // esi -> ebx
	Patch<uint8_t>( 0x7821AF, 0x53 ); // esi -> ebx
	Patch<uint8_t>( 0x7821D1 + 1, 0xFB ); // esi -> ebx
	Patch<int8_t>( 0x7821B1 + 3, 0x54-0x2C ); // use stack space for new lParam
	Patch<int8_t>( 0x7821C2 + 3, 0x4C-0x2C ); // use stack space for new lParam
	Patch<int8_t>( 0x7821D6 + 3, 0x4C-0x2C ); // use stack space for new lParam

	InjectHook( 0x78222F, 0x7823FE, HookType::Jump );
	Patch<uint8_t>( 0x7821F1 + 1, 0x5C ); // esi -> ebx
	Patch<uint8_t>( 0x7821F9, 0x53 ); // esi -> ebx
	Patch<uint8_t>( 0x78221B + 1, 0xFB ); // esi -> ebx
	Patch<int8_t>( 0x7821FB + 3, 0x54-0x2C ); // use stack space for new lParam
	Patch<int8_t>( 0x78220C + 3, 0x4C-0x2C ); // use stack space for new lParam
	Patch<int8_t>( 0x782220 + 3, 0x4C-0x2C ); // use stack space for new lParam


	// FuckCarCompletely not fixing panels
	Nop(0x6F5EC1, 3);


	// 014C cargen counter fix (by spaceeinstein)
	Patch<uint8_t>( 0x6F566D + 1, 0xBF ); // movzx eax, word ptr [ebp+1Ah] -> movsx eax, word ptr [ebp+1Ah]
	Patch<uint8_t>( 0x6F567E + 1, 0xBF ); // movzx ecx, ax -> movsx ecx, ax
	Patch<uint8_t>( 0x6F3E32, 0x74 ); // jge -> jz


	// Linear filtering on script sprites
	ReadCall( 0x59A3F2, orgDrawScriptSpritesAndRectangles );
	InjectHook( 0x59A3F2, DrawScriptSpritesAndRectangles );


	// Fixed police scanner names
	char*			pScannerNames = *(char**)0x4F2B83;
	strcpy_s(pScannerNames + (8*113), 8, "WESTP");
	strcpy_s(pScannerNames + (8*134), 8, "????");

	// STEAM ONLY
	// Proper aspect ratios - why Rockstar, why?
	// Steam aspect ratios were additionally divided by 1.1, producing a squashed image
	static const float f43 = 4.0f/3.0f, f54 = 5.0f/4.0f, f169 = 16.0f/9.0f;
	Patch<const void*>(0x73822B, &f169);
	Patch<const void*>(0x738247, &f54);
	Patch<const void*>(0x73825A, &f43);

	// No IMG size check
	Nop(0x406CD0, 7);
	Nop(0x406D00, 7);

	// Unlock 1.0/1.01 saves loading
	InjectHook(0x5EDFD9, 0x5EE0FA, HookType::Jump);
}

void Patch_SA_NewBinaries_Common(HINSTANCE hInstance)
{
	using namespace Memory;
	using namespace hook::txn;

	ScaleX = &ScalingInternals::ScaleX_Divisor;
	ScaleY = &ScalingInternals::ScaleY_Divisor;

	auto PatchFloat = [](float** address, const float*& org, float& replaced)
		{
			org = *address;
			Patch(address, &replaced);
		};
	auto PatchDouble = [](double** address, const double*& org, double& replaced)
		{
			org = *address;
			Patch(address, &replaced);
		};

	try
	{
		void* isAlreadyRunning = get_pattern( "85 C0 74 08 33 C0 8B E5 5D C2 10 00", -5 );
		ReadCall( isAlreadyRunning, IsAlreadyRunning );
		InjectHook(isAlreadyRunning, InjectDelayedPatches_NewBinaries);
	}
	TXN_CATCH();


	// (Hopefully) more precise frame limiter
	try
	{
		void* rsEventHandler = get_pattern( "83 C4 08 39 3D ? ? ? ? 75 23", -5 );
		void* getTimeSinceLastFrame = get_pattern( "EB 7F E8 ? ? ? ? 89 45 08", 2 );

		ReadCall( rsEventHandler, RsEventHandler );
		InjectHook( rsEventHandler, NewFrameRender );
		InjectHook( getTimeSinceLastFrame, GetTimeSinceLastFrame );
	}
	TXN_CATCH();


	// No framedelay
	try
	{
		auto framedelay_jmpSrc = get_pattern("83 EC 08 E8 ? ? ? ? E8", 3);
		auto framedelay_jmpDest = get_pattern("33 D2 8B C6 F7 F1 A3", 11);
		auto popEsi = pattern("83 C4 04 83 7D 08 00 5E").get_one();

		InjectHook( framedelay_jmpSrc, framedelay_jmpDest, HookType::Jump );

		Patch<BYTE>( popEsi.get<void>( 3 + 2 ), 0x4);
		Nop( popEsi.get<void>( 3 + 4 ), 1 );
	}
	TXN_CATCH();


	// Unlock 1.0/1.01 saves loading
	try
	{
		auto sizeCheck = get_pattern( "0F 84 ? ? ? ? 8D 45 FC" );
		Patch( sizeCheck, { 0x90, 0xE9 } ); // nop / jmp
	}
	TXN_CATCH();


	// Old .set files working again
	try
	{
		void* setFileSave = get_pattern( "C6 45 FD 5F", 0xE + 1 );
		auto setCheckVersion1 = get_pattern( "83 7D F8 07", 3);
		auto setCheckVersion2 = get_pattern( "83 C4 18 83 7D FC 07", 3 + 3);

		static const DWORD dwSetVersion = 6;
		Patch( setFileSave, &dwSetVersion );
		Patch<BYTE>( setCheckVersion1, dwSetVersion );
		Patch<BYTE>( setCheckVersion2, dwSetVersion );
	}
	TXN_CATCH();


	// Disable re-initialization of DirectInput mouse device by the game
	try
	{
		void* reinitMouse1 = get_pattern( "84 C0 ? 0F E8 ? ? ? ? 6A 01 E8", 2 );
		auto reinitMouse2 = pattern( "84 C0 ? 0E E8 ? ? ? ? 53 E8" ).count(2);
		void* diInitMouse = get_pattern( "6A 00 83 C1 1C", -3 );

		Patch<BYTE>( reinitMouse1, 0xEB );

		reinitMouse2.for_each_result( []( pattern_match match ) {
			Patch<BYTE>( match.get<void>( 2 ), 0xEB );
		});

		// Make sure DirectInput mouse device is set non-exclusive (may not be needed?)
		// nop / mov al, 1
		Patch( diInitMouse, { 0x90, 0xB0, 0x01 } );
	}
	TXN_CATCH();


	// Bindable NUM5
	try
	{
		auto keys_exception_list = get_pattern("3D 08 04 00 00 74", 5);
		Nop(keys_exception_list, 2);
	}
	TXN_CATCH();


	// Unlocked widescreen resolutions
	{
		// Assume anybody could have changed those, so bail out if ANYTHING goes wrong
		// However, all those patches are independent so try one by one
		try
		{
			auto wsRes_jmpSrc = pattern( "81 F9 E0 01 00 00 7C").get_one();
			auto wsRes1_jmpDest = pattern( "8B 45 EC 0F AF C2" ).get_one();

			const uintptr_t jumpSource = reinterpret_cast<uintptr_t>(wsRes_jmpSrc.get<void>( 6 + 2 ));
			const uintptr_t jumpDestination = reinterpret_cast<uintptr_t>(wsRes1_jmpDest.get<void>() );
			const ptrdiff_t dist = jumpDestination - jumpSource;
			// Can only do a short jump
			if ( INT8_MIN <= dist && dist <= INT8_MAX )
			{
				// jnl 00B19C33
				Patch( wsRes_jmpSrc.get<void>( 6 ), { 0x7D, static_cast<uint8_t>(dist) } );
			}
		}
		TXN_CATCH();

		try
		{
			auto wsRes2 = pattern( "0F 8C ? ? ? ? 81 7D ? ? ? ? ? 0F 8C" ).get_one();

			Nop( wsRes2.get<void>(), 4 );
			Patch( wsRes2.get<void>( 4 ), { 0x7D, 0xD } );
		}
		TXN_CATCH();

		try
		{
			auto wsRes3 = get_pattern( "7A 4D EB 02" );
			Nop( wsRes3, 2 );
		}
		TXN_CATCH();
	}


	// Default resolution to native resolution
	try
	{
		auto resolution = pattern( "BB 20 03 00 00" ).get_one();
		void* cannotFindResMessage = get_pattern( "6A 00 68 ? ? ? ? 68 ? ? ? ? 6A 00", 7 + 1 );

		RECT			desktop;
		GetWindowRect(GetDesktopWindow(), &desktop);
		sprintf_s(aNoDesktopMode, "Cannot find %dx%dx32 video mode", desktop.right, desktop.bottom);

		Patch<LONG>( resolution.get<void>( 1 ), desktop.right );
		Patch<LONG>( resolution.get<void>( 5 + 1 ), desktop.bottom );
		Patch<const char*>( cannotFindResMessage, aNoDesktopMode );
	}
	TXN_CATCH();


	// No DirectPlay dependency
	try
	{
		auto getDXversion = pattern( "50 68 ? ? ? ? A3" ).get_one();

		// mov eax, 0x900
		Patch<BYTE>( getDXversion.get<void>( -5 ), 0xB8 );
		Patch<DWORD>( getDXversion.get<void>( -5 + 1 ), 0x900 );
	}
	TXN_CATCH();


	// SHGetFolderPath on User Files
	try
	{
		void* getDocumentsPath = get_pattern( "8D 45 FC 50 68 19 00 02 00", -6 );
		InjectHook( getDocumentsPath, GetMyDocumentsPathSA, HookType::Jump );
	}
	TXN_CATCH();


	// Fixed muzzleflash not showing from last bullet
	try
	{
		auto weaponStateCheck = pattern("83 BC 8E A4 05 00 00 01").get_one();
		Nop(weaponStateCheck.get<void>(-16), 22);
		Patch(weaponStateCheck.get<void>(6), { 0x84, 0xC0, 0x74 });
	}
	TXN_CATCH();


	// Proper randomizations
	try
	{
		using namespace ConsoleRandomness;

		auto pedsRand = pattern( "C1 F8 06 99" ).count(2);
		void* prostitutesRand = get_pattern( "8B F8 32 C0", -5 );

		pedsRand.for_each_result( []( pattern_match match ) {
			InjectHook( match.get<void>( -5 ), rand31 ); // Missing ped paths
		});

		InjectHook( prostitutesRand, rand31 ); // Prostitutes
	}
	TXN_CATCH();


	// Help boxes showing with big message
	// Game seems to assume they can show together
	try
	{
		void* showingBigMessage = get_pattern( "38 1D ? ? ? ? 0F 85 ? ? ? ? 38 1D ? ? ? ? 0F 85 ? ? ? ? 38 1D", 6 );
		Nop( showingBigMessage, 6 );
	}
	TXN_CATCH();


	// Fixed lens flare
	try
	{
		auto coronasRenderEpilogue = pattern( "83 C7 3C FF 4D BC" ).get_one();
		auto flushLensSwitchZ = pattern( "6A 01 6A 06 FF D0 83 C4 08" ).get_one();
		auto initBufferSwitchZ = pattern( "6A 01 6A 06 FF D1 8B 75 A8" ).get_one();

		// TODO: This will break badly if applied multiple times, think of something!
		void* flushSpriteBuffer;
		ReadCall( coronasRenderEpilogue.get<void>( 0xC ), flushSpriteBuffer );
		Nop( coronasRenderEpilogue.get<void>( 0xC ), 5); // nop CSprite::FlushSpriteBuffer

		// Add CSprite::FlushSpriteBuffer, jmp loc_7300EC at the bottom of the function
		Patch<BYTE>( coronasRenderEpilogue.get<void>( -0xA + 1 ), 0x20 );
		InjectHook( coronasRenderEpilogue.get<void>( 0x18 ), flushSpriteBuffer, HookType::Call );
		// TODO: Short jumps
		Patch( coronasRenderEpilogue.get<void>( 0x18 + 5 ), { 0xEB, 0xE1 });

		// nop / mov eax, offset FlushLensSwitchZ
		Nop( flushLensSwitchZ.get<void>( -9 ), 4 );
		Patch<BYTE>( flushLensSwitchZ.get<void>( -9 + 4 ), 0xB8 );
		Patch( flushLensSwitchZ.get<void>( -9 + 5 ), &FlushLensSwitchZ );

		// nop / mov ecx, offset InitBufferSwitchZ
		Nop( initBufferSwitchZ.get<void>( -8 ), 3 );
		Patch<BYTE>( initBufferSwitchZ.get<void>( -8 + 3 ), 0xB9 );
		Patch( initBufferSwitchZ.get<void>( -8 + 4 ), &InitBufferSwitchZ );
	}
	TXN_CATCH();


	// Y axis sensitivity fix
	try
	{
		auto horizontalSens = pattern( "D9 05 ? ? ? ? D8 4D 0C D8 C9" ).get_one();
		float* sens = *horizontalSens.get<float*>( 2 );

		// Build a pattern for finding all instances of fld CCamera::m_fMouseAccelVertical
		const uint8_t mask[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
		uint8_t bytes[6] = { 0xD9, 0x05 };

		float* vertSens = *horizontalSens.get<float*>( 0xE + 2 );
		memcpy( bytes + 2, &vertSens, sizeof(vertSens) );

		auto mulVerticalSens1 = pattern( {bytes, _countof(bytes)}, {mask, _countof(mask)} ).count(4);
		void* mulVerticalSens2 = get_pattern( "D8 0D ? ? ? ? D8 C9 D9 5D F4", 2 );

		mulVerticalSens1.for_each_result( [sens]( pattern_match match ) {
			Patch( match.get<void>( 2 ), sens );
		} );

		Patch( mulVerticalSens2, sens );
	}
	TXN_CATCH();


	// Don't lock mouse Y axis during fadeins
	try
	{
		void* followPedWithMouse = get_pattern( "D9 5D 08 A0", -7 );
		void* followPedWithMouse2 = get_pattern( "66 83 3D ? ? ? ? ? 0F 85 ? ? ? ? 80 3D", -6 );

		void* followPedSA = get_pattern( "D9 5D F0 74 14", 3 );
		void* folowPedSA_dest = get_pattern( "D9 87 AC 00 00 00 D8 45 F0" );

		// TODO: Change when short jumps are supported
		Patch( followPedWithMouse, { 0xEB, 0x29 } );
		Patch( followPedWithMouse2, { 0x90, 0xE9 } );
		InjectHook( followPedSA, folowPedSA_dest, HookType::Jump );
	}
	TXN_CATCH();


	// Fixed mirrors crash
	try
	{
		// TODO: Change when short jumps are supported
		void* beforeMainRender = get_pattern( "8B 15 ? ? ? ? 83 C4 0C 52", 0xF );
		Patch( beforeMainRender, { 0x85, 0xC0, 0x74, 0x34, 0x83, 0xC4, 0x04 } );
	}
	TXN_CATCH();


	// Mirrors depth fix & bumped quality
	try
	{
		void* createBuffers = get_pattern( "7B 0A C7 05 ? ? ? ? 01 00 00 00", 0xC );
		InjectHook( createBuffers, CreateMirrorBuffers );
	}
	TXN_CATCH();


	// Fixed MSAA options
	try
	{
		using namespace MSAAFixes;

		auto func1 = pattern("83 BE C8 00 00 00 04 7F 11 E8").get_one();
		void* func2 = get_pattern("76 05 A3 ? ? ? ? 59");
		void* func3 = get_pattern("76 05 A3 ? ? ? ? 8B C7");
		void* func4 = get_pattern("0F 8C ? ? ? ? 8B 44 24 0C", 0x18);

		auto getMaxMultisamplingLevels = pattern( "5F 89 86 C8 00 00 00 8A 45 FF" ).get_one();
		void* changeMultiSamplingLevels = get_pattern( "8B 8E D0 00 00 00 51", -5 );
		void* changeMultiSamplingLevels2 = get_pattern( "8B 96 D0 00 00 00 52", -5 );
		void* setMultiSamplingLevels = get_pattern( "83 C4 04 8B C7 5F 5E 5B 8B E5 5D C3 BB", -5 );

		Patch<BYTE>( func1.get<void>( 0x4A ), 0xEB ); // jmp
		Nop( func1.get<void>( 7 ), 2 ); // nop a jmp

		Patch<BYTE>(func2, 0xEB); // jmp
		Patch<BYTE>(func3, 0xEB); // jmp
		Patch(func4, { 0x90, 0xE9 }); // jmp

		std::array<void*, 2> getMaxMultiSamplingLevels = {
			getMaxMultisamplingLevels.get<void>( -5 ),
			func1.get<void>( 7 + 2 ),
		};
		HookEach_GetMaxMultiSamplingLevels(getMaxMultiSamplingLevels, InterceptCall);

		std::array<void*, 4> setOrChangeMultiSamplingLevels = {
			changeMultiSamplingLevels,
			getMaxMultisamplingLevels.get<void>( -5 + 0x30 ),
			changeMultiSamplingLevels2,
			setMultiSamplingLevels
		};
		HookEach_SetOrChangeMultiSamplingLevels(setOrChangeMultiSamplingLevels, InterceptCall);

		// Only so newsteam r1 doesn't crash
		try
		{
			auto msaaText = pattern( "48 50 68 ? ? ? ? 53" ).get_one();

			// nop / mov edx, offset MSAAText
			Patch( msaaText.get<void>( -6 ), { 0x90, 0xBA } );
			Patch( msaaText.get<void>( -6 + 2 ), MSAAText );
		}
		TXN_CATCH();
	}
	TXN_CATCH();


	// Fixed car collisions - car you're hitting gets proper damage now
	try
	{
		auto fixedCarDamage = pattern( "8B 7D 10 0F B6 47 21" ).get_one();

		Nop( fixedCarDamage.get<void>(), 2 );
		InjectHook( fixedCarDamage.get<void>( 2 ), FixedCarDamage_Newsteam, HookType::Call );
	}
	TXN_CATCH();


	// Car explosion crash with multimonitor
	// Unitialized collision data breaking stencil shadows
	try
	{
		using namespace UnitializedCollisionDataFix;

		void* memMgrAlloc = get_pattern( "E8 ? ? ? ? 66 8B 55 08 8B 4D 10" );
		std::array<void*, 2> newAlloc = {
			get_pattern( "33 C9 83 C4 04 3B C1 74 36", -5 ),
			get_pattern( "33 C9 83 C4 04 3B C1 74 37", -5 ),
		};

		InterceptCall(memMgrAlloc, orgMemMgrMalloc, CollisionData_MallocAndInit);
		HookEach_CollisionDataNew(newAlloc, InterceptCall);
	}
	TXN_CATCH();


	// Crash when entering advanced display options on a dual monitor machine after:
	// - starting game on primary monitor in maximum resolution, exiting,
	// starting again in maximum resolution on secondary monitor.
	// Secondary monitor maximum resolution had to be greater than maximum resolution of primary monitor.
	// Not in 1.01
	try
	{
		void* storeVideoModes = get_pattern( "6A 00 8B F8 6A 04", -5 );
		void* retrieveVideoModes = get_pattern( "57 E8 ? ? ? ? 83 3D", 1 );

		ReadCall( storeVideoModes, orgGetNumVideoModes );
		InjectHook( storeVideoModes, GetNumVideoModes_Store );
		InjectHook( retrieveVideoModes, GetNumVideoModes_Retrieve );
	}
	TXN_CATCH();


	// Fixed escalators crash
	try
	{
		orgEscalatorsUpdate = reinterpret_cast<decltype(orgEscalatorsUpdate)>(get_pattern( "80 3D ? ? ? ? ? 74 23 56" ));

		auto updateEscalators = get_pattern("80 3D ? ? ? ? ? 74 22 56");
		auto removeEscalatorsForEntity = pattern( "80 7E F5 00 74 56" ).get_one();

		InjectHook( updateEscalators, UpdateEscalators, HookType::Jump );

		// lea ecx, [esi-84] / call CEscalator::SwitchOffNoRemove / jmp loc_734C0A
		// TODO: Change when short jmps are supported
		Patch( removeEscalatorsForEntity.get<void>(), { 0x8D, 0x8E } );
		Patch<int32_t>( removeEscalatorsForEntity.get<void>( 2 ), -0x84 );
		InjectHook( removeEscalatorsForEntity.get<void>( 6 ), &CEscalator::SwitchOffNoRemove, HookType::Call );
		Patch( removeEscalatorsForEntity.get<void>( 6 + 5 ), { 0xEB, 0x4F } );
	}
	TXN_CATCH();


	// Don't allocate constant memory for stencil shadows every frame
	try
	{
		auto shadowAlloc = pattern("83 C4 08 6A 00 68 00 60 00 00").get_one();
		auto shadowFree = get_pattern( "A2 ? ? ? ? A1 ? ? ? ? 50 E8", 5);

		InjectHook( shadowAlloc.get<void>( 3 ), StencilShadowAlloc, HookType::Call) ;
		Patch( shadowAlloc.get<void>( 3 + 5 ), { 0xEB, 0x2C } );
		Nop( shadowAlloc.get<void>( 0x3B ), 3 );
		Patch( shadowFree, { 0x5F, 0x5E, 0x5B, 0x5D, 0xC3 } ); // pop edi, pop esi, pop ebx, pop ebp, retn
	}
	TXN_CATCH();


	// "Streaming memory bug" fix
	try
	{
		void* animInterpolator = get_pattern( "83 C4 1C C7 03 00 30 00 00 5B", -5 );
		InjectHook(animInterpolator, GTARtAnimInterpolatorSetCurrentAnim);
	}
	TXN_CATCH();


	// Fixed ammo for melee weapons in cheats
	try
	{
		void* knifeAmmo1 = get_pattern( "6A 00 6A 04 6A FF", 1 );
		void* knifeAmmo2 = get_pattern( "6A 01 6A 00 6A 04 6A 01", 2 + 1 );
		void* chainsawAmmo1 = get_pattern( "6A 00 6A 09 6A FF", 1 );
		void* chainsawAmmo2 = get_pattern( "6A 00 6A 09 6A 01", 1 );
		void* parachuteAmmo = get_pattern( "6A 00 6A 2E 6A FF", 1 );
		void* katanaAmmo = get_pattern( "83 C4 0C 6A 01 53 6A 08 6A FF", 3 );

		Patch<BYTE>(knifeAmmo1, 1); // knife
		Patch<BYTE>(knifeAmmo2, 1); // knife
		Patch<BYTE>(chainsawAmmo1, 1); // chainsaw
		Patch<BYTE>(chainsawAmmo2, 1); // chainsaw
		Patch<BYTE>(parachuteAmmo, 1); // parachute

		// push ebx / push 1
		Patch( katanaAmmo, { 0x53, 0x6A, 0x01 } ); // katana
	}
	TXN_CATCH();


	// Proper aspect ratios
	try
	{
		auto calculateAr = pattern( "74 13 D9 05 ? ? ? ? D9 1D" ).get_one(); // 0x734247; has two matches but both from the same function

		static const float f43 = 4.0f/3.0f, f54 = 5.0f/4.0f, f169 = 16.0f/9.0f;
		Patch<const void*>(calculateAr.get<void>( 2 + 2 ), &f169);
		Patch<const void*>(calculateAr.get<void>( 0x1E + 2 ), &f54);
		Patch<const void*>(calculateAr.get<void>( 0x31 + 2 ), &f43);	
	}
	TXN_CATCH();


	// 6 extra directionals on Medium and higher
	// push dword ptr [CGame::currArea]
	// call GetMaxExtraDirectionals
	// add esp, 4
	// mov ebx, eax
	// nop
	try
	{
		auto maxdirs_addr = pattern( "83 3D ? ? ? ? 00 8D 5E 05 74 05 BB 06 00 00 00" ).get_one();

		Patch( maxdirs_addr.get<void>(), { 0xFF, 0x35 } );
		InjectHook( maxdirs_addr.get<void>(6), GetMaxExtraDirectionals, HookType::Call );
		Patch( maxdirs_addr.get<void>(11), { 0x83, 0xC4, 0x04, 0x8B, 0xD8 } );
		Nop( maxdirs_addr.get<void>(16), 1 );
	}
	TXN_CATCH();


	// AI accuracy issue
	try
	{
		auto match = pattern( "8B 82 8C 05 00 00 85 C0 74 09" ).get_one(); // 0x76DEA7 in newsteam r1
		Nop(match.get<int>(0), 1);
		InjectHook( match.get<int>(1), WeaponRangeMult_VehicleCheck, HookType::Call );
	}
	TXN_CATCH();


	// Don't catch WM_SYSKEYDOWN and WM_SYSKEYUP (fixes Alt+F4)
	try
	{
		auto patternie = pattern( "8B 75 10 8B ? 14 56" ).count(2); // 0x77C588 and 0x77C5CC in newsteam r2
		auto defproc = get_pattern( "8B ? 14 8B ? 10 8B ? 08 ? ? 56" );

		patternie.for_each_result( [&]( pattern_match match ) {
			InjectHook( match.get<int>(0x39), defproc, HookType::Jump );
			Patch<uint8_t>( match.get<int>(1), 0x5D ); // esi -> ebx
			Patch<uint8_t>( match.get<int>(6), 0x53 ); // esi -> ebx
			Patch<uint8_t>( match.get<int>(0x26 + 1), 0xFB ); // esi -> ebx
			Patch<int8_t>( match.get<int>(8 + 2), -8 ); // use stack space for new lParam
			Patch<int8_t>( match.get<int>(0x18 + 2), -8 ); // use stack space for new lParam
			Patch<int8_t>( match.get<int>(0x2B + 2), -8 ); // use stack space for new lParam
		} );
	}
	TXN_CATCH();


	// Reset variables on New Game
	try
	{
		using namespace VariableResets;

		// Variables to reset
		{
			auto timers_init = pattern( "89 45 FC DB 45 FC C6 05 ? ? ? ? 01" ).get_one();

			GameVariablesToReset.emplace_back( *timers_init.get<signed int*>(-17 + 2) );
			GameVariablesToReset.emplace_back( *timers_init.get<signed int*>(-11 + 2) );
			GameVariablesToReset.emplace_back( *timers_init.get<TimeNextMadDriverChaseCreated_t<float>*>(0x41 + 2) );
		}

		GameVariablesToReset.emplace_back( *get_pattern<ResetToTrue_t*>( "A2 ? ? ? ? E9 ? ? ? ? 6A 01 8B CE", 1 ) ); // CGameLogic::bPenaltyForDeathApplies
		GameVariablesToReset.emplace_back( *get_pattern<ResetToTrue_t*>( "88 0D ? ? ? ? E9 ? ? ? ? 6A 05", 2 ) ); // CGameLogic::bPenaltyForArrestApplies

		{
			auto loadPickup = get_pattern("E8 ? ? ? ? EB 1B 6A 00");
			auto loadCarGenerator = get_pattern("E8 ? ? ? ? 83 C4 08 EB 11");
			auto loadStuntJump = get_pattern("50 E8 ? ? ? ? EB 06", 1);

			std::array<void*, 2> reInitGameObjectVariables = {
				get_pattern( "E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 38 1D" ),
				get_pattern( "E8 ? ? ? ? 89 1D ? ? ? ? E8 ? ? ? ? 5E" )
			};
			HookEach_ReInitGameObjectVariables(reInitGameObjectVariables, InterceptCall);

			InterceptCall(loadPickup, orgLoadPickup, LoadPickup_SaveLine);
			InterceptCall(loadCarGenerator, orgLoadCarGenerator, LoadCarGenerator_SaveLine);
			InterceptCall(loadStuntJump, orgLoadStuntJump, LoadStuntJump_SaveLine);
		}
	}
	TXN_CATCH();


	// FuckCarCompletely not fixing panels
	try
	{
		void* panel_addr = get_pattern( "C6 46 04 FA 5E 5B", -3 );
		Nop(panel_addr, 3);
	}
	TXN_CATCH();


	// 014C cargen counter fix (by spaceeinstein)
	try
	{
		auto do_processing = pattern( "B8 C3 2E 57 06 F7 EE C1 FA 06" ).get_one();

		Patch<uint8_t>( do_processing.get<uint8_t*>(27 + 1), 0xBF ); // movzx eax, word ptr [edi+1Ah] -> movsx eax, word ptr [edi+1Ah]
		Patch<uint8_t>( do_processing.get<uint8_t*>(41), 0x74 ); // jge -> jz
	}
	TXN_CATCH();


	// Linear filtering on script sprites
	try
	{
		void* drawScriptSprites = get_pattern( "81 EC 94 01 00 00 53 56 57 50", 10 );
		ReadCall( drawScriptSprites, orgDrawScriptSpritesAndRectangles );
		InjectHook( drawScriptSprites, DrawScriptSpritesAndRectangles );
	}
	TXN_CATCH();

	// Animated Phoenix hood scoop
	// TODO

	// Extra animations for planes
	// TODO

	// Fixed animations for boats
	// TODO

	// Stop BF Injection/Bandito/Hotknife rotating engine components when engine is off
	// TODO


	// Make freeing temp objects more aggressive to fix vending crash
	try
	{
		auto match = get_pattern("57 8B 78 08 89 45 FC 85 FF 74 5B", -9);
		InjectHook( match, CObject::TryToFreeUpTempObjects_SilentPatch, HookType::Jump );
	}
	TXN_CATCH();


	// Remove FILE_FLAG_NO_BUFFERING from CdStreams
	try
	{
		auto match = get_pattern("81 F9 00 08 00 00 ? 05", 6);
		Patch<uint8_t>( match, 0xEB );
	}
	TXN_CATCH();


	// Proper metric-imperial conversion constants
	try
	{
		auto match1 = get_pattern( "83 EC 08 DC 35 ? ? ? ? DD 1C 24", 3 + 2 );
		auto match2 = get_pattern( "51 DC 35 ? ? ? ? DD 1C 24", 1 + 2 );

		static const double METERS_TO_FEET_DIV = 1.0 / 3.280839895;
		Patch<const void*>( match1, &METERS_TO_FEET_DIV );
		Patch<const void*>( match2, &METERS_TO_FEET_DIV );
	}
	TXN_CATCH();


	// Fixed impounding of random vehicles (because CVehicle::~CVehicle doesn't remove cars from apCarsToKeep)
	try
	{
		void* recordVehicleDeleted = get_pattern( "E8 ? ? ? ? 33 C0 66 89 86" );
		ReadCall( recordVehicleDeleted, orgRecordVehicleDeleted );
		InjectHook( recordVehicleDeleted, RecordVehicleDeleted_AndRemoveFromVehicleList );
	}
	TXN_CATCH();


	// Don't include an extra D3DLIGHT on vehicles since we fixed directional already
	// TODO when timecyc.dat illumination is fixed


	// Fixed CAEAudioUtility timers - not typecasting to float so we're not losing precision after X days of PC uptime
	// Also fixed integer division by zero
	try
	{
		auto staticInitialize = pattern( "FF 15 ? ? ? ? 5F 5E 85 C0" ).get_one();

		Patch( staticInitialize.get<void>( 2 ), &pAudioUtilsFrequency );
		InjectHook( staticInitialize.get<void>( 0x1E ), AudioUtilsGetStartTime );
		InjectHook( get_pattern( "50 FF 15 ? ? ? ? DF 6D F8", -9 ), AudioUtilsGetCurrentTimeInMs, HookType::Jump );
	}
	TXN_CATCH();


	// Car generators placed in interiors visible everywhere
	try
	{
		auto match = get_pattern("E8 ? ? ? ? 0F B6 57 0A");
		InjectHook( match, &CEntity::SetPositionAndAreaCode );
	}
	TXN_CATCH();


	// Fixed bomb ownership/bombs saving for bikes
	try
	{
		std::array<void*, 2> restoreCar = {
			get_pattern( "8D 4E EE E8", 3 ),
			get_pattern( "8D 4F EE E8", 3 )
		};
		CStoredCar::HookEach_RestoreCar(restoreCar, InterceptCall);
	}
	TXN_CATCH();


	// unnamed CdStream semaphore
	try
	{
		auto semaName = pattern( "52 6A 40 FF 15" ).get_one();

		Patch( semaName.get<void>( 9 ), { 0x6A, 0x00 } ); // push 0 \ nop
		Nop( semaName.get<void>( 9 + 2 ), 3 );
	}
	TXN_CATCH();


	// Correct streaming when using RC vehicles
	try
	{
		auto findPlayerEntity = get_pattern("88 1D ? ? ? ? E8 ? ? ? ? 8B F0 83 C4 04 3B F3", 6);
		auto findPlayerVehicle = get_pattern("E8 ? ? ? ? 83 C4 08 85 C0 74 07 C6 05");

		InjectHook( findPlayerEntity, FindPlayerEntityWithRC );
		InjectHook( findPlayerVehicle, FindPlayerVehicle_RCWrap );
	}
	TXN_CATCH();


	// Fixed triangle above recruitable peds' heads
	try
	{
		auto match = get_pattern( "83 BE 98 05 00 00 ? D9 45 DC", 6 );
		Patch<uint8_t>( match, 8 ); // GANG2
	}
	TXN_CATCH();


	// Credits =)
	try
	{
		auto renderCredits = pattern( "83 C4 18 E8 ? ? ? ? 80 3D" ).get_one();

		ReadCall( renderCredits.get<void>( -58 ), Credits::PrintCreditText );
		ReadCall( renderCredits.get<void>( -5 ), Credits::PrintCreditText_Hooked );
		InjectHook( renderCredits.get<void>( -5 ), Credits::PrintSPCredits );
	}
	TXN_CATCH();


	// Fixed ammo from SCM
	try
	{
		void* giveWeapon = get_pattern( "8B CE E8 ? ? ? ? 8B CE 8B D8", 2 );

		ReadCall( giveWeapon, CPed::orgGiveWeapon );
		InjectHook( giveWeapon, &CPed::GiveWeapon_SP );
	}
	TXN_CATCH();


	// Fixed bicycle on fire - instead of CJ being set on fire, bicycle's driver is
	try
	{
		using namespace BicycleFire;

		auto doStuffToGoOnFire = pattern( "83 BF 94 05 00 00 0A 75 6D 6A" ).get_one(); // 0x0054A6BE
		constexpr ptrdiff_t START_FIRE_OFFSET = 0x31;

		Patch( doStuffToGoOnFire.get<void>( 9 ), { 0x90, 0x57 } ); // nop \ push edi
		Patch( doStuffToGoOnFire.get<void>( START_FIRE_OFFSET ), { 0x90, 0x57 } ); // nop \ push edi
		InjectHook( doStuffToGoOnFire.get<void>( 9 + 2 ), GetVehicleDriver );
		InjectHook( doStuffToGoOnFire.get<void>( START_FIRE_OFFSET + 2 ), GetVehicleDriver );

		ReadCall( doStuffToGoOnFire.get<void>( 0x15 ), CPlayerPed::orgDoStuffToGoOnFire );
		InjectHook( doStuffToGoOnFire.get<void>( 0x15 ), DoStuffToGoOnFire_NullAndPlayerCheck );

		ReadCall( doStuffToGoOnFire.get<void>( START_FIRE_OFFSET + 0x10 ), CFireManager::orgStartFire );
		InjectHook( doStuffToGoOnFire.get<void>( START_FIRE_OFFSET + 0x10 ), &CFireManager::StartFire_NullEntityCheck );
	}
	TXN_CATCH();


	// Decreased keyboard input latency
	try
	{
		using namespace KeyboardInputFix;

		auto updatePads = pattern( "E8 ? ? ? ? B9 ? ? ? ? BE" ).get_one(); // 0x552DB7

		NewKeyState = *updatePads.get<void*>( 10 + 1 );
		OldKeyState = *updatePads.get<void*>( 15 + 1 );
		TempKeyState = *updatePads.get<void*>( 27 + 1 );
		objSize = *updatePads.get<uint32_t>( 5 + 1 ) * 4;

		ReadCall( updatePads.get<void>( -44 ), orgClearSimButtonPressCheckers );
		InjectHook( updatePads.get<void>( -44 ), ClearSimButtonPressCheckers );
		Nop( updatePads.get<void>( 20 ), 2 );
		Nop( updatePads.get<void>( 37 ), 2 );
	}
	TXN_CATCH();

	// Fixed handling.cfg name matching (names don't need unique prefixes anymore)
	try
	{
		using namespace HandlingNameLoadFix;

		auto findExactWord = pattern( "8B 55 08 56 8D 4D EC" ).get_one(); // 0x6F849B

		InjectHook( findExactWord.get<void>( -5 ), strncpy_Fix );
		InjectHook( findExactWord.get<void>( 9 ), strncmp_Fix );
	}
	TXN_CATCH();

	// No censorships (not set nor loaded from savegame)
	try
	{
		using namespace Localization;

		auto loadCensorshipValues = pattern( "0F B6 8E ED 00 00 00 88 0D" ).get_one();
		void* initialiseLanguage1 = get_pattern( "E8 ? ? ? ? 8B 35 ? ? ? ? FF D6" );
		auto initialiseLanguage2 = pattern( "E8 ? ? ? ? 83 FB 07" ).get_one();

		germanGame = *loadCensorshipValues.get<bool*>( 7 + 2 );
		frenchGame = *loadCensorshipValues.get<bool*>( 0x14 + 2 );
		nastyGame = *loadCensorshipValues.get<bool*>( 0x21 + 1 );

		// Don't load censorship values
		Nop( loadCensorshipValues.get<bool*>( 7 ), 6 );
		Nop( loadCensorshipValues.get<bool*>( 0x14 ), 6 );
		Nop( loadCensorshipValues.get<bool*>( 0x21 ), 5 );

		// Unified censorship levels for all regions
		InjectHook( initialiseLanguage1, EmptyStub );

		void* setNormalGame;
		void* setGermanGame;
		void* setFrenchGame;
		ReadCall( initialiseLanguage2.get<void>(), setNormalGame );
		ReadCall( initialiseLanguage2.get<void>( 0x15 ), setGermanGame );
		ReadCall( initialiseLanguage2.get<void>( 0x2A ), setFrenchGame );

		InjectHook( setNormalGame, SetUncensoredGame, HookType::Jump );
		InjectHook( setGermanGame, SetUncensoredGame, HookType::Jump );
		InjectHook( setFrenchGame, SetUncensoredGame, HookType::Jump );
	}
	TXN_CATCH();


	// Default Steer with Mouse to disabled, like in older executables not based on xbox
	try
	{
		// mov _ZN8CVehicle22m_bEnableMouseSteeringE, bl ->
		// mov _ZN8CVehicle22m_bEnableMouseSteeringE, al
		void* setDefaultPreferences = get_pattern( "89 86 AD 00 00 00 66 89 86 B1 00 00 00", -0xC );
		Patch( setDefaultPreferences, { 0x90, 0xA2 } );
	}
	TXN_CATCH();


	// Re-introduce corona rotation on PC, like it is in III/VC/SA PS2
	try
	{
		using namespace CoronaRotationFix;

		auto mulRecipz = get_pattern( "D9 5D FC D9 45 FC 89 45 FC D9 1C 24 53", -6 );
		auto renderOneXLUSprite = get_pattern( "E8 ? ? ? ? 83 C4 30 EB 02 DD D8 8A 47 FA" );

		// Remove *= 20.0f from recipz to retrieve the original value for later
		Nop( mulRecipz, 6 );

		ReadCall( renderOneXLUSprite, orgRenderOneXLUSprite_Rotate_Aspect );
		InjectHook( renderOneXLUSprite, RenderOneXLUSprite_Rotate_Aspect_SilentPatch );
	}
	TXN_CATCH();


	// Fixed static shadows not rendering under fire and pickups
	try
	{
		using namespace StaticShadowAlphaFix;

		auto renderStaticShadows = pattern( "52 E8 ? ? ? ? E8 ? ? ? ? E8" ).get_one();
		ReadCall( renderStaticShadows.get<void>( 1 + 5 + 5 ), orgRenderStaticShadows );
		InjectHook( renderStaticShadows.get<void>( 1 + 5 + 5 ), RenderStaticShadows_StateFix );

		ReadCall( renderStaticShadows.get<void>( 1 + 5 + 5 + 5 ), orgRenderStoredShadows );
		InjectHook( renderStaticShadows.get<void>( 1 + 5 + 5 + 5 ), RenderStoredShadows_StateFix );
	}
	TXN_CATCH();


	// Disable building pipeline for skinned objects (like parachute)
	try
	{
		using namespace SkinBuildingPipelineFix;

		auto setupAtomic = get_pattern("74 0D 57 E8 ? ? ? ? 83 C4 04 5F 5E 5D C3", 3);

		InterceptCall(setupAtomic, orgCustomBuildingDNPipeline_CustomPipeAtomicSetup, CustomBuildingDNPipeline_CustomPipeAtomicSetup_Skinned);
	}
	TXN_CATCH();


	// Reset requested extras if created vehicle has no extras
	// Fixes eg. lightless taxis
	try
	{
		auto resetComps = pattern( "6A 00 68 ? ? ? ? 57 E8 ? ? ? ? 83 C4 0C 8B C7" ).get_one();

		InjectHook( resetComps.get<void>( -9 ), CVehicleModelInfo::ResetCompsForNoExtras, HookType::Call );
		Nop( resetComps.get<void>( -9 + 5 ), 4 );
	}
	TXN_CATCH();


	// Allow extra6 to be picked with component rule 4 (any)
	try
	{
		void* extra6 = get_pattern( "6A 00 E8 ? ? ? ? 83 C4 08 5E", -2 + 1 );

		Patch<int8_t>( extra6, 6 );
	}
	TXN_CATCH();


	// Disallow moving cam up/down with mouse when looking back/left/right in vehicle
	try
	{
		using namespace FollowCarMouseCamFix;

		bool** useMouse3rdPerson = get_pattern<bool*>( "80 3D ? ? ? ? 00 C6 45 1B 00", 2 );
		auto getPad = get_pattern( "89 45 B8 E8 ? ? ? ? 89 45 FC", 3 );

		orgUseMouse3rdPerson = *useMouse3rdPerson;
		Patch( useMouse3rdPerson, &useMouseAndLooksForwards );

		ReadCall( getPad, orgGetPad );
		InjectHook( getPad, getPadAndSetFlag );
	}
	TXN_CATCH();


	// Add wind animations when driving a Quadbike
	// By Wesser
	try
	{
		auto isOpenTopCar = pattern("8B 11 8B 82 9C 00 00 00 FF D0").get_one();

		InjectHook(isOpenTopCar.get<void>(), &CVehicle::IsOpenTopCarOrQuadbike, HookType::Call);
		Nop(isOpenTopCar.get<void>(5), 5);
	}
	TXN_CATCH();


	// Tie handlebar movement to the stering animations on Quadbike, fixes odd animation interpolations at low speeds
	// By Wesser
	try
	{
		auto processRiderAnims = pattern("DD 05 ? ? ? ? D9 05 ? ? ? ? E8 ? ? ? ? D9 5D F0 80 7D 0B 00").get_one();
		// Compiler reordered variables compared to the older versions, so they need to be preserved
		auto saveDriveByAnim = pattern("D8 71 18 D9 5D EC").get_one();

		Nop(processRiderAnims.get<void>(), 1);
		InjectHook(processRiderAnims.get<void>(1), &QuadbikeHandlebarAnims::ProcessRiderAnims_FixInterp_Steam, HookType::Call);

		Nop(saveDriveByAnim.get<void>(), 1);
		InjectHook(saveDriveByAnim.get<void>(1), &QuadbikeHandlebarAnims::SaveDriveByAnim_Steam, HookType::Call);
	}
	TXN_CATCH();


	// Disable the radio station change anim on boats where CJ stands upright
	// By Wesser
	try
	{
		using namespace UprightBoatRadioStationChange;

		auto blendAnimation = get_pattern("E8 ? ? ? ? 83 C4 10 85 C0 0F 85 ? ? ? ? D9 47 48");
		InterceptCall(blendAnimation, orgAnimManagerBlendAnimation, AnimManagerBlendAnimation_SkipIfBoatDrive);
	}
	TXN_CATCH();


	// Fix a memory leak when taking photos
	try
	{
		using namespace CameraMemoryLeakFix;

		auto psGrabScreen = pattern("8B C6 5E 8B E5 5D C3 33 C0").get_one();

		InjectHook(psGrabScreen.get<void>(2), psGrabScreen_UnlockAndReleaseSurface_Steam, HookType::Jump);
		InjectHook(psGrabScreen.get<void>(7 + 2), psGrabScreen_UnlockAndReleaseSurface_Steam, HookType::Jump);
	}
	TXN_CATCH();


	// Fix crosshair issues when sniper rifle is quipped and a photo is taken by a gang member
	// By Wesser
	try
	{
		using namespace CameraCrosshairFix;

		auto getWeaponInfo = get_pattern("E8 ? ? ? ? 8B 40 0C 83 C4 08 85 C0");
		InterceptCall(getWeaponInfo, orgGetWeaponInfo, GetWeaponInfo_OrCamera);
	}
	TXN_CATCH();


	// Cancel the Drive By task of biker cops when losing the wanted level
	try
	{
		using namespace BikerCopsDriveByFix;

		auto backToCruisingIfNoWantedLevel = get_pattern("56 E8 ? ? ? ? 80 A6 ? ? ? ? ? 83 C4 04", 1);
		InterceptCall(backToCruisingIfNoWantedLevel, orgJoinCarWithRoadSystem, JoinCarWithRoadSystem_AbortDriveByTask);
	}
	TXN_CATCH();


	// Fix miscolored racing checkpoints if no other marker was drawn before them
	try
	{
		using namespace RacingCheckpointsRender;

		auto clumpRender = get_pattern("E8 ? ? ? ? DD 05 ? ? ? ? 83 C4 14");
		InterceptCall(clumpRender, orgRpClumpRender, RpClumpRender_SetLitFlag);
	}
	TXN_CATCH();


	// Delay destroying of cigarettes/bottles held by NPCs so it does not potentially corrupt the moving list
	try
	{
		// CWorld::Process processes all entries in the moving list, calling ProcessControl on them.
		// CPlayerPed::ProcessControl handles the gang recruitment which in turn can result in homies dropping cigarettes or bottles.
		// When this happens, they are destroyed -immediately-. If those props are in the moving list right after the PlayerPed,
		// this corrupts a pre-cached node->next pointer and references an already freed entity.
		// To fix this, queue the entity for a delayed destruction instead of destroying immediately,
		// and let it destroy itself in CWorld::Process later.

		// or [esi+1Ch], 800h // bRemoveFromWorld
		// (The entity reference is already cleared for us, no need to do it)
		// jmp 5E03EC
		auto dropEntity = get_pattern("74 1C 8B 16 8B 42 20", 2);

		Patch(dropEntity, { 0x81, 0x4E, 0x1C, 0x00, 0x08, 0x00, 0x00, 0xEB, 0x13 });
	}
	TXN_CATCH();


	// Spawn lapdm1 (biker cop) correctly if the script requests one with PEDTYPE_COP
	// By Wesser
	try
	{
		using namespace GetCorrectPedModel_Lapdm1;

		auto jumpTablePtr = *get_pattern<void**>("FF 24 8D ? ? ? ? 83 7D 08 06", 3);

		// Only patch if someone else hasn't relocated it
		if (ModCompat::Utils::GetModuleHandleFromAddress(jumpTablePtr) == hInstance)
		{
			Patch(jumpTablePtr+4, &BikerCop_Steam);
		}
	}
	TXN_CATCH();


	// Only allow impounding cars and bikes (and their subclasses), as impounding helicopters, planes, boats makes no sense
	try
	{
		using namespace RestrictImpoundVehicleTypes;

		auto isThisVehicleInteresting_pattern = pattern("56 E8 ? ? ? ? 83 C4 04 84 C0 74 09 56 E8 ? ? ? ? 83 C4 04 56").count(2);
		std::array<void*, 2> isThisVehicleInteresting = {
			isThisVehicleInteresting_pattern.get(0).get<void>(1),
			isThisVehicleInteresting_pattern.get(1).get<void>(1),
		};
		HookEach_ShouldImpound(isThisVehicleInteresting, InterceptCall);
	}
	TXN_CATCH();


	// Fix PlayerPed replay crashes
	// 1. Crash when starting a mocap cutscene after playing a replay wearing different clothes to the ones CJ has currently
	// 2. Crash when playing back a replay with a different motion group anim (fat/muscular/normal) than the current one
	try
	{
		using namespace ReplayPlayerPedCrashFixes;

		auto restoreStuffFromMem = get_pattern("E8 ? ? ? ? 80 3D ? ? ? ? ? C6 05 ? ? ? ? ? 74 3A D9 05");
		auto rebuildPlayer = get_pattern("E8 ? ? ? ? 6A 01 56 E8 ? ? ? ? 83 C4 10 EB 53", 8);

		InterceptCall(restoreStuffFromMem, orgRestoreStuffFromMem, RestoreStuffFromMem_RebuildPlayer);
		InterceptCall(rebuildPlayer, orgRebuildPlayer, RebuildPlayer_LoadAllMotionGroupAnims);
	}
	TXN_CATCH();


	// Fix planes spawning in places where they crash easily
	try
	{
		using namespace FindPlaneCreationCoorsFix;

		auto findPlaneCreationCoors = get_pattern("E8 ? ? ? ? 83 C4 18 84 C0 74 09");
		InterceptCall(findPlaneCreationCoors, orgCheckCameraCollisionBuildings, CheckCameraCollisionBuildings_FixParams_Steam);
	}
	TXN_CATCH();


	// Allow hovering on the Jetpack with Keyboard + Mouse controls
	// Does not modify any other controls, only hovering
	try
	{
		using namespace JetpackKeyboardControlsHover;

		auto processControl_CheckHover = pattern("0F 85 ? ? ? ? 8B CB C6 47 0D 00").get_one();
		auto processControl_DoHover = pattern("E8 ? ? ? ? 84 C0 74 10 D9 EE").get_one();

		ProcessControlInput_DontHover = processControl_CheckHover.get<void>(12);
		ProcessControlInput_Hover = processControl_DoHover.get<void>(9);

		Nop(processControl_CheckHover.get<void>(6), 1);
		InjectHook(processControl_CheckHover.get<void>(6 + 1), &ProcessControlInput_HoverWithKeyboard_Steam, HookType::Jump);
		ReadCall(processControl_DoHover.get<void>(), orgGetLookBehindForCar);
	}
	TXN_CATCH();


	// During riots, don't target the player group during missions
	// Fixes recruited homies panicking during Los Desperados and other riot-time missions
	try
	{
		using namespace RiotDontTargetPlayerGroupDuringMissions;

		auto targettingCheck = pattern("80 BB D0 02 00 00 01").get_one();
		auto skipTargetting = get_pattern("A1 ? ? ? ? A3 ? ? ? ? C7 05 ? ? ? ? ? ? ? ? 8B 4D F4");

		DontSkipTargetting = targettingCheck.get<void>(7);
		SkipTargetting = skipTargetting;
		InjectHook(targettingCheck.get<void>(), CheckIfInPlayerGroupAndOnAMission_Steam, HookType::Jump);
	}
	TXN_CATCH();


	// Rescale light switching randomness in CVehicle::GetVehicleLightsStatus for PC the randomness range
	// The original randomness was 50000 out of 65535, which is impossible to hit with PC's 32767 range
	try
	{
		auto getVehicleLightsStatus = get_pattern("DC 35 ? ? ? ? D9 05 ? ? ? ? D8 D9", 2);

		static const double LightStatusRandomnessThreshold = 25000.0;
		Patch<const void*>(getVehicleLightsStatus, &LightStatusRandomnessThreshold);
	}
	TXN_CATCH();


	// Fixed vehicles exploding twice if the driver leaves the car while it's exploding
	try
	{
		using namespace RemoveDriverStatusFix;

		auto removeDriver = pattern("8A 47 36 24 07 0C 20 80 7D 08 00").get_one();
		auto removeThisPed = get_pattern("80 C9 20 88 48 36 8B 96", 3);
		auto taskSimpleCarSetPedOut = get_pattern("80 C9 20 88 48 36 8B 86", 3);
		auto prepareVehicleForPedExit = get_pattern("57 E8 ? ? ? ? 57 8B CE E8 ? ? ? ? 57", 1);

		Nop(removeDriver.get<void>(), 2);
		InjectHook(removeDriver.get<void>(2), RemoveDriver_SetStatus, HookType::Call);

		InterceptCall(prepareVehicleForPedExit, orgPrepareVehicleForPedExit, PrepareVehicleForPedExit_WreckedCheck);

		// CVehicle::RemoveDriver already sets the status to STATUS_ABANDONED, these are redundant
		Nop(removeThisPed, 3);
		Nop(taskSimpleCarSetPedOut, 3);
	}
	TXN_CATCH();


	// Fixed falling stars rendering black
	try
	{
		using namespace ShootingStarsFix;

		auto rwIm3dTransform = get_pattern("E8 ? ? ? ? 83 C4 10 85 C0 74 16 6A 02 68 ? ? ? ? 6A 02");

		InterceptCall(rwIm3dTransform, orgRwIm3DTransform, RwIm3DTransform_UnsetTexture);
	}
	TXN_CATCH();


	// Enable directional lights on flying car components
	try
	{
		using namespace LitFlyingComponents;

		auto worldAdd = get_pattern("53 E8 ? ? ? ? 8B 4D F4 83 C4 04 5F 5E 8B C3", 1);

		InterceptCall(worldAdd, orgWorldAdd, WorldAdd_SetLightObjectFlag);
	}
	TXN_CATCH();


	// Fix the logic behind exploding cars losing wheels
	// Right now, they lose one wheel at random according to the damage manager, but they always lose the front left wheel visually.
	// This change matches the visuals to the physics
	// Also make it possible for the rear right wheel to be randomly picked
	try
	{
		auto automobileBlowUp = pattern("E8 ? ? ? ? 8B 8E ? ? ? ? 8D 45 08").get_one();
		auto automobileBlowUpCutscene = pattern("E8 ? ? ? ? 80 7D 10 00 C7 45").get_one();
		auto heliBlowUp = pattern("E8 ? ? ? ? 8B 86 ? ? ? ? 8D 55 08").get_one();
		auto planeBlowUp = pattern("E8 ? ? ? ? 8B 86 ? ? ? ? 85 C0 74 24").get_one();
		auto wheelDetachRandomness = get_pattern("DC 0D ? ? ? ? E8 ? ? ? ? 8B CE", 2);

		std::array<void*, 4> spawnFlyingComponent = {
			automobileBlowUp.get<void>(),
			automobileBlowUpCutscene.get<void>(),
			heliBlowUp.get<void>(),
			planeBlowUp.get<void>(),
		};
		CAutomobile::HookEach_SpawnFlyingComponent(spawnFlyingComponent, InterceptCall);

		Nop(automobileBlowUp.get<void>(0x1C), 5);
		Nop(automobileBlowUpCutscene.get<void>(0x22), 5);
		Nop(heliBlowUp.get<void>(0x1C), 5);
		Nop(planeBlowUp.get<void>(0x20), 5);

		static const double fRandomness = -4.0;
		Patch(wheelDetachRandomness, &fRandomness);
	}
	TXN_CATCH();


	// Make script randomness 16-bit, like on PS2
	try
	{
		using namespace Rand16bit;

		std::array<void*, 2> rands = {
			get_pattern("E8 ? ? ? ? 89 45 08 DB 45 08 32 C0"),
			get_pattern("E8 ? ? ? ? 89 06 32 C0"),
		};

		HookEach_Rand(rands, InterceptCall);
	}
	TXN_CATCH();


	// Invert a CPed::IsAlive check in CTaskComplexEnterCar::CreateNextSubTask to avoid assigning
	// CTaskComplexLeaveCarAndDie to alive drivers 
	// Fixes a bug where stealing the car from the passenger side while holding throttle and/or brake would kill the driver,
	// or briefly resurrect them if they were already dead
	try
	{
		auto isAlive = get_pattern("74 38 E8 ? ? ? ? 8B F8");

		Patch<uint8_t>(isAlive, 0x75);
	}
	TXN_CATCH();


	// Improved resolution selection dialog
	try
	{
		using namespace NewResolutionSelectionDialog;

		// RGL changed one of the parameters
		auto dialogBoxParam = [] {
			try {
				// Steam
				return get_pattern("51 FF 15 ? ? ? ? 85 C0 0F 84", 1 + 2);
			} catch (const hook::txn_exception&) {
				// RGL
				return get_pattern("53 FF 15 ? ? ? ? 85 C0", 1 + 2);
			}
		}();
		auto setFocus = get_pattern("53 FF 15 ? ? ? ? 5F", 1 + 2);
			
		auto rRwEngineGetSubSystemInfo = get_pattern("E8 ? ? ? ? 46 83 C4 08 83 C7 50");
		auto rwEngineGetCurrentSubSystem = get_pattern("7C EA E8 ? ? ? ? A3", 2);
		MenuManagerAdapterOffset = 0xD8;

		ppRWD3D9 = *get_pattern<IDirect3D9**>("33 ED A3 ? ? ? ? 3B C5", 2 + 1);
		FrontEndMenuManager = *get_pattern<void**>("50 50 68 ? ? ? ? B9 ? ? ? ? E8", 7 + 1); // This has 2 identical matches, we just need one

		orgGetDocumentsPath = reinterpret_cast<char*(*)()>(get_pattern( "8D 45 FC 50 68 19 00 02 00", -6 ));

		Patch(dialogBoxParam, &pDialogBoxParamA_New);
		Patch(setFocus, &pSetFocus_NOP);

		InterceptCall(rRwEngineGetSubSystemInfo, orgRwEngineGetSubSystemInfo, RwEngineGetSubSystemInfo_GetFriendlyNames);
		InterceptCall(rwEngineGetCurrentSubSystem, orgRwEngineGetCurrentSubSystem, RwEngineGetCurrentSubSystem_FromSettings);
	}
	TXN_CATCH();


	// Fix some big messages staying on screen longer at high resolutions due to a cut sliding text feature
	// Also since we're touching it, optionally allow to re-enable this feature.
	try
	{
		using namespace SlidingTextsScalingFixes;

		// "Unscale" text sliding thresholds, so texts don't stay on screen longer at high resolutions
		auto bigMessage0Threshold = pattern("83 C4 04 89 4D F4 DB 45 F4").get_one();
		auto bigMessage1Threshold = get_pattern("A1 ? ? ? ? D9 05 ? ? ? ? 83 C0 EC", 1);

		Patch(bigMessage1Threshold, &FIXED_RES_WIDTH_SCALE);

		// Replace dword ptr [ebp+X], eax \ dword ptr [ebp+X]
		// with a constant fld [620.0]
		static const float f620 = FIXED_RES_WIDTH_SCALE - 20.0f;
		Patch(bigMessage0Threshold.get<void>(3), { 0xD9, 0x05 });
		Patch(bigMessage0Threshold.get<void>(3 + 2), &f620);
	}
	TXN_CATCH();

	
	// Fix post effects not scaling correctly
	// Heat haze not rescaling after changing resolution
	// Water ripple effect having too high wave frequency at higher resolutions
	try
	{
		using namespace PostEffectsScalingFixes;

		std::array<void*, 4> setCurrentVideoMode = {
			get_pattern("E8 ? ? ? ? 83 C4 08 88 9E"),
			get_pattern("89 86 D0 00 00 00 E8 ? ? ? ? 83 C4 04 8B CE", 6),
			get_pattern("E8 ? ? ? ? 83 C4 08 8B CE E8 ? ? ? ? 8A 45 FF"),
			get_pattern("8B 96 D0 00 00 00 52 E8 ? ? ? ? 83 C4 08", 7),
		};

		auto setupBackBufferVertex = get_pattern("E8 ? ? ? ? A1 ? ? ? ? 8B 48 60");
		auto underWaterRipple = get_pattern("E8 ? ? ? ? 83 C4 18 80 3D ? ? ? ? ? 74 05");

		pHeatHazeFXTypeLast = *get_pattern<int32_t*>("89 1D ? ? ? ? A1 ? ? ? ? 8B 48 0C", 2);

		HookEach_SetCurrentVideoMode(setCurrentVideoMode, InterceptCall);
		InterceptCall(setupBackBufferVertex, orgSetupBackBufferVertex, SetupBackBufferVertex_Nop);

		InterceptCall(underWaterRipple, orgUnderWaterRipple, UnderWaterRipple_ScaleFrequency);
	}
	TXN_CATCH();


	// Fix heat seeking and gamepad crosshairs not scaling to resolution
	try
	{
		using namespace CrosshairScalingFixes;

		auto heatSeekingCrosshair1 = pattern("E8 ? ? ? ? 47 83 C4 30 83 FF 02").count(2);
		auto heatSeekingCrosshair2 = pattern("D9 1C 24 E8 ? ? ? ? 83 C4 30 A1").count(2);

		std::array<void*, 6> renderRotateAspect = {
			// Heat seeking missile crosshair
			heatSeekingCrosshair1.get(0).get<void>(),
			get_pattern("D9 1C 24 E8 ? ? ? ? 8B 15 ? ? ? ? 8B 02", 3),
			heatSeekingCrosshair1.get(1).get<void>(),
			heatSeekingCrosshair2.get(0).get<void>(3),

			// Co-op in-car crosshair
			get_pattern("D9 1C 24 E8 ? ? ? ? 8B 0D ? ? ? ? 8B 81", 3),
			heatSeekingCrosshair2.get(1).get<void>(3),
		};

		auto calcScreenCoors = get_pattern("E8 ? ? ? ? DB 05 ? ? ? ? 83 C4 38");

		// Triangular gamepad crosshairs - their size needs to scale to screen *height*
		auto regularCrosshair = pattern("D8 0D ? ? ? ? D9 5D F4 D9 46 08 DC 0D ? ? ? ? D8 45 F4").get_one();
		auto defaultCrosshairSize = pattern("DD 05 ? ? ? ? D8 C9 D9 5D F4 DC 0D ? ? ? ? D9 5D E8").get_one();
		std::array<float**, 3> triangleSizes = {
			// Co-op offscreen crosshair
			get_pattern<float*>("D9 5D CC D9 05 ? ? ? ? D9 C0 D9 45 FC", 3 + 2),
			get_pattern<float*>("D9 05 ? ? ? ? 83 C4 34 DD 05", 2),

			// Regular crosshair (float)
			regularCrosshair.get<float*>(2),
		};
		std::array<double**, 3> triangleSizesDouble = {
			regularCrosshair.get<double*>(0xC + 2),
			defaultCrosshairSize.get<double*>(2),
			defaultCrosshairSize.get<double*>(0xB + 2),
		};

		HookEach_RenderOneXLUSprite_Rotate_Aspect(renderRotateAspect, InterceptCall);

		InterceptCall(calcScreenCoors, orgCalcScreenCoors, CalcScreenCoors_Recalculate<triangleSizes.size(), triangleSizesDouble.size()>);
		HookEach_GamepadCrosshair(triangleSizes, PatchFloat);
		HookEach_GamepadCrosshair_Double(triangleSizesDouble, PatchDouble);
	}
	TXN_CATCH();


	// Fix nitrous recharging faster when reversing the car
	// By Wesser
	try
	{
		using namespace NitrousReverseRechargeFix;

		auto getGasPedal = pattern("D9 86 9C 04 00 00 D9 E8 D9 C0").get_one();
		Nop(getGasPedal.get<void>(), 1);
		InjectHook(getGasPedal.get<void>(1), &NitrousControl_DontRechargeWhenReversing_NewBinaries, HookType::Call);
	}
	TXN_CATCH();


	// Fix Hydra's jet thrusters not displaying due to an uninitialized variable in RwMatrix
	// By B1ack_Wh1te
	try
	{
		using namespace JetThrustersFix;

		auto thrust = pattern("D9 5D DC E8 ? ? ? ? 83 C4 0C").count(4);

		std::array<void*, 4> matrixMult = {
			thrust.get(0).get<void>(3),
			thrust.get(1).get<void>(3),
			thrust.get(2).get<void>(3),
			thrust.get(3).get<void>(3),
		};
		HookEach_MatrixMultiply(matrixMult, InterceptCall);
	}
	TXN_CATCH();
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	UNREFERENCED_PARAMETER(lpvReserved);

	if ( fdwReason == DLL_PROCESS_ATTACH )
	{
		const HINSTANCE hInstance = GetModuleHandle( nullptr );
		std::unique_ptr<ScopedUnprotect::Unprotect> Protect = ScopedUnprotect::UnprotectSectionOrFullModule( hInstance, ".text" );
		ScopedUnprotect::Section Protect2( hInstance, ".rdata" );

		const int8_t version = Memory::GetVersion().version;
		if ( version == 0 ) Patch_SA_10(hInstance);
		else if ( version == 1 ) Patch_SA_11(); // Not supported anymore
		else if ( version == 2 ) Patch_SA_Steam(); // Not supported anymore
		else
		{
			// TODO:
			// Add r1 low violence check to MemoryMgr.GTA via
			// if ( *(DWORD*)DynBaseAddress(0x49F810) == 0x64EC8B55 ) { normal } else { low violence } 
			Patch_SA_NewBinaries_Common(hInstance);
		}	
	}
	return TRUE;
}

extern "C" __declspec(dllexport)
uint32_t GetBuildNumber()
{
	return (SILENTPATCH_REVISION_ID << 8) | SILENTPATCH_BUILD_ID;
}
