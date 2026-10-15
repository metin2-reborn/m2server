// vim:ts=4 sw=4
#include <metin2/db/clients/ClientManager.h>
#include <metin2/db/storage/DBManager.h>
#include <metin2/db/guilds/GuildManager.h>
#include <metin2/db/app/Main.h>
#include <metin2/db/clients/Peer.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/common/utils.h>
#include <metin2/sql/AsyncSQL.h>
#include <metin2/log/log.h>

#include <mysql.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

namespace metin2::db::inline clients {


void CClientManager::GuildCreate(CPeer * peer, uint32_t dwGuildID)
{
	metin2::log::info( "GuildCreate {:d}", dwGuildID);
	ForwardPacket(HEADER_DG_GUILD_LOAD, &dwGuildID, sizeof(uint32_t));

	CGuildManager::instance().Load(dwGuildID);
}

void CClientManager::GuildChangeGrade(CPeer* peer, TPacketGuild* p)
{
	metin2::log::info( "GuildChangeGrade {:d} {:d}", p->dwGuild, p->dwInfo);
	ForwardPacket(HEADER_DG_GUILD_CHANGE_GRADE, p, sizeof(TPacketGuild));
}

void CClientManager::GuildAddMember(CPeer* peer, TPacketGDGuildAddMember * p)
{
	CGuildManager::instance().TouchGuild(p->dwGuild);
	metin2::log::info( "GuildAddMember {:d} {:d}", p->dwGuild, p->dwPID);

	char szQuery[512];

	snprintf(szQuery, sizeof(szQuery), 
			"INSERT INTO guild_member%s VALUES(%u, %u, %d, 0, 0)",
			GetTablePostfix(), p->dwPID, p->dwGuild, p->bGrade);

	std::unique_ptr<SQLMsg> pmsg_insert(CDBManager::instance().DirectQuery(szQuery));

	snprintf(szQuery, sizeof(szQuery), 
			"SELECT pid, grade, is_general, offer, level, job, name FROM guild_member%s, player%s WHERE guild_id = %u and pid = id and pid = %u", GetTablePostfix(), GetTablePostfix(), p->dwGuild, p->dwPID);

	std::unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));

	if (pmsg->Get()->uiNumRows == 0)
	{
		metin2::log::error("Query failed when getting guild member data {}", pmsg->stQuery.c_str());
		return;
	}

	MYSQL_ROW row = mysql_fetch_row(pmsg->Get()->pSQLResult);

	if (!row[0] || !row[1])
		return;

	TPacketDGGuildMember dg;

	dg.dwGuild = p->dwGuild;
	str_to_number(dg.dwPID, row[0]);
	str_to_number(dg.bGrade, row[1]);
	str_to_number(dg.isGeneral, row[2]);
	str_to_number(dg.dwOffer, row[3]);
	str_to_number(dg.bLevel, row[4]);
	str_to_number(dg.bJob, row[5]);
	strlcpy(dg.szName, row[6], sizeof(dg.szName));

	ForwardPacket(HEADER_DG_GUILD_ADD_MEMBER, &dg, sizeof(TPacketDGGuildMember));
}

void CClientManager::GuildRemoveMember(CPeer* peer, TPacketGuild* p)
{
	metin2::log::info( "GuildRemoveMember {:d} {:d}", p->dwGuild, p->dwInfo);

	char szQuery[512];
	snprintf(szQuery, sizeof(szQuery), "DELETE FROM guild_member%s WHERE pid=%u and guild_id=%u", GetTablePostfix(), p->dwInfo, p->dwGuild);
	CDBManager::instance().AsyncQuery(szQuery);

	snprintf(szQuery, sizeof(szQuery), "REPLACE INTO quest%s (dwPID, szName, szState, lValue) VALUES(%u, 'guild_manage', 'withdraw_time', %u)", GetTablePostfix(), p->dwInfo, (uint32_t) GetCurrentTime());
	CDBManager::instance().AsyncQuery(szQuery);

	ForwardPacket(HEADER_DG_GUILD_REMOVE_MEMBER, p, sizeof(TPacketGuild));
}

void CClientManager::GuildSkillUpdate(CPeer* peer, TPacketGuildSkillUpdate* p)
{
	metin2::log::info( "GuildSkillUpdate {:d}", p->amount);
	ForwardPacket(HEADER_DG_GUILD_SKILL_UPDATE, p, sizeof(TPacketGuildSkillUpdate));
}

