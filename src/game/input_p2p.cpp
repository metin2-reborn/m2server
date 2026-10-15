#include "affect.h"
#include "castle.h"
#include "char.h"
#include "char_manager.h"
#include "cmd.h"
#include "config.h"
#include "desc.h"
#include "desc_manager.h"
#include "empire_text_convert.h"
#include "guild.h"
#include "guild_manager.h"
#include "input.h"
#include "messenger_manager.h"
#include "p2p.h"
#include "packet.h"
#include "questmanager.h"
#include "skill.h"
#include "typedef.h"
#include "unique_item.h"
#include "xmas_event.h"

#include <common/length.h>
#include <metin2/log/log.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace metin2::game {


////////////////////////////////////////////////////////////////////////////////
// Input Processor
CInputP2P::CInputP2P()
{
	BindPacketInfo(&m_packetInfoGG);
}

void CInputP2P::Login(DESC * d, const char * c_pData)
{
	P2P_MANAGER::instance().Login(d, (TPacketGGLogin *) c_pData);
}

void CInputP2P::Logout(DESC * d, const char * c_pData)
{
	TPacketGGLogout * p = (TPacketGGLogout *) c_pData;
	P2P_MANAGER::instance().Logout(p->szName);
}

int CInputP2P::Relay(DESC * d, const char * c_pData, size_t uiBytes)
{
	TPacketGGRelay * p = (TPacketGGRelay *) c_pData;

	if (uiBytes < sizeof(TPacketGGRelay) + p->lSize)
		return -1;

	if (p->lSize < 0)
	{
		metin2::log::error("invalid packet length {:d}", p->lSize);
		d->SetPhase(PHASE_CLOSE);
		return -1;
	}

	metin2::log::info( "InputP2P::Relay : {} size {:d}", p->szName, p->lSize);

	CHARACTER * pkChr = CHARACTER_MANAGER::instance().FindPC(p->szName);

	const uint8_t* c_pbData = (const uint8_t *) (c_pData + sizeof(TPacketGGRelay));

	if (!pkChr)
		return p->lSize;

	if (*c_pbData == HEADER_GC_WHISPER)
	{
		if (pkChr->IsBlockMode(BLOCK_WHISPER))
		{
			// Whisper rejected because whispers are blocked.
			return p->lSize;
		}

		char buf[1024];
		memcpy(buf, c_pbData, std::min<int>(p->lSize, sizeof(buf)));

		TPacketGCWhisper* p2 = (TPacketGCWhisper*) buf;
		// bType upper 4 bits: Empire number
		// bType lower 4 bits: EWhisperType
		uint8_t bToEmpire = (p2->bType >> 4);
		p2->bType = p2->bType & 0x0F;
		if(p2->bType == 0x0F) {
			// System message whispers use all bits of bType, including the upper ones.
			p2->bType = WHISPER_TYPE_SYSTEM;
		} else {
			if (!pkChr->IsEquipUniqueGroup(UNIQUE_GROUP_RING_OF_LANGUAGE))
				if (bToEmpire >= 1 && bToEmpire <= 3 && pkChr->GetEmpire() != bToEmpire)
				{
					ConvertEmpireText(bToEmpire,
							buf + sizeof(TPacketGCWhisper), 
							p2->wSize - sizeof(TPacketGCWhisper),
							10+2*pkChr->GetSkillPower(SKILL_LANGUAGE1 + bToEmpire - 1));
				}
		}

		pkChr->GetDesc()->Packet(buf, p->lSize);
	}
	else
		pkChr->GetDesc()->Packet(c_pbData, p->lSize);

	return (p->lSize);
}

int CInputP2P::Notice(DESC * d, const char * c_pData, size_t uiBytes)
{
	TPacketGGNotice * p = (TPacketGGNotice *) c_pData;

	if (uiBytes < sizeof(TPacketGGNotice) + p->lSize)
		return -1;

	if (p->lSize < 0)
	{
		metin2::log::error("invalid packet length {:d}", p->lSize);
		d->SetPhase(PHASE_CLOSE);
		return -1;
	}

	char szBuf[256+1];
	strlcpy(szBuf, c_pData + sizeof(TPacketGGNotice), std::min<int>(p->lSize + 1, sizeof(szBuf)));
	SendNotice(szBuf);
	return (p->lSize);
}

