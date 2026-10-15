#pragma once

#include <metin2/game/protocol/cipher.h>
#include <metin2/game/app/event.h>
#include <metin2/game/protocol/input.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/tables.h>
#include <metin2/net/connection.h>
#include <metin2/core/utils.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace metin2::game::inline protocol {

inline constexpr int MAX_ALLOW_USER = 4096;
inline constexpr std::size_t MAX_CLIENT_INPUT = 256 * 1024;
inline constexpr std::size_t MAX_CLIENT_OUTPUT = 128 * 1024;
inline constexpr std::size_t MAX_SERVER_INPUT = 256 * 1024 * 1024;
inline constexpr std::size_t MAX_SERVER_OUTPUT = 256 * 1024 * 1024;
inline constexpr int HANDSHAKE_RETRY_LIMIT = 32;
class input_processor;

enum desc_type
{
	DESC_TYPE_ACCEPTOR,
	DESC_TYPE_CONNECTOR
};

class login_key
{
	public:
		login_key(uint32_t key_value, game::protocol::desc * desc_value) : key(key_value), desc(desc_value)
		{
			expire_time = 0;
		}

		void expire()
		{
			expire_time = get_dword_time();
			desc = nullptr;
		}

		uint32_t   key;
		uint32_t   expire_time;
		game::protocol::desc *  desc;
};

struct seq_t
{
	uint8_t	hdr;
	uint8_t	seq;
};
typedef std::vector<seq_t>	seq_vector_t;

class desc : public net::connection_handler
{
	public:
		EVENTINFO(desc_event_info)
		{
			game::protocol::desc * desc;

			desc_event_info() 
			: desc(nullptr)
			{
			}
		};

	public:
		desc();
		virtual ~desc();

		virtual uint8_t		type() { return DESC_TYPE_ACCEPTOR; }
		virtual void		destroy();
		virtual void		set_phase(int phase_value);

		void			flush_output();
		void			send_output_now();

		bool			setup(std::shared_ptr<net::connection> connection, uint32_t handle, uint32_t handshake);

		bool			is_connected() const	{ return connection_ != nullptr; }
		const char *	host_name()		{ return host_.c_str(); }

		void			set_p2_p(const char * h, uint16_t w) { p2_p_host_ = h; p2_p_port_ = w; }
		const char *	p2_p_host()		{ return p2_p_host_.c_str();	}
		uint16_t			p2_p_port() const		{ return p2_p_port_; }

		void			buffered_packet(const void * data_value, int size);
		void			packet(const void * data_value, int size_value);
		void			large_packet(const void * data, int size);

		auto			on_receive(std::span<std::uint8_t> input, std::size_t received) -> std::size_t override;
		void			on_close(const std::error_code & error) override;

		input_processor	*	get_input_processor()	{ return input_processor_; }

		uint32_t			handle() const	{ return handle_; }

		void			bind_account_table(account_table * account_table_value);
		account_table &		get_account_table()	{ return account_table_; }

		void			bind_character(character * ch);
		character *		get_character()		{ return character_; }

		bool			is_phase(int phase) const	{ return phase_ == phase ? true : false; }


		void			start_handshake(uint32_t handshake);
		void			send_handshake(uint32_t cur_time, int32_t new_delta);
		bool			handshake_process(uint32_t time, int32_t delta, bool infinite_retry=false);
		bool			is_handshaking();

		uint32_t			handshake() const	{ return handshake_; }
		uint32_t			client_time();

		void send_key_agreement();
		void send_key_agreement_completed();
		bool finish_handshake(size_t agreed_length, const void* buffer, size_t length);
		bool is_cipher_prepared();

		uint8_t			empire();

		void			set_relay(const char * name);
		bool			delayed_disconnect(int sec);
		void			disconnect_of_same_login();

		void			set_admin_mode();
		bool			is_admin_mode();

		void			set_mark_login(uint32_t owner_handle)	{ mark_login_ = true; mark_owner_handle_ = owner_handle; }
		bool			is_mark_login() const	{ return mark_login_; }
		uint32_t		mark_owner_handle() const	{ return mark_owner_handle_; }

		void			set_pong(bool b);
		bool			is_pong();

		uint8_t			sequence();
		void			set_next_sequence();

		void			send_login_success_packet();

		void			set_login_key(uint32_t key);
		void			set_login_key(login_key * key_value);
		uint32_t			get_login_key();

		void			assemble_crc_magic_cube(uint8_t proc_piece, uint8_t file_piece);

		void			set_client_version(const char * timestamp) { client_version_ = timestamp; }
		const char *		client_version() { return client_version_.c_str(); }

		bool			is_channel_status_requested() const { return channel_status_requested_; }
		void			set_channel_status_requested(bool channel_status_requested) { channel_status_requested_ = channel_status_requested; }

	protected:
		void			initialize();

	protected:
		input_processor *	input_processor_;
		input_close		input_close_;
		input_handshake	input_handshake_;
		input_login		input_login_;
		input_main		input_main_;
		input_auth		input_auth_;

		std::shared_ptr<net::connection>	connection_;
		int				phase_;
		uint32_t			handle_;

		std::string		host_;
		uint16_t			port_;
		time_t			last_try_to_connect_time_;

		uint32_t			handshake_;
		uint32_t			handshake_sent_time_;
		int				handshake_retry_;
		uint32_t			client_time_;
		bool			handshaking_;

		std::vector<uint8_t>	buffered_output_;

		event_ptr			ping_event_;
		character *		character_;
		account_table		account_table_;

		FILE *			log_file_;
		std::string		relay_name_;

		std::string		p2_p_host_;
		uint16_t			p2_p_port_;

		bool			admin_mode_;
		bool			mark_login_;
		uint32_t		mark_owner_handle_;
		bool			pong_;

		int			current_sequence_;

		login_key *		m_pk_login_key_;
		uint32_t			login_key_;

		uint8_t                    crc_magic_cube_idx_;
		uint32_t                   proc_crc_;
		uint32_t                   file_crc_;

		std::string		client_version_;

		std::string		login_;

		bool			destroyed_;
		bool			channel_status_requested_;

		cipher cipher_;

	public:
		event_ptr			disconnect_event;

	protected:

	public:

		void set_login( const char * login ) { login_ = login; }
		const std::string& login() { return login_; }

		void chat_packet(uint8_t type, const char * format, ...);

	public:
		seq_vector_t	seq_vector;
		void			push_seq (uint8_t hdr, uint8_t seq);
		
};
}
