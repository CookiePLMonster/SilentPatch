#include "StdAfxSA.h"

#include <algorithm>
#include "VehicleSA.h"
#include "TimerSA.h"
#include "PedSA.h"
#include "DelimStringReader.hpp"
#include "PlayerInfoSA.h"
#include "ParseUtils.hpp"
#include "SpeechContextsSA.h"
#include "Random.h"

#include "SVF.h"

static constexpr float PHOENIX_FLUTTER_PERIOD	= 70.0f;
static constexpr float PHOENIX_FLUTTER_AMP		= 0.13f;
static constexpr float SWEEPER_BRUSH_SPEED      = 0.3f;

static constexpr float PI = 3.14159265358979323846f;

float CAutomobile::ms_engineCompSpeed;

bool ReadDoubleRearWheels(const wchar_t* pPath)
{
	bool listedAny = false;

	constexpr size_t SCRATCH_PAD_SIZE = 32767;
	WideDelimStringReader reader( SCRATCH_PAD_SIZE );

	GetPrivateProfileSectionW( L"DoubleRearWheels", reader.PutBuffer(), reader.GetSize(), pPath );
	while ( const wchar_t* str = reader.GetString() )
	{
		wchar_t textLine[128];
		wcscpy_s(textLine, str);

		wchar_t* context = nullptr;	
		wchar_t* model = wcstok_s(textLine, L"=", &context);
		if (model == nullptr) continue;

		wchar_t* val = wcstok_s(nullptr, L"=", &context);	
		if (val == nullptr) continue;

		auto value = ParseUtils::TryParseInt(val);
		if (!value) continue;

		auto modelID = ParseUtils::TryParseInt(model);
		if (modelID)
		{
			SVF::RegisterFeature(*modelID, *value ? SVF::Feature::_INTERNAL_FORCE_DOUBLE_RWHEELS_ON : SVF::Feature::_INTERNAL_FORCE_DOUBLE_RWHEELS_OFF);	
		}
		else
		{
			SVF::RegisterFeature(ParseUtils::ParseString(model), *value ? SVF::Feature::_INTERNAL_FORCE_DOUBLE_RWHEELS_ON : SVF::Feature::_INTERNAL_FORCE_DOUBLE_RWHEELS_OFF);	
		}
		listedAny = true;
	}
	return listedAny;
}

// 1.0 ONLY!
bool __stdcall CheckDoubleRWheelsList( void* modelInfo, uint8_t* handlingData )
{
	static void* lastModelInfo = nullptr;
	static bool lastResult = false;

	if ( modelInfo == lastModelInfo ) return lastResult;
	lastModelInfo = modelInfo;

	const uint32_t numModelInfoPtrs = *(uint32_t*)0x4C5956+2;
	int32_t modelID = std::distance( ms_modelInfoPtrs, std::find( ms_modelInfoPtrs, ms_modelInfoPtrs+numModelInfoPtrs, modelInfo ) );

	bool foundFeature = false;
	bool featureStatus = false;
	SVF::ForAllModelFeatures( modelID, [&]( SVF::Feature f ) {
		if ( f == SVF::Feature::_INTERNAL_FORCE_DOUBLE_RWHEELS_OFF )
		{
			foundFeature = true;
			featureStatus = false;
			return false;
		}
		if ( f == SVF::Feature::_INTERNAL_FORCE_DOUBLE_RWHEELS_ON )
		{
			foundFeature = true;
			featureStatus = true;	
			return false;
		}
		return true;
	} );
	if ( !foundFeature )
	{
		uint32_t flags = *(uint32_t*)(handlingData+0xCC);
		lastResult = (flags & 0x20000000) != 0;
		return lastResult;
	}

	lastResult = featureStatus;
	return lastResult;
}


// Now left only for "backwards compatibility"
bool CVehicle::IgnoresRotorFix() const
{
	if ( ms_rotorFixOverride != 0 )
	{
		return ms_rotorFixOverride < 0;
	}
	return SVF::ModelHasFeature( m_nModelIndex.Get(), SVF::Feature::_INTERNAL_NO_ROTOR_FADE );
}