void CClientManager::GuildExpUpdate(CPeer* peer, TPacketGuildExpUpdate* p)
{
	metin2::log::info( "GuildExpUpdate {:d}", p->amount);
	ForwardPacket(HEADER_DG_GUILD_EXP_UPDATE, p, sizeof(TPacketGuildExpUpdate), 0, peer);
}

void CClientManager::GuildChangeMemberData(CPeer* peer, TPacketGuildChangeMemberData* p)
{
	metin2::log::info( "GuildChangeMemberData {:d} {:d} {:d} {:d}", p->pid, p->offer, p->level, p->grade);
	ForwardPacket(HEADER_DG_GUILD_CHANGE_MEMBER_DATA, p, sizeof(TPacketGuildChangeMemberData), 0, peer);
}

void CClientManager::GuildDisband(CPeer* peer, TPacketGuild* p)
{
	metin2::log::info( "GuildDisband {:d}", p->dwGuild);

	char szQuery[512];

	snprintf(szQuery, sizeof(szQuery), "DELETE FROM guild%s WHERE id=%u", GetTablePostfix(), p->dwGuild);
	CDBManager::instance().AsyncQuery(szQuery);

	snprintf(szQuery, sizeof(szQuery), "DELETE FROM guild_grade%s WHERE guild_id=%u", GetTablePostfix(), p->dwGuild);
	CDBManager::instance().AsyncQuery(szQuery);

	snprintf(szQuery, sizeof(szQuery), "REPLACE INTO quest%s (dwPID, szName, szState, lValue) SELECT pid, 'guild_manage', 'withdraw_time', %u FROM guild_member%s WHERE guild_id = %u", GetTablePostfix(), (uint32_t) GetCurrentTime(), GetTablePostfix(), p->dwGuild);
	CDBManager::instance().AsyncQuery(szQuery);

	snprintf(szQuery, sizeof(szQuery), "DELETE FROM guild_member%s WHERE guild_id=%u", GetTablePostfix(), p->dwGuild);
	CDBManager::instance().AsyncQuery(szQuery);
	
	snprintf(szQuery, sizeof(szQuery), "DELETE FROM guild_comment%s WHERE guild_id=%u", GetTablePostfix(), p->dwGuild);
	CDBManager::instance().AsyncQuery(szQuery);

	ForwardPacket(HEADER_DG_GUILD_DISBAND, p, sizeof(TPacketGuild));
}

const char* __GetWarType(int n)
{
	switch (n)
	{
		case 0 :
			return "패왕";
		case 1 :
			return "맹장";
		case 2 :
			return "수호";
		default :
			return "없는 번호";
	}
}

