#pragma once

class CEntity;
class CPed;

class CTask;
class CTaskSimple;

class CEvent
{
public:
	enum eEventType
	{
		EVENT_PED_TO_FLEE = 26,
	};

	virtual ~CEvent();

	virtual int GetEventType() const;
	virtual int GetEventPriority() const;
	virtual int GetLifeTime() const;
	virtual CEvent* Clone() const;
	virtual bool AffectsPed(CPed*) const;
	virtual bool AffectsPedGroup(class CPedGroup*) const;
	virtual bool IsCriminalEvent() const;
	virtual void ReportCriminalEvent(CPed*) const;
	virtual bool HasEditableResponse() const;
	virtual CEntity* GetSourceEntity() const;
	virtual bool TakesPriorityOver(const CEvent&) const;
	virtual float GetLocalSoundLevel() const;
	virtual bool DoInformVehicleOccupants(CPed*) const;

	virtual bool IsValid(CPed* pPed) const;
	virtual bool CanBeInterruptedBySameEvent() const;

private:
	int m_iAccumulatedTime;
	bool m_bIsPersistent;
};
static_assert(sizeof(CEvent) == 0xC, "Wrong size: CEvent");

class CTaskTimer
{
private:
	int m_iStartTime;
	int m_iDuration;
	uint8_t m_bIsActive;
	uint8_t m_bIsStopped;
};
static_assert(sizeof(CTaskTimer) == 0xC, "Wrong size: CTaskTimer");

class CEventHandlerHistory
{
public:
	CEvent* GetCurrentEvent() const;

private:
	CTask* m_pAbortedTask;
	CEvent* m_pCurrentEventActive;
	CEvent* m_pCurrentEventPassive;

	CEvent* m_pStoredEventActive;
	CTaskTimer m_storeTimer;
};
static_assert(sizeof(CEventHandlerHistory) == 0x1C, "Wrong size: CEventHandlerHistory");

class CEventHandler
{
public:
	CEvent* GetCurrentEvent() const { return m_history.GetCurrentEvent(); }

private:
	CPed* m_pPed;

	CEventHandlerHistory m_history;

	CTask* m_pTaskPhysResponse;
	CTask* m_pTaskEventResponse;

	CTaskSimple* m_pTaskSecondaryAim;

	CTaskSimple* m_pTaskSecondarySay;
	CTaskSimple* m_pTaskSecondaryPartialAnim;
};
static_assert(sizeof(CEventHandler) == 0x34, "Wrong size: CEventHandler");

class CEventGroup
{
public:
	virtual ~CEventGroup();

	enum
	{
		MAX_NUM_EVENTS = 16
	};

	CEvent* GetEventOfType(const int iEventType) const;

protected:
	CPed* m_pPed;

	int m_iNoOfEvents;
	CEvent* m_events[MAX_NUM_EVENTS];
};
static_assert(sizeof(CEventGroup) == 0x4C, "Wrong size: CEventGroup");

class CEventPedToFlee : public CEvent
{
public:
	CPed* GetPedToFlee() const { return m_pPedToFlee; }

private:
	CPed* m_pPedToFlee;
};