bool CVehicle::IsOpenTopCarOrQuadbike() const
{
	return IsOpenTopCar() || m_dwVehicleSubClass == VEHICLE_QUAD;
}

static void*	varVehicleRender = AddressByVersion<void*>(0x6D0E60, 0x6D1680, 0x70C0B0);
WRAPPER void CVehicle::Render() { VARJMP(varVehicleRender); }
static void*	varIsLawEnforcementVehicle = AddressByVersion<void*>(0x6D2370, 0x6D2BA0, 0x70D8C0);
WRAPPER bool CVehicle::IsLawEnforcementVehicle() { VARJMP(varIsLawEnforcementVehicle); }

auto GetFrameHierarchyId = AddressByVersion<int32_t(*)(RwFrame*)>(0x732A20, 0x733250, 0x76CC30);

void (CPlane::*CPlane::orgPlanePreRender)();

static int32_t random(int32_t from, int32_t to)
{
	return from + ( ConsoleRandomness::rand31() % (to-from) );
}

static RwObject* GetCurrentAtomicObject( RwFrame* frame )
{
	RwObject* obj = nullptr;
	RwFrameForAllObjects( frame, [&obj]( RwObject* object ) -> RwObject* {
		if ( RpAtomicGetFlags(object) & rpATOMICRENDER )
		{
			obj = object;
			return nullptr;
		}
		return object;
	} );
	return obj;
}

RwFrame* GetFrameFromName( RwFrame* topFrame, const char* name )
{
	class GetFramePredicate
	{
	public:
		RwFrame* foundFrame = nullptr;

		GetFramePredicate( const char* name )
			: m_name( name )
		{
		}

		RwFrame* operator() ( RwFrame* frame )
		{
			if ( _stricmp( m_name, GetFrameNodeName(frame) ) == 0 )
			{
				foundFrame = frame;
				return nullptr;
			}
			RwFrameForAllChildren(frame, std::ref(*this));
			return foundFrame != nullptr ? nullptr : frame;
		}
	
	private:
		const char* const m_name;
	};
;
	return RwFrameForAllChildren( topFrame, GetFramePredicate(name) ).foundFrame;
}

RwFrame* GetFrameFromID( RwFrame* topFrame, int32_t ID )
{
	class GetFramePredicate
	{
	public:
		RwFrame* foundFrame = nullptr;

		GetFramePredicate( int32_t ID )
			: ID( ID )
		{
		}

		RwFrame* operator() ( RwFrame* frame )
		{
			if ( ID == GetFrameHierarchyId(frame) )
			{
				foundFrame = frame;
				return nullptr;
			}
			RwFrameForAllChildren(frame, std::ref(*this));
			return foundFrame != nullptr ? nullptr : frame;
		}

	private:
		const int32_t ID;
	};
	return RwFrameForAllChildren( topFrame, GetFramePredicate(ID) ).foundFrame;
}

void ReadRotorFixExceptions(const wchar_t* pPath)
{
	constexpr size_t SCRATCH_PAD_SIZE = 32767;
	WideDelimStringReader reader( SCRATCH_PAD_SIZE );

	GetPrivateProfileSectionW( L"RotorFixExceptions", reader.PutBuffer(), reader.GetSize(), pPath );
	while ( const wchar_t* str = reader.GetString() )
	{
		auto ID = ParseUtils::TryParseInt(str);
		if (ID)
			SVF::RegisterFeature(*ID, SVF::Feature::_INTERNAL_NO_ROTOR_FADE);
		else
			SVF::RegisterFeature(ParseUtils::ParseString(str), SVF::Feature::_INTERNAL_NO_ROTOR_FADE);
	}
}

