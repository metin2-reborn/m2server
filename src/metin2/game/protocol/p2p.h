
#pragma once

#include <metin2/game/protocol/packet.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/common/singleton.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace metin2::game::inline protocol {

struct cci {
	char	name[CHARACTER_NAME_MAX_LEN + 1];
	uint32_t	pid;
	uint8_t	empire;
	int32_t	map_index;
	uint8_t	channel;

	game::protocol::desc *	desc;
};

class p2_p_manager : public singleton<p2_p_manager>
{
	public:
		p2_p_manager();
		~p2_p_manager() override;

		void			register_acceptor(desc * d);
		void			unregister_acceptor(desc * d);

		void			register_connector(desc * d);
		void			unregister_connector(desc * d);

		void			erase_user_by_desc(desc * d);

		void			flush_output();

		void			boot(desc * d);

		void			send(const void * data, int size, desc * except = nullptr);

		void			login(desc * d, const packet_gg_login * p);
		void			logout(const char * name);

		auto			find(const char * name) -> cci *;
		auto			find_by_pid(uint32_t pid) -> cci *;

		auto				count() -> int;
		auto				empire_user_count(int idx) -> int;
		auto				desc_count() -> int;
		void			p2_p_host_names(std::string& host_names);

	private:
		void			logout(cci * cci_value);

		int			handle_count_;

		typedef std::unordered_map<std::string, std::unique_ptr<cci>> cci_map;
		typedef std::unordered_map<uint32_t, cci*> pidcci_map;
		std::unordered_set<desc *> set_pk_peers_;
		cci_map			map_pk_cci_;
		pidcci_map		map_dw_pid_pk_cci_;
		int			empire_user_count_[EMPIRE_MAX_NUM];
};
}
