#include <metin2/config/config.h>
#include <metin2/game/combat/battle.h>

#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/ani.h>
#include <metin2/game/events/arena.h>
#include <metin2/game/empire/castle.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/event.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/items/item.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/combat/pvp.h>
#include <metin2/game/world/sectree.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace metin2::game::inline combat {

auto battle_hit(character * attacker, character * victim, int & ret_dam) -> int;

auto timed_event_cancel(character * ch) -> bool
{
	if (ch->timed_event)
	{
		event_cancel(&ch->timed_event);
		return true;
	}

	return false;
}

auto battle_is_attackable(character * ch, character * victim) -> bool
{
	if (victim->is_dead())
		return false;

	{
		sectree	*sectree = nullptr;

		sectree	= ch->get_sectree();
		if (sectree && sectree->is_attr(ch->x(), ch->y(), ATTR_BANPK))
			return false;

		sectree = victim->get_sectree();
		if (sectree && sectree->is_attr(victim->x(), victim->y(), ATTR_BANPK))
			return false;
	}

	if (ch->is_stun() || ch->is_dead())
		return false;

	if (ch->is_pc() && victim->is_pc())
	{
		guild* g1 = ch->get_guild();
		guild* g2 = victim->get_guild();

		if (g1 && g2)
		{
			if (g1->under_war(g2->id()))
				return true;
		}
	}

	if (is_castle_map(ch->map_index()) && false==castle_can_attack(ch, victim))
			return false;

	if (arena_manager::instance().can_attack(ch, victim) == true)
		return true;

	return pvp_manager::instance().can_attack(ch, victim);
}

auto battle_melee_attack(character * ch, character * victim) -> int
{
	if (metin2::config::test_server()&&ch->is_pc())
		metin2::log::debug("Melee attack: {} -> {}", ch->name(), victim->name());

	if (!victim || ch == victim)
		return BATTLE_NONE;

	if (!battle_is_attackable(ch, victim))
		return BATTLE_NONE;

	int distance = distance_approx(ch->x() - victim->x(), ch->y() - victim->y());

	if (!victim->is_building())
	{
		int max = 300;

		if (false == ch->is_pc())
		{
			max = (int) (ch->mob_attack_range() * 1.15f);
		}
		else
		{
			if (false == victim->is_pc() && BATTLE_TYPE_MELEE == victim->mob_battle_type())
				max = std::max(300, (int) (victim->mob_attack_range() * 1.15f));
		}

		if (distance > max)
		{
			if (metin2::config::test_server())
				metin2::log::debug("Melee attack: target of {} too far (distance {:d}, max {:d})", ch->name(), distance, max);

			return BATTLE_NONE;
		}
	}

	if (timed_event_cancel(ch))
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("전투가 시작 되어 취소 되었습니다."));

	if (timed_event_cancel(victim))
		victim->chat_packet(CHAT_TYPE_INFO, lc_text("전투가 시작 되어 취소 되었습니다."));

	ch->set_position(POS_FIGHTING);
	ch->set_victim(victim);

	const pixel_position & vpos = victim->xyz();
	ch->set_rotation_to_xy(vpos.x, vpos.y);

	int dam;
	int ret = battle_hit(ch, victim, dam);
	return (ret);
}

void battle_end_ex(character * ch)
{
	if (ch->is_position(POS_FIGHTING))
		ch->set_position(POS_STANDING);
}

void battle_end(character * ch)
{
	battle_end_ex(ch);
}

auto calc_battle_damage(int dam, int attacker_lev, int victim_lev) -> int
{
	if (dam < 3)
		dam = number(1, 5); 

	return dam;
}

auto calc_magic_damage_with_value(int dam, character * attacker, character * victim) -> int
{
	return calc_battle_damage(dam, attacker->level(), victim->level());
}

auto calc_magic_damage(character * attacker, character * victim) -> int
{
	int iDam = 0;

	if (attacker->is_npc())
	{
		iDam = calc_melee_damage(attacker, victim, false, false);	
	}

	iDam += attacker->point(POINT_PARTY_ATTACKER_BONUS);

	return calc_magic_damage_with_value(iDam, attacker, victim);
}

