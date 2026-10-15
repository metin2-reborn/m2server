
#include <metin2/config/config.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/affect_flag.h>
#include <metin2/game/events/arena.h>
#include <metin2/game/combat/battle.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/items/dragon_soul.h>
#include <metin2/game/app/event.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/characters/horsename_manager.h>
#include <metin2/game/items/item.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace metin2::game::inline characters {

static auto is_no_save_affect(uint32_t type) -> bool { return (type == AFFECT_WAR_FLAG || type == AFFECT_REVIVE_INVISIBLE || (type >= AFFECT_PREMIUM_START && type <= AFFECT_PREMIUM_END)); }
static auto is_no_clear_on_death_affect(uint32_t type) -> bool { return (type == AFFECT_BLOCK_CHAT || (type >= 500 && type < 600)); }

void send_affect_remove_packet(desc * d, uint32_t pid, uint32_t type, uint8_t point)
{
	packet_gc_affect_remove ptoc;
	ptoc.header	= HEADER_GC_AFFECT_REMOVE;
	ptoc.type		= type;
	ptoc.apply_on	= point;
	d->packet(&ptoc, sizeof(packet_gc_affect_remove));

	packet_gd_remove_affect ptod;
	ptod.pid		= pid;
	ptod.type		= type;
	ptod.apply_on	= point;
	db_clientdesc->db_packet(HEADER_GD_REMOVE_AFFECT, 0, &ptod, sizeof(ptod));
}

void send_affect_add_packet(desc * d, affect * aff)
{
	packet_gc_affect_add ptoc;
	ptoc.header		= HEADER_GC_AFFECT_ADD;
	ptoc.elem.type		= aff->type;
	ptoc.elem.apply_on		= aff->apply_on;
	ptoc.elem.apply_value	= aff->apply_value;
	ptoc.elem.flag		= aff->flag;
	ptoc.elem.duration		= aff->duration;
	ptoc.elem.sp_cost		= aff->sp_cost;
	d->packet(&ptoc, sizeof(packet_gc_affect_add));
}
auto character::find_affect(uint32_t type_value, uint8_t apply) const -> affect *
{
	auto it = list_pk_affect_.begin();

	while (it != list_pk_affect_.end())
	{
		affect * pkAffect = *it++;

		if (pkAffect->type == type_value && (apply == APPLY_NONE || apply == pkAffect->apply_on))
			return pkAffect;
	}

	return nullptr;
}

EVENTFUNC(affect_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "affect_event> <Factor> Null pointer" );
		return 0;
	}

	character * ch = info->ch;

	if (ch == nullptr) {
		return 0;
	}

	if (!ch->update_affect())
		return 0;
	else
		return metin2::config::server().passes_per_sec;
}

auto character::update_affect() -> bool
{
	if (point(POINT_HP_RECOVERY) > 0)
	{
		if (max_hp() <= hp())
		{
			point_change(POINT_HP_RECOVERY, -point(POINT_HP_RECOVERY));
		}
		else
		{
			int iVal = 0;

			iVal = std::min(point(POINT_HP_RECOVERY), max_hp() * 7 / 100);

			point_change(POINT_HP, iVal);
			point_change(POINT_HP_RECOVERY, -iVal);
		}
	}

	if (point(POINT_SP_RECOVERY) > 0)
	{
		if (max_sp() <= sp())
			point_change(POINT_SP_RECOVERY, -point(POINT_SP_RECOVERY));
		else 
		{
			int iVal;

			iVal = std::min(point(POINT_SP_RECOVERY), max_sp() * 7 / 100);

			point_change(POINT_SP, iVal);
			point_change(POINT_SP_RECOVERY, -iVal);
		}
	}

	if (point(POINT_HP_RECOVER_CONTINUE) > 0)
	{
		point_change(POINT_HP, point(POINT_HP_RECOVER_CONTINUE));
	}

	if (point(POINT_SP_RECOVER_CONTINUE) > 0)
	{
		point_change(POINT_SP, point(POINT_SP_RECOVER_CONTINUE));
	}

	auto_recovery_item_process( AFFECT_AUTO_HP_RECOVERY );
	auto_recovery_item_process( AFFECT_AUTO_SP_RECOVERY );

	if (max_stamina() > stamina())
	{
		int iSec = (get_dword_time() - stop_time()) / 3000;
		if (iSec)
			point_change(POINT_STAMINA, max_stamina()/1);    
	}


	if (process_affect())
		if (point(POINT_HP_RECOVERY) == 0 && point(POINT_SP_RECOVERY) == 0 && stamina() == max_stamina())
		{
			affect_event = nullptr;
			return false;
		}

	return true;
}

