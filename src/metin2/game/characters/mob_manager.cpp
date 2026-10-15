#include <metin2/game/characters/mob_manager.h>

#include <metin2/game/characters/char.h>
#include <metin2/game/characters/char_manager.h>
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


CMob::CMob()
{
	memset( &m_table, 0, sizeof(m_table) );

	for( size_t i=0 ; i < MOB_SKILL_MAX_NUM ; ++i )
	{
		m_mobSkillInfo[i].dwSkillVnum = 0;
		m_mobSkillInfo[i].bSkillLevel = 0;
		m_mobSkillInfo[i].vecSplashAttack.clear();
	}
}

void CMob::AddSkillSplash(int iIndex, uint32_t dwTiming, uint32_t dwHitDistance)
{
	if (iIndex >= MOB_SKILL_MAX_NUM || iIndex < 0)
		return;

	metin2::log::info( "MOB_SPLASH {} idx {:d} timing {:d} hit_distance {:d}",
			m_table.szLocaleName, iIndex, dwTiming, dwHitDistance);

	m_mobSkillInfo[iIndex].vecSplashAttack.push_back(TMobSplashAttackInfo(dwTiming, dwHitDistance));
}

CMobInstance::CMobInstance()
	: m_IsBerserk(false), m_IsGodSpeed(false), m_IsRevive(false)
{
	m_dwLastAttackedTime = get_dword_time();
	m_dwLastWarpTime = get_dword_time();

	memset( &m_posLastAttacked, 0, sizeof(m_posLastAttacked) );
}

CMobManager::CMobManager()
{
}

CMobManager::~CMobManager()
{
}

bool CMobManager::Initialize(TMobTable * pTable, int iSize)
{
	m_map_pkMobByVnum.clear();
	m_map_pkMobByName.clear();

	TMobTable * t = pTable;


	for (int i = 0; i < iSize; ++i, ++t)
	{
		CMob * pkMob = new CMob;

		std::memcpy(&pkMob->m_table, t, sizeof(TMobTable));

		m_map_pkMobByVnum.insert(std::map<uint32_t, CMob *>::value_type(t->dwVnum, pkMob));
		m_map_pkMobByName.insert(std::map<std::string, CMob *>::value_type(t->szLocaleName, pkMob));

		int SkillCount = 0;

		for (int j = 0; j < MOB_SKILL_MAX_NUM; ++j)
			if (pkMob->m_table.Skills[j].dwVnum)
				++SkillCount;

		metin2::log::info( "MOB: #{:<5d} {:<30} LEVEL {:d} HP {:d} DEF {:d} EXP {:d} DROP_ITEM_VNUM {:d} SKILL_COUNT {:d}", 
				t->dwVnum, t->szLocaleName, t->bLevel, t->dwMaxHP, t->wDef, t->dwExp, t->dwDropItemVnum, SkillCount);

		if (t->bType == CHAR_TYPE_NPC || t->bType == CHAR_TYPE_WARP || t->bType == CHAR_TYPE_GOTO)
			CHARACTER_MANAGER::instance().RegisterRaceNum(t->dwVnum);

		quest::CQuestManager::instance().RegisterNPCVnum(t->dwVnum);
	}


	// LOCALE_SERVICE
	const int FILE_NAME_LEN = 256;
	char szGroupFileName[FILE_NAME_LEN];
	char szGroupGroupFileName[FILE_NAME_LEN];

	snprintf(szGroupFileName, sizeof(szGroupGroupFileName),
			"%s/group.txt", LocaleService_GetBasePath().c_str());
	snprintf(szGroupGroupFileName, sizeof(szGroupGroupFileName),
			"%s/group_group.txt", LocaleService_GetBasePath().c_str());
	
	if (!LoadGroup(szGroupFileName))
	{
		metin2::log::error("cannot load {}", szGroupFileName);
		thecore_shutdown();
	}
	if (!LoadGroupGroup(szGroupGroupFileName))
	{
		metin2::log::error("cannot load {}", szGroupGroupFileName);
		thecore_shutdown();
	}
	// END_OF_LOCALE_SERVICE

	CHARACTER_MANAGER::instance().for_each_pc(std::bind(&CMobManager::RebindMobProto, this, std::placeholders::_1));
	return true;
}

