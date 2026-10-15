#include <metin2/game/characters/mob_manager.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/data/file_loader.h>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/data/text_file_loader.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/common/utils.h>
#include <metin2/log/log.h>
#include <metin2/core/main.h>
#include <metin2/core/utils.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game::inline characters {


mob::mob()
{
	memset( &table, 0, sizeof(table) );

	for( size_t i=0 ; i < MOB_SKILL_MAX_NUM ; ++i )
	{
		mob_skill_info[i].skill_vnum = 0;
		mob_skill_info[i].skill_level = 0;
		mob_skill_info[i].splash_attack.clear();
	}
}

void mob::add_skill_splash(int index, uint32_t timing, uint32_t hit_distance)
{
	if (index >= MOB_SKILL_MAX_NUM || index < 0)
		return;

	metin2::log::info( "MOB_SPLASH {} idx {:d} timing {:d} hit_distance {:d}",
			table.locale_name, index, timing, hit_distance);

	mob_skill_info[index].splash_attack.push_back(mob_splash_attack_info(timing, hit_distance));
}

mob_instance::mob_instance()
	: is_berserk(false), is_god_speed(false), is_revive(false)
{
	last_attacked_time = get_dword_time();
	last_warp_time = get_dword_time();

	memset( &pos_last_attacked, 0, sizeof(pos_last_attacked) );
}

mob_manager::mob_manager()
{
}

mob_manager::~mob_manager()
{
}

auto mob_manager::initialize(mob_table * table_value, int size) -> bool
{
	map_pk_mob_by_vnum_.clear();
	map_pk_mob_by_name_.clear();

	mob_table * t = table_value;


	for (int i = 0; i < size; ++i, ++t)
	{
		mob * pkMob = new mob;

		std::memcpy(&pkMob->table, t, sizeof(mob_table));

		map_pk_mob_by_vnum_.insert(std::map<uint32_t, mob *>::value_type(t->vnum, pkMob));
		map_pk_mob_by_name_.insert(std::map<std::string, mob *>::value_type(t->locale_name, pkMob));

		int SkillCount = 0;

		for (int j = 0; j < MOB_SKILL_MAX_NUM; ++j)
			if (pkMob->table.skills[j].vnum)
				++SkillCount;

		metin2::log::info( "MOB: #{:<5d} {:<30} LEVEL {:d} HP {:d} DEF {:d} EXP {:d} DROP_ITEM_VNUM {:d} SKILL_COUNT {:d}", 
				t->vnum, t->locale_name, t->level, t->max_hp, t->def, t->exp, t->drop_item_vnum, SkillCount);

		if (t->type == CHAR_TYPE_NPC || t->type == CHAR_TYPE_WARP || t->type == CHAR_TYPE_GOTO)
			character_manager::instance().register_race_num(t->vnum);

		quest::quest_manager::instance().register_npc_vnum(t->vnum);
	}


	const int FILE_NAME_LEN = 256;
	char szGroupFileName[FILE_NAME_LEN];
	char szGroupGroupFileName[FILE_NAME_LEN];

	snprintf(szGroupFileName, sizeof(szGroupGroupFileName),
			"%s/group.txt", locale_service_get_base_path().c_str());
	snprintf(szGroupGroupFileName, sizeof(szGroupGroupFileName),
			"%s/group_group.txt", locale_service_get_base_path().c_str());
	
	if (!load_group(szGroupFileName))
	{
		metin2::log::error("cannot load {}", szGroupFileName);
		thecore_shutdown();
	}
	if (!load_group_group(szGroupGroupFileName))
	{
		metin2::log::error("cannot load {}", szGroupGroupFileName);
		thecore_shutdown();
	}

	character_manager::instance().for_each_pc(std::bind(&mob_manager::rebind_mob_proto, this, std::placeholders::_1));
	return true;
}

void mob_manager::rebind_mob_proto(character * ch)
{
	if (ch->is_pc())
		return;

	const mob * pMob = get(ch->race_num());

	if (pMob)
		ch->set_proto(pMob);
}

auto mob_manager::get(uint32_t vnum) -> const mob *
{
	std::map<uint32_t, mob *>::iterator it = map_pk_mob_by_vnum_.find(vnum);

	if (it == map_pk_mob_by_vnum_.end())
		return nullptr;

	return it->second;
}

auto mob_manager::get(const char * name, bool is_abbrev) -> const mob *
{
	std::map<std::string, mob *>::iterator it;

	if (!is_abbrev)
	{
		it = map_pk_mob_by_name_.find(name);

		if (it == map_pk_mob_by_name_.end())
			return nullptr;

		return it->second;
	}

	int len = strlen(name);
	it = map_pk_mob_by_name_.begin();

	while (it != map_pk_mob_by_name_.end())
	{
		if (!strncmp(it->first.c_str(), name, len))
			return it->second;

		++it;
	}

	return nullptr;
}

