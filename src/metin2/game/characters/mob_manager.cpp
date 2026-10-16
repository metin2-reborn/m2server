#include <metin2/game/characters/mob_manager.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/data/file_loader.h>
#include <metin2/game/data/text_file_loader.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/world/regen.h>

#include <metin2/common/utils.h>
#include <metin2/core/main.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/mob_types.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game::inline characters {

    mob::mob() {
        memset(&table, 0, sizeof(table));

        for (size_t i = 0; i < MOB_SKILL_MAX_NUM; ++i) {
            mob_skill_info[i].skill_vnum = 0;
            mob_skill_info[i].skill_level = 0;
            mob_skill_info[i].splash_attack.clear();
        }
    }

    void mob::add_skill_splash(int index, uint32_t timing, uint32_t hit_distance) {
        if (index >= MOB_SKILL_MAX_NUM || index < 0)
            return;

        metin2::log::debug("Mob {} skill {:d}: splash timing {:d}, hit distance {:d}",
            table.locale_name, index, timing, hit_distance);

        mob_skill_info[index].splash_attack.push_back(mob_splash_attack_info(timing, hit_distance));
    }

    mob_instance::mob_instance()
        : is_berserk(false), is_god_speed(false), is_revive(false) {
        last_attacked_time = get_dword_time();
        last_warp_time = get_dword_time();

        memset(&pos_last_attacked, 0, sizeof(pos_last_attacked));
    }

    mob_manager::mob_manager() = default;

    mob_manager::~mob_manager() = default;

    auto mob_manager::initialize(mob_table *table_value, int size) -> bool {
        for (auto &[vnum, retired] : map_mob_by_vnum_)
            retired_mobs_.push_back(std::move(retired));

        map_mob_by_vnum_.clear();
        map_mob_by_name_.clear();

        mob_table *t = table_value;

        for (int i = 0; i < size; ++i, ++t) {
            auto mob_owner = std::make_unique<mob>();
            mob *mob_value = mob_owner.get();

            std::memcpy(&mob_value->table, t, sizeof(mob_table));

            if (!map_mob_by_vnum_.try_emplace(t->vnum, std::move(mob_owner)).second)
                retired_mobs_.push_back(std::move(mob_owner));

            map_mob_by_name_.insert(std::map<std::string, mob *>::value_type(t->locale_name, mob_value));

            int skill_count = 0;

            for (int j = 0; j < MOB_SKILL_MAX_NUM; ++j)
                if (mob_value->table.skills[j].vnum)
                    ++skill_count;

            metin2::log::debug("Mob {:d} {}: level {:d}, hp {:d}, def {:d}, exp {:d}, drop item {:d}, {:d} skills",
                t->vnum, t->locale_name, t->level, t->max_hp, t->def, t->exp, t->drop_item_vnum, skill_count);

            if (t->type == CHAR_TYPE_NPC || t->type == CHAR_TYPE_WARP || t->type == CHAR_TYPE_GOTO)
                get_character_manager().register_race_num(t->vnum);

            quest::get_quest_manager().register_npc_vnum(t->vnum);
        }

        const std::string group_file_name = std::format("{}/group.txt", locale_service_get_base_path());
        const std::string group_group_file_name = std::format("{}/group_group.txt", locale_service_get_base_path());

        if (!load_group(group_file_name.c_str())) {
            metin2::log::error("Cannot load mob group file {}", group_file_name);
            thecore_shutdown();
        }
        if (!load_group_group(group_group_file_name.c_str())) {
            metin2::log::error("Cannot load mob group group file {}", group_group_file_name);
            thecore_shutdown();
        }

        get_character_manager().for_each_pc([this](character *ch) { rebind_mob_proto(ch); });
        return true;
    }

    void mob_manager::rebind_mob_proto(character *ch) {
        if (ch->is_pc())
            return;

        const mob *mob = get(ch->race_num());

        if (mob)
            ch->set_proto(mob);
    }

    auto mob_manager::get(uint32_t vnum) -> const mob * {
        auto it = map_mob_by_vnum_.find(vnum);

        if (it == map_mob_by_vnum_.end())
            return nullptr;

        return it->second.get();
    }

    auto mob_manager::get(const char *name, bool is_abbrev) -> const mob * {
        std::map<std::string, mob *>::iterator it;

        if (!is_abbrev) {
            it = map_mob_by_name_.find(name);

            if (it == map_mob_by_name_.end())
                return nullptr;

            return it->second;
        }

        int len = strlen(name);
        it = map_mob_by_name_.begin();

        while (it != map_mob_by_name_.end()) {
            if (!strncmp(it->first.c_str(), name, len))
                return it->second;

            ++it;
        }

        return nullptr;
    }

    void mob_manager::inc_regen_count(uint8_t regen_type, uint32_t vnum, int count, int time) {
        switch (regen_type) {
        case REGEN_TYPE_MOB:
            regen_count_[vnum] += count * 86400. / time;
            break;

        case REGEN_TYPE_GROUP:
            {
                mob_group *group = get_mob_manager().group(vnum);
                if (!group)
                    return;
                const std::vector<uint32_t> &c_rdw_members = group->member_vector();

                for (uint32_t i = 0; i < c_rdw_members.size(); i++)
                    regen_count_[c_rdw_members[i]] += count * 86400. / time;
            }
            break;

        case REGEN_TYPE_GROUP_GROUP:
            {
                auto it = map_mob_group_group_.find(vnum);

                if (it == map_mob_group_group_.end())
                    return;

                std::vector<uint32_t> &v = it->second->vec_member_vnum;
                for (uint32_t i = 0; i < v.size(); i++) {
                    mob_group *group = get_mob_manager().group(v[i]);
                    if (!group)
                        return;
                    const std::vector<uint32_t> &c_rdw_members = group->member_vector();

                    for (uint32_t i = 0; i < c_rdw_members.size(); i++)
                        regen_count_[c_rdw_members[i]] += count * 86400. / time / v.size();
                }
            }
            break;
        }
    }

    void mob_manager::dump_regen_count(const char *filename) {
        FILE *fp = fopen(filename, "w");

        if (fp) {
            std::map<uint32_t, double>::iterator it;

            fprintf(fp, "MOB_VNUM\tCOUNT\n");

            for (it = regen_count_.begin(); it != regen_count_.end(); ++it) {
                fprintf(fp, "%u\t%g\n", it->first, it->second);
            }

            fclose(fp);
        }
    }

    auto mob_manager::group_from_group_group(uint32_t vnum) -> uint32_t {
        auto it = map_mob_group_group_.find(vnum);

        if (it == map_mob_group_group_.end())
            return 0;

        return it->second->member();
    }

    auto mob_manager::group(uint32_t vnum) -> mob_group * {
        auto it = map_mob_group_.find(vnum);

        if (it == map_mob_group_.end())
            return nullptr;

        return it->second.get();
    }

    auto mob_manager::load_group_group(const char *file_name) -> bool {
        text_file_loader loader;

        if (!loader.load(file_name))
            return false;

        std::string name;

        for (uint32_t i = 0; i < loader.child_node_count(); ++i) {
            loader.set_child_node(i);

            loader.current_node_name(&name);

            int vnum_value;

            if (!loader.token_integer("vnum", &vnum_value)) {
                metin2::log::error("Syntax error in {}: group group {} has no vnum", file_name, name);
                loader.set_parent_node();
                continue;
            }

            token_vector *tok;

            auto group = std::make_unique<mob_group_group>(vnum_value);

            for (int k = 1; k < 256; ++k) {
                if (loader.get_token_vector(std::to_string(k), &tok)) {
                    uint32_t mob_vnum = 0;
                    str_to_number(mob_vnum, tok->at(0).c_str());

                    int prob = 1;
                    if (tok->size() > 1)
                        str_to_number(prob, tok->at(1).c_str());

                    if (mob_vnum)
                        group->add_member(mob_vnum);

                    continue;
                }

                break;
            }

            loader.set_parent_node();

            map_mob_group_group_.try_emplace((uint32_t)vnum_value, std::move(group));
        }

        return true;
    }

    auto mob_manager::load_group(const char *file_name) -> bool {
        text_file_loader loader;

        if (!loader.load(file_name))
            return false;

        std::string name;

        for (uint32_t i = 0; i < loader.child_node_count(); ++i) {
            loader.set_child_node(i);

            loader.current_node_name(&name);

            int vnum_value;

            if (!loader.token_integer("vnum", &vnum_value)) {
                metin2::log::error("Syntax error in {}: group {} has no vnum", file_name, name);
                loader.set_parent_node();
                continue;
            }

            token_vector *tok;

            if (!loader.get_token_vector("leader", &tok)) {
                metin2::log::error("Syntax error in {}: group {} has no leader", file_name, name);
                loader.set_parent_node();
                continue;
            }

            if (tok->size() < 2) {
                metin2::log::error("Syntax error in {}: group {} has no leader vnum", file_name, name);
                loader.set_parent_node();
                continue;
            }

            auto group_value = std::make_unique<mob_group>();

            group_value->create(vnum_value, name);
            uint32_t vnum = 0;
            str_to_number(vnum, tok->at(1).c_str());
            group_value->add_member(vnum);

            metin2::log::debug("Mob group {:d}: {}", vnum_value, name);
            metin2::log::debug("Mob group {:d}: leader {} (vnum {})", vnum_value, tok->at(0), tok->at(1));

            for (int k = 1; k < 256; ++k) {
                if (loader.get_token_vector(std::to_string(k), &tok)) {
                    metin2::log::debug("Mob group {:d}: member {} (vnum {})", vnum_value, tok->at(0), tok->at(1));
                    uint32_t vnum = 0;
                    str_to_number(vnum, tok->at(1).c_str());
                    group_value->add_member(vnum);
                    continue;
                }

                break;
            }

            loader.set_parent_node();
            map_mob_group_.try_emplace(vnum_value, std::move(group_value));
        }

        return true;
    }
}
