#include <metin2/game/items/dragon_soul.h>

#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/items/dragon_soul_table.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/text/locale.hpp>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/apply_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/limits.h>

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <format>
#include <list>
#include <memory>
#include <random>
#include <ranges>
#include <set>
#include <string>
#include <vector>

namespace metin2::game::inline items {

    using token_vector = std::vector<std::string>;
    auto gamble(std::vector<float> &probs) -> int {
        if (std::ranges::none_of(probs, [](float p) -> bool { return p > 0.f; }))
            return -1;
        return std::discrete_distribution<int>(probs.begin(), probs.end())(random_engine());
    }

    auto make_distinct_random_number_set(std::list<float> prob_lst, std::vector<int> &random_set) -> bool {
        int size = prob_lst.size();
        int n = random_set.size();
        if (size < n)
            return false;

        std::vector<int> select_bit(size, 0);
        for (int i = 0; i < n; i++) {
            float range = 0.f;
            for (auto it = prob_lst.begin(); it != prob_lst.end(); it++) {
                range += *it;
            }
            float r = fnumber(0.f, range);
            float sum = 0.f;
            int idx = 0;
            for (auto it = prob_lst.begin(); it != prob_lst.end(); it++) {
                while (select_bit[idx++])
                    ;

                sum += *it;
                if (sum >= r) {
                    select_bit[idx - 1] = 1;
                    random_set[i] = idx - 1;
                    prob_lst.erase(it);
                    break;
                }
            }
        }
        return true;
    }

    auto get_type(uint32_t vnum) -> uint8_t {
        return (vnum / 10000);
    }

    auto get_grade_idx(uint32_t vnum) -> uint8_t {
        return (vnum / 1000) % 10;
    }

    auto get_step_idx(uint32_t vnum) -> uint8_t {
        return (vnum / 100) % 10;
    }

    auto get_strength_idx(uint32_t vnum) -> uint8_t {
        return (vnum / 10) % 10;
    }

    auto ds_manager::read_dragon_soul_table_file(const char *file_name) -> bool {
        table_ = std::make_unique<dragon_soul_table>();
        return table_->read_dragon_soul_table_file(file_name);
    }

    void ds_manager::dragon_soul_info(uint32_t vnum, uint8_t &type, uint8_t &grade, uint8_t &step, uint8_t &strength) const {
        type = get_type(vnum);
        grade = get_grade_idx(vnum);
        step = get_step_idx(vnum);
        strength = get_strength_idx(vnum);
    }

    auto ds_manager::is_valid_cell_for_this_item(item *const item_value, const item_pos &cell_value) const -> bool {
        if (nullptr == item_value)
            return false;

        uint16_t base_cell = base_position(item_value);
        if (WORD_MAX == base_cell)
            return false;

        if (cell_value.window_type != DRAGON_SOUL_INVENTORY || (cell_value.cell < base_cell || cell_value.cell >= base_cell + DRAGON_SOUL_BOX_SIZE)) {
            return false;
        } else
            return true;
    }

    auto ds_manager::base_position(item *const item_value) const -> uint16_t {
        if (nullptr == item_value)
            return WORD_MAX;

        uint8_t type, grade_idx, step_idx, strength_idx;
        dragon_soul_info(item_value->vnum(), type, grade_idx, step_idx, strength_idx);

        uint8_t col_type = item_value->sub_type();
        uint8_t row_type = grade_idx;
        if (row_type > DRAGON_SOUL_GRADE_MAX)
            return WORD_MAX;

        return col_type * DRAGON_SOUL_STEP_MAX * DRAGON_SOUL_BOX_SIZE + row_type * DRAGON_SOUL_BOX_SIZE;
    }

