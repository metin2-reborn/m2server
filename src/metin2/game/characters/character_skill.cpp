#include <metin2/game/characters/affect.h>
#include <metin2/game/combat/battle.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/app/event.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/items/item.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/persistence/log.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/world/sectree.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/world/vector.h>
#include <metin2/game/world/vid.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game::inline characters {

static const uint32_t sub_skill_vnums[] =
{
	SKILL_LEADERSHIP,
	SKILL_COMBO,
	SKILL_MINING,
	SKILL_LANGUAGE1,
	SKILL_LANGUAGE2,
	SKILL_LANGUAGE3,
	SKILL_POLYMORPH,
	SKILL_HORSE,
	SKILL_HORSE_SUMMON,
	SKILL_HORSE_WILDATTACK,
	SKILL_HORSE_CHARGE,
	SKILL_HORSE_ESCAPE,
	SKILL_HORSE_WILDATTACK_RANGE,
	SKILL_ADD_HP,
	SKILL_RESIST_PENETRATE
};

time_t character::skill_next_read_time(uint32_t dwVnum) const
{
	if (dwVnum >= SKILL_MAX_NUM)
	{
		metin2::log::error("vnum overflow (vnum: {:d})", dwVnum);
		return 0;
	}

	return skill_levels_ ? skill_levels_[dwVnum].t_next_read : 0;
}

void character::set_skill_next_read_time(uint32_t dwVnum, time_t time)
{
	if (skill_levels_ && dwVnum < SKILL_MAX_NUM)
		skill_levels_[dwVnum].t_next_read = time;
}

bool skill_use_info::hit_once(uint32_t dwVnum)
{
	// if it wasn't used, it can't hit.
	if (!used)
		return false;

	metin2::log::debug( "__HitOnce NextUse {:d} current {:d} count {:d} scount {:d}", next_skill_usable_time, get_dword_time(), hit_count, splash_count);

	if (next_skill_usable_time && next_skill_usable_time<get_dword_time() && dwVnum != SKILL_MUYEONG && dwVnum != SKILL_HORSE_WILDATTACK)
	{
		metin2::log::debug( "__HitOnce can't hit");

		return false;
	}

	if (hit_count == -1)
	{
		metin2::log::debug( "__HitOnce OK {:d} {:d} {:d}", next_skill_usable_time, get_dword_time(), hit_count);
		return true;
	}

	if (hit_count)
	{
		metin2::log::debug( "__HitOnce OK {:d} {:d} {:d}", next_skill_usable_time, get_dword_time(), hit_count);
		hit_count--;
		return true;
	}
	return false;
}

bool skill_use_info::use_skill(bool isGrandMaster, uint32_t vid_value, uint32_t dwCooltime, int splashcount, int hitcount, int range_value)
{
	this->is_grand_master = isGrandMaster;
	uint32_t dwCur = get_dword_time();

	// cooldown has not finished yet.
	if (used && next_skill_usable_time > dwCur)
	{
		metin2::log::info( "cooltime is not over delta {:d}", next_skill_usable_time - dwCur);
		hit_count = 0;
		return false;
	}

	used = true;

	if (dwCooltime)
		next_skill_usable_time = dwCur + dwCooltime;
	else
		next_skill_usable_time = 0;

	range = range_value;
	max_hit_count = hit_count = hitcount;

	if (metin2::config::test_server())
		metin2::log::info( "UseSkill NextUse {:d}  current {:d} cooltime {:d} hitcount {:d}/{:d}", next_skill_usable_time, dwCur, dwCooltime, hit_count, max_hit_count);

	vid = vid_value;
	splash_count = splashcount;
	return true;
}

int character::chain_lightning_max_count() const
{ 
	return chain_lightning_count_by_skill_level[std::min(SKILL_MAX_LEVEL, skill_level(SKILL_CHAIN))];
}

void character::set_affected_eunhyung() 
{ 
	affected_eunhyung_level_ = skill_power(SKILL_EUNHYUNG); 
}

void character::set_skill_group(uint8_t bSkillGroup)
{
	if (bSkillGroup > 2) 
		return;

	if (level() < 5)
		return;

	points_.skill_group = bSkillGroup; 

	packet_gc_change_skill_group p;
	p.header = HEADER_GC_SKILL_GROUP;
	p.skill_group = points_.skill_group;

	get_desc()->packet(&p, sizeof(packet_gc_change_skill_group));
}

int character::compute_cooltime(int time)
{
	return calculate_duration(point(POINT_CASTING_SPEED), time);
}

void character::skill_level_packet()
{
	if (!get_desc())
		return;

	packet_gc_skill_level pack;

	pack.header = HEADER_GC_SKILL_LEVEL;
	std::memcpy(&pack.skills, skill_levels_.get(), sizeof(player_skill) * SKILL_MAX_NUM);
	get_desc()->packet(&pack, sizeof(packet_gc_skill_level));
}

void character::set_skill_level(uint32_t dwVnum, uint8_t bLev)
{
	if (nullptr == skill_levels_)
		return;

	if (dwVnum >= SKILL_MAX_NUM)
	{
		metin2::log::error("vnum overflow (vnum {:d})", dwVnum);
		return;
	}

	skill_levels_[dwVnum].level = std::min<int>(40, bLev);

	if (bLev >= 40)
		skill_levels_[dwVnum].master_type = SKILL_PERFECT_MASTER;
	else if (bLev >= 30)
		skill_levels_[dwVnum].master_type = SKILL_GRAND_MASTER;
	else if (bLev >= 20)
		skill_levels_[dwVnum].master_type = SKILL_MASTER;
	else
		skill_levels_[dwVnum].master_type = SKILL_NORMAL;
}

bool character::is_learnable_skill(uint32_t dwSkillVnum) const
{
	const skill_proto * pkSkill = skill_manager::instance().get(dwSkillVnum);

	if (!pkSkill)
		return false;

	if (skill_level(dwSkillVnum) >= SKILL_MAX_LEVEL)
		return false;

	if (pkSkill->type == 0)
	{
		if (skill_level(dwSkillVnum) >= pkSkill->max_level)
			return false;

		return true;
	}

	if (pkSkill->type == 5)
	{
		if (dwSkillVnum == SKILL_HORSE_WILDATTACK_RANGE && job() != JOB_ASSASSIN)
			return false; 

		return true;
	}

	if (skill_group() == 0)
		return false;

	if (pkSkill->type - 1 == job())
		return true;

	if (6 == pkSkill->type)
	{
		if (SKILL_7_A_ANTI_TANHWAN <= dwSkillVnum && dwSkillVnum <= SKILL_7_D_ANTI_YONGBI)
		{
			for (int i=0 ; i < 4 ; i++)
			{
				if (unsigned(SKILL_7_A_ANTI_TANHWAN + i) != dwSkillVnum)
				{
					if (0 != skill_level(SKILL_7_A_ANTI_TANHWAN + i))
					{
						return false;
					}
				}
			}

			return true;
		}

		if (SKILL_8_A_ANTI_GIGONGCHAM <= dwSkillVnum && dwSkillVnum <= SKILL_8_D_ANTI_BYEURAK)
		{
			for (int i=0 ; i < 4 ; i++)
			{
				if (unsigned(SKILL_8_A_ANTI_GIGONGCHAM + i) != dwSkillVnum)
				{
					if (0 != skill_level(SKILL_8_A_ANTI_GIGONGCHAM + i))
						return false;
				}
			}

			return true;
		}
	}

	return false;
}

// ADD_GRANDMASTER_SKILL
bool character::learn_grand_master_skill(uint32_t dwSkillVnum)
{
	skill_proto * pkSk = skill_manager::instance().get(dwSkillVnum);

	if (!pkSk)
		return false;

	if (!is_learnable_skill(dwSkillVnum))
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("수련할 수 없는 스킬입니다."));
		return false;
	}

	metin2::log::info( "learn grand master skill[{:d}] cur {:d}, next {:d}", dwSkillVnum, get_global_time(), skill_next_read_time(dwSkillVnum));

	// if bType is 0, can be trained with books from the start
	if (pkSk->type == 0)
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("그랜드 마스터 수련을 할 수 없는 스킬입니다."));
		return false;
	}

	if (skill_master_type(dwSkillVnum) != SKILL_GRAND_MASTER)
	{
		if (skill_master_type(dwSkillVnum) > SKILL_GRAND_MASTER)
			chat_packet(CHAT_TYPE_INFO, lc_text("퍼펙트 마스터된 스킬입니다. 더 이상 수련 할 수 없습니다."));
		else
			chat_packet(CHAT_TYPE_INFO, lc_text("이 스킬은 아직 그랜드 마스터 수련을 할 경지에 이르지 않았습니다."));
		return false;
	}

	std::string strTrainSkill;
	{
		std::ostringstream os;
		os << "training_grandmaster_skill.skill" << dwSkillVnum;
		strTrainSkill = os.str();
	}

	// probability is calculated here.
	uint8_t bLastLevel = skill_level(dwSkillVnum);

	int idx = std::min(9, skill_level(dwSkillVnum) - 30);

	metin2::log::info( "LearnGrandMasterSkill {} table idx {:d} value {:d}", name(), idx, grand_master_skill_book_count_for_level_up[idx]);

	int iTotalReadCount = quest_flag(strTrainSkill) + 1;
	set_quest_flag(strTrainSkill, iTotalReadCount);

	int iMinReadCount = grand_master_skill_book_min_count[idx];
	int iMaxReadCount = grand_master_skill_book_max_count[idx];

	int iBookCount = grand_master_skill_book_count_for_level_up[idx];

	if (find_affect(AFFECT_SKILL_BOOK_BONUS))
	{
		if (iBookCount&1)
			iBookCount = iBookCount / 2 + 1; 
		else
			iBookCount = iBookCount / 2; 

		remove_affect(AFFECT_SKILL_BOOK_BONUS);
	}

	int n = number(1, iBookCount);
	metin2::log::info( "Number({:d})", n);

	uint32_t nextTime = get_global_time() + number(28800, 43200);

	metin2::log::info( "GrandMaster SkillBookCount min {:d} cur {:d} max {:d} (next_time={:d})", iMinReadCount, iTotalReadCount, iMaxReadCount, nextTime);

	bool bSuccess = n == 2;

	if (iTotalReadCount < iMinReadCount)
		bSuccess = false;
	if (iTotalReadCount > iMaxReadCount)
		bSuccess = true;

	if (bSuccess)
	{
		skill_level_up(dwSkillVnum, SKILL_UP_BY_QUEST);
	}

	set_skill_next_read_time(dwSkillVnum, nextTime);

	if (bLastLevel == skill_level(dwSkillVnum))
	{
		chat_packet(CHAT_TYPE_TALKING, lc_text("크윽, 기가 역류하고 있어! 이거 설마 주화입마인가!? 젠장!"));
		chat_packet(CHAT_TYPE_INFO, lc_text("수련이 실패로 끝났습니다. 다시 도전해주시기 바랍니다."));
		log_manager::instance().char_log(this, dwSkillVnum, "GM_READ_FAIL", "");
		return false;
	}

	chat_packet(CHAT_TYPE_TALKING, lc_text("몸에서 뭔가 힘이 터져 나오는 기분이야!"));
	chat_packet(CHAT_TYPE_TALKING, lc_text("뜨거운 무엇이 계속 용솟음치고 있어! 이건, 이것은!"));
	chat_packet(CHAT_TYPE_INFO, lc_text("더 높은 경지의 수련을 성공적으로 끝내셨습니다."));
	log_manager::instance().char_log(this, dwSkillVnum, "GM_READ_SUCCESS", "");
	return true;
}
// END_OF_ADD_GRANDMASTER_SKILL

static bool fn_should_check_exp(character * ch)
{

	return true;

	return false;
}

