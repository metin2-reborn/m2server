#include <metin2/game/characters/motion.h>

#include <metin2/game/characters/char.h>
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
static float MSA_GetNormalAttackDuration(const char* msaPath)
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

static float MOB_GetNormalAttackDuration(TMobTable* mobTable)
{
	float minDuration = 99.0f;

	const char * folder = mobTable->szFolder;

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
			float curDuration = MSA_GetNormalAttackDuration(msaPath);
			if (curDuration < minDuration)
				minDuration = curDuration;
		}
	}
	fclose(fp);

	return minDuration;
}
// END_OF_POLYMORPH_BUG_FIX

static const char* GetMotionFileName(TMobTable* mobTable, EPublicMotion motion)
{
	char buf[1024];
	const char * folder = mobTable->szFolder;
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
				metin2::log::error("Motion: no process for this motion({:d}) vnum({:d})", motion, mobTable->dwVnum);
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
		metin2::log::error("Motion: {} have not motlist.txt vnum({:d}) folder({})", folder, mobTable->dwVnum, mobTable->szFolder);
	}

	return nullptr;
}

static void LoadMotion(CMotionSet* pMotionSet, TMobTable* mob_table, EPublicMotion motion)
{
	const char* cpFileName = GetMotionFileName(mob_table, motion);

	if (cpFileName == nullptr)
	{
		return;
	}

	auto pMotion = std::make_unique<CMotion>();

	if (pMotion->LoadFromFile(cpFileName) == true)
	{
		if (motion == MOTION_RUN)
			if (0.0f == pMotion->GetAccumVector().y)
				metin2::log::error("cannot find accumulation data in file '{}'", cpFileName);

		pMotionSet->Insert(MAKE_MOTION_KEY(MOTION_MODE_GENERAL, motion), std::move(pMotion));
	}
	else
	{
		metin2::log::error("Motion: Load failed vnum({:d}) motion({:d}) file({})", mob_table->dwVnum, motion, cpFileName);
	}
}

static void LoadSkillMotion(CMotionSet* pMotionSet, CMob* pMob, EPublicMotion motion)
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

	TMobTable* mob_table = &pMob->m_table;

	if (mob_table->Skills[idx].dwVnum == 0) return;

	const char* cpFileName = GetMotionFileName(mob_table, motion);
	if (cpFileName == nullptr) return;

	auto pMotion = std::make_unique<CMotion>();

	if (pMotion->LoadMobSkillFromFile(cpFileName, pMob, idx) == true)
	{
		pMotionSet->Insert(MAKE_MOTION_KEY(MOTION_MODE_GENERAL, motion), std::move(pMotion));
	}
	else
	{
		if (mob_table->Skills[idx].dwVnum != 0)
		{
			metin2::log::error("Motion: Skill exist but no motion data for index {:d} mob {:d} skill {:d}",
				   	idx, mob_table->dwVnum, mob_table->Skills[idx].dwVnum);
		}
	}
}

CMotionManager::CMotionManager()
{
}

CMotionManager::~CMotionManager()
{
}

const CMotionSet * CMotionManager::GetMotionSet(uint32_t dwVnum)
{
	iterator it = m_map_pkMotionSet.find(dwVnum);

	if (m_map_pkMotionSet.end() == it)
		return nullptr;

	return it->second.get();
}

const CMotion * CMotionManager::GetMotion(uint32_t dwVnum, uint32_t dwKey)
{
	const CMotionSet * pkMotionSet = GetMotionSet(dwVnum);

	if (!pkMotionSet)
		return nullptr;

	return pkMotionSet->GetMotion(dwKey);
}

float CMotionManager::GetMotionDuration(uint32_t dwVnum, uint32_t dwKey)
{
	const CMotion * pkMotion = GetMotion(dwVnum, dwKey);
	return pkMotion ? pkMotion->GetDuration() : 0.0f;
}

// POLYMORPH_BUG_FIX
float	CMotionManager::GetNormalAttackDuration(uint32_t dwVnum)
{
	std::map<uint32_t, float>::iterator f = m_map_normalAttackDuration.find(dwVnum);
	if (m_map_normalAttackDuration.end() == f)
		return 0.0f;
	else
		return f->second;
}
// END_OF_POLYMORPH_BUG_FIX

enum EMotionEventType
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

