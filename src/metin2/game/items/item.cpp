#include <metin2/game/items/item.h>

#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/buff_on_attributes.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/items/dragon_soul.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/app/event.h>
#include <metin2/game/items/item_addon.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/world/sectree.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/common/utils.h>
#include <metin2/proto/vnum_helper.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstring>
#include <ctime>

namespace metin2::game::inline items {

item::item(uint32_t vnum)
	: vnum_(vnum), window_(0), id_(0), equipped_(false), vid_(0), cell_(0), count_(0), flag_(0), last_owner_pid_(0),
	exchanging_(false), destroy_event_(nullptr), unique_expire_event_(nullptr), timer_based_on_wear_expire_event_(nullptr), real_time_expire_event_(nullptr),
   	accessory_socket_expire_event_(nullptr), ownership_event_(nullptr), ownership_pid_(0), skip_save_(false), is_locked_(false),
	mask_vnum_(0), sig_vnum_ (0)
{
	memset( &sockets_, 0, sizeof(sockets_) );
	memset( &attr_, 0, sizeof(attr_) );
}

item::~item()
{
	destroy();
}

void item::initialize()
{
	entity::initialize(ENTITY_ITEM);

	window_ = RESERVED_WINDOW;
	owner_ = nullptr;
	id_ = 0;
	equipped_ = false;
	vid_ = cell_ = count_ = flag_ = 0;
	proto_ = nullptr;
	exchanging_ = false;
	memset(&sockets_, 0, sizeof(sockets_));
	memset(&attr_, 0, sizeof(attr_));

	destroy_event_ = nullptr;
	ownership_event_ = nullptr;
	ownership_pid_ = 0;
	unique_expire_event_ = nullptr;
	timer_based_on_wear_expire_event_ = nullptr;
	real_time_expire_event_ = nullptr;

	accessory_socket_expire_event_ = nullptr;

	skip_save_ = false;
	last_owner_pid_ = 0;
}

void item::destroy()
{
	event_cancel(&destroy_event_);
	event_cancel(&ownership_event_);
	event_cancel(&unique_expire_event_);
	event_cancel(&timer_based_on_wear_expire_event_);
	event_cancel(&real_time_expire_event_);
	event_cancel(&accessory_socket_expire_event_);

	entity::destroy();

	if (get_sectree())
		get_sectree()->remove_entity(this);
}

EVENTFUNC(item_destroy_event)
{
	item_event_info* info = dynamic_cast<item_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "item_destroy_event> <Factor> Null pointer" );
		return 0;
	}

	item * pkItem = info->item;

	if (pkItem->owner())
		metin2::log::error("item_destroy_event: Owner exist. (item {} owner {})", pkItem->name(), pkItem->owner()->name());

	pkItem->set_destroy_event(nullptr);
	item_manager::instance().destroy_item(pkItem);
	return 0;
}

void item::set_destroy_event(event_ptr event)
{
	destroy_event_ = event;
}

void item::start_destroy_event(int sec)
{
	if (destroy_event_)
		return;

	item_event_info* info = alloc_event_info<item_event_info>();
	info->item = this;

	set_destroy_event(event_create(item_destroy_event, info, passes_per_sec(sec)));
}

void item::encode_insert_packet(entity * ent)
{
	desc * d;

	if (!(d = ent->get_desc()))
		return;

	const pixel_position & c_pos = xyz();

	struct packet_gc_item_ground_add pack;

	pack.header	= HEADER_GC_ITEM_GROUND_ADD;
	pack.x		= c_pos.x;
	pack.y		= c_pos.y;
	pack.z		= c_pos.z;
	pack.vnum		= vnum();
	pack.vid		= vid_;

	d->packet(&pack, sizeof(pack));

	if (ownership_event_ != nullptr)
	{
		item_event_info * info = dynamic_cast<item_event_info *>(ownership_event_->info);

		if ( info == nullptr )
		{
			metin2::log::error( "CItem::EncodeInsertPacket> <Factor> Null pointer" );
			return;
		}

		packet_gc_item_ownership p;

		p.header = HEADER_GC_ITEM_OWNERSHIP;
		p.vid = vid_;
		strlcpy(p.name, info->owner_name, sizeof(p.name));

		d->packet(&p, sizeof(packet_gc_item_ownership));
	}
}

void item::encode_remove_packet(entity * ent)
{
	desc * d;

	if (!(d = ent->get_desc()))
		return;

	struct packet_gc_item_ground_del pack;

	pack.header	= HEADER_GC_ITEM_GROUND_DEL;
	pack.vid		= vid_;

	d->packet(&pack, sizeof(pack));
	metin2::log::debug( "Item::EncodeRemovePacket {} to {}", name(), ((character *) ent)->name());
}

void item::set_proto(const item_table * table)
{
	assert(table != nullptr);
	proto_ = table;
	set_flag(proto_->flags);
}

void item::update_packet()
{
	if (!owner_ || !owner_->get_desc())
		return;

	packet_gc_item_update pack;

	pack.header = HEADER_GC_ITEM_UPDATE;
	pack.cell = item_pos(window(), cell_);
	pack.count	= count_;

	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		pack.sockets[i] = sockets_[i];

	std::memcpy(pack.attr, attributes(), sizeof(pack.attr));

	metin2::log::debug( "UpdatePacket {} -> {}", name(), owner_->name());
	owner_->get_desc()->packet(&pack, sizeof(pack));
}

uint32_t item::count()
{
	if (type() == ITEM_ELK) return std::min<int>(count_, INT_MAX);
	else
	{
		return std::min<int>(count_, 200);
	}
}

