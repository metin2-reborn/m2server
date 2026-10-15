#pragma once

#include <metin2/game/protocol/packet_info.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/tables.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

namespace metin2::game::inline protocol {

enum
{
	INPROC_CLOSE,
	INPROC_HANDSHAKE,
	INPROC_LOGIN,
	INPROC_MAIN,
	INPROC_DB,
	INPROC_P2P,
	INPROC_AUTH,
};

void login_failure(desc * d, const char * status_value);

// Copies a fixed-size string field of a client packet, which may lack its NUL terminator, into
// dst: at most N - 1 bytes and never past the end of the field. Always NUL-terminates dst.
template <size_t N, size_t M>
inline void copy_packet_string(char (&dst)[N], const char (&src)[M])
{
	utf8_strnlcpy(dst, N, src, M);
}

// The same for a field of srcMax bytes reached through a pointer.
inline void copy_packet_string(char * dst, size_t dst_size, const char * src, size_t src_max)
{
	utf8_strnlcpy(dst, dst_size, src, src_max);
}

class input_processor
{
	public:
		input_processor();
		virtual ~input_processor() {};

		virtual bool process(desc * desc_value, const void * orig, int bytes, int & r_i_bytes_proceed);
		virtual uint8_t type() = 0;

		void bind_packet_info(packet_info * packet_info_value);
		void pong(desc * d);
		void handshake(desc * d, const char * data);
		void version(character * ch, const char* data);

	protected:
		virtual int	analyze(desc * d, uint8_t header, const char * data) = 0;

		packet_info * packet_info_;
		int	buffer_left_;

		packet_info_cg 	packet_info_cg_;
};

class input_close : public input_processor
{
	public:
		uint8_t	type() override { return INPROC_CLOSE; }

		// A closing connection reads nothing: everything received is dropped unparsed.
		bool	process(desc * d, const void * orig, int bytes, int & r_i_bytes_proceed) override
		{
			r_i_bytes_proceed += bytes;
			buffer_left_ = 0;
			return true;
		}

	protected:
		int	analyze(desc * d, uint8_t header, const char * data) override { return 0; }
};

class input_handshake : public input_processor
{
	public:
		input_handshake();
		~input_handshake() override;

		uint8_t	type() override { return INPROC_HANDSHAKE; }

	protected:
		int	analyze(desc * d, uint8_t header_value, const char * data_value) override;

	protected:

		packet_info *	main_packet_info_;
		std::unique_ptr<packet_info_cg>	handshake_packet_info_;
};

class input_login : public input_processor
{
	public:
		uint8_t	type() override { return INPROC_LOGIN; }

	protected:
		int	analyze(desc * d, uint8_t header, const char * data) override;

	protected:
		void		login(desc * d, const char * data);
		void		login_by_key(desc * d, const char * data);

		void		character_select(desc * d, const char * data);
		void		character_create(desc * d, const char * data);
		void		character_delete(desc * d, const char * data);
		void		entergame(desc * d, const char * data);
		void		empire(desc * d, const char * data);
		void		guild_mark_crc_list(desc * d, const char* data);
		// MARK_BUG_FIX
		void		guild_mark_idx_list(desc * d, const char* data);
		// END_OF_MARK_BUG_FIX
		void		guild_mark_upload(desc * d, const char* data);
		int			guild_symbol_upload(desc * d, const char* data, size_t bytes);
		void		guild_symbol_crc(desc * d, const char* data);
		void		change_name(desc * d, const char * data);
};

class input_main : public input_processor
{
	public:
		uint8_t	type() override { return INPROC_MAIN; }

	protected:
		int	analyze(desc * d, uint8_t header, const char * data) override;

	protected:
		void		attack(character * ch, const uint8_t header, const char* data);

