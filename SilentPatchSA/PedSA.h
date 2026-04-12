#pragma once

#include "GeneralSA.h"
#include "EventsSA.h"

class CEntryExit;
class CEvent;
class CPed;
class CVehicle;

enum ePedType
{
	PEDTYPE_PLAYER1 = 0,
	PEDTYPE_PLAYER2,
	PEDTYPE_PLAYER_NETWORK,
	PEDTYPE_PLAYER_UNUSED,
	PEDTYPE_CIVMALE,
	PEDTYPE_CIVFEMALE,
	PEDTYPE_COP,
	PEDTYPE_GANG1,
	PEDTYPE_GANG2,
	PEDTYPE_GANG3,
	PEDTYPE_GANG4,
	PEDTYPE_GANG5,
	PEDTYPE_GANG6,
	PEDTYPE_GANG7,
	PEDTYPE_GANG8,
	PEDTYPE_GANG9,
	PEDTYPE_GANG10,
	PEDTYPE_DEALER,
	PEDTYPE_MEDIC,
	PEDTYPE_FIRE,
	PEDTYPE_CRIMINAL,
	PEDTYPE_BUM,
	PEDTYPE_PROSTITUTE,
	PEDTYPE_SPECIAL,
	PEDTYPE_MISSION1,
	PEDTYPE_MISSION2,
	PEDTYPE_MISSION3,
	PEDTYPE_MISSION4,
	PEDTYPE_MISSION5,
	PEDTYPE_MISSION6,
	PEDTYPE_MISSION7,
	PEDTYPE_MISSION8,
	PEDTYPE_LAST_PEDTYPE
};

// This structure is very incomplete, but it is good enough for now
class __declspec(novtable) CTask
{
public:
	virtual ~CTask();
	virtual CTask* Clone() const = 0;
	virtual CTask* GetSubTask() const = 0;
	virtual bool IsSimpleTask() const = 0;
	virtual int GetTaskType() const = 0;
	virtual void StopTimer(const CEvent*) { }
	virtual bool MakeAbortable(CPed*, int, const CEvent*) = 0;

public:
	CTask* m_parentTask;
};
static_assert(sizeof(CTask) == 0x8, "Wrong size: CTask");

class __declspec(novtable) CTaskSimple : public CTask
{
public:
	virtual bool IsSimpleTask() const override { return true; }
	virtual bool SetPedPosition(CPed*) { return false; }
};

class __declspec(novtable) CTaskComplex : public CTask
{
private:
	CTask* m_pSubTask;

public:
	virtual bool IsSimpleTask() const override { return false; }
	virtual void SetSubTask(CTask*);
	virtual CTask* CreateNextSubTask(CPed*) = 0;
	virtual CTask* CreateFirstSubTask(CPed*) = 0;
	virtual CTask* ControlSubTask(CPed*) = 0;
};

// Stub
class __declspec(novtable) CTaskSimpleJetPack : public CTaskSimple
{
public:
	void			RenderJetPack(class CPed* pPed);
};

class __declspec(novtable) CTaskComplexSequence : public CTaskComplex
{
public:
	bool Contains(int taskID) const;

public:
	std::byte __pad1[4];
	CTask* m_taskSequence[8];
	std::byte __pad2[16];
};
static_assert(sizeof(CTaskComplexSequence) == 0x40, "Wrong size: CTaskComplexSequence");

class __declspec(novtable) CTaskComplexCarSlowBeDraggedOut : public CTaskComplex
{
public:
	CVehicle* m_pVehicle;
	int	m_status;
};
static_assert(sizeof(CTaskComplexCarSlowBeDraggedOut) == 0x14, "Wrong size: CTaskComplexCarSlowBeDraggedOut");

class CTaskManager
{
public:
	enum
	{
		TASK_PRIORITY_PHYSICAL_RESPONSE = 0,
		TASK_PRIORITY_EVENT_RESPONSE_TEMP,
		TASK_PRIORITY_EVENT_RESPONSE_NONTEMP,
		TASK_PRIORITY_PRIMARY,
		TASK_PRIORITY_DEFAULT,
		TASK_PRIORITY_MAX
	};

