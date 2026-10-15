
#include "char.h"
#include "config.h"
#include "entity.h"
#include "sectree.h"
#include "typedef.h"
#include "utils.h"

#include <metin2/log/log.h>

namespace metin2::game {

void CEntity::ViewCleanup()
{
	ENTITY_MAP::iterator it = m_map_view.begin();

	while (it != m_map_view.end())
	{
		CEntity * entity = it->first;
		++it;

		entity->ViewRemove(this, false);
	}

	m_map_view.clear();
}

void CEntity::ViewReencode()
{
	if (m_bIsObserver)
		return;

	EncodeRemovePacket(this);
	EncodeInsertPacket(this);

	ENTITY_MAP::iterator it = m_map_view.begin();

	while (it != m_map_view.end())
	{
		CEntity * entity = (it++)->first;

		EncodeRemovePacket(entity);
		if (!m_bIsObserver)
			EncodeInsertPacket(entity);

		if (!entity->m_bIsObserver)
			entity->EncodeInsertPacket(this);
	}

}

void CEntity::ViewInsert(CEntity * entity, bool recursive)
{
	if (this == entity)
		return;

	ENTITY_MAP::iterator it = m_map_view.find(entity);

	if (m_map_view.end() != it)
	{
		it->second = m_iViewAge;
		return;
	}

	m_map_view.insert(ENTITY_MAP::value_type(entity, m_iViewAge));

	if (!entity->m_bIsObserver)
		entity->EncodeInsertPacket(this);

	if (recursive)
		entity->ViewInsert(this, false);
}

void CEntity::ViewRemove(CEntity * entity, bool recursive)
{
	ENTITY_MAP::iterator it = m_map_view.find(entity);

	if (it == m_map_view.end())
		return;

	m_map_view.erase(it);

	if (!entity->m_bIsObserver)
		entity->EncodeRemovePacket(this);

	if (recursive)
		entity->ViewRemove(this, false);
}

class CFuncViewInsert
{
	private:
		int dwViewRange;

	public:
		CEntity * m_me;

		CFuncViewInsert(CEntity * ent) :
			dwViewRange(metin2::config::game().view_range + VIEW_BONUS_RANGE),
			m_me(ent)
		{
		}

		void operator () (CEntity * ent)
		{
			// Non-objects are not added if they are too far away by computed distance.
			if (!ent->IsType(ENTITY_OBJECT))
				if (DISTANCE_APPROX(ent->GetX() - m_me->GetX(), ent->GetY() - m_me->GetY()) > dwViewRange)
					return;

			// Add me to the target
			m_me->ViewInsert(ent);

			// If both are characters
			if (ent->IsType(ENTITY_CHARACTER) && m_me->IsType(ENTITY_CHARACTER))
			{
				CHARACTER * chMe = (CHARACTER *) m_me;
				CHARACTER * chEnt = (CHARACTER *) ent;

				// If the target is an NPC, turn on its StateMachine.
				if (chMe->IsPC() && !chEnt->IsPC() && !chEnt->IsWarp() && !chEnt->IsGoto())
					chEnt->StartStateMachine();
			}
		}
};

void CEntity::UpdateSectree()
{
	if (!GetSectree())
	{
		if (IsType(ENTITY_CHARACTER))
		{
			CHARACTER * tch = (CHARACTER *) this;
			metin2::log::error("null sectree name: {} {:d} {:d}",  tch->GetName(), GetX(), GetY());
		}

		return;
	}

	++m_iViewAge;

	CFuncViewInsert f(this); // Add me to the people in the sectree
	GetSectree()->ForEachAround(f);

	ENTITY_MAP::iterator it, this_it;

	//
	// Remove unneeded entries from m_map_view
	// 
	if (m_bObserverModeChange)
	{
		if (m_bIsObserver)
		{
			it = m_map_view.begin();

			while (it != m_map_view.end())
			{
				this_it = it++;
				if (this_it->second < m_iViewAge)
				{
					CEntity * ent = this_it->first;

					// Remove the other from me.
					ent->EncodeRemovePacket(this);
					m_map_view.erase(this_it);

					// Remove me from the other.
					ent->ViewRemove(this, false);
				}
				else
				{

					CEntity * ent = this_it->first;

					// Remove the other from me.

					// Remove me from the other.
					EncodeRemovePacket(ent);
				}
			}
		}
		else
		{
			it = m_map_view.begin();

			while (it != m_map_view.end())
			{
				this_it = it++;

				if (this_it->second < m_iViewAge)
				{
					CEntity * ent = this_it->first;

					// Remove the other from me.
					ent->EncodeRemovePacket(this);
					m_map_view.erase(this_it);

					// Remove me from the other.
					ent->ViewRemove(this, false);
				}
				else
				{
					CEntity * ent = this_it->first;
					ent->EncodeInsertPacket(this);
					EncodeInsertPacket(ent);

					ent->ViewInsert(this, true);
				}
			}
		}

		m_bObserverModeChange = false;
	}
	else
	{
		if (!m_bIsObserver)
		{
			it = m_map_view.begin();

			while (it != m_map_view.end())
			{
				this_it = it++;

				if (this_it->second < m_iViewAge)
				{
					CEntity * ent = this_it->first;

					// Remove the other from me.
					ent->EncodeRemovePacket(this);
					m_map_view.erase(this_it);

					// Remove me from the other.
					ent->ViewRemove(this, false);
				}
			}
		}
	}
}
}
