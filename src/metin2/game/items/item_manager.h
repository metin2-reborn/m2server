#pragma once
#include <metin2/game/characters/char.h>
#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>
#include <metin2/proto/tables.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include <sys/types.h>

namespace metin2::game::inline items {

// attribute group defined in special_item_group.txt
// can be declared with type attr.
// this attribute group can be used by UNIQUE ITEMs belonging to groups defined as Special type in special_item_group.txt.
class special_attr_group
{
public:
	special_attr_group(uint32_t vnum_value)
		: vnum(vnum_value)
	{}
	struct special_attr_info
	{
		special_attr_info (uint32_t _apply_type, uint32_t _apply_value)
			: apply_type(_apply_type), apply_value(_apply_value)
		{}
		uint32_t apply_type;
		uint32_t apply_value;

	};
	uint32_t vnum;
	std::string	effect_file_name;
	std::vector<special_attr_info> attrs;
};

class special_item_group
{
	public:
		enum give_type
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
		enum sig_type { NORMAL, PCT, QUEST, SPECIAL };

		struct special_item_info
		{
			uint32_t vnum;
			int count;
			int rare;

			special_item_info(uint32_t _vnum, int _count, int _rare)
				: vnum(_vnum), count(_count), rare(_rare)
				{}
		};

		special_item_group(uint32_t vnum_value, uint8_t type_value=0)
			: vnum(vnum_value), type(type_value)
			{}

		void add_item(uint32_t vnum, int count, int prob, int rare)
		{
			if (!prob)
				return;
			if (!probs.empty())
				prob += probs.back();
			probs.push_back(prob);
			items.push_back(special_item_info(vnum, count, rare));
		}

		// Type Multi, i.e. when m_bType == PCT,
		// probabilities are not accumulated; each is computed independently to create items.
		// so multiple items may be created.
		// by rtsummit
		int multi_index(std::vector <int> &idx_vec) const
		{
			idx_vec.clear();
			if (type == PCT)
			{
				int count = 0;
				if (number(1,100) <= probs[0])
				{
					idx_vec.push_back(0);
					count++;
				}
				for (uint i = 1; i < probs.size(); i++)
				{
					if (number(1,100) <= probs[i] - probs[i-1])
					{
						idx_vec.push_back(i);
						count++;
					}
				}
				return count;
			}
			else
			{
				idx_vec.push_back(one_index());
				return 1;
			}
		}

		int one_index() const
		{
			int n = number(1, probs.back());
			auto it = lower_bound(probs.begin(), probs.end(), n);
			return std::distance(probs.begin(), it);
		}

		int get_vnum(int idx) const
		{
			return items[idx].vnum;
		}

		int count(int idx) const
		{
			return items[idx].count;
		}

		int rare_pct(int idx) const
		{
			return items[idx].rare;
		}

		bool contains(uint32_t dwVnum) const
		{
			for (uint32_t i = 0; i < items.size(); i++)
			{
				if (items[i].vnum == dwVnum)
					return true;
			}
			return false;
		}
		
		// when the Group's Type is Special
		// returns the AttrVnum matching dwVnum.
		uint32_t attr_vnum(uint32_t dwVnum) const
		{
			if (special_item_group::SPECIAL != type)
				return 0;
			for (auto it = items.begin(); it != items.end(); it++)
			{
				if (it->vnum == dwVnum)
				{
					return it->count;
				}
			}
			return 0;
		}

		uint32_t vnum;
		uint8_t	type;
		std::vector<int> probs;
		std::vector<special_item_info> items; // vnum, count
};

class mob_item_group
{
	public:
		struct mob_item_group_info
		{
			uint32_t item_vnum;
			int count;
			int rare_pct;

			mob_item_group_info(uint32_t dwItemVnum, int iCount, int iRarePct)
				: item_vnum(dwItemVnum),
			count(iCount),
			rare_pct(iRarePct)
			{
			}
		};

		explicit mob_item_group(int iKillDrop) : kill_drop_(iKillDrop)
		{
		}

		int kill_per_drop() const
		{
			return kill_drop_;
		}

