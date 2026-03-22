#include "StdAfxSA.h"
#include "GeneralSA.h"

#include "PedSA.h"
#include "ModelInfoSA.h"
#include "PoolsSA.h"

#include <algorithm>

// Wrappers
static void* EntityRender = AddressByVersion<void*>(0x534310, 0x5347B0, 0x545B30);
WRAPPER void CEntity::Render() { VARJMP(EntityRender); }

static void* varEntityIsVisible = AddressByVersion<void*>( 0x536BC0, Memory::PatternAndOffset("0B F6 41 1C 80 74 05 E9", -5) );
WRAPPER bool CEntity::IsVisible() { VARJMP(varEntityIsVisible); }

static void* varInvertRaster = AddressByVersion<void*>(0x705660, 0x705E90, 0x7497A0);
WRAPPER void CShadowCamera::InvertRaster() { VARJMP(varInvertRaster); }

CWeaponInfo* (*CWeaponInfo::GetWeaponInfo)(eWeaponType, signed char) = AddressByVersion<CWeaponInfo*(*)(eWeaponType, signed char)>(0x743C60, 0x744490, 0x77D940);

static RwTexture*& ms_pRemapTexture = **AddressByVersion<RwTexture***>(0x59F1BD, 0x6D6E53, 0x5B811D);

auto	SetEditableMaterialsCB = AddressByVersion<RpAtomic*(*)(RpAtomic*,void*)>(0x4C83E0, 0x4C8460, 0x4D2CE0);

void*	(CEntity::*CEntity::orgGetColModel)();

static void ResetEditableMaterials(std::pair<void**,void*> pData[], size_t num)
{
	for (size_t i = 0; i < num; ++i)
		*pData[i].first = pData[i].second;
}

RpAtomic* ShadowCameraRenderCB(RpAtomic* pAtomic)
{
	if ( RpAtomicGetFlags(pAtomic) & rpATOMICRENDER )
	{
		RpGeometry*	pGeometry = RpAtomicGetGeometry(pAtomic);

		if ( pGeometry->repEntry != nullptr ) // Only switch to optimized flags if already instanced so as not to break the instanced model
		{
			RwUInt32 geometryFlags = RpGeometryGetFlags(pGeometry);
			pGeometry->flags &= ~(rpGEOMETRYTEXTURED|rpGEOMETRYPRELIT|rpGEOMETRYNORMALS|rpGEOMETRYLIGHT|rpGEOMETRYMODULATEMATERIALCOLOR|rpGEOMETRYTEXTURED2);
			pAtomic = AtomicDefaultRenderCallBack(pAtomic);
			RpGeometrySetFlags(pGeometry, geometryFlags);
		}
		else
		{
			pAtomic = AtomicDefaultRenderCallBack(pAtomic);
		}
	}
	return pAtomic;
}

void CEntity::SetPositionAndAreaCode( CVector position )
{
	SetCoords( position );
	if ( position.z >= 950.0f )
	{
		m_areaCode = 13;
	}
}

void CObject::Render()
{
	if ( m_bDoNotRender || !m_pRwObject )
		return;

	std::pair<void**,void*> materialRestoreData[256];
	size_t numMaterialsToRestore = 0;

	RwScopedRenderState<rwRENDERSTATECULLMODE> cullState;

	const int32_t carPartModelIndex = m_wCarPartModelIndex.Get();
	if ( carPartModelIndex != -1 && m_objectCreatedBy == TEMP_OBJECT && bUseVehicleColours && RwObjectGetType(m_pRwObject) == rpATOMIC )
	{
		auto* pData = materialRestoreData;

		ms_pRemapTexture = m_pPaintjobTex;

		static_cast<CVehicleModelInfo*>(ms_modelInfoPtrs[ carPartModelIndex ])->SetVehicleColour( m_nCarColor[0].Get(), 
						m_nCarColor[1].Get(), m_nCarColor[2].Get(), m_nCarColor[3].Get() );

		SetEditableMaterialsCB(reinterpret_cast<RpAtomic*>(m_pRwObject), &pData);
		assert( pData >= std::begin(materialRestoreData) && pData < std::end(materialRestoreData) );
		numMaterialsToRestore = std::distance(materialRestoreData, pData);

		// Disable backface culling for the part
		RwRenderStateSet(rwRENDERSTATECULLMODE, reinterpret_cast<void*>(rwCULLMODECULLNONE));
	}

	CEntity::Render();

	ResetEditableMaterials(materialRestoreData, numMaterialsToRestore);
}

extern void (*WorldRemove)(CEntity*);
void CObject::TryToFreeUpTempObjects_SilentPatch( int numObjects )
{
	const auto [ numProcessed, numFreed ] = TryOrFreeUpTempObjects( numObjects, false );
	if ( numFreed < numObjects )
	{
		TryOrFreeUpTempObjects( std::min(numProcessed, numObjects - numFreed), true );
	}
}

std::tuple<int,int> CObject::TryOrFreeUpTempObjects( int numObjects, bool force )
{
	int numProcessed = 0, numFreed = 0;

	auto& pool = CPools::GetObjectPool();
	for ( auto obj : pool )
	{
		if ( numFreed >= numObjects ) break;

		if ( pool.IsValidPtr( obj ) )
		{
			if ( obj->m_objectCreatedBy == TEMP_OBJECT )
			{
				numProcessed++;
				if ( force || !obj->IsVisible() )
				{
					numFreed++;
					WorldRemove( obj );
					delete obj;
				}
			}
		}
	}

	return std::make_tuple( numProcessed, numFreed );
}

RwCamera* CShadowCamera::Update(CEntity* pEntity)
{
	RwRGBA	ClearColour = { 255, 255, 255, 0 };
	RwCameraClear(m_pCamera, &ClearColour, rwCAMERACLEARIMAGE|rwCAMERACLEARZ);

	if ( RwCameraBeginUpdate(m_pCamera ) )
	{
		if ( pEntity )
		{
			if ( pEntity->GetType() == ENTITY_TYPE_PED )
				static_cast<CPed*>(pEntity)->RenderForShadow();
			else
				RpClumpForAllAtomics(reinterpret_cast<RpClump*>(pEntity->m_pRwObject), ShadowCameraRenderCB);
		}

		InvertRaster();
		RwCameraEndUpdate(m_pCamera);
	}
	return m_pCamera;
}

std::vector<CEntity*>	CEscalator::ms_entitiesToRemove;
void CEscalator::SwitchOffNoRemove()
{
	if ( !m_bExists )
		return;

	for ( ptrdiff_t i = 0, j = field_7C + field_80 + field_84; i < j; ++i )
	{
		if ( m_pSteps[i] != nullptr )
		{
			ms_entitiesToRemove.push_back( m_pSteps[i] );
			m_pSteps[i] = nullptr;
		}
	}

	m_bExists = false;
}