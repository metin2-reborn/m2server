#include <metin2/game/social/wedding.h>

#include <metin2/game/characters/char.h>
#include <metin2/game/characters/char_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/app/event.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

namespace metin2::game::inline social {

namespace marriage
{
	EVENTINFO(wedding_map_info)
	{
		WeddingMap * pWeddingMap;
		int iStep;

		wedding_map_info()
		: pWeddingMap( nullptr )
		, iStep( 0 )
		{
		}
	};

	EVENTFUNC(wedding_end_event)
	{
		wedding_map_info* info = dynamic_cast<wedding_map_info*>( event->info );

		if ( info == nullptr )
		{
			metin2::log::error( "wedding_end_event> <Factor> Null pointer" );
			return 0;
		}
		
		WeddingMap* pMap = info->pWeddingMap;

		if (info->iStep == 0)
		{
			++info->iStep;
			pMap->WarpAll(); 
			return PASSES_PER_SEC(15);
		}
		WeddingManager::instance().DestroyWeddingMap(pMap);
		return 0;
	}

	// Map instance
	WeddingMap::WeddingMap(uint32_t dwMapIndex, uint32_t dwPID1, uint32_t dwPID2) :
		m_dwMapIndex(dwMapIndex),
		m_pEndEvent(nullptr),
		m_isDark(false),
		m_isSnow(false),
		m_isMusic(false),
		dwPID1(dwPID1),
		dwPID2(dwPID2)
	{
	}

	WeddingMap::~WeddingMap()
	{
		event_cancel(&m_pEndEvent);
	}

	void WeddingMap::SetEnded()
	{
		if (m_pEndEvent)
		{
			metin2::log::error("WeddingMap::SetEnded - ALREADY EndEvent(m_pEndEvent={})", static_cast<const void *>(static_cast<void*>(m_pEndEvent.get())));
			return;
		}

		wedding_map_info* info = AllocEventInfo<wedding_map_info>();

		info->pWeddingMap = this;

		m_pEndEvent = event_create(wedding_end_event, info, PASSES_PER_SEC(5));

		Notice(LC_TEXT("결혼식이 종료됩니다."));
		Notice(LC_TEXT("자동으로 나가게됩니다."));

		for (auto it = m_set_pkChr.begin(); it != m_set_pkChr.end(); ++it)
		{
			CHARACTER * ch = *it;
			if (ch->GetPlayerID() == dwPID1 || ch->GetPlayerID() == dwPID2)
				continue;

			if (ch->GetLevel() < 10) // Not given to level 10 and below.
				continue;

			ch->AutoGiveItem(27002, 5);
		}
	}

	struct FNotice
	{
		FNotice(const char * psz) :
			m_psz(psz)
		{
		}

		void operator() (CHARACTER * ch)
		{
			ch->ChatPacket(CHAT_TYPE_NOTICE, "%s", m_psz);
		}

		const char * m_psz;
	};

	void WeddingMap::Notice(const char * psz)
	{
		FNotice f(psz);
		std::for_each(m_set_pkChr.begin(), m_set_pkChr.end(), f);
	}

	struct FWarpEveryone
	{
		void operator() (CHARACTER * ch)
		{
			if (ch->IsPC())
			{
				// ExitToSavedLocation calls WarpSet, and in that function
				// Sectree becomes NULL. Afterwards this character can't be found
				// via SectreeManager, so it's handled separately in DestroyAll below
				ch->ExitToSavedLocation();
			}
		}
	};

	void WeddingMap::WarpAll()
	{
		FWarpEveryone f;
		std::for_each(m_set_pkChr.begin(), m_set_pkChr.end(), f);
	}

	struct FDestroyEveryone
	{
		void operator() (CHARACTER * ch)
		{
			metin2::log::info( "WeddingMap::DestroyAll: {}", ch->GetName());

			if (ch->GetDesc())
				DESC_MANAGER::instance().DestroyDesc(ch->GetDesc());
			else
				CHARACTER_MANAGER::instance().DestroyCharacter(ch);
		}
	};