bool item::set_count(uint32_t count)
{
	if (type() == ITEM_ELK)
	{
		count_ = std::min<int>(count, INT_MAX);
	}
	else
	{
		count_ = std::min<int>(count, ITEM_MAX_COUNT);
	}

	if (count == 0 && owner_)
	{
		if (sub_type() == USE_ABILITY_UP || sub_type() == USE_POTION || vnum() == 70020)
		{
			character * pOwner = owner();
			uint16_t wCell = cell();

			remove_from_character();

			if (!is_dragon_soul())
			{
				item * pItem = pOwner->find_specify_item(vnum());

				if (nullptr != pItem)
				{
					pOwner->chain_quickslot_item(pItem, QUICKSLOT_TYPE_ITEM, wCell);
				}
				else
				{
					pOwner->sync_quickslot(QUICKSLOT_TYPE_ITEM, wCell, 255);
				}
			}

			item_manager::instance().destroy_item(this);
		}
		else
		{
			if (!is_dragon_soul())
			{
				owner_->sync_quickslot(QUICKSLOT_TYPE_ITEM, cell_, 255);
			}
			item_manager::instance().destroy_item(remove_from_character());
		}

		return false;
	}

	update_packet();

	save();
	return true;
}

item * item::remove_from_character()
{
	if (!owner_)
	{
		metin2::log::error("Item::RemoveFromCharacter owner null");
		return (this);
	}

	character * pOwner = owner_;

	if (equipped_)
	{
		unequip();

		set_window(RESERVED_WINDOW);
		save();
		return (this);
	}
	else
	{
		if (window() != SAFEBOX && window() != MALL)
		{
			if (is_dragon_soul())
			{
				if (cell_ >= DRAGON_SOUL_INVENTORY_MAX_NUM)
					metin2::log::error("CItem::RemoveFromCharacter: pos >= DRAGON_SOUL_INVENTORY_MAX_NUM");
				else
					pOwner->set_item(item_pos(window_, cell_), nullptr);
			}
			else
			{
				item_pos cell(INVENTORY, cell_);

				if (false == cell.is_default_inventory_position() && false == cell.is_belt_inventory_position())
					metin2::log::error("CItem::RemoveFromCharacter: Invalid Item Position");
				else
				{
					pOwner->set_item(cell, nullptr);
				}
			}
		}

		owner_ = nullptr;
		cell_ = 0;

		set_window(RESERVED_WINDOW);
		save();
		return (this);
	}
}

bool item::add_to_character(character * ch, item_pos cell_value)
{
	assert(get_sectree() == nullptr);
	assert(owner_ == nullptr);
	uint16_t pos = cell_value.cell;
	uint8_t window_type = cell_value.window_type;
	
	if (INVENTORY == window_type)
	{
		if (cell_ >= INVENTORY_MAX_NUM && BELT_INVENTORY_SLOT_START > cell_)
		{
			metin2::log::error("CItem::AddToCharacter: cell overflow: {} to {} cell {:d}", proto_->name, ch->name(), cell_);
			return false;
		}
	}
	else if (DRAGON_SOUL_INVENTORY == window_type)
	{
		if (cell_ >= DRAGON_SOUL_INVENTORY_MAX_NUM)
		{
			metin2::log::error("CItem::AddToCharacter: cell overflow: {} to {} cell {:d}", proto_->name, ch->name(), cell_);
			return false;
		}
	}

	if (ch->get_desc())
		last_owner_pid_ = ch->player_id();

	event_cancel(&destroy_event_);

	ch->set_item(item_pos(window_type, pos), this);
	owner_ = ch;

	save();
	return true;
}

item * item::remove_from_ground()
{
	if (get_sectree())
	{
		set_ownership(nullptr);
		
		get_sectree()->remove_entity(this);
		
		view_cleanup();

		save();
	}

	return (this);
}

bool item::add_to_ground(int32_t map_index, const pixel_position & pos, bool skip_owner_check)
{
	if (0 == map_index)
	{
		metin2::log::error("wrong map index argument: {:d}", map_index);
		return false;
	}

	if (get_sectree())
	{
		metin2::log::error("sectree already assigned");
		return false;
	}

	if (!skip_owner_check && owner_)
	{
		metin2::log::error("owner pointer not null");
		return false;
	}

	sectree * tree = sectree_manager::instance().get(map_index, pos.x, pos.y);

	if (!tree)
	{
		metin2::log::error("cannot find sectree by {:d}x{:d}", pos.x, pos.y);
		return false;
	}

	set_window(GROUND);
	set_xyz(pos.x, pos.y, pos.z);
	tree->insert_entity(this);
	update_sectree();
	save();
	return true;
}

bool item::distance_valid(character * ch)
{
	if (!get_sectree())
		return false;

	int iDist = distance_approx(x() - ch->x(), y() - ch->y());

	if (iDist > 300)
		return false;

	return true;
}

bool item::can_used_by(character * ch)
{
	switch (ch->job())
	{
		case JOB_WARRIOR:
			if (anti_flag() & ITEM_ANTIFLAG_WARRIOR)
				return false;
			break;

		case JOB_ASSASSIN:
			if (anti_flag() & ITEM_ANTIFLAG_ASSASSIN)
				return false;
			break;

		case JOB_SHAMAN:
			if (anti_flag() & ITEM_ANTIFLAG_SHAMAN)
				return false;
			break;

		case JOB_SURA:
			if (anti_flag() & ITEM_ANTIFLAG_SURA)
				return false;
			break;
	}

	return true;
}