auto calc_attack_rating(character * attacker, character * victim, bool ignore_target_rating) -> float
{
	int iARSrc;
	int iERSrc;

	int attacker_dx = attacker->polymorph_point(POINT_DX);
	int attacker_lv = attacker->level();

	int victim_dx = victim->polymorph_point(POINT_DX);
	int victim_lv = attacker->level();

	iARSrc = std::min(90, (attacker_dx * 4	+ attacker_lv * 2) / 6);
	iERSrc = std::min(90, (victim_dx	  * 4	+ victim_lv   * 2) / 6);

	float fAR = ((float) iARSrc + 210.0f) / 300.0f;

	if (ignore_target_rating)
		return fAR;

	float fER = ((float) (iERSrc * 2 + 5) / (iERSrc + 95)) * 3.0f / 10.0f;

	return fAR - fER;
}

auto calc_att_bonus(character * attacker, character * victim, int atk) -> int
{
	if (!victim->is_pc())
		atk += attacker->marriage_bonus(UNIQUE_ITEM_MARRIAGE_ATTACK_BONUS);

	if (!attacker->is_pc())
	{
		int iReduceDamagePct = victim->marriage_bonus(UNIQUE_ITEM_MARRIAGE_TRANSFER_DAMAGE);
		atk = atk * (100 + iReduceDamagePct) / 100;
	}

	if (attacker->is_npc() && victim->is_pc())
	{
		atk = (atk * character_manager::instance().mob_damage_rate(attacker)) / 100;
	}

	if (victim->is_npc())
	{
		if (victim->is_race_flag(RACE_FLAG_ANIMAL))
			atk += (atk * attacker->point(POINT_ATTBONUS_ANIMAL)) / 100;
		else if (victim->is_race_flag(RACE_FLAG_UNDEAD))
			atk += (atk * attacker->point(POINT_ATTBONUS_UNDEAD)) / 100;
		else if (victim->is_race_flag(RACE_FLAG_DEVIL))
			atk += (atk * attacker->point(POINT_ATTBONUS_DEVIL)) / 100;
		else if (victim->is_race_flag(RACE_FLAG_HUMAN))
			atk += (atk * attacker->point(POINT_ATTBONUS_HUMAN)) / 100;
		else if (victim->is_race_flag(RACE_FLAG_ORC))
			atk += (atk * attacker->point(POINT_ATTBONUS_ORC)) / 100;
		else if (victim->is_race_flag(RACE_FLAG_MILGYO))
			atk += (atk * attacker->point(POINT_ATTBONUS_MILGYO)) / 100;
		else if (victim->is_race_flag(RACE_FLAG_INSECT))
			atk += (atk * attacker->point(POINT_ATTBONUS_INSECT)) / 100;
		else if (victim->is_race_flag(RACE_FLAG_FIRE))
			atk += (atk * attacker->point(POINT_ATTBONUS_FIRE)) / 100;
		else if (victim->is_race_flag(RACE_FLAG_ICE))
			atk += (atk * attacker->point(POINT_ATTBONUS_ICE)) / 100;
		else if (victim->is_race_flag(RACE_FLAG_DESERT))
			atk += (atk * attacker->point(POINT_ATTBONUS_DESERT)) / 100;
		else if (victim->is_race_flag(RACE_FLAG_TREE))
			atk += (atk * attacker->point(POINT_ATTBONUS_TREE)) / 100;

		atk += (atk * attacker->point(POINT_ATTBONUS_MONSTER)) / 100;
	}
	else if (victim->is_pc())
	{
		atk += (atk * attacker->point(POINT_ATTBONUS_HUMAN)) / 100;

		switch (victim->job())
		{
			case JOB_WARRIOR:
				atk += (atk * attacker->point(POINT_ATTBONUS_WARRIOR)) / 100;
				break;

			case JOB_ASSASSIN:
				atk += (atk * attacker->point(POINT_ATTBONUS_ASSASSIN)) / 100;
				break;

			case JOB_SURA:
				atk += (atk * attacker->point(POINT_ATTBONUS_SURA)) / 100;
				break;

			case JOB_SHAMAN:
				atk += (atk * attacker->point(POINT_ATTBONUS_SHAMAN)) / 100;
				break;
		}
	}

	if (attacker->is_pc() == true)
	{
		switch (attacker->job())
		{
			case JOB_WARRIOR:
				atk -= (atk * victim->point(POINT_RESIST_WARRIOR)) / 100;
				break;

			case JOB_ASSASSIN:
				atk -= (atk * victim->point(POINT_RESIST_ASSASSIN)) / 100;
				break;

			case JOB_SURA:
				atk -= (atk * victim->point(POINT_RESIST_SURA)) / 100;
				break;

			case JOB_SHAMAN:
				atk -= (atk * victim->point(POINT_RESIST_SHAMAN)) / 100;
				break;
		}
	}

	if (attacker->is_npc() && victim->is_pc())
	{
		if (attacker->is_race_flag(RACE_FLAG_ATT_ELEC))
			atk -= (atk * 30 * victim->point(POINT_RESIST_ELEC))		/ 10000;
		else if (attacker->is_race_flag(RACE_FLAG_ATT_FIRE))
			atk -= (atk * 30 * victim->point(POINT_RESIST_FIRE))		/ 10000;
		else if (attacker->is_race_flag(RACE_FLAG_ATT_ICE))
			atk -= (atk * 30 * victim->point(POINT_RESIST_ICE))		/ 10000;
		else if (attacker->is_race_flag(RACE_FLAG_ATT_WIND))
			atk -= (atk * 30 * victim->point(POINT_RESIST_WIND))		/ 10000;
		else if (attacker->is_race_flag(RACE_FLAG_ATT_EARTH))
			atk -= (atk * 30 * victim->point(POINT_RESIST_EARTH))	/ 10000;
		else if (attacker->is_race_flag(RACE_FLAG_ATT_DARK))
			atk -= (atk * 30 * victim->point(POINT_RESIST_DARK))		/ 10000;
	}

	
	return atk;
}

