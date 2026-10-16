

#include <metin2/config/config.h>
#include <metin2/game/items/cube.h>

#include <algorithm>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>

#include <metin2/common/utils.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace metin2::game::inline items {
    static std::vector<std::unique_ptr<cube_data>> cube_proto;
    static bool is_initialized_cube_material_information = false;

    enum cube_result_category {
        CUBE_CATEGORY_POTION,
        CUBE_CATEGORY_WEAPON,
        CUBE_CATEGORY_ARMOR,
    };

    using cube_value_vector = std::vector<cube_value>;

    struct cube_material_info {
        cube_material_info() {
            have_complicate_material = false;
        };

        cube_value reward;
        cube_value_vector material;
        uint32_t gold;
        cube_value_vector complicate_material;

        // Material format shown by the client: "a,1|b,1&c,5" means (a or b) and c.
        std::string info_text;
        bool have_complicate_material;
    };

    struct item_name_and_level {
        item_name_and_level() { level = 0; }

        std::string name;
        int level;
    };

    using cube_result_list = std::vector<cube_material_info>;
    using cube_map_by_npc = std::unordered_map<uint32_t, cube_result_list>;
    using cube_result_info_text_by_npc = std::unordered_map<uint32_t, std::string>;

    cube_map_by_npc cube_info_map;
    cube_result_info_text_by_npc cube_result_info_map_by_npc;

    static auto fn_check_item_count(item **items, uint32_t item_vnum, int need_count) -> bool {
        int count = 0;

        for (int i = 0; i < CUBE_MAX_NUM; ++i) {
            if (nullptr == items[i]) continue;

            if (item_vnum == items[i]->vnum()) {
                count += items[i]->count();
            }
        }

        return (count >= need_count);
    }

    static void fn_remove_material(item **items, uint32_t item_vnum, int need_count) {
        int count = 0;
        item *item = nullptr;

        for (int i = 0; i < CUBE_MAX_NUM; ++i) {
            if (nullptr == items[i]) continue;

            item = items[i];
            if (item_vnum == item->vnum()) {
                count += item->count();

                if (count > need_count) {
                    item->set_count(count - need_count);
                    return;
                } else {
                    item->set_count(0);
                    items[i] = nullptr;
                }
            }
        }
    }

    static auto fn_find_cube(item **items, uint16_t npc_vnum) -> cube_data * {
        uint32_t i, end_index;

        if (0 == npc_vnum) return nullptr;

        end_index = cube_proto.size();
        for (i = 0; i < end_index; ++i) {
            if (cube_proto[i]->can_make_item(items, npc_vnum))
                return cube_proto[i].get();
        }

        return nullptr;
    }

    static auto fn_check_valid_npc(uint16_t vnum) -> bool {
        for (auto iter = cube_proto.begin(); iter != cube_proto.end(); iter++) {
            if (std::find((*iter)->npc_vnum.begin(), (*iter)->npc_vnum.end(), vnum) != (*iter)->npc_vnum.end())
                return true;
        }

        return false;
    }

    static auto fn_check_cube_data(cube_data *cube_data) -> bool {
        uint32_t i = 0;
        uint32_t end_index = 0;

        end_index = cube_data->npc_vnum.size();
        for (i = 0; i < end_index; ++i) {
            if (cube_data->npc_vnum[i] == 0) return false;
        }

        end_index = cube_data->item.size();
        for (i = 0; i < end_index; ++i) {
            if (cube_data->item[i].vnum == 0) return false;
            if (cube_data->item[i].count == 0) return false;
        }

        end_index = cube_data->reward.size();
        for (i = 0; i < end_index; ++i) {
            if (cube_data->reward[i].vnum == 0) return false;
            if (cube_data->reward[i].count == 0) return false;
        }
        return true;
    }

    cube_data::cube_data() {
        this->percent = 0;
        this->gold = 0;
    }

    auto cube_data::can_make_item(game::items::item **items, uint16_t npc_vnum) -> bool {
        uint32_t i, end_index;
        uint32_t need_vnum;
        int need_count;
        int found_npc = false;

        end_index = this->npc_vnum.size();
        for (i = 0; i < end_index; ++i) {
            if (npc_vnum == this->npc_vnum[i])
                found_npc = true;
        }
        if (false == found_npc) return false;

        end_index = this->item.size();
        for (i = 0; i < end_index; ++i) {
            need_vnum = this->item[i].vnum;
            need_count = this->item[i].count;

            if (false == fn_check_item_count(items, need_vnum, need_count))
                return false;
        }

        return true;
    }

    auto cube_data::reward_value() -> cube_value * {
        int end_index = 0;
        uint32_t reward_index = 0;

        end_index = this->reward.size();
        reward_index = number(0, end_index);
        reward_index = number(0, end_index - 1);

        return &this->reward[reward_index];
    }

    void cube_data::remove_material(character *ch) {
        uint32_t i, end_index;
        uint32_t need_vnum;
        int need_count;
        game::items::item **items = ch->cube_item();

        end_index = this->item.size();
        for (i = 0; i < end_index; ++i) {
            need_vnum = this->item[i].vnum;
            need_count = this->item[i].count;

            fn_remove_material(items, need_vnum, need_count);
        }
    }

    void cube_clean_item(character *ch) {
        item **cube_item;

        cube_item = ch->cube_item();

        for (int i = 0; i < CUBE_MAX_NUM; ++i) {
            if (nullptr == cube_item[i])
                continue;

            cube_item[i] = nullptr;
        }
    }

    void cube_open(character *ch) {
        if (false == is_initialized_cube_material_information) {
            cube_information_initialize();
        }

        if (nullptr == ch)
            return;

        character *npc;
        npc = ch->quest_npc();
        if (nullptr == npc) {
            if (metin2::config::test_server())
                metin2::log::debug("Cube: player {} has no cube npc", ch->name());
            return;
        }

        if (fn_check_valid_npc(npc->race_num()) == false) {
            if (metin2::config::test_server() == true) {
                metin2::log::debug("Cube: npc of player {} is not a cube npc", ch->name());
            }
            return;
        }

        if (ch->is_cube_open()) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("이미 제조창이 열려있습니다."));
            return;
        }
        if (ch->get_exchange() || ch->my_shop() || ch->shop_owner() || ch->is_open_safebox() || ch->is_cube_open()) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("다른 거래중(창고,교환,상점)에는 사용할 수 없습니다."));
            return;
        }

        int32_t distance = distance_approx(ch->x() - npc->x(), ch->y() - npc->y());

        if (distance >= CUBE_MAX_DISTANCE) {
            metin2::log::debug("Cube: player {} is too far from the npc (distance {:d})", ch->name(), distance);
            return;
        }

        cube_clean_item(ch);
        ch->set_cube_npc(npc);
        ch->chat_packet(CHAT_TYPE_COMMAND, "cube open %d", npc->race_num());
    }

    void cube_close(character *ch) {
        if (!ch->is_cube_open())
            return;
        cube_clean_item(ch);
        ch->set_cube_npc(nullptr);
        ch->chat_packet(CHAT_TYPE_COMMAND, "cube close");
        metin2::log::debug("Cube: closed for player {}", ch->name());
    }

    void cube_init() {
        const std::string file_name = std::format("{}/cube.txt", locale_service_get_base_path());

        metin2::log::info("Cube: loading {}", file_name);

        cube_proto.clear();

        if (false == cube_load(file_name.c_str()))
            metin2::log::error("Cannot load cube recipes from {}", file_name);
    }

    auto cube_load(const char *file) -> bool {
        FILE *fp;
        char one_line[256];
        int value1, value2;
        const char *delim = " \t\r\n";
        char *v, *token_string;
        std::unique_ptr<cube_data> cube_data;
        cube_value cube_value = {0, 0};

        if (nullptr == file || 0 == file[0])
            return false;

        if ((fp = fopen(file, "r")) == nullptr)
            return false;

        while (fgets(one_line, 256, fp)) {
            value1 = value2 = 0;

            if (one_line[0] == '#')
                continue;

            token_string = strtok(one_line, delim);

            if (nullptr == token_string)
                continue;

            if ((v = strtok(nullptr, delim)))
                str_to_number(value1, v);

            if ((v = strtok(nullptr, delim)))
                str_to_number(value2, v);

            if (!strcasecmp(token_string, "section")) {
                cube_data = std::make_unique<game::items::cube_data>();
            } else if (!cube_data) {
                metin2::log::error("Cube token '{}' is outside a recipe section", token_string);
            } else if (!strcasecmp(token_string, "npc")) {
                cube_data->npc_vnum.push_back((uint16_t)value1);
            } else if (!strcasecmp(token_string, "item")) {
                cube_value.vnum = value1;
                cube_value.count = value2;

                cube_data->item.push_back(cube_value);
            } else if (!strcasecmp(token_string, "reward")) {
                cube_value.vnum = value1;
                cube_value.count = value2;

                cube_data->reward.push_back(cube_value);
            } else if (!strcasecmp(token_string, "percent")) {
                cube_data->percent = value1;
            } else if (!strcasecmp(token_string, "gold")) {
                cube_data->gold = value1;
            } else if (!strcasecmp(token_string, "end")) {
                if (false == fn_check_cube_data(cube_data.get())) {
                    metin2::log::error("Skipping invalid cube recipe in {}", file);
                    cube_data.reset();
                    continue;
                }
                cube_proto.push_back(std::move(cube_data));
            }
        }

        fclose(fp);
        return true;
    }

    static void fn_validate_cube_items(character *ch) {
        item **cube_item = ch->cube_item();

        for (int i = 0; i < CUBE_MAX_NUM; ++i) {
            if (nullptr == cube_item[i])
                continue;

            bool found = false;

            for (int cell = 0; cell < INVENTORY_MAX_NUM; ++cell) {
                if (ch->inventory_item(cell) == cube_item[i]) {
                    found = true;
                    break;
                }
            }

            if (!found)
                cube_item[i] = nullptr;
        }
    }

    static auto fn_update_cube_status(character *ch) -> bool {
        if (nullptr == ch)
            return false;

        if (!ch->is_cube_open())
            return false;

        fn_validate_cube_items(ch);

        character *npc = ch->quest_npc();
        if (nullptr == npc)
            return false;

        cube_data *cube = fn_find_cube(ch->cube_item(), npc->race_num());

        if (nullptr == cube) {
            ch->chat_packet(CHAT_TYPE_COMMAND, "cube info 0 0 0");
            return false;
        }

        ch->chat_packet(CHAT_TYPE_COMMAND, "cube info %d %d %d", cube->gold, 0, 0);
        return true;
    }

    auto cube_make(character *ch) -> bool {

        character *npc;
        int percent_number = 0;
        cube_data *cube_proto;
        item **items;
        item *new_item;

        if (!(ch)->is_cube_open()) {
            (ch)->chat_packet(CHAT_TYPE_INFO, lc_text("제조창이 열려있지 않습니다"));
            return false;
        }

        npc = ch->quest_npc();
        if (nullptr == npc) {
            return false;
        }

        fn_validate_cube_items(ch);
        items = ch->cube_item();
        cube_proto = fn_find_cube(items, npc->race_num());

        if (nullptr == cube_proto) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("제조 재료가 부족합니다"));
            return false;
        }

        if (static_cast<int64_t>(ch->gold()) < static_cast<int64_t>(cube_proto->gold)) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("돈이 부족하거나 아이템이 제자리에 없습니다."));
            return false;
        }

        cube_value *reward_value = cube_proto->reward_value();

        cube_proto->remove_material(ch);

        if (0 < cube_proto->gold)
            ch->point_change(POINT_GOLD, -(cube_proto->gold), false);

        percent_number = number(1, 100);
        if (percent_number <= cube_proto->percent) {
            ch->chat_packet(CHAT_TYPE_COMMAND, "cube success %d %d", reward_value->vnum, reward_value->count);
            new_item = ch->auto_give_item(reward_value->vnum, reward_value->count);

            get_log_manager().cube_log(ch->player_id(), ch->x(), ch->y(),
                reward_value->vnum, new_item ? new_item->id() : 0, reward_value->count, 1);
            return true;
        } else {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("제조에 실패하였습니다."));
            ch->chat_packet(CHAT_TYPE_COMMAND, "cube fail");
            get_log_manager().cube_log(ch->player_id(), ch->x(), ch->y(),
                reward_value->vnum, 0, 0, 0);
            return false;
        }

        return false;
    }

    void cube_show_list(character *ch) {
        item **cube_item;
        item *item;

        if (!ch->is_cube_open())
            return;

        fn_validate_cube_items(ch);
        cube_item = ch->cube_item();

        for (int i = 0; i < CUBE_MAX_NUM; ++i) {
            item = cube_item[i];
            if (nullptr == item) continue;

            ch->chat_packet(CHAT_TYPE_INFO, "cube[%d]: inventory[%d]: %s",
                i, item->cell(), item->name());
        }
    }

    void cube_add_item(character *ch, int cube_index, int inven_index) {
        item *item;
        game::items::item **cube_item;

        if (!ch->is_cube_open())
            return;

        if (inven_index < 0 || INVENTORY_MAX_NUM <= inven_index)
            return;
        if (cube_index < 0 || CUBE_MAX_NUM <= cube_index)
            return;

        item = ch->inventory_item(inven_index);

        if (nullptr == item) return;

        cube_item = ch->cube_item();

        for (int i = 0; i < CUBE_MAX_NUM; ++i) {
            if (item == cube_item[i]) {
                cube_item[i] = nullptr;
                break;
            }
        }

        cube_item[cube_index] = item;

        if (metin2::config::test_server())
            ch->chat_packet(CHAT_TYPE_INFO, "cube[%d]: inventory[%d]: %s added",
                cube_index, inven_index, item->name());

        fn_update_cube_status(ch);

        return;
    }

    void cube_delete_item(character *ch, int cube_index) {
        item *item;
        game::items::item **cube_item;

        if (!ch->is_cube_open())
            return;

        if (cube_index < 0 || CUBE_MAX_NUM <= cube_index) return;

        fn_validate_cube_items(ch);
        cube_item = ch->cube_item();

        if (nullptr == cube_item[cube_index]) return;

        item = cube_item[cube_index];
        cube_item[cube_index] = nullptr;

        if (metin2::config::test_server())
            ch->chat_packet(CHAT_TYPE_INFO, "cube[%d]: cube[%d]: %s deleted",
                cube_index, item->cell(), item->name());

        fn_update_cube_status(ch);

        return;
    }

    auto split_item_name_and_level_from_name(const std::string &name) -> item_name_and_level {
        int level = 0;
        item_name_and_level info;
        info.name = name;

        size_t pos = name.find("+");

        if (std::string::npos != pos) {
            const std::string level_str = name.substr(pos + 1, name.size() - pos - 1);
            str_to_number(level, level_str.c_str());

            info.name = name.substr(0, pos);
        }

        info.level = level;

        return info;
    };

    auto f_is_less_cube_value(const cube_value &a, const cube_value &b) -> bool {
        return a.vnum < b.vnum;
    }

    void cube_make_cube_information_text() {
        for (auto iter = cube_info_map.begin(); cube_info_map.end() != iter; ++iter) {
            cube_result_list &result_list = iter->second;

            for (auto result_iter = result_list.begin(); result_list.end() != result_iter; ++result_iter) {
                cube_material_info &material_info = *result_iter;
                std::string &info_text = material_info.info_text;

                if (0 < material_info.complicate_material.size()) {
                    std::ranges::sort(material_info.complicate_material, f_is_less_cube_value);
                    std::ranges::sort(material_info.material, f_is_less_cube_value);

                    for (auto iter = material_info.complicate_material.begin(); material_info.complicate_material.end() != iter; ++iter) {
                        for (auto target_iter = material_info.material.begin(); material_info.material.end() != target_iter; ++target_iter) {
                            if (*target_iter == *iter) {
                                target_iter = material_info.material.erase(target_iter);
                            }
                        }
                    }

                    for (auto iter = material_info.complicate_material.begin(); material_info.complicate_material.end() != iter; ++iter) {
                        info_text += std::format("{},{}|", iter->vnum, iter->count);
                    }

                    info_text.erase(info_text.size() - 1);

                    if (0 < material_info.material.size())
                        info_text.push_back('&');
                }

                for (auto iter = material_info.material.begin(); material_info.material.end() != iter; ++iter) {
                    info_text += std::format("{},{}&", iter->vnum, iter->count);
                }

                info_text.erase(info_text.size() - 1);

                if (0 < material_info.gold) {
                    info_text += std::format("/{}", material_info.gold);
                }
            }
        }
    }

    auto cube_information_initialize() -> bool {
        for (size_t i = 0; i < cube_proto.size(); ++i) {
            cube_data *cube_data = cube_proto[i].get();

            const std::vector<cube_value> &rewards = cube_data->reward;

            if (1 != rewards.size()) {
                metin2::log::warn("Cube recipe has {:d} rewards; only one is supported", rewards.size());
                continue;
            }

            const cube_value &reward = rewards.at(0);
            const uint16_t &npc_vnum = cube_data->npc_vnum.at(0);
            bool complicate = false;

            cube_map_by_npc &cube_map = cube_info_map;
            cube_result_list &result_list = cube_map[npc_vnum];
            cube_material_info material_info;

            material_info.reward = reward;
            material_info.gold = cube_data->gold;
            material_info.material = cube_data->item;

            for (auto iter = result_list.begin(); result_list.end() != iter; ++iter) {
                cube_material_info &exist_info = *iter;

                if (reward.vnum == exist_info.reward.vnum) {
                    for (auto exist_material_iter = exist_info.material.begin(); exist_info.material.end() != exist_material_iter; ++exist_material_iter) {
                        item_table *exist_material_proto = get_item_manager().table(exist_material_iter->vnum);
                        if (nullptr == exist_material_proto) {
                            metin2::log::error("No item proto for cube material vnum {:d}", exist_material_iter->vnum);
                            return false;
                        }
                        item_name_and_level exist_item_info = split_item_name_and_level_from_name(exist_material_proto->name);

                        if (0 < exist_item_info.level) {
                            for (auto current_material_iter = material_info.material.begin(); material_info.material.end() != current_material_iter; ++current_material_iter) {
                                item_table *current_material_proto = get_item_manager().table(current_material_iter->vnum);
                                item_name_and_level current_item_info = split_item_name_and_level_from_name(current_material_proto->name);

                                if (current_item_info.name == exist_item_info.name) {
                                    complicate = true;
                                    exist_info.complicate_material.push_back(*current_material_iter);

                                    if (std::ranges::find(exist_info.complicate_material, *exist_material_iter) == exist_info.complicate_material.end())
                                        exist_info.complicate_material.push_back(*exist_material_iter);

                                    break;
                                }
                            }
                        }
                    }
                }
            }

            if (false == complicate)
                result_list.push_back(material_info);
        }

        cube_make_cube_information_text();

        is_initialized_cube_material_information = true;
        return true;
    }

    void cube_request_result_list(character *ch) {
        if (!ch->is_cube_open())
            return;

        character *npc = ch->quest_npc();
        if (nullptr == npc)
            return;

        uint32_t npc_vnum_value = npc->race_num();
        size_t result_count = 0;

        std::string &result_text = cube_result_info_map_by_npc[npc_vnum_value];

        if (result_text.length() == 0) {
            result_text.clear();

            const cube_result_list &result_list = cube_info_map[npc_vnum_value];
            for (auto iter = result_list.begin(); result_list.end() != iter; ++iter) {
                const cube_material_info &material_info = *iter;
                result_text += std::format("{},{}/", material_info.reward.vnum, material_info.reward.count);
            }

            result_count = result_list.size();

            if (result_count == 0) {
                return;
            }

            result_text.erase(result_text.size() - 1);

            if (result_text.size() - 20 >= CHAT_MAX_LEN) {
                metin2::log::error("Cube result list of npc {:d} is too long ({:d} chars)", npc_vnum_value, result_text.size());
                result_text.clear();
                result_count = 0;
            }
        }

        // /cube r_list npc_vnum count vnum,count/vnum,count/...

        ch->chat_packet(CHAT_TYPE_COMMAND, "cube r_list %d %d %s", npc_vnum_value, result_count, result_text.c_str());
    }

    void cube_request_material_info(character *ch, int request_start_index, int request_count) {
        if (!ch->is_cube_open())
            return;

        character *npc = ch->quest_npc();
        if (nullptr == npc)
            return;

        uint32_t npc_vnum = npc->race_num();
        std::string material_info_text = "";

        int index = 0;
        bool catch_info = false;

        const cube_result_list &result_list = cube_info_map[npc_vnum];
        for (auto iter = result_list.begin(); result_list.end() != iter; ++iter) {
            const cube_material_info &material_info = *iter;

            if (index++ == request_start_index) {
                catch_info = true;
            }

            if (catch_info) {
                material_info_text += material_info.info_text + "@";
            }

            if (index >= request_start_index + request_count)
                break;
        }

        if (!catch_info || material_info_text.size() == 0) {
            metin2::log::warn("No cube material info for npc {:d}, start index {:d}, count {:d}", npc_vnum, request_start_index, request_count);
            return;
        }

        material_info_text.erase(material_info_text.size() - 1);

        // /cube m_info start count 125,1|126,2&555,5/120000@...
        if (material_info_text.size() - 20 >= CHAT_MAX_LEN) {
            metin2::log::error("Cube material info of npc {:d} (start {:d}, count {:d}) is too long ({:d} chars)", npc_vnum, request_start_index, request_count, material_info_text.size());
        }

        ch->chat_packet(CHAT_TYPE_COMMAND, "cube m_info %d %d %s", request_start_index, request_count, material_info_text.c_str());
    }
}
