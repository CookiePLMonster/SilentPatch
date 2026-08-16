#include "StdAfxSA.h"
#include "PlayerInfoSA.h"

#include "ExternalBindings.hpp"

ExternalRef PlayerInFocus(AddressByVersion<uint8_t**>( 0x56E218 + 3, Memory::PatternAndOffset("08 85 C0 79 07 0F B6 05 ? ? ? ? 69 C0 90 01 00 00 8B 80", 8) ));
ExternalRef Players(AddressByVersion<CPlayerInfo (**)[]>( 0x56E225 + 2, Memory::PatternAndOffset("08 85 C0 79 07 0F B6 05 ? ? ? ? 69 C0 90 01 00 00 8B 80", 20) ));

bool HasGameBindings_FindPlayer()
{
	return EnsureBindings(PlayerInFocus, Players);
}

CPlayerPed* FindPlayerPed( int playerID )
{
	return Players.Get()[ playerID < 0 ? PlayerInFocus.Get() : playerID ].GetPlayerPed();
}

CEntity* FindPlayerEntityWithRC( int playerID )
{
	CPlayerInfo* player = &Players.Get()[ playerID < 0 ? PlayerInFocus.Get() : playerID ];

	CPlayerPed* ped = player->GetPlayerPed();
	CVehicle* remoteVehicle = player->GetControlledVehicle();
	if ( remoteVehicle != nullptr ) return remoteVehicle;
	CVehicle* normalVehicle = ped->GetCurrentVehicle();
	if (normalVehicle != nullptr) return normalVehicle;
	return ped;
}

CVehicle* FindPlayerVehicle( int playerID, bool withRC )
{
	CPlayerInfo* player = &Players.Get()[ playerID < 0 ? PlayerInFocus.Get() : playerID ];

	CPlayerPed* ped = player->GetPlayerPed();
	if ( ped == nullptr ) return nullptr;
	if ( !ped->m_nPedFlags.bInVehicle ) return nullptr;

	if (withRC)
	{
		CVehicle* vehicle = player->GetControlledVehicle();
		if (vehicle != nullptr)
		{
			return vehicle;
		}
	}
	return ped->m_pMyVehicle;
}

void CPlayerInfo::CancelPlayerEnteringCars_ImpoundGarage(CVehicle *pVehicle)
{
	bool has_occupant = pVehicle->GetDriver() != nullptr;

    for (auto i = 0; i < pVehicle->GetMaxPassengers() && !has_occupant; ++i)
	{
		has_occupant = pVehicle->GetPassengers()[i] != nullptr;
    }

    if (!has_occupant)
    {
		CWorld::Remove(pVehicle);
		pVehicle->scalar_deleting_dtor();
    }
}
