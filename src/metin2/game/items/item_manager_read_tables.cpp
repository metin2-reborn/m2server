#include <metin2/game/app/constants.h>
#include <metin2/game/data/file_loader.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/items/item_manager_private_types.h>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/data/text_file_loader.h>

#include <metin2/proto/length.h>
#include <metin2/common/utils.h>
#include <metin2/log/log.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game::inline items {

std::vector<item_drop_info> vec_pk_common_drop_item[MOB_RANK_MAX_NUM];

auto item_manager::read_common_drop_item_file(const char * file_name) -> bool
{
	FILE * fp = fopen(file_name, "r");

	if (!fp)
	{
		metin2::log::error("Cannot open {}", file_name);
		return false;
	}

	char buf[1024];


	while (fgets(buf, 1024, fp))
	{

		if (!*buf || *buf == '\n')
			continue;

		drop_item d[MOB_RANK_MAX_NUM];
		char szTemp[64];

		memset(&d, 0, sizeof(d));

		char * p = buf;
		char * p2;

		for (int i = 0; i <= MOB_RANK_S_KNIGHT; ++i)
		{
			for (int j = 0; j < 6; ++j)
			{
				p2 = strchr(p, '\t');

				if (!p2)
					break;

				strlcpy(szTemp, p, std::min<int>(sizeof(szTemp), (p2 - p) + 1));
				p = p2 + 1;

				switch (j)
				{
				case 0: break;
				case 1: str_to_number(d[i].lv_start, szTemp);	break;
				case 2: str_to_number(d[i].lv_end, szTemp);	break;
				case 3: d[i].percent = atof(szTemp);	break;
				case 4: strlcpy(d[i].item_name, szTemp, sizeof(d[i].item_name));	break;
				case 5: str_to_number(d[i].count, szTemp);	break;
				}
			}

			uint32_t dwPct = (uint32_t) (d[i].percent * 10000.0f);
			uint32_t dwItemVnum = 0;

			if (!item_manager::instance().vnum_by_original_name(d[i].item_name, dwItemVnum))
			{
				str_to_number(dwItemVnum, d[i].item_name);
				if (!item_manager::instance().table(dwItemVnum))
				{
					metin2::log::error("No such an item (name: {})", d[i].item_name);
					fclose(fp);
					return false;
				}
			}

			if (d[i].lv_start == 0)
				continue;

			vec_pk_common_drop_item[i].push_back(item_drop_info(d[i].lv_start, d[i].lv_end, dwPct, dwItemVnum));
		}
	}

	fclose(fp);

	for (int i = 0; i < MOB_RANK_MAX_NUM; ++i)
	{
		std::vector<item_drop_info> & v = vec_pk_common_drop_item[i];
		std::sort(v.begin(), v.end());

		std::vector<item_drop_info>::iterator it = v.begin();

		metin2::log::debug( "CommonItemDrop rank {:d}", i);

		while (it != v.end())
		{
			const item_drop_info & c = *(it++);
			metin2::log::debug( "CommonItemDrop {:d} {:d} {:d} {:d}", c.level_start, c.level_end, c.percent, c.vnum);
		}
	}

	return true;
}

