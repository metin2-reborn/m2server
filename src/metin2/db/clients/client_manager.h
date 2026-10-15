// vim:ts=8 sw=4
#pragma once

#include <metin2/db/login/login_data.h>
#include <metin2/db/clients/peer.h>

#include <metin2/proto/building.h>
#include <metin2/proto/length.h>
#include <metin2/common/singleton.h>
#include <metin2/proto/tables.h>
#include <metin2/net/io.h>
#include <metin2/net/listener.h>
#include <metin2/sql/async_sql.h>

#include <mysql.h>

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace metin2::db::inline caches {
    class item_cache;
    class item_price_list_table_cache;
    class player_table_cache;
}

namespace metin2::db::inline clients {


std::string create_player_save_query(const player_table * tab);

class client_manager : public singleton<client_manager>
{
    public:
	typedef std::list<peer *>			peer_list;
	typedef std::unordered_map<uint32_t, player_table_cache *> player_table_cache_map;
	typedef std::unordered_map<uint32_t, item_cache *> item_cache_map;
	typedef std::unordered_set<item_cache *, std::hash<item_cache*> > item_cache_set;
	typedef std::unordered_map<uint32_t, std::unique_ptr<item_cache_set>> item_cache_set_ptr_map;
	typedef std::unordered_map<uint32_t, item_price_list_table_cache*> item_price_list_cache_map;
	typedef std::unordered_map<short, uint8_t> channel_status_map;

	// MYSHOP_PRICE_LIST
	/// Item price list request info
	/**
	 * first: Peer handle
	 * second: ID of the requesting player
	 */
	typedef std::pair< uint32_t, uint32_t >		item_pricelist_req_info;
	// END_OF_MYSHOP_PRICE_LIST

	class client_handle_info
	{
	    public:
		uint32_t	handle;
		uint32_t	account_id;
		uint32_t	player_id;
		uint8_t	account_index;
		char	login[LOGIN_MAX_LEN + 1];
		char	safebox_password[SAFEBOX_PASSWORD_MAX_LEN + 1];
		char	ip[MAX_HOST_LENGTH + 1];

		std::unique_ptr<proto::account_table> account_table;
		std::unique_ptr<safebox_table> safebox;

		client_handle_info(uint32_t arg_handle, uint32_t pid = 0)
		{
		    handle = arg_handle;
		    player_id = pid;
		};
		//Constructor for the German gift feature
		client_handle_info(uint32_t arg_handle, uint32_t pid, uint32_t account_id_value)
		{
		    handle = arg_handle;
		    player_id = pid;
			account_id = account_id_value;
		};
	};

	public:
	client_manager();
	~client_manager() override;

	bool	initialize();
	time_t	current_time();

	// Handles the packets a peer has received (CPeer::on_receive).
	void	process_packets(peer * peer);

	void	main_loop();
	void	quit();

	void	set_table_postfix(const char* table_postfix);
	void	set_player_id_start(int id_start);

	uint32_t	user_count();	// Returns the number of connected users.

	void	send_time();

	player_table_cache *	player_cache(uint32_t id);
	void			put_player_cache(player_table * updated);

	void			create_item_cache_set(uint32_t pid);
	item_cache_set *		get_item_cache_set(uint32_t pid);
	void			flush_item_cache_set(uint32_t pid);

	item_cache *		get_item_cache(uint32_t id);
	void			put_item_cache(player_item * updated, bool skip_query = false);
	bool			delete_item_cache(uint32_t id);

	void			update_player_cache();
	void			update_item_cache();

	// MYSHOP_PRICE_LIST
	/// Gets the price list cache.
	/**
	 * @param [in]	dwID owner of the price list (player ID)
	 * @return	pointer to the price list cache
	 */
	item_price_list_table_cache*	item_price_list_cache(uint32_t id);

	/// Puts a price list cache.
	/**
	 * @param [in]	pItemPriceList item price list to put in the cache
	 *
	 * If the cache already exists it is replaced, not updated.
	 */
	void			put_item_price_list_cache(const item_price_list_table* item_price_list);

