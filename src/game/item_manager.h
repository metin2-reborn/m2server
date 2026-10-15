#pragma once
#include "char.h"
#include "typedef.h"

#include <common/singleton.h>
#include <common/tables.h>
#include <libthecore/utils.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include <sys/types.h>

// attribute group defined in special_item_group.txt
// can be declared with type attr.
// this attribute group can be used by UNIQUE ITEMs belonging to groups defined as Special type in special_item_group.txt.
class CSpecialAttrGroup
{
public:
	CSpecialAttrGroup(uint32_t vnum)
		: m_dwVnum(vnum)
	{}
	struct CSpecialAttrInfo
	{
		CSpecialAttrInfo (uint32_t _apply_type, uint32_t _apply_value)
			: apply_type(_apply_type), apply_value(_apply_value)
		{}
		uint32_t apply_type;
		uint32_t apply_value;

	};
	uint32_t m_dwVnum;
	std::string	m_stEffectFileName;
	std::vector<CSpecialAttrInfo> m_vecAttrs;
};

class CSpecialItemGroup
{
	public:
		enum EGiveType
		{
			NONE,
			GOLD,
			EXP,
			MOB,
			SLOW,
			DRAIN_HP,
			POISON,
			MOB_GROUP,
		};

		// QUEST type is a group for which vnum.sig_use can be used in quest scripts.
		//		however, to join this group the ITEM's own TYPE must be QUEST.
		// SPECIAL type takes idx, item_vnum, attr_vnum. attr_vnum is the Vnum of CSpecialAttrGroup above.
		//		items in this group cannot be worn together.
		enum ESIGType { NORMAL, PCT, QUEST, SPECIAL };

		struct CSpecialItemInfo
		{
			uint32_t vnum;
			int count;
			int rare;

			CSpecialItemInfo(uint32_t _vnum, int _count, int _rare)
				: vnum(_vnum), count(_count), rare(_rare)
				{}
		};

		CSpecialItemGroup(uint32_t vnum, uint8_t type=0)
			: m_dwVnum(vnum), m_bType(type)
			{}

		void AddItem(uint32_t vnum, int count, int prob, int rare)
		{
			if (!prob)
				return;
			if (!m_vecProbs.empty())
				prob += m_vecProbs.back();
			m_vecProbs.push_back(prob);
			m_vecItems.push_back(CSpecialItemInfo(vnum, count, rare));
		}

		// Type Multi, i.e. when m_bType == PCT,
		// probabilities are not accumulated; each is computed independently to create items.
		// so multiple items may be created.
		// by rtsummit
		int GetMultiIndex(std::vector <int> &idx_vec) const
		{
			idx_vec.clear();
			if (m_bType == PCT)
			{
				int count = 0;
				if (number(1,100) <= m_vecProbs[0])
				{
					idx_vec.push_back(0);
					count++;
				}
				for (uint i = 1; i < m_vecProbs.size(); i++)
				{
					if (number(1,100) <= m_vecProbs[i] - m_vecProbs[i-1])
					{
						idx_vec.push_back(i);
						count++;
					}
				}
				return count;
			}
			else
			{
				idx_vec.push_back(GetOneIndex());
				return 1;
			}
		}

		int GetOneIndex() const
		{
			int n = number(1, m_vecProbs.back());
			auto it = lower_bound(m_vecProbs.begin(), m_vecProbs.end(), n);
			return std::distance(m_vecProbs.begin(), it);
		}

		int GetVnum(int idx) const
		{
			return m_vecItems[idx].vnum;
		}

		int GetCount(int idx) const
		{
			return m_vecItems[idx].count;
		}

		int GetRarePct(int idx) const
		{
			return m_vecItems[idx].rare;
		}

		bool Contains(uint32_t dwVnum) const
		{
			for (uint32_t i = 0; i < m_vecItems.size(); i++)
			{
				if (m_vecItems[i].vnum == dwVnum)
					return true;
			}
			return false;
		}
		
		// when the Group's Type is Special
		// returns the AttrVnum matching dwVnum.
		uint32_t GetAttrVnum(uint32_t dwVnum) const
		{
			if (CSpecialItemGroup::SPECIAL != m_bType)
				return 0;
			for (auto it = m_vecItems.begin(); it != m_vecItems.end(); it++)
			{
				if (it->vnum == dwVnum)
				{
					return it->count;
				}
			}
			return 0;
		}

		uint32_t m_dwVnum;
		uint8_t	m_bType;
		std::vector<int> m_vecProbs;
		std::vector<CSpecialItemInfo> m_vecItems; // vnum, count
};

class CMobItemGroup
{
	public:
		struct SMobItemGroupInfo
		{
			uint32_t dwItemVnum;
			int iCount;
			int iRarePct;

			SMobItemGroupInfo(uint32_t dwItemVnum, int iCount, int iRarePct)
				: dwItemVnum(dwItemVnum),
			iCount(iCount),
			iRarePct(iRarePct)
			{
			}
		};

		explicit CMobItemGroup(int iKillDrop) : m_iKillDrop(iKillDrop)
		{
		}