	enum
	{
		TASK_SECONDARY_ATTACK = 0,
		TASK_SECONDARY_DUCK,
		TASK_SECONDARY_SAY,
		TASK_SECONDARY_FACIAL_COMPLEX,
		TASK_SECONDARY_PARTIAL_ANIM,
		TASK_SECONDARY_IK,
		TASK_SECONDARY_MAX
	};

	CTask* GetTask(int taskPriority) const { return m_tasks[taskPriority]; }

private:
	CTask* m_tasks[TASK_PRIORITY_MAX];
	CTask* m_tasksSecondary[TASK_SECONDARY_MAX];
	CPed* m_pPed;
};
static_assert(sizeof(CTaskManager) == 0x30, "Wrong size: CTaskManager");

class CPedIntelligence
{
public:
	CTask* GetTaskPrimary() const { return m_taskManager.GetTask(CTaskManager::TASK_PRIORITY_PRIMARY); }
	CEvent* GetCurrentEvent() const { return m_eventHandler.GetCurrentEvent(); }

	class CTaskSimpleJetPack*	GetTaskJetPack() const;

private:
	CPed* m_pPed;
	CTaskManager m_taskManager;
	CEventHandler m_eventHandler;
	CEventGroup m_eventGroup;
};

class CPedFlags
{
public:
	unsigned int bIsStanding : 1;
	unsigned int bWasStanding : 1;
	unsigned int bIsLooking : 1;
	unsigned int bIsRestoringLook : 1;
	unsigned int bIsAimingGun : 1;
	unsigned int bIsRestoringGun : 1;
	unsigned int bCanPointGunAtTarget : 1;
	unsigned int bIsTalking : 1;

	unsigned int bInVehicle : 1;
	unsigned int bIsInTheAir : 1;
	unsigned int bIsLanding : 1;
	unsigned int bHitSomethingLastFrame : 1;
	unsigned int bIsNearCar : 1;
	unsigned int bRenderPedInCar : 1;
	unsigned int bUpdateAnimHeading : 1;
	unsigned int bRemoveHead : 1;

	unsigned int bFiringWeapon : 1;
	unsigned int bHasACamera : 1;
	unsigned int bPedIsBleeding : 1;
	unsigned int bStopAndShoot : 1;
	unsigned int bIsPedDieAnimPlaying : 1;
	unsigned int bStayInSamePlace : 1;
	unsigned int bKindaStayInSamePlace : 1;
	unsigned int bBeingChasedByPolice : 1;

	unsigned int bNotAllowedToDuck : 1;
	unsigned int bCrouchWhenShooting : 1;
	unsigned int bIsDucking : 1;
	unsigned int bGetUpAnimStarted : 1;
	unsigned int bDoBloodyFootprints : 1;
	unsigned int bDontDragMeOutCar : 1;
	unsigned int bStillOnValidPoly : 1;
	unsigned int bAllowMedicsToReviveMe : 1;

	unsigned int bResetWalkAnims : 1;
	unsigned int bOnBoat : 1;
	unsigned int bBusJacked : 1;
	unsigned int bFadeOut : 1;
	unsigned int bKnockedUpIntoAir : 1;
	unsigned int bHitSteepSlope : 1;
	unsigned int bCullExtraFarAway : 1;
	unsigned int bTryingToReachDryLand : 1;

	unsigned int bCollidedWithMyVehicle : 1;
	unsigned int bRichFromMugging : 1;
	unsigned int bChrisCriminal : 1;
	unsigned int bShakeFist : 1;
	unsigned int bNoCriticalHits : 1;
	unsigned int bHasAlreadyBeenRecorded : 1;
	unsigned int bUpdateMatricesRequired : 1;
	unsigned int bFleeWhenStanding : 1;

	unsigned int bMiamiViceCop : 1;
	unsigned int bMoneyHasBeenGivenByScript : 1;
	unsigned int bHasBeenPhotographed : 1;
	unsigned int bIsDrowning : 1;
	unsigned int bDrownsInWater : 1;
	unsigned int bHeadStuckInCollision : 1;
	unsigned int bDeadPedInFrontOfCar : 1;
	unsigned int bStayInCarOnJack : 1;