void ReadLightbeamFixExceptions(const wchar_t* pPath)
{
	constexpr size_t SCRATCH_PAD_SIZE = 32767;
	WideDelimStringReader reader( SCRATCH_PAD_SIZE );

	GetPrivateProfileSectionW( L"LightbeamFixExceptions", reader.PutBuffer(), reader.GetSize(), pPath );
	while ( const wchar_t* str = reader.GetString() )
	{
		auto ID = ParseUtils::TryParseInt(str);
		if (ID)
			SVF::RegisterFeature(*ID, SVF::Feature::_INTERNAL_NO_LIGHTBEAM_BFC_FIX);
		else
			SVF::RegisterFeature(ParseUtils::ParseString(str), SVF::Feature::_INTERNAL_NO_LIGHTBEAM_BFC_FIX);
	}
}

bool CVehicle::HasFirelaLadder() const
{
	return SVF::ModelHasFeature( m_nModelIndex.Get(), SVF::Feature::FIRELA_LADDER );
}

void* CVehicle::PlayPedHitSample_GetColModel()
{
	if ( this == FindPlayerVehicle() )
	{
		CPed *pPassenger = PickRandomPassenger();
		if ( pPassenger != nullptr )
		{
			pPassenger->Say( CONTEXT_GLOBAL_CAR_HIT_PED );
		}
	}

	return GetColModel();
}

void CVehicle::SetComponentAtomicAlpha(RpAtomic* pAtomic, int nAlpha)
{
	RpGeometry*	pGeometry = RpAtomicGetGeometry(pAtomic);
	pGeometry->flags |= rpGEOMETRYMODULATEMATERIALCOLOR;

	RpGeometryForAllMaterials( pGeometry, [nAlpha] (RpMaterial* material) {
		material->color.alpha = RwUInt8(nAlpha);
		return material;
	} );
}

bool CVehicle::IgnoresLightbeamFix() const
{
	if ( ms_lightbeamFixOverride != 0 )
	{
		return ms_lightbeamFixOverride < 0;
	}
	return SVF::ModelHasFeature( m_nModelIndex.Get(), SVF::Feature::_INTERNAL_NO_LIGHTBEAM_BFC_FIX );
}

bool CVehicle::CustomCarPlate_TextureCreate(CVehicleModelInfo* pModelInfo)
{
	char		PlateText[CVehicleModelInfo::PLATE_TEXT_LEN+1];
	const char*	pOverrideText = pModelInfo->GetCustomCarPlateText();

	if ( pOverrideText )
		strncpy_s(PlateText, pOverrideText, CVehicleModelInfo::PLATE_TEXT_LEN);
	else
		CCustomCarPlateMgr::GeneratePlateText(PlateText, CVehicleModelInfo::PLATE_TEXT_LEN);

	PlateText[CVehicleModelInfo::PLATE_TEXT_LEN] = '\0';
	PlateTexture = CCustomCarPlateMgr::CreatePlateTexture(PlateText, pModelInfo->m_nPlateType);
	if ( pModelInfo->m_nPlateType != -1 )
		PlateDesign = pModelInfo->m_nPlateType;
	else if ( IsLawEnforcementVehicle() )
		PlateDesign = CCustomCarPlateMgr::GetMapRegionPlateDesign();
	else
 		PlateDesign = random(0, 20) == 0 ? int8_t(random(0, 3)) : CCustomCarPlateMgr::GetMapRegionPlateDesign();

	assert(PlateDesign >= 0 && PlateDesign < 3);

	pModelInfo->m_plateText[0] = '\0';
	pModelInfo->m_nPlateType = -1;

	return true;
}

static std::vector<std::pair<RpMaterial*, RwTexture*>> originalPlateMaterials;
void CVehicle::CustomCarPlate_BeforeRenderingStart(CVehicleModelInfo* pModelInfo)
{
	RpClumpForAllAtomics(reinterpret_cast<RpClump*>(m_pRwObject), [&] (RpAtomic* atomic) -> RpAtomic* {
		RpGeometryForAllMaterials(RpAtomicGetGeometry(atomic), [&] (RpMaterial* material) -> RpMaterial* {
			if ( RwTexture* texture = RpMaterialGetTexture(material) )
			{
				if ( const char* texName = RwTextureGetName(texture) )
				{
					if ( strcmp( texName, "carplate" ) == 0 )
					{
						originalPlateMaterials.emplace_back(material, texture);
						RpMaterialSetTexture(material, PlateTexture);
					}
					else if ( strcmp( texName, "carpback" ) == 0 )
					{
						originalPlateMaterials.emplace_back(material, texture);
						CCustomCarPlateMgr::SetupMaterialPlatebackTexture(material, PlateDesign);
					}
				}
			}

			return material;
		} );
		return atomic;
	} );
}

