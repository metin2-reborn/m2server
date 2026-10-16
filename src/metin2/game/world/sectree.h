#pragma once

#include <metin2/game/app/typedef.h>
#include <metin2/game/world/entity.h>

#include <metin2/common/attribute.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace metin2::game::inline world {

    enum sectree_constants {
        SECTREE_SIZE = 6400,
        CELL_SIZE = 50
    };

    struct sectree_coord {
        unsigned x : 16;
        unsigned y : 16;
    };

    union sectreeid {
        uint32_t package;
        sectree_coord coord;
    };

    enum {
        ATTR_BLOCK = (1 << 0),
        ATTR_WATER = (1 << 1),
        ATTR_BANPK = (1 << 2),
        ATTR_OBJECT = (1 << 7),
    };

    class sectree {
      public:
        friend class sectree_manager;
        friend class sectree_map;

        template <class Func>
        auto find_if(Func &&func) const -> entity * {
            auto it_tree = neighbor_list_.begin();

            while (it_tree != neighbor_list_.end()) {
                auto it_entity = (*it_tree)->set_entity_.begin();

                while (it_entity != (*it_tree)->set_entity_.end()) {
                    if (func(*it_entity))
                        return (*it_entity);

                    ++it_entity;
                }

                ++it_tree;
            }

            return nullptr;
        }

        template <class Func>
        void for_each_around(Func &&func) {
            std::vector<entity *> collected;
            for (sectree *sectree : neighbor_list_)
                sectree->for_each_entity([&collected](entity *e) { collected.push_back(e); });
            for (entity *e : collected)
                func(e);
        }

        template <class Func>
        void for_each_for_find_victim(Func &&func) {
            auto it_tree = neighbor_list_.begin();

            while (it_tree != neighbor_list_.end()) {
                if ((*(it_tree++))->for_each_entity_for_find_victim(func))
                    return;
            }
        }
        template <class Func>
        auto for_each_entity_for_find_victim(Func &&func) -> bool {
            auto it = set_entity_.begin();

            while (it != set_entity_.end()) {
                if (func(*it++))
                    return true;
            }
            return false;
        }

      public:
        sectree();
        ~sectree();

        void initialize();
        void destroy();

        auto id() -> sectreeid;

        auto insert_entity(entity *ent) -> bool;
        void remove_entity(entity *ent);

        void increase_pc();
        void decrease_pc();

        void bind_attribute(std::unique_ptr<attribute> attribute_value);

        auto get_attribute(int32_t x, int32_t y) -> uint32_t;
        auto is_attr(int32_t x, int32_t y, uint32_t flag) -> bool;

        void clone_attribute(sectree *tree);

        auto event_attribute(int32_t x, int32_t y) -> int;

        void set_attribute(uint32_t x, uint32_t y, uint32_t attr);
        void remove_attribute(uint32_t x, uint32_t y, uint32_t attr);

      private:
        template <class Func>
        void for_each_entity(Func &&func) {
            std::erase_if(set_entity_, [this](entity *e) {
                if (e->get_sectree() == this)
                    return false;
                metin2::log::error("Entity in sectree points to a different sectree");
                return true;
            });

            for (entity *e : set_entity_)
                func(e);
        }

        sectreeid id_;
        entity_set set_entity_;
        sectree_list neighbor_list_;
        int pc_count_;

        std::shared_ptr<attribute> attribute_;
    };
}