	unsigned int bDontFight : 1;
	unsigned int bDoomAim : 1;
	unsigned int bCanBeShotInVehicle : 1;
	unsigned int bPushedAlongByCar : 1;
	unsigned int bNeverEverTargetThisPed : 1;
	unsigned int bThisPedIsATargetPriority : 1;
	unsigned int bCrouchWhenScared : 1;
	unsigned int bKnockedOffBike : 1;

	unsigned int bDonePositionOutOfCollision : 1;
	unsigned int bDontRender : 1;
	unsigned int bHasBeenAddedToPopulation : 1;
	unsigned int bHasJustLeftCar : 1;
	unsigned int bIsInDisguise : 1;
	unsigned int bDoesntListenToPlayerGroupCommands : 1;
	unsigned int bIsBeingArrested : 1;
	unsigned int bHasJustSoughtCover : 1;

	unsigned int bKilledByStealth : 1;
	unsigned int bDoesntDropWeaponsWhenDead : 1;
	unsigned int bCalledPreRender : 1;
	unsigned int bBloodPuddleCreated : 1;
	unsigned int bPartOfAttackWave : 1;
	unsigned int bClearRadarBlipOnDeath : 1;
	unsigned int bNeverLeavesGroup : 1;
	unsigned int bTestForBlockedPositions : 1;

	unsigned int bRightArmBlocked : 1;
	unsigned int bLeftArmBlocked : 1;
	unsigned int bDuckRightArmBlocked : 1;
	unsigned int bMidriffBlockedForJump : 1;
	unsigned int bFallenDown : 1;
	unsigned int bUseAttractorInstantly : 1;
	unsigned int bDontAcceptIKLookAts : 1;
	unsigned int bHasAScriptBrain : 1;

	unsigned int bWaitingForScriptBrainToLoad : 1;
	unsigned int bHasGroupDriveTask : 1;
	unsigned int bCanExitCar : 1;
	unsigned int CantBeKnockedOffBike : 2;
	unsigned int bHasBeenRendered : 1;
	unsigned int bIsCached : 1;
	unsigned int bPushOtherPeds : 1;
	unsigned int bHasBulletProofVest : 1;

	unsigned int bUsingMobilePhone : 1;
	unsigned int bUpperBodyDamageAnimsOnly : 1;
	unsigned int bStuckUnderCar : 1;
	unsigned int bKeepTasksAfterCleanUp : 1;
	unsigned int bIsDyingStuck : 1;
	unsigned int bIgnoreHeightCheckOnGotoPointTask : 1;
	unsigned int bForceDieInCar: 1;
	unsigned int bCheckColAboveHead: 1;

	unsigned int bIgnoreWeaponRange : 1;
	unsigned int bDruggedUp : 1;
	unsigned int bWantedByPolice : 1;
	unsigned int bSignalAfterKill: 1;
	unsigned int bCanClimbOntoBoat : 1;
	unsigned int bPedHitWallLastFrame: 1;
	unsigned int bIgnoreHeightDifferenceFollowingNodes: 1;
	unsigned int bMoveAnimSpeedHasBeenSetByTask: 1;

	unsigned int bGetOutUpsideDownCar : 1;
	unsigned int bJustGotOffTrain : 1;
	unsigned int bDeathPickupsPersist : 1;
	unsigned int bTestForShotInVehicle : 1;
	unsigned int bUsedForReplay : 1;
};

class CPlayerPedData
{
public:
    class CWanted* m_Wanted;												// 0x0
    void* m_pClothes;											// 0x4
    CPed* m_ArrestingOfficer;									// 0x8
    CVector2D m_vecFightMovement;									// 0xC
    float m_moveBlendRatio;										// 0x14
    float m_fSprintEnergy;										// 0x18
    float m_fSprintControlCounter;								// 0x1C
    BYTE m_nChosenWeapon;										// 0x20
    BYTE m_nCarDangerCounter;									// 0x21
    DWORD m_nStandStillTimer;									// 0x24
    DWORD m_nHitAnimDelayTimer;									// 0x28
    float m_fAttackButtonCounter;								// 0x2C
    void *m_pDangerCar;											// 0x30

