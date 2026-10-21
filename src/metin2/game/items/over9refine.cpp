#include <metin2/game/items/over9refine.h>

#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/persistence/log.h>

#include <metin2/proto/inventory.h>

#include <cstdint>
#include <cstdio>
#include <format>
#include <string>
#include <utility>

namespace metin2::game::inline items {

    void over9_refine_manager::enable_over9_refine(uint32_t vnum_from, uint32_t vnum_to) {
        item_.insert(std::make_pair(vnum_from, vnum_to));
    }

    auto over9_refine_manager::can_over9_refine(uint32_t vnum) -> int {
        auto iter = item_.find(vnum);

        if (iter != item_.end())
            return 1;

        if (vnum % 10 == 9)
            return 0;

        vnum -= vnum % 10;

        for (iter = item_.begin(); iter != item_.end(); ++iter)
            if (iter->second == vnum)
                return 2;

        return 0;
    }

    auto over9_refine_manager::change9_to_over9(character *ch, item *item) -> bool {
        auto iter = item_.find(item->vnum());

        if (iter == item_.end())
            return false;

        uint32_t vnum = iter->second;

        game::items::item *over9 = get_item_manager().create_item(vnum, 1);

        if (over9 == nullptr)
            return false;

        item->copy_socket_to(over9);
        item->copy_attribute_to(over9);

        int empty_cell = ch->empty_inventory(over9->size());

        if (empty_cell == -1)
            return false;

        item->remove_from_character();

        over9->add_to_character(ch, item_pos(INVENTORY, empty_cell));

        const std::string buf = std::format("SUCCESS {} {} {}", over9->id(), over9->name(), over9->original_vnum());
        get_log_manager().item_log(ch, item, "REFINE OVER9", buf.c_str());
        return true;
    }

    auto over9_refine_manager::over9_refine(character *ch, item *item) -> bool {
        uint32_t vnum = item->refined_vnum();

        if (vnum == 0)
            return false;

        game::items::item *over9 = get_item_manager().create_item(vnum, 1);

        if (over9 == nullptr)
            return false;

        item->copy_socket_to(over9);
        item->copy_attribute_to(over9);

        int empty_cell = ch->empty_inventory(over9->size());

        if (empty_cell == -1)
            return false;

        item->remove_from_character();

        over9->add_to_character(ch, item_pos(INVENTORY, empty_cell));

        const std::string buf = std::format("SUCCESS {} {} {}", over9->id(), over9->name(), over9->original_vnum());
        get_log_manager().item_log(ch, item, "REFINE OVER9", buf.c_str());
        return true;
    }

    auto over9_refine_manager::material_vnum(uint32_t base_vnum) -> uint32_t {
        auto iter = item_.find(base_vnum);

        if (iter != item_.end())
            return (base_vnum - (base_vnum % 10));

        base_vnum -= base_vnum % 10;

        for (iter = item_.begin(); iter != item_.end(); ++iter)
            if (iter->second == base_vnum)
                return (iter->first - (iter->first % 10));

        return 0;
    }
}