void character::start_affect_event()
{
	if (affect_event)
		return;

	char_event_info* info = alloc_event_info<char_event_info>();
	info->ch = this;
	affect_event = event_create(game::characters::affect_event, info, metin2::config::server().passes_per_sec);
	metin2::log::debug( "StartAffectEvent {} {} {}", name(), static_cast<const void *>(this), static_cast<const void *>(affect_event.get()));
}

void character::clear_affect(bool save)
{
	affect_flag afOld = af_affect_flag_;
	uint16_t	wMovSpd = point(POINT_MOV_SPEED);
	uint16_t	wAttSpd = point(POINT_ATT_SPEED);

	auto it = list_pk_affect_.begin();

	while (it != list_pk_affect_.end())
	{
		affect * pkAff = *it;

		if (save)
		{
			if ( is_no_clear_on_death_affect(pkAff->type) || is_no_save_affect(pkAff->type) )
			{
				++it;
				continue;
			}

			if (is_pc())
			{
				send_affect_remove_packet(get_desc(), player_id(), pkAff->type, pkAff->apply_on);
			}
		}

		compute_affect(pkAff, false);

		it = list_pk_affect_.erase(it);
		delete pkAff;
	}

	if (afOld != af_affect_flag_ ||
			wMovSpd != point(POINT_MOV_SPEED) ||
			wAttSpd != point(POINT_ATT_SPEED))
		update_packet();

	check_maximum_points();

	if (list_pk_affect_.empty())
		event_cancel(&affect_event);
}

auto character::process_affect() -> int
{
	bool	bDiff	= false;
	affect	*pkAff	= nullptr;

	for (int i = 0; i <= PREMIUM_MAX_NUM; ++i)
	{
		int aff_idx = i + AFFECT_PREMIUM_START;

		pkAff = find_affect(aff_idx);

		if (!pkAff)
			continue;

		int remain = premium_remain_seconds(i);

		if (remain < 0)
		{
			remove_affect(aff_idx);
			bDiff = true;
		}
		else
			pkAff->duration = remain + 1;
	}

	pkAff = find_affect(AFFECT_HAIR);
	if (pkAff)
	{
		if ( this->quest_flag("hair.limit_time") < get_global_time())
		{
			this->set_part(PART_HAIR, 0);
			remove_affect(AFFECT_HAIR);
		}
		else
		{
			++(pkAff->duration);
		}
	}

	horse_name_manager::instance().validate(this);

	affect_flag afOld = af_affect_flag_;
	int32_t lMovSpd = point(POINT_MOV_SPEED);
	int32_t lAttSpd = point(POINT_ATT_SPEED);

	decltype(list_pk_affect_.begin()) it;

	it = list_pk_affect_.begin();

	while (it != list_pk_affect_.end())
	{
		pkAff = *it;

		bool bEnd = false;

		if (pkAff->type >= GUILD_SKILL_START && pkAff->type <= GUILD_SKILL_END)
		{
			if (!get_guild() || !get_guild()->under_any_war())
				bEnd = true;
		}

		if (pkAff->sp_cost > 0)
		{
			if (sp() < pkAff->sp_cost)
				bEnd = true;
			else
				point_change(POINT_SP, -pkAff->sp_cost);
		}

		if ( --pkAff->duration <= 0 )
		{
			bEnd = true;
		}

		if (bEnd)
		{
			it = list_pk_affect_.erase(it);
			compute_affect(pkAff, false);
			bDiff = true;
			if (is_pc())
			{
				send_affect_remove_packet(get_desc(), player_id(), pkAff->type, pkAff->apply_on);
			}

			delete pkAff;

			continue;
		}

		++it;
	}

	if (bDiff)
	{
		if (afOld != af_affect_flag_ ||
				lMovSpd != point(POINT_MOV_SPEED) ||
				lAttSpd != point(POINT_ATT_SPEED))
		{
			update_packet();
		}

		check_maximum_points();
	}

	if (list_pk_affect_.empty())
		return true;

	return false;
}