int CInputP2P::MonarchNotice(DESC * d, const char * c_pData, size_t uiBytes)
{
	TPacketGGMonarchNotice * p = (TPacketGGMonarchNotice *) c_pData;

	if (uiBytes < p->lSize + sizeof(TPacketGGMonarchNotice))
		return -1;

	if (p->lSize < 0)
	{
		metin2::log::error("invalid packet length {:d}", p->lSize);
		d->SetPhase(PHASE_CLOSE);
		return -1;
	}

	char szBuf[256+1];
	strlcpy(szBuf, c_pData + sizeof(TPacketGGMonarchNotice), std::min<int>(p->lSize + 1, sizeof(szBuf)));
	SendMonarchNotice(p->bEmpire, szBuf);
	return (p->lSize);
}

int CInputP2P::MonarchTransfer(DESC * d, const char* c_pData)
{
	TPacketMonarchGGTransfer* p = (TPacketMonarchGGTransfer*) c_pData;
	CHARACTER * pTargetChar = CHARACTER_MANAGER::instance().FindByPID(p->dwTargetPID);

	if (pTargetChar != nullptr)
	{
		unsigned int qIndex = quest::CQuestManager::instance().GetQuestIndexByName("monarch_transfer");

		if (qIndex != 0)
		{
			pTargetChar->SetQuestFlag("monarch_transfer.x", p->x);
			pTargetChar->SetQuestFlag("monarch_transfer.y", p->y);
			quest::CQuestManager::instance().Letter(pTargetChar->GetPlayerID(), qIndex, 0);
		}
	}

	return 0;
}

int CInputP2P::Guild(DESC * d, const char* c_pData, size_t uiBytes)
{
	TPacketGGGuild * p = (TPacketGGGuild *) c_pData;
	uiBytes -= sizeof(TPacketGGGuild);
	c_pData += sizeof(TPacketGGGuild);

	CGuild * g = CGuildManager::instance().FindGuild(p->dwGuild);

	switch (p->bSubHeader)
	{
		case GUILD_SUBHEADER_GG_CHAT:
			{
				if (uiBytes < sizeof(TPacketGGGuildChat))
					return -1;

				TPacketGGGuildChat * p = (TPacketGGGuildChat *) c_pData;

				if (g)
					g->P2PChat(p->szText);

				return sizeof(TPacketGGGuildChat);
			}
			
		case GUILD_SUBHEADER_GG_SET_MEMBER_COUNT_BONUS:
			{
				if (uiBytes < sizeof(int))
					return -1;

				int iBonus = *((int *) c_pData);
				CGuild* pGuild = CGuildManager::instance().FindGuild(p->dwGuild);
				if (pGuild)
				{
					pGuild->SetMemberCountBonus(iBonus);
				}
				return sizeof(int);
			}
		default:
			metin2::log::error("UNKNOWN GUILD SUB PACKET");
			break;
	}
	return 0;
}


struct FuncShout
{
	const char * m_str;
	uint8_t m_bEmpire;

	FuncShout(const char * str, uint8_t bEmpire) : m_str(str), m_bEmpire(bEmpire)
	{
	}   

	void operator () (DESC * d)
	{
		if (!d->GetCharacter() || (d->GetCharacter()->GetGMLevel() == GM_PLAYER && d->GetEmpire() != m_bEmpire))
			return;

		d->GetCharacter()->ChatPacket(CHAT_TYPE_SHOUT, "%s", m_str);
	}
};

void SendShout(const char * szText, uint8_t bEmpire)
{
	const DESC_MANAGER::DESC_SET & c_ref_set = DESC_MANAGER::instance().GetClientSet();
	std::for_each(c_ref_set.begin(), c_ref_set.end(), FuncShout(szText, bEmpire));
}

void CInputP2P::Shout(const char * c_pData)
{
	TPacketGGShout * p = (TPacketGGShout *) c_pData;
	SendShout(p->szText, p->bEmpire);
}

void CInputP2P::Disconnect(const char * c_pData)
{
	TPacketGGDisconnect * p = (TPacketGGDisconnect *) c_pData;

	DESC * d = DESC_MANAGER::instance().FindByLoginName(p->szLogin);

	if (!d)
		return;

	if (!d->GetCharacter())
	{
		d->SetPhase(PHASE_CLOSE);
	}
	else
		d->DisconnectOfSameLogin();
}

