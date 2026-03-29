#pragma once

#include "PedSA.h"
#include "VehicleSA.h"

// Incomplete
class CWanted
{
public:
	int32_t		m_nWantedLevel;
	int32_t		m_nWantedLevelBeforeParole;
	uint32_t	m_LastTimeWantedDecreased;
	uint32_t	m_LastTimeWantedLevelChanged;
	uint32_t	m_TimeOfParole;
	float		m_fMultiplier;
	uint8_t		m_nCopsInPursuit;
	uint8_t		m_nMaxCopsInPursuit;
	uint8_t		m_nMaxCopCarsInPursuit;
};

class CPlayerInfo
{
private:
	CPlayerPed*		m_pPed;
	uint8_t			__pad2[0xAC];
	CVehicle*		m_pControlledVehicle;
	uint8_t			__pad[0xDC];

public:
	CPlayerPed*		GetPlayerPed() const { return m_pPed; }
	CVehicle*		GetControlledVehicle() const { return m_pControlledVehicle; }
};

CPlayerPed* FindPlayerPed( int playerID = -1 );
CEntity* FindPlayerEntityWithRC( int playerID = -1 );
CVehicle* FindPlayerVehicle( int playerID = -1, bool withRC = false );

static_assert(sizeof(CPlayerInfo) == 0x190, "Wrong size: CPlayerInfo");