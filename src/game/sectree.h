#pragma once

#include "entity.h"
#include "typedef.h"

#include <metin2/log/log.h>

#include <cstdint>
#include <vector>

enum ESectree
{
	SECTREE_SIZE	= 6400,
	CELL_SIZE		= 50
};

struct SECTREE_COORD {
	unsigned            x : 16;
	unsigned            y : 16;
};

typedef union sectreeid
{
	uint32_t		package;
	SECTREE_COORD	coord;
} SECTREEID;

enum
{
	ATTR_BLOCK = (1 << 0),
	ATTR_WATER = (1 << 1),
	ATTR_BANPK = (1 << 2),
	ATTR_OBJECT = (1 << 7),
};

struct FCollectEntity {
	void operator()(CEntity * entity) {
		// Consider removing sanity check after debug pass
		result.push_back(entity);
	}
	template<typename F>
	void ForEach(F& f) {
		std::vector<CEntity *>::iterator it = result.begin();
		for ( ; it != result.end(); ++it) {
			CEntity * entity = *it;
			f(entity);
		}
	}
	typedef std::vector<CEntity *> ListType;
	ListType result; // list collected
};

class CAttribute;

class SECTREE
{
	public:
		friend class SECTREE_MANAGER;
		friend class SECTREE_MAP;

		template <class _Func> CEntity *	find_if (_Func & func) const
		{
			auto it_tree = m_neighbor_list.begin();

			while (it_tree != m_neighbor_list.end())
			{
				ENTITY_SET::iterator it_entity = (*it_tree)->m_set_entity.begin();

				while (it_entity != (*it_tree)->m_set_entity.end())
				{
					if (func(*it_entity))
						return (*it_entity);

					++it_entity;
				}

				++it_tree;
			}

			return nullptr;
		}

		template <class _Func> void ForEachAround(_Func & func)
		{
			// <Factor> Using snapshot copy to avoid side-effects
			FCollectEntity collector;
			SECTREE_LIST::iterator it = m_neighbor_list.begin();
			for ( ; it != m_neighbor_list.end(); ++it)
			{
				SECTREE * sectree = *it;
				sectree->for_each_entity(collector);
			}
			collector.ForEach(func);
		}

		template <class _Func> void for_each_for_find_victim(_Func & func)
		{
			SECTREE_LIST::iterator it_tree = m_neighbor_list.begin();

			while (it_tree != m_neighbor_list.end())
			{
				//Return as soon as the first one is found
				if ( (*(it_tree++))->for_each_entity_for_find_victim(func) )
					return;
			}
		}
		template <class _Func> bool for_each_entity_for_find_victim(_Func & func)
		{
			auto it = m_set_entity.begin();

			while (it != m_set_entity.end())
			{
				//Return as soon as it is found
				if ( func(*it++) )
					return true;
			}
			return false;
		}
		

	public:
		SECTREE();
		~SECTREE();

		void				Initialize();
		void				Destroy();

		SECTREEID			GetID();

		bool				InsertEntity(CEntity * ent);
		void				RemoveEntity(CEntity * ent);

		void				IncreasePC();
		void				DecreasePC();

		void				BindAttribute(CAttribute * pkAttribute);

		uint32_t				GetAttribute(int32_t x, int32_t y);
		bool				IsAttr(int32_t x, int32_t y, uint32_t dwFlag);

		void				CloneAttribute(SECTREE * tree); // Used when handling private maps

		int				GetEventAttribute(int32_t x, int32_t y); // As of 20050313, not used

		void				SetAttribute(uint32_t x, uint32_t y, uint32_t dwAttr);
		void				RemoveAttribute(uint32_t x, uint32_t y, uint32_t dwAttr);

	private:
		template <class _Func> void for_each_entity(_Func & func)
		{
			auto it = m_set_entity.begin();
			for ( ; it != m_set_entity.end(); ++it) {
				CEntity * entity = *it;
				// <Factor> Sanity check
				if (entity->GetSectree() != this) {
					metin2::log::error("<Factor> SECTREE-ENTITY relationship mismatch");
					m_set_entity.erase(it);
					continue;
				}
				func(entity);
			}
		}

		SECTREEID			m_id;
		ENTITY_SET			m_set_entity;
		SECTREE_LIST			m_neighbor_list;
		int				m_iPCCount;
		bool				isClone;

		CAttribute *			m_pkAttribute;
};