void character::save_affect()
{
	packet_gd_add_affect p;

	auto it = list_pk_affect_.begin();

	while (it != list_pk_affect_.end())
	{
		affect * pkAff = *it++;

		if (is_no_save_affect(pkAff->type))
			continue;

		metin2::log::debug( "AFFECT_SAVE: {:d} {:d} {:d} {:d}", pkAff->type, pkAff->apply_on, pkAff->apply_value, pkAff->duration);

		p.pid			= player_id();
		p.elem.type		= pkAff->type;
		p.elem.apply_on		= pkAff->apply_on;
		p.elem.apply_value	= pkAff->apply_value;
		p.elem.flag		= pkAff->flag;
		p.elem.duration	= pkAff->duration;
		p.elem.sp_cost		= pkAff->sp_cost;
		db_clientdesc->db_packet(HEADER_GD_ADD_AFFECT, 0, &p, sizeof(p));
	}
}

EVENTINFO(load_affect_login_event_info)
{
	uint32_t pid;
	uint32_t count;
	char* data;

	load_affect_login_event_info()
	: pid( 0 )
	, count( 0 )
	, data( nullptr )
	{
	}
};

EVENTFUNC(load_affect_login_event)
{
	load_affect_login_event_info* info = dynamic_cast<load_affect_login_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "load_affect_login_event_info> <Factor> Null pointer" );
		return 0;
	}

	uint32_t dwPID = info->pid;
	character * ch = character_manager::instance().find_by_pid(dwPID);

	if (!ch)
	{
		delete[] info->data;
		return 0;
	}

	desc * d = ch->get_desc();

	if (!d)
	{
		delete[] info->data;
		return 0;
	}

	if (d->is_phase(PHASE_HANDSHAKE) ||
			d->is_phase(PHASE_LOGIN) ||
			d->is_phase(PHASE_SELECT) ||
			d->is_phase(PHASE_DEAD) ||
			d->is_phase(PHASE_LOADING))
	{
		return passes_per_sec(1);
	}
	else if (d->is_phase(PHASE_CLOSE))
	{
		delete[] info->data;
		return 0;
	}
	else if (d->is_phase(PHASE_GAME))
	{
		metin2::log::debug( "Affect Load by Event");
		ch->load_affect(info->count, (packet_affect_element*)info->data);
		delete[] info->data;
		return 0;
	}
	else
	{
		metin2::log::error("input_db.cpp:quest_login_event INVALID PHASE pid {:d}", ch->player_id());
		delete[] info->data;
		return 0;
	}
}

void character::load_affect(uint32_t count_value, packet_affect_element * elements)
{

	if (!get_desc()->is_phase(PHASE_GAME))
	{
		if (metin2::config::test_server())
			metin2::log::info( "LOAD_AFFECT: Creating Event {} {:d}", name(), count_value);

		load_affect_login_event_info* info = alloc_event_info<load_affect_login_event_info>();

		info->pid = player_id();
		info->count = count_value;
		info->data = new char[sizeof(packet_affect_element) * count_value];
		std::memcpy(info->data, elements, sizeof(packet_affect_element) * count_value);

		event_create(load_affect_login_event, info, passes_per_sec(1));

		return;
	}

	clear_affect(true);

	if (metin2::config::test_server())
		metin2::log::info( "LOAD_AFFECT: {} count {:d}", name(), count_value);

	affect_flag afOld = af_affect_flag_;

	int32_t lMovSpd = point(POINT_MOV_SPEED);
	int32_t lAttSpd = point(POINT_ATT_SPEED);

	for (uint32_t i = 0; i < count_value; ++i, ++elements)
	{
		if (elements->type == SKILL_MUYEONG)
			continue;

		if (AFFECT_AUTO_HP_RECOVERY == elements->type || AFFECT_AUTO_SP_RECOVERY == elements->type)
		{
			item * item = find_item_by_id( elements->flag );

			if (nullptr == item)
				continue;

			item->lock(true);
		}

		if (elements->apply_on >= POINT_MAX_NUM)
		{
			metin2::log::error("invalid affect data {} ApplyOn {:d} ApplyValue {:d}",
					name(), elements->apply_on, elements->apply_value);
			continue;
		}

		if (metin2::config::test_server())
		{
			metin2::log::info( "Load Affect : Affect {} {:d} {:d}", name(), elements->type, elements->apply_on );
		}

		affect* pkAff = new affect{};
		list_pk_affect_.push_back(pkAff);

		pkAff->type		= elements->type;
		pkAff->apply_on		= elements->apply_on;
		pkAff->apply_value	= elements->apply_value;
		pkAff->flag		= elements->flag;
		pkAff->duration	= elements->duration;
		pkAff->sp_cost		= elements->sp_cost;

		send_affect_add_packet(get_desc(), pkAff);

		compute_affect(pkAff, true);
	
	
	}

	if ( arena_manager::instance().is_arena_map(map_index()) == true )
	{
		remove_good_affect();
	}
	
	if (afOld != af_affect_flag_ || lMovSpd != point(POINT_MOV_SPEED) || lAttSpd != point(POINT_ATT_SPEED))
	{
		update_packet();
	}

	start_affect_event();


	dragon_soul_initialize();
}