bool character::learn_skill_by_book(uint32_t dwSkillVnum, uint8_t bProb)
{
	const skill_proto* pkSk = skill_manager::instance().get(dwSkillVnum);

	if (!pkSk)
		return false;

	if (!is_learnable_skill(dwSkillVnum))
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("수련할 수 없는 스킬입니다."));
		return false;
	}

	uint32_t need_exp = 0;

	if (fn_should_check_exp(this))
	{
		need_exp = 20000;

		if ( get_exp() < need_exp )
		{
			chat_packet(CHAT_TYPE_INFO, lc_text("경험치가 부족하여 책을 읽을 수 없습니다."));
			return false;
		}
	}

	// if bType is 0, can be trained with books from the start
	if (pkSk->type != 0)
	{
		if (skill_master_type(dwSkillVnum) != SKILL_MASTER)
		{
			if (skill_master_type(dwSkillVnum) > SKILL_MASTER)
				chat_packet(CHAT_TYPE_INFO, lc_text("이 스킬은 책으로 더이상 수련할 수 없습니다."));
			else
				chat_packet(CHAT_TYPE_INFO, lc_text("이 스킬은 아직 책으로 수련할 경지에 이르지 않았습니다."));
			return false;
		}
	}

	if (get_global_time() < skill_next_read_time(dwSkillVnum))
	{
		if (!(metin2::config::test_server() && quest::quest_manager::instance().event_flag("no_read_delay")))
		{
			if (find_affect(AFFECT_SKILL_NO_BOOK_DELAY))
			{
				// ignore time limit while using an exorcism scroll
				remove_affect(AFFECT_SKILL_NO_BOOK_DELAY);
				chat_packet(CHAT_TYPE_INFO, lc_text("주안술서를 통해 주화입마에서 빠져나왔습니다."));
			}
			else 	    
			{
				skill_learn_wait_more_time_message(skill_next_read_time(dwSkillVnum) - get_global_time());
				return false;
			}
		}
	}

	// probability is calculated here.
	uint8_t bLastLevel = skill_level(dwSkillVnum);

	if (bProb != 0)
	{
		// SKILL_BOOK_BONUS
		if (find_affect(AFFECT_SKILL_BOOK_BONUS))
		{
			bProb += bProb / 2;
			remove_affect(AFFECT_SKILL_BOOK_BONUS);
		}
		// END_OF_SKILL_BOOK_BONUS

		metin2::log::info( "LearnSkillByBook Pct {:d} prob {:d}", dwSkillVnum, bProb);

		if (number(1, 100) <= bProb)
		{
			if (metin2::config::test_server())
				metin2::log::info( "LearnSkillByBook {:d} SUCC", dwSkillVnum);

			skill_level_up(dwSkillVnum, SKILL_UP_BY_BOOK);
		}
		else
		{
			if (metin2::config::test_server())
				metin2::log::info( "LearnSkillByBook {:d} FAIL", dwSkillVnum);
		}
	}
	else
	{
		int idx = std::min(9, skill_level(dwSkillVnum) - 20);

		metin2::log::info( "LearnSkillByBook {} table idx {:d} value {:d}", name(), idx, skill_book_count_for_level_up[idx]);

		int need_bookcount = skill_level(dwSkillVnum) - 20;

		point_change(POINT_EXP, -need_exp);

		quest::quest_manager& q = quest::quest_manager::instance();
		quest::pc* pPC = q.get_pc(player_id());

		if (pPC)
		{
			char flag[128+1];
			memset(flag, 0, sizeof(flag));
			snprintf(flag, sizeof(flag), "traning_master_skill.%u.read_count", dwSkillVnum);

			int read_count = pPC->flag(flag);
			int percent = 65;

			if (find_affect(AFFECT_SKILL_BOOK_BONUS))
			{
				percent = 0;
				remove_affect(AFFECT_SKILL_BOOK_BONUS);
			}

			if (number(1, 100) > percent)
			{
				// book reading succeeded
				if (read_count >= need_bookcount)
				{
					skill_level_up(dwSkillVnum, SKILL_UP_BY_BOOK);
					pPC->set_flag(flag, 0);

					chat_packet(CHAT_TYPE_INFO, lc_text("책으로 더 높은 경지의 수련을 성공적으로 끝내셨습니다."));
					log_manager::instance().char_log(this, dwSkillVnum, "READ_SUCCESS", "");
					return true;
				}
				else
				{
					pPC->set_flag(flag, read_count + 1);

					switch (number(1, 3))
					{
						case 1:
							chat_packet(CHAT_TYPE_TALKING, lc_text("어느정도 이 기술에 대해 이해가 되었지만 조금 부족한듯 한데.."));
							break;

						case 2:
							chat_packet(CHAT_TYPE_TALKING, lc_text("드디어 끝이 보이는 건가...  이 기술은 이해하기가 너무 힘들어.."));
							break;

						case 3:
						default:
							chat_packet(CHAT_TYPE_TALKING, lc_text("열심히 하는 배움을 가지는 것만이 기술을 배울수 있는 유일한 길이다.."));
							break;
					}

					chat_packet(CHAT_TYPE_INFO, lc_text("%d 권을 더 읽어야 수련을 완료 할 수 있습니다."), need_bookcount - read_count);
					return true;
				}
			}
		}
		else
		{
			// failed to load the user's quest info
		}
		// END_OF_INTERNATIONAL_VERSION
	}

	if (bLastLevel != skill_level(dwSkillVnum))
	{
		chat_packet(CHAT_TYPE_TALKING, lc_text("몸에서 뭔가 힘이 터져 나오는 기분이야!"));
		chat_packet(CHAT_TYPE_TALKING, lc_text("뜨거운 무엇이 계속 용솟음치고 있어! 이건, 이것은!"));
		chat_packet(CHAT_TYPE_INFO, lc_text("책으로 더 높은 경지의 수련을 성공적으로 끝내셨습니다."));
		log_manager::instance().char_log(this, dwSkillVnum, "READ_SUCCESS", "");
	}
	else
	{
		chat_packet(CHAT_TYPE_TALKING, lc_text("크윽, 기가 역류하고 있어! 이거 설마 주화입마인가!? 젠장!"));
		chat_packet(CHAT_TYPE_INFO, lc_text("수련이 실패로 끝났습니다. 다시 도전해주시기 바랍니다."));
		log_manager::instance().char_log(this, dwSkillVnum, "READ_FAIL", "");
	}

	return true;
}

bool character::skill_level_down(uint32_t dwVnum)
{
	if (nullptr == skill_levels_)
		return false;

	if (metin2::config::game().skill_disable)
		return false;

	if (is_polymorphed())
		return false;

	skill_proto * pkSk = skill_manager::instance().get(dwVnum);

	if (!pkSk)
	{
		metin2::log::error("There is no such skill by number {:d}", dwVnum);
		return false;
	}

	if (!is_learnable_skill(dwVnum))
		return false;

	if (skill_master_type(pkSk->vnum) != SKILL_NORMAL)
		return false;

	if (!skill_group())
		return false;

	if (pkSk->vnum >= SKILL_MAX_NUM)
		return false;

	if (skill_levels_[pkSk->vnum].level == 0)
		return false;

	int idx = POINT_SKILL;
	switch (pkSk->type)
	{
		case 0:
			idx = POINT_SUB_SKILL;
			break;
		case 1:
		case 2:
		case 3:
		case 4:
		case 6:
			idx = POINT_SKILL;
			break;
		case 5:
			idx = POINT_HORSE_SKILL;
			break;
		default:
			metin2::log::error("Wrong skill type {:d} skill vnum {:d}", pkSk->type, pkSk->vnum);
			return false;

	}

	point_change(idx, +1);
	set_skill_level(pkSk->vnum, skill_levels_[pkSk->vnum].level - 1);

	metin2::log::info( "SkillDown: {} {:d} {:d} {:d} type {:d}", name(), pkSk->vnum, skill_levels_[pkSk->vnum].master_type, skill_levels_[pkSk->vnum].level, pkSk->type);
	save();

	compute_points();
	skill_level_packet();
	return true;
}

void character::skill_level_up(uint32_t dwVnum, uint8_t bMethod)
{
	if (nullptr == skill_levels_)
		return;

	if (metin2::config::game().skill_disable)
		return;

	if (is_polymorphed())
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("둔갑 중에는 능력을 올릴 수 없습니다."));
		return;
	}

	if (SKILL_7_A_ANTI_TANHWAN <= dwVnum && dwVnum <= SKILL_8_D_ANTI_BYEURAK)
	{
		if (0 == skill_level(dwVnum))
			return;
	}

	const skill_proto* pkSk = skill_manager::instance().get(dwVnum);

	if (!pkSk)
	{
		metin2::log::error("There is no such skill by number (vnum {:d})", dwVnum);
		return;
	}

	if (pkSk->vnum >= SKILL_MAX_NUM)
	{
		metin2::log::error("Skill Vnum overflow (vnum {:d})", dwVnum);
		return;
	}

	if (!is_learnable_skill(dwVnum))
		return;

	// grand master can only be done through quests
	if (pkSk->type != 0)
	{
		switch (skill_master_type(pkSk->vnum))
		{
			case SKILL_GRAND_MASTER:
				if (bMethod != SKILL_UP_BY_QUEST)
					return;
				break;

			case SKILL_PERFECT_MASTER:
				return;
		}
	}

	if (bMethod == SKILL_UP_BY_POINT)
	{
		// can only train when not master
		if (skill_master_type(pkSk->vnum) != SKILL_NORMAL)
			return;

		if (is_set(pkSk->flag, SKILL_FLAG_DISABLE_BY_POINT_UP))
			return;
	}
	else if (bMethod == SKILL_UP_BY_BOOK)
	{
		if (pkSk->type != 0) // skills not belonging to a job or that can't be raised with points can be learned with books from the start.
			if (skill_master_type(pkSk->vnum) != SKILL_MASTER)
				return;
	}

	if (level() < pkSk->level_limit)
		return;

	if (pkSk->pre_skill_vnum)
		if (skill_master_type(pkSk->pre_skill_vnum) == SKILL_NORMAL &&
			skill_level(pkSk->pre_skill_vnum) < pkSk->pre_skill_level)
			return;

	if (!skill_group())
		return;

	if (bMethod == SKILL_UP_BY_POINT)
	{
		int idx;

		switch (pkSk->type)
		{
			case 0:
				idx = POINT_SUB_SKILL;
				break;

			case 1:
			case 2:
			case 3:
			case 4:
			case 6:
				idx = POINT_SKILL;
				break;

			case 5:
				idx = POINT_HORSE_SKILL;
				break;

			default:
				metin2::log::error("Wrong skill type {:d} skill vnum {:d}", pkSk->type, pkSk->vnum);
				return;
		}

		if (point(idx) < 1)
			return;

		point_change(idx, -1);
	}

	int SkillPointBefore = skill_level(pkSk->vnum);
	set_skill_level(pkSk->vnum, skill_levels_[pkSk->vnum].level + 1);

	if (pkSk->type != 0)
	{
		// code for a sudden grade-up
		switch (skill_master_type(pkSk->vnum))
		{
			case SKILL_NORMAL:
				// on the Vietnamese server, master training at a random skill level between 17~20
				if (skill_level(pkSk->vnum) >= 17)
				{
					if (quest_flag("reset_scroll.force_to_master_skill") > 0)
					{
						set_skill_level(pkSk->vnum, 20);
						set_quest_flag("reset_scroll.force_to_master_skill", 0);
					}
					else
					{
						if (number(1, 21 - std::min(20, skill_level(pkSk->vnum))) == 1)
							set_skill_level(pkSk->vnum, 20);
					}
				}
				break;

			case SKILL_MASTER:
				if (skill_level(pkSk->vnum) >= 30)
				{
					if (number(1, 31 - std::min(30, skill_level(pkSk->vnum))) == 1)
						set_skill_level(pkSk->vnum, 30);
				}
				break;

			case SKILL_GRAND_MASTER:
				if (skill_level(pkSk->vnum) >= 40)
				{
					set_skill_level(pkSk->vnum, 40);
				}
				break;
		}
	}

	char szSkillUp[1024];

	snprintf(szSkillUp, sizeof(szSkillUp), "SkillUp: %s %u %d %d[Before:%d] type %u",
			name(), pkSk->vnum, skill_levels_[pkSk->vnum].master_type, skill_levels_[pkSk->vnum].level, SkillPointBefore, pkSk->type);

	metin2::log::info( "{}", szSkillUp);

	log_manager::instance().char_log(this, pkSk->vnum, "SKILLUP", szSkillUp);
	save();

	compute_points();
	skill_level_packet();
}

void character::compute_skill_points()
{
	if (metin2::config::game().skill_disable)
		return;
}

void character::reset_skill()
{
	if (nullptr == skill_levels_)
		return;

	// support skills are not reset
	std::vector<std::pair<uint32_t, player_skill> > vec;
	size_t count = sizeof(sub_skill_vnums) / sizeof(sub_skill_vnums[0]);

	for (size_t i = 0; i < count; ++i)
	{
		if (sub_skill_vnums[i] >= SKILL_MAX_NUM)
			continue;

		vec.push_back(std::make_pair(sub_skill_vnums[i], skill_levels_[sub_skill_vnums[i]]));
	}

	memset(skill_levels_.get(), 0, sizeof(player_skill) * SKILL_MAX_NUM);

	std::vector<std::pair<uint32_t, player_skill> >::const_iterator iter = vec.begin();

	while (iter != vec.end())
	{
		const std::pair<uint32_t, player_skill>& pair = *(iter++);
		skill_levels_[pair.first] = pair.second;
	}

	compute_points();
	skill_level_packet();
}

struct f_find_near_victim
{
	f_find_near_victim(character * center, character * attacker, const character_set& excepts_set_value = empty_set_)
		: chr_center(center),
	chr_next_target(nullptr),
	chr_attacker(attacker),
	count(0),
	excepts_set(excepts_set_value)
	{
	}

	void operator ()(entity * ent)
	{
		if (!ent->is_type(ENTITY_CHARACTER))
			return;

		character * pkChr = (character *) ent;

		if (!excepts_set.empty()) {
			if (excepts_set.find(pkChr) != excepts_set.end())
				return;
		}

		if (chr_center == pkChr)
			return;

		if (!battle_is_attackable(chr_attacker, pkChr))
		{
			return;
		}

		if (abs(chr_center->x() - pkChr->x()) > 1000 || abs(chr_center->y() - pkChr->y()) > 1000)
			return;

		float fDist = distance_approx(chr_center->x() - pkChr->x(), chr_center->y() - pkChr->y());

		if (fDist < 1000)
		{
			++count;

			if ((count == 1) || number(1, count) == 1)
				chr_next_target = pkChr;
		}
	}

	character * victim()
	{
		return chr_next_target;
	}

	character * chr_center;
	character * chr_next_target;
	character * chr_attacker;
	int		count;
	const character_set & excepts_set;
private:
	static character_set empty_set_;
};

character_set f_find_near_victim::empty_set_;

EVENTINFO(chain_lightning_event_info)
{
	uint32_t			victim;
	uint32_t			chr;

	chain_lightning_event_info()
	: victim(0)
	, chr(0)
	{
	}
};

EVENTFUNC(chain_lightning_event)
{
	chain_lightning_event_info * info = dynamic_cast<chain_lightning_event_info *>( event->info );

	character * pkChrVictim = character_manager::instance().find(info->victim);
	character * pkChr = character_manager::instance().find(info->chr);
	character * pkTarget = nullptr;

	if (!pkChr || !pkChrVictim)
	{
		metin2::log::debug( "use chainlighting, but no character");
		return 0;
	}

	metin2::log::debug( "chainlighting event {}", pkChr->name());

	if (pkChrVictim->get_party()) // party first
	{
		pkTarget = pkChrVictim->get_party()->next_ownership(nullptr, pkChrVictim->x(), pkChrVictim->y());
		if (pkTarget == pkChrVictim || !number(0, 2) || pkChr->chain_lighting_except().find(pkTarget) != pkChr->chain_lighting_except().end())
			pkTarget = nullptr;
	}

	if (!pkTarget)
	{
		// 1. Find Next victim
		f_find_near_victim f(pkChrVictim, pkChr, pkChr->chain_lighting_except());

		if (pkChrVictim->get_sectree())
		{
			pkChrVictim->get_sectree()->for_each_around(f);
			// 2. If exist, compute it again
			pkTarget = f.victim();
		}
	}

	if (pkTarget)
	{
		pkChrVictim->create_fly(FLY_CHAIN_LIGHTNING, pkTarget);
		pkChr->compute_skill(SKILL_CHAIN, pkTarget);
		pkChr->add_chain_lightning_except(pkTarget);
	}
	else
	{
		metin2::log::debug( "{} use chainlighting, but find victim failed near {}", pkChr->name(), pkChrVictim->name());
	}

	return 0;
}