bool CMotionManager::Build()
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
		auto pkMotionSetOwner = std::make_unique<CMotionSet>();
		CMotionSet * pkMotionSet = pkMotionSetOwner.get();
		m_map_pkMotionSet.try_emplace(i, std::move(pkMotionSetOwner));

		char sz[256];

		snprintf(sz, sizeof(sz), "%s/general/run.msa", c_apszFolderName[i]);
		pkMotionSet->Load(sz, MOTION_MODE_GENERAL, MOTION_RUN);
		snprintf(sz, sizeof(sz), "%s/general/walk.msa", c_apszFolderName[i]);
		pkMotionSet->Load(sz, MOTION_MODE_GENERAL, MOTION_WALK);

		snprintf(sz, sizeof(sz), "%s/twohand_sword/run.msa", c_apszFolderName[i]);
		pkMotionSet->Load(sz, MOTION_MODE_TWOHAND_SWORD, MOTION_RUN);
		snprintf(sz, sizeof(sz), "%s/twohand_sword/walk.msa", c_apszFolderName[i]);
		pkMotionSet->Load(sz, MOTION_MODE_TWOHAND_SWORD, MOTION_WALK);

		snprintf(sz, sizeof(sz), "%s/onehand_sword/run.msa", c_apszFolderName[i]);
		pkMotionSet->Load(sz, MOTION_MODE_ONEHAND_SWORD, MOTION_RUN);
		snprintf(sz, sizeof(sz), "%s/onehand_sword/walk.msa", c_apszFolderName[i]);
		pkMotionSet->Load(sz, MOTION_MODE_ONEHAND_SWORD, MOTION_WALK);

		snprintf(sz, sizeof(sz), "%s/dualhand_sword/run.msa", c_apszFolderName[i]);
		pkMotionSet->Load(sz, MOTION_MODE_DUALHAND_SWORD, MOTION_RUN);
		snprintf(sz, sizeof(sz), "%s/dualhand_sword/walk.msa", c_apszFolderName[i]);
		pkMotionSet->Load(sz, MOTION_MODE_DUALHAND_SWORD, MOTION_WALK);

		snprintf(sz, sizeof(sz), "%s/bow/run.msa", c_apszFolderName[i]);
		pkMotionSet->Load(sz, MOTION_MODE_BOW, MOTION_RUN);
		snprintf(sz, sizeof(sz), "%s/bow/walk.msa", c_apszFolderName[i]);
		pkMotionSet->Load(sz, MOTION_MODE_BOW, MOTION_WALK);

		snprintf(sz, sizeof(sz), "%s/bell/run.msa", c_apszFolderName[i]);
		pkMotionSet->Load(sz, MOTION_MODE_BELL, MOTION_RUN);
		snprintf(sz, sizeof(sz), "%s/bell/walk.msa", c_apszFolderName[i]);
		pkMotionSet->Load(sz, MOTION_MODE_BELL, MOTION_WALK);

		snprintf(sz, sizeof(sz), "%s/fan/run.msa", c_apszFolderName[i]);
		pkMotionSet->Load(sz, MOTION_MODE_FAN, MOTION_RUN);
		snprintf(sz, sizeof(sz), "%s/fan/walk.msa", c_apszFolderName[i]);
		pkMotionSet->Load(sz, MOTION_MODE_FAN, MOTION_WALK);

		snprintf(sz, sizeof(sz), "%s/horse/run.msa", c_apszFolderName[i]);
		pkMotionSet->Load(sz, MOTION_MODE_HORSE, MOTION_RUN);
		snprintf(sz, sizeof(sz), "%s/horse/walk.msa", c_apszFolderName[i]);
		pkMotionSet->Load(sz, MOTION_MODE_HORSE, MOTION_WALK);
	}

	CMobManager::iterator it = CMobManager::instance().begin();

	while (it != CMobManager::instance().end())
	{
		CMob * pkMob = (it++)->second;
		TMobTable * t = &pkMob->m_table;

		if ('\0' != t->szFolder[0])
		{
			auto pkMotionSetOwner = std::make_unique<CMotionSet>();
			CMotionSet * pkMotionSet = pkMotionSetOwner.get();
			m_map_pkMotionSet.try_emplace(t->dwVnum, std::move(pkMotionSetOwner));

			LoadMotion(pkMotionSet, t, MOTION_WALK);
			LoadMotion(pkMotionSet, t, MOTION_RUN);
			LoadMotion(pkMotionSet, t, MOTION_NORMAL_ATTACK);

			LoadSkillMotion(pkMotionSet, pkMob, MOTION_SPECIAL_1);
			LoadSkillMotion(pkMotionSet, pkMob, MOTION_SPECIAL_2);
			LoadSkillMotion(pkMotionSet, pkMob, MOTION_SPECIAL_3);
			LoadSkillMotion(pkMotionSet, pkMob, MOTION_SPECIAL_4);
			LoadSkillMotion(pkMotionSet, pkMob, MOTION_SPECIAL_5);

			// POLYMORPH_BUG_FIX
			float normalAttackDuration = MOB_GetNormalAttackDuration(t);
			metin2::log::info( "mob_normal_attack_duration:{:d}:{}:{:.2f}", t->dwVnum, t->szFolder, normalAttackDuration);
			m_map_normalAttackDuration.insert(std::map<uint32_t, float>::value_type(t->dwVnum, normalAttackDuration));
			// END_OF_POLYMORPH_BUG_FIX
		}
	}

	return true;
}

CMotionSet::CMotionSet()
{
}

CMotionSet::~CMotionSet()
{
}