	void WeddingMap::DestroyAll()
	{
		metin2::log::info( "WeddingMap::DestroyAll: m_set_pkChr size {:d}", m_set_pkChr.size());
		
		FDestroyEveryone f;

		for (charset_t::iterator it = m_set_pkChr.begin(); it != m_set_pkChr.end(); it = m_set_pkChr.begin())
			f(*it);
	}

	void WeddingMap::IncMember(CHARACTER * ch)
	{
		if (IsMember(ch) == true)
			return;

		m_set_pkChr.insert(ch);

		SendLocalEvent(ch);

		if (ch->GetLevel() < 10)
		{
			ch->SetObserverMode(true);
		}
	}

	void WeddingMap::DecMember(CHARACTER * ch)
	{
		if (IsMember(ch) == false)
			return;

		m_set_pkChr.erase(ch);

		if (ch->GetLevel() < 10)
		{
			ch->SetObserverMode(false);
		}
	}

	bool WeddingMap::IsMember(CHARACTER * ch)
	{
		if (m_set_pkChr.size() <= 0)
			return false;

		return m_set_pkChr.find(ch) != m_set_pkChr.end();
	}

	void WeddingMap::ShoutInMap(uint8_t type, const char* msg)
	{
		for (auto it = m_set_pkChr.begin(); it != m_set_pkChr.end(); ++it)
		{
			CHARACTER * ch = *it;
			ch->ChatPacket(CHAT_TYPE_COMMAND, msg);
		}
	}

	void WeddingMap::SetMusic(bool bSet, const char* musicFileName)
	{
		if (m_isMusic != bSet)
		{
			m_isMusic = bSet;
			m_stMusicFileName = musicFileName;

			char szCommand[256];
			if (m_isMusic)
			{
				ShoutInMap(CHAT_TYPE_COMMAND, __BuildCommandPlayMusic(szCommand, sizeof(szCommand), 1, m_stMusicFileName.c_str()));
			}
			else
			{
				ShoutInMap(CHAT_TYPE_COMMAND, __BuildCommandPlayMusic(szCommand, sizeof(szCommand), 0, "default"));
			}
		} 
	}

	void WeddingMap::SetDark(bool bSet)
	{
		if (m_isDark != bSet)
		{
			m_isDark = bSet;

			if (m_isDark)
				ShoutInMap(CHAT_TYPE_COMMAND, "DayMode dark");
			else
				ShoutInMap(CHAT_TYPE_COMMAND, "DayMode light");
		}
	}

	void WeddingMap::SetSnow(bool bSet)
	{
		if (m_isSnow != bSet)
		{
			m_isSnow = bSet;

			if (m_isSnow)
				ShoutInMap(CHAT_TYPE_COMMAND, "xmas_snow 1");
			else
				ShoutInMap(CHAT_TYPE_COMMAND, "xmas_snow 0");
		}
	}

	bool WeddingMap::IsPlayingMusic()
	{
		return m_isMusic;
	}

	void WeddingMap::SendLocalEvent(CHARACTER * ch)
	{
		char szCommand[256];

		if (m_isDark)
			ch->ChatPacket(CHAT_TYPE_COMMAND, "DayMode dark");
		if (m_isSnow)
			ch->ChatPacket(CHAT_TYPE_COMMAND, "xmas_snow 1");
		if (m_isMusic)
			ch->ChatPacket(CHAT_TYPE_COMMAND, __BuildCommandPlayMusic(szCommand, sizeof(szCommand), 1, m_stMusicFileName.c_str()));	
	}

	const char* WeddingMap::__BuildCommandPlayMusic(char* szCommand, size_t nCmdLen, uint8_t bSet, const char* c_szMusicFileName)
	{
		if (nCmdLen < 1)
		{
			szCommand[0] = '\0';
			return "PlayMusic 0 CommandLengthError";
		}

		snprintf(szCommand, nCmdLen, "PlayMusic %d %s", bSet, c_szMusicFileName);
		return szCommand;
	}
	// Manager

	WeddingManager::WeddingManager()
	{
	}

	WeddingManager::~WeddingManager()
	{
	}

