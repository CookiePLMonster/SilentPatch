#ifndef __GENERAL
#define __GENERAL

#include <stdint.h>
#include "Common.h"
#include "TheFLAUtils.h"

class CSimpleTransform
{
public:
    CVector                         m_translate;
    float                           m_heading;
};

class CPlaceable
{
private:
    CSimpleTransform				m_transform;
    CMatrix*						m_pCoords;

public:
	// Line up the VMTs
	virtual ~CPlaceable() {}

	inline CPlaceable() {}

	explicit inline CPlaceable(int dummy)
	{
		// Dummy ctor
		UNREFERENCED_PARAMETER(dummy);
	}

	inline CVector*					GetCoords()
		{ return m_pCoords ? &m_pCoords->GetPos() : &m_transform.m_translate; }
	inline CMatrix*					GetMatrix()
		{ return m_pCoords; }
	inline CSimpleTransform&		GetTransform()
		{ return m_transform; }
	inline float					GetHeading()
		{ return m_pCoords ? atan2(-m_pCoords->GetUp().x, m_pCoords->GetUp().y) : m_transform.m_heading; }

	inline void						SetCoords(const CVector& pos)
	{	if ( m_pCoords ) { m_pCoords->GetPos() = pos; }
		else m_transform.m_translate = pos; }
	inline void						SetHeading(float fHeading)
		{ if ( m_pCoords ) m_pCoords->SetRotateZOnly(fHeading); else m_transform.m_heading = fHeading; }
};

enum // nStatus
{
	STATUS_PLAYER = 0,
	STATUS_PLAYER_PLAYBACKFROMBUFFER = 1,
	STATUS_SIMPLE = 2,
	STATUS_PHYSICS = 3,
	STATUS_ABANDONED = 4,
	STATUS_WRECKED = 5,

	STATUS_PLAYER_REMOTE = 8,
	STATUS_PLAYER_DISABLED = 9,
	STATUS_TRAILER = 10,
	STATUS_SIMPLE_TRAILER = 11,
	STATUS_GHOST = 12,
};

// TODO: May not be the best place to put it?
class NOVMT CEntity	: public CPlaceable
{
public:
    virtual void	Add_CRect();
    virtual void	Add();
    virtual void	Remove();
    virtual void	SetIsStatic(bool);
    virtual void	SetModelIndex(int nModelIndex);
    virtual void	SetModelIndexNoCreate(int nModelIndex);
    virtual void	CreateRwObject();
    virtual void	DeleteRwObject();
    virtual CRect	GetBoundRect();
    virtual void	ProcessControl();
    virtual void	ProcessCollision();
    virtual void	ProcessShift();
    virtual void	TestCollision();
    virtual void	Teleport();
    virtual void	SpecialEntityPreCollisionStuff();
    virtual void	SpecialEntityCalcCollisionSteps();
    virtual void	PreRender();
    virtual void	Render();
    virtual void	SetupLighting();
    virtual void	RemoveLighting(bool bRemove=true);
    virtual void	FlagToDestroyWhenNextProcessed();

//private:
	RpClump*		m_pRwObject;						// 0x18

    /********** BEGIN CFLAGS (0x1C) **************/
    unsigned long	bUsesCollision : 1;				// does entity use collision
    unsigned long	bCollisionProcessed : 1;			// has object been processed by a ProcessEntityCollision function
    unsigned long	bIsStatic : 1;					// is entity static
    unsigned long	bHasContacted : 1;				// has entity processed some contact forces
    unsigned long	bIsStuck : 1;						// is entity stuck
    unsigned long	bIsInSafePosition : 1;			// is entity in a collision free safe position
    unsigned long	bWasPostponed : 1;				// was entity control processing postponed
    unsigned long	bIsVisible : 1;					//is the entity visible

    unsigned long	bIsBIGBuilding : 1;				// Set if this entity is a big building
    unsigned long	bRenderDamaged : 1;				// use damaged LOD models for objects with applicable damage
    unsigned long	bStreamingDontDelete : 1;			// Dont let the streaming remove this
    unsigned long	bRemoveFromWorld : 1;				// remove this entity next time it should be processed
    unsigned long	bHasHitWall : 1;					// has collided with a building (changes subsequent collisions)
    unsigned long	bImBeingRendered : 1;				// don't delete me because I'm being rendered
    unsigned long	bDrawLast :1;						// draw object last
    unsigned long	bDistanceFade :1;					// Fade entity because it is far away