		void add_item(uint32_t dwItemVnum, int iCount, int iPartPct, int iRarePct)
		{
			if (!probs_.empty())
				iPartPct += probs_.back();
			probs_.push_back(iPartPct);
			items_.push_back(mob_item_group_info(dwItemVnum, iCount, iRarePct));
		}

		// MOB_DROP_ITEM_BUG_FIX
		bool is_empty() const
		{
			return probs_.empty();
		}

		int one_index() const
		{
			int n = number(1, probs_.back());
			auto it = lower_bound(probs_.begin(), probs_.end(), n);
			return std::distance(probs_.begin(), it);
		}
		// END_OF_MOB_DROP_ITEM_BUG_FIX

		const mob_item_group_info& one() const
		{
			return items_[one_index()];
		}

	private:
		int kill_drop_;
		std::vector<int> probs_;
		std::vector<mob_item_group_info> items_;
};

class drop_item_group
{
	struct drop_item_group_info
	{
		uint32_t	vnum;
		uint32_t	pct;
		int	count;

		drop_item_group_info(uint32_t dwVnum, uint32_t dwPct, int iCount)
			: vnum(dwVnum), pct(dwPct), count(iCount)
			{}
	};

	public:
	drop_item_group() = default;

	const std::vector<drop_item_group_info> & get_vector()
	{
		return vec_items_;
	}

	void add_item(uint32_t dwItemVnum, uint32_t dwPct, int iCount)
	{
		vec_items_.push_back(drop_item_group_info(dwItemVnum, dwPct, iCount));
	}

	private:
	std::vector<drop_item_group_info> vec_items_;
};

class level_item_group
{
	struct level_item_group_info
	{
		uint32_t v_num;
		uint32_t pct;
		int count;

		level_item_group_info(uint32_t dwVnum, uint32_t dwPct, int iCount)
			: v_num(dwVnum), pct(dwPct), count(iCount)
		{ }
	};

	private :
		uint32_t level_limit_;
		std::vector<level_item_group_info> vec_items_;

	public :
		level_item_group(uint32_t dwLevelLimit)
			: level_limit_(dwLevelLimit)
		{}

		uint32_t level_limit() { return level_limit_; }

		void add_item(uint32_t dwItemVnum, uint32_t dwPct, int iCount)
		{
			vec_items_.push_back(level_item_group_info(dwItemVnum, dwPct, iCount));
		}

		const std::vector<level_item_group_info> & get_vector()
		{
			return vec_items_;
		}
};

class buyer_thief_gloves_item_group
{
	struct thief_group_info
	{
		uint32_t	vnum;
		uint32_t	pct;
		int	count;

		thief_group_info(uint32_t dwVnum, uint32_t dwPct, int iCount)
			: vnum(dwVnum), pct(dwPct), count(iCount)
			{}
	};

	public:
	buyer_thief_gloves_item_group() = default;

	const std::vector<thief_group_info> & get_vector()
	{
		return vec_items_;
	}

	void add_item(uint32_t dwItemVnum, uint32_t dwPct, int iCount)
	{
		vec_items_.push_back(thief_group_info(dwItemVnum, dwPct, iCount));
	}

	private:
	std::vector<thief_group_info> vec_items_;
};


class item_manager : public singleton<item_manager>
{
	public:
		item_manager();
		~item_manager() override;

		bool                    initialize(item_table * table, int size);
		void			destroy();
		void			update();	// called every loop.
		void			graceful_shutdown();

		uint32_t			new_id();
		bool			set_max_item_id(item_id_range_table range); // set the maximum unique ID
		bool			set_max_spare_item_id(item_id_range_table range);

		// DelayedSave: if a routine does many things that require saving,
		// there are too many queries, so just mark "to be saved" and save
		// shortly after (e.g. 1 frame).
		void			delayed_save(item * item);
		void			flush_delayed_save(item * item); // if in the Delayed list, remove it and save. used on disconnect handling.
		void			save_single_item(item * item);

		item *                  create_item(uint32_t vnum, uint32_t count = 1, uint32_t dwID = 0, bool bTryMagic = false, int iRarePct = -1, bool bSkipSave = false);
		void destroy_item(item * item);
		void			remove_item(item * item, const char * c_pszReason=nullptr); // remove the item from the user