void mob_manager::inc_regen_count(uint8_t regen_type, uint32_t vnum, int count, int time)
{
	switch (regen_type)
	{
		case REGEN_TYPE_MOB:
			regen_count_[vnum] += count * 86400. / time;
			break;

		case REGEN_TYPE_GROUP:
			{
				mob_group * pkGroup = mob_manager::instance().group(vnum);
				if (!pkGroup)
					return;
				const std::vector<uint32_t> & c_rdwMembers = pkGroup->member_vector();

				for (uint32_t i=0; i<c_rdwMembers.size(); i++)
					regen_count_[c_rdwMembers[i]] += count * 86400. / time;
			}
			break;

		case REGEN_TYPE_GROUP_GROUP:
			{
				std::map<uint32_t, mob_group_group *>::iterator it = map_pk_mob_group_group_.find(vnum);

				if (it == map_pk_mob_group_group_.end())
					return;

				std::vector<uint32_t>& v = it->second->vec_dw_member_vnum;
				for (uint32_t i=0; i<v.size(); i++)
				{
					mob_group * pkGroup = mob_manager::instance().group(v[i]);
					if (!pkGroup)
						return;
					const std::vector<uint32_t> & c_rdwMembers = pkGroup->member_vector();

					for (uint32_t i=0; i<c_rdwMembers.size(); i++)
						regen_count_[c_rdwMembers[i]] += count * 86400. / time / v.size();
				}
			}
			break;
	}
}

void mob_manager::dump_regen_count(const char* filename)
{
	FILE* fp = fopen(filename, "w");

	if (fp)
	{
		std::map<uint32_t, double>::iterator it;

		fprintf(fp,"MOB_VNUM\tCOUNT\n");

		for (it = regen_count_.begin(); it != regen_count_.end(); ++it)
		{
			fprintf(fp,"%u\t%g\n", it->first, it->second);
		}

		fclose(fp);
	}
}

auto mob_manager::group_from_group_group(uint32_t vnum) -> uint32_t
{
	std::map<uint32_t, mob_group_group *>::iterator it = map_pk_mob_group_group_.find(vnum);

	if (it == map_pk_mob_group_group_.end())
		return 0;

	return it->second->member();
}

auto mob_manager::group(uint32_t vnum) -> mob_group *
{
	std::map<uint32_t, mob_group *>::iterator it = map_pk_mob_group_.find(vnum);

	if (it == map_pk_mob_group_.end())
		return nullptr;

	return it->second;
}

auto mob_manager::load_group_group(const char * file_name) -> bool
{
	text_file_loader loader;

	if (!loader.load(file_name))
		return false;

	std::string stName;

	for (uint32_t i = 0; i < loader.child_node_count(); ++i)
	{
		loader.set_child_node(i);

		loader.current_node_name(&stName);

		int iVnum;

		if (!loader.token_integer("vnum", &iVnum))
		{
			metin2::log::error("LoadGroupGroup : Syntax error {} : no vnum, node {}", file_name, stName.c_str());
			loader.set_parent_node();
			continue;
		}

		token_vector * pTok;

		mob_group_group * pkGroup = new mob_group_group(iVnum);

		for (int k = 1; k < 256; ++k)
		{
			char buf[4];
			snprintf(buf, sizeof(buf), "%d", k);

			if (loader.get_token_vector(buf, &pTok))
			{
				uint32_t dwMobVnum = 0;
				str_to_number(dwMobVnum, pTok->at(0).c_str());

				int prob = 1;
				if (pTok->size() > 1)
					str_to_number(prob, pTok->at(1).c_str());

				if (dwMobVnum)
					pkGroup->add_member(dwMobVnum);

				continue;
			}

			break;
		}

		loader.set_parent_node();

		map_pk_mob_group_group_.insert(std::make_pair((uint32_t)iVnum, pkGroup));
	}

	return true;
}

auto mob_manager::load_group(const char * file_name) -> bool
{
	text_file_loader loader;

	if (!loader.load(file_name))
		return false;

	std::string stName;

	for (uint32_t i = 0; i < loader.child_node_count(); ++i)
	{
		loader.set_child_node(i);

		loader.current_node_name(&stName);

		int iVnum;

		if (!loader.token_integer("vnum", &iVnum))
		{
			metin2::log::error("LoadGroup : Syntax error {} : no vnum, node {}", file_name, stName.c_str());
			loader.set_parent_node();
			continue;
		}

		token_vector * pTok;

		if (!loader.get_token_vector("leader", &pTok))
		{
			metin2::log::error("LoadGroup : Syntax error {} : no leader, node {}", file_name, stName.c_str());
			loader.set_parent_node();
			continue;
		}

		if (pTok->size() < 2)
		{
			metin2::log::error("LoadGroup : Syntax error {} : no leader vnum, node {}", file_name, stName.c_str());
			loader.set_parent_node();
			continue;
		}

		mob_group * pkGroup = new mob_group;

		pkGroup->create(iVnum, stName);
		uint32_t vnum = 0;
		str_to_number(vnum, pTok->at(1).c_str());
		pkGroup->add_member(vnum);

		metin2::log::info( "GROUP: {:<5d} {}", iVnum, stName.c_str());
		metin2::log::info( "               {} {}", pTok->at(0).c_str(), pTok->at(1).c_str());

		for (int k = 1; k < 256; ++k)
		{
			char buf[4];
			snprintf(buf, sizeof(buf), "%d", k);

			if (loader.get_token_vector(buf, &pTok))
			{
				metin2::log::info( "               {} {}", pTok->at(0).c_str(), pTok->at(1).c_str());
				uint32_t vnum = 0;
				str_to_number(vnum, pTok->at(1).c_str());
				pkGroup->add_member(vnum);
				continue;
			}

			break;
		}

		loader.set_parent_node();
		map_pk_mob_group_.insert(std::map<uint32_t, mob_group *>::value_type(iVnum, pkGroup));
	}

	return true;
}
}
