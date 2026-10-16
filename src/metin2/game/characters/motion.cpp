#include <metin2/game/characters/motion.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/data/text_file_loader.h>
#include <metin2/game/world/vector.h>

#include <metin2/proto/tables.h>
#include <metin2/log/log.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include <strings.h>

namespace metin2::game::inline characters {

static auto msa_get_normal_attack_duration(const char* msa_path) -> float
{
	float duration = 99.0f;
	FILE * fp = fopen(msa_path, "rt");
	if (!fp)
		return duration;

	char line[1024];
	while (fgets(line, sizeof(line), fp))
	{
		char key[1024];
		char val[1024];

		sscanf(line, "%s %s", key, val);
		if (strcmp(key, "MotionDuration") == 0)
		{
			duration = atof(val);
			break;
		}
	}
	fclose(fp);

	return duration;
}

static auto mob_get_normal_attack_duration(mob_table* mob_table_value) -> float
{
	float minDuration = 99.0f;

	const char * folder = mob_table_value->folder;

	char motlistPath[1024];
	snprintf(motlistPath, sizeof(motlistPath), "data/monster/%s/motlist.txt", folder);

	FILE * fp = fopen(motlistPath, "rt");
	if (!fp)
		return minDuration;

	char line[1024];
	while (fgets(line, sizeof(line), fp))
	{
		char mode[1024];
		char type[1024];
		char msaName[1024];
		int percent;

		sscanf(line, "%s %s %s %d", mode, type, msaName, &percent);
		if (strcmp(mode, "GENERAL") == 0 && strncmp(type, "NORMAL_ATTACK", 13) == 0)
		{
			char msaPath[1024];
			snprintf(msaPath, sizeof(msaPath), "data/monster/%s/%s", folder, msaName);
			float curDuration = msa_get_normal_attack_duration(msaPath);
			if (curDuration < minDuration)
				minDuration = curDuration;
		}
	}
	fclose(fp);

	return minDuration;
}

static auto get_motion_file_name(mob_table* mob_table_value, public_motion motion) -> const char*
{
	char buf[1024];
	const char * folder = mob_table_value->folder;
	snprintf(buf, sizeof(buf), "data/monster/%s/motlist.txt", folder);

	FILE * fp = fopen(buf, "rt");
	char * v[4];

	if (fp != nullptr)
	{
		const char* field = nullptr;

		switch (motion)
		{
			case MOTION_WALK				: field = "WALK"; break;
			case MOTION_RUN					: field = "RUN"; break;
			case MOTION_NORMAL_ATTACK		: field = "NORMAL_ATTACK"; break;
			case MOTION_SPECIAL_1			: field = "SPECIAL"; break;
			case MOTION_SPECIAL_2			: field = "SPECIAL1"; break;
			case MOTION_SPECIAL_3			: field = "SPECIAL2"; break;
			case MOTION_SPECIAL_4			: field = "SPECIAL3"; break;
			case MOTION_SPECIAL_5			: field = "SPECIAL4"; break;

			default:
				fclose(fp);
				metin2::log::error("No handler for motion {:d} (mob vnum {:d})", motion, mob_table_value->vnum);
				return nullptr;
		}

		while (fgets(buf, 1024, fp))
		{
			v[0] = strtok(buf,  " \t\r\n");
			v[1] = strtok(nullptr, " \t\r\n");
			v[2] = strtok(nullptr, " \t\r\n");
			v[3] = strtok(nullptr, " \t\r\n");

			if (nullptr != v[0] && nullptr != v[1] && nullptr != v[2] && nullptr != v[3] && !strcasecmp(v[1], field))
			{
				fclose(fp);

				static std::string str;
				str = "data/monster/";
				str += folder;
				str += "/";
				str += v[2];

				return str.c_str();
			}
		}

		fclose(fp);
	}
	else
	{
		metin2::log::error("No motlist.txt in {} for mob vnum {:d} (folder {})", folder, mob_table_value->vnum, mob_table_value->folder);
	}

	return nullptr;
}

static void load_motion(motion_set* motion_set_value, mob_table* mob_table, public_motion motion)
{
	const char* cpFileName = get_motion_file_name(mob_table, motion);

	if (cpFileName == nullptr)
	{
		return;
	}

	auto pMotion = std::make_unique<game::characters::motion>();

	if (pMotion->load_from_file(cpFileName) == true)
	{
		if (motion == MOTION_RUN)
			if (0.0f == pMotion->accum_vector().y)
				metin2::log::warn("No accumulation data in run motion file {}", cpFileName);

		motion_set_value->insert(make_motion_key(MOTION_MODE_GENERAL, motion), std::move(pMotion));
	}
	else
	{
		metin2::log::error("Cannot load motion for mob vnum {:d} (motion {:d}) from {}", mob_table->vnum, motion, cpFileName);
	}
}

static void load_skill_motion(motion_set* motion_set_value, mob* mob_value, public_motion motion)
{
	int idx = 0;

	switch (motion)
	{
		case MOTION_SPECIAL_1 : idx = 0; break;
		case MOTION_SPECIAL_2 : idx = 1; break;
		case MOTION_SPECIAL_3 : idx = 2; break;
		case MOTION_SPECIAL_4 : idx = 3; break;
		case MOTION_SPECIAL_5 : idx = 4; break;

		default :
			return;					
	}

	mob_table* mob_table = &mob_value->table;

	if (mob_table->skills[idx].vnum == 0) return;

	const char* cpFileName = get_motion_file_name(mob_table, motion);
	if (cpFileName == nullptr) return;

	auto pMotion = std::make_unique<game::characters::motion>();

	if (pMotion->load_mob_skill_from_file(cpFileName, mob_value, idx) == true)
	{
		motion_set_value->insert(make_motion_key(MOTION_MODE_GENERAL, motion), std::move(pMotion));
	}
	else
	{
		if (mob_table->skills[idx].vnum != 0)
		{
			metin2::log::error("No motion data for skill index {:d} of mob {:d} (skill {:d})",
				   	idx, mob_table->vnum, mob_table->skills[idx].vnum);
		}
	}
}

motion_manager::motion_manager()
{
}

motion_manager::~motion_manager()
{
}

auto motion_manager::get_motion_set(uint32_t vnum) -> const motion_set *
{
	iterator it = map_pk_motion_set_.find(vnum);

	if (map_pk_motion_set_.end() == it)
		return nullptr;

	return it->second.get();
}

auto motion_manager::get_motion(uint32_t vnum, uint32_t key) -> const motion *
{
	const motion_set * pkMotionSet = get_motion_set(vnum);

	if (!pkMotionSet)
		return nullptr;

	return pkMotionSet->get_motion(key);
}

auto motion_manager::motion_duration(uint32_t vnum, uint32_t key) -> float
{
	const motion * pkMotion = get_motion(vnum, key);
	return pkMotion ? pkMotion->duration() : 0.0f;
}

auto	motion_manager::normal_attack_duration(uint32_t vnum) -> float
{
	std::map<uint32_t, float>::iterator f = map_normal_attack_duration_.find(vnum);
	if (map_normal_attack_duration_.end() == f)
		return 0.0f;
	else
		return f->second;
}

enum motion_event_type
{
	MOTION_EVENT_TYPE_NONE,
	MOTION_EVENT_TYPE_EFFECT,
	MOTION_EVENT_TYPE_SCREEN_WAVING,
	MOTION_EVENT_TYPE_SCREEN_FLASHING,
	MOTION_EVENT_TYPE_SPECIAL_ATTACKING,
	MOTION_EVENT_TYPE_SOUND,
	MOTION_EVENT_TYPE_FLY,
	MOTION_EVENT_TYPE_CHARACTER_SHOW,
	MOTION_EVENT_TYPE_CHARACTER_HIDE,
	MOTION_EVENT_TYPE_WARP,
	MOTION_EVENT_TYPE_EFFECT_TO_TARGET,
	MOTION_EVENT_TYPE_MAX_NUM,
};

auto motion_manager::build() -> bool
{
	const char * c_apszFolderName[MAIN_RACE_MAX_NUM] =
	{
		"data/pc/warrior",
		"data/pc/assassin",
		"data/pc/sura",
		"data/pc/shaman",
		"data/pc2/warrior",
		"data/pc2/assassin",
		"data/pc2/sura",
		"data/pc2/shaman"
	};
	
	for (int i = 0; i < MAIN_RACE_MAX_NUM; ++i)
	{
		auto pkMotionSetOwner = std::make_unique<motion_set>();
		motion_set * pkMotionSet = pkMotionSetOwner.get();
		map_pk_motion_set_.try_emplace(i, std::move(pkMotionSetOwner));

		char sz[256];

		snprintf(sz, sizeof(sz), "%s/general/run.msa", c_apszFolderName[i]);
		pkMotionSet->load(sz, MOTION_MODE_GENERAL, MOTION_RUN);
		snprintf(sz, sizeof(sz), "%s/general/walk.msa", c_apszFolderName[i]);
		pkMotionSet->load(sz, MOTION_MODE_GENERAL, MOTION_WALK);

		snprintf(sz, sizeof(sz), "%s/twohand_sword/run.msa", c_apszFolderName[i]);
		pkMotionSet->load(sz, MOTION_MODE_TWOHAND_SWORD, MOTION_RUN);
		snprintf(sz, sizeof(sz), "%s/twohand_sword/walk.msa", c_apszFolderName[i]);
		pkMotionSet->load(sz, MOTION_MODE_TWOHAND_SWORD, MOTION_WALK);

		snprintf(sz, sizeof(sz), "%s/onehand_sword/run.msa", c_apszFolderName[i]);
		pkMotionSet->load(sz, MOTION_MODE_ONEHAND_SWORD, MOTION_RUN);
		snprintf(sz, sizeof(sz), "%s/onehand_sword/walk.msa", c_apszFolderName[i]);
		pkMotionSet->load(sz, MOTION_MODE_ONEHAND_SWORD, MOTION_WALK);

		snprintf(sz, sizeof(sz), "%s/dualhand_sword/run.msa", c_apszFolderName[i]);
		pkMotionSet->load(sz, MOTION_MODE_DUALHAND_SWORD, MOTION_RUN);
		snprintf(sz, sizeof(sz), "%s/dualhand_sword/walk.msa", c_apszFolderName[i]);
		pkMotionSet->load(sz, MOTION_MODE_DUALHAND_SWORD, MOTION_WALK);

		snprintf(sz, sizeof(sz), "%s/bow/run.msa", c_apszFolderName[i]);
		pkMotionSet->load(sz, MOTION_MODE_BOW, MOTION_RUN);
		snprintf(sz, sizeof(sz), "%s/bow/walk.msa", c_apszFolderName[i]);
		pkMotionSet->load(sz, MOTION_MODE_BOW, MOTION_WALK);

		snprintf(sz, sizeof(sz), "%s/bell/run.msa", c_apszFolderName[i]);
		pkMotionSet->load(sz, MOTION_MODE_BELL, MOTION_RUN);
		snprintf(sz, sizeof(sz), "%s/bell/walk.msa", c_apszFolderName[i]);
		pkMotionSet->load(sz, MOTION_MODE_BELL, MOTION_WALK);

		snprintf(sz, sizeof(sz), "%s/fan/run.msa", c_apszFolderName[i]);
		pkMotionSet->load(sz, MOTION_MODE_FAN, MOTION_RUN);
		snprintf(sz, sizeof(sz), "%s/fan/walk.msa", c_apszFolderName[i]);
		pkMotionSet->load(sz, MOTION_MODE_FAN, MOTION_WALK);

		snprintf(sz, sizeof(sz), "%s/horse/run.msa", c_apszFolderName[i]);
		pkMotionSet->load(sz, MOTION_MODE_HORSE, MOTION_RUN);
		snprintf(sz, sizeof(sz), "%s/horse/walk.msa", c_apszFolderName[i]);
		pkMotionSet->load(sz, MOTION_MODE_HORSE, MOTION_WALK);
	}

	mob_manager::iterator it = mob_manager::instance().begin();

	while (it != mob_manager::instance().end())
	{
		mob * pkMob = (it++)->second;
		mob_table * t = &pkMob->table;

		if ('\0' != t->folder[0])
		{
			auto pkMotionSetOwner = std::make_unique<motion_set>();
			motion_set * pkMotionSet = pkMotionSetOwner.get();
			map_pk_motion_set_.try_emplace(t->vnum, std::move(pkMotionSetOwner));

			load_motion(pkMotionSet, t, MOTION_WALK);
			load_motion(pkMotionSet, t, MOTION_RUN);
			load_motion(pkMotionSet, t, MOTION_NORMAL_ATTACK);

			load_skill_motion(pkMotionSet, pkMob, MOTION_SPECIAL_1);
			load_skill_motion(pkMotionSet, pkMob, MOTION_SPECIAL_2);
			load_skill_motion(pkMotionSet, pkMob, MOTION_SPECIAL_3);
			load_skill_motion(pkMotionSet, pkMob, MOTION_SPECIAL_4);
			load_skill_motion(pkMotionSet, pkMob, MOTION_SPECIAL_5);

			float normalAttackDuration = mob_get_normal_attack_duration(t);
			metin2::log::debug("Mob {:d} ({}): normal attack duration {:.2f}", t->vnum, t->folder, normalAttackDuration);
			map_normal_attack_duration_.insert(std::map<uint32_t, float>::value_type(t->vnum, normalAttackDuration));
		}
	}

	return true;
}

motion_set::motion_set()
{
}

motion_set::~motion_set()
{
}

auto motion_set::get_motion(uint32_t key) const -> const motion *
{
	const_iterator it = map_pk_motion_.find(key);

	if (it == map_pk_motion_.end())
		return nullptr;

	return it->second.get();
}

void motion_set::insert(uint32_t key, std::unique_ptr<motion> motion_value)
{
	map_pk_motion_.insert(container::value_type(key, std::move(motion_value)));
}

auto motion_set::load(const char * file_name, int mode, int motion) -> bool
{
	auto pkMotion = std::make_unique<game::characters::motion>();

	if (!pkMotion->load_from_file(file_name))
		return false;

	insert(make_motion_key(mode, motion), std::move(pkMotion));

	return true;
}

motion::motion() : duration_(0.0f)
{
	vec3_accumulation_.x = 0.0f;
	vec3_accumulation_.y = 0.0f;
	vec3_accumulation_.z = 0.0f;
}  

motion::~motion()
{
}

auto motion::load_mob_skill_from_file(const char * file_name, mob* mob_value, int skill_index) -> bool
{
	text_file_loader rkTextFileLoader;
	if (!rkTextFileLoader.load(file_name))
		return false;

	rkTextFileLoader.set_top();

	if (!rkTextFileLoader.token_float("motionduration", &duration_))
	{
		metin2::log::error("Motion file {} has no duration", file_name);
		return false;
	}

	std::string strNodeName;
	for (uint32_t i = 0; i < rkTextFileLoader.child_node_count(); ++i)
	{
		rkTextFileLoader.set_child_node(i);

		rkTextFileLoader.current_node_name(&strNodeName);

		if (0 == strNodeName.compare("motioneventdata"))
		{
			uint32_t dwMotionEventDataCount;

			if (!rkTextFileLoader.token_double_word("motioneventdatacount", &dwMotionEventDataCount))
				continue;

			for (uint32_t j = 0; j < dwMotionEventDataCount; ++j)
			{
				if (!rkTextFileLoader.set_child_node("event", j))
				{
					metin2::log::error("Motion event {:d} has no data in {}", j, file_name);
					return false;
				}

				int iType;
				if (!rkTextFileLoader.token_integer("motioneventtype", &iType))
				{
					metin2::log::error("Motion file {} has no event type", file_name);
					return false;
				}

				vector v3Position;

				switch (iType)
				{
					case MOTION_EVENT_TYPE_FLY:
					case MOTION_EVENT_TYPE_EFFECT:
					case MOTION_EVENT_TYPE_SCREEN_WAVING:
					case MOTION_EVENT_TYPE_SOUND:
					case MOTION_EVENT_TYPE_CHARACTER_SHOW:
					case MOTION_EVENT_TYPE_CHARACTER_HIDE:
					case MOTION_EVENT_TYPE_WARP:
					case MOTION_EVENT_TYPE_EFFECT_TO_TARGET:
						rkTextFileLoader.set_parent_node();
						continue;

					case MOTION_EVENT_TYPE_SPECIAL_ATTACKING:
						if (!rkTextFileLoader.set_child_node("spheredata", 0))
						{
							metin2::log::error("Motion file {} has no sphere data", file_name);
							return false;
						}

						if (!rkTextFileLoader.token_position("position", &v3Position))
						{
							metin2::log::error("Motion file {} has no position data", file_name);
							return false;
						}

						rkTextFileLoader.set_parent_node();
						break;
					default:
						assert(!" CRaceMotionData::LoadMotionData - Strange Event Type");
						return false;
						break;
				}

				float fStartingTime;

				if (!rkTextFileLoader.token_float("startingtime", &fStartingTime))
				{
					metin2::log::error("Motion file {} has no starting time", file_name);
					return false;
				}

				mob_value->add_skill_splash(skill_index, 100 + uint32_t(fStartingTime * 1000), -(uint32_t)(v3Position.y));

				rkTextFileLoader.set_parent_node();
			}
		}
		rkTextFileLoader.set_parent_node();
	}

	mob_value->mob_skill_info[skill_index].skill_vnum = mob_value->table.skills[skill_index].vnum;
	mob_value->mob_skill_info[skill_index].skill_level = mob_value->table.skills[skill_index].level;
	return true;
}

auto motion::load_from_file(const char * file_name) -> bool
{
	text_file_loader loader;

	if (!loader.load(file_name))
	{
		metin2::log::debug("Cannot load motion file {}", file_name);
		return false;
	}

	if (!loader.token_float("motionduration", &duration_))
	{
		metin2::log::error("Motion file {} has no duration", file_name);
		return false;
	}

	if (loader.token_position("accumulation", &vec3_accumulation_))

	metin2::log::debug("Motion {}: duration {:.3f}, accumulation y {:f}", strchr(file_name, '/') + 1, duration(), accum_vector().y);

	return true;
}

auto motion::duration() const -> float
{
	return duration_;
}

auto motion::accum_vector() const -> const vector &
{
	return vec3_accumulation_;
}
}
