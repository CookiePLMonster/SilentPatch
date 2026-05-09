#pragma once

#include <cstdint>
#include "EntityVC.h"
#include "Maths.h"
#include "TheFLAUtils.h"

enum eVehicleType
{
	VEHICLE_AUTOMOBILE,
	VEHICLE_BOAT,
	VEHICLE_TRAIN,
	VEHICLE_HELI,
	VEHICLE_PLANE,
	VEHICLE_BIKE
};

class CVehicle : public CEntity
{
protected:
	void*		m_pFirstReference;
	int32_t		m_audioEntityId; // TODO: This should really be CPhysical
	uint8_t		__pad5[320];
	void*		m_pDriver;

	uint8_t		__pad1[80];
	uint8_t		m_BombOnBoard : 3;
	uint8_t		__pad2[17];
	class CEntity* m_pBombOwner;
	uint8_t		__pad3[136];
	uint32_t m_dwVehicleClass;


public:
	uint32_t		GetClass() const
		{ return m_dwVehicleClass; }

	void			SetBombOnBoard( uint32_t bombOnBoard )
		{ m_BombOnBoard = bombOnBoard; }
	void			SetBombOwner( class CEntity* owner )
		{ m_pBombOwner = owner; }

	int32_t GetOneShotOwnerID_SilentPatch() const;

	static bool HasMovingBoatRadar(int32_t modelID);
};

class CAutomobile : public CVehicle
{
};

static_assert(sizeof(CVehicle) == 0x2A0, "Wrong size: CVehicle");