    DWORD m_bStoppedMoving : 1;									// 0x34
    DWORD m_bAdrenaline : 1;
    DWORD m_bHaveTargetSelected : 1;
    DWORD m_bFreeAiming : 1;
    DWORD bCanBeDamaged : 1;
    DWORD bAllMeleeAttackPtsBlocked : 1;
    DWORD m_JustBeenSnacking : 1;
    DWORD m_bRequireHandleBreath : 1;
    
    DWORD m_GroupStuffDisabled : 1;								// 0x35
    DWORD m_GroupAlwaysFollow : 1;
    DWORD m_GroupNeverFollow : 1;
    DWORD m_bInVehicleDontAllowWeaponChange : 1;
    DWORD m_bRenderWeapon : 1;
	DWORD m_bUnused1 : 1;
	DWORD m_bUnused2 : 1;
	DWORD m_bUnused3 : 1;

	DWORD m_bCopMode : 1;										// 0x36

    DWORD m_PlayerGroup;										// 0x38

    DWORD m_AdrenalineEndTime;									// 0x3C
    BYTE m_nDrunkenness;										// 0x40
    BYTE m_bFadeDrunkenness;									// 0x41
    BYTE m_nDrugLevel;											// 0x42
    BYTE m_nScriptLimitToGangSize;								// 0x43
    float m_fBreath;											// 0x44
    DWORD m_MeleeWeaponAnimReferenced;							// 0x48
    DWORD m_MeleeWeaponAnimReferencedExtra;						// 0x4C
    float m_fFPSMoveHeading;									// 0x50
    float m_fLookPitch;											// 0x54
    float m_fSkateBoardSpeed;									// 0x58
    float m_fSkateBoardLean;									// 0x5C

    void *m_pSpecialAtomic;										// 0x60
    float m_fGunSpinSpeed;										// 0x64
    float m_fGunSpinAngle;										// 0x68

    DWORD m_LastTimeFiring;										// 0x6C
    DWORD m_nTargetBone;										// 0x70
    CVector m_vecTargetBoneOffset;								// 0x74
    DWORD m_busFaresCollected;									// 0x80
    BYTE m_bPlayerSprintDisabled;								// 0x84
    BYTE m_bDontAllowWeaponChange;								// 0x85
    BYTE m_bForceInteriorLighting;								// 0x86
    WORD m_DPadDownPressedInMilliseconds;						// 0x88
    WORD m_DPadUpPressedInMilliseconds;							// 0x8A
    BYTE m_wetness;												// 0x8C
    BYTE m_playersGangActive;									// 0x8D
    BYTE m_waterCoverPerc;										// 0x8E
    FLOAT m_waterHeight;										// 0x90
    DWORD m_FireHSMissilePressedTime;							// 0x94
    void* m_LastHSMissileTarget;
    DWORD m_nModelIndexOfLastBuildingShot;
    DWORD m_LastHSMissileLOSTime  :31;
    DWORD m_bLastHSMissileLOS     : 1;
    void* m_pCurrentProstitutePed;
    void* m_pLastProstituteShagged;
};

class NOVMT CPed : public CPhysical
{
public:
	BYTE				__pad1[820];
	CPedFlags			m_nPedFlags;
	class CPedIntelligence*	pPedIntelligence;
	CPlayerPedData*		pPlayerData;
	unsigned char		PedCreatedBy;
	BYTE				__pad7[76];
	int					iMoveAnimGroup;
	BYTE				__pad2[28];
	RwObject*			m_pWeaponObject;
	RwFrame*			m_pMuzzleFlashFrame;
	BYTE				__pad10[52];
	int32_t				m_nPedState;
	std::byte			__pad11[12];
	float				fHealth;
	float				fMaxHealth;
	float				fArmour;
	BYTE				__pad8[12];
	float				m_fCurrentRotation;
	float				m_fTargetRotation;
	BYTE				__pad3[44];
	CVehicle*			m_pMyVehicle;
	BYTE				__pad9[8];
	int32_t				m_nPedType;
	BYTE				__pad4[4];
	CWeapon				weaponSlots[13];
	BYTE				__pad5[12];
	BYTE				m_bActiveWeapon;
	BYTE				__pad65[20];
	unsigned char		bFightingStyle, bFightingStyleExtra;
	BYTE				__pad6[92];
	CEntryExit*			pCurrentEntryExit;
	BYTE				__pad64[12];

public:
	inline bool			Save_Stub()
		{ return CPed::Save(); }
	inline bool			Load_Stub()
		{ return CPed::Load(); }

