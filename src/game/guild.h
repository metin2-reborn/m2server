#pragma once

#include "char.h"
#include "event.h"
#include "skill.h"
#include "typedef.h"

#include <common/length.h>
#include <common/tables.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>

struct SQLMsg;

enum
{
	GUILD_GRADE_NAME_MAX_LEN = 8,
	GUILD_GRADE_COUNT = 15,
	GUILD_COMMENT_MAX_COUNT = 12,
	GUILD_COMMENT_MAX_LEN = 50,
	GUILD_LEADER_GRADE = 1,
	GUILD_BASE_POWER = 400,
	GUILD_POWER_PER_LEVEL = 100,
	GUILD_WAR_MIN_MEMBER_COUNT = 8,
	GUILD_LADDER_POINT_PER_LEVEL = 1000,
	GUILD_CREATE_ITEM_VNUM = 70101,
};



typedef struct SGuildMember
{
	uint32_t pid; // id in the player table; primary key
	uint8_t grade; // Player's guild rank, 1 to 15 (1 is the top)
	uint8_t is_general;
	uint8_t job;
	uint8_t level;
	uint32_t offer_exp; // Contributed experience
	uint8_t _dummy;

	std::string name;

	SGuildMember(uint32_t pid, uint8_t grade, uint8_t is_general, uint8_t job, uint8_t level, uint32_t offer_exp, char* name);

} TGuildMember;

#pragma pack(1)
typedef struct SGuildMemberPacketData
{   
	uint32_t pid;
	uint8_t grade;
	uint8_t is_general;
	uint8_t job;
	uint8_t level;
	uint32_t offer;
	uint8_t name_flag;
	char name[CHARACTER_NAME_MAX_LEN+1];
} TGuildMemberPacketData;

typedef struct packet_guild_sub_info
{
	uint16_t member_count;
	uint16_t max_member_count;
	uint32_t guild_id;
	uint32_t master_pid;
	uint32_t exp;
	uint8_t level;
	char name[GUILD_NAME_MAX_LEN+1];
	uint32_t gold;
	uint8_t has_land;
} TPacketGCGuildInfo;

typedef struct SGuildGrade
{
	char grade_name[GUILD_GRADE_NAME_MAX_LEN+1]; // 8+1 names: guild leader, guild members, etc.
	uint8_t auth_flag;
} TGuildGrade;

struct TOneGradeNamePacket
{
	uint8_t grade;
	char grade_name[GUILD_GRADE_NAME_MAX_LEN+1];
};

struct TOneGradeAuthPacket
{
	uint8_t grade;
	uint8_t auth;
};
#pragma pack()

enum
{
	GUILD_AUTH_ADD_MEMBER	= (1 << 0),
	GUILD_AUTH_REMOVE_MEMBER	= (1 << 1),
	GUILD_AUTH_NOTICE		= (1 << 2),
	GUILD_AUTH_USE_SKILL	= (1 << 3),
};

typedef struct SGuildData
{
	uint32_t	guild_id;
	uint32_t	master_pid;
	uint32_t	exp;
	uint8_t	level;
	char	name[GUILD_NAME_MAX_LEN+1];

	TGuildGrade	grade_array[GUILD_GRADE_COUNT];

	uint8_t	skill_point;
	uint8_t	abySkill[GUILD_SKILL_COUNT];

	int		power;
	int		max_power;

	int		ladder_point;

	int		win;
	int		draw;
	int		loss;

	int		gold;
} TGuildData;

struct TGuildCreateParameter
{
	CHARACTER * master;
	char name[GUILD_NAME_MAX_LEN+1];
};

typedef struct SGuildWar
{
	uint32_t war_start_time;
	uint32_t score;
	uint32_t state;
	uint8_t type;
	uint32_t map_index;

	SGuildWar(uint8_t type)
		: war_start_time(0),
	score(0),
	state(GUILD_WAR_RECV_DECLARE),
	type(type),
	map_index(0)
	{
	}
	bool IsWarBegin() const
	{
		return state == GUILD_WAR_ON_WAR;
	}
} TGuildWar;

class CGuild
{
	public:
		CGuild(TGuildCreateParameter& cp);
		explicit CGuild(uint32_t guild_id) { Load(guild_id); }
		~CGuild();

		uint32_t		GetID() const	{ return m_data.guild_id; }
		const char*	GetName() const	{ return m_data.name; }
		int		GetSP() const		{ return m_data.power; }
		uint32_t		GetMasterPID() const	{ return m_data.master_pid; }
		CHARACTER *	GetMasterCharacter();
		uint8_t		GetLevel() const		{ return m_data.level; }

		void		Reset() { m_data.power = m_data.max_power; }

		void		RequestDisband(uint32_t pid);
		void		Disband();

		void		RequestAddMember(CHARACTER * ch, int grade = 15);
		void		AddMember(TPacketDGGuildMember * p);

