#ifndef __INC_METIN_II_GAME_DUNGEON_H
#define __INC_METIN_II_GAME_DUNGEON_H

#include "event.h"
#include "sectree_manager.h"
#include "typedef.h"

#include <common/singleton.h>
#include <libthecore/log.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class CParty;

class CDungeon
{
	typedef std::unordered_map<CParty *, int> TPartyMap;
	typedef std::map<std::string, CHARACTER *> TUniqueMobMap;

	public:
	// <Factor> Non-persistent identifier type
	typedef uint32_t IdType;

	~CDungeon();

	// <Factor>
	IdType GetId() const { return m_id; }

	// DUNGEON_NOTICE
	void	Notice(const char* msg);
	// END_OF_DUNGEON_NOTICE

	void	JoinParty(CParty * pParty);
	void	QuitParty(CParty * pParty);

	void	Join(CHARACTER * ch);

	void	IncMember(CHARACTER * ch);
	void	DecMember(CHARACTER * ch);

	// DUNGEON_KILL_ALL_BUG_FIX
	void	Purge();
	void	KillAll();
	// END_OF_DUNGEON_KILL_ALL_BUG_FIX

	void	IncMonster() { m_iMonsterCount++; sys_log(0, "MonsterCount %d", m_iMonsterCount); }
	void	DecMonster() { m_iMonsterCount--; CheckEliminated(); }
	int	CountMonster() { return m_iMonsterCount; }	// number of monsters regenerated from data

	void	IncPartyMember(CParty * pParty, CHARACTER * ch);
	void	DecPartyMember(CParty * pParty, CHARACTER * ch);

	void	IncKillCount(CHARACTER * pkKiller, CHARACTER * pkVictim);
	int	GetKillMobCount();
	int	GetKillStoneCount();
	bool	IsUsePotion();
	bool	IsUseRevive();
	void	UsePotion(CHARACTER * ch);
	void	UseRevive(CHARACTER * ch);

	int32_t	GetMapIndex() { return m_lMapIndex; }

	void	Spawn(uint32_t vnum, const char* pos);
	CHARACTER *	SpawnMob(uint32_t vnum, int x, int y, int dir = 0);
	CHARACTER *	SpawnMob_ac_dir(uint32_t vnum, int x, int y, int dir = 0);
	CHARACTER *	SpawnGroup(uint32_t vnum, int32_t x, int32_t y, float radius, bool bAggressive=false, int count=1);

	void	SpawnNameMob(uint32_t vnum, int x, int y, const char* name);
	void	SpawnGotoMob(int32_t lFromX, int32_t lFromY, int32_t lToX, int32_t lToY);

	void	SpawnRegen(const char* filename, bool bOnce = true);
	void	AddRegen(REGEN * regen);
	void	ClearRegen();
	bool	IsValidRegen(REGEN * regen, size_t regen_id);

	void	SetUnique(const char* key, uint32_t vid);
	void	SpawnMoveUnique(const char* key, uint32_t vnum, const char* pos_from, const char* pos_to);
	void	SpawnMoveGroup(uint32_t vnum, const char* pos_from, const char* pos_to, int count=1);
	void	SpawnUnique(const char* key, uint32_t vnum, const char* pos);
	void	SpawnStoneDoor(const char* key, const char* pos);
	void	SpawnWoodenDoor(const char* key, const char* pos);
	void	KillUnique(const std::string& key);
	void	PurgeUnique(const std::string& key);
	bool	IsUniqueDead(const std::string& key);
	float	GetUniqueHpPerc(const std::string& key);
	uint32_t	GetUniqueVid(const std::string& key);

	void	DeadCharacter(CHARACTER * ch);

	void	UniqueSetMaxHP(const std::string& key, int iMaxHP);
	void	UniqueSetHP(const std::string& key, int iHP);
	void	UniqueSetDefGrade(const std::string& key, int iGrade);

	void	SendDestPositionToParty(CParty * pParty, int32_t x, int32_t y);

	void	CheckEliminated();

	void	JumpAll(int32_t lFromMapIndex, int x, int y);
	void	WarpAll(int32_t lFromMapIndex, int x, int y);
	void	JumpParty(CParty * pParty, int32_t lFromMapIndex, int x, int y);

	void	ExitAll();
	void	ExitAllToStartPosition();
	void	JumpToEliminateLocation();
	void	SetExitAllAtEliminate(int32_t time);
	void	SetWarpAtEliminate(int32_t time, int32_t lMapIndex, int x, int y, const char* regen_file);

	int	GetFlag(std::string name);
	void	SetFlag(std::string name, int value);
	void	SetWarpLocation (int32_t map_index, int x, int y);

	// an item group consists of item_vnum and item_count.
	typedef std::vector <std::pair <uint32_t, int> > ItemGroup;
	void	CreateItemGroup (std::string& group_name, ItemGroup& item_group);
	const ItemGroup* GetItemGroup (std::string& group_name);

	template <class Func> Func ForEachMember(Func f);

	bool IsAllPCNearTo( int x, int y, int dist );

	protected:
	CDungeon(IdType id, int32_t lOriginalMapIndex, int32_t lMapIndex);

	void	Initialize();

	private:
	IdType 		m_id; // <Factor>
	uint32_t		m_lMapIndex;

	CHARACTER_SET	    m_set_pkCharacter;
	std::map<std::string, int>  m_map_Flag;
	typedef std::map<std::string, ItemGroup> ItemGroupMap;
	ItemGroupMap m_map_ItemGroup;
	TPartyMap	m_map_pkParty;
	TAreaMap&	m_map_Area;
	TUniqueMobMap	m_map_UniqueMob;

	int		m_iMobKill;
	int		m_iStoneKill;
	bool		m_bUsePotion;
	bool		m_bUseRevive;

	int		m_iMonsterCount;

	bool		m_bExitAllAtEliminate;
	bool		m_bWarpAtEliminate;

	// position to warp to when all enemies are wiped out
	int		m_iWarpDelay;
	int32_t		m_lWarpMapIndex;
	int32_t		m_lWarpX;
	int32_t		m_lWarpY;
	std::string	m_stRegenFile;

	std::vector<REGEN *> m_regen;

	LPEVENT		deadEvent;
	// <Factor>
	LPEVENT exit_all_event_;
	LPEVENT jump_to_event_;
	size_t regen_id_;

	friend class CDungeonManager;
	friend EVENTFUNC(dungeon_dead_event);
	// <Factor>
	friend EVENTFUNC(dungeon_exit_all_event);
	friend EVENTFUNC(dungeon_jump_to_event);

	// temporary variable for party-based dungeon entry.
	// m_map_pkParty is poorly managed and judged unusable, so
	// a variable managing a single party was created temporarily.
	
	CParty * m_pParty;
	public :
	void SetPartyNull();
};

class CDungeonManager : public singleton<CDungeonManager>
{
	typedef std::map<CDungeon::IdType, CDungeon *> TDungeonMap;
	typedef std::map<int32_t, CDungeon *> TMapDungeon;

	public:
	CDungeonManager();
	~CDungeonManager() override;

	CDungeon *	Create(int32_t lOriginalMapIndex);
	void		Destroy(CDungeon::IdType dungeon_id);
	CDungeon *	Find(CDungeon::IdType dungeon_id);
	CDungeon *	FindByMapIndex(int32_t lMapIndex);

	private:
	TDungeonMap	m_map_pkDungeon;
	TMapDungeon m_map_pkMapDungeon;

	// <Factor> Introduced unsigned 32-bit dungeon identifier
	CDungeon::IdType next_id_;
};

#endif
