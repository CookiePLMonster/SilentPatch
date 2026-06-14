#include "StdAfxSA.h"
#include "ModelInfoSA.h"

ExternalFunc<RwTexture* (const char* pText, signed char nDesign)> CCustomCarPlateMgr::CreatePlateTexture(AddressByVersion<RwTexture*(*)(const char*,signed char)>(0x6FDEA0, 0x6FE6D0, 0x736AC0));
ExternalFunc<signed char ()> CCustomCarPlateMgr::GetMapRegionPlateDesign(AddressByVersion<signed char(*)()>(0x6FD7A0, 0x6FDFD0, 0x7363E0));
ExternalFunc<void (RpMaterial* pMaterial, signed char nDesign)> CCustomCarPlateMgr::SetupMaterialPlatebackTexture(AddressByVersion<void(*)(RpMaterial*,signed char)>(0x6FDE50, 0x6FE680, 0x736A80));

ExternalFunc<bool (char* pBuf, int nLen)> CCustomCarPlateMgr::GeneratePlateText; // Read from InjectDelayedPatches

ExternalRef<CBaseModelInfo*[]> ms_modelInfoPtrs(AddressByVersion<CBaseModelInfo*(**)[]>(0x509CB1, 0x4C0C96, 0x403DB7));

ExternalRef<int8_t[2]> CVehicleModelInfo::ms_compsUsed(AddressByVersion<int8_t(**)[2]>(0x4C973B + 2, Memory::PatternAndOffset("8B CE A2 ? ? ? ? E8", 2 + 1)));
ExternalRef<int8_t[2]> CVehicleModelInfo::ms_compsToUse(AddressByVersion<int8_t(**)[2]>(0x4C8057 + 2, Memory::PatternAndOffset("0F BE C0 C6 05 ? ? ? ? FE 5E", 3 + 2)));

static ExternalRef ms_aDirtTextures(AddressByVersion<RwTexture*(**)[16]>( 0x5D5DCC + 3, 0, 0x5F259C + 3 ));

extern ExternalFunc<RpMaterial*(RpMaterial* material, RwTexture* texture)> fnBind_RpMaterialSetTexture;

bool HasGameBindings_DirtRemapFix()
{
	return EnsureBindings(ms_aDirtTextures, fnBind_RpMaterialSetTexture);
}

bool HasGameBindings_ResetCompsForNoExtras()
{
	return EnsureBindings(CVehicleModelInfo::ms_compsUsed, CVehicleModelInfo::ms_compsToUse);
}

void RemapDirt( CVehicleModelInfo* modelInfo, uint32_t dirtID )
{
	RpMaterial** materials = modelInfo->m_numDirtMaterials > CVehicleModelInfo::IN_PLACE_BUFFER_DIRT_SIZE ? modelInfo->m_dirtMaterials : modelInfo->m_staticDirtMaterials;

	auto& aDirtTextures = ms_aDirtTextures.Get();
	for ( size_t i = 0; i < modelInfo->m_numDirtMaterials; i++ )
	{
		RpMaterialSetTexture( materials[i], aDirtTextures[dirtID] );
	}
}

uint32_t CVehicleModelInfo::GetNumRemaps() const
{
	uint32_t count = 0;
	while ( m_awRemapTxds[count].Get() != -1 && count < _countof(m_awRemapTxds) )
	{
		count++;
	}
	return count;
}

void (CClumpModelInfo::*CVehicleModelInfo::orgShutdown_CarDirtFix)();
void CVehicleModelInfo::Shutdown_CarDirtFix()
{
	delete[] m_dirtMaterials;
	m_dirtMaterials = nullptr;

	std::invoke(orgShutdown_CarDirtFix, this);
}

void CVehicleModelInfo::FindEditableMaterialList()
{
	std::vector<RpMaterial*> editableMaterials;

	auto GetEditableMaterialListCB = [&]( RpAtomic* atomic ) -> RpAtomic* {
		RpGeometryForAllMaterials( RpAtomicGetGeometry(atomic), [&]( RpMaterial* material ) -> RpMaterial* {
			if ( RwTexture* texture = RpMaterialGetTexture(material) )
			{
				if ( const char* texName = RwTextureGetName(texture) )
				{
					if ( strcmp(texName, "vehiclegrunge256") == 0 )
					{
						editableMaterials.push_back( material );
					}
				}
			}
			return material;
		} );
		return atomic;
	};

	RpClumpForAllAtomics(reinterpret_cast<RpClump*>(pRwObject), GetEditableMaterialListCB);

	for ( uint32_t i = 0; i < m_pVehicleStruct->m_nNumExtras; i++ )
		GetEditableMaterialListCB(m_pVehicleStruct->m_apExtras[i]);

	m_numDirtMaterials = editableMaterials.size();
	if ( m_numDirtMaterials > IN_PLACE_BUFFER_DIRT_SIZE )
	{
		m_dirtMaterials = new RpMaterial* [m_numDirtMaterials];
		std::copy( editableMaterials.begin(), editableMaterials.end(), m_dirtMaterials );
	}
	else
	{
		m_dirtMaterials = nullptr;

		// Use existing space instead of allocating new space
		std::copy( editableMaterials.begin(), editableMaterials.end(), m_staticDirtMaterials );
	}

	m_nPrimaryColor = -1;
	m_nSecondaryColor = -1;
	m_nTertiaryColor = -1;
	m_nQuaternaryColor = -1;
}

void CVehicleModelInfo::SetCarCustomPlate()
{
	m_plateText[0] = '\0';
	m_nPlateType = -1;
}

void CVehicleModelInfo::ResetCompsForNoExtras()
{
	auto& compsUsed = ms_compsUsed.Get();
	auto& compsToUse = ms_compsToUse.Get();

	compsUsed[0] = compsUsed[1] = -1;
	compsToUse[0] = compsToUse[1] = -2;
}

bool CCustomCarPlateMgr::HasGameBindings()
{
	// Deliberately not checking CCustomCarPlateMgr::GeneratePlateText, as it's bound later
	return EnsureBindings(CreatePlateTexture, GetMapRegionPlateDesign, SetupMaterialPlatebackTexture);
}

void CCustomCarPlateMgr::SetupClumpAfterVehicleUpgrade(RpClump* pClump, void* /*unused*/, signed char nDesign)
{
	UNREFERENCED_PARAMETER(nDesign);
}