void set_poly_var_for_attack(character * ch, skill_proto * pkSk, item * pkWeapon)
{
	if (ch->is_pc())
	{
		if (pkWeapon && pkWeapon->type() == ITEM_WEAPON)
		{
			int iWep = number(pkWeapon->value(3), pkWeapon->value(4));
			iWep += pkWeapon->value(5);

			int iMtk = number(pkWeapon->value(1), pkWeapon->value(2));
			iMtk += pkWeapon->value(5);

			pkSk->set_point_var("wep", iWep);
			pkSk->set_point_var("mtk", iMtk);
			pkSk->set_point_var("mwep", iMtk);
		}
		else
		{
			pkSk->set_point_var("wep", 0);
			pkSk->set_point_var("mtk", 0);
			pkSk->set_point_var("mwep", 0);
		}
	}
	else
	{
		int iWep = number(ch->mob_damage_min(), ch->mob_damage_max());
		pkSk->set_point_var("wep", iWep);
		pkSk->set_point_var("mwep", iWep);
		pkSk->set_point_var("mtk", iWep);
	}
}

struct func_splash_damage
{
	func_splash_damage(int x_value, int y_value, skill_proto * pkSk, character * pkChr, int iAmount, int iAG, int iMaxHit, item * pkWeapon, bool bDisableCooltime, skill_use_info* pInfo, uint8_t bUseSkillPower)
		:
		x(x_value), y(y_value), sk(pkSk), chr(pkChr), amount(iAmount), count(0), max_hit(iMaxHit), weapon(pkWeapon), disable_cooltime(bDisableCooltime), info(pInfo), use_skill_power(bUseSkillPower)
		{
		}

	void operator () (entity * ent)
	{
		if (!ent->is_type(ENTITY_CHARACTER))
		{
			return;
		}

		character * pkChrVictim = (character *) ent;

		if (distance_approx(x - pkChrVictim->x(), y - pkChrVictim->y()) > sk->splash_range)
		{
			if(metin2::config::test_server())
				metin2::log::info( "XXX target too far {}", chr->name());
			return;
		}

		if (!battle_is_attackable(chr, pkChrVictim))
		{
			if(metin2::config::test_server())
				metin2::log::info( "XXX target not attackable {}", chr->name());
			return;
		}

		if (chr->is_pc())
			// guild skills have no cooldown handling.
			if (!(sk->vnum >= GUILD_SKILL_START && sk->vnum <= GUILD_SKILL_END))
				if (!disable_cooltime && info && !info->hit_once(sk->vnum) && sk->vnum != SKILL_MUYEONG)
				{
					if(metin2::config::test_server())
						metin2::log::info( "check guild skill {}", chr->name());
					return;
				}

		++count;

		int iDam;

		////////////////////////////////////////////////////////////////////////////////
		sk->set_point_var("k", 1.0 * use_skill_power * sk->max_level / 100);
		sk->set_point_var("lv", chr->level());
		sk->set_point_var("iq", chr->point(POINT_IQ));
		sk->set_point_var("str", chr->point(POINT_ST));
		sk->set_point_var("dex", chr->point(POINT_DX));
		sk->set_point_var("con", chr->point(POINT_HT));
		sk->set_point_var("def", chr->point(POINT_DEF_GRADE));
		sk->set_point_var("odef", chr->point(POINT_DEF_GRADE) - chr->point(POINT_DEF_GRADE_BONUS));
		sk->set_point_var("horse_level", chr->horse_level());

		bool bIgnoreDefense = false;

		if (is_set(sk->flag, SKILL_FLAG_PENETRATE))
		{
			int iPenetratePct = (int) sk->point_poly2.eval();

			if (number(1, 100) <= iPenetratePct)
				bIgnoreDefense = true;
		}

		bool bIgnoreTargetRating = false;

		if (is_set(sk->flag, SKILL_FLAG_IGNORE_TARGET_RATING))
		{
			int iPct = (int) sk->point_poly2.eval();

			if (number(1, 100) <= iPct)
				bIgnoreTargetRating = true;
		}

		sk->set_point_var("ar", calc_attack_rating(chr, pkChrVictim, bIgnoreTargetRating));

		if (is_set(sk->flag, SKILL_FLAG_USE_MELEE_DAMAGE))
			sk->set_point_var("atk", calc_melee_damage(chr, pkChrVictim, true, bIgnoreTargetRating));
		else if (is_set(sk->flag, SKILL_FLAG_USE_ARROW_DAMAGE))
		{
			item * pkBow;
			item * pkArrow;

			if (1 == chr->arrow_and_bow(&pkBow, &pkArrow, 1))
				sk->set_point_var("atk", calc_arrow_damage(chr, pkChrVictim, pkBow, pkArrow, true));
			else
				sk->set_point_var("atk", 0);
		}

		if (sk->point_on == POINT_MOV_SPEED)
			sk->point_poly.set_var("maxv", pkChrVictim->limit_point(POINT_MOV_SPEED));

		sk->set_point_var("maxhp", pkChrVictim->max_hp());
		sk->set_point_var("maxsp", pkChrVictim->max_sp());

		sk->set_point_var("chain", chr->chain_lightning_index());
		chr->inc_chain_lightning_index();

		bool bUnderEunhyung = chr->affected_eunhyung() > 0; // why is this done here??

		sk->set_point_var("ek", chr->affected_eunhyung()*1./100);
		set_poly_var_for_attack(chr, sk, weapon);

		int iAmount = 0;

		if (chr->used_skill_master_type(sk->vnum) >= SKILL_GRAND_MASTER)
		{
			iAmount = (int) sk->master_bonus_poly.eval();
		}
		else
		{
			iAmount = (int) sk->point_poly.eval();
		}

		if (metin2::config::test_server() && iAmount == 0 && sk->point_on != POINT_NONE)
		{
			chr->chat_packet(CHAT_TYPE_INFO, "효과가 없습니다. 스킬 공식을 확인하세요");
		}
		////////////////////////////////////////////////////////////////////////////////
		iAmount = -iAmount;

		if (sk->vnum == SKILL_AMSEOP)
		{
			float fDelta = get_degree_delta(chr->rotation(), pkChrVictim->rotation());
			float adjust;

			if (fDelta < 35.0f)
			{
				adjust = 1.5f;

				if (bUnderEunhyung)
					adjust += 0.5f;

				if (chr->wear(WEAR_WEAPON) && chr->wear(WEAR_WEAPON)->sub_type() == WEAPON_DAGGER)
				{
					adjust += 0.5f;
				}
			}
			else
			{
				adjust = 1.0f;

				if (bUnderEunhyung)
					adjust += 0.5f;

				if (chr->wear(WEAR_WEAPON) && chr->wear(WEAR_WEAPON)->sub_type() == WEAPON_DAGGER)
					adjust += 0.5f;
			}

			iAmount = (int) (iAmount * adjust);
		}
		else if (sk->vnum == SKILL_GUNGSIN)
		{
			float adjust = 1.0;

			if (chr->wear(WEAR_WEAPON) && chr->wear(WEAR_WEAPON)->sub_type() == WEAPON_DAGGER)
			{
				adjust = 1.35f;
			}

			iAmount = (int) (iAmount * adjust);
		}
		////////////////////////////////////////////////////////////////////////////////

		iDam = calc_battle_damage(iAmount, chr->level(), pkChrVictim->level());

		if (chr->is_pc() && chr->skill_use_info_[sk->vnum].main_target_vid() != (uint32_t) pkChrVictim->get_vid())
		{
			// damage reduction
			iDam = (int) (iDam * sk->splash_around_damage_adjust_poly.eval());
		}

		// TODO need to record the damage type per skill.
		damage_type dt = DAMAGE_TYPE_NONE;

		switch (sk->skill_attr_type)
		{
			case SKILL_ATTR_TYPE_NORMAL:
				break;

			case SKILL_ATTR_TYPE_MELEE:
				{
					dt = DAMAGE_TYPE_MELEE;

					item * pkWeapon = chr->wear(WEAR_WEAPON);

					if (pkWeapon)
						switch (pkWeapon->sub_type())
						{
							case WEAPON_SWORD:
								iDam = iDam * (100 - pkChrVictim->point(POINT_RESIST_SWORD)) / 100;
								break;

							case WEAPON_TWO_HANDED:
								iDam = iDam * (100 - pkChrVictim->point(POINT_RESIST_TWOHAND)) / 100;
								// two-handed sword penalty 10%

								break;

							case WEAPON_DAGGER:
								iDam = iDam * (100 - pkChrVictim->point(POINT_RESIST_DAGGER)) / 100;
								break;

							case WEAPON_BELL:
								iDam = iDam * (100 - pkChrVictim->point(POINT_RESIST_BELL)) / 100;
								break;

							case WEAPON_FAN:
								iDam = iDam * (100 - pkChrVictim->point(POINT_RESIST_FAN)) / 100;
								break;
						}

					if (!bIgnoreDefense)
						iDam -= pkChrVictim->point(POINT_DEF_GRADE);
				}
				break;

			case SKILL_ATTR_TYPE_RANGE:
				dt = DAMAGE_TYPE_RANGE;
				// aaaaargh
				// there was an old bug where this wasn't applied; recalculating defense now would make users riot
				iDam = iDam * (100 - pkChrVictim->point(POINT_RESIST_BOW)) / 100;
				break;

			case SKILL_ATTR_TYPE_MAGIC:
				dt = DAMAGE_TYPE_MAGIC;
				iDam = calc_att_bonus(chr, pkChrVictim, iDam);
				// aaaaargh
				// there was an old bug where this wasn't applied; recalculating defense now would make users riot
				iDam = iDam * (100 - pkChrVictim->point(POINT_RESIST_MAGIC)) / 100;
				break;

			default:
				metin2::log::error("Unknown skill attr type {:d} vnum {:d}", sk->skill_attr_type, sk->vnum);
				break;
		}

		//
		// 20091109 Germany skill attribute request work
		// since no skill in the existing skill table had SKILL_FLAG_WIND, SKILL_FLAG_ELEC or SKILL_FLAG_FIRE,
		// monster RESIST_WIND, RESIST_ELEC and RESIST_FIRE were not used either.
		//
		// to separate PvP and PvE balance it intentionally applies only to NPCs, and so that the difference from
		// the existing balance isn't noticeable, mob_proto's RESIST_MAGIC was copied to RESIST_WIND, RESIST_ELEC, RESIST_FIRE
		//
		if (pkChrVictim->is_npc())
		{
			if (is_set(sk->flag, SKILL_FLAG_WIND))
			{
				iDam = iDam * (100 - pkChrVictim->point(POINT_RESIST_WIND)) / 100;
			}

			if (is_set(sk->flag, SKILL_FLAG_ELEC))
			{
				iDam = iDam * (100 - pkChrVictim->point(POINT_RESIST_ELEC)) / 100;
			}

			if (is_set(sk->flag, SKILL_FLAG_FIRE))
			{
				iDam = iDam * (100 - pkChrVictim->point(POINT_RESIST_FIRE)) / 100;
			}
		}

		if (is_set(sk->flag, SKILL_FLAG_COMPUTE_MAGIC_DAMAGE))
			dt = DAMAGE_TYPE_MAGIC;

		if (pkChrVictim->can_begin_fight())
			pkChrVictim->begin_fight(chr);

		if (sk->vnum == SKILL_CHAIN)
			metin2::log::info( "{} CHAIN INDEX {:d} DAM {:d} DT {:d}", chr->name(), chr->chain_lightning_index() - 1, iDam, dt);

		{
			uint8_t AntiSkillID = 0;

			switch (sk->vnum)
			{
				case SKILL_TANHWAN:		AntiSkillID = SKILL_7_A_ANTI_TANHWAN;		break;
				case SKILL_AMSEOP:		AntiSkillID = SKILL_7_B_ANTI_AMSEOP;		break;
				case SKILL_SWAERYUNG:	AntiSkillID = SKILL_7_C_ANTI_SWAERYUNG;		break;
				case SKILL_YONGBI:		AntiSkillID = SKILL_7_D_ANTI_YONGBI;		break;
				case SKILL_GIGONGCHAM:	AntiSkillID = SKILL_8_A_ANTI_GIGONGCHAM;	break;
				case SKILL_YEONSA:		AntiSkillID = SKILL_8_B_ANTI_YEONSA;		break;
				case SKILL_MAHWAN:		AntiSkillID = SKILL_8_C_ANTI_MAHWAN;		break;
				case SKILL_BYEURAK:		AntiSkillID = SKILL_8_D_ANTI_BYEURAK;		break;
			}

			if (0 != AntiSkillID)
			{
				uint8_t AntiSkillLevel = pkChrVictim->skill_level(AntiSkillID);

				if (0 != AntiSkillLevel)
				{
					skill_proto* pkSk = skill_manager::instance().get(AntiSkillID);
					if (!pkSk)
					{
						metin2::log::error("There is no anti skill({:d}) in skill proto", AntiSkillID);
					}
					else
					{
						pkSk->set_point_var("k", 1.0f * pkChrVictim->skill_power(AntiSkillID) * pkSk->max_level / 100);

						double ResistAmount = pkSk->point_poly.eval();

						metin2::log::info( "ANTI_SKILL: Resist({:f}) Orig({:d}) Reduce({:d})", ResistAmount, iDam, int(iDam * (ResistAmount/100.0)));

						iDam -= iDam * (ResistAmount/100.0);
					}
				}
			}
		}

		if (!pkChrVictim->damage(chr, iDam, dt) && !pkChrVictim->is_stun())
		{
			if (is_set(sk->flag, SKILL_FLAG_REMOVE_GOOD_AFFECT))
			{
				int iAmount2 = (int) sk->point_poly2.eval();
				int iDur2 = (int) sk->duration_poly2.eval();
				iDur2 += chr->point(POINT_PARTY_BUFFER_BONUS);

				if (number(1, 100) <= iAmount2)
				{
					pkChrVictim->remove_good_affect();
					pkChrVictim->add_affect(sk->vnum, POINT_NONE, 0, AFF_PABEOP, iDur2, 0, true);
				}
			}

			if (is_set(sk->flag, SKILL_FLAG_SLOW | SKILL_FLAG_STUN | SKILL_FLAG_FIRE_CONT | SKILL_FLAG_POISON))
			{
				int iPct = (int) sk->point_poly2.eval();
				int iDur = (int) sk->duration_poly2.eval();

				iDur += chr->point(POINT_PARTY_BUFFER_BONUS);

				if (is_set(sk->flag, SKILL_FLAG_STUN))
				{
					skill_attack_affect(pkChrVictim, iPct, IMMUNE_STUN, AFFECT_STUN, POINT_NONE, 0, AFF_STUN, iDur, sk->name);
				}
				else if (is_set(sk->flag, SKILL_FLAG_SLOW))
				{
					skill_attack_affect(pkChrVictim, iPct, IMMUNE_SLOW, AFFECT_SLOW, POINT_MOV_SPEED, -30, AFF_SLOW, iDur, sk->name);
				}
				else if (is_set(sk->flag, SKILL_FLAG_FIRE_CONT))
				{
					sk->set_duration_var("k", 1.0 * use_skill_power * sk->max_level / 100);
					sk->set_duration_var("iq", chr->point(POINT_IQ));

					iDur = (int)sk->duration_poly2.eval();
					int bonus = chr->point(POINT_PARTY_BUFFER_BONUS);

					if (bonus != 0)
					{
						iDur += bonus / 2;
					}

					if (number(1, 100) <= iDur)
					{
						pkChrVictim->attacked_by_fire(chr, iPct, 5);
					}
				}
				else if (is_set(sk->flag, SKILL_FLAG_POISON))
				{
					if (number(1, 100) <= iPct)
						pkChrVictim->attacked_by_poison(chr);
				}
			}

			if (is_set(sk->flag, SKILL_FLAG_CRUSH | SKILL_FLAG_CRUSH_LONG) &&
				!is_set(pkChrVictim->ai_flag(), AIFLAG_NOMOVE))
			{
				float fCrushSlidingLength = 200;

				if (chr->is_npc())
					fCrushSlidingLength = 400;

				if (is_set(sk->flag, SKILL_FLAG_CRUSH_LONG))
					fCrushSlidingLength *= 2;

				float fx, fy;
				float degree = get_degree_from_position_xy(chr->x(), chr->y(), pkChrVictim->x(), pkChrVictim->y());

				if (sk->vnum == SKILL_HORSE_WILDATTACK)
				{
					degree -= chr->rotation();
					degree = fmod(degree, 360.0f) - 180.0f;

					if (degree > 0)
						degree = chr->rotation() + 90.0f;
					else
						degree = chr->rotation() - 90.0f;
				}

				get_delta_by_degree(degree, fCrushSlidingLength, &fx, &fy);
				metin2::log::info( "CRUSH! {} -> {} ({:d} {:d}) -> ({:d} {:d})", chr->name(), pkChrVictim->name(), pkChrVictim->x(), pkChrVictim->y(), (int32_t)(pkChrVictim->x()+fx), (int32_t)(pkChrVictim->y()+fy));
				int32_t tx = (int32_t)(pkChrVictim->x()+fx);
				int32_t ty = (int32_t)(pkChrVictim->y()+fy);

				pkChrVictim->sync(tx, ty);
				pkChrVictim->go_to(tx, ty);
				pkChrVictim->calculate_move_duration();

				if (chr->is_pc() && chr->skill_use_info_[sk->vnum].main_target_vid() == (uint32_t) pkChrVictim->get_vid())
				{
					skill_attack_affect(pkChrVictim, 1000, IMMUNE_STUN, sk->vnum, POINT_NONE, 0, AFF_STUN, 4, sk->name);
				}
				else
				{
					pkChrVictim->sync_packet();
				}
			}
		}

		if (is_set(sk->flag, SKILL_FLAG_HP_ABSORB))
		{
			int iPct = (int) sk->point_poly2.eval();
			chr->point_change(POINT_HP, iDam * iPct / 100);
		}

		if (is_set(sk->flag, SKILL_FLAG_SP_ABSORB))
		{
			int iPct = (int) sk->point_poly2.eval();
			chr->point_change(POINT_SP, iDam * iPct / 100);
		}

		if (sk->vnum == SKILL_CHAIN && chr->chain_lightning_index() < chr->chain_lightning_max_count())
		{
			chain_lightning_event_info* info_value = alloc_event_info<chain_lightning_event_info>();

			info_value->victim = pkChrVictim->get_vid();
			info_value->chr = chr->get_vid();

			event_create(chain_lightning_event, info_value, metin2::config::server().passes_per_sec / 5);
		}
		if(metin2::config::test_server())
			metin2::log::info( "FuncSplashDamage End :{} ", chr->name());
	}

