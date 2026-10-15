#include "threeway_war.h"

#include "char.h"
#include "char_manager.h"
#include "cmd.h"
#include "config.h"
#include "event.h"
#include "locale.hpp"
#include "locale_service.h"
#include "log.h"
#include "packet.h"
#include "questlua.h"
#include "questmanager.h"
#include "regen.h"
#include "sectree_manager.h"
#include "start_position.h"
#include "typedef.h"

#include <common/length.h>
#include <metin2/log/log.h>
#include <libthecore/utils.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>


EVENTINFO(regen_mob_event_info)
{
	uint32_t dwMapIndex;

	regen_mob_event_info() 
	: dwMapIndex( 0 )
	{
	}
};

EVENTFUNC(regen_mob_event)
{
	regen_mob_event_info * info = dynamic_cast<regen_mob_event_info *>(event->info);

	if ( info == nullptr )
	{
		metin2::log::error( "regen_mob_event> <Factor> Null pointer" );
		return 0;
	}

	int iMapIndex = info->dwMapIndex;
	
	std::string filename;
	std::string szFilename(GetSungziMapPath());


	int choice = quest::CQuestManager::instance().GetEventFlag("threeway_war_choice");
	if (0 == choice)
		filename = szFilename + "regen00.txt";
	else
	{
		filename = szFilename + "regen00_" + std::to_string(choice) + ".txt";
	}

	SECTREE_MAP * pkMap = SECTREE_MANAGER::instance().GetMap(iMapIndex);

	if (nullptr != pkMap)
	{
		// A missing regen00_<choice>.txt falls back to regen00.txt.
		if (!regen_load_in_file(filename.c_str(), iMapIndex, pkMap->m_setting.iBaseX, pkMap->m_setting.iBaseY) && 0 != choice)
		{
			filename = szFilename + "regen00.txt";
			regen_load_in_file(filename.c_str(), iMapIndex, pkMap->m_setting.iBaseX, pkMap->m_setting.iBaseY);
		}
	}

	return 0;
}

CThreeWayWar::CThreeWayWar()
{
	Initialize();
}

CThreeWayWar::~CThreeWayWar()
{
	RegisterUserMap_.clear();
	ReviveTokenMap_.clear();
}

void CThreeWayWar::Initialize()
{
	RegenFlag_ = 0;

	memset( KillScore_, 0, sizeof(KillScore_) );

	RegisterUserMap_.clear();
	ReviveTokenMap_.clear();
}

int CThreeWayWar::GetKillScore( uint8_t empire ) const
{
	if( empire <= 0 || empire >= EMPIRE_MAX_NUM )
	{
		metin2::log::error("ThreeWayWar::GetKillScore Wrong Empire variable");
		return 0;
	}

	return KillScore_[empire-1];
}

void CThreeWayWar::SetKillScore( uint8_t empire, int count )
{
	if( empire <= 0 || empire >= EMPIRE_MAX_NUM )
	{
		metin2::log::error("ThreeWayWar::SetKillScore Wrong Empire variable");
		return;
	}

	KillScore_[empire-1] = count;
}

void CThreeWayWar::SetReviveTokenForPlayer(uint32_t PlayerID, int count)
{
	if (0 == PlayerID)
		return;

	ReviveTokenMap_[PlayerID] = count;
}

int CThreeWayWar::GetReviveTokenForPlayer(uint32_t PlayerID)
{
	if (0 == PlayerID)
		return 0;

	return ReviveTokenMap_[PlayerID];
}

void CThreeWayWar::DecreaseReviveTokenForPlayer(uint32_t PlayerID)
{
	if (0 == PlayerID)
		return;

	ReviveTokenMap_[PlayerID] = ReviveTokenMap_[PlayerID] - 1;
}

