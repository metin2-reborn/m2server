#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/quest.h>
#include <metin2/game/quest/questnpc.h>
#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>
#include <metin2/log/log.h>

#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <set>
#include <source_location>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace metin2::game::inline characters {
    class character;
}
namespace metin2::game::inline world {
    class dungeon;
}

namespace metin2::game {


namespace quest
{
	bool is_script_true(const char* code, int size);
	std::string script_to_string(const std::string& str);

	class quest_manager : public singleton<quest_manager>
	{
		public:
			enum
			{
				QUEST_SKIN_NOWINDOW,
				QUEST_SKIN_NORMAL,
				//QUEST_SKIN_CINEMATIC,
				QUEST_SKIN_CINEMATIC=5,
				QUEST_SKIN_COUNT
			};

			typedef std::map<std::string, int>		event_name_map;
			typedef std::map<unsigned int, pc>	pc_map;

		public:
			quest_manager();
			~quest_manager() override;

			bool		initialize();
			void		destroy();

			bool		initialize_lua();
			lua_State *		lua_state() { return l_; }
			void		add_lua_function_table(const char * name_value, luaL_reg * preg);

			event_name_map	event_name;

			quest_state		open_state(const std::string& quest_name, int state_index);
			void		close_state(quest_state& qs);
			bool		run_state(quest_state& qs);

			pc *		get_pc(unsigned int pc);
			pc *		pc_force(unsigned int pc);	// Gets the PC pointer without changing the current PC.

			unsigned int	current_npc_race();
			const std::string & 	current_quest_name();
			unsigned int	find_npcid_by_name(const std::string& name);

			//void		SetCurrentNPCCharacterPtr(CHARACTER * ch) { m_pkCurrentNPC = ch; }
			character *		current_npc_character_ptr();

			void		set_current_event_index(int index) { running_event_index_ = index; }

			bool		use_item(unsigned int pc, item * item, bool receive_all);
			bool		pickup_item(unsigned int pc, item * item);
			bool		sig_use(unsigned int pc, uint32_t sig_vnum, item * item, bool receive_all);
			bool		take_item(unsigned int pc, unsigned int npc, item * item);
			item *		current_item();
			void		clear_current_item();
			void		set_current_item(item * item);
			void		add_server_timer(const std::string& name, uint32_t arg, event_ptr event);
			void		clear_server_timer(const std::string& name, uint32_t arg);
			void		clear_server_timer_not_cancel(const std::string& name, uint32_t arg);
			void		cancel_server_timers(uint32_t arg);

			void		set_server_timer_arg(uint32_t arg);
			uint32_t		server_timer_arg();

			// event over state and stae
			bool		server_timer(unsigned int npc, unsigned int arg);

			void		login(unsigned int pc, const char * quest = nullptr);
			void		logout(unsigned int pc);
			bool		timer(unsigned int pc, unsigned int npc);
			bool		click(unsigned int pc, character * chr_target);
			void		kill(unsigned int pc, unsigned int npc);
			void		level_up(unsigned int pc);
			void		attr_in(unsigned int pc, character * ch, int attr);
			void		attr_out(unsigned int pc, character * ch, int attr);
			bool		target(unsigned int pc, uint32_t quest_index, const char * target_name, const char * verb);
			bool		give_item_to_pc(unsigned int pc, character * chr);
			void		unmount(unsigned int pc);

			void		quest_button(unsigned int pc, unsigned int quest_index);
			void		quest_info(unsigned int pc, unsigned int quest_index);

			void		enter_state(uint32_t pc, uint32_t quest_index, int state);
			void		leave_state(uint32_t pc, uint32_t quest_index, int state);

			void		letter(uint32_t pc);
			void		letter(uint32_t pc, uint32_t quest_index, int state);
			
			void		item_informer(unsigned int pc, unsigned int vnum);	//German gift feature

			//

			bool		check_quest_loaded(pc* pc) { return pc && pc->is_loaded(); }

			// event occurs in one state
			void		select(unsigned int pc, unsigned int selection);
			void		resume(unsigned int pc);
			void		input(unsigned int pc, const char* msg);
			void		confirm(unsigned int pc, quest_confirm_type confirm, unsigned int pc2 = 0);
			void		select_item(unsigned int pc, unsigned int selection);

			void		logout_pc(character * ch);
			void		disconnect_pc(character * ch);

			quest_state *	current_state()	{ return current_running_state_; }