	int		x;
	int		y;
	skill_proto * sk;
	character *	chr;
	int		amount;
	int		count;
	int		max_hit;
	item *	weapon;
	bool disable_cooltime;
	skill_use_info* info;
	uint8_t use_skill_power;
};

struct func_splash_affect
{
	func_splash_affect(character * ch, int x_value, int y_value, int iDist, uint32_t dwVnum, uint8_t bPointOn, int iAmount, uint32_t dwAffectFlag, int iDuration, int iSPCost, bool bOverride, int iMaxHit)
	{
		x = x_value;
		y = y_value;
		dist = iDist;
		vnum = dwVnum;
		point_on = bPointOn;
		amount = iAmount;
		affect_flag = dwAffectFlag;
		duration = iDuration;
		sp_cost = iSPCost;
		override = bOverride;
		chr_attacker = ch;
		max_hit = iMaxHit;
		count = 0;
	}

	void operator () (entity * ent)
	{
		if (max_hit && max_hit <= count)
			return;

		if (ent->is_type(ENTITY_CHARACTER))
		{
			character * pkChr = (character *) ent;

			if (metin2::config::test_server())
				metin2::log::info( "FuncSplashAffect step 1 : name:{} vnum:{:d} iDur:{:d}", pkChr->name(), vnum, duration);
			if (distance_approx(x - pkChr->x(), y - pkChr->y()) < dist)
			{
				if (metin2::config::test_server())
					metin2::log::info( "FuncSplashAffect step 2 : name:{} vnum:{:d} iDur:{:d}", pkChr->name(), vnum, duration);
				if (vnum == SKILL_TUSOK)
					if (pkChr->can_begin_fight())
						pkChr->begin_fight(chr_attacker);

				if (pkChr->is_pc() && vnum == SKILL_TUSOK)
					pkChr->add_affect(vnum, point_on, amount, affect_flag, duration/3, sp_cost, override);
				else
					pkChr->add_affect(vnum, point_on, amount, affect_flag, duration, sp_cost, override);

				count ++;
			}
		}
	}

	character * chr_attacker;
	int		x;
	int		y;
	int		dist;
	uint32_t	vnum;
	uint8_t	point_on;
	int		amount;
	uint32_t	affect_flag;
	int		duration;
	int		sp_cost;
	bool	override;
	int         max_hit;
	int         count;
};

EVENTINFO(skill_gwihwan_info)
{
	uint32_t pid;
	uint8_t bsklv;

};

int character::compute_skill_at_position(uint32_t dwVnum, const pixel_position& posTarget, uint8_t bSkillLevel)
{
	if (get_mount_vnum())
		return BATTLE_NONE;

	if (is_polymorphed())
		return BATTLE_NONE;

	if (metin2::config::game().skill_disable)
		return BATTLE_NONE;

	skill_proto * pkSk = skill_manager::instance().get(dwVnum);

	if (!pkSk)
		return BATTLE_NONE;

	if (metin2::config::test_server())
	{
		metin2::log::info( "ComputeSkillAtPosition {} vnum {:d} x {:d} y {:d} level {:d}", 
				name(), dwVnum, posTarget.x, posTarget.y, bSkillLevel); 
	}

	// skills used on myself use my position.

	// a non-splash skill targeting the surroundings is odd
	if (!is_set(pkSk->flag, SKILL_FLAG_SPLASH))
		return BATTLE_NONE;

	if (0 == bSkillLevel)
	{
		if ((bSkillLevel = skill_level(pkSk->vnum)) == 0)
		{
			return BATTLE_NONE;
		}
	}

	const float k = 1.0 * skill_power(pkSk->vnum, bSkillLevel) * pkSk->max_level / 100;

	pkSk->set_point_var("k", k);
	pkSk->splash_around_damage_adjust_poly.set_var("k", k);

	if (is_set(pkSk->flag, SKILL_FLAG_USE_MELEE_DAMAGE))
	{
		pkSk->set_point_var("atk", calc_melee_damage(this, this, true, false));
	}
	else if (is_set(pkSk->flag, SKILL_FLAG_USE_MAGIC_DAMAGE))
	{
		pkSk->set_point_var("atk", calc_magic_damage(this, this));
	}
	else if (is_set(pkSk->flag, SKILL_FLAG_USE_ARROW_DAMAGE))
	{
		item * pkBow;
		item * pkArrow;
		if (1 == arrow_and_bow(&pkBow, &pkArrow, 1))
		{
			pkSk->set_point_var("atk", calc_arrow_damage(this, this, pkBow, pkArrow, true));
		}
		else
		{
			pkSk->set_point_var("atk", 0);
		}
	}

	if (pkSk->point_on == POINT_MOV_SPEED)
	{
		pkSk->set_point_var("maxv", this->limit_point(POINT_MOV_SPEED));
	}

	pkSk->set_point_var("lv", level());
	pkSk->set_point_var("iq", point(POINT_IQ));
	pkSk->set_point_var("str", point(POINT_ST));
	pkSk->set_point_var("dex", point(POINT_DX));
	pkSk->set_point_var("con", point(POINT_HT));
	pkSk->set_point_var("maxhp", this->max_hp());
	pkSk->set_point_var("maxsp", this->max_sp());
	pkSk->set_point_var("chain", 0);
	pkSk->set_point_var("ar", calc_attack_rating(this, this));
	pkSk->set_point_var("def", point(POINT_DEF_GRADE));
	pkSk->set_point_var("odef", point(POINT_DEF_GRADE) - point(POINT_DEF_GRADE_BONUS));
	pkSk->set_point_var("horse_level", horse_level());

	if (pkSk->skill_attr_type != SKILL_ATTR_TYPE_NORMAL)
		on_move(true);

	item * pkWeapon = wear(WEAR_WEAPON);

	set_poly_var_for_attack(this, pkSk, pkWeapon);

	pkSk->set_duration_var("k", k/*bSkillLevel*/);

	int iAmount = (int) pkSk->point_poly.eval();
	int iAmount2 = (int) pkSk->point_poly2.eval();

	// ADD_GRANDMASTER_SKILL
	int iAmount3 = (int) pkSk->point_poly3.eval();

	if (used_skill_master_type(pkSk->vnum) >= SKILL_GRAND_MASTER)
	{
		iAmount = (int) pkSk->master_bonus_poly.eval();
	}

	if (metin2::config::test_server() && iAmount == 0 && pkSk->point_on != POINT_NONE)
	{
		chat_packet(CHAT_TYPE_INFO, "효과가 없습니다. 스킬 공식을 확인하세요");
	}

	if (is_set(pkSk->flag, SKILL_FLAG_REMOVE_BAD_AFFECT))
	{
		if (number(1, 100) <= iAmount2)
		{
			remove_bad_affect();
		}
	}
	// END_OF_ADD_GRANDMASTER_SKILL

	if (is_set(pkSk->flag, SKILL_FLAG_ATTACK | SKILL_FLAG_USE_MELEE_DAMAGE | SKILL_FLAG_USE_MAGIC_DAMAGE))
	{
		//
		// for attack skills
		//
		bool bAdded = false;

		if (pkSk->point_on == POINT_HP && iAmount < 0)
		{
			int iAG = 0;

			func_splash_damage f(posTarget.x, posTarget.y, pkSk, this, iAmount, iAG, pkSk->max_hit, pkWeapon, disable_cooltime_, is_pc()?&skill_use_info_[dwVnum]:nullptr, skill_power(dwVnum, bSkillLevel));

			if (is_set(pkSk->flag, SKILL_FLAG_SPLASH))
			{
				if (get_sectree())
					get_sectree()->for_each_around(f);
			}
			else
			{
				f(this);
			}
		}
		else
		{
			int iDur = (int) pkSk->duration_poly.eval();

			if (is_pc())
				if (!(dwVnum >= GUILD_SKILL_START && dwVnum <= GUILD_SKILL_END)) // guild skills have no cooldown handling.
					if (!disable_cooltime_ && !skill_use_info_[dwVnum].hit_once(dwVnum) && dwVnum != SKILL_MUYEONG)
					{
						return BATTLE_NONE;
					}

			if (iDur > 0)
			{
				iDur += point(POINT_PARTY_BUFFER_BONUS);

				if (!is_set(pkSk->flag, SKILL_FLAG_SPLASH))
					add_affect(pkSk->vnum, pkSk->point_on, iAmount, pkSk->affect_flag, iDur, 0, true);
				else
				{
					if (get_sectree())
					{
						func_splash_affect f(this, posTarget.x, posTarget.y, pkSk->splash_range, pkSk->vnum, pkSk->point_on, iAmount, pkSk->affect_flag, iDur, 0, true, pkSk->max_hit);
						get_sectree()->for_each_around(f);
					}
				}
				bAdded = true;
			}
		}

		if (pkSk->point_on2 != POINT_NONE)
		{
			int iDur = (int) pkSk->duration_poly2.eval();

			metin2::log::debug( "try second {:d} {:d} {:d}", pkSk->vnum, pkSk->point_on2, iDur);

			if (iDur > 0)
			{
				iDur += point(POINT_PARTY_BUFFER_BONUS);

				if (!is_set(pkSk->flag, SKILL_FLAG_SPLASH))
					add_affect(pkSk->vnum, pkSk->point_on2, iAmount2, pkSk->affect_flag2, iDur, 0, !bAdded);
				else
				{
					if (get_sectree())
					{
						func_splash_affect f(this, posTarget.x, posTarget.y, pkSk->splash_range, pkSk->vnum, pkSk->point_on2, iAmount2, pkSk->affect_flag2, iDur, 0, !bAdded, pkSk->max_hit);
						get_sectree()->for_each_around(f);
					}
				}
				bAdded = true;
			}
			else
			{
				point_change(pkSk->point_on2, iAmount2);
			}
		}

		// ADD_GRANDMASTER_SKILL
		if (used_skill_master_type(pkSk->vnum) >= SKILL_GRAND_MASTER && pkSk->point_on3 != POINT_NONE)
		{
			int iDur = (int) pkSk->duration_poly3.eval();

			if (iDur > 0)
			{
				iDur += point(POINT_PARTY_BUFFER_BONUS);

				if (!is_set(pkSk->flag, SKILL_FLAG_SPLASH))
					add_affect(pkSk->vnum, pkSk->point_on3, iAmount3, 0 /*pkSk->dwAffectFlag3*/, iDur, 0, !bAdded);
				else
				{
					if (get_sectree())
					{
						func_splash_affect f(this, posTarget.x, posTarget.y, pkSk->splash_range, pkSk->vnum, pkSk->point_on3, iAmount3, 0 /*pkSk->dwAffectFlag3*/, iDur, 0, !bAdded, pkSk->max_hit);
						get_sectree()->for_each_around(f);
					}
				}
			}
			else
			{
				point_change(pkSk->point_on3, iAmount3);
			}
		}
		// END_OF_ADD_GRANDMASTER_SKILL

		return BATTLE_DAMAGE;
	}
	else
	{
		bool bAdded = false;
		int iDur = (int) pkSk->duration_poly.eval();

		if (iDur > 0)
		{
			iDur += point(POINT_PARTY_BUFFER_BONUS);
			// if there is no AffectFlag, or it's not a toggle..
			pkSk->duration_sp_cost_poly.set_var("k", k/*bSkillLevel*/);

			add_affect(pkSk->vnum,
					  pkSk->point_on,
					  iAmount,
					  pkSk->affect_flag,
					  iDur,
					  (int32_t) pkSk->duration_sp_cost_poly.eval(),
					  !bAdded);

			bAdded = true;
		}
		else
		{
			point_change(pkSk->point_on, iAmount);
		}

		if (pkSk->point_on2 != POINT_NONE)
		{
			int iDur = (int) pkSk->duration_poly2.eval();

			if (iDur > 0)
			{
				iDur += point(POINT_PARTY_BUFFER_BONUS);
				add_affect(pkSk->vnum, pkSk->point_on2, iAmount2, pkSk->affect_flag2, iDur, 0, !bAdded);
				bAdded = true;
			}
			else
			{
				point_change(pkSk->point_on2, iAmount2);
			}
		}

		// ADD_GRANDMASTER_SKILL
		if (used_skill_master_type(pkSk->vnum) >= SKILL_GRAND_MASTER && pkSk->point_on3 != POINT_NONE)
		{
			int iDur = (int) pkSk->duration_poly3.eval();

			if (iDur > 0)
			{
				iDur += point(POINT_PARTY_BUFFER_BONUS);
				add_affect(pkSk->vnum, pkSk->point_on3, iAmount3, 0 /*pkSk->dwAffectFlag3*/, iDur, 0, !bAdded);
			}
			else
			{
				point_change(pkSk->point_on3, iAmount3);
			}
		}
		// END_OF_ADD_GRANDMASTER_SKILL

		return BATTLE_NONE;
	}
}