void item_get_damage(item * item_value, int* pdam_min, int* pdam_max)
{
	*pdam_min = 0;
	*pdam_max = 1;

	if (!item_value)
		return;

	switch (item_value->type())
	{
		case ITEM_ROD:
		case ITEM_PICK:
			return;
	}

	if (item_value->type() != ITEM_WEAPON)
		metin2::log::error("Item {:d} is not a weapon (type {:d})", item_value->original_vnum(), item_value->type());

	*pdam_min = item_value->value(3);
	*pdam_max = item_value->value(4);
}

auto calc_melee_damage(character * attacker, character * victim, bool ignore_defense, bool ignore_target_rating) -> int
{
	item * pWeapon = attacker->wear(WEAR_WEAPON);
	bool bPolymorphed = attacker->is_polymorphed();

	if (pWeapon && !(bPolymorphed && !attacker->is_poly_maintain_stat()))
	{
		if (pWeapon->type() != ITEM_WEAPON)
			return 0;

		switch (pWeapon->sub_type())
		{
			case WEAPON_SWORD:
			case WEAPON_DAGGER:
			case WEAPON_TWO_HANDED:
			case WEAPON_BELL:
			case WEAPON_FAN:
			case WEAPON_MOUNT_SPEAR:
				break;

			case WEAPON_BOW:
				metin2::log::error("Melee damage computed for a bow attack by {}", attacker->name());
				return 0;

			default:
				return 0;
		}
	}

	int iDam = 0;
	float fAR = calc_attack_rating(attacker, victim, ignore_target_rating);
	int iDamMin = 0, iDamMax = 0;

	int DEBUG_iDamCur = 0;
	int DEBUG_iDamBonus = 0;

	if (bPolymorphed && !attacker->is_poly_maintain_stat())
	{
		item_get_damage(pWeapon, &iDamMin, &iDamMax);

		uint32_t dwMobVnum = attacker->polymorph_vnum();
		const mob * pMob = mob_manager::instance().get(dwMobVnum);

		if (pMob)
		{
			int iPower = attacker->polymorph_power();
			iDamMin += pMob->table.damage_range[0] * iPower / 100;
			iDamMax += pMob->table.damage_range[1] * iPower / 100;
		}
	}
	else if (pWeapon)
	{
		item_get_damage(pWeapon, &iDamMin, &iDamMax);
	}
	else if (attacker->is_npc())
	{
		iDamMin = attacker->mob_damage_min();
		iDamMax = attacker->mob_damage_max();
	}

	iDam = number(iDamMin, iDamMax) * 2;

	DEBUG_iDamCur = iDam;
	int iAtk = 0;

	iAtk = attacker->point(POINT_ATT_GRADE) + iDam - (attacker->level() * 2);
	iAtk = (int) (iAtk * fAR);
	iAtk += attacker->level() * 2;

	if (pWeapon)
	{
		iAtk += pWeapon->value(5) * 2;

		DEBUG_iDamBonus = pWeapon->value(5) * 2;
	}

	iAtk += attacker->point(POINT_PARTY_ATTACKER_BONUS);
	iAtk = (int) (iAtk * (100 + (attacker->point(POINT_ATT_BONUS) + attacker->point(POINT_MELEE_MAGIC_ATT_BONUS_PER))) / 100);

	iAtk = calc_att_bonus(attacker, victim, iAtk);

	int iDef = 0;

	if (!ignore_defense)
	{
		iDef = (victim->point(POINT_DEF_GRADE) * (100 + victim->point(POINT_DEF_BONUS)) / 100);

		if (!attacker->is_pc())
			iDef += victim->marriage_bonus(UNIQUE_ITEM_MARRIAGE_DEFENSE_BONUS);
	}

	if (attacker->is_npc())
		iAtk = (int) (iAtk * attacker->mob_damage_multiply());

	iDam = std::max(0, iAtk - iDef);

	if (metin2::config::test_server())
	{
		int DEBUG_iLV = attacker->level()*2;
		int DEBUG_iST = int((attacker->point(POINT_ATT_GRADE) - DEBUG_iLV) * fAR);
		int DEBUG_iPT = attacker->point(POINT_PARTY_ATTACKER_BONUS);
		int DEBUG_iWP = 0;
		int DEBUG_iPureAtk = 0;
		int DEBUG_iPureDam = 0;
		char szRB[32] = "";
		char szGradeAtkBonus[32] = "";

		DEBUG_iWP = int(DEBUG_iDamCur * fAR);
		DEBUG_iPureAtk = DEBUG_iLV + DEBUG_iST + DEBUG_iWP+DEBUG_iDamBonus;
		DEBUG_iPureDam = iAtk - iDef;

		if (attacker->is_npc())
		{
			snprintf(szGradeAtkBonus, sizeof(szGradeAtkBonus), "=%d*%.1f", DEBUG_iPureAtk, attacker->mob_damage_multiply());
			DEBUG_iPureAtk = int(DEBUG_iPureAtk * attacker->mob_damage_multiply());
		}

		if (DEBUG_iDamBonus != 0)
			snprintf(szRB, sizeof(szRB), "+RB(%d)", DEBUG_iDamBonus);

		char szPT[32] = "";

		if (DEBUG_iPT != 0)
			snprintf(szPT, sizeof(szPT), ", PT=%d", DEBUG_iPT);

		char szUnknownAtk[32] = "";

		if (iAtk != DEBUG_iPureAtk)
			snprintf(szUnknownAtk, sizeof(szUnknownAtk), "+?(%d)", iAtk-DEBUG_iPureAtk);

		char szUnknownDam[32] = "";

		if (iDam != DEBUG_iPureDam)
			snprintf(szUnknownDam, sizeof(szUnknownDam), "+?(%d)", iDam-DEBUG_iPureDam);

		char szMeleeAttack[128];

		snprintf(szMeleeAttack, sizeof(szMeleeAttack), 
				"%s(%d)-%s(%d)=%d%s, ATK=LV(%d)+ST(%d)+WP(%d)%s%s%s, AR=%.3g%s", 
				attacker->name(),
				iAtk,
				victim->name(),
				iDef,
				iDam,
				szUnknownDam,
				DEBUG_iLV, 
				DEBUG_iST,
				DEBUG_iWP, 
				szRB,
				szUnknownAtk,
				szGradeAtkBonus,
				fAR,
				szPT);

		attacker->chat_packet(CHAT_TYPE_TALKING, "%s", szMeleeAttack);
		victim->chat_packet(CHAT_TYPE_TALKING, "%s", szMeleeAttack);
	}

	return calc_battle_damage(iDam, attacker->level(), victim->level());
}

