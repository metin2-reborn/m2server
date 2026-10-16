#include <metin2/game/app/constants.h>
#include <metin2/game/data/file_loader.h>
#include <metin2/game/data/text_file_loader.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/items/item_manager_private_types.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/text/locale_service.h>

#include <metin2/common/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/apply_types.h>
#include <metin2/proto/mob_types.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game::inline items {

    std::vector<item_drop_info> common_drop_items[MOB_RANK_MAX_NUM];

    auto item_manager::read_common_drop_item_file(const char *file_name) -> bool {
        FILE *fp = fopen(file_name, "r");

        if (!fp) {
            metin2::log::error("Cannot open {}", file_name);
            return false;
        }

        char buf[1024];

        while (fgets(buf, 1024, fp)) {

            if (!*buf || *buf == '\n')
                continue;

            drop_item d[MOB_RANK_MAX_NUM];
            char temp[64];

            memset(&d, 0, sizeof(d));

            char *p = buf;
            char *p2;

            for (int i = 0; i <= MOB_RANK_S_KNIGHT; ++i) {
                for (int j = 0; j < 6; ++j) {
                    p2 = strchr(p, '\t');

                    if (!p2)
                        break;

                    strlcpy(temp, p, std::min<int>(sizeof(temp), (p2 - p) + 1));
                    p = p2 + 1;

                    switch (j) {
                    case 0: break;
                    case 1: str_to_number(d[i].lv_start, temp); break;
                    case 2: str_to_number(d[i].lv_end, temp); break;
                    case 3: d[i].percent = atof(temp); break;
                    case 4: strlcpy(d[i].item_name, temp, sizeof(d[i].item_name)); break;
                    case 5: str_to_number(d[i].count, temp); break;
                    }
                }

                auto pct = (uint32_t)(d[i].percent * 10000.0f);
                uint32_t item_vnum = 0;

                if (!get_item_manager().vnum_by_original_name(d[i].item_name, item_vnum)) {
                    str_to_number(item_vnum, d[i].item_name);
                    if (!get_item_manager().table(item_vnum)) {
                        metin2::log::error("Unknown item name {}", d[i].item_name);
                        fclose(fp);
                        return false;
                    }
                }

                if (d[i].lv_start == 0)
                    continue;

                common_drop_items[i].push_back(item_drop_info(d[i].lv_start, d[i].lv_end, pct, item_vnum));
            }
        }

        fclose(fp);

        for (int i = 0; i < MOB_RANK_MAX_NUM; ++i) {
            std::vector<item_drop_info> &v = common_drop_items[i];
            std::sort(v.begin(), v.end());

            auto it = v.begin();

            metin2::log::debug("Common drop: rank {:d}", i);

            while (it != v.end()) {
                const item_drop_info &c = *(it++);
                metin2::log::debug("Common drop: levels {:d}-{:d}, chance {:d}, vnum {:d}", c.level_start, c.level_end, c.percent, c.vnum);
            }
        }

        return true;
    }

    auto item_manager::read_special_drop_item_file(const char *file_name) -> bool {
        text_file_loader loader;

        if (!loader.load(file_name))
            return false;

        std::string name_value;

        for (uint32_t i = 0; i < loader.child_node_count(); ++i) {
            loader.set_child_node(i);

            loader.current_node_name(&name_value);

            int vnum_value;

            if (!loader.token_integer("vnum", &vnum_value)) {
                metin2::log::error("{}: group {} has no vnum", file_name, name_value);
                loader.set_parent_node();
                return false;
            }

            metin2::log::debug("Special item group {} (vnum {:d})", name_value, vnum_value);

            token_vector *tok;

            std::string type_value;
            int type = special_item_group::NORMAL;
            if (loader.token_string("type", &type_value)) {
                to_lower(type_value);
                if (type_value == "pct") {
                    type = special_item_group::PCT;
                } else if (type_value == "quest") {
                    type = special_item_group::QUEST;
                    quest::get_quest_manager().register_npc_vnum(vnum_value);
                } else if (type_value == "special") {
                    type = special_item_group::SPECIAL;
                }
            }

            if ("attr" == type_value) {
                auto group_value = std::make_unique<special_attr_group>(vnum_value);
                for (int k = 1; k < 256; ++k) {
                    if (loader.get_token_vector(std::to_string(k), &tok)) {
                        uint32_t apply_type = 0;
                        int apply_value = 0;
                        str_to_number(apply_type, tok->at(0).c_str());
                        if (0 == apply_type) {
                            apply_type = fn_get_apply_type(tok->at(0).c_str());
                            if (0 == apply_type) {
                                metin2::log::error("Unknown apply type {} in special item group {:d}", tok->at(0), vnum_value);
                                return false;
                            }
                        }
                        str_to_number(apply_value, tok->at(1).c_str());
                        if (apply_type > MAX_APPLY_NUM) {
                            metin2::log::error("Apply type {:d} is out of range in special item group {:d}", apply_type, vnum_value);
                            return false;
                        }
                        group_value->attrs.push_back(special_attr_group::special_attr_info(apply_type, apply_value));
                    } else {
                        break;
                    }
                }
                if (loader.get_token_vector("effect", &tok)) {
                    group_value->effect_file_name = tok->at(0);
                }
                loader.set_parent_node();
                special_attr_groups_.emplace(vnum_value, std::move(group_value));
            } else {
                auto group_value = std::make_unique<special_item_group>(vnum_value, type);
                for (int k = 1; k < 256; ++k) {
                    if (loader.get_token_vector(std::to_string(k), &tok)) {
                        const std::string &name = tok->at(0);
                        uint32_t vnum = 0;

                        if (!vnum_by_original_name(name.c_str(), vnum)) {
                            if (name == "경험치" || name == "exp") {
                                vnum = special_item_group::EXP;
                            } else if (name == "mob") {
                                vnum = special_item_group::MOB;
                            } else if (name == "slow") {
                                vnum = special_item_group::SLOW;
                            } else if (name == "drain_hp") {
                                vnum = special_item_group::DRAIN_HP;
                            } else if (name == "poison") {
                                vnum = special_item_group::POISON;
                            } else if (name == "group") {
                                vnum = special_item_group::MOB_GROUP;
                            } else {
                                str_to_number(vnum, name.c_str());
                                if (!get_item_manager().table(vnum)) {
                                    metin2::log::error("Unknown item {} in special item group {}", name, name_value);
                                    return false;
                                }
                            }
                        }

                        int count_value = 0;
                        str_to_number(count_value, tok->at(1).c_str());
                        int prob = 0;
                        str_to_number(prob, tok->at(2).c_str());

                        int rare_pct = 0;
                        if (tok->size() > 3) {
                            str_to_number(rare_pct, tok->at(3).c_str());
                        }

                        metin2::log::debug("Special item group: item {} count {:d} chance {:d} rare {:d}", name, count_value, prob, rare_pct);
                        group_value->add_item(vnum, count_value, prob, rare_pct);

                        if (vnum_value < 30000) {
                            item_to_special_group_[vnum] = vnum_value;
                        }

                        continue;
                    }

                    break;
                }
                loader.set_parent_node();
                if (special_item_group::QUEST == type) {
                    quest_item_groups_.emplace(vnum_value, std::move(group_value));
                } else {
                    special_item_groups_.emplace(vnum_value, std::move(group_value));
                }
            }
        }

        return true;
    }

    auto item_manager::conv_special_drop_item_file() -> bool {
        const std::string special_item_group_file_name = std::format("{}/special_item_group.txt", locale_service_get_base_path());

        FILE *fp = fopen("special_item_group_vnum.txt", "w");
        if (!fp) {
            metin2::log::error("Cannot open {}", "special_item_group_vnum.txt");
            return false;
        }

        text_file_loader loader;

        if (!loader.load(special_item_group_file_name.c_str())) {
            fclose(fp);
            return false;
        }

        std::string name_value;

        for (uint32_t i = 0; i < loader.child_node_count(); ++i) {
            loader.set_child_node(i);

            loader.current_node_name(&name_value);

            int vnum_value;

            if (!loader.token_integer("vnum", &vnum_value)) {
                metin2::log::error("{}: group {} has no vnum", special_item_group_file_name, name_value);
                loader.set_parent_node();
                fclose(fp);
                return false;
            }

            std::string str;
            int type = 0;
            if (loader.token_string("type", &str)) {
                to_lower(str);
                if (str == "pct") {
                    type = 1;
                }
            }

            token_vector *tok;

            fprintf(fp, "Group	%s\n", name_value.c_str());
            fprintf(fp, "{\n");
            fprintf(fp, "	Vnum	%i\n", vnum_value);
            if (type)
                fprintf(fp, "	Type	Pct");

            for (int k = 1; k < 256; ++k) {
                if (loader.get_token_vector(std::to_string(k), &tok)) {
                    const std::string &name = tok->at(0);
                    uint32_t vnum = 0;

                    if (!vnum_by_original_name(name.c_str(), vnum)) {
                        if (name == "경험치" ||
                            name == "mob" ||
                            name == "slow" ||
                            name == "drain_hp" ||
                            name == "poison" ||
                            name == "group") {
                            vnum = 0;
                        } else {
                            str_to_number(vnum, name.c_str());
                            if (!get_item_manager().table(vnum)) {
                                metin2::log::error("Unknown item {} in special item group {}", name, name_value);
                                fclose(fp);

                                return false;
                            }
                        }
                    }

                    int count = 0;
                    str_to_number(count, tok->at(1).c_str());
                    int prob = 0;
                    str_to_number(prob, tok->at(2).c_str());

                    int rare_pct = 0;
                    if (tok->size() > 3) {
                        str_to_number(rare_pct, tok->at(3).c_str());
                    }

                    if (0 == vnum)
                        fprintf(fp, "	%d	%s	%d	%d\n", k, name.c_str(), count, prob);
                    else
                        fprintf(fp, "	%d	%u	%d	%d\n", k, vnum, count, prob);

                    continue;
                }

                break;
            }
            fprintf(fp, "}\n");
            fprintf(fp, "\n");

            loader.set_parent_node();
        }

        fclose(fp);
        return true;
    }

    auto item_manager::read_etc_drop_item_file(const char *file_name) -> bool {
        FILE *fp = fopen(file_name, "r");

        if (!fp) {
            metin2::log::error("Cannot open {}", file_name);
            return false;
        }

        char buf[512];

        while (fgets(buf, 512, fp)) {

            if (!*buf || *buf == '\n')
                continue;

            char item_name[256];
            float prob = 0.0f;

            strlcpy(item_name, buf, sizeof(item_name));
            char *cp_tab = strrchr(item_name, '\t');

            if (!cp_tab)
                continue;

            *cp_tab = '\0';
            prob = atof(cp_tab + 1);

            if (!*item_name || prob == 0.0f)
                continue;

            uint32_t item_vnum;

            if (!get_item_manager().vnum_by_original_name(item_name, item_vnum)) {
                metin2::log::error("Unknown item name {}", item_name);
                fclose(fp);
                return false;
            }

            etc_item_drop_probs_[item_vnum] = (uint32_t)(prob * 10000.0f);
            metin2::log::debug("Etc drop: item {} chance {:f}", item_name, prob);
        }

        fclose(fp);
        return true;
    }

    auto item_manager::read_monster_drop_item_group(const char *file_name) -> bool {
        text_file_loader loader;

        if (!loader.load(file_name))
            return false;

        for (uint32_t i = 0; i < loader.child_node_count(); ++i) {
            std::string name_value("");

            loader.current_node_name(&name_value);

            if (strncmp(name_value.c_str(), "kr_", 3) == 0) {
                continue;
            }

            loader.set_child_node(i);

            int mob_vnum = 0;
            int kill_drop_value = 0;
            int level_limit_value = 0;

            std::string type_value("");

            if (!loader.token_string("type", &type_value)) {
                metin2::log::error("{}: group {} has no type (kill or drop)", file_name, name_value);
                loader.set_parent_node();
                return false;
            }

            if (!loader.token_integer("mob", &mob_vnum)) {
                metin2::log::error("{}: group {} has no mob vnum", file_name, name_value);
                loader.set_parent_node();
                return false;
            }

            if (type_value == "kill") {
                if (!loader.token_integer("kill_drop", &kill_drop_value)) {
                    metin2::log::error("{}: group {} has no kill_drop count", file_name, name_value);
                    loader.set_parent_node();
                    return false;
                }
            } else {
                kill_drop_value = 1;
            }

            if (type_value == "limit") {
                if (!loader.token_integer("level_limit", &level_limit_value)) {
                    metin2::log::error("{}: group {} has no level_limit", file_name, name_value);
                    loader.set_parent_node();
                    return false;
                }
            } else {
                level_limit_value = 0;
            }

            metin2::log::debug("Mob drop group {} ({}), mob {:d}, kill drop {:d}", name_value, type_value, mob_vnum, kill_drop_value);

            if (kill_drop_value == 0) {
                loader.set_parent_node();
                continue;
            }

            token_vector *tok = nullptr;

            if (type_value == "kill") {
                auto group_value = std::make_unique<mob_item_group>(kill_drop_value);

                for (int k = 1; k < 256; ++k) {
                    if (loader.get_token_vector(std::to_string(k), &tok)) {
                        std::string &name = tok->at(0);
                        uint32_t vnum_value = 0;

                        if (!vnum_by_original_name(name.c_str(), vnum_value)) {
                            str_to_number(vnum_value, name.c_str());
                            if (!get_item_manager().table(vnum_value)) {
                                metin2::log::error("Unknown item {} (vnum {:d}) in mob drop group {}", name, vnum_value, name_value);
                                return false;
                            }
                        }

                        int count_value = 0;
                        str_to_number(count_value, tok->at(1).c_str());

                        if (count_value < 1) {
                            metin2::log::error("Item {} (vnum {:d}) in mob drop group {} has invalid count {:d}", name, vnum_value, name_value, count_value);
                            return false;
                        }

                        int part_pct = 0;
                        str_to_number(part_pct, tok->at(2).c_str());

                        if (part_pct == 0) {
                            metin2::log::error("Item {} (vnum {:d}, count {:d}) in mob drop group {} has no drop chance ({:d})", name, vnum_value, count_value, name_value, part_pct);
                            return false;
                        }

                        int rare_pct = 0;
                        str_to_number(rare_pct, tok->at(3).c_str());
                        rare_pct = std::clamp(rare_pct, 0, 100);

                        metin2::log::debug("Mob drop group: item {} count {:d} rare {:d}", name, count_value, rare_pct);
                        group_value->add_item(vnum_value, count_value, part_pct, rare_pct);
                        continue;
                    }

                    break;
                }
                mob_item_groups_.emplace(mob_vnum, std::move(group_value));

            } else if (type_value == "drop") {
                std::unique_ptr<drop_item_group> new_group;
                drop_item_group *group_value = nullptr;
                auto it = drop_item_groups_.find(mob_vnum);
                if (it == drop_item_groups_.end()) {
                    new_group = std::make_unique<drop_item_group>();
                    group_value = new_group.get();
                } else {
                    group_value = it->second.get();
                }

                for (int k = 1; k < 256; ++k) {
                    if (loader.get_token_vector(std::to_string(k), &tok)) {
                        std::string &name = tok->at(0);
                        uint32_t vnum_value = 0;

                        if (!vnum_by_original_name(name.c_str(), vnum_value)) {
                            str_to_number(vnum_value, name.c_str());
                            if (!get_item_manager().table(vnum_value)) {
                                metin2::log::error("Unknown item {} in drop group {}", name, name_value);
                                return false;
                            }
                        }

                        int count_value = 0;
                        str_to_number(count_value, tok->at(1).c_str());

                        if (count_value < 1) {
                            metin2::log::error("Item {} in drop group {} has invalid count", name, name_value);
                            return false;
                        }

                        float percent = atof(tok->at(2).c_str());

                        auto pct = (uint32_t)(10000.0f * percent);

                        metin2::log::debug("Drop group: item {} chance {:d} count {:d}", name, pct, count_value);
                        group_value->add_item(vnum_value, pct, count_value);

                        continue;
                    }

                    break;
                }
                if (new_group)
                    drop_item_groups_.emplace(mob_vnum, std::move(new_group));

            } else if (type_value == "limit") {
                auto level_item_group_value = std::make_unique<level_item_group>(level_limit_value);

                for (int k = 1; k < 256; k++) {
                    if (loader.get_token_vector(std::to_string(k), &tok)) {
                        std::string &name = tok->at(0);
                        uint32_t item_vnum = 0;

                        if (false == vnum_by_original_name(name.c_str(), item_vnum)) {
                            str_to_number(item_vnum, name.c_str());
                            if (!get_item_manager().table(item_vnum)) {
                                return false;
                            }
                        }

                        int count_value = 0;
                        str_to_number(count_value, tok->at(1).c_str());

                        if (count_value < 1) {
                            return false;
                        }

                        float pct_value = atof(tok->at(2).c_str());
                        auto pct = (uint32_t)(10000.0f * pct_value);

                        level_item_group_value->add_item(item_vnum, pct, count_value);

                        continue;
                    }

                    break;
                }

                level_item_groups_.emplace(mob_vnum, std::move(level_item_group_value));
            } else if (type_value == "thiefgloves") {
                auto group_value = std::make_unique<buyer_thief_gloves_item_group>();

                for (int k = 1; k < 256; ++k) {
                    if (loader.get_token_vector(std::to_string(k), &tok)) {
                        std::string &name = tok->at(0);
                        uint32_t vnum_value = 0;

                        if (!vnum_by_original_name(name.c_str(), vnum_value)) {
                            str_to_number(vnum_value, name.c_str());
                            if (!get_item_manager().table(vnum_value)) {
                                metin2::log::error("Unknown item {} in drop group {}", name, name_value);
                                return false;
                            }
                        }

                        int count_value = 0;
                        str_to_number(count_value, tok->at(1).c_str());

                        if (count_value < 1) {
                            metin2::log::error("Item {} in drop group {} has invalid count", name, name_value);
                            return false;
                        }

                        float percent = atof(tok->at(2).c_str());

                        auto pct = (uint32_t)(10000.0f * percent);

                        metin2::log::debug("Drop group: item {} chance {:d} count {:d}", name, pct, count_value);
                        group_value->add_item(vnum_value, pct, count_value);

                        continue;
                    }

                    break;
                }

                glove_item_groups_.emplace(mob_vnum, std::move(group_value));
            } else {
                metin2::log::error("{}: group {} has invalid type {} (expected kill or drop)", file_name, name_value, type_value);
                loader.set_parent_node();
                return false;
            }

            loader.set_parent_node();
        }

        return true;
    }

    auto item_manager::read_drop_item_group(const char *file_name) -> bool {
        text_file_loader loader;

        if (!loader.load(file_name))
            return false;

        std::string name_value;

        for (uint32_t i = 0; i < loader.child_node_count(); ++i) {
            loader.set_child_node(i);

            loader.current_node_name(&name_value);

            int vnum_value;
            int mob_vnum;

            if (!loader.token_integer("vnum", &vnum_value)) {
                metin2::log::error("{}: group {} has no vnum", file_name, name_value);
                loader.set_parent_node();
                return false;
            }

            if (!loader.token_integer("mob", &mob_vnum)) {
                metin2::log::error("{}: group {} has no mob vnum", file_name, name_value);
                loader.set_parent_node();
                return false;
            }

            metin2::log::debug("Drop group {} for mob {:d}", name_value, mob_vnum);

            token_vector *tok;

            auto it = drop_item_groups_.find(mob_vnum);

            std::unique_ptr<drop_item_group> new_group;
            drop_item_group *group_value;

            if (it == drop_item_groups_.end()) {
                new_group = std::make_unique<drop_item_group>();
                group_value = new_group.get();
            } else
                group_value = it->second.get();

            for (int k = 1; k < 256; ++k) {
                if (loader.get_token_vector(std::to_string(k), &tok)) {
                    std::string &name = tok->at(0);
                    uint32_t vnum = 0;

                    if (!vnum_by_original_name(name.c_str(), vnum)) {
                        str_to_number(vnum, name.c_str());
                        if (!get_item_manager().table(vnum)) {
                            metin2::log::error("Unknown item {} in drop group {}", name, name_value);
                            return false;
                        }
                    }

                    float percent = atof(tok->at(1).c_str());

                    auto pct = (uint32_t)(10000.0f * percent);

                    int count_value = 1;
                    if (tok->size() > 2)
                        str_to_number(count_value, tok->at(2).c_str());

                    if (count_value < 1) {
                        metin2::log::error("Item {} in drop group {} has invalid count", name, name_value);
                        return false;
                    }

                    metin2::log::debug("Drop group: item {} chance {:d} count {:d}", name, pct, count_value);
                    group_value->add_item(vnum, pct, count_value);
                    continue;
                }

                break;
            }

            if (new_group)
                drop_item_groups_.emplace(mob_vnum, std::move(new_group));

            loader.set_parent_node();
        }

        return true;
    }

    auto item_manager::read_item_vnum_mask_table(const char *file_name) -> bool {
        FILE *fp = fopen(file_name, "r");
        if (!fp) {
            return false;
        }

        int ori_vnum, new_vnum;
        while (fscanf(fp, "%u %u", &ori_vnum, &new_vnum) != EOF) {
            new_to_ori_.insert(map_dw2_dw::value_type(new_vnum, ori_vnum));
        }
        fclose(fp);
        return true;
    }
}