    unsigned long	bDontCastShadowsOn : 1;			// Dont cast shadows on this object
    unsigned long	bOffscreen : 1;					// offscreen flag. This can only be trusted when it is set to true
    unsigned long	bIsStaticWaitingForCollision : 1; // this is used by script created entities - they are static until the collision is loaded below them
    unsigned long	bDontStream : 1;					// tell the streaming not to stream me
    unsigned long	bUnderwater : 1;					// this object is underwater change drawing order
    unsigned long	bHasPreRenderEffects : 1;			// Object has a prerender effects attached to it
    unsigned long	bIsTempBuilding : 1;				// whether or not the building is temporary (i.e. can be created and deleted more than once)
    unsigned long	bDontUpdateHierarchy : 1;			// Don't update the aniamtion hierarchy this frame

    unsigned long	bHasRoadsignText : 1;				// entity is roadsign and has some 2deffect text stuff to be rendered
    unsigned long	bDisplayedSuperLowLOD : 1;
    unsigned long	bIsProcObject : 1;				// set object has been generate by procedural object generator
    unsigned long	bBackfaceCulled : 1;				// has backface culling on
    unsigned long	bLightObject : 1;					// light object with directional lights
    unsigned long	bUnimportantStream : 1;			// set that this object is unimportant, if streaming is having problems
    unsigned long	bTunnel : 1;						// Is this model part of a tunnel
    unsigned long	bTunnelTransition : 1;			// This model should be rendered from within and outside of the tunnel
    /********** END CFLAGS **************/

    WORD			RandomSeed;					// 0x20
    FLAUtils::int16	m_nModelIndex;				// 0x22
    void*			pReferences;				// 0x24
    void*			m_pLastRenderedLink;		// 0x28
    WORD			m_nScanCode;				// 0x2C
    BYTE			m_iplIndex;					// 0x2E
    BYTE			m_areaCode;					// 0x2F
    CEntity*		m_pLod;					// 0x30
    BYTE			numLodChildren;				// 0x34
    char			numLodChildrenRendered;		// 0x35

    //********* BEGIN CEntityInfo **********//
    uint8_t			nType : 3;							// what type is the entity	// 0x36 (2 == Vehicle)
	uint8_t			nStatus : 5;						// control status			// 0x36
    //********* END CEntityInfo ************//

public:
	static void*	(CEntity::*orgGetColModel)();

public:
	uint8_t	GetStatus() const { return nStatus; }

	void* GetColModel() { return std::invoke(orgGetColModel, this); }

	bool IsVisible();

	void SetPositionAndAreaCode( CVector position );
};

class NOVMT CPhysical : public CEntity
{
private:
    float			pad1; // 56
    int				__pad2; // 60

    unsigned int	b0x01 : 1; // 64
    unsigned int	bApplyGravity : 1;
    unsigned int	bDisableFriction : 1;
    unsigned int	bCollidable : 1;
    unsigned int	b0x10 : 1;
    unsigned int	bDisableMovement : 1;
    unsigned int	b0x40 : 1;
    unsigned int	b0x80 : 1;

    unsigned int	bSubmergedInWater : 1; // 65
    unsigned int	bOnSolidSurface : 1;
    unsigned int	bBroken : 1;
    unsigned int	b0x800 : 1; // ref @ 0x6F5CF0
    unsigned int	b0x1000 : 1;//
    unsigned int	b0x2000 : 1;//
    unsigned int	b0x4000 : 1;//
    unsigned int	b0x8000 : 1;//

    unsigned int	b0x10000 : 1; // 66
    unsigned int	b0x20000 : 1; // ref @ CPhysical__processCollision
    unsigned int	bBulletProof : 1;
    unsigned int	bFireProof : 1;
    unsigned int	bCollisionProof : 1;
    unsigned int	bMeeleProof : 1;
    unsigned int	bInvulnerable : 1;
    unsigned int	bExplosionProof : 1;

    unsigned int	b0x1000000 : 1; // 67
    unsigned int	bAttachedToEntity : 1;
    unsigned int	b0x4000000 : 1;
    unsigned int	bTouchingWater : 1;
    unsigned int	bEnableCollision : 1;
    unsigned int	bDestroyed : 1;
    unsigned int	b0x40000000 : 1;
    unsigned int	b0x80000000 : 1;