	/// Flushes item price list caches whose flush time has expired and removes them from the cache.
	void			update_item_price_list_cache(void);
	// END_OF_MYSHOP_PRICE_LIST

	void			send_guild_skill_usable(uint32_t guild_id, uint32_t skill_vnum_value, bool usable_value);

	void			set_cache_flush_count_limit(int limit);

	template <class Func>
	Func		for_each_peer(Func f);

	peer *		any_peer();

	void			forward_packet(uint8_t header, const void* data, int size, uint8_t channel_value = 0, peer * except = nullptr);

	void			send_notice(const char * format, ...);

	// Tells the game about an item award whose why column starts with "[GIFT]" (German gift feature).
	void			inform_gift_award(const char * login, const char * why, uint32_t vnum);
	void			item_award(peer * peer, char* login);	//German gift feature

    protected:
	void	destroy();

    private:
	bool		initialize_tables();
	bool		initialize_shop_table();
	bool		initialize_mob_table();
	bool		initialize_item_table();
	bool		initialize_skill_table();
	bool		initialize_refine_table();
	bool		initialize_banword_table();
	bool		initialize_item_attr_table();
	bool		initialize_item_rare_table();
	bool		initialize_land_table();
	bool		initialize_object_proto();
	bool		initialize_object_table();
	bool		initialize_monarch();

	// Applies mob_proto and item_proto read from mob_proto.txt, item_proto.txt to the real db.
	//	Even without applying item_proto, mob_proto to the db the game runs fine,
	//	but problems occur because admin tools etc. read item_proto, mob_proto from the db.
	bool		mirror_mob_table_into_db();
	bool		mirror_item_table_into_db();

	void		add_peer(net::tcp::socket socket);
	void		remove_peer(peer * peer_value);
	peer *		get_peer(uint32_t ident);

	int		analyze_query_result(ql_msg * msg);

	int		process();

	login_data *	get_login_data(uint32_t key);
	login_data *	get_login_data_by_login(const char * login);
	login_data *	get_login_data_by_aid(uint32_t aid);

	void		insert_login_data(login_data * ld);
	void		delete_login_data(login_data * ld);

	bool		insert_logon_account(const char * login, uint32_t handle);
	bool		delete_logon_account(const char * login, uint32_t handle);
	bool		find_logon_account(const char * login);

	void		guild_create(peer * peer, uint32_t guild_id);
	void		guild_skill_update(peer * peer, packet_guild_skill_update* p);
	void		guild_exp_update(peer * peer, packet_guild_exp_update* p);
	void		guild_add_member(peer * peer, packet_gd_guild_add_member* p);
	void		guild_change_grade(peer * peer, packet_guild* p);
	void		guild_remove_member(peer * peer, packet_guild* p);
	void		guild_change_member_data(peer * peer, packet_guild_change_member_data* p);
	void		guild_disband(peer * peer, packet_guild * p);
	void		guild_war(peer * peer, packet_guild_war * p);
	void		guild_war_score(peer * peer, packet_guild_war_score * p);
	void		guild_change_ladder_point(packet_guild_ladder_point* p);
	void		guild_use_skill(packet_guild_use_skill* p);
	void		guild_deposit_money(packet_gd_guild_money* p);
	void		guild_withdraw_money(peer* peer, packet_gd_guild_money* p);
	void		guild_withdraw_money_give_reply(packet_gd_guild_money_withdraw_give_reply* p);
	void		guild_war_bet(packet_gd_guild_war_bet * p);
	void		guild_change_master(packet_change_guild_master* p);

	void		query_boot(peer * peer, packet_gd_boot * p);

	void		query_logout(peer * peer, uint32_t handle_value, const char *);

	void		result_login(peer * peer, ql_msg *msg);