    auto ds_manager::refresh_item_attributes(item *ds) -> bool {
        if (!ds->is_dragon_soul()) {
            metin2::log::error("Item (id {:d}) is not a dragon soul", ds->id());
            return false;
        }

        uint8_t ds_type, grade_idx, step_idx, strength_idx;
        dragon_soul_info(ds->vnum(), ds_type, grade_idx, step_idx, strength_idx);

        dragon_soul_table::vec_applys basic_apply_list;
        dragon_soul_table::vec_applys additional_apply_list;

        if (!table_->basic_applys(ds_type, basic_apply_list)) {
            metin2::log::error("No basic applies for dragon soul type {:d}", ds_type);
            return false;
        }

        if (!table_->additional_applys(ds_type, additional_apply_list)) {
            metin2::log::error("No additional applies for dragon soul type {:d}", ds_type);
            return false;
        }

        int basic_apply_num, add_min, add_max;
        if (!table_->apply_num_settings(ds_type, grade_idx, basic_apply_num, add_min, add_max)) {
            metin2::log::error("No apply count settings for dragon soul type {:d}, grade {:d}", ds_type, grade_idx);
            return false;
        }

        float weight = 0.f;
        if (!table_->weight(ds_type, grade_idx, step_idx, strength_idx, weight)) {
            return false;
        }
        weight /= 100.f;

        int n = std::min<int>(basic_apply_num, basic_apply_list.size());
        for (int i = 0; i < n; i++) {
            const apply &basic_apply = basic_apply_list[i];
            uint8_t type = basic_apply.apply_type;
            auto s_value = (short)(ceil((float)basic_apply.apply_value * weight - 0.01f));

            ds->set_force_attribute(i, type, s_value);
        }

        for (int i = DRAGON_SOUL_ADDITIONAL_ATTR_START_IDX; i < ITEM_ATTRIBUTE_MAX_NUM; i++) {
            uint8_t type = ds->attribute_type(i);
            short s_value = 0;
            if (APPLY_NONE == type)
                continue;
            for (size_t j = 0; j < additional_apply_list.size(); j++) {
                if (additional_apply_list[j].apply_type == type) {
                    s_value = additional_apply_list[j].apply_value;
                    break;
                }
            }
            ds->set_force_attribute(i, type, (short)(ceil((float)s_value * weight - 0.01f)));
        }
        return true;
    }

    auto ds_manager::put_attributes(item *ds) -> bool {
        if (!ds->is_dragon_soul()) {
            metin2::log::error("Item (id {:d}) is not a dragon soul", ds->id());
            return false;
        }

        uint8_t ds_type, grade_idx, step_idx, strength_idx;
        dragon_soul_info(ds->vnum(), ds_type, grade_idx, step_idx, strength_idx);

        dragon_soul_table::vec_applys basic_apply_list;
        dragon_soul_table::vec_applys additional_apply_list;

        if (!table_->basic_applys(ds_type, basic_apply_list)) {
            metin2::log::error("No basic applies for dragon soul type {:d}", ds_type);
            return false;
        }
        if (!table_->additional_applys(ds_type, additional_apply_list)) {
            metin2::log::error("No additional applies for dragon soul type {:d}", ds_type);
            return false;
        }

        int basic_apply_num, add_min, add_max;
        if (!table_->apply_num_settings(ds_type, grade_idx, basic_apply_num, add_min, add_max)) {
            metin2::log::error("No apply count settings for dragon soul type {:d}, grade {:d}", ds_type, grade_idx);
            return false;
        }

        float weight = 0.f;
        if (!table_->weight(ds_type, grade_idx, step_idx, strength_idx, weight)) {
            return false;
        }
        weight /= 100.f;

        int n = std::min<int>(basic_apply_num, basic_apply_list.size());
        for (int i = 0; i < n; i++) {
            const apply &basic_apply = basic_apply_list[i];
            uint8_t type = basic_apply.apply_type;
            auto s_value = (short)(ceil((float)basic_apply.apply_value * weight - 0.01f));

            ds->set_force_attribute(i, type, s_value);
        }

        uint8_t additional_attr_num = std::min(number(add_min, add_max), 3);

        std::vector<int> random_set;
        if (additional_attr_num > 0) {
            random_set.resize(additional_attr_num);
            std::list<float> probs;
            for (size_t i = 0; i < additional_apply_list.size(); i++) {
                probs.push_back(additional_apply_list[i].prob);
            }
            if (!make_distinct_random_number_set(probs, random_set)) {
                metin2::log::error("Cannot pick distinct additional applies for dragon soul (id {:d})", ds->id());
                return false;
            }

            for (int i = 0; i < additional_attr_num; i++) {
                int r = random_set[i];
                const apply &additional_attr = additional_apply_list[r];
                uint8_t type = additional_attr.apply_type;
                auto s_value = (short)(ceil((float)additional_attr.apply_value * weight - 0.01f));

                ds->set_force_attribute(DRAGON_SOUL_ADDITIONAL_ATTR_START_IDX + i, type, s_value);
            }
        }

        return true;
    }

