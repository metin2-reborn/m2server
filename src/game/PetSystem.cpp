#include "PetSystem.h"

#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "event.h"
#include "item.h"
#include "item_manager.h"
#include "mob_manager.h"
#include "packet.h"
#include "typedef.h"
#include "utils.h"
#include "vector.h"

#include <common/length.h>
#include <common/tables.h>
#include <libthecore/log.h>
#include <libthecore/utils.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>


EVENTINFO(petsystem_event_info)
{
	CPetSystem* pPetSystem;
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
		sys_err( "check_speedhack_event> <Factor> Null pointer" );
		return 0;
	}

	CPetSystem*	pPetSystem = info->pPetSystem;

	if (nullptr == pPetSystem)
		return 0;

	
	pPetSystem->Update(0);
	// refresh every 0.25 seconds.
	return PASSES_PER_SEC(1) / 4;
}

/// NOTE: limit on how many pets one character can have... if it should differ per character, make it a variable... hmm..
/// the number you can own and the number you can summon at once may differ, but there's no design for that so ignore it for now

///////////////////////////////////////////////////////////////////////////////////////
//  CPetActor
///////////////////////////////////////////////////////////////////////////////////////

CPetActor::CPetActor(CHARACTER * owner, uint32_t vnum, uint32_t options)
{
	m_dwVnum = vnum;
	m_dwVID = 0;
	m_dwOptions = options;

	m_pkChar = nullptr;
	m_pkOwner = owner;

	m_originalMoveSpeed = 0;
	
	m_dwSummonItemVID = 0;
	m_dwSummonItemVnum = 0;
}

CPetActor::~CPetActor()
{
	this->Unsummon();

	m_pkOwner = nullptr;
}

void CPetActor::SetName(const char* name)
{
	std::string petName = m_pkOwner->GetName();

	if (nullptr != m_pkOwner && 
		nullptr == name && 
		nullptr != m_pkOwner->GetName())
	{
		petName += "'s Pet";
	}
	else
		petName += name;

	if (true == IsSummoned())
		m_pkChar->SetName(petName);

	m_name = petName;
}

void CPetActor::Unsummon()
{
	if (true == this->IsSummoned())
	{
		// remove buff
		this->ClearBuff();
		this->SetSummonItem(nullptr);
		if (nullptr != m_pkOwner)
			m_pkOwner->ComputePoints();

		if (nullptr != m_pkChar)
			CHARACTER_MANAGER::instance().DestroyCharacter(m_pkChar);

		m_pkChar = nullptr;
		m_dwVID = 0;
	}
}

uint32_t CPetActor::Summon(const char* petName, CItem * pSummonItem, bool bSpawnFar)
{
	int32_t x = m_pkOwner->GetX();
	int32_t y = m_pkOwner->GetY();
	int32_t z = m_pkOwner->GetZ();

	if (true == bSpawnFar)
	{
		x += (number(0, 1) * 2 - 1) * number(2000, 2500);
		y += (number(0, 1) * 2 - 1) * number(2000, 2500);
	}
	else
	{
		x += number(-100, 100);
		y += number(-100, 100);
	}

	if (nullptr != m_pkChar)
	{
		m_pkChar->Show (m_pkOwner->GetMapIndex(), x, y);
		m_dwVID = m_pkChar->GetVID();

		return m_dwVID;
	}
	
	m_pkChar = CHARACTER_MANAGER::instance().SpawnMob(
				m_dwVnum, 
				m_pkOwner->GetMapIndex(), 
				x, y, z,
				false, (int)(m_pkOwner->GetRotation()+180), false);

	if (nullptr == m_pkChar)
	{
		sys_err("[CPetSystem::Summon] Failed to summon the pet. (vnum: %d)", m_dwVnum);
		return 0;
	}

	m_pkChar->SetPet();

	//set the pet's empire to the owner's empire.
	m_pkChar->SetEmpire(m_pkOwner->GetEmpire());

	m_dwVID = m_pkChar->GetVID();

	this->SetName(petName);

	// calling ComputePoints after SetSummonItem(pSummonItem) applies the buff.
	this->SetSummonItem(pSummonItem);
	m_pkOwner->ComputePoints();
	m_pkChar->Show(m_pkOwner->GetMapIndex(), x, y, z);

	return m_dwVID;
}