auto item_manager::read_special_drop_item_file(const char * file_name) -> bool
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
			metin2::log::error("ReadSpecialDropItemFile : Syntax error {} : no vnum, node {}", file_name, stName.c_str());
			loader.set_parent_node();
			return false;
		}

		metin2::log::info("DROP_ITEM_GROUP {} {:d}", stName.c_str(), iVnum);

		token_vector * pTok;

		std::string stType;
		int type = special_item_group::NORMAL;
		if (loader.token_string("type", &stType))
		{
			to_lower(stType);
			if (stType == "pct")
			{
				type = special_item_group::PCT;
			}
			else if (stType == "quest")
			{
				type = special_item_group::QUEST;
				quest::quest_manager::instance().register_npc_vnum(iVnum);
			}
			else if (stType == "special")
			{
				type = special_item_group::SPECIAL;
			}
		}

		if ("attr" == stType)
		{
			auto pkGroup = std::make_unique<special_attr_group>(iVnum);
			for (int k = 1; k < 256; ++k)
			{
				char buf[4];
				snprintf(buf, sizeof(buf), "%d", k);

				if (loader.get_token_vector(buf, &pTok))
				{
					uint32_t apply_type = 0;
					int	apply_value = 0;
					str_to_number(apply_type, pTok->at(0).c_str());
					if (0 == apply_type)
					{
						apply_type = fn_get_apply_type(pTok->at(0).c_str());
						if (0 == apply_type)
						{
							metin2::log::error("Invalid APPLY_TYPE {} in Special Item Group Vnum {:d}", pTok->at(0).c_str(), iVnum);
							return false;
						}
					}
					str_to_number(apply_value, pTok->at(1).c_str());
					if (apply_type > MAX_APPLY_NUM)
					{
						metin2::log::error("Invalid APPLY_TYPE {:d} in Special Item Group Vnum {:d}", apply_type, iVnum);
						return false;
					}
					pkGroup->attrs.push_back(special_attr_group::special_attr_info(apply_type, apply_value));
				}
				else
				{
					break;
				}
			}
			if (loader.get_token_vector("effect", &pTok))
			{
				pkGroup->effect_file_name = pTok->at(0);
			}
			loader.set_parent_node();
			map_pk_special_attr_group_.insert(std::make_pair(iVnum, pkGroup.release()));
		}
		else
		{
			auto pkGroup = std::make_unique<special_item_group>(iVnum, type);
			for (int k = 1; k < 256; ++k)
			{
				char buf[4];
				snprintf(buf, sizeof(buf), "%d", k);

				if (loader.get_token_vector(buf, &pTok))
				{
					const std::string& name = pTok->at(0);
					uint32_t dwVnum = 0;

					if (!vnum_by_original_name(name.c_str(), dwVnum))
					{
						if (name == "경험치" || name == "exp")
						{
							dwVnum = special_item_group::EXP;
						}
						else if (name == "mob")
						{
							dwVnum = special_item_group::MOB;
						}
						else if (name == "slow")
						{
							dwVnum = special_item_group::SLOW;
						}
						else if (name == "drain_hp")
						{
							dwVnum = special_item_group::DRAIN_HP;
						}
						else if (name == "poison")
						{
							dwVnum = special_item_group::POISON;
						}
						else if (name == "group")
						{
							dwVnum = special_item_group::MOB_GROUP;
						}
						else
						{
							str_to_number(dwVnum, name.c_str());
							if (!item_manager::instance().table(dwVnum))
							{
								metin2::log::error("ReadSpecialDropItemFile : there is no item {} : node {}", name.c_str(), stName.c_str());
								return false;
							}
						}
					}

					int iCount = 0;
					str_to_number(iCount, pTok->at(1).c_str());
					int iProb = 0;
					str_to_number(iProb, pTok->at(2).c_str());

					int iRarePct = 0;
					if (pTok->size() > 3)
					{
						str_to_number(iRarePct, pTok->at(3).c_str());
					}

					metin2::log::info("        name {} count {:d} prob {:d} rare {:d}", name.c_str(), iCount, iProb, iRarePct);
					pkGroup->add_item(dwVnum, iCount, iProb, iRarePct);

					if (iVnum < 30000)
					{
						item_to_special_group_[dwVnum] = iVnum;
					}

					continue;
				}

				break;
			}
			loader.set_parent_node();
			if (special_item_group::QUEST == type)
			{
				map_pk_quest_item_group_.insert(std::make_pair(iVnum, pkGroup.release()));
			}
			else
			{
				map_pk_special_item_group_.insert(std::make_pair(iVnum, pkGroup.release()));
			}
		}
	}

	return true;
}