    auto ds_manager::dragon_soul_item_initialize(item *item_value) -> bool {
        if (nullptr == item_value || !item_value->is_dragon_soul())
            return false;
        put_attributes(item_value);
        int time = get_ds_manager().duration(item_value);
        if (time > 0)
            item_value->set_socket(ITEM_SOCKET_REMAIN_SEC, time);
        return true;
    }

    auto ds_manager::make_dragon_soul_vnum(uint8_t type, uint8_t grade, uint8_t step, uint8_t refine) -> uint32_t {
        return type * 10000 + grade * 1000 + step * 100 + refine * 10;
    }

    auto ds_manager::duration(item *const item_value) const -> int {
        return item_value->duration();
    }

    auto ds_manager::extract_dragon_heart(character *ch, item *item_value, item *extractor) -> bool {
        if (nullptr == ch || nullptr == item_value)
            return false;
        if (item_value->is_equipped()) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("착용 중인 용혼석은 추출할 수 없습니다."));
            return false;
        }

        if (item_value->is_exchanging())
            return false;

        uint32_t vnum_value = item_value->vnum();
        uint8_t ds_type, grade_idx, step_idx, strength_idx;
        dragon_soul_info(vnum_value, ds_type, grade_idx, step_idx, strength_idx);

        int bonus = 0;

        if (nullptr != extractor) {
            bonus = extractor->value(0);
        }

        std::vector<float> chargings;
        std::vector<float> probs;

        if (!table_->dragon_heart_ext_values(ds_type, grade_idx, chargings, probs)) {
            return false;
        }

        int idx = gamble(probs);

        if (-1 == idx) {
            metin2::log::error("Dragon heart extraction roll failed for type {:d}, grade {:d}", ds_type, grade_idx);
            return false;
        }

        float charge = chargings[idx] * (100 + bonus) / 100.f;
        charge = std::clamp(charge, 0.f, 100.f);

        if (charge < FLT_EPSILON) {
            get_log_manager().item_log(ch, item_value, "DS_HEART_EXTRACT_FAIL", "");
            item_value->set_count(item_value->count() - 1);
            if (nullptr != extractor) {
                extractor->set_count(extractor->count() - 1);
            }

            ch->chat_packet(CHAT_TYPE_INFO, lc_text("용심 추출에 실패하였습니다."));
            return false;
        } else {
            item *dh = get_item_manager().create_item(DRAGON_HEART_VNUM);

            if (nullptr == dh) {
                metin2::log::error("Cannot create dragon heart (vnum {:d})", DRAGON_HEART_VNUM);
                return false;
            }

            int charge_value = (int)(charge + 0.5f);

            std::string s = std::to_string(charge_value);
            s += "%s";
            get_log_manager().item_log(ch, item_value, "DS_HEART_EXTRACT_SUCCESS", s.c_str());

            item_value->set_count(item_value->count() - 1);
            if (nullptr != extractor) {
                extractor->set_count(extractor->count() - 1);
            }

            dh->set_socket(ITEM_SOCKET_CHARGING_AMOUNT_IDX, charge_value);
            ch->auto_give_item(dh, true);
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("용심 추출에 성공하였습니다."));
            return true;
        }
    }

    auto ds_manager::pull_out(character *ch, item_pos dest_cell, item *&item_value, item *extractor) -> bool {
        if (nullptr == ch || nullptr == item_value) {
            metin2::log::error("Missing character ({}) or item ({})", static_cast<const void *>(ch), static_cast<const void *>(item_value));
            return false;
        }

        if (!is_valid_cell_for_this_item(item_value, dest_cell)) {
            int empty_cell = ch->empty_dragon_soul_inventory(item_value);
            if (empty_cell < 0) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("소지품에 빈 공간이 없습니다."));
                return false;
            } else {
                dest_cell.window_type = DRAGON_SOUL_INVENTORY;
                dest_cell.cell = empty_cell;
            }
        }

        if (!item_value->is_equipped() || !item_value->remove_from_character())
            return false;

        bool success;
        uint32_t product = 0;
        int bonus = 0;
        float prob_value;
        float dice_value;
        {

            uint8_t ds_type, grade_idx, step_idx, strength_idx;
            dragon_soul_info(item_value->vnum(), ds_type, grade_idx, step_idx, strength_idx);

            if (!table_->dragon_soul_ext_values(ds_type, grade_idx, prob_value, product)) {
                item_value->add_to_character(ch, dest_cell);
                return true;
            }

            if (nullptr != extractor) {
                bonus = extractor->value(ITEM_VALUE_DRAGON_SOUL_POLL_OUT_BONUS_IDX);
                extractor->set_count(extractor->count() - 1);
            }
            dice_value = fnumber(0.f, 100.f);
            success = dice_value <= (prob_value * (100 + bonus) / 100.f);
        }

        {
            std::string buf;

            if (success) {
                if (extractor) {
                    buf = std::format("dice({}) prob({} + {}) EXTR(VN:{})", (int)dice_value, (int)prob_value, bonus, extractor->vnum());
                } else {
                    buf = std::format("dice({:g}) prob({:g})", dice_value, prob_value);
                }
                get_log_manager().item_log(ch, item_value, "DS_PULL_OUT_SUCCESS", buf.c_str());
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("용혼석 추출에 성공하였습니다."));
                item_value->add_to_character(ch, dest_cell);
                return true;
            } else {
                if (extractor) {
                    buf = std::format("dice({}) prob({} + {}) EXTR(VN:{}) ByProd(VN:{})", (int)dice_value, (int)prob_value, bonus, extractor->vnum(), product);
                } else {
                    buf = std::format("dice({}) prob({}) ByProd(VNUM:{})", (int)dice_value, (int)prob_value, product);
                }
                get_log_manager().item_log(ch, item_value, "DS_PULL_OUT_FAILED", buf.c_str());
                get_item_manager().destroy_item(item_value);
                item_value = nullptr;
                if (product) {
                    item *by_product_value = ch->auto_give_item(product, true);
                    if (by_product_value)
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("용혼석 추출에 실패하여 %s를 얻었습니다."), by_product_value->name());
                    else
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("용혼석 추출에 실패하였습니다."));
                } else
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("용혼석 추출에 실패하였습니다."));
            }
        }

        return success;
    }

    namespace {
        auto can_use_refine_window(character *ch, const char *error_message) -> bool {
            if (nullptr == ch)
                return false;

            if (!ch->dragon_soul_refine_window_can_refine()) {
                metin2::log::warn("Player {} sent a dragon soul refine without the refine window open", ch->name());
                ch->chat_packet(CHAT_TYPE_INFO, error_message);
                return false;
            }
            return true;
        }
    }

    auto ds_manager::collect_refine_items(character *ch, item_pos (&item_poses)[DRAGON_SOUL_REFINE_GRID_SIZE], std::set<item *> &items, bool reject_equip_position, const char *non_dragon_soul_message) -> bool {
        for (const item_pos &pos : item_poses) {
            if (reject_equip_position && pos.is_equip_position())
                return false;

            item *item = ch->get_item(pos);
            if (nullptr == item)
                continue;

            if (item->is_equipped() || item->is_exchanging())
                return false;

            if (non_dragon_soul_message && !item->is_dragon_soul()) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text(non_dragon_soul_message));
                send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(item->window(), item->cell()));
                return false;
            }

            items.insert(item);
        }
        return true;
    }

    auto ds_manager::has_refine_fee(character *ch, int fee) -> bool {
        if (ch->gold() < fee) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("개량을 하기 위한 돈이 부족합니다."));
            send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MONEY, NPOS);
            return false;
        }
        return true;
    }

    auto ds_manager::refine_grade_or_step(character *ch, item_pos (&item_poses)[DRAGON_SOUL_REFINE_GRID_SIZE], bool grade_value) -> bool {
        if (!can_use_refine_window(ch, grade_value ? "[SYSTEM ERROR]You cannot upgrade dragon soul without refine window." : "[SYSTEM ERROR]You cannot use dragon soul refine window."))
            return false;

        const char *invalid_material = grade_value ? "등급 개량에 필요한 재료가 아닙니다." : "단계 개량에 필요한 재료가 아닙니다.";

        std::set<item *> items;
        if (!collect_refine_items(ch, item_poses, items, grade_value, invalid_material))
            return false;

        if (items.empty()) {
            send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL, NPOS);
            return false;
        }

        const int count = items.size();
        int need_count = 0;
        int fee = 0;
        std::vector<float> probs;

        uint8_t ds_type, grade_idx, step_idx, strength_idx;

        item *first = *items.begin();
        dragon_soul_info(first->vnum(), ds_type, grade_idx, step_idx, strength_idx);

        const bool has_values = grade_value ? table_->refine_grade_values(ds_type, grade_idx, need_count, fee, probs)
                                            : table_->refine_step_values(ds_type, step_idx, need_count, fee, probs);
        if (!has_values) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text(grade_value ? "등급 개량할 수 없는 용혼석입니다." : "단계 개량할 수 없는 용혼석입니다."));
            send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(first->window(), first->cell()));
            return false;
        }

        for (item *item_value : items | std::views::drop(1)) {
            const uint32_t vnum = item_value->vnum();
            if (ds_type != get_type(vnum) || grade_idx != get_grade_idx(vnum) || (!grade_value && step_idx != get_step_idx(vnum))) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text(invalid_material));
                send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(item_value->window(), item_value->cell()));
                return false;
            }
        }

        if (count != need_count) {
            metin2::log::warn("Player {} sent a dragon soul refine with the wrong material count (possible modified client)", ch->name());
            uint8_t sub_header = count < need_count ? DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL : DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL;
            send_refine_result_packet(ch, sub_header, NPOS);
            return false;
        }

        if (!has_refine_fee(ch, fee))
            return false;

        const int result = gamble(probs);
        if (-1 == result) {
            metin2::log::error("Dragon soul {} refine roll failed; check {} probabilities", grade_value ? "grade" : "step", grade_value ? "RefineGradeTables" : "RefineStepTables");
            return false;
        }

        const uint32_t result_vnum = grade_value ? make_dragon_soul_vnum(ds_type, (uint8_t)result, 0, 0) : make_dragon_soul_vnum(ds_type, grade_idx, (uint8_t)result, 0);
        item *result_item = get_item_manager().create_item(result_vnum);

        if (nullptr == result_item) {
            metin2::log::error("Cannot create dragon soul (vnum {:d})", result_vnum);
            return false;
        }

        ch->point_change(POINT_GOLD, -fee);
        int left_count = need_count;

        for (item *item_value : items) {
            int n = item_value->count();
            if (left_count > n) {
                item_value->remove_from_character();
                get_item_manager().destroy_item(item_value);
                left_count -= n;
            } else {
                item_value->set_count(n - left_count);
            }
        }

        ch->auto_give_item(result_item, true);

        const int previous = grade_value ? grade_idx : step_idx;
        const bool succeeded = result > previous;
        const char *kind = grade_value ? "GRADE" : "STEP";
        const std::string buf = std::format("{} : {} -> {}", kind, previous, result);
        get_log_manager().item_log(ch, result_item, std::format("DS_{}_REFINE_{}", kind, succeeded ? "SUCCESS" : "FAIL").c_str(), buf.c_str());

        if (grade_value)
            ch->chat_packet(CHAT_TYPE_INFO, lc_text(succeeded ? "등급 개량에 성공했습니다." : "등급 개량에 실패했습니다."));
        else
            ch->chat_packet(CHAT_TYPE_INFO, lc_text(succeeded ? "단계 개량에 성공했습니다." : "단계 개량에 실패했습니다."));

        send_refine_result_packet(ch, succeeded ? DS_SUB_HEADER_REFINE_SUCCEED : DS_SUB_HEADER_REFINE_FAIL, item_pos(result_item->window(), result_item->cell()));
        return succeeded;
    }

    auto ds_manager::do_refine_grade(character *ch, item_pos (&item_poses)[DRAGON_SOUL_REFINE_GRID_SIZE]) -> bool {
        return refine_grade_or_step(ch, item_poses, true);
    }

    auto ds_manager::do_refine_step(character *ch, item_pos (&item_poses)[DRAGON_SOUL_REFINE_GRID_SIZE]) -> bool {
        return refine_grade_or_step(ch, item_poses, false);
    }

    auto is_dragon_soul_refine_material(item *item_value) -> bool {
        if (item_value->type() != ITEM_MATERIAL)
            return false;
        return (item_value->sub_type() == MATERIAL_DS_REFINE_NORMAL ||
                item_value->sub_type() == MATERIAL_DS_REFINE_BLESSED ||
                item_value->sub_type() == MATERIAL_DS_REFINE_HOLLY);
    }

    auto ds_manager::do_refine_strength(character *ch, item_pos (&item_poses)[DRAGON_SOUL_REFINE_GRID_SIZE]) -> bool {
        if (!can_use_refine_window(ch, "[SYSTEM ERROR]You cannot use dragon soul refine window."))
            return false;

        std::set<item *> items;
        if (!collect_refine_items(ch, item_poses, items, false, nullptr) || items.empty())
            return false;

        int fee;

        item *refine_stone = nullptr;
        item *dragon_soul = nullptr;
        for (item *item_value : items) {
            if (item_value->is_dragon_soul()) {
                if (dragon_soul != nullptr) {
                    send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL, item_pos(item_value->window(), item_value->cell()));
                    return false;
                }
                dragon_soul = item_value;
            } else if (is_dragon_soul_refine_material(item_value)) {
                if (refine_stone != nullptr) {
                    send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL, item_pos(item_value->window(), item_value->cell()));
                    return false;
                }
                refine_stone = item_value;
            } else {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("강화에 필요한 재료가 아닙니다."));
                send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(item_value->window(), item_value->cell()));
                return false;
            }
        }

        uint8_t type = 0, grade = 0, step = 0, strength = 0;

        if (!dragon_soul || !refine_stone) {
            send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL, NPOS);

            return false;
        }

        dragon_soul_info(dragon_soul->vnum(), type, grade, step, strength);

        float weight = 0.f;
        if (!table_->weight(type, grade, step, strength + 1, weight) || weight < FLT_EPSILON) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("강화할 수 없는 용혼석입니다."));
            send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_MAX_REFINE, item_pos(dragon_soul->window(), dragon_soul->cell()));
            return false;
        }

        float prob;
        if (!table_->refine_strength_values(type, refine_stone->sub_type(), strength, fee, prob)) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("강화할 수 없는 용혼석입니다."));
            send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(dragon_soul->window(), dragon_soul->cell()));

            return false;
        }

        if (!has_refine_fee(ch, fee))
            return false;

        ch->point_change(POINT_GOLD, -fee);
        item *result = nullptr;
        uint8_t sub_header;

        if (fnumber(0.f, 100.f) <= prob) {
            result = get_item_manager().create_item(make_dragon_soul_vnum(type, grade, step, strength + 1));
            if (nullptr == result) {
                metin2::log::error("Cannot create dragon soul (vnum {:d})", make_dragon_soul_vnum(type, grade, step, strength + 1));
                return false;
            }
            dragon_soul->remove_from_character();

            dragon_soul->copy_attribute_to(result);
            refresh_item_attributes(result);

            dragon_soul->set_count(dragon_soul->count() - 1);
            refine_stone->set_count(refine_stone->count() - 1);

            const std::string buf = std::format("STRENGTH : {} -> {}", strength, strength + 1);
            get_log_manager().item_log(ch, dragon_soul, "DS_STRENGTH_REFINE_SUCCESS", buf.c_str());
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("강화에 성공했습니다."));
            ch->auto_give_item(result, true);
            sub_header = DS_SUB_HEADER_REFINE_SUCCEED;
        } else {
            if (strength != 0) {
                result = get_item_manager().create_item(make_dragon_soul_vnum(type, grade, step, strength - 1));
                if (nullptr == result) {
                    metin2::log::error("Cannot create dragon soul (vnum {:d})", make_dragon_soul_vnum(type, grade, step, strength - 1));
                    return false;
                }
                dragon_soul->copy_attribute_to(result);
                refresh_item_attributes(result);
            }
            sub_header = DS_SUB_HEADER_REFINE_FAIL;

            const std::string buf = std::format("STRENGTH : {} -> {}", strength, strength - 1);
            get_log_manager().item_log(ch, dragon_soul, "DS_STRENGTH_REFINE_FAIL", buf.c_str());

            ch->chat_packet(CHAT_TYPE_INFO, lc_text("강화에 실패했습니다."));
            dragon_soul->set_count(dragon_soul->count() - 1);
            refine_stone->set_count(refine_stone->count() - 1);
            if (nullptr != result)
                ch->auto_give_item(result, true);
        }

        send_refine_result_packet(ch, sub_header, nullptr == result ? NPOS : item_pos(result->window(), result->cell()));

        return true;
    }

    void ds_manager::send_refine_result_packet(character *ch, uint8_t sub_header, const item_pos &pos) {
        packet_gc_dragon_soul_refine pack;
        pack.sub_type = sub_header;

        if (pos.is_valid_item_position()) {
            pack.pos = pos;
        }
        desc *d = ch->get_desc();
        if (nullptr == d) {
            return;
        } else {
            d->packet(&pack, sizeof(pack));
        }
    }

    auto ds_manager::left_time(item *item_value) const -> int {
        if (item_value == nullptr)
            return false;

        if (item_value->proto()->limit_timer_based_on_wear_index >= 0) {
            return item_value->get_socket(ITEM_SOCKET_REMAIN_SEC);
        } else {
            return INT_MAX;
        }
    }

    auto ds_manager::is_time_left_dragon_soul(item *item_value) const -> bool {
        if (item_value == nullptr)
            return false;

        if (item_value->proto()->limit_timer_based_on_wear_index >= 0) {
            return item_value->get_socket(ITEM_SOCKET_REMAIN_SEC) > 0;
        } else {
            return true;
        }
    }

    auto ds_manager::is_active_dragon_soul(item *item_value) const -> bool {
        return item_value->get_socket(ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX);
    }

    auto ds_manager::activate_dragon_soul(item *item_value) -> bool {
        if (nullptr == item_value)
            return false;
        character *owner = item_value->owner();
        if (nullptr == owner)
            return false;

        int deck_idx = owner->dragon_soul_get_active_deck();

        if (deck_idx < 0)
            return false;

        if (INVENTORY_MAX_NUM + WEAR_MAX_NUM + DS_SLOT_MAX * deck_idx <= item_value->cell() &&
            item_value->cell() < INVENTORY_MAX_NUM + WEAR_MAX_NUM + DS_SLOT_MAX * (deck_idx + 1)) {
            if (is_time_left_dragon_soul(item_value) && !is_active_dragon_soul(item_value)) {
                const std::string buf = std::format("LEFT TIME({})", left_time(item_value));
                get_log_manager().item_log(owner, item_value, "DS_ACTIVATE", buf.c_str());
                item_value->modify_points(true);
                item_value->set_socket(ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX, 1);

                item_value->start_timer_based_on_wear_expire_event();
            }
            return true;
        } else
            return false;
    }

    auto ds_manager::deactivate_dragon_soul(item *item_value, bool skip_refresh_owner_active_state) -> bool {
        if (nullptr == item_value)
            return false;

        character *owner = item_value->owner();
        if (nullptr == owner)
            return false;

        if (!is_active_dragon_soul(item_value))
            return false;

        item_value->stop_timer_based_on_wear_expire_event();
        item_value->set_socket(ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX, 0);
        item_value->modify_points(false);

        const std::string buf = std::format("LEFT TIME({})", left_time(item_value));
        get_log_manager().item_log(owner, item_value, "DS_DEACTIVATE", buf.c_str());

        if (false == skip_refresh_owner_active_state)
            refresh_dragon_soul_state(owner);

        return true;
    }

    void ds_manager::refresh_dragon_soul_state(character *ch) {
        if (nullptr == ch)
            return;
        for (int i = WEAR_MAX_NUM; i < WEAR_MAX_NUM + static_cast<int>(DS_SLOT_MAX) * DRAGON_SOUL_DECK_MAX_NUM; i++) {
            item *item = ch->wear(i);
            if (item != nullptr) {
                if (is_active_dragon_soul(item)) {
                    return;
                }
            }
        }
        ch->dragon_soul_deactivate_all();
    }

    ds_manager::ds_manager() = default;

    ds_manager::~ds_manager() = default;
}