int item::find_equip_cell(character * ch, int candidate_cell)
{
	if ((0 == wear_flag() || ITEM_TOTEM == type()) && ITEM_COSTUME != type() && ITEM_DS != type() && ITEM_SPECIAL_DS != type() && ITEM_RING != type() && ITEM_BELT != type())
		return -1;

	if (type() == ITEM_DS || type() == ITEM_SPECIAL_DS)
	{
		if (candidate_cell < 0)
		{
			return WEAR_MAX_NUM + sub_type();
		}
		else
		{
			for (int i = 0; i < DRAGON_SOUL_DECK_MAX_NUM; i++)
			{
				if (WEAR_MAX_NUM + i * DS_SLOT_MAX + sub_type() == candidate_cell)
				{
					return candidate_cell;
				}
			}
			return -1;
		}
	}
	else if (type() == ITEM_COSTUME)
	{
		if (sub_type() == COSTUME_BODY)
			return WEAR_COSTUME_BODY;
		else if (sub_type() == COSTUME_HAIR)
			return WEAR_COSTUME_HAIR;
	}
	else if (type() == ITEM_RING)
	{
		if (ch->wear(WEAR_RING1))
			return WEAR_RING2;
		else
			return WEAR_RING1;
	}
	else if (type() == ITEM_BELT)
		return WEAR_BELT;
	else if (wear_flag() & WEARABLE_BODY)
		return WEAR_BODY;
	else if (wear_flag() & WEARABLE_HEAD)
		return WEAR_HEAD;
	else if (wear_flag() & WEARABLE_FOOTS)
		return WEAR_FOOTS;
	else if (wear_flag() & WEARABLE_WRIST)
		return WEAR_WRIST;
	else if (wear_flag() & WEARABLE_WEAPON)
		return WEAR_WEAPON;
	else if (wear_flag() & WEARABLE_SHIELD)
		return WEAR_SHIELD;
	else if (wear_flag() & WEARABLE_NECK)
		return WEAR_NECK;
	else if (wear_flag() & WEARABLE_EAR)
		return WEAR_EAR;
	else if (wear_flag() & WEARABLE_ARROW)
		return WEAR_ARROW;
	else if (wear_flag() & WEARABLE_UNIQUE)
	{
		if (ch->wear(WEAR_UNIQUE1))
			return WEAR_UNIQUE2;
		else
			return WEAR_UNIQUE1;		
	}

	else if (wear_flag() & WEARABLE_ABILITY)
	{
		if (!ch->wear(WEAR_ABILITY1))
		{
			return WEAR_ABILITY1;
		}
		else if (!ch->wear(WEAR_ABILITY2))
		{
			return WEAR_ABILITY2;
		}
		else if (!ch->wear(WEAR_ABILITY3))
		{
			return WEAR_ABILITY3;
		}
		else if (!ch->wear(WEAR_ABILITY4))
		{
			return WEAR_ABILITY4;
		}
		else if (!ch->wear(WEAR_ABILITY5))
		{
			return WEAR_ABILITY5;
		}
		else if (!ch->wear(WEAR_ABILITY6))
		{
			return WEAR_ABILITY6;
		}
		else if (!ch->wear(WEAR_ABILITY7))
		{
			return WEAR_ABILITY7;
		}
		else if (!ch->wear(WEAR_ABILITY8))
		{
			return WEAR_ABILITY8;
		}
		else
		{
			return -1;
		}
	}
	return -1;
}

void item::modify_points(bool add)
{
	int accessoryGrade;

	if (false == is_accessory_for_socket())
	{
		if (proto_->type == ITEM_WEAPON || proto_->type == ITEM_ARMOR)
		{
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			{
				uint32_t dwVnum;

				if ((dwVnum = get_socket(i)) <= 2)
					continue;

				item_table * p = item_manager::instance().table(dwVnum);

				if (!p)
				{
					metin2::log::error("cannot find table by vnum {:d}", dwVnum);
					continue;
				}

				if (ITEM_METIN == p->type)
				{
					for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
					{
						if (p->applies[i].type == APPLY_NONE)
							continue;

						if (p->applies[i].type == APPLY_SKILL)
							owner_->apply_point(p->applies[i].type, add ? p->applies[i].value : p->applies[i].value ^ 0x00800000);
						else
							owner_->apply_point(p->applies[i].type, add ? p->applies[i].value : -p->applies[i].value);
					}
				}
			}
		}

		accessoryGrade = 0;
	}
	else
	{
		accessoryGrade = std::min<int>(accessory_socket_grade(), ITEM_ACCESSORY_SOCKET_MAX_NUM);
	}

	for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
	{
		if (proto_->applies[i].type == APPLY_NONE)
			continue;

		int32_t value = proto_->applies[i].value;

		if (proto_->applies[i].type == APPLY_SKILL)
		{
			owner_->apply_point(proto_->applies[i].type, add ? value : value ^ 0x00800000);
		}
		else
		{
			if (0 != accessoryGrade)
				value += std::max(accessoryGrade, value * accessory_socket_effective_pct[accessoryGrade] / 100);

			owner_->apply_point(proto_->applies[i].type, add ? value : -value);
		}
	}
	if (true == item_vnum_helper::is_ramadan_moon_ring(vnum()) || true == item_vnum_helper::is_halloween_candy(vnum())
		|| true == item_vnum_helper::is_happiness_ring(vnum()) || true == item_vnum_helper::is_love_pendant(vnum()))
	{
	}
	else
	{
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			if (attribute_type(i))
			{
				const player_item_attribute& ia = get_attribute(i);

				if (ia.type == APPLY_SKILL)
					owner_->apply_point(ia.type, add ? ia.s_value : ia.s_value ^ 0x00800000);
				else
					owner_->apply_point(ia.type, add ? ia.s_value : -ia.s_value);
			}
		}
	}

	switch (proto_->type)
	{
		case ITEM_PICK:
		case ITEM_ROD:
			{
				if (add)
				{
					if (cell_ == INVENTORY_MAX_NUM + WEAR_WEAPON)
						owner_->set_part(PART_WEAPON, vnum());
				}
				else
				{
					if (cell_ == INVENTORY_MAX_NUM + WEAR_WEAPON)
						owner_->set_part(PART_WEAPON, owner_->original_part(PART_WEAPON));
				}
			}
			break;

		case ITEM_WEAPON:
			{
				if (add)
				{
					if (cell_ == INVENTORY_MAX_NUM + WEAR_WEAPON)
						owner_->set_part(PART_WEAPON, vnum());
				}
				else
				{
					if (cell_ == INVENTORY_MAX_NUM + WEAR_WEAPON)
						owner_->set_part(PART_WEAPON, owner_->original_part(PART_WEAPON));
				}
			}
			break;

		case ITEM_ARMOR:
			{
				if (nullptr != owner_->wear(WEAR_COSTUME_BODY))
					break;

				if (sub_type() == ARMOR_BODY || sub_type() == ARMOR_HEAD || sub_type() == ARMOR_FOOTS || sub_type() == ARMOR_SHIELD)
				{
					if (add)
					{
						if (proto()->sub_type == ARMOR_BODY)
							owner_->set_part(PART_MAIN, vnum());
					}
					else
					{
						if (proto()->sub_type == ARMOR_BODY)
							owner_->set_part(PART_MAIN, owner_->original_part(PART_MAIN));
					}
				}
			}
			break;

		case ITEM_COSTUME:
			{
				uint32_t toSetValue = this->vnum();
				parts toSetPart = PART_MAX_NUM;

				if (sub_type() == COSTUME_BODY)
				{
					toSetPart = PART_MAIN;

					if (false == add)
					{
						const item* pArmor = owner_->wear(WEAR_BODY);
						toSetValue = (nullptr != pArmor) ? pArmor->vnum() : owner_->original_part(PART_MAIN);						
					}
					
				}

				else if (sub_type() == COSTUME_HAIR)
				{
					toSetPart = PART_HAIR;

					toSetValue = (true == add) ? this->value(3) : 0;
				}

				if (PART_MAX_NUM != toSetPart)
				{
					owner_->set_part((uint8_t)toSetPart, toSetValue);
					owner_->update_packet();
				}
			}
			break;
		case ITEM_UNIQUE:
			{
				if (0 != sig_vnum())
				{
					const special_item_group* pItemGroup = item_manager::instance().get_special_item_group(sig_vnum());
					if (nullptr == pItemGroup)
						break;
					uint32_t dwAttrVnum = pItemGroup->attr_vnum(vnum());
					const special_attr_group* pAttrGroup = item_manager::instance().get_special_attr_group(dwAttrVnum);
					if (nullptr == pAttrGroup)
						break;
					for (auto it = pAttrGroup->attrs.begin(); it != pAttrGroup->attrs.end(); it++)
					{
						owner_->apply_point(it->apply_type, add ? it->apply_value : -it->apply_value);
					}
				}
			}
			break;
	}
}