// if bSkillLevel is non-zero, m_abSkillLevels is not used and it is forcibly
// calculated with bSkillLevel.
int character::compute_skill(uint32_t dwVnum, character * pkVictim, uint8_t bSkillLevel)
{
	const bool bCanUseHorseSkill = can_use_horse_skill();

	// return if riding a horse but skills can't be used
	if (false == bCanUseHorseSkill && true == is_riding())
		return BATTLE_NONE;

	if (is_polymorphed())
		return BATTLE_NONE;

	if (metin2::config::game().skill_disable)
		return BATTLE_NONE;

	skill_proto* pkSk = skill_manager::instance().get(dwVnum);

	if (!pkSk)
		return BATTLE_NONE;

	if (bCanUseHorseSkill && pkSk->type != SKILL_TYPE_HORSE)
		return BATTLE_NONE;

	if (!bCanUseHorseSkill && pkSk->type == SKILL_TYPE_HORSE)
		return BATTLE_NONE;

	// if not used on the target, it must be used on myself.
	if (is_set(pkSk->flag, SKILL_FLAG_SELFONLY))
		pkVictim = this;

	if (!pkVictim)
	{
		if (metin2::config::test_server())
			metin2::log::info( "ComputeSkill: {} Victim == null, skill {:d}", name(), dwVnum);

		return BATTLE_NONE;
	}

	if (pkSk->target_range && distance_sqrt(x() - pkVictim->x(), y() - pkVictim->y()) >= pkSk->target_range + 50)
	{
		if (metin2::config::test_server())
			metin2::log::info( "ComputeSkill: Victim too far, skill {:d} : {} to {} (distance {:d} limit {:d})", 
					dwVnum,
					name(),
					pkVictim->name(),
					(int32_t)distance_sqrt(x() - pkVictim->x(), y() - pkVictim->y()),
					pkSk->target_range);

		return BATTLE_NONE;
	}

	if (0 == bSkillLevel)
	{
		if ((bSkillLevel = skill_level(pkSk->vnum)) == 0)
		{
			if (metin2::config::test_server())
				metin2::log::info( "ComputeSkill : name:{} vnum:{:d}  skillLevelBySkill : {:d} ", name(), pkSk->vnum, bSkillLevel);
			return BATTLE_NONE;
		}
	}

	if (pkVictim->is_affect_flag(AFF_PABEOP) && pkVictim->is_good_affect(dwVnum))
	{
		return BATTLE_NONE;
	}

	const float k = 1.0 * skill_power(pkSk->vnum, bSkillLevel) * pkSk->max_level / 100;

	pkSk->set_point_var("k", k);
	pkSk->splash_around_damage_adjust_poly.set_var("k", k);

	if (pkSk->type == SKILL_TYPE_HORSE)
	{
		item * pkBow;
		item * pkArrow;
		if (1 == arrow_and_bow(&pkBow, &pkArrow, 1))
		{
			pkSk->set_point_var("atk", calc_arrow_damage(this, pkVictim, pkBow, pkArrow, true));
		}
		else
		{
			pkSk->set_point_var("atk", calc_melee_damage(this, pkVictim, true, false));
		}
	}
	else if (is_set(pkSk->flag, SKILL_FLAG_USE_MELEE_DAMAGE))
	{
		pkSk->set_point_var("atk", calc_melee_damage(this, pkVictim, true, false));
	}
	else if (is_set(pkSk->flag, SKILL_FLAG_USE_MAGIC_DAMAGE))
	{
		pkSk->set_point_var("atk", calc_magic_damage(this, pkVictim));
	}
	else if (is_set(pkSk->flag, SKILL_FLAG_USE_ARROW_DAMAGE))
	{
		item * pkBow;
		item * pkArrow;
		if (1 == arrow_and_bow(&pkBow, &pkArrow, 1))
		{
			pkSk->set_point_var("atk", calc_arrow_damage(this, pkVictim, pkBow, pkArrow, true));
		}
		else
		{
			pkSk->set_point_var("atk", 0);
		}
	}

	if (pkSk->point_on == POINT_MOV_SPEED)
	{
		pkSk->set_point_var("maxv", pkVictim->limit_point(POINT_MOV_SPEED));
	}

	pkSk->set_point_var("lv", level());
	pkSk->set_point_var("iq", point(POINT_IQ));
	pkSk->set_point_var("str", point(POINT_ST));
	pkSk->set_point_var("dex", point(POINT_DX));
	pkSk->set_point_var("con", point(POINT_HT));
	pkSk->set_point_var("maxhp", pkVictim->max_hp());
	pkSk->set_point_var("maxsp", pkVictim->max_sp());
	pkSk->set_point_var("chain", 0);
	pkSk->set_point_var("ar", calc_attack_rating(this, pkVictim));
	pkSk->set_point_var("def", point(POINT_DEF_GRADE));
	pkSk->set_point_var("odef", point(POINT_DEF_GRADE) - point(POINT_DEF_GRADE_BONUS));
	pkSk->set_point_var("horse_level", horse_level());

	if (pkSk->skill_attr_type != SKILL_ATTR_TYPE_NORMAL)
		on_move(true);

	item * pkWeapon = wear(WEAR_WEAPON);

	set_poly_var_for_attack(this, pkSk, pkWeapon);

	pkSk->duration_poly.set_var("k", k/*bSkillLevel*/);
	pkSk->duration_poly2.set_var("k", k/*bSkillLevel*/);

	int iAmount = (int) pkSk->point_poly.eval();
	int iAmount2 = (int) pkSk->point_poly2.eval();
	int iAmount3 = (int) pkSk->point_poly3.eval();

	if (metin2::config::test_server() && is_pc())
		metin2::log::info( "iAmount: {:d} {:d} {:d} , atk:{:f} skLevel:{:f} k:{:f} GetSkillPower({:d}) MaxLevel:{:d} Per:{:d}",
				iAmount, iAmount2, iAmount3,
				pkSk->point_poly.var("atk"),
				pkSk->point_poly.var("k"),
				k,
				skill_power(pkSk->vnum, bSkillLevel),
				pkSk->max_level,
				pkSk->max_level/100
				);

	// ADD_GRANDMASTER_SKILL
	if (used_skill_master_type(pkSk->vnum) >= SKILL_GRAND_MASTER)
	{
		iAmount = (int) pkSk->master_bonus_poly.eval();
	}

	if (metin2::config::test_server() && iAmount == 0 && pkSk->point_on != POINT_NONE)
	{
		chat_packet(CHAT_TYPE_INFO, "효과가 없습니다. 스킬 공식을 확인하세요");
	}
	// END_OF_ADD_GRANDMASTER_SKILL

	// REMOVE_BAD_AFFECT_BUG_FIX
	if (is_set(pkSk->flag, SKILL_FLAG_REMOVE_BAD_AFFECT))
	{
		if (number(1, 100) <= iAmount2)
		{
			pkVictim->remove_bad_affect();
		}
	}
	// END_OF_REMOVE_BAD_AFFECT_BUG_FIX

	if (is_set(pkSk->flag, SKILL_FLAG_ATTACK | SKILL_FLAG_USE_MELEE_DAMAGE | SKILL_FLAG_USE_MAGIC_DAMAGE) &&
		!(pkSk->vnum == SKILL_MUYEONG && pkVictim == this) && !(pkSk->is_charge_skill() && pkVictim == this))
	{
		bool bAdded = false;

		if (pkSk->point_on == POINT_HP && iAmount < 0)
		{
			int iAG = 0;

			func_splash_damage f(pkVictim->x(), pkVictim->y(), pkSk, this, iAmount, iAG, pkSk->max_hit, pkWeapon, disable_cooltime_, is_pc()?&skill_use_info_[dwVnum]:nullptr, skill_power(dwVnum, bSkillLevel));
			if (is_set(pkSk->flag, SKILL_FLAG_SPLASH))
			{
				if (pkVictim->get_sectree())
					pkVictim->get_sectree()->for_each_around(f);
			}
			else
			{
				f(pkVictim);
			}
		}
		else
		{
			pkSk->duration_poly.set_var("k", k/*bSkillLevel*/);
			int iDur = (int) pkSk->duration_poly.eval();

			if (is_pc())
				if (!(dwVnum >= GUILD_SKILL_START && dwVnum <= GUILD_SKILL_END)) // guild skills have no cooldown handling.
					if (!disable_cooltime_ && !skill_use_info_[dwVnum].hit_once(dwVnum) && dwVnum != SKILL_MUYEONG)
					{
						return BATTLE_NONE;
					}

			if (iDur > 0)
			{
				iDur += point(POINT_PARTY_BUFFER_BONUS);

				if (!is_set(pkSk->flag, SKILL_FLAG_SPLASH))
					pkVictim->add_affect(pkSk->vnum, pkSk->point_on, iAmount, pkSk->affect_flag, iDur, 0, true);
				else
				{
					if (pkVictim->get_sectree())
					{
						func_splash_affect f(this, pkVictim->x(), pkVictim->y(), pkSk->splash_range, pkSk->vnum, pkSk->point_on, iAmount, pkSk->affect_flag, iDur, 0, true, pkSk->max_hit);
						pkVictim->get_sectree()->for_each_around(f);
					}
				}
				bAdded = true;
			}
		}

		if (pkSk->point_on2 != POINT_NONE && !pkSk->is_charge_skill())
		{
			pkSk->duration_poly2.set_var("k", k/*bSkillLevel*/);
			int iDur = (int) pkSk->duration_poly2.eval();

			if (iDur > 0)
			{
				iDur += point(POINT_PARTY_BUFFER_BONUS);

				if (!is_set(pkSk->flag, SKILL_FLAG_SPLASH))
					pkVictim->add_affect(pkSk->vnum, pkSk->point_on2, iAmount2, pkSk->affect_flag2, iDur, 0, !bAdded);
				else
				{
					if (pkVictim->get_sectree())
					{
						func_splash_affect f(this, pkVictim->x(), pkVictim->y(), pkSk->splash_range, pkSk->vnum, pkSk->point_on2, iAmount2, pkSk->affect_flag2, iDur, 0, !bAdded, pkSk->max_hit);
						pkVictim->get_sectree()->for_each_around(f);
					}
				}

				bAdded = true;
			}
			else
			{
				pkVictim->point_change(pkSk->point_on2, iAmount2);
			}
		}

		// ADD_GRANDMASTER_SKILL
		if (pkSk->point_on3 != POINT_NONE && !pkSk->is_charge_skill() && used_skill_master_type(pkSk->vnum) >= SKILL_GRAND_MASTER)
		{
			pkSk->duration_poly3.set_var("k", k/*bSkillLevel*/);
			int iDur = (int) pkSk->duration_poly3.eval();

			if (iDur > 0)
			{
				iDur += point(POINT_PARTY_BUFFER_BONUS);

				if (!is_set(pkSk->flag, SKILL_FLAG_SPLASH))
					pkVictim->add_affect(pkSk->vnum, pkSk->point_on3, iAmount3, /*pkSk->dwAffectFlag3*/ 0, iDur, 0, !bAdded);
				else
				{
					if (pkVictim->get_sectree())
					{
						func_splash_affect f(this, pkVictim->x(), pkVictim->y(), pkSk->splash_range, pkSk->vnum, pkSk->point_on3, iAmount3, /*pkSk->dwAffectFlag3*/ 0, iDur, 0, !bAdded, pkSk->max_hit);
						pkVictim->get_sectree()->for_each_around(f);
					}
				}

				bAdded = true;
			}
			else
			{
				pkVictim->point_change(pkSk->point_on3, iAmount3);
			}
		}
		// END_OF_ADD_GRANDMASTER_SKILL

		return BATTLE_DAMAGE;
	}
	else
	{
		if (dwVnum == SKILL_MUYEONG)
		{
			pkSk->duration_poly.set_var("k", k/*bSkillLevel*/);
			pkSk->duration_sp_cost_poly.set_var("k", k/*bSkillLevel*/);

			int iDur = (int32_t) pkSk->duration_poly.eval();
			iDur += point(POINT_PARTY_BUFFER_BONUS);

			if (pkVictim == this)
				add_affect(dwVnum,
						POINT_NONE, 0,
						AFF_MUYEONG, 
						iDur,
						(int32_t) pkSk->duration_sp_cost_poly.eval(),
						true);

			return BATTLE_NONE;
		}

		bool bAdded = false;
		pkSk->duration_poly.set_var("k", k/*bSkillLevel*/);
		int iDur = (int) pkSk->duration_poly.eval();

		if (iDur > 0)
		{
			iDur += point(POINT_PARTY_BUFFER_BONUS);
			// if there is no AffectFlag, or it's not a toggle..
			pkSk->duration_sp_cost_poly.set_var("k", k/*bSkillLevel*/);

			if (pkSk->point_on2 != POINT_NONE)
			{
				pkVictim->remove_affect(pkSk->vnum);

				int iDur2 = (int) pkSk->duration_poly2.eval();

				if (iDur2 > 0)
				{
					if (metin2::config::test_server())
						metin2::log::info( "SKILL_AFFECT: {} {} Dur:{:d} To:{:d} Amount:{:d}", 
								name(),
								pkSk->name,
								iDur2,
								pkSk->point_on2,
								iAmount2);

					iDur2 += point(POINT_PARTY_BUFFER_BONUS);
					pkVictim->add_affect(pkSk->vnum, pkSk->point_on2, iAmount2, pkSk->affect_flag2, iDur2, 0, false);
				}
				else
				{
					pkVictim->point_change(pkSk->point_on2, iAmount2);
				}

				uint32_t affact_flag = pkSk->affect_flag;

				// ADD_GRANDMASTER_SKILL
				if ((pkSk->vnum == SKILL_CHUNKEON && used_skill_master_type(pkSk->vnum) < SKILL_GRAND_MASTER))
					affact_flag = AFF_CHEONGEUN_WITH_FALL;
				// END_OF_ADD_GRANDMASTER_SKILL

				pkVictim->add_affect(pkSk->vnum,
						pkSk->point_on,
						iAmount,
						affact_flag,
						iDur,
						(int32_t) pkSk->duration_sp_cost_poly.eval(),
						false);
			}
			else
			{
				if (metin2::config::test_server())
					metin2::log::info( "SKILL_AFFECT: {} {} Dur:{:d} To:{:d} Amount:{:d}", 
							name(),
							pkSk->name,
							iDur,
							pkSk->point_on,
							iAmount);

				pkVictim->add_affect(pkSk->vnum,
						pkSk->point_on,
						iAmount,
						pkSk->affect_flag,
						iDur,
						(int32_t) pkSk->duration_sp_cost_poly.eval(),
						// ADD_GRANDMASTER_SKILL
						!bAdded);
				// END_OF_ADD_GRANDMASTER_SKILL
			}

			bAdded = true;
		}
		else
		{
			if (!pkSk->is_charge_skill())
				pkVictim->point_change(pkSk->point_on, iAmount);

			if (pkSk->point_on2 != POINT_NONE)
			{
				pkVictim->remove_affect(pkSk->vnum);

				int iDur2 = (int) pkSk->duration_poly2.eval();

				if (iDur2 > 0)
				{
					iDur2 += point(POINT_PARTY_BUFFER_BONUS);

					if (pkSk->is_charge_skill())
						pkVictim->add_affect(pkSk->vnum, pkSk->point_on2, iAmount2, AFF_TANHWAN_DASH, iDur2, 0, false);
					else
						pkVictim->add_affect(pkSk->vnum, pkSk->point_on2, iAmount2, pkSk->affect_flag2, iDur2, 0, false);
				}
				else
				{
					pkVictim->point_change(pkSk->point_on2, iAmount2);
				}

			}
		}

		// ADD_GRANDMASTER_SKILL
		if (pkSk->point_on3 != POINT_NONE && !pkSk->is_charge_skill() && used_skill_master_type(pkSk->vnum) >= SKILL_GRAND_MASTER)
		{

			pkSk->duration_poly3.set_var("k", k/*bSkillLevel*/);
			int iDur = (int) pkSk->duration_poly3.eval();

			metin2::log::info( "try third {:d} {:d} {:d} {:d} 1894", pkSk->vnum, pkSk->point_on3, iDur, iAmount3);

			if (iDur > 0)
			{
				iDur += point(POINT_PARTY_BUFFER_BONUS);

				if (!is_set(pkSk->flag, SKILL_FLAG_SPLASH))
					pkVictim->add_affect(pkSk->vnum, pkSk->point_on3, iAmount3, /*pkSk->dwAffectFlag3*/ 0, iDur, 0, !bAdded);
				else
				{
					if (pkVictim->get_sectree())
					{
						func_splash_affect f(this, pkVictim->x(), pkVictim->y(), pkSk->splash_range, pkSk->vnum, pkSk->point_on3, iAmount3, /*pkSk->dwAffectFlag3*/ 0, iDur, 0, !bAdded, pkSk->max_hit);
						pkVictim->get_sectree()->for_each_around(f);
					}
				}

				bAdded = true;
			}
			else
			{
				pkVictim->point_change(pkSk->point_on3, iAmount3);
			}
		}
		// END_OF_ADD_GRANDMASTER_SKILL

		return BATTLE_NONE;
	}
}

