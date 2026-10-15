#include <metin2/game/characters/pet_system.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/world/vector.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game::inline characters {


EVENTINFO(petsystem_event_info)
{
	game::characters::pet_system* pet_system;
};

// event that updates the PetSystem.
// Unlike existing characters, which CHRACTER_MANAGER updates through the FSM, the PetSystem
// is updated by _UpdateFollowAI when the owner's STATE is updated.
// But since the owner's state is updated by CHRACTER_MANAGER,
// a problem occurred when a pet was unsummoned while updating the petsystem.
// (when updated from CHRACTER_MANAGER, character destroy is pending, so CPetSystem ends up holding a dangling pointer.)
// so an event that updates only the PetSystem is raised.
EVENTFUNC(petsystem_update_event)
{
	petsystem_event_info* info = dynamic_cast<petsystem_event_info*>( event->info );
	if ( info == nullptr )
	{
		metin2::log::error( "check_speedhack_event> <Factor> Null pointer" );
		return 0;
	}

	pet_system*	pPetSystem = info->pet_system;

	if (nullptr == pPetSystem)
		return 0;

	
	pPetSystem->update(0);
	// refresh every 0.25 seconds.
	return passes_per_sec(1) / 4;
}

/// NOTE: limit on how many pets one character can have... if it should differ per character, make it a variable... hmm..
/// the number you can own and the number you can summon at once may differ, but there's no design for that so ignore it for now

///////////////////////////////////////////////////////////////////////////////////////
//  CPetActor
///////////////////////////////////////////////////////////////////////////////////////

pet_actor::pet_actor(character * owner, uint32_t vnum, uint32_t options)
{
	vnum_ = vnum;
	vid_ = 0;
	options_ = options;

	character_ = nullptr;
	owner_ = owner;

	original_move_speed_ = 0;
	
	summon_item_vid_ = 0;
	summon_item_vnum_ = 0;
}

pet_actor::~pet_actor()
{
	this->unsummon();

	owner_ = nullptr;
}

void pet_actor::set_name(const char* name)
{
	std::string petName = owner_->name();

	if (nullptr != owner_ && 
		nullptr == name && 
		nullptr != owner_->name())
	{
		petName += "'s Pet";
	}
	else
		petName += name;

	if (true == is_summoned())
		character_->set_name(petName);

	name_ = petName;
}

void pet_actor::unsummon()
{
	if (true == this->is_summoned())
	{
		// remove buff
		this->clear_buff();
		this->set_summon_item(nullptr);
		if (nullptr != owner_)
			owner_->compute_points();

		if (nullptr != character_)
			character_manager::instance().destroy_character(character_);

		character_ = nullptr;
		vid_ = 0;
	}
}

uint32_t pet_actor::summon(const char* pet_name, item * summon_item, bool spawn_far)
{
	int32_t x = owner_->x();
	int32_t y = owner_->y();
	int32_t z = owner_->z();

	if (true == spawn_far)
	{
		x += (number(0, 1) * 2 - 1) * number(2000, 2500);
		y += (number(0, 1) * 2 - 1) * number(2000, 2500);
	}
	else
	{
		x += number(-100, 100);
		y += number(-100, 100);
	}

	if (nullptr != character_)
	{
		character_->show (owner_->map_index(), x, y);
		vid_ = character_->get_vid();

		return vid_;
	}
	
	character_ = character_manager::instance().spawn_mob(
				vnum_, 
				owner_->map_index(), 
				x, y, z,
				false, (int)(owner_->rotation()+180), false);

	if (nullptr == character_)
	{
		metin2::log::error("[CPetSystem::Summon] Failed to summon the pet. (vnum: {:d})", vnum_);
		return 0;
	}

	character_->set_pet();

	//set the pet's empire to the owner's empire.
	character_->set_empire(owner_->empire());

	vid_ = character_->get_vid();

	this->set_name(pet_name);

	// calling ComputePoints after SetSummonItem(pSummonItem) applies the buff.
	this->set_summon_item(summon_item);
	owner_->compute_points();
	character_->show(owner_->map_index(), x, y, z);

	return vid_;
}