	virtual bool		Save();
	virtual bool		Load();

	int32_t			GetPedState() const
							{ return m_nPedState; }
	inline int32_t	GetPedType() const
							{ return m_nPedType; };
	inline CWeapon*	GetWeaponSlots() 
							{ return weaponSlots; };
	inline int			GetMoveAnimGroup() 
							{ return iMoveAnimGroup; };
	inline void			SetMoveAnimGroup(int a) 
							{ iMoveAnimGroup = a; };
	inline CPedIntelligence*	GetPedIntelligence() const
							{ return pPedIntelligence; };
	inline float		GetHealth() 
							{ return fHealth; };
	inline float		GetArmour() 
							{ return fArmour; };
	inline CPlayerPedData*	GetPlayerData() 
							{ return pPlayerData; };
	inline BYTE			GetActiveWeapon() 
							{ return m_bActiveWeapon; };

	CVehicle*			GetCurrentVehicle() const
							{ return m_nPedFlags.bInVehicle ? m_pMyVehicle : nullptr; }

	inline void			SetCurrentHeading(float fVal)
		{ m_fCurrentRotation = fVal; }
	inline void			SetTargetHeading(float fVal)
		{ m_fTargetRotation = fVal; }

	bool				IsPlayer() const
		{ return m_nPedType == PEDTYPE_PLAYER1 || m_nPedType == PEDTYPE_PLAYER2; }

	uint8_t				GetWeaponSkill();
	void				ResetGunFlashAlpha();
	void				SetGunFlashAlpha(bool bSecondWeapon);
	int16_t				Say(uint16_t phrase, uint32_t StartTimeDelay = 0, float Probability = 1.0f, bool bOverideSilence = false, bool bForceAudible = false, bool bFrontEnd = false);

	void				RenderWeapon(bool bWeapon, bool bMuzzleFlash, bool bForShadow);
	void				RenderForShadow();

	static void (CPed::*orgGiveWeapon)(uint32_t weapon, uint32_t ammo, bool flag);
	void				GiveWeapon_SP( uint32_t weapon, uint32_t ammo, bool flag );

	// Extension to accommodate for SA-MP hooking GetWeaponSkill in RenderWeaponPedsForPC dynamically
	static inline uint8_t (CPed::*orgGetWeaponSkillForRenderWeaponPedsForPC)() = &GetWeaponSkill;
	uint8_t				GetWeaponSkillForRenderWeaponPedsForPC()
	{
		return std::invoke( orgGetWeaponSkillForRenderWeaponPedsForPC, this );
	}

	uint8_t				GetWeaponSkillForRenderWeaponPedsForPC_SAMP();

	static inline int16_t (CPed::*orgSay)(uint16_t phrase, void* param2, void* volume, void* param4, void* param5, void* param6);
	template<uint16_t blackSample>
	int16_t				Say_SampleBlackList(uint16_t phrase, void* param2, void* volume, void* param4, void* param5, void* param6)
	{
		if ( !(phrase == blackSample) )
		{
			return std::invoke(orgSay, this, phrase, param2, volume, param4, param5, param6);
		}
		return -1;
	}
};

class NOVMT CPlayerPed : public CPed
{
private:
	CPed*				m_pMouseLockOnRecruitPed;
	int					m_iMouseLockOnRecruitTimer;

public:
	CPed* GetMouseLockOnRecruitPed() const { return m_pMouseLockOnRecruitPed; }

	static void (CPlayerPed::*orgDoStuffToGoOnFire)();

	void DoStuffToGoOnFire()
	{
		(this->*orgDoStuffToGoOnFire)();
	}
};

static_assert(sizeof(CPed) == 0x79C, "Wrong size: CPed");
static_assert(sizeof(CPlayerPed) == 0x7A4, "Wrong size: CPlayerPed");

class CClothes
{
public:
	static void (*RebuildPlayer)(CPlayerPed* ped, bool bForReplay);
};

class CAEPedSpeechAudioEntity
{
public:
	std::byte	_pad[0x92];
	int16_t		m_PedType;
};