auto calc_arrow_damage(character * attacker, character * victim, item * bow, item * arrow, bool ignore_defense) -> int
{
	if (!bow || bow->type() != ITEM_WEAPON || bow->sub_type() != WEAPON_BOW)
		return 0;

	if (!arrow)
		return 0;

	int iDist = (int) (distance_sqrt(attacker->x() - victim->x(), attacker->y() - victim->y()));
	int iGap = (iDist / 100) - 5 - attacker->point(POINT_BOW_DISTANCE);
	int iPercent = 100 - (iGap * 5);

	if (iPercent <= 0)
		return 0;
	else if (iPercent > 100)
		iPercent = 100;

	int iDam = 0;

	float fAR = calc_attack_rating(attacker, victim, false);
	iDam = number(bow->value(3), bow->value(4)) * 2 + arrow->value(3);
	int iAtk;

	iAtk = attacker->point(POINT_ATT_GRADE) + iDam - (attacker->level() * 2);
	iAtk = (int) (iAtk * fAR);
	iAtk += attacker->level() * 2;

	iAtk += bow->value(5) * 2;

	iAtk += attacker->point(POINT_PARTY_ATTACKER_BONUS);
	iAtk = (int) (iAtk * (100 + (attacker->point(POINT_ATT_BONUS) + attacker->point(POINT_MELEE_MAGIC_ATT_BONUS_PER))) / 100);

	iAtk = calc_att_bonus(attacker, victim, iAtk);

	int iDef = 0;

	if (!ignore_defense)
		iDef = (victim->point(POINT_DEF_GRADE) * (100 + attacker->point(POINT_DEF_BONUS)) / 100);

	if (attacker->is_npc())
		iAtk = (int) (iAtk * attacker->mob_damage_multiply());

	iDam = std::max(0, iAtk - iDef);

	int iPureDam = iDam;

	iPureDam = (iPureDam * iPercent) / 100;

	if (metin2::config::test_server())
	{
		attacker->chat_packet(CHAT_TYPE_INFO, "ARROW %s -> %s, DAM %d DIST %d GAP %d %% %d",
				attacker->name(), 
				victim->name(), 
				iPureDam, 
				iDist, iGap, iPercent);
	}

	return iPureDam;
}