void CInputP2P::Setup(DESC * d, const char * c_pData)
{
	TPacketGGSetup * p = (TPacketGGSetup *) c_pData;
	metin2::log::info( "P2P: Setup {}:{:d}", d->GetHostName(), p->wPort);
	d->SetP2P(d->GetHostName(), p->wPort);
}

void CInputP2P::MessengerAdd(const char * c_pData)
{
	TPacketGGMessenger * p = (TPacketGGMessenger *) c_pData;
	metin2::log::info( "P2P: Messenger Add {} {}", p->szAccount, p->szCompanion);
	MessengerManager::instance().__AddToList(p->szAccount, p->szCompanion);
}

void CInputP2P::MessengerRemove(const char * c_pData)
{
	TPacketGGMessenger * p = (TPacketGGMessenger *) c_pData;
	metin2::log::info( "P2P: Messenger Remove {} {}", p->szAccount, p->szCompanion);
	MessengerManager::instance().__RemoveFromList(p->szAccount, p->szCompanion);
}

void CInputP2P::FindPosition(DESC * d, const char* c_pData)
{
	TPacketGGFindPosition* p = (TPacketGGFindPosition*) c_pData;
	CHARACTER * ch = CHARACTER_MANAGER::instance().FindByPID(p->dwTargetPID);
	if (ch && ch->GetMapIndex() < 10000)
	{
		TPacketGGWarpCharacter pw;
		pw.header = HEADER_GG_WARP_CHARACTER;
		pw.pid = p->dwFromPID;
		pw.x = ch->GetX();
		pw.y = ch->GetY();
		d->Packet(&pw, sizeof(pw));
	}
}

void CInputP2P::WarpCharacter(const char* c_pData)
{
	TPacketGGWarpCharacter* p = (TPacketGGWarpCharacter*) c_pData;
	CHARACTER * ch = CHARACTER_MANAGER::instance().FindByPID(p->pid);
	if (ch)
	{
		ch->WarpSet(p->x, p->y);
	}
}

void CInputP2P::GuildWarZoneMapIndex(const char* c_pData)
{
	TPacketGGGuildWarMapIndex * p = (TPacketGGGuildWarMapIndex*) c_pData;
	CGuildManager & gm = CGuildManager::instance();

	metin2::log::info( "P2P: GuildWarZoneMapIndex g1({:d}) vs g2({:d}), mapIndex({:d})", p->dwGuildID1, p->dwGuildID2, p->lMapIndex);

	CGuild * g1 = gm.FindGuild(p->dwGuildID1);
	CGuild * g2 = gm.FindGuild(p->dwGuildID2);

	if (g1 && g2)
	{
		g1->SetGuildWarMapIndex(p->dwGuildID2, p->lMapIndex);
		g2->SetGuildWarMapIndex(p->dwGuildID1, p->lMapIndex);
	}
}

void CInputP2P::Transfer(const char * c_pData)
{
	TPacketGGTransfer * p = (TPacketGGTransfer *) c_pData;

	CHARACTER * ch = CHARACTER_MANAGER::instance().FindPC(p->szName);

	if (ch)
		ch->WarpSet(p->lX, p->lY);
}

void CInputP2P::XmasWarpSanta(const char * c_pData)
{
	TPacketGGXmasWarpSanta * p =(TPacketGGXmasWarpSanta *) c_pData;

	if (p->bChannel == metin2::config::server().channel && map_allow_find(p->lMapIndex))
	{
		int	iNextSpawnDelay = 60;

		iNextSpawnDelay = 50 * 60;

		xmas::SpawnSanta(p->lMapIndex, iNextSpawnDelay); // A new Santa appears after 50 minutes (20 minutes in Korea)

		TPacketGGXmasWarpSantaReply pack_reply;
		pack_reply.bHeader = HEADER_GG_XMAS_WARP_SANTA_REPLY;
		pack_reply.bChannel = metin2::config::server().channel;
		P2P_MANAGER::instance().Send(&pack_reply, sizeof(pack_reply));
	}
}