// just copy-pasted the StateHorse function from char_state.cpp -_-;
bool pet_actor::update_follow_ai()
{
	if (nullptr == character_->mob_data)
	{
		return false;
	}
	
	// NOTE: we need the character's (pet's) original move speed; it could be read directly from m_pkChar->m_pkMobData->m_table.sMovingSpeed, but
	// m_pkChar->m_pkMobData is often invalid. Due to time constraints the cause will be investigated later; for now m_pkChar->m_pkMobData is not used at all.
	// the reason for checking every time here is that sometimes the correct value isn't obtained at initial initialization.. -_-;; TTTTTTTTT
	if (0 == original_move_speed_)
	{
		const mob* mobData = mob_manager::instance().get(vnum_);

		if (nullptr != mobData)
			original_move_speed_ = mobData->table.s_moving_speed;
	}
	float	START_FOLLOW_DISTANCE = 300.0f;		// start following when farther than this distance
	float	START_RUN_DISTANCE = 900.0f;		// run to follow when farther than this distance.

	float	RESPAWN_DISTANCE = 4500.f;			// summon next to the owner when farther than this distance.
	int		APPROACH = 200;						// approach distance

	bool bRun = false;							// should it run?

	uint32_t currentTime = get_dword_time();

	int32_t ownerX = owner_->x();		int32_t ownerY = owner_->y();
	int32_t charX = character_->x();			int32_t charY = character_->y();

	float fDist = distance_approx(charX - ownerX, charY - ownerY);

	if (fDist >= RESPAWN_DISTANCE)
	{
		float fOwnerRot = owner_->rotation() * 3.141592f / 180.f;
		float fx = -APPROACH * cos(fOwnerRot);
		float fy = -APPROACH * sin(fOwnerRot);
		if (character_->show(owner_->map_index(), ownerX + fx, ownerY + fy))
		{
			return true;
		}
	}
	
	
	if (fDist >= START_FOLLOW_DISTANCE)
	{
		if( fDist >= START_RUN_DISTANCE)
		{
			bRun = true;
		}

		character_->set_now_walking(!bRun);		// NOTE: from the function name I thought it stopped, but SetNowWalking(false) means running.. -_-;
		
		follow(APPROACH);

		character_->set_last_attacked(currentTime);
	}
	// following, but stop once within a certain distance of the owner
	else 
		character_->send_move_packet(FUNC_WAIT, 0, 0, 0, 0);

	return true;
}

bool pet_actor::update(uint32_t delta_time)
{
	bool bResult = true;

	// remove the pet if its owner died or the summoned pet's state is abnormal. (NOTE: occasionally a summoned pet falls into DEAD state for various reasons -_-;)
	// remove the pet if the item that summoned it is gone or no longer owned by me.
	if (owner_->is_dead() || (is_summoned() && character_->is_dead()) 
		|| nullptr == item_manager::instance().find_by_vid(this->summon_item_vid())
		|| item_manager::instance().find_by_vid(this->summon_item_vid())->owner() != this->owner()
		)
	{
		this->unsummon();
		return true;
	}

	if (this->is_summoned() && has_option(EPetOption_Followable))
		bResult = bResult && this->update_follow_ai();

	return bResult;
}

//NOTE : careful!!! if MinDistance is large, it won't follow while the displacement is within that amount,
bool pet_actor::follow(float min_distance)
{
	// must face the position it is moving to.
	if( !owner_ || !character_) 
		return false;

	float fOwnerX = owner_->x();
	float fOwnerY = owner_->y();

	float fPetX = character_->x();
	float fPetY = character_->y();

	float fDist = distance_sqrt(fOwnerX - fPetX, fOwnerY - fPetY);
	if (fDist <= min_distance)
		return false;

	character_->set_rotation_to_xy(fOwnerX, fOwnerY);

	float fx, fy;

	float fDistToGo = fDist - min_distance;
	get_delta_by_degree(character_->rotation(), fDistToGo, &fx, &fy);
	
	if (!character_->go_to((int)(fPetX+fx+0.5f), (int)(fPetY+fy+0.5f)) )
		return false;

	character_->send_move_packet(FUNC_WAIT, 0, 0, 0, 0, 0);

	return true;
}

void pet_actor::set_summon_item (item * item_value)
{
	if (nullptr == item_value)
	{
		summon_item_vid_ = 0;
		summon_item_vnum_ = 0;
		return;
	}

	summon_item_vid_ = item_value->get_vid();
	summon_item_vnum_ = item_value->vnum();
}

void pet_actor::give_buff()
{
	// Pahwang pet buff only occurs in dungeons.
	if (34004 == vnum_ || 34009 == vnum_)
	{
		if (nullptr == owner_->get_dungeon())
		{
			return;
		}
	}
	item * item = item_manager::instance().find_by_vid(summon_item_vid_);
	if (nullptr != item)
		item->modify_points(true);
	return ;
}

void pet_actor::clear_buff()
{
	if (nullptr == owner_)
		return ;
	item_table* item_proto = item_manager::instance().table(summon_item_vnum_);
	if (nullptr == item_proto)
		return;
	for (int i = 0; i < ITEM_APPLY_MAX_NUM; i++)
	{
		if (item_proto->applies[i].type == APPLY_NONE)
			continue;
		owner_->apply_point(item_proto->applies[i].type, -item_proto->applies[i].value);
	}

	return ;
}

///////////////////////////////////////////////////////////////////////////////////////
//  CPetSystem
///////////////////////////////////////////////////////////////////////////////////////

pet_system::pet_system(character * owner)
{

	owner_ = owner;
	update_period_ = 400;

	last_update_time_ = 0;
}

pet_system::~pet_system()
{
	destroy();
}