const CMotion * CMotionSet::GetMotion(uint32_t dwKey) const
{
	const_iterator it = m_map_pkMotion.find(dwKey);

	if (it == m_map_pkMotion.end())
		return nullptr;

	return it->second.get();
}

void CMotionSet::Insert(uint32_t dwKey, std::unique_ptr<CMotion> pkMotion)
{
	m_map_pkMotion.insert(TContainer::value_type(dwKey, std::move(pkMotion)));
}

bool CMotionSet::Load(const char * szFileName, int mode, int motion)
{
	auto pkMotion = std::make_unique<CMotion>();

	if (!pkMotion->LoadFromFile(szFileName))
		return false;

	Insert(MAKE_MOTION_KEY(mode, motion), std::move(pkMotion));

	return true;
}

CMotion::CMotion() : m_fDuration(0.0f)
{
	m_vec3Accumulation.x = 0.0f;
	m_vec3Accumulation.y = 0.0f;
	m_vec3Accumulation.z = 0.0f;
}  

CMotion::~CMotion()
{
}

bool CMotion::LoadMobSkillFromFile(const char * c_pszFileName, CMob* pMob, int iSkillIndex)
{
	CTextFileLoader rkTextFileLoader;
	if (!rkTextFileLoader.Load(c_pszFileName))
		return false;

	rkTextFileLoader.SetTop();

	if (!rkTextFileLoader.GetTokenFloat("motionduration", &m_fDuration))
	{
		metin2::log::error("Motion: no motion duration {}", c_pszFileName);
		return false;
	}

	std::string strNodeName;
	for (uint32_t i = 0; i < rkTextFileLoader.GetChildNodeCount(); ++i)
	{
		rkTextFileLoader.SetChildNode(i);

		rkTextFileLoader.GetCurrentNodeName(&strNodeName);

		if (0 == strNodeName.compare("motioneventdata"))
		{
			uint32_t dwMotionEventDataCount;

			if (!rkTextFileLoader.GetTokenDoubleWord("motioneventdatacount", &dwMotionEventDataCount))
				continue;

			for (uint32_t j = 0; j < dwMotionEventDataCount; ++j)
			{
				if (!rkTextFileLoader.SetChildNode("event", j))
				{
					metin2::log::error("Motion: no event data {:d} {}", j, c_pszFileName);
					return false;
				}

				int iType;
				if (!rkTextFileLoader.GetTokenInteger("motioneventtype", &iType))
				{
					metin2::log::error("Motion: no motioneventtype data {}", c_pszFileName);
					return false;
				}

				VECTOR v3Position;

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
						rkTextFileLoader.SetParentNode();
						continue;

					case MOTION_EVENT_TYPE_SPECIAL_ATTACKING:
						// Assume old data has only one
						if (!rkTextFileLoader.SetChildNode("spheredata", 0))
						{
							metin2::log::error("Motion: no sphere data {}", c_pszFileName);
							return false;
						}

						if (!rkTextFileLoader.GetTokenPosition("position", &v3Position))
						{
							metin2::log::error("Motion: no position data {}", c_pszFileName);
							return false;
						}

						rkTextFileLoader.SetParentNode();
						break;
					default:
						assert(!" CRaceMotionData::LoadMotionData - Strange Event Type");
						return false;
						break;
				}

				float fStartingTime;

				if (!rkTextFileLoader.GetTokenFloat("startingtime", &fStartingTime))
				{
					metin2::log::error("Motion: no startingtime data {}", c_pszFileName);
					return false;
				}

				pMob->AddSkillSplash(iSkillIndex, 100 + uint32_t(fStartingTime * 1000), -(uint32_t)(v3Position.y));

				rkTextFileLoader.SetParentNode();
			}
		}
		rkTextFileLoader.SetParentNode();
	}

	pMob->m_mobSkillInfo[iSkillIndex].dwSkillVnum = pMob->m_table.Skills[iSkillIndex].dwVnum;
	pMob->m_mobSkillInfo[iSkillIndex].bSkillLevel = pMob->m_table.Skills[iSkillIndex].bLevel;
	return true;
}

bool CMotion::LoadFromFile(const char * c_pszFileName)
{
	CTextFileLoader loader;

	if (!loader.Load(c_pszFileName))
	{
		metin2::log::info( "Motion: LoadFromFile fail: {}", c_pszFileName);
		return false;
	}

	if (!loader.GetTokenFloat("motionduration", &m_fDuration))
	{
		metin2::log::error("Motion: {} does not have a duration", c_pszFileName);
		return false;
	}

	if (loader.GetTokenPosition("accumulation", &m_vec3Accumulation))

	metin2::log::debug( "{:<48} {:.3f} {:f}", strchr(c_pszFileName, '/') + 1, GetDuration(), GetAccumVector().y);

	return true;
}

float CMotion::GetDuration() const
{
	return m_fDuration;
}

const VECTOR & CMotion::GetAccumVector() const
{
	return m_vec3Accumulation;
}
}
