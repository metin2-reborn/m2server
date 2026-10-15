#pragma once

#include <metin2/game/world/entity.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <cstring>

namespace metin2::game::inline protocol {
    class input_db;
}

namespace metin2::game::inline items {

class item : public entity
{
	protected:
		// override methods from ENTITY class
		void	encode_insert_packet(entity * entity) override;
		void	encode_remove_packet(entity * entity) override;

	public:
		item(uint32_t dwVnum);
		~item() override;

		int			level_limit();

		bool		check_item_use_level(int nLevel);

		bool		is_pc_bang_item();

		bool		is_stackable()		{ return (flag() & ITEM_FLAG_STACKABLE)?true:false; }

		void		initialize();
		void		destroy();

		void		save();

		void		set_window(uint8_t b)	{ window_ = b; }
		uint8_t		window()		{ return window_; }

		void		set_id(uint32_t id)		{ id_ = id;	}
		uint32_t		id()			{ return id_; }

		void			set_proto(const item_table * table);
		item_table const *	proto()	{ return proto_; }

		int		shop_buy_price();
		const char *	name()		{ return proto_ ? proto_->locale_name : nullptr; }
		const char *	base_name()		{ return proto_ ? proto_->name : nullptr; }
		uint8_t		size()		{ return proto_ ? proto_->size : 0;	}

		void		set_flag(int32_t flag)	{ flag_ = flag;	}
		int32_t		flag()		{ return flag_;	}

		uint32_t		wear_flag()		{ return proto_ ? proto_->wear_flags : 0; }
		uint32_t		anti_flag()		{ return proto_ ? proto_->anti_flags : 0; }
		uint32_t		immune_flag()		{ return proto_ ? proto_->immune_flag : 0; }

		void		set_vid(uint32_t vid)	{ vid_ = vid;	}
		uint32_t		get_vid()		{ return vid_;	}

		bool		set_count(uint32_t count);
		uint32_t		count();

		// Comment on GetVnum and GetOriginalVnum
		// GetVnum is the masked Vnum. With it, an item whose real Vnum is 10 can behave as if its Vnum were 20.
		// The masking values are defined in ori_to_new.txt.
		// GetOriginalVnum is the item's own Vnum; use it when logging, when sending item info to the client, and when saving.
		// 
		uint32_t		vnum() const		{ return mask_vnum_ ? mask_vnum_ : vnum_;	}
		uint32_t		original_vnum() const		{ return vnum_;	}
		uint8_t		type() const		{ return proto_ ? proto_->type : 0;	}
		uint8_t		sub_type() const	{ return proto_ ? proto_->sub_type : 0;	}
		uint8_t		limit_type(uint32_t idx) const { return proto_ ? proto_->limits[idx].type : 0;	}
		int32_t		limit_value(uint32_t idx) const { return proto_ ? proto_->limits[idx].value : 0;	}

		int32_t		value(uint32_t idx);

		void		set_cell(character * ch, uint16_t pos)	{ owner_ = ch, cell_ = pos;	}
		uint16_t		cell()				{ return cell_;	}

		item *		remove_from_character();
		bool		add_to_character(character * ch, item_pos Cell);
		character *	owner()		{ return owner_; }

		item *		remove_from_ground();
		bool		add_to_ground(int32_t lMapIndex, const pixel_position & pos, bool skipOwnerCheck = false);

		int			find_equip_cell(character * ch, int bCandidateCell = -1);
		bool		is_equipped() const		{ return equipped_;	}
		bool		equip_to(character * ch, uint8_t bWearCell);
		bool		is_equipable() const;

		bool		can_used_by(character * ch);

		bool		distance_valid(character * ch);

		void		update_packet();

		void		set_exchanging(bool isOn = true);
		bool		is_exchanging()		{ return exchanging_;	}

		void		modify_points(bool bAdd);	// Applies the item's effects to the character. Removes them if bAdd is false

		const int32_t *	sockets()		{ return &sockets_[0];	}
		int32_t		get_socket(int i)	{ return sockets_[i];	}

		void		set_sockets(const int32_t * al);
		void		set_socket(int i, int32_t v, bool bLog = true);

		int		socket_count();
		bool		add_socket();

		const player_item_attribute* attributes()		{ return attr_;	} 
		const player_item_attribute& get_attribute(int i)	{ return attr_[i];	}

		uint8_t		attribute_type(int i)	{ return attr_[i].type;	}
		short		attribute_value(int i){ return attr_[i].s_value;	}

		void		set_attributes(const player_item_attribute* c_pAttribute);
		
		int		find_attribute(uint8_t bType);
		bool		remove_attribute_type(uint8_t bType);

		bool		has_attr(uint8_t bApply);
		bool		has_rare_attr(uint8_t bApply);

		void		set_destroy_event(event_ptr pkEvent);
		void		start_destroy_event(int iSec=300);

		uint32_t		refined_vnum()	{ return proto_ ? proto_->refined_vnum : 0; }
		uint32_t		refine_from_vnum();
		int		refine_level();

		void		set_skip_save(bool b)	{ skip_save_ = b; }
		bool		skip_save()		{ return skip_save_; }

		bool		is_ownership(character * ch);
		void		set_ownership(character * ch, int iSec = 10);
		void		set_ownership_event(event_ptr pkEvent);

		uint32_t		last_owner_pid()	{ return last_owner_pid_; }