    CVector			m_vecLinearVelocity; // 68
    CVector			m_vecAngularVelocity; // 80
    CVector			m_vecCollisionLinearVelocity; // 92
    CVector			m_vecCollisionAngularVelocity; // 104
    CVector			m_vForce;							// 0x74
    CVector			m_vTorque;							// 0x80
	float			fMass;								// 0x8C
    float			fTurnMass;							// 0x90
    float			m_fVelocityFrequency;					// 0x94
    float			m_fAirResistance;						// 0x98
    float			m_fElasticity;						// 0x9C
    float			m_fBuoyancyConstant;					// 0xA0

    CVector			vecCenterOfMass;					// 0xA4
    DWORD			dwUnk1;								// 0xB0
    void*			unkCPtrNodeDoubleLink;				// 0xB4
    BYTE			byUnk: 8;								// 0xB8
    BYTE			byCollisionRecords: 8;					// 0xB9
    BYTE			byUnk2: 8;								// 0xBA (Baracus)
    BYTE			byUnk3: 8;								// 0xBB

    float			pad4[6];								// 0xBC

    float			fDistanceTravelled;					// 0xD4
    float			fDamageImpulseMagnitude;				// 0xD8
    CEntity*		damageEntity;						// 0xDC
    BYTE			pad2[28];								// 0xE0
    CEntity*		pAttachedEntity;					// 0xFC
    CVector			m_vAttachedPosition;				// 0x100
    CVector			m_vAttachedRotation;				// 0x10C
    BYTE			pad3[20];								// 0x118
    float			fLighting;							// 0x12C col lighting? CPhysical::GetLightingFromCol
    float			fLighting_2;							// 0x130 added to col lighting in CPhysical::GetTotalLighting
    BYTE			pad3a[4];								// 0x134

public:
	virtual void*	ProcessEntityCollision(CEntity*, void*);
};

enum // m_objectCreatedBy
{
	GAME_OBJECT = 1,
	MISSION_OBJECT = 2,
	TEMP_OBJECT = 3,
	MISSION_BRAIN_OBJECT = 6,
};

class NOVMT CObject : public CPhysical
{
public:
	void*				m_pObjectList;
	uint8_t				m_objectCreatedBy;
	__int8				field_13D;
	__int16				field_13E;
	bool				bObjectFlag0 : 1;
	bool				bObjectFlag1 : 1;
	bool				bObjectFlag2 : 1;
	bool				bObjectFlag3 : 1;
	bool				bObjectFlag4 : 1;
	bool				bObjectFlag5 : 1;
	bool				m_bIsExploded : 1;
	bool				bUseVehicleColours : 1;
	bool				m_bIsLampPost : 1;
	bool				m_bIsTargetable : 1;
	bool				m_bIsBroken : 1;
	bool				m_bTrainCrossEnabled : 1;
	bool				m_bIsPhotographed : 1;
	bool				m_bIsLiftable : 1;
	bool				bObjectFlag14 : 1;
	bool				m_bIsDoor : 1;
	bool				m_bHasNoModel : 1;
	bool				m_bIsScaled : 1;
	bool				m_bCanBeAttachedToMagnet : 1;
	bool				bObjectFlag19 : 1;
	bool				bObjectFlag20 : 1;
	bool				bObjectFlag21 : 1;
	bool				m_bFadingIn : 1;
	bool				m_bAffectedByColBrightness : 1;
	bool				bObjectFlag24 : 1;
	bool				m_bDoNotRender : 1;
	bool				m_bFadingIn2 : 1;
	bool				bObjectFlag27 : 1;
	bool				bObjectFlag28 : 1;
	bool				bObjectFlag29 : 1;
	bool				bObjectFlag30 : 1;
	bool				bObjectFlag31 : 1;
	unsigned char		m_nColDamageEffect;
	__int8 field_145;
	__int8 field_146;
	__int8 field_147;
	unsigned char		m_nLastWeaponDamage;
	unsigned char		m_nColBrightness;
	FLAUtils::int16     m_wCarPartModelIndex;
	// this is used for detached car parts
	FLAUtils::int8      m_nCarColor[4];
	// time when this object must be deleted
	unsigned __int32    m_dwRemovalTime;
	float               m_fHealth;
	// this is used for door objects
	float               m_fDoorStartAngle;
	float               m_fScale;
	void*				m_pObjectInfo;
	void*				m_pFire; // CFire *
	short				field_168;
	// this is used for detached car parts
	short				m_wPaintjobTxd;
	// this is used for detached car parts
	RwTexture*          m_pPaintjobTex;
	void*				m_pDummyObject;
	// time when particles must be stopped
	signed int			m_dwTimeToStopParticles;
	float               m_fParticlesIntensity;

public:
	inline void			Render_Stub()
	{ CObject::Render(); }