bool item::is_equipable() const
{
	switch (this->type())
	{
	case ITEM_COSTUME:
	case ITEM_ARMOR:
	case ITEM_WEAPON:
	case ITEM_ROD:
	case ITEM_PICK:
	case ITEM_UNIQUE:
	case ITEM_DS:
	case ITEM_SPECIAL_DS:
	case ITEM_RING:
	case ITEM_BELT:
		return true;
	}

	return false;
}

bool item::equip_to(character * ch, uint8_t wear_cell)
{
	if (!ch)
	{
		metin2::log::error("EquipTo: nil character");
		return false;
	}

	if (is_dragon_soul())
	{
		if (wear_cell < WEAR_MAX_NUM || wear_cell >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * static_cast<int>(DS_SLOT_MAX))
		{
			metin2::log::error("EquipTo: invalid dragon soul cell (this: #{:d} {} wearflag: {:d} cell: {:d})", original_vnum(), name(), sub_type(), wear_cell - WEAR_MAX_NUM);
			return false;
		}
	}
	else
	{
		if (wear_cell >= WEAR_MAX_NUM)
		{
			metin2::log::error("EquipTo: invalid wear cell (this: #{:d} {} wearflag: {:d} cell: {:d})", original_vnum(), name(), wear_flag(), wear_cell);
			return false;
		}
	}

	if (ch->wear(wear_cell))
	{
		metin2::log::error("EquipTo: item already exist (this: #{:d} {} cell: {:d} {})", original_vnum(), name(), wear_cell, ch->wear(wear_cell)->name());
		return false;
	}

	if (owner())
		remove_from_character();

	ch->set_wear(wear_cell, this);

	owner_ = ch;
	equipped_ = true;
	cell_	= INVENTORY_MAX_NUM + wear_cell;


	uint32_t dwImmuneFlag = 0;
	item * item = nullptr;
	 
	for (int i = 0; i < WEAR_MAX_NUM; ++i)
	{
		if ((item = owner_->wear(i)))
		{
			if (item->immune_flag() != 0)
				set_bit(dwImmuneFlag, item->immune_flag());
			if (item->attribute_count() > 0)
			{
				if (item->has_attr(APPLY_IMMUNE_STUN))
					set_bit(dwImmuneFlag, IMMUNE_STUN);
				if (item->has_attr(APPLY_IMMUNE_SLOW))
					set_bit(dwImmuneFlag, IMMUNE_SLOW);
				if (item->has_attr(APPLY_IMMUNE_FALL))
					set_bit(dwImmuneFlag, IMMUNE_FALL);
			}
		}
	}
	 
	owner_->set_immune_flag(dwImmuneFlag);

	if (is_dragon_soul())
	{
		ds_manager::instance().activate_dragon_soul(this);
	}
	else
	{
		modify_points(true);	
		start_unique_expire_event();
		if (-1 != proto()->c_limit_timer_based_on_wear_index)
			start_timer_based_on_wear_expire_event();

		start_accessory_socket_expire_event();
	}

	ch->buff_on_attr_add_buffs_from_item(this);

	owner_->compute_battle_points();

	owner_->update_packet();

	save();

	return (true);
}

