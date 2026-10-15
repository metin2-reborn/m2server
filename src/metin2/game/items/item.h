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
		void	encode_insert_packet(entity * ent) override;
		void	encode_remove_packet(entity * ent) override;

	public:
		item(uint32_t vnum);
		~item() override;

		int			level_limit();

		bool		check_item_use_level(int level);

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

		// vnum() can be masked (ori_to_new.txt); original_vnum() is the real one, for the client,
		// logs and saving.
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
		bool		add_to_character(character * ch, item_pos cell_value);
		character *	owner()		{ return owner_; }

		item *		remove_from_ground();
		bool		add_to_ground(int32_t map_index, const pixel_position & pos, bool skip_owner_check = false);

		int			find_equip_cell(character * ch, int candidate_cell = -1);
		bool		is_equipped() const		{ return equipped_;	}
		bool		equip_to(character * ch, uint8_t wear_cell);
		bool		is_equipable() const;

		bool		can_used_by(character * ch);

		bool		distance_valid(character * ch);

		void		update_packet();

		void		set_exchanging(bool on = true);
		bool		is_exchanging()		{ return exchanging_;	}

		void		modify_points(bool add);

		const int32_t *	sockets()		{ return &sockets_[0];	}
		int32_t		get_socket(int i)	{ return sockets_[i];	}

		void		set_sockets(const int32_t * al);
		void		set_socket(int i, int32_t v, bool log = true);

		int		socket_count();
		bool		add_socket();

		const player_item_attribute* attributes()		{ return attr_;	} 
		const player_item_attribute& get_attribute(int i)	{ return attr_[i];	}

		uint8_t		attribute_type(int i)	{ return attr_[i].type;	}
		short		attribute_value(int i){ return attr_[i].s_value;	}

		void		set_attributes(const player_item_attribute* attribute);
		
		int		find_attribute(uint8_t type);
		bool		remove_attribute_type(uint8_t type);

		bool		has_attr(uint8_t apply);
		bool		has_rare_attr(uint8_t apply);

		void		set_destroy_event(event_ptr event);
		void		start_destroy_event(int sec=300);

		uint32_t		refined_vnum()	{ return proto_ ? proto_->refined_vnum : 0; }
		uint32_t		refine_from_vnum();
		int		refine_level();

		void		set_skip_save(bool b)	{ skip_save_ = b; }
		bool		skip_save()		{ return skip_save_; }

		bool		is_ownership(character * ch);
		void		set_ownership(character * ch, int sec = 10);
		void		set_ownership_event(event_ptr event);

		uint32_t		last_owner_pid()	{ return last_owner_pid_; }

		int		attribute_set_index();
		void		alter_to_magic_item();
		void		alter_to_socket_item(int socket_count);

		uint16_t		refine_set()		{ return proto_ ? proto_->refine_set : 0;	}

		void		start_unique_expire_event();
		void		set_unique_expire_event(event_ptr event);

		void		start_timer_based_on_wear_expire_event();
		void		set_timer_based_on_wear_expire_event(event_ptr event);

		void		start_real_time_expire_event();
		bool		is_real_time_item();

		void		stop_unique_expire_event();
		void		stop_timer_based_on_wear_expire_event();
		void		stop_accessory_socket_expire_event();

		int			duration();

		int		attribute_count();
		void		clear_attribute();
		void		change_attribute(const int* change_prob=nullptr);
		void		add_attribute();
		void		add_attribute(uint8_t apply, short s_value);

		void		apply_addon(int addon_type);

		int		special_group() const;
		bool	is_same_special_group(item * const item) const;

		bool		is_accessory_for_socket();

		int		accessory_socket_grade();
		int		accessory_socket_max_grade();
		int		accessory_socket_down_grade_time();

		void		set_accessory_socket_grade(int grade);
		void		set_accessory_socket_max_grade(int max_grade);
		void		set_accessory_socket_down_grade_time(uint32_t time);

		void		accessory_socket_degrade();

		void		start_accessory_socket_expire_event();
		void		set_accessory_socket_expire_event(event_ptr event);

		bool		can_put_into(item * item);

		void		copy_attribute_to(item * item_value);
		void		copy_socket_to(item * item_value);

		int			rare_attr_count();
		bool		add_rare_attribute();
		bool		change_rare_attribute();

		void		attr_log();

		void		lock(bool f) { is_locked_ = f; }
		bool		is_locked() const { return is_locked_; }

	private :
		void		set_attribute(int i, uint8_t type_value, short s_value_value);
	public:
		void		set_force_attribute(int i, uint8_t type_value, short s_value_value);

	protected:
		bool		unequip();

		void		add_attr(uint8_t apply, uint8_t level);
		void		put_attribute(const int * attr_percent_table);
		void		put_attribute_with_level(uint8_t level);

	protected:
		friend class protocol::input_db;
		bool		on_after_created_item();

	public:
		bool		is_ride_item();

		void		clear_mount_attribute_and_affect();


		void		set_mask_vnum(uint32_t vnum)	{	mask_vnum_ = vnum; }

		bool		is_dragon_soul();
		int		give_more_time_per(float percent);
		int		give_more_time_fix(uint32_t time);

	private:
		item_table const * proto_;

		uint32_t		vnum_;
		character *	owner_;

		uint8_t		window_;
		uint32_t		id_;
		bool		equipped_;
		uint32_t		vid_;
		uint16_t		cell_;
		uint32_t		count_;
		int32_t		flag_;
		uint32_t		last_owner_pid_;

		bool		exchanging_;

		int32_t		sockets_[ITEM_SOCKET_MAX_NUM];
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
		void set_sig_vnum(uint32_t sig)
		{
			sig_vnum_ = sig;
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