	virtual void		Render() override;

	static void					TryToFreeUpTempObjects_SilentPatch( int numObjects );
	static std::tuple<int,int>	TryOrFreeUpTempObjects( int numObjects, bool force );
};

class CZoneInfo
{
public:
	unsigned char			GangDensity[10];
	unsigned char			DrugDealerCounter;
	CRGBA					ZoneColour;
	bool					unk1 : 1;
	bool					unk2 : 1;
	bool					unk3 : 1;
	bool					unk4 : 1;
	bool					unk5 : 1;
	bool					bUseColour : 1;
	bool					bInGangWar : 1;
	bool					bNoCops : 1;
	unsigned char			flags;
};

enum eWeaponSkill
{
    WEAPONSKILL_POOR = 0,
    WEAPONSKILL_STD,
    WEAPONSKILL_PRO,
    WEAPONSKILL_SPECIAL,    // for cops using pistols differently for example
    WEAPONSKILL_MAX_NUMBER
};

enum eFireType
{
    FIRETYPE_MELEE,
    FIRETYPE_INSTANT_HIT,
    FIRETYPE_PROJECTILE,
    FIRETYPE_AREA_EFFECT,
    FIRETYPE_CAMERA,
    FIRETYPE_USE,

    FIRETYPE_LAST_FIRETYPE
};

enum eWeaponSlot
{
    WEAPONSLOT_TYPE_UNARMED = 0,
    WEAPONSLOT_TYPE_MELEE,
    WEAPONSLOT_TYPE_HANDGUN,
    WEAPONSLOT_TYPE_SHOTGUN,
    WEAPONSLOT_TYPE_SMG,        //4
    WEAPONSLOT_TYPE_MG, 
    WEAPONSLOT_TYPE_RIFLE,
    WEAPONSLOT_TYPE_HEAVY,
    WEAPONSLOT_TYPE_THROWN,
    WEAPONSLOT_TYPE_SPECIAL,    //9
    WEAPONSLOT_TYPE_GIFT,       //10
    WEAPONSLOT_TYPE_PARACHUTE,  //11
    WEAPONSLOT_TYPE_DETONATOR,  //12

    WEAPONSLOT_MAX
};

enum eWeaponState
{
    WEAPONSTATE_READY,
    WEAPONSTATE_FIRING,
    WEAPONSTATE_RELOADING,
    WEAPONSTATE_OUT_OF_AMMO,
    WEAPONSTATE_MELEE_MADECONTACT
};

/**
 * Contains the weapon types/models
 */
enum eWeaponType
{
    WEAPONTYPE_UNARMED=0,
    WEAPONTYPE_BRASSKNUCKLE, 
    WEAPONTYPE_GOLFCLUB,
    WEAPONTYPE_NIGHTSTICK,
    WEAPONTYPE_KNIFE,
    WEAPONTYPE_BASEBALLBAT,
    WEAPONTYPE_SHOVEL,
    WEAPONTYPE_POOL_CUE,
    WEAPONTYPE_KATANA,
    WEAPONTYPE_CHAINSAW,
    
    // gifts
    WEAPONTYPE_DILDO1, // 10
    WEAPONTYPE_DILDO2,
    WEAPONTYPE_VIBE1,
    WEAPONTYPE_VIBE2,
    WEAPONTYPE_FLOWERS,
    WEAPONTYPE_CANE,

    WEAPONTYPE_GRENADE,
    WEAPONTYPE_TEARGAS,
    WEAPONTYPE_MOLOTOV,
    WEAPONTYPE_ROCKET,
    WEAPONTYPE_ROCKET_HS, // 20
    WEAPONTYPE_FREEFALL_BOMB,

    // FIRST SKILL WEAPON
    WEAPONTYPE_PISTOL,          // handguns
    WEAPONTYPE_PISTOL_SILENCED,
    WEAPONTYPE_DESERT_EAGLE,
    WEAPONTYPE_SHOTGUN,         // shotguns
    WEAPONTYPE_SAWNOFF_SHOTGUN, // one handed
    WEAPONTYPE_SPAS12_SHOTGUN,
    WEAPONTYPE_MICRO_UZI,       // submachine guns
    WEAPONTYPE_MP5,
    WEAPONTYPE_AK47, // 30      // machine guns 
    WEAPONTYPE_M4,          
    WEAPONTYPE_TEC9,            // this uses stat from the micro_uzi
    // END SKILL WEAPONS
    