void normal_attack_affect(character * attacker, character * victim)
{
	if (attacker->point(POINT_POISON_PCT) && !victim->is_affect_flag(AFF_POISON))
	{
		if (number(1, 100) <= attacker->point(POINT_POISON_PCT))
			victim->attacked_by_poison(attacker);
	}

	int iStunDuration = 2;
	if (attacker->is_pc() && !victim->is_pc())
		iStunDuration = 4;

	attack_affect(attacker, victim, POINT_STUN_PCT, IMMUNE_STUN,  AFFECT_STUN, POINT_NONE,        0, AFF_STUN, iStunDuration, "STUN");
	attack_affect(attacker, victim, POINT_SLOW_PCT, IMMUNE_SLOW,  AFFECT_SLOW, POINT_MOV_SPEED, -30, AFF_SLOW, 20,		"SLOW");
}

auto battle_hit(character * attacker, character * victim, int & ret_dam) -> int
{
	if (metin2::config::test_server())
		metin2::log::debug("Hit: {} -> {}, damage {:d}", attacker->name(), victim->name(), ret_dam);

	int iDam = calc_melee_damage(attacker, victim);

	if (iDam <= 0)
		return (BATTLE_DAMAGE);

	normal_attack_affect(attacker, victim);

	item * pkWeapon = attacker->wear(WEAR_WEAPON);

	if (pkWeapon)
		switch (pkWeapon->sub_type())
		{
			case WEAPON_SWORD:
				iDam = iDam * (100 - victim->point(POINT_RESIST_SWORD)) / 100;
				break;

			case WEAPON_TWO_HANDED:
				iDam = iDam * (100 - victim->point(POINT_RESIST_TWOHAND)) / 100;
				break;

			case WEAPON_DAGGER:
				iDam = iDam * (100 - victim->point(POINT_RESIST_DAGGER)) / 100;
				break;

			case WEAPON_BELL:
				iDam = iDam * (100 - victim->point(POINT_RESIST_BELL)) / 100;
				break;

			case WEAPON_FAN:
				iDam = iDam * (100 - victim->point(POINT_RESIST_FAN)) / 100;
				break;

			case WEAPON_BOW:
				iDam = iDam * (100 - victim->point(POINT_RESIST_BOW)) / 100;
				break;
		}

	float attMul = attacker->att_mul();
	float tempIDam = iDam;
	iDam = attMul * tempIDam + 0.5f;

	ret_dam = iDam;

	if (victim->damage(attacker, iDam, DAMAGE_TYPE_NORMAL))
		return (BATTLE_DEAD);

	return (BATTLE_DAMAGE);
}

