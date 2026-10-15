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

	virtual bool	update(uint32_t delta_time);

protected:
	virtual bool	update_follow_ai();


private:
	bool follow(float min_distance = 50.f);

public:
	character *		get_character()	const					{ return character_; }
	character *		owner()	const						{ return owner_; }
	uint32_t			get_vid() const							{ return vid_; }

	bool			has_option(pet_options option) const		{ return options_ & option; }

	void			set_name(const char* name);

	uint32_t			summon(const char* pet_name, item * summon_item, bool spawn_far = false);
	void			unsummon();

	bool			is_summoned() const			{ return nullptr != character_; }
	void			set_summon_item (item * item_value);
	uint32_t			summon_item_vid () { return summon_item_vid_; }
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

	pet_actor*	by_vnum(uint32_t vnum) const;

	bool		update(uint32_t delta_time);
	void		destroy();

	size_t		count_summoned() const;

public:

	pet_actor*	summon(uint32_t mob_vnum, item * summon_item, const char* pet_name, bool spawn_far, uint32_t options = pet_actor::EPetOption_Followable | pet_actor::EPetOption_Summonable);

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
