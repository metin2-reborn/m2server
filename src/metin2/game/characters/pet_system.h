#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace metin2::game::inline characters {

class character;

class pet_actor
{
public:
	enum pet_options
	{
		EPetOption_Followable		= 1 << 0,
		EPetOption_Summonable		= 1 << 2,
		EPetOption_Combatable		= 1 << 3,		
	};

protected:
	friend class pet_system;

	pet_actor(character * owner, uint32_t vnum, uint32_t options = EPetOption_Followable | EPetOption_Summonable);

	virtual ~pet_actor();

	virtual auto	update(uint32_t delta_time) -> bool;

protected:
	virtual auto	update_follow_ai() -> bool;


private:
	auto follow(float min_distance = 50.f) -> bool;

public:
	auto		get_character()	const -> character *					{ return character_; }
	auto		owner()	const -> character *						{ return owner_; }
	auto			get_vid() const -> uint32_t							{ return vid_; }

	auto			has_option(pet_options option) const -> bool		{ return options_ & option; }

	void			set_name(const char* name);

	auto			summon(const char* pet_name, item * summon_item, bool spawn_far = false) -> uint32_t;
	void			unsummon();

	auto			is_summoned() const -> bool			{ return nullptr != character_; }
	void			set_summon_item (item * item_value);
	auto			summon_item_vid () -> uint32_t { return summon_item_vid_; }
	// point_change alone is reset by the next compute_points, so buffs are applied through
	// compute_points (which calls refresh_buff); clear_buff then compute_points removes them.
	void			give_buff();
	void			clear_buff();

private:
	uint32_t			vnum_;
	uint32_t			vid_;
	uint32_t			options_;
	uint32_t			summon_item_vid_;
	uint32_t			summon_item_vnum_;

	short			original_move_speed_;

	std::string		name_;

	character *		character_;
	character *		owner_;

};

class pet_system
{
public:
	typedef	std::unordered_map<uint32_t,	pet_actor*>		pet_actor_map;

public:
	pet_system(character * owner);
	virtual ~pet_system();

	auto	by_vnum(uint32_t vnum) const -> pet_actor*;

	auto		update(uint32_t delta_time) -> bool;
	void		destroy();

	auto		count_summoned() const -> size_t;

public:

	auto	summon(uint32_t mob_vnum, item * summon_item, const char* pet_name, bool spawn_far, uint32_t options = pet_actor::EPetOption_Followable | pet_actor::EPetOption_Summonable) -> pet_actor*;

	void		unsummon(uint32_t vnum, bool delete_from_list = false);
	void		unsummon(pet_actor* pet_actor, bool delete_from_list = false);


	void		delete_pet(pet_actor* pet_actor_value);
	void		refresh_buff();

private:
	pet_actor_map	pet_actor_map_;
	character *		owner_;
	uint32_t			update_period_;
	uint32_t			last_update_time_;
	event_ptr			pet_system_update_event_;
};

}