bool character::use_skill(uint32_t dwVnum, character * pkVictim, bool bUseGrandMaster)
{
	if (false == can_use_skill(dwVnum))
		return false;

	// NO_GRANDMASTER
	if (metin2::config::test_server())
	{
		if (quest::quest_manager::instance().event_flag("no_grand_master"))
		{
			bUseGrandMaster = false;
		}
	}
	// END_OF_NO_GRANDMASTER

	if (metin2::config::game().skill_disable)
		return false;

	if (is_observer_mode())
		return false;

	if (!can_move())
		return false;

	if (is_polymorphed())
		return false;

	const bool bCanUseHorseSkill = can_use_horse_skill();

	if (dwVnum == SKILL_HORSE_SUMMON)
	{
		if (skill_level(dwVnum) == 0)
			return false;

		if (horse_level() <= 0)
			chat_packet(CHAT_TYPE_INFO, lc_text("말이 없습니다. 마굿간 경비병을 찾아가세요."));
		else
			chat_packet(CHAT_TYPE_INFO, lc_text("말 소환 아이템을 사용하세요."));

		return true;
	}

	// return false if riding a horse but skills can't be used
	if (false == bCanUseHorseSkill && true == is_riding())
		return false;

	skill_proto * pkSk = skill_manager::instance().get(dwVnum);
	metin2::log::info( "{}: USE_SKILL: {:d} pkVictim {}", name(), dwVnum, static_cast<const void *>(pkVictim));

	if (!pkSk)
		return false;

	if (bCanUseHorseSkill && pkSk->type != SKILL_TYPE_HORSE)
		return BATTLE_NONE;

	if (!bCanUseHorseSkill && pkSk->type == SKILL_TYPE_HORSE)
		return BATTLE_NONE;

	if (skill_level(dwVnum) == 0)
		return false;

	// NO_GRANDMASTER
	if (skill_master_type(dwVnum) < SKILL_GRAND_MASTER)
		bUseGrandMaster = false;
	// END_OF_NO_GRANDMASTER

	// MINING
	if (wear(WEAR_WEAPON) && (wear(WEAR_WEAPON)->type() == ITEM_ROD || wear(WEAR_WEAPON)->type() == ITEM_PICK))
		return false;
	// END_OF_MINING

	skill_use_info_[dwVnum].target_vid_map.clear();

	if (pkSk->is_charge_skill())
	{
		if (is_affect_flag(AFF_TANHWAN_DASH) || (pkVictim && pkVictim != this))
		{
			if (!pkVictim)
				return false;

			if (!is_affect_flag(AFF_TANHWAN_DASH))
			{
				if (!use_skill(dwVnum, this))
					return false;
			}

			skill_use_info_[dwVnum].set_main_target_vid(pkVictim->get_vid());
			// Tanhwan-gyeok in DASH state is an attack skill
			compute_skill(dwVnum, pkVictim);
			remove_affect(dwVnum);
			return true;
		}
	}

	if (dwVnum == SKILL_COMBO)
	{
		if (combo_index_)
			combo_index_ = 0;
		else
			combo_index_ = skill_level(SKILL_COMBO);

		chat_packet(CHAT_TYPE_COMMAND, "combo %d", combo_index_);
		return true;
	}

	// no SP used when toggling (distinguished by SelfOnly)
	if ((0 != pkSk->affect_flag || pkSk->vnum == SKILL_MUYEONG) && (pkSk->flag & SKILL_FLAG_TOGGLE) && remove_affect(pkSk->vnum))
	{
		return true;
	}

	if (is_affect_flag(AFF_REVIVE_INVISIBLE))
		remove_affect(AFFECT_REVIVE_INVISIBLE);

	const float k = 1.0 * skill_power(pkSk->vnum) * pkSk->max_level / 100;

	pkSk->set_point_var("k", k);
	pkSk->splash_around_damage_adjust_poly.set_var("k", k);

	// cooldown check
	pkSk->cooldown_poly.set_var("k", k);
	int iCooltime = (int) pkSk->cooldown_poly.eval();
	int lMaxHit = pkSk->max_hit ? pkSk->max_hit : -1;

	pkSk->set_sp_cost_var("k", k);

	uint32_t dwCur = get_dword_time();

	if (dwVnum == SKILL_TERROR && skill_use_info_[dwVnum].used && skill_use_info_[dwVnum].next_skill_usable_time > dwCur )
	{
		metin2::log::info( " SKILL_TERROR's Cooltime is not delta over {:d}", skill_use_info_[dwVnum].next_skill_usable_time  - dwCur );
		return false;
	}

	int iNeededSP = 0;

	if (is_set(pkSk->flag, SKILL_FLAG_USE_HP_AS_COST))
	{
		pkSk->set_sp_cost_var("maxhp", max_hp());
		pkSk->set_sp_cost_var("v", hp());
		iNeededSP = (int) pkSk->sp_cost_poly.eval();

		// ADD_GRANDMASTER_SKILL
		if (skill_master_type(dwVnum) >= SKILL_GRAND_MASTER && bUseGrandMaster)
		{
			iNeededSP = (int) pkSk->grand_master_add_sp_cost_poly.eval();
		}
		// END_OF_ADD_GRANDMASTER_SKILL	

		if (hp() < iNeededSP)
			return false;

		point_change(POINT_HP, -iNeededSP);
	}
	else
	{
		// SKILL_FOMULA_REFACTORING
		pkSk->set_sp_cost_var("maxhp", max_hp());
		pkSk->set_sp_cost_var("maxv", max_sp());
		pkSk->set_sp_cost_var("v", sp());

		iNeededSP = (int) pkSk->sp_cost_poly.eval();

		if (skill_master_type(dwVnum) >= SKILL_GRAND_MASTER && bUseGrandMaster)
		{
			iNeededSP = (int) pkSk->grand_master_add_sp_cost_poly.eval();
		}
		// END_OF_SKILL_FOMULA_REFACTORING

		if (sp() < iNeededSP)
			return false;

		if (metin2::config::test_server())
			chat_packet(CHAT_TYPE_INFO, lc_text("%s SP소모: %d"), pkSk->name, iNeededSP);

		point_change(POINT_SP, -iNeededSP);
	}

	if (is_set(pkSk->flag, SKILL_FLAG_SELFONLY))
		pkVictim = this;

	if (pkSk->vnum == SKILL_MUYEONG || (pkSk->is_charge_skill() && !is_affect_flag(AFF_TANHWAN_DASH) && !pkVictim))
	{
		// Muyeongjin on first use attaches an affect to self.
		pkVictim = this;
	}

	int iSplashCount = 1;

	if (false == disable_cooltime_)
	{
		if (false == 
				skill_use_info_[dwVnum].use_skill(
					bUseGrandMaster,
				   	(nullptr != pkVictim && SKILL_HORSE_WILDATTACK != dwVnum) ? pkVictim->get_vid() : 0,
				   	compute_cooltime(iCooltime * 1000),
				   	iSplashCount,
				   	lMaxHit))
		{
			if (metin2::config::test_server())
				chat_packet(CHAT_TYPE_NOTICE, "cooltime not finished %s %d", pkSk->name, iCooltime);

			return false;
		}
	}

	if (dwVnum == SKILL_CHAIN)
	{
		reset_chain_lightning_index();
		add_chain_lightning_except(pkVictim);
	}

	if (is_set(pkSk->flag, SKILL_FLAG_SELFONLY))
		compute_skill(dwVnum, this);
	else if (!is_set(pkSk->flag, SKILL_FLAG_ATTACK))
		compute_skill(dwVnum, pkVictim);
	else if (dwVnum == SKILL_BYEURAK)
		compute_skill(dwVnum, pkVictim);
	else if (dwVnum == SKILL_MUYEONG || pkSk->is_charge_skill())
		compute_skill(dwVnum, pkVictim);

	last_skill_time_ = get_dword_time();

	return true;
}

int character::used_skill_master_type(uint32_t dwVnum)
{
	const skill_use_info& rInfo = skill_use_info_[dwVnum];

	if (skill_master_type(dwVnum) < SKILL_GRAND_MASTER)
		return skill_master_type(dwVnum);

	if (rInfo.is_grand_master)
		return skill_master_type(dwVnum);

	return std::min<int>(skill_master_type(dwVnum), SKILL_MASTER);
}

int character::skill_master_type(uint32_t dwVnum) const
{
	if (!is_pc())
		return 0;

	if (dwVnum >= SKILL_MAX_NUM)
	{
		metin2::log::error("{} skill vnum overflow {:d}", name(), dwVnum);
		return 0;
	}

	return skill_levels_ ? skill_levels_[dwVnum].master_type : static_cast<uint8_t>(SKILL_NORMAL);
}

int character::skill_power(uint32_t dwVnum, uint8_t bLevel) const
{
	// mermaid ring item
	if (dwVnum >= SKILL_LANGUAGE1 && dwVnum <= SKILL_LANGUAGE3 && is_equip_unique_group(UNIQUE_GROUP_RING_OF_LANGUAGE))
	{
		return 100;
	}

	if (dwVnum >= GUILD_SKILL_START && dwVnum <= GUILD_SKILL_END)
	{
		if (get_guild())
			return 100 * get_guild()->skill_level(dwVnum) / 7 / 7;
		else
			return 0;
	}

	if (bLevel)
	{
		//SKILL_POWER_BY_LEVEL
		return skill_power_by_level(bLevel, true);
		//END_SKILL_POWER_BY_LEVEL;
	}

	if (dwVnum >= SKILL_MAX_NUM)
	{
		metin2::log::error("{} skill vnum overflow {:d}", name(), dwVnum);
		return 0;
	}

	//SKILL_POWER_BY_LEVEL
	return skill_power_by_level(skill_level(dwVnum));
	//SKILL_POWER_BY_LEVEL
}

