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

// POLYMORPH_BUG_FIX
static float msa_get_normal_attack_duration(const char* msaPath)
{
	float duration = 99.0f;
	FILE * fp = fopen(msaPath, "rt");
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

static float mob_get_normal_attack_duration(mob_table* mobTable)
{
	float minDuration = 99.0f;

	const char * folder = mobTable->folder;

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
// END_OF_POLYMORPH_BUG_FIX

static const char* get_motion_file_name(mob_table* mobTable, public_motion motion)
{
	char buf[1024];
	const char * folder = mobTable->folder;
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
				metin2::log::error("Motion: no process for this motion({:d}) vnum({:d})", motion, mobTable->vnum);
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
		metin2::log::error("Motion: {} have not motlist.txt vnum({:d}) folder({})", folder, mobTable->vnum, mobTable->folder);
	}

	return nullptr;
}

static void load_motion(motion_set* pMotionSet, mob_table* mob_table, public_motion motion)
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
				metin2::log::error("cannot find accumulation data in file '{}'", cpFileName);

		pMotionSet->insert(make_motion_key(MOTION_MODE_GENERAL, motion), std::move(pMotion));
	}
	else
	{
		metin2::log::error("Motion: Load failed vnum({:d}) motion({:d}) file({})", mob_table->vnum, motion, cpFileName);
	}
}

