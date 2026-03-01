#ifndef __VEHICLE
#define __VEHICLE

#include "GeneralSA.h"
#include "ModelInfoSA.h"
#include "PedSA.h"

#include "Utils/HookEach.hpp"

enum eVehicleType
{
    VEHICLE_AUTOMOBILE,
    VEHICLE_MTRUCK,
    VEHICLE_QUAD,
    VEHICLE_HELI,
    VEHICLE_PLANE,
    VEHICLE_BOAT,
    VEHICLE_TRAIN,
    VEHICLE_FHELI,
    VEHICLE_FPLANE,
    VEHICLE_BIKE,
    VEHICLE_BMX,
    VEHICLE_TRAILER
};

struct CVehicleFlags
{
	//0x428
    unsigned char bIsLawEnforcer : 1;
    unsigned char bIsAmbulanceOnDuty : 1;
    unsigned char bIsFireTruckOnDuty : 1;
    unsigned char bIsLocked : 1;
    unsigned char bEngineOn : 1;
    unsigned char bIsHandbrakeOn : 1;
    unsigned char bLightsOn : 1;
    unsigned char bFreebies : 1;

	//0x429
    unsigned char bIsVan : 1;
    unsigned char bIsBus : 1;
    unsigned char bIsBig : 1;
    unsigned char bLowVehicle : 1;
    unsigned char bComedyControls : 1;
    unsigned char bWarnedPeds : 1;
    unsigned char bCraneMessageDone : 1;
    unsigned char bTakeLessDamage : 1;

    //0x42A
    unsigned char bIsDamaged : 1;
    unsigned char bHasBeenOwnedByPlayer  : 1;
    unsigned char bFadeOut : 1;
    unsigned char bIsBeingCarJacked : 1;
    unsigned char bCreateRoadBlockPeds  : 1;
    unsigned char bCanBeDamaged : 1;
    unsigned char bOccupantsHaveBeenGenerated  : 1;
    unsigned char bGunSwitchedOff : 1;

	//0x42B
    unsigned char bVehicleColProcessed  : 1;
    unsigned char bIsCarParkVehicle : 1;
    unsigned char bHasAlreadyBeenRecorded  : 1;
    unsigned char bPartOfConvoy : 1;
    unsigned char bHeliMinimumTilt : 1;
    unsigned char bAudioChangingGear : 1;
    unsigned char bIsDrowning : 1;
    unsigned char bTyresDontBurst : 1;

    //0x42C
    unsigned char bCreatedAsPoliceVehicle : 1;
    unsigned char bRestingOnPhysical : 1;
    unsigned char bParking : 1;
    unsigned char bCanPark : 1;
    unsigned char bFireGun : 1;
    unsigned char bDriverLastFrame : 1;
    unsigned char bNeverUseSmallerRemovalRange : 1;
    unsigned char bIsRCVehicle : 1;

    //0x42D
    unsigned char bAlwaysSkidMarks : 1;
    unsigned char bEngineBroken : 1;
    unsigned char bVehicleCanBeTargetted : 1;
    unsigned char bPartOfAttackWave : 1;
    unsigned char bWinchCanPickMeUp : 1;
    unsigned char bImpounded : 1;
    unsigned char bVehicleCanBeTargettedByHS : 1;
    unsigned char bSirenOrAlarm : 1;

    //0x42E
    unsigned char bHasGangLeaningOn : 1;
    unsigned char bGangMembersForRoadBlock : 1;
    unsigned char bDoesProvideCover : 1;
    unsigned char bMadDriver : 1;
    unsigned char bUpgradedStereo : 1;
    unsigned char bConsideredByPlayer : 1;
    unsigned char bPetrolTankIsWeakPoint : 1;
    unsigned char bDisableParticles : 1;

    //0x42F
    unsigned char bHasBeenResprayed : 1;
    unsigned char bUseCarCheats : 1;
    unsigned char bDontSetColourWhenRemapping : 1;
    unsigned char bUsedForReplay : 1;
};

class CBouncingPanel
{
public:
	short	m_nNodeIndex;
	short	m_nBouncingType;
	float	m_fBounceRange;
	CVector	field_8;
	CVector	m_vecBounceVector;
};

class CDoor
{
private:
	float	m_fOpenAngle;
	float	m_fClosedAngle;
	int16_t	m_nDirn;
	uint8_t	m_nAxis;
	uint8_t	m_nDoorState;
	float	m_fAngle;
	float	m_fPrevAngle;
	float	m_fAngVel;

public:
	void	SetExtraWheelPositions( float openAngle, float closedAngle, float angle, float prevAngle )
	{
		m_fOpenAngle = openAngle;
		m_fClosedAngle = closedAngle;
		m_fAngle = angle;
		m_fPrevAngle = prevAngle;
	}
};

