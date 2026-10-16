
#include <metin2/config/config.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/buff_on_attributes.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/characters/polymorph.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/empire/castle.h>
#include <metin2/game/empire/monarch.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/empire/threeway_war.h>
#include <metin2/game/events/arena.h>
#include <metin2/game/events/battle_arena.h>
#include <metin2/game/events/fishing.h>
#include <metin2/game/guilds/war_map.h>
#include <metin2/game/items/belt_inventory_helper.h>
#include <metin2/game/items/blend_item.h>
#include <metin2/game/items/dragon_soul.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/items/refine.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/social/marriage.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/trade/safebox.h>
#include <metin2/game/trade/shop.h>
#include <metin2/game/world/dungeon.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/world/sectree.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/world/vector.h>

#include <metin2/core/main.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/apply_types.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/limits.h>
#include <metin2/proto/log_types.h>
#include <metin2/proto/tables.h>
#include <metin2/proto/vnum_helper.h>
#include <metin2/sql/query.h>

#include <algorithm>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <format>
#include <iterator>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game::inline characters {

    const int ITEM_BROKEN_METIN_VNUM = 28960;

    const uint32_t character::default_change_item_attr_cycle_ = 10;
    const char character::last_change_item_attr_flag_[] = "Item.LastChangeItemAttr";
    const char character::change_item_attr_cycle_flag_[] = "change_itemattr_cycle";
    constexpr uint8_t buff_on_attr_points[] = {POINT_ENERGY, POINT_COSTUME_ATTR_BONUS};

    static auto is_summon_item(int vnum) -> bool {
        switch (vnum) {
        case 22000:
        case 22010:
        case 22011:
        case 22020:
        case ITEM_MARRIAGE_RING:
            return true;
        }

        return false;
    }

    static constexpr auto ability_potion_affect_type(int32_t apply) -> uint32_t {
        switch (apply) {
        case APPLY_MOV_SPEED:
            return AFFECT_MOV_SPEED;
        case APPLY_ATT_SPEED:
            return AFFECT_ATT_SPEED;
        case APPLY_STR:
            return AFFECT_STR;
        case APPLY_DEX:
            return AFFECT_DEX;
        case APPLY_CON:
            return AFFECT_CON;
        case APPLY_INT:
            return AFFECT_INT;
        case APPLY_CAST_SPEED:
            return AFFECT_CAST_SPEED;
        case APPLY_ATT_GRADE_BONUS:
            return AFFECT_ATT_GRADE;
        case APPLY_DEF_GRADE_BONUS:
            return AFFECT_DEF_GRADE;
        }

        return AFFECT_NONE;
    }

    static auto is_monkey_dungeon(int map_index) -> bool {
        switch (map_index) {
        case 5:
        case 25:
        case 45:
        case 108:
        case 109:
            return true;
            ;
        }

        return false;
    }

    auto is_summonable_zone(int map_index) -> bool {
        if (is_monkey_dungeon(map_index))
            return false;
        if (is_castle_map(map_index))
            return false;

        switch (map_index) {
        case 66:
        case 71:
        case 72:
        case 73:
        case 193:
        case 216:
        case 217:
        case 208:
            return false;
        }

        if (battle_arena::is_battle_arena_map(map_index)) return false;

        if (map_index > 10000) return false;

        return true;
    }

    static auto fn_check_item_socket(item *item) -> bool {
        for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i) {
            if (item->get_socket(i) != item->proto()->sockets[i])
                return false;
        }

        return true;
    }

    static void fn_copy_item_socket(item *dest, item *src) {
        for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i) {
            dest->set_socket(i, src->get_socket(i));
        }
    }
    static auto fn_check_item_sex(character *ch, item *item) -> bool {
        if (is_set(item->anti_flag(), ITEM_ANTIFLAG_MALE)) {
            if (SEX_MALE == get_sex(ch))
                return false;
        }
        if (is_set(item->anti_flag(), ITEM_ANTIFLAG_FEMALE)) {
            if (SEX_FEMALE == get_sex(ch))
                return false;
        }

        return true;
    }

    auto character::can_handle_item(bool skip_check_refine, bool skip_observer) -> bool {
        if (!skip_observer)
            if (is_observer_)
                return false;

        if (my_shop())
            return false;

        if (!skip_check_refine)
            if (under_refine_)
                return false;

        if (is_cube_open() || nullptr != dragon_soul_refine_window_get_opener())
            return false;

        if (is_warping())
            return false;

        return true;
    }

    auto character::inventory_item(uint16_t cell) const -> item * {
        return get_item(item_pos(INVENTORY, cell));
    }
    auto character::get_item(item_pos cell_value) const -> item * {
        if (!is_valid_item_position(cell_value))
            return nullptr;
        uint16_t w_cell = cell_value.cell;
        uint8_t window_type = cell_value.window_type;
        switch (window_type) {
        case INVENTORY:
        case EQUIPMENT:
            if (w_cell >= INVENTORY_AND_EQUIP_SLOT_MAX) {
                metin2::log::error("Invalid inventory cell {:d}", w_cell);
                return nullptr;
            }
            return points_instant_.items[w_cell];
        case DRAGON_SOUL_INVENTORY:
            if (w_cell >= DRAGON_SOUL_INVENTORY_MAX_NUM) {
                metin2::log::error("Invalid dragon soul inventory cell {:d}", w_cell);
                return nullptr;
            }
            return points_instant_.ds_items[w_cell];

        default:
            return nullptr;
        }
        return nullptr;
    }

    void character::set_item(item_pos cell_value, item *item_value) {
        uint16_t w_cell = cell_value.cell;
        uint8_t window_type = cell_value.window_type;
        if (reinterpret_cast<uintptr_t>((item *)item_value) == 0xff || reinterpret_cast<uintptr_t>((item *)item_value) == 0xffffffff || reinterpret_cast<uintptr_t>((item *)item_value) == UINTPTR_MAX) {
            metin2::log::error("Invalid item pointer 0xff for {} at cell {:d}", name(), w_cell);
            core_dump();
            return;
        }

        if (item_value && item_value->owner()) {
            assert(!"GetOwner exist");
            return;
        }
        switch (window_type) {
        case INVENTORY:
        case EQUIPMENT:
            {
                if (w_cell >= INVENTORY_AND_EQUIP_SLOT_MAX) {
                    metin2::log::error("Invalid inventory cell {:d}", w_cell);
                    return;
                }

                item *old = points_instant_.items[w_cell];

                if (old) {
                    if (w_cell < INVENTORY_MAX_NUM) {
                        for (int i = 0; i < old->size(); ++i) {
                            int p = w_cell + (i * 5);

                            if (p >= INVENTORY_MAX_NUM)
                                continue;

                            if (points_instant_.items[p] && points_instant_.items[p] != old)
                                continue;

                            points_instant_.item_grid[p] = 0;
                        }
                    } else
                        points_instant_.item_grid[w_cell] = 0;
                }

                if (item_value) {
                    if (w_cell < INVENTORY_MAX_NUM) {
                        for (int i = 0; i < item_value->size(); ++i) {
                            int p = w_cell + (i * 5);

                            if (p >= INVENTORY_MAX_NUM)
                                continue;

                            points_instant_.item_grid[p] = w_cell + 1;
                        }
                    } else
                        points_instant_.item_grid[w_cell] = w_cell + 1;
                }

                points_instant_.items[w_cell] = item_value;
            }
            break;
        case DRAGON_SOUL_INVENTORY:
            {
                if (w_cell >= DRAGON_SOUL_INVENTORY_MAX_NUM) {
                    metin2::log::error("Invalid dragon soul inventory cell {:d}", w_cell);
                    return;
                }

                item *old = points_instant_.ds_items[w_cell];

                if (old) {
                    if (w_cell < DRAGON_SOUL_INVENTORY_MAX_NUM) {
                        for (int i = 0; i < old->size(); ++i) {
                            int p = w_cell + (i * DRAGON_SOUL_BOX_COLUMN_NUM);

                            if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
                                continue;

                            if (points_instant_.ds_items[p] && points_instant_.ds_items[p] != old)
                                continue;

                            points_instant_.ds_item_grid[p] = 0;
                        }
                    } else
                        points_instant_.ds_item_grid[w_cell] = 0;
                }

                if (item_value) {
                    if (w_cell >= DRAGON_SOUL_INVENTORY_MAX_NUM) {
                        metin2::log::error("Invalid dragon soul inventory cell {:d}", w_cell);
                        return;
                    }

                    if (w_cell < DRAGON_SOUL_INVENTORY_MAX_NUM) {
                        for (int i = 0; i < item_value->size(); ++i) {
                            int p = w_cell + (i * DRAGON_SOUL_BOX_COLUMN_NUM);

                            if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
                                continue;

                            points_instant_.ds_item_grid[p] = w_cell + 1;
                        }
                    } else
                        points_instant_.ds_item_grid[w_cell] = w_cell + 1;
                }

                points_instant_.ds_items[w_cell] = item_value;
            }
            break;
        default:
            metin2::log::error("Invalid window type {:d}", window_type);
            return;
        }

        if (get_desc()) {
            if (item_value) {
                packet_gc_item_set pack;
                pack.header = HEADER_GC_ITEM_SET;
                pack.cell = cell_value;

                pack.count = item_value->count();
                pack.vnum = item_value->vnum();
                pack.flags = item_value->flag();
                pack.anti_flags = item_value->anti_flag();
                pack.highlight = (cell_value.window_type == DRAGON_SOUL_INVENTORY);

                std::memcpy(pack.sockets, item_value->sockets(), sizeof(pack.sockets));
                std::memcpy(pack.attr, item_value->attributes(), sizeof(pack.attr));

                get_desc()->packet(&pack, sizeof(packet_gc_item_set));
            } else {
                packet_gc_item_del_deprecated pack;
                pack.header = HEADER_GC_ITEM_DEL;
                pack.cell = cell_value;
                pack.count = 0;
                pack.vnum = 0;
                memset(pack.sockets, 0, sizeof(pack.sockets));
                memset(pack.attr, 0, sizeof(pack.attr));

                get_desc()->packet(&pack, sizeof(packet_gc_item_del_deprecated));
            }
        }

        if (item_value) {
            item_value->set_cell(this, w_cell);
            switch (window_type) {
            case INVENTORY:
            case EQUIPMENT:
                if ((w_cell < INVENTORY_MAX_NUM) || (BELT_INVENTORY_SLOT_START <= w_cell && BELT_INVENTORY_SLOT_END > w_cell))
                    item_value->set_window(INVENTORY);
                else
                    item_value->set_window(EQUIPMENT);
                break;
            case DRAGON_SOUL_INVENTORY:
                item_value->set_window(DRAGON_SOUL_INVENTORY);
                break;
            }
        }
    }

    auto character::wear(uint8_t cell) const -> item * {
        if (cell >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * static_cast<int>(DS_SLOT_MAX)) {
            metin2::log::error("Invalid wear cell {:d}", cell);
            return nullptr;
        }

        return points_instant_.items[INVENTORY_MAX_NUM + cell];
    }

    void character::set_wear(uint8_t cell, item *item) {
        if (cell >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * static_cast<int>(DS_SLOT_MAX)) {
            metin2::log::error("Invalid wear cell {:d}", cell);
            return;
        }

        set_item(item_pos(INVENTORY, INVENTORY_MAX_NUM + cell), item);

        if (!item && cell == WEAR_WEAPON) {
            if (is_affect_flag(AFF_GWIGUM))
                remove_affect(SKILL_GWIGEOM);

            if (is_affect_flag(AFF_GEOMGYEONG))
                remove_affect(SKILL_GEOMKYUNG);
        }
    }

    void character::clear_item() {
        int i;
        item *item;

        for (i = 0; i < INVENTORY_AND_EQUIP_SLOT_MAX; ++i) {
            if ((item = inventory_item(i))) {
                item->set_skip_save(true);
                item_manager::instance().flush_delayed_save(item);

                item->remove_from_character();
                item_manager::instance().destroy_item(item);

                sync_quickslot(QUICKSLOT_TYPE_ITEM, i, 255);
            }
        }
        for (i = 0; i < DRAGON_SOUL_INVENTORY_MAX_NUM; ++i) {
            if ((item = get_item(item_pos(DRAGON_SOUL_INVENTORY, i)))) {
                item->set_skip_save(true);
                item_manager::instance().flush_delayed_save(item);

                item->remove_from_character();
                item_manager::instance().destroy_item(item);
            }
        }
    }

    auto character::is_empty_item_grid(item_pos cell_value, uint8_t size, int exception_cell) const -> bool {
        const auto is_free_column = [&](const auto &grid, uint16_t cell, uint16_t row_stride, uint16_t grid_size, uint16_t page_size) -> bool {
            if (grid[cell] && grid[cell] != exception_cell)
                return false;

            if (size == 1)
                return true;

            int j = 1;

            do {
                const uint16_t p = cell + (row_stride * j);

                if (p >= grid_size)
                    return false;

                if (p / page_size != cell / page_size)
                    return false;

                if (grid[p] && grid[p] != exception_cell)
                    return false;
            } while (++j < size);

            return true;
        };

        switch (cell_value.window_type) {
        case INVENTORY:
            {
                if (cell_value.cell >= INVENTORY_AND_EQUIP_SLOT_MAX)
                    return false;

                uint8_t cell = cell_value.cell;

                ++exception_cell;

                if (cell_value.is_belt_inventory_position()) {
                    item *belt_item = wear(WEAR_BELT);

                    if (nullptr == belt_item)
                        return false;

                    if (false == belt_inventory_helper::is_available_cell(cell - BELT_INVENTORY_SLOT_START, belt_item->value(0)))
                        return false;

                    if (points_instant_.item_grid[cell]) {
                        if (points_instant_.item_grid[cell] == exception_cell)
                            return true;

                        return false;
                    }

                    if (size == 1)
                        return true;

                } else if (cell >= INVENTORY_MAX_NUM)
                    return false;

                return is_free_column(points_instant_.item_grid, cell, 5, INVENTORY_MAX_NUM, INVENTORY_MAX_NUM / 2);
            }
        case DRAGON_SOUL_INVENTORY:
            {
                uint16_t w_cell = cell_value.cell;
                if (w_cell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
                    return false;

                exception_cell++;

                return is_free_column(points_instant_.ds_item_grid, w_cell, DRAGON_SOUL_BOX_COLUMN_NUM, DRAGON_SOUL_INVENTORY_MAX_NUM, DRAGON_SOUL_INVENTORY_MAX_NUM);
            }
        }

        return false;
    }

    auto character::empty_inventory(uint8_t size) const -> int {
        // The belt inventory is excluded: only the basic inventory takes new items.
        for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
            if (is_empty_item_grid(item_pos(INVENTORY, i), size))
                return i;
        return -1;
    }

    auto character::empty_dragon_soul_inventory(item *item_value) const -> int {
        if (nullptr == item_value || !item_value->is_dragon_soul())
            return -1;
        if (!dragon_soul_is_qualified()) {
            return -1;
        }
        uint8_t size = item_value->size();
        uint16_t base_cell = ds_manager::instance().base_position(item_value);

        if (WORD_MAX == base_cell)
            return -1;

        for (int i = 0; i < DRAGON_SOUL_BOX_SIZE; ++i)
            if (is_empty_item_grid(item_pos(DRAGON_SOUL_INVENTORY, i + base_cell), size))
                return i + base_cell;

        return -1;
    }

    void character::copy_dragon_soul_item_grid(std::vector<uint16_t> &dragon_soul_item_grid) const {
        dragon_soul_item_grid.resize(DRAGON_SOUL_INVENTORY_MAX_NUM);

        std::copy(points_instant_.ds_item_grid, points_instant_.ds_item_grid + DRAGON_SOUL_INVENTORY_MAX_NUM, dragon_soul_item_grid.begin());
    }

    auto character::count_empty_inventory() const -> int {
        int count = 0;

        for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
            if (inventory_item(i))
                count += inventory_item(i)->size();

        return (INVENTORY_MAX_NUM - count);
    }

    void notify_refine_success(character *ch, item *item, const char *way) {
        if (nullptr != ch && item != nullptr) {
            ch->chat_packet(CHAT_TYPE_COMMAND, "RefineSuceeded");

            log_manager::instance().refine_log(ch->player_id(), item->name(), item->id(), item->refine_level(), 1, way);
        }
    }

    void notify_refine_fail(character *ch, item *item, const char *way, int success = 0) {
        if (nullptr != ch && nullptr != item) {
            ch->chat_packet(CHAT_TYPE_COMMAND, "RefineFailed");

            log_manager::instance().refine_log(ch->player_id(), item->name(), item->id(), item->refine_level(), success, way);
        }
    }

    auto character::is_refine_paused_after_update() const -> bool {
        if (quest::quest_manager::instance().event_flag("update_refine_time") != 0) {
            if (get_global_time() < quest::quest_manager::instance().event_flag("update_refine_time") + (60 * 5)) {
                metin2::log::info("{} (pid {:d}) cannot refine: refining is paused after an update", name(), player_id());
                return true;
            }
        }

        return false;
    }

    auto character::replace_refined_item(item *item, uint32_t new_vnum, int cost, bool refine_succeeded, const char *way) -> bool {
        game::items::item *new_item = item_manager::instance().create_item(new_vnum, 1, 0, false);

        if (!new_item) {
            metin2::log::error("Cannot create refined item {:d}", new_vnum);
            return false;
        }

        item_manager::copy_all_attr_to(item, new_item);
        log_manager::instance().item_log(this, new_item, refine_succeeded ? "REFINE SUCCESS" : "REFINE FAIL", new_item->name());

        uint8_t cell = item->cell();

        if (refine_succeeded) {
            notify_refine_success(this, item, way);
            db_manager::instance().send_money_log(MONEY_LOG_REFINE, item->vnum(), -cost);
        } else {
            db_manager::instance().send_money_log(MONEY_LOG_REFINE, item->vnum(), -cost);
            notify_refine_fail(this, item, way, -1);
        }

        item_manager::instance().remove_item(item, refine_succeeded ? "REMOVE (REFINE SUCCESS)" : "REMOVE (REFINE FAIL)");

        new_item->add_to_character(this, item_pos(INVENTORY, cell));
        item_manager::instance().flush_delayed_save(new_item);
        new_item->attr_log();
        pay_refine_fee(cost);
        return true;
    }

    void character::set_refine_npc(character *ch) {
        if (ch != nullptr) {
            refine_npcvid_ = ch->get_vid();
        } else {
            refine_npcvid_ = 0;
        }
    }

    auto character::do_refine(item *item, bool money_only) -> bool {
        if (!can_handle_item(true)) {
            clear_refine_mode();
            return false;
        }

        if (is_refine_paused_after_update())
            return false;

        const refine_table *prt = refine_manager::instance().refine_recipe(item->refine_set());

        if (!prt)
            return false;

        uint32_t result_vnum = item->refined_vnum();

        int cost = compute_refine_fee(prt->cost);

        int refine_chance = quest_flag("main_quest_lv7.refine_chance");

        if (refine_chance > 0) {
            if (!item->check_item_use_level(20) || item->type() != ITEM_WEAPON) {
                chat_packet(CHAT_TYPE_INFO, lc_text("무료 개량 기회는 20 이하의 무기만 가능합니다"));
                return false;
            }

            cost = 0;
            set_quest_flag("main_quest_lv7.refine_chance", refine_chance - 1);
        }

        if (result_vnum == 0) {
            chat_packet(CHAT_TYPE_INFO, lc_text("더 이상 개량할 수 없습니다."));
            return false;
        }

        if (item->type() == ITEM_USE && item->sub_type() == USE_TUNING)
            return false;

        item_table *proto_value = item_manager::instance().table(item->refined_vnum());

        if (!proto_value) {
            metin2::log::error("No proto for refined item vnum {:d}", item->refined_vnum());
            chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템은 개량할 수 없습니다."));
            return false;
        }

        if (gold() < cost) {
            chat_packet(CHAT_TYPE_INFO, lc_text("개량을 하기 위한 돈이 부족합니다."));
            return false;
        }

        if (!money_only && !refine_chance) {
            for (int i = 0; i < prt->material_count; ++i) {
                if (count_specify_item(prt->materials[i].vnum) < prt->materials[i].count) {
                    if (metin2::config::test_server()) {
                        chat_packet(CHAT_TYPE_INFO, "Find %d, count %d, require %d", prt->materials[i].vnum, count_specify_item(prt->materials[i].vnum), prt->materials[i].count);
                    }
                    chat_packet(CHAT_TYPE_INFO, lc_text("개량을 하기 위한 재료가 부족합니다."));
                    return false;
                }
            }

            for (int i = 0; i < prt->material_count; ++i)
                remove_specify_item(prt->materials[i].vnum, prt->materials[i].count);
        }

        int prob = number(1, 100);

        if (is_refine_through_guild() || money_only)
            prob -= 10;

        if (prob <= prt->prob) {
            if (replace_refined_item(item, result_vnum, cost, true, is_refine_through_guild() ? "GUILD" : "POWER"))
                metin2::log::debug("Refine succeeded, fee {:d}", cost);
            else
                notify_refine_fail(this, item, is_refine_through_guild() ? "GUILD" : "POWER");
        } else {
            db_manager::instance().send_money_log(MONEY_LOG_REFINE, item->vnum(), -cost);
            notify_refine_fail(this, item, is_refine_through_guild() ? "GUILD" : "POWER");
            item->attr_log();
            item_manager::instance().remove_item(item, "REMOVE (REFINE FAIL)");

            pay_refine_fee(cost);
        }

        return true;
    }

    enum enum_refine_scrolls {
        HYUNIRON_CHN = 1,
        YONGSIN_SCROLL = 2,
        MUSIN_SCROLL = 3,
        YAGONG_SCROLL = 4,
        MEMO_SCROLL = 5,
        BDRAGON_SCROLL = 6,
    };

    constexpr int hyuniron_refine_prob[9] = {100, 75, 65, 55, 45, 40, 35, 25, 20};
    constexpr int yagong_refine_prob[9] = {100, 100, 90, 80, 70, 60, 50, 30, 20};

    auto character::do_refine_with_scroll(item *item) -> bool {
        if (!can_handle_item(true)) {
            clear_refine_mode();
            return false;
        }

        clear_refine_mode();

        if (is_refine_paused_after_update())
            return false;

        const refine_table *prt = refine_manager::instance().refine_recipe(item->refine_set());

        if (!prt)
            return false;

        game::items::item *item_scroll;

        if (refine_additional_cell_ < 0)
            return false;

        item_scroll = inventory_item(refine_additional_cell_);

        if (!item_scroll)
            return false;

        if (!(item_scroll->type() == ITEM_USE && item_scroll->sub_type() == USE_TUNING))
            return false;

        if (item_scroll->vnum() == item->vnum())
            return false;

        uint32_t result_vnum = item->refined_vnum();
        uint32_t result_fail_vnum = item->refine_from_vnum();

        if (result_vnum == 0) {
            chat_packet(CHAT_TYPE_INFO, lc_text("더 이상 개량할 수 없습니다."));
            return false;
        }

        const int32_t scroll_type = item_scroll->value(0);

        switch (scroll_type) {
        case MUSIN_SCROLL:
            if (item->refine_level() >= 4) {
                chat_packet(CHAT_TYPE_INFO, lc_text("이 개량서로 더 이상 개량할 수 없습니다."));
                return false;
            }
            break;

        case MEMO_SCROLL:
            if (item->refine_level() != item_scroll->value(1)) {
                chat_packet(CHAT_TYPE_INFO, lc_text("이 개량서로 개량할 수 없습니다."));
                return false;
            }
            break;

        case BDRAGON_SCROLL:
            if (item->type() != ITEM_METIN || item->refine_level() != 4) {
                chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템으로 개량할 수 없습니다."));
                return false;
            }
            break;
        }

        item_table *proto_value = item_manager::instance().table(item->refined_vnum());

        if (!proto_value) {
            metin2::log::error("No proto for refined item vnum {:d}", item->refined_vnum());
            chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템은 개량할 수 없습니다."));
            return false;
        }

        if (gold() < prt->cost) {
            chat_packet(CHAT_TYPE_INFO, lc_text("개량을 하기 위한 돈이 부족합니다."));
            return false;
        }

        for (int i = 0; i < prt->material_count; ++i) {
            if (count_specify_item(prt->materials[i].vnum) < prt->materials[i].count) {
                if (metin2::config::test_server()) {
                    chat_packet(CHAT_TYPE_INFO, "Find %d, count %d, require %d", prt->materials[i].vnum, count_specify_item(prt->materials[i].vnum), prt->materials[i].count);
                }
                chat_packet(CHAT_TYPE_INFO, lc_text("개량을 하기 위한 재료가 부족합니다."));
                return false;
            }
        }

        for (int i = 0; i < prt->material_count; ++i)
            remove_specify_item(prt->materials[i].vnum, prt->materials[i].count);

        int prob = number(1, 100);
        int success_prob = prt->prob;
        bool destroy_when_fail = false;

        const char *refine_type = "SCROLL";

        switch (scroll_type) {
        case HYUNIRON_CHN:
            metin2::log::error("Unknown refine scroll type {:d}", scroll_type);
            destroy_when_fail = true;
            refine_type = "HYUNIRON";
            break;

        case YONGSIN_SCROLL:
            success_prob = hyuniron_refine_prob[std::clamp(item->refine_level(), 0, 8)];
            refine_type = "GOD_SCROLL";
            break;

        case YAGONG_SCROLL:
            success_prob = yagong_refine_prob[std::clamp(item->refine_level(), 0, 8)];
            refine_type = "YAGONG_SCROLL";
            break;

        case MUSIN_SCROLL:
            success_prob = 100;
            refine_type = "MUSIN_SCROLL";
            break;

        case MEMO_SCROLL:
            success_prob = 100;
            refine_type = "MEMO_SCROLL";
            break;

        case BDRAGON_SCROLL:
            success_prob = 80;
            refine_type = "BDRAGON_SCROLL";
            break;
        }

        if (metin2::config::test_server() && (scroll_type == HYUNIRON_CHN || scroll_type == YONGSIN_SCROLL || scroll_type == YAGONG_SCROLL))
            chat_packet(CHAT_TYPE_INFO, "[Only Test] Success_Prob %d, RefineLevel %d ", success_prob, item->refine_level());

        item_scroll->set_count(item_scroll->count() - 1);

        if (prob <= success_prob) {
            if (!replace_refined_item(item, result_vnum, prt->cost, true, refine_type))
                notify_refine_fail(this, item, refine_type);
        } else if (!destroy_when_fail && result_fail_vnum) {
            if (!replace_refined_item(item, result_fail_vnum, prt->cost, false, refine_type))
                notify_refine_fail(this, item, refine_type);
        } else {
            notify_refine_fail(this, item, refine_type);

            pay_refine_fee(prt->cost);
        }

        return true;
    }

    auto character::refine_information(uint8_t cell, uint8_t type_value, int additional_cell) -> bool {
        if (cell > INVENTORY_MAX_NUM)
            return false;

        item *item = inventory_item(cell);

        if (!item)
            return false;

        if (type_value == REFINE_TYPE_MONEY_ONLY && !quest_flag("deviltower_zone.can_refine")) {
            chat_packet(CHAT_TYPE_INFO, lc_text("사귀 타워 완료 보상은 한번까지 사용가능합니다."));
            return false;
        }

        packet_gc_refine_information p;

        p.header = HEADER_GC_REFINE_INFORMATION;
        p.pos = cell;
        p.src_vnum = item->vnum();
        p.result_vnum = item->refined_vnum();
        p.type = type_value;

        if (p.result_vnum == 0) {
            metin2::log::error("Refine information: item has no refine result");
            chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템은 개량할 수 없습니다."));
            return false;
        }

        if (item->type() == ITEM_USE && item->sub_type() == USE_TUNING) {
            if (type_value == 0) {
                chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템은 이 방식으로는 개량할 수 없습니다."));
                return false;
            } else {
                game::items::item *item_scroll = inventory_item(additional_cell);
                if (!item_scroll || item->vnum() == item_scroll->vnum()) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("같은 개량서를 합칠 수는 없습니다."));
                    chat_packet(CHAT_TYPE_INFO, lc_text("축복의 서와 현철을 합칠 수 있습니다."));
                    return false;
                }
            }
        }

        refine_manager &rm = refine_manager::instance();

        const refine_table *prt = rm.refine_recipe(item->refine_set());

        if (!prt) {
            metin2::log::error("Refine set {:d} not found", item->refine_set());
            chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템은 개량할 수 없습니다."));
            return false;
        }

        if (quest_flag("main_quest_lv7.refine_chance") > 0) {
            if (!item->check_item_use_level(20) || item->type() != ITEM_WEAPON) {
                chat_packet(CHAT_TYPE_INFO, lc_text("무료 개량 기회는 20 이하의 무기만 가능합니다"));
                return false;
            }
            p.cost = 0;
        } else
            p.cost = compute_refine_fee(prt->cost);

        p.prob = prt->prob;
        if (type_value == REFINE_TYPE_MONEY_ONLY) {
            p.material_count = 0;
            memset(p.materials, 0, sizeof(p.materials));
        } else {
            p.material_count = prt->material_count;
            std::memcpy(&p.materials, prt->materials, sizeof(prt->materials));
        }

        get_desc()->packet(&p, sizeof(packet_gc_refine_information));

        set_refine_mode(additional_cell);
        return true;
    }

    auto character::refine_item(item *item_value, item *target) -> bool {
        if (!can_handle_item())
            return false;

        if (item_value->sub_type() == USE_TUNING) {
            if (item_value->value(0) == MUSIN_SCROLL)
                refine_information(target->cell(), REFINE_TYPE_MUSIN, item_value->cell());
            else if (item_value->value(0) == HYUNIRON_CHN)
                refine_information(target->cell(), REFINE_TYPE_HYUNIRON, item_value->cell());
            else if (item_value->value(0) == BDRAGON_SCROLL) {
                if (target->refine_set() != 702) return false;
                refine_information(target->cell(), REFINE_TYPE_BDRAGON, item_value->cell());
            } else {
                if (target->refine_set() == 501) return false;
                refine_information(target->cell(), REFINE_TYPE_SCROLL, item_value->cell());
            }
        } else if (item_value->sub_type() == USE_DETACHMENT && is_set(target->flag(), ITEM_FLAG_REFINEABLE)) {
            log_manager::instance().item_log(this, target, "USE_DETACHMENT", target->name());

            bool has_metin_stone = false;

            for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++) {
                int32_t socket = target->get_socket(i);
                if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM) {
                    has_metin_stone = true;
                    break;
                }
            }

            if (has_metin_stone) {
                for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i) {
                    int32_t socket = target->get_socket(i);
                    if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM) {
                        auto_give_item(socket);
                        target->set_socket(i, ITEM_BROKEN_METIN_VNUM);
                    }
                }
                item_value->set_count(item_value->count() - 1);
                return true;
            } else {
                chat_packet(CHAT_TYPE_INFO, lc_text("빼낼 수 있는 메틴석이 없습니다."));
                return false;
            }
        }

        return false;
    }

    EVENTFUNC(kill_campfire_event) {
        auto *info = dynamic_cast<char_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Campfire kill event fired without event info");
            return 0;
        }

        character *ch = info->ch;

        if (ch == nullptr) {
            return 0;
        }
        ch->mining_event = nullptr;
        character_manager::instance().destroy_character(ch);
        return 0;
    }

    auto character::give_recall_item(item *item) -> bool {
        int idx = map_index();
        int empire_by_map_index = -1;

        if (idx < 20)
            empire_by_map_index = 1;
        else if (idx < 40)
            empire_by_map_index = 2;
        else if (idx < 60)
            empire_by_map_index = 3;
        else if (idx < 10000)
            empire_by_map_index = 0;

        switch (idx) {
        case 66:
        case 216:
            empire_by_map_index = -1;
            break;
        }

        if (empire_by_map_index && empire() != empire_by_map_index) {
            chat_packet(CHAT_TYPE_INFO, lc_text("기억해 둘 수 없는 위치 입니다."));
            return false;
        }

        int pos;

        if (item->count() == 1) {
            item->set_socket(0, x());
            item->set_socket(1, y());
        } else if ((pos = empty_inventory(item->size())) != -1) {
            game::items::item *item2 = item_manager::instance().create_item(item->vnum(), 1);

            if (nullptr != item2) {
                item2->set_socket(0, x());
                item2->set_socket(1, y());
                item2->add_to_character(this, item_pos(INVENTORY, pos));

                item->set_count(item->count() - 1);
            }
        } else {
            chat_packet(CHAT_TYPE_INFO, lc_text("소지품에 빈 공간이 없습니다."));
            return false;
        }

        return true;
    }

    void character::process_recall_item(item *item) {
        int idx;

        if ((idx = sectree_manager::instance().map_index(item->get_socket(0), item->get_socket(1))) == 0)
            return;

        int empire_by_map_index = -1;

        if (idx < 20)
            empire_by_map_index = 1;
        else if (idx < 40)
            empire_by_map_index = 2;
        else if (idx < 60)
            empire_by_map_index = 3;
        else if (idx < 10000)
            empire_by_map_index = 0;

        switch (idx) {
        case 66:
        case 216:
            empire_by_map_index = -1;
            break;
        case 301:
        case 302:
        case 303:
        case 304:
            if (level() < 90) {
                chat_packet(CHAT_TYPE_INFO, lc_text("아이템의 레벨 제한보다 레벨이 낮습니다."));
                return;
            } else
                break;
        }

        if (empire_by_map_index && empire() != empire_by_map_index) {
            chat_packet(CHAT_TYPE_INFO, lc_text("기억된 위치가 타제국에 속해 있어서 귀환할 수 없습니다."));
            item->set_socket(0, 0);
            item->set_socket(1, 0);
        } else {
            metin2::log::debug("{} recalls from {:d} {:d} to {:d} {:d}", name(), x(), y(), item->get_socket(0), item->get_socket(1));
            warp_set(item->get_socket(0), item->get_socket(1));
            item->set_count(item->count() - 1);
        }
    }

    void character::open_private_shop() {
        unsigned body_part = part(PART_MAIN);
        switch (body_part) {
        case 0:
        case 1:
        case 2:
            chat_packet(CHAT_TYPE_COMMAND, "OpenPrivateShop");
            break;
        default:
            chat_packet(CHAT_TYPE_INFO, lc_text("갑옷을 벗어야 개인 상점을 열 수 있습니다."));
            break;
        }
    }

    void character::send_my_shop_price_list_cmd(uint32_t item_vnum, uint32_t item_price) {
        const std::string line = std::format("MyShopPriceList {} {}", item_vnum, item_price);
        chat_packet(CHAT_TYPE_COMMAND, "%s", line.c_str());
        metin2::log::debug("Shop price list command: {}", line);
    }

    void character::use_silk_botary_real(const packet_myshop_pricelist_header *p) {
        const auto *info = (const item_price_info *)(p + 1);

        if (!p->count)
            send_my_shop_price_list_cmd(1, 0);
        else {
            for (int idx = 0; idx < p->count; idx++)
                send_my_shop_price_list_cmd(info[idx].vnum, info[idx].price);
        }

        open_private_shop();
    }

    void character::use_silk_botary(void) {
        if (no_opened_shop_) {
            uint32_t player_id_value = player_id();
            db_clientdesc->db_packet(HEADER_GD_MYSHOP_PRICELIST_REQ, get_desc()->handle(), &player_id_value, sizeof(uint32_t));
            no_opened_shop_ = false;
        } else {
            open_private_shop();
        }
    }

    auto calculate_consume(character *ch) -> int {
        static const int warp_need_life_percent = 30;
        static const int warp_min_life_percent = 10;
        int consume_life = 0;
        {
            const int cur_life = ch->hp();
            const int need_percent = warp_need_life_percent;
            const int need_life = ch->max_hp() * need_percent / 100;
            if (cur_life < need_life) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("남은 생명력 양이 모자라 사용할 수 없습니다."));
                return -1;
            }

            consume_life = need_life;

            const int min_percent = warp_min_life_percent;
            const int min_life = ch->max_hp() * min_percent / 100;
            if (cur_life - need_life < min_life)
                consume_life = cur_life - min_life;

            if (consume_life < 0)
                consume_life = 0;
        }
        return consume_life;
    }

    auto calculate_consume_sp(character *ch) -> int {
        static const int need_warp_sp_percent = 30;

        const int cur_sp = ch->sp();
        const int need_sp = ch->max_sp() * need_warp_sp_percent / 100;

        if (cur_sp < need_sp) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("남은 정신력 양이 모자라 사용할 수 없습니다."));
            return -1;
        }

        return need_sp;
    }

    auto character::use_item_ex(item *item, item_pos dest_cell) -> bool {
        int limit_realtime_start_first_use_flag_index = -1;

        uint16_t dest_cell_value = dest_cell.cell;
        uint8_t dest_inven = dest_cell.window_type;
        for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i) {
            int32_t limit_value = item->proto()->limits[i].value;

            switch (item->proto()->limits[i].type) {
            case LIMIT_LEVEL:
                if (level() < limit_value) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("아이템의 레벨 제한보다 레벨이 낮습니다."));
                    return false;
                }
                break;

            case LIMIT_REAL_TIME_START_FIRST_USE:
                limit_realtime_start_first_use_flag_index = i;
                break;

            case LIMIT_TIMER_BASED_ON_WEAR:
                break;
            }
        }

        if (metin2::config::test_server()) {
            metin2::log::debug("Use item {}: window {:d}, cell {:d}, type {:d}, subtype {:d}", item->name(), dest_inven, dest_cell_value, item->type(), item->sub_type());
        }

        if (arena_manager::instance().is_limited_item(map_index(), item->vnum()) == true) {
            chat_packet(CHAT_TYPE_INFO, lc_text("대련 중에는 이용할 수 없는 물품입니다."));
            return false;
        }

        if (-1 != limit_realtime_start_first_use_flag_index) {
            if (0 == item->get_socket(1)) {
                int32_t duration = (0 != item->get_socket(0)) ? item->get_socket(0) : item->proto()->limits[limit_realtime_start_first_use_flag_index].value;

                if (0 == duration)
                    duration = 60 * 60 * 24 * 7;

                item->set_socket(0, get_global_time() + duration);
                item->start_real_time_expire_event();
            }

            if (false == item->is_equipped())
                item->set_socket(1, item->get_socket(1) + 1);
        }

        switch (item->type()) {
        case ITEM_HAIR:
            return item_process_hair(item, dest_cell_value);

        case ITEM_POLYMORPH:
            return item_process_polymorph(item);

        case ITEM_QUEST:
            return use_item_quest(item);

        case ITEM_CAMPFIRE:
            return use_item_campfire(item);

        case ITEM_UNIQUE:
            return use_item_unique(item);

        case ITEM_COSTUME:
        case ITEM_WEAPON:
        case ITEM_ARMOR:
        case ITEM_ROD:
        case ITEM_RING:
        case ITEM_BELT:
        case ITEM_PICK:
            if (!item->is_equipped())
                equip_item(item);
            else
                unequip_item(item);
            break;
        case ITEM_DS:
            {
                if (!item->is_equipped())
                    return false;
                return ds_manager::instance().pull_out(this, NPOS, item);
                break;
            }
        case ITEM_SPECIAL_DS:
            if (!item->is_equipped())
                equip_item(item);
            else
                unequip_item(item);
            break;

        case ITEM_FISH:
            {
                if (arena_manager::instance().is_arena_map(map_index()) == true) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("대련 중에는 이용할 수 없는 물품입니다."));
                    return false;
                }

                if (item->sub_type() == FISH_ALIVE)
                    fishing::use_fish(this, item);
            }
            break;

        case ITEM_TREASURE_BOX:
            {
                return false;
            }
            break;

        case ITEM_TREASURE_KEY:
            return use_item_treasure_key(item, dest_cell);

        case ITEM_GIFTBOX:
            return use_item_giftbox(item);

        case ITEM_SKILLFORGET:
            return use_item_skill_forget(item);

        case ITEM_SKILLBOOK:
            return use_item_skillbook(item);

        case ITEM_USE:
            return use_item_use(item, dest_cell);

        case ITEM_METIN:
            return use_item_metin(item, dest_cell);

        case ITEM_AUTOUSE:
        case ITEM_MATERIAL:
        case ITEM_SPECIAL:
        case ITEM_TOOL:
        case ITEM_LOTTERY:
            break;

        case ITEM_TOTEM:
            {
                if (!item->is_equipped())
                    equip_item(item);
            }
            break;

        case ITEM_BLEND:
            return use_item_blend(item);

        case ITEM_EXTRACT:
            return use_item_extract(item, dest_cell);

        case ITEM_NONE:
            metin2::log::error("Item {} has type none", item->name());
            break;

        default:
            metin2::log::warn("Item {} has unknown type {:d}", item->name(), item->type());
            return false;
        }

        return true;
    }

    auto character::use_item_quest(item *item) -> bool {
        if (get_arena() != nullptr || is_observer_mode() == true) {
            if (item->vnum() == 50051 || item->vnum() == 50052 || item->vnum() == 50053) {
                chat_packet(CHAT_TYPE_INFO, lc_text("대련 중에는 이용할 수 없는 물품입니다."));
                return false;
            }
        }

        if (!is_set(item->flag(), ITEM_FLAG_QUEST_USE | ITEM_FLAG_QUEST_USE_MULTIPLE)) {
            if (item->sig_vnum() == 0) {
                quest::quest_manager::instance().use_item(player_id(), item, false);
            } else {
                quest::quest_manager::instance().sig_use(player_id(), item->sig_vnum(), item, false);
            }
        }

        return true;
    }

    auto character::use_item_campfire(item *item) -> bool {
        float fx, fy;
        get_delta_by_degree(rotation(), 100.0f, &fx, &fy);

        sectree *tree = sectree_manager::instance().get(map_index(), (int32_t)(x() + fx), (int32_t)(y() + fy));

        if (!tree) {
            chat_packet(CHAT_TYPE_INFO, lc_text("모닥불을 피울 수 없는 지점입니다."));
            return false;
        }

        if (tree->is_attr((int32_t)(x() + fx), (int32_t)(y() + fy), ATTR_WATER)) {
            chat_packet(CHAT_TYPE_INFO, lc_text("물 속에 모닥불을 피울 수 없습니다."));
            return false;
        }

        character *campfire = character_manager::instance().spawn_mob(fishing::CAMPFIRE_MOB, map_index(), (int32_t)(x() + fx), (int32_t)(y() + fy), 0, false, number(0, 359));

        auto *info = alloc_event_info<char_event_info>();

        info->ch = campfire;

        campfire->mining_event = event_create(kill_campfire_event, info, passes_per_sec(40));

        item->set_count(item->count() - 1);

        return true;
    }

    auto character::use_item_unique(item *item) -> bool {
        switch (item->sub_type()) {
        case USE_ABILITY_UP:
            add_ability_affect(AFFECT_UNIQUE_ABILITY, item->value(0), item->value(2), item->value(1), true);
            consume_potion(item);
            break;

        default:
            if (item->sub_type() == USE_SPECIAL) {
                metin2::log::debug("Unique item {:d}: special use", item->vnum());

                switch (item->vnum()) {
                case 71049:
                    use_silk_botary();
                    break;
                }
            } else {
                if (!item->is_equipped())
                    equip_item(item);
                else
                    unequip_item(item);
            }
            break;
        }

        return true;
    }

    auto character::use_item_treasure_key(item *item, item_pos dest_cell) -> bool {
        game::items::item *item2;

        if (!get_item(dest_cell) || !(item2 = get_item(dest_cell)))
            return false;

        if (item2->is_exchanging())
            return false;

        if (item2->type() != ITEM_TREASURE_BOX) {
            chat_packet(CHAT_TYPE_TALKING, lc_text("열쇠로 여는 물건이 아닌것 같다."));
            return false;
        }

        if (item->value(0) == item2->value(0)) {
            uint32_t box_vnum = item2->vnum();
            std::vector<uint32_t> vnums;
            std::vector<uint32_t> counts;
            std::vector<game::items::item *> item_gets;
            int count = 0;

            if (give_item_from_special_item_group(box_vnum, vnums, counts, item_gets, count)) {
                item_manager::instance().remove_item(item);
                item_manager::instance().remove_item(item2);

                for (int i = 0; i < count; i++) {
                    notify_box_reward(vnums[i], counts[i], item_gets[i]);
                }
            } else {
                chat_packet(CHAT_TYPE_TALKING, lc_text("열쇠가 맞지 않는 것 같다."));
                return false;
            }
        } else {
            chat_packet(CHAT_TYPE_TALKING, lc_text("열쇠가 맞지 않는 것 같다."));
            return false;
        }

        return true;
    }

    auto character::use_item_giftbox(item *item) -> bool {
        uint32_t box_vnum = item->vnum();
        std::vector<uint32_t> vnums;
        std::vector<uint32_t> counts;
        std::vector<game::items::item *> item_gets;
        int count = 0;

        if ((box_vnum > 51500 && box_vnum < 52000) || (box_vnum >= 50255 && box_vnum <= 50260)) {
            if (!(this->dragon_soul_is_qualified())) {
                chat_packet(CHAT_TYPE_INFO, lc_text("먼저 용혼석 퀘스트를 완료하셔야 합니다."));
                return false;
            }
        }

        if (give_item_from_special_item_group(box_vnum, vnums, counts, item_gets, count)) {
            item->set_count(item->count() - 1);

            for (int i = 0; i < count; i++) {
                notify_box_reward(vnums[i], counts[i], item_gets[i]);
            }
        } else {
            chat_packet(CHAT_TYPE_TALKING, lc_text("아무것도 얻을 수 없었습니다."));
            return false;
        }

        return true;
    }

    auto character::use_item_skill_forget(item *item) -> bool {
        if (!item->get_socket(0)) {
            item_manager::instance().remove_item(item);
            return false;
        }

        uint32_t vnum = item->get_socket(0);

        if (skill_level_down(vnum)) {
            item_manager::instance().remove_item(item);
            chat_packet(CHAT_TYPE_INFO, lc_text("스킬 레벨을 내리는데 성공하였습니다."));
        } else
            chat_packet(CHAT_TYPE_INFO, lc_text("스킬 레벨을 내릴 수 없습니다."));

        return true;
    }

    auto character::use_item_skillbook(item *item) -> bool {
        if (is_polymorphed()) {
            chat_packet(CHAT_TYPE_INFO, lc_text("변신중에는 책을 읽을수 없습니다."));
            return false;
        }

        uint32_t vnum = 0;

        if (item->vnum() == 50300) {
            vnum = item->get_socket(0);
        } else {
            vnum = item->value(0);
        }

        if (0 == vnum) {
            item_manager::instance().remove_item(item);

            return false;
        }

        if (true == learn_skill_by_book(vnum)) {
            item_manager::instance().remove_item(item);

            int read_delay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);

            if (metin2::config::game().distribution_test_server)
                read_delay /= 3;

            set_skill_next_read_time(vnum, get_global_time() + read_delay);
        }

        return true;
    }

    auto character::use_item_use(item *item, item_pos dest_cell) -> bool {
        if (item->vnum() > 50800 && item->vnum() <= 50820)
            return use_item_euro_free(item);

        if (item->vnum() >= 27863 && item->vnum() <= 27883) {
            if (arena_manager::instance().is_arena_map(map_index()) == true) {
                chat_packet(CHAT_TYPE_INFO, lc_text("대련 중에는 이용할 수 없는 물품입니다."));
                return false;
            }
        }

        if (metin2::config::test_server()) {
            metin2::log::debug("Use item {}: type {:d}, subtype {:d}, vnum {:d}", item->name(), item->type(), item->sub_type(), item->original_vnum());
        }

        switch (item->sub_type()) {
        case USE_TIME_CHARGE_PER:
            return use_item_use_time_charge_per(item, dest_cell);

        case USE_TIME_CHARGE_FIX:
            return use_item_use_time_charge_fix(item, dest_cell);

        case USE_SPECIAL:
            return use_item_use_special(item, dest_cell);

        case USE_CLEAR:
            remove_bad_affect();
            item->set_count(item->count() - 1);
            break;

        case USE_INVISIBILITY:
            return use_item_use_invisibility(item);

        case USE_POTION_NODELAY:
            return use_item_use_potion_nodelay(item);

        case USE_POTION:
            return use_item_use_potion(item);

        case USE_POTION_CONTINUE:
            return use_item_use_potion_continue(item);

        case USE_ABILITY_UP:
            if (const uint32_t affect_type = ability_potion_affect_type(item->value(0)); affect_type != AFFECT_NONE)
                add_ability_affect(affect_type, item->value(0), item->value(2), item->value(1), false);

            consume_potion(item);
            break;

        case USE_TALISMAN:
            return use_item_use_talisman(item);

        case USE_TUNING:
        case USE_DETACHMENT:
            return use_item_use_tuning(item, dest_cell);

        case USE_PUT_INTO_BELT_SOCKET:
        case USE_PUT_INTO_RING_SOCKET:
        case USE_PUT_INTO_ACCESSORY_SOCKET:
        case USE_ADD_ACCESSORY_SOCKET:
        case USE_CLEAN_SOCKET:
        case USE_CHANGE_ATTRIBUTE:
        case USE_CHANGE_ATTRIBUTE2:
        case USE_ADD_ATTRIBUTE:
        case USE_ADD_ATTRIBUTE2:
            return use_item_use_socket_or_attribute(item, dest_cell);

        case USE_BAIT:
            return use_item_use_bait(item);

        case USE_MOVE:
        case USE_TREASURE_BOX:
        case USE_MONEYBAG:
            break;

        case USE_AFFECT:
            return use_affect_item(item, item->value(0), false);

        case USE_CREATE_STONE:
            auto_give_item(number(28000, 28013));
            item->set_count(item->count() - 1);
            break;

        case USE_RECIPE:
            return use_item_use_recipe(item);
        }

        return true;
    }

    auto character::use_item_euro_free(item *item) -> bool {
        if (metin2::config::test_server())
            metin2::log::debug("Additional effect item {:d} (subtype {:d})", item->original_vnum(), item->sub_type());

        if (item->value(0) < 0 || item->value(0) >= static_cast<int32_t>(std::size(a_apply_info))) {
            metin2::log::error("Item {:d} has invalid apply type {:d}", item->original_vnum(), item->value(0));
            return false;
        }

        switch (item->sub_type()) {
        case USE_ABILITY_UP:
            if (find_affect(AFFECT_EXP_BONUS_EURO_FREE, a_apply_info[item->value(0)].point_type)) {
                notify_already_affected();
                return false;
            }

            add_ability_affect(AFFECT_EXP_BONUS_EURO_FREE, item->value(0), item->value(2), item->value(1), true);
            consume_potion(item);
            break;

        case USE_AFFECT:
            return use_affect_item(item, AFFECT_EXP_BONUS_EURO_FREE, true);

        case USE_POTION_NODELAY:
            return use_item_use_potion_nodelay(item);
        }

        return true;
    }

    auto character::use_item_use_time_charge_per(item *item, item_pos dest_cell) -> bool {
        game::items::item *dest_item = get_item(dest_cell);
        if (nullptr == dest_item) {
            return false;
        }
        if (dest_item->is_dragon_soul()) {
            int ret;
            std::string buf;
            if (item->vnum() == DRAGON_HEART_VNUM) {
                ret = dest_item->give_more_time_per((float)item->get_socket(ITEM_SOCKET_CHARGING_AMOUNT_IDX));
            } else {
                ret = dest_item->give_more_time_per((float)item->value(ITEM_VALUE_CHARGING_AMOUNT_IDX));
            }
            if (ret > 0) {
                if (item->vnum() == DRAGON_HEART_VNUM) {
                    buf = std::format("Inc {}s by item{{VN:{} SOC{}:{}}}", ret, item->vnum(), ITEM_SOCKET_CHARGING_AMOUNT_IDX, item->get_socket(ITEM_SOCKET_CHARGING_AMOUNT_IDX));
                } else {
                    buf = std::format("Inc {}s by item{{VN:{} VAL{}:{}}}", ret, item->vnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->value(ITEM_VALUE_CHARGING_AMOUNT_IDX));
                }

                chat_packet(CHAT_TYPE_INFO, lc_text("%d초 만큼 충전되었습니다."), ret);
                item->set_count(item->count() - 1);
                log_manager::instance().item_log(this, item, "DS_CHARGING_SUCCESS", buf.c_str());
                return true;
            } else {
                if (item->vnum() == DRAGON_HEART_VNUM) {
                    buf = std::format("No change by item{{VN:{} SOC{}:{}}}", item->vnum(), ITEM_SOCKET_CHARGING_AMOUNT_IDX, item->get_socket(ITEM_SOCKET_CHARGING_AMOUNT_IDX));
                } else {
                    buf = std::format("No change by item{{VN:{} VAL{}:{}}}", item->vnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->value(ITEM_VALUE_CHARGING_AMOUNT_IDX));
                }

                chat_packet(CHAT_TYPE_INFO, lc_text("충전할 수 없습니다."));
                log_manager::instance().item_log(this, item, "DS_CHARGING_FAILED", buf.c_str());
                return false;
            }
        } else
            return false;
    }

    auto character::use_item_use_time_charge_fix(item *item, item_pos dest_cell) -> bool {
        game::items::item *dest_item = get_item(dest_cell);
        if (nullptr == dest_item) {
            return false;
        }
        if (dest_item->is_dragon_soul()) {
            int ret = dest_item->give_more_time_fix(item->value(ITEM_VALUE_CHARGING_AMOUNT_IDX));
            std::string buf;
            if (ret) {
                chat_packet(CHAT_TYPE_INFO, lc_text("%d초 만큼 충전되었습니다."), ret);
                buf = std::format("Increase {}s by item{{VN:{} VAL{}:{}}}", ret, item->vnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->value(ITEM_VALUE_CHARGING_AMOUNT_IDX));
                log_manager::instance().item_log(this, item, "DS_CHARGING_SUCCESS", buf.c_str());
                item->set_count(item->count() - 1);
                return true;
            } else {
                chat_packet(CHAT_TYPE_INFO, lc_text("충전할 수 없습니다."));
                buf = std::format("No change by item{{VN:{} VAL{}:{}}}", item->vnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->value(ITEM_VALUE_CHARGING_AMOUNT_IDX));
                log_manager::instance().item_log(this, item, "DS_CHARGING_FAILED", buf.c_str());
                return false;
            }
        } else
            return false;
    }

    auto character::use_item_use_special(item *item, item_pos dest_cell) -> bool {
        switch (item->vnum()) {
        case ITEM_NOG_POCKET:
            {
                if (find_affect(AFFECT_NOG_ABILITY)) {
                    notify_already_affected();
                    return false;
                }
                int32_t time = item->value(0);
                int32_t move_speed_per = item->value(1);
                int32_t att_per = item->value(2);
                int32_t exp_per = item->value(3);
                add_affect(AFFECT_NOG_ABILITY, POINT_MOV_SPEED, move_speed_per, AFF_MOV_SPEED_POTION, time, 0, true, true);
                add_affect(AFFECT_NOG_ABILITY, POINT_MALL_ATTBONUS, att_per, AFF_NONE, time, 0, true, true);
                add_affect(AFFECT_NOG_ABILITY, POINT_MALL_EXPBONUS, exp_per, AFF_NONE, time, 0, true, true);
                item->set_count(item->count() - 1);
            }
            break;

        case ITEM_RAMADAN_CANDY:
            {
                int32_t time = item->value(0);
                int32_t move_speed_per = item->value(1);
                int32_t att_per = item->value(2);
                int32_t exp_per = item->value(3);
                add_affect(AFFECT_RAMADAN_ABILITY, POINT_MOV_SPEED, move_speed_per, AFF_MOV_SPEED_POTION, time, 0, true, true);
                add_affect(AFFECT_RAMADAN_ABILITY, POINT_MALL_ATTBONUS, att_per, AFF_NONE, time, 0, true, true);
                add_affect(AFFECT_RAMADAN_ABILITY, POINT_MALL_EXPBONUS, exp_per, AFF_NONE, time, 0, true, true);
                item->set_count(item->count() - 1);
            }
            break;
        case ITEM_MARRIAGE_RING:
            {
                marriage::marriage *marriage = marriage::manager::instance().get(player_id());
                if (marriage) {
                    if (marriage->ch1 != nullptr) {
                        if (arena_manager::instance().is_arena_map(marriage->ch1->map_index()) == true) {
                            chat_packet(CHAT_TYPE_INFO, lc_text("대련 중에는 이용할 수 없는 물품입니다."));
                            break;
                        }
                    }

                    if (marriage->ch2 != nullptr) {
                        if (arena_manager::instance().is_arena_map(marriage->ch2->map_index()) == true) {
                            chat_packet(CHAT_TYPE_INFO, lc_text("대련 중에는 이용할 수 없는 물품입니다."));
                            break;
                        }
                    }

                    int consume_sp = calculate_consume_sp(this);

                    if (consume_sp < 0)
                        return false;

                    point_change(POINT_SP, -consume_sp, false);

                    warp_to_pid(marriage->other(player_id()));
                } else
                    chat_packet(CHAT_TYPE_INFO, lc_text("결혼 상태가 아니면 결혼반지를 사용할 수 없습니다."));
            }
            break;

        case UNIQUE_ITEM_CAPE_OF_COURAGE:
        case 70057:
        case REWARD_BOX_UNIQUE_ITEM_CAPE_OF_COURAGE:
            aggregate_monster();
            item->set_count(item->count() - 1);
            break;

        case UNIQUE_ITEM_WHITE_FLAG:
            forget_my_attacker();
            item->set_count(item->count() - 1);
            break;

        case UNIQUE_ITEM_TREASURE_BOX:
            break;

        case 30093:
        case 30094:
        case 30095:
        case 30096:
            return use_item_lucky_bag(item);

        case 50004:
            return use_item_event_stone_detector(item);

        case 27989:
        case 76006:
            return use_item_stone_detector(item);

        case 27996:
            item->set_count(item->count() - 1);
            break;

        case 27987:
            {
                item->set_count(item->count() - 1);

                int r = number(1, 100);

                if (r <= 50) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("조개에서 돌조각이 나왔습니다."));
                    auto_give_item(27990);
                } else {
                    const int prob_table[] =
                        {
                            95, 97, 99};

                    if (r <= prob_table[0]) {
                        chat_packet(CHAT_TYPE_INFO, lc_text("조개가 흔적도 없이 사라집니다."));
                    } else if (r <= prob_table[1]) {
                        chat_packet(CHAT_TYPE_INFO, lc_text("조개에서 백진주가 나왔습니다."));
                        auto_give_item(27992);
                    } else if (r <= prob_table[2]) {
                        chat_packet(CHAT_TYPE_INFO, lc_text("조개에서 청진주가 나왔습니다."));
                        auto_give_item(27993);
                    } else {
                        chat_packet(CHAT_TYPE_INFO, lc_text("조개에서 피진주가 나왔습니다."));
                        auto_give_item(27994);
                    }
                }
            }
            break;

        case 71013:
            create_fly(number(FLY_FIREWORK1, FLY_FIREWORK6), this);
            item->set_count(item->count() - 1);
            break;

        case 50100:
        case 50101:
        case 50102:
        case 50103:
        case 50104:
        case 50105:
        case 50106:
            create_fly(item->vnum() - 50100 + FLY_FIREWORK1, this);
            item->set_count(item->count() - 1);
            break;

        case 50200:
            open_private_shop();
            break;

        case fishing::FISH_MIND_PILL_VNUM:
            add_affect(AFFECT_FISH_MIND_PILL, POINT_NONE, 0, AFF_FISH_MIND, 20 * 60, 0, true);
            item->set_count(item->count() - 1);
            break;

        case 50301:
        case 50302:
        case 50303:
            {
                if (is_polymorphed() == true) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("둔갑 중에는 능력을 올릴 수 없습니다."));
                    return false;
                }

                int lv = skill_level(SKILL_LEADERSHIP);

                if (lv < item->value(0)) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("이 책은 너무 어려워 이해하기가 힘듭니다."));
                    return false;
                }

                if (lv >= item->value(1)) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("이 책은 아무리 봐도 도움이 될 것 같지 않습니다."));
                    return false;
                }

                if (learn_skill_by_book(SKILL_LEADERSHIP)) {
                    item_manager::instance().remove_item(item);

                    int read_delay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
                    if (metin2::config::game().distribution_test_server) read_delay /= 3;

                    set_skill_next_read_time(SKILL_LEADERSHIP, get_global_time() + read_delay);
                }
            }
            break;

        case 50304:
        case 50305:
        case 50306:
            {
                if (is_polymorphed()) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("변신중에는 책을 읽을수 없습니다."));
                    return false;
                }
                if (skill_level(SKILL_COMBO) == 0 && level() < 30) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("레벨 30이 되기 전에는 습득할 수 있을 것 같지 않습니다."));
                    return false;
                }

                if (skill_level(SKILL_COMBO) == 1 && level() < 50) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("레벨 50이 되기 전에는 습득할 수 있을 것 같지 않습니다."));
                    return false;
                }

                if (skill_level(SKILL_COMBO) >= 2) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("연계기는 더이상 수련할 수 없습니다."));
                    return false;
                }

                int pct = item->value(0);

                if (learn_skill_by_book(SKILL_COMBO, pct)) {
                    item_manager::instance().remove_item(item);

                    int read_delay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
                    if (metin2::config::game().distribution_test_server) read_delay /= 3;

                    set_skill_next_read_time(SKILL_COMBO, get_global_time() + read_delay);
                }
            }
            break;
        case 50311:
        case 50312:
        case 50313:
            {
                if (is_polymorphed()) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("변신중에는 책을 읽을수 없습니다."));
                    return false;
                }
                uint32_t skill_vnum = item->value(0);
                int pct = std::clamp(item->value(1), 0, 100);
                if (skill_level(skill_vnum) >= 20 || skill_vnum - SKILL_LANGUAGE1 + 1 == empire()) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("이미 완벽하게 알아들을 수 있는 언어이다."));
                    return false;
                }

                if (learn_skill_by_book(skill_vnum, pct)) {
                    item_manager::instance().remove_item(item);

                    int read_delay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
                    if (metin2::config::game().distribution_test_server) read_delay /= 3;

                    set_skill_next_read_time(skill_vnum, get_global_time() + read_delay);
                }
            }
            break;

        case 50061:
            {
                if (is_polymorphed()) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("변신중에는 책을 읽을수 없습니다."));
                    return false;
                }
                uint32_t skill_vnum = item->value(0);
                int pct = std::clamp(item->value(1), 0, 100);

                if (skill_level(skill_vnum) >= 10) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("더 이상 수련할 수 없습니다."));
                    return false;
                }

                if (learn_skill_by_book(skill_vnum, pct)) {
                    item_manager::instance().remove_item(item);

                    int read_delay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
                    if (metin2::config::game().distribution_test_server) read_delay /= 3;

                    set_skill_next_read_time(skill_vnum, get_global_time() + read_delay);
                }
            }
            break;

        case 50314:
        case 50315:
        case 50316:
        case 50323:
        case 50324:
        case 50325:
        case 50326:
            {
                if (is_polymorphed() == true) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("둔갑 중에는 능력을 올릴 수 없습니다."));
                    return false;
                }

                int skill_level_low_limit = item->value(0);
                int skill_level_high_limit = item->value(1);
                int pct = std::clamp(item->value(2), 0, 100);
                int level_limit = item->value(3);
                uint32_t skill_vnum = 0;

                switch (item->vnum()) {
                case 50314:
                case 50315:
                case 50316:
                    skill_vnum = SKILL_POLYMORPH;
                    break;

                case 50323:
                case 50324:
                    skill_vnum = SKILL_ADD_HP;
                    break;

                case 50325:
                case 50326:
                    skill_vnum = SKILL_RESIST_PENETRATE;
                    break;

                default:
                    return false;
                }

                if (0 == skill_vnum)
                    return false;

                if (level() < level_limit) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("이 책을 읽으려면 레벨을 더 올려야 합니다."));
                    return false;
                }

                if (skill_level(skill_vnum) >= 40) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("더 이상 수련할 수 없습니다."));
                    return false;
                }

                if (skill_level(skill_vnum) < skill_level_low_limit) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("이 책은 너무 어려워 이해하기가 힘듭니다."));
                    return false;
                }

                if (skill_level(skill_vnum) >= skill_level_high_limit) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("이 책으로는 더 이상 수련할 수 없습니다."));
                    return false;
                }

                if (learn_skill_by_book(skill_vnum, pct)) {
                    item_manager::instance().remove_item(item);

                    int read_delay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
                    if (metin2::config::game().distribution_test_server) read_delay /= 3;

                    set_skill_next_read_time(skill_vnum, get_global_time() + read_delay);
                }
            }
            break;

        case 50902:
        case 50903:
        case 50904:
            {
                if (is_polymorphed()) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("변신중에는 책을 읽을수 없습니다."));
                    return false;
                }
                uint32_t skill_vnum = SKILL_CREATE;
                int pct = std::clamp(item->value(1), 0, 100);

                if (skill_level(skill_vnum) >= 40) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("더 이상 수련할 수 없습니다."));
                    return false;
                }

                if (learn_skill_by_book(skill_vnum, pct)) {
                    item_manager::instance().remove_item(item);

                    int read_delay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
                    if (metin2::config::game().distribution_test_server) read_delay /= 3;

                    set_skill_next_read_time(skill_vnum, get_global_time() + read_delay);

                    if (metin2::config::test_server()) {
                        chat_packet(CHAT_TYPE_INFO, "[TEST_SERVER] Success to learn skill ");
                    }
                } else {
                    if (metin2::config::test_server()) {
                        chat_packet(CHAT_TYPE_INFO, "[TEST_SERVER] Failed to learn skill ");
                    }
                }
            }
            break;

        case ITEM_MINING_SKILL_TRAIN_BOOK:
            {
                if (is_polymorphed()) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("변신중에는 책을 읽을수 없습니다."));
                    return false;
                }
                uint32_t skill_vnum = SKILL_MINING;
                int pct = std::clamp(item->value(1), 0, 100);

                if (skill_level(skill_vnum) >= 40) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("더 이상 수련할 수 없습니다."));
                    return false;
                }

                if (learn_skill_by_book(skill_vnum, pct)) {
                    item_manager::instance().remove_item(item);

                    int read_delay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
                    if (metin2::config::game().distribution_test_server) read_delay /= 3;

                    set_skill_next_read_time(skill_vnum, get_global_time() + read_delay);
                }
            }
            break;

        case ITEM_HORSE_SKILL_TRAIN_BOOK:
            {
                if (is_polymorphed()) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("변신중에는 책을 읽을수 없습니다."));
                    return false;
                }
                uint32_t skill_vnum = SKILL_HORSE;
                int pct = std::clamp(item->value(1), 0, 100);

                if (level() < 50) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("아직 승마 스킬을 수련할 수 있는 레벨이 아닙니다."));
                    return false;
                }

                if (!metin2::config::test_server() && get_global_time() < skill_next_read_time(skill_vnum)) {
                    if (find_affect(AFFECT_SKILL_NO_BOOK_DELAY)) {
                        remove_affect(AFFECT_SKILL_NO_BOOK_DELAY);
                        chat_packet(CHAT_TYPE_INFO, lc_text("주안술서를 통해 주화입마에서 빠져나왔습니다."));
                    } else {
                        skill_learn_wait_more_time_message(skill_next_read_time(skill_vnum) - get_global_time());
                        return false;
                    }
                }

                if (point(POINT_HORSE_SKILL) >= 20 ||
                    skill_level(SKILL_HORSE_WILDATTACK) + skill_level(SKILL_HORSE_CHARGE) + skill_level(SKILL_HORSE_ESCAPE) >= 60 ||
                    skill_level(SKILL_HORSE_WILDATTACK_RANGE) + skill_level(SKILL_HORSE_CHARGE) + skill_level(SKILL_HORSE_ESCAPE) >= 60) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("더 이상 승마 수련서를 읽을 수 없습니다."));
                    return false;
                }

                if (prob(pct)) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("승마 수련서를 읽어 승마 스킬 포인트를 얻었습니다."));
                    chat_packet(CHAT_TYPE_INFO, lc_text("얻은 포인트로는 승마 스킬의 레벨을 올릴 수 있습니다."));
                    point_change(POINT_HORSE_SKILL, 1);

                    int read_delay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
                    if (metin2::config::game().distribution_test_server) read_delay /= 3;

                    if (!metin2::config::test_server())
                        set_skill_next_read_time(skill_vnum, get_global_time() + read_delay);
                } else {
                    chat_packet(CHAT_TYPE_INFO, lc_text("승마 수련서 이해에 실패하였습니다."));
                }

                item_manager::instance().remove_item(item);
            }
            break;

        case 70102:
        case 70103:
            {
                if (alignment() >= 0)
                    return false;

                int delta = std::min(-alignment(), item->value(0));

                metin2::log::info("{}: alignment item, delta {:d}", name(), delta);

                update_alignment(delta);
                item->set_count(item->count() - 1);

                if (delta / 10 > 0) {
                    chat_packet(CHAT_TYPE_TALKING, lc_text("마음이 맑아지는군. 가슴을 짓누르던 무언가가 좀 가벼워진 느낌이야."));
                    chat_packet(CHAT_TYPE_INFO, lc_text("선악치가 %d 증가하였습니다."), delta / 10);
                }
            }
            break;

        case 71107:
            {
                int val = item->value(0);
                int interval = item->value(1);
                quest::pc *pc = quest::quest_manager::instance().get_pc(player_id());
                int last_use_time = pc->flag("mythical_peach.last_use_time");

                if (get_global_time() - last_use_time < interval * 60 * 60) {
                    if (metin2::config::test_server() == false) {
                        chat_packet(CHAT_TYPE_INFO, lc_text("아직 사용할 수 없습니다."));
                        return false;
                    } else {
                        chat_packet(CHAT_TYPE_INFO, lc_text("테스트 서버 시간제한 통과"));
                    }
                }

                if (alignment() == 200000) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("선악치를 더 이상 올릴 수 없습니다."));
                    return false;
                }

                if (200000 - alignment() < val * 10) {
                    val = (200000 - alignment()) / 10;
                }

                int old_alignment = alignment() / 10;

                update_alignment(val * 10);

                item->set_count(item->count() - 1);
                pc->set_flag("mythical_peach.last_use_time", get_global_time());

                chat_packet(CHAT_TYPE_TALKING, lc_text("마음이 맑아지는군. 가슴을 짓누르던 무언가가 좀 가벼워진 느낌이야."));
                chat_packet(CHAT_TYPE_INFO, lc_text("선악치가 %d 증가하였습니다."), val);

                const std::string buf = std::format("{} {}", old_alignment, alignment() / 10);
                log_manager::instance().char_log(this, val, "MYTHICAL_PEACH", buf.c_str());
            }
            break;

        case 71109:
        case 72719:
            {
                game::items::item *item2;

                if (!is_valid_item_position(dest_cell) || !(item2 = get_item(dest_cell)))
                    return false;

                if (item2->is_exchanging() == true)
                    return false;

                if (item2->socket_count() == 0)
                    return false;

                switch (item2->type()) {
                case ITEM_WEAPON:
                    break;
                case ITEM_ARMOR:
                    switch (item2->sub_type()) {
                    case ARMOR_EAR:
                    case ARMOR_WRIST:
                    case ARMOR_NECK:
                        chat_packet(CHAT_TYPE_INFO, lc_text("빼낼 영석이 없습니다"));
                        return false;
                    }
                    break;

                default:
                    return false;
                }

                std::stack<int32_t> socket;

                for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
                    socket.push(item2->get_socket(i));

                int idx = ITEM_SOCKET_MAX_NUM - 1;

                while (socket.size() > 0) {
                    if (socket.top() > 2 && socket.top() != ITEM_BROKEN_METIN_VNUM)
                        break;

                    idx--;
                    socket.pop();
                }

                if (socket.size() == 0) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("빼낼 영석이 없습니다"));
                    return false;
                }

                game::items::item *item_reward = auto_give_item(socket.top());

                if (item_reward != nullptr) {
                    item2->set_socket(idx, 1);

                    const std::string buf = std::format("{}({}) {}({})",
                        item2->name(), item2->id(), item_reward->name(), item_reward->id());
                    log_manager::instance().item_log(this, item, "USE_DETACHMENT_ONE", buf.c_str());

                    item->set_count(item->count() - 1);
                }
            }
            break;

        case 70201:
        case 70202:
        case 70203:
        case 70204:
        case 70205:
        case 70206:
            {
                if (part(PART_HAIR) >= 1001) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("현재 헤어스타일에서는 염색과 탈색이 불가능합니다."));
                } else {
                    quest::quest_manager &q = quest::quest_manager::instance();
                    quest::pc *pc = q.get_pc(player_id());

                    if (pc) {
                        int last_dye_level = pc->flag("dyeing_hair.last_dye_level");

                        if (last_dye_level == 0 ||
                            last_dye_level + 3 <= level() ||
                            item->vnum() == 70201) {
                            set_part(PART_HAIR, item->vnum() - 70201);

                            if (item->vnum() == 70201)
                                pc->set_flag("dyeing_hair.last_dye_level", 0);
                            else
                                pc->set_flag("dyeing_hair.last_dye_level", level());

                            item->set_count(item->count() - 1);
                            update_packet();
                        } else {
                            chat_packet(CHAT_TYPE_INFO, lc_text("%d 레벨이 되어야 다시 염색하실 수 있습니다."), last_dye_level + 3);
                        }
                    }
                }
            }
            break;

        case ITEM_NEW_YEAR_GREETING_VNUM:
            {
                uint32_t box_vnum = ITEM_NEW_YEAR_GREETING_VNUM;
                std::vector<uint32_t> vnums;
                std::vector<uint32_t> counts;
                std::vector<game::items::item *> item_gets;
                int count = 0;

                if (give_item_from_special_item_group(box_vnum, vnums, counts, item_gets, count)) {
                    for (int i = 0; i < count; i++) {
                        if (vnums[i] == special_item_group::GOLD)
                            chat_packet(CHAT_TYPE_INFO, lc_text("돈 %d 냥을 획득했습니다."), counts[i]);
                    }

                    item->set_count(item->count() - 1);
                }
            }
            break;

        case ITEM_VALENTINE_ROSE:
        case ITEM_VALENTINE_CHOCOLATE:
            {
                uint32_t box_vnum = item->vnum();
                std::vector<uint32_t> vnums;
                std::vector<uint32_t> counts;
                std::vector<game::items::item *> item_gets;
                int count = 0;

                if ((item->vnum() == ITEM_VALENTINE_ROSE && SEX_MALE == get_sex(this)) ||
                    (item->vnum() == ITEM_VALENTINE_CHOCOLATE && SEX_FEMALE == get_sex(this))) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("성별이 맞지않아 이 아이템을 열 수 없습니다."));
                    return false;
                }

                if (give_item_from_special_item_group(box_vnum, vnums, counts, item_gets, count))
                    item->set_count(item->count() - 1);
            }
            break;

        case ITEM_WHITEDAY_CANDY:
        case ITEM_WHITEDAY_ROSE:
            {
                uint32_t box_vnum = item->vnum();
                std::vector<uint32_t> vnums;
                std::vector<uint32_t> counts;
                std::vector<game::items::item *> item_gets;
                int count = 0;

                if ((item->vnum() == ITEM_WHITEDAY_CANDY && SEX_MALE == get_sex(this)) ||
                    (item->vnum() == ITEM_WHITEDAY_ROSE && SEX_FEMALE == get_sex(this))) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("성별이 맞지않아 이 아이템을 열 수 없습니다."));
                    return false;
                }

                if (give_item_from_special_item_group(box_vnum, vnums, counts, item_gets, count))
                    item->set_count(item->count() - 1);
            }
            break;

        case 50011:
            {
                uint32_t box_vnum = 50011;
                std::vector<uint32_t> vnums;
                std::vector<uint32_t> counts;
                std::vector<game::items::item *> item_gets;
                int count = 0;

                if (give_item_from_special_item_group(box_vnum, vnums, counts, item_gets, count)) {
                    for (int i = 0; i < count; i++) {
                        const std::string buf = std::format("{} {}", vnums[i], counts[i]);
                        log_manager::instance().item_log(this, item, "MOONLIGHT_GET", buf.c_str());

                        item->set_count(item->count() - 1);

                        notify_box_reward(vnums[i], counts[i], item_gets[i]);
                    }
                } else {
                    chat_packet(CHAT_TYPE_TALKING, lc_text("아무것도 얻을 수 없었습니다."));
                    return false;
                }
            }
            break;

        case ITEM_GIVE_STAT_RESET_COUNT_VNUM:
            {
                point_change(POINT_STAT_RESET_COUNT, 1);
                item->set_count(item->count() - 1);
            }
            break;

        case 50107:
            {
                effect_packet(SE_CHINA_FIREWORK);
                add_affect(AFFECT_CHINA_FIREWORK, POINT_STUN_PCT, 30, AFF_CHINA_FIREWORK, 5 * 60, 0, true);
                item->set_count(item->count() - 1);
            }
            break;

        case 50108:
            {
                if (arena_manager::instance().is_arena_map(map_index()) == true) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("대련 중에는 이용할 수 없는 물품입니다."));
                    return false;
                }

                effect_packet(SE_SPIN_TOP);
                add_affect(AFFECT_CHINA_FIREWORK, POINT_STUN_PCT, 30, AFF_CHINA_FIREWORK, 5 * 60, 0, true);
                item->set_count(item->count() - 1);
            }
            break;

        case ITEM_WONSO_BEAN_VNUM:
            point_change(POINT_HP, max_hp() - hp());
            item->set_count(item->count() - 1);
            break;

        case ITEM_WONSO_SUGAR_VNUM:
            point_change(POINT_SP, max_sp() - sp());
            item->set_count(item->count() - 1);
            break;

        case ITEM_WONSO_FRUIT_VNUM:
            point_change(POINT_STAMINA, max_stamina() - stamina());
            item->set_count(item->count() - 1);
            break;

        case ITEM_ELK_VNUM:
            {
                int gold = item->get_socket(0);
                item_manager::instance().remove_item(item);
                chat_packet(CHAT_TYPE_INFO, lc_text("돈 %d 냥을 획득했습니다."), gold);
                point_change(POINT_GOLD, gold);
            }
            break;

        case 70021:
            {
                int heal_price = quest::quest_manager::instance().event_flag("MonarchHealGold");
                if (heal_price == 0)
                    heal_price = 2000000;

                if (monarch::instance().heal_my_empire(this, heal_price)) {
                    char notice[256];
                    snprintf(notice, sizeof(notice), lc_text("군주의 축복으로 이지역 %s 유저는 HP,SP가 모두 채워집니다."), empire_name(empire()));
                    send_notice_map(notice, map_index(), false);

                    chat_packet(CHAT_TYPE_INFO, lc_text("군주의 축복을 사용하였습니다."));
                }
            }
            break;

        case 27995:
            {
            }
            break;

        case 71092:
            {
                if (chr_target_ != nullptr) {
                    if (chr_target_->is_polymorphed()) {
                        chr_target_->set_polymorph(0);
                        chr_target_->remove_affect(AFFECT_POLYMORPH);
                    }
                } else {
                    if (is_polymorphed()) {
                        set_polymorph(0);
                        remove_affect(AFFECT_POLYMORPH);
                    }
                }
            }
            break;

        case 71051:
            {

                game::items::item *item2;

                if (!is_valid_item_position(dest_cell) || !(item2 = inventory_item(dest_cell.cell)))
                    return false;

                if (item2->is_exchanging() == true)
                    return false;

                if (item2->is_equipped() == true)
                    return false;

                if (item2->type() == ITEM_COSTUME)
                    return false;

                if (item2->attribute_set_index() == -1) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("속성을 변경할 수 없는 아이템입니다."));
                    return false;
                }

                if (item2->add_rare_attribute() == true) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("성공적으로 속성이 추가 되었습니다"));

                    int added_idx = item2->rare_attr_count() + 4;
                    const std::string buf = std::to_string(item2->id());

                    log_manager::instance().item_log(
                        player_id(),
                        item2->attribute_type(added_idx),
                        item2->attribute_value(added_idx),
                        item->id(),
                        "ADD_RARE_ATTR",
                        buf.c_str(),
                        get_desc()->host_name(),
                        item->original_vnum());

                    item->set_count(item->count() - 1);
                } else {
                    chat_packet(CHAT_TYPE_INFO, lc_text("더 이상 이 아이템으로 속성을 추가할 수 없습니다"));
                }
            }
            break;

        case 71052:
            {

                game::items::item *item2;

                if (!is_valid_item_position(dest_cell) || !(item2 = get_item(dest_cell)))
                    return false;

                if (item2->is_exchanging() == true)
                    return false;

                if (item2->is_equipped() == true)
                    return false;

                if (item2->type() == ITEM_COSTUME)
                    return false;

                if (item2->attribute_set_index() == -1) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("속성을 변경할 수 없는 아이템입니다."));
                    return false;
                }

                if (item2->change_rare_attribute() == true) {
                    const std::string buf = std::to_string(item2->id());
                    log_manager::instance().item_log(this, item, "CHANGE_RARE_ATTR", buf.c_str());

                    item->set_count(item->count() - 1);
                } else {
                    chat_packet(CHAT_TYPE_INFO, lc_text("변경 시킬 속성이 없습니다"));
                }
            }
            break;

        case ITEM_AUTO_HP_RECOVERY_S:
        case ITEM_AUTO_HP_RECOVERY_M:
        case ITEM_AUTO_HP_RECOVERY_L:
        case ITEM_AUTO_HP_RECOVERY_X:
        case ITEM_AUTO_SP_RECOVERY_S:
        case ITEM_AUTO_SP_RECOVERY_M:
        case ITEM_AUTO_SP_RECOVERY_L:
        case ITEM_AUTO_SP_RECOVERY_X:
        case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_XS:
        case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_S:
        case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_XS:
        case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_S:
        case FUCKING_BRAZIL_ITEM_AUTO_SP_RECOVERY_S:
        case FUCKING_BRAZIL_ITEM_AUTO_HP_RECOVERY_S:
            return use_item_auto_recovery(item);
        }

        return true;
    }

    auto character::use_item_lucky_bag(item *item) -> bool {
        const int max_bag_info = 26;
        static struct lucky_bag_info {
            uint32_t count;
            int prob;
            uint32_t vnum;
        } b1[max_bag_info] =
            {
                {1000, 302, 1    },
                {10,   150, 27002},
                {10,   75,  27003},
                {10,   100, 27005},
                {10,   50,  27006},
                {10,   80,  27001},
                {10,   50,  27002},
                {10,   80,  27004},
                {10,   50,  27005},
                {1,    10,  50300},
                {1,    6,   92   },
                {1,    2,   132  },
                {1,    6,   1052 },
                {1,    2,   1092 },
                {1,    6,   2082 },
                {1,    2,   2122 },
                {1,    6,   3082 },
                {1,    2,   3122 },
                {1,    6,   5052 },
                {1,    2,   5082 },
                {1,    6,   7082 },
                {1,    2,   7122 },
                {1,    1,   11282},
                {1,    1,   11482},
                {1,    1,   11682},
                {1,    1,   11882},
        };

        lucky_bag_info *bi = nullptr;
        bi = b1;

        int pct = number(1, 1000);

        int i;
        for (i = 0; i < max_bag_info; i++) {
            if (pct <= bi[i].prob)
                break;
            pct -= bi[i].prob;
        }
        if (i >= max_bag_info)
            return false;

        if (bi[i].vnum == 50300) {
            give_random_skill_book();
        } else if (bi[i].vnum == 1) {
            point_change(POINT_GOLD, 1000, true);
        } else {
            auto_give_item(bi[i].vnum, bi[i].count);
        }
        item_manager::instance().remove_item(item);

        return true;
    }

    auto character::use_item_event_stone_detector(item *item) -> bool {
        if (item->get_socket(0)) {
            item->set_socket(0, item->get_socket(0) + 1);
        } else {
            int map_index_value = map_index();

            pixel_position pos;

            if (sectree_manager::instance().random_location(map_index_value, pos, 700)) {
                item->set_socket(0, 1);
                item->set_socket(1, pos.x);
                item->set_socket(2, pos.y);
            } else {
                chat_packet(CHAT_TYPE_INFO, lc_text("이 곳에선 이벤트용 감지기가 동작하지 않는것 같습니다."));
                return false;
            }
        }

        int dist = 0;
        float distance = (distance_sqrt(x() - item->get_socket(1), y() - item->get_socket(2)));

        if (distance < 1000.0f) {
            chat_packet(CHAT_TYPE_INFO, lc_text("이벤트용 감지기가 신비로운 빛을 내며 사라집니다."));

            struct event_stone_info {
                uint32_t vnum;
                int count;
                int prob;
            };
            const int event_stone_max_info = 15;
            event_stone_info info_10[event_stone_max_info] =
                {
                    {27001, 10, 8 },
                    {27004, 10, 6 },
                    {27002, 10, 12},
                    {27005, 10, 12},
                    {27100, 1,  9 },
                    {27103, 1,  9 },
                    {27101, 1,  10},
                    {27104, 1,  10},
                    {27999, 1,  12},

                    {25040, 1,  4 },

                    {27410, 1,  0 },
                    {27600, 1,  0 },
                    {25100, 1,  0 },

                    {50001, 1,  0 },
                    {50003, 1,  1 },
            };
            event_stone_info info_7[event_stone_max_info] =
                {
                    {27001, 10, 1 },
                    {27004, 10, 1 },
                    {27004, 10, 9 },
                    {27005, 10, 9 },
                    {27100, 1,  5 },
                    {27103, 1,  5 },
                    {27101, 1,  10},
                    {27104, 1,  10},
                    {27999, 1,  14},

                    {25040, 1,  5 },

                    {27410, 1,  5 },
                    {27600, 1,  5 },
                    {25100, 1,  5 },

                    {50001, 1,  0 },
                    {50003, 1,  5 },
            };
            event_stone_info info_4[event_stone_max_info] =
                {
                    {27001, 10, 0 },
                    {27004, 10, 0 },
                    {27002, 10, 0 },
                    {27005, 10, 0 },
                    {27100, 1,  0 },
                    {27103, 1,  0 },
                    {27101, 1,  0 },
                    {27104, 1,  0 },
                    {27999, 1,  25},

                    {25040, 1,  0 },

                    {27410, 1,  0 },
                    {27600, 1,  0 },
                    {25100, 1,  15},

                    {50001, 1,  10},
                    {50003, 1,  50},
            };

            {
                event_stone_info *info;
                if (item->get_socket(0) <= 4)
                    info = info_4;
                else if (item->get_socket(0) <= 7)
                    info = info_7;
                else
                    info = info_10;

                int prob = number(1, 100);

                for (int i = 0; i < event_stone_max_info; ++i) {
                    if (!info[i].prob)
                        continue;

                    if (prob <= info[i].prob) {
                        if (info[i].vnum == 50001) {
                            db_manager::instance().func_query([pid = player_id(), vnum = info[i].vnum, count = info[i].count](sql_msg &msg) { give_lotto_ticket_result(msg, pid, vnum, count, 0); },
                                sql::format("INSERT INTO lotto_list VALUES(0, 'server', {}, NOW())", player_id()));
                        } else
                            auto_give_item(info[i].vnum, info[i].count);

                        break;
                    }
                    prob -= info[i].prob;
                }
            }

            const std::string chatbuf = std::format("StoneDetect {} 0 0", (uint32_t)get_vid());
            int len = static_cast<int>(chatbuf.size()) + 1;

            packet_gc_chat pack_chat;
            pack_chat.header = HEADER_GC_CHAT;
            pack_chat.size = sizeof(packet_gc_chat) + len;
            pack_chat.type = CHAT_TYPE_COMMAND;
            pack_chat.id = 0;
            pack_chat.empire = get_desc()->empire();

            temp_buffer buf;
            buf.write(&pack_chat, sizeof(packet_gc_chat));
            buf.write(chatbuf.c_str(), len);

            packet_around(buf.read_peek(), buf.size());

            item_manager::instance().remove_item(item, "REMOVE (DETECT_EVENT_STONE) 1");
            return true;
        } else if (distance < 20000)
            dist = 1;
        else if (distance < 70000)
            dist = 2;
        else
            dist = 3;

        const int stone_detect_max_try = 10;
        if (item->get_socket(0) >= stone_detect_max_try) {
            chat_packet(CHAT_TYPE_INFO, lc_text("이벤트용 감지기가 흔적도 없이 사라집니다."));
            item_manager::instance().remove_item(item, "REMOVE (DETECT_EVENT_STONE) 0");
            auto_give_item(27002);
            return true;
        }

        if (dist) {
            const std::string chatbuf = std::format("StoneDetect {} {} {}",
                (uint32_t)get_vid(), dist, (int)get_degree_from_position_xy(x(), item->get_socket(2), item->get_socket(1), y()));
            int len = static_cast<int>(chatbuf.size()) + 1;

            packet_gc_chat pack_chat;
            pack_chat.header = HEADER_GC_CHAT;
            pack_chat.size = sizeof(packet_gc_chat) + len;
            pack_chat.type = CHAT_TYPE_COMMAND;
            pack_chat.id = 0;
            pack_chat.empire = get_desc()->empire();

            temp_buffer buf;
            buf.write(&pack_chat, sizeof(packet_gc_chat));
            buf.write(chatbuf.c_str(), len);

            packet_around(buf.read_peek(), buf.size());
        }

        return true;
    }

    auto character::use_item_stone_detector(item *item) -> bool {
        sectree_map *map = sectree_manager::instance().map(map_index());

        if (map != nullptr) {
            item->set_socket(0, item->get_socket(0) + 1);

            std::vector<character *> stones;

            map->for_each([&](entity *ent) {
                if (ent->is_type(ENTITY_CHARACTER) == true) {
                    auto *ch = (character *)ent;

                    if (ch->is_stone() == true)
                        stones.push_back(ch);
                }
            });

            if (!stones.empty()) {
                auto stone_key = [&](character *stone) {
                    auto dist = (uint32_t)distance_sqrt(x() - stone->x(), y() - stone->y());
                    return std::pair(dist != 0 ? dist : UINT_MAX, (uint32_t)stone->get_vid());
                };

                character *target = *std::ranges::min_element(stones, {}, stone_key);
                uint32_t max = stone_key(target).first;
                int val = 3;

                if (max < 10000)
                    val = 2;
                else if (max < 70000)
                    val = 1;

                chat_packet(CHAT_TYPE_COMMAND, "StoneDetect %u %d %d", (uint32_t)get_vid(), val,
                    (int)get_degree_from_position_xy(x(), target->y(), target->x(), y()));
            } else {
                chat_packet(CHAT_TYPE_INFO, lc_text("감지기를 작용하였으나 감지되는 영석이 없습니다."));
            }

            if (item->get_socket(0) >= 6) {
                chat_packet(CHAT_TYPE_COMMAND, "StoneDetect %u 0 0", (uint32_t)get_vid());
                item_manager::instance().remove_item(item);
            }
        }

        return true;
    }

    auto character::use_item_auto_recovery(item *item) -> bool {
        if (arena_manager::instance().is_arena_map(map_index()) == true) {
            chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
            return false;
        }

        affect_types type = AFFECT_NONE;
        bool is_special_potion = false;

        switch (item->vnum()) {
        case ITEM_AUTO_HP_RECOVERY_X:
            is_special_potion = true;
            [[fallthrough]];

        case ITEM_AUTO_HP_RECOVERY_S:
        case ITEM_AUTO_HP_RECOVERY_M:
        case ITEM_AUTO_HP_RECOVERY_L:
        case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_XS:
        case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_S:
        case FUCKING_BRAZIL_ITEM_AUTO_HP_RECOVERY_S:
            type = AFFECT_AUTO_HP_RECOVERY;
            break;

        case ITEM_AUTO_SP_RECOVERY_X:
            is_special_potion = true;
            [[fallthrough]];

        case ITEM_AUTO_SP_RECOVERY_S:
        case ITEM_AUTO_SP_RECOVERY_M:
        case ITEM_AUTO_SP_RECOVERY_L:
        case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_XS:
        case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_S:
        case FUCKING_BRAZIL_ITEM_AUTO_SP_RECOVERY_S:
            type = AFFECT_AUTO_SP_RECOVERY;
            break;
        }

        if (AFFECT_NONE == type)
            return true;

        if (item->count() > 1) {
            int pos = empty_inventory(item->size());

            if (-1 == pos) {
                chat_packet(CHAT_TYPE_INFO, lc_text("소지품에 빈 공간이 없습니다."));
                return true;
            }

            item->set_count(item->count() - 1);

            game::items::item *item2 = item_manager::instance().create_item(item->vnum(), 1);
            item2->add_to_character(this, item_pos(INVENTORY, pos));

            if (item->get_socket(1) != 0) {
                item2->set_socket(1, item->get_socket(1));
            }

            item = item2;
        }

        affect *affect = find_affect(type);

        if (nullptr == affect) {
            point_types bonus = POINT_NONE;

            if (true == is_special_potion) {
                if (type == AFFECT_AUTO_HP_RECOVERY) {
                    bonus = POINT_MAX_HP_PCT;
                } else if (type == AFFECT_AUTO_SP_RECOVERY) {
                    bonus = POINT_MAX_SP_PCT;
                }
            }

            add_affect(type, bonus, 4, item->id(), INFINITE_AFFECT_DURATION, 0, true, false);

            item->lock(true);
            item->set_socket(0, true);

            auto_recovery_item_process(type);
        } else {
            if (item->id() == affect->flag) {
                remove_affect(affect);

                item->lock(false);
                item->set_socket(0, false);
            } else {
                game::items::item *old = find_item_by_id(affect->flag);

                if (nullptr != old) {
                    old->lock(false);
                    old->set_socket(0, false);
                }

                remove_affect(affect);

                point_types bonus = POINT_NONE;

                if (true == is_special_potion) {
                    if (type == AFFECT_AUTO_HP_RECOVERY) {
                        bonus = POINT_MAX_HP_PCT;
                    } else if (type == AFFECT_AUTO_SP_RECOVERY) {
                        bonus = POINT_MAX_SP_PCT;
                    }
                }

                add_affect(type, bonus, 4, item->id(), INFINITE_AFFECT_DURATION, 0, true, false);

                item->lock(true);
                item->set_socket(0, true);

                auto_recovery_item_process(type);
            }
        }

        return true;
    }

    auto character::use_item_use_invisibility(item *item) -> bool {
        if (item->vnum() == 70026) {
            quest::quest_manager &q = quest::quest_manager::instance();
            quest::pc *pc = q.get_pc(player_id());

            if (pc != nullptr) {
                int last_use_time = pc->flag("mirror_of_disapper.last_use_time");

                if (get_global_time() - last_use_time < 10 * 60) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("아직 사용할 수 없습니다."));
                    return false;
                }

                pc->set_flag("mirror_of_disapper.last_use_time", get_global_time());
            }
        }

        add_affect(AFFECT_INVISIBILITY, POINT_NONE, 0, AFF_INVISIBILITY, 300, 0, true);
        item->set_count(item->count() - 1);

        return true;
    }

    auto character::use_item_use_potion_nodelay(item *item) -> bool {
        if (arena_manager::instance().is_arena_map(map_index()) == true) {
            if (quest::quest_manager::instance().event_flag("arena_potion_limit") > 0) {
                chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
                return false;
            }

            switch (item->vnum()) {
            case 70020:
            case 71018:
            case 71019:
            case 71020:
                if (quest::quest_manager::instance().event_flag("arena_potion_limit_count") < 10000) {
                    if (potion_limit_ <= 0) {
                        chat_packet(CHAT_TYPE_INFO, lc_text("사용 제한량을 초과하였습니다."));
                        return false;
                    }
                }
                break;

            default:
                chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
                return false;
            }
        }

        bool used = false;

        if (item->value(0) != 0) {
            if (hp() < max_hp()) {
                point_change(POINT_HP, item->value(0) * (100 + point(POINT_POTION_BONUS)) / 100);
                effect_packet(SE_HPUP_RED);
                used = true;
            }
        }

        if (item->value(1) != 0) {
            if (sp() < max_sp()) {
                point_change(POINT_SP, item->value(1) * (100 + point(POINT_POTION_BONUS)) / 100);
                effect_packet(SE_SPUP_BLUE);
                used = true;
            }
        }

        if (item->value(3) != 0) {
            if (hp() < max_hp()) {
                point_change(POINT_HP, item->value(3) * max_hp() / 100);
                effect_packet(SE_HPUP_RED);
                used = true;
            }
        }

        if (item->value(4) != 0) {
            if (sp() < max_sp()) {
                point_change(POINT_SP, item->value(4) * max_sp() / 100);
                effect_packet(SE_SPUP_BLUE);
                used = true;
            }
        }

        if (used) {
            if (item->vnum() == 50085 || item->vnum() == 50086) {
                if (metin2::config::test_server())
                    chat_packet(CHAT_TYPE_INFO, lc_text("월병 또는 종자 를 사용하였습니다"));
            }
            consume_potion(item);

            potion_limit_--;
        }

        return true;
    }

    auto character::use_item_use_potion(item *item) -> bool {
        if (arena_manager::instance().is_arena_map(map_index()) == true) {
            if (quest::quest_manager::instance().event_flag("arena_potion_limit") > 0) {
                chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
                return false;
            }

            switch (item->vnum()) {
            case 27001:
            case 27002:
            case 27003:
            case 27004:
            case 27005:
            case 27006:
                if (quest::quest_manager::instance().event_flag("arena_potion_limit_count") < 10000) {
                    if (potion_limit_ <= 0) {
                        chat_packet(CHAT_TYPE_INFO, lc_text("사용 제한량을 초과하였습니다."));
                        return false;
                    }
                }
                break;

            default:
                chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
                return false;
            }
        }

        if (item->value(1) != 0) {
            if (point(POINT_SP_RECOVERY) + sp() >= max_sp()) {
                return false;
            }

            point_change(POINT_SP_RECOVERY, item->value(1) * std::min(200, (100 + point(POINT_POTION_BONUS))) / 100);
            start_affect_event();
            effect_packet(SE_SPUP_BLUE);
        }

        if (item->value(0) != 0) {
            if (point(POINT_HP_RECOVERY) + hp() >= max_hp()) {
                return false;
            }

            point_change(POINT_HP_RECOVERY, item->value(0) * std::min(200, (100 + point(POINT_POTION_BONUS))) / 100);
            start_affect_event();
            effect_packet(SE_HPUP_RED);
        }

        consume_potion(item);
        potion_limit_--;

        return true;
    }

    auto character::use_item_use_potion_continue(item *item) -> bool {
        if (item->value(0) != 0) {
            add_affect(AFFECT_HP_RECOVER_CONTINUE, POINT_HP_RECOVER_CONTINUE, item->value(0), 0, item->value(2), 0, true);
        } else if (item->value(1) != 0) {
            add_affect(AFFECT_SP_RECOVER_CONTINUE, POINT_SP_RECOVER_CONTINUE, item->value(1), 0, item->value(2), 0, true);
        } else
            return false;

        consume_potion(item);

        return true;
    }

    auto character::use_item_use_talisman(item *item) -> bool {
        const int town_portal = 1;
        const int memory_portal = 2;

        if (map_index() == 200 || map_index() == 113) {
            chat_packet(CHAT_TYPE_INFO, lc_text("현재 위치에서 사용할 수 없습니다."));
            return false;
        }

        if (arena_manager::instance().is_arena_map(map_index()) == true) {
            chat_packet(CHAT_TYPE_INFO, lc_text("대련 중에는 이용할 수 없는 물품입니다."));
            return false;
        }

        if (warp_event) {
            chat_packet(CHAT_TYPE_INFO, lc_text("이동할 준비가 되어있음으로 귀환부를 사용할수 없습니다"));
            return false;
        }

        int consume_life = calculate_consume(this);

        if (consume_life < 0)
            return false;

        if (item->value(0) == town_portal) {
            if (item->get_socket(0) == 0) {
                if (!get_dungeon())
                    if (!give_recall_item(item))
                        return false;

                pixel_position pos_warp;

                if (sectree_manager::instance().recall_position_by_empire(map_index(), empire(), pos_warp)) {
                    point_change(POINT_HP, -consume_life, false);

                    warp_set(pos_warp.x, pos_warp.y);
                } else {
                    metin2::log::error("Cannot find spawn position for {} near {:d} x {:d}", name(), x(), y());
                }
            } else {
                if (metin2::config::test_server())
                    chat_packet(CHAT_TYPE_INFO, lc_text("원래 위치로 복귀"));

                process_recall_item(item);
            }
        } else if (item->value(0) == memory_portal) {
            if (item->get_socket(0) == 0) {
                if (get_dungeon()) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("던전 안에서는 %s%s 사용할 수 없습니다."),
                        item->name(),
                        "");
                    return false;
                }

                if (!give_recall_item(item))
                    return false;
            } else {
                point_change(POINT_HP, -consume_life, false);

                process_recall_item(item);
            }
        }

        return true;
    }

    auto character::use_item_use_tuning(item *item, item_pos dest_cell) -> bool {
        game::items::item *item2;

        if (!is_valid_item_position(dest_cell) || !(item2 = get_item(dest_cell)))
            return false;

        if (item2->is_exchanging())
            return false;

        if (item2->is_equipped())
            return false;

        if (item2->vnum() >= 28330 && item2->vnum() <= 28343) {
            chat_packet(CHAT_TYPE_INFO, lc_text("+3 영석은 이 아이템으로 개량할 수 없습니다"));
            return false;
        }

        if (item2->vnum() >= 28430 && item2->vnum() <= 28443) {
            if (item->vnum() == 71056) {
                refine_item(item, item2);
            } else {
                chat_packet(CHAT_TYPE_INFO, lc_text("영석은 이 아이템으로 개량할 수 없습니다"));
            }
        } else {
            refine_item(item, item2);
        }

        return true;
    }

    auto character::use_item_use_socket_or_attribute(item *item, item_pos dest_cell) -> bool {
        game::items::item *item2;
        if (!is_valid_item_position(dest_cell) || !(item2 = get_item(dest_cell)))
            return false;

        if (item2->is_equipped()) {
            buff_on_attr_remove_buffs_from_item(item2);
        }

        if (ITEM_COSTUME == item2->type()) {
            chat_packet(CHAT_TYPE_INFO, lc_text("속성을 변경할 수 없는 아이템입니다."));
            return false;
        }

        if (item2->is_exchanging())
            return false;

        if (item2->is_equipped())
            return false;

        switch (item->sub_type()) {
        case USE_CLEAN_SOCKET:
            {
                int i;
                for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i) {
                    if (item2->get_socket(i) == ITEM_BROKEN_METIN_VNUM)
                        break;
                }

                if (i == ITEM_SOCKET_MAX_NUM) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("청소할 석이 박혀있지 않습니다."));
                    return false;
                }

                int j = 0;

                for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i) {
                    if (item2->get_socket(i) != ITEM_BROKEN_METIN_VNUM && item2->get_socket(i) != 0)
                        item2->set_socket(j++, item2->get_socket(i));
                }

                for (; j < ITEM_SOCKET_MAX_NUM; ++j) {
                    if (item2->get_socket(j) > 0)
                        item2->set_socket(j, 1);
                }

                {
                    const std::string buf = std::to_string(item2->id());
                    log_manager::instance().item_log(this, item, "CLEAN_SOCKET", buf.c_str());
                }

                item->set_count(item->count() - 1);
            }
            break;

        case USE_CHANGE_ATTRIBUTE:
            if (item2->attribute_set_index() == -1) {
                chat_packet(CHAT_TYPE_INFO, lc_text("속성을 변경할 수 없는 아이템입니다."));
                return false;
            }

            if (item2->attribute_count() == 0) {
                chat_packet(CHAT_TYPE_INFO, lc_text("변경할 속성이 없습니다."));
                return false;
            }

            if (GM_PLAYER == gm_level() && false == metin2::config::test_server()) {

                uint32_t change_item_attr_cycle = quest::quest_manager::instance().event_flag(change_item_attr_cycle_flag_);
                if (change_item_attr_cycle < default_change_item_attr_cycle_)
                    change_item_attr_cycle = default_change_item_attr_cycle_;

                quest::pc *pc = quest::quest_manager::instance().get_pc(player_id());

                if (pc) {
                    uint32_t now_min = get_global_time() / 60;

                    pc->set_flag(last_change_item_attr_flag_, now_min);
                }
            }

            if (item->sub_type() == USE_CHANGE_ATTRIBUTE2) {
                int ai_change_prob[ITEM_ATTRIBUTE_MAX_LEVEL] =
                    {
                        0, 0, 30, 40, 3};

                item2->change_attribute(ai_change_prob);
            } else if (item->vnum() == 76014) {
                int ai_change_prob[ITEM_ATTRIBUTE_MAX_LEVEL] =
                    {
                        0, 10, 50, 39, 1};

                item2->change_attribute(ai_change_prob);
            }

            else {
                if (item->vnum() == 71151 || item->vnum() == 76023) {
                    if ((item2->type() == ITEM_WEAPON) || (item2->type() == ITEM_ARMOR && item2->sub_type() == ARMOR_BODY)) {
                        bool can_use = true;
                        for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i) {
                            if (item2->limit_type(i) == LIMIT_LEVEL && item2->limit_value(i) > 40) {
                                can_use = false;
                                break;
                            }
                        }
                        if (false == can_use) {
                            chat_packet(CHAT_TYPE_INFO, lc_text("적용 레벨보다 높아 사용이 불가능합니다."));
                            break;
                        }
                    } else {
                        chat_packet(CHAT_TYPE_INFO, lc_text("무기와 갑옷에만 사용 가능합니다."));
                        break;
                    }
                }
                item2->change_attribute();
            }

            chat_packet(CHAT_TYPE_INFO, lc_text("속성을 변경하였습니다."));
            {
                const std::string buf = std::to_string(item2->id());
                log_manager::instance().item_log(this, item, "CHANGE_ATTRIBUTE", buf.c_str());
            }

            item->set_count(item->count() - 1);
            break;

        case USE_ADD_ATTRIBUTE:
            if (item2->attribute_set_index() == -1) {
                chat_packet(CHAT_TYPE_INFO, lc_text("속성을 변경할 수 없는 아이템입니다."));
                return false;
            }

            if (item2->attribute_count() < 4) {
                if (item->vnum() == 71152 || item->vnum() == 76024) {
                    if ((item2->type() == ITEM_WEAPON) || (item2->type() == ITEM_ARMOR && item2->sub_type() == ARMOR_BODY)) {
                        bool can_use = true;
                        for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i) {
                            if (item2->limit_type(i) == LIMIT_LEVEL && item2->limit_value(i) > 40) {
                                can_use = false;
                                break;
                            }
                        }
                        if (false == can_use) {
                            chat_packet(CHAT_TYPE_INFO, lc_text("적용 레벨보다 높아 사용이 불가능합니다."));
                            break;
                        }
                    } else {
                        chat_packet(CHAT_TYPE_INFO, lc_text("무기와 갑옷에만 사용 가능합니다."));
                        break;
                    }
                }
                const std::string buf = std::to_string(item2->id());

                if (prob(item_attribute_add_percent[item2->attribute_count()])) {
                    item2->add_attribute();
                    chat_packet(CHAT_TYPE_INFO, lc_text("속성 추가에 성공하였습니다."));

                    int added_idx = item2->attribute_count() - 1;
                    log_manager::instance().item_log(
                        player_id(),
                        item2->attribute_type(added_idx),
                        item2->attribute_value(added_idx),
                        item->id(),
                        "ADD_ATTRIBUTE_SUCCESS",
                        buf.c_str(),
                        get_desc()->host_name(),
                        item->original_vnum());
                } else {
                    chat_packet(CHAT_TYPE_INFO, lc_text("속성 추가에 실패하였습니다."));
                    log_manager::instance().item_log(this, item, "ADD_ATTRIBUTE_FAIL", buf.c_str());
                }

                item->set_count(item->count() - 1);
            } else {
                chat_packet(CHAT_TYPE_INFO, lc_text("더이상 이 아이템을 이용하여 속성을 추가할 수 없습니다."));
            }
            break;

        case USE_ADD_ATTRIBUTE2:
            if (item2->attribute_set_index() == -1) {
                chat_packet(CHAT_TYPE_INFO, lc_text("속성을 변경할 수 없는 아이템입니다."));
                return false;
            }

            if (item2->attribute_count() == 4) {
                const std::string buf = std::to_string(item2->id());

                if (prob(item_attribute_add_percent[item2->attribute_count()])) {
                    item2->add_attribute();
                    chat_packet(CHAT_TYPE_INFO, lc_text("속성 추가에 성공하였습니다."));

                    int added_idx = item2->attribute_count() - 1;
                    log_manager::instance().item_log(
                        player_id(),
                        item2->attribute_type(added_idx),
                        item2->attribute_value(added_idx),
                        item->id(),
                        "ADD_ATTRIBUTE2_SUCCESS",
                        buf.c_str(),
                        get_desc()->host_name(),
                        item->original_vnum());
                } else {
                    chat_packet(CHAT_TYPE_INFO, lc_text("속성 추가에 실패하였습니다."));
                    log_manager::instance().item_log(this, item, "ADD_ATTRIBUTE2_FAIL", buf.c_str());
                }

                item->set_count(item->count() - 1);
            } else if (item2->attribute_count() == 5) {
                chat_packet(CHAT_TYPE_INFO, lc_text("더 이상 이 아이템을 이용하여 속성을 추가할 수 없습니다."));
            } else if (item2->attribute_count() < 4) {
                chat_packet(CHAT_TYPE_INFO, lc_text("먼저 재가비서를 이용하여 속성을 추가시켜 주세요."));
            } else {
                metin2::log::error("Item has invalid attribute count {:d}", item2->attribute_count());
            }
            break;

        case USE_ADD_ACCESSORY_SOCKET:
            {
                const std::string buf = std::to_string(item2->id());

                if (item2->is_accessory_for_socket()) {
                    if (item2->accessory_socket_max_grade() < ITEM_ACCESSORY_SOCKET_MAX_NUM) {
                        if (prob(50)) {
                            item2->set_accessory_socket_max_grade(item2->accessory_socket_max_grade() + 1);
                            chat_packet(CHAT_TYPE_INFO, lc_text("소켓이 성공적으로 추가되었습니다."));
                            log_manager::instance().item_log(this, item, "ADD_SOCKET_SUCCESS", buf.c_str());
                        } else {
                            chat_packet(CHAT_TYPE_INFO, lc_text("소켓 추가에 실패하였습니다."));
                            log_manager::instance().item_log(this, item, "ADD_SOCKET_FAIL", buf.c_str());
                        }

                        item->set_count(item->count() - 1);
                    } else {
                        chat_packet(CHAT_TYPE_INFO, lc_text("이 액세서리에는 더이상 소켓을 추가할 공간이 없습니다."));
                    }
                } else {
                    chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템으로 소켓을 추가할 수 없는 아이템입니다."));
                }
            }
            break;

        case USE_PUT_INTO_BELT_SOCKET:
        case USE_PUT_INTO_ACCESSORY_SOCKET:
            if (item2->is_accessory_for_socket() && item->can_put_into(item2)) {
                const std::string buf = std::to_string(item2->id());

                if (item2->accessory_socket_grade() < item2->accessory_socket_max_grade()) {
                    if (prob(accessory_socket_put_pct[item2->accessory_socket_grade()])) {
                        item2->set_accessory_socket_grade(item2->accessory_socket_grade() + 1);
                        chat_packet(CHAT_TYPE_INFO, lc_text("장착에 성공하였습니다."));
                        log_manager::instance().item_log(this, item, "PUT_SOCKET_SUCCESS", buf.c_str());
                    } else {
                        chat_packet(CHAT_TYPE_INFO, lc_text("장착에 실패하였습니다."));
                        log_manager::instance().item_log(this, item, "PUT_SOCKET_FAIL", buf.c_str());
                    }

                    item->set_count(item->count() - 1);
                } else {
                    if (item2->accessory_socket_max_grade() == 0)
                        chat_packet(CHAT_TYPE_INFO, lc_text("먼저 다이아몬드로 악세서리에 소켓을 추가해야합니다."));
                    else if (item2->accessory_socket_max_grade() < ITEM_ACCESSORY_SOCKET_MAX_NUM) {
                        chat_packet(CHAT_TYPE_INFO, lc_text("이 액세서리에는 더이상 장착할 소켓이 없습니다."));
                        chat_packet(CHAT_TYPE_INFO, lc_text("다이아몬드로 소켓을 추가해야합니다."));
                    } else
                        chat_packet(CHAT_TYPE_INFO, lc_text("이 액세서리에는 더이상 보석을 장착할 수 없습니다."));
                }
            } else {
                chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템을 장착할 수 없습니다."));
            }
            break;
        }
        if (item2->is_equipped()) {
            buff_on_attr_add_buffs_from_item(item2);
        }

        return true;
    }

    auto character::use_item_use_bait(item *item) -> bool {
        if (fishing_event) {
            chat_packet(CHAT_TYPE_INFO, lc_text("낚시 중에 미끼를 갈아끼울 수 없습니다."));
            return false;
        }

        game::items::item *weapon = wear(WEAR_WEAPON);

        if (!weapon || weapon->type() != ITEM_ROD)
            return false;

        if (weapon->get_socket(2)) {
            chat_packet(CHAT_TYPE_INFO, lc_text("이미 꽂혀있던 미끼를 빼고 %s를 끼웁니다."), item->name());
        } else {
            chat_packet(CHAT_TYPE_INFO, lc_text("낚시대에 %s를 미끼로 끼웁니다."), item->name());
        }

        weapon->set_socket(2, item->value(0));
        item->set_count(item->count() - 1);

        return true;
    }

    auto character::use_item_use_recipe(item *item) -> bool {
        game::items::item *source1 = find_specify_item(item->value(1));
        uint32_t source_count1 = item->value(2);

        game::items::item *source2 = find_specify_item(item->value(3));
        uint32_t source_count2 = item->value(4);

        if (source_count1 != 0) {
            if (source1 == nullptr) {
                chat_packet(CHAT_TYPE_INFO, lc_text("물약 조합을 위한 재료가 부족합니다."));
                return false;
            }
        }

        if (source_count2 != 0) {
            if (source2 == nullptr) {
                chat_packet(CHAT_TYPE_INFO, lc_text("물약 조합을 위한 재료가 부족합니다."));
                return false;
            }
        }

        if (source1 != nullptr) {
            if (source1->count() < source_count1) {
                chat_packet(CHAT_TYPE_INFO, lc_text("재료(%s)가 부족합니다."), source1->name());
                return false;
            }

            source1->set_count(source1->count() - source_count1);
        }

        if (source2 != nullptr) {
            if (source2->count() < source_count2) {
                chat_packet(CHAT_TYPE_INFO, lc_text("재료(%s)가 부족합니다."), source2->name());
                return false;
            }

            source2->set_count(source2->count() - source_count2);
        }

        game::items::item *bottle = find_specify_item(50901);

        if (!bottle || bottle->count() < 1) {
            chat_packet(CHAT_TYPE_INFO, lc_text("빈 병이 모자릅니다."));
            return false;
        }

        bottle->set_count(bottle->count() - 1);

        if (number(1, 100) > item->value(5)) {
            chat_packet(CHAT_TYPE_INFO, lc_text("물약 제조에 실패했습니다."));
            return false;
        }

        auto_give_item(item->value(0));

        return true;
    }

    auto character::use_item_metin(item *item, item_pos dest_cell) -> bool {
        game::items::item *item2;

        if (!is_valid_item_position(dest_cell) || !(item2 = get_item(dest_cell)))
            return false;

        if (item2->is_exchanging())
            return false;

        if (item2->is_equipped())
            return false;

        if (item2->type() == ITEM_PICK) return false;
        if (item2->type() == ITEM_ROD) return false;

        int i;

        for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i) {
            uint32_t vnum;

            if ((vnum = item2->get_socket(i)) <= 2)
                continue;

            item_table *p = item_manager::instance().table(vnum);

            if (!p)
                continue;

            if (item->value(5) == p->values[5]) {
                chat_packet(CHAT_TYPE_INFO, lc_text("같은 종류의 메틴석은 여러개 부착할 수 없습니다."));
                return false;
            }
        }

        if (item2->type() == ITEM_ARMOR) {
            if (!is_set(item->wear_flag(), WEARABLE_BODY) || !is_set(item2->wear_flag(), WEARABLE_BODY)) {
                chat_packet(CHAT_TYPE_INFO, lc_text("이 메틴석은 장비에 부착할 수 없습니다."));
                return false;
            }
        } else if (item2->type() == ITEM_WEAPON) {
            if (!is_set(item->wear_flag(), WEARABLE_WEAPON)) {
                chat_packet(CHAT_TYPE_INFO, lc_text("이 메틴석은 무기에 부착할 수 없습니다."));
                return false;
            }
        } else {
            chat_packet(CHAT_TYPE_INFO, lc_text("부착할 수 있는 슬롯이 없습니다."));
            return false;
        }

        for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
            if (item2->get_socket(i) >= 1 && item2->get_socket(i) <= 2 && item2->get_socket(i) >= item->value(2)) {
                if (prob(30)) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("메틴석 부착에 성공하였습니다."));
                    item2->set_socket(i, item->vnum());
                } else {
                    chat_packet(CHAT_TYPE_INFO, lc_text("메틴석 부착에 실패하였습니다."));
                    item2->set_socket(i, ITEM_BROKEN_METIN_VNUM);
                }

                log_manager::instance().item_log(this, item2, "SOCKET", item->name());
                item_manager::instance().remove_item(item, "REMOVE (METIN)");
                break;
            }

        if (i == ITEM_SOCKET_MAX_NUM)
            chat_packet(CHAT_TYPE_INFO, lc_text("부착할 수 있는 슬롯이 없습니다."));

        return true;
    }

    auto character::use_item_blend(item *item) -> bool {
        if (blend_item_find(item->vnum())) {
            int affect_type = AFFECT_BLEND;
            if (item->get_socket(0) >= static_cast<int32_t>(std::size(a_apply_info))) {
                metin2::log::error("Invalid blend item (id {:d}, vnum {:d}): apply type {:d}", item->id(), item->vnum(), item->get_socket(0));
                return false;
            }
            int apply_type = a_apply_info[item->get_socket(0)].point_type;
            int apply_value = item->get_socket(1);
            int apply_duration = item->get_socket(2);

            if (find_affect(affect_type, apply_type)) {
                notify_already_affected();
            } else {
                if (find_affect(AFFECT_EXP_BONUS_EURO_FREE, POINT_RESIST_MAGIC)) {
                    notify_already_affected();
                } else {
                    add_affect(affect_type, apply_type, apply_value, 0, apply_duration, 0, false);
                    item->set_count(item->count() - 1);
                }
            }
        }

        return true;
    }

    auto character::use_item_extract(item *item, item_pos dest_cell) -> bool {
        game::items::item *dest_item = get_item(dest_cell);
        if (nullptr == dest_item) {
            return false;
        }
        switch (item->sub_type()) {
        case EXTRACT_DRAGON_SOUL:
            if (dest_item->is_dragon_soul()) {
                return ds_manager::instance().pull_out(this, NPOS, dest_item, item);
            }
            return false;
        case EXTRACT_DRAGON_HEART:
            if (dest_item->is_dragon_soul()) {
                return ds_manager::instance().extract_dragon_heart(this, dest_item, item);
            }
            return false;
        default:
            return false;
        }
    }

    auto character::use_affect_item(item *item, uint32_t affect_type, bool is_cube) -> bool {
        if (find_affect(affect_type, a_apply_info[item->value(1)].point_type)) {
            notify_already_affected();
            return true;
        }

        if (item->is_pc_bang_item() == true) {
            chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템은 PC방에서만 사용할 수 있습니다."));
            return false;
        }

        add_affect(affect_type, a_apply_info[item->value(1)].point_type, item->value(2), 0, item->value(3), 0, false, is_cube);
        item->set_count(item->count() - 1);

        return true;
    }

    void character::add_ability_affect(uint32_t affect_type, int32_t apply, int32_t value, int32_t duration, bool is_cube) {
        uint32_t flag = AFF_NONE;

        switch (apply) {
        case APPLY_MOV_SPEED:
            flag = AFF_MOV_SPEED_POTION;
            break;

        case APPLY_ATT_SPEED:
            flag = AFF_ATT_SPEED_POTION;
            break;

        case APPLY_STR:
        case APPLY_DEX:
        case APPLY_CON:
        case APPLY_INT:
        case APPLY_CAST_SPEED:
        case APPLY_RESIST_MAGIC:
        case APPLY_ATT_GRADE_BONUS:
        case APPLY_DEF_GRADE_BONUS:
            break;

        default:
            return;
        }

        add_affect(affect_type, a_apply_info[apply].point_type, value, flag, duration, 0, true, is_cube);
    }

    void character::consume_potion(item *item) {
        if (get_dungeon())
            get_dungeon()->use_potion(this);

        if (get_war_map())
            get_war_map()->use_potion(this, item);

        item->set_count(item->count() - 1);
    }

    void character::notify_already_affected() {
        chat_packet(CHAT_TYPE_INFO, lc_text("이미 효과가 걸려 있습니다."));
    }

    void character::notify_box_reward(uint32_t vnum, uint32_t count, item *reward) {
        switch (vnum) {
        case special_item_group::GOLD:
            chat_packet(CHAT_TYPE_INFO, lc_text("돈 %d 냥을 획득했습니다."), count);
            break;
        case special_item_group::EXP:
            chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 부터 신비한 빛이 나옵니다."));
            chat_packet(CHAT_TYPE_INFO, lc_text("%d의 경험치를 획득했습니다."), count);
            break;
        case special_item_group::MOB:
            chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 몬스터가 나타났습니다!"));
            break;
        case special_item_group::SLOW:
            chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 나온 빨간 연기를 들이마시자 움직이는 속도가 느려졌습니다!"));
            break;
        case special_item_group::DRAIN_HP:
            chat_packet(CHAT_TYPE_INFO, lc_text("상자가 갑자기 폭발하였습니다! 생명력이 감소했습니다."));
            break;
        case special_item_group::POISON:
            chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 나온 녹색 연기를 들이마시자 독이 온몸으로 퍼집니다!"));
            break;
        case special_item_group::MOB_GROUP:
            chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 몬스터가 나타났습니다!"));
            break;
        default:
            if (reward) {
                if (count > 1)
                    chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 %s 가 %d 개 나왔습니다."), reward->name(), count);
                else
                    chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 %s 가 나왔습니다."), reward->name());
            }
        }
    }

    int portal_limit_time = 10;

    auto character::use_item(item_pos cell_value, item_pos dest_cell) -> bool {
        uint16_t w_cell = cell_value.cell;
        uint8_t window_type = cell_value.window_type;
        item *item;

        if (!can_handle_item())
            return false;

        if (!is_valid_item_position(cell_value) || !(item = get_item(cell_value)))
            return false;

        metin2::log::info("{} uses item {} (window {:d}, cell {:d})", name(), item->name(), window_type, w_cell);

        if (item->is_exchanging())
            return false;

        if (!item->can_used_by(this)) {
            chat_packet(CHAT_TYPE_INFO, lc_text("군직이 맞지않아 이 아이템을 사용할 수 없습니다."));
            return false;
        }

        if (is_stun())
            return false;

        if (false == fn_check_item_sex(this, item)) {
            chat_packet(CHAT_TYPE_INFO, lc_text("성별이 맞지않아 이 아이템을 사용할 수 없습니다."));
            return false;
        }

        if (is_summon_item(item->vnum())) {
            if (false == is_summonable_zone(map_index())) {
                chat_packet(CHAT_TYPE_INFO, lc_text("사용할수 없습니다."));
                return false;
            }

            if (three_way_war::instance().is_three_way_war_map_index(map_index())) {
                chat_packet(CHAT_TYPE_INFO, lc_text("삼거리 전투 참가중에는 귀환부,귀환기억부를 사용할수 없습니다."));
                return false;
            }
            int pulse = thecore_pulse();

            if (pulse - safebox_load_time() < passes_per_sec(portal_limit_time)) {
                chat_packet(CHAT_TYPE_INFO, lc_text("창고를 연후 %d초 이내에는 귀환부,귀환기억부를 사용할 수 없습니다."), portal_limit_time);

                if (metin2::config::test_server())
                    chat_packet(CHAT_TYPE_INFO, "[TestOnly]Pulse %d LoadTime %d PASS %d", pulse, safebox_load_time(), passes_per_sec(portal_limit_time));
                return false;
            }

            if (get_exchange() || my_shop() || shop_owner() || is_open_safebox() || is_cube_open()) {
                chat_packet(CHAT_TYPE_INFO, lc_text("거래창,창고 등을 연 상태에서는 귀환부,귀환기억부 를 사용할수 없습니다."));
                return false;
            }

            {
                if (pulse - get_refine_time() < passes_per_sec(portal_limit_time)) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("아이템 개량후 %d초 이내에는 귀환부,귀환기억부를 사용할 수 없습니다."), portal_limit_time);
                    return false;
                }
            }

            {
                if (pulse - get_my_shop_time() < passes_per_sec(portal_limit_time)) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("개인상점 사용후 %d초 이내에는 귀환부,귀환기억부를 사용할 수 없습니다."), portal_limit_time);
                    return false;
                }
            }

            if (item->vnum() != 70302) {
                pixel_position pos_warp;

                int x_value = 0;
                int y_value = 0;

                double dist = 0;
                const double distant = 5000.0;
                if (item->vnum() == 22010) {
                    x_value = item->get_socket(0) - x();
                    y_value = item->get_socket(1) - y();
                } else if (item->vnum() == 22000) {
                    sectree_manager::instance().recall_position_by_empire(map_index(), empire(), pos_warp);

                    if (item->get_socket(0) == 0) {
                        x_value = pos_warp.x - x();
                        y_value = pos_warp.y - y();
                    } else {
                        x_value = item->get_socket(0) - x();
                        y_value = item->get_socket(1) - y();
                    }
                }

                dist = sqrt(pow((float)x_value, 2) + pow((float)y_value, 2));

                if (distant > dist) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("이동 되어질 위치와 너무 가까워 귀환부를 사용할수 없습니다."));
                    if (metin2::config::test_server())
                        chat_packet(CHAT_TYPE_INFO, "PossibleDistant %f nNowDist %f", distant, dist);
                    return false;
                }
            }

            if (pulse - get_exchange_time() < passes_per_sec(portal_limit_time)) {
                chat_packet(CHAT_TYPE_INFO, lc_text("거래 후 %d초 이내에는 귀환부,귀환기억부등을 사용할 수 없습니다."), portal_limit_time);
                return false;
            }
        }

        if (item->vnum() == 50200 || item->vnum() == 71049) {
            if (get_exchange() || my_shop() || shop_owner() || is_open_safebox() || is_cube_open()) {
                chat_packet(CHAT_TYPE_INFO, lc_text("거래창,창고 등을 연 상태에서는 보따리,비단보따리를 사용할수 없습니다."));
                return false;
            }
        }

        if (is_set(item->flag(), ITEM_FLAG_LOG)) {
            uint32_t vid = item->get_vid();
            uint32_t old_count = item->count();
            uint32_t vnum = item->vnum();

            std::string hint = item->name();

            bool ret = use_item_ex(item, dest_cell);

            if (nullptr == item_manager::instance().find_by_vid(vid)) {
                log_manager::instance().item_log(this, vid, vnum, "REMOVE", hint.c_str());
            } else if (old_count != item->count()) {
                hint += std::format(" {}", old_count - 1);
                log_manager::instance().item_log(this, vid, vnum, "USE_ITEM", hint.c_str());
            }
            return (ret);
        } else
            return use_item_ex(item, dest_cell);
    }

    auto character::drop_item(item_pos cell_value, uint8_t count_value) -> bool {
        item *item = nullptr;

        if (!can_handle_item()) {
            if (nullptr != dragon_soul_refine_window_get_opener())
                chat_packet(CHAT_TYPE_INFO, lc_text("강화창을 연 상태에서는 아이템을 옮길 수 없습니다."));
            return false;
        }

        if (is_dead())
            return false;

        if (!is_valid_item_position(cell_value) || !(item = get_item(cell_value)))
            return false;

        if (item->is_exchanging())
            return false;

        if (true == item->is_locked())
            return false;

        if (quest::quest_manager::instance().pc_force(player_id())->is_running() == true)
            return false;

        if (is_set(item->anti_flag(), ITEM_ANTIFLAG_DROP | ITEM_ANTIFLAG_GIVE)) {
            chat_packet(CHAT_TYPE_INFO, lc_text("버릴 수 없는 아이템입니다."));
            return false;
        }

        if (count_value == 0 || count_value > item->count())
            count_value = item->count();

        sync_quickslot(QUICKSLOT_TYPE_ITEM, cell_value.cell, 255);

        game::items::item *item_to_drop;

        if (count_value == item->count()) {
            item->remove_from_character();
            item_to_drop = item;
        } else {
            if (count_value == 0) {
                if (metin2::config::test_server())
                    metin2::log::debug("Drop item: count is 0");
                return false;
            }

            item->set_count(item->count() - count_value);
            item_manager::instance().flush_delayed_save(item);

            item_to_drop = item_manager::instance().create_item(item->vnum(), count_value);

            fn_copy_item_socket(item_to_drop, item);

            const std::string buf = std::format("{} {}", item_to_drop->id(), item_to_drop->count());
            log_manager::instance().item_log(this, item, "ITEM_SPLIT", buf.c_str());
        }

        pixel_position px_pos = xyz();

        if (thecore_pulse() > last_drop_time + 25)
            count_drops = 0;

        if (thecore_pulse() < last_drop_time + 25 && count_drops >= 4) {
            count_drops = 0;
            metin2::log::warn("{} (pid {:d}) disconnected for drop hacking", name(), player_id());
            get_desc()->set_phase(PHASE_CLOSE);
            return false;
        }

        if (item_to_drop->add_to_ground(map_index(), px_pos)) {

            chat_packet(CHAT_TYPE_INFO, lc_text("떨어진 아이템은 3분 후 사라집니다."));
            item_to_drop->start_destroy_event();

            item_manager::instance().flush_delayed_save(item_to_drop);

            const std::string hint = std::format("{} {} {}", item_to_drop->name(), item_to_drop->count(), item_to_drop->original_vnum());
            log_manager::instance().item_log(this, item_to_drop, "DROP", hint.c_str());

            last_drop_time = thecore_pulse();
            count_drops++;
        }

        return true;
    }

    auto character::drop_gold(int gold_value) -> bool {
        if (gold_value <= 0 || gold_value > gold())
            return false;

        if (!can_handle_item())
            return false;

        if (0 != gold_drop_time_limit_value) {
            if (get_dword_time() < last_gold_drop_time_ + gold_drop_time_limit_value) {
                chat_packet(CHAT_TYPE_INFO, lc_text("아직 골드를 버릴 수 없습니다."));
                return false;
            }
        }

        last_gold_drop_time_ = get_dword_time();

        item *item = item_manager::instance().create_item(1, gold_value);

        if (item) {
            pixel_position pos = xyz();

            if (item->add_to_ground(map_index(), pos)) {
                point_change(POINT_GOLD, -gold_value, true);

                if (gold_value > 1000)
                    log_manager::instance().char_log(this, gold_value, "DROP_GOLD", "");

                item->start_destroy_event(150);
                chat_packet(CHAT_TYPE_INFO, lc_text("떨어진 아이템은 %d분 후 사라집니다."), 150 / 60);
            }

            save();
            return true;
        }

        return false;
    }

    auto character::move_item(item_pos cell_value, item_pos dest_cell, uint8_t count) -> bool {
        if (cell_value.is_same_position(dest_cell))
            return false;

        item *item = nullptr;

        if (!is_valid_item_position(cell_value))
            return false;

        if (!(item = get_item(cell_value)))
            return false;

        if (item->is_exchanging())
            return false;

        if (item->count() < count)
            return false;

        if (INVENTORY == cell_value.window_type && cell_value.cell >= INVENTORY_MAX_NUM && is_set(item->flag(), ITEM_FLAG_IRREMOVABLE))
            return false;

        if (true == item->is_locked())
            return false;

        if (!is_valid_item_position(dest_cell)) {
            return false;
        }

        if (!can_handle_item()) {
            if (nullptr != dragon_soul_refine_window_get_opener())
                chat_packet(CHAT_TYPE_INFO, lc_text("강화창을 연 상태에서는 아이템을 옮길 수 없습니다."));
            return false;
        }

        if (dest_cell.is_belt_inventory_position() && false == belt_inventory_helper::can_move_into_belt_inventory(item)) {
            chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템은 벨트 인벤토리로 옮길 수 없습니다."));
            return false;
        }

        if (cell_value.is_equip_position() && !can_unequip_now(item))
            return false;

        if (dest_cell.is_equip_position()) {
            if (get_item(dest_cell)) {
                chat_packet(CHAT_TYPE_INFO, lc_text("이미 장비를 착용하고 있습니다."));

                return false;
            }

            equip_item(item, dest_cell.cell - INVENTORY_MAX_NUM);
        } else {
            if (item->is_dragon_soul()) {
                if (item->is_equipped()) {
                    return ds_manager::instance().pull_out(this, dest_cell, item);
                } else {
                    if (dest_cell.window_type != DRAGON_SOUL_INVENTORY) {
                        return false;
                    }

                    if (!ds_manager::instance().is_valid_cell_for_this_item(item, dest_cell))
                        return false;
                }
            } else if (DRAGON_SOUL_INVENTORY == dest_cell.window_type)
                return false;

            game::items::item *item2;

            if ((item2 = get_item(dest_cell)) && item != item2 && item2->is_stackable() &&
                !is_set(item2->anti_flag(), ITEM_ANTIFLAG_STACK) &&
                item2->vnum() == item->vnum()) {
                for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
                    if (item2->get_socket(i) != item->get_socket(i))
                        return false;

                if (count == 0)
                    count = item->count();

                metin2::log::debug("{}: stack {} from window {:d} cell {:d} to window {:d} cell {:d}, count {:d}", name(), item->name(), cell_value.window_type, cell_value.cell,
                    dest_cell.window_type, dest_cell.cell, count);

                count = std::min<int>(200 - item2->count(), count);

                item->set_count(item->count() - count);
                item2->set_count(item2->count() + count);
                return true;
            }

            if (!is_empty_item_grid(dest_cell, item->size(), cell_value.cell))
                return false;

            if (count == 0 || count >= item->count() || !item->is_stackable() || is_set(item->anti_flag(), ITEM_ANTIFLAG_STACK)) {
                metin2::log::debug("{}: move {} from window {:d} cell {:d} to window {:d} cell {:d}, count {:d}", name(), item->name(), cell_value.window_type, cell_value.cell,
                    dest_cell.window_type, dest_cell.cell, count);

                item->remove_from_character();
                set_item(dest_cell, item);

                if (INVENTORY == cell_value.window_type && INVENTORY == dest_cell.window_type)
                    sync_quickslot(QUICKSLOT_TYPE_ITEM, cell_value.cell, dest_cell.cell);
            } else if (count < item->count()) {

                metin2::log::debug("{}: split {} from window {:d} cell {:d} to window {:d} cell {:d}, count {:d}", name(), item->name(), cell_value.window_type, cell_value.cell,
                    dest_cell.window_type, dest_cell.cell, count);

                item->set_count(item->count() - count);
                game::items::item *item2 = item_manager::instance().create_item(item->vnum(), count);

                fn_copy_item_socket(item2, item);

                item2->add_to_character(this, dest_cell);

                const std::string buf = std::format("{} {} {} {} ", item2->id(), item2->count(), item->count(), item->count() + item2->count());
                log_manager::instance().item_log(this, item, "ITEM_SPLIT", buf.c_str());
            }
        }

        return true;
    }

    void character::give_gold(int amount) {
        if (amount <= 0)
            return;

        metin2::log::info("{} receives {:d} gold", name(), amount);

        if (get_party()) {
            party *party = get_party();

            uint32_t total_value = amount;
            uint32_t my_amount = total_value;

            const int center_x = x();
            const int center_y = y();
            int total = 0;

            party->for_each_online_member([&](character *ch) {
                if (distance_approx(ch->x() - center_x, ch->y() - center_y) <= PARTY_DEFAULT_RANGE)
                    total += 1;
            });

            if (total > 1) {
                uint32_t share = total_value / total;
                my_amount -= share * (total - 1);

                const int money = share;

                party->for_each_online_member([&](character *ch) {
                    if (ch != this)
                        if (distance_approx(ch->x() - center_x, ch->y() - center_y) <= PARTY_DEFAULT_RANGE) {
                            ch->point_change(POINT_GOLD, money, true);

                            if (money > 1000)
                                log_manager::instance().char_log(ch, money, "GET_GOLD", "");
                        }
                });
            }

            point_change(POINT_GOLD, my_amount, true);

            if (my_amount > 1000)
                log_manager::instance().char_log(this, my_amount, "GET_GOLD", "");
        } else {
            point_change(POINT_GOLD, amount, true);

            if (amount > 1000)
                log_manager::instance().char_log(this, amount, "GET_GOLD", "");
        }
    }

    auto character::pickup_item(uint32_t vid) -> bool {
        item *item = item_manager::instance().find_by_vid(vid);

        if (is_observer_mode())
            return false;

        if (!item || !item->get_sectree())
            return false;

        if (item->distance_valid(this)) {
            if (item->is_ownership(this)) {
                if (item->type() == ITEM_ELK) {
                    give_gold(item->count());
                    item->remove_from_ground();

                    item_manager::instance().destroy_item(item);

                    save();
                } else {
                    if (item->is_stackable() && !is_set(item->anti_flag(), ITEM_ANTIFLAG_STACK)) {
                        uint8_t count = item->count();

                        for (int i = 0; i < INVENTORY_MAX_NUM; ++i) {
                            game::items::item *item2 = inventory_item(i);

                            if (!item2)
                                continue;

                            if (item2->vnum() == item->vnum()) {
                                int j;

                                for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
                                    if (item2->get_socket(j) != item->get_socket(j))
                                        break;

                                if (j != ITEM_SOCKET_MAX_NUM)
                                    continue;

                                uint8_t count2 = std::min<int>(200 - item2->count(), count);
                                count -= count2;

                                item2->set_count(item2->count() + count2);

                                if (count == 0) {
                                    chat_packet(CHAT_TYPE_INFO, lc_text("아이템 획득: %s"), item2->name());
                                    item_manager::instance().destroy_item(item);
                                    if (item2->type() == ITEM_QUEST)
                                        quest::quest_manager::instance().pickup_item(player_id(), item2);
                                    return true;
                                }
                            }
                        }

                        item->set_count(count);
                    }

                    int empty_cell;
                    if (item->is_dragon_soul()) {
                        if ((empty_cell = empty_dragon_soul_inventory(item)) == -1) {
                            metin2::log::debug("No free dragon soul inventory slot for pid {:d} (item size {:d}, id {:d})", player_id(), item->size(), item->id());
                            chat_packet(CHAT_TYPE_INFO, lc_text("소지하고 있는 아이템이 너무 많습니다."));
                            return false;
                        }
                    } else {
                        if ((empty_cell = empty_inventory(item->size())) == -1) {
                            metin2::log::debug("No free inventory slot for pid {:d} (item size {:d}, id {:d})", player_id(), item->size(), item->id());
                            chat_packet(CHAT_TYPE_INFO, lc_text("소지하고 있는 아이템이 너무 많습니다."));
                            return false;
                        }
                    }

                    item->remove_from_ground();

                    if (item->is_dragon_soul())
                        item->add_to_character(this, item_pos(DRAGON_SOUL_INVENTORY, empty_cell));
                    else
                        item->add_to_character(this, item_pos(INVENTORY, empty_cell));

                    const std::string hint = std::format("{} {} {}", item->name(), item->count(), item->original_vnum());
                    log_manager::instance().item_log(this, item, "GET", hint.c_str());
                    chat_packet(CHAT_TYPE_INFO, lc_text("아이템 획득: %s"), item->name());

                    if (item->type() == ITEM_QUEST)
                        quest::quest_manager::instance().pickup_item(player_id(), item);
                }

                return true;
            } else if (!is_set(item->anti_flag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_DROP) && get_party()) {
                character *owner = nullptr;

                get_party()->for_each_online_member([&](character *ch) {
                    if (item->is_ownership(ch))
                        owner = ch;
                });

                int empty_cell;

                if (item->is_dragon_soul()) {
                    if (!(owner && (empty_cell = owner->empty_dragon_soul_inventory(item)) != -1)) {
                        owner = this;

                        if ((empty_cell = empty_dragon_soul_inventory(item)) == -1) {
                            owner->chat_packet(CHAT_TYPE_INFO, lc_text("소지하고 있는 아이템이 너무 많습니다."));
                            return false;
                        }
                    }
                } else {
                    if (!(owner && (empty_cell = owner->empty_inventory(item->size())) != -1)) {
                        owner = this;

                        if ((empty_cell = empty_inventory(item->size())) == -1) {
                            owner->chat_packet(CHAT_TYPE_INFO, lc_text("소지하고 있는 아이템이 너무 많습니다."));
                            return false;
                        }
                    }
                }

                item->remove_from_ground();

                if (item->is_dragon_soul())
                    item->add_to_character(owner, item_pos(DRAGON_SOUL_INVENTORY, empty_cell));
                else
                    item->add_to_character(owner, item_pos(INVENTORY, empty_cell));

                const std::string hint = std::format("{} {} {}", item->name(), item->count(), item->original_vnum());
                log_manager::instance().item_log(owner, item, "GET", hint.c_str());

                if (owner == this)
                    chat_packet(CHAT_TYPE_INFO, lc_text("아이템 획득: %s"), item->name());
                else {
                    owner->chat_packet(CHAT_TYPE_INFO, lc_text("아이템 획득: %s 님으로부터 %s"), name(), item->name());
                    chat_packet(CHAT_TYPE_INFO, lc_text("아이템 전달: %s 님에게 %s"), owner->name(), item->name());
                }

                if (item->type() == ITEM_QUEST)
                    quest::quest_manager::instance().pickup_item(owner->player_id(), item);

                return true;
            }
        }

        return false;
    }

    auto character::swap_item(uint8_t cell_value, uint8_t dest_cell) -> bool {
        if (!can_handle_item())
            return false;

        item_pos src_cell(INVENTORY, cell_value), dest_cell_value(INVENTORY, dest_cell);

        if (src_cell.is_dragon_soul_equip_position() || dest_cell_value.is_dragon_soul_equip_position())
            return false;

        if (cell_value == dest_cell)
            return false;

        if (src_cell.is_equip_position() && dest_cell_value.is_equip_position())
            return false;

        item *item1;
        item *item2;

        if (src_cell.is_equip_position()) {
            item1 = inventory_item(dest_cell);
            item2 = inventory_item(cell_value);
        } else {
            item1 = inventory_item(cell_value);
            item2 = inventory_item(dest_cell);
        }

        if (!item1 || !item2)
            return false;

        if (item1 == item2) {
            metin2::log::warn("Possible hack by {}: swapping an item with itself (cells {:d}, {:d})", name_, cell_value, dest_cell);
            return false;
        }

        if (!is_empty_item_grid(item_pos(INVENTORY, item1->cell()), item2->size(), item1->cell()))
            return false;

        if (item_pos(EQUIPMENT, item2->cell()).is_equip_position()) {
            uint8_t equip_cell = item2->cell() - INVENTORY_MAX_NUM;
            uint8_t inven_cell = item1->cell();

            if (false == can_unequip_now(item2) || false == can_equip_now(item1))
                return false;

            if (equip_cell != item1->find_equip_cell(this))
                return false;

            item2->remove_from_character();

            if (item1->equip_to(this, equip_cell))
                item2->add_to_character(this, item_pos(INVENTORY, inven_cell));
            else
                metin2::log::error("Cannot equip {} when swapping with {}", item2->name(), item1->name());
        } else {
            uint8_t cell1 = item1->cell();
            uint8_t cell2 = item2->cell();

            item1->remove_from_character();
            item2->remove_from_character();

            item1->add_to_character(this, item_pos(INVENTORY, cell2));
            item2->add_to_character(this, item_pos(INVENTORY, cell1));
        }

        return true;
    }

    auto character::unequip_item(item *item) -> bool {
        int pos;

        if (false == can_unequip_now(item))
            return false;

        if (item->is_dragon_soul())
            pos = empty_dragon_soul_inventory(item);
        else
            pos = empty_inventory(item->size());

        if (item->vnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
            show_alignment(true);

        item->remove_from_character();
        if (item->is_dragon_soul()) {
            item->add_to_character(this, item_pos(DRAGON_SOUL_INVENTORY, pos));
        } else
            item->add_to_character(this, item_pos(INVENTORY, pos));

        check_maximum_points();

        return true;
    }

    auto character::equip_item(item *item, int candidate_cell) -> bool {
        if (item->is_exchanging())
            return false;

        if (false == item->is_equipable())
            return false;

        if (false == can_equip_now(item))
            return false;

        int wear_cell = item->find_equip_cell(this, candidate_cell);

        if (wear_cell < 0)
            return false;

        if (wear_cell == WEAR_BODY && is_riding() && (item->vnum() >= 11901 && item->vnum() <= 11904)) {
            chat_packet(CHAT_TYPE_INFO, lc_text("말을 탄 상태에서 예복을 입을 수 없습니다."));
            return false;
        }

        if (wear_cell != WEAR_ARROW && is_polymorphed()) {
            chat_packet(CHAT_TYPE_INFO, lc_text("둔갑 중에는 착용중인 장비를 변경할 수 없습니다."));
            return false;
        }

        if (fn_check_item_sex(this, item) == false) {
            chat_packet(CHAT_TYPE_INFO, lc_text("성별이 맞지않아 이 아이템을 사용할 수 없습니다."));
            return false;
        }

        if (item->is_ride_item() && is_riding()) {
            chat_packet(CHAT_TYPE_INFO, lc_text("이미 탈것을 이용중입니다."));
            return false;
        }

        uint32_t cur_time = get_dword_time();

        if (wear_cell != WEAR_ARROW && (cur_time - last_attack_time() <= 1500 || cur_time - last_skill_time_ <= 1500)) {
            chat_packet(CHAT_TYPE_INFO, lc_text("가만히 있을 때만 착용할 수 있습니다."));
            return false;
        }

        if (item->is_dragon_soul()) {
            if (inventory_item(INVENTORY_MAX_NUM + wear_cell)) {
                chat_packet(CHAT_TYPE_INFO, "이미 같은 종류의 용혼석을 착용하고 있습니다.");
                return false;
            }

            if (!item->equip_to(this, wear_cell)) {
                return false;
            }
        } else {
            if (wear(wear_cell) && !is_set(wear(wear_cell)->flag(), ITEM_FLAG_IRREMOVABLE)) {
                if (item->wear_flag() == WEARABLE_ABILITY)
                    return false;

                if (false == swap_item(item->cell(), INVENTORY_MAX_NUM + wear_cell)) {
                    return false;
                }
            } else {
                uint8_t old_cell = item->cell();

                if (item->equip_to(this, wear_cell)) {
                    sync_quickslot(QUICKSLOT_TYPE_ITEM, old_cell, wear_cell);
                }
            }
        }

        if (true == item->is_equipped()) {
            if (-1 != item->proto()->c_limit_real_time_first_use_index) {
                if (0 == item->get_socket(1)) {
                    int32_t duration = (0 != item->get_socket(0)) ? item->get_socket(0) : item->proto()->limits[static_cast<int>(item->proto()->c_limit_real_time_first_use_index)].value;

                    if (0 == duration)
                        duration = 60 * 60 * 24 * 7;

                    item->set_socket(0, get_global_time() + duration);
                    item->start_real_time_expire_event();
                }

                item->set_socket(1, item->get_socket(1) + 1);
            }

            if (item->vnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
                show_alignment(false);

            const uint32_t &vnum = item->vnum();

            if (item_vnum::is_ramadan_moon_ring(vnum)) {
                this->effect_packet(SE_EQUIP_RAMADAN_RING);
            } else if (item_vnum::is_halloween_candy(vnum)) {
                this->effect_packet(SE_EQUIP_HALLOWEEN_CANDY);
            } else if (item_vnum::is_happiness_ring(vnum)) {
                this->effect_packet(SE_EQUIP_HAPPINESS_RING);
            } else if (item_vnum::is_love_pendant(vnum)) {
                this->effect_packet(SE_EQUIP_LOVE_PENDANT);
            } else if (ITEM_UNIQUE == item->type() && 0 != item->sig_vnum()) {
                const special_item_group *group = item_manager::instance().get_special_item_group(item->sig_vnum());
                if (nullptr != group) {
                    const special_attr_group *attr_group = item_manager::instance().get_special_attr_group(group->attr_vnum(item->vnum()));
                    if (nullptr != attr_group) {
                        const std::string &std = attr_group->effect_file_name;
                        specific_effect_packet(std.c_str());
                    }
                }
            }

            if (UNIQUE_SPECIAL_RIDE == item->sub_type() && is_set(item->flag(), ITEM_FLAG_QUEST_USE)) {
                quest::quest_manager::instance().use_item(player_id(), item, false);
            }
        }

        return true;
    }

    void character::buff_on_attr_add_buffs_from_item(item *item_value) {
        for (size_t i = 0; i < std::size(buff_on_attr_points); i++) {
            auto it = map_buff_on_attrs_.find(buff_on_attr_points[i]);
            if (it != map_buff_on_attrs_.end()) {
                it->second->add_buff_from_item(item_value);
            }
        }
    }

    void character::buff_on_attr_remove_buffs_from_item(item *item_value) {
        for (size_t i = 0; i < std::size(buff_on_attr_points); i++) {
            auto it = map_buff_on_attrs_.find(buff_on_attr_points[i]);
            if (it != map_buff_on_attrs_.end()) {
                it->second->remove_buff_from_item(item_value);
            }
        }
    }

    void character::buff_on_attr_clear_all() {
        for (auto it = map_buff_on_attrs_.begin(); it != map_buff_on_attrs_.end(); it++) {
            buff_on_attributes *buff = it->second.get();
            if (buff) {
                buff->initialize();
            }
        }
    }

    void character::buff_on_attr_value_change(uint8_t type, uint8_t old_value, uint8_t new_value) {
        auto it = map_buff_on_attrs_.find(type);

        if (0 == new_value) {
            if (map_buff_on_attrs_.end() == it)
                return;
            else
                it->second->off();
        } else if (0 == old_value) {
            buff_on_attributes *buff;
            if (map_buff_on_attrs_.end() == it) {
                std::unique_ptr<buff_on_attributes> new_buff;
                switch (type) {
                case POINT_ENERGY:
                    {
                        static uint8_t ab_slot[] = {WEAR_BODY, WEAR_HEAD, WEAR_FOOTS, WEAR_WRIST, WEAR_WEAPON, WEAR_NECK, WEAR_EAR, WEAR_SHIELD};
                        static std::vector<uint8_t> vec_slots(ab_slot, ab_slot + std::size(ab_slot));
                        new_buff = std::make_unique<buff_on_attributes>(this, type, &vec_slots);
                    }
                    break;
                case POINT_COSTUME_ATTR_BONUS:
                    {
                        static uint8_t ab_slot[] = {WEAR_COSTUME_BODY, WEAR_COSTUME_HAIR};
                        static std::vector<uint8_t> vec_slots(ab_slot, ab_slot + std::size(ab_slot));
                        new_buff = std::make_unique<buff_on_attributes>(this, type, &vec_slots);
                    }
                    break;
                default:
                    break;
                }
                buff = new_buff.get();
                map_buff_on_attrs_.insert(map_buff_on_attrs::value_type(type, std::move(new_buff)));

            } else
                buff = it->second.get();

            buff->on(new_value);
        } else {
            if (map_buff_on_attrs_.end() == it)
                return;
            else
                it->second->change_buff_value(new_value);
        }
    }

    auto character::find_specify_item(uint32_t vnum) const -> item * {
        for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
            if (inventory_item(i) && inventory_item(i)->vnum() == vnum)
                return inventory_item(i);

        return nullptr;
    }

    auto character::find_item_by_id(uint32_t id) const -> item * {
        for (int i = 0; i < INVENTORY_MAX_NUM; ++i) {
            if (nullptr != inventory_item(i) && inventory_item(i)->id() == id)
                return inventory_item(i);
        }

        for (int i = BELT_INVENTORY_SLOT_START; i < BELT_INVENTORY_SLOT_END; ++i) {
            if (nullptr != inventory_item(i) && inventory_item(i)->id() == id)
                return inventory_item(i);
        }

        return nullptr;
    }

    auto character::count_specify_item(uint32_t vnum) const -> int {
        int count = 0;
        item *item;

        for (int i = 0; i < INVENTORY_MAX_NUM; ++i) {
            item = inventory_item(i);
            if (nullptr != item && item->vnum() == vnum) {
                if (my_shop_ && my_shop_->is_selling_item(item->id())) {
                    continue;
                } else {
                    count += item->count();
                }
            }
        }

        return count;
    }

    void character::remove_specify_item(uint32_t vnum, uint32_t count) {
        if (0 == count)
            return;

        for (uint32_t i = 0; i < INVENTORY_MAX_NUM; ++i) {
            if (nullptr == inventory_item(i))
                continue;

            if (inventory_item(i)->vnum() != vnum)
                continue;

            if (my_shop_) {
                bool is_item_selling = my_shop_->is_selling_item(inventory_item(i)->id());
                if (is_item_selling)
                    continue;
            }

            if (vnum >= 80003 && vnum <= 80007)
                log_manager::instance().gold_bar_log(player_id(), inventory_item(i)->id(), QUEST, "RemoveSpecifyItem");

            if (count >= inventory_item(i)->count()) {
                count -= inventory_item(i)->count();
                inventory_item(i)->set_count(0);

                if (0 == count)
                    return;
            } else {
                inventory_item(i)->set_count(inventory_item(i)->count() - count);
                return;
            }
        }

        if (count)
            metin2::log::warn("Cannot remove enough of item {:d}: {:d} still remaining", vnum, count);
    }

    auto character::count_specify_type_item(uint8_t type) const -> int {
        int count = 0;

        for (int i = 0; i < INVENTORY_MAX_NUM; ++i) {
            item *item = inventory_item(i);
            if (item != nullptr && item->type() == type) {
                count += item->count();
            }
        }

        return count;
    }

    void character::remove_specify_type_item(uint8_t type, uint32_t count) {
        if (0 == count)
            return;

        for (uint32_t i = 0; i < INVENTORY_MAX_NUM; ++i) {
            if (nullptr == inventory_item(i))
                continue;

            if (inventory_item(i)->type() != type)
                continue;

            if (my_shop_) {
                bool is_item_selling = my_shop_->is_selling_item(inventory_item(i)->id());
                if (is_item_selling)
                    continue;
            }

            if (count >= inventory_item(i)->count()) {
                count -= inventory_item(i)->count();
                inventory_item(i)->set_count(0);

                if (0 == count)
                    return;
            } else {
                inventory_item(i)->set_count(inventory_item(i)->count() - count);
                return;
            }
        }
    }

    void character::auto_give_item(item *item, bool long_owner_ship) {
        if (nullptr == item) {
            metin2::log::error("Cannot auto-give a null item");
            return;
        }
        if (item->owner()) {
            metin2::log::error("Cannot auto-give item {:d}: it already has an owner", item->id());
            return;
        }

        int cell;
        if (item->is_dragon_soul()) {
            cell = empty_dragon_soul_inventory(item);
        } else {
            cell = empty_inventory(item->size());
        }

        if (cell != -1) {
            if (item->is_dragon_soul())
                item->add_to_character(this, item_pos(DRAGON_SOUL_INVENTORY, cell));
            else
                item->add_to_character(this, item_pos(INVENTORY, cell));

            log_manager::instance().item_log(this, item, "SYSTEM", item->name());

            if (item->type() == ITEM_USE && item->sub_type() == USE_POTION) {
                quickslot *slot_value;

                if (get_quickslot(0, &slot_value) && slot_value->type == QUICKSLOT_TYPE_NONE) {
                    quickslot slot;
                    slot.type = QUICKSLOT_TYPE_ITEM;
                    slot.pos = cell;
                    set_quickslot(0, slot);
                }
            }
        } else {
            item->add_to_ground(map_index(), xyz());
            item->start_destroy_event();

            if (long_owner_ship)
                item->set_ownership(this, 300);
            else
                item->set_ownership(this, 60);
            log_manager::instance().item_log(this, item, "SYSTEM_DROP", item->name());
        }
    }

    auto character::auto_give_item(uint32_t item_vnum, uint8_t count_value, int rare_pct, bool msg) -> item * {
        item_table *p = item_manager::instance().table(item_vnum);

        if (!p)
            return nullptr;

        db_manager::instance().send_money_log(MONEY_LOG_DROP, item_vnum, count_value);

        if (p->flags & ITEM_FLAG_STACKABLE && p->type != ITEM_BLEND) {
            for (int i = 0; i < INVENTORY_MAX_NUM; ++i) {
                item *item = inventory_item(i);

                if (!item)
                    continue;

                if (item->vnum() == item_vnum && fn_check_item_socket(item)) {
                    if (is_set(p->flags, ITEM_FLAG_MAKECOUNT)) {
                        if (count_value < p->values[1])
                            count_value = p->values[1];
                    }

                    uint8_t count2 = std::min<int>(200 - item->count(), count_value);
                    count_value -= count2;

                    item->set_count(item->count() + count2);

                    if (count_value == 0) {
                        if (msg)
                            chat_packet(CHAT_TYPE_INFO, lc_text("아이템 획득: %s"), item->name());

                        return item;
                    }
                }
            }
        }

        game::items::item *item = item_manager::instance().create_item(item_vnum, count_value, 0, true);

        if (!item) {
            metin2::log::error("Cannot create item vnum {:d} for {}", item_vnum, name());
            return nullptr;
        }

        if (item->type() == ITEM_BLEND) {
            for (int i = 0; i < INVENTORY_MAX_NUM; i++) {
                game::items::item *inv_item = inventory_item(i);

                if (inv_item == nullptr) continue;

                if (inv_item->type() == ITEM_BLEND) {
                    if (inv_item->vnum() == item->vnum()) {
                        if (inv_item->get_socket(0) == item->get_socket(0) &&
                            inv_item->get_socket(1) == item->get_socket(1) &&
                            inv_item->get_socket(2) == item->get_socket(2) &&
                            inv_item->count() < ITEM_MAX_COUNT) {
                            inv_item->set_count(inv_item->count() + item->count());
                            return inv_item;
                        }
                    }
                }
            }
        }

        int empty_cell;
        if (item->is_dragon_soul()) {
            empty_cell = empty_dragon_soul_inventory(item);
        } else
            empty_cell = empty_inventory(item->size());

        if (empty_cell != -1) {
            if (msg)
                chat_packet(CHAT_TYPE_INFO, lc_text("아이템 획득: %s"), item->name());

            if (item->is_dragon_soul())
                item->add_to_character(this, item_pos(DRAGON_SOUL_INVENTORY, empty_cell));
            else
                item->add_to_character(this, item_pos(INVENTORY, empty_cell));
            log_manager::instance().item_log(this, item, "SYSTEM", item->name());

            if (item->type() == ITEM_USE && item->sub_type() == USE_POTION) {
                quickslot *slot_value;

                if (get_quickslot(0, &slot_value) && slot_value->type == QUICKSLOT_TYPE_NONE) {
                    quickslot slot;
                    slot.type = QUICKSLOT_TYPE_ITEM;
                    slot.pos = empty_cell;
                    set_quickslot(0, slot);
                }
            }
        } else {
            item->add_to_ground(map_index(), xyz());
            item->start_destroy_event();
            if (is_set(item->anti_flag(), ITEM_ANTIFLAG_DROP))
                item->set_ownership(this, 300);
            else
                item->set_ownership(this, 60);
            log_manager::instance().item_log(this, item, "SYSTEM_DROP", item->name());
        }

        metin2::log::debug("Auto-give item vnum {:d}, count {:d}", item_vnum, count_value);
        return item;
    }

    auto character::give_item(character *victim, item_pos cell) -> bool {
        if (!can_handle_item())
            return false;

        item *item = get_item(cell);

        if (item && !item->is_exchanging()) {
            if (victim->can_receive_item(this, item)) {
                victim->receive_item(this, item);
                return true;
            }
        }

        return false;
    }

    auto character::can_receive_item(character *from, item *item) const -> bool {
        if (is_pc())
            return false;

        if (distance_approx(x() - from->x(), y() - from->y()) > 2000)
            return false;

        switch (race_num()) {
        case fishing::CAMPFIRE_MOB:
            if (item->type() == ITEM_FISH &&
                (item->sub_type() == FISH_ALIVE || item->sub_type() == FISH_DEAD))
                return true;
            break;

        case fishing::FISHER_MOB:
            if (item->type() == ITEM_ROD)
                return true;
            break;

        case BLACKSMITH_WEAPON_MOB:
        case DEVILTOWER_BLACKSMITH_WEAPON_MOB:
            if (item->type() == ITEM_WEAPON &&
                item->refined_vnum())
                return true;
            else
                return false;
            break;

        case BLACKSMITH_ARMOR_MOB:
        case DEVILTOWER_BLACKSMITH_ARMOR_MOB:
            if (item->type() == ITEM_ARMOR &&
                (item->sub_type() == ARMOR_BODY || item->sub_type() == ARMOR_SHIELD || item->sub_type() == ARMOR_HEAD) &&
                item->refined_vnum())
                return true;
            else
                return false;
            break;

        case BLACKSMITH_ACCESSORY_MOB:
        case DEVILTOWER_BLACKSMITH_ACCESSORY_MOB:
            if (item->type() == ITEM_ARMOR &&
                !(item->sub_type() == ARMOR_BODY || item->sub_type() == ARMOR_SHIELD || item->sub_type() == ARMOR_HEAD) &&
                item->refined_vnum())
                return true;
            else
                return false;
            break;

        case BLACKSMITH_MOB:
            if (item->refined_vnum() && item->refine_set() < 500) {
                return true;
            } else {
                return false;
            }

        case BLACKSMITH2_MOB:
            if (item->refine_set() >= 500) {
                return true;
            } else {
                return false;
            }

        case ALCHEMIST_MOB:
            if (item->refined_vnum())
                return true;
            break;

        case 20101:
        case 20102:
        case 20103:
            if (item->vnum() == ITEM_REVIVE_HORSE_1) {
                if (!is_dead()) {
                    from->chat_packet(CHAT_TYPE_INFO, lc_text("죽지 않은 말에게 선초를 먹일 수 없습니다."));
                    return false;
                }
                return true;
            } else if (item->vnum() == ITEM_HORSE_FOOD_1) {
                if (is_dead()) {
                    from->chat_packet(CHAT_TYPE_INFO, lc_text("죽은 말에게 사료를 먹일 수 없습니다."));
                    return false;
                }
                return true;
            } else if (item->vnum() == ITEM_HORSE_FOOD_2 || item->vnum() == ITEM_HORSE_FOOD_3) {
                return false;
            }
            break;
        case 20104:
        case 20105:
        case 20106:
            if (item->vnum() == ITEM_REVIVE_HORSE_2) {
                if (!is_dead()) {
                    from->chat_packet(CHAT_TYPE_INFO, lc_text("죽지 않은 말에게 선초를 먹일 수 없습니다."));
                    return false;
                }
                return true;
            } else if (item->vnum() == ITEM_HORSE_FOOD_2) {
                if (is_dead()) {
                    from->chat_packet(CHAT_TYPE_INFO, lc_text("죽은 말에게 사료를 먹일 수 없습니다."));
                    return false;
                }
                return true;
            } else if (item->vnum() == ITEM_HORSE_FOOD_1 || item->vnum() == ITEM_HORSE_FOOD_3) {
                return false;
            }
            break;
        case 20107:
        case 20108:
        case 20109:
            if (item->vnum() == ITEM_REVIVE_HORSE_3) {
                if (!is_dead()) {
                    from->chat_packet(CHAT_TYPE_INFO, lc_text("죽지 않은 말에게 선초를 먹일 수 없습니다."));
                    return false;
                }
                return true;
            } else if (item->vnum() == ITEM_HORSE_FOOD_3) {
                if (is_dead()) {
                    from->chat_packet(CHAT_TYPE_INFO, lc_text("죽은 말에게 사료를 먹일 수 없습니다."));
                    return false;
                }
                return true;
            } else if (item->vnum() == ITEM_HORSE_FOOD_1 || item->vnum() == ITEM_HORSE_FOOD_2) {
                return false;
            }
            break;
        }

        {
            return true;
        }

        return false;
    }

    void character::receive_item(character *from, item *item) {
        if (is_pc())
            return;

        switch (race_num()) {
        case fishing::CAMPFIRE_MOB:
            if (item->type() == ITEM_FISH && (item->sub_type() == FISH_ALIVE || item->sub_type() == FISH_DEAD))
                fishing::grill(from, item);
            else {
                from->set_quest_npcid(get_vid());
                quest::quest_manager::instance().take_item(from->player_id(), race_num(), item);
            }
            break;

        case DEVILTOWER_BLACKSMITH_WEAPON_MOB:
        case DEVILTOWER_BLACKSMITH_ARMOR_MOB:
        case DEVILTOWER_BLACKSMITH_ACCESSORY_MOB:
            if (item->refined_vnum() != 0 && item->refine_set() != 0 && item->refine_set() < 500) {
                from->set_refine_npc(this);
                from->refine_information(item->cell(), REFINE_TYPE_MONEY_ONLY);
            } else {
                from->chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템은 개량할 수 없습니다."));
            }
            break;

        case BLACKSMITH_MOB:
        case BLACKSMITH2_MOB:
        case BLACKSMITH_WEAPON_MOB:
        case BLACKSMITH_ARMOR_MOB:
        case BLACKSMITH_ACCESSORY_MOB:
            if (item->refined_vnum()) {
                from->set_refine_npc(this);
                from->refine_information(item->cell(), REFINE_TYPE_NORMAL);
            } else {
                from->chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템은 개량할 수 없습니다."));
            }
            break;

        case 20101:
        case 20102:
        case 20103:
        case 20104:
        case 20105:
        case 20106:
        case 20107:
        case 20108:
        case 20109:
            if (item->vnum() == ITEM_REVIVE_HORSE_1 ||
                item->vnum() == ITEM_REVIVE_HORSE_2 ||
                item->vnum() == ITEM_REVIVE_HORSE_3) {
                from->revive_horse();
                item->set_count(item->count() - 1);
                from->chat_packet(CHAT_TYPE_INFO, lc_text("말에게 선초를 주었습니다."));
            } else if (item->vnum() == ITEM_HORSE_FOOD_1 ||
                       item->vnum() == ITEM_HORSE_FOOD_2 ||
                       item->vnum() == ITEM_HORSE_FOOD_3) {
                from->feed_horse();
                from->chat_packet(CHAT_TYPE_INFO, lc_text("말에게 사료를 주었습니다."));
                item->set_count(item->count() - 1);
                effect_packet(SE_HPUP_RED);
            }
            break;

        default:
            metin2::log::debug("{} gives npc {:d} item {}", from->name(), race_num(), item->name());
            from->set_quest_npcid(get_vid());
            quest::quest_manager::instance().take_item(from->player_id(), race_num(), item);
            break;
        }
    }

    auto character::is_equip_unique_item(uint32_t item_vnum) const -> bool {
        {
            item *u = wear(WEAR_UNIQUE1);

            if (u && u->vnum() == item_vnum)
                return true;
        }

        {
            item *u = wear(WEAR_UNIQUE2);

            if (u && u->vnum() == item_vnum)
                return true;
        }

        if (item_vnum == UNIQUE_ITEM_RING_OF_LANGUAGE)
            return is_equip_unique_item(UNIQUE_ITEM_RING_OF_LANGUAGE_SAMPLE);

        return false;
    }

    auto character::is_equip_unique_group(uint32_t group_vnum) const -> bool {
        {
            item *u = wear(WEAR_UNIQUE1);

            if (u && u->special_group() == (int)group_vnum)
                return true;
        }

        {
            item *u = wear(WEAR_UNIQUE2);

            if (u && u->special_group() == (int)group_vnum)
                return true;
        }

        return false;
    }

    void character::set_refine_mode(int additional_cell) {
        refine_additional_cell_ = additional_cell;
        under_refine_ = true;
    }

    void character::clear_refine_mode() {
        under_refine_ = false;
        set_refine_npc(nullptr);
    }

    auto character::give_item_from_special_item_group(uint32_t group_num, std::vector<uint32_t> &item_vnums,
        std::vector<uint32_t> &item_counts, std::vector<item *> &item_gets, int &count) -> bool {
        const special_item_group *group_value = item_manager::instance().get_special_item_group(group_num);

        if (!group_value) {
            metin2::log::error("Special item group {:d} not found", group_num);
            return false;
        }

        std::vector<int> idxes;
        int n = group_value->multi_index(idxes);

        bool success;

        for (int i = 0; i < n; i++) {
            success = false;
            int idx = idxes[i];
            uint32_t vnum = group_value->get_vnum(idx);
            uint32_t count_value = group_value->count(idx);
            int rare_pct = group_value->rare_pct(idx);
            item *item_get = nullptr;
            switch (vnum) {
            case special_item_group::GOLD:
                point_change(POINT_GOLD, count_value);
                log_manager::instance().char_log(this, count_value, "TREASURE_GOLD", "");

                success = true;
                break;
            case special_item_group::EXP:
                {
                    point_change(POINT_EXP, count_value);
                    log_manager::instance().char_log(this, count_value, "TREASURE_EXP", "");

                    success = true;
                }
                break;

            case special_item_group::MOB:
                {
                    metin2::log::debug("Special item group: spawn {:d} mobs", count_value);
                    int x_value = x() + number(-500, 500);
                    int y_value = y() + number(-500, 500);

                    character *ch = character_manager::instance().spawn_mob(count_value, map_index(), x_value, y_value, 0, true, -1);
                    if (ch)
                        ch->set_aggressive();
                    success = true;
                }
                break;
            case special_item_group::SLOW:
                {
                    metin2::log::debug("Special item group: slow {:d}", -(int)count_value);
                    add_affect(AFFECT_SLOW, POINT_MOV_SPEED, -(int)count_value, AFF_SLOW, 300, 0, true);
                    success = true;
                }
                break;
            case special_item_group::DRAIN_HP:
                {
                    int drop_hp = max_hp() * count_value / 100;
                    drop_hp = std::min(drop_hp, hp() - 1);
                    metin2::log::debug("Special item group: drain hp {:d}", -drop_hp);
                    point_change(POINT_HP, -drop_hp);
                    success = true;
                }
                break;
            case special_item_group::POISON:
                {
                    attacked_by_poison(nullptr);
                    success = true;
                }
                break;

            case special_item_group::MOB_GROUP:
                {
                    int sx = x() - number(300, 500);
                    int sy = y() - number(300, 500);
                    int ex = x() + number(300, 500);
                    int ey = y() + number(300, 500);
                    character_manager::instance().spawn_group(count_value, map_index(), sx, sy, ex, ey, nullptr, true);

                    success = true;
                }
                break;
            default:
                {
                    item_get = auto_give_item(vnum, count_value, rare_pct);

                    if (item_get) {
                        success = true;
                    }
                }
                break;
            }

            if (success) {
                item_vnums.push_back(vnum);
                item_counts.push_back(count_value);
                item_gets.push_back(item_get);
                count++;

            } else {
                return false;
            }
        }
        return success;
    }

    auto character::item_process_hair(item *item, int dest_cell) -> bool {
        if (item->check_item_use_level(level()) == false) {
            chat_packet(CHAT_TYPE_INFO, lc_text("아직 이 머리를 사용할 수 없는 레벨입니다."));
            return false;
        }

        uint32_t hair = item->vnum();

        switch (job()) {
        case JOB_WARRIOR:
            hair -= 72000;
            break;

        case JOB_ASSASSIN:
            hair -= 71250;
            break;

        case JOB_SURA:
            hair -= 70500;
            break;

        case JOB_SHAMAN:
            hair -= 69750;
            break;

        default:
            return false;
            break;
        }

        if (hair == part(PART_HAIR)) {
            chat_packet(CHAT_TYPE_INFO, lc_text("동일한 머리 스타일로는 교체할 수 없습니다."));
            return true;
        }

        item->set_count(item->count() - 1);

        set_part(PART_HAIR, hair);
        update_packet();

        return true;
    }

    auto character::item_process_polymorph(item *item) -> bool {
        if (is_polymorphed()) {
            chat_packet(CHAT_TYPE_INFO, lc_text("이미 둔갑중인 상태입니다."));
            return false;
        }

        if (true == is_riding()) {
            chat_packet(CHAT_TYPE_INFO, lc_text("둔갑할 수 없는 상태입니다."));
            return false;
        }

        uint32_t vnum_value = item->get_socket(0);

        if (vnum_value == 0) {
            chat_packet(CHAT_TYPE_INFO, lc_text("잘못된 둔갑 아이템입니다."));
            item->set_count(item->count() - 1);
            return false;
        }

        const mob *mob = mob_manager::instance().get(vnum_value);

        if (mob == nullptr) {
            chat_packet(CHAT_TYPE_INFO, lc_text("잘못된 둔갑 아이템입니다."));
            item->set_count(item->count() - 1);
            return false;
        }

        switch (item->vnum()) {
        case 70104:
        case 70105:
        case 70106:
        case 70107:
        case 71093:
            {
                metin2::log::info("Pid {:d} uses polymorph ball (vnum {:d})", player_id(), vnum_value);

                int polymorph_level_limit = std::max(0, 20 - level() * 3 / 10);
                if (mob->table.level >= level() + polymorph_level_limit) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("나보다 너무 높은 레벨의 몬스터로는 변신 할 수 없습니다."));
                    return false;
                }

                int duration = skill_level(POLYMORPH_SKILL_ID) == 0 ? 5 : (5 + (5 + skill_level(POLYMORPH_SKILL_ID) / 40 * 25));
                duration *= 60;

                uint32_t bonus = 0;

                bonus = (2 + skill_level(POLYMORPH_SKILL_ID) / 40) * 100;

                add_affect(AFFECT_POLYMORPH, POINT_POLYMORPH, vnum_value, AFF_POLYMORPH, duration, 0, true);
                add_affect(AFFECT_POLYMORPH, POINT_ATT_BONUS, bonus, AFF_POLYMORPH, duration, 0, false);

                item->set_count(item->count() - 1);
            }
            break;

        case 50322:
            {

                metin2::log::info("{} (pid {:d}) uses polymorph book (vnum {:d})", name(), player_id(), vnum_value);

                if (polymorph_utils::instance().polymorph_character(this, item, mob) == true) {
                    polymorph_utils::instance().update_book_practice_grade(this, item);
                }
            }
            break;

        default:
            metin2::log::error("Invalid polymorph item vnum {:d} used by pid {:d}", item->original_vnum(), player_id());
            return false;
        }

        return true;
    }

    auto character::can_do_cube() const -> bool {
        if (is_observer_) return false;
        if (get_shop()) return false;
        if (my_shop()) return false;
        if (under_refine_) return false;
        if (is_warping()) return false;

        return true;
    }

    auto character::un_equip_special_ride_unique_item() -> bool {
        item *unique1 = wear(WEAR_UNIQUE1);
        item *unique2 = wear(WEAR_UNIQUE2);

        if (nullptr != unique1) {
            if (UNIQUE_GROUP_SPECIAL_RIDE == unique1->special_group()) {
                return unequip_item(unique1);
            }
        }

        if (nullptr != unique2) {
            if (UNIQUE_GROUP_SPECIAL_RIDE == unique2->special_group()) {
                return unequip_item(unique2);
            }
        }

        return true;
    }

    void character::auto_recovery_item_process(const affect_types type) {
        if (true == is_dead() || true == is_stun())
            return;

        if (false == is_pc())
            return;

        if (AFFECT_AUTO_HP_RECOVERY != type && AFFECT_AUTO_SP_RECOVERY != type)
            return;

        if (nullptr != find_affect(AFFECT_STUN))
            return;

        {
            const uint32_t stun_skills[] = {SKILL_TANHWAN, SKILL_GEOMPUNG, SKILL_BYEURAK, SKILL_GIGUNG};

            for (size_t i = 0; i < sizeof(stun_skills) / sizeof(uint32_t); ++i) {
                const affect *p = find_affect(stun_skills[i]);

                if (nullptr != p && AFF_STUN == p->flag)
                    return;
            }
        }

        const affect *affect_value = find_affect(type);
        const size_t idx_of_amount_of_used = 1;
        const size_t idx_of_amount_of_full = 2;

        if (nullptr != affect_value) {
            item *item = find_item_by_id(affect_value->flag);

            if (nullptr != item && true == item->get_socket(0)) {
                if (false == arena_manager::instance().is_arena_map(map_index())) {
                    const int32_t amount_of_used = item->get_socket(idx_of_amount_of_used);
                    const int32_t amount_of_full = item->get_socket(idx_of_amount_of_full);

                    const int32_t avail = amount_of_full - amount_of_used;

                    int32_t amount = 0;

                    if (AFFECT_AUTO_HP_RECOVERY == type) {
                        amount = max_hp() - (hp() + point(POINT_HP_RECOVERY));
                    } else if (AFFECT_AUTO_SP_RECOVERY == type) {
                        amount = max_sp() - (sp() + point(POINT_SP_RECOVERY));
                    }

                    if (amount > 0) {
                        if (avail > amount) {
                            const int pct_of_used = amount_of_used * 100 / amount_of_full;
                            const int pct_of_will_used = (amount_of_used + amount) * 100 / amount_of_full;

                            bool log = false;
                            if ((pct_of_will_used / 10) - (pct_of_used / 10) >= 1)
                                log = true;
                            item->set_socket(idx_of_amount_of_used, amount_of_used + amount, log);
                        } else {
                            amount = avail;

                            item_manager::instance().remove_item(item);
                        }

                        if (AFFECT_AUTO_HP_RECOVERY == type) {
                            point_change(POINT_HP_RECOVERY, amount);
                            effect_packet(SE_AUTO_HPUP);
                        } else if (AFFECT_AUTO_SP_RECOVERY == type) {
                            point_change(POINT_SP_RECOVERY, amount);
                            effect_packet(SE_AUTO_SPUP);
                        }
                    }
                } else {
                    item->lock(false);
                    item->set_socket(0, false);
                    remove_affect(const_cast<affect *>(affect_value));
                }
            } else {
                remove_affect(const_cast<affect *>(affect_value));
            }
        }
    }

    auto character::is_valid_item_position(item_pos pos) const -> bool {
        uint8_t window_type = pos.window_type;
        uint16_t cell = pos.cell;

        switch (window_type) {
        case RESERVED_WINDOW:
            return false;

        case INVENTORY:
        case EQUIPMENT:
            return cell < (INVENTORY_AND_EQUIP_SLOT_MAX);

        case DRAGON_SOUL_INVENTORY:
            return cell < (DRAGON_SOUL_INVENTORY_MAX_NUM);

        case SAFEBOX:
            if (nullptr != safebox_)
                return safebox_->is_valid_position(cell);
            else
                return false;

        case MALL:
            if (nullptr != mall_)
                return mall_->is_valid_position(cell);
            else
                return false;
        default:
            return false;
        }
    }