auto item_manager::conv_special_drop_item_file() -> bool
{
	char szSpecialItemGroupFileName[256];
	snprintf(szSpecialItemGroupFileName, sizeof(szSpecialItemGroupFileName),
		"%s/special_item_group.txt", locale_service_get_base_path().c_str());

	FILE *fp = fopen("special_item_group_vnum.txt", "w");
	if (!fp)
	{
		metin2::log::error("could not open file ({})", "special_item_group_vnum.txt");
		return false;
	}

	text_file_loader loader;

	if (!loader.load(szSpecialItemGroupFileName))
	{
		fclose(fp);
		return false;
	}

	std::string stName;

	for (uint32_t i = 0; i < loader.child_node_count(); ++i)
	{
		loader.set_child_node(i);

		loader.current_node_name(&stName);

		int iVnum;

		if (!loader.token_integer("vnum", &iVnum))
		{
			metin2::log::error("ConvSpecialDropItemFile : Syntax error {} : no vnum, node {}", szSpecialItemGroupFileName, stName.c_str());
			loader.set_parent_node();
			fclose(fp);
			return false;
		}

		std::string str;
		int type = 0;
		if (loader.token_string("type", &str))
		{
			to_lower(str);
			if (str == "pct")
			{
				type = 1;
			}
		}

		token_vector * pTok;

		fprintf(fp, "Group	%s\n", stName.c_str());
		fprintf(fp, "{\n");
		fprintf(fp, "	Vnum	%i\n", iVnum);
		if (type)
			fprintf(fp, "	Type	Pct");

		for (int k = 1; k < 256; ++k)
		{
			char buf[4];
			snprintf(buf, sizeof(buf), "%d", k);

			if (loader.get_token_vector(buf, &pTok))
			{
				const std::string& name = pTok->at(0);
				uint32_t dwVnum = 0;

				if (!vnum_by_original_name(name.c_str(), dwVnum))
				{
					if (	name == "경험치" ||
						name == "mob" ||
						name == "slow" ||
						name == "drain_hp" ||
						name == "poison" ||
						name == "group")
					{
						dwVnum = 0;
					}
					else
					{
						str_to_number(dwVnum, name.c_str());
						if (!item_manager::instance().table(dwVnum))
						{
							metin2::log::error("ReadSpecialDropItemFile : there is no item {} : node {}", name.c_str(), stName.c_str());
							fclose(fp);

							return false;
						}
					}
				}

				int iCount = 0;
				str_to_number(iCount, pTok->at(1).c_str());
				int iProb = 0;
				str_to_number(iProb, pTok->at(2).c_str());

				int iRarePct = 0;
				if (pTok->size() > 3)
				{
					str_to_number(iRarePct, pTok->at(3).c_str());
				}

				if (0 == dwVnum)
					fprintf(fp, "	%d	%s	%d	%d\n", k, name.c_str(), iCount, iProb);
				else
					fprintf(fp, "	%d	%u	%d	%d\n", k, dwVnum, iCount, iProb);

				continue;
			}

			break;
		}
		fprintf(fp, "}\n");
		fprintf(fp, "\n");

		loader.set_parent_node();
	}

	fclose(fp);
	return true;
}

auto item_manager::read_etc_drop_item_file(const char * file_name) -> bool
{
	FILE * fp = fopen(file_name, "r");

	if (!fp)
	{
		metin2::log::error("Cannot open {}", file_name);
		return false;
	}

	char buf[512];


	while (fgets(buf, 512, fp))
	{

		if (!*buf || *buf == '\n')
			continue;

		char szItemName[256];
		float fProb = 0.0f;

		strlcpy(szItemName, buf, sizeof(szItemName));
		char * cpTab = strrchr(szItemName, '\t');

		if (!cpTab)
			continue;

		*cpTab = '\0';
		fProb = atof(cpTab + 1);

		if (!*szItemName || fProb == 0.0f)
			continue;

		uint32_t dwItemVnum;

		if (!item_manager::instance().vnum_by_original_name(szItemName, dwItemVnum))
		{
			metin2::log::error("No such an item (name: {})", szItemName);
			fclose(fp);
			return false;
		}

		map_dw_etc_item_drop_prob_[dwItemVnum] = (uint32_t) (fProb * 10000.0f);
		metin2::log::info( "ETC_DROP_ITEM: {} prob {:f}", szItemName, fProb);
	}

	fclose(fp);
	return true;
}