class CDamageManager
{
private:
	float		WheelDamageEffect;
	uint8_t		EngineStatus;
	uint8_t		Wheel[4];
	uint8_t		Door[6];
	uint32_t	Lights;
	uint32_t	Panels;

public:
	uint32_t	GetWheelStatus(int wheelID) const
	{
		return Wheel[wheelID];
	}
};
static_assert(sizeof(CDamageManager) == 0x18, "Wrong size: CDamageManager");

enum eRotAxis
{
	ROT_AXIS_X = 0,
	ROT_AXIS_Y = 1,
	ROT_AXIS_Z = 2
};

enum eDoors
{
	BONNET,
	BOOT,
	FRONT_LEFT_DOOR,
	FRONT_RIGHT_DOOR,
	REAR_LEFT_DOOR,
	REAR_RIGHT_DOOR,

	NUM_DOORS
};

#define FLAG_HYDRAULICS_INSTALLED 0x20000

class CVehicle;
namespace LightbeamFix
{
	void SetCurrentVehicle( CVehicle* vehicle );
}

class NOVMT CVehicle	: public CPhysical
{
protected:
	BYTE			__pad0[596];
	uint32_t		hFlagsLocal;
	BYTE			__pad1[152];
	CVehicleFlags	m_nVehicleFlags;
	BYTE			__pad2[48];
	CPed*			m_pDriver;
	CPed*			m_apPassengers[8];
	BYTE			__pad8[24];
	float			m_fGasPedal;
	float			m_fBrakePedal;
	uint8_t			m_VehicleCreatedBy;
	uint8_t			m_BombOnBoard : 3;
	BYTE			__pad6[35];
	CEntity*		m_pBombOwner;
	signed int		m_nTimeTillWeNeedThisCar;
	BYTE			__pad4[56];
	CEntity*		pDamagingEntity;
	BYTE			__pad3[116];
	char			padpad, padpad2, padpad3;
	int8_t			PlateDesign;
	RwTexture*		PlateTexture;
	BYTE			__pad78[4];
	uint32_t		m_dwVehicleClass;
	uint32_t		m_dwVehicleSubClass;
	FLAUtils::int16 m_remapTxdSlot;
	FLAUtils::int16 m_remapTxdSlotToLoad;
	RwTexture*		m_pRemapTexture;

public:
	CVehicleFlags&	GetVehicleFlags()
						{ return m_nVehicleFlags; }
	CEntity*		GetDamagingEntity()
						{ return pDamagingEntity; }
	uint32_t		GetClass() const
						{ return m_dwVehicleClass; }
	CPed*			GetDriver() const
						{ return m_pDriver;}

	void			SetBombOnBoard( uint32_t bombOnBoard )
						{ m_BombOnBoard = bombOnBoard; }
	void			SetBombOwner( CEntity* owner )
						{ m_pBombOwner = owner; }