#define VERIFY_MSG(exp, msg)                       \
    if (true == (exp)) {                           \
        chat_packet(CHAT_TYPE_INFO, lc_text(msg)); \
        return false;                              \
    }

    auto character::can_equip_now(item *const item, const item_pos &src_cell, const item_pos &dest_cell) -> bool {
        const item_table *item_table = item->proto();

        switch (job()) {
        case JOB_WARRIOR:
            if (item->anti_flag() & ITEM_ANTIFLAG_WARRIOR)
                return false;
            break;

        case JOB_ASSASSIN:
            if (item->anti_flag() & ITEM_ANTIFLAG_ASSASSIN)
                return false;
            break;

        case JOB_SHAMAN:
            if (item->anti_flag() & ITEM_ANTIFLAG_SHAMAN)
                return false;
            break;

        case JOB_SURA:
            if (item->anti_flag() & ITEM_ANTIFLAG_SURA)
                return false;
            break;
        }

        for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i) {
            int32_t limit = item_table->limits[i].value;
            switch (item_table->limits[i].type) {
            case LIMIT_LEVEL:
                if (level() < limit) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("레벨이 낮아 착용할 수 없습니다."));
                    return false;
                }
                break;

            case LIMIT_STR:
                if (point(POINT_ST) < limit) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("근력이 낮아 착용할 수 없습니다."));
                    return false;
                }
                break;

            case LIMIT_INT:
                if (point(POINT_IQ) < limit) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("지능이 낮아 착용할 수 없습니다."));
                    return false;
                }
                break;

            case LIMIT_DEX:
                if (point(POINT_DX) < limit) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("민첩이 낮아 착용할 수 없습니다."));
                    return false;
                }
                break;

            case LIMIT_CON:
                if (point(POINT_HT) < limit) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("체력이 낮아 착용할 수 없습니다."));
                    return false;
                }
                break;
            }
        }

        if (item->wear_flag() & WEARABLE_UNIQUE) {
            if ((wear(WEAR_UNIQUE1) && wear(WEAR_UNIQUE1)->is_same_special_group(item)) ||
                (wear(WEAR_UNIQUE2) && wear(WEAR_UNIQUE2)->is_same_special_group(item))) {
                chat_packet(CHAT_TYPE_INFO, lc_text("같은 종류의 유니크 아이템 두 개를 동시에 장착할 수 없습니다."));
                return false;
            }

            if (marriage::manager::instance().is_marriage_unique_item(item->vnum()) &&
                !marriage::manager::instance().is_married(player_id())) {
                chat_packet(CHAT_TYPE_INFO, lc_text("결혼하지 않은 상태에서 예물을 착용할 수 없습니다."));
                return false;
            }
        }

        return true;
    }

    auto character::can_unequip_now(item *const item, const item_pos &src_cell, const item_pos &dest_cell) -> bool {

        if (ITEM_BELT == item->type())
            VERIFY_MSG(belt_inventory_helper::is_exist_item_in_belt_inventory(this), "벨트 인벤토리에 아이템이 존재하면 해제할 수 없습니다.");

        if (is_set(item->flag(), ITEM_FLAG_IRREMOVABLE))
            return false;

        {
            int pos = -1;

            if (item->is_dragon_soul())
                pos = empty_dragon_soul_inventory(item);
            else
                pos = empty_inventory(item->size());

            VERIFY_MSG(-1 == pos, "소지품에 빈 공간이 없습니다.");
        }

        return true;
    }
}
