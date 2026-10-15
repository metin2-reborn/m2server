#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace metin2::game::inline characters {

class character;

class pet_actor //: public CHARACTER
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
//	CPetActor(CHARACTER * owner, uint32_t vnum, const SPetAbility& petAbility, uint32_t options = EPetOption_Followable | EPetOption_Summonable);

	virtual ~pet_actor();

	virtual bool	update(uint32_t delta_time);

protected:
	virtual bool	update_follow_ai();				///< AI that follows the owner			///< AI that idles alone near the owner

	/// @TODO
	//virtual bool	_UpdateCombatAI();

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
	// Functions to give and remove the buff.
	// This is a bit bizarre: because the server is dumb, 
	// changing POINT_MOV_SPEED, POINT_ATT_SPEED, POINT_CAST_SPEED with PointChange() alone is useless,
	// because if ComputePoints() is called somewhere after PointChange(), it all gets reset, 
	// and funnier still, if ComputePoints() isn't called, the client's POINT doesn't change at all.
	// So giving the buff is done by calling petsystem->RefreshBuff() inside ComputePoints(),
	// and removing the buff is done by calling ClearBuff() and then ComputePoints.
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

	character *		character_;					// Instance of pet(CHARACTER)
	character *		owner_;

//	SPetAbility		m_petAbility;				// stats
};

/**
*/
class pet_system
{
public:
	typedef	std::unordered_map<uint32_t,	pet_actor*>		pet_actor_map;		/// <VNUM, PetActor> map. (Would one character ever have several pets with the same vnum..??)

public:
	pet_system(character * owner);
	virtual ~pet_system();

	pet_actor*	by_vnum(uint32_t vnum) const;

	bool		update(uint32_t delta_time);
	void		destroy();

	size_t		count_summoned() const;			///< Number of currently summoned pets (with a materialized character)

public:

	pet_actor*	summon(uint32_t mob_vnum, item * summon_item, const char* pet_name, bool spawn_far, uint32_t options = pet_actor::EPetOption_Followable | pet_actor::EPetOption_Summonable);

	void		unsummon(uint32_t vnum, bool delete_from_list = false);
	void		unsummon(pet_actor* pet_actor, bool delete_from_list = false);

	// TODO: implement when the real pet system goes in. (e.g. when adding info on pets owned by a character...)

	void		delete_pet(pet_actor* pet_actor_value);
	void		refresh_buff();

private:
	pet_actor_map	pet_actor_map_;
	character *		owner_;					///< Owner of the pet system
	uint32_t			update_period_;			///< Update interval (ms)
	uint32_t			last_update_time_;
	event_ptr			pet_system_update_event_;
};

/**
// Summon Pet
CPetSystem* petSystem = mainChar->GetPetSystem();
CPetActor* petActor = petSystem->Summon(~~~);

uint32_t petVID = petActor->GetVID();
if (0 == petActor)
{
	ERROR_LOG(...)
};

// Unsummon Pet
petSystem->Unsummon(petVID);

// Mount Pet
petActor->Mount()..

CPetActor::Update(...)
{
	// AI : Follow, actions, etc...
}

*/
}