		int			whisper(character * ch, const char * data, size_t bytes);
		int			chat(character * ch, const char * data, size_t bytes);
		void		item_use(character * ch, const char * data);
		void		item_drop(character * ch, const char * data);
		void		item_drop2(character * ch, const char * data);
		void		item_move(character * ch, const char * data);
		void		item_pickup(character * ch, const char * data);
		void		item_to_item(character * ch, const char * pc_data);
		void		quickslot_add(character * ch, const char * data);
		void		quickslot_delete(character * ch, const char * data);
		void		quickslot_swap(character * ch, const char * data);
		int			shop(character * ch, const char * data, size_t bytes);
		void		on_click(character * ch, const char * data);
		void		exchange(character * ch, const char * data);
		void		position(character * ch, const char * data);
		void		move(character * ch, const char * data);
		int			sync_position(character * ch, const char * pc_data, size_t bytes);
		void		fly_target(character * ch, const char * pc_data, uint8_t header);
		void		use_skill(character * ch, const char * pc_data);
		
		void		script_answer(character * ch, const void * data);
		void		script_button(character * ch, const void * data);
		void		script_select_item(character * ch, const void * data);

		void		quest_input_string(character * ch, const void * data);
		void		quest_confirm(character * ch, const void* data);
		void		target(character * ch, const char * pc_data);
		void		warp(character * ch, const char * pc_data);
		void		safebox_checkin(character * ch, const char * data);
		void		safebox_checkout(character * ch, const char * data, bool mall_value);
		void		safebox_item_move(character * ch, const char * data);
		int			messenger(character * ch, const char* data, size_t bytes);

		void 		party_invite(character * ch, const char * data);
		void 		party_invite_answer(character * ch, const char * data);
		void		party_remove(character * ch, const char * data);
		void		party_set_state(character * ch, const char * data);
		void		party_use_skill(character * ch, const char * data);
		void		party_parameter(character * ch, const char * data);

		int			guild(character * ch, const char * data, size_t bytes);
		void		answer_make_guild(character * ch, const char* data);

		void		fishing(character * ch, const char* data);
		void		item_give(character * ch, const char* data);
		void		hack(character * ch, const char * data);
		int			my_shop(character * ch, const char * data, size_t bytes);

		void		refine(character * ch, const char* data);

};

class input_db : public input_processor
{
public:
	bool process(desc * d, const void * orig, int bytes, int & r_i_bytes_proceed) override;
	uint8_t type() override { return INPROC_DB; }

protected:
	int	analyze(desc * d, uint8_t header, const char * data) override;

protected:
	void		map_locations(const char * data);
	void		login_success(uint32_t handle, const char *data);
	void		player_create_failure(desc * d, uint8_t type_value);	// 0 = general failure 1 = already exists
	void		player_delete_success(desc * d, const char * data);
	void		player_delete_fail(desc * d);
	void		player_load(desc * d, const char* data);
	void		player_create_success(desc * d, const char * data);
	void		boot(const char* data);
	void		quest_load(desc * d, const char * data);
	void		safebox_load(desc * d, const char * data);
	void		safebox_change_size(desc * d, const char * data);
	void		safebox_wrong_password(desc * d);
	void		safebox_change_password_answer(desc * d, const char* data);
	void		mall_load(desc * d, const char * data);
	void		empire_select(desc * d, const char * data);
	void		p2_p(const char * data);
	void		item_load(desc * d, const char * data);
	void		affect_load(desc * d, const char * data);

	void		guild_load(const char * data);
	void		guild_skill_update(const char* data);
	void		guild_skill_recharge();
	void		guild_exp_update(const char* data);
	void		guild_add_member(const char* data);
	void		guild_remove_member(const char* data);
	void		guild_change_grade(const char* data);
	void		guild_change_member_data(const char* data);
	void		guild_disband(const char* data);
	void		guild_ladder(const char* data);
	void		guild_war(const char* data);
	void		guild_war_score(const char* data);
	void		guild_skill_usable_change(const char* data);
	void		guild_money_change(const char* data);
	void		guild_withdraw_money(const char* data);
	void		guild_war_reserve_add(guild_war_reserve * p);
	void		guild_war_reserve_delete(uint32_t id);
	void		guild_war_bet(packet_gd_guild_war_bet * p);
	void		guild_change_master(packet_change_guild_master* p);

