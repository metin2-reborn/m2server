#include <metin2/game/world/sectree.h>

#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/protocol/desc_manager.h>

#include <metin2/common/attribute.h>
#include <metin2/log/log.h>

#include <cassert>
#include <cstdint>
#include <memory>
#include <utility>

namespace metin2::game::inline world {

    sectree::sectree() {
        initialize();
    }

    sectree::~sectree() {
        destroy();
    }

    void sectree::initialize() {
        id_.package = 0;
        attribute_ = nullptr;
        pc_count_ = 0;
    }

    void sectree::destroy() {
        if (!set_entity_.empty()) {
            metin2::log::error("Sectree destroyed with entities still in it");

            auto it = set_entity_.begin();

            for (; it != set_entity_.end(); ++it) {
                entity *ent = *it;

                if (ent->is_type(ENTITY_CHARACTER)) {
                    auto *ch = (character *)ent;

                    metin2::log::error("Sectree destroyed with character {} still in it (is_pc {:d})", ch->name(), ch->is_pc() ? 1 : 0);

                    if (ch->get_desc())
                        desc_manager::instance().destroy_desc(ch->get_desc());
                    else
                        character_manager::instance().destroy_character(ch);
                } else if (ent->is_type(ENTITY_ITEM)) {
                    item *item = (game::items::item *)ent;

                    metin2::log::error("Sectree destroyed with item {} still in it", item->name());

                    item_manager::instance().destroy_item(item);
                } else
                    metin2::log::error("Sectree destroyed with entity of unknown type {:d} still in it", ent->type());
            }
        }
        set_entity_.clear();

        attribute_.reset();
    }

    auto sectree::id() -> sectreeid {
        return id_;
    }

    void sectree::increase_pc() {
        auto it_tree = neighbor_list_.begin();

        while (it_tree != neighbor_list_.end()) {
            ++(*it_tree)->pc_count_;
            ++it_tree;
        }
    }

    void sectree::decrease_pc() {
        auto it_tree = neighbor_list_.begin();

        while (it_tree != neighbor_list_.end()) {
            sectree *tree = *it_tree++;

            if (--tree->pc_count_ <= 0) {
                if (tree->pc_count_ < 0) {
                    metin2::log::error("Sectree player count dropped below zero ({:d}) at {:d}x{:d}", tree->pc_count_, tree->id_.coord.x, tree->id_.coord.y);
                    tree->pc_count_ = 0;
                }

                auto it_entity = tree->set_entity_.begin();

                while (it_entity != tree->set_entity_.end()) {
                    entity *ent = *(it_entity++);

                    if (ent->is_type(ENTITY_CHARACTER)) {
                        auto *ch = (character *)ent;
                        ch->stop_state_machine();
                    }
                }
            }
        }
    }

    auto sectree::insert_entity(entity *ent) -> bool {
        sectree *cur_tree;

        if ((cur_tree = ent->get_sectree()) == this)
            return false;

        if (set_entity_.find(ent) != set_entity_.end()) {
            metin2::log::error("Entity {} is already in this sectree", static_cast<const void *>(ent));
            return false;
        }

        if (cur_tree)
            cur_tree->set_entity_.erase(ent);

        ent->set_sectree(this);

        set_entity_.insert(ent);

        if (ent->is_type(ENTITY_CHARACTER)) {
            auto *chr = (character *)ent;

            if (chr->is_pc()) {
                increase_pc();

                if (cur_tree)
                    cur_tree->decrease_pc();
            } else if (pc_count_ > 0 && !chr->is_warp() && !chr->is_goto()) {
                chr->start_state_machine();
            }
        }

        return true;
    }

    void sectree::remove_entity(entity *ent) {
        auto it = set_entity_.find(ent);

        if (it == set_entity_.end()) {
            return;
        }
        set_entity_.erase(it);

        ent->set_sectree(nullptr);

        if (ent->is_type(ENTITY_CHARACTER)) {
            if (((character *)ent)->is_pc())
                decrease_pc();
        }
    }

    void sectree::bind_attribute(std::unique_ptr<attribute> attribute_value) {
        attribute_ = std::move(attribute_value);
    }

    void sectree::clone_attribute(sectree *tree) {
        attribute_ = tree->attribute_;
    }

    void sectree::set_attribute(uint32_t x, uint32_t y, uint32_t attr) {
        assert(attribute_ != nullptr);
        attribute_->set(x, y, attr);
    }

    void sectree::remove_attribute(uint32_t x, uint32_t y, uint32_t attr) {
        assert(attribute_ != nullptr);
        attribute_->remove(x, y, attr);
    }

    auto sectree::get_attribute(int32_t x, int32_t y) -> uint32_t {
        assert(attribute_ != nullptr);
        return attribute_->get((x % SECTREE_SIZE) / CELL_SIZE, (y % SECTREE_SIZE) / CELL_SIZE);
    }

    auto sectree::is_attr(int32_t x, int32_t y, uint32_t flag) -> bool {
        if (is_set(get_attribute(x, y), flag))
            return true;

        return false;
    }

    auto sectree::event_attribute(int32_t x, int32_t y) -> int {
        return get_attribute(x, y) >> 8;
    }
}