int character::skill_level(uint32_t dwVnum) const
{
	if (dwVnum >= SKILL_MAX_NUM)
	{
		metin2::log::error("{} skill vnum overflow {:d}", name(), dwVnum);
		metin2::log::info( "{} skill vnum overflow {:d}", name(), dwVnum);
		return 0;
	}

	return std::min(SKILL_MAX_LEVEL, skill_levels_ ? skill_levels_[dwVnum].level : 0);
}

EVENTFUNC(skill_muyoung_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "skill_muyoung_event> <Factor> Null pointer" );
		return 0;
	}

	character *	ch = info->ch;

	if (ch == nullptr) { // <Factor>
		return 0;
	}

	if (!ch->is_affect_flag(AFF_MUYEONG))
	{
		ch->stop_muyeong_event();
		return 0;
	}

	// 1. Find Victim
	f_find_near_victim f(ch, ch);
	if (ch->get_sectree())
	{
		ch->get_sectree()->for_each_around(f);
		// 2. Shoot!
		if (f.victim())
		{
			ch->create_fly(FLY_SKILL_MUYEONG, f.victim());
			ch->compute_skill(SKILL_MUYEONG, f.victim());
		}
	}

	return passes_per_sec(3);
}

void character::start_muyeong_event()
{
	if (muyeong_event_)
		return;

	char_event_info* info = alloc_event_info<char_event_info>();

	info->ch = this;
	muyeong_event_ = event_create(skill_muyoung_event, info, passes_per_sec(1));
}

void character::stop_muyeong_event()
{
	event_cancel(&muyeong_event_);
}

void character::skill_learn_wait_more_time_message(uint32_t ms)
{
	//
	if (ms < 3 * 60)
		chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("몸 속이 뜨겁군. 하지만 아주 편안해. 이대로 기를 안정시키자."));
	else if (ms < 5 * 60)
		chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("그래, 천천히. 좀더 천천히, 그러나 막힘 없이 빠르게!"));
	else if (ms < 10 * 60) // 10 minutes
		chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("그래, 이 느낌이야. 체내에 기가 아주 충만해."));
	else if (ms < 30 * 60) // 30 minutes
	{
		chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("다 읽었다! 이제 비급에 적혀있는 대로 전신에 기를 돌리기만 하면,"));
		chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("그것으로 수련은 끝난 거야!"));
	}
	else if (ms < 1 * 3600) // 1 hour
		chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("이제 책의 마지막 장이야! 수련의 끝이 눈에 보이고 있어!"));
	else if (ms < 2 * 3600) // 2 hours
		chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("얼마 안 남았어! 조금만 더!"));
	else if (ms < 3 * 3600)
		chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("좋았어! 조금만 더 읽으면 끝이다!"));
	else if (ms < 6 * 3600)
	{
		chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("책장도 이제 얼마 남지 않았군."));
		chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("뭔가 몸 안에 힘이 생기는 기분인 걸."));
	}
	else if (ms < 12 * 3600)
	{
		chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("이제 좀 슬슬 가닥이 잡히는 것 같은데."));
		chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("좋아, 이 기세로 계속 나간다!"));
	}
	else if (ms < 18 * 3600)
	{
		chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("아니 어떻게 된 게 종일 읽어도 머리에 안 들어오냐."));
		chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("공부하기 싫어지네."));
	}
	else //if (ms < 2 * 86400)
	{
		chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("생각만큼 읽기가 쉽지가 않군. 이해도 어렵고 내용도 난해해."));
		chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("이래서야 공부가 안된다구."));
	}

}

void character::disable_cooltime()
{
	disable_cooltime_ = true;
}

bool character::has_mob_skill() const
{
	return count_mob_skill() > 0;
}

size_t character::count_mob_skill() const
{
	if (!mob_data)
		return 0;

	size_t c = 0;

	for (size_t i = 0; i < MOB_SKILL_MAX_NUM; ++i)
		if (mob_data->table.skills[i].vnum)
			++c;

	return c;
}

const mob_skill_info* character::mob_skill(unsigned int idx) const
{
	if (idx >= MOB_SKILL_MAX_NUM)
		return nullptr;

	if (!mob_data)
		return nullptr;

	if (0 == mob_data->table.skills[idx].vnum)
		return nullptr;

	return &mob_data->mob_skill_info[idx];
}

bool character::can_use_mob_skill(unsigned int idx) const
{
	const mob_skill_info* pInfo = mob_skill(idx);

	if (!pInfo)
		return false;

	if (mob_skill_cooltime_[idx] > get_dword_time())
		return false;

	if (number(0, 1))
		return false;

	return true;
}

EVENTINFO(mob_skill_event_info)
{
	dynamic_character_ptr ch;
	pixel_position pos;
	uint32_t vnum;
	int index;
	uint8_t level;

	mob_skill_event_info()
	: ch()
	, pos()
	, vnum(0)
	, index(0)
	, level(0)
	{
	}
};

EVENTFUNC(mob_skill_hit_event)
{
	mob_skill_event_info * info = dynamic_cast<mob_skill_event_info *>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "mob_skill_event_info> <Factor> Null pointer" );
		return 0;
	}

	// <Factor>
	character * ch = info->ch;
	if (ch == nullptr) {
		return 0;
	}

	ch->compute_skill_at_position(info->vnum, info->pos, info->level);
	ch->mob_skill_event.erase(info->index);

	return 0;
}

bool character::use_mob_skill(unsigned int idx)
{
	if (is_pc())
		return false;

	const mob_skill_info* pInfo = mob_skill(idx);

	if (!pInfo)
		return false;

	uint32_t dwVnum = pInfo->skill_vnum;
	skill_proto * pkSk = skill_manager::instance().get(dwVnum);

	if (!pkSk)
		return false;

	const float k = 1.0 * skill_power(pkSk->vnum, pInfo->skill_level) * pkSk->max_level / 100;

	pkSk->cooldown_poly.set_var("k", k);
	int iCooltime = (int) (pkSk->cooldown_poly.eval() * 1000);

	mob_skill_cooltime_[idx] = get_dword_time() + iCooltime;

	metin2::log::info( "USE_MOB_SKILL: {} idx {:d} vnum {:d} cooltime {:d}", name(), idx, dwVnum, iCooltime);

	if (mob_data->mob_skill_info[idx].splash_attack.empty())
	{
		metin2::log::error("No skill hit data for mob {} index {:d}", name(), idx);
		return false;
	}

	for (size_t i = 0; i < mob_data->mob_skill_info[idx].splash_attack.size(); i++)
	{
		pixel_position pos = xyz();
		const mob_splash_attack_info& rInfo = mob_data->mob_skill_info[idx].splash_attack[i];

		if (rInfo.hit_distance)
		{
			float fx, fy;
			get_delta_by_degree(rotation(), rInfo.hit_distance, &fx, &fy);
			pos.x += (int32_t) fx;
			pos.y += (int32_t) fy;
		}

		if (rInfo.timing)
		{
			if (metin2::config::test_server())
				metin2::log::info( "               timing {:d}ms", rInfo.timing);

			mob_skill_event_info* info = alloc_event_info<mob_skill_event_info>();

			info->ch = this;
			info->pos = pos;
			info->level = pInfo->skill_level;
			info->vnum = dwVnum;
			info->index = i;

			// <Factor> Cancel existing event first
			auto it = mob_skill_event.find(i);
			if (it != mob_skill_event.end()) {
				event_ptr existing = it->second;
				event_cancel(&existing);
				mob_skill_event.erase(it);
			}

			mob_skill_event.insert(std::make_pair(i, event_create(mob_skill_hit_event, info, passes_per_sec(rInfo.timing) / 1000)));
		}
		else
		{
			compute_skill_at_position(dwVnum, pos, pInfo->skill_level);
		}
	}

	return true;
}

void character::reset_mob_skill_cooltime()
{
	memset(mob_skill_cooltime_, 0, sizeof(mob_skill_cooltime_));
}