	virtual void ProcessControlCollisionCheck();
	virtual void ProcessControlInputs(unsigned char playerNum);
	// component index in m_apModelNodes array
	virtual void GetComponentWorldPosition(int componentId, CVector& posnOut);
	// component index in m_apModelNodes array
	virtual bool IsComponentPresent(int componentId);
	virtual void OpenDoor(CPed* ped, int componentId, eDoors door, float doorOpenRatio, bool playSound);
	virtual void ProcessOpenDoor(CPed* ped, unsigned int doorComponentId, unsigned int arg2, unsigned int arg3, float arg4);
	virtual float GetDooorAngleOpenRatio(unsigned int door);
	virtual float GetDooorAngleOpenRatio(eDoors door);
	virtual bool IsDoorReady(unsigned int door);
	virtual bool IsDoorReady(eDoors door);
	virtual bool IsDoorFullyOpen(unsigned int door);
	virtual bool IsDoorFullyOpen(eDoors door);
	virtual bool IsDoorClosed(unsigned int door);
	virtual bool IsDoorClosed(eDoors door);
	virtual bool IsDoorMissing(unsigned int door);
	virtual bool IsDoorMissing(eDoors door);
	// check if car has roof as extra
	virtual bool IsOpenTopCar() const;
	// remove ref to this entity
	virtual void RemoveRefsToVehicle(CEntity* entity);
	virtual void BlowUpCar(CEntity* damager, unsigned char bHideExplosion);
	virtual void BlowUpCarCutSceneNoExtras(bool bNoCamShake, bool bNoSpawnFlyingComps, bool bDetachWheels, bool bExplosionSound);
	virtual bool SetUpWheelColModel(CColModel* wheelCol);
	// returns false if it's not possible to burst vehicle's tyre or it is already damaged. bPhysicalEffect=true applies random moving force to vehicle
	virtual bool BurstTyre(unsigned char tyreComponentId, bool bPhysicalEffect);
	virtual bool IsRoomForPedToLeaveCar(unsigned int arg0, CVector* arg1);
	virtual void ProcessDrivingAnims(CPed* driver, unsigned char arg1);
	// get special ride anim data for bile or quad
	virtual void* GetRideAnimData();
	virtual void SetupSuspensionLines();
	virtual CVector AddMovingCollisionSpeed(CVector& arg0);
	virtual void Fix();
	virtual void SetupDamageAfterLoad();
	virtual void DoBurstAndSoftGroundRatios();
	virtual float GetHeightAboveRoad();
	virtual void PlayCarHorn();
	virtual int GetNumContactWheels();
	virtual void VehicleDamage(float damageIntensity, unsigned short collisionComponent, CEntity* damager, CVector* vecCollisionCoors, CVector* vecCollisionDirection, eWeaponType weapon);
	virtual bool CanPedStepOutCar(bool arg0);
	virtual bool CanPedJumpOutCar(CPed* ped);
	virtual bool GetTowHitchPos(CVector& posnOut, bool defaultPos, CVehicle* trailer);
	virtual bool GetTowBarPos(CVector& posnOut, bool defaultPos, CVehicle* trailer);
	// always return true
	virtual bool SetTowLink(CVehicle* arg0, bool arg1);
	virtual bool BreakTowLink();
	virtual float FindWheelWidth(bool bRear);
	// always return true
	virtual bool Save();
	// always return true
	virtual bool Load();

	virtual void	Render() override;
	virtual void	PreRender() override;

	bool			CustomCarPlate_TextureCreate(CVehicleModelInfo* pModelInfo);
	void			CustomCarPlate_BeforeRenderingStart(CVehicleModelInfo* pModelInfo);
	void			CustomCarPlate_AfterRenderingStop(CVehicleModelInfo* pModelInfo);

	bool			HasFirelaLadder() const;
	void*			PlayPedHitSample_GetColModel();

	bool			IsLawEnforcementVehicle();
	CPed*			PickRandomPassenger();
	bool			CanThisVehicleBeImpounded() const;

	int32_t			GetRemapIndex();

	static void		SetComponentRotation( RwFrame* component, eRotAxis axis, float angle, bool absolute = true );
	static void		SetComponentAtomicAlpha(RpAtomic* pAtomic, int nAlpha);

	static inline int8_t ms_lightbeamFixOverride = 0, ms_rotorFixOverride = 0; // 0 - normal, 1 - always on, -1 - always off
	bool				IgnoresLightbeamFix() const;
	bool				IgnoresRotorFix() const;

	bool IsOpenTopCarOrQuadbike() const;

private:
	template<std::size_t Index>
	static inline void (CVehicle::*orgDoHeadLightBeam)(int type, CMatrix& m, bool right);

	template<std::size_t Index>
	void DoHeadLightBeam_LightBeamFixSaveObj(int type, CMatrix& m, bool right)
	{
		LightbeamFix::SetCurrentVehicle(this);
		std::invoke(orgDoHeadLightBeam<Index>, this, type, m, right);
		LightbeamFix::SetCurrentVehicle(nullptr);
	}

public:
	HOOK_EACH_INIT(DoHeadLightBeam, orgDoHeadLightBeam, &DoHeadLightBeam_LightBeamFixSaveObj);
};

class NOVMT CAutomobile : public CVehicle
{
public:
	CDamageManager	m_DamageManager;
	CDoor			Door[NUM_DOORS];
	RwFrame*		m_pCarNode[25];
	CBouncingPanel	m_aBouncingPanel[3];
	BYTE			padding[320];
	float			m_fRotorSpeed;
	BYTE			__rotorpad[72];
	float			m_fHeightAboveRoad;
	float			m_fRearHeightAboveRoad;
	BYTE			__moarpad[172];
	float			m_fGunOrientation;
	float			m_fGunElevation;
	float			m_fUnknown;
	float			m_fSpecialComponentAngle;
	BYTE			__pad3[44];

public:
	template<std::size_t Index>
	static inline void (CAutomobile::*orgAutomobilePreRender)();