void CVehicle::CustomCarPlate_AfterRenderingStop(CVehicleModelInfo* pModelInfo)
{
 	for (const auto& platesToRestore : originalPlateMaterials)
	{
		RpMaterialSetTexture(platesToRestore.first, platesToRestore.second);
	}
	originalPlateMaterials.clear();
}

void CVehicle::SetComponentRotation( RwFrame* component, eRotAxis axis, float angle, bool absolute )
{
	if ( component == nullptr ) return;

	CMatrix matrix( RwFrameGetMatrix(component) );
	if ( absolute )
	{
		if ( axis == ROT_AXIS_X ) matrix.SetRotateXOnly(angle);
		else if ( axis == ROT_AXIS_Y ) matrix.SetRotateYOnly(angle);
		else if ( axis == ROT_AXIS_Z ) matrix.SetRotateZOnly(angle);
	}
	else
	{
		const CVector pos = matrix.GetPos();
		matrix.SetTranslateOnly(0.0f, 0.0f, 0.0f);

		if ( axis == ROT_AXIS_X ) matrix.RotateX(angle);
		else if ( axis == ROT_AXIS_Y ) matrix.RotateY(angle);
		else if ( axis == ROT_AXIS_Z ) matrix.RotateZ(angle);

		matrix.GetPos() += pos;
	}
	matrix.UpdateRW();
}

CPed* CVehicle::PickRandomPassenger()
{
	const unsigned int randomNum = static_cast<unsigned int>((static_cast<double>(rand()) / RAND_MAX) * 8.0);
	for ( size_t i = 0; i < 8; i++ )
	{
		const size_t index = (i + randomNum) % 8;
		if ( m_apPassengers[index] != nullptr ) return m_apPassengers[index];
	}

	return nullptr;
}

bool CVehicle::CanThisVehicleBeImpounded() const
{
	const bool bIsBike = m_dwVehicleClass == VEHICLE_BIKE;
	const bool bIsCar = m_dwVehicleClass == VEHICLE_AUTOMOBILE && m_dwVehicleSubClass != VEHICLE_HELI && m_dwVehicleSubClass != VEHICLE_PLANE && m_dwVehicleSubClass != VEHICLE_TRAILER;
	return bIsCar || bIsBike;
}

int32_t CVehicle::GetRemapIndex() const
{
	int32_t remapTxdSlot = m_remapTxdSlot.Get();
	if ( remapTxdSlot == -1 )
	{
		// Original code never checked that variable, hence the bug
		remapTxdSlot = m_remapTxdSlotToLoad.Get();
	}
	if ( remapTxdSlot == -1 )
	{
		return -1;
	}

	const CVehicleModelInfo* modelInfo = static_cast<CVehicleModelInfo*>(ms_modelInfoPtrs[ m_nModelIndex.Get() ]);
	int32_t index = 0;
	for (const auto& remapTxd : modelInfo->m_awRemapTxds)
	{
		const int32_t remapTxdId = remapTxd.Get();
		if (remapTxdId == -1)
		{
			break;
		}
		if (remapTxdId == remapTxdSlot)
		{
			return index;
		}
		index++;
	}
	return -1;
}

