#include "StdAfxSA.h"
#include "EventsSA.h"

CEvent* CEventHandlerHistory::GetCurrentEvent() const
{
	return m_pCurrentEventPassive != nullptr ? m_pCurrentEventPassive : m_pCurrentEventActive;
}

CEvent* CEventGroup::GetEventOfType(const int iEventType) const
{
	for (ptrdiff_t i = 0; i < m_iNoOfEvents; ++i)
	{
		if (m_events[i]->GetEventType() == iEventType)
		{
			return m_events[i];
		}
	}
	return nullptr;
}