static void load_skill_motion(motion_set* pMotionSet, mob* pMob, public_motion motion)
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

	mob_table* mob_table = &pMob->table;

	if (mob_table->skills[idx].vnum == 0) return;

	const char* cpFileName = get_motion_file_name(mob_table, motion);
	if (cpFileName == nullptr) return;

	auto pMotion = std::make_unique<game::characters::motion>();

	if (pMotion->load_mob_skill_from_file(cpFileName, pMob, idx) == true)
	{
		pMotionSet->insert(make_motion_key(MOTION_MODE_GENERAL, motion), std::move(pMotion));
	}
	else
	{
		if (mob_table->skills[idx].vnum != 0)
		{
			metin2::log::error("Motion: Skill exist but no motion data for index {:d} mob {:d} skill {:d}",
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

const motion_set * motion_manager::get_motion_set(uint32_t dwVnum)
{
	iterator it = map_pk_motion_set_.find(dwVnum);

	if (map_pk_motion_set_.end() == it)
		return nullptr;

	return it->second.get();
}

const motion * motion_manager::get_motion(uint32_t dwVnum, uint32_t dwKey)
{
	const motion_set * pkMotionSet = get_motion_set(dwVnum);

	if (!pkMotionSet)
		return nullptr;

	return pkMotionSet->get_motion(dwKey);
}

float motion_manager::motion_duration(uint32_t dwVnum, uint32_t dwKey)
{
	const motion * pkMotion = get_motion(dwVnum, dwKey);
	return pkMotion ? pkMotion->duration() : 0.0f;
}

// POLYMORPH_BUG_FIX
float	motion_manager::normal_attack_duration(uint32_t dwVnum)
{
	std::map<uint32_t, float>::iterator f = map_normal_attack_duration_.find(dwVnum);
	if (map_normal_attack_duration_.end() == f)
		return 0.0f;
	else
		return f->second;
}
// END_OF_POLYMORPH_BUG_FIX

enum motion_event_type
{
	MOTION_EVENT_TYPE_NONE,					// 0
	MOTION_EVENT_TYPE_EFFECT,				// 1
	MOTION_EVENT_TYPE_SCREEN_WAVING,		// 2
	MOTION_EVENT_TYPE_SCREEN_FLASHING,		// 3
	MOTION_EVENT_TYPE_SPECIAL_ATTACKING,	// 4
	MOTION_EVENT_TYPE_SOUND,				// 5
	MOTION_EVENT_TYPE_FLY,					// 6
	MOTION_EVENT_TYPE_CHARACTER_SHOW,		// 7
	MOTION_EVENT_TYPE_CHARACTER_HIDE,		// 8
	MOTION_EVENT_TYPE_WARP,					// 9
	MOTION_EVENT_TYPE_EFFECT_TO_TARGET,		// 10
	MOTION_EVENT_TYPE_MAX_NUM,				// 11
};

bool motion_manager::build()
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

			// POLYMORPH_BUG_FIX
			float normalAttackDuration = mob_get_normal_attack_duration(t);
			metin2::log::info( "mob_normal_attack_duration:{:d}:{}:{:.2f}", t->vnum, t->folder, normalAttackDuration);
			map_normal_attack_duration_.insert(std::map<uint32_t, float>::value_type(t->vnum, normalAttackDuration));
			// END_OF_POLYMORPH_BUG_FIX
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

const motion * motion_set::get_motion(uint32_t dwKey) const
{
	const_iterator it = map_pk_motion_.find(dwKey);

	if (it == map_pk_motion_.end())
		return nullptr;

	return it->second.get();
}

void motion_set::insert(uint32_t dwKey, std::unique_ptr<motion> pkMotion)
{
	map_pk_motion_.insert(container::value_type(dwKey, std::move(pkMotion)));
}

bool motion_set::load(const char * szFileName, int mode, int motion)
{
	auto pkMotion = std::make_unique<game::characters::motion>();

	if (!pkMotion->load_from_file(szFileName))
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

bool motion::load_mob_skill_from_file(const char * c_pszFileName, mob* pMob, int iSkillIndex)
{
	text_file_loader rkTextFileLoader;
	if (!rkTextFileLoader.load(c_pszFileName))
		return false;

	rkTextFileLoader.set_top();

	if (!rkTextFileLoader.token_float("motionduration", &duration_))
	{
		metin2::log::error("Motion: no motion duration {}", c_pszFileName);
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
					metin2::log::error("Motion: no event data {:d} {}", j, c_pszFileName);
					return false;
				}

				int iType;
				if (!rkTextFileLoader.token_integer("motioneventtype", &iType))
				{
					metin2::log::error("Motion: no motioneventtype data {}", c_pszFileName);
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
						// Assume old data has only one
						if (!rkTextFileLoader.set_child_node("spheredata", 0))
						{
							metin2::log::error("Motion: no sphere data {}", c_pszFileName);
							return false;
						}

						if (!rkTextFileLoader.token_position("position", &v3Position))
						{
							metin2::log::error("Motion: no position data {}", c_pszFileName);
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
					metin2::log::error("Motion: no startingtime data {}", c_pszFileName);
					return false;
				}

				pMob->add_skill_splash(iSkillIndex, 100 + uint32_t(fStartingTime * 1000), -(uint32_t)(v3Position.y));

				rkTextFileLoader.set_parent_node();
			}
		}
		rkTextFileLoader.set_parent_node();
	}

	pMob->mob_skill_info[iSkillIndex].skill_vnum = pMob->table.skills[iSkillIndex].vnum;
	pMob->mob_skill_info[iSkillIndex].skill_level = pMob->table.skills[iSkillIndex].level;
	return true;
}

bool motion::load_from_file(const char * c_pszFileName)
{
	text_file_loader loader;

	if (!loader.load(c_pszFileName))
	{
		metin2::log::info( "Motion: LoadFromFile fail: {}", c_pszFileName);
		return false;
	}

	if (!loader.token_float("motionduration", &duration_))
	{
		metin2::log::error("Motion: {} does not have a duration", c_pszFileName);
		return false;
	}

	if (loader.token_position("accumulation", &vec3_accumulation_))

	metin2::log::debug( "{:<48} {:.3f} {:f}", strchr(c_pszFileName, '/') + 1, duration(), accum_vector().y);

	return true;
}

float motion::duration() const
{
	return duration_;
}

const vector & motion::accum_vector() const
{
	return vec3_accumulation_;
}
}