auto item_manager::read_monster_drop_item_group(const char * file_name) -> bool
{
	text_file_loader loader;

	if (!loader.load(file_name))
		return false;

	for (uint32_t i = 0; i < loader.child_node_count(); ++i)
	{
		std::string stName("");

		loader.current_node_name(&stName);

		if (strncmp (stName.c_str(), "kr_", 3) == 0)
		{
			continue;
		}

		loader.set_child_node(i);

		int iMobVnum = 0;
		int iKillDrop = 0;
		int iLevelLimit = 0;

		std::string strType("");

		if (!loader.token_string("type", &strType))
		{
			metin2::log::error("ReadMonsterDropItemGroup : Syntax error {} : no type (kill|drop), node {}", file_name, stName.c_str());
			loader.set_parent_node();
			return false;
		}

		if (!loader.token_integer("mob", &iMobVnum))
		{
			metin2::log::error("ReadMonsterDropItemGroup : Syntax error {} : no mob vnum, node {}", file_name, stName.c_str());
			loader.set_parent_node();
			return false;
		}

		if (strType == "kill")
		{
			if (!loader.token_integer("kill_drop", &iKillDrop))
			{
				metin2::log::error("ReadMonsterDropItemGroup : Syntax error {} : no kill drop count, node {}", file_name, stName.c_str());
				loader.set_parent_node();
				return false;
			}
		}
		else
		{
			iKillDrop = 1;
		}

		if ( strType == "limit" )
		{
			if ( !loader.token_integer("level_limit", &iLevelLimit) )
			{
				metin2::log::error("ReadmonsterDropItemGroup : Syntax error {} : no level_limit, node {}", file_name, stName.c_str());
				loader.set_parent_node();
				return false;
			}
		}
		else
		{
			iLevelLimit = 0;
		}

		metin2::log::info("MOB_ITEM_GROUP {} [{}] {:d} {:d}", stName.c_str(), strType.c_str(), iMobVnum, iKillDrop);

		if (iKillDrop == 0)
		{
			loader.set_parent_node();
			continue;
		}

		token_vector* pTok = nullptr;

		if (strType == "kill")
		{
			auto pkGroup = std::make_unique<mob_item_group>(iKillDrop);

			for (int k = 1; k < 256; ++k)
			{
				char buf[4];
				snprintf(buf, sizeof(buf), "%d", k);

				if (loader.get_token_vector(buf, &pTok))
				{
					std::string& name = pTok->at(0);
					uint32_t dwVnum = 0;

					if (!vnum_by_original_name(name.c_str(), dwVnum))
					{
						str_to_number(dwVnum, name.c_str());
						if (!item_manager::instance().table(dwVnum))
						{
							metin2::log::error("ReadMonsterDropItemGroup : there is no item {} : node {} : vnum {:d}", name.c_str(), stName.c_str(), dwVnum);
							return false;
						}
					}

					int iCount = 0;
					str_to_number(iCount, pTok->at(1).c_str());

					if (iCount<1)
					{
						metin2::log::error("ReadMonsterDropItemGroup : there is no count for item {} : node {} : vnum {:d}, count {:d}", name.c_str(), stName.c_str(), dwVnum, iCount);
						return false;
					}

					int iPartPct = 0;
					str_to_number(iPartPct, pTok->at(2).c_str());

					if (iPartPct == 0)
					{
						metin2::log::error("ReadMonsterDropItemGroup : there is no drop percent for item {} : node {} : vnum {:d}, count {:d}, pct {:d}", name.c_str(), stName.c_str(), dwVnum, iCount, iPartPct);
						return false;
					}

					int iRarePct = 0;
					str_to_number(iRarePct, pTok->at(3).c_str());
					iRarePct = std::clamp(iRarePct, 0, 100);

					metin2::log::info("        {} count {:d} rare {:d}", name.c_str(), iCount, iRarePct);
					pkGroup->add_item(dwVnum, iCount, iPartPct, iRarePct);
					continue;
				}

				break;
			}
			map_pk_mob_item_group_.insert(std::map<uint32_t, mob_item_group*>::value_type(iMobVnum, pkGroup.release()));

		}
		else if (strType == "drop")
		{
			drop_item_group* pkGroup = nullptr;
			bool bNew = true;
			auto it = map_pk_drop_item_group_.find (iMobVnum);
			if (it == map_pk_drop_item_group_.end())
			{
				pkGroup = new drop_item_group;
			}
			else
			{
				pkGroup = it->second;
				bNew = false;
			}

			for (int k = 1; k < 256; ++k)
			{
				char buf[4];
				snprintf(buf, sizeof(buf), "%d", k);

				if (loader.get_token_vector(buf, &pTok))
				{
					std::string& name = pTok->at(0);
					uint32_t dwVnum = 0;

					if (!vnum_by_original_name(name.c_str(), dwVnum))
					{
						str_to_number(dwVnum, name.c_str());
						if (!item_manager::instance().table(dwVnum))
						{
							metin2::log::error("ReadDropItemGroup : there is no item {} : node {}", name.c_str(), stName.c_str());
							if (bNew)
								delete pkGroup;

							return false;
						}
					}

					int iCount = 0;
					str_to_number(iCount, pTok->at(1).c_str());

					if (iCount < 1)
					{
						metin2::log::error("ReadMonsterDropItemGroup : there is no count for item {} : node {}", name.c_str(), stName.c_str());
						if (bNew)
							delete pkGroup;

						return false;
					}

					float fPercent = atof(pTok->at(2).c_str());

					uint32_t dwPct = (uint32_t)(10000.0f * fPercent);

					metin2::log::info("        name {} pct {:d} count {:d}", name.c_str(), dwPct, iCount);
					pkGroup->add_item(dwVnum, dwPct, iCount);

					continue;
				}

				break;
			}
			if (bNew)
				map_pk_drop_item_group_.insert(std::map<uint32_t, drop_item_group*>::value_type(iMobVnum, pkGroup));

		}
		else if ( strType == "limit" )
		{
			auto pkLevelItemGroup = std::make_unique<level_item_group>(iLevelLimit);

			for ( int k=1; k < 256; k++ )
			{
				char buf[4];
				snprintf(buf, sizeof(buf), "%d", k);

				if ( loader.get_token_vector(buf, &pTok) )
				{
					std::string& name = pTok->at(0);
					uint32_t dwItemVnum = 0;

					if (false == vnum_by_original_name(name.c_str(), dwItemVnum))
					{
						str_to_number(dwItemVnum, name.c_str());
						if ( !item_manager::instance().table(dwItemVnum) )
						{
							return false;
						}
					}

					int iCount = 0;
					str_to_number(iCount, pTok->at(1).c_str());

					if (iCount < 1)
					{
						return false;
					}

					float fPct = atof(pTok->at(2).c_str());
					uint32_t dwPct = (uint32_t)(10000.0f * fPct);

					pkLevelItemGroup->add_item(dwItemVnum, dwPct, iCount);

					continue;
				}

				break;
			}

			map_pk_level_item_group_.insert(std::map<uint32_t, level_item_group*>::value_type(iMobVnum, pkLevelItemGroup.release()));
		}
		else if (strType == "thiefgloves")
		{
			auto pkGroup = std::make_unique<buyer_thief_gloves_item_group>();

			for (int k = 1; k < 256; ++k)
			{
				char buf[4];
				snprintf(buf, sizeof(buf), "%d", k);

				if (loader.get_token_vector(buf, &pTok))
				{
					std::string& name = pTok->at(0);
					uint32_t dwVnum = 0;

					if (!vnum_by_original_name(name.c_str(), dwVnum))
					{
						str_to_number(dwVnum, name.c_str());
						if (!item_manager::instance().table(dwVnum))
						{
							metin2::log::error("ReadDropItemGroup : there is no item {} : node {}", name.c_str(), stName.c_str());
							return false;
						}
					}

					int iCount = 0;
					str_to_number(iCount, pTok->at(1).c_str());

					if (iCount < 1)
					{
						metin2::log::error("ReadMonsterDropItemGroup : there is no count for item {} : node {}", name.c_str(), stName.c_str());
						return false;
					}

					float fPercent = atof(pTok->at(2).c_str());

					uint32_t dwPct = (uint32_t)(10000.0f * fPercent);

					metin2::log::info("        name {} pct {:d} count {:d}", name.c_str(), dwPct, iCount);
					pkGroup->add_item(dwVnum, dwPct, iCount);

					continue;
				}

				break;
			}

			map_pk_glove_item_group_.insert(std::map<uint32_t, buyer_thief_gloves_item_group*>::value_type(iMobVnum, pkGroup.release()));
		}
		else
		{
			metin2::log::error("ReadMonsterDropItemGroup : Syntax error {} : invalid type {} (kill|drop), node {}", file_name, strType.c_str(), stName.c_str());
			loader.set_parent_node();
			return false;
		}

		loader.set_parent_node();
	}

	return true;
}