void CHeli::Render()
{
	double		dRotorsSpeed, dMovingRotorSpeed;
	const bool	bDisplayRotors = !IgnoresRotorFix();
	const bool	bHasMovingRotor = m_pCarNode[13] != nullptr && bDisplayRotors;
	const bool	bHasMovingRotor2 = m_pCarNode[15] != nullptr && bDisplayRotors;

	m_nTimeTillWeNeedThisCar = CTimer::m_snTimeInMilliseconds + 3000;

	if ( m_fRotorSpeed > 0.0 )
		dRotorsSpeed = std::min(1.7 * (1.0/0.22) * m_fRotorSpeed, 1.5);
	else
		dRotorsSpeed = 0.0;

	dMovingRotorSpeed = dRotorsSpeed - 0.4;
	if ( dMovingRotorSpeed < 0.0 )
		dMovingRotorSpeed = 0.0;

	int			nStaticRotorAlpha = static_cast<int>(std::min((1.5-dRotorsSpeed) * 255.0, 255.0));
	int			nMovingRotorAlpha = static_cast<int>(std::min(dMovingRotorSpeed * 175.0, 175.0));

	if ( m_pCarNode[12] != nullptr )
	{
		RpAtomic*	pOutAtomic = (RpAtomic*)GetCurrentAtomicObject( m_pCarNode[12] );
		if ( pOutAtomic != nullptr )
			SetComponentAtomicAlpha(pOutAtomic, bHasMovingRotor ? nStaticRotorAlpha : 255);
	}

	if ( m_pCarNode[14] != nullptr )
	{
		RpAtomic*	pOutAtomic = (RpAtomic*)GetCurrentAtomicObject( m_pCarNode[14] );
		if ( pOutAtomic != nullptr )
			SetComponentAtomicAlpha(pOutAtomic, bHasMovingRotor2 ? nStaticRotorAlpha : 255);
	}

	if ( m_pCarNode[13] != nullptr )
	{
		RpAtomic*	pOutAtomic = (RpAtomic*)GetCurrentAtomicObject( m_pCarNode[13] );
		if ( pOutAtomic != nullptr )
			SetComponentAtomicAlpha(pOutAtomic, bHasMovingRotor ? nMovingRotorAlpha : 0);
	}

	if ( m_pCarNode[15] != nullptr )
	{
		RpAtomic*	pOutAtomic = (RpAtomic*)GetCurrentAtomicObject( m_pCarNode[15] );
		if ( pOutAtomic != nullptr )
			SetComponentAtomicAlpha(pOutAtomic, bHasMovingRotor2 ? nMovingRotorAlpha : 0);
	}

	CEntity::Render();
}

void CPlane::Render()
{
	double		dRotorsSpeed, dMovingRotorSpeed;
	const bool	bDisplayRotors = !IgnoresRotorFix();
	const bool	bHasMovingProp = m_pCarNode[13] != nullptr && bDisplayRotors;
	const bool	bHasMovingProp2 = m_pCarNode[15] != nullptr && bDisplayRotors;

	m_nTimeTillWeNeedThisCar = CTimer::m_snTimeInMilliseconds + 3000;

	if ( m_fPropellerSpeed > 0.0 )
		dRotorsSpeed = std::min(1.7 * (1.0/0.31) * m_fPropellerSpeed, 1.5);
	else
		dRotorsSpeed = 0.0;

	dMovingRotorSpeed = dRotorsSpeed - 0.4;
	if ( dMovingRotorSpeed < 0.0 )
		dMovingRotorSpeed = 0.0;

	int			nStaticRotorAlpha = static_cast<int>(std::min((1.5-dRotorsSpeed) * 255.0, 255.0));
	int			nMovingRotorAlpha = static_cast<int>(std::min(dMovingRotorSpeed * 175.0, 175.0));

	if ( m_pCarNode[12] != nullptr )
	{
		RpAtomic*	pOutAtomic = (RpAtomic*)GetCurrentAtomicObject( m_pCarNode[12] );
		if ( pOutAtomic != nullptr )
			SetComponentAtomicAlpha(pOutAtomic, bHasMovingProp ? nStaticRotorAlpha : 255);
	}

	if ( m_pCarNode[14] != nullptr )
	{
		RpAtomic*	pOutAtomic = (RpAtomic*)GetCurrentAtomicObject( m_pCarNode[14] );
		if ( pOutAtomic != nullptr )
			SetComponentAtomicAlpha(pOutAtomic, bHasMovingProp2 ? nStaticRotorAlpha : 255);
	}

	if ( m_pCarNode[13] != nullptr )
	{
		RpAtomic*	pOutAtomic = (RpAtomic*)GetCurrentAtomicObject( m_pCarNode[13] );
		if ( pOutAtomic != nullptr )
			SetComponentAtomicAlpha(pOutAtomic, bHasMovingProp ? nMovingRotorAlpha : 0);
	}

	if ( m_pCarNode[15] != nullptr )
	{
		RpAtomic*	pOutAtomic = (RpAtomic*)GetCurrentAtomicObject( m_pCarNode[15] );
		if ( pOutAtomic != nullptr )
			SetComponentAtomicAlpha(pOutAtomic, bHasMovingProp2 ? nMovingRotorAlpha : 0);
	}

	CVehicle::Render();
}