void pet_system::destroy()
{
	for (pet_actor_map::iterator iter = pet_actor_map_.begin(); iter != pet_actor_map_.end(); ++iter)
	{
		pet_actor* petActor = iter->second;

		if (nullptr != petActor)
		{
			delete petActor;
		}
	}
	event_cancel(&pet_system_update_event_);
	pet_actor_map_.clear();
}

/// pet system update. handles AI etc. of registered pets.
bool pet_system::update(uint32_t delta_time)
{
	bool bResult = true;

	uint32_t currentTime = get_dword_time();

	// I thought the value (called Pulse) passed when CHARACTER_MANAGER updates characters was the time delta since the previous frame, but
	// it's a completely different value -_-; so the deltaTime input here is meaningless TT	
	
	if (update_period_ > currentTime - last_update_time_)
		return true;
	
	std::vector <pet_actor*> v_garbageActor;

	for (pet_actor_map::iterator iter = pet_actor_map_.begin(); iter != pet_actor_map_.end(); ++iter)
	{
		pet_actor* petActor = iter->second;

		if (nullptr != petActor && petActor->is_summoned())
		{
			character * pPet = petActor->get_character();
			
			if (nullptr == character_manager::instance().find(pPet->get_vid()))
			{
				v_garbageActor.push_back(petActor);
			}
			else
			{
				bResult = bResult && petActor->update(delta_time);
			}
		}
	}
	for (std::vector<pet_actor*>::iterator it = v_garbageActor.begin(); it != v_garbageActor.end(); it++)
		delete_pet(*it);

	last_update_time_ = currentTime;

	return bResult;
}

/// remove the pet from the managed list
void pet_system::delete_pet(pet_actor* pet_actor_value)
{
	for (pet_actor_map::iterator iter = pet_actor_map_.begin(); iter != pet_actor_map_.end(); ++iter)
	{
		if (iter->second == pet_actor_value)
		{
			delete pet_actor_value;
			pet_actor_map_.erase(iter);

			return;
		}
	}

	metin2::log::error("[CPetSystem::DeletePet] Can't find petActor({}) on my list(size: {:d}) ", static_cast<const void *>(static_cast<void*>(pet_actor_value)), pet_actor_map_.size());
}

void pet_system::unsummon(uint32_t vnum, bool delete_from_list)
{
	pet_actor* actor = this->by_vnum(vnum);

	if (nullptr == actor)
	{
		metin2::log::error("[CPetSystem::GetByVnum({:d})] Null Pointer (petActor)", vnum);
		return;
	}
	actor->unsummon();

	if (true == delete_from_list)
		this->delete_pet(actor);

	bool bActive = false;
	for (pet_actor_map::iterator it = pet_actor_map_.begin(); it != pet_actor_map_.end(); it++)
	{
		bActive |= it->second->is_summoned();
	}
	if (false == bActive)
	{
		event_cancel(&pet_system_update_event_);
		pet_system_update_event_ = nullptr;
	}
}


pet_actor* pet_system::summon(uint32_t mob_vnum, item * summon_item, const char* pet_name, bool spawn_far, uint32_t options)
{
	pet_actor* petActor = this->by_vnum(mob_vnum);

	// if not a registered pet, create it and register it in the managed list.
	if (nullptr == petActor)
	{
		petActor = new pet_actor(owner_, mob_vnum, options);
		pet_actor_map_.insert(std::make_pair(mob_vnum, petActor));
	}

	petActor->summon(pet_name, summon_item, spawn_far);

	if (nullptr == pet_system_update_event_)
	{
		petsystem_event_info* info = alloc_event_info<petsystem_event_info>();

		info->pet_system = this;

		pet_system_update_event_ = event_create(petsystem_update_event, info, passes_per_sec(1) / 4);	// 0.25 seconds	
	}

	return petActor;
}


/// returns the actor with the given mob VNUM among registered pets.
pet_actor* pet_system::by_vnum(uint32_t vnum) const
{
	pet_actor* petActor = nullptr;

	pet_actor_map::const_iterator iter = pet_actor_map_.find(vnum);

	if (pet_actor_map_.end() != iter)
		petActor = iter->second;

	return petActor;
}

size_t pet_system::count_summoned() const
{
	size_t count = 0;

	for (pet_actor_map::const_iterator iter = pet_actor_map_.begin(); iter != pet_actor_map_.end(); ++iter)
	{
		pet_actor* petActor = iter->second;

		if (nullptr != petActor)
		{
			if (petActor->is_summoned())
				++count;
		}
	}

	return count;
}

void pet_system::refresh_buff()
{
	for (pet_actor_map::const_iterator iter = pet_actor_map_.begin(); iter != pet_actor_map_.end(); ++iter)
	{
		pet_actor* petActor = iter->second;

		if (nullptr != petActor)
		{
			if (petActor->is_summoned())
			{
				petActor->give_buff();
			}
		}
	}
}
}