// just copy-pasted the StateHorse function from char_state.cpp -_-;
bool CPetActor::_UpdateFollowAI()
{
	if (nullptr == m_pkChar->m_pkMobData)
	{
		return false;
	}
	
	// NOTE: we need the character's (pet's) original move speed; it could be read directly from m_pkChar->m_pkMobData->m_table.sMovingSpeed, but
	// m_pkChar->m_pkMobData is often invalid. Due to time constraints the cause will be investigated later; for now m_pkChar->m_pkMobData is not used at all.
	// the reason for checking every time here is that sometimes the correct value isn't obtained at initial initialization.. -_-;; TTTTTTTTT
	if (0 == m_originalMoveSpeed)
	{
		const CMob* mobData = CMobManager::instance().Get(m_dwVnum);

		if (nullptr != mobData)
			m_originalMoveSpeed = mobData->m_table.sMovingSpeed;
	}
	float	START_FOLLOW_DISTANCE = 300.0f;		// start following when farther than this distance
	float	START_RUN_DISTANCE = 900.0f;		// run to follow when farther than this distance.

	float	RESPAWN_DISTANCE = 4500.f;			// summon next to the owner when farther than this distance.
	int		APPROACH = 200;						// approach distance

	bool bRun = false;							// should it run?

	uint32_t currentTime = get_dword_time();

	int32_t ownerX = m_pkOwner->GetX();		int32_t ownerY = m_pkOwner->GetY();
	int32_t charX = m_pkChar->GetX();			int32_t charY = m_pkChar->GetY();

	float fDist = DISTANCE_APPROX(charX - ownerX, charY - ownerY);

	if (fDist >= RESPAWN_DISTANCE)
	{
		float fOwnerRot = m_pkOwner->GetRotation() * 3.141592f / 180.f;
		float fx = -APPROACH * cos(fOwnerRot);
		float fy = -APPROACH * sin(fOwnerRot);
		if (m_pkChar->Show(m_pkOwner->GetMapIndex(), ownerX + fx, ownerY + fy))
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

		m_pkChar->SetNowWalking(!bRun);		// NOTE: from the function name I thought it stopped, but SetNowWalking(false) means running.. -_-;
		
		Follow(APPROACH);

		m_pkChar->SetLastAttacked(currentTime);
	}
	// following, but stop once within a certain distance of the owner
	else 
		m_pkChar->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);

	return true;
}

bool CPetActor::Update(uint32_t deltaTime)
{
	bool bResult = true;

	// remove the pet if its owner died or the summoned pet's state is abnormal. (NOTE: occasionally a summoned pet falls into DEAD state for various reasons -_-;)
	// remove the pet if the item that summoned it is gone or no longer owned by me.
	if (m_pkOwner->IsDead() || (IsSummoned() && m_pkChar->IsDead()) 
		|| nullptr == ITEM_MANAGER::instance().FindByVID(this->GetSummonItemVID())
		|| ITEM_MANAGER::instance().FindByVID(this->GetSummonItemVID())->GetOwner() != this->GetOwner()
		)
	{
		this->Unsummon();
		return true;
	}

	if (this->IsSummoned() && HasOption(EPetOption_Followable))
		bResult = bResult && this->_UpdateFollowAI();

	return bResult;
}

//NOTE : careful!!! if MinDistance is large, it won't follow while the displacement is within that amount,
bool CPetActor::Follow(float fMinDistance)
{
	// must face the position it is moving to.
	if( !m_pkOwner || !m_pkChar) 
		return false;

	float fOwnerX = m_pkOwner->GetX();
	float fOwnerY = m_pkOwner->GetY();

	float fPetX = m_pkChar->GetX();
	float fPetY = m_pkChar->GetY();

	float fDist = DISTANCE_SQRT(fOwnerX - fPetX, fOwnerY - fPetY);
	if (fDist <= fMinDistance)
		return false;

	m_pkChar->SetRotationToXY(fOwnerX, fOwnerY);

	float fx, fy;

	float fDistToGo = fDist - fMinDistance;
	GetDeltaByDegree(m_pkChar->GetRotation(), fDistToGo, &fx, &fy);
	
	if (!m_pkChar->Goto((int)(fPetX+fx+0.5f), (int)(fPetY+fy+0.5f)) )
		return false;

	m_pkChar->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0, 0);

	return true;
}

void CPetActor::SetSummonItem (CItem * pItem)
{
	if (nullptr == pItem)
	{
		m_dwSummonItemVID = 0;
		m_dwSummonItemVnum = 0;
		return;
	}

	m_dwSummonItemVID = pItem->GetVID();
	m_dwSummonItemVnum = pItem->GetVnum();
}

void CPetActor::GiveBuff()
{
	// Pahwang pet buff only occurs in dungeons.
	if (34004 == m_dwVnum || 34009 == m_dwVnum)
	{
		if (nullptr == m_pkOwner->GetDungeon())
		{
			return;
		}
	}
	CItem * item = ITEM_MANAGER::instance().FindByVID(m_dwSummonItemVID);
	if (nullptr != item)
		item->ModifyPoints(true);
	return ;
}

void CPetActor::ClearBuff()
{
	if (nullptr == m_pkOwner)
		return ;
	TItemTable* item_proto = ITEM_MANAGER::instance().GetTable(m_dwSummonItemVnum);
	if (nullptr == item_proto)
		return;
	for (int i = 0; i < ITEM_APPLY_MAX_NUM; i++)
	{
		if (item_proto->aApplies[i].bType == APPLY_NONE)
			continue;
		m_pkOwner->ApplyPoint(item_proto->aApplies[i].bType, -item_proto->aApplies[i].lValue);
	}

	return ;
}

