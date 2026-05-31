#include "StdAfx.h"
#include "ModelInfoIII.h"
#include "Common.h"

#include "RWUtils.hpp"

ExternalFunc<RpAtomic*(RpAtomic* atomic, void* data)> CVehicleModelInfo::SetEnvironmentMapCB("8B 5C 24 14 C6 44 24", -5);

// This is actually CBaseModelInfo, but we currently don't have it defined
ExternalRef<CVehicleModelInfo*[]> ms_modelInfoPtrs("8B 2C 85 ? ? ? ? 89 E9", 3);
ExternalValue<int32_t> numModelInfos("81 FD ? ? ? ? 7C B7 31 C0", 2);

bool CVehicleModelInfo::HasGameBindings()
{
	return EnsureBindings(SetEnvironmentMapCB, ms_modelInfoPtrs, numModelInfos);
}

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
					RpMaterialGetSurfaceProperties(material)->specular = 0.0f;
				}
				return material;
			});
	}
}

void CSimpleModelInfo::SetNearDistanceForLOD_SilentPatch()
{
	// 100.0f for real LOD's, 0.0f otherwise
	m_lodDistances[2] = _strnicmp( m_name, "lod", 3 ) == 0 ? 100.0f : 0.0f;
}

void CVehicleModelInfo::SetEnvironmentMap_ExtraComps()
{
	std::invoke(orgSetEnvironmentMap, this);

	const int32_t modelID = std::distance(ms_modelInfoPtrs.Get(), std::find(ms_modelInfoPtrs.Get(), ms_modelInfoPtrs.Get()+numModelInfos.Get(), this));
	const bool bRemoveSpecularity = ExtraCompSpecularity::SpecularityExcluded(modelID);

	for (int32_t i = 0; i < m_numComps; i++)
	{
		if (bRemoveSpecularity)
		{
			RemoveSpecularityFromAtomic(m_comps[i]);
		}

		SetEnvironmentMapCB.Call(m_comps[i], m_envMap);
		AttachCarPipeToRwObject(reinterpret_cast<RwObject*>(m_comps[i]));
	}
}
