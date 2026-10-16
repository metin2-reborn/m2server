#pragma once

#include <metin2/game/app/typedef.h>
#include <metin2/game/world/entity.h>

#include <metin2/common/attribute.h>
#include <metin2/log/log.h>

#include <cstdint>
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

    typedef union sectreeid {
        uint32_t package;
        sectree_coord coord;
    } sectreeid;

    enum {
        ATTR_BLOCK = (1 << 0),
        ATTR_WATER = (1 << 1),
        ATTR_BANPK = (1 << 2),
        ATTR_OBJECT = (1 << 7),
    };

    struct f_collect_entity {
        void operator()(entity *entity) {
            result.push_back(entity);
        }
        template <typename F>
        void for_each(F &f) {
            std::vector<entity *>::iterator it = result.begin();
            for (; it != result.end(); ++it) {
                entity *entity = *it;
                f(entity);
            }
        }
        typedef std::vector<entity *> list_type;
        list_type result;
    };

    class sectree {
      public:
        friend class sectree_manager;
        friend class sectree_map;

        template <class _Func>
        auto find_if(_Func &func) const -> entity * {
            auto it_tree = neighbor_list_.begin();

            while (it_tree != neighbor_list_.end()) {
                entity_set::iterator it_entity = (*it_tree)->set_entity_.begin();

                while (it_entity != (*it_tree)->set_entity_.end()) {
                    if (func(*it_entity))
                        return (*it_entity);

                    ++it_entity;
                }

                ++it_tree;
            }

            return nullptr;
        }

        template <class _Func>
        void for_each_around(_Func &func) {
            f_collect_entity collector;
            sectree_list::iterator it = neighbor_list_.begin();
            for (; it != neighbor_list_.end(); ++it) {
                sectree *sectree = *it;
                sectree->for_each_entity(collector);
            }
            collector.for_each(func);
        }

        template <class _Func>
        void for_each_for_find_victim(_Func &func) {
            sectree_list::iterator it_tree = neighbor_list_.begin();

            while (it_tree != neighbor_list_.end()) {
                if ((*(it_tree++))->for_each_entity_for_find_victim(func))
                    return;
            }
        }
        template <class _Func>
        auto for_each_entity_for_find_victim(_Func &func) -> bool {
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

        void bind_attribute(attribute *attribute_value);

        auto get_attribute(int32_t x, int32_t y) -> uint32_t;
        auto is_attr(int32_t x, int32_t y, uint32_t flag) -> bool;

        void clone_attribute(sectree *tree);

        auto event_attribute(int32_t x, int32_t y) -> int;

        void set_attribute(uint32_t x, uint32_t y, uint32_t attr);
        void remove_attribute(uint32_t x, uint32_t y, uint32_t attr);

      private:
        template <class _Func>
        void for_each_entity(_Func &func) {
            auto it = set_entity_.begin();
            for (; it != set_entity_.end(); ++it) {
                entity *entity = *it;
                if (entity->get_sectree() != this) {
                    metin2::log::error("Entity in sectree points to a different sectree");
                    set_entity_.erase(it);
                    continue;
                }
                func(entity);
            }
        }

        sectreeid id_;
        entity_set set_entity_;
        sectree_list neighbor_list_;
        int pc_count_;
        bool is_clone_;

        attribute *attribute_;
    };
}