bool item::unequip()
{
	if (!owner_ || cell() < INVENTORY_MAX_NUM)
	{
		metin2::log::error("{} {:d} m_pOwner {}, GetCell {:d}", 
				name(), id(), static_cast<const void *>(owner_), cell());
		return false;
	}

	if (this != owner_->wear(cell() - INVENTORY_MAX_NUM))
	{
		metin2::log::error("m_pOwner->GetWear() != this");
		return false;
	}

	if (is_ride_item())
		clear_mount_attribute_and_affect();

	if (is_dragon_soul())
	{
		ds_manager::instance().deactivate_dragon_soul(this);
	}
	else
	{
		modify_points(false);
	}

	stop_unique_expire_event();

	if (-1 != proto()->c_limit_timer_based_on_wear_index)
		stop_timer_based_on_wear_expire_event();

	stop_accessory_socket_expire_event();


	owner_->buff_on_attr_remove_buffs_from_item(this);

	owner_->set_wear(cell() - INVENTORY_MAX_NUM, nullptr);


	uint32_t dwImmuneFlag = 0;
	item * item = nullptr;
	 
	for (int i = 0; i < WEAR_MAX_NUM; ++i)
	{
		if ((item = owner_->wear(i)))
		{
			if (item->immune_flag() != 0)
				set_bit(dwImmuneFlag, item->immune_flag());
			if (item->attribute_count() > 0)
			{
				if (item->has_attr(APPLY_IMMUNE_STUN))
					set_bit(dwImmuneFlag, IMMUNE_STUN);
				if (item->has_attr(APPLY_IMMUNE_SLOW))
					set_bit(dwImmuneFlag, IMMUNE_SLOW);
				if (item->has_attr(APPLY_IMMUNE_FALL))
					set_bit(dwImmuneFlag, IMMUNE_FALL);
			}
		}
	}
	 
	owner_->set_immune_flag(dwImmuneFlag);

	owner_->compute_battle_points();

	owner_->update_packet();

	owner_ = nullptr;
	cell_ = 0;
	equipped_	= false;

	return true;
}

int32_t item::value(uint32_t idx)
{
	assert(idx < ITEM_VALUES_MAX_NUM);
	return proto()->values[idx];
}

void item::set_exchanging(bool on)
{
	exchanging_ = on;
}

void item::save()
{
	if (skip_save_)
		return;

	item_manager::instance().delayed_save(this);
}

void item::set_sockets(const int32_t * al)
{
	std::memcpy(sockets_, al, sizeof(sockets_));
	save();
}

void item::set_socket(int i, int32_t v, bool log)
{
	assert(i < ITEM_SOCKET_MAX_NUM);
	sockets_[i] = v;
	update_packet();
	save();
	if (log)
		log_manager::instance().item_log(i, v, 0, id(), "SET_SOCKET", "", "", original_vnum());
}

int item::shop_buy_price()
{
	return proto()->shop_buy_price;
}

bool item::is_ownership(character * ch)
{
	if (!ownership_event_)
		return true;

	return ownership_pid_ == ch->player_id() ? true : false;
}

EVENTFUNC(ownership_event)
{
	item_event_info* info = dynamic_cast<item_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "ownership_event> <Factor> Null pointer" );
		return 0;
	}

	item * pkItem = info->item;

	pkItem->set_ownership_event(nullptr);

	packet_gc_item_ownership p;

	p.header	= HEADER_GC_ITEM_OWNERSHIP;
	p.vid	= pkItem->get_vid();
	p.name[0]	= '\0';

	pkItem->packet_around(&p, sizeof(p));
	return 0;
}

void item::set_ownership_event(event_ptr event)
{
	ownership_event_ = event;
}

void item::set_ownership(character * ch, int sec)
{
	if (!ch)
	{
		if (ownership_event_)
		{
			event_cancel(&ownership_event_);
			ownership_pid_ = 0;

			packet_gc_item_ownership p;

			p.header	= HEADER_GC_ITEM_OWNERSHIP;
			p.vid	= vid_;
			p.name[0]	= '\0';

			packet_around(&p, sizeof(p));
		}
		return;
	}

	if (ownership_event_)
		return;

	if (sec <= 10)
		sec = 30;

	ownership_pid_ = ch->player_id();

	item_event_info* info = alloc_event_info<item_event_info>();
	strlcpy(info->owner_name, ch->name(), sizeof(info->owner_name));
	info->item = this;

	set_ownership_event(event_create(ownership_event, info, passes_per_sec(sec)));

	packet_gc_item_ownership p;

	p.header = HEADER_GC_ITEM_OWNERSHIP;
	p.vid = vid_;
	strlcpy(p.name, ch->name(), sizeof(p.name));

	packet_around(&p, sizeof(p));
}

int item::socket_count()
{
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
	{
		if (get_socket(i) == 0)
			return i;
	}
	return ITEM_SOCKET_MAX_NUM;
}

bool item::add_socket()
{
	int count = socket_count();
	if (count == ITEM_SOCKET_MAX_NUM)
		return false;
	sockets_[count] = 1;
	return true;
}

void item::alter_to_socket_item(int socket_count)
{
	if (socket_count >= ITEM_SOCKET_MAX_NUM)
	{
		metin2::log::info( "Invalid Socket Count {:d}, set to maximum", ITEM_SOCKET_MAX_NUM);
		socket_count = ITEM_SOCKET_MAX_NUM;
	}

	for (int i = 0; i < socket_count; ++i)
		set_socket(i, 1);
}

void item::alter_to_magic_item()
{
	int idx = attribute_set_index();

	if (idx < 0)
		return;


	int iSecondPct;
	int iThirdPct;

	switch (type())
	{
		case ITEM_WEAPON:
			iSecondPct = 20;
			iThirdPct = 5;
			break;

		case ITEM_ARMOR:
		case ITEM_COSTUME:
			if (sub_type() == ARMOR_BODY)
			{
				iSecondPct = 10;
				iThirdPct = 2;
			}
			else
			{
				iSecondPct = 10;
				iThirdPct = 1;
			}
			break;

		default:
			return;
	}

	put_attribute(item_magic_attribute_percent_high);

	if (number(1, 100) <= iSecondPct)
		put_attribute(item_magic_attribute_percent_low);

	if (number(1, 100) <= iThirdPct)
		put_attribute(item_magic_attribute_percent_low);
}

uint32_t item::refine_from_vnum()
{
	return item_manager::instance().refine_from_vnum(vnum());
}

int item::refine_level()
{
	const char* name_value = base_name();
	char* p = const_cast<char*>(strrchr(name_value, '+'));

	if (!p)
		return 0;

	int	rtn = 0;
	str_to_number(rtn, p+1);

	const char* locale_name = name();
	p = const_cast<char*>(strrchr(locale_name, '+'));

	if (p)
	{
		int	locale_rtn = 0;
		str_to_number(locale_rtn, p+1);
		if (locale_rtn != rtn)
		{
			metin2::log::error("refine_level_based_on_NAME({:d}) is not equal to refine_level_based_on_LOCALE_NAME({:d}).", rtn, locale_rtn);
		}
	}

	return rtn;
}

