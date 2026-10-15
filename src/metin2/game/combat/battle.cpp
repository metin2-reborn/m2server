#include <metin2/game/combat/battle.h>

#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/ani.h>
#include <metin2/game/events/arena.h>
#include <metin2/game/empire/castle.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
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

int battle_hit(character * ch, character * victim, int & iRetDam);

bool timed_event_cancel(character * ch)
{
	if (ch->timed_event)
	{
		event_cancel(&ch->timed_event);
		return true;
	}

	return false;
}

bool battle_is_attackable(character * ch, character * victim)
{
	// Stop if the target is dead.
	if (victim->is_dead())
		return false;

	// Stop if in a safe zone
	{
		sectree	*sectree = nullptr;

		sectree	= ch->get_sectree();
		if (sectree && sectree->is_attr(ch->x(), ch->y(), ATTR_BANPK))
			return false;

		sectree = victim->get_sectree();
		if (sectree && sectree->is_attr(victim->x(), victim->y(), ATTR_BANPK))
			return false;
	}

	// Stop if I'm dead.
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

int battle_melee_attack(character * ch, character * victim)
{
	if (metin2::config::test_server()&&ch->is_pc())
		metin2::log::info( "battle_melee_attack : [{}] attack to [{}]", ch->name(), victim->name());

	if (!victim || ch == victim)
		return BATTLE_NONE;

	if (metin2::config::test_server()&&ch->is_pc())
		metin2::log::info( "battle_melee_attack : [{}] attack to [{}]", ch->name(), victim->name());

	if (!battle_is_attackable(ch, victim))
		return BATTLE_NONE;

	if (metin2::config::test_server()&&ch->is_pc())
		metin2::log::info( "battle_melee_attack : [{}] attack to [{}]", ch->name(), victim->name());

	// Distance check
	int distance = distance_approx(ch->x() - victim->x(), ch->y() - victim->y());

	if (!victim->is_building())
	{
		int max = 300;

		if (false == ch->is_pc())
		{
			// For monsters, use the monster attack range
			max = (int) (ch->mob_attack_range() * 1.15f);
		}
		else
		{
			// For a PC, if the opponent is a melee mob, the mob's attack range is the max attack range
			if (false == victim->is_pc() && BATTLE_TYPE_MELEE == victim->mob_battle_type())
				max = std::max(300, (int) (victim->mob_attack_range() * 1.15f));
		}

		if (distance > max)
		{
			if (metin2::config::test_server())
				metin2::log::info( "VICTIM_FAR: {} distance: {:d} max: {:d}", ch->name(), distance, max);

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

// Actually set GET_BATTLE_VICTIM to NULL and cancel the event.
void battle_end_ex(character * ch)
{
	if (ch->is_position(POS_FIGHTING))
		ch->set_position(POS_STANDING);
}

void battle_end(character * ch)
{
	battle_end_ex(ch);
}

// AG = Attack Grade
// AL = Attack Limit
int calc_battle_damage(int iDam, int iAttackerLev, int iVictimLev)
{
	if (iDam < 3)
		iDam = number(1, 5); 

	return iDam;
}

int calc_magic_damage_with_value(int iDam, character * pkAttacker, character * pkVictim)
{
	return calc_battle_damage(iDam, pkAttacker->level(), pkVictim->level());
}

int calc_magic_damage(character * pkAttacker, character * pkVictim)
{
	int iDam = 0;

	if (pkAttacker->is_npc())
	{
		iDam = calc_melee_damage(pkAttacker, pkVictim, false, false);	
	}

	iDam += pkAttacker->point(POINT_PARTY_ATTACKER_BONUS);

	return calc_magic_damage_with_value(iDam, pkAttacker, pkVictim);
}

float calc_attack_rating(character * pkAttacker, character * pkVictim, bool bIgnoreTargetRating)
{
	int iARSrc;
	int iERSrc;

	int attacker_dx = pkAttacker->polymorph_point(POINT_DX);
	int attacker_lv = pkAttacker->level();

	int victim_dx = pkVictim->polymorph_point(POINT_DX);
	int victim_lv = pkAttacker->level();

	iARSrc = std::min(90, (attacker_dx * 4	+ attacker_lv * 2) / 6);
	iERSrc = std::min(90, (victim_dx	  * 4	+ victim_lv   * 2) / 6);

	float fAR = ((float) iARSrc + 210.0f) / 300.0f; // fAR = 0.7 ~ 1.0

	if (bIgnoreTargetRating)
		return fAR;

	// ((Edx * 2 + 20) / (Edx + 110)) * 0.3
	float fER = ((float) (iERSrc * 2 + 5) / (iERSrc + 95)) * 3.0f / 10.0f;

	return fAR - fER;
}

int calc_att_bonus(character * pkAttacker, character * pkVictim, int iAtk)
{
	// Not applied in PvP
	if (!pkVictim->is_pc())
		iAtk += pkAttacker->marriage_bonus(UNIQUE_ITEM_MARRIAGE_ATTACK_BONUS);

	// Not applied in PvP
	if (!pkAttacker->is_pc())
	{
		int iReduceDamagePct = pkVictim->marriage_bonus(UNIQUE_ITEM_MARRIAGE_TRANSFER_DAMAGE);
		iAtk = iAtk * (100 + iReduceDamagePct) / 100;
	}

	if (pkAttacker->is_npc() && pkVictim->is_pc())
	{
		iAtk = (iAtk * character_manager::instance().mob_damage_rate(pkAttacker)) / 100;
	}

	if (pkVictim->is_npc())
	{
		if (pkVictim->is_race_flag(RACE_FLAG_ANIMAL))
			iAtk += (iAtk * pkAttacker->point(POINT_ATTBONUS_ANIMAL)) / 100;
		else if (pkVictim->is_race_flag(RACE_FLAG_UNDEAD))
			iAtk += (iAtk * pkAttacker->point(POINT_ATTBONUS_UNDEAD)) / 100;
		else if (pkVictim->is_race_flag(RACE_FLAG_DEVIL))
			iAtk += (iAtk * pkAttacker->point(POINT_ATTBONUS_DEVIL)) / 100;
		else if (pkVictim->is_race_flag(RACE_FLAG_HUMAN))
			iAtk += (iAtk * pkAttacker->point(POINT_ATTBONUS_HUMAN)) / 100;
		else if (pkVictim->is_race_flag(RACE_FLAG_ORC))
			iAtk += (iAtk * pkAttacker->point(POINT_ATTBONUS_ORC)) / 100;
		else if (pkVictim->is_race_flag(RACE_FLAG_MILGYO))
			iAtk += (iAtk * pkAttacker->point(POINT_ATTBONUS_MILGYO)) / 100;
		else if (pkVictim->is_race_flag(RACE_FLAG_INSECT))
			iAtk += (iAtk * pkAttacker->point(POINT_ATTBONUS_INSECT)) / 100;
		else if (pkVictim->is_race_flag(RACE_FLAG_FIRE))
			iAtk += (iAtk * pkAttacker->point(POINT_ATTBONUS_FIRE)) / 100;
		else if (pkVictim->is_race_flag(RACE_FLAG_ICE))
			iAtk += (iAtk * pkAttacker->point(POINT_ATTBONUS_ICE)) / 100;
		else if (pkVictim->is_race_flag(RACE_FLAG_DESERT))
			iAtk += (iAtk * pkAttacker->point(POINT_ATTBONUS_DESERT)) / 100;
		else if (pkVictim->is_race_flag(RACE_FLAG_TREE))
			iAtk += (iAtk * pkAttacker->point(POINT_ATTBONUS_TREE)) / 100;

		iAtk += (iAtk * pkAttacker->point(POINT_ATTBONUS_MONSTER)) / 100;
	}
	else if (pkVictim->is_pc())
	{
		iAtk += (iAtk * pkAttacker->point(POINT_ATTBONUS_HUMAN)) / 100;

		switch (pkVictim->job())
		{
			case JOB_WARRIOR:
				iAtk += (iAtk * pkAttacker->point(POINT_ATTBONUS_WARRIOR)) / 100;
				break;

			case JOB_ASSASSIN:
				iAtk += (iAtk * pkAttacker->point(POINT_ATTBONUS_ASSASSIN)) / 100;
				break;

			case JOB_SURA:
				iAtk += (iAtk * pkAttacker->point(POINT_ATTBONUS_SURA)) / 100;
				break;

			case JOB_SHAMAN:
				iAtk += (iAtk * pkAttacker->point(POINT_ATTBONUS_SHAMAN)) / 100;
				break;
		}
	}

	if (pkAttacker->is_pc() == true)
	{
		switch (pkAttacker->job())
		{
			case JOB_WARRIOR:
				iAtk -= (iAtk * pkVictim->point(POINT_RESIST_WARRIOR)) / 100;
				break;

			case JOB_ASSASSIN:
				iAtk -= (iAtk * pkVictim->point(POINT_RESIST_ASSASSIN)) / 100;
				break;

			case JOB_SURA:
				iAtk -= (iAtk * pkVictim->point(POINT_RESIST_SURA)) / 100;
				break;

			case JOB_SHAMAN:
				iAtk -= (iAtk * pkVictim->point(POINT_RESIST_SHAMAN)) / 100;
				break;
		}
	}

	//[ mob -> PC ] apply elemental attribute defense
	//2013/01/17
	//Resistance applies only to the portion equal to 30% of the monster's elemental attack damage.
	if (pkAttacker->is_npc() && pkVictim->is_pc())
	{
		if (pkAttacker->is_race_flag(RACE_FLAG_ATT_ELEC))
			iAtk -= (iAtk * 30 * pkVictim->point(POINT_RESIST_ELEC))		/ 10000;
		else if (pkAttacker->is_race_flag(RACE_FLAG_ATT_FIRE))
			iAtk -= (iAtk * 30 * pkVictim->point(POINT_RESIST_FIRE))		/ 10000;
		else if (pkAttacker->is_race_flag(RACE_FLAG_ATT_ICE))
			iAtk -= (iAtk * 30 * pkVictim->point(POINT_RESIST_ICE))		/ 10000;
		else if (pkAttacker->is_race_flag(RACE_FLAG_ATT_WIND))
			iAtk -= (iAtk * 30 * pkVictim->point(POINT_RESIST_WIND))		/ 10000;
		else if (pkAttacker->is_race_flag(RACE_FLAG_ATT_EARTH))
			iAtk -= (iAtk * 30 * pkVictim->point(POINT_RESIST_EARTH))	/ 10000;
		else if (pkAttacker->is_race_flag(RACE_FLAG_ATT_DARK))
			iAtk -= (iAtk * 30 * pkVictim->point(POINT_RESIST_DARK))		/ 10000;
	}

	
	return iAtk;
}

void item_get_damage(item * pkItem, int* pdamMin, int* pdamMax)
{
	*pdamMin = 0;
	*pdamMax = 1;

	if (!pkItem)
		return;

	switch (pkItem->type())
	{
		case ITEM_ROD:
		case ITEM_PICK:
			return;
	}

	if (pkItem->type() != ITEM_WEAPON)
		metin2::log::error("Item_GetDamage - !ITEM_WEAPON vnum={:d}, type={:d}", pkItem->original_vnum(), pkItem->type());

	*pdamMin = pkItem->value(3);
	*pdamMax = pkItem->value(4);
}

int calc_melee_damage(character * pkAttacker, character * pkVictim, bool bIgnoreDefense, bool bIgnoreTargetRating)
{
	item * pWeapon = pkAttacker->wear(WEAR_WEAPON);
	bool bPolymorphed = pkAttacker->is_polymorphed();

	if (pWeapon && !(bPolymorphed && !pkAttacker->is_poly_maintain_stat()))
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
				metin2::log::error("CalcMeleeDamage should not handle bows (name: {})", pkAttacker->name());
				return 0;

			default:
				return 0;
		}
	}

	int iDam = 0;
	float fAR = calc_attack_rating(pkAttacker, pkVictim, bIgnoreTargetRating);
	int iDamMin = 0, iDamMax = 0;

	// TESTSERVER_SHOW_ATTACKINFO
	int DEBUG_iDamCur = 0;
	int DEBUG_iDamBonus = 0;
	// END_OF_TESTSERVER_SHOW_ATTACKINFO

	if (bPolymorphed && !pkAttacker->is_poly_maintain_stat())
	{
		// MONKEY_ROD_ATTACK_BUG_FIX
		item_get_damage(pWeapon, &iDamMin, &iDamMax);
		// END_OF_MONKEY_ROD_ATTACK_BUG_FIX

		uint32_t dwMobVnum = pkAttacker->polymorph_vnum();
		const mob * pMob = mob_manager::instance().get(dwMobVnum);

		if (pMob)
		{
			int iPower = pkAttacker->polymorph_power();
			iDamMin += pMob->table.damage_range[0] * iPower / 100;
			iDamMax += pMob->table.damage_range[1] * iPower / 100;
		}
	}
	else if (pWeapon)
	{
		// MONKEY_ROD_ATTACK_BUG_FIX
		item_get_damage(pWeapon, &iDamMin, &iDamMax);
		// END_OF_MONKEY_ROD_ATTACK_BUG_FIX
	}
	else if (pkAttacker->is_npc())
	{
		iDamMin = pkAttacker->mob_damage_min();
		iDamMax = pkAttacker->mob_damage_max();
	}

	iDam = number(iDamMin, iDamMax) * 2;

	// TESTSERVER_SHOW_ATTACKINFO
	DEBUG_iDamCur = iDam;
	// END_OF_TESTSERVER_SHOW_ATTACKINFO
	//
	int iAtk = 0;

	// level must be ignored when multiply by fAR, so subtract it before calculation.
	iAtk = pkAttacker->point(POINT_ATT_GRADE) + iDam - (pkAttacker->level() * 2);
	iAtk = (int) (iAtk * fAR);
	iAtk += pkAttacker->level() * 2; // and add again

	if (pWeapon)
	{
		iAtk += pWeapon->value(5) * 2;

		// 2004.11.12.myevan.TESTSERVER_SHOW_ATTACKINFO
		DEBUG_iDamBonus = pWeapon->value(5) * 2;
		///////////////////////////////////////////////
	}

	iAtk += pkAttacker->point(POINT_PARTY_ATTACKER_BONUS); // party attacker role bonus
	iAtk = (int) (iAtk * (100 + (pkAttacker->point(POINT_ATT_BONUS) + pkAttacker->point(POINT_MELEE_MAGIC_ATT_BONUS_PER))) / 100);

	iAtk = calc_att_bonus(pkAttacker, pkVictim, iAtk);

	int iDef = 0;

	if (!bIgnoreDefense)
	{
		iDef = (pkVictim->point(POINT_DEF_GRADE) * (100 + pkVictim->point(POINT_DEF_BONUS)) / 100);

		if (!pkAttacker->is_pc())
			iDef += pkVictim->marriage_bonus(UNIQUE_ITEM_MARRIAGE_DEFENSE_BONUS);
	}

	if (pkAttacker->is_npc())
		iAtk = (int) (iAtk * pkAttacker->mob_damage_multiply());

	iDam = std::max(0, iAtk - iDef);

	if (metin2::config::test_server())
	{
		int DEBUG_iLV = pkAttacker->level()*2;
		int DEBUG_iST = int((pkAttacker->point(POINT_ATT_GRADE) - DEBUG_iLV) * fAR);
		int DEBUG_iPT = pkAttacker->point(POINT_PARTY_ATTACKER_BONUS);
		int DEBUG_iWP = 0;
		int DEBUG_iPureAtk = 0;
		int DEBUG_iPureDam = 0;
		char szRB[32] = "";
		char szGradeAtkBonus[32] = "";

		DEBUG_iWP = int(DEBUG_iDamCur * fAR);
		DEBUG_iPureAtk = DEBUG_iLV + DEBUG_iST + DEBUG_iWP+DEBUG_iDamBonus;
		DEBUG_iPureDam = iAtk - iDef;

		if (pkAttacker->is_npc())
		{
			snprintf(szGradeAtkBonus, sizeof(szGradeAtkBonus), "=%d*%.1f", DEBUG_iPureAtk, pkAttacker->mob_damage_multiply());
			DEBUG_iPureAtk = int(DEBUG_iPureAtk * pkAttacker->mob_damage_multiply());
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
				pkAttacker->name(),
				iAtk,
				pkVictim->name(),
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

		pkAttacker->chat_packet(CHAT_TYPE_TALKING, "%s", szMeleeAttack);
		pkVictim->chat_packet(CHAT_TYPE_TALKING, "%s", szMeleeAttack);
	}

	return calc_battle_damage(iDam, pkAttacker->level(), pkVictim->level());
}

int calc_arrow_damage(character * pkAttacker, character * pkVictim, item * pkBow, item * pkArrow, bool bIgnoreDefense)
{
	if (!pkBow || pkBow->type() != ITEM_WEAPON || pkBow->sub_type() != WEAPON_BOW)
		return 0;

	if (!pkArrow)
		return 0;

	// Hit value calculation
	int iDist = (int) (distance_sqrt(pkAttacker->x() - pkVictim->x(), pkAttacker->y() - pkVictim->y()));
	int iGap = (iDist / 100) - 5 - pkAttacker->point(POINT_BOW_DISTANCE);
	int iPercent = 100 - (iGap * 5);

	if (iPercent <= 0)
		return 0;
	else if (iPercent > 100)
		iPercent = 100;

	int iDam = 0;

	float fAR = calc_attack_rating(pkAttacker, pkVictim, false);
	iDam = number(pkBow->value(3), pkBow->value(4)) * 2 + pkArrow->value(3);
	int iAtk;

	// level must be ignored when multiply by fAR, so subtract it before calculation.
	iAtk = pkAttacker->point(POINT_ATT_GRADE) + iDam - (pkAttacker->level() * 2);
	iAtk = (int) (iAtk * fAR);
	iAtk += pkAttacker->level() * 2; // and add again

	// Refine Grade
	iAtk += pkBow->value(5) * 2;

	iAtk += pkAttacker->point(POINT_PARTY_ATTACKER_BONUS);
	iAtk = (int) (iAtk * (100 + (pkAttacker->point(POINT_ATT_BONUS) + pkAttacker->point(POINT_MELEE_MAGIC_ATT_BONUS_PER))) / 100);

	iAtk = calc_att_bonus(pkAttacker, pkVictim, iAtk);

	int iDef = 0;

	if (!bIgnoreDefense)
		iDef = (pkVictim->point(POINT_DEF_GRADE) * (100 + pkAttacker->point(POINT_DEF_BONUS)) / 100);

	if (pkAttacker->is_npc())
		iAtk = (int) (iAtk * pkAttacker->mob_damage_multiply());

	iDam = std::max(0, iAtk - iDef);

	int iPureDam = iDam;

	iPureDam = (iPureDam * iPercent) / 100;

	if (metin2::config::test_server())
	{
		pkAttacker->chat_packet(CHAT_TYPE_INFO, "ARROW %s -> %s, DAM %d DIST %d GAP %d %% %d",
				pkAttacker->name(), 
				pkVictim->name(), 
				iPureDam, 
				iDist, iGap, iPercent);
	}

	return iPureDam;
}

void normal_attack_affect(character * pkAttacker, character * pkVictim)
{
	// Poison attack is unusual, so special handling
	if (pkAttacker->point(POINT_POISON_PCT) && !pkVictim->is_affect_flag(AFF_POISON))
	{
		if (number(1, 100) <= pkAttacker->point(POINT_POISON_PCT))
			pkVictim->attacked_by_poison(pkAttacker);
	}

	int iStunDuration = 2;
	if (pkAttacker->is_pc() && !pkVictim->is_pc())
		iStunDuration = 4;

	attack_affect(pkAttacker, pkVictim, POINT_STUN_PCT, IMMUNE_STUN,  AFFECT_STUN, POINT_NONE,        0, AFF_STUN, iStunDuration, "STUN");
	attack_affect(pkAttacker, pkVictim, POINT_SLOW_PCT, IMMUNE_SLOW,  AFFECT_SLOW, POINT_MOV_SPEED, -30, AFF_SLOW, 20,		"SLOW");
}

int battle_hit(character * pkAttacker, character * pkVictim, int & iRetDam)
{
	if (metin2::config::test_server())
		metin2::log::info( "battle_hit : [{}] attack to [{}] : dam :{:d}", pkAttacker->name(), pkVictim->name(), iRetDam);

	int iDam = calc_melee_damage(pkAttacker, pkVictim);

	if (iDam <= 0)
		return (BATTLE_DAMAGE);

	normal_attack_affect(pkAttacker, pkVictim);

	// Damage calculation
	item * pkWeapon = pkAttacker->wear(WEAR_WEAPON);

	if (pkWeapon)
		switch (pkWeapon->sub_type())
		{
			case WEAPON_SWORD:
				iDam = iDam * (100 - pkVictim->point(POINT_RESIST_SWORD)) / 100;
				break;

			case WEAPON_TWO_HANDED:
				iDam = iDam * (100 - pkVictim->point(POINT_RESIST_TWOHAND)) / 100;
				break;

			case WEAPON_DAGGER:
				iDam = iDam * (100 - pkVictim->point(POINT_RESIST_DAGGER)) / 100;
				break;

			case WEAPON_BELL:
				iDam = iDam * (100 - pkVictim->point(POINT_RESIST_BELL)) / 100;
				break;

			case WEAPON_FAN:
				iDam = iDam * (100 - pkVictim->point(POINT_RESIST_FAN)) / 100;
				break;

			case WEAPON_BOW:
				iDam = iDam * (100 - pkVictim->point(POINT_RESIST_BOW)) / 100;
				break;
		}

	//Final damage adjustment. (As of Feb 2011, applies only to the Giant Spider.)
	float attMul = pkAttacker->att_mul();
	float tempIDam = iDam;
	iDam = attMul * tempIDam + 0.5f;

	iRetDam = iDam;

	if (pkVictim->damage(pkAttacker, iDam, DAMAGE_TYPE_NORMAL))
		return (BATTLE_DEAD);

	return (BATTLE_DAMAGE);
}

uint32_t get_attack_speed(character * ch)
{
	if (nullptr == ch)
		return 1000;

	item * item = ch->wear(WEAR_WEAPON);	
	uint32_t default_bonus = metin2::config::game().anti_cheat.speed_hack_bonus * 3;    // Leeway attack speed (default 80) (*3 added to keep normal users from being flagged for speed hack. 2013.09.11 CYH)
	uint32_t riding_bonus = 0;

	if (ch->is_riding())
	{
		// +50 attack speed if riding something
		riding_bonus = 50;
	}

	uint32_t ani_speed = ani_attack_speed(ch);
    uint32_t real_speed = (ani_speed * 100) / (default_bonus + ch->point(POINT_ATT_SPEED) + riding_bonus);

	// Double attack speed for daggers
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

bool is_speed_hack(character * ch, character * victim, uint32_t current_time)
{
	// 2013 09 11 CYH debugging log
	/*metin2::log::info( "{} attack test log! time (delta, limit)=({:d}, {:d}). ch->m_kAttackLog.dwvID({:d}) victim->GetVID({:d})",
						ch->GetName(),
						current_time - ch->m_kAttackLog.dwTime,
						GET_ATTACK_SPEED(ch),						
						ch->m_kAttackLog.dwVID,
						victim->GetVID()
						);	

	metin2::log::info( "{} attack test log! time (delta, limit)=({:d}, {:d}). victim->m_AttackedLog.dwPID({:d}) ch->GetPlayerID({:d})",
						ch->GetName(),
						current_time - victim->m_AttackedLog.dwAttackedTime,
						GET_ATTACK_SPEED(ch),						
						victim->m_AttackedLog.dwPID,
						ch->GetPlayerID()
						);*/

	if (ch->attack_log.vid == victim->get_vid())
	{
		if (current_time - ch->attack_log.time < get_attack_speed(ch))
		{
			++ch->speed_hack_count;

			if (metin2::config::test_server())
			{
				metin2::log::info( "{} attack hack! time (delta, limit)=({:d}, {:d}) hack_count {:d}",
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
				metin2::log::info( "{} Attack Speed HACK! time (delta, limit)=({:d}, {:d}), hack_count = {:d}",
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
