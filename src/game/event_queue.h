#pragma once

#include "event.h"

#include <map>

struct TQueueElement
{
	LPEVENT	pvData;
	int		iStartTime;
	int		iKey;		// the pulse the event is due
	bool	bCancel;
};

// Pending events ordered by due pulse; events due on the same pulse run in the order queued.
class CEventQueue
{
	public:
		CEventQueue() = default;
		~CEventQueue();

		CEventQueue(const CEventQueue &) = delete;
		CEventQueue & operator=(const CEventQueue &) = delete;

		TQueueElement *	Enqueue(LPEVENT data, int duration, int pulse);
		TQueueElement *	Dequeue();
		void		Delete(TQueueElement * pElem);
		int		GetTopKey();
		int		Size();

	private:
		std::multimap<int, TQueueElement *> m_queue;
};