		bool		RequestRemoveMember(uint32_t pid);
		bool		RemoveMember(uint32_t pid);

		void		LoginMember(CHARACTER * ch);
		void		P2PLoginMember(uint32_t pid);

		void		LogoutMember(CHARACTER * ch);
		void		P2PLogoutMember(uint32_t pid);

		void		ChangeMemberGrade(uint32_t pid, uint8_t grade);
		bool		OfferExp(CHARACTER * ch, int amount);
		void		LevelChange(uint32_t pid, uint8_t level);
		void		ChangeMemberData(uint32_t pid, uint32_t offer, uint8_t level, uint8_t grade);

		void		ChangeGradeName(uint8_t grade, const char* grade_name);
		void		ChangeGradeAuth(uint8_t grade, uint8_t auth);
		void		P2PChangeGrade(uint8_t grade);

		bool		ChangeMemberGeneral(uint32_t pid, uint8_t is_general);

		bool		ChangeMasterTo(uint32_t dwPID);

		void		Packet(const void* buf, int size);

		void		SendOnlineRemoveOnePacket(uint32_t pid);
		void		SendAllGradePacket(CHARACTER * ch);
		void		SendListPacket(CHARACTER * ch);
		void		SendListOneToAll(uint32_t pid);
		void		SendLoginPacket(CHARACTER * ch, CHARACTER * chLogin);
		void		SendLogoutPacket(CHARACTER * ch, CHARACTER * chLogout);
		void		SendLoginPacket(CHARACTER * ch, uint32_t pid);
		void		SendLogoutPacket(CHARACTER * ch, uint32_t pid);
		void		SendGuildInfoPacket(CHARACTER * ch);
		void		SendGuildDataUpdateToAllMember(SQLMsg* pmsg);

		void		Load(uint32_t guild_id);
		void		SaveLevel();
		void		SaveSkill();
		void		SaveMember(uint32_t pid);

		int		GetMaxMemberCount(); 
		int		GetMemberCount() { return m_member.size(); }

		// GUILD_MEMBER_COUNT_BONUS
		void		SetMemberCountBonus(int iBonus);
		void		BroadcastMemberCountBonus();
		// END_OF_GUILD_MEMBER_COUNT_BONUS

		int		GetMaxGeneralCount() const	{ return 1 /*+ GetSkillLevel(GUILD_SKILL_DEUNGYONG)/3*/;}
		int		GetGeneralCount() const		{ return m_general_count; }

		TGuildMember*	GetMember(uint32_t pid);
		uint32_t			GetMemberPID(const std::string& strName);

		bool		HasGradeAuth(int grade, int auth_flag) const	{ return (bool)(m_data.grade_array[grade-1].auth_flag & auth_flag);}

		void		AddComment(CHARACTER * ch, const std::string& str);
		void		DeleteComment(CHARACTER * ch, uint32_t comment_id);

		void		RefreshComment(CHARACTER * ch);
		void		RefreshCommentForce(uint32_t player_id);

		int			GetSkillLevel(uint32_t vnum);
		void		SkillLevelUp(uint32_t dwVnum);
		void		UseSkill(uint32_t dwVnum, CHARACTER * ch, uint32_t pid);

		void		SendSkillInfoPacket(CHARACTER * ch) const;
		void		ComputeGuildPoints();

		void		GuildPointChange( uint8_t type, int amount, bool save = false );

		void		UpdateSkill(uint8_t grade, uint8_t* skill_levels);
		void		SendDBSkillUpdate(int amount = 0);

		void		SkillRecharge();
		bool		ChargeSP(CHARACTER * ch, int iSP);

		void		Chat(const char* c_pszText); 
		void		P2PChat(const char* c_pszText); // Guild chat

		void		SkillUsableChange(uint32_t dwSkillVnum, bool bUsable);

		// Guild Money
		void		RequestDepositMoney(CHARACTER * ch, int iGold);

		void		RecvMoneyChange(int iGold);
		void		RecvWithdrawMoneyGive(int iChangeGold); // If bGive==1, try to give it to the guild leader and send success/failure to the DB

		int		GetGuildMoney() const	{ return m_data.gold; }

		// War general
		void		GuildWarPacket(uint32_t guild_id, uint8_t bWarType, uint8_t bWarState);
		void		SendEnemyGuild(CHARACTER * ch);

		int		GetGuildWarState(uint32_t guild_id);
		bool		CanStartWar(uint8_t bGuildWarType);
		uint32_t		GetWarStartTime(uint32_t guild_id);
		bool		UnderWar(uint32_t guild_id); // At war?
		uint32_t		UnderAnyWar(uint8_t bType = GUILD_WAR_TYPE_MAX_NUM);

		// War map relative
		void		SetGuildWarMapIndex(uint32_t dwGuildID, int32_t lMapIndex);
		int			GetGuildWarType(uint32_t dwGuildOpponent);
		uint32_t		GetGuildWarMapIndex(uint32_t dwGuildOpponent);