EVENTFUNC(unique_expire_event)
{
	item_event_info* info = dynamic_cast<item_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "unique_expire_event> <Factor> Null pointer" );
		return 0;
	}

	item * pkItem = info->item;

	if (pkItem->value(2) == 0)
	{
		if (pkItem->get_socket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) <= 1)
		{
			metin2::log::info( "UNIQUE_ITEM: expire {} {:d}", pkItem->name(), pkItem->id());
			pkItem->set_unique_expire_event(nullptr);
			item_manager::instance().remove_item(pkItem, "UNIQUE_EXPIRE");
			return 0;
		}
		else
		{
			pkItem->set_socket(ITEM_SOCKET_UNIQUE_REMAIN_TIME, pkItem->get_socket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) - 1);
			return passes_per_sec(60);
		}
	}
	else
	{
		time_t cur = get_global_time();
		
		if (pkItem->get_socket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) <= cur)
		{
			pkItem->set_unique_expire_event(nullptr);
			item_manager::instance().remove_item(pkItem, "UNIQUE_EXPIRE");
			return 0;
		}
		else
		{
			if (pkItem->get_socket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) - cur < 600)
				return passes_per_sec(pkItem->get_socket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) - cur);
			else
				return passes_per_sec(600);
		}
	}
}

EVENTFUNC(timer_based_on_wear_expire_event)
{
	item_event_info* info = dynamic_cast<item_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "expire_event <Factor> Null pointer" );
		return 0;
	}

	item * pkItem = info->item;
	int remain_time = pkItem->get_socket(ITEM_SOCKET_REMAIN_SEC) - processing_time/metin2::config::server().passes_per_sec;
	if (remain_time <= 0)
	{
		metin2::log::info( "ITEM EXPIRED : expired {} {:d}", pkItem->name(), pkItem->id());
		pkItem->set_timer_based_on_wear_expire_event(nullptr);
		pkItem->set_socket(ITEM_SOCKET_REMAIN_SEC, 0);
	
		if (pkItem->is_dragon_soul())
		{
			ds_manager::instance().deactivate_dragon_soul(pkItem);
		}
		else
		{
			item_manager::instance().remove_item(pkItem, "TIMER_BASED_ON_WEAR_EXPIRE");
		}
		return 0;
	}
	pkItem->set_socket(ITEM_SOCKET_REMAIN_SEC, remain_time);
	return passes_per_sec (std::min(60, remain_time));
}

void item::set_unique_expire_event(event_ptr event)
{
	unique_expire_event_ = event;
}

void item::set_timer_based_on_wear_expire_event(event_ptr event)
{
	timer_based_on_wear_expire_event_ = event;
}

EVENTFUNC(real_time_expire_event)
{
	const item_vid_event_info* info = reinterpret_cast<const item_vid_event_info*>(event->info);

	if (nullptr == info)
		return 0;

	item * const item = item_manager::instance().find_by_vid( info->item_vid );

	if (nullptr == item)
		return 0;

	const time_t current = get_global_time();

	if (current > item->get_socket(0))
	{
		item_manager::instance().remove_item(item, "REAL_TIME_EXPIRE");

		return 0;
	}

	return passes_per_sec(1);
}

void item::start_real_time_expire_event()
{
	if (real_time_expire_event_)
		return;
	for (int i=0 ; i < ITEM_LIMIT_MAX_NUM ; i++)
	{
		if (LIMIT_REAL_TIME == proto()->limits[i].type || LIMIT_REAL_TIME_START_FIRST_USE == proto()->limits[i].type)
		{
			item_vid_event_info* info = alloc_event_info<item_vid_event_info>();
			info->item_vid = get_vid();

			real_time_expire_event_ = event_create( real_time_expire_event, info, passes_per_sec(1));

			metin2::log::info( "REAL_TIME_EXPIRE: StartRealTimeExpireEvent");

			return;
		}
	}
}

bool item::is_real_time_item()
{
	if(!proto())
		return false;
	for (int i=0 ; i < ITEM_LIMIT_MAX_NUM ; i++)
	{
		if (LIMIT_REAL_TIME == proto()->limits[i].type)
			return true;
	}
	return false;
}

