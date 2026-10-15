/*
 *    Filename: queue.c
 * Description: Queue handling
 *
 *      Author: Kim Han-ju (aka. Biyeop, Cronan), Song Young-jin (aka. myevan, Bitjaru)
 */
#include "event_queue.h"

#include "event.h"

#include <climits>

namespace metin2::game {

CEventQueue::~CEventQueue()
{
	for (auto & [key, pElem] : m_queue)
		Delete(pElem);
}

TQueueElement * CEventQueue::Enqueue(LPEVENT pvData, int duration, int pulse)
{
	TQueueElement * pElem = new TQueueElement;

	pElem->pvData = pvData;
	pElem->iStartTime = pulse;
	pElem->iKey = duration + pulse;
	pElem->bCancel = false;

	m_queue.emplace(pElem->iKey, pElem);
	return pElem;
}

TQueueElement * CEventQueue::Dequeue()
{
	if (m_queue.empty())
		return nullptr;

	const auto first = m_queue.begin();
	TQueueElement * pElem = first->second;
	m_queue.erase(first);
	return pElem;
}

void CEventQueue::Delete(TQueueElement * pElem)
{
	delete pElem;
}

int CEventQueue::GetTopKey()
{
	if (m_queue.empty())
		return INT_MAX;

	return m_queue.begin()->first;
}

int CEventQueue::Size()
{
	return static_cast<int>(m_queue.size());
}
}