auto character::add_affect(uint32_t type_value, uint8_t apply_on_value, int32_t apply_value_value, uint32_t flag_value, int32_t duration_value, int32_t sp_cost_value, bool override, bool is_cube ) -> bool
{
	if (type_value == AFFECT_BLOCK_CHAT && duration_value > 1)
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("운영자 제제로 채팅이 금지 되었습니다."));
	}

	if (duration_value == 0)
	{
		metin2::log::error("Character::AddAffect lDuration == 0 type {:d}", type_value);
		duration_value = 1;
	}

	affect * pkAff = nullptr;

	if (is_cube)
		pkAff = find_affect(type_value,apply_on_value);
	else
		pkAff = find_affect(type_value);

	if (flag_value == AFF_STUN)
	{
		if (pos_dest_.x != x() || pos_dest_.y != y())
		{
			pos_dest_.x = pos_start_.x = x();
			pos_dest_.y = pos_start_.y = y();
			battle_end(this);

			sync_packet();
		}
	}

	if (pkAff && override)
	{
		compute_affect(pkAff, false);

		if (get_desc())
			send_affect_remove_packet(get_desc(), player_id(), pkAff->type, pkAff->apply_on);
	}
	else
	{
		pkAff = new affect{};
		list_pk_affect_.push_back(pkAff);

	}

	metin2::log::debug( "AddAffect {} type {:d} apply {:d} {:d} flag {:d} duration {:d}", name(), type_value, apply_on_value, apply_value_value, flag_value, duration_value);
	metin2::log::info( "AddAffect {} type {:d} apply {:d} {:d} flag {:d} duration {:d}", name(), type_value, apply_on_value, apply_value_value, flag_value, duration_value);

	pkAff->type	= type_value;
	pkAff->apply_on	= apply_on_value;
	pkAff->apply_value	= apply_value_value;
	pkAff->flag	= flag_value;
	pkAff->duration	= duration_value;
	pkAff->sp_cost	= sp_cost_value;

	uint16_t wMovSpd = point(POINT_MOV_SPEED);
	uint16_t wAttSpd = point(POINT_ATT_SPEED);

	compute_affect(pkAff, true);

	if (pkAff->flag || wMovSpd != point(POINT_MOV_SPEED) || wAttSpd != point(POINT_ATT_SPEED))
		update_packet();

	start_affect_event();

	if (is_pc())
	{
		send_affect_add_packet(get_desc(), pkAff);

		if (is_no_save_affect(pkAff->type))
			return true;

		packet_gd_add_affect p;
		p.pid			= player_id();
		p.elem.type		= pkAff->type;
		p.elem.apply_on		= pkAff->apply_on;
		p.elem.apply_value	= pkAff->apply_value;
		p.elem.flag		= pkAff->flag;
		p.elem.duration	= pkAff->duration;
		p.elem.sp_cost		= pkAff->sp_cost;
		db_clientdesc->db_packet(HEADER_GD_ADD_AFFECT, 0, &p, sizeof(p));
	}

	return true;
}

void character::refresh_affect()
{
	auto it = list_pk_affect_.begin();

	while (it != list_pk_affect_.end())
	{
		affect * pkAff = *it++;
		compute_affect(pkAff, true);
	}
}

