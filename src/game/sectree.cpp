#include "sectree.h"

#include "char.h"
#include "char_manager.h"
#include "desc_manager.h"
#include "item.h"
#include "item_manager.h"
#include "typedef.h"
#include "utils.h"

#include <libgame/attribute.h>
#include <libthecore/log.h>

#include <cassert>
#include <cstdint>

SECTREE::SECTREE()
{
	Initialize();
}

SECTREE::~SECTREE()
{
	Destroy();
}

void SECTREE::Initialize()
{
	m_id.package = 0;
	m_pkAttribute = nullptr;
	m_iPCCount = 0;
	isClone = false;
}

void SECTREE::Destroy()
{
	if (!m_set_entity.empty())
	{
		sys_err("Sectree: entity set not empty!!");

		ENTITY_SET::iterator it = m_set_entity.begin();

		for ( ; it != m_set_entity.end(); ++it)
		{
			CEntity * ent = *it;

			if (ent->IsType(ENTITY_CHARACTER))
			{
				CHARACTER * ch = (CHARACTER *) ent;

				sys_err("Sectree: destroying character: %s is_pc %d", ch->GetName(), ch->IsPC() ? 1 : 0);

				if (ch->GetDesc())
					DESC_MANAGER::instance().DestroyDesc(ch->GetDesc());
				else
					CHARACTER_MANAGER::instance().DestroyCharacter(ch);
			}
			else if (ent->IsType(ENTITY_ITEM))
			{
				CItem * item = (CItem *) ent;

				sys_err("Sectree: destroying Item: %s", item->GetName());

				ITEM_MANAGER::instance().DestroyItem(item);
			}
			else
				sys_err("Sectree: unknown type: %d", ent->GetType());
		}
	}
	m_set_entity.clear();

	if (!isClone && m_pkAttribute)
	{
		delete m_pkAttribute;
		m_pkAttribute = nullptr;
	}
}

SECTREEID SECTREE::GetID()
{
	return m_id;
}

void SECTREE::IncreasePC()
{
	SECTREE_LIST::iterator it_tree = m_neighbor_list.begin();

	while (it_tree != m_neighbor_list.end())
	{
		++(*it_tree)->m_iPCCount;
		++it_tree;
	}
}

void SECTREE::DecreasePC()
{
	SECTREE_LIST::iterator it_tree = m_neighbor_list.begin();

	while (it_tree != m_neighbor_list.end())
	{
		SECTREE * tree = *it_tree++;

		if (--tree->m_iPCCount <= 0)
		{
			if (tree->m_iPCCount < 0)
			{
				sys_err("tree pc count lower than zero (value %d coord %d %d)", tree->m_iPCCount, tree->m_id.coord.x, tree->m_id.coord.y);
				tree->m_iPCCount = 0;
			}

			ENTITY_SET::iterator it_entity = tree->m_set_entity.begin();

			while (it_entity != tree->m_set_entity.end())
			{
				CEntity * pkEnt = *(it_entity++);

				if (pkEnt->IsType(ENTITY_CHARACTER))
				{
					CHARACTER * ch = (CHARACTER *) pkEnt;
					ch->StopStateMachine();
				}
			}
		}
	}
}

bool SECTREE::InsertEntity(CEntity * pkEnt)
{
	SECTREE * pkCurTree;

	if ((pkCurTree = pkEnt->GetSectree()) == this)
		return false;

	if (m_set_entity.find(pkEnt) != m_set_entity.end()) {
		sys_err("entity %p already exist in this sectree!", pkEnt);
		return false;
	}

	if (pkCurTree)
		pkCurTree->m_set_entity.erase(pkEnt);

	pkEnt->SetSectree(this);

	m_set_entity.insert(pkEnt);

	if (pkEnt->IsType(ENTITY_CHARACTER))
	{
		CHARACTER * pkChr = (CHARACTER *) pkEnt;

		if (pkChr->IsPC())
		{
			IncreasePC();

			if (pkCurTree)
				pkCurTree->DecreasePC();
		}
		else if (m_iPCCount > 0 && !pkChr->IsWarp() && !pkChr->IsGoto()) // If it's not a PC and there's a PC here, start the Idle event.
		{
			pkChr->StartStateMachine();
		}
	}

	return true;
}

void SECTREE::RemoveEntity(CEntity * pkEnt)
{
	ENTITY_SET::iterator it = m_set_entity.find(pkEnt);

	if (it == m_set_entity.end()) {
		return;
	}
	m_set_entity.erase(it);

	pkEnt->SetSectree(nullptr);

	if (pkEnt->IsType(ENTITY_CHARACTER))
	{
		if (((CHARACTER *) pkEnt)->IsPC())
			DecreasePC();
	}
}

void SECTREE::BindAttribute(CAttribute * pkAttribute)
{
	m_pkAttribute = pkAttribute;
}

void SECTREE::CloneAttribute(SECTREE * tree)
{
	m_pkAttribute = tree->m_pkAttribute;
	isClone = true;
}

void SECTREE::SetAttribute(uint32_t x, uint32_t y, uint32_t dwAttr)
{
	assert(m_pkAttribute != nullptr);
	m_pkAttribute->Set(x, y, dwAttr);
}

void SECTREE::RemoveAttribute(uint32_t x, uint32_t y, uint32_t dwAttr)
{
	assert(m_pkAttribute != nullptr);
	m_pkAttribute->Remove(x, y, dwAttr);
}

uint32_t SECTREE::GetAttribute(int32_t x, int32_t y)
{
	assert(m_pkAttribute != nullptr);
	return m_pkAttribute->Get((x % SECTREE_SIZE) / CELL_SIZE, (y % SECTREE_SIZE) / CELL_SIZE);
}

bool SECTREE::IsAttr(int32_t x, int32_t y, uint32_t dwFlag)
{
	if (IS_SET(GetAttribute(x, y), dwFlag))
		return true;

	return false;
}

int SECTREE::GetEventAttribute(int32_t x, int32_t y)
{
	return GetAttribute(x, y) >> 8;
}