		// War entry question
		void		GuildWarEntryAsk(uint32_t guild_opp);
		void		GuildWarEntryAccept(uint32_t guild_opp, CHARACTER * ch);

		// War state relative
		void		NotifyGuildMaster(const char* msg);
		void		RequestDeclareWar(uint32_t guild_id, uint8_t type);
		void		RequestRefuseWar(uint32_t guild_id); 

		bool		DeclareWar(uint32_t guild_id, uint8_t type, uint8_t state); 
		void		RefuseWar(uint32_t guild_id); 
		bool		WaitStartWar(uint32_t guild_id); 
		bool		CheckStartWar(uint32_t guild_id);	// check if StartWar method fails (call it before StartWar)
		void		StartWar(uint32_t guild_id);
		void		EndWar(uint32_t guild_id);
		void		ReserveWar(uint32_t guild_id, uint8_t type);

		// War points relative
		void		SetWarScoreAgainstTo(uint32_t guild_opponent, int newpoint);
		int			GetWarScoreAgainstTo(uint32_t guild_opponent);

		int			GetLadderPoint() const	{ return m_data.ladder_point; }
		void		SetLadderPoint(int point);

		void		SetWarData(int iWin, int iDraw, int iLoss) { m_data.win = iWin, m_data.draw = iDraw, m_data.loss = iLoss; }

		void		ChangeLadderPoint(int iChange);

		int			GetGuildWarWinCount() const { return m_data.win; }
		int			GetGuildWarDrawCount() const { return m_data.draw; }
		int			GetGuildWarLossCount() const { return m_data.loss; }

		bool		HasLand();

		// GUILD_JOIN_BUG_FIX
		/// Invite a character to join the guild.
		/**
		 * @param	pchInviter inviting character.
		 * @param	pchInvitee character to invite.
		 *
		 * If unable to invite or be invited, sends the corresponding chat message.
		 */
		void		Invite( CHARACTER * pchInviter, CHARACTER * pchInvitee );

		/// Handle the other character's acceptance of a guild invite.
		/**
		 * @param	pchInvitee invited character
		 *
		 * If unable to join the guild, sends the corresponding chat message.
		 */
		void		InviteAccept( CHARACTER * pchInvitee );

		/// Handle the other character's refusal of a guild invite.
		/**
		 * @param	dwPID PID of the invited character
		 */
		void		InviteDeny( uint32_t dwPID );
		// END_OF_GUILD_JOIN_BUG_FIX

	private:
		void		Initialize();

		TGuildData	m_data;
		int		m_general_count;

		// GUILD_MEMBER_COUNT_BONUS
		int		m_iMemberCountBonus;
		// END_OF_GUILD_MEMBER_COUNT_BONUS

		typedef std::map<uint32_t, TGuildMember> TGuildMemberContainer;
		TGuildMemberContainer m_member;

		typedef CHARACTER_SET TGuildMemberOnlineContainer;
		TGuildMemberOnlineContainer m_memberOnline;

		typedef std::set<uint32_t>	TGuildMemberP2POnlineContainer;
		TGuildMemberP2POnlineContainer m_memberP2POnline;

		void LoadGuildData(SQLMsg* pmsg);
		void LoadGuildGradeData(SQLMsg* pmsg);
		void LoadGuildMemberData(SQLMsg* pmsg);

		void __P2PUpdateGrade(SQLMsg* pmsg);

		typedef std::map<uint32_t, TGuildWar> TEnemyGuildContainer;
		TEnemyGuildContainer m_EnemyGuild;

		std::map<uint32_t, uint32_t> m_mapGuildWarEndTime;

		bool	abSkillUsable[GUILD_SKILL_COUNT];

		// GUILD_JOIN_BUG_FIX
		/// Error codes for when joining a guild is not possible.
		enum GuildJoinErrCode {
			GERR_NONE			= 0,	///< Success
			GERR_WITHDRAWPENALTY,		///< Rejoin cooldown after leaving has not elapsed
			GERR_COMMISSIONPENALTY,		///< Rejoin cooldown after disbanding has not elapsed
			GERR_ALREADYJOIN,			///< Target character is already in a guild
			GERR_GUILDISFULL,			///< Guild member limit exceeded
			GERR_GUILD_IS_IN_WAR,		///< Guild is currently at war
			GERR_INVITE_LIMIT,			///< Guild member joining is restricted
			GERR_MAX				///< Max error code. Add new error codes before this.
		};

		/// Check whether the conditions for joining the guild are met.
		/**
		 * @param [in]	pchInvitee invited character
		 * @return	GuildJoinErrCode
		 */
		GuildJoinErrCode	VerifyGuildJoinableCondition( CHARACTER * const pchInvitee );

		typedef std::map< uint32_t, LPEVENT >	EventMap;
		EventMap	m_GuildInviteEventMap;	///< Guild invite event map. key: PID of the invited character
		// END_OF_GUILD_JOIN_BUG_FIX
};