void character::compute_affect(affect * aff, bool add)
{
	if (add && aff->type >= GUILD_SKILL_START && aff->type <= GUILD_SKILL_END)	
	{
		if (!get_guild())
			return;

		if (!get_guild()->under_any_war())
			return;
	}

	if (aff->flag)
	{
		if (!add)
			af_affect_flag_.reset(aff->flag);
		else
			af_affect_flag_.set(aff->flag);
	}

	if (add)
		point_change(aff->apply_on, aff->apply_value);
	else
		point_change(aff->apply_on, -aff->apply_value);

	if (aff->type == SKILL_MUYEONG)
	{
		if (add)
			start_muyeong_event();
		else
			stop_muyeong_event();
	}
}

auto character::remove_affect(affect * aff) -> bool
{
	if (!aff)
		return false;

	list_pk_affect_.remove(aff);

	compute_affect(aff, false);

	// Removing AFFECT_REVIVE_INVISIBLE skips compute_points, as expiry does: recomputing while
	// polymorphed after a buff would wrongly apply the buffs.
	if (AFFECT_REVIVE_INVISIBLE != aff->type)
	{
		compute_points();
	}
	else
	{
		update_packet();
	}

	check_maximum_points();

	if (metin2::config::test_server())
		metin2::log::info( "AFFECT_REMOVE: {} (flag {:d} apply: {:d})", name(), aff->flag, aff->apply_on);

	if (is_pc())
	{
		send_affect_remove_packet(get_desc(), player_id(), aff->type, aff->apply_on);
	}

	delete aff;
	return true;
}

auto character::remove_affect(uint32_t type) -> bool
{
	if (type == AFFECT_BLOCK_CHAT)
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("채팅 금지가 풀렸습니다."));
	}

	bool flag = false;

	affect * pkAff;

	while ((pkAff = find_affect(type)))
	{
		remove_affect(pkAff);
		flag = true;
	}

	return flag;
}

auto character::is_affect_flag(uint32_t aff) const -> bool
{
	return af_affect_flag_.is_set(aff);
}

void character::remove_good_affect()
{
	remove_affect(AFFECT_MOV_SPEED);
	remove_affect(AFFECT_ATT_SPEED);
	remove_affect(AFFECT_STR);
	remove_affect(AFFECT_DEX);
	remove_affect(AFFECT_INT);
	remove_affect(AFFECT_CON);
	remove_affect(AFFECT_CHINA_FIREWORK);

	remove_affect(SKILL_JEONGWI);
	remove_affect(SKILL_GEOMKYUNG);
	remove_affect(SKILL_CHUNKEON);
	remove_affect(SKILL_EUNHYUNG);
	remove_affect(SKILL_GYEONGGONG);
	remove_affect(SKILL_GWIGEOM);
	remove_affect(SKILL_TERROR);
	remove_affect(SKILL_JUMAGAP);
	remove_affect(SKILL_MANASHILED);
	remove_affect(SKILL_HOSIN);
	remove_affect(SKILL_REFLECT);
	remove_affect(SKILL_KWAESOK);
	remove_affect(SKILL_JEUNGRYEOK);
	remove_affect(SKILL_GICHEON);
}

auto character::is_good_affect(uint8_t affect_type) const -> bool
{
	switch (affect_type)
	{
		case (AFFECT_MOV_SPEED):
		case (AFFECT_ATT_SPEED):
		case (AFFECT_STR):
		case (AFFECT_DEX):
		case (AFFECT_INT):
		case (AFFECT_CON):
		case (AFFECT_CHINA_FIREWORK):

		case (SKILL_JEONGWI):
		case (SKILL_GEOMKYUNG):
		case (SKILL_CHUNKEON):
		case (SKILL_EUNHYUNG):
		case (SKILL_GYEONGGONG):
		case (SKILL_GWIGEOM):
		case (SKILL_TERROR):
		case (SKILL_JUMAGAP):
		case (SKILL_MANASHILED):
		case (SKILL_HOSIN):
		case (SKILL_REFLECT):
		case (SKILL_KWAESOK):
		case (SKILL_JEUNGRYEOK):
		case (SKILL_GICHEON):
			return true;
	}
	return false;
}

void character::remove_bad_affect()
{
	metin2::log::info( "RemoveBadAffect {}", name());
	remove_poison();
	remove_fire();

	remove_affect(AFFECT_STUN);

	remove_affect(AFFECT_SLOW);

	remove_affect(SKILL_TUSOK);








}
}
