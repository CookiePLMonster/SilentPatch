#include "StdAfx.h"
#include "ModelInfoVC.h"

#include "VehicleVC.h"
#include "ExternalBindings.hpp"

ExternalFunc<RwFrame*(RpClump*,int)> GetFrameFromId("8B 4C 24 0C 89 04 24", -7);

bool CVehicleModelInfo::HasGameBindings_Extras()
{
	return EnsureBindings(GetFrameFromId);
}

RwFrame* CVehicleModelInfo::GetExtrasFrame( RpClump* clump )
{
	RwFrame* frame;
	if ( m_dwType == VEHICLE_TYPE_HELI || m_dwType == VEHICLE_TYPE_BIKE )
	{
		frame = GetFrameFromId.Call( clump, 1 );
		if ( frame == nullptr )
		{
			frame = RpClumpGetFrame( clump );
		}
	}
	else
	{
		frame = RpClumpGetFrame( clump );
	}
	return frame;
}