		int GetKillPerDrop() const
		{
			return m_iKillDrop;
		}

		void AddItem(uint32_t dwItemVnum, int iCount, int iPartPct, int iRarePct)
		{
			if (!m_vecProbs.empty())
				iPartPct += m_vecProbs.back();
			m_vecProbs.push_back(iPartPct);
			m_vecItems.push_back(SMobItemGroupInfo(dwItemVnum, iCount, iRarePct));
		}

		// MOB_DROP_ITEM_BUG_FIX
		bool IsEmpty() const
		{
			return m_vecProbs.empty();
		}

		int GetOneIndex() const
		{
			int n = number(1, m_vecProbs.back());
			auto it = lower_bound(m_vecProbs.begin(), m_vecProbs.end(), n);
			return std::distance(m_vecProbs.begin(), it);
		}
		// END_OF_MOB_DROP_ITEM_BUG_FIX

		const SMobItemGroupInfo& GetOne() const
		{
			return m_vecItems[GetOneIndex()];
		}

	private:
		int m_iKillDrop;
		std::vector<int> m_vecProbs;
		std::vector<SMobItemGroupInfo> m_vecItems;
};

class CDropItemGroup
{
	struct SDropItemGroupInfo
	{
		uint32_t	dwVnum;
		uint32_t	dwPct;
		int	iCount;

		SDropItemGroupInfo(uint32_t dwVnum, uint32_t dwPct, int iCount)
			: dwVnum(dwVnum), dwPct(dwPct), iCount(iCount)
			{}
	};

	public:
	CDropItemGroup() = default;

	const std::vector<SDropItemGroupInfo> & GetVector()
	{
		return m_vec_items;
	}

	void AddItem(uint32_t dwItemVnum, uint32_t dwPct, int iCount)
	{
		m_vec_items.push_back(SDropItemGroupInfo(dwItemVnum, dwPct, iCount));
	}

	private:
	std::vector<SDropItemGroupInfo> m_vec_items;
};

class CLevelItemGroup
{
	struct SLevelItemGroupInfo
	{
		uint32_t dwVNum;
		uint32_t dwPct;
		int iCount;

		SLevelItemGroupInfo(uint32_t dwVnum, uint32_t dwPct, int iCount)
			: dwVNum(dwVnum), dwPct(dwPct), iCount(iCount)
		{ }
	};

	private :
		uint32_t m_dwLevelLimit;
		std::vector<SLevelItemGroupInfo> m_vec_items;

	public :
		CLevelItemGroup(uint32_t dwLevelLimit)
			: m_dwLevelLimit(dwLevelLimit)
		{}

		uint32_t GetLevelLimit() { return m_dwLevelLimit; }

		void AddItem(uint32_t dwItemVnum, uint32_t dwPct, int iCount)
		{
			m_vec_items.push_back(SLevelItemGroupInfo(dwItemVnum, dwPct, iCount));
		}

		const std::vector<SLevelItemGroupInfo> & GetVector()
		{
			return m_vec_items;
		}
};

class CBuyerThiefGlovesItemGroup
{
	struct SThiefGroupInfo
	{
		uint32_t	dwVnum;
		uint32_t	dwPct;
		int	iCount;

		SThiefGroupInfo(uint32_t dwVnum, uint32_t dwPct, int iCount)
			: dwVnum(dwVnum), dwPct(dwPct), iCount(iCount)
			{}
	};

	public:
	CBuyerThiefGlovesItemGroup() = default;

	const std::vector<SThiefGroupInfo> & GetVector()
	{
		return m_vec_items;
	}

	void AddItem(uint32_t dwItemVnum, uint32_t dwPct, int iCount)
	{
		m_vec_items.push_back(SThiefGroupInfo(dwItemVnum, dwPct, iCount));
	}

	private:
	std::vector<SThiefGroupInfo> m_vec_items;
};

class ITEM;

class ITEM_MANAGER : public singleton<ITEM_MANAGER>
{
	public:
		ITEM_MANAGER();
		~ITEM_MANAGER() override;

		bool                    Initialize(TItemTable * table, int size);
		void			Destroy();
		void			Update();	// called every loop.
		void			GracefulShutdown();

		uint32_t			GetNewID();
		bool			SetMaxItemID(TItemIDRangeTable range); // set the maximum unique ID
		bool			SetMaxSpareItemID(TItemIDRangeTable range);

		// DelayedSave: if a routine does many things that require saving,
		// there are too many queries, so just mark "to be saved" and save
		// shortly after (e.g. 1 frame).
		void			DelayedSave(CItem * item);
		void			FlushDelayedSave(CItem * item); // if in the Delayed list, remove it and save. used on disconnect handling.
		void			SaveSingleItem(CItem * item);

		CItem *                  CreateItem(uint32_t vnum, uint32_t count = 1, uint32_t dwID = 0, bool bTryMagic = false, int iRarePct = -1, bool bSkipSave = false);
		void DestroyItem(CItem * item);
		void			RemoveItem(CItem * item, const char * c_pszReason=nullptr); // remove the item from the user