auto item_manager::read_drop_item_group(const char * file_name) -> bool
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
		int iMobVnum;

		if (!loader.token_integer("vnum", &iVnum))
		{
			metin2::log::error("ReadDropItemGroup : Syntax error {} : no vnum, node {}", file_name, stName.c_str());
			loader.set_parent_node();
			return false;
		}

		if (!loader.token_integer("mob", &iMobVnum))
		{
			metin2::log::error("ReadDropItemGroup : Syntax error {} : no mob vnum, node {}", file_name, stName.c_str());
			loader.set_parent_node();
			return false;
		}

		metin2::log::info("DROP_ITEM_GROUP {} {:d}", stName.c_str(), iMobVnum);

		token_vector * pTok;

		auto it = map_pk_drop_item_group_.find(iMobVnum);

		std::unique_ptr<drop_item_group> pkNewGroup;
		drop_item_group* pkGroup;

		if (it == map_pk_drop_item_group_.end())
		{
			pkNewGroup = std::make_unique<drop_item_group>();
			pkGroup = pkNewGroup.get();
		}
		else
			pkGroup = it->second;

		for (int k = 1; k < 256; ++k)
		{
			char buf[4];
			snprintf(buf, sizeof(buf), "%d", k);

			if (loader.get_token_vector(buf, &pTok))
			{
				std::string& name = pTok->at(0);
				uint32_t dwVnum = 0;

				if (!vnum_by_original_name(name.c_str(), dwVnum))
				{
					str_to_number(dwVnum, name.c_str());
					if (!item_manager::instance().table(dwVnum))
					{
						metin2::log::error("ReadDropItemGroup : there is no item {} : node {}", name.c_str(), stName.c_str());
						return false;
					}
				}

				float fPercent = atof(pTok->at(1).c_str());

				uint32_t dwPct = (uint32_t)(10000.0f * fPercent);

				int iCount = 1;
				if (pTok->size() > 2)
					str_to_number(iCount, pTok->at(2).c_str());

				if (iCount < 1)
				{
					metin2::log::error("ReadDropItemGroup : there is no count for item {} : node {}", name.c_str(), stName.c_str());
					return false;
				}

				metin2::log::info("        {} {:d} {:d}", name.c_str(), dwPct, iCount);
				pkGroup->add_item(dwVnum, dwPct, iCount);
				continue;
			}

			break;
		}

		if (pkNewGroup)
			map_pk_drop_item_group_.insert(std::map<uint32_t, drop_item_group*>::value_type(iMobVnum, pkNewGroup.release()));

		loader.set_parent_node();
	}

	return true;
}

auto item_manager::read_item_vnum_mask_table(const char * file_name) -> bool
{
	FILE *fp = fopen(file_name, "r");
	if (!fp)
	{
		return false;
	}

	int ori_vnum, new_vnum;
	while (fscanf(fp, "%u %u", &ori_vnum, &new_vnum) != EOF)
	{
		map_new_to_ori_.insert (map_dw2_dw::value_type (new_vnum, ori_vnum));
	}
	fclose(fp);
	return true;
}
}
