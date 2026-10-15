#include "affect.h"
#include "char.h"
#include "entity.h"
#include "sectree.h"
#include "shop_manager.h"
#include "typedef.h"
#include "utils.h"

#include <common/length.h>
#include <common/tables.h>
#include <libthecore/log.h>

#include <cstdint>
#include <cstdlib>
#include <limits>

int	OnClickShop(TRIGGERPARAM);
int	OnClickTalk(TRIGGERPARAM);

int	OnAttackDefault(TRIGGERPARAM);

struct TTriggerFunction {
	int (*func) (TRIGGERPARAM);
};

TTriggerFunction OnClickTriggers[ON_CLICK_MAX_NUM] =
{
	{ nullptr,          	},	// ON_CLICK_NONE,
	{ OnClickShop,	},	// ON_CLICK_SHOP,
};

void CHARACTER::AssignTriggers(const TMobTable * table)
{
	if (table->bOnClickType >= ON_CLICK_MAX_NUM)
	{
		sys_err("%s has invalid OnClick value %d", GetName(), table->bOnClickType);
		abort();
	}

	m_triggerOnClick.bType = table->bOnClickType;
	m_triggerOnClick.pFunc = OnClickTriggers[table->bOnClickType].func;
}

/*
 * ON_CLICK
 */
int OnClickShop(TRIGGERPARAM)
{
	CShopManager::instance().StartShopping(causer, ch);
	return 1;
}

/*
 * monster AI functions changed into the BattleAI class
 */

class FuncFindMobVictim
{
	public:
		FuncFindMobVictim(CHARACTER * pkChr, int iMaxDistance) :
			m_pkChr(pkChr),
			m_iMinDistance(std::numeric_limits<int>::max()),
			m_iMaxDistance(iMaxDistance),
			m_lx(pkChr->GetX()),
			m_ly(pkChr->GetY()),
			m_pkChrVictim(nullptr),
			m_pkChrBuilding(nullptr)
	{
	};

		bool operator () (CEntity * ent)
		{
			if (!ent->IsType(ENTITY_CHARACTER))
				return false;

			CHARACTER * pkChr = (CHARACTER *) ent;

			if (pkChr->IsBuilding() && 
				(pkChr->IsAffectFlag(AFF_BUILDING_CONSTRUCTION_SMALL) ||
				 pkChr->IsAffectFlag(AFF_BUILDING_CONSTRUCTION_LARGE) ||
				 pkChr->IsAffectFlag(AFF_BUILDING_UPGRADE)))
			{
				m_pkChrBuilding = pkChr;
			}

			if (pkChr->IsNPC())
			{
				if ( !pkChr->IsMonster() || !m_pkChr->IsAttackMob() || m_pkChr->IsAggressive()  )
					return false;
					
			}

			if (pkChr->IsDead())
				return false;

			if (pkChr->IsAffectFlag(AFF_EUNHYUNG) || 
					pkChr->IsAffectFlag(AFF_INVISIBILITY) ||
					pkChr->IsAffectFlag(AFF_REVIVE_INVISIBLE))
				return false;

			if (pkChr->IsAffectFlag(AFF_TERROR) && m_pkChr->IsImmune(IMMUNE_TERROR) == false )	// fear handling
			{
				if ( pkChr->GetLevel() >= m_pkChr->GetLevel() )
					return false;
			}

		 	if ( m_pkChr->IsNoAttackShinsu() )
			{
				if ( pkChr->GetEmpire() == 1 )
					return false;
			}

			if ( m_pkChr->IsNoAttackChunjo() )
			{
				if ( pkChr->GetEmpire() == 2 )
					return false;
			}
			

			if ( m_pkChr->IsNoAttackJinno() )
			{
				if ( pkChr->GetEmpire() == 3 )
					return false;
			}

			int iDistance = DISTANCE_APPROX(m_lx - pkChr->GetX(), m_ly - pkChr->GetY());

			if (iDistance < m_iMinDistance && iDistance <= m_iMaxDistance)
			{
				m_pkChrVictim = pkChr;
				m_iMinDistance = iDistance;
			}
			return true;
		}

		CHARACTER * GetVictim()
		{
			// if there's a building nearby and HP is high, attack the building. attack it even if there's only a building
			if (m_pkChrBuilding && m_pkChr->GetHP() * 2 > m_pkChr->GetMaxHP() || !m_pkChrVictim)
			{
				return m_pkChrBuilding;
			}

			return (m_pkChrVictim);
		}

	private:
		CHARACTER *	m_pkChr;

		int		m_iMinDistance;
		int		m_iMaxDistance;
		int32_t		m_lx;
		int32_t		m_ly;

		CHARACTER *	m_pkChrVictim;
		CHARACTER *	m_pkChrBuilding;
};

CHARACTER * FindVictim(CHARACTER * pkChr, int iMaxDistance)
{
	FuncFindMobVictim f(pkChr, iMaxDistance);
	if (pkChr->GetSectree() != nullptr) {
		pkChr->GetSectree()->ForEachAround(f);	
	}
	return f.GetVictim();
}