void CClientManager::GuildWar(CPeer* peer, TPacketGuildWar* p)
{
	switch (p->bWar)
	{
		case GUILD_WAR_SEND_DECLARE:
			metin2::log::info( "GuildWar: GUILD_WAR_SEND_DECLARE type({}) guild({:d} - {:d})",  __GetWarType(p->bType), p->dwGuildFrom, p->dwGuildTo);
			CGuildManager::instance().AddDeclare(p->bType, p->dwGuildFrom, p->dwGuildTo);
			break;

		case GUILD_WAR_REFUSE:
			metin2::log::info( "GuildWar: GUILD_WAR_REFUSE type({}) guild({:d} - {:d})", __GetWarType(p->bType), p->dwGuildFrom, p->dwGuildTo);
			CGuildManager::instance().RemoveDeclare(p->dwGuildFrom, p->dwGuildTo);
			break;

		case GUILD_WAR_WAIT_START:
			metin2::log::info( "GuildWar: GUILD_WAR_WAIT_START type({}) guild({:d} - {:d})", __GetWarType(p->bType), p->dwGuildFrom, p->dwGuildTo);
			[[fallthrough]];
		case GUILD_WAR_RESERVE:	// Guild war reservation
			if (p->bWar != GUILD_WAR_WAIT_START)
				metin2::log::info( "GuildWar: GUILD_WAR_RESERVE type({}) guild({:d} - {:d})", __GetWarType(p->bType), p->dwGuildFrom, p->dwGuildTo);
			CGuildManager::instance().RemoveDeclare(p->dwGuildFrom, p->dwGuildTo);

			if (!CGuildManager::instance().ReserveWar(p))
				p->bWar = GUILD_WAR_CANCEL;
			else
				p->bWar = GUILD_WAR_RESERVE;

			break;

		case GUILD_WAR_ON_WAR:		// Start the guild war. (Field wars start immediately)
			metin2::log::info( "GuildWar: GUILD_WAR_ON_WAR type({}) guild({:d} - {:d})", __GetWarType(p->bType), p->dwGuildFrom, p->dwGuildTo);
			CGuildManager::instance().RemoveDeclare(p->dwGuildFrom, p->dwGuildTo);
			CGuildManager::instance().StartWar(p->bType, p->dwGuildFrom, p->dwGuildTo);
			break;

		case GUILD_WAR_OVER:		// Guild war ended normally
			metin2::log::info( "GuildWar: GUILD_WAR_OVER type({}) guild({:d} - {:d})", __GetWarType(p->bType), p->dwGuildFrom, p->dwGuildTo);
			CGuildManager::instance().RecvWarOver(p->dwGuildFrom, p->dwGuildTo, p->bType, p->lWarPrice);
			break;

		case GUILD_WAR_END:		// Guild war ended abnormally
			metin2::log::info( "GuildWar: GUILD_WAR_END type({}) guild({:d} - {:d})", __GetWarType(p->bType), p->dwGuildFrom, p->dwGuildTo);
			CGuildManager::instance().RecvWarEnd(p->dwGuildFrom, p->dwGuildTo);
			return; // NOTE: RecvWarEnd sends the packet, so no separate broadcast.

		case GUILD_WAR_CANCEL :
			metin2::log::info( "GuildWar: GUILD_WAR_CANCEL type({}) guild({:d} - {:d})", __GetWarType(p->bType), p->dwGuildFrom, p->dwGuildTo);
			CGuildManager::instance().CancelWar(p->dwGuildFrom, p->dwGuildTo);
			break;
	}

	ForwardPacket(HEADER_DG_GUILD_WAR, p, sizeof(TPacketGuildWar));
}

void CClientManager::GuildWarScore(CPeer* peer, TPacketGuildWarScore * p)
{
	CGuildManager::instance().UpdateScore(p->dwGuildGainPoint, p->dwGuildOpponent, p->lScore, p->lBetScore);
}

void CClientManager::GuildChangeLadderPoint(TPacketGuildLadderPoint* p)
{
	metin2::log::info( "GuildChangeLadderPoint Recv {:d} {:d}", p->dwGuild, p->lChange);
	CGuildManager::instance().ChangeLadderPoint(p->dwGuild, p->lChange);
}

void CClientManager::GuildUseSkill(TPacketGuildUseSkill* p)
{
	metin2::log::info( "GuildUseSkill Recv {:d} {:d}", p->dwGuild, p->dwSkillVnum);
	CGuildManager::instance().UseSkill(p->dwGuild, p->dwSkillVnum, p->dwCooltime);
	SendGuildSkillUsable(p->dwGuild, p->dwSkillVnum, false);
}

void CClientManager::SendGuildSkillUsable(uint32_t guild_id, uint32_t dwSkillVnum, bool bUsable)
{
	metin2::log::info( "SendGuildSkillUsable Send {:d} {:d} {}", guild_id, dwSkillVnum, bUsable?"true":"false");

	TPacketGuildSkillUsableChange p;

	p.dwGuild = guild_id;
	p.dwSkillVnum = dwSkillVnum;
	p.bUsable = bUsable;

	ForwardPacket(HEADER_DG_GUILD_SKILL_USABLE_CHANGE, &p, sizeof(TPacketGuildSkillUsableChange));
}

void CClientManager::GuildChangeMaster(TPacketChangeGuildMaster* p)
{
	if (CGuildManager::instance().ChangeMaster(p->dwGuildID, p->idFrom, p->idTo) == true)
	{
		TPacketChangeGuildMaster packet;
		packet.dwGuildID = p->dwGuildID;
		packet.idFrom = 0;
		packet.idTo = 0;

		ForwardPacket(HEADER_DG_ACK_CHANGE_GUILD_MASTER, &packet, sizeof(packet));
	}
}
}