bool CThreeWayWar::LoadSetting(const char* szFileName)
{
	char szPath[1024];
	snprintf( szPath, sizeof(szPath), "%s/%s", LocaleService_GetBasePath().c_str(), szFileName);

	FILE* pf = fopen( szPath, "r" );

	if (nullptr == pf)
	{
		metin2::log::error("[INIT_FORKED] Do not open file ({})", szPath );
		return false;
	}

	char szLine[256];
	char szSungziName[128];
	char szPassName[3][128];

	while( nullptr != fgets(szLine, 256, pf) )
	{
		if (0 == strncmp(szLine, "sungzi:", 7))
		{
			struct ForkedSungziMapInfo sungziinfo;

			sscanf( szLine+7, "%d %d %d %d %d %d %d %s %d",
					&sungziinfo.m_iForkedSung,
					&sungziinfo.m_iForkedSungziStartPosition[0][0], &sungziinfo.m_iForkedSungziStartPosition[0][1],
					&sungziinfo.m_iForkedSungziStartPosition[1][0], &sungziinfo.m_iForkedSungziStartPosition[1][1],
					&sungziinfo.m_iForkedSungziStartPosition[2][0], &sungziinfo.m_iForkedSungziStartPosition[2][1],
					szSungziName, &sungziinfo.m_iBossMobVnum);

			sungziinfo.m_stMapName = static_cast<std::string>(szSungziName);

			SungZiInfoMap_.push_back( sungziinfo );

			MapIndexSet_.insert( sungziinfo.m_iForkedSung );
		}
		else if (0 == strncmp(szLine, "pass:", 5))
		{
			struct ForkedPassMapInfo passinfo;

			sscanf( szLine+5, "%d %d %d %s %d %d %d %s %d %d %d %s",
					&passinfo.m_iForkedPass[0],
					&passinfo.m_iForkedPassStartPosition[0][0], &passinfo.m_iForkedPassStartPosition[0][1], szPassName[0],
					&passinfo.m_iForkedPass[1],
					&passinfo.m_iForkedPassStartPosition[1][0], &passinfo.m_iForkedPassStartPosition[1][1], szPassName[1],
					&passinfo.m_iForkedPass[2],
					&passinfo.m_iForkedPassStartPosition[2][0], &passinfo.m_iForkedPassStartPosition[2][1], szPassName[2] );

			passinfo.m_stMapName[0] = static_cast<std::string>(szPassName[0]);
			passinfo.m_stMapName[1] = static_cast<std::string>(szPassName[1]);
			passinfo.m_stMapName[2] = static_cast<std::string>(szPassName[2]);

			PassInfoMap_.push_back( passinfo );

			MapIndexSet_.insert( passinfo.m_iForkedPass[0] );
			MapIndexSet_.insert( passinfo.m_iForkedPass[1] );
			MapIndexSet_.insert( passinfo.m_iForkedPass[2] );
		}
	}

	fclose(pf);

	return true;
}

const ForkedPassMapInfo& CThreeWayWar::GetEventPassMapInfo() const
{
	const size_t idx = quest::CQuestManager::instance().GetEventFlag( "threeway_war_pass_idx" );

	return PassInfoMap_[idx];
}

const ForkedSungziMapInfo& CThreeWayWar::GetEventSungZiMapInfo() const
{
	const size_t idx = quest::CQuestManager::instance().GetEventFlag( "threeway_war_sungzi_idx" );

	return SungZiInfoMap_[idx];
}

bool CThreeWayWar::IsThreeWayWarMapIndex(int iMapIndex) const
{
	return MapIndexSet_.find(iMapIndex) != MapIndexSet_.end();
}

bool CThreeWayWar::IsSungZiMapIndex(int iMapIndex) const
{
	std::vector<ForkedSungziMapInfo>::const_iterator it = SungZiInfoMap_.begin();

	for( ; it != SungZiInfoMap_.end() ; ++it )
	{
		if (iMapIndex == it->m_iForkedSung)
		{
			return true;
		}
	}

	return false;
}

void CThreeWayWar::RandomEventMapSet()
{
	const size_t pass_idx = number( 0, PassInfoMap_.size()-1 );
	const size_t sung_idx = number( 0, SungZiInfoMap_.size()-1 );

	quest::CQuestManager::instance().RequestSetEventFlag( "threeway_war_sungzi_idx", sung_idx );
	quest::CQuestManager::instance().RequestSetEventFlag( "threeway_war_pass_idx", pass_idx );
}

bool CThreeWayWar::IsRegisteredUser(uint32_t PlayerID) const
{
	std::unordered_map<uint32_t, uint32_t>::const_iterator iter = RegisterUserMap_.find(PlayerID);

	if (iter == RegisterUserMap_.end())
	{
		return false;
	}

	return true;
}

void CThreeWayWar::RegisterUser(uint32_t PlayerID)
{
	if (0 == PlayerID)
		return;

	RegisterUserMap_.insert( std::make_pair(PlayerID, PlayerID) );
}

