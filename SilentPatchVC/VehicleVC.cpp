#include "StdAfx.h"
#include "VehicleVC.h"
#include "SVF.h"

int32_t CVehicle::GetOneShotOwnerID_SilentPatch() const
{
	if ( m_pDriver != nullptr )
	{
		// TODO: Define this as a proper CPhysical
		uintptr_t ptr = reinterpret_cast<uintptr_t>(m_pDriver);
		return *reinterpret_cast<int32_t*>( ptr + 0x64 );
	}

	// Fallback
	return m_audioEntityId;
}

bool CVehicle::HasMovingBoatRadar(int32_t modelID)
{
	return SVF::ModelHasFeature(modelID, SVF::Feature::MOVING_BOAT_RADAR);
}
