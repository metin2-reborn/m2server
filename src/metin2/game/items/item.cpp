#include <metin2/config/config.h>
#include <metin2/game/items/item.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/buff_on_attributes.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/items/dragon_soul.h>
#include <metin2/game/items/item_addon.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/world/entity.h>
#include <metin2/game/world/sectree.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/common/utils.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/apply_types.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/mob_types.h>
#include <metin2/proto/tables.h>
#include <metin2/proto/vnum_helper.h>

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
          mask_vnum_(0), sig_vnum_(0) {
        memset(&sockets_, 0, sizeof(sockets_));
        memset(&attr_, 0, sizeof(attr_));
    }

    item::~item() {
        destroy();
    }

    void item::initialize() {
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

    void item::destroy() {
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

    EVENTFUNC(item_destroy_event) {
        auto *info = dynamic_cast<item_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Item destroy event has no info");
            return 0;
        }

        item *item_value = info->item;

        if (item_value->owner())
            metin2::log::error("Destroying item {} still owned by player {}", item_value->name(), item_value->owner()->name());

        item_value->set_destroy_event(nullptr);
        get_item_manager().destroy_item(item_value);
        return 0;
    }

    void item::set_destroy_event(event_ptr event) {
        destroy_event_ = event;
    }

    void item::start_destroy_event(int sec) {
        if (destroy_event_)
            return;

        auto *info = alloc_event_info<item_event_info>();
        info->item = this;

        set_destroy_event(event_create(item_destroy_event, info, passes_per_sec(sec)));
    }

    void item::encode_insert_packet(entity *ent) {
        desc *d;

        if (!(d = ent->get_desc()))
            return;

        const pixel_position &c_pos = xyz();

        struct packet_gc_item_ground_add pack;

        pack.header = HEADER_GC_ITEM_GROUND_ADD;
        pack.x = c_pos.x;
        pack.y = c_pos.y;
        pack.z = c_pos.z;
        pack.vnum = vnum();
        pack.vid = vid_;

        d->packet(&pack, sizeof(pack));

        if (ownership_event_ != nullptr) {
            auto *info = dynamic_cast<item_event_info *>(ownership_event_->info.get());

            if (info == nullptr) {
                metin2::log::error("Item ownership event has no info");
                return;
            }

            packet_gc_item_ownership p;

            p.header = HEADER_GC_ITEM_OWNERSHIP;
            p.vid = vid_;
            strlcpy(p.name, info->owner_name, sizeof(p.name));

            d->packet(&p, sizeof(packet_gc_item_ownership));
        }
    }

    void item::encode_remove_packet(entity *ent) {
        desc *d;

        if (!(d = ent->get_desc()))
            return;

        struct packet_gc_item_ground_del pack;

        pack.header = HEADER_GC_ITEM_GROUND_DEL;
        pack.vid = vid_;

        d->packet(&pack, sizeof(pack));
        metin2::log::debug("Item: sent removal of {} to {}", name(), ((character *)ent)->name());
    }

    void item::set_proto(const item_table *table) {
        assert(table != nullptr);
        proto_ = table;
        set_flag(proto_->flags);
    }

    void item::update_packet() {
        if (!owner_ || !owner_->get_desc())
            return;

        packet_gc_item_update pack;

        pack.header = HEADER_GC_ITEM_UPDATE;
        pack.cell = item_pos(window(), cell_);
        pack.count = count_;

        for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
            pack.sockets[i] = sockets_[i];

        std::memcpy(pack.attr, attributes(), sizeof(pack.attr));

        metin2::log::debug("Item: sent update of {} to {}", name(), owner_->name());
        owner_->get_desc()->packet(&pack, sizeof(pack));
    }

    auto item::count() -> uint32_t {
        if (type() == ITEM_ELK)
            return std::min<int>(count_, INT_MAX);
        else {
            return std::min<int>(count_, 200);
        }
    }

    auto item::set_count(uint32_t count) -> bool {
        if (type() == ITEM_ELK) {
            count_ = std::min<int>(count, INT_MAX);
        } else {
            count_ = std::min<int>(count, ITEM_MAX_COUNT);
        }

        if (count == 0 && owner_) {
            if (sub_type() == USE_ABILITY_UP || sub_type() == USE_POTION || vnum() == 70020) {
                character *owner_value = owner();
                uint16_t cell_value = cell();

                remove_from_character();

                if (!is_dragon_soul()) {
                    item *item = owner_value->find_specify_item(vnum());

                    if (nullptr != item) {
                        owner_value->chain_quickslot_item(item, QUICKSLOT_TYPE_ITEM, cell_value);
                    } else {
                        owner_value->sync_quickslot(QUICKSLOT_TYPE_ITEM, cell_value, 255);
                    }
                }

                get_item_manager().destroy_item(this);
            } else {
                if (!is_dragon_soul()) {
                    owner_->sync_quickslot(QUICKSLOT_TYPE_ITEM, cell_, 255);
                }
                get_item_manager().destroy_item(remove_from_character());
            }

            return false;
        }

        update_packet();

        save();
        return true;
    }

    auto item::remove_from_character() -> item * {
        if (!owner_) {
            metin2::log::error("Item {} (id {:d}) has no owner to remove from", name(), id());
            return (this);
        }

        character *owner = owner_;

        if (equipped_) {
            unequip();

            set_window(RESERVED_WINDOW);
            save();
            return (this);
        } else {
            if (window() != SAFEBOX && window() != MALL) {
                if (is_dragon_soul()) {
                    if (cell_ >= DRAGON_SOUL_INVENTORY_MAX_NUM)
                        metin2::log::error("Dragon soul item {} (id {:d}) has cell {:d} beyond the inventory", name(), id(), cell_);
                    else
                        owner->set_item(item_pos(window_, cell_), nullptr);
                } else {
                    item_pos cell(INVENTORY, cell_);

                    if (false == cell.is_default_inventory_position() && false == cell.is_belt_inventory_position())
                        metin2::log::error("Item {} (id {:d}) has invalid inventory cell {:d}", name(), id(), cell_);
                    else {
                        owner->set_item(cell, nullptr);
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

    auto item::add_to_character(character *ch, item_pos cell_value) -> bool {
        assert(get_sectree() == nullptr);
        assert(owner_ == nullptr);
        uint16_t pos = cell_value.cell;
        uint8_t window_type = cell_value.window_type;

        if (INVENTORY == window_type) {
            if (cell_ >= INVENTORY_MAX_NUM && BELT_INVENTORY_SLOT_START > cell_) {
                metin2::log::error("Cannot add item {} to player {}: cell {:d} is out of range", proto_->name, ch->name(), cell_);
                return false;
            }
        } else if (DRAGON_SOUL_INVENTORY == window_type) {
            if (cell_ >= DRAGON_SOUL_INVENTORY_MAX_NUM) {
                metin2::log::error("Cannot add item {} to player {}: cell {:d} is out of range", proto_->name, ch->name(), cell_);
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

    auto item::remove_from_ground() -> item * {
        if (get_sectree()) {
            set_ownership(nullptr);

            get_sectree()->remove_entity(this);

            view_cleanup();

            save();
        }

        return (this);
    }

    auto item::add_to_ground(int32_t map_index, const pixel_position &pos, bool skip_owner_check) -> bool {
        if (0 == map_index) {
            metin2::log::error("Cannot place item {} (id {:d}) on map index 0", name(), id());
            return false;
        }

        if (get_sectree()) {
            metin2::log::error("Item {} (id {:d}) is already on the ground", name(), id());
            return false;
        }

        if (!skip_owner_check && owner_) {
            metin2::log::error("Item {} (id {:d}) still has an owner", name(), id());
            return false;
        }

        sectree *tree = get_sectree_manager().get(map_index, pos.x, pos.y);

        if (!tree) {
            metin2::log::error("No sectree at ({:d}, {:d}) on map {:d}", pos.x, pos.y, map_index);
            return false;
        }

        set_window(GROUND);
        set_xyz(pos.x, pos.y, pos.z);
        tree->insert_entity(this);
        update_sectree();
        save();
        return true;
    }

    auto item::distance_valid(character *ch) -> bool {
        if (!get_sectree())
            return false;

        int dist = distance_approx(x() - ch->x(), y() - ch->y());

        if (dist > 300)
            return false;

        return true;
    }

    auto item::can_used_by(character *ch) -> bool {
        switch (ch->job()) {
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

    auto item::find_equip_cell(character *ch, int candidate_cell) -> int {
        if ((0 == wear_flag() || ITEM_TOTEM == type()) && ITEM_COSTUME != type() && ITEM_DS != type() && ITEM_SPECIAL_DS != type() && ITEM_RING != type() && ITEM_BELT != type())
            return -1;

        if (type() == ITEM_DS || type() == ITEM_SPECIAL_DS) {
            if (candidate_cell < 0) {
                return WEAR_MAX_NUM + sub_type();
            } else {
                for (int i = 0; i < DRAGON_SOUL_DECK_MAX_NUM; i++) {
                    if (WEAR_MAX_NUM + i * DS_SLOT_MAX + sub_type() == candidate_cell) {
                        return candidate_cell;
                    }
                }
                return -1;
            }
        } else if (type() == ITEM_COSTUME) {
            if (sub_type() == COSTUME_BODY)
                return WEAR_COSTUME_BODY;
            else if (sub_type() == COSTUME_HAIR)
                return WEAR_COSTUME_HAIR;
        } else if (type() == ITEM_RING) {
            if (ch->wear(WEAR_RING1))
                return WEAR_RING2;
            else
                return WEAR_RING1;
        } else if (type() == ITEM_BELT)
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
        else if (wear_flag() & WEARABLE_UNIQUE) {
            if (ch->wear(WEAR_UNIQUE1))
                return WEAR_UNIQUE2;
            else
                return WEAR_UNIQUE1;
        }

        else if (wear_flag() & WEARABLE_ABILITY) {
            if (!ch->wear(WEAR_ABILITY1)) {
                return WEAR_ABILITY1;
            } else if (!ch->wear(WEAR_ABILITY2)) {
                return WEAR_ABILITY2;
            } else if (!ch->wear(WEAR_ABILITY3)) {
                return WEAR_ABILITY3;
            } else if (!ch->wear(WEAR_ABILITY4)) {
                return WEAR_ABILITY4;
            } else if (!ch->wear(WEAR_ABILITY5)) {
                return WEAR_ABILITY5;
            } else if (!ch->wear(WEAR_ABILITY6)) {
                return WEAR_ABILITY6;
            } else if (!ch->wear(WEAR_ABILITY7)) {
                return WEAR_ABILITY7;
            } else if (!ch->wear(WEAR_ABILITY8)) {
                return WEAR_ABILITY8;
            } else {
                return -1;
            }
        }
        return -1;
    }

    void item::modify_points(bool add) {
        int accessory_grade;

        if (false == is_accessory_for_socket()) {
            if (proto_->type == ITEM_WEAPON || proto_->type == ITEM_ARMOR) {
                for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i) {
                    uint32_t vnum_value;

                    if ((vnum_value = get_socket(i)) <= 2)
                        continue;

                    item_table *p = get_item_manager().table(vnum_value);

                    if (!p) {
                        metin2::log::error("No item proto for socketed vnum {:d} in item {} (id {:d})", vnum_value, name(), id());
                        continue;
                    }

                    if (ITEM_METIN == p->type) {
                        for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i) {
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

            accessory_grade = 0;
        } else {
            accessory_grade = std::min<int>(accessory_socket_grade(), ITEM_ACCESSORY_SOCKET_MAX_NUM);
        }

        for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i) {
            if (proto_->applies[i].type == APPLY_NONE)
                continue;

            int32_t value = proto_->applies[i].value;

            if (proto_->applies[i].type == APPLY_SKILL) {
                owner_->apply_point(proto_->applies[i].type, add ? value : value ^ 0x00800000);
            } else {
                if (0 != accessory_grade)
                    value += std::max(accessory_grade, value * accessory_socket_effective_pct[accessory_grade] / 100);

                owner_->apply_point(proto_->applies[i].type, add ? value : -value);
            }
        }
        if (item_vnum::is_ramadan_moon_ring(vnum()) || item_vnum::is_halloween_candy(vnum()) || item_vnum::is_happiness_ring(vnum()) || item_vnum::is_love_pendant(vnum())) {
        } else {
            for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i) {
                if (attribute_type(i)) {
                    const player_item_attribute &ia = get_attribute(i);

                    if (ia.type == APPLY_SKILL)
                        owner_->apply_point(ia.type, add ? ia.s_value : ia.s_value ^ 0x00800000);
                    else
                        owner_->apply_point(ia.type, add ? ia.s_value : -ia.s_value);
                }
            }
        }

        switch (proto_->type) {
        case ITEM_PICK:
        case ITEM_ROD:
            {
                if (add) {
                    if (cell_ == INVENTORY_MAX_NUM + WEAR_WEAPON)
                        owner_->set_part(PART_WEAPON, vnum());
                } else {
                    if (cell_ == INVENTORY_MAX_NUM + WEAR_WEAPON)
                        owner_->set_part(PART_WEAPON, owner_->original_part(PART_WEAPON));
                }
            }
            break;

        case ITEM_WEAPON:
            {
                if (add) {
                    if (cell_ == INVENTORY_MAX_NUM + WEAR_WEAPON)
                        owner_->set_part(PART_WEAPON, vnum());
                } else {
                    if (cell_ == INVENTORY_MAX_NUM + WEAR_WEAPON)
                        owner_->set_part(PART_WEAPON, owner_->original_part(PART_WEAPON));
                }
            }
            break;

        case ITEM_ARMOR:
            {
                if (nullptr != owner_->wear(WEAR_COSTUME_BODY))
                    break;

                if (sub_type() == ARMOR_BODY || sub_type() == ARMOR_HEAD || sub_type() == ARMOR_FOOTS || sub_type() == ARMOR_SHIELD) {
                    if (add) {
                        if (proto()->sub_type == ARMOR_BODY)
                            owner_->set_part(PART_MAIN, vnum());
                    } else {
                        if (proto()->sub_type == ARMOR_BODY)
                            owner_->set_part(PART_MAIN, owner_->original_part(PART_MAIN));
                    }
                }
            }
            break;

        case ITEM_COSTUME:
            {
                uint32_t to_set_value = this->vnum();
                parts to_set_part = PART_MAX_NUM;

                if (sub_type() == COSTUME_BODY) {
                    to_set_part = PART_MAIN;

                    if (false == add) {
                        const item *armor = owner_->wear(WEAR_BODY);
                        to_set_value = (nullptr != armor) ? armor->vnum() : owner_->original_part(PART_MAIN);
                    }

                }

                else if (sub_type() == COSTUME_HAIR) {
                    to_set_part = PART_HAIR;

                    to_set_value = (true == add) ? this->value(3) : 0;
                }

                if (PART_MAX_NUM != to_set_part) {
                    owner_->set_part((uint8_t)to_set_part, to_set_value);
                    owner_->update_packet();
                }
            }
            break;
        case ITEM_UNIQUE:
            {
                if (0 != sig_vnum()) {
                    const special_item_group *item_group = get_item_manager().get_special_item_group(sig_vnum());
                    if (nullptr == item_group)
                        break;
                    uint32_t attr_vnum = item_group->attr_vnum(vnum());
                    const special_attr_group *attr_group = get_item_manager().get_special_attr_group(attr_vnum);
                    if (nullptr == attr_group)
                        break;
                    for (auto it = attr_group->attrs.begin(); it != attr_group->attrs.end(); it++) {
                        owner_->apply_point(it->apply_type, add ? it->apply_value : -it->apply_value);
                    }
                }
            }
            break;
        }
    }

    auto item::is_equipable() const -> bool {
        switch (this->type()) {
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

    auto item::equip_to(character *ch, uint8_t wear_cell) -> bool {
        if (!ch) {
            metin2::log::error("Cannot equip item {} (id {:d}): no character", name(), id());
            return false;
        }

        if (is_dragon_soul()) {
            if (wear_cell < WEAR_MAX_NUM || wear_cell >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * static_cast<int>(DS_SLOT_MAX)) {
                metin2::log::error("Cannot equip dragon soul {1} (vnum {0:d}, sub type {2:d}) in deck cell {3:d}", original_vnum(), name(), sub_type(), wear_cell - WEAR_MAX_NUM);
                return false;
            }
        } else {
            if (wear_cell >= WEAR_MAX_NUM) {
                metin2::log::error("Cannot equip item {1} (vnum {0:d}, wear flag {2:d}) in invalid cell {3:d}", original_vnum(), name(), wear_flag(), wear_cell);
                return false;
            }
        }

        if (ch->wear(wear_cell)) {
            metin2::log::error("Cannot equip item {1} (vnum {0:d}) in cell {2:d}: {3} is already there", original_vnum(), name(), wear_cell, ch->wear(wear_cell)->name());
            return false;
        }

        if (owner())
            remove_from_character();

        ch->set_wear(wear_cell, this);

        owner_ = ch;
        equipped_ = true;
        cell_ = INVENTORY_MAX_NUM + wear_cell;

        uint32_t immune_flag = 0;
        item *item = nullptr;

        for (int i = 0; i < WEAR_MAX_NUM; ++i) {
            if ((item = owner_->wear(i))) {
                if (item->immune_flag() != 0)
                    set_bit(immune_flag, item->immune_flag());
                if (item->attribute_count() > 0) {
                    if (item->has_attr(APPLY_IMMUNE_STUN))
                        set_bit(immune_flag, IMMUNE_STUN);
                    if (item->has_attr(APPLY_IMMUNE_SLOW))
                        set_bit(immune_flag, IMMUNE_SLOW);
                    if (item->has_attr(APPLY_IMMUNE_FALL))
                        set_bit(immune_flag, IMMUNE_FALL);
                }
            }
        }

        owner_->set_immune_flag(immune_flag);

        if (is_dragon_soul()) {
            get_ds_manager().activate_dragon_soul(this);
        } else {
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

    auto item::unequip() -> bool {
        if (!owner_ || cell() < INVENTORY_MAX_NUM) {
            metin2::log::error("Cannot unequip item {} (id {:d}): owner {}, cell {:d}",
                name(), id(), static_cast<const void *>(owner_), cell());
            return false;
        }

        if (this != owner_->wear(cell() - INVENTORY_MAX_NUM)) {
            metin2::log::error("Cannot unequip item {} (id {:d}): not worn in its cell", name(), id());
            return false;
        }

        if (is_ride_item())
            clear_mount_attribute_and_affect();

        if (is_dragon_soul()) {
            get_ds_manager().deactivate_dragon_soul(this);
        } else {
            modify_points(false);
        }

        stop_unique_expire_event();

        if (-1 != proto()->c_limit_timer_based_on_wear_index)
            stop_timer_based_on_wear_expire_event();

        stop_accessory_socket_expire_event();

        owner_->buff_on_attr_remove_buffs_from_item(this);

        owner_->set_wear(cell() - INVENTORY_MAX_NUM, nullptr);

        uint32_t immune_flag = 0;
        item *item = nullptr;

        for (int i = 0; i < WEAR_MAX_NUM; ++i) {
            if ((item = owner_->wear(i))) {
                if (item->immune_flag() != 0)
                    set_bit(immune_flag, item->immune_flag());
                if (item->attribute_count() > 0) {
                    if (item->has_attr(APPLY_IMMUNE_STUN))
                        set_bit(immune_flag, IMMUNE_STUN);
                    if (item->has_attr(APPLY_IMMUNE_SLOW))
                        set_bit(immune_flag, IMMUNE_SLOW);
                    if (item->has_attr(APPLY_IMMUNE_FALL))
                        set_bit(immune_flag, IMMUNE_FALL);
                }
            }
        }

        owner_->set_immune_flag(immune_flag);

        owner_->compute_battle_points();

        owner_->update_packet();

        owner_ = nullptr;
        cell_ = 0;
        equipped_ = false;

        return true;
    }

    auto item::value(uint32_t idx) -> int32_t {
        assert(idx < ITEM_VALUES_MAX_NUM);
        return proto()->values[idx];
    }

    void item::set_exchanging(bool on) {
        exchanging_ = on;
    }

    void item::save() {
        if (skip_save_)
            return;

        get_item_manager().delayed_save(this);
    }

    void item::set_sockets(const int32_t *al) {
        std::memcpy(sockets_, al, sizeof(sockets_));
        save();
    }

    void item::set_socket(int i, int32_t v, bool log) {
        assert(i < ITEM_SOCKET_MAX_NUM);
        sockets_[i] = v;
        update_packet();
        save();
        if (log)
            get_log_manager().item_log(i, v, 0, id(), "SET_SOCKET", "", "", original_vnum());
    }

    auto item::shop_buy_price() -> int {
        return proto()->shop_buy_price;
    }

    auto item::is_ownership(character *ch) -> bool {
        if (!ownership_event_)
            return true;

        return ownership_pid_ == ch->player_id() ? true : false;
    }

    EVENTFUNC(ownership_event) {
        auto *info = dynamic_cast<item_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Item ownership event has no info");
            return 0;
        }

        item *item = info->item;

        item->set_ownership_event(nullptr);

        packet_gc_item_ownership p;

        p.header = HEADER_GC_ITEM_OWNERSHIP;
        p.vid = item->get_vid();
        p.name[0] = '\0';

        item->packet_around(&p, sizeof(p));
        return 0;
    }

    void item::set_ownership_event(event_ptr event) {
        ownership_event_ = event;
    }

    void item::set_ownership(character *ch, int sec) {
        if (!ch) {
            if (ownership_event_) {
                event_cancel(&ownership_event_);
                ownership_pid_ = 0;

                packet_gc_item_ownership p;

                p.header = HEADER_GC_ITEM_OWNERSHIP;
                p.vid = vid_;
                p.name[0] = '\0';

                packet_around(&p, sizeof(p));
            }
            return;
        }

        if (ownership_event_)
            return;

        if (sec <= 10)
            sec = 30;

        ownership_pid_ = ch->player_id();

        auto *info = alloc_event_info<item_event_info>();
        strlcpy(info->owner_name, ch->name(), sizeof(info->owner_name));
        info->item = this;

        set_ownership_event(event_create(ownership_event, info, passes_per_sec(sec)));

        packet_gc_item_ownership p;

        p.header = HEADER_GC_ITEM_OWNERSHIP;
        p.vid = vid_;
        strlcpy(p.name, ch->name(), sizeof(p.name));

        packet_around(&p, sizeof(p));
    }

    auto item::socket_count() -> int {
        for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++) {
            if (get_socket(i) == 0)
                return i;
        }
        return ITEM_SOCKET_MAX_NUM;
    }

    auto item::add_socket() -> bool {
        int count = socket_count();
        if (count == ITEM_SOCKET_MAX_NUM)
            return false;
        sockets_[count] = 1;
        return true;
    }

    void item::alter_to_socket_item(int socket_count) {
        if (socket_count >= ITEM_SOCKET_MAX_NUM) {
            metin2::log::warn("Socket count {:d} capped at {:d}", socket_count, ITEM_SOCKET_MAX_NUM);
            socket_count = ITEM_SOCKET_MAX_NUM;
        }

        for (int i = 0; i < socket_count; ++i)
            set_socket(i, 1);
    }

    void item::alter_to_magic_item() {
        int idx = attribute_set_index();

        if (idx < 0)
            return;

        int second_pct;
        int third_pct;

        switch (type()) {
        case ITEM_WEAPON:
            second_pct = 20;
            third_pct = 5;
            break;

        case ITEM_ARMOR:
        case ITEM_COSTUME:
            if (sub_type() == ARMOR_BODY) {
                second_pct = 10;
                third_pct = 2;
            } else {
                second_pct = 10;
                third_pct = 1;
            }
            break;

        default:
            return;
        }

        put_attribute(item_magic_attribute_percent_high);

        if (prob(second_pct))
            put_attribute(item_magic_attribute_percent_low);

        if (prob(third_pct))
            put_attribute(item_magic_attribute_percent_low);
    }

    auto item::refine_from_vnum() -> uint32_t {
        return get_item_manager().refine_from_vnum(vnum());
    }

    auto item::refine_level() -> int {
        const char *name_value = base_name();
        char *p = const_cast<char *>(strrchr(name_value, '+'));

        if (!p)
            return 0;

        int rtn = 0;
        str_to_number(rtn, p + 1);

        const char *locale_name = name();
        p = const_cast<char *>(strrchr(locale_name, '+'));

        if (p) {
            int locale_rtn = 0;
            str_to_number(locale_rtn, p + 1);
            if (locale_rtn != rtn) {
                metin2::log::error("Refine level {:d} from proto name differs from {:d} in locale name", rtn, locale_rtn);
            }
        }

        return rtn;
    }

    EVENTFUNC(unique_expire_event) {
        auto *info = dynamic_cast<item_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Unique item expire event has no info");
            return 0;
        }

        item *item_value = info->item;

        if (item_value->value(2) == 0) {
            if (item_value->get_socket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) <= 1) {
                metin2::log::info("Item: unique item {} (id {:d}) expired", item_value->name(), item_value->id());
                item_value->set_unique_expire_event(nullptr);
                get_item_manager().remove_item(item_value, "UNIQUE_EXPIRE");
                return 0;
            } else {
                item_value->set_socket(ITEM_SOCKET_UNIQUE_REMAIN_TIME, item_value->get_socket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) - 1);
                return passes_per_sec(60);
            }
        } else {
            time_t cur = get_global_time();

            if (item_value->get_socket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) <= cur) {
                item_value->set_unique_expire_event(nullptr);
                get_item_manager().remove_item(item_value, "UNIQUE_EXPIRE");
                return 0;
            } else {
                if (item_value->get_socket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) - cur < 600)
                    return passes_per_sec(item_value->get_socket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) - cur);
                else
                    return passes_per_sec(600);
            }
        }
    }

    EVENTFUNC(timer_based_on_wear_expire_event) {
        auto *info = dynamic_cast<item_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Item expire event has no info");
            return 0;
        }

        item *item = info->item;
        int remain_time = item->get_socket(ITEM_SOCKET_REMAIN_SEC) - processing_time / metin2::config::server().passes_per_sec;
        if (remain_time <= 0) {
            metin2::log::info("Item: {} (id {:d}) expired", item->name(), item->id());
            item->set_timer_based_on_wear_expire_event(nullptr);
            item->set_socket(ITEM_SOCKET_REMAIN_SEC, 0);

            if (item->is_dragon_soul()) {
                get_ds_manager().deactivate_dragon_soul(item);
            } else {
                get_item_manager().remove_item(item, "TIMER_BASED_ON_WEAR_EXPIRE");
            }
            return 0;
        }
        item->set_socket(ITEM_SOCKET_REMAIN_SEC, remain_time);
        return passes_per_sec(std::min(60, remain_time));
    }

    void item::set_unique_expire_event(event_ptr event) {
        unique_expire_event_ = event;
    }

    void item::set_timer_based_on_wear_expire_event(event_ptr event) {
        timer_based_on_wear_expire_event_ = event;
    }

    EVENTFUNC(real_time_expire_event) {
        const auto *info = dynamic_cast<const item_vid_event_info *>(event->info.get());

        if (nullptr == info)
            return 0;

        item *const item = get_item_manager().find_by_vid(info->item_vid);

        if (nullptr == item)
            return 0;

        const time_t current = get_global_time();

        if (current > item->get_socket(0)) {
            get_item_manager().remove_item(item, "REAL_TIME_EXPIRE");

            return 0;
        }

        return passes_per_sec(1);
    }

    void item::start_real_time_expire_event() {
        if (real_time_expire_event_)
            return;
        for (int i = 0; i < ITEM_LIMIT_MAX_NUM; i++) {
            if (LIMIT_REAL_TIME == proto()->limits[i].type || LIMIT_REAL_TIME_START_FIRST_USE == proto()->limits[i].type) {
                auto *info = alloc_event_info<item_vid_event_info>();
                info->item_vid = get_vid();

                real_time_expire_event_ = event_create(real_time_expire_event, info, passes_per_sec(1));

                metin2::log::debug("Item: started real-time expiry for {} (id {:d})", name(), id());

                return;
            }
        }
    }

    auto item::is_real_time_item() -> bool {
        if (!proto())
            return false;
        for (int i = 0; i < ITEM_LIMIT_MAX_NUM; i++) {
            if (LIMIT_REAL_TIME == proto()->limits[i].type)
                return true;
        }
        return false;
    }

    void item::start_unique_expire_event() {
        if (type() != ITEM_UNIQUE)
            return;

        if (unique_expire_event_)
            return;

        if (is_real_time_item())
            return;

        if (vnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
            owner_->show_alignment(false);

        int sec = get_socket(ITEM_SOCKET_UNIQUE_SAVE_TIME);

        if (sec == 0)
            sec = 60;
        else
            sec = std::min(sec, 60);

        set_socket(ITEM_SOCKET_UNIQUE_SAVE_TIME, 0);

        auto *info = alloc_event_info<item_event_info>();
        info->item = this;

        set_unique_expire_event(event_create(unique_expire_event, info, passes_per_sec(sec)));
    }

    void item::start_timer_based_on_wear_expire_event() {
        if (timer_based_on_wear_expire_event_)
            return;

        if (is_real_time_item())
            return;

        if (-1 == proto()->c_limit_timer_based_on_wear_index)
            return;

        int sec = get_socket(0);

        if (0 != sec) {
            sec %= 60;
            if (0 == sec)
                sec = 60;
        }

        auto *info = alloc_event_info<item_event_info>();
        info->item = this;

        set_timer_based_on_wear_expire_event(event_create(timer_based_on_wear_expire_event, info, passes_per_sec(sec)));
    }

    void item::stop_unique_expire_event() {
        if (!unique_expire_event_)
            return;

        if (value(2) != 0)
            return;

        if (vnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
            owner_->show_alignment(true);

        set_socket(ITEM_SOCKET_UNIQUE_SAVE_TIME, event_time(unique_expire_event_) / metin2::config::server().passes_per_sec);
        event_cancel(&unique_expire_event_);

        get_item_manager().save_single_item(this);
    }

    void item::stop_timer_based_on_wear_expire_event() {
        if (!timer_based_on_wear_expire_event_)
            return;

        int remain_time = get_socket(ITEM_SOCKET_REMAIN_SEC) - event_processing_time(timer_based_on_wear_expire_event_) / metin2::config::server().passes_per_sec;

        set_socket(ITEM_SOCKET_REMAIN_SEC, remain_time);
        event_cancel(&timer_based_on_wear_expire_event_);

        get_item_manager().save_single_item(this);
    }

    void item::apply_addon(int addon_type) {
        get_item_addon_manager().apply_addon_to(addon_type, this);
    }

    auto item::special_group() const -> int {
        return get_item_manager().special_group_from_item(vnum());
    }

    auto item::is_accessory_for_socket() -> bool {
        return (proto_->type == ITEM_ARMOR && (proto_->sub_type == ARMOR_WRIST || proto_->sub_type == ARMOR_NECK || proto_->sub_type == ARMOR_EAR)) ||
               (proto_->type == ITEM_BELT);
    }

    void item::set_accessory_socket_grade(int grade) {
        set_socket(0, std::clamp(grade, 0, accessory_socket_max_grade()));

        int down_time = accessory_socket_degrade_time[accessory_socket_grade()];

        set_accessory_socket_down_grade_time(down_time);
    }

    void item::set_accessory_socket_max_grade(int max_grade) {
        set_socket(1, std::clamp<int>(max_grade, 0, ITEM_ACCESSORY_SOCKET_MAX_NUM));
    }

    void item::set_accessory_socket_down_grade_time(uint32_t time) {
        set_socket(2, time);

        if (metin2::config::test_server() && owner())
            owner()->chat_packet(CHAT_TYPE_INFO, lc_text("%s에서 소켓 빠질때까지 남은 시간 %d"), name(), time);
    }

    EVENTFUNC(accessory_socket_expire_event) {
        auto *info = dynamic_cast<item_vid_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Accessory socket expire event has no info");
            return 0;
        }

        item *item = get_item_manager().find_by_vid(info->item_vid);

        if (item->accessory_socket_down_grade_time() <= 1) {
        degrade:
            item->set_accessory_socket_expire_event(nullptr);
            item->accessory_socket_degrade();
            return 0;
        } else {
            int time = item->accessory_socket_down_grade_time() - 60;

            if (time <= 1)
                goto degrade;

            item->set_accessory_socket_down_grade_time(time);

            if (time > 60)
                return passes_per_sec(60);
            else
                return passes_per_sec(time);
        }
    }

    void item::start_accessory_socket_expire_event() {
        if (!is_accessory_for_socket())
            return;

        if (accessory_socket_expire_event_)
            return;

        if (accessory_socket_max_grade() == 0)
            return;

        if (accessory_socket_grade() == 0)
            return;

        int sec = accessory_socket_down_grade_time();
        set_accessory_socket_expire_event(nullptr);

        if (sec <= 1)
            sec = 5;
        else
            sec = std::min(sec, 60);

        auto *info = alloc_event_info<item_vid_event_info>();
        info->item_vid = get_vid();

        set_accessory_socket_expire_event(event_create(accessory_socket_expire_event, info, passes_per_sec(sec)));
    }

    void item::stop_accessory_socket_expire_event() {
        if (!accessory_socket_expire_event_)
            return;

        if (!is_accessory_for_socket())
            return;

        int new_time = accessory_socket_down_grade_time() - (60 - event_time(accessory_socket_expire_event_) / metin2::config::server().passes_per_sec);

        event_cancel(&accessory_socket_expire_event_);

        if (new_time <= 1) {
            accessory_socket_degrade();
        } else {
            set_accessory_socket_down_grade_time(new_time);
        }
    }

    auto item::is_ride_item() -> bool {
        if (ITEM_UNIQUE == type() && UNIQUE_SPECIAL_RIDE == sub_type())
            return true;
        if (ITEM_UNIQUE == type() && UNIQUE_SPECIAL_MOUNT_RIDE == sub_type())
            return true;
        return false;
    }

    void item::clear_mount_attribute_and_affect() {
        character *ch = owner();

        ch->remove_affect(AFFECT_MOUNT);
        ch->remove_affect(AFFECT_MOUNT_BONUS);

        ch->mount_vnum(0);

        ch->point_change(POINT_ST, 0);
        ch->point_change(POINT_DX, 0);
        ch->point_change(POINT_HT, 0);
        ch->point_change(POINT_IQ, 0);
    }

    void item::set_accessory_socket_expire_event(event_ptr event) {
        accessory_socket_expire_event_ = event;
    }

    void item::accessory_socket_degrade() {
        if (accessory_socket_grade() > 0) {
            character *ch = owner();

            if (ch) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s에 박혀있던 보석이 사라집니다."), name());
            }

            modify_points(false);
            set_accessory_socket_grade(accessory_socket_grade() - 1);
            modify_points(true);

            int down_time = accessory_socket_degrade_time[accessory_socket_grade()];

            if (metin2::config::test_server())
                down_time /= 60;

            set_accessory_socket_down_grade_time(down_time);

            if (down_time)
                start_accessory_socket_expire_event();
        }
    }

    static auto can_put_into_ring(item *ring, item *item) -> bool {
        return false;
    }

    auto item::can_put_into(item *item) -> bool {
        if (item->type() == ITEM_BELT)
            return this->sub_type() == USE_PUT_INTO_BELT_SOCKET;

        else if (item->type() == ITEM_RING)
            return can_put_into_ring(item, this);

        else if (item->type() != ITEM_ARMOR)
            return false;

        uint32_t vnum_value = item->vnum();

        struct jewel_accessory_info {
            uint32_t jewel;
            uint32_t wrist;
            uint32_t neck;
            uint32_t ear;
        };
        const static jewel_accessory_info infos[] = {
            {50634, 14420, 16220, 17220},
            {50635, 14500, 16500, 17500},
            {50636, 14520, 16520, 17520},
            {50637, 14540, 16540, 17540},
            {50638, 14560, 16560, 17560},
        };

        uint32_t item_type = (item->vnum() / 10) * 10;
        for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++) {
            const jewel_accessory_info &info = infos[i];
            switch (item->sub_type()) {
            case ARMOR_WRIST:
                if (info.wrist == item_type) {
                    if (info.jewel == vnum()) {
                        return true;
                    } else {
                        return false;
                    }
                }
                break;
            case ARMOR_NECK:
                if (info.neck == item_type) {
                    if (info.jewel == vnum()) {
                        return true;
                    } else {
                        return false;
                    }
                }
                break;
            case ARMOR_EAR:
                if (info.ear == item_type) {
                    if (info.jewel == vnum()) {
                        return true;
                    } else {
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

        if (type > 11) {
            type = (vnum_value - 170) / 20;

            if (50623 + type != vnum())
                return false;
            else
                return true;
        } else if (item->vnum() >= 16210 && item->vnum() <= 16219) {
            if (50625 != vnum())
                return false;
            else
                return true;
        } else if (item->vnum() >= 16230 && item->vnum() <= 16239) {
            if (50626 != vnum())
                return false;
            else
                return true;
        }

        return 50623 + type == vnum();
    }

    auto item::is_pc_bang_item() -> bool {
        for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i) {
            if (proto_->limits[i].type == LIMIT_PCBANG)
                return true;
        }
        return false;
    }

    auto item::check_item_use_level(int level) -> bool {
        for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i) {
            if (this->proto_->limits[i].type == LIMIT_LEVEL) {
                if (this->proto_->limits[i].value > level)
                    return false;
                else
                    return true;
            }
        }
        return true;
    }

    void item::copy_socket_to(item *item_value) {
        for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i) {
            item_value->sockets_[i] = sockets_[i];
        }
    }

    auto item::accessory_socket_grade() -> int {
        return std::clamp(get_socket(0), 0, accessory_socket_max_grade());
    }

    auto item::accessory_socket_max_grade() -> int {
        return std::clamp<int>(get_socket(1), 0, ITEM_ACCESSORY_SOCKET_MAX_NUM);
    }

    auto item::accessory_socket_down_grade_time() -> int {
        return std::clamp(get_socket(2), 0, accessory_socket_degrade_time[accessory_socket_grade()]);
    }

    void item::attr_log() {
        const char *ip = nullptr;

        if (owner() && owner()->get_desc())
            ip = owner()->get_desc()->host_name();

        for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i) {
            if (sockets_[i]) {
                get_log_manager().item_log(i, sockets_[i], 0, id(), "INFO_SOCKET", "", ip ? ip : "", original_vnum());
            }
        }

        for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i) {
            int type = attr_[i].type;
            int value = attr_[i].s_value;

            if (type)
                get_log_manager().item_log(i, type, value, id(), "INFO_ATTR", "", ip ? ip : "", original_vnum());
        }
    }

    auto item::level_limit() -> int {
        for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i) {
            if (this->proto_->limits[i].type == LIMIT_LEVEL) {
                return this->proto_->limits[i].value;
            }
        }
        return 0;
    }

    auto item::on_after_created_item() -> bool {
        if (-1 != this->proto()->c_limit_real_time_first_use_index) {
            if (0 != get_socket(1)) {
                start_real_time_expire_event();
            }
        }

        return true;
    }

    auto item::is_dragon_soul() -> bool {
        return type() == ITEM_DS;
    }

    auto item::give_more_time_per(float percent) -> int {
        if (is_dragon_soul()) {
            uint32_t duration = get_ds_manager().duration(this);
            int remain_sec = get_socket(ITEM_SOCKET_REMAIN_SEC);
            int given_time = percent * duration / 100;
            if (static_cast<uint32_t>(remain_sec) == duration)
                return false;
            if (static_cast<uint32_t>(given_time + remain_sec) >= duration) {
                set_socket(ITEM_SOCKET_REMAIN_SEC, duration);
                return duration - remain_sec;
            } else {
                set_socket(ITEM_SOCKET_REMAIN_SEC, given_time + remain_sec);
                return given_time;
            }
        } else
            return 0;
    }

    auto item::give_more_time_fix(uint32_t time) -> int {
        if (is_dragon_soul()) {
            uint32_t duration = get_ds_manager().duration(this);
            int remain_sec = get_socket(ITEM_SOCKET_REMAIN_SEC);
            if (static_cast<uint32_t>(remain_sec) == duration)
                return false;
            if ((time + remain_sec) >= duration) {
                set_socket(ITEM_SOCKET_REMAIN_SEC, duration);
                return duration - remain_sec;
            } else {
                set_socket(ITEM_SOCKET_REMAIN_SEC, time + remain_sec);
                return time;
            }
        } else
            return 0;
    }

    auto item::duration() -> int {
        if (!proto())
            return -1;

        for (int i = 0; i < ITEM_LIMIT_MAX_NUM; i++) {
            if (LIMIT_REAL_TIME == proto()->limits[i].type)
                return proto()->limits[i].value;
        }

        if (-1 != proto()->c_limit_timer_based_on_wear_index)
            return proto()->limits[static_cast<int>(proto()->c_limit_timer_based_on_wear_index)].value;

        return -1;
    }

    auto item::is_same_special_group(item *const item) const -> bool {
        if (this->vnum() == item->vnum())
            return true;

        if (special_group() && (item->special_group() == special_group()))
            return true;

        return false;
    }
}