	void		login_already(desc * d, const char * data);

	void		party_create(const char* data);
	void		party_delete(const char* data);
	void		party_add(const char* data);
	void		party_remove(const char* data);
	void		party_state_change(const char* data);
	void		party_set_member_level(const char* data);

	void		time(const char * data);

	void		reload_proto(const char * data);
	void		change_name(desc * d, const char * data);

	void		auth_login(desc * d, const char * data);
	void		item_award(const char * data);

	void		change_empire_priv(const char* data);
	void		change_guild_priv(const char* data);
	void		change_character_priv(const char* data);

	void		money_log(const char* data);

	void		set_event_flag(const char* data);

	void		create_object(const char * data);
	void		delete_object(const char * data);
	void		update_land(const char * data);

	void		notice(const char * data);

	void		marriage_add(packet_marriage_add * p);
	void		marriage_update(packet_marriage_update * p);
	void		marriage_remove(packet_marriage_remove * p);

	void		wedding_request(packet_wedding_request* p);
	void		wedding_ready(packet_wedding_ready* p);
	void		wedding_start(packet_wedding_start* p);
	void		wedding_end(packet_wedding_end* p);

	void		take_monarch_money(desc * d, const char * data );
	void		add_monarch_money(desc * d, const char * data );
	void		dec_monarch_money(desc * d, const char * data );
	void		set_monarch( desc * d, const char * data );

	void		change_monarch_lord(packet_change_monarch_lord_ack* info);
	void		update_monarch_info(monarch_info* info);

	// MYSHOP_PRICE_LIST
	/// Handler for the response packet to the item price list request (HEADER_DG_MYSHOP_PRICELIST_RES)
	/**
	* @param	d descriptor of the player who requested the item price list
	* @param	p pointer to the packet data
	*/
	void		myshop_pricelist_res( desc * d, const packet_myshop_pricelist_header* p );
	// END_OF_MYSHOP_PRICE_LIST
	//
	//RELOAD_ADMIN
	void reload_admin( const char * data );
	//END_RELOAD_ADMIN

	void		detail_log(const packet_need_login_log_info* info);
	// German gift feature test
	void		item_award_informer(packet_item_award_infromer* data);

	void		respond_channel_status(desc * desc, const char* pc_data);

	protected:
		uint32_t		handle_;
};

class input_p2_p : public input_processor
{
	public:
		input_p2_p();
		uint8_t	type() override { return INPROC_P2P; }

	protected:
		int	analyze(desc * d, uint8_t header, const char * data) override;

	public:
		void		setup(desc * d, const char * data);
		void		login(desc * d, const char * data);
		void		logout(desc * d, const char * data);
		int			relay(desc * d, const char * data, size_t bytes);
		int			notice(desc * d, const char * data, size_t bytes);
		int			monarch_notice(desc * d, const char * data, size_t bytes);
		int			monarch_transfer(desc * d, const char * data);
		int			guild(desc * d, const char* data, size_t bytes);
		void		shout(const char * data);
		void		disconnect(const char * data);
		void		messenger_add(const char * data);
		void		messenger_remove(const char * data);
		void		find_position(desc * d, const char* data);
		void		warp_character(const char* data);
		void		guild_war_zone_map_index(const char* data);
		void		transfer(const char * data);
		void		xmas_warp_santa(const char * data);
		void		xmas_warp_santa_reply(const char * data);
		void		block_chat(const char * data);
		void		iam_awake(desc * d, const char * data);

	protected:
		packet_info_gg 	packet_info_gg_;
};

class input_auth : public input_processor
{
	public:
		input_auth();
		uint8_t type() override { return INPROC_AUTH; }

	protected:
		int	analyze(desc * d, uint8_t header, const char * data) override;

	public:
		void		login(desc * d, const char * data);

};

extern bool race_to_job(unsigned race, unsigned *ret_job);
extern void send_shout(const char * text, uint8_t empire);
}
