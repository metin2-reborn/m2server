#pragma once

#include <metin2/game/app/typedef.h>
#include <metin2/game/world/vid.h>

#include <metin2/proto/length.h>
#include <metin2/common/singleton.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <unordered_map>

namespace metin2::game::inline world {
    class dungeon;
}

namespace metin2::game::inline characters {

class character;
class character_vector_interactor;

class character_manager : public singleton<character_manager>
{
	public:
		typedef std::unordered_map<std::string, character *> name_map;

		character_manager();
		~character_manager() override;

		void                    destroy();

		void			graceful_shutdown();	// Used for a normal shutdown. Saves all PCs and destroys them.

		uint32_t			alloc_vid();

		character *             create_character(const char * name, uint32_t pid = 0);
		void destroy_character(character * ch);

		void			update(int pulse);

		character *		spawn_mob(uint32_t vnum, int32_t map_index, int32_t x, int32_t y, int32_t z, bool spawn_motion = false, int rot = -1, bool show_value = true);
		character *		spawn_mob_range(uint32_t vnum, int32_t map_index, int sx, int sy, int ex, int ey, bool is_exception=false, bool spawn_motion = false , bool aggressive = false);
		character *		spawn_group(uint32_t vnum, int32_t map_index, int sx, int sy, int ex, int ey, regen * regen_value = nullptr, bool aggressive = false, dungeon * dungeon_value = nullptr);
		bool			spawn_group_group(uint32_t vnum, int32_t map_index, int sx, int sy, int ex, int ey, regen * regen_value = nullptr, bool aggressive = false, dungeon * dungeon_value = nullptr);
		bool			spawn_move_group(uint32_t vnum, int32_t map_index, int sx, int sy, int ex, int ey, int tx, int ty, regen * regen_value = nullptr, bool aggressive = false);
		character *		spawn_mob_random_position(uint32_t vnum, int32_t map_index);

		void			select_stone(character * chr);

		name_map &		pc_map() { return map_pk_pc_chr_; }

		character *		find(uint32_t vid);
		character *		find(const vid & vid);
		character *		find_pc(const char * name);
		character *		find_by_pid(uint32_t pid);

		bool			add_to_state_list(character * ch);
		void			remove_from_state_list(character * ch);

		// DelayedSave: if a routine does many things that require saving,
		// there are too many queries, so just mark it "to be saved" and save it
		// a moment later (e.g. 1 frame).
		void                    delayed_save(character * ch);
		bool                    flush_delayed_save(character * ch); // If in the delayed list, remove it and save. Used when handling disconnects.
		void			process_delayed_save();

		template<class Func>	Func for_each_pc(Func f);

		void			register_for_monster_log(character * ch);
		void			unregister_for_monster_log(character * ch);
		void			packet_monster_log(character * ch, const void* buf, int size);

		void			kill_log(uint32_t vnum);

		void			register_race_num(uint32_t vnum);
		void			register_race_num_map(character * ch);
		void			unregister_race_num_map(character * ch);
		bool			characters_by_race_num(uint32_t race_num, character_vector_interactor & i);

		character *		find_specify_pc(unsigned int job_flag, int32_t map_index_value, character * except=nullptr, int min_level = 1, int max_level = PLAYER_MAX_LEVEL_CONST);

		void			set_mob_item_rate(int value)	{ mob_item_rate_ = value; }
		void			set_mob_damage_rate(int value)	{ mob_damage_rate_ = value; }
		void			set_mob_gold_amount_rate(int value)	{ mob_gold_amount_rate_ = value; }
		void			set_mob_gold_drop_rate(int value)	{ mob_gold_drop_rate_ = value; }
		void			set_mob_exp_rate(int value)	{ mob_exp_rate_ = value; }

		void			set_mob_item_rate_premium(int value)	{ mob_item_rate_premium_ = value; }
		void			set_mob_gold_amount_rate_premium(int value)	{ mob_gold_amount_rate_premium_ = value; }
		void			set_mob_gold_drop_rate_premium(int value)	{ mob_gold_drop_rate_premium_ = value; }
		void			set_mob_exp_rate_premium(int value)		{ mob_exp_rate_premium_ = value; }

		int			mob_item_rate(character * ch);
		int			mob_damage_rate(character * ch);
		int			mob_gold_amount_rate(character * ch);
		int			mob_gold_drop_rate(character * ch);
		int			mob_exp_rate(character * ch);

		void		send_script_to_map(int32_t map_index, const std::string & s);

		bool			begin_pending_destroy();
		void			flush_pending_destroy();

	private:
		int					mob_item_rate_;
		int					mob_damage_rate_;
		int					mob_gold_amount_rate_;
		int					mob_gold_drop_rate_;
		int					mob_exp_rate_;

		int					mob_item_rate_premium_;
		int					mob_gold_amount_rate_premium_;
		int					mob_gold_drop_rate_premium_;
		int					mob_exp_rate_premium_;

		int					vid_count_;

		std::unordered_map<uint32_t, character *> map_pk_chr_by_vid_;
		std::unordered_map<uint32_t, character *> map_pk_chr_by_pid_;
		name_map			map_pk_pc_chr_;

		character_set		set_pk_chr_state_;	// Those with a running FSM
		character_set		set_pk_chr_for_delayed_save_;
		character_set		set_pk_chr_monster_log_;

		character *			chr_selected_stone_;

		std::map<uint32_t, uint32_t> map_dw_mob_kill_count_;

		std::set<uint32_t>		set_dw_registered_race_num_;
		std::map<uint32_t, character_set> map_pk_chr_by_race_num_;

		bool				use_pending_destroy_;
		character_set		set_pk_chr_pending_destroy_;

};

	template<class Func>	
Func character_manager::for_each_pc(Func f)
{
	std::unordered_map<uint32_t, character *>::iterator it;

	for (it = map_pk_chr_by_pid_.begin(); it != map_pk_chr_by_pid_.end(); ++it)
		f(it->second);

	return f;
}

class character_vector_interactor : public character_vector
{
	public:
		character_vector_interactor() : my_begin_(false) { }

		character_vector_interactor(const character_set & r);
		virtual ~character_vector_interactor();

	private:
		bool my_begin_;
};
}