bool character::is_usable_skill_motion(uint32_t dwMotionIndex) const
{
	uint32_t selfJobGroup = (job()+1) * 10 + skill_group();

	const uint32_t SKILL_NUM = 158;
	static uint32_t s_anSkill2JobGroup[SKILL_NUM] = {
		0, // common_skill 0
		11, // job_skill 1
		11, // job_skill 2
		11, // job_skill 3
		11, // job_skill 4
		11, // job_skill 5
		11, // job_skill 6
		0, // common_skill 7
		0, // common_skill 8
		0, // common_skill 9
		0, // common_skill 10
		0, // common_skill 11
		0, // common_skill 12
		0, // common_skill 13
		0, // common_skill 14
		0, // common_skill 15
		12, // job_skill 16
		12, // job_skill 17
		12, // job_skill 18
		12, // job_skill 19
		12, // job_skill 20
		12, // job_skill 21
		0, // common_skill 22
		0, // common_skill 23
		0, // common_skill 24
		0, // common_skill 25
		0, // common_skill 26
		0, // common_skill 27
		0, // common_skill 28
		0, // common_skill 29
		0, // common_skill 30
		21, // job_skill 31
		21, // job_skill 32
		21, // job_skill 33
		21, // job_skill 34
		21, // job_skill 35
		21, // job_skill 36
		0, // common_skill 37
		0, // common_skill 38
		0, // common_skill 39
		0, // common_skill 40
		0, // common_skill 41
		0, // common_skill 42
		0, // common_skill 43
		0, // common_skill 44
		0, // common_skill 45
		22, // job_skill 46
		22, // job_skill 47
		22, // job_skill 48
		22, // job_skill 49
		22, // job_skill 50
		22, // job_skill 51
		0, // common_skill 52
		0, // common_skill 53
		0, // common_skill 54
		0, // common_skill 55
		0, // common_skill 56
		0, // common_skill 57
		0, // common_skill 58
		0, // common_skill 59
		0, // common_skill 60
		31, // job_skill 61
		31, // job_skill 62
		31, // job_skill 63
		31, // job_skill 64
		31, // job_skill 65
		31, // job_skill 66
		0, // common_skill 67
		0, // common_skill 68
		0, // common_skill 69
		0, // common_skill 70
		0, // common_skill 71
		0, // common_skill 72
		0, // common_skill 73
		0, // common_skill 74
		0, // common_skill 75
		32, // job_skill 76
		32, // job_skill 77
		32, // job_skill 78
		32, // job_skill 79
		32, // job_skill 80
		32, // job_skill 81
		0, // common_skill 82
		0, // common_skill 83
		0, // common_skill 84
		0, // common_skill 85
		0, // common_skill 86
		0, // common_skill 87
		0, // common_skill 88
		0, // common_skill 89
		0, // common_skill 90
		41, // job_skill 91
		41, // job_skill 92
		41, // job_skill 93
		41, // job_skill 94
		41, // job_skill 95
		41, // job_skill 96
		0, // common_skill 97
		0, // common_skill 98
		0, // common_skill 99
		0, // common_skill 100
		0, // common_skill 101
		0, // common_skill 102
		0, // common_skill 103
		0, // common_skill 104
		0, // common_skill 105
		42, // job_skill 106
		42, // job_skill 107
		42, // job_skill 108
		42, // job_skill 109
		42, // job_skill 110
		42, // job_skill 111
		0, // common_skill 112
		0, // common_skill 113
		0, // common_skill 114
		0, // common_skill 115
		0, // common_skill 116
		0, // common_skill 117
		0, // common_skill 118
		0, // common_skill 119
		0, // common_skill 120
		0, // common_skill 121
		0, // common_skill 122
		0, // common_skill 123
		0, // common_skill 124
		0, // common_skill 125
		0, // common_skill 126
		0, // common_skill 127
		0, // common_skill 128
		0, // common_skill 129
		0, // common_skill 130
		0, // common_skill 131
		0, // common_skill 132
		0, // common_skill 133
		0, // common_skill 134
		0, // common_skill 135
		0, // common_skill 136
		0, // job_skill 137
		0, // job_skill 138
		0, // job_skill 139
		0, // job_skill 140
		0, // common_skill 141
		0, // common_skill 142
		0, // common_skill 143
		0, // common_skill 144
		0, // common_skill 145
		0, // common_skill 146
		0, // common_skill 147
		0, // common_skill 148
		0, // common_skill 149
		0, // common_skill 150
		0, // common_skill 151
		0, // job_skill 152
		0, // job_skill 153
		0, // job_skill 154
		0, // job_skill 155
		0, // job_skill 156
		0, // job_skill 157
	}; // s_anSkill2JobGroup

	const uint32_t MOTION_MAX_NUM 	= 124;
	const uint32_t SKILL_LIST_MAX_COUNT	= 5;

	static uint32_t s_anMotion2SkillVnumList[MOTION_MAX_NUM][SKILL_LIST_MAX_COUNT] =
	{
		// count    WarriorID   AssassinID  SuraID      ShamanID
		{   0,		0,			0,			0,			0		}, //  0

		// job group 1 basic skills
		{   4,		1,			31,			61,			91		}, //  1
		{   4,		2,			32,			62,			92		}, //  2
		{   4,		3,			33,			63,			93		}, //  3
		{   4,		4,			34,			64,			94		}, //  4
		{   4,		5,			35,			65,			95		}, //  5
		{   4,		6,			36,			66,			96		}, //  6
		{   0,		0,			0,			0,			0		}, //  7
		{   0,		0,			0,			0,			0		}, //  8
		// job group 1 basic skills end

		// reserved
		{   0,		0,			0,			0,			0		}, //  9
		{   0,		0,			0,			0,			0		}, //  10
		{   0,		0,			0,			0,			0		}, //  11
		{   0,		0,			0,			0,			0		}, //  12
		{   0,		0,			0,			0,			0		}, //  13
		{   0,		0,			0,			0,			0		}, //  14
		{   0,		0,			0,			0,			0		}, //  15
		// reserved end

		// job group 2 basic skills
		{   4,		16,			46,			76,			106		}, //  16
		{   4,		17,			47,			77,			107		}, //  17
		{   4,		18,			48,			78,			108		}, //  18
		{   4,		19,			49,			79,			109		}, //  19
		{   4,		20,			50,			80,			110		}, //  20
		{   4,		21,			51,			81,			111		}, //  21
		{   0,		0,			0,			0,			0		}, //  22
		{   0,		0,			0,			0,			0		}, //  23
		// job group 2 basic skills end

		// reserved
		{   0,		0,			0,			0,			0		}, //  24
		{   0,		0,			0,			0,			0		}, //  25
		// reserved end

		// job group 1 master skills
		{   4,		1,			31,			61,			91		}, //  26
		{   4,		2,			32,			62,			92		}, //  27
		{   4,		3,			33,			63,			93		}, //  28
		{   4,		4,			34,			64,			94		}, //  29
		{   4,		5,			35,			65,			95		}, //  30
		{   4,		6,			36,			66,			96		}, //  31
		{   0,		0,			0,			0,			0		}, //  32
		{   0,		0,			0,			0,			0		}, //  33
		// job group 1 master skills end

		// reserved
		{   0,		0,			0,			0,			0		}, //  34
		{   0,		0,			0,			0,			0		}, //  35
		{   0,		0,			0,			0,			0		}, //  36
		{   0,		0,			0,			0,			0		}, //  37
		{   0,		0,			0,			0,			0		}, //  38
		{   0,		0,			0,			0,			0		}, //  39
		{   0,		0,			0,			0,			0		}, //  40
		// reserved end

		// job group 2 master skills
		{   4,		16,			46,			76,			106		}, //  41
		{   4,		17,			47,			77,			107		}, //  42
		{   4,		18,			48,			78,			108		}, //  43
		{   4,		19,			49,			79,			109		}, //  44
		{   4,		20,			50,			80,			110		}, //  45
		{   4,		21,			51,			81,			111		}, //  46
		{   0,		0,			0,			0,			0		}, //  47
		{   0,		0,			0,			0,			0		}, //  48
		// job group 2 master skills end

		// reserved
		{   0,		0,			0,			0,			0		}, //  49
		{   0,		0,			0,			0,			0		}, //  50
		// reserved end

		// job group 1 grand master skills
		{   4,		1,			31,			61,			91		}, //  51
		{   4,		2,			32,			62,			92		}, //  52
		{   4,		3,			33,			63,			93		}, //  53
		{   4,		4,			34,			64,			94		}, //  54
		{   4,		5,			35,			65,			95		}, //  55
		{   4,		6,			36,			66,			96		}, //  56
		{   0,		0,			0,			0,			0		}, //  57
		{   0,		0,			0,			0,			0		}, //  58
		// job group 1 grand master skills end

		// reserved
		{   0,		0,			0,			0,			0		}, //  59
		{   0,		0,			0,			0,			0		}, //  60
		{   0,		0,			0,			0,			0		}, //  61
		{   0,		0,			0,			0,			0		}, //  62
		{   0,		0,			0,			0,			0		}, //  63
		{   0,		0,			0,			0,			0		}, //  64
		{   0,		0,			0,			0,			0		}, //  65
		// reserved end

		// job group 2 grand master skills
		{   4,		16,			46,			76,			106		}, //  66
		{   4,		17,			47,			77,			107		}, //  67
		{   4,		18,			48,			78,			108		}, //  68
		{   4,		19,			49,			79,			109		}, //  69
		{   4,		20,			50,			80,			110		}, //  70
		{   4,		21,			51,			81,			111		}, //  71
		{   0,		0,			0,			0,			0		}, //  72
		{   0,		0,			0,			0,			0		}, //  73
		// job group 2 grand master skills end

		//reserved
		{   0,		0,			0,			0,			0		}, //  74
		{   0,		0,			0,			0,			0		}, //  75
		// reserved end

		// job group 1 perfect master skills
		{   4,		1,			31,			61,			91		}, //  76
		{   4,		2,			32,			62,			92		}, //  77
		{   4,		3,			33,			63,			93		}, //  78
		{   4,		4,			34,			64,			94		}, //  79
		{   4,		5,			35,			65,			95		}, //  80
		{   4,		6,			36,			66,			96		}, //  81
		{   0,		0,			0,			0,			0		}, //  82
		{   0,		0,			0,			0,			0		}, //  83
		// job group 1 perfect master skills end

		// reserved
		{   0,		0,			0,			0,			0		}, //  84
		{   0,		0,			0,			0,			0		}, //  85
		{   0,		0,			0,			0,			0		}, //  86
		{   0,		0,			0,			0,			0		}, //  87
		{   0,		0,			0,			0,			0		}, //  88
		{   0,		0,			0,			0,			0		}, //  89
		{   0,		0,			0,			0,			0		}, //  90
		// reserved end

		// job group 2 perfect master skills
		{   4,		16,			46,			76,			106		}, //  91
		{   4,		17,			47,			77,			107		}, //  92
		{   4,		18,			48,			78,			108		}, //  93
		{   4,		19,			49,			79,			109		}, //  94
		{   4,		20,			50,			80,			110		}, //  95
		{   4,		21,			51,			81,			111		}, //  96
		{   0,		0,			0,			0,			0		}, //  97
		{   0,		0,			0,			0,			0		}, //  98
		// job group 2 perfect master skills end

		// reserved
		{   0,		0,			0,			0,			0		}, //  99
		{   0,		0,			0,			0,			0		}, //  100
		// reserved end

		// guild skills
		{   1,  152,    0,    0,    0}, //  101
		{   1,  153,    0,    0,    0}, //  102
		{   1,  154,    0,    0,    0}, //  103
		{   1,  155,    0,    0,    0}, //  104
		{   1,  156,    0,    0,    0}, //  105
		{   1,  157,    0,    0,    0}, //  106
		// guild skills end

		// reserved
		{   0,    0,    0,    0,    0}, //  107
		{   0,    0,    0,    0,    0}, //  108
		{   0,    0,    0,    0,    0}, //  109
		{   0,    0,    0,    0,    0}, //  110
		{   0,    0,    0,    0,    0}, //  111
		{   0,    0,    0,    0,    0}, //  112
		{   0,    0,    0,    0,    0}, //  113
		{   0,    0,    0,    0,    0}, //  114
		{   0,    0,    0,    0,    0}, //  115
		{   0,    0,    0,    0,    0}, //  116
		{   0,    0,    0,    0,    0}, //  117
		{   0,    0,    0,    0,    0}, //  118
		{   0,    0,    0,    0,    0}, //  119
		{   0,    0,    0,    0,    0}, //  120
		// reserved end

		// riding skills
		{   2,  137,  140,    0,    0}, //  121
		{   1,  138,    0,    0,    0}, //  122
		{   1,  139,    0,    0,    0}, //  123
		// riding skills end
	};

	if (dwMotionIndex >= MOTION_MAX_NUM)
	{
		metin2::log::error("OUT_OF_MOTION_VNUM: name={}, motion={:d}/{:d}", name(), dwMotionIndex, MOTION_MAX_NUM);
		return false;
	}

	uint32_t* skillVNums = s_anMotion2SkillVnumList[dwMotionIndex];

	uint32_t skillCount = *skillVNums++;
	if (skillCount >= SKILL_LIST_MAX_COUNT)
	{
		metin2::log::error("OUT_OF_SKILL_LIST: name={}, count={:d}/{:d}", name(), skillCount, SKILL_LIST_MAX_COUNT);
		return false;
	}

	for (uint32_t skillIndex = 0; skillIndex != skillCount; ++skillIndex)
	{
		if (skillIndex >= SKILL_MAX_NUM)
		{
			metin2::log::error("OUT_OF_SKILL_VNUM: name={}, skill={:d}/{:d}", name(), skillIndex, SKILL_MAX_NUM);
			return false;
		}

		uint32_t eachSkillVNum = skillVNums[skillIndex];
		if ( eachSkillVNum != 0 )
		{
			uint32_t eachJobGroup = s_anSkill2JobGroup[eachSkillVNum];

			if (0 == eachJobGroup || eachJobGroup == selfJobGroup)
			{
				// GUILDSKILL_BUG_FIX
				uint32_t eachSkillLevel = 0;

				if (eachSkillVNum >= GUILD_SKILL_START && eachSkillVNum <= GUILD_SKILL_END)
				{
					if (get_guild())
						eachSkillLevel = get_guild()->skill_level(eachSkillVNum);
					else
						eachSkillLevel = 0;
				}
				else
				{
					eachSkillLevel = skill_level(eachSkillVNum);
				}

				if (eachSkillLevel > 0)
				{
					return true;
				}
				// END_OF_GUILDSKILL_BUG_FIX
			}
		}
	}

	return false;
}

void character::clear_skill()
{
	point_change(POINT_SKILL, 4 + (level() - 5) - point(POINT_SKILL));

	reset_skill();
}

void character::clear_sub_skill()
{
	point_change(POINT_SUB_SKILL, level() < 10 ? 0 : (level() - 9) - point(POINT_SUB_SKILL));

	if (skill_levels_ == nullptr)
	{
		metin2::log::error("m_pSkillLevels nil (name: {})", name());
		return;
	}

	player_skill CleanSkill;
	memset(&CleanSkill, 0, sizeof(player_skill));

	size_t count = sizeof(sub_skill_vnums) / sizeof(sub_skill_vnums[0]);

	for (size_t i = 0; i < count; ++i)
	{
		if (sub_skill_vnums[i] >= SKILL_MAX_NUM)
			continue;

		skill_levels_[sub_skill_vnums[i]] = CleanSkill;
	}

	compute_points();
	skill_level_packet();
}

bool character::reset_one_skill(uint32_t dwVnum)
{
	if (nullptr == skill_levels_)
	{
		metin2::log::error("m_pSkillLevels nil (name {}, vnum {:d})", name(), dwVnum);
		return false;
	}

	if (dwVnum >= SKILL_MAX_NUM)
	{
		metin2::log::error("vnum overflow (name {}, vnum {:d})", name(), dwVnum);
		return false;
	}

	uint8_t level = skill_levels_[dwVnum].level;

	skill_levels_[dwVnum].level = 0;
	skill_levels_[dwVnum].master_type = 0;
	skill_levels_[dwVnum].t_next_read = 0;

	if (level > 17)
		level = 17;

	point_change(POINT_SKILL, level);

	log_manager::instance().char_log(this, dwVnum, "ONE_SKILL_RESET_BY_SCROLL", "");

	compute_points();
	skill_level_packet();

	return true;
}

bool character::can_use_skill(uint32_t dwSkillVnum) const
{
	if (0 == dwSkillVnum) return false;

	if (0 < skill_group())
	{
		const int SKILL_COUNT = 6;
		static const uint32_t SkillList[JOB_MAX_NUM][SKILL_GROUP_MAX_NUM][SKILL_COUNT] =
		{
			{ {	1,	2,	3,	4,	5,	6	}, {	16,	17,	18,	19,	20,	21	} },
			{ {	31,	32,	33,	34,	35,	36	}, {	46,	47,	48,	49,	50,	51	} },
			{ {	61,	62,	63,	64,	65,	66	}, {	76,	77,	78,	79,	80,	81	} },
			{ {	91,	92,	93,	94,	95,	96	}, {	106,107,108,109,110,111	} },
		};

		const uint32_t* pSkill = SkillList[ job() ][ skill_group()-1 ];

		for (int i=0 ; i < SKILL_COUNT ; ++i)
		{
			if (pSkill[i] == dwSkillVnum) return true;
		}
	}

	//if (true == IsHorseRiding())

	if (true == is_riding())
	{
		//among mounts, only premium horses can use skills
		if(get_mount_vnum())
		{
			if( get_mount_vnum() < 20209 && get_mount_vnum() > 20212)
				if (get_mount_vnum() != 20215 || get_mount_vnum() != 20218 || get_mount_vnum() != 20220)
					return false;
		}

		switch(dwSkillVnum)
		{
			case SKILL_HORSE_WILDATTACK:
			case SKILL_HORSE_CHARGE:
			case SKILL_HORSE_ESCAPE:
			case SKILL_HORSE_WILDATTACK_RANGE:
				return true;
		}
	}

	switch( dwSkillVnum )
	{
		case 121: case 122: case 124: case 126: case 127: case 128: case 129: case 130:
		case 131:
		case 151: case 152: case 153: case 154: case 155: case 156: case 157: case 158: case 159:
			return true;
	}

	return false;
}

bool character::check_skill_hit_count(const uint8_t SkillID, const vid TargetVID)
{
	std::map<int, skill_use_info>::iterator iter = skill_use_info_.find(SkillID);

	if (iter == skill_use_info_.end())
	{
		metin2::log::info( "SkillHack: Skill({:d}) is not in container", SkillID);
		return false;
	}

	skill_use_info& rSkillUseInfo = iter->second;

	if (false == rSkillUseInfo.used)
	{
		metin2::log::info( "SkillHack: not used skill({:d})", SkillID);
		return false;
	}

	switch (SkillID)
	{
		case SKILL_YONGKWON:
		case SKILL_HWAYEOMPOK:
		case SKILL_DAEJINGAK:
		case SKILL_PAERYONG:
			metin2::log::info( "SkillHack: cannot use attack packet for skill({:d})", SkillID);
			return false;
	}

	auto iterTargetMap = rSkillUseInfo.target_vid_map.find(TargetVID);

	if (rSkillUseInfo.target_vid_map.end() != iterTargetMap)
	{
		size_t MaxAttackCountPerTarget = 1;

		switch (SkillID)
		{
			case SKILL_SAMYEON:
			case SKILL_CHARYUN:
				MaxAttackCountPerTarget = 3;
				break;

			case SKILL_HORSE_WILDATTACK_RANGE:
				MaxAttackCountPerTarget = 5;
				break;

			case SKILL_YEONSA:
				MaxAttackCountPerTarget = 7;
				break;

			case SKILL_HORSE_ESCAPE:
				MaxAttackCountPerTarget = 10;
				break;
		}

		if (iterTargetMap->second >= MaxAttackCountPerTarget)
		{
			metin2::log::info( "SkillHack: Too Many Hit count from SkillID({:d}) count({:d})", SkillID, iterTargetMap->second);
			return false;
		}

		iterTargetMap->second++;
	}
	else
	{
		rSkillUseInfo.target_vid_map.insert( std::make_pair(TargetVID, 1) );
	}

	return true;
}
}