void CPlane::Fix_SilentPatch()
{
	// Reset bouncing panels
	// No reset on Vortex
	for ( ptrdiff_t i = SVF::ModelHasFeature( m_nModelIndex.Get(), SVF::Feature::VORTEX_EXHAUST ) ? 1 : 0; i < 3; i++ )
	{
		m_aBouncingPanel[i].m_nNodeIndex = -1;
	}
}

void CPlane::PreRender()
{
	(this->*(orgPlanePreRender))();

	const int32_t extID = m_nModelIndex.Get();

	auto copyRotation = [&]( size_t src, size_t dest ) {
		if ( m_pCarNode[src] != nullptr && m_pCarNode[dest] != nullptr )
		{
			RwMatrix* lhs = RwFrameGetMatrix( m_pCarNode[dest] );
			const RwMatrix* rhs = RwFrameGetMatrix( m_pCarNode[src] );

			lhs->at = rhs->at;
			lhs->up = rhs->up;
			lhs->right = rhs->right;
			RwMatrixUpdate( lhs );
		}
	};

	if ( SVF::ModelHasFeature( extID, SVF::Feature::EXTRA_AILERONS1 ) )
	{
		copyRotation( 18, 21 );
	}

	if ( SVF::ModelHasFeature( extID, SVF::Feature::EXTRA_AILERONS2 ) )
	{
		copyRotation( 19, 23 );
		copyRotation( 20, 24 );
	}
}

RwFrame* CAutomobile::GetTowBarFrame() const
{
	RwFrame* towBar = m_pCarNode[20];
	if ( towBar == nullptr )
	{
		towBar = m_pCarNode[21];
	}
	return towBar;
}

void CAutomobile::BeforePreRender()
{
	// For rotating engine components
	ms_engineCompSpeed = m_nVehicleFlags.bEngineOn ? CTimer::m_fTimeStep : 0.0f;
}

void CAutomobile::AfterPreRender()
{
	const int32_t extID = m_nModelIndex.Get();
	if ( SVF::ModelHasFeature( extID, SVF::Feature::PHOENIX_FLUTTER ) )
	{
		ProcessPhoenixBlower( extID );
	}

	if ( SVF::ModelHasFeature( extID, SVF::Feature::SWEEPER_BRUSHES ) )
	{
		ProcessSweeper();
	}

	if ( SVF::ModelHasFeature( extID, SVF::Feature::NEWSVAN_DISH ) )
	{
		ProcessNewsvan();
	}
}

