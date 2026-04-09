#include "StdAfxSA.h"
#include "PlayerInfoSA.h"

uint8_t& PlayerInFocus = **AddressByVersion<uint8_t**>( 0x56E218 + 3, Memory::PatternAndOffset("08 85 C0 79 07 0F B6 05 ? ? ? ? 69 C0 90 01 00 00 8B 80", 8) );
CPlayerInfo* const Players = *AddressByVersion<CPlayerInfo**>( 0x56E225 + 2, Memory::PatternAndOffset("08 85 C0 79 07 0F B6 05 ? ? ? ? 69 C0 90 01 00 00 8B 80", 20) );

CPlayerPed* FindPlayerPed( int playerID )
{
	return Players[ playerID < 0 ? PlayerInFocus : playerID ].GetPlayerPed();
}

CEntity* FindPlayerEntityWithRC( int playerID )
{
	CPlayerInfo* player = &Players[ playerID < 0 ? PlayerInFocus : playerID ];

	CPlayerPed* ped = player->GetPlayerPed();
	CVehicle* remoteVehicle = player->GetControlledVehicle();
	if ( remoteVehicle != nullptr ) return remoteVehicle;
	CVehicle* normalVehicle = ped->GetCurrentVehicle();
	if (normalVehicle != nullptr) return normalVehicle;
	return ped;
}

CVehicle* FindPlayerVehicle( int playerID, bool withRC )
{
	CPlayerInfo* player = &Players[ playerID < 0 ? PlayerInFocus : playerID ];

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