	void		query_player_load(peer * peer, uint32_t handle_value, player_load_packet*);
	void		result_composite_player(peer * peer, ql_msg * msg, uint32_t qid);
	void		result_player_load(peer * peer, MYSQL_RES * res, client_handle_info * info_value);
	void		result_item_load(peer * peer, MYSQL_RES * res, uint32_t handle, uint32_t pid);
	void		result_quest_load(peer * peer, MYSQL_RES * res, uint32_t handle, uint32_t pid);
	void		result_affect_load(peer * peer, MYSQL_RES * res, uint32_t handle);

	// PLAYER_INDEX_CREATE_BUG_FIX
	void		result_player_index_create(peer *peer_value, ql_msg *msg);
	// END_PLAYER_INDEX_CREATE_BUG_FIX
 
	// MYSHOP_PRICE_LIST
	/// Handles the result of the price info load query
	/**
	 * @param	peer pointer to the peer object of the game server that requested the price info
	 * @param	pMsg pointer to the object received as the query result
	 *
	 * Stores the loaded price list in the cache and sends the list to the peer.
	 */
	void		result_pricelist_load(peer* peer, ql_msg* msg);

	/// Handles the result of the load query for a price info update
	/**
	 * @param	pMsg pointer to the object received as the query result
	 *
	 * Builds the price list cache from the loaded info and updates it with the received price info.
	 */
	void		result_pricelist_load_for_update(ql_msg* msg);
	// END_OF_MYSHOP_PRICE_LIST

	void		query_player_save(peer * peer, uint32_t handle, player_table*);

	void		query_player_create(peer * peer, uint32_t handle, player_create_packet *);
	void		query_player_delete(peer * peer, uint32_t handle_value, player_delete_packet *);
	void		result_player_delete(peer * peer, ql_msg* msg);

	void		query_player_count(peer * peer_value, player_count_packet *);

	void		query_item_save(peer * peer_value, const char * data);
	void		query_item_destroy(peer * peer_value, const char * data);
	void		query_item_flush(peer * peer_value, const char * data);

	void		query_quest_save(peer * peer_value, quest_table *, uint32_t len);
	void		query_add_affect(peer * peer, packet_gd_add_affect * p);
	void		query_remove_affect(peer * peer, packet_gd_remove_affect * p);

	void		query_safebox_load(peer * peer_value, uint32_t handle_value, safebox_load_packet *, bool mall);
	void		query_safebox_save(peer * peer_value, safebox_table * table);
	void		query_safebox_change_size(peer * peer_value, uint32_t handle_value, safebox_change_size_packet * p);
	void		query_safebox_change_password(peer * peer_value, uint32_t handle_value, safebox_change_password_packet * p);

	void		result_safebox_load(peer * peer_value, ql_msg * msg);
	void		result_safebox_change_size(peer * peer_value, ql_msg * msg);
	void		result_safebox_change_password(peer * peer_value, ql_msg * msg);
	void		result_safebox_change_password_second(peer * peer_value, ql_msg * msg);

	void		query_empire_select(peer * peer_value, uint32_t handle, empire_select_packet * p);
	void		query_setup(peer * peer, uint32_t handle_value, const char * data);

	void		send_party_on_setup(peer * peer_value);

	void		query_highscore_register(peer * peer, packet_gd_highscore* data);
	void		result_highscore_register(peer * peer_value, ql_msg * msg);

	void		query_flush_cache(peer * peer_value, const char * data);

	void		query_party_create(peer * peer, packet_party_create* p);
	void		query_party_delete(peer * peer, packet_party_delete* p);
	void		query_party_add(peer * peer, packet_party_add* p);
	void		query_party_remove(peer * peer, packet_party_remove* p);
	void		query_party_state_change(peer * peer, packet_party_state_change* p);
	void		query_party_set_member_level(peer * peer, packet_party_set_member_level* p);

	void		query_reload_proto();

	void		query_change_name(peer * peer, uint32_t handle, packet_gd_change_name * p);

	void		query_sms(peer * peer_value, packet_gdsms * pack);

	void		add_guild_priv(packet_give_guild_priv* p);
	void		add_empire_priv(packet_give_empire_priv* p);
	void		add_character_priv(packet_give_character_priv* p);