void CAutomobile::HideDestroyedWheels_SilentPatch(void (CAutomobile::*spawnFlyingComponentCB)(int, unsigned int), int, unsigned int modelID)
{
	auto trySpawnAndHideWheel = [this, spawnFlyingComponentCB, modelID](int nodeID)
		{
			RwFrame* wheelNode = m_pCarNode[nodeID];
			if (wheelNode != nullptr)
			{
				bool bWheelVisible = false;
				RwFrameForAllObjects(wheelNode, [&bWheelVisible](RwObject* object) -> RwObject*
					{
						if ((rwObjectGetFlags(object) & rpATOMICRENDER) != 0)
						{
							bWheelVisible = true;
							return nullptr;
						}
						return object;
					});

				if (bWheelVisible)
				{
					std::invoke(spawnFlyingComponentCB, this, nodeID, modelID);
					RwFrameForAllObjects(wheelNode, [](RwObject* object)
						{
							rwObjectSetFlags(object, 0);
							return object;
						});
				}
			}
		};


	if (m_DamageManager.GetWheelStatus(0) == 2)
	{
		trySpawnAndHideWheel(5);
	}
	if (m_DamageManager.GetWheelStatus(2) == 2)
	{
		trySpawnAndHideWheel(2);
	}

	// For rear wheels, also hide and spawn the middle wheel (if it exists)
	if (m_DamageManager.GetWheelStatus(1) == 2)
	{
		trySpawnAndHideWheel(6);
		trySpawnAndHideWheel(7);
	}
	if (m_DamageManager.GetWheelStatus(3) == 2)
	{
		trySpawnAndHideWheel(3);
		trySpawnAndHideWheel(4);
	}
}

void CAutomobile::Fix_SilentPatch()
{
	ResetFrames();

	// Reset bouncing panels
	const int32_t extID = m_nModelIndex.Get();
	for ( ptrdiff_t i = (m_pCarNode[21] != nullptr && SVF::ModelHasFeature( extID, SVF::Feature::TOWTRUCK_HOOK )) || (m_pCarNode[17] != nullptr && SVF::ModelHasFeature( extID, SVF::Feature::TRACTOR_HOOK )) ? 1 : 0; i < 3; i++ )
	{
		// Towtruck/Tractor fix
		m_aBouncingPanel[i].m_nNodeIndex = -1;
	}

	// Reset Rhino middle wheels state
	if ( SVF::ModelHasFeature( extID, SVF::Feature::RHINO_WHEELS ) )
	{
		Door[REAR_LEFT_DOOR].SetExtraWheelPositions( 1.0f, 1.0f, 1.0f, 1.0f );
		Door[REAR_RIGHT_DOOR].SetExtraWheelPositions( 1.0f, 1.0f, 1.0f, 1.0f );

		if ( m_pCarNode[3] != nullptr )
		{
			RwObject* object = GetFirstObject( m_pCarNode[3] );
			RpAtomicSetFlags( object, 0 );
		}

		if ( m_pCarNode[6] != nullptr )
		{
			RwObject* object = GetFirstObject( m_pCarNode[6] );
			RpAtomicSetFlags( object, 0 );
		}
	}
}

void CAutomobile::ResetFrames()
{
	RpClump*	pOrigClump = reinterpret_cast<RpClump*>(ms_modelInfoPtrs[ m_nModelIndex.Get() ]->pRwObject);
	if ( pOrigClump != nullptr )
	{
		// Instead of setting frame rotation to (0,0,0) like R* did, obtain the original frame matrix from CBaseNodelInfo clump
		for ( ptrdiff_t i = 8; i < 25; i++ )
		{
			if ( m_pCarNode[i] != nullptr )
			{
				// Find a frame in CBaseModelInfo object
				RwFrame* origFrame = GetFrameFromID( RpClumpGetFrame(pOrigClump), static_cast<int32_t>(i) );
				if ( origFrame != nullptr )
				{
					// Found a frame, reset it
					*RwFrameGetMatrix(m_pCarNode[i]) = *RwFrameGetMatrix(origFrame);
					RwMatrixUpdate(RwFrameGetMatrix(m_pCarNode[i]));
				}
				else
				{
					// Same as original code
					CMatrix matrix( RwFrameGetMatrix(m_pCarNode[i]) );
					const CVector pos( matrix.GetPos() );
					matrix.SetTranslate( pos.x, pos.y, pos.z );
					matrix.UpdateRW();
				}
			}
		}
	}
}

