#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/world/sectree.h>
#include <metin2/game/trade/shop_manager.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <limits>

namespace metin2::game::inline characters {

auto	on_click_shop(TRIGGERPARAM) -> int;
auto	on_click_talk(TRIGGERPARAM) -> int;

auto	on_attack_default(TRIGGERPARAM) -> int;

struct trigger_function {
	int (*func) (TRIGGERPARAM);
};

trigger_function on_click_triggers[ON_CLICK_MAX_NUM] =
{
	{ nullptr,          	},
	{ on_click_shop,	},
};

void character::assign_triggers(const mob_table * table)
{
	if (table->on_click_type >= ON_CLICK_MAX_NUM)
	{
		metin2::log::error("{} has invalid on-click type {:d}", name(), table->on_click_type);
		abort();
	}

	trigger_on_click_.type = table->on_click_type;
	trigger_on_click_.func = on_click_triggers[table->on_click_type].func;
}

auto on_click_shop(TRIGGERPARAM) -> int
{
	shop_manager::instance().start_shopping(causer, ch);
	return 1;
}


class func_find_mob_victim
{
	public:
		func_find_mob_victim(character * chr, int max_distance) :
			chr_(chr),
			min_distance_(std::numeric_limits<int>::max()),
			max_distance_(max_distance),
			lx_(chr->x()),
			ly_(chr->y()),
			chr_victim_(nullptr),
			chr_building_(nullptr)
	{
	};

		auto operator () (entity * ent) -> bool
		{
			if (!ent->is_type(ENTITY_CHARACTER))
				return false;

			character * pkChr = (character *) ent;

			if (pkChr->is_building() && 
				(pkChr->is_affect_flag(AFF_BUILDING_CONSTRUCTION_SMALL) ||
				 pkChr->is_affect_flag(AFF_BUILDING_CONSTRUCTION_LARGE) ||
				 pkChr->is_affect_flag(AFF_BUILDING_UPGRADE)))
			{
				chr_building_ = pkChr;
			}

			if (pkChr->is_npc())
			{
				if ( !pkChr->is_monster() || !chr_->is_attack_mob() || chr_->is_aggressive()  )
					return false;
					
			}

			if (pkChr->is_dead())
				return false;

			if (pkChr->is_affect_flag(AFF_EUNHYUNG) || 
					pkChr->is_affect_flag(AFF_INVISIBILITY) ||
					pkChr->is_affect_flag(AFF_REVIVE_INVISIBLE))
				return false;

			if (pkChr->is_affect_flag(AFF_TERROR) && chr_->is_immune(IMMUNE_TERROR) == false )
			{
				if ( pkChr->level() >= chr_->level() )
					return false;
			}

		 	if ( chr_->is_no_attack_shinsu() )
			{
				if ( pkChr->empire() == 1 )
					return false;
			}

			if ( chr_->is_no_attack_chunjo() )
			{
				if ( pkChr->empire() == 2 )
					return false;
			}
			

			if ( chr_->is_no_attack_jinno() )
			{
				if ( pkChr->empire() == 3 )
					return false;
			}

			int iDistance = distance_approx(lx_ - pkChr->x(), ly_ - pkChr->y());

			if (iDistance < min_distance_ && iDistance <= max_distance_)
			{
				chr_victim_ = pkChr;
				min_distance_ = iDistance;
			}
			return true;
		}

		auto victim() -> character *
		{
			if ((chr_building_ && chr_->hp() * 2 > chr_->max_hp()) || !chr_victim_)
			{
				return chr_building_;
			}

			return (chr_victim_);
		}

	private:
		character *	chr_;

		int		min_distance_;
		int		max_distance_;
		int32_t		lx_;
		int32_t		ly_;

		character *	chr_victim_;
		character *	chr_building_;
};

auto find_victim(character * chr, int max_distance) -> character *
{
	func_find_mob_victim f(chr, max_distance);
	if (chr->get_sectree() != nullptr) {
		chr->get_sectree()->for_each_around(f);	
	}
	return f.victim();
}
}