    WEAPONTYPE_COUNTRYRIFLE,    // rifles
    WEAPONTYPE_SNIPERRIFLE, 
    WEAPONTYPE_ROCKETLAUNCHER,  // specials
    WEAPONTYPE_ROCKETLAUNCHER_HS,
    WEAPONTYPE_FLAMETHROWER,
    WEAPONTYPE_MINIGUN,
    WEAPONTYPE_REMOTE_SATCHEL_CHARGE,
    WEAPONTYPE_DETONATOR, // 40 // plastic explosive
    WEAPONTYPE_SPRAYCAN,
    WEAPONTYPE_EXTINGUISHER,
    WEAPONTYPE_CAMERA,
    WEAPONTYPE_NIGHTVISION,
    WEAPONTYPE_INFRARED,
    WEAPONTYPE_PARACHUTE,
    WEAPONTYPE_LAST_WEAPONTYPE,

    WEAPONTYPE_ARMOUR,
    // these are possible ways to die
    WEAPONTYPE_RAMMEDBYCAR,
    WEAPONTYPE_RUNOVERBYCAR, // 50
    WEAPONTYPE_EXPLOSION,
    WEAPONTYPE_UZI_DRIVEBY,
    WEAPONTYPE_DROWNING,
    WEAPONTYPE_FALL,
    WEAPONTYPE_UNIDENTIFIED,    // Used for damage being done
    WEAPONTYPE_ANYMELEE,
    WEAPONTYPE_ANYWEAPON,
    WEAPONTYPE_FLARE,
};

class CWeapon
{
public:
	eWeaponType		m_eWeaponType;
	eWeaponState	m_eState;
	int				m_nAmmoInClip;
	int				m_nAmmoTotal;
	int				m_nTimer;
	int				m_Unknown;
	void*			m_pParticle;
};

class CWeaponInfo
{
public:
	DWORD				weaponType;
	DWORD				targetRange;
	DWORD				weaponRange;
	int					dwModelID;
	int					dwModelID2;
	int					nSlot;
	DWORD				hexFlags;
	DWORD				animStyle;
	WORD				ammoClip;
	DWORD				fireOffsetX;
	DWORD				fireOffsetY;
	DWORD				fireOffsetZ;
	DWORD				skillLevel;
	DWORD				reqStatLevel;
	float				accuracy;
	DWORD				moveSpeed;
	DWORD				animLoopStart;
	DWORD				animLoopEnd;
	DWORD				animLoopFire;
	DWORD				animLoop2Start;
	DWORD				animLoop2End;
	DWORD				animLoop2Fire;
	DWORD				breakoutTime;
	DWORD				speed;
	DWORD				radius;
	DWORD				lifespan;
	DWORD				spread;
	DWORD				animStyle2;

public:
	inline float				GetAccuracy() 
							{ return accuracy; };
	inline DWORD				GetWeaponType() 
							{ return weaponType; };
	inline DWORD				GetClipSize() 
							{ return ammoClip; };
	inline DWORD				GetWeaponSlot() 
							{ return nSlot; };

	static CWeaponInfo*		(*GetWeaponInfo)(eWeaponType weaponID, signed char bType);
};

class CShadowCamera
{
public:
	RwCamera*		m_pCamera;
	RwTexture*		m_pTexture;

public:
	void			InvertRaster();

	RwCamera*		Update(CEntity* pEntity);
};

#include <vector>

class CEscalator
{
private:
	CVector		field_0;
	CVector		field_C;
	CVector		field_18;
	CVector		field_24;
	CMatrix		m_matrix;
	bool		field_78;
	bool		m_bExists;
	bool		field_7A;
	int			field_7C;
	int			field_80;
	int			field_84;
	char		gap_88[8];
	CVector		field_90;
	float		field_9C;
	float		field_A0;
	CEntity*	m_pMainEntity;
	CEntity*	m_pSteps[42];

public:
	static std::vector<CEntity*>	ms_entitiesToRemove;

public:
	void		SwitchOffNoRemove();
};


RpAtomic* ShadowCameraRenderCB(RpAtomic* pAtomic);

static_assert(sizeof(CEntity) == 0x38, "Wrong size: CEntity");
static_assert(sizeof(CPhysical) == 0x138, "Wrong size: CPhysical");
static_assert(sizeof(CObject) == 0x17C, "Wrong size: CObject");
static_assert(sizeof(CEscalator) == 0x150, "Wrong size: CEscalator");

#endif