	bool WeddingManager::IsWeddingMap(uint32_t dwMapIndex)
	{
		return (dwMapIndex == WEDDING_MAP_INDEX || dwMapIndex / 10000 == WEDDING_MAP_INDEX);
	}

	WeddingMap* WeddingManager::Find(uint32_t dwMapIndex)
	{
		auto it = m_mapWedding.find(dwMapIndex);

		if (it == m_mapWedding.end())
			return nullptr;

		return it->second;
	}

	uint32_t WeddingManager::__CreateWeddingMap(uint32_t dwPID1, uint32_t dwPID2)
	{
		SECTREE_MANAGER& rkSecTreeMgr = SECTREE_MANAGER::instance();

		uint32_t dwMapIndex = rkSecTreeMgr.CreatePrivateMap(WEDDING_MAP_INDEX);

		if (!dwMapIndex)
		{
			metin2::log::error("CreateWeddingMap(pid1={:d}, pid2={:d}) / CreatePrivateMap({:d}) FAILED", dwPID1, dwPID2, WEDDING_MAP_INDEX);
			return 0;
		}

		m_mapWedding.insert(std::make_pair(dwMapIndex, new WeddingMap(dwMapIndex, dwPID1, dwPID2)));


		// LOCALE_SERVICE
		SECTREE_MAP * pkSectreeMap = rkSecTreeMgr.GetMap(dwMapIndex);
		if (pkSectreeMap == nullptr) {
			return 0;
		}
		std::string st_weddingMapRegenFileName;
		st_weddingMapRegenFileName.reserve(64);
		st_weddingMapRegenFileName  = LocaleService_GetMapPath();
		st_weddingMapRegenFileName += "/metin2_map_wedding_01/npc.txt";

		if (!regen_do(st_weddingMapRegenFileName.c_str(), dwMapIndex, pkSectreeMap->m_setting.iBaseX, pkSectreeMap->m_setting.iBaseY, nullptr, true))
		{
			metin2::log::error("CreateWeddingMap(pid1={:d}, pid2={:d}) / regen_do(fileName={}, mapIndex={:d}, basePos=({:d}, {:d})) FAILED", dwPID1, dwPID2, st_weddingMapRegenFileName.c_str(), dwMapIndex, pkSectreeMap->m_setting.iBaseX, pkSectreeMap->m_setting.iBaseY);
		}
		else
		{
			metin2::log::info( "CreateWeddingMap(pid1={:d}, pid2={:d}) / regen_do(fileName={}, mapIndex={:d}, basePos=({:d}, {:d})) ok", dwPID1, dwPID2, st_weddingMapRegenFileName.c_str(), dwMapIndex, pkSectreeMap->m_setting.iBaseX, pkSectreeMap->m_setting.iBaseY);
		}
		// END_OF_LOCALE_SERVICE

		return dwMapIndex;
	}
	
	void WeddingManager::DestroyWeddingMap(WeddingMap* pMap)
	{
		metin2::log::info( "DestroyWeddingMap(index={:d})", pMap->GetMapIndex());
		pMap->DestroyAll();
		m_mapWedding.erase(pMap->GetMapIndex());
		SECTREE_MANAGER::instance().DestroyPrivateMap(pMap->GetMapIndex());
		delete pMap;
	}

	bool WeddingManager::End(uint32_t dwMapIndex)
	{
		auto it = m_mapWedding.find(dwMapIndex);

		if (it == m_mapWedding.end())
			return false;

		it->second->SetEnded();
		return true;
	}

	void WeddingManager::Request(uint32_t dwPID1, uint32_t dwPID2)
	{
		if (map_allow_find(WEDDING_MAP_INDEX))
		{
			uint32_t dwMapIndex = __CreateWeddingMap(dwPID1, dwPID2);

			if (!dwMapIndex)
			{
				metin2::log::error("cannot create wedding map for {:d}, {:d}", dwPID1, dwPID2);
				return;
			}

			TPacketWeddingReady p;
			p.dwPID1 = dwPID1;
			p.dwPID2 = dwPID2;
			p.dwMapIndex = dwMapIndex;

			db_clientdesc->DBPacket(HEADER_GD_WEDDING_READY, 0, &p, sizeof(p));
		}
	}

}
}