void item::start_unique_expire_event()
{
	if (type() != ITEM_UNIQUE)
		return;

	if (unique_expire_event_)
		return;

	if (is_real_time_item())
		return;

	if (vnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
		owner_->show_alignment(false);

	int iSec = get_socket(ITEM_SOCKET_UNIQUE_SAVE_TIME);

	if (iSec == 0)
		iSec = 60;
	else
		iSec = std::min(iSec, 60);

	set_socket(ITEM_SOCKET_UNIQUE_SAVE_TIME, 0);

	item_event_info* info = alloc_event_info<item_event_info>();
	info->item = this;

	set_unique_expire_event(event_create(unique_expire_event, info, passes_per_sec(iSec)));
}

void item::start_timer_based_on_wear_expire_event()
{
	if (timer_based_on_wear_expire_event_)
		return;

	if (is_real_time_item())
		return;

	if (-1 == proto()->c_limit_timer_based_on_wear_index)
		return;

	int iSec = get_socket(0);
	
	if (0 != iSec)
	{
		iSec %= 60;
		if (0 == iSec)
			iSec = 60;
	}

	item_event_info* info = alloc_event_info<item_event_info>();
	info->item = this;

	set_timer_based_on_wear_expire_event(event_create(timer_based_on_wear_expire_event, info, passes_per_sec(iSec)));
}

void item::stop_unique_expire_event()
{
	if (!unique_expire_event_)
		return;

	if (value(2) != 0)
		return;

	if (vnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
		owner_->show_alignment(true);

	set_socket(ITEM_SOCKET_UNIQUE_SAVE_TIME, event_time(unique_expire_event_) / metin2::config::server().passes_per_sec);
	event_cancel(&unique_expire_event_);

	item_manager::instance().save_single_item(this);
}

void item::stop_timer_based_on_wear_expire_event()
{
	if (!timer_based_on_wear_expire_event_)
		return;

	int remain_time = get_socket(ITEM_SOCKET_REMAIN_SEC) - event_processing_time(timer_based_on_wear_expire_event_) / metin2::config::server().passes_per_sec;

	set_socket(ITEM_SOCKET_REMAIN_SEC, remain_time);
	event_cancel(&timer_based_on_wear_expire_event_);

	item_manager::instance().save_single_item(this);
}

void item::apply_addon(int addon_type)
{
	item_addon_manager::instance().apply_addon_to(addon_type, this);
}

int item::special_group() const
{ 
	return item_manager::instance().special_group_from_item(vnum()); 
}

bool item::is_accessory_for_socket()
{
	return (proto_->type == ITEM_ARMOR && (proto_->sub_type == ARMOR_WRIST || proto_->sub_type == ARMOR_NECK || proto_->sub_type == ARMOR_EAR)) ||
		(proto_->type == ITEM_BELT);
}

void item::set_accessory_socket_grade(int grade) 
{ 
	set_socket(0, std::clamp(grade, 0, accessory_socket_max_grade())); 

	int iDownTime = accessory_socket_degrade_time[accessory_socket_grade()];

	set_accessory_socket_down_grade_time(iDownTime);
}

void item::set_accessory_socket_max_grade(int max_grade) 
{ 
	set_socket(1, std::clamp<int>(max_grade, 0, ITEM_ACCESSORY_SOCKET_MAX_NUM)); 
}

void item::set_accessory_socket_down_grade_time(uint32_t time) 
{ 
	set_socket(2, time); 

	if (metin2::config::test_server() && owner())
		owner()->chat_packet(CHAT_TYPE_INFO, lc_text("%s에서 소켓 빠질때까지 남은 시간 %d"), name(), time);
}

EVENTFUNC(accessory_socket_expire_event)
{
	item_vid_event_info* info = dynamic_cast<item_vid_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "accessory_socket_expire_event> <Factor> Null pointer" );
		return 0;
	}

	item * item = item_manager::instance().find_by_vid(info->item_vid);

	if (item->accessory_socket_down_grade_time() <= 1)
	{
degrade:
		item->set_accessory_socket_expire_event(nullptr);
		item->accessory_socket_degrade();
		return 0;
	}
	else
	{
		int iTime = item->accessory_socket_down_grade_time() - 60;

		if (iTime <= 1)
			goto degrade;

		item->set_accessory_socket_down_grade_time(iTime);

		if (iTime > 60)
			return passes_per_sec(60);
		else
			return passes_per_sec(iTime);
	}
}

void item::start_accessory_socket_expire_event()
{
	if (!is_accessory_for_socket())
		return;

	if (accessory_socket_expire_event_)
		return;

	if (accessory_socket_max_grade() == 0)
		return;

	if (accessory_socket_grade() == 0)
		return;

	int iSec = accessory_socket_down_grade_time();
	set_accessory_socket_expire_event(nullptr);

	if (iSec <= 1)
		iSec = 5;
	else
		iSec = std::min(iSec, 60);

	item_vid_event_info* info = alloc_event_info<item_vid_event_info>();
	info->item_vid = get_vid();

	set_accessory_socket_expire_event(event_create(accessory_socket_expire_event, info, passes_per_sec(iSec)));
}

void item::stop_accessory_socket_expire_event()
{
	if (!accessory_socket_expire_event_)
		return;

	if (!is_accessory_for_socket())
		return;

	int new_time = accessory_socket_down_grade_time() - (60 - event_time(accessory_socket_expire_event_) / metin2::config::server().passes_per_sec);

	event_cancel(&accessory_socket_expire_event_);

	if (new_time <= 1)
	{
		accessory_socket_degrade();
	}
	else
	{
		set_accessory_socket_down_grade_time(new_time);
	}
}
		
bool item::is_ride_item()
{
	if (ITEM_UNIQUE == type() && UNIQUE_SPECIAL_RIDE == sub_type())
		return true;
	if (ITEM_UNIQUE == type() && UNIQUE_SPECIAL_MOUNT_RIDE == sub_type())
		return true;
	return false;
}
		

void item::clear_mount_attribute_and_affect()
{
	character * ch = owner();

	ch->remove_affect(AFFECT_MOUNT);
	ch->remove_affect(AFFECT_MOUNT_BONUS);

	ch->mount_vnum(0);

	ch->point_change(POINT_ST, 0);
	ch->point_change(POINT_DX, 0);
	ch->point_change(POINT_HT, 0);
	ch->point_change(POINT_IQ, 0);
}

void item::set_accessory_socket_expire_event(event_ptr event)
{
	accessory_socket_expire_event_ = event;
}

void item::accessory_socket_degrade()
{
	if (accessory_socket_grade() > 0)
	{
		character * ch = owner();

		if (ch)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s에 박혀있던 보석이 사라집니다."), name());
		}

		modify_points(false);
		set_accessory_socket_grade(accessory_socket_grade()-1);
		modify_points(true);

		int iDownTime = accessory_socket_degrade_time[accessory_socket_grade()];

		if (metin2::config::test_server())
			iDownTime /= 60;

		set_accessory_socket_down_grade_time(iDownTime);

		if (iDownTime)
			start_accessory_socket_expire_event();
	}
}

static bool can_put_into_ring(item * ring, item * item)
{
	return false;
}