int GetKillValue(int level)
{
	int iMinLevelFor1Point, iMaxLevelFor1Point;
	int iMinLevelFor2Point, iMaxLevelFor2Point;
	int iMinLevelFor3Point, iMaxLevelFor3Point;

	iMinLevelFor1Point = 30; iMaxLevelFor1Point = 39;
	iMinLevelFor2Point = 40; iMaxLevelFor2Point = 49;
	iMinLevelFor3Point = 50; iMaxLevelFor3Point = 99;

	if (iMinLevelFor1Point <= level && level <= iMaxLevelFor1Point)
	{
		return 1;
	}
	else if (iMinLevelFor2Point <= level && level <= iMaxLevelFor2Point)
	{
		return 2;
	}
	else if (iMinLevelFor3Point <= level && level <= iMaxLevelFor3Point)
	{
		return 3;
	}

	return 0;
}

void CThreeWayWar::onDead(CHARACTER * pChar, CHARACTER * pkKiller)
{
	if (false == pChar->IsPC())
		return;

	if (GM_PLAYER != pChar->GetGMLevel() && false == metin2::config::test_server())
		return;

	if (-1 == GetRegenFlag())
		return;

	DecreaseReviveTokenForPlayer( pChar->GetPlayerID() );

	if (false == IsSungZiMapIndex(pChar->GetMapIndex()))
		return;

	if (nullptr == pkKiller || true != pkKiller->IsPC())
		return;

	// Don't count the same empire
	if (pChar->GetEmpire() == pkKiller->GetEmpire())
		return;

	int nKillScore = GetKillScore(pkKiller->GetEmpire());

	// An empire kill score of -1 means an eliminated empire, so its score must not be checked.
	if (nKillScore >= 0)
	{
		nKillScore += GetKillValue(pChar->GetLevel());
		SetKillScore(pkKiller->GetEmpire(), nKillScore);
	}

	if (nKillScore != 0 && (metin2::config::test_server() || (nKillScore % 5) == 0))
	{
		char szBuf[64 + 1];

		snprintf(szBuf, sizeof(szBuf), LC_TEXT("현재 스코어 신수국:%d 천조국:%d 진노국:%d"),
				GetKillScore(1), GetKillScore(2), GetKillScore(3));

		SendNoticeMap(szBuf, GetSungziMapIndex(), false);
	}

	const int nVictoryScore = quest::CQuestManager::instance().GetEventFlag("threeway_war_kill_count");

	if (0 == GetRegenFlag())
	{
		int nEliminatedEmpireCount = 0;
		uint8_t bLoseEmpire = 0;

		for (int n = 1; n < 4; ++n)
		{
			if (nVictoryScore > GetKillScore(n))
			{
				++nEliminatedEmpireCount;
				bLoseEmpire = n;
			}
		}

		if (1 != nEliminatedEmpireCount)
			return;

		//----------------------
		//Reset count 
		//----------------------
		SetKillScore(1, 0);
		SetKillScore(2, 0);
		SetKillScore(3, 0);
		SetKillScore(bLoseEmpire, -1);

		quest::warp_all_to_map_my_empire_event_info * info;

		//----------------------
		//Kick out eliminated empires: from the sanctuary 
		//----------------------
		info = AllocEventInfo<quest::warp_all_to_map_my_empire_event_info>();

		info->m_lMapIndexFrom	= GetSungziMapIndex();
		info->m_lMapIndexTo		= EMPIRE_START_MAP(bLoseEmpire);
		info->m_x				= EMPIRE_START_X(bLoseEmpire);
		info->m_y				= EMPIRE_START_Y(bLoseEmpire);
		info->m_bEmpire			= bLoseEmpire;

		event_create(quest::warp_all_to_map_my_empire_event, info, PASSES_PER_SEC(10));

		//----------------------
		//Kick out eliminated empires: from the passage 
		//----------------------
		info = AllocEventInfo<quest::warp_all_to_map_my_empire_event_info>();

		info->m_lMapIndexFrom	= GetPassMapIndex(bLoseEmpire);
		info->m_lMapIndexTo		= EMPIRE_START_MAP(bLoseEmpire);
		info->m_x				= EMPIRE_START_X(bLoseEmpire);
		info->m_y				= EMPIRE_START_Y(bLoseEmpire);
		info->m_bEmpire			= bLoseEmpire;

		event_create(quest::warp_all_to_map_my_empire_event, info, PASSES_PER_SEC(10));

		//----------------------
		//The demon king announces the empire being kicked from the sanctuary!
		//----------------------
		const std::string Nation(EMPIRE_NAME(bLoseEmpire));
		const std::string Script(
				LC_TEXT("성지의 마왕: 너희 ") +
				Nation +
				LC_TEXT("녀석들은 이곳 성지에 있을 자격을 잃었다. 모두 성지에서 물러나거라~~[ENTER][ENTER] 10초 후에 모두 마을로 이동하게 됩니다. ") +
				"[ENTER][DONE]"
				);

		CHARACTER_MANAGER::instance().SendScriptToMap(pChar->GetMapIndex(), Script);

		//----------------------
		// Send out a notice.
		//----------------------
		char szNotice[512+1];
		snprintf(szNotice, sizeof(szNotice), LC_TEXT("삼거리 전투에서 %s 국가가 가장먼저 탈락을 하였습니다"), Nation.c_str());
		BroadcastNotice(szNotice);

		snprintf(szNotice, sizeof(szNotice), "First Step: %s exclusion", Nation.c_str());
		LogManager::instance().CharLog(0, 0, 0, 0, "THREEWAY", szNotice, nullptr);

		//----------------------
		// Respawn the mobs.
		//----------------------
		regen_mob_event_info* regen_info = AllocEventInfo<regen_mob_event_info>();

		regen_info->dwMapIndex = pChar->GetMapIndex();

		event_create(regen_mob_event, regen_info, PASSES_PER_SEC(10));

		SetRegenFlag(1);
	}
	else if (1 == GetRegenFlag())
	{
		int nVictoryEmpireIndex = 0;

		for (int n = 1; n < 4; ++n)
		{
			nKillScore = GetKillScore(n);

			if (nKillScore == -1)
				continue;

			if (nVictoryScore <= nKillScore)
			{
				nVictoryEmpireIndex = n;
				break;
			}
		}

		if (0 == nVictoryEmpireIndex)
			return;

		for (int n = 1; n < 4; ++n)
		{
			if (n != nVictoryEmpireIndex)
			{
				uint8_t bLoseEmpire = n;
				quest::warp_all_to_map_my_empire_event_info * info;

				//----------------------
				//Kick out eliminated empires: from the sanctuary 
				//----------------------
				info = AllocEventInfo<quest::warp_all_to_map_my_empire_event_info>();

				info->m_lMapIndexFrom	= GetSungziMapIndex();
				info->m_lMapIndexTo		= EMPIRE_START_MAP(bLoseEmpire);
				info->m_x				= EMPIRE_START_X(bLoseEmpire);
				info->m_y				= EMPIRE_START_Y(bLoseEmpire);
				info->m_bEmpire			= bLoseEmpire;

				event_create(quest::warp_all_to_map_my_empire_event, info, PASSES_PER_SEC(5));

				//----------------------
				//Kick out eliminated empires: from the passage 
				//----------------------
				info = AllocEventInfo<quest::warp_all_to_map_my_empire_event_info>();

				info->m_lMapIndexFrom	= GetPassMapIndex(bLoseEmpire);
				info->m_lMapIndexTo		= EMPIRE_START_MAP(bLoseEmpire);
				info->m_x				= EMPIRE_START_X(bLoseEmpire);
				info->m_y				= EMPIRE_START_Y(bLoseEmpire);
				info->m_bEmpire			= bLoseEmpire;

				event_create(quest::warp_all_to_map_my_empire_event, info, PASSES_PER_SEC(5));
			}
		}

		//------------------------------
		// Show the final score 
		//------------------------------
		{
			char szBuf[64 + 1];
			snprintf(szBuf, sizeof(szBuf), LC_TEXT("현재 스코어 신수국:%d 천조국:%d 진노국:%d"),
					GetKillScore(1), GetKillScore(2), GetKillScore(3));

			SendNoticeMap(szBuf, GetSungziMapIndex(), false);
		}

		// Show a message.
		SECTREE_MAP * pSecMap = SECTREE_MANAGER::instance().GetMap(pChar->GetMapIndex());

		if (nullptr != pSecMap)
		{
			const std::string EmpireName(EMPIRE_NAME(nVictoryEmpireIndex));
			const std::string Script(
					EmpireName +
					LC_TEXT(". 너희가 성지의 수호자를 잡게 된다면 너희는 성지의 주인이 된다.[ENTER][ENTER] ") +
					"[ENTER][DONE]");

			struct packet_script pack_script;

			pack_script.header = HEADER_GC_SCRIPT;
			pack_script.skin = 1;
			pack_script.src_size = Script.size();

			quest::FSendPacketToEmpire fSend;
			fSend.bEmpire = nVictoryEmpireIndex;

			pack_script.size = pack_script.src_size + sizeof(struct packet_script);
			fSend.buf.write(&pack_script, sizeof(struct packet_script));
			fSend.buf.write(&Script[0], Script.size());

			pSecMap->for_each(fSend);

			char szBuf[512];
			snprintf(szBuf, sizeof(szBuf), "Second Step: %s remain", EMPIRE_NAME( nVictoryEmpireIndex ));
			LogManager::instance().CharLog(0, 0, 0, 0, "THREEWAY", szBuf, nullptr);
		}

		//------------------------------
		// Final reward: summon the true nine-tailed fox 
		//-----------------------------	
		for (int n = 0; n < quest::CQuestManager::instance().GetEventFlag("threeway_war_boss_count");)
		{
			int x = pChar->GetX();
			int y = pChar->GetY();

			x = (thecore_random() & 1) ? x - number(200, 1000) : x + number(200, 1000);
			y = (thecore_random() & 1) ? y - number(200, 1000) : y + number(200, 1000);

			if (x < 0)
				x = pChar->GetX();

			if (y < 0)
				y = pChar->GetY();

			CHARACTER * ch = CHARACTER_MANAGER::instance().SpawnMob(
					GetEventSungZiMapInfo().m_iBossMobVnum,
					pChar->GetMapIndex(),
					x, y, 0, false);

			if (nullptr != ch)
			{
				ch->SetAggressive();
				++n;
			}
		}
		
		SetRegenFlag(-1);
	}
}