	void		money_log(packet_money_log* p);

	void		query_auth_login(peer * peer_value, uint32_t handle, packet_gd_auth_login * p);

	void		query_login_by_key(peer * peer_value, uint32_t handle_value, packet_gd_login_by_key * p);
	void		result_login_by_key(peer * peer, ql_msg * msg);

	void		charge_cash(const request_charge_cash * packet);

	void		load_event_flag();
	void		set_event_flag(packet_set_event_flag* p);
	void		send_event_flags_on_setup(peer* peer);

	// Marriage
	void		marriage_add(packet_marriage_add * p);
	void		marriage_update(packet_marriage_update * p);
	void		marriage_remove(packet_marriage_remove * p);

	void		wedding_request(packet_wedding_request * p);
	void		wedding_ready(packet_wedding_ready * p);
	void		wedding_end(packet_wedding_end * p);

	// MYSHOP_PRICE_LIST
	// Private shop price info

	/// Handler for the item price list update packet (HEADER_GD_MYSHOP_PRICELIST_UPDATE)
	/**
	 * @param [in]	pPacket pointer to the packet data
	 */
	void		myshop_pricelist_update(const item_price_list_table* packet);

	/// Handler for the item price list request packet (HEADER_GD_MYSHOP_PRICELIST_REQ)
	/**
	 * @param	peer pointer to the peer object of the game server that sent the packet
	 * @param [in]	dwHandle handle of the peer that requested the price info
	 * @param [in]	dwPlayerID ID of the player who requested the price list
	 */
	void		myshop_pricelist_request(peer* peer, uint32_t handle_value, uint32_t player_id);
	// END_OF_MYSHOP_PRICE_LIST

	// Building
	void		create_object(packet_gd_create_object * p);
	void		delete_object(uint32_t id);
	void		update_land(uint32_t * pdw);

	// BLOCK_CHAT
	void		block_chat(packet_block_chat * p);
	// END_OF_BLOCK_CHAT
   
    private:
	std::shared_ptr<net::listener>		listener_;	// accepts the game and auth servers
	peer_list				peer_list_;

	peer *					auth_peer_;

	// LoginKey, LoginData pair
	typedef std::unordered_map<uint32_t, login_data *> login_data_by_login_key;
	login_data_by_login_key			map_pk_login_data_;

	// Login LoginData pair
	typedef std::unordered_map<std::string, login_data *> login_data_by_login;
	login_data_by_login			map_pk_login_data_by_login_;
	
	// AccountID LoginData pair
	typedef std::unordered_map<uint32_t, login_data *> login_data_by_aid;
	login_data_by_aid				map_pk_login_data_by_aid_;

	// Login LoginData pair (accounts actually logged in)
	typedef std::unordered_map<std::string, login_data *> logon_account_map;
	logon_account_map			map_k_logon_account_;

	int					player_id_start_;
	int					player_delete_level_limit_;
	int					player_delete_level_limit_lower_;

	std::vector<mob_table>			vec_mob_table_;
	std::vector<item_table>			vec_item_table_;
	std::map<uint32_t, item_table *>		map_item_table_by_vnum_;

	int					shop_table_size_;
	std::unique_ptr<shop_table[]>		shop_table_;

	int					refine_table_size_;
	std::unique_ptr<refine_table[]>		refine_table_;

	std::vector<skill_table>		vec_skill_table_;
	std::vector<banword_table>		vec_banword_table_;
	std::vector<item_attr_table>		vec_item_attr_table_;
	std::vector<item_attr_table>		vec_item_rare_table_;

	std::vector<building::land_table>		vec_k_land_table_;
	std::vector<building::object_proto>	vec_k_object_proto_;
	std::map<uint32_t, std::unique_ptr<building::object_table>>	map_pk_object_table_;

	bool					shutdowned_;

	player_table_cache_map			map_player_cache_;  // keyed by player id

