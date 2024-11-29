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
    unsigned char bIsLawEnforcer: 1; // Is this guy chasing the player at the moment
    unsigned char bIsAmbulanceOnDuty: 1; // Ambulance trying to get to an accident
    unsigned char bIsFireTruckOnDuty: 1; // Firetruck trying to get to a fire
    unsigned char bIsLocked: 1; // Is this guy locked by the script (cannot be removed)
    unsigned char bEngineOn: 1; // For sound purposes. Parked cars have their engines switched off (so do destroyed cars)
    unsigned char bIsHandbrakeOn: 1; // How's the handbrake doing ?
    unsigned char bLightsOn: 1; // Are the lights switched on ?
    unsigned char bFreebies: 1; // Any freebies left in this vehicle ?

	//0x429
    unsigned char bIsVan: 1; // Is this vehicle a van (doors at back of vehicle)
    unsigned char bIsBus: 1; // Is this vehicle a bus
    unsigned char bIsBig: 1; // Is this vehicle big
    unsigned char bLowVehicle: 1; // Need this for sporty type cars to use low getting-in/out anims
    unsigned char bComedyControls: 1; // Will make the car hard to control (hopefully in a funny way)
    unsigned char bWarnedPeds: 1; // Has scan and warn peds of danger been processed?
    unsigned char bCraneMessageDone: 1; // A crane message has been printed for this car allready
    // unsigned char bExtendedRange: 1; // This vehicle needs to be a bit further away to get deleted
    unsigned char bTakeLessDamage: 1; // This vehicle is stronger (takes about 1/4 of damage)

    //0x42A
    unsigned char bIsDamaged: 1; // This vehicle has been damaged and is displaying all its components
    unsigned char bHasBeenOwnedByPlayer : 1;// To work out whether stealing it is a crime
    unsigned char bFadeOut: 1; // Fade vehicle out
    unsigned char bIsBeingCarJacked: 1;
    unsigned char bCreateRoadBlockPeds : 1;// If this vehicle gets close enough we will create peds (coppers or gang members) round it
    unsigned char bCanBeDamaged: 1; // Set to FALSE during cut scenes to avoid explosions
    // unsigned char bUsingSpecialColModel : 1;
    // Is player vehicle using special collision model, stored in player strucure
    unsigned char bOccupantsHaveBeenGenerated : 1; // Is true if the occupants have already been generated. (Shouldn't happen again)
    unsigned char bGunSwitchedOff: 1; // Level designers can use this to switch off guns on boats
    
	//0x42B
    unsigned char bVehicleColProcessed : 1;// Has ProcessEntityCollision been processed for this car?
    unsigned char bIsCarParkVehicle: 1; // Car has been created using the special CAR_PARK script command
    unsigned char bHasAlreadyBeenRecorded : 1; // Used for replays
    unsigned char bPartOfConvoy: 1;
    unsigned char bHeliMinimumTilt: 1; // This heli should have almost no tilt really
    unsigned char bAudioChangingGear: 1; // sounds like vehicle is changing gear
    unsigned char bIsDrowning: 1; // is vehicle occupants taking damage in water (i.e. vehicle is dead in water)
    unsigned char bTyresDontBurst: 1; // If this is set the tyres are invincible

    //0x42C
    unsigned char bCreatedAsPoliceVehicle : 1;// True if this guy was created as a police vehicle (enforcer, policecar, miamivice car etc)
    unsigned char bRestingOnPhysical: 1; // Dont go static cause car is sitting on a physical object that might get removed
    unsigned char      bParking                    : 1;
    unsigned char      bCanPark                    : 1;
    unsigned char bFireGun: 1; // Does the ai of this vehicle want to fire it's gun?
    unsigned char bDriverLastFrame: 1; // Was there a driver present last frame ?
    unsigned char bNeverUseSmallerRemovalRange: 1;// Some vehicles (like planes) we don't want to remove just behind the camera.
    unsigned char bIsRCVehicle: 1; // Is this a remote controlled (small) vehicle. True whether the player or AI controls it.

    //0x42D
    unsigned char bAlwaysSkidMarks: 1; // This vehicle leaves skidmarks regardless of the wheels' states.
    unsigned char bEngineBroken: 1; // Engine doesn't work. Player can get in but the vehicle won't drive
    unsigned char bVehicleCanBeTargetted : 1;// The ped driving this vehicle can be targetted, (for Torenos plane mission)
    unsigned char bPartOfAttackWave: 1; // This car is used in an attack during a gang war
    unsigned char bWinchCanPickMeUp: 1; // This car cannot be picked up by any ropes.
    unsigned char bImpounded: 1; // Has this vehicle been in a police impounding garage
    unsigned char bVehicleCanBeTargettedByHS  : 1;// Heat seeking missiles will not target this vehicle.
    unsigned char bSirenOrAlarm: 1; // Set to TRUE if siren or alarm active, else FALSE

    //0x42E
    unsigned char bHasGangLeaningOn: 1;
    unsigned char bGangMembersForRoadBlock : 1;// Will generate gang members if NumPedsForRoadBlock > 0
    unsigned char bDoesProvideCover: 1; // If this is false this particular vehicle can not be used to take cover behind.
    unsigned char bMadDriver: 1; // This vehicle is driving like a lunatic
    unsigned char bUpgradedStereo: 1; // This vehicle has an upgraded stereo
    unsigned char bConsideredByPlayer: 1; // This vehicle is considered by the player to enter
    unsigned char bPetrolTankIsWeakPoint : 1;// If false shootong the petrol tank will NOT Blow up the car
    unsigned char bDisableParticles: 1; // Disable particles from this car. Used in garage.

    //0x42F
    unsigned char bHasBeenResprayed: 1; // Has been resprayed in a respray garage. Reset after it has been checked.
    unsigned char bUseCarCheats: 1; // If this is true will set the car cheat stuff up in ProcessControl()
    unsigned char bDontSetColourWhenRemapping : 1;// If the texture gets remapped we don't want to change the colour with it.
    unsigned char bUsedForReplay: 1; // This car is controlled by replay and should be removed when replay is done.
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
	STATIC_INLINE void (CVehicle::*orgDoHeadLightBeam)(int type, CMatrix& m, bool right);

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
	STATIC_INLINE void (CAutomobile::*orgAutomobilePreRender)();

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
	STATIC_INLINE void (CAutomobile::*orgSpawnFlyingComponent)(int, unsigned int);

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
	STATIC_INLINE CVehicle* (CStoredCar::*orgRestoreCar)();
	
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