		CItem *			Find(uint32_t id);
		CItem *                  FindByVID(uint32_t vid);
		TItemTable *            GetTable(uint32_t vnum);
		bool			GetVnum(const char * c_pszName, uint32_t & r_dwVnum);
		bool			GetVnumByOriginalName(const char * c_pszName, uint32_t & r_dwVnum);

		bool			GetDropPct(CHARACTER * pkChr, CHARACTER * pkKiller, int& iDeltaPercent, int& iRandRange);
		bool			CreateDropItem(CHARACTER * pkChr, CHARACTER * pkKiller, std::vector<CItem *> & vec_item);

		bool			ReadCommonDropItemFile(const char * c_pszFileName);
		bool			ReadEtcDropItemFile(const char * c_pszFileName);
		bool			ReadDropItemGroup(const char * c_pszFileName);
		bool			ReadMonsterDropItemGroup(const char * c_pszFileName);
		bool			ReadSpecialDropItemFile(const char * c_pszFileName);
		
		// convert name -> vnum special_item_group.txt
		bool			ConvSpecialDropItemFile();
		// convert name -> vnum special_item_group.txt

		uint32_t			GetRefineFromVnum(uint32_t dwVnum);

		static void		CopyAllAttrTo(CItem * pkOldItem, CItem * pkNewItem);		// copies all attributes and socket values to pkNewItem.

		const CSpecialItemGroup* GetSpecialItemGroup(uint32_t dwVnum);
		const CSpecialAttrGroup* GetSpecialAttrGroup(uint32_t dwVnum);

		// CHECK_UNIQUE_GROUP
		int			GetSpecialGroupFromItem(uint32_t dwVnum) const { auto it = m_ItemToSpecialGroup.find(dwVnum); return (it == m_ItemToSpecialGroup.end()) ? 0 : it->second; }
		// END_OF_CHECK_UNIQUE_GROUP

	protected:
		int                     RealNumber(uint32_t vnum);
		void			CreateQuestDropItem(CHARACTER * pkChr, CHARACTER * pkKiller, std::vector<CItem *> & vec_item, int iDeltaPercent, int iRandRange);

	protected:
		typedef std::map<uint32_t, CItem *> ITEM_VID_MAP;		

		std::vector<TItemTable>		m_vec_prototype;
		std::vector<TItemTable*> m_vec_item_vnum_range_info;
		std::map<uint32_t, uint32_t>		m_map_ItemRefineFrom;

		ITEM_VID_MAP			m_VIDMap;			///< saves items in units of m_dwVIDCount.
		uint32_t				m_dwVIDCount;			///< this is not a VID, just a per-process unique number.
		uint32_t				m_dwCurrentID;
		TItemIDRangeTable	m_ItemIDRange;
		TItemIDRangeTable	m_ItemIDSpareRange;

		std::unordered_set<CItem *> m_set_pkItemForDelayedSave;
		std::map<uint32_t, CItem *>		m_map_pkItemByID;
		std::map<uint32_t, uint32_t>		m_map_dwEtcItemDropProb;
		std::map<uint32_t, CDropItemGroup*> m_map_pkDropItemGroup;
		std::map<uint32_t, CSpecialItemGroup*> m_map_pkSpecialItemGroup;
		std::map<uint32_t, CSpecialItemGroup*> m_map_pkQuestItemGroup;
		std::map<uint32_t, CSpecialAttrGroup*> m_map_pkSpecialAttrGroup;
		std::map<uint32_t, CMobItemGroup*> m_map_pkMobItemGroup;
		std::map<uint32_t, CLevelItemGroup*> m_map_pkLevelItemGroup;
		std::map<uint32_t, CBuyerThiefGlovesItemGroup*> m_map_pkGloveItemGroup;

		// CHECK_UNIQUE_GROUP
		std::map<uint32_t, int>		m_ItemToSpecialGroup;
		// END_OF_CHECK_UNIQUE_GROUP
	
	private:
		// Germany wanted cash items identical to existing ones but tradeable,
		// so new items were made from the originals with only the no-trade flag removed,
		// and a new item range was allocated.
		// the problem is the new items must have the same effects as the originals,
		// but both server and client are vnum-based,
		// so we faced the sad situation of having to hardcode all the new vnums into the server.
		// so for an item with a new vnum, it runs on the server as the original item vnum,
		// and is converted back to its own vnum when saving.
		// for this, a map linking original vnums to new vnums is created.
		typedef std::map <uint32_t, uint32_t> TMapDW2DW;
		TMapDW2DW	m_map_new_to_ori;

	public:	
		uint32_t	GetMaskVnum(uint32_t dwVnum);
		std::map<uint32_t, TItemTable>  m_map_vid;
		
		const static int MAX_NORM_ATTR_NUM = 5;
		const static int MAX_RARE_ATTR_NUM = 2;
		bool ReadItemVnumMaskTable(const char * c_pszFileName);
	private:
};

extern bool DropEvent_RefineBox_SetValue(const std::string& name, int value);
extern bool DropEvent_CharStone_SetValue(const std::string& name, int value);
