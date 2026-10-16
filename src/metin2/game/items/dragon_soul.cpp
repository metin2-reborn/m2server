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
#include <metin2/proto/length.h>

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
#include <set>
#include <string>
#include <vector>

namespace metin2::game::inline items {

    typedef std::vector<std::string> token_vector;
    auto gamble(std::vector<float> &vec_probs) -> int {
        if (std::ranges::none_of(vec_probs, [](float p) { return p > 0.f; }))
            return -1;
        return std::discrete_distribution<int>(vec_probs.begin(), vec_probs.end())(random_engine());
    }

    auto make_distinct_random_number_set(std::list<float> prob_lst, std::vector<int> &random_set) -> bool {
        int size = prob_lst.size();
        int n = random_set.size();
        if (size < n)
            return false;

        std::vector<int> select_bit(size, 0);
        for (int i = 0; i < n; i++) {
            float range = 0.f;
            for (std::list<float>::iterator it = prob_lst.begin(); it != prob_lst.end(); it++) {
                range += *it;
            }
            float r = fnumber(0.f, range);
            float sum = 0.f;
            int idx = 0;
            for (std::list<float>::iterator it = prob_lst.begin(); it != prob_lst.end(); it++) {
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

        uint16_t wBaseCell = base_position(item_value);
        if (WORD_MAX == wBaseCell)
            return false;

        if (cell_value.window_type != DRAGON_SOUL_INVENTORY || (cell_value.cell < wBaseCell || cell_value.cell >= wBaseCell + DRAGON_SOUL_BOX_SIZE)) {
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

        dragon_soul_table::vec_applys vec_basic_applys;
        dragon_soul_table::vec_applys vec_addtional_applys;

        if (!table_->basic_applys(ds_type, vec_basic_applys)) {
            metin2::log::error("No basic applies for dragon soul type {:d}", ds_type);
            return false;
        }

        if (!table_->additional_applys(ds_type, vec_addtional_applys)) {
            metin2::log::error("No additional applies for dragon soul type {:d}", ds_type);
            return false;
        }

        int basic_apply_num, add_min, add_max;
        if (!table_->apply_num_settings(ds_type, grade_idx, basic_apply_num, add_min, add_max)) {
            metin2::log::error("No apply count settings for dragon soul type {:d}, grade {:d}", ds_type, grade_idx);
            return false;
        }

        float fWeight = 0.f;
        if (!table_->weight(ds_type, grade_idx, step_idx, strength_idx, fWeight)) {
            return false;
        }
        fWeight /= 100.f;

        int n = std::min<int>(basic_apply_num, vec_basic_applys.size());
        for (int i = 0; i < n; i++) {
            const apply &basic_apply = vec_basic_applys[i];
            uint8_t bType = basic_apply.apply_type;
            short sValue = (short)(ceil((float)basic_apply.apply_value * fWeight - 0.01f));

            ds->set_force_attribute(i, bType, sValue);
        }

        for (int i = DRAGON_SOUL_ADDITIONAL_ATTR_START_IDX; i < ITEM_ATTRIBUTE_MAX_NUM; i++) {
            uint8_t bType = ds->attribute_type(i);
            short sValue = 0;
            if (APPLY_NONE == bType)
                continue;
            for (size_t j = 0; j < vec_addtional_applys.size(); j++) {
                if (vec_addtional_applys[j].apply_type == bType) {
                    sValue = vec_addtional_applys[j].apply_value;
                    break;
                }
            }
            ds->set_force_attribute(i, bType, (short)(ceil((float)sValue * fWeight - 0.01f)));
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

        dragon_soul_table::vec_applys vec_basic_applys;
        dragon_soul_table::vec_applys vec_addtional_applys;

        if (!table_->basic_applys(ds_type, vec_basic_applys)) {
            metin2::log::error("No basic applies for dragon soul type {:d}", ds_type);
            return false;
        }
        if (!table_->additional_applys(ds_type, vec_addtional_applys)) {
            metin2::log::error("No additional applies for dragon soul type {:d}", ds_type);
            return false;
        }

        int basic_apply_num, add_min, add_max;
        if (!table_->apply_num_settings(ds_type, grade_idx, basic_apply_num, add_min, add_max)) {
            metin2::log::error("No apply count settings for dragon soul type {:d}, grade {:d}", ds_type, grade_idx);
            return false;
        }

        float fWeight = 0.f;
        if (!table_->weight(ds_type, grade_idx, step_idx, strength_idx, fWeight)) {
            return false;
        }
        fWeight /= 100.f;

        int n = std::min<int>(basic_apply_num, vec_basic_applys.size());
        for (int i = 0; i < n; i++) {
            const apply &basic_apply = vec_basic_applys[i];
            uint8_t bType = basic_apply.apply_type;
            short sValue = (short)(ceil((float)basic_apply.apply_value * fWeight - 0.01f));

            ds->set_force_attribute(i, bType, sValue);
        }

        uint8_t additional_attr_num = std::min(number(add_min, add_max), 3);

        std::vector<int> random_set;
        if (additional_attr_num > 0) {
            random_set.resize(additional_attr_num);
            std::list<float> list_probs;
            for (size_t i = 0; i < vec_addtional_applys.size(); i++) {
                list_probs.push_back(vec_addtional_applys[i].prob);
            }
            if (!make_distinct_random_number_set(list_probs, random_set)) {
                metin2::log::error("Cannot pick distinct additional applies for dragon soul (id {:d})", ds->id());
                return false;
            }

            for (int i = 0; i < additional_attr_num; i++) {
                int r = random_set[i];
                const apply &additional_attr = vec_addtional_applys[r];
                uint8_t bType = additional_attr.apply_type;
                short sValue = (short)(ceil((float)additional_attr.apply_value * fWeight - 0.01f));

                ds->set_force_attribute(DRAGON_SOUL_ADDITIONAL_ATTR_START_IDX + i, bType, sValue);
            }
        }

        return true;
    }

    auto ds_manager::dragon_soul_item_initialize(item *item_value) -> bool {
        if (nullptr == item_value || !item_value->is_dragon_soul())
            return false;
        put_attributes(item_value);
        int time = ds_manager::instance().duration(item_value);
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

        uint32_t dwVnum = item_value->vnum();
        uint8_t ds_type, grade_idx, step_idx, strength_idx;
        dragon_soul_info(dwVnum, ds_type, grade_idx, step_idx, strength_idx);

        int iBonus = 0;

        if (nullptr != extractor) {
            iBonus = extractor->value(0);
        }

        std::vector<float> vec_chargings;
        std::vector<float> vec_probs;

        if (!table_->dragon_heart_ext_values(ds_type, grade_idx, vec_chargings, vec_probs)) {
            return false;
        }

        int idx = gamble(vec_probs);

        if (-1 == idx) {
            metin2::log::error("Dragon heart extraction roll failed for type {:d}, grade {:d}", ds_type, grade_idx);
            return false;
        }

        float fCharge = vec_chargings[idx] * (100 + iBonus) / 100.f;
        fCharge = std::clamp(fCharge, 0.f, 100.f);

        if (fCharge < FLT_EPSILON) {
            log_manager::instance().item_log(ch, item_value, "DS_HEART_EXTRACT_FAIL", "");
            item_value->set_count(item_value->count() - 1);
            if (nullptr != extractor) {
                extractor->set_count(extractor->count() - 1);
            }

            ch->chat_packet(CHAT_TYPE_INFO, lc_text("용심 추출에 실패하였습니다."));
            return false;
        } else {
            item *pDH = item_manager::instance().create_item(DRAGON_HEART_VNUM);

            if (nullptr == pDH) {
                metin2::log::error("Cannot create dragon heart (vnum {:d})", DRAGON_HEART_VNUM);
                return false;
            }

            int iCharge = (int)(fCharge + 0.5f);

            std::string s = std::to_string(iCharge);
            s += "%s";
            log_manager::instance().item_log(ch, item_value, "DS_HEART_EXTRACT_SUCCESS", s.c_str());

            item_value->set_count(item_value->count() - 1);
            if (nullptr != extractor) {
                extractor->set_count(extractor->count() - 1);
            }

            pDH->set_socket(ITEM_SOCKET_CHARGING_AMOUNT_IDX, iCharge);
            ch->auto_give_item(pDH, true);
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
            int iEmptyCell = ch->empty_dragon_soul_inventory(item_value);
            if (iEmptyCell < 0) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("소지품에 빈 공간이 없습니다."));
                return false;
            } else {
                dest_cell.window_type = DRAGON_SOUL_INVENTORY;
                dest_cell.cell = iEmptyCell;
            }
        }

        if (!item_value->is_equipped() || !item_value->remove_from_character())
            return false;

        bool bSuccess;
        uint32_t dwByProduct = 0;
        int iBonus = 0;
        float fProb;
        float fDice;
        {

            uint8_t ds_type, grade_idx, step_idx, strength_idx;
            dragon_soul_info(item_value->vnum(), ds_type, grade_idx, step_idx, strength_idx);

            if (!table_->dragon_soul_ext_values(ds_type, grade_idx, fProb, dwByProduct)) {
                item_value->add_to_character(ch, dest_cell);
                return true;
            }

            if (nullptr != extractor) {
                iBonus = extractor->value(ITEM_VALUE_DRAGON_SOUL_POLL_OUT_BONUS_IDX);
                extractor->set_count(extractor->count() - 1);
            }
            fDice = fnumber(0.f, 100.f);
            bSuccess = fDice <= (fProb * (100 + iBonus) / 100.f);
        }

        {
            std::string buf;

            if (bSuccess) {
                if (extractor) {
                    buf = std::format("dice({}) prob({} + {}) EXTR(VN:{})", (int)fDice, (int)fProb, iBonus, extractor->vnum());
                } else {
                    buf = std::format("dice({:g}) prob({:g})", fDice, fProb);
                }
                log_manager::instance().item_log(ch, item_value, "DS_PULL_OUT_SUCCESS", buf.c_str());
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("용혼석 추출에 성공하였습니다."));
                item_value->add_to_character(ch, dest_cell);
                return true;
            } else {
                if (extractor) {
                    buf = std::format("dice({}) prob({} + {}) EXTR(VN:{}) ByProd(VN:{})", (int)fDice, (int)fProb, iBonus, extractor->vnum(), dwByProduct);
                } else {
                    buf = std::format("dice({}) prob({}) ByProd(VNUM:{})", (int)fDice, (int)fProb, dwByProduct);
                }
                log_manager::instance().item_log(ch, item_value, "DS_PULL_OUT_FAILED", buf.c_str());
                item_manager::instance().destroy_item(item_value);
                item_value = nullptr;
                if (dwByProduct) {
                    item *pByProduct = ch->auto_give_item(dwByProduct, true);
                    if (pByProduct)
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("용혼석 추출에 실패하여 %s를 얻었습니다."), pByProduct->name());
                    else
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("용혼석 추출에 실패하였습니다."));
                } else
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("용혼석 추출에 실패하였습니다."));
            }
        }

        return bSuccess;
    }

    auto ds_manager::do_refine_grade(character *ch, item_pos (&item_poses)[DRAGON_SOUL_REFINE_GRID_SIZE]) -> bool {
        if (nullptr == ch)
            return false;

        if (!ch->dragon_soul_refine_window_can_refine()) {
            metin2::log::warn("Player {} sent a dragon soul refine without the refine window open", ch->name());
            ch->chat_packet(CHAT_TYPE_INFO, "[SYSTEM ERROR]You cannot upgrade dragon soul without refine window.");
            return false;
        }

        std::set<item *> set_items;
        for (int i = 0; i < DRAGON_SOUL_REFINE_GRID_SIZE; i++) {
            if (item_poses[i].is_equip_position())
                return false;
            item *pItem = ch->get_item(item_poses[i]);
            if (nullptr != pItem) {
                if (pItem->is_equipped() || pItem->is_exchanging())
                    return false;

                if (!pItem->is_dragon_soul()) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("등급 개량에 필요한 재료가 아닙니다."));
                    send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(pItem->window(), pItem->cell()));

                    return false;
                }

                set_items.insert(pItem);
            }
        }

        if (set_items.size() == 0) {
            send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL, NPOS);
            return false;
        }

        int count = set_items.size();
        int need_count = 0;
        int fee = 0;
        std::vector<float> vec_probs;

        uint8_t ds_type, grade_idx, step_idx, strength_idx;
        int result_grade;

        std::set<item *>::iterator it = set_items.begin();
        {
            item *pItem = *it;

            dragon_soul_info(pItem->vnum(), ds_type, grade_idx, step_idx, strength_idx);

            if (!table_->refine_grade_values(ds_type, grade_idx, need_count, fee, vec_probs)) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("등급 개량할 수 없는 용혼석입니다."));
                send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(pItem->window(), pItem->cell()));

                return false;
            }
        }
        while (++it != set_items.end()) {
            item *pItem = *it;

            if (pItem->is_equipped()) {
                return false;
            }

            if (ds_type != get_type(pItem->vnum()) || grade_idx != get_grade_idx(pItem->vnum())) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("등급 개량에 필요한 재료가 아닙니다."));
                send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(pItem->window(), pItem->cell()));

                return false;
            }
        }

        if (count != need_count) {
            metin2::log::warn("Player {} sent a dragon soul refine with the wrong material count (possible modified client)", ch->name());
            uint8_t bSubHeader = count < need_count ? DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL : DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL;
            send_refine_result_packet(ch, bSubHeader, NPOS);
            return false;
        }

        if (ch->gold() < fee) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("개량을 하기 위한 돈이 부족합니다."));
            send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MONEY, NPOS);
            return false;
        }

        if (-1 == (result_grade = gamble(vec_probs))) {
            metin2::log::error("Dragon soul grade refine roll failed; check RefineGradeTables probabilities");
            return false;
        }

        item *pResultItem = item_manager::instance().create_item(make_dragon_soul_vnum(ds_type, (uint8_t)result_grade, 0, 0));

        if (nullptr == pResultItem) {
            metin2::log::error("Cannot create dragon soul (vnum {:d})", make_dragon_soul_vnum(ds_type, (uint8_t)result_grade, 0, 0));
            return false;
        }

        ch->point_change(POINT_GOLD, -fee);
        int left_count = need_count;

        for (std::set<item *>::iterator it = set_items.begin(); it != set_items.end(); it++) {
            item *pItem = *it;
            int n = pItem->count();
            if (left_count > n) {
                pItem->remove_from_character();
                item_manager::instance().destroy_item(pItem);
                left_count -= n;
            } else {
                pItem->set_count(n - left_count);
            }
        }

        ch->auto_give_item(pResultItem, true);

        if (result_grade > grade_idx) {
            const std::string buf = std::format("GRADE : {} -> {}", grade_idx, result_grade);
            log_manager::instance().item_log(ch, pResultItem, "DS_GRADE_REFINE_SUCCESS", buf.c_str());
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("등급 개량에 성공했습니다."));
            send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_SUCCEED, item_pos(pResultItem->window(), pResultItem->cell()));
            return true;
        } else {
            const std::string buf = std::format("GRADE : {} -> {}", grade_idx, result_grade);
            log_manager::instance().item_log(ch, pResultItem, "DS_GRADE_REFINE_FAIL", buf.c_str());
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("등급 개량에 실패했습니다."));
            send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL, item_pos(pResultItem->window(), pResultItem->cell()));
            return false;
        }
    }

    auto ds_manager::do_refine_step(character *ch, item_pos (&item_poses)[DRAGON_SOUL_REFINE_GRID_SIZE]) -> bool {
        if (nullptr == ch)
            return false;
        if (!ch->dragon_soul_refine_window_can_refine()) {
            metin2::log::warn("Player {} sent a dragon soul refine without the refine window open", ch->name());
            ch->chat_packet(CHAT_TYPE_INFO, "[SYSTEM ERROR]You cannot use dragon soul refine window.");
            return false;
        }

        std::set<item *> set_items;
        for (int i = 0; i < DRAGON_SOUL_REFINE_GRID_SIZE; i++) {
            item *pItem = ch->get_item(item_poses[i]);
            if (nullptr != pItem) {
                if (pItem->is_equipped() || pItem->is_exchanging())
                    return false;

                if (!pItem->is_dragon_soul()) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("단계 개량에 필요한 재료가 아닙니다."));
                    send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(pItem->window(), pItem->cell()));
                    return false;
                }
                set_items.insert(pItem);
            }
        }

        if (set_items.size() == 0) {
            send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL, NPOS);
            return false;
        }

        std::string stGroupName;
        int count = set_items.size();
        int need_count = 0;
        int fee = 0;
        std::vector<float> vec_probs;

        uint8_t ds_type, grade_idx, step_idx, strength_idx;
        int result_step;

        std::set<item *>::iterator it = set_items.begin();
        {
            item *pItem = *it;
            dragon_soul_info(pItem->vnum(), ds_type, grade_idx, step_idx, strength_idx);

            if (!table_->refine_step_values(ds_type, step_idx, need_count, fee, vec_probs)) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("단계 개량할 수 없는 용혼석입니다."));
                send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(pItem->window(), pItem->cell()));
                return false;
            }
        }

        while (++it != set_items.end()) {
            item *pItem = *it;
            if (pItem->is_equipped()) {
                return false;
            }
            if (ds_type != get_type(pItem->vnum()) || grade_idx != get_grade_idx(pItem->vnum()) || step_idx != get_step_idx(pItem->vnum())) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("단계 개량에 필요한 재료가 아닙니다."));
                send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(pItem->window(), pItem->cell()));
                return false;
            }
        }

        if (count != need_count) {
            metin2::log::warn("Player {} sent a dragon soul refine with the wrong material count (possible modified client)", ch->name());
            uint8_t bSubHeader = count < need_count ? DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL : DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL;
            send_refine_result_packet(ch, bSubHeader, NPOS);
            return false;
        }

        if (ch->gold() < fee) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("개량을 하기 위한 돈이 부족합니다."));
            send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MONEY, NPOS);
            return false;
        }

        if (-1 == (result_step = gamble(vec_probs))) {
            metin2::log::error("Dragon soul step refine roll failed; check RefineStepTables probabilities");
            return false;
        }

        item *pResultItem = item_manager::instance().create_item(make_dragon_soul_vnum(ds_type, grade_idx, (uint8_t)result_step, 0));

        if (nullptr == pResultItem) {
            metin2::log::error("Cannot create dragon soul (vnum {:d})", make_dragon_soul_vnum(ds_type, grade_idx, (uint8_t)result_step, 0));
            return false;
        }

        ch->point_change(POINT_GOLD, -fee);
        int left_count = need_count;
        for (std::set<item *>::iterator it = set_items.begin(); it != set_items.end(); it++) {
            item *pItem = *it;
            int n = pItem->count();
            if (left_count > n) {
                pItem->remove_from_character();
                item_manager::instance().destroy_item(pItem);
                left_count -= n;
            } else {
                pItem->set_count(n - left_count);
            }
        }

        ch->auto_give_item(pResultItem, true);
        if (result_step > step_idx) {
            const std::string buf = std::format("STEP : {} -> {}", step_idx, result_step);
            log_manager::instance().item_log(ch, pResultItem, "DS_STEP_REFINE_SUCCESS", buf.c_str());
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("단계 개량에 성공했습니다."));
            send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_SUCCEED, item_pos(pResultItem->window(), pResultItem->cell()));
            return true;
        } else {
            const std::string buf = std::format("STEP : {} -> {}", step_idx, result_step);
            log_manager::instance().item_log(ch, pResultItem, "DS_STEP_REFINE_FAIL", buf.c_str());
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("단계 개량에 실패했습니다."));
            send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL, item_pos(pResultItem->window(), pResultItem->cell()));
            return false;
        }
    }

    auto is_dragon_soul_refine_material(item *item_value) -> bool {
        if (item_value->type() != ITEM_MATERIAL)
            return false;
        return (item_value->sub_type() == MATERIAL_DS_REFINE_NORMAL ||
                item_value->sub_type() == MATERIAL_DS_REFINE_BLESSED ||
                item_value->sub_type() == MATERIAL_DS_REFINE_HOLLY);
    }

    auto ds_manager::do_refine_strength(character *ch, item_pos (&item_poses)[DRAGON_SOUL_REFINE_GRID_SIZE]) -> bool {
        if (nullptr == ch)
            return false;
        if (!ch->dragon_soul_refine_window_can_refine()) {
            metin2::log::warn("Player {} sent a dragon soul refine without the refine window open", ch->name());
            ch->chat_packet(CHAT_TYPE_INFO, "[SYSTEM ERROR]You cannot use dragon soul refine window.");
            return false;
        }

        std::set<item *> set_items;
        for (int i = 0; i < DRAGON_SOUL_REFINE_GRID_SIZE; i++) {
            item *pItem = ch->get_item(item_poses[i]);
            if (pItem) {
                if (pItem->is_equipped() || pItem->is_exchanging())
                    return false;

                set_items.insert(pItem);
            }
        }
        if (set_items.size() == 0) {
            return false;
        }

        int fee;

        item *pRefineStone = nullptr;
        item *pDragonSoul = nullptr;
        for (std::set<item *>::iterator it = set_items.begin(); it != set_items.end(); it++) {
            item *pItem = *it;
            if (pItem->is_equipped()) {
                return false;
            }

            if (pItem->is_dragon_soul()) {
                if (pDragonSoul != nullptr) {
                    send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL, item_pos(pItem->window(), pItem->cell()));
                    return false;
                }
                pDragonSoul = pItem;
            } else if (is_dragon_soul_refine_material(pItem)) {
                if (pRefineStone != nullptr) {
                    send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL, item_pos(pItem->window(), pItem->cell()));
                    return false;
                }
                pRefineStone = pItem;
            } else {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("강화에 필요한 재료가 아닙니다."));
                send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(pItem->window(), pItem->cell()));
                return false;
            }
        }

        uint8_t bType = 0, bGrade = 0, bStep = 0, bStrength = 0;

        if (!pDragonSoul || !pRefineStone) {
            send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL, NPOS);

            return false;
        }

        if (nullptr != pDragonSoul) {
            dragon_soul_info(pDragonSoul->vnum(), bType, bGrade, bStep, bStrength);

            float fWeight = 0.f;
            if (!table_->weight(bType, bGrade, bStep, bStrength + 1, fWeight)) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("강화할 수 없는 용혼석입니다."));
                send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_MAX_REFINE, item_pos(pDragonSoul->window(), pDragonSoul->cell()));
                return false;
            }
            if (fWeight < FLT_EPSILON) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("강화할 수 없는 용혼석입니다."));
                send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_MAX_REFINE, item_pos(pDragonSoul->window(), pDragonSoul->cell()));
                return false;
            }
        }

        float fProb;
        if (!table_->refine_strength_values(bType, pRefineStone->sub_type(), bStrength, fee, fProb)) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("강화할 수 없는 용혼석입니다."));
            send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, item_pos(pDragonSoul->window(), pDragonSoul->cell()));

            return false;
        }

        if (ch->gold() < fee) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("개량을 하기 위한 돈이 부족합니다."));
            send_refine_result_packet(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MONEY, NPOS);
            return false;
        }

        ch->point_change(POINT_GOLD, -fee);
        item *pResult = nullptr;
        uint8_t bSubHeader;

        if (fnumber(0.f, 100.f) <= fProb) {
            pResult = item_manager::instance().create_item(make_dragon_soul_vnum(bType, bGrade, bStep, bStrength + 1));
            if (nullptr == pResult) {
                metin2::log::error("Cannot create dragon soul (vnum {:d})", make_dragon_soul_vnum(bType, bGrade, bStep, bStrength + 1));
                return false;
            }
            pDragonSoul->remove_from_character();

            pDragonSoul->copy_attribute_to(pResult);
            refresh_item_attributes(pResult);

            pDragonSoul->set_count(pDragonSoul->count() - 1);
            pRefineStone->set_count(pRefineStone->count() - 1);

            const std::string buf = std::format("STRENGTH : {} -> {}", bStrength, bStrength + 1);
            log_manager::instance().item_log(ch, pDragonSoul, "DS_STRENGTH_REFINE_SUCCESS", buf.c_str());
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("강화에 성공했습니다."));
            ch->auto_give_item(pResult, true);
            bSubHeader = DS_SUB_HEADER_REFINE_SUCCEED;
        } else {
            if (bStrength != 0) {
                pResult = item_manager::instance().create_item(make_dragon_soul_vnum(bType, bGrade, bStep, bStrength - 1));
                if (nullptr == pResult) {
                    metin2::log::error("Cannot create dragon soul (vnum {:d})", make_dragon_soul_vnum(bType, bGrade, bStep, bStrength - 1));
                    return false;
                }
                pDragonSoul->copy_attribute_to(pResult);
                refresh_item_attributes(pResult);
            }
            bSubHeader = DS_SUB_HEADER_REFINE_FAIL;

            const std::string buf = std::format("STRENGTH : {} -> {}", bStrength, bStrength - 1);
            log_manager::instance().item_log(ch, pDragonSoul, "DS_STRENGTH_REFINE_FAIL", buf.c_str());

            ch->chat_packet(CHAT_TYPE_INFO, lc_text("강화에 실패했습니다."));
            pDragonSoul->set_count(pDragonSoul->count() - 1);
            pRefineStone->set_count(pRefineStone->count() - 1);
            if (nullptr != pResult)
                ch->auto_give_item(pResult, true);
        }

        send_refine_result_packet(ch, bSubHeader, nullptr == pResult ? NPOS : item_pos(pResult->window(), pResult->cell()));

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

        if (item_value->proto()->c_limit_timer_based_on_wear_index >= 0) {
            return item_value->get_socket(ITEM_SOCKET_REMAIN_SEC);
        } else {
            return INT_MAX;
        }
    }

    auto ds_manager::is_time_left_dragon_soul(item *item_value) const -> bool {
        if (item_value == nullptr)
            return false;

        if (item_value->proto()->c_limit_timer_based_on_wear_index >= 0) {
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
        character *pOwner = item_value->owner();
        if (nullptr == pOwner)
            return false;

        int deck_idx = pOwner->dragon_soul_get_active_deck();

        if (deck_idx < 0)
            return false;

        if (INVENTORY_MAX_NUM + WEAR_MAX_NUM + DS_SLOT_MAX * deck_idx <= item_value->cell() &&
            item_value->cell() < INVENTORY_MAX_NUM + WEAR_MAX_NUM + DS_SLOT_MAX * (deck_idx + 1)) {
            if (is_time_left_dragon_soul(item_value) && !is_active_dragon_soul(item_value)) {
                const std::string buf = std::format("LEFT TIME({})", left_time(item_value));
                log_manager::instance().item_log(pOwner, item_value, "DS_ACTIVATE", buf.c_str());
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

        character *pOwner = item_value->owner();
        if (nullptr == pOwner)
            return false;

        if (!is_active_dragon_soul(item_value))
            return false;

        item_value->stop_timer_based_on_wear_expire_event();
        item_value->set_socket(ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX, 0);
        item_value->modify_points(false);

        const std::string buf = std::format("LEFT TIME({})", left_time(item_value));
        log_manager::instance().item_log(pOwner, item_value, "DS_DEACTIVATE", buf.c_str());

        if (false == skip_refresh_owner_active_state)
            refresh_dragon_soul_state(pOwner);

        return true;
    }

    void ds_manager::refresh_dragon_soul_state(character *ch) {
        if (nullptr == ch)
            return;
        for (int i = WEAR_MAX_NUM; i < WEAR_MAX_NUM + static_cast<int>(DS_SLOT_MAX) * DRAGON_SOUL_DECK_MAX_NUM; i++) {
            item *pItem = ch->wear(i);
            if (pItem != nullptr) {
                if (is_active_dragon_soul(pItem)) {
                    return;
                }
            }
        }
        ch->dragon_soul_deactivate_all();
    }

    ds_manager::ds_manager() {
    }

    ds_manager::~ds_manager() {
    }
}
