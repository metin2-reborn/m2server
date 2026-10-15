#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/char.h>
#include <metin2/game/characters/char_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace metin2::game::inline characters {

// poison
const int poison_damage_rate[MOB_RANK_MAX_NUM] = 
{
	80, 50, 40, 30, 25, 1
};

int get_poison_damage_rate(character * ch)
{
	int iRate;

	if (ch->is_pc())
	{
		iRate = 50;
	}
	else
		iRate = poison_damage_rate[ch->get_mob_rank()];

	iRate = std::max(0, iRate - ch->point(POINT_POISON_REDUCE));
	return iRate;
}

EVENTINFO(poison_event_info)
{
	dynamic_character_ptr ch;
	int		count;
	uint32_t	attacker_pid;

	poison_event_info()
	: ch()
	, count(0)
	, attacker_pid(0)
	{
	}
};

EVENTFUNC(poison_event)
{
	poison_event_info * info = dynamic_cast<poison_event_info *>( event->info );
	
	if ( info == nullptr )
	{
		metin2::log::error( "poison_event> <Factor> Null pointer" );
		return 0;
	}

	character * ch = info->ch;

	if (ch == nullptr) { // <Factor>
		return 0;
	}
	character * pkAttacker = character_manager::instance().find_by_pid(info->attacker_pid);

	int dam = ch->max_hp() * get_poison_damage_rate(ch) / 1000;
	if (metin2::config::test_server()) ch->chat_packet(CHAT_TYPE_NOTICE, "Poison Damage %d", dam);

	if (ch->damage(pkAttacker, dam, DAMAGE_TYPE_POISON))
	{
		ch->poison_event = nullptr;
		return 0;
	}

	--info->count;

	if (info->count)
		return passes_per_sec(3);
	else
	{
		ch->poison_event = nullptr;
		return 0;
	}
}

EVENTINFO(fire_event_info)
{
	dynamic_character_ptr ch;
	int		count;
	int		amount;
	uint32_t	attacker_pid;

	fire_event_info()
	: ch()
	, count(0)
	, amount(0)
	, attacker_pid(0)
	{
	}
};

EVENTFUNC(fire_event)
{
	fire_event_info * info = dynamic_cast<fire_event_info *>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "fire_event> <Factor> Null pointer" );
		return 0;
	}

	character * ch = info->ch;
	if (ch == nullptr) { // <Factor>
		return 0;
	}
	character * pkAttacker = character_manager::instance().find_by_pid(info->attacker_pid);

	int dam = info->amount;
	if (metin2::config::test_server()) ch->chat_packet(CHAT_TYPE_NOTICE, "Fire Damage %d", dam);

	if (ch->damage(pkAttacker, dam, DAMAGE_TYPE_FIRE))
	{
		ch->fire_event = nullptr;
		return 0;
	}

	--info->count;

	if (info->count)
		return passes_per_sec(3);
	else
	{
		ch->fire_event = nullptr;
		return 0;
	}
}

/*

   By LEVEL..

   +8   0%
   +7   5%
   +6  10%
   +5  30%
   +4  50%
   +3  70%
   +2  80%
   +1  90%
   +0 100%
   -1 100%
   -2 100%
   -3 100%
   -4 100%
   -5 100%
   -6 100%
   -7 100%
   -8 100%

 */

static int poison_level_adjust[9] =
{
	100, 90, 80, 70, 50, 30, 10, 5, 0
};

void character::attacked_by_fire(character * pkAttacker, int amount, int count)
{
	if (fire_event)
		return;

	add_affect(AFFECT_FIRE, POINT_NONE, 0, AFF_FIRE, count*3+1, 0, true);

	fire_event_info* info = alloc_event_info<fire_event_info>();

	info->ch = this;
	info->count = count;
	info->amount = amount;
	info->attacker_pid = pkAttacker->player_id();

	fire_event = event_create(game::characters::fire_event, info, 1);
}

void character::attacked_by_poison(character * pkAttacker)
{
	if (poison_event)
		return;

	if (has_poisoned && !is_pc()) // monsters can only be poisoned once.
		return;

	if (pkAttacker && pkAttacker->level() < level())
	{
		int delta = level() - pkAttacker->level();

		if (delta > 8)
			delta = 8;

		if (number(1, 100) > poison_level_adjust[delta])
			return;
	}

	// poison resist roll failed, poisoned!
	has_poisoned = true;

	add_affect(AFFECT_POISON, POINT_NONE, 0, AFF_POISON, POISON_LENGTH + 1, 0, true);

	poison_event_info* info = alloc_event_info<poison_event_info>();

	info->ch = this;
	info->count = 10;
	info->attacker_pid = pkAttacker?pkAttacker->player_id():0;

	poison_event = event_create(game::characters::poison_event, info, 1);

	if (metin2::config::test_server() && pkAttacker)
	{
		char buf[256];
		snprintf(buf, sizeof(buf), "POISON %s -> %s", pkAttacker->name(), name());
		pkAttacker->chat_packet(CHAT_TYPE_INFO, "%s", buf);
	}
}

void character::remove_fire()
{
	remove_affect(AFFECT_FIRE);
	event_cancel(&fire_event);
}

void character::remove_poison()
{
	remove_affect(AFFECT_POISON);
	event_cancel(&poison_event);
}

void character::apply_mob_attribute(const mob_table* table)
{
	for (int i = 0; i < MOB_ENCHANTS_MAX_NUM; ++i)
	{
		if (table->c_enchants[i] != 0)
			apply_point(mob_enchant_apply_idx[i], table->c_enchants[i]);
	}

	for (int i = 0; i < MOB_RESISTS_MAX_NUM; ++i)
	{
		if (table->c_resists[i] != 0)
			apply_point(mob_resists_apply_idx[i], table->c_resists[i]);
	}
}

bool character::is_immune(uint32_t dwImmuneFlag)
{
	if (is_set(points_instant_.immune_flag, dwImmuneFlag))
	{
		int immune_pct = 90;
		int	percent = number(1, 100);

		if (percent <= immune_pct)	// 90% Immune
		{
			if (metin2::config::test_server() && is_pc())
				chat_packet(CHAT_TYPE_PARTY, "<IMMUNE_SUCCESS> (%s)", name()); 

			return true;
		}
		else
		{
			if (metin2::config::test_server() && is_pc())
				chat_packet(CHAT_TYPE_PARTY, "<IMMUNE_FAIL> (%s)", name());

			return false;
		}
	}

	if (metin2::config::test_server() && is_pc())
		chat_packet(CHAT_TYPE_PARTY, "<IMMUNE_FAIL> (%s) NO_IMMUNE_FLAG", name());

	return false;
}
}