bool item::can_put_into(item * item)
{
	if (item->type() == ITEM_BELT)
		return this->sub_type() == USE_PUT_INTO_BELT_SOCKET;

	else if(item->type() == ITEM_RING)
		return can_put_into_ring(item, this);

	else if (item->type() != ITEM_ARMOR)
		return false;

	uint32_t vnum_value = item->vnum();

	struct jewel_accessory_info
	{
		uint32_t jewel;
		uint32_t wrist;
		uint32_t neck;
		uint32_t ear;
	};
	const static jewel_accessory_info infos[] = { 
		{ 50634, 14420, 16220, 17220 }, 
		{ 50635, 14500, 16500, 17500 }, 
		{ 50636, 14520, 16520, 17520 }, 
		{ 50637, 14540, 16540, 17540 }, 
		{ 50638, 14560, 16560, 17560 }, 
	};
	
	uint32_t item_type = (item->vnum() / 10) * 10;
	for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++)
	{
		const jewel_accessory_info& info = infos[i];
		switch(item->sub_type())
		{
		case ARMOR_WRIST:
			if (info.wrist == item_type)
			{
				if (info.jewel == vnum())
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			break;
		case ARMOR_NECK:
			if (info.neck == item_type)
			{
				if (info.jewel == vnum())
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			break;
		case ARMOR_EAR:
			if (info.ear == item_type)
			{
				if (info.jewel == vnum())
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			break;
		}
	}
	if (item->sub_type() == ARMOR_WRIST)
		vnum_value -= 14000;
	else if (item->sub_type() == ARMOR_NECK)
		vnum_value -= 16000;
	else if (item->sub_type() == ARMOR_EAR)
		vnum_value -= 17000;
	else
		return false;

	uint32_t type = vnum_value / 20;

	if (type > 11)
	{
		type = (vnum_value - 170) / 20;

		if (50623 + type != vnum())
			return false;
		else
			return true;
	}
	else if (item->vnum() >= 16210 && item->vnum() <= 16219)
	{
		if (50625 != vnum())
			return false;
		else
			return true;
	}
	else if (item->vnum() >= 16230 && item->vnum() <= 16239)
	{
		if (50626 != vnum())
			return false;
		else
			return true;
	}

	return 50623 + type == vnum();
}

bool item::is_pc_bang_item()
{
	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
	{
		if (proto_->limits[i].type == LIMIT_PCBANG)
			return true;
	}
	return false;
}

bool item::check_item_use_level(int level)
{
	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
	{
		if (this->proto_->limits[i].type == LIMIT_LEVEL)
		{
			if (this->proto_->limits[i].value > level) return false;
			else return true;
		}
	}
	return true;
}

void item::copy_socket_to(item * item_value)
{
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
	{
		item_value->sockets_[i] = sockets_[i];
	}
}

int item::accessory_socket_grade()
{
   	return std::clamp(get_socket(0), 0, accessory_socket_max_grade()); 
}

int item::accessory_socket_max_grade()
{
   	return std::clamp<int>(get_socket(1), 0, ITEM_ACCESSORY_SOCKET_MAX_NUM);
}

int item::accessory_socket_down_grade_time()
{
	return std::clamp(get_socket(2), 0, accessory_socket_degrade_time[accessory_socket_grade()]);
}

void item::attr_log()
{
	const char * pszIP = nullptr;

	if (owner() && owner()->get_desc())
		pszIP = owner()->get_desc()->host_name();

	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
	{
		if (sockets_[i])
		{
			log_manager::instance().item_log(i, sockets_[i], 0, id(), "INFO_SOCKET", "", pszIP ? pszIP : "", original_vnum());
		}
	}

	for (int i = 0; i<ITEM_ATTRIBUTE_MAX_NUM; ++i)
	{
		int	type	= attr_[i].type;
		int value	= attr_[i].s_value;

		if (type)
			log_manager::instance().item_log(i, type, value, id(), "INFO_ATTR", "", pszIP ? pszIP : "", original_vnum());
	}
}

int item::level_limit()
{
	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
	{
		if (this->proto_->limits[i].type == LIMIT_LEVEL)
		{
			return this->proto_->limits[i].value;
		}
	}
	return 0;
}

bool item::on_after_created_item()
{
	if (-1 != this->proto()->c_limit_real_time_first_use_index)
	{
		if (0 != get_socket(1))
		{
			start_real_time_expire_event();
		}
	}

	return true;
}



bool item::is_dragon_soul()
{
	return type() == ITEM_DS;
}

int item::give_more_time_per(float percent)
{
	if (is_dragon_soul())
	{
		uint32_t duration = ds_manager::instance().duration(this);
		int remain_sec = get_socket(ITEM_SOCKET_REMAIN_SEC);
		int given_time = percent * duration / 100;
		if (static_cast<uint32_t>(remain_sec) == duration)
			return false;
		if (static_cast<uint32_t>(given_time + remain_sec) >= duration)
		{
			set_socket(ITEM_SOCKET_REMAIN_SEC, duration);
			return duration - remain_sec;
		}
		else
		{
			set_socket(ITEM_SOCKET_REMAIN_SEC, given_time + remain_sec);
			return given_time;
		}
	}
	else
		return 0;
}

int item::give_more_time_fix(uint32_t time)
{
	if (is_dragon_soul())
	{
		uint32_t duration = ds_manager::instance().duration(this);
		int remain_sec = get_socket(ITEM_SOCKET_REMAIN_SEC);
		if (static_cast<uint32_t>(remain_sec) == duration)
			return false;
		if ((time + remain_sec) >= duration)
		{
			set_socket(ITEM_SOCKET_REMAIN_SEC, duration);
			return duration - remain_sec; 
		}
		else
		{
			set_socket(ITEM_SOCKET_REMAIN_SEC, time + remain_sec);
			return time;
		}
	}
	else
		return 0;
}


int	item::duration()
{
	if(!proto())
		return -1;

	for (int i=0 ; i < ITEM_LIMIT_MAX_NUM ; i++)
	{
		if (LIMIT_REAL_TIME == proto()->limits[i].type)
			return proto()->limits[i].value;
	}
	
	if (-1 != proto()->c_limit_timer_based_on_wear_index)
		return proto()->limits[static_cast<int>(proto()->c_limit_timer_based_on_wear_index)].value;	

	return -1;
}

bool item::is_same_special_group(item * const item) const
{
	if (this->vnum() == item->vnum())
		return true;

	if (special_group() && (item->special_group() == special_group()))
		return true;

	return false;
}
}