///////////////////////////////////////////////////////////////////////////////////////
//  CPetSystem
///////////////////////////////////////////////////////////////////////////////////////

CPetSystem::CPetSystem(CHARACTER * owner)
{

	m_pkOwner = owner;
	m_dwUpdatePeriod = 400;

	m_dwLastUpdateTime = 0;
}

CPetSystem::~CPetSystem()
{
	Destroy();
}

void CPetSystem::Destroy()
{
	for (TPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CPetActor* petActor = iter->second;

		if (nullptr != petActor)
		{
			delete petActor;
		}
	}
	event_cancel(&m_pkPetSystemUpdateEvent);
	m_petActorMap.clear();
}

/// pet system update. handles AI etc. of registered pets.
bool CPetSystem::Update(uint32_t deltaTime)
{
	bool bResult = true;

	uint32_t currentTime = get_dword_time();

	// I thought the value (called Pulse) passed when CHARACTER_MANAGER updates characters was the time delta since the previous frame, but
	// it's a completely different value -_-; so the deltaTime input here is meaningless TT	
	
	if (m_dwUpdatePeriod > currentTime - m_dwLastUpdateTime)
		return true;
	
	std::vector <CPetActor*> v_garbageActor;

	for (TPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CPetActor* petActor = iter->second;

		if (nullptr != petActor && petActor->IsSummoned())
		{
			CHARACTER * pPet = petActor->GetCharacter();
			
			if (nullptr == CHARACTER_MANAGER::instance().Find(pPet->GetVID()))
			{
				v_garbageActor.push_back(petActor);
			}
			else
			{
				bResult = bResult && petActor->Update(deltaTime);
			}
		}
	}
	for (std::vector<CPetActor*>::iterator it = v_garbageActor.begin(); it != v_garbageActor.end(); it++)
		DeletePet(*it);

	m_dwLastUpdateTime = currentTime;

	return bResult;
}

/// remove the pet from the managed list
void CPetSystem::DeletePet(CPetActor* petActor)
{
	for (TPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		if (iter->second == petActor)
		{
			delete petActor;
			m_petActorMap.erase(iter);

			return;
		}
	}

	sys_err("[CPetSystem::DeletePet] Can't find petActor(%p) on my list(size: %zu) ", static_cast<void*>(petActor), m_petActorMap.size());
}

void CPetSystem::Unsummon(uint32_t vnum, bool bDeleteFromList)
{
	CPetActor* actor = this->GetByVnum(vnum);

	if (nullptr == actor)
	{
		sys_err("[CPetSystem::GetByVnum(%d)] Null Pointer (petActor)", vnum);
		return;
	}
	actor->Unsummon();

	if (true == bDeleteFromList)
		this->DeletePet(actor);

	bool bActive = false;
	for (TPetActorMap::iterator it = m_petActorMap.begin(); it != m_petActorMap.end(); it++)
	{
		bActive |= it->second->IsSummoned();
	}
	if (false == bActive)
	{
		event_cancel(&m_pkPetSystemUpdateEvent);
		m_pkPetSystemUpdateEvent = nullptr;
	}
}


CPetActor* CPetSystem::Summon(uint32_t mobVnum, CItem * pSummonItem, const char* petName, bool bSpawnFar, uint32_t options)
{
	CPetActor* petActor = this->GetByVnum(mobVnum);

	// if not a registered pet, create it and register it in the managed list.
	if (nullptr == petActor)
	{
		petActor = new CPetActor(m_pkOwner, mobVnum, options);
		m_petActorMap.insert(std::make_pair(mobVnum, petActor));
	}

	petActor->Summon(petName, pSummonItem, bSpawnFar);

	if (nullptr == m_pkPetSystemUpdateEvent)
	{
		petsystem_event_info* info = AllocEventInfo<petsystem_event_info>();

		info->pPetSystem = this;

		m_pkPetSystemUpdateEvent = event_create(petsystem_update_event, info, PASSES_PER_SEC(1) / 4);	// 0.25 seconds	
	}

	return petActor;
}


/// returns the actor with the given mob VNUM among registered pets.
CPetActor* CPetSystem::GetByVnum(uint32_t vnum) const
{
	CPetActor* petActor = nullptr;

	TPetActorMap::const_iterator iter = m_petActorMap.find(vnum);

	if (m_petActorMap.end() != iter)
		petActor = iter->second;

	return petActor;
}

size_t CPetSystem::CountSummoned() const
{
	size_t count = 0;

	for (TPetActorMap::const_iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CPetActor* petActor = iter->second;

		if (nullptr != petActor)
		{
			if (petActor->IsSummoned())
				++count;
		}
	}

	return count;
}

void CPetSystem::RefreshBuff()
{
	for (TPetActorMap::const_iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CPetActor* petActor = iter->second;

		if (nullptr != petActor)
		{
			if (petActor->IsSummoned())
			{
				petActor->GiveBuff();
			}
		}
	}
}