#pragma once

#include "Maths.h"

#if _GTA_III
#include "../SilentPatchIII/VehicleIII.h"
#elif _GTA_VC
#include "../SilentPatchVC/VehicleVC.h"
#endif

class CStoredCar
{
private:
	int32_t m_modelIndex;
    CVector m_position;
    CVector m_angle;
	uint32_t m_handlingFlags;
	uint8_t m_nPrimaryColor;
	uint8_t m_nSecondaryColor;
	int8_t m_nRadioStation;
	int8_t m_anCompsToUse[2];
	uint8_t m_bombType;

public:
	static CVehicle* (CStoredCar::*orgRestoreCar)();

	CVehicle* RestoreCar_SilentPatch();

	static bool HasGameBindings();
};

static_assert(sizeof(CStoredCar) == 0x28, "Wrong size: CStoredCar");