void CMobManager::RebindMobProto(CHARACTER * ch)
{
	if (ch->IsPC())
		return;

	const CMob * pMob = Get(ch->GetRaceNum());

	if (pMob)
		ch->SetProto(pMob);
}

const CMob * CMobManager::Get(uint32_t dwVnum)
{
	std::map<uint32_t, CMob *>::iterator it = m_map_pkMobByVnum.find(dwVnum);

	if (it == m_map_pkMobByVnum.end())
		return nullptr;

	return it->second;
}

const CMob * CMobManager::Get(const char * c_pszName, bool bIsAbbrev)
{
	std::map<std::string, CMob *>::iterator it;

	if (!bIsAbbrev)
	{
		it = m_map_pkMobByName.find(c_pszName);

		if (it == m_map_pkMobByName.end())
			return nullptr;

		return it->second;
	}

	int len = strlen(c_pszName);
	it = m_map_pkMobByName.begin();

	while (it != m_map_pkMobByName.end())
	{
		if (!strncmp(it->first.c_str(), c_pszName, len))
			return it->second;

		++it;
	}

	return nullptr;
}

void CMobManager::IncRegenCount(uint8_t bRegenType, uint32_t dwVnum, int iCount, int iTime)
{
	switch (bRegenType)
	{
		case REGEN_TYPE_MOB:
			m_mapRegenCount[dwVnum] += iCount * 86400. / iTime;
			break;

		case REGEN_TYPE_GROUP:
			{
				CMobGroup * pkGroup = CMobManager::instance().GetGroup(dwVnum);
				if (!pkGroup)
					return;
				const std::vector<uint32_t> & c_rdwMembers = pkGroup->GetMemberVector();

				for (uint32_t i=0; i<c_rdwMembers.size(); i++)
					m_mapRegenCount[c_rdwMembers[i]] += iCount * 86400. / iTime;
			}
			break;

		case REGEN_TYPE_GROUP_GROUP:
			{
				std::map<uint32_t, CMobGroupGroup *>::iterator it = m_map_pkMobGroupGroup.find(dwVnum);

				if (it == m_map_pkMobGroupGroup.end())
					return;

				std::vector<uint32_t>& v = it->second->m_vec_dwMemberVnum;
				for (uint32_t i=0; i<v.size(); i++)
				{
					CMobGroup * pkGroup = CMobManager::instance().GetGroup(v[i]);
					if (!pkGroup)
						return;
					const std::vector<uint32_t> & c_rdwMembers = pkGroup->GetMemberVector();

					for (uint32_t i=0; i<c_rdwMembers.size(); i++)
						m_mapRegenCount[c_rdwMembers[i]] += iCount * 86400. / iTime / v.size();
				}
			}
			break;
	}
}

void CMobManager::DumpRegenCount(const char* c_szFilename)
{
	FILE* fp = fopen(c_szFilename, "w");

	if (fp)
	{
		std::map<uint32_t, double>::iterator it;

		fprintf(fp,"MOB_VNUM\tCOUNT\n");

		for (it = m_mapRegenCount.begin(); it != m_mapRegenCount.end(); ++it)
		{
			fprintf(fp,"%u\t%g\n", it->first, it->second);
		}

		fclose(fp);
	}
}

uint32_t CMobManager::GetGroupFromGroupGroup(uint32_t dwVnum)
{
	std::map<uint32_t, CMobGroupGroup *>::iterator it = m_map_pkMobGroupGroup.find(dwVnum);

	if (it == m_map_pkMobGroupGroup.end())
		return 0;

	return it->second->GetMember();
}

CMobGroup * CMobManager::GetGroup(uint32_t dwVnum)
{
	std::map<uint32_t, CMobGroup *>::iterator it = m_map_pkMobGroup.find(dwVnum);

	if (it == m_map_pkMobGroup.end())
		return nullptr;

	return it->second;
}