	template<std::size_t Index>
	void		PreRender_SilentPatch()
	{
		BeforePreRender();
		std::invoke(orgAutomobilePreRender<Index>, this);
		AfterPreRender();
	}

	HOOK_EACH_INIT(PreRender, orgAutomobilePreRender, &PreRender_SilentPatch);

	void HideDestroyedWheels_SilentPatch(void (CAutomobile::*spawnFlyingComponentCB)(int, unsigned int), int nodeID, unsigned int modelID);

	template<std::size_t Index>
	static inline void (CAutomobile::*orgSpawnFlyingComponent)(int, unsigned int);

	template<std::size_t Index>
	void		SpawnFlyingComponent_HideWheels(int nodeID, unsigned int modelID)
	{
		HideDestroyedWheels_SilentPatch(orgSpawnFlyingComponent<Index>, nodeID, modelID);
	}

	HOOK_EACH_INIT(SpawnFlyingComponent, orgSpawnFlyingComponent, &SpawnFlyingComponent_HideWheels);

	void		Fix_SilentPatch();
	RwFrame*	GetTowBarFrame() const;

	static float ms_engineCompSpeed;

private:
	void		BeforePreRender();
	void		AfterPreRender();
	void		ResetFrames();
	void		ProcessPhoenixBlower( int32_t modelID );
	void		ProcessSweeper();
	void		ProcessNewsvan();
};

class NOVMT CHeli : public CAutomobile
{
public:
	inline void			Render_Stub()
	{ CHeli::Render(); }

	virtual void		Render() override;
};

class NOVMT CPlane : public CAutomobile
{
public:
	BYTE				__pad[60];
	float				m_fPropellerSpeed;

public:
	inline void			Render_Stub()
	{ CPlane::Render(); }
	inline void			PreRender_Stub()
	{ CPlane::PreRender(); }

	virtual void		Render() override;
	virtual void		PreRender() override;

	void				Fix_SilentPatch();

	static void (CPlane::*orgPlanePreRender)();
};

class NOVMT CTrailer : public CAutomobile
{
public:
	virtual bool GetTowBarPos(CVector& posnOut, bool defaultPos, CVehicle* trailer) override;

	inline bool			GetTowBarPos_Stub( CVector& pos, bool anyPos, CVehicle* trailer )
	{
		return CTrailer::GetTowBarPos( pos, anyPos, trailer );
	}


	inline bool GetTowBarPos_GTA( CVector& pos, bool anyPos, CVehicle* trailer )
	{
		return std::invoke(orgGetTowBarPos, this, pos, anyPos, trailer);
	}

	static inline bool (CTrailer::*orgGetTowBarPos)(CVector& pos, bool anyPos, CVehicle* trailer);
};

class NOVMT CBoat : public CVehicle
{
	uint8_t			__pad[16];
	RwFrame*		m_pBoatNode[12];
};

class CStoredCar
{
private:
	CVector m_position;
	uint32_t m_handlingFlags;
	uint8_t m_flags;
	uint16_t m_modelIndex;
	uint16_t m_carMods[15];
	uint8_t m_colour[4];
	uint8_t m_radioStation;
	int8_t m_extra[2];
	uint8_t m_bombType;
	uint8_t m_remapIndex;
	uint8_t m_nitro;
	int8_t m_angleX, m_angleY, m_angleZ;

private:
	template<std::size_t Index>
	static inline CVehicle* (CStoredCar::*orgRestoreCar)();

	template<std::size_t Index>
	CVehicle* RestoreCar_SilentPatch()
	{
		return RestoreCar_LoadBombOwnership(std::invoke(orgRestoreCar<Index>, this));
	}

public:
	HOOK_EACH_INIT(RestoreCar, orgRestoreCar, &RestoreCar_SilentPatch);

private:
	CVehicle* RestoreCar_LoadBombOwnership(CVehicle* vehicle);
};

void ReadRotorFixExceptions(const wchar_t* pPath);
void ReadLightbeamFixExceptions(const wchar_t* pPath);

namespace LightbeamFix
{
	void SetCurrentVehicle( CVehicle* vehicle );
}

static_assert(sizeof(CDoor) == 0x18, "Wrong size: CDoor");
static_assert(sizeof(CBouncingPanel) == 0x20, "Wrong size: CBouncingPanel");
static_assert(sizeof(CVehicle) == 0x5A0, "Wrong size: CVehicle");
static_assert(sizeof(CAutomobile) == 0x988, "Wrong size: CAutomobile");
static_assert(sizeof(CStoredCar) == 0x40, "Wrong size: CStoredCar");

#endif