auto get_attack_speed(character * ch) -> uint32_t
{
	if (nullptr == ch)
		return 1000;

	item * item = ch->wear(WEAR_WEAPON);	
	uint32_t default_bonus = metin2::config::game().anti_cheat.speed_hack_bonus * 3;
	uint32_t riding_bonus = 0;

	if (ch->is_riding())
	{
		riding_bonus = 50;
	}

	uint32_t ani_speed = ani_attack_speed(ch);
    uint32_t real_speed = (ani_speed * 100) / (default_bonus + ch->point(POINT_ATT_SPEED) + riding_bonus);

	if (item && item->sub_type() == WEAPON_DAGGER)
		real_speed /= 2;

    return real_speed;

}

void set_attack_time(character * ch, character * victim, uint32_t current_time)
{
	if (nullptr == ch || nullptr == victim)
		return;

	if (!ch->is_pc())
		return;

	ch->attack_log.vid = victim->get_vid();
	ch->attack_log.time = current_time;
}

void set_attacked_time(character * ch, character * victim, uint32_t current_time)
{
	if (nullptr == ch || nullptr == victim)
		return;

	if (!ch->is_pc())
		return;

	victim->attacked_log.pid			= ch->player_id();
	victim->attacked_log.attacked_time= current_time;
}

auto is_speed_hack(character * ch, character * victim, uint32_t current_time) -> bool
{

	if (ch->attack_log.vid == victim->get_vid())
	{
		if (current_time - ch->attack_log.time < get_attack_speed(ch))
		{
			++ch->speed_hack_count;

			if (metin2::config::test_server())
			{
				metin2::log::warn("Possible attack speed hack by {}: attack interval {:d} ms below limit {:d} ms (count {:d})",
						ch->name(),
						current_time - ch->attack_log.time,
						get_attack_speed(ch),
						ch->speed_hack_count);

				ch->chat_packet(CHAT_TYPE_INFO, "%s attack hack! time (delta, limit)=(%u, %u) hack_count %d",
						ch->name(),
						current_time - ch->attack_log.time,
						get_attack_speed(ch),
						ch->speed_hack_count);
			}

			set_attack_time(ch, victim, current_time);
			set_attacked_time(ch, victim, current_time);
			return true;
		}
	}

	set_attack_time(ch, victim, current_time);

	if (victim->attacked_log.pid == ch->player_id())
	{
		if (current_time - victim->attacked_log.attacked_time < get_attack_speed(ch))
		{
			++ch->speed_hack_count;

			if (metin2::config::test_server())
			{
				metin2::log::warn("Possible attack speed hack by {}: hit interval on victim {:d} ms below limit {:d} ms (count {:d})",
						ch->name(),
						current_time - victim->attacked_log.attacked_time,
						get_attack_speed(ch),
						ch->speed_hack_count);

				ch->chat_packet(CHAT_TYPE_INFO, "Attack Speed Hack(%s), (delta, limit)=(%u, %u)",
						ch->name(),
						current_time - victim->attacked_log.attacked_time,
						get_attack_speed(ch));
			}

			set_attacked_time(ch, victim, current_time);
			return true;
		}
	}

	set_attacked_time(ch, victim, current_time);
	return false;
}
}