	item_cache_map				map_item_cache_;  // keyed by item id
	item_cache_set_ptr_map			map_pk_item_cache_set_ptr_;  // keyed by player id; which item caches does this player have?

	// MYSHOP_PRICE_LIST
	/// Per-player item price list map. key: player ID, value: price list cache
	item_price_list_cache_map item_price_list_cache_;  ///< Per-player item price lists
	// END_OF_MYSHOP_PRICE_LIST

	channel_status_map m_channel_status_;

	struct party_info
	{
	    uint8_t role;
	    uint8_t level;

		party_info() :role(0), level(0)
		{
		}
	};

	typedef std::map<uint32_t, party_info>	party_member;
	typedef std::map<uint32_t, party_member>	party_map;
	typedef std::map<uint8_t, party_map>	party_channel_map;
	party_channel_map map_pk_channel_party_;

	typedef std::map<std::string, int32_t>	event_flag_map;
	event_flag_map map_l_event_flag_;

	int					cache_flush_count_;
	int					cache_flush_count_limit_;

    private :
	item_id_range_table item_range_;

    public :
	bool initialize_now_item_id();
	uint32_t item_id();
	uint32_t gain_item_id();
	item_id_range_table item_range() { return item_range_; }

	//BOOT_LOCALIZATION
    public:
	/* Initialize local info 
	 **/
	bool initialize_localization(); 

    private:
	std::vector<t_locale> vec_locale_;
	//END_BOOT_LOCALIZATION
	//ADMIN_MANAGER

	bool admin_info(const char *ip, std::vector<t_admin_info> & admin_vec);
	bool host_info(std::vector<std::string> & ip_vec);
	//END_ADMIN_MANAGER

		
	//RELOAD_ADMIN
	void reload_admin(peer * peer, packet_reload_admin * p);
	//END_RELOAD_ADMIN
	void break_marriage(peer * peer, const char * data);

	struct logout_player
	{
	    uint32_t	pid;
	    time_t	time;

	};

	typedef std::unordered_map<uint32_t, std::unique_ptr<logout_player>> logout_player_map;
	logout_player_map map_logout_;
	
	void insert_logout_player(uint32_t pid);
	void delete_logout_player(uint32_t pid);
	void update_logout_player();
	void update_item_cache_set(uint32_t pid);

	void flush_player_cache_set(uint32_t pid);

	//MONARCH
	void election(peer * peer, uint32_t handle, const char * data);
	void candidacy(peer * peer, uint32_t handle, const char * data);
	void add_monarch_money(peer * peer, uint32_t handle, const char * data);
	void take_monarch_money(peer * peer, uint32_t handle, const char * data);
	void come_to_vote(peer * peer, uint32_t handle, const char * data);
	void rm_candidacy(peer * peer, uint32_t handle, const char * data);
	void set_monarch(peer * peer, uint32_t handle, const char * data);
	void rm_monarch(peer * peer, uint32_t handle, const char * data);
								

	void dec_monarch_money(peer * peer, uint32_t handle, const char * data);
	//END_MONARCH

	void change_monarch_lord(peer* peer, uint32_t handle, packet_change_monarch_lord* info);

	void send_spare_item_id_range(peer* peer);

	void update_horse_name(packet_update_horse_name* data, peer* peer);
	void ack_horse_name(uint32_t pid, peer* peer);
	void delete_login_key(packet_dc *data);
	void reset_last_player_id(const packet_need_login_log_info* data);
	//delete gift notify icon
	void delete_award_id(packet_delete_award_id* data);
	void update_channel_status(channel_status* data);
	void request_channel_status(peer* peer, uint32_t handle);
};

template<class Func>	
Func client_manager::for_each_peer(Func f)
{
    peer_list::iterator it;
    for (it = peer_list_.begin(); it!=peer_list_.end();++it)
    {
	f(*it);
    }
    return f;
}
// Packet and query statistics (client_manager.cpp).
extern int item_count;
extern int query_count[2];

bool create_player_table_from_res(MYSQL_RES * res, player_table * tab);
}
