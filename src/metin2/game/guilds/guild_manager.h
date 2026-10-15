#pragma once
#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>
#include <metin2/proto/tables.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game::inline guilds {

class guild;
struct guild_create_parameter;

inline constexpr const char GUILD_SYMBOL_FILENAME[] = "guild_symbol.tga";
class guild_war_reserve_for_game
{
	public:
		guild_war_reserve				data;
		std::map<std::string, std::pair<uint32_t, uint32_t> > bet;
};

class guild_manager : public singleton<guild_manager>
{
	public:
		guild_manager();
		~guild_manager() override;

		uint32_t		create_guild(guild_create_parameter& gcp);
		guild *	find_guild(uint32_t guild_id);
		guild *	find_guild_by_name(const std::string guild_name);
		void		load_guild(uint32_t guild_id);
		guild *	touch_guild(uint32_t guild_id);
		void		disband_guild(uint32_t guild_id);

		void		initialize();

		void		link(uint32_t pid, guild* guild);
		void		unlink(uint32_t pid);
		guild *	linked_guild(uint32_t pid);

		void		login_member(character * ch);
		void		p2_p_login_member(uint32_t pid);
		void		p2_p_logout_member(uint32_t pid);

		void		skill_recharge();

		void		show_guild_war_list(character * ch);
		void		send_guild_war(character * ch);

		void		request_end_war(uint32_t guild_id1, uint32_t guild_id2);
		void		request_cancel_war(uint32_t guild_id1, uint32_t guild_id2);
		void		request_war_over(uint32_t dwGuild1, uint32_t dwGuild2, uint32_t dwGuildWinner, int32_t lReward);

		void		declare_war(uint32_t guild_id1, uint32_t guild_id2, uint8_t bType);
		void		refuse_war(uint32_t guild_id1, uint32_t guild_id2);
		void		start_war(uint32_t guild_id1, uint32_t guild_id2);
		void		wait_start_war(uint32_t guild_id1, uint32_t guild_id2);
		void		war_over(uint32_t guild_id1, uint32_t guild_id2, bool bDraw);
		void		cancel_war(uint32_t guild_id1, uint32_t guild_id2);
		bool		end_war(uint32_t guild_id1, uint32_t guild_id2);
		void		reserve_war(uint32_t dwGuild1, uint32_t dwGuild2, uint8_t bType);

		void            reserve_war_add(guild_war_reserve * p);
		void            reserve_war_delete(uint32_t dwID);
		std::vector<guild_war_reserve_for_game *> & reserve_war_ref();
		void		reserve_war_bet(packet_gd_guild_war_bet * p);
		bool		is_bet(uint32_t dwID, const char * c_pszLogin);

		void		stop_all_guild_war();

		void		kill(character * killer, character * victim);

		int		rank(guild* g);
		void		high_rank_string(uint32_t dwMyGuild, char * buffer, size_t buflen);
		void		around_rank_string(uint32_t dwMyGuild, char * buffer, size_t buflen);

		template <typename Func> void for_each_war(Func & f);

		int		disband_delay();
		int		withdraw_delay();

		void		change_master(uint32_t dwGID);

	private:
		typedef std::map<uint32_t, guild*> guild_map;	
		std::map<uint32_t, std::unique_ptr<guild>> guild_;

		typedef std::set<std::pair<uint32_t, uint32_t> > guild_war_container;
		guild_war_container guild_war_;

		typedef std::map<std::pair<uint32_t, uint32_t>, uint32_t> guild_war_end_time_container;
		guild_war_end_time_container guild_war_end_time_;

		guild_map				map_pk_guild_by_pid_;

		std::map<uint32_t, std::unique_ptr<guild_war_reserve_for_game>>	map_k_reserve_war_;
		std::vector<guild_war_reserve_for_game *>		vec_k_reserve_war_;

		friend class guild;
};

template <typename Func> void guild_manager::for_each_war(Func & f)
{
	for (auto it = guild_war_.begin(); it != guild_war_.end(); ++it)
	{
		f(it->first, it->second);
	}
}

extern void send_guild_war_score(uint32_t dwGuild, uint32_t dwGuildOpp, int iDelta, int iBetScoreDelta = 0);
}