		item *			find(uint32_t id);
		item *                  find_by_vid(uint32_t vid);
		item_table *            table(uint32_t vnum);
		bool			vnum(const char * c_pszName, uint32_t & r_dwVnum);
		bool			vnum_by_original_name(const char * c_pszName, uint32_t & r_dwVnum);

		bool			drop_pct(character * pkChr, character * pkKiller, int& iDeltaPercent, int& iRandRange);
		bool			create_drop_item(character * pkChr, character * pkKiller, std::vector<item *> & vec_item);

		bool			read_common_drop_item_file(const char * c_pszFileName);
		bool			read_etc_drop_item_file(const char * c_pszFileName);
		bool			read_drop_item_group(const char * c_pszFileName);
		bool			read_monster_drop_item_group(const char * c_pszFileName);
		bool			read_special_drop_item_file(const char * c_pszFileName);
		
		// convert name -> vnum special_item_group.txt
		bool			conv_special_drop_item_file();
		// convert name -> vnum special_item_group.txt

		uint32_t			refine_from_vnum(uint32_t dwVnum);

		static void		copy_all_attr_to(item * pkOldItem, item * pkNewItem);		// copies all attributes and socket values to pkNewItem.

		const special_item_group* get_special_item_group(uint32_t dwVnum);
		const special_attr_group* get_special_attr_group(uint32_t dwVnum);

		// CHECK_UNIQUE_GROUP
		int			special_group_from_item(uint32_t dwVnum) const { auto it = item_to_special_group_.find(dwVnum); return (it == item_to_special_group_.end()) ? 0 : it->second; }
		// END_OF_CHECK_UNIQUE_GROUP

	protected:
		int                     real_number(uint32_t vnum);
		void			create_quest_drop_item(character * pkChr, character * pkKiller, std::vector<item *> & vec_item, int iDeltaPercent, int iRandRange);

	protected:
		typedef std::map<uint32_t, item *> item_vid_map;		

		std::vector<item_table>		vec_prototype_;
		std::vector<item_table*> vec_item_vnum_range_info_;
		std::map<uint32_t, uint32_t>		map_item_refine_from_;

		item_vid_map			vid_map_;			///< saves items in units of m_dwVIDCount.
		uint32_t				vid_count_;			///< this is not a VID, just a per-process unique number.
		uint32_t				current_id_;
		item_id_range_table	item_id_range_;
		item_id_range_table	item_id_spare_range_;

		std::unordered_set<item *> set_pk_item_for_delayed_save_;
		std::map<uint32_t, item *>		map_pk_item_by_id_;
		std::map<uint32_t, uint32_t>		map_dw_etc_item_drop_prob_;
		std::map<uint32_t, drop_item_group*> map_pk_drop_item_group_;
		std::map<uint32_t, special_item_group*> map_pk_special_item_group_;
		std::map<uint32_t, special_item_group*> map_pk_quest_item_group_;
		std::map<uint32_t, special_attr_group*> map_pk_special_attr_group_;
		std::map<uint32_t, mob_item_group*> map_pk_mob_item_group_;
		std::map<uint32_t, level_item_group*> map_pk_level_item_group_;
		std::map<uint32_t, buyer_thief_gloves_item_group*> map_pk_glove_item_group_;

		// CHECK_UNIQUE_GROUP
		std::map<uint32_t, int>		item_to_special_group_;
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
		typedef std::map <uint32_t, uint32_t> map_d_w2_dw;
		map_d_w2_dw	map_new_to_ori_;

	public:	
		uint32_t	mask_vnum(uint32_t dwVnum);
		std::map<uint32_t, item_table>  map_vid;
		
		const static int max_norm_attr_num = 5;
		const static int max_rare_attr_num = 2;
		bool read_item_vnum_mask_table(const char * c_pszFileName);
	private:
};

extern bool drop_event_refine_box_set_value(const std::string& name, int value);
extern bool drop_event_char_stone_set_value(const std::string& name, int value);
}
