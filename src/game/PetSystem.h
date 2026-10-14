#ifndef	__HEADER_PET_SYSTEM__
#define	__HEADER_PET_SYSTEM__


#include "event.h"
#include "typedef.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

class CHARACTER;

class CPetActor //: public CHARACTER
{
public:
	enum EPetOptions
	{
		EPetOption_Followable		= 1 << 0,
		EPetOption_Summonable		= 1 << 2,
		EPetOption_Combatable		= 1 << 3,		
	};


protected:
	friend class CPetSystem;

	CPetActor(CHARACTER * owner, uint32_t vnum, uint32_t options = EPetOption_Followable | EPetOption_Summonable);
//	CPetActor(CHARACTER * owner, uint32_t vnum, const SPetAbility& petAbility, uint32_t options = EPetOption_Followable | EPetOption_Summonable);

	virtual ~CPetActor();

	virtual bool	Update(uint32_t deltaTime);

protected:
	virtual bool	_UpdateFollowAI();				///< AI that follows the owner			///< AI that idles alone near the owner

	/// @TODO
	//virtual bool	_UpdateCombatAI();

private:
	bool Follow(float fMinDistance = 50.f);

public:
	CHARACTER *		GetCharacter()	const					{ return m_pkChar; }
	CHARACTER *		GetOwner()	const						{ return m_pkOwner; }
	uint32_t			GetVID() const							{ return m_dwVID; }

	bool			HasOption(EPetOptions option) const		{ return m_dwOptions & option; }

	void			SetName(const char* petName);


	uint32_t			Summon(const char* petName, CItem * pSummonItem, bool bSpawnFar = false);
	void			Unsummon();

	bool			IsSummoned() const			{ return nullptr != m_pkChar; }
	void			SetSummonItem (CItem * pItem);
	uint32_t			GetSummonItemVID () { return m_dwSummonItemVID; }
	// Functions to give and remove the buff.
	// This is a bit bizarre: because the server is dumb, 
	// changing POINT_MOV_SPEED, POINT_ATT_SPEED, POINT_CAST_SPEED with PointChange() alone is useless,
	// because if ComputePoints() is called somewhere after PointChange(), it all gets reset, 
	// and funnier still, if ComputePoints() isn't called, the client's POINT doesn't change at all.
	// So giving the buff is done by calling petsystem->RefreshBuff() inside ComputePoints(),
	// and removing the buff is done by calling ClearBuff() and then ComputePoints.
	void			GiveBuff();
	void			ClearBuff();

private:
	uint32_t			m_dwVnum;
	uint32_t			m_dwVID;
	uint32_t			m_dwOptions;
	uint32_t			m_dwSummonItemVID;
	uint32_t			m_dwSummonItemVnum;

	short			m_originalMoveSpeed;

	std::string		m_name;

	CHARACTER *		m_pkChar;					// Instance of pet(CHARACTER)
	CHARACTER *		m_pkOwner;

//	SPetAbility		m_petAbility;				// stats
};

/**
*/
class CPetSystem
{
public:
	typedef	std::unordered_map<uint32_t,	CPetActor*>		TPetActorMap;		/// <VNUM, PetActor> map. (Would one character ever have several pets with the same vnum..??)

public:
	CPetSystem(CHARACTER * owner);
	virtual ~CPetSystem();

	CPetActor*	GetByVnum(uint32_t vnum) const;

	bool		Update(uint32_t deltaTime);
	void		Destroy();

	size_t		CountSummoned() const;			///< Number of currently summoned pets (with a materialized character)

public:

	CPetActor*	Summon(uint32_t mobVnum, CItem * pSummonItem, const char* petName, bool bSpawnFar, uint32_t options = CPetActor::EPetOption_Followable | CPetActor::EPetOption_Summonable);

	void		Unsummon(uint32_t mobVnum, bool bDeleteFromList = false);
	void		Unsummon(CPetActor* petActor, bool bDeleteFromList = false);

	// TODO: implement when the real pet system goes in. (e.g. when adding info on pets owned by a character...)

	void		DeletePet(CPetActor* petActor);
	void		RefreshBuff();

private:
	TPetActorMap	m_petActorMap;
	CHARACTER *		m_pkOwner;					///< Owner of the pet system
	uint32_t			m_dwUpdatePeriod;			///< Update interval (ms)
	uint32_t			m_dwLastUpdateTime;
	LPEVENT			m_pkPetSystemUpdateEvent;
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



#endif	//__HEADER_PET_SYSTEM__