#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/common/singleton.h>
#include <metin2/net/io.h>

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace metin2::game::inline protocol {

class login_key;

class desc_manager : public singleton<desc_manager>
{
	public:
		typedef std::unordered_set<desc *>			desc_set;
		typedef std::unordered_set<client_desc *>	client_desc_set;
		typedef std::map<int, desc *>					desc_handle_map;
		typedef std::map<uint32_t, desc *>					desc_handshake_map;
		typedef std::unordered_map<std::string, desc *>	desc_loginname_map;
		typedef std::map<uint32_t, uint32_t>					desc_handle_random_key_map;

	public:
		desc_manager();
		~desc_manager() override;

		void			initialize();
		void			destroy();

		desc *			accept_desc(net::tcp::socket socket);
		desc *			accept_p2_p_desc(net::tcp::socket socket);
		void			destroy_desc(desc * d, bool erase_from_set = true);

		uint32_t			create_handshake();

		client_desc *		create_connection_desc(const char * host, uint16_t port, int phase_when_succeed, bool retry_when_closed);
		void			try_connect();

		desc *			find_by_handle(uint32_t handle);

		desc *			find_by_character_name(const char* name);
		desc *			find_by_login_name(const std::string& login);
		void			connect_account(const std::string& login, desc * d);
		void			disconnect_account(const std::string& login);

		void			destroy_closed();

		void			update_local_user_count();
		uint32_t			local_user_count() { return local_user_count_; }
		void			user_count(int & total, int ** pai_empire_user_count, int & local_count);

		const desc_set &	client_set();

		uint32_t			make_random_key(uint32_t handle);
		bool			check_random_key(uint32_t handle, uint32_t key);

		uint32_t			create_login_key(desc * d);
		desc *			find_by_login_key(uint32_t key);
		void			process_expired_login_key();

		bool			is_p2_p_desc_exist(const char * host, uint16_t port);


	private:

		desc_handle_random_key_map	map_handle_random_key_;

		client_desc_set		set_pk_client_desc_;
		desc_set			set_pk_desc_;

		desc_handle_map			map_handle_;
		desc_handshake_map		map_handshake_;
		desc_loginname_map		map_login_name_;
		std::map<uint32_t, login_key *>	map_pk_login_key_;

		int				sockets_connected_;

		int				handle_count_;

		int				local_user_count_;
		int				empire_user_count_[EMPIRE_MAX_NUM];

		bool			destroyed_;

};
}