bool CMobManager::LoadGroupGroup(const char * c_pszFileName)
{
	CTextFileLoader loader;

	if (!loader.Load(c_pszFileName))
		return false;

	std::string stName;

	for (uint32_t i = 0; i < loader.GetChildNodeCount(); ++i)
	{
		loader.SetChildNode(i);

		loader.GetCurrentNodeName(&stName);

		int iVnum;

		if (!loader.GetTokenInteger("vnum", &iVnum))
		{
			metin2::log::error("LoadGroupGroup : Syntax error {} : no vnum, node {}", c_pszFileName, stName.c_str());
			loader.SetParentNode();
			continue;
		}

		TTokenVector * pTok;

		CMobGroupGroup * pkGroup = new CMobGroupGroup(iVnum);

		for (int k = 1; k < 256; ++k)
		{
			char buf[4];
			snprintf(buf, sizeof(buf), "%d", k);

			if (loader.GetTokenVector(buf, &pTok))
			{
				uint32_t dwMobVnum = 0;
				str_to_number(dwMobVnum, pTok->at(0).c_str());

				// ADD_MOB_GROUP_GROUP_PROB
				int prob = 1;
				if (pTok->size() > 1)
					str_to_number(prob, pTok->at(1).c_str());
				// END_OF_ADD_MOB_GROUP_GROUP_PROB

				if (dwMobVnum)
					pkGroup->AddMember(dwMobVnum);

				continue;
			}

			break;
		}

		loader.SetParentNode();

		m_map_pkMobGroupGroup.insert(std::make_pair((uint32_t)iVnum, pkGroup));
	}

	return true;
}

bool CMobManager::LoadGroup(const char * c_pszFileName)
{
	CTextFileLoader loader;

	if (!loader.Load(c_pszFileName))
		return false;

	std::string stName;

	for (uint32_t i = 0; i < loader.GetChildNodeCount(); ++i)
	{
		loader.SetChildNode(i);

		loader.GetCurrentNodeName(&stName);

		int iVnum;

		if (!loader.GetTokenInteger("vnum", &iVnum))
		{
			metin2::log::error("LoadGroup : Syntax error {} : no vnum, node {}", c_pszFileName, stName.c_str());
			loader.SetParentNode();
			continue;
		}

		TTokenVector * pTok;

		if (!loader.GetTokenVector("leader", &pTok))
		{
			metin2::log::error("LoadGroup : Syntax error {} : no leader, node {}", c_pszFileName, stName.c_str());
			loader.SetParentNode();
			continue;
		}

		if (pTok->size() < 2)
		{
			metin2::log::error("LoadGroup : Syntax error {} : no leader vnum, node {}", c_pszFileName, stName.c_str());
			loader.SetParentNode();
			continue;
		}

		CMobGroup * pkGroup = new CMobGroup;

		pkGroup->Create(iVnum, stName);
		uint32_t vnum = 0;
		str_to_number(vnum, pTok->at(1).c_str());
		pkGroup->AddMember(vnum);

		metin2::log::info( "GROUP: {:<5d} {}", iVnum, stName.c_str());
		metin2::log::info( "               {} {}", pTok->at(0).c_str(), pTok->at(1).c_str());

		for (int k = 1; k < 256; ++k)
		{
			char buf[4];
			snprintf(buf, sizeof(buf), "%d", k);

			if (loader.GetTokenVector(buf, &pTok))
			{
				metin2::log::info( "               {} {}", pTok->at(0).c_str(), pTok->at(1).c_str());
				uint32_t vnum = 0;
				str_to_number(vnum, pTok->at(1).c_str());
				pkGroup->AddMember(vnum);
				continue;
			}

			break;
		}

		loader.SetParentNode();
		m_map_pkMobGroup.insert(std::map<uint32_t, CMobGroup *>::value_type(iVnum, pkGroup));
	}

	return true;
}
}