		int		attribute_set_index(); // Returns which index of the attribute-assignment array is used.
		void		alter_to_magic_item();
		void		alter_to_socket_item(int iSocketCount);

		uint16_t		refine_set()		{ return proto_ ? proto_->refine_set : 0;	}

		void		start_unique_expire_event();
		void		set_unique_expire_event(event_ptr pkEvent);

		void		start_timer_based_on_wear_expire_event();
		void		set_timer_based_on_wear_expire_event(event_ptr pkEvent);

		void		start_real_time_expire_event();
		bool		is_real_time_item();

		void		stop_unique_expire_event();
		void		stop_timer_based_on_wear_expire_event();
		void		stop_accessory_socket_expire_event();

		//			Currently only works correctly for REAL_TIME and TIMER_BASED_ON_WEAR items.
		int			duration();

		int		attribute_count();
		void		clear_attribute();
		void		change_attribute(const int* aiChangeProb=nullptr);
		void		add_attribute();
		void		add_attribute(uint8_t bType, short sValue);

		void		apply_addon(int iAddonType);

		int		special_group() const;
		bool	is_same_special_group(item * const item) const;

		// ACCESSORY_REFINE
		// Add a socket to an accessory via the mine
		bool		is_accessory_for_socket();

		int		accessory_socket_grade();
		int		accessory_socket_max_grade();
		int		accessory_socket_down_grade_time();

		void		set_accessory_socket_grade(int iGrade);
		void		set_accessory_socket_max_grade(int iMaxGrade);
		void		set_accessory_socket_down_grade_time(uint32_t time);

		void		accessory_socket_degrade();

		// Timer that runs when an accessory (copper, etc.) is set into an item
		void		start_accessory_socket_expire_event();
		void		set_accessory_socket_expire_event(event_ptr pkEvent);

		bool		can_put_into(item * item);
		// END_OF_ACCESSORY_REFINE

		void		copy_attribute_to(item * pItem);
		void		copy_socket_to(item * pItem);

		int			rare_attr_count();
		bool		add_rare_attribute();
		bool		change_rare_attribute();

		void		attr_log();

		void		lock(bool f) { is_locked_ = f; }
		bool		is_locked() const { return is_locked_; }

	private :
		void		set_attribute(int i, uint8_t bType, short sValue);
	public:
		void		set_force_attribute(int i, uint8_t bType, short sValue);

	protected:
		bool		unequip();

		void		add_attr(uint8_t bApply, uint8_t bLevel);
		void		put_attribute(const int * aiAttrPercentTable);
		void		put_attribute_with_level(uint8_t bLevel);

	protected:
		friend class protocol::input_db;
		bool		on_after_created_item();			// Called after the item has been fully created (loaded) on the server with all its data.

	public:
		bool		is_ride_item();

		void		clear_mount_attribute_and_affect();

		// Germany wanted cash items identical to the existing ones but tradable,
		// so new items, the originals with only the no-trade flag removed, were assigned to a new item range.
		// The problem is that the new items must have the same effects as the originals,
		// but both server and client are vnum based,
		// so we faced the sad situation of having to hardcode all the new vnums into the server.
		// So for an item with a new vnum, the server runs it as the original item vnum,
		// and changes it back to the real vnum when saving.

		// The mask vnum exists for items that run under a different vnum for some reason (e.g. the situation above).
		void		set_mask_vnum(uint32_t vnum)	{	mask_vnum_ = vnum; }

		// Dragon soul
		bool		is_dragon_soul();
		int		give_more_time_per(float fPercent);
		int		give_more_time_fix(uint32_t dwTime);

	private:
		item_table const * proto_;		// Prototype

		uint32_t		vnum_;
		character *	owner_;

		uint8_t		window_;		// Window the item is currently in 
		uint32_t		id_;			// Unique ID
		bool		equipped_;	// Is it equipped?
		uint32_t		vid_;		// VID
		uint16_t		cell_;		// Position
		uint32_t		count_;		// Count
		int32_t		flag_;		// Extra flag
		uint32_t		last_owner_pid_;	// PID of the last owner

		bool		exchanging_;	///< Currently being traded 

		int32_t		sockets_[ITEM_SOCKET_MAX_NUM];	// Item sockets
		player_item_attribute	attr_[ITEM_ATTRIBUTE_MAX_NUM];

		event_ptr		destroy_event_;
		event_ptr		unique_expire_event_;
		event_ptr		timer_based_on_wear_expire_event_;
		event_ptr		real_time_expire_event_;
		event_ptr		accessory_socket_expire_event_;
		event_ptr		ownership_event_;

		uint32_t		ownership_pid_;

		bool		skip_save_;

		bool		is_locked_;
		
		uint32_t		mask_vnum_;
		uint32_t		sig_vnum_;
	public:
		void set_sig_vnum(uint32_t dwSIG)
		{
			sig_vnum_ = dwSIG;
		}
		uint32_t	sig_vnum() const
		{
			return sig_vnum_;
		}
};

EVENTINFO(item_event_info)
{
	game::items::item * item;
	char owner_name[CHARACTER_NAME_MAX_LEN];

	item_event_info() 
	: item( nullptr )
	{
		::memset( owner_name, 0, CHARACTER_NAME_MAX_LEN );
	}
};

EVENTINFO(item_vid_event_info)
{
	uint32_t item_vid;

	item_vid_event_info() 
	: item_vid( 0 )
	{
	}
};
}