void CAutomobile::ProcessPhoenixBlower( int32_t modelID )
{
	if ( m_pCarNode[20] == nullptr ) return;
	if ( !m_nVehicleFlags.bEngineOn ) return;

	RpClump*	pOrigClump = reinterpret_cast<RpClump*>(ms_modelInfoPtrs[ modelID ]->pRwObject);
	if ( pOrigClump != nullptr )
	{
		RwFrame* origFrame = GetFrameFromID( RpClumpGetFrame(pOrigClump), 20 );
		if ( origFrame != nullptr )
		{
			*RwFrameGetMatrix(m_pCarNode[20]) = *RwFrameGetMatrix(origFrame);
		}
	}

	float finalAngle = 0.0f;
	if ( std::abs(m_fGasPedal) > 0.0f )
	{
		if ( m_fSpecialComponentAngle < 1.3f )
		{
			finalAngle = m_fSpecialComponentAngle = std::min( m_fSpecialComponentAngle + 0.1f * CTimer::m_fTimeStep, 1.3f );
		}
		else
		{
			finalAngle = m_fSpecialComponentAngle + (std::sin( (CTimer::m_snTimeInMilliseconds % 10000) / PHOENIX_FLUTTER_PERIOD ) * PHOENIX_FLUTTER_AMP);
		}
	}
	else
	{
		if ( m_fSpecialComponentAngle > 0.0f )
		{
			finalAngle = m_fSpecialComponentAngle = std::max( m_fSpecialComponentAngle - 0.05f * CTimer::m_fTimeStep, 0.0f );
		}
	}

	SetComponentRotation( m_pCarNode[20], ROT_AXIS_X, finalAngle, false );
}

void CAutomobile::ProcessSweeper()
{
	if ( !m_nVehicleFlags.bEngineOn ) return;

	if ( GetStatus() == STATUS_PLAYER || GetStatus() == STATUS_PHYSICS || GetStatus() == STATUS_SIMPLE )
	{
		const float angle = CTimer::m_fTimeStep * SWEEPER_BRUSH_SPEED;

		SetComponentRotation( m_pCarNode[20], ROT_AXIS_Z, angle, false );
		SetComponentRotation( m_pCarNode[21], ROT_AXIS_Z, -angle, false );
	}
}

void CAutomobile::ProcessNewsvan()
{
	if ( GetStatus() == STATUS_PLAYER || GetStatus() == STATUS_PHYSICS || GetStatus() == STATUS_SIMPLE )
	{
		// TODO: Point at something? Like nearest collectable or safehouse
		m_fGunOrientation += CTimer::m_fTimeStep * 0.05f;
		if ( m_fGunOrientation > 2.0f * PI ) m_fGunOrientation -= 2.0f * PI;
		SetComponentRotation( m_pCarNode[20], ROT_AXIS_Z, m_fGunOrientation );
	}
}

bool CTrailer::GetTowBarPos(CVector& posnOut, bool defaultPos, CVehicle* trailer)
{
	const int32_t modelID = m_nModelIndex.Get();
	if ( SVF::ModelHasFeature( modelID, SVF::Feature::DOUBLE_TRAILER ) )
	{
		if ( m_pCarNode[21] != nullptr )
		{
			const RwMatrix* ltm = RwFrameGetLTM( m_pCarNode[21] );
			posnOut.x = ltm->pos.x;
			posnOut.y = ltm->pos.y;
			posnOut.z = ltm->pos.z;
			return true;
		}

		// Fallback, same as in original CTrailer::GetTowBarPos
		if ( defaultPos )
		{
			posnOut = *GetMatrix() * CVector(0.0f, ms_modelInfoPtrs[ modelID ]->pColModel->boundingBox.vecMin.y - 0.05f, 0.5f - m_fHeightAboveRoad);
			return true;
		}
	}

	return GetTowBarPos_GTA(posnOut, defaultPos, trailer);
}

CVehicle* CStoredCar::RestoreCar_LoadBombOwnership(CVehicle* vehicle)
{
	if (vehicle != nullptr)
	{
		if (m_bombType != 0)
		{
			// Fixup bomb stuff
			if (vehicle->GetClass() == VEHICLE_AUTOMOBILE || vehicle->GetClass() == VEHICLE_BIKE)
			{
				vehicle->SetBombOnBoard(m_bombType);
				vehicle->SetBombOwner(FindPlayerPed());
			}
		}
	}
	return vehicle;
}