			void 		load_start_quest(const std::string& quest_name, unsigned int idx);
			bool		can_start_quest(unsigned int quest_index, const pc& pc);
			bool		can_start_quest(unsigned int quest_index);

			character *		current_character_ptr() { return current_character_; }
			character *		current_party_member() { return current_party_member_; }
			pc *		current_pc() { return current_pc_; }

			dungeon *		current_dungeon();
			void		select_dungeon(dungeon * dungeon_value);

			void		clear_script();
			void		send_script();
			void		add_script(const std::string& str);

			void		build_state_index_to_name(const char* quest_name);

			int			quest_state_index(const std::string& quest_name, const std::string& state_name);
			const char*		quest_state_name(const std::string& quest_name, const int state_index);

			void		set_skin_style(int style);

			void		set_no_send() { no_send_ = true; }

			unsigned int	load_timer_script(const std::string& name);

			void		register_quest(const std::string & quest_name, unsigned int idx);
			unsigned int 	quest_index_by_name(const std::string& name);
			const std::string& 	quest_name_by_index(unsigned int idx);

			void		request_set_event_flag(const std::string& name, int value);

			void		set_event_flag(const std::string& name, int value);
			int			event_flag(const std::string& name);
			void		broadcast_event_flag_on_login(character * ch);

			void		send_event_flag_list(character * ch);

			void		reload();

			void		set_error() { error_ = true; }
			void		clear_error() { error_ = false; }
			bool		is_error() { return error_; }
			void		write_running_state_to_syserr();

			void		register_npc_vnum(uint32_t vnum);

		private:
			dungeon *			selected_dungeon_;
			uint32_t			server_timer_arg_;

			std::map<std::pair<std::string, uint32_t>, event_ptr>	server_timer_;

			int				running_event_index_;

			std::map<std::string, int>		event_flag_;

			void			goto_select_state(quest_state& qs);
			void			goto_pause_state(quest_state& qs);
			void			goto_end_state(quest_state& qs);
			void			goto_input_state(quest_state& qs);
			void			goto_confirm_state(quest_state& qs);
			void			goto_select_item_state(quest_state& qs);

			lua_State *		l_;

			bool			no_send_;

			std::set<unsigned int>			registered_npc_vnum_;
			std::map<unsigned int, npc>		npc_;
			std::map<std::string, unsigned int>	npc_name_id_;
			std::map<std::string, unsigned int>	timer_id_;

			quest_state *	current_running_state_;

			pc_map			pc_;

			character *		current_character_;
			character *		current_party_member_;
			pc*				current_pc_;

			std::string			script_;

			int				current_skin_;

			struct stringhash
			{
				size_t operator () (const std::string& str) const
				{
					const unsigned char * s = (const unsigned char*) str.c_str();
					const unsigned char * end = s + str.size();
					size_t h = 0;

					while (s < end)
					{
						h *= 16777619;
						h ^= (unsigned char) *(unsigned char *) (s++);
					}

					return h;

				}
			};

			typedef std::unordered_map<std::string, int, stringhash> hash_map_quest_name;
			typedef std::unordered_map<unsigned int, std::vector<char> > hash_map_quest_start_script;

			hash_map_quest_name			hm_quest_name_;
			hash_map_quest_start_script	hm_quest_start_script_;
			std::map<unsigned int, std::string>	quest_name_by_index_;

			bool						error_;

		public:
			static bool execute_quest_script(pc& pc, const std::string& quest_name, const int state, const char* code, const int code_size, std::vector<a_arg_script*>* chat_scripts_value = nullptr, bool use_cache = true);
			static bool execute_quest_script(pc& pc, uint32_t quest_index, const int state, const char* code, const int code_size, std::vector<a_arg_script*>* chat_scripts = nullptr, bool use_cache = true);
		

		// Objects for begin_other_pc_blcok, end_other_pc_block.
		public:
			void		begin_other_pc_block(uint32_t pid);
			void		end_other_pc_block();
			bool		is_in_other_pc_block();
			pc*			other_pc_block_root_pc();
		private:
			pc*			other_pc_block_root_pc_;
			std::vector <uint32_t>	pc_stack_;
	};
};

namespace quest
{
	// Logs an error of a quest function; on test servers the current character sees it too.
	void report_error(const std::source_location & where, std::string_view message);

	template <typename... Args>
	void error(metin2::log::format_string<Args...> format, const Args &... args)
	{
		report_error(format.location, metin2::log::format(format, args...));
	}
}
}