void CInputP2P::XmasWarpSantaReply(const char* c_pData)
{
	TPacketGGXmasWarpSantaReply* p = (TPacketGGXmasWarpSantaReply*) c_pData;

	if (p->bChannel == metin2::config::server().channel)
	{
		CharacterVectorInteractor i;

		if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(xmas::MOB_SANTA_VNUM, i))
		{
			CharacterVectorInteractor::iterator it = i.begin();

			while (it != i.end()) {
				CHARACTER_MANAGER::instance().DestroyCharacter(*it++);
			}
		}
	}
}

// BLOCK_CHAT
void CInputP2P::BlockChat(const char * c_pData)
{
	TPacketGGBlockChat * p = (TPacketGGBlockChat *) c_pData;

	CHARACTER * ch = CHARACTER_MANAGER::instance().FindPC(p->szName);

	if (ch)
	{
		metin2::log::info( "BLOCK CHAT apply name {} dur {:d}", p->szName, p->lBlockDuration);
		ch->AddAffect(AFFECT_BLOCK_CHAT, POINT_NONE, 0, AFF_NONE, p->lBlockDuration, 0, true);
	}
	else
	{
		metin2::log::info( "BLOCK CHAT fail name {} dur {:d}", p->szName, p->lBlockDuration);
	}
}   
// END_OF_BLOCK_CHAT
//

void CInputP2P::IamAwake(DESC * d, const char * c_pData)
{
	std::string hostNames;
	P2P_MANAGER::instance().GetP2PHostNames(hostNames);
	metin2::log::info( "P2P Awakeness check from {}. My P2P connection number is {:d}. and details...\n{}", d->GetHostName(), P2P_MANAGER::instance().GetDescCount(), hostNames.c_str());
}

int CInputP2P::Analyze(DESC * d, uint8_t bHeader, const char * c_pData)
{
	if (metin2::config::test_server())
		metin2::log::info( "CInputP2P::Anlayze[Header {:d}]", bHeader);

	int iExtraLen = 0;

	switch (bHeader)
	{
		case HEADER_GG_SETUP:
			Setup(d, c_pData);
			break;

		case HEADER_GG_LOGIN:
			Login(d, c_pData);
			break;

		case HEADER_GG_LOGOUT:
			Logout(d, c_pData);
			break;

		case HEADER_GG_RELAY:
			if ((iExtraLen = Relay(d, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_GG_NOTICE:
			if ((iExtraLen = Notice(d, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_GG_SHUTDOWN:
			metin2::log::error("Accept shutdown p2p command from {}.", d->GetHostName());
			Shutdown(10);
			break;

		case HEADER_GG_GUILD:
			if ((iExtraLen = Guild(d, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_GG_SHOUT:
			Shout(c_pData);
			break;

		case HEADER_GG_DISCONNECT:
			Disconnect(c_pData);
			break;

		case HEADER_GG_MESSENGER_ADD:
			MessengerAdd(c_pData);
			break;

		case HEADER_GG_MESSENGER_REMOVE:
			MessengerRemove(c_pData);
			break;

		case HEADER_GG_FIND_POSITION:
			FindPosition(d, c_pData);
			break;

		case HEADER_GG_WARP_CHARACTER:
			WarpCharacter(c_pData);
			break;

		case HEADER_GG_GUILD_WAR_ZONE_MAP_INDEX:
			GuildWarZoneMapIndex(c_pData);
			break;

		case HEADER_GG_TRANSFER:
			Transfer(c_pData);
			break;

		case HEADER_GG_XMAS_WARP_SANTA:
			XmasWarpSanta(c_pData);
			break;

		case HEADER_GG_XMAS_WARP_SANTA_REPLY:
			XmasWarpSantaReply(c_pData);
			break;

		case HEADER_GG_CHECK_CLIENT_VERSION:
			CheckClientVersion();
			break;

		case HEADER_GG_BLOCK_CHAT:
			BlockChat(c_pData);
			break;

		case HEADER_GG_SIEGE:
			{
				TPacketGGSiege* pSiege = (TPacketGGSiege*)c_pData;
				castle_siege(pSiege->bEmpire, pSiege->bTowerCount);
			}
			break;

		case HEADER_GG_MONARCH_NOTICE:
			if ((iExtraLen = MonarchNotice(d, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_GG_MONARCH_TRANSFER :
			MonarchTransfer(d, c_pData);
			break;

		case HEADER_GG_CHECK_AWAKENESS:
			IamAwake(d, c_pData);
			break;
	}

	return (iExtraLen);
}
}