struct FDestroyAllEntity
{
	void operator() (CEntity * ent)
	{
		if (true == ent->IsType(ENTITY_CHARACTER))
		{
			CHARACTER * ch = static_cast<CHARACTER *>(ent);

			if (false == ch->IsPC())
			{
				ch->Dead();
			}
		}
	}
};

void CThreeWayWar::RemoveAllMonstersInThreeWay() const
{
	std::set<int>::const_iterator iter = MapIndexSet_.begin();

	while( iter != MapIndexSet_.end() )
	{
		SECTREE_MAP * pSecMap = SECTREE_MANAGER::instance().GetMap( *iter );

		if (nullptr != pSecMap)
		{
			FDestroyAllEntity f;

			pSecMap->for_each( f );
		}

		++iter;
	}
}

//
// C functions
//
const char* GetSungziMapPath()
{
	static char s_szMapPath[128];

	snprintf(s_szMapPath, sizeof(s_szMapPath), "%s/map/%s/",
		   	LocaleService_GetBasePath().c_str(),
		   	CThreeWayWar::instance().GetEventSungZiMapInfo().m_stMapName.c_str());

	return s_szMapPath;
}

const char* GetPassMapPath( uint8_t bEmpire )
{
	if (bEmpire > 0 && bEmpire < EMPIRE_MAX_NUM)
	{
		static char s_szMapPath[128];

		snprintf(s_szMapPath, sizeof(s_szMapPath), "%s/map/%s/",
				LocaleService_GetBasePath().c_str(),
				CThreeWayWar::instance().GetEventPassMapInfo().m_stMapName[bEmpire-1].c_str());

		return s_szMapPath;
	}

	return nullptr;
}

int GetPassMapIndex( uint8_t bEmpire )
{
	if (bEmpire > 0 && bEmpire < EMPIRE_MAX_NUM)
	{
		return CThreeWayWar::instance().GetEventPassMapInfo().m_iForkedPass[bEmpire-1];
	}

	return 0;
}

int GetSungziMapIndex()
{
	return CThreeWayWar::instance().GetEventSungZiMapInfo().m_iForkedSung;
}

int GetSungziStartX( uint8_t bEmpire )
{
	if (bEmpire > 0 && bEmpire < EMPIRE_MAX_NUM)
	{
		return CThreeWayWar::instance().GetEventSungZiMapInfo().m_iForkedSungziStartPosition[bEmpire-1][0];
	}

	return 0;
}

int GetSungziStartY( uint8_t bEmpire )
{
	if (bEmpire > 0 && bEmpire < EMPIRE_MAX_NUM)
	{
		return CThreeWayWar::instance().GetEventSungZiMapInfo().m_iForkedSungziStartPosition[bEmpire-1][1];
	}

	return 0;
}

